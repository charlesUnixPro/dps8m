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

enum _appendingUnit_cycle_type appendingUnitCycleType = APPUNKNOWN;

word24 finalAddress = 0;    ///< final, 24-bit address that comes out of the APU
word36 CY = 0;              ///< C(Y) operand data from memory
                            // XXX do we need to make CY part of DCDstruct ?
word36 CY1 = 0;             ///< C(Y+1) operand data .....
word36 YPair[2];            ///< Ypair


/**

 The Use of Bit 29 in the Instruction Word
 The reader is reminded that there is a preliminary step of loading TPR.CA with the ADDRESS field of the instruction word during instruction decode.
 If bit 29 of the instruction word is set to 1, modification by pointer register is invoked and the preliminary step is executed as follows:
 1. The ADDRESS field of the instruction word is interpreted as shown in Figure 6-7 below.
 2. C(PRn.SNR) → C(TPR.TSR)
 ￼￼￼￼￼￼￼￼￼￼￼￼￼3. maximum of ( C(PRn.RNR), C(TPR.TRR), C(PPR.PRR) ) → C(TPR.TRR)
 4. C(PRn.WORDNO) + OFFSET → C(TPR.CA) (NOTE: OFFSET is a signed binary number.)
 5. C(PRn.BITNO) → TPR.BITNO
 */

void doAddrModPtrReg(DCDstruct *i)
{
    word3 n = GET_PRN(i->IWB);  // get PRn
    word15 offset = GET_OFFSET(i->IWB);
    int soffset = SIGNEXT15(GET_OFFSET(i->IWB));
    
    TPR.TSR = PR[n].SNR;
    TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
    
    TPR.CA = (PR[n].WORDNO + SIGNEXT15(offset)) & 0777777;
    TPR.TBR = PR[n].BITNO;  // TPR.BITNO = PR[n].BITNO;
    i->address = TPR.CA;
    rY = i->address;    // is this right?
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAddrModPtrReg(): n=%o offset=%05o TPR.CA=%06o TPR.TBR=%o TPR.TSR=%05o TPR.TRR=%o\n", n, offset, TPR.CA, TPR.TBR, TPR.TSR, TPR.TRR);
}

void doPtrReg(DCDstruct *i)
{
    processorAddressingMode = APPEND_MODE;

    word3 n = GET_PRN(i->IWB);  // get PRn
    word15 offset = GET_OFFSET(i->IWB);
    
    TPR.TSR = PR[n].SNR;
    TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
    
    TPR.CA = (PR[n].WORDNO + SIGNEXT15(offset)) & 0777777;
    TPR.TBR = PR[n].BITNO;  // TPR.BITNO = PR[n].BITNO;
    //address = TPR.CA;
    //rY = address;    // is this right?
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doPtrReg(): n=%o offset=%05o TPR.CA=%06o TPR.TBR=%o TPR.TSR=%05o TPR.TRR=%o\n", n, offset, TPR.CA, TPR.TBR, TPR.TSR, TPR.TRR);
    
}

/**
 * fetch descriptor segment PTW ...
 */
_ptw0* fetchDSPTW(word15 segno)
{
    if (2 * segno >= 16 * (DSBR.BND + 1))
        // XXX: generate access violation, out of segment bounds fault
        ;
        
    word24 y1 = (2 * segno) % 1024;
    word24 x1 = (2 * segno - y1) / 1024;

    word36 PTWx1;
    core_read(DSBR.ADDR + x1, &PTWx1);
    
    PTW0.ADDR = GETHI(PTWx1);
    PTW0.U = TSTBIT(PTWx1, 9);
    PTW0.M = TSTBIT(PTWx1, 6);
    PTW0.F = TSTBIT(PTWx1, 2);
    PTW0.FC = PTWx1 & 3;
    
    return &PTW0;
}
/**
 * modify descriptor segment PTW (Set U=1) ...
 */
_ptw0* modifyDSPTW(word15 segno)
{
    if (2 * segno >= 16 * (DSBR.BND + 1))
        // XXX: generate access violation, out of segment bounds fault
        ;
    
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
_sdw* fetchSDWfromSDWAM(word15 segno)
{
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(0):segno=%05o\n", segno);

    for(int _n = 0 ; _n < 64 ; _n++)
    {
        // make certain we initialize SDWAM prior to use!!!
        //if (SDWAM[_n]._initialized && segno == SDWAM[_n].POINTER)
        if (SDWAM[_n].F && segno == SDWAM[_n].POINTER)
        {
            if (apndTrace)
                sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(1):found match for segno %05o at _n=%d\n", segno, _n);

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
                sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(2):SDWAM[%d]=%s\n", _n, strSDW(SDW));

            return SDW;
        }
    }
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchSDWfromSDWAM(3):SDW for segment %05o not found in SDWAM\n", segno);

    return NULL;    // segment not referenced in SDWAM
}

/**
 * Fetches an SDW from a paged descriptor segment.
 */
_sdw0* fetchPSDW(word15 segno)
{
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchPSDW(0):segno=%05o\n", segno);

    _ptw0 *p = fetchDSPTW(segno);

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
    
    return &SDW0;
}

/// \brief Nonpaged SDW Fetch
/// Fetches an SDW from an unpaged descriptor segment.
_sdw0 *fetchNSDW(word15 segno)
{
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(0):segno=%05o\n", segno);

    if (2 * segno >= 16 * (DSBR.BND + 1))
    {
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(1):Access Violation, out of segment bounds for segno=%05o DBSR.BND=%d\n", segno, DSBR.BND);

        // XXX: generate access violation, out of segment bounds fault
        ;
    }
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(2):fetching SDW from %05o\n", DSBR.ADDR + 2 * segno);

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
        sim_debug(DBG_APPENDING, &cpu_dev, "fetchNSDW(2):SDW0=%s\n", strSDW0(&SDW0));

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
t_stat dumpSDWAM()
{
    for(int _n = 0 ; _n < 64 ; _n++)
    {
        _sdw *p = &SDWAM[_n];
        
        //if (p->_initialized)
        if (p->F)
            fprintf(stderr, "SDWAM n:%d %s\n\r", _n, strSDW(p));
    }
    return SCPE_OK;
}


/**
 * load the current in-core SDW0 into the SDWAM ...
 */
void loadSDWAM(word15 segno)
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
            
            if (apndTrace)
                sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(2):SDWAM[%d]=%s\n", _n, strSDW(p));

            return;
        }
    }
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "loadSDWAM(3) no USE=0 found for segment=%d\n", segno);

    fprintf(stderr, "loadSDWAM(%05o): no USE=0 found!\n", segno);
    dumpSDWAM();
}

_ptw* fetchPTWfromPTWAM(word15 segno, word18 CA)
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

_ptw0* fetchPTW(_sdw *sdw, word18 offset)
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

void loadPTWAM(word15 segno, word18 offset)
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
    fprintf(stderr, "loadPTWAM(segno=%05o, offset=%012o): no USE=0 found!\n", segno, offset);

}

/**
 * modify target segment PTW (Set M=1) ...
 */
_ptw* modifyPTW(_sdw *sdw, word18 offset)
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
bool isSTROP(word36 inst)
{
    // XXX: implement
    return false;
}

bool isAPUDataMovement(word36 inst)
{
    // XXX: implement - when we figure out what it is
    return false;
}


char *strAccessType(MemoryAccessType accessType)
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

char *strACV(enum enumACV acv)
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
    }
}

word36 acvFaults = 0;   ///< pending ACV faults
void acvFault(enum enumACV acvfault)
{
    
    fprintf(stderr, "group 6 ACV fault %s(%d)\n", strACV(acvfault), acvfault);
    
    //acvFaults |= (1 << acvfault);   // or 'em all together
    acvFaults |= acvfault;   // or 'em all together

    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(acvFault): acvFault=%s(%d) acvFaults=%d\n", strACV(acvfault), acvFault, acvFaults);

}

/*
 * what follows is my attempt to mimick the appending unit. I try to follow Fig 8-1 rather slavishly even at the
 * expense of effeciently. Make certain everything works, and then clean-up. Later. Much later .....
 *
 */

/**
 * sequential instruction fetch .....
 */
word36
doAppendInstructionFetch(DCDstruct *i, word36 *readData)
{
    word3 RSDWH_R1; ///< I think
    
    // either sequential instruction fetch ...
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(Entry 1) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(Entry 2) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(Entry 3) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    

    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(TPR.TSR))
    {
        // No
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(A):DSBR.U=%o\n", DSBR.U);
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                ;
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
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(A):SDW0.F == 0! Initiating directed fault\n");
            
            // XXX initiate a directed fault ...
            ;
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
    // Yes...
    RSDWH_R1 = SDW->R1;

    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(B)\n");
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(ACV0);

F:; ///< transfer or instruction fetch
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(F): instruction fetch\n");
    
    // C(TPR.TRR) < C(SDW .R1)?
    if (TPR.TRR < SDW->R1)
        acvFault(ACV1);
    
    //C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        acvFault(ACV1);
    
    //SDW .E set ON?
    if (!SDW->E)
        acvFault(ACV2);
    
    //C(PPR.PRR) = C(TPR.TRR)?
    if (PPR.PRR == TPR.TRR)
        goto D;
    
    acvFault(ACV12);

D:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(D)\n");
    
    // instruction fetch
    if (rRALR == 0)
        goto G;
    
    // C(PPR.PRR) < RALR?
    if (!(PPR.PRR < rRALR))
        acvFault(ACV13);
    
    goto G;
G:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(G)\n");
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        ;
    
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
            ;
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    // The MVT, TCT, TCTR, and CMPCT instruction have a prepage check. The size of the translate table is determined by the TA1 data type as shown in the table below. Before the instruction is executed, a check is made for allocation in memory for the page for the translate table. If the page is not in memory, a Missing Page fault occurs before execution of the instruction. (cf. Bull, RJ78, p.7-75, sec 7.14.15)
    
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(H:FANP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
    
    goto L;
    
I:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(I)\n");
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendInstructionFetch(I:FAP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
    goto L;
    
L:;
    PPR.PSR = TPR.TSR;
    //PPR.IC = TPR.CA; removed 10 MAR 2013 HWR
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(L): IC set to %05o:%08o\n", TPR.TSR, TPR.CA);
    
M:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M)\n");
    
    //C(TPR.TRR)= 0?
    if (TPR.TRR == 0)
        //C(SDW.P) → C(PPR.P)
        PPR.P = SDW->P;
    else
        PPR.P = 0;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M): Exit\n");
    
    return finalAddress;
    

}

word36
doAppendDataRead(DCDstruct *i, word36 *readData, bool bNotOperand)
{
    word3 RSDWH_R1; ///< I think
    
    if (apndTrace)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) PPR.TRR=%o PPR.TSR=%o\n", PPR.PRR, PPR.PSR);
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    }
    
    if ((GET_A(i->IWB) && i->iwb->ndes == 0)  || didITSITP || bNotOperand)    // indirect or ITS/ITP just setup
    {
        if (apndTrace && didITSITP)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): previous ITS/ITP detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        if (apndTrace && bNotOperand)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry): Data Read detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        goto A;
    }
    
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);

A:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A)\n");
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(TPR.TSR))
    {
        // No
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):DSBR.U=%o\n", DSBR.U);
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                ;
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
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(A):SDW0.F == 0! Initiating directed fault\n");
            
            // initiate a directed fault ...
            ;
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
    // Yes...
    RSDWH_R1 = SDW->R1;
    
B:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(B)\n");
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(ACV0);
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV3 = ORB
        acvFault(ACV3);
    
    if (SDW->R)
        goto G;
    
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
        //Set fault ACV4 = R-OFF
        acvFault(ACV4);
    
    goto G;

    
G:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(G)\n");
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        ;
    
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
            ;
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(H:FANP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
    return finalAddress;
    
I:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(I)\n");
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataRead(I:FAP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
    
    return finalAddress;
}

word36
doAppendDataWrite(DCDstruct *i, word36 writeData, bool bNotOperand)
{
    word3 RSDWH_R1; ///< I think
    
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
            else if (bNotOperand)
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): bit-29 (a) detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry): wrote operand detected. TPR.TRR=%o TPR.TSR=%o\n",TPR.TRR, TPR.TSR);
        }
        goto A;
    }
    
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    
A:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A)\n");
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(TPR.TSR))
    {
        // No
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):DSBR.U=%o\n", DSBR.U);
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                ;
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
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(A):SDW0.F == 0! Initiating directed fault\n");
            
            // initiate a directed fault ...
            ;
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
    // Yes...
    RSDWH_R1 = SDW->R1;
    
B:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(B)\n");
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(ACV0);
    
    //if (isSTROP(IWB))
    // Yes ...
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(B): a STR-OP\n");
        
        
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV5 = OWB
        acvFault(ACV5);
        
    if (!SDW->W)
        // Set fault ACV6 = W-OFF
        acvFault(ACV6);
    goto G;
    
G:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(G)\n");
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        ;
    
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
            ;
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_write(finalAddress, writeData);  // I think now is the time to do it ...
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(H:FANP) Write: finalAddress=%08o writeData=%012o\n", finalAddress, writeData);
  
    return finalAddress;
    
I:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(I)\n");
    
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
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendDataWrite(I:FAP) Write: finalAddress=%08o writeData=%012o\n", finalAddress, writeData);
    
    return finalAddress;
    
}

word18 getCr(word4 Tdes);

bool didITSITP = false; ///< true after an ITS/ITP processing

void doITP(word4 Tag)
{
    if ((cpu_dev.dctrl & DBG_APPENDING) && sim_deb)
        sim_debug(DBG_APPENDING, &cpu_dev, "ITP Pair: PRNUM=%o BITNO=%o WORDNO=%o MOD=%o\n", GET_ITP_PRNUM(YPair), GET_ITP_WORDNO(YPair), GET_ITP_BITNO(YPair), GET_ITP_MOD(YPair));
    
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

void doITS(word4 Tag)
{
    if ((cpu_dev.dctrl & DBG_APPENDING) && sim_deb)
        sim_debug(DBG_APPENDING, &cpu_dev, "ITS Pair: SEGNO=%o RN=%o WORDNO=%o BITNO=%o MOD=%o\n", GET_ITS_SEGNO(YPair), GET_ITS_RN(YPair), GET_ITS_WORDNO(YPair), GET_ITS_BITNO(YPair), GET_ITS_MOD(YPair));
    
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
        sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: indword:%012o Tag:%o\n", indword, Tag);
    
    if (!((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITP(indword) || ISITS(indword))))
    //if (!(ISITP(indword) || ISITS(indword)))
    {
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: returning false\n");

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
    if (processorAddressingMode != APPEND_MODE || TPR.CA & 1)
        // XXX illegal procedure, illegal modifier, fault
        ;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: reading indirect words from %06o\n", TPR.CA);
    
    // this is probably sooo wrong, but it's a start ...
    processorCycle = INDIRECT_WORD_FETCH;
    YPair[0] = indword;
    
    Read(i, TPR.CA + 1, &YPair[1], DataRead, Tag);
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: YPair= %012lo %012lo\n", YPair[0], YPair[1]);

    
    if (ISITS(indTag))
        doITS(Tag);
    else
        doITP(Tag);
    
    didITSITP = true;
    processorAddressingMode = APPEND_MODE;
    return true;
    
}


word36
doAppendIndirectRead(DCDstruct *i, word36 *readData, word6 Tag)
{
    word3 RSDWH_R1; ///< I think
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
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);
    
A:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A)\n");
    
    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(TPR.TSR))
    {
        // No
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):DSBR.U=%o\n", DSBR.U);
        
        if (DSBR.U == 0)
        {
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                ;
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
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(A):SDW0.F == 0! Initiating directed fault\n");
            
            // initiate a directed fault ...
            ;
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
    // Yes...
    RSDWH_R1 = SDW->R1;
    
B:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(B)\n");
    
    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(ACV0);
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV3 = ORB
        acvFault(ACV3);
    
    if (SDW->R)
        goto G;
    
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
        //Set fault ACV4 = R-OFF
        acvFault(ACV4);
    
    goto G;
    
    
G:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(G)\n");
    
    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        ;
    
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
            ;
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;
    
H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);
    
    finalAddress = SDW->ADDR + TPR.CA;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(H:FANP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
    goto J;
    
I:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(I)\n");
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;
    
    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    core_read(finalAddress, readData);  // I think now is the time to do it ...
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(I:FAP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
    
    goto J;
    
J:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) ISITS(%d) ISITP(%d)\n", ISITS(*readData), ISITP(*readData));
    
    //if ((Tdes == TM_IR || Tdes == TM_RI) && (ISITS(*readData) || ISITP(*readData)))
    //    doITSITP(*readData, Tdes);
    if ((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITS(*readData) || ISITP(*readData)))
        doITSITP(i, *readData, Tag);
    
    else
    {
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Exit (Non-ITS/ITP): finalAddress=%08o readData=%012o\n", finalAddress, *readData);
        return finalAddress;
    }
    
    /// at this point CA, TBR, TRR, TSR, & Tdes are set up to read from where the ITS/ITP pointer said ....
    //
    
    word36 newwrd = (TPR.CA << 18) | rTAG;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Exit (ITS/ITP): newwrd=%012o\n", newwrd);
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendIndirectRead(J) Need to read from CA:%6o FA:%08o\n", TPR.CA, finalAddress);
    
    *readData = newwrd;
    return finalAddress;
}


word36 doAppendCycle(DCDstruct *i, MemoryAccessType accessType, word6 Tag, word36 writeData, word36 *readData)
{
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry): accessType=%s IWB=%012lo A=%d\n", strAccessType(accessType), i->IWB, GET_A(i->IWB));
    
    word36 fa = 0;
    switch (accessType)
    {
        case InstructionFetch:
            fa = doAppendInstructionFetch(i, readData);
            break;
        case DataRead:
            fa = doAppendDataRead(i, readData, true); //  a data read
            break;
        case OperandRead:
            fa = doAppendDataRead(i, readData, false); // an operand read
            break;
        case DataWrite:// XXX will need to do something similiar to the read ops here
            fa = doAppendDataWrite(i, writeData, true);
            break;
        case OperandWrite:
            fa = doAppendDataWrite(i, writeData, false);
            break;
        case IndirectRead:
            fa = doAppendIndirectRead(i, readData, Tag);
            break;
        default:
            fprintf(stderr,  "doAppendCycle(Entry): unsupported accessType=%s\n", strAccessType(accessType));
            return 0;
    }
    didITSITP = false;
    return fa;
}

/// delete when ready
#if SAFE
void doAppendCycleFig8p1(MemoryAccessType accessType, word36 writeData, word36 *readData)    ///< from AL39, Figure 8-1
{
    static MemoryAccessType lastAccessType = Unknown;

    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry): accesType=%s lastAccessType=%s\n", strAccessType(accessType), strAccessType(lastAccessType));
    
    
    word3 RSDWH_R1; ///< I think 
    int n = 0;      ///< PR[n]
    
    // start append
    acvFaults = 0; // no acv faults (yet)
    goto A;
    
    /* 
     * Was the last cycle an indirect word fetch?
     */
    if (lastAccessType == IndirectRead)
        goto A; // Yes, an indirect word fetch......
    // no
    
    if (lastAccessType == RTCDOperand)
        goto A; // Yes, an RTCD operand fetch
    // no
    
    if (lastAccessType != InstructionFetch)
    { // no
        if (GET_A(IWB)) // bin-29 on?
        { // yes
            n = GET_PRN(IWB);
            if(PR[n].RNR > PPR.PRR)
                TPR.TRR = PR[n].RNR;
            else
                TPR.TRR = PPR.PRR;
            TPR.TSR = PR[n].SNR;
            goto A;
        }
    }
    // either sequential instruction fetch or bit-29 not on...
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;

    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(Entry) TPR.TRR=%o TPR.TSR=%o\n", TPR.TRR, TPR.TSR);

A:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A)\n");

    // is SDW for C(TPR.TSR) in SDWAM?
    if (!fetchSDWfromSDWAM(TPR.TSR))
    {
        // No
        
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A):SDW for segment %05o not in SDWAM\n", TPR.TSR);

        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A):DSBR.U=%o\n", DSBR.U);

        if (DSBR.U == 0)
        {            
            appendingUnitCycleType = DSPTW; // Descriptor segment PTW fetch
            fetchDSPTW(TPR.TSR);
            
            if (!PTW0.F)
                // XXX initiate a directed fault
                ;
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
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(A):SDW0.F == 0! Initiating directed fault\n");

            // initiate a directed fault ...
            ;
        }
        else
            // load SDWAM .....
            loadSDWAM(TPR.TSR);
    }
    
    // Yes...
    RSDWH_R1 = SDW->R1;

B:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B)\n");

    //C(SDW.R1) ≤ C(SDW.R2) ≤ C(SDW .R3)?
    if (!(SDW->R1 <= SDW->R2 && SDW->R2 <= SDW->R3))
        // Set fault ACV0 = IRO
        acvFault(ACV0);
    
    if (lastAccessType == RTCDOperand)
        goto C;
    
    // is OPCODE call6?
    //if (GET_OP(IWB) == 0713 && !GET_OPX(IWB))    // XXX need to check for EIS?
    if (iwb->flags & CALL6_INS)
        goto E;
    
    // XXX current or last? How can iwb be set if this is the 1st instructionfetch?
    if (lastAccessType == InstructionFetch || iwb->flags & TRANSFER_INS)
        goto F;
    
    //if (isSTROP(IWB))
    if (accessType == OperandWrite)
    {
        // Yes ...
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(B): a STR-OP\n");

        
        // C(TPR.TRR) > C(SDW .R2)?
        if (TPR.TRR > SDW->R2)
            //Set fault ACV5 = OWB
            acvFault(ACV5);
        
        if (!SDW->W)
            // Set fault ACV6 = W-OFF
            acvFault(ACV6);
        goto G;
    }
    
    // No
    // C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        //Set fault ACV3 = ORB
        acvFault(ACV3);
    
    if (SDW->R)
        goto G;
        
    //C(PPR.PSR) = C(TPR.TSR)?
    if (PPR.PSR != TPR.TSR)
        //Set fault ACV4 = R-OFF
        acvFault(ACV4);
    
    goto G;

C:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(C)\n");
    
    // rtcd operand
    // C(TPR.TRR) < C(SDW .R1)?
    if (TPR.TRR < SDW->R1)
        acvFault(ACV1);
    
    // C(TPR.TRR) > C(SDW .R2)
    if (TPR.TRR > SDW->R2)
        acvFault(ACV1);
    
    if (!SDW->E)
        acvFault(ACV2);

    // C(TPR.TRR) ≥ C(PPR.PRR)
    if (!(TPR.TRR >= PPR.PRR))
        acvFault(ACV11);
        
D:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(D)\n");

    // instruction fetch
    if (rRALR == 0)
        goto G;
    
    // C(PPR.PRR) < RALR?
    if (!(PPR.PRR < rRALR))
        acvFault(ACV13);
    
    goto G;
    
E:; ///< call6
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(E) : call6\n");

    if (!SDW->E)
        acvFault(ACV2);

    if (!SDW->G)
    {
        // C(PPR.PSR) = C(TPR.TSR)?
        if (PPR.PSR != TPR.TSR)
        {
             // C(TPR.CA)4,17 >= SDW.CL?
            if ((TPR.CA & 037777) >=  SDW->CL)
                acvFault(ACV7);
        }
    }
    
    //C(TPR.TRR) > SDW.R3?
    if (TPR.TRR > SDW->R3)
        acvFault(ACV8);
    
    //￼￼￼C(TPR.TRR) < SDW.R1?
    if (TPR.TRR < SDW->R1)
        acvFault(ACV9);

    // C(TPR.TRR) > C(PPR.PRR)?
    if (TPR.TRR > PPR.PRR)
    {
        // C(PPR.PRR) < SDW.R2?
        if (PPR.PRR < SDW->R2)
            acvFault(ACV10);
    }
    
    //C(TPR.TRR) > SDW.R2?
    if (TPR.TRR > SDW->R2)
        TPR.TRR = SDW->R2;
    
    goto G;
    
F:; ///< transfer or instruction fetch
  
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(F): transfer or instruction fetch\n");

    // C(TPR.TRR) < C(SDW .R1)?
    if (TPR.TRR < SDW->R1)
        acvFault(ACV1);
    
    //C(TPR.TRR) > C(SDW .R2)?
    if (TPR.TRR > SDW->R2)
        acvFault(ACV1);

    //SDW .E set ON?
    if (!SDW->E)
        acvFault(ACV2);
    
    //C(PPR.PRR) = C(TPR.TRR)?
    if (PPR.PRR == TPR.TRR)
        goto D;
    
    acvFault(ACV12);

G:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(G)\n");

    //C(TPR.CA)0,13 > SDW.BOUND?
    if (((TPR.CA >> 4) & 037777) > SDW->BOUND)
        acvFault(ACV15);
    
    if (acvFaults)
        // Initiate an access violation fault
        ;
    
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
            ;
        
        loadPTWAM(SDW->POINTER, TPR.CA);    // load PTW0 to PTWAM
    }
    
    // is prepage mode???
    // XXX: don't know what todo with this yet ...
    
    goto I;

H:; ///< Final address nonpaged
    
    appendingUnitCycleType = FANP;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H): SDW->ADDR=%08o TPR.CA=%06o \n", SDW->ADDR, TPR.CA);

    finalAddress = SDW->ADDR + TPR.CA;
    
    if (accessType == DataWrite || accessType == OperandWrite)  // XXX not sure about operand write
    {
        core_write(finalAddress, writeData);  // I think now is the time to do it ...
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H:FANP) Write: finalAddress=%08o writeData=%012o\n", finalAddress, writeData);
        
    }
    //XXX how about RTCD operand???
    if (accessType == DataRead || accessType == IndirectRead || accessType == InstructionFetch)  // XXX not sure about operand write
    {
        core_read(finalAddress, readData);  // I think now is the time to do it ...
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(H:FANP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
        
        if (accessType == InstructionFetch)
            iwb = getIWBInfo();

    }
    goto I2;
    
I:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(I)\n");

    //if (isSTROP(IWB) && PTW->M == 0)
    if (accessType == OperandWrite && PTW->M == 0)
    {
        appendingUnitCycleType = MPTW;
        modifyPTW(SDW, TPR.CA);
    }
    
    // final address paged
    appendingUnitCycleType = FAP;
    
    word24 y2 = TPR.CA % 1024;

    finalAddress = ((PTW->ADDR << 6) & 037777) + y2;
    
    //core_read(finalAddress, data);  // I think now is the time to do it ...
    if (accessType == DataWrite || accessType == OperandWrite)  // XXX not sure about operand write
    {
        core_write(finalAddress, writeData);  // I think now is the time to do it ...
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(I:FAP) Write: finalAddress=%08o writeData=%012o\n", finalAddress, writeData);

    }
    //XXX how about RTCD operand???
    if (accessType == DataRead || accessType == IndirectRead || accessType == InstructionFetch)  // XXX not sure about operand write
    {
        core_read(finalAddress, readData);  // I think now is the time to do it ...
        if (apndTrace)
            sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(I:FAP) Read: finalAddress=%08o readData=%012o\n", finalAddress, *readData);
        
        //if (accessType == InstructionFetch)
        //    iwb = getIWBInfo();
    }
    
    
    
I2:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(I2)\n");

    if (accessType == IndirectRead)
        goto J;

    if (accessType == RTCDOperand)
        goto K;
   
    //if (GET_OP(IWB) == 0713 && !GET_OPX(IWB))    // XXX need to check for EIS?
    if (iwb->flags & CALL6_INS)
        goto N;
   
    if (accessType == InstructionFetch || iwb->flags & TRANSFER_INS)
        goto L;
    
    // APU data movement?
    if (isAPUDataMovement(IWB))
    {
        // XXX finish when we figure out what this is
    }
    
    appendingUnitCycleType = APPUNKNOWN;
    
    lastAccessType = accessType;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(I2):Exit\n");

    return;
    
J:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(J)\n");

    if ((Tm == TM_RI || Tm == TM_IR) && ((TPR.CA & 1) == 0))
    {
        if (ISITS(CY))
            goto O;
        if (ISITP(CY))
            goto P;
    }
    // some other indirect? (Thanks Gary!)
    if (Tm == TM_RI || Tm == TM_IR || Tm == TM_IT)
    {
        SETHI(IWB, GETHI(CY));  // C(Y)0,17 → C(IWB)0,17
        IWB &= 0777777777700;   // C(Y)30,35 → C(IWB)30,35
        IWB |= (CY & 077);
        IWB = CLRBIT(IWB, 6);   // 0 → C(IWB)29
    }
   
    lastAccessType = accessType;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(J): Exit\n");

    return;
    
K:;

    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(K)\n");

    TPR.TSR = GETHI(CY) & 077777;   // C(Y)3,17 → C(TPR.TSR)
    TPR.CA = GETHI(CY1);            // C(Y+1)0,17 → C(TPR.CA)

    // C(TPR.TRR) ≥ C(PPR.PRR)?
    if (TPR.TRR >= PPR.PRR)
        // ￼C(TPR.TRR) → C(PRi .RNR) for i = 0, 7
        for(int i = 0 ; i < 8 ; i++)
            PR[i].RNR = TPR.TRR;

    PPR.PRR = TPR.TRR;
    
    goto L2;
    
L:; ///< is opcode tspn?
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(L)\n");

    if (iwb->flags & TSPN_INS)
    {
        //C(PPR.PRR) → C(PRn .RNR)
        //C(PPR.PSR) → C(PRn .SNR)
        //C(PPR.IC) → C(PRn .WORDNO)
        //000000 → C(PRn .BITNO)
        
        PR[n].RNR = PPR.PRR;
        PR[n].SNR = PPR.PSR;
        PR[n].WORDNO = PPR.IC;
        PR[n].BITNO = 0;
    }
L2:;
    //C(TPR.TSR) → C(PPR.PSR)
    //C(TPR.CA) → C(PPR.IC)
    PPR.PSR = TPR.TSR;
    PPR.IC = TPR.CA;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(L2): IC set to %08o\n", TPR.CA);

M:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M)\n");

    //C(TPR.TRR)= 0?
    if (TPR.TRR == 0)
        //C(SDW.P) → C(PPR.P)
        PPR.P = SDW->P;
    else
        PPR.P = 0;
    
    if (accessType == RTCDOperand)
        goto O;
    
    lastAccessType = accessType;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(M): Exit\n");

    return;
    
N:;
   
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(N)\n");

    // C(TPR.TRR) = C(PPR.PRR)?
    if (TPR.TRR == PPR.PRR)
        //C(PR6.SNR) → C(PR7.SNR)
        PR[7].SNR = PR[6].SNR;
    else
        // C(DSBR.STACK) || C(TPR.TRR) → C(PR7.SNR)
        PR[7].SNR = (DSBR.STACK << 3) | (TPR.TRR & 3);

    // C(TPR.TRR) → C(PR7.RNR)
    // 00...0 → C(PR7.WORDNO)
    // 000000 → C(PR7.BITNO)
    // C(TPR.TRR) → C(PPR.PRR)
    // C(TPR.TSR) → C(PPR.PSR)
    // C(TPR.CA) → C(PPR.IC)
    
    PR[7].RNR = TPR.TRR;
    PR[7].WORDNO = 0;
    PR[7].BITNO = 0;
    PPR.PRR = TPR.TSR;
    PPR.PSR = TPR.TSR;
    PPR.IC = TPR.CA;
    
    goto M;

O:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(O)\n");

    word3 tmp = (GETLO(CY) >> 15) & 3;
    
    // C(TPR.TRR) ≥ RSDWH.R1?
    if (TPR.TRR >= RSDWH_R1)
    {
        // C(TPR.TRR) ≥ C(Y)18,20?
        if (TPR.TRR >= tmp)
        {
            lastAccessType = accessType;
            
            if (apndTrace)
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(O): Exit 1\n");

            return;
        }

        //C(Y)18,20 → C(TPR.TRR)
        TPR.TRR = tmp;
    } else {
        // C(Y)18,20 ≥ RSDWH.R1?
        if (tmp >= RSDWH_R1)
            TPR.TRR = tmp;
        else
            TPR.TRR = RSDWH_R1;
    }
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(O): Exit 2\n");

    lastAccessType = accessType;
    return;
    
P:;
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(P)\n");

    // C(TPR.TRR) ≥ RSDWH.R1?
    if (TPR.TRR >= RSDWH_R1)
    {
        // C(TPR.TRR) ≥ C(PRn .RNR)?
        if (TPR.TRR >= PR[n].RNR)
        {
            lastAccessType = accessType;

            if (apndTrace)
                sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(P): Exit 1\n");

            return;
        }
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
    
    lastAccessType = accessType;
    
    if (apndTrace)
        sim_debug(DBG_APPENDING, &cpu_dev, "doAppendCycle(P): Exit 2\n");

    return;
}
#endif
