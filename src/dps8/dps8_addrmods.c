/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

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
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_ins.h"
#include "dps8_utils.h"
#include "dps8_iefp.h"
#include "dps8_opcodetable.h"

// Computed Address Formation Flowcharts

//
// return contents of register indicated by Td
//

static word18 getCr (word4 Tdes)
  {
    cpu.ou.directOperandFlag = false;

    if (Tdes == 0)
      return 0;

    if (Tdes & 010) // Xn
      return cpu.rX [X (Tdes)];

    switch (Tdes)
      {
        case TD_N:  // rY = address from opcode
          return 0;

        case TD_AU: // rY + C(A)0,17
          return GETHI (cpu.rA);

        case TD_QU: // rY + C(Q)0,17
          return GETHI (cpu.rQ);

        case TD_DU: // none; operand has the form y || (00...0)18
          cpu.ou.directOperand = 0;
          SETHI (cpu.ou.directOperand, cpu.rY);
          cpu.ou.directOperandFlag = true;

          sim_debug (DBG_ADDRMOD, & cpu_dev,
                    "getCr(TD_DU): rY=%06o directOperand=%012"PRIo64"\n",
                    cpu.rY, cpu.ou.directOperand);

          return 0;

        case TD_IC: // rY + C (PPR.IC)
            return cpu.PPR.IC;

        case TD_AL: // rY + C(A)18,35
            return GETLO (cpu.rA);

        case TD_QL: // rY + C(Q)18,35
            return GETLO (cpu.rQ);

        case TD_DL: // none; operand has the form (00...0)18 || y
            cpu.ou.directOperand = 0;
            SETLO (cpu.ou.directOperand, cpu.rY);
            cpu.ou.directOperandFlag = true;

            sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "getCr(TD_DL): rY=%06o directOperand=%012"PRIo64"\n",
                       cpu.rY, cpu.ou.directOperand);

            return 0;
      }
    return 0;
  }

// Warning: returns ptr to static buffer.

static char * opDescSTR (void)
  {
    static char temp [256];
    DCDstruct * i = & cpu.currentInstruction;

    strcpy (temp, "");

    if (READOP (i))
      {
        switch (OPSIZE ())
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

            case 32:
              strcat (temp, "readCYblock32");
              break;
          }
      }

    if (WRITEOP (i))
      {
        if (strlen (temp))
          strcat (temp, "/");

        switch (OPSIZE ())
          {
            case 1:
              strcat (temp, "writeCY");
              break;

            case 2:
              strcat (temp, "writeCYpair2");
              break;

            case 8:
              strcat (temp, "writeCYblock8");
              break;

            case 16:
              strcat (temp, "writeCYblock16");
              break;

            case 32:
              strcat (temp, "writeCYblock32");
              break;
          }
      }

    if (TRANSOP (i))
      {
        if (strlen (temp))
          strcat (temp, "/");

        strcat (temp, "prepareCA (TRA)");
      }

    if (! READOP (i) && ! WRITEOP (i) && ! TRANSOP (i) &&
        i -> info -> flags & PREPARE_CA)
      {
        if (strlen (temp))
          strcat (temp, "/");

        strcat (temp, "prepareCA");
      }

    return temp;    //"opDescSTR(???)";
  }

#ifndef QUIET_UNUSED
static char * operandSTR (void)
  {
    DCDstruct * i = & cpu.currentInstruction;
    if (i -> info -> ndes > 0)
      return "operandSTR(): MWEIS not handled yet";

    static char temp [1024];

    int n = OPSIZE ();
    switch (n)
      {
        case 1:
          sprintf (temp, "CY=%012"PRIo64"", cpu.CY);
          break;
        case 2:
          sprintf (temp, "CYpair[0]=%012"PRIo64" CYpair[1]=%012"PRIo64"",
                   cpu.Ypair [0], cpu.Ypair [1]);
          break;
        case 8:
        case 16:
        case 32:
        default:
          sprintf (temp, "Unhandled size: %d", n);
          break;
      }
    return temp;
}
#endif


static void doITP (void)
  {
    sim_debug (DBG_APPENDING, & cpu_dev,
               "ITP Pair: PRNUM=%o BITNO=%o WORDNO=%o MOD=%o\n",
               GET_ITP_PRNUM (cpu.itxPair), GET_ITP_WORDNO (cpu.itxPair),
               GET_ITP_BITNO (cpu.itxPair), GET_ITP_MOD (cpu.itxPair));
    //
    // For n = C(ITP.PRNUM):
    // C(PRn.SNR) -> C(TPR.TSR)
    // maximum of ( C(PRn.RNR), C(SDW.R1), C(TPR.TRR) ) -> C(TPR.TRR)
    // C(ITP.BITNO) -> C(TPR.TBR)
    // C(PRn.WORDNO) + C(ITP.WORDNO) + C(r) -> C(TPR.CA)

    // Notes:
    // 1. r = C(CT-HOLD) if the instruction word or preceding indirect word
    //    specified indirect then register modification, or
    // 2. r = C(ITP.MOD.Td) if the instruction word or preceding indirect word
    //   specified register then indirect modification and ITP.MOD.Tm specifies
    //    either register or register then indirect modification.
    // 3. SDW.R1 is the upper limit of the read/write ring bracket for the
    // segment C(TPR.TSR) (see Section 8).
    //

    word3 n = GET_ITP_PRNUM (cpu.itxPair);
    CPTUR (cptUsePRn + n);
    cpu.TPR.TSR = cpu.PR[n].SNR;
    cpu.TPR.TRR = max3 (cpu.PR[n].RNR, cpu.RSDWH_R1, cpu.TPR.TRR);
    cpu.TPR.TBR = GET_ITP_BITNO (cpu.itxPair);
    cpu.TPR.CA = cpu.PAR[n].WORDNO + GET_ITP_WORDNO (cpu.itxPair);
    cpu.TPR.CA &= AMASK;
    cpu.rY = cpu.TPR.CA;

    cpu.rTAG = GET_ITP_MOD (cpu.itxPair);
    return;
  }

static void doITS (void)
  {
    sim_debug (DBG_APPENDING, & cpu_dev,
               "ITS Pair: SEGNO=%o RN=%o WORDNO=%o BITNO=%o MOD=%o\n",
               GET_ITS_SEGNO (cpu.itxPair), GET_ITS_RN (cpu.itxPair),
               GET_ITS_WORDNO (cpu.itxPair), GET_ITS_BITNO (cpu.itxPair),
               GET_ITS_MOD (cpu.itxPair));
    // C(ITS.SEGNO) -> C(TPR.TSR)
    // maximum of ( C(ITS.RN), C(SDW.R1), C(TPR.TRR) ) -> C(TPR.TRR)
    // C(ITS.BITNO) -> C(TPR.TBR)
    // C(ITS.WORDNO) + C(r) -> C(TPR.CA)
    //
    // 1. r = C(CT-HOLD) if the instruction word or preceding indirect word
    //    specified indirect then register modification, or
    // 2. r = C(ITS.MOD.Td) if the instruction word or preceding indirect word
    //    specified register then indirect modification and ITS.MOD.Tm specifies
    //    either register or register then indirect modification.
    // 3. SDW.R1 is the upper limit of the read/write ring bracket for the
    //    segment C(TPR.TSR) (see Section 8).

    cpu.TPR.TSR = GET_ITS_SEGNO (cpu.itxPair);

    sim_debug (DBG_APPENDING, & cpu_dev,
               "ITS Pair Ring: RN %o RSDWH_R1 %o TRR %o max %o\n",
               GET_ITS_RN (cpu.itxPair), cpu.RSDWH_R1, cpu.TPR.TRR,
               max3 (GET_ITS_RN (cpu.itxPair), cpu.RSDWH_R1, cpu.TPR.TRR));

    cpu.TPR.TRR = max3 (GET_ITS_RN (cpu.itxPair), cpu.RSDWH_R1, cpu.TPR.TRR);
    cpu.TPR.TBR = GET_ITS_BITNO (cpu.itxPair);
    cpu.TPR.CA = GET_ITS_WORDNO (cpu.itxPair);
    cpu.TPR.CA &= AMASK;

    cpu.rY = cpu.TPR.CA;

    cpu.rTAG = GET_ITS_MOD (cpu.itxPair);

    return;
  }


// CANFAULT
static void doITSITP (word6 Tag, word6 * newtag)
  {
    word6 indTag = GET_TAG (cpu.itxPair [0]);

    sim_debug (DBG_APPENDING, & cpu_dev,
               "doITS/ITP: %012"PRIo64" %012"PRIo64" Tag:%o\n",
               cpu.itxPair[0], cpu.itxPair[1], Tag);

    if (! ((GET_TM (Tag) == TM_IR || GET_TM (Tag) == TM_RI) &&
           (ISITP (cpu.itxPair[0]) || ISITS (cpu.itxPair[0]))))
      {
        sim_debug (DBG_APPENDING, & cpu_dev, "doITSITP: faulting\n");
        doFault (FAULT_IPR, (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                 "Incorrect address modifier");
      }

    // Whenever the processor is forming a virtual address two special address
    // modifiers may be specified and are effective under certain restrictive
    // conditions. The special address modifiers are shown in Table 6-4 and
    // discussed in the paragraphs below.
    //
    //  The conditions for which the special address modifiers are effective
    //  are as follows:
    //
    // 1. The instruction word (or preceding indirect word) must specify
    // indirect then register or register then indirect modification.
    //
    // 2. The computed address for the indirect word must be even.
    //
    // If these conditions are satisfied, the processor examines the indirect
    // word TAG field for the special address modifiers.
    //
    // If either condition is violated, the indirect word TAG field is
    // interpreted as a normal address modifier and the presence of a special
    // address modifier will cause an illegal procedure, illegal modifier,
    // fault.

    //cpu.itxPair [0] = indword;

    //Read (address + 1, & cpu.itxPair [1], INDIRECT_WORD_FETCH);

    //sim_debug (DBG_APPENDING, & cpu_dev,
               //"doITS/ITP: YPair= %012"PRIo64" %012"PRIo64"\n",
               //cpu.itxPair [0], cpu.itxPair [1]);

    if (ISITS (indTag))
        doITS ();
    else
        doITP ();

    * newtag = GET_TAG (cpu.itxPair [1]);
    //didITSITP = true;
    set_went_appending ();
  }


void updateIWB (word18 addr, word6 tag)
  {
    word36 * wb;
    if (USE_IRODD)
      wb = & cpu.cu.IRODD;
    else
      wb = & cpu.cu.IWB;
    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "updateIWB: IWB was %012"PRIo64" %06o %s\n",
               * wb, GET_ADDR (* wb),
               extMods [GET_TAG (* wb)].mod);

    putbits36_18 (wb,  0, addr);
    putbits36_6 (wb, 30, tag);
    putbits36_1 (wb, 29,  0);

    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "updateIWB: IWB now %012"PRIo64" %06o %s\n",
               * wb, GET_ADDR (* wb),
               extMods [GET_TAG (* wb)].mod);

    decodeInstruction (IWB_IRODD, & cpu.currentInstruction);
  }

//
// Input:
//   cu.IWB
//   currentInstruction
//   TPR.TSR
//   TPR.TRR
//   TPR.TBR // XXX check to see if this is initialized
//   TPR.CA
//
//  Output:
//   TPR.CA
//   directOperandFlag
//
// CANFAULT

t_stat doComputedAddressFormation (void)
  {
    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "%s(Entry): operType:%s TPR.CA=%06o\n",
                __func__, opDescSTR (), cpu.TPR.CA);
    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "%s(Entry): CT_HOLD %o\n",
                __func__, cpu.cu.CT_HOLD);

    DCDstruct * i = & cpu.currentInstruction;

    word6 Tm = 0;
    word6 Td = 0;

    word6 iTAG;   // tag of word preceeding an indirect fetch

    cpu.ou.directOperandFlag = false;

    if (i -> info -> flags & NO_TAG) // for instructions line STCA/STCQ
      cpu.rTAG = 0;
    else
      cpu.rTAG = GET_TAG (IWB_IRODD);

    int lockupCnt = 0;
#define lockupLimit 4096 // approx. 2 ms

startCA:;

    if (++ lockupCnt > lockupLimit)
      {
        doFault (FAULT_LUF, (_fault_subtype) {.bits=0},
                 "Lockup in addrmod");
      }

    Td = GET_TD (cpu.rTAG);
    Tm = GET_TM (cpu.rTAG);

    // CT_HOLD is set to 0 on instruction setup; if it is non-zero here,
    // we must have faulted during an indirect word fetch. Restore 
    // state and restart the fetch.
    if (cpu.cu.CT_HOLD)
      {
//if (currentRunningCPUnum) sim_printf ("restart CT_HOLD %o\n", cpu.cu.CT_HOLD);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "%s(startCA): IR mode restart; CT_HOLD %02o\n",
                   __func__, cpu.cu.CT_HOLD);
        cpu.rTAG = cpu.cu.CT_HOLD;
        Td = GET_TD (cpu.rTAG);
        Tm = GET_TM (cpu.rTAG);
        goto IR_MOD_1;
      }
    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "%s(startCA): TAG=%02o(%s) Tm=%o Td=%o\n",
               __func__, cpu.rTAG, getModString (cpu.rTAG), Tm, Td);

    switch (Tm)
      {
        case TM_R:
          goto R_MOD;
        case TM_RI:
          goto RI_MOD;
        case TM_IT:
          goto IT_MOD;
        case TM_IR:
          goto IR_MOD;
        default:
          break;
      }

    sim_printf ("%s(startCA): unknown Tm??? %o\n",
                __func__, GET_TM (cpu.rTAG));
    sim_warn ("(startCA): unknown Tmi; can't happen!\n");
    return SCPE_OK;


    // Register modification. Fig 6-3

    R_MOD:;
      {
        if (Td == 0) // TPR.CA = address from opcode
          {
            //updateIWB (identity) // known that Td is 0.
            return SCPE_OK;
          }

        word18 Cr = getCr (Td);

        sim_debug (DBG_ADDRMOD, & cpu_dev, "R_MOD: Cr=%06o\n", Cr);

        if (cpu.ou.directOperandFlag)
          {
            sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "R_MOD: directOperand = %012"PRIo64"\n",
                       cpu.ou.directOperand);

            //cpu.TPR.CA = cpu.ou.directOperand;
            //updateIWB (identity) // known that rTag is DL or DU
            return SCPE_OK;
          }

        // For the case of RPT/RPD, the instruction decoder has
        // verified that Tm is R or RI, and Td is X1..X7.
        if (cpu.cu.rpt || cpu.cu.rd | cpu.cu.rl)
          {
#if 0 // executeInstruction has already cleared PRNO
            //if (cpu.isb29)
            if (ISB29)
              {
                word3 PRn = (i -> address >> 15) & MASK3;
                CPTUR (cptUsePRn + PRn);
                cpu.TPR.CA = (Cr & MASK15) + cpu.PR [PRn].WORDNO;
                cpu.TPR.CA &= AMASK;
              }
            else
              {
                cpu.TPR.CA = Cr;
              }
#else
            //if (cpu.isb29)
            if (ISB29)
              {
                word3 PRn = cpu.cu.TSN_PRNO[0];
                CPTUR (cptUsePRn + PRn);
                cpu.TPR.CA = (Cr & MASK15) + cpu.PR [PRn].WORDNO;
                cpu.TPR.CA &= AMASK;
              }
            else
              {
                cpu.TPR.CA = Cr;
              }
#endif
          }
        else
          {
            cpu.TPR.CA += Cr;
            cpu.TPR.CA &= MASK18;   // keep to 18-bits
          }
        sim_debug (DBG_ADDRMOD, & cpu_dev, "R_MOD: TPR.CA=%06o\n",
                   cpu.TPR.CA);

        // If repeat, the indirection chain is limited, so it is not necessary
        // to clear the tag; the delta code later on needs the tag to know
        // which X register to update
        if (! (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl))
          updateIWB (cpu.TPR.CA, 0); // Known to be 0 or ,n
        return SCPE_OK;
      } // R_MOD


    // Figure 6-4. Register Then Indirect Modification Flowchart

    RI_MOD:;
      {
        sim_debug (DBG_ADDRMOD, & cpu_dev, "RI_MOD: Td=%o\n", Td);

        if (Td == TD_DU || Td == TD_DL)
          doFault (FAULT_IPR, (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                   "RI_MOD: Td == TD_DU || Td == TD_DL");

#ifdef OLD_CYCLE
        word18 tmpCA = cpu.TPR.CA;
#endif

        if (Td != 0)
          {
            word18 Cr = getCr (Td);  // C(r)

            // We don''t need to worry about direct operand here, since du
            // and dl are disallowed above

#ifdef OLDCYCLE
            sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "RI_MOD: Cr=%06o tmpCA(Before)=%06o\n", Cr, tmpCA);
#else
            sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "RI_MOD: Cr=%06o CA(Before)=%06o\n", Cr, cpu.TPR.CA);
#endif

            if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
              {
                 word6 Td_ = GET_TD (i -> tag);
                 uint Xn = X (Td_);  // Get Xn of next instruction
#ifdef OLDCYCLE
                 tmpCA = cpu.rX [Xn];
#else
                 cpu.TPR.CA = cpu.rX [Xn];
#endif
              }
            else
              {
#ifdef OLDCYCLE
                tmpCA += Cr;
                tmpCA &= MASK18;   // keep to 18-bits
#else
                cpu.TPR.CA += Cr;
                cpu.TPR.CA &= MASK18;   // keep to 18-bits
#endif
              }
#ifdef OLDCYCLE
            sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "RI_MOD: tmpCA(After)=%06o\n", tmpCA);
#else
            sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "RI_MOD: CA(After)=%06o\n", cpu.TPR.CA);
#endif
          }

        // If the indirect word faults, on restart the CA will be the post
        // register modification value, so we want to prevent it from 
        // happening again on restart

#ifdef OLDCYCLE
        updateIWB (cpu.TPR.CA, TM_RI | TD_N);
#endif

        // in case it turns out to be a ITS/ITP
        iTAG = cpu.rTAG;

#ifdef OLDCYCLE
        Read2 (tmpCA, cpu.itxPair, INDIRECT_WORD_FETCH); //TM_RI);
#else
        //Read2 (cpu.TPR.CA, cpu.itxPair, INDIRECT_WORD_FETCH); //TM_RI);
        ReadIndirect ();
#endif

        // "In the case of RI modification, only one indirect reference is made
        // per repeated execution. The TAG field of the indirect word is not
        // interpreted.  The indirect word is treated as though it had R
        // modification with R = N."

        if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
          {
             cpu.itxPair[0] &= ~ INST_M_TAG;
             cpu.itxPair[0] |= TM_R | GET_TD (iTAG);
           }

        // (Closed) Ticket 15: Check for fault causing tags before updating
        // the IWB, so the instruction restart will reload the offending
        // indirect word.

        if (GET_TM (GET_TAG (cpu.itxPair[0])) == TM_IT)
          {
            if (GET_TD (GET_TAG (cpu.itxPair[0])) == IT_F2)
              {
#ifdef OLDCYCLE
                cpu.TPR.CA = tmpCA;
#endif
                doFault (FAULT_F2, (_fault_subtype) {.bits=0},
                         "RI_MOD: IT_F2 (0)");
              }
            if (GET_TD (GET_TAG (cpu.itxPair[0])) == IT_F3)
              {
#ifdef OLDCYCLE
                cpu.TPR.CA = tmpCA;
#endif
                doFault (FAULT_F3, (_fault_subtype) {.bits=0},
                         "RI_MOD: IT_F3");
              }
          }

        if (ISITP (cpu.itxPair[0]) || ISITS (cpu.itxPair[0]))
          {
#ifdef OLDCYCLE
            doITSITP (iTAG, & cpu.rTAG);
#else
            doITSITP (iTAG, & cpu.rTAG);
#endif
          }
        else
          {
            cpu.TPR.CA = GETHI (cpu.itxPair[0]);
            cpu.rTAG = GET_TAG (cpu.itxPair[0]);
            cpu.rY = cpu.TPR.CA;
          }

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "RI_MOD: cpu.itxPair[0]=%012"PRIo64" TPR.CA=%06o rTAG=%02o\n",
                   cpu.itxPair[0], cpu.TPR.CA, cpu.rTAG);
        // If repeat, the indirection chain is limited, so it is not needed
        // to clear the tag; the delta code later on needs the tag to know
        // which X register to update
        if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
          return SCPE_OK;

#ifdef OLDCYCLE
        updateIWB (cpu.TPR.CA, cpu.rTAG);
#endif
        goto startCA;
      } // RI_MOD

    // Figure 6-5. Indirect Then Register Modification Flowchart
    IR_MOD:;
      {
        cpu.cu.CT_HOLD = cpu.rTAG;

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "IR_MOD: CT_HOLD=%o %o\n", cpu.cu.CT_HOLD, Td);

        IR_MOD_1:;

        if (++ lockupCnt > lockupLimit)
          {
            doFault (FAULT_LUF, (_fault_subtype) {.bits=0},
                     "Lockup in addrmod IR mode");
          }

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "IR_MOD: fetching indirect word from %06o\n",
                   cpu.TPR.CA);

        // in case it turns out to be a ITS/ITP
        iTAG = cpu.rTAG;

        word18 saveCA = cpu.TPR.CA;
        //Read2 (cpu.TPR.CA, cpu.itxPair, INDIRECT_WORD_FETCH);
        ReadIndirect ();

        if (ISITP (cpu.itxPair[0]) || ISITS (cpu.itxPair[0]))
          {
            doITSITP (iTAG, & cpu.rTAG);
          }
        else
          {
            cpu.TPR.CA = GETHI (cpu.itxPair[0]);
            cpu.rY = cpu.TPR.CA;
            cpu.rTAG = GET_TAG (cpu.itxPair[0]);
          }

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "IR_MOD: CT_HOLD=%o\n", cpu.cu.CT_HOLD);
        Td = GET_TD (cpu.rTAG);
        Tm = GET_TM (cpu.rTAG);

        // IR_MOD_2:;

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "IR_MOD1: cpu.itxPair[0]=%012"PRIo64
                   " TPR.CA=%06o Tm=%o Td=%02o (%s)\n",
                   cpu.itxPair[0], cpu.TPR.CA, Tm, Td,
                   getModString (GET_TAG (cpu.itxPair[0])));

        switch (Tm)
          {
            case TM_IT:
              {
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IR_MOD(TM_IT): Td=%02o => %02o\n",
                           Td, cpu.cu.CT_HOLD);

                if (Td == IT_F2 || Td == IT_F3)
                  {
                    // Abort. FT2 or 3
                    switch (Td)
                      {
                        case IT_F2:
                          cpu.TPR.CA = saveCA;
                          doFault (FAULT_F2, (_fault_subtype) {.bits=0},
                                   "TM_IT: IT_F2 (1)");

                        case IT_F3:
                          cpu.TPR.CA = saveCA;
                          doFault (FAULT_F3, (_fault_subtype) {.bits=0},
                                   "TM_IT: IT_F3");
                      }
                  }
                // fall through to TM_R
                } // TM_IT

            case TM_R:
              {
                word18 Cr = getCr (GET_TD (cpu.cu.CT_HOLD));

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IR_MOD(TM_R): CT_HOLD %o Cr=%06o\n",
                           GET_TD (cpu.cu.CT_HOLD), Cr);

                if (cpu.ou.directOperandFlag)
                  {
                    sim_debug (DBG_ADDRMOD, & cpu_dev,
                               "IR_MOD(TM_R): CT_HOLD DO %012"PRIo64"\n",
                               cpu.ou.directOperand);
                    // CT_HOLD has *DU or *DL; convert to DU or DL
                    word6 tag = TM_R | GET_TD (cpu.cu.CT_HOLD);
                    updateIWB (cpu.TPR.CA, tag); // Known to be DL or DU
                  }
                else
                  {
                    cpu.TPR.CA += Cr;
                    cpu.TPR.CA &= MASK18;   // keep to 18-bits

                    sim_debug (DBG_ADDRMOD, & cpu_dev,
                               "IR_MOD(TM_R): TPR.CA=%06o\n", cpu.TPR.CA);

#ifdef OLDCYCLE
                    updateIWB (cpu.TPR.CA, 0);
#endif
                  }
                cpu.cu.CT_HOLD = 0;
                return SCPE_OK;
              } // TM_R

            case TM_RI:
              {
//if (currentRunningCPUnum) sim_printf ("TM_RI\n");
                word18 Cr = getCr (Td);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IR_MOD(TM_RI): Td=%o Cr=%06o TPR.CA(Before)=%06o\n",
                           Td, Cr, cpu.TPR.CA);

                if (cpu.ou.directOperandFlag)
                  {
                    // keep to 18-bits
                    cpu.TPR.CA = (word18) cpu.ou.directOperand & MASK18;

                    sim_debug (DBG_ADDRMOD, & cpu_dev,
                               "IR_MOD(TM_RI): DO TPR.CA=%06o\n",
                               cpu.TPR.CA);
                  }
                else
                  {
                    cpu.TPR.CA += Cr;
                    cpu.TPR.CA &= MASK18;   // keep to 18-bits

                    sim_debug (DBG_ADDRMOD, & cpu_dev,
                               "IR_MOD(TM_RI): TPR.CA=%06o\n", cpu.TPR.CA);

                  }

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IR_MOD(TM_RI): TPR.CA(After)=%06o\n",
                           cpu.TPR.CA);

                updateIWB (cpu.TPR.CA, cpu.rTAG); // XXX guessing here...
                goto IR_MOD_1;
              } // TM_RI

            case TM_IR:
              {
#ifdef OLDCYCLE
                updateIWB (cpu.TPR.CA, cpu.rTAG); // XXX guessing here...
#endif
                goto IR_MOD;
              } // TM_IR
          } // Tm

        sim_printf ("%s(IR_MOD): unknown Tm??? %o\n", 
                    __func__, GET_TM (cpu.rTAG));
        return SCPE_OK;
      } // IR_MOD

    IT_MOD:;
      {
        //    IT_SD     = 004,
        //    IT_SCR        = 005,
        //    IT_CI     = 010,
        //    IT_I      = 011,
        //    IT_SC     = 012,
        //    IT_AD     = 013,
        //    IT_DI     = 014,
        //    IT_DIC        = 015,
        //    IT_ID     = 016,
        //    IT_IDC        = 017
        word6 idwtag, delta;
        word24 Yi = (word24) -1;

        switch (Td)
          {
            // XXX this is probably wrong. ITS/ITP are not standard addr mods
            case SPEC_ITP:
            case SPEC_ITS:
              {
                doFault (FAULT_IPR,
                         (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                         "ITx in IT_MOD)");
              }

            case 2:
              {
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(): illegal procedure, illegal modifier, "
                           "fault Td=%o\n", Td);
                doFault (FAULT_IPR,
                         (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                         "IT_MOD(): illegal procedure, illegal modifier, "
                         "fault");
              }

            case IT_F1:
              {
                doFault (FAULT_F1, (_fault_subtype) {.bits=0},
                         "IT_MOD: IT_F1");
              }

            case IT_F2:
              {
                doFault (FAULT_F2, (_fault_subtype) {.bits=0},
                         "IT_MOD: IT_F2 (2)");
              }

            case IT_F3:
              {
                doFault (FAULT_F3, (_fault_subtype) {.bits=0},
                         "IT_MOD: IT_F3");
              }


            case IT_CI:  // Character indirect (Td = 10)
            case IT_SC:  // Sequence character (Td = 12)
            case IT_SCR: // Sequence character reverse (Td = 5)
              {
	      // There is complexity with managing page faults and tracking
	      // the indirect word address and the operand address.
                //
	      // To address this, we force those pages in during PREPARE_CA,
	      // so that they won't fault during operand read/write.

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD CI/SC/SCR reading indirect word from %06o\n",
                           cpu.TPR.CA);

                //
                // Get the indirect word
                //

                word36 indword;
                word18 indwordAddress = cpu.TPR.CA;
                Read (indwordAddress, & indword, OPERAND_READ);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD CI/SC/SCR indword=%012"PRIo64"\n", indword);

                //
                // Parse and validate the indirect word
                //

                word18 Yi_ = GET_ADDR (indword);
                cpu.ou.characterOperandSize = GET_TB (GET_TAG (indword));
                cpu.ou.characterOperandOffset = GET_CF (GET_TAG (indword));

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD CI/SC/SCR size=%o offset=%o Yi=%06o\n",
                           cpu.ou.characterOperandSize,
                           cpu.ou.characterOperandOffset,
                           Yi_);

                if (cpu.ou.characterOperandSize == TB6 &&
                    cpu.ou.characterOperandOffset > 5)
                  // generate an illegal procedure, illegal modifier fault
                  doFault (FAULT_IPR,
                           (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                           "co size == TB6 && offset > 5");

                if (cpu.ou.characterOperandSize == TB9 &&
                    cpu.ou.characterOperandOffset > 3)
                  // generate an illegal procedure, illegal modifier fault
                  doFault (FAULT_IPR,
                           (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                           "co size == TB9 && offset > 3");

                // CI uses the address, and SC uses the pre-increment address;
                // but SCR use the post-decrement address
                if (Td == IT_SCR)
                  {
		// For each reference to the indirect word, the character
		// counter, cf, is reduced by 1 and the TALLY field is
		// increased by 1 before the computed address is formed.
                    //
		// Character count arithmetic is modulo 6 for 6-bit
		// characters and modulo 4 for 9-bit bytes. If the
		// character count, cf, underflows to -1, it is reset to 5
		// for 6-bit characters or to 3 for 9-bit bytes and ADDRESS
		// is reduced by 1. ADDRESS arithmetic is modulo 2^18.
		// TALLY arithmetic is modulo 4096.
                    //
		// If the TALLY field overflows to 0, the tally runout
		// indicator is set ON, otherwise it is set OFF. The
		// computed address is the (possibly) decremented value of
		// the ADDRESS field of the indirect word. The effective
		// character/byte number is the decremented value of the
		// character position count, cf, field of the indirect
		// word.

                    if (cpu.ou.characterOperandOffset == 0)
                      {
                        if (cpu.ou.characterOperandSize == TB6)
                            cpu.ou.characterOperandOffset = 5;
                        else
                            cpu.ou.characterOperandOffset = 3;
                        Yi_ -= 1;
                        Yi_ &= MASK18;
                      }
                        else
                      {
                        cpu.ou.characterOperandOffset -= 1;
                      }
                  }

                //
                // Get the data word
                //

                cpu.cu.pot = 1;

                word36 data;
                Read (Yi_, & data, OPERAND_READ);
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "IT_MOD CI/SC/SCR data=%012"PRIo64"\n", data);

                cpu.cu.pot = 0;

                //
                // Restore the CA to point to the indirect word
                //

                cpu.TPR.CA =  indwordAddress;

                return SCPE_OK;
              } // IT_CI, IT_SC, IT_SCR
   
            case IT_I: // Indirect (Td = 11)
              {
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_I): reading indirect word from %06o\n",
                           cpu.TPR.CA);

{ static bool first = true;
if (first) {
first = false;
sim_printf ("XXX this had b29 of 0; it may be necessary to clear TSN_VALID[0]\n");
}}
                //Read2 (cpu.TPR.CA, cpu.itxPair, INDIRECT_WORD_FETCH);
                ReadIndirect ();

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_I): indword=%012"PRIo64"\n",
                           cpu.itxPair[0]);

                cpu.TPR.CA = GET_ADDR (cpu.itxPair[0]);

                updateIWB (cpu.TPR.CA, 0);

                return SCPE_OK;
              } // IT_I

            case IT_AD: ///< Add delta (Td = 13)
              {
                // The TAG field of the indirect word is interpreted as a
                // 6-bit, unsigned, positive address increment value, delta.
                // For each reference to the indirect word, the ADDRESS field
                // is increased by delta and the TALLY field is reduced by 1
                // after the computed address is formed. ADDRESS arithmetic is
                // modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY
                // field is reduced to 0, the tally runout indicator is set ON,
                // otherwise it is set OFF. The computed address is the value
                // of the unmodified ADDRESS field of the indirect word.

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_AD): reading indirect word from %06o\n",
                           cpu.TPR.CA);

                word18 saveCA = cpu.TPR.CA;
                word36 indword;
                Read (cpu.TPR.CA, & indword, OPERAND_READ);

                cpu.AM_tally = GET_TALLY (indword); // 12-bits
                delta = GET_DELTA (indword); // 6-bits
                Yi = GETHI (indword);        // from where data live

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_AD): indword=%012"PRIo64"\n",
                           indword);
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_AD): address:%06o tally:%04o "
                           "delta:%03o\n",
                           Yi, cpu.AM_tally, delta);

                cpu.TPR.CA = Yi;
                word18 computedAddress = cpu.TPR.CA;

                Yi += delta;
                Yi &= MASK18;

                cpu.AM_tally -= 1;
                cpu.AM_tally &= 07777; // keep to 12-bits
                // breaks emacs
                //if (cpu.AM_tally == 0)
                  //SET_I_TALLY;
                SC_I_TALLY (cpu.AM_tally == 0);
                indword = (word36) (((word36) Yi << 18) |
                                    (((word36) cpu.AM_tally & 07777) << 6) |
                                    delta);
                Write (saveCA, indword, OPERAND_STORE);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_AD): wrote tally word %012"PRIo64
                           " to %06o\n",
                           indword, saveCA);

                cpu.TPR.CA = computedAddress;

                updateIWB (cpu.TPR.CA, 0); // XXX guessing here...

                return SCPE_OK;
              } // IT_AD

            case IT_SD: ///< Subtract delta (Td = 4)
              {
                // The TAG field of the indirect word is interpreted as a
                // 6-bit, unsigned, positive address increment value, delta.
                // For each reference to the indirect word, the ADDRESS field
                // is reduced by delta and the TALLY field is increased by 1
                // before the computed address is formed. ADDRESS arithmetic is
                // modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY
                // field overflows to 0, the tally runout indicator is set ON,
                // otherwise it is set OFF. The computed address is the value
                // of the decremented ADDRESS field of the indirect word.

                word18 saveCA = cpu.TPR.CA;
                word36 indword;
                Read (cpu.TPR.CA, & indword, OPERAND_READ);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_SD): reading indirect word from %06o\n",
                           cpu.TPR.CA);
                cpu.AM_tally = GET_TALLY (indword); // 12-bits
                delta = GET_DELTA (indword); // 6-bits
                Yi    = GETHI (indword);     // from where data live

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_SD): indword=%012"PRIo64"\n",
                           indword);
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_SD): address:%06o tally:%04o "
                           "delta:%03o\n",
                           Yi, cpu.AM_tally, delta);

                Yi -= delta;
                Yi &= MASK18;
                cpu.TPR.CA = Yi;

                cpu.AM_tally += 1;
                cpu.AM_tally &= 07777; // keep to 12-bits
                //SC_I_TALLY (cpu.AM_tally == 0);
                if (cpu.AM_tally == 0)
                  SET_I_TALLY;

                // write back out indword
                indword = (word36) (((word36) Yi << 18) |
                                    (((word36) cpu.AM_tally & 07777) << 6) |
                                    delta);
                Write (saveCA, indword, OPERAND_STORE);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_SD): wrote tally word %012"PRIo64
                           " to %06o\n",
                           indword, saveCA);


                cpu.TPR.CA = Yi;
                updateIWB (cpu.TPR.CA, 0); // XXX guessing here...

                return SCPE_OK;
              } // IT_SD

            case IT_DI: ///< Decrement address, increment tally (Td = 14)
              {
                // For each reference to the indirect word, the ADDRESS field
                // is reduced by 1 and the TALLY field is increased by 1 before
                // the computed address is formed. ADDRESS arithmetic is modulo
                // 2^18. TALLY arithmetic is modulo 4096. If the TALLY field
                // overflows to 0, the tally runout indicator is set ON,
                // otherwise it is set OFF. The TAG field of the indirect word
                // is ignored. The computed address is the value of the
                // decremented ADDRESS field.

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_DI): reading indirect word from %06o\n",
                           cpu.TPR.CA);

                word18 saveCA = cpu.TPR.CA;
                word36 indword;
                Read (cpu.TPR.CA, & indword, OPERAND_READ);

                Yi = GETHI (indword);
                cpu.AM_tally = GET_TALLY (indword); // 12-bits
                word6 junk = GET_TAG (indword); // get tag field, but ignore it

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_DI): indword=%012"PRIo64"\n",
                           indword);
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_DI): address:%06o tally:%04o\n",
                           Yi, cpu.AM_tally);

                Yi -= 1;
                Yi &= MASK18;
                cpu.TPR.CA = Yi;

                cpu.AM_tally += 1;
                cpu.AM_tally &= 07777; // keep to 12-bits
                SC_I_TALLY (cpu.AM_tally == 0);
                //if (cpu.AM_tally == 0)
                  //SET_I_TALLY;

                // write back out indword

                indword = (word36) (((word36) cpu.TPR.CA << 18) |
                                    ((word36) cpu.AM_tally << 6) |
                                    junk);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_DI): writing indword=%012"PRIo64" to "
                           "addr %06o\n",
                           indword, saveCA);

                Write (saveCA, indword, OPERAND_STORE);

                cpu.TPR.CA = Yi;
                updateIWB (cpu.TPR.CA, 0); // XXX guessing here...

                return SCPE_OK;
              } // IT_DI

            case IT_ID: ///< Increment address, decrement tally (Td = 16)
              {
                // For each reference to the indirect word, the ADDRESS field
                // is increased by 1 and the TALLY field is reduced by 1 after
                // the computed address is formed. ADDRESS arithmetic is modulo
                // 2^18. TALLY arithmetic is modulo 4096. If the TALLY field is
                // reduced to 0, the tally runout indicator is set ON,
                // otherwise it is set OFF. The TAG field of the indirect word
                // is ignored. The computed address is the value of the
                // unmodified ADDRESS field.

                word18 saveCA = cpu.TPR.CA;

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_ID): fetching indirect word from %06o\n",
                           cpu.TPR.CA);

                word36 indword;
                Read (cpu.TPR.CA, & indword, OPERAND_READ);

                Yi = GETHI (indword);
                cpu.AM_tally = GET_TALLY (indword); // 12-bits
                // get tag field, but ignore it
                word6 junk = GET_TAG (indword);
                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_ID): indword=%012"PRIo64
                           " Yi=%06o tally=%04o\n",
                           indword, Yi, cpu.AM_tally);

                cpu.TPR.CA = Yi;
                word18 computedAddress = cpu.TPR.CA;

                Yi += 1;
                Yi &= MASK18;

                cpu.AM_tally -= 1;
                cpu.AM_tally &= 07777; // keep to 12-bits

                // XXX Breaks boot?
                //if (cpu.AM_tally == 0)
                  //SET_I_TALLY;
                SC_I_TALLY (cpu.AM_tally == 0);

                // write back out indword
                indword = (word36) (((word36) Yi << 18) |
                                    ((word36) cpu.AM_tally << 6) |
                                    junk);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_ID): writing indword=%012"PRIo64" to "
                           "addr %06o\n",
                           indword, saveCA);

                Write (saveCA, indword, OPERAND_STORE);

                cpu.TPR.CA = computedAddress;
                updateIWB (cpu.TPR.CA, 0); // XXX guessing here...

                return SCPE_OK;
              } // IT_ID

            // Decrement address, increment tally, and continue (Td = 15)
            case IT_DIC:
              {
                // The action for this variation is identical to that for the
                // decrement address, increment tally variation except that the
                // TAG field of the indirect word is interpreted and
                // continuation of the indirect chain is possible. If the TAG
                // of the indirect word invokes a register, that is, specifies
                // r, ri, or ir modification, the effective Td value for the
                // register is forced to "null" before the next computed
                // address is formed.

                // a:RJ78/idc1
	      // The address and tally fields are used as described under the
	      // ID variation. The tag field uses the set of variations for
	      // instruction address modification under the following
	      // restrictions: no variation is permitted that requires an
	      // indexing modification in the DIC cycle since the indexing
	      // adder is being used by the tally phase of the operation.
	      // Thus, permissible variations are any allowable form of IT or
	      // IR, but if RI or R is used, R must equal N (RI and R forced
	      // to N).

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_DIC): fetching indirect word from %06o\n",
                           cpu.TPR.CA);

                word18 saveCA = cpu.TPR.CA;
                word36 indword;
                Read (cpu.TPR.CA, & indword, OPERAND_READ);

                Yi = GETHI (indword);
                cpu.AM_tally = GET_TALLY (indword); // 12-bits
                idwtag = GET_TAG (indword);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_DIC): indword=%012"PRIo64" Yi=%06o "
                           "tally=%04o idwtag=%02o\n", 
                           indword, Yi, cpu.AM_tally, idwtag);

                Yi -= 1;
                Yi &= MASK18;

                word24 YiSafe2 = Yi; // save indirect address for later use

                cpu.TPR.CA = Yi;

                cpu.AM_tally += 1;
                cpu.AM_tally &= 07777; // keep to 12-bits
// Set the tally after the indirect word is processed; if it faults, the IR
// should be unchanged. ISOLTS ps791 test-02g
                //SC_I_TALLY (cpu.AM_tally == 0);
                //if (cpu.AM_tally == 0)
                  //SET_I_TALLY;

                // write back out indword
                indword = (word36) (((word36) Yi << 18) |
                          ((word36) cpu.AM_tally << 6) | idwtag);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_DIC): writing indword=%012"PRIo64" to "
                           "addr %06o\n", indword, saveCA);

                Write (saveCA, indword, OPERAND_STORE);
#if 0
                // If the TAG of the indirect word invokes a register, that is,
                // specifies r, ri, or ir modification, the effective Td value
                // for the register is forced to "null" before the next
                // computed address is formed.

                cpu.TPR.CA = YiSafe2;

                cpu.rTAG = idwtag;
                Tm = GET_TM (cpu.rTAG);

                if (Tm != TM_IT)
                  {
                    cpu.rTAG = idwtag & 0x70; // force R to 0
                  }
#else
                // Thus, permissible variations are any allowable form of IT or
                // IR, but if RI or R is used, R must equal N (RI and R forced
                // to N).
                cpu.TPR.CA = YiSafe2;

                cpu.rTAG = idwtag;
                Tm = GET_TM (cpu.rTAG);
                if (Tm == TM_RI || Tm == TM_R)
                  {
                     if (GET_TD (cpu.rTAG) != 0)
                       {
                         doFault (FAULT_IPR,
                                 (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                                 "DIC Incorrect address modifier");
                       }
                  }
#endif
// Set the tally after the indirect word is processed; if it faults, the IR
// should be unchanged. ISOLTS ps791 test-02g
                SC_I_TALLY (cpu.AM_tally == 0);
                updateIWB (cpu.TPR.CA, cpu.rTAG);
                goto startCA;
              } // IT_DIC

            // Increment address, decrement tally, and continue (Td = 17)
            case IT_IDC: 
              {
                // The action for this variation is identical to that for the
                // increment address, decrement tally variation except that the
                // TAG field of the indirect word is interpreted and
                // continuation of the indirect chain is possible. If the TAG
                // of the indirect word invokes a register, that is, specifies
                // r, ri, or ir modification, the effective Td value for the
                // register is forced to "null" before the next computed
                // address is formed.

                // a:RJ78/idc1
	      // The address and tally fields are used as described under the
	      // ID variation. The tag field uses the set of variations for
	      // instruction address modification under the following
	      // restrictions: no variation is permitted that requires an
	      // indexing modification in the IDC cycle since the indexing
	      // adder is being used by the tally phase of the operation.
	      // Thus, permissible variations are any allowable form of IT or
	      // IR, but if RI or R is used, R must equal N (RI and R forced
	      // to N).

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_IDC): fetching indirect word from %06o\n",
                           cpu.TPR.CA);

                word18 saveCA = cpu.TPR.CA;
                word36 indword;
                Read (cpu.TPR.CA, & indword, OPERAND_READ);

                Yi = GETHI (indword);
                cpu.AM_tally = GET_TALLY (indword); // 12-bits
                idwtag = GET_TAG (indword);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_IDC): indword=%012"PRIo64" Yi=%06o "
                           "tally=%04o idwtag=%02o\n",
                           indword, Yi, cpu.AM_tally, idwtag);

                word24 YiSafe = Yi; // save indirect address for later use

                Yi += 1;
                Yi &= MASK18;

                cpu.AM_tally -= 1;
                cpu.AM_tally &= 07777; // keep to 12-bits
// Set the tally after the indirect word is processed; if it faults, the IR
// should be unchanged. ISOLTS ps791 test-02f
                //SC_I_TALLY (cpu.AM_tally == 0);

                // write back out indword
                indword = (word36) (((word36) Yi << 18) |
                                    ((word36) cpu.AM_tally << 6) |
                                    idwtag);

                sim_debug (DBG_ADDRMOD, & cpu_dev,
                           "IT_MOD(IT_IDC): writing indword=%012"PRIo64""
                           " to addr %06o\n",
                           indword, saveCA);

                Write (saveCA, indword, OPERAND_STORE);

#if 0
                // If the TAG of the indirect word invokes a register, that is,
                // specifies r, ri, or ir modification, the effective Td value
                // for the register is forced to "null" before the next
                // computed address is formed.
                // But for the dps88 you can use everything but ir/ri.

                // force R to 0 (except for IT)
                cpu.TPR.CA = YiSafe;

                cpu.rTAG = idwtag;
                Tm = GET_TM (cpu.rTAG);

                if (Tm != TM_IT)
                  {
                    cpu.rTAG = idwtag & 0x70; // force R to 0
                  }
#else
                // Thus, permissible variations are any allowable form of IT or
                // IR, but if RI or R is used, R must equal N (RI and R forced
                // to N).
                cpu.TPR.CA = YiSafe;

                cpu.rTAG = idwtag;
                Tm = GET_TM (cpu.rTAG);
                if (Tm == TM_RI || Tm == TM_R)
                  {
                     if (GET_TD (cpu.rTAG) != 0)
                       {
                         doFault (FAULT_IPR, (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD}, "IDC Incorrect address modifier");
                       }
                  }
#endif
// Set the tally after the indirect word is processed; if it faults, the IR
// should be unchanged. ISOLTS ps791 test-02f
                SC_I_TALLY (cpu.AM_tally == 0);
                updateIWB (cpu.TPR.CA, cpu.rTAG);
                goto startCA;
              } // IT_IDC
          } // Td
        sim_printf ("IT_MOD/Td how did we get here?\n");
        return SCPE_OK;
     } // IT_MOD
  } // doComputedAddressFormation


