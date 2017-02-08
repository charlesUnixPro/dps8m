// Threads wrappers

#include <unistd.h>

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

static pthread_mutex_t simh_lock = PTHREAD_MUTEX_INITIALIZER;

void lock_simh (void)
  {
    pthread_mutex_lock (& simh_lock);
  }

void unlock_simh (void)
  {
    pthread_mutex_unlock (& simh_lock);
  }

static pthread_mutex_t libuv_lock = PTHREAD_MUTEX_INITIALIZER;

void lock_libuv (void)
  {
    pthread_mutex_lock (& libuv_lock);
  }

void unlock_libuv (void)
  {
    pthread_mutex_unlock (& libuv_lock);
  }

////////////////////////////////////////////////////////////////////////////////
//
// CPU threads
//
////////////////////////////////////////////////////////////////////////////////

struct cpuThreadz_t cpuThreadz [N_CPU_UNITS_MAX];

// Create CPU thread

void createCPUThread (uint cpuNum)
  {
    int rc;
    struct cpuThreadz_t * p = & cpuThreadz[cpuNum];
    p->cpuThreadArg = (int) cpuNum;
    // initialize run/stop switch
    rc = pthread_mutex_init (& p->runLock, NULL);
    if (rc)
      sim_printf ("createCPUThread pthread_mutex_init runLock %d\n", rc);
    rc = pthread_cond_init (& p->runCond, NULL);
    if (rc)
      sim_printf ("createCPUThread pthread_cond_init runCond %d\n", rc);
    p->run = false;
    //p->ready = false;

    // initialize DIS sleep
    rc = pthread_mutex_init (& p->sleepLock, NULL);
    if (rc)
      sim_printf ("createCPUThread pthread_mutex_init sleepLock %d\n", rc);
    rc = pthread_cond_init (& p->sleepCond, NULL);
    if (rc)
      sim_printf ("createCPUThread pthread_cond_init sleepCond %d\n", rc);

    rc = pthread_create (& p->cpuThread, NULL, cpuThreadMain, 
                    & p->cpuThreadArg);
    if (rc)
      sim_printf ("createCPUThread pthread_create %d\n", rc);
  }

#if 0
void cpuRdyWait (uint cpuNum)
  {
    struct cpuThreadz_t * p = & cpuThreadz[cpuNum];
    while (! p -> ready)
      usleep (10000);
   }
#endif

// Set CPU thread run/sleep

void setCPURun (uint cpuNum, bool run)
  {
    int rc;
    struct cpuThreadz_t * p = & cpuThreadz[cpuNum];
    rc = pthread_mutex_lock (& p->runLock);
    if (rc)
      sim_printf ("setCPUrun pthread_mutex_lock %d\n", rc);
    p->run = run;
    rc = pthread_cond_signal (& p->runCond);
    if (rc)
      sim_printf ("setCPUrun pthread_cond_signal %d\n", rc);
    rc = pthread_mutex_unlock (& p->runLock);
    if (rc)
      sim_printf ("setCPUrun pthread_mutex_unlock %d\n", rc);
  }

// Called by CPU thread to block on run/sleep

void cpuRunningWait (void)
  {
    int rc;
    struct cpuThreadz_t * p = & cpuThreadz[thisCPUnum];
    if (p->run)
      return;
    rc = pthread_mutex_lock (& p->runLock);
    if (rc)
      sim_printf ("cpuRunningWait pthread_mutex_lock %d\n", rc);
    while (! p->run)
      {
        rc = pthread_cond_wait (& p->runCond, & p->runLock);
        if (rc)
          sim_printf ("cpuRunningWait pthread_cond_wait %d\n", rc);
     }
    rc = pthread_mutex_unlock (& p->runLock);
    if (rc)
      sim_printf ("cpuRunningWait pthread_mutex_unlock %d\n", rc);
  }

// Called by CPU thread to sleep until time up or signaled

void sleepCPU (unsigned long nsec)
  {
    int rc;
    struct cpuThreadz_t * p = & cpuThreadz[thisCPUnum];
    struct timespec abstime;
    clock_gettime (CLOCK_REALTIME, & abstime);
    abstime.tv_nsec += nsec;
    abstime.tv_sec += abstime.tv_nsec / 1000000000;
    abstime.tv_nsec %= 1000000000;
    rc = pthread_mutex_lock (& p->sleepLock);
    if (rc)
      sim_printf ("sleepCPU pthread_mutex_lock %d\n", rc);
    //p->sleep = false;
    //while (! p->sleep)
    //  int n = 
    rc = pthread_cond_timedwait (& p->sleepCond,
                                 & p->sleepLock,
                                 & abstime);
    if (rc && rc != ETIMEDOUT)
      sim_printf ("sleepCPU pthread_cond_timedwait %d\n", rc);
    rc = pthread_mutex_unlock (& p->sleepLock);
    if (rc)
      sim_printf ("sleepCPU pthread_mutex_unlock %d\n", rc);
    //sim_printf ("pthread_cond_timedwait %lu %d\n", nsec, n);
  }

////////////////////////////////////////////////////////////////////////////////
//
// IOM threads
//
////////////////////////////////////////////////////////////////////////////////

struct iomThreadz_t iomThreadz [N_IOM_UNITS_MAX];

// Create IOM thread

void createIOMThread (uint iomNum)
  {
    int rc;
    struct iomThreadz_t * p = & iomThreadz[iomNum];
#ifdef tdbg
    p->inCnt = 0;
    p->outCnt = 0;
#endif
    p->iomThreadArg = (int) iomNum;

    p->ready = false;
    // initialize interrupt wait
    p->intr = false;
    rc = pthread_mutex_init (& p->intrLock, NULL);
    if (rc)
      sim_printf ("createIOMThread pthread_mutex_init intrLock %d\n", rc);
    rc = pthread_cond_init (& p->intrCond, NULL);
    if (rc)
      sim_printf ("createIOMThread pthread_cond_init intrCond %d\n", rc);

    rc = pthread_create (& p->iomThread, NULL, iomThreadMain, 
                    & p->iomThreadArg);
    if (rc)
      sim_printf ("createIOMThread pthread_create %d\n", rc);
  }

// Called by IOM thread to block until CIOC call

void iomInterruptWait (void)
  {
    int rc;
    struct iomThreadz_t * p = & iomThreadz[thisIOMnum];
    rc = pthread_mutex_lock (& p->intrLock);
    if (rc)
      sim_printf ("iomInterruptWait pthread_mutex_lock %d\n", rc);
    p -> ready = true;
    while (! p->intr)
      {
        rc = pthread_cond_wait (& p->intrCond, & p->intrLock);
        if (rc)
          sim_printf ("iomInterruptWait pthread_cond_wait %d\n", rc);
      }
#ifdef tdbg
    p->outCnt++;
    if (p->inCnt != p->outCnt)
      sim_printf ("iom thread %d in %d out %d\n", thisIOMnum,
                  p->inCnt, p->outCnt);
#endif
  }

// Called by IOM thread to signal CIOC complete

void iomInterruptDone (void)
  {
    int rc;
    struct iomThreadz_t * p = & iomThreadz[thisIOMnum];
    p->intr = false;
    rc = pthread_cond_signal (& p->intrCond);
    if (rc)
      sim_printf ("iomInterruptDone pthread_cond_signal %d\n", rc);
    rc = pthread_mutex_unlock (& p->intrLock);
    if (rc)
      sim_printf ("iomInterruptDone pthread_mutex_unlock %d\n", rc);
  }

// Called by CPU thread to wait for iomInterruptDone

void iomDoneWait (uint iomNum)
  {
    int rc;
    struct iomThreadz_t * p = & iomThreadz[iomNum];
    rc = pthread_mutex_lock (& p->intrLock);
    if (rc)
      sim_printf ("iomDoneWait pthread_mutex_lock %d\n", rc);
    while (p->intr)
      {
        rc = pthread_cond_wait (& p->intrCond, & p->intrLock);
        if (rc)
          sim_printf ("iomDoneWait pthread_cond_wait %d\n", rc);
      }
    rc = pthread_mutex_unlock (& p->intrLock);
    if (rc)
      sim_printf ("iomDoneWait pthread_mutex_unlock %d\n", rc);
  }


// Signal CIOC to IOM thread

void setIOMInterrupt (uint iomNum)
  {
    int rc;
    struct iomThreadz_t * p = & iomThreadz[iomNum];
    rc = pthread_mutex_lock (& p->intrLock);
    if (rc)
      sim_printf ("setIOMInterrupt pthread_mutex_lock %d\n", rc);
    while (p->intr)
      {
        rc = pthread_cond_wait(&p->intrCond, &p->intrLock);
        if (rc)
          sim_printf ("setIOMInterrupt pthread_cond_wait intrLock %d\n", rc);
      }
#ifdef tdbg
    p->inCnt++;
#endif
    p->intr = true;
    rc = pthread_cond_signal (& p->intrCond);
    if (rc)
      sim_printf ("setIOMInterrupt pthread_cond_signal %d\n", rc);
    rc = pthread_mutex_unlock (& p->intrLock);
    if (rc)
      sim_printf ("setIOMInterrupt pthread_mutex_unlock %d\n", rc);
  }

// Wait for IOM thread to initialize

void iomRdyWait (uint iomNum)
  {
    struct iomThreadz_t * p = & iomThreadz[iomNum];
    while (! p -> ready)
      usleep (10000);
   }


////////////////////////////////////////////////////////////////////////////////
//
// Channel threads
//
////////////////////////////////////////////////////////////////////////////////

struct chnThreadz_t chnThreadz [N_IOM_UNITS_MAX] [MAX_CHANNELS];

// Create channel thread

void createChnThread (uint iomNum, uint chnNum)
  {
    int rc;
    struct chnThreadz_t * p = & chnThreadz[iomNum][chnNum];
    p->chnThreadArg = (int) (chnNum + iomNum * MAX_CHANNELS);

#ifdef tdbg
    p->inCnt = 0;
    p->outCnt = 0;
#endif
    p->ready = false;
    // initialize interrupt wait
    p->connect = false;
    rc = pthread_mutex_init (& p->connectLock, NULL);
    if (rc)
      sim_printf ("createChnThread pthread_mutex_init connectLock %d\n", rc);
    rc = pthread_cond_init (& p->connectCond, NULL);
    if (rc)
      sim_printf ("createChnThread pthread_cond_init connectCond %d\n", rc);

    rc = pthread_create (& p->chnThread, NULL, chnThreadMain, 
                    & p->chnThreadArg);
    if (rc)
      sim_printf ("createChnThread pthread_create %d\n", rc);
  }

// Called by channel thread to block until I/O command presented

void chnConnectWait (void)
  {
    int rc;
    struct chnThreadz_t * p = & chnThreadz[thisIOMnum][thisChnNum];

   
    rc = pthread_mutex_lock (& p->connectLock);
    if (rc)
      sim_printf ("chnConnectWait pthread_mutex_lock %d\n", rc);
    p -> ready = true;
    while (! p->connect)
      {
        rc = pthread_cond_wait (& p->connectCond, & p->connectLock);
        if (rc)
          sim_printf ("chnConnectWait pthread_cond_wait %d\n", rc);
      }
#ifdef tdbg
    p->outCnt++;
    if (p->inCnt != p->outCnt)
      sim_printf ("chn thread %d in %d out %d\n", thisChnNum,
                  p->inCnt, p->outCnt);
#endif
  }

// Called by channel thread to signal I/O complete

void chnConnectDone (void)
  {
    int rc;
    struct chnThreadz_t * p = & chnThreadz[thisIOMnum][thisChnNum];
    p->connect = false;
    rc = pthread_cond_signal (& p->connectCond);
    if (rc)
      sim_printf ("chnInterruptDone pthread_cond_signal %d\n", rc);
    rc = pthread_mutex_unlock (& p->connectLock);
    if (rc)
      sim_printf ("chnConnectDone pthread_mutex_unlock %d\n", rc);
  }

// Signal I/O presented to channel thread

void setChnConnect (uint iomNum, uint chnNum)
  {
    int rc;
    struct chnThreadz_t * p = & chnThreadz[iomNum][chnNum];
    rc = pthread_mutex_lock (& p->connectLock);
    if (rc)
      sim_printf ("setChnConnect pthread_mutex_lock %d\n", rc);
    while (p->connect)
      {
        rc = pthread_cond_wait(&p->connectCond, &p->connectLock);
        if (rc)
          sim_printf ("setChnInterrupt pthread_cond_wait connectLock %d\n", rc);
      }
#ifdef tdbg
    p->inCnt++;
#endif
    p->connect = true;
    rc = pthread_cond_signal (& p->connectCond);
    if (rc)
      sim_printf ("setChnConnect pthread_cond_signal %d\n", rc);
    rc = pthread_mutex_unlock (& p->connectLock);
    if (rc)
      sim_printf ("setChnConnect pthread_mutex_unlock %d\n", rc);
  }

// Block until channel thread ready

void chnRdyWait (uint iomNum, uint chnNum)
  {
    struct chnThreadz_t * p = & chnThreadz[iomNum][chnNum];
    while (! p -> ready)
      usleep (10000);
   }

void initThreadz (void)
  {
    // chnThreadz is sparse; make sure 'started' is false
    memset (chnThreadz, 0, sizeof (chnThreadz));
  }
