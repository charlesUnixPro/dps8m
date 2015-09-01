#include <ctype.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_iefp.h"
#include "dps8_decimal.h"

// Local optimization
#define ABD_BITS

//  EISWriteCache  -- flush the cache
//
//
//  EISWriteIdx (p, n, data); -- write to cache at p->addr [n]; 
//  EISRead (p) -- read to cache from p->addr
//  EISReadIdx (p, n)  -- read to cache from p->addr[n]; 
//  EISReadN (p, n, dst) -- read N words to dst; 
 
//  EISget469 (k, i)
//  EISput469 (k, i, c)

//  EISget49 (k, *pos, tn) get p->addr[*pos++]

static word4 get4 (word36 w, int pos)
  {
    switch (pos)
      {
        case 0:
         return bitfieldExtract36 (w, 31, 4);

        case 1:
          return bitfieldExtract36 (w, 27, 4);

        case 2:
          return bitfieldExtract36 (w, 22, 4);

        case 3:
          return bitfieldExtract36 (w, 18, 4);

        case 4:
          return bitfieldExtract36 (w, 13, 4);

        case 5:
          return bitfieldExtract36 (w, 9, 4);

        case 6:
          return bitfieldExtract36 (w, 4, 4);

        case 7:
          return bitfieldExtract36 (w, 0, 4);

      }
    sim_printf ("get4(): How'd we get here?\n");
    return 0;
}

static word4 get6 (word36 w, int pos)
  {
    switch (pos)
      {
        case 0:
          return bitfieldExtract36 (w, 30, 6);

        case 1:
          return bitfieldExtract36 (w, 24, 6);

        case 2:
          return bitfieldExtract36 (w, 18, 6);

        case 3:
          return bitfieldExtract36 (w, 12, 6);

        case 4:
          return bitfieldExtract36 (w, 6, 6);

        case 5:
          return bitfieldExtract36 (w, 0, 6);

      }
    sim_printf ("get6(): How'd we get here?\n");
    return 0;
  }

static word9 get9(word36 w, int pos)
  {
    
    switch (pos)
      {
        case 0:
          return bitfieldExtract36 (w, 27, 9);

        case 1:
          return bitfieldExtract36 (w, 18, 9);

        case 2:
          return bitfieldExtract36 (w, 9, 9);

        case 3:
          return bitfieldExtract36 (w, 0, 9);

      }
    sim_printf ("get9(): How'd we get here?\n");
    return 0;
  }

static word36 put4 (word36 w, int pos, word6 c)
  {
    switch (pos)
      {
        case 0:
         return bitfieldInsert36 (w, c, 31, 4);

        case 1:
          return bitfieldInsert36 (w, c, 27, 4);

        case 2:
          return bitfieldInsert36 (w, c, 22, 4);

        case 3:
          return bitfieldInsert36 (w, c, 18, 4);

        case 4:
          return bitfieldInsert36 (w, c, 13, 4);

        case 5:
          return bitfieldInsert36 (w, c, 9, 4);

        case 6:
          return bitfieldInsert36 (w, c, 4, 4);

        case 7:
          return bitfieldInsert36 (w, c, 0, 4);

      }
    sim_printf ("put4(): How'd we get here?\n");
    return 0;
  }

static word36 put6 (word36 w, int pos, word4 c)
  {
    switch (pos)
      {
        case 0:
          return bitfieldInsert36 (w, c, 30, 6);

        case 1:
          return bitfieldInsert36 (w, c, 24, 6);

        case 2:
          return bitfieldInsert36 (w, c, 18, 6);

        case 3:
          return bitfieldInsert36 (w, c, 12, 6);

        case 4:
          return bitfieldInsert36 (w, c, 6, 6);

        case 5:
          return bitfieldInsert36 (w, c, 0, 6);

      }
    sim_printf ("put6(): How'd we get here?\n");
    return 0;
  }

static word36 put9 (word36 w, int pos, word9 c)
  {
    
    switch (pos)
      {
        case 0:
          return bitfieldInsert36 (w, c, 27, 9);

        case 1:
          return bitfieldInsert36 (w, c, 18, 9);

        case 2:
          return bitfieldInsert36 (w, c, 9, 9);

        case 3:
          return bitfieldInsert36 (w, c, 0, 9);

      }
    sim_printf ("put9(): How'd we get here?\n");
    return 0;
  }

/**
 * get register value indicated by reg for Address Register operations
 * (not for use with address modifications)
 */

static word36 getCrAR (word4 reg)
  {
    if (reg == 0)
      return 0;
    
    if (reg & 010) /* Xn */
      return rX [X (reg)];
    
    switch (reg)
      {
        case TD_N:
          return 0;

        case TD_AU: // C(A)0,17
          return GETHI (rA);

        case TD_QU: //  C(Q)0,17
          return GETHI (rQ);

        case TD_IC: // C(PPR.IC)
          return PPR . IC;

        case TD_AL: // C(A)18,35
          return rA; // See AL36, Table 4-1

        case TD_QL: // C(Q)18,35
          return rQ; // See AL36, Table 4-1
      }
    return 0;
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
          return GETHI (rA);

        case 2: // qu
          return GETHI (rQ);

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
          return PPR . IC;

        case 5: // al / a
          return GETLO (rA);

        case 6: // ql / a
          return GETLO (rQ);

        case 7: // dl
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
          return GETHI (rA);

        case 2: // qu
          return GETHI (rQ);

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
          return PPR . IC;

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

static void EISWriteCache (EISaddr * p)
  {
    word3 saveTRR = TPR . TRR;

    if (p -> cacheValid && p -> cacheDirty)
      {
        if (p -> mat == viaPR)
          {
            TPR . TRR = p -> RNR;
            TPR . TSR = p -> SNR;
        
            sim_debug (DBG_TRACEEXT, & cpu_dev, 
                       "%s: writeCache (PR) %012llo@%o:%06o\n", 
                       __func__, p -> cachedWord, p -> SNR, p -> cachedAddr);
            Write (p->cachedAddr, p -> cachedWord, EIS_OPERAND_STORE, true);
          }
        else
          {
            if (get_addr_mode() == APPEND_mode)
              {
                TPR . TRR = PPR . PRR;
                TPR . TSR = PPR . PSR;
              }
        
            sim_debug (DBG_TRACEEXT, & cpu_dev, 
                       "%s: writeCache %012llo@%o:%06o\n", 
                       __func__, p -> cachedWord, TPR . TSR, p -> cachedAddr);
            Write (p->cachedAddr, p -> cachedWord, EIS_OPERAND_STORE, false);
          }
      }
    p -> cacheDirty = false;
    TPR . TRR = saveTRR;
  }

static void EISWriteIdx (EISaddr *p, uint n, word36 data)
{
    word3 saveTRR = TPR . TRR;
    word18 addressN = p -> address + n;
    addressN &= AMASK;
    if (p -> cacheValid && p -> cacheDirty && p -> cachedAddr != addressN)
      {
        EISWriteCache (p);
      }
    p -> cacheValid = true;
    p -> cacheDirty = true;
    p -> cachedAddr = addressN;
    p -> cachedWord = data;
// XXX ticket #31
// This a little brute force; it we fault on the next read, the cached value
// is lost. There might be a way to logic it up so that when the next read
// word offset changes, then we write the cache before doing the read. For
// right now, be pessimistic. Sadly, since this is a bit loop, it is very.
    EISWriteCache (p);

    TPR . TRR = saveTRR;
}

static word36 EISRead (EISaddr * p)
  {
    word36 data;

    word3 saveTRR = TPR . TRR;

    if (p -> cacheValid && p -> cachedAddr == p -> address)
      {
        return p -> cachedWord;
      }
    if (p -> cacheValid && p -> cacheDirty)
      {
        EISWriteCache (p);
      }
    p -> cacheDirty = false;

    if (p -> mat == viaPR)
    {
        TPR . TRR = p -> RNR;
        TPR . TSR = p -> SNR;
        
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "%s: read %o:%06o\n", __func__, TPR . TSR, p -> address);
         // read data via AR/PR. TPR.{TRR,TSR} already set up
        Read (p -> address, & data, EIS_OPERAND_READ, true);
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "%s: read* %012llo@%o:%06o\n", __func__,
                   data, TPR . TSR, p -> address);
      }
    else
      {
        if (get_addr_mode() == APPEND_mode)
          {
            TPR . TRR = PPR . PRR;
            TPR . TSR = PPR . PSR;
          }
        
        Read (p -> address, & data, EIS_OPERAND_READ, false);  // read operand
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "%s: read %012llo@%o:%06o\n", 
                   __func__, data, TPR . TSR, p -> address);
    }
    p -> cacheValid = true;
    p -> cachedAddr = p -> address;
    p -> cachedWord = data;
    TPR . TRR = saveTRR;
    return data;
  }

static word36 EISReadIdx (EISaddr * p, uint n)
  {
    word18 saveAddr = p -> address;
    word18 addressN = p -> address + n;
    addressN &= AMASK;
    p -> address = addressN;
    word36 data = EISRead (p);
    p -> address = saveAddr;
    return data;
  }

static void EISReadN (EISaddr * p, uint N, word36 *dst)
  {
    word18 saveAddr = p -> address;
    for (uint n = 0; n < N; n ++)
      {
        * dst ++ = EISRead (p);
        p -> address ++;
        p -> address &= AMASK;
      }
    p -> address = saveAddr;
  }

static uint EISget469 (int k, uint i)
  {
    EISstruct * e = & currentInstruction . e;
    
    int nPos = 4; // CTA9
    switch (e -> TA [k - 1])
      {
        case CTA4:
            nPos = 8;
            break;
            
        case CTA6:
            nPos = 6;
            break;
      }
    
    word18 address = e -> WN [k - 1];
    uint nChars = i + e -> CN [k - 1];

    address += nChars / nPos;
    uint residue = nChars % nPos;

    e -> addr [k - 1] . address = address;
    word36 data = EISRead (& e -> addr [k - 1]);    // read it from memory

    uint c = 0;
    switch (e -> TA [k - 1])
      {
        case CTA4:
          c = get4 (data, residue);
          break;

        case CTA6:
          c = get6 (data, residue);
          break;

        case CTA9:
          c = get9 (data, residue);
          break;
      }
    sim_debug (DBG_TRACEEXT, & cpu_dev, "EISGet469 : k: %u TAk %u coffset %u c %o \n", k, e -> TA [k - 1], residue, c);
    
    return c;
  }

static void EISput469 (int k, uint i, word9 c469)
  {
    EISstruct * e = & currentInstruction . e;

    int nPos = 4; // CTA9
    switch (e -> TA [k - 1])
      { 
        case CTA4:
          nPos = 8;
          break;
            
        case CTA6:
          nPos = 6;
          break;
      }

    word18 address = e -> WN [k - 1];
    uint nChars = i + e -> CN [k - 1];

    address += nChars / nPos;
    uint residue = nChars % nPos;

    e -> addr [k - 1] . address = address;
    word36 data = EISRead (& e -> addr [k - 1]);    // read it from memory

    word36 w;
    switch (e -> TA [k - 1])
      {
        case CTA4:
          w = put4 (data, residue, c469);
          break;

        case CTA6:
          w = put6 (data, residue, c469);
          break;

        case CTA9:
          w = put9 (data, residue, c469);
          break;
      }
    EISWriteIdx (& e -> addr [k - 1], 0, w);
  }

/*
 * return a 4- or 9-bit character at memory "*address" and position "*pos". 
 * Increment pos (and address if necesary)
 */

static word9 EISget49 (EISaddr * p, int * pos, int tn)
  {
    int maxPos = tn == CTN4 ? 7 : 3;

    if (* pos > maxPos)        // overflows to next word?
      {   // yep....
        * pos = 0;        // reset to 1st byte
        // bump source to next address
        p -> address = (p -> address + 1) & AMASK;
        p -> data = EISRead (p);    // read it from memory
      }
    else
      {
        p -> data = EISRead (p);   // read data word from memory
      }

    word9 c = 0;
    switch (tn)
      {
        case CTN4:
          c = get4 (p -> data, * pos);
          break;
        case CTN9:
          c = get9 (p -> data, * pos);
          break;
      }

    (* pos) ++;
    return c;
  }

static bool EISgetBitRWN (EISaddr * p)
  {
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
        
        EISWriteIdx (p, 0, p -> data); // write data word to memory
      }

    p -> address = saveAddr;
    return p -> bit;
  }

static void setupOperandDescriptorCache(int k, EISstruct *e)
  {
    e -> addr [k - 1] .  cacheValid = false;
  }

static void setupOperandDescriptor (int k, EISstruct * e)
  {
    switch (k)
      {
        case 1:
          e -> MF1 = getbits36 (e -> op0, 29, 7);
          break;
        case 2:
          e -> MF2 = getbits36 (e -> op0, 11, 7);
          break;
        case 3:
          e -> MF3 = getbits36 (e -> op0,  2, 7);
          break;
      }
    
    word18 MFk = e -> MF [k - 1];
    
    if (MFk & MFkID)
    {
        word36 opDesc = e -> op [k - 1];
        
        // fill operand according to MFk....
        word18 address = GETHI (opDesc);
        e -> addr [k - 1] . address = address;
        
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
            uint n = getbits18 (address, 0, 3);
            word15 offset = address & MASK15;  // 15-bit signed number
            address = (AR [n] . WORDNO + SIGNEXT15_18 (offset)) & AMASK;

            e -> addr [k - 1] . address = address;
            if (get_addr_mode () == APPEND_mode)
              {
                e -> addr [k - 1] . SNR = PR [n] . SNR;
                e -> addr [k - 1] . RNR = max3 (PR [n] . RNR,
                                                TPR . TRR,
                                                PPR . PRR);
                
                e -> addr [k - 1] . mat = viaPR;   // ARs involved
              }
          }
        else
          //e->addr[k-1].mat = IndirectRead;      // no ARs involved yet
          e->addr [k - 1] . mat = OperandRead;      // no ARs involved yet

        // Address modifier for ADDRESS. All register modifiers except du and
        // dl may be used. If the ic modifier is used, then ADDRESS is an
        // 18-bit offset relative to value of the instruction counter for the
        // instruction word. C(REG) is always interpreted as a word offset. REG 

        uint reg = opDesc & 017;
        address += getMFReg18 (reg, false);
        address &= AMASK;

        e -> addr [k - 1] . address = address;
        
        // read EIS operand .. this should be an indirectread
        e -> op [k - 1] = EISRead (& e -> addr [k - 1]); 
    }
    setupOperandDescriptorCache (k, e);
}


static void parseAlphanumericOperandDescriptor (uint k, EISstruct * e,
                                                uint useTA)
  {
    word18 MFk = e -> MF [k - 1];
    
    word36 opDesc = e -> op [k - 1];
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;
    
    word18 address = GETHI (opDesc);
    
    if (useTA != k)
      e -> TA [k - 1] = e -> TA [useTA - 1];
    else
      e -> TA [k - 1] = getbits36 (opDesc, 21, 2);    // type alphanumeric

    if (MFk & MFkAR)
      {
        // if MKf contains ar then it Means Y-charn is not the memory address
        // of the data but is a reference to a pointer register pointing to the
        // data.
        uint n = getbits18 (address, 0, 3);
        word18 offset = SIGNEXT15_18 (address);  // 15-bit signed number
        address = (AR [n] . WORDNO + offset) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
          {
            e -> addr [k - 1] . SNR = PR [n] . SNR;
            e -> addr [k - 1] . RNR = max3 (PR [n] . RNR, TPR . TRR, PPR . PRR);

            e -> addr [k - 1] . mat = viaPR;   // ARs involved
          }
      }

    uint CN = getbits36 (opDesc, 18, 3);    // character number

    sim_debug (DBG_TRACEEXT, & cpu_dev, "initial CN%u %u\n", k, CN);
    
    if (MFk & MFkRL)
    {
        uint reg = opDesc & 017;
        e -> N [k - 1] = getMFReg36 (reg, false);
        switch (e -> TA [k - 1])
          {
            case CTA4:
              e -> N [k - 1] &= 017777777; // 22-bits of length
              break;

            case CTA6:
            case CTA9:
              e -> N [k - 1] &= 07777777;  // 21-bits of length.
              break;

            default:
              sim_printf ("parseAlphanumericOperandDescriptor(ta=%d) How'd we get here 1?\n", e->TA[k-1]);
              break;
          }
      }
    else
      e -> N [k - 1] = opDesc & 07777;
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, "N%u %u\n", k, e->N[k-1]);

    word36 r = getMFReg36 (MFk & 017, false);
    
    // AL-39 implies, and RJ-76 say that RL and reg == IC is illegal;
    // but it the emulator ignores RL if reg == IC, then that PL/I
    // generated code in Multics works. "Pragmatic debugging."

    if (/*!(MFk & MFkRL) && */ (MFk & 017) == 4)   // reg == IC ?
      {
        // The ic modifier is permitted in MFk.REG and C (od)32,35 only if
        // MFk.RL = 0, that is, if the contents of the register is an address
        // offset, not the designation of a register containing the operand
        // length.
        address += r;
        address &= AMASK;
        r = 0;
      }

    // If seems that the effect address calcs given in AL39 p.6-27 are not 
    // quite right. E.g. For CTA4/CTN4 because of the 4 "slop" bits you need 
    // to do 32-bit calcs not 36-bit!
    switch (e -> TA [k - 1])
      {
        case CTA4:
          e -> effBITNO = 4 * (ARn_CHAR + 2 * r + ARn_BITNO / 4) % 2 + 1;
          e -> effCHAR = ((4 * CN + 
                           9 * ARn_CHAR +
                           4 * r + ARn_BITNO) % 32) / 4;  //9;36) / 4;  //9;
          e -> effWORDNO = address +
                           (4 * CN +
                           9 * ARn_CHAR +
                           4 * r +
                           ARn_BITNO) / 32;    // 36
          e -> effWORDNO &= AMASK;
            
          //e->YChar4[k-1] = e->effWORDNO;
          //e->CN[k-1] = CN;    //e->effCHAR;
          e -> CN [k - 1] = e -> effCHAR;
          e -> WN [k - 1] = e -> effWORDNO;
          sim_debug (DBG_TRACEEXT, & cpu_dev, "CN%d set to %d by CTA4\n",
                     k, e -> CN [k - 1]);
          break;

        case CTA6:
          e -> effBITNO = (9 * ARn_CHAR + 6 * r + ARn_BITNO) % 9;
          e -> effCHAR = ((6 * CN +
                           9 * ARn_CHAR +
                           6 * r + ARn_BITNO) % 36) / 6;//9;
          e -> effWORDNO = address +
                           (6 * CN +
                            9 * ARn_CHAR +
                            6 * r +
                            ARn_BITNO) / 36;
          e -> effWORDNO &= AMASK;
            
          //e->YChar6[k-1] = e->effWORDNO;
          e -> CN [k - 1] = e -> effCHAR;   // ??????
          e -> WN [k - 1] = e -> effWORDNO;
          sim_debug (DBG_TRACEEXT, & cpu_dev, "CN%d set to %d by CTA6\n",
                     k, e -> CN [k - 1]);
          break;

        case CTA9:
          CN = (CN >> 1) & 07;  // XXX Do error checking
            
          e -> effBITNO = 0;
          e -> effCHAR = (CN + ARn_CHAR + r) % 4;
          sim_debug (DBG_TRACEEXT, & cpu_dev, 
                     "effCHAR %d = (CN %d + ARn_CHAR %d + r %lld) %% 4)\n",
                     e -> effCHAR, CN, ARn_CHAR, r);
          e -> effWORDNO = address +
                           ((9 * CN +
                             9 * ARn_CHAR +
                             9 * r +
                             ARn_BITNO) / 36);
          e -> effWORDNO &= AMASK;
            
          //e->YChar9[k-1] = e->effWORDNO;
          e -> CN [k - 1] = e -> effCHAR;   // ??????
          e -> WN [k - 1] = e -> effWORDNO;
          sim_debug (DBG_TRACEEXT, & cpu_dev, "CN%d set to %d by CTA9\n",
                     k, e -> CN [k - 1]);
          break;

        default:
          sim_printf ("parseAlphanumericOperandDescriptor(ta=%d) How'd we get here 2?\n", e->TA[k-1]);
            break;
    }
    
    EISaddr * a = & e -> addr [k - 1];
    a -> address = e -> effWORDNO;
    a -> cPos= e -> effCHAR;
    a -> bPos = e -> effBITNO;
    
    a->_type = eisTA;
    a -> TA = e -> TA [k - 1];
  }

static void parseArgOperandDescriptor (uint k, EISstruct * e)
  {
    word36 opDesc = e -> op [k - 1];
    word18 y = GETHI (opDesc);
    word1 yA = GET_A (opDesc);

    uint yREG = opDesc & 0xf;
    
    word36 r = getMFReg36 (yREG, false);
    
    word8 ARn_CHAR = 0;
    word6 ARn_BITNO = 0;

    if (yA)
      {
        // if operand contains A (bit-29 set) then it Means Y-char9n is not
        // the memory address of the data but is a reference to a pointer
        // register pointing to the data.
        word3 n = GET_ARN (opDesc);
        word15 offset = y & MASK15;  // 15-bit signed number
        y = (AR [n] . WORDNO + SIGNEXT15_18 (offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
        if (get_addr_mode() == APPEND_mode)
          {
            e -> addr [k - 1] . SNR = PR[n].SNR;
            e -> addr [k - 1] . RNR = max3 (PR [n] . RNR, TPR . TRR, PPR . PRR);
            e -> addr [k - 1] . mat = viaPR;
          }
      }
    
    y += ((9 * ARn_CHAR + 36 * r + ARn_BITNO) / 36);
    y &= AMASK;
    
    e -> addr [k - 1] . address = y;
  }

static void parseNumericOperandDescriptor(int k, EISstruct *e)
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

static void cleanupOperandDescriptor(int k, EISstruct *e)
  {
    if (e -> addr [k - 1] . cacheValid && e -> addr [k - 1] . cacheDirty)
      {
        EISWriteCache(& e -> addr [k - 1]);
      }
    e -> addr [k - 1] . cacheDirty = false;
  }

void a4bd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR (reg);
                
    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu . IWB))
      {
        // If A = 0, then
        //   ADDRESS + C(REG) / 4 -> C(ARn.WORDNO)
        //   C(REG)mod4 -> C(ARn.CHAR)
        //   4 * C(REG)mod2 + 1 -> C(ARn.BITNO)
        AR [ARn] . WORDNO = address + r / 4;
        // AR[ARn].CHAR = r % 4;
        // AR[ARn].ABITNO = 4 * r % 2 + 1;
        SET_AR_CHAR_BIT (ARn, r % 4, 4 * r % 2 + 1);
      }
    else
      {
        // If A = 1, then
        //   C(ARn.WORDNO) + ADDRESS + (9 * C(ARn.CHAR) + 4 * C(REG) +
        //      C(ARn.BITNO)) / 36 -> C(ARn.WORDNO)
        //   ((9 * C(ARn.CHAR) + 4 * C(REG) + C(ARn.BITNO))mod36) / 9 -> 
        //     C(ARn.CHAR)
        //   4 * (C(ARn.CHAR) + 2 * C(REG) + C(ARn.BITNO) / 4)mod2 + 1 -> 
        //     C(ARn.BITNO)
        AR [ARn] . WORDNO = AR [ARn] . WORDNO + 
                            address + 
                            (9 * GET_AR_CHAR (ARn) + 
                             4 * r + 
                             GET_AR_BITNO (ARn)) / 36;
        // AR[ARn].CHAR = ((9 * AR[ARn].CHAR + 4 * r + 
        //   GET_AR_BITNO (ARn)) % 36) / 9;
        // AR[ARn].ABITNO = 4 * (AR[ARn].CHAR + 2 * r + 
        //   GET_AR_BITNO (ARn) / 4) % 2 + 1;
        SET_AR_CHAR_BIT (ARn,
                         ((9 * GET_AR_CHAR (ARn) + 
                           4 * r + GET_AR_BITNO (ARn)) % 36) / 9,
                         4 * (GET_AR_CHAR (ARn) + 
                           2 * r + GET_AR_BITNO (ARn) / 4) % 2 + 1);
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;
    // AR[ARn].ABITNO &= 077;
  }


void a6bd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR (reg);
                
    // If A = 0, then
    //   ADDRESS + C(REG) / 6 -> C(ARn.WORDNO)
    //   ((6 * C(REG))mod36) / 9 -> C(ARn.CHAR)
    //   (6 * C(REG))mod9 -> C(ARn.BITNO)
    //If A = 1, then
    //   C(ARn.WORDNO) + ADDRESS + (9 * C(ARn.CHAR) + 6 * C(REG) + 
    //     C(ARn.BITNO)) / 36 -> C(ARn.WORDNO)
    //   ((9 * C(ARn.CHAR) + 6 * C(REG) + C(ARn.BITNO))mod36) / 9 -> C(ARn.CHAR)
    //   (9 * C(ARn.CHAR) + 6 * C(REG) + C(ARn.BITNO))mod9 -> C(ARn.BITNO)

    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu . IWB))
      {
        AR [ARn] . WORDNO = address + r / 6;
        // AR[ARn].CHAR = ((6 * r) % 36) / 9;
        // AR[ARn].ABITNO = (6 * r) % 9;
        SET_AR_CHAR_BIT (ARn, ((6 * r) % 36) / 9, (6 * r) % 9);
      }
    else
      {
        AR [ARn] . WORDNO = AR [ARn] . WORDNO +
                            address +
                            (9 * GET_AR_CHAR (ARn) +
                            6 * r + GET_AR_BITNO (ARn)) / 36;
        SET_AR_CHAR_BIT (ARn,
                         ((9 * GET_AR_CHAR (ARn) +
                           6 * r + GET_AR_BITNO (ARn)) % 36) / 9,
                         (9 * GET_AR_CHAR (ARn) +
                          6 * r +
                          GET_AR_BITNO (ARn)) % 9);
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;
    // AR[ARn].ABITNO &= 077;
  }


void a9bd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR (reg);
                
    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu . IWB))
      {
        // If A = 0, then
        //   ADDRESS + C(REG) / 4 -> C(ARn.WORDNO)
        //   C(REG)mod4 -> C(ARn.CHAR)
        AR [ARn] . WORDNO = (address + r / 4);
        // AR[ARn].CHAR = r % 4;
        SET_AR_CHAR_BIT (ARn, r % 4, 0);
      }
    else
      {
        // If A = 1, then
        //   C(ARn.WORDNO) + ADDRESS + (C(REG) + C(ARn.CHAR)) / 
        //     4 -> C(ARn.WORDNO)
        //   (C(ARn.CHAR) + C(REG))mod4 -> C(ARn.CHAR)
        // (r and AR_CHAR are 18 bit values, but we want not to 
        // lose most significant digits in the addition.
        AR [ARn] . WORDNO += 
          (address + (r + ((word36) GET_AR_CHAR (ARn))) / 4);
        // AR[ARn].CHAR = (AR[ARn].CHAR + r) % 4;
        SET_AR_CHAR_BIT (ARn, (r + ((word36) GET_AR_CHAR (ARn))) % 4, 0);
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;

    // 0000 -> C(ARn.BITNO)
    // Zero set in SET_AR_CHAR_BIT calls above
    // AR[ARn].ABITNO = 0;
  }


void abd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
// The bit string count in the register specified in the DR field is divided by
// 36. The quotient is taken as the word count and the remainder is taken as
// the bit count. The word count is added to the y field for which bit 3 of the
// instruction word is extended and the sum is taken.

    word36 bitStringCnt = getCrAR (reg);
    word36 wordCnt = bitStringCnt / 36u;
    word36 bitCnt = bitStringCnt % 36u;

    word18 address = 
      SIGNEXT15_18 ((word15) getbits36 (cu . IWB, 3, 15)) & AMASK;
    address += wordCnt;

    if (! GET_A (cu . IWB))
      {

// If bit 29=0, the sum is loaded into bits 0-17 of the specified AR, and the
// character portion and the bit portion of the remainder are loaded into bits
// 18-23 of the specified AR.

        AR [ARn] . WORDNO = address;
#ifdef ABD_BITS
        AR [ARn] . BITNO = bitCnt;
#else
        SET_AR_CHAR_BIT (ARn, bitCnt / 9u, bitCnt % 9u);
#endif
      }
    else
      {
// If bit 29=1, the sum is added to bits 0-17 of the specified AR.
        AR [ARn] . WORDNO += address;
//  The CHAR and BIT fields (bits 18-23) of the specified AR are added to the 
//  character portion and the bit portion of the remainder. 

#ifdef ABD_BITS
        word36 bits = AR [ARn] . BITNO + bitCnt;
        AR [ARn] . WORDNO += bits / 36;
        AR [ARn] . BITNO = bits % 36;
#else
        word36 charPortion = bitCnt / 9;
        word36 bitPortion = bitCnt % 9;

        charPortion += GET_AR_CHAR (ARn);
        bitPortion += GET_AR_BITNO (ARn);

// WORD, CHAR and BIT fields generated in this manner are loaded into bits 0-23
// of the specified AR. With this addition, carry from the BIT field (bit 20)
// and the CHAR field (bit 18) is transferred (when BIT field >8, CHAR field
// >3).

// XXX replace with modulus arithmetic

        while (bitPortion > 8)
          {
            bitPortion -= 9;
            charPortion += 1;
          }

        while (charPortion > 3)
          {
            charPortion -= 3;
            AR [ARn] . WORDNO += 1;
          }


        SET_AR_CHAR_BIT (ARn, charPortion, bitPortion);
#endif
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;
    // AR[ARn].ABITNO &= 077;
  }


void awd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)

    // If A = 0, then
    //   ADDRESS + C(REG) -> C(ARn.WORDNO)
    // If A = 1, then
    //   C(ARn.WORDNO) + ADDRESS + C(REG) -> C(ARn.WORDNO)
    // 00 -> C(ARn.CHAR)
    // 0000 -> C(ARn.BITNO)
                
    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu. IWB))
      AR [ARn] . WORDNO = (address + getCrAR (reg));
    else
      AR [ARn] . WORDNO += (address + getCrAR (reg));
                
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // AR[ARn].CHAR = 0;
    // AR[ARn].ABITNO = 0;
    SET_AR_CHAR_BIT (ARn, 0, 0);
  }


void s4bd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR (reg);

    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu. IWB))
      {
        // If A = 0, then
        //   - (ADDRESS + C(REG) / 4) -> C(ARn.WORDNO)
        //   - C(REG)mod4 -> C(ARn.CHAR)
        //   - 4 * C(REG)mod2 + 1 -> C(ARn.BITNO)
        AR [ARn] . WORDNO = - (address + r / 4);
        // AR[ARn].CHAR = -(r % 4);
        // AR[ARn].ABITNO = -4 * r % 2 + 1;
        SET_AR_CHAR_BIT (ARn, - (r % 4), -4 * r % 2 + 1);
      }
    else
      {
        // If A = 1, then
        //   C(ARn.WORDNO) - ADDRESS + (9 * C(ARn.CHAR) - 4 * C(REG) + 
        //     C(ARn.BITNO)) / 36 -> C(ARn.WORDNO)
        //   ((9 * C(ARn.CHAR) - 4 * C(REG) + C(ARn.BITNO))mod36) / 9 -> 
        //     C(ARn.CHAR)
        //   4 * (C(ARn.CHAR) - 2 * C(REG) + C(ARn.BITNO) / 4)mod2 + 1 -> 
        //     C(ARn.BITNO)

        AR [ARn] . WORDNO = AR [ARn] . WORDNO - 
                            address +
                            (9 * GET_AR_CHAR (ARn) -
                             4 * r + GET_AR_BITNO (ARn)) / 36;
        // AR[ARn].CHAR = ((9 * AR[ARn].CHAR - 4 * r + 
        //   AR[ARn].ABITNO) % 36) / 9;
        // AR[ARn].ABITNO = 4 * (AR[ARn].CHAR - 2 * r + 
        //   AR[ARn].ABITNO / 4) % 2 + 1;
        SET_AR_CHAR_BIT (ARn, 
                         ((9 * GET_AR_CHAR (ARn) - 
                           4 * r + 
                           GET_AR_BITNO (ARn)) % 36) / 9,
                         4 * (GET_AR_CHAR (ARn) -
                         2 * r + 
                         GET_AR_BITNO (ARn) / 4) % 2 + 1);
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;
    // AR[ARn].ABITNO &= 077;
  }


void s6bd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR (reg);
                
    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu. IWB))
      {
        // If A = 0, then
        //   - (ADDRESS + C(REG) / 6) -> C(ARn.WORDNO)
        //   - ((6 * C(REG))mod36) / 9 -> C(ARn.CHAR)
        //   - (6 * C(REG))mod9 -> C(ARn.BITNO)
        AR [ARn] . WORDNO = -(address + r / 6);
        // AR[ARn].CHAR = -((6 * r) % 36) / 9;
        // AR[ARn].ABITNO = -(6 * r) % 9;
        SET_AR_CHAR_BIT (ARn, -((6 * r) % 36) / 9, -(6 * r) % 9);
      }
    else
      {
        // If A = 1, then
        //   C(ARn.WORDNO) - ADDRESS + (9 * C(ARn.CHAR) - 6 * C(REG) + C(ARn.BITNO)) / 36 -> C(ARn.WORDNO)
        //   ((9 * C(ARn.CHAR) - 6 * C(REG) + C(ARn.BITNO))mod36) / 9 -> C(ARn.CHAR)
        //   (9 * C(ARn.CHAR) - 6 * C(REG) + C(ARn.BITNO))mod9 -> C(ARn.BITNO)
        AR [ARn] . WORDNO = AR [ARn] . WORDNO - 
                            address +
                            (9 * GET_AR_CHAR (ARn) -
                             6 * r + GET_AR_BITNO (ARn)) / 36;
        //AR[ARn].CHAR = ((9 * AR[ARn].CHAR - 6 * r + AR[ARn].ABITNO) % 36) / 9;
        //AR[ARn].ABITNO = (9 * AR[ARn].CHAR - 6 * r + AR[ARn].ABITNO) % 9;
        SET_AR_CHAR_BIT (ARn, 
                         ((9 * GET_AR_CHAR (ARn) -
                           6 * r + GET_AR_BITNO (ARn)) % 36) / 9,
                         (9 * GET_AR_CHAR (ARn) - 
                          6 * r + GET_AR_BITNO (ARn)) % 9);
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;
    // AR[ARn].ABITNO &= 077;
  }


void s9bd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR (reg);

    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu. IWB))
      {
        // If A = 0, then
        //   - (ADDRESS + C(REG) / 4) -> C(ARn.WORDNO)
        //   - C(REG)mod4 -> C(ARn.CHAR)
        AR [ARn] . WORDNO = -(address + r / 4);
        // AR[ARn].CHAR = - (r % 4);
        SET_AR_CHAR_BIT (ARn, (r % 4), 0);
      }
    else
      {
        // If A = 1, then
        //   C(ARn.WORDNO) - ADDRESS + (C(ARn.CHAR) - C(REG)) / 4 -> 
        //     C(ARn.WORDNO)
        //   (C(ARn.CHAR) - C(REG))mod4 -> C(ARn.CHAR)
        AR [ARn] . WORDNO = AR [ARn] . WORDNO -
                            address +
                            (GET_AR_CHAR (ARn) - r) / 4;
        // AR[ARn].CHAR = (AR[ARn].CHAR - r) % 4;
        SET_AR_CHAR_BIT (ARn, (GET_AR_CHAR (ARn) - r) % 4, 0);
      }
                
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;
    // 0000 -> C(ARn.BITNO)
    // Zero set above in macro call
    // AR[ARn].ABITNO = 0;
  }


void sbd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR (reg);

    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu. IWB))
      {
        // If A = 0, then
        //   - (ADDRESS + C(REG) / 36) -> C(ARn.WORDNO)
        //   - (C(REG)mod36) / 9 -> C(ARn.CHAR)
        //   - C(REG)mod9 -> C(ARn.BITNO)
        AR [ARn] . WORDNO = -(address + r / 36);
        // AR[ARn].CHAR = -(r %36) / 9;
        // AR[ARn].ABITNO = -(r % 9);
        SET_AR_CHAR_BIT (ARn, - (r %36) / 9, - (r % 9));
      }
    else
      {
        // If A = 1, then
        //   C(ARn.WORDNO) - ADDRESS + (9 * C(ARn.CHAR) - 36 * C(REG) + 
        //    C(ARn.BITNO)) / 36 -> C(ARn.WORDNO)
        //  ((9 * C(ARn.CHAR) - 36 * C(REG) + C(ARn.BITNO))mod36) / 9 -> 
        //    C(ARn.CHAR)
        //  (9 * C(ARn.CHAR) - 36 * C(REG) + C(ARn.BITNO))mod9 -> C(ARn.BITNO)
        AR [ARn] . WORDNO = AR [ARn] . WORDNO - 
                            address +
                           (9 * GET_AR_CHAR (ARn) -
                            36 * r + GET_AR_BITNO (ARn)) / 36;
        // AR[ARn].CHAR = ((9 * AR[ARn].CHAR - 36 * r + 
        //   AR[ARn].ABITNO) % 36) / 9;
        // AR[ARn].ABITNO = (9 * AR[ARn].CHAR - 36 * r + AR[ARn].ABITNO) % 9;
        SET_AR_CHAR_BIT (ARn,
                         ((9 * GET_AR_CHAR (ARn) -
                           36 * r +
                           GET_AR_BITNO (ARn)) % 36) / 9,
                         (9 * GET_AR_CHAR (ARn) -
                          36 * r + GET_AR_BITNO (ARn)) % 9);
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // Masking done in SET_AR_CHAR_BIT
    // AR[ARn].CHAR &= 03;
    // AR[ARn].ABITNO &= 077;
  }


void swd (void)
  {
    uint ARn = GET_ARN (cu . IWB);
    word18 address = SIGNEXT15_18 (GET_OFFSET (cu . IWB));
    uint reg = GET_TD (cu . IWB); // 4-bit register modification (None except 
                                  // au, qu, al, ql, xn)
    word36 r = getCrAR(reg);
                
    // The i->a bit is zero since IGN_B29 is set
    if (! GET_A (cu. IWB))
      {
        // If A = 0, then
        //   - (ADDRESS + C(REG)) -> C(ARn.WORDNO)
        AR [ARn] . WORDNO = - (address + r);
      }
    else
      {
        // If A = 1, then
        //     C(ARn.WORDNO) - (ADDRESS + C(REG)) -> C(ARn.WORDNO)
        AR [ARn] . WORDNO = AR [ARn] . WORDNO - (address + r);
      }
    AR [ARn] . WORDNO &= AMASK;    // keep to 18-bits
    // 00 -> C(ARn.CHAR)
    // 0000 -> C(ARn.BITNO)
    // AR[ARn].CHAR = 0;
    // AR[ARn].ABITNO = 0;
    SET_AR_CHAR_BIT (ARn, 0, 0);
  }


void cmpc (void)
  {
    EISstruct * e = & currentInstruction . e;

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
    
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    parseAlphanumericOperandDescriptor (1, e, 1);
    parseAlphanumericOperandDescriptor (2, e, 1);
    
    int fill = (int) getbits36 (cu . IWB, 0, 9);
    
    SETF (cu . IR, I_ZERO);  // set ZERO flag assuming strings are equal ...
    SETF (cu . IR, I_CARRY); // set CARRY flag assuming strings are equal ...
    
    for (; du . CHTALLY < min (e->N1, e->N2); du . CHTALLY ++)
      {
        uint c1 = EISget469 (1, du . CHTALLY); // get Y-char1n
        uint c2 = EISget469 (2, du . CHTALLY); // get Y-char2n

        if (c1 != c2)
          {
            CLRF (cu . IR, I_ZERO);  // an inequality found
            SCF (c1 > c2, cu . IR, I_CARRY);
            cleanupOperandDescriptor (1, e);
            cleanupOperandDescriptor (2, e);
            return;
          }
      }

    if (e -> N1 < e -> N2)
      {
        for( ; du . CHTALLY < e->N2; du . CHTALLY ++)
          {
            uint c1 = fill;     // use fill for Y-char1n
            uint c2 = EISget469 (2, du . CHTALLY); // get Y-char2n
            
            if (c1 != c2)
              {
                CLRF (cu . IR, I_ZERO);  // an inequality found
                SCF (c1 > c2, cu . IR, I_CARRY);
                cleanupOperandDescriptor (1, e);
                cleanupOperandDescriptor (2, e);
                return;
              }
          }
      }
    else if (e->N1 > e->N2)
      {
        for ( ; du . CHTALLY < e->N1; du . CHTALLY ++)
          {
            uint c1 = EISget469 (1, du . CHTALLY); // get Y-char1n
            uint c2 = fill;   // use fill for Y-char2n
            
            if (c1 != c2)
              {
                CLRF (cu.IR, I_ZERO);  // an inequality found
                SCF (c1 > c2, cu.IR, I_CARRY);
                cleanupOperandDescriptor (1, e);
                cleanupOperandDescriptor (2, e);
                return;
              }
          }
      }
    // else ==
    cleanupOperandDescriptor (1, e);
    cleanupOperandDescriptor (2, e);
  }


/*
 * SCD - Scan Characters Double
 */

void scd ()
  {
    EISstruct * e = & currentInstruction . e;

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
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    setupOperandDescriptorCache (3, e);
    
    parseAlphanumericOperandDescriptor (1, e, 1);
    parseAlphanumericOperandDescriptor (2, e, 1); // use TA1
    parseArgOperandDescriptor (3, e);
    
    // Both the string and the test character pair are treated as the data type
    // given for the string, TA1. A data type given for the test character
    // pair, TA2, is ignored.
    
    // fetch 'test' char - double
    // If MF2.ID = 0 and MF2.REG = du, then the second word following the
    // instruction word does not contain an operand descriptor for the test
    // character; instead, it contains the test character as a direct upper
    // operand in bits 0,8.
    
    uint c1 = 0;
    uint c2 = 0;
    
    if (! (e -> MF2 & MFkID) && ((e -> MF2 & MFkREGMASK) == 3))  // MF2.du
      {
        // per Bull RJ78, p. 5-45
        switch (e -> TA1) // Use TA1, not TA2
        {
            case CTA4:
              c1 = (e -> ADDR2 . address >> 13) & 017;
              c2 = (e -> ADDR2 . address >>  9) & 017;
              break;

            case CTA6:
              c1 = (e -> ADDR2 . address >> 12) & 077;
              c2 = (e -> ADDR2 . address >>  6) & 077;
              break;

            case CTA9:
              c1 = (e -> ADDR2 . address >> 9) & 0777;
              c2 = (e -> ADDR2 . address     ) & 0777;
              break;
          }
      }
    else
      {  
        c1 = EISget469 (2, 0);
        c2 = EISget469 (2, 1);
      }

    switch (e -> TA1) // Use TA1, not TA2
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
    

    uint yCharn11;
    uint yCharn12;
    uint limit = e -> N1 - 1;

    for ( ; du . CHTALLY < limit; du . CHTALLY ++)
      {
        yCharn11 = EISget469 (1, du . CHTALLY);
        yCharn12 = EISget469 (1, du . CHTALLY + 1);
        // sim_printf ("yCharn11 %o c1 %o yCharn12 %o c2 %o\n",
                      //  yCharn11, c1, yCharn12, c2);
        if (yCharn11 == c1 && yCharn12 == c2)
          break;
      }
    
    word36 CY3 = bitfieldInsert36 (0, du . CHTALLY, 0, 24);
    
    SCF (du . CHTALLY == limit, cu . IR, I_TALLY);
    
    EISWriteIdx (& e -> ADDR3, 0, CY3);
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
 }

/*
 * SCDR - Scan Characters Double Reverse
 */

void scdr (void)
  {
    EISstruct * e = & currentInstruction . e;

    // For i = 1, 2, ..., N1-1
    //   C(Y-charn1)N1-i-1,N1-i :: C(Y-charn2)0,1
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
    setupOperandDescriptorCache(3, e);

    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 1); // Use TA1
    parseArgOperandDescriptor (3, e);
    
    // Both the string and the test character pair are treated as the data type
    // given for the string, TA1. A data type given for the test character
    // pair, TA2, is ignored.

    // fetch 'test' char - double
    // If MF2.ID = 0 and MF2.REG = du, then the second word following the
    // instruction word does not contain an operand descriptor for the test
    // character; instead, it contains the test character as a direct upper
    // operand in bits 0,8.
    
    uint c1 = 0;
    uint c2 = 0;
    
    if (! (e -> MF2 & MFkID) && ((e -> MF2 & MFkREGMASK) == 3))  // MF2.du
      {
        // per Bull RJ78, p. 5-45
        switch (e -> TA1)
          {
            case CTA4:
              c1 = (e -> ADDR2 . address >> 13) & 017;
              c2 = (e -> ADDR2 . address >>  9) & 017;
              break;

            case CTA6:
              c1 = (e -> ADDR2 . address >> 12) & 077;
              c2 = (e -> ADDR2 . address >>  6) & 077;
              break;

            case CTA9:
              c1 = (e -> ADDR2 . address >> 9) & 0777;
              c2 = (e -> ADDR2 . address     ) & 0777;
              break;
          }
      }
    else
      {
        c1 = EISget469 (2, 0);
        c2 = EISget469 (2, 1);
      }

    switch (e -> TA1) // Use TA1, not TA2
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
    
    uint yCharn11;
    uint yCharn12;
    uint limit = e -> N1 - 1;
    
    for ( ; du . CHTALLY < limit; du . CHTALLY ++)
      {
        yCharn11 = EISget469 (1, limit - du . CHTALLY - 1);
        yCharn12 = EISget469 (1, limit - du . CHTALLY);
        
        if (yCharn11 == c1 && yCharn12 == c2)
            break;
      }
    
    word36 CY3 = bitfieldInsert36(0, du . CHTALLY, 0, 24);
    
    SCF(du . CHTALLY == limit, cu . IR, I_TALLY);
    
    // write Y3 .....
    //Write (y3, CY3, OperandWrite, 0);
    EISWriteIdx (& e -> ADDR3, 0, CY3);
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
  }


/*
 * SCM - Scan with Mask
 */

void scm (void)
  {
    EISstruct * e = & currentInstruction . e;

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
    
    // Starting at location YC1, the L1 type TA1 characters are masked and
    // compared with the assumed type TA1 character contained either in
    // location YC2 or in bits 0-8 or 0-5 of the address field of operand
    // descriptor 2 (when the REG field of MF2 specifies DU modification). The
    // mask is right-justified in bit positions 0-8 of the instruction word.
    // Each bit position of the mask that is a 1 prevents that bit position in
    // the two characters from entering into the compare.

    // The masked compare operation continues until either a match is found or
    // the tally (L1) is exhausted. For each unsuccessful match, a count is
    // incremented by 1. When a match is found or when the L1 tally runs out,
    // this count is stored right-justified in bits 12-35 of location Y3 and
    // bits 0-11 of Y3 are zeroed. The contents of location YC2 and the source
    // string remain unchanged. The RL bit of the MF2 field is not used.
    
    // The REG field of MF1 is checked for a legal code. If DU is specified in
    // the REG field of MF2 in one of the four multiword instructions (SCD,
    // SCDR, SCM, or SCMR) for which DU is legal, the CN field is ignored and
    // the character or characters are arranged within the 18 bits of the word
    // address portion of the operand descriptor.
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    setupOperandDescriptorCache (3, e);

    parseAlphanumericOperandDescriptor (1, e, 1);
    parseAlphanumericOperandDescriptor (2, e, 1);
    parseArgOperandDescriptor (3, e);
    
    // Both the string and the test character pair are treated as the data type
    // given for the string, TA1. A data type given for the test character
    // pair, TA2, is ignored.

    // get 'mask'
    uint mask = (uint) bitfieldExtract36 (e -> op0, 27, 9);
    
    // fetch 'test' char
    // If MF2.ID = 0 and MF2.REG = du, then the second word following the
    // instruction word does not contain an operand descriptor for the test
    // character; instead, it contains the test character as a direct upper
    // operand in bits 0,8.
    
    uint ctest = 0;
    if (! (e -> MF2 & MFkID) && ((e -> MF2 & MFkREGMASK) == 3))  // MF2.du
      {
        word18 duo = GETHI (e -> OP2);
        // per Bull RJ78, p. 5-45
        switch (e -> TA1)
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
        ctest = EISget469 (2, 0);
      }

    switch (e -> TA1) // use TA1, not TA2
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

    uint limit = e -> N1;

    for ( ; du . CHTALLY < limit; du . CHTALLY ++)
      {
        uint yCharn1 = EISget469 (1, du . CHTALLY);
        uint c = ((~mask) & (yCharn1 ^ ctest)) & 0777;
        if (c == 0)
          {
            //00...0 → C(Y3)0,11
            //i-1 → C(Y3)12,35
            //Y3 = bitfieldInsert36(Y3, du . CHTALLY, 0, 24);
            break;
          }
      }
    word36 CY3 = bitfieldInsert36 (0, du . CHTALLY, 0, 24);
    
    SCF (du . CHTALLY == limit, cu . IR, I_TALLY);
    
    EISWriteIdx (& e -> ADDR3, 0, CY3);

    cleanupOperandDescriptor (1, e);
    cleanupOperandDescriptor (2, e);
    cleanupOperandDescriptor (3, e);
  }
/*
 * SCMR - Scan with Mask in Reverse
 */

void scmr (void)
  {
    EISstruct * e = & currentInstruction . e;

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
    
    // Starting at location YC1, the L1 type TA1 characters are masked and
    // compared with the assumed type TA1 character contained either in
    // location YC2 or in bits 0-8 or 0-5 of the address field of operand
    // descriptor 2 (when the REG field of MF2 specifies DU modification). The
    // mask is right-justified in bit positions 0-8 of the instruction word.
    // Each bit position of the mask that is a 1 prevents that bit position in
    // the two characters from entering into the compare.
 
    // The masked compare operation continues until either a match is found or
    // the tally (L1) is exhausted. For each unsuccessful match, a count is
    // incremented by 1. When a match is found or when the L1 tally runs out,
    // this count is stored right-justified in bits 12-35 of location Y3 and
    // bits 0-11 of Y3 are zeroed. The contents of location YC2 and the source
    // string remain unchanged. The RL bit of the MF2 field is not used.
    
    // The REG field of MF1 is checked for a legal code. If DU is specified in
    // the REG field of MF2 in one of the four multiword instructions (SCD,
    // SCDR, SCM, or SCMR) for which DU is legal, the CN field is ignored and
    // the character or characters are arranged within the 18 bits of the word
    // address portion of the operand descriptor.
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    setupOperandDescriptorCache (3, e);

    parseAlphanumericOperandDescriptor (1, e, 1);
    parseAlphanumericOperandDescriptor (2, e, 1);
    parseArgOperandDescriptor (3, e);
    
    // Both the string and the test character pair are treated as the data type
    // given for the string, TA1. A data type given for the test character
    // pair, TA2, is ignored.

    // get 'mask'
    uint mask = (uint) bitfieldExtract36 (e -> op0, 27, 9);
    
    // fetch 'test' char
    // If MF2.ID = 0 and MF2.REG = du, then the second word following the
    // instruction word does not contain an operand descriptor for the test
    // character; instead, it contains the test character as a direct upper
    // operand in bits 0,8.
    
    uint ctest = 0;
    if (! (e -> MF2 & MFkID) && ((e -> MF2 & MFkREGMASK) == 3))  // MF2.du
      {
        word18 duo = GETHI (e -> OP2);
        // per Bull RJ78, p. 5-45
        switch (e -> TA1)
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
        ctest = EISget469 (2, 0);
      }

    switch (e -> TA1) // use TA1, not TA2
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

    uint limit = e -> N1;
    for ( ; du . CHTALLY < limit; du . CHTALLY ++)
      {
        uint yCharn1 = EISget469 (1, limit - du . CHTALLY - 1);
        uint c = ((~mask) & (yCharn1 ^ ctest)) & 0777;
        if (c == 0)
          {
            //00...0 → C(Y3)0,11
            //i-1 → C(Y3)12,35
            //Y3 = bitfieldInsert36(Y3, du . CHTALLY, 0, 24);
            break;
          }
      }
    word36 CY3 = bitfieldInsert36 (0, du . CHTALLY, 0, 24);
    
    SCF (du . CHTALLY == limit, cu . IR, I_TALLY);
    
    EISWriteIdx (& e -> ADDR3, 0, CY3);

    cleanupOperandDescriptor (1, e);
    cleanupOperandDescriptor (2, e);
    cleanupOperandDescriptor (3, e);
  }

/*
 * TCT - Test Character and Translate
 */

static word9 xlate (word36 * xlatTbl, uint dstTA, uint c)
  {
    uint idx = (c / 4) & 0177;      // max 128-words (7-bit index)
    word36 entry = xlatTbl [idx];

    uint pos9 = c % 4;      // lower 2-bits
    uint cout = GETBYTE (entry, pos9);
    switch (dstTA)
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


void tct (void)
  {
    EISstruct * e = & currentInstruction . e;

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
    // Indicators: Tally Run Out. If the string length count exhausts, then ON;
    // otherwise, OFF
    //
    // If the data type of the string to be scanned is not 9-bit (TA1 ≠ 0),
    // then characters from C(Y-charn1) are high-order zero filled in forming
    // the table index, m.
    // Instruction execution proceeds until a non-zero table entry is found or
    // the string length count is exhausted.
    // The character number of Y-char92 must be zero, i.e., Y-char92 must start
    // on a word boundary.
    
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptorCache (2, e);
    setupOperandDescriptorCache (3, e);
    
    parseAlphanumericOperandDescriptor (1, e, 1);
    parseArgOperandDescriptor (2, e);
    parseArgOperandDescriptor (3, e);
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT CN1: %d TA1: %d\n", e -> CN1, e -> TA1);

    uint srcSZ;

    switch (e -> TA1)
      {
        case CTA4:
            srcSZ = 4;
            break;
        case CTA6:
            srcSZ = 6;
            break;
        case CTA9:
            srcSZ = 9;
            break;
      }
    
    
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
    
    word36 CY3 = 0;
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT N1 %d\n", e -> N1);

    for ( ; du . CHTALLY < e -> N1; du . CHTALLY ++)
      {
        uint c = EISget469 (1, du . CHTALLY); // get src char

        uint m = 0;
        
        switch (srcSZ)
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
        
        word9 cout = xlate (xlatTbl, CTA9, m);

        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "TCT c %03o %c cout %03o %c\n",
                   m, isprint (m) ? '?' : m, 
                   cout, isprint (cout) ? '?' : cout);

        if (cout)
          {
            CY3 = bitfieldInsert36 (0, cout, 27, 9); // C(Y-char92)m -> C(Y3)0,8
            break;
          }
      }
    
    SCF (du . CHTALLY == e -> N1, cu . IR, I_TALLY);
    
    CY3 = bitfieldInsert36 (CY3, du . CHTALLY, 0, 24);
    EISWriteIdx (& e -> ADDR3, 0, CY3);
    
    cleanupOperandDescriptor (1, e);
    cleanupOperandDescriptor (2, e);
    cleanupOperandDescriptor (3, e);
  }

void tctr (void)
  {
    EISstruct * e = & currentInstruction . e;

    // For i = 1, 2, ..., N1
    //   m = C(Y-charn1)N1-i
    //   If C(Y-char92)m ≠ 00...0, then
    //     C(Y-char92)m → C(Y3)0,8
    //     000 → C(Y3)9,11
    //     i-1 → C(Y3)12,35
    //   otherwise, continue scan of C(Y-charn1) If a non-zero table entry was
    //   not found, then
    // 00...0 → C(Y3)0,11
    // N1 → C(Y3)12,35
    //
    // Indicators: Tally Run Out. If the string length count exhausts, then ON;
    // otherwise, OFF
    //
    // If the data type of the string to be scanned is not 9-bit (TA1 ≠ 0),
    // then characters from C(Y-charn1) are high-order zero filled in forming
    // the table index, m.

    // Instruction execution proceeds until a non-zero table entry is found or
    // the string length count is exhausted.

    // The character number of Y-char92 must be zero, i.e., Y-char92 must start
    // on a word boundary.
 
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptorCache (2, e);
    setupOperandDescriptorCache (3, e);
    
    parseAlphanumericOperandDescriptor (1, e, 1);
    parseArgOperandDescriptor (2, e);
    parseArgOperandDescriptor (3, e);
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT CN1: %d TA1: %d\n", e -> CN1, e -> TA1);

    uint srcSZ;

    switch (e -> TA1)
      {
        case CTA4:
            srcSZ = 4;
            break;
        case CTA6:
            srcSZ = 6;
            break;
        case CTA9:
            srcSZ = 9;
            break;
      }
    
    
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
    
    word36 CY3 = 0;
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "TCT N1 %d\n", e -> N1);

    uint limit = e -> N1;
    for ( ; du . CHTALLY < limit; du . CHTALLY ++)
      {
        uint c = EISget469 (1, limit - du . CHTALLY - 1); // get src char

        uint m = 0;
        
        switch (srcSZ)
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
        
        word9 cout = xlate (xlatTbl, CTA9, m);

        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "TCT c %03o %c cout %03o %c\n",
                   m, isprint (m) ? '?' : m, 
                   cout, isprint (cout) ? '?' : cout);

        if (cout)
          {
            CY3 = bitfieldInsert36 (0, cout, 27, 9); // C(Y-char92)m -> C(Y3)0,8
            break;
          }
      }
    
    SCF (du . CHTALLY == e -> N1, cu . IR, I_TALLY);
    
    CY3 = bitfieldInsert36 (CY3, du . CHTALLY, 0, 24);
    EISWriteIdx (& e -> ADDR3, 0, CY3);
    
    cleanupOperandDescriptor (1, e);
    cleanupOperandDescriptor (2, e);
    cleanupOperandDescriptor (3, e);
  }

/*
 * MLR - Move Alphanumeric Left to Right
 *
 * (Nice, simple instruction if it weren't for the stupid overpunch stuff that ruined it!!!!)
 */

/*
 * does 6-bit char represent a GEBCD negative overpuch? if so, whice numeral?
 * Refer to Bull NovaScale 9000 RJ78 Rev2 p11-178
 */

static bool isOvp (uint c, uint * on)
  {
    // look for GEBCD -' 'A B C D E F G H I (positive overpunch)
    // look for GEBCD - ^ J K L M N O P Q R (negative overpunch)
    
    uint c2 = c & 077;   // keep to 6-bits
    * on = 0;
    
    if (c2 >= 020 && c2 <= 031)   // positive overpunch
      {
        * on = c2 - 020;          // return GEBCD number 0-9 (020 is +0)
        return false;             // well, it's not a negative overpunch is it?
      }
    if (c2 >= 040 && c2 <= 052)   // negative overpunch
      {
        * on = c2 - 040;  // return GEBCD number 0-9 
                         // (052 is just a '-' interpreted as -0)
        return true;
      }
    return false;
}

void mlr (void)
  {
    EISstruct * e = & currentInstruction . e;

    // For i = 1, 2, ..., minimum (N1,N2)
    //     C(Y-charn1)N1-i → C(Y-charn2)N2-i
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // Indicators: Truncation. If N1 > N2 then ON; otherwise OFF
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    setupOperandDescriptorCache (3, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 2);
    
    int srcSZ, dstSZ;

    switch (e -> TA1)
      {
        case CTA4:
          srcSZ = 4;
          break;
        case CTA6:
          srcSZ = 6;
          break;
        case CTA9:
          srcSZ = 9;
          break;
      }
    
    switch (e -> TA2)
      {
        case CTA4:
          dstSZ = 4;
          break;
        case CTA6:
          dstSZ = 6;
          break;
        case CTA9:
          dstSZ = 9;
          break;
      }
    
    uint T = bitfieldExtract36 (e -> op0, 26, 1) != 0;  // truncation bit
    
    uint fill = bitfieldExtract36 (e -> op0, 27, 9);
    uint fillT = fill;  // possibly truncated fill pattern

    // play with fill if we need to use it
    switch (dstSZ)
      {
        case 4:
          fillT = fill & 017;    // truncate upper 5-bits
          break;
        case 6:
          fillT = fill & 077;    // truncate upper 3-bits
          break;
      }
    
    // If N1 > N2, then (N1-N2) leading characters of C(Y-charn1) are not moved
    // and the truncation indicator is set ON.

    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL
    // characters are high-order truncated as they are moved to C(Y-charn2). No
    // character conversion takes place.

    // The user of string replication or overlaying is warned that the decimal
    // unit addresses the main memory in unaligned (not on modulo 8 boundary)
    // units of Y-block8 words and that the overlayed string, C(Y-charn2), is
    // not returned to main memory until the unit of Y-block8 words is filled or
    // the instruction completes.

    // If T = 1 and the truncation indicator is set ON by execution of the
    // instruction, then a truncation (overflow) fault occurs.

    // Attempted execution with the xed instruction causes an illegal procedure
    // fault.

    // Attempted repetition with the rpt, rpd, or rpl instructions causes an
    // illegal procedure fault.
    
    bool ovp = (e -> N1 < e -> N2) && (fill & 0400) && (e -> TA1 == 1) &&
               (e -> TA2 == 2); // (6-4 move)
    uint on;     // number overpunch represents (if any)
    bool bOvp = false;  // true when a negative overpunch character has been 
                        // found @ N1-1 

    
//
// Multics frequently uses certain code sequences which are easily detected
// and optimized; eg. it uses the MLR instruction to copy or zeros segments.
//
// The MLR implementation is correct, not efficent. Copy invokes 12 append
// cycles per word, and fill 8.
//

// Test for the case of aligned word move; and do things a word at a time,
// instead of a byte at a time...

    if (e -> TA1 == CTA9 &&  // src and dst are both char 9
        e -> TA2 == CTA9 &&
        e -> N1 % 4 == 0 &&  // a whole number of words in the src
        e -> N2 == e -> N1 && // the src is the same size as the dest.
        e -> CN1 == 0 &&  // and it starts at a word boundary // BITNO?
        e -> CN2 == 0)
      {
        sim_debug (DBG_TRACE, & cpu_dev, "MLR special case #1\n");
        for ( ; du . CHTALLY < e -> N2; du . CHTALLY += 4)
          {
            uint n = du . CHTALLY / 4;
            word36 w = EISReadIdx (& e -> ADDR1, n);
            EISWriteIdx (& e -> ADDR2, n, w);
          }
        cleanupOperandDescriptor (1, e);
        cleanupOperandDescriptor (2, e);
        // truncation fault check does need to be checked for here since 
        // it is known that N1 == N2
        CLRF (cu . IR, I_TRUNC);
        return;
      }

// Test for the case of aligned word fill; and do things a word at a time,
// instead of a byte at a time...

    if (e -> TA1 == CTA9 && // src and dst are both char 9
        e -> TA2 == CTA9 &&
        e -> N1 == 0 && // the source is entirely fill
        e -> N2 % 4 == 0 && // a whole number of words in the dest
        e -> CN1 == 0 &&  // and it starts at a word boundary // BITNO?
        e -> CN2 == 0)
      {
        sim_debug (DBG_TRACE, & cpu_dev, "MLR special case #2\n");
        word36 w = (word36) fill | ((word36) fill << 9) | ((word36) fill << 18) | ((word36) fill << 27);
        for ( ; du . CHTALLY < e -> N2; du . CHTALLY += 4)
          {
            uint n = du . CHTALLY / 4;
            EISWriteIdx (& e -> ADDR2, n, w);
          }
        cleanupOperandDescriptor (1, e);
        cleanupOperandDescriptor (2, e);
        // truncation fault check does need to be checked for here since 
        // it is known that N1 <= N2
        CLRF (cu . IR, I_TRUNC);
        return;
      }

    for ( ; du . CHTALLY < min (e -> N1, e -> N2); du . CHTALLY ++)
      {
        word9 c = EISget469 (1, du . CHTALLY); // get src char
        word9 cout = 0;
        
        if (e -> TA1 == e -> TA2) 
          EISput469 (2, du . CHTALLY, c);
        else
          {
	  // If data types are dissimilar (TA1 ≠ TA2), each character is
	  // high-order truncated or zero filled, as appropriate, as it is
	  // moved. No character conversion takes place.
            cout = c;
            switch (srcSZ)
              {
                case 6:
                  switch(dstSZ)
                    {
                      case 4:
                        cout = c & 017;    // truncate upper 2-bits
                        break;
                      case 9:
                        break;              // should already be 0-filled
                    }
                  break;
                case 9:
                  switch(dstSZ)
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

	  // If N1 < N2, C(FILL)0 = 1, TA1 = 1, and TA2 = 2 (6-4 move), then
	  // C(Y-charn1)N1-1 is examined for a GBCD overpunch sign. If a
	  // negative overpunch sign is found, then the minus sign character
	  // is placed in C(Y-charn2)N2-1; otherwise, a plus sign character
	  // is placed in C(Y-charn2)N2-1.
            
            if (ovp && (du . CHTALLY == e -> N1 - 1))
              {
	      // this is kind of wierd. I guess that C(FILL)0 = 1 means that
	      // there *is* an overpunch char here.
                bOvp = isOvp (c, & on);
                cout = on;   // replace char with the digit the overpunch 
                             // represents
              }
            EISput469 (2, du . CHTALLY, cout);
          }
      }
    
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL
    // characters are high-order truncated as they are moved to C(Y-charn2). No
    // character conversion takes place.

    if (e -> N1 < e -> N2)
      {
        for ( ; du . CHTALLY < e -> N2 ; du . CHTALLY ++)
          {
            // if there's an overpunch then the sign will be the last of the 
            // fill
            if (ovp && (du . CHTALLY == e -> N2 - 1))
              {
                if (bOvp)   // is c an GEBCD negative overpunch? and of what?
                  EISput469 (2, du . CHTALLY, 015); // 015 is decimal -
                else
                  EISput469 (2, du . CHTALLY, 014); // 014 is decimal +
              }
            else
              EISput469 (2, du . CHTALLY, fillT);
          }
    }
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);

    if (e -> N1 > e -> N2)
      {
        SETF (cu . IR, I_TRUNC);
        if (T && ! TSTF (cu . IR, I_OMASK))
          doFault (FAULT_OFL, 0, "mlr truncation fault");
      }
    else
      CLRF (cu . IR, I_TRUNC);
  } 


void mrl (void)
  {
    EISstruct * e = & currentInstruction . e;

    // For i = 1, 2, ..., minimum (N1,N2)
    //     C(Y-charn1)N1-i → C(Y-charn2)N2-i
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // Indicators: Truncation. If N1 > N2 then ON; otherwise OFF
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    setupOperandDescriptorCache (3, e);
    
    parseAlphanumericOperandDescriptor(1, e, 1);
    parseAlphanumericOperandDescriptor(2, e, 2);
    
    int srcSZ, dstSZ;

    switch (e -> TA1)
      {
        case CTA4:
          srcSZ = 4;
          break;
        case CTA6:
          srcSZ = 6;
          break;
        case CTA9:
          srcSZ = 9;
          break;
      }
    
    switch (e -> TA2)
      {
        case CTA4:
          dstSZ = 4;
          break;
        case CTA6:
          dstSZ = 6;
          break;
        case CTA9:
          dstSZ = 9;
          break;
      }
    
    uint T = bitfieldExtract36 (e -> op0, 26, 1) != 0;  // truncation bit
    
    uint fill = bitfieldExtract36 (e -> op0, 27, 9);
    uint fillT = fill;  // possibly truncated fill pattern

    // play with fill if we need to use it
    switch (dstSZ)
      {
        case 4:
          fillT = fill & 017;    // truncate upper 5-bits
          break;
        case 6:
          fillT = fill & 077;    // truncate upper 3-bits
          break;
      }
    
    // If N1 > N2, then (N1-N2) leading characters of C(Y-charn1) are not moved
    // and the truncation indicator is set ON.

    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL
    // characters are high-order truncated as they are moved to C(Y-charn2). No
    // character conversion takes place.

    // The user of string replication or overlaying is warned that the decimal
    // unit addresses the main memory in unaligned (not on modulo 8 boundary)
    // units of Y-block8 words and that the overlayed string, C(Y-charn2), is
    // not returned to main memory until the unit of Y-block8 words is filled or
    // the instruction completes.

    // If T = 1 and the truncation indicator is set ON by execution of the
    // instruction, then a truncation (overflow) fault occurs.

    // Attempted execution with the xed instruction causes an illegal procedure
    // fault.

    // Attempted repetition with the rpt, rpd, or rpl instructions causes an
    // illegal procedure fault.
    
    bool ovp = (e -> N1 < e -> N2) && (fill & 0400) && (e -> TA1 == 1) &&
               (e -> TA2 == 2); // (6-4 move)
    uint on;     // number overpunch represents (if any)
    bool bOvp = false;  // true when a negative overpunch character has been 
                        // found @ N1-1 

    
//
// Multics frequently uses certain code sequences which are easily detected
// and optimized; eg. it uses the MLR instruction to copy or zeros segments.
//
// The MLR implementation is correct, not efficent. Copy invokes 12 append
// cycles per word, and fill 8.
//

// Test for the case of aligned word move; and do things a word at a time,
// instead of a byte at a time...

    if (e -> TA1 == CTA9 &&  // src and dst are both char 9
        e -> TA2 == CTA9 &&
        e -> N1 % 4 == 0 &&  // a whole number of words in the src
        e -> N2 == e -> N1 && // the src is the same size as the dest.
        e -> CN1 == 0 &&  // and it starts at a word boundary // BITNO?
        e -> CN2 == 0)
      {
        sim_debug (DBG_TRACE, & cpu_dev, "MLR special case #1\n");
        uint limit = e -> N2;
        for ( ; du . CHTALLY < limit; du . CHTALLY += 4)
          {
            uint n = (limit - du . CHTALLY - 1) / 4;
            word36 w = EISReadIdx (& e -> ADDR1, n);
            EISWriteIdx (& e -> ADDR2, n, w);
          }
        cleanupOperandDescriptor (1, e);
        cleanupOperandDescriptor (2, e);
        // truncation fault check does need to be checked for here since 
        // it is known that N1 == N2
        CLRF (cu . IR, I_TRUNC);
        return;
      }

// Test for the case of aligned word fill; and do things a word at a time,
// instead of a byte at a time...

    if (e -> TA1 == CTA9 && // src and dst are both char 9
        e -> TA2 == CTA9 &&
        e -> N1 == 0 && // the source is entirely fill
        e -> N2 % 4 == 0 && // a whole number of words in the dest
        e -> CN1 == 0 &&  // and it starts at a word boundary // BITNO?
        e -> CN2 == 0)
      {
        sim_debug (DBG_TRACE, & cpu_dev, "MLR special case #2\n");
        word36 w = (word36) fill |
                  ((word36) fill << 9) |
                  ((word36) fill << 18) |
                  ((word36) fill << 27);
        uint limit = e -> N2;
        for ( ; du . CHTALLY < e -> N2; du . CHTALLY += 4)
          {
            uint n = (limit - du . CHTALLY - 1) / 4;
            EISWriteIdx (& e -> ADDR2, n, w);
          }
        cleanupOperandDescriptor (1, e);
        cleanupOperandDescriptor (2, e);
        // truncation fault check does need to be checked for here since 
        // it is known that N1 <= N2
        CLRF (cu . IR, I_TRUNC);
        return;
      }

    for ( ; du . CHTALLY < min (e -> N1, e -> N2); du . CHTALLY ++)
      {
        word9 c = EISget469 (1, e -> N1 - du . CHTALLY - 1); // get src char
        word9 cout = 0;
        
        if (e -> TA1 == e -> TA2) 
          EISput469 (2, e -> N2 - du . CHTALLY - 1, c);
        else
          {
	  // If data types are dissimilar (TA1 ≠ TA2), each character is
	  // high-order truncated or zero filled, as appropriate, as it is
	  // moved. No character conversion takes place.
            cout = c;
            switch (srcSZ)
              {
                case 6:
                  switch(dstSZ)
                    {
                      case 4:
                        cout = c & 017;    // truncate upper 2-bits
                        break;
                      case 9:
                        break;              // should already be 0-filled
                    }
                  break;
                case 9:
                  switch(dstSZ)
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

	  // If N1 < N2, C(FILL)0 = 1, TA1 = 1, and TA2 = 2 (6-4 move), then
	  // C(Y-charn1)N1-1 is examined for a GBCD overpunch sign. If a
	  // negative overpunch sign is found, then the minus sign character
	  // is placed in C(Y-charn2)N2-1; otherwise, a plus sign character
	  // is placed in C(Y-charn2)N2-1.
            
            if (ovp && (du . CHTALLY == e -> N1 - 1))
              {
	      // this is kind of wierd. I guess that C(FILL)0 = 1 means that
	      // there *is* an overpunch char here.
                bOvp = isOvp (c, & on);
                cout = on;   // replace char with the digit the overpunch 
                             // represents
              }
            EISput469 (2, e -> N2 - du . CHTALLY - 1, cout);
          }
      }
    
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    C(FILL) → C(Y-charn2)N2-i
    // If N1 < N2 and TA2 = 2 (4-bit data) or 1 (6-bit data), then FILL
    // characters are high-order truncated as they are moved to C(Y-charn2). No
    // character conversion takes place.

    if (e -> N1 < e -> N2)
      {
        for ( ; du . CHTALLY < e -> N2 ; du . CHTALLY ++)
          {
            // if there's an overpunch then the sign will be the last of the 
            // fill
            if (ovp && (du . CHTALLY == e -> N2 - 1))
              {
                if (bOvp)   // is c an GEBCD negative overpunch? and of what?
                  EISput469 (2, e -> N2 - du . CHTALLY - 1, 015); // 015 is decimal -
                else
                  EISput469 (2, e -> N2 - du . CHTALLY - 1, 014); // 014 is decimal +
              }
            else
              EISput469 (2, e -> N2 - du . CHTALLY - 1, fillT);
          }
    }
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);

    if (e -> N1 > e -> N2)
      {
        SETF (cu . IR, I_TRUNC);
        if (T && ! TSTF (cu . IR, I_OMASK))
          doFault (FAULT_OFL, 0, "mlr truncation fault");
      }
    else
      CLRF (cu . IR, I_TRUNC);
  } 

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

/*
 * Load the entire sending string number (maximum length 63 characters) into
 * the decimal unit input buffer as 4-bit digits (high-order truncating 9-bit
 * data). Strip the sign and exponent characters (if any), put them aside into
 * special holding registers and decrease the input buffer count accordingly.
 */

static void EISloadInputBufferNumeric(int k)
{
    EISstruct * e = & currentInstruction . e;

    word9 *p = e->inBuffer; // p points to position in inBuffer where 4-bit chars are stored
    memset(e->inBuffer, 0, sizeof(e->inBuffer));   // initialize to all 0's

    int pos = e->CN[k-1];

    int TN = e->TN[k-1];
    int S = e->S[k-1];  // This is where MVNE gets really nasty.
    // I spit on the designers of this instruction set (and of COBOL.) >Ptui!<

    int N = e->N[k-1];  // number of chars in src string

    EISaddr *a = &e->addr[k-1];

    e->sign = 1;
    e->exponent = 0;

    for(int n = 0 ; n < N ; n += 1)
    {
        word9 c = EISget49(a, &pos, TN);
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


/*
 * Load decimal unit input buffer with sending string characters. Data is read
 * from main memory in unaligned units (not modulo 8 boundary) of Y-block8
 * words. The number of characters loaded is the minimum of the remaining
 * sending string count, the remaining receiving string count, and 64.
 */

static void EISloadInputBufferAlphnumeric (EISstruct * e, int k)
  {
    // p points to position in inBuffer where 4-bit chars are stored
    word9 * p = e -> inBuffer;
    memset (e -> inBuffer, 0, sizeof (e -> inBuffer));// initialize to all 0's

    // minimum of the remaining sending string count, the remaining receiving
    // string count, and 64.
    uint N = min3 (e -> N1, e -> N3, 64);

    for (uint n = 0 ; n < N ; n ++)
      {
        uint c = EISget469 (k, n);
        * p ++ = c;
      }
}

static void EISwriteOutputBufferToMemory (int k)
  {
    EISstruct * e = & currentInstruction . e;

    // 4. If an edit insertion table entry or MOP insertion character is to be
    // stored, ANDed, or ORed into a receiving string of 4- or 6-bit
    // characters, high-order truncate the character accordingly.
    // 5. If the receiving string is 9-bit characters, high-order fill the
    // (4-bit) digits from the input buffer with bits 0-4 of character 8 of the
    // edit insertion table. If the receiving string is 6-bit characters,
    // high-order fill the digits with "00"b.

    for (int n = 0 ; n < e -> dstTally; n ++)
      {
        uint c49 = e -> outBuffer [n];
        EISput469 (k, n, c49);
      }
  }


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
 * This is the Micro Operation Executor/Interpreter
 */

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


#ifdef V4
            if (isDecimalZero (c))
#elif defined (V3)
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

#ifdef V4
            if (isDecimalZero (c))
#elif defined (V3)
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
#ifdef V4
        if (!e->mopES && isDecimalZero (c))
#elif defined (V3)
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
#ifdef V4
        else if ((! e->mopES) && (! isDecimalZero (c)))
#elif defined (V3)
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
#ifdef V4
        if ((!e->mopES) && isDecimalZero (c))
#elif defined (V3)
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
#ifdef V4
        if ((! e->mopES) && (! isDecimalZero (c)))
#elif defined (V3)
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

struct MOPstruct
{
    char *mopName;             // name of microoperation
    int (*f)(EISstruct *e);    // pointer to mop() [returns character to be stored]
};

// Table 4-9. Micro Operation Code Assignment Map
#ifndef QUIET_UNUSED 
static char * mopCodes [040] =
  {
    //        0       1       2       3       4       5       6       7
    /* 00 */  0,     "insm", "enf",  "ses",  "mvzb", "mvza", "mfls", "mflc",
    /* 10 */ "insb", "insa", "insn", "insp", "ign",  "mvc",  "mses", "mors",
    /* 20 */ "lte",  "cht",   0,      0,      0,      0,      0,      0,
    /* 30 */   0,      0,     0,      0,      0,      0,      0,      0
  };
#endif


static MOPstruct mopTab[040] = {
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


/*!
 * fetch MOP from e->mopAddr/e->mopCN ...
 */

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


static void mopExecutor (EISstruct * e, int kMop)
  {
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
    
    EISgetMop(NULL);   // initialize mop getter

    // execute dstTally micro operations
    // The micro operation sequence is terminated normally when the receiving
    // string length becomes exhausted. The micro operation sequence is
    // terminated abnormally (with an illegal procedure fault) if a move from
    // an exhausted sending string or the use of an exhausted MOP string is
    // attempted.
    
    //sim_printf ("(I) mopTally=%d srcTally=%d\n", e->mopTally, e->srcTally);

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
      {
        e->_faults |= FAULT_IPR;   // XXX ill proc fault
      }
    
    if (e -> _faults)
      doFault (FAULT_IPR, ill_proc, "mopExecutor");
}


void mve (void)
  {
    EISstruct * e = & currentInstruction . e;

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

    switch (e -> srcTA)
      {
        case CTA4:
          e -> srcSZ = 4;
          break;
        case CTA6:
          e -> srcSZ = 6;
          break;
        case CTA9:
          e -> srcSZ = 9;
          break;
      }
    
    e -> dstTA = e -> TA3;    // type of chars in dst
    e -> dstCN = e -> CN3;    // starting at char pos CN

    switch (e -> dstTA)
      {
        case CTA4:
          e -> dstSZ = 4;
          break;
        case CTA6:
          e -> dstSZ = 6;
          break;
        case CTA9:
          e -> dstSZ = 9;
          break;
      }
    
    // 1. load sending string into inputBuffer
    EISloadInputBufferAlphnumeric (e, 1);   // according to MF1
    
    // 2. Execute micro operation string, starting with first (4-bit) digit.
    e -> mvne = false;
    
    mopExecutor (e, 2);
    
    e -> dstTally = e -> N3;  // restore dstTally for output
    
    EISwriteOutputBufferToMemory (3);
    cleanupOperandDescriptor (1, e);
    cleanupOperandDescriptor (2, e);
    cleanupOperandDescriptor (3, e);
  }

void mvne (void)
  {
    EISstruct * e = & currentInstruction . e;

    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    setupOperandDescriptor (3, e);
    
    parseNumericOperandDescriptor (1, e);
    parseAlphanumericOperandDescriptor (2, e, 2);
    parseAlphanumericOperandDescriptor (3, e, 3);
    
    // initialize mop flags. Probably best done elsewhere.
    e->mopES = false; // End Suppression flag
    e->mopSN = false; // Sign flag
    e->mopZ  = true;  // Zero flag
    e->mopBZ = false; // Blank-when-zero flag
    
    e -> srcTally = e -> N1;  // number of chars in src (max 63)
    e -> dstTally = e -> N3;  // number of chars in dst (max 63)
    
    e -> srcTN = e -> TN1;    // type of chars in src

#if defined(V1) || defined(V2) || defined(V4)
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
    EISloadInputBufferNumeric(1);   // according to MF1
    
    // 2. Test sign and, if required, set the SN flag. (Sign flag; initially
    // set OFF if the sending string has an alphanumeric descriptor or an
    // unsigned numeric descriptor. If the sending string has a signed numeric
    // descriptor, the sign is initially read from the sending string from the
    // digit position defined by the sign and the decimal type field (S); SN is
    // set OFF if positive, ON if negative. If all digits are zero, the data is
    // assumed positive and the SN flag is set OFF, even when the sign is
    // negative.)

    int sum = 0;
    for(int n = 0 ; n < e -> srcTally ; n ++)
        sum += e -> inBuffer [n];
    if ((e -> sign == -1) && sum)
        e -> mopSN = true;
    
    // 3. Execute micro operation string, starting with first (4-bit) digit.
    e -> mvne = true;
    
    mopExecutor (e, 2);

    e -> dstTally = e -> N3;  // restore dstTally for output
    
    EISwriteOutputBufferToMemory (3);
    cleanupOperandDescriptor (1, e);
    cleanupOperandDescriptor (2, e);
    cleanupOperandDescriptor (3, e);
  }

/*  
 * MVT - Move Alphanumeric with Translation
 */

void mvt (void)
  {
    EISstruct * e = & currentInstruction . e;

    // For i = 1, 2, ..., minimum (N1,N2)
    //    m = C(Y-charn1)i-1
    //    C(Y-char93)m → C(Y-charn2)i-1
    // If N1 < N2, then for i = N1+1, N1+2, ..., N2
    //    m = C(FILL)
    //    C(Y-char93)m → C(Y-charn2)i-1
    
    // Indicators: Truncation. If N1 > N2 then ON; otherwise OFF
    
    setupOperandDescriptor (1, e);
    setupOperandDescriptor (2, e);
    setupOperandDescriptorCache (3, e);
    
    parseAlphanumericOperandDescriptor (1, e, 1);
    parseAlphanumericOperandDescriptor (2, e, 2);
    
    e->srcCN = e->CN1;    // starting at char pos CN
    e->dstCN = e->CN2;    // starting at char pos CN
    
    e->srcTA = e->TA1;
    e->dstTA = e->TA2;
    
    switch (e -> TA1)
      {
        case CTA4:
          e -> srcSZ = 4;
          break;
        case CTA6:
          e -> srcSZ = 6;
          break;
        case CTA9:
          e -> srcSZ = 9;
         break;
      }
    
    switch (e -> TA2)
      {
        case CTA4:
          e -> dstSZ = 4;
          break;
        case CTA6:
          e -> dstSZ = 6;
          break;
        case CTA9:
          e -> dstSZ = 9;
          break;
      }
    
    word36 xlat = e -> op [2];  // 3rd word is a pointer to a translation table
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
        xAddress = (AR[n].WORDNO + SIGNEXT15_18(offset)) & AMASK;
        
        ARn_CHAR = GET_AR_CHAR (n); // AR[n].CHAR;
        ARn_BITNO = GET_AR_BITNO (n); // AR[n].BITNO;
        
#if 0
        if (get_addr_mode() == APPEND_mode || get_addr_mode() == APPEND_BAR_mode)
#else
        if (get_addr_mode() == APPEND_mode)
#endif
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
    
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;  // truncation bit
    
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

    //SCF(e->N1 > e->N2, cu.IR, I_TALLY);   // HWR 7 Feb 2014. Possibly undocumented behavior. TRO may be set also!

    //get469(NULL, 0, 0, 0);    // initialize char getter buffer
    
    for(uint i = 0 ; i < min(e->N1, e->N2); i += 1)
    {
        //int c = get469(e, &e->srcAddr, &e->srcCN, e->TA1); // get src char
        int c = EISget469(1, i); // get src char
        int cidx = 0;
    
        if (e->TA1 == e->TA2)
            EISput469(2, i, xlate (xlatTbl, e -> dstTA, c));
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
            
            EISput469 (2, i, cout);
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
            EISput469 (2, j, cfill);
    }
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif
    if (e->N1 > e->N2)
      {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
          doFault(FAULT_OFL, 0, "mvt truncation fault");
      }
    else
      CLRF(cu.IR, I_TRUNC);
}


/*
 * cmpn - Compare Numeric
 */

void cmpn (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    // C(Y-charn1) :: C(Y-charn2) as numeric values
    
    // Zero If C(Y-charn1) = C(Y-charn2), then ON; otherwise OFF
    // Negative If C(Y-charn1) > C(Y-charn2), then ON; otherwise OFF
    // Carry If | C(Y-charn1) | > | C(Y-charn2) | , then OFF, otherwise ON
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    set.traps=0;
    
    decNumber _1, _2, _3;
    
    int n1 = 0, n2 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(1);   // according to MF1
    
    /*
     * Here we need to distinguish between 4 type of numbers.
     *
     * CSFL - Floating-point, leading sign
     * CSLS - Scaled fixed-point, leading sign
     * CSTS - Scaled fixed-point, trailing sign
     * CSNS - Scaled fixed-point, unsigned
     */
    
    // determine precision
    switch(e->S1)
    {
        case CSFL:
            n1 = e->N1 - 1; // need to account for the - sign
            if (e->srcTN == CTN4)
                n1 -= 2;    // 2 4-bit digits exponent
            else
                n1 -= 1;    // 1 9-bit digit exponent
            sc1 = 0;        // no scaling factor
            break;
            
        case CSLS:
        case CSTS:
            n1 = e->N1 - 1; // only 1 sign
            sc1 = -e->SF1;
            break;
            
        case CSNS:
            n1 = e->N1;     // no sign
            sc1 = -e->SF1;
            break;  // no sign wysiwyg
    }
    decNumber *op1 = decBCD9ToNumber(e->inBuffer, n1, sc1, &_1);
    if (e->sign == -1)
        op1->bits = DECNEG;
    if (e->S1 == CSFL)
        op1->exponent = e->exponent;
    
    
    EISloadInputBufferNumeric(2);   // according to MF2
    switch(e->S2)
    {
        case CSFL:
            n2 = e->N2 - 1; // need to account for the sign
            if (e->srcTN2 == CTN4)
                n2 -= 2;    // 2 4-bit digit exponent
            else
                n2 -= 1;    // 1 9-bit digit exponent
            sc2 = 0;        // no scaling factor
            break;
            
        case CSLS:
        case CSTS:
            n2 = e->N2 - 1; // 1 sign
            sc2 = -e->SF2;
            break;
            
        case CSNS:
            n2 = e->N2;     // no sign
            sc2 = -e->SF2;
            break;  // no sign wysiwyg
    }
    
    decNumber *op2 = decBCD9ToNumber(e->inBuffer, n2, sc2, &_2);
    if (e->sign == -1)
        op2->bits = DECNEG;
    if (e->S2 == CSFL)
        op2->exponent = e->exponent;
    
    // signed-compare
    decNumber *cmp = decNumberCompare(&_3, op1, op2, &set); // compare signed op1 :: op2
    int cSigned = decNumberToInt32(cmp, &set);
    
    // take absolute value of operands
    op1 = decNumberAbs(op1, op1, &set);
    op2 = decNumberAbs(op2, op2, &set);

    // magnitude-compare
    decNumber *mcmp = decNumberCompare(&_3, op1, op2, &set); // compare signed op1 :: op2
    int cMag = decNumberToInt32(mcmp, &set);
    
    // Zero If C(Y-charn1) = C(Y-charn2), then ON; otherwise OFF
    // Negative If C(Y-charn1) > C(Y-charn2), then ON; otherwise OFF
    // Carry If | C(Y-charn1) | > | C(Y-charn2) | , then OFF, otherwise ON

    SCF(cSigned == 0, cu.IR, I_ZERO);
    SCF(cSigned == 1, cu.IR, I_NEG);
    SCF(cMag != 1, cu.IR, I_CARRY);
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
#endif
}

/*
 * mvn - move numeric (initial version was deleted by house gnomes)
 */

/*
 * write 4-bit chars to memory @ pos ...
 */

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


    EISWriteIdx(p, 0, w); // XXX this is the ineffecient part!

    *pos += 1;       // to next char.
}

/*
 * write 6-bit digits to memory @ pos ...
 */

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

    EISWriteIdx (p, 0, w); // XXX this is the ineffecient part!

    *pos += 1;       // to next byte.
}

/*
 * write 9-bit bytes to memory @ pos ...
 */

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

    EISWriteIdx (p, 0, w); // XXX this is the ineffecient part!

    *pos += 1;       // to next byte.
}

/*
 * write a 4-, or 9-bit numeric char to dstAddr ....
 */

static void EISwrite49(EISaddr *p, int *pos, int tn, int c49)
{
    switch(tn)
    {
        case CTN4:
            return EISwrite4(p, pos, c49);
        case CTN9:
            return EISwrite9(p, pos, c49);
    }
}

/*
 * write a 4-, 6-, or 9-bit char to dstAddr ....
 */

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

void mvn (void)
{
    /*
     * EXPLANATION:
     * Starting at location YC1, the decimal number of data type TN1 and sign and decimal type S1 is moved, properly scaled, to the decimal number of data type TN2 and sign and decimal type S2 that starts at location YC2.
     * If S2 indicates a fixed-point format, the results are stored as L2 digits using scale factor SF2, and thereby may cause most-significant-digit overflow and/or least- significant-digit truncation.
     * If P = 1, positive signed 4-bit results are stored using octal 13 as the plus sign. Rounding is legal for both fixed-point and floating-point formats. If P = 0, positive signed 4-bit results are stored using octal 14 as the plus sign.
     * Provided that string 1 and string 2 are not overlapped, the contents of the decimal number that starts in location YC1 remain unchanged.
     */

    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;
    
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->P = bitfieldExtract36(e->op0, 35, 1) != 0;  // 4-bit data sign character control
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;  // truncation bit
    e->R = bitfieldExtract36(e->op0, 25, 1) != 0;  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
    
    e->dstTN = e->TN2;    // type of chars in dst
    e->dstCN = e->CN2;    // starting at char pos CN
    
    sim_debug (DBG_CAC, & cpu_dev, "mvn(1): TN1 %d CN1 %d N1 %d TN2 %d CN2 %d N2 %d\n", e->TN1, e->CN1, e->N1, e->TN2, e->CN2, e->N2);
    sim_debug (DBG_CAC, & cpu_dev, "mvn(2): SF1 %d              SF2 %d\n", e->SF1, e->SF2);
    sim_debug (DBG_CAC, & cpu_dev, "mvn(3): OP1 %012llo OP2 %012llo\n", e->OP1, e->OP2);

    decContext set;
    decContextDefaultDPS8(&set);
    set.traps=0;
    
    decNumber _1;
    
    int n1 = 0, n2 = 0, sc1 = 0;
    
    EISloadInputBufferNumeric (1);   // according to MF1
    
    /*
     * Here we need to distinguish between 4 type of numbers.
     *
     * CSFL - Floating-point, leading sign
     * CSLS - Scaled fixed-point, leading sign
     * CSTS - Scaled fixed-point, trailing sign
     * CSNS - Scaled fixed-point, unsigned
     */
    
    // determine precision
    switch(e->S1)
    {
        case CSFL:
            n1 = e->N1 - 1; // need to account for the - sign
            if (e->srcTN == CTN4)
                n1 -= 2;    // 2 4-bit digits exponent
            else
                n1 -= 1;    // 1 9-bit digit exponent
            sc1 = 0;        // no scaling factor
            break;
        
        case CSLS:
        case CSTS:
            n1 = e->N1 - 1; // only 1 sign
            sc1 = -e->SF1;
            break;
        
        case CSNS:
            n1 = e->N1;     // no sign
            sc1 = -e->SF1;
            break;  // no sign wysiwyg
    }
    
    sim_debug (DBG_CAC, & cpu_dev, "n1 %d sc1 %d\n", n1, sc1);

    decNumber *op1 = decBCD9ToNumber(e->inBuffer, n1, sc1, &_1);
    
    
    if (e->sign == -1)
        op1->bits = DECNEG;
    if (e->S1 == CSFL)
        op1->exponent = e->exponent;
    if (decNumberIsZero(op1))
        op1->exponent = 127;
   
    if_sim_debug (DBG_CAC, & cpu_dev)
    {
        PRINTDEC("mvn input (op1)", op1);
    }
    
    bool Ovr = false, Trunc = false;
    
//    int SF = calcSF(e->SF1, e->SF2, 0);
//    char *res = formatDecimal(&set, op1, e->dstTN, e->N2, e->S2, SF, e->R, &Ovr, &Trunc);
    
    char *res = formatDecimal(&set, op1, e->dstTN, e->N2, e->S2, e->SF2, e->R, &Ovr, &Trunc);
    
#ifndef SPEED
    if_sim_debug (DBG_CAC, & cpu_dev)
        sim_printf("mvn res: '%s'\n", res);
#endif
    
    // now write to memory in proper format.....
    switch(e->S2)
    {
        case CSFL:
            n2 = e->N2 - 1; // need to account for the sign
            if (e->dstTN == CTN4)
                n2 -= 2;    // 2 4-bit digit exponent
            else
                n2 -= 1;    // 1 9-bit digit exponent
            break;
        
        case CSLS:
        case CSTS:
            n2 = e->N2 - 1; // 1 sign
            break;
        
        case CSNS:
            n2 = e->N2;     // no sign
            break;          // no sign wysiwyg
    }
    
    sim_debug (DBG_CAC, & cpu_dev,
      "n2 %d\n",
      n2);

    //word18 dstAddr = e->dstAddr;
    int pos = e->dstCN;
    
    // 1st, take care of any leading sign .......
    switch(e->S2)
    {
        case CSFL:  // floating-point, leading sign.
        case CSLS:  // fixed-point, leading sign
            switch(e->dstTN)
            {
                case CTN4:
                    if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, (decNumberIsNegative(op1) && !decNumberIsZero(op1)) ? 015 : 013);  // special +
                    else
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, (decNumberIsNegative(op1) && !decNumberIsZero(op1)) ? 015 : 014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR2, &pos, e->dstTN, (decNumberIsNegative(op1) && !decNumberIsZero(op1)) ? '-' : '+');
                    break;
            }
            break;
        
        case CSTS:  // nuttin' to do here .....
        case CSNS:
            break;  // no sign wysiwyg
    }
    
    // 2nd, write the characteristic .....
    for(int i = 0 ; i < n2 ; i++)
        switch(e->dstTN)
        {
            case CTN4:
                EISwrite49(&e->ADDR2, &pos, e->dstTN, res[i] - '0');
                break;
            case CTN9:
                EISwrite49(&e->ADDR2, &pos, e->dstTN, res[i]);
                break;
        }
    
    // 3rd, take care of any trailing sign or exponent ...
    switch(e->S2)
    {
        case CSTS:  // write trailing sign ....
            switch(e->dstTN)
            {
                case CTN4:
                    if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, (decNumberIsNegative(op1) && !decNumberIsZero(op1)) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, (decNumberIsNegative(op1) && !decNumberIsZero(op1)) ? 015 :  014);  // default +
                    break;
            
                case CTN9:
                    EISwrite49(&e->ADDR2, &pos, e->dstTN, (decNumberIsNegative(op1) && !decNumberIsZero(op1)) ? '-' : '+');
                    break;
            }
            break;
        
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    EISwrite49(&e->ADDR2, &pos, e->dstTN, (op1->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR2, &pos, e->dstTN,  op1->exponent       & 0xf); // lower 4-bits
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR2, &pos, e->dstTN, op1->exponent & 0xff);    // write 8-bit exponent
                break;
            }
            break;
        
        case CSLS:  // fixed point, leading sign - already done
        case CSNS:  // fixed point, unsigned - nuttin' needed to do
            break;
    }
    
    // set flags, etc ...
    if (e->S2 == CSFL)
    {
        if (op1->exponent > 127)
            SETF(cu.IR, I_EOFL);
        if (op1->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF((decNumberIsNegative(op1) && !decNumberIsZero(op1)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op1), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF

#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
#endif
    
    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"mvn truncation(overflow) fault");
    }
    
    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
          doFault(FAULT_OFL, 0,"mvn overflow fault");
    }

}


void csl (bool isSZTL)
{
    DCDstruct * ins = & currentInstruction;
//sim_printf ("[%lld] %05o\n", sim_timell (), PPR . PSR);
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
    
    e->F = bitfieldExtract36(e->op0, 35, 1) != 0;   // fill bit
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;   // T (enablefault) bit
    
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

    bool bR = false; // result bit
//sim_printf ("CHTALLY %d N1 %d N2 %d\n", du . CHTALLY, e -> N1, e -> N2);
    for( ; du . CHTALLY < min(e->N1, e->N2) ; du . CHTALLY += 1)
    {
        //bool b1 = EISgetBitRW(&e->ADDR1);  // read w/ addt incr from src 1
        bool b1 = EISgetBitRWN(&e->ADDR1);  // read w/ addt incr from src 1
        
        // If we are a SZTL, addr2 is read only, increment here.
        // If we are a CSL, addr2 will be incremented below in the write cycle
        e->ADDR2.incr = isSZTL;
        e->ADDR2.mode = eRWreadBit;
        //bool b2 = EISgetBitRW(&e->ADDR2);  // read w/ no addr incr from src2 to in anticipation of a write
        bool b2 = EISgetBitRWN(&e->ADDR2);  // read w/ no addr incr from src2 to in anticipation of a write
        
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
            //CLRF(cu.IR, I_ZERO);
            du . Z = 0;
            if (isSZTL)
                break;
        }

        if (! isSZTL)
        {
            // write out modified bit
            e->ADDR2.bit = bR ? 1 : 0;              // set bit contents to write
            e->ADDR2.incr = true;           // we want address incrementing
            e->ADDR2.mode = eRWwriteBit;    // we want to write the bit
            //EISgetBitRW(&e->ADDR2);    // write bit w/ addr increment to memory
            EISgetBitRWN(&e->ADDR2);    // write bit w/ addr increment to memory
#ifdef EIS_CACHE
// XXX ticket #31
// This a little brute force; it we fault on the next read, the cached value
// is lost. There might be a way to logic it up so that when the next read
// word offset changes, then we write the cache before doing the read. For
// right now, be pessimistic. Sadly, since this is a bit loop, it is very.
            EISWriteCache (&e->ADDR2);
#endif
        }
    }
    
    if (e->N1 < e->N2)
    {
        for(; du . CHTALLY < e->N2 ; du . CHTALLY += 1)
        {
            bool b1 = e->F;
            
            // If we are a SZTL, addr2 is read only, increment here.
            // If we are a CSL, addr2 will be incremented below in the write cycle
            e->ADDR2.incr = isSZTL;
            e->ADDR2.mode = eRWreadBit;
            //bool b2 = EISgetBitRW(&e->ADDR2); // read w/ no addr incr from src2 to in anticipation of a write
            bool b2 = EISgetBitRWN(&e->ADDR2); // read w/ no addr incr from src2 to in anticipation of a write
            
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
                //CLRF(cu.IR, I_ZERO);
                du . Z = 0;
                if (isSZTL)
                  break;
            }
        
            if (! isSZTL)
            {
                // write out modified bit
                e->ADDR2.bit = bR ? 1 : 0;
                e->ADDR2.mode = eRWwriteBit;
                e->ADDR2.incr = true;
                //EISgetBitRW(&e->ADDR2);
                EISgetBitRWN(&e->ADDR2);
#ifdef EIS_CACHE
// XXX ticket #31
// This a little brute force; it we fault on the next read, the cached value
// is lost. There might be a way to logic it up so that when the next read
// word offset changes, then we write the cache before doing the read. For
// right now, be pessimistic. Sadly, since this is a bit loop, it is very.
                EISWriteCache (&e->ADDR2);
#endif
            }
        }
    }
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
#endif
    if (du . Z)
      SETF (cu . IR, I_ZERO);
    else
      CLRF (cu . IR, I_ZERO);
    if (e->N1 > e->N2)
    {
        // NOTES: If N1 > N2, the low order (N1-N2) bits of C(Y-bit1) are not
        // processed and the truncation indicator is set ON.
        //
        // If T = 1 and the truncation indicator is set ON by execution of the
        // instruction, then a truncation (overflow) fault occurs.
        
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
        {
            doFault(FAULT_OFL, 0, "csl truncation fault");
        }
    }
    else
        CLRF(cu.IR, I_TRUNC);
}

