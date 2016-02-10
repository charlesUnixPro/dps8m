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
#include "dps8_faults.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#ifdef HDBG
#include "hdbg.h"
#endif

// XXX This is used wherever a single unit only is assumed
#define ASSUME0 0
// XXX Use this for places where is matters when we have multiple CPUs
#define ASSUME_CPU0 0

// In the case of RPDA, the odd instruction has been observed to reference the
// same X register as the even instruction, and does not expect the X register
// to be updated until after the odd instruction as executed. The code needs a
// way to remember what the even instruction register number was so that
// it can be updated. Stash it in cu.CT_HOLD; this is safe, as the repeated 
// instructions are not allowed to use addressing modes that would disturb it.

word36 CY = 0;              ///< C(Y) operand data from memory
word36 Ypair[2];        ///< 2-words
word36 Yblock8[8];      ///< 8-words
word36 Yblock16[16];    ///< 16-words
word36 Yblock32[32];    ///< 32-words
static int doABSA (word36 * result);

static t_stat doInstruction (void);
static int emCall (void);

// CANFAULT 
static void writeOperands (void)
{
    DCDstruct * i = & currentInstruction;

    sim_debug (DBG_ADDRMOD, & cpu_dev,
               "writeOperands(%s):mne=%s flags=%x\n",
               disAssemble (IWB_IRODD), i -> info -> mne, i -> info -> flags);

    if (characterOperandFlag)
      {
        word36 data;

        // read data where chars/bytes now live (if it hasn't already been 
        // read in)

        if (i->info->flags & READ_OPERAND)
          data = CY;
        else
          Read (TPR . CA, & data, OPERAND_READ, i -> a);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "IT_MOD(IT_SC): read char/byte %012llo from %06o tTB=%o tCF=%o\n",
                   data, TPR . CA, 
                   characterOperandSize, characterOperandOffset);

        // set byte/char
        switch (characterOperandSize)
          {
            case TB6:
              putChar (& data, CY & 077, characterOperandOffset);
              break;
            case TB9:
              putByte (& data, CY & 0777, characterOperandOffset);
              break;
            default:
              sim_printf ("WriteOperands IT_MOD(IT_SC): unknown tTB:%o\n",
                          characterOperandSize);
              break;
          }

        // write it
        Write (TPR . CA, data, OPERAND_STORE, i -> a);   //TM_IT);

        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "IT_MOD(IT_SC): wrote char/byte %012llo to %06o tTB=%o tCF=%o\n",
                   data, TPR . CA, 
                   characterOperandSize, characterOperandOffset);


        return;
      }

    WriteOP (TPR . CA, OPERAND_STORE, i -> a);

    return;
}

// CANFAULT 
static void readOperands (void)
{
    DCDstruct * i = & currentInstruction;

    sim_debug(DBG_ADDRMOD, &cpu_dev, "readOperands(%s):mne=%s flags=%x dof=%d do=%012llo\n", disAssemble(IWB_IRODD), i->info->mne, i->info->flags, directOperandFlag, directOperand);
sim_debug(DBG_ADDRMOD, &cpu_dev, "readOperands a %d address %08o\n", i -> a, TPR.CA);
    if (directOperandFlag)
      {
        CY = directOperand;
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands direct CY=%012llo\n", CY);
        return;
      }

    if (characterOperandFlag)
      {
        word36 data;
        Read (TPR . CA, & data, OPERAND_READ, i -> a);
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands: IT_MOD(IT_SC): indword=%012llo\n", data);
        switch (characterOperandSize)
          {
            case TB6:
              CY = GETCHAR (data, characterOperandOffset % 6); // XXX magic number
              break;

            case TB9:
              CY = GETBYTE (data, characterOperandOffset % 4); // XXX magic number
              break;

            default:
              sim_printf ("readOperands: IT_MOD(IT_SC): unknown tTB:%o\n", tTB);
              break;
          }
        sim_debug (DBG_ADDRMOD, & cpu_dev,
                   "readOperands: IT_MOD(IT_SC): read operand %012llo from %06o char/byte=%llo\n",
                   data, TPR . CA, CY);

        return;
      }

    ReadOP (TPR.CA, OPERAND_READ, i -> a);
    return;
}

static word36 scu_data[8];    // For SCU instruction

static void scu2words(word36 *words)
  {
    memset (words, 0, 8 * sizeof (* words));
    
    // words [0]

    putbits36 (& words [0],  0,  3, PPR . PRR);
    putbits36 (& words [0],  3, 15, PPR . PSR);
    putbits36 (& words [0], 18,  1, PPR . P);
    // 19, 1 XSF External segment flag
    // 20, 1 SDWAMM Match on SDWAM
    putbits36 (& words [0], 21,  1, cu . SD_ON);
    // 22, 1 PTWAMM Match on PTWAM
    putbits36 (& words [0], 23,  1, cu . PT_ON);
#if 0
    putbits36 (& words [0], 24,  1, cu . PI_AP);   // 24    PI-AP
    putbits36 (& words [0], 25,  1, cu . DSPTW);   // 25    DSPTW
    putbits36 (& words [0], 26,  1, cu . SDWNP);   // 26    SDWNP
    putbits36 (& words [0], 27,  1, cu . SDWP);    // 27    SDWP
    putbits36 (& words [0], 28,  1, cu . PTW);     // 28    PTW
    putbits36 (& words [0], 29,  1, cu . PTW2);    // 29    PTW2
    putbits36 (& words [0], 30,  1, cu . FAP);     // 30    FAP
    putbits36 (& words [0], 31,  1, cu . FANP);    // 31    FANP
    putbits36 (& words [0], 32,  1, cu . FABS);    // 32    FABS
#else
    // XXX Only the top 9 bits are used in APUCycleBits, so this is
    // zeroing the 3 FTC bits at the end of the word; on the
    // other hand this keeps the values in apuStatusBits clearer. 
    // If FTC is ever used, be sure to put it's save code after this
    // line.
    putbits36 (& words [0], 24, 12, cu . APUCycleBits);
#endif
//sim_printf ("scu2words wrote %012llo @ %08o\n", words [0], words);
    // words [1]
    
    //putbits36 (& words [1],  0, 16, cpu . subFault & MASK16);
    putbits36 (& words [1],  0,  1, cu .  IRO_ISN);
    putbits36 (& words [1],  1,  1, cu .  OEB_IOC);
    putbits36 (& words [1],  2,  1, cu .  EOFF_IAIM);
    putbits36 (& words [1],  3,  1, cu .  ORB_ISP);
    putbits36 (& words [1],  4,  1, cu .  ROFF_IPR);
    putbits36 (& words [1],  5,  1, cu .  OWB_NEA);
    putbits36 (& words [1],  6,  1, cu .  WOFF_OOB);
    putbits36 (& words [1],  7,  1, cu .  NO_GA);
    putbits36 (& words [1],  8,  1, cu .  OCB);
    putbits36 (& words [1],  9,  1, cu .  OCALL);
    putbits36 (& words [1], 10,  1, cu .  BOC);
    putbits36 (& words [1], 11,  1, cu .  PTWAM_ER);
    putbits36 (& words [1], 12,  1, cu .  CRT);
    putbits36 (& words [1], 13,  1, cu .  RALR);
    putbits36 (& words [1], 14,  1, cu .  SWWAM_ER);
    putbits36 (& words [1], 15,  1, cu .  OOSB);
    putbits36 (& words [1], 16,  1, cu .  PARU);
    putbits36 (& words [1], 17,  1, cu .  PARL);
    putbits36 (& words [1], 18,  1, cu .  ONC1);
    putbits36 (& words [1], 19,  1, cu .  ONC2);
    putbits36 (& words [1], 20,  4, cu .  IA);
    putbits36 (& words [1], 24,  3, cu .  IACHN);
    putbits36 (& words [1], 27,  3, cu .  CNCHN);
    putbits36 (& words [1], 30,  5, cu .  FI_ADDR);
    putbits36 (& words [1], 35, 1, cpu . cycle == INTERRUPT_cycle ? 0 : 1);

    // words [2]
    
    putbits36 (& words [2],  0,  3, TPR . TRR);
    putbits36 (& words [2],  3, 15, TPR . TSR);
    // 18, 4 PTWAM levels enabled
    // 22, 4 SDWAM levels enabled
    // 26, 1 0
    putbits36 (& words [2], 27,  3, switches . cpu_num);
    putbits36 (& words [2], 30,  6, cu . delta);
    
    // words [3]

    //  0, 18 0
#ifdef ABUSE_CT_HOLD
    putbits36 (& words [3], 3, 1, cu . coFlag);    // 3
    putbits36 (& words [3], 4, 1, cu . coSize);    // 4
    putbits36 (& words [3], 5, 3, cu . coOffset);  // 5-7
#endif

    // 18, 4 TSNA pointer register number for non-EIS or EIS operand #1
    // 22, 4 TSNB pointer register number for EIS operand #2
    // 26, 4 TSNC pointer register number for EIS operand #3
    putbits36 (& words [3], 30, 6, TPR . TBR);
    
    // words [4]

    putbits36 (& words [4],  0, 18, PPR . IC);
    putbits36 (& words [4], 18, 18, cu . IR); // HWR
    
    // words [5]

    putbits36 (& words [5],  0, 18, TPR . CA);
    putbits36 (& words [5], 18,  1, cu . repeat_first);
    putbits36 (& words [5], 19,  1, cu . rpt);
    putbits36 (& words [5], 20,  1, cu . rd);
    // 21, 1 RL repeat link
    // 22, 1 POT Prepare operand tally
    // 23, 1 PON Prepare operand no tally
    putbits36 (& words [5], 24,  1, cu . xde);
    putbits36 (& words [5], 25,  1, cu . xdo);
    // 26, 1 ITP Execute ITP indirect cycle
    putbits36 (& words [5], 27,  1, cu . rfi);
    // 28, 1 ITS Execute ITS indirect cycle
    putbits36 (& words [5], 29,  1, cu . FIF);
    putbits36 (& words [5], 30,  6, cu . CT_HOLD);
    
    // words [6]

    words [6] = cu . IWB; 
    
    // words [7]

    words [7] = cu . IRODD;
  }


void cu_safe_store(void)
{
    // Save current Control Unit Data in hidden temporary so a later SCU instruction running
    // in FAULT mode can save the state as it existed at the time of the fault rather than
    // as it exists at the time the scu instruction is executed.
    scu2words(scu_data);

    tidy_cu ();

}

void tidy_cu (void)
  {
// The only places this is called is in fault and interrupt processing;
// once the CU is saved, it needs to be set to a usable state. Refactoring
// that code here so that there is only a single copy to maintain.

    cu . delta = 0;
    cu . repeat_first = false;
    cu . rpt = false;
    cu . rd = false;
    cu . xde = false;
    cu . xdo = false;
    cu . IR &= ~ I_MIIF; 
  }

static void words2scu (word36 * words)
{
    // BUG:  We don't track much of the data that should be tracked
    
    // words [0]

    PPR.PRR         = getbits36(words[0], 0, 3);
    PPR.PSR         = getbits36(words[0], 3, 15);
    PPR.P           = getbits36(words[0], 18, 1);
    cu.SD_ON        = getbits36(words[0], 21, 1);
    cu.PT_ON        = getbits36(words[0], 23, 1);
#if 0
    cu.PI_AP        = getbits36(words[0], 24, 1);
    cu.DSPTW        = getbits36(words[0], 25, 1);
    cu.SDWNP        = getbits36(words[0], 26, 1);
    cu.SDWP         = getbits36(words[0], 27, 1);
    cu.PTW          = getbits36(words[0], 28, 1);
    cu.PTW2         = getbits36(words[0], 29, 1);
    cu.FAP          = getbits36(words[0], 30, 1);
    cu.FANP         = getbits36(words[0], 31, 1);
    cu.FABS         = getbits36(words[0], 32, 1);
#else
    cu . APUCycleBits = getbits36 (words [0], 24, 12) & 07770;
#endif
    
    // words[1]

    cu . IRO_ISN      = getbits36 (words [1],  0,  1);
    cu . OEB_IOC      = getbits36 (words [1],  1,  1);
    cu . EOFF_IAIM    = getbits36 (words [1],  2,  1);
    cu . ORB_ISP      = getbits36 (words [1],  3,  1);
    cu . ROFF_IPR     = getbits36 (words [1],  4,  1);
    cu . OWB_NEA      = getbits36 (words [1],  5,  1);
    cu . WOFF_OOB     = getbits36 (words [1],  6,  1);
    cu . NO_GA        = getbits36 (words [1],  7,  1);
    cu . OCB          = getbits36 (words [1],  8,  1);
    cu . OCALL        = getbits36 (words [1],  9,  1);
    cu . BOC          = getbits36 (words [1], 10,  1);
    cu . PTWAM_ER     = getbits36 (words [1], 11,  1);
    cu . CRT          = getbits36 (words [1], 12,  1);
    cu . RALR         = getbits36 (words [1], 13,  1);
    cu . SWWAM_ER     = getbits36 (words [1], 14,  1);
    cu . OOSB         = getbits36 (words [1], 15,  1);
    cu . PARU         = getbits36 (words [1], 16,  1);
    cu . PARL         = getbits36 (words [1], 17,  1);
    cu . ONC1         = getbits36 (words [1], 18,  1);
    cu . ONC2         = getbits36 (words [1], 19,  1);
    cu . IA           = getbits36 (words [1], 20,  4);
    cu . IACHN        = getbits36 (words [1], 24,  3);
    cu . CNCHN        = getbits36 (words [1], 27,  3);
    cu . FI_ADDR      = getbits36 (words [1], 30,  5);
    cu . FLT_INT      = getbits36 (words [1], 35,  1);

    // words[2]
    
    TPR.TRR         = getbits36(words[2], 0, 3);
    TPR.TSR         = getbits36(words[2], 3, 15);
    cu.delta        = getbits36(words[2], 30, 6);
    
    // words[3]

#ifdef ABUSE_CT_HOLD
    cu . coFlag     = getbits36 (words [3], 3, 1);    // 3
    cu . coSize     = getbits36 (words [3], 4, 1);    // 4
    cu . coOffset   = getbits36 (words [3], 5, 3);  // 5-7
#endif

    TPR.TBR         = getbits36(words[3], 30, 6);
    
    // words [4]

    cu.IR           = getbits36(words[4], 18, 18); // HWR
    PPR.IC          = getbits36(words[4], 0, 18);
    
    // words [5]

    TPR.CA          = getbits36(words[5], 0, 18);
    cu.repeat_first = getbits36(words[5], 18, 1);
    cu.rpt          = getbits36(words[5], 19, 1);
    cu.rd           = getbits36(words[5], 20, 1);
    cu.xde          = getbits36(words[5], 24, 1);
    cu.xdo          = getbits36(words[5], 25, 1);
    cu.rfi          = getbits36(words[5], 27, 1);
    cu.FIF          = getbits36(words[5], 29, 1);
    cu.CT_HOLD      = getbits36(words[5], 30, 6);
    
    // words [6]

    cu.IWB = words [6];

    // words [7]

    cu.IRODD = words [7];
}

void cu_safe_restore (void)
  {
    words2scu (scu_data);
  }

static void du2words (word36 * words)
  {
    memset (words, 0, 8 * sizeof (* words));

    // Word 0

    putbits36 (& words [0],  9,  1, du . Z);
    putbits36 (& words [0], 10,  1, du . NOP);
    putbits36 (& words [0], 12, 24, du . CHTALLY);

    // Word 1

    // Word 2

    putbits36 (& words [2],  0, 18, du . D1_PTR_W);
    putbits36 (& words [2], 18,  6, du . D1_PTR_B);
    putbits36 (& words [2], 25,  2, du . TAk [0]);
    putbits36 (& words [2], 31,  1, du . F1);
    putbits36 (& words [2], 32,  1, du . Ak [0]);

    // Word 3

    putbits36 (& words [3],  0, 10, du . LEVEL1);
    putbits36 (& words [3], 12, 24, du . D1_RES);
    
    // Word 4

    putbits36 (& words [4],  0, 18, du . D2_PTR_W);
    putbits36 (& words [4], 18,  6, du . D2_PTR_B);
    putbits36 (& words [4], 25,  2, du . TAk [1]);
    putbits36 (& words [4], 30,  1, du . R);
    putbits36 (& words [4], 31,  1, du . F2);
    putbits36 (& words [4], 32,  1, du . Ak [1]);

    // Word 5

    putbits36 (& words [5],  9,  1, du . LEVEL2);
    putbits36 (& words [5], 12, 24, du . D2_RES);

    // Word 6

    putbits36 (& words [6],  0, 18, du . D3_PTR_W);
    putbits36 (& words [6], 18,  6, du . D3_PTR_B);
    putbits36 (& words [6], 25,  2, du . TAk [2]);
    putbits36 (& words [6], 31,  1, du . F3);
    putbits36 (& words [6], 32,  1, du . Ak [2]);
    putbits36 (& words [6], 33,  3, du . JMP);

    // Word 7

    putbits36 (& words [7], 12, 24, du . D3_RES);

  }

static void words2du (word36 * words)
  {
    // Why on earth?
    //memset (words, 0, 8 * sizeof (* words));

    // Word 0

    du . Z        = getbits36 (words [0],  9,  1);
    du . NOP      = getbits36 (words [0], 10,  1);
    du . CHTALLY  = getbits36 (words [0], 12, 24);
    // Word 1

    // Word 2

    du . D1_PTR_W = getbits36 (words [2],  0, 18);
    du . D1_PTR_B = getbits36 (words [2], 18,  6);
    du . TAk [0]  = getbits36 (words [2], 25,  2);
    du . F1       = getbits36 (words [2], 31,  1);
    du . Ak [0]   = getbits36 (words [2], 32,  1);

    // Word 3

    du . LEVEL1   = getbits36 (words [3],  0, 10);
    du . D1_RES   = getbits36 (words [3], 12, 24);
    
    // Word 4

    du . D2_PTR_W = getbits36 (words [4],  0, 18);
    du . D2_PTR_B = getbits36 (words [4], 18,  6);
    du . TAk [1]  = getbits36 (words [4], 25,  2);
    du . F2       = getbits36 (words [4], 31,  1);
    du . Ak [1]   = getbits36 (words [4], 32,  1);

    // Word 5

    du . LEVEL2   = getbits36 (words [5],  9,  1);
    du . D2_RES   = getbits36 (words [5], 12, 24);

    // Word 6

    du . D3_PTR_W = getbits36 (words [6],  0, 18);
    du . D3_PTR_B = getbits36 (words [6], 18,  6);
    du . TAk [2]  = getbits36 (words [6], 25,  2);
    du . F3       = getbits36 (words [6], 31,  1);
    du . Ak [2]   = getbits36 (words [6], 32,  1);
    du . JMP      = getbits36 (words [6], 33,  3);

    // Word 7

    du . D3_RES   = getbits36 (words [7], 12, 24);

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
    // --   au     qu     du     ic     al     ql     dl     0      1      2      3      4      5      6      7
    false, false, false, true,  false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --     0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp    --     its    sd     scr    f2     f3      ci      i      sc     ad     di     dic   id     idc
    false, false, true,  false, false, false, false, false, false, false, false, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --     *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
};

// No DU
// No DL



// (NO_CI | NO_SC | NO_SCR)
static bool _nocss[] = {
    // Tm = 0 (register) R
    // *    au     qu     du     ic     al     ql     dl      0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --     0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp     --     its    sd     scr    f2     f3    ci     i     sc     ad     di    dic    id     idc
    false, false, true,  false, false, true, false, false, true, false, true, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --     *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
};

// (NO_DUDL | NO_CISCSCR)
static bool _noddcss[] = {
    // Tm = 0 (register) R
    // *    au     qu     du     ic     al     ql    dl     0      1      2      3      4      5      6      7
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --     0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp     --     its    sd     scr    f2     f3    ci     i     sc     ad     di    dic    id     idc
    false, false, true,  false, false, true, false, false, true, false, true, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --     *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
};

// (NO_DUDL | NO_CISCSCR)
static bool _nodlcss[] = {
    // Tm = 0 (register) R
    // *    au     qu     du      ic     al     ql    dl     0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --     0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp     --     its    sd     scr    f2     f3    ci     i     sc     ad     di    dic    id     idc
    false, false, true,  false, false, true, false, false, true, false, true, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --     *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
};

#ifndef QUIET_UNUSED
static bool _illmod[] = {
    // Tm = 0 (register) R
    // *    au     qu     du     ic     al     ql     dl     0      1      2      3      4      5      6      7
    false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false,
    // Tm = 1 (register then indirect) RI
    // n*  au*    qu*    --     ic*    al*    al*    --     0*     1*     2*     3*     4*     5*     6*     7*
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
    // Tm = 2 (indirect then tally) IT
    // f1  itp    --     its    sd     scr    f2     f3      ci      i      sc     ad     di     dic   id     idc
    false, false, true,  false, false, false, false, false, false, false, false, false, false, false, false, false,
    // Tm = 3 (indirect then register) IR
    // *n   *au    *qu    --     *ic   *al    *al    --     *0     *1     *2     *3     *4     *5     *6     *7
    false, false, false, true, false, false, false, true, false, false, false, false, false, false, false, false,
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
            sim_printf ("%20lld: ? opcode 0%04o X %d a %d tag 0%02do\n", count, opcode, opcodeX, a, tag);
        else
            sim_printf ("%20lld: %s\n", count, result);
    }
    return SCPE_OK;
}


// fetch instrcution at address
// CANFAULT
void fetchInstruction (word18 addr)
{
    DCDstruct * p = & currentInstruction;
    
    memset (p, 0, sizeof (struct DCDstruct));

    // since the next memory cycle will be a instruction fetch setup TPR
    TPR.TRR = PPR.PRR;
    TPR.TSR = PPR.PSR;

    //if (get_addr_mode() == ABSOLUTE_mode || get_addr_mode () == BAR_mode)
    if (get_addr_mode() == ABSOLUTE_mode)
      {
        TPR . TRR = 0;
        RSDWH_R1 = 0;
      }

    if (cu . rd && ((PPR . IC & 1) != 0))
      {
        if (cu . repeat_first)
          Read(addr, & cu . IRODD, INSTRUCTION_FETCH, 0);
      }
    else if (cu . rpt || cu . rd)
      {
        if (cu . repeat_first)
          Read(addr, & cu . IWB, INSTRUCTION_FETCH, 0);
      }
    else
      {
        Read(addr, & cu . IWB, INSTRUCTION_FETCH, 0);
      }

    // TODO: Need to add no DL restrictions?
}

#ifndef QUIET_UNUSED
// read operands (if any)
//t_stat ReadOPs(DCDstruct *i)
//{
//    
//    return SCPE_OK;
//}

// CANFAULT 
void fetchOperands (void)
{
    DCDstruct * p = & currentInstruction;
    
    if (i->info->ndes > 0)
        for(int n = 0 ; n < i->info->ndes; n += 1)
            Read(i, PPR.IC + 1 + n, &i->e->op[n], READ_OPERAND, 0); 
    else
        if (READOP(i) || RMWOP(i))
            ReadOP(i, TPR.CA, READ_OPERAND, XXX);
    
}
#endif

#ifndef QUIET_UNUSED
/*
 * initializes the TPR registers.
 */
static t_stat setupForOperandRead (void)
{
    DCDstruct * i = & currentInstruction;
    if (!i->a)   // if A bit set set-up TPR stuff ...
    {
        TPR.TRR = PPR.PRR;
        TPR.TSR = PPR.PSR;
    }
    return SCPE_OK;
}
#endif

void traceInstruction (uint flag)
  {
    if (! flag) goto force;
    if_sim_debug (flag, &cpu_dev)
      {
force:;
        char * compname;
        word18 compoffset;
        char * where = lookupAddress (PPR.PSR, PPR.IC, & compname, & compoffset);
        bool isBAR = TSTF (cu . IR, I_NBAR) ? false : true;
        if (where)
          {
            if (get_addr_mode() == ABSOLUTE_mode)
              {
                if (isBAR)
                  {
                    sim_debug(flag, &cpu_dev, "%06o|%06o %s\n", BAR.BASE, PPR.IC, where);
                  }
                else
                  {
                    sim_debug(flag, &cpu_dev, "%06o %s\n", PPR.IC, where);
                  }
              }
            else if (get_addr_mode() == APPEND_mode)
              {
                if (isBAR)
                  {
                    sim_debug(flag, &cpu_dev, "%05o:%06o|%06o %s\n", PPR.PSR, BAR.BASE, PPR.IC, where);
                  }
                else
                  {
                    sim_debug(flag, &cpu_dev, "%05o:%06o %s\n", PPR.PSR, PPR.IC, where);
                  }
              }
            listSource (compname, compoffset, flag);
          }

        if (get_addr_mode() == ABSOLUTE_mode)
          {
            if (isBAR)
              {
                sim_debug(flag, &cpu_dev, "%05o|%06o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n", BAR.BASE, PPR.IC, IWB_IRODD, disAssemble(IWB_IRODD), currentInstruction . address, currentInstruction . opcode, currentInstruction . opcodeX, currentInstruction . a, currentInstruction . i, GET_TM(currentInstruction . tag) >> 4, GET_TD(currentInstruction . tag) & 017);
              }
            else
              {
                sim_debug(flag, &cpu_dev, "%06o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n", PPR.IC, IWB_IRODD, disAssemble (IWB_IRODD), currentInstruction . address, currentInstruction . opcode, currentInstruction . opcodeX, currentInstruction . a, currentInstruction . i, GET_TM(currentInstruction . tag) >> 4, GET_TD(currentInstruction . tag) & 017);
              }
          }
        else if (get_addr_mode() == APPEND_mode)
          {
            if (isBAR)
              {
                sim_debug(flag, &cpu_dev, "%05o:%06o|%06o %o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n", PPR.PSR, BAR.BASE, PPR.IC, PPR.PRR, IWB_IRODD, disAssemble(IWB_IRODD), currentInstruction . address, currentInstruction . opcode, currentInstruction . opcodeX, currentInstruction . a, currentInstruction . i, GET_TM(currentInstruction . tag) >> 4, GET_TD(currentInstruction . tag) & 017);
              }
            else
              {
                sim_debug(flag, &cpu_dev, "%05o:%06o %o %012llo (%s) %06o %03o(%d) %o %o %o %02o\n", PPR.PSR, PPR.IC, PPR.PRR, IWB_IRODD, disAssemble(IWB_IRODD), currentInstruction . address, currentInstruction . opcode, currentInstruction . opcodeX, currentInstruction . a, currentInstruction . i, GET_TM(currentInstruction . tag) >> 4, GET_TD(currentInstruction . tag) & 017);
              }
          }
      }

  }

static bool tstOVFfault (void)
  {
    // Masked?
    if (TSTF (cu . IR, I_OMASK))
      return false;
    // Doing a RPT/RPD?
    if (cu . rpt || cu . rd) 
      {
        // a:AL39/rpd2
        // Did the repeat instruction inhibit overflow faults?
        if ((rX [0] & 00001) == 0)
          return false;
      }
    return true;
  }

t_stat executeInstruction (void)
  {


///
/// executeInstruction: Decode the instruction
///

    DCDstruct * ci = & currentInstruction;
    if (cu . rd && ((PPR . IC & 1) != 0))
      {
        decodeInstruction(cu . IRODD, ci);
      }
    else
      {
        decodeInstruction(cu . IWB, ci);
      }

    const opCode *info = ci->info;       // opCode *
    const uint32  opcode = ci->opcode;   // opcode
    const bool   opcodeX = ci->opcodeX;  // opcode extension
    const word18 address = ci->address;  // bits 0-17 of instruction XXX replace with rY
    const bool   a = ci->a;              // bit-29 - addressing via pointer register
    const word6  tag = ci->tag;          // instruction tag XXX replace with rTAG
    
   
    addToTheMatrix (opcode, opcodeX, a, tag);
    
///
/// executeInstruction: Non-restart processing
///

    if (cu . IR & I_MIIF)
      goto restart_1;

    // check for priv ins - Attempted execution in normal or BAR modes causes a
    // illegal procedure fault.
    // XXX Not clear what the subfault should be; see Fault Register in AL39.
    if ((ci -> info -> flags & PRIV_INS) && ! is_priv_mode ())
        doFault (FAULT_IPR, ill_proc, 
                 "Attempted execution of privileged instruction.");
    
    ///
    /// executeInstruction: Non-restart processing
    ///                     check for illegal addressing mode(s) ...
    ///

    // No CI/SC/SCR allowed
    if (ci->info->mods == NO_CSS)
    {
        if (_nocss[ci->tag])
            doFault(FAULT_IPR, ill_mod, "Illegal CI/SC/SCR modification");
    }
    // No DU/DL/CI/SC/SCR allowed
    else if (ci->info->mods == NO_DDCSS)
    {
        if (_noddcss[ci->tag])
            doFault(FAULT_IPR, ill_mod, "Illegal DU/DL/CI/SC/SCR modification");
    }
    // No DL/CI/SC/SCR allowed
    else if (ci->info->mods == NO_DLCSS)
    {
        if (_nodlcss[ci->tag])
            doFault(FAULT_IPR, ill_mod, "Illegal DL/CI/SC/SCR modification");
    }
    // No DU/DL allowed
    else if (ci->info->mods == NO_DUDL)
    {
        if (_nodudl[ci->tag])
            doFault(FAULT_IPR, ill_mod, "Illegal DU/DL modification");
    }
    if (cu . xdo == 1) // Execute even or odd of XED
    {
    // XXX Not clear what the subfault should be; see Fault Register in AL39.
        if (ci->info->flags == NO_XED)
            doFault(FAULT_IPR, ill_proc, "Instruction not allowed in XED");
    }
    if (cu . xde == 1 && cu . xdo == 0) // Execute XEC
    {
    // XXX Not clear what the subfault should be; see Fault Register in AL39.
        if (ci->info->flags == NO_XEC)
            doFault(FAULT_IPR, ill_proc, "Instruction not allowed in XEC");
    }

    // RPT/RPD illegal modifiers
    // a:AL39/rpd3
    if (cu . rpt || cu .rd)
      {
        // check for illegal modifiers:
        //    only R & RI are allowed 
        //    only X1..X7
        switch (GET_TM(ci->tag))
          {
            case TM_R:
            case TM_RI:
              break;
            default:
              // generate fault. Only R & RI allowed
              doFault(FAULT_IPR, ill_mod, "ill addr mod from RPT");
          }
        word6 Td = GET_TD(ci->tag);
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
              doFault(FAULT_IPR, ill_mod, "ill addr mod from RPT");
          }
        // XXX Does this need to also check for NO_RPL?
        // repeat allowed for this instruction?
        // XXX Not clear what the subfault should be
        if (ci->info->flags & NO_RPT)
          doFault(FAULT_IPR, ill_proc, "no rpt allowed for instruction");
      }

    ///
    /// executeInstruction: Non-restart processing
    ///                     Initialize address registers
    ///

    TPR.CA = address;
    iefpFinalAddress = TPR . CA;
    rY = TPR.CA;

    if (!switches . append_after)
    {
        if (info->ndes == 0 && a && (info->flags & TRANSFER_INS))
        {
#if 0
            if (get_addr_mode () == BAR_mode)
              set_addr_mode(APPEND_BAR_mode);
            else
#endif
              set_addr_mode(APPEND_mode);
        }
    }

restart_1:

///
/// executeInstruction: Initialize state saving registers
///

    // XXX this may be wrong; make sure that the right value is used
    // if a page fault occurs. (i.e. this may belong above restart_1.
    ci->stiTally = cu.IR & I_TALLY;   //TSTF(cu.IR, I_TALLY);  // for sti instruction
   
///
/// executeInstruction: simh hooks
///

    // XXX Don't trace Multics idle loop
    if (PPR.PSR != 061 && PPR.IC != 0307)

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
    if (! (cu . IR & I_MIIF))
      {
        if (! ci -> a)
          {
            TPR.TRR = PPR.PRR;
            TPR.TSR = PPR.PSR;
          }
      }

    du . JMP = info -> ndes;

///
/// executeInstruction: RPT/RPD special processing for 'first time'
///

    // possible states
    // repeat_first rpt rd    do it?
    //       f       f   f      y
    //       t       x   x      y
    //       f       t   x      n
    //       f       x   t      n

    if (cu . rpt || cu . rd)
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

        sim_debug (DBG_TRACE, & cpu_dev,
                   "RPT/RPD first %d rpt %d rd %d e/o %d X0 %06o a %d b %d\n", 
                   cu . repeat_first, cu . rpt, cu . rd, PPR . IC & 1, 
                   rX [0], !! (rX [0] & 01000), !! (rX [0] & 0400));
        sim_debug (DBG_TRACE, & cpu_dev,
                   "RPT/RPD CA %06o\n", TPR . CA);

// Handle first time of a RPT or RPD

        if (cu . repeat_first)
          {
            // The semantics of these are that even is the first instruction of
            // and RPD, and odd the second.

            bool icOdd = !! (PPR . IC & 1);
            bool icEven = ! icOdd;

//if (cu.rd && (rX [0] & 01000) && (rX [0] & 0400) && (cu . delta != 0)) sim_printf ("rd %d %012llo\n", PPR.IC & 1, cu .IWB);
            // If RPT or (RPD and the odd instruction)
            if (cu . rpt || (cu . rd && icOdd))
              cu . repeat_first = false;

            // a:RJ78/rpd6
            // For the first execution of the repeated instruction: 
            // C(C(PPR.IC)+1)0,17 + C(Xn) → y, y → C(Xn)
            if (cu . rpt ||              // rpt
                (cu . rd && icEven) ||   // rpd & even
                (cu . rd && icOdd))      // rpd & odd
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
                           "rpt/rd repeat first; offset is %06o\n", offset);

                word6 Td = GET_TD (ci -> tag);
                uint Xn = X (Td);  // Get Xn of next instruction
                sim_debug (DBG_TRACE, & cpu_dev, 
                           "rpt/rd repeat first; X%d was %06o\n", 
                           Xn, rX [Xn]);
                // a:RJ78/rpd5
                rX [Xn] = (rX [Xn] + offset) & AMASK;
                sim_debug (DBG_TRACE, & cpu_dev, 
                           "rpt/rd repeat first; X%d now %06o\n", 
                           Xn, rX [Xn]);
              }
          }
      } // cu . rpt || cu . rd

///
/// executeInstruction: EIS operand processing
///

    if (info -> ndes > 0)
      {
        // This must not happen on instruction restart
        if (! (cu . IR & I_MIIF))
          {
//sim_debug (DBG_TRACEEXT, & cpu_dev, "EIS start\n");
            du . CHTALLY = 0;
            du . Z = 1;
          }
//else {sim_debug (DBG_TRACEEXT, & cpu_dev, "EIS restart, tally %d\n", du . CHTALLY);}
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
            TPR.TRR = PPR.PRR;
            TPR.TSR = PPR.PSR;
            Read (PPR . IC + 1 + n, & currentEISinstruction . op [n], EIS_OPERAND_READ, 0); // I think.
          }
        setupEISoperands ();
      }
    else

///
/// executeInstruction: non-EIS operand processing
///

      {
        // This must not happen on instruction restart
        if (! (cu . IR & I_MIIF))
          {
            if (ci -> a)   // if A bit set set-up TPR stuff ...
              {
                doPtrReg ();

// Putting the a29 clear here makes sense, but breaks the emulator for unclear
// reasons (possibly ABSA?). Do it in updateIWB instead
//                ci -> a = false;
//                // Don't clear a; it is needed to detect change to appending mode 
//                //a = false;
//                putbits36 (& cu . IWB, 29,  1, 0);
              }
            else
              {
                TPR . TBR = 0;
                //if (get_addr_mode () == ABSOLUTE_mode || get_addr_mode () == BAR_mode)
                if (get_addr_mode() == ABSOLUTE_mode)
                  {
                    TPR . TSR = PPR . PSR;
                    TPR . TRR = 0;
                    RSDWH_R1 = 0;
                  }
                clr_went_appending ();
              }
            // This must not happen on instruction restart
#ifdef ABUSE_CT_HOLD
            cu . coFlag = false;
#endif
            cu . CT_HOLD = 0; // Clear interrupted IR mode flag
          }


        if (ci->info->flags & PREPARE_CA)
          {
            doComputedAddressFormation ();
            iefpFinalAddress = TPR . CA;
          }
        else if (READOP (ci))
          {
            doComputedAddressFormation ();
            iefpFinalAddress = TPR . CA;
#if 0 // test code
// Test to verify that recalling CAF is stable.
        {
          sim_debug (DBG_ADDRMOD, & cpu_dev, "2nd call\n");
          word18 save = TPR.CA;
          bool savef = characterOperandFlag;
          int saves = characterOperandSize;
          int saveo = characterOperandOffset;
          bool savedof = directOperandFlag;
          word36 savedo = directOperand;

          doComputedAddressFormation ();

          if (save != TPR.CA)
            sim_printf ("XXX save %06o %06o %lld\n", save, TPR . CA, sim_timell ());
          if (savef != characterOperandFlag)
            sim_printf ("XXX savef %o %o %lld\n", savef, characterOperandOffset, sim_timell ());
          if (saves != characterOperandSize)
            sim_printf ("XXX saves %o %o %lld\n", saves, characterOperandSize, sim_timell ());
          if (saveo != characterOperandOffset)
            sim_printf ("XXX saveo %o %o %lld\n", saveo, characterOperandOffset, sim_timell ());
          if (savedof != directOperandFlag)
            sim_printf ("XXX savedof %o %o %lld\n", savedof, directOperandFlag, sim_timell ());
          if (savedo != directOperand)
            sim_printf ("XXX savedo %012llo %012llo %lld\n", savedo, directOperand, sim_timell ());

          sim_debug (DBG_ADDRMOD, & cpu_dev, "back from 2nd call\n");
        }
#endif
            readOperands ();
          }
      }

///
/// executeInstruction: Execute the instruction
///

    t_stat ret = doInstruction ();
    
///
/// executeInstruction: Transfer into append mode
///

    if (switches . append_after)
    {
        if (info->ndes == 0 && a && (info->flags & TRANSFER_INS))
        {
#if 0
            if (get_addr_mode () == BAR_mode)
              set_addr_mode(APPEND_BAR_mode);
            else
#endif
              set_addr_mode(APPEND_mode);
        }
    }

///
/// executeInstruction: Write operand
///

    if (WRITEOP (ci))
      {
        if (! READOP (ci))
          {
            doComputedAddressFormation ();
            iefpFinalAddress = TPR . CA;
          }
        writeOperands ();
      }
    
///
/// executeInstruction: RPT/RPD processing
///


    // The semantics of these are that even is the first instruction of
    // and RPD, and odd the second.

    bool icOdd = !! (PPR . IC & 1);
    bool icEven = ! icOdd;

    // Here, repeat_first means that the instruction just executed was the
    // RPT or RPD; but when the even instruction of a RPD is executed, 
    // repeat_first is still set, since repeat_first cannot be cleared
    // until the odd instruction gets its first execution. Put some
    // ugly logic in to detect that condition.

    bool rf = cu . repeat_first;
    if (rf && cu . rd && icEven)
      rf = false;

    if ((! rf) && (cu . rpt || cu . rd))
      {
        // If we get here, the instruction just executed was a
        // RPT or RPD target instruction, and not the RPT or RPD
        // instruction itself

        // Add delta to index register.

        bool rptA = !! (rX [0] & 01000);
        bool rptB = !! (rX [0] & 00400);

        sim_debug (DBG_TRACE, & cpu_dev,
            "RPT/RPD delta first %d rf %d rpt %d rd %d "
            "e/o %d X0 %06o a %d b %d\n", 
            cu . repeat_first, rf, cu . rpt, cu . rd, icOdd,
            rX [0], rptA, rptB);

        if (cu . rpt) // rpt
          {
            uint Xn = getbits36 (cu . IWB, 36 - 3, 3);
            rX[Xn] = (rX[Xn] + cu . delta) & AMASK;
            sim_debug (DBG_TRACE, & cpu_dev,
                       "RPT/RPD delta; X%d now %06o\n", Xn, rX [Xn]);
          }


        // a:RJ78/rpd6
        // We know that the X register is not to be incremented until
        // after both instructions have executed, so the following
        // if uses icOdd instead of the more sensical icEven.
        if (cu . rd && icOdd && rptA) // rpd, even instruction
          {
            // a:RJ78/rpd7
            uint Xn = getbits36 (cu . IWB, 36 - 3, 3);
            rX[Xn] = (rX[Xn] + cu . delta) & AMASK;
            sim_debug (DBG_TRACE, & cpu_dev,
                       "RPT/RPD delta; X%d now %06o\n", Xn, rX [Xn]);
          }

        if (cu . rd && icOdd && rptB) // rpdb, odd instruction
          {
            // a:RJ78/rpd8
            uint Xn = getbits36 (cu . IRODD, 36 - 3, 3);
            rX[Xn] = (rX[Xn] + cu . delta) & AMASK;
            sim_debug (DBG_TRACE, & cpu_dev,
                       "RPT/RPD delta; X%d now %06o\n", Xn, rX [Xn]);
          }

        // Check for termination conditions.

        if (cu . rpt || (cu . rd && icOdd))
          {
            bool exit = false;
            // The repetition cycle consists of the following steps:
            //  a. Execute the repeated instruction
            //  b. C(X0)0,7 - 1 -> C(X0)0,7
            uint x = bitfieldExtract (rX [0], 10, 8);
            x -= 1;
            rX [0] = bitfieldInsert (rX [0], x, 10, 8);

            //sim_debug (DBG_TRACE, & cpu_dev, "x %03o rX[0] %06o\n", x, rX[0]);
                    
            //  c. If C(X0)0,7 = 0, then set the tally runout indicator ON 
            //     and terminate

            sim_debug (DBG_TRACE, & cpu_dev, "tally %d\n", x);
            if (x == 0)
              {
                sim_debug (DBG_TRACE, & cpu_dev, "tally runout\n");
                SETF(cu.IR, I_TALLY);
                exit = true;
              } 
            else
              {
                sim_debug (DBG_TRACE, & cpu_dev, "not tally runout\n");
                CLRF(cu.IR, I_TALLY);
              }

            //  d. If a terminate condition has been met, then set 
            //     the tally runout indicator OFF and terminate

            if (TSTF(cu.IR, I_ZERO) && (rX[0] & 0100))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is zero terminate\n");
                CLRF(cu.IR, I_TALLY);
                exit = true;
              }
            if (!TSTF(cu.IR, I_ZERO) && (rX[0] & 040))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is not zero terminate\n");
                CLRF(cu.IR, I_TALLY);
                exit = true;
              } 
            if (TSTF(cu.IR, I_NEG) && (rX[0] & 020))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is neg terminate\n");
                CLRF(cu.IR, I_TALLY);
                exit = true;
              } 
            if (!TSTF(cu.IR, I_NEG) && (rX[0] & 010))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is not neg terminate\n");
                CLRF(cu.IR, I_TALLY);
                exit = true;
              } 
            if (TSTF(cu.IR, I_CARRY) && (rX[0] & 04))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is carry terminate\n");
                CLRF(cu.IR, I_TALLY);
                exit = true;
              } 
            if (!TSTF(cu.IR, I_CARRY) && (rX[0] & 02))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is not carry terminate\n");
                CLRF(cu.IR, I_TALLY);
                exit = true;
              } 
            if (TSTF(cu.IR, I_OFLOW) && (rX[0] & 01))
              {
                sim_debug (DBG_TRACE, & cpu_dev, "is overflow terminate\n");
                CLRF(cu.IR, I_TALLY);
                exit = true;
              }

            if (exit)
              {
                cu . rpt = false;
                cu . rd = false;
              }
            else
              {
                sim_debug (DBG_TRACE, & cpu_dev, "not terminate\n");
              }
          } // if (cu . rpt || cu . rd & (PPR.IC & 1))
      } // (! rf) && (cu . rpt || cu . rd)

///
/// executeInstruction: simh hooks
///

    sys_stats . total_cycles += 1; // bump cycle counter
    
    //if ((cpu_dev.dctrl & DBG_REGDUMP) && sim_deb)
    if_sim_debug (DBG_REGDUMP, & cpu_dev)
    {
        sim_debug(DBG_REGDUMPAQI, &cpu_dev, "A=%012llo Q=%012llo IR:%s\n", rA, rQ, dumpFlags(cu.IR));
        
        sim_debug(DBG_REGDUMPFLT, &cpu_dev, "E=%03o A=%012llo Q=%012llo %.10Lg\n", rE, rA, rQ, EAQToIEEElongdouble());
        
        sim_debug(DBG_REGDUMPIDX, &cpu_dev, "X[0]=%06o X[1]=%06o X[2]=%06o X[3]=%06o\n", rX[0], rX[1], rX[2], rX[3]);
        sim_debug(DBG_REGDUMPIDX, &cpu_dev, "X[4]=%06o X[5]=%06o X[6]=%06o X[7]=%06o\n", rX[4], rX[5], rX[6], rX[7]);
        for(int n = 0 ; n < 8 ; n++)
        {
            sim_debug(DBG_REGDUMPPR, &cpu_dev, "PR%d/%s: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\n",
                      n, PRalias[n], PR[n].SNR, PR[n].RNR, PR[n].WORDNO, PR[n].BITNO);
        }
        for(int n = 0 ; n < 8 ; n++)
            sim_debug(DBG_REGDUMPADR, &cpu_dev, "AR%d: WORDNO=%06o CHAR:%o BITNO:%02o\n",
                  n, AR[n].WORDNO, GET_AR_CHAR (n) /* AR[n].CHAR */, GET_AR_BITNO (n) /* AR[n].ABITNO */);
        sim_debug(DBG_REGDUMPPPR, &cpu_dev, "PRR:%o PSR:%05o P:%o IC:%06o\n", PPR.PRR, PPR.PSR, PPR.P, PPR.IC);
        sim_debug(DBG_REGDUMPDSBR, &cpu_dev, "ADDR:%08o BND:%05o U:%o STACK:%04o\n", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK);
    }
    
///
/// executeInstruction: done. (Whew!)
///

    return ret;
}

static t_stat DoBasicInstruction (void);
static t_stat DoEISInstruction (void);


// Return values
//  CONT_TRA
//  STOP_UNIMP
//  STOP_ILLOP
//  emCall()
//     STOP_HALT
//  scu_sscr()
//     CONT_FAULT (faults)
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
    DCDstruct * i = & currentInstruction;
    CLRF(cu.IR, I_MIIF);
    
    return i->opcodeX ? DoEISInstruction () : DoBasicInstruction ();
}

// CANFAULT 
static t_stat DoBasicInstruction (void)
{
    DCDstruct * i = & currentInstruction;
    uint opcode  = i->opcode;  // get opcode
    
    switch (opcode)
    {
        //    FIXED-POINT ARITHMETIC INSTRUCTIONS
        
        /// ￼Fixed-Point Data Movement Load
        case 0635:  ///< eaa
            rA = 0;
            SETHI(rA, TPR.CA);
            
            SCF(TPR.CA == 0, cu.IR, I_ZERO);
            SCF(TPR.CA & SIGN18, cu.IR, I_NEG);
            
            break;
            
        case 0636:  ///< eaq
            rQ = 0;
            SETHI(rQ, TPR.CA);

            SCF(TPR.CA == 0, cu.IR, I_ZERO);
            SCF(TPR.CA & SIGN18, cu.IR, I_NEG);
            break;

        case 0620:  ///< eax0
        case 0621:  ///< eax1
        case 0622:  ///< eax2
        case 0623:  ///< eax3
        case 0624:  ///< eax4
        case 0625:  ///< eax5
        case 0626:  ///< eax6
        case 0627:  ///< eax7
            {
                uint32 n = opcode & 07;  ///< get n
                rX[n] = TPR.CA;

                SCF(TPR.CA == 0, cu.IR, I_ZERO);
                SCF(TPR.CA & SIGN18, cu.IR, I_NEG);
            }
            break;
            
        case 0335:  ///< lca
          {
            bool ovf;
            rA = compl36 (CY, & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "lca overflow fault");
              }
          }
          break;
            
        case 0336:  ///< lcq
          {
            bool ovf;
            rQ = compl36 (CY, & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "lcq overflow fault");
              }
          }
          break;
            
        case 0320:  ///< lcx0
        case 0321:  ///< lcx1
        case 0322:  ///< lcx2
        case 0323:  ///< lcx3
        case 0324:  ///< lcx4
        case 0325:  ///< lcx5
        case 0326:  ///< lcx6
        case 0327:  ///< lcx7
          {
	  // XXX ToDo: Attempted repetition with the rpl instruction and with
	  // the same register given as target and modifier causes an illegal
	  // procedure fault.

            bool ovf;
            uint32 n = opcode & 07;  // get n
            rX [n] = compl18 (GETHI (CY), & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "lcxn overflow fault");
              }
          }
          break;
            
        case 0337:  // lcaq
          {
	  // The lcaq instruction changes the number to its negative while
	  // moving it from Y-pair to AQ. The operation is executed by
	  // forming the twos complement of the string of 72 bits. In twos
	  // complement arithmetic, the value 0 is its own negative. An
	  // overflow condition exists if C(Y-pair) = -2**71.
            
            if (Ypair[0] == 0400000000000LL && Ypair[1] == 0)
              {
                SETF(cu.IR, I_OFLOW);
                if (tstOVFfault ())
                    doFault(FAULT_OFL, 0, "lcaq overflow fault");
              }
            else if (Ypair[0] == 0 && Ypair[1] == 0)
              {
                rA = 0;
                rQ = 0;
                
                SETF(cu.IR, I_ZERO);
                CLRF(cu.IR, I_NEG);
              }
            else
              {
                word72 tmp72 = 0;

                tmp72 = bitfieldInsert72(tmp72, Ypair[0], 36, 36);
                tmp72 = bitfieldInsert72(tmp72, Ypair[1],  0, 36);
            
                tmp72 = ~tmp72 + 1;
            
                rA = bitfieldExtract72(tmp72, 36, 36);
                rQ = bitfieldExtract72(tmp72,  0, 36);
            
                if (rA == 0 && rQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);

                if (rA & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
              }
          }
          break;

        case 0235:  ///< lda
            rA = CY;
            
            if (rA == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            if (rA & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;

        case 0034: ///< ldac
            rA = CY;
            
            if (rA == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            if (rA & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            CY = 0;
            
            break;
            
        case 0237:  ///< ldaq
            rA = Ypair[0];
            rQ = Ypair[1];
            
            if (rA == 0 && rQ == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            if (rA & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            break;
            
            
        case 0634:  ///< ldi
            {
            // C(Y)18,31 -> C(IR)

            word18 tmp18 = GETLO(CY) & 0777760;  // upper 14-bits of lower 18-bits

            bool bAbsPriv = (get_addr_mode() == ABSOLUTE_mode) || is_priv_mode();

            SCF(tmp18 & I_ZERO,  cu.IR, I_ZERO);
            SCF(tmp18 & I_NEG,   cu.IR, I_NEG);
            SCF(tmp18 & I_CARRY, cu.IR, I_CARRY);
            SCF(tmp18 & I_OFLOW, cu.IR, I_OFLOW);
            SCF(tmp18 & I_EOFL,  cu.IR, I_EOFL);
            SCF(tmp18 & I_EUFL,  cu.IR, I_EUFL);
            SCF(tmp18 & I_OMASK, cu.IR, I_OMASK);
            SCF(tmp18 & I_TALLY, cu.IR, I_TALLY);
            SCF(tmp18 & I_PERR,  cu.IR, I_PERR);
            //SCF(bAbsPriv && (cu.IR & I_PMASK), cu.IR, I_PMASK);
            if (bAbsPriv)
              SCF(tmp18 & I_PMASK, cu.IR, I_PMASK);
            SCF(tmp18 & I_TRUNC, cu.IR, I_TRUNC);
            if (bAbsPriv)
              SCF(cu.IR & I_MIIF, cu.IR, I_MIIF);

            // Indicators:
            //  Parity Mask:
            //      If C(Y)27 = 1, and the processor is in absolute or instruction privileged mode, then ON; otherwise OFF.
            //      This indicator is not affected in the normal or BAR modes.
            //  Not BAR mode:
            //      Cannot be changed by the ldi instruction
            //  MIIF:
            //      If C(Y)30 = 1, and the processor is in absolute or instruction privileged mode, then ON; otherwise OFF.
            //      This indicator is not affected in normal or BAR modes.
            //  Absolute mode:
            //      Cannot be changed by the ldi instruction
            //  All others: If corresponding bit in C(Y) is 1, then ON; otherwise, OFF
            }

            break;
            
        case 0236:  ///< ldq
            rQ = CY;
            
            if (rQ == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            if (rQ & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;
            
        case 0032: ///< ldqc
            rQ = CY;

            CY = 0;
            
            if (rQ == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            if (rQ & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;

        case 0220:  ///< ldx0
        case 0221:  ///< ldx1
        case 0222:  ///< ldx2
        case 0223:  ///< ldx3
        case 0224:  ///< ldx4
        case 0225:  ///< ldx5
        case 0226:  ///< ldx6
        case 0227:  ///< ldx7
            {
                uint32 n = opcode & 07;  // get n
                rX[n] = GETHI(CY);
            
                if (rX[n] == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                if (rX[n] & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0073:   ///< lreg
            //ReadN(i, 8, TPR.CA, Yblock8, OperandRead, rTAG); // read 8-words from memory
            
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
            {
                uint32 n = opcode & 07;  // get n
                rX[n] = GETLO(CY);
                if (rX[n] == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                if (rX[n] & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
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
#ifdef REAL_TR
            Yblock8[7] = ((getTR (NULL) & MASK27) << 9) | (rRALR & 07);    // needs checking
#else
            Yblock8[7] = ((rTR & MASK27) << 9) | (rRALR & 07);    // needs checking
#endif
                    
            //WriteN(i, 8, TPR.CA, Yblock8, OperandWrite, rTAG); // write 8-words to memory
            
            break;

        case 0755:  ///< sta
            CY = rA;
            break;
            
        case 0354:  ///< stac
            if (CY == 0)
            {
                SETF(cu.IR, I_ZERO);
                CY = rA;
            } else
                CLRF(cu.IR, I_ZERO);
            break;
            
        case 0654:  ///< stacq
            if (CY == rQ)
            {
                CY = rA;
                SETF(cu.IR, I_ZERO);
                
            } else
                CLRF(cu.IR, I_ZERO);
            break;
            
        case 0757:  ///< staq
            Ypair[0] = rA;
            Ypair[1] = rQ;
            
            break;
            
        case 0551:  ///< stba
            // 9-bit bytes of C(A) -> corresponding bytes of C(Y), the byte 
            // positions affected being specified in the TAG field.
            copyBytes((i->tag >> 2) & 0xf, rA, &CY);
            break;
            
        case 0552:  ///< stbq
            // 9-bit bytes of C(Q) -> corresponding bytes of C(Y), the byte 
            // positions affected being specified in the TAG field.
            copyBytes((i->tag >> 2) & 0xf, rQ, &CY);
            
            break;

        case 0554:  ///< stc1
            // "C(Y)25 reflects the state of the tally runout indicator
            // prior to modification.
            SETHI(CY, (PPR.IC + 1) & MASK18);
            SETLO(CY, cu.IR & 0777760);
            if (i -> stiTally)
              SETF(CY, I_TALLY);
            else
              CLRF(CY, I_TALLY);
            break;
            
        case 0750:  ///< stc2
            // AL-39 doesn't specify if the low half is set to zero,
            // set to IR, or left unchanged
            // RJ78 specifies unchanged
            SETHI(CY, (PPR.IC + 2) & MASK18);

            break;
            
        case 0751: //< stca
            /// Characters of C(A) -> corresponding characters of C(Y), 
            /// the character positions affected being specified in the TAG 
            /// field.
            copyChars(i->tag, rA, &CY);

            break;
            
        case 0752: ///< stcq
            /// Characters of C(Q) -> corresponding characters of C(Y), the 
            /// character positions affected being specified in the TAG field.
            copyChars(i->tag, rQ, &CY);
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
            
            Ypair[0] = 0;
            //Ypair[0] = bitfieldInsert36(Ypair[0], PPR.PSR, 18, 15);
            //Ypair[0] = bitfieldInsert36(Ypair[0], PPR.PRR, 15,  3);
            //Ypair[0] = bitfieldInsert36(Ypair[0],     043,  0,  6);
            putbits36 (& Ypair [0],  3, 15, PPR.PSR);
            putbits36 (& Ypair [0], 18,  3, PPR.PRR);
            putbits36 (& Ypair [0], 30,  6,     043);
            
            Ypair[1] = 0;
            //Ypair[1] = bitfieldInsert36(Ypair[0], PPR.IC + 2, 18, 18);
            putbits36(& Ypair [1],  0, 18, PPR.IC + 2);
            
            break;
            
            
        case 0754: ///< sti
            
            // C(IR) -> C(Y)18,31
            // 00...0 -> C(Y)32,35
            
            /// The contents of the indicator register after address
            /// preparation are stored in C(Y)18,31  C(Y)18,31 reflects the 
            /// state of the tally runout indicator prior to address 
            /// preparation. The relation between C(Y)18,31 and the indicators 
            /// is given in Table 4-5.
            
            SETLO(CY, (cu.IR & 0000000777760LL));
            if (i -> stiTally)
              SETF(CY, I_TALLY);
            else
              CLRF(CY, I_TALLY);
            if (switches . invert_absolute)
              CY ^= 020;
            break;
            
        case 0756: ///< stq
            CY = rQ;
            break;

        case 0454:  ///< stt
#ifdef REAL_TR
             CY = (getTR (NULL) & MASK27) << 9;
#else
             CY = (rTR & MASK27) << 9;
#endif
             break;
            
            
        case 0740:  ///< stx0
        case 0741:  ///< stx1
        case 0742:  ///< stx2
        case 0743:  ///< stx3
        case 0744:  ///< stx4
        case 0745:  ///< stx5
        case 0746:  ///< stx6
        case 0747:  ///< stx7
            {
                uint32 n = opcode & 07;  // get n
                SETHI(CY, rX[n]);
            }
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
            {
                uint32 n = opcode & 07;  // get n
            
                SETLO(CY, rX[n]);
            }
            break;
        
        /// Fixed-Point Data Movement Shift
        case 0775:  ///< alr
            {
                word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
                for(uint i = 0 ; i < tmp36 ; i++)
                {
                    bool a0 = rA & SIGN36;    ///< A0
                    rA <<= 1;               // shift left 1
                    if (a0)                 // rotate A0 -> A35
                        rA |= 1;
                }
                rA &= DMASK;    // keep to 36-bits
            
                if (rA == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (rA & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
        
        case 0735:  ///< als
            {
                word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
            
                word36 tmpSign = rA & SIGN36;
                CLRF(cu.IR, I_CARRY);

                for (uint i = 0; i < tmp36; i ++)
                {
                    rA <<= 1;
                    if (tmpSign != (rA & SIGN36))
                        SETF(cu.IR, I_CARRY);
                }
                rA &= DMASK;    // keep to 36-bits 

                if (rA == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (rA & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0771:  ///< arl
            /// Shift C(A) right the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with zeros.
            {
                rA &= DMASK; // Make sure the shifted in bits are 0
                word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17

                rA >>= tmp36;
                rA &= DMASK;    // keep to 36-bits

                if (rA == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (rA & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
            
        case 0731:  // ars
          {
	  // Shift C(A) right the number of positions given in
	  // C(TPR.CA)11,17; filling vacated positions with initial C(A)0.
            
            rA &= DMASK; // Make sure the shifted in bits are 0
            word18 tmp18 = TPR.CA & 0177;   // CY bits 11-17

            bool a0 = rA & SIGN36;    // A0
            for (uint i = 0 ; i < tmp18 ; i ++)
              {
                rA >>= 1;               // shift right 1
                if (a0)                 // propagate sign bit
                    rA |= SIGN36;
              }
            rA &= DMASK;    // keep to 36-bits
            
            if (rA == 0)
                SETF (cu . IR, I_ZERO);
            else
                CLRF (cu . IR, I_ZERO);
            
            if (rA & SIGN36)
                SETF (cu . IR, I_NEG);
            else
                CLRF (cu . IR, I_NEG);
          }
          break;

        case 0777:  ///< llr

            /// Shift C(AQ) left by the number of positions given in C(TPR.CA)11,17;
            /// entering each bit leaving AQ0 into AQ71.

            {
                word36 tmp36 = TPR.CA & 0177;      // CY bits 11-17
                for(uint i = 0 ; i < tmp36 ; i++)
                {
                    bool a0 = rA & SIGN36;    ///< A0
                
                    rA <<= 1;               // shift left 1
                
                    bool b0 = rQ & SIGN36;    ///< Q0
                    if (b0)
                        rA |= 1;            // Q0 => A35
                
                    rQ <<= 1;               // shift left 1
                
                    if (a0)                 // propagate A sign bit
                        rQ |= 1;
                }
            
                rA &= DMASK;    // keep to 36-bits
                rQ &= DMASK;
            
                if (rA == 0 && rQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                if (rA & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
            
        case 0737:  // lls
          {
	  // Shift C(AQ) left the number of positions given in
	  // C(TPR.CA)11,17; filling vacated positions with zeros.

            CLRF (cu . IR, I_CARRY);
 
            word36 tmp36 = TPR . CA & 0177;   // CY bits 11-17
            word36 tmpSign = rA & SIGN36;
            for(uint i = 0 ; i < tmp36 ; i ++)
              {
                rA <<= 1;               // shift left 1
            
                if (tmpSign != (rA & SIGN36))
                  SETF (cu . IR, I_CARRY);
                
                bool b0 = rQ & SIGN36;    ///< Q0
                if (b0)
                  rA |= 1;            // Q0 => A35
                    
                rQ <<= 1;               // shift left 1
              }
                                
            rA &= DMASK;    // keep to 36-bits
            rQ &= DMASK;
                
            if (rA == 0 && rQ == 0)
              SETF (cu . IR, I_ZERO);
            else
              CLRF (cu . IR, I_ZERO);

            if (rA & SIGN36)
              SETF (cu . IR, I_NEG);
            else
              CLRF (cu . IR, I_NEG);
          }
          break;

        case 0773:  ///< lrl
            /// Shift C(AQ) right the number of positions given in C(TPR.CA)11,17; filling
            /// vacated positions with zeros.
            {
                rA &= DMASK; // Make sure the shifted in bits are 0
                rQ &= DMASK; // Make sure the shifted in bits are 0
                word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
                for(uint i = 0 ; i < tmp36 ; i++)
                {
                    bool a35 = rA & 1;      ///< A35
                    rA >>= 1;               // shift right 1
                    
                    rQ >>= 1;               // shift right 1
                    
                    if (a35)                // propagate sign bit
                        rQ |= SIGN36;
                }
                rA &= DMASK;    // keep to 36-bits
                rQ &= DMASK;
                
                if (rA == 0 && rQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                if (rA & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0733:  // lrs
          {
	  // Shift C(AQ) right the number of positions given in
	  // C(TPR.CA)11,17; filling vacated positions with initial C(AQ)0.

            word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
            rA &= DMASK; // Make sure the shifted in bits are 0
            rQ &= DMASK; // Make sure the shifted in bits are 0
            bool a0 = rA & SIGN36;    ///< A0

            for (uint i = 0 ; i < tmp36 ; i ++)
              {
                bool a35 = rA & 1;      // A35
                    
                rA >>= 1;               // shift right 1
                if (a0)
                  rA |= SIGN36;
                    
                rQ >>= 1;               // shift right 1
                if (a35)                // propagate sign bit1
                  rQ |= SIGN36;
              }
            rA &= DMASK;    // keep to 36-bits (probably ain't necessary)
            rQ &= DMASK;
                
            if (rA == 0 && rQ == 0)
              SETF (cu . IR, I_ZERO);
            else
              CLRF (cu . IR, I_ZERO);

            if (rA & SIGN36)
              SETF (cu . IR, I_NEG);
            else
              CLRF (cu . IR, I_NEG);
          }
          break;
         
        case 0776:  ///< qlr
            /// Shift C(Q) left the number of positions given in C(TPR.CA)11,17; entering
            /// each bit leaving Q0 into Q35.
            {
                word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
                for(uint i = 0 ; i < tmp36 ; i++)
                {
                    bool q0 = rQ & SIGN36;    ///< Q0
                    rQ <<= 1;               // shift left 1
                    if (q0)                 // rotate A0 -> A35
                        rQ |= 1;
                }
                rQ &= DMASK;    // keep to 36-bits
                
                if (rQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                
                if (rQ & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
    
        case 0736:  ///< qls
            // Shift C(Q) left the number of positions given in C(TPR.CA)11,17; fill vacated positions with zeros.
            {
                word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
                word36 tmpSign = rQ & SIGN36;
                CLRF(cu.IR, I_CARRY);

                for (uint i = 0; i < tmp36; i ++)
                {
                    rQ <<= 1;
                    if (tmpSign != (rQ & SIGN36))
                        SETF(cu.IR, I_CARRY);
                }
                rQ &= DMASK;    // keep to 36-bits 
    
                if (rQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                
                if (rQ & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0772:  ///< qrl
            /// Shift C(Q) right the number of positions specified by Y11,17; fill vacated
            /// positions with zeros.
            {
                word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
            
                rQ &= DMASK;    // Make sure the shifted in bits are 0
                rQ >>= tmp36;
                rQ &= DMASK;    // keep to 36-bits
            
                if (rQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                
                if (rQ & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            
            }
            break;

        case 0732:  // qrs
          {
	  // Shift C(Q) right the number of positions given in
	  // C(TPR.CA)11,17; filling vacated positions with initial C(Q)0.

            rQ &= DMASK; // Make sure the shifted in bits are 0
            word36 tmp36 = TPR.CA & 0177;   // CY bits 11-17
            bool q0 = rQ & SIGN36;    // Q0
            for(uint i = 0 ; i < tmp36 ; i++)
              {
                rQ >>= 1;               // shift right 1
                if (q0)                 // propagate sign bit
                  rQ |= SIGN36;
              }
            rQ &= DMASK;    // keep to 36-bits
                
            if (rQ == 0)
              SETF (cu . IR, I_ZERO);
            else
              CLRF (cu . IR, I_ZERO);
                
            if (rQ & SIGN36)
              SETF (cu . IR, I_NEG);
            else
              CLRF (cu . IR, I_NEG);
          }
          break;

        /// Fixed-Point Addition

        case 0075:  ///< ada
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

            bool ovf;
            rA = Add36b (rA, CY, 0, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "ada overflow fault");
              }
          }
          break;
         
        case 0077:   // adaq
          {
            // C(AQ) + C(Y-pair) -> C(AQ)
            bool ovf;
            word72 tmp72 = YPAIRTO72 (Ypair);
            tmp72 = Add72b (convertToWord72 (rA, rQ), tmp72, 0,
                            I_ZERO | I_NEG | I_OFLOW | I_CARRY, & cu . IR,
                            & ovf);
            convertToWord36 (tmp72, & rA, & rQ);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "adaq overflow fault");
              }
          }
          break;
            
        case 0033:   // adl
          {
            // C(AQ) + C(Y) sign extended -> C(AQ)
            bool ovf;
            word72 tmp72 = SIGNEXT36_72 (CY); // sign extend Cy
            tmp72 = Add72b (convertToWord72 (rA, rQ), tmp72, 0,
                            I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                            & cu . IR, & ovf);
            convertToWord36 (tmp72, & rA, & rQ);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "adl overflow fault");
              }
          }
          break;
            
            
        case 0037:   // adlaq
          {
	  // The adlaq instruction is identical to the adaq instruction with
	  // the exception that the overflow indicator is not affected by the
	  // adlaq instruction, nor does an overflow fault occur. Operands
	  // and results are treated as unsigned, positive binary integers.
            bool ovf;
            word72 tmp72 = YPAIRTO72 (Ypair);
        
            tmp72 = Add72b (convertToWord72 (rA, rQ), tmp72, 0,
                            I_ZERO | I_NEG | I_CARRY, & cu . IR, & ovf);
            convertToWord36 (tmp72, & rA, & rQ);
          }
          break;
            
        case 0035:   // adla
          {
	  // The adla instruction is identical to the ada instruction with
	  // the exception that the overflow indicator is not affected by the
	  // adla instruction, nor does an overflow fault occur. Operands and
	  // results are treated as unsigned, positive binary integers. */

            bool ovf;
            rA = Add36b (rA, CY, 0, I_ZERO | I_NEG | I_CARRY, & cu . IR, & ovf);
          }
          break;
            
        case 0036:   ///< adlq
          {
	  // The adlq instruction is identical to the adq instruction with
	  // the exception that the overflow indicator is not affected by the
	  // adlq instruction, nor does an overflow fault occur. Operands and
	  // results are treated as unsigned, positive binary integers. */

            bool ovf;
            rQ = Add36b (rQ, CY, 0, I_ZERO | I_NEG | I_CARRY, & cu . IR, & ovf);
          }
          break;
            
        case 0020:   ///< adlx0
        case 0021:   ///< adlx1
        case 0022:   ///< adlx2
        case 0023:   ///< adlx3
        case 0024:   ///< adlx4
        case 0025:   ///< adlx5
        case 0026:   ///< adlx6
        case 0027:   ///< adlx7
          {
            bool ovf;
            uint32 n = opcode & 07;  // get n
            rX [n] = Add18b (rX [n], GETHI (CY), 0, I_ZERO | I_NEG | I_CARRY,
                             & cu . IR, & ovf);
          }
          break;
            
        case 0076:   // adq
          {
            bool ovf;
            rQ = Add36b (rQ, CY, 0, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "adq overflow fault");
              }
          }
          break;

        case 0060:   ///< adx0
        case 0061:   ///< adx1
        case 0062:   ///< adx2
        case 0063:   ///< adx3
        case 0064:   ///< adx4
        case 0065:   ///< adx5
        case 0066:   ///< adx6
        case 0067:   ///< adx7
          {
            bool ovf;
            uint32 n = opcode & 07;  // get n
            rX [n] = Add18b (rX [n], GETHI (CY), 0,
                             I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                             & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "adxn overflow fault");
              }
          }
          break;
        
        case 0054:   // aos
          {
            // C(Y)+1->C(Y)
            
            bool ovf;
            CY = Add36b (CY, 1, 0, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "aos overflow fault");
              }
          }
          break;
        
        case 0055:   // asa
          {
            // C(A) + C(Y) -> C(Y)

            bool ovf;
            CY = Add36b (rA, CY, 0, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "asa overflow fault");
              }
          }
          break;
            
        case 0056:   // asq
          {
            // C(Q) + C(Y) -> C(Y)
            bool ovf;
            CY = Add36b (rQ, CY, 0, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "asq overflow fault");
              }
          }
          break;
         
        case 0040:   ///< asx0
        case 0041:   ///< asx1
        case 0042:   ///< asx2
        case 0043:   ///< asx3
        case 0044:   ///< asx4
        case 0045:   ///< asx5
        case 0046:   ///< asx6
        case 0047:   ///< asx7
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            //    \brief C(Xn) + C(Y)0,17 -> C(Y)0,17
            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = Add18b (rX [n], GETHI (CY), 0,
                                   I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                                   & cu . IR, & ovf);
            SETHI (CY, tmp18);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "asxn overflow fault");
              }
          }
          break;

        case 0071:   // awca
          {
            // If carry indicator OFF, then C(A) + C(Y) -> C(A)
            // If carry indicator ON, then C(A) + C(Y) + 1 -> C(A)

            bool ovf;
            rA = Add36b (rA, CY, TSTF (cu . IR, I_CARRY) ? 1 : 0,
                          I_ZERO | I_NEG | I_OFLOW | I_CARRY, & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "awca overflow fault");
              }
          }
          break;
            
        case 0072:   // awcq
          {
            // If carry indicator OFF, then C(Q) + C(Y) -> C(Q)
            // If carry indicator ON, then C(Q) + C(Y) + 1 -> C(Q)

            bool ovf;
            rQ = Add36b (rQ, CY, TSTF (cu . IR, I_CARRY),
                         I_ZERO | I_NEG | I_OFLOW | I_CARRY, & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "ada overflow fault");
              }
          }
          break;
           
        /// Fixed-Point Subtraction
            
        case 0175:  ///< sba
          {
            // C(A) - C(Y) -> C(A)

            bool ovf;
            rA = Sub36b (rA, CY, 1, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "sba overflow fault");
              }
          }
          break;
         
        case 0177:  ///< sbaq
          {
            // C(AQ) - C(Y-pair) -> C(AQ)
            bool ovf;
            word72 tmp72 = YPAIRTO72 (Ypair); 
            tmp72 = Sub72b (convertToWord72 (rA, rQ), tmp72, 1,
                            I_ZERO | I_NEG | I_OFLOW | I_CARRY, & cu . IR,
                            & ovf);
            convertToWord36 (tmp72, & rA, & rQ);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "ada overflow fault");
              }
          }
          break;
          
        case 0135:  // sbla
          {
            // C(A) - C(Y) -> C(A) logical

            bool ovf;
            rA = Sub36b (rA, CY, 1, I_ZERO | I_NEG | I_CARRY, & cu . IR, & ovf);
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
            word72 tmp72 = YPAIRTO72 (Ypair);
        
            tmp72 = Sub72b (convertToWord72 (rA, rQ), 1, tmp72,
                            I_ZERO | I_NEG |  I_CARRY, & cu . IR, & ovf);
            convertToWord36 (tmp72, & rA, & rQ);
          }
          break;
            
        case 0136:  // sblq
          {
            // C(Q) - C(Y) -> C(Q)
            bool ovf;
            rQ = Sub36b (rQ, CY, 1, I_ZERO | I_NEG | I_CARRY, & cu . IR, & ovf);
          }
          break;
            
        case 0120:  ///< sblx0
        case 0121:  ///< sblx1
        case 0122:  ///< sblx2
        case 0123:  ///< sblx3
        case 0124:  ///< sblx4
        case 0125:  ///< sblx5
        case 0126:  ///< sblx6
        case 0127:  ///< sblx7
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            // C(Xn) - C(Y)0,17 -> C(Xn)

            bool ovf;
            uint32 n = opcode & 07;  // get n
            rX [n] = Sub18b (rX [n], GETHI (CY), 1,
                             I_ZERO | I_NEG | I_CARRY, & cu . IR, & ovf);
          }
          break;
         
        case 0176:  ///< sbq
          {
            // C(Q) - C(Y) -> C(Q)

            bool ovf;
            rQ = Sub36b (rQ, CY, 1, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "sbq overflow fault");
              }
          }
          break;
            
        case 0160:  ///< sbx0
        case 0161:  ///< sbx1
        case 0162:  ///< sbx2
        case 0163:  ///< sbx3
        case 0164:  ///< sbx4
        case 0165:  ///< sbx5
        case 0166:  ///< sbx6
        case 0167:  ///< sbx7
          {
            // For n = 0, 1, ..., or 7 as determined by operation code
            // C(Xn) - C(Y)0,17 -> C(Xn)

            bool ovf;
            uint32 n = opcode & 07;  // get n
            rX [n] = Sub18b (rX [n], GETHI (CY), 1,
                             I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                             & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "sbxn overflow fault");
              }
          }
          break;

        case 0155:  // ssa
          {
            // C(A) - C(Y) -> C(Y)

            bool ovf;
            CY = Sub36b (rA, CY, 1, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "ssa overflow fault");
              }
          }
          break;

        case 0156:  // ssq
          {
            // C(Q) - C(Y) -> C(Y)

            bool ovf;
            CY = Sub36b (rQ, CY, 1, I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "ssq overflow fault");
              }
          }
          break;
        
        case 0140:  ///< ssx0
        case 0141:  ///< ssx1
        case 0142:  ///< ssx2
        case 0143:  ///< ssx3
        case 0144:  ///< ssx4
        case 0145:  ///< ssx5
        case 0146:  ///< ssx6
        case 0147:  ///< ssx7
          {
            // For uint32 n = 0, 1, ..., or 7 as determined by operation code
            // C(Xn) - C(Y)0,17 -> C(Y)0,17

            bool ovf;
            uint32 n = opcode & 07;  // get n
            word18 tmp18 = Sub18b (rX [n], GETHI (CY), 1,
                                   I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                                   & cu . IR, & ovf);
            SETHI (CY, tmp18);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "ada overflow fault");
              }
          }
          break;

            
        case 0171:  // swca
          {
            // If carry indicator ON, then C(A)- C(Y) -> C(A)
            // If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

            bool ovf;
            rA = Sub36b (rA, CY, TSTF (cu . IR, I_CARRY) ? 1 : 0,
                         I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "swca overflow fault");
              }
          }
          break;

        case 0172:  // swcq
          {
            // If carry indicator ON, then C(Q) - C(Y) -> C(Q)
            // If carry indicator OFF, then C(Q) - C(Y) - 1 -> C(Q)

            bool ovf;
            rQ = Sub36b (rQ, CY, TSTF (cu . IR, I_CARRY) ? 1 : 0,
                         I_ZERO | I_NEG | I_OFLOW | I_CARRY,
                         & cu . IR, & ovf);
            if (ovf && tstOVFfault ())
              {
                doFault (FAULT_OFL, 0, "ada overflow fault");
              }
          }
          break;
        
        /// Fixed-Point Multiplication

        case 0401:  ///< mpf
            {
            /// C(A) × C(Y) -> C(AQ), left adjusted
            /**
             * Two 36-bit fractional factors (including sign) are multiplied to form a 71- bit fractional product (including sign), which is stored left-adjusted in the AQ register. AQ71 contains a zero. Overflow can occur only in the case of A and Y containing negative 1 and the result exceeding the range of the AQ register.
             */
            {
                word72 tmp72 = (word72)rA * (word72)CY;
                tmp72 &= MASK72;
                tmp72 <<= 1;    // left adjust so AQ71 contains 0
                bool isovr = false;
                if (rA == MAXNEG && CY == MAXNEG) // Overflow can occur only in the case of A and Y containing negative 1
                {
                    SETF(cu.IR, I_OFLOW);
                    isovr = true;
                }

                convertToWord36(tmp72, &rA, &rQ);
                SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
                SCF(rA & SIGN36, cu.IR, I_NEG);
            
                if (isovr && tstOVFfault ())
                    doFault(FAULT_OFL, 0,"mpf overflow fault");
                }
            }
            break;

        case 0402:  ///< mpy
            /// C(Q) × C(Y) -> C(AQ), right adjusted
            
            {
                int64_t t0 = SIGNEXT36_64 (rQ & DMASK);
                int64_t t1 = SIGNEXT36_64 (CY & DMASK);

                __int128_t prod = (__int128_t) t0 * (__int128_t) t1;

                convertToWord36((word72)prod, &rA, &rQ);

                SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
                SCF(rA & SIGN36, cu.IR, I_NEG);
            }
            break;
            
//#define DIV_TRACE
        /// Fixed-Point Division
        case 0506:  ///< div
            /// C(Q) / (Y) integer quotient -> C(Q), integer remainder -> C(A)
            /**
             * A 36-bit integer dividend (including sign) is divided by a 36-bit integer divisor (including sign) to form a 36-bit integer quotient (including sign) and a 36-bit integer remainder (including sign). The remainder sign is equal to the dividend sign unless the remainder is zero.
               If the dividend = -2**35 and the divisor = -1 or if the divisor = 0, then division does not take place. Instead, a divide check fault occurs, C(Q) contains the dividend magnitude, and the negative indicator reflects the dividend sign.
             */
            
            if ((rQ == MAXNEG && CY == NEG136) || (CY == 0))
            {
                // no division takes place
                SCF(CY == 0, cu.IR, I_ZERO);
                SCF(rQ & SIGN36, cu.IR, I_NEG);
                doFault(FAULT_DIV, 0, "div divide check");
            }
            else
            {
                t_int64 dividend = (t_int64) (SIGNEXT36_64(rQ));
                t_int64 divisor = (t_int64) (SIGNEXT36_64(CY));

#ifdef DIV_TRACE
                sim_debug (DBG_CAC, & cpu_dev, "\n");
                sim_debug (DBG_CAC, & cpu_dev, ">>> dividend rQ %lld (%012llo)\n", dividend, rQ);
                sim_debug (DBG_CAC, & cpu_dev, ">>> divisor  CY %lld (%012llo)\n", divisor, CY);
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
                    sim_debug (DBG_CAC, & cpu_dev, ">>> quot 2 %lld\n", quotient);
                    sim_debug (DBG_CAC, & cpu_dev, ">>> rem 2 %lld\n", remainder);
#endif
                  }
#endif

#ifdef DIV_TRACE
                //  (a/b)*b + a%b is equal to a.
                sim_debug (DBG_CAC, & cpu_dev, "dividend was                   = %lld\n", dividend);
                sim_debug (DBG_CAC, & cpu_dev, "quotient * divisor + remainder = %lld\n", quotient * divisor + remainder);
                if (dividend != quotient * divisor + remainder)
                  {
                    sim_debug (DBG_CAC, & cpu_dev, "---------------------------------^^^^^^^^^^^^^^^\n");
                  }
#endif


                if (dividend != quotient * divisor + remainder)
                  {
                    sim_debug (DBG_ERR, & cpu_dev, "Internal division error; rQ %012llo CY %012llo\n", rQ, CY);
                  }

                rA = remainder & DMASK;
                rQ = quotient & DMASK;

#ifdef DIV_TRACE
                sim_debug (DBG_CAC, & cpu_dev, "rA (rem)  %012llo\n", rA);
                sim_debug (DBG_CAC, & cpu_dev, "rQ (quot) %012llo\n", rQ);
#endif

                SCF(rQ == 0, cu.IR, I_ZERO);
                SCF(rQ & SIGN36, cu.IR, I_NEG);
            }
            
            break;
            
        case 0507:  ///< dvf
            /// C(AQ) / (Y)
            ///  fractional quotient -> C(A)
            ///  fractional remainder -> C(Q)
            
            /// A 71-bit fractional dividend (including sign) is divided by a 36-bit fractional divisor yielding a 36-bit fractional quotient (including sign) and a 36-bit fractional remainder (including sign). C(AQ)71 is ignored; bit position 35 of the remainder corresponds to bit position 70 of the dividend. The
            ///  remainder sign is equal to the dividend sign unless the remainder is zero.
            /// If | dividend | >= | divisor | or if the divisor = 0, division does not take place. Instead, a divide check fault occurs, C(AQ) contains the dividend magnitude in absolute, and the negative indicator reflects the dividend sign.
            
            dvf ();
            
            break;
            
        // Fixed-Point Negate
        case 0531:  // neg
            // -C(A) -> C(A) if C(A) ≠ 0

            rA &= DMASK;
            bool ov = rA == 0400000000000ULL;
                
            rA = -rA;
 
            rA &= DMASK;    // keep to 36-bits
                
            if (rA == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (rA & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            if (ov)
            {
                SETF(cu.IR, I_OFLOW);
                if (tstOVFfault ())
                    doFault(FAULT_OFL, 0,"neg overflow fault");
            }
            break;
            
        case 0533:  // negl
            // -C(AQ) -> C(AQ) if C(AQ) ≠ 0
            {
                rA &= DMASK;
                rQ &= DMASK;
                word72 tmp72 = convertToWord72(rA, rQ);

                bool ov = (rA == 0400000000000ULL) & (rQ == 0);
                
                tmp72 = -tmp72;
                
                if (tmp72 == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
                
                if (tmp72 & SIGN72)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
                
                if (ov)
                {
                    SETF(cu.IR, I_OFLOW);
                    if (tstOVFfault ())
                        doFault(FAULT_OFL, 0,"negl overflow fault");
                }
                
                convertToWord36(tmp72, &rA, &rQ);
            }
            break;
            
        /// Fixed-Point Comparison
        case 0405:  ///< cmg
            /// | C(A) | :: | C(Y) |
            /// Zero:     If | C(A) | = | C(Y) | , then ON; otherwise OFF
            /// Negative: If | C(A) | < | C(Y) | , then ON; otherwise OFF
            {
                // This is wrong for MAXNEG
                //word36 a = rA & SIGN36 ? -rA : rA;
                //word36 y = CY & SIGN36 ? -CY : CY;

                // If we do the 64 math, the MAXNEG case works
                t_int64 a = SIGNEXT36_64 (rA);
                if (a < 0)
                  a = -a;
                t_int64 y = SIGNEXT36_64 (CY);
                if (y < 0)
                  y = -y;
                
                SCF(a == y, cu.IR, I_ZERO);
                SCF(a < y,  cu.IR, I_NEG);
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


                SCF(Z == 0, cu.IR, I_ZERO);
                SCF(Z & SIGN36, cu.IR, I_NEG);
            }
            break;
            
        case 0115:  ///< cmpa
            /// C(A) :: C(Y)
        
            cmp36(rA, CY, &cu.IR);
            break;
            
        case 0116:  ///< cmpq
            /// C(Q) :: C(Y)
            cmp36(rQ, CY, &cu.IR);
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
            {
                uint32 n = opcode & 07;  // get n
                cmp18(rX[n], GETHI(CY), &cu.IR);
            }
            break;
            
        case 0111:  ///< cwl
            /// C(Y) :: closed interval [C(A);C(Q)]
            /**
             The cwl instruction tests the value of C(Y) to determine if it is within the range of values set by C(A) and C(Q). The comparison of C(Y) with C(Q) locates C(Y) with respect to the interval if C(Y) is not contained within the
             interval.
             */
            cmp36wl(rA, CY, rQ, &cu.IR);
            break;
            
        case 0117:  ///< cmpaq
            /// C(AQ) :: C(Y-pair)
            {
                word72 tmp72 = YPAIRTO72(Ypair);   //
        
                word72 trAQ = convertToWord72(rA, rQ);
                trAQ &= MASK72;
            
                cmp72(trAQ, tmp72, &cu.IR);
            }
            break;
            
        /// Fixed-Point Miscellaneous
        case 0234:  ///< szn
            /// Set indicators according to C(Y)
            CY &= DMASK;
            if (CY == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (CY & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;
            
        case 0214:  ///< sznc
            /// Set indicators according to C(Y)
            CY &= DMASK;
            if (CY == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (CY & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);

            // ... and clear
            CY = 0;
            break;

        /// BOOLEAN OPERATION INSTRUCTIONS
            
        /// ￼Boolean And
        case 0375:  ///< ana
            /// C(A)i & C(Y)i -> C(A)i for i = (0, 1, ..., 35)
            rA = rA & CY;
            rA &= DMASK;
            
            if (rA == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (rA & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;
            
        case 0377:  ///< anaq
            /// C(AQ)i & C(Y-pair)i -> C(AQ)i for i = (0, 1, ..., 71)
            {
                //!!!
                word72 tmp72 = YPAIRTO72(Ypair);   //
        
        
                word72 trAQ = convertToWord72(rA, rQ);
                trAQ = trAQ & tmp72;
                trAQ &= MASK72;
            
                if (trAQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trAQ & SIGN72)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);

                convertToWord36(trAQ, &rA, &rQ);
            }
            break;
            
        case 0376:  ///< anq
            /// C(Q)i & C(Y)i -> C(Q)i for i = (0, 1, ..., 35)
            rQ = rQ & CY;
            rQ &= DMASK;

            if (rQ == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (rQ & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;

        case 0355:  ///< ansa
            /// C(A)i & C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
            {
                CY = rA & CY;
                CY &= DMASK;
            
                if (CY == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (CY & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
        
        case 0356:  ///< ansq
            /// C(Q)i & C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
            {
                CY = rQ & CY;
                CY &= DMASK;
            
                if (CY == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (CY & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
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
            /// \brief C(Xn)i & C(Y)i -> C(Y)i for i = (0, 1, ..., 17)
            {
                uint32 n = opcode & 07;  // get n
                word18 tmp18 = rX[n] & GETHI(CY);
                tmp18 &= MASK18;
            
                if (tmp18 == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (tmp18 & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            
                SETHI(CY, tmp18);
            }

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
            /// \brief C(Xn)i & C(Y)i -> C(Xn)i for i = (0, 1, ..., 17)
            {
                uint32 n = opcode & 07;  // get n
                rX[n] &= GETHI(CY);
                rX[n] &= MASK18;
            
                if (rX[n] == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (rX[n] & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        /// Boolean Or
        case 0275:  ///< ora
            /// C(A)i | C(Y)i -> C(A)i for i = (0, 1, ..., 35)
            rA = rA | CY;
            rA &= DMASK;
            
            if (rA == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (rA & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;
         
        case 0277:  ///< oraq
            /// C(AQ)i | C(Y-pair)i -> C(AQ)i for i = (0, 1, ..., 71)
            {
                //!!!
                word72 tmp72 = YPAIRTO72(Ypair);   //
        
                word72 trAQ = convertToWord72(rA, rQ);
                trAQ = trAQ | tmp72;
                trAQ &= MASK72;
            
                if (trAQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trAQ & SIGN72)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            
                convertToWord36(trAQ, &rA, &rQ);
            }
            break;

        case 0276:  ///< orq
            /// C(Q)i | C(Y)i -> C(Q)i for i = (0, 1, ..., 35)
            rQ = rQ | CY;
            rQ &= DMASK;

            if (rQ == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (rQ & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;

        case 0255:  ///< orsa
            /// C(A)i | C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
            CY = rA | CY;
            CY &= DMASK;
            
            if (CY == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (CY & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;

        case 0256:  ///< orsq
            /// C(Q)i | C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
            
            CY = rQ | CY;
            CY &= DMASK;

            if (CY == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (CY & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
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
            /// \brief C(Xn)i | C(Y)i -> C(Y)i for i = (0, 1, ..., 17)
            {
                uint32 n = opcode & 07;  // get n
            
                word18 tmp18 = rX[n] | GETHI(CY);
                tmp18 &= MASK18;
           
                if (tmp18 == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (tmp18 & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            
                SETHI(CY, tmp18);
            }
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
            /// \brief C(Xn)i | C(Y)i -> C(Xn)i for i = (0, 1, ..., 17)
            {   
                uint32 n = opcode & 07;  // get n
                rX[n] |= GETHI(CY);
                rX[n] &= MASK18;
           
                if (rX[n] == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (rX[n] & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
           
        /// Boolean Exclusive Or
        case 0675:  ///< era
            /// C(A)i ⊕ C(Y)i -> C(A)i for i = (0, 1, ..., 35)
            rA = rA ^ CY;
            rA &= DMASK;

            if (rA == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (rA & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;

        case 0677:  ///< eraq
            /// C(AQ)i ⊕ C(Y-pair)i -> C(AQ)i for i = (0, 1, ..., 71)
            {
                //!!!
                word72 tmp72 = YPAIRTO72(Ypair);   //
        
                word72 trAQ = convertToWord72(rA, rQ);
                trAQ = trAQ ^ tmp72;
                trAQ &= MASK72;
            
                if (trAQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trAQ & SIGN72)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            
                convertToWord36(trAQ, &rA, &rQ);
            }
            break;

        case 0676:  ///< erq
            /// C(Q)i ⊕ C(Y)i -> C(Q)i for i = (0, 1, ..., 35)
            rQ = rQ ^ CY;
            rQ &= DMASK;
            if (rQ == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (rQ & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            break;

        case 0655:  ///< ersa
            /// C(A)i ⊕ C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
            
            CY = rA ^ CY;
            CY &= DMASK;

            if (CY == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (CY & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            //Write(i, TPR.CA, CY, DataWrite, rTAG);

            break;

        case 0656:  ///< ersq
            /// C(Q)i ⊕ C(Y)i -> C(Y)i for i = (0, 1, ..., 35)
            
            CY = rQ ^ CY;
            CY &= DMASK;

            if (CY == 0)
                SETF(cu.IR, I_ZERO);
            else
                CLRF(cu.IR, I_ZERO);
            
            if (CY & SIGN36)
                SETF(cu.IR, I_NEG);
            else
                CLRF(cu.IR, I_NEG);
            
            //Write(i, TPR.CA, CY, DataWrite, rTAG);

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
            /// \brief C(Xn)i ⊕ C(Y)i -> C(Y)i for i = (0, 1, ..., 17)
            {
                uint32 n = opcode & 07;  // get n
            
                word18 tmp18 = rX[n] ^ GETHI(CY);
                tmp18 &= MASK18;

                if (tmp18 == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (tmp18 & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            
                SETHI(CY, tmp18);
            }
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
            /// \brief C(Xn)i ⊕ C(Y)i -> C(Xn)i for i = (0, 1, ..., 17)
            {
                uint32 n = opcode & 07;  // get n
                rX[n] ^= GETHI(CY);
                rX[n] &= MASK18;
            
                if (rX[n] == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (rX[n] & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        /// Boolean Comparative And
            
        case 0315:  ///< cana
            /// C(Z)i = C(A)i & C(Y)i for i = (0, 1, ..., 35)
            {
                word36 trZ = rA & CY;
                trZ &= MASK36;

                if (trZ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trZ & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0317:  ///< canaq
            /// C(Z)i = C(AQ)i & C(Y-pair)i for i = (0, 1, ..., 71)
            {
                //!!!
                word72 tmp72 = YPAIRTO72(Ypair);   //
        
        
                word72 trAQ = convertToWord72(rA, rQ);
                trAQ = trAQ & tmp72;
                trAQ &= MASK72;

                if (trAQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trAQ & SIGN72)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0316:  ///< canq
            /// C(Z)i = C(Q)i & C(Y)i for i = (0, 1, ..., 35)
            {
                word36 trZ = rQ & CY;
                trZ &= DMASK;

                if (trZ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trZ & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
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
            {
                uint32 n = opcode & 07;  // get n
                word18 tmp18 = rX[n] & GETHI(CY);
                tmp18 &= MASK18;
                sim_debug (DBG_TRACE, & cpu_dev, 
                           "n %o rX %06o HI %06o tmp %06o\n",
                           n, rX [n], (word18) (GETHI(CY) & MASK18), tmp18);

                if (tmp18 == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (tmp18 & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
            
        /// Boolean Comparative Not
        case 0215:  ///< cnaa
            /// C(Z)i = C(A)i & ~C(Y)i for i = (0, 1, ..., 35)
            {
                word36 trZ = rA & ~CY;
                trZ &= DMASK;

                if (trZ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (rA & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0217:  ///< cnaaq
            /// C(Z)i = C (AQ)i & ~C(Y-pair)i for i = (0, 1, ..., 71)
            {
                word72 tmp72 = YPAIRTO72(Ypair);   //
        
                word72 trAQ = convertToWord72(rA, rQ);
                trAQ = trAQ & ~tmp72;
                trAQ &= MASK72;

                if (trAQ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trAQ & SIGN72)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;

        case 0216:  ///< cnaq
            /// C(Z)i = C(Q)i & ~C(Y)i for i = (0, 1, ..., 35)
            {
                word36 trZ = rQ & ~CY;
                trZ &= DMASK;
                if (trZ == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (trZ & SIGN36)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
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
            {
                uint32 n = opcode & 07;  // get n
                word18 tmp18 = rX[n] & ~GETHI(CY);
                tmp18 &= MASK18;
            
                if (tmp18 == 0)
                    SETF(cu.IR, I_ZERO);
                else
                    CLRF(cu.IR, I_ZERO);
            
                if (tmp18 & SIGN18)
                    SETF(cu.IR, I_NEG);
                else
                    CLRF(cu.IR, I_NEG);
            }
            break;
            
        /// FLOATING-POINT ARITHMETIC INSTRUCTIONS
            
        case 0433:  ///< dfld
            /// C(Y-pair)0,7 -> C(E)
            /// C(Y-pair)8,71 -> C(AQ)0,63
            /// 00...0 -> C(AQ)64,71
            /// Zero: If C(AQ) = 0, then ON; otherwise OFF
            /// Neg: If C(AQ)0 = 1, then ON; otherwise OFF
            
            rE = (Ypair[0] >> 28) & MASK8;
            
            rA = (Ypair[0] & FLOAT36MASK) << 8;
            rA |= (Ypair[1] >> 28) & MASK8;
            
            rQ = (Ypair[1] & FLOAT36MASK) << 8;
            
            SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
            SCF(rA & SIGN36, cu.IR, I_NEG);
            break;
            
        case 0431:  ///< fld
            /// C(Y)0,7 -> C(E)
            /// C(Y)8,35 -> C(AQ)0,27
            /// 00...0 -> C(AQ)30,71
            /// Zero: If C(AQ) = 0, then ON; otherwise OFF
            /// Neg: If C(AQ)0 = 1, then ON; otherwise OFF
            
            CY &= DMASK;
            rE = (CY >> 28) & 0377;
            rA = (CY & FLOAT36MASK) << 8;
            rQ = 0;
            
            SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
            SCF(rA & SIGN36, cu.IR, I_NEG);
            break;

        case 0457:  ///< dfst
            /// C(E) -> C(Y-pair)0,7
            /// C(AQ)0,63 -> C(Y-pair)8,71
            
            Ypair[0] = ((word36)rE << 28) | ((rA & 0777777777400LLU) >> 8);
            Ypair[1] = ((rA & 0377) << 28) | ((rQ & 0777777777400LLU) >> 8);
            
            break;
            
        case 0472:  ///< dfstr
            
            dfstr (Ypair);
            break;
            
        case 0455:  ///< fst
            /// C(E) -> C(Y)0,7
            /// C(A)0,27 -> C(Y)8,35
            rE &= MASK18;
            rA &= DMASK;
            CY = ((word36)rE << 28) | (((rA >> 8) & 01777777777LL));
            break;
            
        case 0470:  ///< fstr
            /// The fstr instruction performs a true round and normalization on C(EAQ) as it is stored.
            
//            frd();
//            
//            /// C(E) -> C(Y)0,7
//            /// C(A)0,27 -> C(Y)8,35
//            CY = ((word36)rE << 28) | (((rA >> 8) & 01777777777LL));
//
//            /// Zero: If C(Y) = floating point 0, then ON; otherwise OFF
//            //SCF((CY & 01777777777LL) == 0, cu.IR, I_ZERO);
//            bool isZero = rE == -128 && rA == 0;
//            SCF(isZero, cu.IR, I_ZERO);
//            
//            /// Neg: If C(Y)8 = 1, then ON; otherwise OFF
//            //SCF(CY & 01000000000LL, cu.IR, I_NEG);
//            SCF(rA & SIGN36, cu.IR, I_NEG);
//            
//            /// Exp Ovr: If exponent is greater than +127, then ON
//            /// Exp Undr: If exponent is less than -128, then ON
//            /// XXX: not certain how these can occur here ....
            
            fstr(&CY);
            
            break;
            
        case 0477:  ///< dfad
            /// The dfad instruction may be thought of as a dufa instruction followed by a fno instruction.

            dufa ();
            fno ();
            break;
            
        case 0437:  ///< dufa
            dufa ();
            break;
            
        case 0475:  ///< fad
            /// The fad instruction may be thought of a an ufa instruction followed by a fno instruction.
            /// (Heh, heh. We'll see....)
            
            ufa();
            fno ();
            
            break;
  
        case 0435:  ///< ufa
            /// C(EAQ) + C(Y) -> C(EAQ)
            
            ufa();
            break;
            
        case 0577:  ///< dfsb
            // The dfsb instruction is identical to the dfad instruction with
            // the exception that the twos complement of the mantissa of the
            // operand from main memory is used.
            
            dufs ();
            fno ();
            break;

        case 0537:  ///< dufs
            dufs ();
            break;
            
        case 0575:  ///< fsb
            ///< The fsb instruction may be thought of as an ufs instruction followed by a fno instruction.
            ufs ();
            fno ();
            
            break;
            
        case 0535:  ///< ufs
            ///< C(EAQ) - C(Y) -> C(EAQ)
            
            ufs ();
            break;
            
        case 0463:  ///< dfmp
            /// The dfmp instruction may be thought of as a dufm instruction followed by a
            /// fno instruction.

            dufm ();
            fno ();
            
            break;
            
        case 0423:  ///< dufm

            dufm ();
            break;
            
        case 0461:  ///< fmp
            /// The fmp instruction may be thought of as a ufm instruction followed by a
            /// fno instruction.
 
            ufm ();
            fno ();
            
            break;
            
        case 0421:  ///< ufm
            /// C(EAQ) × C(Y) -> C(EAQ)
            ufm ();
            break;
            
        case 0527:  ///< dfdi

            dfdi ();
            break;
            
        case 0567:  ///< dfdv

            dfdv ();
            break;
            
        case 0525:  ///< fdi
            /// C(Y) / C(EAQ) -> C(EA)
            
            fdi ();
            break;
            
        case 0565:  ///< fdv
            /// C(EAQ) /C(Y) -> C(EA)
            /// 00...0 -> C(Q)
            fdv ();
            break;
            
        case 0513:  ///< fneg
            /// -C(EAQ) normalized -> C(EAQ)
            fneg ();
            break;
            
        case 0573:  ///< fno
            /// The fno instruction normalizes the number in C(EAQ) if C(AQ) ≠ 0 and the overflow indicator is OFF.
            // A normalized floating number is defined as one whose mantissa lies in the interval [0.5,1.0) such that 0.5<= |C(AQ)| <1.0 which, in turn, requires that C(AQ)0 ≠ C(AQ)1.list
            
            // !!!! For personal reasons the following 3 lines of comment must never be removed from this program or any code derived therefrom. HWR 25 Aug 2014
            ///Charles Is the coolest
            ///true story y'all
            //you should get me darksisers 2 for christmas
            
            fno ();
            break;
            
        case 0473:  ///< dfrd
            /// C(EAQ) rounded to 64 bits -> C(EAQ)
            /// 0 -> C(AQ)64,71 (See notes in dps8_math.c on dfrd())

            dfrd ();
            break;
            
        case 0471:  ///< frd
            /// C(EAQ) rounded to 28 bits -> C(EAQ)
            /// 0 -> C(AQ)28,71 (See notes in dps8_math.c on frd())
            
            frd ();
            break;
            
        case 0427:  ///< dfcmg
            /// C(E) :: C(Y-pair)0,7
            /// | C(AQ)0,63 | :: | C(Y-pair)8,71 |

            dfcmg ();
            break;
            
        case 0517:  ///< dfcmp
            /// C(E) :: C(Y-pair)0,7
            /// C(AQ)0,63 :: C(Y-pair)8,71

            dfcmp ();
            break;

        case 0425:  ///< fcmg
            /// C(E) :: C(Y)0,7
            /// | C(AQ)0,27 | :: | C(Y)8,35 |
            
            fcmg ();
            break;
            
        case 0515:  ///< fcmp
            /// C(E) :: C(Y)0,7
            /// C(AQ)0,27 :: C(Y)8,35
            
            fcmp();
            break;
            
        case 0415:  ///< ade
            /// C(E) + C(Y)0,7 -> C(E)
            {
                int8 e = (CY >> 28) & 0377;
                SCF((rE + e) >  127, cu.IR, I_EOFL);
                SCF((rE + e) < -128, cu.IR, I_EUFL);
                
                CLRF(cu.IR, I_ZERO);
                CLRF(cu.IR, I_NEG);
                
                rE += e;    // add
                rE &= 0377; // keep to 8-bits
            }
            break;
            
        case 0430:  ///< fszn

            /// Zero: If C(Y)8,35 = 0, then ON; otherwise OFF
            /// Negative: If C(Y)8 = 1, then ON; otherwise OFF
            
            SCF((CY & 001777777777LL) == 0, cu.IR, I_ZERO);
            SCF(CY & 001000000000LL, cu.IR, I_NEG);
            
            break;
            
        case 0411:  ///< lde
            /// C(Y)0,7 -> C(E)
            
            rE = (CY >> 28) & 0377;
            CLRF(cu.IR, I_ZERO | I_NEG);
    
            break;
            
        case 0456:  ///< ste
            /// C(E) -> C(Y)0,7
            /// 00...0 -> C(Y)8,17
            
            //CY = (rE << 28);
            //CY = bitfieldInsert36(CY, ((word36)(rE & 0377) << 10), 18, 8);
            putbits36 (& CY, 0, 18, ((word36)(rE & 0377) << 10));
            break;
            
            
        /// TRANSFER INSTRUCTIONS
        case 0713:  ///< call6
            
            if (TPR.TRR > PPR.PRR)
            {
                sim_debug (DBG_APPENDING, & cpu_dev,
                           "call6 access violation fault (outward call)");
                doFault (FAULT_ACV, OCALL,
                         "call6 access violation fault (outward call)");
            }
            if (TPR.TRR < PPR.PRR)
                PR[7].SNR = ((DSBR.STACK << 3) | TPR.TRR) & MASK15; // keep to 15-bits
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
            sim_debug (DBG_TRACE, & cpu_dev,
                       "call6 PPR.PRR %o\n", PPR . PRR);
            return CONT_TRA;
            
            
        case 0630:  ///< ret
          {           
            // Parity mask: If C(Y)27 = 1, and the processor is in absolute or
            // mask privileged mode, then ON; otherwise OFF. This indicator is
            // not affected in the normal or BAR modes.
            // Not BAR mode: Can be set OFF but not ON by the ret instruction
            // Absolute mode: Can be set OFF but not ON by the ret instruction
            // All oter indicators: If corresponding bit in C(Y) is 1, then ON;
            // otherwise, OFF
            
            /// C(Y)0,17 -> C(PPR.IC)
            /// C(Y)18,31 -> C(IR)

            word18 tempIR = GETLO(CY) & 0777770;
            // XXX Assuming 'mask privileged mode' is 'temporary absolute mode'
            if (get_addr_mode () != ABSOLUTE_mode) // abs. or temp. abs.
              {
                // if not abs, copy existing parity mask to tempIR
                if (cu.IR & I_PMASK)
                  SETF (tempIR, I_EOFL);
                else
                  CLRF (tempIR, I_EOFL);
              }
            // can be set OFF but not on
            //  IR   ret   result
            //  off  off   off
            //  off  on    off
            //  on   on    on
            //  on   off   off
            // "If it was on, set it to on"
            if (cu.IR & I_NBAR)
              SETF (tempIR, I_NBAR);
            if (cu.IR & I_ABS)
              SETF (tempIR, I_ABS);

            //sim_debug (DBG_TRACE, & cpu_dev,
            //           "RET NBAR was %d now %d\n", 
            //           TSTF (cu . IR, I_NBAR) ? 1 : 0, 
            //           TSTF (tempIR, I_NBAR) ? 1 : 0);
            //sim_debug (DBG_TRACE, & cpu_dev,
            //           "RET ABS  was %d now %d\n", 
            //           TSTF (cu . IR, I_ABS) ? 1 : 0, 
            //           TSTF (tempIR, I_ABS) ? 1 : 0);
            PPR.IC = GETHI(CY);
            cu.IR = tempIR;
            
            return CONT_TRA;
          }
            
        case 0610:  ///< rtcd
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
            
            /// C(Y-pair)3,17 -> C(PPR.PSR)
            /// Maximum of C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) -> C(PPR.PRR)
            /// C(Y-pair)36,53 -> C(PPR.IC)
            /// If C(PPR.PRR) = 0 then C(SDW.P) -> C(PPR.P);
            /// otherwise 0 -> C(PPR.P)
            /// C(PPR.PRR) -> C(PRn.RNR) for n = (0, 1, ..., 7)
            
            //processorCycle = RTCD_OPERAND_FETCH;

            sim_debug (DBG_TRACE, & cpu_dev,
                       "RTCD even %012llo odd %012llo\n", Ypair [0], Ypair [1]);

            /// C(Y-pair)3,17 -> C(PPR.PSR)
            PPR.PSR = GETHI(Ypair[0]) & 077777LL;
            
            // XXX ticket #16
            /// Maximum of C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) -> C(PPR.PRR)
            //PPR.PRR = max3(((GETLO(Ypair[0]) >> 15) & 7), TPR.TRR, SDW->R1);
            PPR.PRR = max3(((GETLO(Ypair[0]) >> 15) & 7), TPR.TRR, RSDWH_R1);
            
            /// C(Y-pair)36,53 -> C(PPR.IC)
            PPR.IC = GETHI(Ypair[1]);
            
            sim_debug (DBG_TRACE, & cpu_dev,
                       "RTCD %05o:%06o\n", PPR . PSR, PPR . IC);

            /// If C(PPR.PRR) = 0 then C(SDW.P) -> C(PPR.P);
            /// otherwise 0 -> C(PPR.P)
            if (PPR.PRR == 0)
                PPR.P = SDW->P;
            else
                PPR.P = 0;
            
            sim_debug (DBG_TRACE, & cpu_dev,
                       "RTCD PPR.PRR %o PPR.P %o\n", PPR . PRR, PPR . P);

            /// C(PPR.PRR) -> C(PRn.RNR) for n = (0, 1, ..., 7)
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
            
//if (rRALR && PPR.PRR >= rRALR) sim_printf ("RTCD expects a ring alarm\n");
            return CONT_TRA;
            
            
        case 0614:  ///< teo
            /// If exponent overflow indicator ON then
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            if (cu.IR & I_EOFL)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                CLRF(cu.IR, I_EOFL);
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;
            
        case 0615:  ///< teu
            /// If exponent underflow indicator ON then
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            if (cu.IR & I_EUFL)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                CLRF(cu.IR, I_EUFL);
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;

        
        case 0604:  ///< tmi
            /// If negative indicator ON then
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            if (cu.IR & I_NEG)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;
            
        case 0602:  ///< tnc
            /// If carry indicator OFF then
            ///   C(TPR.CA) -> C(PPR.IC)
            ///   C(TPR.TSR) -> C(PPR.PSR)
            if (!(cu.IR & I_CARRY))
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;
            
        case 0601:  ///< tnz
            /// If zero indicator OFF then
            ///     C(TPR.CA) -> C(PPR.IC)
            ///     C(TPR.TSR) -> C(PPR.PSR)
            if (!(cu.IR & I_ZERO))
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;

        case 0617:  ///< tov
            /// If overflow indicator ON then
            ///   C(TPR.CA) -> C(PPR.IC)
            ///   C(TPR.TSR) -> C(PPR.PSR)
            if (cu.IR & I_OFLOW)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                CLRF(cu.IR, I_OFLOW);
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;

        case 0605:  ///< tpl
            /// If negative indicator OFF, then
            ///   C(TPR.CA) -> C(PPR.IC)
            ///   C(TPR.TSR) -> C(PPR.PSR)
            if (!(cu.IR & I_NEG)) 
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;
        
        case 0710:  ///< tra
            /// C(TPR.CA) -> C(PPR.IC)
            /// C(TPR.TSR) -> C(PPR.PSR)
            
            PPR.IC = TPR.CA;
            PPR.PSR = TPR.TSR;
            
            return CONT_TRA;

        case 0603:  ///< trc
            ///  If carry indicator ON then
            ///    C(TPR.CA) -> C(PPR.IC)
            ///    C(TPR.TSR) -> C(PPR.PSR)
            if (cu.IR & I_CARRY)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
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
            ///  C(PPR.PRR) -> C(PRn.RNR)
            ///  C(PPR.PSR) -> C(PRn.SNR)
            ///  C(PPR.IC) + 1 -> C(PRn.WORDNO)
            ///  00...0 -> C(PRn.BITNO)
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            {
                uint32 n;
                if (opcode <= 0273)
                    n = (opcode & 3);
                else
                    n = (opcode & 3) + 4;

            // XXX According to figure 8.1, all of this is done by the append unit.
                PR[n].RNR = PPR.PRR;
                PR[n].SNR = PPR.PSR;
                PR[n].WORDNO = (PPR.IC + 1) & MASK18;
                PR[n].BITNO = 0;
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
            }
            return CONT_TRA;

        case 0715:  ///< tss
            if (TPR.CA >= ((word18) BAR.BOUND) << 9)
            {
                doFault (FAULT_ACV, ACV15, "TSS boundary violation");
                //break;
            }
            /// C(TPR.CA) + (BAR base) -> C(PPR.IC)
            /// C(TPR.TSR) -> C(PPR.PSR)
            PPR.IC = TPR.CA /* + (BAR.BASE << 9) */; // getBARaddress does the adding
            PPR.PSR = TPR.TSR;

#if 0
            set_addr_mode (BAR_mode);
#else
            CLRF(cu.IR, I_NBAR);
#endif
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
            ///   C(PPR.IC) + 1 -> C(Xn)
            /// C(TPR.CA) -> C(PPR.IC)
            /// C(TPR.TSR) -> C(PPR.PSR)
            {
                uint32 n = opcode & 07;  // get n
                rX[n] = (PPR.IC + 1) & MASK18;
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
            }
            return CONT_TRA;
        
        case 0607:  ///< ttf
            /// If tally runout indicator OFF then
            ///   C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            if ((cu.IR & I_TALLY) == 0)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;
         
        case 0600:  ///< tze
            /// If zero indicator ON then
            ///   C(TPR.CA) -> C(PPR.IC)
            ///   C(TPR.TSR) -> C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            if (cu.IR & I_ZERO)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif

                return CONT_TRA;
            }
            break;

        case 0311:  ///< easp0
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[0].SNR = TPR.CA & MASK15;
            break;
        case 0313:  ///< easp2
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[2].SNR = TPR.CA & MASK15;
            break;
        case 0331:  ///< easp4
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[4].SNR = TPR.CA & MASK15;
            break;
        case 0333:  ///< easp6
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[6].SNR = TPR.CA & MASK15;
            break;
        
        case 0310:  ///< eawp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[0].WORDNO = TPR.CA;
            PR[0].BITNO = TPR.TBR;
            break;
        case 0312:  ///< eawp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[2].WORDNO = TPR.CA;
            PR[2].BITNO = TPR.TBR;
            break;
        case 0330:  ///< eawp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[4].WORDNO = TPR.CA;
            PR[4].BITNO = TPR.TBR;
            break;
        case 0332:  ///< eawp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[6].WORDNO = TPR.CA;
            PR[6].BITNO = TPR.TBR;
            break;
        
            
        case 0351:  ///< epbp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[1].RNR = TPR.TRR;
            PR[1].SNR = TPR.TSR;
            PR[1].WORDNO = 0;
            PR[1].BITNO = 0;
            break;
        case 0353:  ///< epbp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[3].RNR = TPR.TRR;
            PR[3].SNR = TPR.TSR;
            PR[3].WORDNO = 0;
            PR[3].BITNO = 0;
            break;
        case 0371:  ///< epbp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[5].RNR = TPR.TRR;
            PR[5].SNR = TPR.TSR;
            PR[5].WORDNO = 0;
            PR[5].BITNO = 0;
            break;
            
        case 0373:  ///< epbp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[7].RNR = TPR.TRR;
            PR[7].SNR = TPR.TSR;
            PR[7].WORDNO = 0;
            PR[7].BITNO = 0;
            break;
        
        case 0350:  ///< epp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[0].RNR = TPR.TRR;
            PR[0].SNR = TPR.TSR;
            PR[0].WORDNO = TPR.CA;
            PR[0].BITNO = TPR.TBR;
            break;
        case 0352:  ///< epp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[2].RNR = TPR.TRR;
            PR[2].SNR = TPR.TSR;
            PR[2].WORDNO = TPR.CA;
            PR[2].BITNO = TPR.TBR;
            break;
        case 0370:  ///< epp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[4].RNR = TPR.TRR;
            PR[4].SNR = TPR.TSR;
            PR[4].WORDNO = TPR.CA;
            PR[4].BITNO = TPR.TBR;
            break;
        case 0372:  ///< epp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[6].RNR = TPR.TRR;
            PR[6].SNR = TPR.TSR;
            PR[6].WORDNO = TPR.CA;
            PR[6].BITNO = TPR.TBR;
            break;

        case 0173:  ///< lpri
            /// For n = 0, 1, ..., 7
            ///  Y-pair = Y-block16 + 2n
            ///  Maximum of C(Y-pair)18,20; C(SDW.R1); C(TPR.TRR) -> C(PRn.RNR)
            ///  C(Y-pair) 3,17 -> C(PRn.SNR)
            ///  C(Y-pair)36,53 -> C(PRn.WORDNO)
            ///  C(Y-pair)57,62 -> C(PRn.BITNO)
            
            for(uint32 n = 0 ; n < 8 ; n ++)
            {
                //word36 Ypair[2];
                Ypair[0] = Yblock16[n * 2 + 0]; // Even word of ITS pointer pair
                Ypair[1] = Yblock16[n * 2 + 1]; // Odd word of ITS pointer pair
                
                word3 Crr = (GETLO(Ypair[0]) >> 15) & 07;       ///< RNR from ITS pair
                //if (get_addr_mode () == APPEND_mode || get_addr_mode () == APPEND_BAR_mode)
                if (get_addr_mode () == APPEND_mode)
                  PR[n].RNR = max3(Crr, SDW->R1, TPR.TRR) ;
                else
                  PR[n].RNR = Crr;
                PR[n].SNR = (Ypair[0] >> 18) & MASK15;
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
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  If C(Y)0,1 ≠ 11, then
            ///    C(Y)0,5 -> C(PRn.BITNO);
            ///  otherwise,
            ///    generate command fault
            /// If C(Y)6,17 = 11...1, then 111 -> C(PRn.SNR)0,2
            ///  otherwise,
            /// 000 -> C(PRn.SNR)0,2
            /// C(Y)6,17 -> C(PRn.SNR)3,14
            /// C(Y)18,35 -> C(PRn.WORDNO)
            {
                uint32 n = opcode & 07;  // get n
                PR[n].RNR = TPR.TRR;

// [CAC] sprpn says: If C(PRn.SNR) 0,2 are nonzero, and C(PRn.SNR) ≠ 11...1, 
// then a store fault (illegal pointer) will occur and C(Y) will not be changed.
// I interpret this has meaning that only the high bits should be set here

                if (((CY >> 34) & 3) != 3)
                    PR[n].BITNO = (CY >> 30) & 077;
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

                    // Therefore the subfault well no illegal action, and Multics will peek it the
                    // instruction to deduce that it is a lprpn fault.
                    doFault(FAULT_CMD, lprpn_bits, "Load Pointer Register Packed (lprpn)");
                  }
#if 0
                //If C(Y)6,17 = 11...1, then 111 -> C(PRn.SNR)0,2
                if ((CY & 07777000000LLU) == 07777000000LLU)
                    PR[n].SNR |= 070000; // XXX check to see if this is correct
                else // otherwise, 000 -> C(PRn.SNR)0,2
                    PR[n].SNR &= 007777;
                // XXX completed, but needs testing
                //C(Y)6,17 -> C(PRn.SNR)3,14
                //PR[n].SNR &= 3; -- huh? Never code when tired
                PR[n].SNR &=             070000; // [CAC] added this
                PR[n].SNR |= GETHI(CY) & 007777;
#else
// The SPRPn instruction stores only the low 12 bits of the 15 bit SNR.
// A special case is made for an SNR of all ones; it is stored as 12 1's.
// The pcode in AL39 handles this awkwardly; I believe this is
// the same, but in a more straightforward manner

               // Get the 12 bit operand SNR
               word12 oSNR = getbits36 (CY, 6, 12);
               // Test for special case
               if (oSNR == 07777)
                 PR[n] . SNR = 077777;
               else
                 PR[n] . SNR = oSNR; // usigned word will 0-extend.
#endif
                //C(Y)18,35 -> C(PRn.WORDNO)
                PR[n].WORDNO = GETLO(CY);

                sim_debug (DBG_APPENDING, & cpu_dev, "lprp%d CY 0%012llo, PR[n].RNR 0%o, PR[n].BITNO 0%o, PR[n].SNR 0%o, PR[n].WORDNO %o\n", n, CY, PR[n].RNR, PR[n].BITNO, PR[n].SNR, PR[n].WORDNO);
            }
            break;
         
        case 0251:  ///< spbp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[1].SNR) << 18;
            Ypair[0] |= ((word36) PR[1].RNR) << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
            
        case 0253:  ///< spbp3
            //l For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[3].SNR) << 18;
            Ypair[0] |= ((word36) PR[3].RNR) << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
            
        case 0651:  ///< spbp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[5].SNR) << 18;
            Ypair[0] |= ((word36) PR[5].RNR) << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
        
        case 0653:  ///< spbp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[7].SNR) << 18;
            Ypair[0] |= ((word36) PR[7].RNR) << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
        
        case 0254:  ///< spri
            /// For n = 0, 1, ..., 7
            ///  Y-pair = Y-block16 + 2n
            
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
        
            for(uint32 n = 0 ; n < 8 ; n++)
            {
                Yblock16[2 * n] = 043;
                Yblock16[2 * n] |= ((word36) PR[n].SNR) << 18;
                Yblock16[2 * n] |= ((word36) PR[n].RNR) << 15;
                
                Yblock16[2 * n + 1] = (word36) PR[n].WORDNO << 18;
                Yblock16[2 * n + 1] |= (word36) PR[n].BITNO << 9;
            }
            
            //WriteN(i, 16, TPR.CA, Yblock16, OperandWrite, rTAG);
            break;
            
        case 0250:  ///< spri0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[0].SNR) << 18;
            Ypair[0] |= ((word36) PR[0].RNR) << 15;
            
            Ypair[1] = (word36) PR[0].WORDNO << 18;
            Ypair[1]|= (word36) PR[0].BITNO << 9;
            
            break;
            
        case 0252:  ///< spri2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[2].SNR) << 18;
            Ypair[0] |= ((word36) PR[2].RNR) << 15;
            
            Ypair[1] = (word36) PR[2].WORDNO << 18;
            Ypair[1]|= (word36) PR[2].BITNO << 9;
            
            break;
  
        case 0650:  ///< spri4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PAR[4].SNR) << 18;
            Ypair[0] |= ((word36) PAR[4].RNR) << 15;
            
            Ypair[1] = (word36) PAR[4].WORDNO << 18;
            Ypair[1]|= (word36) PAR[4].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
    
        case 0652:  ///< spri6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[6].SNR) << 18;
            Ypair[0] |= ((word36) PR[6].RNR) << 15;
            
            Ypair[1] = (word36) PR[6].WORDNO << 18;
            Ypair[1]|= (word36) PR[6].BITNO << 9;
            
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
            ///  C(PRn.BITNO) -> C(Y)0,5
            ///  C(PRn.SNR)3,14 -> C(Y)6,17
            //  C(PRn.WORDNO) -> C(Y)18,35
            {
                uint32 n = opcode & 07;  // get n
            
                //If C(PRn.SNR)0,2 are nonzero, and C(PRn.SNR) ≠ 11...1, then a store fault (illegal pointer) will occur and C(Y) will not be changed.
            
                // sim_printf ("sprp%d SNR %05o\n", n, PR[n].SNR);
                if ((PR[n].SNR & 070000) != 0 && PR[n].SNR != MASK15)
                  doFault(FAULT_STR, ill_ptr, "Store Pointer Register Packed (sprpn)");
            
                if (switches . lprp_highonly)
                  {
                    CY  =  ((word36) (PR[n].BITNO & 077)) << 30;
                    CY |=  ((word36) (PR[n].SNR & 07777)) << 18; // lower 12- of 15-bits
                    CY |=  PR[n].WORDNO & PAMASK;
                  }
                else
                  {
                    CY  =  ((word36) PR[n].BITNO) << 30;
                    CY |=  ((word36) (PR[n].SNR) & 07777) << 18; // lower 12- of 15-bits
                    CY |=  PR[n].WORDNO;
                  }
            
                CY &= DMASK;    // keep to 36-bits
            }
            break;
            
        case 0050:   ///< adwp0
        case 0051:   ///< adwp1
        case 0052:   ///< adwp2
        case 0053:   ///< adwp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(Y)0,17 + C(PRn.WORDNO) -> C(PRn.WORDNO)
            ///   00...0 -> C(PRn.BITNO)
            {
                uint32 n = opcode & 03;  // get n
                PR[n].WORDNO += GETHI(CY);
                PR[n].WORDNO &= MASK18;
                PR[n].BITNO = 0;
            }
            break;
            
        case 0150:   ///< adwp4
        case 0151:   ///< adwp5
        case 0152:   ///< adwp6
        case 0153:   ///< adwp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(Y)0,17 + C(PRn.WORDNO) -> C(PRn.WORDNO)
            ///   00...0 -> C(PRn.BITNO)
            {
                uint32 n = (opcode & MASK3) + 4U;  // get n
                PR[n].WORDNO += GETHI(CY);
                PR[n].WORDNO &= MASK18;
                PR[n].BITNO = 0;
            }
            break;
        
        case 0213:  ///< epaq
            /// 000 -> C(AQ)0,2
            /// C(TPR.TSR) -> C(AQ)3,17
            /// 00...0 -> C(AQ)18,32
            /// C(TPR.TRR) -> C(AQ)33,35
            
            /// C(TPR.CA) -> C(AQ)36,53
            /// 00...0 -> C(AQ)54,65
            /// C(TPR.TBR) -> C(AQ)66,71
            
            rA = TPR.TRR & MASK3;
            rA |= (word36) (TPR.TSR & MASK15) << 18;
            
            rQ = TPR.TBR & MASK6;
            rQ |= (word36) (TPR.CA & MASK18) << 18;
            
            break;
        
        case 0633:  ///< rccl
            /// 00...0 -> C(AQ)0,19
            /// C(calendar clock) -> C(AQ)20,71
            {
// XXX see ticket #23
              // For the rccl instruction, the first 2 or 3 bits of the addr
              // field of the instruction are used to specify which SCU.
              // 2 bits for the DPS8M.
              //int cpu_port_num = getbits36 (TPR.CA, 0, 2);
              uint cpu_port_num = (TPR.CA >> 15) & 03;
              int scu_unit_num = query_scu_unit_num (ASSUME_CPU0, cpu_port_num);
              if (scu_unit_num < 0)
                {
                  sim_warn ("RCCL can't find the right SCU; using #0\n");
                  scu_unit_num = 0;
                }
              t_stat rc = scu_rscr (scu_unit_num, ASSUME_CPU0, 040, & rA, & rQ);
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
                  __uint128_t big = ((__uint128_t) rA) << 36 | rQ;
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

            if (switches . drl_fatal)
              {
                return STOP_HALT;
              }
            doFault (FAULT_DRL, 0, "drl");
            // break;
         
        case 0716:  ///< xec
            {
                cu . IWB = CY;
                cu . xde = 1;
                cu . xdo = 0;
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
            
                cu . IWB = Ypair [0];
                cu . IRODD = Ypair [1];
                cu . xde = 1;
                cu . xdo = 1;
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
	  //instruction pair at main memory location C+(52)8. The value of C
	  //is obtained from the FAULT VECTOR switches on the processor
	  //configuration panel.
            doFault(FAULT_MME2, 0, "Master Mode Entry 2 (mme2)");
            // break;

        case 0005:   ///< mme3
            /// Causes a fault that fetches and executes, in absolute mode, the instruction pair at main memory location C+(54)8. The value of C is obtained from the FAULT VECTOR switches on the processor configuration panel.
            doFault(FAULT_MME3, 0, "Master Mode Entry 3 (mme3)");
            // break;

        case 0007:   ///< mme4
            /// Causes a fault that fetches and executes, in absolute mode, the instruction pair at main memory location C+(56)8. The value of C is obtained from the FAULT VECTOR switches on the processor configuration panel.
            doFault(FAULT_MME4, 0, "Master Mode Entry 4 (mme4)");
            // break;

        case 0011:   // nop
            break;

        case 0012:   // puls1
            break;

        case 0013:   // puls2
            break;
         
        case 0560:  // rpd
            {
              cu . delta = i->tag;
              // a:AL39/rpd1
              word1 c = (i->address >> 7) & 1;
              if (c)
                rX[0] = i->address;    // Entire 18 bits
              cu . rd = 1;
              cu . repeat_first = 1;
//sim_printf ("[%lld] rpd delta %02o c %d X0:%06o %012llo\n",sim_timell (),  cu.delta, c, rX[0], cu . IWB);
            }
            break;

        case 0500:  ///< rpl
            return STOP_UNIMP;

        case 0520:  ///< rpt
            // AL39, page 209
            {
              uint c = (i->address >> 7) & 1;
              cu . delta = i->tag;
              if (c)
                rX[0] = i->address;    // Entire 18 bits
              cu . rpt = 1;
              cu . repeat_first = 1;
//sim_printf ("repeatd first; delta %02o c %d X0:%06o\n", cu.delta, c, rX[0]);
            }
            break;
            
        case 0550:  ///< sbar
            /// C(BAR) -> C(Y) 0,17
            SETHI(CY, (BAR.BASE << 9) | BAR.BOUND);
            
            break;
            
            
        /// translation
        case 0505:  ///< bcd
            /// Shift C(A) left three positions
            /// | C(A) | / C(Y) -> 4-bit quotient plus remainder
            /// Shift C(Q) left six positions
            /// 4-bit quotient -> C(Q)32,35
            /// remainder -> C(A)
            
            {
                word36 tmp1 = rA & SIGN36; // A0
            
                rA <<= 3;       // Shift C(A) left three positions
                rA &= DMASK;    // keep to 36-bits
            
                //word36 tmp36 = llabs(SIGNEXT36(rA));
                //word36 tmp36 = SIGNEXT36(rA) & MASK36;;
                word36 tmp36 = rA & MASK36;;
                word36 tmp36q = tmp36 / CY; // | C(A) | / C(Y) -> 4-bit quotient plus remainder
                word36 tmp36r = tmp36 % CY;
            
                rQ <<= 6;       // Shift C(Q) left six positions
                rQ &= DMASK;
            
                rQ &= ~017;     // 4-bit quotient -> C(Q)32,35
                rQ |= (tmp36q & 017);
            
                rA = tmp36r;    // remainder -> C(A)
            
                SCF(rA == 0, cu.IR, I_ZERO);  // If C(A) = 0, then ON; otherwise OFF
                SCF(tmp1,    cu.IR, I_NEG);   // If C(A)0 = 1 before execution, then ON; otherwise OFF
            }
            break;
           
        case 0774:  ///< gtb
            /// C(A)0 -> C(A)0
            /// C(A)i ⊕ C(A)i-1 -> C(A)i for i = 1, 2, ..., 35
            {
                /// TODO: untested.
            
                word36 tmp1 = rA & SIGN36; // save A0
            
                word36 tmp36 = rA >> 1;
                rA = rA ^ tmp36;
            
                if (tmp1)
                    rA |= SIGN36;   // set A0
                else
                    rA &= ~SIGN36;  // reset A0
            
                SCF(rA == 0,    cu.IR, I_ZERO);  // If C(A) = 0, then ON; otherwise OFF
                SCF(rA & SIGN36,cu.IR, I_NEG);   // If C(A)0 = 1, then ON; otherwise OFF
            }
            break;
         
        /// REGISTER LOAD
        case 0230:  ///< lbar
            /// C(Y)0,17 -> C(BAR)
            BAR.BASE = (GETHI(CY) >> 9) & 0777; /// BAR.BASE is upper 9-bits (0-8)
            BAR.BOUND = GETHI(CY) & 0777;       /// BAR.BOUND is next lower 9-bits (9-17)
            break;
           
        // Privileged Instructions

        // Privileged - Register Load
        // DPS8M interpratation
        case 0674:  ///< lcpr
            switch (i->tag)
              {
// Extract bits from 'from' under 'mask' shifted to where (where is
// dps8 '0 is the msbit.
#define GETBITS(from,mask,where) \
 (((from) >> (35 - (where))) & (word36) (mask))
                case 02: // cache mode register
                  //CMR = CY;
                  // CMR . cache_dir_address = <ignored for lcpr>
                  // CMR . par_bit = <ignored for lcpr>
                  // CMR . lev_ful = <ignored for lcpr>
                     CMR . csh1_on = GETBITS (CY, 1, 72 - 54);
                     CMR . csh2_on = GETBITS (CY, 1, 72 - 55);
                  // CMR . opnd_on = ; // DPS8, not DPS8M
                     CMR . inst_on = GETBITS (CY, 1, 72 - 57);
                     CMR . csh_reg = GETBITS (CY, 1, 72 - 59);
                  // CMR . str_asd = <ignored for lcpr>
                  // CMR . col_ful = <ignored for lcpr>
                  // CMR . rro_AB = GETBITS (CY, 1, 18);
                     CMR . luf = GETBITS (CY, 3, 72 - 71);
                  // You need bypass_cache_bit to actually manage the cache,
                  // but it is not stored
#ifndef QUIET_UNUSED
                     uint bypass_cache_bit = GETBITS (CY, 1, 72 - 68);
#endif
                  break;

                case 04: // mode register
                  MR . cuolin = GETBITS (CY, 1, 18);
                  MR . solin = GETBITS (CY, 1, 19);
                  MR . sdpap = GETBITS (CY, 1, 20);
                  MR . separ = GETBITS (CY, 1, 21);
                  MR . tm = GETBITS (CY, 3, 23);
                  MR . vm = GETBITS (CY, 3, 26);
                  MR . hrhlt = GETBITS (CY, 1, 28);
                  MR . hrxfr = GETBITS (CY, 1, 29);
                  MR . ihr = GETBITS (CY, 1, 30);
                  MR . ihrrs = GETBITS (CY, 1, 31);
                  MR . mrgctl = GETBITS (CY, 1, 32);
                  MR . hexfp = GETBITS (CY, 1, 33);
                  MR . emr = GETBITS (CY, 1, 35);
                  break;

                case 03: // DPS 8m 0's -> history
                  // XXX punt
                  break;

                case 05: // DPS 8m 1's -> history
                  // XXX punt
                  break;

                default:
                  doFault (FAULT_IPR, ill_mod, "lcpr tag invalid");

              }
            break;

        case 0232:  ///< ldbr
            do_ldbr (Ypair);
            break;

        case 0637:  ///< ldt
            {
#ifdef REAL_TR
              word27 val = (CY >> 9) & MASK27;
              sim_debug (DBG_TRACE, & cpu_dev, "ldt rTR %d (%o)\n", val, val);
              setTR (val);
#else
              rTR = (CY >> 9) & MASK27;
              sim_debug (DBG_TRACE, & cpu_dev, "ldt rTR %d (%o)\n", rTR, rTR);
#endif
              // Undocumented feature. return to bce has been observed to
              // experience TRO while masked, setting the TR to -1, and
              // experiencing an unexpected TRo interrupt when unmasking.
              // Reset any pending TRO fault when the TR is loaded.
              clearTROFault ();
            }
            break;

        case 0257:  ///< lsdp
            // Not clear what the subfault should be; see Fault Register in AL39.
            doFault(FAULT_IPR, ill_proc, "lsdp is illproc on DPS8M");

        case 0613:  ///< rcu
            doRCU (); // never returns

        case 0452:  ///< scpr
          {
            uint tag = (i -> tag) & MASK6;
            switch (tag)
              {
                case 000: // C(APU history register#1) -> C(Y-pair)
                  {
                    // XXX punt
                    Ypair [0] = 0;
                    Ypair [1] = 0;
                  }
                  break;

                case 001: // C(fault register) -> C(Y-pair)0,35
                          // 00...0 -> C(Y-pair)36,71
                  {
                    Ypair [0] = faultRegister [0];
                    Ypair [1] = faultRegister [1];
                    faultRegister [0] = 0;
                    faultRegister [1] = 0;
                  }
                  break;

                case 006: // C(mode register) -> C(Y-pair)0,35
                          // C(cache mode register) -> C(Y-pair)36,72
                  {
                    Ypair [0] = 0;
                    putbits36 (& Ypair [0], 18, 1, MR . cuolin);
                    putbits36 (& Ypair [0], 19, 1, MR . solin);
                    putbits36 (& Ypair [0], 20, 1, MR . sdpap);
                    putbits36 (& Ypair [0], 21, 1, MR . separ);
                    putbits36 (& Ypair [0], 22, 2, MR . tm);
                    putbits36 (& Ypair [0], 24, 2, MR . vm);
                    putbits36 (& Ypair [0], 28, 1, MR . hrhlt);
                    putbits36 (& Ypair [0], 29, 1, MR . hrxfr);
                    putbits36 (& Ypair [0], 30, 1, MR . ihr);
                    putbits36 (& Ypair [0], 31, 1, MR . ihrrs);
                    putbits36 (& Ypair [0], 32, 1, MR . mrgctl);
                    putbits36 (& Ypair [0], 33, 1, MR . hexfp);
                    putbits36 (& Ypair [0], 35, 1, MR . emr);
                    Ypair [1] = 0;
                    putbits36 (& Ypair [1], 36 - 36, 15, CMR . cache_dir_address);
                    putbits36 (& Ypair [1], 51 - 36, 1, CMR . par_bit);
                    putbits36 (& Ypair [1], 52 - 36, 1, CMR . lev_ful);
                    putbits36 (& Ypair [1], 54 - 36, 1, CMR . csh1_on);
                    putbits36 (& Ypair [1], 55 - 36, 1, CMR . csh2_on);
                    putbits36 (& Ypair [1], 57 - 36, 1, CMR . inst_on);
                    putbits36 (& Ypair [1], 59 - 36, 1, CMR . csh_reg);
                    putbits36 (& Ypair [1], 60 - 36, 1, CMR . str_asd);
                    putbits36 (& Ypair [1], 61 - 36, 1, CMR . col_ful);
                    putbits36 (& Ypair [1], 62 - 36, 2, CMR . rro_AB);
                    putbits36 (& Ypair [1], 68 - 36, 1, CMR . bypass_cache);
                    putbits36 (& Ypair [1], 70 - 36, 2, CMR . luf);
                  }
                  break;

                case 010: // C(APU history register#2) -> C(Y-pair)
                  {
                    // XXX punt
                    Ypair [0] = 0;
                    Ypair [1] = 0;
                  }
                  break;

                case 020: // C(CU history register) -> C(Y-pair)
                  {
                    // XXX punt
                    Ypair [0] = 0;
                    Ypair [1] = 0;
                  }
                  break;

                case 040: // C(OU/DU history register) -> C(Y-pair)
                  {
                    // XXX punt
                    Ypair [0] = 0;
                    Ypair [1] = 0;
                  }
                  break;

                default:
                  {
                    doFault(FAULT_IPR, ill_mod, "SCPR Illegal register select value");
                  }
              }
          }
          break;

        case 0657:  ///< scu
            // AL-39 defines the behaivor of SCU during fault/interrupt
            // processing, but not otherwise.
            // The T&D tape uses SCU during normal processing, and apparently
            // expects the current CU state to be saved.

            if (cpu . cycle == EXEC_cycle)
              {
                // T&D behavior
                scu2words (Yblock8);
              }
            else
              {
                // AL-39 behavior
                for (int i = 0; i < 8; i ++)
                  Yblock8 [i] = scu_data [i];
              }
            break;
            
        case 0154:  ///< sdbr
            do_sdbr (Ypair);
            break;

        case 0557:  ///< ssdp
          {
// XXX The associative memory is ignored (forced to "no match") during address preparation.

#ifndef SPEED
            // Level j is selected by C(TPR.CA)12,13
            uint level = (TPR . CA >> 4) & 02u;
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                Yblock16 [i] = 0;
#ifndef SPEED
                putbits36 (& Yblock16 [i], 0, 15, SDWAM [toffset + i] . POINTER);
                putbits36 (& Yblock16 [i], 27, 1, SDWAM [toffset + i] . F);
                putbits36 (& Yblock16 [i], 30, 6, SDWAM [toffset + i] . USE);
#endif
              }
          }
          break;

        case 0532:  ///< cams
            do_cams (TPR.CA);
            break;
            
        // Privileged - Configuration and Status
            
        case 0233:  ///< rmcm
            {
                // C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor) 
                // specify which processor port (i.e., which system 
                // controller) is used.
                uint cpu_port_num = (TPR.CA >> 15) & 03;
                int scu_unit_num = query_scu_unit_num (ASSUME_CPU0, cpu_port_num);
                if (scu_unit_num < 0)
                  {
                    sim_warn ("RMCM can't find the right SCU; using #0\n");
                    scu_unit_num = 0;
                  }
                t_stat rc = scu_rmcm (scu_unit_num, ASSUME_CPU0, & rA, & rQ);
                if (rc)
                    return rc;
                SCF (rA == 0, cu.IR, I_ZERO);
                SCF (rA & SIGN36, cu.IR, I_NEG);
            }
            break;

        case 0413:  ///< rscr
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

//sim_debug (DBG_TRACE, & cpu_dev, "CA %06d\n", TPR . CA);

              //int scu_unit_num = getbits36 (TPR.CA, 0, 2);
              //uint scu_unit_num = (TPR.CA >> 10) & MASK8;
              int cpu_port_num = query_scbank_map (iefpFinalAddress);
              if (cpu_port_num < 0)
                {
                  sim_debug (DBG_ERR, & cpu_dev, "RSCR: Unable to determine port for address %08o; defaulting to port A\n", iefpFinalAddress);
                  cpu_port_num = 0;
                }
              uint scu_unit_num = cables -> cablesFromScuToCpu [ASSUME_CPU0] . ports [cpu_port_num] . scu_unit_num;

              t_stat rc = scu_rscr (scu_unit_num, ASSUME_CPU0, iefpFinalAddress & MASK15, & rA, & rQ);
              if (rc)
                return rc;
            }
            
            break;
            
        case 0231:  ///< rsw
          {
            if (i -> tag == TD_DL)
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
                  switches . serno, // 13-25 CPU Serial number
                  "140730  ");      // 26-33 Ship date (YYMMDD)
                
#else
                static unsigned char PROM [1024] =
                  "DPS8/70M Emul" //  0-12 CPU Model number
                  "1            " // 13-25 CPU Serial number
                  "140730  "      // 26-33 Ship date (YYMMDD)
                  ;
#endif
                rA = PROM [TPR . CA & 1023];
                break;
              }
            uint select = TPR.CA & 0x7;
            switch (select)
              {
                case 0: // data switches
                  rA = switches . data_switches;
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

                  rA  = 0;
                  rA |= (switches . assignment  [0] & 07LL)  << (35 -  (2 +  0));
                  rA |= (switches . enable      [0] & 01LL)  << (35 -  (3 +  0));
                  rA |= (switches . init_enable [0] & 01LL)  << (35 -  (4 +  0));
                  rA |= (switches . interlace   [0] ? 1LL:0LL)  << (35 -  (5 +  0));
                  rA |= (switches . store_size  [0] & 07LL)  << (35 -  (8 +  0));

                  rA |= (switches . assignment  [1] & 07LL)  << (35 -  (2 +  9));
                  rA |= (switches . enable      [1] & 01LL)  << (35 -  (3 +  9));
                  rA |= (switches . init_enable [1] & 01LL)  << (35 -  (4 +  9));
                  rA |= (switches . interlace   [1] ? 1LL:0LL)  << (35 -  (5 +  9));
                  rA |= (switches . store_size  [1] & 07LL)  << (35 -  (8 +  9));

                  rA |= (switches . assignment  [2] & 07LL)  << (35 -  (2 + 18));
                  rA |= (switches . enable      [2] & 01LL)  << (35 -  (3 + 18));
                  rA |= (switches . init_enable [2] & 01LL)  << (35 -  (4 + 18));
                  rA |= (switches . interlace   [2] ? 1LL:0LL)  << (35 -  (5 + 18));
                  rA |= (switches . store_size  [2] & 07LL)  << (35 -  (8 + 18));

                  rA |= (switches . assignment  [3] & 07LL)  << (35 -  (2 + 27));
                  rA |= (switches . enable      [3] & 01LL)  << (35 -  (3 + 27));
                  rA |= (switches . init_enable [3] & 01LL)  << (35 -  (4 + 27));
                  rA |= (switches . interlace   [3] ? 1LL:0LL)  << (35 -  (5 + 27));
                  rA |= (switches . store_size  [3] & 07LL)  << (35 -  (8 + 27));
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

                  rA = 0;
                  rA |= (switches . interlace [0] == 2 ? 1LL : 0LL) << (35 -  0);
                  rA |= (switches . interlace [1] == 2 ? 1LL : 0LL) << (35 -  1);
                  rA |= (switches . interlace [2] == 2 ? 1LL : 0LL) << (35 -  2);
                  rA |= (switches . interlace [3] == 2 ? 1LL : 0LL) << (35 -  3);
                  rA |= (0b01L)  /* DPS8M */                        << (35 -  5);
                  rA |= (switches . FLT_BASE & 0177LL)              << (35 - 12);
                  rA |= (0b1L)                                      << (35 - 13);
                  rA |= (0b0000L)                                   << (35 - 17);
                  rA |= (0b111L)                                    << (35 - 20);
                  rA |= (0b00L)                                     << (35 - 22);
                  rA |= (0b1L)  /* DPS8M */                         << (35 - 23);
                  rA |= (switches . proc_mode & 01LL)               << (35 - 24);
                  rA |= (0b1L)                                      << (35 - 25);
                  rA |= (0b000L)                                    << (35 - 28);
                  rA |= (switches . proc_speed & 017LL)             << (35 - 32);
                  rA |= (switches . cpu_num & 07LL)                 << (35 - 35);
                  break;

                case 3: // configuration switches for ports E-H, which
                        // the DPS didn't have (SCUs had more memory, so
                        // fewer SCUs were needed
                  rA = 0;
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

                  rA  = 0;
                  rA |= (switches . interlace [0] == 2 ? 1LL : 0LL) << (35 - 13);
                  rA |= (switches . interlace [1] == 2 ? 1LL : 0LL) << (35 - 15);
                  rA |= (switches . interlace [2] == 2 ? 1LL : 0LL) << (35 - 17);
                  rA |= (switches . interlace [3] == 2 ? 1LL : 0LL) << (35 - 19);
                  break;

                default:
                  // XXX Guessing values; also don't know if this is actually a fault
                  doFault(FAULT_IPR, ill_mod, "Illegal register select value");
              }
            SCF (rA == 0, cu.IR, I_ZERO);
            SCF (rA & SIGN36, cu.IR, I_NEG);
          }
          break;

        // Privileged -- System Control

        case 0015:  // cioc
          {
            // cioc The system controller addressed by Y (i.e., contains 
            // the word at Y) sends a connect signal to the port specified 
            // by C(Y) 33,35 .
            int cpu_port_num = query_scbank_map (iefpFinalAddress);

            // This shouldn't happen; every existing address is contained in a SCU, by defintion.
            // Therefore, this should throw a NEm fault.
            if (cpu_port_num < 0)
              {
                //sim_debug (DBG_ERR, & cpu_dev, "CIOC: Unable to determine port for address %08o; defaulting to port A\n", iefpFinalAddress);
                //cpu_port_num = 0;
                doFault (FAULT_ONC, nem, "(cioc)");
              }
            int scu_unit_num = query_scu_unit_num (ASSUME_CPU0, cpu_port_num);
            if (scu_unit_num < 0)
              {
                doFault (FAULT_ONC, nem, "(cioc)");
              }
            uint scu_port_num = CY & MASK3;
            scu_cioc ((uint) scu_unit_num, scu_port_num);
          }
          break;
   
        case 0553:  ///< smcm
            {
                // C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M processor) 
                // specify which processor port (i.e., which system 
                // controller) is used.
                uint cpu_port_num = (TPR.CA >> 15) & 03;
                int scu_unit_num = query_scu_unit_num (ASSUME_CPU0, cpu_port_num);
                if (scu_unit_num < 0)
                  {
                    if (cpu_port_num == 0)
                      putbits36 (& faultRegister [0], 16, 4, 010);
                    else if (cpu_port_num == 1)
                      putbits36 (& faultRegister [0], 20, 4, 010);
                    else if (cpu_port_num == 2)
                      putbits36 (& faultRegister [0], 24, 4, 010);
                    else
                      putbits36 (& faultRegister [0], 28, 4, 010);
                    doFault (FAULT_CMD, not_control, "(smcm)");
                  }
                t_stat rc = scu_smcm (scu_unit_num, ASSUME_CPU0, rA, rQ);
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
            uint cpu_port_num = (TPR.CA >> 15) & 03;
            int scu_unit_num = query_scu_unit_num (ASSUME_CPU0, cpu_port_num);

            if (scu_unit_num < 0)
              {
                if (cpu_port_num == 0)
                  putbits36 (& faultRegister [0], 16, 4, 010);
                else if (cpu_port_num == 1)
                  putbits36 (& faultRegister [0], 20, 4, 010);
                else if (cpu_port_num == 2)
                  putbits36 (& faultRegister [0], 24, 4, 010);
                else
                  putbits36 (& faultRegister [0], 28, 4, 010);
                doFault (FAULT_CMD, not_control, "(smic)");
              }
            t_stat rc = scu_smic (scu_unit_num, ASSUME_CPU0, cpu_port_num, rA);
            // Not used bu 4MW
            // if (rc == CONT_FAULT)
              // doFault (FAULT_STR, not_control, "(smic)");
            if (rc)
              return rc;
          }
          break;


        case 0057:  // sscr
          {
            uint cpu_port_num = (TPR.CA >> 15) & 03;
            int scu_unit_num = query_scu_unit_num (ASSUME_CPU0, cpu_port_num);

            if (scu_unit_num < 0)
              {
                if (cpu_port_num == 0)
                  putbits36 (& faultRegister [0], 16, 4, 010);
                else if (cpu_port_num == 1)
                  putbits36 (& faultRegister [0], 20, 4, 010);
                else if (cpu_port_num == 2)
                  putbits36 (& faultRegister [0], 24, 4, 010);
                else
                  putbits36 (& faultRegister [0], 28, 4, 010);
                doFault (FAULT_CMD, not_control, "(smic)");
              }
            t_stat rc = scu_sscr (scu_unit_num, ASSUME_CPU0, cpu_port_num, iefpFinalAddress & MASK15, rA, rQ);
            if (rc)
              return rc;
          }
            break;

        // Privileged - Miscellaneous
        case 0212:  ///< absa
          {
            word36 result;
            int rc = doABSA (& result);
            if (rc)
              return rc;
            rA = result;
            SCF (rA == 0, cu.IR, I_ZERO);
            SCF (rA & SIGN36, cu.IR, I_NEG);
          }
          break;
            
        case 0616:  // dis

            if (! switches . dis_enable)
              {
                return STOP_DIS;
              }

            if ((! switches . tro_enable) &&
                (! sample_interrupts ()) &&
                (sim_qcount () == 0))  // XXX If clk_svc is implemented it will 
                                     // break this logic
              {
                sim_printf ("DIS@0%06o with no interrupts pending and no events in queue\n", PPR.IC);
                sim_printf("\nsimCycles = %lld\n", sim_timell ());
                sim_printf("\ncpuCycles = %lld\n", sys_stats . total_cycles);
                //stop_reason = STOP_DIS;
                longjmp (jmpMain, JMP_STOP);
              }

// Multics/BCE halt
            if (PPR . PSR == 0430 && PPR . IC == 012)
                {
                  sim_printf ("BCE DIS causes CPU halt\n");
                  sim_debug (DBG_MSG, & cpu_dev, "BCE DIS causes CPU halt\n");
                  longjmp (jmpMain, JMP_STOP);
                }

            sim_debug (DBG_MSG, & cpu_dev, "entered DIS_cycle\n");
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
                cpu . interrupt_flag = true;
                break;
              }
            // Currently, the only G7 fault we recognize is TRO, so
            // this code suffices for "all other G7 faults."
            if (GET_I (cu . IWB) ? bG7PendingNoTRO () : bG7Pending ())
              {
                cpu . g7_flag = true;
                break;
              }
            else
              {
                sys_stats . total_cycles ++;
                longjmp (jmpMain, JMP_REFETCH);
              }
            
        default:
            if (switches . halt_on_unimp)
                return STOP_ILLOP;
            else
                doFault(FAULT_IPR, ill_op, "Illegal instruction");
    }
    return SCPE_OK;
}

// CANFAULT 
static t_stat DoEISInstruction (void)
{
    DCDstruct * i = & currentInstruction;
    int32 opcode = i->opcode;

    switch (opcode)
    {
        case 0604:  ///< tmoz
            /// If negative or zero indicator ON then
            /// C(TPR.CA) -> C(PPR.IC)
            /// C(TPR.TSR) -> C(PPR.PSR)
            if (cu.IR & (I_NEG | I_ZERO))
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;

#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;

        case 0605:  ///< tpnz
            /// If negative and zero indicators are OFF then
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            if (!(cu.IR & I_NEG) && !(cu.IR & I_ZERO))
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;

        case 0601:  ///< trtf
            /// If truncation indicator OFF then
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            if (!(cu.IR & I_TRUNC))
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;

        case 0600:  ///< trtn
            /// If truncation indicator ON then
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            if (cu.IR & I_TRUNC)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
                CLRF(cu.IR, I_TRUNC);
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;
            
        case 0606:  ///< ttn
            /// If tally runout indicator ON then
            ///  C(TPR.CA) -> C(PPR.IC)
            ///  C(TPR.TSR) -> C(PPR.PSR)
            /// otherwise, no change to C(PPR)
            if (cu.IR & I_TALLY)
            {
                PPR.IC = TPR.CA;
                PPR.PSR = TPR.TSR;
                
#ifdef RALR_FIX_0
                readOperands ();
#endif
                
                return CONT_TRA;
            }
            break;

        case 0310:  ///< easp1
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[1].SNR = TPR.CA & MASK15;
            break;
        case 0312:  ///< easp3
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[3].SNR = TPR.CA & MASK15;
            break;
        case 0330:  ///< easp5
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[5].SNR = TPR.CA & MASK15;
            break;
        case 0332:  ///< easp7
            /// C(TPR.CA) -> C(PRn.SNR)
            PR[7].SNR = TPR.CA & MASK15;
            break;

        case 0311:  ///< eawp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[1].WORDNO = TPR.CA;
            PR[1].BITNO = TPR.TBR;
            break;
        case 0313:  ///< eawp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[3].WORDNO = TPR.CA;
            PR[3].BITNO = TPR.TBR;
            break;
        case 0331:  ///< eawp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[5].WORDNO = TPR.CA;
            PR[5].BITNO = TPR.TBR;
            break;
        case 0333:  ///< eawp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.CA) -> C(PRn.WORDNO)
            ///  C(TPR.TBR) -> C(PRn.BITNO)
            PR[7].WORDNO = TPR.CA;
            PR[7].BITNO = TPR.TBR;
            break;        
        case 0350:  ///< epbp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[0].RNR = TPR.TRR;
            PR[0].SNR = TPR.TSR;
            PR[0].WORDNO = 0;
            PR[0].BITNO = 0;
            break;
        case 0352:  ///< epbp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[2].RNR = TPR.TRR;
            PR[2].SNR = TPR.TSR;
            PR[2].WORDNO = 0;
            PR[2].BITNO = 0;
            break;
        case 0370:  ///< epbp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[4].RNR = TPR.TRR;
            PR[4].SNR = TPR.TSR;
            PR[4].WORDNO = 0;
            PR[4].BITNO = 0;
            break;
        case 0372:  ///< epbp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(TPR.TRR) -> C(PRn.RNR)
            ///  C(TPR.TSR) -> C(PRn.SNR)
            ///  00...0 -> C(PRn.WORDNO)
            ///  0000 -> C(PRn.BITNO)
            PR[6].RNR = TPR.TRR;
            PR[6].SNR = TPR.TSR;
            PR[6].WORDNO = 0;
            PR[6].BITNO = 0;
            break;
         
        
        case 0351:  ///< epp1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[1].RNR = TPR.TRR;
            PR[1].SNR = TPR.TSR;
            PR[1].WORDNO = TPR.CA;
            PR[1].BITNO = TPR.TBR;
            break;
        case 0353:  ///< epp3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[3].RNR = TPR.TRR;
            PR[3].SNR = TPR.TSR;
            PR[3].WORDNO = TPR.CA;
            PR[3].BITNO = TPR.TBR;
            break;
        case 0371:  ///< epp5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[5].RNR = TPR.TRR;
            PR[5].SNR = TPR.TSR;
            PR[5].WORDNO = TPR.CA;
            PR[5].BITNO = TPR.TBR;
            break;
        case 0373:  ///< epp7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///   C(TPR.TRR) -> C(PRn.RNR)
            ///   C(TPR.TSR) -> C(PRn.SNR)
            ///   C(TPR.CA) -> C(PRn.WORDNO)
            ///   C(TPR.TBR) -> C(PRn.BITNO)
            PR[7].RNR = TPR.TRR;
            PR[7].SNR = TPR.TSR;
            PR[7].WORDNO = TPR.CA;
            PR[7].BITNO = TPR.TBR;
            break;
        
        case 0250:  ///< spbp0
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[0].SNR) << 18;
            Ypair[0] |= ((word36) PR[0].RNR) << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
        
        case 0252:  ///< spbp2
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[2].SNR) << 18;
            Ypair[0] |= ((word36) PR[2].RNR) << 15;
            Ypair[1] = 0;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandRead, rTAG);
            
            break;
            
        case 0650:  ///< spbp4
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[4].SNR) << 18;
            Ypair[0] |= ((word36) PR[4].RNR) << 15;
            Ypair[1] = 0;
        
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandRead, rTAG);
            
            break;
  
        case 0652:  ///< spbp6
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  000 -> C(Y-pair)0,2
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  00...0 -> C(Y-pair)36,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[6].SNR) << 18;
            Ypair[0] |= ((word36) PR[6].RNR) << 15;
            Ypair[1] = 0;
            
            //fWrite2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0251:  ///< spri1
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[1].SNR) << 18;
            Ypair[0] |= ((word36) PR[1].RNR) << 15;
            
            Ypair[1] = (word36) PR[1].WORDNO << 18;
            Ypair[1]|= (word36) PR[1].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;
    
        case 0253:  ///< spri3
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[3].SNR) << 18;
            Ypair[0] |= ((word36) PR[3].RNR) << 15;
            
            Ypair[1] = (word36) PR[3].WORDNO << 18;
            Ypair[1]|= (word36) PR[3].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0651:  ///< spri5
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[5].SNR) << 18;
            Ypair[0] |= ((word36) PR[5].RNR) << 15;
            
            Ypair[1] = (word36) PR[5].WORDNO << 18;
            Ypair[1]|= (word36) PR[5].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0653:  ///< spri7
            /// For n = 0, 1, ..., or 7 as determined by operation code
            ///  000 -> C(Y-pair)0,2
            ///  C(PRn.SNR) -> C(Y-pair)3,17
            ///  C(PRn.RNR) -> C(Y-pair)18,20
            ///  00...0 -> C(Y-pair)21,29
            ///  (43)8 -> C(Y-pair)30,35
            ///  C(PRn.WORDNO) -> C(Y-pair)36,53
            ///  000 -> C(Y-pair)54,56
            ///  C(PRn.BITNO) -> C(Y-pair)57,62
            ///  00...0 -> C(Y-pair)63,71
            Ypair[0] = 043;
            Ypair[0] |= ((word36) PR[7].SNR) << 18;
            Ypair[0] |= ((word36) PR[7].RNR) << 15;
            
            Ypair[1] = (word36) PR[7].WORDNO << 18;
            Ypair[1]|= (word36) PR[7].BITNO << 9;
            
            //Write2(i, TPR.CA, Ypair[0], Ypair[1], OperandWrite, rTAG);
            
            break;

        case 0754:  ///< sra
            /// 00...0 -> C(Y)0,32
            /// C(RALR) -> C(Y)33,35
            
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
                
                uint32 n = opcode & 07;  // get

                // C(Y)0,17 -> C(ARn.WORDNO)
                AR[n].WORDNO = GETHI(CY);

                int TA = (int)bitfieldExtract36(CY, 13, 2); // C(Y) 21-22
                int CN = (int)bitfieldExtract36(CY, 15, 3); // C(Y) 18-20
                
                switch(TA)
                {
                    case CTA4:  // 2
                        // If C(Y)21,22 = 10 (TA code = 2), then
                        //   C(Y)18,20 / 2 -> C(ARn.CHAR)
                        //   4 * (C(Y)18,20)mod2 + 1 -> C(ARn.BITNO)
                        // AR[n].CHAR = CN / 2;
                        // AR[n].ABITNO = 4 * (CN % 2) + 1;
                        SET_AR_CHAR_BIT (n,  CN / 2, 4 * (CN % 2) + 1);
                        break;
                        
                    case CTA6:  // 1
                        // If C(Y)21,22 = 01 (TA code = 1), then
                        //   (6 * C(Y)18,20) / 9 -> C(ARn.CHAR)
                        //   (6 * C(Y)18,20)mod9 -> C(ARn.BITNO)
                        // AR[n].CHAR = (6 * CN) / 9;
                        // AR[n].ABITNO = (6 * CN) % 9;
                        SET_AR_CHAR_BIT (n, (6 * CN) / 9, (6 * CN) % 9);
                        break;
                        
                    case CTA9:  // 0
                        // If C(Y)21,22 = 00 (TA code = 0), then
                        //   C(Y)18,19 -> C(ARn.CHAR)
                        //   0000 -> C(ARn.BITNO)
                        // AR[n].CHAR = (CN >> 1); // remember, 9-bit CN's are funky
                        // AR[n].ABITNO = 0;
                        SET_AR_CHAR_BIT (n, (CN >> 1), 0); // remember, 9-bit CN's are funky
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
            // For n = 0, 1, ..., or 7 as determined by operation code C(Y)0,23 -> C(ARn)
            {
                uint32 n = opcode & 07;  // get n
                AR[n].WORDNO = GETHI(CY);
                //AR[n].ABITNO = (word6)bitfieldExtract36(CY, 12, 4);
                //AR[n].CHAR  = (word2)bitfieldExtract36(CY, 16, 2);
                SET_AR_CHAR_BIT (n, (word6)bitfieldExtract36(CY, 12, 4), (word2)bitfieldExtract36(CY, 16, 2));
            }
            break;
            
        case 0463:  ///< lareg - Load Address Registers
            for(uint32 n = 0 ; n < 8 ; n += 1)
            {
                word36 tmp36 = Yblock8[n];

                AR[n].WORDNO = GETHI(tmp36);
                // AR[n].ABITNO = (word6)bitfieldExtract36(tmp36, 12, 4);
                // AR[n].CHAR  = (word2)bitfieldExtract36(tmp36, 16, 2);
                SET_AR_CHAR_BIT (n, (word6)bitfieldExtract36(tmp36, 12, 4), (word2)bitfieldExtract36(tmp36, 16, 2));
            }
            break;
            
        case 0467:  ///< lpl - Load Pointers and Lengths
            words2du (Yblock8);
            break;

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
                
                uint32 n = opcode & 07;  // get
                
                // C(Y)0,17 -> C(ARn.WORDNO)
                AR[n].WORDNO = GETHI(CY);
                
                int TN = (int)bitfieldExtract36(CY, 13, 1); // C(Y) 21
                int CN = (int)bitfieldExtract36(CY, 15, 3); // C(Y) 18-20
                
                switch(TN)
                {
                    case CTN4:   // 1
                        // If C(Y)21 = 1 (TN code = 1), then
                        //   (C(Y)18,20) / 2 -> C(ARn.CHAR)
                        //   4 * (C(Y)18,20)mod2 + 1 -> C(ARn.BITNO)
                        // AR[n].CHAR = CN / 2;
                        // AR[n].ABITNO = 4 * (CN % 2) + 1;
                        SET_AR_CHAR_BIT (n,  CN / 2, 4 * (CN % 2) + 1);
                        break;
                        
                    case CTN9:  // 0
                        // If C(Y)21 = 0 (TN code = 0), then
                        //   C(Y)18,20 -> C(ARn.CHAR)
                        //   0000 -> C(ARn.BITNO)
                        // AR[n].CHAR = CN;
                        // AR[n].ABITNO = 0;
                        SET_AR_CHAR_BIT (n, CN, 0); 
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
                
                // If C(Y)21,22 = 11 (TA code = 3) or C(Y)23 = 1 (unused bit), 
                // an illegal procedure fault occurs.
                if (TA == 03)
                  doFault (FAULT_IPR, ill_proc, "ARAn tag == 3");
                if (getbits36 (CY, 23, 1) != 0)
                  doFault (FAULT_IPR, ill_proc, "ARAn b23 == 1");

                uint32 n = opcode & 07;  // get
                // For n = 0, 1, ..., or 7 as determined by operation code
                
                // C(ARn.WORDNO) -> C(Y)0,17
                CY = bitfieldInsert36(CY, AR[n].WORDNO, 18, 18);
                
                // If TA = 1 (6-bit data) or TA = 2 (4-bit data), C(ARn.CHAR) and C(ARn.BITNO) are translated to an equivalent character position that goes to C(Y)18,20.

                int CN = 0;
                
                switch (TA)
                {
                    case CTA4:  // 2
                        // If C(Y)21,22 = 10 (TA code = 2), then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO) – 1) / 4 -> C(Y)18,20
                        CN = (9 * GET_AR_CHAR (n) /* AR[n].CHAR */ + GET_AR_BITNO (n) /* AR[n].ABITNO */ - 1) / 4;
                        CY = bitfieldInsert36(CY, CN, 15, 3);
                        break;
                        
                    case CTA6:  // 1
                        // If C(Y)21,22 = 01 (TA code = 1), then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO)) / 6 -> C(Y)18,20
                        CN = (9 * GET_AR_CHAR (n) /* AR[n].CHAR */ + GET_AR_BITNO (n) /* AR[n].ABITNO */) / 6;
                        CY = bitfieldInsert36(CY, CN, 15, 3);
                        break;
                        
                    case CTA9:  // 0
                        // If C(Y)21,22 = 00 (TA code = 0), then
                        //   C(ARn.CHAR) -> C(Y)18,19
                        //   0 -> C(Y)20
                        CY = bitfieldInsert36(CY,          0, 15, 1);
                        CY = bitfieldInsert36(CY, GET_AR_CHAR (n) /* AR[n].CHAR */, 16, 2);
                        break;
                }
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
                uint32 n = opcode & 07;  // get register #
                
                // The Numeric descriptor is fetched from Y and C(Y)21,22 (TA field) is examined to determine the data type described.
                
                int TN = (int)bitfieldExtract36(CY, 14, 1); // C(Y) 21
                
                // For n = 0, 1, ..., or 7 as determined by operation code
                // C(ARn.WORDNO) -> C(Y)0,17
                CY = bitfieldInsert36(CY, AR[n].WORDNO, 18, 18);
                
                int CN = 0;
                switch(TN)
                {
                    case CTN4:  // 1
                        // If C(Y)21 = 1 (TN code = 1) then
                        //   (9 * C(ARn.CHAR) + C(ARn.BITNO) – 1) / 4 -> C(Y)18,20
                        CN = (9 * GET_AR_CHAR (n) /* AR[n].CHAR */ + GET_AR_BITNO (n) /* AR[n].ABITNO */ - 1) / 4;
                        CY = bitfieldInsert36(CY, CN, 15, 3);
                        break;
                        
                    case CTN9:  // 0
                        // If C(Y)21 = 0 (TN code = 0), then
                        //   C(ARn.CHAR) -> C(Y)18,19
                        //   0 -> C(Y)20
                        CY = bitfieldInsert36(CY,          0, 15, 1);
                        CY = bitfieldInsert36(CY, GET_AR_CHAR (n) /* AR[n].CHAR */, 16, 2);
                        break;
                }
                
                //If C(Y)21,22 = 11 (TA code = 3) or C(Y)23 = 1 (unused bit), an illegal procedure fault occurs.
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
            //  C(ARn) -> C(Y)0,23
            //  C(Y)24,35 -> unchanged
            {
                uint32 n = opcode & 07;  // get n
                CY = bitfieldInsert36(CY, AR[n].WORDNO, 18, 18);
                CY = bitfieldInsert36(CY, GET_AR_BITNO (n) /* AR[n].ABITNO */,  12,  4);
                CY = bitfieldInsert36(CY, GET_AR_CHAR (n) /* AR[n].CHAR */,   16,  2);
            }
            break;
            
        case 0443:  ///< sareg - Store Address Registers
            memset(Yblock8, 0, sizeof(Yblock8));
            for(uint32 n = 0 ; n < 8 ; n += 1)
            {
                word36 arx = 0;
                arx = bitfieldInsert36(arx, AR[n].WORDNO, 18, 18);
                arx = bitfieldInsert36(arx, GET_AR_BITNO (n) /* AR[n].ABITNO */,  12,  4);
                arx = bitfieldInsert36(arx, GET_AR_CHAR (n) /* AR[n].CHAR */,   16,  2);
                
                Yblock8[n] = arx;
            }
            break;
            
        case 0447:  ///< spl - Store Pointers and Lengths
            du2words (Yblock8);
          break;
            
        // EIS - Address Register Special Arithmetic
        case 0500:  // a9bd Add 9-bit Displacement to Address Register
          axbd (9);
          break;
            
        case 0501:  // a6bd Add 6-bit Displacement to Address Register
          axbd (6);
          break;
            
        case 0502:  // a4bd Add 4-bit Displacement to Address Register
          a4bd ();
          break;
            
// If defined, do all ABD calculations in bits, not chars and bits in chars.
#define ABD_BITS
        case 0503:  // abd  Add bit Displacement to Address Register
          axbd (1);
          break;
            
        case 0507:  // awd Add  word Displacement to Address Register
          axbd (36);
          break;
            
        case 0520:  // s9bd   Subtract 9-bit Displacement from Address Register
          sxbd (9);
          break;
            
        case 0521:  // s6bd   Subtract 6-bit Displacement from Address Register
          sxbd (6);
          break;
                
        case 0522:  // s4bd Subtract 4-bit Displacement from Address Register
          s4bd ();
          break;
            
        case 0523:  // sbd Subtract   bit Displacement from Address Register
          sxbd (1);
          break;
            
        case 0527:  // swd Subtract  word Displacement from Address Register
          sxbd (36);
          break;
            
        /// Multiword EIS ...
        case 0301:  // btd
          btd ();
          break;
            
        case 0305:  // dtb
          dtb ();
          break;
            
        case 0024:   // mvne
          mvne ();
          break;
         
        case 0020:   // mve
          mve ();
          break;

        case 0100:  // mlr
          mlr();
          break;

        case 0101:  // mrl
          mrl ();
          break;
        
        case 0160:  // mvt
          mvt ();
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
            
        case 0106:  // cmpc
          cmpc ();
          break;
            
        case 0120:  // scd
          scd ();
          break;
      
        case 0121:  // scdr
          scdr ();
          break;
          
        // bit-string operations
        case 0066:   // cmpb
          cmpb ();
          break;

        case 0060:   // csl
          csl (false);
          break;

        case 0061:   // csr
          csr (false);
          break;

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

        // decimal arithmetic instrutions
        case 0202:  ///< ad2d
            ad2d ();
            break;

        case 0222:  ///< ad3d
            ad3d ();
            break;
            
        case 0203:  ///< sb2d
            sb2d ();
            break;
            
        case 0223:  ///< sb3d
            sb3d ();
            break;

        case 0206:  ///< mp2d
            mp2d ();
            break;

        case 0226:  ///< mp3d
            mp3d ();
            break;

        case 0207:  ///< dv2d
            dv2d ();
            break;

        case 0227:  ///< dv3d
            dv3d ();
            break;

        case 0300:  // mvn
          mvn ();
          break;
        
        case 0303:  // cmpn
          cmpn ();
          break;

#if EMULATOR_ONLY
            
        case 0420:  ///< emcall instruction Custom, for an emulator call for simh stuff ...
        {
            int ret = emCall ();
            if (ret)
              return ret;
            break;
        }

#endif
        // priviledged instructions
            
        case 0173:  ///< lptr
            // Not clear what the subfault should be; see Fault Register in AL39.
            doFault(FAULT_IPR, ill_proc, "lptr is illproc on DPS8M");

        case 0232:  ///< lsdr
            // Not clear what the subfault should be; see Fault Register in AL39.
            doFault(FAULT_IPR, ill_proc, "lsdr is illproc on DPS8M");

        case 0257:  ///< lptp
            // Not clear what the subfault should be; see Fault Register in AL39.
            doFault(FAULT_IPR, ill_proc, "lptp is illproc on DPS8M");

        case 0774:  ///< lra
            rRALR = CY & MASK3;
            sim_debug (DBG_TRACE, & cpu_dev, "RALR set to %o\n", rRALR);
            break;

        case 0557:  ///< sptp
          {
// XXX The associative memory is ignored (forced to "no match") during address preparation.

#ifndef SPEED
            // Level j is selected by C(TPR.CA)12,13
            uint level = (TPR . CA >> 4) & 02u;
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                Yblock16 [i] = 0;
#ifndef SPEED
                putbits36 (& Yblock16 [i], 0, 15, PTWAM [toffset + i] . POINTER);
                putbits36 (& Yblock16 [i], 15, 12, PTWAM [toffset + i] . PAGENO);
                putbits36 (& Yblock16 [i], 27, 1, PTWAM [toffset + i] . F);
                putbits36 (& Yblock16 [i], 30, 6, PTWAM [toffset + i] . USE);
#endif
              }
          }
          break;

        case 0154:  ///< sptr
          {
// XXX The associative memory is ignored (forced to "no match") during address preparation.

#ifndef SPEED
            // Level j is selected by C(TPR.CA)12,13
            uint level = (TPR . CA >> 4) & 02u;
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                Yblock16 [i] = 0;
#ifndef SPEED
                putbits36 (& Yblock16 [i], 0, 13, PTWAM [toffset + i] . ADDR);
                putbits36 (& Yblock16 [i], 29, 1, PTWAM [toffset + i] . M);
#endif
              }
          }
          break;

        case 0254:  ///< ssdr
          {
// XXX The associative memory is ignored (forced to "no match") during address preparation.

#ifndef SPEED
            // Level j is selected by C(TPR.CA)12,13
            uint level = (TPR . CA >> 4) & 02u;
            uint toffset = level * 16;
#endif
            for (uint i = 0; i < 16; i ++)
              {
                Yblock32 [i * 2] = 0;
#ifndef SPEED
                putbits36 (& Yblock32 [i * 2], 0, 23, SDWAM [toffset + i] . ADDR);
                putbits36 (& Yblock32 [i * 2], 24, 3, SDWAM [toffset + i] . R1);
                putbits36 (& Yblock32 [i * 2], 27, 3, SDWAM [toffset + i] . R2);
                putbits36 (& Yblock32 [i * 2], 30, 3, SDWAM [toffset + i] . R3);
#endif
                Yblock32 [i * 2 + 1] = 0;
#ifndef SPEED
                putbits36 (& Yblock32 [i * 2 + 1], 37 - 36, 14, SDWAM [toffset + i] . BOUND);
                putbits36 (& Yblock32 [i * 2 + 1], 51 - 36, 1, SDWAM [toffset + i] . R);
                putbits36 (& Yblock32 [i * 2 + 1], 52 - 36, 1, SDWAM [toffset + i] . E);
                putbits36 (& Yblock32 [i * 2 + 1], 53 - 36, 1, SDWAM [toffset + i] . W);
                putbits36 (& Yblock32 [i * 2 + 1], 54 - 36, 1, SDWAM [toffset + i] . P);
                putbits36 (& Yblock32 [i * 2 + 1], 55 - 36, 1, SDWAM [toffset + i] . U);
                putbits36 (& Yblock32 [i * 2 + 1], 56 - 36, 1, SDWAM [toffset + i] . G);
                putbits36 (& Yblock32 [i * 2 + 1], 57 - 36, 1, SDWAM [toffset + i] . C);
                putbits36 (& Yblock32 [i * 2 + 1], 58 - 36, 14, SDWAM [toffset + i] . CL);
#endif
              }
          }
          break;

        // Privileged - Clear Associative Memory
        case 0532:  ///< camp
            do_camp (TPR.CA);
            break;
            
        default:
            if (switches . halt_on_unimp)
                return STOP_ILLOP;
            else
                doFault(FAULT_IPR, ill_op, "Illegal instruction");
    }

    return SCPE_OK;

}



#include <ctype.h>

/**
 * emulator call instruction. Do whatever address field sez' ....
 */
static int emCall (void)
{
    DCDstruct * i = & currentInstruction;
    switch (i->address) /// address field
    {
        case 1:     ///< putc9 - put 9-bit char in AL to stdout
        {
            if (rA > 0xff)  // don't want no 9-bit bytes here!
                break;
            
            char c = rA & 0x7f;
            if (c)  // ignore NULL chars. 
                // putc(c, stdout);
                //sim_putchar(c);
                sim_printf("%c", c);
            break; 
        }
        case 0100:     ///< putc9 - put 9-bit char in A(0) to stdout
        {
            char c = (rA >> 27) & 0x7f;
            if (isascii(c))  // ignore NULL chars.
                //putc(c, stdout);
                //sim_putchar(c);
                sim_printf("%c", c);
            else
                sim_printf("\\%03o", c);
            break;
        }
        case 2:     ///< putc6 - put 6-bit char in A to stdout
        {
            int c = GEBcdToASCII[rA & 077];
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
        case 3:     ///< putoct - put octal contents of A to stdout (split)
        {
            sim_printf("%06o %06o", GETHI(rA), GETLO(rA));
            break;
        }
        case 4:     ///< putoctZ - put octal contents of A to stdout (zero-suppressed)
        {
            sim_printf("%llo", rA);
            break;
        }
        case 5:     ///< putdec - put decimal contents of A to stdout
        {
            t_int64 tmp = SIGNEXT36_64(rA);
            sim_printf("%lld", tmp);
            break;
        }
        case 6:     ///< putEAQ - put float contents of C(EAQ) to stdout
        {
            long double eaq = EAQToIEEElongdouble();
            sim_printf("%12.8Lg", eaq);
            break;
        }
        case 7:   ///< dump index registers
            //sim_printf("Index registers * * *\n");
            for(int i = 0 ; i < 8 ; i += 4)
                sim_printf("r[%d]=%06o r[%d]=%06o r[%d]=%06o r[%d]=%06o\n", i+0, rX[i+0], i+1, rX[i+1], i+2, rX[i+2], i+3, rX[i+3]);
            break;
            
        case 17: ///< dump pointer registers
            for(int n = 0 ; n < 8 ; n++)
            {
                sim_printf("PR[%d]/%s: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\n",
                          n, PRalias[n], PR[n].SNR, PR[n].RNR, PR[n].WORDNO, PR[n].BITNO);
            }
            break;
        case 27:    // dump registers A & Q
            sim_printf("A: %012llo Q:%012llo\n", rA, rQ);
            break;
            
        case 8: ///< crlf to console
            sim_printf("\n");
            break;
            
        case 13:     ///< putoct - put octal contents of Q to stdout (split)
        {
            sim_printf("%06o %06o", GETHI(rQ), GETLO(rQ));
            break;
        }
        case 14:     ///< putoctZ - put octal contents of Q to stdout (zero-suppressed)
        {
            sim_printf("%llo", rQ);
            break;
        }
        case 15:     ///< putdec - put decimal contents of Q to stdout
        {
            t_int64 tmp = SIGNEXT36_64(rQ);
            sim_printf("%lld", tmp);
            break;
        }

        case 16:     ///< puts - A high points to by an aci string; print it.
                     // The string includes C-sytle escapes: \0 for end
                     // of string, \n for newline, \\ for a backslash
        case 21: // puts: A contains a 24 bit address
        {
            const int maxlen = 256;
            char buf [maxlen + 1];

            word36 addr;
            if (i->address == 16)
              addr = rA >> 18;
            else // 21
              addr = rA >> 12;
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

        case 18:     ///< halt
            return STOP_HALT;

        case 19:     ///< putdecaq - put decimal contents of AQ to stdout
        {
            int64_t t0 = SIGNEXT36_64 (rA);
            __int128_t AQ = ((__int128_t) t0) << 36;
            AQ |= (rQ & DMASK);
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

// XXX This code may be redundant
// CANFAULT 
static int doABSA (word36 * result)
  {
    DCDstruct * i = & currentInstruction;
    word36 res;
    sim_debug (DBG_APPENDING, & cpu_dev, "absa CA:%08o\n", TPR.CA);

    if (get_addr_mode () == ABSOLUTE_mode && ! i -> a)
      {
        //sim_debug (DBG_ERR, & cpu_dev, "ABSA in absolute mode\n");
        // Not clear what the subfault should be; see Fault Register in AL39.
        //doFault (FAULT_IPR, ill_proc, "ABSA in absolute mode.");
        * result = (TPR . CA & MASK18) << 12; // 24:12 format
        return SCPE_OK;
      }

    if (DSBR.U == 1) // Unpaged
      {
        sim_debug (DBG_APPENDING, & cpu_dev, "absa DSBR is unpaged\n");
        // 1. If 2 * segno >= 16 * (DSBR.BND + 1), then generate an access
        // violation, out of segment bounds, fault.

        sim_debug (DBG_APPENDING, & cpu_dev, 
          "absa Boundary check: TSR: %05o f(TSR): %06o "
          "BND: %05o f(BND): %06o\n", 
          TPR . TSR, 2 * (uint) TPR . TSR, 
          DSBR . BND, 16 * ((uint) DSBR . BND + 1));

        if (2 * (uint) TPR . TSR >= 16 * ((uint) DSBR . BND + 1))
          {
            doFault (FAULT_ACV, ACV15, "ABSA in DSBR boundary violation.");
          }

        // 2. Fetch the target segment SDW from DSBR.ADDR + 2 * segno.

        sim_debug (DBG_APPENDING, & cpu_dev,
          "absa DSBR.ADDR %08o TSR %o SDWe offset %o SWDe %08o\n",
          DSBR . ADDR, TPR . TSR, 2 * TPR . TSR, 
          DSBR . ADDR + 2 * TPR . TSR);

        word36 SDWe, SDWo;
        core_read ((DSBR . ADDR + 2 * TPR . TSR) & PAMASK, & SDWe, __func__);
        core_read ((DSBR . ADDR + 2 * TPR . TSR  + 1) & PAMASK, & SDWo, __func__);

//sim_debug (DBG_TRACE, & cpu_dev, "absa SDW0 %s\n", strSDW0 (& SDW0));
//sim_debug (DBG_TRACE, & cpu_dev, "absa  DSBR.ADDR %08o TPR.TSR %08o\n", DSBR . ADDR, TPR . TSR);
//sim_debug (DBG_TRACE, & cpu_dev, "absa  SDWaddr: %08o SDW: %012llo %012llo\n", DSBR . ADDR + 2 * TPR . TSR, SDWe, SDWo);
        // 3. If SDW.F = 0, then generate directed fault n where n is given in
        // SDW.FC. The value of n used here is the value assigned to define a
        // missing segment fault or, simply, a segment fault.

        // ABSA doesn't care if the page isn't resident


        // 4. If offset >= 16 * (SDW.BOUND + 1), then generate an access violation, out of segment bounds, fault.

        word14 BOUND = (SDWo >> (35u - 14u)) & 037777u;
        if (TPR . CA >= 16u * (BOUND + 1u))
          {
            doFault (FAULT_ACV, ACV15, "ABSA in SDW boundary violation.");
          }

        // 5. If the access bits (SDW.R, SDW.E, etc.) of the segment are incompatible with the reference, generate the appropriate access violation fault.

        // t4d doesn't care
        // XXX Don't know what the correct behavior is here for ABSA


        // 6. Generate 24-bit absolute main memory address SDW.ADDR + offset.

        word24 ADDR = (SDWe >> 12) & 077777760;
        res = (word36) ADDR + (word36) TPR.CA;
        res &= PAMASK; //24 bit math
        res <<= 12; // 24:12 format

      }
    else
      {
        sim_debug (DBG_APPENDING, & cpu_dev, "absa DSBR is paged\n");
        // paged
        word15 segno = TPR . TSR;
        word18 offset = TPR . CA;

        // 1. If 2 * segno >= 16 * (DSBR.BND + 1), then generate an access 
        // violation, out of segment bounds, fault.

        sim_debug (DBG_APPENDING, & cpu_dev, 
          "absa Segment boundary check: segno: %05o f(segno): %06o "
          "BND: %05o f(BND): %06o\n", 
          segno, 2 * (uint) segno, 
          DSBR . BND, 16 * ((uint) DSBR . BND + 1));

        if (2 * (uint) segno >= 16 * ((uint) DSBR . BND + 1))
          {
            doFault (FAULT_ACV, ACV15, "ABSA in DSBR boundary violation.");
          }

        // 2. Form the quantities:
        //       y1 = (2 * segno) modulo 1024
        //       x1 = (2 * segno ­ y1) / 1024

        word24 y1 = (2 * segno) % 1024;
        word24 x1 = (2 * segno - y1) / 1024;

        sim_debug (DBG_APPENDING, & cpu_dev, 
          "absa y1:%08o x1:%08o\n", y1, x1);

        // 3. Fetch the descriptor segment PTW(x1) from DSBR.ADR + x1.

        sim_debug (DBG_APPENDING, & cpu_dev, 
          "absa read PTW1@%08o+%08o %08o\n",
          DSBR . ADDR, x1, (DSBR . ADDR + x1) & PAMASK);

        word36 PTWx1;
        core_read ((DSBR . ADDR + x1) & PAMASK, & PTWx1, __func__);

        struct _ptw0 PTW1;
        PTW1.ADDR = GETHI(PTWx1);
        PTW1.U = TSTBIT(PTWx1, 9);
        PTW1.M = TSTBIT(PTWx1, 6);
        PTW1.F = TSTBIT(PTWx1, 2);
        PTW1.FC = PTWx1 & 3;

        sim_debug (DBG_APPENDING, & cpu_dev, 
          "absa PTW1 ADDR %08o U %o M %o F %o FC %o\n", 
          PTW1 . ADDR, PTW1 . U, PTW1 . M, PTW1 . F, PTW1 . FC);

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
          PTW1 . ADDR, y1, ((PTW1 . ADDR << 6) + y1) & PAMASK);

        word36 SDWeven, SDWodd;
        core_read2(((PTW1 . ADDR << 6) + y1) & PAMASK, & SDWeven, & SDWodd, __func__);

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
          SDW0 . ADDR, SDW0 . R1, SDW0 . R2, SDW0 . R3, SDW0 . F, 
          SDW0 . FC);

        sim_debug (DBG_APPENDING, & cpu_dev, 
          "absa SDW0 BOUND %06o R %o E %o W %o P %o U %o G %o C %o "
          "EB %05o\n", 
          SDW0 . BOUND, SDW0 . R, SDW0 . E, SDW0 . W, SDW0 . P, SDW0 . U,
          SDW0 . G, SDW0 . C, SDW0 . EB);


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
          offset, offset >> 4, SDW0 . BOUND);

        if (((offset >> 4) & 037777) > SDW0 . BOUND)
          {
            sim_debug (DBG_APPENDING, & cpu_dev, "absa SDW boundary violation\n");
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
              SDW0 . ADDR, x2, (SDW0 . ADDR + x2) & PAMASK);

            word36 PTWx2;
            core_read ((SDW0 . ADDR + x2) & PAMASK, & PTWx2, __func__);
    
            struct _ptw0 PTW_2;
            PTW_2.ADDR = GETHI(PTWx2);
            PTW_2.U = TSTBIT(PTWx2, 9);
            PTW_2.M = TSTBIT(PTWx2, 6);
            PTW_2.F = TSTBIT(PTWx2, 2);
            PTW_2.FC = PTWx2 & 3;

            sim_debug (DBG_APPENDING, & cpu_dev, 
              "absa PTW_2 ADDR %08o U %o M %o F %o FC %o\n", 
              PTW_2 . ADDR, PTW_2 . U, PTW_2 . M, PTW_2 . F, PTW_2 . FC);

            // 11.If PTW(x2).F = 0, then generate directed fault n where n is 
            // given in PTW(x2).FC. This is a page fault as in Step 4 above.

            // ABSA only wants the address; it doesn't care if the page is
            // resident

            // if (!PTW_2.F)
            //   {
            //     sim_debug (DBG_APPENDING, & cpu_dev, "absa fault !PTW_2.F\n");
            //     // initiate a directed fault
            //     doFault(FAULT_DF0 + PTW_2.FC, 0, "ABSA !PTW_2.F");
            //   }

            // 12. Generate the 24-bit absolute main memory address 
            // PTW(x2).ADDR + y2.

            res = (((word36) PTW_2 . ADDR) << 6)  + (word36) y2;
            res &= PAMASK; //24 bit math
            res <<= 12; // 24:12 format
          }
        else
          {
            // Segment is unpaged
            // SDW0.ADDR is the base address of the segment
            res = (word36) SDW0 . ADDR + offset;
            res &= PAMASK; //24 bit math
            res <<= 12; // 24:12 format
          }
      }


    * result = res;

    return SCPE_OK;
  }

void doRCU (void)
  {

    words2scu (Yblock8);

// Restore addressing mode

#if 0
    if ((cu . IR & I_NBAR) == 0) // BAR
      {
        if ((cu . IR & I_ABS) == 0)
          set_addr_mode (APPEND_BAR_mode);
        else
          set_addr_mode (BAR_mode);
      }
    else // not BAR
      {
        if ((cu . IR & I_ABS) == 0)
          set_addr_mode (APPEND_mode);
        else
          set_addr_mode (ABSOLUTE_mode);

      }
#else
    if ((cu . IR & I_ABS) == 0)
      set_addr_mode (APPEND_mode);
    else
      set_addr_mode (ABSOLUTE_mode);
#endif


//sim_printf ("F/I is %d\n", cu . FLT_INT);
    if (cu . FLT_INT == 0) // is interrupt, not fault
      {
        //cpu . cycle = FETCH_cycle;
        //longjmp (jmpMain, JMP_REENTRY);
//sim_printf ("int refetch\n");
        longjmp (jmpMain, JMP_REFETCH);
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
//        SMIC: store fault (not control)   > I believe that these should be command fault
//        SSCR: store fault (not control)  /
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

    if (cu . rfi || // S/W asked for the instruction to be started
        cu . FIF) // fault occured during instruction fetch
      {
        longjmp (jmpMain, JMP_REFETCH);
      }

// It seems obvious that MMEx should do a JMP_SYNC_FAULT_RETURN, but doing
// a JMP_RESTART makes 'debug' work. (The same change to DRL does not make
// 'gtss' work, tho.

    if (cu . FI_ADDR == FAULT_MME2)
      {
        longjmp (jmpMain, JMP_RESTART);
      }

    // MME faults resume with the next instruction

    if (cu . FI_ADDR == FAULT_MME ||
        /* cu . FI_ADDR == FAULT_MME2 || */
        cu . FI_ADDR == FAULT_MME3 ||
        cu . FI_ADDR == FAULT_MME4 ||
        cu . FI_ADDR == FAULT_DRL ||
        cu . FI_ADDR == FAULT_DIV ||
        cu . FI_ADDR == FAULT_OFL ||
        cu . FI_ADDR == FAULT_IPR)
      longjmp (jmpMain, JMP_SYNC_FAULT_RETURN);

    // LUF can happen during fetch or CAF. If fetch, handled above
    if (cu . FI_ADDR == FAULT_LUF)
      {
        cu . IR |= I_MIIF;
        longjmp (jmpMain, JMP_RESTART);
      }

    if (cu . FI_ADDR == FAULT_DF0 || 
        cu . FI_ADDR == FAULT_DF1 || 
        cu . FI_ADDR == FAULT_DF2 || 
        cu . FI_ADDR == FAULT_DF3 || 
        cu . FI_ADDR == FAULT_ACV || 
        cu . FI_ADDR == FAULT_F1 || 
        cu . FI_ADDR == FAULT_F2 || 
        cu . FI_ADDR == FAULT_F3 ||
        //cu . FI_ADDR == FAULT_DRL ||
        cu . FI_ADDR == FAULT_CMD ||
        cu . FI_ADDR == FAULT_EXF)
        //cu . FI_ADDR == FAULT_OFL)
      {
        // If the fault occurred during fetch, handled above.
        cu . IR |= I_MIIF;
        longjmp (jmpMain, JMP_RESTART);
      }
    sim_printf ("doRCU dies with unhandled fault number %d\n", cu . FI_ADDR);
    doFault (FAULT_TRB, cu . FI_ADDR, "doRCU dies with unhandled fault number");
  }

#ifdef REAL_TR
static uint timerRegVal;
static struct timeval timerRegT0;
//static bool overrunAck;

void setTR (word27 val)
  {
    val &= MASK27;
    if (val)
      {
        timerRegVal = val & MASK27;
      }
    else
      {
        // Special case
        timerRegVal = -1 & MASK27;
      }
    gettimeofday (& timerRegT0, NULL);
    //overrunAck = false;

//sim_printf ("tr set %10u %09o %10lu%06lu\n", val, timerRegVal, timerRegT0 . tv_sec, timerRegT0 . tv_usec);
  }

word27 getTR (bool * runout)
  {
#if 0
    struct timeval tnow, tdelta;
    gettimeofday (& tnow, NULL);
    timersub (& tnow, & timerRegT0, & tdelta);
    // 1000000 can be represented in 20 bits; so in a 64 bit word, we have room for
    // 44 bits of seconds, way more then enough.
    // Do 64 bit math; much faster.
    //
    //delta = (tnowus - t0us) / 1.953125
    uint64 delta;
    delta = ((uint64) tdelta . tv_sec) * 1000000 + ((uint64) tdelta . tv_usec);
    // 1M * 1M ~= 40 bits; still leaves 24bits of seconds.
    delta = (delta * 1000000) / 1953125;
#else
    uint128 t0us, tnowus, delta;
    struct timeval tnow;
    gettimeofday (& tnow, NULL);
    t0us = timerRegT0 . tv_sec * 1000000 + timerRegT0 . tv_usec;
    tnowus = tnow . tv_sec * 1000000 + tnow . tv_usec;
    //delta = (tnowus - t0us) / 1.953125
    delta = ((tnowus - t0us) * 1000000) / 1953125;
#endif
    if (runout)
     //* runout = (! overrunAck) && delta > timerRegVal;
     * runout = delta > timerRegVal;
    word27 val = (timerRegVal - delta) & MASK27;
//if (val % 100000 == 0) sim_printf ("tr get %10u %09o %8llu %s\n", val, val, (unsigned long long) delta, runout ? * runout ? "runout" : "" : "");
    return val;
  }

void ackTR (void)
  {
    //overrunAck = true;
    setTR (0);
  }
#endif

