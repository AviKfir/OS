#include "uthreads.h"
#include <csetjmp>

/**
 * enum for the different thread states
 */
enum ThreadState {
    READY,
    RUNNING,
    BLOCKED,
    SLEEP_NOT_BLOCKED
};

/**
 * represents one thread in the process
 */
class Thread {

public:
    sigjmp_buf _env;
    // explicit: to prevent the compiler to cast a non thread_entry_point variable to be thread_entry_point.
    // (happens in C++ automatically when a constructor has exactly one argument)
    explicit Thread (thread_entry_point entryPoint);
    Thread();
    ~Thread ();
    unsigned int getQuantums () const;
    void incQuantums ();
    ThreadState getState ();
    int getSleepTime () const;
    void decSleepTime ();
    void setSleepTime (int num_quantums);
    void setState (ThreadState state);



private:

    thread_entry_point _entryPoint;
    char *_threadStack = nullptr;
    unsigned int _quantumCounter = 1;
    ThreadState _state;
    int _sleepTime = 0;

};
