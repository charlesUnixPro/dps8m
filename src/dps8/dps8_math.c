/**
 * \file dps8_math.c
 * \project dps8
 * \date 12/6/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
 * \brief stuff related to math routines for the dps8 simulator
 *        * floating-point routines
 *        * fixed-point routines (eventually)
*/

#include <stdio.h>
#include <math.h>

#include "dps8.h"
#include "dps8_cpu.h"
#include "dps8_ins.h"
#include "dps8_math.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_sys.h"

//! floating-point stuff ....
//! quad to octal
//char *Qtoo(__uint128_t n128);

/*!
 * convert floating point quantity in C(EAQ) to a IEEE long double ...
 */
long double EAQToIEEElongdouble(void)
{
    word8 E = rE;    ///< exponent
    word72 M = ((word72)(rA & DMASK) << 36) | ((word72) rQ & DMASK);   ///< mantissa

    //printf("rA=%012llo rQ=%012llo %s\n", rA, rQ, Qtoo(M));
    
    if (M == 0)
        return 0;
    
    bool S = M & SIGN72; ///< sign of mantissa
    if (S)
        M = (-M) & MASK72;  //((1LL << 63) - 1); // 63 bits (not 28!)
    
    long double m = 0;  ///< mantissa value;
    int8 e = (int8)E;  ///< make signed
    
    long double v = 0.5;
    for(int n = 70 ; n >= 0 ; n -= 1)
    {
        if (M & ((word72)1 << n))
        {
            m += v;
            //printf("1");
        }   //else
        //printf("0");
        v /= 2.0;
    }
    //printf("\n");
    //sim_printf ("E=%o m=%Lf e=%d\n", E, m, e);
    
    if (m == 0 && e == -128)    // special case - normalized 0
        return 0;
    if (m == 0)
        return (S ? -1 : 1) * exp2l(e);
    
    //sim_printf ("frac=%Lf e=%d\n", m, e);
    return (S ? -1 : 1) * ldexpl(m, e);
}

#ifndef QUIET_UNUSED
/*!
 * return normalized dps8 representation of IEEE double f0 ...
 */
float72 IEEElongdoubleToFloat72(long double f0)
{
    if (f0 == 0)
        return (float72)((float72)0400000000000LL << 36);
    
    bool sign = f0 < 0 ? true : false;
    long double f = fabsl(f0);
    
    int exp;
    long double mant = frexpl(f, &exp);
    //sim_printf (sign=%d f0=%Lf mant=%Lf exp=%d\n", sign, f0, mant, exp);
    
    word72 result = 0;
    
    // now let's examine the mantissa and assign bits as necessary...
    
    if (sign && mant == 0.5)
    {
        result = bitfieldInsert72(result, 1, 63, 1);
        exp -= 1;
        mant -= 0.5;
    }
    
    long double bitval = 0.5;    ///< start at 1/2 and go down .....
    for(int n = 62 ; n >= 0 && mant > 0; n -= 1)
    {
        if (mant >= bitval)
        {
            result = bitfieldInsert72(result, 1, n, 1);
            mant -= bitval;
            //sim_printf ("Inserting a bit @ %d %012llo %012llo\n", n , (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
        }
        bitval /= 2.0;
    }
    //sim_printf ("n=%d mant=%f\n", n, mant);
    
    //sim_printf ("result=%012llo %012llo\n", (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
    
    // if f is < 0 then take 2-comp of result ...
    if (sign)
    {
        result = -result & (((word72)1 << 64) - 1);
        //sim_printf ("-result=%012llo %012llo\n", (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
    }
    //! insert exponent ...
    int e = (int)exp;
    result = bitfieldInsert72(result, e & 0377, 64, 8);    ///< & 0777777777777LL;
    
    // XXX TODO test for exp under/overflow ...
    
    return result;
}
#endif


#ifndef QUIET_UNUSED
static long double MYfrexpl(long double x, int *exp)
{
    long double exponents[20], *next;
    int exponent, bit;
    
    /* Check for zero, nan and infinity. */
    if (x != x || x + x == x )
    {
        *exp = 0;
        return x;
    }
    
    if (x < 0)
        return -MYfrexpl(-x, exp);
    
    exponent = 0;
    if (x > 1.0)
    {
        for (next = exponents, exponents[0] = 2.0L, bit = 1;
             *next <= x + x;
             bit <<= 1, next[1] = next[0] * next[0], next++);
        
        for (; next >= exponents; bit >>= 1, next--)
            if (x + x >= *next)
            {
                x /= *next;
                exponent |= bit;
            }
        
    }
    
    else if (x < 0.5)
    {
        for (next = exponents, exponents[0] = 0.5L, bit = 1;
             *next > x;
             bit <<= 1, next[1] = next[0] * next[0], next++);
        
        for (; next >= exponents; bit >>= 1, next--)
            if (x < *next)
            {
                x /= *next;
                exponent |= bit;
            }
        
        exponent = -exponent;
    }
    
    *exp = exponent;
    return x;
}
#endif

#ifndef QUIET_UNUSED
/*!
 * Store EAQ with normalized dps8 representation of IEEE double f0 ...
 */
void IEEElongdoubleToEAQ(long double f0)
{
    if (f0 == 0)
    {
        rA = 0;
        rQ = 0;
        rE = (word8)-128;
        return;
    }
    
    bool sign = f0 < 0 ? true : false;
    long double f = fabsl(f0);
    
    int exp;
    long double mant = MYfrexpl(f, &exp);
    
    word72 result = 0;
    
    // now let's examine the mantissa and assign bits as necessary...
    if (sign && mant == 0.5)
    {
        result = bitfieldInsert72(result, 1, 63, 1);
        exp -= 1;
        mant -= 0.5;
    }

    long double bitval = 0.5;    ///< start at 1/2 and go down .....
    for(int n = 70 ; n >= 0 && mant > 0; n -= 1)
    {
        if (mant >= bitval)
        {
            result = bitfieldInsert72(result, 1, n, 1);
            mant -= bitval;
            //sim_printf ("Inserting a bit @ %d %012llo %012llo\n", n , (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
        }
        bitval /= 2.0;
    }
    
    // if f is < 0 then take 2-comp of result ...
    if (sign)
        result = -result & (((word72)1 << 72) - 1);
    
    rE = exp & 0377;
    rA = (result >> 36) & MASK36;
    rQ = result & MASK36;
}
#endif

#ifndef QUIET_UNUSED
/*!
 * return IEEE double version dps8 single-precision number ...
 */
double float36ToIEEEdouble(float36 f36)
{
    word8 E;    ///< exponent
    word36 M;   ///< mantissa
    //                         off len
    E = bitfieldExtract36(f36, 28, 8);      ///< 8-bit signed integer (incl sign)
    M = bitfieldExtract36(f36, 0, 28);      ///< 28-bit mantissa (incl sign)
    
    if (M == 0)
        return 0;
    
    bool S = M & 01000000000LL; ///< sign of mantissa
    if (S)
        M = (-M) & 0777777777; // 27 bits (not 28!)
    
    double m = 0;       ///< mantissa value;
    int8 e = (uint8)E;  ///< make signed
    
    double v = 0.5;
    for(int n = 26 ; n >= 0 ; n -= 1)
    {
        if (M & (1 << n))
        {
            m += v;
            //printf("1");
        }   //else
            //printf("0");
        v /= 2.0;
    }
    //printf("\n");
    //sim_printf ("s72=%012llo, E=%o M=%llo m=%f e=%d\n", f36, E, M, m, e);
    
    if (m == 0 && e == -128)    // special case - normalized 0
        return 0;
    if (m == 0)
        return (S ? -1 : 1) * exp2(e);
    
    return (S ? -1 : 1) * ldexp(m, e);
}
#endif

#ifndef QUIET_UNUSED
/*!
 * return normalized dps8 representation of IEEE double f0 ...
 */
float36 IEEEdoubleTofloat36(double f0)
{
    if (f0 == 0)
        return 0400000000000LL;
 
    double f = f0;
    bool sign = f0 < 0 ? true : false;
    if (sign)
        f = fabs(f0);
    
    int exp;
    double mant = frexp(f, &exp);
    
    //sim_printf (sign=%d f0=%f mant=%f exp=%d\n", sign, f0, mant, exp);
    
    word36 result = 0;
    
    // now let's examine the mantissa and assign bits as necessary...
    
    double bitval = 0.5;    ///< start at 1/2 and go down .....
    for(int n = 26 ; n >= 0 && mant > 0; n -= 1)
    {
        if (mant >= bitval)
        {
            result = bitfieldInsert36(result, 1, n, 1);
            mant -= bitval;
            //sim_printf ("Inserting a bit @ %d result=%012llo\n", n, result);
        }
        bitval /= 2.0;
    }
    //sim_printf ("result=%012llo\n", result);
    
    // if f is < 0 then take 2-comp of result ...
    if (sign)
    {
        result = -result & 001777777777LL;
        //sim_printf ("-result=%012llo\n", result);
    }
    // insert exponent ...
    int e = (int)exp;
    result = bitfieldInsert36(result, e, 28, 8) & 0777777777777LL;
    
    // XXX TODO test for exp under/overflow ...
    
    return result;
}
#endif

/*
 * single-precision arithmetic routines ...
 */

/*!
 * unnormalized floating single-precision add
 */
void ufa (void)
{
    //! C(EAQ) + C(Y) → C(EAQ)
    //! The ufa instruction is executed as follows:
    //! The mantissas are aligned by shifting the mantissa of the operand having the algebraically smaller exponent to the right the number of places equal to the absolute value of the difference in the two exponents. Bits shifted beyond the bit position equivalent to AQ71 are lost.
    //! The algebraically larger exponent replaces C(E). The sum of the mantissas replaces C(AQ).
    //! If an overflow occurs during addition, then;
    //! * C(AQ) are shifted one place to the right.
    //! * C(AQ)0 is inverted to restore the sign.
    //! * C(E) is increased by one.

    float72 m1 = ((word72)rA << 36) | (word72)rQ;
    float72 op2 = CY;
            
    int8   e1 = (int8)rE; 
    
    int8   e2 = (int8)(bitfieldExtract36(op2, 28, 8) & 0377U);      ///< 8-bit signed integer (incl sign)
    word72 m2 = (word72)bitfieldExtract36(op2, 0, 28) << 44; ///< 28-bit mantissa (incl sign)
    
    int8 e3 = -1;
    word72 m3 = 0;
    
//    if (m1 == 0) // op1 is 0
//    {
//        m3 = m2;
//        e3 = e2;
//        goto here;
//    }
//    if (m2 == 0) // op2 is 0
//    {
//        m3 = m1;
//        e3 = e1;
//        goto here;
//    }

    //which exponent is smaller???
    
 
    int shift_count = -1;
    
    if (e1 == e2)
    {
        shift_count = 0;
        e3 = e1;
    }
    else if (e1 < e2)
    {
        shift_count = abs(e2 - e1);
        bool s = m1 & SIGN72;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m1 >>= 1;
            if (s)
                m1 |= SIGN72;
        }
        
        m1 &= MASK72;
        e3 = e2;
    }
    else
    {
        // e2 < e1;
        shift_count = abs(e1 - e2);
        bool s = m2 & SIGN72;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m2 >>= 1;
            if (s)
                m2 |= SIGN72;
        }
        m2 &= MASK72;
        e3 = e1;
    }
    //sim_printf ("shift_count = %d\n", shift_count);
    
    m3 = m1 + m2;
    
    /*
     oVerflow rules .....
     
     oVerflow occurs for addition if the operands have the
     same sign and the result has a different sign. MSB(a) = MSB(b) and MSB(r) <> MSB(a)
     */
    bool ov = ((m1 & SIGN72) == (m2 & SIGN72)) && ((m1 & SIGN72) != (m3 & SIGN72)) ;
    
    //ov = m3 & 077000000000000LL;
    if (ov)
    {
        m3 >>= 1;
        m3 &= MASK72; /// MAY need to preserve sign not just set it
        e3 += 1;
    }
    
//here:;
    // Carry: If a carry out of AQ0 is generated, then ON; otherwise OFF
    SCF(m3 > MASK72, cu.IR, I_CARRY);
    
    // Zero: If C(AQ) = 0, then ON; otherwise OFF
    SCF(m3 == 0, cu.IR, I_ZERO);
    
    // Neg: If C(AQ)0 = 1, then ON; otherwise OFF
    SCF(m3 & SIGN72, cu.IR, I_NEG);

    // EOFL: If exponent is greater than +127, then ON
    if (e3 > 127)
        SETF(cu.IR, I_EOFL);
    
    // EUFL: If exponent is less than -128, then ON
    if(e3 < -128)
        SETF(cu.IR, I_EUFL);
    
    if (m3 == 0)
    {
        rE = (word8)-128;
        rA = 0;
        rQ = 0;
    }
    else
    {
        rE = e3 & 0377;
        rA = (m3 >> 36) & MASK36;
        rQ = m3 & MASK36;
    }
    
}

/*!
 * unnormalized floating single-precision subtract
 */
void ufs (void)
{
    //! The ufs instruction is identical to the ufa instruction with the exception that the twos complement of the mantissa of the operand from main memory (op2) is used.
    
    //! They're probably a few gotcha's here but we'll see.
    //! Yup ... when mantissa 1 000 000 .... 000 we can't do 2'c comp.
    
    word36 m2 = bitfieldExtract36(CY, 0, 28) & FLOAT36MASK; ///< 28-bit mantissa (incl sign)
    
    // -1  001000000000 = 2^0 * -1 = -1
    // S          S    
    // 000 000 00 1 000 000 000 000 000 000 000 000 000
    // eee eee ee m mmm mmm mmm mmm mmm mmm mmm mmm mmm
    //      ~     0 111 111 111 111 111 111 111 111 111
    // (add 1)
    //            1 000 000 000 000 000 000 000 000 000  <== Oops! The "weird" number problem.
    //
    // +1 002400000000
    // 000 000 01 0 100 000 000 000 000 000 000 000 000 // what we wan't
    //
    
    word36 m2c = ~m2 + 1;     ///< & 01777777777LL;     ///< take 2-comp of mantissa
    m2c &= FLOAT36MASK;
   
    /*
     * When signs are the *same* after complement we have an overflow
     * (Treat as in addition when we get an overflow)
     */
    bool ov = ((m2 & 01000000000LL) == (m2c & 01000000000LL)); // the "weird" number.

    if (ov && m2 != 0)
    {
        //sim_printf ("OV\n");
        int8   e = (int8)(bitfieldExtract36(CY, 28, 8) & 0377U);      ///< 8-bit signed integer (incl sign)

        m2c >>= 1;
        m2c &= FLOAT36MASK;
        
        if ((e + 1) > 127)
            SETF(cu.IR, I_EOFL);
        else // XXX my interpretation
            e += 1;
        
        CY = bitfieldInsert36(CY, (word36)e, 28, 8) & DMASK;
        
    }

    if (m2c == 0)
        CY = 0400000000000LL;
    else
        CY = bitfieldInsert36(CY, m2c & FLOAT36MASK, 0, 28) & MASK36;
   
    ufa();

}

/*!
 * floating normalize ...
 */

void fno (void)
{
    //! The fno instruction normalizes the number in C(EAQ) if C(AQ) ≠ 0 and the overflow indicator is OFF.
    //!    A normalized floating number is defined as one whose mantissa lies in the interval [0.5,1.0) such that
    //!    0.5<= |C(AQ)| <1.0 which, in turn, requires that C(AQ)0 ≠ C(AQ)1.
    //!    If the overflow indicator is ON, then C(AQ) is shifted one place to the right, C(AQ)0 is inverted to reconstitute the actual sign, and the overflow indicator is set OFF. This action makes the fno instruction useful in correcting overflows that occur with fixed point numbers.
    //!  Normalization is performed by shifting C(AQ)1,71 one place to the left and reducing C(E) by 1, repeatedly, until the conditions for C(AQ)0 and C(AQ)1 are met. Bits shifted out of AQ1 are lost.
    //!  If C(AQ) = 0, then C(E) is set to -128 and the zero indicator is set ON.
    
    rA &= DMASK;
    rQ &= DMASK;
    float72 m = ((word72)rA << 36) | (word72)rQ;
    if (TSTF(cu.IR, I_OFLOW))
    {
        m >>= 1;
        m &= MASK72;
        
        m ^= ((word72)1 << 71);

        SCF(rA && SIGN72, cu.IR, I_NEG);
        CLRF(cu.IR, I_OFLOW);
        
        // Zero: If C(AQ) = floating point 0, then ON; otherwise OFF
        //SCF(rE == -128 && m == 0, cu.IR, I_ZERO);
        SCF(rE == 0200U /*-128*/ && m == 0, cu.IR, I_ZERO);
        // Neg:
        CLRF(cu.IR, I_NEG);
        return; // XXX: ???
    }
    
    // only normalize C(EAQ) if C(AQ) ≠ 0 and the overflow indicator is OFF
    if (m == 0) // C(AQ) == 0.
    {
        rA = (m >> 36) & MASK36;
        rQ = m & MASK36;

        // Zero: If C(AQ) = floating point 0, then ON; otherwise OFF
        //SCF(rE == -128 && m == 0, cu.IR, I_ZERO);
        SCF(rE == 0200U /*-128*/ && m == 0, cu.IR, I_ZERO);
        // Neg:
        CLRF(cu.IR, I_NEG);
        
        return;
    }
    int8   e = (int8)rE;

    bool s = (m & SIGN72) != (word72)0;    ///< save sign bit
    //while ((bool)(m & SIGN72) == (bool)(m & (SIGN72 >> 1))) // until C(AQ)0 ≠ C(AQ)1?
    while (bitfieldExtract72(m, 71, 1) == bitfieldExtract72(m, 70, 1)) // until C(AQ)0 ≠ C(AQ)1?
    {
        m <<= 1;
        m &= MASK72;
        
        if (s)
            m |= SIGN72;
        
        if ((e - 1) < -128)
            SETF(cu.IR, I_EOFL);
        else    // XXX: my interpretation
            e -= 1;
        
        if (m == 0) // XXX: necessary??
        {
            rE = (word8)-128;
            break;
        }
    }
      
    rE = e & 0377;
    rA = (m >> 36) & MASK36;
    rQ = m & MASK36;

    if (rA == 0)    // set to normalized 0
        rE = (word8)-128;
    
    // Zero: If C(AQ) = floating point 0, then ON; otherwise OFF
    SCF(rA == 0, cu.IR, I_ZERO);
    
    // Neg: If C(AQ)0 = 1, then ON; otherwise OFF
    SCF(rA & SIGN36, cu.IR, I_NEG);

}

// XXX eventually replace fno() with fnoEAQ()
void fnoEAQ(word8 *E, word36 *A, word36 *Q)
{
    //! The fno instruction normalizes the number in C(EAQ) if C(AQ) ≠ 0 and the overflow indicator is OFF.
    //!    A normalized floating number is defined as one whose mantissa lies in the interval [0.5,1.0) such that
    //!    0.5<= |C(AQ)| <1.0 which, in turn, requires that C(AQ)0 ≠ C(AQ)1.
    //!    If the overflow indicator is ON, then C(AQ) is shifted one place to the right, C(AQ)0 is inverted to reconstitute the actual sign, and the overflow indicator is set OFF. This action makes the fno instruction useful in correcting overflows that occur with fixed point numbers.
    //!  Normalization is performed by shifting C(AQ)1,71 one place to the left and reducing C(E) by 1, repeatedly, until the conditions for C(AQ)0 and C(AQ)1 are met. Bits shifted out of AQ1 are lost.
    //!  If C(AQ) = 0, then C(E) is set to -128 and the zero indicator is set ON.
    
    float72 m = ((word72)*A << 36) | (word72)*Q;
    if (TSTF(cu.IR, I_OFLOW))
    {
        m >>= 1;
        m &= MASK72;
        
        m ^= ((word72)1 << 71);
        
        CLRF(cu.IR, I_OFLOW);
        
        // Zero: If C(AQ) = floating point 0, then ON; otherwise OFF
        //SCF(*E == -128 && m == 0, cu.IR, I_ZERO);
        SCF(*E == 0200U /*-128*/ && m == 0, cu.IR, I_ZERO);
        // Neg:
        CLRF(cu.IR, I_NEG);
        return; // XXX: ???
    }
    
    // only normalize C(EAQ) if C(AQ) ≠ 0 and the overflow indicator is OFF
    if (m == 0) // C(AQ) == 0.
    {
        *A = (m >> 36) & MASK36;
        *Q = m & MASK36;
        
        // Zero: If C(AQ) = floating point 0, then ON; otherwise OFF
        //SCF(*E == -128 && m == 0, cu.IR, I_ZERO);
        SCF(*E == 0200U /*-128*/ && m == 0, cu.IR, I_ZERO);
        // Neg:
        CLRF(cu.IR, I_NEG);
        
        return;
    }
    int8   e = (int8)*E;
    
    
    bool s = m & SIGN72;    ///< save sign bit
    while ((bool)(m & SIGN72) == (bool)(m & (SIGN72 >> 1))) // until C(AQ)0 ≠ C(AQ)1?
    {
        m <<= 1;
        m &= MASK72;
        
        if (s)
            m |= SIGN72;
        
        if ((e - 1) < -128)
            SETF(cu.IR, I_EOFL);
        else    // XXX: my interpretation
            e -= 1;
        
        if (m == 0) // XXX: necessary??
        {
            *E = (word8)-128;
            break;
        }
    }
    
    *E = e & 0377;
    *A = (m >> 36) & MASK36;
    *Q = m & MASK36;
    
    if (*A == 0)    // set to normalized 0
        *E = (word8)-128;
    
    // Zero: If C(AQ) = floating point 0, then ON; otherwise OFF
    SCF(*A == 0, cu.IR, I_ZERO);
    
    // Neg: If C(AQ)0 = 1, then ON; otherwise OFF
    SCF(*A & SIGN36, cu.IR, I_NEG);
    
}

/*!
 * floating negate ...
 */
void fneg (void)
{
    //! This instruction changes the number in C(EAQ) to its normalized negative (if C(AQ) ≠ 0). The operation is executed by first forming the twos complement of C(AQ), and then normalizing C(EAQ).
    //! Even if originally C(EAQ) were normalized, an exponent overflow can still occur, namely when C(E) = +127 and C(AQ) = 100...0 which is the twos complement approximation for the decimal value -1.0.
    
    float72 m = ((word72)rA << 36) | (word72)rQ;
    
    if (m == 0) // (if C(AQ) ≠ 0)
    {
        SETF(cu.IR, I_ZERO);      // it's zero
        CLRF(cu.IR, I_NEG);       // it ain't negative
        return; //XXX: ????
    }
    
    int8 e = (int8)rE;
    
    word72 mc = 0;
    if (m == FLOAT72SIGN)
    {
        mc = m >> 1;
        e += 1;
    } else
        mc = ~m + 1;     // take 2-comp of mantissa
    
    
    //mc &= FLOAT72MASK;
    mc &= ((word72)1 << 72) - 1;
    /*
     * When signs are the *same* we have an overflow
     * (Treat as in addition when we get an overflow)
     */
    //bool ov = ((m & FLOAT72SIGN) == (mc & FLOAT72SIGN)); // the "weird" number!
    bool ov = ((m & ((word72)1 << 71)) == (mc & ((word72)1 << 71))); // the "weird" number!
//    if (ov)
//    bool ov = ((m & 01000000000LL) == (mc & 01000000000LL)); // the "weird" number.
    if (ov && m != 0)
    {
        mc >>= 1;
        //mc &= FLOAT72MASK;
        mc &= ((word72)1 << 72) - 1;
        
        if ((e + 1) > 127)
            SETF(cu.IR, I_EOFL);
        else    // XXX: this is my interpretation
            rE += 1;
    }
    
    rE = e & 0377;
    rA = (mc >> 36) & MASK36;
    rQ = mc & MASK36;

    fno ();  // normalize
}

/*!
 * Unnormalized Floating Multiply ...
 */
void ufm (void)
{
    //! The ufm instruction is executed as follows:
    //!      C(E) + C(Y)0,7 → C(E)
    //!      ( C(AQ) × C(Y)8,35 )0,71 → C(AQ)
    //! A normalization is performed only in the case of both factor mantissas being 100...0 which is the twos  complement approximation to the decimal value -1.0.
    //! The definition of normalization is located under the description of the fno instruction.
    
    //! Zero: If C(AQ) = 0, then ON; otherwise OFF
    //! Neg: If C(AQ)0 = 1, then ON; otherwise OFF
    //! Exp Ovr: If exponent is greater than +127, then ON
    //! Exp Undr: If exponent is less than -128, then ON
    
    uint64 m1 = (rA << 28) | ((rQ & 0777777777400LL) >> 8) ; 
    int8   e1 = (int8)rE;

    uint64 m2 = bitfieldExtract36(CY, 0, 28) << (8 + 28); ///< 28-bit mantissa (incl sign)
    int8   e2 = (int8)(bitfieldExtract36(CY, 28, 8) & 0377U);      ///< 8-bit signed integer (incl sign)
    
    if (m1 == 0 || m2 == 0)
    {
        SETF(cu.IR, I_ZERO);
        CLRF(cu.IR, I_NEG);
        
        rE = (word8)-128;
        rA = 0;
        rQ = 0;
        
        return; // normalized 0
    }

    int sign = 1;

    if (m1 & FLOAT72SIGN)   //0x8000000000000000LL)
    {
        if (m1 == FLOAT72SIGN)
        {
            m1 >>= 1;
            e1 += 1;
        }
        else
            m1 = (~m1 + 1);
        sign = -sign;
    }

    if (m2 & FLOAT72SIGN)   //0x8000000000000000LL)
    {
        if (m1 == FLOAT72SIGN)
        {
            m2 >>= 1;
            e2 += 1;
        }
        else
            m2 = (~m2 + 1);
        sign = -sign;
    }
    
    int8 e3 = e1 + e2;
    
    word72 m3 = ((word72)m1) * ((word72)m2);
    word72 m3a = m3 >> 63;
    
    if (sign == -1)
        m3a = (~m3a + 1) & 0xffffffffffffffffLL;
    
    rE = e3 & 0377;
    rA = (m3a >> 28) & MASK36;
    rQ = m3a & MASK36;
    
    // A normalization is performed only in the case of both factor mantissas being 100...0 which is the twos complement approximation to the decimal value -1.0.
    //if ((rE == -128 && rA == 0 && rQ == 0) && (m2 == 0 && e2 == -128)) // XXX FixMe
    if ((m1 == ((uint64)1 << 63)) && (m2 == ((uint64)1 << 63)))
        fno ();
    
    SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
    //SCF(rA && SIGN72, cu.IR, I_NEG);
    SCF(rA && SIGN36, cu.IR, I_NEG);
    
    if (e1 + e2 >  127) SETF(cu.IR, I_EOFL);
    if (e1 - e2 < -128) SETF(cu.IR, I_EUFL);
}

/*!
 * floating divide ...
 */
// CANFAULT
static void fdvX(bool bInvert)
{
    //! C(EAQ) / C (Y) → C(EA)
    //! C(Y) / C(EAQ) → C(EA) (Inverted)
    
    //! 00...0 → C(Q)
    
    //! The fdv instruction is executed as follows:
    //! The dividend mantissa C(AQ) is shifted right and the dividend exponent
    //! C(E) increased accordingly until | C(AQ)0,27 | < | C(Y)8,35 |
    //! C(E) - C(Y)0,7 → C(E)
    //! C(AQ) / C(Y)8,35 → C(A)
    //! 00...0 → C(Q)
    //! If the divisor mantissa C(Y)8,35 is zero after alignment, the division does
    //! not take place. Instead, a divide check fault occurs, C(AQ) contains the dividend magnitude, and the negative indicator reflects the dividend sign.
  
    word36 m1;
    int8   e1;
    
    word36 m2;
    int8   e2;
    
    if (!bInvert)
    {
        m1 = rA;    // & 0777777777400LL;
        e1 = (int8)rE;
    
        m2 = bitfieldExtract36(CY, 0, 28) << 8 ;     // 28-bit mantissa (incl sign)
        e2 = (int8)(bitfieldExtract36(CY, 28, 8) & 0377U);    // 8-bit signed integer (incl sign)
    } else { // invert
        m2 = rA;    //& 0777777777400LL ;
        e2 = (int8)rE;
    
        m1 = bitfieldExtract36(CY, 0, 28) << 8 ;     // 28-bit mantissa (incl sign)
        e1 = (int8) (bitfieldExtract36(CY, 28, 8) & 0377U);    // 8-bit signed integer (incl sign)
    }

    // make everything positive, but save sign info for later....
    int sign = 1;
    if (m1 & SIGN36)
    {
        if (m1 == SIGN36)
        {
            m1 >>= 1;
            e1 += 1;
        } else
            m1 = (~m1 + 1) & 0777777777777;
        sign = -sign;
    }
    
    if (m2 & SIGN36)
    {
        if (m2 == SIGN36)
        {
            m2 >>= 1;
            e2 += 1;
        } else
        m2 = (~m2 + 1) & 0777777777777;
        sign = -sign;
    }
    
    if (m2 == 0)
    {
        // If the divisor mantissa C(Y)8,35 is zero after alignment (HWR: why after?), the division does
        // not take place. Instead, a divide check fault occurs, C(AQ) contains the dividend magnitude, and the negative indicator reflects the dividend sign.
        
        // NB: If C(Y)8,35 ==0 then the alignment loop will never exit! That's why it been moved before the alignment
        
        SETF(cu.IR, I_ZERO);
        SCF(rA & SIGN36, cu.IR, I_NEG);
        
        rA = m1;
        
        doFault(div_fault, 0, "DFDV: divide check fault");
    }

    while (m1 >= m2)
    {
        m1 >>= 1;
        
        if (e1 + 1 > 127)
            SETF(cu.IR, I_EOFL);
        else // XXX: this is my interpretation
            e1 += 1;
    }
    
        
    int8 e3 = e1 - e2;
        
    word72 m3 = (((word72)m1) << 35) / ((word72)m2);
    word36 m3b = m3 & (0777777777777LL);
    
    if (sign == -1)
        m3b = (~m3b + 1) & 0777777777777LL;
    
    rE = e3 & 0377;
    rA = m3b & MASK36;
    rQ = 0;
    
    SCF(rA == 0, cu.IR, I_ZERO);
    SCF(rA & SIGN36, cu.IR, I_NEG);
    
    if (rA == 0)    // set to normalized 0
        rE = (word8)-128;
}

void fdv (void)
{
    fdvX(false);    // no inversion
}
void fdi (void)
{
    fdvX(true);
}

/*!
 * single precision floating round ...
 */
void frd (void)
{
    //! If C(AQ) ≠ 0, the frd instruction performs a true round to a precision of 28 bits and a normalization on C(EAQ).
    //! A true round is a rounding operation such that the sum of the result of applying the operation to two numbers of equal magnitude but opposite sign is exactly zero.
    
    //! The frd instruction is executed as follows:
    //! C(AQ) + (11...1)29,71 → C(AQ)
    //! If C(AQ)0 = 0, then a carry is added at AQ71
    //! If overflow occurs, C(AQ) is shifted one place to the right and C(E) is increased by 1.
    //! If overflow does not occur, C(EAQ) is normalized.
    //! If C(AQ) = 0, C(E) is set to -128 and the zero indicator is set ON.
    
    //! I believe AL39 is incorrect; bits 28-71 should be set to 0, not 29-71. DH02-01 & Bull 9000 is correct.
    
    //! test case 15.5
    //!                 rE                     rA                                     rQ
    //! 014174000000 00000110 000111110000000000000000000000000000 000000000000000000000000000000000000
    //! +                                                  1111111 111111111111111111111111111111111111
    //! =            00000110 000111110000000000000000000001111111 111111111111111111111111111111111111
    //! If C(AQ)0 = 0, then a carry is added at AQ71
    //! =            00000110 000111110000000000000000000010000000 000000000000000000000000000000000000
    //! 0 → C(AQ)29,71
    //!              00000110 000111110000000000000000000010000000 000000000000000000000000000000000000
    //! after normalization .....
    //! 010760000002 00000100 011111000000000000000000001000000000 000000000000000000000000000000000000
    //! I think this is wrong
    
    //! 0 → C(AQ)28,71
    //!              00000110 000111110000000000000000000000000000 000000000000000000000000000000000000
    //! after normalization .....
    //! 010760000000 00000100 011111000000000000000000000000000000 000000000000000000000000000000000000
    //! which I think is correct
    
    //!
    //! GE CPB1004F, DH02-01 (DPS8/88) & Bull DPS9000 assembly ... have this ...
    
    //! The rounding operation is performed in the following way.
    //! -  a) A constant (all 1s) is added to bits 29-71 of the mantissa.
    //! -  b) If the number being rounded is positive, a carry is inserted into the least significant bit position of the adder.
    //! -  c) If the number being rounded is negative, the carry is not inserted.
    //! -  d) Bits 28-71 of C(AQ) are replaced by zeros.
    //! If the mantissa overflows upon rounding, it is shifted right one place and a corresponding correction is made to the exponent.
    //! If the mantissa does not overflow and is nonzero upon rounding, normalization is performed.
    
    //! If the resultant mantissa is all zeros, the exponent is forced to -128 and the zero indicator is set.
    //! If the exponent resulting from the operation is greater than +127, the exponent Overflow indicator is set.
    //! If the exponent resulting from the operation is less than -128, the exponent Underflow indicator is set.
    //! The definition of normalization is located under the description of the FNO instruction.
    
    //! So, Either AL39 is wrong or the DPS8m did it wrong. (Which was fixed in later models.) I'll assume AL39 is wrong.
    
    float72 m = ((word72)rA << 36) | (word72)rQ;
    if (m == 0)
    {
        rE = (word8)-128;
        SETF(cu.IR, I_ZERO);
        CLRF(cu.IR, I_NEG);
        
        return;
    }
    
    // C(AQ) + (11...1)29,71 → C(AQ)
    bool s1 = m & SIGN72;
    
    m += (word72)0177777777777777LL; // add 1's into lower 43-bits
        
    // If C(AQ)0 = 0, then a carry is added at AQ71
    if (!s1)
        m += 1;
   
    // 0 → C(AQ)29,71 (AL39)
    // 0 → C(AQ)28,71 (DH02-01 / DPS9000)
    //m &= (word72)0777777777400LL << 36; // 28-71 => 0 per DH02-01/Bull DPS9000
    m = bitfieldInsert72(m, 0, 0, 44);    // 28-71 => 0 per DH02
    
    bool s2 = (bool)(m & SIGN72);
    
    bool ov = s1 != s2;   // sign change denotes overflow
    if (ov)
    {
        // If overflow occurs, C(AQ) is shifted one place to the right and C(E) is increased by 1.
        m >>= 1;
        if (s1) // (was s2) restore sign if necessary
            m |= SIGN72;
        
        rA = (m >> 36) & MASK36;
        rQ = m & MASK36;
        
        if (rE + 1 > 127)
            SETF(cu.IR, I_EOFL);
        rE +=  1;
    }
    else
    {
        // If overflow does not occur, C(EAQ) is normalized.
        rA = (m >> 36) & MASK36;
        rQ = m & MASK36;
        
        fno ();
    }
    
    // If C(AQ) = 0, C(E) is set to -128 and the zero indicator is set ON.
    if (rA == 0 && rQ == 0)
    {
        rE = (word8)-128;
        SETF(cu.IR, I_ZERO);
    }
    
    SCF(rA & SIGN36, cu.IR, I_NEG);
    
}

void fstr(word36 *Y)
{
    //The fstr instruction performs a true round and normalization on C(EAQ) as it is stored.
    //The definition of true round is located under the description of the frd instruction.
    //The definition of normalization is located under the description of the fno instruction.
    //Attempted repetition with the rpl instruction causes an illegal procedure fault.
    
    word36 A = rA, Q = rQ;
    word8 E = rE;
    A &= DMASK;
    Q &= DMASK;
    E &= MASK8;
   
    float72 m = ((word72)A << 36) | (word72)Q;
    if (m == 0)
    {
        E = (word8)-128;
        SETF(cu.IR, I_ZERO);
        CLRF(cu.IR, I_NEG);
        
        *Y = bitfieldInsert36(A >> 8, E, 28, 8) & MASK36;
        return;
    }
    
    // C(AQ) + (11...1)29,71 → C(AQ)
    bool s1 = (m & SIGN72) != (word72)0;
    
    m += (word72)0177777777777777LL; // add 1's into lower 43-bits
    
    // If C(AQ)0 = 0, then a carry is added at AQ71
    if (!s1)
        m += 1;
    
    // 0 → C(AQ)29,71 (AL39)
    // 0 → C(AQ)28,71 (DH02-01 / DPS9000)
    //m &= (word72)0777777777400LL << 36; // 28-71 => 0 per DH02-01/Bull DPS9000
    m = bitfieldInsert72(m, 0, 0, 44);    // 28-71 => 0 per DH02
    
    bool s2 = (m & SIGN72) != (word72)0;
    
    bool ov = s1 != s2;   // sign change denotes overflow
    if (ov)
    {
        // If overflow occurs, C(AQ) is shifted one place to the right and C(E) is increased by 1.
        m >>= 1;
        if (s1) // (was s2) restore sign if necessary
            m |= SIGN72;
        
        A = (m >> 36) & MASK36;
        Q = m & MASK36;
        
        if (E + 1 > 127)
            SETF(cu.IR, I_EOFL);
        E +=  1;
    }
    else
    {
        // If overflow does not occur, C(EAQ) is normalized.
        A = (m >> 36) & MASK36;
        Q = m & MASK36;
        
        fnoEAQ(&E, &A, &Q);
    }
    
    // If C(AQ) = 0, C(E) is set to -128 and the zero indicator is set ON.
    if (A == 0 && Q == 0)
    {
        E = (word8)-128;
        SETF(cu.IR, I_ZERO);
    }
    
    SCF(A & SIGN36, cu.IR, I_NEG);
    
    *Y = bitfieldInsert36(A >> 8, E, 28, 8) & MASK36;
}

/*!
 * single precision Floating Compare ...
 */
void fcmp(void)
{
    //! C(E) :: C(Y)0,7
    //! C(AQ)0,27 :: C(Y)8,35
    
    //! Zero: If C(EAQ) = C(Y), then ON; otherwise OFF
    //! Neg: If C(EAQ) < C(Y), then ON; otherwise OFF
    
    //! Notes: The fcmp instruction is executed as follows:
    //! The mantissas are aligned by shifting the mantissa of the operand with the algebraically smaller exponent to the right the number of places equal to the difference in the two exponents.
    //! The aligned mantissas are compared and the indicators set accordingly.
    
    word36 m1 = rA & 0777777777400LL;
    int8   e1 = (int8)rE;
    
    word36 m2 = bitfieldExtract36(CY, 0, 28) << 8;      ///< 28-bit mantissa (incl sign)
    int8   e2 = (int8) (bitfieldExtract36(CY, 28, 8) & 0377U);    ///< 8-bit signed integer (incl sign)
    
    //int8 e3 = -1;
       
    //which exponent is smaller???
    
    int shift_count = -1;
    
    if (e1 == e2)
    {
        shift_count = 0;
        //e3 = e1;
    }
    else
    if (e1 < e2)
    {
        shift_count = abs(e2 - e1);
        bool s = m1 & SIGN36;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m1 >>= 1;
            if (s)
                m1 |= SIGN36;
        }
        
        m1 &= MASK36;
        //e3 = e2;
    }
    else
    {
        // e2 < e1;
        shift_count = abs(e1 - e2);
        bool s = m2 & SIGN36;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m2 >>= 1;
            if (s)
                m2 |= SIGN36;
        }
        m2 &= MASK36;
        //e3 = e1;
    }
    
    // need todo algebraic comparisons of mantissae
    SCF((word36s)SIGNEXT36(m1) == (word36s)SIGNEXT36(m2), cu.IR, I_ZERO);
    SCF((word36s)SIGNEXT36(m1) <  (word36s)SIGNEXT36(m2), cu.IR, I_NEG);
}

/*!
 * single precision Floating Compare magnitude ...
 */
void fcmg ()
{
    //! C(E) :: C(Y)0,7
    //! | C(AQ)0,27 | :: | C(Y)8,35 |
    //! * Zero: If | C(EAQ)| = | C(Y) |, then ON; otherwise OFF
    //! * Neg : If | C(EAQ)| < | C(Y) |, then ON; otherwise OFF
    
    //! Notes: The fcmp instruction is executed as follows:
    //! The mantissas are aligned by shifting the mantissa of the operand with the algebraically smaller exponent to the right the number of places equal to the difference in the two exponents.
    //! The aligned mantissas are compared and the indicators set accordingly.
    
    word36 m1 = rA & 0777777777400LL;
    int8   e1 = (int8)rE;
    
    word36 m2 = bitfieldExtract36(CY, 0, 28) << 8;      ///< 28-bit mantissa (incl sign)
    int8   e2 = (int8) (bitfieldExtract36(CY, 28, 8) & 0377U);    ///< 8-bit signed integer (incl sign)
    
    //int8 e3 = -1;
    
    //which exponent is smaller???
    
    int shift_count = -1;
    
    if (e1 == e2)
    {
        shift_count = 0;
        //e3 = e1;
    }
    else
    if (e1 < e2)
    {
        shift_count = abs(e2 - e1);
        bool s = m1 & SIGN36;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m1 >>= 1;
            if (s)
                m1 |= SIGN36;
        }
        
        m1 &= MASK36;
        //e3 = e2;
    }
    else
    {
        // e2 < e1;
        shift_count = abs(e1 - e2);
        bool s = m2 & SIGN36;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m2 >>= 1;
            if (s)
                m2 |= SIGN36;
        }
        m2 &= MASK36;
        //e3 = e1;
    }
    
    // fetch magnitudes of mantissae
    if (m1 & SIGN36)
        m1 = (~m1 + 1) & MASK36;
    
    if (m2 & SIGN36)
        m2 = (~m2 + 1) & MASK36;
    
    SCF(m1 == m2, cu.IR, I_ZERO);
    SCF(m1 < m2, cu.IR, I_NEG);
}

/*
 * double-precision arithmetic routines ....
 */

//! extract mantissa + exponent from a YPair ....
static void YPairToExpMant(word36 Ypair[], word72 *mant, int8 *exp)
{
    *mant = (word72)bitfieldExtract36(Ypair[0], 0, 28) << 44;   // 28-bit mantissa (incl sign)
    *mant |= (Ypair[1] & DMASK) << 8;
    *exp = (int8) (bitfieldExtract36(Ypair[0], 28, 8) & 0377U);           // 8-bit signed integer (incl sign)
}

//! combine mantissa + exponent intoa YPair ....
static void ExpMantToYpair(word72 mant, int8 exp, word36 *yPair)
{
    yPair[0] = (word36)exp << 28;
    yPair[0] |= (mant >> 44) & 01777777777LL;
    yPair[1] = (mant >> 8) & 0777777777777LL;   //400LL;
}


/*!
 * unnormalized floating double-precision add
 */
void dufa (void)
{
    //! Except for the precision of the mantissa of the operand from main memory,
    //! the dufa instruction is identical to the ufa instruction.
    
    //! C(EAQ) + C(Y) → C(EAQ)
    //! The ufa instruction is executed as follows:
    //! The mantissas are aligned by shifting the mantissa of the operand having the algebraically smaller exponent to the right the number of places equal to the absolute value of the difference in the two exponents. Bits shifted beyond the bit position equivalent to AQ71 are lost.
    //! The algebraically larger exponent replaces C(E). The sum of the mantissas replaces C(AQ).
    //! If an overflow occurs during addition, then;
    //! *  C(AQ) are shifted one place to the right.
    //! *  C(AQ)0 is inverted to restore the sign.
    //! *  C(E) is increased by one.
    
    rA &= DMASK;
    rQ &= DMASK;
    rE &= MASK8;

    float72 m1 = ((word72)rA << 36) | (word72)rQ;
    int8   e1 = (int8)rE;
    
    float72 m2 = 0;
    int8 e2 = -128;
    
    YPairToExpMant(Ypair, &m2, &e2);
    
    int8 e3 = -1;
    word72 m3 = 0;
    
    //which exponent is smaller???
    
    int shift_count = -1;
    
    if (e1 == e2)
    {
        shift_count = 0;
        e3 = e1;
    }
    else if (e1 < e2)
    {
        shift_count = abs(e2 - e1);
        bool s = m1 & SIGN72;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m1 >>= 1;
            if (s)
                m1 |= SIGN72;
        }
        
        m1 &= MASK72;
        e3 = e2;
    }
    else
    {
        // e2 < e1;
        shift_count = abs(e1 - e2);
        bool s = m2 & SIGN72;   ///< mantissa negative?
        for(int n = 0 ; n < shift_count ; n += 1)
        {
            m2 >>= 1;
            if (s)
                m2 |= SIGN72;
        }
        m2 &= MASK72;
        e3 = e1;
    }
    //sim_printf ("shift_count = %d\n", shift_count);
    
    m3 = m1 + m2;
    
    /*
     oVerflow rules .....
     
     oVerflow occurs for addition if the operands have the
     same sign and the result has a different sign. MSB(a) = MSB(b) and MSB(r) <> MSB(a)
     */
    bool ov = ((m1 & SIGN72) == (m2 & SIGN72)) && ((m1 & SIGN72) != (m3 & SIGN72)) ;
    
    //ov = m3 & 077000000000000LL;
    if (ov)
    {
        m3 >>= 1;
        m3 &= MASK72; /// MAY need to preserve sign not just set it
        e3 += 1;
    }
    
    //here:;
    // Carry: If a carry out of AQ0 is generated, then ON; otherwise OFF
    SCF(m3 > MASK72, cu.IR, I_CARRY);
    
    // Zero: If C(AQ) = 0, then ON; otherwise OFF
    SCF(m3 == 0, cu.IR, I_ZERO);
    
    // Neg: If C(AQ)0 = 1, then ON; otherwise OFF
    SCF(m3 & SIGN72, cu.IR, I_NEG);
    
    // EOFL: If exponent is greater than +127, then ON
    if (e3 > 127)
        SETF(cu.IR, I_EOFL);
    
    // EUFL: If exponent is less than -128, then ON
    if(e3 < -128)
        SETF(cu.IR, I_EUFL);
    
    if (m3 == 0)
    {
        rE = (word8)-128;
        rA = 0;
        rQ = 0;
    }
    else
    {
        rE = e3 & 0377;
        rA = (m3 >> 36) & MASK36;
        rQ = m3 & MASK36;
    }
}

/*!
 * unnormalized floating double-precision subtract
 */
void dufs (void)
{
    //! Except for the precision of the mantissa of the operand from main memory,
    //! the dufs instruction is identical with the ufs instruction.
    
    //! The ufs instruction is identical to the ufa instruction with the exception that the twos complement of the mantissa of the operand from main memory (op2) is used.
    
    //! They're probably a few gotcha's here but we'll see.
    //! Yup ... when mantissa 1 000 000 .... 000 we can't do 2'c comp.
    
    float72 m2 = 0;
    int8 e2 = -128;
    
    YPairToExpMant(Ypair, &m2, &e2);
    
    word72 m2c = ~m2 + 1;     ///< & 01777777777LL;     ///< take 2-comp of mantissa
    m2c &= MASK72;
    
    /*!
     * When signs are the *same* after complement we have an overflow
     * (Treat as in addition when we get an overflow)
     */
    bool ov = ((m2 & SIGN72) == (m2c & SIGN72)); ///< the "weird" number.
    
    if (ov && m2 != 0)
    {
        m2c >>= 1;
        m2c &= MASK72;
        
        if ((e2 + 1) > 127)
            SETF(cu.IR, I_EOFL);
        else // XXX my interpretation
            e2 += 1;
    }
    
    if (m2c == 0)
    {
//      Ypair[0] = 0400000000000LL;
//      Ypair[1] = 0;
        ExpMantToYpair(0, -128, Ypair);
    }
    else
    {
//        Ypair[0]  = ((word36)e2 & 0377) << 28;
//        Ypair[0] |= (m2c >> 44) & 01777777777LL;
//        Ypair[1]  = (m2c >> 8) &  0777777777400LL;
        ExpMantToYpair(m2c, e2, Ypair);
    }
    
    dufa ();
    
}

/*!
 * double-precision Unnormalized Floating Multiply ...
 */
void dufm (void)
{
    //! Except for the precision of the mantissa of the operand from main memory,
    //!    the dufm instruction is identical to the ufm instruction.
    
    //! The ufm instruction is executed as follows:
    //!      C(E) + C(Y)0,7 → C(E)
    //!      ( C(AQ) × C(Y)8,35 )0,71 → C(AQ)
    //! A normalization is performed only in the case of both factor mantissas being 100...0 which is the twos  complement approximation to the decimal value -1.0.
    //! The definition of normalization is located under the description of the fno instruction.
    
    //! * Zero: If C(AQ) = 0, then ON; otherwise OFF
    //! * Neg: If C(AQ)0 = 1, then ON; otherwise OFF
    //! * Exp Ovr: If exponent is greater than +127, then ON
    //! * Exp Undr: If exponent is less than -128, then ON
    
    uint64 m1 = (rA << 28) | ((rQ & 0777777777400LL) >> 8) ; ///< only keep the 1st 64-bits :(
    int8   e1 = (int8)rE;
    
    uint64 m2  = bitfieldExtract36(Ypair[0], 0, 28) << 36;    ///< 64-bit mantissa (incl sign)
           m2 |= Ypair[1];
    
    int8   e2 = (int8)(bitfieldExtract36(Ypair[0], 28, 8) & 0377U);    ///< 8-bit signed integer (incl sign)

    
    //float72 m2 = 0;
    //int8 e2 = -128;
    
    //YPairToExpMant(Ypair, &m2, &e2);
    
    if (m1 == 0 || m2 == 0)
    {
        SETF(cu.IR, I_ZERO);
        CLRF(cu.IR, I_NEG);
        
        rE = (word8)-128;
        rA = 0;
        rQ = 0;
        
        return; // normalized 0
    }
    
    int sign = 1;

    
    if (m1 & FLOAT72SIGN)
    {
     if (m1 == FLOAT72SIGN)
     {
         m1 >>= 1;
         e1 += 1;
     } else
        m1 = (~m1 + 1) & MASK72;
        sign = -sign;
    }
    if (m2 & FLOAT72SIGN)
    {
        if (m2 == FLOAT72SIGN)
        {
            m2 >>= 1;
            e2 += 1;
        } else
        m2 = (~m2 + 1) & MASK72;
        sign = -sign;
    }
    
    int8 e3 = e1 + e2;
    
    uint128 m3 = ((uint128)m1) * ((uint128)m2);
    uint128 m3a = m3 >> 63;
    
    if (sign == -1)
        m3a = (~m3a + 1) & (((word72)1 << 71) - 1);    //0xffffffffffffffff;
    
    rE = e3 & 0377;
    rA = (m3a >> 28) & MASK36;
    rQ = m3a & MASK36;
    //rQ = (m3a & 01777777777LL) << 8;
    
    // A normalization is performed only in the case of both factor mantissas being 100...0 which is the twos complement approximation to the decimal value -1.0.
    //if ((rE == -128 && rA == 0 && rQ == 0) && (m2 == 0 && e2 == -128)) // XXX FixMe
    if ((m1 == ((uint64)1 << 63)) && (m2 == ((uint64)1 << 63)))
        fno ();
    
    SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
    //SCF(rA && SIGN72, cu.IR, I_NEG);
    SCF(rA && SIGN36, cu.IR, I_NEG);
    
    if (e1 + e2 >  127) SETF(cu.IR, I_EOFL);
    if (e1 - e2 < -128) SETF(cu.IR, I_EUFL);
}

/*!
 * floating divide ...
 */
// CANFAULT 
static void dfdvX (bool bInvert)
{
    //! C(EAQ) / C (Y) → C(EA)
    //! C(Y) / C(EAQ) → C(EA) (Inverted)
    
    //! 00...0 → C(Q)
    
    //! The dfdv instruction is executed as follows:
    //! The dividend mantissa C(AQ) is shifted right and the dividend exponent
    //! C(E) increased accordingly until
    //!    | C(AQ)0,63 | < | C(Y-pair)8,71 |
    //!    C(E) - C(Y-pair)0,7 → C(E)
    //!    C(AQ) / C(Y-pair)8,71 → C(AQ)0,63 00...0 → C(Q)64,71
    //! If the divisor mantissa C(Y-pair)8,71 is zero after alignment, the division does not take place. Instead, a divide check fault occurs, C(AQ) contains the dividend magnitude, and the negative indicator reflects the dividend sign.
    
    uint64 m1;
    int8   e1;
    
    uint64 m2;
    int8   e2;
    
    if (!bInvert)
    {
        m1 = (rA << 28) | ((rQ & 0777777777400LL) >> 8) ;  // only keep the 1st 64-bits :(
        e1 = (int8)rE;
        
        m2  = bitfieldExtract36(Ypair[0], 0, 28) << 36;    // 64-bit mantissa (incl sign)
        m2 |= Ypair[1];
        
        e2 = (int8)(bitfieldExtract36(Ypair[0], 28, 8) & 0377U);    // 8-bit signed integer (incl sign)
    } else { // invert
        m2 = (rA << 28) | ((rQ & 0777777777400LL) >> 8) ; // only keep the 1st 64-bits :(
        e2 = (int8)rE;
        
        m1  = bitfieldExtract36(Ypair[0], 0, 28) << 36;    // 64-bit mantissa (incl sign)
        m1 |= Ypair[1];
        
        e1 = (int8) (bitfieldExtract36(Ypair[0], 28, 8) & 0377U);    // 8-bit signed integer (incl sign)
    }
    
    if (m1 == 0)
    {
        // XXX check flags
        SETF(cu.IR, I_ZERO);
        SCF(rA & SIGN36, cu.IR, I_NEG);
        
        rE = (word8)-128;
        rA = 0;
        rQ = 0;
        
        return;
    }
    

    
    //! make everything positive, but save sign info for later....
    int sign = 1;
    if (m1 & SIGN64)    //((uint64)1 << 63))
    {
        if (m1 == SIGN64)
        {
            m1 >>= 1;
            e1 += 1;
        } else
        m1 = (~m1 + 1);     //& (((uint64)1 << 64) - 1);
        sign = -sign;
    }
    
    if (m2 & SIGN64)    //((uint64)1 << 63))
    {
        if (m2 == SIGN64)
        {
            m2 >>= 1;
            e2 += 1;
        } else
        m2 = (~m2 + 1);     //& (((uint64)1 << 64) - 1);
        sign = -sign;
    }
    
    if (m2 == 0)
    {
        //If the divisor mantissa C(Y-pair)8,71 is zero after alignment, the division does not take place. Instead, a divide check fault occurs, C(AQ) contains the dividend magnitude, and the negative indicator reflects the dividend sign.
        
        // NB: If C(Y-pair)8,71 == 0 then the alignment loop will never exit! That's why it been moved before the alignment
        
        SETF(cu.IR, I_ZERO);
        SCF(rA & SIGN36, cu.IR, I_NEG);
        
        rA = m1;
        
        doFault(div_fault, 0, "DFDV: divide check fault");
    }
    
    while (m1 >= m2)
    {
        m1 >>= 1;
        
        if (e1 + 1 > 127)
            SETF(cu.IR, I_EOFL);
        else // XXX: this is my interpretation
            e1 += 1;
    }
    
    int e3 = e1 - e2;
    if (e3 > 127 || e3 < -128)
    {
        // XXX ahndle correctly
        sim_printf ("Exp Underflow/Overflow (%d)\n", e3);
    }
    //uint128 M1 = (uint128)m1 << 63;
    //uint128 M2 = (uint128)m2; ///< << 36;
    
    //uint128 m3 = M1 / M2;
    //uint128 m3 = (uint128)m1 << 35 / (uint128)m2;
    uint128 m3 = ((uint128)m1 << 63) / (uint128)m2;
    uint64 m3b = m3 & ((uint64)-1);  ///< only keep last 64-bits :-(
    
    if (sign == -1)
        m3b = (~m3b + 1); // & (((uint64)1 << 63) - 1);
    
    rE = e3 & 0377;
    rA = (m3b >> 28) & MASK36;
    rQ = (m3b & 01777777777LL) << 8;//MASK36;
    
    SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
    SCF(rA & SIGN36, cu.IR, I_NEG);
    
    if (rA == 0 && rQ == 0)    // set to normalized 0
        rE = (word8)-128;
}

// CANFAULT 
void dfdv (void)
{
    dfdvX (false);    // no inversion
}

// CANFAULT 
void dfdi (void)
{
    dfdvX (true);
}

// CANFAULT 
void dvf (void)
{
    // C(AQ) / (Y)
    //  fractional quotient → C(A)
    //  fractional remainder → C(Q)
    
    // A 71-bit fractional dividend (including sign) is divided by a 36-bit 
    // fractional divisor yielding a 36-bit fractional quotient (including 
    // sign) and a 36-bit fractional remainder (including sign). C(AQ)71 is 
    // ignored; bit position 35 of the remainder corresponds to bit position 
    // 70 of the dividend. The remainder sign is equal to the dividend sign 
    // unless the remainder is zero.
    //
    // If | dividend | >= | divisor | or if the divisor = 0, division does 
    // not take place. Instead, a divide check fault occurs, C(AQ) contains 
    // the dividend magnitude in absolute, and the negative indicator 
    // reflects the dividend sign.
    
// HWR code
#if 0
    // m1 divedend
    // m2 divisor

    word72 m1 = SIGNEXT72((rA << 36) | (rQ & 0777777777776LLU));
    word72 m2 = SIGNEXT72(SIGNEXT36(CY));

sim_printf ("[%lld]\n", sim_timell ());
sim_printf ("m1 "); print_int128 (m1); sim_printf ("\n");
sim_printf ("-----------------\n");
sim_printf ("m2 "); print_int128 (m2); sim_printf ("\n");

    if (m2 == 0)
    {
        // XXX check flags
        SETF(cu.IR, I_ZERO);
        SCF(rA & SIGN36, cu.IR, I_NEG);
        
        rA = 0;
        rQ = 0;
        
        return;
    }
    
    // make everything positive, but save sign info for later....
    int sign = 1;
    int dividendSign = 1;
    if (m1 & SIGN72)
    {
        m1 = (~m1 + 1);
        sign = -sign;
        dividendSign = -1;
    }
    
    if (m2 & SIGN72)
    {
        m2 = (~m2 + 1);
        sign = -sign;
    }
    
    if (m2 == 0)
    {        
        SETF(cu.IR, I_ZERO);
        SCF(rA & SIGN36, cu.IR, I_NEG);
        
        //rA = m1;
        rA = (m1 >> 36) & MASK36;
        rQ = m1 & 0777777777776LLU;
        
        doFault(div_fault, 0, "DVF: divide check fault");
    }
    
    uint128 dividend = (uint128)m1 << 63;
    uint128 divisor = (uint128)m2;
    
    //uint128 m3  = ((uint128)m1 << 63) / (uint128)m2;
    //uint128 m3r = ((uint128)m1 << 63) % (uint128)m2;
    int128 m3  = (int128)(dividend / divisor);
    int128 m3r = (int128)(dividend % divisor);

    if (sign == -1) 
        m3 = -m3;   //(~m3 + 1);
    
    if (dividendSign == -1) // The remainder sign is equal to the dividend sign unless the remainder is zero.
        m3r = -m3r; //(~m3r + 1);
    
    rA = (m3 >> 64) & MASK36;
    rQ = m3r & MASK36;   //01777777777LL;
#endif

// canonial code
#if 0 

sim_printf ("dvf [%lld]\n", sim_timell ());
sim_printf ("rA %llu\n", rA);
sim_printf ("rQ %llu\n", rQ);
sim_printf ("CY %llu\n", CY);

    if (CY == 0)
      {
        // XXX check flags
        SETF (cu . IR, I_ZERO);
        SCF (rA & SIGN36, cu . IR, I_NEG);
        
        rA = 0;
        rQ = 0;
        
        return;
    }
// http://www.ece.ucsb.edu/~parhami/pres_folder/f31-book-arith-pres-pt4.pdf
// slide 10: sequential algorithim

    // dividend format
    // 0  1     70 71
    // s  dividend x
    //  C(AQ)

    int sign = 1;
    bool dividendNegative = (getbits36 (rA, 0, 1) != 0);
    bool divisorNegative = (getbits36 (CY, 0, 1) != 0);

    // Get the 70 bits of the dividend (72 bits less the sign bit and the
    // ignored bit 71.

    // dividend format:   . d(0) ...  d(69)

    uint128 zFrac = ((rA & MASK35) << 35) | ((rQ >> 1) & MASK35);
    if (dividendNegative)
      {
        zFrac = ~zFrac + 1;
        sign = - sign;
      }
    zFrac &= MASK70;
sim_printf ("zFrac "); print_int128 (zFrac); sim_printf ("\n");

    // Get the 35 bits of the divisor (36 bits less the sign bit)

    // divisor format: . d(0) .... d(34) 0 0 0 .... 0 
    
#if 1
    // divisor goes in the high half
    uint128 dFrac = (CY & MASK35) << 35;
    if (divisorNegative)
      {
        dFrac = ~dFrac + 1;
        sign = - sign;
      }
    dFrac &= MASK35 << 35;
#else
    // divisor goes in the low half
    uint128 dFrac = CY & MASK35;
    if (divisorNegative)
      {
        dFrac = ~dFrac + 1;
        sign = - sign;
      }
    dFrac &= MASK35;
#endif
sim_printf ("dFrac "); print_int128 (dFrac); sim_printf ("\n");

    uint128 sn = zFrac;
    word36 quot = 0;
    for (uint i = 0; i < 35; i ++)
      {
        // 71 bit number
        uint128 s2n = sn << 1;
        if (s2n > dFrac)
          {
            s2n -= dFrac;
            quot |= (1llu << (34 - i));
          }
        sn = s2n;
      }
    word36 remainder = sn;

    if (sign == -1)
      quot = ~quot + 1;

    if (dividendNegative)
      remainder = ~remainder + 1;

    rA = quot & MASK36;
    rQ = remainder & MASK36;
 
#endif

// MM code
#if 1

//sim_printf ("dvf [%lld]\n", sim_timell ());
//sim_printf ("rA %llu\n", rA);
//sim_printf ("rQ %llu\n", rQ);
//sim_printf ("CY %llu\n", CY);

    if (CY == 0)
      {
        // XXX check flags
        SETF (cu . IR, I_ZERO);
        SCF (rA & SIGN36, cu . IR, I_NEG);
        
        rA = 0;
        rQ = 0;
        
        return;
    }
// http://www.ece.ucsb.edu/~parhami/pres_folder/f31-book-arith-pres-pt4.pdf
// slide 10: sequential algorithim

    // dividend format
    // 0  1     70 71
    // s  dividend x
    //  C(AQ)

    int sign = 1;
    bool dividendNegative = (getbits36 (rA, 0, 1) != 0);
    bool divisorNegative = (getbits36 (CY, 0, 1) != 0);

    // Get the 70 bits of the dividend (72 bits less the sign bit and the
    // ignored bit 71.

    // dividend format:   . d(0) ...  d(69)

    uint128 zFrac = ((rA & MASK35) << 35) | ((rQ >> 1) & MASK35);
    if (dividendNegative)
      {
        zFrac = ~zFrac + 1;
        sign = - sign;
      }
    zFrac &= MASK70;
//sim_printf ("zFrac "); print_int128 (zFrac); sim_printf ("\n");

    // Get the 35 bits of the divisor (36 bits less the sign bit)

    // divisor format: . d(0) .... d(34) 0 0 0 .... 0 
    
    // divisor goes in the low half
    uint128 dFrac = CY & MASK35;
    if (divisorNegative)
      {
        dFrac = ~dFrac + 1;
        sign = - sign;
      }
    dFrac &= MASK35;
//sim_printf ("dFrac "); print_int128 (dFrac); sim_printf ("\n");


    uint128 quot = zFrac / dFrac;
    uint128 remainder = zFrac % dFrac;

    if (sign == -1)
      quot = ~quot + 1;

    if (dividendNegative)
      remainder = ~remainder + 1;

    rA = quot & MASK36;
    rQ = remainder & MASK36;
 
#endif

//sim_printf ("Quotient %lld (%llo)\n", rA, rA);
//sim_printf ("Remainder %lld\n", rQ);
    SCF(rA == 0 && rQ == 0, cu.IR, I_ZERO);
    SCF(rA & SIGN36, cu.IR, I_NEG);
}


/*!
 * double precision floating round ...
 */
void dfrd (void)
{
    //! The dfrd instruction is identical to the frd instruction except that the rounding constant used is (11...1)65,71 instead of (11...1)29,71.
    
    //! If C(AQ) ≠ 0, the frd instruction performs a true round to a precision of 64 bits and a normalization on C(EAQ).
    //! A true round is a rounding operation such that the sum of the result of applying the operation to two numbers of equal magnitude but opposite sign is exactly zero.
    
    //! The frd instruction is executed as follows:
    //! C(AQ) + (11...1)65,71 → C(AQ)
    //! * If C(AQ)0 = 0, then a carry is added at AQ71
    //! * If overflow occurs, C(AQ) is shifted one place to the right and C(E) is increased by 1.
    //! * If overflow does not occur, C(EAQ) is normalized.
    //! * If C(AQ) = 0, C(E) is set to -128 and the zero indicator is set ON.
        
    float72 m = ((word72)rA << 36) | (word72)rQ;
    if (m == 0)
    {
        rE = (word8)-128;
        SETF(cu.IR, I_ZERO);
        CLRF(cu.IR, I_NEG);
        
        return;
    }

    bool s1 = (bool)(m & SIGN72);
    
    // C(AQ) + (11...1)65,71 → C(AQ)
    m += (word72)0177LL; // add 1's into lower 64-bits

    // If C(AQ)0 = 0, then a carry is added at AQ71
    if (!s1)     //bitfieldExtract72(m, 71, 1) == 0)
        m += 1;
    
    // 0 → C(AQ)64,71 
    //m &= (word72)0777777777777LL << 36 | 0777777777400LL; // 64-71 => 0 per DH02-01/Bull DPS9000
    m = bitfieldInsert72(m, 0, 0, 8);
    
    bool s2 = (bool)(m & SIGN72);
    
    bool ov = s1 != s2;   ///< sign change denotes overflow
    if (ov)
    {
        // If overflow occurs, C(AQ) is shifted one place to the right and C(E) is increased by 1.
        m >>= 1;
        if (s1) // restore sign if necessary (was s2)
            m |= SIGN72;
        
        rA = (m >> 36) & MASK36;
        rQ = m & MASK36;
        
        if (rE + 1 > 127)
            SETF(cu.IR, I_EOFL);
        rE +=  1;
    }
    else
    {
        // If overflow does not occur, C(EAQ) is normalized.
        rA = (m >> 36) & MASK36;
        rQ = m & MASK36;
        
        fno ();
    }
    
    // If C(AQ) = 0, C(E) is set to -128 and the zero indicator is set ON.
    if (rA == 0 && rQ == 0)
    {
        rE = (word8)-128;
        SETF(cu.IR, I_ZERO);
    }
    
    SCF(rA & SIGN36, cu.IR, I_NEG);
}

void dfstr (word36 *Ypair)
{
    //! The dfstr instruction performs a double-precision true round and normalization on C(EAQ) as it is stored.
    //! The definition of true round is located under the description of the frd instruction.
    //! The definition of normalization is located under the description of the fno instruction.
    //! Except for the precision of the stored result, the dfstr instruction is identical to the fstr instruction.
        
    //! The dfrd instruction is identical to the frd instruction except that the rounding constant used is (11...1)65,71 instead of (11...1)29,71.
    
    //! If C(AQ) ≠ 0, the frd instruction performs a true round to a precision of 28 bits and a normalization on C(EAQ).
    //! A true round is a rounding operation such that the sum of the result of applying the operation to two numbers of equal magnitude but opposite sign is exactly zero.
    
    //! The frd instruction is executed as follows:
    //! C(AQ) + (11...1)29,71 → C(AQ)
    //! * If C(AQ)0 = 0, then a carry is added at AQ71
    //! * If overflow occurs, C(AQ) is shifted one place to the right and C(E) is increased by 1.
    //! * If overflow does not occur, C(EAQ) is normalized.
    //! * If C(AQ) = 0, C(E) is set to -128 and the zero indicator is set ON.
    
    //! I believe AL39 is incorrect; bits 64-71 should be set to 0, not 65-71. DH02-01 & Bull 9000 is correct.
    
    word36 A = rA, Q = rQ;
    word8 E = rE;
    A &= DMASK;
    Q &= DMASK;
    E &= MASK8;

    float72 m = ((word72)A << 36) | (word72)rQ;
    if (m == 0)
    {
        E = (word8)-128;
        SETF(cu.IR, I_ZERO);
        CLRF(cu.IR, I_NEG);
        
        Ypair[0] = ((word36)E << 28) | ((A & 0777777777400LLU) >> 8);
        Ypair[1] = ((A & MASK8) << 28) | ((Q & 0777777777400LLU) >> 8);

        return;
    }
    
    
    // C(AQ) + (11...1)65,71 → C(AQ)
    bool s1 = (m & SIGN72) != (word72)0;
    
    m += (word72)0177LLU; // add 1's into lower 43-bits
    
    // If C(AQ)0 = 0, then a carry is added at AQ71
    if (s1 == 0)
        m += 1;
    
    // 0 → C(AQ)64,71
    //m &= (word72)0777777777777LL << 36 | 0777777777400LL; // 64-71 => 0 per DH02-01/Bull DPS9000
    m = bitfieldInsert72(m, 0, 0, 8);
    
    bool s2 = (m & SIGN72) != (word72)0;
    
    bool ov = s1 != s2;   ///< sign change denotes overflow
    if (ov)
    {
        // If overflow occurs, C(AQ) is shifted one place to the right and C(E) is increased by 1.
        m >>= 1;
        if (s1) // restore sign if necessary (was s2)
            m |= SIGN72;
        
        A = (m >> 36) & MASK36;
        Q = m & MASK36;
        
        if (E + 1 > 127)
            SETF(cu.IR, I_EOFL);
        E +=  1;
    }
    else
    {
        // If overflow does not occur, C(EAQ) is normalized.
        A = (m >> 36) & MASK36;
        Q = m & MASK36;
        
        fnoEAQ(&E, &A, &Q);
    }
    
    // If C(AQ) = 0, C(E) is set to -128 and the zero indicator is set ON.
    if (A == 0 && Q == 0)
    {
        E = (word8)-128;
        SETF(cu.IR, I_ZERO);
    }
    
    SCF(A & SIGN36, cu.IR, I_NEG);
    
    Ypair[0] = ((word36)E << 28) | ((A & 0777777777400LL) >> 8);
    Ypair[1] = ((A & 0377) << 28) | ((Q & 0777777777400LL) >> 8);
}

/*!
 * double precision Floating Compare ...
 */
void dfcmp (void)
{
    //! C(E) :: C(Y-pair)0,7
    //! C(AQ)0,63 :: C(Y-pair)8,71
    
    //! Zero: If C(EAQ) = C(Y), then ON; otherwise OFF
    //! Neg: If C(EAQ) < C(Y), then ON; otherwise OFF
    
    //! The dfcmp instruction is identical to the fcmp instruction except for the precision of the mantissas actually compared.
    
    //! Notes: The fcmp instruction is executed as follows:
    //! The mantissas are aligned by shifting the mantissa of the operand with the algebraically smaller exponent to the right the number of places equal to the difference in the two exponents.
    //! The aligned mantissas are compared and the indicators set accordingly.
    
    int64 m1 = (int64) ((rA << 28) | ((rQ & 0777777777400LL) >> 8));  ///< only keep the 1st 64-bits :(
    int8  e1 = (int8)rE;
    
    int64 m2  = (int64) (bitfieldExtract36(Ypair[0], 0, 28) << 36);    ///< 64-bit mantissa (incl sign)
          m2 |= Ypair[1];
    
    int8 e2 = (int8) (bitfieldExtract36(Ypair[0], 28, 8) & 0377U);    ///< 8-bit signed integer (incl sign)

    //which exponent is smaller???
    
    int shift_count = -1;
    
    if (e1 == e2)
    {
        shift_count = 0;
    }
    else if (e1 < e2)
    {
        shift_count = abs(e2 - e1);
        m1 >>= shift_count;
    }
    else
    {
        shift_count = abs(e1 - e2);
        m2 >>= shift_count;
    }
    
    // need to do algebraic comparisons of mantissae
    SCF(m1 == m2, cu.IR, I_ZERO);
    SCF(m1 <  m2, cu.IR, I_NEG);
}

/*!
 * double precision Floating Compare magnitude ...
 */
void dfcmg (void)
{
    //! C(E) :: C(Y)0,7
    //! | C(AQ)0,27 | :: | C(Y)8,35 |
    //! * Zero: If | C(EAQ)| = | C(Y) |, then ON; otherwise OFF
    //! * Neg : If | C(EAQ)| < | C(Y) |, then ON; otherwise OFF
    
    //! Notes: The fcmp instruction is executed as follows:
    //! The mantissas are aligned by shifting the mantissa of the operand with the algebraically smaller exponent to the right the number of places equal to the difference in the two exponents.
    //! The aligned mantissas are compared and the indicators set accordingly.
    
    //! The dfcmg instruction is identical to the dfcmp instruction except that the magnitudes of the mantissas are compared instead of the algebraic values.
    
    int64 m1 = (int64) ((rA << 28) | ((rQ & 0777777777400LL) >> 8));  ///< only keep the 1st 64-bits :(
    int8  e1 = (int8)rE;
    
    int64 m2  = (int64) bitfieldExtract36(Ypair[0], 0, 28) << 36;    ///< 64-bit mantissa (incl sign)
    m2 |= Ypair[1];
    
    int8 e2 = (int8) (bitfieldExtract36(Ypair[0], 28, 8) & 0377U);    ///< 8-bit signed integer (incl sign)
    
    //which exponent is smaller???
    
    int shift_count = -1;
    
    if (e1 == e2)
    {
        shift_count = 0;
    }
    else
    if (e1 < e2)
    {
        shift_count = abs(e2 - e1);
        m1 >>= shift_count;
    }
    else
    {
        shift_count = abs(e1 - e2);
        m2 >>= shift_count;
    }
    
    // fetch magnitudes of mantissae
    m1 = llabs(m1);
    m2 = llabs(m2);
    
    SCF(m1 == m2, cu.IR, I_ZERO);
    SCF(m1 < m2, cu.IR, I_NEG);
}
