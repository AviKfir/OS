#include "Thread.h"
#include <csignal>

/* code for 64 bit Intel arch */
typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

/**
 * constructor
 * @param entryPoint the start address for the thread function.
 */
Thread::Thread (thread_entry_point entryPoint)
{
    _entryPoint = entryPoint;
    _threadStack = new char[STACK_SIZE];
    _quantumCounter = 0;
    _state = READY;

    // initializes env to use the right stack, and to run from the function 'entry_point', when we'll use
    // siglongjmp to jump into the thread.
    address_t sp = (address_t) this->_threadStack + STACK_SIZE - sizeof(address_t);
    auto pc = (address_t) _entryPoint;
    sigsetjmp(_env, 1);
    (_env->__jmpbuf)[JB_SP] = translate_address(sp);
    (_env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&_env->__saved_mask);
};

/**
 * constructor only for main thread
 */
Thread::Thread() {
    _quantumCounter = 1;
    _state = RUNNING;
    sigsetjmp((_env), 1);
    sigemptyset(&(_env)->__saved_mask);
}

/**
 * destructor
 */
Thread::~Thread ()
{
    delete[] _threadStack;
    _threadStack = nullptr;
}

/**
 * Getter for num of quantums.
 */
unsigned int Thread::getQuantums () const
{
    return _quantumCounter;
}

/**
 * increment the quantums time by 1.
 */
void Thread::incQuantums ()
{
    _quantumCounter++;
}

/**
 * Getter for the thread state.
 */
ThreadState Thread::getState ()
{
    return _state;
}

/**
 * Getter for the thread sleep time.
 */
int Thread::getSleepTime () const
{
    return _sleepTime;
}

/**
 * decrement the quantums time by 1.
 */
void Thread::decSleepTime ()
{
    _sleepTime--;
}

/**
 * Setter for the thread sleep time.
 */
void Thread::setSleepTime (int num_quantums)
{
    _sleepTime = num_quantums;
}

/**
 * Setter for the thread state.
 */
void Thread::setState (ThreadState state)
{
    _state = state;
}
