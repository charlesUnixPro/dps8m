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
            flags & I_NBAR	? "-BAR "  : "",
            flags & I_PMASK ? "PMask " : "",
            flags & I_PAR   ? "Par "   : "",
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

