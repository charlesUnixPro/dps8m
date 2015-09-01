#include <ctype.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_iefp.h"

// Local optimization
#define ABD_BITS

//  EISWriteCache  -- flush the cache
//
//
//  EISWriteIdx (p, n, data); -- write to cache at p->addr [n]; updates p->addr
//  EISRead (p) -- read to cache from p->addr
//  EISReadIdx (p, n)  -- read to cache from p->addr[n]; updates p->addr
//  EISReadN (p, n, dst) -- read N words to dst; updates p-> addr
 
//  EISget369 (k, i)
//  EISput369 (k, i, c)

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
    word18 addressN = p -> address + n;
    addressN &= AMASK;
    p -> address = addressN;
    word36 data = EISRead (p);
    return data;
  }

static void EISReadN (EISaddr * p, uint N, word36 *dst)
  {
    for (uint n = 0; n < N; n ++)
      {
        * dst ++ = EISRead (p);
        p -> address ++;
        p -> address &= AMASK;
      }
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

void xmlr (void)
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
