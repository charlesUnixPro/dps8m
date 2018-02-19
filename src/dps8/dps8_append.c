/*
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2017 by Charles Anthony
 Copyright 2017 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file dps8_append.c
 * \project dps8
 * \date 10/28/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_addrmods.h"
#if defined(THREADZ) || defined(LOCKLESS)
#include "threadz.h"
#endif

#define DBG_CTR cpu.cycleCnt

/**
 * The appending unit ...
 */

#ifdef TESTING
#define DBG_CTR cpu.cycleCnt
#define DBGAPP(...) sim_debug (DBG_APPENDING, & cpu_dev, __VA_ARGS__)
#else
#define DBGAPP(...)
#endif

void setAPUStatus (apuStatusBits status)
  {
#if 1
    word12 FCT = cpu.cu.APUCycleBits & MASK3;
    cpu.cu.APUCycleBits = (status & 07770) | FCT;
#else
    cpu.cu.PI_AP = 0;
    cpu.cu.DSPTW = 0;
    cpu.cu.SDWNP = 0;
    cpu.cu.SDWP  = 0;
    cpu.cu.PTW   = 0;
    cpu.cu.PTW2  = 0;
    cpu.cu.FAP   = 0;
    cpu.cu.FANP  = 0;
    cpu.cu.FABS  = 0;
    switch (status)
      {
        case apuStatus_PI_AP:
          cpu.cu.PI_AP = 1;
          break;
        case apuStatus_DSPTW:
        case apuStatus_MDSPTW: // XXX this doesn't seem like the right solution.
                               // XXX there is a MDSPTW bit in the APU history
                               // register, but not in the CU.
          cpu.cu.DSPTW = 1;
          break;
        case apuStatus_SDWNP:
          cpu.cu.SDWNP = 1;
          break;
        case apuStatus_SDWP:
          cpu.cu.SDWP  = 1;
          break;
        case apuStatus_PTW:
        case apuStatus_MPTW: // XXX this doesn't seem like the right solution.
                             // XXX there is a MPTW bit in the APU history
                             // XXX register, but not in the CU.
          cpu.cu.PTW   = 1;
          break;
        case apuStatus_PTW2:
          cpu.cu.PTW2  = 1;
          break;
        case apuStatus_FAP:
          cpu.cu.FAP   = 1;
          break;
        case apuStatus_FANP:
          cpu.cu.FANP  = 1;
          break;
        case apuStatus_FABS:
          cpu.cu.FABS  = 1;
          break;
      }
#endif
  }

#ifdef WAM
static char *strSDW (char * buf, sdw_s *SDW);
#endif

//
// 
//  The Use of Bit 29 in the Instruction Word
//  The reader is reminded that there is a preliminary step of loading TPR.CA
//  with the ADDRESS field of the instruction word during instruction decode.
//  If bit 29 of the instruction word is set to 1, modification by pointer
//  register is invoked and the preliminary step is executed as follows:
//  1. The ADDRESS field of the instruction word is interpreted as shown in
//  Figure 6-7 below.
//  2. C(PRn.SNR) -> C(TPR.TSR)
//  3. maximum of ( C(PRn.RNR), C(TPR.TRR), C(PPR.PRR) ) -> C(TPR.TRR)
//  4. C(PRn.WORDNO) + OFFSET -> C(TPR.CA) (NOTE: OFFSET is a signed binary
//  number.)
//  5. C(PRn.BITNO) -> TPR.BITNO
//


// Define this to do error detection on the PTWAM table.
// Useful if PTWAM reports an error message, but it slows the emulator
// down 50%

#ifdef do_selftestPTWAM
static void selftestPTWAM (void)
  {
    int usages [N_WAM_ENTRIES];
    for (int i = 0; i < N_WAM_ENTRIES; i ++)
      usages [i] = -1;

    for (int i = 0; i < N_WAM_ENTRIES; i ++)
      {
        ptw_s * p = cpu.PTWAM + i;
        if (p -> USE > N_WAM_ENTRIES - 1)
          sim_printf ("PTWAM[%d].USE is %d; > %d!\n",
                      i, p -> USE, N_WAM_ENTRIES - 1);
        if (usages [p -> USE] != -1)
          sim_printf ("PTWAM[%d].USE is equal to PTWAM[%d].USE; %d\n",
                      i, usages [p -> USE], p -> USE);
        usages [p -> USE] = i;
      }
    for (int i = 0; i < N_WAM_ENTRIES; i ++)
      {
        if (usages [i] == -1)
          sim_printf ("No PTWAM had a USE of %d\n", i);
      }
  }
#endif

/**
 * implement ldbr instruction
 */

void do_ldbr (word36 * Ypair)
  {
    CPTUR (cptUseDSBR);
#ifdef WAM
    if (! cpu.switches.disable_wam) 
      {
        if (cpu.cu.SD_ON) 
          {
            // If SDWAM is enabled, then
            //   0 -> C(SDWAM(i).FULL) for i = 0, 1, ..., 15
            //   i -> C(SDWAM(i).USE) for i = 0, 1, ..., 15
            for (uint i = 0; i < N_WAM_ENTRIES; i ++)
              {
                cpu.SDWAM [i].FE = 0;
#ifdef L68
                cpu.SDWAM [i].USE = (word4) i;
#endif
#ifdef DPS8M
                cpu.SDWAM [i].USE = 0;
#endif
              }
          }

        if (cpu.cu.PT_ON) 
          {
            // If PTWAM is enabled, then
            //   0 -> C(PTWAM(i).FULL) for i = 0, 1, ..., 15
            //   i -> C(PTWAM(i).USE) for i = 0, 1, ..., 15
            for (uint i = 0; i < N_WAM_ENTRIES; i ++)
              {
                cpu.PTWAM [i].FE = 0;
#ifdef L68
                cpu.PTWAM [i].USE = (word4) i;
#endif
#ifdef DPS8M
                cpu.PTWAM [i].USE = 0;
#endif
              }
#ifdef do_selftestPTWAM
            selftestPTWAM ();
#endif
          }
      }
#else
    cpu.SDW0.FE = 0;
    cpu.SDW0.USE = 0;
    cpu.PTW0.FE = 0;
    cpu.PTW0.USE = 0;
#endif // WAM

    // If cache is enabled, reset all cache column and level full flags
    // XXX no cache

    // C(Y-pair) 0,23 -> C(DSBR.ADDR)
    cpu.DSBR.ADDR = (Ypair [0] >> (35 - 23)) & PAMASK;

    // C(Y-pair) 37,50 -> C(DSBR.BOUND)
    cpu.DSBR.BND = (Ypair [1] >> (71 - 50)) & 037777;

    // C(Y-pair) 55 -> C(DSBR.U)
    cpu.DSBR.U = (Ypair [1] >> (71 - 55)) & 01;

    // C(Y-pair) 60,71 -> C(DSBR.STACK)
    cpu.DSBR.STACK = (Ypair [1] >> (71 - 71)) & 07777;
    DBGAPP ("ldbr 0 -> SDWAM/PTWAM[*].F, i -> SDWAM/PTWAM[i].USE, "
            "DSBR.ADDR 0%o, DSBR.BND 0%o, DSBR.U 0%o, DSBR.STACK 0%o\n",
            cpu.DSBR.ADDR, cpu.DSBR.BND, cpu.DSBR.U, cpu.DSBR.STACK); 
  }

/**
 * implement sdbr instruction
 */

void do_sdbr (word36 * Ypair)
  {
    CPTUR (cptUseDSBR);
    // C(DSBR.ADDR) -> C(Y-pair) 0,23
    // 00...0 -> C(Y-pair) 24,36
    Ypair [0] = ((word36) (cpu.DSBR.ADDR & PAMASK)) << (35 - 23); 

    // C(DSBR.BOUND) -> C(Y-pair) 37,50
    // 0000 -> C(Y-pair) 51,54
    // C(DSBR.U) -> C(Y-pair) 55
    // 000 -> C(Y-pair) 56,59
    // C(DSBR.STACK) -> C(Y-pair) 60,71
    Ypair [1] = ((word36) (cpu.DSBR.BND & 037777)) << (71 - 50) |
                ((word36) (cpu.DSBR.U & 1)) << (71 - 55) |
                ((word36) (cpu.DSBR.STACK & 07777)) << (71 - 71);
  }

/**
 * implement camp instruction
 */

void do_camp (UNUSED word36 Y)
  {
    // C(TPR.CA) 16,17 control disabling or enabling the associative memory.
    // This may be done to either or both halves.
    // The full/empty bit of cache PTWAM register is set to zero and the LRU
    // counters are initialized.
#ifdef WAM
    if (! cpu.switches.disable_wam)
      { // disabled by simh, do nothing
#ifdef DPS8M
        if (cpu.cu.PT_ON) // only clear when enabled
#endif
            for (uint i = 0; i < N_WAM_ENTRIES; i ++)
              {
                cpu.PTWAM[i].FE = 0;
#ifdef L68
                cpu.PTWAM[i].USE = (word4) i;
#endif
#ifdef DPS8M
                cpu.PTWAM[i].USE = 0;
#endif
              }

// 58009997-040 A level of the associative memory is disabled if
// C(TPR.CA) 16,17 = 01
// 58009997-040 A level of the associative memory is enabled if
// C(TPR.CA) 16,17 = 10
// Level j is selected to be enabled/disable if
// C(TPR.CA) 10+j = 1; j=1,2,3,4
// All levels are selected to be enabled/disabled if
// C(TPR.CA) 11,14 = 0
// This is contrary to what AL39 says, so I'm not going to implement it. In
// fact, I'm not even going to implement the halves.

#ifdef DPS8M
        if (cpu.TPR.CA != 0000002 && (cpu.TPR.CA & 3) != 0)
          sim_warn ("CAMP ignores enable/disable %06o\n", cpu.TPR.CA);
#endif
        if ((cpu.TPR.CA & 3) == 02)
          cpu.cu.PT_ON = 1;
        else if ((cpu.TPR.CA & 3) == 01)
          cpu.cu.PT_ON = 0;
      }
    else
      {
        cpu.PTW0.FE = 0;
        cpu.PTW0.USE = 0;
      }
#else
    cpu.PTW0.FE = 0;
    cpu.PTW0.USE = 0;
#endif
  }

/**
 * implement cams instruction
 */

void do_cams (UNUSED word36 Y)
  {
    // The full/empty bit of each SDWAM register is set to zero and the LRU
    // counters are initialized. The remainder of the contents of the registers
    // are unchanged. If the associative memory is disabled, F and LRU are
    // unchanged.
    // C(TPR.CA) 16,17 control disabling or enabling the associative memory.
    // This may be done to either or both halves.
#ifdef WAM
    if (!cpu.switches.disable_wam)
      { // disabled by simh, do nothing
#ifdef DPS8M
        if (cpu.cu.SD_ON) // only clear when enabled
#endif
            for (uint i = 0; i < N_WAM_ENTRIES; i ++)
              {
                cpu.SDWAM[i].FE = 0;
#ifdef L68
                cpu.SDWAM[i].USE = (word4) i;
#endif
#ifdef DPS8M
                cpu.SDWAM[i].USE = 0;
#endif
              }
#ifdef ISOLTS
IF1 sim_printf ("CAMS cleared it\n");
#endif

// 58009997-040 A level of the associative memory is disabled if
// C(TPR.CA) 16,17 = 01
// 58009997-040 A level of the associative memory is enabled if
// C(TPR.CA) 16,17 = 10
// Level j is selected to be enabled/disable if
// C(TPR.CA) 10+j = 1; j=1,2,3,4
// All levels are selected to be enabled/disabled if
// C(TPR.CA) 11,14 = 0
// This is contrary to what AL39 says, so I'm not going to implement it. In
// fact, I'm not even going to implement the halves.

#ifdef DPS8M
        if (cpu.TPR.CA != 0000006 && (cpu.TPR.CA & 3) != 0)
          sim_warn ("CAMS ignores enable/disable %06o\n", cpu.TPR.CA);
#endif
        if ((cpu.TPR.CA & 3) == 02)
          cpu.cu.SD_ON = 1;
        else if ((cpu.TPR.CA & 3) == 01)
          cpu.cu.SD_ON = 0;
      }
    else
      {
        cpu.SDW0.FE = 0;
        cpu.SDW0.USE = 0;
      }
#else
    cpu.SDW0.FE = 0;
    cpu.SDW0.USE = 0;
#endif
  }

    
/**
 * fetch descriptor segment PTW ...
 */

// CANFAULT

static void fetchDSPTW (word15 segno)
  {
    DBGAPP ("fetchDSPTW segno 0%o\n", segno);
    PNL (L68_ (cpu.apu.state |= apu_FDPT;))

    if (2 * segno >= 16 * (cpu.DSBR.BND + 1))
      {
        DBGAPP ("fetchDSPTW ACV15\n");
        // generate access violation, out of segment bounds fault
        PNL (cpu.acvFaults |= ACV15;)
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        doFault (FAULT_ACV, fst_acv15,
                 "acvFault: fetchDSPTW out of segment bounds fault");
      }
    setAPUStatus (apuStatus_DSPTW);

#ifndef SPEED
    word24 y1 = (2u * segno) % 1024u;
#endif
    word24 x1 = (2u * segno) / 1024u; // floor

    PNL (cpu.lastPTWOffset = segno;)
    PNL (cpu.lastPTWIsDS = true;)

    word36 PTWx1;
    core_read ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);
    
    cpu.PTW0.ADDR = GETHI (PTWx1);
    cpu.PTW0.U = TSTBIT (PTWx1, 9);
    cpu.PTW0.M = TSTBIT (PTWx1, 6);
    cpu.PTW0.DF = TSTBIT (PTWx1, 2);
    cpu.PTW0.FC = PTWx1 & 3;
    
#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (APUH_FDSPTW);
#endif

    DBGAPP ("fetchDSPTW x1 0%o y1 0%o DSBR.ADDR 0%o PTWx1 0%012"PRIo64" "
            "PTW0: ADDR 0%o U %o M %o F %o FC %o\n",
            x1, y1, cpu.DSBR.ADDR, PTWx1, cpu.PTW0.ADDR, cpu.PTW0.U,
            cpu.PTW0.M, cpu.PTW0.DF, cpu.PTW0.FC);
  }


/**
 * modify descriptor segment PTW (Set U=1) ...
 */

// CANFAULT

static void modifyDSPTW (word15 segno)
  {

    PNL (L68_ (cpu.apu.state |= apu_MDPT;))

    setAPUStatus (apuStatus_MDSPTW); 

    word24 x1 = (2u * segno) / 1024u; // floor
    
#ifdef TEST_OLIN
          cmpxchg ();
#endif
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
    bool lck = get_rmw_lock ();
    if (! lck)
      lock_rmw ();
#endif

    word36 PTWx1;
#ifdef LOCKLESS
    core_read_lock ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);
    PTWx1 = SETBIT (PTWx1, 9);
    core_write_unlock ((cpu.DSBR.ADDR + x1) & PAMASK, PTWx1, __func__);
#else
    core_read ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);
    PTWx1 = SETBIT (PTWx1, 9);
    core_write ((cpu.DSBR.ADDR + x1) & PAMASK, PTWx1, __func__);
#endif
    
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
    if (! lck)
      unlock_rmw ();
#endif

    cpu.PTW0.U = 1;
#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (APUH_MDSPTW);
#endif
  }


#ifdef WAM
#ifdef DPS8M
static word6 calcHitAM (word6 LRU, uint hitLevel)
  {
    switch (hitLevel)
      {
        case 0:  // hit level A
          return (LRU | 070);
        case 1:  // hit level B
          return ((LRU & 037) | 06);
        case 2:  // hit level C
          return ((LRU & 053) | 01);
        case 3:  // hit level D
          return (LRU & 064);
        default:
          DBGAPP ("calcHitAM: invalid AM level\n");
          return 0;
     }
  }
#endif

static sdw_s * fetchSDWfromSDWAM (word15 segno)
  {
    DBGAPP ("fetchSDWfromSDWAM(0):segno=%05o\n", segno);
    
    if (cpu.switches.disable_wam || ! cpu.cu.SD_ON)
      {
        DBGAPP ("fetchSDWfromSDWAM(0): SDWAM disabled\n");
        return NULL;
      }

#ifdef L68
    int nwam = N_WAM_ENTRIES;
    for (int _n = 0; _n < nwam; _n++)
      {
        // make certain we initialize SDWAM prior to use!!!
        if (cpu.SDWAM[_n].FE && segno == cpu.SDWAM[_n].POINTER)
          {
            DBGAPP ("fetchSDWfromSDWAM(1):found match for segno %05o "
                    "at _n=%d\n",
                     segno, _n);
            
            cpu.cu.SDWAMM = 1;
            cpu.SDWAMR = (word4) _n;
            cpu.SDW = & cpu.SDWAM[_n];
            
            // If the SDWAM match logic circuitry indicates a hit, all usage
            // counts (SDWAM.USE) greater than the usage count of the register
            // hit are decremented by one, the usage count of the register hit
            // is set to 15, and the contents of the register hit are read out
            // into the address preparation circuitry. 

            for (int _h = 0; _h < nwam; _h++)
              {
                if (cpu.SDWAM[_h].USE > cpu.SDW->USE)
                  cpu.SDWAM[_h].USE -= 1;
              }
            cpu.SDW->USE = N_WAM_ENTRIES - 1;
 
            char buf [256];
            DBGAPP ("fetchSDWfromSDWAM(2):SDWAM[%d]=%s\n",
                     _n, strSDW (buf, cpu.SDW));

            return cpu.SDW;
          }
      }
#endif
#ifdef DPS8M
    uint setno = segno & 017;
    uint toffset;
    sdw_s *p;
    for (toffset = 0; toffset < 64; toffset += 16)
      {
        p = & cpu.SDWAM[toffset + setno];
        if (p->FE && segno == p->POINTER)
          {
            DBGAPP ("fetchSDWfromSDWAM(1):found match for segno %05o "
                    "at _n=%d\n",
                    segno, toffset + setno);
            
            cpu.cu.SDWAMM = 1;
            cpu.SDWAMR = (word6) (toffset + setno);
            cpu.SDW = p; // export pointer for appending

            word6 u = calcHitAM (p->USE, toffset >> 4);
            for (toffset = 0; toffset < 64; toffset += 16) // update LRU
              {
                p = & cpu.SDWAM[toffset + setno];
                if (p->FE) 
                  p->USE = u;
              }

            char buf [256];
            DBGAPP ("fetchSDWfromSDWAM(2):SDWAM[%d]=%s\n",
                    toffset + setno, strSDW (buf, cpu.SDW));
            return cpu.SDW;
          }
      }
#endif
    DBGAPP ("fetchSDWfromSDWAM(3):SDW for segment %05o not found in SDWAM\n",
            segno);
    cpu.cu.SDWAMM = 0;
    return NULL;    // segment not referenced in SDWAM
  }
#endif // WAM

/**
 * Fetches an SDW from a paged descriptor segment.
 */
// CANFAULT

static void fetchPSDW (word15 segno)
  {
    DBGAPP ("fetchPSDW(0):segno=%05o\n",
            segno);

    PNL (L68_ (cpu.apu.state |= apu_FSDP;))

    setAPUStatus (apuStatus_SDWP);
    word24 y1 = (2 * segno) % 1024;
    
    word36 SDWeven, SDWodd;
    
    core_read2 (((((word24) cpu.PTW0.ADDR & 0777760) << 6) + y1) & PAMASK, 
                & SDWeven, & SDWodd, __func__);
    
    // even word
    cpu.SDW0.ADDR = (SDWeven >> 12) & 077777777;
    cpu.SDW0.R1 = (SDWeven >> 9) & 7;
    cpu.SDW0.R2 = (SDWeven >> 6) & 7;
    cpu.SDW0.R3 = (SDWeven >> 3) & 7;
    cpu.SDW0.DF = TSTBIT (SDWeven, 2);
    cpu.SDW0.FC = SDWeven & 3;
    
    // odd word
    cpu.SDW0.BOUND = (SDWodd >> 21) & 037777;
    cpu.SDW0.R = TSTBIT (SDWodd, 20);
    cpu.SDW0.E = TSTBIT (SDWodd, 19);
    cpu.SDW0.W = TSTBIT (SDWodd, 18);
    cpu.SDW0.P = TSTBIT (SDWodd, 17);
    cpu.SDW0.U = TSTBIT (SDWodd, 16);
    cpu.SDW0.G = TSTBIT (SDWodd, 15);
    cpu.SDW0.C = TSTBIT (SDWodd, 14);
    cpu.SDW0.EB = SDWodd & 037777;
    
#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (APUH_FSDWP);
#endif
    DBGAPP ("fetchPSDW y1 0%o p->ADDR 0%o SDW 0%012"PRIo64" 0%012"PRIo64" "
            "ADDR %o R %o%o%o BOUND 0%o REWPUGC %o%o%o%o%o%o%o "
            "F %o FC %o FE %o USE %o\n",
            y1, cpu.PTW0.ADDR, SDWeven, SDWodd, cpu.SDW0.ADDR,
            cpu.SDW0.R1, cpu.SDW0.R2, cpu.SDW0.R3, cpu.SDW0.BOUND,
            cpu.SDW0.R, cpu.SDW0.E, cpu.SDW0.W, cpu.SDW0.P, cpu.SDW0.U,
            cpu.SDW0.G, cpu.SDW0.C, cpu.SDW0.DF, cpu.SDW0.FC, cpu.SDW0.FE,
            cpu.SDW0.USE);
  }

// Nonpaged SDW Fetch
// Fetches an SDW from an unpaged descriptor segment.
// CANFAULT

static void fetchNSDW (word15 segno)
  {
    DBGAPP ("fetchNSDW (0):segno=%05o\n", segno);

    PNL (L68_ (cpu.apu.state |= apu_FSDN;))

    setAPUStatus (apuStatus_SDWNP);

    if (2 * segno >= 16 * (cpu.DSBR.BND + 1))
      {
        DBGAPP ("fetchNSDW (1):Access Violation, out of segment bounds for "
                "segno=%05o DSBR.BND=%d\n",
                segno, cpu.DSBR.BND);
        // generate access violation, out of segment bounds fault
        PNL (cpu.acvFaults |= ACV15;)
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        doFault (FAULT_ACV, fst_acv15,
                 "acvFault fetchNSDW: out of segment bounds fault");
      }
    DBGAPP ("fetchNSDW (2):fetching SDW from %05o\n",
            cpu.DSBR.ADDR + 2u * segno);

    word36 SDWeven, SDWodd;
    core_read2 ((cpu.DSBR.ADDR + 2u * segno) & PAMASK,
                & SDWeven, & SDWodd, __func__);
    
    // even word
    cpu.SDW0.ADDR = (SDWeven >> 12) & 077777777;
    cpu.SDW0.R1 = (SDWeven >> 9) & 7;
    cpu.SDW0.R2 = (SDWeven >> 6) & 7;
    cpu.SDW0.R3 = (SDWeven >> 3) & 7;
    cpu.SDW0.DF = TSTBIT (SDWeven, 2);
    cpu.SDW0.FC = SDWeven & 3;
    
    // odd word
    cpu.SDW0.BOUND = (SDWodd >> 21) & 037777;
    cpu.SDW0.R = TSTBIT (SDWodd, 20);
    cpu.SDW0.E = TSTBIT (SDWodd, 19);
    cpu.SDW0.W = TSTBIT (SDWodd, 18);
    cpu.SDW0.P = TSTBIT (SDWodd, 17);
    cpu.SDW0.U = TSTBIT (SDWodd, 16);
    cpu.SDW0.G = TSTBIT (SDWodd, 15);
    cpu.SDW0.C = TSTBIT (SDWodd, 14);
    cpu.SDW0.EB = SDWodd & 037777;
    
#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (0 /* No fetch no paged bit */);
#endif
#ifndef SPEED
    char buf [256];
    DBGAPP ("fetchNSDW (2):SDW0=%s\n", str_SDW0 (buf, & cpu.SDW0));
#endif
  }

#ifdef WAM
static char *strSDW (char * buf, sdw_s *SDW)
  {
    if (! SDW->FE)
      sprintf (buf, "*** SDW Uninitialized ***");
    else
      sprintf (buf,
               "ADDR:%06o R1:%o R2:%o R3:%o BOUND:%o R:%o E:%o W:%o P:%o "
               "U:%o G:%o C:%o CL:%o DF:%o FC:%o POINTER=%o USE=%d",
               SDW->ADDR,
               SDW->R1,
               SDW->R2,
               SDW->R3,
               SDW->BOUND,
               SDW->R,
               SDW->E,
               SDW->W,
               SDW->P,
               SDW->U,
               SDW->G,
               SDW->C,
               SDW->EB,
               SDW->DF,
               SDW->FC,
               SDW->POINTER,
               SDW->USE);
    return buf;
  }
#endif

#ifdef WAM
#ifdef L68

/**
 * dump SDWAM...
 */

t_stat dumpSDWAM (void)
  {
    char buf [256];
    for (int _n = 0; _n < N_WAM_ENTRIES; _n++)
      {
        sdw_s *p = & cpu.SDWAM[_n];
        
        if (p->FE)
          sim_printf ("SDWAM n:%d %s\n", _n, strSDW (buf, p));
      }
    return SCPE_OK;
  }
#endif

#ifdef DPS8M
static uint toBeDiscardedAM (word6 LRU)
  {
#if 0
    uint cA=0,cB=0,cC=0,cD=0;
    if (LRU & 040) cB++; else cA++;
    if (LRU & 020) cC++; else cA++;
    if (LRU & 010) cD++; else cA++;
    if (cA==3) return 0;
    if (LRU & 04)  cC++; else cB++;
    if (LRU & 02)  cD++; else cB++;
    if (cB==3) return 1;
    if (LRU & 01)  return 3; else return 2;
#endif

    if ((LRU & 070) == 070) return 0;
    if ((LRU & 046) == 006) return 1;
    if ((LRU & 025) == 001) return 2;
    return 3;
  }
#endif
#endif

/**
 * load the current in-core SDW0 into the SDWAM ...
 */

static void loadSDWAM (word15 segno, UNUSED bool nomatch)
  {
#ifndef WAM
    cpu.SDW0.POINTER = segno;
    cpu.SDW0.USE = 0;
            
    cpu.SDW0.FE = true;     // in use by SDWAM
            
    cpu.SDW = & cpu.SDW0;
            
#else
    if (nomatch || cpu.switches.disable_wam || ! cpu.cu.SD_ON)
      {
        DBGAPP ("loadSDWAM: SDWAM disabled\n");
        sdw_s * p = & cpu.SDW0;
        p->POINTER = segno;
        p->USE = 0;
        p->FE = true;     // in use by SDWAM
        cpu.SDW = p;
        return;
      }

#ifdef L68    
    // If the SDWAM match logic does not indicate a hit, the SDW is fetched
    // from the descriptor segment in main memory and loaded into the SDWAM
    // register with usage count 0 (the oldest), all usage counts are
    // decremented by one with the newly loaded register rolling over from 0 to
    // 15, and the newly loaded register is read out into the address
    // preparation circuitry.

    for (int _n = 0; _n < N_WAM_ENTRIES; _n++)
      {
        sdw_s * p = & cpu.SDWAM[_n];
        if (! p->FE || p->USE == 0)
          {
            DBGAPP ("loadSDWAM(1):SDWAM[%d] FE=0 || USE=0\n", _n);
            
            * p = cpu.SDW0;
            p->POINTER = segno;
            p->USE = 0;
            p->FE = true;     // in use by SDWAM
            
            for (int _h = 0; _h < N_WAM_ENTRIES; _h++)
              {
                sdw_s * q = & cpu.SDWAM[_h];
                q->USE -= 1;
                q->USE &= N_WAM_MASK;
              }
            
            cpu.SDW = p;
            
            char buf [256];
            DBGAPP ("loadSDWAM(2):SDWAM[%d]=%s\n",
                    _n, strSDW (buf, p));
            
            return;
          }
      }
    // if we reach this, USE is scrambled
    DBGAPP ("loadSDWAM(3) no USE=0 found for segment=%d\n", segno);
    
    sim_printf ("loadSDWAM(%05o): no USE=0 found!\n", segno);
    dumpSDWAM ();
#endif

#ifdef DPS8M
    uint setno = segno & 017;
    uint toffset;
    sdw_s *p;
    for (toffset = 0; toffset < 64; toffset += 16)
      {
        p = & cpu.SDWAM[toffset + setno];
        if (!p->FE)
          break;
      }
    if (toffset == 64) // all FE==1
      {
        toffset = toBeDiscardedAM (p->USE) << 4;
        p = & cpu.SDWAM[toffset + setno];
      }
    DBGAPP ("loadSDWAM(1):SDWAM[%d] FE=0 || LRU\n",
            toffset + setno);
            
    word6 u = calcHitAM (p->USE, toffset >> 4); // before loading the SDWAM!
    * p = cpu.SDW0; // load the SDW
    p->POINTER = segno;
    p->FE = true;  // in use
    cpu.SDW = p; // export pointer for appending

    for (uint toffset1 = 0; toffset1 < 64; toffset1 += 16) // update LRU
      {
        p = & cpu.SDWAM[toffset1 + setno];
        if (p->FE) 
          p->USE = u;
      }
            
    char buf [256];
    DBGAPP ("loadSDWAM(2):SDWAM[%d]=%s\n",
            toffset + setno, strSDW (buf, cpu.SDW));
#endif
#endif // WAM
  }

#ifdef WAM
static ptw_s * fetchPTWfromPTWAM (word15 segno, word18 CA)
  {
    if (cpu.switches.disable_wam || ! cpu.cu.PT_ON)
      {
        DBGAPP ("fetchPTWfromPTWAM: PTWAM disabled\n");
        return NULL;
      }
    
#ifdef L68
    int nwam = N_WAM_ENTRIES;
    for (int _n = 0; _n < nwam; _n++)
      {
        if (cpu.PTWAM[_n].FE && ((CA >> 6) & 07760) == cpu.PTWAM[_n].PAGENO &&
            cpu.PTWAM[_n].POINTER == segno)   //_initialized)
          {
            DBGAPP ("fetchPTWfromPTWAM: found match for segno=%o pageno=%o "
                    "at _n=%d\n",
                    segno, cpu.PTWAM[_n].PAGENO, _n);
            cpu.cu.PTWAMM = 1;
            cpu.PTWAMR = (word4) _n;
            cpu.PTW = & cpu.PTWAM[_n];
            
            // If the PTWAM match logic circuitry indicates a hit, all usage
            // counts (PTWAM.USE) greater than the usage count of the register
            // hit are decremented by one, the usage count of the register hit
            // is set to 15, and the contents of the register hit are read out
            // into the address preparation circuitry.

            for (int _h = 0; _h < nwam; _h++)
              {
                if (cpu.PTWAM[_h].USE > cpu.PTW->USE)
                  cpu.PTWAM[_h].USE -= 1; //PTW->USE -= 1;
              }
            cpu.PTW->USE = N_WAM_ENTRIES - 1;
#ifdef do_selftestPTWAM
            selftestPTWAM ();
#endif
            DBGAPP ("fetchPTWfromPTWAM: ADDR 0%o U %o M %o F %o FC %o\n",
                    cpu.PTW->ADDR, cpu.PTW->U, cpu.PTW->M, cpu.PTW->DF,
                    cpu.PTW->FC);
            return cpu.PTW;
          }
      }
#endif

#ifdef DPS8M
    uint setno = (CA >> 10) & 017;
    uint toffset;
    ptw_s *p;
    for (toffset = 0; toffset < 64; toffset += 16)
      {
        p = & cpu.PTWAM[toffset + setno];

        if (p->FE && ((CA >> 6) & 07760) == p->PAGENO && p->POINTER == segno)
          {
            DBGAPP ("fetchPTWfromPTWAM: found match for segno=%o pageno=%o "
                    "at _n=%d\n",
                    segno, p->PAGENO, toffset + setno);
            cpu.cu.PTWAMM = 1;
            cpu.PTWAMR = (word6) (toffset + setno);
            cpu.PTW = p; // export pointer for appending
            
            word6 u = calcHitAM (p->USE, toffset >> 4);
            for (toffset = 0; toffset < 64; toffset += 16) // update LRU
              {
                p = & cpu.PTWAM[toffset + setno];
                if (p->FE) 
                  p->USE = u;
              }

            DBGAPP ("fetchPTWfromPTWAM: ADDR 0%o U %o M %o F %o FC %o\n",
                    cpu.PTW->ADDR, cpu.PTW->U, cpu.PTW->M, cpu.PTW->DF,
                    cpu.PTW->FC);
            return cpu.PTW;
          }
      }
#endif
    cpu.cu.PTWAMM = 0;
    return NULL;    // page not referenced in PTWAM
  }
#endif // WAM

static void fetchPTW (sdw_s *sdw, word18 offset)
  {
    // AL39 p.5-7
    // Fetches a PTW from a page table other than a descriptor segment page
    // table and sets the page accessed bit (PTW.U)
    PNL (L68_ (cpu.apu.state |= apu_FPTW;))
    setAPUStatus (apuStatus_PTW);

#ifndef SPEED
    word24 y2 = offset % 1024;
#endif
    word24 x2 = (offset) / 1024; // floor
    
    word36 PTWx2;
    
    DBGAPP ("fetchPTW address %08o\n",
            sdw->ADDR + x2);

    PNL (cpu.lastPTWOffset = offset;)
    PNL (cpu.lastPTWIsDS = false;)

#ifdef TEST_OLIN
          cmpxchg ();
#endif
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
    bool lck = get_rmw_lock ();
    if (! lck)
      lock_rmw ();
#endif
#ifdef LOCKLESS
    core_read_lock ((sdw->ADDR + x2) & PAMASK, & PTWx2, __func__);
#else
    core_read ((sdw->ADDR + x2) & PAMASK, & PTWx2, __func__);
#endif
    
    cpu.PTW0.ADDR = GETHI (PTWx2);
    cpu.PTW0.U = TSTBIT (PTWx2, 9);
    cpu.PTW0.M = TSTBIT (PTWx2, 6);
    cpu.PTW0.DF = TSTBIT (PTWx2, 2);
    cpu.PTW0.FC = PTWx2 & 3;

    // ISOLTS-861 02
#ifndef LOCKLESS
    if (! cpu.PTW0.U)
#endif
      {
        PTWx2 = SETBIT (PTWx2, 9);
#ifdef LOCKLESS
	core_write_unlock ((sdw->ADDR + x2) & PAMASK, PTWx2, __func__);
#else
        core_write ((sdw->ADDR + x2) & PAMASK, PTWx2, __func__);
#endif
        cpu.PTW0.U = 1;
      }
    
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
    if (! lck)
      unlock_rmw ();
#endif

#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (APUH_FPTW);
#endif

    DBGAPP ("fetchPTW x2 0%o y2 0%o sdw->ADDR 0%o PTWx2 0%012"PRIo64" "
            "PTW0: ADDR 0%o U %o M %o F %o FC %o\n",
            x2, y2, sdw->ADDR, PTWx2, cpu.PTW0.ADDR, cpu.PTW0.U,
            cpu.PTW0.M, cpu.PTW0.DF, cpu.PTW0.FC);
  }

static void loadPTWAM (word15 segno, word18 offset, UNUSED bool nomatch)
  {
#ifndef WAM
    cpu.PTW0.PAGENO = (offset >> 6) & 07760;
    cpu.PTW0.POINTER = segno;
    cpu.PTW0.USE = 0;
    cpu.PTW0.FE = true;
            
    cpu.PTW = & cpu.PTW0;
#else
    if (nomatch || cpu.switches.disable_wam || ! cpu.cu.PT_ON)
      {
        DBGAPP ("loadPTWAM: PTWAM disabled\n");
        ptw_s * p = & cpu.PTW0;
        p->PAGENO = (offset >> 6) & 07760;  // ISOLTS-861 02, AL39 p.3-22
        p->POINTER = segno;
        p->USE = 0;
        p->FE = true;
            
        cpu.PTW = p;
        return;
      }

#ifdef L68    
    // If the PTWAM match logic does not indicate a hit, the PTW is fetched
    // from main memory and loaded into the PTWAM register with usage count 0
    // (the oldest), all usage counts are decremented by one with the newly
    // loaded register rolling over from 0 to 15, and the newly loaded register
    // is read out into the address preparation circuitry.

    for (int _n = 0; _n < N_WAM_ENTRIES; _n++)
      {
        ptw_s * p = & cpu.PTWAM[_n];
        if (! p->FE || p->USE == 0)
          {
            DBGAPP ("loadPTWAM(1):PTWAM[%d] FE=0 || USE=0\n", _n);
            *p = cpu.PTW0;
            p->PAGENO = (offset >> 6) & 07760;
            p->POINTER = segno;
            p->USE = 0;
            p->FE = true;
            
            for (int _h = 0; _h < N_WAM_ENTRIES; _h++)
              {
                ptw_s * q = & cpu.PTWAM[_h];
                q->USE -= 1;
                q->USE &= N_WAM_MASK;
              }
            
            cpu.PTW = p;
            DBGAPP ("loadPTWAM(2): ADDR 0%o U %o M %o F %o FC %o "
                    "POINTER=%o PAGENO=%o USE=%d\n",
                    cpu.PTW->ADDR, cpu.PTW->U, cpu.PTW->M, cpu.PTW->DF,
                    cpu.PTW->FC, cpu.PTW->POINTER, cpu.PTW->PAGENO,
                    cpu.PTW->USE);
#ifdef do_selftestPTWAM
            selftestPTWAM ();
#endif
            return;
          }
      }
    // if we reach this, USE is scrambled
    sim_printf ("loadPTWAM(segno=%05o, offset=%012o): no USE=0 found!\n",
                segno, offset);
#endif

#ifdef DPS8M
    uint setno = (offset >> 10) & 017;
    uint toffset;
    ptw_s *p;
    for (toffset = 0; toffset < 64; toffset += 16)
      {
        p = & cpu.PTWAM [toffset + setno];
        if (! p->FE)
          break;
      }
    if (toffset == 64) // all FE==1
      {
        toffset = toBeDiscardedAM (p->USE) << 4;
        p = & cpu.PTWAM [toffset + setno];
      }

    DBGAPP ("loadPTWAM(1):PTWAM[%d] FE=0 || LRU\n",
            toffset + setno);

    word6 u = calcHitAM (p->USE, toffset >> 4); // before loading the PTWAM
    * p = cpu.PTW0; // load the PTW
    p->PAGENO = (offset >> 6) & 07760;
    p->POINTER = segno;
    p->FE = true;  // in use
    cpu.PTW = p; // export pointer for appending

    for (uint toffset1 = 0; toffset1 < 64; toffset1 += 16) // update LRU
      {
        p = & cpu.PTWAM[toffset1 + setno];
        if (p->FE) 
          p->USE = u;
      }

    DBGAPP ("loadPTWAM(2): ADDR 0%o U %o M %o F %o FC %o POINTER=%o "
            "PAGENO=%o USE=%d\n",
            cpu.PTW->ADDR, cpu.PTW->U, cpu.PTW->M, cpu.PTW->DF, 
            cpu.PTW->FC, cpu.PTW->POINTER, cpu.PTW->PAGENO, cpu.PTW->USE);
#endif
#endif // WAM
  }

/**
 * modify target segment PTW (Set M=1) ...
 */

static void modifyPTW (sdw_s *sdw, word18 offset)
  {
    PNL (L68_ (cpu.apu.state |= apu_MPTW;))
    //word24 y2 = offset % 1024;
    word24 x2 = offset / 1024; // floor
    
    word36 PTWx2;
    
    setAPUStatus (apuStatus_MPTW);

#ifdef TEST_OLIN
          cmpxchg ();
#endif
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
    bool lck = get_rmw_lock ();
    if (! lck)
      lock_rmw ();
#endif
#ifdef LOCKLESS
    core_read_lock ((sdw->ADDR + x2) & PAMASK, & PTWx2, __func__);
    PTWx2 = SETBIT (PTWx2, 6);
    core_write_unlock ((sdw->ADDR + x2) & PAMASK, PTWx2, __func__);
#else    
    core_read ((sdw->ADDR + x2) & PAMASK, & PTWx2, __func__);
    PTWx2 = SETBIT (PTWx2, 6);
    core_write ((sdw->ADDR + x2) & PAMASK, PTWx2, __func__);
#endif
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
    if (! lck)
      unlock_rmw ();
#endif
    cpu.PTW->M = 1;
#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (APUH_MPTW);
#endif
  }

static void doPTW2 (sdw_s *sdw, word18 offset)
  {
    PNL (L68_ (cpu.apu.state |= apu_FPTW2;))
    setAPUStatus (apuStatus_PTW2);

#ifndef SPEED
    word24 y2 = offset % 1024;
#endif
    word24 x2 = (offset) / 1024; // floor
    
    word36 PTWx2n;
    
    DBGAPP ("doPTW2 address %08o\n",
            sdw->ADDR + x2 + 1);

    core_read ((sdw->ADDR + x2 + 1) & PAMASK, & PTWx2n, __func__);

    ptw_s PTW2; 
    PTW2.ADDR = GETHI (PTWx2n);
    PTW2.U = TSTBIT (PTWx2n, 9);
    PTW2.M = TSTBIT (PTWx2n, 6);
    PTW2.DF = TSTBIT (PTWx2n, 2);
    PTW2.FC = PTWx2n & 3;
    
#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (APUH_FPTW2);
#endif

    DBGAPP ("doPTW2 x2 0%o y2 0%o sdw->ADDR 0%o PTW2 0%012"PRIo64" "
            "PTW2: ADDR 0%o U %o M %o F %o FC %o\n",
            x2, y2, sdw->ADDR, PTWx2n, PTW2.ADDR, PTW2.U, PTW2.M,
            PTW2.DF, PTW2.FC);

    // check that PTW2 is the next page of the same segment
    // ISOLTS 875 02a
    if ((PTW2.ADDR & 0777760) == (cpu.PTW->ADDR & 0777760) + 16)
       //Is PTW2.F set ON?
       if (! PTW2.DF)
           // initiate a directed fault
           doFault (FAULT_DF0 + PTW2.FC, fst_zero, "PTW2.F == 0");

  }


/**
 * Is the instruction a SToRage OPeration ?
 */
#ifndef QUIET_UNUSED
static bool isAPUDataMovement (word36 inst)
  {
    // XXX: implement - when we figure out what it is
    return false;
  }
#endif

#ifndef QUIET_UNUSED
static char *strAccessType (MemoryAccessType accessType)
  {
    switch (accessType)
      {
        case UnknownMAT:        return "Unknown";
        case OperandRead:       return "OperandRead";
        case OperandWrite:      return "OperandWrite";
        default:                return "???";
      }
  }
#endif

#ifndef QUIET_UNUSED
static char *strACV (_fault_subtype acv)
  {
    switch (acv)
      {
        case ACV0:  return "Illegal ring order (ACV0=IRO)";
        case ACV1:  return "Not in execute bracket (ACV1=OEB)";
        case ACV2:  return "No execute permission (ACV2=E-OFF)";
        case ACV3:  return "Not in read bracket (ACV3=ORB)";
        case ACV4:  return "No read permission (ACV4=R-OFF)";
        case ACV5:  return "Not in write bracket (ACV5=OWB)";
        case ACV6:  return "No write permission (ACV6=W-OFF)";
        case ACV7:  return "Call limiter fault (ACV7=NO GA)";
        case ACV8:  return "Out of call brackets (ACV8=OCB)";
        case ACV9:  return "Outward call (ACV9=OCALL)";
        case ACV10: return "Bad outward call (ACV10=BOC)";
        case ACV11: return "Inward return (ACV11=INRET) XXX ??";
        case ACV12: return "Invalid ring crossing (ACV12=CRT)";
        case ACV13: return "Ring alarm (ACV13=RALR)";
        case ACV14: return "Associative memory error XXX ??";
        case ACV15: return "Out of segment bounds (ACV15=OOSB)";
        //case ACDF0: return "Directed fault 0";
        //case ACDF1: return "Directed fault 1";
        //case ACDF2: return "Directed fault 2";
        //case ACDF3: return "Directed fault 3";
        default:
            break;
      }
  return "unhandled acv in strACV";
  }
#endif

static char *strPCT (_processor_cycle_type t)
  {
    switch (t)
      {
        case UNKNOWN_CYCLE: return "UNKNOWN_CYCLE";
        case OPERAND_STORE : return "OPERAND_STORE";
        case OPERAND_READ : return "OPERAND_READ";
        case INDIRECT_WORD_FETCH: return "INDIRECT_WORD_FETCH";
        case RTCD_OPERAND_FETCH: return "RTCD_OPERAND_FETCH";
        case INSTRUCTION_FETCH: return "INSTRUCTION_FETCH";
        case APU_DATA_READ: return "APU_DATA_READ";
        case APU_DATA_STORE: return "APU_DATA_STORE";
        case ABSA_CYCLE : return "ABSA_CYCLE";
#ifdef LOCKLESS
        case OPERAND_RMW : return "OPERAND_RMW";
        case APU_DATA_RMW : return "APU_DATA_RMW";
#endif

        default:
            return "Unhandled _processor_cycle_type";
      }
  
  }


/*
 * recoding APU functions to more closely match Fig 5,6 & 8 ...
 * Returns final address suitable for core_read/write
 */

//
// Phase 1:
//
//     A: Get the SDW
//
//     B: Check the ring
//
// Phase 2:
//
//     B1: If CALL6 operand
//           goto E
//         If instruction fetch or transfer instruction operand
//           goto F
//         If write
//           check write permission
//         else
//           check read permission
//         goto G
//
//     E: -- CALL6 operand handling
//        Check execute and gate bits
//        Get the ring
//        goto G
//
//     F: -- instruction fetch or transfer instruction operand
//        Check execute bit and ring
//        goto D
//
//     D: Check RALR
//        goto G
//
// Phase 3
//
//     G: Check BOUND
//        If not paged
//          goto H
//        Fetch PTW
//        Fetch prepage PTW
//        Goto I
//
//     H: Compute final address
//        Goto HI
//
//     I: If write
//          set PTW.M
//        Compute final address
//        Goto HI
//
// Phase 4
//
//     HI: --
//         If indirect word fetch
//           goto J
//         if rtcd operand fetch
//           goto K
//         if CALL6
//           goto N
//         If instruction fetch or transfer instruction operand
//           goto L
//         APU data movement?
//           load/store APU data
//
//    K: Set PPR.P
//         Goto J
//
//    J: return
//

// CANFAULT

word24 doAppendCycle (_processor_cycle_type thisCycle, word36 * data,
                      uint nWords)
  {
    DCDstruct * i = & cpu.currentInstruction;
    DBGAPP ("doAppendCycle(Entry) thisCycle=%s\n",
            strPCT (thisCycle));
    DBGAPP ("doAppendCycle(Entry) lastCycle=%s\n",
            strPCT (cpu.apu.lastCycle));
    DBGAPP ("doAppendCycle(Entry) CA %06o\n",
            cpu.TPR.CA);
    DBGAPP ("doAppendCycle(Entry) n=%2u\n",
            nWords);
    DBGAPP ("doAppendCycle(Entry) PPR.PRR=%o PPR.PSR=%05o\n",
            cpu.PPR.PRR, cpu.PPR.PSR);
    DBGAPP ("doAppendCycle(Entry) TPR.TRR=%o TPR.TSR=%05o\n",
            cpu.TPR.TRR, cpu.TPR.TSR);
    //if (cpu.isb29)
    if (i->b29)
      {
        DBGAPP ("doAppendCycle(Entry) isb29 PRNO %o\n",
                GET_PRN (IWB_IRODD));
      }

    bool instructionFetch = (thisCycle == INSTRUCTION_FETCH);
    bool StrOp = (thisCycle == OPERAND_STORE ||
                  thisCycle == APU_DATA_STORE);

#ifdef WAM
    // AL39: The associative memory is ignored (forced to "no match") during
    // address preparation.
    // lptp,lptr,lsdp,lsdr,sptp,sptr,ssdp,ssdr
    // Unfortunately, ISOLTS doesn't try to execute any of these in append mode.
    // XXX should this be only for OPERAND_READ and OPERAND_STORE?
    bool nomatch = ((i->opcode == 0232 || i->opcode == 0254 ||
                     i->opcode == 0154 || i->opcode == 0173) &&
                     i->opcodeX ) ||
                    ((i->opcode == 0557 || i->opcode == 0257) &&
                     ! i->opcodeX);
#endif

    _processor_cycle_type lastCycle = cpu.apu.lastCycle;
    cpu.apu.lastCycle = thisCycle;

    DBGAPP ("doAppendCycle(Entry) XSF %o\n", cpu.cu.XSF);

    PNL (L68_ (cpu.apu.state = 0;))

    cpu.RSDWH_R1 = 0;
    
    cpu.acvFaults = 0;

//#define FMSG(x) x
#define FMSG(x) 
    FMSG (char * acvFaultsMsg = "<unknown>";)

    word24 finalAddress = (word24) -1;  // not everything requires a final
                                        // address
    
////////////////////////////////////////
//
// Sheet 1: "START APPEND"
//
////////////////////////////////////////

// START APPEND
    word3 n = 0; // PRn to be saved to TSN_PRNO

    if (thisCycle == APU_DATA_READ ||
#ifdef LOCKLESS
	thisCycle == APU_DATA_RMW ||
#endif
        thisCycle == APU_DATA_STORE)
      goto A;

#ifdef LOCKLESS
    // locked RMW
    if (thisCycle == OPERAND_RMW)
      goto A;
#endif

    // R/M/W?
    if (thisCycle == OPERAND_STORE &&
        lastCycle == OPERAND_READ)
      goto A;

    if (lastCycle == INDIRECT_WORD_FETCH || cpu.cu.XSF)
      goto A;

    if (lastCycle == RTCD_OPERAND_FETCH)
      goto A;

    //if (lastCycle != INSTRUCTION_FETCH && i -> a)
    //if (lastCycle != INSTRUCTION_FETCH && cpu.cu.TSN_VALID [0])
    //if (lastCycle != INSTRUCTION_FETCH)
    if (thisCycle != INSTRUCTION_FETCH)
      {
#if 0
        for (uint tsn = 0; tsn < 3; tsn ++)
          {
            if (cpu.cu.TSN_VALID [tsn])
              {
                PNL (L68_ (cpu.apu.state |= apu_ESN_SNR;))
                word3 n = cpu.cu.TSN_PRNO[tsn];
                CPTUR (cptUsePRn + n);
                if (cpu.PAR[n].RNR > cpu.PPR.PRR)
                  {
                    cpu.TPR.TRR = cpu.PAR[n].RNR;
                  }
                else
                 {
                    cpu.TPR.TRR = cpu.PPR.PRR;
                 }
                cpu.TPR.TSR = cpu.PAR[n].SNR;
                DBGAPP ("TSN TSR %05o TRR %o\n", cpu.TPR.TSR, cpu.TPR.TRR);
                goto A;
              }
          }
#else
        //if (cpu.isb29)
        if (i->b29)
          {
            PNL (L68_ (cpu.apu.state |= apu_ESN_SNR;))
            n = GET_PRN(IWB_IRODD);
            CPTUR (cptUsePRn + n);
            if (cpu.PAR[n].RNR > cpu.PPR.PRR)
              {
                cpu.TPR.TRR = cpu.PAR[n].RNR;
              }
            else
             {
                cpu.TPR.TRR = cpu.PPR.PRR;
             }
            cpu.TPR.TSR = cpu.PAR[n].SNR;
            cpu.cu.XSF = 1;
sim_debug (DBG_TRACEEXT, & cpu_dev, "doAppendCycle bit 29 sets XSF to 1\n");
            DBGAPP ("TSN TSR %05o TRR %o\n", cpu.TPR.TSR, cpu.TPR.TRR);
            goto A;
          }
#endif
      }

    cpu.TPR.TRR = cpu.PPR.PRR;
    cpu.TPR.TSR = cpu.PPR.PSR;

    // If the rtcd instruction is executed with the processor in absolute
    // mode with bit 29 of the instruction word set OFF and without
    // indirection through an ITP or ITS pair, then:
    //
    //   appending mode is entered for address preparation for the
    //   rtcd operand and is retained if the instruction executes
    //   successfully, and the effective segment number generated for
    //   the SDW fetch and subsequent loading into C(TPR.TSR) is equal
    //   to C(PPR.PSR) and may be undefined in absolute mode, and the
    //   effective ring number loaded into C(TPR.TRR) prior to the SDW
    //   fetch is equal to C(PPR.PRR) (which is 0 in absolute mode)
    //   implying that control is always transferred into ring 0.
    //
    if (thisCycle == RTCD_OPERAND_FETCH &&
        get_addr_mode() == ABSOLUTE_mode &&
        ! cpu.cu.XSF /*get_went_appending()*/)
      { 
        cpu.TPR.TSR = 0;
      }

    DBGAPP ("set TSR %05o TRR %o\n", cpu.TPR.TSR, cpu.TPR.TRR);
    goto A;

////////////////////////////////////////
//
// Sheet 2: "A"
//
////////////////////////////////////////

//
//  A:
//    Get SDW

A:;

    //PNL (cpu.APUMemAddr = address;)
    PNL (cpu.APUMemAddr = cpu.TPR.CA;)

    DBGAPP ("doAppendCycle(A)\n");
    
#ifndef WAM
    if (cpu.DSBR.U == 0)
      {
        fetchDSPTW (cpu.TPR.TSR);
        
        if (! cpu.PTW0.DF)
         {
          doFault (FAULT_DF0 + cpu.PTW0.FC, fst_zero, 
                   "doAppendCycle(A): PTW0.F == 0");
         }
        
        if (! cpu.PTW0.U)
         {
          modifyDSPTW (cpu.TPR.TSR);
          }
        
        fetchPSDW (cpu.TPR.TSR);
      }
    else
      {
        fetchNSDW (cpu.TPR.TSR); // load SDW0 from descriptor segment table.
      }

    if (cpu.SDW0.DF == 0)
      {
        if (thisCycle != ABSA_CYCLE)
          {
            DBGAPP ("doAppendCycle(A): SDW0.F == 0! "
                    "Initiating directed fault\n");
            // initiate a directed fault ...
            doFault (FAULT_DF0 + cpu.SDW0.FC, fst_zero, "SDW0.F == 0");
          }
      }

    loadSDWAM (cpu.TPR.TSR, true); // load SDW0 POINTER, always bypass SDWAM
#else

    // is SDW for C(TPR.TSR) in SDWAM?
    if (nomatch || ! fetchSDWfromSDWAM (cpu.TPR.TSR))
      {
        // No
        DBGAPP ("doAppendCycle(A):SDW for segment %05o not in SDWAM\n",
                 cpu.TPR.TSR);
        
        DBGAPP ("doAppendCycle(A):DSBR.U=%o\n",
                cpu.DSBR.U);
        
        if (cpu.DSBR.U == 0)
          {
            fetchDSPTW (cpu.TPR.TSR);
            
            if (! cpu.PTW0.DF)
              doFault (FAULT_DF0 + cpu.PTW0.FC, fst_zero,
                       "doAppendCycle(A): PTW0.F == 0");
            
            if (! cpu.PTW0.U)
              modifyDSPTW (cpu.TPR.TSR);
            
            fetchPSDW (cpu.TPR.TSR);
          }
        else
          fetchNSDW (cpu.TPR.TSR); // load SDW0 from descriptor segment table.
        
        if (cpu.SDW0.DF == 0)
          {
            if (thisCycle != ABSA_CYCLE)
              {
                DBGAPP ("doAppendCycle(A): SDW0.F == 0! "
                        "Initiating directed fault\n");
                // initiate a directed fault ...
                doFault (FAULT_DF0 + cpu.SDW0.FC, fst_zero, "SDW0.F == 0");
              }
          }
        // load SDWAM .....
        loadSDWAM (cpu.TPR.TSR, nomatch);
      }
#endif
    DBGAPP ("doAppendCycle(A) R1 %o R2 %o R3 %o E %o\n",
            cpu.SDW->R1, cpu.SDW->R2, cpu.SDW->R3, cpu.SDW->E);

    // Yes...
    cpu.RSDWH_R1 = cpu.SDW->R1;

////////////////////////////////////////
//
// Sheet 3: "B"
//
////////////////////////////////////////

//
// B: Check the ring
//

    DBGAPP ("doAppendCycle(B)\n");
    
    //C(SDW.R1) <= C(SDW.R2) <= C(SDW .R3)?
    if (! (cpu.SDW->R1 <= cpu.SDW->R2 && cpu.SDW->R2 <= cpu.SDW->R3))
      {
        // Set fault ACV0 = IRO
        cpu.acvFaults |= ACV0;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(B) C(SDW.R1) <= C(SDW.R2) <= "
                              "C(SDW .R3)";)
      }

    // Was last cycle an rtcd operand fetch?
    if (lastCycle == RTCD_OPERAND_FETCH)
      goto C;

//
// B1: The operand is one of: an instruction, data to be read or data to be
//     written
//

    // Is OPCODE call6?
    if (thisCycle == OPERAND_READ && (i->info->flags & CALL6_INS))
      goto E;

    // If the instruction is a transfer operand or we are doing an instruction
    // fetch, the operand is destined to be executed. Verify that the operand
    // is executable

    // The flowchart trips up on the TSP PRn|foo,* for the INDIRECT_WORD_FETCH.
    // Also, it transfers to F on RTCD PRn,n and E-OFFs; the operand is not in
    // an executable segment, and should be treated as READ_OPERAND here.


    // Transfer or instruction fetch?
    if (instructionFetch ||
        ((i->info->flags & TRANSFER_INS) &&
         thisCycle != INDIRECT_WORD_FETCH &&
         thisCycle != RTCD_OPERAND_FETCH))
      goto F;
    
#ifdef LOCKLESS
    if (!StrOp || thisCycle == OPERAND_RMW || thisCycle == APU_DATA_RMW)
#else
    if (!StrOp)
#endif
      {
        DBGAPP ("doAppendCycle(B):!STR-OP\n");
        
        // No
        // C(TPR.TRR) > C(SDW .R2)?
        if (cpu.TPR.TRR > cpu.SDW->R2)
          {
            DBGAPP ("ACV3\n");
            DBGAPP ("doAppendCycle(B) ACV3\n");
            //Set fault ACV3 = ORB
            cpu.acvFaults |= ACV3;
            PNL (L68_ (cpu.apu.state |= apu_FLT;))
            FMSG (acvFaultsMsg = "acvFaults(B) C(TPR.TRR) > C(SDW .R2)";)
          }
        
        if (cpu.SDW->R == 0)
          {
            //C(PPR.PSR) = C(TPR.TSR)?
            if (cpu.PPR.PSR != cpu.TPR.TSR)
              {
                DBGAPP ("ACV4\n");
                DBGAPP ("doAppendCycle(B) ACV4\n");
                //Set fault ACV4 = R-OFF
                cpu.acvFaults |= ACV4;
                PNL (L68_ (cpu.apu.state |= apu_FLT;))
                FMSG (acvFaultsMsg = "acvFaults(B) C(PPR.PSR) = C(TPR.TSR)";)
              }
          }
        
      }
#ifdef LOCKLESS
    if (StrOp || thisCycle == OPERAND_RMW || thisCycle == APU_DATA_RMW)
#else
    if (StrOp)
#endif
      {
        DBGAPP ("doAppendCycle(B):STR-OP\n");
        
        // C(TPR.TRR) > C(SDW .R1)? Note typo in AL39, R2 should be R1
        if (cpu.TPR.TRR > cpu.SDW->R1)
          {
            DBGAPP ("ACV5 TRR %o R1 %o\n",
                    cpu.TPR.TRR, cpu.SDW->R1);
            //Set fault ACV5 = OWB
            cpu.acvFaults |= ACV5;
            PNL (L68_ (cpu.apu.state |= apu_FLT;))
            FMSG (acvFaultsMsg = "acvFaults(B) C(TPR.TRR) > C(SDW .R1)";)
          }
        
        if (! cpu.SDW->W)
          {
            DBGAPP ("ACV6\n");
            // Set fault ACV6 = W-OFF
            cpu.acvFaults |= ACV6;
            PNL (L68_ (cpu.apu.state |= apu_FLT;))
            FMSG (acvFaultsMsg = "acvFaults(B) ACV6 = W-OFF";)
          }
        
      }
    goto G;
    
////////////////////////////////////////
//
// Sheet 4: "C" "D"
//
////////////////////////////////////////

C:;
    DBGAPP ("doAppendCycle(C)\n");
    // (rtcd operand)
    // C(TPR.TRR) < C(SDW.R1)?
    // C(TPR.TRR) > C(SDW.R2)?
    if (cpu.TPR.TRR < cpu.SDW->R1 ||
        cpu.TPR.TRR > cpu.SDW->R2)
      {
        DBGAPP ("ACV1 c\n");
        DBGAPP ("doAppendCycle(C) ACV1\n");
        //Set fault ACV1 = OEB
        cpu.acvFaults |= ACV1;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(C) C(SDW.R1 > C(TPR.TRR) > C(SDW.R2)";)
      }
    // SDW.E set ON?
    if (! cpu.SDW->E)
      {
        DBGAPP ("ACV2 a\n");
        DBGAPP ("doAppendCycle(C) ACV2\n");
        //Set fault ACV2 = E-OFF
        cpu.acvFaults |= ACV2;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(C) SDW.E";)
      }
    // C(TPR.TRR) >= C(PPR.PRR)
    if (cpu.TPR.TRR < cpu.PPR.PRR)
      {
        DBGAPP ("ACV11\n");
        DBGAPP ("doAppendCycle(C) ACV11\n");
        //Set fault ACV11 = INRET
        cpu.acvFaults |= ACV11;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(C) TRR>=PRR";)
      }

D:;
    DBGAPP ("doAppendCycle(D)\n");
    
    // transfer or instruction fetch

    if (cpu.rRALR == 0)
        goto G;
    
    // C(PPR.PRR) < RALR?
    if (! (cpu.PPR.PRR < cpu.rRALR))
      {
        DBGAPP ("ACV13\n");
        DBGAPP ("acvFaults(D) C(PPR.PRR) %o < RALR %o\n", 
                cpu.PPR.PRR, cpu.rRALR);
        cpu.acvFaults |= ACV13;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(D) C(PPR.PRR) < RALR";)
      }
    
    goto G;
    
////////////////////////////////////////
//
// Sheet 5: "E"
//
////////////////////////////////////////

E:;

//
// E: CALL6
//

    DBGAPP ("doAppendCycle(E): CALL6\n");
    DBGAPP ("doAppendCycle(E): E %o G %o PSR %05o TSR %05o CA %06o "
            "EB %06o R %o%o%o TRR %o PRR %o\n",
            cpu.SDW->E, cpu.SDW->G, cpu.PPR.PSR, cpu.TPR.TSR, cpu.TPR.CA,
            cpu.SDW->EB, cpu.SDW->R1, cpu.SDW->R2, cpu.SDW->R3,
            cpu.TPR.TRR, cpu.PPR.PRR);

    //SDW.E set ON?
    if (! cpu.SDW->E)
      {
        DBGAPP ("ACV2 b\n");
        DBGAPP ("doAppendCycle(E) ACV2\n");
        // Set fault ACV2 = E-OFF
        cpu.acvFaults |= ACV2;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(E) SDW .E set OFF";)
      }
    
    //SDW .G set ON?
    if (cpu.SDW->G)
      goto E1;
    
    // C(PPR.PSR) = C(TPR.TSR)?
    if (cpu.PPR.PSR == cpu.TPR.TSR && ! TST_I_ABS)
      goto E1;
    
    // XXX This doesn't seem right
// EB is word 15; masking address makes no sense; rather 0-extend EB
// Fixes ISOLTS 880-01
    if (cpu.TPR.CA >= (word18) cpu.SDW->EB)
      {
        DBGAPP ("ACV7\n");
        DBGAPP ("doAppendCycle(E) ACV7\n");
        // Set fault ACV7 = NO GA
        cpu.acvFaults |= ACV7;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(E) TPR.CA4-17 >= SDW.CL";)
      }
    
E1:
    DBGAPP ("doAppendCycle(E1): CALL6 (cont'd)\n");

    // C(TPR.TRR) > SDW.R3?
    if (cpu.TPR.TRR > cpu.SDW->R3)
      {
        DBGAPP ("ACV8\n");
        DBGAPP ("doAppendCycle(E) ACV8\n");
        //Set fault ACV8 = OCB
        cpu.acvFaults |= ACV8;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(E1) C(TPR.TRR) > SDW.R3";)
      }
    
    // C(TPR.TRR) < SDW.R1?
    if (cpu.TPR.TRR < cpu.SDW->R1)
      {
        DBGAPP ("ACV9\n");
        DBGAPP ("doAppendCycle(E) ACV9\n");
        // Set fault ACV9 = OCALL
        cpu.acvFaults |= ACV9;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(E1) C(TPR.TRR) < SDW.R1";)
      }
    
    
    // C(TPR.TRR) > C(PPR.PRR)?
    if (cpu.TPR.TRR > cpu.PPR.PRR)
      {
        // C(PPR.PRR) < SDW.R2?
        if (cpu.PPR.PRR < cpu.SDW->R2)
          {
            DBGAPP ("ACV10\n");
            DBGAPP ("doAppendCycle(E) ACV10\n");
            // Set fault ACV10 = BOC
            cpu.acvFaults |= ACV10;
            PNL (L68_ (cpu.apu.state |= apu_FLT;))
            FMSG (acvFaultsMsg = "acvFaults(E1) C(TPR.TRR) > C(PPR.PRR) && "
                  "C(PPR.PRR) < SDW.R2";)
          }
      }

    
    DBGAPP ("doAppendCycle(E1): CALL6 TPR.TRR %o SDW->R2 %o\n",
            cpu.TPR.TRR, cpu.SDW->R2);

    // C(TPR.TRR) > SDW.R2?
    if (cpu.TPR.TRR > cpu.SDW->R2)
      {
        // SDW.R2 -> C(TPR.TRR)
        cpu.TPR.TRR = cpu.SDW->R2;
      }

    DBGAPP ("doAppendCycle(E1): CALL6 TPR.TRR %o\n", cpu.TPR.TRR);
    
    goto G;
    
////////////////////////////////////////
//
// Sheet 6: "F"
//
////////////////////////////////////////

F:;
    PNL (L68_ (cpu.apu.state |= apu_PIAU;))
    DBGAPP ("doAppendCycle(F): transfer or instruction fetch\n");

    // C(TPR.TRR) < C(SDW .R1)?
    if (cpu.TPR.TRR < cpu.SDW->R1)
      {
        DBGAPP ("ACV1 a\n");
        DBGAPP ("acvFaults(F) C(TPR.TRR) %o < C(SDW .R1) %o\n",
                   cpu.TPR.TRR, cpu.SDW->R1);
        cpu.acvFaults |= ACV1;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(F) C(TPR.TRR) < C(SDW .R1)";)
      }
    
    // C(TPR.TRR) > C(SDW .R2)?
    if (cpu.TPR.TRR > cpu.SDW->R2)
      {
        DBGAPP ("ACV1 b\n");
        DBGAPP ("acvFaults(F) C(TPR.TRR) %o > C(SDW .R2) %o\n",
                   cpu.TPR.TRR, cpu.SDW -> R2);
        cpu.acvFaults |= ACV1;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(F) C(TPR.TRR) > C(SDW .R2)";)
      }
    
    // SDW .E set ON?
    if (! cpu.SDW->E)
      {
        DBGAPP ("ACV2 c \n");
        DBGAPP ("doAppendCycle(F) ACV2\n");
        cpu.acvFaults |= ACV2;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(F) SDW .E set OFF";)
      }
    
    // C(PPR.PRR) = C(TPR.TRR)?
    if (cpu.PPR.PRR != cpu.TPR.TRR)
      {
        DBGAPP ("ACV12\n");
        DBGAPP ("doAppendCycle(F) ACV12\n");
        //Set fault ACV12 = CRT
        cpu.acvFaults |= ACV12;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(F) C(PPR.PRR) != C(TPR.TRR)";)
      }
    
    goto D;

////////////////////////////////////////
//
// Sheet 7: "G"
//
////////////////////////////////////////

G:;
    
    DBGAPP ("doAppendCycle(G)\n");
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((cpu.TPR.CA >> 4) & 037777) > cpu.SDW->BOUND)
      {
        DBGAPP ("ACV15\n");
        DBGAPP ("doAppendCycle(G) ACV15\n");
        cpu.acvFaults |= ACV15;
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        FMSG (acvFaultsMsg = "acvFaults(G) C(TPR.CA)0,13 > SDW.BOUND";)
        DBGAPP ("acvFaults(G) C(TPR.CA)0,13 > SDW.BOUND\n"
                "   CA %06o CA>>4 & 037777 %06o SDW->BOUND %06o",
                cpu.TPR.CA, ((cpu.TPR.CA >> 4) & 037777), cpu.SDW->BOUND);
      }
    
    if (cpu.acvFaults)
      {
        DBGAPP ("doAppendCycle(G) acvFaults\n");
        PNL (L68_ (cpu.apu.state |= apu_FLT;))
        // Initiate an access violation fault
        doFault (FAULT_ACV, (_fault_subtype) {.fault_acv_subtype=cpu.acvFaults},
                 "ACV fault");
      }

    // is segment C(TPR.TSR) paged?
    if (cpu.SDW->U)
      goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    DBGAPP ("doAppendCycle(G) CA %06o\n", cpu.TPR.CA);
#ifndef WAM
    fetchPTW (cpu.SDW, cpu.TPR.CA);
    if (! cpu.PTW0.DF)
      {
        // cpu.TPR.CA = address;
        if (thisCycle != ABSA_CYCLE)
          {
            // initiate a directed fault
            doFault (FAULT_DF0 + cpu.PTW0.FC, fst_zero, "PTW0.F == 0");
          }
      } 
    // load PTW0 POINTER, always bypass PTWAM
    loadPTWAM (cpu.SDW->POINTER, cpu.TPR.CA, true);

#else
    if (nomatch ||
        ! fetchPTWfromPTWAM (cpu.SDW->POINTER, cpu.TPR.CA))  //TPR.CA))
      {
        fetchPTW (cpu.SDW, cpu.TPR.CA);
        if (! cpu.PTW0.DF)
          {
            // cpu.TPR.CA = address;
            if (thisCycle != ABSA_CYCLE)
              {
                // initiate a directed fault
                doFault (FAULT_DF0 + cpu.PTW0.FC, (_fault_subtype) {.bits=0},
                         "PTW0.F == 0");
              }
          }
        loadPTWAM (cpu.SDW->POINTER, cpu.TPR.CA, nomatch); // load PTW0 to PTWAM
      }
#endif
    
    // Prepage mode?
    // check for "uninterruptible" EIS instruction
    // ISOLTS-878 02: mvn,cmpn,mvne,ad3d; obviously also
    // ad2/3d,sb2/3d,mp2/3d,dv2/3d
    // DH03 p.8-13: probably also mve,btd,dtb
    if (i->opcodeX && ((i->opcode & 0770)== 0200|| (i->opcode & 0770) == 0220
        || (i->opcode & 0770)== 020|| (i->opcode & 0770) == 0300))
      {
        doPTW2 (cpu.SDW, cpu.TPR.CA);
      } 
    goto I;
    
////////////////////////////////////////
//
// Sheet 8: "H", "I"
//
////////////////////////////////////////

H:;
    DBGAPP ("doAppendCycle(H): FANP\n");

    PNL (L68_ (cpu.apu.state |= apu_FANP;))
#if 0
    // ISOLTS pa865 test-01a 101232
    if (get_bar_mode ())
      {
        setAPUStatus (apuStatus_FABS);
      }
    else
      ....
#endif
    setAPUStatus (apuStatus_FANP);

    DBGAPP ("doAppendCycle(H): SDW->ADDR=%08o CA=%06o \n",
            cpu.SDW->ADDR, cpu.TPR.CA);

    if (thisCycle == RTCD_OPERAND_FETCH &&
        get_addr_mode () == ABSOLUTE_mode &&
        ! cpu.cu.XSF /*get_went_appending ()*/)
      { 
        finalAddress = cpu.TPR.CA;
      }
    else
      {
        finalAddress = (cpu.SDW->ADDR & 077777760) + cpu.TPR.CA;
        finalAddress &= 0xffffff;
      }
    PNL (cpu.APUMemAddr = finalAddress;)
    
    DBGAPP ("doAppendCycle(H:FANP): (%05o:%06o) finalAddress=%08o\n",
            cpu.TPR.TSR, cpu.TPR.CA, finalAddress);
    
    //if (thisCycle == ABSA_CYCLE)
    //    goto J;
    goto HI;
    
I:;

// Set PTW.M

    DBGAPP ("doAppendCycle(I): FAP\n");
#ifdef LOCKLESS
    if ((StrOp ||
        thisCycle == OPERAND_RMW ||
        thisCycle == APU_DATA_RMW) && cpu.PTW->M == 0)  // is this the right way to do this?
#else
    if (StrOp && cpu.PTW->M == 0)  // is this the right way to do this?
#endif
      {
       modifyPTW (cpu.SDW, cpu.TPR.CA);
      }
    
    // final address paged
    setAPUStatus (apuStatus_FAP);
    PNL (L68_ (cpu.apu.state |= apu_FAP;))
    
    word24 y2 = cpu.TPR.CA % 1024;
    
    // AL39: The hardware ignores low order bits of the main memory page
    // address according to page size    
    finalAddress = (((word24)cpu.PTW->ADDR & 0777760) << 6) + y2; 
    finalAddress &= 0xffffff;
    PNL (cpu.APUMemAddr = finalAddress;)
    
#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr)
      addAPUhist (APUH_FAP);
#endif
    DBGAPP ("doAppendCycle(H:FAP): (%05o:%06o) finalAddress=%08o\n",
            cpu.TPR.TSR, cpu.TPR.CA, finalAddress);

    //if (thisCycle == ABSA_CYCLE)
    //    goto J;
    goto HI;

HI:
    DBGAPP ("doAppendCycle(HI)\n");

    if (thisCycle == OPERAND_STORE && cpu.useZone)
      {
        core_write_zone (finalAddress, * data, strPCT (thisCycle));
      }
    else if (StrOp)
      {
        core_writeN (finalAddress, data, nWords, strPCT (thisCycle));
      }
    else
      {
#ifdef LOCKLESS
	if (thisCycle == OPERAND_RMW || thisCycle == APU_DATA_RMW)
	  {
	    if (operand_size() != 1)
	      sim_warn("doAppend operand size !=1\n");
	    core_read_lock (finalAddress, data, strPCT (thisCycle));
	  }
	else
	  core_readN (finalAddress, data, nWords, strPCT (thisCycle));
#else
        core_readN (finalAddress, data, nWords, strPCT (thisCycle));
#endif
      }

    // Was this an indirect word fetch?
    if (thisCycle == INDIRECT_WORD_FETCH)
      goto J;

    // Was this an rtcd operand fetch?
    if (thisCycle == RTCD_OPERAND_FETCH)
      goto K;

    // is OPCODE call6?
    if ((! instructionFetch) && i->info->flags & CALL6_INS)
      goto N;

    // Transfer or instruction fetch?
    if (instructionFetch || (i->info->flags & TRANSFER_INS))
      goto L;

    // APU data movement?
    //  handled above
   goto Exit;

    
////////////////////////////////////////
//
// Sheet 9: "J"
//
////////////////////////////////////////

// Indirect operand fetch

J:;
    DBGAPP ("doAppendCycle(J)\n");

    // ri or ir & TPC.CA even?
    word6 tag = GET_TAG (IWB_IRODD);
    if ((GET_TM (tag) == TM_IR || GET_TM (tag) == TM_RI) &&
        (cpu.TPR.CA & 1) == 0)
      {
        if (ISITS (* data))
          goto O;
        if (ISITP (* data))
          goto P;
      }

    // C(Y) tag == other indirect?
    //   TM_R never indirects
    //   TM_RI always indirects
    //   TM_IR always indirects
    //   TM_IT always indirects
#if 0
    //     IT_CI, IT_SC, IT_SCR -- address is used for tally word
    //     IT_I indirects
    //     IT_AD -- address is used for tally word
    //     IT_SD -- address is used for tally word
    //     IT_DI -- address is used for tally word
    //     IT_ID -- address is used for tally word
    //     IT_DIC -- address is used for tally word
    //     IT_IDC -- address is used for tally word
    static const bool isInd [64] =
      {
        // 00-15 R_MOD
        false, false, false, false, false, false, false, false,
        false, false, false, false, false, false, false, false,
        // 16-31 RI_MOD
        true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true,
        // 32-47 IT_MOD
        // f1  und    und    und    sd     scr    f2     f3
        false, false, false, false, true,  true,  false, false,
        // ci  i      sc     ad     di     dic    id     idc
        true,  true,  true,  true,  true,  true,  true,  true, 
        // 48-63 IR_MOD
        true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true
      };
    if (isInd [(* data) & MASK6])
#else
    if ((* data) & 060)
#endif
      {
        // C(Y)0,17 -> C(IWB)0,17
        // C(Y)30,35 -> C(IWB)30,35
        // 0 -> C(IWB)29
        updateIWB (GET_OFFSET (* data), (* data) & MASK6);

        cpu.cu.TSN_PRNO [0] = n;
        cpu.cu.TSN_VALID [0] = 1;

      }

     goto Exit;

////////////////////////////////////////
//
// Sheet 10: "K", "L", "M", "N"
//
////////////////////////////////////////

K:; // RTCD operand fetch
    DBGAPP ("doAppendCycle(K)\n");

    word3 y = getbits36_3 (* data, 18);

    // C(Y-pair)3,17 -> C(PPR.PSR)
    // We set TSR here; TSR will be copied to PSR at KL
    cpu.TPR.TSR = GETHI (* data) & MASK15;

    // Maximum of
    // C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) -> C(PPR.PRR)
    // We set TRR here as well
    cpu.PPR.PRR = cpu.TPR.TRR = max3 (y, cpu.TPR.TRR, cpu.RSDWH_R1);

    // C(Y-pair)36,53 -> C(PPR.IC)
    // We set CA here; copied to IC  at KL
    cpu.TPR.CA = GETHI (* (data + 1));

    // If C(PPR.PRR) = 0 then C(SDW.P) -> C(PPR.P);
    //     otherwise 0 -> C(PPR.P)
    // Done at M

    goto KL;

L:; // Transfer or instruction fetch

    DBGAPP ("doAppendCycle(L)\n");

    // Is OPCODE tspn?
    if ((! instructionFetch) && i->info->flags & TSPN_INS)
      {
        //word3 n;
        if (i->opcode <= 0273)
          n = (i->opcode & 3);
        else
          n = (i->opcode & 3) + 4;

        // C(PPR.PRR) -> C(PRn .RNR)
        // C(PPR.PSR) -> C(PRn .SNR)
        // C(PPR.IC) -> C(PRn .WORDNO)
        // 000000 -> C(PRn .BITNO)
        cpu.PR[n].RNR = cpu.PPR.PRR;
// According the AL39, the PSR is 'undefined' in absolute mode.
// ISOLTS thinks means don't change the operand
        if (get_addr_mode () == APPEND_mode)
          cpu.PR[n].SNR = cpu.PPR.PSR;
        cpu.PR[n].WORDNO = (cpu.PPR.IC + 1) & MASK18;
        SET_PR_BITNO (n, 0);
        HDBGRegPR (n);
      }

    if (thisCycle == INSTRUCTION_FETCH &&
        lastCycle == RTCD_OPERAND_FETCH)
      {
        // C(PPR.PRR) -> C(PRn.RNR) for n = (0, 1, ..., 7)
        // Use TRR here; PRR not set until KL
        CPTUR (cptUsePRn + 0);
        CPTUR (cptUsePRn + 1);
        CPTUR (cptUsePRn + 2);
        CPTUR (cptUsePRn + 3);
        CPTUR (cptUsePRn + 4);
        CPTUR (cptUsePRn + 5);
        CPTUR (cptUsePRn + 6);
        CPTUR (cptUsePRn + 7);
        cpu.PR[0].RNR =
        cpu.PR[1].RNR =
        cpu.PR[2].RNR =
        cpu.PR[3].RNR =
        cpu.PR[4].RNR =
        cpu.PR[5].RNR =
        cpu.PR[6].RNR =
        cpu.PR[7].RNR = cpu.TPR.TRR;
        HDBGRegPR (0);
        HDBGRegPR (1);
        HDBGRegPR (2);
        HDBGRegPR (3);
        HDBGRegPR (4);
        HDBGRegPR (5);
        HDBGRegPR (6);
        HDBGRegPR (7);
      }
    goto KL;

KL:
    DBGAPP ("doAppendCycle(KL)\n");

    // C(TPR.TSR) -> C(PPR.PSR)
    cpu.PPR.PSR = cpu.TPR.TSR;
    // C(TPR.CA) -> C(PPR.IC) 
    cpu.PPR.IC = cpu.TPR.CA;

    goto M;

M: // Set P
    DBGAPP ("doAppendCycle(M)\n");

    // C(TPR.TRR) = 0?
    if (cpu.TPR.TRR == 0)
      {
        // C(SDW.P) -> C(PPR.P) 
        cpu.PPR.P = cpu.SDW->P;
      }
    else
      {
        // 0 C(PPR.P)
        cpu.PPR.P = 0;
      }

    goto Exit; 

N: // CALL6
    DBGAPP ("doAppendCycle(N)\n");

    // C(TPR.TRR) = C(PPR.PRR)?
    if (cpu.TPR.TRR == cpu.PPR.PRR)
      {
        // C(PR6.SNR) -> C(PR7.SNR) 
        cpu.PR[7].SNR = cpu.PR[6].SNR;
        DBGAPP ("doAppendCycle(N) PR7.SNR = PR6.SNR %05o\n", cpu.PR[7].SNR);
      }
    else
      {
        // C(DSBR.STACK) || C(TPR.TRR) -> C(PR7.SNR)
        cpu.PR[7].SNR = ((word15) (cpu.DSBR.STACK << 3)) | cpu.TPR.TRR;
        DBGAPP ("doAppendCycle(N) STACK %05o TRR %o\n",
                cpu.DSBR.STACK, cpu.TPR.TRR);
        DBGAPP ("doAppendCycle(N) PR7.SNR = STACK||TRR  %05o\n", cpu.PR[7].SNR);
      }

    // C(TPR.TRR) -> C(PR7.RNR)
    cpu.PR[7].RNR = cpu.TPR.TRR;
    // 00...0 -> C(PR7.WORDNO)
    cpu.PR[7].WORDNO = 0;
    // 000000 -> C(PR7.BITNO)
    SET_PR_BITNO (7, 0);
    HDBGRegPR (7);
    // C(TPR.TRR) -> C(PPR.PRR)
    cpu.PPR.PRR = cpu.TPR.TRR;
    // C(TPR.TSR) -> C(PPR.PSR)
    cpu.PPR.PSR = cpu.TPR.TSR;
    // C(TPR.CA) -> C(PPR.IC)
    cpu.PPR.IC = cpu.TPR.CA;

    goto M;

////////////////////////////////////////
//
// Sheet 11: "O", "P"
//
////////////////////////////////////////

O:; // ITS, RTCD
    DBGAPP ("doAppendCycle(O)\n");
    DBGAPP ("doAppendCycle(O) TRR %o RSDWH.R1 %o ITS.RNR %o\n",
            cpu.TPR.TRR, cpu.RSDWH_R1,
            (word3) (((* data) >> (18 - 3)) & MASK3));

    // C(TPR.TRR) >= RSDWH.R1?
    if (cpu.TPR.TRR >= cpu.RSDWH_R1)
      {
        // C(TPR.TRR) >= C(Y)18,20?
        if (cpu.TPR.TRR >= (((* data) >> (18 - 3)) & MASK3))
          goto Exit;
        // C(Y)18,20 -> C(TPR.TRR)
        cpu.TPR.TRR = (((* data) >> (18 - 3)) & MASK3);
        DBGAPP ("doAppendCycle(O) Set TRR from ITS [a] %o\n", cpu.TPR.TRR);
        goto Exit;
      }
    // C(Y)18,20 >= RSDWH.R1?
    if ((((* data) >> (18 - 3)) & MASK3) >= cpu.RSDWH_R1)
      {
        // C(Y)18,20 -> C(TPR.TRR)
        cpu.TPR.TRR = (((* data) >> (18 - 3)) & MASK3);
        DBGAPP ("doAppendCycle(O) Set TRR from ITS [b] %o\n", cpu.TPR.TRR);
      }
    else
      {
        // RSDWH.R1 -> C(TPR.TRR)
        cpu.TPR.TRR = cpu.RSDWH_R1;
        DBGAPP ("doAppendCycle(O) Set TRR from RSDWH_R1 [c] %o\n",
                   cpu.TPR.TRR);
      }
    goto Exit;
    
P:; // ITP

    DBGAPP ("doAppendCycle(P)\n");

    n = GET_PRN (* data);
    // C(TPR.TRR) >= RSDWH.R1?
    if (cpu.TPR.TRR >= cpu.RSDWH_R1)
      {
        // C(TPR.TRR) >= C(PRn .RNR)?
        if (cpu.TPR.TRR >= cpu.PR[n].RNR)
          goto Exit;
        // C(PRn .RNR) -> C(TPR.TRR)
        cpu.TPR.TRR = cpu.PR[n].RNR;
        goto Exit;
      }
    // C(PRn.RNR) >= RSDWH.R1?
    if (cpu.PR[n].RNR >= cpu.RSDWH_R1)
      {
        // C(PRn .RNR) -> C(TPR.TRR)
        cpu.TPR.TRR = cpu.PR[n].RNR;
      }
    else
      {
        // RSDWH.R1 -> C(TPR.TRR)
        cpu.TPR.TRR = cpu.RSDWH_R1;
      }
    goto Exit;
    

Exit:;

    PNL (cpu.APUDataBusOffset = cpu.TPR.CA;)
    PNL (cpu.APUDataBusAddr = finalAddress;)

    PNL (L68_ (cpu.apu.state |= apu_FA;))

    DBGAPP ("doAppendCycle (Exit) PRR %o PSR %05o P %o IC %06o\n",
            cpu.PPR.PRR, cpu.PPR.PSR, cpu.PPR.P, cpu.PPR.IC);
    DBGAPP ("doAppendCycle (Exit) TRR %o TSR %05o TBR %02o CA %06o\n",
            cpu.TPR.TRR, cpu.TPR.TSR, cpu.TPR.TBR, cpu.TPR.CA);

    //cpu.TPR.CA = address;
    return finalAddress;    // or 0 or -1???
  }

// Translate a segno:offset to a absolute address.
// Return 0 if successful.

int dbgLookupAddress (word18 segno, word18 offset, word24 * finalAddress,
                      char * * msg)
  {
    // Local copies so we don't disturb machine state

    ptw_s PTW1;
    sdw_s SDW1;

   if (2u * segno >= 16u * (cpu.DSBR.BND + 1u))
     {
       if (msg)
         * msg = "DSBR boundary violation.";
       return 1;
     }

    if (cpu.DSBR.U == 0)
      {
        // fetchDSPTW

        word24 y1 = (2 * segno) % 1024;
        word24 x1 = (2 * segno) / 1024; // floor

        word36 PTWx1;
        core_read ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);
        
        PTW1.ADDR = GETHI (PTWx1);
        PTW1.U = TSTBIT (PTWx1, 9);
        PTW1.M = TSTBIT (PTWx1, 6);
        PTW1.DF = TSTBIT (PTWx1, 2);
        PTW1.FC = PTWx1 & 3;
    
        if (! PTW1.DF)
          {
            if (msg)
              * msg = "!PTW0.F";
            return 2;
          }

        // fetchPSDW

        y1 = (2 * segno) % 1024;
    
        word36 SDWeven, SDWodd;
    
        core_read2 (((((word24)PTW1. ADDR & 0777760) << 6) + y1) & PAMASK, 
                    & SDWeven, & SDWodd, __func__);
    
        // even word
        SDW1.ADDR = (SDWeven >> 12) & 077777777;
        SDW1.R1 = (SDWeven >> 9) & 7;
        SDW1.R2 = (SDWeven >> 6) & 7;
        SDW1.R3 = (SDWeven >> 3) & 7;
        SDW1.DF = TSTBIT (SDWeven, 2);
        SDW1.FC = SDWeven & 3;
    
        // odd word
        SDW1.BOUND = (SDWodd >> 21) & 037777;
        SDW1.R = TSTBIT (SDWodd, 20);
        SDW1.E = TSTBIT (SDWodd, 19);
        SDW1.W = TSTBIT (SDWodd, 18);
        SDW1.P = TSTBIT (SDWodd, 17);
        SDW1.U = TSTBIT (SDWodd, 16);
        SDW1.G = TSTBIT (SDWodd, 15);
        SDW1.C = TSTBIT (SDWodd, 14);
        SDW1.EB = SDWodd & 037777;
      }
    else // ! DSBR.U
      {
        // fetchNSDW

        word36 SDWeven, SDWodd;
        
        core_read2 ((cpu.DSBR.ADDR + 2 * segno) & PAMASK, 
                    & SDWeven, & SDWodd, __func__);
        
        // even word
        SDW1.ADDR = (SDWeven >> 12) & 077777777;
        SDW1.R1 = (SDWeven >> 9) & 7;
        SDW1.R2 = (SDWeven >> 6) & 7;
        SDW1.R3 = (SDWeven >> 3) & 7;
        SDW1.DF = TSTBIT (SDWeven, 2);
        SDW1.FC = SDWeven & 3;
        
        // odd word
        SDW1.BOUND = (SDWodd >> 21) & 037777;
        SDW1.R = TSTBIT (SDWodd, 20);
        SDW1.E = TSTBIT (SDWodd, 19);
        SDW1.W = TSTBIT (SDWodd, 18);
        SDW1.P = TSTBIT (SDWodd, 17);
        SDW1.U = TSTBIT (SDWodd, 16);
        SDW1.G = TSTBIT (SDWodd, 15);
        SDW1.C = TSTBIT (SDWodd, 14);
        SDW1.EB = SDWodd & 037777;
    
      }

    if (SDW1.DF == 0)
      {
        if (msg)
          * msg = "!SDW0.F != 0";
        return 3;
      }

    if (((offset >> 4) & 037777) > SDW1.BOUND)
      {
        if (msg)
          * msg = "C(TPR.CA)0,13 > SDW.BOUND";
        return 4;
      }

    // is segment C(TPR.TSR) paged?
    if (SDW1.U)
      {
        * finalAddress = (SDW1.ADDR + offset) & PAMASK;
      }
    else
      {
        // fetchPTW
        word24 y2 = offset % 1024;
        word24 x2 = (offset) / 1024; // floor
    
        word36 PTWx2;
    
        core_read ((SDW1.ADDR + x2) & PAMASK, & PTWx2, __func__);
    
        PTW1.ADDR = GETHI (PTWx2);
        PTW1.U = TSTBIT (PTWx2, 9);
        PTW1.M = TSTBIT (PTWx2, 6);
        PTW1.DF = TSTBIT (PTWx2, 2);
        PTW1.FC = PTWx2 & 3;

        if (! PTW1.DF)
          {
            if (msg)
              * msg = "!PTW0.F";
            return 5;
          }

        y2 = offset % 1024;
    
        * finalAddress = ((((word24)PTW1.ADDR & 0777760) << 6) + y2) & PAMASK;
      }
    if (msg)
      * msg = "";
    return 0;
  }



