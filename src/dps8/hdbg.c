/*
 Copyright 2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// history debugging
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "dps8.h"
#include "hdbg.h"


#ifdef HDBG
#include "dps8_utils.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"

enum hevtType { hevtEmpty = 0, hevtTrace, hevtMRead, hevtMWrite, hevtIWBUpdate, hevtRegs, hevtFault, hevtReg, hevtPAReg };

struct hevt
  {
    enum hevtType type;
    uint64 time;
    union
      {
        struct
          {
            addr_modes_t addrMode;
            word15 segno;
            word18 ic;
            word3 ring;
            word36 inst;
          } trace;

        struct
          {
            word24 addr;
            word36 data;
          } memref;

        struct
          {
            _fault faultNumber;
            _fault_subtype subFault;
            char faultMsg [64];
          } fault;

        struct
          {
            enum hregs_t type;
            word36 data;
          } reg;

        struct
          {
            enum hregs_t type;
            struct _par data;
          } par;
      };
  };

static struct hevt * hevents = NULL;
static unsigned long hdbgSize = 0;
static unsigned long hevtPtr = 0;

#ifdef THREADZ
static pthread_mutex_t hdbg_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

static void createBuffer (void)
  {
    if (hevents)
      {
        free (hevents);
        hevents = NULL;
      }
    if (! hdbgSize)
      return;
    hevents = malloc (sizeof (struct hevt) * hdbgSize);
    if (! hevents)
      {
        sim_printf ("hdbg createBuffer failed\n");
        return;
      }
    memset (hevents, 0, sizeof (struct hevt) * hdbgSize);

    hevtPtr = 0;
  }

void hdbgTrace (void)
  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    if (! hevents)
      goto done;
#ifdef ISOLTS
if (currentRunningCpuIdx == 0)
  goto done;
#endif
    hevents [hevtPtr] . type = hevtTrace;
    hevents [hevtPtr] . time = cpu.cycleCnt;
    hevents [hevtPtr] . trace . addrMode = get_addr_mode ();
    hevents [hevtPtr] . trace . segno = cpu . PPR.PSR;
    hevents [hevtPtr] . trace . ic = cpu . PPR.IC;
    hevents [hevtPtr] . trace . ring = cpu . PPR.PRR;
    hevents [hevtPtr] . trace . inst = cpu.cu.IWB;
    hevtPtr = (hevtPtr + 1) % hdbgSize;
done: ;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }

void hdbgMRead (word24 addr, word36 data)
  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    if (! hevents)
      goto done;
#ifdef ISOLTS
if (currentRunningCpuIdx == 0)
  goto done;
#endif
    hevents [hevtPtr] . type = hevtMRead;
    hevents [hevtPtr] . time = cpu.cycleCnt;
    hevents [hevtPtr] . memref . addr = addr;
    hevents [hevtPtr] . memref . data = data;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
done: ;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }

void hdbgMWrite (word24 addr, word36 data)
  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    if (! hevents)
      goto done;
#ifdef ISOLTS
if (currentRunningCpuIdx == 0)
  goto done;
#endif
    hevents [hevtPtr] . type = hevtMWrite;
    hevents [hevtPtr] . time = cpu.cycleCnt;
    hevents [hevtPtr] . memref . addr = addr;
    hevents [hevtPtr] . memref . data = data;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
done: ;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }

void hdbgFault (_fault faultNumber, _fault_subtype subFault,
                const char * faultMsg)

  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    if (! hevents)
      goto done;
#ifdef ISOLTS
if (currentRunningCpuIdx == 0)
  goto done;
#endif
    hevents [hevtPtr] . type = hevtFault;
    hevents [hevtPtr] . time = cpu.cycleCnt;
    hevents [hevtPtr] . fault . faultNumber = faultNumber;
    hevents [hevtPtr] . fault . subFault = subFault;
    strncpy (hevents [hevtPtr] . fault . faultMsg, faultMsg, 63);
    hevents [hevtPtr] . fault . faultMsg [63] = 0;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
done: ;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }

void hdbgReg (enum hregs_t type, word36 data)
  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    if (! hevents)
      goto done;
#ifdef ISOLTS
if (currentRunningCpuIdx == 0)
  goto done;
#endif
    hevents[hevtPtr].type = hevtReg;
    hevents[hevtPtr].time = cpu.cycleCnt;
    hevents[hevtPtr].reg.type = type;
    hevents[hevtPtr].reg.data = data;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
done: ;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }


void hdbgPAReg (enum hregs_t type, struct _par * data)
  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    if (! hevents)
      goto done;
#ifdef ISOLTS
if (currentRunningCpuIdx == 0)
  goto done;
#endif
    hevents[hevtPtr].type = hevtPAReg;
    hevents[hevtPtr].time = cpu.cycleCnt;
    hevents[hevtPtr].par.type = type;
    hevents[hevtPtr].par.data =  * data;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
done: ;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }

static FILE * hdbgOut = NULL;

static void printMRead (struct hevt * p)
  {
    fprintf (hdbgOut, "DBG(%"PRId64")> CPU FINAL: Read %08o %012"PRIo64"\n",
                p -> time, 
                p -> memref . addr, p -> memref . data);
  }

static void printMWrite (struct hevt * p)
  {
    fprintf (hdbgOut, "DBG(%"PRId64")> CPU FINAL: Write %08o %012"PRIo64"\n",
                p -> time, 
                p -> memref . addr, p -> memref . data);
  }

static void printTrace (struct hevt * p)
  {
    char buf [256];
    if (p -> trace . addrMode == ABSOLUTE_mode)
      {
        fprintf (hdbgOut, "DBG(%"PRId64")> CPU TRACE: %06o %o %012"PRIo64" (%s)\n",
                    p -> time, 
                    p -> trace . ic, p -> trace . ring,
                    p -> trace . inst, disAssemble (buf, p -> trace . inst));
      }
    else
      {
        fprintf (hdbgOut, "DBG(%"PRId64")> CPU TRACE: %05o:%06o %o %012"PRIo64" (%s)\n",
                    p -> time, p -> trace . segno,
                    p -> trace . ic, p -> trace . ring,
                    p -> trace . inst, disAssemble (buf, p -> trace . inst));
      }
  }

static void printFault (struct hevt * p)
  {
    fprintf (hdbgOut, "DBG(%"PRId64")> CPU FAULT: Fault %d(0%o), sub %"PRId64"(0%"PRIo64"), '%s'\n",
                p -> time, 
                p -> fault.faultNumber, p -> fault.faultNumber,
                p -> fault.subFault.bits, p -> fault.subFault.bits,
                p -> fault.faultMsg);
  }

static char * regNames [] =
  {
    "A ",
    "Q ",
    "X0", "X1", "X2", "X3", "X4", "X5", "X6", "X7",
    "AR0", "AR1", "AR2", "AR3", "AR4", "AR5", "AR6", "AR7",
    "PR0", "PR1", "PR2", "PR3", "PR4", "PR5", "PR6", "PR7"
  };

static void printReg (struct hevt * p)
  {
    if (p->reg.type >= hreg_X0 && p->reg.type <= hreg_X7)
      fprintf (hdbgOut, "DBG(%"PRId64")> CPU REG: %s %06"PRIo64"\n",
                  p->time, 
                  regNames[p->reg.type],
                  p->reg.data);
    else
      fprintf (hdbgOut, "DBG(%"PRId64")> CPU REG: %s %012"PRIo64"\n",
                  p->time, 
                  regNames[p->reg.type],
                  p->reg.data);
  }

static void printPAReg (struct hevt * p)
  {
    if (p->reg.type >= hreg_PR0 && p->reg.type <= hreg_PR7)
      fprintf (hdbgOut, "DBG(%"PRId64")> CPU REG: %s "
               "%05o:%06o BIT %2o RNR %o\n",
               p->time, 
               regNames[p->reg.type],
               p->par.data.SNR,
               p->par.data.WORDNO,
               p->par.data.PR_BITNO,
               p->par.data.RNR);
    else
      fprintf (hdbgOut, "DBG(%"PRId64")> CPU REG: %s "
               "%05o:%06o CHAR %o BIT %2o RNR %o\n",
               p->time, 
               regNames[p->reg.type],
               p->par.data.SNR,
               p->par.data.WORDNO,
               p->par.data.AR_CHAR,
               p->par.data.AR_BITNO,
               p->par.data.RNR);
  }

void hdbgPrint (void)
  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    if (! hevents)
      goto done;
    hdbgOut = fopen ("hdbg.list", "w");
    if (! hdbgOut)
      {
        sim_printf ("can't open hdbg.list\n");
        goto done;
      }
    time_t curtime;
    time (& curtime);
    fprintf (hdbgOut, "%s\n", ctime (& curtime));

    for (unsigned long p = 0; p < hdbgSize; p ++)
      {
        unsigned long q = (hevtPtr + p) % hdbgSize;
        struct hevt * evtp = hevents + q;
        switch (evtp -> type)
          {
            case hevtEmpty:
              break;

            case hevtTrace:
              printTrace (evtp);
              break;
                
            case hevtMRead:
              printMRead (evtp);
              break;
                
            case hevtMWrite:
              printMWrite (evtp);
              break;
                
            case hevtFault:
              printFault (evtp);
              break;
                
            case hevtReg:
              printReg (evtp);
              break;
                
            case hevtPAReg:
              printPAReg (evtp);
              break;
                
            default:
              fprintf (hdbgOut, "hdbgPrint ? %d\n", evtp -> type);
              break;
          }
      }
    fclose (hdbgOut);
    int fd = open ("M.dump", O_WRONLY | O_CREAT, 0660);
    if (fd == -1)
      {
        sim_printf ("can't open M.dump\n");
        goto done;
      }
    /* ssize_t n = */ write (fd, M, MEMSIZE * sizeof (word36));
    close (fd);
done: ;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }

// set buffer size 
t_stat hdbg_size (UNUSED int32 arg, const char * buf)
  {
#ifdef THREADZ
    pthread_mutex_lock (& debug_lock);
#endif
    hdbgSize = strtoul (buf, NULL, 0);
    sim_printf ("hdbg size set to %ld\n", hdbgSize);
    createBuffer ();
    return SCPE_OK;
#ifdef THREADZ
    pthread_mutex_unlock (& debug_lock);
#endif
  }

t_stat hdbg_print (UNUSED int32 arg, const char * buf)
  {
    hdbgPrint ();
    return SCPE_OK;
  }
#else
#include "dps8_utils.h"
t_stat hdbg_size (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_printf ("hdbg not enabled; ignoring\n");
    return SCPE_OK;
  }
#endif // HDBG
