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
word36 YPair[2];            ///< Ypair


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

void doPtrReg(DCDstruct *i)
{
    //processorAddressingMode = APPEND_MODE;
    //set_addr_mode(APPEND_mode);   // HWR This should not set append mode

    word3 n = GET_PRN(i->IWB);  // get PRn
    word15 offset = GET_OFFSET(i->IWB);
    
    TPR.TSR = PR[n].SNR;
    TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
    
    TPR.CA = (PR[n].WORDNO + SIGNEXT15(offset)) & 0777777;
    TPR.TBR = PR[n].BITNO;  // TPR.BITNO = PR[n].BITNO;
    //address = TPR.CA;
    rY = TPR.CA;     //address;    // is this right?
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doPtrReg(): n=%o offset=%05o TPR.CA=%06o TPR.TBR=%o TPR.TSR=%05o TPR.TRR=%o\n", n, offset, TPR.CA, TPR.TBR, TPR.TSR, TPR.TRR);
    }
}

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
        doFault(NULL /* XXX */, acc_viol_fault, ACV15, "acvFault");
        
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
        doFault(NULL /* XXX */, acc_viol_fault, ACV15, "acvFault");
    
    word24 y1 = (2 * segno) % 1024;
    word24 x1 = (2 * segno - y1) / 1024;
    
    word36 PTWx1;
    core_read(DSBR.ADDR + x1, &PTWx1);
    PTWx1 = SETBIT(PTWx1, 9);
    core_write(DSBR.ADDR + x1, PTWx1);
    
    PTW0.U = 1; // XXX ???
    
    return &PTW0;
}


/// \brief XXX SDW0 is the in-core representation of a SDW. Need to have a SDWAM struct as current SDW!!!
static _sdw* fetchSDWfromSDWAM(DCDstruct *i, word15 segno)
{
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(0):segno=%05o\n", segno);
    }
    
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
    
    core_read2(p->ADDR + y1, &SDWeven, &SDWodd);
    
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
    
    sim_debug (DBG_APPENDING, & cpu_dev, "fetchPSDW y1 0%o p->ADDR 0%o SDW 0%012llo 0%012llo ADDR 0%o BOUND 0%0 U %o F %o\n",
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
        doFault(NULL /* XXX */, acc_viol_fault, ACV15, "acvFault");
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
                    PTW->USE -= 1;
            }
            PTW->USE = 63;
            
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
    PTW0.M = TSTBIT(PTW2, 6);
    PTW0.F = TSTBIT(PTW2,2);
    PTW0.FC = PTWx2 & 3;
    
    return &PTW0;
}

static void loadPTWAM(word15 segno, word18 offset)
{
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
                if (!q->F)
                    continue;
                
                q->USE -= 1;
                q->USE &= 077;
            }
            
            PTW = p;
            return;
        }
    }
    sim_printf("loadPTWAM(segno=%05o, offset=%012o): no USE=0 found!\n", segno, offset);

}

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




/**
 * Is the instruction a SToRage OPeration ?
 */
#ifndef QUIET_UNUSED
static bool isSTROP(word36 inst)
{
    // XXX: implement
    return false;
}

static bool isAPUDataMovement(word36 inst)
{
    // XXX: implement - when we figure out what it is
    return false;
}
#endif

static char *strAccessType(MemoryAccessType accessType)
{
    switch (accessType)
    {
        case Unknown:           return "Unknown";
        case InstructionFetch:  return "InstructionFetch";
        case IndirectRead:      return "IndirectRead";
        case DataRead:          return "DataRead";
        case DataWrite:         return "DataWrite";
        case OperandRead:       return "OperandRead";
        case OperandWrite:      return "OperandWrite";
        case Call6Operand:      return "Call6Operand";
        case RTCDOperand:       return "RTCDOperand";
        default:                return "???";
    }
}

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

static word36 acvFaults = 0;   ///< pending ACV faults

static void acvFault(DCDstruct *i, _fault_subtype acvfault)
{
    
    char temp[256];
    sprintf(temp, "group 6 ACV fault %s(%d)\n", strACV(acvfault), acvfault);

    sim_printf(temp);
    
    //acvFaults |= (1 << acvfault);   // or 'em all together
    acvFaults |= acvfault;   // or 'em all together

    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(acvFault): acvFault=%s(%d) acvFaults=%llu\n", strACV(acvfault), (int)acvFault, acvFaults);
    }
    
    doFault(i, acc_viol_fault, acvfault, temp); // NEW HWR 17 Dec 2013
}

/*
 * what follows is my attempt to mimick the appending unit. I try to follow Fig 8-1 rather slavishly even at the
 * expense of effeciently. Make certain everything works, and then clean-up. Later. Much later .....
 *
 */

/**
 * sequential instruction fetch .....
 */
static word36
doAppendInstructionFetch(DCDstruct *i, word36 *readData)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
    
    // either sequential instruction fetch ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(Entry 1) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(Entry 2) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(Entry 3) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }

    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(A):DSBR.U=%o\n", DSBR.U);
        }
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                doFault(i, dir_flt0_fault + PTW0.FC, 0, "!PTW0.F");
                // XXX what if they ignore the fault? Can it be ignored?
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(A):SDW0.F == 0! Initiating directed fault\n");
            }
            
            // XXX initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
            
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif

    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(B)\n");
    }
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(i, ACV0);

#ifndef QUIET_UNUSED
F:; ///< transfer or instruction fetch
#endif
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(F): instruction fetch\n");
    }
    
    // C(TPR.TRR) < C(SDW .R1)?
    if (TPR.TRR < SDW->R1)
        acvFault(i, ACV1);
    
    //C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        acvFault(i, ACV1);
    
    //SDW .E set ON?
    if (!SDW->E)
        acvFault(i, ACV2);
    
    //C(PPR.PRR) = C(TPR.TRR)?
    if (PPR.PRR == TPR.TRR)
        goto D;
    
    acvFault(i, ACV12);

D:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(D)\n");
    }
    // instruction fetch
    if (rRALR == 0)
        goto G;
    
    // C(PPR.PRR) < RALR?
    if (!(PPR.PRR < rRALR))
        acvFault(i, ACV13);
    
    goto G;
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(G)\n");
    }
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(i, ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        doFault(i, acc_viol_fault, 0, "acvFaults");
    // XXX what if they ignore the fault? Can it be ignored?
    
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
        goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
            // initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
        
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    // The MVT, TCT, TCTR, and CMPCT instruction have a prepage check. The size of the translate table is determined by the TA1 data type as shown in the table below. Before the instruction is executed, a check is made for allocation in memory for the page for the translate table. If the page is not in memory, a Missing Page fault occurs before execution of the instruction. (cf. Bull, RJ78, p.7-75, sec 7.14.15)
    
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(H:FANP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    goto L;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(I)\n");
    }
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(I:FAP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    goto L;
    
L:;
    PPR.PSR = TPR.TSR;
    //PPR.IC = TPR.CA; removed 10 MAR 2013 HWR
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(L): IC set to %05o:%08o\n", TPR.TSR, TPR.CA);
    }
    
#ifndef QUIET_UNUSED
M:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M)\n");
    }
    
    //C(TPR.TRR)= 0?
    if (TPR.TRR == 0)
        //C(SDW.P) → C(PPR.P)
        PPR.P = SDW->P;
    else
        PPR.P = 0;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M): Exit\n");
    }
    
    return finalAddress;
    

}

static word36
doAppendDataReadOLD(DCDstruct *i, word36 *readData, bool bNotOperand)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
// XXX CAC isn't GET_A(i->IWB) the same as i->a?
    if ((GET_A(i->IWB) && i->iwb->ndes == 0)  || didITSITP || bNotOperand)    // indirect or ITS/ITP just setup
    {
        if (apndTrace && didITSITP)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): previous ITS/ITP detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        if (apndTrace && GET_A(i->IWB))
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): Data Read detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        goto A;
    }
   
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }

A:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A)\n");
    }
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        }
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):DSBR.U=%o\n", DSBR.U);
        }
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F == 0");
                // XXX what if they ignore the fault? Can it be ignored?
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):SDW0.F == 0! Initiating directed fault\n");
            }
            
            // initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
            
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif
    
#ifndef QUIET_UNUSED
B:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(B)\n");
    }
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(i, ACV0);
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV3 = ORB
        acvFault(i, ACV3);
    
    if (SDW->R)
        goto G;
    
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
        //Set fault ACV4 = R-OFF
        acvFault(i, ACV4);
    
    goto G;

    
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(G)\n");
    }
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(i, ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        doFault(i, acc_viol_fault, 0, "acvFault");
        // XXX what if they ignore the fault? Can it be ignored?
    
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
        goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
            // initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTWF0.F");

        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(H:FANP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    return finalAddress;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(I)\n");
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(I:FAP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    return finalAddress;
}

static word36
doAppendDataRead(DCDstruct *i, word36 *readData, bool bNotOperand)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
    if ((GET_A(i->IWB) && i->iwb->ndes == 0)  || didITSITP || bNotOperand)    // indirect or ITS/ITP just setup
    {
        if (apndTrace && didITSITP)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): previous ITS/ITP detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        if (apndTrace && GET_A(i->IWB))
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): Data Read detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        goto A;
    }
    
    if (get_addr_mode() == APPEND_mode)
    {
        TPR.TRR = PPR.PRR;
        TPR.TSR = PPR.PSR;
    }
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
A:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A)\n");
    }
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        }
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):DSBR.U=%o\n", DSBR.U);
        }
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
            // XXX initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):SDW0.F == 0! Initiating directed fault\n");
            }
            
            // initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
            
        }
        else
        // load SDWAM .....
        loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif
    
#ifndef QUIET_UNUSED
B:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(B)\n");
    }
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
    // Set fault ACV0 = IRO
    acvFault(i, ACV0);
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
    //Set fault ACV3 = ORB
    acvFault(i, ACV3);
    
    if (SDW->R)
    goto G;
    
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
    //Set fault ACV4 = R-OFF
    acvFault(i, ACV4);
    
    goto G;
    
    
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(G)\n");
    }
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
    acvFault(i, ACV15);
    
    if (acvFaults)
    // Initiate an access violation fault
    doFault(i, acc_viol_fault, 0, "acvFault");
    // XXX what if they ignore the fault? Can it be ignored?
    
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
    goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
        // initiate a directed fault
        doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTWF0.F");
        
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(H:FANP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    return finalAddress;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(I)\n");
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(I:FAP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    return finalAddress;
}

static word36
doAppendDataWriteOLD(DCDstruct *i, word36 writeData, bool bNotOperand)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
    if ((GET_A(i->IWB) && i->iwb->ndes == 0) || didITSITP || bNotOperand)    // indirect or ITS/ITP just setup
    {
        if (apndTrace)
        {
            if (didITSITP)
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): previous ITS/ITP detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
            else if (GET_A(i->IWB))
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
            }
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): wrote operand detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        goto A;
    }
    
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
A:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A)\n");
    }
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        }
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):DSBR.U=%o\n", DSBR.U);
        }
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F == 0");
                // XXX what if they ignore the fault? Can it be ignored?
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):SDW0.F == 0! Initiating directed fault\n");
            }
            // initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
            
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif
    
#ifndef QUIET_UNUSED
B:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(B)\n");
    }
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(i, ACV0);
    
    //if (isSTROP(IWB))
    // Yes ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(B): a STR-OP\n");
    }
    
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV5 = OWB
        acvFault(i, ACV5);
        
    if (!SDW->W)
        // Set fault ACV6 = W-OFF
        acvFault(i, ACV6);
    goto G;
    
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(G)\n");
    }
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(i, ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        doFault(i, acc_viol_fault, 0, "acvFault");
        // XXX what if they ignore the fault? Can it be ignored?
    
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
        goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
            // initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F != 0");
            // XXX what if they ignore the fault? Can it be ignored?
        
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_write(finalAddress, writeData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(H:FANP) Write: finalAddress=%08o writeData=%012llo\n", finalAddress, writeData);
    }
    return finalAddress;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(I)\n");
    }
    
    //if (isSTROP(IWB) && PTW->M == 0)
    if (PTW->M == 0)
    {
        appendingUnitCycleType = MPTW;
        modifyPTW(SDW, TPR.CA);
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;

    core_write(finalAddress, writeData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(I:FAP) Write: finalAddress=%08o writeData=%012llo\n", finalAddress, writeData);
    }
    return finalAddress;
    
}
static word36
doAppendDataWrite(DCDstruct *i, word36 writeData, bool bNotOperand)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
    if ((GET_A(i->IWB) && i->iwb->ndes == 0) || didITSITP || bNotOperand)    // indirect or ITS/ITP just setup
    {
        if (apndTrace)
        {
            if (didITSITP)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): previous ITS/ITP detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
            else if (GET_A(i->IWB))
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
            }
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): wrote operand detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        goto A;
    }
    
    if (get_addr_mode() == APPEND_mode)
    {
        TPR.TRR = PPR.PRR;
        TPR.TSR = PPR.PSR;
    }
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
A:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A)\n");
    }
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        }
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):DSBR.U=%o\n", DSBR.U);
        }
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
            // XXX initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):SDW0.F == 0! Initiating directed fault\n");
            }
            // initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            // XXX what if they ignore the fault? Can it be ignored?
            
        }
        else
        // load SDWAM .....
        loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif
    
#ifndef QUIET_UNUSED
B:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(B)\n");
    }
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
    // Set fault ACV0 = IRO
    acvFault(i, ACV0);
    
    //if (isSTROP(IWB))
    // Yes ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(B): a STR-OP\n");
    }
    
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
    //Set fault ACV5 = OWB
    acvFault(i, ACV5);
    
    if (!SDW->W)
    // Set fault ACV6 = W-OFF
    acvFault(i, ACV6);
    goto G;
    
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(G)\n");
    }
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
    acvFault(i, ACV15);
    
    if (acvFaults)
    // Initiate an access violation fault
    doFault(i, acc_viol_fault, 0, "acvFault");
    // XXX what if they ignore the fault? Can it be ignored?
    
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
    goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
        // initiate a directed fault
        doFault(i, dir_flt0_fault + PTW0.FC, 0, "PTW0.F != 0");
        // XXX what if they ignore the fault? Can it be ignored?
        
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_write(finalAddress, writeData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(H:FANP) Write: finalAddress=%08o writeData=%012llo\n", finalAddress, writeData);
    }
    return finalAddress;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(I)\n");
    }
    
    //if (isSTROP(IWB) && PTW->M == 0)
    if (PTW->M == 0)
    {
        appendingUnitCycleType = MPTW;
        modifyPTW(SDW, TPR.CA);
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_write(finalAddress, writeData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(I:FAP) Write: finalAddress=%08o writeData=%012llo\n", finalAddress, writeData);
    }
    return finalAddress;
    
}


bool didITSITP = false; ///< true after an ITS/ITP processing

static void doITP(word4 Tag)
{
    if ((cpu_dev.dctrl & DBG_APPENDING) && sim_deb)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "ITP Pair: PRNUM=%o BITNO=%o WORDNO=%o MOD=%o\n", GET_ITP_PRNUM(YPair), GET_ITP_WORDNO(YPair), GET_ITP_BITNO(YPair), GET_ITP_MOD(YPair));
    }
    /**
     For n = C(ITP.PRNUM):
     C(PRn.SNR) → C(TPR.TSR)
     maximum of ( C(PRn.RNR), C(SDW.R1), C(TPR.TRR) ) → C(TPR.TRR)
     C(ITP.BITNO) → C(TPR.TBR)
     C(PRn.WORDNO) + C(ITP.WORDNO) + C(r) → C(TPR.CA)
     
     Notes:
     1. r = C(CT-HOLD) if the instruction word or preceding indirect word specified indirect then register modification, or
     2. r = C(ITP.MOD.Td) if the instruction word or preceding indirect word specified register then indirect modification and ITP.MOD.Tm specifies either register or register then indirect modification.
     3. SDW.R1 is the upper limit of the read/write ring bracket for the segment C(TPR.TSR) (see Section 8).
     */
    word3 n = GET_ITP_PRNUM(YPair);
    TPR.TSR = PR[n].SNR;
    TPR.TRR = max3(PR[n].RNR, SDW->R1, TPR.TRR);
    TPR.TBR = GET_ITP_BITNO(YPair);
    TPR.CA = PAR[n].WORDNO + GET_ITP_WORDNO(YPair) + getCr(GET_TD(Tag));
    rY = TPR.CA;
    
    rTAG = GET_ITP_MOD(YPair);
    return;
}

static void doITS(word4 Tag)
{
    if ((cpu_dev.dctrl & DBG_APPENDING) && sim_deb)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "ITS Pair: SEGNO=%o RN=%o WORDNO=%o BITNO=%o MOD=%o\n", GET_ITS_SEGNO(YPair), GET_ITS_RN(YPair), GET_ITS_WORDNO(YPair), GET_ITS_BITNO(YPair), GET_ITS_MOD(YPair));
    }
    /*
     C(ITS.SEGNO) → C(TPR.TSR)
     maximum of ( C(ITS. RN), C(SDW.R1), C(TPR.TRR) ) → C(TPR.TRR)
     C(ITS.BITNO) → C(TPR.TBR)
     C(ITS.WORDNO) + C(r) → C(TPR.CA)
     
     1. r = C(CT-HOLD) if the instruction word or preceding indirect word specified indirect then register modification, or
     2. r = C(ITS.MOD.Td) if the instruction word or preceding indirect word specified register then indirect modification and ITS.MOD.Tm specifies either register or register then indirect modification.
     3. SDW.R1 is the upper limit of the read/write ring bracket for the segment C(TPR.TSR) (see Section 8).
     */
    TPR.TSR = GET_ITS_SEGNO(YPair);
    TPR.TRR = max3(GET_ITS_RN(YPair), SDW->R1, TPR.TRR);
    TPR.TBR = GET_ITS_BITNO(YPair);
    TPR.CA = GET_ITS_WORDNO(YPair) + getCr(GET_TD(Tag));
    rY = TPR.CA;
    
    rTAG = GET_ITS_MOD(YPair);
    
    return;
}

bool
doITSITP(DCDstruct *i, word36 indword, word6 Tag)
{
    word6 indTag = GET_TAG(indword);
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: indword:%012llo Tag:%o\n", indword, Tag);
    }
    
    if (!((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITP(indword) || ISITS(indword))))
    //if (!(ISITP(indword) || ISITS(indword)))
    {
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: returning false\n");
        }
        doFault(i, illproc_fault, ill_mod, "Incorrect address modifier");
        return false;  // couldnt/woudlnt/shouldnt do ITS/ITP indirection
    }
    
    /*
     Whenever the processor is forming a virtual address two special address modifiers may be specified and are effective under certain restrictive conditions. The special address modifiers are shown in Table 6-4 and discussed in the paragraphs below.
     The conditions for which the special address modifiers are effective are as follows:
     1. The instruction word (or preceding indirect word) must specify indirect then register or register then indirect modification.
     2. The computed address for the indirect word must be even.
     If these conditions are satisfied, the processor examines the indirect word TAG field for the special address modifiers.
     XXX If either condition is violated, the indirect word TAG field is interpreted as a normal address modifier and the presence of a special address modifier will cause an illegal procedure, illegal modifier, fault.
     */
    //if (processorAddressingMode != APPEND_MODE || TPR.CA & 1)
    /*
    if (get_addr_mode() != APPEND_mode || (TPR.CA & 1))
        // XXX illegal procedure, illegal modifier, fault
        doFault(i, illproc_fault, ill_mod, "get_addr_mode() != APPEND_MODE || (TPR.CA & 1)");
     */
    if ((TPR.CA & 1))
    // XXX illegal procedure, illegal modifier, fault
        doFault(i, illproc_fault, ill_mod, "doITSITP() : (TPR.CA & 1)");

    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: reading indirect words from %06o\n", TPR.CA);
    }
    
    // this is probably sooo wrong, but it's a start ...
    processorCycle = INDIRECT_WORD_FETCH;
    YPair[0] = indword;
    
    Read(i, TPR.CA + 1, &YPair[1], DataRead, Tag);
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: YPair= %012llo %012llo\n", YPair[0], YPair[1]);
    }
    
    if (ISITS(indTag))
        doITS(Tag);
    else
        doITP(Tag);
    
    didITSITP = true;
    //processorAddressingMode = APPEND_MODE;
    
    // HWR 22 Dec 2013 APpend mode only not set ITS/ITP
    //set_addr_mode(APPEND_mode);
    
    return true;
    
}


static word36
doAppendIndirectReadOLD(DCDstruct *i, word36 *readData, word6 Tag)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
//    if (apndTrace)
//    {
//        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
//        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
//    }
//    
//    if (GET_A(IWB))
//    {
//        if (apndTrace)
//            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
//        goto A;
//    }
//    
//    TPR.TRR = PPR.PRR;
//    TPR.TSR = PPR.PSR;
//
    didITSITP = false;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
A:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A)\n");
    }
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):DSBR.U=%o\n", DSBR.U);
        }
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                doFault(i, dir_flt0_fault + PTW0.FC, 0, "!PTW0.F");
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):SDW0.F == 0! Initiating directed fault\n");
            }
            
            // initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");

        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif
    
#ifndef QUIET_UNUSED
B:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(B)\n");
    }
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(i, ACV0);
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV3 = ORB
        acvFault(i, ACV3);
    
    if (SDW->R)
        goto G;
    
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
        //Set fault ACV4 = R-OFF
        acvFault(i, ACV4);
    
    goto G;
    
    
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(G)\n");
    }
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(i, ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        doFault(i, acc_viol_fault, 0, "acvFault");

    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
        goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
            // initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA)");

        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(H:FANP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    goto J;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(I)\n");
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(I:FAP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    goto J;
    
J:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) ISITS(%d) ISITP(%d)\n", ISITS(*readData), ISITP(*readData));
    }
    
    //if ((Tdes == TM_IR || Tdes == TM_RI) && (ISITS(*readData) || ISITP(*readData)))
    //    doITSITP(*readData, Tdes);
    if ((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITS(*readData) || ISITP(*readData)))
        doITSITP(i, *readData, Tag);
    
    else
    {
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Exit (Non-ITS/ITP): finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
        }
        return finalAddress;
    }
    
    /// at this point CA, TBR, TRR, TSR, & Tdes are set up to read from where the ITS/ITP pointer said ....
    //
    
    word36 newwrd = (TPR.CA << 18) | rTAG;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Exit (ITS/ITP): newwrd=%012llo\n", newwrd);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Need to read from CA:%6o FA:%08o\n", TPR.CA, finalAddress);
    }
    
    *readData = newwrd;
    return finalAddress;
}

static word36
doAppendIndirectRead(DCDstruct *i, word36 *readData, word6 Tag)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
    //    if (apndTrace)
    //    {
    //        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
    //        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    //    }
    //
    //    if (GET_A(IWB))
    //    {
    //        if (apndTrace)
    //            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
    //        goto A;
    //    }
    //
    //    TPR.TRR = PPR.PRR;
    //    TPR.TSR = PPR.PSR;
    //
    didITSITP = false;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
A:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A)\n");
    }
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):DSBR.U=%o\n", DSBR.U);
        }
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
            // XXX initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "!PTW0.F");
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):SDW0.F == 0! Initiating directed fault\n");
            }
            
            // initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            
        }
        else
        // load SDWAM .....
        loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif
    
#ifndef QUIET_UNUSED
B:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(B)\n");
    }
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
    // Set fault ACV0 = IRO
    acvFault(i, ACV0);
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
    //Set fault ACV3 = ORB
    acvFault(i, ACV3);
    
    if (SDW->R)
    goto G;
    
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
    //Set fault ACV4 = R-OFF
    acvFault(i, ACV4);
    
    goto G;
    
    
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(G)\n");
    }
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
    acvFault(i, ACV15);
    
    if (acvFaults)
    // Initiate an access violation fault
    doFault(i, acc_viol_fault, 0, "acvFault");
    
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
    goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
        // initiate a directed fault
        doFault(i, dir_flt0_fault + PTW0.FC, 0, "!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA)");
        
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(H:FANP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    goto J;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(I)\n");
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(I:FAP) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
    }
    goto J;
    
J:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) ISITS(%d) ISITP(%d)\n", ISITS(*readData), ISITP(*readData));
    }
    
    //if ((Tdes == TM_IR || Tdes == TM_RI) && (ISITS(*readData) || ISITP(*readData)))
    //    doITSITP(*readData, Tdes);
    if ((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITS(*readData) || ISITP(*readData)))
    doITSITP(i, *readData, Tag);
    
    else
    {
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Exit (Non-ITS/ITP): finalAddress=%08o readData=%012llo\n", finalAddress, *readData);
        }
        return finalAddress;
    }
    
    /// at this point CA, TBR, TRR, TSR, & Tdes are set up to read from where the ITS/ITP pointer said ....
    //
    
    word36 newwrd = (TPR.CA << 18) | rTAG;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Exit (ITS/ITP): newwrd=%012llo\n", newwrd);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Need to read from CA:%6o FA:%08o\n", TPR.CA, finalAddress);
    }
    
    *readData = newwrd;
    return finalAddress;
}

static word36
doAppendIndirectWrite(DCDstruct *i, word36 writeData, word6 Tag)
{
#ifndef QUIET_UNUSED
    word3 RSDWH_R1; ///< I think
#endif
    //    if (apndTrace)
    //    {
    //        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
    //        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    //    }
    //
    //    if (GET_A(IWB))
    //    {
    //        if (apndTrace)
    //            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
    //        goto A;
    //    }
    //
    //    TPR.TRR = PPR.PRR;
    //    TPR.TSR = PPR.PSR;
    //
    didITSITP = false;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
A:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(A)\n");
    }
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(i, TPR.TSR))
    {
        // No
        
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(A):DSBR.U=%o\n", DSBR.U);
        }
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
            // XXX initiate a directed fault
            doFault(i, dir_flt0_fault + PTW0.FC, 0, "!PTW0.F");
            
            if (!PTW0.U)
            {
                appendingUnitCycleType = MDSPTW;
                modifyDSPTW(TPR.TSR);
            }
            
            appendingUnitCycleType = PSDW;  // Paged SDW Fetch. Fetches an SDW from a paged descriptor segment.
            fetchPSDW(TPR.TSR);
        }
        else
        {
            appendingUnitCycleType = NSDW; // Nonpaged SDW Fetch. Fetches an SDW from an unpaged descriptor segment.
            fetchNSDW(TPR.TSR); // load SDW0 from descriptor segment table.
        }
        
        if (SDW0.F == 0)
        {
            if (apndTrace)
            {
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(A):SDW0.F == 0! Initiating directed fault\n");
            }
            
            // initiate a directed fault ...
            doFault(i, dir_flt0_fault + SDW0.FC, 0, "SDW0.F == 0");
            
        }
        else
        // load SDWAM .....
        loadSDWAM(TPR.TSR);
    }
    
#ifndef QUIET_UNUSED
    // Yes...
    RSDWH_R1 = SDW->R1;
#endif
    
#ifndef QUIET_UNUSED
B:;
#endif
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(B)\n");
    }
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(i, ACV0);
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV5 = OWB
        acvFault(i, ACV5);
    
    if (SDW->W) // write permissions?
        goto G; // Yes.
    
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
        //Set fault ACV6 = W-OFF
        acvFault(i, ACV6);
    
    goto G;
    
    
G:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(G)\n");
    }
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(i, ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        doFault(i, acc_viol_fault, 0, "acvFault");
    
    
    // is segment C(TPR.TSR) paged?
    if (SDW->U)
        goto H; // Not paged
    
    // Yes. segment is paged ...
    // is PTW for C(TPR.CA) in PTWAM?
    
    if (!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA))
    {
        appendingUnitCycleType = PTWfetch;
        fetchPTW(SDW, TPR.CA);
        if (PTW0.F)
        // initiate a directed fault
        doFault(i, dir_flt0_fault + PTW0.FC, 0, "!fetchPTWfromPTWAM(SDW->POINTER, TPR.CA)");
        
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    }
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_write(finalAddress, writeData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(H:FANP) Read: finalAddress=%08o writeData=%012llo\n", finalAddress, writeData);
    }
    goto J;
    
I:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(I)\n");
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_write(finalAddress, writeData);  // I think now is the time to do it ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(I:FAP) Read: finalAddress=%08o writeData=%012llo\n", finalAddress, writeData);
    }
    goto J;
    
J:;
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(J) ISITS(%d) ISITP(%d)\n", ISITS(writeData), ISITP(writeData));
    }
    
    //if ((Tdes == TM_IR || Tdes == TM_RI) && (ISITS(*readData) || ISITP(*readData)))
    //    doITSITP(*readData, Tdes);
    if ((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITS(writeData) || ISITP(writeData)))
    doITSITP(i, writeData, Tag);
    
    else
    {
        if (apndTrace)
        {
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(J) Exit (Non-ITS/ITP): finalAddress=%08o writeData=%012llo\n", finalAddress, writeData);
        }
        return finalAddress;
    }
    
    /// at this point CA, TBR, TRR, TSR, & Tdes are set up to read from where the ITS/ITP pointer said ....
    //
    
    word36 newwrd = (TPR.CA << 18) | rTAG;
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(J) Exit (ITS/ITP): newwrd=%012llo\n", newwrd);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectWrite(J) Need to read from CA:%6o FA:%08o\n", TPR.CA, finalAddress);
    }
    
    return finalAddress;
}


word36 doAppendCycle(DCDstruct *i, MemoryAccessType accessType, word6 Tag, word36 writeData, word36 *readData)
{
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry): accessType=%s IWB=%012llo A=%d\n", strAccessType(accessType), i->IWB, GET_A(i->IWB));
    }
    
    word36 fa = 0;
    switch (accessType)
    {
        case InstructionFetch:
            fa = doAppendInstructionFetch(i, readData);
            break;
        case DataRead:
        case APUDataRead:        // append operations from absolute mode
            fa = doAppendDataRead(i, readData, true); //  a data read
            break;
        case OperandRead:
        case APUOperandRead:
            fa = doAppendDataRead(i, readData, false); // an operand read
            break;
        case APUDataWrite:      // append operations from absolute mode
        case DataWrite:// XXX will need to do something similiar to the read ops here
            fa = doAppendDataWrite(i, writeData, true);
            break;
        case OperandWrite:
        case APUOperandWrite:
            fa = doAppendDataWrite(i, writeData, false);
            break;
        case IndirectRead:
            fa = doAppendIndirectRead(i, readData, Tag);
            break;
        case IndirectWrite:
            fa = doAppendIndirectRead(i, readData, Tag);
            break;
        default:
            fprintf(stderr,  "doAppendCycle(Entry): unsupported accessType=%s\n", strAccessType(accessType));
            return 0;
    }
    didITSITP = false;
    return fa;
}
