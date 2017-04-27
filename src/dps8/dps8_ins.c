/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//#define ISOLTS_BITNO

/**
 * \file dps8_ins.c
 * \project dps8
 * \date 9/22/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"
#include "dps8_addrmods.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_eis.h"
#include "dps8_ins.h"
#include "dps8_math.h"
#include "dps8_opcodetable.h"
#include "dps8_scu.h"
#include "dps8_utils.h"
#include "dps8_decimal.h"
#include "dps8_iefp.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#ifdef HDBG
#include "hdbg.h"
#endif

// Forward declarations

static int doABSA (word36 * result);
static t_stat doInstruction (void);
#ifdef TESTING
#if EMULATOR_ONLY
static int emCall (void);
#endif
#endif

#ifdef LOOPTRC
#include <time.h>
void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

void elapsedtime (void)
  {
    static bool init = false;
    static struct timespec t0;
    struct timespec now, delta;

    if (! init)
      {
        init = true;
        clock_gettime (CLOCK_REALTIME, & t0);
      }
    clock_gettime (CLOCK_REALTIME, & now);
    timespec_diff (& t0, & now, & delta);
    sim_printf ("%5ld.%03ld", delta.tv_sec, delta.tv_nsec/1000000);
  }
#endif

// CANFAULT
static void writeOperands (void)
{
    CPT (cpt2U, 0); // write operands
    DCDstruct * i = & cpu.currentInstruction;

    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "writeOperands (%s):mne=%s flags=%x\n",
               disAssemble (IWB_IRODD), i->info->mne, i->info->flags);

    PNL (cpu.prepare_state |= ps_RAW);

    word6 rTAG = 0;
    if (! (i->info->flags & NO_TAG))
      rTAG = GET_TAG (cpu.cu.IWB);
    word6 Td = GET_TD (rTAG);
    word6 Tm = GET_TM (rTAG);

//
// IT CI/SC/SCR
//

    if (Tm == TM_IT && (Td == IT_CI || Td == IT_SC || Td == IT_SCR))
      {
        CPT (cpt2U, 1); // read indirect operand
        // CI:
        // Bit 30 of the TAG field of the indirect word is interpreted
        // as a character size flag, tb, with the value 0 indicating
        // 6-bit characters and the value 1 indicating 9-bit bytes.
        // Bits 33-35 of the TAG field are interpreted as a 3-bit
        // character/byte position value, cf. Bits 31-32 of the TAG
        // field must be zero.  If the character position value is
        // greater than 5 for 6-bit characters or greater than 3 for 9-
        // bit bytes, an illegal procedure, illegal modifier, fault
        // will occur. The TALLY field is ignored. The computed address
        // is the value of the ADDRESS field of the indirect word. The
        // effective character/byte number is the value of the
        // character position count, cf, field of the indirect word.

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT reading indirect word from %06o\n",
                            cpu.TPR.CA);

        //
        // Get the indirect word
        //

        word18 saveCA = cpu.TPR.CA;

        //word18 indwordAddress = cpu.TPR.CA;
        //Read2 (indwordAddress, cpu.itxPair, INDIRECT_WORD_FETCH);
        //ReadIndirect ();
        Read (cpu.TPR.CA, cpu.itxPair, OPERAND_READ);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT indword=%012"PRIo64"\n", cpu.itxPair[0]);

        //
        // Parse and validate the indirect word
        //

        word18 Yi = GET_ADDR (cpu.itxPair[0]);
        cpu.ou.characterOperandSize = GET_TB (GET_TAG (cpu.itxPair[0]));
        cpu.ou.characterOperandOffset = GET_CF (GET_TAG (cpu.itxPair[0]));

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT size=%o offset=%o Yi=%06o\n",
                   cpu.ou.characterOperandSize, cpu.ou.characterOperandOffset,
                   Yi);

        if (cpu.ou.characterOperandSize == TB6 && cpu.ou.characterOperandOffset > 5)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR,
                   (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                   "co size == TB6 && offset > 5");

        if (cpu.ou.characterOperandSize == TB9 && cpu.ou.characterOperandOffset > 3)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR,
                   (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                   "co size == TB9 && offset > 3");

        if (Td == IT_SCR)
          {
            CPT (cpt2U, 2); // write IT_SCR
            // For each reference to the indirect word, the character
            // counter, cf, is reduced by 1 and the TALLY field is
            // increased by 1 before the computed address is formed.
            // Character count arithmetic is modulo 6 for 6-bit characters
            // and modulo 4 for 9-bit bytes. If the character count, cf,
            // underflows to -1, it is reset to 5 for 6-bit characters or
            // to 3 for 9-bit bytes and ADDRESS is reduced by 1. ADDRESS
            // arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096.
            // If the TALLY field overflows to 0, the tally runout
            // indicator is set ON, otherwise it is set OFF. The computed
            // address is the (possibly) decremented value of the ADDRESS
            // field of the indirect word. The effective character/byte
            // number is the decremented value of the character position
            // count, cf, field of the indirect word.

            if (cpu.ou.characterOperandOffset == 0)
              {
                if (cpu.ou.characterOperandSize == TB6)
                    cpu.ou.characterOperandOffset = 5;
                else
                    cpu.ou.characterOperandOffset = 3;
                Yi -= 1;
                Yi &= MASK18;
              }
                else
              {
                cpu.ou.characterOperandOffset -= 1;
              }
          }

        //
        // Get the data word
        //

        PNL (cpu.prepare_state |= ps_POT);

        cpu.cu.pot = 1;

        word36 data;
        Read (Yi, & data, OPERAND_READ);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT data=%012"PRIo64"\n", data);

        cpu.cu.pot = 0;

        //
        // Put the character into the data word
        //

        switch (cpu.ou.characterOperandSize)
          {
            case TB6:
              putChar (& data, cpu.CY & 077, cpu.ou.characterOperandOffset);
              break;

            case TB9:
              putByte (& data, cpu.CY & 0777, cpu.ou.characterOperandOffset);
              break;
          }

        //
        // Write it
        //

        PNL (cpu.prepare_state |= ps_SAW);

        Write (Yi, data, OPERAND_STORE);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT wrote char/byte %012"PRIo64" to %06o "
                   "tTB=%o tCF=%o\n",
                   data, Yi,
                   cpu.ou.characterOperandSize, cpu.ou.characterOperandOffset);

        // Restore the CA; Read/Write() updates it.
        //cpu.TPR.CA = indwordAddress;
        cpu.TPR.CA = saveCA;

        return;
      } // IT


    WriteOP (cpu.TPR.CA, OPERAND_STORE);

    return;
}

// CANFAULT
static void readOperands (void)
{
    CPT (cpt2U, 3); // read operands
    DCDstruct * i = & cpu.currentInstruction;

    sim_debug (DBG_ADDRMOD, &cpu_dev,
               "readOperands (%s):mne=%s flags=%x\n",
               disAssemble (cpu.cu.IWB), i->info->mne, i->info->flags);
    sim_debug (DBG_ADDRMOD, &cpu_dev,
              "readOperands a %d address %08o\n", i->b29, cpu.TPR.CA);

    PNL (cpu.prepare_state |= ps_POA);

    word6 rTAG = 0;
    if (! (i->info->flags & NO_TAG))
      rTAG = GET_TAG (cpu.cu.IWB);
    word6 Td = GET_TD (rTAG);
    word6 Tm = GET_TM (rTAG);

//
// DU
//

    if (Tm == TM_R && Td == TD_DU)
      {
        cpu.CY = 0;
        SETHI (cpu.CY, cpu.TPR.CA);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands DU CY=%012"PRIo64"\n", cpu.CY);
        return;
      }

//
// DL
//

    if (Tm == TM_R && Td == TD_DL)
      {
        cpu.CY = 0;
        SETLO (cpu.CY, cpu.TPR.CA);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands DL CY=%012"PRIo64"\n", cpu.CY);
        return;
      }

//
// IT CI/SC/SCR
//

    if (Tm == TM_IT && (Td == IT_CI || Td == IT_SC || Td == IT_SCR))
      {
        CPT (cpt2U, 4); // read IT/CI/SCR
        // CI
        // Bit 30 of the TAG field of the indirect word is interpreted
        // as a character size flag, tb, with the value 0 indicating
        // 6-bit characters and the value 1 indicating 9-bit bytes.
        // Bits 33-35 of the TAG field are interpreted as a 3-bit
        // character/byte position value, cf. Bits 31-32 of the TAG
        // field must be zero.  If the character position value is
        // greater than 5 for 6-bit characters or greater than 3 for 9-
        // bit bytes, an illegal procedure, illegal modifier, fault
        // will occur. The TALLY field is ignored. The computed address
        // is the value of the ADDRESS field of the indirect word. The
        // effective character/byte number is the value of the
        // character position count, cf, field of the indirect word.

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT reading indirect word from %06o\n",
                            cpu.TPR.CA);

        //
        // Get the indirect word
        //

        PNL (cpu.prepare_state |= ps_RIW);

        word36 indword;
        word18 indwordAddress = cpu.TPR.CA;
        Read (indwordAddress, & indword, OPERAND_READ);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT indword=%012"PRIo64"\n", indword);

        //
        // Parse and validate the indirect word
        //

        word18 Yi = GET_ADDR (indword);
        cpu.ou.characterOperandSize = GET_TB (GET_TAG (indword));
        cpu.ou.characterOperandOffset = GET_CF (GET_TAG (indword));

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT size=%o offset=%o Yi=%06o\n",
                   cpu.ou.characterOperandSize, cpu.ou.characterOperandOffset,
                   Yi);

        if (cpu.ou.characterOperandSize == TB6 && cpu.ou.characterOperandOffset > 5)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR,
                   (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                   "co size == TB6 && offset > 5");

        if (cpu.ou.characterOperandSize == TB9 && cpu.ou.characterOperandOffset > 3)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR,
                   (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                   "co size == TB9 && offset > 3");

        if (Td == IT_SCR)
          {
            CPT (cpt2U, 5); // read SCR
            // For each reference to the indirect word, the character
            // counter, cf, is reduced by 1 and the TALLY field is
            // increased by 1 before the computed address is formed.
            // Character count arithmetic is modulo 6 for 6-bit characters
            // and modulo 4 for 9-bit bytes. If the character count, cf,
            // underflows to -1, it is reset to 5 for 6-bit characters or
            // to 3 for 9-bit bytes and ADDRESS is reduced by 1. ADDRESS
            // arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096.
            // If the TALLY field overflows to 0, the tally runout
            // indicator is set ON, otherwise it is set OFF. The computed
            // address is the (possibly) decremented value of the ADDRESS
            // field of the indirect word. The effective character/byte
            // number is the decremented value of the character position
            // count, cf, field of the indirect word.

            if (cpu.ou.characterOperandOffset == 0)
              {
                if (cpu.ou.characterOperandSize == TB6)
                    cpu.ou.characterOperandOffset = 5;
                else
                    cpu.ou.characterOperandOffset = 3;
                Yi -= 1;
                Yi &= MASK18;
              }
                else
              {
                cpu.ou.characterOperandOffset -= 1;
              }
          }

        //
        // Get the data word
        //

        PNL (cpu.prepare_state |= ps_SIW);

        word36 data;
        Read (Yi, & data, OPERAND_READ);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT data=%012"PRIo64"\n", data);

        //
        // Get the character from the data word
        //

        switch (cpu.ou.characterOperandSize)
          {
            case TB6:
              cpu.CY = GETCHAR (data, cpu.ou.characterOperandOffset);
              break;

            case TB9:
              cpu.CY = GETBYTE (data, cpu.ou.characterOperandOffset);
              break;
          }

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT read operand %012"PRIo64" from"
                   " %06o char/byte=%"PRIo64"\n",
                   data, Yi, cpu.CY);

        // Restore the CA; Read/Write() updates it.
        cpu.TPR.CA = indwordAddress;

        return;
      } // IT

    ReadOP (cpu.TPR.CA, OPERAND_READ);

    return;
  }

static void scu2words (word36 *words)
  {
    CPT (cpt2U, 6); // scu2words
    memset (words, 0, 8 * sizeof (* words));

    // words[0]

    putbits36_3 (& words[0],  0,  cpu.PPR.PRR);
    putbits36_15 (& words[0],  3, cpu.PPR.PSR);
    putbits36_1 (& words[0], 18,  cpu.PPR.P);
    putbits36_1 (& words[0], 19,  cpu.cu.XSF);
    // 20, 1 SDWAMM Match on SDWAM
    putbits36_1 (& words[0], 21,  cpu.cu.SD_ON);
    // 22, 1 PTWAMM Match on PTWAM
    putbits36_1 (& words[0], 23,  cpu.cu.PT_ON);
#if 0
    putbits36_1 (& words[0], 24,  cpu.cu.PI_AP);   // 24    PI-AP
    putbits36_1 (& words[0], 25,  cpu.cu.DSPTW);   // 25    DSPTW
    putbits36_1 (& words[0], 26,  cpu.cu.SDWNP);   // 26    SDWNP
    putbits36_1 (& words[0], 27,  cpu.cu.SDWP);    // 27    SDWP
    putbits36_1 (& words[0], 28,  cpu.cu.PTW);     // 28    PTW
    putbits36_1 (& words[0], 29,  cpu.cu.PTW2);    // 29    PTW2
    putbits36_1 (& words[0], 30,  cpu.cu.FAP);     // 30    FAP
    putbits36_1 (& words[0], 31,  cpu.cu.FANP);    // 31    FANP
    putbits36_1 (& words[0], 32,  cpu.cu.FABS);    // 32    FABS
#else
    // XXX Only the top 9 bits are used in APUCycleBits, so this is
    // zeroing the 3 FTC bits at the end of the word; on the
    // other hand this keeps the values in apuStatusBits clearer.
    // If FTC is ever used, be sure to put it's save code after this
    // line.
    putbits36_12 (& words[0], 24, cpu.cu.APUCycleBits);
#endif

    // words[1]

    putbits36_1 (& words[1],  0, cpu.cu.IRO_ISN);
    putbits36_1 (& words[1],  1, cpu.cu.OEB_IOC);
    putbits36_1 (& words[1],  2, cpu.cu.EOFF_IAIM);
    putbits36_1 (& words[1],  3, cpu.cu.ORB_ISP);
    putbits36_1 (& words[1],  4, cpu.cu.ROFF_IPR);
    putbits36_1 (& words[1],  5, cpu.cu.OWB_NEA);
    putbits36_1 (& words[1],  6, cpu.cu.WOFF_OOB);
    putbits36_1 (& words[1],  7, cpu.cu.NO_GA);
    putbits36_1 (& words[1],  8, cpu.cu.OCB);
    putbits36_1 (& words[1],  9, cpu.cu.OCALL);
    putbits36_1 (& words[1], 10, cpu.cu.BOC);
    putbits36_1 (& words[1], 11, cpu.cu.PTWAM_ER);
    putbits36_1 (& words[1], 12, cpu.cu.CRT);
    putbits36_1 (& words[1], 13, cpu.cu.RALR);
    putbits36_1 (& words[1], 14, cpu.cu.SDWAM_ER);
    putbits36_1 (& words[1], 15, cpu.cu.OOSB);
    putbits36_1 (& words[1], 16, cpu.cu.PARU);
    putbits36_1 (& words[1], 17, cpu.cu.PARL);
    putbits36_1 (& words[1], 18, cpu.cu.ONC1);
    putbits36_1 (& words[1], 19, cpu.cu.ONC2);
    putbits36_4 (& words[1], 20, cpu.cu.IA);
    putbits36_3 (& words[1], 24, cpu.cu.IACHN);
    putbits36_3 (& words[1], 27, cpu.cu.CNCHN);
    putbits36_5 (& words[1], 30, cpu.cu.FI_ADDR);
    putbits36_1 (& words[1], 35, cpu.cycle == INTERRUPT_cycle ? 0 : 1);

    // words[2]

    putbits36_3 (& words[2],  0,  cpu.TPR.TRR);
    putbits36_15 (& words[2],  3, cpu.TPR.TSR);
    // 18, 4 PTWAM levels enabled
    // 22, 4 SDWAM levels enabled
    // 26, 1 0
    putbits36_3 (& words[2], 27, (word3) cpu.switches.cpu_num);
    putbits36_6 (& words[2], 30, cpu.cu.delta);

    // words[3]

    putbits36_3 (& words[3], 18, cpu.cu.TSN_PRNO[0]);
    putbits36_1 (& words[3], 21, cpu.cu.TSN_VALID[0]);
    putbits36_3 (& words[3], 22, cpu.cu.TSN_PRNO[1]);
    putbits36_1 (& words[3], 25, cpu.cu.TSN_VALID[1]);
    putbits36_3 (& words[3], 26, cpu.cu.TSN_PRNO[2]);
    putbits36_1 (& words[3], 29, cpu.cu.TSN_VALID[2]);
    putbits36_6 (& words[3], 30, cpu.TPR.TBR);

    // words[4]

    putbits36_18 (& words[4],  0, cpu.PPR.IC);

// According the AL39, the Hex Mode bit should be 0, but ISOLTS pas2 exec checks it; this code does not set it to zero and indicated by AL39.

    putbits36_18 (& words[4], 18, cpu.cu.IR);

    // ISOLTS 887 test-03a
    // Adding this makes test03 hang instead of errorign;
    // presumably it's stuck on some later test.
    // An 'Add Delta' addressing mode will alter the TALLY bit;
    // restore it.

    // Breaks ISOLTS 768
    //putbits36_1 (& words[4], 25, cpu.currentInstruction.stiTally);

#ifdef ISOLTS
//testing for ipr fault by attempting execution of
//the illegal opcode  000   and bit 27 not set
//in privileged-append-bar mode.
//
//expects ABS to be clear....
//
//testing for ipr fault by attempting execution of
//the illegal opcode  000   and bit 27 not set
//in absolute-bar mode.
//
//expects ABS to be set

//if (cpu.PPR.P && TST_I_NBAR == 0) fails 101007 absolute-bar mode; s/b set
//if (cpu.PPR.P == 0 && TST_I_NBAR == 0)
//if (TST_I_NBAR == 0 && TST_I_ABS == 1) // If ABS BAR
//{
  //putbits36 (& words[4], 31, 1, 0);
//  putbits36 (& words[4], 31, 1, cpu.PPR.P ? 0 : 1);
//if (currentRunningCPUnum)
//sim_printf ("cleared ABS\n");
//}
#endif

    // words[5]

    putbits36 (& words[5],  0, 18, cpu.TPR.CA);
    putbits36 (& words[5], 18,  1, cpu.cu.repeat_first);
    putbits36 (& words[5], 19,  1, cpu.cu.rpt);
    putbits36 (& words[5], 20,  1, cpu.cu.rd);
    putbits36 (& words[5], 21,  1, cpu.cu.rl);
    putbits36 (& words[5], 22,  1, cpu.cu.pot);
    // 23, 1 PON Prepare operand no tally
    putbits36_1 (& words[5], 24, cpu.cu.xde);
    putbits36_1 (& words[5], 25, cpu.cu.xdo);
    // 26, 1 ITP Execute ITP indirect cycle
    putbits36_1 (& words[5], 27, cpu.cu.rfi);
    // 28, 1 ITS Execute ITS indirect cycle
    putbits36_1 (& words[5], 29, cpu.cu.FIF);
    putbits36_6 (& words[5], 30, cpu.cu.CT_HOLD);

    // words[6]

    words[6] = cpu.cu.IWB;

    // words[7]

    words[7] = cpu.cu.IRODD;
//sim_printf ("scu2words %lld %012llo\n", sim_timell (), words [6]);
  }


void cu_safe_store (void)
{
    // Save current Control Unit Data in hidden temporary so a later SCU
    // instruction running in FAULT mode can save the state as it existed at
    // the time of the fault rather than as it exists at the time the SCU
    //  instruction is executed.
    scu2words (cpu.scu_data);

    cpu.cu_data.PSR = cpu.PPR.PSR;
    cpu.cu_data.PRR = cpu.PPR.PRR;
    cpu.cu_data.IC =  cpu.PPR.IC;

    tidy_cu ();

}

void tidy_cu (void)
  {
// The only places this is called is in fault and interrupt processing;
// once the CU is saved, it needs to be set to a usable state. Refactoring
// that code here so that there is only a single copy to maintain.

    cpu.cu.delta = 0;
    cpu.cu.repeat_first = false;
    cpu.cu.rpt = false;
    cpu.cu.rd = false;
    cpu.cu.rl = false;
    cpu.cu.pot = false;
    cpu.cu.xde = false;
    cpu.cu.xdo = false;
  }

static void words2scu (word36 * words)
{
    CPT (cpt2U, 7); // words2scu
    // BUG:  We don't track much of the data that should be tracked

    // words[0]

    cpu.PPR.PRR         = getbits36_3  (words[0], 0);
    cpu.PPR.PSR         = getbits36_15 (words[0], 3);
    cpu.PPR.P           = getbits36_1  (words[0], 18);
    cpu.cu.XSF          = getbits36_1  (words[0], 19);
    //cpu.cu.SDWAMM       = getbits36_1  (words[0], 20);
    //cpu.cu.SD_ON        = getbits36_1  (words[0], 21);
    //cpu.cu.PTWAMM       = getbits36_1  (words[0], 22);
    //cpu.cu.PT_ON        = getbits36_1  (words[0], 23);
#if 0
    //cpu.cu.PI_AP        = getbits36_1  (words[0], 24);
    //cpu.cu.DSPTW        = getbits36_1  (words[0], 25);
    //cpu.cu.SDWNP        = getbits36_1  (words[0], 26);
    //cpu.cu.SDWP         = getbits36_1  (words[0], 27);
    //cpu.cu.PTW          = getbits36_1  (words[0], 28);
    //cpu.cu.PTW2         = getbits36_1  (words[0], 29);
    //cpu.cu.FAP          = getbits36_1  (words[0], 30);
    //cpu.cu.FANP         = getbits36_1  (words[0], 31);
    //cpu.cu.FABS         = getbits36_1  (words[0], 32);
#else
    //cpu.cu.APUCycleBits = getbits36_12 (words[0], 24);
#endif
    // The FCT is stored in APUCycleBits
    cpu.cu.APUCycleBits = (word12) ((cpu.cu.APUCycleBits & 07770) | (word12) getbits36_3 (words[0], 33));

    // words[1]

    cpu.cu.IRO_ISN      = getbits36_1  (words[1],  0);
    cpu.cu.OEB_IOC      = getbits36_1  (words[1],  1);
    cpu.cu.EOFF_IAIM    = getbits36_1  (words[1],  2);
    cpu.cu.ORB_ISP      = getbits36_1  (words[1],  3);
    cpu.cu.ROFF_IPR     = getbits36_1  (words[1],  4);
    cpu.cu.OWB_NEA      = getbits36_1  (words[1],  5);
    cpu.cu.WOFF_OOB     = getbits36_1  (words[1],  6);
    cpu.cu.NO_GA        = getbits36_1  (words[1],  7);
    cpu.cu.OCB          = getbits36_1  (words[1],  8);
    cpu.cu.OCALL        = getbits36_1  (words[1],  9);
    cpu.cu.BOC          = getbits36_1  (words[1], 10);
    cpu.cu.PTWAM_ER     = getbits36_1  (words[1], 11);
    cpu.cu.CRT          = getbits36_1  (words[1], 12);
    cpu.cu.RALR         = getbits36_1  (words[1], 13);
    cpu.cu.SDWAM_ER     = getbits36_1  (words[1], 14);
    cpu.cu.OOSB         = getbits36_1  (words[1], 15);
    cpu.cu.PARU         = getbits36_1  (words[1], 16);
    cpu.cu.PARL         = getbits36_1  (words[1], 17);
    cpu.cu.ONC1         = getbits36_1  (words[1], 18);
    cpu.cu.ONC2         = getbits36_1  (words[1], 19);
    cpu.cu.IA           = getbits36_4  (words[1], 20);
    cpu.cu.IACHN        = getbits36_3  (words[1], 24);
    cpu.cu.CNCHN        = getbits36_3  (words[1], 27);
    cpu.cu.FI_ADDR      = getbits36_5  (words[1], 30);
    cpu.cu.FLT_INT      = getbits36_1  (words[1], 35);

    // words[2]

    cpu.TPR.TRR         = getbits36_3  (words[2], 0);
    cpu.TPR.TSR         = getbits36_15 (words[2], 3);
    // 18-21 PTW
    // 22-25 SDW
    // 26 0
    // 27-29 CPU number
    cpu.cu.delta        = getbits36_6  (words[2], 30);

    // words[3]

    // 0-17 0

    cpu.cu.TSN_PRNO[0]  = getbits36_3  (words[3], 18);
    cpu.cu.TSN_VALID[0] = getbits36_1  (words[3], 21);
    cpu.cu.TSN_PRNO[1]  = getbits36_3  (words[3], 22);
    cpu.cu.TSN_VALID[1] = getbits36_1  (words[3], 25);
    cpu.cu.TSN_PRNO[2]  = getbits36_3  (words[3], 26);
    cpu.cu.TSN_VALID[2] = getbits36_1  (words[3], 29);
    cpu.TPR.TBR         = getbits36_6  (words[3], 30);

    // words[4]

    cpu.cu.IR           = getbits36_18 (words[4], 18); // HWR
    cpu.PPR.IC          = getbits36_18 (words[4], 0);

    // words[5]

// XXX According to AL39 pg 75, RCU does not restore CA, but boot crashes
// if not restored.
    //cpu.TPR.CA          = getbits36_18 (words[5], 0);
    cpu.cu.repeat_first = getbits36_1  (words[5], 18);
    cpu.cu.rpt          = getbits36_1  (words[5], 19);
    cpu.cu.rd           = getbits36_1  (words[5], 20);
    cpu.cu.rl           = getbits36_1  (words[5], 21);
    cpu.cu.pot          = getbits36_1  (words[5], 22);
    // 23 PON
    cpu.cu.xde          = getbits36_1  (words[5], 24);
    cpu.cu.xdo          = getbits36_1  (words[5], 25);
    // 26 ITP
    cpu.cu.rfi          = getbits36_1  (words[5], 27);
    // 28 ITS
    cpu.cu.FIF          = getbits36_1  (words[5], 29);
    cpu.cu.CT_HOLD      = getbits36_6  (words[5], 30);

    // words[6]

    cpu.cu.IWB = words[6];

    // words[7]

    cpu.cu.IRODD = words[7];
}

void cu_safe_restore (void)
  {
    words2scu (cpu.scu_data);
  }

static void du2words (word36 * words)
  {
    CPT (cpt2U, 7); // du2words

#ifdef ISOLTS
    for (int i = 0; i < 8; i ++)
      {
        words[i] = cpu.du.image[i];
      }
#else
    memset (words, 0, 8 * sizeof (* words));
#endif
    // Word 0

    putbits36_1  (& words[0],  9, cpu.du.Z);
    putbits36_1  (& words[0], 10, cpu.du.NOP);
    putbits36_24 (& words[0], 12, cpu.du.CHTALLY);

    // Word 1

#ifdef ISOLTS
    words[1] = words[0];
#endif
    // Word 2

    putbits36_18 (& words[2],  0, cpu.du.D1_PTR_W);
    putbits36_6  (& words[2], 18, cpu.du.D1_PTR_B);
    putbits36_2  (& words[2], 25, cpu.du.TAk[0]);
    putbits36_1  (& words[2], 31, cpu.du.F1);
    putbits36_1  (& words[2], 32, cpu.du.Ak[0]);

    // Word 3

    putbits36_10 (& words[3],  0, cpu.du.LEVEL1);
    putbits36_24 (& words[3], 12, cpu.du.D1_RES);

    // Word 4

    putbits36_18 (& words[4],  0, cpu.du.D2_PTR_W);
    putbits36_6  (& words[4], 18, cpu.du.D2_PTR_B);
    putbits36_2  (& words[4], 25, cpu.du.TAk[1]);
    putbits36_1  (& words[4], 30, cpu.du.R);
    putbits36_1  (& words[4], 31, cpu.du.F2);
    putbits36_1  (& words[4], 32, cpu.du.Ak[1]);

    // Word 5

    putbits36_10 (& words[5],  0, cpu.du.LEVEL2);
    putbits36_24 (& words[5], 12, cpu.du.D2_RES);

    // Word 6

    putbits36_18 (& words[6],  0, cpu.du.D3_PTR_W);
    putbits36_6  (& words[6], 18, cpu.du.D3_PTR_B);
    putbits36_2  (& words[6], 25, cpu.du.TAk[2]);
    putbits36_1  (& words[6], 31, cpu.du.F3);
    putbits36_1  (& words[6], 32, cpu.du.Ak[2]);
    putbits36_3  (& words[6], 33, cpu.du.JMP);

    // Word 7

    putbits36_24 (& words[7], 12, cpu.du.D3_RES);

  }

static void words2du (word36 * words)
  {
    CPT (cpt2U, 8); // words2du
    // Word 0

    cpu.du.Z        = getbits36_1  (words[0],  9);
    cpu.du.NOP      = getbits36_1  (words[0], 10);
    cpu.du.CHTALLY  = getbits36_24 (words[0], 12);
    // Word 1

    // Word 2

    cpu.du.D1_PTR_W = getbits36_18 (words[2],  0);
    cpu.du.D1_PTR_B = getbits36_6  (words[2], 18);
    cpu.du.TAk[0]   = getbits36_2  (words[2], 25);
    cpu.du.F1       = getbits36_1  (words[2], 31);
    cpu.du.Ak[0]    = getbits36_1  (words[2], 32);

    // Word 3

    cpu.du.LEVEL1   = getbits36_10 (words[3],  0);
    cpu.du.D1_RES   = getbits36_24 (words[3], 12);

    // Word 4

    cpu.du.D2_PTR_W = getbits36_18 (words[4],  0);
    cpu.du.D2_PTR_B = getbits36_6  (words[4], 18);
    cpu.du.TAk[1]   = getbits36_2  (words[4], 25);
    cpu.du.F2       = getbits36_1  (words[4], 31);
    cpu.du.Ak[1]    = getbits36_1  (words[4], 32);

    // Word 5

    cpu.du.LEVEL2   = getbits36_1  (words[5],  9);
    cpu.du.D2_RES   = getbits36_24 (words[5], 12);

    // Word 6

    cpu.du.D3_PTR_W = getbits36_18 (words[6],  0);
    cpu.du.D3_PTR_B = getbits36_6  (words[6], 18);
    cpu.du.TAk[2]   = getbits36_2  (words[6], 25);
    cpu.du.F3       = getbits36_1  (words[6], 31);
    cpu.du.Ak[2]    = getbits36_1  (words[6], 32);
    cpu.du.JMP      = getbits36_3  (words[6], 33);

    // Word 7

    cpu.du.D3_RES   = getbits36_24 (words[7], 12);

#ifdef ISOLTS
    for (int i = 0; i < 8; i ++)
      {
        cpu.du.image[i] = words[i];
      }
#endif
  }

static char *PRalias[] = {"ap", "ab", "bp", "bb", "lp", "lb", "sp", "sb" };


//=============================================================================

// illegal modifications for various instructions

/*

        00  01  02  03  04  05  06  07

 00     --  au  qu  du  ic  al  ql  dl  R
 10     0   1   2   3   4   5   6   7

 20     n*  au* qu* --  ic* al* al* --  RI
 30     0*  1*  2*  3*  4*  5*  6*  7*

 40     f1  itp --  its sd  scr f2  f3  IT
 50     ci  i   sc  ad  di  dic id  idc

 60     *n  *au *qu --  *ic *al *al --  IR
 70     *0  *1  *2  *3  *4  *5  *6  *7


 bool _allowed[] = {
 // Tm = 0 (register) R
 // --  au     qu     du     ic     al     ql     dl
 true,  false, false, false, false, false, false, false,
 // 0   1      2      3      4      5      6      7
 false, false, false, false, false, false, false, false,
 // Tm = 1 (register then indirect) RI
 // n*  au*    qu*    --     ic*    al*    al*    --
 false, false, false, true,  false, false, false, true,
 // 0*  1*     2*     3*     4*     5*     6*     7*
 false, false, false, false, false, false, false, false,
 // Tm = 2 (indirect then tally) IT
 // f1  itp    --     its    sd     scr    f2     f3
 false, false, true, false, false, false, false, false,
 // ci  i      sc     ad     di     dic    id     idc
 false, false, false, false, false, false, false, false,
 // Tm = 3 (indirect then register) IR
 // *n  *au    *qu    --     *ic    *al    *al    --
 false, false, false, true,  false, false, false, true,
 // *0  *1     *2     *3     *4     *5     *6     *7
 false, false, false, false, false, false, false, false,
 };

 */
// No DUDL

static bool _nodudl[] = {
    // Tm = 0 (register) R
    // --   au     qu     du     ic     al     ql     dl
    false, false, false, true,  false, false, false, true, 
    // 0      1      2      3      4      5      6      7
     false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --
    false, false, false, true, false, false, false, true, 
    // 0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp    --     its    sd     scr    f2     f3
    false, false, true,  false, false, false, false, false,
    // ci     i      sc     ad     di     dic   id     idc
    false, false, false, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --
    false, false, false, true, false, false, false, true, 
    // *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, false, false, false, false, false,
};

// No DU
// No DL



// (NO_CI | NO_SC | NO_SCR)
static bool _nocss[] = {
    // Tm = 0 (register) R
    // *    au     qu     du     ic     al     ql     dl
    false, false, false, false, false, false, false, false,
    // 0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --
    false, false, false, true, false, false, false, true,
    // 0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp     --     its    sd     scr    f2     f3
    false, false, true,  false, false, true, false, false,
    // ci     i     sc     ad     di    dic    id     idc
    true, false, true, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --
    false, false, false, true, false, false, false, true,
    // *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, false, false, false, false, false,
};

// (NO_DUDL | NO_CISCSCR)
static bool _noddcss[] = {
    // Tm = 0 (register) R
    // *    au     qu     du     ic     al     ql    dl
    false, false, false, true, false, false, false, true,
    // 0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --
    false, false, false, true, false, false, false, true, 
    // 0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp     --     its    sd     scr    f2     f3
    false, false, true,  false, false, true, false, false,
    // ci     i     sc     ad     di    dic    id     idc
    true, false, true, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --
    false, false, false, true, false, false, false, true,
    // *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, false, false, false, false, false,
};

// (NO_DUDL | NO_CISCSCR)
static bool _nodlcss[] = {
    // Tm = 0 (register) R
    // *    au     qu     du      ic     al     ql    dl
    false, false, false, false, false, false, false, true,
    // 0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --
    false, false, false, true, false, false, false, true,
    // 0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp     --     its    sd     scr    f2     f3
    false, false, true,  false, false, true, false, false,
    // ci     i     sc     ad     di    dic    id     idc
    true, false, true, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --
    false, false, false, true, false, false, false, true,
    // *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, false, false, false, false, false,
};

static bool _onlyaqxn[] = {
    // Tm = 0 (register) R
    // --   au     qu     du     ic     al     ql     dl
    false, false, false, true,  true, false, false, true, 
    // 0      1      2      3      4      5      6      7
     false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --
    false, false, false, true, false, false, false, true, 
    // 0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp    --     its    sd     scr    f2     f3
    false, false, true,  false, false, false, false, false,
    // ci     i      sc     ad     di     dic   id     idc
    false, false, false, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --
    false, false, false, true, false, false, false, true, 
    // *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, false, false, false, false, false,
};

#ifndef QUIET_UNUSED
static bool _illmod[] = {
    // Tm = 0 (register) R
    // *    au     qu     du     ic     al     ql     dl
    false, false, false, false, false, false, false, false,
    // 0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --
    false, false, false, true, false, false, false, true, 
    // 0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp    --     its    sd     scr    f2     f3
    false, false, true,  false, false, false, false, false,
    // ci      i      sc     ad     di     dic   id     idc
    false, false, false, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --
    // *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, true, false, false, false, true,
    false, false, false, false, false, false, false, false,
};
#endif

//=============================================================================

#ifdef MATRIX

static long long theMatrix[1024] // 1024 opcodes (2^10)
                          [2]    // opcode extension
                          [2]    // bit 29
                          [64];  // Tag

void initializeTheMatrix (void)
{
    memset (theMatrix, 0, sizeof (theMatrix));
}

void addToTheMatrix (uint32 opcode, bool opcodeX, bool a, word6 tag)
{
    // safety
    uint _opcode = opcode & 01777;
    int _opcodeX = opcodeX ? 1 : 0;
    int _a = a ? 1 : 0;
    int _tag = tag & 077;
    theMatrix[_opcode][_opcodeX][_a][_tag] ++;
}
#endif

t_stat displayTheMatrix (UNUSED int32 arg, UNUSED const char * buf)
{
#ifdef MATRIX
    long long count;
    for (int opcode = 0; opcode < 01000; opcode ++)
    for (int opcodeX = 0; opcodeX < 2; opcodeX ++)
    for (int a = 0; a < 2; a ++)
    for (int tag = 0; tag < 64; tag ++)
    if ((count = theMatrix[opcode][opcodeX][a][tag]))
    {
        // disAssemble doesn't quite do what we want so copy the good bits
        static char result[132] = "???";
        strcpy (result, "???");
        // get mnemonic ...
        // non-EIS first
        if (!opcodeX)
        {
            if (NonEISopcodes[opcode].mne)
                strcpy (result, NonEISopcodes[opcode].mne);
        }
        else
        {
            // EIS second...
            if (EISopcodes[opcode].mne)
                strcpy (result, EISopcodes[opcode].mne);

            if (EISopcodes[opcode].ndes > 0)
            {
                // XXX need to reconstruct multi-word EIS instruction.

            }
        }

        if (a)
            strcat (result, " prn|nnnn");
        else
            strcat (result, " nnnn");

        // get mod
        if (extMods[tag].mod)
        {
            strcat (result, ",");
            strcat (result, extMods[tag].mod);
        }
        if (result[0] == '?')
            sim_printf ("%20"PRId64": ? opcode 0%04o X %d a %d tag 0%02do\n",
                        count, opcode, opcodeX, a, tag);
        else
            sim_printf ("%20"PRId64": %s\n", count, result);
    }
#else
    sim_printf ("matrix code not enabled\n");
#endif
    return SCPE_OK;
}


// fetch instrcution at address
// CANFAULT
void fetchInstruction (word18 addr)
{
    CPT (cpt2U, 9); // fetchInstruction

    if (get_addr_mode () == ABSOLUTE_mode)
      {
        cpu.TPR.TRR = 0;
        cpu.RSDWH_R1 = 0;
        cpu.PPR.P = 1; // ISOLTS-870 02m
      }

    if (cpu.cu.rd && ((cpu.PPR.IC & 1) != 0))
      {
        if (cpu.cu.repeat_first)
          {
            CPT (cpt2U, 10); // fetch rpt odd
            Read (addr, & cpu.cu.IRODD, INSTRUCTION_FETCH);
          }
      }
    else if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        if (cpu.cu.repeat_first)
          {
            CPT (cpt2U, 11); // fetch rpt even
            Read (addr, & cpu.cu.IWB, INSTRUCTION_FETCH);
#if 0
            word36 tmp[2];
            Read2 (addr, tmp, INSTRUCTION_FETCH);
            cpu.cu.IWB = tmp[0];
            cpu.cu.IRODD = tmp[1];
#endif
          }
      }
    else
      {
        CPT (cpt2U, 12); // fetch 
// ISOLTS test pa870 expects IRODD to be set up.
// If we are fetching an even instruction, also fetch the odd.
// If we are fetching an odd instruction, copy it to IRODD as
// if that was where we got it from.
        //Read (addr, & cpu.cu.IWB, INSTRUCTION_FETCH);
        if ((cpu.PPR.IC & 1) == 0) // Even
          {
            word36 tmp[2];
            Read2 (addr, tmp, INSTRUCTION_FETCH);
            cpu.cu.IWB = tmp[0];
            cpu.cu.IRODD = tmp[1];
          }
        else // Odd
          {
            Read (addr, & cpu.cu.IWB, INSTRUCTION_FETCH);
            cpu.cu.IRODD = cpu.cu.IWB; 
          }
      }
}

void traceInstruction (uint flag)
  {
    if (! flag) goto force;
    if_sim_debug (flag, &cpu_dev)
      {
force:;
        char * compname;
        word18 compoffset;
        char * where = lookupAddress (cpu.PPR.PSR, cpu.PPR.IC, & compname,
                                      & compoffset);
        bool isBAR = TST_I_NBAR ? false : true;
        if (where)
          {
            if (get_addr_mode () == ABSOLUTE_mode)
              {
                if (isBAR)
                  {
                    sim_debug (flag, &cpu_dev, "%06o|%06o %s\n",
                               cpu.BAR.BASE, cpu.PPR.IC, where);
                  }
                else
                  {
                    sim_debug (flag, &cpu_dev, "%06o %s\n", cpu.PPR.IC, where);
                  }
              }
            else if (get_addr_mode () == APPEND_mode)
              {
                if (isBAR)
                  {
                    sim_debug (flag, &cpu_dev, "%05o:%06o|%06o %s\n",
                               cpu.PPR.PSR,
                               cpu.BAR.BASE, cpu.PPR.IC, where);
                  }
                else
                  {
                    sim_debug (flag, &cpu_dev, "%05o:%06o %s\n",
                               cpu.PPR.PSR, cpu.PPR.IC, where);
                  }
              }
            listSource (compname, compoffset, flag);
          }
        if (get_addr_mode () == ABSOLUTE_mode)
          {
            if (isBAR)
              {
#ifdef ROUND_ROBIN
                sim_debug (flag, &cpu_dev,
                  "%d: "
                  "%05o|%06o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  currentRunningCPUnum,
                  cpu.BAR.BASE,
                  cpu.PPR.IC,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29,
                  cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#else
                sim_debug (flag, &cpu_dev,
                  "%05o|%06o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.BAR.BASE,
                  cpu.PPR.IC,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29,
                  cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#endif
              }
            else
              {
#ifdef ROUND_ROBIN
                sim_debug (flag, &cpu_dev,
                  "%d: "
                  "%06o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  currentRunningCPUnum,
                  cpu.PPR.IC,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29,
                  cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#else
                sim_debug (flag, &cpu_dev,
                  "%06o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.PPR.IC,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29,
                  cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#endif
              }
          }
        else if (get_addr_mode () == APPEND_mode)
          {
            if (isBAR)
              {
#ifdef ROUND_ROBIN
                sim_debug (flag, &cpu_dev,
                  "%d: "
                 "%05o:%06o|%06o %o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  currentRunningCPUnum,
                  cpu.PPR.PSR,
                  cpu.BAR.BASE,
                  cpu.PPR.IC,
                  cpu.PPR.PRR,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29, cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#else
                sim_debug (flag, &cpu_dev,
                 "%05o:%06o|%06o %o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.PPR.PSR,
                  cpu.BAR.BASE,
                  cpu.PPR.IC,
                  cpu.PPR.PRR,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29, cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#endif
              }
            else
              {
#ifdef ROUND_ROBIN
                sim_debug (flag, &cpu_dev,
                  "%d: "
                  "%05o:%06o %o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  currentRunningCPUnum,
                  cpu.PPR.PSR,
                  cpu.PPR.IC,
                  cpu.PPR.PRR,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29,
                  cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#else
                sim_debug (flag, &cpu_dev,
                  "%05o:%06o %o %012"PRIo64" (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.PPR.PSR,
                  cpu.PPR.IC,
                  cpu.PPR.PRR,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.b29,
                  cpu.currentInstruction.i,
                  GET_TM (cpu.currentInstruction.tag) >> 4,
                  GET_TD (cpu.currentInstruction.tag) & 017);
#endif
              }
          }
      }

  }

bool chkOVF (void)
  {
    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        // a:AL39/rpd2
        // Did the repeat instruction inhibit overflow faults?
        if ((cpu.rX[0] & 00001) == 0)
          return false;
      }
    return true;
  }
    
bool tstOVFfault (void)
  {
    // Masked?
    if (TST_I_OMASK)
      return false;
    // Doing a RPT/RPD?
    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        // a:AL39/rpd2
        // Did the repeat instruction inhibit overflow faults?
        if ((cpu.rX[0] & 00001) == 0)
          return false;
      }
    return true;
  }

t_stat executeInstruction (void)
  {
    CPT (cpt2U, 13); // execute instruction 

//
// Decode the instruction
//
// If not restart
//     if xec/xed
//         check for illegal execute
//     if rpt/rpd
//         check for illegal rpt/rpd modifiers
//     check for illegal modifiers
//     check for privilege
//     initialize CA
//
// Save tally
// Debug trace instruction
// If not restart
//    Initialize TPR
//
// Initialize DU.JMP
// If rpt/rpd
//     If first repeat
//         Initialize Xn
//
// If EIS instruction
//     If not restart
//         Initialize DU.CHTALLY, DU.Z
//     Read operands
//     Parse operands
// Else not EIS instruction
//     If not restart
//         If B29
//             Set TPR from pointer register
//         Else
//             Setup up TPR
//         Initialize CU.CT_HOLD
//     If restart and CU.POT
//         Restore CA from IWB
//     Do CAF if needed
//     Read operand if needed
//
// Execute the instruction
//
// Write operand if needed
// Update IT tally if needed
// If XEC/XED, move instructions into IWB/IRODD
// If instruction was repeated
//     Update Xn
//     Check for repeat termination
// Post-instruction debug


///
/// executeInstruction: Decode the instruction
///

    DCDstruct * ci = & cpu.currentInstruction;
    decodeInstruction (IWB_IRODD, ci);
    //cpu.isb29 = ci->b29;
    //ISB29 = ci->b29;
    const opCode *info = ci->info;       // opCode *

#ifdef MATRIX
    const uint32  opcode = ci->opcode;   // opcode
    const bool   opcodeX = ci->opcodeX;  // opcode extension
                                         // XXX replace with rY
    const bool   b29 = ci->b29;              // bit-29 - addressing via pointer
                                         // register
    const word6  tag = ci->tag;          // instruction tag
                                         //  XXX replace withrTAG


    addToTheMatrix (opcode, opcodeX, b29, tag);
#endif

//sim_debug (DBG_TRACE, & cpu_dev, "isb29 %o\n", ci->b29);
    if (ci->b29)
      ci->address = SIGNEXT15_18 (ci->address & MASK15);

#ifdef L68
    CPTUR (cptUseMR);
    if (cpu.MR.emr && cpu.MR.OC_TRAP)
      {
        if (cpu.MR.OPCODE == ci->opcode &&
            cpu.MR.OPCODEX == ci->opcodeX) 
          {
            if (cpu.MR.ihrrs)
              {
                cpu.MR.ihr = 0;
              }
            CPT (cpt2U, 14); // opcode trap
IF1 sim_printf ("trapping opcode match......\n");
            //set_FFV_fault (2); // XXX According to AL39
            do_FFV_fault (1, "OC TRAP");
          }
      }
#endif // L68

///
/// executeInstruction: Non-restart processing
///

    if (!ci->restart || info->ndes > 0) // until we implement EIS restart
    {
        cpu.cu.TSN_VALID[0] = 0;
        cpu.cu.TSN_VALID[1] = 0;
        cpu.cu.TSN_VALID[2] = 0;
        cpu.cu.TSN_PRNO[0] = 0;
        cpu.cu.TSN_PRNO[1] = 0;
        cpu.cu.TSN_PRNO[2] = 0;
    }

    if (ci->restart)
      goto restart_1;

#ifdef XSF_IND
    cpu.cu.XSF = 0;
#endif
    CPT (cpt2U, 14); // non-restart processing
    // Set Address register empty
    PNL (L68_ (cpu.AR_F_E = false;))

    // Reset the fault counter
    cpu.cu.APUCycleBits &= 07770;

    //cpu.cu.TSN_VALID[0] = 0;
    //cpu.cu.TSN_VALID[1] = 0;
    //cpu.cu.TSN_VALID[2] = 0;

    // If executing the target of XEC/XED, check the instruction is allowed
    if (cpu.isXED)
    {
		if (ci->info->flags & NO_XED)
            doFault (FAULT_IPR,
                     (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                     "Instruction not allowed in XEC/XED");
        // The even instruction from C(Y-pair) must not alter
        // C(Y-pair)36,71, and must not be another xed instruction.
        if (ci->opcode == 0717 && !ci->opcodeX && cpu.cu.xdo /* even instruction being executed */)
            doFault (FAULT_IPR,
                     (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                     "XED of XED on even word");
        // ISOLTS 791 03k, 792 03k
        if (ci->opcode == 0560 && !ci->opcodeX) {
            // To Execute Double (XED) the RPD instruction, the RPD must be the second
            // instruction at an odd-numbered address.
            if (cpu.cu.xdo /* even instr being executed */)
                doFault (FAULT_IPR,
                     (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                     "XED of RPD on even word");
            // To execute an instruction pair having an rpd instruction as the odd
            // instruction, the xed instruction must be located at an odd address.
            if (!cpu.cu.xdo /* odd instr being executed */ && !(cpu.PPR.IC & 1))
                doFault (FAULT_IPR,
                     (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                     "XED of RPD on odd word, even IC");
        }
    } else if (cpu.isExec) {
        // To execute a rpd instruction, the xec instruction must be in an odd location.
        // ISOLTS 768 01w
        if (ci->opcode == 0560 && !ci->opcodeX && !cpu.cu.xde && !(cpu.PPR.IC & 1)) 
            doFault (FAULT_IPR,
                 (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                 "XEC of RPx on even word");
    }

    // ISOLTS wants both the not allowed in RPx and RPx illegal modifier 
    // tested.
    fault_ipr_subtype_ RPx_fault = 0;

    // In BAR mode and not allowed?

#if 0
    if (TST_I_NBAR == 0)
      if (ci->info->flags & NO_BAR)
        RPx_fault |= FR_ILL_SLV;
#endif

    // RPT/RPD illegal modifiers
    // a:AL39/rpd3
    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        if (! (ci->info->flags & NO_TAG))
          {
            // check for illegal modifiers:
            //    only R & RI are allowed
            //    only X1..X7
            switch (GET_TM (ci->tag))
              {
                case TM_RI:
                  if (cpu.cu.rl)
                    {
                      RPx_fault |= FR_ILL_MOD;
                    }
                  break;
                case TM_R:
                  break;
                default:
                  // generate fault. Only R & RI allowed
                  RPx_fault |= FR_ILL_MOD;
              }

            word6 Td = GET_TD (ci->tag);
            if (Td == TD_X0)
              {
                RPx_fault |= FR_ILL_MOD;
              }
            //if (! cpu.cu.rd && Td < TD_X0)
            if (Td < TD_X0)
              {
                RPx_fault |= FR_ILL_MOD;
              }
          }

#ifdef DPS8M
        // ISOLTS 792 03e
        // this is really strange. possibly a bug in DPS8M HW (L68 handles it the same as all other instructions)
        if (RPx_fault && !ci->opcodeX && ci->opcode==0413) // rscr
          {
              doFault (FAULT_IPR,
                 (_fault_subtype) {.fault_ipr_subtype=RPx_fault},
                 "DPS8M rscr early raise");
          }
#endif

    // Instruction not allowed in RPx?

    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        if (ci->info->flags & NO_RPT)
          {
            RPx_fault |= FR_ILL_PROC;
          }
      }

    if (cpu.cu.rl)
      {
        if (ci->info->flags & NO_RPL)
          {
            RPx_fault |= FR_ILL_PROC;
          }
      }

#ifdef L68
        // ISOLTS 791 03d, 792 03d
        // L68 wants ILL_MOD here - stca,stcq,stba,stbq,scpr,lcpr
        // all these instructions have a nonstandard TAG field interpretation. probably a HW bug in decoder
        if (RPx_fault && !ci->opcodeX && (ci->opcode==0751 || ci->opcode==0752 || ci->opcode==0551 
            || ci->opcode==0552 || ci->opcode==0452 || ci->opcode==0674))
          {
            RPx_fault |= FR_ILL_MOD;
          }
#endif
      }

    if (RPx_fault)
      {
        doFault (FAULT_IPR,
                 (_fault_subtype) {.fault_ipr_subtype=RPx_fault},
                 "RPx test fail");
      }

    ///                     check for illegal addressing mode(s) ...
    ///
    // ISOLTS wants both the IPR and illegal modifier tested.
    fault_ipr_subtype_ mod_fault = 0;

    // No CI/SC/SCR allowed
    if (ci->info->mods == NO_CSS)
    {
        if (_nocss[ci->tag])
            mod_fault |= FR_ILL_MOD; // "Illegal CI/SC/SCR modification"
    }
    // No DU/DL/CI/SC/SCR allowed
    else if (ci->info->mods == NO_DDCSS)
    {
        if (_noddcss[ci->tag])
            mod_fault |= FR_ILL_MOD; // "Illegal DU/DL/CI/SC/SCR modification"
    }
    // No DL/CI/SC/SCR allowed
    else if (ci->info->mods == NO_DLCSS)
    {
        if (_nodlcss[ci->tag])
            mod_fault |= FR_ILL_MOD; // "Illegal DL/CI/SC/SCR modification"
    }
    // No DU/DL allowed
    else if (ci->info->mods == NO_DUDL)
    {
        if (_nodudl[ci->tag])
            mod_fault |= FR_ILL_MOD; // "Illegal DU/DL modification"
    }
    else if (ci->info->mods == ONLY_AU_QU_AL_QL_XN)
    {
        if (_onlyaqxn[ci->tag])
            mod_fault |= FR_ILL_MOD; // "Illegal DU/DL/IC modification"
    }

#ifdef L68
    // L68 raises it immediately
    if (mod_fault)
      {
        doFault (FAULT_IPR,
                 (_fault_subtype) {.fault_ipr_subtype=mod_fault},
                 "Illegal modifier");
      }
#endif

    // check for priv ins - Attempted execution in normal or BAR modes causes a
    // illegal procedure fault.
    if (ci->info->flags & PRIV_INS)
      {
#ifdef DPS8M
        // DPS8M illegal instructions lptp,lptr,lsdp,lsdr
        // ISOLTS 890 05abc
        if (((ci->opcode == 0232 || ci->opcode == 0173) && ci->opcodeX ) 
           || (ci->opcode == 0257))
        {
            doFault (FAULT_IPR,
                (_fault_subtype) {.fault_ipr_subtype=FR_ILL_OP|mod_fault},
                "Attempted execution of multics privileged instruction.");
        }
#endif
        if (! ((get_addr_mode () == ABSOLUTE_mode) ||
                            is_priv_mode ()) || get_bar_mode())
          {
            // "multics" privileged instructions: absa,ldbr,lra,rcu,scu,sdbr,ssdp,ssdr,sptp,sptr
            // ISOLTS 890 05abc,06abc
#ifdef DPS8M
            if (((ci->opcode == 0212 || ci->opcode == 0232 || ci->opcode == 0613 || ci->opcode == 0657) && !ci->opcodeX )
               || ((ci->opcode == 0254 || ci->opcode == 0774) && ci->opcodeX ) 
               || (ci->opcode == 0557 || ci->opcode == 0154))
#else // L68
            // on L68, lptp,lptr,lsdp,lsdr instructions are not illegal, so handle them here
            if (((ci->opcode == 0212 || ci->opcode == 0232 || ci->opcode == 0613 || ci->opcode == 0657) && !ci->opcodeX )
               || ((ci->opcode == 0254 || ci->opcode == 0774 || ci->opcode == 0232 || ci->opcode == 0173) && ci->opcodeX ) 
               || (ci->opcode == 0557 || ci->opcode == 0154 || ci->opcode == 0257))
#endif
            {
                if ((!is_priv_mode () && !get_bar_mode())) {
                    // SLV makes no sense here, but ISOLTS 890 sez so.
                    doFault (FAULT_IPR,
                        (_fault_subtype) {.fault_ipr_subtype=FR_ILL_SLV|mod_fault},
                        "Attempted execution of multics privileged instruction.");
                } else {
                    doFault (FAULT_IPR,
                        (_fault_subtype) {.fault_ipr_subtype=FR_ILL_OP|mod_fault},
                        "Attempted execution of multics privileged instruction.");
                }
            }
            doFault (FAULT_IPR,
                (_fault_subtype) {.fault_ipr_subtype=FR_ILL_SLV|mod_fault},
                "Attempted execution of privileged instruction.");
          }
      }

    if (get_bar_mode())
      if (ci->info->flags & NO_BAR) {
          // lbar
          // ISOLTS 890 06a
          // ISOLTS says that L68 handles this in the same way
          if (ci->opcode == 0230 && !ci->opcodeX) {
            doFault (FAULT_IPR,
                (_fault_subtype) {.fault_ipr_subtype=FR_ILL_SLV|mod_fault},
                "Attempted BAR execution of nonprivileged instruction.");
          } else
            doFault (FAULT_IPR,
                (_fault_subtype) {.fault_ipr_subtype=FR_ILL_OP|mod_fault},
                "Attempted BAR execution of nonprivileged instruction.");
      }

#ifdef DPS8M
    // DPS8M raises it delayed
    if (mod_fault)
      {
        doFault (FAULT_IPR,
                 (_fault_subtype) {.fault_ipr_subtype=mod_fault},
                 "Illegal modifier");
      }
#endif

    ///
    /// executeInstruction: Non-restart processing
    ///                     Initialize address registers
    ///
restart_1:

#if 1
    cpu.TPR.CA = ci->address;
    cpu.iefpFinalAddress = cpu.TPR.CA;
    //cpu.rY = cpu.TPR.CA;
#else
    cpu.iefpFinalAddress = ci->address;
    cpu.rY = ci->address;
#endif



    CPT (cpt2U, 15); // instruction processing
///
/// executeInstruction: Initialize state saving registers
///

    // XXX this may be wrong; make sure that the right value is used
    // if a page fault occurs. (i.e. this may belong above restart_1.
    // This is also used by the SCU instruction. ISOLTS tst887 does
    // a 'SCU n,ad' with a tally of 1; the tally is decremented, setting
    // the IR tally bit as part of the CA calculation; this is not
    // the machine conditions that the SCU instruction is saving.

    ci->stiTally = TST_I_TALLY;   // for sti instruction

///
/// executeInstruction: simh hooks
///

#ifndef SPEED
    // Don't trace Multics idle loop
    if (cpu.PPR.PSR != 061 || cpu.PPR.IC != 0307)

      {
        traceInstruction (DBG_TRACE);
#ifdef HDBG
        hdbgTrace ();
#endif
      }
#endif

///
/// executeInstruction: Initialize misc.
///

    cpu.du.JMP = (word3) info->ndes;
    cpu.dlyFlt = false;
#ifndef XSF_IND
    cpu.cu.XSF = 0;
#endif

///
/// executeInstruction: RPT/RPD/RPL special processing for 'first time'
///

    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        CPT (cpt2U, 15); // RPx processing
//
// RPT:
//
// The computed address, y, of the operand (in the case of R modification) or
// indirect word (in the case of RI modification) is determined as follows:
//
// For the first execution of the repeated instruction:
//      C(C(PPR.IC)+1)0,17 + C(Xn) -> y, y -> C(Xn)
//
// For all successive executions of the repeated instruction:
//      C(Xn) + Delta -> y, y -> C(Xn);
//
//
// RPD:
//
// The computed addresses, y-even and y-odd, of the operands (in the case of
// R modification) or indirect words (in the case of RI modification) are
// determined as follows:
//
// For the first execution of the repeated instruction pair:
//      C(C(PPR.IC)+1)0,17 + C(X-even) -> y-even, y-even -> C(X-even)
//      C(C(PPR.IC)+2)0,17 + C(X-odd) -> y-odd, y-odd -> C(X-odd)
//
// For all successive executions of the repeated instruction pair:
//      if C(X0)8 = 1, then C(X-even) + Delta -> y-even,
//           y-even -> C(X-even);
//      otherwise, C(X-even) -> y-even
//      if C(X0)9 = 1, then C(X-odd) + Delta -> y-odd,
//           y-odd -> C(X-odd);
//      otherwise, C(X-odd) -> y-odd
//
// C(X0)8,9 correspond to control bits A and B, respectively, of the rpd
// instruction word.
//
//
// RL:
//
// The computed address, y, of the operand is determined as follows:
//
// For the first execution of the repeated instruction:
//
//      C(C(PPR.IC)+1)0,17 + C(Xn) -> y, y -> C(Xn)
//
// For all successive executions of the repeated instruction:
//
//      C(Xn) -> y
//
//      if C(Y)0,17 != 0, then C (y)0,17 -> C(Xn);
//
//      otherwise, no change to C(Xn)
//
//  C(Y)0,17 is known as the link address and is the computed address of the
//  next entry in a threaded list of operands to be referenced by the repeated
//  instruction.
//

        sim_debug (DBG_TRACE, & cpu_dev,
                   "RPT/RPD first %d rpt %d rd %d e/o %d X0 %06o a %d b %d\n",
                   cpu.cu.repeat_first, cpu.cu.rpt, cpu.cu.rd, cpu.PPR.IC & 1,
                   cpu.rX[0], !! (cpu.rX[0] & 01000), !! (cpu.rX[0] & 0400));
        sim_debug (DBG_TRACE, & cpu_dev,
                   "RPT/RPD CA %06o\n", cpu.TPR.CA);

// Handle first time of a RPT or RPD

        if (cpu.cu.repeat_first)
          {
            CPT (cpt2U, 16); // RPx first processing
            // The semantics of these are that even is the first instruction of
            // and RPD, and odd the second.

            bool icOdd = !! (cpu.PPR.IC & 1);
            bool icEven = ! icOdd;

            // If RPT or (RPD and the odd instruction)
            if (cpu.cu.rpt || (cpu.cu.rd && icOdd) || cpu.cu.rl)
              cpu.cu.repeat_first = false;

            // a:RJ78/rpd6
            // For the first execution of the repeated instruction:
            // C(C(PPR.IC)+1)0,17 + C(Xn) -> y, y -> C(Xn)
            if (cpu.cu.rpt ||              // rpt
                (cpu.cu.rd && icEven) ||   // rpd & even
                (cpu.cu.rd && icOdd)  ||   // rpd & odd
                cpu.cu.rl)                 // rl
              {
#if 0
                word18 offset;
                if (ci->b29)
                  {
{ static bool first = true;
if (first) {
first = false;
sim_printf ("XXX rethink this; bit 29 is finagled below; should this be done in a different order?\n");
}}
sim_debug (DBG_TRACE, & cpu_dev, "b29, ci->address %o\n", ci->address);
                    // a:RJ78/rpd4
                    offset = SIGNEXT15_18 (ci->address & MASK15);
                  }
                else
                  offset = ci->address;
#else
                word18 offset = ci->address;
#endif
                offset &= AMASK;

                sim_debug (DBG_TRACE, & cpu_dev,
                           "rpt/rd/rl repeat first; offset is %06o\n", offset);

                word6 Td = GET_TD (ci->tag);
                uint Xn = X (Td);  // Get Xn of next instruction
                sim_debug (DBG_TRACE, & cpu_dev,
                           "rpt/rd/rl repeat first; X%d was %06o\n",
                           Xn, cpu.rX[Xn]);
                // a:RJ78/rpd5
                cpu.TPR.CA = (cpu.rX[Xn] + offset) & AMASK;
                cpu.rX[Xn] = cpu.TPR.CA;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "rpt/rd/rl repeat first; X%d now %06o\n",
                           Xn, cpu.rX[Xn]);
              } // rpt or rd or rl

          } // repeat first
      } // cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl

///
/// executeInstruction: EIS operand processing
///

    if (info->ndes > 0)
      {
        CPT (cpt2U, 27); // EIS operand processing
        sim_debug (DBG_APPENDING, &cpu_dev, "initialize EIS descriptors\n");
        // This must not happen on instruction restart
        if (! ci->restart)
          {
            CPT (cpt2U, 28); // EIS not restart
            cpu.du.CHTALLY = 0;
            cpu.du.Z = 1;
          }
        for (uint n = 0; n < info->ndes; n += 1)
          {
            CPT (cpt2U, 29 + n); // EIS operand fetch (29, 30, 31)
// XXX This is a bit of a hack; In general the code is good about
// setting up for bit29 or PR operations by setting up TPR, but
// assumes that the 'else' case can be ignored when it should set
// TPR to the canonical values. Here, in the case of a EIS instruction
// restart after page fault, the TPR is in an unknown state. Ultimately,
// this should not be an issue, as this folderol would be in the DU, and
// we would not be re-executing that code, but until then, set the TPR
// to the condition we know it should be in.
            cpu.TPR.TRR = cpu.PPR.PRR;
            cpu.TPR.TSR = cpu.PPR.PSR;
#if 0
{ static bool first = true;
if (first) {
first = false;
sim_printf ("XXX this had b29 of 0; it may be necessary to clear TSN_VALID[0]\n");
}}
#else
            //Read (cpu.PPR.IC + 1 + n, & cpu.currentEISinstruction.op[n],
                  //INSTRUCTION_FETCH);
            Read (cpu.PPR.IC + 1 + n, & cpu.currentEISinstruction.op[n],
                  OPERAND_READ);
#endif
          }
        PNL (cpu.IWRAddr = cpu.currentEISinstruction.op[0]);
        setupEISoperands ();
      }
    else

///
/// executeInstruction: non-EIS operand processing
///

      {
        CPT (cpt2U, 32); // non-EIS operand processing
        // This must not happen on instruction restart
        //if (! ci->restart)
          //{
            CPT (cpt2U, 33); // not restart non-EIS operand processing
            //if (cpu.isb29)   // if A bit set set-up TPR stuff ...
            if (ci->b29)   // if A bit set set-up TPR stuff ...
              {
                CPT (cpt2U, 34); // B29
                //doPtrReg ();
                word3 n = GET_PRN(IWB_IRODD);  // get PRn
                word15 offset = GET_OFFSET(IWB_IRODD);
                CPTUR (cptUsePRn + n);

                sim_debug (DBG_APPENDING, &cpu_dev,
                           "doPtrReg(): PR[%o] SNR=%05o RNR=%o WORDNO=%06o "
                           "BITNO=%02o\n",
                           n, cpu.PAR[n].SNR, cpu.PAR[n].RNR,
                           cpu.PAR[n].WORDNO, GET_PR_BITNO (n));

                //cpu.cu.TSN_PRNO [0] = n;
                //cpu.cu.TSN_VALID [0] = 1;

                cpu.TPR.CA = (cpu.PAR[n].WORDNO + SIGNEXT15_18 (offset))
                             & MASK18;
                cpu.TPR.TBR = GET_PR_BITNO (n);

                //if (! (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl))
                //  updateIWB (cpu.TPR.CA, GET_TAG (IWB_IRODD));

                cpu.TPR.TSR = cpu.PAR[n].SNR;
                cpu.TPR.TRR = max3 (cpu.PAR[n].RNR, cpu.TPR.TRR, cpu.PPR.PRR);

                sim_debug (DBG_APPENDING, &cpu_dev,
                           "doPtrReg(): n=%o offset=%05o TPR.CA=%06o "
                           "TPR.TBR=%o TPR.TSR=%05o TPR.TRR=%o\n",
                           n, offset, cpu.TPR.CA, cpu.TPR.TBR, 
                           cpu.TPR.TSR, cpu.TPR.TRR);

                set_went_appending ();

// Putting the a29 clear here makes sense, but breaks the emulator for unclear
// reasons (possibly ABSA?). Do it in updateIWB instead
//                ci->a = false;
//                // Don't clear a; it is needed to detect change to appending
//                //  mode
//                //a = false;
//                putbits36_1 (& cpu.cu.IWB, 29, 0);
              }
            else
              {
                CPT (cpt2U, 35); // not B29
                cpu.cu.TSN_VALID [0] = 0;
                cpu.TPR.TBR = 0;
                if (get_addr_mode () == ABSOLUTE_mode)
                  {
                    cpu.TPR.TSR = cpu.PPR.PSR;
                    cpu.TPR.TRR = 0;
                    cpu.RSDWH_R1 = 0;
                  }
                clr_went_appending ();
              }
          //}

        // This must not happen on instruction restart
        if (! ci->restart)
          {
            cpu.cu.CT_HOLD = 0; // Clear interrupted IR mode flag
          }


        //
        // If POT is set, a page fault occured during the fetch of the data word
        // pointed to by an indirect addressing word, and the saved CA points
        // to the data word instead of the indirect word; reset the CA correctly
        //

        if (ci->restart && cpu.cu.pot)
          {
            CPT (cpt2L, 0); // POT set
            cpu.TPR.CA = GET_ADDR (IWB_IRODD);
            if (getbits36_1 (cpu.cu.IWB, 29) != 0)
              cpu.TPR.CA &= MASK15;
          }

#define REORDER
#ifdef REORDER
        if ((ci->info->flags & PREPARE_CA) || WRITEOP (ci) || READOP (ci))
          {
            CPT (cpt2L, 1); // CAF
            doComputedAddressFormation ();
            PNL (L68_ (cpu.AR_F_E = true;))
            cpu.iefpFinalAddress = cpu.TPR.CA;
          }

        //if (READOP (ci) && ! ((bool) (ci->info->flags & TRANSFER_INS)))
        if (READOP (ci))
          {
            CPT (cpt2L, 2); // Read operands
            readOperands ();
            if (cpu.cu.rl)
              {
                switch (OPSIZE ())
                  {
                    case 1:
                      {
                        cpu.lnk = GETHI36 (cpu.CY);
                        cpu.CY &= MASK18;
                        break;
                      }
                    case 2:
                      {
                        cpu.lnk = GETHI36 (cpu.Ypair[0]);
                        cpu.Ypair[0] &= MASK18;
                        break;
                      }
                    default:
                      {
                        break;
                      }
                  }
              }
          }
#else
        if (ci->info->flags & PREPARE_CA)
          {
            doComputedAddressFormation ();
            L68_ (cpu.AR_F_E = true;)
            cpu.iefpFinalAddress = cpu.TPR.CA;
          }
        else if (READOP (ci))
          {
            doComputedAddressFormation ();
            cpu.iefpFinalAddress = cpu.TPR.CA;
            readOperands ();
          }
#endif
        PNL (cpu.IWRAddr = 0);
      }

///
/// executeInstruction: Execute the instruction
///

    t_stat ret = doInstruction ();

///
/// executeInstruction: Write operand
///

    if (WRITEOP (ci))
      {
        CPT (cpt2L, 3); // Write operands
#ifndef REORDER
        if (! READOP (ci))
          {
            doComputedAddressFormation ();
            cpu.iefpFinalAddress = cpu.TPR.CA;
          }
#endif
        writeOperands ();
      }

    else if (ci->info->flags & PREPARE_CA)
      {
        // 'EPP ITS; TRA' confuses the APU by leaving last_cycle 
        // at INDIRECT_WORD_FETCH; defoobarize the APU:
        fauxDoAppendCycle (OPERAND_READ);
#ifdef XSF_IND
        cpu.TPR.TRR = cpu.PPR.PRR;
        cpu.TPR.TSR = cpu.PPR.PSR;
        cpu.TPR.TBR = 0;
#endif
      }

///
/// executeInstruction: Update IT tally
///

    word6 rTAG;
    if (ci->info->flags & NO_TAG) // for instructions line STCA/STCQ
      rTAG = 0;
    else
      rTAG = GET_TAG (cpu.cu.IWB);

    word6 Tm = GET_TM (rTAG);
    word6 Td = GET_TD (rTAG);

    if (info->ndes == 0 /* non-EIS */ &&
        (! (ci->info->flags & NO_TAG)) &&
        Tm == TM_IT && (Td == IT_SC || Td == IT_SCR))
      {
        CPT (cpt2L, 4); // Update IT Tally; fetch indirect word
        //
        // Get the indirect word
        //

        //Read2 (cpu.TPR.CA, cpu.itxPair, INDIRECT_WORD_FETCH);
        Read (cpu.TPR.CA, cpu.itxPair, OPERAND_READ);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "update IT indword=%012"PRIo64"\n", cpu.itxPair[0]);

        //
        // Parse and validate the indirect word
        //

        word18 Yi = GET_ADDR (cpu.itxPair[0]);
        cpu.ou.characterOperandSize = GET_TB (GET_TAG (cpu.itxPair[0]));
        cpu.ou.characterOperandOffset = GET_CF (GET_TAG (cpu.itxPair[0]));

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "update IT size=%o offset=%o Yi=%06o\n",
                   cpu.ou.characterOperandSize, cpu.ou.characterOperandOffset,
                   Yi);

        word12 tally = GET_TALLY (cpu.itxPair[0]);    // 12-bits

        if (Td == IT_SCR)
          {
            CPT (cpt2L, 5); // Update IT Tally; SCR
            // For each reference to the indirect word, the character
            // counter, cf, is reduced by 1 and the TALLY field is
            // increased by 1 before the computed address is formed.
            // Character count arithmetic is modulo 6 for 6-bit characters
            // and modulo 4 for 9-bit bytes. If the character count, cf,
            // underflows to -1, it is reset to 5 for 6-bit characters or
            // to 3 for 9-bit bytes and ADDRESS is reduced by 1. ADDRESS
            // arithmetic is modulo 2^18. TALLY arithmetic is modulo 4096.
            // If the TALLY field overflows to 0, the tally runout
            // indicator is set ON, otherwise it is set OFF. The computed
            // address is the (possibly) decremented value of the ADDRESS
            // field of the indirect word. The effective character/byte
            // number is the decremented value of the character position
            // count, cf, field of the indirect word.

            if (cpu.ou.characterOperandOffset == 0)
              {
                if (cpu.ou.characterOperandSize == TB6)
                    cpu.ou.characterOperandOffset = 5;
                else
                    cpu.ou.characterOperandOffset = 3;
                Yi -= 1;
                Yi &= MASK18;
              }
                else
              {
                cpu.ou.characterOperandOffset -= 1;
              }
            tally ++;
            tally &= 07777; // keep to 12-bits
          }
        else // SC
          {
            CPT (cpt2L, 6); // Update IT Tally; SC
            // For each reference to the indirect word, the character
            // counter, cf, is increased by 1 and the TALLY field is
            // reduced by 1 after the computed address is formed. Character
            // count arithmetic is modulo 6 for 6-bit characters and modulo
            // 4 for 9-bit bytes. If the character count, cf, overflows to
            // 6 for 6-bit characters or to 4 for 9-bit bytes, it is reset
            // to 0 and ADDRESS is increased by 1. ADDRESS arithmetic is
            // modulo 2^18. TALLY arithmetic is modulo 4096. If the TALLY
            // field is reduced to 0, the tally runout indicator is set ON,
            // otherwise it is set OFF.

            cpu.ou.characterOperandOffset ++;

            if (((cpu.ou.characterOperandSize == TB6) &&
                 (cpu.ou.characterOperandOffset > 5)) ||
                ((cpu.ou.characterOperandSize == TB9) &&
                 (cpu.ou.characterOperandOffset > 3)))
              {
                cpu.ou.characterOperandOffset = 0;
                Yi += 1;
                Yi &= MASK18;
              }
            tally --;
            tally &= 07777; // keep to 12-bits
          }


        sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "update IT tally now %o\n", tally);

        SC_I_TALLY (tally == 0);

        cpu.itxPair[0] = (word36) (((word36) Yi << 18) |
                            (((word36) tally & 07777) << 6) |
                            cpu.ou.characterOperandSize |
                            cpu.ou.characterOperandOffset);

//sim_printf ("XXX this has got to be wrong; OPERAND_WRITE?\n");
        //Write (cpu.TPR.CA, indword, INDIRECT_WORD_FETCH);
        Write (cpu.TPR.CA, cpu.itxPair[0], OPERAND_STORE);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "update IT wrote tally word %012"PRIo64" to %06o\n",
                   cpu.itxPair[0], cpu.TPR.CA);
      } // SC/SCR

///
/// executeInstruction: RPT/RPD/RPL processing
///


    // The semantics of these are that even is the first instruction of
    // and RPD, and odd the second.

    bool icOdd = !! (cpu.PPR.IC & 1);
    bool icEven = ! icOdd;

    // Here, repeat_first means that the instruction just executed was the
    // RPT or RPD; but when the even instruction of a RPD is executed,
    // repeat_first is still set, since repeat_first cannot be cleared
    // until the odd instruction gets its first execution. Put some
    // ugly logic in to detect that condition.

    bool rf = cpu.cu.repeat_first;
    if (rf && cpu.cu.rd && icEven)
      rf = false;

    if ((! rf) && (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl))
      {
        CPT (cpt2L, 7); // Post execution RPx
        // If we get here, the instruction just executed was a
        // RPT, RPD or RPL target instruction, and not the RPT or RPD
        // instruction itself

        if (cpu.cu.rpt || cpu.cu.rd)
          {
            // Add delta to index register.

            bool rptA = !! (cpu.rX[0] & 01000);
            bool rptB = !! (cpu.rX[0] & 00400);

            sim_debug (DBG_TRACE, & cpu_dev,
                "RPT/RPD delta first %d rf %d rpt %d rd %d "
                "e/o %d X0 %06o a %d b %d\n",
                cpu.cu.repeat_first, rf, cpu.cu.rpt, cpu.cu.rd, icOdd,
                cpu.rX[0], rptA, rptB);

            if (cpu.cu.rpt) // rpt
              {
                CPT (cpt2L, 8); // RPT delta
                uint Xn = (uint) getbits36_3 (cpu.cu.IWB, 36 - 3);
                cpu.TPR.CA = (cpu.rX[Xn] + cpu.cu.delta) & AMASK;
                cpu.rX[Xn] = cpu.TPR.CA;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "RPT/RPD delta; X%d now %06o\n", Xn, cpu.rX[Xn]);
              }

            // a:RJ78/rpd6
            // We know that the X register is not to be incremented until
            // after both instructions have executed, so the following
            // if uses icOdd instead of the more sensical icEven.
            if (cpu.cu.rd && icOdd && rptA) // rpd, even instruction
              {
                CPT (cpt2L, 9); // RD even
                // a:RJ78/rpd7
                uint Xn = (uint) getbits36_3 (cpu.cu.IWB, 36 - 3);
                cpu.TPR.CA = (cpu.rX[Xn] + cpu.cu.delta) & AMASK;
                cpu.rX[Xn] = cpu.TPR.CA;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "RPT/RPD delta; X%d now %06o\n", Xn, cpu.rX[Xn]);
              }

            if (cpu.cu.rd && icOdd && rptB) // rpdb, odd instruction
              {
                CPT (cpt2L, 10); // RD odd
                // a:RJ78/rpd8
                uint Xn = (uint) getbits36_3 (cpu.cu.IRODD, 36 - 3);
                cpu.TPR.CA = (cpu.rX[Xn] + cpu.cu.delta) & AMASK;
                cpu.rX[Xn] = cpu.TPR.CA;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "RPT/RPD delta; X%d now %06o\n", Xn, cpu.rX[Xn]);
              }
          } // rpt || rd

#if 0
        else if (cpu.cu.rl)
          {
            CPT (cpt2L, 11); // RL
            // C(Xn) -> y
#if 1
            uint Xn = (uint) getbits36_3 (cpu.cu.IWB, 36 - 3);
            word18 lnk = GETHI36 (cpu.CY);
            cpu.CY &= MASK18;
            cpu.rX[Xn] = lnk;
            //putbits36 (& cpu.cu.IWB,  0, 18, lnk);
#else
            uint Xn = (uint) getbits36_3 (cpu.cu.IWB, 36 - 3);
            putbits36 (& cpu.cu.IWB,  0, 18, cpu.rX[Xn]);
#endif
          }
#endif

        // Check for termination conditions.

///////
//
// ISOLTS test 769 claims in test-02a that 'rpt;div' with a divide
// fault should delay the divide fault until after the tremination
// check (it checks that the tally should be decremented) and in test-02b 
// that 'rpl;div' with a divide fault should not due the termination
// check (the tally should not be decremented).
//
// This implies that rpt and rpl are handled differently; as a test
// trying:

#ifdef DPS8M
        if (cpu.cu.rl && cpu.dlyFlt)
#else // L68
        if ((cpu.cu.rl || cpu.cu.rpt || cpu.cu.rd) && cpu.dlyFlt)
#endif
          {
            CPT (cpt2L, 14); // Delayed fault
            doFault (cpu.dlyFltNum, cpu.dlySubFltNum, cpu.dlyCtx);
          }

// Sadly, it fixes ISOLTS 769 test 02a and 02b.
//
///////

        if (cpu.cu.rpt || (cpu.cu.rd && icOdd) || cpu.cu.rl)
          {
            CPT (cpt2L, 12); // RPx termination check
            bool exit = false;
            // The repetition cycle consists of the following steps:
            //  a. Execute the repeated instruction
            //  b. C(X0)0,7 - 1 -> C(X0)0,7
            // a:AL39/rpd9
            uint x = (uint) getbits18 (cpu.rX[0], 0, 8);
            x -= 1;
            x &= MASK8;
            putbits18 (& cpu.rX[0], 0, 8, x);

            //sim_debug (DBG_TRACE, & cpu_dev, "x %03o rX[0] %06o\n", x, rX[0]);

            // a:AL39/rpd10
            //  c. If C(X0)0,7 = 0, then set the tally runout indicator ON
            //     and terminate

            sim_debug (DBG_TRACE, & cpu_dev, "tally %d\n", x);
            if (x == 0)
              {
                sim_debug (DBG_TRACE, & cpu_dev, "tally runout\n");
                SET_I_TALLY;
                exit = true;
              }
            else
              {
                sim_debug (DBG_TRACE, & cpu_dev, "not tally runout\n");
                CLR_I_TALLY;
              }

            //  d. If a terminate condition has been met, then set
            //     the tally runout indicator OFF and terminate

            if (TST_I_ZERO && (cpu.rX[0] & 0100))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is zero terminate\n");
                CLR_I_TALLY;
                exit = true;
              }
            if (!TST_I_ZERO && (cpu.rX[0] & 040))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is not zero terminate\n");
                CLR_I_TALLY;
                exit = true;
              }
            if (TST_I_NEG && (cpu.rX[0] & 020))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is neg terminate\n");
                CLR_I_TALLY;
                exit = true;
              }
            if (!TST_I_NEG && (cpu.rX[0] & 010))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is not neg terminate\n");
                CLR_I_TALLY;
                exit = true;
              }
            if (TST_I_CARRY && (cpu.rX[0] & 04))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is carry terminate\n");
                CLR_I_TALLY;
                exit = true;
              }
            if (!TST_I_CARRY && (cpu.rX[0] & 02))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is not carry terminate\n");
                CLR_I_TALLY;
                exit = true;
              }
            if (TST_I_OFLOW && (cpu.rX[0] & 01))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is overflow terminate\n");
// ISOLTS test ps805 says that on overflow the tally should be set.
                //CLR_I_TALLY;
                SET_I_TALLY;
                exit = true;
              }

            if (exit)
              {
                CPT (cpt2L, 13); // RPx terminated
                cpu.cu.rpt = false;
                cpu.cu.rd = false;
                cpu.cu.rl = false;
              }
            else
              {
                sim_debug (DBG_TRACE, & cpu_dev, "not terminate\n");
              }
          } // if (cpu.cu.rpt || cpu.cu.rd & (cpu.PPR.IC & 1))

        if (cpu.cu.rl)
          {
            CPT (cpt2L, 11); // RL
            if (cpu.lnk == 0)
              {
                CPT (cpt2L, 13); // RPx terminated
                cpu.cu.rpt = false;
                cpu.cu.rd = false;
                cpu.cu.rl = false;
                SET_I_TALLY;
              }
            else
              {
                // C(Xn) -> y
#if 1
                uint Xn = (uint) getbits36_3 (cpu.cu.IWB, 36 - 3);
                //word18 lnk = GETHI36 (cpu.CY);
                //cpu.CY &= MASK18;
                cpu.rX[Xn] = cpu.lnk;
                //putbits36 (& cpu.cu.IWB,  0, 18, lnk);
#else
                uint Xn = (uint) getbits36_3 (cpu.cu.IWB, 36 - 3);
                putbits36 (& cpu.cu.IWB,  0, 18, cpu.rX[Xn]);
#endif
              }
          } // rl
      } // (! rf) && (cpu.cu.rpt || cpu.cu.rd)

    if (cpu.dlyFlt)
      {
        CPT (cpt2L, 14); // Delayed fault
        doFault (cpu.dlyFltNum, cpu.dlySubFltNum, cpu.dlyCtx);
      }
///
/// executeInstruction: simh hooks
///

    sys_stats.total_cycles += 1; // bump cycle counter

    if_sim_debug (DBG_REGDUMP, & cpu_dev)
    {
        sim_debug (DBG_REGDUMPAQI, &cpu_dev,
                   "A=%012"PRIo64" Q=%012"PRIo64" IR:%s\n",
                   cpu.rA, cpu.rQ, dumpFlags (cpu.cu.IR));

#ifndef __MINGW64__
        sim_debug (DBG_REGDUMPFLT, &cpu_dev,
                   "E=%03o A=%012"PRIo64" Q=%012"PRIo64" %.10Lg\n",
                   cpu.rE, cpu.rA, cpu.rQ, EAQToIEEElongdouble ());
#else
        sim_debug (DBG_REGDUMPFLT, &cpu_dev,
                   "E=%03o A=%012"PRIo64" Q=%012"PRIo64" %.10g\n",
                   cpu.rE, cpu.rA, cpu.rQ, EAQToIEEEdouble ());
#endif

        sim_debug (DBG_REGDUMPIDX, &cpu_dev,
                   "X[0]=%06o X[1]=%06o X[2]=%06o X[3]=%06o\n",
                   cpu.rX[0], cpu.rX[1], cpu.rX[2], cpu.rX[3]);
        sim_debug (DBG_REGDUMPIDX, &cpu_dev,
                   "X[4]=%06o X[5]=%06o X[6]=%06o X[7]=%06o\n",
                   cpu.rX[4], cpu.rX[5], cpu.rX[6], cpu.rX[7]);
        for (int n = 0 ; n < 8 ; n++)
        {
            sim_debug (DBG_REGDUMPPR, &cpu_dev,
                       "PR%d/%s: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o ARCHAR:%o ARBITNO:%02o\n",
                       n, PRalias[n], cpu.PR[n].SNR, cpu.PR[n].RNR, 
                       cpu.PR[n].WORDNO, GET_PR_BITNO (n),
                       GET_AR_CHAR (n), GET_AR_BITNO (n));
        }
        sim_debug (DBG_REGDUMPPPR, &cpu_dev,
                   "PRR:%o PSR:%05o P:%o IC:%06o\n",
                   cpu.PPR.PRR, cpu.PPR.PSR, cpu.PPR.P, cpu.PPR.IC);
        sim_debug (DBG_REGDUMPDSBR, &cpu_dev,
                   "ADDR:%08o BND:%05o U:%o STACK:%04o\n",
                   cpu.DSBR.ADDR, cpu.DSBR.BND, cpu.DSBR.U, cpu.DSBR.STACK);
    }

///
/// executeInstruction: done. (Whew!)
///

    return ret;
}

static t_stat DoBasicInstruction (void);
static t_stat DoEISInstruction (void);

static inline void overflow (bool ovf, bool dly, const char * msg)
  {
    CPT (cpt2L, 15); // overflow check
    // If an overflow occured and the repeat instruction is not inhibiting
    // overflow checking.
    if (ovf && chkOVF ())
      {
        SET_I_OFLOW;
        // If overflows are not masked
        if (tstOVFfault ())
          {
            CPT (cpt2L, 16); // overflow 
            // ISOLTS test ps768: Overflows set TRO.
            if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
              {
                SET_I_TALLY;
              }
            if (dly)
              dlyDoFault (FAULT_OFL, (_fault_subtype) {.bits=0}, msg);
            else
              doFault (FAULT_OFL, (_fault_subtype) {.bits=0}, msg);
          }
      }
  }

// Return values
//  CONT_TRA
//  STOP_UNIMP
//  STOP_ILLOP
//  emCall()
//     STOP_HALT
//  scu_sscr()
//     STOP_BUG
//     STOP_WARN
//  scu_rmcm()
//     STOP_BUG
//  scu_smcm()
//  STOP_DIS
//  simh_hooks()
//    hard to document what this can return....
//  0
//

// CANFAULT
static t_stat doInstruction (void)
{
    DCDstruct * i = & cpu.currentInstruction;
    // AL39 says it is always cleared, but that makes no sense (what good
    // is an indicator bit if it is always 0 when you check it?). Clear it if
    // an multiword EIS is at bat.
    // NB: Never clearing it renders Multics unbootable.
    if (i->info->ndes > 0)
      CLR_I_MIF;

#ifdef L68
    cpu.ou.eac = 0;
    cpu.ou.RB1_FULL = 0;
    cpu.ou.RP_FULL = 0;
    cpu.ou.RS_FULL = 0;
    cpu.ou.STR_OP = 0;
    cpu.ou.cycle = 0;
#endif
    PNL (cpu.ou.RS = (word9) i->opcode);
    PNL (L68_ (DU_CYCLE_FDUD;)) // set DU idle
    cpu.skip_cu_hist = false;
    memcpy (& cpu.MR_cache, & cpu.MR, sizeof (cpu.MR_cache));

    t_stat ret =  i->opcodeX ? DoEISInstruction () : DoBasicInstruction ();

    return ret;
}
// CANFAULT
static t_stat DoBasicInstruction (void)
{
    DCDstruct * i = & cpu.currentInstruction;
    uint opcode  = i->opcode;  // get opcode

#ifdef PANEL
    if (insGrp [opcode])
      {
        word8 grp = insGrp [opcode] - 1;
        uint row = grp / 36;
        uint col = grp % 36;
        CPT (cpt3U + row, col); // 3U 0-35, 3L 0-17
      }
#endif
#ifdef L68
    bool is_ou = false;
#endif
    cpu.ou.directOperandFlag = false;
    cpu.ou.directOperand = 0;
    cpu.ou.characterOperandSize = 0;
    cpu.ou.characterOperandOffset = 0;
    cpu.ou.crflag = false;

#ifdef PANEL
    if (NonEISopcodes[i->opcode].reg_use & is_OU)
      {
        is_ou = true;
// XXX Punt on RP FULL, RS FULL
        cpu.ou.RB1_FULL = cpu.ou.RP_FULL = cpu.ou.RS_FULL = 1;
        cpu.ou.cycle |= ou_GIN;
        cpu.ou.opsz = (NonEISopcodes[i->opcode].reg_use >> 12) & 037;
        word10 reguse = (NonEISopcodes[i->opcode].reg_use) & MASK10;
        cpu.ou.reguse = reguse;
        if (reguse & ru_A) CPT (cpt5U, 4);
        if (reguse & ru_Q) CPT (cpt5U, 5);
        if (reguse & ru_X0) CPT (cpt5U, 6);
        if (reguse & ru_X1) CPT (cpt5U, 7);
        if (reguse & ru_X2) CPT (cpt5U, 8);
        if (reguse & ru_X3) CPT (cpt5U, 9);
        if (reguse & ru_X4) CPT (cpt5U, 10);
        if (reguse & ru_X5) CPT (cpt5U, 11);
        if (reguse & ru_X6) CPT (cpt5U, 12);
        if (reguse & ru_X7) CPT (cpt5U, 13);
      }
#endif

    switch (opcode)
      {
        ///    FIXED-POINT ARITHMETIC INSTRUCTIONS

        /// Fixed-Point Data Movement Load

        case 0635:  // eaa
          cpu.rA = 0;
          SETHI (cpu.rA, cpu.TPR.CA);

          SC_I_ZERO (cpu.TPR.CA == 0);
          SC_I_NEG (cpu.TPR.CA & SIGN18);

          break;

        case 0636:  // eaq
          cpu.rQ = 0;
          SETHI (cpu.rQ, cpu.TPR.CA);

          SC_I_ZERO (cpu.TPR.CA == 0);
          SC_I_NEG (cpu.TPR.CA & SIGN18);

          break;

        case 0620:  // eax0
        case 0621:  // eax1
        case 0622:  // eax2
        case 0623:  // eax3
        case 0624:  // eax4
        case 0625:  // eax5
        case 0626:  // eax6
        case 0627:  // eax7
          {
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = cpu.TPR.CA;

            SC_I_ZERO (cpu.TPR.CA == 0);
            SC_I_NEG (cpu.TPR.CA & SIGN18);

          }
          break;

        case 0335:  // lca
          {
            bool ovf;
            cpu.rA = compl36 (cpu.CY, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "lca overflow fault");
          }
          break;

        case 0336:  // lcq
          {
            bool ovf;
            cpu.rQ = compl36 (cpu.CY, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "lcq overflow fault");
          }
          break;

        case 0320:  // lcx0
        case 0321:  // lcx1
        case 0322:  // lcx2
        case 0323:  // lcx3
        case 0324:  // lcx4
        case 0325:  // lcx5
        case 0326:  // lcx6
        case 0327:  // lcx7
          {
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = compl18 (GETHI (cpu.CY), & cpu.cu.IR, & ovf);
            overflow (ovf, false, "lcxn overflow fault");
          }
          break;

        case 0337:  // lcaq
          {
            // The lcaq instruction changes the number to its negative while
            // moving it from Y-pair to AQ. The operation is executed by
            // forming the twos complement of the string of 72 bits. In twos
            // complement arithmetic, the value 0 is its own negative. An
            // overflow condition exists if C(Y-pair) = -2**71.


            if (cpu.Ypair[0] == 0400000000000LL && cpu.Ypair[1] == 0)
              {
                cpu.rA = cpu.Ypair[0];
                cpu.rQ = cpu.Ypair[1];
                SET_I_NEG;
                CLR_I_ZERO;
                overflow (true, false, "lcaq overflow fault");
              }
            else if (cpu.Ypair[0] == 0 && cpu.Ypair[1] == 0)
              {
                cpu.rA = 0;
                cpu.rQ = 0;

                SET_I_ZERO;
                CLR_I_NEG;
              }
            else
              {
                word72 tmp72 = (((word72) (cpu.Ypair[0] & MASK36)) << 36) |
                               (cpu.Ypair[1] & MASK36);
                tmp72 = ~tmp72 + 1;
                cpu.rA = GETHI72 (tmp72); 
                cpu.rQ = GETLO72 (tmp72);

                SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
                SC_I_NEG (cpu.rA & SIGN36);
              }
          }
          break;

        case 0235:  // lda
          cpu.rA = cpu.CY;
          SC_I_ZERO (cpu.rA == 0);
          SC_I_NEG (cpu.rA & SIGN36);
          break;

        case 0034: // ldac
          cpu.rA = cpu.CY;
          SC_I_ZERO (cpu.rA == 0);
          SC_I_NEG (cpu.rA & SIGN36);
          cpu.CY = 0;
          break;

        case 0237:  // ldaq
          cpu.rA = cpu.Ypair[0];
          cpu.rQ = cpu.Ypair[1];
          SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0)
          SC_I_NEG (cpu.rA & SIGN36);
          break;

        case 0634:  // ldi
          {
            CPTUR (cptUseIR);
            // C(Y)18,31 -> C(IR)

            // Indicators:
            //  Parity Mask:
            //      If C(Y)27 = 1, and the processor is in absolute or
            //      instruction privileged mode, then ON; otherwise OFF.
            //      This indicator is not affected in the normal or BAR modes.
            //  Not BAR mode:
            //      Cannot be changed by the ldi instruction
            //  MIF:
            //      If C(Y)30 = 1, and the processor is in absolute or
            //      instruction privileged mode, then ON; otherwise OFF.
            //      This indicator is not affected in normal or BAR modes.
            //  Absolute mode:
            //      Cannot be changed by the ldi instruction
            //  All others: If corresponding bit in C(Y) is 1, then ON;
            //  otherwise, OFF

            // upper 14-bits of lower 18-bits

            // AL39 ldi says that HEX is ignored, but the mode register 
            // description says that it isn't
#ifdef DPS8M
            word18 tmp18 = GETLO (cpu.CY) & 0777770;
#endif
#ifdef L68
            word18 tmp18 = GETLO (cpu.CY) & 0777760;
#endif

            bool bAbsPriv = (get_addr_mode () == ABSOLUTE_mode) ||
                            is_priv_mode ();

            SC_I_ZERO  (tmp18 & I_ZERO);
            SC_I_NEG   (tmp18 & I_NEG);
            SC_I_CARRY (tmp18 & I_CARRY);
            SC_I_OFLOW (tmp18 & I_OFLOW);
            SC_I_EOFL  (tmp18 & I_EOFL);
            SC_I_EUFL  (tmp18 & I_EUFL);
            SC_I_OMASK (tmp18 & I_OMASK);
            SC_I_TALLY (tmp18 & I_TALLY);
            SC_I_PERR  (tmp18 & I_PERR);
            // I_PMASK handled below
            // LDI cannot change I_NBAR
            SC_I_TRUNC (tmp18 & I_TRUNC);
            // I_MIF handled below
            // LDI cannot change I_ABS
#ifdef DPS8M
            SC_I_HEX  (tmp18 & I_HEX);
#endif

#if 0
            cpu.bar_attempt = false;

#endif
            if (bAbsPriv)
              {
                SC_I_PMASK (tmp18 & I_PMASK);
                SC_I_MIF (tmp18 & I_MIF);
#if 0
                if (! (tmp18 & I_NBAR))
                  {
                    cpu.bar_attempt = true;
                  }
#endif
              }
            else
              {
                CLR_I_PMASK;
                CLR_I_MIF;
              }
          }
          break;

        case 0236:  // ldq
          cpu.rQ = cpu.CY;
          SC_I_ZERO (cpu.rQ == 0);
          SC_I_NEG (cpu.rQ & SIGN36);
          break;

        case 0032: // ldqc
          cpu.rQ = cpu.CY;
          SC_I_ZERO (cpu.rQ == 0);
          SC_I_NEG (cpu.rQ & SIGN36);
          cpu.CY = 0;
          break;

        case 0220:  // ldx0
        case 0221:  // ldx1
        case 0222:  // ldx2
        case 0223:  // ldx3
        case 0224:  // ldx4
        case 0225:  // ldx5
        case 0226:  // ldx6
        case 0227:  // ldx7
          {
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = GETHI (cpu.CY);
            SC_I_ZERO (cpu.rX[n] == 0);
            SC_I_NEG (cpu.rX[n] & SIGN18);
          }
          break;

        case 0073:   // lreg
          CPTUR (cptUseE);
#ifdef L68
          cpu.ou.cycle |= ou_GOS;
          cpu.ou.eac = 0;
#endif
          cpu.rX[0] = GETHI (cpu.Yblock8[0]);
          cpu.rX[1] = GETLO (cpu.Yblock8[0]);
#ifdef L68
          cpu.ou.eac ++;
#endif
          cpu.rX[2] = GETHI (cpu.Yblock8[1]);
          cpu.rX[3] = GETLO (cpu.Yblock8[1]);
#ifdef L68
          cpu.ou.eac ++;
#endif
          cpu.rX[4] = GETHI (cpu.Yblock8[2]);
          cpu.rX[5] = GETLO (cpu.Yblock8[2]);
#ifdef L68
          cpu.ou.eac ++;
#endif
          cpu.rX[6] = GETHI (cpu.Yblock8[3]);
          cpu.rX[7] = GETLO (cpu.Yblock8[3]);
#ifdef L68
          cpu.ou.eac = 0;
#endif
          cpu.rA = cpu.Yblock8[4];
          cpu.rQ = cpu.Yblock8[5];
          cpu.rE = (GETHI (cpu.Yblock8[6]) >> 10) & 0377;   // need checking
          break;

        case 0720:  // lxl0
        case 0721:  // lxl1
        case 0722:  // lxl2
        case 0723:  // lxl3
        case 0724:  // lxl4
        case 0725:  // lxl5
        case 0726:  // lxl6
        case 0727:  // lxl7
          {
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = GETLO (cpu.CY);
            SC_I_ZERO (cpu.rX[n] == 0);
            SC_I_NEG (cpu.rX[n] & SIGN18);
          }
          break;

        /// Fixed-Point Data Movement Store

        case 0753:  // sreg
          CPTUR (cptUseE);
          CPTUR (cptUseRALR);
          // clear block (changed to memset() per DJ request)
          //memset (cpu.Yblock8, 0, sizeof (cpu.Yblock8));
#ifdef L68
          cpu.ou.cycle |= ou_GOS;
          cpu.ou.eac = 0;
#endif
          SETHI (cpu.Yblock8[0], cpu.rX[0]);
          SETLO (cpu.Yblock8[0], cpu.rX[1]);
#ifdef L68
          cpu.ou.eac ++;
#endif
          SETHI (cpu.Yblock8[1], cpu.rX[2]);
          SETLO (cpu.Yblock8[1], cpu.rX[3]);
#ifdef L68
          cpu.ou.eac ++;
#endif
          SETHI (cpu.Yblock8[2], cpu.rX[4]);
          SETLO (cpu.Yblock8[2], cpu.rX[5]);
#ifdef L68
          cpu.ou.eac ++;
#endif
          SETHI (cpu.Yblock8[3], cpu.rX[6]);
          SETLO (cpu.Yblock8[3], cpu.rX[7]);
#ifdef L68
          cpu.ou.eac = 0;
#endif
          cpu.Yblock8[4] = cpu.rA;
          cpu.Yblock8[5] = cpu.rQ;
          cpu.Yblock8[6] = ((word36)(cpu.rE & MASK8)) << 28;
          cpu.Yblock8[7] = ((cpu.rTR & MASK27) << 9) | (cpu.rRALR & 07);
          break;

        case 0755:  // sta
          cpu.CY = cpu.rA;
          break;

        case 0354:  // stac
          if (cpu.CY == 0)
            {
              SET_I_ZERO;
              cpu.CY = cpu.rA;
            }
          else
            CLR_I_ZERO;
          break;

        case 0654:  // stacq
          if (cpu.CY == cpu.rQ)
            {
              cpu.CY = cpu.rA;
              SET_I_ZERO;
            }
          else
            CLR_I_ZERO;
          break;

        case 0757:  // staq
          cpu.Ypair[0] = cpu.rA;
          cpu.Ypair[1] = cpu.rQ;
          break;

        case 0551:  // stba
          // 9-bit bytes of C(A) -> corresponding bytes of C(Y), the byte
          // positions affected being specified in the TAG field.
          copyBytes ((i->tag >> 2) & 0xf, cpu.rA, &cpu.CY);
          cpu.ou.crflag = true;
          break;

        case 0552:  // stbq
          // 9-bit bytes of C(Q) -> corresponding bytes of C(Y), the byte
          // positions affected being specified in the TAG field.
          copyBytes ((i->tag >> 2) & 0xf, cpu.rQ, &cpu.CY);
          cpu.ou.crflag = true;
          break;

        case 0554:  // stc1
          // "C(Y)25 reflects the state of the tally runout indicator
          // prior to modification.
          SETHI (cpu.CY, (cpu.PPR.IC + 1) & MASK18);
          // AL39 stc1 says that HEX is ignored, but the mode register 
          // description says that it isn't
#ifdef DPS8M
          SETLO (cpu.CY, cpu.cu.IR & 0777770);
#endif
#ifdef L68
          SETLO (cpu.CY, cpu.cu.IR & 0777760);
#endif
          SCF (i->stiTally, cpu.CY, I_TALLY);
          break;

        case 0750:  // stc2
          // AL-39 doesn't specify if the low half is set to zero,
          // set to IR, or left unchanged
          // RJ78 specifies unchanged
          SETHI (cpu.CY, (cpu.PPR.IC + 2) & MASK18);
          break;

        case 0751: // stca
          // Characters of C(A) -> corresponding characters of C(Y),
          // the character positions affected being specified in the TAG
          // field.
          copyChars (i->tag, cpu.rA, &cpu.CY);
          cpu.ou.crflag = true;
          break;

        case 0752: // stcq
          // Characters of C(Q) -> corresponding characters of C(Y), the
          // character positions affected being specified in the TAG field.
          copyChars (i->tag, cpu.rQ, &cpu.CY);
          cpu.ou.crflag = true;
          break;

        case 0357: //< stcd
          // C(PPR) -> C(Y-pair) as follows:

          //  000 -> C(Y-pair)0,2
          //  C(PPR.PSR) -> C(Y-pair)3,17
          //  C(PPR.PRR) -> C(Y-pair)18,20
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35

          //  C(PPR.IC)+2 -> C(Y-pair)36,53
          //  00...0 -> C(Y-pair)54,71

          // ISOLTS 880 5a has an STCD in an XED in a fault pair;
          // it reports the wrong ring number. This was fixed by
          // emulating the SCU instruction (different behavior in fault
          // pair).

          if (cpu.cycle == EXEC_cycle)
            {
              sim_debug (DBG_CAC, & cpu_dev, "stcd exec PRR %o\n", cpu.PPR.PRR);
              cpu.Ypair[0] = 0;
              putbits36_15 (& cpu.Ypair[0],  3, cpu.PPR.PSR);
              putbits36_3  (& cpu.Ypair[0], 18, cpu.PPR.PRR);
              putbits36_6  (& cpu.Ypair[0], 30, 043);
              sim_debug (DBG_CAC, & cpu_dev, "stcd Y0 %012llo\n", cpu.Ypair[0]);

              cpu.Ypair[1] = 0;
              putbits36_18 (& cpu.Ypair[1],  0, cpu.PPR.IC + 2);
            }
          else
            {
              sim_debug (DBG_CAC, & cpu_dev, "stcd fault PRR %o\n", cpu.PPR.PRR);
              cpu.Ypair[0] = 0;
              putbits36_15 (& cpu.Ypair[0],  3, cpu.cu_data.PSR);
              putbits36_3  (& cpu.Ypair[0], 18, cpu.cu_data.PRR);
              //putbits36_6  (& cpu.Ypair[0], 30, 043);
              sim_debug (DBG_CAC, & cpu_dev, "stcd Y0 %012llo\n", cpu.Ypair[0]);

              cpu.Ypair[1] = 0;
              putbits36_18 (& cpu.Ypair[1],  0, cpu.cu_data.IC + 2);
            }
          break;


        case 0754: // sti

          // C(IR) -> C(Y)18,31
          // 00...0 -> C(Y)32,35

          // The contents of the indicator register after address
          // preparation are stored in C(Y)18,31  C(Y)18,31 reflects the
          // state of the tally runout indicator prior to address
          // preparation. The relation between C(Y)18,31 and the indicators
          // is given in Table 4-5.

          CPTUR (cptUseIR);
            // AL39 sti says that HEX is ignored, but the mode register 
            // description says that it isn't
#ifdef DPS8M
          SETLO (cpu.CY, (cpu.cu.IR & 0000000777770LL));
#endif
#ifdef L68
          SETLO (cpu.CY, (cpu.cu.IR & 0000000777760LL));
#endif
          SCF (i->stiTally, cpu.CY, I_TALLY);
          break;

        case 0756: // stq
          cpu.CY = cpu.rQ;
          break;

        case 0454:  // stt
          CPTUR (cptUseTR);
#ifdef ISOLTS
          if (currentRunningCPUnum)
            cpu.CY = ((-- cpu.shadowTR) & MASK27) << 9;
          else
            cpu.CY = (cpu.rTR & MASK27) << 9;
#else
          cpu.CY = (cpu.rTR & MASK27) << 9;
#endif
          break;


        case 0740:  // stx0
        case 0741:  // stx1
        case 0742:  // stx2
        case 0743:  // stx3
        case 0744:  // stx4
        case 0745:  // stx5
        case 0746:  // stx6
        case 0747:  // stx7
          {
            uint32 n = opcode & 07;  // get n
            SETHI (cpu.CY, cpu.rX[n]);
          }
          break;

        case 0450: // stz
          cpu.CY = 0;
          break;

        case 0440:  // sxl0
        case 0441:  // sxl1
        case 0442:  // sxl2
        case 0443:  // sxl3
        case 0444:  // sxl4
        case 0445:  // sxl5
        case 0446:  // sxl6
        case 0447:  // sxl7
          SETLO (cpu.CY, cpu.rX[opcode & 07]);
          break;

        /// Fixed-Point Data Movement Shift

        case 0775:  // alr
          {
              word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
              for (uint j = 0 ; j < tmp36 ; j++)
              {
                  bool a0 = cpu.rA & SIGN36;    // A0
                  cpu.rA <<= 1;               // shift left 1
                  if (a0)                 // rotate A0 -> A35
                      cpu.rA |= 1;
              }
              cpu.rA &= DMASK;    // keep to 36-bits

              SC_I_ZERO (cpu.rA == 0);
              SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0735:  // als
          {
            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17

            word36 tmpSign = cpu.rA & SIGN36;
            CLR_I_CARRY;

            for (uint j = 0; j < tmp36; j ++)
              {
                cpu.rA <<= 1;
                if (tmpSign != (cpu.rA & SIGN36))
                  SET_I_CARRY;
              }
            cpu.rA &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0771:  // arl
          // Shift C(A) right the number of positions given in
          // C(TPR.CA)11,17; filling vacated positions with zeros.
          {
            cpu.rA &= DMASK; // Make sure the shifted in bits are 0
            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17

            cpu.rA >>= tmp36;
            cpu.rA &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0731:  // ars
          {
            // Shift C(A) right the number of positions given in
            // C(TPR.CA)11,17; filling vacated positions with initial C(A)0.

            cpu.rA &= DMASK; // Make sure the shifted in bits are 0
            word18 tmp18 = cpu.TPR.CA & 0177;   // CY bits 11-17

            bool a0 = cpu.rA & SIGN36;    // A0
            for (uint j = 0 ; j < tmp18 ; j ++)
              {
                cpu.rA >>= 1;               // shift right 1
                if (a0)                 // propagate sign bit
                    cpu.rA |= SIGN36;
              }
            cpu.rA &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0777:  // llr
          // Shift C(AQ) left by the number of positions given in
          // C(TPR.CA)11,17; entering each bit leaving AQ0 into AQ71.

          {
            word36 tmp36 = cpu.TPR.CA & 0177;      // CY bits 11-17
            for (uint j = 0 ; j < tmp36 ; j++)
              {
                bool a0 = cpu.rA & SIGN36;         // A0

                cpu.rA <<= 1;                      // shift left 1

                bool b0 = cpu.rQ & SIGN36;         // Q0
                if (b0)
                  cpu.rA |= 1;                     // Q0 => A35

                cpu.rQ <<= 1;                      // shift left 1

                if (a0)                            // propagate A sign bit
                  cpu.rQ |= 1;
              }

            cpu.rA &= DMASK;    // keep to 36-bits
            cpu.rQ &= DMASK;

            SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0737:  // lls
          {
            // Shift C(AQ) left the number of positions given in
            // C(TPR.CA)11,17; filling vacated positions with zeros.

            CLR_I_CARRY;

            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
            word36 tmpSign = cpu.rA & SIGN36;
            for (uint j = 0 ; j < tmp36 ; j ++)
              {
                cpu.rA <<= 1;               // shift left 1

                if (tmpSign != (cpu.rA & SIGN36))
                  SET_I_CARRY;

                bool b0 = cpu.rQ & SIGN36;    // Q0
                if (b0)
                  cpu.rA |= 1;            // Q0 => A35

                cpu.rQ <<= 1;               // shift left 1
              }

            cpu.rA &= DMASK;    // keep to 36-bits
            cpu.rQ &= DMASK;

            SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0773:  // lrl
          // Shift C(AQ) right the number of positions given in
          // C(TPR.CA)11,17; filling vacated positions with zeros.
          {
            cpu.rA &= DMASK; // Make sure the shifted in bits are 0
            cpu.rQ &= DMASK; // Make sure the shifted in bits are 0
            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
            for (uint j = 0 ; j < tmp36 ; j++)
              {
                bool a35 = cpu.rA & 1;      // A35
                cpu.rA >>= 1;               // shift right 1

                cpu.rQ >>= 1;               // shift right 1

                if (a35)                // propagate sign bit
                  cpu.rQ |= SIGN36;
              }
            cpu.rA &= DMASK;    // keep to 36-bits
            cpu.rQ &= DMASK;

            SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0733:  // lrs
          {
            // Shift C(AQ) right the number of positions given in
            // C(TPR.CA)11,17; filling vacated positions with initial C(AQ)0.

            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
            cpu.rA &= DMASK; // Make sure the shifted in bits are 0
            cpu.rQ &= DMASK; // Make sure the shifted in bits are 0
            bool a0 = cpu.rA & SIGN36;    // A0

            for (uint j = 0 ; j < tmp36 ; j ++)
              {
                bool a35 = cpu.rA & 1;      // A35

                cpu.rA >>= 1;               // shift right 1
                if (a0)
                  cpu.rA |= SIGN36;

                cpu.rQ >>= 1;               // shift right 1
                if (a35)                // propagate sign bit1
                  cpu.rQ |= SIGN36;
              }
            cpu.rA &= DMASK;    // keep to 36-bits (probably ain't necessary)
            cpu.rQ &= DMASK;

            SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0776:  // qlr
          // Shift C(Q) left the number of positions given in
          // C(TPR.CA)11,17; entering each bit leaving Q0 into Q35.
          {
            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
            for (uint j = 0 ; j < tmp36 ; j++)
              {
                bool q0 = cpu.rQ & SIGN36;    // Q0
                cpu.rQ <<= 1;               // shift left 1
                if (q0)                 // rotate A0 -> A35
                  cpu.rQ |= 1;
              }
            cpu.rQ &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rQ == 0);
            SC_I_NEG (cpu.rQ & SIGN36);
          }
          break;

        case 0736:  // qls
          // Shift C(Q) left the number of positions given in
          // C(TPR.CA)11,17; fill vacated positions with zeros.
          {
            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
            word36 tmpSign = cpu.rQ & SIGN36;
            CLR_I_CARRY;

            for (uint j = 0; j < tmp36; j ++)
              {
                cpu.rQ <<= 1;
                if (tmpSign != (cpu.rQ & SIGN36))
                  SET_I_CARRY;
              }
            cpu.rQ &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rQ == 0);
            SC_I_NEG (cpu.rQ & SIGN36);
          }
          break;

        case 0772:  // qrl
          // Shift C(Q) right the number of positions specified by
          // Y11,17; fill vacated positions with zeros.
          {
            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17

            cpu.rQ &= DMASK;    // Make sure the shifted in bits are 0
            cpu.rQ >>= tmp36;
            cpu.rQ &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rQ == 0);
            SC_I_NEG (cpu.rQ & SIGN36);

          }
          break;

        case 0732:  // qrs
          {
            // Shift C(Q) right the number of positions given in
            // C(TPR.CA)11,17; filling vacated positions with initial C(Q)0.

            cpu.rQ &= DMASK; // Make sure the shifted in bits are 0
            word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
            bool q0 = cpu.rQ & SIGN36;    // Q0
            for (uint j = 0 ; j < tmp36 ; j++)
              {
                cpu.rQ >>= 1;               // shift right 1
                if (q0)                 // propagate sign bit
                  cpu.rQ |= SIGN36;
              }
            cpu.rQ &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rQ == 0);
            SC_I_NEG (cpu.rQ & SIGN36);
          }
          break;

        /// Fixed-Point Addition

        case 0075:  // ada
          {
            // C(A) + C(Y) -> C(A)
            // Modifications: All
            //
            //  (Indicators not listed are not affected)
            //  ZERO: If C(A) = 0, then ON; otherwise OFF
            //  NEG: If C(A)0 = 1, then ON; otherwise OFF
            //  OVR: If range of A is exceeded, then ON
            //  CARRY: If a carry out of A0 is generated, then ON; otherwise OFF

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rA = Add36b (cpu.rA, cpu.CY, 0, I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "ada overflow fault");
          }
          break;

        case 0077:   // adaq
          {
            // C(AQ) + C(Y-pair) -> C(AQ)
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNOC, & cpu.cu.IR, & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            overflow (ovf, false, "adaq overflow fault");
          }
          break;

        case 0033:   // adl
          {
            // C(AQ) + C(Y) sign extended -> C(AQ)
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            word72 tmp72 = SIGNEXT36_72 (cpu.CY); // sign extend Cy
            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNOC,
                            & cpu.cu.IR, & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            overflow (ovf, false, "adl overflow fault");
          }
          break;


        case 0037:   // adlaq
          {
            // The adlaq instruction is identical to the adaq instruction with
            // the exception that the overflow indicator is not affected by the
            // adlaq instruction, nor does an overflow fault occur. Operands
            // and results are treated as unsigned, positive binary integers.
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);

            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNC, & cpu.cu.IR, & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
          }
          break;

        case 0035:   // adla
          {
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            // The adla instruction is identical to the ada instruction with
            // the exception that the overflow indicator is not affected by the
            // adla instruction, nor does an overflow fault occur. Operands and
            // results are treated as unsigned, positive binary integers. */

            bool ovf;
            cpu.rA = Add36b (cpu.rA, cpu.CY, 0, I_ZNC, & cpu.cu.IR, & ovf);
          }
          break;

        case 0036:   // adlq
          {
            // The adlq instruction is identical to the adq instruction with
            // the exception that the overflow indicator is not affected by the
            // adlq instruction, nor does an overflow fault occur. Operands and
            // results are treated as unsigned, positive binary integers. */

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rQ = Add36b (cpu.rQ, cpu.CY, 0, I_ZNC, & cpu.cu.IR, & ovf);
          }
          break;

        case 0020:   // adlx0
        case 0021:   // adlx1
        case 0022:   // adlx2
        case 0023:   // adlx3
        case 0024:   // adlx4
        case 0025:   // adlx5
        case 0026:   // adlx6
        case 0027:   // adlx7
          {
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = Add18b (cpu.rX[n], GETHI (cpu.CY), 0, I_ZNC,
                             & cpu.cu.IR, & ovf);
          }
          break;

        case 0076:   // adq
          {
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rQ = Add36b (cpu.rQ, cpu.CY, 0, I_ZNOC,
                                 & cpu.cu.IR, & ovf);
            overflow (ovf, false, "adq overflow fault");
          }
          break;

        case 0060:   // adx0
        case 0061:   // adx1
        case 0062:   // adx2
        case 0063:   // adx3
        case 0064:   // adx4
        case 0065:   // adx5
        case 0066:   // adx6
        case 0067:   // adx7
          {
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = Add18b (cpu.rX[n], GETHI (cpu.CY), 0,
                                 I_ZNOC,
                                 & cpu.cu.IR, & ovf);
            overflow (ovf, false, "adxn overflow fault");
          }
          break;

        case 0054:   // aos
          {
            // C(Y)+1->C(Y)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.CY = Add36b (cpu.CY, 1, 0, I_ZNOC,
                                 & cpu.cu.IR, & ovf);
            overflow (ovf, true, "aos overflow fault");
          }
          break;

        case 0055:   // asa
          {
            // C(A) + C(Y) -> C(Y)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.CY = Add36b (cpu.rA, cpu.CY, 0, I_ZNOC,
                             & cpu.cu.IR, & ovf);
            overflow (ovf, true, "asa overflow fault");
          }
          break;

        case 0056:   // asq
          {
            // C(Q) + C(Y) -> C(Y)
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.CY = Add36b (cpu.rQ, cpu.CY, 0, I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, true, "asq overflow fault");
          }
          break;

        case 0040:   // asx0
        case 0041:   // asx1
        case 0042:   // asx2
        case 0043:   // asx3
        case 0044:   // asx4
        case 0045:   // asx5
        case 0046:   // asx6
        case 0047:   // asx7
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            //    C(Xn) + C(Y)0,17 -> C(Y)0,17
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = Add18b (cpu.rX[n], GETHI (cpu.CY), 0,
                                   I_ZNOC, & cpu.cu.IR, & ovf);
            SETHI (cpu.CY, tmp18);
            overflow (ovf, true, "asxn overflow fault");
          }
          break;

        case 0071:   // awca
          {
            // If carry indicator OFF, then C(A) + C(Y) -> C(A)
            // If carry indicator ON, then C(A) + C(Y) + 1 -> C(A)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rA = Add36b (cpu.rA, cpu.CY, TST_I_CARRY ? 1 : 0,
                                 I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "awca overflow fault");
          }
          break;

        case 0072:   // awcq
          {
            // If carry indicator OFF, then C(Q) + C(Y) -> C(Q)
            // If carry indicator ON, then C(Q) + C(Y) + 1 -> C(Q)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rQ = Add36b (cpu.rQ, cpu.CY, TST_I_CARRY ? 1 : 0,
                             I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "awcq overflow fault");
          }
          break;

        /// Fixed-Point Subtraction

        case 0175:  // sba
          {
            // C(A) - C(Y) -> C(A)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rA = Sub36b (cpu.rA, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "sba overflow fault");
          }
          break;

        case 0177:  // sbaq
          {
            // C(AQ) - C(Y-pair) -> C(AQ)
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            tmp72 = Sub72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 1,
                            I_ZNOC, & cpu.cu.IR,
                            & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            overflow (ovf, false, "sbaq overflow fault");
          }
          break;

        case 0135:  // sbla
          {
            // C(A) - C(Y) -> C(A) logical

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rA = Sub36b (cpu.rA, cpu.CY, 1, I_ZNC, & cpu.cu.IR, & ovf);
          }
          break;

        case 0137:  // sblaq
          {
            // The sblaq instruction is identical to the sbaq instruction with
            // the exception that the overflow indicator is not affected by the
            // sblaq instruction, nor does an overflow fault occur. Operands
            // and results are treated as unsigned, positive binary integers.
            // C(AQ) - C(Y-pair) -> C(AQ)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);

            tmp72 = Sub72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 1,
                            I_ZNC, & cpu.cu.IR, & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
          }
          break;

        case 0136:  // sblq
          {
            // C(Q) - C(Y) -> C(Q)
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rQ = Sub36b (cpu.rQ, cpu.CY, 1, I_ZNC, & cpu.cu.IR, & ovf);
          }
          break;

        case 0120:  // sblx0
        case 0121:  // sblx1
        case 0122:  // sblx2
        case 0123:  // sblx3
        case 0124:  // sblx4
        case 0125:  // sblx5
        case 0126:  // sblx6
        case 0127:  // sblx7
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            // C(Xn) - C(Y)0,17 -> C(Xn)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = Sub18b (cpu.rX[n], GETHI (cpu.CY), 1,
                             I_ZNC, & cpu.cu.IR, & ovf);
          }
          break;

        case 0176:  // sbq
          {
            // C(Q) - C(Y) -> C(Q)
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rQ = Sub36b (cpu.rQ, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "sbq overflow fault");
          }
          break;

        case 0160:  // sbx0
        case 0161:  // sbx1
        case 0162:  // sbx2
        case 0163:  // sbx3
        case 0164:  // sbx4
        case 0165:  // sbx5
        case 0166:  // sbx6
        case 0167:  // sbx7
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            // C(Xn) - C(Y)0,17 -> C(Xn)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] = Sub18b (cpu.rX[n], GETHI (cpu.CY), 1,
                                 I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "sbxn overflow fault");
          }
          break;

        case 0155:  // ssa
          {
            // C(A) - C(Y) -> C(Y)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.CY = Sub36b (cpu.rA, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, true, "ssa overflow fault");
          }
          break;

        case 0156:  // ssq
          {
            // C(Q) - C(Y) -> C(Y)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.CY = Sub36b (cpu.rQ, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, true, "ssq overflow fault");
          }
          break;

        case 0140:  // ssx0
        case 0141:  // ssx1
        case 0142:  // ssx2
        case 0143:  // ssx3
        case 0144:  // ssx4
        case 0145:  // ssx5
        case 0146:  // ssx6
        case 0147:  // ssx7
          {
            // For uint32 n = 0, 1, ..., or 7 as determined by operation code
            // C(Xn) - C(Y)0,17 -> C(Y)0,17

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = Sub18b (cpu.rX[n], GETHI (cpu.CY), 1,
                                   I_ZNOC, & cpu.cu.IR, & ovf);
            SETHI (cpu.CY, tmp18);
            overflow (ovf, true, "ssxn overflow fault");
          }
          break;


        case 0171:  // swca
          {
            // If carry indicator ON, then C(A)- C(Y) -> C(A)
            // If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rA = Sub36b (cpu.rA, cpu.CY, TST_I_CARRY ? 1 : 0,
                             I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "swca overflow fault");
          }
          break;

        case 0172:  // swcq
          {
            // If carry indicator ON, then C(Q) - C(Y) -> C(Q)
            // If carry indicator OFF, then C(Q) - C(Y) - 1 -> C(Q)

#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            bool ovf;
            cpu.rQ = Sub36b (cpu.rQ, cpu.CY, TST_I_CARRY ? 1 : 0,
                                 I_ZNOC, & cpu.cu.IR, & ovf);
            overflow (ovf, false, "swcq overflow fault");
          }
          break;

        /// Fixed-Point Multiplication

        case 0401:  // mpf
          {
            // C(A) * C(Y) -> C(AQ), left adjusted
            //
            // Two 36-bit fractional factors (including sign) are multiplied
            // to form a 71- bit fractional product (including sign), which
            // is stored left-adjusted in the AQ register. AQ71 contains a
            // zero. Overflow can occur only in the case of A and Y
            // containing negative 1 and the result exceeding the range of
            // the AQ register.

#ifdef L68
            cpu.ou.cycle |= ou_GD1;
#endif
            word72 tmp72 = SIGNEXT36_72 (cpu.rA) * SIGNEXT36_72 (cpu.CY);
#ifdef L68
            cpu.ou.cycle |= ou_GD2;
#endif
            tmp72 &= MASK72;
            tmp72 <<= 1;    // left adjust so AQ71 contains 0
            // Overflow can occur only in the case of A and Y containing
            // negative 1
            if (cpu.rA == MAXNEG && cpu.CY == MAXNEG)
              {
                SET_I_NEG;
                CLR_I_ZERO;
                overflow (true, false, "mpf overflow fault");
              }

            convertToWord36 (tmp72, &cpu.rA, &cpu.rQ);
            SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0402:  // mpy
          // C(Q) * C(Y) -> C(AQ), right adjusted

          {
#ifdef L68
            cpu.ou.cycle |= ou_GOS;
#endif
            int64_t t0 = SIGNEXT36_64 (cpu.rQ & DMASK);
            int64_t t1 = SIGNEXT36_64 (cpu.CY & DMASK);

            __int128_t prod = (__int128_t) t0 * (__int128_t) t1;

            convertToWord36 ((word72)prod, &cpu.rA, &cpu.rQ);

            SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

//#define DIV_TRACE

        /// Fixed-Point Division

        case 0506:  // div
          // C(Q) / (Y) integer quotient -> C(Q), integer remainder -> C(A)
          //
          // A 36-bit integer dividend (including sign) is divided by a
          // 36-bit integer divisor (including sign) to form a 36-bit integer
          // * quotient (including sign) and a 36-bit integer remainder
          // * (including sign). The remainder sign is equal to the dividend
          // * sign unless the remainder is zero.
          // *
          // * If the dividend = -2**35 and the divisor = -1 or if the divisor
          // * = 0, then division does not take place. Instead, a divide check
          // * fault occurs, C(Q) contains the dividend magnitude, and the
          // * negative indicator reflects the dividend sign.

#ifdef L68
          cpu.ou.cycle |= ou_GD1;
#endif
          // RJ78: If the dividend = -2**35 and the divisor = +/-1, or if 
          // the divisor is 0

          if ((cpu.rQ == MAXNEG && (cpu.CY == 1 || cpu.CY == NEG136)) ||
              (cpu.CY == 0))
            {
//sim_printf ("DIV Q %012"PRIo64" Y %012"PRIo64"\n", cpu.rQ, cpu.CY); 
// case 1  400000000000 000000000000 --> 000000000000
// case 2  000000000000 000000000000 --> 400000000000
              //cpu.rA = 0;  // works for case 1
              cpu.rA = (cpu.rQ & SIGN36) ? 0 : SIGN36; // works for case 1,2

              // no division takes place
              SC_I_ZERO (cpu.CY == 0);
              SC_I_NEG (cpu.rQ & SIGN36);

              if (cpu.rQ & SIGN36)
                cpu.rQ = (- cpu.rQ) & MASK36;

              dlyDoFault (FAULT_DIV,
                          (_fault_subtype) {.fault_ipr_subtype=FR_ILL_OP},
                          "div divide check");
            }
          else
            {
              t_int64 dividend = (t_int64) (SIGNEXT36_64 (cpu.rQ));
              t_int64 divisor = (t_int64) (SIGNEXT36_64 (cpu.CY));

#ifdef DIV_TRACE
              sim_debug (DBG_CAC, & cpu_dev, "\n");
              sim_debug (DBG_CAC, & cpu_dev,
                         ">>> dividend cpu.rQ %"PRId64" (%012"PRIo64")\n",
                         dividend, cpu.rQ);
              sim_debug (DBG_CAC, & cpu_dev,
                         ">>> divisor  CY %"PRId64" (%012"PRIo64")\n",
                         divisor, cpu.CY);
#endif

              t_int64 quotient = dividend / divisor;
#ifdef L68
              cpu.ou.cycle |= ou_GD2;
#endif
              t_int64 remainder = dividend % divisor;

#ifdef DIV_TRACE
              sim_debug (DBG_CAC, & cpu_dev, ">>> quot 1 %"PRId64"\n", quotient);
              sim_debug (DBG_CAC, & cpu_dev, ">>> rem 1 %"PRId64"\n", remainder);
#endif

// Evidence is that DPS8M rounds toward zero; if it turns out that it
// rounds toward -inf, try this code:
#if 0
              // XXX C rounds toward zero; I suspect that DPS8M rounded toward
              // -inf.
              // If the remainder is negative, we rounded the wrong way
              if (remainder < 0)
                {
                  remainder += divisor;
                  quotient -= 1;

#ifdef DIV_TRACE
                  sim_debug (DBG_CAC, & cpu_dev,
                             ">>> quot 2 %"PRId64"\n", quotient);
                  sim_debug (DBG_CAC, & cpu_dev,
                             ">>> rem 2 %"PRId64"\n", remainder);
#endif
                }
#endif

#ifdef DIV_TRACE
              //  (a/b)*b + a%b is equal to a.
              sim_debug (DBG_CAC, & cpu_dev,
                         "dividend was                   = %"PRId64"\n", dividend);
              sim_debug (DBG_CAC, & cpu_dev,
                         "quotient * divisor + remainder = %"PRId64"\n",
                         quotient * divisor + remainder);
              if (dividend != quotient * divisor + remainder)
                {
                  sim_debug (DBG_CAC, & cpu_dev,
                     "---------------------------------^^^^^^^^^^^^^^^\n");
                }
#endif


              if (dividend != quotient * divisor + remainder)
                {
                  sim_debug (DBG_ERR, & cpu_dev,
                             "Internal division error;"
                             " rQ %012"PRIo64" CY %012"PRIo64"\n", cpu.rQ, cpu.CY);
                }

              cpu.rA = (word36) remainder & DMASK;
              cpu.rQ = (word36) quotient & DMASK;

#ifdef DIV_TRACE
              sim_debug (DBG_CAC, & cpu_dev, "rA (rem)  %012"PRIo64"\n", cpu.rA);
              sim_debug (DBG_CAC, & cpu_dev, "rQ (quot) %012"PRIo64"\n", cpu.rQ);
#endif

              SC_I_ZERO (cpu.rQ == 0);
              SC_I_NEG (cpu.rQ & SIGN36);
            }

          break;

        case 0507:  // dvf
          // C(AQ) / (Y)
          //  fractional quotient -> C(A)
          //  fractional remainder -> C(Q)

          // A 71-bit fractional dividend (including sign) is divided by a
          // 36-bit fractional divisor yielding a 36-bit fractional quotient
          // (including sign) and a 36-bit fractional remainder (including
          // sign). C(AQ)71 is ignored; bit position 35 of the remainder
          // corresponds to bit position 70 of the dividend. The remainder
          // sign is equal to the dividend sign unless the remainder is zero.

          // If | dividend | >= | divisor | or if the divisor = 0, division
          // does not take place. Instead, a divide check fault occurs, C(AQ)
          // contains the dividend magnitude in absolute, and the negative
          // indicator reflects the dividend sign.

          dvf ();

          break;

        /// Fixed-Point Negate

        case 0531:  // neg
          // -C(A) -> C(A) if C(A) != 0

          cpu.rA &= DMASK;
          if (cpu.rA == 0400000000000ULL)
            {
              CLR_I_ZERO;
              SET_I_NEG;
              overflow (true, false, "neg overflow fault");
            }

          cpu.rA = -cpu.rA;

          cpu.rA &= DMASK;    // keep to 36-bits

          SC_I_ZERO (cpu.rA == 0);
          SC_I_NEG (cpu.rA & SIGN36);

          break;

        case 0533:  // negl
          // -C(AQ) -> C(AQ) if C(AQ) != 0
          {
            cpu.rA &= DMASK;
            cpu.rQ &= DMASK;

            if (cpu.rA == 0400000000000ULL && cpu.rQ == 0)
            {
                CLR_I_ZERO;
                SET_I_NEG;
                overflow (true, false, "negl overflow fault");
            }

            word72 tmp72 = convertToWord72 (cpu.rA, cpu.rQ);
            tmp72 = -tmp72;

            SC_I_ZERO (tmp72 == 0);
            SC_I_NEG (tmp72 & SIGN72);

            convertToWord36 (tmp72, &cpu.rA, &cpu.rQ);
          }
          break;

        /// Fixed-Point Comparison

        case 0405:  // cmg
          // | C(A) | :: | C(Y) |
          // Zero:     If | C(A) | = | C(Y) | , then ON; otherwise OFF
          // Negative: If | C(A) | < | C(Y) | , then ON; otherwise OFF
          {
            // This is wrong for MAXNEG
            //word36 a = cpu.rA & SIGN36 ? -cpu.rA : cpu.rA;
            //word36 y = cpu.CY & SIGN36 ? -cpu.CY : cpu.CY;

              // If we do the 64 math, the MAXNEG case works
              t_int64 a = SIGNEXT36_64 (cpu.rA);
              if (a < 0)
                a = -a;
              t_int64 y = SIGNEXT36_64 (cpu.CY);
              if (y < 0)
                y = -y;

              SC_I_ZERO (a == y);
              SC_I_NEG (a < y);
          }
          break;

        case 0211:  // cmk
          // For i = 0, 1, ..., 35
          // C(Z)i = ~C(Q)i & ( C(A)i XOR C(Y)i )

          /**
           * The cmk instruction compares the contents of bit positions of A
           * and Y for identity that are not masked by a 1 in the
           * corresponding bit position of Q.
           *
           * The zero indicator is set ON if the comparison is successful for
           * all bit positions; i.e., if for all i = 0, 1, ..., 35 there is
           * either: C(A)i = C(Y)i (the identical case) or C(Q)i = 1 (the
           * masked case); otherwise, the zero indicator is set OFF.
           *
           * The negative indicator is set ON if the comparison is
           * unsuccessful for bit position 0; i.e., if C(A)0 XOR C(Y)0 (they
           * are nonidentical) as well as C(Q)0 = 0 (they are unmasked);
           * otherwise, the negative indicator is set OFF.
           */
          {
            word36 Z = ~cpu.rQ & (cpu.rA ^ cpu.CY);
            Z &= DMASK;

// Q  A  Y   ~Q   A^Y   Z
// 0  0  0    1     0   0
// 0  0  1    1     1   1
// 0  1  0    1     1   1
// 0  1  1    1     0   0
// 1  0  0    0     0   0
// 1  0  1    0     1   0
// 1  1  0    0     1   0
// 1  1  1    0     0   0


            SC_I_ZERO (Z == 0);
            SC_I_NEG (Z & SIGN36);
          }
          break;

        case 0115:  // cmpa
          // C(A) :: C(Y)

          cmp36 (cpu.rA, cpu.CY, &cpu.cu.IR);
          break;

        case 0116:  // cmpq
          // C(Q) :: C(Y)
          cmp36 (cpu.rQ, cpu.CY, &cpu.cu.IR);
          break;

        case 0100:  // cmpx0
        case 0101:  // cmpx1
        case 0102:  // cmpx2
        case 0103:  // cmpx3
        case 0104:  // cmpx4
        case 0105:  // cmpx5
        case 0106:  // cmpx6
        case 0107:  // cmpx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Xn) :: C(Y)0,17
          {
            uint32 n = opcode & 07;  // get n
            cmp18 (cpu.rX[n], GETHI (cpu.CY), &cpu.cu.IR);
          }
          break;

        case 0111:  // cwl
          // C(Y) :: closed interval [C(A);C(Q)]
          /**
           * The cwl instruction tests the value of C(Y) to determine if it
           * is within the range of values set by C(A) and C(Q). The
           * comparison of C(Y) with C(Q) locates C(Y) with respect to the
           * interval if C(Y) is not contained within the
           interval.
           */
          cmp36wl (cpu.rA, cpu.CY, cpu.rQ, &cpu.cu.IR);
          break;

        case 0117:  // cmpaq
          // C(AQ) :: C(Y-pair)
          {
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);

            word72 trAQ = convertToWord72 (cpu.rA, cpu.rQ);
            trAQ &= MASK72;

            cmp72 (trAQ, tmp72, &cpu.cu.IR);
          }
          break;

        /// Fixed-Point Miscellaneous

        case 0234:  // szn
          // Set indicators according to C(Y)
          cpu.CY &= DMASK;
          SC_I_ZERO (cpu.CY == 0);
          SC_I_NEG (cpu.CY & SIGN36);
          break;

        case 0214:  // sznc
          // Set indicators according to C(Y)
          cpu.CY &= DMASK;
          SC_I_ZERO (cpu.CY == 0);
          SC_I_NEG (cpu.CY & SIGN36);
          // ... and clear
          cpu.CY = 0;
          break;

        /// BOOLEAN OPERATION INSTRUCTIONS

        /// Boolean And

        case 0375:  // ana
          // C(A)i & C(Y)i -> C(A)i for i = (0, 1, ..., 35)
          cpu.rA = cpu.rA & cpu.CY;
          cpu.rA &= DMASK;
          SC_I_ZERO (cpu.rA == 0);
          SC_I_NEG (cpu.rA & SIGN36);
          break;

        case 0377:  //< anaq
          // C(AQ)i & C(Y-pair)i -> C(AQ)i for i = (0, 1, ..., 71)
          {
              word72 tmp72 = YPAIRTO72 (cpu.Ypair);
              word72 trAQ = convertToWord72 (cpu.rA, cpu.rQ);
              trAQ = trAQ & tmp72;
              trAQ &= MASK72;

              SC_I_ZERO (trAQ == 0);
              SC_I_NEG (trAQ & SIGN72);
              convertToWord36 (trAQ, &cpu.rA, &cpu.rQ);
          }
          break;

        case 0376:  // anq
          // C(Q)i & C(Y)i -> C(Q)i for i = (0, 1, ..., 35)
          cpu.rQ = cpu.rQ & cpu.CY;
          cpu.rQ &= DMASK;

          SC_I_ZERO (cpu.rQ == 0);
          SC_I_NEG (cpu.rQ & SIGN36);
          break;

        case 0355:  // ansa
          // C(A)i & C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
          {
            cpu.CY = cpu.rA & cpu.CY;
            cpu.CY &= DMASK;

            SC_I_ZERO (cpu.CY == 0);
            SC_I_NEG (cpu.CY & SIGN36);
          }
          break;

        case 0356:  // ansq
          // C(Q)i & C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
          {
              cpu.CY = cpu.rQ & cpu.CY;
              cpu.CY &= DMASK;

              SC_I_ZERO (cpu.CY == 0);
              SC_I_NEG (cpu.CY & SIGN36);
          }
          break;

        case 0340:  // ansx0
        case 0341:  // ansx1
        case 0342:  // ansx2
        case 0343:  // ansx3
        case 0344:  // ansx4
        case 0345:  // ansx5
        case 0346:  // ansx6
        case 0347:  // ansx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Xn)i & C(Y)i -> C(Y)i for i = (0, 1, ..., 17)
          {
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = cpu.rX[n] & GETHI (cpu.CY);
            tmp18 &= MASK18;

            SC_I_ZERO (tmp18 == 0);
            SC_I_NEG (tmp18 & SIGN18);

            SETHI (cpu.CY, tmp18);
          }

          break;

        case 0360:  // anx0
        case 0361:  // anx1
        case 0362:  // anx2
        case 0363:  // anx3
        case 0364:  // anx4
        case 0365:  // anx5
        case 0366:  // anx6
        case 0367:  // anx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Xn)i & C(Y)i -> C(Xn)i for i = (0, 1, ..., 17)
          {
              uint32 n = opcode & 07;  // get n
              cpu.rX[n] &= GETHI (cpu.CY);
              cpu.rX[n] &= MASK18;

              SC_I_ZERO (cpu.rX[n] == 0);
              SC_I_NEG (cpu.rX[n] & SIGN18);
          }
          break;

        /// Boolean Or

        case 0275:  // ora
          // C(A)i | C(Y)i -> C(A)i for i = (0, 1, ..., 35)
          cpu.rA = cpu.rA | cpu.CY;
          cpu.rA &= DMASK;

          SC_I_ZERO (cpu.rA == 0);
          SC_I_NEG (cpu.rA & SIGN36);
          break;

        case 0277:  // oraq
          // C(AQ)i | C(Y-pair)i -> C(AQ)i for i = (0, 1, ..., 71)
          {
              word72 tmp72 = YPAIRTO72 (cpu.Ypair);
              word72 trAQ = convertToWord72 (cpu.rA, cpu.rQ);
              trAQ = trAQ | tmp72;
              trAQ &= MASK72;

              SC_I_ZERO (trAQ == 0);
              SC_I_NEG (trAQ & SIGN72);
              convertToWord36 (trAQ, &cpu.rA, &cpu.rQ);
          }
          break;

        case 0276:  // orq
          // C(Q)i | C(Y)i -> C(Q)i for i = (0, 1, ..., 35)
          cpu.rQ = cpu.rQ | cpu.CY;
          cpu.rQ &= DMASK;

          SC_I_ZERO (cpu.rQ == 0);
          SC_I_NEG (cpu.rQ & SIGN36);

          break;

        case 0255:  // orsa
          // C(A)i | C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
          cpu.CY = cpu.rA | cpu.CY;
          cpu.CY &= DMASK;

          SC_I_ZERO (cpu.CY == 0);
          SC_I_NEG (cpu.CY & SIGN36);
          break;

        case 0256:  // orsq
          // C(Q)i | C(Y)i -> C(Y)i for i = (0, 1, ..., 35)

          cpu.CY = cpu.rQ | cpu.CY;
          cpu.CY &= DMASK;

          SC_I_ZERO (cpu.CY == 0);
          SC_I_NEG (cpu.CY & SIGN36);
          break;

        case 0240:  // orsx0
        case 0241:  // orsx1
        case 0242:  // orsx2
        case 0243:  // orsx3
        case 0244:  // orsx4
        case 0245:  // orsx5
        case 0246:  // orsx6
        case 0247:  // orsx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Xn)i | C(Y)i -> C(Y)i for i = (0, 1, ..., 17)
          {
            uint32 n = opcode & 07;  // get n

            word18 tmp18 = cpu.rX[n] | GETHI (cpu.CY);
            tmp18 &= MASK18;

            SC_I_ZERO (tmp18 == 0);
            SC_I_NEG (tmp18 & SIGN18);

            SETHI (cpu.CY, tmp18);
          }
          break;

        case 0260:  // orx0
        case 0261:  // orx1
        case 0262:  // orx2
        case 0263:  // orx3
        case 0264:  // orx4
        case 0265:  // orx5
        case 0266:  // orx6
        case 0267:  // orx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Xn)i | C(Y)i -> C(Xn)i for i = (0, 1, ..., 17)
          {
              uint32 n = opcode & 07;  // get n
              cpu.rX[n] |= GETHI (cpu.CY);
              cpu.rX[n] &= MASK18;

              SC_I_ZERO (cpu.rX[n] == 0);
              SC_I_NEG (cpu.rX[n] & SIGN18);
          }
          break;

        /// Boolean Exclusive Or

        case 0675:  // era
          // C(A)i XOR C(Y)i -> C(A)i for i = (0, 1, ..., 35)
          cpu.rA = cpu.rA ^ cpu.CY;
          cpu.rA &= DMASK;

          SC_I_ZERO (cpu.rA == 0);
          SC_I_NEG (cpu.rA & SIGN36);

          break;

        case 0677:  // eraq
          // C(AQ)i XOR C(Y-pair)i -> C(AQ)i for i = (0, 1, ..., 71)
          {
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            word72 trAQ = convertToWord72 (cpu.rA, cpu.rQ);
            trAQ = trAQ ^ tmp72;
            trAQ &= MASK72;

            SC_I_ZERO (trAQ == 0);
            SC_I_NEG (trAQ & SIGN72);

            convertToWord36 (trAQ, &cpu.rA, &cpu.rQ);
          }
          break;

        case 0676:  // erq
          // C(Q)i XOR C(Y)i -> C(Q)i for i = (0, 1, ..., 35)
          cpu.rQ = cpu.rQ ^ cpu.CY;
          cpu.rQ &= DMASK;
          SC_I_ZERO (cpu.rQ == 0);
          SC_I_NEG (cpu.rQ & SIGN36);
          break;

        case 0655:  // ersa
          // C(A)i XOR C(Y)i -> C(Y)i for i = (0, 1, ..., 35)

          cpu.CY = cpu.rA ^ cpu.CY;
          cpu.CY &= DMASK;

          SC_I_ZERO (cpu.CY == 0);
          SC_I_NEG (cpu.CY & SIGN36);
          break;

        case 0656:  // ersq
          // C(Q)i XOR C(Y)i -> C(Y)i for i = (0, 1, ..., 35)

          cpu.CY = cpu.rQ ^ cpu.CY;
          cpu.CY &= DMASK;

          SC_I_ZERO (cpu.CY == 0);
          SC_I_NEG (cpu.CY & SIGN36);

          break;

        case 0640:   // ersx0
        case 0641:   // ersx1
        case 0642:   // ersx2
        case 0643:   // ersx3
        case 0644:   // ersx4
        case 0645:   // ersx5
        case 0646:   // ersx6
        case 0647:   // ersx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Xn)i XOR C(Y)i -> C(Y)i for i = (0, 1, ..., 17)
          {
            uint32 n = opcode & 07;  // get n

            word18 tmp18 = cpu.rX[n] ^ GETHI (cpu.CY);
            tmp18 &= MASK18;

            SC_I_ZERO (tmp18 == 0);
            SC_I_NEG (tmp18 & SIGN18);

            SETHI (cpu.CY, tmp18);
          }
          break;

        case 0660:  // erx0
        case 0661:  // erx1
        case 0662:  // erx2
        case 0663:  // erx3
        case 0664:  // erx4
        case 0665:  // erx5
        case 0666:  // erx6 !!!! Beware !!!!
        case 0667:  // erx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Xn)i XOR C(Y)i -> C(Xn)i for i = (0, 1, ..., 17)
          {
            uint32 n = opcode & 07;  // get n
            cpu.rX[n] ^= GETHI (cpu.CY);
            cpu.rX[n] &= MASK18;

            SC_I_ZERO (cpu.rX[n] == 0);
            SC_I_NEG (cpu.rX[n] & SIGN18);
          }
          break;

        /// Boolean Comparative And

        case 0315:  // cana
          // C(Z)i = C(A)i & C(Y)i for i = (0, 1, ..., 35)
          {
            word36 trZ = cpu.rA & cpu.CY;
            trZ &= MASK36;

            SC_I_ZERO (trZ == 0);
            SC_I_NEG (trZ & SIGN36);
          }
          break;

        case 0317:  // canaq
          // C(Z)i = C(AQ)i & C(Y-pair)i for i = (0, 1, ..., 71)
          {
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            word72 trAQ = convertToWord72 (cpu.rA, cpu.rQ);
            trAQ = trAQ & tmp72;
            trAQ &= MASK72;

            SC_I_ZERO (trAQ == 0);
            SC_I_NEG (trAQ & SIGN72);
          }
            break;

        case 0316:  // canq
          // C(Z)i = C(Q)i & C(Y)i for i = (0, 1, ..., 35)
          {
            word36 trZ = cpu.rQ & cpu.CY;
            trZ &= DMASK;

            SC_I_ZERO (trZ == 0);
            SC_I_NEG (trZ & SIGN36);
          }
          break;

        case 0300:  // canx0
        case 0301:  // canx1
        case 0302:  // canx2
        case 0303:  // canx3
        case 0304:  // canx4
        case 0305:  // canx5
        case 0306:  // canx6
        case 0307:  // canx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          // C(Z)i = C(Xn)i & C(Y)i for i = (0, 1, ..., 17)
          {
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = cpu.rX[n] & GETHI (cpu.CY);
            tmp18 &= MASK18;
            sim_debug (DBG_TRACE, & cpu_dev,
                       "n %o rX %06o HI %06o tmp %06o\n",
                       n, cpu.rX[n], (word18) (GETHI (cpu.CY) & MASK18),
                       tmp18);

            SC_I_ZERO (tmp18 == 0);
            SC_I_NEG (tmp18 & SIGN18);
          }
          break;

        /// Boolean Comparative Not

        case 0215:  // cnaa
          // C(Z)i = C(A)i & ~C(Y)i for i = (0, 1, ..., 35)
          {
            word36 trZ = cpu.rA & ~cpu.CY;
            trZ &= DMASK;

            SC_I_ZERO (trZ == 0);
            SC_I_NEG (trZ & SIGN36);
          }
          break;

        case 0217:  // cnaaq
          // C(Z)i = C (AQ)i & ~C(Y-pair)i for i = (0, 1, ..., 71)
          {
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);   //

            word72 trAQ = convertToWord72 (cpu.rA, cpu.rQ);
            trAQ = trAQ & ~tmp72;
            trAQ &= MASK72;

            SC_I_ZERO (trAQ == 0);
            SC_I_NEG (trAQ & SIGN72);
          }
          break;

        case 0216:  // cnaq
          // C(Z)i = C(Q)i & ~C(Y)i for i = (0, 1, ..., 35)
          {
            word36 trZ = cpu.rQ & ~cpu.CY;
            trZ &= DMASK;
            SC_I_ZERO (trZ == 0);
            SC_I_NEG (trZ & SIGN36);
          }
          break;

        case 0200:  // cnax0
        case 0201:  // cnax1
        case 0202:  // cnax2
        case 0203:  // cnax3
        case 0204:  // cnax4
        case 0205:  // cnax5
        case 0206:  // cnax6
        case 0207:  // cnax7
          // C(Z)i = C(Xn)i & ~C(Y)i for i = (0, 1, ..., 17)
          {
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = cpu.rX[n] & ~GETHI (cpu.CY);
            tmp18 &= MASK18;

            SC_I_ZERO (tmp18 == 0);
            SC_I_NEG (tmp18 & SIGN18);
          }
          break;

        /// FLOATING-POINT ARITHMETIC INSTRUCTIONS

        /// Floating-Point Data Movement Load

        case 0433:  // dfld
          // C(Y-pair)0,7 -> C(E)
          // C(Y-pair)8,71 -> C(AQ)0,63
          // 00...0 -> C(AQ)64,71
          // Zero: If C(AQ) = 0, then ON; otherwise OFF
          // Neg: If C(AQ)0 = 1, then ON; otherwise OFF

          CPTUR (cptUseE);
          cpu.rE = (cpu.Ypair[0] >> 28) & MASK8;

          cpu.rA = (cpu.Ypair[0] & FLOAT36MASK) << 8;
          cpu.rA |= (cpu.Ypair[1] >> 28) & MASK8;

          cpu.rQ = (cpu.Ypair[1] & FLOAT36MASK) << 8;

          SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
          SC_I_NEG (cpu.rA & SIGN36);
          break;

        case 0431:  // fld
          // C(Y)0,7 -> C(E)
          // C(Y)8,35 -> C(AQ)0,27
          // 00...0 -> C(AQ)30,71
          // Zero: If C(AQ) = 0, then ON; otherwise OFF
          // Neg: If C(AQ)0 = 1, then ON; otherwise OFF

          CPTUR (cptUseE);
          cpu.CY &= DMASK;
          cpu.rE = (cpu.CY >> 28) & 0377;
          cpu.rA = (cpu.CY & FLOAT36MASK) << 8;
          cpu.rQ = 0;

          SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
          SC_I_NEG (cpu.rA & SIGN36);
          break;

        /// Floating-Point Data Movement Store

        case 0457:  // dfst
          // C(E) -> C(Y-pair)0,7
          // C(AQ)0,63 -> C(Y-pair)8,71

          CPTUR (cptUseE);
          cpu.Ypair[0] = ((word36)cpu.rE << 28) |
                         ((cpu.rA & 0777777777400LLU) >> 8);
          cpu.Ypair[1] = ((cpu.rA & 0377) << 28) |
                         ((cpu.rQ & 0777777777400LLU) >> 8);

          break;

        case 0472:  // dfstr

          dfstr (cpu.Ypair);
          break;

        case 0455:  // fst
          // C(E) -> C(Y)0,7
          // C(A)0,27 -> C(Y)8,35
          CPTUR (cptUseE);
          cpu.rE &= MASK8;
          cpu.rA &= DMASK;
          cpu.CY = ((word36)cpu.rE << 28) | (((cpu.rA >> 8) & 01777777777LL));
          break;

        case 0470:  // fstr
          // The fstr instruction performs a true round and normalization on
          // C(EAQ) as it is stored.

//            frd ();
//
//            // C(E) -> C(Y)0,7
//            // C(A)0,27 -> C(Y)8,35
//            cpu.CY = ((word36)cpu.rE << 28) |
//                     (((cpu.rA >> 8) & 01777777777LL));
//
//            // Zero: If C(Y) = floating point 0, then ON; otherwise OFF
//            //SC_I_ZERO ((cpu.CY & 01777777777LL) == 0);
//            bool isZero = cpu.rE == -128 && cpu.rA == 0;
//            SC_I_ZERO (isZero);
//
//            // Neg: If C(Y)8 = 1, then ON; otherwise OFF
//            //SC_I_NEG (cpu.CY & 01000000000LL);
//            SC_I_NEG (cpu.rA & SIGN36);
//
//            // Exp Ovr: If exponent is greater than +127, then ON
//            // Exp Undr: If exponent is less than -128, then ON
//            // XXX: not certain how these can occur here ....

          fstr (&cpu.CY);

          break;

        /// Floating-Point Addition

        case 0477:  // dfad
          // The dfad instruction may be thought of as a dufa instruction
          // followed by a fno instruction.

          CPTUR (cptUseE);
          dufa (false);
          fno (&cpu.rE, &cpu.rA, &cpu.rQ);
          break;

        case 0437:  // dufa
          dufa (false);
          break;

        case 0475:  // fad
          // The fad instruction may be thought of a an ufa instruction
          // followed by a fno instruction.
          // (Heh, heh. We'll see....)

          CPTUR (cptUseE);
          ufa (false);
          fno (&cpu.rE, &cpu.rA, &cpu.rQ);

          break;

        case 0435:  // ufa
            // C(EAQ) + C(Y) -> C(EAQ)

          ufa (false);
          break;

        /// Floating-Point Subtraction

        case 0577:  // dfsb
          // The dfsb instruction is identical to the dfad instruction with
          // the exception that the twos complement of the mantissa of the
          // operand from main memory is used.

          CPTUR (cptUseE);
          dufa (true);
          fno (&cpu.rE, &cpu.rA, &cpu.rQ);
          break;

        case 0537:  // dufs
          dufa (true);
          break;

        case 0575:  // fsb
          // The fsb instruction may be thought of as an ufs instruction
          // followed by a fno instruction.
          CPTUR (cptUseE);
          ufa (true);
          fno (&cpu.rE, &cpu.rA, &cpu.rQ);

          break;

        case 0535:  // ufs
          // C(EAQ) - C(Y) -> C(EAQ)
          ufa (true);
          break;

        /// Floating-Point Multiplication

        case 0463:  // dfmp
          // The dfmp instruction may be thought of as a dufm instruction
          // followed by a fno instruction.

          CPTUR (cptUseE);
          dufm ();
          fno (&cpu.rE, &cpu.rA, &cpu.rQ);
          break;

        case 0423:  // dufm

          dufm ();
          break;

        case 0461:  // fmp
          // The fmp instruction may be thought of as a ufm instruction
          // followed by a fno instruction.

          CPTUR (cptUseE);
          ufm ();
          fno (&cpu.rE, &cpu.rA, &cpu.rQ);

          break;

        case 0421:  // ufm
          // C(EAQ)* C(Y) -> C(EAQ)
          ufm ();
          break;

        /// Floating-Point Division

        case 0527:  // dfdi

          dfdi ();
          break;

        case 0567:  // dfdv

          dfdv ();
          break;

        case 0525:  // fdi
          // C(Y) / C(EAQ) -> C(EA)

          fdi ();
          break;

        case 0565:  // fdv
          // C(EAQ) /C(Y) -> C(EA)
          // 00...0 -> C(Q)
          fdv ();
          break;

        /// Floating-Point Negation

        case 0513:  // fneg
          // -C(EAQ) normalized -> C(EAQ)
          fneg ();
          break;

        /// Floating-Point Normalize

        case 0573:  // fno
          // The fno instruction normalizes the number in C(EAQ) if C(AQ)
          // != 0 and the overflow indicator is OFF.
          //
          // A normalized floating number is defined as one whose mantissa
          // lies in the interval [0.5,1.0) such that 0.5<= |C(AQ)| <1.0
          // which, in turn, requires that C(AQ)0 != C(AQ)1.list
          //
          // !!!! For personal reasons the following 3 lines of comment must
          // never be removed from this program or any code derived
          // therefrom. HWR 25 Aug 2014
          ///Charles Is the coolest
          ///true story y'all
          //you should get me darksisers 2 for christmas

          CPTUR (cptUseE);
          fno (& cpu.rE, & cpu.rA, & cpu.rQ);
          break;

        /// Floating-Point Round

        case 0473:  // dfrd
          // C(EAQ) rounded to 64 bits -> C(EAQ)
          // 0 -> C(AQ)64,71 (See notes in dps8_math.c on dfrd())

          dfrd ();
          break;

        case 0471:  // frd
          // C(EAQ) rounded to 28 bits -> C(EAQ)
          // 0 -> C(AQ)28,71 (See notes in dps8_math.c on frd())

          frd ();
          break;

        /// Floating-Point Compare

        case 0427:  // dfcmg
          // C(E) :: C(Y-pair)0,7
          // | C(AQ)0,63 | :: | C(Y-pair)8,71 |

          dfcmg ();
          break;

        case 0517:  // dfcmp
          // C(E) :: C(Y-pair)0,7
          // C(AQ)0,63 :: C(Y-pair)8,71

          dfcmp ();
          break;

        case 0425:  // fcmg
          // C(E) :: C(Y)0,7
          // | C(AQ)0,27 | :: | C(Y)8,35 |

          fcmg ();
          break;

        case 0515:  // fcmp
          // C(E) :: C(Y)0,7
          // C(AQ)0,27 :: C(Y)8,35

          fcmp ();
          break;

        /// Floating-Point Miscellaneous

        case 0415:  // ade
          // C(E) + C(Y)0,7 -> C(E)
          {
            CPTUR (cptUseE);
            int y = SIGNEXT8_int ((cpu.CY >> 28) & 0377);
            int e = SIGNEXT8_int (cpu.rE);
            e = e + y;

            cpu.rE = e & 0377;
            CLR_I_ZERO;
            CLR_I_NEG;

            if (e > 127)
              {
                SET_I_EOFL;
                if (tstOVFfault ())
                  doFault (FAULT_OFL, (_fault_subtype) {.bits=0},
                           "ade exp overflow fault");
              }

            if (e < -128)
              {
                SET_I_EUFL;
                if (tstOVFfault ())
                  doFault (FAULT_OFL, (_fault_subtype) {.bits=0},
                           "ade exp underflow fault");
              }
          }
          break;

        case 0430:  // fszn

          // Zero: If C(Y)8,35 = 0, then ON; otherwise OFF
          // Negative: If C(Y)8 = 1, then ON; otherwise OFF

          SC_I_ZERO ((cpu.CY & 001777777777LL) == 0);
          SC_I_NEG (cpu.CY & 001000000000LL);

          break;

        case 0411:  // lde
          // C(Y)0,7 -> C(E)

          CPTUR (cptUseE);
          cpu.rE = (cpu.CY >> 28) & 0377;
          CLR_I_ZERO;
          CLR_I_NEG;

          break;

        case 0456:  // ste
          // C(E) -> C(Y)0,7
          // 00...0 -> C(Y)8,17

          CPTUR (cptUseE);
          putbits36_18 (& cpu.CY, 0, ((word18) (cpu.rE & 0377) << 10));
          break;


        /// TRANSFER INSTRUCTIONS

        case 0713:  // call6

          CPTUR (cptUsePRn + 7);

          //ReadOP (cpu.TPR.CA, RTCD_OPERAND_FETCH);
          ReadTraOp ();
          sim_debug (DBG_TRACE, & cpu_dev,
                     "call6 PRR %o PSR %o\n", cpu.PPR.PRR, cpu.PPR.PSR);

          return CONT_TRA;


        case 0630:  // ret
          {
            // Parity mask: If C(Y)27 = 1, and the processor is in absolute or
            // mask privileged mode, then ON; otherwise OFF. This indicator is
            // not affected in the normal or BAR modes.
            // Not BAR mode: Can be set OFF but not ON by the ret instruction
            // Absolute mode: Can be set OFF but not ON by the ret instruction
            // All oter indicators: If corresponding bit in C(Y) is 1, then ON;
            // otherwise, OFF

            // C(Y)0,17 -> C(PPR.IC)
            // C(Y)18,31 -> C(IR)
            //ReadOP (cpu.TPR.CA, OPERAND_READ);
            ReadTraOp ();

            cpu.PPR.IC = GETHI (cpu.CY);
            word18 tempIR = GETLO (cpu.CY) & 0777770;
            // Assuming 'mask privileged mode' is 'temporary absolute mode'
            if (is_priv_mode ()) // abs. or temp. abs. or priv.
              {
                // if abs, copy existing parity mask to tempIR
                // According to ISOLTS pm785, not the case.
                //SCF (TST_I_PMASK, tempIR, I_PMASK);
                // if abs, copy existing I_MIF to tempIR
                SCF (TST_I_MIF, tempIR, I_MIF);
              }
            else
              {
                CLRF (tempIR, I_MIF);
              }
            // can be set OFF but not on
            //  IR   ret   result
            //  off  off   off
            //  off  on    off
            //  on   on    on
            //  on   off   off
            // "If it was on, set it to on"
            //SCF (TST_I_NBAR, tempIR, I_NBAR);
            if (! (TST_I_NBAR && TSTF (tempIR, I_NBAR)))
              {
                CLRF (tempIR, I_NBAR);
              }
            if (! (TST_I_ABS && TSTF (tempIR, I_ABS)))
              {
                CLRF (tempIR, I_ABS);
              }
            

            //sim_debug (DBG_TRACE, & cpu_dev,
            //           "RET NBAR was %d now %d\n",
            //           TST_NBAR ? 1 : 0,
            //           TSTF (tempIR, I_NBAR) ? 1 : 0);
            //sim_debug (DBG_TRACE, & cpu_dev,
            //           "RET ABS  was %d now %d\n",
            //           TST_I_ABS ? 1 : 0,
            //           TSTF (tempIR, I_ABS) ? 1 : 0);
            CPTUR (cptUseIR);
            cpu.cu.IR = tempIR;
            return CONT_TRA;
          }

        case 0610:  // rtcd
          // If an access violation fault occurs when fetching the SDW for
          // the Y-pair, the C(PPR.PSR) and C(PPR.PRR) are not altered.  If
          // the rtcd instruction is executed with the processor in absolute
          // mode with bit 29 of the instruction word set OFF and without
          // indirection through an ITP or ITS pair, then:
          //
          //   appending mode is entered for address preparation for the
          //   rtcd operand and is retained if the instruction executes
          //   successfully, and the effective segment number generated for
          //   the SDW fetch and subsequent loading into C(TPR.TSR) is equal
          //   to C(PPR.PSR) and may be undefined in absolute mode, and the
          //   effective ring number loaded into C(TPR.TRR) prior to the SDW
          //   fetch is equal to C(PPR.PRR) (which is 0 in absolute mode)
          //   implying that control is always transferred into ring 0.
          //

          ReadRTCDOp ();
          // RTCD always ends up in append mode.
          set_addr_mode (APPEND_mode);
            
          return CONT_TRA;


        case 0614:  // teo
          // If exponent overflow indicator ON then
          //  C(TPR.CA) -> C(PPR.IC)
          //  C(TPR.TSR) -> C(PPR.PSR)
          // otherwise, no change to C(PPR)
          if (TST_I_EOFL)
            {
              CLR_I_EOFL;
              //ReadOP (cpu.TPR.CA, OPERAND_READ);
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0615:  // teu
          // If exponent underflow indicator ON then
          //  C(TPR.CA) -> C(PPR.IC)
          //  C(TPR.TSR) -> C(PPR.PSR)
          if (TST_I_EUFL)
            {
              CLR_I_EUFL;
              ReadTraOp ();
              return CONT_TRA;
            }
          break;


        case 0604:  // tmi
          // If negative indicator ON then
          //  C(TPR.CA) -> C(PPR.IC)
          //  C(TPR.TSR) -> C(PPR.PSR)
          if (TST_I_NEG)
            {
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0602:  // tnc
          // If carry indicator OFF then
          //   C(TPR.CA) -> C(PPR.IC)
          //   C(TPR.TSR) -> C(PPR.PSR)
          if (!TST_I_CARRY)
            {
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0601:  // tnz
          // If zero indicator OFF then
          //     C(TPR.CA) -> C(PPR.IC)
          //     C(TPR.TSR) -> C(PPR.PSR)
          if (!TST_I_ZERO)
            {
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0617:  // tov
          // If overflow indicator ON then
          //   C(TPR.CA) -> C(PPR.IC)
          //   C(TPR.TSR) -> C(PPR.PSR)
          if (TST_I_OFLOW)
            {
              CLR_I_OFLOW;
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0605:  // tpl
          // If negative indicator OFF, then
          //   C(TPR.CA) -> C(PPR.IC)
          //   C(TPR.TSR) -> C(PPR.PSR)
          if (! (TST_I_NEG))
            {
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0710:  // tra
          // C(TPR.CA) -> C(PPR.IC)
          // C(TPR.TSR) -> C(PPR.PSR)
          ReadTraOp ();
          return CONT_TRA;

        case 0603:  // trc
          //  If carry indicator ON then
          //    C(TPR.CA) -> C(PPR.IC)
          //    C(TPR.TSR) -> C(PPR.PSR)
          if (TST_I_CARRY)
            {
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0270:  //< tsp0
        case 0271:  //< tsp1
        case 0272:  //< tsp2
        case 0273:  //< tsp3

        case 0670:  // tsp4
        case 0671:  // tsp5
        case 0672:  // tsp6
        case 0673:  // tsp7
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(PPR.PRR) -> C(PRn.RNR)
          //  C(PPR.PSR) -> C(PRn.SNR)
          //  C(PPR.IC) + 1 -> C(PRn.WORDNO)
          //  00...0 -> C(PRn.BITNO)
          //  C(TPR.CA) -> C(PPR.IC)
          //  C(TPR.TSR) -> C(PPR.PSR)
          {
            uint32 n;
            if (opcode <= 0273)
              n = (opcode & 3);
            else
              n = (opcode & 3) + 4;
            CPTUR (cptUsePRn + n);

            ReadTraOp ();
          }
          return CONT_TRA;

        case 0715:  // tss
          CPTUR (cptUseBAR);
          if (cpu.TPR.CA >= ((word18) cpu.BAR.BOUND) << 9)
            {
              doFault (FAULT_ACV,
                       (_fault_subtype) {.fault_acv_subtype=ACV15},
                       "TSS boundary violation");
            }
          ReadTraOp ();
          CLR_I_NBAR;
          return CONT_TRA;

        case 0700:  // tsx0
        case 0701:  // tsx1
        case 0702:  // tsx2
        case 0703:  // tsx3
        case 0704:  // tsx4
        case 0705:  // tsx5
        case 0706:  // tsx6
        case 0707:  // tsx7
          // For n = 0, 1, ..., or 7 as determined by operation code
          //   C(PPR.IC) + 1 -> C(Xn)
          // C(TPR.CA) -> C(PPR.IC)
          // C(TPR.TSR) -> C(PPR.PSR)
          {
            // We can't set Xn yet as the CAF may refer to Xn
            word18 ret = (cpu.PPR.IC + 1) & MASK18;
            ReadTraOp ();
            cpu.rX[opcode & 07] = ret;
          }
          return CONT_TRA;

        case 0607:  // ttf
          // If tally runout indicator OFF then
          //   C(TPR.CA) -> C(PPR.IC)
          //  C(TPR.TSR) -> C(PPR.PSR)
          // otherwise, no change to C(PPR)
          if (TST_I_TALLY == 0)
            {
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        case 0600:  // tze
          // If zero indicator ON then
          //   C(TPR.CA) -> C(PPR.IC)
          //   C(TPR.TSR) -> C(PPR.PSR)
          // otherwise, no change to C(PPR)
          if (TST_I_ZERO)
            {
              ReadTraOp ();
              return CONT_TRA;
            }
          break;

        /// POINTER REGISTER INSTRUCTIONS

        /// Pointer Register Data Movement Load

        case 0311:  // easp0
          // C(TPR.CA) -> C(PRn.SNR)
          CPTUR (cptUsePRn + 0);
          cpu.PR[0].SNR = cpu.TPR.CA & MASK15;
          break;
        case 0313:  // easp2
          // C(TPR.CA) -> C(PRn.SNR)
          CPTUR (cptUsePRn + 2);
          cpu.PR[2].SNR = cpu.TPR.CA & MASK15;
          break;
        case 0331:  // easp4
          // C(TPR.CA) -> C(PRn.SNR)
          CPTUR (cptUsePRn + 4);
          cpu.PR[4].SNR = cpu.TPR.CA & MASK15;
          break;
        case 0333:  // easp6
          // C(TPR.CA) -> C(PRn.SNR)
          CPTUR (cptUsePRn + 6);
          cpu.PR[6].SNR = cpu.TPR.CA & MASK15;
          break;

        case 0310:  // eawp0
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.CA) -> C(PRn.WORDNO)
          //  C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 0);
          cpu.PR[0].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (0, cpu.TPR.TBR);
          break;
        case 0312:  // eawp2
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.CA) -> C(PRn.WORDNO)
          //  C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 2);
          cpu.PR[2].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (2, cpu.TPR.TBR);
          break;
        case 0330:  // eawp4
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.CA) -> C(PRn.WORDNO)
          //  C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 4);
          cpu.PR[4].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (4, cpu.TPR.TBR);
          break;
        case 0332:  // eawp6
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.CA) -> C(PRn.WORDNO)
          //  C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 6);
          cpu.PR[6].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (6, cpu.TPR.TBR);
          break;

        case 0351:  // epbp1
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.TRR) -> C(PRn.RNR)
          //  C(TPR.TSR) -> C(PRn.SNR)
          //  00...0 -> C(PRn.WORDNO)
          //  0000 -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 1);
          cpu.PR[1].RNR = cpu.TPR.TRR;
          cpu.PR[1].SNR = cpu.TPR.TSR;
          cpu.PR[1].WORDNO = 0;
          SET_PR_BITNO (1, 0);
          break;
        case 0353:  // epbp3
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.TRR) -> C(PRn.RNR)
          //  C(TPR.TSR) -> C(PRn.SNR)
          //  00...0 -> C(PRn.WORDNO)
          //  0000 -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 3);
          cpu.PR[3].RNR = cpu.TPR.TRR;
          cpu.PR[3].SNR = cpu.TPR.TSR;
          cpu.PR[3].WORDNO = 0;
          SET_PR_BITNO (3, 0);
          break;
        case 0371:  // epbp5
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.TRR) -> C(PRn.RNR)
          //  C(TPR.TSR) -> C(PRn.SNR)
          //  00...0 -> C(PRn.WORDNO)
          //  0000 -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 5);
          cpu.PR[5].RNR = cpu.TPR.TRR;
          cpu.PR[5].SNR = cpu.TPR.TSR;
          cpu.PR[5].WORDNO = 0;
          SET_PR_BITNO (5, 0);
          break;
        case 0373:  // epbp7
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.TRR) -> C(PRn.RNR)
          //  C(TPR.TSR) -> C(PRn.SNR)
          //  00...0 -> C(PRn.WORDNO)
          //  0000 -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 7);
          cpu.PR[7].RNR = cpu.TPR.TRR;
          cpu.PR[7].SNR = cpu.TPR.TSR;
          cpu.PR[7].WORDNO = 0;
          SET_PR_BITNO (7, 0);
          break;

        case 0350:  // epp0
          // For n = 0, 1, ..., or 7 as determined by operation code
          //   C(TPR.TRR) -> C(PRn.RNR)
          //   C(TPR.TSR) -> C(PRn.SNR)
          //   C(TPR.CA) -> C(PRn.WORDNO)
          //   C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 0);
          cpu.PR[0].RNR = cpu.TPR.TRR;
          cpu.PR[0].SNR = cpu.TPR.TSR;
          cpu.PR[0].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (0, cpu.TPR.TBR);
          break;
        case 0352:  // epp2
          // For n = 0, 1, ..., or 7 as determined by operation code
          //   C(TPR.TRR) -> C(PRn.RNR)
          //   C(TPR.TSR) -> C(PRn.SNR)
          //   C(TPR.CA) -> C(PRn.WORDNO)
          //   C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 2);
          cpu.PR[2].RNR = cpu.TPR.TRR;
          cpu.PR[2].SNR = cpu.TPR.TSR;
          cpu.PR[2].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (2, cpu.TPR.TBR);
          break;
        case 0370:  // epp4
          // For n = 0, 1, ..., or 7 as determined by operation code
          //   C(TPR.TRR) -> C(PRn.RNR)
          //   C(TPR.TSR) -> C(PRn.SNR)
          //   C(TPR.CA) -> C(PRn.WORDNO)
          //   C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 4);
          cpu.PR[4].RNR = cpu.TPR.TRR;
          cpu.PR[4].SNR = cpu.TPR.TSR;
          cpu.PR[4].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (4, cpu.TPR.TBR);
          break;
        case 0372:  // epp6
          // For n = 0, 1, ..., or 7 as determined by operation code
          //   C(TPR.TRR) -> C(PRn.RNR)
          //   C(TPR.TSR) -> C(PRn.SNR)
          //   C(TPR.CA) -> C(PRn.WORDNO)
          //   C(TPR.TBR) -> C(PRn.BITNO)
          CPTUR (cptUsePRn + 6);
          cpu.PR[6].RNR = cpu.TPR.TRR;
          cpu.PR[6].SNR = cpu.TPR.TSR;
          cpu.PR[6].WORDNO = cpu.TPR.CA;
          SET_PR_BITNO (6, cpu.TPR.TBR);
          break;

        case 0173:  // lpri
          // For n = 0, 1, ..., 7
          //  Y-pair = Y-block16 + 2n
          //  Maximum of C(Y-pair)18,20; C(SDW.R1); C(TPR.TRR) -> C(PRn.RNR)
          //  C(Y-pair) 3,17 -> C(PRn.SNR)
          //  C(Y-pair)36,53 -> C(PRn.WORDNO)
          //  C(Y-pair)57,62 -> C(PRn.BITNO)

          for (uint32 n = 0 ; n < 8 ; n ++)
            {
              CPTUR (cptUsePRn + n);
              // Even word of ITS pointer pair
              cpu.Ypair[0] = cpu.Yblock16[n * 2 + 0];
              // Odd word of ITS pointer pair
              cpu.Ypair[1] = cpu.Yblock16[n * 2 + 1];

              // RNR from ITS pair
              word3 Crr = (GETLO (cpu.Ypair[0]) >> 15) & 07;
              if (get_addr_mode () == APPEND_mode)
                cpu.PR[n].RNR = max3 (Crr, cpu.SDW->R1, cpu.TPR.TRR);
              else
                cpu.PR[n].RNR = Crr;
              cpu.PR[n].SNR = (cpu.Ypair[0] >> 18) & MASK15;
              cpu.PR[n].WORDNO = GETHI (cpu.Ypair[1]);
              word6 bitno = (GETLO (cpu.Ypair[1]) >> 9) & 077;
//IF1 sim_printf ("LPRI n %u bitno 0%o %u.\n", n, bitno, bitno);
// According to ISOLTS, loading a 077 into bitno results in 037
// pa851    test-04b    lpri test       bar-100176
// test start 105321   patch 105461   subtest loop point 105442
// s/b 77777737
// was 77777733
              if (bitno == 077)
                bitno = 037;
              SET_PR_BITNO (n, bitno);
            }

          break;

        case 0760:  // lprp0
        case 0761:  // lprp1
        case 0762:  // lprp2
        case 0763:  // lprp3
        case 0764:  // lprp4
        case 0765:  // lprp5
        case 0766:  // lprp6
        case 0767:  // lprp7
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(TPR.TRR) -> C(PRn.RNR)
          //  If C(Y)0,1 != 11, then
          //    C(Y)0,5 -> C(PRn.BITNO);
          //  otherwise,
          //    generate command fault
          // If C(Y)6,17 = 11...1, then 111 -> C(PRn.SNR)0,2
          //  otherwise,
          // 000 -> C(PRn.SNR)0,2
          // C(Y)6,17 -> C(PRn.SNR)3,14
          // C(Y)18,35 -> C(PRn.WORDNO)
          {
              uint32 n = opcode & 07;  // get n
              CPTUR (cptUsePRn + n);
              cpu.PR[n].RNR = cpu.TPR.TRR;

// [CAC] sprpn says: If C(PRn.SNR) 0,2 are nonzero, and C(PRn.SNR) != 11...1,
// then a store fault (illegal pointer) will occur and C(Y) will not be changed.
// I interpret this has meaning that only the high bits should be set here

              if (((cpu.CY >> 34) & 3) != 3)
                {
                  word6 bitno = (cpu.CY >> 30) & 077;
                  SET_PR_BITNO (n, bitno);
                }
              else
                {
// fim.alm
// command_fault:
//           eax7      com       assume normal command fault
//           ldq       bp|mc.scu.port_stat_word check illegal action
//           canq      scu.ial_mask,dl
//           tnz       fixindex            nonzero, treat as normal case
//           ldq       bp|scu.even_inst_word check for LPRPxx instruction
//           anq       =o770400,dl
//           cmpq      lprp_insts,dl
//           tnz       fixindex            isn't LPRPxx, treat as normal

// ial_mask is checking SCU word 1, field IA: 0 means "no illegal action"

                    // Therefore the subfault well no illegal action, and
                    // Multics will peek it the instruction to deduce that it
                    // is a lprpn fault.
                  doFault (FAULT_CMD,
                           (_fault_subtype)
                           {.fault_cmd_subtype=flt_cmd_lprpn_bits},
                           "lprpn");
                }
// The SPRPn instruction stores only the low 12 bits of the 15 bit SNR.
// A special case is made for an SNR of all ones; it is stored as 12 1's.
// The pcode in AL39 handles this awkwardly; I believe this is
// the same, but in a more straightforward manner

             // Get the 12 bit operand SNR
             word12 oSNR = getbits36_12 (cpu.CY, 6);
             // Test for special case
             if (oSNR == 07777)
               cpu.PR[n].SNR = 077777;
             else
               cpu.PR[n].SNR = oSNR; // usigned word will 0-extend.
              //C(Y)18,35 -> C(PRn.WORDNO)
              cpu.PR[n].WORDNO = GETLO (cpu.CY);

              sim_debug (DBG_APPENDING, & cpu_dev,
                         "lprp%d CY 0%012"PRIo64", PR[n].RNR 0%o, "
                         "PR[n].BITNO 0%o, PR[n].SNR 0%o, PR[n].WORDNO %o\n",
                         n, cpu.CY, cpu.PR[n].RNR, GET_PR_BITNO (n),
                         cpu.PR[n].SNR, cpu.PR[n].WORDNO);
          }
          break;

        /// Pointer Register Data Movement Store

        case 0251:  // spbp1
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  000 -> C(Y-pair)0,2
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  00...0 -> C(Y-pair)36,71
          CPTUR (cptUsePRn + 1);
          cpu.Ypair[0] = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[1].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[1].RNR) << 15;
          cpu.Ypair[1] = 0;
          break;

        case 0253:  // spbp3
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  000 -> C(Y-pair)0,2
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  00...0 -> C(Y-pair)36,71
          CPTUR (cptUsePRn + 3);
          cpu.Ypair[0] = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[3].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[3].RNR) << 15;
          cpu.Ypair[1] = 0;
          break;

        case 0651:  // spbp5
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  000 -> C(Y-pair)0,2
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  00...0 -> C(Y-pair)36,71
          CPTUR (cptUsePRn + 5);
          cpu.Ypair[0] = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[5].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[5].RNR) << 15;
          cpu.Ypair[1] = 0;
          break;

        case 0653:  // spbp7
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  000 -> C(Y-pair)0,2
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  00...0 -> C(Y-pair)36,71
          CPTUR (cptUsePRn + 7);
          cpu.Ypair[0] = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[7].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[7].RNR) << 15;
          cpu.Ypair[1] = 0;
          break;

        case 0254:  // spri
          // For n = 0, 1, ..., 7
          //  Y-pair = Y-block16 + 2n

          //  000 -> C(Y-pair)0,2
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35

          //  C(PRn.WORDNO) -> C(Y-pair)36,53
          //  000 -> C(Y-pair)54,56
          //  C(PRn.BITNO) -> C(Y-pair)57,62
          //  00...0 -> C(Y-pair)63,71

          for (uint32 n = 0 ; n < 8 ; n++)
            {
              CPTUR (cptUsePRn + n);
              cpu.Yblock16[2 * n] = 043;
              cpu.Yblock16[2 * n] |= ((word36) cpu.PR[n].SNR) << 18;
              cpu.Yblock16[2 * n] |= ((word36) cpu.PR[n].RNR) << 15;

              cpu.Yblock16[2 * n + 1] = (word36) cpu.PR[n].WORDNO << 18;
              cpu.Yblock16[2 * n + 1] |= (word36) GET_PR_BITNO(n) << 9;
            }

          break;

        case 0250:  // spri0
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  000 -> C(Y-pair)0,2
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  C(PRn.WORDNO) -> C(Y-pair)36,53
          //  000 -> C(Y-pair)54,56
          //  C(PRn.BITNO) -> C(Y-pair)57,62
          //  00...0 -> C(Y-pair)63,71
          CPTUR (cptUsePRn + 0);
          cpu.Ypair[0]  = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[0].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[0].RNR) << 15;

          cpu.Ypair[1]  = (word36) cpu.PR[0].WORDNO << 18;
          cpu.Ypair[1] |= (word36) GET_PR_BITNO (0) << 9;
          break;

        case 0252:  // spri2
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  000 -> C(Y-pair)0,2
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  C(PRn.WORDNO) -> C(Y-pair)36,53
          //  000 -> C(Y-pair)54,56
          //  C(PRn.BITNO) -> C(Y-pair)57,62
          //  00...0 -> C(Y-pair)63,71
          CPTUR (cptUsePRn + 2);
          cpu.Ypair[0] = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[2].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[2].RNR) << 15;

          cpu.Ypair[1] = (word36) cpu.PR[2].WORDNO << 18;
          cpu.Ypair[1] |= (word36) GET_PR_BITNO (2) << 9;
          break;

        case 0650:  // spri4
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  000 -> C(Y-pair)0,2
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  C(PRn.WORDNO) -> C(Y-pair)36,53
          //  000 -> C(Y-pair)54,56
          //  C(PRn.BITNO) -> C(Y-pair)57,62
          //  00...0 -> C(Y-pair)63,71
          CPTUR (cptUsePRn + 4);
          cpu.Ypair[0] = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[4].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[4].RNR) << 15;

          cpu.Ypair[1] = (word36) cpu.PR[4].WORDNO << 18;
          cpu.Ypair[1] |= (word36) GET_PR_BITNO (4) << 9;
          break;

        case 0652:  // spri6
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  000 -> C(Y-pair)0,2
          //  C(PRn.SNR) -> C(Y-pair)3,17
          //  C(PRn.RNR) -> C(Y-pair)18,20
          //  00...0 -> C(Y-pair)21,29
          //  (43)8 -> C(Y-pair)30,35
          //  C(PRn.WORDNO) -> C(Y-pair)36,53
          //  000 -> C(Y-pair)54,56
          //  C(PRn.BITNO) -> C(Y-pair)57,62
          //  00...0 -> C(Y-pair)63,71
          CPTUR (cptUsePRn + 6);
          cpu.Ypair[0] = 043;
          cpu.Ypair[0] |= ((word36) cpu.PR[6].SNR) << 18;
          cpu.Ypair[0] |= ((word36) cpu.PR[6].RNR) << 15;

          cpu.Ypair[1] = (word36) cpu.PR[6].WORDNO << 18;
          cpu.Ypair[1] |= (word36) GET_PR_BITNO (6) << 9;
          break;

        case 0540:  // sprp0
        case 0541:  // sprp1
        case 0542:  // sprp2
        case 0543:  // sprp3
        case 0544:  // sprp4
        case 0545:  // sprp5
        case 0546:  // sprp6
        case 0547:  // sprp7
          // For n = 0, 1, ..., or 7 as determined by operation code
          //  C(PRn.BITNO) -> C(Y)0,5
          //  C(PRn.SNR)3,14 -> C(Y)6,17
          //  C(PRn.WORDNO) -> C(Y)18,35
          {
            uint32 n = opcode & 07;  // get n
            CPTUR (cptUsePRn + n);

            // If C(PRn.SNR)0,2 are nonzero, and C(PRn.SNR) != 11...1, then
            // a store fault (illegal pointer) will occur and C(Y) will not
            // be changed.

            if ((cpu.PR[n].SNR & 070000) != 0 && cpu.PR[n].SNR != MASK15)
              doFault (FAULT_STR,
                       (_fault_subtype)
                       {.fault_str_subtype=flt_str_ill_ptr},
                       "sprpn");

            cpu.CY  =  ((word36) (GET_PR_BITNO(n) & 077)) << 30;
            // lower 12- of 15-bits
            cpu.CY |=  ((word36) (cpu.PR[n].SNR & 07777)) << 18;
            cpu.CY |=  cpu.PR[n].WORDNO & PAMASK;
            cpu.CY &= DMASK;    // keep to 36-bits
          }
          break;

        /// Pointer Register Address Arithmetic

        case 0050:   // adwp0
        case 0051:   // adwp1
        case 0052:   // adwp2
        case 0053:   // adwp3
          // For n = 0, 1, ..., or 7 as determined by operation code
          //   C(Y)0,17 + C(PRn.WORDNO) -> C(PRn.WORDNO)
          //   00...0 -> C(PRn.BITNO)
          {
              uint32 n = opcode & 03;  // get n
              CPTUR (cptUsePRn + n);
              cpu.PR[n].WORDNO += GETHI (cpu.CY);
              cpu.PR[n].WORDNO &= MASK18;
              SET_PR_BITNO (n, 0);
          }
          break;

        case 0150:   // adwp4
        case 0151:   // adwp5
        case 0152:   // adwp6
        case 0153:   // adwp7
          // For n = 0, 1, ..., or 7 as determined by operation code
          //   C(Y)0,17 + C(PRn.WORDNO) -> C(PRn.WORDNO)
          //   00...0 -> C(PRn.BITNO)
          {
              uint32 n = (opcode & MASK3) + 4U;  // get n
              CPTUR (cptUsePRn + n);
              cpu.PR[n].WORDNO += GETHI (cpu.CY);
              cpu.PR[n].WORDNO &= MASK18;
              SET_PR_BITNO (n, 0);
          }
          break;

        /// Pointer Register Miscellaneous

        case 0213:  // epaq
          // 000 -> C(AQ)0,2
          // C(TPR.TSR) -> C(AQ)3,17
          // 00...0 -> C(AQ)18,32
          // C(TPR.TRR) -> C(AQ)33,35

          // C(TPR.CA) -> C(AQ)36,53
          // 00...0 -> C(AQ)54,65
          // C(TPR.TBR) -> C(AQ)66,71

          cpu.rA = cpu.TPR.TRR & MASK3;
          cpu.rA |= (word36) (cpu.TPR.TSR & MASK15) << 18;

          cpu.rQ = cpu.TPR.TBR & MASK6;
          cpu.rQ |= (word36) (cpu.TPR.CA & MASK18) << 18;

          SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);

          break;

        /// MISCELLANEOUS INSTRUCTIONS

        case 0633:  // rccl
          // 00...0 -> C(AQ)0,19
          // C(calendar clock) -> C(AQ)20,71
          {
// XXX see ticket #23
            // For the rccl instruction, the first 2 or 3 bits of the addr
            // field of the instruction are used to specify which SCU.
            // init_processor.alm systematically steps through the SCUs,
            // using addresses 000000 100000 200000 300000.
#ifdef DPS8M
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
#endif
#ifdef L68
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 07;
#endif
            int scu_unit_num =
              query_scu_unit_num ((int) currentRunningCPUnum,
                                  (int) cpu_port_num);
            sim_debug (DBG_TRACE, & cpu_dev,
                       "rccl CA %08o cpu port %o scu unit %d\n",
                       cpu.TPR.CA, cpu_port_num, scu_unit_num);
            if (scu_unit_num < 0)
              {
                sim_warn ("rccl on CPU %u port %d has no SCU; faulting\n",
                          currentRunningCPUnum, cpu_port_num);
                doFault (FAULT_ONC,
                         (_fault_subtype) {.fault_onc_subtype=flt_onc_nem},
                         "(rccl)");
              }

            t_stat rc = scu_rscr ((uint) scu_unit_num, currentRunningCPUnum,
                                  040, & cpu.rA, & cpu.rQ);
            if (rc > 0)
              return rc;
#ifndef SPEED
            if_sim_debug (DBG_TRACE, & cpu_dev)
              {
                // Clock at initialization
                // date -d "Tue Jul 22 16:39:38 PDT 1999" +%s
                // 932686778
                uint64 UnixSecs = 932686778;
                uint64 UnixuSecs = UnixSecs * 1000000LL;
                // now determine uSecs since Jan 1, 1901 ...
                uint64 MulticsuSecs = 2177452800000000LL + UnixuSecs;

                // Back into 72 bits
                __uint128_t big = ((__uint128_t) cpu.rA) << 36 | cpu.rQ;
                // Convert to time since boot
                big -= MulticsuSecs;

                unsigned long uSecs = big % 1000000u;
                unsigned long secs = (unsigned long) (big / 1000000u);
                sim_debug (DBG_TRACE, & cpu_dev,
                           "Clock time since boot %4lu.%06lu seconds\n",
                           secs, uSecs);
              }
#endif
          }
          break;

        case 0002:   // drl
          // Causes a fault which fetches and executes, in absolute mode, the
          // instruction pair at main memory location C+(14)8. The value of C
          // is obtained from the FAULT VECTOR switches on the processor
          // configuration panel.

          if (cpu.switches.drl_fatal)
            {
              return STOP_HALT;
            }
          doFault (FAULT_DRL, (_fault_subtype) {.bits=0}, "drl");

        case 0716:  // xec
          cpu.cu.xde = 1;
          cpu.cu.xdo = 0;
          cpu.cu.IWB = cpu.CY;
          return CONT_XEC;

        case 0717:  // xed
          // The xed instruction itself does not affect any indicator.
          // However, the execution of the instruction pair from C(Y-pair)
          // may affect indicators.
          //
          // The even instruction from C(Y-pair) must not alter
          // C(Y-pair)36,71, and must not be another xed instruction.
          //
          // If the execution of the instruction pair from C(Y-pair) alters
          // C(PPR.IC), then a transfer of control occurs; otherwise, the
          // next instruction to be executed is fetched from C(PPR.IC)+1. If
          // the even instruction from C(Y-pair) alters C(PPR.IC), then the
          // transfer of control is effective immediately and the odd
          // instruction is not executed.
          //
          // To execute an instruction pair having an rpd instruction as the
          // odd instruction, the xed instruction must be located at an odd
          // address. The instruction pair repeated is that instruction pair
          // at C PPR.IC)+1, that is, the instruction pair immediately
          // following the xed instruction. C(PPR.IC) is adjusted during the
          // execution of the repeated instruction pair so the the next
          // instruction fetched for execution is from the first word
          // following the repeated instruction pair.
          //
          // The instruction pair at C(Y-pair) may cause any of the processor
          // defined fault conditions, but only the directed faults (0,1,2,3)
          // and the access violation fault may be restarted successfully by
          // the hardware. Note that the software induced fault tag (1,2,3)
          // faults cannot be properly restarted.
          //
          //  An attempt to execute an EIS multiword instruction causes an
          //  illegal procedure fault.
          //
          //  Attempted repetition with the rpt, rpd, or rpl instructions
          //  causes an illegal procedure fault.

          cpu.cu.xde = 1;
          cpu.cu.xdo = 1;
          cpu.cu.IWB = cpu.Ypair[0];
          cpu.cu.IRODD = cpu.Ypair[1];
          return CONT_XEC;

        case 0001:   // mme
          if (sim_deb_mme_cntdwn > 0)
            sim_deb_mme_cntdwn --;
          // Causes a fault that fetches and executes, in absolute mode, the
          // instruction pair at main memory location C+4. The value of C is
          // obtained from the FAULT VECTOR switches on the processor
          // configuration panel.
          doFault (FAULT_MME, (_fault_subtype) {.bits=0},
                   "Master Mode Entry (mme)");

        case 0004:   // mme2
          // Causes a fault that fetches and executes, in absolute mode, the
          // instruction pair at main memory location C+(52)8. The value of C
          // is obtained from the FAULT VECTOR switches on the processor
          // configuration panel.
          doFault (FAULT_MME2, (_fault_subtype) {.bits=0},
                   "Master Mode Entry 2 (mme2)");

        case 0005:   // mme3
          // Causes a fault that fetches and executes, in absolute mode, the
          // instruction pair at main memory location C+(54)8. The value of C
          // is obtained from the FAULT VECTOR switches on the processor
          // configuration panel.
          doFault (FAULT_MME3, (_fault_subtype) {.bits=0},
                   "Master Mode Entry 3 (mme3)");

        case 0007:   // mme4
          // Causes a fault that fetches and executes, in absolute mode, the
          // instruction pair at main memory location C+(56)8. The value of C
          // is obtained from the FAULT VECTOR switches on the processor
          // configuration panel.
          doFault (FAULT_MME4, (_fault_subtype) {.bits=0},
                   "Master Mode Entry 4 (mme4)");

        case 0011:   // nop
          break;

        case 0012:   // puls1
          break;

        case 0013:   // puls2
          break;

        /// Repeat

        case 0560:  // rpd
          {
            if ((cpu.PPR.IC & 1) == 0)
              doFault (FAULT_IPR,
                       (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                       "lcpr tag invalid");
            cpu.cu.delta = i->tag;
            // a:AL39/rpd1
            word1 c = (i->address >> 7) & 1;
            if (c)
              cpu.rX[0] = i->address;    // Entire 18 bits
            cpu.cu.rd = 1;
            cpu.cu.repeat_first = 1;
          }
          break;

        case 0500:  // rpl
          {
            uint c = (i->address >> 7) & 1;
            cpu.cu.delta = i->tag;
            if (c)
              cpu.rX[0] = i->address;    // Entire 18 bits
            cpu.cu.rl = 1;
            cpu.cu.repeat_first = 1;
          }
          break;

        case 0520:  // rpt
          {
            uint c = (i->address >> 7) & 1;
            cpu.cu.delta = i->tag;
            if (c)
              cpu.rX[0] = i->address;    // Entire 18 bits
            cpu.cu.rpt = 1;
            cpu.cu.repeat_first = 1;
          }
          break;

        /// Store Base Address Register

        case 0550:  // sbar
          // C(BAR) -> C(Y) 0,17
          CPTUR (cptUseBAR);
          SETHI (cpu.CY, (cpu.BAR.BASE << 9) | cpu.BAR.BOUND);

          break;


        /// Translation

        case 0505:  // bcd
          // Shift C(A) left three positions
          // | C(A) | / C(Y) -> 4-bit quotient
          // C(A) - C(Y) * quotient -> remainder
          // Shift C(Q) left six positions
          // 4-bit quotient -> C(Q)32,35
          // remainder -> C(A)

          {
//IF1 sim_printf("BCD A %012"PRIo64" Q %012"PRIo64" Y %012"PRIo64"\n", cpu.rA & MASK36, cpu.rQ & MASK36, cpu.CY); 
            word36 tmp1 = cpu.rA & SIGN36; // A0
            word36 tmp36 = (cpu.rA << 3) & DMASK;
            word36 tmp36q = tmp36 / cpu.CY; // this may be more than 4 bits, keep it for remainder calculation
            word36 tmp36r = 0;
            if (!tmp1) {
                tmp36r = tmp36 - tmp36q * cpu.CY;
            } else {
                // ISOLTS-745 05i: bcd when rA is negative.
                // Note that this only gets called in the first round of the bcd
                // conversion; the rA sign bit will get shifted out.
                // Looking at the expected results, it appears that a 'borrow'
                // is represented in a residue style notation -- an unborrow
                // result is 0-9 (000 - 011), a borrowed digit as 6-15 (006-017)
                tmp36q += 6;
                tmp36r = tmp36 + tmp36q * cpu.CY;
            }

            cpu.rQ <<= 6;       // Shift C(Q) left six positions
            cpu.rQ &= DMASK;

            //cpu.rQ &= (word36) ~017;     // 4-bit quotient -> C(Q)32,35  lo6 bits already zeroed out
            cpu.rQ |= (tmp36q & 017);

            cpu.rA = tmp36r & DMASK;    // remainder -> C(A)

//IF1 sim_printf("BCD final A %012"PRIo64" Q %012"PRIo64"\n", cpu.rA & MASK36, cpu.rQ & MASK36); 

            SC_I_ZERO (cpu.rA == 0);  // If C(A) = 0, then ON;
                                            // otherwise OFF
            SC_I_NEG (tmp1);   // If C(A)0 = 1 before execution,
                                            // then ON; otherwise OFF
          }
          break;

        case 0774:  // gtb
          // C(A)0 -> C(A)0
          // C(A)i XOR C(A)i-1 -> C(A)i for i = 1, 2, ..., 35
          {
            word36 tmp = cpu.rA & MASK36;
            word36 mask = SIGN36;

            for (int n=1;n<=35;n++) {
                tmp ^= (tmp & mask) >> 1;
                mask >>= 1;
            }
            
            cpu.rA = tmp;
            
            SC_I_ZERO (cpu.rA == 0);  // If C(A) = 0, then ON;
                                      // otherwise OFF
            SC_I_NEG (cpu.rA & SIGN36);   // If C(A)0 = 1, then ON; 
                                          // otherwise OFF
          }
          break;

        /// REGISTER LOAD

        case 0230:  // lbar
          // C(Y)0,17 -> C(BAR)
          CPTUR (cptUseBAR);
          // BAR.BASE is upper 9-bits (0-8)
          cpu.BAR.BASE = (GETHI (cpu.CY) >> 9) & 0777;
          // BAR.BOUND is next lower 9-bits (9-17)
          cpu.BAR.BOUND = GETHI (cpu.CY) & 0777;
          break;

        /// PRIVILEGED INSTRUCTIONS

        /// Privileged - Register Load

        case 0674:  // lcpr
          // DPS8M interpratation
//IF1 sim_printf ("lcpr %d\n", i->tag);
          switch (i->tag)
            {
              // Extract bits from 'from' under 'mask' shifted to where (where
              // is dps8 '0 is the msbit.

#define GETBITS(from,mask,where) \
 (((from) >> (35 - (where))) & (word36) (mask))

              case 02: // cache mode register
                {
                  //cpu.CMR = cpu.CY;
                  // cpu.CMR.cache_dir_address = <ignored for lcpr>
                  // cpu.CMR.par_bit = <ignored for lcpr>
                  // cpu.CMR.lev_ful = <ignored for lcpr>

                  CPTUR (cptUseCMR);
                  // a:AL39/cmr2  If either cache enable bit c or d changes
                  // from disable state to enable state, the entire cache is
                  // cleared.
                  uint csh1_on = GETBITS (cpu.CY, 1, 72 - 54);
                  uint csh2_on = GETBITS (cpu.CY, 1, 72 - 55);
                  //bool clear = (cpu.CMR.csh1_on == 0 && csh1_on != 0) ||
                               //(cpu.CMR.csh1_on == 0 && csh1_on != 0);
                  cpu.CMR.csh1_on = (word1) csh1_on;
                  cpu.CMR.csh2_on = (word1) csh2_on;
                  //if (clear) // a:AL39/cmr2
                    //{
                    //}
#ifdef L68
                  cpu.CMR.opnd_on = GETBITS (cpu.CY, 1, 72 - 56);
#endif
                  cpu.CMR.inst_on = GETBITS (cpu.CY, 1, 72 - 57);
                  cpu.CMR.csh_reg = GETBITS (cpu.CY, 1, 72 - 59);
                  if (cpu.CMR.csh_reg)
                    sim_warn ("LCPR set csh_reg\n");
                  // cpu.CMR.str_asd = <ignored for lcpr>
                  // cpu.CMR.col_ful = <ignored for lcpr>
                  // cpu.CMR.rro_AB = GETBITS (cpu.CY, 1, 18);
#ifdef DPS8M
                  cpu.CMR.bypass_cache = GETBITS (cpu.CY, 1, 72 - 68);
#endif
                  cpu.CMR.luf = GETBITS (cpu.CY, 2, 72 - 72);
                }
                break;

              case 04: // mode register
                {
#if 1
                  CPTUR (cptUseMR);
                  cpu.MR.r = cpu.CY;
// XXX TEST/NORMAL switch is set to NORMAL
                  putbits36_1 (& cpu.MR.r, 32, 0);
// SBZ
                  putbits36_2 (& cpu.MR.r, 33, 0);
#ifdef L68
                  cpu.MR.FFV = getbits36_15 (cpu.CY, 0);
                  cpu.MR.OC_TRAP = getbits36_1 (cpu.CY, 16);
                  cpu.MR.ADR_TRAP = getbits36_1 (cpu.CY, 17);
                  cpu.MR.OPCODE = getbits36_9 (cpu.CY, 18);
                  cpu.MR.OPCODEX = getbits36_1 (cpu.CY, 27);
#endif
                  cpu.MR.sdpap = getbits36_1 (cpu.CY, 20);
                  cpu.MR.separ = getbits36_1 (cpu.CY, 21);
                  cpu.MR.hrhlt = getbits36_1 (cpu.CY, 28);
#ifdef DPS8M
                  cpu.MR.hrxfr = getbits36_1 (cpu.CY, 29);
#endif
                  cpu.MR.ihr = getbits36_1 (cpu.CY, 30);
                  cpu.MR.ihrrs = getbits36_1 (cpu.CY, 31);
                  cpu.MR.emr = getbits36_1 (cpu.CY, 35);
#ifdef DPS8M
                  cpu.MR.hexfp = getbits36_1 (cpu.CY, 33);
#endif
//IF1 sim_printf ("hrhlt %u ihr %u emr %u\n", cpu.MR.hrhlt, cpu.MR.ihr, cpu.MR.emr);
#else
IF1 sim_printf ("set mode register %012"PRIo64"\n", cpu.CY);
#ifdef L68
                  cpu.MR.FFV = getbits36_15 (cpu.CY, 0);
                  cpu.MR.isolts_tracks = getbits36_1 (cpu.CY, 15);
                  cpu.MR.OC_TRAP = getbits36_1 (cpu.CY, 16);
                  cpu.MR.ADR_TRAP = getbits36_1 (cpu.CY, 17);
                  cpu.MR.hropc = getbits36_1 (cpu.CY, 29);
#if 1
                  //if (cpu.MR.OC_TRAP)
                  if (cpu.MR.OC_TRAP || cpu.MR.hropc)
                    {
                      cpu.MR.OPCODE = getbits36_10 (cpu.CY, 18);
                    }
                  else
#endif
                    {
                      cpu.MR.cuolin = getbits36_1 (cpu.CY, 18);
                      cpu.MR.solin = getbits36_1 (cpu.CY, 19);
                      cpu.MR.sdpap = getbits36_1 (cpu.CY, 20);
                      cpu.MR.separ = getbits36_1 (cpu.CY, 21);
// tm/vm are only set if the processor maintainence panel PROG switch is on 
#if 1
                      cpu.MR.tm = getbits36_2 (cpu.CY, 22);
                      cpu.MR.vm = getbits36_2 (cpu.CY, 24);
                      cpu.MR.isolts_tracks2 = getbits36_2 (cpu.CY, 26);
#endif
                    }
                  cpu.MR.hrhlt = getbits36_1 (cpu.CY, 28);
                  // Captured above
                  //cpu.MR.hropc = getbits36_1 (cpu.CY, 29);
                  cpu.MR.ihr = getbits36_1 (cpu.CY, 30);
                  cpu.MR.ihrrs = getbits36_1 (cpu.CY, 31);
                  //cpu.MR.mrgctl = getbits36_1 (cpu.CY, 32);
                  cpu.MR.emr = getbits36_1 (cpu.CY, 35);
#endif
#ifdef DPS8M
                  cpu.MR.cuolin = getbits36_1 (cpu.CY, 18);
                  cpu.MR.solin = getbits36_1 (cpu.CY, 19);
                  cpu.MR.sdpap = getbits36_1 (cpu.CY, 20);
                  cpu.MR.separ = getbits36_1 (cpu.CY, 21);
// tm/vm are only set if the processor maintainence panel PROG switch is on 
#if 1
                  cpu.MR.tm = getbits36_2 (cpu.CY, 22);
                  cpu.MR.vm = getbits36_2 (cpu.CY, 24);
                  cpu.MR.isolts_tracks2 = getbits36_2 (cpu.CY, 26);
#endif
                  cpu.MR.hrhlt = getbits36_1 (cpu.CY, 28);
                  cpu.MR.hrxfr = getbits36_1 (cpu.CY, 29);
                  cpu.MR.ihr = getbits36_1 (cpu.CY, 30);
                  cpu.MR.ihrrs = getbits36_1 (cpu.CY, 31);
                  //cpu.MR.mrgctl = getbits36_1 (cpu.CY, 32);
                  cpu.MR.hexfp = getbits36_1 (cpu.CY, 33);
                  cpu.MR.emr = getbits36_1 (cpu.CY, 35);
#endif
#endif

                  // Stop HR Strobe on HR Counter Overflow. (Setting bit 28
                  // shall cause the HR counter to be reset to zero.)
                  // CAC: It is unclear if bit 28 is edge or level 
                  // triggered; assuming level for simplicity.
                  if (cpu.MR.hrhlt)
                    {
                      for (uint hset = 0; hset < N_HIST_SETS; hset ++)
                         cpu.history_cyclic[hset] = 0;
                    }

#if 0 
                  if (cpu.MR.sdpap)
                    {
                      sim_warn ("LCPR set SDPAP\n");
                    }

                  if (cpu.MR.separ)
                    {
                      sim_warn ("LCPR set SEPAR\n");
                    }
#endif
                }
                break;

              case 03: // 0's -> history
                {
IF1 sim_printf ("0-> %u\n", cpu.history_cyclic[CU_HIST_REG]);
                  for (uint i = 0; i < N_HIST_SETS; i ++)
                    addHistForce (i, 0, 0);
// XXX ISOLTS pm700 test-01n 
// The test clears the history registers but with ihr & emr set, causing
// the registers to fill with alternating 0's and lcpr instructions.
// Set flag to prevent the LCPR from being recorded.
                    //cpu.MR.ihr = 0;
                    cpu.skip_cu_hist = true;
                     
                }
                break;

              case 07: // 1's -> history
                {
IF1 sim_printf ("1-> %u\n", cpu.history_cyclic[CU_HIST_REG]);
                  for (uint i = 0; i < N_HIST_SETS; i ++)
                    addHistForce (i, MASK36, MASK36);
// XXX ISOLTS pm700 test-01n 
// The test clears the history registers but with ihr & emr set, causing
// the registers to fill with alternating 0's and lcpr instructions.
// Set flag to prevent the LCPR from being recorded.
                    //cpu.MR.ihr = 0;
                    cpu.skip_cu_hist = true;
                }
                break;

              default:
                doFault (FAULT_IPR,
                         (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                         "lcpr tag invalid");

            }
            break;

        case 0232:  // ldbr
          do_ldbr (cpu.Ypair);
          break;

        case 0637:  // ldt
          CPTUR (cptUseTR);
          cpu.rTR = (cpu.CY >> 9) & MASK27;
#if ISOLTS
          cpu.shadowTR = cpu.rTR;
          cpu.rTRlsb = 0;
//IF1 sim_printf ("CPU A ldt %d. (%o)\n", cpu.rTR, cpu.rTR);
#endif
          sim_debug (DBG_TRACE, & cpu_dev, "ldt TR %d (%o)\n",
                     cpu.rTR, cpu.rTR);
#ifdef LOOPTRC
elapsedtime ();
 sim_printf (" ldt %d  PSR:IC %05o:%06o\r\n", cpu.rTR, cpu.PPR.PSR, cpu.PPR.IC);
#endif
          // Undocumented feature. return to bce has been observed to
          // experience TRO while masked, setting the TR to -1, and
          // experiencing an unexpected TRo interrupt when unmasking.
          // Reset any pending TRO fault when the TR is loaded.
          clearTROFault ();
          break;

        case 0257:  // lsdp
#ifdef DPS8M
          break;
#endif
#ifdef L68
          {
            // For i = 0, 1, ..., 15
            //   m = C(SDWAM(i).USE)
            //   C(Y-block16+m)0,14 -> C(SDWAM(m).POINTER)
            //   C(Y-block16+m)27 -> C(SDWAM(m).F) Note: typo in AL39, P(17) should be F(27)
#ifdef WAM
            for (uint i = 0; i < 16; i ++)
              {
                word4 m = cpu.SDWAM[i].USE;
                cpu.SDWAM[m].POINTER = getbits36_15 (cpu.Yblock16[i],  0);
                cpu.SDWAM[m].FE =      getbits36_1  (cpu.Yblock16[i], 27);
              }
#endif
          }
          break;
#endif

        case 0613:  // rcu
          doRCU (); // never returns

        /// Privileged - Register Store

        case 0452:  // scpr
          {
            uint tag = (i->tag) & MASK6;
//IF1 sim_printf ("scpr %d\n", i->tag);
            switch (tag)
              {
                case 000: // C(APU history register#1) -> C(Y-pair)
                  {
                    cpu.Ypair[0] =
                      cpu.history[APU_HIST_REG]
                                 [cpu.history_cyclic[APU_HIST_REG]][0];
                    cpu.Ypair[1] =
                      cpu.history[APU_HIST_REG]
                                 [cpu.history_cyclic[APU_HIST_REG]][1];
                    cpu.history_cyclic[APU_HIST_REG] =
                      (cpu.history_cyclic[APU_HIST_REG] + 1) % N_HIST_SIZE;
                  }
                  break;

                case 001: // C(fault register) -> C(Y-pair)0,35
                          // 00...0 -> C(Y-pair)36,71
                  {
                    CPTUR (cptUseFR);
                    cpu.Ypair[0] = cpu.faultRegister[0];
                    cpu.Ypair[1] = cpu.faultRegister[1];
                    cpu.faultRegister[0] = 0;
                    cpu.faultRegister[1] = 0;
                  }
                  break;

                case 006: // C(mode register) -> C(Y-pair)0,35
                          // C(cache mode register) -> C(Y-pair)36,72
                  {
                    CPTUR (cptUseMR);
                    cpu.Ypair[0] = cpu.MR.r;
                    putbits36_1 (& cpu.Ypair[0], 20, cpu.MR.sdpap);
                    putbits36_1 (& cpu.Ypair[0], 21, cpu.MR.separ);
                    putbits36_1 (& cpu.Ypair[0], 30, cpu.MR.ihr);
#ifdef DPS8M
                    putbits36_1 (& cpu.Ypair[0], 33, cpu.MR.hexfp);
#endif
#if 0
                    cpu.Ypair[0] = 0;
#ifdef L68
                    putbits36_15 (& cpu.Ypair[0], 0, cpu.MR.FFV);
                    putbits36_1 (& cpu.Ypair[0], 15, cpu.MR.isolts_tracks);
                    putbits36_1 (& cpu.Ypair[0], 16, cpu.MR.OC_TRAP);
                    putbits36_1 (& cpu.Ypair[0], 17, cpu.MR.ADR_TRAP);
#if 1
                    if (cpu.MR.OC_TRAP || cpu.MR.hropc)
                      {
                        putbits36_10 (& cpu.Ypair[0], 18, cpu.MR.OPCODE);
                      }
                    else
#endif
                      {
                        putbits36_1 (& cpu.Ypair[0], 18, cpu.MR.cuolin);
                        putbits36_1 (& cpu.Ypair[0], 19, cpu.MR.solin);
                        putbits36_1 (& cpu.Ypair[0], 20, cpu.MR.sdpap);
                        putbits36_1 (& cpu.Ypair[0], 21, cpu.MR.separ);
// tm/vm are only set if the processor maintainence panel PROG switch is on 
#if 1
                        putbits36_2 (& cpu.Ypair[0], 22, cpu.MR.tm);
                        putbits36_2 (& cpu.Ypair[0], 24, cpu.MR.vm);
#else
                        putbits36_2 (& cpu.Ypair[0], 22, 01llu);
                        putbits36_2 (& cpu.Ypair[0], 24, 01llu);
#endif
                      }
#endif
#ifdef DPS8M
                    putbits36_1 (& cpu.Ypair[0], 18, cpu.MR.cuolin);
                    putbits36_1 (& cpu.Ypair[0], 19, cpu.MR.solin);
                    putbits36_1 (& cpu.Ypair[0], 20, cpu.MR.sdpap);
                    putbits36_1 (& cpu.Ypair[0], 21, cpu.MR.separ);
#endif
// tm/vm are only set if the processor maintainence panel PROG switch is on 
#if 1
                    putbits36_2 (& cpu.Ypair[0], 22, cpu.MR.tm);
                    putbits36_2 (& cpu.Ypair[0], 24, cpu.MR.vm);
                    putbits36_2 (& cpu.Ypair[0], 26, cpu.MR.isolts_tracks2);
#else
                    putbits36_2 (& cpu.Ypair[0], 22, 01llu);
                    putbits36_2 (& cpu.Ypair[0], 24, 01llu);
                    putbits36_2 (& cpu.Ypair[0], 26, 03llu);
#endif
                    putbits36_1 (& cpu.Ypair[0], 28, cpu.MR.hrhlt);
#ifdef DPS8M
                    putbits36_1 (& cpu.Ypair[0], 29, cpu.MR.hrxfr);
#endif
#ifdef L68
                    putbits36_1 (& cpu.Ypair[0], 29, cpu.MR.hropc);
#endif
                    putbits36_1 (& cpu.Ypair[0], 30, cpu.MR.ihr);
                    putbits36_1 (& cpu.Ypair[0], 31, cpu.MR.ihrrs);
                    //putbits36_1 (& cpu.Ypair[0], 32, cpu.MR.mrgctl);
#ifdef DPS8M
                    putbits36_1 (& cpu.Ypair[0], 33, cpu.MR.hexfp);
#endif
                    putbits36_1 (& cpu.Ypair[0], 35, cpu.MR.emr);
IF1 sim_printf ("get mode register %012"PRIo64"\n", cpu.Ypair[0]);
#endif
                    CPTUR (cptUseCMR);
                    cpu.Ypair[1] = 0;
                    putbits36_15 (& cpu.Ypair[1], 36 - 36,
                                  cpu.CMR.cache_dir_address);
                    putbits36_1 (& cpu.Ypair[1], 51 - 36, cpu.CMR.par_bit);
                    putbits36_1 (& cpu.Ypair[1], 52 - 36, cpu.CMR.lev_ful);
                    putbits36_1 (& cpu.Ypair[1], 54 - 36, cpu.CMR.csh1_on);
                    putbits36_1 (& cpu.Ypair[1], 55 - 36, cpu.CMR.csh2_on);
#ifdef L68
                    putbits36_1 (& cpu.Ypair[1], 56 - 36, cpu.CMR.opnd_on);
#endif
                    putbits36_1 (& cpu.Ypair[1], 57 - 36, cpu.CMR.inst_on);
                    putbits36_1 (& cpu.Ypair[1], 59 - 36, cpu.CMR.csh_reg);
                    putbits36_1 (& cpu.Ypair[1], 60 - 36, cpu.CMR.str_asd);
                    putbits36_1 (& cpu.Ypair[1], 61 - 36, cpu.CMR.col_ful);
                    putbits36_2 (& cpu.Ypair[1], 62 - 36, cpu.CMR.rro_AB);
#ifdef DPS8M
                    putbits36_1 (& cpu.Ypair[1], 68 - 36, 
                                 cpu.CMR.bypass_cache);
#endif
                    putbits36_2 (& cpu.Ypair[1], 70 - 36, cpu.CMR.luf);
//IF1 sim_printf ("get mode register %012"PRIo64"\n", cpu.Ypair[1]);
                  }
                  break;

                case 010: // C(APU history register#2) -> C(Y-pair)
                  {
#ifdef DPS8M
                    cpu.Ypair[0] =
                      cpu.history[EAPU_HIST_REG]
                                 [cpu.history_cyclic[EAPU_HIST_REG]][0];
                    cpu.Ypair[1] =
                      cpu.history[EAPU_HIST_REG]
                                 [cpu.history_cyclic[EAPU_HIST_REG]][1];
                    cpu.history_cyclic[EAPU_HIST_REG] =
                      (cpu.history_cyclic[EAPU_HIST_REG] + 1) % N_HIST_SIZE;
#endif
#ifdef L68
                    cpu.Ypair[0] =
                      cpu.history[DU_HIST_REG]
                                 [cpu.history_cyclic[DU_HIST_REG]][0];
                    cpu.Ypair[1] =
                      cpu.history[DU_HIST_REG]
                                 [cpu.history_cyclic[DU_HIST_REG]][1];
                    cpu.history_cyclic[DU_HIST_REG] =
                      (cpu.history_cyclic[DU_HIST_REG] + 1) % N_HIST_SIZE;
#endif
                  }
                  break;

                case 020: // C(CU history register) -> C(Y-pair)
                  {
                    cpu.Ypair[0] =
                      cpu.history[CU_HIST_REG]
                                 [cpu.history_cyclic[CU_HIST_REG]][0];
                    cpu.Ypair[1] =
                      cpu.history[CU_HIST_REG]
                                 [cpu.history_cyclic[CU_HIST_REG]][1];
//IF1 sim_printf ("scpr cu %u %012"PRIo64" %012"PRIo64"\n", cpu.history_cyclic[CU_HIST_REG], cpu.Ypair[0], cpu.Ypair[1]);
                    cpu.history_cyclic[CU_HIST_REG] =
                      (cpu.history_cyclic[CU_HIST_REG] + 1) % N_HIST_SIZE;
                  }
                  break;

                case 040: // C(OU/DU history register) -> C(Y-pair)
                  {
#ifdef DPS8M
                    cpu.Ypair[0] =
                      cpu.history[DU_OU_HIST_REG]
                                 [cpu.history_cyclic[DU_OU_HIST_REG]][0];
                    cpu.Ypair[1] = 
                     cpu.history[DU_OU_HIST_REG]
                                 [cpu.history_cyclic[DU_OU_HIST_REG]][1];
                    cpu.history_cyclic[DU_OU_HIST_REG] =
                      (cpu.history_cyclic[DU_OU_HIST_REG] + 1) % N_HIST_SIZE;
#endif
#ifdef L68
                    cpu.Ypair[0] =
                      cpu.history[OU_HIST_REG]
                                 [cpu.history_cyclic[OU_HIST_REG]][0];
                    cpu.Ypair[1] = 
                      cpu.history[OU_HIST_REG]
                                 [cpu.history_cyclic[OU_HIST_REG]][1];
                    cpu.history_cyclic[OU_HIST_REG] =
                      (cpu.history_cyclic[OU_HIST_REG] + 1) % N_HIST_SIZE;
#endif
                  }
                  break;

                default:
                  {
                    doFault (FAULT_IPR,
                             (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                             "SCPR Illegal register select value");
                  }
              }
          }
          break;

        case 0657:  // scu
          // AL-39 defines the behaivor of SCU during fault/interrupt
          // processing, but not otherwise.
          // The T&D tape uses SCU during normal processing, and apparently
          // expects the current CU state to be saved.

          if (cpu.cycle == EXEC_cycle)
            {
              // T&D behavior

              // An 'Add Delta' addressing mode will alter the TALLY bit;
              // restore it.
              //SC_I_TALLY (cpu.currentInstruction.stiTally == 0);

              scu2words (cpu.Yblock8);
            }
          else
            {
              // AL-39 behavior
              for (int j = 0; j < 8; j ++)
                cpu.Yblock8[j] = cpu.scu_data[j];
            }
          break;

        case 0154:  // sdbr
          do_sdbr (cpu.Ypair);
          break;

        case 0557:  // ssdp
          {
            // XXX AL39: The associative memory is ignored (forced to "no match")
            // during address preparation.
            // Level j is selected by C(TPR.CA)12,13
#ifdef DPS8M
            uint level = (cpu.TPR.CA >> 4) & 03;
#endif
#ifdef L68
            uint level = 0;
#endif
#ifdef WAM
            uint toffset = level * 16;
#endif
            for (uint j = 0; j < 16; j ++)
              {
                cpu.Yblock16[j] = 0;
#ifdef WAM
                putbits36_15 (& cpu.Yblock16[j], 0,
                           cpu.SDWAM[toffset + j].POINTER);
                putbits36_1 (& cpu.Yblock16[j], 27,
                           cpu.SDWAM[toffset + j].FE);
#ifdef DPS8M
                uint parity = 0;
                if (cpu.SDWAM[toffset + j].FE)
                {
                    // calculate parity
                    // 58009997-040 p.112
                    parity = cpu.SDWAM[toffset + j].POINTER >> 4;
                    //parity = parity ^ (parity >>16);
                    parity = parity ^ (parity >> 8);
                    parity = parity ^ (parity >> 4);
                    parity = ~ (0x6996u >> (parity & 0xf)); 
                }
                putbits36_1 (& cpu.Yblock16[j], 15, parity);

                putbits36_6 (& cpu.Yblock16[j], 30,
                           cpu.SDWAM[toffset + j].USE);
#endif
#ifdef L68
                putbits36_4 (& cpu.Yblock16[j], 32,
                           cpu.SDWAM[toffset + j].USE);
#endif
#endif
              }
#ifndef WAM
            if (level == 0)
              {
                putbits36 (& cpu.Yblock16[0], 0, 15,
                           cpu.SDW0.POINTER);
                putbits36 (& cpu.Yblock16[0], 27, 1,
                           cpu.SDW0.FE);
#ifdef DPS8M
                putbits36 (& cpu.Yblock16[0], 30, 6,
                           cpu.SDW0.USE);
#endif
#ifdef L68
                putbits36 (& cpu.Yblock16[0], 32, 4,
                           cpu.SDW0.USE);
#endif
              }
#endif
          }
          break;

        /// Privileged - Clear Associative Memory

        case 0532:  // cams
          do_cams (cpu.TPR.CA);
          break;

        /// Privileged - Configuration and Status

        case 0233:  // rmcm
          {
            // C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor)
            // specify which processor port (i.e., which system
            // controller) is used.
#ifdef DPS8M
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
#endif
#ifdef L68
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 07;
#endif
            int scu_unit_num =
              query_scu_unit_num ((int) currentRunningCPUnum, 
                                  (int) cpu_port_num);
            if (scu_unit_num < 0)
              {
                sim_warn ("rmcm to non-existent controller on "
                          "cpu %d port %d\n",
                          currentRunningCPUnum, cpu_port_num);
                break;
              }
            t_stat rc = scu_rmcm ((uint) scu_unit_num,
                                  currentRunningCPUnum,
                                  & cpu.rA, & cpu.rQ);
            if (rc)
              return rc;
            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0413:  // rscr
          {
            // For the rscr instruction, the first 2 (DPS8M) or 3 (L68) bits of
            // the addr field of the instruction are used to specify which SCU.
            // (2 bits for the DPS8M. (Expect for x6x and x7x below, where 
            // the selected SCU is the one holding the addressed memory).
            
            // According to DH02:
            //   XXXXXX0X  SCU Mode Register (Level 66 only)
            //   XXXXXX1X  Configuration switches
            //   XXXXXn2X  Interrupt mask port n
            //   XXXXXX3X  Interrupt cells
            //   XXXXXX4X  Elapsed time clock
            //   XXXXXX5X  Elapsed time clock
            //   XXXXXX6X  Mode register
            //   XXXXXX7X  Mode register

            // According to privileged_mode_ut,
            //   port*1024 + scr_input*8

            // privileged_mode_ut makes no reference to the special case
            // of x6x and x7x.


            // According to DH02, RSCR in Slave Mode does the CAF
            // without BAR correction, and then forces the CA to 040,
            // resulting in a Clock Read from the SCU on port 0.

            // According to AL93, RSCR in BAR mode is IPR.


//
// Implementing privileged_mode_ut.alm algorithm
//

           // Extract port number
#ifdef DPS8M
            uint cpu_port_num = (cpu.TPR.CA >> 10) & 03;
#endif
#ifdef L68
            uint cpu_port_num = (cpu.TPR.CA >> 10) & 07;
#endif


            // Trace the cable from the port to find the SCU number
            // connected to that port
            int scu_unit_num =
              query_scu_unit_num ((int) currentRunningCPUnum,
                                  (int) cpu_port_num);

            // If none such, fault...
            if (scu_unit_num < 0)
              {
                // CPTUR (cptUseFR) -- will be set by doFault

                // Set IAn in Fault register
                if (cpu_port_num == 0)
                  putbits36 (& cpu.faultRegister[0], 16, 4, 010);
                else if (cpu_port_num == 1)
                  putbits36 (& cpu.faultRegister[0], 20, 4, 010);
                else if (cpu_port_num == 2)
                  putbits36 (& cpu.faultRegister[0], 24, 4, 010);
                else
                  putbits36 (& cpu.faultRegister[0], 28, 4, 010);

                doFault (FAULT_CMD,
                         (_fault_subtype)
                          {.fault_cmd_subtype=flt_cmd_not_control},
                         "(rscr)");
              }

#ifdef PANEL
            {
               uint function = (cpu.iefpFinalAddress >> 3) & 07;
               CPT (cpt13L, function);
            }
#endif
            t_stat rc = scu_rscr ((uint) scu_unit_num, currentRunningCPUnum,
                                  cpu.iefpFinalAddress & MASK15,
                                  & cpu.rA, & cpu.rQ);
            if (rc)
              return rc;
          }

          break;

        case 0231:  // rsw
          {
#ifdef DPS8M
            //if (i->tag == TD_DL)
            word6 rTAG = GET_TAG (IWB_IRODD);
            word6 Td = GET_TD (rTAG);
            word6 Tm = GET_TM (rTAG);
            if (Tm == TM_R && Td == TD_DL)
              {

// 58009997-040 MULTICS Differences Manual DPS 8-70M Aug83
// disagress with Multics source, but probably a typo,
//  0-13 CPU Model Number
// 13-25 CPU Serial Number
// 26-33 Date-Ship code (YYMMDD)
// 34-40 CPU ID Field (reference RSW 2)
//  Byte 40: Bits 03 (Bits 32-35 of RSW 2 Field
//           Bit 4=1 Hex Option included
//           Bit 5=1 RSCR (Clock) is Slave Mode included
//           Bits 6-7 Reserved for later use.
//       50: Operating System Use
// 51-1777(8) To be defined.
// NOTE: There is the possibility of disagreement between the
//       ID bits of RSW 2 and the ID bits of PROM locations
//       35-40. This condition could result when alterable
//       configuration condition is contained in the PROM.
//       The user is adviced to ignore the PROM fields which
//       contain the processor fault vector base (GCOS III)
//       and the processor number and rely on the RSW 2 bits
//       for this purpose. Bits 14-16 of the RSW 2 should be
//       ignored and the bits represnting this information in
//       the PROM should be treated as valid.

// CAC notes: I interpret the fields as
//  0-12 CPU Model Number  // 13 chars, typo
// 13-25 CPU Serial Number // 13 chars
// 26-33 Date-Ship code (YYMMDD) // 8 chars (enough for YYYYMMDD).
// 34-40 CPU ID Field (reference RSW 2)
//  Byte 40: Bits 03 (Bits 32-35 of RSW 2 Field
//           Bit 4=1 Hex Option included
//           Bit 5=1 RSCR (Clock) is Slave Mode included
//           Bits 6-7 Reserved for later use.
//       50: Operating System Use

                unsigned char PROM[1024];
                memset (PROM, 0, sizeof (PROM));
                sprintf ((char *) PROM, "%13s%13d%8s",
                  "DPS8/70M Emul",  //  0-12 CPU Model number
                  cpu.switches.serno, // 13-25 CPU Serial number
                  "20160304");      // 26-33 Ship date (YYMMDD)
                word36 tmp = 0;
                tmp |= (word36) ((cpu.switches.interlace[0] == 2 ? 1LL : 0LL)
                       << (35- 0));
                tmp |= (word36) ((cpu.switches.interlace[1] == 2 ? 1LL : 0LL)
                       << (35- 1));
                tmp |= (word36) ((cpu.switches.interlace[2] == 2 ? 1LL : 0LL)
                       << (35- 2));
                tmp |= (word36) ((cpu.switches.interlace[3] == 2 ? 1LL : 0LL)
                       << (35- 3));
                tmp |= (word36) ((0b01L)  /* DPS8M */                         
                       << (35- 5));
                tmp |= (word36) ((cpu.switches.FLT_BASE & 0177LL)             
                       << (35-12));
                tmp |= (word36) ((0b1L) /* ID_PROM installed */               
                       << (35-13));
                tmp |= (word36) ((0b0000L)                                    
                       << (35-17));
                //tmp |= (word36) ((0b111L)                                   
                       //<< (35-20));
                // According to rsw.incl.pl1, Multics ignores this bit.
                tmp |= (word36) ((0b0L)                                       
                       << (35-18));  //BCD option off
                tmp |= (word36) ((0b1L)                                       
                       << (35-19));  //DPS option
                tmp |= (word36) ((cpu.switches.disable_cache ? 0 : 1)                                       
                       << (35-20));  //8K cache not installed
                tmp |= (word36) ((0b00L)                                      
                       << (35-22));
                tmp |= (word36) ((0b1L)  /* DPS8M */                          
                       << (35-23));
                tmp |= (word36) ((cpu.switches.proc_mode & 01LL)              
                       << (35-24));
                tmp |= (word36) ((0b0L)                                       
                       << (35-25)); // new product line (CPL/NPL)
                tmp |= (word36) ((0b000L)                                     
                       << (35-28));
                tmp |= (word36) ((cpu.switches.proc_speed & 017LL)            
                       << (35-32));
                tmp |= (word36) ((cpu.switches.cpu_num & 07LL)                
                       << (35-35));
                // 36: bits 00-07
                PROM[36] = getbits36_8 (tmp, 0);
                // 37: bits 08-15
                PROM[37] = getbits36_8 (tmp, 8);
                // 38: bits 16-23
                PROM[38] = getbits36_8 (tmp, 16);
                // 39: bits 24-31
                PROM[39] = getbits36_8 (tmp, 24);
                // 40: bits 32-35
                // 40: bits 0-3: bits 32-35 of RSW 2 field 
                //     (this is dps8m, so only 32 is always 0)
                //            4: hex option
                //            5: RSCR clock is slave
                //          6-7: reserved
                PROM[40] = ((unsigned char) ((tmp & 017) << 4))
                   // | 0100  // hex option
                   // | 0040  // clock is slave
                  ;
                            
                cpu.rA = PROM[cpu.TPR.CA & 1023];
                break;
              }
#endif // DPS8M
            uint select = cpu.TPR.CA & 0x7;
            switch (select)
              {
                case 0: // data switches
                  cpu.rA = cpu.switches.data_switches;
                  break;

                case 1: // configuration switches for ports A, B, C, D
// y = 1:
//
//   0               0 0               1 1               2 2               3
//   0               8 9               7 8               6 7               5
//  -------------------------------------------------------------------------
//  |      PORT A     |     PORT B      |     PORT C      |     PORT D      |
//  -------------------------------------------------------------------------
//  | ADR |j|k|l| MEM | ADR |j|k|l| MEM | ADR |j|k|l| MEM | ADR |j|k|l| MEM |
//  -------------------------------------------------------------------------
//
//
//   ADR: Address assignment switch setting for port
//         This defines the base address for the SCU
//   j: port enabled flag
//   k: system initialize enabled flag
//   l: interface enabled flag
//   MEM coded memory size
//     000 32K     2^15
//     001 64K     2^16
//     010 128K    2^17
//     011 256K    2^18
//     100 512K    2^19
//     101 1024K   2^20
//     110 2048K   2^21
//     111 4096K   2^22

                  cpu.rA  = 0;
                  cpu.rA |= (word36) (cpu.switches.assignment  [0] & 07LL)
                            << (35 -  (2 +  0));
                  cpu.rA |= (word36) (cpu.switches.enable      [0] & 01LL)
                            << (35 -  (3 +  0));
                  cpu.rA |= (word36) (cpu.switches.init_enable [0] & 01LL)
                            << (35 -  (4 +  0));
                  cpu.rA |= (word36) (cpu.switches.interlace   [0] ? 1LL:0LL)
                            << (35 -  (5 +  0));
                  cpu.rA |= (word36) (cpu.switches.store_size  [0] & 07LL)
                            << (35 -  (8 +  0));

                  cpu.rA |= (word36) (cpu.switches.assignment  [1] & 07LL)
                            << (35 -  (2 +  9));
                  cpu.rA |= (word36) (cpu.switches.enable      [1] & 01LL)
                            << (35 -  (3 +  9));
                  cpu.rA |= (word36) (cpu.switches.init_enable [1] & 01LL)
                            << (35 -  (4 +  9));
                  cpu.rA |= (word36) (cpu.switches.interlace   [1] ? 1LL:0LL)
                            << (35 -  (5 +  9));
                  cpu.rA |= (word36) (cpu.switches.store_size  [1] & 07LL)
                            << (35 -  (8 +  9));

                  cpu.rA |= (word36) (cpu.switches.assignment  [2] & 07LL)
                            << (35 -  (2 + 18));
                  cpu.rA |= (word36) (cpu.switches.enable      [2] & 01LL)
                            << (35 -  (3 + 18));
                  cpu.rA |= (word36) (cpu.switches.init_enable [2] & 01LL)
                            << (35 -  (4 + 18));
                  cpu.rA |= (word36) (cpu.switches.interlace   [2] ? 1LL:0LL)
                            << (35 -  (5 + 18));
                  cpu.rA |= (word36) (cpu.switches.store_size  [2] & 07LL)
                            << (35 -  (8 + 18));

                  cpu.rA |= (word36) (cpu.switches.assignment  [3] & 07LL)
                            << (35 -  (2 + 27));
                  cpu.rA |= (word36) (cpu.switches.enable      [3] & 01LL)
                            << (35 -  (3 + 27));
                  cpu.rA |= (word36) (cpu.switches.init_enable [3] & 01LL)
                            << (35 -  (4 + 27));
                  cpu.rA |= (word36) (cpu.switches.interlace   [3] ? 1LL:0LL)
                            << (35 -  (5 + 27));
                  cpu.rA |= (word36) (cpu.switches.store_size  [3] & 07LL)
                            << (35 -  (8 + 27));
                  break;

                case 2: // fault base and processor number  switches
// y = 2:
//
//   0     0 0 0 0            1 1 1     1 1 1 2 2 2 2 2 2 2   2 2     3 3   3
//   0     3 4 5 6            2 3 4     7 8 9 0 1 2 3 4 5 6   8 9     2 3   5
//  --------------------------------------------------------------------------
//  |A|B|C|D|   |              | |       | | | |   | | | |     |       |     |
//  --------- b |   FLT BASE   |c|0 0 0 0|d|e|f|0 0|g|h|i|0 0 0| SPEED | CPU |
//  |a|a|a|a|   |              | |       | | | |   | | | |     |       |     |
//  --------------------------------------------------------------------------
//

//   a: port A-D is 0: 4 word or 1: 2 word
//   b: processor type 0:L68 or DPS, 1: DPS8M, 2,3: reserved for future use
//   c: id prom 0: not installed, 1: installed
//   d: 1: bcd option installed (marketing designation)
//   e: 1: dps option installed (marketing designation)
//   f: 1: 8k cache installed
//   g: processor type designation: 0: dps8/xx, 1: dps8m/xx
//   h: gcos/vms switch position: 0:GCOS mode 1: virtual mode
//   i: current or new product line peripheral type: 0:CPL, 1:NPL
//   SPEED: 0000 = 8/70, 0100 = 8/52
//   CPU: Processor number
// DPS 8M processors:
// C(Port interlace, Ports A-D) -> C(A) 0,3
// 01 -> C(A) 4,5
// C(Fault base switches) -> C(A) 6,12
// 1 -> C(A) 13
// 0000 -> C(A) 14,17
// 111 -> C(A) 18,20
// 00 -> C(A) 21,22
// 1 -> C(A) 23
// C(Processor mode sw) -> C(A) 24
// 1 -> C(A) 25
// 000 -> C(A) 26,28
// C(Processor speed) -> C (A) 29,32



// C(Processor number switches) -> C(A) 33,35

// According to bound_gcos_.1.s.archive/gcos_fault_processor_.pl1 (L68/DPS):
//
// /* Set the A register to reflect switch info. */
//                          mc.regs.a =
// 
// /* (A-reg bits) */
// /* (0-3) Port address expansion option:           */ (4)"0"b
// /* (4-5) Reserved for future use:                 */ || (2)"0"b
// /* (6-12) Processor fault base address switches:  */ || (7)"0"b
// /* (13-16) L66 peripheral connectability:         */ || (4)"0"b
// /* (17) Future use (must be zero):                */ || (1)"1"b
// /* (18) BCD option installed:                     */ || (1)"1"b
// /* (19) DPS type processor:                       */ || (1)"0"b
// /* (20) 8K cache option installed:                */ || (1)"0"b
// /* (21) Gear shift model processor:               */ || (1)"0"b
// /* (22) Power pach option installed:              */ || (1)"0"b
// /* (23) VMS-CU option installed - 66B' proc:      */ || (1)"0"b
// /* (24) VMS-VU option installed - 66B proc:       */ || (1)"0"b
// /* (25) Type processor (0) CPL, (1) DPSE-NPL:     */ || (1)"0"b
// /* (26) 6025, 6605 or 6610 type processor:        */ || (1)"0"b
// /* (27) 2K cache option installed:                */ || (1)"0"b
// /* (28) Extended memory option installed:         */ || (1)"0"b
// /* (29-30) cabinet (00) 8/70, (01) 8/52, (10) 862, (11) 846:          */ || (2)"0"b
// /* (31) EIS option installed:                     */ || (1)"1"b
// /* (32) (1) slow memory access, (0) fast memory:  */ || (1)"0"b
// /* (33) (1) no instruction overlap, (0) overlap:  */ || (1)"0"b
// /* (34-35) Processor number:                      */ ||unspec (mc.cpu_type);

                  cpu.rA = 0;
#ifdef DPS8M
                  cpu.rA |= (word36) ((cpu.switches.interlace[0] == 2 ?
                            1LL : 0LL) << (35- 0));
                  cpu.rA |= (word36) ((cpu.switches.interlace[1] == 2 ?
                            1LL : 0LL) << (35- 1));
                  cpu.rA |= (word36) ((cpu.switches.interlace[2] == 2 ?
                            1LL : 0LL) << (35- 2));
                  cpu.rA |= (word36) ((cpu.switches.interlace[3] == 2 ?
                            1LL : 0LL) << (35- 3));
#endif

#ifdef DPS8M
                  cpu.rA |= (word36) ((0b01L)  /* DPS8M */
                             << (35- 5));
#endif
#ifdef L68
                  cpu.rA |= (word36) ((0b00L)  /* L68/DPS */
                             << (35- 5));
#endif
                  cpu.rA |= (word36) ((cpu.switches.FLT_BASE & 0177LL)
                             << (35-12));
#ifdef DPS8M
                  cpu.rA |= (word36) ((0b1L) /* ID_PROM installed */
                             << (35-13));
#endif
                  cpu.rA |= (word36) ((0b0000L) 
                            << (35-17));
                  //cpu.rA |= (word36) ((0b111L)
                              //<< (35-20));
                  // According to rsw.incl.pl1, Multics ignores this bit.
                  cpu.rA |= (word36) ((0b0L)
                            << (35-18));  //BCD option off
#ifdef DPS8M
                  cpu.rA |= (word36) ((0b1L)
                            << (35-19));  // L68/DPS option: DPS
#endif
#ifdef L68
                  cpu.rA |= (word36) ((0b0L)
                            << (35-19));  // L68/DPS option: L68
#endif
#ifdef DPS8M
                  cpu.rA |= (word36) ((cpu.switches.disable_cache ? 0 : 1)
                            << (35-20));  //8K cache not installed
                  cpu.rA |= (word36) ((0b00L)
                            << (35-22));
                  cpu.rA |= (word36) ((0b1L)  /* DPS8M */
                            << (35-23));
                  cpu.rA |= (word36) ((cpu.switches.proc_mode & 01LL)
                            << (35-24));
                  cpu.rA |= (word36) ((0b0L)
                            << (35-25)); // new product line (CPL/NPL)
                  cpu.rA |= (word36) ((0b000L)
                            << (35-28));
                  cpu.rA |= (word36) ((cpu.switches.proc_speed & 017LL)
                            << (35-32));
#endif
#ifdef L68
                  cpu.rA |= (word36) ((0b0L)
                            << (35-27)); // 2K cache disabled
                  cpu.rA |= (word36) ((0b0L)
                            << (35-28)); // GCOS mode extended memory disabled
                  cpu.rA |= (word36) ((0b1110L)
                            << (35-32)); // CPU ID
#endif
                  cpu.rA |= (word36) ((cpu.switches.cpu_num & 07LL)
                            << (35-35));
                  break;

                case 3: // configuration switches for ports E, F, G, H
#ifdef DPS8M
                  cpu.rA = 0;
                  break;
#endif
#ifdef L68
// y = 3:
//
//   0               0 0               1 1               2 2               3
//   0               8 9               7 8               6 7               5
//  -------------------------------------------------------------------------
//  |      PORT E     |     PORT F      |     PORT G      |     PORT H      |
//  -------------------------------------------------------------------------
//  | ADR |j|k|l| MEM | ADR |j|k|l| MEM | ADR |j|k|l| MEM | ADR |j|k|l| MEM |
//  -------------------------------------------------------------------------
//
//
//   ADR: Address assignment switch setting for port
//         This defines the base address for the SCU
//   j: port enabled flag
//   k: system initialize enabled flag
//   l: interface enabled flag
//   MEM coded memory size
//     000 32K     2^15
//     001 64K     2^16
//     010 128K    2^17
//     011 256K    2^18
//     100 512K    2^19
//     101 1024K   2^20
//     110 2048K   2^21
//     111 4096K   2^22

                  cpu.rA  = 0;
                  cpu.rA |= (word36) (cpu.switches.assignment  [4] & 07LL)
                            << (35 -  (2 +  0));
                  cpu.rA |= (word36) (cpu.switches.enable      [4] & 01LL)
                            << (35 -  (3 +  0));
                  cpu.rA |= (word36) (cpu.switches.init_enable [4] & 01LL)
                            << (35 -  (4 +  0));
                  cpu.rA |= (word36) (cpu.switches.interlace   [4] ? 1LL:0LL)
                            << (35 -  (5 +  0));
                  cpu.rA |= (word36) (cpu.switches.store_size  [4] & 07LL)
                            << (35 -  (8 +  0));

                  cpu.rA |= (word36) (cpu.switches.assignment  [5] & 07LL)
                            << (35 -  (2 +  9));
                  cpu.rA |= (word36) (cpu.switches.enable      [5] & 01LL)
                            << (35 -  (3 +  9));
                  cpu.rA |= (word36) (cpu.switches.init_enable [5] & 01LL)
                            << (35 -  (4 +  9));
                  cpu.rA |= (word36) (cpu.switches.interlace   [5] ? 1LL:0LL)
                            << (35 -  (5 +  9));
                  cpu.rA |= (word36) (cpu.switches.store_size  [5] & 07LL)
                            << (35 -  (8 +  9));

                  cpu.rA |= (word36) (cpu.switches.assignment  [6] & 07LL)
                            << (35 -  (2 + 18));
                  cpu.rA |= (word36) (cpu.switches.enable      [6] & 01LL)
                            << (35 -  (3 + 18));
                  cpu.rA |= (word36) (cpu.switches.init_enable [6] & 01LL)
                            << (35 -  (4 + 18));
                  cpu.rA |= (word36) (cpu.switches.interlace   [6] ? 1LL:0LL)
                            << (35 -  (5 + 18));
                  cpu.rA |= (word36) (cpu.switches.store_size  [6] & 07LL)
                            << (35 -  (8 + 18));

                  cpu.rA |= (word36) (cpu.switches.assignment  [7] & 07LL)
                            << (35 -  (2 + 27));
                  cpu.rA |= (word36) (cpu.switches.enable      [7] & 01LL)
                            << (35 -  (3 + 27));
                  cpu.rA |= (word36) (cpu.switches.init_enable [7] & 01LL)
                            << (35 -  (4 + 27));
                  cpu.rA |= (word36) (cpu.switches.interlace   [7] ? 1LL:0LL)
                            << (35 -  (5 + 27));
                  cpu.rA |= (word36) (cpu.switches.store_size  [7] & 07LL)
                            << (35 -  (8 + 27));
                  break;

#endif

                case 4:
                  // I suspect the this is a L68 only, but AL39 says both
                  // port interlace and half/full size
                  // The DPS doesn't seem to have the half/full size switches
                  // so we'll always report full, and the interlace bits were
                  // squeezed into RSW 2

//  0                       1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2           3
//  0                       2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9           5
// -------------------------------------------------------------------------
// |                         | A | B | C | D | E | F | G | H |             |
// |0 0 0 0 0 0 0 0 0 0 0 0 0---------------------------------0 0 0 0 0 0 0|
// |                         |f|g|f|g|f|g|f|g|f|g|f|g|f|g|f|g|             |
// -------------------------------------------------------------------------
//                         13 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1             7

                  cpu.rA  = 0;
                  cpu.rA |= (word36) (cpu.switches.interlace [0] == 2 ?
                            1LL : 0LL) << (35-13);
                  cpu.rA |= (word36) (cpu.switches.interlace [1] == 2 ?
                            1LL : 0LL) << (35-15);
                  cpu.rA |= (word36) (cpu.switches.interlace [2] == 2 ?
                            1LL : 0LL) << (35-17);
                  cpu.rA |= (word36) (cpu.switches.interlace [3] == 2 ?
                            1LL : 0LL) << (35-19);
#ifdef L68
                  cpu.rA |= (word36) (cpu.switches.interlace [4] == 2 ?
                            1LL : 0LL) << (35-21);
                  cpu.rA |= (word36) (cpu.switches.interlace [5] == 2 ?
                            1LL : 0LL) << (35-23);
                  cpu.rA |= (word36) (cpu.switches.interlace [6] == 2 ?
                            1LL : 0LL) << (35-25);
                  cpu.rA |= (word36) (cpu.switches.interlace [7] == 2 ?
                            1LL : 0LL) << (35-27);
#endif
                  break;

                default:
                  // XXX Guessing values; also don't know if this is actually
                  //  a fault
                  doFault (FAULT_IPR,
                           (_fault_subtype) {.fault_ipr_subtype=FR_ILL_MOD},
                           "Illegal register select value");
              }
            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        /// Privileged - System Control

        case 0015:  // cioc
          {
            // cioc The system controller addressed by Y (i.e., contains
            // the word at Y) sends a connect signal to the port specified
            // by C(Y) 33,35.
            int cpu_port_num = query_scbank_map (cpu.iefpFinalAddress);
            // If the there is no port to that memory location, fault
            if (cpu_port_num < 0)
              {
                doFault (FAULT_ONC,
                         (_fault_subtype) {.fault_onc_subtype=flt_onc_nem},
                         "(cioc)");
              }
            int scu_unit_num = query_scu_unit_num ((int) currentRunningCPUnum,
                                                   cpu_port_num);
            if (scu_unit_num < 0)
              {
                doFault (FAULT_ONC,
                         (_fault_subtype) {.fault_onc_subtype=flt_onc_nem},
                         "(cioc)");
              }

// expander word
// dcl  1 scs$reconfig_general_cow aligned external, /* Used during reconfig
//                                                      ops. */
//   2 pad bit (36) aligned,
//   2 cow,                        /* Connect operand word, in odd location. */
//   3 sub_mask bit (8) unaligned, /* Expander sub-port mask */
//   3 mbz1 bit (13) unaligned,
//   3 expander_command bit (3) unaligned,   /* Expander command. */
//   3 mbz2 bit (9) unaligned,
//   3 controller_port fixed bin (3) unaligned unsigned;/* controller port for
//                                                          this CPU */

            word8 sub_mask = getbits36_8 (cpu.CY, 0);
            word3 expander_command = getbits36_3 (cpu.CY, 21);
            uint scu_port_num = (uint) getbits36_3 (cpu.CY, 33);
            scu_cioc (currentRunningCPUnum, (uint) scu_unit_num, scu_port_num, 
                      expander_command, sub_mask);
          }
          break;

        case 0553:  // smcm
          {
            // C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor)
            // specify which processor port (i.e., which system
            // controller) is used.
#ifdef DPS8M
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
#endif
#ifdef L68
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 07;
#endif
            int scu_unit_num =
               query_scu_unit_num ((int) currentRunningCPUnum,
                                   (int) cpu_port_num);
#if 0 // not on 4MW
            if (scu_unit_num < 0)
              {
                if (cpu_port_num == 0)
                  putbits36_4 (& cpu.faultRegister[0], 16, 010);
                else if (cpu_port_num == 1)
                  putbits36_4 (& cpu.faultRegister[0], 20, 010);
                else if (cpu_port_num == 2)
                  putbits36_4 (& cpu.faultRegister[0], 24, 010);
                else
                  putbits36 (& cpu.faultRegister[0], 28, 4, 010);
                doFault (FAULT_CMD,
                         (_fault_subtype)
                           {.fault_cmd_subtype=flt_cmd_not_control},
                         "(smcm)");
              }
#endif
            if (scu_unit_num < 0)
              {
                sim_warn ("smcm to non-existent controller on "
                          "cpu %d port %d\n", 
                          currentRunningCPUnum, cpu_port_num);
                break;
              }
            t_stat rc = scu_smcm ((uint) scu_unit_num,
                                  currentRunningCPUnum, cpu.rA, cpu.rQ);
            if (rc)
              return rc;
          }
          break;

        case 0451:  // smic
          {
            // For the smic instruction, the first 2 or 3 bits of the addr
            // field of the instruction are used to specify which SCU.
            // 2 bits for the DPS8M.
            //int scu_unit_num = getbits36_2 (TPR.CA, 0);

            // C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor)
            // specify which processor port (i.e., which system
            // controller) is used.
#ifdef DPS8M
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
#endif
#ifdef L68
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 07;
#endif
            int scu_unit_num = query_scu_unit_num ((int) currentRunningCPUnum,
                                                   (int) cpu_port_num);

            if (scu_unit_num < 0)
              {
#ifdef DPS8M
                return SCPE_OK;
#endif 
#ifdef L68
                // CPTUR (cptUseFR) -- will be set by doFault
                if (cpu_port_num == 0)
                  putbits36_4 (& cpu.faultRegister[0], 16, 010);
                else if (cpu_port_num == 1)
                  putbits36_4 (& cpu.faultRegister[0], 20, 010);
                else if (cpu_port_num == 2)
                  putbits36_4 (& cpu.faultRegister[0], 24, 010);
                else if (cpu_port_num == 3)
                  putbits36 (& cpu.faultRegister[0], 28, 4, 010);
// XXX What if the port is > 3?
                doFault (FAULT_CMD,
                         (_fault_subtype)
                           {.fault_cmd_subtype=flt_cmd_not_control},
                         "(smic)");
#endif
              }
            t_stat rc = scu_smic ((uint) scu_unit_num, currentRunningCPUnum, 
                                  cpu_port_num, cpu.rA);
            if (rc)
              return rc;
          }
          break;


        case 0057:  // sscr
          {
            //uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
            // Looking at privileged_mode_ut.alm, shift 10 bits...
#ifdef DPS8M
            uint cpu_port_num = (cpu.TPR.CA >> 10) & 03;
#endif
#ifdef L68
            uint cpu_port_num = (cpu.TPR.CA >> 10) & 07;
#endif
            int scu_unit_num = query_scu_unit_num ((int) currentRunningCPUnum,
                                                   (int) cpu_port_num);
            if (scu_unit_num < 0)
              {
                // CPTUR (cptUseFR) -- will be set by doFault
                if (cpu_port_num == 0)
                  putbits36_4 (& cpu.faultRegister[0], 16, 010);
                else if (cpu_port_num == 1)
                  putbits36_4 (& cpu.faultRegister[0], 20, 010);
                else if (cpu_port_num == 2)
                  putbits36_4 (& cpu.faultRegister[0], 24, 010);
                else
                  putbits36 (& cpu.faultRegister[0], 28, 4, 010);
                doFault (FAULT_CMD,
                         (_fault_subtype)
                           {.fault_cmd_subtype=flt_cmd_not_control},
                         "(sscr)");
              }
            t_stat rc = scu_sscr ((uint) scu_unit_num, currentRunningCPUnum,
                                  cpu_port_num, cpu.iefpFinalAddress & MASK15,
                                  cpu.rA, cpu.rQ);

            if (rc)
              return rc;
          }
          break;

        // Privileged - Miscellaneous

        case 0212:  // absa
          {
            word36 result;
            int rc = doABSA (& result);
            if (rc)
              return rc;
            cpu.rA = result;
            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
          }
          break;

        case 0616:  // dis

          if (! cpu.switches.dis_enable)
            {
              return STOP_DIS;
            }

          // XXX This is subtle; g7Pending below won't see the queued
          // g7Fault. I don't understand how the real hardware dealt 
          // with this, but this seems to work. (I would hazard a guess
          // that DIS was doing a continuous FETCH/EXECUTE cycle 
          // ('if !interrupt goto .'))
          advanceG7Faults ();

          if ((! cpu.switches.tro_enable) &&
              (! sample_interrupts ()) &&
              (sim_qcount () == 0))  // XXX If clk_svc is implemented it will
                                     // break this logic
            {
              sim_printf ("DIS@0%06o with no interrupts pending and"
                          " no events in queue\n", cpu.PPR.IC);
              sim_printf ("\nsimCycles = %"PRId64"\n", sim_timell ());
              sim_printf ("\ncpuCycles = %"PRId64"\n", sys_stats.total_cycles);
              longjmp (cpu.jmpMain, JMP_STOP);
            }

// Multics/BCE halt
          if (cpu.PPR.PSR == 0430 && cpu.PPR.IC == 012)
              {
                sim_printf ("BCE DIS causes CPU halt\n");
                sim_debug (DBG_MSG, & cpu_dev, "BCE DIS causes CPU halt\n");
                longjmp (cpu.jmpMain, JMP_STOP);
              }

          sim_debug (DBG_TRACEEXT, & cpu_dev, "entered DIS_cycle\n");
          //sim_printf ("entered DIS_cycle\n");

          // No operation takes place, and the processor does not
          // continue with the next instruction; it waits for a
          // external interrupt signal.
          // AND, according to pxss.alm, TRO

// Bless NovaScale...
//  DIS
//
//    NOTES:
//
//      1. The inhibit bit in this instruction only affects the recognition
//         of a Timer Runout (TROF) fault.
//
//         Inhibit ON delays the recognition of a TROF until the processor
//         enters Slave mode.
//
//         Inhibit OFF allows the TROF to interrupt the DIS state.
//
//      2. For all other faults and interrupts, the inhibit bit is ignored.
//
//      3. The use of this instruction in the Slave or Master mode causes a
//         Command fault.

          if (sample_interrupts ())
            {
              sim_debug (DBG_TRACEEXT, & cpu_dev, "DIS sees an interrupt\n");
              cpu.interrupt_flag = true;
              break;
            }
// Implementing TRO according to AL39 for the DIS cause caues idle systems to
// hang in the DIS instruction. Revert back to the old behavior.
#if 1
          if (GET_I (cpu.cu.IWB) ? bG7PendingNoTRO () : bG7Pending ())
#else
          //if (GET_I (cpu.cu.IWB) ? bG7PendingNoTRO () : bG7Pending ())
          // Don't check timer runout if in absolute mode, privledged, or
          // interrupts inhibited.
          bool noCheckTR = (get_addr_mode () == ABSOLUTE_mode) || 
                            is_priv_mode ()  ||
                            GET_I (cpu.cu.IWB);
          if (noCheckTR ? bG7PendingNoTRO () : bG7Pending ())
#endif
            {
              sim_debug (DBG_TRACEEXT, & cpu_dev, "DIS sees a TRO\n");
              cpu.g7_flag = true;
              break;
            }
          else
            {
              sim_debug (DBG_TRACEEXT, & cpu_dev, "DIS refetches\n");
              sys_stats.total_cycles ++;
              //longjmp (cpu.jmpMain, JMP_REFETCH);
#ifdef ROUND_ROBIN
#ifdef ISOLTS
              if (currentRunningCPUnum)
              {
//sim_printf ("stopping CPU %c\n", currentRunningCPUnum + 'A');
                cpu.isRunning = false;
              }
#endif
#endif
              return CONT_DIS;
            }

        default:
          if (cpu.switches.halt_on_unimp)
            return STOP_ILLOP;
          doFault (FAULT_IPR,
                   (_fault_subtype) {.fault_ipr_subtype=FR_ILL_OP},
                   "Illegal instruction");
      }
#ifdef L68
    cpu.ou.STR_OP = (is_ou && (i->info->flags & (STORE_OPERAND | STORE_YPAIR))) ? 1 : 0; 
    cpu.ou.cycle |= ou_GOF;
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr && is_ou)
      addOUhist ();
#endif
    return SCPE_OK;
}

// CANFAULT
static t_stat DoEISInstruction (void)
{
    DCDstruct * i = & cpu.currentInstruction;
    uint32 opcode = i->opcode;

#ifdef PANEL
    if (insGrp [opcode])
      {
        word8 grp = eisGrp [opcode] - 1;
        uint row = grp / 36;
        uint col = grp % 36;
        CPT (cpt3U + row, col); // cpt3U 0-35, cpt3L 0-17
      }
#endif
#ifdef L68
    bool is_du = false;
    if (EISopcodes[i->opcode].reg_use & is_DU)
      {
        is_du = true;
        PNL (DU_CYCLE_nDUD;) // set not idle
      }
#endif

    switch (opcode)
    {
        /// TRANSFER INSTRUCTIONS

        case 0604:  // tmoz
            // If negative or zero indicator ON then
            // C(TPR.CA) -> C(PPR.IC)
            // C(TPR.TSR) -> C(PPR.PSR)
            if (cpu.cu.IR & (I_NEG | I_ZERO))
              {
                ReadTraOp ();
                return CONT_TRA;
              }
            break;

        case 0605:  // tpnz
            // If negative and zero indicators are OFF then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (! (cpu.cu.IR & I_NEG) && ! (cpu.cu.IR & I_ZERO))
            {
                ReadTraOp ();
                return CONT_TRA;
            }
            break;

        case 0601:  // trtf
            // If truncation indicator OFF then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (!TST_I_TRUNC)
            {
                ReadTraOp ();
                return CONT_TRA;
            }
            break;

        case 0600:  // trtn
            // If truncation indicator ON then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (TST_I_TRUNC)
            {
                CLR_I_TRUNC;
                ReadTraOp ();
                return CONT_TRA;
            }
            break;

        case 0606:  // ttn
            // If tally runout indicator ON then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            // otherwise, no change to C(PPR)
            if (TST_I_TALLY)
            {
                ReadTraOp ();
                return CONT_TRA;
            }
            break;

        /// POINTER REGISTER INSTRUCTIONS

        /// Pointer Register Data Movement Load

        case 0310:  // easp1
            // C(TPR.CA) -> C(PRn.SNR)
            CPTUR (cptUsePRn + 1);
            cpu.PR[1].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0312:  // easp3
            // C(TPR.CA) -> C(PRn.SNR)
            CPTUR (cptUsePRn + 3);
            cpu.PR[3].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0330:  // easp5
            // C(TPR.CA) -> C(PRn.SNR)
            CPTUR (cptUsePRn + 5);
            cpu.PR[5].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0332:  // easp7
            // C(TPR.CA) -> C(PRn.SNR)
            CPTUR (cptUsePRn + 7);
            cpu.PR[7].SNR = cpu.TPR.CA & MASK15;
            break;

        case 0311:  // eawp1
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 1);
            cpu.PR[1].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (1, cpu.TPR.TBR);
            break;
        case 0313:  // eawp3
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 3);
            cpu.PR[3].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (3, cpu.TPR.TBR);
            break;
        case 0331:  // eawp5
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 5);
            cpu.PR[5].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (5, cpu.TPR.TBR);
            break;
        case 0333:  // eawp7
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 7);
            cpu.PR[7].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (7, cpu.TPR.TBR);
            break;
        case 0350:  // epbp0
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[0].RNR = cpu.TPR.TRR;
            cpu.PR[0].SNR = cpu.TPR.TSR;
            cpu.PR[0].WORDNO = 0;
            SET_PR_BITNO (0, 0);
            break;
        case 0352:  // epbp2
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 2);
            cpu.PR[2].RNR = cpu.TPR.TRR;
            cpu.PR[2].SNR = cpu.TPR.TSR;
            cpu.PR[2].WORDNO = 0;
            SET_PR_BITNO (2, 0);
            break;
        case 0370:  // epbp4
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 4);
            cpu.PR[4].RNR = cpu.TPR.TRR;
            cpu.PR[4].SNR = cpu.TPR.TSR;
            cpu.PR[4].WORDNO = 0;
            SET_PR_BITNO (4, 0);
            break;
        case 0372:  // epbp6
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 6);
            cpu.PR[6].RNR = cpu.TPR.TRR;
            cpu.PR[6].SNR = cpu.TPR.TSR;
            cpu.PR[6].WORDNO = 0;
            SET_PR_BITNO (6, 0);
            break;

        case 0351:  // epp1
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 1);
            cpu.PR[1].RNR = cpu.TPR.TRR;
            cpu.PR[1].SNR = cpu.TPR.TSR;
            cpu.PR[1].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (1, cpu.TPR.TBR);
            break;
        case 0353:  // epp3
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 3);
            cpu.PR[3].RNR = cpu.TPR.TRR;
            cpu.PR[3].SNR = cpu.TPR.TSR;
            cpu.PR[3].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (3, cpu.TPR.TBR);
            break;
        case 0371:  // epp5
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 5);
            cpu.PR[5].RNR = cpu.TPR.TRR;
            cpu.PR[5].SNR = cpu.TPR.TSR;
            cpu.PR[5].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (5, cpu.TPR.TBR);
            break;
        case 0373:  // epp7
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            CPTUR (cptUsePRn + 7);
            cpu.PR[7].RNR = cpu.TPR.TRR;
            cpu.PR[7].SNR = cpu.TPR.TSR;
            cpu.PR[7].WORDNO = cpu.TPR.CA;
            SET_PR_BITNO (7, cpu.TPR.TBR);
            break;

        /// Pointer Register Data Movement Store

        case 0250:  // spbp0
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  000 -> C(Y-pair)0,2
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  00...0 -> C(Y-pair)36,71
            CPTUR (cptUsePRn + 0);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[0].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[0].RNR) << 15;
            cpu.Ypair[1] = 0;
            break;

        case 0252:  // spbp2
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  000 -> C(Y-pair)0,2
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  00...0 -> C(Y-pair)36,71
            CPTUR (cptUsePRn + 2);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[2].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[2].RNR) << 15;
            cpu.Ypair[1] = 0;
            break;

        case 0650:  // spbp4
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  000 -> C(Y-pair)0,2
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  00...0 -> C(Y-pair)36,71
            CPTUR (cptUsePRn + 4);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[4].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[4].RNR) << 15;
            cpu.Ypair[1] = 0;
            break;

        case 0652:  // spbp6
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  000 -> C(Y-pair)0,2
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  00...0 -> C(Y-pair)36,71
            CPTUR (cptUsePRn + 6);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[6].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[6].RNR) << 15;
            cpu.Ypair[1] = 0;
            break;

        case 0251:  // spri1
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  000 -> C(Y-pair)0,2
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  C(PRn.WORDNO) -> C(Y-pair)36,53
            //  000 -> C(Y-pair)54,56
            //  C(PRn.BITNO) -> C(Y-pair)57,62
            //  00...0 -> C(Y-pair)63,71
            CPTUR (cptUsePRn + 1);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[1].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[1].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[1].WORDNO << 18;
            cpu.Ypair[1]|= (word36) GET_PR_BITNO (1) << 9;
            break;

        case 0253:  // spri3
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  000 -> C(Y-pair)0,2
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  C(PRn.WORDNO) -> C(Y-pair)36,53
            //  000 -> C(Y-pair)54,56
            //  C(PRn.BITNO) -> C(Y-pair)57,62
            //  00...0 -> C(Y-pair)63,71
            CPTUR (cptUsePRn + 3);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[3].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[3].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[3].WORDNO << 18;
            cpu.Ypair[1]|= (word36) GET_PR_BITNO (3) << 9;
            break;

        case 0651:  // spri5
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  000 -> C(Y-pair)0,2
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  C(PRn.WORDNO) -> C(Y-pair)36,53
            //  000 -> C(Y-pair)54,56
            //  C(PRn.BITNO) -> C(Y-pair)57,62
            //  00...0 -> C(Y-pair)63,71
            CPTUR (cptUsePRn + 5);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[5].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[5].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[5].WORDNO << 18;
            cpu.Ypair[1]|= (word36) GET_PR_BITNO (5) << 9;
            break;

        case 0653:  // spri7
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  000 -> C(Y-pair)0,2
            //  C(PRn.SNR) -> C(Y-pair)3,17
            //  C(PRn.RNR) -> C(Y-pair)18,20
            //  00...0 -> C(Y-pair)21,29
            //  (43)8 -> C(Y-pair)30,35
            //  C(PRn.WORDNO) -> C(Y-pair)36,53
            //  000 -> C(Y-pair)54,56
            //  C(PRn.BITNO) -> C(Y-pair)57,62
            //  00...0 -> C(Y-pair)63,71
            CPTUR (cptUsePRn + 7);
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[7].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[7].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[7].WORDNO << 18;
            cpu.Ypair[1]|= (word36) GET_PR_BITNO (7) << 9;
            break;

        /// Ring Alarm Register

        case 0754:  // sra
            // 00...0 -> C(Y)0,32
            // C(RALR) -> C(Y)33,35

            CPTUR (cptUseRALR);
            cpu.CY = (word36)cpu.rRALR;

            break;

        /// PRIVILEGED INSTRUCTIONS

        /// Privileged - Register Load

        case 0257:  // lptp
#ifdef DPS8M
          break;
#endif
#ifdef L68
          {
            // For i = 0, 1, ..., 15
            //   m = C(PTWAM(i).USE)
            //   C(Y-block16+m)0,14 -> C(PTWAM(m).POINTER)
            //   C(Y-block16+m)15,26 -> C(PTWAM(m).PAGE)
            //   C(Y-block16+m)27 -> C(PTWAM(m).F)

#ifdef WAM
            for (uint i = 0; i < 16; i ++)
              {
                word4 m = cpu.PTWAM[i].USE;
                cpu.PTWAM[m].POINTER = getbits36_15 (cpu.Yblock16[i],  0);
                cpu.PTWAM[m].PAGENO =  getbits36_12 (cpu.Yblock16[i], 15);
                cpu.PTWAM[m].FE =      getbits36_1  (cpu.Yblock16[i], 27);
              }
#endif
          }
          break;
#endif
        case 0173:  // lptr
#ifdef DPS8M
          break;
#endif
#ifdef L68
          {
            // For i = 0, 1, ..., 15
            //   m = C(PTWAM(i).USE)
            //   C(Y-block16+m)0,17 -> C(PTWAM(m).ADDR)
            //   C(Y-block16+m)29 -> C(PTWAM(m).M)
#ifdef WAM
            for (uint i = 0; i < 16; i ++)
              {
                word4 m = cpu.PTWAM[i].USE;
                cpu.PTWAM[m].ADDR = getbits36_18 (cpu.Yblock16[i],  0);
                cpu.PTWAM[m].M =    getbits36_1  (cpu.Yblock16[i], 29);
              }
#endif
          }
          break;
#endif

        case 0774:  // lra
            CPTUR (cptUseRALR);
            cpu.rRALR = cpu.CY & MASK3;
            sim_debug (DBG_TRACE, & cpu_dev, "RALR set to %o\n", cpu.rRALR);
#ifdef LOOPTRC
{
void elapsedtime (void);
elapsedtime ();
 sim_printf (" RALR set to %o  PSR:IC %05o:%06o\r\n", cpu.rRALR, cpu.PPR.PSR, cpu.PPR.IC);
}
#endif
            break;

        case 0232:  // lsdr
#ifdef DPS8M
          break;
#endif
#ifdef L68
          {
            // For i = 0, 1, ..., 15
            //   m = C(SDWAM(i).USE)
            //   C(Y-block32+2m)0,23 -> C(SDWAM(m).ADDR)
            //   C(Y-block32+2m)24,32 -> C(SDWAM(m).R1, R2, R3)
            //   C(Y-block32+2m)37,50 -> C(SDWAM(m).BOUND)
            //   C(Y-block32+2m)51,57 -> C(SDWAM(m).R, E, W, P, U, G, C) Note: typo in AL39, 52 should be 51
            //   C(Y-block32+2m)58,71 -> C(SDWAM(m).CL)
#ifdef WAM
            for (uint i = 0; i < 16; i ++)
              {
                word4 m = cpu.SDWAM[i].USE;
                uint j = (uint)m * 2;
                cpu.SDWAM[m].ADDR =    getbits36_24 (cpu.Yblock32[j],  0);
                cpu.SDWAM[m].R1 =      getbits36_3  (cpu.Yblock32[j], 24);
                cpu.SDWAM[m].R2 =      getbits36_3  (cpu.Yblock32[j], 27);
                cpu.SDWAM[m].R3 =      getbits36_3  (cpu.Yblock32[j], 30);

                cpu.SDWAM[m].BOUND =   getbits36_14 (cpu.Yblock32[j + 1], 37 - 36);
                cpu.SDWAM[m].R =       getbits36_1  (cpu.Yblock32[j + 1], 51 - 36);
                cpu.SDWAM[m].E =       getbits36_1  (cpu.Yblock32[j + 1], 52 - 36);
                cpu.SDWAM[m].W =       getbits36_1  (cpu.Yblock32[j + 1], 53 - 36);
                cpu.SDWAM[m].P =       getbits36_1  (cpu.Yblock32[j + 1], 54 - 36);
                cpu.SDWAM[m].U =       getbits36_1  (cpu.Yblock32[j + 1], 55 - 36);
                cpu.SDWAM[m].G =       getbits36_1  (cpu.Yblock32[j + 1], 56 - 36);
                cpu.SDWAM[m].C =       getbits36_1  (cpu.Yblock32[j + 1], 57 - 36);
                cpu.SDWAM[m].EB =      getbits36_14 (cpu.Yblock32[j + 1], 58 - 36);
              }
#endif
          }
          break;
#endif

        case 0557:  // sptp
          {
// XXX AL39 The associative memory is ignored (forced to "no match") during address
// preparation.
            // Level j is selected by C(TPR.CA)12,13
#ifdef DPS8M
            uint level = (cpu.TPR.CA >> 4) & 03;
#endif
#ifdef L68
            uint level = 0;
#endif
#ifdef WAM
            uint toffset = level * 16;
#endif
            for (uint j = 0; j < 16; j ++)
              {
                cpu.Yblock16[j] = 0;
#ifdef WAM
                putbits36_15 (& cpu.Yblock16[j],  0,
                           cpu.PTWAM[toffset + j].POINTER);
#ifdef DPS8M
                putbits36_12 (& cpu.Yblock16[j], 15,
                           cpu.PTWAM[toffset + j].PAGENO & 07760);

                uint parity = 0;
                if (cpu.PTWAM[toffset + j].FE)
                {
                    // calculate parity
                    // 58009997-040 p.101,111
                    parity = ((uint) cpu.PTWAM[toffset + j].POINTER << 4) | (cpu.PTWAM[toffset + j].PAGENO >> 8);
                    parity = parity ^ (parity >>16);
                    parity = parity ^ (parity >> 8);
                    parity = parity ^ (parity >> 4);
                    parity = ~ (0x6996u >> (parity & 0xf)); 
                }
                putbits36_1 (& cpu.Yblock16[j], 23, parity);
#endif
#ifdef L68
                putbits36_12 (& cpu.Yblock16[j], 15,
                           cpu.PTWAM[toffset + j].PAGENO);
#endif
                putbits36_1 (& cpu.Yblock16[j], 27, 
                           cpu.PTWAM[toffset + j].FE);
#ifdef DPS8M
                putbits36_6 (& cpu.Yblock16[j], 30,
                           cpu.PTWAM[toffset + j].USE);
#endif
#ifdef L68
                putbits36_4 (& cpu.Yblock16[j], 32,
                           cpu.PTWAM[toffset + j].USE);
#endif

#endif
              }
#ifndef WAM
            if (level == 0)
              {
                putbits36 (& cpu.Yblock16[0],  0, 15,
                           cpu.PTW0.POINTER);
#ifdef DPS8M
                putbits36 (& cpu.Yblock16[0], 15, 12,
                           cpu.PTW0.PAGENO & 07760);
#endif
#ifdef L68
                putbits36 (& cpu.Yblock16[0], 15, 12,
                           cpu.PTW0.PAGENO);
#endif
                putbits36 (& cpu.Yblock16[0], 27,  1,
                           cpu.PTW0.FE);
#ifdef DPS8M
                putbits36 (& cpu.Yblock16[0], 30,  6,
                           cpu.PTW0.USE);
#endif
#ifdef L68
                putbits36 (& cpu.Yblock16[0], 32,  4,
                           cpu.PTW0.USE);
#endif
              }
#endif
          }
          break;

        case 0154:  // sptr
          {
// XXX The associative memory is ignored (forced to "no match") during address
// preparation.

            // Level j is selected by C(TPR.CA)12,13
#ifdef DPS8M
            uint level = (cpu.TPR.CA >> 4) & 03;
#endif
#ifdef L68
            uint level = 0;
#endif
#ifdef WAM
            uint toffset = level * 16;
#endif
            for (uint j = 0; j < 16; j ++)
              {
                cpu.Yblock16[j] = 0;
#ifdef WAM
#ifdef DPS8M
                putbits36_18 (& cpu.Yblock16[j], 0,
                              cpu.PTWAM[toffset + j].ADDR & 0777760);
#endif
#ifdef L68
                putbits36_18 (& cpu.Yblock16[j], 0,
                              cpu.PTWAM[toffset + j].ADDR);
#endif
                putbits36_1 (& cpu.Yblock16[j], 29,
                             cpu.PTWAM[toffset + j].M);
#endif
              }
#ifndef WAM
            if (level == 0)
              {
#ifdef DPS8M
                putbits36 (& cpu.Yblock16[0], 0, 13, cpu.PTW0.ADDR & 0777760);
#endif
#ifdef L68
                putbits36 (& cpu.Yblock16[0], 0, 13, cpu.PTW0.ADDR);
#endif
                putbits36_1 (& cpu.Yblock16[0], 29, cpu.PTW0.M);
              }
#endif
          }
          break;

        case 0254:  // ssdr
          {
// XXX AL39: The associative memory is ignored (forced to "no match") during
// address preparation.

            // Level j is selected by C(TPR.CA)11,12
            // Note: not bits 12,13. This is due to operand being Yblock32
#ifdef DPS8M
            uint level = (cpu.TPR.CA >> 5) & 03;
#endif
#ifdef L68
            uint level = 0;
#endif
#ifdef WAM
            uint toffset = level * 16;
#endif
            for (uint j = 0; j < 16; j ++)
              {
                cpu.Yblock32[j * 2] = 0;
#ifdef WAM
                putbits36_24 (& cpu.Yblock32[j * 2],  0,
                           cpu.SDWAM[toffset + j].ADDR);
                putbits36_3 (& cpu.Yblock32[j * 2], 24,
                           cpu.SDWAM[toffset + j].R1);
                putbits36_3 (& cpu.Yblock32[j * 2], 27,
                           cpu.SDWAM[toffset + j].R2);
                putbits36_3 (& cpu.Yblock32[j * 2], 30,
                           cpu.SDWAM[toffset + j].R3);
#endif
                cpu.Yblock32[j * 2 + 1] = 0;
#ifdef WAM
                putbits36_14 (& cpu.Yblock32[j * 2 + 1], 37 - 36,
                           cpu.SDWAM[toffset + j].BOUND);
                putbits36_1 (& cpu.Yblock32[j * 2 + 1], 51 - 36,
                           cpu.SDWAM[toffset + j].R);
                putbits36_1 (& cpu.Yblock32[j * 2 + 1], 52 - 36,
                           cpu.SDWAM[toffset + j].E);
                putbits36_1 (& cpu.Yblock32[j * 2 + 1], 53 - 36,
                           cpu.SDWAM[toffset + j].W);
                putbits36_1 (& cpu.Yblock32[j * 2 + 1], 54 - 36,
                           cpu.SDWAM[toffset + j].P);
                putbits36_1 (& cpu.Yblock32[j * 2 + 1], 55 - 36,
                           cpu.SDWAM[toffset + j].U);
                putbits36_1 (& cpu.Yblock32[j * 2 + 1], 56 - 36,
                           cpu.SDWAM[toffset + j].G);
                putbits36_1 (& cpu.Yblock32[j * 2 + 1], 57 - 36,
                           cpu.SDWAM[toffset + j].C);
                putbits36_14 (& cpu.Yblock32[j * 2 + 1], 58 - 36,
                           cpu.SDWAM[toffset + j].EB);
#endif
              }
#ifndef WAM
            if (level == 0)
              {
                putbits36 (& cpu.Yblock32[0],  0, 24,
                           cpu.SDW0.ADDR);
                putbits36 (& cpu.Yblock32[0], 24,  3,
                           cpu.SDW0.R1);
                putbits36 (& cpu.Yblock32[0], 27,  3,
                           cpu.SDW0.R2);
                putbits36 (& cpu.Yblock32[0], 30,  3,
                           cpu.SDW0.R3);
                putbits36 (& cpu.Yblock32[0], 37 - 36, 14,
                           cpu.SDW0.BOUND);
                putbits36 (& cpu.Yblock32[1], 51 - 36,  1,
                           cpu.SDW0.R);
                putbits36 (& cpu.Yblock32[1], 52 - 36,  1,
                           cpu.SDW0.E);
                putbits36 (& cpu.Yblock32[1], 53 - 36,  1,
                           cpu.SDW0.W);
                putbits36 (& cpu.Yblock32[1], 54 - 36,  1,
                           cpu.SDW0.P);
                putbits36 (& cpu.Yblock32[1], 55 - 36,  1,
                           cpu.SDW0.U);
                putbits36 (& cpu.Yblock32[1], 56 - 36,  1,
                           cpu.SDW0.G);
                putbits36 (& cpu.Yblock32[1], 57 - 36,  1,
                           cpu.SDW0.C);
                putbits36 (& cpu.Yblock32[1], 58 - 36, 14,
                           cpu.SDW0.EB);

              }
#endif
          }
          break;

        /// Privileged - Clear Associative Memory

        case 0532:  // camp
            do_camp (cpu.TPR.CA);
            break;

        /// EIS - Address Register Load

        case 0560:  // aarn - Alphanumeric Descriptor to Address Register n
        case 0561:
        case 0562:
        case 0563:
        case 0564:
        case 0565:
        case 0566:
        case 0567:
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            PNL (L68_ (DU_CYCLE_DDU_LDEA;))

            if (getbits36_1 (cpu.CY, 23) != 0)
              doFault (FAULT_IPR,
                       (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                       "aarn C(Y)23 != 0");

            uint32 n = opcode & 07;  // get
            CPTUR (cptUsePRn + n);

            // C(Y)0,17 -> C(ARn.WORDNO)
            cpu.AR[n].WORDNO = GETHI (cpu.CY);

            uint TA = getbits36_2 (cpu.CY, 21);
            uint CN = getbits36_3 (cpu.CY, 18);

            switch (TA)
              {
                case CTA4:  // 2
                  // If C(Y)21,22 = 10 (TA code = 2), then
                  //   C(Y)18,20 / 2 -> C(ARn.CHAR)
                  //   4 * (C(Y)18,20)mod2 + 1 -> C(ARn.BITNO)

                  // According to AL39, CN is translated:
                  //  CN   CHAR  BIT
                  //   0      0    1
                  //   1      0    5
                  //   2      1    1
                  //   3      1    5
                  //   4      2    1
                  //   5      2    5
                  //   6      3    1
                  //   7      3    5
                  //SET_AR_CHAR_BITNO (n, CN/2, 4 * (CN % 2) + 1);

                  // According to ISOLTS ps805
                  //  CN   CHAR  BIT
                  //   0      0    0
                  //   1      0    5
                  //   2      1    0
                  //   3      1    5
                  //   4      2    0
                  //   5      2    5
                  //   6      3    0
                  //   7      3    5
                  SET_AR_CHAR_BITNO (n, (word2) (CN/2), (CN % 2) ? 5 : 0);
                        
                  break;

                case CTA6:  // 1
                  // If C(Y)21,22 = 01 (TA code = 1) and C(Y)18,20 = 110
                  // or 111 an illegal procedure fault occurs.
                  if (CN > 5)
                    {
                      cpu.AR[n].WORDNO = 0;
                      SET_AR_CHAR_BITNO (n, 0, 0);
                      doFault (FAULT_IPR,
                               (_fault_subtype)
                                 {.fault_ipr_subtype=FR_ILL_PROC},
                               "aarn TN > 5");
                    }

                  // If C(Y)21,22 = 01 (TA code = 1), then
                  //   (6 * C(Y)18,20) / 9 -> C(ARn.CHAR)
                  //   (6 * C(Y)18,20)mod9 -> C(ARn.BITNO)
                  SET_AR_CHAR_BITNO (n, (word2) ((6 * CN) / 9),
                                     (6 * CN) % 9);
                  break;

                case CTA9:  // 0
                  // If C(Y)21,22 = 00 (TA code = 0), then
                  //   C(Y)18,19 -> C(ARn.CHAR)
                  //   0000 -> C(ARn.BITNO)
                  // remember, 9-bit CN's are funky
                  SET_AR_CHAR_BITNO (n, (word2) (CN >> 1), 0);
                  break;

                case CTAILL: // 3
                  // If C(Y)21,22 = 11 (TA code = 3) an illegal procedure
                  // fault occurs.
                  cpu.AR[n].WORDNO = 0;
                  SET_AR_CHAR_BITNO (n, 0, 0);
                  doFault (FAULT_IPR,
                           (_fault_subtype)
                           {.fault_ipr_subtype=FR_ILL_PROC},
                           "aarn TA = 3");
              }
          }
          break;

        case 0760: // larn - Load Address Register n
        case 0761:
        case 0762:
        case 0763:
        case 0764:
        case 0765:
        case 0766:
        case 0767:
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            //    C(Y)0,23 -> C(ARn)
            PNL (L68_ (DU_CYCLE_DDU_LDEA;))

            uint32 n = opcode & 07;  // get n
            CPTUR (cptUsePRn + n);
            cpu.AR[n].WORDNO = GETHI (cpu.CY);
// AL-38 implies CHAR/BITNO, but ISOLTS requires PR.BITNO.
            SET_AR_CHAR_BITNO (n,  getbits36_2 (cpu.CY, 18),
                               getbits36_4 (cpu.CY, 20));
          }
          break;

        case 0463:  // lareg - Load Address Registers
          PNL (L68_ (DU_CYCLE_DDU_LDEA;))

          for (uint32 n = 0 ; n < 8 ; n += 1)
            {
              CPTUR (cptUsePRn + n);
              word36 tmp36 = cpu.Yblock8[n];
              cpu.AR[n].WORDNO = getbits36_18 (tmp36, 0);
              SET_AR_CHAR_BITNO (n,  getbits36_2 (tmp36, 18),
                                 getbits36_4 (tmp36, 20));
            }
          break;

        case 0467:  // lpl - Load Pointers and Lengths
          PNL (L68_ (DU_CYCLE_DDU_LDEA;))
          words2du (cpu.Yblock8);
          break;

        case 0660: // narn -  (G'Kar?) Numeric Descriptor to Address Register n
        case 0661:
        case 0662:
        case 0663:
        case 0664:
        case 0665:
        case 0666:  // beware!!!! :-)
        case 0667:
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            PNL (L68_ (DU_CYCLE_DDU_LDEA;))

            uint32 n = opcode & 07;  // get
            CPTUR (cptUsePRn + n);

            // C(Y)0,17 -> C(ARn.WORDNO)
            cpu.AR[n].WORDNO = GETHI (cpu.CY);

            uint TN = getbits36_1 (cpu.CY, 21); // C(Y) 21
            uint CN = getbits36_3 (cpu.CY, 18); // C(Y) 18-20

            switch(TN)
              {
                case CTN4:   // 1
                    // If C(Y)21 = 1 (TN code = 1), then
                    //   (C(Y)18,20) / 2 -> C(ARn.CHAR)
                    //   4 * (C(Y)18,20)mod2 + 1 -> C(ARn.BITNO)

                    // According to AL39, CN is translated:
                    //  CN   CHAR  BIT
                    //   0      0    1
                    //   1      0    5
                    //   2      1    1
                    //   3      1    5
                    //   4      2    1
                    //   5      2    5
                    //   6      3    1
                    //   7      3    5
                    //SET_AR_CHAR_BITNO (n, CN/2, 4 * (CN % 2) + 1);

                    // According to ISOLTS ps805
                    //  CN   CHAR  BIT
                    //   0      0    0
                    //   1      0    5
                    //   2      1    0
                    //   3      1    5
                    //   4      2    0
                    //   5      2    5
                    //   6      3    0
                    //   7      3    5
                    SET_AR_CHAR_BITNO (n, (word2) (CN/2), (CN % 2) ? 5 : 0);
                        
                    break;

                case CTN9:  // 0
                  // If C(Y)21 = 0 (TN code = 0) and C(Y)20 = 1 an
                  // illegal procedure fault occurs.
                  if ((CN & 1) != 0)
                    doFault (FAULT_IPR,
                             (_fault_subtype)
                               {.fault_ipr_subtype=FR_ILL_PROC},
                             "narn N9 and CN odd");
                  // The character number is in bits 18-19; recover it
                  CN >>= 1;
                  // If C(Y)21 = 0 (TN code = 0), then
                  //   C(Y)18,20 -> C(ARn.CHAR)
                  //   0000 -> C(ARn.BITNO)
                  SET_AR_CHAR_BITNO (n, (word2) CN, 0);
                  break;
              }
          }
          break;

        /// EIS - Address Register Store

        case 0540:  // aran - Address Register n to Alphanumeric Descriptor
        case 0541:
        case 0542:
        case 0543:
        case 0544:
        case 0545:
        case 0546:
        case 0547:
            {
                // The alphanumeric descriptor is fetched from Y and C(Y)21,22
                // (TA field) is examined to determine the data type described.
                PNL (L68_ (DU_CYCLE_DDU_STEA;))

                uint TA = getbits36_2 (cpu.CY, 21);

                // If C(Y)21,22 = 11 (TA code = 3) or C(Y)23 = 1 (unused bit),
                // an illegal procedure fault occurs.
                if (TA == 03)
                  doFault (FAULT_IPR,
                           (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                           "ARAn tag == 3");
                if (getbits36_1 (cpu.CY, 23) != 0)
                  doFault (FAULT_IPR,
                           (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC},
                           "ARAn b23 == 1");

                uint32 n = opcode & 07;  // get
                CPTUR (cptUsePRn + n);
                // For n = 0, 1, ..., or 7 as determined by operation code

                // C(ARn.WORDNO) -> C(Y)0,17
                putbits36_18 (& cpu.CY, 0, cpu.AR[n].WORDNO & MASK18);

                // If TA = 1 (6-bit data) or TA = 2 (4-bit data), C(ARn.CHAR)
                // and C(ARn.BITNO) are translated to an equivalent character
                // position that goes to C(Y)18,20.

                int CN = 0;

                switch (TA)
                {
                    case CTA4:  // 2
                        // If C(Y)21,22 = 10 (TA code = 2), then
                        // (9 * C(ARn.CHAR) + C(ARn.BITNO) - 1) / 4 -> C(Y)18,20
                        CN = (9 * GET_AR_CHAR (n) + GET_AR_BITNO (n) - 1) / 4;
                        putbits36_3 (& cpu.CY, 18, (word3) CN & MASK3);
                        break;

                    case CTA6:  // 1
                        // If C(Y)21,22 = 01 (TA code = 1), then
                        // (9 * C(ARn.CHAR) + C(ARn.BITNO)) / 6 -> C(Y)18,20
                        CN = (9 * GET_AR_CHAR (n) + GET_AR_BITNO (n)) / 6;
                        putbits36_3 (& cpu.CY, 18, (word3) CN & MASK3);
                        break;

                    case CTA9:  // 0
                        // If C(Y)21,22 = 00 (TA code = 0), then
                        //   C(ARn.CHAR) -> C(Y)18,19
                        //   0 -> C(Y)20
                        putbits36_3 (& cpu.CY, 18,
                                     (word3) ((GET_AR_CHAR (n) & MASK2) << 1));
                        break;
                }
            }
            break;

        case 0640:  // arnn -  Address Register n to Numeric Descriptor
        case 0641:
        case 0642:
        case 0643:
        case 0644:
        case 0645:
        case 0646:
        case 0647:
            {
                PNL (L68_ (DU_CYCLE_DDU_STEA;))
                uint32 n = opcode & 07;  // get register #
                CPTUR (cptUsePRn + n);

                // The Numeric descriptor is fetched from Y and C(Y)21,22 (TA
                // field) is examined to determine the data type described.

                uint TN = getbits36_1 (cpu.CY, 21); // C(Y) 21

                // For n = 0, 1, ..., or 7 as determined by operation code
                // C(ARn.WORDNO) -> C(Y)0,17
                putbits36_18 (& cpu.CY, 0, cpu.AR[n].WORDNO & MASK18);

                switch (TN)
                {
                    case CTN4:  // 1
                      {
                        // If C(Y)21 = 1 (TN code = 1) then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO) - 1) / 4 ->
                        //     C(Y)18,20
                        word3 CN = (9 * GET_AR_CHAR (n) +
                                    GET_AR_BITNO (n) - 1) / 4;
                        putbits36_3 (& cpu.CY, 18, CN & MASK3);
                        break;
                      }
                    case CTN9:  // 0
                        // If C(Y)21 = 0 (TN code = 0), then
                        //   C(ARn.CHAR) -> C(Y)18,19
                        //   0 -> C(Y)20
                        putbits36_3 (& cpu.CY, 18,
                                     (word3) ((GET_AR_CHAR (n) & MASK2) << 1));
                        break;
                }
            }
            break;

        case 0740:  // sarn - Store Address Register n
        case 0741:
        case 0742:
        case 0743:
        case 0744:
        case 0745:
        case 0746:
        case 0747:
            //For n = 0, 1, ..., or 7 as determined by operation code
            //  C(ARn) -> C(Y)0,23
            //  C(Y)24,35 -> unchanged
            {
                PNL (L68_ (DU_CYCLE_DDU_STEA;))
                uint32 n = opcode & 07;  // get n
                CPTUR (cptUsePRn + n);
                putbits36 (& cpu.CY,  0, 18, cpu.PR[n].WORDNO);
// AL-39 implies CHAR/BITNO, but ISOLTS test 805 requires BITNO
                putbits36 (& cpu.CY, 18, 2, GET_AR_CHAR (n));
                putbits36 (& cpu.CY, 20, 4, GET_AR_BITNO (n));
                //putbits36 (& cpu.CY, 18, 6, GET_PR_BITNO (n));
                break;
            }

        case 0443:  // sareg - Store Address Registers
            // a:AL39/ar1 According to ISOLTS ps805, the BITNO data is stored
            // in BITNO format, not CHAR/BITNO.
            PNL (L68_ (DU_CYCLE_DDU_STEA;))
            memset (cpu.Yblock8, 0, sizeof (cpu.Yblock8));
            for (uint32 n = 0 ; n < 8 ; n += 1)
            {
                CPTUR (cptUsePRn + n);
                word36 arx = 0;
                putbits36 (& arx,  0, 18, cpu.PR[n].WORDNO);
                putbits36 (& arx, 18,  2, GET_AR_CHAR (n));
                putbits36 (& arx, 20,  4, GET_AR_BITNO (n));
                cpu.Yblock8[n] = arx;
            }
            break;

        case 0447:  // spl - Store Pointers and Lengths
            PNL (L68_ (DU_CYCLE_DDU_STEA;))
            du2words (cpu.Yblock8);
          break;

        /// EIS - Address Register Special Arithmetic

        case 0502:  // a4bd Add 4-bit Displacement to Address Register
          asxbd (4, false);
          break;

        case 0501:  // a6bd Add 6-bit Displacement to Address Register
          asxbd (6, false);
          break;

        case 0500:  // a9bd Add 9-bit Displacement to Address Register
          asxbd (9, false);
          break;

        case 0503:  // abd  Add bit Displacement to Address Register
          asxbd (1, false);
          break;

        case 0507:  // awd Add  word Displacement to Address Register
          asxbd (36, false);
          break;

        case 0522:  // s4bd Subtract 4-bit Displacement from Address Register
          asxbd (4, true);
          break;

        case 0521:  // s6bd   Subtract 6-bit Displacement from Address Register
          asxbd (6, true);
          break;

        case 0520:  // s9bd   Subtract 9-bit Displacement from Address Register
          asxbd (9, true);
          break;

        case 0523:  // sbd Subtract   bit Displacement from Address Register
          asxbd (1, true);
          break;

        case 0527:  // swd Subtract  word Displacement from Address Register
          asxbd (36, true);
          break;

        /// EIS = Alphanumeric Compare

        case 0106:  // cmpc
          cmpc ();
          break;

        case 0120:  // scd
          scd ();
          break;

        case 0121:  // scdr
          scdr ();
          break;

        case 0124:  // scm
          scm ();
          break;

        case 0125:  // scmr
          scmr ();
          break;

        case 0164:  // tct
          tct ();
          break;

        case 0165:  // tctr
          tctr ();
          break;

        /// EIS - Alphanumeric Move

        case 0100:  // mlr
          mlr ();
          break;

        case 0101:  // mrl
          mrl ();
          break;

        case 0020:  // mve
          mve ();
          break;

        case 0160:  // mvt
          mvt ();
          break;

        /// EIS - Numeric Compare

        case 0303:  // cmpn
          cmpn ();
          break;

        /// EIS - Numeric Move

        case 0300:  // mvn
          mvn ();
          break;

        case 0024:   // mvne
          mvne ();
          break;

        /// EIS - Bit String Combine

        case 0060:   // csl
          csl (false);
          break;

        case 0061:   // csr
          csr (false);
          break;

        /// EIS - Bit String Compare

        case 0066:   // cmpb
          cmpb ();
          break;

        /// EIS - Bit String Set Indicators

        case 0064:   // sztl
          // The execution of this instruction is identical to the Combine
          // Bit Strings Left (csl) instruction except that C(BOLR)m is
          // not placed into C(Y-bit2)i-1.
          csl (true);
          break;

        case 0065:   // sztr
          // The execution of this instruction is identical to the Combine
          // Bit Strings Left (csr) instruction except that C(BOLR)m is
          // not placed into C(Y-bit2)i-1.
          csr (true);
          break;

        /// EIS -- Data Conversion

        case 0301:  // btd
          btd ();
          break;

        case 0305:  // dtb
          dtb ();
          break;

        /// EIS - Decimal Addition

        case 0202:  // ad2d
            ad2d ();
            break;

        case 0222:  // ad3d
            ad3d ();
            break;

        /// EIS - Decimal Subtraction

        case 0203:  // sb2d
            sb2d ();
            break;

        case 0223:  // sb3d
            sb3d ();
            break;

        /// EIS - Decimal Multiplication

        case 0206:  // mp2d
            mp2d ();
            break;

        case 0226:  // mp3d
            mp3d ();
            break;

        /// EIS - Decimal Division

        case 0207:  // dv2d
            dv2d ();
            break;

        case 0227:  // dv3d
            dv3d ();
            break;

#ifdef TESTING
#if EMULATOR_ONLY

        case 0420:  // emcall instruction Custom, for an emulator call for
                    //  simh stuff ...
        {
            int ret = emCall ();
            if (ret)
              return ret;
            break;
        }

#endif
#endif
        default:
            if (cpu.switches.halt_on_unimp)
                return STOP_ILLOP;
            doFault (FAULT_IPR,
                     (_fault_subtype) {.fault_ipr_subtype=FR_ILL_OP},
                     "Illegal instruction");
    }
    PNL (L68_ (DU_CYCLE_END;))

#ifdef L68
    if (cpu.MR_cache.emr && cpu.MR_cache.ihr && is_du)
      addDUhist ();
#endif

    return SCPE_OK;

}



#ifdef TESTING
#include <ctype.h>

#if EMULATOR_ONLY
/**
 * emulator call instruction. Do whatever address field sez' ....
 */
static int emCall (void)
{
    DCDstruct * i = & cpu.currentInstruction;
    switch (i->address) // address field
    {
        case 1:     // putc9 - put 9-bit char in AL to stdout
        {
            if (cpu.rA > 0xff)  // don't want no 9-bit bytes here!
                break;

            char c = cpu.rA & 0x7f;
            if (c)  // ignore NULL chars.
                sim_printf ("%c", c);
            break;
        }
        case 0100:     // putc9 - put 9-bit char in A(0) to stdout
        {
            char c = (cpu.rA >> 27) & 0x7f;
            if (isascii (c))  // ignore NULL chars.
                sim_printf ("%c", c);
            else
                sim_printf ("\\%03o", c);
            break;
        }
        case 2:     // putc6 - put 6-bit char in A to stdout
        {
            int c = GEBcdToASCII[cpu.rA & 077];
            if (c != -1)
            {
                if (isascii (c))  // ignore NULL chars.
                    sim_printf ("%c", c);
                else
                    sim_printf ("\\%3o", c);
            }
            break;
        }
        case 3:     // putoct - put octal contents of A to stdout (split)
        {
            sim_printf ("%06o %06o", GETHI (cpu.rA), GETLO (cpu.rA));
            break;
        }
        case 4:     // putoctZ - put octal contents of A to stdout
                    // (zero-suppressed)
        {
            sim_printf ("%"PRIo64"", cpu.rA);
            break;
        }
        case 5:     // putdec - put decimal contents of A to stdout
        {
            t_int64 tmp = SIGNEXT36_64 (cpu.rA);
            sim_printf ("%"PRId64"", tmp);
            break;
        }
        case 6:     // putEAQ - put float contents of C(EAQ) to stdout
        {
#ifndef __MINGW64__
            long double eaq = EAQToIEEElongdouble ();
            sim_printf ("%12.8Lg", eaq);
#else
            double eaq = EAQToIEEEdouble();
            sim_printf("%12.8g", eaq);
#endif
            break;
        }
        case 7:   // dump index registers
            for (int i = 0 ; i < 8 ; i += 4)
                sim_printf ("r[%d]=%06o r[%d]=%06o r[%d]=%06o r[%d]=%06o\n",
                           i+0, cpu.rX[i+0], i+1, cpu.rX[i+1], i+2, cpu.rX[i+2],
                           i+3, cpu.rX[i+3]);
            break;

        case 17: // dump pointer registers
            for (int n = 0 ; n < 8 ; n++)
            {
                sim_printf ("PR[%d]/%s: SNR=%05o RNR=%o WORDNO=%06o "
                           "BITNO:%02o\n",
                           n, PRalias[n], cpu.PR[n].SNR, cpu.PR[n].RNR,
                           cpu.PR[n].WORDNO, GET_PR_BITNO (n));
            }
            break;
        case 27:    // dump registers A & Q
            sim_printf ("A: %012"PRIo64" Q:%012"PRIo64"\n", cpu.rA, cpu.rQ);
            break;

        case 8: // crlf to console
            sim_printf ("\n");
            break;

        case 13:     // putoct - put octal contents of Q to stdout (split)
        {
            sim_printf ("%06o %06o", GETHI (cpu.rQ), GETLO (cpu.rQ));
            break;
        }
        case 14:     // putoctZ - put octal contents of Q to stdout
                     // (zero-suppressed)
        {
            sim_printf ("%"PRIo64"", cpu.rQ);
            break;
        }
        case 15:     // putdec - put decimal contents of Q to stdout
        {
            t_int64 tmp = SIGNEXT36_64 (cpu.rQ);
            sim_printf ("%"PRId64"", tmp);
            break;
        }

        case 16:     // puts - A high points to by an aci string; print it.
                     // The string includes C-sytle escapes: \0 for end
                     // of string, \n for newline, \\ for a backslash
        case 21: // puts: A contains a 24 bit address
        {
            const int maxlen = 256;
            char buf[maxlen + 1];

            word36 addr;
            if (i->address == 16)
              addr = cpu.rA >> 18;
            else // 21
              addr = cpu.rA >> 12;
            word36 chunk = 0;
            int i;
            bool is_escape = false;
            int cnt = 0;

            for (i = 0; cnt < maxlen; i ++)
            {
                // fetch char
                if (i % 4 == 0)
                    chunk = M[addr ++];
                word36 wch = chunk >> (9 * 3);
                chunk = (chunk << 9) & DMASK;
                char ch = (char) (wch & 0x7f);

                if (is_escape)
                {
                    if (ch == '0')
                        ch = '\0';
                    else if (ch == 'n')
                        ch = '\n';
                    else
                    {
                        /* ch = ch */;
                    }
                    is_escape = false;
                    buf[cnt ++] = ch;
                    if (ch == '\0')
                      break;
                }
                else
                {
                    if (ch == '\\')
                        is_escape = true;
                    else
                    {
                        buf[cnt ++] = ch;
                        if (ch == '\0')
                            break;
                    }
                }
            }
            // Safety; if filled buffer before finding eos, put an eos
            // in the extra space that was allocated
            buf[maxlen] = '\0';
            sim_printf ("%s", buf);
            break;
        }

        // case 17 used above

        case 18:     // halt
            return STOP_HALT;

        case 19:     // putdecaq - put decimal contents of AQ to stdout
        {
            int64_t t0 = SIGNEXT36_64 (cpu.rA);
            __int128_t AQ = ((__int128_t) t0) << 36;
            AQ |= (cpu.rQ & DMASK);
            print_int128 (AQ, NULL);
            break;
        }

        case 20:    // Report fault
        {
            emCallReportFault ();
             break;
        }

        // case 21 defined above

    }
    return 0;
}
#endif
#endif // TESTING

// CANFAULT
static int doABSA (word36 * result)
  {
    word36 res;
    sim_debug (DBG_APPENDING, & cpu_dev, "absa CA:%08o\n", cpu.TPR.CA);

    //if (get_addr_mode () == ABSOLUTE_mode && ! cpu.isb29)
    if (get_addr_mode () == ABSOLUTE_mode && ! cpu.currentInstruction.b29)
      {
        * result = ((word36) (cpu.TPR.CA & MASK18)) << 12; // 24:12 format
        return SCPE_OK;
      }

    // ABSA handles directed faults differently, so a special append cycle is needed.
    // doAppendCycle also provides WAM support, which is required by ISOLTS-860 02
    //res = (word36) doAppendCycle (cpu.TPR.CA & MASK18, ABSA_CYCLE, NULL, 0) << 12;
    res = (word36) doAppendCycle (ABSA_CYCLE, NULL, 0) << 12;

    * result = res;

    return SCPE_OK;
  }

void doRCU (void)
  {
#ifdef LOOPTRC
elapsedtime ();
 sim_printf (" rcu to %05o:%06o  PSR:IC %05o:%06o\r\n",  (cpu.Yblock8[0]>>18)&MASK15, (cpu.Yblock8[4]>>18)&MASK18, cpu.PPR.PSR, cpu.PPR.IC);
#endif

    if_sim_debug (DBG_TRACE, & cpu_dev)
      {
        for (int i = 0; i < 8; i ++)
          {
            sim_debug (DBG_TRACE, & cpu_dev, "RCU %d %012"PRIo64"\n", i,
                       cpu.Yblock8[i]);
          }
      }

    words2scu (cpu.Yblock8);

// Restore addressing mode

    word1 saveP = cpu.PPR.P; // ISOLTS-870 02m
    if (TST_I_ABS == 0)
      set_addr_mode (APPEND_mode);
    else
      set_addr_mode (ABSOLUTE_mode);
    cpu.PPR.P = saveP;

    if (cpu.cu.FLT_INT == 0) // is interrupt, not fault
      {
        sim_debug (DBG_TRACE, & cpu_dev, "RCU interrupt return\n");
        longjmp (cpu.jmpMain, JMP_REFETCH);
      }

    // Resync the append unit
    fauxDoAppendCycle (INSTRUCTION_FETCH);

// All of the faults list as having handlers have actually
// been encountered in Multics operation and are believed
// to be being handled correctly. The handlers in
// parenthesis are speculative and untested.
//
// Unhandled:
//
//    SDF Shutdown: Why would you RCU from a shutdown fault?
//    STR Store:
//      AL39 is contradictory or vague about store fault subfaults and store
//      faults in general. They are mentioned:
//        SPRPn: store fault (illegal pointer) (assuming STR:ISN)
//        SMCM: store fault (not control)  -
//        SMIC: store fault (not control)   > I believe that these should be
//        SSCR: store fault (not control)  -  command fault
//        TSS:  STR:OOB
//        Bar mode out-of-bounds: STR:OOB
//     The SCU register doesn't define which bit is "store fault (not control)"
// STR:ISN - illegal segment number
// STR:NEA - nonexistent address
// STR:OOB - bar mode out-of-bounds
//
// decimal   octal
// fault     fault  mnemonic   name             priority group  handler
// number   address
//   0         0      sdf      Shutdown               27 7
//   1         2      str      Store                  10 4                                  getBARaddress, instruction execution
//   2         4      mme      Master mode entry 1    11 5      JMP_SYNC_FAULT_RETURN       instruction execution
//   3         6      f1       Fault tag 1            17 5      (JMP_REFETCH/JMP_RESTART)   doComputedAddressFormation
//   4        10      tro      Timer runout           26 7      JMP_REFETCH                 FETCH_cycle
//   5        12      cmd      Command                 9 4      JMP_REFETCH/JMP_RESTART     instruction execution
//   6        14      drl      Derail                 15 5      JMP_REFETCH/JMP_RESTART     instruction execution
//   7        16      luf      Lockup                  5 4      JMP_REFETCH                 doComputedAddressFormation, FETCH_cycle
//   8        20      con      Connect                25 7      JMP_REFETCH                 FETCH_cycle
//   9        22      par      Parity                  8 4
//  10        24      ipr      Illegal procedure      16 5                                  doITSITP, doComputedAddressFormation, instruction execution
//  11        26      onc      Operation not complete  4 2                                  nem_check, instruction execution
//  12        30      suf      Startup                 1 1
//  13        32      ofl      Overflow                7 3      JMP_REFETCH/JMP_RESTART     instruction execution
//  14        34      div      Divide check            6 3                                  instruction execution
//  15        36      exf      Execute                 2 1      JMP_REFETCH/JMP_RESTART     FETCH_cycle
//  16        40      df0      Directed fault 0       20 6      JMP_REFETCH/JMP_RESTART     getSDW, doAppendCycle
//  17        42      df1      Directed fault 1       21 6      JMP_REFETCH/JMP_RESTART     getSDW, doAppendCycle
//  18        44      df2      Directed fault 2       22 6      (JMP_REFETCH/JMP_RESTART)   getSDW, doAppendCycle
//  19        46      df3      Directed fault 3       23 6      JMP_REFETCH/JMP_RESTART     getSDW, doAppendCycle
//  20        50      acv      Access violation       24 6      JMP_REFETCH/JMP_RESTART     fetchDSPTW, modifyDSPTW, fetchNSDW, doAppendCycle, EXEC_cycle (ring alarm)
//  21        52      mme2     Master mode entry 2    12 5      JMP_SYNC_FAULT_RETURN       instruction execution
//  22        54      mme3     Master mode entry 3    13 5      (JMP_SYNC_FAULT_RETURN)     instruction execution
//  23        56      mme4     Master mode entry 4    14 5      (JMP_SYNC_FAULT_RETURN)     instruction execution
//  24        60      f2       Fault tag 2            18 5      JMP_REFETCH/JMP_RESTART     doComputedAddressFormation
//  25        62      f3       Fault tag 3            19 5      JMP_REFETCH/JMP_RESTART     doComputedAddressFormation
//  26        64               Unassigned
//  27        66               Unassigned
//  28        70               Unassigned
//  29        72               Unassigned
//  30        74               Unassigned
//  31        76      trb      Trouble                 3 2                                  FETCH_cycle, doRCU


// Reworking logic

#define rework
#ifdef rework
    if (cpu.cu.FIF) // fault occured during instruction fetch
      {
//if (cpu.cu.rfi) sim_printf ( "RCU FIF refetch return caught rfi\n");
        // I am misusing this bit; on restart I want a way to tell the
        // CPU state machine to restart the instruction, which is not
        // how Multics uses it. I need to pick a different way to
        // communicate; for now, turn it off on refetch so the state
        // machine doesn't become confused.
        cpu.cu.rfi = 0;
        sim_debug (DBG_TRACE, & cpu_dev, "RCU FIF REFETCH return\n");
        longjmp (cpu.jmpMain, JMP_REFETCH);
      }

// RFI means 'refetch this instruction'
    if (cpu.cu.rfi)
      {
//sim_printf ( "RCU rfi refetch return\n");
        sim_debug (DBG_TRACE, & cpu_dev, "RCU rfi refetch return\n");
// Setting the to RESTART causes ISOLTS 776 to report unexpected
// trouble faults.
// Without clearing rfi, ISOLTS pm776-08i LUFs.
        cpu.cu.rfi = 0;
        longjmp (cpu.jmpMain, JMP_REFETCH);
      }

// The debug command uses MME2 to implement breakpoints, but it is not
// clear what it does to the MC data to signal RFI behavior.

    if (cpu.cu.FI_ADDR == FAULT_MME ||
        cpu.cu.FI_ADDR == FAULT_MME2 ||
        cpu.cu.FI_ADDR == FAULT_MME3 ||
        cpu.cu.FI_ADDR == FAULT_MME4 ||
        cpu.cu.FI_ADDR == FAULT_DRL)
    //if (cpu.cu.FI_ADDR == FAULT_MME2)
      {
//sim_printf ("MME2 restart\n");
        sim_debug (DBG_TRACE, & cpu_dev, "RCU MME2 restart return\n");
        cpu.cu.rfi = 0;
        longjmp (cpu.jmpMain, JMP_RESTART);
      }
#else
    if (cpu.cu.rfi || // S/W asked for the instruction to be started
        cpu.cu.FIF) // fault occured during instruction fetch
      {

        // I am misusing this bit; on restart I want a way to tell the
        // CPU state machine to restart the instruction, which is not
        // how Multics uses it. I need to pick a different way to
        // communicate; for now, turn it off on refetch so the state
        // machine doesn't become confused.

        cpu.cu.rfi = 0;
        sim_debug (DBG_TRACE, & cpu_dev, "RCU rfi/FIF REFETCH return\n");
        longjmp (cpu.jmpMain, JMP_REFETCH);
      }

// It seems obvious that MMEx should do a JMP_SYNC_FAULT_RETURN, but doing
// a JMP_RESTART makes 'debug' work. (The same change to DRL does not make
// 'gtss' work, tho.

    if (cpu.cu.FI_ADDR == FAULT_MME2)
      {
//sim_printf ("MME2 restart\n");
        sim_debug (DBG_TRACE, & cpu_dev, "RCU MME2 restart return\n");
        cpu.cu.rfi = 1;
        longjmp (cpu.jmpMain, JMP_RESTART);
      }
#endif

#if 0
// I beleive this logic is correct (cf. ISOLTS pa870 test-02d TRA PR1|6 not
// switching to append mode do to page fault clearing went_appending), but the
// emulator's refetching of operand descriptors after page fault of EIS
// instruction in absolute mode is breaking the logic.
    // If restarting after a page fault, set went_appending...
    if (cpu.cu.FI_ADDR == FAULT_DF0 ||
        cpu.cu.FI_ADDR == FAULT_DF1 ||
        cpu.cu.FI_ADDR == FAULT_DF2 ||
        cpu.cu.FI_ADDR == FAULT_DF3 ||
        cpu.cu.FI_ADDR == FAULT_ACV ||
        cpu.cu.FI_ADDR == FAULT_F1 ||
        cpu.cu.FI_ADDR == FAULT_F2 ||
        cpu.cu.FI_ADDR == FAULT_F3)
      {
        set_went_appending ();
      }
#endif
    // MME faults resume with the next instruction



#ifdef rework
    if (cpu.cu.FI_ADDR == FAULT_DIV ||
        cpu.cu.FI_ADDR == FAULT_OFL ||
        cpu.cu.FI_ADDR == FAULT_IPR)
      {
        sim_debug (DBG_TRACE, & cpu_dev, "RCU sync fault return\n");
        cpu.cu.rfi = 0;
        longjmp (cpu.jmpMain, JMP_SYNC_FAULT_RETURN);
      }
#else
    if (cpu.cu.FI_ADDR == FAULT_MME ||
        /* cpu.cu.FI_ADDR == FAULT_MME2 || */
        cpu.cu.FI_ADDR == FAULT_MME3 ||
        cpu.cu.FI_ADDR == FAULT_MME4 ||
        cpu.cu.FI_ADDR == FAULT_DRL ||
        cpu.cu.FI_ADDR == FAULT_DIV ||
        cpu.cu.FI_ADDR == FAULT_OFL ||
        cpu.cu.FI_ADDR == FAULT_IPR)
      {
        sim_debug (DBG_TRACE, & cpu_dev, "RCU MMEx sync fault return\n");
        cpu.cu.rfi = 0;
        longjmp (cpu.jmpMain, JMP_SYNC_FAULT_RETURN);
      }
#endif






    // LUF can happen during fetch or CAF. If fetch, handled above
    if (cpu.cu.FI_ADDR == FAULT_LUF)
      {
        cpu.cu.rfi = 1;
        sim_debug (DBG_TRACE, & cpu_dev, "RCU LUF RESTART return\n");
        longjmp (cpu.jmpMain, JMP_RESTART);
      }

    if (cpu.cu.FI_ADDR == FAULT_DF0 ||
        cpu.cu.FI_ADDR == FAULT_DF1 ||
        cpu.cu.FI_ADDR == FAULT_DF2 ||
        cpu.cu.FI_ADDR == FAULT_DF3 ||
        cpu.cu.FI_ADDR == FAULT_ACV ||
        cpu.cu.FI_ADDR == FAULT_F1 ||
        cpu.cu.FI_ADDR == FAULT_F2 ||
        cpu.cu.FI_ADDR == FAULT_F3 ||
        cpu.cu.FI_ADDR == FAULT_CMD ||
        cpu.cu.FI_ADDR == FAULT_EXF)
      {
        // If the fault occurred during fetch, handled above.
        cpu.cu.rfi = 1;
        sim_debug (DBG_TRACE, & cpu_dev, "RCU ACV RESTART return\n");
        longjmp (cpu.jmpMain, JMP_RESTART);
      }
    sim_printf ("doRCU dies with unhandled fault number %d\n", cpu.cu.FI_ADDR);
    doFault (FAULT_TRB,
             (_fault_subtype) {.bits=cpu.cu.FI_ADDR},
             "doRCU dies with unhandled fault number");
  }


