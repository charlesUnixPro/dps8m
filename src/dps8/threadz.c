// Threads wrappers

#include "dps8.h"
#include "dps8_cpu.h"

#include "threadz.h"

//
// Resource locks
//

static cthread_mutex_t simh_lock;
void lock_simh (void)
  {
    cthread_mutex_lock (& simh_lock);
  }
void unlock_simh (void)
  {
    cthread_mutex_unlock (& simh_lock);
  }

static cthread_mutex_t libuv_lock;
void lock_libuv (void)
  {
    cthread_mutex_lock (& libuv_lock);
  }
void unlock_libuv (void)
  {
    cthread_mutex_unlock (& libuv_lock);
  }

// cpu threads

struct cpuThreadz_t cpuThreadz [N_CPU_UNITS_MAX];

// Create CPU thread

#if 0
// temp
static void * cpuThreadMain (void * arg)
  {
    int myid = * (int *) arg;
    sim_printf("Hello, world, I'm %d\n",myid);

    // wait on run/switch

    cthread_mutex_lock (& cpuThreadz[myid].runLock);
    while (! cpuThreadz[myid].run)
      cthread_cond_wait (& cpuThreadz[myid].runCond, & cpuThreadz[myid].runLock);
    cthread_mutex_unlock (& cpuThreadz[myid].runLock);
    sim_printf("running %d\n",myid);
    return arg;
  }
#endif

void createCPUThread (uint cpuNum)
  {
    cpuThreadz[cpuNum].cpuThreadArg = (int) cpuNum;
    // initialize run/stop switch
    cthread_mutex_init (& cpuThreadz[cpuNum].runLock, NULL);
    cthread_cond_init (& cpuThreadz[cpuNum].runCond, NULL);
    cpuThreadz[cpuNum].run = false;

    // initialize DIS sleep
    cthread_mutex_init (& cpuThreadz[cpuNum].sleepLock, NULL);
    cthread_cond_init (& cpuThreadz[cpuNum].sleepCond, NULL);

    cthread_create (& cpuThreadz[cpuNum].cpuThread, NULL, cpuThreadMain, 
                    & cpuThreadz[cpuNum].cpuThreadArg);
  }

void setCPURun (uint cpuNum, bool run)
  {
    cthread_mutex_lock (& cpuThreadz[cpuNum].runLock);
    cpuThreadz[cpuNum].run = run;
    cthread_cond_signal (& cpuThreadz[cpuNum].runCond);
    cthread_mutex_unlock (& cpuThreadz[cpuNum].runLock);
  }

void sleepCPU (unsigned long nsec)
  {
    struct timespec abstime;
    clock_gettime (CLOCK_REALTIME, & abstime);
    abstime.tv_nsec += nsec;
    abstime.tv_sec += abstime.tv_nsec / 1000000000;
    abstime.tv_nsec %= 1000000000;
    cthread_mutex_lock (& cpuThreadz[currentRunningCPUnum].sleepLock);
    //cpuThreadz[currentRunningCPUnum].sleep = false;
    //while (! cpuThreadz[currentRunningCPUnum].sleep)
      int n = cthread_cond_timedwait (& cpuThreadz[currentRunningCPUnum].sleepCond,
                              & cpuThreadz[currentRunningCPUnum].sleepLock,
                              & abstime);
    cthread_mutex_unlock (& cpuThreadz[currentRunningCPUnum].sleepLock);
    //sim_printf ("cthread_cond_timedwait %lu %d\n", nsec, n);
  }
