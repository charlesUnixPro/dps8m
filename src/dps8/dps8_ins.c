/**
 * \file dps8_ins.c
 * \project dps8
 * \date 9/22/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"

word36 Ypair[2];        ///< 2-words
word36 Yblock8[8];      ///< 8-words
word36 Yblock16[16];    ///< 16-words

int bitfieldExtract(int a, int b, int c);
int bitfieldInsert(int a, int b, int c, int d);

DCDstruct *decodeInstruction(word36 inst, DCDstruct *dst);     // decode instruction into structure
DCDstruct *fetchInstruction(word18 addr, DCDstruct *dst);      // fetch (+ decode) instrcution at address

t_stat doInstruction(DCDstruct *i);

/**
 * writeOperand() - write (a potentially modified) CY to memory at TPR.CA using whatever modifications are necessary ...
 */
void
writeOperand(DCDstruct *i)
{
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "writeOperand(%s):mne=%s flags=%x\n", disAssemble(i->IWB), i->iwb->mne, i->iwb->flags);
    }
    // TPR.CA may be different from instruction spec because of various addr mod operations.
    // This is especially true in a R/M/W cycle such as stxn. So, restore it.
    
    if (i->iwb->flags == RMW)  /// Is this always the right thing todo???? or only for R/M/W instructions
    {
        TPR.CA = i->address;   // address from opcode
        rY = i->address;
    }
    doComputedAddressFormation(i, writeCY);
}

/**
 * writeOperand2() - write (a potentially modified) YPair to memory at TPR.CA/TPR.CA+1 
 */
void
writeOperand2(DCDstruct *i)//, word36 *YPair)
{
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "writeOperand2(%s):mne=%s flags=%x\n", disAssemble(i->IWB), i->iwb->mne, i->iwb->flags);
    }
    
    // TPR.CA may be different from instruction spec because of various addr mod operations.
    // This is especially true in a R/M/W cycle such as stxn. So, restore it.
    
    if (i->iwb->flags == RMW)  /// Is this always the right thing todo???? or only for R/M/W instructions
    {
        TPR.CA = i->address;   // address from opcode
        rY = i->address;
    }
    
    // XXX this may be way too simplistic ..... 
    Write2(i, TPR.CA, Ypair[0], Ypair[1], DataWrite, i->tag);
    
    
    // XXX may need to check for alignment restrictions/faults here ...
    
    //TPR.CA &= 0777776;  // make even address
    
    //CY = YPair[0];
    //doComputedAddressFormation(i, writeCY);
    
    //CY = YPair[1];
    //TPR.CA += 1;
    //doComputedAddressFormation(i, writeCY);
}

/**
 * get register value indicated by reg for Address Register operations
 * (not for use with address modifications)
 */
word18 getCrAR(word4 reg)
{
    if (reg == 0)
        return 0;
    
    if (reg & 010) /* Xn */
        return rX[X(reg)];
    
    switch(reg)
    {
        case TD_N:
            return 0;
        case TD_AU: ///< C(A)0,17
            return GETHI(rA);
        case TD_QU: ///<  C(Q)0,17
            return GETHI(rQ);
        case TD_IC: ///< C(PPR.IC)
            return PPR.IC;
        case TD_AL: ///< C(A)18,35
            return GETLO(rA);
        case TD_QL: ///< C(Q)18,35
            return GETLO(rQ);
    }
    return 0;
}

static t_uint64 scu_data[8];    // For SCU instruction

static void scu2words(t_uint64 *words)
{
    // BUG:  We don't track much of the data that should be tracked
    
    memset(words, 0, 8 * sizeof(*words));
    
    words[0] = setbits36(0, 0, 3, PPR.PRR);
    words[0] = setbits36(words[0], 3, 15, PPR.PSR);
    words[0] = setbits36(words[0], 18, 1, PPR.P);
    // 19 "b" XSF
    // 20 "c" SDWAMN
    words[0] = setbits36(words[0], 21, 1, cu.SD_ON);
    // 22 "e" PTWAM
    words[0] = setbits36(words[0], 23, 1, cu.PT_ON);
    // 24..32 various
    // 33-35 FCT
    
    // words[1]
    
    words[2] = setbits36(0, 0, 3, TPR.TRR);
    words[2] = setbits36(words[2], 3, 15, TPR.TSR);
    words[2] = setbits36(words[2], 27, 3, switches.cpu_num);
    words[2] = setbits36(words[2], 30, 6, cu.delta);
    
    words[3] = 0;
    words[3] = setbits36(words[3], 30, 6, TPR.TBR);
    
    //save_IR(&words[4]);
    words[4] = rIR; // HWR
    
    words[4] = setbits36(words[4], 0, 18, PPR.IC);
    
    words[5] = setbits36(0, 0, 18, TPR.CA);
    words[5] = setbits36(words[5], 18, 1, cu.repeat_first);
    words[5] = setbits36(words[5], 19, 1, cu.rpt);
    // BUG: Not all of CU data exists and/or is saved
    words[5] = setbits36(words[5], 24, 1, cu.xde);
    words[5] = setbits36(words[5], 24, 1, cu.xdo);
    words[5] = setbits36(words[5], 30, 6, cu.CT_HOLD);
    
    encode_instr(&cu.IR, &words[6]);    // BUG: cu.IR isn't kept fully up-to-date
    //words[6] = ins;  // I think HWR
    
    words[7] = cu.IRODD;
}

void cu_safe_store()
{
    // Save current Control Unit Data in hidden temporary so a later SCU instruction running
    // in FAULT mode can save the state as it existed at the time of the fault rather than
    // as it exists at the time the scu instruction is executed.
    scu2words(scu_data);
}

PRIVATE char *PRalias[] = {"ap", "ab", "bp", "bb", "lp", "lb", "sp", "sb" };

t_stat executeInstruction(DCDstruct *ci)
{
    word36 IWB  = ci->IWB;          ///< instruction working buffer
    opCode *iwb = ci->iwb;          ///< opCode *
    int32  opcode = ci->opcode;     ///< opcode
    bool   opcodeX = ci->opcodeX;   ///< opcode extension
    word18 address = ci->address;   ///< bits 0-17 of instruction XXX replace with rY
    bool   a = ci->a;               ///< bit-29 - addressing via pointer register
    bool   i = ci->i;               ///< interrupt inhibit bit.
    word6  tag = ci->tag;           ///< instruction tag XXX replace with rTAG
    
    TPR.CA = ci->address;           // address from opcode
    rY = ci->address;
    
    ci->stiTally = TSTF(rIR, I_TALLY);  // for sti instruction
    
    if ((cpu_dev.dctrl & DBG_TRACE) && sim_deb)
    {
        //if (processorAddressingMode == ABSOLUTE_MODE)
        if (get_addr_mode() == ABSOLUTE_MODE)
        {
            sim_debug(DBG_TRACE, &cpu_dev, "%06o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n", rIC, IWB, disAssemble(IWB), address, opcode, opcodeX, a, i, GET_TM(tag) >> 4, GET_TD(tag) & 017);
        }
        //if (processorAddressingMode == APPEND_MODE)
        if (get_addr_mode() == APPEND_mode)
        {
            sim_debug(DBG_TRACE, &cpu_dev, "%05o:%06o (%08o) %012llo (%s) %06o %03o(%d) %o %o %o %02o\n", PPR.PSR, rIC, finalAddress, IWB, disAssemble(IWB), address, opcode, opcodeX, a, i, GET_TM(tag) >> 4, GET_TD(tag) & 017);
        }
    }
    
    if (iwb->ndes == 0)
    {
        if (a && !(iwb->flags & IGN_B29))
        {
            doAddrModPtrReg(ci);
            
            // invoking bit-29 puts us into append mode ... usually
            //processorAddressingMode = APPEND_MODE;
            set_addr_mode(APPEND_mode);
        }
        
        // if instructions need an operand (CY) to operate, read it now. Else defer AM calcs until instruction execution
        // do any address modifications (and fetch operand if necessary)
        
        /*
         * XXX TODO: make read/write all operands automagic based on instruction flags. If READ_YPAIR then read in YPAIR prior to instruction. If STORE_YPAIR the store automatically after instruction exec ala STORE_OPERAND, etc.....
         
         */
        
        if (iwb->flags & (READ_OPERAND | PREPARE_CA ))
            doComputedAddressFormation(ci, (iwb->flags & READ_OPERAND) ? readCY : prepareCA);

        // XXX this may be too simplistic ....

        // ToDo: Read72 is also used to read in 72-bits, but not into Ypair! Fix this
        if (iwb->flags & READ_YPAIR)
            Read2(ci, TPR.CA, &Ypair[0], &Ypair[1], DataRead, rTAG);

        finalAddress = (word24)CY;
    }
    
    //t_stat ret = opcodeX ? DoEISInstruction(ci) : DoBasicInstruction(ci);
    t_stat ret = doInstruction(ci);
    
    cpuCycles += 1; // bump cycle counter
    
    if ((cpu_dev.dctrl & DBG_REGDUMP) && sim_deb)
    {
        sim_debug(DBG_REGDUMPAQI, &cpu_dev, "A=%012llo Q=%012llo IR:%s\n", rA, rQ, dumpFlags(rIR));
        
        sim_debug(DBG_REGDUMPFLT, &cpu_dev, "E=%03o A=%012llo Q=%012llo %.10Lg\n", rE, rA, rQ, EAQToIEEElongdouble());
        
        sim_debug(DBG_REGDUMPIDX, &cpu_dev, "X[0]=%06o X[1]=%06o X[2]=%06o X[3]=%06o\n", rX[0], rX[1], rX[2], rX[3]);
        sim_debug(DBG_REGDUMPIDX, &cpu_dev, "X[4]=%06o X[5]=%06o X[6]=%06o X[7]=%06o\n", rX[4], rX[5], rX[6], rX[7]);
        for(int n = 0 ; n < 8 ; n++)
        {
            sim_debug(DBG_REGDUMPPR, &cpu_dev, "PR[%d]/%s: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\n",
                      n, PRalias[n], PR[n].SNR, PR[n].RNR, PR[n].WORDNO, PR[n].BITNO);
        }
        for(int n = 0 ; n < 8 ; n++)
            sim_debug(DBG_REGDUMPADR, &cpu_dev, "AR[%d]: WORDNO=%06o CHAR:%o BITNO:%02o\n",
                      n, AR[n].WORDNO, AR[n].CHAR, AR[n].BITNO);
        sim_debug(DBG_REGDUMPPPR, &cpu_dev, "PRR:%o PSR:%05o P:%o IC:%06o\n", PPR.PRR, PPR.PSR, PPR.P, PPR.IC);
        sim_debug(DBG_REGDUMPDSBR, &cpu_dev, "ADDR:%08o BND:%05o U:%o STACK:%04o\n", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK);
    }
    
    return ret;
}

t_stat DoBasicInstruction(DCDstruct *i), DoEISInstruction(DCDstruct *i);    //, DoInstructionPair(DCDstruct *i);


t_stat doInstruction(DCDstruct *i)
{
    CLRF(rIR, I_MIIF);
    
    //if (i->e)
    if (i->iwb->ndes > 0)
    {
        i->e->ins = i;
        i->e->addr[0].e = i->e;
        i->e->addr[1].e = i->e;
        i->e->addr[2].e = i->e;
        
        i->e->addr[0].mat = OperandRead;   // no ARs involved yet
        i->e->addr[1].mat = OperandRead;   // no ARs involved yet
        i->e->addr[2].mat = OperandRead;   // no ARs involved yet
    }
    
    return i->opcodeX ? DoEISInstruction(i) : DoBasicInstruction(i);
}

//
//t_stat DoInstructionPair(DCDstruct *i, word18 CA)
//{
//    word36 Ypair[2];
//    core_read2(CA, &Ypair[0], &Ypair[1]);
//    
//    word18 thisIC = rIC;
//    t_stat retEven = DoInstruction(i, Ypair[0]);
//    
//    if (rIC != thisIC)   // last instruction changed rIC so don't execute odd word.
//        return retEven;
//    
//    return DoInstruction(i, Ypair[1]);
//}

word72 CYpair = 0;

word18 tmp18 = 0;

word36 tmp36 = 0;
word36 tmp36q = 0;      ///< tmp quotent
word36 tmp36r = 0;      ///< tmp remainder

word72 tmp72 = 0;
word72 trAQ = 0;     ///< a temporary C(AQ)
word36 trZ = 0;     ///< a temporary C(Z)
word1  tmp1 = 0;

int32 n;

//extern word18 stiTally;         ///< in dps8_cpu.c (only used for sti instruction)

extern EISstruct *e;            ///< for mw EIS ops
bool bPuls2 = false;

t_stat DoBasicInstruction(DCDstruct *i)
{
    int opcode  = i->opcode;  // get opcode
    
    switch (opcode)
    {
        //    FIXED-POINT ARITHMETIC INSTRUCTIONS
        
        /// ￼Fixed-Point Data Movement Load
        case opcode0_eaa:   // 0635:  ///< eaa
            rA = 0;
            SETHI(rA, TPR.CA);
            
            SCF(TPR.CA == 0, rIR, I_ZERO);
            SCF(TPR.CA & SIGN18, rIR, I_NEG);
            
            break;
            
        case opcode0_eaq:   // 0636:  ///< eaq
            rQ = 0;
            SETHI(rQ, TPR.CA);

            SCF(TPR.CA == 0, rIR, I_ZERO);
            SCF(TPR.CA & SIGN18, rIR, I_NEG);
            break;

        case opcode0_eax0:  // 0620:  ///< eax0
        case opcode0_eax1:  // 0621:  ///< eax1
        case opcode0_eax2:  // 0622:  ///< eax2
        case opcode0_eax3:  // 0623:  ///< eax3
        case opcode0_eax4:  // 0624:  ///< eax4
        case opcode0_eax5:  // 0625:  ///< eax5
        case opcode0_eax6:  // 0626:  ///< eax6
        case opcode0_eax7:  // 0627:  ///< eax7
            n = opcode & 07;  ///< get n
            rX[n] = TPR.CA;

            SCF(TPR.CA == 0, rIR, I_ZERO);
            SCF(TPR.CA & SIGN18, rIR, I_NEG);
            break;
            
        case opcode0_lca:   // 0335:  ///< lca
            rA = compl36(CY, &rIR);
            break;
            
        case opcode0_lcq:   // 0336:  ///< lcq
            rQ = compl36(CY, &rIR);
            break;
            
        case 0320:  ///< lcx0
        case 0321:  ///< lcx1
        case 0322:  ///< lcx2
        case 0323:  ///< lcx3
        case 0324:  ///< lcx4
        case 0325:  ///< lcx5
        case 0326:  ///< lcx6
        case 0327:  ///< lcx7
            n = opcode & 07;  // get n
            rX[n] = compl18(GETHI(CY), &rIR);
            break;
            
        case 0337:  ///< lcaq
            // The lcaq instruction changes the number to its negative while moving it from Y-pair to AQ. The operation is executed by forming the twos complement of the string of 72 bits. In twos complement arithmetic, the value 0 is its own negative. An overflow condition exists if C(Y-pair) = -2**71.
            
            //Read2(i, TPR.CA, &Ypair[0], &Ypair[1], DataRead, rTAG);
            
            if (Ypair[0] == 0400000000000LL && Ypair[1] == 0)
                SETF(rIR, I_OFLOW);
            else if (Ypair[1] == 0 && Ypair[1] == 0)
            {
                rA = 0;
                rQ = 0;
                
                SETF(rIR, I_ZERO);
                CLRF(rIR, I_NEG);
            }
            else
            {
                tmp72 = 0;

                tmp72 = bitfieldInsert72(tmp72, Ypair[0], 36, 36);
                tmp72 = bitfieldInsert72(tmp72, Ypair[1],  0, 36);
            
                tmp72 = ~tmp72 + 1;
            
                rA = bitfieldExtract72(tmp72, 36, 36);
                rQ = bitfieldExtract72(tmp72,  0, 36);
            
                if (rA == 0 && rQ == 0)
                    SETF(rIR, I_ZERO);
                else
                    CLRF(rIR, I_ZERO);
                if (rA & SIGN)
                    SETF(rIR, I_NEG);
                else
                    CLRF(rIR, I_NEG);
            }
            break;

        case 0235:  ///< lda
            rA = CY;
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 034: ///< ldac
            rA = CY;
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            CY = 0;
            
            break;
            
        case 0237:  ///< ldaq
            //Read2(i, TPR.CA, &rA, &rQ, DataRead, rTAG);
            rA = Ypair[0];
            rQ = Ypair[1];
            
            if (rA == 0 && rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            break;
            
            
        case 0634:  ///< ldi
            // C(Y)18,31 → C(IR)
            
            tmp18 = (GETLO(CY) >> 4) & 037777;  // 14-bits
            rIR = bitfieldInsert(rIR, tmp18, 4, 14);
            
            break;
            
        case 0236:  ///< ldq
            rQ = CY;
            
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
            
        case 0032: ///< ldqc
            rQ = CY;

            CY = 0;
            
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0220:  ///< ldx0
        case 0221:  ///< ldx1
        case 0222:  ///< ldx2
        case 0223:  ///< ldx3
        case 0224:  ///< ldx4
        case 0225:  ///< ldx5
        case 0226:  ///< ldx6
        case 0227:  ///< ldx7
            n = opcode & 07;  // get n
            rX[n] = GETHI(CY);
            
            if (rX[n] == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rX[n] & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0073:   ///< lreg
            ReadN(i, 8, TPR.CA, Yblock8, OperandRead, rTAG); // read 8-words from memory
            
            rX[0] = GETHI(Yblock8[0]);
            rX[1] = GETLO(Yblock8[0]);
            rX[2] = GETHI(Yblock8[1]);
            rX[3] = GETLO(Yblock8[1]);
            rX[4] = GETHI(Yblock8[2]);
            rX[5] = GETLO(Yblock8[2]);
            rX[6] = GETHI(Yblock8[3]);
            rX[7] = GETLO(Yblock8[3]);
            rA = Yblock8[4];
            rQ = Yblock8[5];
            rE = (GETHI(Yblock8[6]) >> 10) & 0377;   // need checking
            
            break;
            
        case 0720:  ///< lxl0
        case 0721:  ///< lxl1
        case 0722:  ///< lxl2
        case 0723:  ///< lxl3
        case 0724:  ///< lxl4
        case 0725:  ///< lxl5
        case 0726:  ///< lxl6
        case 0727:  ///< lxl7
            n = opcode & 07;  // get n
            rX[n] = GETLO(CY);
            
            if (rX[n] == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rX[n] & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        /// Fixed-Point Data Movement Store
        
        case 0753:  ///< sreg
            memset(Yblock8, 0, sizeof(Yblock8)); // clear block (changed to memset() per DJ request)
            
            SETHI(Yblock8[0], rX[0]);
            SETLO(Yblock8[0], rX[1]);
            SETHI(Yblock8[1], rX[2]);
            SETLO(Yblock8[1], rX[3]);
            SETHI(Yblock8[2], rX[4]);
            SETLO(Yblock8[2], rX[5]);
            SETHI(Yblock8[3], rX[6]);
            SETLO(Yblock8[3], rX[7]);
            Yblock8[4] = rA;
            Yblock8[5] = rQ;
            Yblock8[6] = SETHI(Yblock8[7], (word18)rE << 10);           // needs checking
            Yblock8[7] = ((rTR & 0777777777LL) << 9) | (rRALR & 07);    // needs checking
                    
            WriteN(i, 8, TPR.CA, Yblock8, OperandWrite, rTAG); // write 8-words to memory
            
            break;

        case 0755:  ///< sta
            /* check status */
            CY = rA;
            break;
            
        case 0354:  ///< stac
            if (CY == 0)
            {
                SETF(rIR, I_ZERO);
                CY = rA;
            } else
                CLRF(rIR, I_ZERO);
            break;
            
        case 0654:  ///< stacq
            if (CY == rQ)
            {
                CY = rA;
                SETF(rIR, I_ZERO);
                
            } else
                CLRF(rIR, I_ZERO);
            break;
            
        case 0757:  ///< staq
            //Write2(i, TPR.CA, rA, rQ, OperandWrite, rTAG);
            Ypair[0] = rA;
            Ypair[1] = rQ;
            
            break;
            
        case 0551:  ///< stba
            // 9-bit bytes of C(A) → corresponding bytes of C(Y), the byte positions
            // affected being specified in the TAG field.
            copyBytes((i->tag >> 2) & 0xf, rA, &CY);
            break;
            
        case 0552:  ///< stbq
            // 9-bit bytes of C(Q) → corresponding bytes of C(Y), the byte positions
            // affected being specified in the TAG field.
            copyBytes((i->tag >> 2) & 0xf, rQ, &CY);
            
            break;

        case 0554:  ///< stc1
            //tmp36 = 0;
            SETHI(CY, (rIC + 1) & 0777777);
            SETLO(CY, rIR & 0777760);
            
            break;
            
        case 0750:  ///< stc2
            SETHI(CY, (rIC + 2) & 0777777);

            break;
            
        case 0751: //< stca
            /// Characters of C(A) → corresponding characters of C(Y), the character
            /// positions affected being specified in the TAG field.
            copyChars(i->tag, rA, &CY);

            break;
            
        case 0752: ///< stcq
            /// Characters of C(Q) → corresponding characters of C(Y), the character
            /// positions affected being specified in the TAG field.
            copyChars(i->tag, rQ, &CY);
            break;
            
        case 0357: //< stcd
            // C(PPR) → C(Y-pair) as follows:
            
            //  000 → C(Y-pair)0,2
            //  C(PPR.PSR) → C(Y-pair)3,17
            //  C(PPR.PRR) → C(Y-pair)18,20
            //  00...0 → C(Y-pair)21,29
            //  (43)8 → C(Y-pair)30,35
            
            //  C(PPR.IC)+2 → C(Y-pair)36,53
            //  00...0 → C(Y-pair)54,71
            
            Ypair[0] = 0;
            Ypair[0] = bitfieldInsert36(Ypair[0], PPR.PSR, 18, 15);
            Ypair[0] = bitfieldInsert36(Ypair[0], PPR.PRR, 15,  3);
            Ypair[0] = bitfieldInsert36(Ypair[0],     043,  0,  6);
            
            Ypair[1] = 0;
            Ypair[1] = bitfieldInsert36(Ypair[0], rIC + 2, 18, 18);
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);

            break;
            
            
        case 0754: ///< sti
            
            // C(IR) → C(Y)18,31
            // 00...0 → C(Y)32,35
            
            /// The contents of the indicator register after address preparation are stored in C(Y)18,31  C(Y)18,31 reflects the state of the tally runout indicator prior to address preparation. The relation between C(Y)18,31 and the indicators is given in Table 4-5.
            
            SETLO(CY, ((rIR | i->stiTally) & 0000000777760LL));
            break;
            
        case 0756: ///< stq
            CY = rQ;
            break;

        case 0454:  ///< stt
            /// XXX need to implement timer
            
            return STOP_UNIMP;
            break;
            
            
        case 0740:  ///< stx0
        case 0741:  ///< stx1
        case 0742:  ///< stx2
        case 0743:  ///< stx3
        case 0744:  ///< stx4
        case 0745:  ///< stx5
        case 0746:  ///< stx6
        case 0747:  ///< stx7
            n = opcode & 07;  // get n
            SETHI(CY, rX[n]);

            break;
        
        case 0450: ///< stz
            CY = 0;
            break;
        
        case 0440:  ///< sxl0
        case 0441:  ///< sxl1
        case 0442:  ///< sxl2
        case 0443:  ///< sxl3
        case 0444:  ///< sxl4
        case 0445:  ///< sxl5
        case 0446:  ///< sxl6
        case 0447:  ///< sxl7
            n = opcode & 07;  // get n
            
            SETLO(CY, rX[n]);

            break;
        
        /// Fixed-Point Data Movement Shift
        case 0775:  ///< alr
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            for(int i = 0 ; i < tmp36 ; i++)
            {
                bool a0 = rA & SIGN;    ///< A0
                rA <<= 1;               // shift left 1
                if (a0)                 // rotate A0 -> A35
                    rA |= 1;
            }
            rA &= DMASK;    // keep to 36-bits
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
        
        case 0735:  ///< als
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            
            rA <<= tmp36;
            rA &= DMASK;    // keep to 36-bits
            
            
            if (rA & 0xfffffff000000000LL)  // any bit shifted out???
                SETF(rIR, I_CARRY);
            else
                CLRF(rIR, I_CARRY);
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0771:  ///< arl
            /// Shift C(A) right the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with zeros.
            
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            
            rA >>= tmp36;
            rA &= DMASK;    // keep to 36-bits
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
            
        case 0731:  ///< ars
            /// Shift C(A) right the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with initial C(A)0.
            
            tmp18 = TPR.CA & 0177;   // CY bits 11-17

            for(int i = 0 ; i < tmp18 ; i++)
            {
                bool a0 = rA & SIGN;    ///< A0
                rA >>= 1;               // shift right 1
                if (a0)                 // propagate sign bit
                    rA |= SIGN;
            }
            rA &= DMASK;    // keep to 36-bits
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0777:  ///< llr

            /// Shift C(AQ) left by the number of positions given in C(TPR.CA)11,17;
            /// entering each bit leaving AQ0 into AQ71.

            tmp36 = TPR.CA & 0177;      // CY bits 11-17
            for(int i = 0 ; i < tmp36 ; i++)
            {
                bool a0 = rA & SIGN;    ///< A0
                
                rA <<= 1;               // shift left 1
                
                bool b0 = rQ & SIGN;    ///< Q0
                if (b0)
                    rA |= 1;            // Q0 => A35
                
                rQ <<= 1;               // shift left 1
                
                if (a0)                 // propagate A sign bit
                    rQ |= 1;
            }
            
            rA &= DMASK;    // keep to 36-bits
            rQ &= DMASK;
            
            if (rA == 0 && rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);

            break;
            
        case 0737:  ///< lls
            /// Shift C(AQ) left the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with zeros.
            
            CLRF(rIR, I_CARRY);
            
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            for(int i = 0 ; i < tmp36 ; i++)
            {
                rA <<= 1;               // shift left 1
            
                if (rA & 0xfffffff000000000LL)  // any bits shifted out???
                    SETF(rIR, I_CARRY);
                
                bool b0 = rQ & SIGN;    ///< Q0
                if (b0)
                    rA |= 1;            // Q0 => A35
                
                rQ <<= 1;               // shift left 1
            }
                            
            rA &= DMASK;    // keep to 36-bits
            rQ &= DMASK;
            
            if (rA == 0 && rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);

            break;

        case 0773:  ///< lrl
            /// Shift C(AQ) right the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with zeros.
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            for(int i = 0 ; i < tmp36 ; i++)
            {
                bool a35 = rA & 1;      ///< A35
                rA >>= 1;               // shift right 1
                
                rQ >>= 1;               // shift right 1
                
                if (a35)                // propagate sign bit
                    rQ |= SIGN;
            }
            rA &= DMASK;    // keep to 36-bits
            rQ &= DMASK;
            
            if (rA == 0 && rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);

            break;

        case 0733:  ///< lrs
            /// Shift C(AQ) right the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with initial C(AQ)0.
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            for(int i = 0 ; i < tmp36 ; i++)
            {
                bool a0 = rA & SIGN;    ///< A0
                bool a35 = rA & 1;      ///< A35
                
                rA >>= 1;               // shift right 1
                if (a0)
                    rA |= SIGN;
                
                rQ >>= 1;               // shift right 1
                if (a35)                // propagate sign bit1
                    rQ |= SIGN;
            }
            rA &= DMASK;    // keep to 36-bits (probably ain't necessary)
            rQ &= DMASK;
            
            if (rA == 0 && rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);

            break;
         
        case 0776:  ///< qlr
            /// Shift C(Q) left the number of positions given in C(TPR.CA)11,17; entering
            /// each bit leaving Q0 into Q35.
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            for(int i = 0 ; i < tmp36 ; i++)
            {
                bool q0 = rQ & SIGN;    ///< Q0
                rQ <<= 1;               // shift left 1
                if (q0)                 // rotate A0 -> A35
                    rQ |= 1;
            }
            rQ &= DMASK;    // keep to 36-bits
            
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
    
        case 0736:  ///< qls
            // Shift C(Q) left the number of positions given in C(TPR.CA)11,17; fill vacated positions with zeros.
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            
            rQ <<= tmp36;
            
            
            if (rQ & 0xfffffff000000000LL)  // any bit shifted out???
                SETF(rIR, I_CARRY);
            else
                CLRF(rIR, I_CARRY);
            
            rQ &= DMASK;    // keep to 36-bits (after we test for carry)
            
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0772:  ///< qrl
            /// Shift C(Q) right the number of positions specified by Y11,17; fill vacated
            /// positions with zeros.
        
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            
            rQ >>= tmp36;
            rQ &= DMASK;    // keep to 36-bits
            
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0732:  ///< qrs
            /// Shift C(Q) right the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with initial C(Q)0.
            tmp36 = TPR.CA & 0177;   // CY bits 11-17
            for(int i = 0 ; i < tmp36 ; i++)
            {
                bool q0 = rQ & SIGN;    ///< Q0
                rQ >>= 1;               // shift right 1
                if (q0)                 // propagate sign bit
                    rQ |= SIGN;
            }
            rQ &= DMASK;    // keep to 36-bits
            
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        /// Fixed-Point Addition
        case 0075:  ///< ada
            /**
              \brief C(A) + C(Y) → C(A)
              Modifications: All
             
            (Indicators not listed are not affected)
            ZERO: If C(A) = 0, then ON; otherwise OFF
            NEG: If C(A)0 = 1, then ON; otherwise OFF
            OVR: If range of A is exceeded, then ON
            CARRY: If a carry out of A0 is generated, then ON; otherwise OFF
             
            XXX: check Michael Mondy's notes on 36-bit addition and T&D stuff. Need to reimplement code
             
             
            */
        
            rA = AddSub36b('+', true, rA, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;
         
        case 077:   ///< adaq
            // C(AQ) + C(Y-pair) → C(AQ)
            Read72(i, TPR.CA, &tmp72, OperandRead, rTAG);
            
            tmp72 = AddSub72b('+', true, convertToWord72(rA, rQ), tmp72, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            convertToWord36(tmp72, &rA, &rQ);
            
            break;
            
        case 033:   ///< adl
            // C(AQ) + C(Y) sign extended → C(AQ)
            tmp72 = SIGNEXT72(CY); // sign extend Cy
            tmp72 = AddSub72b('+', true, convertToWord72(rA, rQ), tmp72, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            convertToWord36(tmp72, &rA, &rQ);
            
            break;
            
            
        case 037:   ///< adlaq
            /// The adlaq instruction is identical to the adaq instruction with the exception that the overflow indicator is not affected by the adlaq instruction, nor does an overflow fault occur. Operands and results are treated as unsigned, positive binary integers.
            /// C(AQ) + C(Y-pair) → C(AQ)
            Read72(i, TPR.CA, &tmp72, OperandRead, rTAG);
            
            tmp72 = AddSub72b('+', true, convertToWord72(rA, rQ), tmp72, I_ZERO|I_NEG|I_CARRY, &rIR);
            convertToWord36(tmp72, &rA, &rQ);
            
            break;
            
        case 035:   ///< adla
            /** The adla instruction is identical to the ada instruction with the exception that the overflow indicator is not affected by the adla instruction, nor does an overflow fault occur. Operands and results are treated as unsigned, positive binary integers. */
            rA = AddSub36b('+', false, rA, CY, I_ZERO|I_NEG|I_CARRY, &rIR);
            break;
            
        case 036:   ///< adlq
            /** The adlq instruction is identical to the adq instruction with the exception that the overflow indicator is not affected by the adlq instruction, nor does an overflow fault occur. Operands and results are treated as unsigned, positive binary integers. */
            rQ = AddSub36b('+', false, rQ, CY, I_ZERO|I_NEG|I_CARRY, &rIR);
            break;
            
        case 020:   ///< adlx0
        case 021:   ///< adlx1
        case 022:   ///< adlx2
        case 023:   ///< adlx3
        case 024:   ///< adlx4
        case 025:   ///< adlx5
        case 026:   ///< adlx6
        case 027:   ///< adlx7
            n = opcode & 07;  // get n
            rX[n] = AddSub18b('+', false, rX[n], GETHI(CY), I_ZERO|I_NEG|I_CARRY, &rIR);
            break;
            
        case 076:   ///< adq
            rQ = AddSub36b('+', true, rQ, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;

        case 060:   ///< adx0
        case 061:   ///< adx1
        case 062:   ///< adx2
        case 063:   ///< adx3
        case 064:   ///< adx4
        case 065:   ///< adx5
        case 066:   ///< adx6
        case 067:   ///< adx7
            n = opcode & 07;  // get n
            rX[n] = AddSub18b('+', true, rX[n], GETHI(CY), I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;
        
        case 054:   ///< aos
            /// C(Y)+1→C(Y)
            
            tmp36 = AddSub36b('+', true, CY, 1, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;
        
        case 055:   ///< asa
            /// C(A) + C(Y) → C(Y)
            CY = AddSub36b('+', true, rA, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;
            
        case 056:   ///< asq
            /// C(Q) + C(Y) → C(Y)
            CY = AddSub36b('+', true, rQ, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;
         
        case 040:   ///< asx0
        case 041:   ///< asx1
        case 042:   ///< asx2
        case 043:   ///< asx3
        case 044:   ///< asx4
        case 045:   ///< asx5
        case 046:   ///< asx6
        case 047:   ///< asx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///    \brief C(Xn) + C(Y)0,17 → C(Y)0,17
            
            n = opcode & 07;  // get n
            tmp18 = AddSub18b('+', true, rX[n], GETHI(CY), I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            SETHI(CY, tmp18);
            
            break;

        case 071:   ///< awca
            /// If carry indicator OFF, then C(A) + C(Y) → C(A)
            /// If carry indicator ON, then C(A) + C(Y) + 1 → C(A)
            if (TSTF(rIR, I_CARRY))
                rA = AddSub36b('+', true, rA, 1, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            rA = AddSub36b('+', true, rA, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            
            break;
            
            
        case 072:   ///< awcq
            /// If carry indicator OFF, then C(Q) + C(Y) → C(Q)
            /// If carry indicator ON, then C(Q) + C(Y) + 1 → C(Q)
            if (TSTF(rIR, I_CARRY))
                rQ = AddSub36b('+', true, rQ, 1, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            rQ = AddSub36b('+', true, rQ, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            
            break;
           
        /// Fixed-Point Subtraction
            
        case 0175:  ///< sba
            /// C(A) - C(Y) → C(A)
            rA = AddSub36b('-', true, rA, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;
         
        case 0177:  ///< sbaq
            /// C(AQ) - C(Y-pair) → C(AQ)
            Read72(i, TPR.CA, &tmp72, OperandRead, rTAG);
            
            tmp72 = AddSub72b('-', true, convertToWord72(rA, rQ), tmp72, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            convertToWord36(tmp72, &rA, &rQ);
            break;
          
        case 0135:  ///< sbla
            /// C(A) - C(Y) → C(A) logical
            rA = AddSub36b('-', false, rA, CY, I_ZERO|I_NEG|I_CARRY, &rIR);
            break;
            
        case 0137:  ///< sblaq
            /// The sblaq instruction is identical to the sbaq instruction with the exception that the overflow indicator is not affected by the sblaq instruction, nor does an overflow fault occur. Operands and results are treated as unsigned, positive binary integers.
            /// \brief C(AQ) - C(Y-pair) → C(AQ)
            Read72(i, TPR.CA, &tmp72, OperandRead, rTAG);
            
            tmp72 = AddSub72b('-', true, convertToWord72(rA, rQ), tmp72, I_ZERO|I_NEG| I_CARRY, &rIR);
            convertToWord36(tmp72, &rA, &rQ);
            break;
            
        case 0136:  ///< sblq
            ///< C(Q) - C(Y) → C(Q)
            rQ = AddSub36b('-', false, rQ, CY, I_ZERO|I_NEG|I_CARRY, &rIR);
            break;
            
        case 0120:  ///< sblx0
        case 0121:  ///< sblx1
        case 0122:  ///< sblx2
        case 0123:  ///< sblx3
        case 0124:  ///< sblx4
        case 0125:  ///< sblx5
        case 0126:  ///< sblx6
        case 0127:  ///< sblx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief     C(Xn) - C(Y)0,17 → C(Xn)
            n = opcode & 07;  // get n
            rX[n] = AddSub18b('-', false, rX[n], GETHI(CY), I_ZERO|I_NEG|I_CARRY, &rIR);
            break;
         
        case 0176:  ///< sbq
            /// C(Q) - C(Y) → C(Q)
            rQ = AddSub36b('-', true, rQ, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;
            
        case 0160:  ///< sbx0
        case 0161:  ///< sbx1
        case 0162:  ///< sbx2
        case 0163:  ///< sbx3
        case 0164:  ///< sbx4
        case 0165:  ///< sbx5
        case 0166:  ///< sbx6
        case 0167:  ///< sbx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief  C(Xn) - C(Y)0,17 → C(Xn)
            n = opcode & 07;  // get n
            rX[n] = AddSub18b('-', true, rX[n], GETHI(CY), I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            break;

        case 0155:  ///< ssa
            /// C(A) - C(Y) → C(Y)
            CY = AddSub36b('-', true, rA, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);

            break;

        case 0156:  ///< ssq
            /// C(Q) - C(Y) → C(Y)
            CY = AddSub36b('-', true, rQ, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);

            break;
        
        case 0140:  ///< ssx0
        case 0141:  ///< ssx1
        case 0142:  ///< ssx2
        case 0143:  ///< ssx3
        case 0144:  ///< ssx4
        case 0145:  ///< ssx5
        case 0146:  ///< ssx6
        case 0147:  ///< ssx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Xn) - C(Y)0,17 → C(Y)0,17
            n = opcode & 07;  // get n
            tmp18 = AddSub18b('-', true, rX[n], GETHI(CY), I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            SETHI(CY, tmp18);
            
            break;

            
        case 0171:  ///< swca
            /// If carry indicator ON, then C(A)- C(Y) → C(A)
            /// If carry indicator OFF, then C(A) - C(Y) - 1 → C(A)
            if (!TSTF(rIR, I_CARRY))
                rA = AddSub36b('-', true, rA, 1, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            rA = AddSub36b('-', true, rA, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            
            break;
         
        case 0172:  ///< swcq
            /// If carry indicator ON, then C(Q) - C(Y) → C(Q)
            /// If carry indicator OFF, then C(Q) - C(Y) - 1 → C(Q)
            if (!TSTF(rIR, I_CARRY))
                rQ = AddSub36b('-', true, rQ, 1, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            rQ = AddSub36b('-', true, rQ, CY, I_ZERO|I_NEG|I_OFLOW|I_CARRY, &rIR);
            
            break;
        
        /// Fixed-Point Multiplication
        case 0401:  ///< mpf
            /// C(A) × C(Y) → C(AQ), left adjusted
            /**
             * Two 36-bit fractional factors (including sign) are multiplied to form a 71- bit fractional product (including sign), which is stored left-adjusted in the AQ register. AQ71 contains a zero. Overflow can occur only in the case of A and Y containing negative 1 and the result exceeding the range of the AQ register.
             */
            
            tmp72 = (word72)rA * (word72)CY;
            tmp72 &= MASK72;
            tmp72 <<= 1;    // left adjust so AQ71 contains 0
            
            if (rA == MAXNEG && CY == MAXNEG) // Overflow can occur only in the case of A and Y containing negative 1
                SETF(rIR, I_OFLOW);

            convertToWord36(tmp72, &rA, &rQ);
            SCF(rA == 0 && rQ == 0, rIR, I_ZERO);
            SCF(rA & SIGN, rIR, I_NEG);
            
            break;

        case 0402:  ///< mpy
            /// C(Q) × C(Y) → C(AQ), right adjusted
            
            /// XXX need todo some sign extension here!!!!!!
            tmp72 = (word72)rQ * (word72)CY;
            tmp72 &= MASK72;
            
            convertToWord36(tmp72, &rA, &rQ);
            SCF(rA == 0 && rQ == 0, rIR, I_ZERO);
            
            // do this extended sign thang
            /* Two 36-bit integer factors (including sign) are multiplied to form a 71-bit integer product (including sign) which is stored right-adjusted in the AQ- register. AQ0 is filled with an "extended sign bit".
            ￼   In the case of (-2*35) × (-2**35) = +2**70, AQ1 is used to represent the product rather than the sign. No overflow can occur.
             */
            
            // XXX this is probably the wrong way of doing it, but let's hope testing sorts this out.
            if (rA & SIGNEX)
                rA |= SIGN;
            
            SCF(rA & SIGN, rIR, I_NEG);
            break;
            
        /// Fixed-Point Division
        case 0506:  ///< div
            /// C(Q) / (Y) integer quotient → C(Q), integer remainder → C(A)
            /**
             * A 36-bit integer dividend (including sign) is divided by a 36-bit integer divisor (including sign) to form a 36-bit integer quotient (including sign) and a 36-bit integer remainder (including sign). The remainder sign is equal to the dividend sign unless the remainder is zero.
               If the dividend = -2**35 and the divisor = -1 or if the divisor = 0, then division does not take place. Instead, a divide check fault occurs, C(Q) contains the dividend magnitude, and the negative indicator reflects the dividend sign.
             */
            
            if ((rQ == MAXNEG && CY == NEG136) || (CY == 0))
            {
                // no division takes place
                SCF(CY == 0, rIR, I_ZERO);
                SCF(rQ & SIGN, rIR, I_NEG);
                // XXX divide check fault
            }
            else
            {
                // XXX need to fix to perform signed arithmetic
                rA = SIGNEXT36(rQ) % SIGNEXT36(CY);   // remainder 1st to keep rQ
                rQ = SIGNEXT36(rQ) / SIGNEXT36(CY);
                
                SCF(rQ == 0, rIR, I_ZERO);
                SCF(rQ & SIGN, rIR, I_NEG);
            }
            
            rA &= DMASK;
            rQ &= DMASK;
            
            break;
            
        case 0507:  ///< dvf
            /// C(AQ) / (Y)
            ///  fractional quotient → C(A)
            ///  fractional remainder → C(Q)
            
            /// A 71-bit fractional dividend (including sign) is divided by a 36-bit fractional divisor yielding a 36-bit fractional quotient (including sign) and a 36-bit fractional remainder (including sign). C(AQ)71 is ignored; bit position 35 of the remainder corresponds to bit position 70 of the dividend. The
            ///  remainder sign is equal to the dividend sign unless the remainder is zero.
            /// If | dividend | >= | divisor | or if the divisor = 0, division does not take place. Instead, a divide check fault occurs, C(AQ) contains the dividend magnitude in absolute, and the negative indicator reflects the dividend sign.
            
            // XXX Untested, needs further testing.
            dvf(i);
            
            break;
            
        /// Fixed-Point Negate
        case 0531:  ///< neg
            /// -C(A) → C(A) if C(A) ≠ 0
            /// XXX: what if C(A) == 0? Are flags affected? Assume yes, for now.
            
            if (rA != 0)
            {
                bool ov = rA & 0400000000000LL;
                
                rA = -rA;
 
                rA &= DMASK;    // keep to 36-bits
                
                if (rA == 0)
                    SETF(rIR, I_ZERO);
                else
                    CLRF(rIR, I_ZERO);
                
                if (rA & SIGN)
                    SETF(rIR, I_NEG);
                else
                    CLRF(rIR, I_NEG);
                
                //if (rA > MAX36) // XXX can we even generate overflow with the DMASK? revisit this
                if (ov)
                    SETF(rIR, I_OFLOW);
            }
            break;
            
        case 0533:  ///< negl
            /// -C(AQ) → C(AQ) if C(AQ) ≠ 0
            /// XXX same problem as neg above - fixed
            
            tmp72 = convertToWord72(rA, rQ);
            if (tmp72 != 0)
            {
                bool ov = (rA & 0400000000000LL) & (rQ == 0);
                
                tmp72 = -tmp72;
                
                if (tmp72 == 0)
                    SETF(rIR, I_ZERO);
                else
                    CLRF(rIR, I_ZERO);
                
                if (tmp72 & SIGN72)
                    SETF(rIR, I_NEG);
                else
                    CLRF(rIR, I_NEG);
                
                //if (tmp72 > MAX72)
                if (ov)
                    SETF(rIR, I_OFLOW);
                
                convertToWord36(tmp72, &rA, &rQ);
            }

            break;
            
        /// Fixed-Point Comparison
        case 0405:  ///< cmg
            /// | C(A) | :: | C(Y) |
            /// Zero:     If | C(A) | = | C(Y) | , then ON; otherwise OFF
            /// Negative: If | C(A) | < | C(Y) | , then ON; otherwise OFF
            {
                word36 a = rA & SIGN ? -rA : rA;
                word36 y = CY & SIGN ? -CY : CY;
                
                SCF(a == y, rIR, I_ZERO);
                SCF(a < y,  rIR, I_NEG);
            }
            break;
            
        case 0211:  ///< cmk
            /// For i = 0, 1, ..., 35
            /// \brief C(Z)i = ~C(Q)i & ( C(A)i ⊕ C(Y)i )
            
            /**
             The cmk instruction compares the contents of bit positions of A and Y for identity that are not masked by a 1 in the corresponding bit position of Q.
             The zero indicator is set ON if the comparison is successful for all bit positions; i.e., if for all i = 0, 1, ..., 35 there is either: C(A)i = C(Y)i (the identical case) or C(Q)i = 1 (the masked case); otherwise, the zero indicator is set OFF.
             The negative indicator is set ON if the comparison is unsuccessful for bit position 0; i.e., if C(A)0 ⊕ C(Y)0 (they are nonidentical) as well as C(Q)0 = 0 (they are unmasked); otherwise, the negative indicator is set OFF.
             */
            {
                word36 Z = ~rQ & (rA ^ CY);
                
                SCF(Z == 0, rIR, I_ZERO);
                SCF(Z & SIGN, rIR, I_NEG);
            }
            break;
            
        case 0115:  ///< cmpa
            /// C(A) :: C(Y)
        
            cmp36(rA, CY, &rIR);
            break;
            
        case 0116:  ///< cmpq
            /// C(Q) :: C(Y)
            cmp36(rQ, CY, &rIR);
            break;
            
        case 0100:  ///< cmpx0
        case 0101:  ///< cmpx1
        case 0102:  ///< cmpx2
        case 0103:  ///< cmpx3
        case 0104:  ///< cmpx4
        case 0105:  ///< cmpx5
        case 0106:  ///< cmpx6
        case 0107:  ///< cmpx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief  C(Xn) :: C(Y)0,17
            n = opcode & 07;  // get n
            cmp18(rX[n], GETHI(CY), &rIR);
            break;
            
        case 0111:  ///< cwl
            /// C(Y) :: closed interval [C(A);C(Q)]
            /**
             The cwl instruction tests the value of C(Y) to determine if it is within the range of values set by C(A) and C(Q). The comparison of C(Y) with C(Q) locates C(Y) with respect to the interval if C(Y) is not contained within the
             interval.
             */
            cmp36wl(rA, CY, rQ, &rIR);
            break;
            
        case 0117:  ///< cmpaq
            /// C(AQ) :: C(Y-pair)
            
            Read72(i, TPR.CA, &CYpair, OperandRead, rTAG);
            
            trAQ = convertToWord72(rA, rQ);
            
            cmp72(trAQ, CYpair, &rIR);
            break;
            
        /// Fixed-Point Miscellaneous
        case 0234:  ///< szn
            /// Set indicators according to C(Y)
            if (CY == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (CY & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
            
        case 0214:  ///< sznc
            /// Set indicators according to C(Y)
            if (CY == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (CY & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);

            // ... and clear
            CY = 0;
            break;

        /// BOOLEAN OPERATION INSTRUCTIONS
            
        /// ￼Boolean And
        case 0375:  ///< ana
            /// C(A)i & C(Y)i → C(A)i for i = (0, 1, ..., 35)
            rA = rA & CY;
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
            
        case 0377:  ///< anaq
            /// C(AQ)i & C(Y-pair)i → C(AQ)i for i = (0, 1, ..., 71)
            
            Read72(i, TPR.CA, &CYpair, OperandRead, rTAG);
            
            trAQ = convertToWord72(rA, rQ);
            trAQ = trAQ & CYpair;
            
            if (trAQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trAQ & SIGN72)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);

            convertToWord36(trAQ, &rA, &rQ);
            break;
            
        case 0376:  ///< anq
            /// C(Q)i & C(Y)i → C(Q)i for i = (0, 1, ..., 35)
            rQ = rQ & CY;
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0355:  ///< ansa
            /// C(A)i & C(Y)i → C(Y)i for i = (0, 1, ..., 35)
            tmp36 = rA & CY;
            
            if (tmp36 == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (tmp36 & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            CY = tmp36;
            break;
        
        case 0356:  ///< ansq
            /// C(Q)i & C(Y)i → C(Y)i for i = (0, 1, ..., 35)
            tmp36 = rQ & CY;
            
            if (tmp36 == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (tmp36 & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            CY = tmp36;

            break;

        case 0340:  ///< asnx0
        case 0341:  ///< asnx1
        case 0342:  ///< asnx2
        case 0343:  ///< asnx3
        case 0344:  ///< asnx4
        case 0345:  ///< asnx5
        case 0346:  ///< asnx6
        case 0347:  ///< asnx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Xn)i & C(Y)i → C(Y)i for i = (0, 1, ..., 17)
            n = opcode & 07;  // get n
            tmp18 = rX[n] & GETHI(CY);
            
            if (tmp18 == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (tmp18 & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            SETHI(CY, tmp18);
            
            break;

        case 0360:  ///< anx0
        case 0361:  ///< anx1
        case 0362:  ///< anx2
        case 0363:  ///< anx3
        case 0364:  ///< anx4
        case 0365:  ///< anx5
        case 0366:  ///< anx6
        case 0367:  ///< anx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Xn)i & C(Y)i → C(Xn)i for i = (0, 1, ..., 17)
            n = opcode & 07;  // get n
            rX[n] &= GETHI(CY);
            
            if (rX[n] == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rX[n] & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            
            break;

        /// Boolean Or
        case 0275:  ///< ora
            /// C(A)i | C(Y)i → C(A)i for i = (0, 1, ..., 35)
            rA = rA | CY;
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
         
        case 0277:  ///< oraq
            /// C(AQ)i | C(Y-pair)i → C(AQ)i for i = (0, 1, ..., 71)
            Read72(i, TPR.CA, &CYpair, OperandRead, rTAG);
            
            trAQ = convertToWord72(rA, rQ);
            trAQ = trAQ | CYpair;
            
            if (trAQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trAQ & SIGN72)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            convertToWord36(trAQ, &rA, &rQ);
            break;

        case 0276:  ///< orq
            /// C(Q)i | C(Y)i → C(Q)i for i = (0, 1, ..., 35)
            rQ = rQ | CY;
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0255:  ///< orsa
            /// C(A)i | C(Y)i → C(Y)i for i = (0, 1, ..., 35)
            CY = rA | CY;
            
            if (CY == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (CY & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0256:  ///< orsq
            /// C(Q)i | C(Y)i → C(Y)i for i = (0, 1, ..., 35)
            
            CY = rQ & CY;
            
            if (CY == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (CY & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0240:  ///< orsx0
        case 0241:  ///< orsx1
        case 0242:  ///< orsx2
        case 0243:  ///< orsx3
        case 0244:  ///< orsx4
        case 0245:  ///< orsx5
        case 0246:  ///< orsx6
        case 0247:  ///< orsx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Xn)i | C(Y)i → C(Y)i for i = (0, 1, ..., 17)
            n = opcode & 07;  // get n
            
            tmp18 = rX[n] | GETHI(CY);
            
            if (tmp18 == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (tmp18 & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            SETHI(CY, tmp18);
            
            break;

        case 0260:  ///< orx0
        case 0261:  ///< orx1
        case 0262:  ///< orx2
        case 0263:  ///< orx3
        case 0264:  ///< orx4
        case 0265:  ///< orx5
        case 0266:  ///< orx6
        case 0267:  ///< orx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Xn)i | C(Y)i → C(Xn)i for i = (0, 1, ..., 17)
            
            n = opcode & 07;  // get n
            rX[n] |= GETHI(CY);
            
            if (rX[n] == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rX[n] & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
           
        /// Boolean Exclusive Or
        case 0675:  ///< era
            /// C(A)i ⊕ C(Y)i → C(A)i for i = (0, 1, ..., 35)
            rA = rA ^ CY;
            
            if (rA == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0677:  ///< eraq
            /// C(AQ)i ⊕ C(Y-pair)i → C(AQ)i for i = (0, 1, ..., 71)
            Read72(i, TPR.CA, &CYpair, OperandRead, rTAG);
            
            trAQ = convertToWord72(rA, rQ);
            trAQ = trAQ ^ CYpair;
            
            if (trAQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trAQ & SIGN72)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            convertToWord36(trAQ, &rA, &rQ);
            break;

        case 0676:  ///< erq
            /// C(Q)i ⊕ C(Y)i → C(Q)i for i = (0, 1, ..., 35)
            rQ = rQ ^ CY;
            if (rQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rQ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0655:  ///< ersa
            /// C(A)i ⊕ C(Y)i → C(Y)i for i = (0, 1, ..., 35)
            
            CY = rA ^ CY;
            
            if (CY == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (CY & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            Write(i, TPR.CA, CY, DataWrite, rTAG);

            break;

        case 0656:  ///< ersq
            /// C(Q)i ⊕ C(Y)i → C(Y)i for i = (0, 1, ..., 35)
            
            CY = rQ ^ CY;
            
            if (CY == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (CY & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            Write(i, TPR.CA, CY, DataWrite, rTAG);

            break;

        case 0640:   ///< ersx0
        case 0641:   ///< ersx1
        case 0642:   ///< ersx2
        case 0643:   ///< ersx3
        case 0644:   ///< ersx4
        case 0645:   ///< ersx5
        case 0646:   ///< ersx6
        case 0647:   ///< ersx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Xn)i ⊕ C(Y)i → C(Y)i for i = (0, 1, ..., 17)
            
            n = opcode & 07;  // get n
            
            tmp18 = rX[n] ^ GETHI(CY);
            
            if (tmp18 == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (tmp18 & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            SETHI(CY, tmp18);
            
            Write(i, TPR.CA, CY, DataWrite, rTAG);

            break;

        case 0660:  ///< erx0
        case 0661:  ///< erx1
        case 0662:  ///< erx2
        case 0663:  ///< erx3
        case 0664:  ///< erx4
        case 0665:  ///< erx5
        case 0666:  ///< erx6 !!!! Beware !!!!
        case 0667:  ///< erx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Xn)i ⊕ C(Y)i → C(Xn)i for i = (0, 1, ..., 17)
        
            n = opcode & 07;  // get n
            rX[n] ^= GETHI(CY);
            
            if (rX[n] == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rX[n] & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        /// Boolean Comparative And
            
        case 0315:  ///< cana
            /// C(Z)i = C(A)i & C(Y)i for i = (0, 1, ..., 35)
            trZ = rA & CY;
            
            if (trZ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trZ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0317:  ///< canaq
            /// C(Z)i = C(AQ)i & C(Y-pair)i for i = (0, 1, ..., 71)
            
            Read72(i, TPR.CA, &CYpair, OperandRead, rTAG);
            
            trAQ = convertToWord72(rA, rQ);
            trAQ = trAQ & CYpair;
            
            if (trAQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trAQ & SIGN72)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0316:  ///< canq
            /// C(Z)i = C(Q)i & C(Y)i for i = (0, 1, ..., 35)
            trZ = rQ & CY;
            if (trZ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trZ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
            
        case 0300:  ///< canx0
        case 0301:  ///< canx1
        case 0302:  ///< canx2
        case 0303:  ///< canx3
        case 0304:  ///< canx4
        case 0305:  ///< canx5
        case 0306:  ///< canx6
        case 0307:  ///< canx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            /// \brief C(Z)i = C(Xn)i & C(Y)i for i = (0, 1, ..., 17)
            
            n = opcode & 07;  // get n
            tmp18 = rX[n] & GETHI(CY);
            
            if (tmp18 == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (tmp18 & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
            
        /// Boolean Comparative Not
        case 0215:  ///< cnaa
            /// C(Z)i = C(A)i & ~C(Y)i for i = (0, 1, ..., 35)
            trZ = rA & ~CY;
            
            if (trZ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (rA & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0217:  ///< cnaaq
            /// C(Z)i = C (AQ)i & ~C(Y-pair)i for i = (0, 1, ..., 71)
            Read72(i, TPR.CA, &CYpair, OperandRead, rTAG);
            
            trAQ = convertToWord72(rA, rQ);
            trAQ = trAQ & ~CYpair;
            
            if (trAQ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trAQ & SIGN72)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0216:  ///< cnaq
            /// C(Z)i = C(Q)i & ~C(Y)i for i = (0, 1, ..., 35)
            trZ = rQ & ~CY;
            if (trZ == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (trZ & SIGN)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;

        case 0200:  ///< cnax0
        case 0201:  ///< cnax1
        case 0202:  ///< cnax2
        case 0203:  ///< cnax3
        case 0204:  ///< cnax4
        case 0205:  ///< cnax5
        case 0206:  ///< cnax6
        case 0207:  ///< cnax7
            /// C(Z)i = C(Xn)i & ~C(Y)i for i = (0, 1, ..., 17)
            
            n = opcode & 07;  // get n
            tmp18 = rX[n] & ~GETHI(CY);
            
            if (tmp18 == 0)
                SETF(rIR, I_ZERO);
            else
                CLRF(rIR, I_ZERO);
            
            if (tmp18 & SIGN18)
                SETF(rIR, I_NEG);
            else
                CLRF(rIR, I_NEG);
            
            break;
            
        /// FLOATING-POINT ARITHMETIC INSTRUCTIONS
            
        case 0433:  ///< dfld
            /// C(Y-pair)0,7 → C(E)
            /// C(Y-pair)8,71 → C(AQ)0,63
            /// 00...0 → C(AQ)64,71
            /// Zero: If C(AQ) = 0, then ON; otherwise OFF
            /// Neg: If C(AQ)0 = 1, then ON; otherwise OFF
            
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);
            
            rE = (Ypair[0] >> 28) & 0377;
            
            rA = (Ypair[0] & 01777777777LL) << 8;
            rA |= (Ypair[1] >> 28) & 0377;
            
            rQ = (Ypair[1] & 01777777777LL) << 8;
            
            SCF(rA == 0 && rQ == 0, rIR, I_ZERO);
            SCF(rA & SIGN36, rIR, I_NEG);
            break;
            
        case 0431:  ///< fld
            /// C(Y)0,7 → C(E)
            /// C(Y)8,35 → C(AQ)0,27
            /// 00...0 → C(AQ)30,71
            /// Zero: If C(AQ) = 0, then ON; otherwise OFF
            /// Neg: If C(AQ)0 = 1, then ON; otherwise OFF
            
            rE = (CY >> 28) & 0377;
            rA = (CY & 01777777777LL) << 8;
            rQ = 0;
            
            SCF(rA == 0 && rQ == 0, rIR, I_ZERO);
            SCF(rA & SIGN36, rIR, I_NEG);
            break;

        case 0457:  ///< dfst
            /// C(E) → C(Y-pair)0,7
            /// C(AQ)0,63 → C(Y-pair)8,71
            
            Ypair[0] = ((word36)rE << 28) | ((rA & 0777777777400LL) >> 8);
            Ypair[1] = ((rA & 0377) << 28) | ((rQ & 0777777777400LL) >> 8);
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            break;
            
        case 0472:  ///< dfstr
            
            dfstr(i, Ypair);
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            break;
            
            //return STOP_UNIMP;
            
        case 0455:  ///< fst
            /// C(E) → C(Y)0,7
            /// C(A)0,27 → C(Y)8,35
            CY = ((word36)rE << 28) | (((rA >> 8) & 01777777777LL));
            break;
            
        case 0470:  ///< fstr
            /// The fstr instruction performs a true round and normalization on C(EAQ) as it is stored.
            
//            frd();
//            
//            /// C(E) → C(Y)0,7
//            /// C(A)0,27 → C(Y)8,35
//            CY = ((word36)rE << 28) | (((rA >> 8) & 01777777777LL));
//
//            /// Zero: If C(Y) = floating point 0, then ON; otherwise OFF
//            //SCF((CY & 01777777777LL) == 0, rIR, I_ZERO);
//            bool isZero = rE == -128 && rA == 0;
//            SCF(isZero, rIR, I_ZERO);
//            
//            /// Neg: If C(Y)8 = 1, then ON; otherwise OFF
//            //SCF(CY & 01000000000LL, rIR, I_NEG);
//            SCF(rA & SIGN36, rIR, I_NEG);
//            
//            /// Exp Ovr: If exponent is greater than +127, then ON
//            /// Exp Undr: If exponent is less than -128, then ON
//            /// XXX: not certain how these can occur here ....
            
            fstr(i, &CY);
            
            break;
            
        case 0477:  ///< dfad
            /// The dfad instruction may be thought of as a dufa instruction followed by a fno instruction.
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dufa(i);
            fno(i);
            break;
            
        case 0437:  ///< dufa
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dufa(i);
            break;
            
        case 0475:  ///< fad
            /// The fad instruction may be thought of a an ufa instruction followed by a fno instruction.
            /// (Heh, heh. We'll see....)
            
            ufa(i);
            fno(i);
            
            break;
  
        case 0435:  ///< ufa
            /// C(EAQ) + C(Y) → C(EAQ)
            
            ufa(i);
            break;
            
        case 0577:  ///< dfsb
            // The dfsb instruction is identical to the dfad instruction with the exception that
            // the twos complement of the mantissa of the operand from main memory is used.
            //return STOP_UNIMP;
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dufs(i);
            fno(i);
            break;

        case 0537:  ///< dufs
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dufs(i);
            break;
            
        case 0575:  ///< fsb
            ///< The fsb instruction may be thought of as an ufs instruction followed by a fno instruction.
            ufs(i);
            fno(i);
            
            break;
            
        case 0535:  ///< ufs
            ///< C(EAQ) - C(Y) → C(EAQ)
            
            ufs(i);
            break;
            
        case 0463:  ///< dfmp
            /// The dfmp instruction may be thought of as a dufm instruction followed by a
            /// fno instruction.
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dufm(i);
            fno(i);
            
            break;
            
        case 0423:  ///< dufm
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dufm(i);
            break;
            
        case 0461:  ///< fmp
            /// The fmp instruction may be thought of as a ufm instruction followed by a
            /// fno instruction.
 
            ufm(i);
            fno(i);
            
            break;
            
        case 0421:  ///< ufm
            /// C(EAQ) × C(Y) → C(EAQ)
            ufm(i);
            break;
            
        case 0527:  ///< dfdi
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dfdi(i);
            break;
            
        case 0567:  ///< dfdv
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dfdv(i);
            break;
            
        case 0525:  ///< fdi
            /// C(Y) / C(EAQ) → C(EA)
            
            fdi(i);
            break;
            
        case 0565:  ///< fdv
            /// C(EAQ) /C(Y) → C(EA)
            /// 00...0 → C(Q)
            fdv(i);
            break;
            
        case 0513:  ///< fneg
            /// -C(EAQ) normalized → C(EAQ)
            fneg(i);
            break;
            
        case 0573:  ///< fno
            /// The fno instruction normalizes the number in C(EAQ) if C(AQ) ≠ 0 and the overflow indicator is OFF.
            // A normalized floating number is defined as one whose mantissa lies in the interval [0.5,1.0) such that 0.5<= |C(AQ)| <1.0 which, in turn, requires that C(AQ)0 ≠ C(AQ)1.
            ///Charles Is the coolest
            ///true story y'all
            //you should get me darksisers 2 for christmas
            fno(i);
            break;
            
        case 0473:  ///< dfrd
            /// C(EAQ) rounded to 64 bits → C(EAQ)
            /// 0 → C(AQ)64,71 (See notes in dps8_math.c on dfrd())

            dfrd(i);
            break;
            
        case 0471:  ///< frd
            /// C(EAQ) rounded to 28 bits → C(EAQ)
            /// 0 → C(AQ)28,71 (See notes in dps8_math.c on frd())
            
            frd(i);
            break;
            
        case 0427:  ///< dfcmg
            /// C(E) :: C(Y-pair)0,7
            /// | C(AQ)0,63 | :: | C(Y-pair)8,71 |
            
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dfcmg(i);
            break;
            
        case 0517:  ///< dfcmp
            /// C(E) :: C(Y-pair)0,7
            /// C(AQ)0,63 :: C(Y-pair)8,71
            
            ReadYPair(i, TPR.CA, Ypair, OperandRead, rTAG);

            dfcmp(i);
            break;

        case 0425:  ///< fcmg
            /// C(E) :: C(Y)0,7
            /// | C(AQ)0,27 | :: | C(Y)8,35 |
            
            fcmg(i);
            break;
            
        case 0515:  ///< fcmp
            /// C(E) :: C(Y)0,7
            /// C(AQ)0,27 :: C(Y)8,35
            
            fcmp(i);
            break;
            
        case 0415:  ///< ade
            /// C(E) + C(Y)0,7 → C(E)
            {
                int8 e = (CY >> 28) & 0377;
                SCF((rE + e) >  127, rIR, I_EOFL);
                SCF((rE + e) < -128, rIR, I_EUFL);
                
                CLRF(rIR, I_ZERO);
                CLRF(rIR, I_NEG);
                
                rE += e;    // add
                rE &= 0377; // keep to 8-bits
            }
            break;
            
        case 0430:  ///< fszn

            /// Zero: If C(Y)8,35 = 0, then ON; otherwise OFF
            /// Negative: If C(Y)8 = 1, then ON; otherwise OFF
            
            SCF((CY & 001777777777LL) == 0, rIR, I_ZERO);
            SCF(CY & 001000000000LL, rIR, I_NEG);
            
            break;
            
        case 0411:  ///< lde
            /// C(Y)0,7 → C(E)
            
            rE = (CY >> 28) & 0377;
            CLRF(rIR, I_ZERO | I_NEG);
    
            break;
            
        case 0456:  ///< ste
            /// C(E) → C(Y)0,7
            /// 00...0 → C(Y)8,17
            
            //CY = (rE << 28);
            CY = bitfieldInsert36(CY, ((word36)(rE & 0377) << 10), 18, 8);
            break;
            
            
        /// TRANSFER INSTRUCTIONS
        case 0713:  ///< call6
            /// XXX not fully implemented
            
           // if (TPR.TRR > PPR.PRR)
           //     return STOP_FAULT; // access violation fault (outward call)
            if (TPR.TRR < PPR.PRR)
                PR[7].SNR = ((DSBR.STACK << 3) | TPR.TRR) & 077777; // keep to 15-bits
            if (TPR.TRR == PPR.PRR)
                PR[7].SNR = PR[6].SNR;
            PR[7].RNR = TPR.TRR;
            if (TPR.TRR == 0)
                PPR.P = SDW->P;
            else
                PPR.P = 0;
            PR[7].WORDNO = 0;
            PR[7].BITNO = 0;
            PPR.PRR = TPR.TRR;
            PPR.PSR = TPR.TSR;
            PPR.IC = TPR.CA;
            
            return CONT_TRA;
            
            
        case 0630:  ///< ret
            /// C(Y)0,17 → C(PPR.IC)
            /// C(Y)18,31 → C(IR)
            /// XXX Not completely implemented
            PPR.IC = GETHI(CY);
            rIR = GETLO(CY) & 0777760;
            
            return CONT_TRA;
            
        case 0610:  ///< rtcd
            /// C(Y-pair)3,17 → C(PPR.PSR)
            /// Maximum of C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) → C(PPR.PRR)
            /// C(Y-pair)36,53 → C(PPR.IC)
            /// If C(PPR.PRR) = 0 then C(SDW.P) → C(PPR.P);
            /// otherwise 0 → C(PPR.P)
            /// C(PPR.PRR) → C(PRn.RNR) for n = (0, 1, ..., 7)
            
            processorCycle = RTCD_OPERAND_FETCH;

            //Read2(i, TPR.CA, &Ypair[0], &Ypair[1], OperandRead, rTAG);
            
            /// C(Y-pair)3,17 → C(PPR.PSR)
            PPR.PSR = GETHI(Ypair[0]) & 077777LL;
            
            /// Maximum of C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) → C(PPR.PRR)
            PPR.PRR = max3(((GETLO(Ypair[0]) >> 15) & 7), TPR.TRR, SDW->R1);
            
            /// C(Y-pair)36,53 → C(PPR.IC)
            PPR.IC = GETHI(Ypair[1]);
            
            /// If C(PPR.PRR) = 0 then C(SDW.P) → C(PPR.P);
            /// otherwise 0 → C(PPR.P)
            if (PPR.PRR == 0)
                PPR.P = SDW->P;
            else
                PPR.P = 0;
            
            /// C(PPR.PRR) → C(PRn.RNR) for n = (0, 1, ..., 7)
            //for(int n = 0 ; n < 8 ; n += 1)
            //  PR[n].RNR = PPR.PRR;

            PR[0].RNR =
            PR[1].RNR =
            PR[2].RNR =
            PR[3].RNR =
            PR[4].RNR =
            PR[5].RNR =
            PR[6].RNR =
            PR[7].RNR = PPR.PRR;
            
            return CONT_TRA;
            
            
        case 0614:  ///< teo
            /// If exponent overflow indicator ON then
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            /// XXX Not completely implemented
            if (rIR & I_EOFL)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                CLRF(rIR, I_EOFL);
                
                return CONT_TRA;
            }
            break;
            
        case 0615:  ///< teu
            /// If exponent underflow indicator ON then
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            /// XXX Not completely implemented
            if (rIR & I_EUFL)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                CLRF(rIR, I_EUFL);
                
                return CONT_TRA;
            }
            break;

        
        case 0604:  ///< tmi
            /// If negative indicator ON then
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            if (rIR & I_NEG)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;
            
        case 0602:  ///< tnc
            /// If carry indicator OFF then
            ///   C(TPR.CA) → C(PPR.IC)
            ///   C(TPR.TSR) → C(PPR.PSR)
            if (!(rIR & I_CARRY))
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;
            
        case 0601:  ///< tnz
            /// If zero indicator OFF then
            ///     C(TPR.CA) → C(PPR.IC)
            ///     C(TPR.TSR) → C(PPR.PSR)
            if (!(rIR & I_ZERO))
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;

        case 0617:  ///< tov
            /// If overflow indicator ON then
            ///   C(TPR.CA) → C(PPR.IC)
            ///   C(TPR.TSR) → C(PPR.PSR)
            if (rIR & I_OFLOW)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                CLRF(rIR, I_OFLOW);
                
                return CONT_TRA;
            }
            break;

        case 0605:  ///< tpl
            /// If negative indicator OFF, then
            ///   C(TPR.CA) → C(PPR.IC)
            ///   C(TPR.TSR) → C(PPR.PSR)
            if (!(rIR & I_NEG)) 
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;
        
        case 0710:  ///< tra
            /// C(TPR.CA) → C(PPR.IC)
            /// C(TPR.TSR) → C(PPR.PSR)
            
            PPR.IC = TPR.CA;
            PPR.PSR = TPR.TSR;
            
            return CONT_TRA;

        case 0603:  ///< trc
            ///  If carry indicator ON then
            ///    C(TPR.CA) → C(PPR.IC)
            ///    C(TPR.TSR) → C(PPR.PSR)
            if (rIR & I_CARRY)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;

        case 0270:  ///< tsp0
        case 0271:  ///< tsp1
        case 0272:  ///< tsp2
        case 0273:  ///< tsp3
        case 0670:  ///< tsp4
        case 0671:  ///< tsp5
        case 0672:  ///< tsp6
        case 0673:  ///< tsp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PPR.PRR) → C(PRn.RNR)
            ///  C(PPR.PSR) → C(PRn.SNR)
            ///  C(PPR.IC) + 1 → C(PRn.WORDNO)
            ///  00...0 → C(PRn.BITNO)
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            
            if (opcode <= 0273)
                n = (opcode & 3);
            else
                n = (opcode & 3) + 4;

            PR[n].RNR = PPR.PRR;
            PR[n].SNR = PPR.PSR;
            PR[n].WORDNO = (PPR.IC + 1) & 0777777;
            PR[n].BITNO = 0;
            PPR.IC = TPR.CA;
            PPR.PSR = TPR.TSR;
            
            return CONT_TRA;
        
        case 0715:  ///< tss
            /// C(TPR.CA) + (BAR base) → C(PPR.IC)
            /// C(TPR.TSR) → C(PPR.PSR)
            /// XXX partially implemented
            PPR.IC = TPR.CA + (BAR.BASE << 9);
            PPR.PSR = TPR.TSR;
            
            
            return CONT_TRA;
            
        case 0700:  ///< tsx0
        case 0701:  ///< tsx1
        case 0702:  ///< tsx2
        case 0703:  ///< tsx3
        case 0704:  ///< tsx4
        case 0705:  ///< tsx5
        case 0706:  ///< tsx6
        case 0707:  ///< tsx7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(PPR.IC) + 1 → C(Xn)
            /// C(TPR.CA) → C(PPR.IC)
            /// C(TPR.TSR) → C(PPR.PSR)
            n = opcode & 07;  // get n
            rX[n] = (PPR.IC + 1) & 0777777;
            PPR.IC = TPR.CA;
            PPR.PSR = TPR.TSR;
            
            return CONT_TRA;
        
        case 0607:  ///< ttf
            /// If tally runout indicator OFF then
            ///   C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            if ((rIR & I_TALLY) == 0)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;
         
        case 0600:  ///< tze
            /// If zero indicator ON then
            ///   C(TPR.CA) → C(PPR.IC)
            ///   C(TPR.TSR) → C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            if (rIR & I_ZERO)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;

        case 0311:  ///< easp0
            /// C(TPR.CA) → C(PRn.SNR)
            PR[0].SNR = TPR.CA;
            break;
        case 0313:  ///< easp2
            /// C(TPR.CA) → C(PRn.SNR)
            PR[2].SNR = TPR.CA;
            break;
        case 0331:  ///< easp4
            /// C(TPR.CA) → C(PRn.SNR)
            PR[4].SNR = TPR.CA;
            break;
        case 0333:  ///< easp6
            /// C(TPR.CA) → C(PRn.SNR)
            PR[6].SNR = TPR.CA;
            break;
        
        case 0310:  ///< eawp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[0].WORDNO = TPR.CA;
            PR[0].BITNO = TPR.TBR;
            break;
        case 0312:  ///< eawp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[2].WORDNO = TPR.CA;
            PR[2].BITNO = TPR.TBR;
            break;
        case 0330:  ///< eawp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[4].WORDNO = TPR.CA;
            PR[4].BITNO = TPR.TBR;
            break;
        case 0332:  ///< eawp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[6].WORDNO = TPR.CA;
            PR[6].BITNO = TPR.TBR;
            break;
        
            
        case 0351:  ///< epbp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[1].RNR = TPR.TRR;
            PR[1].SNR = TPR.TSR;
            PR[1].WORDNO = 0;
            PR[1].BITNO = 0;
            break;
        case 0353:  ///< epbp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[3].RNR = TPR.TRR;
            PR[3].SNR = TPR.TSR;
            PR[3].WORDNO = 0;
            PR[3].BITNO = 0;
            break;
        case 0371:  ///< epbp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[5].RNR = TPR.TRR;
            PR[5].SNR = TPR.TSR;
            PR[5].WORDNO = 0;
            PR[5].BITNO = 0;
            break;
            
        case 0373:  ///< epbp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[7].RNR = TPR.TRR;
            PR[7].SNR = TPR.TSR;
            PR[7].WORDNO = 0;
            PR[7].BITNO = 0;
            break;
        
        case 0350:  ///< epp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[0].RNR = TPR.TRR;
            PR[0].SNR = TPR.TSR;
            PR[0].WORDNO = TPR.CA;
            PR[0].BITNO = TPR.TBR;
            break;
        case 0352:  ///< epp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[2].RNR = TPR.TRR;
            PR[2].SNR = TPR.TSR;
            PR[2].WORDNO = TPR.CA;
            PR[2].BITNO = TPR.TBR;
            break;
        case 0370:  ///< epp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[4].RNR = TPR.TRR;
            PR[4].SNR = TPR.TSR;
            PR[4].WORDNO = TPR.CA;
            PR[4].BITNO = TPR.TBR;
            break;
        case 0372:  ///< epp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[6].RNR = TPR.TRR;
            PR[6].SNR = TPR.TSR;
            PR[6].WORDNO = TPR.CA;
            PR[6].BITNO = TPR.TBR;
            break;

        case 0173:  ///< lpri
            /// For n = 0, 1, ..., 7
            ///  Y-pair = Y-block16 + 2n
            ///  Maximum of C(Y-pair)18,20; C(SDW.R1); C(TPR.TRR) → C(PRn.RNR)
            ///  C(Y-pair) 3,17 → C(PRn.SNR)
            ///  C(Y-pair)36,53 → C(PRn.WORDNO)
            ///  C(Y-pair)57,62 → C(PRn.BITNO)
            
            ReadN(i, 16, TPR.CA, Yblock16, OperandRead, rTAG);  // read 16-words from memory

            for(n = 0 ; n < 8 ; n ++)
            {
                word36 Ypair[2];
                Ypair[0] = Yblock16[n * 2 + 0]; // Even word of ITS pointer pair
                Ypair[1] = Yblock16[n * 2 + 1]; // Odd word of ITS pointer pair
                
                word3 Crr = (GETLO(Ypair[0]) >> 15) & 07;       ///< RNR from ITS pair
                PR[n].RNR = max3(Crr, SDW->R1, TPR.TRR) ;
                PR[n].SNR = (Ypair[0] >> 18) & 077777;
                PR[n].WORDNO = GETHI(Ypair[1]);
                PR[n].BITNO = (GETLO(Ypair[1]) >> 9) & 077;
            }
            break;
            
        case 0760:  ///< lprp0
        case 0761:  ///< lprp1
        case 0762:  ///< lprp2
        case 0763:  ///< lprp3
        case 0764:  ///< lprp4
        case 0765:  ///< lprp5
        case 0766:  ///< lprp6
        case 0767:  ///< lprp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  If C(Y)0,1 ≠ 11, then
            ///    C(Y)0,5 → C(PRn.BITNO);
            ///  otherwise,
            ///    generate command fault
            /// If C(Y)6,17 = 11...1, then 111 → C(PRn.SNR)0,2
            ///  otherwise,
            /// 000 → C(PRn.SNR)0,2
            /// C(Y)6,17 → C(PRn.SNR)3,14
            /// C(Y)18,35 → C(PRn.WORDNO)
            
            n = opcode & 07;  // get n
            PR[n].RNR = TPR.TRR;
            if ((CY & 3) != 3)
                PR[n].BITNO = (CY >> 30) & 077;
            else
                // generate command fault
                ;
            //If C(Y)6,17 = 11...1, then 111 → C(PRn.SNR)0,2
            if ((CY & 07777000000LL) == 07777000000LL)
                PR[n].SNR = 070000; // XXX check to see if this is correct
            else // otherwise, 000 → C(PRn.SNR)0,2
                PR[n].SNR = 0;  //&= 07777;
            // XXX completed, but needs testing
            //C(Y)6,17 → C(PRn.SNR)3,14
            //PR[n].SNR &= 3; -- huh? Never code when tired
            PR[n].SNR |= GETHI(CY) & 07777;
            //C(Y)18,35 → C(PRn.WORDNO)
            PAR[n].WORDNO = GETLO(CY);
            
            break;
         
        case 0251:  ///< spbp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[1].SNR << 18;
            Ypair[0] |= PR[1].RNR << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
            
        case 0253:  ///< spbp3
            //l For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[3].SNR << 18;
            Ypair[0] |= PR[3].RNR << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
            
        case 0651:  ///< spbp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[5].SNR << 18;
            Ypair[0] |= PR[5].RNR << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
        
        case 0653:  ///< spbp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[7].SNR << 18;
            Ypair[0] |= PR[7].RNR << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
        
        case 0254:  ///< spri
            /// For n = 0, 1, ..., 7
            ///  Y-pair = Y-block16 + 2n
            
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
        
            for(n = 0 ; n < 8 ; n++)
            {
                Yblock16[2 * n] = 043;
                Yblock16[2 * n] |= PR[n].SNR << 18;
                Yblock16[2 * n] |= PR[n].RNR << 15;
                
                Yblock16[2 * n + 1] = PR[n].WORDNO << 18;
                Yblock16[2 * n + 1] |= PR[n].BITNO << 9;
            }
            
            WriteN(i, 16, TPR.CA, Yblock16, OperandWrite, rTAG);
            break;
            
        case 0250:  ///< spri0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[0].SNR << 18;
            Ypair[0] |= PR[0].RNR << 15;
            
            Ypair[1] = PR[0].WORDNO << 18;
            Ypair[1]|= PR[0].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
            
        case 0252:  ///< spri2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[2].SNR << 18;
            Ypair[0] |= PR[2].RNR << 15;
            
            Ypair[1] = PR[2].WORDNO << 18;
            Ypair[1]|= PR[2].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
  
        case 0650:  ///< spri4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[4].SNR << 18;
            Ypair[0] |= PR[4].RNR << 15;
            
            Ypair[1] = PR[4].WORDNO << 18;
            Ypair[1]|= PR[4].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
    
        case 0652:  ///< spri6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[6].SNR << 18;
            Ypair[0] |= PR[6].RNR << 15;
            
            Ypair[1] = PAR[6].WORDNO << 18;
            Ypair[1]|= PR[6].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
    
        case 0540:  ///< sprp0
        case 0541:  ///< sprp1
        case 0542:  ///< sprp2
        case 0543:  ///< sprp3
        case 0544:  ///< sprp4
        case 0545:  ///< sprp5
        case 0546:  ///< sprp6
        case 0547:  ///< sprp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.BITNO) → C(Y)0,5
            ///  C(PRn.SNR)3,14 → C(Y)6,17
            //  C(PRn.WORDNO) → C(Y)18,35
            
            n = opcode & 07;  // get n
            
            //If C(PRn.SNR)0,2 are nonzero, and C(PRn.SNR) ≠ 11...1, then a store fault (illegal pointer) will occur and C(Y) will not be changed.
            
            if ((PR[n].SNR & 070000) != 0 && PR[n].SNR != 077777)
                // store fault (illegal pointer)
                ;
            
            CY  =  PR[n].BITNO << 30;
            CY |= (PR[n].SNR & 07777) << 18; // lower 12- of 15-bits
            CY |=  PR[n].WORDNO;
            
            CY &= DMASK;    // keep to 36-bits
            
            break;
            
        case 050:   ///< adwp0
        case 051:   ///< adwp1
        case 052:   ///< adwp2
        case 053:   ///< adwp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(Y)0,17 + C(PRn.WORDNO) → C(PRn.WORDNO)
            ///   00...0 → C(PRn.BITNO)
            
            n = opcode & 03;  // get n
            PR[n].WORDNO += GETHI(CY);
            PR[n].WORDNO &= 0777777;
            PR[n].BITNO = 0;
            break;
            
        case 0150:   ///< adwp4
        case 0151:   ///< adwp5
        case 0152:   ///< adwp6
        case 0153:   ///< adwp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(Y)0,17 + C(PRn.WORDNO) → C(PRn.WORDNO)
            ///   00...0 → C(PRn.BITNO)
            
            n = (opcode & 03) + 4;  // get n
            PR[n].WORDNO += GETHI(CY);
            PR[n].WORDNO &= 0777777;
            PR[n].BITNO = 0;
            break;
        
        case 0213:  ///< epaq
            /// 000 → C(AQ)0,2
            /// C(TPR.TSR) → C(AQ)3,17
            /// 00...0 → C(AQ)18,32
            /// C(TPR.TRR) → C(AQ)33,35
            
            /// C(TPR.CA) → C(AQ)36,53
            /// 00...0 → C(AQ)54,65
            /// C(TPR.TBR) → C(AQ)66,71
            
            rA = TPR.TRR & 7;
            rA |= TPR.TSR << 18;
            
            rQ = TPR.TBR & 077;
            rQ |= TPR.CA << 18;
            
            break;
        
        case 0633:  ///< rccl
            /// 00...0 → C(AQ)0,19
            /// C(calendar clock) → C(AQ)20,71
            
            c_0633:;    // for rscr clock/cal call.
            
            {
                /// The calendar clock consists of a 52-bit register which counts microseconds and is readable as a double-precision integer by a single instruction from any central processor. This rate is in the same order of magnitude as the instruction processing rate of the GE-645, so that timing of 10-instruction subroutines is meaningful. The register is wide enough that overflow requires several tens of years; thus it serves as a calendar containing the number of microseconds since 0000 GMT, January 1, 1901
                ///  Secs from Jan 1, 1901 to Jan 1, 1970 - 2 177 452 800          Seconds
                /// uSecs from Jan 1, 1901 to Jan 1, 1970 - 2 177 452 800 000 000 uSeconds
 
                struct timeval now;                
                gettimeofday(&now, NULL);
                
                t_uint64 UnixSecs = now.tv_sec;                            // get uSecs since Jan 1, 1970
                t_uint64 UnixuSecs = UnixSecs * 1000000LL + now.tv_usec;
                
                // now determine uSecs since Jan 1, 1901 ...
                t_uint64 MulticsuSecs = 2177452800000000LL + UnixuSecs;
                
                static t_uint64 lastRccl;                    //  value from last call
                
                if (MulticsuSecs == lastRccl)
                    lastRccl = MulticsuSecs + 1;
                else
                    lastRccl = MulticsuSecs;

                rQ =  lastRccl & 0777777777777;     // lower 36-bits of clock
                rA = (lastRccl >> 36) & 0177777;    // upper 16-bits of clock
            }
            break;
        
        case 002:   ///< drl
            /// Causes a fault which fetches and executes, in absolute mode, the instruction pair at main memory location C+(14)8. The value of C is obtained from the FAULT VECTOR switches on the processor configuration panel.

            /// XXX not completed
            /// generate fault...
            
            break;
         
        case 0716:  ///< xec
            {
            /// XXX partially implemented
            
            /// The xec instruction itself does not affect any indicator. However, the execution of the instruction from C(Y) may affect indicators.
            /// If the execution of the instruction from C(Y) modifies C(PPR.IC), then a transfer of control occurs; otherwise, the next instruction to be executed is fetched from C(PPR.IC)+1.
            /// To execute a rpd instruction, the xec instruction must be in an odd location. The instruction pair repeated is that instruction pair at C(PPR.IC) +1, that is, the instruction pair immediately following the xec instruction. C(PPR.IC) is adjusted during the execution of the repeated instruction pair so that the next instruction fetched for execution is from the first word following the repeated instruction pair.
            ///    EIS multiword instructions may be executed with the xec instruction but the required operand descriptors must be located immediately after the xec instruction, that is, starting at C(PPR.IC)+1. C(PPR.IC) is adjusted during execution of the EIS multiword instruction so that the next instruction fetched for execution is from the first word following the EIS operand descriptors.
            ///    Attempted repetition with the rpt, rpd, or rpl instructions causes an illegal procedure fault.
                
                DCDstruct _xec;   // our decoded instruction struct
                EISstruct _eis;
                
                _xec.IWB = CY;
                _xec.e = &_eis;
                
                DCDstruct *xec = decodeInstruction(CY, &_xec);    // fetch instruction into current instruction
                
                t_stat ret = executeInstruction(xec);
                
                if (ret)
                    return (ret);
            }
            break;
            
        case 0717:  ///< xed
            {
            /// The xed instruction itself does not affect any indicator. However, the execution of the instruction pair from C(Y-pair) may affect indicators.
            /// The even instruction from C(Y-pair) must not alter C(Y-pair)36,71, and must not be another xed instruction.
            /// If the execution of the instruction pair from C(Y-pair) alters C(PPR.IC), then a transfer of control occurs; otherwise, the next instruction to be executed is fetched from C(PPR.IC)+1. If the even instruction from C(Y-pair) alters C(PPR.IC), then the transfer of control is effective immediately and the odd instruction is not executed.
    
            /// To execute an instruction pair having an rpd instruction as the odd instruction, the xed instruction must be located at an odd address. The instruction pair repeated is that instruction pair at C PPR.IC)+1, that is, the instruction pair immediately following the xed instruction. C(PPR.IC) is adjusted during the execution of the repeated instruction pair so the the next instruction fetched for execution is from the first word following the repeated instruction pair.
            /// The instruction pair at C(Y-pair) may cause any of the processor defined fault conditions, but only the directed faults (0,1,2,3) and the access violation fault may be restarted successfully by the hardware. Note that the software induced fault tag (1,2,3) faults cannot be properly restarted.
            ///  An attempt to execute an EIS multiword instruction causes an illegal procedure fault.
            ///  Attempted repetition with the rpt, rpd, or rpl instructions causes an illegal procedure fault.
            
            // XXX This is probably way wrong and too simplistic, but it's a start ...
                
            DCDstruct _xec;   // our decoded instruction struct
            EISstruct _eis;
            
            _xec.IWB = Ypair[0];
            _xec.e = &_eis;
            
            DCDstruct *xec = decodeInstruction(Ypair[0], &_xec);    // fetch instruction into current instruction
            
            t_stat ret = executeInstruction(xec);
            
            if (ret)
                return (ret);

            _xec.IWB = Ypair[1];
            _xec.e = &_eis;
                
            xec = decodeInstruction(Ypair[1], &_xec);               // fetch instruction into current instruction
                
            ret = executeInstruction(xec);
                
            if (ret)
                return (ret);
    
            }
            break;
            
        case 001:   ///< mme
            /// Causes a fault that fetches and executes, in absolute mode, the instruction pair at main memory location C+4. The value of C is obtained from the FAULT VECTOR switches on the processor configuration panel.
        case 004:   ///< mme2
            /// Causes a fault that fetches and executes, in absolute mode, the instruction pair at main memory location C+(52)8. The value of C is obtained from the FAULT VECTOR switches on the processor configuration panel.
        case 005:   ///< mme3
            /// Causes a fault that fetches and executes, in absolute mode, the instruction pair at main memory location C+(54)8. The value of C is obtained from the FAULT VECTOR switches on the processor configuration panel.
        case 007:   ///< mme4
            /// Causes a fault that fetches and executes, in absolute mode, the instruction pair at main memory location C+(56)8. The value of C is obtained from the FAULT VECTOR switches on the processor configuration panel.
        case 011:   ///< nop
            break;

        case 012:   ///< puls1
            {
            printf("A=%012llo Q=%012llo IR:%s\r\n", rA, rQ, dumpFlags(rIR));
            printf("X[0]=%06o X[1]=%06o X[2]=%06o X[3]=%06o\r\n", rX[0], rX[1], rX[2], rX[3]);
            printf("X[4]=%06o X[5]=%06o X[6]=%06o X[7]=%06o\r\n", rX[4], rX[5], rX[6], rX[7]);
                for(int n = 0 ; n < 8 ; n++)
                    printf("PR[%d]: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\r\n", n, PR[n].SNR, PR[n].RNR, PR[n].WORDNO, PR[n].BITNO);

            }
            break;

        case 013:   ///< puls2
            bPuls2 = true;
            break;
         
            // TODo: implement RPD/RPL
        case 0560:  ///< rpd
        case 0500:  ///< rpl
            return STOP_UNIMP;
            
        case 0520:  ///< rpt            
            {
                //Execute the instruction at C(PPR.IC)+1 either a specified number of times
                //or until a specified termination condition is met.

                int delta = i->IWB & 077;
                
                // fetch next instruction ...
                
                DCDstruct _nxt;   // our decoded instruction struct
                DCDstruct *nxt = fetchInstruction(rIC+1, &_nxt);    // fetch next instruction into current instruction

                // XXX check for illegal modifiers only R & RI are allowed and only X1..X7
                switch (GET_TM(nxt->tag))
                {
                    case TM_R:
                    case TM_RI:
                        break;
                    default:
                        // XXX generate fault. Only R & RI allowed
                        doFault(i, FAULT_IPR, 0, "ill addr mod from RPT");
                }
                
                int Xn = nxt->tag - 8;          // Get Xn of next instruction
    
                if (nxt->iwb->flags & NO_RPT)   // repeat allowed for this instruction?
                {
                    doFault(i, FAULT_IPR, 0, "no rpt allowed for instruction");
                }
                
                //If C = 1, then C(rpt instruction word)0,17 → C(X0); otherwise, C(X0) unchanged prior to execution.
                bool c1 = TSTBIT(i->IWB, 25);
                if (c1)
                    rX[0] = GET_ADDR(i->IWB);
                
                // For the first execution of the repeated instruction: C(C(PPR.IC)+1)0,17 + C(Xn) → y, y → C(Xn)
                TPR.CA = (rX[Xn] + nxt->address) & AMASK;
                rX[Xn] = TPR.CA;

                bool exit = false;  // when true terminate rpt instruction
                do 
                {                   
                    // fetch operand into CY (if necessary)
                    
                    // XXX This is probably soooo wrong, but let's see what happens ......
                    // what about instructions that need more that 1 word?????
                    if (nxt->iwb->flags & READ_OPERAND)
                    {
                        switch (GET_TM(nxt->tag))
                        {
                            case TM_R:
                                Read(nxt, TPR.CA, &CY, OperandRead, 0);
                                break;
                            case TM_RI:
                            {
                                //In the case of RI modification, only one indirect reference is made per repeated execution. The TAG field of the indirect word is not interpreted. The indirect word is treated as though it had R modification with R = N.
                                word36 tmp;
                                Read(nxt, TPR.CA, &tmp, OperandRead, 0);     // XXX can append mode be invoked here?
                                Read(nxt, GETHI(tmp), &CY, OperandRead, 0);  // XXX ""
                                break;
                            }
                            default:
                                // XXX generate fault. Only R & RI allowed
                                doFault(i, FAULT_IPR, 0, "ill addr mod from RPT");
                        }
                    }
                    

                    // The repetition cycle consists of the following steps:
                    //  a. Execute the repeated instruction

                    t_stat ret = doInstruction(nxt);
                    if (ret)
                    {
                        cpuCycles += 1; // XXX remove later when we can get this into the main loop
                        return (ret);   
                    }
                    
                    //  b. C(X0)0,7 - 1 → C(X0)0,7
                    int x = bitfieldExtract(rX[0], 10, 8);
                    x -= 1;
                    rX[0] = bitfieldInsert(rX[0], x, 10, 8);
                    
                    //  Modify C(Xn) as described below
                    //The computed address, y, of the operand (in the case of R modification) or indirect word (in the case of RI modification) is determined as follows:
                    
                    TPR.CA = (rX[Xn] + delta) & AMASK;
                    rX[Xn] = TPR.CA;

                    //  c. If C(X0)0,7 = 0, then set the tally runout indicator ON and terminate
                    if (x == 0)
                    {
                        SETF(rIR, I_TALLY);
                        exit = true;
                    }
                    
                    //  d. If a terminate condition has been met, then set the tally runout indicator OFF and terminate
                    if (TSTF(rIR, I_ZERO) && (rX[0] & 0100))
                    {
                        CLRF(rIR, I_TALLY);
                        exit = true;
                    } else if (!TSTF(rIR, I_ZERO) && (rX[0] & 040))
                    {
                        CLRF(rIR, I_TALLY);
                        exit = true;
                    } else if (TSTF(rIR, I_NEG) && (rX[0] & 020))
                    {
                        CLRF(rIR, I_TALLY);
                        exit = true;
                    } else if (!TSTF(rIR, I_NEG) && (rX[0] & 010))
                    {
                        CLRF(rIR, I_TALLY);
                        exit = true;
                    } else if (TSTF(rIR, I_CARRY) && (rX[0] & 04))
                    {
                        CLRF(rIR, I_TALLY);
                        exit = true;
                    } else if (!TSTF(rIR, I_CARRY) && (rX[0] & 02))
                    {
                        CLRF(rIR, I_TALLY);
                        exit = true;
                    } else if (TSTF(rIR, I_OFLOW) && (rX[0] & 01))
                    {
                        CLRF(rIR, I_TALLY);
                        exit = true;
                    }
                    
                    //  e. Go to step a
                    cpuCycles += 1; // XXX remove later when we can get this into the main loop

                } while (exit == false);
                
                rIC += 1;   // bump instruction counter
            }
            break;
            
            
        case 0550:  ///< sbar
            /// C(BAR) → C(Y) 0,17
            SETHI(CY, (BAR.BASE << 9) | BAR.BOUND);
            
            break;
            
            
        /// translation
        case 0505:  ///< bcd
            /// Shift C(A) left three positions
            /// | C(A) | / C(Y) → 4-bit quotient plus remainder
            /// Shift C(Q) left six positions
            /// 4-bit quotient → C(Q)32,35
            /// remainder → C(A)
            
            /// XXX Tested. Seems to work OK
            
            tmp1 = rA & SIGN36; // A0
            
            rA <<= 3;       // Shift C(A) left three positions
            rA &= DMASK;    // keep to 36-bits
            
            tmp36 = llabs(SIGNEXT36(rA));
            tmp36q = tmp36 / CY; // | C(A) | / C(Y) → 4-bit quotient plus remainder
            tmp36r = tmp36 % CY;
            
            rQ <<= 6;       // Shift C(Q) left six positions
            rQ &= DMASK;
            
            rQ &= ~017;     // 4-bit quotient → C(Q)32,35
            rQ |= (tmp36q & 017);
            
            rA = tmp36r;    // remainder → C(A)
            
            SCF(rA == 0, rIR, I_ZERO);  // If C(A) = 0, then ON; otherwise OFF
            SCF(tmp1,    rIR, I_NEG);   // If C(A)0 = 1 before execution, then ON; otherwise OFF
            
            break;
           
        case 9774:  ///< gtb
            /// C(A)0 → C(A)0
            /// C(A)i ⊕ C(A)i-1 → C(A)i for i = 1, 2, ..., 35
            
            /// XXX untested.
            
            tmp1 = rA & SIGN36; // save A0
            
            tmp36 = rA >> 1;
            rA = rA ^ tmp36;
            
            if (tmp1)
                rA |= SIGN36;   // set A0
            else
                rA &= ~SIGN36;  // reset A0
            
            SCF(rA == 0,    rIR, I_ZERO);  // If C(A) = 0, then ON; otherwise OFF
            SCF(rA & SIGN36,rIR, I_NEG);   // If C(A)0 = 1, then ON; otherwise OFF

            break;
         
        /// REGISTER LOAD
        case 0230:  ///< lbar
            /// C(Y)0,17 → C(BAR)
            BAR.BASE = (GETHI(CY) >> 9) & 0777; /// BAR.BASE is upper 9-bits (0-8)
            BAR.BOUND = GETHI(CY) & 0777;       /// BAR.BOUND is next lower 9-bits (9-17)
            break;
           
        // Privileged Instructions

        // Privileged - Register Load
        case 0674:  ///< lcpr
        case 0232:  ///< ldbr
        case 0637:  ///< ldt
        case 0257:  ///< lsdp
        case 0613:  ///< rcu
        case 0452:  ///< scpr
            return STOP_UNIMP;

        case 0657:  ///< scu
            
            // ToDo: need to decode i into cu.IR
            cu_safe_store();
            break;
            
        case 0154:  ///< sdbr
        case 0557:  ///< sdp
        case 0532:  ///< cams
            
        // Privileged - Configuration and Status
            
        case 0233:  ///< rmcm
            return STOP_UNIMP;

        case 0413:  ///< rscr
            //The final computed address, C(TPR.CA), is used to select a system
            //controller and the function to be performed as follows:
            //EffectiveAddress Function
            // y0000x C(system controller mode register) → C(AQ)
            // y0001x C(system controller configuration switches) → C(AQ) y0002x C(mask register assigned to port 0) → C(AQ)
            // y0012x C(mask register assigned to port 1) → C(AQ)
            // y0022x C(mask register assigned to port 2) → C(AQ)
            // y0032x C(mask register assigned to port 3) → C(AQ)
            // y0042x C(mask register assigned to port 4) → C(AQ)
            // y0052x C(mask register assigned to port 5) → C(AQ)
            // y0062x C(mask register assigned to port 6) → C(AQ)
            // y0072x C(mask register assigned to port 7) → C(AQ)
            // y0003x C(interrupt cells) → C(AQ)
            // y0004x
            //   or C(calendar clock) → C(AQ)
            // y0005x
            // y0006x
            //   or C(store unit mode register) → C(AQ)
            // y0007x
            //where: y = value of C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor) used to select the system controller
            //x = any octal digit
            {
                int hi = GETHI(i->IWB);
                int Ysc = (hi >> 15) & 03;
                
                int EAF = hi & 077770;
                switch (EAF)
                {
                    case 0000:
                    case 0010:
                    case 0120:
                    case 0220:
                    case 0320:
                    case 0420:
                    case 0520:
                    case 0620:
                    case 0720:
                        return STOP_UNIMP;
                    case 0040:
                    case 0050:  // return clock/calendar in AQ
                        goto c_0633;    // Goto's are sometimes necesary. I like them ... sosueme.
                        
                        
                    case 0060:
                    case 0070:
                    default:
                        return STOP_UNIMP;
                }
            }
            
            break;
            
        case 0231:  ///< rsw
        
        // Privileged – System Control
        case 0015:  ///< cioc
        case 0553:  ///< smcm
        case 0451:  ///< smic
        case 0057:  ///< sscr
            return STOP_UNIMP;

        // Privileged - Miscellaneous
        case 0212:  ///< absa
            rA = finalAddress;
            break;
            
        case 0616:  ///< dis
            return STOP_DIS;
 
            
        default:
            return STOP_UNIMP;
            
    }
    
    if (i->iwb->flags & STORE_OPERAND)
        writeOperand(i);         // write C(Y) to TPR.CA for any instructions that needs it .....
    
    if (i->iwb->flags & STORE_YPAIR)
        writeOperand2(i); // write YPair to TPR.CA/TPR.CA+1 for any instructions that needs it .....
  
    return 0;
}

t_stat DoEISInstruction(DCDstruct *i)
{
    // XXX not complete .....
    
    int32 opcode = i->opcode;
    if (i->e)
        i->e->ins = i;

    switch (opcode)
    {
        case 0604:  ///< tmoz
            /// If negative or zero indicator ON then
            /// C(TPR.CA) → C(PPR.IC)
            /// C(TPR.TSR) → C(PPR.PSR)
            if (rIR & (I_NEG | I_ZERO))
            {
                rIC = TPR.CA;
                PPR.PSR = TPR.TSR;
                return CONT_TRA;
            }
            break;

        case 0605:  ///< tpnz
            /// If negative and zero indicators are OFF then
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            if (!(rIR & I_NEG) && !(rIR & I_ZERO))
            {
                rIC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;

        case 0601:  ///< trtf
            /// If truncation indicator OFF then
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            if (!(rIR & I_TRUNC))
            {
                rIC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;

        case 0600:  ///< trtn
            /// If truncation indicator ON then
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            if (rIR & I_TRUNC)
            {
                rIC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                CLRF(rIR, I_TRUNC);
                
                return CONT_TRA;
            }
            break;
            
        case 0606:  ///< ttn
            /// If tally runout indicator ON then
            ///  C(TPR.CA) → C(PPR.IC)
            ///  C(TPR.TSR) → C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            if (rIR & I_TALLY)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                return CONT_TRA;
            }
            break;

        case 0310:  ///< easp1
            /// C(TPR.CA) → C(PRn.SNR)
            PR[1].SNR = TPR.CA;
            break;
        case 0312:  ///< easp3
            /// C(TPR.CA) → C(PRn.SNR)
            PR[3].SNR = TPR.CA;
            break;
        case 0330:  ///< easp5
            /// C(TPR.CA) → C(PRn.SNR)
            PR[5].SNR = TPR.CA;
            break;
        case 0332:  ///< easp7
            /// C(TPR.CA) → C(PRn.SNR)
            PR[7].SNR = TPR.CA;
            break;

        case 0311:  ///< eawp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[1].WORDNO = TPR.CA;
            PR[1].BITNO = TPR.TBR;
            break;
        case 0313:  ///< eawp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[3].WORDNO = TPR.CA;
            PR[3].BITNO = TPR.TBR;
            break;
        case 0331:  ///< eawp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[5].WORDNO = TPR.CA;
            PR[5].BITNO = TPR.TBR;
            break;
        case 0333:  ///< eawp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) → C(PRn.WORDNO)
            ///  C(TPR.TBR) → C(PRn.BITNO)
            PR[7].WORDNO = TPR.CA;
            PR[7].BITNO = TPR.TBR;
            break;        
        case 0350:  ///< epbp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[0].RNR = TPR.TRR;
            PR[0].SNR = TPR.TSR;
            PR[0].WORDNO = 0;
            PR[0].BITNO = 0;
            break;
        case 0352:  ///< epbp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[2].RNR = TPR.TRR;
            PR[2].SNR = TPR.TSR;
            PR[2].WORDNO = 0;
            PR[2].BITNO = 0;
            break;
        case 0370:  ///< epbp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[4].RNR = TPR.TRR;
            PR[4].SNR = TPR.TSR;
            PR[4].WORDNO = 0;
            PR[4].BITNO = 0;
            break;
        case 0372:  ///< epbp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) → C(PRn.RNR)
            ///  C(TPR.TSR) → C(PRn.SNR)
            ///  00...0 → C(PRn.WORDNO)
            ///  0000 → C(PRn.BITNO)
            PR[6].RNR = TPR.TRR;
            PR[6].SNR = TPR.TSR;
            PR[6].WORDNO = 0;
            PR[6].BITNO = 0;
            break;
         
        
        case 0351:  ///< epp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[1].RNR = TPR.TRR;
            PR[1].SNR = TPR.TSR;
            PR[1].WORDNO = TPR.CA;
            PR[1].BITNO = TPR.TBR;
            break;
        case 0353:  ///< epp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[3].RNR = TPR.TRR;
            PR[3].SNR = TPR.TSR;
            PR[3].WORDNO = TPR.CA;
            PR[3].BITNO = TPR.TBR;
            break;
        case 0371:  ///< epp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[5].RNR = TPR.TRR;
            PR[5].SNR = TPR.TSR;
            PR[5].WORDNO = TPR.CA;
            PR[5].BITNO = TPR.TBR;
            break;
        case 0373:  ///< epp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) → C(PRn.RNR)
            ///   C(TPR.TSR) → C(PRn.SNR)
            ///   C(TPR.CA) → C(PRn.WORDNO)
            ///   C(TPR.TBR) → C(PRn.BITNO)
            PR[7].RNR = TPR.TRR;
            PR[7].SNR = TPR.TSR;
            PR[7].WORDNO = TPR.CA;
            PR[7].BITNO = TPR.TBR;
            break;
        
        case 0250:  ///< spbp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[0].SNR << 18;
            Ypair[0] |= PR[0].RNR << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
        
        case 0252:  ///< spbp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[2].SNR << 18;
            Ypair[0] |= PR[2].RNR << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandRead, rTAG);
            
            break;
            
        case 0650:  ///< spbp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[4].SNR << 18;
            Ypair[0] |= PR[4].RNR << 15;
            Ypair[1] = 0;
        
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandRead, rTAG);
            
            break;
  
        case 0652:  ///< spbp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  000 → C(Y-pair)0,2
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  00...0 → C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= PR[6].SNR << 18;
            Ypair[0] |= PR[6].RNR << 15;
            Ypair[1] = 0;
            
            //fWrite2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0251:  ///< spri1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[1].SNR << 18;
            Ypair[0] |= PR[1].RNR << 15;
            
            Ypair[1] = PR[1].WORDNO << 18;
            Ypair[1]|= PR[1].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
    
        case 0253:  ///< spri3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[3].SNR << 18;
            Ypair[0] |= PR[3].RNR << 15;
            
            Ypair[1] = PR[3].WORDNO << 18;
            Ypair[1]|= PR[3].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0651:  ///< spri5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[5].SNR << 18;
            Ypair[0] |= PR[5].RNR << 15;
            
            Ypair[1] = PR[5].WORDNO << 18;
            Ypair[1]|= PR[5].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0653:  ///< spri7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 → C(Y-pair)0,2
            ///  C(PRn.SNR) → C(Y-pair)3,17
            ///  C(PRn.RNR) → C(Y-pair)18,20
            ///  00...0 → C(Y-pair)21,29
            ///  (43)8 → C(Y-pair)30,35
            ///  C(PRn.WORDNO) → C(Y-pair)36,53
            ///  000 → C(Y-pair)54,56
            ///  C(PRn.BITNO) → C(Y-pair)57,62
            ///  00...0 → C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= PR[7].SNR << 18;
            Ypair[0] |= PR[7].RNR << 15;
            
            Ypair[1] = PR[7].WORDNO << 18;
            Ypair[1]|= PR[7].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0754:  ///< sra
            /// 00...0 → C(Y)0,32
            /// C(RALR) → C(Y)33,35
            
            //Write(i, TPR.CA, (word36)rRALR, OperandWrite, rTAG);
            CY = (word36)rRALR;
            
            break;
            
            
        // EIS - Address Register Load
        
        case 0560:  ///< aarn - Alphanumeric Descriptor to Address Register n
        case 0561:
        case 0562:
        case 0563:
        case 0564:
        case 0565:
        case 0566:
        case 0567:  
            {
                // For n = 0, 1, ..., or 7 as determined by operation code
                
                n = opcode & 07;  // get

                // C(Y)0,17 → C(ARn.WORDNO)
                AR[n].WORDNO = GETHI(CY);

                int TA = (int)bitfieldExtract36(CY, 13, 2); // C(Y) 21-22
                int CN = (int)bitfieldExtract36(CY, 15, 3); // C(Y) 18-20
                
                switch(TA)
                {
                    case CTA4:  // 2
                        // If C(Y)21,22 = 10 (TA code = 2), then
                        //   C(Y)18,20 / 2 → C(ARn.CHAR)
                        //   4 * (C(Y)18,20)mod2 + 1 → C(ARn.BITNO)
                        AR[n].CHAR = CN / 2;
                        AR[n].BITNO = 4 * (CN % 2) + 1;
                        break;
                        
                    case CTA6:  // 1
                        // If C(Y)21,22 = 01 (TA code = 1), then
                        //   (6 * C(Y)18,20) / 9 → C(ARn.CHAR)
                        //   (6 * C(Y)18,20)mod9 → C(ARn.BITNO)
                        AR[n].CHAR = (6 * CN) / 9;
                        AR[n].BITNO = (6 * CN) % 9;
                        break;
                        
                    case CTA9:  // 0
                        // If C(Y)21,22 = 00 (TA code = 0), then
                        //   C(Y)18,19 → C(ARn.CHAR)
                        //   0000 → C(ARn.BITNO)
                        AR[n].CHAR = (CN >> 1); // remember, 9-bit CN's are funky
                        AR[n].BITNO = 0;
                        break;
                }
            }
            break;
            
        case 0760: ///< larn - Load Address Register n
        case 0761:
        case 0762:
        case 0763:
        case 0764:
        case 0765:
        case 0766:
        case 0767:
            // For n = 0, 1, ..., or 7 as determined by operation code C(Y)0,23 → C(ARn)
            
            n = opcode & 07;  // get n
            AR[n].WORDNO = GETHI(CY);
            AR[n].BITNO = (word6)bitfieldExtract36(CY, 12, 4);
            AR[n].CHAR  = (word2)bitfieldExtract36(CY, 16, 2);
            
            break;
            
        case 0463:  ///< lareg - Load Address Registers
            
            // XXX This will eventually be done automagically
            ReadN(i, 8, TPR.CA, Yblock8, OperandRead, rTAG); // read 8-words from memory
            
            for(n = 0 ; n < 8 ; n += 1)
            {
                tmp36 = Yblock8[n];

                AR[n].WORDNO = GETHI(tmp36);
                AR[n].BITNO = (word6)bitfieldExtract36(tmp36, 12, 4);
                AR[n].CHAR  = (word2)bitfieldExtract36(tmp36, 16, 2);
            }
            break;
            
        case 0467:  ///< lpl - Load Pointers and Lengths
            return STOP_UNIMP;

        case 0660: ///< narn -  (G'Kar?) Numeric Descriptor to Address Register n
        case 0661: 
        case 0662:
        case 0663:
        case 0664:
        case 0665:
        case 0666:  // beware!!!! :-)
        case 0667:
            {
                // For n = 0, 1, ..., or 7 as determined by operation code
                
                n = opcode & 07;  // get
                
                // C(Y)0,17 → C(ARn.WORDNO)
                AR[n].WORDNO = GETHI(CY);
                
                int TN = (int)bitfieldExtract36(CY, 13, 1); // C(Y) 21
                int CN = (int)bitfieldExtract36(CY, 15, 3); // C(Y) 18-20
                
                switch(TN)
                {
                    case CTN4:   // 1
                        // If C(Y)21 = 1 (TN code = 1), then
                        //   (C(Y)18,20) / 2 → C(ARn.CHAR)
                        //   4 * (C(Y)18,20)mod2 + 1 → C(ARn.BITNO)
                        AR[n].CHAR = CN / 2;
                        AR[n].BITNO = 4 * (CN % 2) + 1;
                        break;
                        
                    case CTN9:  // 0
                        // If C(Y)21 = 0 (TN code = 0), then
                        //   C(Y)18,20 → C(ARn.CHAR)
                        //   0000 → C(ARn.BITNO)
                        AR[n].CHAR = CN;
                        AR[n].BITNO = 0;
                        break;
                }
            }
            break;
            
        // EIS - Address Register Store
        case 0540:  ///< aran - Address Register n to Alphanumeric Descriptor
        case 0541:
        case 0542:
        case 0543:
        case 0544:
        case 0545:
        case 0546:
        case 0547:
            {
                // The alphanumeric descriptor is fetched from Y and C(Y)21,22 (TA field) is examined to determine the data type described.
                
                int TA = (int)bitfieldExtract36(CY, 13, 2); // C(Y) 21,22
                
                n = opcode & 07;  // get
                // For n = 0, 1, ..., or 7 as determined by operation code
                
                // C(ARn.WORDNO) → C(Y)0,17
                CY = bitfieldInsert36(CY, AR[n].WORDNO, 18, 18);
                
                // If TA = 1 (6-bit data) or TA = 2 (4-bit data), C(ARn.CHAR) and C(ARn.BITNO) are translated to an equivalent character position that goes to C(Y)18,20.

                int CN = 0;
                
                switch (TA)
                {
                    case CTA4:  // 2
                        // If C(Y)21,22 = 10 (TA code = 2), then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO) – 1) / 4 → C(Y)18,20
                        CN = (9 * AR[n].CHAR + AR[n].BITNO - 1) / 4;
                        CY = bitfieldInsert36(CY, CN, 15, 3);
                        break;
                        
                    case CTA6:  // 1
                        // If C(Y)21,22 = 01 (TA code = 1), then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO)) / 6 → C(Y)18,20
                        CN = (9 * AR[n].CHAR + AR[n].BITNO) / 6;
                        CY = bitfieldInsert36(CY, CN, 15, 3);
                        break;
                        
                    case CTA9:  // 0
                        // If C(Y)21,22 = 00 (TA code = 0), then
                        //   C(ARn.CHAR) → C(Y)18,19
                        //   0 → C(Y)20
                        CY = bitfieldInsert36(CY,          0, 15, 1);
                        CY = bitfieldInsert36(CY, AR[n].CHAR, 16, 2);
                        break;
                }

            // If C(Y)21,22 = 11 (TA code = 3) or C(Y)23 = 1 (unused bit), an illegal procedure fault occurs.
            // Attempted repetition with the rpt, rpd, or rpl instructions causes an illegal procedure fault.
   
            // XXX wire in fault detection
                
            }
            break;
        case 0640:  ///< arnn -  Address Register n to Numeric Descriptor
        case 0641:
        case 0642:
        case 0643:
        case 0644:
        case 0645:
        case 0646:
        case 0647:
            {
                n = opcode & 07;  // get register #
                
                // The Numeric descriptor is fetched from Y and C(Y)21,22 (TA field) is examined to determine the data type described.
                
                int TN = (int)bitfieldExtract36(CY, 14, 1); // C(Y) 21
                
                // For n = 0, 1, ..., or 7 as determined by operation code
                // C(ARn.WORDNO) → C(Y)0,17
                CY = bitfieldInsert36(CY, AR[n].WORDNO, 18, 18);
                
                int CN = 0;
                switch(TN)
                {
                    case CTN4:  // 1
                        // If C(Y)21 = 1 (TN code = 1) then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO) – 1) / 4 → C(Y)18,20
                        CN = (9 * AR[n].CHAR + AR[n].BITNO - 1) / 4;
                        CY = bitfieldInsert36(CY, CN, 15, 3);
                        break;
                        
                    case CTN9:  // 0
                        // If C(Y)21 = 0 (TN code = 0), then
                        //   C(ARn.CHAR) → C(Y)18,19
                        //   0 → C(Y)20
                        CY = bitfieldInsert36(CY,          0, 15, 1);
                        CY = bitfieldInsert36(CY, AR[n].CHAR, 16, 2);
                        break;
                }
                
                //If C(Y)21,22 = 11 (TA code = 3) or C(Y)23 = 1 (unused bit), an illegal procedure fault occurs.
                //Attempted repetition with the rpt, rpd, or rpl instructions causes an illegal procedure fault.
            }
            
            
            break;
            
        case 0740:  ///< sarn - Store Address Register n
        case 0741:
        case 0742:
        case 0743:
        case 0744:
        case 0745:
        case 0746:
        case 0747:
            //For n = 0, 1, ..., or 7 as determined by operation code
            //  C(ARn) → C(Y)0,23
            //  C(Y)24,35 → unchanged
            
            n = opcode & 07;  // get n
            CY = bitfieldInsert36(CY, AR[n].WORDNO, 18, 18);
            CY = bitfieldInsert36(CY, AR[n].BITNO,  12,  4);
            CY = bitfieldInsert36(CY, AR[n].CHAR,   16,  2);
            
            break;
            
        case 0443:  ///< sareg - Store Address Registers
            memset(Yblock8, 0, sizeof(Yblock8));
            for(n = 0 ; n < 8 ; n += 1)
            {
                word36 arx = 0;
                arx = bitfieldInsert36(arx, AR[n].WORDNO, 18, 18);
                arx = bitfieldInsert36(arx, AR[n].BITNO,  12,  4);
                arx = bitfieldInsert36(arx, AR[n].CHAR,   16,  2);
                
                Yblock8[n] = arx;
            }
            // XXX this will eventually be done automagically.....
            WriteN(i, 8, TPR.CA, Yblock8, OperandWrite, rTAG); // write 8-words to memory
            
            break;
            
        case 0447:  ///< spl - Store Pointers and Lengths
            return STOP_UNIMP;
            
        // EIS - Address Register Special Arithmetic
        case 0500:  ///< a9bd        Add 9-bit Displacement to Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)

                int r = getCrAR(reg);

                if (!i->a)
                {
                    // If A = 0, then
                    //   ADDRESS + C(REG) / 4 → C(ARn.WORDNO)
                    //   C(REG)mod4 → C(ARn.CHAR)
                    AR[ARn].WORDNO = (address + r / 4);
                    AR[ARn].CHAR = r % 4;
                }
                else
                {
                    // If A = 1, then
                    //   C(ARn.WORDNO) + ADDRESS + (C(REG) + C(ARn.CHAR)) / 4 → C(ARn.WORDNO)
                    //   (C(ARn.CHAR) + C(REG))mod4 → C(ARn.CHAR)
                    AR[ARn].WORDNO += (address + r / 4);
                    AR[ARn].CHAR = (AR[ARn].CHAR + r) % 4;
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;

                // 0000 → C(ARn.BITNO)
                AR[ARn].BITNO = 0;
            }
            break;
            
        case 0501:  ///< a6bd        Add 6-bit Displacement to Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                int r = getCrAR(reg);
                
                // If A = 0, then
                //   ADDRESS + C(REG) / 6 → C(ARn.WORDNO)
                //   ((6 * C(REG))mod36) / 9 → C(ARn.CHAR)
                //   (6 * C(REG))mod9 → C(ARn.BITNO)
                //If A = 1, then
                //   C(ARn.WORDNO) + ADDRESS + (9 * C(ARn.CHAR) + 6 * C(REG) + C(ARn.BITNO)) / 36 → C(ARn.WORDNO)
                //   ((9 * C(ARn.CHAR) + 6 * C(REG) + C(ARn.BITNO))mod36) / 9 → C(ARn.CHAR)
                //   (9 * C(ARn.CHAR) + 6 * C(REG) + C(ARn.BITNO))mod9 → C(ARn.BITNO)
                if (!i->a)
                {
                    AR[ARn].WORDNO = address + r / 6;
                    AR[ARn].CHAR = ((6 * r) % 36) / 9;
                    AR[ARn].BITNO = (6 * r) % 9;
                }
                else
                {
                    AR[ARn].WORDNO = AR[ARn].WORDNO + address + (9 * AR[ARn].CHAR + 6 * r + AR[ARn].BITNO) / 36;
                    AR[ARn].CHAR = ((9 * AR[ARn].CHAR + 6 * r + AR[ARn].BITNO) % 36) / 9;
                    AR[ARn].BITNO = (9 * AR[ARn].CHAR + 6 * r + AR[ARn].BITNO) % 9;
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;
                AR[ARn].BITNO &= 077;
            }
            break;
            
        case 0502:  ///< a4bd        Add 4-bit Displacement to Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                int r = getCrAR(reg);
                
                if (!i->a)
                {
                    // If A = 0, then
                    //   ADDRESS + C(REG) / 4 → C(ARn.WORDNO)
                    //   C(REG)mod4 → C(ARn.CHAR)
                    //   4 * C(REG)mod2 + 1 → C(ARn.BITNO)
                    AR[ARn].WORDNO = address + r / 4;
                    AR[ARn].CHAR = r % 4;
                    AR[ARn].BITNO = 4 * r % 2 + 1;
                } else
                {
                    // If A = 1, then
                    //   C(ARn.WORDNO) + ADDRESS + (9 * C(ARn.CHAR) + 4 * C(REG) + C(ARn.BITNO)) / 36 → C(ARn.WORDNO)
                    //   ((9 * C(ARn.CHAR) + 4 * C(REG) + C(ARn.BITNO))mod36) / 9 → C(ARn.CHAR)
                    //   4 * (C(ARn.CHAR) + 2 * C(REG) + C(ARn.BITNO) / 4)mod2 + 1 → C(ARn.BITNO)
                    AR[ARn].WORDNO = AR[ARn].WORDNO + address + (9 * AR[ARn].CHAR + 4 * r + AR[ARn].BITNO) / 36;
                    AR[ARn].CHAR = ((9 * AR[ARn].CHAR + 4 * r + AR[ARn].BITNO) % 36) / 9;
                    AR[ARn].BITNO = 4 * (AR[ARn].CHAR + 2 * r + AR[ARn].BITNO / 4) % 2 + 1;
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;
                AR[ARn].BITNO &= 077;
            }
            break;
            
        case 0503:  ///< abd         Add   bit Displacement to Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                int r = getCrAR(reg);
                
                if (!i->a)
                {
                    // If A = 0, then
                    //   ADDRESS + C(REG) / 36 → C(ARn.WORDNO)
                    //   (C(REG)mod36) / 9 → C(ARn.CHAR)
                    //   C(REG)mod9 → C(ARn.BITNO)
                    AR[ARn].WORDNO = address + r / 36;
                    AR[ARn].CHAR = (r % 36) / 9;
                    AR[ARn].BITNO = r % 9;
                }
                else
                {
                    // If A = 1, then
                    //   C(ARn.WORDNO) + ADDRESS + (9 * C(ARn.CHAR) + 36 * C(REG) + C(ARn.BITNO)) / 36 → C(ARn.WORDNO)
                    //   ((9 * C(ARn.CHAR) + 36 * C(REG) + C(ARn.BITNO))mod36) / 9 → C(ARn.CHAR)
                    //   (9 * C(ARn.CHAR) + 36 * C(REG) + C(ARn.BITNO))mod9 → C(ARn.BITNO)
                    AR[ARn].WORDNO = AR[ARn].WORDNO + address + (9 * AR[ARn].CHAR + 36 * r + AR[ARn].BITNO) / 36;
                    AR[ARn].CHAR = ((9 * AR[ARn].CHAR + 36 * r + AR[ARn].BITNO) % 36) / 9;
                    AR[ARn].BITNO = (9 * AR[ARn].CHAR + 36 * r + AR[ARn].BITNO) % 9;
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;
                AR[ARn].BITNO &= 077;
            }
            break;
            
        case 0507:  ///< awd         Add  word Displacement to Address Register
            {
            
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                
                // If A = 0, then
                //   ADDRESS + C(REG) → C(ARn.WORDNO)
                // If A = 1, then
                //   C(ARn.WORDNO) + ADDRESS + C(REG) → C(ARn.WORDNO)
                // 00 → C(ARn.CHAR)
                // 0000 → C(ARn.BITNO)
                
                if (!i->a)
                    AR[ARn].WORDNO = (address + getCrAR(reg));
                else
                    AR[ARn].WORDNO += (address + getCrAR(reg));
                
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR = 0;
                AR[ARn].BITNO = 0;
            }
            break;
            
        case 0520:  ///< s9bd   Subtract 9-bit Displacement from Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)

                int r = getCrAR(reg);
                
                if (!i->a)
                {
                    // If A = 0, then
                    //   - (ADDRESS + C(REG) / 4) → C(ARn.WORDNO)
                    //   - C(REG)mod4 → C(ARn.CHAR)
                    AR[ARn].WORDNO = -(address + r / 4);
                    AR[ARn].CHAR = - (r % 4);
                }
                else
                {
                    // If A = 1, then
                    //   C(ARn.WORDNO) - ADDRESS + (C(ARn.CHAR) - C(REG)) / 4 → C(ARn.WORDNO)
                    //   (C(ARn.CHAR) - C(REG))mod4 → C(ARn.CHAR)
                    AR[ARn].WORDNO = AR[ARn].WORDNO - address + (AR[ARn].CHAR - r) / 4;
                    AR[ARn].CHAR = (AR[ARn].CHAR - r) % 4;
                }
                
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;
                // 0000 → C(ARn.BITNO)
                AR[ARn].BITNO = 0;
            }
            break;
            
        case 0521:  ///< s6bd   Subtract 6-bit Displacement from Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                int r = getCrAR(reg);
                
                if (!i->a)
                {
                    // If A = 0, then
                    //   - (ADDRESS + C(REG) / 6) → C(ARn.WORDNO)
                    //   - ((6 * C(REG))mod36) / 9 → C(ARn.CHAR)
                    //   - (6 * C(REG))mod9 → C(ARn.BITNO)
                    AR[ARn].WORDNO = -(address + r / 6);
                    AR[ARn].CHAR = -((6 * r) % 36) / 9;
                    AR[ARn].BITNO = -(6 * r) % 9;
                }
                else
                {
                    // If A = 1, then
                    //   C(ARn.WORDNO) - ADDRESS + (9 * C(ARn.CHAR) - 6 * C(REG) + C(ARn.BITNO)) / 36 → C(ARn.WORDNO)
                    //   ((9 * C(ARn.CHAR) - 6 * C(REG) + C(ARn.BITNO))mod36) / 9 → C(ARn.CHAR)
                    //   (9 * C(ARn.CHAR) - 6 * C(REG) + C(ARn.BITNO))mod9 → C(ARn.BITNO)
                    AR[ARn].WORDNO = AR[ARn].WORDNO - address + (9 * AR[ARn].CHAR - 6 * r + AR[ARn].BITNO) / 36;
                    AR[ARn].CHAR = ((9 * AR[ARn].CHAR - 6 * r + AR[ARn].BITNO) % 36) / 9;
                    AR[ARn].BITNO = (9 * AR[ARn].CHAR - 6 * r + AR[ARn].BITNO) % 9;
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;
                AR[ARn].BITNO &= 077;
            }
            break;
                
        case 0522:  ///< s4bd   Subtract 4-bit Displacement from Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                int r = getCrAR(reg);
                
                if (!i->a)
                {
                    // If A = 0, then
                    //   - (ADDRESS + C(REG) / 4) → C(ARn.WORDNO)
                    //   - C(REG)mod4 → C(ARn.CHAR)
                    //   - 4 * C(REG)mod2 + 1 → C(ARn.BITNO)
                    AR[ARn].WORDNO = -(address + r / 4);
                    AR[ARn].CHAR = -(r % 4);
                    AR[ARn].BITNO = -4 * r % 2 + 1;
                }
                else
                {
                    // If A = 1, then
                    //   C(ARn.WORDNO) - ADDRESS + (9 * C(ARn.CHAR) - 4 * C(REG) + C(ARn.BITNO)) / 36 → C(ARn.WORDNO)
                    //   ((9 * C(ARn.CHAR) - 4 * C(REG) + C(ARn.BITNO))mod36) / 9 → C(ARn.CHAR)
                    //   4 * (C(ARn.CHAR) - 2 * C(REG) + C(ARn.BITNO) / 4)mod2 + 1 → C(ARn.BITNO)

                    AR[ARn].WORDNO = AR[ARn].WORDNO - address + (9 * AR[ARn].CHAR - 4 * r + AR[ARn].BITNO) / 36;
                    AR[ARn].CHAR = ((9 * AR[ARn].CHAR - 4 * r + AR[ARn].BITNO) % 36) / 9;
                    AR[ARn].BITNO = 4 * (AR[ARn].CHAR - 2 * r + AR[ARn].BITNO / 4) % 2 + 1;
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;
                AR[ARn].BITNO &= 077;
            }
            break;
            
        case 0523:  ///< sbd    Subtract   bit Displacement from Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                int r = getCrAR(reg);
                
                if (!i->a)
                {
                    // If A = 0, then
                    //   - (ADDRESS + C(REG) / 36) → C(ARn.WORDNO)
                    //   - (C(REG)mod36) / 9 → C(ARn.CHAR)
                    //   - C(REG)mod9 → C(ARn.BITNO)
                    AR[ARn].WORDNO = -(address + r / 36);
                    AR[ARn].CHAR = -(r %36) / 9;
                    AR[ARn].BITNO = -(r % 9);
                }
                else
                {
                    // If A = 1, then
                    //   C(ARn.WORDNO) - ADDRESS + (9 * C(ARn.CHAR) - 36 * C(REG) + C(ARn.BITNO)) / 36 → C(ARn.WORDNO)
                    //  ((9 * C(ARn.CHAR) - 36 * C(REG) + C(ARn.BITNO))mod36) / 9 → C(ARn.CHAR)
                    //  (9 * C(ARn.CHAR) - 36 * C(REG) + C(ARn.BITNO))mod9 → C(ARn.BITNO)
                    AR[ARn].WORDNO = AR[ARn].WORDNO - address + (9 * AR[ARn].CHAR - 36 * r + AR[ARn].BITNO) / 36;
                    AR[ARn].CHAR = ((9 * AR[ARn].CHAR - 36 * r + AR[ARn].BITNO) % 36) / 9;
                    AR[ARn].BITNO = (9 * AR[ARn].CHAR - 36 * r + AR[ARn].BITNO) % 9;
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                AR[ARn].CHAR &= 03;
                AR[ARn].BITNO &= 077;
            }
            break;
            
        case 0527:  ///< swd    Subtract  word Displacement from Address Register
            {
                int ARn = (int)bitfieldExtract36(i->IWB, 33, 3);// 3-bit register specifier
                int address = SIGNEXT15((int)bitfieldExtract36(i->IWB, 18, 15));// 15-bit Address (Signed?)
                int reg = i->IWB & 017;                     // 4-bit register modification (None except au, qu, al, ql, xn)
                
                int r = getCrAR(reg);
                
                if (!i->a)
                {
                    // If A = 0, then
                    //   - (ADDRESS + C(REG)) → C(ARn.WORDNO)
                    AR[ARn].WORDNO = -(address + r);
                }
                else
                {
                    // If A = 1, then
                    //     C(ARn.WORDNO) - (ADDRESS + C(REG)) → C(ARn.WORDNO)
                    AR[ARn].WORDNO = AR[ARn].WORDNO - (address + r);
                }
                AR[ARn].WORDNO &= AMASK;    // keep to 18-bits
                // 00 → C(ARn.CHAR)
                // 0000 → C(ARn.BITNO)
                AR[ARn].CHAR = 0;
                AR[ARn].BITNO = 0;
            }
            break;
            
        /// Multiword EIS ...
        case opcode1_btd:   // 0301:  ///< btd
            btd (i);
            break;
            
        case 0305:  ///< dtb
            dtb (i);
            break;
            
        case 024:   ///< mvne
            mvne(i);
            break;
         
        case 020:   ///< mve
            mve(i);
            break;

        case 0100:  ///< mlr
            mlr(i);
            break;

        case 0101:  ///< mrl
            mrl(i);
            break;
        
        case 0160:  ///< mvt
            mvt(i);
            break;
            
        case 0124:  ///< scm
            scm(i);
            break;

        case 0125:  ///< scmr
            scmr(i);
            break;

        case 0164:  ///< tct
            tct(i);
            break;

        case 0165:  ///< tctr
            tctr(i);
            break;
            
        case 0106:  ///< cmpc
            cmpc(i);
            break;
            
        case 0120:  ///< scd
            scd(i);
            break;
      
        case 0121:  ///< scdr
            scdr(i);
            break;
          
        // bit-string operations
        case 066:   ///< cmpb
            cmpb(i);
            break;

        case 060:   ///< csl
            csl(i);
            break;

        case 061:   ///< csr
            csr(i);
            break;

        case 064:   ///< sztl
            sztl(i);
            break;

        case 065:   ///< sztr
            sztr(i);
            break;

        // decimal arithmetic instrutions
        case 0202:  ///< ad2d
            ad2d(i);
            break;

        case 0222:  ///< ad3d
            ad3d(i);
            break;
            
        case 0203:  ///< sb2d
            sb2d(i);
            break;
            
        case 0223:  ///< sb3d
            sb3d(i);
            break;

        case 0206:  ///< mp2d
            mp2d(i);
            break;

        case 0226:  ///< mp3d
            mp3d(i);
            break;

        case 0207:  ///< dv2d
            dv2d(i);
            break;

        case 0227:  ///< dv3d
            dv3d(i);
            break;

        case 0303:  ///< cmpn
            cmpn(i);
            break;

#ifdef EMULATOR_ONLY
            
        case 0420:  ///< emcall instruction Custom, for an emulator call for simh stuff ...
            emCall(i);
            break;

#ifdef DEPRECIATED
            /*
             * a proposal for PUSHT, POPT from MIT AI memoramdum AIM-072 ...
             
             A single instruction for entering a procedure and reserving a block of storage on the push down stack must be capable of incrementing the push down pointer, checking to determine if the push down stack has been exhausted, placing the saved instruction location and the size of the push down block at the end of the block, and transferring to the entry point of the procedure. The proposed instructions PUSHT, and POPT will probably have to be modified to accomodate the program segment scheme.
             
             
             PUSHT:
             
             The instruction has an address subject to all normal modifications. The address determines the location of a data word which is interpreted according to format I.
             
             0    entry        11 t 22    blk      3
                               78   12             5
             
             Format I
             
             After the data word has been obtained, the instruction proceeds as follows.
             1) The contents of blk are added to Xt (The field blk is made into an 18-bit field
             by preceeding it with bits having the same sign as the left-most bit on blk. This permits negative indexing.)
             2) A word is formed using the contents of the instruction counter (ic) and the block size (blk) as shown by format II.
             
             0      ic        1    2     blk    3
                              7    2            5
             
             Format II
             
             3) If C(Xt) > C(Xt+1) then the format II word is stored at the address determined by X(t), otherwise control goes to trap.
             4) A transfer is made to the address specified in the entry field.
             
             TRAP:
             A trap is made to a fixed location relocated within the segment containing the PUSHT instruction. THe processor does not leave slave mode.
             
             POPT:
             
             0       y        11 P 2         3 t 3
                              78 O 1         2   5
                                 P
                                 T
             
             1) The data word addressed by Xt is obtained and interpreted according to format II.
             2) The blk field is subtracted from Xt
             3) A transfer is made to the location which is the sum of the y field of the instruction and the ic field of the format II word.
             
             
             HWR 29 Nov 2012 
             
             Interesting proposal. I wonder if the author really thought about this much. It seems that the format I/II words
             are not terribly useful outside of these instructions. If, however, blk is reduced to 12-bits and the fields reorganized
             to match a standard indirect word then the instructions become much more useful... giving us the ability to use the
             fmt 1/2 words more like modern fp/sp/bp's, Of course, I'm a noob to the 645/dps8 so who am I to judge?
             
             new Format  I word... Entry (0-17), blk(18-29), Tag (30-35)    // to where we're going
             mew Format II word...    IC (0-17), blk(18-29), Tag (30-35)    // whence we came from
             */

        case 0421:  ///< callx    (call using index register as SP)
            {
                
                word18 entry = GET_ADDR(CY);
                int blk = SIGNEXT12(GET_TALLY(CY));
                word4 t = X(GET_TAG(CY));
                
                //sim_debug(DBG_TRACE, &cpu_dev,  "pusht():C(Y)=%012llo\n", CY);
                //sim_debug(DBG_TRACE, &cpu_dev,  "pusht():entry:%06o t:%o blk:%o X[%o]=%06o\n", entry, t, blk, t, rX[t]);
                
                //if (rX[t] > rX[t+1]) // too big!
                //    break;
                
                rX[t] += blk;
                
                word36 fmt2 = (((rIC + 1) & AMASK) << 18) | (blk << 6);     ///< | t;
                //sim_debug(DBG_TRACE, &cpu_dev,  "pusht():writine fmt2=%012llo to X[%o]=%06o\n", fmt2, t, rX[t]);
                
                Write(rX[t], fmt2, OperandWrite, 0);    // write fmt 2 word to X[n] + blk
                
                rX[t] += 1;
                
                rIC = entry;
                
                return CONT_TRA;
            }
            break;
        case 0422:  ///< exit n, x - exit subroutine removing n args via x
            {
                n = X(tag);    // n
                word18 y = GETHI(IWB);
                
                rX[n] -= 1;
                
                word36 fmt2;
                //sim_debug(DBG_TRACE, &cpu_dev,  "popt(): reading fmt2 from X[%o]=%06o\n", n, rX[n]);
                Read(rX[n], &fmt2, OperandRead, 0);
                //sim_debug(DBG_TRACE, &cpu_dev,  "popt(): fmt2=%012llo\n", fmt2);
                
                word18 ic =  GETHI(fmt2);
                int blk = SIGNEXT12(GET_TALLY(fmt2));
            
                rX[n] -= blk;
                
                //word18 rICx = (y + ic) & AMASK;
                //sim_debug(DBG_TRACE, &cpu_dev, "popt() y=%06o n=%o ic:%06o blk=%d rICx=%06o\n", y, n, ic, blk, rICx);
                
                rIC = (y + ic) & AMASK;
                //sim_debug(DBG_TRACE, &cpu_dev,  "popt() new IC = %06o X[%o]=%06o\n", rIC, n, rX[n]);
                
                return CONT_TRA;
                
            }
            break;
            
        case 0423:  ///< pusha
            n = X(tag);    // n
            Write(rX[n], rA, OperandWrite, 0);    // write A to TOS
            rX[n] += 1;
            break;
            
        case 0424:  ///< popa
            n = X(tag);    // n
            rX[n] -= 1;
            Read(rX[n], &rA, OperandRead, 0);    // read A from TOS-1
            break;
            
        case 0425:  ///< pushq
            n = X(tag);    // n
            Write(rX[n], rQ, OperandWrite, 0);    // write Q to TOS
            rX[n] += 1;
            break;

        case 0426:  ///< popq
            n = X(tag);    // n
            rX[n] -= 1;
            Read(rX[n], &rQ, OperandRead, 0);    // read A from TOS-1
            break;
#endif  /* DEPRECIATED */
            
#endif
        // priviledged instructions
            
        case 0257:  ///< lptp
        case 0173:  ///< lptr
        case 0774:  ///< lra
        case 0232:  ///< ldsr
        case 0557:  ///< sptp
        case 0154:  ///< sptr
        case 0254:  ///< ssdr
            
        // Privileged - Clear Associative Memory
        case 0532:  ///< camp
            
        default:
            return STOP_UNIMP;
    }

    if (i->iwb->flags & STORE_OPERAND)
        writeOperand(i); // write C(Y) to TPR.CA for any instructions that needs it .....

    if (i->iwb->flags & STORE_YPAIR)
        writeOperand2(i); // write YPair to TPR.CA/TPR.CA+1 for any instructions that needs it .....

    return 0;

}

#include <ctype.h>

/**
 * emulator call instruction. Do whatever address field sez' ....
 */
void
emCall(DCDstruct *i)
{
    //fprintf(stderr, "emCall()... %d\n", TPR.CA);
    switch (i->address) /// address field
    {
        case 1:     ///< putc9 - put 9-bit char in AL to stdout
        {
            char c = rA & 0x7f;
            if (c)  // ignore NULL chars. 
                putc(c, stdout);
            break; 
        }
        case 100:     ///< putc9 - put 9-bit char in A(0) to stdout
        {
            char c = (rA >> 27) & 0x7f;
            if (isascii(c))  // ignore NULL chars.
                putc(c, stdout);
            else
                printf("\\%03o", c);
            break;
        }
        case 2:     ///< putc6 - put 6-bit char in A to stdout
        {
            int c = GEBcdToASCII[rA & 077];
            if (c != -1)
            {
                if (isascii(c))  // ignore NULL chars.
                    putc(c, stdout);
                else
                    printf("\\%3o", c);
            }
            //putc(c, stdout);
            break;
        }
        case 3:     ///< putoct - put octal contents of A to stdout (split)
        {
            printf("%06o %06o", GETHI(rA), GETLO(rA));
            break;
        }
        case 4:     ///< putoctZ - put octal contents of A to stdout (zero-suppressed)
        {
            printf("%llo", rA);
            break;
        }
        case 5:     ///< putdec - put decimal contents of A to stdout
        {
            word36 tmp = SIGNEXT36(rA);
            printf("%lld", tmp);
            break;
        }
        case 6:     ///< putEAQ - put float contents of C(EAQ) to stdout
        {
            long double eaq = EAQToIEEElongdouble();
            printf("%12.8Lg", eaq);
            break;
        }
            
//        case 7:     ///< putdescA put alpha-numeric descriptor given in A to stdout .....
//        {
//            word18 addr = GETHI(rA);    // get address of data to write (ABS mode only)
//            int CN = (int)bitfieldExtract36(rA, 15, 3);  // get starting character position
//            int TA = (int)bitfieldExtract36(rA, 13, 2);  // get data type
//            int  N = (int)bitfieldExtract36(rA, 0, 12);   // get char count
//            
//            get469(0, 0, 0);    // initialize char getter
//            for(int n = 0 ; n < N ; n += 1)
//            {
//                int c = get469(&addr, &CN, TA); // get character
//                switch (TA)
//                {
//                    case CTA4:
//                    case CTA6:
//                        putc(GEBcdToASCII[c], stdout);
//                        break;
//                    case CTA9:
//                        putc(c & 0x7f, stdout);
//                        break;
//                }
//            }
//            break;
//        }
    }
}

