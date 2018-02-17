// Threads wrappers


#include <unistd.h>
#include <unistd.h>
#include <signal.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_faults.h"
#include "dps8_iom.h"
#include "dps8_utils.h"

#include "threadz.h"
#ifdef __FreeBSD__
#include <pthread_np.h>
#endif

//
// Resource locks
//

// simh library serializer

static pthread_mutex_t simh_lock = PTHREAD_MUTEX_INITIALIZER;

void lock_simh (void)
  {
    pthread_mutex_lock (& simh_lock);
  }

void unlock_simh (void)
  {
    pthread_mutex_unlock (& simh_lock);
  }

// libuv library serializer

static pthread_mutex_t libuv_lock = PTHREAD_MUTEX_INITIALIZER;

void lock_libuv (void)
  {
    pthread_mutex_lock (& libuv_lock);
  }

void unlock_libuv (void)
  {
    pthread_mutex_unlock (& libuv_lock);
  }

// Memory serializer

//   addrmods RMW lock
//     Read()
//     Write()
//
//   CPU -- reset locks
//
//   core_read/core_write locks
//
//   read_operand/write operand RMW lock
//     Read()
//     Write()
//
//  IOM 

// mem_lock -- memory atomicity lock
// rmw_lock -- big R/M/W cycle lock

#ifndef LOCKLESS
pthread_rwlock_t mem_lock = PTHREAD_RWLOCK_INITIALIZER;
static __thread bool have_mem_lock = false;
static __thread bool have_rmw_lock = false;

bool get_rmw_lock (void)
  {
    return have_rmw_lock;
  }

void lock_rmw (void)
  {
    if (have_rmw_lock)
      {
        sim_warn ("%s: Already have RMW lock\n", __func__);
        return;
      }
    if (have_mem_lock)
      {
        sim_warn ("%s: Already have memory lock\n", __func__);
        return;
      }
    int rc= pthread_rwlock_wrlock (& mem_lock);
    if (rc)
      sim_printf ("%s pthread_rwlock_rdlock mem_lock %d\n", __func__, rc);
    have_mem_lock = true;
    have_rmw_lock = true;
  }

void lock_mem_rd (void)
  {
    // If the big RMW lock is on, do nothing.
    if (have_rmw_lock)
      return;

    if (have_mem_lock)
      {
        sim_warn ("%s: Already have memory lock\n", __func__);
        return;
      }
    int rc= pthread_rwlock_rdlock (& mem_lock);
    if (rc)
      sim_printf ("%s pthread_rwlock_rdlock mem_lock %d\n", __func__, rc);
    have_mem_lock = true;
  }

void lock_mem_wr (void)
  {
    // If the big RMW lock is on, do nothing.
    if (have_rmw_lock)
      return;

    if (have_mem_lock)
      {
        sim_warn ("%s: Already have memory lock\n", __func__);
        return;
      }
    int rc= pthread_rwlock_wrlock (& mem_lock);
    if (rc)
      sim_printf ("%s pthread_rwlock_wrlock mem_lock %d\n", __func__, rc);
    have_mem_lock = true;
  }


void unlock_rmw (void)
  {
    if (! have_mem_lock)
      {
        sim_warn ("%s: Don't have memory lock\n", __func__);
        return;
      }
    if (! have_rmw_lock)
      {
        sim_warn ("%s: Don't have RMW lock\n", __func__);
        return;
      }

    int rc = pthread_rwlock_unlock (& mem_lock);
    if (rc)
      sim_printf ("%s pthread_rwlock_ublock mem_lock %d\n", __func__, rc);
    have_mem_lock = false;
    have_rmw_lock = false;
  }

void unlock_mem (void)
  {
    if (have_rmw_lock)
      return; 
    if (! have_mem_lock)
      {
        sim_warn ("%s: Don't have memory lock\n", __func__);
        return;
      }

    int rc = pthread_rwlock_unlock (& mem_lock);
    if (rc)
      sim_printf ("%s pthread_rwlock_ublock mem_lock %d\n", __func__, rc);
    have_mem_lock = false;
  }

void unlock_mem_force (void)
  {
    if (have_mem_lock)
      {
        int rc = pthread_rwlock_unlock (& mem_lock);
        if (rc)
          sim_printf ("%s pthread_rwlock_unlock mem_lock %d\n", __func__, rc);
      }
    have_mem_lock = false;
    have_rmw_lock = false;
  }
#endif

// SCU serializer

static pthread_spinlock_t scu_lock;

void lock_scu (void)
  {
    //sim_debug (DBG_TRACE, & cpu_dev, "lock_scu\n");
    int rc;
    rc = pthread_spin_lock (& scu_lock);
    if (rc)
      sim_printf ("lock_scu pthread_spin_lock scu %d\n", rc);
  }

void unlock_scu (void)
  {
    //sim_debug (DBG_TRACE, & cpu_dev, "unlock_scu\n");
    int rc;
    rc = pthread_spin_unlock (& scu_lock);
    if (rc)
      sim_printf ("unlock_scu pthread_spin_lock scu %d\n", rc);
  }


// IOM serializer

static pthread_spinlock_t iom_lock;

void lock_iom (void)
  {
    int rc;
    rc = pthread_spin_lock (& iom_lock);
    if (rc)
      sim_printf ("%s pthread_spin_lock iom %d\n", __func__, rc);
  }

void unlock_iom (void)
  {
    int rc;
    rc = pthread_spin_unlock (& iom_lock);
    if (rc)
      sim_printf ("%s pthread_spin_lock iom %d\n", __func__, rc);
  }


// Debugging tool

static pthread_mutex_t tst_lock = PTHREAD_MUTEX_INITIALIZER;

void lock_tst (void)
  {
    //sim_debug (DBG_TRACE, & cpu_dev, "lock_tst\n");
    int rc;
    rc = pthread_mutex_lock (& tst_lock);
    if (rc)
      sim_printf ("lock_tst pthread_mutex_lock tst_lock %d\n", rc);
  }

void unlock_tst (void)
  {
    //sim_debug (DBG_TRACE, & cpu_dev, "unlock_tst\n");
    int rc;
    rc = pthread_mutex_unlock (& tst_lock);
    if (rc)
      sim_printf ("unlock_tst pthread_mutex_lock tst_lock %d\n", rc);
  }

// assertion

bool test_tst_lock (void)
  {
    //sim_debug (DBG_TRACE, & cpu_dev, "test_tst_lock\n");
    int rc;
    rc = pthread_mutex_trylock (& tst_lock);
    if (rc)
      {
         // couldn't lock; presumably already  
         return true;
      }
    // lock acquired, it wasn't locked
    rc = pthread_mutex_unlock (& tst_lock);
    if (rc)
      sim_printf ("test_tst_lock pthread_mutex_lock tst_lock %d\n", rc);
    return false;   
  }


////////////////////////////////////////////////////////////////////////////////
//
// CPU threads
//
////////////////////////////////////////////////////////////////////////////////

//
// main thread
//   createCPUThread 
//
// CPU and SCU thread
//   setCPURun (bool)
// CPU thread
//   cpuRunningWait
//   sleepCPU
// SCU thread
//   wakeCPU
//
// cpuThread:
//
//    while (1)
//      {
//        cpuRunningWait
//        compute...
//        dis:  sleepCPU
//      }

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

    rc = pthread_create (& p->cpuThread, NULL, cpu_thread_main, 
                    & p->cpuThreadArg);
    if (rc)
      sim_printf ("createCPUThread pthread_create %d\n", rc);

    char nm [17];
    sprintf (nm, "CPU %c", 'a' + cpuNum);
#ifndef __FreeBSD__
    pthread_setname_np (p->cpuThread, nm);
#else
    pthread_set_name_np (p->cpuThread, nm);
#endif
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
    struct cpuThreadz_t * p = & cpuThreadz[current_running_cpu_idx];
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
// Return time left
unsigned long  sleepCPU (unsigned long usec)
  {
    int rc;
    struct cpuThreadz_t * p = & cpuThreadz[current_running_cpu_idx];
    struct timespec abstime;
    clock_gettime (CLOCK_REALTIME, & abstime);
    abstime.tv_nsec += (long int) usec * 1000;
    abstime.tv_sec += abstime.tv_nsec / 1000000000;
    abstime.tv_nsec %= 1000000000;
    rc = pthread_mutex_lock (& p->sleepLock);
    if (rc)
      sim_printf ("sleepCPU pthread_mutex_lock %d\n", rc);
    rc = pthread_cond_timedwait (& p->sleepCond,
                                 & p->sleepLock,
                                 & abstime);
    if (rc && rc != ETIMEDOUT)
      sim_printf ("sleepCPU pthread_cond_timedwait %d\n", rc);
    bool timeout = rc == ETIMEDOUT;
    rc = pthread_mutex_unlock (& p->sleepLock);
    if (rc)
      sim_printf ("sleepCPU pthread_mutex_unlock %d\n", rc);
    //sim_printf ("pthread_cond_timedwait %lu %d\n", nsec, n);
    if (timeout)
      return 0;
    struct timespec newtime, delta;
    clock_gettime (CLOCK_REALTIME, & newtime);
    timespec_diff (& abstime, & newtime, & delta);
    if (delta.tv_nsec < 0)
      return 0; // safety
    return (unsigned long) delta.tv_nsec / 1000;
  }

// Called to wake sleeping CPU; such as interrupt during DIS

void wakeCPU (uint cpuNum)
  {
    int rc;
    struct cpuThreadz_t * p = & cpuThreadz[cpuNum];
    rc = pthread_mutex_lock (& p->sleepLock);
    if (rc)
      sim_printf ("wakeCPU pthread_mutex_lock %d\n", rc);
    //p->run = run;
    rc = pthread_cond_signal (& p->sleepCond);
    if (rc)
      sim_printf ("wakeCPU pthread_cond_signal %d\n", rc);
    rc = pthread_mutex_unlock (& p->sleepLock);
    if (rc)
      sim_printf ("wakeCPU pthread_mutex_unlock %d\n", rc);
  }

#ifdef IO_THREADZ
////////////////////////////////////////////////////////////////////////////////
//
// IOM threads
//
////////////////////////////////////////////////////////////////////////////////

// main thread
//   createIOMThread    create thread
//   iomRdyWait         wait for IOM started
//   setIOMInterrupt    signal IOM to start
//   iomDoneWait        wait for IOM done
// IOM thread
//   iomInterruptWait   IOM thread wait for work
//   iomInterruptDone   IOM thread signal done working
//
//   IOM thread
//     while (1)
//       {
//         iomInterruptWake
//         work...
//         iomInterruptDone
//      }

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

    rc = pthread_create (& p->iomThread, NULL, iom_thread_main, 
                    & p->iomThreadArg);
    if (rc)
      sim_printf ("createIOMThread pthread_create %d\n", rc);

    char nm [17];
    sprintf (nm, "IOM %c", 'a' + iomNum);
#ifndef __FreeBSD__
    pthread_setname_np (p->iomThread, nm);
#else
    pthread_set_name_np (p->cpuThread, nm);
#endif
  }

// Called by IOM thread to block until CIOC call

void iomInterruptWait (void)
  {
    int rc;
    struct iomThreadz_t * p = & iomThreadz[this_iom_idx];
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
      sim_printf ("iom thread %d in %d out %d\n", this_iom_idx,
                  p->inCnt, p->outCnt);
#endif
  }

// Called by IOM thread to signal CIOC complete

void iomInterruptDone (void)
  {
    int rc;
    struct iomThreadz_t * p = & iomThreadz[this_iom_idx];
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

// main thread
//   createChnThread    create thread
// IOM thread
//   chnRdyWait         wait for channel started
//   setChnConnect      signal channel to start
// Channel thread
//   chnConnectWait     Channel thread wait for work
//   chnConnectDone     Channel thread signal done working
//
//   IOM thread
//     while (1)
//       {
//         iomInterruptWake
//         work...
//         iomInterruptDone
//      }
struct chnThreadz_t chnThreadz [N_IOM_UNITS_MAX] [MAX_CHANNELS];

// Create channel thread

void createChnThread (uint iomNum, uint chnNum, const char * devTypeStr)
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

    rc = pthread_create (& p->chnThread, NULL, chan_thread_main, 
                    & p->chnThreadArg);
    if (rc)
      sim_printf ("createChnThread pthread_create %d\n", rc);

    char nm [17];
    sprintf (nm, "chn %c/%u %s", 'a' + iomNum, chnNum, devTypeStr);
#ifndef __FreeBSD__
    pthread_setname_np (p->chnThread, nm);
#else
    pthread_set_name_np (p->cpuThread, nm);
#endif
  }

// Called by channel thread to block until I/O command presented

void chnConnectWait (void)
  {
    int rc;
    struct chnThreadz_t * p = & chnThreadz[this_iom_idx][this_chan_num];

   
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
      sim_printf ("chn thread %d in %d out %d\n", this_chan_num,
                  p->inCnt, p->outCnt);
#endif
  }

// Called by channel thread to signal I/O complete

void chnConnectDone (void)
  {
    int rc;
    struct chnThreadz_t * p = & chnThreadz[this_iom_idx][this_chan_num];
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
#endif

void initThreadz (void)
  {
#ifdef IO_THREADZ
    // chnThreadz is sparse; make sure 'started' is false
    memset (chnThreadz, 0, sizeof (chnThreadz));
#endif

#ifndef LOCKLESS
    //pthread_rwlock_init (& mem_lock, PTHREAD_PROCESS_PRIVATE);
    have_mem_lock = false;
    have_rmw_lock = false;
#endif

    pthread_spin_init (& scu_lock, PTHREAD_PROCESS_PRIVATE);
    pthread_spin_init (& iom_lock, PTHREAD_PROCESS_PRIVATE);
  }

// Set up per-thread signal handlers

void int_handler (int signal);

void setSignals (void)
  {
    struct sigaction act;
    memset (& act, 0, sizeof (act));
    act.sa_handler = int_handler;
    act.sa_flags = 0;
    sigaction (SIGINT, & act, NULL);
    //sigaction (SIGHUP, & act, NULL);
    sigaction (SIGTERM, & act, NULL);
  }

// Force cache coherency

static pthread_mutex_t fenceLock = PTHREAD_MUTEX_INITIALIZER;
void fence (void)
  {
    pthread_mutex_lock (& fenceLock);
    pthread_mutex_unlock (& fenceLock);
  }
