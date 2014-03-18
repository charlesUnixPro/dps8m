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
#include "dps8_utils.h"

/*
 * misc utility routines used by simulator
 */

//I_HEX I_ABS I_MIIF I_TRUNC  I_NBAR	I_PMASK I_PAR	 I_TALLY I_OMASK  I_EUFL	 I_EOFL	I_OFLOW	I_CARRY	    I_NEG	 I_ZERO
char * dumpFlags(word18 flags)
{
    static char buffer[256] = "";
    
    sprintf(buffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
            flags & I_HEX   ? "Hex "   : "",
            flags & I_ABS   ? "Abs "   : "",
            flags & I_MIIF  ? "Miif "  : "",
            flags & I_TRUNC ? "Trunc " : "",
            flags & I_NBAR	? "~BAR "  : "",
            flags & I_PMASK ? "PMask " : "",
            flags & I_PERR  ? "PErr"   : "",
            flags & I_TALLY ? "Tally " : "",
            flags & I_OMASK ? "OMASK " : "",
            flags & I_EUFL  ? "EUFL "  : "",
            flags & I_EOFL  ? "EOFL "  : "",
            flags & I_OFLOW	? "Ovr "   : "",
            flags & I_CARRY	? "Carry " : "",
            flags & I_NEG   ? "Neg "   : "",
            flags & I_ZERO  ? "Zero "  : ""
            );
    return buffer;
    
}

static char *
strupr(char *str)
{
    char *s;
    
    for(s = str; *s; s++)
        *s = (char) toupper((unsigned char)*s);
    return str;
}

//extern char *opcodes[], *mods[];
//extern struct opCode NonEISopcodes[0100], EISopcodes[01000];

//! get instruction info for IWB ...

static opCode UnImp = {"(unimplemented)", 0, 0, 0, 0};

struct opCode *getIWBInfo(DCDstruct *i)
{
    opCode *p;
    
    if (i->opcodeX == false)
        p = &NonEISopcodes[i->opcode];
    else
        p = &EISopcodes[i->opcode];
    
    if (p->mne == 0)
    {
#ifndef QUIET_UNUSED
        int r = 1;
#endif
    }
    
    return p->mne ? p : &UnImp;
}

char *disAssemble(word36 instruction)
{
    int32 opcode  = GET_OP(instruction);   ///< get opcode
    int32 opcodeX = GET_OPX(instruction);  ///< opcode extension
    a8    address = GET_ADDR(instruction);
    int32 a       = GET_A(instruction);
    //int32 i       = GET_I(instruction);
    int32 tag     = GET_TAG(instruction);

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
        // return strupr(result);
    } else {
        sprintf(buff, " %06o", address);
        strcat (result, buff);
    }
    // get mod
    strcpy(buff, "");
    for(int n = 0 ; n < 0100 ; n++)
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
    
    return strupr(result);
}

/*
 * getModString ()
 *
 * Convert instruction address modifier tag to printable string
 * WARNING: returns pointer to statically allocated string
 *
 */

char *getModString(int32 tag)
{
    static char msg[256];
    strcpy(msg, "none");
    
    if (tag >= 0100)
    {
        sprintf(msg, "getModReg(tag out-of-range %o)", tag);
    } else {
        for(int n = 0 ; n < 0100 ; n++)
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


word36 AddSub36b(char op, bool isSigned, word36 op1, word36 op2, word18 flagsToSet, word18 *flags)
{
    word36 res = 0;
    op1 &= ZEROEXT;
    op2 &= ZEROEXT;
        
    // perform requested operation
    bool overflow = false;
    
    switch (op)
    {
        case '+':
            res = op1 + op2;
            overflow = ((~(op1 ^ op2)) & (op1 ^ res) & 0x800000000);
            break;
        case '-':
            res = op1 - op2;
            //op2 = (1 + ~op2) & DMASK;
            //res = op1 + op2;
            overflow = ((~(op1 ^ ~op2)) & (op1 ^ res) & 0x800000000);
            break;
            // XXX make provisions for logicals
    }
    
       // now let's set some flags...
    
    // carry
    // NB: CARRY is not an overflow!
    
    if (flagsToSet & I_CARRY)
    {
        /* const */  bool carry = (res > 0xfffffffff);
if (op == '-') carry = ! carry; // XXX CAC black magic
        if (carry)
            SETF(*flags, I_CARRY);
        else
            CLRF(*flags, I_CARRY);
    }
    
    
    
    /*
     oVerflow rules .....
     
     oVerflow occurs for addition if the operands have the
     same sign and the result has a different sign. MSB(a) = MSB(b) and MSB(r) <> MSB(a)
     
     oVerflow occurs for subtraction if the operands have
     different signs and the sign of the result is different from the
     sign of the first operand. MSB(a) <> MSB(b) and MSB(r) <> MSB(a)
     */
    if (flagsToSet & I_OFLOW)
    {
        if (overflow)
            SETF(*flags, I_OFLOW);      // overflow
    }
    
    if (flagsToSet & I_ZERO)
    {
        if ((res & DMASK) == 0)
            SETF(*flags, I_ZERO);       // zero result
        else
            CLRF(*flags, I_ZERO);
    }
    
    if (flagsToSet & I_NEG)
    {
        if (res & SIGN36)            // if negative (things seem to want this even if unsigned ops)
            SETF(*flags, I_NEG);
        else
            CLRF(*flags, I_NEG);
    }
    
    if (flagsToSet & I_OFLOW)
    {
        if (overflow && ! TSTF (*flags, I_OMASK))
        {
            doFault(NULL, overflow_fault, 0,"addsub36b overflow fault");
        }
    }

    return res & DMASK;           // 64 => 36-bit. Mask off unnecessary bits ...
}

word18 AddSub18b(char op, bool isSigned, word18 op1, word18 op2, word18 flagsToSet, word18 *flags)
{
    word18 res = 0;
    op1 &= ZEROEXT18;
    op2 &= ZEROEXT18;
    //word18 op1 = SIGNEXT18(oP1);
    //word18 op2 = SIGNEXT18(oP2);
    
    // perform requested operation
    bool overflow = false;
    
    switch (op)
    {
        case '+':
            res = op1 + op2;
            overflow = ((~(op1 ^ op2)) & (op1 ^ res) & SIGN18);
            break;
        case '-':
            res = op1 - op2;
            //op2 = (1 + ~op2) & DMASK;
            //res = op1 + op2;
            overflow = ((~(op1 ^ ~op2)) & (op1 ^ res) & SIGN18);
            break;
            // XXX make provisions for logicals
    }
    
    // now let's set some flags...
    
    // carry
    // NB: CARRY is not an overflow!
    
    if (flagsToSet & I_CARRY)
    {
        /* const */ bool carry = (res > 0777777);
if (op == '-') carry = ! carry; // XXX CAC black magic
        if (carry)
            SETF(*flags, I_CARRY);
        else
            CLRF(*flags, I_CARRY);
    }
    
    
    
    /*
     oVerflow rules .....
     
     oVerflow occurs for addition if the operands have the
     same sign and the result has a different sign. MSB(a) = MSB(b) and MSB(r) <> MSB(a)
     
     oVerflow occurs for subtraction if the operands have
     different signs and the sign of the result is different from the
     sign of the first operand. MSB(a) <> MSB(b) and MSB(r) <> MSB(a)
     */
    if (flagsToSet & I_OFLOW)
    {
        if (overflow)
            SETF(*flags, I_OFLOW);      // overflow
    }
    
    if (flagsToSet & I_ZERO)
    {
        if ((res & MASK18) == 0)
            SETF(*flags, I_ZERO);       // zero result
        else
            CLRF(*flags, I_ZERO);
    }
    
    if (flagsToSet & I_NEG)
    {
        if (res & SIGN18)            // if negative (things seem to want this even if unsigned ops)
            SETF(*flags, I_NEG);
        else
            CLRF(*flags, I_NEG);
    }
    
    if (flagsToSet & I_OFLOW)
    {
        if (overflow && ! TSTF (*flags, I_OMASK))
        {
            doFault(NULL, overflow_fault, 0,"addsub18b overflow fault");
        }
    }

    return res & MASK18;           // 32 => 18-bit. Mask off unnecessary bits ...
}

word72 AddSub72b(char op, bool isSigned, word72 op1, word72 op2, word18 flagsToSet, word18 *flags)
{
    word72 res = 0;
    op1 &= ZEROEXT72;
    op2 &= ZEROEXT72;
    
    // perform requested operation
    bool overflow = false;
    
    switch (op)
    {
        case '+':
            res = op1 + op2;
            overflow = ((~(op1 ^ op2)) & (op1 ^ res) & SIGN72);
            break;
        case '-':
            res = op1 - op2;
            overflow = ((~(op1 ^ ~op2)) & (op1 ^ res) & SIGN72);
            break;
            // XXX make provisions for logicals
    }
    
    // now let's set some flags...
    
    // carry
    // NB: CARRY is not an overflow!
    
    if (flagsToSet & I_CARRY)
    {
        /* const */ bool carry = (res > MASK72);
if (op == '-') carry = ! carry; // XXX CAC black magic
        if (carry)
            SETF(*flags, I_CARRY);
        else
            CLRF(*flags, I_CARRY);
    }

    if (flagsToSet & I_OFLOW)
    {
        if (overflow)
            SETF(*flags, I_OFLOW);      // overflow
    }
    
    if (flagsToSet & I_ZERO)
    {
        if ((res & MASK72) == 0)
            SETF(*flags, I_ZERO);       // zero result
        else
            CLRF(*flags, I_ZERO);
    }
    
    if (flagsToSet & I_NEG)
    {
        if (res & SIGN18)            // if negative (things seem to want this even if unsigned ops)
            SETF(*flags, I_NEG);
        else
            CLRF(*flags, I_NEG);
    }
    
    if (flagsToSet & I_OFLOW)
    {
        if (overflow)
        {
            doFault(NULL, overflow_fault, 0,"addsub72 overflow fault");
        }
    }
    
    return res & MASK72;           // 128 => 72-bit. Mask off unnecessary bits ...
}

word36 compl36(word36 op1, word18 *flags)
{
    //printf("op1 = %llo %llo\n", op1, (-op1) & DMASK);
    
    op1 &= DMASK;
    
    word36 res = -op1 & DMASK;
    
    const bool ovr = op1 == MAXNEG;
    if (ovr)
        SETF(*flags, I_OFLOW);
    if (res & SIGN36)
        SETF(*flags, I_NEG);
    else
        CLRF(*flags, I_NEG);
    
    if (res == 0)
        SETF(*flags, I_ZERO);
    else
        CLRF(*flags, I_ZERO);
    
    if (ovr && ! TSTF (*flags, I_OMASK))
    {
        doFault(NULL, overflow_fault, 0,"compl36 overflow fault");
    }

    return res;
}

word18 compl18(word18 op1, word18 *flags)
{
    //printf("op1 = %llo %llo\n", op1, (-op1) & DMASK);
    
    op1 &= MASK18;
    
    word18 res = -op1 & MASK18;
    
    const bool ovr = op1 == MAX18NEG;
    if (ovr)
        SETF(*flags, I_OFLOW);
    if (res & SIGN18)
        SETF(*flags, I_NEG);
    else
        CLRF(*flags, I_NEG);
    
    if (res == 0)
        SETF(*flags, I_ZERO);
    else
        CLRF(*flags, I_ZERO);
    
    if (ovr && ! TSTF (*flags, I_OMASK))
        doFault(NULL, overflow_fault, 0,"compl18 overflow fault");

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
    
    int offset = 27 - (9 * posn);//    0;
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
    *dst = bitfieldInsert36(*dst, (word36)data, offset, 9);
}
void putChar(word36 *dst, word6 data, int posn)
{
    // XXX which is faster switch() or calculation?
    
    int offset = 30 - (6 * posn);   //0;
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
    *dst = bitfieldInsert36(*dst, (word36)data, offset, 6);
}

word72 convertToWord72(word36 even, word36 odd)
{
    return ((word72)even << 36) | (word72)odd;
}

void convertToWord36(word72 src, word36 *even, word36 *odd)
{
    *even = (word36)(src >> 36) & DMASK;
    *odd = (word36)src & DMASK;
}

//! XXX the following compare routines probably need sign extension
void cmp36(word36 oP1, word36 oP2, word18 *flags)
{
    word36s op1 = (word36s) SIGNEXT36(oP1 & DMASK);
    word36s op2 = (word36s) SIGNEXT36(oP2 & DMASK);
    
    if (!((word36)op1 & SIGN36) && ((word36)op2 & SIGN36) && (op1 > op2))
        CLRF(*flags, I_ZERO | I_NEG | I_CARRY);
    else if (((word36)op1 & SIGN36) == ((word36)op2 & SIGN36) && (op1 > op2))
    {
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_ZERO | I_NEG);
    } else if ((((word36)op1 & SIGN36) == ((word36)op2 & SIGN36)) && (op1 == op2))
    {
        SETF(*flags, I_ZERO | I_CARRY);
        CLRF(*flags, I_NEG);
    } else if ((((word36)op1 & SIGN36) == ((word36)op2 & SIGN36)) && (op1 < op2))
    {
        SETF(*flags, I_NEG);
        CLRF(*flags, I_ZERO | I_CARRY);
    } else if ((((word36)op1 & SIGN36) && !((word36)op2 & SIGN36)) && (op1 < op2))
    {
        SETF(*flags, I_CARRY | I_NEG);
        CLRF(*flags, I_ZERO);
    }
}
void cmp18(word18 oP1, word18 oP2, word18 *flags)
{
    word18s op1 = (word18s) SIGNEXT18(oP1 & MASK18);
    word18s op2 = (word18s) SIGNEXT18(oP2 & MASK18);

    if (!((word18)op1 & SIGN18) && ((word18)op2 & SIGN18) && (op1 > op2))
        CLRF(*flags, I_ZERO | I_NEG | I_CARRY);
    else if (((word18)op1 & SIGN18) == ((word18)op2 & SIGN18) && (op1 > op2))
    {
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_ZERO | I_NEG);
    } else if ((((word18)op1 & SIGN18) == ((word18)op2 & SIGN18)) && (op1 == op2))
    {
        SETF(*flags, I_ZERO | I_CARRY);
        CLRF(*flags, I_NEG);
    } else if ((((word18)op1 & SIGN18) == ((word18)op2 & SIGN18)) && (op1 < op2))
    {
        SETF(*flags, I_NEG);
        CLRF(*flags, I_ZERO | I_CARRY);
    } else if ((((word18)op1 & SIGN18) && !((word18)op2 & SIGN18)) && (op1 < op2))
    {
        SETF(*flags, I_CARRY | I_NEG);
        CLRF(*flags, I_ZERO);
    }
}
void cmp36wl(word36 A, word36 Y, word36 Q, word18 *flags)
{
    // This is wrong; signed math is needed.

    //bool Z = (A <= Y && Y <= Q) || (A >= Y && Y >= Q);

    word36s As = (word36s) SIGNEXT36(A & DMASK);
    word36s Ys = (word36s) SIGNEXT36(Y & DMASK);
    word36s Qs = (word36s) SIGNEXT36(Q & DMASK);
    bool Z = (As <= Ys && Ys <= Qs) || (As >= Ys && Ys >= Qs);

    SCF(Z, *flags, I_ZERO);
    
    if (!(Q & SIGN36) && (Y & SIGN36) && (Qs > Ys))
        CLRF(*flags, I_NEG | I_CARRY);
    else if (((Q & SIGN36) == (Y & SIGN36)) && (Qs >= Ys))
    {
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_NEG);
    } else if (((Q & SIGN36) == (Y & SIGN36)) && (Qs < Ys))
    {
        CLRF(*flags, I_CARRY);
        SETF(*flags, I_NEG);
    } else if ((Q & SIGN36) && !(Y & SIGN36) && (Qs < Ys))
        SETF(*flags, I_NEG | I_CARRY);
}

void cmp72(word72 op1, word72 op2, word18 *flags)
{
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
    word72s op1s = (word72s) SIGNEXT72 (op1 & MASK72);
    word72s op2s = (word72s) SIGNEXT72 (op2 & MASK72);
    if (op1s > op2s)
      {
        if (op2 & SIGN72)
          CLRF (* flags, I_CARRY);
        else
          SETF (* flags, I_CARRY);
        CLRF (* flags, I_ZERO | I_NEG);
      }
    else if (op1s == op2s)
      {
        SETF (* flags, I_CARRY | I_ZERO);
        CLRF (* flags, I_NEG);
      }
    else /* op1s < op2s */
      {
        if (op1 & SIGN72)
          SETF (* flags, I_CARRY);
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

char *
strlower(char *q)
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
#define	STAR	0
#define	NOTSTAR	1
#define	RESET	2

int strmask(char *str, char *mask)
/*!
 Tests string 'str' against mask string 'mask'
 Returns TRUE if the string matches the mask.
 
 The mask can contain '?' and '*' wild card characters.
 '?' matches any	single character.
 '*' matches any number of any characters.
 
 For example:
 strmask("Hello", "Hello");	---> TRUE
 strmask("Hello", "Jello");	---> FALSE
 strmask("Hello", "H*o");	---> TRUE
 strmask("Hello", "H*g");	---> FALSE
 strmask("Hello", "?ello");	---> TRUE
 strmask("Hello", "H????");	---> TRUE
 strmask("H", "H????");		---> FALSE
 */
{
	char *sp, *mp, *reset_string, *reset_mask, *sn;
	int state;
    
	sp = str;
	mp = mask;
    
	while (1) {
		switch (*mp) {
            case '\0':
                return(*sp ? false : true);
            case '?':
                sp++;
                mp++;
                break;
            default:
                if (*mp == *sp) {
                    sp++;
                    mp++;
                    break;
                } else {
                    return(false);
                }
            case '*':
                if (*(mp + 1) == '\0') {
                    return(true);
                }
                if ((sn = strchr(sp, *(mp + 1))) == NULL) {
                    return(false);
                }
                
                /* save place -- match rest of string */
                /* if fail, reset to here */
                reset_mask = mp;
                reset_string = sn + 1;
                
                mp = mp + 2;
                sp = sn + 1;
                state = NOTSTAR;
                while (state == NOTSTAR) {
                    switch (*mp) {
                        case '\0':
                            if (*sp == '\0')
                                return(false);
                            else
                                state = RESET;
                            break;
                        case '?':
                            sp++;
                            mp++;
                            break;
                        default:
                            if (*mp == *sp) {
                                sp++;
                                mp++;
                            } else
                                state = RESET;
                            break;
                        case '*':
                            state = STAR;
                            break;
                    }
                }
                /* we've reach a new star or should reset to last star */
                if (state == RESET) {
                    sp = reset_string;
                    mp = reset_mask;
                }
                break;
		}
	}
	return(true);
}

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
    //printf("mask=%012llo\n", mask);
    if (b > 0)
        return (a >> b) & mask; // original pseudocode had b-1
    else
        return a & mask;
}
word72 bitfieldExtract72(word72 a, int b, int c)
{
    word72 mask = ~((word72)-1 << c);
    if (b > 0)
        return (a >> b) & mask; // original pseudocode had b-1
    else
        return a & mask;
}
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


//#define MASKBITS(x) ( ~(~((t_uint64)0)<<x) ) // lower (x) bits all ones

/*
 * getbits36()
 *
 * Extract a range of bits from a 36-bit word.
 */

inline word36 getbits36(word36 x, uint i, uint n) {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)n+1;
    if (shift < 0 || shift > 35) {
//        sim_debug (DBG_ERR, & cpu_dev, "getbits36: bad args (%012llo,i=%d,n=%d)\n", x, i, n);
//        cancel_run(STOP_BUG);
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

inline word36 setbits36(word36 x, uint p, uint n, word36 val)
{
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
//        sim_debug (DBG_ERR, & cpu_dev, "setbits36: bad args (%012llo,pos=%d,n=%d)\n", x, p, n);
//        cancel_run(STOP_BUG);
        return 0;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    word36 result = (x & ~ mask) | ((val&MASKBITS(n)) << (36 - p - n));
    return result;
}



/*
 * bin2text()
 *
 * Display as bit string.
 * WARNING: returns pointer of two alternating static buffers
 *
 */

#include <ctype.h>

char *bin2text(t_uint64 word, int n)
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

// XXX what about config=addr7=123, where clist has a "addr%"?

// return -2: error; -1: done; >= 0 option found
int cfgparse (const char * tag, char * cptr, config_list_t * clist, config_state_t * state, int64_t * result)
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

