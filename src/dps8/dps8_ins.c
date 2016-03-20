#define ISOLTS_BITNO

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
#if EMULATOR_ONLY
static int emCall (void);
#endif

// CANFAULT
static void writeOperands (void)
{
    DCDstruct * i = & cpu.currentInstruction;

    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "writeOperands(%s):mne=%s flags=%x\n",
               disAssemble (IWB_IRODD), i -> info -> mne, i -> info -> flags);

    word6 rTAG = 0;
    if (! (i -> info -> flags & NO_TAG))
      rTAG = GET_TAG (cpu.cu.IWB);
    word6 Td = GET_TD (rTAG);
    word6 Tm = GET_TM (rTAG);

//
// IT CI/SC/SCR
//

    if (Tm == TM_IT && (Td == IT_CI || Td == IT_SC || Td == IT_SCR))
      {
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

        word36 indword;
        word36 indwordAddress = cpu.TPR.CA;
        Read (indwordAddress, & indword, OPERAND_READ, i -> a);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT indword=%012llo\n", indword);

        //
        // Parse and validate the indirect word
        //

        word18 Yi = GET_ADDR (indword);
        word6 characterOperandSize = GET_TB (GET_TAG (indword));
        word6 characterOperandOffset = GET_CF (GET_TAG (indword));

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT size=%o offset=%o Yi=%06o\n",
                   characterOperandSize, characterOperandOffset,
                   Yi);

        if (characterOperandSize == TB6 && characterOperandOffset > 5)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR, flt_ipr_ill_mod,
                   "co size == TB6 && offset > 5");

        if (characterOperandSize == TB9 && characterOperandOffset > 3)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR, flt_ipr_ill_mod,
                   "co size == TB9 && offset > 3");

        if (Td == IT_SCR)
          {
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

            if (characterOperandOffset == 0)
              {
                if (characterOperandSize == TB6)
                    characterOperandOffset = 5;
                else
                    characterOperandOffset = 3;
                Yi -= 1;
                Yi &= MASK18;
              }
                else
              {
                characterOperandOffset -= 1;
              }
          }

        //
        // Get the data word
        //

        cpu.cu.pot = 1;

        word36 data;
        Read (Yi, & data, OPERAND_READ, i -> a);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT data=%012llo\n", data);

        cpu.cu.pot = 0;

        //
        // Put the character into the data word
        //

        switch (characterOperandSize)
          {
            case TB6:
              putChar (& data, cpu.CY & 077, characterOperandOffset);
              break;

            case TB9:
              putByte (& data, cpu.CY & 0777, characterOperandOffset);
              break;
          }

        //
        // Write it
        //

        Write (Yi, data, OPERAND_STORE, i -> a);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "writeOperands IT wrote char/byte %012llo to %06o "
                   "tTB=%o tCF=%o\n",
                   data, Yi,
                   characterOperandSize, characterOperandOffset);

        // Restore the CA; Read/Write() updates it.
        cpu.TPR.CA = indwordAddress;

        return;
      } // IT



    WriteOP (cpu.TPR.CA, OPERAND_STORE, i -> a);

    return;
}

// CANFAULT
static void readOperands (void)
{
    DCDstruct * i = & cpu.currentInstruction;

    sim_debug(DBG_ADDRMOD, &cpu_dev,
              "readOperands(%s):mne=%s flags=%x\n",
              disAssemble(cpu.cu.IWB), i->info->mne, i->info->flags);
    sim_debug(DBG_ADDRMOD, &cpu_dev,
              "readOperands a %d address %08o\n", i -> a, cpu.TPR.CA);

    word6 rTAG = 0;
    if (! (i -> info -> flags & NO_TAG))
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
                   "readOperands DU CY=%012llo\n", cpu.CY);
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
                   "readOperands DL CY=%012llo\n", cpu.CY);
        return;
      }

//
// IT CI/SC/SCR
//

    if (Tm == TM_IT && (Td == IT_CI || Td == IT_SC || Td == IT_SCR))
      {
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

        word36 indword;
        word36 indwordAddress = cpu.TPR.CA;
        Read (indwordAddress, & indword, OPERAND_READ, i -> a);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT indword=%012llo\n", indword);

        //
        // Parse and validate the indirect word
        //

        word18 Yi = GET_ADDR (indword);
        word6 characterOperandSize = GET_TB (GET_TAG (indword));
        word6 characterOperandOffset = GET_CF (GET_TAG (indword));

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT size=%o offset=%o Yi=%06o\n",
                   characterOperandSize, characterOperandOffset,
                   Yi);

        if (characterOperandSize == TB6 && characterOperandOffset > 5)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR, flt_ipr_ill_mod,
                   "co size == TB6 && offset > 5");

        if (characterOperandSize == TB9 && characterOperandOffset > 3)
          // generate an illegal procedure, illegal modifier fault
          doFault (FAULT_IPR, flt_ipr_ill_mod,
                   "co size == TB9 && offset > 3");

        if (Td == IT_SCR)
          {
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

            if (characterOperandOffset == 0)
              {
                if (characterOperandSize == TB6)
                    characterOperandOffset = 5;
                else
                    characterOperandOffset = 3;
                Yi -= 1;
                Yi &= MASK18;
              }
                else
              {
                characterOperandOffset -= 1;
              }
          }

        //
        // Get the data word
        //

        word36 data;
        Read (Yi, & data, OPERAND_READ, i -> a);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT data=%012llo\n", data);

        //
        // Get the character from the data word
        //

        switch (characterOperandSize)
          {
            case TB6:
              cpu.CY = GETCHAR (data, characterOperandOffset);
              break;

            case TB9:
              cpu.CY = GETBYTE (data, characterOperandOffset);
              break;
          }

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands IT read operand %012llo from"
                   " %06o char/byte=%llo\n",
                   data, Yi, cpu.CY);

        // Restore the CA; Read/Write() updates it.
        cpu.TPR.CA = indwordAddress;

        return;
      } // IT


    ReadOP (cpu.TPR.CA, OPERAND_READ, i -> a);

    return;
  }

static void scu2words(word36 *words)
  {
    memset (words, 0, 8 * sizeof (* words));

    // words [0]
    putbits36 (& words [0],  0,  3, cpu.PPR.PRR);
    putbits36 (& words [0],  3, 15, cpu.PPR.PSR);
    putbits36 (& words [0], 18,  1, cpu.PPR.P);
    // 19, 1 XSF External segment flag
    // 20, 1 SDWAMM Match on SDWAM
    putbits36 (& words [0], 21,  1, cpu.cu.SD_ON);
    // 22, 1 PTWAMM Match on PTWAM
    putbits36 (& words [0], 23,  1, cpu.cu.PT_ON);
#if 0
    putbits36 (& words [0], 24,  1, cpu.cu.PI_AP);   // 24    PI-AP
    putbits36 (& words [0], 25,  1, cpu.cu.DSPTW);   // 25    DSPTW
    putbits36 (& words [0], 26,  1, cpu.cu.SDWNP);   // 26    SDWNP
    putbits36 (& words [0], 27,  1, cpu.cu.SDWP);    // 27    SDWP
    putbits36 (& words [0], 28,  1, cpu.cu.PTW);     // 28    PTW
    putbits36 (& words [0], 29,  1, cpu.cu.PTW2);    // 29    PTW2
    putbits36 (& words [0], 30,  1, cpu.cu.FAP);     // 30    FAP
    putbits36 (& words [0], 31,  1, cpu.cu.FANP);    // 31    FANP
    putbits36 (& words [0], 32,  1, cpu.cu.FABS);    // 32    FABS
                   // 33-35 FCT   Fault counter - counts retries

#else
    putbits36 (& words [0], 24, 12, cpu.cu.APUCycleBits);
#endif

    // words [1]

    putbits36 (& words [1],  0,  1, cpu.cu.IRO_ISN);
    putbits36 (& words [1],  1,  1, cpu.cu.OEB_IOC);
    putbits36 (& words [1],  2,  1, cpu.cu.EOFF_IAIM);
    putbits36 (& words [1],  3,  1, cpu.cu.ORB_ISP);
    putbits36 (& words [1],  4,  1, cpu.cu.ROFF_IPR);
    putbits36 (& words [1],  5,  1, cpu.cu.OWB_NEA);
    putbits36 (& words [1],  6,  1, cpu.cu.WOFF_OOB);
    putbits36 (& words [1],  7,  1, cpu.cu.NO_GA);
    putbits36 (& words [1],  8,  1, cpu.cu.OCB);
    putbits36 (& words [1],  9,  1, cpu.cu.OCALL);
    putbits36 (& words [1], 10,  1, cpu.cu.BOC);
    putbits36 (& words [1], 11,  1, cpu.cu.PTWAM_ER);
    putbits36 (& words [1], 12,  1, cpu.cu.CRT);
    putbits36 (& words [1], 13,  1, cpu.cu.RALR);
    putbits36 (& words [1], 14,  1, cpu.cu.SWWAM_ER);
    putbits36 (& words [1], 15,  1, cpu.cu.OOSB);
    putbits36 (& words [1], 16,  1, cpu.cu.PARU);
    putbits36 (& words [1], 17,  1, cpu.cu.PARL);
    putbits36 (& words [1], 18,  1, cpu.cu.ONC1);
    putbits36 (& words [1], 19,  1, cpu.cu.ONC2);
    putbits36 (& words [1], 20,  4, cpu.cu.IA);
    putbits36 (& words [1], 24,  3, cpu.cu.IACHN);
    putbits36 (& words [1], 27,  3, cpu.cu.CNCHN);
    putbits36 (& words [1], 30,  5, cpu.cu.FI_ADDR);
    putbits36 (& words [1], 35, 1, cpu.cycle == INTERRUPT_cycle ? 0 : 1);

    // words [2]

    putbits36 (& words [2],  0,  3, cpu.TPR.TRR);
    putbits36 (& words [2],  3, 15, cpu.TPR.TSR);
    // 18, 4 PTWAM levels enabled
    // 22, 4 SDWAM levels enabled
    // 26, 1 0
    putbits36 (& words [2], 27,  3, cpu.switches.cpu_num);
    putbits36 (& words [2], 30,  6, cpu.cu.delta);

    // words [3]

    //  0, 18 0
    // 18, 4 TSNA pointer register number for non-EIS or EIS operand #1
    // 22, 4 TSNB pointer register number for EIS operand #2
    // 26, 4 TSNC pointer register number for EIS operand #3
    putbits36 (& words [3], 30, 6, cpu.TPR.TBR);

    // words [4]

    putbits36 (& words [4],  0, 18, cpu.PPR.IC);
    putbits36 (& words [4], 18, 18, cpu.cu.IR);

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
  //putbits36 (& words [4], 31, 1, 0);
//  putbits36 (& words [4], 31, 1, cpu.PPR.P ? 0 : 1);
//if(currentRunningCPUnum)
//sim_printf ("cleared ABS\n");
//}
#endif

    // words [5]

    putbits36 (& words [5],  0, 18, cpu.TPR.CA);
    putbits36 (& words [5], 18,  1, cpu.cu.repeat_first);
    putbits36 (& words [5], 19,  1, cpu.cu.rpt);
    putbits36 (& words [5], 20,  1, cpu.cu.rd);
    putbits36 (& words [5], 21,  1, cpu.cu.rl);
    putbits36 (& words [5], 22,  1, cpu.cu.pot);
    // 23, 1 PON Prepare operand no tally
    putbits36 (& words [5], 24,  1, cpu.cu.xde);
    putbits36 (& words [5], 25,  1, cpu.cu.xdo);
    // 26, 1 ITP Execute ITP indirect cycle
    putbits36 (& words [5], 27,  1, cpu.cu.rfi);
    // 28, 1 ITS Execute ITS indirect cycle
    putbits36 (& words [5], 29,  1, cpu.cu.FIF);
    putbits36 (& words [5], 30,  6, cpu.cu.CT_HOLD);

    // words [6]

    words [6] = cpu.cu.IWB;

    // words [7]

    words [7] = cpu.cu.IRODD;
  }


void cu_safe_store(void)
{
    // Save current Control Unit Data in hidden temporary so a later SCU
    // instruction running in FAULT mode can save the state as it existed at
    // the time of the fault rather than as it exists at the time the SCU
    //  instruction is executed.
    scu2words(cpu.scu_data);

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
    // BUG:  We don't track much of the data that should be tracked

    // words [0]

    cpu.PPR.PRR         = getbits36(words[0], 0, 3);
    cpu.PPR.PSR         = getbits36(words[0], 3, 15);
    cpu.PPR.P           = getbits36(words[0], 18, 1);
    // 19 XSF
    // 20 SDWAMM
    cpu.cu.SD_ON        = getbits36(words[0], 21, 1);
    // 22 PTWAMM
    cpu.cu.PT_ON        = getbits36(words[0], 23, 1);
#if 0
    cpu.cu.PI_AP        = getbits36(words[0], 24, 1);
    cpu.cu.DSPTW        = getbits36(words[0], 25, 1);
    cpu.cu.SDWNP        = getbits36(words[0], 26, 1);
    cpu.cu.SDWP         = getbits36(words[0], 27, 1);
    cpu.cu.PTW          = getbits36(words[0], 28, 1);
    cpu.cu.PTW2         = getbits36(words[0], 29, 1);
    cpu.cu.FAP          = getbits36(words[0], 30, 1);
    cpu.cu.FANP         = getbits36(words[0], 31, 1);
    cpu.cu.FABS         = getbits36(words[0], 32, 1);
#else
    cpu.cu.APUCycleBits = getbits36 (words [0], 24, 12);
#endif

    // words[1]

    cpu.cu.IRO_ISN      = getbits36 (words [1],  0,  1);
    cpu.cu.OEB_IOC      = getbits36 (words [1],  1,  1);
    cpu.cu.EOFF_IAIM    = getbits36 (words [1],  2,  1);
    cpu.cu.ORB_ISP      = getbits36 (words [1],  3,  1);
    cpu.cu.ROFF_IPR     = getbits36 (words [1],  4,  1);
    cpu.cu.OWB_NEA      = getbits36 (words [1],  5,  1);
    cpu.cu.WOFF_OOB     = getbits36 (words [1],  6,  1);
    cpu.cu.NO_GA        = getbits36 (words [1],  7,  1);
    cpu.cu.OCB          = getbits36 (words [1],  8,  1);
    cpu.cu.OCALL        = getbits36 (words [1],  9,  1);
    cpu.cu.BOC          = getbits36 (words [1], 10,  1);
    cpu.cu.PTWAM_ER     = getbits36 (words [1], 11,  1);
    cpu.cu.CRT          = getbits36 (words [1], 12,  1);
    cpu.cu.RALR         = getbits36 (words [1], 13,  1);
    cpu.cu.SWWAM_ER     = getbits36 (words [1], 14,  1);
    cpu.cu.OOSB         = getbits36 (words [1], 15,  1);
    cpu.cu.PARU         = getbits36 (words [1], 16,  1);
    cpu.cu.PARL         = getbits36 (words [1], 17,  1);
    cpu.cu.ONC1         = getbits36 (words [1], 18,  1);
    cpu.cu.ONC2         = getbits36 (words [1], 19,  1);
    cpu.cu.IA           = getbits36 (words [1], 20,  4);
    cpu.cu.IACHN        = getbits36 (words [1], 24,  3);
    cpu.cu.CNCHN        = getbits36 (words [1], 27,  3);
    cpu.cu.FI_ADDR      = getbits36 (words [1], 30,  5);
    cpu.cu.FLT_INT      = getbits36 (words [1], 35,  1);

    // words[2]

    cpu.TPR.TRR         = getbits36(words[2], 0, 3);
    cpu.TPR.TSR         = getbits36(words[2], 3, 15);
    // 18-21 PTW
    // 22-25 SDW
    // 26 0
    // 27-29 CPU number
    cpu.cu.delta        = getbits36(words[2], 30, 6);

    // words[3]

    // 0-17 0
    // 18-21 TSNA
    // 22-26 TSNB
    // 26-29 TSNC
    cpu.TPR.TBR         = getbits36(words[3], 30, 6);

    // words [4]

    cpu.cu.IR           = getbits36(words[4], 18, 18); // HWR
    cpu.PPR.IC          = getbits36(words[4], 0, 18);

    // words [5]

    cpu.TPR.CA          = getbits36(words[5], 0, 18);
    cpu.cu.repeat_first = getbits36(words[5], 18, 1);
    cpu.cu.rpt          = getbits36(words[5], 19, 1);
    cpu.cu.rd           = getbits36(words[5], 20, 1);
    cpu.cu.rl           = getbits36(words[5], 21, 1);
    cpu.cu.pot          = getbits36(words[5], 22, 1);
    // 23 PON
    cpu.cu.xde          = getbits36(words[5], 24, 1);
    cpu.cu.xdo          = getbits36(words[5], 25, 1);
    // 26 ITP
    cpu.cu.rfi          = getbits36(words[5], 27, 1);
    // 28 ITS
    cpu.cu.FIF          = getbits36(words[5], 29, 1);
    cpu.cu.CT_HOLD      = getbits36(words[5], 30, 6);

    // words [6]

    cpu.cu.IWB = words [6];

    // words [7]

    cpu.cu.IRODD = words [7];
}

void cu_safe_restore (void)
  {
    words2scu (cpu.scu_data);
  }

static void du2words (word36 * words)
  {

#ifdef ISOLTS
    for (int i = 0; i < 8; i ++)
      {
        words [i] = cpu.du.image [i];
//if (currentRunningCPUnum)
//sim_printf ("rest %d %012llo\n", i, words [i]);
      }
#else
    memset (words, 0, 8 * sizeof (* words));
#endif
    // Word 0

    putbits36 (& words [0],  9,  1, cpu.du.Z);
    putbits36 (& words [0], 10,  1, cpu.du.NOP);
    putbits36 (& words [0], 12, 24, cpu.du.CHTALLY);

    // Word 1

#ifdef ISOLTS
//if (words [1] == 0) putbits36 (& words [1], 35, 1, 1);
    words[1] = words[0];
    //words [1] ++;
#endif
    // Word 2

    putbits36 (& words [2],  0, 18, cpu.du.D1_PTR_W);
    putbits36 (& words [2], 18,  6, cpu.du.D1_PTR_B);
    putbits36 (& words [2], 25,  2, cpu.du.TAk [0]);
    putbits36 (& words [2], 31,  1, cpu.du.F1);
    putbits36 (& words [2], 32,  1, cpu.du.Ak [0]);

    // Word 3

    putbits36 (& words [3],  0, 10, cpu.du.LEVEL1);
    putbits36 (& words [3], 12, 24, cpu.du.D1_RES);

    // Word 4

    putbits36 (& words [4],  0, 18, cpu.du.D2_PTR_W);
    putbits36 (& words [4], 18,  6, cpu.du.D2_PTR_B);
    putbits36 (& words [4], 25,  2, cpu.du.TAk [1]);
    putbits36 (& words [4], 30,  1, cpu.du.R);
    putbits36 (& words [4], 31,  1, cpu.du.F2);
    putbits36 (& words [4], 32,  1, cpu.du.Ak [1]);

    // Word 5

    putbits36 (& words [5],  9,  1, cpu.du.LEVEL2);
    putbits36 (& words [5], 12, 24, cpu.du.D2_RES);

    // Word 6

    putbits36 (& words [6],  0, 18, cpu.du.D3_PTR_W);
    putbits36 (& words [6], 18,  6, cpu.du.D3_PTR_B);
    putbits36 (& words [6], 25,  2, cpu.du.TAk [2]);
    putbits36 (& words [6], 31,  1, cpu.du.F3);
    putbits36 (& words [6], 32,  1, cpu.du.Ak [2]);
    putbits36 (& words [6], 33,  3, cpu.du.JMP);

    // Word 7

    putbits36 (& words [7], 12, 24, cpu.du.D3_RES);

  }

static void words2du (word36 * words)
  {
    // Word 0

    cpu.du.Z        = getbits36 (words [0],  9,  1);
    cpu.du.NOP      = getbits36 (words [0], 10,  1);
    cpu.du.CHTALLY  = getbits36 (words [0], 12, 24);
    // Word 1

    // Word 2

    cpu.du.D1_PTR_W = getbits36 (words [2],  0, 18);
    cpu.du.D1_PTR_B = getbits36 (words [2], 18,  6);
    cpu.du.TAk [0]  = getbits36 (words [2], 25,  2);
    cpu.du.F1       = getbits36 (words [2], 31,  1);
    cpu.du.Ak [0]   = getbits36 (words [2], 32,  1);

    // Word 3

    cpu.du.LEVEL1   = getbits36 (words [3],  0, 10);
    cpu.du.D1_RES   = getbits36 (words [3], 12, 24);

    // Word 4

    cpu.du.D2_PTR_W = getbits36 (words [4],  0, 18);
    cpu.du.D2_PTR_B = getbits36 (words [4], 18,  6);
    cpu.du.TAk [1]  = getbits36 (words [4], 25,  2);
    cpu.du.F2       = getbits36 (words [4], 31,  1);
    cpu.du.Ak [1]   = getbits36 (words [4], 32,  1);

    // Word 5

    cpu.du.LEVEL2   = getbits36 (words [5],  9,  1);
    cpu.du.D2_RES   = getbits36 (words [5], 12, 24);

    // Word 6

    cpu.du.D3_PTR_W = getbits36 (words [6],  0, 18);
    cpu.du.D3_PTR_B = getbits36 (words [6], 18,  6);
    cpu.du.TAk [2]  = getbits36 (words [6], 25,  2);
    cpu.du.F3       = getbits36 (words [6], 31,  1);
    cpu.du.Ak [2]   = getbits36 (words [6], 32,  1);
    cpu.du.JMP      = getbits36 (words [6], 33,  3);

    // Word 7

    cpu.du.D3_RES   = getbits36 (words [7], 12, 24);

#ifdef ISOLTS
    for (int i = 0; i < 8; i ++)
      {
        cpu.du.image [i] = words [i];
//if (currentRunningCPUnum)
//sim_printf ("save %d %012llo\n", i, words [i]);
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

static long long theMatrix [1024] // 1024 opcodes (2^10)
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
    theMatrix [_opcode] [_opcodeX] [_a] [_tag] ++;
}

t_stat displayTheMatrix (UNUSED int32 arg, UNUSED char * buf)
{
    long long count;
    for (int opcode = 0; opcode < 01000; opcode ++)
    for (int opcodeX = 0; opcodeX < 2; opcodeX ++)
    for (int a = 0; a < 2; a ++)
    for (int tag = 0; tag < 64; tag ++)
    if ((count = theMatrix [opcode] [opcodeX] [a] [tag]))
    {
        // disAssemble doesn't quite do what we want so copy the good bits
        static char result[132] = "???";
        strcpy(result, "???");
        // get mnemonic ...
        // non-EIS first
        if (!opcodeX)
        {
            if (NonEISopcodes[opcode].mne)
                strcpy(result, NonEISopcodes[opcode].mne);
        }
        else
        {
            // EIS second...
            if (EISopcodes[opcode].mne)
                strcpy(result, EISopcodes[opcode].mne);

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
            strcat(result, ",");
            strcat(result, extMods[tag].mod);
        }
        if (result [0] == '?')
            sim_printf ("%20lld: ? opcode 0%04o X %d a %d tag 0%02do\n",
                        count, opcode, opcodeX, a, tag);
        else
            sim_printf ("%20lld: %s\n", count, result);
    }
    return SCPE_OK;
}


// fetch instrcution at address
// CANFAULT
void fetchInstruction (word18 addr)
{
    DCDstruct * p = & cpu.currentInstruction;

    memset (p, 0, sizeof (struct DCDstruct));

    // since the next memory cycle will be a instruction fetch setup TPR
    cpu.TPR.TRR = cpu.PPR.PRR;
    cpu.TPR.TSR = cpu.PPR.PSR;

    //if (get_addr_mode() == ABSOLUTE_mode || get_addr_mode () == BAR_mode)
    if (get_addr_mode() == ABSOLUTE_mode)
      {
        cpu.TPR.TRR = 0;
        cpu.RSDWH_R1 = 0;
      }

    if (cpu.cu.rd && ((cpu.PPR.IC & 1) != 0))
      {
        if (cpu.cu.repeat_first)
          Read(addr, & cpu.cu.IRODD, INSTRUCTION_FETCH, 0);
      }
    else if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        if (cpu.cu.repeat_first)
          Read(addr, & cpu.cu.IWB, INSTRUCTION_FETCH, 0);
      }
    else
      {
        Read(addr, & cpu.cu.IWB, INSTRUCTION_FETCH, 0);
#ifdef ISOLTS
// ISOLTS test pa870 expects IRODD to be set up.
// If we are fetching an even instruction, also fetch the odd.
// If we are fetching an odd instruction, copy it to IRODD as
// if that was where we got it from.
        if ((cpu.PPR.IC & 1) == 0) // Even
          Read(addr+1, & cpu.cu.IRODD, INSTRUCTION_FETCH, 0);
        else
          cpu.cu.IRODD = cpu.cu.IWB;
#endif
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
            if (get_addr_mode() == ABSOLUTE_mode)
              {
                if (isBAR)
                  {
                    sim_debug(flag, &cpu_dev, "%06o|%06o %s\n",
                              cpu.BAR.BASE, cpu.PPR.IC, where);
                  }
                else
                  {
                    sim_debug(flag, &cpu_dev, "%06o %s\n", cpu.PPR.IC, where);
                  }
              }
            else if (get_addr_mode() == APPEND_mode)
              {
                if (isBAR)
                  {
                    sim_debug(flag, &cpu_dev, "%05o:%06o|%06o %s\n", cpu.PPR.PSR,
                              cpu.BAR.BASE, cpu.PPR.IC, where);
                  }
                else
                  {
                    sim_debug(flag, &cpu_dev, "%05o:%06o %s\n",
                              cpu.PPR.PSR, cpu.PPR.IC, where);
                  }
              }
            listSource (compname, compoffset, flag);
          }
        if (get_addr_mode() == ABSOLUTE_mode)
          {
            if (isBAR)
              {
                sim_debug(flag, &cpu_dev,
                  "%05o|%06o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.BAR.BASE,
                  cpu.PPR.IC,
                  IWB_IRODD,
                  disAssemble(IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.a,
                  cpu.currentInstruction.i,
                  GET_TM(cpu.currentInstruction.tag) >> 4,
                  GET_TD(cpu.currentInstruction.tag) & 017);
              }
            else
              {
                sim_debug(flag, &cpu_dev,
                  "%06o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.PPR.IC,
                  IWB_IRODD,
                  disAssemble (IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.a,
                  cpu.currentInstruction.i,
                  GET_TM(cpu.currentInstruction.tag) >> 4,
                  GET_TD(cpu.currentInstruction.tag) & 017);
              }
          }
        else if (get_addr_mode() == APPEND_mode)
          {
            if (isBAR)
              {
                sim_debug(flag, &cpu_dev,
                 "%05o:%06o|%06o %o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.PPR.PSR,
                  cpu.BAR.BASE,
                  cpu.PPR.IC,
                  cpu.PPR.PRR,
                  IWB_IRODD,
                  disAssemble(IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.a, cpu.currentInstruction.i,
                  GET_TM(cpu.currentInstruction.tag) >> 4,
                  GET_TD(cpu.currentInstruction.tag) & 017);
              }
            else
              {
                sim_debug(flag, &cpu_dev,
                  "%05o:%06o %o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n",
                  cpu.PPR.PSR,
                  cpu.PPR.IC,
                  cpu.PPR.PRR,
                  IWB_IRODD,
                  disAssemble(IWB_IRODD),
                  cpu.currentInstruction.address,
                  cpu.currentInstruction.opcode,
                  cpu.currentInstruction.opcodeX,
                  cpu.currentInstruction.a,
                  cpu.currentInstruction.i,
                  GET_TM(cpu.currentInstruction.tag) >> 4,
                  GET_TD(cpu.currentInstruction.tag) & 017);
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
        if ((cpu.rX [0] & 00001) == 0)
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
        if ((cpu.rX [0] & 00001) == 0)
          return false;
      }
    return true;
  }

t_stat executeInstruction (void)
  {

//
// Decode the instruction
//
// If not restart
//     check for priviledge
//     chcck for illegal modifiers
//     if rpt/rpd
//         check for illegal rpt/rpd modifiers
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
    decodeInstruction(IWB_IRODD, ci);

    const opCode *info = ci->info;       // opCode *
    const uint32  opcode = ci->opcode;   // opcode
    const bool   opcodeX = ci->opcodeX;  // opcode extension
    const word18 address = ci->address;  // bits 0-17 of instruction
                                         // XXX replace with rY
    const bool   a = ci->a;              // bit-29 - addressing via pointer
                                         // register
    const word6  tag = ci->tag;          // instruction tag
                                         //  XXX replace withrTAG


    addToTheMatrix (opcode, opcodeX, a, tag);

///
/// executeInstruction: Non-restart processing
///

    if (ci -> restart)
      goto restart_1;

    // Reset the fault counter
    cpu.cu.APUCycleBits &= 07770;

    // check for priv ins - Attempted execution in normal or BAR modes causes a
    // illegal procedure fault.
    // XXX Not clear what the subfault should be; see Fault Register in AL39.
    if ((ci -> info -> flags & PRIV_INS) && ! is_priv_mode ())
        doFault (FAULT_IPR, flt_ipr_ill_proc,
                 "Attempted execution of privileged instruction.");

    ///
    /// executeInstruction: Non-restart processing
    ///                     check for illegal addressing mode(s) ...
    ///

    // No CI/SC/SCR allowed
    if (ci->info->mods == NO_CSS)
    {
        if (_nocss[ci->tag])
            doFault(FAULT_IPR, flt_ipr_ill_mod, "Illegal CI/SC/SCR modification");
    }
    // No DU/DL/CI/SC/SCR allowed
    else if (ci->info->mods == NO_DDCSS)
    {
        if (_noddcss[ci->tag])
            doFault(FAULT_IPR, flt_ipr_ill_mod, "Illegal DU/DL/CI/SC/SCR modification");
    }
    // No DL/CI/SC/SCR allowed
    else if (ci->info->mods == NO_DLCSS)
    {
        if (_nodlcss[ci->tag])
            doFault(FAULT_IPR, flt_ipr_ill_mod, "Illegal DL/CI/SC/SCR modification");
    }
    // No DU/DL allowed
    else if (ci->info->mods == NO_DUDL)
    {
        if (_nodudl[ci->tag])
            doFault(FAULT_IPR, flt_ipr_ill_mod, "Illegal DU/DL modification");
    }

    // If executing the target of XEC/XED, check the instruction is allowed
    if (cpu.isXED)
    {
        if (ci->info->flags & NO_XED)
            doFault(FAULT_IPR, flt_ipr_ill_proc, "Instruction not allowed in XEC/XED");
    }

    // Instruction not allowed in RPx?

    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        if (ci->info->flags & NO_RPT)
          doFault(FAULT_IPR, flt_ipr_ill_proc, "no rpx allowed for instruction");
      }

    // RPT/RPD illegal modifiers
    // a:AL39/rpd3
    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
        // check for illegal modifiers:
        //    only R & RI are allowed
        //    only X1..X7
        switch (GET_TM(ci->tag))
          {
            case TM_RI:
              if (cpu.cu.rl)
                doFault(FAULT_IPR, flt_ipr_ill_mod, "ill addr mod from RPL");
              break;
            case TM_R:
              break;
            default:
              // generate fault. Only R & RI allowed
              doFault(FAULT_IPR, flt_ipr_ill_mod,
                      "ill addr mod from RPT/RPD/RPL");
          }
        word6 Td = GET_TD(ci->tag);
#if 0
        switch (Td)
          {
            //case TD_X0: Only X1-X7 permitted
            case TD_X1:
            case TD_X2:
            case TD_X3:
            case TD_X4:
            case TD_X5:
            case TD_X6:
            case TD_X7:
              break;
            default:
              // generate fault. Only Xn allowed
              doFault(FAULT_IPR, flt_ipr_ill_mod, "ill addr mod from RPT");
          }
#else
        if (Td == TD_X0)
          doFault(FAULT_IPR, flt_ipr_ill_mod, "ill addr mod from RPT");
        if (! cpu.cu.rd && Td < TD_X0)
          doFault(FAULT_IPR, flt_ipr_ill_mod, "ill addr mod from RPT/RPL");
#endif
      }

    ///
    /// executeInstruction: Non-restart processing
    ///                     Initialize address registers
    ///

    cpu.TPR.CA = address;
    cpu.iefpFinalAddress = cpu.TPR.CA;
    cpu.rY = cpu.TPR.CA;


restart_1:

///
/// executeInstruction: Initialize state saving registers
///

    // XXX this may be wrong; make sure that the right value is used
    // if a page fault occurs. (i.e. this may belong above restart_1.
    ci->stiTally = TST_I_TALLY;   // for sti instruction

///
/// executeInstruction: simh hooks
///

    // XXX Don't trace Multics idle loop
    if (cpu.PPR.PSR != 061 || cpu.PPR.IC != 0307)

      {
        traceInstruction (DBG_TRACE);
#ifdef HDBG
        hdbgTrace ();
#endif
      }

///
/// executeInstruction: Initialize TPR
///

    // This must not happen on instruction restart
    if (! ci -> restart)
      {
        if (! ci -> a)
          {
            cpu.TPR.TRR = cpu.PPR.PRR;
            cpu.TPR.TSR = cpu.PPR.PSR;
          }
      }

    cpu.du.JMP = info -> ndes;

    cpu.dlyFlt = false;

///
/// executeInstruction: RPT/RPD/RPL special processing for 'first time'
///

    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
      {
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
//      if C(Y)0,17 ≠ 0, then C (y)0,17 -> C(Xn);
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
                   cpu.rX [0], !! (cpu.rX [0] & 01000), !! (cpu.rX [0] & 0400));
        sim_debug (DBG_TRACE, & cpu_dev,
                   "RPT/RPD CA %06o\n", cpu.TPR.CA);

// Handle first time of a RPT or RPD

        if (cpu.cu.repeat_first)
          {
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
                word18 offset;
                if (ci -> a)
                  {
                    // a:RJ78/rpd4
                    offset = SIGNEXT15_18 (ci -> address & MASK15);
                  }
                else
                  offset = ci -> address;
                offset &= AMASK;

                sim_debug (DBG_TRACE, & cpu_dev,
                           "rpt/rd/rl repeat first; offset is %06o\n", offset);

                word6 Td = GET_TD (ci -> tag);
                uint Xn = X (Td);  // Get Xn of next instruction
                sim_debug (DBG_TRACE, & cpu_dev,
                           "rpt/rd/rl repeat first; X%d was %06o\n",
                           Xn, cpu.rX [Xn]);
                // a:RJ78/rpd5
                cpu.rX [Xn] = (cpu.rX [Xn] + offset) & AMASK;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "rpt/rd/rl repeat first; X%d now %06o\n",
                           Xn, cpu.rX [Xn]);
              } // rpt or rd or rl

          } // repeat first
      } // cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl

///
/// executeInstruction: EIS operand processing
///

    if (info -> ndes > 0)
      {
        // This must not happen on instruction restart
        if (! ci -> restart)
          {
            cpu.du.CHTALLY = 0;
            cpu.du.Z = 1;
          }
        for(int n = 0; n < info -> ndes; n += 1)
          {
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
            Read (cpu.PPR.IC + 1 + n, & cpu.currentEISinstruction.op [n],
                  EIS_OPERAND_READ, 0); // I think.
          }
        setupEISoperands ();
      }
    else

///
/// executeInstruction: non-EIS operand processing
///

      {
        // This must not happen on instruction restart
        if (! ci -> restart)
          {
            if (ci -> a)   // if A bit set set-up TPR stuff ...
              {
                doPtrReg ();

// Putting the a29 clear here makes sense, but breaks the emulator for unclear
// reasons (possibly ABSA?). Do it in updateIWB instead
//                ci -> a = false;
//                // Don't clear a; it is needed to detect change to appending
//                //  mode
//                //a = false;
//                putbits36 (& cpu.cu.IWB, 29,  1, 0);
              }
            else
              {
                cpu.TPR.TBR = 0;
                if (get_addr_mode() == ABSOLUTE_mode)
                  {
                    cpu.TPR.TSR = cpu.PPR.PSR;
                    cpu.TPR.TRR = 0;
                    cpu.RSDWH_R1 = 0;
                  }
                clr_went_appending ();
              }
          }

        // This must not happen on instruction restart
        if (! ci -> restart)
          {
            cpu.cu.CT_HOLD = 0; // Clear interrupted IR mode flag
          }


        //
        // If POT is set, a page fault occured during the fetch of the data word
        // pointed to by an indirect addressing word, and the saved CA points
        // to the data word instead of the indirect word; reset the CA correctly
        //

        if (ci -> restart && cpu.cu.pot)
          {
            cpu.TPR.CA = GET_ADDR (IWB_IRODD);
            if (getbits36 (cpu.cu.IWB, 29, 1) != 0)
              cpu.TPR.CA &= MASK15;
          }



        if (ci->info->flags & PREPARE_CA)
          {
            doComputedAddressFormation ();
            cpu.iefpFinalAddress = cpu.TPR.CA;
          }
        else if (READOP (ci))
          {
            doComputedAddressFormation ();
            cpu.iefpFinalAddress = cpu.TPR.CA;
            readOperands ();
          }
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
        if (! READOP (ci))
          {
            doComputedAddressFormation ();
            cpu.iefpFinalAddress = cpu.TPR.CA;
          }
        writeOperands ();
      }

///
/// executeInstruction: Update IT tally
///

    word6 rTAG;
    if (ci -> info -> flags & NO_TAG) // for instructions line STCA/STCQ
      rTAG = 0;
    else
      rTAG = GET_TAG (cpu.cu.IWB);

    word6 Tm = GET_TM (rTAG);
    word6 Td = GET_TD (rTAG);

    if (info->ndes == 0 /* non-EIS */ &&
        (! (ci -> info -> flags & NO_TAG)) &&
        Tm == TM_IT && (Td == IT_SC || Td == IT_SCR))
      {
        //
        // Get the indirect word
        //

        word36 indword;
        Read (cpu.TPR.CA, & indword, OPERAND_READ, ci -> a);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "update IT indword=%012llo\n", indword);

        //
        // Parse and validate the indirect word
        //

        word18 Yi = GET_ADDR (indword);
        word6 characterOperandSize = GET_TB (GET_TAG (indword));
        word6 characterOperandOffset = GET_CF (GET_TAG (indword));

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "update IT size=%o offset=%o Yi=%06o\n",
                   characterOperandSize, characterOperandOffset,
                   Yi);

        word12 tally = GET_TALLY (indword);    // 12-bits

        if (Td == IT_SCR)
          {
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

            if (characterOperandOffset == 0)
              {
                if (characterOperandSize == TB6)
                    characterOperandOffset = 5;
                else
                    characterOperandOffset = 3;
                Yi -= 1;
                Yi &= MASK18;
              }
                else
              {
                characterOperandOffset -= 1;
              }
            tally ++;
            tally &= 07777; // keep to 12-bits
          }
        else // SC
          {
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

            characterOperandOffset ++;

            if (((characterOperandSize == TB6) &&
                 (characterOperandOffset > 5)) ||
                ((characterOperandSize == TB9) &&
                 (characterOperandOffset > 3)))
              {
                characterOperandOffset = 0;
                Yi += 1;
                Yi &= MASK18;
              }
            tally --;
            tally &= 07777; // keep to 12-bits
          }


        sim_debug (DBG_ADDRMOD, & cpu_dev,
                       "update IT tally now %o\n", tally);

        SC_I_TALLY (tally == 0);

        indword = (word36) (((word36) Yi << 18) |
                            (((word36) tally & 07777) << 6) |
                            characterOperandSize |
                            characterOperandOffset);

        Write (cpu.TPR.CA, indword, INDIRECT_WORD_FETCH, ci -> a);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "update IT wrote tally word %012llo to %06o\n",
                   indword, cpu.TPR.CA);
      } // SC/SCR

///
/// executeInstruction: Delayed overflow fault
///

    if (cpu.dlyFlt)
      {
        doFault (cpu.dlyFltNum, cpu.dlySubFltNum, cpu.dlyCtx);
      }

///
/// executeInstruction: XEC/XED processing
///

// Delay updating IWB/IRODD until after operand processing.

    if (cpu.cu.xde && cpu.cu.xdo)
      {
        cpu.cu.IWB = cpu.Ypair [0];
        cpu.cu.IRODD = cpu.Ypair [1];
if (currentRunningCPUnum)
sim_printf ("xed %012llo %012llo\n", cpu.cu.IWB, cpu.cu.IRODD);
      }
    else if (cpu.cu.xde)
      {
        cpu.cu.IWB = cpu.CY;
      }

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
        // If we get here, the instruction just executed was a
        // RPT, RPD or RPL target instruction, and not the RPT or RPD
        // instruction itself

        if (cpu.cu.rpt || cpu.cu.rd)
          {
            // Add delta to index register.

            bool rptA = !! (cpu.rX [0] & 01000);
            bool rptB = !! (cpu.rX [0] & 00400);

            sim_debug (DBG_TRACE, & cpu_dev,
                "RPT/RPD delta first %d rf %d rpt %d rd %d "
                "e/o %d X0 %06o a %d b %d\n",
                cpu.cu.repeat_first, rf, cpu.cu.rpt, cpu.cu.rd, icOdd,
                cpu.rX [0], rptA, rptB);

            if (cpu.cu.rpt) // rpt
              {
                uint Xn = getbits36 (cpu.cu.IWB, 36 - 3, 3);
                cpu.rX[Xn] = (cpu.rX[Xn] + cpu.cu.delta) & AMASK;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "RPT/RPD delta; X%d now %06o\n", Xn, cpu.rX [Xn]);
              }


            // a:RJ78/rpd6
            // We know that the X register is not to be incremented until
            // after both instructions have executed, so the following
            // if uses icOdd instead of the more sensical icEven.
            if (cpu.cu.rd && icOdd && rptA) // rpd, even instruction
              {
                // a:RJ78/rpd7
                uint Xn = getbits36 (cpu.cu.IWB, 36 - 3, 3);
                cpu.rX[Xn] = (cpu.rX[Xn] + cpu.cu.delta) & AMASK;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "RPT/RPD delta; X%d now %06o\n", Xn, cpu.rX [Xn]);
              }

            if (cpu.cu.rd && icOdd && rptB) // rpdb, odd instruction
              {
                // a:RJ78/rpd8
                uint Xn = getbits36 (cpu.cu.IRODD, 36 - 3, 3);
                cpu.rX[Xn] = (cpu.rX[Xn] + cpu.cu.delta) & AMASK;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "RPT/RPD delta; X%d now %06o\n", Xn, cpu.rX [Xn]);
              }
          } // rpt || rd

        else if (cpu.cu.rl)
          {
            // C(Xn) -> y
            uint Xn = getbits36 (cpu.cu.IWB, 36 - 3, 3);
            putbits36 (& cpu . cu  . IWB,  0, 18, cpu.rX[Xn]);
          }

        // Check for termination conditions.

        if (cpu.cu.rpt || (cpu.cu.rd && icOdd) || cpu.cu.rl)
          {
            bool exit = false;
            // The repetition cycle consists of the following steps:
            //  a. Execute the repeated instruction
            //  b. C(X0)0,7 - 1 -> C(X0)0,7
            // a:AL39/rpd9
            uint x = getbits18 (cpu.rX [0], 0, 8);
            x -= 1;
            x &= MASK8;
            putbits18 (& cpu.rX [0], 0, 8, x);

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
                cpu.cu.rpt = false;
                cpu.cu.rd = false;
                cpu.cu.rl = false;
              }
            else
              {
                sim_debug (DBG_TRACE, & cpu_dev, "not terminate\n");
              }
          } // if (cpu.cu.rpt || cpu.cu.rd & (cpu.PPR.IC & 1))
      } // (! rf) && (cpu.cu.rpt || cpu.cu.rd)

///
/// executeInstruction: simh hooks
///

    sys_stats.total_cycles += 1; // bump cycle counter

    if_sim_debug (DBG_REGDUMP, & cpu_dev)
    {
        sim_debug(DBG_REGDUMPAQI, &cpu_dev,
                  "A=%012llo Q=%012llo IR:%s\n", cpu.rA, cpu.rQ, dumpFlags(cpu.cu.IR));

        sim_debug(DBG_REGDUMPFLT, &cpu_dev,
                  "E=%03o A=%012llo Q=%012llo %.10Lg\n",
                  cpu.rE, cpu.rA, cpu.rQ, EAQToIEEElongdouble());

        sim_debug(DBG_REGDUMPIDX, &cpu_dev,
                  "X[0]=%06o X[1]=%06o X[2]=%06o X[3]=%06o\n",
                  cpu.rX[0], cpu.rX[1], cpu.rX[2], cpu.rX[3]);
        sim_debug(DBG_REGDUMPIDX, &cpu_dev,
                  "X[4]=%06o X[5]=%06o X[6]=%06o X[7]=%06o\n",
                  cpu.rX[4], cpu.rX[5], cpu.rX[6], cpu.rX[7]);
        for(int n = 0 ; n < 8 ; n++)
        {
            sim_debug(DBG_REGDUMPPR, &cpu_dev,
                      "PR%d/%s: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\n",
                      n, PRalias[n], cpu.PR[n].SNR, cpu.PR[n].RNR, cpu.PR[n].WORDNO, 
                      cpu.PR[n].BITNO);
        }
        for(int n = 0 ; n < 8 ; n++)
            sim_debug(DBG_REGDUMPADR, &cpu_dev,
                  "AR%d: WORDNO=%06o CHAR:%o BITNO:%02o\n",
                  n, cpu.AR[n].WORDNO, GET_AR_CHAR (n), GET_AR_BITNO (n));
        sim_debug(DBG_REGDUMPPPR, &cpu_dev,
                  "PRR:%o PSR:%05o P:%o IC:%06o\n",
                  cpu.PPR.PRR, cpu.PPR.PSR, cpu.PPR.P, cpu.PPR.IC);
        sim_debug(DBG_REGDUMPDSBR, &cpu_dev,
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
    // If an overflow occured and the repeat instruction is not inhibiting
    // overflow checking.
    if (ovf && chkOVF ())
      {
        SET_I_OFLOW;
        // If overflows are not masked
        if (tstOVFfault ())
          {
            // ISOLTS test ps768: Overflows set TRO.
            if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
              {
                SET_I_TALLY;
              }
            if (dly)
              dlyDoFault (FAULT_OFL, 0, msg);
            else
              doFault (FAULT_OFL, 0, msg);
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
    if (i -> info -> ndes > 0)
      CLR_I_MIF;

    // Simple CU history hack
    addCUhist (0, cpu.cu.IWB & MASK18, cpu.iefpFinalAddress, 0, CUH_XINT);

    return i->opcodeX ? DoEISInstruction () : DoBasicInstruction ();
}

// CANFAULT
static t_stat DoBasicInstruction (void)
{
    DCDstruct * i = & cpu.currentInstruction;
    uint opcode  = i->opcode;  // get opcode

    switch (opcode)
    {
        ///    FIXED-POINT ARITHMETIC INSTRUCTIONS

        /// Fixed-Point Data Movement Load

        case 0635:  // eaa
            cpu.rA = 0;
            SETHI(cpu.rA, cpu.TPR.CA);

            SC_I_ZERO (cpu.TPR.CA == 0);
            SC_I_NEG (cpu.TPR.CA & SIGN18);

            break;

        case 0636:  // eaq
            cpu.rQ = 0;
            SETHI(cpu.rQ, cpu.TPR.CA);

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
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rA = compl36 (cpu.CY, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "lca overflow fault");
              //}
            overflow (ovf, false, "lca overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = compl36 (cpu.CY, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "lca overflow fault");
              }
            cpu.rA = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0336:  // lcq
          {
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rQ = compl36 (cpu.CY, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "lcq overflow fault");
              //}
            overflow (ovf, false, "lcq overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = compl36 (cpu.CY, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "lcq overflow fault");
              }
            cpu.rQ = tmp;
            cpu.cu.IR = ir;
#endif
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
            // XXX ToDo: Attempted repetition with the rpl instruction and with
            // the same register given as target and modifier causes an illegal
            // procedure fault.

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX [n] = compl18 (GETHI (cpu.CY), & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "lcxn overflow fault");
              //}
            overflow (ovf, false, "lcxn overflow fault");
#else
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 ir = cpu.cu.IR;
            word18 tmp = compl18 (GETHI (cpu.CY), & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "lcxn overflow fault");
              }
            cpu.rX [n] = tmp;
            cpu.cu.IR = ir;
#endif
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
#ifdef OVERFLOW_WRITE_THROUGH
                cpu.rA = cpu.Ypair[0];
                cpu.rQ = cpu.Ypair[1];
                SET_I_NEG;
                CLR_I_ZERO;
#endif
                //if (tstOVFfault ())
                  //{
                    //SET_I_OFLOW;
                    //doFault(FAULT_OFL, 0, "lcaq overflow fault");
                  //}
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
                word72 tmp72 = 0;

                tmp72 = bitfieldInsert72(tmp72, cpu.Ypair[0] & MASK36, 36, 36);
                tmp72 = bitfieldInsert72(tmp72, cpu.Ypair[1] & MASK36,  0, 36);

                tmp72 = ~tmp72 + 1;

                //cpu.rA = bitfieldExtract72(tmp72, 36, 36);
                //cpu.rQ = bitfieldExtract72(tmp72,  0, 36);
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
            word18 tmp18 = GETLO (cpu.CY) & 0777760;

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
            SC_I_HEX  (tmp18 & I_HEX);

            if (bAbsPriv)
              {
                SC_I_PMASK (tmp18 & I_PMASK);
                SC_I_MIF (tmp18 & I_MIF);
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
                cpu.rX[n] = GETHI(cpu.CY);
                SC_I_ZERO (cpu.rX[n] == 0);
                SC_I_NEG (cpu.rX[n] & SIGN18);
            }
            break;

        case 0073:   // lreg

            cpu.rX[0] = GETHI(cpu.Yblock8[0]);
            cpu.rX[1] = GETLO(cpu.Yblock8[0]);
            cpu.rX[2] = GETHI(cpu.Yblock8[1]);
            cpu.rX[3] = GETLO(cpu.Yblock8[1]);
            cpu.rX[4] = GETHI(cpu.Yblock8[2]);
            cpu.rX[5] = GETLO(cpu.Yblock8[2]);
            cpu.rX[6] = GETHI(cpu.Yblock8[3]);
            cpu.rX[7] = GETLO(cpu.Yblock8[3]);
            cpu.rA = cpu.Yblock8[4];
            cpu.rQ = cpu.Yblock8[5];
            cpu.rE = (GETHI(cpu.Yblock8[6]) >> 10) & 0377;   // need checking

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
                cpu.rX[n] = GETLO(cpu.CY);
                SC_I_ZERO (cpu.rX[n] == 0);
                SC_I_NEG (cpu.rX[n] & SIGN18);
            }
            break;

        /// Fixed-Point Data Movement Store

        case 0753:  // sreg
            // clear block (changed to memset() per DJ request)
            memset(cpu.Yblock8, 0, sizeof(cpu.Yblock8));

            SETHI(cpu.Yblock8[0], cpu.rX[0]);
            SETLO(cpu.Yblock8[0], cpu.rX[1]);
            SETHI(cpu.Yblock8[1], cpu.rX[2]);
            SETLO(cpu.Yblock8[1], cpu.rX[3]);
            SETHI(cpu.Yblock8[2], cpu.rX[4]);
            SETLO(cpu.Yblock8[2], cpu.rX[5]);
            SETHI(cpu.Yblock8[3], cpu.rX[6]);
            SETLO(cpu.Yblock8[3], cpu.rX[7]);
            cpu.Yblock8[4] = cpu.rA;
            cpu.Yblock8[5] = cpu.rQ;
            cpu.Yblock8[6] = SETHI(cpu.Yblock8[7], (word18)cpu.rE << 10);
#ifdef REAL_TR
            cpu.Yblock8[7] = ((getTR (NULL) & MASK27) << 9) | (cpu.rRALR & 07);
#else
            cpu.Yblock8[7] = ((cpu.rTR & MASK27) << 9) | (cpu.rRALR & 07);
#endif
            break;

        case 0755:  // sta
            cpu.CY = cpu.rA;
            break;

        case 0354:  // stac
            if (cpu.CY == 0)
            {
                SET_I_ZERO;
                cpu.CY = cpu.rA;
            } else
                CLR_I_ZERO;
            break;

        case 0654:  // stacq
            if (cpu.CY == cpu.rQ)
            {
                cpu.CY = cpu.rA;
                SET_I_ZERO;

            } else
                CLR_I_ZERO;
            break;

        case 0757:  // staq
            cpu.Ypair[0] = cpu.rA;
            cpu.Ypair[1] = cpu.rQ;

            break;

        case 0551:  // stba
            // 9-bit bytes of C(A) -> corresponding bytes of C(Y), the byte
            // positions affected being specified in the TAG field.
            copyBytes((i->tag >> 2) & 0xf, cpu.rA, &cpu.CY);
            break;

        case 0552:  // stbq
            // 9-bit bytes of C(Q) -> corresponding bytes of C(Y), the byte
            // positions affected being specified in the TAG field.
            copyBytes((i->tag >> 2) & 0xf, cpu.rQ, &cpu.CY);

            break;

        case 0554:  // stc1
            // "C(Y)25 reflects the state of the tally runout indicator
            // prior to modification.
            SETHI(cpu.CY, (cpu.PPR.IC + 1) & MASK18);
            SETLO(cpu.CY, cpu.cu.IR & 0777760);
            SCF (i -> stiTally, cpu.CY, I_TALLY);
            break;

        case 0750:  // stc2
            // AL-39 doesn't specify if the low half is set to zero,
            // set to IR, or left unchanged
            // RJ78 specifies unchanged
            SETHI(cpu.CY, (cpu.PPR.IC + 2) & MASK18);

            break;

        case 0751: // stca
            // Characters of C(A) -> corresponding characters of C(Y),
            // the character positions affected being specified in the TAG
            // field.
            copyChars(i->tag, cpu.rA, &cpu.CY);

            break;

        case 0752: // stcq
            // Characters of C(Q) -> corresponding characters of C(Y), the
            // character positions affected being specified in the TAG field.
            copyChars(i->tag, cpu.rQ, &cpu.CY);
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

            cpu.Ypair[0] = 0;
            //cpu.Ypair[0] = bitfieldInsert36(cpu.Ypair[0], cpu.PPR.PSR, 18, 15);
            //cpu.Ypair[0] = bitfieldInsert36(cpu.Ypair[0], cpu.PPR.PRR, 15,  3);
            //cpu.Ypair[0] = bitfieldInsert36(cpu.Ypair[0],     043,  0,  6);
            putbits36 (& cpu.Ypair [0],  3, 15, cpu.PPR.PSR);
            putbits36 (& cpu.Ypair [0], 18,  3, cpu.PPR.PRR);
            putbits36 (& cpu.Ypair [0], 30,  6,     043);

            cpu.Ypair[1] = 0;
            //cpu.Ypair[1] = bitfieldInsert36(cpu.Ypair[0], cpu.PPR.IC + 2, 18, 18);
            putbits36(& cpu.Ypair [1],  0, 18, cpu.PPR.IC + 2);

            break;


        case 0754: // sti

            // C(IR) -> C(Y)18,31
            // 00...0 -> C(Y)32,35

            // The contents of the indicator register after address
            // preparation are stored in C(Y)18,31  C(Y)18,31 reflects the
            // state of the tally runout indicator prior to address
            // preparation. The relation between C(Y)18,31 and the indicators
            // is given in Table 4-5.

            SETLO(cpu.CY, (cpu.cu.IR & 0000000777760LL));
            SCF (i -> stiTally, cpu.CY, I_TALLY);
            if (cpu.switches.invert_absolute)
              cpu.CY ^= 020;
            break;

        case 0756: // stq
            cpu.CY = cpu.rQ;
            break;

        case 0454:  // stt
#ifdef REAL_TR
             cpu.CY = (getTR (NULL) & MASK27) << 9;
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
                SETHI(cpu.CY, cpu.rX[n]);
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
            {
                uint32 n = opcode & 07;  // get n

                SETLO(cpu.CY, cpu.rX[n]);
            }
            break;

        /// Fixed-Point Data Movement Shift

        case 0775:  // alr
            {
                word36 tmp36 = cpu.TPR.CA & 0177;   // CY bits 11-17
                for(uint i = 0 ; i < tmp36 ; i++)
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

                for (uint i = 0; i < tmp36; i ++)
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
            for (uint i = 0 ; i < tmp18 ; i ++)
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
                for(uint i = 0 ; i < tmp36 ; i++)
                {
                    bool a0 = cpu.rA & SIGN36;    // A0

                    cpu.rA <<= 1;               // shift left 1

                    bool b0 = cpu.rQ & SIGN36;    // Q0
                    if (b0)
                        cpu.rA |= 1;            // Q0 => A35

                    cpu.rQ <<= 1;               // shift left 1

                    if (a0)                 // propagate A sign bit
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
            for(uint i = 0 ; i < tmp36 ; i ++)
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
                for(uint i = 0 ; i < tmp36 ; i++)
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

            for (uint i = 0 ; i < tmp36 ; i ++)
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
                for(uint i = 0 ; i < tmp36 ; i++)
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

                for (uint i = 0; i < tmp36; i ++)
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
            for(uint i = 0 ; i < tmp36 ; i++)
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
            /**
             * C(A) + C(Y) -> C(A)
             * Modifications: All
             *
             *  (Indicators not listed are not affected)
             *  ZERO: If C(A) = 0, then ON; otherwise OFF
             *  NEG: If C(A)0 = 1, then ON; otherwise OFF
             *  OVR: If range of A is exceeded, then ON
             *  CARRY: If a carry out of A0 is generated, then ON; otherwise OFF
             */

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rA = Add36b (cpu.rA, cpu.CY, 0, I_ZNOC, & cpu.cu.IR, & ovf);
#if 0
            if (ovf && chkOVF ())
              {
                SET_I_OFLOW;
                if (tstOVFfault ())
                  {
                    if (cpu.cu.rpt || cpu.cu.rd || cpu.cu.rl)
                      {
                        SET_I_TALLY;
                      }
                    doFault (FAULT_OFL, 0, "ada overflow fault");
                  }
              }
#endif
            overflow (ovf, false, "ada overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Add36b (cpu.rA, cpu.CY, 0, I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "ada overflow fault");
              }
            cpu.rA = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0077:   // adaq
          {
            // C(AQ) + C(Y-pair) -> C(AQ)
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNOC, & cpu.cu.IR, & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "adaq overflow fault");
              //}
            overflow (ovf, false, "adaq overflow fault");
#else
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            word18 ir = cpu.cu.IR;
            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "adaq overflow fault");
              }
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
	  cpu.cu.IR = ir;
#endif
          }
          break;

        case 0033:   // adl
          {
            // C(AQ) + C(Y) sign extended -> C(AQ)
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            word72 tmp72 = SIGNEXT36_72 (cpu.CY); // sign extend Cy
            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNOC,
                            & cpu.cu.IR, & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "adl overflow fault");
              //}
            overflow (ovf, false, "adl overflow fault");
#else
            bool ovf;
            word72 tmp72 = SIGNEXT36_72 (cpu.CY); // sign extend Cy
            word18 ir = cpu.cu.IR;
            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNOC,
                            & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "adl overflow fault");
              }
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            cpu.cu.IR = ir;
#endif
          }
          break;


        case 0037:   // adlaq
          {
            // The adlaq instruction is identical to the adaq instruction with
            // the exception that the overflow indicator is not affected by the
            // adlaq instruction, nor does an overflow fault occur. Operands
            // and results are treated as unsigned, positive binary integers.
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);

            tmp72 = Add72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 0,
                            I_ZNC, & cpu.cu.IR, & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
          }
          break;

        case 0035:   // adla
          {
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
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX [n] = Add18b (cpu.rX [n], GETHI (cpu.CY), 0, I_ZNC,
                             & cpu.cu.IR, & ovf);
          }
          break;

        case 0076:   // adq
          {
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rQ = Add36b (cpu.rQ, cpu.CY, 0, I_ZNOC,
                                 & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "adq overflow fault");
              //}
            overflow (ovf, false, "adq overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Add36b (cpu.rQ, cpu.CY, 0, I_ZNOC,
                                 & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "adq overflow fault");
              }
            cpu.rQ = tmp;
            cpu.cu.IR = ir;
#endif
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
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX [n] = Add18b (cpu.rX [n], GETHI (cpu.CY), 0,
                                 I_ZNOC,
                                 & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "adxn overflow fault");
              //}
            overflow (ovf, false, "adxn overflow fault");
#else
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 ir = cpu.cu.IR;
            word18 tmp = Add18b (cpu.rX [n], GETHI (cpu.CY), 0,
                                 I_ZNOC,
                                 & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "adxn overflow fault");
              }
            cpu.rX [n] = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0054:   // aos
          {
            // C(Y)+1->C(Y)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.CY = Add36b (cpu.CY, 1, 0, I_ZNOC,
                                 & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //dlyDoFault (FAULT_OFL, 0, "aos overflow fault");
              //}
            overflow (ovf, true, "aos overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Add36b (cpu.CY, 1, 0, I_ZNOC,
                                 & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "aos overflow fault");
              }
            cpu.CY = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0055:   // asa
          {
            // C(A) + C(Y) -> C(Y)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.CY = Add36b (cpu.rA, cpu.CY, 0, I_ZNOC,
                             & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //dlyDoFault (FAULT_OFL, 0, "asa overflow fault");
              //}
            overflow (ovf, true, "asa overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp  = Add36b (cpu.rA, cpu.CY, 0, I_ZNOC,
                                  & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "asa overflow fault");
              }
            cpu.CY = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0056:   // asq
          {
            // C(Q) + C(Y) -> C(Y)
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.CY = Add36b (cpu.rQ, cpu.CY, 0, I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //dlyDoFault (FAULT_OFL, 0, "asq overflow fault");
              //}
            overflow (ovf, true, "asq overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Add36b (cpu.rQ, cpu.CY, 0, I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "asq overflow fault");
              }
            cpu.CY = tmp;
            cpu.cu.IR = ir;
#endif
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
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = Add18b (cpu.rX [n], GETHI (cpu.CY), 0,
                                   I_ZNOC, & cpu.cu.IR, & ovf);
            SETHI (cpu.CY, tmp18);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "asxn overflow fault");
              //}
            overflow (ovf, true, "asxn overflow fault");
#else
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 ir = cpu.cu.IR;
            word18 tmp18 = Add18b (cpu.rX [n], GETHI (cpu.CY), 0,
                                   I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "asxn overflow fault");
              }
            SETHI (cpu.CY, tmp18);
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0071:   // awca
          {
            // If carry indicator OFF, then C(A) + C(Y) -> C(A)
            // If carry indicator ON, then C(A) + C(Y) + 1 -> C(A)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rA = Add36b (cpu.rA, cpu.CY, TST_I_CARRY ? 1 : 0,
                                 I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "awca overflow fault");
              //}
            overflow (ovf, false, "awca overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Add36b (cpu.rA, cpu.CY, TST_I_CARRY ? 1 : 0,
                                 I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "awca overflow fault");
              }
            cpu.rA = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0072:   // awcq
          {
            // If carry indicator OFF, then C(Q) + C(Y) -> C(Q)
            // If carry indicator ON, then C(Q) + C(Y) + 1 -> C(Q)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rQ = Add36b (cpu.rQ, cpu.CY, TST_I_CARRY ? 1 : 0,
                             I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "awcq overflow fault");
              //}
            overflow (ovf, false, "awcq overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Add36b (cpu.rQ, cpu.CY, TST_I_CARRY ? 1 : 0,
                         I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "awcq overflow fault");
              }
            cpu.rQ = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        /// Fixed-Point Subtraction

        case 0175:  // sba
          {
            // C(A) - C(Y) -> C(A)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rA = Sub36b (cpu.rA, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "sba overflow fault");
              //}
            overflow (ovf, false, "sba overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Sub36b (cpu.rA, cpu.CY, 1, I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "sba overflow fault");
              }
            cpu.rA = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0177:  // sbaq
          {
            // C(AQ) - C(Y-pair) -> C(AQ)
#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            tmp72 = Sub72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 1,
                            I_ZNOC, & cpu.cu.IR,
                            & ovf);
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "sbaq overflow fault");
              //}
            overflow (ovf, false, "sbaq overflow fault");
#else
            bool ovf;
            word72 tmp72 = YPAIRTO72 (cpu.Ypair);
            word18 ir = cpu.cu.IR;
            tmp72 = Sub72b (convertToWord72 (cpu.rA, cpu.rQ), tmp72, 1,
                            I_ZNOC, & ir,
                            & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "sbaq overflow fault");
              }
            convertToWord36 (tmp72, & cpu.rA, & cpu.rQ);
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0135:  // sbla
          {
            // C(A) - C(Y) -> C(A) logical

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

            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX [n] = Sub18b (cpu.rX [n], GETHI (cpu.CY), 1,
                             I_ZNC, & cpu.cu.IR, & ovf);
          }
          break;

        case 0176:  // sbq
          {
            // C(Q) - C(Y) -> C(Q)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rQ = Sub36b (cpu.rQ, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "sbq overflow fault");
              //}
            overflow (ovf, false, "sbq overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Sub36b (cpu.rQ, cpu.CY, 1, I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "sbq overflow fault");
              }
            cpu.rQ = tmp;
            cpu.cu.IR = ir;
#endif
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

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            uint32 n = opcode & 07;  // get n
            cpu.rX [n] = Sub18b (cpu.rX [n], GETHI (cpu.CY), 1,
                                 I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "sbxn overflow fault");
              //}
            overflow (ovf, false, "sbxn overflow fault");
#else
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 ir = cpu.cu.IR;
            word18 tmp = Sub18b (cpu.rX [n], GETHI (cpu.CY), 1,
                                 I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "sbxn overflow fault");
              }
            cpu.rX [n] = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0155:  // ssa
          {
            // C(A) - C(Y) -> C(Y)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.CY = Sub36b (cpu.rA, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //dlyDoFault (FAULT_OFL, 0, "ssa overflow fault");
              //}
            overflow (ovf, true, "ssa overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Sub36b (cpu.rA, cpu.CY, 1, I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "ssa overflow fault");
              }
            cpu.CY = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0156:  // ssq
          {
            // C(Q) - C(Y) -> C(Y)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.CY = Sub36b (cpu.rQ, cpu.CY, 1, I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //dlyDoFault (FAULT_OFL, 0, "ssq overflow fault");
              //}
            overflow (ovf, true, "ssq overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Sub36b (cpu.rQ, cpu.CY, 1, I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "ssq overflow fault");
              }
            cpu.CY = tmp;
            cpu.cu.IR = ir;
#endif
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

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = Sub18b (cpu.rX [n], GETHI (cpu.CY), 1,
                                   I_ZNOC, & cpu.cu.IR, & ovf);
            SETHI (cpu.CY, tmp18);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //dlyDoFault (FAULT_OFL, 0, "ssxn overflow fault");
              //}
            overflow (ovf, true, "ssxn overflow fault");
#else
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 ir = cpu.cu.IR;
            word18 tmp18 = Sub18b (cpu.rX [n], GETHI (cpu.CY), 1,
                                   I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "ssxn overflow fault");
              }
            SETHI (cpu.CY, tmp18);
            cpu.cu.IR = ir;
#endif
          }
          break;


        case 0171:  // swca
          {
            // If carry indicator ON, then C(A)- C(Y) -> C(A)
            // If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rA = Sub36b (cpu.rA, cpu.CY, TST_I_CARRY ? 1 : 0,
                             I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "swca overflow fault");
              //}
            overflow (ovf, false, "swca overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Sub36b (cpu.rA, cpu.CY, TST_I_CARRY ? 1 : 0,
                                 I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "swca overflow fault");
              }
            cpu.rA = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        case 0172:  // swcq
          {
            // If carry indicator ON, then C(Q) - C(Y) -> C(Q)
            // If carry indicator OFF, then C(Q) - C(Y) - 1 -> C(Q)

#ifdef OVERFLOW_WRITE_THROUGH
            bool ovf;
            cpu.rQ = Sub36b (cpu.rQ, cpu.CY, TST_I_CARRY ? 1 : 0,
                                 I_ZNOC, & cpu.cu.IR, & ovf);
            //if (ovf && tstOVFfault ())
              //{
                //SET_I_OFLOW;
                //doFault (FAULT_OFL, 0, "swcq overflow fault");
              //}
            overflow (ovf, false, "swcq overflow fault");
#else
            bool ovf;
            word18 ir = cpu.cu.IR;
            word36 tmp = Sub36b (cpu.rQ, cpu.CY, TST_I_CARRY ? 1 : 0,
                                 I_ZNOC, & ir, & ovf);
            if (ovf && tstOVFfault ())
              {
                SET_I_OFLOW;
                doFault (FAULT_OFL, 0, "swcq overflow fault");
              }
            cpu.rQ = tmp;
            cpu.cu.IR = ir;
#endif
          }
          break;

        /// Fixed-Point Multiplication

        case 0401:  // mpf
            {
               // C(A) * C(Y) -> C(AQ), left adjusted
               /**
                * Two 36-bit fractional factors (including sign) are multiplied to
                * form a 71- bit fractional product (including sign), which is
                * stored left-adjusted in the AQ register. AQ71 contains a zero.
                * Overflow can occur only in the case of A and Y containing
                * negative 1 and the result exceeding the range of the AQ
                * register.
                */
                word72 tmp72 = SIGNEXT36_72 (cpu.rA) * SIGNEXT36_72 (cpu.CY);
                tmp72 &= MASK72;
                tmp72 <<= 1;    // left adjust so AQ71 contains 0
                // Overflow can occur only in the case of A and Y containing
                // negative 1
                if (cpu.rA == MAXNEG && cpu.CY == MAXNEG)
                {
#ifdef OVERFLOW_WRITE_THROUGH
                    SET_I_NEG;
                    CLR_I_ZERO;
#endif
                    //if (tstOVFfault ())
                      //{
                        //SET_I_OFLOW;
                        //doFault(FAULT_OFL, 0,"mpf overflow fault");
                      //}
                    overflow (true, false, "mpf overflow fault");
                }

                convertToWord36(tmp72, &cpu.rA, &cpu.rQ);
                SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
                SC_I_NEG (cpu.rA & SIGN36);
            }
            break;

        case 0402:  // mpy
            // C(Q) * C(Y) -> C(AQ), right adjusted

            {
                int64_t t0 = SIGNEXT36_64 (cpu.rQ & DMASK);
                int64_t t1 = SIGNEXT36_64 (cpu.CY & DMASK);

                __int128_t prod = (__int128_t) t0 * (__int128_t) t1;

                convertToWord36((word72)prod, &cpu.rA, &cpu.rQ);

                SC_I_ZERO (cpu.rA == 0 && cpu.rQ == 0);
                SC_I_NEG (cpu.rA & SIGN36);
            }
            break;

//#define DIV_TRACE

        /// Fixed-Point Division

        case 0506:  // div
            // C(Q) / (Y) integer quotient -> C(Q), integer remainder -> C(A)
            /**
             * A 36-bit integer dividend (including sign) is divided by a
             * 36-bit integer divisor (including sign) to form a 36-bit integer
             * quotient (including sign) and a 36-bit integer remainder
             * (including sign). The remainder sign is equal to the dividend
             * sign unless the remainder is zero.
             *
             * If the dividend = -2**35 and the divisor = -1 or if the divisor
             * = 0, then division does not take place. Instead, a divide check
             * fault occurs, C(Q) contains the dividend magnitude, and the
             * negative indicator reflects the dividend sign.
             */

            // RJ78: If the dividend = -2**35 and the divisor = +/-1, or if the divisor is 0

            if ((cpu.rQ == MAXNEG && (cpu.CY == 1 || cpu.CY == NEG136)) || (cpu.CY == 0))
            {
//sim_printf ("DIV Q %012llo Y %012llo\n", cpu.rQ, cpu.CY); 
// case 1  400000000000 000000000000 --> 000000000000
// case 2  000000000000 000000000000 --> 400000000000
                //cpu.rA = 0;  // works for case 1
                cpu.rA = (cpu.rQ & SIGN36) ? 0 : SIGN36; // works for case 1 & 2
                //if (cpu.rQ & SIGN36 != cpu.CY & SIGN36)
                  //cpu.rA = SIGN36;
                //else

                // no division takes place
                SC_I_ZERO (cpu.CY == 0);
                SC_I_NEG (cpu.rQ & SIGN36);
                doFault(FAULT_DIV, 0, "div divide check");
            }
            else
            {
                t_int64 dividend = (t_int64) (SIGNEXT36_64(cpu.rQ));
                t_int64 divisor = (t_int64) (SIGNEXT36_64(cpu.CY));

#ifdef DIV_TRACE
                sim_debug (DBG_CAC, & cpu_dev, "\n");
                sim_debug (DBG_CAC, & cpu_dev,
                           ">>> dividend cpu.rQ %lld (%012llo)\n", dividend, cpu.rQ);
                sim_debug (DBG_CAC, & cpu_dev,
                           ">>> divisor  CY %lld (%012llo)\n", divisor, cpu.CY);
#endif

                t_int64 quotient = dividend / divisor;
                t_int64 remainder = dividend % divisor;

#ifdef DIV_TRACE
                sim_debug (DBG_CAC, & cpu_dev, ">>> quot 1 %lld\n", quotient);
                sim_debug (DBG_CAC, & cpu_dev, ">>> rem 1 %lld\n", remainder);
#endif

// Evidence is that DPS8 rounds toward zero; if it turns out that it
// rounds toward -inf, try this code:
#if 0
                // XXX C rounds toward zero; I suspect that DPS8 rounded toward
                // -inf.
                // If the remainder is negative, we rounded the wrong way
                if (remainder < 0)
                  {
                    remainder += divisor;
                    quotient -= 1;

#ifdef DIV_TRACE
                    sim_debug (DBG_CAC, & cpu_dev,
                               ">>> quot 2 %lld\n", quotient);
                    sim_debug (DBG_CAC, & cpu_dev,
                               ">>> rem 2 %lld\n", remainder);
#endif
                  }
#endif

#ifdef DIV_TRACE
                //  (a/b)*b + a%b is equal to a.
                sim_debug (DBG_CAC, & cpu_dev,
                           "dividend was                   = %lld\n", dividend);
                sim_debug (DBG_CAC, & cpu_dev,
                           "quotient * divisor + remainder = %lld\n",
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
                               " rQ %012llo CY %012llo\n", cpu.rQ, cpu.CY);
                  }

                cpu.rA = remainder & DMASK;
                cpu.rQ = quotient & DMASK;

#ifdef DIV_TRACE
                sim_debug (DBG_CAC, & cpu_dev, "rA (rem)  %012llo\n", cpu.rA);
                sim_debug (DBG_CAC, & cpu_dev, "rQ (quot) %012llo\n", cpu.rQ);
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
            // -C(A) -> C(A) if C(A) ≠ 0

#ifdef OVERFLOW_WRITE_THROUGH
            cpu.rA &= DMASK;
            if (cpu.rA == 0400000000000ULL)
            {
                CLR_I_ZERO;
                SET_I_NEG;
                //if (tstOVFfault ())
                  //{
                    //SET_I_OFLOW;
                    //doFault(FAULT_OFL, 0,"neg overflow fault");
                  //}
                overflow (true, false, "neg overflow fault");
            }

            cpu.rA = -cpu.rA;

            cpu.rA &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
#else
            cpu.rA &= DMASK;
            if (cpu.rA == 0400000000000ULL)
            {
                if (tstOVFfault ())
                  {
                    SET_I_OFLOW;
                    doFault(FAULT_OFL, 0,"neg overflow fault");
                  }
            }

            cpu.rA = -cpu.rA;

            cpu.rA &= DMASK;    // keep to 36-bits

            SC_I_ZERO (cpu.rA == 0);
            SC_I_NEG (cpu.rA & SIGN36);
#endif

            break;

        case 0533:  // negl
            // -C(AQ) -> C(AQ) if C(AQ) ≠ 0
            {
#ifdef OVERFLOW_WRITE_THROUGH
                cpu.rA &= DMASK;
                cpu.rQ &= DMASK;

                if (cpu.rA == 0400000000000ULL & cpu.rQ == 0)
                {
                    CLR_I_ZERO;
                    SET_I_NEG;
                    //if (tstOVFfault ())
                      //{
                        //SET_I_OFLOW;
                        //doFault(FAULT_OFL, 0,"negl overflow fault");
                      //}
                    overflow (true, false, "negl overflow fault");
                }

                word72 tmp72 = convertToWord72(cpu.rA, cpu.rQ);
                tmp72 = -tmp72;

                SC_I_ZERO (tmp72 == 0);
                SC_I_NEG (tmp72 & SIGN72);

                convertToWord36(tmp72, &cpu.rA, &cpu.rQ);
#else
                cpu.rA &= DMASK;
                cpu.rQ &= DMASK;

                if (cpu.rA == 0400000000000ULL & cpu.rQ == 0)
                {
                    if (tstOVFfault ())
                      {
                        SET_I_OFLOW;
                        doFault(FAULT_OFL, 0,"negl overflow fault");
                      }
                }

                word72 tmp72 = convertToWord72(cpu.rA, cpu.rQ);
                tmp72 = -tmp72;

                SC_I_ZERO (tmp72 == 0);
                SC_I_NEG (tmp72 & SIGN72);

                convertToWord36(tmp72, &cpu.rA, &cpu.rQ);
#endif
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

            cmp36(cpu.rA, cpu.CY, &cpu.cu.IR);
            break;

        case 0116:  // cmpq
            // C(Q) :: C(Y)
            cmp36(cpu.rQ, cpu.CY, &cpu.cu.IR);
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
                cmp18(cpu.rX[n], GETHI(cpu.CY), &cpu.cu.IR);
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
            cmp36wl(cpu.rA, cpu.CY, cpu.rQ, &cpu.cu.IR);
            break;

        case 0117:  // cmpaq
            // C(AQ) :: C(Y-pair)
            {
                word72 tmp72 = YPAIRTO72(cpu.Ypair);

                word72 trAQ = convertToWord72(cpu.rA, cpu.rQ);
                trAQ &= MASK72;

                cmp72(trAQ, tmp72, &cpu.cu.IR);
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
                word72 tmp72 = YPAIRTO72(cpu.Ypair);
                word72 trAQ = convertToWord72(cpu.rA, cpu.rQ);
                trAQ = trAQ & tmp72;
                trAQ &= MASK72;

                SC_I_ZERO (trAQ == 0);
                SC_I_NEG (trAQ & SIGN72);
                convertToWord36(trAQ, &cpu.rA, &cpu.rQ);
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

        case 0340:  // asnx0
        case 0341:  // asnx1
        case 0342:  // asnx2
        case 0343:  // asnx3
        case 0344:  // asnx4
        case 0345:  // asnx5
        case 0346:  // asnx6
        case 0347:  // asnx7
            // For n = 0, 1, ..., or 7 as determined by operation code
            // C(Xn)i & C(Y)i -> C(Y)i for i = (0, 1, ..., 17)
            {
                uint32 n = opcode & 07;  // get n
                word18 tmp18 = cpu.rX[n] & GETHI(cpu.CY);
                tmp18 &= MASK18;

                SC_I_ZERO (tmp18 == 0);
                SC_I_NEG (tmp18 & SIGN18);

                SETHI(cpu.CY, tmp18);
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
                cpu.rX[n] &= GETHI(cpu.CY);
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
                word72 tmp72 = YPAIRTO72(cpu.Ypair);
                word72 trAQ = convertToWord72(cpu.rA, cpu.rQ);
                trAQ = trAQ | tmp72;
                trAQ &= MASK72;

                SC_I_ZERO (trAQ == 0);
                SC_I_NEG (trAQ & SIGN72);
                convertToWord36(trAQ, &cpu.rA, &cpu.rQ);
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

                word18 tmp18 = cpu.rX[n] | GETHI(cpu.CY);
                tmp18 &= MASK18;

                SC_I_ZERO (tmp18 == 0);
                SC_I_NEG (tmp18 & SIGN18);

                SETHI(cpu.CY, tmp18);
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
                cpu.rX[n] |= GETHI(cpu.CY);
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
                word72 tmp72 = YPAIRTO72(cpu.Ypair);
                word72 trAQ = convertToWord72(cpu.rA, cpu.rQ);
                trAQ = trAQ ^ tmp72;
                trAQ &= MASK72;

                SC_I_ZERO (trAQ == 0);
                SC_I_NEG (trAQ & SIGN72);

                convertToWord36(trAQ, &cpu.rA, &cpu.rQ);
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

                word18 tmp18 = cpu.rX[n] ^ GETHI(cpu.CY);
                tmp18 &= MASK18;

                SC_I_ZERO (tmp18 == 0);
                SC_I_NEG (tmp18 & SIGN18);

                SETHI(cpu.CY, tmp18);
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
                cpu.rX[n] ^= GETHI(cpu.CY);
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
                word72 tmp72 = YPAIRTO72(cpu.Ypair);
                word72 trAQ = convertToWord72(cpu.rA, cpu.rQ);
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
                word18 tmp18 = cpu.rX[n] & GETHI(cpu.CY);
                tmp18 &= MASK18;
                sim_debug (DBG_TRACE, & cpu_dev,
                           "n %o rX %06o HI %06o tmp %06o\n",
                           n, cpu.rX [n], (word18) (GETHI(cpu.CY) & MASK18), tmp18);

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
                word72 tmp72 = YPAIRTO72(cpu.Ypair);   //

                word72 trAQ = convertToWord72(cpu.rA, cpu.rQ);
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
                word18 tmp18 = cpu.rX[n] & ~GETHI(cpu.CY);
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

            cpu.Ypair[0] = ((word36)cpu.rE << 28) | ((cpu.rA & 0777777777400LLU) >> 8);
            cpu.Ypair[1] = ((cpu.rA & 0377) << 28) | ((cpu.rQ & 0777777777400LLU) >> 8);

            break;

        case 0472:  // dfstr

            dfstr (cpu.Ypair);
            break;

        case 0455:  // fst
            // C(E) -> C(Y)0,7
            // C(A)0,27 -> C(Y)8,35
            cpu.rE &= MASK8;
            cpu.rA &= DMASK;
            cpu.CY = ((word36)cpu.rE << 28) | (((cpu.rA >> 8) & 01777777777LL));
            break;

        case 0470:  // fstr
            // The fstr instruction performs a true round and normalization on
            // C(EAQ) as it is stored.

//            frd();
//
//            // C(E) -> C(Y)0,7
//            // C(A)0,27 -> C(Y)8,35
//            cpu.CY = ((word36)cpu.rE << 28) | (((cpu.rA >> 8) & 01777777777LL));
//
//            // Zero: If C(Y) = floating point 0, then ON; otherwise OFF
//            //SC_I_ZERO ((cpu.CY & 01777777777LL) == 0);
//            bool isZero = cpu.rE == -128 && cpu.rA == 0;
//            SC_I_ZERO(isZero);
//
//            // Neg: If C(Y)8 = 1, then ON; otherwise OFF
//            //SC_I_NEG(cpu.CY & 01000000000LL);
//            SC_I_NEG(cpu.rA & SIGN36);
//
//            // Exp Ovr: If exponent is greater than +127, then ON
//            // Exp Undr: If exponent is less than -128, then ON
//            // XXX: not certain how these can occur here ....

            fstr(&cpu.CY);

            break;

        /// Floating-Point Addition

        case 0477:  // dfad
            // The dfad instruction may be thought of as a dufa instruction
            // followed by a fno instruction.

            dufa (false);
            fno ();
            break;

        case 0437:  // dufa
            dufa (false);
            break;

        case 0475:  // fad
            // The fad instruction may be thought of a an ufa instruction
            // followed by a fno instruction.
            // (Heh, heh. We'll see....)

            ufa();
            fno ();

            break;

        case 0435:  // ufa
            // C(EAQ) + C(Y) -> C(EAQ)

            ufa();
            break;

        /// Floating-Point Subtraction

        case 0577:  // dfsb
            // The dfsb instruction is identical to the dfad instruction with
            // the exception that the twos complement of the mantissa of the
            // operand from main memory is used.

            //dufs ();
            dufa (true);
            fno ();
            break;

        case 0537:  // dufs
            //dufs ();
            dufa (true);
            break;

        case 0575:  // fsb
            // The fsb instruction may be thought of as an ufs instruction
            // followed by a fno instruction.
            ufs ();
            fno ();

            break;

        case 0535:  // ufs
            // C(EAQ) - C(Y) -> C(EAQ)

            ufs ();
            break;

        /// Floating-Point Multiplication

        case 0463:  // dfmp
            // The dfmp instruction may be thought of as a dufm instruction
            // followed by a fno instruction.

            dufm ();
            fno ();

            break;

        case 0423:  // dufm

            dufm ();
            break;

        case 0461:  // fmp
            // The fmp instruction may be thought of as a ufm instruction
            // followed by a fno instruction.

            ufm ();
            fno ();

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
            // which, in turn, requires that C(AQ)0 ≠ C(AQ)1.list
            //
            // !!!! For personal reasons the following 3 lines of comment must
            // never be removed from this program or any code derived
            // therefrom. HWR 25 Aug 2014
            ///Charles Is the coolest
            ///true story y'all
            //you should get me darksisers 2 for christmas

            fno ();
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

            fcmp();
            break;

        /// Floating-Point Miscellaneous

        case 0415:  // ade
            // C(E) + C(Y)0,7 -> C(E)
            {
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
                        doFault (FAULT_OFL, 0, "ade exp overflow fault");
                }

                if (e < -128)
                {
                    SET_I_EUFL;
                    if (tstOVFfault ())
                        doFault (FAULT_OFL, 0, "ade exp underflow fault");
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

            cpu.rE = (cpu.CY >> 28) & 0377;
            CLR_I_ZERO;
            CLR_I_NEG;

            break;

        case 0456:  // ste
            // C(E) -> C(Y)0,7
            // 00...0 -> C(Y)8,17

            putbits36 (& cpu.CY, 0, 18, ((word36)(cpu.rE & 0377) << 10));
            break;


        /// TRANSFER INSTRUCTIONS

        case 0713:  // call6

            if (cpu.TPR.TRR > cpu.PPR.PRR)
            {
                sim_debug (DBG_APPENDING, & cpu_dev,
                           "call6 access violation fault (outward call)");
                doFault (FAULT_ACV, ACV9,
                         "call6 access violation fault (outward call)");
            }
            if (cpu.TPR.TRR < cpu.PPR.PRR)
                cpu.PR[7].SNR = ((cpu.DSBR.STACK << 3) | cpu.TPR.TRR) & MASK15;
            if (cpu.TPR.TRR == cpu.PPR.PRR)
                cpu.PR[7].SNR = cpu.PR[6].SNR;
            cpu.PR[7].RNR = cpu.TPR.TRR;
            if (cpu.TPR.TRR == 0)
                cpu.PPR.P = cpu.SDW->P;
            else
                cpu.PPR.P = 0;
            cpu.PR[7].WORDNO = 0;
            cpu.PR[7].BITNO = 0;
            cpu.PPR.PRR = cpu.TPR.TRR;
            cpu.PPR.PSR = cpu.TPR.TSR;
            cpu.PPR.IC = cpu.TPR.CA;
            sim_debug (DBG_TRACE, & cpu_dev,
                       "call6 PPR.PRR %o\n", cpu.PPR.PRR);
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

            word18 tempIR = GETLO(cpu.CY) & 0777770;
            // XXX Assuming 'mask privileged mode' is 'temporary absolute mode'
            //if (get_addr_mode () == ABSOLUTE_mode) // abs. or temp. abs.
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
            cpu.PPR.IC = GETHI(cpu.CY);
            cpu.cu.IR = tempIR;

            return CONT_TRA;
          }

        case 0610:  // rtcd
            /*
             TODO: Complete RTCD
             If an access violation fault occurs when fetching the SDW for the
             Y-pair, the C(PPR.PSR) and C(PPR.PRR) are not altered.  If the
             rtcd instruction is executed with the processor in absolute mode
             with bit 29 of the instruction word set OFF and without
             indirection through an ITP or ITS pair, then:

                 appending mode is entered for address preparation for the
                 rtcd operand and is retained if the instruction executes
                 successfully, and the effective segment number generated for
                 the SDW fetch and subsequent loading into C(TPR.TSR) is equal
                 to C(PPR.PSR) and may be undefined in absolute mode, and the
                 effective ring number loaded into C(TPR.TRR) prior to the SDW
                 fetch is equal to C(PPR.PRR) (which is 0 in absolute mode)
                 implying that control is always transferred into ring 0.
             */

            // C(Y-pair)3,17 -> C(PPR.PSR)
            // Maximum of C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) -> C(PPR.PRR)
            // C(Y-pair)36,53 -> C(PPR.IC)
            // If C(PPR.PRR) = 0 then C(SDW.P) -> C(PPR.P);
            // otherwise 0 -> C(PPR.P)
            // C(PPR.PRR) -> C(PRn.RNR) for n = (0, 1, ..., 7)

            //processorCycle = RTCD_OPERAND_FETCH;

            sim_debug (DBG_TRACE, & cpu_dev,
                       "RTCD even %012llo odd %012llo\n", cpu.Ypair [0], cpu.Ypair [1]);

            // C(Y-pair)3,17 -> C(PPR.PSR)
            cpu.PPR.PSR = GETHI(cpu.Ypair[0]) & 077777LL;

            // XXX ticket #16
            // Maximum of C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) -> C(PPR.PRR)
            //cpu.PPR.PRR = max3(((GETLO(cpu.Ypair[0]) >> 15) & 7), cpu.TPR.TRR, SDW->R1);
            cpu.PPR.PRR = max3(((GETLO(cpu.Ypair[0]) >> 15) & 7), cpu.TPR.TRR, cpu.RSDWH_R1);

            // C(Y-pair)36,53 -> C(PPR.IC)
            cpu.PPR.IC = GETHI(cpu.Ypair[1]);

            sim_debug (DBG_TRACE, & cpu_dev,
                       "RTCD %05o:%06o\n", cpu.PPR.PSR, cpu.PPR.IC);

            // If C(PPR.PRR) = 0 then C(SDW.P) -> C(PPR.P);
            // otherwise 0 -> C(PPR.P)
            if (cpu.PPR.PRR == 0)
                cpu.PPR.P = cpu.SDW->P;
            else
                cpu.PPR.P = 0;

            sim_debug (DBG_TRACE, & cpu_dev,
                       "RTCD PPR.PRR %o PPR.P %o\n", cpu.PPR.PRR, cpu.PPR.P);

            // C(PPR.PRR) -> C(PRn.RNR) for n = (0, 1, ..., 7)
            //for(int n = 0 ; n < 8 ; n += 1)
            //  PR[n].RNR = cpu.PPR.PRR;

            cpu.PR[0].RNR =
            cpu.PR[1].RNR =
            cpu.PR[2].RNR =
            cpu.PR[3].RNR =
            cpu.PR[4].RNR =
            cpu.PR[5].RNR =
            cpu.PR[6].RNR =
            cpu.PR[7].RNR = cpu.PPR.PRR;

            return CONT_TRA;


        case 0614:  // teo
            // If exponent overflow indicator ON then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            // otherwise, no change to C(PPR)
            if (TST_I_EOFL)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;
                CLR_I_EOFL;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0615:  // teu
            // If exponent underflow indicator ON then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (TST_I_EUFL)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

                CLR_I_EUFL;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;


        case 0604:  // tmi
            // If negative indicator ON then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (TST_I_NEG)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0602:  // tnc
            // If carry indicator OFF then
            //   C(TPR.CA) -> C(PPR.IC)
            //   C(TPR.TSR) -> C(PPR.PSR)
            if (!TST_I_CARRY)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0601:  // tnz
            // If zero indicator OFF then
            //     C(TPR.CA) -> C(PPR.IC)
            //     C(TPR.TSR) -> C(PPR.PSR)
            if (!TST_I_ZERO)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0617:  // tov
            // If overflow indicator ON then
            //   C(TPR.CA) -> C(PPR.IC)
            //   C(TPR.TSR) -> C(PPR.PSR)
            if (TST_I_OFLOW)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

                CLR_I_OFLOW;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0605:  // tpl
            // If negative indicator OFF, then
            //   C(TPR.CA) -> C(PPR.IC)
            //   C(TPR.TSR) -> C(PPR.PSR)
            if (!(TST_I_NEG))
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0710:  // tra
            // C(TPR.CA) -> C(PPR.IC)
            // C(TPR.TSR) -> C(PPR.PSR)

            cpu.PPR.IC = cpu.TPR.CA;
            cpu.PPR.PSR = cpu.TPR.TSR;
            sim_debug (DBG_TRACE, & cpu_dev, "TRA %05o:%06o\n", cpu.PPR.PSR, cpu.PPR.IC);
            if (TST_I_ABS && get_went_appending ())
              {
                set_addr_mode(APPEND_mode);
              }

            return CONT_TRA;

        case 0603:  // trc
            //  If carry indicator ON then
            //    C(TPR.CA) -> C(PPR.IC)
            //    C(TPR.TSR) -> C(PPR.PSR)
            if (TST_I_CARRY)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0270:  //< tsp0
        case 0271:  //< tsp1
        case 0272:  //< tsp2
        case 0273:  //< tsp3

        case 0670:  //< tsp4
        case 0671:  //< tsp5
        case 0672:  //< tsp6
        case 0673:  //< tsp7
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

                // XXX According to figure 8.1, all of this is done by the
                //  append unit.
                cpu.PR[n].RNR = cpu.PPR.PRR;

// According the AL39, the PSR is 'undefined' in absolute mode.
// ISOLTS thinks means don't change the operand
                if (get_addr_mode () == APPEND_mode)
                  cpu.PR[n].SNR = cpu.PPR.PSR;
                cpu.PR[n].WORDNO = (cpu.PPR.IC + 1) & MASK18;
                cpu.PR[n].BITNO = 0;
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;
            }
            return CONT_TRA;

        case 0715:  // tss
            if (cpu.TPR.CA >= ((word18) cpu.BAR.BOUND) << 9)
            {
                doFault (FAULT_ACV, ACV15, "TSS boundary violation");
                //break;
            }
            // AL39 is misleading; the BAR base is added in by the
            // instruction fetch.
            // C(TPR.CA) + (BAR base) -> C(PPR.IC)
            // C(TPR.TSR) -> C(PPR.PSR)
            cpu.PPR.IC = cpu.TPR.CA /* + (cpu.BAR.BASE << 9) */;
            cpu.PPR.PSR = cpu.TPR.TSR;

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
                uint32 n = opcode & 07;  // get n
                cpu.rX[n] = (cpu.PPR.IC + 1) & MASK18;
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;
            }
            return CONT_TRA;

        case 0607:  // ttf
            // If tally runout indicator OFF then
            //   C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            // otherwise, no change to C(PPR)
            if (TST_I_TALLY == 0)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

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
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        /// POINTER REGISTER INSTRUCTIONS

        /// Pointer Register Data Movement Load

        case 0311:  // easp0
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[0].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0313:  // easp2
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[2].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0331:  // easp4
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[4].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0333:  // easp6
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[6].SNR = cpu.TPR.CA & MASK15;
            break;

        case 0310:  // eawp0
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[0].WORDNO = cpu.TPR.CA;
            cpu.PR[0].BITNO = cpu.TPR.TBR;
            break;
        case 0312:  // eawp2
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[2].WORDNO = cpu.TPR.CA;
            cpu.PR[2].BITNO = cpu.TPR.TBR;
            break;
        case 0330:  // eawp4
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[4].WORDNO = cpu.TPR.CA;
            cpu.PR[4].BITNO = cpu.TPR.TBR;
            break;
        case 0332:  // eawp6
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[6].WORDNO = cpu.TPR.CA;
            cpu.PR[6].BITNO = cpu.TPR.TBR;
            break;

        case 0351:  // epbp1
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[1].RNR = cpu.TPR.TRR;
            cpu.PR[1].SNR = cpu.TPR.TSR;
            cpu.PR[1].WORDNO = 0;
            cpu.PR[1].BITNO = 0;
            break;
        case 0353:  // epbp3
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[3].RNR = cpu.TPR.TRR;
            cpu.PR[3].SNR = cpu.TPR.TSR;
            cpu.PR[3].WORDNO = 0;
            cpu.PR[3].BITNO = 0;
            break;
        case 0371:  // epbp5
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[5].RNR = cpu.TPR.TRR;
            cpu.PR[5].SNR = cpu.TPR.TSR;
            cpu.PR[5].WORDNO = 0;
            cpu.PR[5].BITNO = 0;
            break;
        case 0373:  // epbp7
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[7].RNR = cpu.TPR.TRR;
            cpu.PR[7].SNR = cpu.TPR.TSR;
            cpu.PR[7].WORDNO = 0;
            cpu.PR[7].BITNO = 0;
            break;

        case 0350:  // epp0
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[0].RNR = cpu.TPR.TRR;
            cpu.PR[0].SNR = cpu.TPR.TSR;
            cpu.PR[0].WORDNO = cpu.TPR.CA;
            cpu.PR[0].BITNO = cpu.TPR.TBR;
            break;
        case 0352:  // epp2
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[2].RNR = cpu.TPR.TRR;
            cpu.PR[2].SNR = cpu.TPR.TSR;
            cpu.PR[2].WORDNO = cpu.TPR.CA;
            cpu.PR[2].BITNO = cpu.TPR.TBR;
            break;
        case 0370:  // epp4
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[4].RNR = cpu.TPR.TRR;
            cpu.PR[4].SNR = cpu.TPR.TSR;
            cpu.PR[4].WORDNO = cpu.TPR.CA;
            cpu.PR[4].BITNO = cpu.TPR.TBR;
            break;
        case 0372:  // epp6
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[6].RNR = cpu.TPR.TRR;
            cpu.PR[6].SNR = cpu.TPR.TSR;
            cpu.PR[6].WORDNO = cpu.TPR.CA;
            cpu.PR[6].BITNO = cpu.TPR.TBR;
            break;

        case 0173:  // lpri
            // For n = 0, 1, ..., 7
            //  Y-pair = Y-block16 + 2n
            //  Maximum of C(Y-pair)18,20; C(SDW.R1); C(TPR.TRR) -> C(PRn.RNR)
            //  C(Y-pair) 3,17 -> C(PRn.SNR)
            //  C(Y-pair)36,53 -> C(PRn.WORDNO)
            //  C(Y-pair)57,62 -> C(PRn.BITNO)

            for(uint32 n = 0 ; n < 8 ; n ++)
            {
                cpu.Ypair[0] = cpu.Yblock16[n * 2 + 0]; // Even word of ITS pointer pair
                cpu.Ypair[1] = cpu.Yblock16[n * 2 + 1]; // Odd word of ITS pointer pair

                word3 Crr = (GETLO(cpu.Ypair[0]) >> 15) & 07; // RNR from ITS pair
                if (get_addr_mode () == APPEND_mode)
                  cpu.PR[n].RNR = max3(Crr, cpu.SDW->R1, cpu.TPR.TRR) ;
                else
                  cpu.PR[n].RNR = Crr;
                cpu.PR[n].SNR = (cpu.Ypair[0] >> 18) & MASK15;
                cpu.PR[n].WORDNO = GETHI(cpu.Ypair[1]);
// According to ISOLTS, loading a 077 into bitno results in 037
                //cpu.PR[n].BITNO = (GETLO(cpu.Ypair[1]) >> 9) & 077;
                uint bitno = (GETLO(cpu.Ypair[1]) >> 9) & 077;
                if (bitno == 077)
                  bitno = 037;
                cpu.PR[n].BITNO = bitno;
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
            //  If C(Y)0,1 ≠ 11, then
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
                cpu.PR[n].RNR = cpu.TPR.TRR;

// [CAC] sprpn says: If C(PRn.SNR) 0,2 are nonzero, and C(PRn.SNR) != 11...1,
// then a store fault (illegal pointer) will occur and C(Y) will not be changed.
// I interpret this has meaning that only the high bits should be set here

                if (((cpu.CY >> 34) & 3) != 3)
                    cpu.PR[n].BITNO = (cpu.CY >> 30) & 077;
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
                    doFault(FAULT_CMD, flt_cmd_lprpn_bits, "lprpn");
                  }
// The SPRPn instruction stores only the low 12 bits of the 15 bit SNR.
// A special case is made for an SNR of all ones; it is stored as 12 1's.
// The pcode in AL39 handles this awkwardly; I believe this is
// the same, but in a more straightforward manner

               // Get the 12 bit operand SNR
               word12 oSNR = getbits36 (cpu.CY, 6, 12);
               // Test for special case
               if (oSNR == 07777)
                 cpu.PR[n].SNR = 077777;
               else
                 cpu.PR[n].SNR = oSNR; // usigned word will 0-extend.
                //C(Y)18,35 -> C(PRn.WORDNO)
                cpu.PR[n].WORDNO = GETLO(cpu.CY);

                sim_debug (DBG_APPENDING, & cpu_dev,
                           "lprp%d CY 0%012llo, PR[n].RNR 0%o, "
                           "PR[n].BITNO 0%o, PR[n].SNR 0%o, PR[n].WORDNO %o\n",
                           n, cpu.CY, cpu.PR[n].RNR, cpu.PR[n].BITNO, cpu.PR[n].SNR,
                           cpu.PR[n].WORDNO);
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

            for(uint32 n = 0 ; n < 8 ; n++)
            {
                cpu.Yblock16[2 * n] = 043;
                cpu.Yblock16[2 * n] |= ((word36) cpu.PR[n].SNR) << 18;
                cpu.Yblock16[2 * n] |= ((word36) cpu.PR[n].RNR) << 15;

                cpu.Yblock16[2 * n + 1] = (word36) cpu.PR[n].WORDNO << 18;
                cpu.Yblock16[2 * n + 1] |= (word36) cpu.PR[n].BITNO << 9;
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
            cpu.Ypair[0]  = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[0].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[0].RNR) << 15;

            cpu.Ypair[1]  = (word36) cpu.PR[0].WORDNO << 18;
            cpu.Ypair[1] |= (word36) cpu.PR[0].BITNO << 9;

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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[2].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[2].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[2].WORDNO << 18;
            cpu.Ypair[1]|= (word36) cpu.PR[2].BITNO << 9;

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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[4].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[4].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[4].WORDNO << 18;
            cpu.Ypair[1]|= (word36) cpu.PR[4].BITNO << 9;
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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[6].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[6].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[6].WORDNO << 18;
            cpu.Ypair[1]|= (word36) cpu.PR[6].BITNO << 9;
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

                // If C(PRn.SNR)0,2 are nonzero, and C(PRn.SNR) ≠ 11...1, then
                // a store fault (illegal pointer) will occur and C(Y) will not
                // be changed.

                if ((cpu.PR[n].SNR & 070000) != 0 && cpu.PR[n].SNR != MASK15)
                  doFault(FAULT_STR, flt_str_ill_ptr, "sprpn");

                if (cpu.switches.lprp_highonly)
                  {
                    cpu.CY  =  ((word36) (cpu.PR[n].BITNO & 077)) << 30;
                    // lower 12- of 15-bits
                    cpu.CY |=  ((word36) (cpu.PR[n].SNR & 07777)) << 18;
                    cpu.CY |=  cpu.PR[n].WORDNO & PAMASK;
                  }
                else
                  {
                    cpu.CY  =  ((word36) cpu.PR[n].BITNO) << 30;
                    // lower 12- of 15-bits
                    cpu.CY |=  ((word36) (cpu.PR[n].SNR) & 07777) << 18;
                    cpu.CY |=  cpu.PR[n].WORDNO;
                  }

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
                cpu.PR[n].WORDNO += GETHI(cpu.CY);
                cpu.PR[n].WORDNO &= MASK18;
                cpu.PR[n].BITNO = 0;
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
                cpu.PR[n].WORDNO += GETHI(cpu.CY);
                cpu.PR[n].WORDNO &= MASK18;
                cpu.PR[n].BITNO = 0;
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
              uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
              int scu_unit_num = query_scu_unit_num (currentRunningCPUnum, cpu_port_num);
              sim_debug (DBG_TRACE, & cpu_dev, "rccl CA %08o cpu port %o scu unit %d\n", cpu.TPR.CA, cpu_port_num, scu_unit_num);
              if (scu_unit_num < 0)
                {
                  sim_warn ("rccl on CPU %u port %d has no SCU; faulting\n", currentRunningCPUnum, cpu_port_num);
                  doFault (FAULT_ONC, flt_onc_nem, "(rccl)"); // XXX nem?
                }

              t_stat rc = scu_rscr (scu_unit_num, currentRunningCPUnum, 040, & cpu.rA, & cpu.rQ);
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
                  unsigned long secs = big / 1000000u;
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
            doFault (FAULT_DRL, 0, "drl");
            // break;

        case 0716:  // xec
            {
                cpu.cu.xde = 1;
                cpu.cu.xdo = 0;
            }
            break;

        case 0717:  // xed
            {
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
            }
            break;

        case 0001:   // mme
            // Causes a fault that fetches and executes, in absolute mode, the
            // instruction pair at main memory location C+4. The value of C is
            // obtained from the FAULT VECTOR switches on the processor
            // configuration panel.
            doFault(FAULT_MME, 0, "Master Mode Entry (mme)");
            // break;

        case 0004:   // mme2
            // Causes a fault that fetches and executes, in absolute mode, the
            // instruction pair at main memory location C+(52)8. The value of C
            // is obtained from the FAULT VECTOR switches on the processor
            // configuration panel.
            doFault(FAULT_MME2, 0, "Master Mode Entry 2 (mme2)");
            // break;

        case 0005:   // mme3
            // Causes a fault that fetches and executes, in absolute mode, the
            // instruction pair at main memory location C+(54)8. The value of C
            // is obtained from the FAULT VECTOR switches on the processor
            // configuration panel.
            doFault(FAULT_MME3, 0, "Master Mode Entry 3 (mme3)");
            // break;

        case 0007:   // mme4
            // Causes a fault that fetches and executes, in absolute mode, the
            // instruction pair at main memory location C+(56)8. The value of C
            // is obtained from the FAULT VECTOR switches on the processor
            // configuration panel.
            doFault(FAULT_MME4, 0, "Master Mode Entry 4 (mme4)");
            // break;

        case 0011:   // nop
            break;

        case 0012:   // puls1
            break;

        case 0013:   // puls2
            break;

        /// Repeat

        case 0560:  // rpd
            {
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
#if 0
            if (cpu.switches.halt_on_unimp)
                return STOP_UNIMP;
            // Technically not true
            doFault(FAULT_IPR, flt_ipr_ill_op, "Illegal instruction");
#endif
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
            SETHI(cpu.CY, (cpu.BAR.BASE << 9) | cpu.BAR.BOUND);

            break;


        /// Translation

        case 0505:  // bcd
            // Shift C(A) left three positions
            // | C(A) | / C(Y) -> 4-bit quotient plus remainder
            // Shift C(Q) left six positions
            // 4-bit quotient -> C(Q)32,35
            // remainder -> C(A)

            {
                word36 tmp1 = cpu.rA & SIGN36; // A0

                cpu.rA <<= 3;       // Shift C(A) left three positions
                cpu.rA &= DMASK;    // keep to 36-bits

                word36 tmp36 = cpu.rA & MASK36;;
                word36 tmp36q = tmp36 / cpu.CY; // | C(A) | / C(Y) ->
                                            // 4-bit quotient plus remainder
                word36 tmp36r = tmp36 % cpu.CY;

                cpu.rQ <<= 6;       // Shift C(Q) left six positions
                cpu.rQ &= DMASK;

                cpu.rQ &= ~017;     // 4-bit quotient -> C(Q)32,35
                cpu.rQ |= (tmp36q & 017);

                cpu.rA = tmp36r;    // remainder -> C(A)

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
                // TODO: untested.

                word36 tmp1 = cpu.rA & SIGN36; // save A0

                word36 tmp36 = cpu.rA >> 1;
                cpu.rA = cpu.rA ^ tmp36;

                if (tmp1)
                    cpu.rA |= SIGN36;   // set A0
                else
                    cpu.rA &= ~SIGN36;  // reset A0

                SC_I_ZERO (cpu.rA == 0);  // If C(A) = 0, then ON;
                                                 // otherwise OFF
                SC_I_NEG (cpu.rA & SIGN36);   // If C(A)0 = 1, then ON; 
                                                 // otherwise OFF
            }
            break;

        /// REGISTER LOAD

        case 0230:  // lbar
            // C(Y)0,17 -> C(BAR)
            cpu.BAR.BASE = (GETHI(cpu.CY) >> 9) & 0777; // BAR.BASE is upper 9-bits
                                                //  (0-8)
            cpu.BAR.BOUND = GETHI(cpu.CY) & 0777;       // BAR.BOUND is next lower
                                                //  9-bits (9-17)
            break;

        /// PRIVILEGED INSTRUCTIONS

        /// Privileged - Register Load

        case 0674:  // lcpr
            // DPS8M interpratation
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

                   // a:AL39/cmr2  If either cache enable bit c or d changes
                   // from disable state to enable state, the entire cache is
                   // cleared.
                     uint csh1_on = GETBITS (cpu.CY, 1, 72 - 54);
                     uint csh2_on = GETBITS (cpu.CY, 1, 72 - 55);
                     //bool clear = (cpu.CMR.csh1_on == 0 && csh1_on != 0) ||
                                  //(cpu.CMR.csh1_on == 0 && csh1_on != 0);
                     cpu.CMR.csh1_on = csh1_on;
                     cpu.CMR.csh2_on = csh2_on;
                     //if (clear) // a:AL39/cmr2
                       //{
                       //}
                      // cpu.CMR.opnd_on = ; // DPS8, not DPS8M
                     cpu.CMR.inst_on = GETBITS (cpu.CY, 1, 72 - 57);
                     cpu.CMR.csh_reg = GETBITS (cpu.CY, 1, 72 - 59);
                     if (cpu.CMR.csh_reg)
                       sim_warn ("LCPR set csh_reg\n");
                      // cpu.CMR.str_asd = <ignored for lcpr>
                      // cpu.CMR.col_ful = <ignored for lcpr>
                      // cpu.CMR.rro_AB = GETBITS (cpu.CY, 1, 18);
                     cpu.CMR.bypass_cache = GETBITS (cpu.CY, 1, 72 - 68);
                     cpu.CMR.luf = GETBITS (cpu.CY, 2, 72 - 72);
                  }
                  break;

                case 04: // mode register
                  if (GETBITS (cpu.CY, 1, 35))
                    {
                      cpu.MR.cuolin = GETBITS (cpu.CY, 1, 18);
                      cpu.MR.solin = GETBITS (cpu.CY, 1, 19);
                      cpu.MR.sdpap = GETBITS (cpu.CY, 1, 20);
                      cpu.MR.separ = GETBITS (cpu.CY, 1, 21);
                      cpu.MR.tm = GETBITS (cpu.CY, 3, 23);
                      cpu.MR.vm = GETBITS (cpu.CY, 3, 26);
                      cpu.MR.hrhlt = GETBITS (cpu.CY, 1, 28);
                      cpu.MR.hrxfr = GETBITS (cpu.CY, 1, 29);
                      cpu.MR.ihr = GETBITS (cpu.CY, 1, 30);
                      cpu.MR.ihrrs = GETBITS (cpu.CY, 1, 31);
                      cpu.MR.mrgctl = GETBITS (cpu.CY, 1, 32);
                      cpu.MR.hexfp = GETBITS (cpu.CY, 1, 33);
                      //cpu.MR.emr = GETBITS (cpu.CY, 1, 35);
                    }
                  break;

                case 03: // DPS 8m 0's -> history
                  {
                    for (uint i = 0; i < N_HIST_SETS; i ++)
                      addHist (i, 0, 0);
                  }
                  break;

                case 07: // DPS 8m 1's -> history
                  {
                    for (uint i = 0; i < N_HIST_SETS; i ++)
                      addHist (i, MASK36, MASK36);
                  }
                  break;

                default:
                  doFault (FAULT_IPR, flt_ipr_ill_mod, "lcpr tag invalid");

              }
            break;

        case 0232:  // ldbr
            do_ldbr (cpu.Ypair);
            break;

        case 0637:  // ldt
            {
#ifdef REAL_TR
              word27 val = (cpu.CY >> 9) & MASK27;
              sim_debug (DBG_TRACE, & cpu_dev, "ldt TR %d (%o)\n", val, val);
              setTR (val);
#else
              cpu.rTR = (cpu.CY >> 9) & MASK27;
              sim_debug (DBG_TRACE, & cpu_dev, "ldt TR %d (%o)\n", cpu.rTR, cpu.rTR);
#endif
              // Undocumented feature. return to bce has been observed to
              // experience TRO while masked, setting the TR to -1, and
              // experiencing an unexpected TRo interrupt when unmasking.
              // Reset any pending TRO fault when the TR is loaded.
              clearTROFault ();
            }
            break;

        case 0257:  // lsdp
            // Not clear what the subfault should be; see Fault Register in
            //  AL39.
            doFault(FAULT_IPR, flt_ipr_ill_op, "lsdp is illproc on DPS8M");

        case 0613:  // rcu
            doRCU (); // never returns

        /// Privileged - Register Store

        case 0452:  // scpr
          {
            uint tag = (i -> tag) & MASK6;
            switch (tag)
              {
                case 000: // C(APU history register#1) -> C(Y-pair)
                  {
                    cpu.Ypair [0] = cpu.history [APU_HIST_REG] [cpu.history_cyclic[APU_HIST_REG]] [0];
                    cpu.Ypair [1] = cpu.history [APU_HIST_REG] [cpu.history_cyclic[APU_HIST_REG]] [1];
                    cpu.history_cyclic[APU_HIST_REG] = (cpu.history_cyclic[APU_HIST_REG] + 1) % N_HIST_SIZE;
                  }
                  break;

                case 001: // C(fault register) -> C(Y-pair)0,35
                          // 00...0 -> C(Y-pair)36,71
                  {
                    cpu.Ypair [0] = cpu.faultRegister [0];
                    cpu.Ypair [1] = cpu.faultRegister [1];
                    cpu.faultRegister [0] = 0;
                    cpu.faultRegister [1] = 0;
                  }
                  break;

                case 006: // C(mode register) -> C(Y-pair)0,35
                          // C(cache mode register) -> C(Y-pair)36,72
                  {
                    cpu.Ypair [0] = 0;
                    putbits36 (& cpu.Ypair [0], 18, 1, cpu.MR.cuolin);
                    putbits36 (& cpu.Ypair [0], 19, 1, cpu.MR.solin);
                    putbits36 (& cpu.Ypair [0], 20, 1, cpu.MR.sdpap);
                    putbits36 (& cpu.Ypair [0], 21, 1, cpu.MR.separ);
                    putbits36 (& cpu.Ypair [0], 22, 2, cpu.MR.tm);
                    putbits36 (& cpu.Ypair [0], 24, 2, cpu.MR.vm);
                    putbits36 (& cpu.Ypair [0], 28, 1, cpu.MR.hrhlt);
                    putbits36 (& cpu.Ypair [0], 29, 1, cpu.MR.hrxfr);
                    putbits36 (& cpu.Ypair [0], 30, 1, cpu.MR.ihr);
                    putbits36 (& cpu.Ypair [0], 31, 1, cpu.MR.ihrrs);
                    putbits36 (& cpu.Ypair [0], 32, 1, cpu.MR.mrgctl);
                    putbits36 (& cpu.Ypair [0], 33, 1, cpu.MR.hexfp);
                    putbits36 (& cpu.Ypair [0], 35, 1, cpu.MR.emr);
                    cpu.Ypair [1] = 0;
                    putbits36 (& cpu.Ypair [1], 36 - 36, 15,
                               cpu.CMR.cache_dir_address);
                    putbits36 (& cpu.Ypair [1], 51 - 36, 1, cpu.CMR.par_bit);
                    putbits36 (& cpu.Ypair [1], 52 - 36, 1, cpu.CMR.lev_ful);
                    putbits36 (& cpu.Ypair [1], 54 - 36, 1, cpu.CMR.csh1_on);
                    putbits36 (& cpu.Ypair [1], 55 - 36, 1, cpu.CMR.csh2_on);
                    putbits36 (& cpu.Ypair [1], 57 - 36, 1, cpu.CMR.inst_on);
                    putbits36 (& cpu.Ypair [1], 59 - 36, 1, cpu.CMR.csh_reg);
                    putbits36 (& cpu.Ypair [1], 60 - 36, 1, cpu.CMR.str_asd);
                    putbits36 (& cpu.Ypair [1], 61 - 36, 1, cpu.CMR.col_ful);
                    putbits36 (& cpu.Ypair [1], 62 - 36, 2, cpu.CMR.rro_AB);
                    putbits36 (& cpu.Ypair [1], 68 - 36, 1, cpu.CMR.bypass_cache);
                    putbits36 (& cpu.Ypair [1], 70 - 36, 2, cpu.CMR.luf);
                  }
                  break;

                case 010: // C(APU history register#2) -> C(Y-pair)
                  {
                    cpu.Ypair [0] = cpu.history [EAPU_HIST_REG] [cpu.history_cyclic[EAPU_HIST_REG]] [0];
                    cpu.Ypair [1] = cpu.history [EAPU_HIST_REG] [cpu.history_cyclic[EAPU_HIST_REG]] [1];
                    cpu.history_cyclic[EAPU_HIST_REG] = (cpu.history_cyclic[EAPU_HIST_REG] + 1) % N_HIST_SIZE;
                  }
                  break;

                case 020: // C(CU history register) -> C(Y-pair)
                  {
                    cpu.Ypair [0] = cpu.history [CU_HIST_REG] [cpu.history_cyclic[CU_HIST_REG]] [0];
                    cpu.Ypair [1] = cpu.history [CU_HIST_REG] [cpu.history_cyclic[CU_HIST_REG]] [1];
                    cpu.history_cyclic[CU_HIST_REG] = (cpu.history_cyclic[CU_HIST_REG] + 1) % N_HIST_SIZE;
                  }
                  break;

                case 040: // C(OU/DU history register) -> C(Y-pair)
                  {
                    cpu.Ypair [0] = cpu.history [DU_OU_HIST_REG] [cpu.history_cyclic[DU_OU_HIST_REG]] [0];
                    cpu.Ypair [1] = cpu.history [DU_OU_HIST_REG] [cpu.history_cyclic[DU_OU_HIST_REG]] [1];
                    cpu.history_cyclic[DU_OU_HIST_REG] = (cpu.history_cyclic[DU_OU_HIST_REG] + 1) % N_HIST_SIZE;
                  }
                  break;

                default:
                  {
                    doFault(FAULT_IPR, flt_ipr_ill_mod,
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
                scu2words (cpu.Yblock8);
              }
            else
              {
                // AL-39 behavior
                for (int i = 0; i < 8; i ++)
                  cpu.Yblock8 [i] = cpu.scu_data [i];
              }
            break;

        case 0154:  // sdbr
            do_sdbr (cpu.Ypair);
            break;

        case 0557:  // ssdp
          {
            // XXX The associative memory is ignored (forced to "no match")
            // during address preparation.

            // Level j is selected by C(TPR.CA)12,13
            uint level = (cpu.TPR.CA >> 4) & 02u;
#ifndef SPEED
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                cpu.Yblock16 [i] = 0;
#ifndef SPEED
                putbits36 (& cpu.Yblock16 [i], 0, 15,
                           cpu.SDWAM [toffset + i].POINTER);
                putbits36 (& cpu.Yblock16 [i], 27, 1,
                           cpu.SDWAM [toffset + i].F);
                putbits36 (& cpu.Yblock16 [i], 30, 6,
                           cpu.SDWAM [toffset + i].USE);
#endif
              }
#ifdef SPEED
            if (level == 0)
              {
                putbits36 (& cpu.Yblock16 [0], 0, 15,
                           cpu.SDWAM0.POINTER);
                putbits36 (& cpu.Yblock16 [0], 27, 1,
                           cpu.SDWAM0.F);
                putbits36 (& cpu.Yblock16 [0], 30, 6,
                           cpu.SDWAM0.USE);
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
                uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
                int scu_unit_num = query_scu_unit_num (currentRunningCPUnum, 
                                                       cpu_port_num);
                if (scu_unit_num < 0)
                  {
                    sim_warn ("rmcm to non-existent controller on cpu %d port %d\n", currentRunningCPUnum, cpu_port_num);
                    break;
                  }
//sim_printf ("calling scu_rmcm iwb %012llo CA %08o cpu port num %d scu num %d cpu num %d\n", cpu.cu . IWB, cpu.TPR.CA, cpu_port_num, scu_unit_num, currentRunningCPUnum);
                t_stat rc = scu_rmcm (scu_unit_num, currentRunningCPUnum, & cpu.rA, & cpu.rQ);
                if (rc)
                    return rc;
                SC_I_ZERO (cpu.rA == 0);
                SC_I_NEG (cpu.rA & SIGN36);
            }
            break;

        case 0413:  // rscr
            {
              // For the rscr instruction, the first 2 or 3 bits of the addr
              // field of the instruction are used to specify which SCU.
              // 2 bits for the DPS8M.

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

// Looking at privileged_mode_ut.alm, shift 10 bits...
              uint cpu_port_num = (cpu.TPR.CA >> 10) & 03;
              int scu_unit_num = query_scu_unit_num (currentRunningCPUnum, cpu_port_num);

              if (scu_unit_num < 0)
                {
                  if (cpu_port_num == 0)
                    putbits36 (& cpu.faultRegister [0], 16, 4, 010);
                  else if (cpu_port_num == 1)
                    putbits36 (& cpu.faultRegister [0], 20, 4, 010);
                  else if (cpu_port_num == 2)
                    putbits36 (& cpu.faultRegister [0], 24, 4, 010);
                  else
                    putbits36 (& cpu.faultRegister [0], 28, 4, 010);
                  doFault (FAULT_CMD, flt_cmd_not_control, "(rscr)");
                }

              t_stat rc = scu_rscr (scu_unit_num, currentRunningCPUnum,
                                    cpu.iefpFinalAddress & MASK15, & cpu.rA, & cpu.rQ);
              if (rc)
                return rc;
            }

            break;

        case 0231:  // rsw
          {
            //if (i -> tag == TD_DL)
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
#if 1
                static unsigned char PROM [1024];
                memset (PROM, ' ', sizeof (PROM));
                sprintf ((char *) PROM, "%13s%13d%8s",
                  "DPS8/70M Emul",  //  0-12 CPU Model number
                  cpu.switches.serno, // 13-25 CPU Serial number
                  "20160304");      // 26-33 Ship date (YYMMDD)

#else
                static unsigned char PROM [1024] =
                  "DPS8/70M Emul" //  0-12 CPU Model number
                  "1            " // 13-25 CPU Serial number
                  "140730  "      // 26-33 Ship date (YYMMDD)
                  ;
#endif
                cpu.rA = PROM [cpu.TPR.CA & 1023];
                break;
              }
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
                  cpu.rA |= (cpu.switches.assignment  [0] & 07LL)  <<
                        (35 -  (2 +  0));
                  cpu.rA |= (cpu.switches.enable      [0] & 01LL)  <<
                        (35 -  (3 +  0));
                  cpu.rA |= (cpu.switches.init_enable [0] & 01LL)  <<
                        (35 -  (4 +  0));
                  cpu.rA |= (cpu.switches.interlace   [0] ? 1LL:0LL)  <<
                        (35 -  (5 +  0));
                  cpu.rA |= (cpu.switches.store_size  [0] & 07LL)  <<
                        (35 -  (8 +  0));

                  cpu.rA |= (cpu.switches.assignment  [1] & 07LL)  <<
                        (35 -  (2 +  9));
                  cpu.rA |= (cpu.switches.enable      [1] & 01LL)  <<
                        (35 -  (3 +  9));
                  cpu.rA |= (cpu.switches.init_enable [1] & 01LL)  <<
                        (35 -  (4 +  9));
                  cpu.rA |= (cpu.switches.interlace   [1] ? 1LL:0LL)  <<
                        (35 -  (5 +  9));
                  cpu.rA |= (cpu.switches.store_size  [1] & 07LL)  <<
                        (35 -  (8 +  9));

                  cpu.rA |= (cpu.switches.assignment  [2] & 07LL)  <<
                        (35 -  (2 + 18));
                  cpu.rA |= (cpu.switches.enable      [2] & 01LL)  <<
                        (35 -  (3 + 18));
                  cpu.rA |= (cpu.switches.init_enable [2] & 01LL)  <<
                        (35 -  (4 + 18));
                  cpu.rA |= (cpu.switches.interlace   [2] ? 1LL:0LL)  <<
                        (35 -  (5 + 18));
                  cpu.rA |= (cpu.switches.store_size  [2] & 07LL)  <<
                        (35 -  (8 + 18));

                  cpu.rA |= (cpu.switches.assignment  [3] & 07LL)  <<
                        (35 -  (2 + 27));
                  cpu.rA |= (cpu.switches.enable      [3] & 01LL)  <<
                        (35 -  (3 + 27));
                  cpu.rA |= (cpu.switches.init_enable [3] & 01LL)  <<
                        (35 -  (4 + 27));
                  cpu.rA |= (cpu.switches.interlace   [3] ? 1LL:0LL)  <<
                        (35 -  (5 + 27));
                  cpu.rA |= (cpu.switches.store_size  [3] & 07LL)  <<
                        (35 -  (8 + 27));
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

                  cpu.rA = 0;
                  cpu.rA |= (cpu.switches.interlace [0] == 2 ? 1LL : 0LL) << (35- 0);
                  cpu.rA |= (cpu.switches.interlace [1] == 2 ? 1LL : 0LL) << (35- 1);
                  cpu.rA |= (cpu.switches.interlace [2] == 2 ? 1LL : 0LL) << (35- 2);
                  cpu.rA |= (cpu.switches.interlace [3] == 2 ? 1LL : 0LL) << (35- 3);
                  cpu.rA |= (0b01L)  /* DPS8M */                          << (35- 5);
                  cpu.rA |= (cpu.switches.FLT_BASE & 0177LL)              << (35-12);
                  cpu.rA |= (0b1L)                                        << (35-13);
                  cpu.rA |= (0b0000L)                                     << (35-17);
                  //cpu.rA |= (0b111L)                                    << (35-20);
                  cpu.rA |= (0b1L)                                        << (35-18);  //BCD option
                  cpu.rA |= (0b1L)                                        << (35-19);  //DPS option
                  cpu.rA |= (0b1L)                                        << (35-20);  //8K cache installed
                  cpu.rA |= (0b00L)                                       << (35-22);
                  cpu.rA |= (0b1L)  /* DPS8M */                           << (35-23);
                  cpu.rA |= (cpu.switches.proc_mode & 01LL)               << (35-24);
                  cpu.rA |= (0b0L)                                        << (35-25); // new product line (CPL/NPL)
                  cpu.rA |= (0b000L)                                      << (35-28);
                  cpu.rA |= (cpu.switches.proc_speed & 017LL)             << (35-32);
                  cpu.rA |= (cpu.switches.cpu_num & 07LL)                 << (35-35);
                  break;

                case 3: // configuration switches for ports E-H, which
                        // the DPS didn't have (SCUs had more memory, so
                        // fewer SCUs were needed
                  cpu.rA = 0;
                  break;

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
                  cpu.rA |= (cpu.switches.interlace [0] == 2 ? 1LL : 0LL) << (35-13);
                  cpu.rA |= (cpu.switches.interlace [1] == 2 ? 1LL : 0LL) << (35-15);
                  cpu.rA |= (cpu.switches.interlace [2] == 2 ? 1LL : 0LL) << (35-17);
                  cpu.rA |= (cpu.switches.interlace [3] == 2 ? 1LL : 0LL) << (35-19);
                  break;

                default:
                  // XXX Guessing values; also don't know if this is actually
                  //  a fault
                  doFault(FAULT_IPR, flt_ipr_ill_mod, "Illegal register select value");
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
                doFault (FAULT_ONC, flt_onc_nem, "(cioc)");
              }
            int scu_unit_num = query_scu_unit_num (currentRunningCPUnum, cpu_port_num);
            if (scu_unit_num < 0)
              {
                doFault (FAULT_ONC, flt_onc_nem, "(cioc)");
              }
            uint scu_port_num = cpu.CY & MASK3;
            scu_cioc ((uint) scu_unit_num, scu_port_num);
          }
          break;

        case 0553:  // smcm
            {
                // C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor)
                // specify which processor port (i.e., which system
                // controller) is used.
                uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
                int scu_unit_num = query_scu_unit_num (currentRunningCPUnum,
                                                       cpu_port_num);
#if 0 // not on 4MW
                if (scu_unit_num < 0)
                  {
                    if (cpu_port_num == 0)
                      putbits36 (& cpu.faultRegister [0], 16, 4, 010);
                    else if (cpu_port_num == 1)
                      putbits36 (& cpu.faultRegister [0], 20, 4, 010);
                    else if (cpu_port_num == 2)
                      putbits36 (& cpu.faultRegister [0], 24, 4, 010);
                    else
                      putbits36 (& cpu.faultRegister [0], 28, 4, 010);
                    doFault (FAULT_CMD, flt_cmd_not_control, "(smcm)");
                  }
#endif
                if (scu_unit_num < 0)
                  {
                    sim_warn ("smcm to non-existent controller on cpu %d port %d\n", currentRunningCPUnum, cpu_port_num);
                    break;
                  }
//sim_printf ("calling scu_smcm iwb %012llo CA %08o cpu port num %d scu num %d cpu num %d\n", cpu.cu . IWB, cpu.TPR.CA, cpu_port_num, scu_unit_num, currentRunningCPUnum);
                t_stat rc = scu_smcm (scu_unit_num, currentRunningCPUnum, cpu.rA, cpu.rQ);
                if (rc)
                    return rc;
            }
            break;

        case 0451:  // smic
          {
            // For the smic instruction, the first 2 or 3 bits of the addr
            // field of the instruction are used to specify which SCU.
            // 2 bits for the DPS8M.
            //int scu_unit_num = getbits36 (TPR.CA, 0, 2);

            // C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor)
            // specify which processor port (i.e., which system
            // controller) is used.
            uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
            int scu_unit_num = query_scu_unit_num (currentRunningCPUnum, cpu_port_num);

            if (scu_unit_num < 0)
              {
                if (cpu_port_num == 0)
                  putbits36 (& cpu.faultRegister [0], 16, 4, 010);
                else if (cpu_port_num == 1)
                  putbits36 (& cpu.faultRegister [0], 20, 4, 010);
                else if (cpu_port_num == 2)
                  putbits36 (& cpu.faultRegister [0], 24, 4, 010);
                else
                  putbits36 (& cpu.faultRegister [0], 28, 4, 010);
                doFault (FAULT_CMD, flt_cmd_not_control, "(smic)");
              }
            t_stat rc = scu_smic (scu_unit_num, currentRunningCPUnum, cpu_port_num, cpu.rA);
            if (rc)
              return rc;
          }
          break;


        case 0057:  // sscr
          {
            //uint cpu_port_num = (cpu.TPR.CA >> 15) & 03;
            // Looking at privileged_mode_ut.alm, shift 10 bits...
            uint cpu_port_num = (cpu.TPR.CA >> 10) & 03;
            int scu_unit_num = query_scu_unit_num (currentRunningCPUnum, cpu_port_num);
//sim_printf ("sscr CA %08o cpu port %o scu unit %o\n", cpu.TPR.CA, cpu_port_num, scu_unit_num);
            if (scu_unit_num < 0)
              {
                if (cpu_port_num == 0)
                  putbits36 (& cpu.faultRegister [0], 16, 4, 010);
                else if (cpu_port_num == 1)
                  putbits36 (& cpu.faultRegister [0], 20, 4, 010);
                else if (cpu_port_num == 2)
                  putbits36 (& cpu.faultRegister [0], 24, 4, 010);
                else
                  putbits36 (& cpu.faultRegister [0], 28, 4, 010);
                doFault (FAULT_CMD, flt_cmd_not_control, "(sscr)");
              }
            t_stat rc = scu_sscr (scu_unit_num, currentRunningCPUnum, cpu_port_num, cpu.iefpFinalAddress & MASK15, cpu.rA, cpu.rQ);

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

            if ((! cpu.switches.tro_enable) &&
                (! sample_interrupts ()) &&
                (sim_qcount () == 0))  // XXX If clk_svc is implemented it will
                                     // break this logic
              {
                sim_printf ("DIS@0%06o with no interrupts pending and"
                            " no events in queue\n", cpu.PPR.IC);
                sim_printf("\nsimCycles = %lld\n", sim_timell ());
                sim_printf("\ncpuCycles = %lld\n", sys_stats.total_cycles);
                //stop_reason = STOP_DIS;
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
            // Currently, the only G7 fault we recognize is TRO, so
            // this code suffices for "all other G7 faults."
            if (GET_I (cpu.cu.IWB) ? bG7PendingNoTRO () : bG7Pending ())
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
            doFault(FAULT_IPR, flt_ipr_ill_op, "Illegal instruction");
    }
    return SCPE_OK;
}

// CANFAULT
static t_stat DoEISInstruction (void)
{
    DCDstruct * i = & cpu.currentInstruction;
    int32 opcode = i->opcode;

    switch (opcode)
    {
        /// TRANSFER INSTRUCTIONS

        case 0604:  // tmoz
            // If negative or zero indicator ON then
            // C(TPR.CA) -> C(PPR.IC)
            // C(TPR.TSR) -> C(PPR.PSR)
            if (cpu.cu.IR & (I_NEG | I_ZERO))
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0605:  // tpnz
            // If negative and zero indicators are OFF then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (!(cpu.cu.IR & I_NEG) && !(cpu.cu.IR & I_ZERO))
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0601:  // trtf
            // If truncation indicator OFF then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (!TST_I_TRUNC)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0600:  // trtn
            // If truncation indicator ON then
            //  C(TPR.CA) -> C(PPR.IC)
            //  C(TPR.TSR) -> C(PPR.PSR)
            if (TST_I_TRUNC)
            {
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

                CLR_I_TRUNC;

#ifdef RALR_FIX_0
                readOperands ();
#endif

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
                cpu.PPR.IC = cpu.TPR.CA;
                cpu.PPR.PSR = cpu.TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        /// POINTER REGISTER INSTRUCTIONS

        /// Pointer Register Data Movement Load

        case 0310:  // easp1
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[1].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0312:  // easp3
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[3].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0330:  // easp5
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[5].SNR = cpu.TPR.CA & MASK15;
            break;
        case 0332:  // easp7
            // C(TPR.CA) -> C(PRn.SNR)
            cpu.PR[7].SNR = cpu.TPR.CA & MASK15;
            break;

        case 0311:  // eawp1
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[1].WORDNO = cpu.TPR.CA;
            cpu.PR[1].BITNO = cpu.TPR.TBR;
            break;
        case 0313:  // eawp3
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[3].WORDNO = cpu.TPR.CA;
            cpu.PR[3].BITNO = cpu.TPR.TBR;
            break;
        case 0331:  // eawp5
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[5].WORDNO = cpu.TPR.CA;
            cpu.PR[5].BITNO = cpu.TPR.TBR;
            break;
        case 0333:  // eawp7
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.CA) -> C(PRn.WORDNO)
            //  C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[7].WORDNO = cpu.TPR.CA;
            cpu.PR[7].BITNO = cpu.TPR.TBR;
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
            cpu.PR[0].BITNO = 0;
            break;
        case 0352:  // epbp2
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[2].RNR = cpu.TPR.TRR;
            cpu.PR[2].SNR = cpu.TPR.TSR;
            cpu.PR[2].WORDNO = 0;
            cpu.PR[2].BITNO = 0;
            break;
        case 0370:  // epbp4
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[4].RNR = cpu.TPR.TRR;
            cpu.PR[4].SNR = cpu.TPR.TSR;
            cpu.PR[4].WORDNO = 0;
            cpu.PR[4].BITNO = 0;
            break;
        case 0372:  // epbp6
            // For n = 0, 1, ..., or 7 as determined by operation code
            //  C(TPR.TRR) -> C(PRn.RNR)
            //  C(TPR.TSR) -> C(PRn.SNR)
            //  00...0 -> C(PRn.WORDNO)
            //  0000 -> C(PRn.BITNO)
            cpu.PR[6].RNR = cpu.TPR.TRR;
            cpu.PR[6].SNR = cpu.TPR.TSR;
            cpu.PR[6].WORDNO = 0;
            cpu.PR[6].BITNO = 0;
            break;

        case 0351:  // epp1
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[1].RNR = cpu.TPR.TRR;
            cpu.PR[1].SNR = cpu.TPR.TSR;
            cpu.PR[1].WORDNO = cpu.TPR.CA;
            cpu.PR[1].BITNO = cpu.TPR.TBR;
            break;
        case 0353:  // epp3
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[3].RNR = cpu.TPR.TRR;
            cpu.PR[3].SNR = cpu.TPR.TSR;
            cpu.PR[3].WORDNO = cpu.TPR.CA;
            cpu.PR[3].BITNO = cpu.TPR.TBR;
            break;
        case 0371:  // epp5
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[5].RNR = cpu.TPR.TRR;
            cpu.PR[5].SNR = cpu.TPR.TSR;
            cpu.PR[5].WORDNO = cpu.TPR.CA;
            cpu.PR[5].BITNO = cpu.TPR.TBR;
            break;
        case 0373:  // epp7
            // For n = 0, 1, ..., or 7 as determined by operation code
            //   C(TPR.TRR) -> C(PRn.RNR)
            //   C(TPR.TSR) -> C(PRn.SNR)
            //   C(TPR.CA) -> C(PRn.WORDNO)
            //   C(TPR.TBR) -> C(PRn.BITNO)
            cpu.PR[7].RNR = cpu.TPR.TRR;
            cpu.PR[7].SNR = cpu.TPR.TSR;
            cpu.PR[7].WORDNO = cpu.TPR.CA;
            cpu.PR[7].BITNO = cpu.TPR.TBR;
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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[6].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[6].RNR) << 15;
            cpu.Ypair[1] = 0;

            //fWrite2(i, TPR.CA, cpu.Ypair[0], cpu.Ypair[1], OperandWrite, rTAG);

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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[1].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[1].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[1].WORDNO << 18;
            cpu.Ypair[1]|= (word36) cpu.PR[1].BITNO << 9;
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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[3].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[3].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[3].WORDNO << 18;
            cpu.Ypair[1]|= (word36) cpu.PR[3].BITNO << 9;
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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[5].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[5].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[5].WORDNO << 18;
            cpu.Ypair[1]|= (word36) cpu.PR[5].BITNO << 9;
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
            cpu.Ypair[0] = 043;
            cpu.Ypair[0] |= ((word36) cpu.PR[7].SNR) << 18;
            cpu.Ypair[0] |= ((word36) cpu.PR[7].RNR) << 15;

            cpu.Ypair[1] = (word36) cpu.PR[7].WORDNO << 18;
            cpu.Ypair[1]|= (word36) cpu.PR[7].BITNO << 9;
            break;

        /// Ring Alarm Register

        case 0754:  // sra
            // 00...0 -> C(Y)0,32
            // C(RALR) -> C(Y)33,35

            cpu.CY = (word36)cpu.rRALR;

            break;

        /// PRIVILEGED INSTRUCTIONS

        /// Privileged - Register Load

        case 0257:  // lptp
            // Not clear what the subfault should be; see Fault Register in
            // AL39.
            doFault(FAULT_IPR, flt_ipr_ill_proc, "lptp is illproc on DPS8M");

        case 0173:  // lptr
            // Not clear what the subfault should be; see Fault Register in
            // AL39.
            doFault(FAULT_IPR, flt_ipr_ill_proc, "lptr is illproc on DPS8M");

        case 0774:  // lra
            cpu.rRALR = cpu.CY & MASK3;
            sim_debug (DBG_TRACE, & cpu_dev, "RALR set to %o\n", cpu.rRALR);
            break;

        case 0232:  // lsdr
            // Not clear what the subfault should be; see Fault Register in
            // AL39.
            doFault(FAULT_IPR, flt_ipr_ill_proc, "lsdr is illproc on DPS8M");

        case 0557:  // sptp
          {
// XXX The associative memory is ignored (forced to "no match") during address
// preparation.

            // Level j is selected by C(TPR.CA)12,13
            uint level = (cpu.TPR.CA >> 4) & 02u;
#ifndef SPEED
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                cpu.Yblock16 [i] = 0;
#ifndef SPEED
                putbits36 (& cpu.Yblock16 [i],  0, 15,
                           cpu.PTWAM [toffset + i].POINTER);
                putbits36 (& cpu.Yblock16 [i], 15, 12,
                           cpu.PTWAM [toffset + i].PAGENO);
                putbits36 (& cpu.Yblock16 [i], 27,  1,
                           cpu.PTWAM [toffset + i].F);
                putbits36 (& cpu.Yblock16 [i], 30,  6,
                           cpu.PTWAM [toffset + i].USE);
#endif
              }
#ifdef SPEED
            if (level == 0)
              {
                putbits36 (& cpu.Yblock16 [0],  0, 15,
                           cpu.PTWAM0.POINTER);
                putbits36 (& cpu.Yblock16 [0], 15, 12,
                           cpu.PTWAM0.PAGENO);
                putbits36 (& cpu.Yblock16 [0], 27,  1,
                           cpu.PTWAM0.F);
                putbits36 (& cpu.Yblock16 [0], 30,  6,
                           cpu.PTWAM0.USE);
              }
#endif
          }
          break;

        case 0154:  // sptr
          {
// XXX The associative memory is ignored (forced to "no match") during address
// preparation.

            // Level j is selected by C(TPR.CA)12,13
            uint level = (cpu.TPR.CA >> 4) & 02u;
#ifndef SPEED
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                cpu.Yblock16 [i] = 0;
#ifndef SPEED
                putbits36 (& cpu.Yblock16 [i], 0, 13, cpu.PTWAM [toffset + i].ADDR);
                putbits36 (& cpu.Yblock16 [i], 29, 1, cpu.PTWAM [toffset + i].M);
#endif
              }
#ifdef SPEED
            if (level == 0)
              putbits36 (& cpu.Yblock16 [0], 0, 13, cpu.PTWAM0.ADDR);
#endif
          }
          break;

        case 0254:  // ssdr
          {
// XXX The associative memory is ignored (forced to "no match") during address
// preparation.

            // Level j is selected by C(TPR.CA)12,13
            uint level = (cpu.TPR.CA >> 4) & 02u;
#ifndef SPEED
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                cpu.Yblock32 [i * 2] = 0;
#ifndef SPEED
                putbits36 (& cpu.Yblock32 [i * 2],  0, 23,
                           cpu.SDWAM [toffset + i].ADDR);
                putbits36 (& cpu.Yblock32 [i * 2], 24,  3,
                           cpu.SDWAM [toffset + i].R1);
                putbits36 (& cpu.Yblock32 [i * 2], 27,  3,
                           cpu.SDWAM [toffset + i].R2);
                putbits36 (& cpu.Yblock32 [i * 2], 30,  3,
                           cpu.SDWAM [toffset + i].R3);
#endif
                cpu.Yblock32 [i * 2 + 1] = 0;
#ifndef SPEED
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 37 - 36, 14,
                           cpu.SDWAM [toffset + i].BOUND);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 51 - 36,  1,
                           cpu.SDWAM [toffset + i].R);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 52 - 36,  1,
                           cpu.SDWAM [toffset + i].E);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 53 - 36,  1,
                           cpu.SDWAM [toffset + i].W);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 54 - 36,  1,
                           cpu.SDWAM [toffset + i].P);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 55 - 36,  1,
                           cpu.SDWAM [toffset + i].U);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 56 - 36,  1,
                           cpu.SDWAM [toffset + i].G);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 57 - 36,  1,
                           cpu.SDWAM [toffset + i].C);
                putbits36 (& cpu.Yblock32 [i * 2 + 1], 58 - 36, 14,
                           cpu.SDWAM [toffset + i].CL);
#endif
              }
#ifdef SPEED
            if (level == 0)
              {
                putbits36 (& cpu.Yblock32 [0],  0, 23,
                           cpu.SDWAM0.ADDR);
                putbits36 (& cpu.Yblock32 [0], 24,  3,
                           cpu.SDWAM0.R1);
                putbits36 (& cpu.Yblock32 [0], 27,  3,
                           cpu.SDWAM0.R2);
                putbits36 (& cpu.Yblock32 [0], 30,  3,
                           cpu.SDWAM0.R3);
                putbits36 (& cpu.Yblock32 [0], 37 - 36, 14,
                           cpu.SDWAM0.BOUND);
                putbits36 (& cpu.Yblock32 [1], 51 - 36,  1,
                           cpu.SDWAM0.R);
                putbits36 (& cpu.Yblock32 [1], 52 - 36,  1,
                           cpu.SDWAM0.E);
                putbits36 (& cpu.Yblock32 [1], 53 - 36,  1,
                           cpu.SDWAM0.W);
                putbits36 (& cpu.Yblock32 [1], 54 - 36,  1,
                           cpu.SDWAM0.P);
                putbits36 (& cpu.Yblock32 [1], 55 - 36,  1,
                           cpu.SDWAM0.U);
                putbits36 (& cpu.Yblock32 [1], 56 - 36,  1,
                           cpu.SDWAM0.G);
                putbits36 (& cpu.Yblock32 [1], 57 - 36,  1,
                           cpu.SDWAM0.C);
                putbits36 (& cpu.Yblock32 [1], 58 - 36, 14,
                           cpu.SDWAM0.CL);

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

                if (getbits36 (cpu.CY, 23, 1) != 0)
                  doFault (FAULT_IPR, flt_ipr_ill_proc, "aarn C(Y)23 != 0");

                uint32 n = opcode & 07;  // get

                // C(Y)0,17 -> C(ARn.WORDNO)
                cpu.AR[n].WORDNO = GETHI(cpu.CY);

                //int TA = (int)bitfieldExtract36(cpu.CY, 13, 2); // C(Y) 21-22
                //int CN = (int)bitfieldExtract36(cpu.CY, 15, 3); // C(Y) 18-20
                uint TA = getbits36 (cpu.CY, 21, 2);
                uint CN = getbits36 (cpu.CY, 18, 3);

                switch(TA)
                {
                    case CTA4:  // 2
                        // If C(Y)21,22 = 10 (TA code = 2), then
                        //   C(Y)18,20 / 2 -> C(ARn.CHAR)
                        //   4 * (C(Y)18,20)mod2 + 1 -> C(ARn.BITNO)
                        SET_AR_CHAR_BIT (n,  CN / 2, 4 * (CN % 2) + 1);
                        break;

                    case CTA6:  // 1
                        // If C(Y)21,22 = 01 (TA code = 1) and C(Y)18,20 = 110
                        // or 111 an illegal procedure fault occurs.
                        if (CN > 5)
                          doFault (FAULT_IPR, flt_ipr_ill_proc, "aarn TN > 5");

                        // If C(Y)21,22 = 01 (TA code = 1), then
                        //   (6 * C(Y)18,20) / 9 -> C(ARn.CHAR)
                        //   (6 * C(Y)18,20)mod9 -> C(ARn.BITNO)
                        SET_AR_CHAR_BIT (n, (6 * CN) / 9, (6 * CN) % 9);
                        break;

                    case CTA9:  // 0
                        // If C(Y)21,22 = 00 (TA code = 0), then
                        //   C(Y)18,19 -> C(ARn.CHAR)
                        //   0000 -> C(ARn.BITNO)
                        // remember, 9-bit CN's are funky
                        SET_AR_CHAR_BIT (n, (CN >> 1), 0);
                        break;

                    case CTAILL: // 3
                        // If C(Y)21,22 = 11 (TA code = 3) an illegal procedure
                        // fault occurs.
                        doFault (FAULT_IPR, flt_ipr_ill_proc, "aarn TA = 3");
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
            // For n = 0, 1, ..., or 7 as determined by operation code
            //    C(Y)0,23 -> C(ARn)

	  // a:AL39/ar1 According to ISOLTS ps805, the BITNO data is stored
	  // in BITNO format, not CHAR/BITNO.
            {
                uint32 n = opcode & 07;  // get n
#ifdef ISOLTS_BITNO
                cpu.AR[n].WORDNO = GETHI(cpu.CY);
                SET_AR_CHAR_BIT (n, (word6)bitfieldExtract36(cpu.CY, 12, 4),
                                 (word2)bitfieldExtract36(cpu.CY, 16, 2));
#else
                cpu.AR[n].WORDNO = getbits36 (cpu.CY, 0, 18);
                cpu.AR[n].BITNO = getbits36 (cpu.CY, 18, 6);
#endif
            }
            break;

        case 0463:  // lareg - Load Address Registers

	  // a:AL39/ar1 According to ISOLTS ps805, the BITNO data is stored
	  // in BITNO format, not CHAR/BITNO.
            for(uint32 n = 0 ; n < 8 ; n += 1)
            {
                word36 tmp36 = cpu.Yblock8[n];

#ifdef ISOLTS_BITNO
                cpu.AR[n].WORDNO = GETHI(tmp36);
                SET_AR_CHAR_BIT (n, (word6)bitfieldExtract36(tmp36, 12, 4),
                                 (word2)bitfieldExtract36(tmp36, 16, 2));
#else
                cpu.AR[n].WORDNO = getbits36 (tmp36, 0, 18);
                cpu.AR[n].BITNO = getbits36 (tmp36, 18, 6);
#endif
            }
            break;

        case 0467:  // lpl - Load Pointers and Lengths
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
sim_printf ("NARn CY %012llo\n", cpu.CY);
                // For n = 0, 1, ..., or 7 as determined by operation code

                uint32 n = opcode & 07;  // get

                // C(Y)0,17 -> C(ARn.WORDNO)
                cpu.AR[n].WORDNO = GETHI(cpu.CY);

                //int TN = (int)bitfieldExtract36(cpu.CY, 13, 1); // C(Y) 21
                //int CN = (int)bitfieldExtract36(cpu.CY, 15, 3); // C(Y) 18-20
                uint TN = getbits36 (cpu.CY, 21, 1); // C(Y) 21
                uint CN = getbits36 (cpu.CY, 18, 3); // C(Y) 18-20

sim_printf ("NARn CY %012llo TN %o CN %o\n", cpu.CY, TN, CN);
                switch(TN)
                {
                    case CTN4:   // 1
                        // If C(Y)21 = 1 (TN code = 1), then
                        //   (C(Y)18,20) / 2 -> C(ARn.CHAR)
                        //   4 * (C(Y)18,20)mod2 + 1 -> C(ARn.BITNO)
sim_printf ("NARn4 CHAR %o %d. BIT %o %d.\n", CN/2, CN/2, 4 * (CN % 2) + 1, 4 * (CN % 2) + 1);
                        SET_AR_CHAR_BIT (n,  CN / 2, 4 * (CN % 2) + 1);
                        break;

                    case CTN9:  // 0
                        // If C(Y)21 = 0 (TN code = 0) and C(Y)20 = 1 an
                        // illegal procedure fault occurs.
                        if ((CN & 1) != 0)
                          doFault (FAULT_IPR, flt_ipr_ill_proc, "narn N9 and CN odd");
                        // If C(Y)21 = 0 (TN code = 0), then
                        //   C(Y)18,20 -> C(ARn.CHAR)
                        //   0000 -> C(ARn.BITNO)
sim_printf ("NARn9 CHAR %o %d. BIT %o %d.\n", CN, CN, 0, 0);
                        SET_AR_CHAR_BIT (n, CN, 0);
                        break;
                }
sim_printf ("NARn BITNO %o\n", cpu.PR[n].BITNO);
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

                int TA = (int)bitfieldExtract36(cpu.CY, 13, 2); // C(Y) 21,22

                // If C(Y)21,22 = 11 (TA code = 3) or C(Y)23 = 1 (unused bit),
                // an illegal procedure fault occurs.
                if (TA == 03)
                  doFault (FAULT_IPR, flt_ipr_ill_proc, "ARAn tag == 3");
                if (getbits36 (cpu.CY, 23, 1) != 0)
                  doFault (FAULT_IPR, flt_ipr_ill_proc, "ARAn b23 == 1");

                uint32 n = opcode & 07;  // get
                // For n = 0, 1, ..., or 7 as determined by operation code

                // C(ARn.WORDNO) -> C(Y)0,17
                cpu.CY = bitfieldInsert36(cpu.CY, cpu.AR[n].WORDNO & MASK18, 18, 18);

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
                        cpu.CY = bitfieldInsert36(cpu.CY, CN & MASK3, 15, 3);
                        break;

                    case CTA6:  // 1
                        // If C(Y)21,22 = 01 (TA code = 1), then
                        // (9 * C(ARn.CHAR) + C(ARn.BITNO)) / 6 -> C(Y)18,20
                        CN = (9 * GET_AR_CHAR (n) + GET_AR_BITNO (n)) / 6;
                        cpu.CY = bitfieldInsert36(cpu.CY, CN & MASK3, 15, 3);
                        break;

                    case CTA9:  // 0
                        // If C(Y)21,22 = 00 (TA code = 0), then
                        //   C(ARn.CHAR) -> C(Y)18,19
                        //   0 -> C(Y)20
                        cpu.CY = bitfieldInsert36(cpu.CY,          0, 15, 1);
                        cpu.CY = bitfieldInsert36(cpu.CY, GET_AR_CHAR (n) & MASK2, 16, 2);
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
                uint32 n = opcode & 07;  // get register #

                // The Numeric descriptor is fetched from Y and C(Y)21,22 (TA
                // field) is examined to determine the data type described.

                int TN = (int)bitfieldExtract36(cpu.CY, 14, 1); // C(Y) 21

                // For n = 0, 1, ..., or 7 as determined by operation code
                // C(ARn.WORDNO) -> C(Y)0,17
                cpu.CY = bitfieldInsert36(cpu.CY, cpu.AR[n].WORDNO & MASK18, 18, 18);

                int CN = 0;
                switch(TN)
                {
                    case CTN4:  // 1
                        // If C(Y)21 = 1 (TN code = 1) then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO) - 1) / 4 ->
                        //     C(Y)18,20
                        CN = (9 * GET_AR_CHAR (n) + GET_AR_BITNO (n) - 1) / 4;
                        cpu.CY = bitfieldInsert36(cpu.CY, CN & MASK3, 15, 3);
                        break;

                    case CTN9:  // 0
                        // If C(Y)21 = 0 (TN code = 0), then
                        //   C(ARn.CHAR) -> C(Y)18,19
                        //   0 -> C(Y)20
                        cpu.CY = bitfieldInsert36(cpu.CY,          0, 15, 1);
                        cpu.CY = bitfieldInsert36(cpu.CY, GET_AR_CHAR (n) & MASK2, 16, 2);
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

	  // a:AL39/ar1 According to ISOLTS ps805, the BITNO data is stored
	  // in BITNO format, not CHAR/BITNO.
            {
                uint32 n = opcode & 07;  // get n
#ifdef ISOLTS_BITNO
                cpu.CY = bitfieldInsert36(cpu.CY, cpu.AR[n].WORDNO & MASK18, 18, 18);
                cpu.CY = bitfieldInsert36(cpu.CY, GET_AR_BITNO (n) & MASK4,  12,  4);
                cpu.CY = bitfieldInsert36(cpu.CY, GET_AR_CHAR (n) & MASK2,   16,  2);
#else
                putbits36 (& cpu.CY,  0, 18, cpu.PR[n].WORDNO);
                putbits36 (& cpu.CY, 18,  6, cpu.PR[n].BITNO);
#endif
            }
            break;

        case 0443:  // sareg - Store Address Registers
	  // a:AL39/ar1 According to ISOLTS ps805, the BITNO data is stored
	  // in BITNO format, not CHAR/BITNO.
            memset(cpu.Yblock8, 0, sizeof(cpu.Yblock8));
            for(uint32 n = 0 ; n < 8 ; n += 1)
            {
                word36 arx = 0;
#ifdef ISOLTS_BITNO
                arx = bitfieldInsert36(arx, cpu.AR[n].WORDNO & MASK18, 18, 18);
                arx = bitfieldInsert36(arx, GET_AR_BITNO (n) & MASK4,  12,  4);
                arx = bitfieldInsert36(arx, GET_AR_CHAR (n) & MASK2,   16,  2);
#else
                putbits36 (& arx,  0, 18, cpu.PR[n].WORDNO);
                putbits36 (& arx, 18,  6, cpu.PR[n].BITNO);
#endif
                cpu.Yblock8[n] = arx;
            }
            break;

        case 0447:  // spl - Store Pointers and Lengths
            du2words (cpu.Yblock8);
          break;

        /// EIS - Address Register Special Arithmetic

        case 0502:  // a4bd Add 4-bit Displacement to Address Register
          a4bd ();
          break;

        case 0501:  // a6bd Add 6-bit Displacement to Address Register
          axbd (6);
          break;

        case 0500:  // a9bd Add 9-bit Displacement to Address Register
          axbd (9);
          break;

        case 0503:  // abd  Add bit Displacement to Address Register
          axbd (1);
          break;

        case 0507:  // awd Add  word Displacement to Address Register
          axbd (36);
          break;

        case 0522:  // s4bd Subtract 4-bit Displacement from Address Register
          s4bd ();
          break;

        case 0521:  // s6bd   Subtract 6-bit Displacement from Address Register
          sxbd (6);
          break;

        case 0520:  // s9bd   Subtract 9-bit Displacement from Address Register
          sxbd (9);
          break;

        case 0523:  // sbd Subtract   bit Displacement from Address Register
          sxbd (1);
          break;

        case 0527:  // swd Subtract  word Displacement from Address Register
          sxbd (36);
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
          mlr();
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
        default:
            if (cpu.switches.halt_on_unimp)
                return STOP_ILLOP;
            doFault(FAULT_IPR, flt_ipr_ill_op, "Illegal instruction");
    }

    return SCPE_OK;

}



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
                // putc(c, stdout);
                //sim_putchar(c);
                sim_printf("%c", c);
            break;
        }
        case 0100:     // putc9 - put 9-bit char in A(0) to stdout
        {
            char c = (cpu.rA >> 27) & 0x7f;
            if (isascii(c))  // ignore NULL chars.
                //putc(c, stdout);
                //sim_putchar(c);
                sim_printf("%c", c);
            else
                sim_printf("\\%03o", c);
            break;
        }
        case 2:     // putc6 - put 6-bit char in A to stdout
        {
            int c = GEBcdToASCII[cpu.rA & 077];
            if (c != -1)
            {
                if (isascii(c))  // ignore NULL chars.
                    //putc(c, stdout);
                    //sim_putchar(c);
                    sim_printf("%c", c);
                else
                    sim_printf("\\%3o", c);
            }
            //putc(c, stdout);
            break;
        }
        case 3:     // putoct - put octal contents of A to stdout (split)
        {
            sim_printf("%06o %06o", GETHI(cpu.rA), GETLO(cpu.rA));
            break;
        }
        case 4:     // putoctZ - put octal contents of A to stdout
                    // (zero-suppressed)
        {
            sim_printf("%llo", cpu.rA);
            break;
        }
        case 5:     // putdec - put decimal contents of A to stdout
        {
            t_int64 tmp = SIGNEXT36_64(cpu.rA);
            sim_printf("%lld", tmp);
            break;
        }
        case 6:     // putEAQ - put float contents of C(EAQ) to stdout
        {
            long double eaq = EAQToIEEElongdouble();
            sim_printf("%12.8Lg", eaq);
            break;
        }
        case 7:   // dump index registers
            //sim_printf("Index registers * * *\n");
            for(int i = 0 ; i < 8 ; i += 4)
                sim_printf("r[%d]=%06o r[%d]=%06o r[%d]=%06o r[%d]=%06o\n",
                           i+0, cpu.rX[i+0], i+1, cpu.rX[i+1], i+2, cpu.rX[i+2],
                           i+3, cpu.rX[i+3]);
            break;

        case 17: // dump pointer registers
            for(int n = 0 ; n < 8 ; n++)
            {
                sim_printf("PR[%d]/%s: SNR=%05o RNR=%o WORDNO=%06o "
                           "BITNO:%02o\n",
                           n, PRalias[n], cpu.PR[n].SNR, cpu.PR[n].RNR, cpu.PR[n].WORDNO, 
                           cpu.PR[n].BITNO);
            }
            break;
        case 27:    // dump registers A & Q
            sim_printf("A: %012llo Q:%012llo\n", cpu.rA, cpu.rQ);
            break;

        case 8: // crlf to console
            sim_printf("\n");
            break;

        case 13:     // putoct - put octal contents of Q to stdout (split)
        {
            sim_printf("%06o %06o", GETHI(cpu.rQ), GETLO(cpu.rQ));
            break;
        }
        case 14:     // putoctZ - put octal contents of Q to stdout
                     // (zero-suppressed)
        {
            sim_printf("%llo", cpu.rQ);
            break;
        }
        case 15:     // putdec - put decimal contents of Q to stdout
        {
            t_int64 tmp = SIGNEXT36_64(cpu.rQ);
            sim_printf("%lld", tmp);
            break;
        }

        case 16:     // puts - A high points to by an aci string; print it.
                     // The string includes C-sytle escapes: \0 for end
                     // of string, \n for newline, \\ for a backslash
        case 21: // puts: A contains a 24 bit address
        {
            const int maxlen = 256;
            char buf [maxlen + 1];

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
                    chunk = M [addr ++];
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
                    buf [cnt ++] = ch;
                    if (ch == '\0')
                      break;
                }
                else
                {
                    if (ch == '\\')
                        is_escape = true;
                    else
                    {
                        buf [cnt ++] = ch;
                        if (ch == '\0')
                            break;
                    }
                }
            }
            // Safety; if filled buffer before finding eos, put an eos
            // in the extra space that was allocated
            buf [maxlen] = '\0';
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

// XXX This code may be redundant
// CANFAULT
static int doABSA (word36 * result)
  {
    DCDstruct * i = & cpu.currentInstruction;
    word36 res;
    sim_debug (DBG_APPENDING, & cpu_dev, "absa CA:%08o\n", cpu.TPR.CA);

    if (get_addr_mode () == ABSOLUTE_mode && ! i -> a)
      {
        //sim_debug (DBG_ERR, & cpu_dev, "ABSA in absolute mode\n");
        // Not clear what the subfault should be; see Fault Register in AL39.
        //doFault (FAULT_IPR, flt_ipr_ill_proc, "ABSA in absolute mode.");
        * result = (cpu.TPR.CA & MASK18) << 12; // 24:12 format
        return SCPE_OK;
      }

    if (cpu.DSBR.U == 1) // Unpaged
      {
        sim_debug (DBG_APPENDING, & cpu_dev, "absa DSBR is unpaged\n");
        // 1. If 2 * segno >= 16 * (DSBR.BND + 1), then generate an access
        // violation, out of segment bounds, fault.

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa Boundary check: TSR: %05o f(TSR): %06o "
          "BND: %05o f(BND): %06o\n",
          cpu.TPR.TSR, 2 * (uint) cpu.TPR.TSR,
          cpu.DSBR.BND, 16 * ((uint) cpu.DSBR.BND + 1));

        if (2 * (uint) cpu.TPR.TSR >= 16 * ((uint) cpu.DSBR.BND + 1))
          {
            doFault (FAULT_ACV, ACV15, "ABSA in DSBR boundary violation.");
          }

        // 2. Fetch the target segment SDW from DSBR.ADDR + 2 * segno.

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa DSBR.ADDR %08o TSR %o SDWe offset %o SWDe %08o\n",
          cpu.DSBR.ADDR, cpu.TPR.TSR, 2 * cpu.TPR.TSR,
          cpu.DSBR.ADDR + 2 * cpu.TPR.TSR);

        word36 SDWe, SDWo;
        core_read ((cpu.DSBR.ADDR + 2 * cpu.TPR.TSR) & PAMASK, & SDWe, __func__);
        core_read ((cpu.DSBR.ADDR + 2 * cpu.TPR.TSR  + 1) & PAMASK, & SDWo, 
                    __func__);

//sim_debug (DBG_TRACE, & cpu_dev, "absa SDW0 %s\n", strSDW0 (& SDW0));
//sim_debug (DBG_TRACE, & cpu_dev, "absa  DSBR.ADDR %08o TPR.TSR %08o\n",
//           DSBR.ADDR, cpu.TPR.TSR);
//sim_debug (DBG_TRACE, & cpu_dev,
//           "absa  SDWaddr: %08o SDW: %012llo %012llo\n",
//           DSBR.ADDR + 2 * cpu.TPR.TSR, SDWe, SDWo);
        // 3. If SDW.F = 0, then generate directed fault n where n is given in
        // SDW.FC. The value of n used here is the value assigned to define a
        // missing segment fault or, simply, a segment fault.

        // ABSA doesn't care if the page isn't resident


        // 4. If offset >= 16 * (SDW.BOUND + 1), then generate an access
        // violation, out of segment bounds, fault.

        word14 BOUND = (SDWo >> (35u - 14u)) & 037777u;
        if (cpu.TPR.CA >= 16u * (BOUND + 1u))
          {
            doFault (FAULT_ACV, ACV15, "ABSA in SDW boundary violation.");
          }

        // 5. If the access bits (SDW.R, SDW.E, etc.) of the segment are
        // incompatible with the reference, generate the appropriate access
        // violation fault.

        // t4d doesn't care
        // XXX Don't know what the correct behavior is here for ABSA


        // 6. Generate 24-bit absolute main memory address SDW.ADDR + offset.

        word24 ADDR = (SDWe >> 12) & 077777760;
        res = (word36) ADDR + (word36) cpu.TPR.CA;
        res &= PAMASK; //24 bit math
        res <<= 12; // 24:12 format

      }
    else
      {
        sim_debug (DBG_APPENDING, & cpu_dev, "absa DSBR is paged\n");
        // paged
        word15 segno = cpu.TPR.TSR;
        word18 offset = cpu.TPR.CA;

        // 1. If 2 * segno >= 16 * (DSBR.BND + 1), then generate an access
        // violation, out of segment bounds, fault.

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa Segment boundary check: segno: %05o f(segno): %06o "
          "BND: %05o f(BND): %06o\n",
          segno, 2 * (uint) segno,
          cpu.DSBR.BND, 16 * ((uint) cpu.DSBR.BND + 1));

        if (2 * (uint) segno >= 16 * ((uint) cpu.DSBR.BND + 1))
          {
            doFault (FAULT_ACV, ACV15, "ABSA in DSBR boundary violation.");
          }

        // 2. Form the quantities:
        //       y1 = (2 * segno) modulo 1024
        //       x1 = (2 * segno - y1) / 1024

        word24 y1 = (2 * segno) % 1024;
        word24 x1 = (2 * segno - y1) / 1024;

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa y1:%08o x1:%08o\n", y1, x1);

        // 3. Fetch the descriptor segment PTW(x1) from DSBR.ADR + x1.

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa read PTW1@%08o+%08o %08o\n",
          cpu.DSBR.ADDR, x1, (cpu.DSBR.ADDR + x1) & PAMASK);

        word36 PTWx1;
        core_read ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);

        struct _ptw0 PTW1;
        PTW1.ADDR = GETHI(PTWx1);
        PTW1.U = TSTBIT(PTWx1, 9);
        PTW1.M = TSTBIT(PTWx1, 6);
        PTW1.F = TSTBIT(PTWx1, 2);
        PTW1.FC = PTWx1 & 3;

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa PTW1 ADDR %08o U %o M %o F %o FC %o\n",
          PTW1.ADDR, PTW1.U, PTW1.M, PTW1.F, PTW1.FC);

        // 4. If PTW(x1).F = 0, then generate directed fault n where n is
        // given in PTW(x1).FC. The value of n used here is the value
        // assigned to define a missing page fault or, simply, a
        // page fault.

        if (!PTW1.F)
          {
            sim_debug (DBG_APPENDING, & cpu_dev, "absa fault !PTW1.F\n");
            // initiate a directed fault
            doFault(FAULT_DF0 + PTW1.FC, 0, "ABSA !PTW1.F");
          }

        // 5. Fetch the target segment SDW, SDW(segno), from the
        // descriptor segment page at PTW(x1).ADDR + y1.

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa read SDW@%08o<<6+%08o %08o\n",
          PTW1.ADDR, y1, ((PTW1.ADDR << 6) + y1) & PAMASK);

        word36 SDWeven, SDWodd;
        core_read2(((PTW1.ADDR << 6) + y1) & PAMASK, & SDWeven, & SDWodd,
                     __func__);

        struct _sdw0 SDW0;
        // even word
        SDW0.ADDR = (SDWeven >> 12) & PAMASK;
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

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa SDW0 ADDR %08o R1 %o R1 %o R3 %o F %o FC %o\n",
          SDW0.ADDR, SDW0.R1, SDW0.R2, SDW0.R3, SDW0.F,
          SDW0.FC);

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa SDW0 BOUND %06o R %o E %o W %o P %o U %o G %o C %o "
          "EB %05o\n",
          SDW0.BOUND, SDW0.R, SDW0.E, SDW0.W, SDW0.P, SDW0.U,
          SDW0.G, SDW0.C, SDW0.EB);


        // 6. If SDW(segno).F = 0, then generate directed fault n where
        // n is given in SDW(segno).FC.
        // This is a segment fault as discussed earlier in this section.

        if (!SDW0.F)
          {
            sim_debug (DBG_APPENDING, & cpu_dev, "absa fault !SDW0.F\n");
            doFault(FAULT_DF0 + SDW0.FC, 0, "ABSA !SDW0.F");
          }

        // 7. If offset >= 16 * (SDW(segno).BOUND + 1), then generate an
        // access violation, out of segment bounds, fault.

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa SDW boundary check: offset: %06o f(offset): %06o "
          "BOUND: %06o\n",
          offset, offset >> 4, SDW0.BOUND);

        if (((offset >> 4) & 037777) > SDW0.BOUND)
          {
            sim_debug (DBG_APPENDING, & cpu_dev,
                       "absa SDW boundary violation\n");
            doFault (FAULT_ACV, ACV15, "ABSA in SDW boundary violation.");
          }

        // 8. If the access bits (SDW(segno).R, SDW(segno).E, etc.) of the
        // segment are incompatible with the reference, generate the
        // appropriate access violation fault.

        // Only the address is wanted, so no check

        if (SDW0.U == 0)
          {
            // Segment is paged

            // 9. Form the quantities:
            //    y2 = offset modulo 1024
            //    x2 = (offset - y2) / 1024

            word24 y2 = offset % 1024;
            word24 x2 = (offset - y2) / 1024;

            sim_debug (DBG_APPENDING, & cpu_dev,
              "absa y2:%08o x2:%08o\n", y2, x2);

            // 10. Fetch the target segment PTW(x2) from SDW(segno).ADDR + x2.

            sim_debug (DBG_APPENDING, & cpu_dev,
              "absa read PTWx2@%08o+%08o %08o\n",
              SDW0.ADDR, x2, (SDW0.ADDR + x2) & PAMASK);

            word36 PTWx2;
            core_read ((SDW0.ADDR + x2) & PAMASK, & PTWx2, __func__);

            struct _ptw0 PTW_2;
            PTW_2.ADDR = GETHI(PTWx2);
            PTW_2.U = TSTBIT(PTWx2, 9);
            PTW_2.M = TSTBIT(PTWx2, 6);
            PTW_2.F = TSTBIT(PTWx2, 2);
            PTW_2.FC = PTWx2 & 3;

            sim_debug (DBG_APPENDING, & cpu_dev,
              "absa PTW_2 ADDR %08o U %o M %o F %o FC %o\n",
              PTW_2.ADDR, PTW_2.U, PTW_2.M, PTW_2.F, PTW_2.FC);

            // 11.If PTW(x2).F = 0, then generate directed fault n where n is
            // given in PTW(x2).FC. This is a page fault as in Step 4 above.

            // ABSA only wants the address; it doesn't care if the page is
            // resident

            // if (!PTW_2.F)
            //   {
            //     sim_debug (DBG_APPENDING, & cpu_dev,
            //                "absa fault !PTW_2.F\n");
            //     // initiate a directed fault
            //     doFault(FAULT_DF0 + PTW_2.FC, 0, "ABSA !PTW_2.F");
            //   }

            // 12. Generate the 24-bit absolute main memory address
            // PTW(x2).ADDR + y2.

            res = (((word36) PTW_2.ADDR) << 6)  + (word36) y2;
            res &= PAMASK; //24 bit math
            res <<= 12; // 24:12 format
          }
        else
          {
            // Segment is unpaged
            // SDW0.ADDR is the base address of the segment
            res = (word36) SDW0.ADDR + offset;
            res &= PAMASK; //24 bit math
            res <<= 12; // 24:12 format
          }
      }


    * result = res;

    return SCPE_OK;
  }

void doRCU (void)
  {

    if_sim_debug (DBG_TRACE, & cpu_dev)
      {
        for (int i = 0; i < 8; i ++)
          {
            sim_debug (DBG_TRACE, & cpu_dev, "RCU %d %012llo\n", i, cpu.Yblock8 [i]);
          }
      }

    words2scu (cpu.Yblock8);

// Restore addressing mode

    if (TST_I_ABS == 0)
      set_addr_mode (APPEND_mode);
    else
      set_addr_mode (ABSOLUTE_mode);

    if (cpu.cu.FLT_INT == 0) // is interrupt, not fault
      {
        sim_debug (DBG_TRACE, & cpu_dev, "RCU interrupt return\n");
        longjmp (cpu.jmpMain, JMP_REFETCH);
      }

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
//        SMCM: store fault (not control)  \
//        SMIC: store fault (not control)   > I believe that these should be
//        SSCR: store fault (not control)  /  command fault
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
        sim_debug (DBG_TRACE, & cpu_dev, "RCU MME2 restart return\n");
        cpu.cu.rfi = 1;
        longjmp (cpu.jmpMain, JMP_RESTART);
      }

    // MME faults resume with the next instruction

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

    // LUF can happen during fetch or CAF. If fetch, handled above
    if (cpu.cu.FI_ADDR == FAULT_LUF)
      {
        cpu.cu.rfi = 1;
        longjmp (cpu.jmpMain, JMP_RESTART);
        sim_debug (DBG_TRACE, & cpu_dev, "RCU LUF RESTART return\n");
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
    doFault (FAULT_TRB, cpu.cu.FI_ADDR, "doRCU dies with unhandled fault number");
  }

#ifdef REAL_TR
//static bool overrunAck;

void setTR (word27 val)
  {
    val &= MASK27;
    if (val)
      {
        cpu.timerRegVal = val & MASK27;
      }
    else
      {
        // Special case
        cpu.timerRegVal = -1 & MASK27;
      }
    gettimeofday (& cpu.timerRegT0, NULL);
    //overrunAck = false;

//sim_printf ("tr set %10u %09o %10lu%06lu\n",
//  val, cpu.timerRegVal, cpu.timerRegT0.tv_sec, cpu.timerRegT0.tv_usec);
  }

word27 getTR (bool * runout)
  {
#if 0
    struct timeval tnow, tdelta;
    gettimeofday (& tnow, NULL);
    timersub (& tnow, & cpu.timerRegT0, & tdelta);
    // 1000000 can be represented in 20 bits; so in a 64 bit word, we have 
    // room for 44 bits of seconds, way more then enough.
    // Do 64 bit math; much faster.
    //
    //delta = (tnowus - t0us) / 1.953125
    uint64 delta;
    delta = ((uint64) tdelta.tv_sec) * 1000000 + ((uint64) tdelta.tv_usec);
    // 1M * 1M ~= 40 bits; still leaves 24bits of seconds.
    delta = (delta * 1000000) / 1953125;
#else
    uint128 t0us, tnowus, delta;
    struct timeval tnow;
    gettimeofday (& tnow, NULL);
    t0us = cpu.timerRegT0.tv_sec * 1000000 + cpu.timerRegT0.tv_usec;
    tnowus = tnow.tv_sec * 1000000 + tnow.tv_usec;
    //delta = (tnowus - t0us) / 1.953125
    delta = ((tnowus - t0us) * 1000000) / 1953125;
#endif
    if (runout)
     //* runout = (! overrunAck) && delta > cpu.timerRegVal;
     * runout = delta > cpu.timerRegVal;
    word27 val = (cpu.timerRegVal - delta) & MASK27;
//if (val % 100000 == 0)
// sim_printf ("tr get %10u %09o %8llu %s\n",
// val, val, (unsigned long long) delta,
//  runout ? * runout ? "runout" : "" : "");
    return val;
  }

void ackTR (void)
  {
    //overrunAck = true;
    setTR (0);
  }
#endif

