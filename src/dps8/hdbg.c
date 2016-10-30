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

enum hevtType { hevtEmpty = 0, hevtTrace, hevtMRead, hevtMWrite, hevtIWBUpdate, hevtRegs, hevtFault };

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
      };
  };

static struct hevt * hevents = NULL;
static unsigned long hdbgSize = 0;
static unsigned long hevtPtr = 0;

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
    if (! hevents)
      return;
#ifdef ISOLTS
if (currentRunningCPUnum == 0)
  return;
#endif
    hevents [hevtPtr] . type = hevtTrace;
    hevents [hevtPtr] . time = sim_timell ();
    hevents [hevtPtr] . trace . addrMode = get_addr_mode ();
    hevents [hevtPtr] . trace . segno = cpu . PPR.PSR;
    hevents [hevtPtr] . trace . ic = cpu . PPR.IC;
    hevents [hevtPtr] . trace . ring = cpu . PPR.PRR;
    hevents [hevtPtr] . trace . inst = cpu.cu.IWB;
    hevtPtr = (hevtPtr + 1) % hdbgSize;
  }

void hdbgMRead (word24 addr, word36 data)
  {
    if (! hevents)
      return;
#ifdef ISOLTS
if (currentRunningCPUnum == 0)
  return;
#endif
    hevents [hevtPtr] . type = hevtMRead;
    hevents [hevtPtr] . time = sim_timell ();
    hevents [hevtPtr] . memref . addr = addr;
    hevents [hevtPtr] . memref . data = data;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
  }

void hdbgMWrite (word24 addr, word36 data)
  {
    if (! hevents)
      return;
#ifdef ISOLTS
if (currentRunningCPUnum == 0)
  return;
#endif
    hevents [hevtPtr] . type = hevtMWrite;
    hevents [hevtPtr] . time = sim_timell ();
    hevents [hevtPtr] . memref . addr = addr;
    hevents [hevtPtr] . memref . data = data;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
  }

void hdbgFault (_fault faultNumber, _fault_subtype subFault,
                const char * faultMsg)

  {
    if (! hevents)
      return;
#ifdef ISOLTS
if (currentRunningCPUnum == 0)
  return;
#endif
    hevents [hevtPtr] . type = hevtFault;
    hevents [hevtPtr] . time = sim_timell ();
    hevents [hevtPtr] . fault . faultNumber = faultNumber;
    hevents [hevtPtr] . fault . subFault = subFault;
    strncpy (hevents [hevtPtr] . fault . faultMsg, faultMsg, 63);
    hevents [hevtPtr] . fault . faultMsg [63] = 0;
    hevtPtr = (hevtPtr + 1) % hdbgSize; 
  }

static FILE * hdbgOut = NULL;

static void printMRead (struct hevt * p)
  {
    fprintf (hdbgOut, "DBG(%lld)> CPU FINAL: Read %08o %012llo\n",
                p -> time, 
                p -> memref . addr, p -> memref . data);
  }

static void printMWrite (struct hevt * p)
  {
    fprintf (hdbgOut, "DBG(%lld)> CPU FINAL: Write %08o %012llo\n",
                p -> time, 
                p -> memref . addr, p -> memref . data);
  }

static void printTrace (struct hevt * p)
  {
    if (p -> trace . addrMode == ABSOLUTE_mode)
      {
        fprintf (hdbgOut, "DBG(%lld)> CPU TRACE: %06o %o %012llo (%s)\n",
                    p -> time, 
                    p -> trace . ic, p -> trace . ring,
                    p -> trace . inst, disAssemble (p -> trace . inst));
      }
    else
      {
        fprintf (hdbgOut, "DBG(%lld)> CPU TRACE: %05o:%06o %o %012llo (%s)\n",
                    p -> time, p -> trace . segno,
                    p -> trace . ic, p -> trace . ring,
                    p -> trace . inst, disAssemble (p -> trace . inst));
      }
  }

static void printFault (struct hevt * p)
  {
    fprintf (hdbgOut, "DBG(%lld)> CPU FAULT: Fault %d(0%o), sub %lld(0%llo), '%s'\n",
                p -> time, 
                p -> fault.faultNumber, p -> fault.faultNumber,
                p -> fault.subFault.bits, p -> fault.subFault.bits,
                p -> fault.faultMsg);
  }

void hdbgPrint (void)
  {
    if (! hevents)
      return;
    hdbgOut = fopen ("hdbg.list", "w");
    if (! hdbgOut)
      {
        sim_printf ("can't open hdbg.list\n");
        return;
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
        return;
      }
    /* ssize_t n = */ write (fd, M, MEMSIZE * sizeof (word36));
    close (fd);
  }

// set buffer size 
t_stat hdbg_size (UNUSED int32 arg, char * buf)
  {
    hdbgSize = strtoul (buf, NULL, 0);
    sim_printf ("hdbg size set to %ld\n", hdbgSize);
    createBuffer ();
    return SCPE_OK;
  }
#else
#include "dps8_utils.h"
t_stat hdbg_size (UNUSED int32 arg, UNUSED char * buf)
  {
    sim_printf ("hdbg not enabled; ignoring\n");
    return SCPE_OK;
  }
#endif // HDBG
