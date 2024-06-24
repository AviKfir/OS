/*
 * User-Level Threads Library (uthreads)
 * Hebrew University OS course.
 * Author: OS, os@cs.huji.ac.il
 */

/* Libraries */
#include "uthreads.h"
#include "Thread.h"
#include <csignal>
#include <iostream>
#include <algorithm>
#include <utmpx.h>
#include <queue>

/* CONSTANTS */
#define FAILURE 1
#define SUCCESS 0
#define FAIL -1
#define MSG_SETITIMER_FAIL "system error: system call - setitimer failed."
#define MSG_PROCMASK_FAIL "system error: system call - sigprocmask failed"
#define MSG_NON_POSITIVE "system error: invalid - quantum_usecs is non-positive."
#define MSG_SIGEMPTYSET_FAIL "system error: system call - sigemptyset failed"
#define MSG_SIGACTION_FAIL "system error: system call - sigaction failed."
#define MSG_NUM_EXCEED "system error: number of concurrent threads to exceed the limit."
#define MSG_ENTRY_POINT_NULL "system error: entry_point is NULL."
#define MSG_INVALID_ID "system error: invalid id number."
#define MSG_BLOCK_MAIN_THREAD "system error: trying to block the main thread is forbidden."
#define MSG_RESUME_INVALID "system error: trying to resume an invalid id thread."
#define MSG_SLEEP_INVALID_TIME "system error: uthread_sleep - number of quantums is non positive."
#define MSG_SLEEP_MAIN_THREAD "system error: the main thread trying to sleep."
#define MSG_GET_QUANTUMS "system error: trying to get quantums of not exists thread."
#define MSG_SIGADDSET_FAIL "system error: system call - sigaddset failed"

/* internal interface (functions declaration) */
void block_signals();
void unblock_signals();
void jump_to_thread (int tid);
void switch_thread ();
void timer_handler (int sig);
void init_Timer(int quantum_usecs);

/* global variables (including data structures) */
struct itimerval timer;
sigset_t set;
int user_quantum_usecs = 0;
int currentThread = 0;
int concurrentThreads = 1;
int totalNumQuantums = 1;
Thread *ThreadList[MAX_THREAD_NUM];
std::deque<int> readyList;
std::vector<int> blockedList;
std::vector<int> sleepList;

/**
 * this function deallocates all the memory that was allocated in the Heap.
 **/
void free_all() {
    for (int i = MAX_THREAD_NUM - 1; i >= 0; --i)
    {
        if (ThreadList[i] != nullptr)
        {
            delete ThreadList[i];
            ThreadList[i] = nullptr;
        }
    }
    readyList.clear();
    blockedList.clear();
    sleepList.clear();
    concurrentThreads = 0;
}

/**
 * blocking SIGVTALRM signal.
 **/
void block_signals() {
    if (sigprocmask(SIG_BLOCK, &set, nullptr))
    {
        std::cerr << MSG_PROCMASK_FAIL << std::endl;
        free_all();
        exit (FAILURE);
    }
}

/**
 * unblocking SIGVTALRM signal.
 **/
void unblock_signals() {
    if (sigprocmask(SIG_UNBLOCK, &set, nullptr))
    {
        std::cerr << MSG_PROCMASK_FAIL << std::endl;
        free_all();
        exit (FAILURE);
    }
}

/**
 * totalNumQuantums++ and sleepList check.
 * set of the timer.
 **/
void timer_sleep_check() {
    block_signals();
    totalNumQuantums++;
    for (unsigned long int i = 0; i < sleepList.size (); i++)
    {
        ThreadList[sleepList[i]]->decSleepTime ();
        if (ThreadList[sleepList[i]]->getSleepTime () == 0)
        {
            sleepList.erase (sleepList.begin () + i);
            // if thread's state is BLOCKED, we do nothing (thread stays in blockedList until resume()).
            if (ThreadList[sleepList[i]]->getState () == SLEEP_NOT_BLOCKED)
            {
                ThreadList[sleepList[i]]->setState (READY);
                readyList.push_back (sleepList[i]);
            }
        }
    }
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << MSG_SETITIMER_FAIL << std::endl;
        free_all();
        unblock_signals();
        exit (FAILURE);
    }
    unblock_signals();
}

/**
 * jumping to next thread with siglongjmp.
 **/
void jump_to_thread (int tid)
{
    block_signals();
    // signal still blocked (from switch_thread)
    currentThread = tid;
    ThreadList[tid]->setState (RUNNING);
    ThreadList[tid]->incQuantums ();
    timer_sleep_check();
    unblock_signals(); // important to jump to next threat without blocking signals!
    siglongjmp(ThreadList[tid]->_env, 1); // no return value
}

/**
 * switch between two Threads.
 **/
void switch_thread ()
{
    // we can't put switch_thread() inside timer_handler()
    // because sometimes we do not want the thread to get inside readyList.
    block_signals();
    // sigsetjmp can not fail
    int ret_val = sigsetjmp(ThreadList[currentThread]->_env, 1);
    bool did_just_save_bookmark = ret_val == 0;
    if (did_just_save_bookmark)
        // enters only first time (it saves env with sigset).
        // Doesn't enter if we continue the thread from the saved point.
    {
        int firstElem = readyList.front ();
        readyList.pop_front ();
        jump_to_thread (firstElem);
    }
    unblock_signals();
}

/**
 * override of the default signal handler (each time the signal occurs, it switches two threads).
 **/
void timer_handler (int sig)
{
    block_signals();
    // we first put the currentThread inside readyList and only after that, we take the first thread from there
    // It's in case only main thread is running. So readyList won't be empty when we do pop_front().
    ThreadList[currentThread]->setState (READY);
    readyList.push_back (currentThread);
    switch_thread ();
    unblock_signals();
}

/**
 * Defines the timer cycle (the signal SIGVTALRM is called by the OS each quantum time).
 **/
void init_Timer(int quantum_usecs)
{
    // Configure the timer to expire after quantum microseconds... */
    timer.it_value.tv_sec = quantum_usecs / (int)(1e6);        // first time interval, seconds part
    timer.it_value.tv_usec = quantum_usecs % (int)(1e6);    // first time interval, microseconds part

    // configure the timer to expire every quantum microseconds after that.
    timer.it_interval.tv_sec = quantum_usecs / (int)(1e6);      // following time intervals, seconds part
    timer.it_interval.tv_usec = quantum_usecs % (int)(1e6);   // following time intervals, microseconds part
}

/**
 * @brief initializes the thread library.
 *
 * You may assume that this function is called before any other thread library
 * function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init (int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        std::cerr << MSG_NON_POSITIVE << std::endl;
        return FAIL;
    }
    if (sigemptyset(&set))
    {
        std::cerr << MSG_SIGEMPTYSET_FAIL << std::endl;
        free_all();
        exit (FAILURE);
    }
    if (sigaddset(&set, SIGVTALRM))
    {
        std::cerr << MSG_SIGADDSET_FAIL << std::endl;
        free_all();
        exit (FAILURE);
    }

    struct sigaction sa = {0};
    sa.sa_handler = &timer_handler;
    user_quantum_usecs = quantum_usecs;
    init_Timer(user_quantum_usecs);

    if (sigaction (SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << MSG_SIGACTION_FAIL << std::endl;
        free_all();
        exit (FAILURE);
    }

    // we make sure signals are blocked before we start the timer (setitimer).
    block_signals();
    Thread* mainThread = new Thread();
    ThreadList[0] = mainThread;
    currentThread = 0;

    // starts timer
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr))
    {
        std::cerr << MSG_SETITIMER_FAIL << std::endl;
        free_all();
        exit (FAILURE);
    }
    unblock_signals();
    return SUCCESS;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point
 * with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of
 * concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 *
 * @return On success, return the ID of the created thread. On failure,
 * return -1.
*/
int uthread_spawn (thread_entry_point entry_point)
{
    block_signals();
    if (concurrentThreads >= MAX_THREAD_NUM)
    {
        std::cerr << MSG_NUM_EXCEED << std::endl;
        unblock_signals();
        return FAIL;
    }
    if (entry_point == nullptr) {
        std::cerr << MSG_ENTRY_POINT_NULL << std::endl;
        unblock_signals();
        return FAIL;
    }
    for (int i = 1; i < MAX_THREAD_NUM; ++i)
    { // i = 0 is taken by the main thread
        if (ThreadList[i] == nullptr)
        {
            ThreadList[i] = new Thread(entry_point);
            ThreadList[i]->setState (READY);
            readyList.push_back (i);
            concurrentThreads++;
            unblock_signals();
            return i;
        }
    }
    return SUCCESS;
}


/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant
 * control structures.
 *
 * All the resources allocated by the library for this thread should be
 * released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will
 * result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated
 * and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate (int tid)
{
    block_signals();
    if (tid >= MAX_THREAD_NUM || tid < 0 || ThreadList[tid] == nullptr)
    {
        std::cerr << MSG_INVALID_ID << std::endl;
        unblock_signals();
        return FAIL;
    }

    if (tid != 0)
    {
        concurrentThreads--;
        if (tid == currentThread)
        {
            delete ThreadList[tid];
            ThreadList[tid] = nullptr;
            int firstElem = readyList.front ();
            readyList.pop_front ();
            jump_to_thread (firstElem);
        }
        else {
            if (ThreadList[tid]->getState () == READY)
            {
                for (unsigned long int i = 0; i < readyList.size (); ++i)
                {
                    if (readyList[i] == tid)
                    {
                        readyList.erase (readyList.begin () + i);
                        break;
                    }
                }
            }
            if (ThreadList[tid]->getState () == BLOCKED)
            {
                for (unsigned long int i = 0; i < blockedList.size (); ++i)
                {
                    if (blockedList[i] == tid)
                    {
                        blockedList.erase (blockedList.begin () + i);
                        break;
                    }
                }
            }
            if (ThreadList[tid]->getState () == SLEEP_NOT_BLOCKED)
            {
                for (unsigned long int i = 0; i < sleepList.size (); ++i)
                {
                    if (sleepList[i] == tid)
                    {
                        sleepList.erase (sleepList.begin () + i);
                        break;
                    }
                }
            }
            // we delete after checking which list it was because we use getState().
            delete ThreadList[tid];
            ThreadList[tid] = nullptr;
        }
    }
    else // terminate main thread and the whole process
    {
        free_all();
        exit (SUCCESS);
    }
    unblock_signals();
    return SUCCESS;

}


/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later
 * using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition,
 * it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block (int tid)
{
    block_signals();
    if (tid == 0)
    {
        std::cerr << MSG_BLOCK_MAIN_THREAD << std::endl;
        unblock_signals();
        return FAIL;
    }
    if (tid >= MAX_THREAD_NUM || tid < 0 || ThreadList[tid] == nullptr)
    {
        std::cerr << MSG_INVALID_ID << std::endl;
        unblock_signals();
        return FAIL;
    }
    if (ThreadList[tid]->getState () == BLOCKED)
    {
        unblock_signals();
        return SUCCESS; //the thread is already blocked
    }
    if (currentThread != tid)
    {
        if (ThreadList[tid]->getState () != SLEEP_NOT_BLOCKED)
        {
            for (unsigned long int i = 0; i < readyList.size (); ++i)
            {
                if (readyList[i] == tid)
                {
                    readyList.erase (readyList.begin () + i);
                }
            }
        }
        ThreadList[tid]->setState (BLOCKED);
        blockedList.push_back (tid);
    }
    else
    { //thread is blocking itself
        ThreadList[tid]->setState (BLOCKED);
        blockedList.push_back (tid);
        switch_thread ();
    }
    unblock_signals();
    return SUCCESS;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not
 * considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume (int tid)
{
    block_signals();
    if (tid >= MAX_THREAD_NUM || tid < 0 || ThreadList[tid] == nullptr)
    {
        std::cerr << MSG_RESUME_INVALID << std::endl;
        unblock_signals();
        return FAIL;
    }
    if (ThreadList[tid]->getState () == BLOCKED &&
    ThreadList[tid]->getSleepTime () == 0)
    {
        ThreadList[tid]->setState (READY);
        readyList.push_back (tid);
        blockedList.erase (std::remove (blockedList.begin (), blockedList.end (), tid),
                           blockedList.end ());
    }
    if (ThreadList[tid]->getState () == BLOCKED &&
    ThreadList[tid]->getSleepTime () > 0)
    {
        ThreadList[tid]->setState (SLEEP_NOT_BLOCKED);
    }
    unblock_signals();
    return SUCCESS;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a
 * scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of
 * the READY threads list.
 * The number of quantums refers to the number of times a new quantum starts,
 * regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid==0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep (int num_quantums)
{
    block_signals();
    if (num_quantums <= 0)
    {
        std::cerr << MSG_SLEEP_INVALID_TIME << std::endl;
        unblock_signals();
        return FAIL;
    }
    if (currentThread == 0)
    {
        std::cerr << MSG_SLEEP_MAIN_THREAD << std::endl;
        unblock_signals();
        return FAIL;
    }
    sleepList.push_back (currentThread);
    ThreadList[currentThread]->setSleepTime (num_quantums + 1);
    // +1 because we dont count the current quantum (we decrease it by 1 in the time handler).
    ThreadList[currentThread]->setState (SLEEP_NOT_BLOCKED);
    switch_thread ();
    unblock_signals();
    return SUCCESS;
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid ()
{
    return currentThread;
}

/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums ()
{
    return totalNumQuantums;
}

/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums (int tid)
{
    block_signals();
    if (tid >= MAX_THREAD_NUM || tid < 0 || ThreadList[tid] == nullptr)
    {
        std::cerr << MSG_GET_QUANTUMS << std::endl;
        return FAIL;
    }
    unblock_signals();
    return (int) ThreadList[tid]->getQuantums ();
}
