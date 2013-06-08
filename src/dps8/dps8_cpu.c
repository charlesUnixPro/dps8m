/**
 * \file dps8_cpu.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"

a8 saved_PC = 0;
int32 flags = 0;

enum _processor_cycle_type processorCycle;  ///< to keep tract of what type of cycle the processor is in
enum _processor_addressing_mode processorAddressingMode;    ///< what addressing mode are we using

// h6180 stuff
/* [map] designates mapping into 36-bit word from DPS-8 proc manual */

/* GE-625/635 */

word36	rA;	/*!< accumulator */
word36	rQ;	/*!< quotient */
word8	rE;	/*!< exponent [map: rE, 28 0's] */

word18	rX[8];	/*!< index */

//word18	rBAR;	/*!< base address [map: BAR, 18 0's] */
/* format: 9b base, 9b bound */

int XECD; /*!< for out-of-line XEC,XED,faults, etc w/o rIC fetch */
word36 XECD1; /*!< XEC instr / XED instr#1 */
word36 XECD2; /*!< XED instr#2 */


//word18	rIC;	/*!< instruction counter */
// same as PPR.IC
word18	rIR;	/*!< indicator [15b] [map: 18 x's, rIR w/ 3 0's] */
word27	rTR;	/*!< timer [map: TR, 9 0's] */

word24	rY;     /*!< address operand */
word8	rTAG;	/*!< instruction tag */

word8	tTB;	/*!< char size indicator (TB6=6-bit,TB9=9-bit) [3b] */
word8	tCF;	/*!< character position field [3b] */

/* GE-645 */


//word36	rDBR;	/*!< descriptor base */
//!<word27	rABR[8];/*!< address base */
//
///* H6180; L68; DPS-8M */
//
word8	rRALR;	/*!< ring alarm [3b] [map: 33 0's, RALR] */
//word36	rPRE[8];/*!< pointer, even word */
//word36	rPRO[8];/*!< pointer, odd word */
//word27	rAR[8];	/*!< address [24b] [map: ARn, 12 0's] */
//word36	rPPRE;	/*!< procedure pointer, even word */
//word36	rPPRO;	/*!< procedure pointer, odd word */
//word36	rTPRE;	/*!< temporary pointer, even word */
//word36	rTPRO;	/*!< temporary pointer, odd word */
//word36	rDSBRE;	/*!< descriptor segment base, even word */
//word36	rDSBRO;	/*!< descriptor segment base, odd word */
//word36	mSE[16];/*!< word-assoc-mem: seg descrip regs, even word */
//word36	mSO[16];/*!< word-assoc-mem: seg descrip regs, odd word */
//word36	mSP[16];/*!< word-assoc-mem: seg descrip ptrs */
//word36	mPR[16];/*!< word-assoc-mem: page tbl regs */
//word36	mPP[16];/*!< word-assoc-mem: page tbl ptrs */
//word36	rFRE;	/*!< fault, even word */
//word36	rFRO;	/*!< fault, odd word */
//word36	rMR;	/*!< mode */
//word36	rCMR;	/*!< cache mode */
//word36	hCE[16];/*!< history: control unit, even word */
//word36	hCO[16];/*!< history: control unit, odd word */
//word36	hOE[16];/*!< history: operations unit, even word */
//word36	hOO[16];/*!< history: operations unit, odd word */
//word36	hDE[16];/*!< history: decimal unit, even word */
//word36	hDO[16];/*!< history: decimal unit, odd word */
//word36	hAE[16];/*!< history: appending unit, even word */
//word36	hAO[16];/*!< history: appending unit, odd word */
//word36	rSW[5];	/*!< switches */

word12 rFAULTBASE;  ///< fault base (12-bits of which the top-most 7-bits are used)

// end h6180 stuff

struct _tpr TPR;    ///< Temporary Pointer Register
struct _ppr PPR;    ///< Procedure Pointer Register
//struct _prn PR[8];  ///< Pointer Registers
//struct _arn AR[8];  ///< Address Registers
struct _par PAR[8]; ///< pointer/address resisters
struct _bar BAR;    ///< Base Address Register
struct _dsbr DSBR;  ///< Descriptor Segment Base Register


// XXX given this is not real hardware we can eventually remove the SDWAM -- I think. But for now just leave it in.
// For the DPS 8M processor, the SDW associative memory will hold the 64 MRU SDWs and have a 4-way set associative organization with LRU replacement.

struct _sdw  SDWAM[64], *SDW = &SDWAM[0];    ///< Segment Descriptor Word Associative Memory & working SDW
struct _sdw0 SDW0;  ///< a SDW not in SDWAM

struct _ptw PTWAM[64], *PTW = &PTWAM[0];    ///< PAGE TABLE WORD ASSOCIATIVE MEMORY and working PTW
struct _ptw0 PTW0;  ///< a PTW not in PTWAM (PTWx1)


/*
 * register stuff ...
 */
const char *z1[] = {"0", "1"};
BITFIELD dps8_IR_bits[] = {    
    BITNCF(3),
    BITFNAM(HEX,   1, z1),    /*!< base-16 exponent */ ///< 0000010
    BITFNAM(ABS,   1, z1),    /*!< absolute mode */ ///< 0000020
    BITFNAM(MIIF,  1, z1),	  /*!< mid-instruction interrupt fault */ ///< 0000040
    BITFNAM(TRUNC, 1, z1),    /*!< truncation */ ///< 0000100
    BITFNAM(NBAR,  1, z1),	  /*!< not BAR mode */ ///< 0000200
    BITFNAM(PMASK, 1, z1),	  /*!< parity mask */ ///< 0000400
    BITFNAM(PAR,   1, z1),    /*!< parity error */ ///< 0001000
    BITFNAM(TALLY, 1, z1),	  /*!< tally runout */ ///< 0002000
    BITFNAM(OMASK, 1, z1),    /*!< overflow mask */ ///< 0004000
    BITFNAM(EUFL,  1, z1),	  /*!< exponent underflow */ ///< 0010000
    BITFNAM(EOFL,  1, z1),	  /*!< exponent overflow */ ///< 0020000
    BITFNAM(OFLOW, 1, z1),	  /*!< overflow */ ///< 0040000
    BITFNAM(CARRY, 1, z1),	  /*!< carry */ ///< 0100000
    BITFNAM(NEG,   1, z1),    /*!< negative */ ///< 0200000
    BITFNAM(ZERO,  1, z1),	  /*!< zero */ ///< 0400000
    ENDBITS
};

REG cpu_reg[] = {
    { ORDATA (IC, rIC, VASIZE) },
    //{ ORDATA (IR, rIR, 18) },
    { ORDATADF (IR, rIR, 18, "Indicator Register", dps8_IR_bits) },
    
    //    { FLDATA (Zero, rIR, F_V_A) },
    //    { FLDATA (Negative, rIR, F_V_B) },
    //    { FLDATA (Carry, rIR, F_V_C) },
    //    { FLDATA (Overflow, rIR, F_V_D) },
    //    { FLDATA (ExpOvr, rIR, F_V_E) },
    //    { FLDATA (ExpUdr, rIR, F_V_F) },
    //    { FLDATA (OvrMask, rIR, F_V_G) },
    //    { FLDATA (TallyRunOut, rIR, F_V_H) },
    //    { FLDATA (ParityErr, rIR, F_V_I) }, ///< Yeah, right!
    //    { FLDATA (ParityMask, rIR, F_V_J) },
    //    { FLDATA (NotBAR, rIR, F_V_K) },
    //    { FLDATA (Trunc, rIR, F_V_L) },
    //    { FLDATA (MidInsFlt, rIR, F_V_M) },
    //    { FLDATA (AbsMode, rIR, F_V_N) },
    //    { FLDATA (HexMode, rIR, F_V_O) },
    
    { ORDATA (A, rA, 36) },
    { ORDATA (Q, rQ, 36) },
    { ORDATA (E, rE, 8) },
    
    { ORDATA (X0, rX[0], 18) },
    { ORDATA (X1, rX[1], 18) },
    { ORDATA (X2, rX[2], 18) },
    { ORDATA (X3, rX[3], 18) },
    { ORDATA (X4, rX[4], 18) },
    { ORDATA (X5, rX[5], 18) },
    { ORDATA (X6, rX[6], 18) },
    { ORDATA (X7, rX[7], 18) },
    
    { ORDATA (PPR.IC,  PPR.IC,  18) },
    { ORDATA (PPR.PRR, PPR.PRR,  3) },
    { ORDATA (PPR.PSR, PPR.PSR, 15) },
    { ORDATA (PPR.P,   PPR.P,    1) },
    
    { ORDATA (DSBR.ADDR,  DSBR.ADDR,  24) },
    { ORDATA (DSBR.BND,   DSBR.BND,   14) },
    { ORDATA (DSBR.U,     DSBR.U,      1) },
    { ORDATA (DSBR.STACK, DSBR.STACK, 12) },
    
    { ORDATA (BAR.BASE,  BAR.BASE,  9) },
    { ORDATA (BAR.BOUND, BAR.BOUND, 9) },
    
    { ORDATA (FAULTBASE, rFAULTBASE, 12) }, ///< only top 7-msb are used
    
    //{ ORDATA (PR0, PR[0], 18) },
    //{ ORDATA (PR1, PR[1], 18) },
    //{ ORDATA (PR2, PR[2], 18) },
    //{ ORDATA (PR3, PR[3], 18) },
    //{ ORDATA (PR4, PR[4], 18) },
    //{ ORDATA (PR5, PR[5], 18) },
    //{ ORDATA (PR6, PR[6], 18) },
    //{ ORDATA (PR7, PR[7], 18) },
    
    
    //  { ORDATA (BAR, rBAR, 18) },
    
    /*
     { ORDATA (EBR, ebr, EBR_N_EBR) },
     { FLDATA (PGON, ebr, EBR_V_PGON) },
     { FLDATA (T20P, ebr, EBR_V_T20P) },
     { ORDATA (UBR, ubr, 36) },
     { GRDATA (CURAC, ubr, 8, 3, UBR_V_CURAC), REG_RO },
     { GRDATA (PRVAC, ubr, 8, 3, UBR_V_PRVAC) },
     { ORDATA (SPT, spt, 36) },
     { ORDATA (CST, cst, 36) },
     { ORDATA (PUR, pur, 36) },
     { ORDATA (CSTM, cstm, 36) },
     { ORDATA (HSB, hsb, 36) },
     { ORDATA (DBR1, dbr1, PASIZE) },
     { ORDATA (DBR2, dbr2, PASIZE) },
     { ORDATA (DBR3, dbr3, PASIZE) },
     { ORDATA (DBR4, dbr4, PASIZE) },
     { ORDATA (PCST, pcst, 36) },
     { ORDATA (PIENB, pi_enb, 7) },
     { FLDATA (PION, pi_on, 0) },
     { ORDATA (PIACT, pi_act, 7) },
     { ORDATA (PIPRQ, pi_prq, 7) },
     { ORDATA (PIIOQ, pi_ioq, 7), REG_RO },
     { ORDATA (PIAPR, pi_apr, 7), REG_RO },
     { ORDATA (APRENB, apr_enb, 8) },
     { ORDATA (APRFLG, apr_flg, 8) },
     { ORDATA (APRLVL, apr_lvl, 3) },
     { ORDATA (RLOG, rlog, 10) },
     { FLDATA (F1PR, its_1pr, 0) },
     { BRDATA (PCQ, pcq, 8, VASIZE, PCQ_SIZE), REG_RO+REG_CIRC },
     { ORDATA (PCQP, pcq_p, 6), REG_HRO },
     { DRDATA (INDMAX, ind_max, 8), PV_LEFT + REG_NZ },
     { DRDATA (XCTMAX, xct_max, 8), PV_LEFT + REG_NZ },
     { ORDATA (WRU, sim_int_char, 8) },
     { FLDATA (STOP_ILL, stop_op0, 0) },
     { BRDATA (REG, acs, 8, 36, AC_NUM * AC_NBLK) },
     */
    
    { NULL }
};

//word36 IWB;         ///< instruction working buffer
//opCode *iwb = NULL; ///< opCode *
//int32  opcode;      ///< opcode
//bool   opcodeX;     ///< opcode extension
//word18 address;     ///< bits 0-17 of instruction XXX replace with rY
//bool   a;           ///< bin-29 - indirect addressing mode?
//bool   i;           ///< interrupt inhinit bit.
//word6  tag;         ///< instruction tag XXX replace with rTAG

//word18 stiTally;    ///< for sti instruction


DCDstruct *decodeInstruction(word36 inst, DCDstruct *dst);     // decode instruction into structure
DCDstruct *fetchInstruction(word18 addr, DCDstruct *dst);      // fetch (+ decode) instrcution at address

t_stat doInstruction(DCDstruct *i);
t_stat DoBasicInstruction(DCDstruct *); //word36 instruction);
t_stat DoEISInstruction(DCDstruct *);   //word36 instruction);
t_stat executeInstruction(DCDstruct *ci);

t_stat reason;

t_uint64 cpuCycles = 0; ///< # of instructions executed in this run...

jmp_buf jmpMain;        ///< This is where we should return to from a fault or interrupt (if necessary)

DCDstruct _currentInstruction;
DCDstruct *currentInstruction  = &_currentInstruction;;

EISstruct E;
//EISstruct *e = &E;

t_stat sim_instr (void)
{
    extern int32 sim_interval;
    
    /* Main instruction fetch/decode loop */
    adrTrace  = (cpu_dev.dctrl & DBG_ADDRMOD  ) && sim_deb; // perform address mod tracing
    apndTrace = (cpu_dev.dctrl & DBG_APPENDING) && sim_deb; // perform APU tracing
    
    //currentInstruction = &_currentInstruction;
    currentInstruction->e = &E;
    
    int val = setjmp(jmpMain);    // here's our main fault/interrupt return. Back to executing instructions....
    if (val)
    {
        // if we're here, we're returning from a fault etc.....
        
    } else {
        reason = 0;
        cpuCycles = 0;  // XXX This probably needs to be moved so our fault handler won't reset cpuCycles back to 0
    }
    
    do {
        /* loop until halted */
        if (sim_interval <= 0) {                                /* check clock queue */
            if ((reason = sim_process_event ()))
                break;
        }
        
        if (sim_brk_summ && sim_brk_test (rIC, SWMASK ('E'))) {    /* breakpoint? */
            reason = STOP_BKPT;                        /* stop simulation */
            break;
        }
        
        // fetch instruction
        processorCycle = SEQUENTIAL_INSTRUCTION_FETCH;
        
        DCDstruct *ci = fetchInstruction(rIC, currentInstruction);    // fetch next instruction into current instruction struct
        
        // XXX: what if sim stops during XEC/XED? if user wants to re-step
        // instruc, is this logic OK?
        if(XECD == 1) {
          ci->IWB = XECD1;
        } else if(XECD == 2) {
          ci->IWB = XECD2;
        }
        
        t_stat ret = executeInstruction(ci);
        
        if (ret)
        {
            if (ret > 0)
            {
                reason = ret;
                break;
            } else {
                switch (ret)
                {
                    case CONT_TRA:
                        continue;   // don't bump rIC, instruction already did it
                }
            }
        }
        
        if (ci->opcode == 0616) { // DIS
            reason = STOP_DIS;
            break;
        }
        
        // doesn't seem to work as advertized
        if (sim_poll_kbd())
            reason = STOP_BKPT;
       
        // XXX: what if sim stops during XEC/XED? if user wants to re-step
        // instruc, is this logic OK?
        if(XECD == 1) {
          XECD = 2;
        } else if(XECD == 2) {
          XECD = 0;
        } else
          rIC += 1;
        
        // is this a multiword EIS?
        // XXX: no multiword EIS for XEC/XED/fault, right?? -MCW
        if (ci->iwb->ndes > 0)
          rIC += ci->iwb->ndes;
        
    } while (reason == 0);
    
    fprintf(stdout, "\r\ncpuCycles = %lld\n", cpuCycles);
    
    return reason;
}


static uint32 bkpt_type[4] = { SWMASK ('E') , SWMASK ('N'), SWMASK ('R'), SWMASK ('W') };

extern bool doITSITP(word36 indword, word4 Tdes);

#define DOITSITP(indword, Tag) ((_TM(Tag) == TM_IR || _TM(Tag) == TM_RI) && (ISITP(indword) || ISITS(indword)))
//bool DOITSITP(indword, Tag)
//{
//    return ((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITP(indword) || ISITS(indword)));
//}


/*!
 * the Read, Write functions access main memory, but optionally calls the appending unit to
 * determine the actual memory address
 */
t_stat Read (DCDstruct *i, word24 addr, word36 *dat, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else {
        //*dat = M[addr];
        rY = addr;
        TPR.CA = addr;  //XXX for APU
        
        switch (processorAddressingMode)
        {
            case APPEND_MODE:
APPEND_MODE:;
                doAppendCycle(i, acctyp, Tag, -1, dat);
                
                //word24 fa = doFinalAddressCalculation(acctyp, TPR.TSR, TPR.CA, &acvf);
                //if (fa)
                //    core_read(fa, dat);
                
                //rY = finalAddress;
                //*dat = CY;  // XXX this may be a nasty loop
                break;
            case ABSOLUTE_MODE:
                core_read(addr, dat);
                if (acctyp == IndirectRead && DOITSITP(*dat, Tag))
                {
                    if (apndTrace)
                        sim_debug(DBG_APPENDING, &cpu_dev, "Read(%06o %012llo %02o): going into APPENDING mode\n", addr, *dat, Tag);

                    processorAddressingMode = APPEND_MODE;
                    goto APPEND_MODE;   // ???
                }
                break;
            case BAR_MODE:
                // XXX probably not right.
                rY = getBARaddress(addr);
                core_read(rY, dat);
                return SCPE_OK;
            default:
                fprintf(stderr, "Read(): acctyp\n");
                break;
        }
        
    }
    return SCPE_OK;
}

t_stat Write (DCDstruct *i, word24 addr, word36 dat, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
    {
        rY = addr;
        TPR.CA = addr;  //XXX for APU
        
        switch (processorAddressingMode)
        {
            case APPEND_MODE:
APPEND_MODE:;
                doAppendCycle(i, acctyp, Tag, dat, NULL);    // SXXX should we have a tag value here for RI, IR ITS, ITP, etc or is 0 OK
                //word24 fa = doFinalAddressCalculation(acctyp, TPR.TSR, TPR.CA, &acvf);
                //if (fa)
                //    core_write(fa, dat);
                
                break;
            case ABSOLUTE_MODE:
                core_write(addr, dat);
                //if (doITSITP(dat, GET_TD(Tag)))
                // XXX what kind of dataop can put a write operation into appending mode?
                //if (DOITSITP(dat, Tag))
                //{
                //   processorAddressingMode = APPEND_MODE;
                //    goto APPEND_MODE;   // ???
                //}
                break;
            case BAR_MODE:
                // XXX probably not right.
                rY = getBARaddress(addr);
                core_write(rY, dat);
                return SCPE_OK;
            default:
                fprintf(stderr, "Write(): acctyp\n");
                break;
        }
        
        
    }
    return SCPE_OK;
}

t_stat Read2 (DCDstruct *i, word24 addr, word36 *datEven, word36 *datOdd, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else {        
        // need to check for even/odd?
        if (addr & 1)
        {
            addr &= ~1; /* make it an even address */
            addr &= DMASK;
        }
        Read(i, addr + 0, datEven, acctyp, Tag);
        Read(i, addr + 1, datOdd, acctyp, Tag);
        //printf("read2: addr=%06o\n", addr);
    }
    return SCPE_OK;
}
t_stat Write2 (DCDstruct *i, word24 addr, word36 datEven, word36 datOdd, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else {
        // need to check for even/odd?
        if (addr & 1)
        {
            addr &= ~1; /* make it an even address */
            addr &= DMASK;
        }
        Write(i, addr + 0, datEven, acctyp, Tag);
        Write(i, addr + 1, datOdd,  acctyp, Tag);
        
        //printf("write2: addr=%06o\n", addr);

    }
    return SCPE_OK;
}

t_stat Read72(DCDstruct *i, word24 addr, word72 *dst, enum eMemoryAccessType acctyp, int32 Tag) // needs testing
{
    word36 even, odd;
    t_stat res = Read2(i, addr, &even, &odd, acctyp, Tag);
    if (res != SCPE_OK)
        return res;
    
    *dst = ((word72)even << 36) | (word72)odd;
    return SCPE_OK;
}
t_stat ReadYPair (DCDstruct *i, word24 addr, word36 *Ypair, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else {
        // need to check for even/odd?
        if (addr & 1)
            addr &= ~1; /* make it an even address */
        Read(i, addr + 0, Ypair+0, acctyp, Tag);
        Read(i, addr + 1, Ypair+1, acctyp, Tag);
        
    }
    return SCPE_OK;
}

/*!
 cd@libertyhaven.com - sez ....
 If the instruction addresses a block of four words, the target of the instruction is supposed to be an address that is aligned on a four-word boundary (0 mod 4). If not, the processor will grab the four-word block containing that address that begins on a four-word boundary, even if it has to go back 1 to 3 words. Analogous explanation for 8, 16, and 32 cases.
 
 olin@olinsibert.com - sez ...
 It means that the appropriate low bits of the address are forced to zero. So it's the previous words, not the succeeding words, that are used to satisfy the request.
 
 -- Olin

 */
t_stat ReadN (DCDstruct *i, int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
        for (int j = 0 ; j < n ; j ++)
            Read(i, addr + j, Yblock + j, acctyp, Tag);
    
    return SCPE_OK;
}

//
// read N words in a non-aligned fashion for EIS
//
// XXX here is where we probably need to to the prepage thang...
t_stat ReadNnoalign (DCDstruct *i, int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
        for (int j = 0 ; j < n ; j ++)
            Read(i, addr + j, Yblock + j, acctyp, Tag);
    
    return SCPE_OK;
}

t_stat WriteN (DCDstruct *i, int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag)
{
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
        for (int j = 0 ; j < n ; j ++)
            Write(i, addr + j, Yblock[j], acctyp, Tag);
    
    return SCPE_OK;
}

/*!
 * "Raw" core interface ....
 */
int32 core_read(word24 addr, word36 *data)
{
    if(addr >= MEMSIZE) {
        *data = 0;
        return -1;
    } else {
        *data = M[addr] & DMASK;
    }
    return 0;
}

int core_write(word24 addr, word36 data) {
    if(addr >= MEMSIZE) {
        return -1;
    } else {
        M[addr] = data & DMASK;
    }
    return 0;
}

int core_read2(word24 addr, word36 *even, word36 *odd) {
    if(addr >= MEMSIZE) {
        *even = 0;
        *odd = 0;
        return -1;
    } else {
        if(addr & 1) {
            sim_debug(DBG_MSG, &cpu_dev,"warning: subtracting 1 from pair at %lo in core_read2\n", addr);
            addr &= ~1; /* make it an even address */
        }
        *even = M[addr++] & DMASK;
        *odd = M[addr] & DMASK;
        return 0;
    }
}

//! for working with CY-pairs
int core_read72(word24 addr, word72 *dst) // needs testing
{
    word36 even, odd;
    if (core_read2(addr, &even, &odd) == -1)
        return -1;
    *dst = ((word72)even << 36) | (word72)odd;
    return 0;
}

int core_write2(word24 addr, word36 even, word36 odd) {
    if(addr >= MEMSIZE) {
        return -1;
    } else {
        if(addr & 1) {
            sim_debug(DBG_MSG, &cpu_dev, "warning: subtracting 1 from pair at %lo in core_write2\n", addr);
            addr &= ~1; /* make it even a dress, or iron a skirt ;) */
        }
        M[addr++] = even;
        M[addr] = odd;
    }
    return 0;
}
//! for working with CY-pairs
int core_write72(word24 addr, word72 src) // needs testing
{
    word36 even = (word36)(src >> 36) & DMASK;
    word36 odd = ((word36)src) & DMASK;
    
    return core_write2(addr, even, odd);
}

int core_readN(word24 addr, word36 *data, int n)
{
    addr %= n;  // better be an even power of 2, 4, 8, 16, 32, 64, ....
    for(int i = 0 ; i < n ; i++)
        if(addr >= MEMSIZE) {
            *data = 0;
            return -1;
        } else {
            *data++ = M[addr++];
        }
    return 0;
}

int core_writeN(a8 addr, d8 *data, int n)
{
    addr %= n;  // better be an even power of 2, 4, 8, 16, 32, 64, ....
    for(int i = 0 ; i < n ; i++)
        if(addr >= MEMSIZE) {
            return -1;
        } else {
            M[addr++] = *data++;
        }
    return 0;
}

DCDstruct *newDCDstruct()
{
    DCDstruct *p = malloc(sizeof(DCDstruct));
    p->e = malloc(sizeof(EISstruct));
    
    return p;
}

void freeDCDstruct(DCDstruct *p)
{
    if (p->e)
        free(p->e);
    free(p);
}

/*
 * instruction fetcher ...
 * fetch + decode instruction at 18-bit address 'addr'
 */
DCDstruct *fetchInstruction(word18 addr, DCDstruct *i)  // fetch instrcution at address
{
    DCDstruct *p = (i == NULL) ? newDCDstruct() : i;

    Read(p, addr, &p->IWB, InstructionFetch, 0);
    
    return decodeInstruction(p->IWB, p);
}

/*
 * instruction decoder .....
 *
 * if dst is not NULL place results into dst, if dst is NULL plae results into global currentInstruction
 */
DCDstruct *decodeInstruction(word36 inst, DCDstruct *dst)     // decode instruction into structure
{
    DCDstruct *p = dst == NULL ? newDCDstruct() : dst;
    
    p->opcode  = GET_OP(inst);  // get opcode
    p->opcodeX = GET_OPX(inst); // opcode extension
    p->address = GET_ADDR(inst);// address field from instruction
    p->a       = GET_A(inst);   // "A" - the indirect via pointer register flag
    p->i       = GET_I(inst);   // inhibit interrupt flag
    p->tag     = GET_TAG(inst); // instruction tag
    
    p->iwb = getIWBInfo(p);     // get info for IWB instruction
    
    
    // is this a multiword EIS?
    if (p->iwb->ndes > 1)
    {
        memset(p->e, 0, sizeof(EISstruct)); // clear out e
        p->e->op0 = p->IWB;
        // XXX: for XEC/XED/faults, this should trap?? I think -MCW
        for(int n = 0 ; n < p->iwb->ndes; n += 1)
            //Read(p, rIC + 1 + n, &p->e->op[n], InstructionFetch, 0);
            Read(p, rIC + 1 + n, &p->e->op[n], OperandRead, 0); // I think.
    }
    //if (p->e)
    //    p->e->ins = p;    // Yes, it's a cycle
    
    return p;
}

/*
 * fault handler(s). move to seperate file - later after things are working properly
 */

void doFault(int faultNumber, int faultGroup, char *faultMsg)
{
    printf("fault: %d %d '%s'\r\n", faultNumber, faultGroup, faultMsg ? faultMsg : "?");
    
    // XXX we really only want to do this in extreme conditions since faults can be returned from *more-or-less*
    // XXX do it properly - later..
    //longjmp(jmpMain, faultNumber);
}

