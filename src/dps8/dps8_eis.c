/**
 * \file dps8_eis.c
 * \project dps8
 * \date 12/31/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
 * \brief EIS support code...
*/

#define V3

#ifdef V1
// This makes MVE 1-28 and all of MVNE work
// Fails MVE 29-37 (6->6)
#define decimalZero (e->srcTA != CTA4 ? '0' : 0)
#endif

#ifdef V2
// This makes MVE 1-10 and 20-37 and all of MVNE work
// Fails MVE 11-19 (6->9)
#define decimalZero (e->srcTA == CTA9 ? '0' : 0)
#endif

#include <stdio.h>
//#define DBGF // page fault debugging
//#define DBGX
#include <ctype.h>

#include "dps8.h"
#include "dps8_cpu.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_iefp.h"
#include "dps8_faults.h"

#ifdef DBGF
// debugging
static word18 dbgAddr0;
static word36 dbgData0;
static word18 dbgAddr1;
static word36 dbgData1;
#endif

static word18 getMFReg18 (uint n, bool allowDUL);
static word36 getMFReg36 (uint n, bool allowDUL);
static word4 get4 (word36 w, int pos);
static word6 get6 (word36 w, int pos);
static word9 get9 (word36 w, int pos);

struct MOPstruct
{
    char *mopName;             // name of microoperation
    int (*f)(EISstruct *e);    // pointer to mop() [returns character to be stored]
};


static word36 EIScac (EISaddr * p, int offset, int ta)
  {
    word36 data;
    int maxChars = 4; // CTA9
    switch(ta)
    {
        case CTA4:
            maxChars = 8;
            break;
            
        case CTA6:
            maxChars = 6;
            break;
    }
    int woffset = offset / maxChars;
    int coffset = offset % maxChars;

    if (p -> mat == viaPR)    //&& get_addr_mode() == APPEND_mode)
      {
        TPR . TRR = p -> RNR;
        TPR . TSR = p -> SNR;
        
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read %o:%06o\n", 
                   __func__, TPR . TSR, p -> address);
        Read (p -> address + woffset, & data, EIS_OPERAND_READ, true);
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read* %012llo@%o:%06o\n", 
                   __func__, data, TPR . TSR, p -> address);
      }
    else
      {
        if (get_addr_mode () == APPEND_mode)
          {
            TPR . TRR = PPR . PRR;
            TPR . TSR = PPR . PSR;
          }
        
        Read (p -> address + woffset, & data, EIS_OPERAND_READ, false);
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read %012llo@%o:%06o\n", 
                   __func__, data, TPR . TSR, p -> address);
      }

    word36 c = 0;
    switch (ta)
      {
        case CTA4:
          c = get4 (data, coffset);
          break;
        case CTA6:
          c = get6 (data, coffset);
          break;
        case CTA9:
          c = get9 (data, coffset);
          break;
      }
    return c;
  }
#ifdef DBGF
static void packCharBit (word6 * D_PTR_B, word3 TAk, uint effCHAR, uint effBITNO)
  {
    switch (TAk)
      {
        case CTA4:
          // CHARNO 0-7, BITNO 0-3
          * D_PTR_B = effCHAR * 4 + effBITNO;
        case CTA6:
          // CHARNO 0-5, BITNO 0-5
          * D_PTR_B = effCHAR * 6 + effBITNO;
        case CTA9:
          // CHARNO 0-3, BITNO 0-8
          * D_PTR_B = effCHAR * 9 + effBITNO;
        //default:
          //doFault (illproc_fault, 0, "illegal TAk");
      }
  }

static void unpackCharBit (word6 D_PTR_B, word3 TAk, uint * effCHAR, uint * effBITNO)
  {
    switch (TAk)
      {
        case CTA4:
          // CHARNO 0-7, BITNO 0-3
          // * D_PTR_B = effCHAR * 4 + effBITNO;
          * effCHAR = D_PTR_B / 4;
          * effBITNO = D_PTR_B % 4;
        case CTA6:
          // CHARNO 0-5, BITNO 0-5
          // * D_PTR_B = effCHAR * 6 + effBITNO;
          * effCHAR = D_PTR_B / 6;
          * effBITNO = D_PTR_B % 6;
        case CTA9:
          // CHARNO 0-3, BITNO 0-8
          // * D_PTR_B = effCHAR * 9 + effBITNO;
          * effCHAR = D_PTR_B / 9;
          * effBITNO = D_PTR_B % 9;
        //default:
          //doFault (illproc_fault, 0, "illegal TAk");
      }
  }

//
// 5.2.10.5  Operand Descriptor Address Preparation Flowchart
//
// A flowchart of the operations involved in operand descriptor address
// preparation is shown in Figure 5-2. The chart depicts the address
// preparation for operand descriptor 1 of a multiword instruction as described
// by modification field 1 (MF1). A similar type address preparation would be
// carried out for each operand descriptor as specified by its MF code.
//
//    (Bull Nova 9000 pg 5-40  67 A2 RJ78 REV02)
//
// 1. The multiword instruction is obtained from memory.
//
// 2. The indirect (ID) bit of MF1 is queried to determine if the descriptor
// for operand 1 is present or is an indirect word.
//
// 3. This step is reached only if an indirect word was in the operand
// descriptor location. Address modification for the indirect word is now
// performed. If the AR bit of the indirect word is 1, address register
// modification step 4 is performed.
//
// 4. The y field of the indirect word is added to the contents of the
// specified address register.
//
// 5. A check is now made to determine if the REG field of the indirect word
// specifies that a register type modification be performed.
//
// 6. The indirect address as modified by the address register is now modified
// by the contents of the specified register, producing the effective address
// of the operand descriptor.
//
// 7. The operand descriptor is obtained from the location determined by the
// generated effective address in item 6.
//
// 8. Modification of the operand descriptor address begins. This step is
// reached directly from 2 if no indirection is involved. The AR bit of MF1 is
// checked to determine if address register modification is specified.
//
// 9. Address register modification is performed on the operand descriptor as
// described under "Address Modification with Address Registers" above. The
// character and bit positions of the specified address register are used in
// one of two ways, depending on the type of operand descriptor, i.e., whether
// the type is a bit string, a numeric, or an alphanumeric descriptor.
//
// 10. The REG field of MF1 is checked for a legal code. If DU is specified in
// the REG field of MF2 in one of the four multiword instructions (SCD, SCDR,
// SCM, or SCMR) for which DU is legal, the CN field is ignored and the
// character or characters are arranged within the 18 bits of the word address
// portion of the operand descriptor.
//
// 11. The count contained in the register specified by the REG field code is
// appropriately converted and added to the operand address.
//
// 12. The operand is retrieved from the calculated effective address location.
//

// CANFAULT

static void mySetupOperandDesc (int k)
  {

// mySetupOperandDesc fills in the following:
//
//   du . MFk
//   du . Dk_PTR_W  -- The *pre-indirect* operand address
//   du . Dk_RES
//   du . Fk       <-- true
//   du . Ak       <-- true

// temp while buggy
word15 saveTSR = TPR . TSR;
word3 saveTRR = TPR . TRR;

// XXX restart: only do this if the valid bit[k] is not set
    DCDstruct * ci = & currentInstruction;

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "mySetupOperandDesc k %d IWB %012llo\n", k, cu . IWB);
    word7 MFk;
    uint opType;
    switch (k)
      {
        case 1:
          MFk = du . MF1 = bitfieldExtract36 (cu . IWB,  0, 7);
          opType = ((ci -> info -> flags) & EOP1_MASK) >> EOP1_SHIFT;
          break;
        case 2:
          MFk = du . MF2 = bitfieldExtract36 (cu . IWB, 18, 7);
          opType = ((ci -> info -> flags) & EOP2_MASK) >> EOP2_SHIFT;
          break;
        case 3:
          MFk = du . MF3 = bitfieldExtract36 (cu . IWB, 27, 7);
          opType = ((ci -> info -> flags) & EOP3_MASK) >> EOP3_SHIFT;
          break;
      }
    du . MF [k - 1] = MFk;

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "mySetupOperandDesc k %u MFk %o\n", k, MFk);

    word36 operandDesc;

    // CANFAULT
    Read (PPR . IC + k, & operandDesc, OPERAND_READ, false);

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "mySetupOperandDesc k %u 1: opd %012llo\n",
               k, operandDesc);

    du . Dk_PTR_W [k - 1] = GETHI (operandDesc);

// operandDesc is either the operand descriptor, or the address of it.

    // operandDesc is either the operand descriptor, or a pointer to it

// Numbers are from RJ78, Figure 5-2.


    word18 address = GETHI (operandDesc);
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "mySetupOperandDesc k %u 1: address %06o\n",
               k, address);

    TPR . TSR = PPR . PSR;
    TPR . TRR = PPR . PRR;

    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;

    // 2

    if (MFk & MFkID)
      {
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "mySetupOperandDesc k %u 2: MFkID\n", k);

        // Indirect descriptor control. If ID = 1 for Mfk, then the kth word
        // following the instruction word is an indirect pointer to the operand
        // descriptor for the kth operand; otherwise, that word is the operand
        // descriptor.

        // If MFk.ID = 1, then the kth word following an EIS multiword
        // instruction word is not an operand descriptor, but is an indirect
        // pointer to an operand descriptor and is interpreted as shown in
        // Figure 4-5.

        bool a = operandDesc & (1 << 6); 
        
        if (a)  // 3
          {
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "mySetupOperandDesc k %u 3: a\n", k);

            // A 3-bit pointer register number (n) and a 15-bit offset 
            // relative to C (ARn . WORDNO)

            // 4

            word3 arn = GET_PRN (operandDesc);
            word15 offset = GET_OFFSET (operandDesc);
            address = (AR [arn] . WORDNO + SIGNEXT15 (offset)) & AMASK;

            if (get_addr_mode () == APPEND_mode)
              {
                TPR . TSR = AR [arn] . SNR;
                TPR . TRR = max3 (AR [arn] . RNR, TPR . TRR, PPR . PRR);
              }

            ARn_CHAR = GET_AR_CHAR (arn); // AR[n].CHAR;
            ARn_BITNO = GET_AR_BITNO (arn); // AR[n].BITNO;

            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "mySetupOperandDesc k %u 4: arn %u offset %05u address %06u\n",
              k, arn, offset, address);
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "mySetupOperandDesc k %u 4: TSR %u TRR %u CHAR %u BITNO %u\n",
              k, TPR . TSR, TPR . TRR, ARn_CHAR, ARn_BITNO);
          }

        // Address modifier for ADDRESS. All register modifiers except du and
        // dl may be used. If the ic modifier is used, then ADDRESS is an
        // 18-bit offset relative to value of the instruction counter for the
        // instruction word. C(REG) is always interpreted as a word offset. 

        // 5, 6

        word4 reg = GET_TD (operandDesc);

        sim_debug (DBG_TRACEEXT, & cpu_dev,
              "mySetupOperandDesc k %u 5: reg %u getMFReg18 %llu\n",
              k, reg, getMFReg18 (reg, false));

        address += getMFReg18 (reg, false);
        address &= AMASK;

        sim_debug (DBG_TRACEEXT, & cpu_dev,
              "mySetupOperandDesc k %u 6: addr %08u\n",
              k, address);

        // 7

        // CANFAULT
        Read (address, & operandDesc, EIS_OPERAND_READ, a);

        sim_debug (DBG_TRACEEXT, & cpu_dev,
               "mySetupOperandDesc k %u 7: opd %012llo\n",
               k, operandDesc);
      }

// We now have the actual operand descriptor.

    // 8

    if (MFk & MFkAR)
      {

        sim_debug (DBG_TRACEEXT, & cpu_dev,
               "mySetupOperandDesc k %u 8: MFkAR\n", k);

        // 9

        word3 arn = (address >> 15) & MASK3;
        word15 offset = address & MASK15;
        address = (AR [arn] . WORDNO + SIGNEXT15 (offset)) & AMASK;

#if 0 // we are not actually going to do the read
        if (get_addr_mode () == APPEND_mode)
          {
            TPR . TSR = AR [arn] . SNR;
            TPR . TRR = max3 (AR [arn] . RNR, TPR . TRR, PPR . PRR);
          }
#endif
        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "mySetupOperandDesc k %u 9: arn %u offset %05u address %06u\n",
          k, arn, offset, address);
      }

    //  10, 11

    int reg = MFk & MFkREGMASK;

    sim_debug (DBG_TRACEEXT, & cpu_dev,
          "mySetupOperandDesc k %u 10: reg %u getMFReg18 %llu\n",
          k, reg, getMFReg18 (reg, false));

    address += getMFReg18 (reg, false);
    address &= AMASK;

    sim_debug (DBG_TRACEEXT, & cpu_dev,
        "mySetupOperandDesc k %u 11: addr %08u\n",
        k, address);

    du . TAk [k - 1] = bitfieldExtract36 (operandDesc, 13, 2);

    sim_debug (DBG_TRACEEXT, & cpu_dev,
        "mySetupOperandDesc k %u 12: TAk %u\n",
        k, du . TAk [k - 1]);

    //du . Dk_PTR_B [k - 1] = bitfieldExtract36 (operandDesc, 15, 3); // CNk
    if (MFk & MFkRL)
      {
        uint reg = operandDesc & MFkREGMASK;

        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "mySetupOperandDesc k %u 13: reg %u getMFReg36 %llu\n",
          k, reg, getMFReg36 (reg, false));

        du . Dk_RES [k - 1] = getMFReg36 (reg, false);

        switch (du . TAk [k - 1])
          {
            case CTA4:
              du . Dk_RES [k - 1] &= 017777777; ///< 22-bits of length
              break;

            case CTA6:
            case CTA9:
              du . Dk_RES [k - 1] &= 07777777;  ///< 21-bits of length.
              break;

            default:
              sim_printf ("mySetupOperandDesc (ta=%d) How'd we get here 1?\n", du . TAk [k - 1]);
              break;
          }
      }
    else
      du . Dk_RES [k - 1] = operandDesc & 07777;


    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "mySetupOperandDesc k %u 14: Dk_RES %u\n",
      k, du . Dk_RES [k - 1]);

    du . Fk [k - 1] = 1; // First time
    du . Ak [k - 1] = 1; // Active

//
// cmpc
//   descna  Y-charn1[(CN1)],N1
//   descna  Y-charn2[(CN2)],N2 (TA2 ignored)
//
// scd, scdr
//   descna  Y-charn1[(CN1)],N1
//   descna  Y-charn2[(CN2)],N2 (TA2 ignored)
//   arg     Y3[,tag]
//
// scm, scmr
//   descna  Y-charn1[(CN1)],N1
//   descna  Y-charn2[(CN2)]    (TA2, N  ignored)
//   arg     Y3[,tag]
//
// tct, tctr
//   descna  Y-charn1[(CN1)],N1
//   arg     Ycharn92[,tag]
//   arg     Y3[,tag]
//
// mlr, mrl
//   descna  Y-charn1[(CN1)],N1
//   descna  Y-charn2[(CN2)],N2
//
// mve
//   descna  Y-charn1[(CN1)],N1
//   desc9a  Y-char92[(CN2)],N2 (n must be 9)
//   descna  Y-charn3[(CN3)],N3
//
// mvt
//   descna  Y-charn1[(CN1)],N1
//   descna  Y-charn2[(CN2)],N2
//   arg     Y3[,tag]
//
// cmpn, mvn
//   descn[fl,ls,ns,ts]  Y-charn1[(CN1)],N1,SF1   n=4 or 9)
//   descn[fl,ls,ns,ts]  Y-charn2[(CN2)],N2,SF2   n=4 or 9)
//
// mvne
//   descn[fl,ls,ns,ts]  Y-charn1[(CN1)],N1,SF1   n=4 or 9)
//   desc9a  Y-char92[(CN2)],N2 (n must be 9)
//   descna  Y-charn3[(CN3)],N3
//
// csl, csr, cmb, sztl, sztr, btd
//   descb  Y-bit1[(BITNO1)],N1
//   descb  Y-bit2[(BITNO2)],N2
//   descn[fl,ls,ns,ts]  Y-charnk[(CNk],Nk,SFk   n=4 or 9)
//
// btd
//   desc9a  Y-char91[(CN1)],N1 (n must be 9)
//   descn[fl,ls,ns,ts]  Y-charn2[(CN2],N2,SF2   n=4 or 9)
//     (btd forbids fl, but can handle that as IPR fault in instruction)
//
// dtb
//   descn[fl,ls,ns,ts]  Y-charn1[(CN1],N1,SF1   n=4 or 9)
//   desc9a  Y-char92[(CN2)],N2 (n must be 9)
//     (dtb forbids fl, but can handle that as IPR fault in instruction)
//
// ad2d, sb2d, mp2d, dv2d
//   descn[fl,ls,ns,ts]  Y-charn1[(CN1],N1,SF1   n=4 or 9)
//   descn[fl,ls,ns,ts]  Y-charn2[(CN2],N2,SF2   n=4 or 9)
//
// ad3d, mp3d, dv3d
//   descn[fl,ls,ns,ts]  Y-charn1[(CN1],N1,SF1   n=4 or 9)
//   descn[fl,ls,ns,ts]  Y-charn2[(CN2],N2,SF2   n=4 or 9)
//   descn[fl,ls,ns,ts]  Y-charn3[(CN3],N3,SF3   n=4 or 9)
//
// Special cases
//
//   BTD and DTB forbid DESCNFL, but that does affect operand
//   parsing, and can be handled with an IPR fault in the instruction.
//
//   CMPC, SCD, SCDR, SCM and SCMR  use the value of TA1 for TA2.
//
//   SCD and SCDR ignore N2; I think that this does not require special 
//   handling here.
//   
//   descna  Y-charnk[(CNk],Nk
//   descna  Y-charnk[(CNk],Nk (TA2 ignored)
//   desc9a  Y-char9k[(CNk],Nk (n must be 9)
//   descna  Y-charnk[(CNk]    (TA2, N ignored)
//   arg     Yk[,tag]
//   descn[fl,ls,ns,ts]  Y-charnk[(CNk],Nk,SFk   n=4 or 9)
//   descb  Y-bitk[(BITNOk)],Nk

    // operand type specific processing:
    //
    //   ALPHA:    "Operand Descriptor Form" ADDRESS, CN, TA, N
    //
    //             "Pointer Register Form" WORDNO, BITNO, TAG
    //             "Address Register Form" WORDNO, CHAR, BITNO, TAG
    //   Anotag    Same as alpha, but TA is taken from TA1, and N is 
    //               taken from N1
    //   WORDP:    AR (or PR) with WORDNO/BIT 0
    //   NUMERIC:  ADDRESS, CN, TN, S, SF, N
    //   BIT:      ADDRESS, C, B, N
// XXX btd op1 must be Y-char9; but there is no definition of
// what happens if is different.

// temp  while buggy
TPR . TSR = saveTSR;
TPR . TRR = saveTRR;
  }

// CANFAULT
void doEIS_CAF (void)
  {
    DCDstruct * ci = & currentInstruction;

    int ndes = ci -> info -> ndes;
    if (ndes != 2 && ndes != 3)
      {
        sim_printf ("Dazed and confused; ndes = %d\n", ndes);
        return;
      }

    mySetupOperandDesc (1);
    mySetupOperandDesc (2);
    if (ndes == 3)
      mySetupOperandDesc (3);
  }
#endif

// CANFAULT
static void EISWrite(EISaddr *p, word36 data)
{
    if (p->mat == viaPR)
    {
        TPR.TRR = p->RNR;
        TPR.TSR = p->SNR;
        
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: write %012llo@%o:%06o\n", __func__, data, p -> SNR, p -> address);
        Write (p->address, data, EIS_OPERAND_STORE, true); // write data
    }
    else
    {
        if (get_addr_mode() == APPEND_mode)
        {
            TPR.TRR = PPR.PRR;
            TPR.TSR = PPR.PSR;
        }
        
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: write %012llo@%o:%06o\n", __func__, data, TPR . TSR, p -> address);
        Write (p->address, data, EIS_OPERAND_STORE, false); // write data
    }
}

#ifdef DBGF
static void myEISDevelop (uint k, uint opType, uint pos,
                          uint * addr, bool * ind)
  {
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "%s k %u opType %u pos %u\n", __func__, k, opType, pos);

    word36 operandDesc;

    TPR . TSR = PPR . PSR;
    TPR . TRR = PPR . PRR;
    // CANFAULT
    Read (PPR . IC + k, & operandDesc, OPERAND_READ, false);
// Numbers are from RJ78, Figure 5-2.

    word7 MFk = du . MF [k - 1];

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "%s k %u MFk %o\n", __func__, k, MFk);

    word18 address = du . Dk_PTR_W [k - 1];

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "%s k %u 1: address %06o\n",
               __func__, k, address);

    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;

    if (MFk & MFkID)  // 2
      {
        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "%s k %u 2: MFkID\n", __func__, k);

        bool a = operandDesc & (1 << 6); 
        if (a) // 3
          {
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "%s k %u 3: a\n", __func__, k);

            // If MKf contains AR then it Means Y-charn is not the memory 
            // address of the data but is a reference to a pointer register 
            // pointing to the data.

            // 4

            word3 arn = (address >> 15) & MASK3;
            word15 offset = address & MASK15;
            address = (AR [arn] . WORDNO + SIGNEXT15 (offset)) & AMASK;

            if (get_addr_mode () == APPEND_mode)
              {
                TPR . TSR = AR [arn] . SNR;
                TPR . TRR = max3 (AR [arn] . RNR, TPR . TRR, PPR . PRR);
              }
            ARn_CHAR = GET_AR_CHAR (arn); // AR[n].CHAR;
            ARn_BITNO = GET_AR_BITNO (arn); // AR[n].BITNO;

            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "%s k %u 4: arn %u offset %05u address %06u\n",
              __func__, k, arn, offset, address);
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "%s k %u 4: TSR %u TRR %u CHAR %u BITNO %u\n",
              __func__, k, TPR . TSR, TPR . TRR, ARn_CHAR, ARn_BITNO);
          }


        // 5, 6

        int reg = MFk & MFkREGMASK;

        sim_debug (DBG_TRACEEXT, & cpu_dev,
              "%s k %u 5: reg %u getMFReg18 %llu\n",
              __func__, k, reg, getMFReg18 (reg, false));

        address += getMFReg18 (reg, false);
        address &= AMASK;

        sim_debug (DBG_TRACEEXT, & cpu_dev,
              "%s k %u 6: addr %08u\n",
              __func__, k, address);

        // 7

        Read (address, & operandDesc, EIS_OPERAND_READ, a);  // read operand
        sim_debug (DBG_TRACEEXT, & cpu_dev,
               "%s k %u 7: opd %012llo\n",
               __func__, k, operandDesc);
      }

    // 8

    bool a = MFk & MFkAR;
    if (a)
      {

        sim_debug (DBG_TRACEEXT, & cpu_dev,
               "%s k %u 8: MFkAR\n", __func__, k);

        // 9

        word3 arn = (address >> 15) & MASK3;
        word15 offset = address & MASK15;
        address = (AR [arn] . WORDNO + SIGNEXT15 (offset)) & AMASK;

        if (get_addr_mode () == APPEND_mode)
          {
            TPR . TSR = AR [arn] . SNR;
            TPR . TRR = max3 (AR [arn] . RNR, TPR . TRR, PPR . PRR);
          }

        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "%s k %u 9: arn %u offset %05u address %06u\n",
          __func__, k, arn, offset, address);
        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "%s k %u 9: TSR %o TSR %o\n",
          __func__, k, TPR . TSR, TPR . TRR);
      }

    //  10, 11

    int reg = MFk & MFkREGMASK;

    sim_debug (DBG_TRACEEXT, & cpu_dev,
          "%s k %u 10: reg %u getMFReg18 %llu\n",
          __func__, k, reg, getMFReg18 (reg, false));

    address += getMFReg18 (reg, false);
    address &= AMASK;

    sim_debug (DBG_TRACEEXT, & cpu_dev,
        "%s k %u 11: addr %08u\n",
        __func__, k, address);

    sim_debug (DBG_TRACEEXT, & cpu_dev,
        "%s k %u 12: TAk %u\n",
        __func__, k, du . TAk [k - 1]);

    uint effBITNO;
    uint effCHAR;
    uint effWORDNO;
  
    if (opType == EOP_ALPHA) // alphanumeric
      {
        // Use TA from du; instruction may have overridden
        // du . TAk [k - 1] = bitfieldExtract36 (operandDesc, 13, 2);
        uint CN = bitfieldExtract36 (operandDesc, 15, 3); // CNk
sim_debug (DBG_TRACEEXT, & cpu_dev, "new CN%u %u\n", k, CN);
        uint res;
        if (MFk & MFkRL)
          {
            uint reg = operandDesc & MFkREGMASK;

            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "%s k %u 13: reg %u getMFReg36 %llu\n",
              __func__, k, reg, getMFReg36 (reg, false));

            if (reg == 04u) // IC
              {
                res = 0;
              }
            else
              {
                res = getMFReg36 (reg, false);
                switch (du . TAk [k - 1])
                  {
                    case CTA4:
                      res &= 017777777; ///< 22-bits of length
                      break;
    
                    case CTA6:
                    case CTA9:
                      res &= 07777777;  ///< 21-bits of length.
                      break;
    
                    default:
                      sim_printf ("parseAlphanumericOperandDescriptor(ta=%d) How'd we get here 1?\n", du . TAk [k - 1]);
                      break;
                  }
              }
          }
        else
          res = operandDesc & 07777;

// The instruction may have over ridden the type; recalculate Dk_RES if the
// first time myEISRead called

        if (du . Fk [k - 1]) // If first time
          du . Dk_RES [k - 1] = res;

        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "%s k %u 14: Dk_RES %u\n",
          __func__, k, du . Dk_RES [k - 1]);

        word36 r = getMFReg36 (MFk & 017, false);
    
        // AL-39 implies, and RJ-76 say that RL and reg == IC is
        // illegal; but it the emulator ignores RL if reg == IC,
        // then that PL/I generated code in Multics works. 
        // "Pragmatic debugging."

        if (/*!(MFk & MFkRL) && */ (MFk & 017) == 4) // reg == IC ?
          {
sim_debug (DBG_TRACEEXT, & cpu_dev, "wierd case k %u r %u\n", k, r);
sim_printf ("wierd case k %u r %u\n", k, r);
            // The ic modifier is permitted in MFk.REG and 
            // C (od)32,35 only if MFk.RL = 0, that is, if the 
            // contents of the register is an address offset, 
            // not the designation of a register containing the 
            // operand length.
            address += r;
            r = 0;
          }

//sim_debug (DBG_TRACE, & cpu_dev, "CAC second address [%d] %06o\n", k, address);

        // If seems that the effect address calcs given in AL39 p.6-27 are 
        // not quite right.
        // E.g. For CTA4/CTN4 because of the 4 "slop" bits you need to do 
        // 32-bit calcs not 36-bit!

sim_debug (DBG_TRACEEXT, & cpu_dev, "address %o ARn_CHAR %u pos %u r %u ARn_BITNO %u CN %u\n",
 address, ARn_CHAR, pos, r, ARn_BITNO, CN);
        ARn_CHAR += pos;
        switch (du . TAk [k - 1])
          {
            case CTA4:
              effBITNO = 4 * (ARn_CHAR + 2*r + ARn_BITNO/4) % 2 + 1;
              effCHAR = ((4 * CN + 9 * ARn_CHAR + 4 * r + ARn_BITNO) % 32) / 4;
              effWORDNO = address + 
                          (4 * CN + 9 * ARn_CHAR + 4 * r + ARn_BITNO) / 32;
              effWORDNO &= AMASK;
            
              break;
            case CTA6:
              effBITNO = (6 * ARn_CHAR + 6 * r + ARn_BITNO) % 6;
              effCHAR = ((6 * CN + 6 * ARn_CHAR + 6 * r + ARn_BITNO) % 36) / 6;
              effWORDNO = address + 
                          (6 * CN + 6 * ARn_CHAR + 6 * r + ARn_BITNO) / 36;
//sim_printf ("delta %u\n", (6 * CN + 6 * ARn_CHAR + 6 * r + ARn_BITNO) / 36);
              effWORDNO &= AMASK;
              break;
            case CTA9:
              CN = (CN >> 1) & 03;  // XXX Do error checking
              effBITNO = (9 * ARn_CHAR + 9 * r + ARn_BITNO) % 9;
              effCHAR = ((9 * CN + 9 * ARn_CHAR + 9 * r + ARn_BITNO) % 36) / 9;
              effWORDNO = address + 
                          (9 * CN + 9 * ARn_CHAR + 9 * r + ARn_BITNO) / 36;
//sim_printf ("delta %u\n", (6 * CN + 6 * ARn_CHAR + 6 * r + ARn_BITNO) / 36);
              effWORDNO &= AMASK;
              break;
          }
        du . Dk_PTR_B [k - 1] = effCHAR;
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s D%u_PTR_B %u\n",
           __func__, k, effCHAR);
      }
    else
      {
sim_printf ("%s crash and burn\n", __func__);
exit (1);
      }

    du . Fk [k - 1] = false; // No longer first time

    //sim_printf ("operand %08o:%012llo\n", address, data);
    * addr = effWORDNO;
    * ind = a;
  }

static void myEISParse (uint k, uint opType)
  {
  
// temp while buggy
word15 saveTSR = TPR . TSR;
word3 saveTRR = TPR . TRR;
//sim_debug (DBG_TRACEEXT, & cpu_dev, "myEISParse k %u type %d\n", k, opType);
    uint addr;
    bool ind;
    myEISDevelop (k, opType, 0, & addr, & ind);
// temp  while buggy
TPR . TSR = saveTSR;
TPR . TRR = saveTRR;
  }

static word36 myEISRead (uint k, uint opType, uint pos)
  {
// temp while buggy
word15 saveTSR = TPR . TSR;
word3 saveTRR = TPR . TRR;
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "myEISRead k %u opType %u pos %u\n", k, opType, pos);
    uint effWORDNO;
    bool a;
    myEISDevelop (k, opType, pos, & effWORDNO, & a);

    word36 data;

    Read (effWORDNO, & data, EIS_OPERAND_READ, a);  // read operand
    sim_debug (DBG_TRACEEXT, & cpu_dev, "myEISRead: read %012llo@%o:%06o\n",
               data, TPR . TSR, effWORDNO);

    //sim_printf ("operand %08o:%012llo\n", address, data);
dbgAddr1 = effWORDNO;
dbgData1 = data;

// temp  while buggy
TPR . TSR = saveTSR;
TPR . TRR = saveTRR;
    return data;
  }

static uint myEISget469 (uint k, uint pos)
  {
    uint maxPos = 4u; // CTA9
    switch (du . TAk [k - 1])
      {
        case CTA4:
          maxPos = 8u;
          break;
            
        case CTA6:
          maxPos = 6u;
          break;
      }
    

    word36 data = myEISRead (k, EOP_ALPHA, pos);

    uint c = 0;
    uint coffset = du . Dk_PTR_B [k - 1]; // CN
    sim_debug (DBG_TRACEEXT, & cpu_dev, "myEISRead468 %u coffset %u\n",
           k, coffset);
sim_debug (DBG_TRACEEXT, & cpu_dev, "myEISRead %u coffset %u\n", k, coffset);

    switch (du . TAk [k - 1])
      {
        case CTA4:
          c = get4 (data, coffset);
          break;
        case CTA6:
          c = get6 (data, coffset);
          break;
        case CTA9:
          c = get9 (data, coffset);
          break;
       }
//sim_printf ("new: k %u TAk %u pos %d coffset %u c %o \n", k, du . TAk [k - 1], pos, coffset, c);
    
    return c;
  }
#endif

// CANFAULT
static word36 EISRead(EISaddr *p)
{
    word36 data;
    if (p->mat == viaPR)    //&& get_addr_mode() == APPEND_mode)
    {
        TPR.TRR = p->RNR;
        TPR.TSR = p->SNR;
        
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read %o:%06o\n", __func__, TPR . TSR, p -> address);
        Read (p->address, &data, EIS_OPERAND_READ, true);     // read data via AR/PR. TPR.{TRR,TSR} already set up
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read* %012llo@%o:%06o\n", __func__, data, TPR . TSR, p -> address);
    }
    else
    {
        if (get_addr_mode() == APPEND_mode)
        {
            TPR.TRR = PPR.PRR;
            TPR.TSR = PPR.PSR;
        }
        
        Read (p->address, &data, EIS_OPERAND_READ, false);  // read operand
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read %012llo@%o:%06o\n", __func__, data, TPR . TSR, p -> address);
    }
    return data;
}

// CANFAULT
static void EISReadN(EISaddr *p, int N, word36 *dst)
{
    for(int n = 0 ; n < N ; n++)
    {
        *dst++ = EISRead(p);
        p->address += 1;
        p->address &= AMASK;
    }
}


// getMFReg
//  RType reflects the AL-39 R-type and C(op. desc.)32,35 columns
//
//  Table 4-1. R-type Modifiers for REG Fields
//  
//                   Meaning as used in:
//
//  Octal  R-type  MF.REG   Indirect operand    C(operand descriptor)32,35
//  Code                    decriptor-pointer
//  00         n       n          n                      IPR
//  01        au      au          au                      au
//  02        qu      qu          qu                      qu
//  03        du     IPR         IPR                      du (a)
//  04        ic      ic          ic                      ic (b)
//  05        al       a (c)      al                       a (c)
//  06        ql       q (c)      ql                       a (c)
//  07        dl     IPR         IPR                     IPR
//  1n        xn      xn          xn                      xn
//

static word18 getMFReg18 (uint n, bool allowDUL)
  {
    switch (n)
      {
        case 0: // n
            return 0;
        case 1: // au
            return GETHI(rA);
        case 2: // qu
            return GETHI(rQ);
        case 3: // du
            // du is a special case for SCD, SCDR, SCM, and SCMR
// XXX needs attention; doesn't work with old code; triggered by
// XXX parseOperandDescriptor;
           // if (! allowDUL)
             //doFault (illproc_fault, ill_proc, "getMFReg18 du");
            return 0;
        case 4: // ic - The ic modifier is permitted in MFk.REG and 
                // C (od)32,35 only if MFk.RL = 0, that is, if the contents of 
                // the register is an address offset, not the designation of 
                // a register containing the operand length.
            return PPR.IC;
        case 5: ///< al / a
            return GETLO(rA);
        case 6: ///< ql / a
            return GETLO(rQ);
        case 7: ///< dl
             doFault (illproc_fault, ill_proc, "getMFReg18 dl");
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            return rX [n - 8];
      }
    sim_printf ("getMFReg18(): How'd we get here? n=%d\n", n);
    return 0;
  }


static word36 getMFReg36 (uint n, bool allowDUL)
  {
    switch (n)
      {
        case 0: // n
            return 0;
        case 1: // au
            return GETHI(rA);
        case 2: // qu
            return GETHI(rQ);
        case 3: // du
            // du is a special case for SCD, SCDR, SCM, and SCMR
// XXX needs attention; doesn't work with old code; triggered by
// XXX parseOperandDescriptor;
           // if (! allowDUL)
             //doFault (illproc_fault, ill_proc, "getMFReg36 du");
            return 0;
        case 4: // ic - The ic modifier is permitted in MFk.REG and 
                // C (od)32,35 only if MFk.RL = 0, that is, if the contents of 
                // the register is an address offset, not the designation of 
                // a register containing the operand length.
            return PPR.IC;
        case 5: ///< al / a
            return rA;
        case 6: ///< ql / a
            return rQ;
        case 7: ///< dl
             doFault (illproc_fault, ill_proc, "getMFReg36 dl");
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            return rX [n - 8];
      }
    sim_printf ("getMFReg36(): How'd we get here? n=%d\n", n);
    return 0;
  }



//
// 5.2.10.5  Operand Descriptor Address Preparation Flowchart
//
// A flowchart of the operations involved in operand descriptor address
// preparation is shown in Figure 5-2. The chart depicts the address
// preparation for operand descriptor 1 of a multiword instruction as described
// by modification field 1 (MF1). A similar type address preparation would be
// carried out for each operand descriptor as specified by its MF code.
//    (Bull Nova 9000 pg 5-40  67 A2 RJ78 REV02)
//
// 1. The multiword instruction is obtained from memory.
//
// 2. The indirect (ID) bit of MF1 is queried to determine if the descriptor
// for operand 1 is present or is an indirect word.
//
// 3. This step is reached only if an indirect word was in the operand
// descriptor location. Address modification for the indirect word is now
// performed. If the AR bit of the indirect word is 1, address register
// modification step 4 is performed.
//
// 4. The y field of the indirect word is added to the contents of the
// specified address register.
//
// 5. A check is now made to determine if the REG field of the indirect word
// specifies that a register type modification be performed.
//
// 6. The indirect address as modified by the address register is now modified
// by the contents of the specified register, producing the effective address
// of the operand descriptor.
//
// 7. The operand descriptor is obtained from the location determined by the
// generated effective address in item 6.
//
// 8. Modification of the operand descriptor address begins. This step is
// reached directly from 2 if no indirection is involved. The AR bit of MF1 is
// checked to determine if address register modification is specified.
//
// 9. Address register modification is performed on the operand descriptor as
// described under "Address Modification with Address Registers" above. The
// character and bit positions of the specified address register are used in
// one of two ways, depending on the type of operand descriptor, i.e., whether
// the type is a bit string, a numeric, or an alphanumeric descriptor.
//
// 10. The REG field of MF1 is checked for a legal code. If DU is specified in
// the REG field of MF2 in one of the four multiword instructions (SCD, SCDR,
// SCM, or SCMR) for which DU is legal, the CN field is ignored and the
// character or characters are arranged within the 18 bits of the word address
// portion of the operand descriptor.
//
// 11. The count contained in the register specified by the REG field code is
// appropriately converted and added to the operand address.
//
// 12. The operand is retrieved from the calculated effective address location.
//

// prepare MFk operand descriptor for use by EIS instruction ....

// CANFAULT
void setupOperandDescriptor(int k, EISstruct *e)
{
    switch (k)
    {
        case 1:
            e->MF1 = (int)bitfieldExtract36(e->op0,  0, 7);  ///< Modification field for operand descriptor 1
            break;
        case 2:
            e->MF2 = (int)bitfieldExtract36(e->op0, 18, 7);  ///< Modification field for operand descriptor 2
            break;
        case 3:
            e->MF3 = (int)bitfieldExtract36(e->op0, 27, 7);  ///< Modification field for operand descriptor 3
            break;
    }
    
    word18 MFk = e->MF[k-1];
    
    if (MFk & MFkID)
    {
        word36 opDesc = e->op[k-1];
        
        // fill operand according to MFk....
        word18 address = GETHI(opDesc);
        e->addr[k-1].address = address;
        
	// Indirect descriptor control. If ID = 1 for Mfk, then the kth word
	// following the instruction word is an indirect pointer to the operand
	// descriptor for the kth operand; otherwise, that word is the operand
	// descriptor.
        //
	// If MFk.ID = 1, then the kth word following an EIS multiword
	// instruction word is not an operand descriptor, but is an indirect
	// pointer to an operand descriptor and is interpreted as shown in
	// Figure 4-5.
        
        
        // Mike Mondy michael.mondy@coffeebird.net sez' ...
        // EIS indirect pointers to operand descriptors use PR registers.
        // However, operand descriptors use AR registers according to the
        // description of the AR registers and the description of EIS operand
        // descriptors. However, the description of the MF field
        // claims that operands use PR registers. The AR doesn't have a
        // segment field. Emulation confirms that operand descriptors
        // need to be fetched via segments given in PR registers.

        bool a = opDesc & (1 << 6); 
        
        if (a)
        {
	    // A 3-bit pointer register number (n) and a 15-bit offset relative
	    // to C(PRn.WORDNO) if A = 1 (all modes)
            uint n = bitfieldExtract36(address, 15, 3);
            word15 offset = address & MASK15;  // 15-bit signed number
            address = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;

            e->addr[k-1].address = address;
            if (get_addr_mode() == APPEND_mode)
            {
                e->addr[k-1].SNR = PR[n].SNR;
                e->addr[k-1].RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
                
                e->addr[k-1].mat = viaPR;   // ARs involved
            }
        } else
            //e->addr[k-1].mat = IndirectRead;      // no ARs involved yet
            e->addr[k-1].mat = OperandRead;      // no ARs involved yet

        
	// Address modifier for ADDRESS. All register modifiers except du and
	// dl may be used. If the ic modifier is used, then ADDRESS is an
	// 18-bit offset relative to value of the instruction counter for the
	// instruction word. C(REG) is always interpreted as a word offset. REG 
        uint reg = opDesc & 017;
        address += getMFReg18(reg, false);
        address &= AMASK;
        
        e->addr[k-1].address = address;
        
        e->op[k-1] = EISRead(&e->addr[k-1]);  // read EIS operand .. this should be an indirectread
    }
}

static void parseAlphanumericOperandDescriptor(uint k, EISstruct *e, uint useTA)
{
    word18 MFk = e->MF[k-1];
    
    word36 opDesc = e->op[k-1];
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    
    word18 address = GETHI (opDesc);
    
    if (useTA != k)
      e -> TA [k - 1] = e -> TA [useTA - 1];
    else
      e -> TA [k - 1] = (int)bitfieldExtract36(opDesc, 13, 2);    // type alphanumeric

    if (MFk & MFkAR)
    {
	// if MKf contains ar then it Means Y-charn is not the memory address
	// of the data but is a reference to a pointer register pointing to the
	// data.
        uint n = bitfieldExtract36(address, 15, 3);
        word18 offset = SIGNEXT15 (address);  // 15-bit signed number
        address = (AR [n] . WORDNO + offset) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
        {
            e->addr[k-1].SNR = PR[n].SNR;
            e->addr[k-1].RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);

            e->addr[k-1].mat = viaPR;   // ARs involved
        }
    }

    uint CN = bitfieldExtract36(opDesc, 15, 3);    ///< character number

    sim_debug (DBG_TRACEEXT, & cpu_dev, "initial CN%u %u\n", k, CN);
    
    if (MFk & MFkRL)
    {
        uint reg = opDesc & 017;
        e->N[k-1] = getMFReg36(reg, false);
        switch (e->TA[k-1])
        {
            case CTA4:
//sim_printf ("CTA4\n");
                e->N[k-1] &= 017777777; ///< 22-bits of length
                break;
            case CTA6:
//sim_printf ("CTA6\n");
            case CTA9:
                e->N[k-1] &= 07777777;  ///< 21-bits of length.
                break;
            default:
                sim_printf ("parseAlphanumericOperandDescriptor(ta=%d) How'd we get here 1?\n", e->TA[k-1]);
                break;
        }
    }
    else
        e->N[k-1] = opDesc & 07777;
    
sim_debug (DBG_TRACEEXT, & cpu_dev, "N%u %u\n", k, e->N[k-1]);
    word36 r = getMFReg36 (MFk & 017, false);
    
//sim_debug (DBG_TRACEEXT, & cpu_dev, "reg offset %d\n", r);

    // AL-39 implies, and RJ-76 say that RL and reg == IC is illegal;
    // but it the emulator ignores RL if reg == IC, then that PL/I
    // generated code in Multics works. "Pragmatic debugging."

    if (/*!(MFk & MFkRL) && */ (MFk & 017) == 4)   // reg == IC ?
    {
        //The ic modifier is permitted in MFk.REG and C (od)32,35 only if MFk.RL = 0, that is, if the contents of the register is an address offset, not the designation of a register containing the operand length.
        address += r;
        address &= AMASK;
        r = 0;
    }

#if 0
// The character count contained in the register is divided by 4, 6, or 8
// (depending upon the data type), which gives a word count with a character
// remainder. The word and character counts are then appropriately arranged in
// 21 bits (18-word address and 3 for character position) and added to the
// modified descriptor operand address. The appropriate carries occur from the
// character positions to the word when the summed character counts exceed the
// number of characters in a 36-bit word. When the A- or Q-registers are
// specified, large counts can cause the result of the division to be greater
// than 2**18-1, which is interpreted modulo 2**18, the same as for bit
// addressing.

    uint nCharsWord = 4u;
    switch (e->TA[k-1])
      {
        case CTA4: nCharsWord = 8u; break;
        case CTA6: nCharsWord = 6u; break;
        case CTA9: nCharsWord = 4u; break;
      }

    uint wordCnt = r / nCharsWord;
    wordCnt &= AMASK;
    uint charCnt = r % nCharsWord;

    ARn_CHAR += charCnt + CN;
    address += wordCnt;

    while (ARn_CHAR > nCharsWord)
      {
        ARn_CHAR -= nCharsWord;
        address += 1;
      }

    address &= AMASK;

    e -> effBITNO = ARn_BITNO;
    e -> effCHAR = ARn_CHAR;
    e -> effWORDNO = address;
    e -> CN [k - 1] = e -> effCHAR;
  
#else
    // If seems that the effect address calcs given in AL39 p.6-27 are not quite right.
    // E.g. For CTA4/CTN4 because of the 4 "slop" bits you need to do 32-bit calcs not 36-bit!
    switch (e->TA[k-1])
    {
        case CTA4:
            e->effBITNO = 4 * (ARn_CHAR + 2*r + ARn_BITNO/4) % 2 + 1;   // XXX Check
            e->effCHAR = ((4*CN + 9*ARn_CHAR + 4*r + ARn_BITNO) % 32) / 4;  //9;36) / 4;  //9;
            e->effWORDNO = address + (4*CN + 9*ARn_CHAR + 4*r + ARn_BITNO) / 32;    // 36
            e->effWORDNO &= AMASK;
            
            //e->YChar4[k-1] = e->effWORDNO;
            //e->CN[k-1] = CN;    //e->effCHAR;
            e->CN[k-1] = e->effCHAR;
            break;
        case CTA6:
            e->effBITNO = (9*ARn_CHAR + 6*r + ARn_BITNO) % 9;
            e->effCHAR = ((6*CN + 9*ARn_CHAR + 6*r + ARn_BITNO) % 36) / 6;//9;
            e->effWORDNO = address + (6*CN + 9*ARn_CHAR + 6*r + ARn_BITNO) / 36;
            e->effWORDNO &= AMASK;
            
            //e->YChar6[k-1] = e->effWORDNO;
            e->CN[k-1] = e->effCHAR;   // ??????
            break;
        case CTA9:
            CN = (CN >> 1) & 07;  // XXX Do error checking
            
            e->effBITNO = 0;
            e->effCHAR = (CN + ARn_CHAR + r) % 4;
            e->effWORDNO = address + ((9*CN + 9*ARn_CHAR + 9*r + ARn_BITNO) / 36);
            e->effWORDNO &= AMASK;
            
            //e->YChar9[k-1] = e->effWORDNO;
            e->CN[k-1] = e->effCHAR;   // ??????
            break;
        default:
            sim_printf ("parseAlphanumericOperandDescriptor(ta=%d) How'd we get here 2?\n", e->TA[k-1]);
            break;
    }
#endif
    
    EISaddr *a = &e->addr[k-1];
    a->address = e->effWORDNO;
    a->cPos= e->effCHAR;
    a->bPos = e->effBITNO;
    
    a->_type = eisTA;
    a->TA = e->TA[k-1];

}

void parseNumericOperandDescriptor(int k, EISstruct *e)
{
    word18 MFk = e->MF[k-1];
    
    word36 opDesc = e->op[k-1];
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    
    word18 address = GETHI(opDesc);
    if (MFk & MFkAR)
    {
        // if MKf contains ar then it Means Y-charn is not the memory address of the data but is a reference to a pointer register pointing to the data.
        uint n = (int)bitfieldExtract36(address, 15, 3);
        word15 offset = address & MASK15;  ///< 15-bit signed number
        address = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
        {
            e->addr[k-1].SNR = PR[n].SNR;
            e->addr[k-1].RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->addr[k-1].mat = viaPR;   // ARs involved
        }
    }
    
    word8 CN = (word8)bitfieldExtract36(opDesc, 15, 3);    ///< character number
    // XXX need to do some error checking here with CN
    
    e->TN[k-1] = (int)bitfieldExtract36(opDesc, 14, 1);    // type numeric
    e->S[k-1]  = (int)bitfieldExtract36(opDesc, 12, 2);    // Sign and decimal type of data
    e->SF[k-1] = (int)SIGNEXT6(bitfieldExtract36(opDesc, 6, 6));    // Scaling factor.
    
    // Operand length. If MFk.RL = 0, this field contains the operand length in
    // digits. If MFk.RL = 1, it contains the REG code for the register holding
    // the operand length and C(REG) is treated as a 0 modulo 64 number. See
    // Table 4-1 and EIS modification fields (MF) above for a discussion of
    // register codes.

    if (MFk & MFkRL)
    {
        uint reg = opDesc & 017;
        e->N[k-1] = getMFReg18(reg, false) & 077;
    }
    else
        e->N[k-1] = opDesc & 077;
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, "N%u %u\n", k, e->N[k-1]);

    word36 r = getMFReg36(MFk & 017, false);
    if (!(MFk & MFkRL) && (MFk & 017) == 4)   // reg == IC ?
    {
        //The ic modifier is permitted in MFk.REG and C (od)32,35 only if MFk.RL = 0, that is, if the contents of the register is an address offset, not the designation of a register containing the operand length.
        address += r;
        r = 0;
    }

    e->effBITNO = 0;
    e->effCHAR = 0;
    e->effWORDNO = 0;
    
    // If seems that the effect address calcs given in AL39 p.6-27 are not quite right.
    // E.g. For CTA4/CTN4 because of the 4 "slop" bits you need to do 32-bit calcs not 36-bit!

    switch (e->TN[k-1])
    {
        case CTN4:
            e->effBITNO = 4 * (ARn_CHAR + 2*r + ARn_BITNO/4) % 2 + 1; // XXX check
            e->effCHAR = ((4*CN + 9*ARn_CHAR + 4*r + ARn_BITNO) % 32) / 4;  //9; 36) / 4;  //9;
            e->effWORDNO = address + (4*CN + 9*ARn_CHAR + 4*r + ARn_BITNO) / 32;    //36;
            e->effWORDNO &= AMASK;
            
            e->CN[k-1] = e->effCHAR;        // ?????
            //e->YChar4[k-1] = e->effWORDNO;
            
            break;
        case CTN9:
            CN = (CN >> 1) & 07;  // XXX Do error checking
            
            e->effBITNO = 0;
            e->effCHAR = (CN + ARn_CHAR + r) % 4;
            e->effWORDNO = address + (9*CN + 9*ARn_CHAR + 9*r + ARn_BITNO) / 36;
            e->effWORDNO &= AMASK;
            
            //e->YChar9[k-1] = e->effWORDNO;
            e->CN[k-1] = e->effCHAR;        // ?????
            
            break;
        default:
            sim_printf ("parseNumericOperandDescriptor(ta=%d) How'd we get here 2?\n", e->TA[k-1]);
            break;
    }
    
    EISaddr *a = &e->addr[k-1];
    a->address = e->effWORDNO;
    a->cPos = e->effCHAR;
    a->bPos = e->effBITNO;
    
    a->_type = eisTN;
    a->TN = e->TN[k-1];
}


static void parseBitstringOperandDescriptor(int k, EISstruct *e)
{
    word18 MFk = e->MF[k-1];
    
    word36 opDesc = e->op[k-1];
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    
    word18 address = GETHI(opDesc);
    if (MFk & MFkAR)
    {
        // if MKf contains ar then it Means Y-charn is not the memory address of the data but is a reference to a pointer register pointing to the data.
        uint n = (int)bitfieldExtract36(address, 15, 3);
        word15 offset = address & MASK15;  // 15-bit signed number
        address = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
sim_debug (DBG_TRACEEXT, & cpu_dev, "bitstring k %d AR%d\n", k, n);
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
        {
            e->addr[k-1].SNR = PR[n].SNR;
            e->addr[k-1].RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->addr[k-1].mat = viaPR;   // ARs involved
        }
    }
    
    //Operand length. If MFk.RL = 0, this field contains the string length of the operand. If MFk.RL = 1, this field contains the code for a register holding the operand string length. See Table 4-1 and EIS modification fields (MF) above for a discussion of register codes.
    if (MFk & MFkRL)
    {
        int reg = opDesc & 017;
sim_debug (DBG_TRACEEXT, & cpu_dev, "bitstring k %d RL reg %d val %llo\n", k, reg, getMFReg36(reg, false));
        e->N[k-1] = getMFReg36(reg, false) & 077777777;
    }
    else
        e->N[k-1] = opDesc & 07777;
sim_debug (DBG_TRACEEXT, & cpu_dev, "bitstring k %d opdesc %012llo\n", k, opDesc);
sim_debug (DBG_TRACEEXT, & cpu_dev, "N%u %u\n", k, e->N[k-1]);
    
    
    //e->B[k-1] = (int)bitfieldExtract36(opDesc, 12, 4) & 0xf;
    //e->C[k-1] = (int)bitfieldExtract36(opDesc, 16, 2) & 03;
    int B = (int)bitfieldExtract36(opDesc, 12, 4) & 0xf;    // bit# from descriptor
    int C = (int)bitfieldExtract36(opDesc, 16, 2) & 03;     // char# from descriptor
    
    word36 r = getMFReg36(MFk & 017, false);
    if (!(MFk & MFkRL) && (MFk & 017) == 4)   // reg == IC ?
    {
        //The ic modifier is permitted in MFk.REG and C (od)32,35 only if MFk.RL = 0, that is, if the contents of the register is an address offset, not the designation of a register containing the operand length.
        address += r;
        r = 0;
    }

    //e->effBITNO = (9*ARn_CHAR + 36*r + ARn_BITNO) % 9;
    //e->effCHAR = ((9*ARn_CHAR + 36*r + ARn_BITNO) % 36) / 9;
    //e->effWORDNO = address + (9*ARn_CHAR + 36*r + ARn_BITNO) / 36;
    e->effBITNO = (9*ARn_CHAR + r + ARn_BITNO + B + 9*C) % 9;
    e->effCHAR = ((9*ARn_CHAR + r + ARn_BITNO + B + 9*C) % 36) / 9;
    e->effWORDNO = address + (9*ARn_CHAR + r + ARn_BITNO + B + 9*C) / 36;
    e->effWORDNO &= AMASK;
    
    e->B[k-1] = e->effBITNO;
    e->C[k-1] = e->effCHAR;
    e->YBit[k-1] = e->effWORDNO;
    
    EISaddr *a = &e->addr[k-1];
    a->address = e->effWORDNO;
    a->cPos = e->effCHAR;
    a->bPos = e->effBITNO;
    a->_type = eisBIT;
}


/*!
 * determine sign of N*9-bit length word
 */
static bool sign9n(word72 n128, int N)
{
    
    // sign bit of  9-bit is bit 8  (1 << 8)
    // sign bit of 18-bit is bit 17 (1 << 17)
    // .
    // .
    // .
    // sign bit of 72-bit is bit 71 (1 << 71)
    
    if (N < 1 || N > 8) // XXX largest int we'll play with is 72-bits? Makes sense
        return false;
    
    word72 sgnmask = (word72)1 << ((N * 9) - 1);
    
    return (bool)(sgnmask & n128);
}

/*!
 * sign extend a N*9 length word to a (word72) 128-bit word
 */
static word72 signExt9(word72 n128, int N)
{
    // ext mask for  9-bit = 037777777777777777777777777777777777777400  8 0's
    // ext mask for 18-bit = 037777777777777777777777777777777777400000 17 0's
    // ext mask for 36-bit = 037777777777777777777777777777400000000000 35 0's
    // etc...
    
    int bits = (N * 9) - 1;
    if (sign9n(n128, N))
    {
        uint128 extBits = ((uint128)-1 << bits);
        return n128 | extBits;
    }
    uint128 zeroBits = ~((uint128)-1 << bits);
    return n128 & zeroBits;
}

/**
 * get sign to buffer position p
 */
static int getSign(word72s n128, EISstruct *e)
{
    // 4- or 9-bit?
    if (e->TN2 == CTN4) // 4-bit
    {
        // If P=1, positive signed 4-bit results are stored using octal 13 as the plus sign.
        // If P=0, positive signed 4-bit results are stored with octal 14 as the plus sign.
        if (n128 >= 0)
        {
            if (e->P)
                return 013;  // alternate + sign
            else
                return 014;  // default + sign
        }
        else
        {
            SETF(e->_flags, I_NEG); 
            return 015;      // - sign
        }
    }
    else
    {   // 9-bit
        if (n128 >= 0)
            return 053;     // default 9-bit +
        else
        {
            SETF(e->_flags, I_NEG);
            return 055;     // default 9-bit -
        }
    }
}


/*!
 * add sign to buffer position p
 */
#ifndef QUIET_UNUSED
static void addSign(word72s n128, EISstruct *e)
{
    *(e->p++) = getSign(n128, e);
}
#endif

/*!
 * load a 9*n bit integer into e->x ...
 */
// CANFAULT
static void load9x(int n, EISaddr *addr, int pos, EISstruct *e)
{
    int128 x = 0;
    
    //word36 data;
    //Read (sourceAddr, &data, OperandRead, 0);    // read data word from memory
    word36 data = EISRead(addr);
    
    int m = n;
    while (m)
    {
        x <<= 9;         // make room for next 9-bit byte
        
        if (pos > 3)        // overflows to next word?
        {   // yep....
            pos = 0;        // reset to 1st byte
            //sourceAddr = (sourceAddr + 1) & AMASK;          // bump source to next address
            //Read (sourceAddr, &data, OperandRead, 0);    // read it from memory
            addr->address = (addr->address + 1) & AMASK;          // bump source to next address
            data = EISRead(addr);    // read it from memory
        }
        
        x |= GETBYTE(data, pos);   // fetch byte at position pos and 'or' it in
        
        pos += 1;           // onto next posotion
        
        m -= 1;             // decrement byte counter
    }
    e->x = signExt9(x, n);  // form proper 2's-complement integer
}

static word4 get4(word36 w, int pos)
{
    switch (pos)
    {
        case 0:
            return bitfieldExtract36(w, 31, 4);
            // break;
        case 1:
            return bitfieldExtract36(w, 27, 4);
            // break;
        case 2:
            return bitfieldExtract36(w, 22, 4);
            // break;
        case 3:
            return bitfieldExtract36(w, 18, 4);
            // break;
        case 4:
            return bitfieldExtract36(w, 13, 4);
            // break;
        case 5:
            return bitfieldExtract36(w, 9, 4);
            // break;
        case 6:
            return bitfieldExtract36(w, 4, 4);
            // break;
        case 7:
            return bitfieldExtract36(w, 0, 4);
            // break;
    }
    sim_printf ("get4(): How'd we get here?\n");
    return 0;
}

static word4 get6(word36 w, int pos)
{
    switch (pos)
    {
        case 0:
            return bitfieldExtract36(w, 30, 6);
            // break;
        case 1:
            return bitfieldExtract36(w, 24, 6);
            // break;
        case 2:
            return bitfieldExtract36(w, 18, 6);
            // break;
        case 3:
            return bitfieldExtract36(w, 12, 6);
            // break;
        case 4:
            return bitfieldExtract36(w, 6, 6);
            // break;
        case 5:
            return bitfieldExtract36(w, 0, 6);
            // break;
    }
    sim_printf ("get6(): How'd we get here?\n");
    return 0;
}

static word9 get9(word36 w, int pos)
{
    
    switch (pos)
    {
        case 0:
            return bitfieldExtract36(w, 27, 9);
            //break;
        case 1:
            return bitfieldExtract36(w, 18, 9);
            //break;
        case 2:
            return bitfieldExtract36(w, 9, 9);
            //break;
        case 3:
            return bitfieldExtract36(w, 0, 9);
            //break;
    }
    sim_printf ("get9(): How'd we get here?\n");
    return 0;
}


/*!
 * return a 4- or 9-bit character at memory "*address" and position "*pos". Increment pos (and address if necesary)
 */
// CANFAULT
static int EISget49(EISaddr *p, int *pos, int tn)
{
    if (!p)
    //{
    //    p->lastAddress = -1;
    //    p->data = 0;
        return 0;
    //}
    
    int maxPos = tn == CTN4 ? 7 : 3;
    
    //if (p->lastAddress != p->address)                 // read from memory if different address
        // p->data = EISRead(p);   // read data word from memory
    
    if (*pos > maxPos)        // overflows to next word?
    {   // yep....
        *pos = 0;        // reset to 1st byte
        p->address = (p->address + 1) & AMASK;          // bump source to next address
        p->data = EISRead(p);    // read it from memory
    } else {
        p->data = EISRead(p);   // read data word from memory
    }
    
    int c = 0;
    switch(tn)
    {
        case CTN4:
            c = (word4)get4(p->data, *pos);
            break;
        case CTN9:
            c = (word9)get9(p->data, *pos);
            break;
    }
    
    *pos += 1;
    //p->lastAddress = p->address;
    
    return c;
}

/*!
 * return a 4-, 6- or 9-bit character at memory "*address" and position "*pos". Increment pos (and address if necesary)
 * NB: must be initialized before use or else unpredictable side-effects may result. Not thread safe!
 */
// CANFAULT
static int EISget469(EISaddr *p, int *pos, int ta)
{
    if (!p)
    //{
    //    p->lastAddress = -1;
        return 0;
    //}
    
    int maxPos = 3; // CTA9
    switch(ta)
    {
        case CTA4:
            maxPos = 7;
            break;
            
        case CTA6:
            maxPos = 5;
            break;
    }
    
    //if (p->lastAddress != p->address)                 // read from memory if different address
    //    p->data = EISRead(p);   // read data word from memory
    
    while (*pos > maxPos)        // overflows to next word?
    {   // yep....
        *pos -= (maxPos + 1);        // reset to 1st byte
        p->address = (p->address + 1) & AMASK;          // bump source to next address
        //p->data = EISRead(p);    // read it from memory
    }
    p->data = EISRead(p);    // read it from memory

#ifdef DBGF
dbgAddr0 = p -> address;
dbgData0 = p -> data;
#endif
//sim_printf ("pos %u\n", * pos); 
    int c = 0;
    switch(ta)
    {
        case CTA4:
            c = (word4)get4(p->data, *pos);
            break;
        case CTA6:
            c = (word6)get6(p->data, *pos);
            break;
        case CTA9:
            c = (word9)get9(p->data, *pos);
            break;
    }
sim_debug (DBG_TRACEEXT, & cpu_dev, "EISGet469 : k: %u TAk %u coffset %u c %o \n", 0, ta, * pos, c);
    
    *pos += 1;
    //p->lastAddress = p->address;
    return c;
}

/*!
 * return a 4-, 6- or 9-bit character at memory "*address" and position "*pos". Decrement pos (and address if necesary)
 * NB: must be initialized before use or else unpredictable side-effects may result. Not thread safe
 */

// CANFAULT
static int EISget469r(EISaddr *p, int *pos, int ta)
{
    //static word18 lastAddress;// try to keep memory access' down
    //static word36 data;
    
    if (!p)
   // {
    //    lastAddress = -1;
        return 0;
   // }
    
    int maxPos = 3; // CTA9
    switch(ta)
    {
        case CTA4:
            maxPos = 7;
            break;
            
        case CTA6:
            maxPos = 5;
            break;
    }
    
    if (*pos < 0)  // underlows to next word?
    {
        *pos = maxPos;
        p->address = (p->address - 1) & AMASK;          // bump source to prev address
        //p->data = EISRead(p);    // read it from memory
    }
    
    //if (p->lastAddress != p->address) {                // read from memory if different address
        p->data = EISRead(p);   // read data word from memory
    //    p->lastAddress = p->address;
    //}
    
    int c = 0;
    switch(ta)
    {
        case CTA4:
            c = (word4)get4(p->data, *pos);
            break;
        case CTA6:
            c = (word6)get6(p->data, *pos);
            break;
        case CTA9:
            c = (word9)get9(p->data, *pos);
            break;
    }
    
    *pos -= 1;
    
    return c;
}

/*!
 * load a decimal number into e->x ...
 */
// CANFAULT
static void loadDec(EISaddr *p, int pos, EISstruct *e)
{
    int128 x = 0;
    
    
    // XXX use get49() for this later .....
    //word36 data;
    //Read (sourceAddr, &data, OperandRead, 0);    // read data word from memory
    p->data = EISRead(p);    // read data word from memory
    
    int maxPos = e->TN1 == CTN4 ? 7 : 3;

    int sgn = 1;
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "loadDec: maxPos %d N1 %d\n", maxPos, e->N1);
    for(uint n = 0 ; n < e->N1 ; n += 1)
    {
        if (pos > maxPos)   // overflows to next word?
        {   // yep....
            pos = 0;        // reset to 1st byte
            //sourceAddr = (sourceAddr + 1) & AMASK;      // bump source to next address
            //Read (sourceAddr, &data, OperandRead, 0);    // read it from memory
            p->address = (p->address + 1) & AMASK;      // bump source to next address
            p->data = EISRead(p);    // read it from memory
        }
        
        int c = 0;
        switch(e->TN1)
        {
            case CTN4:
                c = (word4)get4(p->data, pos);
                break;
            case CTN9:
                c = (word9)GETBYTE(p->data, pos);
                break;
        }
        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "loadDec: n %d c %d(%o)\n", n, c, c);
        
        if (n == 0 && e->TN1 == CTN4 && e->S1 == CSLS)
        {
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "loadDec: n 0, TN1 CTN4, S1 CSLS\n");
            switch (c)
            {
                case 015:   // 6-bit - sign
                    SETF(e->_flags, I_NEG);
                    
                    sgn = -1;
                    break;
                case 013:   // alternate 4-bit + sign
                case 014:   // default   4-bit + sign
                    break;
                default:
                    sim_printf ("loadDec:1\n");
                    // not a leading sign
                    // XXX generate Ill Proc fault
            }
            pos += 1;           // onto next posotion
            continue;
        }

        if (n == 0 && e->TN1 == CTN9 && e->S1 == CSLS)
        {
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "loadDec: n 0, TN1 CTN9, S1 CSLS\n");
            switch (c)
            {
                case '-':
                    SETF(e->_flags, I_NEG);
                    
                    sgn = -1;
                    break;
                case '+':
                    break;
                default:
                    sim_printf ("loadDec:2\n");
                    // not a leading sign
                    // XXX generate Ill Proc fault

            }
            pos += 1;           // onto next posotion
            continue;
        }

        if (n == e->N1-1 && e->TN1 == CTN4 && e->S1 == CSTS)
        {
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "loadDec: n N1-1, TN1 CTN4, S1 CSTS\n");
            switch (c)
            {
                case 015:   // 4-bit - sign
                    SETF(e->_flags, I_NEG);
                    
                    sgn = -1;
                    break;
                case 013:   // alternate 4-bit + sign
                case 014:   // default   4-bit + sign
                    break;
                default:
                    sim_printf ("loadDec:3\n");
                    // not a trailing sign
                    // XXX generate Ill Proc fault
            }
            break;
        }

        if (n == e->N1-1 && e->TN1 == CTN9 && e->S1 == CSTS)
        {
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "loadDec: n N1-1, TN1 CTN9, S1 CSTS\n");
            switch (c)
            {
                case '-':
                    SETF(e->_flags, I_NEG);
                    
                    sgn = -1;
                    break;
                case '+':
                    break;
                default:
                    sim_printf ("loadDec:4\n");
                    // not a trailing sign
                    // XXX generate Ill Proc fault
            }
            break;
        }
        
        x *= 10;
        x += c & 0xf;
        sim_debug (DBG_TRACEEXT, & cpu_dev,
              "loadDec:  x %lld\n", (int64) x);
        
        pos += 1;           // onto next posotion
    }
    
    e->x = sgn * x;
    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "loadDec:  final x %lld\n", (int64) x);
}

/*!
 * write 9-bit bytes to memory @ pos (in reverse)...
 */
// CANFAULT
static void EISwrite9r(EISaddr *p, int *pos, int char9)
{
    word36 w;
    if (*pos < 0)    // out-of-range?
    {
        *pos = 3;    // reset to 1st byte
        p->address = (p->address - 1) & AMASK;        // goto next dstAddr in memory
    }
    
    //Read (*dstAddr, &w, OperandRead, 0);      // read dst memory into w
    w = EISRead(p);      // read dst memory into w
    
    switch (*pos)
    {
        case 0:
            w = bitfieldInsert36(w, char9, 27, 9);
            break;
        case 1:
            w = bitfieldInsert36(w, char9, 18, 9);
            break;
        case 2:
            w = bitfieldInsert36(w, char9, 9, 9);
            break;
        case 3:
            w = bitfieldInsert36(w, char9, 0, 9);
            break;
    }
    
    //Write (*dstAddr, w, OperandWrite, 0); // XXX this is the ineffecient part!
    EISWrite(p, w); // XXX this is the ineffecient part!
    
    *pos -= 1;       // to prev byte.
}

/*!
 * write 6-bit chars to memory @ pos (in reverse)...
 */
// CANFAULT
static void EISwrite6r(EISaddr *p, int *pos, int char6)
{
    word36 w;
    if (*pos < 0)    // out-of-range?
    {
        *pos = 5;    // reset to 1st byte
        p->address = (p->address - 1) & AMASK;        // goto next dstAddr in memory
    }
    
    //Read (*dstAddr, &w, OperandRead, 0);      // read dst memory into w
    w = EISRead(p);      // read dst memory into w
    
    switch (*pos)
    {
        case 0:
            w = bitfieldInsert36(w, char6, 30, 6);
            break;
        case 1:
            w = bitfieldInsert36(w, char6, 24, 6);
            break;
        case 2:
            w = bitfieldInsert36(w, char6, 18, 6);
            break;
        case 3:
            w = bitfieldInsert36(w, char6, 12, 6);
            break;
        case 4:
            w = bitfieldInsert36(w, char6, 6, 6);
            break;
        case 5:
            w = bitfieldInsert36(w, char6, 0, 6);
            break;
    }
    
    //Write (*dstAddr, w, OperandWrite, 0); // XXX this is the ineffecient part!
    EISWrite(p, w); // XXX this is the ineffecient part!
    
    *pos -= 1;       // to prev byte.
}



/*!
 * write 4-bit digits to memory @ pos (in reverse) ...
 */
// CANFAULT
static void EISwrite4r(EISaddr *p, int *pos, int char4)
{
    word36 w;
    
    if (*pos < 0)    // out-of-range?
    {
        *pos = 7;    // reset to 1st byte
        p->address = (p->address - 1) & AMASK;         // goto prev dstAddr in memory
    }
    //Read (*dstAddr, &w, OperandRead, 0);      // read dst memory into w
    w = EISRead(p);
    
    switch (*pos)
    {
        case 0:
            w = bitfieldInsert36(w, char4, 31, 5);
            break;
        case 1:
            w = bitfieldInsert36(w, char4, 27, 4);
            break;
        case 2:
            w = bitfieldInsert36(w, char4, 22, 5);
            break;
        case 3:
            w = bitfieldInsert36(w, char4, 18, 4);
            break;
        case 4:
            w = bitfieldInsert36(w, char4, 13, 5);
            break;
        case 5:
            w = bitfieldInsert36(w, char4, 9, 4);
            break;
        case 6:
            w = bitfieldInsert36(w, char4, 4, 5);
            break;
        case 7:
            w = bitfieldInsert36(w, char4, 0, 4);
            break;
    }
    
    //Write (*dstAddr, w, OperandWrite, 0); // XXX this is the ineffecient part!
    EISWrite(p, w); // XXX this is the ineffecient part!
    
    *pos -= 1;       // to prev byte.
}

/*!
 * write 4-bit chars to memory @ pos ...
 */
// CANFAULT
static void EISwrite4(EISaddr *p, int *pos, int char4)
{
    word36 w;
    if (*pos > 7)    // out-of-range?
    {
        *pos = 0;    // reset to 1st byte
        p->address = (p->address + 1) & AMASK;        // goto next dstAddr in memory
    }
    
    w = EISRead(p);      // read dst memory into w
    
    switch (*pos)
    {
        case 0:
            w = bitfieldInsert36(w, char4, 31, 5);
            break;
        case 1:
            w = bitfieldInsert36(w, char4, 27, 4);
            break;
        case 2:
            w = bitfieldInsert36(w, char4, 22, 5);
            break;
        case 3:
            w = bitfieldInsert36(w, char4, 18, 4);
            break;
        case 4:
            w = bitfieldInsert36(w, char4, 13, 5);
            break;
        case 5:
            w = bitfieldInsert36(w, char4, 9, 4);
            break;
        case 6:
            w = bitfieldInsert36(w, char4, 4, 5);
            break;
        case 7:
            w = bitfieldInsert36(w, char4, 0, 4);
            break;
    }
    
    
    EISWrite(p, w); // XXX this is the ineffecient part!
    
    *pos += 1;       // to next char.
}

/*!
 * write 6-bit digits to memory @ pos ...
 */
// CANFAULT
static void EISwrite6(EISaddr *p, int *pos, int char6)
{
    word36 w;
    
    if (*pos > 5)    // out-of-range?
    {
        *pos = 0;    // reset to 1st byte
        p->address = (p->address + 1) & AMASK;        // goto next dstAddr in memory
    }
    
    w = EISRead(p);      // read dst memory into w
    
    switch (*pos)
    {
        case 0:
            w = bitfieldInsert36(w, char6, 30, 6);
            break;
        case 1:
            w = bitfieldInsert36(w, char6, 24, 6);
            break;
        case 2:
            w = bitfieldInsert36(w, char6, 18, 6);
            break;
        case 3:
            w = bitfieldInsert36(w, char6, 12, 6);
            break;
        case 4:
            w = bitfieldInsert36(w, char6, 6, 6);
            break;
        case 5:
            w = bitfieldInsert36(w, char6, 0, 6);
            break;
    }
    
    EISWrite(p, w); // XXX this is the ineffecient part!
    
    *pos += 1;       // to next byte.
}

/*!
 * write 9-bit bytes to memory @ pos ...
 */
// CANFAULT
static void EISwrite9(EISaddr *p, int *pos, int char9)
{
    word36 w;
    if (*pos > 3)    // out-of-range?
    {
        *pos = 0;    // reset to 1st byte
        p->address = (p->address + 1) & AMASK;       // goto next dstAddr in memory
    }
    
    w = EISRead(p);      // read dst memory into w
   
    switch (*pos)
    {
        case 0:
            w = bitfieldInsert36(w, char9, 27, 9);
            break;
        case 1:
            w = bitfieldInsert36(w, char9, 18, 9);
            break;
        case 2:
            w = bitfieldInsert36(w, char9, 9, 9);
            break;
        case 3:
            w = bitfieldInsert36(w, char9, 0, 9);
            break;
    }
    
    EISWrite(p, w); // XXX this is the ineffecient part!
    
    *pos += 1;       // to next byte.
}

/*!
 * write a 4-, 6-, or 9-bit char to dstAddr ....
 */
// CANFAULT
static void EISwrite469(EISaddr *p, int *pos, int ta, int c469)
{
    switch(ta)
    {
        case CTA4:
            return EISwrite4(p, pos, c469);
        case CTA6:
            return EISwrite6(p, pos, c469);
        case CTA9:
            return EISwrite9(p, pos, c469);
    }
    
}

/*!
 * write a 4-, 6-, or 9-bit char to dstAddr (in reverse)....
 */
// CANFAULT
static void EISwrite469r(EISaddr *p, int *pos, int ta, int c469)
{
    switch(ta)
    {
        case CTA4:
            return EISwrite4r(p, pos, c469);
        case CTA6:
            return EISwrite6r(p, pos, c469);
        case CTA9:
            return EISwrite9r(p, pos, c469);
    }
    
}

/*!
 * write a 4-, or 9-bit numeric char to dstAddr ....
 */
// CANFAULT
void EISwrite49(EISaddr *p, int *pos, int tn, int c49)
{
    switch(tn)
    {
        case CTN4:
            return EISwrite4(p, pos, c49);
        case CTN9:
            return EISwrite9(p, pos, c49);
    }
}

/*!
 * write char to output string in Reverse. Right Justified and taking into account string length of destination
 */
// CANFAULT
static void EISwriteToOutputStringReverse(EISstruct *e, int k, int charToWrite)
{
    // first thing we need to do is to find out the last position is the buffer we want to start writing to.
    
    static int N = 0;           // length of output buffer in native chars (4, 6 or 9-bit chunks)
    static int CN = 0;          // character number 0-7 (4), 0-5 (6), 0-3 (9)
    static int TN = 0;          // type code
    static int pos = 0;         // current character position
    //static int size = 0;        // size of char
    static int _k = -1;         // k of MFk

    if (k)
    {
        _k = k;
        
        N = e->N[k-1];      // length of output buffer in native chars (4, 9-bit chunks)
        CN = e->CN[k-1];    // character number (position) 0-7 (4), 0-5 (6), 0-3 (9)
        TN = e->TN[k-1];    // type code
        
        //int chunk = 0;
        int maxPos;
        switch (TN)
        {
            case CTN4:
                //address = e->addr[k-1].address;
                //size = 4;
                //chunk = 32;
                maxPos = 8;
                break;
            case CTN9:
                //address = e->addr[k-1].address;
                //size = 9;
                //chunk = 36;
                maxPos = 4;
                break;
        }
        
        // since we want to write the data in reverse (since it's right 
        // justified) we need to determine the final address/CN for the 
        // type and go backwards from there
        
        //int numBits = size * N;      // 8 4-bit digits, 4 9-bit bytes / word
        // (remember there are 4 slop bits in a 36-bit word when dealing with 
        // BCD)

        // how many additional words will the N chars take up?
        //int numWords = numBits / ((TN == CTN4) ? 32 : 36);      

// CN+N    numWords  (CN+N+3)/4   lastChar
//   1       1                      0
//   2       1                      1
//   3       1                      2
//   4       1                      3
//   5       2                      0

        int numWords = (CN + N + (maxPos - 1)) / maxPos;
        int lastWordOffset = numWords - 1;
        int lastChar = (CN + N - 1) % maxPos;   // last character number
        
        if (lastWordOffset > 0)           // more that the 1 word needed?
            //address += lastWordOffset;    // highest memory address
            e->addr[k-1].address += lastWordOffset;
        
        pos = lastChar;             // last character number
        
        //sim_printf ("numWords=%d lastChar=%d\n", numWords, lastChar);
        return;
    }
    
    // any room left in output string?
    if (N == 0)
    {
        return;
    }
    
    // we should write character to word/pos in memory .....
    switch(TN)
    {
        case CTN4:
            EISwrite4r(&e->addr[_k-1], &pos, charToWrite);
            break;
        case CTN9:
            EISwrite9r(&e->addr[_k-1], &pos, charToWrite);
            break;
    }
    N -= 1;
}

// CANFAULT
static void EISwriteToBinaryStringReverse(EISaddr *p, int k)
{
    /// first thing we need to do is to find out the last position is the buffer we want to start writing to.
    
    int N = p->e->N[k-1];            ///< length of output buffer in native chars (4, 6 or 9-bit chunks)
    int CN = p->e->CN[k-1];          ///< character number 0-3 (9)
    //word18 address  = e->YChar9[k-1]; ///< current write address
    
    /// since we want to write the data in reverse (since it's right justified) we need to determine
    /// the final address/CN for the type and go backwards from there
    
    int numBits = 9 * N;               ///< 4 9-bit bytes / word
    //int numWords = numBits / 36;       ///< how many additional words will the N chars take up?
    //int numWords = (numBits + CN * 9) / 36;       ///< how many additional words will the N chars take up?
    int numWords = (numBits + CN * 9 + 35) / 36;       ///< how many additional words will the N chars take up?
    // convert from count to offset
    int lastWordOffset = numWords - 1;
    int lastChar = (CN + N - 1) % 4;   ///< last character number
    
    if (lastWordOffset > 0)           // more that the 1 word needed?
        p->address += lastWordOffset;    // highest memory address
    int pos = lastChar;             // last character number
    
    int128 x = p->e->x;
    
    for(int n = 0 ; n < N ; n += 1)
    {
        int charToWrite = x & 0777; // get 9-bits of data
        x >>=9;
        
        // we should write character to word/pos in memory .....
        //write9r(e, &address, &pos, charToWrite);
        EISwrite9r(p, &pos, charToWrite);
    }
    
    // anything left in x?. If it's not all 1's we have an overflow!
    if (~x && x != 0)    // if it's all 1's this will be 0
        SETF(p->e->_flags, I_OFLOW);
}



/// perform a binary to decimal conversion ...

/// Since (according to DH02) we want to "right-justify" the output string it might be better to presere the reverse writing and start writing
/// characters directly into the output string taking into account the output string length.....


// CANFAULT
static void _btd(EISstruct *e)
{
    word72s n128 = e->x;    ///< signExt9(e->x, e->N1);          ///< adjust for +/-
    int sgn = (n128 < 0) ? -1 : 1;  ///< sgn(x)
    if (n128 < 0)
        n128 = -n128;
    
    //if (n128 == 0)  // If C(Y-charn2) = decimal 0, then ON: otherwise OFF
        //SETF(e->_flags, I_ZERO);
    SCF(n128 == 0, e->_flags, I_ZERO);
   
    int N = e->N2;  // number of chars to write ....
    
    // handle any trailing sign stuff ...
    if (e->S2 == CSTS)  // a trailing sign
    {
        EISwriteToOutputStringReverse(e, 0, getSign(sgn, e));
        if (TSTF(e->_flags, I_OFLOW))   // Overflow! Too many chars, not enough room!
            return;
        N -= 1;
    }
    do
    {
        int n = n128 % 10;
        
        EISwriteToOutputStringReverse(e, 0, (e->TN2 == CTN4) ? n : (n + '0'));
        
        if (TSTF(e->_flags, I_OFLOW))   // Overflow! Too many chars, not enough room!
            return;
        
        N -= 1;
        
        n128 /= 10;
    } while (n128);
    
    // at this point we've exhausted our digits, but may still have spaces left.
    
    // handle any leading sign stuff ...
    if (e->S2 == CSLS)  // a leading sign
    {
        while (N > 1)
        {
            EISwriteToOutputStringReverse(e, 0, (e->TN2 == CTN4) ? 0 : '0');
            N -= 1;
        }
        EISwriteToOutputStringReverse(e, 0, getSign(sgn, e));
        if (TSTF(e->_flags, I_OFLOW))   // Overflow! Too many chars, not enough room!
            return;
    }
    else
    {
        while (N > 0)
        {
            EISwriteToOutputStringReverse(e, 0, (e->TN2 == CTN4) ? 0 : '0');
            N -= 1;
        }
    }
}


// CANFAULT
void btd(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    
    //! \brief C(Y-char91) converted to decimal → C(Y-charn2)
    /*!
     * C(Y-char91) contains a twos complement binary integer aligned on 9-bit character boundaries with length 0 < N1 <= 8.
     * If TN2 and S2 specify a 4-bit signed number and P = 1, then if C(Y-char91) is positive (bit 0 of C(Y-char91)0 = 0), then the 13(8) plus sign character is moved to C(Y-charn2) as appropriate.
     *   The scaling factor of C(Y-charn2), SF2, must be 0.
     *   If N2 is not large enough to hold the digits generated by conversion of C(Y-char91) an overflow condition exists; the overflow indicator is set ON and an overflow fault occurs. This implies that an unsigned fixed-point receiving field has a minimum length of 1 character and a signed fixed- point field, 2 characters.
     * If MFk.RL = 1, then Nk does not contain the operand length; instead; it contains a register code for a register holding the operand length.
     * If MFk.ID = 1, then the kth word following the instruction word does not contain an operand descriptor; instead, it contains an indirect pointer to the operand descriptor.
     * C(Y-char91) and C(Y-charn2) may be overlapping strings; no check is made.
     * Attempted conversion to a floating-point number (S2 = 0) or attempted use of a scaling factor (SF2 ≠ 0) causes an illegal procedure fault.
     * If N1 = 0 or N1 > 8 an illegal procedure fault occurs.
     * Attempted execution with the xed instruction causes an illegal procedure fault.
     * Attempted repetition with the rpt, rpd, or rpl instructions causes an illegal procedure fault.
     */
    
    //! C(string 1) -> C(string 2) (converted)
    
    //! The two's complement binary integer starting at location YC1 is converted into a signed string of decimal characters of data type TN2, sign and decimal type S2 (S2 = 00 is illegal) and scale factor 0; and is stored, right-justified, as a string of length L2 starting at location YC2. If the string generated is longer than L2, the high-order excess is truncated and the overflow indicator is set. If strings 1 and 2 are not overlapped, the contents of string 1 remain unchanged. The length of string 1 (L1) is given as the number of 9-bit segments that make up the string. L1 is equal to or is less than 8. Thus, the binary string to be converted can be 9, 18, 27, 36, 45, 54, 63, or 72 bits long. CN1 designates a 9-bit character boundary. If P=1, positive signed 4-bit results are stored using octal 13 as the plus sign. If P=0, positive signed 4-bit results are stored with octal 14 as the plus sign.

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);

    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    
    //word18 addr = (e->TN1 == CTN4) ? e->YChar41 : e->YChar91;
    //load9x(e->N1, addr, e->CN1, e);
                  // Technically, ill_proc should be "illegal eis modifier",
                  // but the Fault Register has no such bit; the Fault
                  // register description says ill_proc is anything not
                  // handled by other bits.
    if (e->N1 == 0 || e->N1 > 8)
        doFault(illproc_fault, ill_proc, "btd(1): N1 == 0 || N1 > 8"); 

    load9x(e->N1, &e->ADDR1, e->CN1, e);
    
    EISwriteToOutputStringReverse(e, 2, 0);    // initialize output writer .....
    
    e->_flags = cu.IR;
    
    CLRF(e->_flags, I_NEG);     // If a minus sign character is moved to C(Y-charn2), then ON; otherwise OFF
    CLRF(e->_flags, I_ZERO);    // If C(Y-charn2) = decimal 0, then ON: otherwise OFF

    _btd(e);
    
    cu.IR = e->_flags;
    if (TSTF(cu.IR, I_OFLOW))
        doFault(overflow_fault, 0, "btd() overflow!");   // XXX generate overflow fault
    
}

// CANFAULT
void dtb(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
   
    //Attempted conversion of a floating-point number (S1 = 0) or attempted use of a scaling factor (SF1 ≠ 0) causes an illegal procedure fault.
    //If N2 = 0 or N2 > 8 an illegal procedure fault occurs.
    if (e->S1 == 0 || e->SF1 != 0 || e->N2 == 0 || e->N2 > 8)
        ; // generate ill proc fault
    
    e->_flags = cu.IR;
    
    // Negative: If a minus sign character is found in C(Y-charn1), then ON; otherwise OFF
    CLRF(e->_flags, I_NEG);
    
    //loadDec(e->TN1 == CTN4 ? e->YChar41 : e->YChar91, e->CN1, e);
    loadDec(&e->ADDR1, e->CN1, e);
    
    // Zero: If C(Y-char92) = 0, then ON: otherwise OFF
    SCF(e->x == 0, e->_flags, I_ZERO);
    
    EISwriteToBinaryStringReverse(&e->ADDR2, 2);
    
    cu.IR = e->_flags;

    if (TSTF(cu.IR, I_OFLOW))
        ;   // XXX generate overflow fault
    
}

/*!
 * Edit instructions & support code ...
 */
// CANFAULT
static void EISwriteOutputBufferToMemory(EISaddr *p)
{
    //4. If an edit insertion table entry or MOP insertion character is to be stored, ANDed, or ORed into a receiving string of 4- or 6-bit characters, high-order truncate the character accordingly.
    //5. If the receiving string is 9-bit characters, high-order fill the (4-bit) digits from the input buffer with bits 0-4 of character 8 of the edit insertion table. If the receiving string is 6-bit characters, high-order fill the digits with "00"b.
    
    int pos = p->e->dstCN; // what char position to start writing to ...
    //word24 dstAddr = p->e->dstAddr;
    
    for(int n = 0 ; n < p->e->dstTally ; n++)
    {
        int c49 = p->e->outBuffer[n];
        
        switch(p->e->dstSZ)
        {
            case 4:
                EISwrite4(p, &pos, c49 & 0xf);    // write only-4-bits
                break;
            case 6:
                EISwrite6(p, &pos, c49 & 0x3f);   // write only 6-bits
                break;
            case 9:
                EISwrite9(p, &pos, c49);
                break;
        }
        //                break;
        //        }
    }
}

// CANFAULT
static void writeToOutputBuffer(EISstruct *e, word9 **dstAddr, int szSrc, int szDst, int c49)
{
    //4. If an edit insertion table entry or MOP insertion character is to be stored, ANDed, or ORed into a receiving string of 4- or 6-bit characters, high-order truncate the character accordingly.
    //5. If the receiving string is 9-bit characters, high-order fill the (4-bit) digits from the input buffer with bits 0-4 of character 8 of the edit insertion table. If the receiving string is 6-bit characters, high-order fill the digits with "00"b.
    
    switch (szSrc)
    {
        case 4:
            switch(szDst)
            {
                case 4:
                    **dstAddr = c49 & 0xf;
                    break;
                case 6:
                    **dstAddr = c49 & 077;   // high-order fill the digits with "00"b.
                    break;
                case 9:
                    **dstAddr = c49 | (e->editInsertionTable[7] & 0760);
                    break;
            }
            break;
        case 6:
            switch(szDst)
            {
                case 4:
                    **dstAddr = c49 & 0xf;    // write only-4-bits
                    break;
                case 6:
                    **dstAddr = c49;   
                    break;
                case 9:
                    **dstAddr = c49;
                    break;
            }
            break;
        case 9:
            switch(szDst)
            {
                case 4:
                    **dstAddr = c49 & 0xf;    // write only-4-bits
                    break;
                case 6:
                    **dstAddr = c49 & 077;   // write only 6-bits
                    break;
                case 9:
                    **dstAddr = c49;
                    break;
            }
            break;
    }
    e->dstTally -= 1;
    *dstAddr += 1;
}


/*!
 * Load the entire sending string number (maximum length 63 characters) into
 * the decimal unit input buffer as 4-bit digits (high-order truncating 9-bit
 * data). Strip the sign and exponent characters (if any), put them aside into
 * special holding registers and decrease the input buffer count accordingly.
 */

// CANFAULT
void EISloadInputBufferNumeric(DCDstruct *ins, int k)
{
    EISstruct *e = &ins->e;
    
    word9 *p = e->inBuffer; // p points to position in inBuffer where 4-bit chars are stored
    memset(e->inBuffer, 0, sizeof(e->inBuffer));   // initialize to all 0's
    
    int pos = e->CN[k-1];
    
    int TN = e->TN[k-1];
    int S = e->S[k-1];  // This is where MVNE gets really nasty.
    // I spit on the designers of this instruction set (and of COBOL.) >Ptui!<
    
    int N = e->N[k-1];  // number of chars in src string
    
    //word18 addr = (TN == CTN4) ? e->YChar4[k-1] : e->YChar9[k-1];   // get address of numeric source string
    EISaddr *a = &e->addr[k-1];
    
    e->sign = 1;
    e->exponent = 0;
    
    // load according to MFk.....
    //word36 data;
    //Read(addr, &data, OperandRead, 0);    // read data word from memory
    
    //int maxPos = e->TN1 == CTN4 ? 7 : 3;
    EISget49(NULL, 0, 0);
    
    for(int n = 0 ; n < N ; n += 1)
    {
        int c = EISget49(a, &pos, TN);
        sim_debug (DBG_TRACEEXT, & cpu_dev, "src: %d: %o\n", n, c);
        
        /*
         * Here we need to distinguish between 4 type of numbers.
         *
         * CSFL - Floating-point, leading sign
         * CSLS - Scaled fixed-point, leading sign
         * CSTS - Scaled fixed-point, trailing sign
         * CSNS - Scaled fixed-point, unsigned
         */
        switch(S)
        {
            case CSFL:  // this is the real evil one ....
                /* Floating-point:
                 * [sign=c0] c1×10(n-3) + c2×10(n-4) + ... + c(n-3) [exponent=8 bits]
                 * where:
                 *  ci is the decimal value of the byte in the ith byte position.
                 *  [sign=ci] indicates that ci is interpreted as a sign byte.
                 *  [exponent=8 bits] indicates that the exponent value is taken from the last 8 bits of the string. If the data is in 9-bit bytes, the exponent is bits 1-8 of c(n-1). If the data is in 4- bit bytes, the exponent is the binary value of the concatenation of c(n-2) and c(n-1).
                 */
                if (n == 0) // first had better be a sign ....
                {
                    c &= 0xf;   // hack off all but lower 4 bits
                    
                    if (c < 012 || c > 017)
                        doFault(illproc_fault, ill_dig, "loadInputBufferNumric(1): illegal char in input"); // TODO: generate ill proc fault
                    
                    if (c == 015)   // '-'
                        e->sign = -1;
                    
                    e->srcTally -= 1;   // 1 less source char
                }
                else if (TN == CTN9 && n == N-1)    // the 9-bit exponent (of which only 8-bits are used)
                {
                    e->exponent = (signed char)(c & 0377); // want to do a sign extend
                    e->srcTally -= 1;   // 1 less source char
                }
                else if (TN == CTN4 && n == N-2)    // the 1st 4-chars of the 8-bit exponent
                {
                    e->exponent = (c & 0xf);// << 4;
                    e->exponent <<= 4;
                    e->srcTally -= 1;   // 1 less source char
                }
                else if (TN == CTN4 && n == N-1)    // the 2nd 4-chars of the 8-bit exponent
                {
                    e->exponent |= (c & 0xf);
                    
                    signed char ce = e->exponent & 0xff;
                    e->exponent = ce;
                    
                    e->srcTally -= 1;   // 1 less source char
                }
                else
                {
                    c &= 0xf;   // hack off all but lower 4 bits
                    if (c > 011)
                        doFault(illproc_fault,ill_dig,"loadInputBufferNumric(2): illegal char in input"); // TODO: generate ill proc fault
                    
                    *p++ = c; // store 4-bit char in buffer
                }
                break;
                
            case CSLS:
                // Only the byte values [0,11]8 are legal in digit positions and only the byte values [12,17]8 are legal in sign positions. Detection of an illegal byte value causes an illegal procedure fault
                c &= 0xf;   // hack off all but lower 4 bits
                
                if (n == 0) // first had better be a sign ....
                {
                    if (c < 012 || c > 017)
                        doFault(illproc_fault,ill_dig,"loadInputBufferNumric(3): illegal char in input"); // TODO: generate ill proc fault
                    if (c == 015)   // '-'
                        e->sign = -1;
                    e->srcTally -= 1;   // 1 less source char
                }
                else
                {
                    if (c > 011)
                        doFault(illproc_fault, ill_dig,"loadInputBufferNumric(4): illegal char in input"); // XXX generate ill proc fault
                    *p++ = c; // store 4-bit char in buffer
                }
                break;
                
            case CSTS:
                c &= 0xf;   // hack off all but lower 4 bits
                
                if (n == N-1) // last had better be a sign ....
                {
                    if (c < 012 || c > 017)
                         doFault(illproc_fault, ill_dig,"loadInputBufferNumric(5): illegal char in input"); // XXX generate ill proc fault; // XXX generate ill proc fault
                    if (c == 015)   // '-'
                        e->sign = -1;
                    e->srcTally -= 1;   // 1 less source char
                }
                else
                {
                    if (c > 011)
                        doFault(illproc_fault, ill_dig,"loadInputBufferNumric(6): illegal char in input"); // XXX generate ill proc fault
                    *p++ = c; // store 4-bit char in buffer
                }
                break;
                
            case CSNS:
                c &= 0xf; // hack off all but lower 4 bits
                
                *p++ = c; // the "easy" one
                break;
        }
    }
    if_sim_debug (DBG_TRACEEXT, & cpu_dev)
      {
        sim_debug (DBG_TRACEEXT, & cpu_dev, "inBuffer:");
        for (word9 *q = e->inBuffer; q < p; q ++)
          sim_debug (DBG_TRACEEXT, & cpu_dev, " %02o", * q);
        sim_debug (DBG_TRACEEXT, & cpu_dev, "\n");
      }
}

/*!
 * Load decimal unit input buffer with sending string characters. Data is read
 * from main memory in unaligned units (not modulo 8 boundary) of Y-block8
 * words. The number of characters loaded is the minimum of the remaining
 * sending string count, the remaining receiving string count, and 64.
 */

// CANFAULT
static void EISloadInputBufferAlphnumeric(EISstruct *e, int k)
{
    word9 *p = e->inBuffer; // p points to position in inBuffer where 4-bit chars are stored
    memset(e->inBuffer, 0, sizeof(e->inBuffer));   // initialize to all 0's
    
    int pos = e->CN[k-1];
    
    int TA = e->TA[k-1];
    
    int N = min3(e->N1, e->N3, 64);  // minimum of the remaining sending string count, the remaining receiving string count, and 64.
    
    //word18 addr = 0;
//    switch(TA)
//    {
//        case CTA4:
//            //addr = e->YChar4[k-1];
//            break;
//        case CTA6:
//            //addr = e->YChar6[k-1];
//            break;
//        case CTA9:
//            //addr = e->YChar9[k-1];
//            break;
//    }
    
    
    //get469(NULL, 0, 0, 0);    // initialize char getter buffer
    EISaddr *a = &e->addr[k-1];
    
    for(int n = 0 ; n < N ; n += 1)
    {
        int c = EISget469(a, &pos, TA);
        *p++ = c;
    }
    
   // get469(NULL, 0, 0, 0);    // initialize char getter buffer
    
}

/*!
 * MVE/MVNE ...
 */

///< MicroOperations ...
///< Table 4-9. Micro Operation Code Assignment Map
#ifndef QUIET_UNUSED
static 
char* mopCodes[040] = {
    //        0       1       2       3       4       5       6       7
    /* 00 */  0,     "insm", "enf",  "ses",  "mvzb", "mvza", "mfls", "mflc",
    /* 10 */ "insb", "insa", "insn", "insp", "ign",  "mvc",  "mses", "mors",
    /* 20 */ "lte",  "cht",   0,      0,      0,      0,      0,      0,
    /* 30 */   0,      0,     0,      0,      0,      0,      0,      0
};
#endif

static int mopINSM (EISstruct *);
static int mopENF  (EISstruct *);
static int mopSES  (EISstruct *);
static int mopMVZB (EISstruct *);
static int mopMVZA (EISstruct *);
static int mopMFLS (EISstruct *);
static int mopMFLC (EISstruct *);
static int mopINSB (EISstruct *);
static int mopINSA (EISstruct *);
static int mopINSN (EISstruct *);
static int mopINSP (EISstruct *);
static int mopIGN  (EISstruct *);
static int mopMVC  (EISstruct *);
static int mopMSES (EISstruct *);
static int mopMORS (EISstruct *);
static int mopLTE  (EISstruct *);
static int mopCHT(EISstruct *e);

static 
MOPstruct mopTab[040] = {
    {NULL, 0},
    {"insm", mopINSM },
    {"enf",  mopENF  },
    {"ses",  mopSES  },
    {"mvzb", mopMVZB },
    {"mvza", mopMVZA },
    {"mfls", mopMFLS },
    {"mflc", mopMFLC },
    {"insb", mopINSB },
    {"insa", mopINSA },
    {"insn", mopINSN },
    {"insp", mopINSP },
    {"ign",  mopIGN  },
    {"mvc",  mopMVC  },
    {"mses", mopMSES },
    {"mors", mopMORS },
    {"lte",  mopLTE  },
    {"cht",  mopCHT  },
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0},
    {NULL, 0}
};

static char* defaultEditInsertionTable = " *+-$,.0";


// Edit Flags
//
// The processor provides the following four edit flags for use by the micro
// operations.  
//
// bool mopES = false; // End Suppression flag; initially OFF, set ON by a
// micro operation when zero-suppression ends.
//
// bool mopSN = false; 
// Sign flag; initially set OFF if the sending string has an alphanumeric
// descriptor or an unsigned numeric descriptor. If the sending string has a
// signed numeric descriptor, the sign is initially read from the sending
// string from the digit position defined by the sign and the decimal type
// field (S); SN is set OFF if positive, ON if negative. If all digits are
// zero, the data is assumed positive and the SN flag is set OFF, even when the
// sign is negative.
//
//bool mopZ = true;
// Zero flag; initially set ON and set OFF whenever a sending string character
// that is not decimal zero is moved into the receiving string.
//
//bool mopBZ = false; i
// Blank-when-zero flag; initially set OFF and set ON by either the ENF or SES
// micro operation. If, at the completion of a move (L1 exhausted), both the Z
// and BZ flags are ON, the receiving string is filled with character 1 of the
// edit insertion table.

/*!
 * CHT Micro Operation - Change Table
 * EXPLANATION: The edit insertion table is replaced by the string of eight
 * 9-bit characters immediately following the CHT micro operation.
 * FLAGS: None affected
 * NOTE: C(IF) is not interpreted for this operation.
 */

// CANFAULT
static int mopCHT(EISstruct *e)
{
    memset(&e->editInsertionTable, 0, sizeof(e->editInsertionTable)); // XXX do we really need this?
    for(int i = 0 ; i < 8 ; i += 1)
    {
        if (e->mopTally == 0)
        {
            e->_faults |= FAULT_IPR;
            return -1;      // Oops! ran out of micro-operations!
        }
        word9 entry = EISget49(e->mopAddress, &e->mopPos, CTN9);  // get mop table entries
        e->editInsertionTable[i] = entry & 0777;            // keep to 9-bits
        
        e->mopTally -= 1;
    }
    return 0;
}

/*!
 * ENF Micro Operation - End Floating Suppression
 * EXPLANATION:
 *  Bit 0 of IF, IF(0), specifies the nature of the floating suppression. Bit 1
 *  of IF, IF(1), specifies if blank when zero option is used.
 * For IF(0) = 0 (end floating-sign operation),
 * − If ES is OFF and SN is OFF, then edit insertion table entry 3 is moved to
 * the receiving field and ES is set ON.
 * − If ES is OFF and SN is ON, then edit insertion table entry 4 is moved to
 * the receiving field and ES is set ON.
 * − If ES is ON, no action is taken.
 * For IF(0) = 1 (end floating currency symbol operation),
 * − If ES is OFF, then edit insertion table entry 5 is moved to the receiving
 * field and ES is set ON.
 * − If ES is ON, no action is taken.
 * For IF(1) = 1 (blank when zero): the BZ flag is set ON. For IF(1) = 0 (no
 * blank when zero): no action is taken.
 * FLAGS: (Flags not listed are not affected)
 *      ES - If OFF, then set ON
 *      BZ - If bit 1 of C(IF) = 1, then set ON; otherwise, unchanged
 */

// CANFAULT
static int mopENF(EISstruct *e)
{
    // For IF(0) = 0 (end floating-sign operation),
    if (!(e->mopIF & 010))
    {
        // If ES is OFF and SN is OFF, then edit insertion table entry 3 is moved to the receiving field and ES is set ON.
        if (!e->mopES && !e->mopSN)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[2]);
            e->mopES = true;
        }
        // If ES is OFF and SN is ON, then edit insertion table entry 4 is moved to the receiving field and ES is set ON.
        if (!e->mopES && e->mopSN)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[3]);
            e->mopES = true;
        }
        // If ES is ON, no action is taken.
    } else { // IF(0) = 1 (end floating currency symbol operation),
        if (!e->mopES)
        {
            // If ES is OFF, then edit insertion table entry 5 is moved to the receiving field and ES is set ON.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[4]);
            e->mopES = true;
        }
        // If ES is ON, no action is taken.
    }
    
    // For IF(1) = 1 (blank when zero): the BZ flag is set ON. For IF(1) = 0 (no blank when zero): no action is taken.
    if (e->mopIF & 04)
        e->mopBZ = true;
    
    return 0;
}

/*!
 * IGN Micro Operation - Ignore Source Characters
 * EXPLANATION:
 * IF specifies the number of characters to be ignored, where IF = 0 specifies
 * 16 characters.
 * The next IF characters in the source data field are ignored and the sending
 * tally is reduced accordingly.
 * FLAGS: None affected
 */

static int mopIGN(EISstruct *e)
{
    if (e->mopIF == 0)
        e->mopIF = 16;

    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0)
            return -1;  // sending buffer exhausted.
        
        e->srcTally -= 1;
        e->in += 1;
    }
    return 0;
}

/*!
 * INSA Micro Operation - Insert Asterisk on Suppression
 * EXPLANATION:
 * This MOP is the same as INSB except that if ES is OFF, then edit insertion
 * table entry 2 is moved to the receiving field.
 * FLAGS: None affected
 * NOTE: If C(IF) = 9-15, an IPR fault occurs.
 */

// CANFAULT
static int mopINSA(EISstruct *e)
{
    // If C(IF) = 9-15, an IPR fault occurs.
    if (e->mopIF >= 9 && e->mopIF <= 15)
    {
        e->_faults |= FAULT_IPR;
        return -1;
    }
    
    // If IF = 0, the 9 bits immediately following the INSB micro operation are
    // treated as a 9-bit character (not a MOP) and are moved or skipped
    // according to ES.
    if (e->mopIF == 0)
    {
        // If ES is OFF, then edit insertion table entry 2 is moved to the
        // receiving field. If IF = 0, then the next 9 bits are also skipped.
        // If IF is not 0, the next 9 bits are treated as a MOP.
        if (!e->mopES)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[1]);
           
            EISget49(e->mopAddress, &e->mopPos, CTN9);
            e->mopTally -= 1;
        } else {
            // If ES is ON and IF = 0, then the 9-bit character immediately
            // following the INSB micro-instruction is moved to the receiving
            // field.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, EISget49(e->mopAddress, &e->mopPos, CTN9));
            e->mopTally -= 1;
        }
        
    } else {
        // If ES is ON and IF<>0, then IF specifies which edit insertion table
        // entry (1-8) is to be moved to the receiving field.
        if (e->mopES)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[e->mopIF-1]);
        }
    }
    return 0;
}

/*!
 * INSB Micro Operation - Insert Blank on Suppression
 * EXPLANATION:
 * IF specifies which edit insertion table entry is inserted.
 * If IF = 0, the 9 bits immediately following the INSB micro operation are
 * treated as a 9-bit character (not a MOP) and are moved or skipped according
 * to ES.
 * − If ES is OFF, then edit insertion table entry 1 is moved to the receiving
 * field. If IF = 0, then the next 9 bits are also skipped. If IF is not 0, the
 * next 9 bits are treated as a MOP.
 * − If ES is ON and IF = 0, then the 9-bit character immediately following the
 * INSB micro-instruction is moved to the receiving field.
 * − If ES is ON and IF<>0, then IF specifies which edit insertion table entry
 * (1-8) is to be moved to the receiving field.
 * FLAGS: None affected
 * NOTE: If C(IF) = 9-15, an IPR fault occurs.
 */

// CANFAULT
static int mopINSB(EISstruct *e)
{
    // If C(IF) = 9-15, an IPR fault occurs.
    if (e->mopIF >= 9 && e->mopIF <= 15)
    {
        e->_faults |= FAULT_IPR;
        return -1;
    }
    
    // If IF = 0, the 9 bits immediately following the INSB micro operation are
    // treated as a 9-bit character (not a MOP) and are moved or skipped
    // according to ES.
    if (e->mopIF == 0)
    {
        // If ES is OFF, then edit insertion table entry 1 is moved to the
        // receiving field. If IF = 0, then the next 9 bits are also skipped.
        // If IF is not 0, the next 9 bits are treated as a MOP.
        if (!e->mopES)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[0]);
     
            //get49(e, &e->mopAddr, &e->mopPos, CTN9);
            EISget49(e->mopAddress, &e->mopPos, CTN9);
            e->mopTally -= 1;
        } else {
            // If ES is ON and IF = 0, then the 9-bit character immediately
            // following the INSB micro-instruction is moved to the receiving
            // field.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, EISget49(e->mopAddress, &e->mopPos, CTN9));
            e->mopTally -= 1;            
        }
      
    } else {
      // If ES is ON and IF<>0, then IF specifies which edit insertion table
      // entry (1-8) is to be moved to the receiving field.
        if (e->mopES)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[e->mopIF - 1]);
        }
    }
    return 0;
}

/*!
 * INSM Micro Operation - Insert Table Entry One Multiple
 * EXPLANATION:
 * IF specifies the number of receiving characters affected, where IF = 0
 * specifies 16 characters.
 * Edit insertion table entry 1 is moved to the next IF (1-16) receiving field
 * characters.
 * FLAGS: None affected
 */

// CANFAULT
static int mopINSM(EISstruct *e)
{
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[0]);
    }
    return 0;
}

/*!
 * INSN Micro Operation - Insert on Negative
 * EXPLANATION:
 * IF specifies which edit insertion table entry is inserted. If IF = 0, the 9
 * bits immediately following the INSN micro operation are treated as a 9-bit
 * character (not a MOP) and are moved or skipped according to SN.
 * − If SN is OFF, then edit insertion table entry 1 is moved to the receiving
 * field. If IF = 0, then the next 9 bits are also skipped. If IF is not 0, the
 * next 9 bits are treated as a MOP.
 * − If SN is ON and IF = 0, then the 9-bit character immediately following the
 * INSN micro-instruction is moved to the receiving field.
 * − If SN is ON and IF <> 0, then IF specifies which edit insertion table
 * entry (1-8) is to be moved to the receiving field.
 * FLAGS: None affected
 * NOTE: If C(IF) = 9-15, an IPR fault occurs.
 */

// CANFAULT
static int mopINSN(EISstruct *e)
{
    // If C(IF) = 9-15, an IPR fault occurs.
    if (e->mopIF >= 9 && e->mopIF <= 15)
    {
        e->_faults |= FAULT_IPR;
        return -1;
    }
    
    // If IF = 0, the 9 bits immediately following the INSN micro operation are
    // treated as a 9-bit character (not a MOP) and are moved or skipped
    // according to SN.
    
    if (e->mopIF == 0)
    {
        if (!e->mopSN)
        {
	    //If SN is OFF, then edit insertion table entry 1 is moved to the
	    //receiving field. If IF = 0, then the next 9 bits are also
	    //skipped. If IF is not 0, the next 9 bits are treated as a MOP.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[0]);
        } else {
	    // If SN is ON and IF = 0, then the 9-bit character immediately
	    // following the INSN micro-instruction is moved to the receiving
	    // field.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, EISget49(e->mopAddress, &e->mopPos, CTN9));

            e->mopTally -= 1;   // I think
        }
    }
    else
    {
        if (e->mopSN)
        {
	    //If SN is ON and IF <> 0, then IF specifies which edit insertion
	    //table entry (1-8) is to be moved to the receiving field.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[e->mopIF - 1]);
        }
    }
    return 0;
}

/*!
 * INSP Micro Operation - Insert on Positive
 * EXPLANATION:
 * INSP is the same as INSN except that the responses for the SN values are
 * reversed.
 * FLAGS: None affected
 * NOTE: If C(IF) = 9-15, an IPR fault occurs.
 */

// CANFAULT
static int mopINSP(EISstruct *e)
{
    // If C(IF) = 9-15, an IPR fault occurs.
    if (e->mopIF >= 9 && e->mopIF <= 15)
    {
        e->_faults |= FAULT_IPR;
        return -1;
    }
    
    if (e->mopIF == 0)
    {
        if (e->mopSN)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[0]);
        } else {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, EISget49(e->mopAddress, &e->mopPos, CTN9));
            e->mopTally -= 1;
        }
    }
    else
    {
        if (!e->mopSN)
        {
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[e->mopIF - 1]);
        }
    }

    return 0;
}

/*!
 * LTE Micro Operation - Load Table Entry
 * EXPLANATION:
 * IF specifies the edit insertion table entry to be replaced.
 * The edit insertion table entry specified by IF is replaced by the 9-bit
 * character immediately following the LTE microinstruction.
 * FLAGS: None affected
 * NOTE: If C(IF) = 0 or C(IF) = 9-15, an Illegal Procedure fault occurs.
 */

// CANFAULT
static int mopLTE(EISstruct *e)
{
    if (e->mopIF == 0 || (e->mopIF >= 9 && e->mopIF <= 15))
    {
        e->_faults |= FAULT_IPR;
        return -1;
    }
    word9 next = EISget49(e->mopAddress, &e->mopPos, CTN9);
    e->mopTally -= 1;
    
    e->editInsertionTable[e->mopIF - 1] = next;
    sim_debug (DBG_TRACEEXT, & cpu_dev, "LTE IT[%d]<=%d\n", e -> mopIF - 1, next);    
    return 0;
}

/*!
 * MFLC Micro Operation - Move with Floating Currency Symbol Insertion
 * EXPLANATION:
 * IF specifies the number of characters of the sending field upon which the
 * operation is performed, where IF = 0 specifies 16 characters.
 * Starting with the next available sending field character, the next IF
 * characters are individually fetched and the following conditional actions
 * occur.
 * − If ES is OFF and the character is zero, edit insertion table entry 1 is
 * moved to the receiving field in place of the character.
 * − If ES is OFF and the character is not zero, then edit insertion table
 * entry 5 is moved to the receiving field, the character is also moved to the
 * receiving field, and ES is set ON.
 * − If ES is ON, the character is moved to the receiving field.
 * The number of characters placed in the receiving field is data-dependent. If
 * the entire sending field is zero, IF characters are placed in the receiving
 * field. However, if the sending field contains a nonzero character, IF+1
 * characters (the insertion character plus the characters from the sending
 * field) are placed in the receiving field.
 * An IPR fault occurs when the sending field is exhausted before the receiving
 * field is filled. In order to provide space in the receiving field for an
 * inserted currency symbol, the receiving field must have a string length one
 * character longer than the sending field. When the sending field is all
 * zeros, no currency symbol is inserted by the MFLC micro operation and the
 * receiving field is not filled when the sending field is exhausted. The user
 * should provide an ENF (ENF,12) micro operation after a MFLC micro operation
 * that has as its character count the number of characters in the sending
 * field. The ENF micro operation is engaged only when the MFLC micro operation
 * fails to fill the receiving field. Then it supplies a currency symbol to
 * fill the receiving field and blanks out the entire field.
 * FLAGS: (Flags not listed are not affected.)
 * ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise, it
 * is unchanged.
 * NOTE: Since the number of characters moved to the receiving string is
 * data-dependent, a possible IPR fault may be avoided by ensuring that the Z
 * and BZ flags are ON.
 */

// CANFAULT
static int mopMFLC(EISstruct *e)
{
    if (e->mopIF == 0)
        e->mopIF = 16;

    //  Starting with the next available sending field character, the next IF
    //  characters are individually fetched and the following conditional
    //  actions occur.
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0 || e->dstTally == 0)
        {
            e->_faults |= FAULT_IPR;
            return -1;
        }
	// If ES is OFF and the character is zero, edit insertion table entry 1
	// is moved to the receiving field in place of the character.
	// If ES is OFF and the character is not zero, then edit insertion
	// table entry 5 is moved to the receiving field, the character is also
	// moved to the receiving field, and ES is set ON.
        
        int c = *(e->in);
        if (!e->mopES) { // e->mopES is OFF
            //if (c == 0) {
            // XXX See srcTA comment in MVNE


#ifdef V3
            if ((c & 017) == 0)
#else
            if (c == decimalZero)
#endif
                {
		// edit insertion table entry 1 is moved to the receiving field
		// in place of the character.
                writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[0]);
                e->in += 1;
                e->srcTally -= 1;
            } else {
		// then edit insertion table entry 5 is moved to the receiving
		// field, the character is also moved to the receiving field,
		// and ES is set ON.
                writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[4]);
                
                e->in += 1;
                e->srcTally -= 1;
                if (e->srcTally == 0 || e->dstTally == 0)
                {
                    e->_faults |= FAULT_IPR;
                    return -1;
                }
                
                writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
                
                e->mopES = true;
            }
        } else {
            // If ES is ON, the character is moved to the receiving field.
            writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
            
            e->in += 1;
            e->srcTally -= 1;
        }
    }
    // ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise,
    // it is unchanged.
    // NOTE: Since the number of characters moved to the receiving string is
    // data-dependent, a possible IPR fault may be avoided by ensuring that the
    // Z and BZ flags are ON.
    // XXX Not certain how to interpret either one of these!
    
    return 0;
}

/*!
 * MFLS Micro Operation - Move with Floating Sign Insertion
 * EXPLANATION:
 * IF specifies the number of characters of the sending field upon which the
 * operation is performed, where IF = 0 specifies 16 characters.
 * Starting with the next available sending field character, the next IF
 * characters are individually fetched and the following conditional actions
 * occur.
 * − If ES is OFF and the character is zero, edit insertion table entry 1 is
 * moved to the receiving field in place of the character.
 * − If ES is OFF, the character is not zero, and SN is OFF; then edit
 * insertion table entry 3 is moved to the receiving field; the character is
 * also moved to the receiving field, and ES is set ON.
 * − If ES is OFF, the character is nonzero, and SN is ON; edit insertion table
 * entry 4 is moved to the receiving field; the character is also moved to the
 * receiving field, and ES is set ON.
 * − If ES is ON, the character is moved to the receiving field.
 * The number of characters placed in the receiving field is data-dependent. If
 * the entire sending field is zero, IF characters are placed in the receiving
 * field. However, if the sending field contains a nonzero character, IF+1
 * characters (the insertion character plus the characters from the sending
 * field) are placed in the receiving field.
 * An IPR fault occurs when the sending field is exhausted before the receiving
 * field is filled. In order to provide space in the receiving field for an
 * inserted sign, the receiving field must have a string length one character
 * longer than the sending field. When the sending field is all zeros, no sign
 * is inserted by the MFLS micro operation and the receiving field is not
 * filled when the sending field is exhausted. The user should provide an ENF
 * (ENF,4) micro operation after a MFLS micro operation that has as its
 * character count the number of characters in the sending field. The ENF micro
 * operation is engaged only when the MFLS micro operation fails to fill the
 * receiving field; then, it supplies a sign character to fill the receiving
 * field and blanks out the entire field.
 *
 * FLAGS: (Flags not listed are not affected.)
 *     ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise,
 *     it is unchanged.
 * NOTE: Since the number of characters moved to the receiving string is
 * data-dependent, a possible Illegal Procedure fault may be avoided by
 * ensuring that the Z and BZ flags are ON.
 */

// CANFAULT
static int mopMFLS(EISstruct *e)
{
    if (e->mopIF == 0)
        e->mopIF = 16;
    
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0 && e->dstTally > 1)
        {
            e->_faults = FAULT_IPR;
            return -1;
        }
        
        int c = *(e->in);
        sim_debug (DBG_TRACEEXT, & cpu_dev, "MFLS n %d c %o\n", n, c);
        if (!e->mopES) { // e->mopES is OFF
            //if (c == 0) {
            // XXX See srcTA comment in MVNE
#ifdef V3
            if ((c & 017) == 0)
#else
            if (c == decimalZero)
#endif
            {
		// edit insertion table entry 1 is moved to the receiving field
		// in place of the character.
                sim_debug (DBG_TRACEEXT, & cpu_dev, "ES is off, c is zero; edit insertion table entry 1 is moved to the receiving field in place of the character.\n");
                writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[0]);
                e->in += 1;
                e->srcTally -= 1;
            } else {
                // c is non-zero
                if (!e->mopSN)
                {
		    // then edit insertion table entry 3 is moved to the
		    // receiving field; the character is also moved to the
		    // receiving field, and ES is set ON.
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "ES is off, c is non-zero, SN is off; edit insertion table entry 3 is moved to the receiving field; the character is also moved to the receiving field, and ES is set ON.\n");
                    writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[2]);

                    e->in += 1;
                    e->srcTally -= 1;
                    if (e->srcTally == 0 && e->dstTally > 1)
                    {
                        e->_faults |= FAULT_IPR;
                        return -1;
                    }
                    
                    writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);

                    e->mopES = true;
                } else {
		    //  SN is ON; edit insertion table entry 4 is moved to the
		    //  receiving field; the character is also moved to the
		    //  receiving field, and ES is set ON.
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "ES is off, c is non-zero, SN is OFF; edit insertion table entry 4 is moved to the receiving field; the character is also moved to the receiving field, and ES is set ON.\n");
                    writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[3]);
                    
                    e->in += 1;
                    e->srcTally -= 1;
                    if (e->srcTally == 0 && e->dstTally > 1)
                    {
                        e->_faults |= FAULT_IPR;
                        return -1;
                    }
                    
                    writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
                    
                    e->mopES = true;
                }
            }
        } else {
            // If ES is ON, the character is moved to the receiving field.
            sim_debug (DBG_TRACEEXT, & cpu_dev, "ES is ON, the character is moved to the receiving field.\n");
            writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
            
            e->in += 1;
            e->srcTally -= 1;
        }
    }
    
    // ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise,
    // it is unchanged.
    // NOTE: Since the number of characters moved to the receiving string is
    // data-dependent, a possible Illegal Procedure fault may be avoided by
    // ensuring that the Z and BZ flags are ON.

    // XXX Have no idea how to interpret either one of these statements.
    
    return 0;
}

/*!
 * MORS Micro Operation - Move and OR Sign
 * EXPLANATION:
 * IF specifies the number of characters of the sending field upon which the
 * operation is performed, where IF = 0 specifies 16 characters.
 * Starting with the next available sending field character, the next IF
 * characters are individually fetched and the following conditional actions
 * occur.
 * − If SN is OFF, the next IF characters in the source data field are moved to
 * the receiving data field and, during the move, edit insertion table entry 3
 * is ORed to each character.
 * − If SN is ON, the next IF characters in the source data field are moved to
 * the receiving data field and, during the move, edit insertion table entry 4
 * is ORed to each character.
 * MORS can be used to generate a negative overpunch for a receiving field to
 * be used later as a sending field.
 * FLAGS: None affected
 */

// CANFAULT
static int mopMORS(EISstruct *e)
{
    if (e->mopIF == 0)
        e->mopIF = 16;
    
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0 || e->dstTally == 0)
        {
            e->_faults |= FAULT_IPR;
            return -1;
        }
        
        // XXX this is probably wrong regarding the ORing, but it's a start ....
        int c = (*e->in | (!e->mopSN ? e->editInsertionTable[2] : e->editInsertionTable[3]));
        e->in += 1;
        e->srcTally -= 1;
        
        writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
    }

    return 0;
}

/*!
 * MSES Micro Operation - Move and Set Sign
 * EXPLANATION:
 * IF specifies the number of characters of the sending field upon which the
 * operation is performed, where IF = 0 specifies 16 characters. For MVE,
 * starting with the next available sending field character, the next IF
 * characters are individually fetched and the following conditional actions
 * occur.
 * Starting with the first character during the move, a comparative AND is made
 * first with edit insertion table entry 3. If the result is nonzero, the first
 * character and the rest of the characters are moved without further
 * comparative ANDs. If the result is zero, a comparative AND is made between
 * the character being moved and edit insertion table entry 4 If that result is
 * nonzero, the SN indicator is set ON (indicating negative) and the first
 * character and the rest of the characters are moved without further
 * comparative ANDs. If the result is zero, the second character is treated
 * like the first. This process continues until one of the comparative AND
 * results is nonzero or until all characters are moved.
 * For MVNE instruction, the sign (SN) flag is already set and IF characters
 * are moved to the destination field (MSES is equivalent to the MVC
 * instruction).
 * FLAGS: (Flags not listed are not affected.)
 * SN If edit insertion table entry 4 is found in C(Y-1), then ON; otherwise,
 * it is unchanged.
 */

// CANFAULT
static int mopMSES(EISstruct *e)
{
    if (e->mvne == true)
        return mopMVC(e);   // XXX I think!
        
        
    if (e->mopIF == 0)
        e->mopIF = 16;
    
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0 || e->dstTally == 0)
        {
            e->_faults |= FAULT_IPR;
            return -1;
        }
        
	//Starting with the first character during the move, a comparative AND
	//is made first with edit insertion table entry 3. If the result is
	//nonzero, the first character and the rest of the characters are moved
	//without further comparative ANDs. If the result is zero, a
	//comparative AND is made between the character being moved and edit
	//insertion table entry 4 If that result is nonzero, the SN indicator
	//is set ON (indicating negative) and the first character and the rest
	//of the characters are moved without further comparative ANDs. If the
	//result is zero, the second character is treated like the first. This
	//process continues until one of the comparative AND results is nonzero
	//or until all characters are moved.
        
        int c = *(e->in);

        // a comparative AND is made first with edit insertion table entry 3.
        int cmpAnd = (c & e->editInsertionTable[2]);  // only lower 4-bits are considered
	//If the result is nonzero, the first character and the rest of the
	//characters are moved without further comparative ANDs.
        if (cmpAnd)
        {
            for(int n2 = n ; n2 < e->mopIF ; n2 += 1)
            {
                if (e->srcTally == 0 || e->dstTally == 0)
                {
                    e->_faults |= FAULT_IPR;
                    return -1;
                }
                writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, *e->in);
                e->in += 1;
            
                e->srcTally -= 1;
            }
            return 0;
        }
        
	//If the result is zero, a comparative AND is made between the
	//character being moved and edit insertion table entry 4 If that result
	//is nonzero, the SN indicator is set ON (indicating negative) and the
	//first character and the rest of the characters are moved without
	//further comparative ANDs.
        
        cmpAnd = (c & e->editInsertionTable[3]);  // XXX only lower 4-bits are considered
        if (cmpAnd)
        {
            e->mopSN = true;
            for(int n2 = n ; n2 < e->mopIF ; n2 += 1)
            {
                if (e->srcTally == 0 || e->dstTally == 0)
                {
                    e->_faults |= FAULT_IPR;
                    return -1;
                }
                writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, *e->in);

                e->in += 1;
                e->srcTally -= 1;
            }
            return 0;
        }
	//If the result is zero, the second character is treated like the
	//first. This process continues until one of the comparative AND
	//results is nonzero or until all characters are moved.
        e->in += 1;
        e->srcTally -= 1;   // XXX is this correct? No chars have been consumed, but ......
    }
    
    return 0;
}

/*!
 * MVC Micro Operation - Move Source Characters
 * EXPLANATION:
 * IF specifies the number of characters to be moved, where IF = 0 specifies 16
 * characters.
 * The next IF characters in the source data field are moved to the receiving
 * data field.
 * FLAGS: None affected
 */

// CANFAULT
static int mopMVC(EISstruct *e)
{
    if (e->mopIF == 0)
        e->mopIF = 16;
    
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0 || e->dstTally == 0)
        {
            e->_faults |= FAULT_IPR;
            return -1;
        }
        
        writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, *e->in);
        e->in += 1;
        
        e->srcTally -= 1;
    }
    
    return 0;
}

/*!
 * MVZA Micro Operation - Move with Zero Suppression and Asterisk Replacement
 * EXPLANATION:
 * MVZA is the same as MVZB except that if ES is OFF and the character is zero,
 * then edit insertion table entry 2 is moved to the receiving field.
 * FLAGS: (Flags not listed are not affected.)
 * ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise, it
 * is unchanged.
 */

// CANFAULT
static int mopMVZA(EISstruct *e)
{
    if (e->mopIF == 0)
        e->mopIF = 16;
    
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0 || e->dstTally == 0)
        {
            e->_faults |= FAULT_IPR;
            return -1;
        }
        
        int c = *e->in;
        e->in += 1;
        e->srcTally -= 1;
        
        //if (!e->mopES && c == 0)
        // XXX See srcTA comment in MVNE
#ifdef V3
        if (!e->mopES && (c & 017) == 0)
#else
        if (!e->mopES && c == decimalZero)
#endif
        {
	    //If ES is OFF and the character is zero, then edit insertion table
	    //entry 2 is moved to the receiving field in place of the
	    //character.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[1]);
        //} else if (!e->mopES && c != 0)
        // XXX See srcTA comment in MVNE
        }
#ifdef V3
        else if (!e->mopES && (c & 017) != 0)
#else
        else if (!e->mopES && c != decimalZero)
#endif
        {
	    //If ES is OFF and the character is not zero, then the character is
	    //moved to the receiving field and ES is set ON.
            writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
            
            e->mopES = true;
        } else if (e->mopES)
        {
	    //If ES is ON, the character is moved to the receiving field.
            writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
        }
    }
    
    // XXX have no idea how to interpret this
    // ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise,
    // it is unchanged.

    return 0;
}

/*!
 * MVZB Micro Operation - Move with Zero Suppression and Blank Replacement
 * EXPLANATION:
 * IF specifies the number of characters of the sending field upon which the
 * operation is performed, where IF = 0 specifies 16 characters.
 * Starting with the next available sending field character, the next IF
 * characters are individually fetched and the following conditional actions
 * occur.
 * − If ES is OFF and the character is zero, then edit insertion table entry 1
 * is moved to the receiving field in place of the character.
 * − If ES is OFF and the character is not zero, then the character is moved to
 * the receiving field and ES is set ON.
 * − If ES is ON, the character is moved to the receiving field. 
 * FLAGS: (Flags not listed are not affected.)
 *   ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise,
 *   it is unchanged.
 */

// CANFAULT
static int mopMVZB(EISstruct *e)
{
    if (e->mopIF == 0)
        e->mopIF = 16;
    
    for(int n = 0 ; n < e->mopIF ; n += 1)
    {
        if (e->srcTally == 0 || e->dstTally == 0)
        {
            e->_faults |= FAULT_IPR;
            return -1;
        }
        
        int c = *e->in;
        e->srcTally -= 1;
        e->in += 1;
        
        //if (!e->mopES && c == 0)
        // XXX See srcTA comment in MVNE
#ifdef V3
        if (!e->mopES && (c & 017) == 0)
#else
        if (!e->mopES && c == decimalZero)
#endif
        {
	    //If ES is OFF and the character is zero, then edit insertion table
	    //entry 1 is moved to the receiving field in place of the
	    //character.
            writeToOutputBuffer(e, &e->out, 9, e->dstSZ, e->editInsertionTable[0]);
        //} else if (!e->mopES && c != 0)
        // XXX See srcTA comment in MVNE
        }
#ifdef V3
        else if (!e->mopES && (c & 017) != 0)
#else
        else if (!e->mopES && c != decimalZero)
#endif
        {
	    //If ES is OFF and the character is not zero, then the character is
	    //moved to the receiving field and ES is set ON.
            writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
            
            e->mopES = true;
        } else if (e->mopES)
        {
            //If ES is ON, the character is moved to the receiving field.
            writeToOutputBuffer(e, &e->out, e->srcSZ, e->dstSZ, c);
        }
    }

    // XXX have no idea how to interpret this......
    // ES If OFF and any of C(Y) is less than decimal zero, then ON; otherwise,
    // it is unchanged.
    return 0;
}

/*!
 * SES Micro Operation - Set End Suppression
 * EXPLANATION:
 * Bit 0 of IF (IF(0)) specifies the setting of the ES switch.
 * If IF(0) = 0, the ES flag is set OFF. If IF(0) = 1, the ES flag is set ON.
 * Bit 1 of IF (IF(1)) specifies the setting of the blank-when-zero option.
 * If IF(1) = 0, no action is taken.
 * If IF(1) = 1, the BZ flag is set ON.
 * FLAGS: (Flags not listed are not affected.)
 * ES set by this micro operation
 * BZ If bit 1 of C(IF) = 1, then ON; otherwise, it is unchanged.
 */

static int mopSES(EISstruct *e)
{
    if (e->mopIF & 010)
        e->mopES = true;
    else
        e->mopES = false;
    
    if (e->mopIF & 04)
        e->mopBZ = true;
    
    return 0;
}

/*!
 * fetch MOP from e->mopAddr/e->mopCN ...
 */

// CANFAULT
static MOPstruct* EISgetMop(EISstruct *e)
{
    //static word18 lastAddress;  // try to keep memory access' down
    //static word36 data;
    
    
    if (e == NULL)
    //{
    //    p->lastAddress = -1;
    //    p->data = 0;
        return NULL;
    //}
   
    EISaddr *p = e->mopAddress;
    
    //if (p->lastAddress != p->address)                 // read from memory if different address
        p->data = EISRead(p);   // read data word from memory
    
    if (e->mopPos > 3)   // overflows to next word?
    {   // yep....
        e->mopPos = 0;   // reset to 1st byte
        e->mopAddress->address = (e->mopAddress->address + 1) & AMASK;     // bump source to next address
        p->data = EISRead(e->mopAddress);   // read it from memory
    }
    
    e->mop9  = (word9)get9(p->data, e->mopPos);       // get 9-bit mop
    e->mop   = (e->mop9 >> 4) & 037;
    e->mopIF = e->mop9 & 0xf;
    
    MOPstruct *m = &mopTab[e->mop];
    sim_debug (DBG_TRACEEXT, & cpu_dev, "MOP %s\n", m -> mopName);
    e->m = m;
    if (e->m == NULL || e->m->f == NULL)
    {
        sim_printf ("getMop(e->m == NULL || e->m->f == NULL): mop:%d IF:%d\n", e->mop, e->mopIF);
        return NULL;
    }
    
    e->mopPos += 1;
    e->mopTally -= 1;
    
    //p->lastAddress = p->address;
    
    return m;
}

/*!
 * This is the Micro Operation Executor/Interpreter
 */

// CANFAULT
static void mopExecutor(EISstruct *e, int kMop)
{
    //e->mopAddr = e->YChar9[kMop-1];    // get address of microoperations
    e->mopAddress = &e->addr[kMop-1];
    e->mopTally = e->N[kMop-1];        // number of micro-ops
    e->mopCN   = e->CN[kMop-1];        // starting at char pos CN
    e->mopPos  = e->mopCN;
    
    word9 *p9 = e->editInsertionTable; // re-initilize edit insertion table
    char *q = defaultEditInsertionTable;
    while((*p9++ = *q++))
        ;
    
    e->in = e->inBuffer;    // reset input buffer pointer
    e->out = e->outBuffer;  // reset output buffer pointer
    e->outPos = 0;
    
    e->_faults = 0; // No faults (yet!)
    
    //getMop(NULL);   // initialize mop getter
    EISgetMop(NULL);   // initialize mop getter
    //get49(NULL, 0, 0, 0); // initialize byte getter

    // execute dstTally micro operations
    // The micro operation sequence is terminated normally when the receiving
    // string length becomes exhausted. The micro operation sequence is
    // terminated abnormally (with an illegal procedure fault) if a move from
    // an exhausted sending string or the use of an exhausted MOP string is
    // attempted.
    
    //sim_printf ("(I) mopTally=%d srcTally=%d\n", e->mopTally, e->srcTally);

    //while (e->mopTally && e->srcTally && e->dstTally)
    while (e->dstTally && e->mopTally)
    {
        MOPstruct *m = EISgetMop(e);
        
        int mres = m->f(e);    // execute mop
        if (mres)
            break;        
    }
    
    // XXX this stuff should probably best be done in the mop's themselves. We'll see.
    if (e->dstTally == 0)  // normal termination
        return;
   
    // mop string exhausted?
    if (e->mopTally != 0)
        e->_faults |= FAULT_IPR;   // XXX ill proc fault
    
    if (e->srcTally != 0)  // sending string exhausted?
        e->_faults |= FAULT_IPR;   // XXX ill proc fault
}

// CANFAULT
void mvne(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseAlphanumericOperandDescriptor(2, e, 2);
    parseAlphanumericOperandDescriptor(3, e, 3);
    
    // initialize mop flags. Probably best done elsewhere.
    e->mopES = false; // End Suppression flag
    e->mopSN = false; // Sign flag
    e->mopZ  = true;  // Zero flag
    e->mopBZ = false; // Blank-when-zero flag
    
    e->srcTally = e->N1;  // number of chars in src (max 63)
    e->dstTally = e->N3;  // number of chars in dst (max 63)
    
    e->srcTN = e->TN1;    // type of chars in src

#if defined(V1) || defined(V2)
// XXX Temp hack to get MOP to work. Merge TA/TN?
// The MOP operators look at srcTA to make 9bit/not 9-bit decisions about
// the contents of inBuffer; parseNumericOperandDescriptor() always puts
// 4 bit data in inBuffer, so signal the MOPS code of that.
    e->srcTA = CTA4;    // type of chars in src
#endif

    e->srcCN = e->CN1;    // starting at char pos CN
    switch(e->srcTN)
    {
        case CTN4:
            //e->srcAddr = e->YChar41;
            e->srcSZ = 4;   // stored as 4-bit decimals
            break;
        case CTN9:
            //e->srcAddr = e->YChar91;
            e->srcSZ = 4;   // 'cause everything is stored as 4-bit decimals
            break;
    }

    e->dstTA = e->TA3;    // type of chars in dst
    e->dstCN = e->CN3;    // starting at char pos CN
    switch(e->dstTA)
    {
        case CTA4:
            //e->dstAddr = e->YChar43;
            e->dstSZ = 4;
            break;
        case CTA6:
            //e->dstAddr = e->YChar63;
            e->dstSZ = 6;
            break;
        case CTA9:
            //e->dstAddr = e->YChar93;
            e->dstSZ = 9;
            break;
    }

    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "mvne N1 %d N2 %d N3 %d TN1 %d CN1 %d TA3 %d CN3 %d\n",
      e->N1, e->N2, e->N3, e->TN1, e->CN1, e->TA3, e->CN3);

    // 1. load sending string into inputBuffer
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
    // 2. Test sign and, if required, set the SN flag. (Sign flag; initially set OFF if the sending string has an alphanumeric descriptor or an unsigned numeric descriptor. If the sending string has a signed numeric descriptor, the sign is initially read from the sending string from the digit position defined by the sign and the decimal type field (S); SN is set OFF if positive, ON if negative. If all digits are zero, the data is assumed positive and the SN flag is set OFF, even when the sign is negative.)

    int sum = 0;
    for(int n = 0 ; n < e->srcTally ; n += 1)
        sum += e->inBuffer[n];
    if ((e->sign == -1) && sum)
        e->mopSN = true;
    
    // 3. Execute micro operation string, starting with first (4-bit) digit.
    e->mvne = true;
    
    mopExecutor(e, 2);

    e->dstTally = e->N3;  // restore dstTally for output
    
    EISwriteOutputBufferToMemory(&e->ADDR3);
}

// CANFAULT
void mve(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 2);
    parseAlphanumericOperandDescriptor(3, e, 3);
    
    // initialize mop flags. Probably best done elsewhere.
    e->mopES = false; // End Suppression flag
    e->mopSN = false; // Sign flag
    e->mopZ  = true;  // Zero flag
    e->mopBZ = false; // Blank-when-zero flag
    
    e->srcTally = e->N1;  // number of chars in src (max 63)
    e->dstTally = e->N3;  // number of chars in dst (max 63)
    
    e->srcTA = e->TA1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
    switch(e->srcTA)
    {
        case CTA4:
            //e->srcAddr = e->YChar41;
            e->srcSZ = 4;
            break;
        case CTA6:
            //e->srcAddr = e->YChar61;
            e->srcSZ = 6;
            break;
        case CTA9:
            //e->srcAddr = e->YChar91;
            e->srcSZ = 9;
            break;
    }
    
    e->dstTA = e->TA3;    // type of chars in dst
    e->dstCN = e->CN3;    // starting at char pos CN
    switch(e->dstTA)
    {
        case CTA4:
            //e->dstAddr = e->YChar43;
            e->dstSZ = 4;
            break;
        case CTA6:
            //e->dstAddr = e->YChar63;
            e->dstSZ = 6;
            break;
        case CTA9:
            //e->dstAddr = e->YChar93;
            e->dstSZ = 9;
            break;
    }
    
    // 1. load sending string into inputBuffer
    EISloadInputBufferAlphnumeric(e, 1);   // according to MF1
    
    // 2. Execute micro operation string, starting with first (4-bit) digit.
    e->mvne = false;
    
    mopExecutor(e, 2);
    
    e->dstTally = e->N3;  // restore dstTally for output
    
    EISwriteOutputBufferToMemory(&e->ADDR3);
}

/*!
 * does 6-bit char represent a GEBCD negative overpuch? if so, whice numeral?
 * Refer to Bull NovaScale 9000 RJ78 Rev2 p11-178
 */
static bool isOvp(int c, int *on)
{
    // look for GEBCD -' 'A B C D E F G H I (positive overpunch)
    // look for GEBCD - ^ J K L M N O P Q R (negative overpunch)
    
    int c2 = c & 077;   // keep to 6-bits
    *on = 0;
    
    if (c2 >= 020 && c2 <= 031)            // positive overpunch
    {
        *on = c2 - 020;                    // return GEBCD number 0-9 (020 is +0)
        return false;                      // well, it's not a negative overpunch is it?
    }
    if (c2 >= 040 && c2 <= 052)            // negative overpunch
    {
        *on = c2 - 040;  // return GEBCD number 0-9 (052 is just a '-' interpreted as -0)
        return true;
    }
    return false;
}

/*!
 * MLR - Move Alphanumeric Left to Right
 *
 * (Nice, simple instruction if it weren't for the stupid overpunch stuff that ruined it!!!!)
 */
// CANFAULT
void mlr(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., minimum (N1,N2)
    //     C(Y-charn1)N1-i → C(Y-charn2)N2-i
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // Indicators: Truncation. If N1 > N2 then ON; otherwise OFF
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 2);
    
    e->srcCN = e->CN1;    // starting at char pos CN
    e->dstCN = e->CN2;    // starting at char pos CN
    
    switch(e->TA1)
    {
        case CTA4:
            //e->srcAddr = e->YChar41;
            e->srcSZ = 4;
            break;
        case CTA6:
            //e->srcAddr = e->YChar61;
            e->srcSZ = 6;
            break;
        case CTA9:
            //e->srcAddr = e->YChar91;
            e->srcSZ = 9;
            break;
    }
    
    switch(e->TA2)
    {
        case CTA4:
            //e->dstAddr = e->YChar42;
            e->dstSZ = 4;
            break;
        case CTA6:
            //e->dstAddr = e->YChar62;
            e->dstSZ = 6;
            break;
        case CTA9:
            //e->dstAddr = e->YChar92;
            e->dstSZ = 9;
            break;
    }
    
    e->T = bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    
    int fill = (int)bitfieldExtract36(e->op0, 27, 9);
    int fillT = fill;  // possibly truncated fill pattern
    // play with fill if we need to use it
    switch(e->dstSZ)
    {
        case 4:
            fillT = fill & 017;    // truncate upper 5-bits
            break;
        case 6:
            fillT = fill & 077;    // truncate upper 3-bits
            break;
    }
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, 
      "%s srcCN:%d dstCN:%d srcSZ:%d dstSZ:%d T:%d fill:%03o/%03o N1:%d N2:%d ADDR1:%06o ADDR2:%06o\n",
      __func__, e -> srcCN, e -> dstCN, e -> srcSZ, e -> dstSZ, e -> T,
      fill, fillT, e -> N1, e -> N2, e->ADDR1.address, e->ADDR2.address);

    // If N1 > N2, then (N1-N2) leading characters of C(Y-charn1) are not moved and the truncation indicator is set ON.
    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL characters are high-order truncated as they are moved to C(Y-charn2). No character conversion takes place.
    //The user of string replication or overlaying is warned that the decimal unit addresses the main memory in unaligned (not on modulo 8 boundary) units of Y-block8 words and that the overlayed string, C(Y-charn2), is not returned to main memory until the unit of Y-block8 words is filled or the instruction completes.
    //If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
    //Attempted execution with the xed instruction causes an illegal procedure fault.
    //Attempted repetition with the rpt, rpd, or rpl instructions causes an illegal procedure fault.
    
    /// XXX when do we do a truncation fault?
    
    SCF(e->N1 > e->N2, cu.IR, I_TRUNC);
    if (e->N1 > e->N2 && e -> T)
      doFault(overflow_fault, 0, "mlr truncation fault");
    
    bool ovp = (e->N1 < e->N2) && (fill & 0400) && (e->TA1 == 1) && (e->TA2 == 2); // (6-4 move)
    int on;     // number overpunch represents (if any)
    bool bOvp = false;  // true when a negative overpunch character has been found @ N1-1 

    //get469(NULL, 0, 0, 0);    // initialize char getter buffer
    
    for(uint i = 0 ; i < min(e->N1, e->N2); i += 1)
    {
        //int c = get469(e, &e->srcAddr, &e->srcCN, e->TA1); // get src char
        int c = EISget469(&e->ADDR1, &e->srcCN, e->TA1); // get src char
        int cout = 0;
        
        if (e->TA1 == e->TA2) 
            //write469(e, &e->dstAddr, &e->dstCN, e->TA1, c);
            EISwrite469(&e->ADDR2, &e->dstCN, e->TA1, c);
        else
        {
            // If data types are dissimilar (TA1 ≠ TA2), each character is high-order truncated or zero filled, as appropriate, as it is moved. No character conversion takes place.
            cout = c;
            switch (e->srcSZ)
            {
                case 6:
                    switch(e->dstSZ)
                    {
                        case 4:
                            cout = c & 017;    // truncate upper 2-bits
                            break;
                        case 9:
                            break;              // should already be 0-filled
                    }
                    break;
                case 9:
                    switch(e->dstSZ)
                    {
                        case 4:
                            cout = c & 017;    // truncate upper 5-bits
                            break;
                        case 6:
                            cout = c & 077;    // truncate upper 3-bits
                            break;
                    }
                    break;
            }

            // If N1 < N2, C(FILL)0 = 1, TA1 = 1, and TA2 = 2 (6-4 move), then C(Y-charn1)N1-1 is examined for a GBCD overpunch sign. If a negative overpunch sign is found, then the minus sign character is placed in C(Y-charn2)N2-1; otherwise, a plus sign character is placed in C(Y-charn2)N2-1.
            
            if (ovp && (i == e->N1-1))
            {
                // this is kind of wierd. I guess that C(FILL)0 = 1 means that there *is* an overpunch char here.
                bOvp = isOvp(c, &on);
                cout = on;      // replace char with the digit the overpunch represents
            }
            //write469(e, &e->dstAddr, &e->dstCN, e->TA2, cout);
            EISwrite469(&e->ADDR2, &e->dstCN, e->TA2, cout);
        }
    }
    
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL characters are high-order truncated as they are moved to C(Y-charn2). No character conversion takes place.

    if (e->N1 < e->N2)
    {
        for(uint i = e->N1 ; i < e->N2 ; i += 1)
            if (ovp && (i == e->N2-1))    // if there's an overpunch then the sign will be the last of the fill
            {
                if (bOvp)   // is c an GEBCD negative overpunch? and of what?
                    //write469(e, &e->dstAddr, &e->dstCN, e->TA2, 015);  // 015 is decimal -
                    EISwrite469(&e->ADDR2, &e->dstCN, e->TA2, 015);  // 015 is decimal -
                else
                    //write469(e, &e->dstAddr, &e->dstCN, e->TA2, 014);  // 014 is decimal +
                    EISwrite469(&e->ADDR2, &e->dstCN, e->TA2, 014);  // 014 is decimal +
            }
            else
                //write469(e, &e->dstAddr, &e->dstCN, e->TA2, fillT);
                EISwrite469(&e->ADDR2, &e->dstCN, e->TA2, fillT);
    }
}

/*
 * return CN (char position) and word offset given:
 *  'n' # of chars/bytes/bits, etc ....
 *  'initPos' initial char position (CN)
 *  'ta' alphanumeric type (if ta < 0 then size = abs(ta))
 */
static void getOffsets(int n, int initCN, int ta, int *nWords, int *newCN)
{
    int maxPos = 0;
    
    int size = 0;
    if (ta < 0)
        size = -ta;
    else
        switch (ta)
        {
            case CTA4:
                size = 4;
                maxPos = 8;
                break;
            case CTA6:
                size = 6;
                maxPos = 6;
                break;
            case CTA9:
                size = 9;
                maxPos = 4;
                break;
        }
    
    int chunk = size * maxPos; // 32 or 36
    int numBits = size * n;
    
    int numWords =  (numBits + (initCN * size) + (chunk - 1)) / chunk;  ///< how many additional words will the N chars take up?
    int lastWordOffset = numWords - 1;
                                                                            ///< (remember there are 4 slop bits in a 36-bit word when dealing with BCD)
    int lastChar = (initCN + n - 1) % maxPos;       ///< last character number
    
    if (lastWordOffset > 0)          // more that the 1 word needed?
        *nWords = lastWordOffset;  // # of additional words
    else
        *nWords = 0;           // no additional words needed
    *newCN = lastChar;         // last character number
}

/*!
 * MRL - Move Alphanumeric Right to Left
 *
 * (Like MLR, nice, simple instruction if it weren't for the stupid overpunch stuff that ruined it!!!!)
 */
// CANFAULT
void mrl(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., minimum (N1,N2)
    //   C(Y-charn1)N1-i → C(Y-charn2)N2-i
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //   C(FILL) → C(Y-charn2)N2-i
  
    // Indicators: Truncation. If N1 > N2 then ON; otherwise OFF
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 2);
    
    e->srcCN = e->CN1;    // starting at char pos CN
    e->dstCN = e->CN2;    // starting at char pos CN
    
    e->srcTA = e->TA1;
    e->dstTA = e->TA2;
    
    switch(e->TA1)
    {
        case CTA4:
            //e->srcAddr = e->YChar41;
            e->srcSZ = 4;
            break;
        case CTA6:
            //e->srcAddr = e->YChar61;
            e->srcSZ = 6;
            break;
        case CTA9:
            //e->srcAddr = e->YChar91;
            e->srcSZ = 9;
            break;
    }
    
    switch(e->TA2)
    {
        case CTA4:
            //e->dstAddr = e->YChar42;
            e->dstSZ = 4;
            break;
        case CTA6:
            //e->dstAddr = e->YChar62;
            e->dstSZ = 6;
            break;
        case CTA9:
            //e->dstAddr = e->YChar92;
            e->dstSZ = 9;
            break;
    }
    
    // adjust addresses & offsets for writing in reverse ....
    int nSrcWords = 0;
    int newSrcCN = 0;
    getOffsets(e->N1, e->srcCN, e->srcTA, &nSrcWords, &newSrcCN);

    int nDstWords = 0;
    int newDstCN = 0;
    getOffsets(e->N2, e->dstCN, e->dstTA, &nDstWords, &newDstCN);

    //word18 newSrcAddr = e->srcAddr + nSrcWords;
    //word18 newDstAddr = e->dstAddr + nDstWords;
    e->ADDR1.address += nSrcWords;
    e->ADDR2.address += nDstWords;
    
    e->T = bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    
    int fill = (int)bitfieldExtract36(e->op0, 27, 9);
    int fillT = fill;  // possibly truncated fill pattern
    
    switch(e->dstSZ)
    {
        case 4:
            fillT = fill & 017;    // truncate upper 5-bits
            break;
        case 6:
            fillT = fill & 077;    // truncate upper 3-bits
            break;
    }
    
    // If N1 > N2, then (N1-N2) leading characters of C(Y-charn1) are not moved and the truncation indicator is set ON.
    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL characters are high-order truncated as they are moved to C(Y-charn2). No character conversion takes place.
    //The user of string replication or overlaying is warned that the decimal unit addresses the main memory in unaligned (not on modulo 8 boundary) units of Y-block8 words and that the overlayed string, C(Y-charn2), is not returned to main memory until the unit of Y-block8 words is filled or the instruction completes.
    //If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
    //Attempted execution with the xed instruction causes an illegal procedure fault.
    //Attempted repetition with the rpt, rpd, or rpl instructions causes an illegal procedure fault.
    
    /// XXX when do we do a truncation fault?
    
    SCF(e->N1 > e->N2, cu.IR, I_TRUNC);
    if (e->N1 > e->N2 && e -> T)
      doFault(overflow_fault, 0, "mrl truncation fault");
    
    bool ovp = (e->N1 < e->N2) && (fill & 0400) && (e->TA1 == 1) && (e->TA2 == 2); // (6-4 move)
    int on;     // number overpunch represents (if any)
    bool bOvp = false;  // true when a negative overpunch character has been found @ N1-1
   
    //get469r(NULL, 0, 0, 0);    // initialize char getter buffer
    
    for(uint i = 0 ; i < min(e->N1, e->N2); i += 1)
    {
        //int c = get469r(e, &newSrcAddr, &newSrcCN, e->srcTA); // get src char
        int c = EISget469r(&e->ADDR1, &newSrcCN, e->srcTA); // get src char
        int cout = 0;
        
        if (e->TA1 == e->TA2)
            EISwrite469r(&e->ADDR2, &newDstCN, e->dstTA, c);
        else
        {
            // If data types are dissimilar (TA1 ≠ TA2), each character is high-order truncated or zero filled, as appropriate, as it is moved. No character conversion takes place.
            cout = c;
            switch (e->srcSZ)
            {
                case 6:
                    switch(e->dstSZ)
                    {
                        case 4:
                            cout = c & 017;    // truncate upper 2-bits
                            break;
                        case 9:
                            break;              // should already be 0-filled
                    }
                    break;
                case 9:
                    switch(e->dstSZ)
                    {
                        case 4:
                            cout = c & 017;    // truncate upper 5-bits
                            break;
                        case 6:
                            cout = c & 077;    // truncate upper 3-bits
                            break;
                    }
                    break;
            }
            
            // If N1 < N2, C(FILL)0 = 1, TA1 = 1, and TA2 = 2 (6-4 move), then C(Y-charn1)N1-1 is examined for a GBCD overpunch sign. If a negative overpunch sign is found, then the minus sign character is placed in C(Y-charn2)N2-1; otherwise, a plus sign character is placed in C(Y-charn2)N2-1.
            
            //if (ovp && (i == e->N1-1))
            if (ovp && (i == 0))    // since we're going backwards, we actually test the 1st char for overpunch
            {
                // this is kind of wierd. I guess that C(FILL)0 = 1 means that there *is* an overpunch char here.
                bOvp = isOvp(c, &on);
                cout = on;      // replace char with the digit the overpunch represents
            }
            EISwrite469r(&e->ADDR2, &newDstCN, e->dstTA, cout);
        }
    }
    
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL characters are high-order truncated as they are moved to C(Y-charn2). No character conversion takes place.
    
    if (e->N1 < e->N2)
    {
        for(uint i = e->N1 ; i < e->N2 ; i += 1)
            if (ovp && (i == e->N2-1))    // if there's an overpunch then the sign will be the last of the fill
            {
                if (bOvp)   // is c an GEBCD negative overpunch? and of what?
                    EISwrite469r(&e->ADDR2, &newDstCN, e->dstTA, 015);  // 015 is decimal -
                else
                    EISwrite469r(&e->ADDR2, &newDstCN, e->dstTA, 014);  // 014 is decimal +
            }
            else
                EISwrite469r(&e->ADDR2, &newDstCN, e->dstTA, fillT);
    }
}

static word9 xlate(word36 *xlatTbl, int dstTA, int c)
{
    int idx = (c / 4) & 0177;      // max 128-words (7-bit index)
    word36 entry = xlatTbl[idx];
    
    int pos9 = c % 4;      // lower 2-bits
    unsigned int cout = (int)GETBYTE(entry, pos9);
    //int cout = getByte(pos9, entry);
    switch(dstTA)
    {
        case CTA4:
            return cout & 017;
        case CTA6:
            return cout & 077;
        case CTA9:
            return cout;
    }

    return 0;
}

/*  
 * MVT - Move Alphanumeric with Translation
 */
// CANFAULT
void mvt(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., minimum (N1,N2)
    //    m = C(Y-charn1)i-1
    //    C(Y-char93)m → C(Y-charn2)i-1
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    m = C(FILL)
    //    C(Y-char93)m → C(Y-charn2)i-1
    
    // Indicators: Truncation. If N1 > N2 then ON; otherwise OFF
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 2);
    
    e->srcCN = e->CN1;    // starting at char pos CN
    e->dstCN = e->CN2;    // starting at char pos CN
    
    e->srcTA = e->TA1;
    e->dstTA = e->TA2;
    
    switch(e->TA1)
    {
        case CTA4:
            //e->srcAddr = e->YChar41;
            e->srcSZ = 4;
            break;
        case CTA6:
            //e->srcAddr = e->YChar61;
            e->srcSZ = 6;
            break;
        case CTA9:
            //e->srcAddr = e->YChar91;
            e->srcSZ = 9;
            break;
    }
    
    switch(e->TA2)
    {
        case CTA4:
            //e->dstAddr = e->YChar42;
            e->dstSZ = 4;
            break;
        case CTA6:
            //e->dstAddr = e->YChar62;
            e->dstSZ = 6;
            break;
        case CTA9:
            //e->dstAddr = e->YChar92;
            e->dstSZ = 9;
            break;
    }
    
    word36 xlat = e->op[2];                         // 3rd word is a pointer to a translation table
    int xA = (int)bitfieldExtract36(xlat, 6, 1);    // 'A' bit - indirect via pointer register
    int xREG = xlat & 0xf;

    word36 r = getMFReg36(xREG, false);

    word18 xAddress = GETHI(xlat);

    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    if (xA)
    {
        // if 3rd operand contains A (bit-29 set) then it Means Y-char93 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        uint n = (int)bitfieldExtract36(xAddress, 15, 3);
        word15 offset = xAddress & MASK15;  // 15-bit signed number
        xAddress = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
        {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e->ADDR3.SNR = PR[n].SNR;
            e->ADDR3.RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->ADDR3.mat = viaPR;
        }
    }
    
    xAddress +=  ((9*ARn_CHAR + 36*r + ARn_BITNO) / 36);
    xAddress &= AMASK;
    e->ADDR3.address = xAddress;
    
    // XXX I think this is where prepage mode comes in. Need to ensure that the translation table's page is im memory.
    // XXX handle, later. (Yeah, just like everything else hard.)
    //  Prepage Check in a Multiword Instruction
    //  The MVT, TCT, TCTR, and CMPCT instruction have a prepage check. The size of the translate table is determined by the TA1 data type as shown in the table below. Before the instruction is executed, a check is made for allocation in memory for the page for the translate table. If the page is not in memory, a Missing Page fault occurs before execution of the instruction. (Bull RJ78 p.7-75)
        
    // TA1              TRANSLATE TABLE SIZE
    // 4-BIT CHARACTER      4 WORDS
    // 6-BIT CHARACTER     16 WORDS
    // 9-BIT CHARACTER    128 WORDS
    
    int xlatSize = 0;   // size of xlation table in words .....
    switch(e->TA1)
    {
        case CTA4:
            xlatSize = 4;
            break;
        case CTA6:
            xlatSize = 16;
            break;
        case CTA9:
            xlatSize = 128;
            break;
    }
    
    word36 xlatTbl[128];
    memset(xlatTbl, 0, sizeof(xlatTbl));    // 0 it out just in case
    
    // XXX here is where we probably need to to the prepage thang...
    //ReadNnoalign(xlatSize, xAddress, xlatTbl, OperandRead, 0);
    EISReadN(&e->ADDR3, xlatSize, xlatTbl);
    
    e->T = bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    
    int fill = (int)bitfieldExtract36(e->op0, 27, 9);
    int fillT = fill;  // possibly truncated fill pattern
    // play with fill if we need to use it
    switch(e->srcSZ)
    {
        case 4:
            fillT = fill & 017;    // truncate upper 5-bits
            break;
        case 6:
            fillT = fill & 077;    // truncate upper 3-bits
            break;
    }
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, 
      "%s srcCN:%d dstCN:%d srcSZ:%d dstSZ:%d T:%d fill:%03o/%03o N1:%d N2:%d\n",
      __func__, e -> srcCN, e -> dstCN, e -> srcSZ, e -> dstSZ, e -> T,
      fill, fillT, e -> N1, e -> N2);

    //int xlatAddr = 0;
    //int xlatCN = 0;

    /// XXX when do we do a truncation fault?
    
    SCF(e->N1 > e->N2, cu.IR, I_TRUNC);
    if (e->N1 > e->N2 && e -> T)
      doFault(overflow_fault, 0, "mvt truncation fault");

    //SCF(e->N1 > e->N2, cu.IR, I_TALLY);   // HWR 7 Feb 2014. Possibly undocumented behavior. TRO may be set also!

    //get469(NULL, 0, 0, 0);    // initialize char getter buffer
    
    for(uint i = 0 ; i < min(e->N1, e->N2); i += 1)
    {
        //int c = get469(e, &e->srcAddr, &e->srcCN, e->TA1); // get src char
        int c = EISget469(&e->ADDR1, &e->srcCN, e->TA1); // get src char
        int cidx = 0;
    
        if (e->TA1 == e->TA2)
            //write469(e, &e->dstAddr, &e->dstCN, e->TA1, xlate(xlatTbl, e->dstTA, c));
            EISwrite469(&e->ADDR2, &e->dstCN, e->TA1, xlate(xlatTbl, e->dstTA, c));
        else
        {
            // If data types are dissimilar (TA1 ≠ TA2), each character is high-order truncated or zero filled, as appropriate, as it is moved. No character conversion takes place.
            cidx = c;
            
            unsigned int cout = xlate(xlatTbl, e->dstTA, cidx);

//            switch(e->dstSZ)
//            {
//                case 4:
//                    cout &= 017;    // truncate upper 5-bits
//                    break;
//                case 6:
//                    cout &= 077;    // truncate upper 3-bits
//                    break;
//            }

            switch (e->srcSZ)
            {
                case 6:
                    switch(e->dstSZ)
                    {
                        case 4:
                            cout &= 017;    // truncate upper 2-bits
                            break;
                        case 9:
                            break;              // should already be 0-filled
                    }
                    break;
                case 9:
                    switch(e->dstSZ)
                    {
                        case 4:
                            cout &= 017;    // truncate upper 5-bits
                            break;
                        case 6:
                            cout &= 077;    // truncate upper 3-bits
                            break;
                    }
                    break;
            }
            
            //write469(e, &e->dstAddr, &e->dstCN, e->TA2, cout);
            EISwrite469(&e->ADDR2, &e->dstCN, e->TA2, cout);
        }
    }
    
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL characters are high-order truncated as they are moved to C(Y-charn2). No character conversion takes place.
    
    if (e->N1 < e->N2)
    {
        unsigned int cfill = xlate(xlatTbl, e->dstTA, fillT);
        switch (e->srcSZ)
        {
            case 6:
                switch(e->dstSZ)
                {
                    case 4:
                        cfill &= 017;    // truncate upper 2-bits
                        break;
                    case 9:
                        break;              // should already be 0-filled
                }
                break;
            case 9:
                switch(e->dstSZ)
                {
                    case 4:
                        cfill &= 017;    // truncate upper 5-bits
                        break;
                    case 6:
                        cfill &= 077;    // truncate upper 3-bits
                        break;
                }
                break;
        }
        
//        switch(e->dstSZ)
//        {
//            case 4:
//                cfill &= 017;    // truncate upper 5-bits
//                break;
//            case 6:
//                cfill &= 077;    // truncate upper 3-bits
//                break;
//        }
        
        for(uint j = e->N1 ; j < e->N2 ; j += 1)
            //write469(e, &e->dstAddr, &e->dstCN, e->TA2, cfill);
            EISwrite469(&e->ADDR2, &e->dstCN, e->TA2, cfill);
    }
}

static word18 getMF2Reg(int n, word18 data)
{
    switch (n)
    {
        case 0: ///< n
            return 0;
        case 1: ///< au
            return GETHI(rA);
        case 2: ///< qu
            return GETHI(rQ);
        case 3: ///< du
            return GETHI(data);
        case 5: ///< al
            return GETLO(rA);
        case 6: ///< ql / a
            return GETLO(rQ);
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            return rX[n - 8];
        default:
            // XXX: IPR generate Illegal Procedure Fault
            sim_printf ("XXX: IPR generate Illegal Procedure Fault\n");
            return 0;
    }
    //sim_printf ("getMF2Reg(): How'd we get here? n=%d\n", n);
    //return 0;
}

/*
 * SCM - Scan with Mask
 */
// CANFAULT
void scm(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For characters i = 1, 2, ..., N1
    //   For bits j = 0, 1, ..., 8
    //      C(Z)j = ~C(MASK)j & ((C(Y-charn1)i-1 )j ⊕ (C(Y-charn2)0)j)
    //      If C(Z)0,8 = 00...0, then
    //           00...0 → C(Y3)0,11
    //           i-1 → C(Y3)12,35
    //      otherwise, continue scan of C(Y-charn1)
    // If a masked character match was not found, then
    //   00...0 → C(Y3)0,11
    //   N1 → C(Y3)12,35
    
    // Starting at location YC1, the L1 type TA1 characters are masked and compared with the assumed type TA1 character contained either in location YC2 or in bits 0-8 or 0-5 of the address field of operand descriptor 2 (when the REG field of MF2 specifies DU modification). The mask is right-justified in bit positions 0-8 of the instruction word. Each bit position of the mask that is a 1 prevents that bit position in the two characters from entering into the compare.
    // The masked compare operation continues until either a match is found or the tally (L1) is exhausted. For each unsuccessful match, a count is incremented by 1. When a match is found or when the L1 tally runs out, this count is stored right-justified in bits 12-35 of location Y3 and bits 0-11 of Y3 are zeroed. The contents of location YC2 and the source string remain unchanged. The RL bit of the MF2 field is not used.
    
    // The REG field of MF1 is checked for a legal code. If DU is specified in the REG field of MF2 in one of the four multiword instructions (SCD, SCDR, SCM, or SCMR) for which DU is legal, the CN field is ignored and the character or characters are arranged within the 18 bits of the word address portion of the operand descriptor.
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
  
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 1);
    
    e->srcCN = e->CN1;  ///< starting at char pos CN
    e->srcCN2= e->CN2;  ///< character number
    
    //Both the string and the test character pair are treated as the data type given for the string, TA1. A data type given for the test character pair, TA2, is ignored.
    e->srcTA = e->TA1;
    e->srcTA2 = e->TA1;

    switch(e->srcTA)
    {
        case CTA4:
            e->srcSZ = 4;
            break;
        case CTA6:
            e->srcSZ = 6;
            break;
        case CTA9:
            e->srcSZ = 9;
            break;
    }
    
    // get 'mask'
    int mask = (int)bitfieldExtract36(e->op0, 27, 9);
    
    // fetch 'test' char
    //If MF2.ID = 0 and MF2.REG = du, then the second word following the instruction word does not contain an operand descriptor for the test character; instead, it contains the test character as a direct upper operand in bits 0,8.
    
    int ctest = 0;
    if (!(e->MF2 & MFkID) && ((e->MF2 & MFkREGMASK) == 3))  // MF2.du
    {
        int duo = GETHI(e->OP2);
        // per Bull RJ78, p. 5-45
        switch(e->srcTA)
        {
            case CTA4:
                ctest = (duo >> 13) & 017;
                break;
            case CTA6:
                ctest = (duo >> 12) & 077;
                break;
            case CTA9:
                ctest = (duo >> 9) & 0777;
                break;

        }
    }
    else
    {
        ctest = EISget469(&e->ADDR2, &e->srcCN2, e->TA2);
    }
    switch(e->srcTA2)
    {
        case CTA4:
            ctest &= 017;    // keep 4-bits
            break;
        case CTA6:
            ctest &= 077;    // keep 6-bits
            break;
        case CTA9:
            ctest &= 0777;   // keep 9-bits
    }

    sim_debug (DBG_TRACEEXT, & cpu_dev, 
      "%s srcCN:%d srcCN2:%d srcTA:%d srcTA2:%d srcSZ:%d mask:0%06o ctest: 0%03o\n",
      __func__, e -> srcCN, e -> srcCN2, e -> srcTA, e -> srcTA2, e -> srcSZ, 
      mask, ctest);

    word18 y3 = GETHI(e->OP3);
    int y3A = (int)bitfieldExtract36(e->OP3, 6, 1); // 'A' bit - indirect via pointer register
    int y3REG = e->OP3 & 0xf;
    
    word36 r = getMFReg36(y3REG, false);
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, 
      "%s y3:0%06o y3A:%d y3REG: %d, r:%lld\n",
      __func__, y3, y3A, y3REG, r);

    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    if (y3A)
    {
        // if 3rd operand contains A (bit-29 set) then it Means Y-char93 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        uint n = (int)bitfieldExtract36(y3, 15, 3);
        word15 offset = y3 & MASK15;  // 15-bit signed number
        y3 = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
        {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e->ADDR3.SNR = PR[n].SNR;
            e->ADDR3.RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->ADDR3.mat = viaPR;
        }

        sim_debug (DBG_TRACEEXT, & cpu_dev, 
          "%s is y3a: n:%d offset:0%06o y3:0%06o ARn_CHAR:%d ARn_BITNO:%d SNR:0%5o RNR:%d mat: %d\n",
          __func__, n, offset, y3, ARn_CHAR, ARn_BITNO, e->ADDR3.SNR, e->ADDR3.RNR, e->ADDR3.mat);

    }
    
    y3 +=  ((9*ARn_CHAR + 36*r + ARn_BITNO) / 36);
    y3 &= AMASK;
    
    e->ADDR3.address = y3;
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, 
      "%s y3: :0%06o\n",
      __func__, y3);
    //get469(NULL, 0, 0, 0);    // initialize char getter
    
#ifdef DBGX
sim_printf ("SCM mask %06o ctest %06o\n", mask, ctest);
#endif
    uint i = 0;
    for(; i < e->N1; i += 1)
    {
        int yCharn1 = EISget469(&e->ADDR1, &e->srcCN, e->srcTA2);
        
        int c = ((~mask) & (yCharn1 ^ ctest)) & 0777;
#ifdef DBGX
sim_printf ("SCM %03o %c %03o\n", yCharn1, iscntrl (yCharn1) ? '?' : yCharn1, c);
#endif
        if (c == 0)
        {
            //00...0 → C(Y3)0,11
            //i-1 → C(Y3)12,35
            //Y3 = bitfieldInsert36(Y3, i, 0, 24);
            break;
        }
    }
    word36 CY3 = bitfieldInsert36(0, i, 0, 24);
    
    SCF(i == e->N1, cu.IR, I_TALLY);
    
    // write Y3 .....
    //Write (y3, CY3, OperandWrite, 0);
    EISWrite(&e->ADDR3, CY3);
}

/*
 * SCMR - Scan with Mask Reverse
 */
// CANFAULT
void scmr(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For characters i = 1, 2, ..., N1
    //   For bits j = 0, 1, ..., 8
    //      C(Z)j = ~C(MASK)j & ((C(Y-charn1)i-1 )j ⊕ (C(Y-charn2)0)j)
    //      If C(Z)0,8 = 00...0, then
    //           00...0 → C(Y3)0,11
    //           i-1 → C(Y3)12,35
    //      otherwise, continue scan of C(Y-charn1)
    // If a masked character match was not found, then
    //   00...0 → C(Y3)0,11
    //   N1 → C(Y3)12,35
    
    // Starting at location YC1, the L1 type TA1 characters are masked and compared with the assumed type TA1 character contained either in location YC2 or in bits 0-8 or 0-5 of the address field of operand descriptor 2 (when the REG field of MF2 specifies DU modification). The mask is right-justified in bit positions 0-8 of the instruction word. Each bit position of the mask that is a 1 prevents that bit position in the two characters from entering into the compare.
    // The masked compare operation continues until either a match is found or the tally (L1) is exhausted. For each unsuccessful match, a count is incremented by 1. When a match is found or when the L1 tally runs out, this count is stored right-justified in bits 12-35 of location Y3 and bits 0-11 of Y3 are zeroed. The contents of location YC2 and the source string remain unchanged. The RL bit of the MF2 field is not used.
    
    // The REG field of MF1 is checked for a legal code. If DU is specified in the REG field of MF2 in one of the four multiword instructions (SCD, SCDR, SCM, or SCMR) for which DU is legal, the CN field is ignored and the character or characters are arranged within the 18 bits of the word address portion of the operand descriptor.
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 1); // Use TA1
    
    e->srcCN = e->CN1;  ///< starting at char pos CN
    e->srcCN2= e->CN2;  ///< character number
    
    //Both the string and the test character pair are treated as the data type given for the string, TA1. A data type given for the test character pair, TA2, is ignored.
    e->srcTA = e->TA1;
    e->srcTA2 = e->TA1;
    
    switch(e->srcTA)
    {
        case CTA4:
            e->srcSZ = 4;
            break;
        case CTA6:
            e->srcSZ = 6;
            break;
        case CTA9:
            e->srcSZ = 9;
            break;
    }
    
    // adjust addresses & offsets for reading in reverse ....
    int nSrcWords = 0;
    int newSrcCN = 0;
    getOffsets(e->N1, e->srcCN, e->srcTA, &nSrcWords, &newSrcCN);

    //word18 newSrcAddr = e->srcAddr + nSrcWords;
    e->ADDR1.address += nSrcWords;
    
    // get 'mask'
    int mask = (int)bitfieldExtract36(e->op0, 27, 9);
    
    // fetch 'test' char
    //If MF2.ID = 0 and MF2.REG = du, then the second word following the instruction word does not contain an operand descriptor for the test character; instead, it contains the test character as a direct upper operand in bits 0,8.
    
    int ctest = 0;
    if (!(e->MF2 & MFkID) && ((e->MF2 & MFkREGMASK) == 3))  // MF2.du
    {
        int duo = GETHI(e->OP2);
        // per Bull RJ78, p. 5-45
        switch(e->srcTA)
        {
            case CTA4:
                ctest = (duo >> 13) & 017;
                break;
            case CTA6:
                ctest = (duo >> 12) & 077;
                break;
            case CTA9:
                ctest = (duo >> 9) & 0777;
                break;
        }
    }
    else
    {
        //if (!(e->MF2 & MFkID))  // if id is set then we don't bother with MF2 reg as an address modifier
        //{
        //    int y2offset = getMF2Reg(e->MF2 & MFkREGMASK, (word18)bitfieldExtract36(e->OP2, 27, 9));
        //    e->srcAddr2 += y2offset;
        //}
        ctest = EISget469(&e->ADDR2, &e->srcCN2, e->srcTA2);
    }
    switch(e->srcTA2)
    {
        case CTA4:
            ctest &= 017;    // keep 4-bits
            break;
        case CTA6:
            ctest &= 077;    // keep 6-bits
            break;
        case CTA9:
            ctest &= 0777;   // keep 9-bits
    }
    
    word18 y3 = GETHI(e->OP3);
    int y3A = (int)bitfieldExtract36(e->OP3, 6, 1); // 'A' bit - indirect via pointer register
    int y3REG = e->OP3 & 0xf;
    
    word36 r = getMFReg36(y3REG, false);
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    if (y3A)
    {
        // if 3rd operand contains A (bit-29 set) then it Means Y-char93 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        uint n = (int)bitfieldExtract36(y3, 15, 3);
        word15 offset = y3 & MASK15;  // 15-bit signed number
        y3 = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;

        if (get_addr_mode() == APPEND_mode)
        {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e->ADDR3.SNR = PR[n].SNR;
            e->ADDR3.RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->ADDR3.mat = viaPR;
        }
    }
    
    y3 +=  ((9*ARn_CHAR + 36*r + ARn_BITNO) / 36);
    y3 &= AMASK;
    
    e->ADDR3.address = y3;
    
    //get469r(NULL, 0, 0, 0);    // initialize char getter
    
    uint i = 0;
    for(; i < e->N1; i += 1)
    {
        //int yCharn1 = get469r(e, &newSrcAddr, &newSrcCN, e->srcTA2);
        int yCharn1 = EISget469r(&e->ADDR1, &newSrcCN, e->srcTA2);
        
        int c = ((~mask) & (yCharn1 ^ ctest)) & 0777;
        if (c == 0)
        {
            //00...0 → C(Y3)0,11
            //i-1 → C(Y3)12,35
            //Y3 = bitfieldInsert36(Y3, i, 0, 24);
            break;
        }
    }
    word36 CY3 = bitfieldInsert36(0, i, 0, 24);
    
    SCF(i == e->N1, cu.IR, I_TALLY);
    
    // write Y3 .....
    //Write (y3, CY3, OperandWrite, 0);
    EISWrite(&e->ADDR3, CY3);
}

/*
 * TCT - Test Character and Translate
 */
// CANFAULT
void tct(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., N1
    //   m = C(Y-charn1)i-1
    //   If C(Y-char92)m ≠ 00...0, then
    //     C(Y-char92)m → C(Y3)0,8
    //     000 → C(Y3)9,11
    //     i-1 → C(Y3)12,35
    //   otherwise, continue scan of C(Y-charn1)
    // If a non-zero table entry was not found, then 00...0 → C(Y3)0,11
    // N1 → C(Y3)12,35
    //
    // Indicators: Tally Run Out. If the string length count exhausts, then ON; otherwise, OFF
    //
    // If the data type of the string to be scanned is not 9-bit (TA1 ≠ 0),
    // then characters from C(Y-charn1) are high-order zero filled in forming
    // the table index, m.
    // Instruction execution proceeds until a non-zero table entry is found or
    // the string length count is exhausted.
    // The character number of Y-char92 must be zero, i.e., Y-char92 must start
    // on a word boundary.
    
    
    setupOperandDescriptor(1, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    
    e->srcCN = e->CN1;    // starting at char pos CN
    e->srcTA = e->TA1;
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT CN1: %d TA1: %d\n", e -> CN1, e -> TA1);

    switch(e->TA1)
    {
        case CTA4:
            //e->srcAddr = e->YChar41;
            e->srcSZ = 4;
            break;
        case CTA6:
            //e->srcAddr = e->YChar61;
            e->srcSZ = 6;
            break;
        case CTA9:
            //e->srcAddr = e->YChar91;
            e->srcSZ = 9;
            break;
    }
    
    
    // fetch 2nd operand ...
    
    word36 xlat = e->OP2;   //op[1];                 // 2nd word is a pointer to a translation table
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT OP2: %012llo\n", e -> OP2);

    //int xA = (int)bitfieldExtract36(xlat, 6, 1); // 'A' bit - indirect via pointer register
    word1 xA = GET_A (xlat);

    //int xREG = xlat & 0xf;
    word4 xREG = GET_TD (xlat);

    //word36 r = getMFReg36(xREG, false);
    // XXX I am not sure about this; the documentation is vague about 18/36
    // bit usage here.
    word36 r = getMFReg36 (xREG, false);
    
    sim_debug (DBG_CAC, & cpu_dev,
               "TCT xREG %o r %llo\n", xREG, r);

    word18 xAddress = GETHI (xlat);
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    if (xA)
    {
        // if 2nd operand contains A (bit-29 set) then it Means Y-char92 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        //int n = (int)bitfieldExtract36(xAddress, 15, 3);
        word3 n = GET_ARN (xlat);
        //int offset = xAddress & MASK15;  // 15-bit signed number
        word18 offset = SIGNEXT15 (xAddress & MASK15);  // 15-bit signed number
        xAddress = (AR [n] . WORDNO + offset) & AMASK;
        
        sim_debug (DBG_CAC, & cpu_dev,
                   "TCT OP2 is indirect; offset %06o AR[%d].WORDNO %06o\n",
                   offset, n, AR [n] . WORDNO);

        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "TCT OP2 is indirect; offset %06o AR[%d].WORDNO %06o\n",
                   offset, n, AR [n] . WORDNO);
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;

        if (get_addr_mode() == APPEND_mode)
        {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e->ADDR2.SNR = AR[n].SNR;
            e->ADDR2.RNR = max3(AR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->ADDR2.mat = viaPR;
        }
    }
    
    sim_debug (DBG_CAC, & cpu_dev,
               "TCT xAddress %06o ARn_CHAR %o r %llo ARn_BITno %o\n",
               xAddress, ARn_CHAR, r, ARn_BITNO);
    // XXX Watch for 36u*r overflow here
    xAddress +=  ((9u*ARn_CHAR + 36u*r + ARn_BITNO) / 36u);
    xAddress &= AMASK;
    
    sim_debug (DBG_CAC, & cpu_dev,
               "TCT OP2 final address %06o\n",
               xAddress);

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT OP2 final address %06o\n",
               xAddress);

    e->ADDR2.address = xAddress;

    // XXX I think this is where prepage mode comes in. Need to ensure that the translation table's page is im memory.
    // XXX handle, later. (Yeah, just like everything else hard.)
    //  Prepage Check in a Multiword Instruction
    //  The MVT, TCT, TCTR, and CMPCT instruction have a prepage check. The size of the translate table is determined by the TA1 data type as shown in the table below. Before the instruction is executed, a check is made for allocation in memory for the page for the translate table. If the page is not in memory, a Missing Page fault occurs before execution of the instruction. (Bull RJ78 p.7-75)
    
    // TA1              TRANSLATE TABLE SIZE
    // 4-BIT CHARACTER      4 WORDS
    // 6-BIT CHARACTER     16 WORDS
    // 9-BIT CHARACTER    128 WORDS
    
    uint xlatSize = 0;   // size of xlation table in words .....
    switch(e -> TA1)
    {
        case CTA4:
            xlatSize = 4;
            break;
        case CTA6:
            xlatSize = 16;
            break;
        case CTA9:
            xlatSize = 128;
            break;
    }
    
    word36 xlatTbl [128];
    memset (xlatTbl, 0, sizeof (xlatTbl));    // 0 it out just in case
    
    // XXX here is where we probably need to to the prepage thang...
    //ReadNnoalign(xlatSize, xAddress, xlatTbl, OperandRead, 0);
    EISReadN (& e -> ADDR2, xlatSize, xlatTbl);
    
    // fetch 3rd operand ...
    
    word18 y3 = GETHI (e -> OP3);
    //int y3A = (int)bitfieldExtract36(e->OP3, 6, 1); // 'A' bit - indirect via pointer register
    word1 y3A = GET_A (e -> OP3);
    //int y3REG = e->OP3 & 0xf;
    word4 y3REG = GET_TD (e -> OP3);
    
    //r = (word18)getMFReg(y3REG, true, false);
    // XXX I am not sure about this; the documentation is vague about 18/36
    // bit usage here.
    r = getMFReg36 (y3REG, false);
    
    ARn_CHAR = 0;
    ARn_BITNO = 0;
    if (y3A)
      {
        // if 3rd operand contains A (bit-29 set) then it Means Y-char93 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        //int n = (int)bitfieldExtract36(y3, 15, 3);
        word3 n = GET_ARN (e -> OP3);
        //int offset = y3 & MASK15;  // 15-bit signed number
        word18 offset = SIGNEXT15 (y3 & MASK15);  // 15-bit signed number
        y3 = (AR [n] . WORDNO + offset) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode () == APPEND_mode)
          {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e -> ADDR3 . SNR = PR [n] . SNR;
            e -> ADDR3 . RNR = max3 (PR [n] . RNR, TPR . TRR, PPR . PRR);
            
            e -> ADDR3 . mat = viaPR;
          }
      }
    
    // XXX Watch for 36u*r overflow here
    y3 +=  ((9u*ARn_CHAR + 36u*r + ARn_BITNO) / 36u);
    y3 &= AMASK;

    e -> ADDR3 . address = y3;
    
    
    word36 CY3 = 0;
    
    //get469(NULL, 0, 0, 0);    // initialize char getter

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT N1 %d\n", e -> N1);

    uint i = 0;
    for(; i < e->N1 ; i += 1)
    {
        int c = EISget469(&e->ADDR1, &e->srcCN, e->TA1); // get src char

        int m = 0;
        
        switch (e->srcSZ)
        {
            case 4:
                m = c & 017;    // truncate upper 2-bits
                break;
            case 6:
                m = c & 077;    // truncate upper 3-bits
                break;
            case 9:
                m = c;          // keep all 9-bits
                break;              // should already be 0-filled
        }
        
        unsigned int cout = xlate(xlatTbl, CTA9, m);

        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "TCT c %03o %c cout %03o %c\n",
                   m, isprint (m) ? '?' : m, 
                   cout, isprint (cout) ? '?' : cout);

        if (cout)
        {
            CY3 = bitfieldInsert36(0, cout, 27, 9); // C(Y-char92)m → C(Y3)0,8
            break;
        }
    }
    
    SCF(i == e->N1, cu.IR, I_TALLY);
    
    CY3 = bitfieldInsert36(CY3, i, 0, 24);
    
    // write Y3 .....
    //Write (y3, CY3, OperandWrite, 0);
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT y3 %012llo\n", CY3);
    EISWrite(&e->ADDR3, CY3);
   
}

/*
 * TCTR - Test Character and Translate Reverse
 */
// CANFAULT
void tctr(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., N1
    //   m = C(Y-charn1)N1-i
    //   If C(Y-char92)m ≠ 00...0, then
    //     C(Y-char92)m → C(Y3)0,8
    //     000 → C(Y3)9,11
    //     i-1 → C(Y3)12,35
    //   otherwise, continue scan of C(Y-charn1) If a non-zero table entry was not found, then
    // 00...0 → C(Y3)0,11
    // N1 → C(Y3)12,35
    //
    // Indicators: Tally Run Out. If the string length count exhausts, then ON; otherwise, OFF
    //
    // If the data type of the string to be scanned is not 9-bit (TA1 ≠ 0), then characters from C(Y-charn1) are high-order zero filled in forming the table index, m.
    // Instruction execution proceeds until a non-zero table entry is found or the string length count is exhausted.
    // The character number of Y-char92 must be zero, i.e., Y-char92 must start on a word boundary.
    
    
    setupOperandDescriptor(1, e);
    parseAlphanumericOperandDescriptor(1, e, 1);
    
    e->srcCN = e->CN1;    // starting at char pos CN
    e->srcTA = e->TA1;
    
    switch(e->TA1)
    {
        case CTA4:
            //e->srcAddr = e->YChar41;
            e->srcSZ = 4;
            break;
        case CTA6:
            //e->srcAddr = e->YChar61;
            e->srcSZ = 6;
            break;
        case CTA9:
            //e->srcAddr = e->YChar91;
            e->srcSZ = 9;
            break;
    }
    sim_debug (DBG_TRACEEXT, & cpu_dev, "tctr srcCN %d srcT %d srcSz %d\n",
               e -> srcCN, e -> srcTA, e -> srcSZ);    
    // adjust addresses & offsets for reading in reverse ....
    int nSrcWords = 0;
    int newSrcCN = 0;
    getOffsets(e->N1, e->srcCN, e->srcTA, &nSrcWords, &newSrcCN);
    sim_debug (DBG_TRACEEXT, & cpu_dev, "tctr nSrcWords %d newSrcCN %d\n",
               nSrcWords, nSrcWords);
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, "tctr address was %06o\n",
               e -> ADDR1 . address);
    //word18 newSrcAddr = e->srcAddr + nSrcWords;
    e->ADDR1.address += nSrcWords;
    sim_debug (DBG_TRACEEXT, & cpu_dev, "tctr address is %06o\n",
               e -> ADDR1 . address);

    // fetch 2nd operand ...
    
    word36 xlat = e->OP2;   //op[1];                 // 2nd word is a pointer to a translation table
    int xA = (int)bitfieldExtract36(xlat, 6, 1); // 'A' bit - indirect via pointer register
    int xREG = xlat & 0xf;
    
    word36 r = getMFReg36(xREG, false);
    
    word18 xAddress = GETHI(xlat);
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    if (xA)
    {
        // if 2nd operand contains A (bit-29 set) then it Means Y-char92 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        int n = (int)bitfieldExtract36(xAddress, 15, 3);
        int offset = xAddress & MASK15;  // 15-bit signed number
        xAddress = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
// XXX CAC this check doesn't seem right; the address is from a PR register,
// therefore, it is a virtual address, regardless of get_addr_mode().
        if (get_addr_mode() == APPEND_mode)
        {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e->ADDR2.SNR = PR[n].SNR;
            e->ADDR2.RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);

            e->ADDR2.mat = viaPR;
        }
    }
    
    xAddress +=  ((9*ARn_CHAR + 36*r + ARn_BITNO) / 36);
    xAddress &= AMASK;
    
    e->ADDR2.address = xAddress;
    
    // XXX I think this is where prepage mode comes in. Need to ensure that the translation table's page is im memory.
    // XXX handle, later. (Yeah, just like everything else hard.)
    //  Prepage Check in a Multiword Instruction
    //  The MVT, TCT, TCTR, and CMPCT instruction have a prepage check. The size of the translate table is determined by the TA1 data type as shown in the table below. Before the instruction is executed, a check is made for allocation in memory for the page for the translate table. If the page is not in memory, a Missing Page fault occurs before execution of the instruction. (Bull RJ78 p.7-75)
    
    // TA1              TRANSLATE TABLE SIZE
    // 4-BIT CHARACTER      4 WORDS
    // 6-BIT CHARACTER     16 WORDS
    // 9-BIT CHARACTER    128 WORDS
    
    int xlatSize = 0;   // size of xlation table in words .....
    switch(e->TA1)
    {
        case CTA4:
            xlatSize = 4;
            break;
        case CTA6:
            xlatSize = 16;
            break;
        case CTA9:
            xlatSize = 128;
            break;
    }
    
    word36 xlatTbl[128];
    memset(xlatTbl, 0, sizeof(xlatTbl));    // 0 it out just in case
    
    // XXX here is where we probably need to to the prepage thang...
    //ReadNnoalign(xlatSize, xAddress, xlatTbl, OperandRead, 0);
    EISReadN(&e->ADDR2, xlatSize, xlatTbl);
    
    // fetch 3rd operand ...
    
    int y3 = GETHI(e->OP3);
    int y3A = (int)bitfieldExtract36(e->OP3, 6, 1); // 'A' bit - indirect via pointer register
    int y3REG = e->OP3 & 0xf;
    
    r = getMFReg36(y3REG, false);
    
    ARn_CHAR = 0;
    ARn_BITNO = 0;
    if (y3A)
    {
        // if 3rd operand contains A (bit-29 set) then it Means Y-char93 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        uint n = (int)bitfieldExtract36(y3, 15, 3);
        word15 offset = y3 & MASK15;  // 15-bit signed number
        y3 = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
// XXX CAC this check doesn't seem right; the address is from a PR register,
// therefore, it is a virtual address, regardless of get_addr_mode().
        if (get_addr_mode() == APPEND_mode)
        {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e->ADDR3.SNR = PR[n].SNR;
            e->ADDR3.RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->ADDR3.mat = viaPR;
        }
    }
    
    y3 +=  ((9*ARn_CHAR + 36*r + ARn_BITNO) / 36);
    y3 &= AMASK;
    
    e->ADDR3.address = y3;
    
    word36 CY3 = 0;
    
    //get469r(NULL, 0, 0, 0);    // initialize char getter
    
#ifdef DBGX
sim_printf ("TCTR N1 %d\n", e -> N1);
#endif

    uint i = 0;
    for(; i < e->N1 ; i += 1)
    {
        //int c = get469r(e, &newSrcAddr, &newSrcCN, e->TA1); // get src char
        int c = EISget469r(&e->ADDR1, &newSrcCN, e->TA1); // get src char
        
        int m = 0;
        
        switch (e->srcSZ)
        {
            case 4:
                m = c & 017;    // truncate upper 2-bits
                break;
            case 6:
                m = c & 077;    // truncate upper 3-bits
                break;
            case 9:
                m = c;          // keep all 9-bits
                break;          // should already be 0-filled
        }
        
        //unsigned int cout = xlate(xlatTbl, e->srcTA, m);
        unsigned int cout = xlate(xlatTbl, CTA9, m);
#ifdef DBGX
sim_printf ("TCT c %03o %c cout %03o %c\n",
            m, iscntrl (m) ? '?' : m, 
            cout, iscntrl (cout) ? '?' : cout);
#endif
        if (cout)
        {
            CY3 = bitfieldInsert36(0, cout, 27, 9); // C(Y-char92)m → C(Y3)0,8
            break;
        }
    }
    
    SCF(i == e->N1, cu.IR, I_TALLY);
    
    CY3 = bitfieldInsert36(CY3, i, 0, 24);
    
    // write Y3 .....
    //Write (y3, CY3, OperandWrite, 0);
    EISWrite(&e->ADDR3, CY3);
}


//#define DBGX
// CANFAULT
void cmpc(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., minimum (N1,N2)
    //    C(Y-charn1)i-1 :: C(Y-charn2)i-1
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) :: C(Y-charn2)i-1
    // If N1 > N2, then for i = N2+1, N2+2, ..., N1
    //    C(Y-charn1)i-1 :: C(FILL)
    //
    // Indicators:
    //     Zero: If C(Y-charn1)i-1 = C(Y-charn2)i-1 for all i, then ON; 
    //       otherwise, OFF
    //     Carry: If C(Y-charn1)i-1 < C(Y-charn2)i-1 for any i, then OFF; 
    //       otherwise ON
    
    // Both strings are treated as the data type given for the left-hand
    // string, TA1. A data type given for the right-hand string, TA2, is
    // ignored.
    //
    // Comparison is made on full 9-bit fields. If the given data type is not
    // 9-bit (TA1 ≠ 0), then characters from C(Y-charn1) and C(Y-charn2) are
    // high- order zero filled. All 9 bits of C(FILL) are used.
    //
    // Instruction execution proceeds until an inequality is found or the
    // larger string length count is exhausted.
    
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 1);
    
    du . TAk [1] = du . TAk [0]; // TA2 = TA1

    e->srcCN = e->CN1;  ///< starting at char pos CN
    e->srcCN2= e->CN2;  ///< character number
    
    e->srcTA = e->TA1;
    
    int fill = (int) getbits36 (cu . IWB, 0, 9);
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, 
      "%s N1: %d N2: %d srcCN:%d srcCN2:%d srcTA:%d srcSZ:%d fill:0%03o\n",
      __func__, e -> N1, e -> N2, e -> srcCN, e -> srcCN2, e -> srcTA, e -> srcSZ, 
      fill);

    SETF (cu . IR, I_ZERO);  // set ZERO flag assuming strings are equal ...
    SETF (cu . IR, I_CARRY); // set CARRY flag assuming strings are equal ...
    
    if_sim_debug (DBG_CAC, & cpu_dev)
      {
        if (e -> N1 < 80 && e -> N2 < 80)
          {
            char buffer [4 * 80 + 1];
            buffer [0] = '\0';

            //sim_printf ("[%lld]\n", sim_timell ());
            //sim_printf ("s1: <");
            for (uint i = 0; i < e -> N1; i ++)
              {
                char * bp = buffer + strlen (buffer);
                unsigned char c = (unsigned char) EIScac (& e -> ADDR1, e -> srcCN + i, e -> srcTA);
                if (isprint (c))
                  sprintf (bp, "%c", c);
                else
                  sprintf (bp, "\\%03o", c);
              }
            //sim_printf (">\n");

            //sim_printf ("s2: <");
            char buffer2 [4 * 80 + 1];
            buffer2 [0] = '\0';
            for (uint i = 0; i < e -> N2; i ++)
              {
                char * bp = buffer2 + strlen (buffer2);
                unsigned char c = (unsigned char) EIScac (& e -> ADDR2, e -> srcCN2 + i, e -> srcTA);
                if (isprint (c))
                  sprintf (bp, "%c", c);
                else
                  sprintf (bp, "\\%03o", c);
              }
            //sim_printf (">\n");
            sim_debug (DBG_CAC, & cpu_dev, "s1: <%s>\n", buffer);
            sim_debug (DBG_CAC, & cpu_dev, "s2: <%s>\n", buffer2);
          }
      }


    uint i = 0;
    for (; i < min (e->N1, e->N2); i += 1)
      {
        int c1 = EISget469(&e->ADDR1,  &e->srcCN,  e->TA1);   // get Y-char1n
        int c2 = EISget469(&e->ADDR2, &e->srcCN2, e->TA1);   // get Y-char2n
sim_debug (DBG_TRACEEXT, & cpu_dev, "cmpc c1 %u c2 %u\n", c1, c2);
        if (c1 != c2)
          {
            CLRF (cu . IR, I_ZERO);  // an inequality found
            SCF (c1 > c2, cu . IR, I_CARRY);
            return;
        }
      }
    if (e->N1 < e->N2)
        for(; i < e->N2; i += 1)
          {
            int c1 = fill;     // use fill for Y-char1n
            int c2 = EISget469(&e->ADDR2, &e->srcCN2, e->TA1); // get Y-char2n
            
            if (c1 != c2)
              {
                CLRF (cu . IR, I_ZERO);  // an inequality found
                SCF (c1 > c2, cu . IR, I_CARRY);
                return;
              }
          }
    else if (e->N1 > e->N2)
        for(; i < e->N1; i ++)
          {
            int c1 = EISget469 (&e->ADDR1,  &e->srcCN,  e->TA1);   // get Y-char1n
            int c2 = fill;   // use fill for Y-char2n
            
            if (c1 != c2)
              {
                CLRF(cu.IR, I_ZERO);  // an inequality found
                
                SCF(c1 > c2, cu.IR, I_CARRY);
                
                return;
              }
          }
  }

/*
 * SCD - Scan Characters Double
 */
// CANFAULT
void scd(DCDstruct *ins)
{
//sim_printf ("SCD %lld\n", sim_timell ());
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., N1-1
    //   C(Y-charn1)i-1,i :: C(Y-charn2)0,1
    // On instruction completion, if a match was found:
    //   00...0 → C(Y3)0,11
    //   i-1 → C(Y3)12,35
    // If no match was found:
    //   00...0 → C(Y3)0,11
    //      N1-1→ C(Y3)12,35
    //
    
    // The REG field of MF1 is checked for a legal code. If DU is specified in
    // the REG field of MF2 in one of the four multiword instructions (SCD,
    // SCDR, SCM, or SCMR) for which DU is legal, the CN field is ignored and
    // the character or characters are arranged within the 18 bits of the word
    // address portion of the operand descriptor.
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 1); // use TA1
    
    e->srcCN = e->CN1;  ///< starting at char pos CN
    e->srcCN2= e->CN2;  ///< character number
    
    // Both the string and the test character pair are treated as the data type
    // given for the string, TA1. A data type given for the test character
    // pair, TA2, is ignored.
    e->srcTA = e->TA1;
    e->srcTA2 = e->TA1;
    
    
    // fetch 'test' char - double
    // If MF2.ID = 0 and MF2.REG = du, then the second word following the
    // instruction word does not contain an operand descriptor for the test
    // character; instead, it contains the test character as a direct upper
    // operand in bits 0,8.
    
    int c1 = 0;
    int c2 = 0;
    
    if (!(e->MF2 & MFkID) && ((e->MF2 & MFkREGMASK) == 3))  // MF2.du
    {
//sim_printf ("Bull\n");
        // per Bull RJ78, p. 5-45
        switch(e->srcTA)
        {
            case CTA4:
                c1 = (e->ADDR2.address >> 13) & 017;
                c2 = (e->ADDR2.address >>  9) & 017;
                break;
            case CTA6:
                c1 = (e->ADDR2.address >> 12) & 077;
                c2 = (e->ADDR2.address >>  6) & 077;
                break;
            case CTA9:
                c1 = (e->ADDR2.address >> 9) & 0777;
                c2 = (e->ADDR2.address     ) & 0777;
                break;
        }
    }
    else
    {
#if 0
        // Not indirect and not du
        if (!(e->MF2 & MFkID))  // if id is set then we don't bother with MF2 reg as an address modifier
        {
            word18 y2offset = getMF2Reg(e->MF2 & MFkREGMASK, (word18)bitfieldExtract36(e->OP2, 27, 9));

            e->ADDR2.address += y2offset;
            e->ADDR2.address &= AMASK;
        }
#endif
        c1 = EISget469(&e->ADDR2, &e->srcCN2, e->srcTA2);
        c2 = EISget469(&e->ADDR2, &e->srcCN2, e->srcTA2);
    }

    switch(e->srcTA2)
    {
        case CTA4:
//sim_printf ("CTA4\n");
            c1 &= 017;    // keep 4-bits
            c2 &= 017;    // keep 4-bits
            break;
        case CTA6:
//sim_printf ("CTA6\n");
            c1 &= 077;    // keep 6-bits
            c2 &= 077;    // keep 6-bits
            break;
        case CTA9:
//sim_printf ("CTA9\n");
            c1 &= 0777;   // keep 9-bits
            c2 &= 0777;   // keep 9-bits
            break;
    }
    
    word18 y3 = GETHI(e->OP3);
    int y3A = (int)bitfieldExtract36(e->OP3, 6, 1); // 'A' bit - indirect via pointer register
    int y3REG = e->OP3 & 0xf;
    
    word36 r = getMFReg36(y3REG, false);
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    if (y3A)
    {
	// if 3rd operand contains A (bit-29 set) then it Means Y-char93 is not
	// the memory address of the data but is a reference to a pointer
	// register pointing to the data.
        uint n = (int)bitfieldExtract36(y3, 15, 3);
        word15 offset = y3 & MASK15;  // 15-bit signed number
        y3 = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
        {
            e->ADDR3.SNR = PR[n].SNR;
            e->ADDR3.RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->ADDR3.mat = viaPR;
        }
    }
    
    y3 +=  ((9*ARn_CHAR + 36*r + ARn_BITNO) / 36);
    y3 &= AMASK;
    
    e->ADDR3.address = y3;
    
    int yCharn11 = 0;
    int yCharn12 = 0;
    
    uint i = 0;
    for(; i < e->N1-1; i += 1)
    {
        
        if (i == 0)
        {
            yCharn11 = EISget469(&e->ADDR1, &e->srcCN, e->srcTA2);
            yCharn12 = EISget469(&e->ADDR1, &e->srcCN, e->srcTA2);
        }
        else
        {
            yCharn11 = yCharn12;
            yCharn12 = EISget469(&e->ADDR1, &e->srcCN, e->srcTA2);
        }
//sim_printf ("yCharn11 %o c1 %o yCharn12 %o c2 %o\n", yCharn11, c1, yCharn12, c2);
        if (yCharn11 == c1 && yCharn12 == c2)
            break;
    }
    
    word36 CY3 = bitfieldInsert36(0, i, 0, 24);
    
    SCF(i == e->N1-1, cu.IR, I_TALLY);
    
    // write Y3 .....
    //Write (y3, CY3, OperandWrite, 0);
    EISWrite(&e->ADDR3, CY3);
}
/*
 * SCDR - Scan Characters Double Reverse
 */
// CANFAULT
void scdr(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = 1, 2, ..., N1-1
    //   C(Y-charn1)N1-i-1,N1-i :: C(Y-charn2)0,1
    // On instruction completion, if a match was found:
    //   00...0 → C(Y3)0,11
    //   i-1 → C(Y3)12,35
    // If no match was found:
    //   00...0 → C(Y3)0,11
    //      N1-1→ C(Y3)12,35
    //
    
    // The REG field of MF1 is checked for a legal code. If DU is specified in the REG field of MF2 in one of the four multiword instructions (SCD, SCDR, SCM, or SCMR) for which DU is legal, the CN field is ignored and the character or characters are arranged within the 18 bits of the word address portion of the operand descriptor.
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 1); // Use TA1
    
    e->srcCN = e->CN1;  ///< starting at char pos CN
    e->srcCN2= e->CN2;  ///< character number
    
    //Both the string and the test character pair are treated as the data type given for the string, TA1. A data type given for the test character pair, TA2, is ignored.
    e->srcTA = e->TA1;
    e->srcTA2 = e->TA1;
    
    // adjust addresses & offsets for reading in reverse ....
    int nSrcWords = 0;
    int newSrcCN = 0;
    getOffsets(e->N1, e->srcCN, e->srcTA, &nSrcWords, &newSrcCN);
    
    //word18 newSrcAddr = e->srcAddr + nSrcWords;
    e->ADDR1.address += nSrcWords;

    
    //e->srcAddr2 = GETHI(e->OP2);
    //e->ADDR2.address = GETHI(e->OP2);
    
    // fetch 'test' char - double
    //If MF2.ID = 0 and MF2.REG = du, then the second word following the instruction word does not contain an operand descriptor for the test character; instead, it contains the test character as a direct upper operand in bits 0,8.
    
    int c1 = 0;
    int c2 = 0;
    
    if (!(e->MF2 & MFkID) && ((e->MF2 & MFkREGMASK) == 3))  // MF2.du
    {
        // per Bull RJ78, p. 5-45
        switch(e->srcTA)
        {
            case CTA4:
                c1 = (e->ADDR2.address >> 13) & 017;
                c2 = (e->ADDR2.address >>  9) & 017;
                break;
            case CTA6:
                c1 = (e->ADDR2.address >> 12) & 077;
                c2 = (e->ADDR2.address >>  6) & 077;

                break;
            case CTA9:
                c1 = (e->ADDR2.address >> 9) & 0777;
                c2 = (e->ADDR2.address     ) & 0777;
                
                break;
        }
    }
    else
    {
#if 0
        if (!(e->MF2 & MFkID))  // if id is set then we don't bother with MF2 reg as an address modifier
        {
            word18 y2offset = getMF2Reg(e->MF2 & MFkREGMASK, (word18)bitfieldExtract36(e->OP2, 27, 9));
            //e->srcAddr2 += y2offset;
            e->ADDR2.address += y2offset;
            e->ADDR2.address &= AMASK;
        }
#endif
        c1 = EISget469(&e->ADDR2, &e->srcCN2, e->srcTA2);
        c2 = EISget469(&e->ADDR2, &e->srcCN2, e->srcTA2);
    }
    switch(e->srcTA2)
    {
        case CTA4:
            c1 &= 017;    // keep 4-bits
            c2 &= 017;    // keep 4-bits
            break;
        case CTA6:
            c1 &= 077;    // keep 6-bits
            c2 &= 077;    // keep 6-bits
            break;
        case CTA9:
            c1 &= 0777;   // keep 9-bits
            c2 &= 0777;   // keep 9-bits
            break;
    }
    
    word18 y3 = GETHI(e->OP3);
    int y3A = (int)bitfieldExtract36(e->OP3, 6, 1); // 'A' bit - indirect via pointer register
    int y3REG = e->OP3 & 0xf;
    
    word36 r = getMFReg36(y3REG, false);
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    if (y3A)
    {
        // if 3rd operand contains A (bit-29 set) then it Means Y-char93 is not the memory address of the data but is a reference to a pointer register pointing to the data.
        uint n = (int)bitfieldExtract36(y3, 15, 3);
        word15 offset = y3 & MASK15;  // 15-bit signed number
        y3 = (AR[n].WORDNO + SIGNEXT15(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
        {
            //TPR.TSR = PR[n].SNR;
            //TPR.TRR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            e->ADDR3.SNR = PR[n].SNR;
            e->ADDR3.RNR = max3(PR[n].RNR, TPR.TRR, PPR.PRR);
            
            e->ADDR3.mat = viaPR;
        }
    }
    
    y3 +=  ((9*ARn_CHAR + 36*r + ARn_BITNO) / 36);
    y3 &= AMASK;
    
    e->ADDR3.address = y3;
    
    //get469r(NULL, 0, 0, 0);    // initialize char getter
    
    int yCharn11 = 0;
    int yCharn12 = 0;
    
    uint i = 0;
    for(; i < e->N1-1; i += 1)
    {
        // since we're going in reverse things are a bit different
        if (i == 0)
        {
            yCharn12 = EISget469r(&e->ADDR1, &newSrcCN, e->srcTA2);
            yCharn11 = EISget469r(&e->ADDR1, &newSrcCN, e->srcTA2);
        }
        else
        {
            yCharn12 = yCharn11;
            yCharn11 = EISget469r(&e->ADDR1, &newSrcCN, e->srcTA2);
        }
        
        if (yCharn11 == c1 && yCharn12 == c2)
            break;
    }
    
    word36 CY3 = bitfieldInsert36(0, i, 0, 24);
    
    SCF(i == e->N1-1, cu.IR, I_TALLY);
    
    // write Y3 .....
    //Write (y3, CY3, OperandWrite, 0);
    EISWrite(&e->ADDR3, CY3);
}

/*
 * get a bit from memory ....
 */
// XXX this is terribly ineffecient, but it'll do for now ......
// CANFAULT
static bool EISgetBit(EISaddr *p, int *cpos, int *bpos)
{
    //static word18 lastAddress;  // try to keep memory access' down
    
    if (!p)
    {
        //lastAddress = -1;
        return 0;
    }
    
    if (*bpos > 8)      // bits 0-8
    {
        *bpos = 0;
        *cpos += 1;
        if (*cpos > 3)  // chars 0-3
        {
            *cpos = 0;
            p->address += 1;
            p->address &= AMASK;
        }
    }
    
    //static word36 data;
    //if (p->lastAddress != p->address)                     // read from memory if different address
        p->data = EISRead(p); // read data word from memory
    
    int charPosn = ((3 - *cpos) * 9);     // 9-bit char bit position
    int bitPosn = charPosn + (8 - *bpos);
    
    bool b = (bool)bitfieldExtract36(p->data, bitPosn, 1);
    
    *bpos += 1;
    //p->lastAddress = p->address;
    
    return b;
}


/*
 * write a bit to memory (in the most ineffecient way possible)
 */

#ifndef QUIET_UNUSED
// CANFAULT
static void EISwriteBit(EISaddr *p, int *cpos, int *bpos, bool bit)
{
    if (*bpos > 8)      // bits 0-8
    {
        *bpos = 0;
        *cpos += 1;
        if (*cpos > 3)  // chars 0-3
        {
            *cpos = 0;
            p->address += 1;
            p->address &= AMASK;
        }
    }
    
    p->data = EISRead(p); // read data word from memory
    
    int charPosn = ((3 - *cpos) * 9);     // 9-bit char bit position
    int bitPosn = charPosn + (8 - *bpos);
    
    p->data = bitfieldInsert36(p->data, bit, bitPosn, 1);
    
    EISWrite(p, p->data); // write data word to memory
    
    *bpos += 1;
}
#endif

// CANFAULT
static bool EISgetBitRW(EISaddr *p)
{
    // make certain we have a valid address
    if (p->bPos > 8)      // bits 0-8
    {
        p->bPos = 0;
        p->cPos += 1;
        if (p->cPos > 3)  // chars 0-3
        {
            p->cPos = 0;
            p->address += 1;
            p->address &= AMASK;
        }
    }
    else if (p->bPos < 0)      // bits 0-8
    {
        p->bPos = 8;
        p->cPos -= 1;
        if (p->cPos < 0)  // chars 0-3
        {
            p->cPos = 3;
            p->address -= 1;
            p->address &= AMASK;
        }
    }
    
    int charPosn = ((3 - p->cPos) * 9);     // 9-bit char bit position
    int bitPosn = charPosn + (8 - p->bPos);
    
    //if (p->lastAddress != p->address)                     // read from memory if different address
        //Read(NULL, p->addr, &p->data, OperandRead, 0); // read data word from memory
    p->data = EISRead(p); // read data word from memory
    
    if (p->mode == eRWreadBit)
    {
        p->bit = (bool)bitfieldExtract36(p->data, bitPosn, 1);
        
        //if (p->lastAddress != p->address)                     // read from memory if different address
            p->data = EISRead(p); // read data word from memory
        
        // increment address after use
        if (p->incr)
            p->bPos += 1;
        // decrement address after use
        if (p->decr)
            p->bPos -= 1;
        
        
    } else if (p->mode == eRWwriteBit)
    {
        p->data = bitfieldInsert36(p->data, p->bit, bitPosn, 1);
        
        EISWrite(p, p->data); // write data word to memory
        
        if (p->incr)
            p->bPos += 1;
        if (p->decr)
            p->bPos -= 1;
    }
    //p->lastAddress = p->address;
    return p->bit;
}


/*
 * CMPB - Compare Bit Strings
 */
// CANFAULT
void cmpb(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    
    // For i = 1, 2, ..., minimum (N1,N2)
    //   C(Y-bit1)i-1 :: C(Y-bit2)i-1
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //   C(FILL) :: C(Y-bit2)i-1
    // If N1 > N2, then for i = N2+l, N2+2, ..., N1
    //   C(Y-bit1)i-1 :: C(FILL)
    //
    // Indicators:
    //    Zero:  If C(Y-bit1)i = C(Y-bit2)i for all i, then ON; otherwise, OFF
    //    Carry: If C(Y-bit1)i < C(Y-bit2)i for any i, then OFF; otherwise ON
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseBitstringOperandDescriptor(1, e);
    parseBitstringOperandDescriptor(2, e);
    
    //word18 srcAddr1 = e->YBit1;
    //word18 srcAddr2 = e->YBit2;
    
    int charPosn1 = e->C1;
    int charPosn2 = e->C2;
    
    int bitPosn1 = e->B1;
    int bitPosn2 = e->B2;
    
    e->F = (bool)bitfieldExtract36(e->op0, 25, 1) & 1;     // fill bit

    SETF(cu.IR, I_ZERO);  // assume all =
    SETF(cu.IR, I_CARRY); // assume all >=
    
    //getBit (0, 0, 0);   // initialize bit getter 1
    //getBit2(0, 0, 0);   // initialize bit getter 2
    
sim_debug (DBG_TRACEEXT, & cpu_dev, "cmpb N1 %d N2 %d\n", e -> N1, e -> N2);
    uint i;
    for(i = 0 ; i < min(e->N1, e->N2) ; i += 1)
    {
        //bool b1 = getBit (&srcAddr1, &charPosn1, &bitPosn1);
        //bool b2 = getBit2(&srcAddr2, &charPosn2, &bitPosn2);
        bool b1 = EISgetBit (&e->ADDR1, &charPosn1, &bitPosn1);
        bool b2 = EISgetBit (&e->ADDR2, &charPosn2, &bitPosn2);
        
sim_debug (DBG_TRACEEXT, & cpu_dev, "cmpb i %d b1 %d b2 %d\n", i, b1, b2);
        if (b1 != b2)
        {
            CLRF(cu.IR, I_ZERO);
            if (!b1 && b2)  // 0 < 1
                CLRF(cu.IR, I_CARRY);
            return;
        }
        
    }
    if (e->N1 < e->N2)
    {
        for(; i < e->N2 ; i += 1)
        {
            bool b1 = e->F;
            //bool b2 = getBit2(&srcAddr2, &charPosn2, &bitPosn2);
            bool b2 = EISgetBit(&e->ADDR2, &charPosn2, &bitPosn2);
sim_debug (DBG_TRACEEXT, & cpu_dev, "cmpb i %d b1fill %d b2 %d\n", i, b1, b2);
        
            if (b1 != b2)
            {
                CLRF(cu.IR, I_ZERO);
                if (!b1 && b2)  // 0 < 1
                    CLRF(cu.IR, I_CARRY);
                return;
            }
        }   
    } else if (e->N1 > e->N2)
    {
        for(; i < e->N1 ; i += 1)
        {
            //bool b1 = getBit(&srcAddr1, &charPosn1, &bitPosn1);
            bool b1 = EISgetBit(&e->ADDR1, &charPosn1, &bitPosn1);
            bool b2 = e->F;
sim_debug (DBG_TRACEEXT, & cpu_dev, "cmpb i %d b1 %d b2fill %d\n", i, b1, b2);
        
            if (b1 != b2)
            {
                CLRF(cu.IR, I_ZERO);
                if (!b1 && b2)  // 0 < 1
                    CLRF(cu.IR, I_CARRY);
                return;
            }
        }
    }
}

// CANFAULT
void csl(DCDstruct *ins, bool isSZTL)
{
    EISstruct *e = &ins->e;

    // For i = bits 1, 2, ..., minimum (N1,N2)
    //   m = C(Y-bit1)i-1 || C(Y-bit2)i-1 (a 2-bit number)
    //   C(BOLR)m → C(Y-bit2)i-1
    // If N1 < N2, then for i = N1+l, N1+2, ..., N2
    //   m = C(F) || C(Y-bit2)i-1 (a 2-bit number)
    //   C(BOLR)m → C(Y-bit2)i-1
    //
    // INDICATORS: (Indicators not listed are not affected)
    //     Zero If C(Y-bit2) = 00...0, then ON; otherwise OFF
    //     Truncation If N1 > N2, then ON; otherwise OFF
    //
    // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not
    // processed and the truncation indicator is set ON.
    //
    // If T = 1 and the truncation indicator is set ON by execution of the
    // instruction, then a truncation (overflow) fault occurs.
    //
    // BOLR
    // If first operand    and    second operand    then result
    // bit is:                    bit is:           is from bit:
    //        0                          0                      5
    //        0                          1                      6
    //        1                          0                      7
    //        1                          1                      8
    //
    // The Boolean operations most commonly used are
    //                  BOLR Field Bits
    // Operation        5      6      7      8
    //
    // MOVE             0      0      1      1
    // AND              0      0      0      1
    // OR               0      1      1      1
    // NAND             1      1      1      0
    // EXCLUSIVE OR     0      1      1      0
    // Clear            0      0      0      0
    // Invert           1      1      0      0
    //
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseBitstringOperandDescriptor(1, e);
    parseBitstringOperandDescriptor(2, e);
    
    e->ADDR1.cPos = e->C1;
    e->ADDR2.cPos = e->C2;
    
    e->ADDR1.bPos = e->B1;
    e->ADDR2.bPos = e->B2;
    
    e->F = (bool)bitfieldExtract36(e->op0, 35, 1);   // fill bit
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);   // T (enablefault) bit
    
    e->BOLR = (int)bitfieldExtract36(e->op0, 27, 4);  // BOLR field
    bool B5 = (bool)((e->BOLR >> 3) & 1);
    bool B6 = (bool)((e->BOLR >> 2) & 1);
    bool B7 = (bool)((e->BOLR >> 1) & 1);
    bool B8 = (bool)( e->BOLR      & 1);
    
    e->ADDR1.incr = true;
    e->ADDR1.mode = eRWreadBit;

    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "CSL N1 %d N2 %d\n"
               "CSL C1 %d C2 %d B1 %d B2 %d F %o T %d\n"
               "CSL BOLR %u%u%u%u\n"
               "CSL op1 SNR %06o WORDNO %06o CHAR %d BITNO %d\n"
               "CSL op2 SNR %06o WORDNO %06o CHAR %d BITNO %d\n",
               e -> N1, e -> N2,
               e -> C1, e -> C2, e -> B1, e -> B2, e -> F, e -> T,
               B5, B6, B7, B8,
               e -> addr [0] . SNR, e -> addr [0] . address, 
               e -> addr [0] . cPos, e -> addr [0] . bPos,
               e -> addr [1] . SNR, e -> addr [1] . address, 
               e -> addr [1] . cPos, e -> addr [1] . bPos);

    SETF(cu.IR, I_ZERO);      // assume all Y-bit2 == 0
    CLRF(cu.IR, I_TRUNC);     // assume N1 <= N2
    
    bool bR = false; // result bit
    uint i = 0;
    for(i = 0 ; i < min(e->N1, e->N2) ; i += 1)
    {
        bool b1 = EISgetBitRW(&e->ADDR1);  // read w/ addt incr from src 1
        
        // If we are a SZTL, addr2 is read only, increment here.
        // If we are a CSL, addr2 will be incremented below in the write cycle
        e->ADDR2.incr = isSZTL;
        e->ADDR2.mode = eRWreadBit;
        bool b2 = EISgetBitRW(&e->ADDR2);  // read w/ no addr incr from src2 to in anticipation of a write
        
        if (!b1 && !b2)
            bR = B5;
        else if (!b1 && b2)
            bR = B6;
        else if (b1 && !b2)
            bR = B7;
        else if (b1 && b2)
            bR = B8;
        
        if (bR)
        {
            CLRF(cu.IR, I_ZERO);
            if (isSZTL)
                break;
        }

        if (! isSZTL)
        {
            // write out modified bit
            e->ADDR2.bit = bR;              // set bit contents to write
            e->ADDR2.incr = true;           // we want address incrementing
            e->ADDR2.mode = eRWwriteBit;    // we want to write the bit
            EISgetBitRW(&e->ADDR2);    // write bit w/ addr increment to memory
        }
    }
    
    if (e->N1 < e->N2)
    {
        for(; i < e->N2 ; i += 1)
        {
            bool b1 = e->F;
            
            // If we are a SZTL, addr2 is read only, increment here.
            // If we are a CSL, addr2 will be incremented below in the write cycle
            e->ADDR2.incr = isSZTL;
            e->ADDR2.mode = eRWreadBit;
            bool b2 = EISgetBitRW(&e->ADDR2); // read w/ no addr incr from src2 to in anticipation of a write
            
            if (!b1 && !b2)
                bR = B5;
            else if (!b1 && b2)
                bR = B6;
            else if (b1 && !b2)
                bR = B7;
            else if (b1 && b2)
                bR = B8;
            
            if (bR)
            {
                CLRF(cu.IR, I_ZERO);
                if (isSZTL)
                  break;
            }
        
            if (! isSZTL)
            {
                // write out modified bit
                e->ADDR2.bit = bR;
                e->ADDR2.mode = eRWwriteBit;
                e->ADDR2.incr = true;
                EISgetBitRW(&e->ADDR2);
            }
        }
    } else if (e->N1 > e->N2)
    {
        // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not processed and the truncation indicator is set ON.
        //
        // If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
        
        SETF(cu.IR, I_TRUNC);
        if (e->T)
        {
            doFault(overflow_fault, 0, "csl truncation fault");
        }
    }
}


/*
 * return B (bit position), C (char position) and word offset given:
 *  'length' # of bits, etc ....
 *  'initC' initial char position (C)
 *  'initB' initial bit position
 */
static void getBitOffsets(int length, int initC, int initB, int *nWords, int *newC, int *newB)
{
    if (length == 0)
        return;
    
    int endBit = (length + 9 * initC + initB - 1) % 36;
    
    //int numWords = (length + 35) / 36;  ///< how many additional words will the bits take up?
    int numWords = (length + 9 * initC + initB + 35) / 36;  ///< how many additional words will the bits take up?
    int lastWordOffset = numWords - 1;
    
    if (lastWordOffset > 0)          // more that the 1 word needed?
        *nWords = lastWordOffset;  // # of additional words
    else
        *nWords = 0;    // no additional words needed
    
    *newC = endBit / 9; // last character number
    *newB = endBit % 9; // last bit number
}

// CANFAULT
void csr(DCDstruct *ins, bool isSZTR)
{
    EISstruct *e = &ins->e;

    // For i = bits 1, 2, ..., minimum (N1,N2)
    //   m = C(Y-bit1)N1-i || C(Y-bit2)N2-i (a 2-bit number)
    //   C(BOLR)m → C( Y-bit2)N2-i
    // If N1 < N2, then for i = N1+i, N1+2, ..., N2
    //   m = C(F) || C(Y-bit2)N2-i (a 2-bit number)
    //    C(BOLR)m → C( Y-bit2)N2-i
    // INDICATORS: (Indicators not listed are not affected)
    //     Zero If C(Y-bit2) = 00...0, then ON; otherwise OFF
    //     Truncation If N1 > N2, then ON; otherwise OFF
    //
    // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not processed and the truncation indicator is set ON.
    //
    // If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
    //
    // BOLR
    // If first operand    and    second operand    then result
    // bit is:                    bit is:           is from bit:
    //        0                          0                      5
    //        0                          1                      6
    //        1                          0                      7
    //        1                          1                      8
    //
    // The Boolean operations most commonly used are
    //                  BOLR Field Bits
    // Operation        5      6      7      8
    //
    // MOVE             0      0      1      1
    // AND              0      0      0      1
    // OR               0      1      1      1
    // NAND             1      1      1      0
    // EXCLUSIVE OR     0      1      1      0
    // Clear            0      0      0      0
    // Invert           1      1      0      0
    //
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseBitstringOperandDescriptor(1, e);
    parseBitstringOperandDescriptor(2, e);
    
    e->ADDR1.cPos = e->C1;
    e->ADDR2.cPos = e->C2;
    
    e->ADDR1.bPos = e->B1;
    e->ADDR2.bPos = e->B2;
    
    // get new char/bit offsets
    int numWords1=0, numWords2=0;
    
    getBitOffsets(e->N1, e->C1, e->B1, &numWords1, &e->ADDR1.cPos, &e->ADDR1.bPos);
    e->ADDR1.address += numWords1;
        
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "CSR N1 %d C1 %d B1 %d numWords1 %d cPos %d bPos %d\n",
               e->N1, e->C1, e->B1, numWords1, e->ADDR1.cPos, e->ADDR1.bPos);
    getBitOffsets(e->N2, e->C2, e->B2, &numWords2, &e->ADDR2.cPos, &e->ADDR2.bPos);
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "CSR N2 %d C2 %d B2 %d numWords2 %d cPos %d bPos %d\n",
               e->N2, e->C2, e->B2, numWords2, e->ADDR2.cPos, e->ADDR2.bPos);
    e->ADDR2.address += numWords2;
    
    e->F = (bool)bitfieldExtract36(e->op0, 35, 1);   // fill bit
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);   // T (enablefault) bit
    
    e->BOLR = (int)bitfieldExtract36(e->op0, 27, 4);  // BOLR field
    bool B5 = (bool)((e->BOLR >> 3) & 1);
    bool B6 = (bool)((e->BOLR >> 2) & 1);
    bool B7 = (bool)((e->BOLR >> 1) & 1);
    bool B8 = (bool)( e->BOLR      & 1);
    
    
    e->ADDR1.decr = true;
    e->ADDR1.mode = eRWreadBit;
    
    SETF(cu.IR, I_ZERO);      // assume all Y-bit2 == 0
    CLRF(cu.IR, I_TRUNC);     // assume N1 <= N2
    
    bool bR = false; // result bit
    
    uint i = 0;
    for(i = 0 ; i < min(e->N1, e->N2) ; i += 1)
    {
        bool b1 = EISgetBitRW(&e->ADDR1);  // read w/ addt decr from src 1
        
        // If we are a SZTR, addr2 is read only, decrement here.
        // If we are a CSR, addr2 will be decremented below in the write cycle
        e->ADDR2.decr = isSZTR;
        e->ADDR2.mode = eRWreadBit;
        bool b2 = EISgetBitRW(&e->ADDR2);  // read w/ no addr decr from src2 to in anticipation of a write
        
        if (!b1 && !b2)
            bR = B5;
        else if (!b1 && b2)
            bR = B6;
        else if (b1 && !b2)
            bR = B7;
        else if (b1 && b2)
            bR = B8;
        
        if (bR)
        {
            CLRF(cu.IR, I_ZERO);
            if (isSZTR)
                break;
        }
        
        if (! isSZTR)
        {
            // write out modified bit
            e->ADDR2.bit = bR;              // set bit contents to write
            e->ADDR2.decr = true;           // we want address incrementing
            e->ADDR2.mode = eRWwriteBit;    // we want to write the bit
            EISgetBitRW(&e->ADDR2);  // write bit w/ addr increment to memory
        }
    }
    
    if (e->N1 < e->N2)
    {
        for(; i < e->N2 ; i += 1)
        {
            bool b1 = e->F;
            
            // If we are a SZTR, addr2 is read only, decrement here.
            // If we are a CSR, addr2 will be decremented below in the write cycle
            e->ADDR2.decr = isSZTR;
            e->ADDR2.mode = eRWreadBit;
            bool b2 = EISgetBitRW(&e->ADDR2); // read w/ no addr decr from src2 to in anticipation of a write
            
            if (!b1 && !b2)
                bR = B5;
            else if (!b1 && b2)
                bR = B6;
            else if (b1 && !b2)
                bR = B7;
            else if (b1 && b2)
                bR = B8;
            
            if (bR)
            {
                CLRF(cu.IR, I_ZERO);
                if (isSZTR)
                  break;
            }
        
            if (! isSZTR)
            {
                // write out modified bit
                e->ADDR2.bit = bR;
                e->ADDR2.mode = eRWwriteBit;
                e->ADDR2.decr = true;
                EISgetBitRW(&e->ADDR2);
            }
        }
    } else if (e->N1 > e->N2)
    {
        // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not processed and the truncation indicator is set ON.
        //
        // If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
        
        SETF(cu.IR, I_TRUNC);
        if (e->T)
        {
            doFault(overflow_fault, 0, "csr truncation fault");
            //sim_printf("fault: 0 0 'csr truncation fault'\n");
        }
    }
}


#if 0
// CANFAULT
void sztl(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    // For i = bits 1, 2, ..., minimum (N1,N2)
    //    m = C(Y-bit1)i-1 || C(Y-bit2)i-1 (a 2-bit number)
    //    If C(BOLR)m ≠ 0, then terminate
    // If N1 < N2, then for i = N1+i, N1+2, ..., N2
    //    m = C(F) || C(Y-bit2)i-1 (a 2-bit number)
    //    If C(BOLR)m ≠ 0, then terminate
    //
    // INDICATORS: (Indicators not listed are not affected)
    //     Zero If C(BOLR)m = 0 for all i, then ON; otherwise OFF
    //     Truncation If N1 > N2, then ON; otherwise OFF
    //
    // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not processed and the truncation indicator is set ON.
    //
    // If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
    //
    // BOLR
    // If first operand    and    second operand    then result
    // bit is:                    bit is:           is from bit:
    //        0                          0                      5
    //        0                          1                      6
    //        1                          0                      7
    //        1                          1                      8
    //
    // The Boolean operations most commonly used are
    //                  BOLR Field Bits
    // Operation        5      6      7      8
    //
    // MOVE             0      0      1      1
    // AND              0      0      0      1
    // OR               0      1      1      1
    // NAND             1      1      1      0
    // EXCLUSIVE OR     0      1      1      0
    // Clear            0      0      0      0
    // Invert           1      1      0      0
    //
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseBitstringOperandDescriptor(1, e);
    parseBitstringOperandDescriptor(2, e);
    
    e->ADDR1.cPos = e->C1;
    e->ADDR2.cPos = e->C2;
    
    e->ADDR1.bPos = e->B1;
    e->ADDR2.bPos = e->B2;
    
    e->F = (bool)bitfieldExtract36(e->op0, 35, 1);   // fill bit
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);   // T (enablefault) bit
    
    e->BOLR = (int)bitfieldExtract36(e->op0, 27, 4);  // BOLR field
    bool B5 = (bool)((e->BOLR >> 3) & 1);
    bool B6 = (bool)((e->BOLR >> 2) & 1);
    bool B7 = (bool)((e->BOLR >> 1) & 1);
    bool B8 = (bool)( e->BOLR      & 1);
    
    e->ADDR1.incr = true;
    e->ADDR1.mode = eRWreadBit;
    e->ADDR2.incr = false;
    e->ADDR2.mode = eRWreadBit;
    
    SETF(cu.IR, I_ZERO);      // assume all C(BOLR) == 0
    CLRF(cu.IR, I_TRUNC);     // N1 >= N2
    
    bool bR = false; // result bit
    
    uint i = 0;
    for(i = 0 ; i < min(e->N1, e->N2) ; i += 1)
    {
        bool b1 = EISgetBitRW(&e->ADDR1);  // read w/ addr incr from src 1
        bool b2 = EISgetBitRW(&e->ADDR2);  // read w/ addr incr from src 2
        
        if (!b1 && !b2)
            bR = B5;
        else if (!b1 && b2)
            bR = B6;
        else if (b1 && !b2)
            bR = B7;
        else if (b1 && b2)
            bR = B8;
        
        if (bR)
        {
            CLRF(cu.IR, I_ZERO);
            break;
        }
    }
    
    if (e->N1 < e->N2)
    {
        for(; i < e->N2 ; i += 1)
        {
            bool b1 = e->F;
            bool b2 = EISgetBitRW(&e->ADDR2); // read w/ addr incr from src2
                        
            if (!b1 && !b2)
                bR = B5;
            else if (!b1 && b2)
                bR = B6;
            else if (b1 && !b2)
                bR = B7;
            else if (b1 && b2)
                bR = B8;
            
            if (bR)
            {
                CLRF(cu.IR, I_ZERO);
                break;
            }
        }
    } else if (e->N1 > e->N2)
    {
        // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not processed and the truncation indicator is set ON.
        //
        // If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
        
        SETF(cu.IR, I_TRUNC);
        if (e->T)
        {
            doFault(overflow_fault, 0, "sztl truncation fault");
        }
    }
}
#endif


#if 0
// CANFAULT
void sztr(DCDstruct *ins)
{
    EISstruct *e = &ins->e;

    //
    // For i = bits 1, 2, ..., minimum (N1,N2)
    //   m = C(Y-bit1)N1-i || C(Y-bit2)N2-i (a 2-bit number)
    //   If C(BOLR)m ≠ 0, then terminate
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //   m = C(F) || C(Y-bit2)N2-i (a 2-bit number)
    //   If C(BOLR)m ≠ 0, then terminate
    //
    // INDICATORS: (Indicators not listed are not affected)
    //     Zero If C(Y-bit2) = 00...0, then ON; otherwise OFF
    //     Truncation If N1 > N2, then ON; otherwise OFF
    //
    // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not processed and the truncation indicator is set ON.
    //
    // If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
    //
    // BOLR
    // If first operand    and    second operand    then result
    // bit is:                    bit is:           is from bit:
    //        0                          0                      5
    //        0                          1                      6
    //        1                          0                      7
    //        1                          1                      8
    //
    // The Boolean operations most commonly used are
    //                  BOLR Field Bits
    // Operation        5      6      7      8
    //
    // MOVE             0      0      1      1
    // AND              0      0      0      1
    // OR               0      1      1      1
    // NAND             1      1      1      0
    // EXCLUSIVE OR     0      1      1      0
    // Clear            0      0      0      0
    // Invert           1      1      0      0
    //
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseBitstringOperandDescriptor(1, e);
    parseBitstringOperandDescriptor(2, e);
    
    e->ADDR1.cPos = e->C1;
    e->ADDR2.cPos = e->C2;
    
    e->ADDR1.bPos = e->B1;
    e->ADDR2.bPos = e->B2;
    
    // get new char/bit offsets
    int numWords1=0, numWords2=0;
    
    getBitOffsets(e->N1, e->C1, e->B1, &numWords1, &e->ADDR1.cPos, &e->ADDR1.bPos);
    e->ADDR1.address += numWords1;
    
    getBitOffsets(e->N2, e->C2, e->B2, &numWords2, &e->ADDR2.cPos, &e->ADDR2.bPos);
    e->ADDR2.address += numWords2;
    
    e->F = (bool)bitfieldExtract36(e->op0, 35, 1);   // fill bit
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);   // T (enablefault) bit
    
    e->BOLR = (int)bitfieldExtract36(e->op0, 27, 4);  // BOLR field
    bool B5 = (bool)((e->BOLR >> 3) & 1);
    bool B6 = (bool)((e->BOLR >> 2) & 1);
    bool B7 = (bool)((e->BOLR >> 1) & 1);
    bool B8 = (bool)( e->BOLR      & 1);
    
    e->ADDR1.decr = true;
    e->ADDR1.mode = eRWreadBit;
    e->ADDR2.decr = true;
    e->ADDR2.mode = eRWreadBit;
    
    SETF(cu.IR, I_ZERO);      // assume all Y-bit2 == 0
    CLRF(cu.IR, I_TRUNC);     // assume N1 <= N2
    
    bool bR = false; // result bit
    
    uint i = 0;
    for(i = 0 ; i < min(e->N1, e->N2) ; i += 1)
    {
        bool b1 = EISgetBitRW(&e->ADDR1);  // read w/ addr incr from src 1
        bool b2 = EISgetBitRW(&e->ADDR2);  // read w/ addr incr from src 2
        
        if (!b1 && !b2)
            bR = B5;
        else if (!b1 && b2)
            bR = B6;
        else if (b1 && !b2)
            bR = B7;
        else if (b1 && b2)
            bR = B8;
        
        if (bR)
        {
            CLRF(cu.IR, I_ZERO);
            break;
        }
    }
    
    if (e->N1 < e->N2)
    {
        for(; i < e->N2 ; i += 1)
        {
            bool b1 = e->F;
            bool b2 = EISgetBitRW(&e->ADDR2); // read w/ no addr incr from src2 to in anticipation of a write
            
            if (!b1 && !b2)
                bR = B5;
            else if (!b1 && b2)
                bR = B6;
            else if (b1 && !b2)
                bR = B7;
            else if (b1 && b2)
                bR = B8;
            
            if (bR)
            {
                CLRF(cu.IR, I_ZERO);
                break;
            }
        }
    } else if (e->N1 > e->N2)
    {
        // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not processed and the truncation indicator is set ON.
        //
        // If T = 1 and the truncation indicator is set ON by execution of the instruction, then a truncation (overflow) fault occurs.
        
        SETF(cu.IR, I_TRUNC);
        if (e->T)
        {
            doFault(overflow_fault, 0, "sztr truncation fault");
        }
    }
}
#endif

/*
 * EIS decimal arithmetic routines are to be found in dps8_decimal.c .....
 */

