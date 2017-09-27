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
 * \file dps8_utils.c
 * \project dps8
 * \date 9/25/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_ins.h"
#include "dps8_utils.h"
#include "dps8_opcodetable.h"

/*
 * misc utility routines used by simulator
 */

char * dumpFlags(word18 flags)
{
    static char buffer[256] = "";
    
    sprintf(buffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
#ifdef DPS8M
            flags & I_HEX   ? "Hex "   : "",
#endif
#ifdef L68
            "",
#endif
            flags & I_ABS   ? "Abs "   : "",
            flags & I_MIF   ? "MIF "  : "",
            flags & I_TRUNC ? "Trunc " : "",
            flags & I_NBAR  ? "~BAR "  : "",
            flags & I_PMASK ? "PMask " : "",
            flags & I_PERR  ? "PErr"   : "",
            flags & I_TALLY ? "Tally " : "",
            flags & I_OMASK ? "OMASK " : "",
            flags & I_EUFL  ? "EUFL "  : "",
            flags & I_EOFL  ? "EOFL "  : "",
            flags & I_OFLOW ? "Ovr "   : "",
            flags & I_CARRY ? "Carry " : "",
            flags & I_NEG   ? "Neg "   : "",
            flags & I_ZERO  ? "Zero "  : ""
            );
    return buffer;
    
}

static char * dps8_strupr(char *str)
{
    char *s;
    
    for(s = str; *s; s++)
        *s = (char) toupper((unsigned char)*s);
    return str;
}

//! get instruction info for IWB ...

static opCode UnImp = {"(unimplemented)", 0, 0, 0, 0};

struct opCode *getIWBInfo(DCDstruct *i)
{
    opCode *p;
    
    if (i->opcodeX == false)
        p = &NonEISopcodes[i->opcode];
    else
        p = &EISopcodes[i->opcode];
    
#ifndef QUIET_UNUSED
    if (p->mne == 0)
    {
        int r = 1;
    }
#endif
    
    return p->mne ? p : &UnImp;
}

char *disAssemble(word36 instruction)
{
    word9  opcode  = GET_OP(instruction);   ///< get opcode
    int32  opcodeX = GET_OPX(instruction);  ///< opcode extension
    word18 address = GET_ADDR(instruction);
    word1  a       = GET_A(instruction);
    //int32 i       = GET_I(instruction);
    word6  tag     = GET_TAG(instruction);

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
    
    char buff[64];
    
    if (a)
    {
        int n = (address >> 15) & 07;
        int offset = address & 077777;
    
        sprintf(buff, " pr%d|%o", n, offset);
        strcat (result, buff);
        // return dps8_strupr(result);
    } else {
        sprintf(buff, " %06o", address);
        strcat (result, buff);
    }
    // get mod
    strcpy(buff, "");
    for(uint n = 0 ; n < 0100 ; n++)
        if (extMods[n].mod)
            if(n == tag)
            {
                strcpy(buff, extMods[n].mod);
                break;
            }

    if (strlen(buff))
    {
        strcat(result, ",");
        strcat(result, buff);
    }
    
    return dps8_strupr(result);
}

/*
 * getModString ()
 *
 * Convert instruction address modifier tag to printable string
 * WARNING: returns pointer to statically allocated string
 *
 */

char *getModString(word6 tag)
{
    static char msg[256];
    strcpy(msg, "none");
    
    if (tag >= 0100)
    {
        sprintf(msg, "getModReg(tag out-of-range %o)", tag);
    } else {
        for(uint n = 0 ; n < 0100 ; n++)
            if (extMods[n].mod)
                if(n == tag)
                {
                    strcpy(msg, extMods[n].mod);
                    break;
                }

    }
    return msg;
}


/*
 * 36-bit arithmetic stuff ...
 */
/* Single word integer routines */

word36 Add36b (word36 op1, word36 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {
    CPT (cpt2L, 17); // Add36b
    sim_debug (DBG_TRACE, & cpu_dev, "Add36b op1 %012"PRIo64" op2 %012"PRIo64" carryin %o flagsToSet %06o flags %06o\n", op1, op2, carryin, flagsToSet, * flags); 
// https://en.wikipedia.org/wiki/Two%27s_complement#Addition
//
// In general, any two N-bit numbers may be added without overflow, by first
// sign-extending both of them to N + 1 bits, and then adding as above. The
// N + 1 bits result is large enough to represent any possible sum (N = 5 two's
// complement can represent values in the range −16 to 15) so overflow will
// never occur. It is then possible, if desired, to 'truncate' the result back
// to N bits while preserving the value if and only if the discarded bit is a
// proper sign extension of the retained result bits. This provides another
// method of detecting overflow—which is equivalent to the method of comparing
// the carry bits—but which may be easier to implement in some situations,
// because it does not require access to the internals of the addition.

    // 37 bit arithmetic for the above N+1 algorithm
    word38 op1e = op1 & MASK36;
    word38 op2e = op2 & MASK36;
    word38 ci = carryin ? 1 : 0;

    // extend sign bits
    if (op1e & SIGN36)
      op1e |= BIT37;
    if (op2e & SIGN36)
      op2e |= BIT37;

    // Do the math
    word38 res = op1e + op2e + ci;

    // Extract the overflow bits
    bool r37 = res & BIT37 ? true : false;
    bool r36 = res & SIGN36 ? true : false;

    // Extract the carry bit
    bool r38 = res & BIT38 ? true : false;
   
    // Check for overflow 
    * ovf = r37 ^ r36;

    // Check for carry 
    bool cry = r38;

    // Truncate the result
    res &= MASK36;

#ifdef PANEL
    if (cry) CPT (cpt2L, 28); // carry
    if (ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN36) CPT (cpt2L, 31); // neg
#endif

    if (flagsToSet & I_CARRY)
      {
        if (cry)
          SETF (* flags, I_CARRY);
        else
          CLRF (* flags, I_CARRY);
      }
 
    if (chkOVF () && (flagsToSet & I_OFLOW))
      {
        if (* ovf)
          SETF (* flags, I_OFLOW);      // overflow
      }
    
    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }
    
    if (flagsToSet & I_NEG)
      {
        if (res & SIGN36)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }
    
    sim_debug (DBG_TRACE, & cpu_dev, "Add36b res %012"PRIo64" flags %06o ovf %o\n", res, * flags, * ovf); 
    return res;
  }

word36 Sub36b (word36 op1, word36 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {
    CPT (cpt2L, 18); // Sub36b

// https://en.wikipedia.org/wiki/Two%27s_complement
//
// As for addition, overflow in subtraction may be avoided (or detected after
// the operation) by first sign-extending both inputs by an extra bit.
//
// AL39:
//
//  If carry indicator ON, then C(A) - C(Y) -> C(A)
//  If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

    // 38 bit arithmetic for the above N+1 algorithm
    word38 op1e = op1 & MASK36;
    word38 op2e = op2 & MASK36;
    // Note that carryin has an inverted sense for borrow
    word38 ci = carryin ? 0 : 1;

    // extend sign bits
    if (op1e & SIGN36)
      op1e |= BIT37;
    if (op2e & SIGN36)
      op2e |= BIT37;

    // Do the math
    word38 res = op1e - op2e - ci;

    // Extract the overflow bits
    bool r37 = (res & BIT37) ? true : false;
    bool r36 = (res & SIGN36) ? true : false;

    // Extract the carry bit
    bool r38 = res & BIT38 ? true : false;
   
    // Truncate the result
    res &= MASK36;

    // Check for overflow 
    * ovf = r37 ^ r36;

    // Check for carry 
    bool cry = r38;

#ifdef PANEL
    if (cry) CPT (cpt2L, 28); // carry
    if (ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN36) CPT (cpt2L, 31); // neg
#endif

    if (flagsToSet & I_CARRY)
      {
        if (cry) // Note inverted logic for subtraction
          CLRF (* flags, I_CARRY);
        else
          SETF (* flags, I_CARRY);
      }
 
    if (chkOVF () && (flagsToSet & I_OFLOW))
      {
        if (* ovf)
          SETF (* flags, I_OFLOW);      // overflow
      }
    
    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }
    
    if (flagsToSet & I_NEG)
      {
        if (res & SIGN36)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }
    
    return res;
  }

word18 Add18b (word18 op1, word18 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {
    CPT (cpt2L, 19); // Add18b

// https://en.wikipedia.org/wiki/Two%27s_complement#Addition
//
// In general, any two N-bit numbers may be added without overflow, by first
// sign-extending both of them to N + 1 bits, and then adding as above. The
// N + 1 bits result is large enough to represent any possible sum (N = 5 two's
// complement can represent values in the range −16 to 15) so overflow will
// never occur. It is then possible, if desired, to 'truncate' the result back
// to N bits while preserving the value if and only if the discarded bit is a
// proper sign extension of the retained result bits. This provides another
// method of detecting overflow—which is equivalent to the method of comparing
// the carry bits—but which may be easier to implement in some situations,
// because it does not require access to the internals of the addition.

    // 19 bit arithmetic for the above N+1 algorithm
    word20 op1e = op1 & MASK18;
    word20 op2e = op2 & MASK18;
    word20 ci = carryin ? 1 : 0;

    // extend sign bits
    if (op1e & SIGN18)
      op1e |= BIT19;
    if (op2e & SIGN18)
      op2e |= BIT19;

    // Do the math
    word20 res = op1e + op2e + ci;

    // Extract the overflow bits
    bool r19 = (res & BIT19) ? true : false;
    bool r18 = (res & SIGN18) ? true : false;

    // Extract the carry bit
    bool r20 = res & BIT20 ? true : false;
   
    // Truncate the result
    res &= MASK18;

    // Check for overflow 
    * ovf = r19 ^ r18;

    // Check for carry 
    bool cry = r20;

#ifdef PANEL
    if (cry) CPT (cpt2L, 28); // carry
    if (ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN36) CPT (cpt2L, 31); // neg
#endif

    if (flagsToSet & I_CARRY)
      {
        if (cry)
          SETF (* flags, I_CARRY);
        else
          CLRF (* flags, I_CARRY);
      }
 
    if (chkOVF () && (flagsToSet & I_OFLOW))
      {
        if (* ovf)
          SETF (* flags, I_OFLOW);      // overflow
      }
    
    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }
    
    if (flagsToSet & I_NEG)
      {
        if (res & SIGN18)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }
    
    return (word18) res;
  }

word18 Sub18b (word18 op1, word18 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {
    CPT (cpt2L, 20); // Sub18b

// https://en.wikipedia.org/wiki/Two%27s_complement
//
// As for addition, overflow in subtraction may be avoided (or detected after
// the operation) by first sign-extending both inputs by an extra bit.
//
// AL39:
//
//  If carry indicator ON, then C(A) - C(Y) -> C(A)
//  If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

    // 19 bit arithmetic for the above N+1 algorithm
    word20 op1e = op1 & MASK18;
    word20 op2e = op2 & MASK18;
    // Note that carryin has an inverted sense for borrow
    word20 ci = carryin ? 0 : 1;

    // extend sign bits
    if (op1e & SIGN18)
      op1e |= BIT19;
    if (op2e & SIGN18)
      op2e |= BIT19;

    // Do the math
    word20 res = op1e - op2e - ci;

    // Extract the overflow bits
    bool r19 = res & BIT19 ? true : false;
    bool r18 = res & SIGN18 ? true : false;

    // Extract the carry bit
    bool r20 = res & BIT20 ? true : false;
   
    // Truncate the result
    res &= MASK18;

    // Check for overflow 
    * ovf = r19 ^ r18;

    // Check for carry 
    bool cry = r20;

#ifdef PANEL
    if (cry) CPT (cpt2L, 28); // carry
    if (ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN36) CPT (cpt2L, 31); // neg
#endif

    if (flagsToSet & I_CARRY)
      {
        if (cry) // Note inverted logic for subtraction
          CLRF (* flags, I_CARRY);
        else
          SETF (* flags, I_CARRY);
      }
 
    if (chkOVF () && (flagsToSet & I_OFLOW))
      {
        if (* ovf)
          SETF (* flags, I_OFLOW);      // overflow
      }
    
    if (flagsToSet & I_ZERO)
      {
        if (res)
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }
    
    if (flagsToSet & I_NEG)
      {
        if (res & SIGN18)
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }
    
    return res;
  }

word72 Add72b (word72 op1, word72 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {
    CPT (cpt2L, 21); // Add72b
#ifdef ISOLTS
//if (currentRunningCpuIdx)
//sim_printf ("Add72b op1 %012"PRIo64"%012"PRIo64" op2 %012"PRIo64"%012"PRIo64" carryin %o flagsToSet %06o flags %06o ovf %o\n",
 //(word36) ((op1 >> 36) & MASK36), (word36) (op1 & MASK36), (word36) ((op2 >> 36) & MASK36), (word36) (op2 & MASK36), carryin, flagsToSet, * flags, * ovf); 
#endif

// https://en.wikipedia.org/wiki/Two%27s_complement#Addition
//
// In general, any two N-bit numbers may be added without overflow, by first
// sign-extending both of them to N + 1 bits, and then adding as above. The
// N + 1 bits result is large enough to represent any possible sum (N = 5 two's
// complement can represent values in the range −16 to 15) so overflow will
// never occur. It is then possible, if desired, to 'truncate' the result back
// to N bits while preserving the value if and only if the discarded bit is a
// proper sign extension of the retained result bits. This provides another
// method of detecting overflow—which is equivalent to the method of comparing
// the carry bits—but which may be easier to implement in some situations,
// because it does not require access to the internals of the addition.

    // 73 bit arithmetic for the above N+1 algorithm
#ifdef NEED_128
    word74 op1e = and_128 (op1, MASK72);
    word74 op2e = and_128 (op2, MASK72);
    word74 ci = construct_128 (0, carryin ? 1 : 0);
#else
    word74 op1e = op1 & MASK72;
    word74 op2e = op2 & MASK72;
    word74 ci = carryin ? 1 : 0;
#endif

    // extend sign bits
#ifdef NEED_128
    if (isnonzero_128 (and_128 (op1e, SIGN72)))
      op1e = or_128 (op1e, BIT73);
    if (isnonzero_128 (and_128 (op2e, SIGN72)))
      op2e = or_128 (op2e, BIT73);
#else
    if (op1e & SIGN72)
      op1e |= BIT73;
    if (op2e & SIGN72)
      op2e |= BIT73;
#endif

    // Do the math
#ifdef NEED_128
    word74 res = add_128 (op1e, add_128 (op2e, ci));
#else
    word74 res = op1e + op2e + ci;
#endif

    // Extract the overflow bits
#ifdef NEED_128
    bool r73 = isnonzero_128 (and_128 (res, BIT73));
    bool r72 = isnonzero_128 (and_128 (res, SIGN72));
#else
    bool r73 = res & BIT73 ? true : false;
    bool r72 = res & SIGN72 ? true : false;
#endif

    // Extract the carry bit
#ifdef NEED_128
    bool r74 = isnonzero_128 (and_128 (res, BIT74));
#else
    bool r74 = res & BIT74 ? true : false;
#endif
   
    // Truncate the result
#ifdef NEED_128
    res = and_128 (res, MASK72);
#else
    res &= MASK72;
#endif

    // Check for overflow 
    * ovf = r73 ^ r72;

    // Check for carry 
    bool cry = r74;

#ifdef PANEL
    if (cry) CPT (cpt2L, 28); // carry
    if (ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN36) CPT (cpt2L, 31); // neg
#endif

#ifdef ISOLTS
//if (currentRunningCpuIdx)
//{
////char buf [1024];
////print_int128 (res, buf);
//sim_printf ("res %012"PRIo64"%012"PRIo64"\nr72 %d r73 %d r74 %d ovf %d cry %d\n", ((word36) (res >> 36)) & MASK36, (word36) res & MASK36, r72, r73, r74, * ovf, cry);
//}
#endif
    if (flagsToSet & I_CARRY)
      {
        if (cry)
          SETF (* flags, I_CARRY);
        else
          CLRF (* flags, I_CARRY);
      }
 
    if (chkOVF () && (flagsToSet & I_OFLOW))
      {
        if (* ovf)
          SETF (* flags, I_OFLOW);      // overflow
      }
    
    if (flagsToSet & I_ZERO)
      {
#ifdef NEED_128
        if (isnonzero_128 (res))
#else
        if (res)
#endif
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }
    
    if (flagsToSet & I_NEG)
      {
#ifdef NEED_128
        if (isnonzero_128 (and_128 (res, SIGN72)))
#else
        if (res & SIGN72)
#endif
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }
    
#ifdef ISOLTS
//if (currentRunningCpuIdx)
//{
//sim_printf ("Sub72b res %012"PRIo64"%012"PRIo64" flags %06o ovf %o\n", (word36) ((res >> 36) & MASK36), (word36) (res & MASK36), * flags, * ovf); 
//}
#endif
    return res;
  }


word72 Sub72b (word72 op1, word72 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf)
  {
    CPT (cpt2L, 22); // Sub72b
#ifdef ISOLTS
//if (currentRunningCpuIdx)
//sim_printf ("Sub72b op1 %012"PRIo64"%012"PRIo64" op2 %012"PRIo64"%012"PRIo64" carryin %o flagsToSet %06o flags %06o ovf %o\n",
 //(word36) ((op1 >> 36) & MASK36), (word36) (op1 & MASK36), (word36) ((op2 >> 36) & MASK36), (word36) (op2 & MASK36), carryin, flagsToSet, * flags, * ovf); 
#endif
#ifdef NEED_128
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b op1 %012"PRIo64"%012"PRIo64" op2 %012"PRIo64"%012"PRIo64" carryin %o flagsToSet %06o flags %06o\n",
 (word36) ((rshift_128 (op1, 36).l) & MASK36), (word36) (op1.l & MASK36), (word36) (rshift_128 (op2, 36).l & MASK36), (word36) (op2.l & MASK36), carryin, flagsToSet, * flags); 
#else
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b op1 %012"PRIo64"%012"PRIo64" op2 %012"PRIo64"%012"PRIo64" carryin %o flagsToSet %06o flags %06o\n",
 (word36) ((op1 >> 36) & MASK36), (word36) (op1 & MASK36), (word36) ((op2 >> 36) & MASK36), (word36) (op2 & MASK36), carryin, flagsToSet, * flags); 
#endif

// https://en.wikipedia.org/wiki/Two%27s_complement
//
// As for addition, overflow in subtraction may be avoided (or detected after
// the operation) by first sign-extending both inputs by an extra bit.
//
// AL39:
//
//  If carry indicator ON, then C(A) - C(Y) -> C(A)
//  If carry indicator OFF, then C(A) - C(Y) - 1 -> C(A)

    // 73 bit arithmetic for the above N+1 algorithm
#ifdef NEED_128
    word74 op1e = and_128 (op1, MASK72);
    word74 op2e = and_128 (op2, MASK72);
#else
    word74 op1e = op1 & MASK72;
    word74 op2e = op2 & MASK72;
#endif
    // Note that carryin has an inverted sense for borrow
#ifdef NEED_128
    word74 ci = construct_128 (0, carryin ? 0 : 1);
#else
    word74 ci = carryin ? 0 : 1;
#endif

    // extend sign bits
#ifdef NEED_128
    if (isnonzero_128 (and_128 (op1e, SIGN72)))
      op1e = or_128 (op1e, BIT73);
    if (isnonzero_128 (and_128 (op2e, SIGN72)))
      op2e = or_128 (op2e, BIT73);
#else
    if (op1e & SIGN72)
      op1e |= BIT73;
    if (op2e & SIGN72)
      op2e |= BIT73;
#endif

    // Do the math
#ifdef NEED_128
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b op1e %012"PRIo64"%012"PRIo64" op2e %012"PRIo64"%012"PRIo64" carryin %o flagsToSet %06o flags %06o\n",
 (word36) ((rshift_128 (op1e, 36).l) & MASK36), (word36) (op1e.l & MASK36), (word36) (rshift_128 (op2e, 36).l & MASK36), (word36) (op2e.l & MASK36), carryin, flagsToSet, * flags); 
#else
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b op1e %012"PRIo64"%012"PRIo64" op2e %012"PRIo64"%012"PRIo64" carryin %o flagsToSet %06o flags %06o\n",
 (word36) ((op1e >> 36) & MASK36), (word36) (op1e & MASK36), (word36) ((op2e >> 36) & MASK36), (word36) (op2e & MASK36), carryin, flagsToSet, * flags); 
#endif
#ifdef NEED_128
    word74 res = subtract_128 (subtract_128 (op1e, op2e), ci);
#else
    word74 res = op1e - op2e - ci;
#endif
#ifdef NEED_128
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b res %012"PRIo64"%012"PRIo64" flags %06o ovf %o\n", (word36) (rshift_128 (res, 36).l & MASK36), (word36) (res.l & MASK36), * flags, * ovf); 
#else
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b res %012"PRIo64"%012"PRIo64" flags %06o ovf %o\n", (word36) ((res >> 36) & MASK36), (word36) (res & MASK36), * flags, * ovf); 
#endif

    // Extract the overflow bits
#ifdef NEED_128
    bool r73 = isnonzero_128 (and_128 (res, BIT73));
    bool r72 = isnonzero_128 (and_128 (res, SIGN72));
#else
    bool r73 = res & BIT73 ? true : false;
    bool r72 = res & SIGN72 ? true : false;
#endif

    // Extract the carry bit
#ifdef NEED_128
    bool r74 = isnonzero_128 (and_128 (res, BIT74));
#else
    bool r74 = res & BIT74 ? true : false;
#endif   

    // Truncate the result
#ifdef NEED_128
    res = and_128 (res, MASK72);
#else
    res &= MASK72;
#endif

    // Check for overflow 
    * ovf = r73 ^ r72;

    // Check for carry 
    bool cry = r74;

#ifdef ISOLTS
//if (currentRunningCpuIdx)
//{
////char buf [1024];
////print_int128 (res, buf);
//sim_printf ("res %012"PRIo64"%012"PRIo64"\nr72 %d r73 %d r74 %d ovf %d cry %d\n", ((word36) (res >> 36)) & MASK36, (word36) res & MASK36, r72, r73, r74, * ovf, cry);
//}
#endif

#ifdef PANEL
    if (cry) CPT (cpt2L, 28); // carry
    if (ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN36) CPT (cpt2L, 31); // neg
#endif

    if (flagsToSet & I_CARRY)
      {
        if (cry) // Note inverted logic for subtraction
          CLRF (* flags, I_CARRY);
        else
          SETF (* flags, I_CARRY);
      }
 
    if (chkOVF () && (flagsToSet & I_OFLOW))
      {
        if (* ovf)
          SETF (* flags, I_OFLOW);      // overflow
      }
    
    if (flagsToSet & I_ZERO)
      {
#ifdef NEED_128
        if (isnonzero_128 (res))
#else
        if (res)
#endif
          CLRF (* flags, I_ZERO);
        else
          SETF (* flags, I_ZERO);       // zero result
      }
    
    if (flagsToSet & I_NEG)
      {
#ifdef NEED_128
        if (isnonzero_128 (and_128 (res, SIGN72)))
#else
        if (res & SIGN72)
#endif
          SETF (* flags, I_NEG);
        else
          CLRF (* flags, I_NEG);
      }
    
#ifdef NEED_128
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b res %012"PRIo64"%012"PRIo64" flags %06o ovf %o\n", (word36) (rshift_128 (res, 36).l & MASK36), (word36) (res.l & MASK36), * flags, * ovf); 
#else
    sim_debug (DBG_TRACE, & cpu_dev, "Sub72b res %012"PRIo64"%012"PRIo64" flags %06o ovf %o\n", (word36) ((res >> 36) & MASK36), (word36) (res & MASK36), * flags, * ovf); 
#endif
    return res;
  }

// CANFAULT
word36 compl36(word36 op1, word18 *flags, bool * ovf)
{
    CPT (cpt2L, 23); // compl36
    //printf("op1 = %"PRIo64" %"PRIo64"\n", op1, (-op1) & DMASK);
    
    op1 &= DMASK;
    
    word36 res = -op1 & DMASK;
    
    * ovf = op1 == MAXNEG;

#ifdef PANEL
    if (* ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN36) CPT (cpt2L, 31); // neg
#endif

    if (chkOVF () && * ovf)
        SETF(*flags, I_OFLOW);

    if (res & SIGN36)
        SETF(*flags, I_NEG);
    else
        CLRF(*flags, I_NEG);
    
    if (res == 0)
        SETF(*flags, I_ZERO);
    else
        CLRF(*flags, I_ZERO);
    
    return res;
}

// CANFAULT
word18 compl18(word18 op1, word18 *flags, bool * ovf)
{
    CPT (cpt2L, 24); // compl18
    //printf("op1 = %"PRIo64" %"PRIo64"\n", op1, (-op1) & DMASK);
    
    op1 &= MASK18;
    
    word18 res = -op1 & MASK18;
    
    * ovf = op1 == MAX18NEG;
#ifdef PANEL
    if (* ovf) CPT (cpt2L, 29); // ovf
    if (!res) CPT (cpt2L, 30); // zero
    if (res & SIGN18) CPT (cpt2L, 31); // neg
#endif

    if (chkOVF () && * ovf)
        SETF(*flags, I_OFLOW);
    if (res & SIGN18)
        SETF(*flags, I_NEG);
    else
        CLRF(*flags, I_NEG);
    
    if (res == 0)
        SETF(*flags, I_ZERO);
    else
        CLRF(*flags, I_ZERO);
    
    return res;
}

void copyBytes(int posn, word36 src, word36 *dst)
{
    word36 mask = 0;
    
    if (posn & 8) // bit 30 - byte 0 - (bits 0-8)
        mask |= 0777000000000LL;
   
    if (posn & 4) // bit 31 - byte 1 - (bits 9-17)
        mask |= 0000777000000LL;
    
    if (posn & 2) // bit 32 - byte 2 - (bits 18-26)
        mask |= 0000000777000LL;
    
    if (posn & 1) // bit 33 - byte 3 - (bits 27-35)
        mask |= 0000000000777LL;
         
    word36 byteVals = src & mask;   // get byte bits
    
    // clear the bits in dst
    *dst &= ~mask;
    
    // and set the bits in dst
    *dst |= byteVals;
}


#ifndef QUIET_UNUSED
word9 getByte(int posn, word36 src)
{
    // XXX what's wrong with the macro????
    // XXX NB different parameter order
//    word36 mask = 0;
    
//    switch (posn)
//    {
//        case 0: // byte 0 - (bits 0-8)
//            mask |= 0777000000000LL;
//            break;
//        case 1: // byte 1 - (bits 9-17)
//            mask |= 0000777000000LL;
//            break;
//        case 2: // byte 2 - (bits 18-26)
//            mask |= 0000000777000LL;
//            break;
//        case 3: // byte 3 - (bits 27-35)
//            mask |= 0000000000777LL;
//            break;
//    }
    word9 byteVal = (word9) (src >> (9 * (3 - posn))) & 0777;   ///< get byte bits
    return byteVal;
}
#endif

void copyChars(int posn, word36 src, word36 *dst)
{
    word36 mask = 0;
    
    if (posn & 32) // bit 30 - char 0 - (bits 0-5)
        mask |= 0770000000000LL;
    
    if (posn & 16) // bit 31 - char 1 - (bits 6-11)
        mask |= 0007700000000LL;
    
    if (posn & 8) // bit 32 - char 2 - (bits 12-17)
        mask |= 0000077000000LL;
    
    if (posn & 4) // bit 33 - char 3 - (bits 18-23)
        mask |= 0000000770000LL;
    
    if (posn & 2) // bit 34 - char 4 - (bits 24-29)
        mask |= 0000000007700LL;
    
    if (posn & 1) // bit 35 - char 5 - (bits 30-35)
        mask |= 0000000000077LL;
    
    word36 byteVals = src & mask;   // get byte bits
    
    // clear the bits in dst
    *dst &= ~mask;
    
    // and set the bits in dst
    *dst |= byteVals;
    
}


/*!
 * write 9-bit byte into 36-bit word....
 */
void putByte(word36 *dst, word9 data, int posn)
{
    // XXX which is faster switch() or calculation?
    
//    int offset = 27 - (9 * posn);//    0;
//    switch (posn)
//    {
//        case 0:
//            offset = 27;
//            break;
//        case 1:
//            offset = 18;
//            break;
//        case 2:
//            offset = 9;
//            break;
//        case 3:
//            offset = 0;
//            break;
//    }
    //*dst = bitfieldInsert36(*dst, (word36)data, offset, 9);
    putbits36_9 (dst, (uint) posn * 9, data);
}

void putChar(word36 *dst, word6 data, int posn)
{
    // XXX which is faster switch() or calculation?
    
//    int offset = 30 - (6 * posn);   //0;
//    switch (posn)
//    {
//        case 0:
//            offset = 30;
//            break;
//        case 1:
//            offset = 24;
//            break;
//        case 2:
//            offset = 18;
//            break;
//        case 3:
//            offset = 12;
//            break;
//        case 4:
//            offset = 6;
//            break;
//        case 5:
//            offset = 0;
//            break;
//    }
    //*dst = bitfieldInsert36(*dst, (word36)data, offset, 6);
    putbits36_6 (dst, (uint) posn * 6, data);
}

word72 convertToWord72(word36 even, word36 odd)
{
#ifdef NEED_128
//sim_debug (DBG_TRACE, & cpu_dev, "even %016llx\n", even);
//sim_debug (DBG_TRACE, & cpu_dev, "construct even %016llx %016llx\n", construct_128 (0, even).h, construct_128 (0, even).l);
//sim_debug (DBG_TRACE, & cpu_dev, "shift even %016llx %016llx\n", lshift_128 (construct_128 (0, even), 36).h, lshift_128 (construct_128 (0, even), 36).l);
//sim_debug (DBG_TRACE, & cpu_dev, "odd %016llx\n", odd);
//sim_debug (DBG_TRACE, & cpu_dev, "construct odd %016llx %016llx\n", construct_128 (0, odd).h, construct_128 (0, odd).l);
//sim_debug (DBG_TRACE, & cpu_dev, "or %016llx %016llx\n",
    //or_128 (lshift_128 (construct_128 (0, even), 36), construct_128 (0, odd)).h,
    //or_128 (lshift_128 (construct_128 (0, even), 36), construct_128 (0, odd)).l);
    return or_128 (lshift_128 (construct_128 (0, even), 36), construct_128 (0, odd));
#else
    return ((word72)even << 36) | (word72)odd;
#endif
}

void convertToWord36(word72 src, word36 *even, word36 *odd)
{
#ifdef NEED_128
    *even = rshift_128 (src, 36).l & DMASK;
    *odd = src.l & DMASK;
#else
    *even = (word36)(src >> 36) & DMASK;
    *odd = (word36)src & DMASK;
#endif
}

void cmp36(word36 oP1, word36 oP2, word18 *flags)
  {
    CPT (cpt2L, 25); // cmp36
#ifdef L68
    cpu.ou.cycle |= ou_GOS;
#endif
    t_int64 op1 = SIGNEXT36_64(oP1 & DMASK);
    t_int64 op2 = SIGNEXT36_64(oP2 & DMASK);
    
    word36 sign1 = (word36) op1 & SIGN36;
    word36 sign2 = (word36) op2 & SIGN36;


    if ((! sign1) && sign2)  // op1 > 0, op2 < 0 :: op1 > op2
      CLRF (* flags, I_ZERO | I_NEG | I_CARRY);

    else if (sign1 == sign2) // both operands have the same sogn
      {
         if (op1 > op2)
           {
             CPT (cpt2L, 28); // carry
             SETF (* flags, I_CARRY);
             CLRF (* flags, I_ZERO | I_NEG);
           }
         else if (op1 == op2)
           {
             CPT (cpt2L, 28); // carry
             CPT (cpt2L, 30); // zero
             SETF (* flags, I_ZERO | I_CARRY);
             CLRF (* flags, I_NEG);
           }
         else //  op1 < op2
          {
            CPT (cpt2L, 31); // neg
            SETF (* flags, I_NEG);
            CLRF (* flags, I_ZERO | I_CARRY);
          }
      }
    else // op1 < 0, op2 > 0 :: op1 < op2
      {
        CPT (cpt2L, 28); // carry
        CPT (cpt2L, 31); // neg
        SETF (* flags, I_CARRY | I_NEG);
        CLRF (* flags, I_ZERO);
      }
  }

void cmp18(word18 oP1, word18 oP2, word18 *flags)
  {
    CPT (cpt2L, 26); // cmp18
#ifdef L68
    cpu.ou.cycle |= ou_GOS;
#endif
    int32 op1 = SIGNEXT18_32 (oP1 & MASK18);
    int32 op2 = SIGNEXT18_32 (oP2 & MASK18);

    word18 sign1 = (word18) op1 & SIGN18;
    word18 sign2 = (word18) op2 & SIGN18;


    if ((! sign1) && sign2)  // op1 > 0, op2 < 0 :: op1 > op2
      CLRF (* flags, I_ZERO | I_NEG | I_CARRY);

    else if (sign1 == sign2) // both operands have the same sogn
      {
        if (op1 > op2)
          {
            CPT (cpt2L, 28); // carry
            SETF (* flags, I_CARRY);
            CLRF (* flags, I_ZERO | I_NEG);
          }
        else if (op1 == op2)
          {
            CPT (cpt2L, 28); // carry
            CPT (cpt2L, 30); // zero
            SETF (* flags, I_ZERO | I_CARRY);
            CLRF (* flags, I_NEG);
          }
        else //  op1 < op2
          {
            CPT (cpt2L, 31); // neg
            SETF (* flags, I_NEG);
            CLRF (* flags, I_ZERO | I_CARRY);
          }
      }
    else // op1 < 0, op2 > 0 :: op1 < op2
      {
        CPT (cpt2L, 28); // carry
        CPT (cpt2L, 31); // neg
        SETF (* flags, I_CARRY | I_NEG);
        CLRF (* flags, I_ZERO);
      }
  }

void cmp36wl(word36 A, word36 Y, word36 Q, word18 *flags)
{
    CPT (cpt2L, 26); // cmp36wl
    // This is wrong; signed math is needed.

    //bool Z = (A <= Y && Y <= Q) || (A >= Y && Y >= Q);

#ifdef L68
    cpu.ou.cycle |= ou_GOS;
#endif
    t_int64 As = (word36s) SIGNEXT36_64(A & DMASK);
    t_int64 Ys = (word36s) SIGNEXT36_64(Y & DMASK);
    t_int64 Qs = (word36s) SIGNEXT36_64(Q & DMASK);
    bool Z = (As <= Ys && Ys <= Qs) || (As >= Ys && Ys >= Qs);

    SCF(Z, *flags, I_ZERO);
    
    if (!(Q & SIGN36) && (Y & SIGN36) && (Qs > Ys))
        CLRF(*flags, I_NEG | I_CARRY);
    else if (((Q & SIGN36) == (Y & SIGN36)) && (Qs >= Ys))
    {
        CPT (cpt2L, 28); // carry
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_NEG);
    } else if (((Q & SIGN36) == (Y & SIGN36)) && (Qs < Ys))
    {
        CPT (cpt2L, 31); // neg
        CLRF(*flags, I_CARRY);
        SETF(*flags, I_NEG);
    } else if ((Q & SIGN36) && !(Y & SIGN36) && (Qs < Ys))
    {
        CPT (cpt2L, 28); // carry
        CPT (cpt2L, 31); // neg
        SETF(*flags, I_NEG | I_CARRY);
    }
}

void cmp72(word72 op1, word72 op2, word18 *flags)
{
    CPT (cpt2L, 27); // cmp72
   // The case of op1 == 400000000000000000000000 and op2 == 0 falls through
   // this code.
#if 0
    if (!(op1 & SIGN72) && (op2 & SIGN72) && (op1 > op2))
        CLRF(*flags, I_ZERO | I_NEG | I_CARRY);
    else if ((op1 & SIGN72) == (op2 & SIGN72) && (op1 > op2))
    {
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_ZERO | I_NEG);
    } else if (((op1 & SIGN72) == (op2 & SIGN72)) && (op1 == op2))
    {
        SETF(*flags, I_ZERO | I_CARRY);
        CLRF(*flags, I_NEG);
    } else if (((op1 & SIGN72) == (op2 & SIGN72)) && (op1 < op2))
    {
        SETF(*flags, I_NEG);
        CLRF(*flags, I_ZERO | I_CARRY);
    } else if (((op1 & SIGN72) && !(op2 & SIGN72)) && (op1 < op2))
    {
        SETF(*flags, I_CARRY | I_NEG);
        CLRF(*flags, I_ZERO);
    }
#else
#ifdef L68
    cpu.ou.cycle |= ou_GOS;
#endif
#ifdef NEED_128
sim_debug (DBG_TRACE, & cpu_dev, "op1 %016llx%016llx\n", op1.h, op1.l);
sim_debug (DBG_TRACE, & cpu_dev, "op2 %016llx%016llx\n", op2.h, op2.l);
    int128 op1s =  SIGNEXT72_128 (and_128 (op1, MASK72));
    int128 op2s =  SIGNEXT72_128 (and_128 (op2, MASK72));
sim_debug (DBG_TRACE, & cpu_dev, "op1s %016llx%016llx\n", op1s.h, op1s.l);
sim_debug (DBG_TRACE, & cpu_dev, "op2s %016llx%016llx\n", op2s.h, op2s.l);
#else
sim_debug (DBG_TRACE, & cpu_dev, "op1 %016lx%016lx\n", (uint64_t) (op1>>64), (uint64_t) op1);
sim_debug (DBG_TRACE, & cpu_dev, "op2 %016lx%016lx\n", (uint64_t) (op2>>64), (uint64_t) op2);
    int128 op1s =  SIGNEXT72_128 (op1 & MASK72);
    int128 op2s =  SIGNEXT72_128 (op2 & MASK72);
sim_debug (DBG_TRACE, & cpu_dev, "op1s %016lx%016lx\n", (uint64_t) (op1s>>64), (uint64_t) op1s);
sim_debug (DBG_TRACE, & cpu_dev, "op2s %016lx%016lx\n", (uint64_t) (op2s>>64), (uint64_t) op2s);
#endif
#ifdef NEED_128
    if (isgt_s128 (op1s, op2s))
#else
    if (op1s > op2s)
#endif
      {
#ifdef NEED_128
        if (isnonzero_128 (and_128 (op2, SIGN72)))
#else
        if (op2 & SIGN72)
#endif
          CLRF (* flags, I_CARRY);
        else
          {
            CPT (cpt2L, 28); // carry
            SETF (* flags, I_CARRY);
          }
        CLRF (* flags, I_ZERO | I_NEG);
      }
#ifdef NEED_128
    else if (iseq_128 (cast_128 (op1s), cast_128 (op2s)))
#else
    else if (op1s == op2s)
#endif
      {
        CPT (cpt2L, 28); // carry
        CPT (cpt2L, 30); // zero
        SETF (* flags, I_CARRY | I_ZERO);
        CLRF (* flags, I_NEG);
      }
    else /* op1s < op2s */
      {
        CPT (cpt2L, 31); // neg
#ifdef NEED_128
        if (isnonzero_128 (and_128 (op1, SIGN72)))
#else
        if (op1 & SIGN72)
#endif
          {
            CPT (cpt2L, 28); // carry
            SETF (* flags, I_CARRY);
          }
        else
          CLRF (* flags, I_CARRY);
        CLRF (* flags, I_ZERO);
        SETF (* flags, I_NEG);
      }


#endif
}

/*
 * String utilities ...
 */

/* ------------------------------------------------------------------------- */

char * strlower(char *q)
{
        char *s = q;
    
        while (*s) {
                if (isupper(*s))
                        *s = (char) tolower(*s);
                s++;
        }
        return q;
}

/* ------------------------------------------------------------------------- */

/*  state definitions  */
#define STAR    0
#define NOTSTAR 1
#define RESET   2

int strmask (char * str, char * mask)
/*!
 Tests string 'str' against mask string 'mask'
 Returns TRUE if the string matches the mask.
 
 The mask can contain '?' and '*' wild card characters.
 '?' matches any        single character.
 '*' matches any number of any characters.
 
 For example:
 strmask("Hello", "Hello");     ---> TRUE
 strmask("Hello", "Jello");     ---> FALSE
 strmask("Hello", "H*o");       ---> TRUE
 strmask("Hello", "H*g");       ---> FALSE
 strmask("Hello", "?ello");     ---> TRUE
 strmask("Hello", "H????");     ---> TRUE
 strmask("H", "H????");         ---> FALSE
 */
  {
    char * sp, * mp, * reset_string, * reset_mask, * sn;
    int state;
    
    sp = str;
    mp = mask;
    
    while (1)
      {
        switch (* mp)
          {
            case '\0':
              return * sp ? false : true;

            case '?':
              sp ++;
              mp ++;
              break;

            default:
              if (* mp == * sp)
                {
                  sp ++;
                  mp ++;
                  break;
                }
              else
                {
                  return false;
                }

            case '*':
              if (* (mp + 1) == '\0')
                {
                  return true;
                }
              if ((sn = strchr (sp, * (mp + 1))) == NULL)
                {
                  return false;
                }
                
              /* save place -- match rest of string */
              /* if fail, reset to here */
              reset_mask = mp;
              reset_string = sn + 1;
                
              mp = mp + 2;
              sp = sn + 1;
              state = NOTSTAR;
              while (state == NOTSTAR)
                {
                  switch (* mp)
                    {
                      case '\0':
                        if (* sp == '\0')
                          return false;
                        else
                          state = RESET;
                        break;
                      case '?':
                        sp ++;
                        mp ++;
                        break;
                      default:
                        if (* mp == * sp)
                          {
                            sp ++;
                            mp ++;
                          }
                        else
                          state = RESET;
                        break;
                      case '*':
                        state = STAR;
                        break;
                    }
                } // while STATE == NOTSTAR
              /* we've reach a new star or should reset to last star */
              if (state == RESET)
                {
                  sp = reset_string;
                  mp = reset_mask;
                }
              break;
          } // switch (* mp)
      } // while (1)
  }

/**
 * strtok() with string quoting...
 * (implemented as a small fsm, kinda...
 * (add support for embedded " later, much later...)
 */
#define NORMAL 		1
#define IN_STRING	2
#define EOB			3

char *
Strtok(char *line, char *sep)
{
    
    static char *p;		/*!< current pointer position in input line	*/
    static int state = NORMAL;
    
    char *q;			/*!< beginning of current field			*/
    
    if (line) {			/* 1st invocation						*/
        p = line;
        state = NORMAL;
    }
    
    q = p;
    while (state != EOB) {
        switch (state) {
            case NORMAL:
                switch (*p) {
                    case 0:				///< at end of buffer
                        state = EOB;	// set state to "end Of Buffer
                        return q;
                        
                    case '"':		///< beginning of a quoted string
                        state = IN_STRING;	// we're in a string
                        p++;
                        continue;
                        
                    default:    ///< only a few special characters
                        if (strchr(sep, *p) == NULL) {	// not a sep
                            p++;				// goto next char
                            continue;
                        } else {
                            *p++ = (char)0;	/* ... iff >0	*/
                            while (*p && strchr(sep, *p))	/* skip over seperator(s)*/
                                p++;
                            return q;	/* return field		*/
                        }
                }
                
            case IN_STRING:
                if (*p == 0) {		  /*!< incomplete quoted string	*/
                    state = EOB;
                    return q;
                }
                
                if (*p != '"') { // not end of line and still in a string
                    p++;
                    continue;
                }
                state = NORMAL;			/* end of quoted string	*/
                p++;
                
                continue;
                
            case EOB:					/*!< just in case	*/
                state = NORMAL;
                return NULL;
                
            default:
                fprintf(stderr, "(Strtok):unknown state - %d",state);
                state = EOB;
                return NULL;
        }
        
    }
    
    return NULL;		/* no more fields in buffer		*/
    
}
#if 0
bool startsWith(const char *str, const char *pre)
{
    size_t lenpre = strlen(pre),
    lenstr = strlen(str);
    return lenstr < lenpre ? false : strncasecmp(pre, str, lenpre) == 0;
}
#endif

/**
 * Removes the trailing spaces from a string.
 */
char *rtrim(char *s)
{
    if (! s)
      return s;
    int index;
    
    //for (index = (int)strlen(s) - 1; index >= 0 && (s[index] == ' ' || s[index] == '\t'); index--)
    for (index = (int)strlen(s) - 1; index >= 0 && isspace(s[index]); index--)
    {
        s[index] = '\0';
    }
    return(s);
}
/** ------------------------------------------------------------------------- */
char *ltrim(char *s)
/**
 *	Removes the leading spaces from a string.
 */
{
    char *p;
    if (s == NULL)
        return NULL;
    
    //for (p = s; (*p == ' ' || *p == '\t') && *p != '\0'; p++)
    for (p = s; isspace(*p) && *p != '\0'; p++)
        ;
    
    //strcpy(s, p);
    memmove(s, p, strlen(p) + 1);
    return(s);
}

/** ------------------------------------------------------------------------- */

char *trim(char *s)
{
    return ltrim(rtrim(s));
}


char *
stripquotes(char *s)
{
    if (! s || ! *s)
        return s;
    /*
     char *p;
     
     while ((p = strchr(s, '"')))
     *p = ' ';
     strchop(s);
     
     return s;
     */
    int nLast = (int)strlen(s) - 1;
    // trim away leading/trailing "'s
    if (s[0] == '"')
        s[0] = ' ';
    if (s[nLast] == '"')
        s[nLast] = ' ';
    return trim(s);
}

#if 0
/*!
 a - Bitfield to insert bits into.
 b - Bit pattern to insert.
 c - Bit offset number.
 d = Number of bits to insert.
 
 Description
 
 Returns the result of inserting bits B at offset C of length D in the bitfield A.
 */
word72 bitfieldInsert72(word72 a, word72 b, int c, int d)
{
    word72 mask = ~((word72)-1 << d) << c;
    mask = ~mask;
    a &= mask;
    return a | (b << c);
}
#endif

#if 0
/*!
 a - Bitfield to insert bits into.
 b - Bit pattern to insert.
 c - Bit offset number.
 d = Number of bits to insert.
 
 Description
 
 Returns the result of inserting bits B at offset C of length D in the bitfield A.
 
 XXX: c & d should've been expressed in dps8 big-endian rather than little-endian numbering. Oh, well.
 
 */
word36 bitfieldInsert36(word36 a, word36 b, int c, int d)
{
    word36 mask = ~(0xffffffffffffffffLL << d) << c;
    mask = ~mask;
    a &= mask;
    return a | (b << c);
}
#endif

#if 0
/*!
a - Bitfield to insert bits into.
b - Bit pattern to insert.
c - Bit offset number.
d = Number of bits to insert.
 
 Description
 
 Returns the result of inserting bits B at offset C of length D in the bitfield A.
*/
int bitfieldInsert(int a, int b, int c, int d)
{
    uint32 mask = ~(0xffffffff << d) << c;
    mask = ~mask;
    a &= mask;
    return a | (b << c);
}
#endif

#if 0
/*!
 a -  Bitfield to extract bits from.
 b -  Bit offset number. Bit offsets start at 0.
 c - Number of bits to extract.
 
 Description
 
 Returns bits from offset b of length c in the bitfield a.
 */
int bitfieldExtract(int a, int b, int c)
{
    int mask = ~((int)0xffffffff << c);
    if (b > 0)
        return (a >> b) & mask; // original pseudocode had b-1
    else
        return a & mask;
}
#endif

#if 0
/*!
 a -  Bitfield to extract bits from.
 b -  Bit offset number. Bit offsets start at 0.
 c - Number of bits to extract.
 
 Description
 
 Returns bits from offset b of length c in the bitfield a.
 NB: This would've been much easier to use of I changed, 'c', the bit offset to reflect the dps8s 36bit word!! Oh, well.

 */
word36 bitfieldExtract36(word36 a, int b, int c)
{
    word36 mask = ~(0xffffffffffffffffLL  << c);
    //printf("mask=%012"PRIo64"\n", mask);
    if (b > 0)
        return (a >> b) & mask; // original pseudocode had b-1
    else
        return a & mask;
}
#endif

#if 0
word72 bitfieldExtract72(word72 a, int b, int c)
{
    word72 mask = ~((word72)-1 << c);
    if (b > 0)
        return (a >> b) & mask; // original pseudocode had b-1
    else
        return a & mask;
}
#endif

#ifndef QUIET_UNUSED
/*!
 @param[in] x Bitfield to count bits in.
 
 \brief Returns the count of set bits (value of 1) in the bitfield x.
 */
int bitCount(int x)
{
    int i;
    int res = 0;
    for(i = 0; i < 32; i++) {
        uint32 mask = 1 << i;
        if (x & (int) mask)
            res ++;
    }
    return res;
}
#endif 

#ifndef QUIET_UNUSED
/*!
 @param[in] x Bitfield to find LSB in.
 
 \brief Returns the bit number of the least significant bit (value of 1) in the bitfield x. If no bits have the value 1 then -1 is returned.
 */
int findLSB(int x)
{
    int i;
    int mask;
    int res = -1;
    for(i = 0; i < 32; i++) {
        mask = 1 << i;
        if (x & mask) {
            res = i;
            break;
        }
    }
    return res;
}
#endif


#ifndef QUIET_UNUSED
/*!
 @param[in] x  Bitfield to find MSB in.
 
 \brief Returns the bit number of the most significant bit (value of 1) in the bitfield x. If the number is negative then the position of the first zero bit is returned. If no bits have the value 1 (or 0 in the negative case) then -1 is returned.

 from http://http.developer.nvidia.com/Cg/findMSB.html

 NB: the above site provides buggy "pseudocode". >sheesh<
 
 */
int findMSB(int x)
{
    int i;
    int mask;
    int res = -1;
    if (x < 0) x = ~x;
    for(i = 0; i < 32; i++) {
        mask = (int) 0x80000000 >> i;
        if (x & mask) {
            res = 31 - i;
            break;
        }
    }
    return res;
}
#endif

#ifndef QUIET_UNUSED
/*!
 @param[in] x Bitfield to reverse.
 
 \brief Returns the reverse of the bitfield x.
 */
int bitfieldReverse(int x)
{
    int res = 0;
    int i, shift, mask;
    
    for(i = 0; i < 32; i++) {
        mask = 1 << i;
        shift = 32 - 2*i - 1;
        mask &= x;
        mask = (shift > 0) ? mask << shift : mask >> -shift;
        res |= mask;
    }
    
    return res;
}
#endif


#if 0
/*
 * getbits36()
 *
 * Extract a range of bits from a 36-bit word.
 */

word36 getbits36(word36 x, uint i, uint n) {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)n+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36: bad args (%012"PRIo64",i=%d,n=%d)\n", x, i, n);
        return 0;
    } else
        return (x >> (unsigned) shift) & ~ (~0U << n);
}

// ============================================================================

/*
 * setbits36()
 *
 * Set a range of bits in a 36-bit word -- Returned value is x with n bits
 * starting at p set to the n lowest bits of val
 */

word36 setbits36(word36 x, uint p, uint n, word36 val)
{
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("setbits36: bad args (%012"PRIo64",pos=%d,n=%d)\n", x, p, n);
        return 0;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    word36 result = (x & ~ mask) | ((val&MASKBITS(n)) << (36 - p - n));
    return result;
}


// ============================================================================

/*
 * putbits36()
 *
 * Set a range of bits in a 36-bit word -- Sets the bits in the argument,
 * starting at p set to the n lowest bits of val
 */

void putbits36 (word36 * x, uint p, uint n, word36 val)
  {
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35)
      {
        sim_printf ("putbits36: bad args (%012"PRIo64",pos=%d,n=%d)\n", * x, p, n);
        return;
      }
    word36 mask = ~ (~0U << n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    * x = (* x & ~mask) | ((val & MASKBITS (n)) << (36 - p - n));
    return;
  }
#endif

/*
 * bin2text()
 *
 * Display as bit string.
 * WARNING: returns pointer of two alternating static buffers
 *
 */

#include <ctype.h>

char *bin2text(uint64 word, int n)
{
    // WARNING: static buffer
    static char str1[65];
    static char str2[65];
    static char *str = NULL;
    if (str == NULL)
        str = str1;
    else if (str == str1)
        str = str2;
    else
        str = str1;
    str[n] = 0;
    int i;
    for (i = 0; i < n; ++ i) {
        str[n-i-1] = ((word % 2) == 1) ? '1' : '0';
        word >>= 1;
    }
    return str;
}

#include <ctype.h>

// No longer putting the tty in "no output proceesing mode"; all of this
// is irrelevant...
//
// simh puts the tty in raw mode when the sim is running;
// this means that test output to the console will lack CR's and
// be all messed up.
//
// There are three ways around this:
//   1: switch back to cooked mode for the message
//   2: walk the message and output CRs before LFs with sim_putchar()
//   3: walk the message and output CRs before LFs with sim_os_putchar()
//
// The overhead and obscure side effects of switching are unknown.
//
// sim_putchar adds the text to the log file, but not the debug file; and
// sim_putchar puts the CRs into the log file.
//
// sim_os_putchar skips checks that sim_putchar does that verify that
// we are actually talking to tty, instead of a telnet connection or other
// non-tty thingy.
//
// USE_COOKED does the wrong thing when stdout is piped; ie not a terminal

//#define USE_COOKED
#define USE_PUTCHAR
//#define USE_NONE

#if 0
void sim_printf( const char * format, ... )
{
    char buffer[4096];
    
    va_list args;
    va_start (args, format);
    vsnprintf (buffer, sizeof(buffer), format, args);
    
#ifdef USE_COOKED
    if (sim_is_running)
      sim_ttcmd ();
#endif

    for(uint i = 0 ; i < sizeof(buffer); i += 1)
    {
        if (buffer[i]) {
#ifdef USE_PUTCHAR
            if (sim_is_running && buffer [i] == '\n')
              sim_putchar ('\r');
#endif
#ifdef USE_OS_PUTCHAR
            if (sim_is_running && buffer [i] == '\n')
              sim_os_putchar ('\r');
#endif
            sim_putchar(buffer[i]);
            //if (sim_deb)
              //fputc (buffer [i], sim_deb);
        } else
            break;
    }
 
#ifdef USE_COOKED
    if (sim_is_running)
      sim_ttrun ();
#endif

    va_end (args);
}
#endif

#if 0
// Rework sim_printf.
//
// Distinguish between the console device and the window in which dps8 is
// running.
//
// There are (up to) three outputs:
//
//   - the window in which dps8 is running (stdout)
//   - the log file (which may be stdout or stderr)
//   - the console device
//
// sim_debug --
//   prints time stamped strings to the logfile; controlled by scp commands.
// sim_err --
//   prints sim_debug style messages regardless of scp commands and returns
//   to scp; not necessarily restartable.
// sim_printf --
//   prints strings to logfile and stdout
// sim_printl --
//   prints strings to console logfile 
// sim_putchar/sim_os_putchar/sim_puts
//   prints char/string to the console

void sim_printf (const char * format, ...)
  {
    char buffer [4096];
    bool bOut = (sim_deb ? fileno (sim_deb) != fileno (stdout) : false);

    va_list args;
    va_start (args, format);
    vsnprintf (buffer, sizeof (buffer), format, args);
    va_end (args);
    
#ifdef USE_COOKED
    if (sim_is_running)
      sim_ttcmd ();
#endif

    for (uint i = 0 ; i < sizeof (buffer); i ++)
      {
        if (! buffer [i])
          break;

        // stdout

#ifdef USE_PUTCHAR
        if (sim_is_running && buffer [i] == '\n')
          {
            putchar ('\r');
          }
#endif
#ifdef USE_OS_PUTCHAR
        if (sim_is_running && buffer [i] == '\n')
          sim_os_putchar ('\r');
#endif
        putchar (buffer [i]);

        // logfile

        if (bOut)
          {
            //if (sim_is_running && buffer [i] == '\n')
              //fputc  ('\r', sim_deb);
            fputc (buffer [i], sim_deb);
          }
    }

#ifdef USE_COOKED
    if (sim_is_running)
      sim_ttrun ();
#endif

    fflush (sim_deb);
    if (bOut)
      fflush (stdout);
}
#endif

void sim_printl (const char * format, ...)
  {
    if (! sim_log)
      return;
    char buffer [4096];

    va_list args;
    va_start (args, format);
    vsnprintf (buffer, sizeof (buffer), format, args);
    
    for (uint i = 0 ; i < sizeof (buffer); i ++)
      {
        if (! buffer [i])
          break;

        // logfile

        fputc (buffer [i], sim_log);
      }
    va_end (args);
    fflush (sim_log);
}

void sim_puts (char * str)
  {
    char * p = str;
    while (* p)
      sim_putchar (* (p ++));
  }

// XXX what about config=addr7=123, where clist has a "addr%"?

// return -2: error; -1: done; >= 0 option found
int cfgparse (const char * tag, const char * cptr, config_list_t * clist, config_state_t * state, int64_t * result)
  {
    if (! cptr)
      return -2;
    char * start = NULL;
    if (! state -> copy)
      {
        state -> copy = strdup (cptr);
        start = state -> copy;
        state ->  statement_save = NULL;
      }

    int ret = -2; // error

    // grab every thing up to the next semicolon
    char * statement;
    statement = strtok_r (start, ";", & state -> statement_save);
    start = NULL;
    if (! statement)
      {
        ret = -1; // done
        goto done;
      }

    // extract name
    char * name_start = statement;
    char * name_save = NULL;
    char * name;
    name = strtok_r (name_start, "=", & name_save);
    if (! name)
      {
        sim_printf ("error: %s: can't parse name\n", tag);
        goto done;
      }

    // lookup name
    config_list_t * p = clist;
    while (p -> name)
      {
        if (strcasecmp (name, p -> name) == 0)
          break;
        p ++;
      }
    if (! p -> name)
      {
        sim_printf ("error: %s: don't know name <%s>\n", tag, name);
        goto done;
      }

    // extract value
    char * value;
    value = strtok_r (NULL, "", & name_save);
    if (! value)
      {
        // Special case; min>max and no value list
        // means that a missing value is ok
        if (p -> min > p -> max && ! p -> value_list)
          {
            return (int) (p - clist);
          }
        sim_printf ("error: %s: can't parse value\n", tag);
        goto done;
      }

    // first look to value in the value list
    config_value_list_t * v = p -> value_list;
    if (v)
      {
        while (v -> value_name)
          {
            if (strcasecmp (value, v -> value_name) == 0)
              break;
            v ++;
          }

        // Hit?
        if (v -> value_name)
          {
            * result = v -> value;
            return (int) (p - clist);
          }
      }

    // Must be a number

    if (p -> min > p -> max)
      {
        sim_printf ("error: %s: can't parse value\n", tag);
        goto done;
      }

    if (strlen (value) == 0)
      {
         sim_printf ("error: %s: missing value\n", tag);
         goto done;
      }
    char * endptr;
    int64_t n = strtoll (value, & endptr, 0);
    if (* endptr)
      {
        sim_printf ("error: %s: can't parse value\n", tag);
        goto done;
      } 

// XXX small bug; doesn't check for junk after number...
    if (n < p -> min || n > p -> max)
      {
        sim_printf ("error: %s: value out of range\n", tag);
        goto done;
      } 
    
    * result = n;
    return (int) (p - clist);

done:
    free (state -> copy);
    state -> copy= NULL;
    return ret;
  }

void cfgparse_done (config_state_t * state)
  {
    if (state -> copy)
      free (state -> copy);
    state -> copy = NULL;
  }

// strdup with limited C-style escape processing
//
//  strdupesc ("foo\nbar") --> 'f' 'o' 'o' 012 'b' 'a' 'r'
//
//  Handles:
//   \\  backslash
//   \n  newline
//   \t  tab
//   \f  formfeed
//   \r  carrriage return
//
// \\ doesn't seem to work...
//  Also, a simh specific:
//
//   \e  (end simulation)
//
//  the simh parser doesn't handle these very well...
//
//   \_  space
//   \c  comma
//   \s  semicolon
//   \d  dollar
//   \q  double quote
//   \w  <backslash>
//   \z  ^Z
//
// And a special case:
//
//   \TZ replaced with the timezone string. Three characters are used
//       to allow for space in the buffer. 
//
//  all others silently ignored and left unprocessed
//

char * strdupesc (const char * str)
  {
    char * buf = strdup (str);
    char * p = buf;
    while (* p)
      {
        if (* p != '\\')
          {
            p ++;
            continue;
          }
        if (p [1] == '\\')           //   \\    backslash
          * p = '\\';
        else if (p [1] == 'w')       //   \w    backslash
          * p = '\\';
        else if (p [1] == 'n')       //   \n    newline
          * p = '\n';
        else if (p [1] == 't')       //  \t    tab
          * p = '\t';
        else if (p [1] == 'f')       //  \f    formfeed
          * p = '\f';
        else if (p [1] == 'r')       //  \r    carriage return
          * p = '\r';
        else if (p [1] == 'e')       //  \e    control E; Multics escape char.
          * p = '\005';
        else if (p [1] == '_')       //  \_    space; needed for leading or 
                                     //        trailing spaces (simh parser 
                                     //        issue)
          * p = ' ';
        else if (p [1] == 'c')       //  \c    comma (simh parser issue)
          * p = ',';
        else if (p [1] == 's')       //  \s    semicolon (simh parser issue)
          * p = ';';
        else if (p [1] == 'd')       //  \d    dollar sign (simh parser issue)
          * p = '$';
        else if (p [1] == 'q')       //  \q    double quote (simh parser issue)
          * p = '"';
        else if (p [1] == 'z')       //  \z    ^D  eof (VAXism)
          * p = '\004';
        else if (p [1] == 'k')       //  \k    caret
          * p = '^';

#if 0
        else if (p [1] == 'T' && p [2] == 'Z')  // \TZ   time zone
          {
            strncpy (p, "pst", 3);
            time_t t = time (NULL);
            struct tm * lt = localtime (& t);
            if (strlen (lt -> tm_zone) == 3)
              {
                //strncpy (p, lt -> tm_zone, 3);
                p [0] = tolower (lt -> tm_zone [0]);
                p [1] = tolower (lt -> tm_zone [1]);
                p [2] = tolower (lt -> tm_zone [2]);
              }
            p += 2;
          }
#endif
        else
          {
            p ++;
            continue;
          }
        p ++;
//sim_printf ("was <%s>\n", buf);
        memmove (p, p + 1, strlen (p + 1) + 1);
//sim_printf ("is  <%s>\n", buf);
      }
    return buf;
  }



// Layout of data as read from simh tape format
//
//   bits: buffer of bits from a simh tape. The data is
//   packed as 2 36 bit words in 9 eight bit bytes (2 * 36 == 7 * 9)
//   The of the bytes in bits is
//      byte     value
//       0       most significant byte in word 0
//       1       2nd msb in word 0
//       2       3rd msb in word 0
//       3       4th msb in word 0
//       4       upper half is 4 least significant bits in word 0
//               lower half is 4 most significant bit in word 1
//       5       5th to 13th most signicant bits in word 1
//       6       ...
//       7       ...
//       8       least significant byte in word 1
//

// Multics humor: this is idiotic


// Data conversion routines
//
//  'bits' is the packed bit stream read from the simh tape
//    it is assumed to start at an even word36 address
//
//   extr36
//     extract the word36 at woffset
//

static word36 extrASCII36 (uint8 * bits, uint woffset)
  {
    uint8 * p = bits + woffset * 4;

    uint64 w;
    w  = ((uint64) p [0]) << 27;
    w |= ((uint64) p [1]) << 18;
    w |= ((uint64) p [2]) << 9;
    w |= ((uint64) p [3]);
    // mask shouldn't be neccessary but is robust
    return (word36) (w & MASK36);
  }

// Data conversion routines
//
//  'bits' is the packed bit stream read from the simh tape
//    it is assumed to start at an even word36 address
//
//   extr36
//     extract the word36 at woffset
//

word36 extr36 (uint8 * bits, uint woffset)
  {
    uint isOdd = woffset % 2;
    uint dwoffset = woffset / 2;
    uint8 * p = bits + dwoffset * 9;

    uint64 w;
    if (isOdd)
      {
        w  = (((uint64) p [4]) & 0xf) << 32;
        w |=  ((uint64) p [5]) << 24;
        w |=  ((uint64) p [6]) << 16;
        w |=  ((uint64) p [7]) << 8;
        w |=  ((uint64) p [8]);
      }
    else
      {
        w  =  ((uint64) p [0]) << 28;
        w |=  ((uint64) p [1]) << 20;
        w |=  ((uint64) p [2]) << 12;
        w |=  ((uint64) p [3]) << 4;
        w |= (((uint64) p [4]) >> 4) & 0xf;
      }
    // mask shouldn't be neccessary but is robust
    return (word36) (w & MASK36);
  }

static void putASCII36 (word36 val, uint8 * bits, uint woffset)
  {
    uint8 * p = bits + woffset * 4;
    p [0]  = (val >> 27) & 0xff;
    p [1]  = (val >> 18) & 0xff;
    p [2]  = (val >>  9) & 0xff;
    p [3]  = (val      ) & 0xff;
  }

void put36 (word36 val, uint8 * bits, uint woffset)
  {
    uint isOdd = woffset % 2;
    uint dwoffset = woffset / 2;
    uint8 * p = bits + dwoffset * 9;

    if (isOdd)
      {
        p [4] &=               0xf0;
        p [4] |= (val >> 32) & 0x0f;
        p [5]  = (val >> 24) & 0xff;
        p [6]  = (val >> 16) & 0xff;
        p [7]  = (val >>  8) & 0xff;
        p [8]  = (val >>  0) & 0xff;
        //w  = ((uint64) (p [4] & 0xf)) << 32;
        //w |=  (uint64) (p [5]) << 24;
        //w |=  (uint64) (p [6]) << 16;
        //w |=  (uint64) (p [7]) << 8;
        //w |=  (uint64) (p [8]);
      }
    else
      {
        p [0]  = (val >> 28) & 0xff;
        p [1]  = (val >> 20) & 0xff;
        p [2]  = (val >> 12) & 0xff;
        p [3]  = (val >>  4) & 0xff;
        p [4] &=               0x0f;
        p [4] |= (val <<  4) & 0xf0;
        //w  =  (uint64) (p [0]) << 28;
        //w |=  (uint64) (p [1]) << 20;
        //w |=  (uint64) (p [2]) << 12;
        //w |=  (uint64) (p [3]) << 4;
        //w |= ((uint64) (p [4]) >> 4) & 0xf;
      }
    // mask shouldn't be neccessary but is robust
  }

#ifndef QUIET_UNUSED
//
//   extr9
//     extract the word9 at coffset
//
//   | 012345678 | 012345678 |012345678 | 012345678 | 012345678 | 012345678 | 012345678 | 012345678 |
//     0       1          2         3          4          5          6          7          8
//     012345670   123456701  234567012   345670123   456701234   567012345   670123456   701234567  
//

word9 extr9 (uint8 * bits, uint coffset)
  {
    uint charNum = coffset % 8;
    uint dwoffset = coffset / 8;
    uint8 * p = bits + dwoffset * 9;

    word9 w;
    switch (charNum)
      {
        case 0:
          w = ((((word9) p [0]) << 1) & 0776) | ((((word9) p [1]) >> 7) & 0001);
          break;
        case 1:
          w = ((((word9) p [1]) << 2) & 0774) | ((((word9) p [2]) >> 6) & 0003);
          break;
        case 2:
          w = ((((word9) p [2]) << 3) & 0770) | ((((word9) p [3]) >> 5) & 0007);
          break;
        case 3:
          w = ((((word9) p [3]) << 4) & 0760) | ((((word9) p [4]) >> 4) & 0017);
          break;
        case 4:
          w = ((((word9) p [4]) << 5) & 0740) | ((((word9) p [5]) >> 3) & 0037);
          break;
        case 5:
          w = ((((word9) p [5]) << 6) & 0700) | ((((word9) p [6]) >> 2) & 0077);
          break;
        case 6:
          w = ((((word9) p [6]) << 7) & 0600) | ((((word9) p [7]) >> 1) & 0177);
          break;
        case 7:
          w = ((((word9) p [7]) << 8) & 0400) | ((((word9) p [8]) >> 0) & 0377);
          break;
      }
    // mask shouldn't be neccessary but is robust
    return w & 0777U;
  }
#endif

#ifndef QUIET_UNUSED
//
//   extr18
//     extract the word18 at coffset
//
//   |           11111111 |           11111111 |           11111111 |           11111111 |
//   | 012345678901234567 | 012345678901234567 | 012345678901234567 | 012345678901234567 |
//
//     0       1       2          3       4          5       6          7       8
//     012345670123456701   234567012345670123   456701234567012345   670123456701234567  
//
//     000000001111111122   222222333333334444   444455555555666666   667777777788888888
//
//       0  0  0  0  0  0     0  0  0  0  0  0     0  0  0  0  0  0     0  0  0  0  0  0
//       7  7  6  0  0  0     7  7  0  0  0  0     7  4  0  0  0  0     6  0  0  0  0  0
//       0  0  1  7  7  4     0  0  7  7  6  0     0  3  7  7  0  0     1  7  7  4  0  0
//       0  0  0  0  0  3     0  0  0  0  1  7     0  0  0  0  7  7     0  0  0  3  7  7

word18 extr18 (uint8 * bits, uint boffset)
  {
    uint byteNum = boffset % 4;
    uint dwoffset = boffset / 4;
    uint8 * p = bits + dwoffset * 18;

    word18 w;
    switch (byteNum)
      {
        case 0:
          w = ((((word18) p [0]) << 10) & 0776000) | ((((word18) p [1]) << 2) & 0001774) | ((((word18) p [2]) >> 6) & 0000003);
          break;
        case 1:
          w = ((((word18) p [2]) << 12) & 0770000) | ((((word18) p [3]) << 4) & 0007760) | ((((word18) p [4]) >> 4) & 0000017);
          break;
        case 2:
          w = ((((word18) p [4]) << 14) & 0740000) | ((((word18) p [5]) << 6) & 0037700) | ((((word18) p [6]) >> 2) & 0000077);
          break;
        case 3:
          w = ((((word18) p [6]) << 16) & 0600000) | ((((word18) p [7]) << 8) & 0177400) | ((((word18) p [8]) >> 0) & 0000377);
          break;
      }
    // mask shouldn't be neccessary but is robust
    return w & 0777777U;
  }
#endif

//
//  getbit
//     Get a single bit. offset can be bigger when word size
//

uint8 getbit (void * bits, int offset)
  {
    unsigned int offsetInWord = (uint) offset % 36;
    unsigned int revOffsetInWord = 35 - offsetInWord;
    unsigned int offsetToStartOfWord = (uint) offset - offsetInWord;
    unsigned int revOffset = offsetToStartOfWord + revOffsetInWord;

    uint8 * p = (uint8 *) bits;
    unsigned int byte_offset = revOffset / 8;
    unsigned int bit_offset = revOffset % 8;
    // flip the byte back
    bit_offset = 7 - bit_offset;

    uint8 byte = p [byte_offset];
    byte >>= bit_offset;
    byte &= 1;
    //printf ("offset %d, byte_offset %d, bit_offset %d, byte %x, bit %x\n", offset, byte_offset, bit_offset, p [byte_offset], byte);
    return byte;
  }

#ifndef QUIET_UNUSED
//
// extr
//    Get a string of bits (up to 64)
//

uint64 extr (void * bits, int offset, int nbits)
  {
    uint64 n = 0;
    int i;
    for (i = nbits - 1; i >= 0; i --)
      {
        n <<= 1;
        n |= getbit (bits, i + offset);
        //printf ("%012lo\n", n);
      }
    return n;
  }
#endif

int extractASCII36FromBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 *wordp)
  {
    uint wp = * words_processed; // How many words have been processed

    // 1 dps8m word == 4 bytes

    uint bytes_processed = wp * 4;
    if (bytes_processed >= tbc)
      return 1;
    //sim_printf ("store 0%08lo@0%012"PRIo64"\n", wordp - M, extr36 (bufp, wp));

    * wordp = extrASCII36 (bufp, wp);
//if (* wordp & ~MASK36) sim_printf (">>>>>>> extr %012"PRIo64"\n", * wordp); 
    //sim_printf ("* %06lo = %012"PRIo64"\n", wordp - M, * wordp);
    (* words_processed) ++;

    return 0;
  }

int extractWord36FromBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 *wordp)
  {
    uint wp = * words_processed; // How many words have been processed

    // 2 dps8m words == 9 bytes

    uint bytes_processed = (wp * 9 + 1) / 2;
    if (bytes_processed >= tbc)
      return 1;
    //sim_printf ("store 0%08lo@0%012"PRIo64"\n", wordp - M, extr36 (bufp, wp));

    * wordp = extr36 (bufp, wp);
//if (* wordp & ~MASK36) sim_printf (">>>>>>> extr %012"PRIo64"\n", * wordp); 
    //sim_printf ("* %06lo = %012"PRIo64"\n", wordp - M, * wordp);
    (* words_processed) ++;

    return 0;
  }

int insertASCII36toBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 word)
  {
    uint wp = * words_processed; // How many words have been processed

    // 1 dps8m word == 4 bytes

    uint bytes_processed = wp * 4;
    if (bytes_processed >= tbc)
      return 1;
    //sim_printf ("store 0%08lo@0%012"PRIo64"\n", wordp - M, extr36 (bufp, wp));

    putASCII36 (word, bufp, wp);
    //sim_printf ("* %06lo = %012"PRIo64"\n", wordp - M, * wordp);
    (* words_processed) ++;

    return 0;
  }

int insertWord36toBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 word)
  {
    uint wp = * words_processed; // How many words have been processed

    // 2 dps8m words == 9 bytes

    uint bytes_processed = (wp * 9 + 1) / 2;
    if (bytes_processed >= tbc)
      return 1;
    //sim_printf ("store 0%08lo@0%012"PRIo64"\n", wordp - M, extr36 (bufp, wp));

    put36 (word, bufp, wp);
    //sim_printf ("* %06lo = %012"PRIo64"\n", wordp - M, * wordp);
    (* words_processed) ++;

    return 0;
  }

#ifndef NEED_128
static void print_uint128_r (uint128 n, char * p)
  {
    if (n == 0)
      return;

    print_uint128_r(n / 10, p);
    if (p)
      {
        char s [2];
        s [0] = n % 10 + '0';
        s [1] = '\0';
        strcat (p, s);
      }
    else
      sim_printf("%c", (int) (n%10+0x30));
  }

void print_int128 (int128 n, char * p)
  {
    if (n == 0)
      {
        if (p)
          strcat (p, "0");
        else
          sim_printf ("0");
        return;
      }
    if (n < 0)
      {
        if (p)
          strcat (p, "-");
        else
          sim_printf ("-");
        n = -n;
      }
    print_uint128_r ((uint128) n, p);
  }
#endif

// Return simh's gtime as a long long.
uint64 sim_timell (void)
  {
    return (uint64) sim_gtime ();
  }

