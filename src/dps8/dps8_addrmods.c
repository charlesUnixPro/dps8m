
/**
 * \file dps8_addrmods.c
 * \project dps8
 * \author Harry Reed
 * \date 9/25/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
 */

#include <stdio.h>
#include "dps8.h"
#include "dps8_addrmods.h"
#include "dps8_cpu.h"
#include "dps8_ins.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_iefp.h"
#include "dps8_faults.h"

// Computed Address Formation Flowcharts

static bool directOperandFlag = false;
static word36 directOperand = 0;

/*!
 * return contents of register indicated by Td
 */
static word18 getCr(word4 Tdes)
{
    directOperandFlag = false;
    
    if (Tdes == 0)
        return 0;
    
    if (Tdes & 010) /* Xn */
        return rX[X(Tdes)];
    
    switch(Tdes)
    {
        case TD_N:  ///< rY = address from opcode
            return 0;
        case TD_AU: ///< rY + C(A)0,17
            return GETHI(rA);
        case TD_QU: ///< rY + C(Q)0,17
            return GETHI(rQ);
        case TD_DU: ///< none; operand has the form y || (00...0)18
            directOperand = 0;
            SETHI(directOperand, rY);
            directOperandFlag = true;
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "getCr(TD_DU): rY=%06o directOperand=%012llo\n", rY, directOperand);
        
            return 0;
        case TD_IC: ///< rY + C(PPR.IC)
            return PPR.IC;
        case TD_AL: ///< rY + C(A)18,35
            return GETLO(rA);
        case TD_QL: ///< rY + C(Q)18,35
            return GETLO(rQ);
        case TD_DL: ///< none; operand has the form (00...0)18 || y
            directOperand = 0;
            SETLO(directOperand, rY);
            directOperandFlag = true;
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "getCr(TD_DL): rY=%06o directOperand=%012llo\n", rY, directOperand);
            return 0;
    }
    return 0;
}

/*
 * New address stuff (EXPERIMENTAL)
 */

static char * opDescSTR (DCDstruct *i)
{
    static char temp[256];
    
    strcpy(temp, "");
    
    if (READOP(i))
    {
        switch (OPSIZE(i))
        {
            case 1:
                strcat (temp, "readCY");
                break;
            case 2:
                strcat (temp, "readCYpair2");
                break;
            case 8:
                strcat (temp, "readCYblock8");
                break;
            case 16:
                strcat (temp, "readCYblock16");
                break;
        }
    }
    if (WRITEOP(i))
    {
        if (strlen(temp))
            strcat(temp, "/");
        
        switch (OPSIZE(i))
        {
            case 1:
                strcat(temp, "writeCY");
                break;
            case 2:
                strcat(temp, "writeCYpair2");
                break;
            case 8:
                strcat(temp, "writeCYblock8");
                break;
            case 16:
                strcat(temp, "writeCYblock16");
                break;
        }
    }
    if (TRANSOP(i))
    {
        if (strlen(temp))
            strcat(temp, "/");
        
        strcat(temp, "prepareCA (TRA)");
    }
    
    if (!READOP(i) && !WRITEOP(i) && !TRANSOP(i) && i->info->flags & PREPARE_CA)
    {
        if (strlen(temp))
        strcat(temp, "/");
        
        strcat(temp, "prepareCA");
    }
    return temp;    //"opDescSTR(???)";
}

static char * operandSTR(DCDstruct *i)
{
    if (i->info->ndes > 0)
        return "operandSTR(): MWEIS not handled yet";
        
    static char temp[1024];
    
    int n = OPSIZE(i);
    switch (n)
    {
        case 1:
            sprintf(temp, "CY=%012llo", CY);
            break;
        case 2:
            sprintf(temp, "CYpair[0]=%012llo CYpair[1]=%012llo", Ypair[0], Ypair[1]);
            break;
        case 8:
        case 16:
        default:
            sprintf(temp, "Unhandled size: %d", n);
            break;
    }
    return temp;
}

modificationContinuation _modCont, *modCont = &_modCont;

static char * modContSTR(modificationContinuation *i)
{
    if (!i)
        return "modCont is null";
    
    //if (i->bActive == false)
    //    return "modCont NOT active";
    
    static char temp[256];
    sprintf(temp,
            "Address:%06o segment:%05o tally:%d delta%d mod:%d tb:%d cf:%d indword:%012llo",
            i->address, i->segment, i->tally, i->delta, i->mod, i->tb, i->cf, i->indword );
    return temp;
    
}


void doComputedAddressContinuation(DCDstruct *i)    //, eCAFoper operType)
{
    if (modCont->bActive == false)
        return; // no continuation available
    
    //    if (operType == writeCY)
    //    {
    //        sim_printf("doComputedAddressContinuation(): operTpe != writeCY (%s)\n", opDescSTR(i));
    //        return;
    //    }
    
    directOperandFlag = false;
    
    TPR.TSR = modCont->segment;
    
    //modCont->bActive = false;   // assume no continuation necessary
    
    sim_debug(DBG_ADDRMOD, &cpu_dev, "doComputedAddressContinuation(Entry): '%s'\n", modContSTR(modCont));
    
    switch (modCont->mod)
    {
        case IT_CI: ///< Character indirect (Td = 10)
        
            ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character/byte position value, cf. Bits 31-32 of the TAG field must be zero.
            ///< If the character position value is greater than 5 for 6-bit characters or greater than 3 for 9- bit bytes, an illegal procedure, illegal modifier, fault will occur. The TALLY field is ignored. The computed address is the value of the ADDRESS field of the indirect word. The effective character/byte number is the value of the character position count, cf, field of the indirect word.
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): restoring instruction continuation '%s'\n", modContSTR(modCont));
        
            int Yi = modCont->address ;
            tTB = modCont->tb;
            tCF = modCont->cf;
        
            word36 data;
        
            // read data where chars/bytes now live
            //Read(i, Yi, &data, INDIRECT_WORD_FETCH, i->a); //TM_IT);
            Read(i, Yi, &data, OPERAND_READ, i->a); //TM_IT);
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): read char/byte %012llo from %06o tTB=%o tCF=%o\n", data, Yi, tTB, tCF);
        
        // set byte/char
        switch (tTB)
        {
            case TB6:
                putChar(&data, CY & 077, tCF);
                break;
            case TB9:
                putByte(&data, CY & 0777, tCF);
                break;
            default:
                sim_printf("IT_MOD(IT_CI): unknown tTB:%o\n", tTB);
                break;
        }
        
        // write it
        Write(i, Yi, data, OPERAND_STORE, i->a);   //TM_IT);
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): wrote char/byte %012llo to %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
        
        return;
        
        case IT_SC: ///< Sequence character (Td = 12)
        ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character/byte position counter, cf. Bits 31-32 of the TAG field must be zero.
        
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): restoring instruction continuation '%s'\n", modContSTR(modCont));
        
        Yi = modCont->address;
        tTB = modCont->tb;
        tCF = modCont->cf;
        
        // read data where chars/bytes now live (if it hasn't already been read in)
        if (!(i->info->flags & READ_OPERAND))
            Read(i, Yi, &data, OPERAND_READ, i->a); //TM_IT);
        else
            data = CY;
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): read char/byte %012llo from %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
        
        // set byte/char
        switch (tTB)
        {
            case TB6:
                putChar(&data, CY & 077, tCF);
                break;
            case TB9:
                putByte(&data, CY & 0777, tCF);
                break;
            default:
                sim_printf("IT_MOD(IT_SC): unknown tTB:%o\n", tTB);
                break;
        }
        
        // write it
        Write(i, Yi, data, OPERAND_STORE, i->a);   //TM_IT);
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): wrote char/byte %012llo to %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
        
        return;
        
        case IT_SCR: ///< Sequence character reverse (Td = 5)
        ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character position counter, cf. Bits 31-32 of the TAG field must be zero.
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): restoring instruction continuation '%s'\n", modContSTR(modCont));
        
        Yi = modCont->address;
        tTB = modCont->tb;
        tCF = modCont->cf;
        
        
        // read data where chars/bytes now live (if it hasn't already been read in)
        if (!(i->info->flags & READ_OPERAND))
            Read(i, Yi, &data, OPERAND_READ, i->a); //TM_IT);
        else
            data = CY;
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): read char/byte %012llo from %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
        
        // set byte/char
        switch (tTB)
        {
            case TB6:
                putChar(&data, CY & 077, tCF);
                break;
            case TB9:
                putByte(&data, CY & 0777, tCF);
                break;
            default:
                fprintf(stderr, "IT_MOD(IT_SCR): unknown tTB:%o\n", tTB);
                break;
        }
        
            // write it
            Write(i, Yi, data, OPERAND_STORE, i->a);   //TM_IT);
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): wrote char/byte %012llo to %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
        
        
        return;
        
        case IT_I: ///< Indirect (Td = 11)
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): restoring continuation '%s'\n", modContSTR(modCont));
            TPR.CA = modCont->address;
        
            Write(i, TPR.CA, CY, OPERAND_STORE, i->a); //TM_IT);
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): wrote operand %012llo to %06o\n", CY, TPR.CA);
            return;
        
        case IT_AD: ///< Add delta (Td = 13)
        ///< The TAG field of the indirect word is interpreted as a 6-bit, unsigned, positive address increment value, delta. For each reference to the indirect word, the ADDRESS field is increased by delta and the TALLY field is reduced by 1 after the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF. The computed address is the value of the unmodified ADDRESS field of the indirect word.
        
        // read data
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): restoring continuation '%s'\n", modContSTR(modCont));
        
            TPR.CA = modCont->address;
        
            Write(i, TPR.CA, CY, OPERAND_STORE, i->a); //TM_IT);
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): wrote operand %012llo to %06o\n", CY, TPR.CA);
            return;
        
        case IT_SD: ///< Subtract delta (Td = 4)
        ///< The TAG field of the indirect word is interpreted as a 6-bit, unsigned, positive address increment value, delta. For each reference to the indirect word, the ADDRESS field is reduced by delta and the TALLY field is increased by 1 before the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field overflows to 0, the tally runout indicator is set ON, otherwise it is set OFF. The computed address is the value of the decremented ADDRESS field of the indirect word.
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): restoring continuation '%s'\n", modContSTR(modCont));
        
            Yi = modCont->address;
        
            Write(i, Yi, CY, OPERAND_STORE, i->a); //TM_IT);
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): wrote operand %012llo to %06o\n", CY, TPR.CA);
            return;
        
        case IT_DI: ///< Decrement address, increment tally (Td = 14)
        
        ///< For each reference to the indirect word, the ADDRESS field is reduced by 1 and the TALLY field is increased by 1 before the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field overflows to 0, the tally runout indicator is set ON, otherwise it is set OFF. The TAG field of the indirect word is ignored. The computed address is the value of the decremented ADDRESS field.
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): restoring continuation '%s'\n", modContSTR(modCont));
        
            Yi = modCont->address;
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): writing operand %012llo to %06o\n", CY, TPR.CA);
        
            Write(i, Yi, CY, OPERAND_STORE, i->a); //TM_IT);
        
            return;
        
        
        case IT_ID: ///< Increment address, decrement tally (Td = 16)
        
            ///< For each reference to the indirect word, the ADDRESS field is increased by 1 and the TALLY field is reduced by 1 after the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF. The TAG field of the indirect word is ignored. The computed address is the value of the unmodified ADDRESS field.
        
        //if (operType == writeCY)

            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): restoring continuation '%s'\n", modContSTR(modCont));
        
            TPR.CA = modCont->address;
        
            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): writing operand %012llo to %06o\n", CY, TPR.CA);
        
            Write(i, TPR.CA, CY, STORE_OPERAND, i->a); //TM_IT);
        
            return;
        
        default:
            //sim_printf("doContinuedAddressContinuation(): How'd we get here (%d)???\n", modCont->mod);
        
            //sim_debug(DBG_ADDRMOD, &cpu_dev, "default: restoring continuation '%s'\n", modContSTR(modCont));
            TPR.CA = modCont->address;
            
            //Write(i, TPR.CA, CY, DataWrite, TM_IT);
            WriteOP(i, TPR.CA, OPERAND_STORE, i->a);    //modCont->mod);
        
            
            switch (OPSIZE(i))
            {
                case 1:
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "default: %s wrote operand %012llo to %05o:%06o\n", opDescSTR(i), CY, TPR.TSR, TPR.CA);
                    break;
                case 2:
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "default: %s wrote operand %012llo to %05o:%06o\n", opDescSTR(i), Ypair[0], TPR.TSR, TPR.CA + 0);
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "         %s               %012llo to %05o:%06o\n", opDescSTR(i), Ypair[1], TPR.TSR, TPR.CA + 1);
                    break;
                case 8:
                    sim_debug(DBG_ADDRMOD, &cpu_dev,     "default: %s wrote operand %012llo to %05o:%06o\n", opDescSTR(i), Yblock8[0], TPR.TSR, TPR.CA + 0);
                    for (int j = 1 ; j < 8 ; j += 1)
                        sim_debug(DBG_ADDRMOD, &cpu_dev, "         %s               %012llo to %05o:%06o\n", opDescSTR(i), Yblock8[j], TPR.TSR, TPR.CA + j);
                    break;
                case 16:
                    sim_debug(DBG_ADDRMOD, &cpu_dev,     "default: %s wrote operand %012llo to %05o:%06o\n", opDescSTR(i), Yblock16[0], TPR.TSR, TPR.CA + 0);
                    for (int j = 1 ; j < 16 ; j += 1)
                        sim_debug(DBG_ADDRMOD, &cpu_dev, "         %s               %012llo to %05o:%06o\n", opDescSTR(i), Yblock16[j], TPR.TSR, TPR.CA + j);
                    break;
            }
            break;
    }
    
}


/*
 * new stuff ...
 */
static word36 itxPair[2];   // a Y-pair for ITS/ITP operations (so we don't have to muck with the real Ypair)

static bool didITSITP = false; ///< true after an ITS/ITP processing

static void doITP(word4 Tag)
{
    sim_debug(DBG_APPENDING, &cpu_dev, "ITP Pair: PRNUM=%o BITNO=%o WORDNO=%o MOD=%o\n", GET_ITP_PRNUM(itxPair), GET_ITP_WORDNO(itxPair), GET_ITP_BITNO(itxPair), GET_ITP_MOD(itxPair));
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
    word3 n = GET_ITP_PRNUM(itxPair);
    TPR.TSR = PR[n].SNR;
    TPR.TRR = max3(PR[n].RNR, SDW->R1, TPR.TRR);
    TPR.TBR = GET_ITP_BITNO(itxPair);
    TPR.CA = SIGNEXT18(PAR[n].WORDNO) + SIGNEXT18(GET_ITP_WORDNO(itxPair));
    if (GET_TM (Tag) == TM_IR)
        TPR.CA += SIGNEXT18(getCr(cu.CT_HOLD));
    else if (GET_TM (Tag) == TM_RI)
    {
        if (GET_ITP_MOD (itxPair) == TM_R || GET_ITP_MOD (itxPair) == TM_RI)
            TPR.CA += SIGNEXT18(getCr(GET_TAG (GET_ITP_MOD (itxPair))));
    }
    rY = TPR.CA;
    
    rTAG = GET_ITP_MOD(itxPair);
    return;
}

static void doITS(word4 Tag)
{
    if ((cpu_dev.dctrl & DBG_APPENDING) && sim_deb)
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "ITS Pair: SEGNO=%o RN=%o WORDNO=%o BITNO=%o MOD=%o\n", GET_ITS_SEGNO(itxPair), GET_ITS_RN(itxPair), GET_ITS_WORDNO(itxPair), GET_ITS_BITNO(itxPair), GET_ITS_MOD(itxPair));
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
    TPR.TSR = GET_ITS_SEGNO(itxPair);
    TPR.TRR = max3(GET_ITS_RN(itxPair), SDW->R1, TPR.TRR);
    TPR.TBR = GET_ITS_BITNO(itxPair);
    //TPR.CA = GET_ITS_WORDNO(itxPair) + getCr(GET_TD(Tag));
    TPR.CA = GET_ITS_WORDNO(itxPair);
    if (GET_TM (Tag) == TM_IR)
        TPR.CA += SIGNEXT18(getCr(cu.CT_HOLD));
    else if (GET_TM (Tag) == TM_RI)
    {
        if (GET_ITS_MOD (itxPair) == TM_R || GET_ITS_MOD (itxPair) == TM_RI)
            TPR.CA += SIGNEXT18(getCr(GET_TAG (GET_ITS_MOD (itxPair))));
    }

    rY = TPR.CA;
    
    rTAG = GET_ITS_MOD(itxPair);
    
    return;
}


static bool doITSITP(DCDstruct *i, word18 address, word36 indword, word6 Tag)
{
    word6 indTag = GET_TAG(indword);
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: indword:%012llo Tag:%o\n", indword, Tag);
    
    if (!((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITP(indword) || ISITS(indword))))
    //if (!(ISITP(indword) || ISITS(indword)))
    {
        sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: returning false\n");
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
//    if ((TPR.CA & 1))
//        // XXX illegal procedure, illegal modifier, fault
//    doFault(i, illproc_fault, ill_mod, "doITSITP() : (TPR.CA & 1)");
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: reading indirect words from %06o\n", address);
    
    // this is probably sooo wrong, but it's a start ...
    itxPair[0] = indword;
    
    Read(i, address + 1, &itxPair[1], INDIRECT_WORD_FETCH, i->a);
    
    sim_debug(DBG_APPENDING, &cpu_dev, "doITS/ITP: YPair= %012llo %012llo\n", itxPair[0], itxPair[1]);
    
    if (ISITS(indTag))
        doITS(Tag);
    else
        doITP(Tag); 
    
    didITSITP = true;
    set_went_appending ();
    return true;
    
}

t_stat doComputedAddressFormation(DCDstruct *i)
{
    word6 Tm = 0;
    word6 Td = 0;
    didITSITP = false;
    eCAFoper operType = prepareCA;  // just for now
    if (RMWOP(i))
        operType = rmwCY;           // r/m/w cycle
    else if (READOP(i))
        operType = readCY;          // read cycle
    else if (WRITEOP(i))
        operType = writeCY;         // write cycle (handled by continuations)
    
    word18 tmp18;
    word36 indword;
        
    int iTAG;   // tag of word preceeding an indirect fetch
    word18 iCA; // CA   "" "" "" ""
        
    directOperandFlag = false;
        
    modCont->bActive = false;   // assume no continuation necessary
        
    if (i->info->flags & NO_TAG) // for instructions line STCA/STCQ
        rTAG = 0;
    else
        rTAG = i->tag;
        
    sim_debug(DBG_ADDRMOD, &cpu_dev, "doComputedAddressFormation(Entry): operType:%s TPR.CA=%06o\n", opDescSTR(i), TPR.CA);
    
startCA:;
        
        Td = GET_TD(rTAG);
        Tm = GET_TM(rTAG);
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "doComputedAddressFormation(startCA): TAG=%02o(%s) Tm=%o Td=%o\n", rTAG, getModString(rTAG), Tm, Td);
    
        switch(Tm)
        {
            case TM_R:
                goto R_MOD;
            case TM_RI:
                goto RI_MOD;
            case TM_IT:
                goto IT_MOD;
            case TM_IR:
                goto IR_MOD;
        }
        sim_printf("doComputedAddressFormation(startCA): unknown Tm??? %o\n", GET_TM(rTAG));
        return SCPE_OK;
        
        
        //! Register modification. Fig 6-3
R_MOD:;
        
        if (Td == 0) // TPR.CA = address from opcode
            goto R_MOD1;
        
        word18 Cr = getCr(Td);  // C(r)
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD: Cr=%06o\n", Cr);
    
        if (directOperandFlag)
        {
            CY = directOperand;
            sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD: directOperand = %012llo\n", CY);

            return SCPE_OK;
        }
        
// possible states
// repeat_first rpt rd    do it?
//       f       f   f      y
//       t       x   x      y
//       f       t   x      n
//       f       x   t      n
//sim_debug (DBG_TRACE, & cpu_dev, "addrmode rf rpt rd tst %d %d %d %d\n", cu.repeat_first,cu.rpt,cu.rd, (! ((! cu . repeat_first) && (cu . rpt || cu . rd))));
        if (! ((! cu . repeat_first) && (cu . rpt || cu . rd)))
        {
            TPR.CA += Cr;
            TPR.CA &= MASK18;   // keep to 18-bits
        }
        else
        {
            sim_debug (DBG_ADDRMOD, & cpu_dev, "R_MOD: rpt special case\n");
        }

        sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD: TPR.CA=%06o\n", TPR.CA);
    
    R_MOD1:;
    
        if (operType == readCY || operType == rmwCY)
        {
            ReadOP(i, TPR.CA, OPERAND_READ, i->a);  // read appropriate operand(s)
          
            sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD1: %s: C(%06o)=%012llo\n", opDescSTR(i), TPR.CA, CY);
        }
    
        if (operType == writeCY || operType == rmwCY)
        {
            modCont->bActive = true;    // will continue the write operation after instruction implementation
            modCont->address = TPR.CA;
            modCont->mod = TM_R;
            modCont->i = i;
            modCont->segment = TPR.TSR;
            
            sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD(operType == %s): saving continuation '%s'\n", opDescSTR(i), modContSTR(modCont));
        }
        
        return SCPE_OK;
        
        //! Figure 6-4. Register Then Indirect Modification Flowchart
    RI_MOD:;
        
        sim_debug(DBG_ADDRMOD, &cpu_dev, "RI_MOD: Td=%o\n", Td);
    
        if (Td == TD_DU || Td == TD_DL) // XXX illegal procedure, illegal modifier, fault
            doFault(i, illproc_fault, ill_mod, "RI_MOD: Td == TD_DU || Td == TD_DL");
        
        if (!Td == 0)
        {
            word18 Cr = getCr(Td);  // C(r)
            
            sim_debug(DBG_ADDRMOD, &cpu_dev, "RI_MOD: Cr=%06o TPR.CA(Before)=%06o\n", Cr, TPR.CA);
            
// possible states
// repeat_first rpt rd    do it?
//       f       f   f      y
//       t       x   x      y
//       f       t   x      n
//       f       x   t      n
//sim_debug (DBG_TRACE, & cpu_dev, "addrmode rf rpt rd tst %d %d %d %d\n", cu.repeat_first,cu.rpt,cu.rd, (! ((! cu . repeat_first) && (cu . rpt || cu . rd))));
            if (! ((! cu . repeat_first) && (cu . rpt || cu . rd)))
            {
                TPR.CA += Cr;
                TPR.CA &= MASK18;
            }
            else
            {
                sim_debug (DBG_ADDRMOD, & cpu_dev, "R_MOD: rpt special case\n");
            }

            sim_debug(DBG_ADDRMOD, &cpu_dev, "RI_MOD: TPR.CA(After)=%06o\n", TPR.CA);
        }
    
        // in case it turns out to be a ITS/ITP
        iCA = TPR.CA;
        iTAG = rTAG;
        
        Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a); //TM_RI);
    
        if (ISITP(indword) || ISITS(indword))
        {
           if (!doITSITP(i, iCA, indword, iTAG))
               return SCPE_UNK;    // some problem with ITS/ITP stuff
           // doITSITP set TPR.CA, rTag, ry
        }
        else
        {
            TPR.CA = GETHI(indword);
            rTAG = GET_TAG(indword);
            rY = TPR.CA;
        }
        
        
#ifndef QUIET_UNUSED
    RI_MOD2:;
#endif
        sim_debug(DBG_ADDRMOD, &cpu_dev, "RI_MOD: indword=%012llo TPR.CA=%06o rTAG=%02o\n", indword, TPR.CA, rTAG);
    
        //if (operType == prepareCA)
        //{
        //CY = directOperand;
        //return;
        //}
        goto startCA;
        
        //! Figure 6-5. Indirect Then Register Modification Flowchart
    IR_MOD:;
        
        cu.CT_HOLD = Td;
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD: CT_HOLD=%o %o\n", cu.CT_HOLD, Td);
    
    IR_MOD_1:
    
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD: fetching indirect word from %06o\n", TPR.CA);
    
        // in case it turns out to be a ITS/ITP
        iCA = TPR.CA;
        iTAG = rTAG;
    
        Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
    
        if (ISITP(indword) || ISITS(indword))
#if 0
            goto IT_MOD;
#else
        {
            if (!doITSITP(i, iCA, indword, iTAG))
               return SCPE_UNK;    // some problem with ITS/ITP stuff
        
            if (operType == prepareCA)
            {
                return SCPE_OK;     // end the indirection chain here
            }
            if (operType == readCY || operType == rmwCY)
            {
                ReadOP(i, TPR.CA, OPERAND_READ, true);
            }
            if (operType == writeCY || operType == rmwCY)
            {
                modCont->bActive = true;    // will continue the write operatio
                modCont->address = TPR.CA;
                modCont->mod = iTAG;    //TM_R;
                modCont->i = i;
                modCont->segment = TPR.TSR;
            }
            return SCPE_OK;
        }
#endif
    
        TPR.CA = GETHI(indword);
        rY = TPR.CA;
        
        Td = GET_TAG(GET_TD(indword));
        Tm = GET_TAG(GET_TM(indword));
        
#ifndef QUIET_UNUSED
    IR_MOD_2:;
#endif
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD1: indword=%012llo TPR.CA=%06o Tm=%o Td=%02o (%s)\n", indword, TPR.CA, Tm, Td, getModString(GET_TAG(indword)));
    
        switch (Tm)
        {
            case TM_IT:
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_IT): Td=%02o => %02o\n", Td, cu.CT_HOLD);
                if (Td == IT_F2 || Td == IT_F3)
                {
                    // Abort. FT2 or 3
                    switch (Td)
                    {
                        case IT_F2:
                            doFault(i, f2_fault, 0, "IT_F2");
                            return SCPE_OK;
                        case IT_F3:
                            doFault(i, f3_fault, 0, "IT_F3");
                            return SCPE_OK;
                    }
                }
                // fall through to TM_R
        
            case TM_R:
                Cr = getCr(cu.CT_HOLD);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R): Cr=%06o\n", Cr);
            
                if (directOperandFlag)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R:directOperandFlag): operand=%012llo\n", directOperand);
                    CY = directOperand;
                
                    return SCPE_OK;
                }
                TPR.CA += Cr;
                TPR.CA &= MASK18;   // keep to 18-bits
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R): TPR.CA=%06o\n", TPR.CA);
            
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    ReadOP(i, TPR.CA, OPERAND_READ, i->a);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R): %s: C(Y)=%012llo\n", opDescSTR(i), CY);
                    
                }
            
                // writes are handled by doAddressContinuation()
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = TPR.CA;
                    modCont->mod = TM_R;
                    modCont->i = i;
                    modCont->segment = TPR.TSR;
                    
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(): saving continuation '%s'\n", modContSTR(modCont));
                }
            
                return SCPE_OK;
            
            case TM_RI:
                Cr = getCr(Td);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_RI): Td=%o Cr=%06o TPR.CA(Before)=%06o\n", Td, Cr, TPR.CA);
            
                TPR.CA += Cr;
                TPR.CA &= MASK18;   // keep to 18-bits
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_RI): TPR.CA(After)=%06o\n", TPR.CA);
            
                goto IR_MOD_1;
            
            case TM_IR:
                goto IR_MOD;
            
        }
        
        sim_printf("doComputedAddressFormation(IR_MOD): unknown Tm??? %o\n", GET_TM(rTAG));
        return SCPE_OK;
        
    IT_MOD:;
        //    IT_SD     = 004,
        //    IT_SCR	= 005,
        //    IT_CI     = 010,
        //    IT_I      = 011,
        //    IT_SC     = 012,
        //    IT_AD     = 013,
        //    IT_DI     = 014,
        //    IT_DIC	= 015,
        //    IT_ID     = 016,
        //    IT_IDC	= 017
        word12 tally;
        word6 idwtag, delta;
        word36 data;
        word24 Yi = -1;
        
        switch (Td)
        {
            // XXX this is probably wrong. ITS/ITP are not standard addr mods .....
            case SPEC_ITP:
            case SPEC_ITS:
            //TODO: insert special rules for abs mode ITS/ITP ...
            
                if ((iCA & 1))
                    // XXX illegal procedure, illegal modifier, fault
                    doFault(i, illproc_fault, ill_mod, "doITSITP() : (TPR.CA & 1)");

                if (!doITSITP(i, iCA, indword, iTAG))
                    return SCPE_UNK;    // some problem with ITS/ITP stuff

                sim_debug((DBG_ADDRMOD | DBG_APPENDING), &cpu_dev, "SPEC_ITS/ITP: TPR.TSR:%06o TPR.CA:%06o\n", TPR.TSR, TPR.CA);

                if (operType == prepareCA)
                {
                    goto startCA;       // interpret possibly more ITS/ITP -or-
                    return SCPE_OK;     // end the indirection chain here?
                }
                if (operType == readCY || operType == rmwCY)
                {
                    sim_debug((DBG_ADDRMOD | DBG_APPENDING), &cpu_dev, "SPEC_ITS/ITP (%s):\n", opDescSTR(i));

                    ReadOP(i, TPR.CA, OPERAND_READ, true);
                    
                    sim_debug((DBG_ADDRMOD | DBG_APPENDING), &cpu_dev, "SPEC_ITS/ITP (%s): Operand contents: %s\n", opDescSTR(i), operandSTR(i));
                }
            
                if (operType == writeCY || operType == rmwCY)
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = TPR.CA;
                    modCont->mod = iTAG;    //TM_R;
                    modCont->i = i;
                    modCont->segment = TPR.TSR;
                    
                    sim_debug((DBG_ADDRMOD | DBG_APPENDING), &cpu_dev, "IT_MOD(SPEC_ITP/SPEC_ITS): saving continuation '%s'\n", modContSTR(modCont));
                }
                goto startCA;
                
                return SCPE_OK;
            
                // check for illegal ITS/ITP
                ///< XXX illegal procedure, illegal modifier, fault
                doFault(i, illproc_fault, ill_mod, "IT_MOD(): illegal procedure, illegal modifier, fault");
                break;
            
            case 2:
                ///< XXX illegal procedure, illegal modifier, fault
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(): illegal procedure, illegal modifier, fault Td=%o\n", Td);
                doFault(i, illproc_fault, ill_mod, "IT_MOD(): illegal procedure, illegal modifier, fault");
                return SCPE_OK;
            
                ///< XXX Abort. FT2 or 3
            case IT_F1:
                doFault(i, f1_fault, 0, "IT_F1");
                return SCPE_OK;
            
            case IT_F2:
                doFault(i, f2_fault, 0, "IT_F2");
                return SCPE_OK;
            case IT_F3:
                doFault(i, f3_fault, 0, "IT_F3");
                return SCPE_OK;
            
            case IT_CI: ///< Character indirect (Td = 10)
            
                ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character/byte position value, cf. Bits 31-32 of the TAG field must be zero.
                ///< If the character position value is greater than 5 for 6-bit characters or greater than 3 for 9- bit bytes, an illegal procedure, illegal modifier, fault will occur. The TALLY field is ignored. The computed address is the value of the ADDRESS field of the indirect word. The effective character/byte number is the value of the character position count, cf, field of the indirect word.
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): reading indirect word from %06o\n", TPR.CA);
            
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);  //TM_IT);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);  //TM_IT);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): indword=%012llo\n", indword);
            
                TPR.CA = GET_ADDR(indword);
                tTB = GET_TB(GET_TAG(indword));
                tCF = GET_CF(GET_TAG(indword));
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): tTB=%o tCF=%o TPR.CA=%06o\n", tTB, tCF, TPR.CA);
            
                if (tTB == TB6 && tCF > 5)
                    // generate an illegal procedure, illegal modifier fault
                    doFault(i, illproc_fault, ill_mod, "tTB == TB6 && tCF > 5");
            
                if (tTB == TB9 && tCF > 3)
                    // generate an illegal procedure, illegal modifier fault
                    doFault(i, illproc_fault, ill_mod, "tTB == TB9 && tCF > 3");
            
            
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    //Read(i, TPR.CA, &CY, DataRead, TM_IT);
                    ReadOP(i, TPR.CA, OPERAND_READ, i->a);
                    if (tTB == TB6)
                        CY = GETCHAR(CY, tCF);
                    else
                        CY = GETBYTE(CY, tCF);
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): read operand from %06o char/byte=%llo\n", TPR.CA, CY);
                }
            
                if (operType == writeCY || operType == rmwCY) //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = TPR.CA;
                    modCont->mod = IT_CI;
                    modCont->i = i;
                    modCont->tb = tTB;
                    modCont->cf = tCF;
                    modCont->segment = TPR.TSR;
                    
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): saving continuation '%s'\n", modContSTR(modCont));
                }
            
                return SCPE_OK;
            
            case IT_SC: ///< Sequence character (Td = 12)
                ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character/byte position counter, cf. Bits 31-32 of the TAG field must be zero.
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): reading indirect word from %06o\n", TPR.CA);
            
                tmp18 = TPR.CA;
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): indword=%012llo\n", indword);
            
                tally = GET_TALLY(indword);    // 12-bits
                idwtag = GET_TAG(indword);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tally=%04o idwtag=%02o\n", tally, idwtag);
            
                //The computed address is the unmodified value of the ADDRESS field. The effective character/byte number is the unmodified value of the character position counter, cf, field of the indirect word.
                tTB = GET_TB(idwtag);
                tCF = GET_CF(idwtag);
                Yi = GETHI(indword);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tTB=%o tCF=%o Yi=%06o\n", tTB, tCF, Yi);
                
                TPR.CA = Yi;
            
                // read data where chars/bytes live
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    Read(i, TPR.CA, &data, OPERAND_READ, i->a);
                
                    switch (tTB)
                    {
                        case TB6:
                            CY = GETCHAR(data, tCF % 6);
                            break;
                        case TB9:
                            CY = GETBYTE(data, tCF % 4);
                            break;
                        default:
                            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): unknown tTB:%o\n", tTB);
                            break;
                    }
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): read operand %012llo from %06o char/byte=%llo\n", data, TPR.CA, CY);
                }
            
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = Yi;
                    modCont->mod = IT_SC;
                    modCont->i = i;
                    modCont->tb = tTB;
                    modCont->cf = tCF;
                    modCont->indword = indword;
                    modCont->segment = TPR.TSR;
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): saving continuation '%s'\n", modContSTR(modCont));
                
                }
                if (operType == prepareCA)
                {
                    // prepareCA shouln't muck about with the tallys, But I don;t think it's ever hit this
                    return SCPE_OK;
                }
            
                // For each reference to the indirect word, the character counter, cf, is increased by 1 and the TALLY field is reduced by 1 after the computed address is formed. Character count arithmetic is modulo 6 for 6-bit characters and modulo 4 for 9-bit bytes. If the character count, cf, overflows to 6 for 6-bit characters or to 4 for 9-bit bytes, it is reset to 0 and ADDRESS is increased by 1. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF.
            
                tCF += 1;
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tCF = %o\n", tCF);
            
                if(((tTB == TB6) && (tCF > 5)) || ((tTB == TB9) && (tCF > 3)))
                {
                    tCF = 0;
                    Yi += 1;
                    Yi &= MASK18;
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): reset tCF. Yi now %06o\n", Yi);
                }
            
                tally -= 1;
                tally &= 07777; // keep to 12-bits
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tally now %o\n", tally);
            
                SCF(tally == 0, cu.IR, I_TALLY);
            
                indword = (word36) (((word36) Yi << 18) | (((word36) tally & 07777) << 6) | tTB | tCF);
                Write(i, tmp18, indword, OPERAND_STORE, i->a);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): wrote tally word %012llo to %06o\n", indword, tmp18);
            
                return SCPE_OK;
            
            case IT_SCR: ///< Sequence character reverse (Td = 5)
                ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character position counter, cf. Bits 31-32 of the TAG field must be zero.
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): reading indirect word from %06o\n", TPR.CA);
            
                tmp18 = TPR.CA;
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): indword=%012llo\n", indword);

                tally = GET_TALLY(indword);    // 12-bits
                idwtag = GET_TAG(indword);
                Yi = GETHI(indword);           // where chars/bytes live
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): tally=%04o idwtag=%02o\n", tally, idwtag);
            
                tTB = GET_TB(idwtag);   //GET_TAG(indword));
                tCF = GET_CF(idwtag);   //GET_TAG(indword));
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): tTB=%o tCF=%o Yi=%06o\n", tTB, tCF, Yi);
            
                //For each reference to the indirect word, the character counter, cf, is reduced by 1 and the TALLY field is increased by 1 before the computed address is formed. Character count arithmetic is modulo 6 for 6-bit characters and modulo 4 for 9-bit bytes. If the character count, cf, underflows to -1, it is reset to 5 for 6-bit characters or to 3 for 9-bit bytes and ADDRESS is reduced by 1. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field overflows to 0, the tally runout indicator is set ON, otherwise it is set OFF. The computed address is the (possibly) decremented value of the ADDRESS field of the indirect word. The effective character/byte number is the decremented value of the character position count, cf, field of the indirect word.
            
                if (tCF == 0)
                {
                    if (tTB == TB6)
                        tCF = 5;
                    else
                        tCF = 3;
                    Yi -= 1;
                    Yi &= MASK18;
                } else
                    tCF -= 1;
            
                tally += 1;
                tally &= 07777; // keep to 12-bits
                SCF(tally == 0, cu.IR, I_TALLY);
            
                //if (operType != prepareCA)
                {
                    // Only update the tally and address if not prepareCA
                    // XXX can this be moved to the continuation?
                    indword = (word36) (((word36) Yi << 18) | (((word36) tally & 07777) << 6) | tTB | tCF);
                    Write(i, tmp18, indword, OPERAND_STORE, i->a);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): wrote tally word %012llo to %06o\n", indword, tmp18);
                }
            
                TPR.CA = Yi;
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    // read data where chars/bytes now live
                
                    Read(i, Yi, &data, OPERAND_READ, i->a);
                
                    switch (tTB)
                    {
                        case TB6:
                            CY = GETCHAR(data, tCF);
                            break;
                        case TB9:
                            CY = GETBYTE(data, tCF);
                            break;
                        default:
                            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): unknown tTB:%o\n", tTB);
                            break;
                    }
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): read operand %012llo from %06o char/byte=%llo\n", data, TPR.CA, CY);
                }
            
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = Yi;
                    modCont->mod = IT_SCR;
                    modCont->i = i;
                    modCont->tb = tTB;
                    modCont->cf = tCF;
                    modCont->indword = indword;
                    modCont->segment = TPR.TSR;
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): saving instruction continuation '%s'\n", modContSTR(modCont));
                }
                return SCPE_OK;
            
            case IT_I: ///< Indirect (Td = 11)
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): reading indirect word from %06o\n", TPR.CA);
            
                Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): indword=%012llo\n", indword);
            
                TPR.CA = GET_ADDR(indword);
            
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    //Read(i, TPR.CA, &CY, DataRead, TM_IT);
                    ReadOP(i, TPR.CA, OPERAND_READ, i->a);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): read operand %012llo from %06o\n", CY, TPR.CA);
                }
            
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = TPR.CA;
                    modCont->mod = IT_I;
                    modCont->i = i;
                    modCont->indword = indword;
                    modCont->segment = TPR.TSR;
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): saving continuation '%s'\n", modContSTR(modCont));
                
                }
                return SCPE_OK;
            
            case IT_AD: ///< Add delta (Td = 13)
                ///< The TAG field of the indirect word is interpreted as a 6-bit, unsigned, positive address increment value, delta. For each reference to the indirect word, the ADDRESS field is increased by delta and the TALLY field is reduced by 1 after the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF. The computed address is the value of the unmodified ADDRESS field of the indirect word.
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): reading indirect word from %06o\n", TPR.CA);
            
                tmp18 = TPR.CA;
                //            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                tally = GET_TALLY(indword); // 12-bits
                delta = GET_DELTA(indword); // 6-bits
                Yi = GETHI(indword);        // from where data live
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): indword=%012llo\n", indword);
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): address:%06o tally:%04o delta:%03o\n", Yi, tally, delta);
            
                TPR.CA = Yi;
                // read data
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    //Read(i, TPR.CA, &CY, DataRead, TM_IT);
                    ReadOP(i, TPR.CA, OPERAND_READ, i->a);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): read operand %012llo from %06o\n", CY, TPR.CA);
                }
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = TPR.CA;
                    modCont->mod = IT_AD;
                    modCont->i = i;
                    modCont->indword = indword;
                    modCont->segment = TPR.TSR;
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): saving continuation '%s'\n", modContSTR(modCont));
                
                }
            
                //else if (operType == prepareCA)
                //{
                //    // prepareCA shouln't muck about with the tallys
                //    return;
                //}
            
                Yi += delta;
                Yi &= MASK18;
            
                tally -= 1;
                tally &= 07777; // keep to 12-bits
                SCF(tally == 0, cu.IR, I_TALLY);
            
                indword = (word36) (((word36) Yi << 18) | (((word36) tally & 07777) << 6) | delta);
                Write(i, tmp18, indword, OPERAND_STORE, i->a);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): wrote tally word %012llo to %06o\n", indword, tmp18);
                return SCPE_OK;
            
            case IT_SD: ///< Subtract delta (Td = 4)
                ///< The TAG field of the indirect word is interpreted as a 6-bit, unsigned, positive address increment value, delta. For each reference to the indirect word, the ADDRESS field is reduced by delta and the TALLY field is increased by 1 before the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field overflows to 0, the tally runout indicator is set ON, otherwise it is set OFF. The computed address is the value of the decremented ADDRESS field of the indirect word.
            
                tmp18 = TPR.CA;
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): reading indirect word from %06o\n", TPR.CA);
                tally = GET_TALLY(indword); // 12-bits
                delta = GET_DELTA(indword); // 6-bits
                Yi    = GETHI(indword);     // from where data live
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): indword=%012llo\n", indword);
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): address:%06o tally:%04o delta:%03o\n", Yi, tally, delta);
            
                Yi -= delta;
                Yi &= MASK18;
                //TPR.CA = Yi;
            
                tally += 1;
                tally &= 07777; // keep to 12-bits
                SCF(tally == 0, cu.IR, I_TALLY);
            
                //if (operType != prepareCA)
                {
                    // Only update the tally and address if not prepareCA
                    // write back out indword
                    indword = (word36) (((word36) Yi << 18) | (((word36) tally & 07777) << 6) | delta);
                    Write(i, tmp18, indword, OPERAND_STORE, 0);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): wrote tally word %012llo to %06o\n", indword, tmp18);
                }
            
                // read data
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    //Read(i, Yi, &CY, DataRead, TM_IT);
                    ReadOP(i, Yi, OPERAND_READ, i->a);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): read operand %012llo from %06o\n", CY, TPR.CA);
                }
            
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = Yi;
                    modCont->mod = IT_SD;
                    modCont->i = i;
                    modCont->indword = indword;
                    modCont->segment = TPR.TSR;
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): saving instruction continuation '%s'\n", modContSTR(modCont));
                
                }
                if (operType == prepareCA)
                    TPR.CA = Yi;
            
                return SCPE_OK;
            
            case IT_DI: ///< Decrement address, increment tally (Td = 14)
            
                ///< For each reference to the indirect word, the ADDRESS field is reduced by 1 and the TALLY field is increased by 1 before the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field overflows to 0, the tally runout indicator is set ON, otherwise it is set OFF. The TAG field of the indirect word is ignored. The computed address is the value of the decremented ADDRESS field.
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): reading indirect word from %06o\n", TPR.CA);
            
                tmp18 = TPR.CA;
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                Yi = GETHI(indword);
                tally = GET_TALLY(indword); // 12-bits
                word6 junk = GET_TAG(indword);    // get tag field, but ignore it
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): indword=%012llo\n", indword);
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): address:%06o tally:%04o\n", Yi, tally);
            
                Yi -= 1;
                Yi &= MASK18;
                //TPR.CA = Yi;
            
                tally += 1;
                tally &= 07777; // keep to 12-bits
                SCF(tally == 0, cu.IR, I_TALLY);
            
                // if (operType != prepareCA)
                {
                    // Only update the tally and address if not prepareCA
                    // write back out indword
                
                    indword = (word36) (((word36) Yi << 18) | ((word36) tally << 6) | junk);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): writing indword=%012llo to addr %06o\n", indword, tmp18);
                    
                    Write(i, tmp18, indword, OPERAND_STORE, i->a);
                }
            
                // read data
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): reading operand from %06o\n", TPR.CA);
                
                    //Read(i, Yi, &CY, DataRead, TM_IT);
                    ReadOP(i, Yi, OPERAND_READ, i->a);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): operand = %012llo\n", CY);
                }
            
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = Yi;
                    modCont->mod = IT_DI;
                    modCont->i = i;
                    modCont->indword = indword;
                    modCont->segment = TPR.TSR;
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): saving continuation '%s'\n", modContSTR(modCont));

                }
                if (operType == prepareCA)
                    TPR.CA = Yi;
            
                return SCPE_OK;
            
            
            case IT_ID: ///< Increment address, decrement tally (Td = 16)
            
                ///< For each reference to the indirect word, the ADDRESS field is increased by 1 and the TALLY field is reduced by 1 after the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF. The TAG field of the indirect word is ignored. The computed address is the value of the unmodified ADDRESS field.
            
                tmp18 = TPR.CA;
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): fetching indirect word from %06o\n", TPR.CA);
            
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                Yi = GETHI(indword);
                tally = GET_TALLY(indword); // 12-bits
                junk = GET_TAG(indword);    // get tag field, but ignore it
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): indword=%012llo Yi=%06o tally=%04o\n", indword, Yi, tally);
            
// XXX Should this be if(prepare)?
                TPR.CA = Yi;
                // read data
                if (operType == readCY || operType == rmwCY) //READOP(i))
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): reading operand from %06o\n", TPR.CA);
                
                    //Read(i, TPR.CA, &CY, DataRead, TM_IT);
                    ReadOP(i, Yi, OPERAND_READ, i->a);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): operand = %012llo\n", CY);
                
                }
                if (operType == writeCY || operType == rmwCY)    //WRITEOP(i))
                {
                    modCont->bActive = true;    // will continue the write operation after instruction implementation
                    modCont->address = TPR.CA;
                    modCont->mod = IT_DI;
                    modCont->i = i;
                    modCont->indword = indword;
                    modCont->tmp18 = tmp18;
                    modCont->segment = TPR.TSR;
                    
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): saving instruction continuation '%s'\n", modContSTR(modCont));
                
                }
                //    else if (operType == prepareCA)
                //    {
                //        // prepareCA shouln't muck about with the tallys
                //        return;
                //    }
            
                Yi += 1;
                Yi &= MASK18;
            
                tally -= 1;
                tally &= 07777; // keep to 12-bits
                SCF(tally == 0, cu.IR, I_TALLY);
            
                // write back out indword
                indword = (word36) (((word36) Yi << 18) | ((word36) tally << 6) | junk);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): writing indword=%012llo to addr %06o\n", indword, tmp18);
            
                Write(i, tmp18, indword, OPERAND_STORE, i->a);
            
                //TPR.CA = Yi;
            
            return SCPE_OK;
            
            case IT_DIC: ///< Decrement address, increment tally, and continue (Td = 15)
            
                ///< The action for this variation is identical to that for the decrement address, increment tally variation except that the TAG field of the indirect word is interpreted and continuation of the indirect chain is possible. If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed .
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DIC): fetching indirect word from %06o\n", TPR.CA);
            
                tmp18 = TPR.CA;
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                Yi = GETHI(indword);
                tally = GET_TALLY(indword); // 12-bits
                idwtag = GET_TAG(indword);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DIC): indword=%012llo Yi=%06o tally=%04o idwtag=%02o\n", indword, Yi, tally, idwtag);
            
                Yi -= 1;
                Yi &= MASK18;
            
                word24 YiSafe2 = Yi; // save indirect address for later use
            
                TPR.CA = Yi;
            
                tally += 1;
                tally &= 07777; // keep to 12-bits
                SCF(tally == 0, cu.IR, I_TALLY);
            
                // if (operType != prepareCA)
                {
                    // Only update the tally and address if not prepareCA
                    // write back out indword
                    indword = (word36) (((word36) Yi << 18) | ((word36) tally << 6) | idwtag);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DIC): writing indword=%012llo to addr %06o\n", indword, tmp18);
                    
                    Write(i, tmp18, indword, OPERAND_STORE, i->a);
                }
            
                // If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed.
            
                //TPR.CA = GETHI(CY);
            
                TPR.CA = YiSafe2;
            
                rTAG = idwtag & 0x70; // force R to 0
                Td = GET_TD(rTAG);
                Tm = GET_TM(rTAG);
            
                //TPR.CA = i->address;
                //rY = TPR.CA;    //i->address;
            
                switch(Tm)
                {
                    case TM_R:
                        goto R_MOD;
                
                    case TM_RI:
                        goto RI_MOD;
                
                    case TM_IR:
                        goto IR_MOD;
                
                    case TM_IT:
                        rTAG = idwtag;
                        Td = GET_TD(rTAG);
                        Tm = GET_TM(rTAG);
                
                        goto IT_MOD;
                }
                sim_printf("IT_DIC: how'd we get here???\n");
                return SCPE_OK;
            
            case IT_IDC: ///< Increment address, decrement tally, and continue (Td = 17)
            
                ///< The action for this variation is identical to that for the increment address, decrement tally variation except that the TAG field of the indirect word is interpreted and continuation of the indirect chain is possible. If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed.
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_IDC): fetching indirect word from %06o\n", TPR.CA);
            
                tmp18 = TPR.CA;
                //Read(i, TPR.CA, &indword, INDIRECT_WORD_FETCH, i->a);
                Read(i, TPR.CA, &indword, OPERAND_READ, i->a);
            
                Yi = GETHI(indword);
                tally = GET_TALLY(indword); // 12-bits
                idwtag = GET_TAG(indword);
            
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_IDC): indword=%012llo Yi=%06o tally=%04o idwtag=%02o\n", indword, Yi, tally, idwtag);
            
                word24 YiSafe = Yi; // save indirect address for later use
            
                Yi += 1;
                Yi &= MASK18;
            
                tally -= 1;
                tally &= 07777; // keep to 12-bits
                SCF(tally == 0, cu.IR, I_TALLY);
            
                //if (operType != prepareCA)
                {
                    // Only update the tally and address if not prepareCA
                    // write back out indword
                    indword = (word36) (((word36) Yi << 18) | ((word36) tally << 6) | idwtag);
                
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_IDC): writing indword=%012llo to addr %06o\n", indword, tmp18);
                
                    Write(i, tmp18, indword, OPERAND_STORE, i->a);
                }
            
                // If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed.
                // But for the dps88 you can use everything but ir/ri.
            
                // force R to 0 (except for IT)
                TPR.CA = YiSafe;
                //TPR.CA = GETHI(indword);
            
                rTAG = idwtag & 0x70; // force R to 0 (except for IT)
                Td = GET_TD(rTAG);
                Tm = GET_TM(rTAG);
            
                //TPR.CA = i->address;
                rY = TPR.CA;    //i->address;
            
                switch(Tm)
                {
                    case TM_R:
                        goto R_MOD;
                
                    case TM_RI:
                        goto RI_MOD;
                
                    case TM_IR:
                        goto IR_MOD;
                
                    case TM_IT:
                        rTAG = GET_TAG(idwtag);
                        Td = _TD(rTAG);
                        Tm = _TM(rTAG);
                
                        goto IT_MOD;
                }
                sim_printf("IT_IDC: how'd we get here???\n");
                return SCPE_OK;
            }

        return SCPE_OK;
}

