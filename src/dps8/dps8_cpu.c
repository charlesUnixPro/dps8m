 /**
 * \file dps8_cpu.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"

//#define sim_instr_NEW sim_instr
#define sim_instr_HWR sim_instr

a8 saved_PC = 0;
int32 flags = 0;

enum _processor_cycle_type processorCycle;                  ///< to keep tract of what type of cycle the processor is in
//enum _processor_addressing_mode processorAddressingMode;    ///< what addressing mode are we using
enum _processor_operating_mode processorOperatingMode;      ///< what operating mode

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
IR_t IR;        // Indicator register   (until I can map MM IR to my rIR)


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

//word12 rFAULTBASE;  ///< fault base (12-bits of which the top-most 7-bits are used)

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

//t_uint64 cpuCycles = 0; ///< # of instructions executed in this run...
#define cpuCycles sys_stats.total_cycles

jmp_buf jmpMain;        ///< This is where we should return to from a fault or interrupt (if necessary)

DCDstruct _currentInstruction;
DCDstruct *currentInstruction  = &_currentInstruction;;

EISstruct E;
//EISstruct *e = &E;

void check_events();

events_t events;
switches_t switches;
cpu_ports_t cpu_ports; // Describes connections to SCUs
// the following two should probably be combined
cpu_state_t cpu;
ctl_unit_data_t cu;
stats_t sys_stats;

scu_t scu; // only one for now
iom_t iom; // only one for now

// This is an out-of-band flag for the APU. User commands to
// display or modify memory can invoke much the APU. Howeveer, we don't
// want interactive attempts to access non-existant memory locations
// to register a fault.
flag_t fault_gen_no_fault;

/*
 *  cancel_run()
 *
 *  Cancel_run can be called by any portion of the code to let
 *  sim_instr() know that it should stop looping and drop back
 *  to the SIMH command prompt.
 */

static int cancel;

void cancel_run(t_stat reason)
{
    // Maybe we should generate an OOB fault?
    
    (void) sim_cancel_step();
    if (cancel == 0 || reason < cancel)
        cancel = reason;
    //log_msg(DEBUG_MSG, "CU", "Cancel requested: %d\n", reason);
}



t_stat sim_instr_HWR (void)
{
    extern int32 sim_interval;
    
    /* Main instruction fetch/decode loop */
    adrTrace  = (cpu_dev.dctrl & DBG_ADDRMOD  ) && sim_deb; // perform address mod tracing
    apndTrace = (cpu_dev.dctrl & DBG_APPENDING) && sim_deb; // perform APU tracing
    unitTrace = (cpu_dev.dctrl & DBG_UNIT) && sim_deb;      // perform UNIT tracing
    
    //currentInstruction = &_currentInstruction;
    currentInstruction->e = &E;
    
    int val = setjmp(jmpMain);    // here's our main fault/interrupt return. Back to executing instructions....
    switch (val)
    {
        case 0:
            reason = 0;
            cpuCycles = 0;
            break;
        case JMP_NEXT:
            goto jmpNext;
        case JMP_RETRY:
            goto jmpRetry;
        case JMP_TRA:
            goto jmpTra;
    }
//    if (val)
//    {
//        // if we're here, we're returning from a fault etc and we want to retry an instruction
//        goto retry;
//    } else {
//        reason = 0;
//        cpuCycles = 0;  // XXX This probably needs to be moved so our fault handler won't reset cpuCycles back to 0
//    }
    
    do {
jmpRetry:;
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
jmpTra:                 continue;   // don't bump rIC, instruction already did it
                }
            }
        }
        
        if (ci->opcode == 0616) { // DIS
            reason = STOP_DIS;
            break;
        }

jmpNext:;
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
        
        //switch (processorAddressingMode)
        switch (get_addr_mode())
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
                    {
                        sim_debug(DBG_APPENDING, &cpu_dev, "Read(%06o %012llo %02o): going into APPENDING mode\n", addr, *dat, Tag);
                    }
                    
                    //processorAddressingMode = APPEND_MODE;
                    set_addr_mode(APPEND_mode);
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
    cpu.read_addr = addr;
    
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
        
        //switch (processorAddressingMode)
        switch (get_addr_mode())
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
    //return SCPE_OK;
    
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

//#define MM
#if 1   //def MM
//=============================================================================

/*
 * fetch_word()
 *
 * Fetches a word at the specified address according to the current
 * addressing mode.
 *
 * Returns non-zero if a fault in groups 1-6 is detected
 */

int fetch_appended(uint offset, t_uint64 *wordp);
int fetch_abs_word(uint addr, t_uint64 *wordp);

int fetch_word_MM(uint addr, t_uint64 *wordp)
{
    addr_modes_t mode = get_addr_mode();
    
    if (mode == APPEND_mode) {
        return fetch_appended(addr, wordp);
    } else if (mode == ABSOLUTE_mode) {
        return fetch_abs_word(addr, wordp);
    } else if (mode == BAR_mode) {
        if (addr >= (BAR.BOUND << 9)) {
            log_msg(NOTIFY_MSG, "CU::fetch", "Address %#o is out of BAR bounds of %#o.\n", addr, BAR.BOUND << 9);
            fault_gen(store_fault);
            // fault_reg.oob = 1;           // ERROR: fault_reg does not exist
            // cancel_run(STOP_WARN);
            return 1;
        }
        log_msg(DEBUG_MSG, "CU::fetch", "Translating offset %#o to %#o.\n", addr, addr + (BAR.BASE << 9));
        addr += BAR.BOUND << 9;
        return fetch_abs_word(addr, wordp);
#if 0
        log_msg(ERR_MSG, "CU::fetch", "Addr=%#o:  BAR mode unimplemented.\n", addr);
        cancel_run(STOP_BUG);
        return fetch_abs_word(addr, wordp);
#endif
    } else {
        log_msg(ERR_MSG, "CU::fetch", "Addr=%#o:  Unknown addr mode %d.\n", addr, mode);
        cancel_run(STOP_BUG);
        return fetch_abs_word(addr, wordp);
    }
}

int fetch_word(DCDstruct *i, uint addr, t_uint64 *wordp)
{
    addr_modes_t mode = get_addr_mode();
    
    if (mode == APPEND_mode) {
        return Read(i, addr, wordp, DataRead, 0);
    } else if (mode == ABSOLUTE_mode) {
        return Read(i, addr, wordp, DataRead, 0);
    } else if (mode == BAR_mode) {
        if (addr >= (BAR.BOUND << 9)) {
            log_msg(NOTIFY_MSG, "CU::fetch", "Address %#o is out of BAR bounds of %#o.\n", addr, BAR.BOUND << 9);
            fault_gen(store_fault);
            // fault_reg.oob = 1;           // ERROR: fault_reg does not exist
            // cancel_run(STOP_WARN);
            return 1;
        }
        log_msg(DEBUG_MSG, "CU::fetch", "Translating offset %#o to %#o.\n", addr, addr + (BAR.BASE << 9));
        addr += BAR.BOUND << 9;
        return Read(i, addr, wordp, DataRead, 0);
#if 0
        log_msg(ERR_MSG, "CU::fetch", "Addr=%#o:  BAR mode unimplemented.\n", addr);
        cancel_run(STOP_BUG);
        return Read(i, addr, wordp, DataRead, 0);
#endif
    } else {
        log_msg(ERR_MSG, "CU::fetch", "Addr=%#o:  Unknown addr mode %d.\n", addr, mode);
        cancel_run(STOP_BUG);
        return Read(i, addr, wordp, DataRead, 0);
    }
}

/*
 * decode_instr()
 *
 * Convert a 36-bit word into a instr_t struct.
 *
 */
extern int is_eis[];

void decode_instr(instr_t *ip, t_uint64 word)
{
    ip->addr = (uint)getbits36(word, 0, 18);
    ip->opcode = (uint)getbits36(word, 18, 10);
    ip->inhibit = (uint)getbits36(word, 28, 1);
    if (! (ip->is_eis_multiword = is_eis[ip->opcode])) {
        ip->mods.single.pr_bit = (uint)getbits36(word, 29, 1);
        ip->mods.single.tag = (uint)getbits36(word, 30, 6);
    } else {
        ip->mods.mf1.ar = (uint)getbits36(word, 29, 1);
        ip->mods.mf1.rl = (uint)getbits36(word, 30, 1);
        ip->mods.mf1.id = (uint)getbits36(word, 31, 1);
        ip->mods.mf1.reg = (uint)getbits36(word, 32, 4);
    }
}

//=============================================================================

/*
 * encode_instr()
 *
 * Convert an instr_t struct into a  36-bit word.
 *
 */

extern int is_eis[1024];    // hack

void encode_instr(const instr_t *ip, t_uint64 *wordp)
{
    *wordp = setbits36(0, 0, 18, ip->addr);
#if 1
    *wordp = setbits36(*wordp, 18, 10, ip->opcode);
#else
    *wordp = setbits36(*wordp, 18, 9, ip->opcode & 0777);
    *wordp = setbits36(*wordp, 27, 1, ip->opcode >> 9);
#endif
    *wordp = setbits36(*wordp, 28, 1, ip->inhibit);
    if (! is_eis[ip->opcode&MASKBITS(10)]) {
        *wordp = setbits36(*wordp, 29, 1, ip->mods.single.pr_bit);
        *wordp = setbits36(*wordp, 30, 6, ip->mods.single.tag);
    } else {
        *wordp = setbits36(*wordp, 29, 1, ip->mods.mf1.ar);
        *wordp = setbits36(*wordp, 30, 1, ip->mods.mf1.rl);
        *wordp = setbits36(*wordp, 31, 1, ip->mods.mf1.id);
        *wordp = setbits36(*wordp, 32, 4, ip->mods.mf1.reg);
    }
}

//=============================================================================

/*
 * fetch_instr()
 *
 * Fetch intstruction
 *
 * Note that we allow fetch from an arbitrary address.
 * Returns non-zero if a fault in groups 1-6 is detected
 *
 * TODO: limit this to only the CPU by re-working the "xec" instruction.
 */

#if OLD
int fetch_instr_MM(uint IC, instr_t *ip)
{
    
    t_uint64 word;
    int ret = fetch_word(IC, &word);
    if (ip)
        decode_instr(ip, word);
    
    ip->wordEven = word;
    return ret;
}
#endif

/*
 * instruction decoder .....
 *
 * if dst is not NULL place results into dst, if dst is NULL plae results into global currentInstruction
 */
DCDstruct *decodeInstruction2(word36 inst, DCDstruct *dst)     // decode instruction into structure
{
    DCDstruct *p = dst == NULL ? newDCDstruct() : dst;
    
    p->opcode  = GET_OP(inst);  // get opcode
    p->opcodeX = GET_OPX(inst); // opcode extension
    p->address = GET_ADDR(inst);// address field from instruction
    p->a       = GET_A(inst);   // "A" - the indirect via pointer register flag
    p->i       = GET_I(inst);   // inhibit interrupt flag
    p->tag     = GET_TAG(inst); // instruction tag
    
    //cu.tag = p->tag;    // ???
    
    p->iwb = getIWBInfo(p);     // get info for IWB instruction
    
    // HWR 18 June 2013
    p->iwb->opcode = p->opcode;
    p->IWB = inst;
    
    return p;
}

int fetch_instr(uint IC, DCDstruct *i)
{
    DCDstruct *p = (i == NULL) ? newDCDstruct() : i;
    
    Read(p, IC, &p->IWB, InstructionFetch, 0);
    
    cpu.read_addr = IC;
    
    decodeInstruction2(p->IWB, p);
    // ToDo: cause to return non 0 if a fault occurs
    return 0;
}

//=============================================================================

/*
 * fetch_abs_word(uint addr, t_uint64 *wordp)
 *
 * Fetch word at given 24-bit absolute address.
 * Returns non-zero if a fault in groups 1-6 is detected
 *
 */

int fetch_abs_word(uint addr, t_uint64 *wordp)
{
    
#define CONFIG_DECK_LOW 012000
#define CONFIG_DECK_LEN 010000
    
    // TODO: Efficiency: If the compiler doesn't do it for us, the tests
    // below should be combined under a single min/max umbrella.
    if (addr >= IOM_MBX_LOW && addr < IOM_MBX_LOW + IOM_MBX_LEN) {
        log_msg(DEBUG_MSG, "CU::fetch", "Fetch from IOM mailbox area for addr %#o\n", addr);
    }
    if (addr >= DN355_MBX_LOW && addr < DN355_MBX_LOW + DN355_MBX_LEN) {
        log_msg(DEBUG_MSG, "CU::fetch", "Fetch from DN355 mailbox area for addr %#o\n", addr);
    }
    if (addr >= CONFIG_DECK_LOW && addr < CONFIG_DECK_LOW + CONFIG_DECK_LEN) {
        log_msg(DEBUG_MSG, "CU::fetch", "Fetch from CONFIG DECK area for addr %#o\n", addr);
    }
    if (addr <= 030) {
        log_msg(DEBUG_MSG, "CU::fetch", "Fetch from 0..030 for addr %#o\n", addr);
    }
    
    if (addr >= MAXMEMSIZE) {
        log_msg(ERR_MSG, "CU::fetch", "Addr %#o (%d decimal) is too large\n", addr, addr);
        (void) cancel_run(STOP_BUG);
        return 1;
    }
    
//  HWR   if (sim_brk_summ) {
//        // Check for absolute mode breakpoints.  Note that fetch_appended()
//        // has its own test for appending mode breakpoints.
//        t_uint64 simh_addr = addr_emul_to_simh(ABSOLUTE_mode, 0, addr);
//        if (sim_brk_test (simh_addr, SWMASK ('M'))) {
//            log_msg(WARN_MSG, "CU::fetch", "Memory Breakpoint, address %#o.  Fetched value %012llo\n", addr, M[addr]);
//            (void) cancel_run(STOP_BKPT);
//        }
//    }
    
    cpu.read_addr = addr;   // Should probably be in scu
#if MEM_CHECK_UNINIT
    {
        t_uint64 word = Mem[addr];  // absolute memory reference
        if (word == ~ 0) {
            word = 0;
            if (sys_opts.warn_uninit)
                log_msg(WARN_MSG, "CU::fetch", "Fetch from uninitialized absolute location %#o.\n", addr);
        }
        *wordp = word;
    }
#else
    *wordp = M[addr]; // absolute memory reference
#endif
    if (get_addr_mode() == BAR_mode)
        log_msg(DEBUG_MSG, "CU::fetch-abs", "fetched word at %#o\n", addr);
    return 0;
}

//=============================================================================

/*
 * store_word()
 *
 * Store a word to the specified address according to the current
 * addressing mode.
 *
 * Returns non-zero if a fault in groups 1-6 is detected
 */

int store_word(uint addr, t_uint64 word)
{
    
    addr_modes_t mode = get_addr_mode();
    
    if (mode == APPEND_mode) {
        return store_appended(addr, word);
    } else if (mode == ABSOLUTE_mode) {
        return store_abs_word(addr, word);
    } else if (mode == BAR_mode) {
#if 0
        log_msg(ERR_MSG, "CU::store", "Addr=%#o:  BAR mode unimplemented.\n", addr);
        cancel_run(STOP_BUG);
        return store_abs_word(addr, word);
#else
        if (addr >= (BAR.BOUND << 9)) {
            log_msg(NOTIFY_MSG, "CU::store", "Address %#o is out of BAR bounds of %#o.\n", addr, BAR.BOUND << 9);
            fault_gen(store_fault);
            // fault_reg.oob = 1;           // ERROR: fault_reg does not exist
            // cancel_run(STOP_WARN);
            return 1;
        }
        log_msg(DEBUG_MSG, "CU::store", "Translating offset %#o to %#o.\n", addr, addr + (BAR.BASE << 9));
        addr += BAR.BASE << 9;
        return store_abs_word(addr, word);
        //return store_appended(addr, word);
#endif
    } else {
        // impossible
        log_msg(ERR_MSG, "CU::store", "Addr=%#o:  Unknown addr mode %d.\n", addr, mode);
        cancel_run(STOP_BUG);
        return 1;
    }
}

//=============================================================================

/*
 * store-abs_word()
 *
 * Store word to the given 24-bit absolute address.
 */

int store_abs_word(uint addr, t_uint64 word)
{
    
    // TODO: Efficiency: If the compiler doesn't do it for us, the tests
    // below should be combined under a single min/max umbrella.
    if (addr >= IOM_MBX_LOW && addr < IOM_MBX_LOW + IOM_MBX_LEN) {
        log_msg(DEBUG_MSG, "CU::store", "Store to IOM mailbox area for addr %#o\n", addr);
    }
    if (addr >= DN355_MBX_LOW && addr < DN355_MBX_LOW + DN355_MBX_LEN) {
        log_msg(DEBUG_MSG, "CU::store", "Store to DN355 mailbox area for addr %#o\n", addr);
    }
    if (addr >= CONFIG_DECK_LOW && addr < CONFIG_DECK_LOW + CONFIG_DECK_LEN) {
        log_msg(DEBUG_MSG, "CU::store", "Store to CONFIG DECK area for addr %#o\n", addr);
    }
    if (addr <= 030) {
        //log_msg(DEBUG_MSG, "CU::store", "Fetch from 0..030 for addr %#o\n", addr);
    }
    
    if (addr >= MAXMEMSIZE) {
        log_msg(ERR_MSG, "CU::store", "Addr %#o (%d decimal) is too large\n");
        (void) cancel_run(STOP_BUG);
        return 1;
    }
// HWR   if (sim_brk_summ) {
//        // Check for absolute mode breakpoints.  Note that store_appended()
//        // has its own test for appending mode breakpoints.
//        t_uint64 simh_addr = addr_emul_to_simh(ABSOLUTE_mode, 0, addr);
//        uint mask;
//        if ((mask = sim_brk_test(simh_addr, SWMASK('W') | SWMASK('M') | SWMASK('E'))) != 0) {
//            if ((mask & SWMASK ('W')) != 0) {
//                log_msg(NOTIFY_MSG, "CU::store", "Memory Write Breakpoint, address %#o\n", addr);
//                (void) cancel_run(STOP_BKPT);
//            } else if ((mask & SWMASK ('M')) != 0) {
//                log_msg(NOTIFY_MSG, "CU::store", "Memory Breakpoint, address %#o\n", addr);
//                (void) cancel_run(STOP_BKPT);
//            } else if ((mask & SWMASK ('E')) != 0) {
//                log_msg(NOTIFY_MSG, "CU::store", "Write to a location that has an execution breakpoint, address %#o\n", addr);
//            } else {
//                log_msg(NOTIFY_MSG, "CU::store", "Write to a location that has an unknown type of breakpoint, address %#o\n", addr);
//                (void) cancel_run(STOP_BKPT);
//            }
//            log_msg(INFO_MSG, "CU::store", "Address %08o: value was %012llo, storing %012llo\n", addr, M[addr], word);
//        }
//    }
    
    M[addr] = word;   // absolute memory reference
    if (addr == cpu.IC_abs) {
        log_msg(NOTIFY_MSG, "CU::store", "Flagging cached odd instruction from %o as invalidated.\n", addr);
        cpu.irodd_invalid = 1;
    }
    if (get_addr_mode() == BAR_mode)
        log_msg(DEBUG_MSG, "CU::store-abs", "stored word to %#o\n", addr);
    return 0;
}

//=============================================================================

/*
 * store_abs_pair()
 *
 * Store to even and odd words at Y-pair given by 24-bit absolute address
 * addr.  Y-pair addresses are always constrained such that the first
 * address used will be even.
 *
 * Returns non-zero if fault in groups 1-6 detected
 */

int store_abs_pair(uint addr, t_uint64 word0, t_uint64 word1)
{
    int ret;
    uint Y = (addr % 2 == 0) ? addr : addr - 1;
    
    if ((ret = store_abs_word(Y, word0)) != 0) {
        return ret;
    }
    if ((ret = store_abs_word(Y+1, word1)) != 0) {
        return ret;
    }
    return 0;
}

//=============================================================================

/*
 * store_pair()
 *
 * Store to even and odd words at given Y-pair address according to the
 * current addressing mode.
 * Y-pair addresses are always constrained such that the first address used
 * will be even.
 *
 * Returns non-zero if fault in groups 1-6 detected
 *
 * BUG: Is it that the given y-pair offset must be even or is it that the
 * final computed addr must be even?   Note that the question may be moot --
 * it might be that segements always start on an even address and contain
 * an even number of words.
 */

int store_pair(uint addr, t_uint64 word0, t_uint64 word1)
{
    
    int ret;
    uint Y = (addr % 2 == 0) ? addr : addr - 1;
    
    if ((ret = store_word(Y, word0)) != 0) {
        return ret;
    }
    if ((ret = store_word(Y+1, word1)) != 0) {
        return ret;
    }
    return 0;
}

//=============================================================================

/*
 * fetch_abs_pair()
 *
 * Fetch even and odd words at Y-pair given by the specified 24-bit absolute
 * address.
 *
 * Returns non-zero if fault in groups 1-6 detected
 */

int fetch_abs_pair(uint addr, t_uint64* word0p, t_uint64* word1p)
{
    
    int ret;
    uint Y = (addr % 2 == 0) ? addr : addr - 1;
    
    if ((ret = fetch_abs_word(Y, word0p)) != 0) {
        return ret;
    }
    if ((ret = fetch_abs_word(Y+1, word1p)) != 0) {
        return ret;
    }
    return 0;
}

//=============================================================================

/*
 * fetch_pair()
 *
 * Fetch from the even and odd words at given Y-pair address according to the
 * current addressing mode.
 * Y-pair addresses are always constrained such that the first address used
 * will be even.
 *
 * Returns non-zero if fault in groups 1-6 detected
 *
 * BUG: see comments at store_pair() re what does "even" mean?
 */

#if OLD
int fetch_pair(uint addr, t_uint64* word0p, t_uint64* word1p)
{
    int ret;
    uint Y = (addr % 2 == 0) ? addr : addr - 1;
    
    if ((ret = fetch_word(Y, word0p)) != 0) {
        return ret;
    }
    if ((ret = fetch_word(Y+1, word1p)) != 0) {
        return ret;
    }
    return 0;
}
#endif

//=============================================================================

/*
 * fetch_yblock()
 *
 * Aligned or un-aligned fetch from the given Y-block address according to
 * the current addressing mode.
 *
 * Returns non-zero if fault in groups 1-6 detected
 *
 * BUG: What does "aligned" mean for appending mode?  See comments at
 * store_pair().
 */

int fetch_yblock(uint addr, int aligned, uint n, t_uint64 *wordsp)
{
    int ret;
    uint Y = (aligned) ? (addr / n) * n : addr;
    
    for (int i = 0; i < n; ++i)
        if ((ret = fetch_word_MM(Y++, wordsp++)) != 0)
            return ret;
    return 0;
}

//=============================================================================

/*
 * fetch_yblock8()
 *
 * Aligned fetch from the given Y-block-8 address according to
 * the current addressing mode.
 *
 * Returns non-zero if fault in groups 1-6 detected
 *
 * BUG: What does "aligned" mean for appending mode?  See comments at
 * fetch_yblock() and store_pair().
 */

int fetch_yblock8(uint addr, t_uint64 *wordsp)
{
    return fetch_yblock(addr, 1, 8, wordsp);
}


//=============================================================================

/*
 * store_yblock()
 *
 * Aligned or un-aligned store to the given Y-block address according to
 * the current addressing mode.
 *
 * Returns non-zero if fault in groups 1-6 detected
 *
 * BUG: What does "aligned" mean for appending mode?  See comments at
 * store_pair().
 */

static int store_yblock(uint addr, int aligned, int n, const t_uint64 *wordsp)
{
    int ret;
    uint Y = (aligned) ? (addr / n) * n : addr;
    
    for (int i = 0; i < n; ++i)
        if ((ret = store_word(Y++, *wordsp++)) != 0)
            return ret;
    return 0;
}

//=============================================================================

int store_yblock8(uint addr, const t_uint64 *wordsp)
{
    return store_yblock(addr, 1, 8, wordsp);
}

//=============================================================================

int store_yblock16(uint addr, const t_uint64 *wordsp)
{
    return store_yblock(addr, 1, 16, wordsp);
}

//=============================================================================

int store_appended(uint offset, t_uint64 word)
{
    // Store a word at the given offset in the current segment (if possible).
    addr_modes_t addr_mode = get_addr_mode();
    if (addr_mode != APPEND_mode && addr_mode != BAR_mode) {
        // impossible
        log_msg(ERR_MSG, "APU::store-append", "Not APPEND mode\n");
        cancel_run(STOP_BUG);
    }
    
// HWR   t_uint64 simh_addr = addr_emul_to_simh(addr_mode, TPR.TSR, offset);
//    if (sim_brk_summ && sim_brk_test(simh_addr, SWMASK('W') | SWMASK('M'))) {
//        log_msg(WARN_MSG, "APU", "Memory Breakpoint on write.\n");
//        cancel_run(STOP_BKPT);
//    }
    
    // FixMe: fix this HWR
    /*
    uint addr;
    uint minaddr, maxaddr;  // results unneeded
    int ret = page_in(offset, 0, &addr, &minaddr, &maxaddr);
    if (ret == 0) {
        if(opt_debug>0) log_msg(DEBUG_MSG, "APU::store-append", "Using addr 0%o\n", addr);
        ret = store_abs_word(addr, word);
    } else
        if(opt_debug>0) log_msg(DEBUG_MSG, "APU::store-append", "page-in faulted\n");
    return ret;
     */
    return 0;
}

//=============================================================================

static int addr_append(t_uint64 *wordp)
{
    // Implements AL39, figure 5-4
    // NOTE: ri mode is expecting a fetch
    return fetch_appended(TPR.CA, wordp);
}

//=============================================================================

/*
 * fetch_appended()
 *
 * Fetch a word at the given offset in the current segment (if possible).
 *
 * Implements AL39, figure 5-4
 *
 * Note that we allow an arbitrary offset not just TPR.CA.   This is to support
 * instruction fetches.
 *
 * FIXME: Need to handle y-pairs -- fixed?
 *
 * In BAR mode, caller is expected to have already added the BAR.base and
 * checked the BAR.bounds
 *
 * Returns non-zero if a fault in groups 1-6 detected
 */

int fetch_appended(uint offset, t_uint64 *wordp)
{
    addr_modes_t addr_mode = get_addr_mode();
    if (addr_mode == ABSOLUTE_mode)
        return fetch_abs_word(offset, wordp);
#if 0
    if (addr_mode == BAR_mode) {
        log_msg(WARN_MSG, "APU::fetch_append", "APU not intended for BAR mode\n");
        offset += BAR.base << 9;
        cancel_run(STOP_WARN);
        return fetch_abs_word(offset, wordp);
    }
    if (addr_mode != APPEND_mode) {
#else
        if (addr_mode != APPEND_mode && addr_mode != BAR_mode) {
#endif
            // impossible
            log_msg(ERR_MSG, "APU::append", "Unknown mode\n");
            cancel_run(STOP_BUG);
            return fetch_abs_word(offset, wordp);
        }
        
// HWR       t_uint64 simh_addr = addr_emul_to_simh(addr_mode, TPR.TSR, offset);
//        if (sim_brk_summ && sim_brk_test(simh_addr, SWMASK('M'))) {
//            log_msg(WARN_MSG, "APU", "Memory Breakpoint on read.\n");
//            cancel_run(STOP_BKPT);
//        }
        
        // FixMe: fix this HWR
        /*
        uint addr;
        uint minaddr, maxaddr;  // results unneeded
        int ret = page_in(offset, 0, &addr, &minaddr, &maxaddr);
        if (ret == 0) {
            if(opt_debug>0) log_msg(DEBUG_MSG, "APU::fetch_append", "Using addr 0%o\n", addr);
            ret = fetch_abs_word(addr, wordp);
        } else {
            if(opt_debug>0) log_msg(DEBUG_MSG, "APU::fetch_append", "page-in faulted\n");
        }
        return ret;
         */
        return 0;
    }
    
    //=============================================================================
    
//    int store_appended(uint offset, t_uint64 word)
//    {
//        // Store a word at the given offset in the current segment (if possible).
//        addr_modes_t addr_mode = get_addr_mode();
//        if (addr_mode != APPEND_mode && addr_mode != BAR_mode) {
//            // impossible
//            log_msg(ERR_MSG, "APU::store-append", "Not APPEND mode\n");
//            cancel_run(STOP_BUG);
//        }
//        
//        t_uint64 simh_addr = addr_emul_to_simh(addr_mode, TPR.TSR, offset);
//        if (sim_brk_summ && sim_brk_test(simh_addr, SWMASK('W') | SWMASK('M'))) {
//            log_msg(WARN_MSG, "APU", "Memory Breakpoint on write.\n");
//            cancel_run(STOP_IBKPT);
//        }
//        
//        uint addr;
//        uint minaddr, maxaddr;  // results unneeded
//        int ret = page_in(offset, 0, &addr, &minaddr, &maxaddr);
//        if (ret == 0) {
//            if(opt_debug>0) log_msg(DEBUG_MSG, "APU::store-append", "Using addr 0%o\n", addr);
//            ret = store_abs_word(addr, word);
//        } else
//            if(opt_debug>0) log_msg(DEBUG_MSG, "APU::store-append", "page-in faulted\n");
//        return ret;
//    }
    
    //=============================================================================

#endif // MM
    
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
    
    cpu.read_addr = addr;
    
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
    
    // HWR 18 June 2013 
    p->iwb->opcode = p->opcode;
    p->IWB = inst;
    
    
    // ToDo: may need to rethink how.when this is dome. Seems to crash the cu
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

// MM stuff ...

    /*
     * is_priv_mode()
     *
     * Report whether or or not the CPU is in privileged mode.
     * True if in absolute mode or if priv bit is on in segment TPR.TSR
     *
     * TODO: is_priv_mode() probably belongs in the CPU source file.
     *
     */
    
    int is_priv_mode()
    {
        // TODO: fix this when time permits
        
        switch (get_addr_mode())
        {
            case ABSOLUTE_mode:
                return 1;
            default:
                break;
        }
        //if (!TSTF(rIR, I_ABS))       //IR.abs_mode)
        //    return 1;
        
//        SDW_t *SDWp = get_sdw();    // Get SDW for segment TPR.TSR
//        if (SDWp == NULL) {
//            if (cpu.apu_state.fhld) {
//                // TODO: Do we need to check cu.word1flags.oosb and other flags to
//                // know what kind of fault to gen?
//                fault_gen(acc_viol_fault);
//                cpu.apu_state.fhld = 0;
//            }
//            log_msg(WARN_MSG, "APU::is-priv-mode", "Segment does not exist?!?\n");
//            cancel_run(STOP_BUG);
//            return 0;   // arbitrary
//        }
//        if (SDWp->priv)
//            return 1;
//        if(opt_debug>0)
//            log_msg(DEBUG_MSG, "APU", "Priv check fails for segment %#o.\n", TPR.TSR);
//        return 0;
        
        // XXX This is probably too simplistic, but it's a start
        if (SDW0.P)
            return 1;
        
        return 0;
    }


/*
 * addr_modes_t get_addr_mode()
 *
 * Report what mode the CPU is in.
 * This is determined by examining a couple of IR flags.
 *
 * TODO: get_addr_mode() probably belongs in the CPU source file.
 *
 */

addr_modes_t get_addr_mode()
{
    //if (IR.abs_mode)
    if (TSTF(rIR, I_ABS))
        return ABSOLUTE_mode;
    
    //if (IR.not_bar_mode == 0)
    if (! TSTF(rIR, I_NBAR))
        return BAR_mode;
    
    return APPEND_mode;
}


/*
 * set_addr_mode()
 *
 * Put the CPU into the specified addressing mode.   This involves
 * setting a couple of IR flags and the PPR priv flag.
 *
 */

void set_addr_mode(addr_modes_t mode)
{
    if (mode == ABSOLUTE_mode) {
        IR.abs_mode = 1;
        SETF(rIR, I_ABS);
        
        // FIXME: T&D tape section 3 wants not-bar-mode true in absolute mode,
        // but section 9 wants false?
        IR.not_bar_mode = 1;
        SETF(rIR, I_NBAR);
        
        PPR.P = 1;
        if (opt_debug) log_msg(DEBUG_MSG, "APU", "Setting absolute mode.\n");
    } else if (mode == APPEND_mode) {
        if (opt_debug) {
            if (! IR.abs_mode && IR.not_bar_mode)
                log_msg(DEBUG_MSG, "APU", "Keeping append mode.\n");
            else
                log_msg(DEBUG_MSG, "APU", "Setting append mode.\n");
        }
        IR.abs_mode = 0;
        CLRF(rIR, I_ABS);
        
        IR.not_bar_mode = 1;
        SETF(rIR, I_NBAR);
        
    } else if (mode == BAR_mode) {
        IR.abs_mode = 0;
        CLRF(rIR, I_ABS);
        IR.not_bar_mode = 0;
        CLRF(rIR, I_NBAR);
        
        log_msg(WARN_MSG, "APU", "Setting bar mode.\n");
    } else {
        log_msg(ERR_MSG, "APU", "Unable to determine address mode.\n");
        cancel_run(STOP_BUG);
    }
    
    //processorAddressingMode = mode;
}


//=============================================================================

/*
 ic_hist - Circular queue of instruction history
 Used for display via cpu_show_history()
 */

static int ic_hist_max = 0;
static int ic_hist_ptr;
static int ic_hist_wrapped;
enum hist_enum { h_instruction, h_fault, h_intr } htype;
struct ic_hist_t {
    addr_modes_t addr_mode;
    uint seg;
    uint ic;
    //enum hist_enum { instruction, fault, intr } htype;
    enum hist_enum htype;
    union {
        int intr;
        int fault;
        instr_t instr;
    } detail;
};

typedef struct ic_hist_t ic_hist_t;

static ic_hist_t *ic_hist;

void ic_history_init()
{
    ic_hist_wrapped = 0;
    ic_hist_ptr = 0;
    if (ic_hist != NULL)
        free(ic_hist);
    if (ic_hist_max < 60)
        ic_hist_max = 60;
    ic_hist = (ic_hist_t*) malloc(sizeof(*ic_hist) * ic_hist_max);
}

//=============================================================================

static ic_hist_t* ic_history_append()
{
    
    ic_hist_t* ret =  &ic_hist[ic_hist_ptr];
    
    if (++ic_hist_ptr == ic_hist_max) {
        ic_hist_wrapped = 1;
        ic_hist_ptr = 0;
    }
    return ret;
}

//=============================================================================

// Maintain a queue of recently executed instructions for later display via cmd_dump_history()
// Caller should make sure IR and PPR are set to the recently executed instruction.

void ic_history_add()
{
    if (ic_hist_max == 0)
        return;
    ic_hist_t* hist = ic_history_append();
    hist->htype = h_instruction;
    hist->addr_mode = get_addr_mode();
    hist->seg = PPR.PSR;
    hist->ic = PPR.IC;
    memcpy(&hist->detail.instr, &cu.IR, sizeof(hist->detail.instr));
}

//=============================================================================

void ic_history_add_fault(int fault)
{
    if (ic_hist_max == 0)
        return;
    ic_hist_t* hist = ic_history_append();
    hist->htype = h_fault;
    hist->detail.fault = fault;
}

//=============================================================================

void ic_history_add_intr(int intr)
{
    if (ic_hist_max == 0)
        return;
    ic_hist_t* hist = ic_history_append();
    hist->htype = h_intr;
    hist->detail.intr = intr;
}

//=============================================================================

int cmd_dump_history(int32 arg, char *buf, int nshow)
// Dumps the queue of instruction history
{
    if (ic_hist_max == 0) {
        out_msg("History is disabled.\n");
        return SCPE_NOFNC;
    }
    // The queue is implemented via an array and is circular,
    // so we make up to two passes through the array.
    int n = (ic_hist_wrapped) ? ic_hist_max : ic_hist_ptr;
    int n_ignore = (nshow < n) ? n - nshow : 0;
    for (int wrapped = ic_hist_wrapped; wrapped >= 0; --wrapped) {
        int start, end;
        if (wrapped) {
            start = ic_hist_ptr;
            end = ic_hist_max;
        } else {
            start = 0;
            end = ic_hist_ptr;
        }
        for (int i = start; i < end; ++i) {
            if (n_ignore-- > 0)
                continue;
            switch(ic_hist[i].htype) {
                case h_instruction: {
                    int segno = (ic_hist[i].addr_mode == APPEND_mode) ? (int) ic_hist[i].seg: -1;
        // HWR            print_src_loc("", ic_hist[i].addr_mode, segno, ic_hist[i].ic, &ic_hist[i].detail.instr);
                    break;
                }
                case h_fault:
                    out_msg("    Fault %#o (%d)\n", ic_hist[i].detail.fault, ic_hist[i].detail.fault);
                    break;
                case h_intr:
                    out_msg("    Interrupt %#o (%d)\n", ic_hist[i].detail.intr, ic_hist[i].detail.intr);
                    break;
            }
        }
    }
    return 0;
}

//=============================================================================

int cpu_show_history(FILE *st, UNIT *uptr, int val, void *desc)
{
    // FIXME: use FILE *st
    
    if (ic_hist_max == 0) {
        out_msg("History is disabled.\n");
        return SCPE_NOFNC;
    }
    
    char* cptr = (char *) desc;
    int n;
    if (cptr == NULL)
        n = ic_hist_max;
    else {
        char c;
        if (sscanf(cptr, "%d %c", &n, &c) != 1) {
            out_msg("Error, expecting a number.\n");
            return SCPE_ARG;
        }
    }
    
    return cmd_dump_history(0, NULL, n);
}

//=============================================================================

int cpu_set_history (UNIT *uptr, int32 val, char *cptr, void *desc)
{
    if (cptr == NULL) {
        out_msg("Error, usage is set cpu history=<n>\n");
        return SCPE_ARG;
    }
    char c;
    int n;
    if (sscanf(cptr, "%d %c", &n, &c) != 1) {
        out_msg("Error, expecting a number.\n");
        return SCPE_ARG;
    }
    
    if (n <= 0) {
        if (ic_hist != NULL)
            free(ic_hist);
        ic_hist = NULL;
        ic_hist_wrapped = 0;
        ic_hist_ptr = 0;
        ic_hist_max = 0;
        out_msg("History disabled\n");
        return 0;
    }
    
    ic_hist_t* new_hist = (ic_hist_t*) malloc(sizeof(*ic_hist) * n);
    
    int old_n;
    int old_head_loc;
    int old_nhead;  // amount at ptr..end
    int old_ntail;  // amount at 0..ptr (or all if never wrapped)
    int old_tail_loc = 0;
    if (ic_hist_wrapped)  {
        // data order is ptr..(max-1), 0..ptr-1
        old_n = ic_hist_max;
        old_head_loc = ic_hist_ptr;
        old_nhead = ic_hist_max - ic_hist_ptr;
        old_ntail = ic_hist_ptr;
    } else {
        // data is 0..(ptr-1)
        old_n = ic_hist_ptr;
        // old_head_loc = "N/A";
        old_head_loc = -123;
        old_nhead = 0;
        old_ntail = ic_hist_ptr;
    }
    int nhead = old_nhead;
    int ntail = old_ntail;
    if (old_n > n) {
        nhead -= old_n - n; // lose some of the earlier stuff
        if (nhead < 0) {
            // under flow, use none of ptr..end and lose some of 0..ptr
            ntail += nhead;
            old_tail_loc -= nhead;
            nhead = 0;
        } else
            old_head_loc += old_n - n;
    }
    if (nhead != 0)
        memcpy(new_hist, ic_hist + old_head_loc, sizeof(*new_hist) * nhead);
    if (ntail != 0)
        memcpy(new_hist + nhead, ic_hist, sizeof(*new_hist) * ntail);
    if (ic_hist != 0)
        free(ic_hist);
    ic_hist = new_hist;
    
    if (n <= old_n)  {
        ic_hist_ptr = 0;
        ic_hist_wrapped = 1;
    } else {
        ic_hist_ptr = old_n;
        ic_hist_wrapped = 0;
    }
    
    if (n >= ic_hist_max)
        if (ic_hist_max == 0)
            out_msg("History enabled.\n");
        else
            out_msg("History increased from %d entries to %d.\n", ic_hist_max, n);
        else
            out_msg("History reduced from %d entries to %d.\n", ic_hist_max, n);
    
    ic_hist_max = n;
    
    return 0;
}

// ============================================================================
 
DCDstruct *copy2DCDstruct(instr_t *ip)
{
    DCDstruct *i = currentInstruction;
    
    return decodeInstruction(ip->wordEven, i);
}
    
static uint saved_tro;

static int do_an_op(instr_t *ip)
{
    // Returns non-zero on error or non-group-7  fault
    // BUG: check for illegal modifiers
    
    DCDstruct *id = copy2DCDstruct(ip);
    
    cpu.opcode = ip->opcode;
        
    uint op = ip->opcode;
    char *opname = opcodes2text[op];
        
    cu.rpts = 0;    // current instruction isn't a repeat type instruction (as far as we know so far)
    saved_tro = IR.tally_runout;
        
    int bit27 = op % 2;
    op >>= 1;
    if (opname == NULL) {
        if (op == 0172 && bit27 == 1) {
            log_msg(WARN_MSG, "OPU", "Unavailable instruction 0172(1).  The ldo  instruction is only available on ADP aka ORION aka DPS88.  Ignoring instruction.\n");
            cancel_run(STOP_BUG);
            return 0;
        }
        log_msg(WARN_MSG, "OPU", "Illegal opcode %03o(%d)\n", op, bit27);
        fault_gen(illproc_fault);
        return 1;
    } else {
        if (opt_debug) log_msg(DEBUG_MSG, "OPU", "Opcode %#o(%d) -- %s\n", op, bit27, disAssemble(id->IWB));    //instr2text(ip));
    }
        
    // Check instr type for format before addr_mod
    // Todo: check efficiency of lookup table versus switch table
    // Also consider placing calls to addr_mod() in next switch table
    flag_t initial_tally = IR.tally_runout;
    cpu.poa = 1;        // prepare operand address flag
    
    word36 word;
    if (id->iwb->flags & (READ_OPERAND | PREPARE_CA))
    {
        int ret = fetch_word_MM(ip, &word);
        if (!ret)
        {
            if (id->iwb->flags & READ_OPERAND)
                CY = word;
            TPR.CA = ip->addr;
        }
    }
    t_stat r = doInstruction(id);
    if (r == CONT_TRA)
        cpu.trgo = 1;
    
    return 0;
}

static int do_op(instr_t *ip)
{
    // Wrapper for do_an_op() with detection of change in address mode (for debugging).
        
    ++ sys_stats.n_instr;
#if FEAT_INSTR_STATS
    ++ sys_stats.instr[ip->opcode].nexec;
#if FEAT_INSTR_STATS_TIMING
    uint32 start = sim_os_msec();
#endif
#endif
        
//HWR   do_18bit_math = 0;
    // do_18bit_math = (switches.FLT_BASE != 2);    // diag tape seems to want this, probably inappropriately
        
    addr_modes_t orig_mode = get_addr_mode();
    int orig_ic = PPR.IC;
        
#if 0
    if (ip->is_eis_multiword) {
        extern DEVICE cpu_dev;
        ++opt_debug; ++ cpu_dev.dctrl;  // BUG
    }
#endif
    int ret = do_an_op(ip);
#if 0
    if (ip->is_eis_multiword) {
        extern DEVICE cpu_dev;
        --opt_debug; --cpu_dev.dctrl;   // BUG
    }
#endif
    
    addr_modes_t mode = get_addr_mode();
    if (orig_mode != mode) {
        if (orig_ic == PPR.IC) {
            if (cpu.trgo)
                log_msg(WARN_MSG, "OPU", "Transfer to current location detected; Resetting addr mode as if it were sequential\n");
                set_addr_mode(orig_mode);
                log_msg(DEBUG_MSG, "OPU", "Resetting addr mode for sequential instr\n");
            } else {
                log_msg(NOTIFY_MSG, "OPU", "Address mode has been changed with an IC change.  Was %#o, now %#o\n", orig_ic, PPR.IC);
                if (switches.FLT_BASE == 2) {
                    cancel_run(STOP_BKPT);
                    log_msg(NOTIFY_MSG, "OPU", "Auto breakpoint.\n");
            }
        }
    }
        
#if FEAT_INSTR_STATS
#if FEAT_INSTR_STATS_TIMING
    sys_stats.instr[ip->opcode].nmsec += sim_os_msec() - start;
#endif
#endif
    
    return ret;
    }

void execute_instr(void)
{
    // execute whatever instruction is in the IR (not whatever the IC points at)
    // BUG: handle interrupt inhibit
        
    do_op(&cu.IR);
}

    
/*
 *  execute_ir()
 *
 *  execute whatever instruction is in the IR instruction register (and
 *  not whatever the IC points at)
 */
    
static void execute_ir_MM(void)
{
    cpu.trgo = 0;       // will be set true by instructions that alter flow
    execute_instr();    // located in opu.c
}

t_stat execute_ir(DCDstruct *i)
{
    ++ sys_stats.n_instr;
#if FEAT_INSTR_STATS
    ++ sys_stats.instr[ip->opcode].nexec;
#if FEAT_INSTR_STATS_TIMING
    uint32 start = sim_os_msec();
#endif
#endif

    cpu.trgo = 0;       // will be set true by instructions that alter flow
    
    cpu.opcode = i->opcode << 1 | (i->opcodeX ? 1 : 0);
    
    uint op = cpu.opcode;

    int bit27 = op % 2;

    char *opname = bit27 ? op1text[op >> 1] : op0text[op >> 1]; //;opcodes2text[op];
    
    cu.rpts = 0;    // current instruction isn't a repeat type instruction (as far as we know so far)
    saved_tro = TSTF(rIR, I_TALLY); // IR.tally_runout;
    
    op >>= 1;
    if (opname == NULL) {
        if (op == 0172 && bit27 == 1) {
            log_msg(WARN_MSG, "OPU", "Unavailable instruction 0172(1).  The ldo  instruction is only available on ADP aka ORION aka DPS88.  Ignoring instruction.\n");
            cancel_run(STOP_BUG);
            return 0;
        }
        log_msg(WARN_MSG, "OPU", "Illegal opcode %03o(%d) -- %06lo\n", op, bit27, rIC);
        fault_gen(illproc_fault);
        return 1;
    } else {
        if (opt_debug) log_msg(DEBUG_MSG, "OPU", "Opcode %#o(%d) -- %s\n", op, bit27, disAssemble(i->IWB)); 
    }
    
    // Check instr type for format before addr_mod
    // Todo: check efficiency of lookup table versus switch table
    // Also consider placing calls to addr_mod() in next switch table
    flag_t initial_tally = IR.tally_runout;
    cpu.poa = 1;        // prepare operand address flag

    
    // ToDo: may need to rethink how.when this is dome. Seems to crash the cu
    // is this a multiword EIS?
    if (i->iwb->ndes > 1)
    {
        memset(i->e, 0, sizeof(EISstruct)); // clear out e
        i->e->op0 = i->IWB;
        // XXX: for XEC/XED/faults, this should trap?? I think -MCW
        for(int n = 0 ; n < i->iwb->ndes; n += 1)
            Read(i, rIC + 1 + n, &i->e->op[n], OperandRead, 0); // I think.
    }
    
    t_stat ret = executeInstruction(i);
    if (ret == CONT_TRA)
    {
        cpu.trgo = 1;
        cpu.irodd_invalid = 1;
    }
    
    if (i->iwb->ndes)
    {
        cpu.irodd_invalid = 1;
        rIC += i->iwb->ndes + 1;
    }
    
#if FEAT_INSTR_STATS
#if FEAT_INSTR_STATS_TIMING
    sys_stats.instr[ip->opcode].nmsec += sim_os_msec() - start;
#endif
#endif

    // ToDo: change to return non-zero if fault
    return 0;
}

//=============================================================================
    

/*
 *  control_unit()
 *
 *  Emulation of the control unit -- fetch cycle, execute cycle,
 *  interrupt handling, fault handling, etc.
 *
 *  We allow SIMH to regain control between any of the cycles of
 *  the control unit.   This includes returning to SIMH on fault
 *  detection and between the even and odd words of a fetched
 *  two word instruction pair.   See cancel_run().
 */

void cu_safe_store();

static t_stat control_unit(void)
{
    // ------------------------------------------------------------------------
    
    // See the following portions of AL39:
    //    Various registers in Section 3: "CONTROL UNIT DATA", IR, Fault, etc
    //    Note word 5 etc in control unit data
    //    All of Section 7
    
    // SEE ALSO
    //  AN87 -- CPU history registers (interrupt is present, etc)
    //  AN87 -- Mode register's overlap inhibit settings
    
    // ------------------------------------------------------------------------
    
    // BUG: Check non group 7 faults?  No, expect cycle to have been reset
    // to FAULT_cycle
    
    int reason = 0;
    int break_on_fault = switches.FLT_BASE == 2;    // on for multics, off for t&d tape
    
    switch(cpu.cycle) {
        case DIS_cycle: {
            // TODO: Use SIMH's idle facility
            // Until then, just freewheel
            //
            // We should probably use the inhibit flag to determine
            // whether or not to examine faults.  However, it appears
            // that we should accept external interrupts regardless of
            // the inhibit flag.   See AL-39 discussion of the timer
            // register for hints.
            if (events.int_pending) {
                cpu.cycle = INTERRUPT_cycle;
                if (cpu.ic_odd && ! cpu.irodd_invalid) {
                    log_msg(NOTIFY_MSG, "CU", "DIS sees an interrupt.\n");
                    log_msg(WARN_MSG, "CU", "Previously fetched odd instruction will be ignored.\n");
                    cancel_run(STOP_WARN);
                } else {
                    log_msg(NOTIFY_MSG, "CU", "DIS sees an interrupt; Auto breakpoint.\n");
                    cancel_run(STOP_BKPT);
                }
                break;
            }
            // No interrupt pending; will we ever see one?
            uint32 n = sim_qcount();
            if (n == 0) {
                log_msg(ERR_MSG, "CU", "DIS instruction running, but no activities are pending.\n");
                //reason = STOP_BUG;
                reason = STOP_DIS;
            } else
                log_msg(DEBUG_MSG, "CU", "Delaying until an interrupt is set.\n");
            break;
        }
            
        case FETCH_cycle:
            if (opt_debug) log_msg(DEBUG_MSG, "CU", "Cycle = FETCH; IC = %0o (%dd)\n", PPR.IC, PPR.IC);
            // If execution of the current pair is complete, the processor
            // checks two? internal flags for group 7 faults and/or interrupts.
            if (events.any) {
                if (cu.IR.inhibit)
                    log_msg(DEBUG_MSG, "CU", "Interrupt or Fault inhibited.\n");
                else {
                    if (break_on_fault) {
                        log_msg(WARN_MSG, "CU", "Fault: auto breakpoint\n");
                        (void) cancel_run(STOP_BKPT);
                    }
                    if (events.low_group != 0) {
                        // BUG: don't need test below now that we detect 1-6 here
                        if (opt_debug>0) log_msg(DEBUG_MSG, "CU", "Fault detected prior to FETCH\n");
                        cpu.cycle = FAULT_cycle;
                        break;
                    }
                    if (events.group7 != 0) {
                        // Group 7 -- See tally runout in IR, connect fields of the
                        // fault register.  DC power off must come via an interrupt?
                        if (opt_debug>0) log_msg(DEBUG_MSG, "CU", "Fault detected prior to FETCH\n");
                        cpu.cycle = FAULT_cycle;
                        break;
                    }
                    if (events.int_pending) {
                        if (opt_debug>0) log_msg(DEBUG_MSG, "CU", "Interrupt detected prior to FETCH\n");
                        cpu.cycle = INTERRUPT_cycle;
                        break;
                    }
                }
            }
            // Fetch a pair of words
            // AL39, 1-13: for fetches, procedure pointer reg (PPR) is
            // ignored. [PPR IC is a dup of IC]
            cpu.ic_odd = PPR.IC % 2;    // don't exec even if fetch from odd
            cpu.cycle = EXEC_cycle;
            TPR.TSR = PPR.PSR;
            TPR.TRR = PPR.PRR;
            cu.instr_fetch = 1;
            
            //if (fetch_instr_MM(PPR.IC - PPR.IC % 2, &cu.IR) != 0) {
            if (fetch_instr(PPR.IC - PPR.IC % 2, currentInstruction) != 0) {
                cpu.cycle = FAULT_cycle;
                cpu.irodd_invalid = 1;
            } else {
#if 0
                t_uint64 simh_addr = addr_emul_to_simh(get_addr_mode(), PPR.PSR, PPR.IC - PPR.IC % 2);

                if (sim_brk_summ && sim_brk_test (simh_addr, SWMASK ('E'))) {
                    log_msg(WARN_MSG, "CU", "Execution Breakpoint (fetch even)\n");
                    reason = STOP_IBKPT;    /* stop simulation */
                }
#endif
                if (fetch_word(currentInstruction, PPR.IC - PPR.IC % 2 + 1, &cu.IRODD) != 0) {
                    cpu.cycle = FAULT_cycle;
                    cpu.irodd_invalid = 1;
                } else {
#if 0
// HWR                   t_uint64 simh_addr = addr_emul_to_simh(get_addr_mode(), PPR.PSR, PPR.IC - PPR.IC % 2 + 1);
//                    if (sim_brk_summ && sim_brk_test (simh_addr, SWMASK ('E'))) {
//                        log_msg(WARN_MSG, "CU", "Execution Breakpoint (fetch odd)\n");
//                        reason = STOP_IBKPT;    /* stop simulation */
//                    }
#endif
                    cpu.irodd_invalid = 0;
                    if (opt_debug && get_addr_mode() != ABSOLUTE_mode)
                        log_msg(DEBUG_MSG, "CU", "Fetched odd half of instruction pair from %06o %012llo\n", PPR.IC - PPR.IC % 2 + 1, cu.IRODD);
                }
            }
            cpu.IC_abs = cpu.read_addr;
            cu.instr_fetch = 0;
            break;
            
#if 0
            // we don't use an ABORT cycle
        case ABORT_cycle:
            log_msg(DEBUG_MSG, "CU", "Cycle = ABORT\n");
            // Invoked when control unit decides to handle fault
            // Bring all overlapped functions to an orderly halt -- however,
            // the simulator has no overlapped functions?
            // Also bring asynchronous functions within the processor
            // to an orderly halt -- TODO -- do we have any?
            cpu.cycle = FAULT_cycle;
            break;
#endif
            
        case FAULT_cycle:
        {
            log_msg(INFO_MSG, "CU", "Cycle = FAULT\n");
            
            // First, find the highest fault.   Group 7 type faults are handled
            // as a special case.  Group 7 faults have different detection
            // rules and there can be multiple pending  faults for group 7.
            
            int fault = 0;
            int group = 0;
            if (events.low_group != 0 && events.low_group <= 6) {
                group = events.low_group;
                fault = events.fault[group];
                if (fault == 0) {
                    log_msg(ERR_MSG, "CU", "Lost fault\n");
                    cancel_run(STOP_BUG);
                }
            }
            if (fault == 0) {
                if (events.group7 == 0) {
                    // bogus fault
                    log_msg(ERR_MSG, "CU", "Fault cycle with no faults set\n");
                    reason = STOP_BUG;
                    events.any = events.int_pending;    // recover
                    cpu.cycle = FETCH_cycle;
                    break;
                } else {
                    // find highest priority group 7 fault
                    int hi = -1;
                    int i;
                    for (i = 0; i < 31; ++i) {
                        if (fault2group[i] == 7 && (events.group7 & (1<<fault)))
                            if (hi == -1 || fault2prio[i] < fault2prio[hi])
                                hi = i;
                    }
                    if (hi == -1) {
                        // no group 7 fault
                        log_msg(ERR_MSG, "CU", "Fault cycle with missing group-7 fault\n");
                        reason = STOP_BUG;
                        events.any = events.int_pending;    // recover
                        cpu.cycle = FETCH_cycle;
                        break;
                    } else {
                        fault = hi;
                        group = 7;
                    }
                }
            }
            ic_history_add_fault(fault);
            log_msg(DEBUG_MSG, "CU", "fault = %d (group %d)\n", fault, group);
            if (fault != trouble_fault)
                cu_safe_store();
            
            // Faults cause the CPU to go into absolute mode.  The CPU will
            // remain in absolute mode until execution of a transfer instr
            // whose operand is obtained via explicit use of the appending
            // HW mechanism -- AL39, 1-3
            set_addr_mode(ABSOLUTE_mode);
            
            // We found a fault.
            // BUG: should we clear the fault?  Or does scr instr do that? Or
            // maybe the OS's fault handling routines do it?   At the
            // moment, we clear it and find the next pending fault.
            // Above was written before we had FR and scpr; perhaps
            // we need to revist the transitions to/from FAULT cycles.
            // AL-39, section 1 says that faults and interrupts are
            // cleared when their trap occurs (that is when the CPU decides
            // to recognize a perhaps delayed signal).
            // Morever, AN71-1 says that the SCU clears the interrupt cell after
            // reporting to the CPU the value of the highest priority interrupt.
            
            int next_fault = 0;
            if (group == 7) {
                // BUG: clear group 7 fault
                log_msg(ERR_MSG, "CU", "BUG: Fault group-7\n");
            } else {
                events.fault[group] = 0;
                // Find next remaining fault (and its group)
                events.low_group = 0;
                for (group = 0; group <= 6; ++ group) {
                    if ((next_fault = events.fault[group]) != 0) {
                        events.low_group = group;
                        break;
                    }
                }
                if (! events.low_group)
                    if (events.group7 != 0)
                        events.low_group = 7;
            }
            events.any = events.int_pending || events.low_group != 0;
            
            // Force computed addr and xed opcode into the instruction
            // register and execute (during FAULT CYCLE not EXECUTE CYCLE).
            // The code below is much the same for interrupts and faults...
            
            PPR.PRR = 0;    // set ring zero
            uint addr = (switches.FLT_BASE << 5) + 2 * fault; // ABSOLUTE mode
            cu.IR.addr = addr;
            cu.IR.opcode = (opcode0_xed << 1);
            cu.IR.inhibit = 1;
            cu.IR.mods.single.pr_bit = 0;
            cu.IR.mods.single.tag = 0;
            
            // Maybe instead of calling execute_ir(), we should just set a
            // flag and run the EXEC case?  // Maybe the following increments
            // and tests could be handled by EXEC and/or the XED opcode?
            
            // Update history (show xed at current location; next two will be addr and addr+1
            //uint IC_temp = PPR.IC;
            //PPR.IC = addr;
            ic_history_add();       // record the xed
            //PPR.IC = IC_temp;
            
            // TODO: Check for SIMH breakpoint on execution for the addr of
            // the XED instruction.  Or, maybe check in the code for the
            // xed opcode.
            log_msg(DEBUG_MSG, "CU::fault", "calling execute_ir() for xed\n");
            
            word36 xr= cu.IR.addr << 18;
            xr |= cu.IR.opcode << 8;
            xr |= cu.IR.inhibit << 7;
            
            DCDstruct i;
            decodeInstruction2(xr, &i);
            
            execute_ir(& i);   // executing in FAULT CYCLE, not EXECUTE CYCLE
            if (break_on_fault) {
                log_msg(WARN_MSG, "CU", "Fault: auto breakpoint\n");
                (void) cancel_run(STOP_BKPT);
            }
            
            if (events.any && events.fault[fault2group[trouble_fault]] == trouble_fault) {
                // Fault occured during execution, so fault_gen() flagged
                // a trouble fault
                // Stay in FAULT CYCLE
                log_msg(WARN_MSG, "CU", "re-faulted, remaining in fault cycle\n");
            } else {
                // cycle = FAULT_EXEC_cycle;
                //cpu.cycle = EXEC_cycle;       // NOTE: scu will be in EXEC not FAULT cycle
                cpu.cycle = FAULT_EXEC_cycle;
                events.xed = 1;     // BUG: is this a hack?
            }
        } // end case FAULT_cycle
            break;
            
        case INTERRUPT_cycle: {
            // This code is just a quick-n-dirty sketch to test booting via
            // an interrupt instead of via a fault
            
            // TODO: Merge the INTERRUPT_cycle and FAULT_cycle code
            
            // The CPU will
            // remain in absolute mode until execution of a transfer instr
            // whose operand is obtained via explicit use of the appending
            // HW mechanism -- AL39, 1-3
            set_addr_mode(ABSOLUTE_mode);
            
            log_msg(WARN_MSG, "CU", "Interrupts only partially implemented\n");
            int intr;
            for (intr = 0; intr < 32; ++intr)
                if (events.interrupts[intr])
                    break;
            if (intr == 32) {
                log_msg(ERR_MSG, "CU", "Interrupt cycle with no pending interrupt.\n");
                // BUG: Need error handling
            }
            ic_history_add_intr(intr);
            log_msg(WARN_MSG, "CU", "Interrupt %#o (%d) found.\n", intr, intr);
            events.interrupts[intr] = 0;
            
            // Force computed addr and xed opcode into the instruction
            // register and execute (during INTERRUPT CYCLE not EXECUTE CYCLE).
            // The code below is much the same for interrupts and faults...
            
            PPR.PRR = 0;    // set ring zero
            const int interrupt_base = 0;
            uint addr = interrupt_base + 2 * intr; // ABSOLUTE mode
            cu.IR.addr = addr;
            cu.IR.opcode = (opcode0_xed << 1);
            cu.IR.inhibit = 1;
            cu.IR.mods.single.pr_bit = 0;
            cu.IR.mods.single.tag = 0;
            
            // Maybe instead of calling execute_ir(), we should just set a
            // flag and run the EXEC case?  // Maybe the following increments
            // and tests could be handled by EXEC and/or the XED opcode?
            
            // Update history
            uint IC_temp = PPR.IC;
            // PPR.IC = 0;
            ic_history_add();       // record the xed
            PPR.IC = IC_temp;
            
            // TODO: Check for SIMH breakpoint on execution for the addr of
            // the XED instruction.  Or, maybe check in the code for the
            // xed opcode.
            log_msg(DEBUG_MSG, "CU::interrupt", "calling execute_ir() for xed\n");
            
            word36 xr= cu.IR.addr << 18;
            xr |= cu.IR.opcode << 8;
            xr |= cu.IR.inhibit << 7;
            
            DCDstruct i;
            decodeInstruction2(xr, &i);
            execute_ir(&i);   // executing in INTERRUPT CYCLE, not EXECUTE CYCLE
            log_msg(WARN_MSG, "CU", "Interrupt -- lightly tested\n");
            (void) cancel_run(STOP_BKPT);
            
            // We executed an XED just above.  XED set various CPU flags.
            // So, now, set the CPU into the EXEC cycle so that the
            // instructions referenced by the XED will be executed.
            // cpu.cycle = FAULT_EXEC_cycle;
            // cpu.cycle = EXEC_cycle;
            cpu.cycle = FAULT_EXEC_cycle;
            events.xed = 1;     // BUG: is this a hack?
            events.int_pending = 0;     // FIXME: make this a counter
            for (intr = 0; intr < 32; ++intr)
                if (events.interrupts[intr]) {
                    events.int_pending = 1;
                    break;
                }
            if (!events.int_pending && ! events.low_group && ! events.group7)
                events.any = 0;
            break;
        }   // end case INTERRUPT_cycle
            
        case EXEC_cycle:
            // Assumption: IC will be at curr instr, even
            // when we're ready to execute the odd half of the pair.
            // Note that the fetch cycle sets the cpu.ic_odd flag.
            TPR.TSR = PPR.PSR;
            TPR.TRR = PPR.PRR;
            
            // Fall through -- FAULT_EXEC_cycle is a subset of EXEC_cycle
            
        case FAULT_EXEC_cycle:
        {
            // FAULT-EXEC is a pseudo cycle not present in the actual
            // hardware.  The hardware does execute intructions (e.g. a
            // fault's xed) in a FAULT cycle.   We should be able to use the
            // FAULT-EXEC cycle for this to gain code re-use.
            
            flag_t do_odd = 0;
            
            // We need to know if we should execute an instruction from the
            // normal fetch process or an extruction loaded by XDE.  The
            // answer controls whether we execute a buffered even instruction
            // or a buffered odd instruction.
            int doing_xde = cu.xde;
            int doing_xdo = cu.xdo;
            if (doing_xde) {
                if (cu.xdo)     // xec too common
                    log_msg(INFO_MSG, "CU", "XDE-EXEC even\n");
                else
                    log_msg(DEBUG_MSG, "CU", "XDE-EXEC even\n");
            } else if (doing_xdo) {
                log_msg(INFO_MSG, "CU", "XDE-EXEC odd\n");
                do_odd = 1;
            } else if (! cpu.ic_odd) {
                if (opt_debug)
                    log_msg(DEBUG_MSG, "CU", "Cycle = EXEC, even instr\n");
            } else {
                if (opt_debug)
                    log_msg(DEBUG_MSG, "CU", "Cycle = EXEC, odd instr\n");
                do_odd = 1;
            }
            if (do_odd) {
                // Our previously buffered odd location instruction may have
                // later been invalidated by a write to that location.
                if (cpu.irodd_invalid) {
                    cpu.irodd_invalid = 0;
                    if (cpu.cycle != FETCH_cycle) {
                        // Auto-breakpoint for multics, but not the T&D tape.
                        // The two tapes use different fault vectors.
                        if (switches.FLT_BASE == 2) {
                            // Multics boot tape
                            reason = STOP_BKPT;    /* stop simulation */
                            log_msg(NOTIFY_MSG, "CU", "Invalidating cached odd instruction; auto breakpoint\n");
                        } else
                            log_msg(NOTIFY_MSG, "CU", "Invalidating cached odd instruction.\n");
                        cpu.cycle = FETCH_cycle;
                    }
                    break;
                }
                //decode_instr(&cu.IR, cu.IRODD);
                decodeInstruction2(cu.IRODD, currentInstruction);
            }
            
            // Do we have a breakpoint here?
// HWR           if (sim_brk_summ) {
//                t_uint64 simh_addr = addr_emul_to_simh(get_addr_mode(), PPR.PSR, PPR.IC);
//                if (sim_brk_test (simh_addr, SWMASK ('E'))) {
//                    // BUG: misses breakpoints on target of xed, rpt, and
//                    // similar instructions because those instructions don't
//                    // update the IC.  Some of those instructions
//                    // do however provide their own breakpoint checks.
//                    log_msg(WARN_MSG, "CU", "Execution Breakpoint\n");
//                    reason = STOP_BKPT;    /* stop simulation */
//                    break;
//                }
//            }
            
            // We assume IC always points to the correct instr -- should
            // be advanced after even instr
            uint IC_temp = PPR.IC;
            // Munge PPR.IC for history debug
            if (cu.xde)
                PPR.IC = TPR.CA;
            else
                if (cu.xdo) {
                    // BUG: This lie may be wrong if prior instr updated TPR.CA, so
                    // we should probably remember the prior xde addr
                    PPR.IC = TPR.CA + 1;
                }
            ic_history_add();
            PPR.IC = IC_temp;
            
            //execute_ir();
            execute_ir(currentInstruction);  // HWR
            
            // Check for fault from instr
            // todo: simplify --- cycle won't be EXEC anymore
            // Note: events.any zeroed by fault handler prior to xed even
            // if other events are pending, so if it's on now, we have a
            // new fault
            
            // Only fault groups 1-6 are recognized here, not interrupts or
            // group 7 faults
            flag_t is_fault = events.any && events.low_group && events.low_group < 7;
            if (is_fault) {
                log_msg(WARN_MSG, "CU", "Probable fault detected after instruction execution\n");
                if (PPR.IC != IC_temp) {
                    // Our OPU always advances the IC, but should not do so
                    // on faults, so we restore it
                    log_msg(INFO_MSG, "CU", "Restoring IC to %06o (from %06o)\n",
                            IC_temp, PPR.IC);
                    // Note: Presumably, none of the instructions that change the PPR.PSR
                    // are capable of generating a fault after doing so...
                    PPR.IC = IC_temp;
                }
                if (doing_xde || doing_xdo) {
                    char *which = doing_xde ? "even" : "odd";
                    log_msg(WARN_MSG, "CU", "XED %s instruction terminated by fault.\n", which);
                    // Note that we don't clear xde and xdo because they might
                    // be about to be stored by scu. Since we'll be in a FAULT
                    // cycle next, both flags will be set as part of the fault
                    // handler's xed
                }
                if (cu.rpt) {
                    log_msg(WARN_MSG, "CU", "Repeat instruction terminated by fault.\n");
                    cu.rpt = 0;
                }
            }
            if (! is_fault) {
                // Special handling for RPT and other "repeat" instructions
                if (cu.rpt) {
                    if (cu.rpts) {
                        // Just executed the RPT instr.
                        cu.rpts = 0;    // finished "starting"
                        ++ PPR.IC;
                        if (! cpu.ic_odd)
                            cpu.ic_odd = 1;
                        else
                            cpu.cycle = FETCH_cycle;
                    } else {
                        // Executed a repeated instruction
                        // log_msg(WARN_MSG, "CU", "Address handing for repeated instr was probably wrong.\n");
                        // Check for tally runout or termination conditions
                        uint t = reg_X[0] >> 10; // bits 0..7 of 18bit register
                        if (cu.repeat_first && t == 0)
                            t = 256;
                        cu.repeat_first = 0;
                        --t;
                        reg_X[0] = ((t&0377) << 10) | (reg_X[0] & 01777);
                        // Note that we increment X[n] here, not in the APU.
                        // So, for instructions like cmpaq, the index register
                        // points to the entry after the one found.
                        int n = cu.tag & 07;
                        rX[n] += cu.delta;
                        if (opt_debug) log_msg(DEBUG_MSG, "CU", "Incrementing X[%d] by %#o to %#o.\n", n, cu.delta, reg_X[n]);
                        // Note that the code in bootload_tape.alm expects that
                        // the tally runout *not* be set when both the
                        // termination condition is met and bits 0..7 of
                        // reg X[0] hits zero.
                        if (t == 0) {
                            IR.tally_runout = 1;
                            SETF(rIR, I_TALLY);
                            
                            cu.rpt = 0;
                            if (opt_debug) log_msg(DEBUG_MSG, "CU", "Repeated instruction hits tally runout; halting rpt.\n");
                        }
                        // Check for termination conditions -- even if we hit
                        // the tally runout
                        // Note that register X[0] is 18 bits
                        int terminate = 0;
                        if (getbit18(reg_X[0], 11))
                            terminate |= TSTF(rIR, I_ZERO); //IR.zero;
                        if (getbit18(reg_X[0], 12))
                            terminate |= !TSTF(rIR, I_ZERO);   // ! IR.zero;
                        if (getbit18(reg_X[0], 13))
                            terminate |= TSTF(rIR, I_NEG);  //IR.neg;
                        if (getbit18(reg_X[0], 14))
                            terminate |= !TSTF(rIR, I_NEG); //! IR.neg;
                        if (getbit18(reg_X[0], 15))
                            terminate |= TSTF(rIR, I_CARRY);    //IR.carry;
                        if (getbit18(reg_X[0], 16))
                            terminate |= !TSTF(rIR, I_CARRY);    // ! IR.carry;
                        if (getbit18(reg_X[0], 17)) {
                            log_msg(DEBUG_MSG, "CU", "Checking termination conditions for overflows.\n");
                            // Process overflows -- BUG: what are all the
                            // types of overflows?
                            if (IR.overflow || IR.exp_overflow) {
                                if (IR.overflow_mask)
                                    IR.overflow = 1;
                                else
                                    fault_gen(overflow_fault);
                                terminate = 1;
                            }
                            if (TSTF(rIR, I_OFLOW) || TSTF(rIR, I_EOFL)) {
                                if (TSTF(rIR, I_OMASK))
                                    SETF(rIR, I_OFLOW);
                                else
                                    fault_gen(overflow_fault);
                                terminate = 1;
                            }

                        }
                        if (terminate) {
                            cu.rpt = 0;
                            log_msg(DEBUG_MSG, "CU", "Repeated instruction meets termination condition.\n");
                            IR.tally_runout = 0;
                            CLRF(rIR, I_TALLY);
                            // BUG: need IC incr, etc
                        } else {
                            if (! IR.tally_runout)
                            {
                                if (opt_debug>0) log_msg(DEBUG_MSG, "CU", "Repeated instruction will continue.\n");
                            }
                            if (!TSTF(rIR, I_TALLY))
                            {
                                if (opt_debug>0) log_msg(DEBUG_MSG, "CU", "Repeated instruction will continue.\n");
                            }

                        }
                    }
                    // TODO: if rpt double incr PPR.IC with wrap
                }
                // Retest cu.rpt -- we might have just finished repeating
                if (cu.rpt) {
                    // Don't do anything
                } else if (doing_xde) {
                    cu.xde = 0;
                    if (cpu.trgo) {
                        log_msg(NOTIFY_MSG, "CU", "XED even instruction was a transfer\n");
                        check_events();
                        cu.xdo = 0;
                        events.xed = 0;
                        cpu.cycle = FETCH_cycle;
                    } else {
                        if (cu.IR.is_eis_multiword) {
                            log_msg(WARN_MSG, "CU", "XEC/XED may mishandle EIS MW instructions; IC changed from %#o to %#o\n", IC_temp, PPR.IC);
                            (void) cancel_run(STOP_BUG);
                            -- PPR.IC;
                        }
                        if (cu.xdo)
                            log_msg(INFO_MSG, "CU", "Resetting XED even flag\n");
                        else {
                            // xec is very common, so use level debug
                            log_msg(DEBUG_MSG, "CU", "Resetting XED even flag\n");
                            ++ PPR.IC;
                        }
                        // BUG? -- do we need to reset events.xed if cu.xdo
                        // isn't set?  -- but xdo must be set unless xed
                        // doesn't really mean double...
                    }
                } else if (doing_xdo) {
                    log_msg(INFO_MSG, "CU", "Resetting XED odd flag\n");
                    cu.xdo = 0;
                    if (events.xed) {
                        events.xed = 0;
                        if (events.any)
                            log_msg(NOTIFY_MSG, "CU", "XED was from fault or interrupt; other faults and/or interrupts occured during XED\n");
                        else {
                            log_msg(NOTIFY_MSG, "CU", "XED was from fault or interrupt; checking if lower priority faults exist\n");
                            check_events();
                        }
                    }
                    cpu.cycle = FETCH_cycle;
                    if (!cpu.trgo) {
                        if (PPR.IC == IC_temp)
                            ++ PPR.IC;
                        else
                            if (cu.IR.is_eis_multiword)
                                log_msg(INFO_MSG, "CU", "Not updating IC after XED because EIS MW instruction updated the IC from %#o to %#o\n", IC_temp, PPR.IC);
                            else
                                log_msg(WARN_MSG, "CU", "No transfer instruction in XED, but IC changed from %#o to %#o\n", IC_temp, PPR.IC);
                    }
                } else if (! cpu.ic_odd) {
                    // Performed non-repeat instr at even loc (or finished the
                    // last repetition)
                    if (cpu.cycle == EXEC_cycle) {
                        // After an xde, we'll increment PPR.IC.   Setting
                        // cpu.ic_odd will be ignored.
                        if (cpu.trgo) {
                            // IC changed; previously fetched instr for odd location isn't any good now
                            cpu.cycle = FETCH_cycle;
                        } else {
                            if (PPR.IC == IC_temp) {
                                // cpu.ic_odd ignored if cu.xde or cu.xdo
                                cpu.ic_odd = 1; // execute odd instr of current pair
                                if (! cu.xde && ! cu.xdo)
                                    ++ PPR.IC;
                                else
                                    log_msg(DEBUG_MSG, "CU", "Not advancing IC after even instr because of xde/xdo\n");
                            } else {
                                if (cpu.irodd_invalid) {
                                    // possibly an EIS multi-word instr
                                } else
                                    log_msg(NOTIFY_MSG, "CU", "No transfer instruction and IRODD not invalidated, but IC changed from %#o to %#o; changing to fetch cycle\n", IC_temp, PPR.IC);
                                cpu.cycle = FETCH_cycle;
                            }
                        }
                    } else {
                        log_msg(WARN_MSG, "CU", "Changed from EXEC cycle to %d, not updating IC\n", cpu.cycle);
                    }
                } else {
                    // Performed non-repeat instr at odd loc (or finished last
                    // repetition)
                    if (cpu.cycle == EXEC_cycle) {
                        if (cpu.trgo) {
                            cpu.cycle = FETCH_cycle;
                        } else {
                            if (PPR.IC == IC_temp) {
                                if (cu.xde || cu.xdo)
                                    log_msg(INFO_MSG, "CU", "Not advancing IC or fetching because of cu.xde or cu.xdo.\n");
                                else {
                                    cpu.ic_odd = 0; // finished with odd half; BUG: restart issues?
                                    ++ PPR.IC;
                                    cpu.cycle = FETCH_cycle;
                                }
                            } else {
                                if (!cpu.irodd_invalid)
                                    log_msg(NOTIFY_MSG, "CU", "No transfer instruction and IRODD not invalidated, but IC changed from %#o to %#o\n", IC_temp, PPR.IC);  // DEBUGGING; BUG: this shouldn't happen?
                                cpu.cycle = FETCH_cycle;
                            }
                        }
                    } else {
                        log_msg(NOTIFY_MSG, "CU", "Cycle is %d after EXEC_cycle\n", cpu.cycle);
                        //cpu.cycle = FETCH_cycle;
                    }
                }
            }   // if (! is_fault)
        }   // case FAULT_EXEC_cycle
            break;
        default:
            log_msg(ERR_MSG, "CU", "Unknown cycle # %d\n", cpu.cycle);
            reason = STOP_BUG;
    }   // switch(cpu.cycle)
    
    return reason;
} 

//=============================================================================

/*
 *  sim_instr()
 *
 *  This function is called by SIMH to execute one or more instructions (or
 *  at least perform one or more processor cycles).
 *
 *  The principal elements of the 6180 processor are:
 *      The appending unit (addressing)
 *      The associative memory assembly (vitual memory registers)
 *      The control unit (responsible for addr mod, instr_mode, interrupt
 *      recognition, decode instr & indir words, timer registers
 *      The operation unit (binary arithmetic, boolean)
 *      The decimal unit (decimal arithmetic instructions, char string, bit
 *      string)
 *
 *  This code is essentially a wrapper for the "control unit" plus
 *  housekeeping such as debug controls, displays, and dropping in
 *  and out of the SIMH command processor.
 */

#define FEATURE_TIME_EXCL_EVENTS 1  // Don't count time spent in sim_process_event()
/*
 *  save_to_simh
 *
 *  Some of the data we give to SIMH is in simple encodings instead of
 *  the more complex structures used internal to the emulator.
 *
 */

static void save_to_simh(void)
{
    // Note that we record the *current* IC and addressing mode.  These may
    // have changed during instruction execution.
    
//    saved_IC = PPR.IC;
//    addr_modes_t mode = get_addr_mode();
//    saved_PPR = save_PPR(&PPR);
//    saved_PPR_addr = addr_emul_to_simh(mode, PPR.PSR, PPR.IC);
//    t_uint64 sIR;
//    save_IR(&sIR);
//    saved_IR = sIR & MASKBITS(18);
//    save_PR_registers();
//    
//    saved_BAR[0] = BAR.base;
//    saved_BAR[1] = BAR.bound;
//    saved_DSBR =
//    cpup->DSBR.stack | // 12 bits
//    (cpup->DSBR.u << 12) | // 1 bit
//    ((t_uint64) cpup->DSBR.bound << 13) | // 14 bits
//    ((t_uint64) cpup->DSBR.addr << 27); // 24 bits
}

//=============================================================================

/*
 *  restore_from_simh(void)
 *
 *  Some of the data we give to SIMH is in simple encodings instead of
 *  the more complex structures used internal to the emulator.
 *
 */

void restore_from_simh(void)
{
    
//    PPR.IC = saved_IC;
//    load_IR(&IR, saved_IR);
//    
//    restore_PR_registers();
//    BAR.base = saved_BAR[0];
//    BAR.bound = saved_BAR[1];
//    load_PPR(saved_PPR, &PPR);
//    PPR.IC = saved_IC;  // allow user to update "IC"
//    cpup->DSBR.stack = saved_DSBR & MASKBITS(12);
//    cpup->DSBR.u = (saved_DSBR >> 12) & 1;
//    cpup->DSBR.bound = (saved_DSBR >> 13) & MASKBITS(14);
//    cpup->DSBR.addr = (saved_DSBR >> 27) & MASKBITS(24);
//    
//    // Set default debug and check for a per-segment debug override
//    check_seg_debug();
}

void flush_logs()
{
    if (sim_log != NULL)
        fflush(sim_log);
    if (sim_deb != NULL)
        fflush(sim_deb);
}

t_stat sim_instr_NEW (void)
{
    /* Main instruction fetch/decode loop */
    adrTrace  = (cpu_dev.dctrl & DBG_ADDRMOD  ) && sim_deb; // perform address mod tracing
    apndTrace = (cpu_dev.dctrl & DBG_APPENDING) && sim_deb; // perform APU tracing
    unitTrace = (cpu_dev.dctrl & DBG_UNIT) && sim_deb;      // perform UNIT tracing
    
    currentInstruction->e = &E;
    
    restore_from_simh();
    // setup_streams(); // Route the C++ clog and cdebug streams to match SIMH settings
    
    int reason = 0;
    
//    if (! bootimage_loaded) {
//        // We probably should not do this
//        // See AL70, section 8
//        log_msg(WARN_MSG, "MAIN", "Memory is empty, no bootimage loaded yet\n");
//        reason = STOP_MEMCLEAR;
//    }
    
    //// opt_debug = (cpu_dev.dctrl != 0);    // todo: should CPU control all debug settings?
    
    //state_invalidate_cache();   // todo: only need to do when changing debug settings
    
    // Setup clocks
    (void) sim_rtcn_init(CLK_TR_HZ, TR_CLK);
    
    cancel = 0;
    
    uint32 start_cycles = sys_stats.total_cycles;
    sys_stats.n_instr = 0;
    uint32 start = sim_os_msec();
    uint32 delta = 0;
    
    // TODO: use sim_activate for the kbd poll
    // if (opt_debug && sim_interval > 32)
    if (opt_debug && sim_interval == NOQUEUE_WAIT) {
        // debug mode is slow, so be more responsive to keyboard interrupt
        sim_interval = 32;
    }
    
    int prev_seg = PPR.PSR;
    int prev_debug = opt_debug;
    // Loop until it's time to bounce back to SIMH
    //log_msg(DEBUG_MSG, "MAIN::CU", "Starting cycle loop; total cycles %lld; sim time is %f, %d events pending, first event at %d\n", sys_stats.total_cycles, sim_gtime(), sim_qcount(), sim_interval);
    while (reason == 0) {
        // HWR  
        if (sim_brk_summ && sim_brk_test (rIC, SWMASK ('E'))) {    /* breakpoint? */
            reason = STOP_BKPT;                        /* stop simulation */
            break;
        }
        
        //log_msg(DEBUG_MSG, "MAIN::CU", "Starting cycle %lld; sim time is %f, %d events pending, first event at %d\n", sys_stats.total_cycles, sim_gtime(), sim_qcount(), sim_interval);
        if (PPR.PSR != prev_seg) {
        //    check_seg_debug();
            prev_seg = PPR.PSR;
        }
        if (sim_interval<= 0) { /* check clock queue */
            // Process any SIMH timed events including keyboard halt
#if FEATURE_TIME_EXCL_EVENTS
            delta += sim_os_msec() - start;
#endif
            reason = sim_process_event();
#if FEATURE_TIME_EXCL_EVENTS
            start = sim_os_msec();
#endif
            if (reason != 0)
                break;
        }
#if 0
        uint32 t;
        {
            if ((t = sim_is_active(&TR_clk_unit)) == 0)
                ; // log_msg(DEBUG_MSG, "MAIN::clock", "TR is not running\n", t);
            else
                log_msg(DEBUG_MSG, "MAIN::clock", "TR is running with %d time units left.\n", t);
        }
#endif
        
//        if (prev_debug != opt_debug) {
//            // stack tracking depends on being called after every instr
//            state_invalidate_cache();
//            prev_debug = opt_debug;
//        }
        
        //
        // Log a message about where we're at -- if debugging, or if we want
        // procedure call tracing, or if we're displaying source code
        //
        
//        const int show_source_lines = 1;
//        int known_loc = 0;
//        uint saved_seg;
//        int saved_IC;
//        int saved_cycle;
//        if (opt_debug || show_source_lines) {
//            known_loc = show_location(show_source_lines) == 0;
//            saved_seg = PPR.PSR;
//            saved_IC = PPR.IC;
//            saved_cycle = cpu.cycle;
//        }
        
        //
        // Execute instructions (or fault or whatever)
        //
        
        reason = control_unit();
        
//        if (saved_cycle != FETCH_cycle) {
//            if (!known_loc)
//                known_loc = cpu.trgo;
//            if (/*known_loc && */ (opt_debug || show_source_lines)) {
//                // log_ignore_ic_change();
//                show_variables(saved_seg, saved_IC);
//                // log_notice_ic_change();
//            }
//        }
        
        //
        // And record history, etc
        //
        
        ++ sys_stats.total_cycles;
        sim_interval--; // todo: maybe only per instr or by brkpoint type?
//        if (opt_debug) {
//            log_ignore_ic_change();
//            state_dump_changes();
//            log_notice_ic_change();
//            // Save all registers etc so that we can detect/display changes
//            state_save();
//        }
        if (cancel) {
            if (reason == 0)
                reason = cancel;
#if 0
            if (reason == STOP_DIS) {¡
                // Until we implement something fancier, DIS will just freewheel...
                cpu.cycle = DIS_cycle;
                reason = 0;
                cancel = 0;
            }
#endif
        }
    }   // while (reason == 0)
    log_msg(DEBUG_MSG, "MAIN::CU", "Finished cycle loop; total cycles %lld; sim time is %f, %d events pending, first event at %d\n", sys_stats.total_cycles, sim_gtime(), sim_qcount(), sim_interval);

    if (!sim_quiet)
        printf("Finished cycle loop; total cycles %lld; sim time is %f, %d events pending, first event at %d\n", sys_stats.total_cycles, sim_gtime(), sim_qcount(), sim_interval);

    delta += sim_os_msec() - start;
    uint32 ncycles = sys_stats.total_cycles - start_cycles;
    sys_stats.total_msec += delta;
    sys_stats.total_instr += sys_stats.n_instr;
    if (delta > 500)
        log_msg(INFO_MSG, "CU", "Step: %.3f seconds: %d cycles at %d cycles/sec, %d instructions at %d instr/sec\n",
                (float) delta / 1000, ncycles, ncycles*1000/delta, sys_stats.n_instr, sys_stats.n_instr*1000/delta);

    //if (delta > 500)
    if (!sim_quiet)
        printf("Step: %.3f seconds: %d cycles at %d cycles/sec, %d instructions at %d instr/sec\n",
                (float) delta / 1000, ncycles, ncycles*1000/delta, sys_stats.n_instr, sys_stats.n_instr*1000/delta);

    save_to_simh();     // pack private variables into SIMH's world
    flush_logs();
    
    return reason;
}

#if 0
    enum atag_tm { atag_r = 0, atag_ri = 1, atag_it = 2, atag_ir = 3 };
    
    // CPU allows [2^6 .. 2^12]; multics uses 2^10
    static const int page_size = 1024;
    
    typedef struct {    // TODO: having a temp CA is ugly and doesn't match HW
        int32 soffset;      // Signed copy of CA (15 or 18 bits if from instr;
        // 18 bits if from indirect word)
        uint32 tag;
        flag_t more;
        enum atag_tm special;
    } ca_temp_t;

    /*
     * addr_mod()
     *
     * Called by OPU for most instructions
     * Generate 18bit computed address TPR.CA
     * Returns non-zero on error or group 1-6 fault
     *
     */
    
    int addr_mod(const instr_t *ip)
    {
        
        /*
         AL39,5-1: In abs mode, the appending unit is bypassed for instr
         fetches and *most* operand fetches and the final 18-bit computed
         address (TPR.CA) from addr prep becomes the main memory addr.
         
         ...
         Two modes -- absolute mode or appending mode.  [Various constucts] in
         absolute mode places the processor in append mode for one or more addr
         preparation cycles.  If a transfer of control is made with any of the
         above constructs, the proc remains in append mode after the xfer.
         */
        
        char *moi = "APU::addr-mod";
        
        // FIXME: do reg and indir word stuff first?
        
        TPR.is_value = 0;   // FIXME: Use "direct operand flag" instead
        TPR.value = 0135701234567;  // arbitrary junk ala 0xdeadbeef
        
        // Addr appending below
        
        addr_modes_t orig_mode = get_addr_mode();
        addr_modes_t addr_mode = orig_mode;
        int ptr_reg_flag = ip->mods.single.pr_bit;
        ca_temp_t ca_temp;  // FIXME: hack
        
        // FIXME: The following check should only be done after a sequential
        // instr fetch, not after a transfer!  We're only called by do_op(),
        // so this criteria is *almost* met.   Need to detect transfers.
        // Figure 6-10 claims we update the TPR.TSR segno as instructed by a "PR"
        // bit 29 only if we're *not* doing a sequential instruction fetch.
        
        ca_temp.tag = ip->mods.single.tag;
        
        if (cu.rpt) {
            // Special handling for repeat instructions
            TPR.TBR = 0;
            cu.tag = ca_temp.tag;
            uint td = ca_temp.tag & 017;
            int n = td & 07;
            if (cu.repeat_first) {
                if (opt_debug)
                    log_msg(DEBUG_MSG, moi,
                            "RPT: First repetition; incr will be 0%o(%d).\n",
                            ip->addr, ip->addr);
                TPR.CA = ip->addr;
                // FIXME: do we need to sign-extend to allow for negative "addresses"?
                ca_temp.soffset = ip->addr;
            } else {
                // Note that we don't add in a delta for X[n] here.   Instead the
                // CPU increments X[n] after every instruction.  So, for
                // instructions like cmpaq, the index register points to the entry
                // after the one found.
                TPR.CA = 0;
                ca_temp.soffset = 0;
                if(opt_debug)
                    log_msg(DEBUG_MSG, moi,
                            "RPT: X[%d] is 0%o(%d).\n", n, reg_X[n], reg_X[n]);
            }
        } else if (ptr_reg_flag == 0) {
            ca_temp.soffset = sign18(ip->addr);
            // TPR.TSR = PPR.PSR;   -- done prior to fetch_instr()
            // TPR.TRR = PPR.PRR;   -- done prior to fetch_instr()
            TPR.CA = ip->addr;
            TPR.TBR = 0;
        } else {
            if (cu.instr_fetch) {
                log_msg(ERR_MSG, moi,
                        "Bit 29 is on during an instruction fetch.\n");
                cancel_run(STOP_WARN);
            }
            if (orig_mode != APPEND_mode) {
                log_msg(NOTIFY_MSG, moi,
                        "Turning on APPEND mode for PR based operand.\n");
                set_addr_mode(addr_mode = APPEND_mode);
            }
            // AL39: Page 341, Figure 6-7 shows 3 bit PR & 15 bit offset
            int32 offset = ip->addr & MASKBITS(15);
            ca_temp.soffset = sign15(offset);
            uint pr = ip->addr >> 15;
            TPR.TSR = AR_PR[pr].PR.snr;     // FIXME: see comment above re figure 6-10
            TPR.TRR = max3(AR_PR[pr].PR.rnr, TPR.TRR, PPR.PRR);
            TPR.CA = (AR_PR[pr].wordno + ca_temp.soffset) & MASK18;
            TPR.TBR = AR_PR[pr].PR.bitno;
            int err = TPR.TBR < 0 || TPR.TBR > 35;
            if (opt_debug || err)
                log_msg(err ? WARN_MSG : DEBUG_MSG, moi,
                        "Using PR[%d]: TSR=0%o, TRR=0%o, CA=0%o(0%o+0%o<=>%d+%d), bitno=0%o\n",
                        pr, TPR.TSR, TPR.TRR, TPR.CA, AR_PR[pr].wordno, ca_temp.soffset,
                        AR_PR[pr].wordno, ca_temp.soffset, TPR.TBR);
            if (err) {
                log_msg(ERR_MSG, moi, "Bit offset %d outside range of 0..35\n",
                        TPR.TBR);
                cancel_run(STOP_BUG);
            }
            ca_temp.soffset = sign18(TPR.CA);
            
            // FIXME: Enter append mode & stay if execute a transfer -- fixed?
        }
        
        if (cu.instr_fetch) {
            uint p = is_priv_mode();    // Get priv bit from the SDW for TPR.TSR
            if (PPR.P != p)
                log_msg(INFO_MSG, moi, "PPR.P priv flag changing from %c to %c.\n",
                        (PPR.P) ? 'Y' : 'N', p ? 'Y' : 'N');
            PPR.P = p;
        }
        
        int op = ip->opcode;
        int bit27 = op % 2;
        op >>= 1;
        if (bit27 == 0) {
            if (op == opcode0_stca || op == opcode0_stcq)
                return 0;
            if (op == opcode0_stba || op == opcode0_stbq)
                return 0;
            if (op == opcode0_lcpr)
                return 0;
            if (op == opcode0_scpr)
                return 0;
        }
        
#if 0
        ???
        if eis multi-word
            variable = bits 0..17 (first 18 bits)
            int_inhibit = bit 28 // aka I
            mf1 = bits 29..36   // aka modification field
#endif
            
            ca_temp.more = 1;
        int mult = 0;
        
        while (ca_temp.more) {
            if (compute_addr(ip, &ca_temp) != 0) {
                if (ca_temp.more) log_msg(NOTIFY_MSG, moi, "Not-Final (not-incomplete) CA: 0%0o\n", TPR.CA);
                ca_temp.soffset = sign18(TPR.CA);
                // return 1;
            }
            if (ca_temp.more)
                mult = 1;
            ca_temp.soffset = sign18(TPR.CA);
            if (ca_temp.more)
                if(opt_debug>0)
                    log_msg(DEBUG_MSG, moi, "Post CA: Continuing indirect fetches\n");
#if 0
            if (ca_temp.more)
                log_msg(DEBUG_MSG, moi, "Pre Seg: Continuing indirect fetches\n");
            if (do_esn_segmentation(ip, &ca_temp) != 0) {
                log_msg(DEBUG_MSG, moi, "Final (incomplete) CA: 0%0o\n", TPR.CA);
                return 1;
            }
            if (ca_temp.more)
                log_msg(DEBUG_MSG, moi, "Post Seg: Continuing indirect fetches\n");
#endif
            if (ca_temp.more)
                mult = 1;
        }
        if (mult) {
            if(opt_debug>0) log_msg(DEBUG_MSG, moi, "Final CA: 0%0o\n", TPR.CA);
        }
        
        addr_mode = get_addr_mode();    // may have changed
        
        if (addr_mode == BAR_mode) {
            if (addr_mode == BAR_mode && ptr_reg_flag == 0) {
                // Todo: Add CA to BAR.base; add in PR; check CA vs bound
            }
            // FIXME: Section 4 says make sure CA cycle handled AR reg mode and
            // constants
            if (TPR.is_value) {
                log_msg(WARN_MSG, moi, "BAR mode not fully implemented.\n");
                return 0;
            } else {
                log_msg(ERR_MSG, moi, "BAR mode not implemented.\n");
                cancel_run(STOP_BUG);
                return 1;
            }
        }
        
        if (addr_mode == ABSOLUTE_mode && ptr_reg_flag == 0) {
            // TPR.CA is the 18-bit absolute main memory addr
            if (orig_mode != addr_mode)
                log_msg(DEBUG_MSG, moi, "finished\n");
            return 0;
        }
        
        // APPEND mode handled by fetch_word() etc
        
        if (orig_mode != addr_mode)
            log_msg(DEBUG_MSG, moi, "finished\n");
        return 0;
    }
    
    //=============================================================================
    
    /*
     * compute_addr()
     *
     * Perform a "CA" cycle as per figure 6-2 of AL39.
     *
     * Generate an 18-bit computed address (in TPR.CA) as specified in section 6
     * of AL39.
     * In our version, this may include replacing TPR.CA with a 36 bit constant or
     * other value if an appropriate modifier (e.g., du) is present.
     *
     */
    
    static int compute_addr(const instr_t *ip, ca_temp_t *ca_tempp)
    {
        ca_tempp->more = 0;
        
        // FIXME: Need to do ESN special handling if loop is continued
        
        // the and is a hint to the compiler for the following switch...
        enum atag_tm tm = (ca_tempp->tag >> 4) & 03;
        
        uint td = ca_tempp->tag & 017;
        
        ca_tempp->special = tm;
        
        if (cu.rpt) {
            // Check some requirements, but don't generate a fault (AL39 doesn't
            // say whether or not we should fault)
            if (tm != atag_r && tm != atag_ri)
                log_msg(ERR_MSG, "APU", "Repeated instructions must use register or register-indirect address modes.\n");
            else
                if (td == 0)
                    log_msg(ERR_MSG, "APU", "Repeated instructions should not use X[0].\n");
        }
        
        switch(tm) {
            case atag_r: {
                // Tm=0 -- register (r)
                if (td != 0)
                    reg_mod(td, ca_tempp->soffset);
                if (cu.rpt) {
                    int n = td & 07;
                    reg_X[n] = TPR.CA;
                }
                return 0;
            }
            case atag_ri: {
                // Tm=1 -- register then indirect (ri)
                if (td == 3 || td == 7) {
                    // ",du" or ",dl"
                    log_msg(WARN_MSG, "APU", "RI with td==0%o is illegal.\n", td);
                    fault_gen(illproc_fault);   // need illmod sub-category
                    return 1;
                }
                int off = ca_tempp->soffset;
                uint ca = TPR.CA;
                reg_mod(td, off);
                if(opt_debug)
                    log_msg(DEBUG_MSG, "APU",
                            "RI: pre-fetch:  TPR.CA=0%o <==  TPR.CA=%o + 0%o\n",
                            TPR.CA, ca, TPR.CA - ca);
                t_uint64 word;
                if (cu.rpt) {
                    int n = td & 07;
                    reg_X[n] = TPR.CA;
                    if (opt_debug>0) {
                        log_msg(DEBUG_MSG, "APU",
                                "RI for repeated instr: Setting X[%d] to CA 0%o(%d).\n",
                                n, reg_X[n], reg_X[n]);
                        log_msg(DEBUG_MSG, "APU",
                                "RI for repeated instr: Not doing address appending on CA.\n");
                    }
                    if (fetch_abs_word(TPR.CA, &word) != 0)
                        return 1;
                } else
                    if (addr_append(&word) != 0)
                        return 1;
                if(opt_debug>0) log_msg(DEBUG_MSG, "APU",
                                        "RI: fetch:  word at TPR.CA=0%o is 0%llo\n", TPR.CA, word);
                if (cu.rpt) {
                    // ignore tag and don't allow more indirection
                    return 0;
                }
                ca_tempp->tag = word & MASKBITS(6);
                if (TPR.CA % 2 == 0 && (ca_tempp->tag == 041 || ca_tempp->tag == 043)) {
                    int ret = do_its_itp(ip, ca_tempp, word);
                    if(opt_debug>0) log_msg(DEBUG_MSG, "APU",
                                            "RI: post its/itp: TPR.CA=0%o, tag=0%o\n",
                                            TPR.CA, ca_tempp->tag);
                    if (ret != 0) {
                        if (ca_tempp->tag != 0) {
                            log_msg(WARN_MSG, "APU",
                                    "RI: post its/itp: canceling remaining APU cycles.\n");
                            cancel_run(STOP_WARN);
                        }
                        return ret;
                    }
                } else {
                    TPR.CA = word >> 18;
                    if(opt_debug>0) log_msg(DEBUG_MSG, "APU",
                                            "RI: post-fetch: TPR.CA=0%o, tag=0%o\n",
                                            TPR.CA, ca_tempp->tag);
                }
                // break;   // Continue a new CA cycle
                ca_tempp->more = 1;     // Continue a new CA cycle
                // FIXME: flowchart says start CA, but we do ESN
                return 0;
            }
            case atag_it: {
                // Tm=2 -- indirect then tally (it)
                // FIXME: see "it" flowchart for looping (Td={15,17}
                switch(td) {
                    case 0:
                        log_msg(WARN_MSG, "APU", "IT with Td zero not valid in instr word.\n");
                        fault_gen(f1_fault);    // This mode not ok in instr word
                        break;
                    case 6:
                        // This mode not ok in an instr word
                        log_msg(WARN_MSG, "APU", "IT with Td six is a fault.\n");
                        fault_gen(fault_tag_2_fault);
                        break;
                    case 014: {
                        t_uint64 iword;
                        int ret;
                        int iloc = TPR.CA;
                        if ((ret = addr_append(&iword)) == 0) {
                            int addr = getbits36(iword, 0, 18);
                            int tally = getbits36(iword, 18, 12);
                            int tag = getbits36(iword, 30, 6);
                            ++tally;
                            tally &= MASKBITS(12);  // wrap from 4095 to zero
                            // FIXME: do we need to fault?
                            IR.tally_runout = (tally == 0);
                            if (IR.tally_runout)
                                log_msg(NOTIFY_MSG, "APU", "IT(di): tally runout\n");
                            --addr;
                            addr &= MASK18; // wrap from zero to 2^18-1
                            iword = setbits36(iword, 0, 18, addr);
                            iword = setbits36(iword, 18, 12, tally);
                            TPR.CA = addr;
                            if (opt_debug) {
                                // give context for appending msgs
                                log_msg(DEBUG_MSG, "APU",
                                        "IT(di): addr now 0%o, tally 0%o\n",
                                        addr, tally);
                            }
                            ret = store_word(iloc, iword);
                        }
                        return ret;
                    }
                        // case 015: more=1 depending upon tag
                    case 016: {
                        // mode "id" -- increment addr and decrement tally
                        t_uint64 iword;
                        int ret;
                        int iloc = TPR.CA;
                        if ((ret = addr_append(&iword)) == 0) {
                            int addr = getbits36(iword, 0, 18);
                            int tally = getbits36(iword, 18, 12);
                            int tag = getbits36(iword, 30, 6);
                            TPR.CA = addr;
                            --tally;
                            tally &= MASKBITS(12);  // wrap from zero to 4095
                            IR.tally_runout = (tally == 0); // NOTE: The Bpush macro usage in bootload_0 implies that we should *not* fault
                            ++addr;
                            addr &= MASK18; // wrap from 2^18-1 to zero
                            iword = setbits36(iword, 0, 18, addr);
                            iword = setbits36(iword, 18, 12, tally);
                            if (opt_debug) {
                                // give context for appending msgs
                                log_msg(DEBUG_MSG, "APU", "IT(id): addr now 0%o, tally 0%o\n", addr, tally);
                            }
                            ret = store_word(iloc, iword);
                        }
                        return ret;
                    }
                    default:
                        log_msg(ERR_MSG, "APU",
                                "IT with Td 0%o not implemented.\n", td);
                        cancel_run(STOP_BUG);
                        return 1;
                }
                break;
            }
            case atag_ir: {
                // Tm=3 -- indirect then register (ir)
                int nloops = 0;
                while(tm == atag_ir || tm == atag_ri) {
                    if (++nloops > 1)
                        log_msg(NOTIFY_MSG, "APU::IR", "loop # %d\n", nloops);
                    if (tm == atag_ir)
                        cu.CT_HOLD = td;
                    // FIXME: Maybe handle special tag (41 itp, 43 its).  Or post
                    // handle?
                    if(opt_debug>0) log_msg(DEBUG_MSG, "APU::IR",
                                            "pre-fetch: Td=0%o, TPR.CA=0%o\n", td, TPR.CA);
                    t_uint64 word;
                    if (addr_append(&word) != 0)
                        return 1;
                    if(opt_debug>0) log_msg(DEBUG_MSG, "APU::IR",
                                            "fetched:  word at TPR.CA=0%o is 0%llo:\n",
                                            TPR.CA, word);
                    ca_tempp->tag = word & MASKBITS(6);
                    if (TPR.CA % 2 == 0 &&
                        (ca_tempp->tag == 041 || ca_tempp->tag == 043))
                    {
                        int ret = do_its_itp(ip, ca_tempp, word);
                        if(opt_debug>0) log_msg(DEBUG_MSG, "APU::IR",
                                                "post its/itp: TPR.CA=0%o, tag=0%o\n",
                                                TPR.CA, ca_tempp->tag);
                        if (ret != 0) {
                            if (ca_tempp->tag != 0) {
                                log_msg(WARN_MSG, "APU",
                                        "IR: post its/itp: canceling remaining APU cycles.\n");
                                cancel_run(STOP_WARN);
                            }
                            return ret;
                        }
                    } else {
                        TPR.CA = word >> 18;
                        tm = (ca_tempp->tag >> 4) & 03;
                        td = ca_tempp->tag & 017;
                        if(opt_debug>0) log_msg(DEBUG_MSG, "APU::IR",
                                                "post-fetch: TPR.CA=0%o, tag=0%o, new tm=0%o; td = %o\n",
                                                TPR.CA, ca_tempp->tag, tm, td);
                        if (td == 0) {
                            // FIXME: Disallow a reg_mod() with td equal to
                            // NULL (AL39)
                            // Disallow always or maybe ok for ir?
                            log_msg(ERR_MSG, "APU::IR", "Found td==0 (for tm=0%o)\n", tm);
                            cancel_run(STOP_WARN);
                        }
                        switch(tm) {
                            case atag_ri:
                                log_msg(WARN_MSG, "APU::IR",
                                        "IR followed by RI.  Not tested\n");
                                reg_mod(td, ca_tempp->soffset);
                                break;      // continue looping
                            case atag_r:
                                reg_mod(cu.CT_HOLD, ca_tempp->soffset);
                                return 0;
                            case atag_it:
                                //reg_mod(td, ca_tempp->soffset);
                                log_msg(ERR_MSG, "APU::IR", "Need to run normal IT algorithm, ignoring fault 1.\n"); // actually cannot have fault 1 if disallow td=0 above
                                cancel_run(STOP_BUG);
                                return 0;
                            case atag_ir:
                                log_msg(WARN_MSG, "APU::IR", "IR followed by IR, continuing to loop.  Not tested\n");
                                cu.CT_HOLD = ca_tempp->tag & MASKBITS(4);
                                break;      // keep looping
                        }
                    }
                    //log_msg(WARN_MSG, "APU::IR", "Finished, but unverified.\n");
                    //cancel_run(STOP_WARN);
                    log_msg(DEBUG_MSG, "APU::IR", "Finished.\n");
                    return 0;
                }
            }
        }
        
        // FIXME: Need to do ESN special handling if loop is continued
        
        return 0;
    }
#endif
