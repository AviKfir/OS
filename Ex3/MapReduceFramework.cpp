#include <atomic>
#include "iostream"
#include <pthread.h>
#include <algorithm>
#include <queue>
#include "MapReduceFramework.h"
#include "MapReduceClient.h"
#include "Barrier.h"

/**
 * constants
 */
#define PTHREAD_LOCK_FAIL_MSG "system error: error on pthread_mutex_lock\n"
#define PTHREAD_UNLOCK_FAIL_MSG "system error: error on pthread_mutex_unlock\n"
#define PTHREAD_JOIN_FAIL_MSG "system error: pthread_join function failed\n"
#define PTHREAD_DESTROY_FAIL_MSG "system error: error on pthread_mutex_destroy"
#define PTHREAD_CREATE_FAIL_MSG "system error: pthread create function failed\n"

/**
 * typedef
 */
typedef struct ThreadContext ThreadContext;
typedef struct JobContext JobContext;
typedef void *JobHandle;

/**
 * struct which includes all the parameters which are relevant to the thread.
 * Each thread has its own thread context.
 */
struct ThreadContext {
    int id;
    JobContext *jobC;
    IntermediateVec *intermediateVec;
};

/**
 * Shared to all the threads, includes all the shares parameters between all the threads.
 */
struct JobContext {
    int MT_LEVEL;
    stage_t stage = UNDEFINED_STAGE;
    int *numPairs;
    const MapReduceClient *client;
    std::atomic<uint32_t> *atomicIndexMap;
    std::atomic<uint32_t> *atomicFinishMap;
    std::atomic<uint32_t> *atomicShuffle;
    std::atomic<uint32_t> *atomicReduce;
    pthread_t *threads;
    ThreadContext *tContexts;
    const InputVec *inputVec;
    OutputVec *outputVec;
    std::deque<IntermediateVec *> *afterShuffleVec;
    pthread_mutex_t *mutexReduce;
    pthread_mutex_t *mutexOutputVec;
    pthread_mutex_t *mutexJoin;
    bool flagJoin;
    Barrier *barrier;

};

void map_phase(ThreadContext *tc);
void shuffle_phase(ThreadContext *tc);
void *reduce_phase(ThreadContext *tc);
bool IsAllEmpty (ThreadContext *tContexts, int len);
bool cmp (IntermediatePair firstPair, IntermediatePair secondPair);
void *entryPoint (void *arg);
void lockThread(pthread_mutex_t* mutex);
void unlockThread(pthread_mutex_t* mutex);

/**
 * this function checks if all the intermediate vectors are empty.
 * @param tContexts array which contains pointers to all the vectors to check.
 * @param len num of intermediate vectors.
 * @return true- if all vectors empty, false- otherwise.
 */
bool IsAllEmpty (ThreadContext *tContexts, int len)
{
  for (int i = 0; i < len; ++i)
    {
      if (!tContexts[i].intermediateVec->empty ()) return false;
    }
  return true;
}

/**
 * compare function.
 * @param firstPair the first element to compare- pair with key, value
 * @param secondPair the second element to compare- pair with key, value
 * @return true if second greater than first, false- otherwise.
 */
bool cmp (IntermediatePair firstPair, IntermediatePair secondPair)
{
  return *firstPair.first < *secondPair.first;
}

/**
 * This function produces a (K2*, V2*) pair.
 * @param key pointer to K2 object.
 * @param value pointer to V2 object.
 * @param context contains data structure of the thread that created the intermediary element.
 */
void emit2 (K2 *key, V2 *value, void *context)
{
  ThreadContext *tc = (ThreadContext *) context;
  tc->intermediateVec->push_back (IntermediatePair (key, value));

}

/**
 * This function produces a (K3*, V3*) pair.
 * @param key pointer to K3 object.
 * @param value pointer to V3 object.
 * @param context contains data structure of the thread that created the intermediary element.
 */
void emit3 (K3 *key, V3 *value, void *context)
{
  ThreadContext *tc = (ThreadContext *) context;
  lockThread(tc->jobC->mutexOutputVec);
  tc->jobC->outputVec->push_back (OutputPair (key, value));
  unlockThread(tc->jobC->mutexOutputVec);
}

/**
 * this function is what each thread does after it is created.
 * @param arg  pointer to struct which includes all the parameters which are relevant to the thread.
 * Each thread has its own thread context.
 * @return nullptr
 */
void *entryPoint (void *arg)
{
  ThreadContext *tc = (ThreadContext *) arg;

  // MAP PHASE - [ [null, "zzabbaazzz"], [null, "world"] ... ]
  map_phase(tc);

  // SORT PHASE - Each one (for example: string) has IntermediateVec [[a, 3], [b, 2], [z, 5] ...]
  std::sort (tc->intermediateVec->begin (), tc->intermediateVec->end (), cmp);

  //barrier until all threads finish to sort
  tc->jobC->barrier->barrier();

  // SHUFFLE PHASE
  shuffle_phase(tc);

  // barrier until the thread 0 will finish shuffling
  tc->jobC->barrier->barrier();

  // REDUCE PHASE [ [[a, 3], [a, 5] ...], [[b, 5], [b, 1] ...], ... ]
  return reduce_phase(tc);
}

/**
 * The reduce function in turn will produce (k3, v3) pairs and will call emit3 to add them to the
 * framework data structures. These can be inserted directly to the output vector.
 * @param tc struct which includes all the parameters which are relevant to the thread.
 * Each thread has its own thread context.
 * @return nullptr
 */
void *reduce_phase(ThreadContext *tc) {
    while (true)
      {
        // only one thread can will in to change stage.
        if ((tc->jobC->stage) == SHUFFLE_STAGE)
        {
            (tc->jobC->stage) = REDUCE_STAGE;
            *(tc->jobC->atomicIndexMap) = 0;
        }
        // we lock with mutex to protect back() / pop_back() functions.
        lockThread(tc->jobC->mutexReduce);
        if (tc->jobC->afterShuffleVec->empty ())
          {
            unlockThread(tc->jobC->mutexReduce);
            return nullptr;
          }
        IntermediateVec * VecToReduce = tc->jobC->afterShuffleVec->back ();
        int numPairsToReduce = (int) VecToReduce->size ();
        tc->jobC->afterShuffleVec->pop_back();
        unlockThread(tc->jobC->mutexReduce);
        tc->jobC->client->reduce (VecToReduce, tc);
        *(tc->jobC->atomicReduce) += numPairsToReduce;
        // the pop_back() calls the destructor of the elements he has.
        // afterShuffleVec has pointers to IntermediateVec, the destructor of a pointer does nothing!
        // This is why we must delete the IntermediateVec by ourselves.
        delete VecToReduce;
      }
}

/**
 * this function create new sequences of (k2, v2) where in each sequence all
 * keys are identical and all elements with a given key are in a single sequence.
 * @param tc struct which includes all the parameters which are relevant to the thread.
 * Each thread has its own thread context.
 */
void shuffle_phase(ThreadContext *tc)
{
    // waits for thread 0 to be the running thread (ONLY thread 0 a.k.a. main-thread gets in)
    if (tc->id == 0)
      {
        (tc->jobC->stage) = SHUFFLE_STAGE;
        int totalNumPairs = 0;
        int t_num = tc->jobC->MT_LEVEL;
        for (int i = 0; i < t_num; i++)
          {
            totalNumPairs += (int) tc->jobC->tContexts[i].intermediateVec->size ();
          }
          *tc->jobC->numPairs = totalNumPairs;
        while (!IsAllEmpty (tc->jobC->tContexts, t_num))
          {
            K2 *max_key = nullptr;
            for (int i = 0; i < t_num; ++i)
              {
                if (!tc->jobC->tContexts[i].intermediateVec->empty ())
                  {
                    K2 *currentKey = tc->jobC->tContexts[i].intermediateVec->back ().first;
                    if (max_key == nullptr)
                      {
                        max_key = currentKey;
                      }
                    else
                      {
                        if (*max_key < *currentKey)
                          {
                            max_key = currentKey;
                          }
                      }
                  }
              }
            IntermediateVec *NewVecKey = new IntermediateVec;
            for (int i = 0; i < t_num; ++i)
              {
                IntermediateVec *currentVec = tc->jobC->tContexts[i].intermediateVec;
                while (!currentVec->empty ()
                && !(*max_key < *(currentVec->back().first)) && !(*(currentVec->back().first) < *max_key))
                  {
                    NewVecKey->push_back (currentVec->back ());
                    currentVec->pop_back ();
                  }
              }
            tc->jobC->afterShuffleVec->push_front(NewVecKey);
            *(tc->jobC->atomicShuffle) += (NewVecKey->size ());
          }
      }
}

/**
 * In this phase each thread reads pairs of (k1,  v1) from the input vector and calls the map function
 * on each of them.
 * @param tc struct which includes all the parameters which are relevant to the thread.
 * Each thread has its own thread context.
 */
void map_phase(ThreadContext *tc) {//change the state of the counter to map phase
    if ((tc->jobC->stage) == UNDEFINED_STAGE)
      {
        (tc->jobC->stage) = MAP_STAGE;
      }

    // store the current index to map in old val and increment by 1 the counter
    int oldValue = (int) (*(tc->jobC->atomicIndexMap))++;
    int inputVecSize = (int) tc->jobC->inputVec->size ();

    // until we finish mapping all the pairs in inputVec
    while (inputVecSize > oldValue)
      {
        const K1 *key = tc->jobC->inputVec->at (oldValue).first;
        const V1 *value = tc->jobC->inputVec->at (oldValue).second;
        // inside map emit2 is called. the pairs(k2, v2) are put inside the
        // intermediateVec that each thread context has.
        tc->jobC->client->map (key, value, tc);
          (*(tc->jobC->atomicFinishMap)) ++; // after map() is done, we increase the num of finished pairs.
          oldValue = (int) (*(tc->jobC->atomicIndexMap))++;
      }
}

/**
 * This function starts running the MapReduce algorithm (with several threads)
 * @param client The implementation of MapReduceClient or in other words the task that the framework should run.
 * @param inputVec a vector of type std::vector<std::pair<K1*, V1*>>, the input elements.
 * @param outputVec a vector of type std::vector<std::pair<K3*, V3*>>, to which the output
 * elements will be added before returning
 * @param multiThreadLevel the number of worker threads to be used for running the algorithm.
 * @return an identifier of a running job.
 */
JobHandle startMapReduceJob (const MapReduceClient &client,
                             const InputVec &inputVec, OutputVec &outputVec,
                             int multiThreadLevel)
{
  JobContext *jc = new JobContext;
  pthread_mutex_t *mutexReduce = new pthread_mutex_t (PTHREAD_MUTEX_INITIALIZER);
  pthread_mutex_t *mutexOutputVec = new pthread_mutex_t (PTHREAD_MUTEX_INITIALIZER);
  pthread_mutex_t *mutexJoin = new pthread_mutex_t (PTHREAD_MUTEX_INITIALIZER);
  jc->mutexReduce = mutexReduce;
  jc->mutexOutputVec = mutexOutputVec;
  jc->mutexJoin = mutexJoin;
  jc->threads = new pthread_t[multiThreadLevel];
  jc->tContexts = new ThreadContext[multiThreadLevel];
  jc->afterShuffleVec = new std::deque<IntermediateVec *>;
  jc->MT_LEVEL = multiThreadLevel;
  jc->client = &client;
  jc->inputVec = &inputVec;
  jc->outputVec = &outputVec;
  jc->atomicIndexMap = new std::atomic<uint32_t> (0);
  jc->atomicFinishMap = new std::atomic<uint32_t> (0);
  jc->atomicShuffle = new std::atomic<uint32_t> (0);
  jc->atomicReduce= new std::atomic<uint32_t> (0);
  jc->flagJoin = false;
  jc->barrier = new Barrier(multiThreadLevel);
  jc->numPairs = new int(0);
  for (int i = 0; i < multiThreadLevel; ++i)
    {
      jc->tContexts[i].id = i;
      jc->tContexts[i].jobC = jc;
      jc->tContexts[i].intermediateVec = new IntermediateVec;
    }
  for (int i = 0; i < multiThreadLevel; i++)
    {
      if (pthread_create (jc->threads + i, NULL, entryPoint, jc->tContexts + i)
          != 0)
        {
          std::cerr << PTHREAD_CREATE_FAIL_MSG << std::endl;
          exit (1);
        }
    }
  return (JobHandle) jc;
}

/**
 *  a function gets JobHandle returned by startMapReduceFramework and waits
 *  until it is finished.
 * @param job an identifier of a running job.
 */
void waitForJob (JobHandle job)
{

  JobContext *jc = (JobContext *) job;

  lockThread(jc->mutexJoin);
    if (jc->flagJoin)
    {
      unlockThread(jc->mutexJoin);
      return;
    }
  jc->flagJoin = true;
  unlockThread(jc->mutexJoin);

  for (int i = 0; i < jc->MT_LEVEL; ++i)
    {
      if (pthread_join (jc->threads[i], nullptr) != 0)
        {
          std::cerr << PTHREAD_JOIN_FAIL_MSG << std::endl;
          exit (1);
        }
    }

  delete[] jc->threads;
  jc->threads = nullptr;

}

void lockThread(pthread_mutex_t* mutex) {
    if (pthread_mutex_lock (mutex) != 0)
      {
        fprintf (stderr, PTHREAD_LOCK_FAIL_MSG);
        exit (1);
      }
}

void unlockThread(pthread_mutex_t* mutex) {
    if (pthread_mutex_unlock (mutex) != 0)
    {
        fprintf (stderr, PTHREAD_UNLOCK_FAIL_MSG);
        exit (1);
    }
}

/**
 * this function gets a JobHandle and updates the state of the job into the given
 * JobState struct.
 * @param job an identifier of a running job.
 * @param state a struct which quantizes the state of a job.
 */
void getJobState (JobHandle job, JobState *state)
{
  // Only main thread can get in this function.
  // because the user in using this func, and only has access to the main thread. So there's no need of mutex.

  JobContext *jc = (JobContext *) job;

    unsigned int finishedMap = (jc->atomicFinishMap->load());
    unsigned int finished1 = (jc->atomicShuffle->load());
    unsigned int finished2 = (jc->atomicReduce->load());
    float inputVecSize = (float) jc->inputVec->size();
    int numPairs = *jc->numPairs;

    if ((jc->stage) == UNDEFINED_STAGE)
    {
        state->percentage = 0;
        state->stage = UNDEFINED_STAGE;
    }
    else if ((jc->stage) == MAP_STAGE)
    {
        state->stage = MAP_STAGE;

        state->percentage = (float) finishedMap / inputVecSize * 100;
    }
    else if ((jc->stage) == SHUFFLE_STAGE) {
        state->stage = SHUFFLE_STAGE;
        state->percentage = ((float)finished1 / (float) numPairs) * 100;
    }
    else if ((jc->stage) == REDUCE_STAGE) {
        state->stage = REDUCE_STAGE;
        state->percentage = ((float) finished2 / (float) numPairs) * 100;
  }
}

/**
 * Releasing all resources of a job.
 * @param job an identifier of a running job.
 */
void closeJobHandle (JobHandle job)
{
  // Can be called only once, thus no mutex needed.

  waitForJob (job); // deletes jc->threads.
  JobContext *jc = (JobContext *) job;
  if (pthread_mutex_destroy (jc->mutexReduce) != 0)
    {
      fprintf (stderr, PTHREAD_DESTROY_FAIL_MSG);
      exit (1);
    }
  delete jc->mutexReduce;
  if (pthread_mutex_destroy (jc->mutexOutputVec) != 0)
    {
      fprintf (stderr, PTHREAD_DESTROY_FAIL_MSG);
      exit (1);
    }
  delete jc->mutexOutputVec;
  if (pthread_mutex_destroy (jc->mutexJoin) != 0)
    {
      fprintf (stderr, PTHREAD_DESTROY_FAIL_MSG);
      exit (1);
    }
  delete jc->mutexJoin;
  delete jc->atomicIndexMap;
  delete jc->atomicShuffle;
  delete jc->atomicReduce;
  delete jc->atomicFinishMap;
  delete jc->barrier;
  delete jc->numPairs;
  delete jc->afterShuffleVec;
  for(int i = 0; i < jc->MT_LEVEL; i++) {
    delete jc->tContexts[i].intermediateVec;
  }
  delete[] jc->tContexts;
  delete jc;
}

