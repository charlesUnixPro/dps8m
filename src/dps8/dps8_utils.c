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

char *
strupr(char *str)
{
    char *s;
    
    for(s = str; *s; s++)
        *s = toupper((unsigned char)*s);
    return str;
}

//extern char *opcodes[], *mods[];
//extern struct opCode NonEISopcodes[0100], EISopcodes[01000];

//! get instruction info for IWB ...
struct opCode *getIWBInfo(DCDstruct *i)
{
    if (i->opcodeX == false)
        return &NonEISopcodes[i->opcode];
    return &EISopcodes[i->opcode];
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
        return strupr(result);
    }
    
    sprintf(buff, " %06o", address);
    strcat (result, buff);
    
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

#ifdef PDP10_FLAGS_DONT_WORK
/*! Integer add
 
 Truth table for integer add
 
 case    a       b       r       flags
 1       +       +       +       none
 2       +       +       -       AOV + C1
 3       +       -       +       C0 + C1
 4       +       -       -       -
 5       -       +       +       C0 + C1
 6       -       +       -       -
 7       -       -       +       AOV + C0
 8       -       -       -       C0 + C1
 */

d8 add36PDP10 (char op, d8 a, d8 b, word18 *flags)
{
    
    d8 r = 0;
    switch (op)
    {
        case '+':
            r = (a + b) & DMASK;
            if (TSTS (a & b)) {                                     /* cases 7,8 */
                if (TSTS (r))                                       /* case 8 */
                    SETF (*flags, I_CARRY);
                else
                    SETF (*flags, (I_CARRY | I_OFLOW));             /* case 7 */
                return r;
            }
            if (!TSTS (a | b)) {                                    /* cases 1,2 */
                if (TSTS (r))                                       /* case 2 */
                    SETF (*flags, (I_CARRY | I_OFLOW));
                return r;                                           /* case 1 */
            }
            if (!TSTS (r))                                          /* cases 3,5 */
                SETF (*flags, I_CARRY);
            return r;

        case '-':
            r = (a - b) & DMASK;
            if (TSTS (a & ~b)) {                                    /* cases 7,8 */
                if (TSTS (r))                                       /* case 8 */
                    SETF (*flags, I_CARRY);
                else SETF (*flags, I_CARRY| I_OFLOW);                    /* case 7 */
                return r;
            }
            if (!TSTS (a | ~b)) {                                   /* cases 1,2 */
                if (TSTS (r))                                       /* case 2 */
                    SETF (*flags, I_CARRY | I_OFLOW);
                return r;                                           /* case 1 */
            }
            if (!TSTS (r))                                          /* cases 3,5 */
                SETF (*flags, I_CARRY);
            return r;
    }
   }
#endif

word36 AddSub36(char op, bool isSigned, word36 op1, word36 op2, word18 flagsToSet, word18 *flags)
{
    word36 res = 0;
    
    // assuming these are proper 36-bit integers we may need todo some sign extension
    // prior to any signed ops .....
//
//    if (isSigned)
//    {
//        // is op1 a 36-bit negative number?
//        if (op1 & SIGN) // sign bit set (<0)?
//            op1 |= SIGNEXT;
//        // is op2 a 36-bit negative number?
//        if (op2 & SIGN) // sign bit set (<0)?
//            op2 |= SIGNEXT;
//    } else {
        // just in case .....
        op1 &= ZEROEXT;
        op2 &= ZEROEXT;
    //}
    //op1 &= DMASK;
    //op2 &= DMASK;
    
    // perform requested operation
    bool overflow2 =   false;
    
    switch (op)
    {
        case '+':
            res = op1 + op2;
            break;
        case '-':
            res = op1 - op2;
            break;
        // XXX make provisions for logicals
    }
    
    // now let's set some flags...
    
    // carry
    // NB: CARRY is not an overflow!
    
    if (flagsToSet & I_CARRY)
    {
        //bool carry = res & CARRY;
        bool carry = false;
        //bool neg1 = op1 & SIGN;
        //bool neg2 = op2 & SIGN;
        
        //if(!neg1 && !neg2)
            if(res & CARRY) {
                carry = true;
                //res &= 0377777777777LL;
            }
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
        bool ovr = false;       // arith overflow?

        bool MSB1 = op1 & SIGN;
        bool MSB2 = op2 & SIGN;
        bool MSBr = res & SIGN;
            
        if (op == '+')
            ovr = (MSB1 == MSB2) && (MSBr != MSB1);
        else // '-'
            ovr = (MSB1 != MSB2) && (MSBr != MSB1);
//        if (isSigned)
//            ovr = (res > 34359738367) || (res < -34359738368) ;
//        else
//            ovr = res > 68719476736;
        if (ovr)
            SETF(*flags, I_OFLOW);      // overflow
    }
                
    if (flagsToSet & I_ZERO)
    {
        if (res == 0)
            SETF(*flags, I_ZERO);       // zero result
        else
            CLRF(*flags, I_ZERO);
    }
    
    if (flagsToSet & I_NEG)
    {
        if (res & SIGN)            // if negative (things seem to want this even if unsigned ops)
            SETF(*flags, I_NEG);
        else
            CLRF(*flags, I_NEG);
    }
    
    return res & DMASK;           // 64 => 36-bit. Mask off unnecessary bits ...
}

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
        const bool carry = (res > 0xfffffffff);
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
        if (res == 0)
            SETF(*flags, I_ZERO);       // zero result
        else
            CLRF(*flags, I_ZERO);
    }
    
    if (flagsToSet & I_NEG)
    {
        if (res & SIGN)            // if negative (things seem to want this even if unsigned ops)
            SETF(*flags, I_NEG);
        else
            CLRF(*flags, I_NEG);
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
        const bool carry = (res > 0777777);
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
        if (res == 0)
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
        const bool carry = (res > MASK72);
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
        if (res == 0)
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
    
    return res & MASK72;           // 128 => 72-bit. Mask off unnecessary bits ...
}

word36 compl36(word36 op1, word18 *flags)
{
    //printf("op1 = %llo %llo\n", op1, (-op1) & DMASK);
    
    op1 &= DMASK;
    
    word36 res = -op1 & DMASK;
    
    const bool ovr = res == MAXNEG;
    if (ovr)
        SETF(*flags, I_OFLOW);
        // should we continue operation if OVR fault?
    if (res & SIGN)
        SETF(*flags, I_NEG);
    else
        CLRF(*flags, I_NEG);
    
    if (res == 0)
        SETF(*flags, I_ZERO);
    else
        CLRF(*flags, I_ZERO);
    
    return res;
}
word18 compl18(word18 op1, word18 *flags)
{
    //printf("op1 = %llo %llo\n", op1, (-op1) & DMASK);
    
    op1 &= MASK18;
    
    word18 res = -op1 & MASK18;
    
    const bool ovr = res == MAX18NEG;
    if (ovr)
        SETF(*flags, I_OFLOW);
    // should we continue operation if OVR fault?
    if (res & SIGN)
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
    word36s op1 = SIGNEXT36(oP1);
    word36s op2 = SIGNEXT36(oP2);
    
    if (!(op1 & SIGN) && (op2 & SIGN) && (op1 > op2))
        CLRF(*flags, I_ZERO | I_NEG | I_CARRY);
    else if ((op1 & SIGN) == (op2 & SIGN) && (op1 > op2))
    {
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_ZERO | I_NEG);
    } else if (((op1 & SIGN) == (op2 & SIGN)) && (op1 == op2))
    {
        SETF(*flags, I_ZERO | I_CARRY);
        CLRF(*flags, I_NEG);
    } else if (((op1 & SIGN) == (op2 & SIGN)) && (op1 < op2))
    {
        SETF(*flags, I_NEG);
        CLRF(*flags, I_ZERO | I_CARRY);
    } else if (((op1 & SIGN) && !(op2 & SIGN)) && (op1 < op2))
    {
        SETF(*flags, I_CARRY | I_NEG);
        CLRF(*flags, I_ZERO);
    }
}
void cmp18(word18 oP1, word18 oP2, word18 *flags)
{
    word18s op1 = SIGNEXT18(oP1);
    word18s op2 = SIGNEXT18(oP2);

    if (!(op1 & SIGN18) && (op2 & SIGN18) && (op1 > op2))
        CLRF(*flags, I_ZERO | I_NEG | I_CARRY);
    else if ((op1 & SIGN18) == (op2 & SIGN18) && (op1 > op2))
    {
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_ZERO | I_NEG);
    } else if (((op1 & SIGN18) == (op2 & SIGN18)) && (op1 == op2))
    {
        SETF(*flags, I_ZERO | I_CARRY);
        CLRF(*flags, I_NEG);
    } else if (((op1 & SIGN18) == (op2 & SIGN18)) && (op1 < op2))
    {
        SETF(*flags, I_NEG);
        CLRF(*flags, I_ZERO | I_CARRY);
    } else if (((op1 & SIGN18) && !(op2 & SIGN18)) && (op1 < op2))
    {
        SETF(*flags, I_CARRY | I_NEG);
        CLRF(*flags, I_ZERO);
    }
}
void cmp36wl(word36 A, word36 Y, word36 Q, word18 *flags)
{
    bool Z = (A <= Y && Y <= Q) || (A >= Y && Y >= Q);
    SCF(Z, *flags, I_ZERO);
    
    if (!(Q & SIGN) && (Y & SIGN) && (Q > Y))
        CLRF(*flags, I_NEG | I_CARRY);
    else if (((Q & SIGN) == (Y & SIGN)) && (Q >= Y))
    {
        SETF(*flags, I_CARRY);
        CLRF(*flags, I_NEG);
    } else if (((Q & SIGN) == (Y & SIGN)) && (Q < Y))
    {
        CLRF(*flags, I_CARRY);
        SETF(*flags, I_NEG);
    } else if ((Q & SIGN) && !(Y & SIGN) && (Q < Y))
        SETF(*flags, I_NEG | I_CARRY);
}

void cmp72(word72 op1, word72 op2, word18 *flags)
{
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
			*s = tolower(*s);
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
    int mask = ~(0xffffffff << c);
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
        if (x & mask)
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
        mask = 0x80000000 >> i;
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


// From MM's code ...


// Debugging and statistics
int opt_debug;
t_uint64 calendar_a; // Used to "normalize" A/Q dumps of the calendar as deltas
t_uint64 calendar_q;
//stats_t sys_stats;


extern DEVICE cpu_dev;
extern FILE *sim_deb, *sim_log;

static void msg(enum log_level level, const char *who, const char* format, va_list ap);
static int _scan_seg(uint segno, int msgs);
uint ignore_IC = 0;
uint last_IC;
uint last_IC_seg;

static int _log_any_io = 0;


#define MASKBITS(x) ( ~(~((t_uint64)0)<<x) ) // lower (x) bits all ones

/*
 * getbits36()
 *
 * Extract a range of bits from a 36-bit word.
 */

inline t_uint64 getbits36(t_uint64 x, int i, unsigned n) {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-i-n+1;
    if (shift < 0 || shift > 35) {
//        log_msg(ERR_MSG, "getbits36", "bad args (%012llo,i=%d,n=%d)\n", x, i, n);
//        cancel_run(STOP_BUG);
        return 0;
    } else
        return (x >> (unsigned) shift) & ~ (~0 << n);
}

// ============================================================================

/*
 * setbits36()
 *
 * Set a range of bits in a 36-bit word -- Returned value is x with n bits
 * starting at p set to the n lowest bits of val
 */

inline t_uint64 setbits36(t_uint64 x, int p, unsigned n, t_uint64 val)
{
    int shift = 36 - p - n;
    if (shift < 0 || shift > 35) {
//        log_msg(ERR_MSG, "setbits36", "bad args (%012llo,pos=%d,n=%d)\n", x, p, n);
//        cancel_run(STOP_BUG);
        return 0;
    }
    t_uint64 mask = ~ (~0<<n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    t_uint64 result = (x & ~ mask) | ((val&MASKBITS(n)) << (36 - p - n));
    return result;
}

bool unitTrace = false; // when TRUE unit tracing is enabled ...

void log_msg(enum log_level level, const char* who, const char* format, ...)
{
    //if (sim_quiet && level != INFO_MSG)
    //    return;
 
    if (unitTrace == false)
        return;
    
    if (level == DEBUG_MSG) {
        if (opt_debug == 0)
            return;
        if (cpu_dev.dctrl == 0 && opt_debug < 1) // todo: should CPU control all debug settings?
            return;
    }
    
    // Make sure all messages have a prior display of the IC
//    if (!ignore_IC && (PPR.IC != last_IC || PPR.PSR != last_IC_seg)) {
//        last_IC = PPR.IC;
//        last_IC_seg = PPR.PSR;
//        char *tag = "Debug";
//        // char *who = "IC";
//        // out_msg("\n%s: %*s %s %*sIC: %o\n", tag, 7-strlen(tag), "", who, 18-strlen(who), "", PPR.IC);
//        char icbuf[80];
//        addr_modes_t addr_mode = get_addr_mode();
//        ic2text(icbuf, addr_mode, PPR.PSR, PPR.IC);
//        // out_msg("\n");
//        // out_msg("%s: %*s IC: %s\n", tag, 7-strlen(tag), "", icbuf);
//        msg(DEBUG_MSG, NULL, "\n", NULL);
//        char buf[80];
//        sprintf(buf, "%s: %*s IC: %s\n", tag, 7 - (int) strlen(tag), "", icbuf);
//        msg(DEBUG_MSG, NULL, buf, NULL);
//    }
    
    va_list ap;
    va_start(ap, format);
#if 0
    char *tag = (level == DEBUG_MSG) ? "Debug" :
    (level == INFO_MSG) ? "Info" :
    (level == NOTIFY_MSG) ? "Note" :
    (level == WARN_MSG) ? "WARNING" :
    (level == ERR_MSG) ? "ERROR" :
    "???MESSAGE";
    msg(tag, who, format, ap);
#else
    msg(level, who, format, ap);
#endif
    va_end(ap);
}

#if 0
void debug_msg(const char* who, const char* format, ...)
{
    if (opt_debug == 0)
        return;
    if (cpu_dev.dctrl == 0 && opt_debug < 1) // todo: should CPU control all debug settings?
        return;
    va_list ap;
    va_start(ap, format);
    msg("Debug", who, format, ap);
    va_end(ap);
}

void warn_msg(const char* who, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    msg("WARNING", who, format, ap);
    va_end(ap);
}

void complain_msg(const char* who, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    msg("ERROR", who, format, ap);
    va_end(ap);
}
#endif


static void crnl_out(FILE *stream, const char *format, va_list ap)
{
    // SIMH does something odd with the terminal, so output CRNL
    int len =strlen(format);
    int nl = *(format + len - 1) == '\n';
    if (nl) {
        char *f = malloc(len + 2);
        if (f) {
            strcpy(f, format);
            *(f + len - 1) = '\r';
            *(f + len) = '\n';
            *(f + len + 1) = 0;
            if (ap == NULL)
                fprintf(stream, "%s", f);
            else
                vfprintf(stream, f, ap);
            free(f);
        } else {
            if (ap == NULL)
                printf("%s", format);
            else
                vprintf(format, ap);
            if (*(format + strlen(format) - 1) == '\n')
                fprintf(stream, "\r");
        }
    } else {
        if (ap == NULL)
            fprintf(stream, "%s", format);
        else
            vfprintf(stream, format, ap);
        if (*(format + strlen(format) - 1) == '\n')
            fprintf(stream, "\r");
    }
    _log_any_io = 1;
}


void out_msg(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    
    FILE *stream = (sim_log != NULL) ? sim_log : stdout;
    crnl_out(stream, format, ap);
    va_end(ap);
    if (sim_deb != NULL) {
        va_start(ap, format);
        crnl_out(sim_deb, format, ap);
        va_end(ap);
    }
}

#if 0
static void sim_hmsg(const char* tag, const char *who, const char* format, va_list ap)
{
    // This version uses SIMH facilities -- not tested
    char buf[2000];
    snprintf(buf, sizeof(buf), "%s: %*s %s: %*s", tag, 7-strlen(tag), "", who, 18-strlen(who), "");
    int l = strlen(buf);
    vsnprintf(buf + l, sizeof(buf) - l, format, ap);
    // TODO: setup every device with sim_debtab entries to reflect different debug levels
    sim_debug(~0, &cpu_dev, "%s", buf);
}
#endif

static void msg(enum log_level level, const char *who, const char* format, va_list ap)
{
    // This version does not use SIMH facilities -- except for the sim_deb and sim_log streams
    
    enum { con, dbg };
    FILE *streams[2];
    
    streams[con] = (sim_log != NULL) ? sim_log : stdout;
    streams[dbg] = sim_deb;
    if (level == DEBUG_MSG || level == INFO_MSG) {
        // Debug and info messages go to a debug log if one exists, otherwise to
        // the console
        if (streams[dbg] != NULL)
            streams[con] = NULL;
    } else {
        // Non debug msgs always go to the console. If a seperate debug
        // log exists, it also gets non-debug msgs.
        streams[dbg] = (sim_log == sim_deb) ? NULL : sim_deb;
    }
    
    char *tag = (level == DEBUG_MSG) ? "Debug" :
    (level == INFO_MSG) ? "Info" :
    (level == NOTIFY_MSG) ? "Note" :
    (level == WARN_MSG) ? "WARNING" :
    (level == WARN_MSG) ? "WARNING" :
    (level == ERR_MSG) ? "ERROR" :
    "???MESSAGE";
    
    for (int s = 0; s <= dbg; ++s) {
        FILE *stream = streams[s];
        if (stream == NULL)
            continue;
        if (who != NULL)
            fprintf(stream, "%s: %*s %s: %*s", tag, 7 - (int) strlen(tag), "", who, 18 - (int) strlen(who), "");
        
        if (ap == NULL)
            crnl_out(stream, format, NULL);
        else {
            va_list aq;
            va_copy(aq, ap);
            crnl_out(stream, format, aq);
            va_end(aq);
        }
        if (level != DEBUG_MSG) // BUG: ?
            fflush(stream);
    }
}

/*
 * bin2text()
 *
 * Display as bit string.
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

/*
 Provides a library for reading chunks of an arbitrary number
 of bits from a stream.
 
 NOTE: It was later discovered that the emulator doesn't need
 this generality.   All known "tape" files are multiples of
 72 bits, so the emulator could simply read nine 8-bit bytes at
 a time which would yield two 36-bit "words".
 
 WARNING: uses mmap() and mumap() which may not be available on
 non POSIX systems.  We only read the input data one byte at a time,
 so switching to ordinary file I/O would be trivial.
 
 */
/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#include "sim_defs.h"

//=============================================================================

/*
 bitstm_create() -- allocate and initialize an empty bitstream_t object.
 */

static bitstream_t* bitstm_create()
{
    
    bitstream_t* bp;
    if ((bp = malloc(sizeof(*bp))) == NULL) {
        perror("malloc");
        return NULL;
    }
    memset(bp, 0, sizeof(*bp));
    
    bp->fd = -1;
    bp->used = 8;   // e.g., none left
    return bp;
}

//=============================================================================

/*
 bitstm_new() -- Create a bitstream_t object tied to the given source buffer.
 */

bitstream_t* bitstm_new(const unsigned char* addr, uint32 len)
{
    
    bitstream_t* bp;
    if ((bp = bitstm_create()) == NULL)
        return NULL;
    
    bp->len = len;
    bp->p = addr;
    bp->head = addr;
    return bp;
}


//=============================================================================


/*
 bitstm_open() -- Create a bitstream_t object and tie a file to it.
 */

bitstream_t* bitstm_open(const char* fname)
{
    bitstream_t *bp;
    
    if ((bp = bitstm_create()) == NULL)
        return NULL;
    if ((bp->fname = strdup(fname)) == NULL) {
        perror("strdup");
        return NULL;
    }
    if ((bp->fd = open(fname, O_RDONLY)) == -1) {
        perror(fname);
        return NULL;
    }
    struct stat sbuf;
    if (stat(fname, &sbuf) != 0) {
        perror(fname);
        return NULL;
    }
    bp->len = sbuf.st_size;
    
    void* addr = mmap(0, bp->len, PROT_READ, MAP_SHARED|MAP_NORESERVE, bp->fd, 0);
    if (addr == NULL) {
        perror("mmap");
        return NULL;
    }
    bp->p = addr;
    bp->head = addr;
    return bp;
}

//=============================================================================

/*
 bitstm_get() -- Extract len bits from a stream
 
 Returns non-zero if unable to provide all of the
 requested bits.  Note that this model doesn't
 allow the caller to distinguish EOF from partial
 reads or other error conditions.
 */

int bitstm_get(bitstream_t *bp, size_t len, t_uint64 *word)
{
    
    *word = 0;
    size_t orig_len = len;
    int used = bp->used;
    int left = 8 - used;
    if (left != 0 && len < left) {
        // We have enough bits left in the currently buffered byte.
        unsigned val = bp->byte >> (8-len); // Consume bits from left of byte
        *word = val;
        //printf("b-debug: used %d leading bits of %d-bit curr byte to fufill small request.\n", len, left);
        bp->byte = bp->byte << len;
        bp->used += len;
        goto b_end;
    }
    
    t_uint64 wtmp;
    
    // Consume remainder of curr byte (but it's not enough)
    if (left != 0) {
        wtmp = bp->byte >> (8-left);
        len -= left;
        //printf("b-debug: using remaing %d bits of curr byte\n", left);
        bp->used = 8;
        bp->byte = 0;
    } else
        wtmp = 0;
    
    // Consume zero or more full bytes
    int i;
    for (i = 0; i < len / 8; ++ i) {
        //printf("b-debug: consuming next byte %03o\n", *bp->p);
        if (bp->p == bp->head + bp->len) {
            //fflush(stdout); fflush(stderr);
            // fprintf(stderr, "bits.c: bit stream exhausted\n");   // FIXME: remove text msg
            return 1;
        }
        // left shift in next byte
        wtmp = (wtmp << 8) | (*bp->p);
        ++ bp->p;
    }
    
    // Consume one partial byte if needed (buffer the leftover bits)
    int extra = len % 8;
    if (extra != 0) {
        //printf("b-debug: consuming %d bits of next byte %03o\n", *bp->p);
        if (bp->p == bp->head + bp->len) {
            //fflush(stdout); fflush(stderr);
            //fprintf(stderr, "bits.c: bit stream exhausted\n");    // FIXME: remove text msg
            return 1;
        }
        bp->byte = *bp->p;
        ++ bp->p;
        unsigned val = bp->byte >> (8-extra);
        wtmp = (wtmp << extra) | val;
        //printf("b-debug: used %d leading bits of curr byte to finish request.\n", extra);
        bp->byte = bp->byte << extra;
        bp->used = extra;
    }
    *word = wtmp;
    
b_end:
    //printf("b-debug: after %u req: offset=%u, %d curr bits used, return = %lu\n",
    //  orig_len, bp->p - bp->head, bp->used, (unsigned long) *word);
    return 0;
}

//=============================================================================

/*
 Free all memory associated with given bitstream.
 Returns non-zero on error.
 */

int bitstm_destroy(bitstream_t *bp)
{
    if (bp == NULL)
        return -1;
    
    int err = 0;
    if (bp->fd >= 0) {
        if (bp->p != NULL) {
            err |= munmap((void*)bp->p, bp->len);   // WARNING: casting away const
        }
        err |= close(bp->fd);
    }
    free((void*) bp->fname);
    free(bp);
    return err;
}


