/**
 * \file dps8_append.c
 * \project dps8
 * \date 10/28/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include "dps8.h"

/**
 * \brief the appending unit ...
 */
bool apndTrace = false;     ///< when true do appending unit tracing

static enum _appendingUnit_cycle_type appendingUnitCycleType = APPUNKNOWN;

word24 finalAddress = 0;    ///< final, 24-bit address that comes out of the APU
word36 CY = 0;              ///< C(Y) operand data from memory
                            // XXX do we need to make CY part of DCDstruct ?
//word36 CY1 = 0;             ///< C(Y+1) operand data .....
//word36 YPair[2];            ///< Ypair


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

#if REDUNDANT
void doAddrModPtrReg(DCDstruct *i)
{
    word3 n = GET_PRN(i->IWB);  // get PRn
    word15 offset = GET_OFFSET(i->IWB);
#ifndef QUIET_UNUSED
    int soffset = SIGNEXT15(GET_OFFSET(i->IWB));
#endif
    
    TPR.TSR = PR[n].SNR;
    TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
    
    TPR.CA = (PR[n].WORDNO + SIGNEXT15(offset)) & 0777777;
    TPR.TBR = PR[n].BITNO;  // TPR.BITNO = PR[n].BITNO;
    
    //i->address = TPR.CA;    // why do I muck with i->address?
    //rY = i->address;    // is this right?
    
    //rY = TPR.CA;    // why do I muck with i->address?
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAddrModPtrReg(): n=%o offset=%05o TPR.CA=%06o TPR.TBR=%o TPR.TSR=%05o TPR.TRR=%o\n", n, offset, TPR.CA, TPR.TBR, TPR.TSR, TPR.TRR);
    }
}
#endif

void doPtrReg(DCDstruct *i)
{
    word3 n = GET_PRN(i->IWB);  // get PRn
    word15 offset = GET_OFFSET(i->IWB);
    
    TPR.TSR = PAR[n].SNR;
    TPR.TRR = max3(PAR[n].RNR, TPR.TRR, PPR.PRR);
    
    TPR.CA = (PAR[n].WORDNO + SIGNEXT15(offset)) & 0777777;
    TPR.TBR = PAR[n].BITNO;
    
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
  }

/**
 * implement camp instruction
 */

void do_camp (word36 Y)
  {
    // C(TPR.CA) 16,17 control disabling or enabling the associative memory.
    // This may be done to either or both halves.
    // The full/empty bit of cache PTWAM register is set to zero and the LRU
    // counters are initialized.
    // XXX enable/disable and LRU don't seem to be implemented; punt
    for (int i = 0; i < 64; i ++)
      {
        PTWAM [i] . F = 0;
      }
    sim_debug (DBG_ERR, & cpu_dev, "do_camp: punt\n");
  }

/**
 * implement cams instruction
 */

void do_cams (word36 Y)
  {
    // The full/empty bit of each SDWAM register is set to zero and the LRU
    // counters are initialized. The remainder of the contents of the registers
    // are unchanged. If the associative memory is disabled, F and LRU are
    // unchanged.
    // C(TPR.CA) 16,17 control disabling or enabling the associative memory.
    // This may be done to either or both halves.
    // XXX enable/disable and LRU don't seem to be implemented; punt
    for (int i = 0; i < 64; i ++)
      {
        SDWAM [i] . F = 0;
      }
    sim_debug (DBG_ERR, & cpu_dev, "do_cams: punt\n");
  }

    
/**
 * fetch descriptor segment PTW ...
 */
static _ptw0* fetchDSPTW(word15 segno)
{
    sim_debug (DBG_APPENDING, & cpu_dev, "fetchDSPTW segno 0%o\n", segno);
    if (2 * segno >= 16 * (DSBR.BND + 1))
        // generate access violation, out of segment bounds fault
        doFault(NULL /* XXX */, acc_viol_fault, ACV15, "acvFault: fetchDSPTW out of segment bounds fault");
        
    word24 y1 = (2 * segno) % 1024;
    word24 x1 = (2 * segno - y1) / 1024;

    word36 PTWx1;
    core_read(DSBR.ADDR + x1, &PTWx1);
    
    PTW0.ADDR = GETHI(PTWx1);
    PTW0.U = TSTBIT(PTWx1, 9);
    PTW0.M = TSTBIT(PTWx1, 6);
    PTW0.F = TSTBIT(PTWx1, 2);
    PTW0.FC = PTWx1 & 3;
    
    sim_debug (DBG_APPENDING, & cpu_dev, "fetchDSPTW x1 0%o y1 0%o DSBR.ADDR 0%o PTWx1 0%012llo PTW0: ADDR 0%o U %o M %o F %o FC %o\n", x1, y1, DSBR.ADDR, PTWx1, PTW0.ADDR, PTW0.U, PTW0.M, PTW0.F, PTW0.FC);
    return &PTW0;
}
/**
 * modify descriptor segment PTW (Set U=1) ...
 */
static _ptw0* modifyDSPTW(word15 segno)
{
    if (2 * segno >= 16 * (DSBR.BND + 1))
        // generate access violation, out of segment bounds fault
        doFault(NULL /* XXX */, acc_viol_fault, ACV15, "acvFault: modifyDSPTW out of segment bounds fault");
    
    word24 y1 = (2 * segno) % 1024;
    word24 x1 = (2 * segno - y1) / 1024;
    
    word36 PTWx1;
    core_read(DSBR.ADDR + x1, &PTWx1);
    PTWx1 = SETBIT(PTWx1, 9);
    core_write(DSBR.ADDR + x1, PTWx1);
    
    PTW0.U = 1;
    
    return &PTW0;
}


/// \brief XXX SDW0 is the in-core representation of a SDW. Need to have a SDWAM struct as current SDW!!!
static _sdw* fetchSDWfromSDWAM(DCDstruct *i, word15 segno)
{
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(0):segno=%05o\n", segno);
    }
    
    if (switches . disable_wam)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(0): SDWAM disabled\n");
        return NULL;
    }
    
#if 0
    if (switches . degenerate_mode && (! i -> a) && (get_addr_mode () == ABSOLUTE_mode))
      {
        sim_debug (DBG_APPENDING, & cpu_dev, "fetchSDWfromSDWAM: degenerate case\n");
        static _sdw degenerate_SDW =
          {
            0, // ADDR;
            0, // R1;
            0, // R2;
            0, // R3;
            037777, // BOUND;
            1, // R
            1, // E
            1, // W
            1, // P
            1, // U
            1, // G
            1, // C,
            037777, // CL
            0, // POINTER
            1, // F
            0, // USE
          };
        SDW = & degenerate_SDW;
        return SDW;
      }
#endif

    for(int _n = 0 ; _n < 64 ; _n++)
    {
        // make certain we initialize SDWAM prior to use!!!
        //if (SDWAM[_n]._initialized && segno == SDWAM[_n].POINTER)
        if (SDWAM[_n].F && segno == SDWAM[_n].POINTER)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(1):found match for segno %05o at _n=%d\n", segno, _n);
            }
            
            SDW = &SDWAM[_n];
            
            /*
             If the SDWAM match logic circuitry indicates a hit, all usage counts (SDWAM.USE) greater than the usage count of the register hit are decremented by one, the usage count of the register hit is set to 15 (63?), and the contents of the register hit are read out into the address preparation circuitry. 
             */
            for(int _h = 0 ; _h < 64 ; _h++)
            {
                if (SDWAM[_h].USE > SDW->USE)
                    SDWAM[_h].USE -= 1;
            }
            SDW->USE = 63;
            
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(2):SDWAM[%d]=%s\n", _n, strSDW(SDW));
            }
            return SDW;
        }
    }
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(3):SDW for segment %05o not found in SDWAM\n", segno);
    }
    return NULL;    // segment not referenced in SDWAM
}

/**
 * Fetches an SDW from a paged descriptor segment.
 */
static _sdw0* fetchPSDW(word15 segno)
{
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchPSDW(0):segno=%05o\n", segno);
    }
    
    _ptw0 *p = fetchDSPTW(segno); // XXX [CAC] is this redundant??

    //if (apndTrace)
    //    sim_debug(DBG_APPENDING, &cpu_dev, "fetchPSDW(1):PTW:%s\n",

    word24 y1 = (2 * segno) % 1024;
    
    word36 SDWeven, SDWodd;
    
    core_read2((p->ADDR << 6) + y1, &SDWeven, &SDWodd);
    
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
    
    PPR.P = (SDW0.P && PPR.PRR == 0);   // set priv bit (if OK)

    sim_debug (DBG_APPENDING, & cpu_dev, "fetchPSDW y1 0%o p->ADDR 0%o SDW 0%012llo 0%012llo ADDR 0%o BOUND 0%o U %o F %o\n",
 y1, p->ADDR, SDWeven, SDWodd, SDW0.ADDR, SDW0.BOUND, SDW0.U, SDW0.F);
    return &SDW0;
}

/// \brief Nonpaged SDW Fetch
/// Fetches an SDW from an unpaged descriptor segment.
static _sdw0 *fetchNSDW(word15 segno)
{
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(0):segno=%05o\n", segno);
    }
    if (2 * segno >= 16 * (DSBR.BND + 1))
    {
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(1):Access Violation, out of segment bounds for segno=%05o DSBR.BND=%d\n", segno, DSBR.BND);
        }
        // generate access violation, out of segment bounds fault
        doFault(NULL /* XXX */, acc_viol_fault, ACV15, "acvFault fetchNSDW: out of segment bounds fault");
    }
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(2):fetching SDW from %05o\n", DSBR.ADDR + 2 * segno);
    }
    word36 SDWeven, SDWodd;
    
    core_read2(DSBR.ADDR + 2 * segno, &SDWeven, &SDWodd);
    
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
    
    PPR.P = (SDW0.P && PPR.PRR == 0);   // set priv bit (if OK)
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(2):SDW0=%s\n", strSDW0(&SDW0));
    }
    return &SDW0;
}

char *strSDW(_sdw *SDW)
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
            sim_printf("SDWAM n:%d %s\n\r", _n, strSDW(p));
    }
    return SCPE_OK;
}


/**
 * load the current in-core SDW0 into the SDWAM ...
 */
static void loadSDWAM(word15 segno)
{
    if (switches . disable_wam)
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
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(1):SDWAM[%d] p->USE=0\n", _n);
            }
            
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
            
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(2):SDWAM[%d]=%s\n", _n, strSDW(p));
            }
            
            return;
        }
    }
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(3) no USE=0 found for segment=%d\n", segno);
    }
    
    sim_printf("loadSDWAM(%05o): no USE=0 found!\n", segno);
    dumpSDWAM();
}

static _ptw* fetchPTWfromPTWAM(word15 segno, word18 CA)
{
    if (switches . disable_wam)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchPTWfromPTWAM: PTWAM disabled\n");
        return NULL;
    }
    
    for(int _n = 0 ; _n < 64 ; _n++)
    {
        if (((CA >> 10) & 0377) == ((PTWAM[_n].PAGENO >> 4) & 0377) && PTWAM[_n].POINTER == segno && PTWAM[_n].F)   //_initialized)
        {
            PTW = &PTWAM[_n];
            
            /*
             * If the PTWAM match logic circuitry indicates a hit, all usage counts (PTWAM.USE) greater than the usage count of the register hit are decremented by one, the usage count of the register hit is set to 15 (63?), and the contents of the register hit are read out into the address preparation circuitry.
             */
            for(int _h = 0 ; _h < 64 ; _h++)
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

static _ptw0* fetchPTW(_sdw *sdw, word18 offset)
{
    word24 y2 = offset % 1024;
    word24 x2 = (offset - y2) / 1024;
    
    word36 PTWx2;
    
    core_read(sdw->ADDR + x2, &PTWx2);
    
    PTW0.ADDR = GETHI(PTWx2);
    PTW0.U = TSTBIT(PTWx2, 9);
    PTW0.M = TSTBIT(PTWx2, 6);
    PTW0.F = TSTBIT(PTWx2, 2);
    PTW0.FC = PTWx2 & 3;
    
    sim_debug (DBG_APPENDING, & cpu_dev, "fetchPTW x2 0%o y2 0%o sdw->ADDR 0%o PTWx2 0%012llo PTW0: ADDR 0%o U %o M %o F %o FC %o\n", x2, y2, sdw->ADDR, PTWx2, PTW0.ADDR, PTW0.U, PTW0.M, PTW0.F, PTW0.FC);
    return &PTW0;
}

static void loadPTWAM(word15 segno, word18 offset)
{
    if (switches . disable_wam)
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

}

#ifndef QUIET_UNUSED
/**
 * modify target segment PTW (Set M=1) ...
 */
static _ptw* modifyPTW(_sdw *sdw, word18 offset)
{
    word24 y2 = offset % 1024;
    word24 x2 = (offset - y2) / 1024;
    
    word36 PTWx2;
    
    core_read(sdw->ADDR + x2, &PTWx2);
    PTWx2 = SETBIT(PTWx2, 6);
    core_write(sdw->ADDR + x2, PTWx2);
    
    PTW->M = 1;
    
    return PTW;
}
#endif



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
        case AME:   return "Associative memory error XXX ??";
        case ACV15: return "Out of segment bounds (ACV15=OOSB)";
        case ACDF0: return "Directed fault 0";
        case ACDF1: return "Directed fault 1";
        case ACDF2: return "Directed fault 2";
        case ACDF3: return "Directed fault 3";
        default:
            break;
    }
  return "unhandled acv in strACV";
}

static int acvFaults = 0;   ///< pending ACV faults

void acvFault(DCDstruct *i, _fault_subtype acvfault, char * msg)
{
    
    char temp[256];
    sprintf(temp, "group 6 ACV fault %s(%d): %s\n", strACV(acvfault), acvfault, msg);

    sim_printf("%s", temp);
    
    //acvFaults |= (1 << acvfault);   // or 'em all together
    acvFaults |= acvfault;   // or 'em all together

    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(acvFault): acvFault=%s(%ld) acvFaults=%d: %s\n", strACV(acvfault), (long)acvFault, acvFaults, msg);
    }
    
    doFault(i, acc_viol_fault, acvfault, temp); // NEW HWR 17 Dec 2013
}

/*
 extern enum _processor_cycle_type {
 UNKNOWN_CYCLE = 0,
 APPEND_CYCLE,
 CA_CYCLE,
 OPERAND_STORE,
 DIVIDE_EXECUTION,
 FAULT,
 INDIRECT_WORD_FETCH,
 RTCD_OPERAND_FETCH,
 SEQUENTIAL_INSTRUCTION_FETCH,
 INSTRUCTION_FETCH,
 APU_DATA_MOVEMENT,
 ABORT_CYCLE,
 FAULT_CYCLE
 } processorCycle;
 typedef enum _processor_cycle_type _processor_cycle_type;
*/

char *strPCT(_processor_cycle_type t)
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

//// is instruction a STR-OP?
//static bool isSTROP(DCDstruct *i)
//{
//    if (i->info->flags & (STORE_OPERAND | STORE_YBLOCK8 | STORE_YBLOCK16))
//        return true;
//    return false;
//}
//
//// is instruction a read-OP?
//static bool isREADOP(DCDstruct *i)
//{
//    if (i->info->flags & (READ_OPERAND | READ_YBLOCK8 | READ_YBLOCK16))
//        return true;
//    return false;
//}


//_processor_cycle_type lastCycle = UNKNOWN_CYCLE;

bool bPrePageMode = false;

/*
 * recoding APU functions to more closely match Fig 5,6 & 8 ...
 * Returns final address suitable for core_read/write
 */

word24
doAppendCycle(DCDstruct *i, word18 address, _processor_cycle_type thisCycle)
{
//    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) lastCycle=%s, thisCycle=%s\n", strPCT(lastCycle), strPCT(thisCycle));
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) thisCycle=%s\n", strPCT(thisCycle));
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) PPR.PRR=%o PPR.PSR=%05o\n", PPR.PRR, PPR.PSR);
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) TPR.TRR=%o TPR.TSR=%05o\n", TPR.TRR, TPR.TSR);

    //bool instructionFetch = (thisCycle == INSTRUCTION_FETCH) || (thisCycle == SEQUENTIAL_INSTRUCTION_FETCH);
    bool instructionFetch = (thisCycle == INSTRUCTION_FETCH);
    bool StrOp = (thisCycle == OPERAND_STORE || thisCycle == EIS_OPERAND_STORE);
    
    int n = 0;  // # of PR
    int RSDWH_R1 = 0;
    
    acvFaults = 0;
    char * acvFaultsMsg = "<unknown>";

    
    finalAddress = -1;  // not everything requires a final address
    
//    if (thisCycle == EIS_OPERAND_READ || thisCycle == EIS_OPERAND_STORE)
//    {
//        // TPR already setup properly
//        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(EIS) TPR.TRR=%o TPR.TSR=%05o\n", TPR.TRR, TPR.TSR);
//        goto A;
//    }
    
//    if (lastCycle == INDIRECT_WORD_FETCH)
//        goto A;
//    
//    if (lastCycle == RTCD_OPERAND_FETCH)
//        goto A;
    
    //if (lastCycle == SEQUENTIAL_INSTRUCTION_FETCH || instructionFetch)
//    if (instructionFetch)
//    {
//        if (i && i->a)   // bit 29 on?
//        {
//              sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(1): bit-29 (a) detected\n");
//              sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(2) TPR.TRR=%o TPR.TSR=%05o\n", TPR.TRR, TPR.TSR);
//            goto A;
//        }
//        
//        TPR.TRR = PPR.PRR;
//        TPR.TSR = PPR.PSR;
//        
//        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(3) instructionfetch - TPR.TRR=%o TPR.TSR=%05o\n", TPR.TRR, TPR.TSR);
//    }

#ifndef QUIET_UNUSED
A:;
#endif
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A)\n");
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A):DSBR.U=%o\n", DSBR.U);
        
        if (DSBR.U == 0)
        {
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                doFault(i, dir_flt0_fault + PTW0.FC, 0, "doAppendCycle(A): PTW0.F == 0");
            
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
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            
            return -1;
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
    // Yes...
    RSDWH_R1 = SDW->R1;

    goto B;
    
B:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B)\n");
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(i, ACV0, "doAppendCycle(B) C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)");
    
    // No
    
    // Was last cycle an rtcd operand fetch?
//    if (lastCycle == RTCD_OPERAND_FETCH)
//        goto C;
    
    // Is OPCODE call6?
    //if (!instructionFetch && i->info->flags & CALL6_INS)
    if (thisCycle == OPERAND_READ && i->info->flags & CALL6_INS)
        goto E;
    
    // Transfer or instruction fetch?
    if (instructionFetch || (thisCycle == OPERAND_READ && (i->info && i->info->flags & TRANSFER_INS)))
        goto F;
    
    //if (isSTROP(i))
    //if (thisCycle == STORE_OPERAND) // is this the right way to do this?
    if (StrOp)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B):STR-OP\n");
        
        // C(TPR.TRR) > C(SDW .R2)?
        if (TPR.TRR > SDW->R2)
            //Set fault ACV5 = OWB
            acvFault(i, ACV5, "doAppendCycle(B) C(TPR.TRR) > C(SDW .R2)");
        
        if (!SDW->W)
            // Set fault ACV6 = W-OFF
            acvFault(i, ACV6, "doAppendCycle(B) ACV6 = W-OFF");
        goto G;
        
    } else {
        // XXX should we test for READOP() here?
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B):!STR-OP\n");
        
        // No
        // C(TPR.TRR) > C(SDW .R2)?
        if (TPR.TRR > SDW->R2) {
            //Set fault ACV3 = ORB
            acvFaults |= ACV3;
            acvFaultsMsg = "acvFaults(B) C(TPR.TRR) > C(SDW .R2)";
        }
        
        if (SDW->R)
            goto G;
        
        //C(PPR.PSR) = C(TPR.TSR)?
        if (PPR.PSR != TPR.TSR) {
            //Set fault ACV4 = R-OFF
            acvFaults |= ACV4;
            acvFaultsMsg = "acvFaults(B) C(PPR.PSR) = C(TPR.TSR)";
        }
        
        goto G;
    }
    
#ifndef QUIET_UNUSED
C:;
#endif
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(C)\n");

    //C(TPR.TRR) < C(SDW .R1)?
    if (TPR.TRR < SDW->R1) {
        //Set fault ACV1 = OEB
        acvFaults |= ACV1;
        acvFaultsMsg = "acvFaults(C) C(TPR.TRR) < C(SDW .R1)";
    }

    //C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2) {
        //Set fault ACV1 = OEB
        acvFaults |= ACV1;
        acvFaultsMsg = "acvFaults(C) C(TPR.TRR) > C(SDW .R2)";
    }

    // SDW .E set ON?
    if (SDW->E) {
        //Set fault ACV2 = E-OFF
        acvFaults |= ACV2;
        acvFaultsMsg = "acvFaults(C) SDW .E set ON";
    }
    
    //C(TPR.TRR) ≥ C(PPR.PRR)
    if (!(TPR.TRR >= PPR.PRR)) {
        // Set fault ACV11 = INRET
        acvFaults |= ACV11;
        acvFaultsMsg = "acvFaults(C) C(TPR.TRR) ≥ C(PPR.PRR)";
    }
    
D:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(D)\n");
    
    // instruction fetch
    if (rRALR == 0)
        goto G;
    
    // C(PPR.PRR) < RALR?
    if (!(PPR.PRR < rRALR)) {
        acvFaults |= ACV13;
        acvFaultsMsg = "acvFaults(D) C(PPR.PRR) < RALR";
    }
    
    goto G;
    
E:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(E): CALL6\n");

    //SDW .E set ON?
    if (!SDW->E) {
        // Set fault ACV2 = E-OFF
        acvFaults |= ACV2;
        acvFaultsMsg = "acvFaults(E) SDW .E set ON";
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
    
    // C(TPR.TRR) > SDW.R2?
    if (TPR.TRR > SDW->R2)
        // ￼SDW.R2 → C(TPR.TRR)
        TPR.TRR = SDW->R2;
    
    goto G;
    
F:;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(F): transfer or instruction fetch\n");

    // C(TPR.TRR) < C(SDW .R1)?
    if (TPR.TRR < SDW->R1) {
        acvFaults |= ACV1;
        acvFaultsMsg = "acvFaults(F) C(TPR.TRR) < C(SDW .R1)";
    }
    
    //C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2) {
        acvFaults |= ACV1;
        acvFaultsMsg = "acvFaults(F) C(TPR.TRR) > C(SDW .R2)";
    }
    
    //SDW .E set ON?
    if (!SDW->E) {
        acvFaults |= ACV2;
        acvFaultsMsg = "acvFaults(F) SDW .E set ON";
    }
    
    //C(PPR.PRR) = C(TPR.TRR)?
    if (PPR.PRR != TPR.TRR) {
        //Set fault ACV12 = CRT
        acvFaults |= ACV12;
        acvFaultsMsg = "acvFaults(F) C(PPR.PRR) = C(TPR.TRR)";
    }
    
    goto D;

G:;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(G)\n");
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    //if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
    if (((address >> 4) & 037777) > SDW->BOUND) {
        acvFaults |= ACV15;
        acvFaultsMsg = "acvFaults(G) C(TPR.CA)0,13 > SDW.BOUND";
    }
    
    if (acvFaults)
        // Initiate an access violation fault
        doFault(i, acc_viol_fault, acvFaults, acvFaultsMsg);
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
        goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, address))  //TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        //fetchPTW(SDW, TPR.CA);
        fetchPTW(SDW, address);
        if (!PTW0.F)
            // initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F == 0");
        
        //loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
        loadPTWAM(SDW->POINTER, address);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    // The MVT, TCT, TCTR, and CMPCT instruction have a prepage check. The size of the translate table is determined by the TA1 data type as shown in the table below. Before the instruction is executed, a check is made for allocation in memory for the page for the translate table. If the page is not in memory, a Missing Page fault occurs before execution of the instruction. (cf. Bull, RJ78, p.7-75, sec 7.14.15)
    if (bPrePageMode)
    {
        //Is PTW.F set ON?
       if (!PTW0.F)
          // initiate a directed fault
         doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F == 0");
        
    }
    bPrePageMode = false;   // done with this
    goto I;
    
H:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H): FANP\n");
    appendingUnitCycleType = FANP;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, address);

    //finalAddress = SDW->ADDR + TPR.CA;
    finalAddress = SDW->ADDR + address;
    finalAddress &= 0xffffff;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H:FANP): (%05o:%06o) finalAddress=%08o\n",TPR.TSR, address, finalAddress);
    
    goto HI;
    
I:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(I)\n");
    //if (isSTROP(i) && PTW->M == 0)
    if (thisCycle == STORE_OPERAND && PTW->M == 0)  // is this the right way to do this?
    {
        // Modify PTW -  Sets the page modified bit (PTW.M) in the PTW for a page in other than a descriptor segment page table.
        appendingUnitCycleType = MPTW;
        PTW->M = 1;
        
       // modifyPTW(SDW, address); is this better?
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    //word24 y2 = TPR.CA % 1024;
    word24 y2 = address % 1024;
    
    finalAddress = ((PTW->ADDR & 0777777) << 6) + y2;
    finalAddress &= 0xffffff;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H:FAP): (%05o:%06o) finalAddress=%08o\n",TPR.TSR, address, finalAddress);

HI:
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(HI)\n");
    
    if (thisCycle == INSTRUCTION_FETCH)
        goto J;
    
    if (thisCycle == RTCD_OPERAND_FETCH)
        goto K;
    
    if (i && i->info->flags & CALL6_INS)
        goto N;
    
    if (i && ((i->info->flags & TRANSFER_INS) || instructionFetch))
        goto L;
    
    // load/store data .....

//    if (isREADOP(i))
//    {
//        core_read(finalAddress, readData);  // I think now is the time to do it ...
//        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(HI):Read: finalAddress=%08o readData-%012llo\n", finalAddress, *readData);
//    }
//    
//    if (isSTROP(i))
//    {
//        core_write(finalAddress, writeData);  // I think now is the time to do it ...
//        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(HI):Write: finalAddress=%08o writeData-%012llo\n", finalAddress, writeData);
//    }
    
    goto Exit;
    
J:; // implement
    
K:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(K)\n");

    // C(Y)3,17 → C(TPR.TSR)
    // C(Y+1)0,17 → C(TPR.CA)
    
// handled in RTCD instruction .....
    
//    TPR.TSR = (Ypair[0] & 0077777) >> 18;
//    TPR.CA = (Ypair[1] & 07777777) >> 18;
//    
//    // C(TPR.TRR) ≥ C(PPR.PRR)?
//    if (TPR.TRR >= PPR.PRR)
//        // C(TPR.￼￼TRR) → C(PRi.RNR) for i = 0, 7
//        PR[n].RNR = TPR.TRR;
//    
//    // C(TPR.TRR) → C(PPR.PRR)
//    PPR.PRR = TPR.TRR;
    
KL:;
    // C(TPR.TSR) → C(PPR.PSR) C(TPR.CA) → C(PPR.IC)
//    PPR.PSR = TPR.TSR;
//    PPR.IC = address;   //TPR.CA;

//    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(KL): PPR is set to address %05o:%06o\n", TPR.TSR, TPR.CA);

KLM:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(KLM)\n");
    
    if (TPR.TRR == 0)
        // C(SDW.P) → C(PPR.P)
        PPR.P = SDW->P;
    else
        // ￼0 → C(PPR.P)
        PPR.P = 0;
    
    if (thisCycle == RTCD_OPERAND_FETCH)
        goto O;
    
    goto Exit;    // this may not be setup or right
    
L:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(L)\n");
    
//    if (i->info->flags & TSPN_INS)
//    {
//        if (i->opcode <= 0273)
//            n = (i->opcode & 3);
//        else
//            n = (i->opcode & 3) + 4;
//        
//        PR[n].RNR = PPR.PRR;
//        PR[n].SNR = PPR.PSR;
//        PR[n].WORDNO = (PPR.IC + 1) & 0777777; // IC or IC+1?????
//        PR[n].BITNO = 0;
//
//    }
    
    goto KL;
    
M:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M)\n");
    
    goto KLM;
    
    
    //C(TPR.TRR)= 0?
    if (TPR.TRR == 0)
        //C(SDW.P) → C(PPR.P)
        PPR.P = SDW->P;
    else
        PPR.P = 0;
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M): Exit\n");
    
    return finalAddress;

N:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(N): CALL6\n");

    if (TPR.TRR == PPR.PRR)
        PR[7].SNR = PR[6].SNR;
    else
        PR[7].SNR = ((DSBR.STACK << 3) | TPR.TRR) & 077777; // keep to 15-bits
    
    PR[7].RNR = TPR.TRR;
    PR[7].WORDNO = 0;
    PR[7].BITNO = 0;
    PPR.PRR = TPR.TRR;
    PPR.PSR = TPR.TSR;
    PPR.IC = TPR.CA;    // IC or IC+1???

    goto M;
    
O:;
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(O): RTCD\n");
    
// All of this is done in the instruction; doing it here fouls the fetch
// of the second word of the Ypair
#if 0 
    int CY316 = (CY >> 16) & 03;
 
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(O): C(Y)18,20 = %06o\n", CY316);
    
    // C(TPR.TRR) ≥ RSDWH.R1?
    if (TPR.TRR >= RSDWH_R1)
    {
        // C(TPR.TRR) ≥ C(Y)18,20?
        if (TPR.TRR >= CY316)
            goto Exit;
        else
            // C(Y)18,20 → C(TPR.TRR)
            TPR.TRR = CY316;
    }
    else
    {
        // C(Y)18,20 ≥ RSDWH.R1?
        if (CY316 >= RSDWH_R1)
            TPR.TRR = CY316;
        else
            // RSDWH.R1 → C(TPR.TRR)
            TPR.TRR = RSDWH_R1;
    }
#endif
    
    goto Exit;    // or 0 or -1???
    
#ifndef QUIET_UNUSED
P:;
#endif

    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(P): ITP\n");
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(P): RSDWH_R1 = %0o", RSDWH_R1);
    
    // TODO: need to extract n from the ITP pair
    
    // C(TPR.TRR) ≥ RSDWH.R1?
    if (TPR.TRR >= RSDWH_R1)
    {
        // C(TPR.TRR) ≥ C(PRn .RNR)??
        if (TPR.TRR >= PR[n].RNR)
            goto Exit;
        else
            //C(PRn .RNR) → C(TPR.TRR)
            TPR.TRR = PR[n].RNR;
    }
    else
    {
        // C(PRn .RNR) ≥ RSDWH.R1?
        if (PR[n].RNR >= RSDWH_R1)
            TPR.TRR = PR[n].RNR;
        else
            // RSDWH.R1 → C(TPR.TRR)
            TPR.TRR = RSDWH_R1;
    }
   
Exit:;
//    sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Exit): lastCycle: %s => %s\n", strPCT(lastCycle), strPCT(thisCycle));

    
//    lastCycle = thisCycle;
    return finalAddress;    // or 0 or -1???
}


