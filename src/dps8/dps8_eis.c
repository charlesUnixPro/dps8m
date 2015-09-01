/**
 * \file dps8_eis.c
 * \project dps8
 * \date 12/31/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
 * \brief EIS support code...
*/

#define V4

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

#ifdef V4
#define isDecimalZero(c) ((e->srcTA == CTA9) ? \
                            ((c) == '0') : \
                            (((c) & 017) == 0)) 
#endif

#include <stdio.h>
//#define DBGF // page fault debugging
//#define DBGX
#include <ctype.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
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


#ifdef DBGF
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

    word3 saveTRR = TPR . TRR;

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
#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
          {
            TPR . TRR = PPR . PRR;
            TPR . TSR = PPR . PSR;
          }
        
        Read (p -> address + woffset, & data, EIS_OPERAND_READ, false);
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read %012llo@%o:%06o\n", 
                   __func__, data, TPR . TSR, p -> address);
      }
    TPR . TRR = saveTRR;

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
#endif

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
          //doFault (FAULT_IPR, 0, "illegal TAk");
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
          //doFault (FAULT_IPR, 0, "illegal TAk");
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
            address = (AR [arn] . WORDNO + SIGNEXT15_18 (offset)) & AMASK;

#if 0
            if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
            if (get_addr_mode() == APPEND_mode)
#endif
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
        address = (AR [arn] . WORDNO + SIGNEXT15_18 (offset)) & AMASK;

#if 0 // we are not actually going to do the read
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
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

#ifdef EIS_CACHE
// CANFAULT
static void EISWriteCache(EISaddr *p)
{
    word3 saveTRR = TPR . TRR;

    if (p -> cacheValid && p -> cacheDirty)
      {
        if (p->mat == viaPR)
        {
            TPR.TRR = p->RNR;
            TPR.TSR = p->SNR;
        
            sim_debug (DBG_TRACEEXT, & cpu_dev, 
                       "%s: writeCache (PR) %012llo@%o:%06o\n", 
                       __func__, p -> cachedWord, p -> SNR, p -> cachedAddr);
            Write (p->cachedAddr, p -> cachedWord, EIS_OPERAND_STORE, true); // write data
        }
        else
        {
#if 0
            if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
            if (get_addr_mode() == APPEND_mode)
#endif
            {
                TPR.TRR = PPR.PRR;
                TPR.TSR = PPR.PSR;
            }
        
            sim_debug (DBG_TRACEEXT, & cpu_dev, 
                       "%s: writeCache %012llo@%o:%06o\n", 
                       __func__, p -> cachedWord, TPR . TSR, p -> cachedAddr);
            Write (p->cachedAddr, p -> cachedWord, EIS_OPERAND_STORE, false); // write data
        }
    }
    p -> cacheDirty = false;
    TPR . TRR = saveTRR;
  }
#endif

// CANFAULT
static void EISWrite(EISaddr *p, word36 data)
{
    word3 saveTRR = TPR . TRR;
#ifdef EIS_CACHE
    if (p -> cacheValid && p -> cacheDirty && p -> cachedAddr != p -> address)
      {
        EISWriteCache (p);
      }
    p -> cacheValid = true;
    p -> cacheDirty = true;
    p -> cachedAddr = p -> address;
    p -> cachedWord = data;
#else
    if (p->mat == viaPR)
    {
        TPR.TRR = p->RNR;
        TPR.TSR = p->SNR;
        
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: write %012llo@%o:%06o\n", __func__, data, p -> SNR, p -> address);
        Write (p->address, data, EIS_OPERAND_STORE, true); // write data
    }
    else
    {
#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
        {
            TPR.TRR = PPR.PRR;
            TPR.TSR = PPR.PSR;
        }
        
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: write %012llo@%o:%06o\n", __func__, data, TPR . TSR, p -> address);
        Write (p->address, data, EIS_OPERAND_STORE, false); // write data
    }
#endif
    TPR . TRR = saveTRR;
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
            address = (AR [arn] . WORDNO + SIGNEXT15_18 (offset)) & AMASK;

#if 0
            if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
            if (get_addr_mode() == APPEND_mode)
#endif
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
        address = (AR [arn] . WORDNO + SIGNEXT15_18 (offset)) & AMASK;

#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
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

    word3 saveTRR = TPR . TRR;

#ifdef EIS_CACHE
#ifndef EIS_CACHE_READTEST
    if (p -> cacheValid && p -> cachedAddr == p -> address)
      {
        return p -> cachedWord;
      }
    if (p -> cacheValid && p -> cacheDirty)
      {
        EISWriteCache (p);
      }
    p -> cacheDirty = false;
#endif
#endif

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
#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
        {
            TPR.TRR = PPR.PRR;
            TPR.TSR = PPR.PSR;
        }
        
        Read (p->address, &data, EIS_OPERAND_READ, false);  // read operand
        sim_debug (DBG_TRACEEXT, & cpu_dev, "%s: read %012llo@%o:%06o\n", __func__, data, TPR . TSR, p -> address);
    }
#ifdef EIS_CACHE
#ifdef EIS_CACHE_READTEST
    if (p -> cacheValid && p -> cachedAddr == p -> address)
      {
        if (p -> cachedData != * data)
          {
            sim_printf ("cache read data fail %012llo %012llo %08o\n",
                        p -> cachedData, * data, p -> address);
          }
      }
#endif
    p -> cacheValid = true;
    p -> cachedAddr = p -> address;
    p -> cachedWord = data;
#endif
    TPR . TRR = saveTRR;
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

static word18 getMFReg18 (uint n, bool UNUSED allowDUL)
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
             //doFault (FAULT_IPR, ill_proc, "getMFReg18 du");
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
             doFault (FAULT_IPR, ill_mod, "getMFReg18 dl");
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


static word36 getMFReg36 (uint n, bool UNUSED allowDUL)
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
             //doFault (FAULT_IPR, ill_proc, "getMFReg36 du");
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
             doFault (FAULT_IPR, ill_mod, "getMFReg36 dl");
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

#ifdef EIS_CACHE
void setupOperandDescriptorCache(int k, EISstruct *e)
  {
    e -> addr [k - 1] .  cacheValid = false;
  }
#endif


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
            address = (AR[n].WORDNO + SIGNEXT15_18(offset)) & AMASK;

            e->addr[k-1].address = address;
#if 0
            if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
            if (get_addr_mode() == APPEND_mode)
#endif
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

#ifdef EIS_CACHE
    setupOperandDescriptorCache (k, e);
#endif
}

#ifdef EIS_CACHE
// CANFAULT
void cleanupOperandDescriptor(int k, EISstruct *e)
  {
    if (e -> addr [k - 1] . cacheValid && e -> addr [k - 1] . cacheDirty)
      {
        EISWriteCache(& e -> addr [k - 1]);
      }
    e -> addr [k - 1] . cacheDirty = false;
  }
#endif

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
        word18 offset = SIGNEXT15_18 (address);  // 15-bit signed number
        address = (AR [n] . WORDNO + offset) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
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
            sim_debug (DBG_TRACEEXT, & cpu_dev, "CN%d set to %d by CTA4\n",
                       k, e->CN[k-1]);
            break;
        case CTA6:
            e->effBITNO = (9*ARn_CHAR + 6*r + ARn_BITNO) % 9;
            e->effCHAR = ((6*CN + 9*ARn_CHAR + 6*r + ARn_BITNO) % 36) / 6;//9;
            e->effWORDNO = address + (6*CN + 9*ARn_CHAR + 6*r + ARn_BITNO) / 36;
            e->effWORDNO &= AMASK;
            
            //e->YChar6[k-1] = e->effWORDNO;
            e->CN[k-1] = e->effCHAR;   // ??????
            sim_debug (DBG_TRACEEXT, & cpu_dev, "CN%d set to %d by CTA6\n",
                       k, e->CN[k-1]);
            break;
        case CTA9:
            CN = (CN >> 1) & 07;  // XXX Do error checking
            
            e->effBITNO = 0;
            e->effCHAR = (CN + ARn_CHAR + r) % 4;
            sim_debug (DBG_TRACEEXT, & cpu_dev, 
                       "effCHAR %d = (CN %d + ARn_CHAR %d + r %lld) %% 4)\n",
                       e->effCHAR, CN, ARn_CHAR, r);
            e->effWORDNO = address + ((9*CN + 9*ARn_CHAR + 9*r + ARn_BITNO) / 36);
            e->effWORDNO &= AMASK;
            
            //e->YChar9[k-1] = e->effWORDNO;
            e->CN[k-1] = e->effCHAR;   // ??????
            sim_debug (DBG_TRACEEXT, & cpu_dev, "CN%d set to %d by CTA9\n",
                       k, e->CN[k-1]);
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
        address = (AR[n].WORDNO + SIGNEXT15_18(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
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
    e->SF[k-1] = (int)SIGNEXT6_int(bitfieldExtract36(opDesc, 6, 6));    // Scaling factor.
    
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
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, "parseNumericOperandDescriptor(): N%u %u\n", k, e->N[k-1]);

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
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, "parseNumericOperandDescriptor(): address:%06o cPos:%d bPos:%d N%u %u\n", a->address, a->cPos, a->bPos, k, e->N[k-1]);

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
        address = (AR[n].WORDNO + SIGNEXT15_18(offset)) & AMASK;
sim_debug (DBG_TRACEEXT, & cpu_dev, "bitstring k %d AR%d\n", k, n);
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
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
        int maxPos = 4;
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
                        doFault(FAULT_IPR, ill_dig, "loadInputBufferNumric(1): illegal char in input"); // TODO: generate ill proc fault

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
                        doFault(FAULT_IPR,ill_dig,"loadInputBufferNumric(2): illegal char in input"); // TODO: generate ill proc fault

                    *p++ = c; // store 4-bit char in buffer
                }
                break;

             case CSLS:
                // Only the byte values [0,11]8 are legal in digit positions and only the byte values [12,17]8 are legal in sign positions. Detection of an illegal byte value causes an illegal procedure fault
                c &= 0xf;   // hack off all but lower 4 bits

                if (n == 0) // first had better be a sign ....
                {
                    if (c < 012 || c > 017)
                        doFault(FAULT_IPR,ill_dig,"loadInputBufferNumric(3): illegal char in input"); // TODO: generate ill proc fault
                    if (c == 015)   // '-'
                        e->sign = -1;
                    e->srcTally -= 1;   // 1 less source char
                }
                else
                {
                    if (c > 011)
                        doFault(FAULT_IPR, ill_dig,"loadInputBufferNumric(4): illegal char in input"); // XXX generate ill proc fault
                    *p++ = c; // store 4-bit char in buffer
                }
                break;

            case CSTS:
                c &= 0xf;   // hack off all but lower 4 bits

                if (n == N-1) // last had better be a sign ....
                {
                    if (c < 012 || c > 017)
                         doFault(FAULT_IPR, ill_dig,"loadInputBufferNumric(5): illegal char in input"); // XXX generate ill proc fault; // XXX generate ill proc fault
                    if (c == 015)   // '-'
                        e->sign = -1;
                    e->srcTally -= 1;   // 1 less source char
                }
                else
                {
                    if (c > 011)
                        doFault(FAULT_IPR, ill_dig,"loadInputBufferNumric(6): illegal char in input"); // XXX generate ill proc fault
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
#if 0
sim_printf ("inBuffer:");
        for (word9 *q = e->inBuffer; q < p; q ++)
          sim_printf (" %02o", * q);
        sim_printf ("\n");
#endif
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


#if 0 // UNUSED
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
#endif






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
static void EISwriteBit(EISaddr *p, int *cpos, int *bpos, word1 bit)
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

#if 0
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
        p->bit = bitfieldExtract36(p->data, bitPosn, 1);
        
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
#endif

// CANFAULT
static bool EISgetBitRWN (EISaddr * p)
  {
#if 0
    return EISgetBitRW (p);
#else
//sim_printf ("cPos %d bPos %d\n", p->cPos, p->bPos);
    int baseCharPosn = (p -> cPos * 9);     // 9-bit char bit position
    int baseBitPosn = baseCharPosn + p -> bPos;
//sim_printf ("baseCharPosn %d baseBitPosn %d\n", baseCharPosn, baseBitPosn);
    baseBitPosn += du . CHTALLY;
//sim_printf ("CHTALLY %d baseBitPosn %d\n", du . CHTALLY, baseBitPosn);

    int bitPosn = baseBitPosn % 36;
    int woff = baseBitPosn / 36;
//sim_printf ("bitPosn %d woff %d\n", bitPosn, woff);

    word18 saveAddr = p -> address;
    p -> address += woff;

    p -> data = EISRead (p); // read data word from memory
//if (PPR . PSR == 0400) sim_printf ("addr %08o pos %d\n", p->address, bitPosn);  
    
    if (p -> mode == eRWreadBit)
      {
        //p -> bit = (bool) bitfieldExtract36 (p -> data, bitPosn, 1);
        p -> bit = getbits36 (p -> data, bitPosn, 1);
      } 
    else if (p -> mode == eRWwriteBit)
      {
        //p -> data = bitfieldInsert36 (p -> data, p -> bit, bitPosn, 1);
        p -> data = setbits36 (p -> data, bitPosn, 1, p -> bit);
        
        EISWrite (p, p -> data); // write data word to memory
      }

    p -> address = saveAddr;
    return p -> bit;
#endif
  }

// CANFAULT
static bool EISgetBitRWNR (EISaddr * p)
  {
//sim_printf ("cPos %d bPos %d\n", p->cPos, p->bPos);
    int baseCharPosn = (p -> cPos * 9);     // 9-bit char bit position
    int baseBitPosn = baseCharPosn + p -> bPos;
//sim_printf ("baseCharPosn %d baseBitPosn %d\n", baseCharPosn, baseBitPosn);
    baseBitPosn -= du . CHTALLY;
//sim_printf ("CHTALLY %d baseBitPosn %d\n", du . CHTALLY, baseBitPosn);

    int bitPosn = baseBitPosn % 36;
    int woff = baseBitPosn / 36;
    while (bitPosn < 0)
      {
        bitPosn += 36;
        woff -= 1;
      }
if (bitPosn < 0) {
sim_printf ("cPos %d bPos %d\n", p->cPos, p->bPos);
sim_printf ("baseCharPosn %d baseBitPosn %d\n", baseCharPosn, baseBitPosn);
sim_printf ("CHTALLY %d baseBitPosn %d\n", du . CHTALLY, baseBitPosn);
sim_printf ("bitPosn %d woff %d\n", bitPosn, woff);
sim_err ("oops\n");
}
//sim_printf ("bitPosn %d woff %d\n", bitPosn, woff);

    word18 saveAddr = p -> address;
    p -> address += woff;

    p -> data = EISRead (p); // read data word from memory
//if (PPR . PSR == 0400) sim_printf ("addr %08o pos %d\n", p->address, bitPosn);  
    
    if (p -> mode == eRWreadBit)
      {
        //p -> bit = (bool) bitfieldExtract36 (p -> data, bitPosn, 1);
        p -> bit = getbits36 (p -> data, bitPosn, 1);
      } 
    else if (p -> mode == eRWwriteBit)
      {
        //p -> data = bitfieldInsert36 (p -> data, p -> bit, bitPosn, 1);
        p -> data = setbits36 (p -> data, bitPosn, 1, p -> bit);
        
        EISWrite (p, p -> data); // write data word to memory
      }

    p -> address = saveAddr;
    return p -> bit;
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
    
    e->F = bitfieldExtract36(e->op0, 35, 1) != 0;   // fill bit
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;   // T (enablefault) bit
    
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
            doFault(FAULT_OFL, 0, "sztl truncation fault");
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
    
    e->F = bitfieldExtract36(e->op0, 35, 1) != 0;   // fill bit
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;   // T (enablefault) bit
    
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
            doFault(FAULT_OFL, 0, "sztr truncation fault");
        }
    }
}
#endif

/*
 * EIS decimal arithmetic routines are to be found in dps8_decimal.c .....
 */

