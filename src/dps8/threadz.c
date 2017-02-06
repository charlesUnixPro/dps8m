// Threads wrappers

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"

#include "threadz.h"

__thread uint thisCPUnum;
__thread uint thisIOMnum;
__thread uint thisChnNum;

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

void createCPUThread (uint cpuNum)
  {
    struct cpuThreadz_t * p = & cpuThreadz[cpuNum];
    p->cpuThreadArg = (int) cpuNum;
    // initialize run/stop switch
    cthread_mutex_init (& p->runLock, NULL);
    cthread_cond_init (& p->runCond, NULL);
    p->run = false;

    // initialize DIS sleep
    cthread_mutex_init (& p->sleepLock, NULL);
    cthread_cond_init (& p->sleepCond, NULL);

    cthread_create (& p->cpuThread, NULL, cpuThreadMain, 
                    & p->cpuThreadArg);
  }

void setCPURun (uint cpuNum, bool run)
  {
    struct cpuThreadz_t * p = & cpuThreadz[cpuNum];
    cthread_mutex_lock (& p->runLock);
    p->run = run;
    cthread_cond_signal (& p->runCond);
    cthread_mutex_unlock (& p->runLock);
  }

void cpuRunningWait (void)
  {
    struct cpuThreadz_t * p = & cpuThreadz[thisCPUnum];
    if (p->run)
      return;
    cthread_mutex_lock (& p->runLock);
    while (! p->run)
      cthread_cond_wait (& p->runCond,
                         & p->runLock);
    cthread_mutex_unlock (& p->runLock);
  }

void sleepCPU (unsigned long nsec)
  {
    struct cpuThreadz_t * p = & cpuThreadz[thisCPUnum];
    struct timespec abstime;
    clock_gettime (CLOCK_REALTIME, & abstime);
    abstime.tv_nsec += nsec;
    abstime.tv_sec += abstime.tv_nsec / 1000000000;
    abstime.tv_nsec %= 1000000000;
    cthread_mutex_lock (& p->sleepLock);
    //p->sleep = false;
    //while (! p->sleep)
    //  int n = 
    cthread_cond_timedwait (& p->sleepCond,
                            & p->sleepLock,
                            & abstime);
    cthread_mutex_unlock (& p->sleepLock);
    //sim_printf ("cthread_cond_timedwait %lu %d\n", nsec, n);
  }

// IOM threads

struct iomThreadz_t iomThreadz [N_IOM_UNITS_MAX];

void createIOMThread (uint iomNum)
  {
    struct iomThreadz_t * p = & iomThreadz[iomNum];
#ifdef tdbg
    p->inCnt = 0;
    p->outCnt = 0;
#endif
    p->iomThreadArg = (int) iomNum;

    // initialize interrupt wait
    p->intr = false;
    cthread_mutex_init (& p->intrLock, NULL);
    cthread_cond_init (& p->intrCond, NULL);

    cthread_create (& p->iomThread, NULL, iomThreadMain, 
                    & p->iomThreadArg);
  }

void iomInterruptWait (void)
  {
    struct iomThreadz_t * p = & iomThreadz[thisIOMnum];
    cthread_mutex_lock (& p->intrLock);
    while (! p->intr)
      cthread_cond_wait (& p->intrCond, & p->intrLock);
#ifdef tdbg
    p->outCnt++;
    if (p->inCnt != p->outCnt)
      sim_printf ("iom thread %d in %d out %d\n", thisIOMnum,
                  p->inCnt, p->outCnt);
#endif
  }

void iomInterruptDone (void)
  {
    struct iomThreadz_t * p = & iomThreadz[thisIOMnum];
    p->intr = false;
    cthread_mutex_unlock (& p->intrLock);
  }

void setIOMInterrupt (uint iomNum)
  {
    struct iomThreadz_t * p = & iomThreadz[iomNum];
    cthread_mutex_lock (& p->intrLock);
#ifdef tdbg
    p->inCnt++;
#endif
    p->intr = true;
    cthread_cond_signal (& p->intrCond);
    cthread_mutex_unlock (& p->intrLock);
  }

// Channel threads

struct chnThreadz_t chnThreadz [N_IOM_UNITS_MAX] [MAX_CHANNELS];

void createChnThread (uint iomNum, uint chnNum)
  {
    struct chnThreadz_t * p = & chnThreadz[iomNum][chnNum];
    p->chnThreadArg = (int) (chnNum + iomNum * MAX_CHANNELS);

#ifdef tdbg
    p->inCnt = 0;
    p->outCnt = 0;
#endif
    // initialize interrupt wait
    p->connect = false;
    cthread_mutex_init (& p->connectLock, NULL);
    cthread_cond_init (& p->connectCond, NULL);

    cthread_create (& p->chnThread, NULL, chnThreadMain, 
                    & p->chnThreadArg);
  }

void chnConnectWait (void)
  {
    struct chnThreadz_t * p = & chnThreadz[thisIOMnum][thisChnNum];
    cthread_mutex_lock (& p->connectLock);
    while (! p->connect)
      cthread_cond_wait (& p->connectCond, & p->connectLock);
#ifdef tdbg
    p->outCnt++;
    if (p->inCnt != p->outCnt)
      sim_printf ("chn thread %d in %d out %d\n", thisChnNum,
                  p->inCnt, p->outCnt);
#endif
  }

void chnConnectDone (void)
  {
    struct chnThreadz_t * p = & chnThreadz[thisIOMnum][thisChnNum];
    p->connect = false;
    cthread_mutex_unlock (& p->connectLock);
  }

void setChnConnect (uint iomNum, uint chnNum)
  {
    struct chnThreadz_t * p = & chnThreadz[iomNum][chnNum];
    cthread_mutex_lock (& p->connectLock);
    p->connect = true;
#ifdef tdbg
    p->inCnt++;
#endif
    cthread_cond_signal (& p->connectCond);
    cthread_mutex_unlock (& p->connectLock);
  }

void initThreadz (void)
  {
    // chnThreadz is sparse; make sure 'started' is false
    memset (chnThreadz, 0, sizeof (chnThreadz));
  }
