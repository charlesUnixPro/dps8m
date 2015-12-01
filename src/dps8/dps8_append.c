/**
 * \file dps8_append.c
 * \project dps8
 * \date 10/28/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_append.h"

/**
 * \brief the appending unit ...
 */

#if 0
typedef enum apuStatusBits
  {
    apuStatus_PI_AP = 1llu < (36 - 24),
    apuStatus_DSPTW = 1llu < (36 - 25),
    apuStatus_SDWNP = 1llu < (36 - 26),
    apuStatus_SDWP  = 1llu < (36 - 27),
    apuStatus_PTW   = 1llu < (36 - 28),
    apuStatus_PTW2  = 1llu < (36 - 29),
    apuStatus_FAP   = 1llu < (36 - 30),
    apuStatus_FANP  = 1llu < (36 - 31),
    apuStatus_FABS  = 1llu < (36 - 32),
  } apuStatusBits;

static const apuStatusBits apuStatusAll =
    apuStatus_PI_AP |
    apuStatus_DSPTW |
    apuStatus_SDWNP |
    apuStatus_SDWP  |
    apuStatus_PTW   |
    apuStatus_PTW2  |
    apuStatus_FAP   |
    apuStatus_FANP  |
    apuStatus_FABS;
#endif

void setAPUStatus (apuStatusBits status)
  {
#if 1
    CPU -> cu . APUCycleBits = status & 07770;
#else
    CPU -> cu . PI_AP = 0;
    CPU -> cu . DSPTW = 0;
    CPU -> cu . SDWNP = 0;
    CPU -> cu . SDWP  = 0;
    CPU -> cu . PTW   = 0;
    CPU -> cu . PTW2  = 0;
    CPU -> cu . FAP   = 0;
    CPU -> cu . FANP  = 0;
    CPU -> cu . FABS  = 0;
    switch (status)
      {
        case apuStatus_PI_AP:
          CPU -> cu . PI_AP = 1;
          break;
        case apuStatus_DSPTW:
        case apuStatus_MDSPTW: // XXX this doesn't seem like the right solution.
                               // XXX there is a MDSPTW bit in the APU history
                               // register, but not in the CU.
          CPU -> cu . DSPTW = 1;
          break;
        case apuStatus_SDWNP:
          CPU -> cu . SDWNP = 1;
          break;
        case apuStatus_SDWP:
          CPU -> cu . SDWP  = 1;
          break;
        case apuStatus_PTW:
        case apuStatus_MPTW: // XXX this doesn't seem like the right solution.
                             // XXX there is a MPTW bit in the APU history
                             // XXX register, but not in the CU.
          CPU -> cu . PTW   = 1;
          break;
        case apuStatus_PTW2:
          CPU -> cu . PTW2  = 1;
          break;
        case apuStatus_FAP:
          CPU -> cu . FAP   = 1;
          break;
        case apuStatus_FANP:
          CPU -> cu . FANP  = 1;
          break;
        case apuStatus_FABS:
          CPU -> cu . FABS  = 1;
          break;
      }
#endif
  }

#ifndef SPEED
static char *strSDW(_sdw *SDW);
#endif

static enum _appendingUnit_cycle_type appendingUnitCycleType = apuCycle_APPUNKNOWN;

/**

 The Use of Bit 29 in the Instruction Word
 The reader is reminded that there is a preliminary step of loading TPR.CA with the ADDRESS field of the instruction word during instruction decode.
 If bit 29 of the instruction word is set to 1, modification by pointer register is invoked and the preliminary step is executed as follows:
 1. The ADDRESS field of the instruction word is interpreted as shown in Figure 6-7 below.
 2. C(PRn.SNR) → C(TPR.TSR)
 3. maximum of ( C(PRn.RNR), C(TPR.TRR), C(PPR.PRR) ) → C(TPR.TRR)
 4. C(PRn.WORDNO) + OFFSET → C(TPR.CA) (NOTE: OFFSET is a signed binary number.)
 5. C(PRn.BITNO) → TPR.BITNO
 */


void doPtrReg(void)
{
    word3 n = GET_PRN(CPU -> cu.IWB);  // get PRn
    word15 offset = GET_OFFSET(CPU -> cu.IWB);
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doPtrReg(): PR[%o] SNR=%05o RNR=%o WORDNO=%06o BITNO=%02o\n", n, PAR[n].SNR, PAR[n].RNR, PAR[n].WORDNO, PAR[n].BITNO);
    TPR.TSR = PAR[n].SNR;
    TPR.TRR = max3(PAR[n].RNR, TPR.TRR, PPR.PRR);
    
    TPR.CA = (PAR[n].WORDNO + SIGNEXT15_18(offset)) & 0777777;
    TPR.TBR = PAR[n].BITNO;
    
    set_went_appending ();
    sim_debug(DBG_APPENDING, &cpu_dev, "doPtrReg(): n=%o offset=%05o TPR.CA=%06o TPR.TBR=%o TPR.TSR=%05o TPR.TRR=%o\n", n, offset, TPR.CA, TPR.TBR, TPR.TSR, TPR.TRR);
}

// Define this to do error detection on the PTWAM table.
// Useful if PTWAM reports an error message, but it slows the emulator
// down 50%

#ifdef do_selftestPTWAM
static void selftestPTWAM (void)
  {
    int usages [64];
    for (int i = 0; i < 64; i ++)
      usages [i] = -1;

    for (int i = 0; i < 64; i ++)
      {
        _ptw * p = PTWAM + i;
        if (p -> USE > 63)
          sim_printf ("PTWAM[%d].USE is %d; > 63!\n", i, p -> USE);
        if (usages [p -> USE] != -1)
          sim_printf ("PTWAM[%d].USE is equal to PTWAM[%d].USE; %d\n",
                      i, usages [p -> USE], p -> USE);
        usages [p -> USE] = i;
      }
    for (int i = 0; i < 64; i ++)
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
#ifndef SPEED
    // XXX is it enabled?
    // XXX Assuming 16 is 64 for the DPS8M

    // If SDWAM is enabled, then
    //   0 → C(SDWAM(i).FULL) for i = 0, 1, ..., 15
    //   i → C(SDWAM(i).USE) for i = 0, 1, ..., 15
    for (int i = 0; i < 64; i ++)
      {
        SDWAM [i] . F = 0;
        SDWAM [i] . USE = i;
      }

    // If PTWAM is enabled, then
    //   0 → C(PTWAM(i).FULL) for i = 0, 1, ..., 15
    //   i → C(PTWAM(i).USE) for i = 0, 1, ..., 15
    for (int i = 0; i < 64; i ++)
      {
        PTWAM [i] . F = 0;
        PTWAM [i] . USE = i;
      }
#ifdef do_selftestPTWAM
    selftestPTWAM ();
#endif
#endif // SPEED

    // If cache is enabled, reset all cache column and level full flags
    // XXX no cache

    // C(Y-pair) 0,23 → C(DSBR.ADDR)
    DSBR . ADDR = (Ypair [0] >> (35 - 23)) & PAMASK;

    // C(Y-pair) 37,50 → C(DSBR.BOUND)
    DSBR . BND = (Ypair [1] >> (71 - 50)) & 037777;

    // C(Y-pair) 55 → C(DSBR.U)
    DSBR . U = (Ypair [1] >> (71 - 55)) & 01;

    // C(Y-pair) 60,71 → C(DSBR.STACK)
    DSBR . STACK = (Ypair [1] >> (71 - 71)) & 07777;
    sim_debug (DBG_APPENDING, &cpu_dev, "ldbr 0 -> SDWAM/PTWAM[*].F, i -> SDWAM/PTWAM[i].USE, DSBR.ADDR 0%o, DSBR.BND 0%o, DSBR.U 0%o, DSBR.STACK 0%o\n", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK); 
    //sim_printf ("ldbr %012llo %012llo\n", Ypair [0], Ypair [1]);
    //sim_printf ("ldbr DSBR.ADDR %08o, DSBR.BND %05o, DSBR.U %o, DSBR.STACK %04o\n", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK); 
  }

/**
 * implement sdbr instruction
 */

void do_sdbr (word36 * Ypair)
  {
    // C(DSBR.ADDR) → C(Y-pair) 0,23
    // 00...0 → C(Y-pair) 24,36
    Ypair [0] = ((word36) (DSBR . ADDR & PAMASK)) << (35 - 23); 

    // C(DSBR.BOUND) → C(Y-pair) 37,50
    // 0000 → C(Y-pair) 51,54
    // C(DSBR.U) → C(Y-pair) 55
    // 000 → C(Y-pair) 56,59
    // C(DSBR.STACK) → C(Y-pair) 60,71
    Ypair [1] = ((word36) (DSBR . BND & 037777)) << (71 - 50) |
                ((word36) (DSBR . U & 1)) << (71 - 55) |
                ((word36) (DSBR . STACK & 07777)) << (71 - 71);
    //sim_printf ("sdbr DSBR.ADDR %08o, DSBR.BND %05o, DSBR.U %o, DSBR.STACK %04o\n", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK); 
    //sim_printf ("sdbr %012llo %012llo\n", Ypair [0], Ypair [1]);
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
    // XXX enable/disable and LRU don't seem to be implemented; punt
    // XXX ticket #1
#ifndef SPEED
    for (int i = 0; i < 64; i ++)
      {
        PTWAM [i] . F = 0;
      }
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
    // XXX enable/disable and LRU don't seem to be implemented; punt
    // XXX ticket #2
#ifndef SPEED
    for (int i = 0; i < 64; i ++)
      {
        SDWAM [i] . F = 0;
      }
#endif
  }

    
/**
 * fetch descriptor segment PTW ...
 */
// CANFAULT
static void fetchDSPTW(word15 segno)
{
    sim_debug (DBG_APPENDING, & cpu_dev, "fetchDSPTW segno 0%o\n", segno);
    if (2 * segno >= 16 * (DSBR.BND + 1))
        // generate access violation, out of segment bounds fault
        doFault(FAULT_ACV, ACV15, "acvFault: fetchDSPTW out of segment bounds fault");
        
    setAPUStatus (apuStatus_DSPTW);

    word24 y1 = (2 * segno) % 1024;
    word24 x1 = (2 * segno - y1) / 1024;

    word36 PTWx1;
    core_read((DSBR.ADDR + x1) & PAMASK, &PTWx1, __func__);
    
    PTW0.ADDR = GETHI(PTWx1);
    PTW0.U = TSTBIT(PTWx1, 9);
    PTW0.M = TSTBIT(PTWx1, 6);
    PTW0.F = TSTBIT(PTWx1, 2);
    PTW0.FC = PTWx1 & 3;
    
    sim_debug (DBG_APPENDING, & cpu_dev, "fetchDSPTW x1 0%o y1 0%o DSBR.ADDR 0%o PTWx1 0%012llo PTW0: ADDR 0%o U %o M %o F %o FC %o\n", x1, y1, DSBR.ADDR, PTWx1, PTW0.ADDR, PTW0.U, PTW0.M, PTW0.F, PTW0.FC);
}


/**
 * modify descriptor segment PTW (Set U=1) ...
 */
// CANFAULT
static void modifyDSPTW(word15 segno)
{
    if (2 * segno >= 16 * (DSBR.BND + 1))
        // generate access violation, out of segment bounds fault
        doFault(FAULT_ACV, ACV15, "acvFault: modifyDSPTW out of segment bounds fault");

    setAPUStatus (apuStatus_MDSPTW); 

    word24 y1 = (2 * segno) % 1024;
    word24 x1 = (2 * segno - y1) / 1024;
    
    word36 PTWx1;
    core_read((DSBR.ADDR + x1) & PAMASK, &PTWx1, __func__);
    PTWx1 = SETBIT(PTWx1, 9);
    core_write((DSBR.ADDR + x1) & PAMASK, PTWx1, __func__);
    
    PTW0.U = 1;
}


#ifndef SPEED
// XXX SDW0 is the in-core representation of a SDW. Need to have a SDWAM struct as current SDW!!!
static _sdw* fetchSDWfromSDWAM(word15 segno)
{
    sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(0):segno=%05o\n", segno);
    
    int nwam = 64;
    if (CPU -> switches . disable_wam)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(0): SDWAM disabled\n");
        nwam = 1;
        return NULL;
    }
    
    for(int _n = 0 ; _n < nwam ; _n++)
    {
        // make certain we initialize SDWAM prior to use!!!
        //if (SDWAM[_n]._initialized && segno == SDWAM[_n].POINTER)
        if (SDWAM[_n].F && segno == SDWAM[_n].POINTER)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(1):found match for segno %05o at _n=%d\n", segno, _n);
            
            SDW = &SDWAM[_n];
            
            /*
             If the SDWAM match logic circuitry indicates a hit, all usage counts (SDWAM.USE) greater than the usage count of the register hit are decremented by one, the usage count of the register hit is set to 15 (63?), and the contents of the register hit are read out into the address preparation circuitry. 
             */
            for(int _h = 0 ; _h < nwam ; _h++)
            {
                if (SDWAM[_h].USE > SDW->USE)
                    SDWAM[_h].USE -= 1;
            }
            SDW->USE = 63;
            
            sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(2):SDWAM[%d]=%s\n", _n, strSDW(SDW));
            return SDW;
        }
    }
    sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(3):SDW for segment %05o not found in SDWAM\n", segno);
    return NULL;    // segment not referenced in SDWAM
}
#endif // SPEED
/**
 * Fetches an SDW from a paged descriptor segment.
 */
// CANFAULT
static void fetchPSDW(word15 segno)
{
    sim_debug(DBG_APPENDING, &cpu_dev, "fetchPSDW(0):segno=%05o\n", segno);
    
    setAPUStatus (apuStatus_SDWP);
    word24 y1 = (2 * segno) % 1024;
    
    word36 SDWeven, SDWodd;
    
    core_read2(((PTW0.ADDR << 6) + y1) & PAMASK, &SDWeven, &SDWodd, __func__);
    
    // even word
    SDW0.ADDR = (SDWeven >> 12) & 077777777;
    SDW0.R1 = (SDWeven >> 9) & 7;
    SDW0.R2 = (SDWeven >> 6) & 7;
    SDW0.R3 = (SDWeven >> 3) & 7;
    SDW0.F = TSTBIT(SDWeven, 2);
    SDW0.FC = SDWeven & 3;
    
    // odd word
    SDW0.BOUND = (SDWodd >> 21) & 037777;
    SDW0.R = TSTBIT(SDWodd, 20);
    SDW0.E = TSTBIT(SDWodd, 19);
    SDW0.W = TSTBIT(SDWodd, 18);
    SDW0.P = TSTBIT(SDWodd, 17);
    SDW0.U = TSTBIT(SDWodd, 16);
    SDW0.G = TSTBIT(SDWodd, 15);
    SDW0.C = TSTBIT(SDWodd, 14);
    SDW0.EB = SDWodd & 037777;
    
    //PPR.P = (SDW0.P && PPR.PRR == 0);   // set priv bit (if OK)

    sim_debug (DBG_APPENDING, & cpu_dev, "fetchPSDW y1 0%o p->ADDR 0%o SDW 0%012llo 0%012llo ADDR 0%o BOUND 0%o U %o F %o\n",
 y1, PTW0.ADDR, SDWeven, SDWodd, SDW0.ADDR, SDW0.BOUND, SDW0.U, SDW0.F);
}

/// \brief Nonpaged SDW Fetch
/// Fetches an SDW from an unpaged descriptor segment.
// CANFAULT
static void fetchNSDW(word15 segno)
{
    sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(0):segno=%05o\n", segno);

    setAPUStatus (apuStatus_SDWNP);

    if (2 * segno >= 16 * (DSBR.BND + 1))
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(1):Access Violation, out of segment bounds for segno=%05o DSBR.BND=%d\n", segno, DSBR.BND);
        // generate access violation, out of segment bounds fault
        doFault(FAULT_ACV, ACV15, "acvFault fetchNSDW: out of segment bounds fault");
    }
    sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(2):fetching SDW from %05o\n", DSBR.ADDR + 2 * segno);
    word36 SDWeven, SDWodd;
    
    core_read2((DSBR.ADDR + 2 * segno) & PAMASK, &SDWeven, &SDWodd, __func__);
    
    // even word
    SDW0.ADDR = (SDWeven >> 12) & 077777777;
    SDW0.R1 = (SDWeven >> 9) & 7;
    SDW0.R2 = (SDWeven >> 6) & 7;
    SDW0.R3 = (SDWeven >> 3) & 7;
    SDW0.F = TSTBIT(SDWeven, 2);
    SDW0.FC = SDWeven & 3;
    
    // odd word
    SDW0.BOUND = (SDWodd >> 21) & 037777;
    SDW0.R = TSTBIT(SDWodd, 20);
    SDW0.E = TSTBIT(SDWodd, 19);
    SDW0.W = TSTBIT(SDWodd, 18);
    SDW0.P = TSTBIT(SDWodd, 17);
    SDW0.U = TSTBIT(SDWodd, 16);
    SDW0.G = TSTBIT(SDWodd, 15);
    SDW0.C = TSTBIT(SDWodd, 14);
    SDW0.EB = SDWodd & 037777;
    
    //PPR.P = (SDW0.P && PPR.PRR == 0);   // set priv bit (if OK)
    
    sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(2):SDW0=%s\n", strSDW0(&SDW0));
}

#ifndef SPEED
static char *strSDW(_sdw *SDW)
{
    static char buff[256];
    
    //if (SDW->ADDR == 0 && SDW->BOUND == 0) // need a better test
    //if (!SDW->_initialized)
    if (!SDW->F)
        sprintf(buff, "*** SDW Uninitialized ***");
    else
        sprintf(buff, "ADDR:%06o R1:%o R2:%o R3:%o BOUND:%o R:%o E:%o W:%o P:%o U:%o G:%o C:%o CL:%o POINTER=%o USE=%d",
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
                SDW->CL,
                SDW->POINTER,
                SDW->USE);
    return buff;
}
#endif

#ifndef SPEED
/**
 * dump SDWAM...
 */
t_stat dumpSDWAM (void)
{
    for(int _n = 0 ; _n < 64 ; _n++)
    {
        _sdw *p = &SDWAM[_n];
        
        //if (p->_initialized)
        if (p->F)
            sim_printf("SDWAM n:%d %s\n", _n, strSDW(p));
    }
    return SCPE_OK;
}
#endif


/**
 * load the current in-core SDW0 into the SDWAM ...
 */
static void loadSDWAM(word15 segno)
{
#ifdef SPEED
    SDWAM0 . ADDR = SDW0.ADDR;
    SDWAM0 . R1 = SDW0.R1;
    SDWAM0 . R2 = SDW0.R2;
    SDWAM0 . R3 = SDW0.R3;
    SDWAM0 . BOUND = SDW0.BOUND;
    SDWAM0 . R = SDW0.R;
    SDWAM0 . E = SDW0.E;
    SDWAM0 . W = SDW0.W;
    SDWAM0 . P = SDW0.P;
    SDWAM0 . U = SDW0.U;
    SDWAM0 . G = SDW0.G;
    SDWAM0 . C = SDW0.C;
    SDWAM0 . CL = SDW0.EB;
    SDWAM0 . POINTER = segno;
    SDWAM0 . USE = 0;
            
    SDWAM0 . F = true;     // in use by SDWAM
            
    SDW = & SDWAM0;
            
#else
    if (CPU -> switches . disable_wam)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM: SDWAM disabled\n");
        _sdw *p = &SDWAM[0];
        p->ADDR = SDW0.ADDR;
        p->R1 = SDW0.R1;
        p->R2 = SDW0.R2;
        p->R3 = SDW0.R3;
        p->BOUND = SDW0.BOUND;
        p->R = SDW0.R;
        p->E = SDW0.E;
        p->W = SDW0.W;
        p->P = SDW0.P;
        p->U = SDW0.U;
        p->G = SDW0.G;
        p->C = SDW0.C;
        p->CL = SDW0.EB;
        p->POINTER = segno;
        p->USE = 0;
            
        //p->_initialized = true;     // in use by SDWAM
        p->F = true;     // in use by SDWAM
            
        SDW = p;
            
        return;
    }
    
    /* If the SDWAM match logic does not indicate a hit, the SDW is fetched from the descriptor segment in main memory and loaded into the SDWAM register with usage count 0 (the oldest), all usage counts are decremented by one with the newly loaded register rolling over from 0 to 15 (63?), and the newly loaded register is read out into the address preparation circuitry.
     */
    for(int _n = 0 ; _n < 64 ; _n++)
    {
        _sdw *p = &SDWAM[_n];
        //if (!p->_initialized || p->USE == 0)
        if (!p->F || p->USE == 0)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(1):SDWAM[%d] p->USE=0\n", _n);
            
            p->ADDR = SDW0.ADDR;
            p->R1 = SDW0.R1;
            p->R2 = SDW0.R2;
            p->R3 = SDW0.R3;
            p->BOUND = SDW0.BOUND;
            p->R = SDW0.R;
            p->E = SDW0.E;
            p->W = SDW0.W;
            p->P = SDW0.P;
            p->U = SDW0.U;
            p->G = SDW0.G;
            p->C = SDW0.C;
            p->CL = SDW0.EB;
            p->POINTER = segno;
            p->USE = 0;
            
            //p->_initialized = true;     // in use by SDWAM
            p->F = true;     // in use by SDWAM
            
            for(int _h = 0 ; _h < 64 ; _h++)
            {
                _sdw *q = &SDWAM[_h];
                //if (!q->_initialized)
                if (!q->F)
                    continue;
                
                q->USE -= 1;
                q->USE &= 077;
            }
            
            SDW = p;
            
            sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(2):SDWAM[%d]=%s\n", _n, strSDW(p));
            
            return;
        }
    }
    sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(3) no USE=0 found for segment=%d\n", segno);
    
    sim_printf("loadSDWAM(%05o): no USE=0 found!\n", segno);
    dumpSDWAM();
#endif
}

#ifndef SPEED
static _ptw* fetchPTWfromPTWAM(word15 segno, word18 CA)
{
    int nwam = 64;
    if (CPU -> switches . disable_wam)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchPTWfromPTWAM: PTWAM disabled\n");
        nwam = 1;
        return NULL;
    }
    
    for(int _n = 0 ; _n < nwam ; _n++)
    {
        if (((CA >> 10) & 0377) == ((PTWAM[_n].PAGENO >> 4) & 0377) && PTWAM[_n].POINTER == segno && PTWAM[_n].F)   //_initialized)
        {
            PTW = &PTWAM[_n];
            
            /*
             * If the PTWAM match logic circuitry indicates a hit, all usage counts (PTWAM.USE) greater than the usage count of the register hit are decremented by one, the usage count of the register hit is set to 15 (63?), and the contents of the register hit are read out into the address preparation circuitry.
             */
            for(int _h = 0 ; _h < nwam ; _h++)
            {
                if (PTWAM[_h].USE > PTW->USE)
                    PTWAM[_h].USE -= 1; //PTW->USE -= 1;
            }
            PTW->USE = 63;
#ifdef do_selftestPTWAM
            selftestPTWAM ();
#endif
            return PTW;
        }
    }
    return NULL;    // segment not referenced in SDWAM
}
#endif

static void fetchPTW(_sdw *sdw, word18 offset)
{

    setAPUStatus (apuStatus_PTW);

    word24 y2 = offset % 1024;
    word24 x2 = (offset - y2) / 1024;
    
    word36 PTWx2;
    
    sim_debug (DBG_APPENDING,& cpu_dev, "fetchPTW address %08o\n", sdw->ADDR + x2);

    core_read((sdw->ADDR + x2) & PAMASK, &PTWx2, __func__);
    
    PTW0.ADDR = GETHI(PTWx2);
    PTW0.U = TSTBIT(PTWx2, 9);
    PTW0.M = TSTBIT(PTWx2, 6);
    PTW0.F = TSTBIT(PTWx2, 2);
    PTW0.FC = PTWx2 & 3;
    
    sim_debug (DBG_APPENDING, & cpu_dev, "fetchPTW x2 0%o y2 0%o sdw->ADDR 0%o PTWx2 0%012llo PTW0: ADDR 0%o U %o M %o F %o FC %o\n", x2, y2, sdw->ADDR, PTWx2, PTW0.ADDR, PTW0.U, PTW0.M, PTW0.F, PTW0.FC);
}

static void loadPTWAM(word15 segno, word18 offset)
{
#ifdef SPEED
    PTWAM0 . ADDR = PTW0.ADDR;
    PTWAM0 . M = PTW0.M;
    PTWAM0 . PAGENO = (offset >> 6) & 07777;
    PTWAM0 . POINTER = segno;
    PTWAM0 . USE = 0;
            
    PTWAM0 . F = true;
            
    PTW = & PTWAM0;
#else
    if (CPU -> switches . disable_wam)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "loadPTWAM: PTWAM disabled\n");
        _ptw *p = &PTWAM[0];
        p->ADDR = PTW0.ADDR;
        p->M = PTW0.M;
        p->PAGENO = (offset >> 6) & 07777;
        p->POINTER = segno;
        p->USE = 0;
            
            //p->_initialized = true;
        p->F = true;
            
        PTW = p;
        return;
    }
    
    /*
     * If the PTWAM match logic does not indicate a hit, the PTW is fetched from main memory and loaded into the PTWAM register with usage count 0 (the oldest), all usage counts are decremented by one with the newly loaded register rolling over from 0 to 15 (63), and the newly loaded register is read out into the address preparation circuitry.
     */
    for(int _n = 0 ; _n < 64 ; _n++)
    {
        _ptw *p = &PTWAM[_n];
        //if (!p->_initialized || p->USE == 0)
        if (!p->F || p->USE == 0)
        {
            p->ADDR = PTW0.ADDR;
            p->M = PTW0.M;
            p->PAGENO = (offset >> 6) & 07777;
            p->POINTER = segno;
            p->USE = 0;
            
            //p->_initialized = true;
            p->F = true;
            
            for(int _h = 0 ; _h < 64 ; _h++)
            {
                _ptw *q = &PTWAM[_h];
                //if (!q->_initialized)
                //if (!q->F)
                    //continue;
                
                q->USE -= 1;
                q->USE &= 077;
            }
            
            PTW = p;
#ifdef do_selftestPTWAM
            selftestPTWAM ();
#endif
            return;
        }
    }
    sim_printf("loadPTWAM(segno=%05o, offset=%012o): no USE=0 found!\n", segno, offset);
#endif // SPEED
}

/**
 * modify target segment PTW (Set M=1) ...
 */
static void modifyPTW(_sdw *sdw, word18 offset)
{
    word24 y2 = offset % 1024;
    word24 x2 = (offset - y2) / 1024;
    
    word36 PTWx2;
    
    setAPUStatus (apuStatus_MPTW);

    core_read((sdw->ADDR + x2) & PAMASK, &PTWx2, __func__);
    PTWx2 = SETBIT(PTWx2, 6);
    core_write((sdw->ADDR + x2) & PAMASK, PTWx2, __func__);
//if_sim_debug (DBG_TRACE, & cpu_dev)
//sim_printf ("modifyPTW 0%o %012llo ADDR %o U %llo M %llo F %llo FC %llo\n",
            //sdw -> ADDR + x2, PTWx2, GETHI (PTWx2), TSTBIT(PTWx2, 9), 
            //TSTBIT(PTWx2, 6), TSTBIT(PTWx2, 2), PTWx2 & 3);
   
    PTW->M = 1;
}



/**
 * Is the instruction a SToRage OPeration ?
 */
#ifndef QUIET_UNUSED
static bool isAPUDataMovement(word36 inst)
{
    // XXX: implement - when we figure out what it is
    return false;
}
#endif

#ifndef QUIET_UNUSED
static char *strAccessType(MemoryAccessType accessType)
{
    switch (accessType)
    {
        case Unknown:           return "Unknown";
        case InstructionFetch:  return "InstructionFetch";
        case IndirectRead:      return "IndirectRead";
        //case DataRead:          return "DataRead";
        //case DataWrite:         return "DataWrite";
        case OperandRead:       return "Operand/Data-Read";
        case OperandWrite:      return "Operand/Data-Write";
        case Call6Operand:      return "Call6Operand";
        case RTCDOperand:       return "RTCDOperand";
        default:                return "???";
    }
}
#endif

static char *strACV(_fault_subtype acv)
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

static int acvFaults = 0;   ///< pending ACV faults

// CANFAULT
void acvFault(_fault_subtype acvfault, char * msg)
{
    
    char temp[256];
    sprintf(temp, "group 6 ACV fault %s(%d): %s\n", strACV(acvfault), acvfault, msg);

    sim_debug (DBG_APPENDING, & cpu_dev, "%s", temp);
    
    //acvFaults |= (1 << acvfault);   // or 'em all together
    acvFaults |= acvfault;   // or 'em all together

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(acvFault): acvFault=%s(%ld) acvFaults=%d: %s\n", strACV(acvfault), (long)acvfault, acvFaults, msg);
    
    doFault(FAULT_ACV, acvfault, temp); // NEW HWR 17 Dec 2013
}

static char *strPCT(_processor_cycle_type t)
{
    switch (t)
    {
        case UNKNOWN_CYCLE: return "UNKNOWN_CYCLE";
        case APPEND_CYCLE: return "APPEND_CYCLE";
        case CA_CYCLE: return "CA_CYCLE";
        case OPERAND_STORE : return "OPERAND_STORE";
        case OPERAND_READ : return "OPERAND_READ";
        case DIVIDE_EXECUTION: return "DIVIDE_EXECUTION";
        case FAULT: return "FAULT";
        case INDIRECT_WORD_FETCH: return "INDIRECT_WORD_FETCH";
        case RTCD_OPERAND_FETCH: return "RTCD_OPERAND_FETCH";
        //case SEQUENTIAL_INSTRUCTION_FETCH: return "SEQUENTIAL_INSTRUCTION_FETCH";
        case INSTRUCTION_FETCH: return "INSTRUCTION_FETCH";
        case APU_DATA_MOVEMENT: return "APU_DATA_MOVEMENT";
        case ABORT_CYCLE: return "ABORT_CYCLE";
        case FAULT_CYCLE: return "FAULT_CYCLE";
        case EIS_OPERAND_STORE : return "EIS_OPERAND_STORE";
        case EIS_OPERAND_READ : return "EIS_OPERAND_READ";

        default:
            return "Unhandled _processor_cycle_type";
    }
  
}

// CANFAULT
_sdw0 * getSDW (word15 segno)
  {
     sim_debug (DBG_APPENDING, & cpu_dev, "getSDW for segment %05o\n", segno);
     sim_debug (DBG_APPENDING, & cpu_dev, "getSDW DSBR.U=%o\n", DSBR . U);
        
    if (DSBR . U == 0)
      {
        fetchDSPTW (segno);
            
        if (! PTW0 . F)
          doFault (FAULT_DF0 + PTW0.FC, 0, "getSDW PTW0.F == 0");
            
        if (! PTW0 . U)
          modifyDSPTW (segno);
            
        fetchPSDW (segno);
      }
    else
      fetchNSDW (segno);
        
    if (SDW0 . F == 0)
      {
        sim_debug (DBG_APPENDING, & cpu_dev,
                   "getSDW SDW0.F == 0! Initiating directed fault\n");
        doFault (FAULT_DF0 + SDW0 . FC, 0, "SDW0.F == 0");
     }
   return & SDW0;
  }

//static bool bPrePageMode = false;

/*
 * recoding APU functions to more closely match Fig 5,6 & 8 ...
 * Returns final address suitable for core_read/write
 */

// Usage notes:
//   Checks for the following conditions:
//     thisCycle == INSTRUCTION_FETCH, OPERAND_STORE, EIS_OPERAND_STORE, 
//                  RTCD_OPERAND_FETCH
//     thisCycle == OPERAND_READ && i->info->flags & CALL6_INS
//     thisCycle == OPERAND_READ && (i->info && i->info->flags & TRANSFER_INS

// CANFAULT
word24 doAppendCycle (word18 address, _processor_cycle_type thisCycle)
{
    DCDstruct * i = & CPU -> currentInstruction;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) thisCycle=%s\n", strPCT(thisCycle));
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) Address=%06o\n", address);
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) PPR.PRR=%o PPR.PSR=%05o\n", PPR.PRR, PPR.PSR);
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) TPR.TRR=%o TPR.TSR=%05o\n", TPR.TRR, TPR.TSR);

    bool instructionFetch = (thisCycle == INSTRUCTION_FETCH);
    bool StrOp = (thisCycle == OPERAND_STORE || thisCycle == EIS_OPERAND_STORE);
    
    RSDWH_R1 = 0;
    
    acvFaults = 0;
    char * acvFaultsMsg = "<unknown>";

    word24 finalAddress = -1;  // not everything requires a final address
    
//
//  A:
//    Get SDW
#ifndef QUIET_UNUSED
A:;
#endif

    TPR . CA = address;
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
//           goto Exit
//         if rtcd operand fetch
//           goto KL
//         If instruction fetch or transfer instruction operand
//           goto KL
//         "APU data movement"
//         Goto Exit
//
//    KL: Set PPR.P
//         Goto Exit
//
//    Exit: return
//

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A)\n");
    
#ifdef SPEED
    if (DSBR.U == 0)
    {
        fetchDSPTW(TPR.TSR);
        
        if (!PTW0.F)
            doFault(FAULT_DF0 + PTW0.FC, 0, "doAppendCycle(A): PTW0.F == 0");
        
        if (!PTW0.U)
            modifyDSPTW(TPR.TSR);
        
        fetchPSDW(TPR.TSR);
    }
    else
        fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
    
    if (SDW0.F == 0)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A): SDW0.F == 0! Initiating directed fault\n");
        // initiate a directed fault ...
        doFault(FAULT_DF0 + SDW0.FC, 0, "SDW0.F == 0");
    }
    loadSDWAM(TPR.TSR);
#else
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(TPR.TSR))
    {
        // No
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A):DSBR.U=%o\n", DSBR.U);
        
        if (DSBR.U == 0)
        {
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                doFault(FAULT_DF0 + PTW0.FC, 0, "doAppendCycle(A): PTW0.F == 0");
            
            if (!PTW0.U)
                modifyDSPTW(TPR.TSR);
            
            fetchPSDW(TPR.TSR);
        }
        else
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        
        if (SDW0.F == 0)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A): SDW0.F == 0! Initiating directed fault\n");
            // initiate a directed fault ...
            doFault(FAULT_DF0 + SDW0.FC, 0, "SDW0.F == 0");
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
#endif
    sim_debug (DBG_APPENDING, & cpu_dev,
               "doAppendCycle(A) R1 %o R2 %o R3 %o\n", SDW -> R1, SDW -> R1, SDW -> R3);
    // Yes...
    RSDWH_R1 = SDW->R1;

//
// B: Check the ring
//

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B)\n");
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(ACV0, "doAppendCycle(B) C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)");

    // No

//
// B1: The operand is one of: an instruction, data to be read or data to be
//     written
//

    // CALL6 must check the call gate. Also it has the feature of checking 
    // the target address for execution permission (instead of allowing the 
    // instruction fetch of the next cycle to do it). This allows the fault 
    // to occur in the calling instruction, which is easier to debug.

    // Is OPCODE call6?
    if (thisCycle == OPERAND_READ && i->info->flags & CALL6_INS)
      goto E;
 

    // If the instruction is a transfer operand or we are doing an instruction
    // fetch, the operand is destined to be executed. Verify that the operand
    // is executable

    // Transfer or instruction fetch?
    if (instructionFetch || (thisCycle == OPERAND_READ && (i->info && i->info->flags & TRANSFER_INS)))
        goto F;
    
    // Not executed, therefore it is data. Read or Write?

    if (StrOp)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B):STR-OP\n");
        
        // C(TPR.TRR) > C(SDW .R2)?
        if (TPR.TRR > SDW->R2)
            //Set fault ACV5 = OWB
            acvFault(ACV5, "doAppendCycle(B) C(TPR.TRR) > C(SDW .R2)");
        
        if (!SDW->W)
            // Set fault ACV6 = W-OFF
            acvFault(ACV6, "doAppendCycle(B) ACV6 = W-OFF");
        
    } else {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B):!STR-OP\n");
        
        // No
        // C(TPR.TRR) > C(SDW .R2)?
        if (TPR.TRR > SDW->R2) {
            //Set fault ACV3 = ORB
            acvFaults |= ACV3;
            acvFaultsMsg = "acvFaults(B) C(TPR.TRR) > C(SDW .R2)";
        }
        
        if (SDW->R == 0)
        {
            //C(PPR.PSR) = C(TPR.TSR)?
            if (PPR.PSR != TPR.TSR) {
                //Set fault ACV4 = R-OFF
                acvFaults |= ACV4;
                acvFaultsMsg = "acvFaults(B) C(PPR.PSR) = C(TPR.TSR)";
            }
        }
        
    }
    goto G;
    
D:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(D)\n");
    
    // transfer or instruction fetch

    // AL39, pg 31, RING ALARM REGISTER:
    // "...and the instruction for which an absolute main memory address is 
    //  being prepared is a transfer instruction..."
    if (instructionFetch)
      goto G;

    if (rRALR == 0)
        goto G;
    
    // C(PPR.PRR) < RALR?
    if (!(PPR.PRR < rRALR)) {
        sim_debug (DBG_APPENDING, & cpu_dev,
                   "acvFaults(D) C(PPR.PRR) %o < RALR %o\n", PPR . PRR, rRALR);
        acvFaults |= ACV13;
        acvFaultsMsg = "acvFaults(D) C(PPR.PRR) < RALR";
    }
    
    goto G;
    
E:;

//
// E: CALL6
//

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(E): CALL6\n");

    //SDW .E set ON?
    if (!SDW->E) {
        // Set fault ACV2 = E-OFF
        acvFaults |= ACV2;
        acvFaultsMsg = "acvFaults(E) SDW .E set OFF";
    }
    
    //SDW .G set ON?
    if (SDW->G)
        goto E1;
    
    // C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR == TPR.TSR)
        goto E1;
    
    // XXX This doesn't seem right
    // TPR.CA4-17 ≥ SDW.CL?
    //if ((TPR.CA & 0037777) >= SDW->CL)
    if ((address & 0037777) >= SDW->CL) {
        // Set fault ACV7 = NO GA
        acvFaults |= ACV7;
        acvFaultsMsg = "acvFaults(E) TPR.CA4-17 ≥ SDW.CL";
    }
    
E1:
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(E1): CALL6 (cont'd)\n");

    // C(TPR.TRR) > SDW.R3?
    if (TPR.TRR > SDW->R3) {
        //Set fault ACV8 = OCB
        acvFaults |= ACV8;
        acvFaultsMsg = "acvFaults(E1) C(TPR.TRR) > SDW.R3";
    }
    
    // C(TPR.TRR) < SDW.R1?
    if (TPR.TRR < SDW->R1) {
        // Set fault ACV9 = OCALL
        acvFaults |= ACV9;
        acvFaultsMsg = "acvFaults(E1) C(TPR.TRR) < SDW.R1";
    }
    
    
    // C(TPR.TRR) > C(PPR.PRR)?
    if (TPR.TRR > PPR.PRR)
        // C(PPR.PRR) < SDW.R2?
        if (PPR.PRR < SDW->R2) {
            // Set fault ACV10 = BOC
            acvFaults |= ACV10;
            acvFaultsMsg = "acvFaults(E1) C(TPR.TRR) > C(PPR.PRR) && C(PPR.PRR) < SDW.R2";
        }
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(E1): CALL6 TPR.TRR %o SDW->R2 %o\n", TPR . TRR, SDW -> R2);
    // C(TPR.TRR) > SDW.R2?
    if (TPR.TRR > SDW->R2)
        // ￼SDW.R2 → C(TPR.TRR)
        TPR.TRR = SDW->R2;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(E1): CALL6 TPR.TRR %o\n", TPR . TRR);
    
    goto G;
    
F:;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(F): transfer or instruction fetch\n");

    // C(TPR.TRR) < C(SDW .R1)?
    if (TPR.TRR < SDW->R1) {
        sim_debug (DBG_APPENDING, & cpu_dev,
                   "acvFaults(F) C(TPR.TRR) %o < C(SDW .R1) %o\n", TPR . TRR, SDW -> R1);
        acvFaults |= ACV1;
        acvFaultsMsg = "acvFaults(F) C(TPR.TRR) < C(SDW .R1)";
    }
    
    //C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2) {
        sim_debug (DBG_TRACE, & cpu_dev,
                   "acvFaults(F) C(TPR.TRR) %o > C(SDW .R2) %o\n", TPR . TRR, SDW -> R2);
        acvFaults |= ACV1;
        acvFaultsMsg = "acvFaults(F) C(TPR.TRR) > C(SDW .R2)";
    }
    
    //SDW .E set ON?
    if (!SDW->E) {
        acvFaults |= ACV2;
        acvFaultsMsg = "acvFaults(F) SDW .E set OFF";
    }
    
    //C(PPR.PRR) = C(TPR.TRR)?
    if (PPR.PRR != TPR.TRR) {
        //Set fault ACV12 = CRT
        acvFaults |= ACV12;
        acvFaultsMsg = "acvFaults(F) C(PPR.PRR) != C(TPR.TRR)";
    }
    
    goto D;

G:;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(G)\n");
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    //if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
    if (((address >> 4) & 037777) > SDW->BOUND) {
        acvFaults |= ACV15;
        acvFaultsMsg = "acvFaults(G) C(TPR.CA)0,13 > SDW.BOUND";
        sim_debug (DBG_FAULT, & cpu_dev, "acvFaults(G) C(TPR.CA)0,13 > SDW.BOUND\n    address %06o address>>4&037777 %06o SDW->BOUND %06o",
                    address, ((address >> 4) & 037777), SDW->BOUND);
    }
    
    if (acvFaults)
        // Initiate an access violation fault
        doFault(FAULT_ACV, acvFaults, acvFaultsMsg);
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
        goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
#ifdef SPEED
    fetchPTW(SDW, address);
    if (!PTW0.F)
    {
        //TPR.CA = address;
        // initiate a directed fault
        doFault(FAULT_DF0 + PTW0.FC, 0, "PTW0.F == 0");
    }
    loadPTWAM(SDW->POINTER, address);    // load PTW0 to PTWAM

#else
    if (!fetchPTWfromPTWAM(SDW->POINTER, address))  //TPR.CA))
    {
        appendingUnitCycleType = apuCycle_PTWfetch;
        //fetchPTW(SDW, TPR.CA);
        fetchPTW(SDW, address);
        if (!PTW0.F)
        {
            //TPR.CA = address;
            // initiate a directed fault
            doFault(FAULT_DF0 + PTW0.FC, 0, "PTW0.F == 0");
        }

        //loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
        loadPTWAM(SDW->POINTER, address);    // load PTW0 to PTWAM
    }
#endif
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    // XXX: ticket #11
    // The MVT, TCT, TCTR, and CMPCT instruction have a prepage check. The size of the translate table is determined by the TA1 data type as shown in the table below. Before the instruction is executed, a check is made for allocation in memory for the page for the translate table. If the page is not in memory, a Missing Page fault occurs before execution of the instruction. (cf. Bull, RJ78, p.7-75, sec 7.14.15)
#if 0
    if (bPrePageMode)
    {
        //Is PTW.F set ON?
       if (!PTW0.F)
          // initiate a directed fault
         doFault(FAULT_DF0 + PTW0.FC, 0, "PTW0.F == 0");
        
    }
    bPrePageMode = false;   // done with this
#endif
    goto I;
    
H:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H): FANP\n");
    appendingUnitCycleType = apuCycle_FANP;
    setAPUStatus (apuStatus_FANP);

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, address);

    finalAddress = SDW->ADDR + address;
    finalAddress &= 0xffffff;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H:FANP): (%05o:%06o) finalAddress=%08o\n",TPR.TSR, address, finalAddress);
    
    goto HI;
    
I:;

// Set PTW.M

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(I)\n");
    //if (isSTROP(i) && PTW->M == 0)
    //if (thisCycle == OPERAND_STORE && PTW->M == 0)  // is this the right way to do this?
    if (StrOp && PTW->M == 0)  // is this the right way to do this?
    {
#if 0
        // Modify PTW -  Sets the page modified bit (PTW.M) in the PTW for a page in other than a descriptor segment page table.
        appendingUnitCycleType = MPTW;
        PTW->M = 1;
        
#else
       modifyPTW(SDW, address);
#endif
    }
    
    // final address paged
    appendingUnitCycleType = apuCycle_FAP;
    setAPUStatus (apuStatus_FAP);
    
    //word24 y2 = TPR.CA % 1024;
    word24 y2 = address % 1024;
    
    finalAddress = ((PTW->ADDR & 0777777) << 6) + y2;
    finalAddress &= 0xffffff;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H:FAP): (%05o:%06o) finalAddress=%08o\n",TPR.TSR, address, finalAddress);
    goto HI;

HI:

// Check for conditions that change the PPR.P bit

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(HI)\n");
    
    //if (thisCycle == INSTRUCTION_FETCH)
    if (thisCycle == INDIRECT_WORD_FETCH)
        goto Exit;
    
    if (thisCycle == RTCD_OPERAND_FETCH)
        goto KL;
    
    //if (i && ((i->info->flags & TRANSFER_INS) || instructionFetch))
    if (instructionFetch || (i && (i->info->flags & TRANSFER_INS)))
        goto KL;
    
// XXX "APU data movement; Load/store APU data" not implemented"
// XXX ticket 12
    // load/store data .....

    
    goto Exit;
 
KL:;

// We end up here if the operand data is destined for the IC.
//  Set PPR.P

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(KLM)\n");
    
    if (TPR.TRR == 0)
        // C(SDW.P) → C(PPR.P)
        PPR.P = SDW->P;
    else
        // 0 → C(PPR.P)
        PPR.P = 0;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(KLM) TPR.TRR %o SDW.P %o PPR.P %o\n", TPR.TRR, SDW->P, PPR.P);
    
    goto Exit;    // this may not be setup or right
    

    
    
   
Exit:;
//    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Exit): lastCycle: %s => %s\n", strPCT(lastCycle), strPCT(thisCycle));

    TPR . CA = address;
    return finalAddress;    // or 0 or -1???
}

// Translate a segno:offset to a absolute address.
// Return 0 if successful.

int dbgLookupAddress (word18 segno, word18 offset, word24 * finalAddress,
                      char * * msg)
  {
    // Local copies so we don't disturb machine state

    struct _ptw0 PTW1;
    struct _sdw0 SDW1;

   if (2u * segno >= 16u * (DSBR.BND + 1u))
     {
       if (msg)
         * msg = "DSBR boundary violation.";
       return 1;
     }

    if (DSBR . U == 0)
      {
        // fetchDSPTW

        word24 y1 = (2 * segno) % 1024;
        word24 x1 = (2 * segno - y1) / 1024;

        word36 PTWx1;
        core_read ((DSBR . ADDR + x1) & PAMASK, & PTWx1, __func__);
        
        PTW1 . ADDR = GETHI (PTWx1);
        PTW1 . U = TSTBIT (PTWx1, 9);
        PTW1 . M = TSTBIT (PTWx1, 6);
        PTW1 . F = TSTBIT (PTWx1, 2);
        PTW1 . FC = PTWx1 & 3;
    
        if (! PTW1 . F)
          {
            if (msg)
              * msg = "!PTW0.F";
            return 2;
          }

        // fetchPSDW

        y1 = (2 * segno) % 1024;
    
        word36 SDWeven, SDWodd;
    
        core_read2 (((PTW1 .  ADDR << 6) + y1) & PAMASK, & SDWeven, & SDWodd, __func__);
    
        // even word
        SDW1 . ADDR = (SDWeven >> 12) & 077777777;
        SDW1 . R1 = (SDWeven >> 9) & 7;
        SDW1 . R2 = (SDWeven >> 6) & 7;
        SDW1 . R3 = (SDWeven >> 3) & 7;
        SDW1 . F = TSTBIT(SDWeven, 2);
        SDW1 . FC = SDWeven & 3;
    
        // odd word
        SDW1 . BOUND = (SDWodd >> 21) & 037777;
        SDW1 . R = TSTBIT (SDWodd, 20);
        SDW1 . E = TSTBIT (SDWodd, 19);
        SDW1 . W = TSTBIT (SDWodd, 18);
        SDW1 . P = TSTBIT (SDWodd, 17);
        SDW1 . U = TSTBIT (SDWodd, 16);
        SDW1 . G = TSTBIT (SDWodd, 15);
        SDW1 . C = TSTBIT (SDWodd, 14);
        SDW1 . EB = SDWodd & 037777;
      }
    else // ! DSBR . U
      {
        // fetchNSDW

        word36 SDWeven, SDWodd;
        
        core_read2 ((DSBR . ADDR + 2 * segno) & PAMASK, & SDWeven, & SDWodd, __func__);
        
        // even word
        SDW1 . ADDR = (SDWeven >> 12) & 077777777;
        SDW1 . R1 = (SDWeven >> 9) & 7;
        SDW1 . R2 = (SDWeven >> 6) & 7;
        SDW1 . R3 = (SDWeven >> 3) & 7;
        SDW1 . F = TSTBIT (SDWeven, 2);
        SDW1 . FC = SDWeven & 3;
        
        // odd word
        SDW1 . BOUND = (SDWodd >> 21) & 037777;
        SDW1 . R = TSTBIT(SDWodd, 20);
        SDW1 . E = TSTBIT(SDWodd, 19);
        SDW1 . W = TSTBIT(SDWodd, 18);
        SDW1 . P = TSTBIT(SDWodd, 17);
        SDW1 . U = TSTBIT(SDWodd, 16);
        SDW1 . G = TSTBIT(SDWodd, 15);
        SDW1 . C = TSTBIT(SDWodd, 14);
        SDW1 . EB = SDWodd & 037777;
    
      }

    if (SDW1 . F == 0)
      {
        if (msg)
          * msg = "!SDW0.F != 0";
        return 3;
      }

    if (((offset >> 4) & 037777) > SDW1 . BOUND)
      {
        if (msg)
          * msg = "C(TPR.CA)0,13 > SDW.BOUND";
        return 4;
      }

    // is segment C(TPR.TSR) paged?
    if (SDW1 . U)
      {
        * finalAddress = (SDW1 . ADDR + offset) & PAMASK;
      }
    else
      {
        // fetchPTW
        word24 y2 = offset % 1024;
        word24 x2 = (offset - y2) / 1024;
    
        word36 PTWx2;
    
        core_read ((SDW1 . ADDR + x2) & PAMASK, & PTWx2, __func__);
    
        PTW1 . ADDR = GETHI (PTWx2);
        PTW1 . U = TSTBIT (PTWx2, 9);
        PTW1 . M = TSTBIT (PTWx2, 6);
        PTW1 . F = TSTBIT (PTWx2, 2);
        PTW1 . FC = PTWx2 & 3;

        if ( !PTW1 . F)
          {
            if (msg)
              * msg = "!PTW0.F";
            return 5;
          }

        y2 = offset % 1024;
    
        * finalAddress = (((PTW1 . ADDR & 0777777) << 6) + y2) & PAMASK;
      }
    if (msg)
      * msg = "";
    return 0;
  }



