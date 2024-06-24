#include "VirtualMemory.h"
#include "PhysicalMemory.h"

bool
dfs (word_t now_frame, word_t saved_frame, word_t *found_frame, word_t *max_index,
     word_t level_depth, uint64_t virtual_page, long long int *max_distance,
     uint64_t access_page, bool flag_empty_table, uint64_t *virtual_victim_page, word_t *physical_victim_frame,
     word_t now_parent, word_t *victim_parent, word_t *victim_parent_i, word_t *victim_parent_k, word_t i);

/**
 * inline function reduce overhead.
 * When a function is called in a few places but executed many times, changing the function
 * to an inline function typically saves many function calls and results in performance improvement.
 */
inline long long int abs (long long int var)
{
    if (var < 0)
    {
        var = var * (-1);
    }
    return var;
}

/**
 * find the minimum between tow ints
 * @param var1 int 1
 * @param var2 int 2
 * @return the minimum int
 */
inline long long int min (long long int var1, long long int var2)
{
    if (var1 < var2) return var1;
    return var2;
}

/**
 * Initialize the virtual memory.
 */
void VMinitialize ()
{
    for (uint64_t i = 0; i < PAGE_SIZE; i++)
    {
        PMwrite (i, 0);
    }
}

/**
 * translates virtual Address to Physical Address.
 */
uint64_t Translator (uint64_t virtualAddress)
{
    word_t cur_frame = 0;
    long int root_size =
    VIRTUAL_ADDRESS_WIDTH % OFFSET_WIDTH; //number of bits of root
    if (root_size == 0)
    {
        root_size = OFFSET_WIDTH;
    }
    for (int i = 0; i < TABLES_DEPTH; i++)
    {
        unsigned long cur_word;
        if (i == 0)
        {
            cur_word = ((1 << root_size) - 1)
                    & (virtualAddress >> (OFFSET_WIDTH * TABLES_DEPTH));
        }
        if (i != 0)
        {

            cur_word = ((1 << OFFSET_WIDTH) - 1)
                    & (virtualAddress >> (OFFSET_WIDTH * (TABLES_DEPTH - i)));
        }
        word_t dad = 0;
        PMread ((cur_frame * PAGE_SIZE) + cur_word, &dad);
        if (dad == 0)
        {
            word_t found_frame = 0;
            word_t max_index = 0;
            uint64_t virtual_victim_page = 0;
            word_t physical_victim_frame = 0;
            word_t victim_parent = 0;
            word_t victim_parent_i = 0;
            word_t victim_parent_k = 0;
            long long int max_distance = -1;
            uint64_t access_page = virtualAddress >> OFFSET_WIDTH; // doesn't change (no need of pointer).

            if (dfs (0, cur_frame, &found_frame, &max_index, 0, 0, &max_distance,
                     access_page, true, &virtual_victim_page, &physical_victim_frame, 0,
                     &victim_parent, &victim_parent_i, &victim_parent_k, 0))
            {
                // found empty table
                PMwrite ((cur_frame * PAGE_SIZE)
                + cur_word, found_frame); // updating the son's address into dad.
                dad = found_frame;
                PMwrite ((victim_parent * PAGE_SIZE) + victim_parent_i, 0);
            }
            else if (max_index + 1 < NUM_FRAMES)
            {
                if (i != (TABLES_DEPTH - 1)) {
                    for (long long j = 0; j < PAGE_SIZE; j++)
                        // Initialize the virtual memory.
                        {
                        PMwrite (((max_index + 1) * PAGE_SIZE) + j, 0);
                        }
                }
                PMwrite (
                        (cur_frame * PAGE_SIZE) + cur_word,
                        max_index + 1); // updating the son's address into dad.
                        dad = max_index + 1;
            }
            else
            {
                PMevict (physical_victim_frame, virtual_victim_page);
                if (i != (TABLES_DEPTH - 1)) {
                    for (long long j = 0; j < PAGE_SIZE; j++)
                        // Initialize the virtual memory.
                        {
                        PMwrite ((physical_victim_frame * PAGE_SIZE) + j, 0);
                        }
                }
                PMwrite ((cur_frame * PAGE_SIZE)
                + cur_word, physical_victim_frame); // updating the son's address into dad.
                dad = physical_victim_frame;
                PMwrite ((victim_parent * PAGE_SIZE) + victim_parent_k, 0);
            }
            if (i == (TABLES_DEPTH - 1)) {  // last depth, we got to the virtual page. bringing it back from disk.
                PMrestore (dad, access_page);
            }
        }
    cur_frame = dad;
    }
    uint64_t offset = ((1 << OFFSET_WIDTH) - 1) & virtualAddress;
    uint64_t physical_addr = cur_frame * PAGE_SIZE + offset;
    return physical_addr;
}

/** Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread (uint64_t virtualAddress, word_t *value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
        return 0;
    }
    uint64_t physical_addr = Translator (virtualAddress);
    if (physical_addr == 0 || physical_addr >= RAM_SIZE)
    {
        return 0;
    }
    PMread (physical_addr, value);
    return 1;
}

/** Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite (uint64_t virtualAddress, word_t value)
{
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
        return 0;
    }
    uint64_t physical_addr = Translator (virtualAddress);
    if (physical_addr == 0 || physical_addr >= RAM_SIZE)
    {
        return 0;
    }
    PMwrite (physical_addr, value);
    return 1;
}

/**
 * physical memory = RAM
 * CASE 1: A frame containing an empty table.
 * CASE 2: An unused frame.
 * CASE 3: All frames are already used, we swap out the frame with maximal cyclical
 * distance in order to replace it with the relevant page.
 *
 * @param now_frame : current frame's physical address
 * @param saved_frame : the frame where we stopped (when we found dad == 0)
 * @param found_frame : the frame with the empty table (only zeros), CASE 1.
 * @param max_index : the biggest physical memory index in use.
 * @param level_depth : Hierarchical Tree level.
 * @param virtual_page : virtual address of now_frame.
 * @param max_distance : max cyclical_distance
 * @param access_page: the virtual address we are looking for (VMwrite/VMread).
 * @param flag_empty_table: is true at the beginning of each recursion call.
 * @param virtual_victim_page : for the PMevict func.
 * @param physical_victim_frame : for the PMevict func.
 * @param now_parent : current frame's parent physical address.
 * @param victim_parent : after CASE 3. we have to remove the reference to the victim page from its parent table.
 * @return true: CASE 1.  false: CASE 2/3.
 */
bool
dfs (word_t now_frame, word_t saved_frame, word_t *found_frame, word_t *max_index,
     word_t level_depth, uint64_t virtual_page, long long int *max_distance,
     uint64_t access_page, bool flag_empty_table, uint64_t *virtual_victim_page, word_t *physical_victim_frame,
     word_t now_parent, word_t *victim_parent, word_t *victim_parent_i, word_t *victim_parent_k, word_t frame_i)
{
    if (level_depth == TABLES_DEPTH)
    {
        long long int distance = abs ((long long int) (access_page
                - virtual_page));
        long long int cyclical_distance = min (NUM_PAGES - distance, distance);
        if (cyclical_distance > *max_distance)
        {
            *max_distance = cyclical_distance;
            *virtual_victim_page = virtual_page;
            *physical_victim_frame = now_frame;
            *victim_parent = now_parent;
            *victim_parent_k = frame_i;
        }
        return false;
    }

    for (int i = 0; i < PAGE_SIZE; i++)
    {
        word_t addr;
        PMread ((now_frame * PAGE_SIZE) + i, &addr);
        if (addr != 0)
        {
            flag_empty_table = false;
            if (*max_index < addr)
            {
                *max_index = addr;
            }
            if (dfs (addr, saved_frame, found_frame, max_index,
                     level_depth + 1, (virtual_page << OFFSET_WIDTH)
                     | i, max_distance, access_page,
                     true, virtual_victim_page, physical_victim_frame,
                     now_frame, victim_parent, victim_parent_i, victim_parent_k, i))
                return true;
        }
    }

    if (flag_empty_table && (now_frame != saved_frame))
    {
        *found_frame = now_frame;
        *victim_parent = now_parent;
        *victim_parent_i = frame_i;
        return true;
    }
    return false;
}
