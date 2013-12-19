/**
 * \file dps8_addrmods.c
 * \project dps8
 * \author Harry Reed
 * \date 9/25/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include "dps8.h"


// Computed Address Formation Flowcharts

word6 Tm = 0;
word6 Td = 0;
//word6 CT_HOLD = 0;

static bool directOperandFlag = false;
static word36 directOperand = 0;


bool adrTrace = false;   ///< when true do address modifications traceing

static char *strCAFoper(eCAFoper o)
{
    switch (o)
    {
        case unknown:           return "Unknown";
        case readCY:            return "readCY";
        case writeCY:           return "writeCY";
        case prepareCA:         return "prepareCA";
        default:                return "???";
    }
}

/*!
 * return contents of register indicated by Td
 */
word18 getCr(word4 Tdes)
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
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "getCr(TD_DU): rY=%06o directOperand=%012llo\n", rY, directOperand);
            }
            
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
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "getCr(TD_DL): rY=%06o directOperand=%012llo\n", rY, directOperand);
            }
            return 0;
    }
    return 0;
}

/*!
 * do address modifications.
 */
void doComputedAddressFormation(DCDstruct *i, eCAFoper operType) // What about write operands esp in tally IT???
{
    
    word18 tmp18;
    
    directOperandFlag = false;
    //TPR.CA = address;   // address from opcode
    //rY = address;
    
    if (i->iwb->flags & NO_TAG) // for instructions line STCA/STCQ
        rTAG = 0;
    else
        rTAG = i->tag;
    
                         
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "doComputedAddressFormation(Entry): operType:%s(%d) TPR.CA=%06o\n",
                  strCAFoper(operType), operType, TPR.CA);
    }
    
startCA:;
    
    Td = GET_TD(rTAG);
    Tm = GET_TM(rTAG);
    
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "doComputedAddressFormation(startCA): TAG=%02o(%s) Tm=%o Td=%o\n", rTAG, getModString(rTAG), Tm, Td);
    }
    switch(Tm) {
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
    return;
    
    
//! Register modification. Fig 6-3
R_MOD:;
    
    if (Td == 0) // TPR.CA = address from opcode
        goto R_MOD1;
  
    word18 Cr = getCr(Td);  // C(r)

    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD: Cr=%06o\n", Cr);
    }
    
    if (directOperandFlag)
    {
        CY = directOperand;
        return;
    }
    
    TPR.CA += Cr;
    TPR.CA &= MASK18;   // keep to 18-bits

    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD: TPR.CA=%06o\n", TPR.CA);
    }
    
R_MOD1:;
    if (operType == readCY)
    {
        processorCycle = APU_DATA_MOVEMENT; // ???
        Read(i, TPR.CA, &CY, DataRead, TM_R);

        if (adrTrace)
        {
            sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD1: readCY: C(%06o)=%012llo\n", TPR.CA, CY);
        }
    } else if (operType == writeCY)
    {
        Write(i, TPR.CA, CY, DataWrite, TM_R);
        
        if (adrTrace)
        {
            sim_debug(DBG_ADDRMOD, &cpu_dev, "R_MOD1: writeCY: C(%06o)=%012llo\n", TPR.CA, CY);
        }
    }
    return;

//! Figure 6-4. Register Then Indirect Modification Flowchart
RI_MOD:;
    
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "RI_MOD: Td=%o\n", Td);
    }
    
    if (Td == TD_DU || Td == TD_DL)
        // XXX illegal procedure, illegal modifier, fault
        doFault(i, illproc_fault, ill_mod, "Td == TD_DU || Td == TD_DL");

    
    if (!Td == 0)
    {
        word18 Cr = getCr(Td);  // C(r)
    
        if (adrTrace)
        {
            sim_debug(DBG_ADDRMOD, &cpu_dev, "RI_MOD: Cr=%06o TPR.CA(Before)=%06o ", Cr, TPR.CA);
        }
        
        TPR.CA += Cr;
        TPR.CA &= MASK18;
        
        if (adrTrace)
        {
            sim_debug(DBG_ADDRMOD, &cpu_dev, "TPR.CA(After)=%06o\n", TPR.CA);
        }
    }
    
    word36 indword;
    
    processorCycle = INDIRECT_WORD_FETCH;
    Read(i, TPR.CA, &indword, IndirectRead, rTAG); //TM_RI);
    
    TPR.CA = GETHI(indword);
    rTAG = GET_TAG(indword);
    
    rY = TPR.CA;
#ifndef QUIET_UNUSED
RI_MOD2:;
#endif
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "RI_MOD: indword=%012llo TPR.CA=%06o rTAG=%02o\n", indword, TPR.CA, rTAG);
    }
    
    goto startCA;

//! Figure 6-5. Indirect Then Register Modification Flowchart
IR_MOD:;
    
    cu.CT_HOLD = Td;
    
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD: CT_HOLD=%o %o\n", cu.CT_HOLD, Td);
    }
    
IR_MOD_1:
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD: fetching indirect word from %06o\n", TPR.CA);
    }
    
    processorCycle = INDIRECT_WORD_FETCH;
    Read(i, TPR.CA, &indword, IndirectRead, TM_IR);
    
    TPR.CA = GETHI(indword);
    rY = TPR.CA;
    Td = GET_TAG(GET_TD(indword));
    Tm = GET_TAG(GET_TM(indword));
#ifndef QUIET_UNUSED
IR_MOD_2:;
#endif
    if (adrTrace)
    {
        sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD1: indword=%012llo TPR.CA=%06o Tm=%o Td=%02o (%s)\n", indword, TPR.CA, Tm, Td, getModString(GET_TAG(indword)));
    }
    
    switch (Tm)
    {
        case TM_IT:
             
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_IT): Td=%02o => %02o\n", Td, cu.CT_HOLD);
            }
            if (Td == IT_F2 || Td == IT_F3)
            {
                // Abort. FT2 or 3
                switch (Td)
                {
                    case IT_F2:
                        doFault(i, dir_flt2_fault, 0, "IT_F2");
                        break;
                    case IT_F3:
                        doFault(i, dir_flt3_fault, 0, "IT_F3");
                        break;
                }
            }
            // fall through to TM_R
            
        case TM_R:
            Cr = getCr(cu.CT_HOLD);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R): Cr=%06o\n", Cr);
            }
            
            if (directOperandFlag)
            {
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R:directOperandFlag): operand=%012llo\n", directOperand);
                }
                
                CY = directOperand;
                return;
            }
            TPR.CA += Cr;
            TPR.CA &= MASK18;   // keep to 18-bits
    
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R): TPR.CA=%06o\n", TPR.CA);
            }
            
            if (operType == readCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &CY, DataRead, TM_R);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R): readCY: C(Y)=%012llo\n", CY);
                }

            }
            else if (operType == writeCY)
            {
                    processorCycle = APU_DATA_MOVEMENT; // ???
                    Write(i, TPR.CA, CY, DataWrite, TM_R);
                    
                    if (adrTrace)
                    {
                        sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_R): writeCY: C(Y)=%012llo\n", CY);
                    }
                
            }
            return;
            
        case TM_RI:
            Cr = getCr(Td);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_RI): Td=%o Cr=%06o TPR.CA(Before)=%06o ", Td, Cr, TPR.CA);
            }
            
            TPR.CA += Cr;
            TPR.CA &= MASK18;   // keep to 18-bits
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IR_MOD(TM_RI): TPR.CA(After)=%06o\n", TPR.CA);
            }
            
            goto IR_MOD_1;
            
        case TM_IR:
            goto IR_MOD;
            
    }
    sim_printf("doComputedAddressFormation(IR_MOD): unknown Tm??? %o\n", GET_TM(rTAG));
    return;
    
IT_MOD:;
//    IT_SD	= 004,
//    IT_SCR	= 005,
//    IT_CI	= 010,
//    IT_I	= 011,
//    IT_SC	= 012,
//    IT_AD	= 013,
//    IT_DI	= 014,
//    IT_DIC	= 015,
//    IT_ID	= 016,
//    IT_IDC	= 017
    word12 tally;
    word6 idwtag, delta;
    word36 data;
    word24 Yi = -1;
    
    switch (Td)
    {
        // XXX this is probably wrong. ITS/ITP are not standard addr mods .....
        //case SPEC_ITP:
        //case SPEC_ITS:
            //if (doITSITP(IWB, Td))
            //    goto startCA;
            
            ///< XXX illegal procedure, illegal modifier, fault
            //;
            
        case 1:
        case 2:
        case 3:
            ///< XXX illegal procedure, illegal modifier, fault
            ;
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(): illegal procedure, illegal modifier, fault Td=%o\n", Td);
            }
            
        case IT_F1:
        case IT_F2:
        case IT_F3:
            ///< XXX Abort. FT2 or 3
            ;
        case IT_CI: ///< Character indirect (Td = 10)
            
            ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character/byte position value, cf. Bits 31-32 of the TAG field must be zero.
            ///< If the character position value is greater than 5 for 6-bit characters or greater than 3 for 9- bit bytes, an illegal procedure, illegal modifier, fault will occur. The TALLY field is ignored. The computed address is the value of the ADDRESS field of the indirect word. The effective character/byte number is the value of the character position count, cf, field of the indirect word.
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): reading indirect word from %06o\n", TPR.CA);
            }
            
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): indword=%012llo\n", indword);
            }
            
            TPR.CA = GET_ADDR(indword);
            tTB = GET_TB(GET_TAG(indword));
            tCF = GET_CF(GET_TAG(indword));
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): tTB=%o tCF=%o TPR.CA=%06o\n", tTB, tCF, TPR.CA);
            }
            
            if (tTB == TB6 && tCF > 5)
                // generate an illegal procedure, illegal modifier fault
                doFault(i, illproc_fault, ill_mod, "tTB == TB6 && tCF > 5");
            
            if (tTB == TB9 && tCF > 3)
                // generate an illegal procedure, illegal modifier fault
                doFault(i, illproc_fault, ill_mod, "tTB == TB9 && tCF > 3");
            
            
            if (operType == readCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &CY, DataRead, TM_IT);
                if (tTB == TB6)
                    CY = GETCHAR(CY, tCF);
                else
                    CY = GETBYTE(CY, tCF);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): read operand from %06o char/byte=%llo\n", TPR.CA, CY);
                }
            } else if (operType == writeCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                
                // read data where chars/bytes now live
                Read(i, Yi, &data, DataRead, TM_IT);
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): read char/byte %012llo from %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
                }
                
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
                        fprintf(stderr, "IT_MOD(IT_CI): unknown tTB:%o\n", tTB);
                        break;
                        
                }
                
                // write it
                Write(i, Yi, data, DataWrite, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_CI): wrote char/byte %012llo to %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);            }
                }
            
            return;
            
        case IT_SC: ///< Sequence character (Td = 12)
            ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character/byte position counter, cf. Bits 31-32 of the TAG field must be zero.
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): reading indirect word from %06o\n", TPR.CA);
            }
            
            tmp18 = TPR.CA;
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): indword=%012llo\n", indword);
            }
            
            tally = GET_TALLY(indword);    // 12-bits
            idwtag = GET_TAG(indword);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tally=%04o idwtag=%02o\n", tally, idwtag);
            }
            
            //The computed address is the unmodified value of the ADDRESS field. The effective character/byte number is the unmodified value of the character position counter, cf, field of the indirect word.
            tTB = GET_TB(idwtag);
            tCF = GET_CF(idwtag);
            Yi = GETHI(indword);

            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tTB=%o tCF=%o Yi=%06o\n", tTB, tCF, Yi);
            }
            
            TPR.CA = Yi;
            
            // read data where chars/bytes live
            if (operType == readCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &data, DataRead, TM_IT);
                
                switch (tTB)
                {
                    case TB6:
                        CY = GETCHAR(data, tCF % 6);
                        break;
                    case TB9:
                        CY = GETBYTE(data, tCF % 4);
                        break;
                    default:
                        if (adrTrace)
                        {
                            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): unknown tTB:%o\n", tTB);
                        }
                        break;
                }
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): read operand %012llo from %06o char/byte=%llo\n", data, TPR.CA, CY);
                }
            }
            else if (operType == writeCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                
                // read data where chars/bytes now live
                Read(i, Yi, &data, DataRead, TM_IT);
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): read char/byte %012llo from %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
                }
                
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
                        fprintf(stderr, "IT_MOD(IT_SC): unknown tTB:%o\n", tTB);
                        break;
                        
                }
                
                // write it
                Write(i, Yi, data, DataWrite, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): wrote char/byte %012llo to %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
                }
            }

            // For each reference to the indirect word, the character counter, cf, is increased by 1 and the TALLY field is reduced by 1 after the computed address is formed. Character count arithmetic is modulo 6 for 6-bit characters and modulo 4 for 9-bit bytes. If the character count, cf, overflows to 6 for 6-bit characters or to 4 for 9-bit bytes, it is reset to 0 and ADDRESS is increased by 1. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF.
            
            tCF += 1;
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tCF = %o\n", tCF);
            }

            if(((tTB == TB6) && (tCF > 5)) || ((tTB == TB9) && (tCF > 3)))
            {
                tCF = 0;
                Yi += 1;
                Yi &= MASK18;
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): reset tCF. Yi now %06o\n", Yi);
                }

            }
           
            tally -= 1;
            tally &= 07777; // keep to 12-bits
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): tally now %o\n", tally);
            }

            SCF(tally == 0, rIR, I_TALLY);
            
            indword = (word36) ((Yi << 18) | ((tally & 07777) << 6) | tTB | tCF);
            Write(i, tmp18, indword, DataWrite, idwtag);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SC): wrote tally word %012llo to %06o\n", indword, tmp18);
            }

            return;
            
        case IT_SCR: ///< Sequence character reverse (Td = 5)
            ///< Bit 30 of the TAG field of the indirect word is interpreted as a character size flag, tb, with the value 0 indicating 6-bit characters and the value 1 indicating 9-bit bytes. Bits 33-35 of the TAG field are interpreted as a 3-bit character position counter, cf. Bits 31-32 of the TAG field must be zero.

            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): reading indirect word from %06o\n", TPR.CA);
            }

            tmp18 = TPR.CA;
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): indword=%012llo\n", indword);
            }
            
            tally = GET_TALLY(indword);    // 12-bits
            idwtag = GET_TAG(indword);
            Yi = GETHI(indword);           // where chars/bytes live
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): tally=%04o idwtag=%02o\n", tally, idwtag);
            }
            
            tTB = GET_TB(idwtag);   //GET_TAG(indword));
            tCF = GET_CF(idwtag);   //GET_TAG(indword));
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): tTB=%o tCF=%o Yi=%06o\n", tTB, tCF, Yi);
            }

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
            SCF(tally == 0, rIR, I_TALLY);
            
            indword = (word36) ((Yi << 18) | ((tally & 07777) << 6) | tTB | tCF);
            Write(i, tmp18, indword, DataWrite, idwtag);

            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): wrote tally word %012llo to %06o\n", indword, tmp18);
            }
            
            TPR.CA = Yi;
            if (operType == readCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                // read data where chars/bytes now live
                
                Read(i, Yi, &data, DataRead, TM_IT);
                
                switch (tTB)
                {
                    case TB6:
                        CY = GETCHAR(data, tCF);
                        break;
                    case TB9:
                        CY = GETBYTE(data, tCF);
                        break;
                    default:
                        if (adrTrace)
                        {
                            sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): unknown tTB:%o\n", tTB);
                        }
                        break;
                }
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): read operand %012llo from %06o char/byte=%llo\n", data, TPR.CA, CY);
                }
                
            } else if (operType == writeCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                
                // read data where chars/bytes now live
                Read(i, Yi, &data, DataRead, TM_IT);
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): read char/byte %012llo from %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
                }
                
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
                Write(i, Yi, data, DataWrite, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SCR): wrote char/byte %012llo to %06o tTB=%o tCF=%o\n", data, TPR.CA, tTB, tCF);
                }
                
            }
            return;
            
        case IT_I: ///< Indirect (Td = 11)
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): reading indirect word from %06o\n", TPR.CA);
            }

            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): indword=%012llo\n", indword);
            }
            
            TPR.CA = GET_ADDR(indword);
            if (operType == readCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &CY, DataRead, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): read operand %012llo from %06o\n", CY, TPR.CA);
                }
            } else if (operType == writeCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Write(i, TPR.CA, CY, DataWrite, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_I): wrote operand %012llo to %06o\n", CY, TPR.CA);
                }
            }
            return;
            
        case IT_AD: ///< Add delta (Td = 13)
            ///< The TAG field of the indirect word is interpreted as a 6-bit, unsigned, positive address increment value, delta. For each reference to the indirect word, the ADDRESS field is increased by delta and the TALLY field is reduced by 1 after the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF. The computed address is the value of the unmodified ADDRESS field of the indirect word.
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): reading indirect word from %06o\n", TPR.CA);
            }
            
            tmp18 = TPR.CA;
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            tally = GET_TALLY(indword); // 12-bits
            delta = GET_DELTA(indword); // 6-bits
            Yi = GETHI(indword);        // from where data live

            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): indword=%012llo\n", indword);
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): address:%06o tally:%04o delta:%03o\n", Yi, tally, delta);
            }
            
            TPR.CA = Yi;
            // read data
            if (operType == readCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &CY, DataRead, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): read operand %012llo from %06o\n", CY, TPR.CA);
                }
            } else if (operType == writeCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Write(i, TPR.CA, CY, DataWrite, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): wrote operand %012llo to %06o\n", CY, TPR.CA);
                }
            }
            
            Yi += delta;
            Yi &= MASK18;
            
            tally -= 1;
            tally &= 07777; // keep to 12-bits
            SCF(tally == 0, rIR, I_TALLY);
            
            indword = (word36) ((Yi << 18) | ((tally & 07777) << 6) | delta);
            Write(i, tmp18, indword, DataWrite, DataWrite);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_AD): wrote tally word %012llo to %06o\n", indword, tmp18);
            }
            return;
            
        case IT_SD: ///< Subtract delta (Td = 4)            
            ///< The TAG field of the indirect word is interpreted as a 6-bit, unsigned, positive address increment value, delta. For each reference to the indirect word, the ADDRESS field is reduced by delta and the TALLY field is increased by 1 before the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field overflows to 0, the tally runout indicator is set ON, otherwise it is set OFF. The computed address is the value of the decremented ADDRESS field of the indirect word.
            
            tmp18 = TPR.CA;
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): reading indirect word from %06o\n", TPR.CA);
            }
            tally = GET_TALLY(indword); // 12-bits
            delta = GET_DELTA(indword); // 6-bits
            Yi    = GETHI(indword);     // from where data live
 
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): indword=%012llo\n", indword);
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): address:%06o tally:%04o delta:%03o\n", Yi, tally, delta);
            }
            
            Yi -= delta;
            Yi &= MASK18;
            TPR.CA = Yi;
            
            tally += 1;
            tally &= 07777; // keep to 12-bits
            SCF(tally == 0, rIR, I_TALLY);

            // write back out indword
            indword = (word36) ((Yi << 18) | ((tally & 07777) << 6) | delta);
            Write(i, tmp18, indword, DataWrite, 0);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): wrote tally word %012llo to %06o\n", indword, tmp18);
            }
            // read data
            if (operType == readCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &CY, DataRead, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): read operand %012llo from %06o\n", CY, TPR.CA);
                }
            } else if (operType == writeCY)
            {
                processorCycle = APU_DATA_MOVEMENT; // ???
                Write(i, TPR.CA, CY, DataWrite, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_SD): wrote operand %012llo to %06o\n", CY, TPR.CA);
                }
            }


            return;
            
        case IT_DI: ///< Decrement address, increment tally (Td = 14)
            
            ///< For each reference to the indirect word, the ADDRESS field is reduced by 1 and the TALLY field is increased by 1 before the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field overflows to 0, the tally runout indicator is set ON, otherwise it is set OFF. The TAG field of the indirect word is ignored. The computed address is the value of the decremented ADDRESS field.
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): reading indirect word from %06o\n", TPR.CA);
            }
            
            tmp18 = TPR.CA;
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            Yi = GETHI(indword);
            tally = GET_TALLY(indword); // 12-bits
            word6 junk = GET_TAG(indword);    // get tag field, but ignore it
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): indword=%012llo\n", indword);
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): address:%06o tally:%04o\n", Yi, tally);
            }

            Yi -= 1;
            Yi &= MASK18;
            TPR.CA = Yi;
            
            tally += 1;
            tally &= 07777; // keep to 12-bits
            SCF(tally == 0, rIR, I_TALLY);
          
            // write back out indword

            indword = (word36) ((Yi << 18) | (tally << 6) | junk);

            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): writing indword=%012llo to addr %06o\n", indword, tmp18);
            }
            
            Write(i, tmp18, indword, DataWrite, 0);
            
            // read data
            if (operType == readCY)
            {
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): reading operand from %06o\n", TPR.CA);
                }
                
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &CY, DataRead, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): operand = %012llo\n", CY);
                }
                
            } else if (operType == writeCY)
            {
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DI): writing operand %012llo to %06o\n", CY, TPR.CA);
                }
                
                processorCycle = APU_DATA_MOVEMENT; // ???
                Write(i, TPR.CA, CY, DataWrite, TM_IT);
                
            }
        
            return;

            
            
        case IT_ID: ///< Increment address, decrement tally (Td = 16)
            
            ///< For each reference to the indirect word, the ADDRESS field is increased by 1 and the TALLY field is reduced by 1 after the computed address is formed. ADDRESS arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is reduced to 0, the tally runout indicator is set ON, otherwise it is set OFF. The TAG field of the indirect word is ignored. The computed address is the value of the unmodified ADDRESS field.
            
            tmp18 = TPR.CA;

            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): fetching indirect word from %06o\n", TPR.CA);
            }
            
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            

            Yi = GETHI(indword);
            tally = GET_TALLY(indword); // 12-bits
            junk = GET_TAG(indword);    // get tag field, but ignore it
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): indword=%012llo Yi=%06o tally=%04o\n", indword, Yi, tally);
            }
            
            TPR.CA = Yi;
            // read data
            if (operType == readCY)
            {
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): reading operand from %06o\n", TPR.CA);
                }
                
                processorCycle = APU_DATA_MOVEMENT; // ???
                Read(i, TPR.CA, &CY, DataRead, TM_IT);
                
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): operand = %012llo\n", CY);
                }
                
            } else if (operType == writeCY)
            {
                if (adrTrace)
                {
                    sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): writing operand %012llo to %06o\n", CY, TPR.CA);
                }
                
                processorCycle = APU_DATA_MOVEMENT; // ???
                Write(i, TPR.CA, CY, DataWrite, TM_IT);
            }

            
            Yi += 1;
            Yi &= MASK18;
            
            tally -= 1;
            tally &= 07777; // keep to 12-bits
            SCF(tally == 0, rIR, I_TALLY);

            // write back out indword
            indword = (word36) ((Yi << 18) | (tally << 6) | junk);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_ID): writing indword=%012llo to addr %06o\n", indword, tmp18);
            }
            
            Write(i, tmp18, indword, DataWrite, 0);

            return;

        case IT_DIC: ///< Decrement address, increment tally, and continue (Td = 15)
            
            ///< The action for this variation is identical to that for the decrement address, increment tally variation except that the TAG field of the indirect word is interpreted and continuation of the indirect chain is possible. If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed .
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DIC): fetching indirect word from %06o\n", TPR.CA);
            }
            
            tmp18 = TPR.CA;
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            Yi = GETHI(indword);
            tally = GET_TALLY(indword); // 12-bits
            idwtag = GET_TAG(indword);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DIC): indword=%012llo Yi=%06o tally=%04o idwtag=%02o\n", indword, Yi, tally, idwtag);
            }
            
            Yi -= 1;
            Yi &= MASK18;
            TPR.CA = Yi;
            
            tally += 1;
            tally &= 07777; // keep to 12-bits
            SCF(tally == 0, rIR, I_TALLY);
            
            // write back out indword
            indword = (word36) ((Yi << 18) | (tally << 6) | idwtag);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_DIC): writing indword=%012llo to addr %06o\n", indword, tmp18);
            }
            
            Write(i, tmp18, indword, DataWrite, 0);
            
            // If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed.
            
            
            i->address = Yi;
            rTAG = idwtag & 0x60; // force R to 0
            Td = GET_TD(rTAG);
            Tm = GET_TM(rTAG);
            
            TPR.CA = i->address;
            rY = i->address;
            
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
            return;
            
        case IT_IDC: ///< Increment address, decrement tally, and continue (Td = 17)
            
            ///< The action for this variation is identical to that for the increment address, decrement tally variation except that the TAG field of the indirect word is interpreted and continuation of the indirect chain is possible. If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed.
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_IDC): fetching indirect word from %06o\n", TPR.CA);
            }
            
            tmp18 = TPR.CA;
            processorCycle = INDIRECT_WORD_FETCH;
            Read(i, TPR.CA, &indword, IndirectRead, TM_IT);
            
            Yi = GETHI(indword);
            tally = GET_TALLY(indword); // 12-bits
            idwtag = GET_TAG(indword);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_IDC): indword=%012llo Yi=%06o tally=%04o idwtag=%02o\n", indword, Yi, tally, idwtag);
            }
            
            TPR.CA = Yi;
            
            Yi += 1;
            Yi &= MASK18;
            
            tally -= 1;
            tally &= 07777; // keep to 12-bits
            SCF(tally == 0, rIR, I_TALLY);
            
            // write back out indword
            indword = (word36) ((Yi << 18) | (tally << 6) | idwtag);
            
            if (adrTrace)
            {
                sim_debug(DBG_ADDRMOD, &cpu_dev, "IT_MOD(IT_IDC): writing indword=%012llo to addr %06o\n", indword, tmp18);
            }
            
            Write(i, tmp18, indword, DataWrite, TM_IT);
            
            // If the TAG of the indirect word invokes a register, that is, specifies r, ri, or ir modification, the effective Td value for the register is forced to "null" before the next computed address is formed.
            
            // force R to 0 (except for IT)
            i->address = TPR.CA;
            rTAG = idwtag & 0x60; // force R to 0 (except for IT)
            Td = GET_TD(rTAG);
            Tm = GET_TM(rTAG);
            
            TPR.CA = i->address;
            rY = i->address;
            
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
                    Td = _TD(rTAG);
                    Tm = _TM(rTAG);
                    
                    goto IT_MOD;
            }
            sim_printf("IT_IDC: how'd we get here???\n");
            return;
    }
    
    
}
