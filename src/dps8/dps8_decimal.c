//
//  dps8_decimal.c
//  dps8
//
//  Created by Harry Reed on 2/9/13.
//  Original portions Copyright (c) 2013 Harry Reed. All rights reserved.
//
//  decimal arithmetic support code for dps8 simulator.
//
//  portions based off of the 'decNumber' decimal arithmetic library
//  Copyright (c) IBM Corporation, 2000, 2009.  All rights reserved.
//

#define DECUSE64    1
#define DECSUBSET   1
#define DECDPUN     8
#define DECBUFFER   32

#define DECNUMDIGITS    64

#include "decNumber.h"        // base number library
#include "decNumberLocal.h"   // decNumber local types, etc.

#include "dps8.h"

#include <stdio.h>

//void write49(EISstruct *e, word18 *dstAddr, int *pos, int tn, int c49);
//void loadInputBufferNumeric(EISstruct *e, int k);
void parseNumericOperandDescriptor(int k, EISstruct *e);

void EISwrite49(EISaddr *p, int *pos, int tn, int c49);
void EISloadInputBufferNumeric(EISstruct *e, int k);


//void printBCD(decNumber *a, decContext *set, int width);
//decNumber *decBCDToNumber(const uint8_t *bcd, int length, const int scale, decNumber *dn);
//uint8_t * decBCDFromNumber(uint8_t *bcd, int length, int *scale, const decNumber *dn);



/* ------------------------------------------------------------------ */
/* HWR 2/07 15:49 derived from ......                                 */
/*                                                                    */
/* decPackedToNumber -- convert BCD Packed Decimal to a decNumber     */
/*                                                                    */
/*   bcd    is the BCD bytes                                          */
/*   length is the length of the BCD array                            */
/*   scale  is the scale associated with the BCD integer              */
/*   dn     is the decNumber [with space for length*2 digits]         */
/*   returns dn, or NULL if error                                     */
/*                                                                    */
/* The BCD decimal byte array, together with an associated scale,     */
/* is converted to a decNumber.  The BCD array is assumed full        */
/* of digits.                                                         */
/* The scale is used (negated) as the exponent of the decNumber.      */
/* Note that zeros may have a scale.                                  */
/*                                                                    */
/* The decNumber structure is assumed to have sufficient space to     */
/* hold the converted number (that is, up to length-1 digits), so     */
/* no error is possible unless the adjusted exponent is out of range. */
/* In these error cases, NULL is returned and the decNumber will be 0.*/
/* ------------------------------------------------------------------ */
decNumber * decBCDToNumber(const uByte *bcd, Int length, const Int scale, decNumber *dn)
{
    const uByte *last=bcd+length-1;  // -> last byte
    const uByte *first;              // -> first non-zero byte
    uInt  nib;                       // work nibble
    Unit  *up=dn->lsu;               // output pointer
    Int   digits;                    // digits count
    Int   cut=0;                     // phase of output
    
    decNumberZero(dn);               // default result
    last = &bcd[length-1];
    //nib = *last & 0x0f;                // get the sign
    //if (nib==DECPMINUS || nib==DECPMINUSALT) dn->bits=DECNEG;
    //else if (nib<=9) return NULL;   // not a sign nibble
    
    // skip leading zero bytes [final byte is always non-zero, due to sign]
    for (first=bcd; *first==0;) first++;
    digits=(last-first)+1;              // calculate digits ..
    //if ((*first & 0xf0)==0) digits--;     // adjust for leading zero nibble
    if (digits!=0) dn->digits=digits;     // count of actual digits [if 0,
    // leave as 1]
    
    // check the adjusted exponent; note that scale could be unbounded
    dn->exponent=-scale;                 // set the exponent
    if (scale>=0) {                      // usual case
        if ((dn->digits-scale-1)<-DECNUMMAXE) {      // underflow
            decNumberZero(dn);
            return NULL;}
    }
    else { // -ve scale; +ve exponent
        // need to be careful to avoid wrap, here, also BADINT case
        if ((scale<-DECNUMMAXE)            // overflow even without digits
            || ((dn->digits-scale-1)>DECNUMMAXE)) { // overflow
            decNumberZero(dn);
            return NULL;}
    }
    if (digits==0) return dn;             // result was zero
    
    // copy the digits to the number's units, starting at the lsu
    // [unrolled]
    for (;;) {                            // forever
        nib=(unsigned)(*last & 0x0f);
        // got a digit, in nib
        if (nib>9) {decNumberZero(dn); return NULL;}    // bad digit
        
        if (cut==0) *up=(Unit)nib;
        else *up=(Unit)(*up+nib*DECPOWERS[cut]);
        digits--;
        if (digits==0) break;               // got them all
        cut++;
        if (cut==DECDPUN) {
            up++;
            cut=0;
        }
        last--;                             // ready for next
        //        nib = *last & 0x0f;                // get right nibble
        //        if (nib>9) {decNumberZero(dn); return NULL;}
        //
        //        // got a digit, in nib
        //        if (cut==0) *up=(Unit)nib;
        //        else *up=(Unit)(*up+nib*DECPOWERS[cut]);
        //        digits--;
        //        if (digits==0) break;               // got them all
        //        cut++;
        //        if (cut==DECDPUN) {
        //            up++;
        //            cut=0;
        //        }
    } // forever
    
    return dn;
} // decBCDToNumber

decNumber * decBCD9ToNumber(const word9 *bcd, Int length, const Int scale, decNumber *dn)
{
    const word9 *last=bcd+length-1;  // -> last byte
    const word9 *first;              // -> first non-zero byte
    uInt  nib;                       // work nibble
    Unit  *up=dn->lsu;               // output pointer
    Int   digits;                    // digits count
    Int   cut=0;                     // phase of output
    
    decNumberZero(dn);               // default result
    last = &bcd[length-1];
    //nib = *last & 0x0f;                // get the sign
    //if (nib==DECPMINUS || nib==DECPMINUSALT) dn->bits=DECNEG;
    //else if (nib<=9) return NULL;   // not a sign nibble
    
    // skip leading zero bytes [final byte is always non-zero, due to sign]
    for (first=bcd; *first==0;) first++;
    digits=(last-first)+1;              // calculate digits ..
    //if ((*first & 0xf0)==0) digits--;     // adjust for leading zero nibble
    if (digits!=0) dn->digits=digits;     // count of actual digits [if 0,
    // leave as 1]
    
    // check the adjusted exponent; note that scale could be unbounded
    dn->exponent=-scale;                 // set the exponent
    if (scale>=0) {                      // usual case
        if ((dn->digits-scale-1)<-DECNUMMAXE) {      // underflow
            decNumberZero(dn);
            return NULL;}
    }
    else { // -ve scale; +ve exponent
        // need to be careful to avoid wrap, here, also BADINT case
        if ((scale<-DECNUMMAXE)            // overflow even without digits
            || ((dn->digits-scale-1)>DECNUMMAXE)) { // overflow
            decNumberZero(dn);
            return NULL;}
    }
    if (digits==0) return dn;             // result was zero
    
    // copy the digits to the number's units, starting at the lsu
    // [unrolled]
    for (;;) {                            // forever
        nib=(unsigned)(*last & 0x0f);
        // got a digit, in nib
        if (nib>9) {decNumberZero(dn); return NULL;}    // bad digit
        
        if (cut==0) *up=(Unit)nib;
        else *up=(Unit)(*up+nib*DECPOWERS[cut]);
        digits--;
        if (digits==0) break;               // got them all
        cut++;
        if (cut==DECDPUN) {
            up++;
            cut=0;
        }
        last--;                             // ready for next
        //        nib = *last & 0x0f;                // get right nibble
        //        if (nib>9) {decNumberZero(dn); return NULL;}
        //
        //        // got a digit, in nib
        //        if (cut==0) *up=(Unit)nib;
        //        else *up=(Unit)(*up+nib*DECPOWERS[cut]);
        //        digits--;
        //        if (digits==0) break;               // got them all
        //        cut++;
        //        if (cut==DECDPUN) {
        //            up++;
        //            cut=0;
        //        }
    } // forever
    
    return dn;
} // decBCD9ToNumber

/* ------------------------------------------------------------------ */
/* HWR 2/07 15:49 derived from ......                                 */
/*                                                                    */
/* decPackedFromNumber -- convert decNumber to BCD Packed Decimal     */
/*                                                                    */
/*   bcd    is the BCD bytes                                          */
/*   length is the length of the BCD array                            */
/*   scale  is the scale result                                       */
/*   dn     is the decNumber                                          */
/*   returns bcd, or NULL if error                                    */
/*                                                                    */
/* The number is converted to a BCD decimal byte array,               */
/* right aligned in the bcd array, whose length is indicated by the   */
/* second parameter.                                                  */
/* scale is set to the scale of the number (this is the exponent,     */
/* negated).  To force the number to a specified scale, first use the */
/* decNumberRescale routine, which will round and change the exponent */
/* as necessary.                                                      */
/*                                                                    */
/* If there is an error (that is, the decNumber has too many digits   */
/* to fit in length bytes, or it is a NaN or Infinity), NULL is       */
/* returned and the bcd and scale results are unchanged.  Otherwise   */
/* bcd is returned.                                                   */
/* ------------------------------------------------------------------ */
uint8_t * decBCDFromNumber(uint8_t *bcd, int length, int *scale, const decNumber *dn) {
    const Unit *up=dn->lsu;     // Unit array pointer
    uByte obyte, *out;          // current output byte, and where it goes
    Int indigs=dn->digits;      // digits processed
    uInt cut=DECDPUN;           // downcounter per Unit
    uInt u=*up;                 // work
    uInt nib;                   // ..
#if DECDPUN<=4
    uInt temp;                  // ..
#endif
    
    if (dn->digits>length                  // too long ..
        ||(dn->bits & DECSPECIAL)) return NULL;   // .. or special -- hopeless
    
    //if (dn->bits&DECNEG) obyte=DECPMINUS;      // set the sign ..
    //else                obyte=DECPPLUS;
    *scale=-dn->exponent;                      // .. and scale
    
    // loop from lowest (rightmost) byte
    out=bcd+length-1;                          // -> final byte
    for (; out>=bcd; out--) {
        if (indigs>0) {
            if (cut==0) {
                up++;
                u=*up;
                cut=DECDPUN;
            }
#if DECDPUN<=4
            temp=(u*6554)>>16;         // fast /10
            nib=u-X10(temp);
            u=temp;
#else
            nib=u%10;                  // cannot use *6554 trick :-(
            u=u/10;
#endif
            //obyte|=(nib<<4);
            obyte=nib;
            indigs--;
            cut--;
        }
        *out=obyte;
        obyte=0;                       // assume 0
        //        if (indigs>0) {
        //            if (cut==0) {
        //                up++;
        //                u=*up;
        //                cut=DECDPUN;
        //            }
        //#if DECDPUN<=4
        //            temp=(u*6554)>>16;         // as above
        //            obyte=(uByte)(u-X10(temp));
        //            u=temp;
        //#else
        //            obyte=(uByte)(u%10);
        //            u=u/10;
        //#endif
        //            indigs--;
        //            cut--;
        //        }
    } // loop
    
    return bcd;
} // decBCDFromNumber


void printBCD(decNumber *a, decContext *set, int width)
{
    uint8_t bcd[256];
    memset(bcd, 0, sizeof(bcd));
    
    decNumberGetBCD(a, bcd);
    fprintf(stderr, "Bcd: %c", decNumberIsNegative(a) ? '-' : '+');
    
    for(int n = 0 ; n < width-1 ; n += 1)
        fprintf(stderr, "%d", bcd[n]);
    fprintf(stderr, "  scale=%d\n", -(a->exponent));
}

char *getBCD(decNumber *a)
{
    static uint8_t bcd[256];
    memset(bcd, 0, sizeof(bcd));
    int scale;
    
    decBCDFromNumber(bcd, a->digits, &scale, a);
    for(int i = 0 ; i < a->digits ; i += 1 )
        bcd[i] += '0';
    
    return bcd;
    
    
}


int calcOD(int lengthOfDividend,
           int leadingZerosInDividend,
           int lengthOfDivisor,
           int leadingZerosInDivisor,
           int expoonentOfDividend,
           int exppnentOfDivisor,
           int scaleFactorForQuotient)
{
    //#QD = (LD-#LZD+1)-(LDR-#LZR)+(ED-EDR-EQ)
    //where:
    //  #LZD = number of leading zeros in dividend
    //  #QD  = number of quotient digits to form
    //  LD   = length of dividend
    //  LDR  = length of divisor
    //  #LZR = number of leading zeros in divisor
    //  ED   = exponent of dividend
    //  EDR  = exponent of divisor
    //  EQ   = scale factor for quotient
    return (lengthOfDividend-leadingZerosInDividend+1)-(lengthOfDivisor-leadingZerosInDivisor)+(expoonentOfDividend-exppnentOfDivisor-scaleFactorForQuotient);
}

char *CS[] = {"CSFL", "CSLS", "CSTS", "CSNS"};
char *CTN[] = {"CTN9", "CTN4"};

int calcSF(int sf1, int sf2, int sf3)
{
    //If the result is given by a fixed-point, operations are performed by justifying the scaling factors (SF1, SF2, and SF3) of the operands 1, 2, and 3:
    //If SF1 > SF2
    //  SF1 > SF2 >= SF3 —> Justify to SF2
    //  SF3 > SF1 > SF2 —> Justify to SF1
    //  SF1 >= SF3 > SF1 —>Justify to SF3-1
    //If SF2 > SF1
    //  SF2 > SF1 >= SF3 —> Justify to SF1
    //  SF3 > SF2 > SF1 —> Justify to SF2
    //  SF2 >= SF3 > SF1 —> Justify to SF3-1
    //
    if (sf1 > sf2)
    {
        if (sf1 > sf2 && sf2 >= sf3)
            return sf2;
        
        if (sf3 > sf1 && sf1 > sf2)
            return sf1;
        
        if (sf1 >= sf3 && sf3 > sf1)
            return sf3-1;
    }
    if (sf2 > sf1)
    {
        if (sf2 > sf1 && sf1 >= sf3)
            return sf1;
        if (sf3 > sf2 && sf2 > sf1)
            return sf2;
        if (sf2 >= sf3 && sf3 > sf1)
            return sf3-1;
    }
    //fprintf(stderr, "calcSF(): How'd we get here?\n");
    return sf3;
}

char *formatDecimal(decContext *set, decNumber *r, int tn, int n, int s, int sf, bool R, bool *OVR, bool *TRUNC)
{
    
    if (s == CSFL)
        sf = 0;
    
    // XXX what happens if we try to write a negative number to an unsigned field?????
    // Detection of a character outside the range [0,11]8 in a digit position or a character outside the range [12,17]8 in a sign position causes an illegal procedure fault.
    
    // adjust output length according to type ....
    //This implies that an unsigned fixed-point receiving field has a minimum length of 1 character; a signed fixed-point field, 2 characters; and a floating-point field, 3 characters.
    int adjLen = n;
    switch (s)
    {
        case CSFL:          // we have a leading sign and a trailing exponent.
            if (tn == CTN9)
                adjLen -= 2;     // a sign and an 1 9-bit exponent
            else
                adjLen -= 3;     // a sign and 2 4-bit digits making up the exponent
            break;
        case CSLS:
        case CSTS:          // take sign into assount. One less char to play with
            adjLen -= 1;
            break;
        case CSNS:
            break;          // no sign to worry about. Use everything
    }
    
    //fprintf(stderr, "\nINFO: adjLen=%d SF=%d S=%s TN=%s\n", adjLen, sf, CS[s], CTN[tn]);
    //fprintf(stderr,   "INFO: %s  r->digits=%d  r->exponent=%d\n", getBCD(r), r->digits, r->exponent);
    
    if (adjLen < 1)
    {
        // adjusted length is too small for anything but sign and/or exponent
        //*OVR = 1;
        
        // XXX what do we fill in here? Sign and exp?
        *OVR = true;
        return ("");
    }
    
    // scale result (if not floating)
    
    decNumber _r2;
    decNumberZero(&_r2);
    
    decNumber *r2 = &_r2;
    
    decNumber _sf;
    
    {
        decNumberTrim(r);   // clean up any trailing 0's
        
        int scale;
        //char out[256], out2[256];
        //bzero(out, sizeof(out));
        //bzero(out2, sizeof(out2));
        
        //decBCDFromNumber(out, r->digits, &scale, r);
        //for(int i = 0 ; i < r->digits ; i += 1 )
        //    out[i] += '0';
        
        if (s != CSFL)
        {
            decNumberFromInt32(&_sf, sf);
            
            r2 = decNumberRescale(&_r2, r, &_sf, set);
            //fprintf(stderr, "INFO: %s r2->digits=%d r2->exponent=%d\n", getBCD(r2), r2->digits, r2->exponent);
        }
        else
            *r2 = *r;
        
        // this breaks things so dont do it
        //decNumberReduce(r2, r2, set);   // clean up any trailing 0's
        //decNumberTrim(r2);   // clean up any trailing 0's
        
        //decBCDFromNumber(out2, r2->digits, &scale, r2);
        //for(int i = 0 ; i < r2->digits ; i += 1 )
        //    out2[i] += '0';
        
        //fprintf(stderr, "INFO: adjLen=%d digits(r)=%s E=%d SF=%d S=%s TN=%s digits(r2)=%s E2=%d\n", adjLen, out,r->exponent, sf, CS[s], CTN[tn],out2, r2->exponent);
    }
        
    int scale;
    
    static uint8_t out[256];
    
    bzero(out, sizeof(out));
    
    bool ovr = (r->digits-sf) > adjLen;     // is integer portion too large to fit?
    bool trunc = r->digits > r2->digits;     // did we loose something along the way?
    
    // now let's check for overflows
    if (!ovr && !trunc)
        //if ((r2->digits <= adjLen) && (r->digits == r2->digits))
    {
        //fprintf(stderr, "formatDecimal(OK): r->digits(%d) <= adjLen(%d) r2->digits(%d)\n", r->digits, adjLen, r2->digits);
        if (s == CSFL)
            if (r2->digits < adjLen)
            {
                decNumber _s, *s;
                s = decNumberFromInt32(&_s, r2->exponent - (adjLen - r2->digits));
                r2 = decNumberRescale(r2, r2, s, set);
            }
        decBCDFromNumber(out, adjLen, &scale, r2);
        for(int i = 0 ; i < adjLen ; i += 1 )
            out[i] += '0';
    }
    else
    {
        ovr = false;
        trunc = false;
        
        // if we get here then we have either overflow or truncation....
        
        //fprintf(stderr, "formatDecimal(%s): r->digits %d > adjLen %d\n", R ? "R" : "", r->digits, adjLen);
        
        // so, what do we do?
        if (R)
        {
            // NB even with rounding you can have an overflow...
            
            // if we're in rounding mode then we just make things fit and everything is OK - except if we have an overflow.
            
            //            int safe = set->digits;
            //            set->digits = adjLen;
            //
            //            decNumber *ro = (s == CSFL ? r : r2);
            //            decNumberPlus(ro, ro, set);
            //
            //            decBCDFromNumber(out, adjLen, &scale, ro);
            //
            //            for(int i = 0 ; i < adjLen ; i += 1 )
            //                out[i] += '0';
            //            out[adjLen] = 0;
            //
            //            set->digits = safe;
            
            
            decNumber *ro = r2; //(s == CSFL ? r : r2);
            
            int safe = set->digits;
            
            if (ro->digits > (adjLen + 1))
            {
                set->digits = ro->digits + sf + 1;
                decNumberPlus(ro, ro, set);
                
                decBCDFromNumber(out, set->digits, &scale, ro);
                for(int i = 0 ; i < set->digits ; i += 1 )
                    out[i] += '0';
                //out[set->digits] = 0;
                strcpy(out, out+set->digits-adjLen);
                
                //fprintf(stderr, "R OVR\n");
                ovr = true;
            }
            else
            {
                set->digits = adjLen;
                decNumberPlus(ro, ro, set);
                
                decBCDFromNumber(out, adjLen, &scale, ro);
                
                for(int i = 0 ; i < adjLen ; i += 1 )
                    out[i] += '0';
                out[adjLen] = 0;
                
                ovr = false;    // since we've rounded we can have no overflow ?????
            }
            
            
            
            set->digits = safe;
            
            
            
            
            // display int of number
            //decNumber _i;
            //decNumber *i = decNumberToIntegralValue(&_i, ro, set);
            
            //char outi[256];
            //bzero(outi, sizeof(outi));
            //decBCDFromNumber(outi, adjLen, &scale, i);
            //for(int i = 0 ; i < adjLen; i += 1 )
            //    outi[i] += '0';
            //fprintf(stderr, "i=%s\n", outi);
            
        }
        else
        {
            // if we're not in rounding mode then we can either have a truncation or an overflow
            
            // The decimal number of data type TN1, sign and decimal type S1, and starting location YC1, are added to the decimal number of data type TN2, sign and decimal type S2, and starting location YC2. The sum is stored starting in location YC3 as a decimal number of data type TN3 and sign and decimal type S3.
            // If S3 indicates a fixed-point format, the results are stored using scale factor SF3, which causes leading or trailing zeros (4 bits - 0000, 9 bits - 000110000) to be supplied and/or most significant digit overflow or least significant digit truncation to occur.
            // If S3 indicates a floating-point format, the result is right-justified to preserve the most significant nonzero digits even if this causes least significant truncation.
            
            // If N3 is not large enough to hold the integer part of the result as scaled by SF3, an overflow condition exists; the overflow indicator is set ON and an overflow fault occurs. This implies that an unsigned fixed-point receiving field has a minimum length of 1 character; a signed fixed-point field, 2 characters; and a floating-point field, 3 characters.
            // If N3 is not large enough to hold all the digits of the result as scaled by SF3 and R = 0, then a truncation condition exists; data movement stops when C(Y-charn3) is filled and the truncation indicator is set ON. If R = 1, then the last digit moved is rounded according to the absolute value of the remaining digits of the result and the instruction completes normally.
            
            
            if (s == CSFL)
            {
                int safeR = decContextGetRounding(set);         // save rounding mode
                decContextSetRounding(set, DEC_ROUND_DOWN);     // Round towards 0 (truncation).
                
                int safe = set->digits;
                set->digits = adjLen;
                decNumberPlus(r2, r2, set);
                
                decBCDFromNumber(out, r2->digits, &scale, r2);
                for(int i = 0 ; i < adjLen ; i += 1 )
                    out[i] += '0';
                out[adjLen] = 0;
                
                set->digits = safe;
                decContextSetRounding(set, safeR);              // restore rounding mode
                
                //fprintf(stderr, "CSFL TRUNC\n");
                trunc = true;
            }
            else
            {
                if (r2->digits < r->digits)
                {
                    int safeR = decContextGetRounding(set);         // save rounding mode
                    decContextSetRounding(set, DEC_ROUND_DOWN);     // Round towards 0 (truncation).
                    
                    // re-rescale r with an eye towards truncation notrounding
                    
                    r2 = decNumberRescale(r2, r, &_sf, set);
                    
                    if (r2->digits <= adjLen)
                        decBCDFromNumber(out, adjLen, &scale, r2);
                    else
                        decBCDFromNumber(out, r2->digits, &scale, r2);
                    for(int i = 0 ; i < adjLen; i += 1 )
                        out[i] += '0';
                    out[adjLen] = 0;
                    
                    decContextSetRounding(set, safeR);              // restore rounding mode
                    
                    //fprintf(stderr, "TRUNC\n");
                    trunc = true;
                    
                } else if (r->digits > adjLen)
                {
                    // OVR
                    decBCDFromNumber(out, r->digits, &scale, r);
                    for(int i = 0 ; i < r->digits ; i += 1 )
                        out[i] += '0';
                    out[r->digits] = 0;
                    strcpy(out, out+r->digits-adjLen);
                    
                    //fprintf(stderr, "OVR\n");
                    ovr = true;
                }
                else
                    fprintf(stderr, "formatDecimal(): How'd we get here?\n");
            }
        }
    }
    //fprintf(stderr, "INFO: ovrflow=%d trunc=%d R=%d\n", ovr, trunc, R);
    *OVR = ovr;
    *TRUNC = trunc;
    
    decNumberCopy(r, r2);
    return out;
}

/*
 * decimal EIS instructions ... 
 */


#define ASC(x)  ((x) + '0')

/*
 * ad2d - Add Using Two Decimal Operands
 */
void ad2d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
    switch(e->srcTN)
    {
        case CTN4:
            //e->srcAddr = e->YChar41;
            break;
        case CTN9:
            //e->srcAddr = e->YChar91;
            break;
    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
    
    e->dstTN = e->TN2;    // type of chars in dst
    e->dstCN = e->CN2;    // starting at char pos CN


//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            e->dstAddr = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            e->dstAddr = e->YChar92;
//            break;
//    }
    
    e->ADDR3 = e->ADDR2;
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    decNumber *op3 = decNumberAdd(&_3, op1, op2, &set);
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N2, e->S2, e->SF2, e->R, &Ovr, &Trunc);

    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    //printf("%s\r\n", res);
    
    // now write to memory in proper format.....
    
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
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
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
                //write49(e, &dstAddr, &pos, e->dstTN, res[i] - '0');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, res[i]);
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
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
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    break;
            }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    //write49(e, &dstAddr, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    //write49(e, &dstAddr, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
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
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        //doFault(0,0,"ad2d truncation(overflow) fault");
        SETF(rIR, I_OFLOW);

    if (Ovr)
        SETF(rIR, I_OFLOW);
        
    //    doFault(0,0,"ad2d overflow fault");
    
}


/*
 * ad3d - Add Using Three Decimal Operands
 */
void ad3d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
//    switch(e->srcTN2)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            break;
//    }
 
    e->dstTN = e->TN3;    // type of chars in dst
    e->dstCN = e->CN3;    // starting at char pos CN
//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->dstAddr = e->YChar43;
//            break;
//        case CTN9:
//            e->dstAddr = e->YChar93;
//            break;
//    }
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, n3, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    decNumber *op3 = decNumberAdd(&_3, op1, op2, &set);
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N3, e->S3, e->SF3, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    //printf("%s\r\n", res);
    
    // now write to memory in proper format.....
    switch(e->S3)
    {
        case CSFL:
            n3 = e->N3 - 1; // need to account for the sign
            if (e->dstTN == CTN4)
                n3 -= 2;    // 2 4-bit digit exponent
            else
                n3 -= 1;    // 1 9-bit digit exponent
            break;
            
        case CSLS:
        case CSTS:
            n3 = e->N3 - 1; // 1 sign
            break;
            
        case CSNS:
            n3 = e->N3;     // no sign
            break;  // no sign wysiwyg
    }

    //word18 dstAddr = e->dstAddr;
    int pos = e->dstCN;
    
    // 1st, take care of any leading sign .......
    switch(e->S3)
    {
        case CSFL:  // floating-point, leading sign.
        case CSLS:  // fixed-point, leading sign
            switch(e->dstTN)
            {
            case CTN4:
                if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                break;
            }
            break;
            
        case CSTS:  // nuttin' to do here .....
        case CSNS:
            break;  // no sign wysiwyg
    }
    
    // 2nd, write the characteristic .....
    for(int i = 0 ; i < n3 ; i++)
        switch(e->dstTN)
        {
        case CTN4:
            //write49(e, &dstAddr, &pos, e->dstTN, res[i] - '0');
            EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
            break;
        case CTN9:
            //write49(e, &dstAddr, &pos, e->dstTN, res[i]);
            EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
            break;
        }
    
    // 3rd, take care of any trailing sign or exponent ...
    switch(e->S3)
    {
        case CSTS:  // write trailing sign ....
            switch(e->dstTN)
            {
            case CTN4:
                if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                break;
            }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
            case CTN4:
                //write49(e, &dstAddr, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                //write49(e, &dstAddr, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    
                    break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                break;
            }
            break;
            
        case CSLS:  // fixed point, leading sign - already done
        case CSNS:  // fixed point, unsigned - nuttin' needed to do
            break;
    }
    
    // set flags, etc ...
    if (e->S3 == CSFL)
    {
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        //doFault(0,0,"ad2d truncation(overflow) fault");
        SETF(rIR, I_OFLOW);
    
    if (Ovr)
        SETF(rIR, I_OFLOW);
    
    //    doFault(0,0,"ad2d overflow fault");
    
}

/*
 * sb2d - Subtract Using Two Decimal Operands
 */
void sb2d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
    
    e->dstTN = e->TN2;    // type of chars in dst
    e->dstCN = e->CN2;    // starting at char pos CN
//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            e->dstAddr = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            e->dstAddr = e->YChar92;
//            break;
//    }
    
    e->ADDR3 = e->ADDR2;
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    decNumber *op3 = decNumberSubtract(&_3, op1, op2, &set);
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N2, e->S2, e->SF2, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    //printf("%s\r\n", res);
    
    // now write to memory in proper format.....
    
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
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
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
                //write49(e, &dstAddr, &pos, e->dstTN, res[i] - '0');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, res[i]);
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
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
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    break;
            }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    //write49(e, &dstAddr, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    //write49(e, &dstAddr, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits

                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
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
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        SETF(rIR, I_OFLOW);
    
    if (Ovr)
        SETF(rIR, I_OFLOW);
        
}

/*
 * sb3d - Subtract Using Three Decimal Operands
 */
void sb3d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
//    switch(e->srcTN2)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            break;
//    }
    
    e->dstTN = e->TN3;    // type of chars in dst
    e->dstCN = e->CN3;    // starting at char pos CN
//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->dstAddr = e->YChar43;
//            break;
//        case CTN9:
//            e->dstAddr = e->YChar93;
//            break;
//    }
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, n3, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    decNumber *op3 = decNumberSubtract(&_3, op1, op2, &set);
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N3, e->S3, e->SF3, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    //printf("%s\r\n", res);
    
    // now write to memory in proper format.....
    switch(e->S3)
    {
        case CSFL:
            n3 = e->N3 - 1; // need to account for the sign
            if (e->dstTN == CTN4)
                n3 -= 2;    // 2 4-bit digit exponent
            else
                n3 -= 1;    // 1 9-bit digit exponent
            break;
            
        case CSLS:
        case CSTS:
            n3 = e->N3 - 1; // 1 sign
            break;
            
        case CSNS:
            n3 = e->N3;     // no sign
            break;  // no sign wysiwyg
    }
    
    //word18 dstAddr = e->dstAddr;
    int pos = e->dstCN;
    
    // 1st, take care of any leading sign .......
    switch(e->S3)
    {
        case CSFL:  // floating-point, leading sign.
        case CSLS:  // fixed-point, leading sign
            switch(e->dstTN)
        {
            case CTN4:
                if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                break;
        }
            break;
            
        case CSTS:  // nuttin' to do here .....
        case CSNS:
            break;  // no sign wysiwyg
    }
    
    // 2nd, write the characteristic .....
    for(int i = 0 ; i < n3 ; i++)
        switch(e->dstTN)
    {
        case CTN4:
            //write49(e, &dstAddr, &pos, e->dstTN, res[i] - '0');
            EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
            break;
        case CTN9:
            //write49(e, &dstAddr, &pos, e->dstTN, res[i]);
            EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
            break;
    }
    
    // 3rd, take care of any trailing sign or exponent ...
    switch(e->S3)
    {
        case CSTS:  // write trailing sign ....
            switch(e->dstTN)
            {
                case CTN4:
                    if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    break;
            }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    //write49(e, &dstAddr, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    //write49(e, &dstAddr, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits

                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                    break;
            }
            break;
            
        case CSLS:  // fixed point, leading sign - already done
        case CSNS:  // fixed point, unsigned - nuttin' needed to do
            break;
    }
    
    // set flags, etc ...
    if (e->S3 == CSFL)
    {
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        //doFault(0,0,"ad2d truncation(overflow) fault");
        SETF(rIR, I_OFLOW);
    
    if (Ovr)
        SETF(rIR, I_OFLOW);
    
    //    doFault(0,0,"ad2d overflow fault");
    
}

/*
 * mp2d - Multiply Using Two Decimal Operands
 */
void mp2d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
    
    e->dstTN = e->TN2;    // type of chars in dst
    e->dstCN = e->CN2;    // starting at char pos CN
//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            e->dstAddr = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            e->dstAddr = e->YChar92;
//            break;
//    }
    
    e->ADDR3 = e->ADDR2;
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    decNumber *op3 = decNumberMultiply(&_3, op1, op2, &set);
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N2, e->S2, e->SF2, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    // now write to memory in proper format.....
    
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
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
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
            //write49(e, &dstAddr, &pos, e->dstTN, res[i] - '0');
            EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
            break;
        case CTN9:
            //write49(e, &dstAddr, &pos, e->dstTN, res[i]);
            EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
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
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                break;
        }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    //write49(e, &dstAddr, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    //write49(e, &dstAddr, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
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
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        SETF(rIR, I_OFLOW);
    
    if (Ovr)
        SETF(rIR, I_OFLOW);
    
}

/*
 * mp3d - Multiply Using Three Decimal Operands
 */
void mp3d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
//    switch(e->srcTN2)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            break;
//    }
    
    e->dstTN = e->TN3;    // type of chars in dst
    e->dstCN = e->CN3;    // starting at char pos CN
//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->dstAddr = e->YChar43;
//            break;
//        case CTN9:
//            e->dstAddr = e->YChar93;
//            break;
//    }
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, n3, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    decNumber *op3 = decNumberMultiply(&_3, op1, op2, &set);
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N3, e->S3, e->SF3, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    //printf("%s\r\n", res);
    
    // now write to memory in proper format.....
    switch(e->S3)
    {
        case CSFL:
            n3 = e->N3 - 1; // need to account for the sign
            if (e->dstTN == CTN4)
                n3 -= 2;    // 2 4-bit digit exponent
            else
                n3 -= 1;    // 1 9-bit digit exponent
            break;
            
        case CSLS:
        case CSTS:
            n3 = e->N3 - 1; // 1 sign
            break;
            
        case CSNS:
            n3 = e->N3;     // no sign
            break;  // no sign wysiwyg
    }
    
    //word18 dstAddr = e->dstAddr;
    int pos = e->dstCN;
    
    // 1st, take care of any leading sign .......
    switch(e->S3)
    {
        case CSFL:  // floating-point, leading sign.
        case CSLS:  // fixed-point, leading sign
            switch(e->dstTN)
        {
            case CTN4:
                if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                else
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                break;
            case CTN9:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                break;
        }
            break;
            
        case CSTS:  // nuttin' to do here .....
        case CSNS:
            break;  // no sign wysiwyg
    }
    
    // 2nd, write the characteristic .....
    for(int i = 0 ; i < n3 ; i++)
        switch(e->dstTN)
        {
            case CTN4:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
                break;
            case CTN9:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
                break;
        }
    
        // 3rd, take care of any trailing sign or exponent ...
    switch(e->S3)
    {
        case CSTS:  // write trailing sign ....
            switch(e->dstTN)
            {
                case CTN4:
                    if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    break;
            }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                    break;
            }
            break;
            
        case CSLS:  // fixed point, leading sign - already done
        case CSNS:  // fixed point, unsigned - nuttin' needed to do
            break;
    }
    
    // set flags, etc ...
    if (e->S3 == CSFL)
    {
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        //doFault(0,0,"ad2d truncation(overflow) fault");
        SETF(rIR, I_OFLOW);
    
    if (Ovr)
        SETF(rIR, I_OFLOW);
    
    //    doFault(0,0,"ad2d overflow fault");
    
}


/*
 * dv2d - Divide Using Two Decimal Operands
 */
// XXX need to put in divide checks, etc ...
void dv2d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
    
    e->dstTN = e->TN2;    // type of chars in dst
    e->dstCN = e->CN2;    // starting at char pos CN
//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            e->dstAddr = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            e->dstAddr = e->YChar92;
//            break;
//    }
    
    e->ADDR3 = e->ADDR2;
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    decNumber *op3 = decNumberDivide(&_3, op2, op1, &set);  // Yes, they're reversed
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N2, e->S2, e->SF2, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    // now write to memory in proper format.....
    
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
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
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
                break;
            case CTN9:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    break;
            }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
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
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        SETF(rIR, I_OFLOW);
    
    if (Ovr)
        SETF(rIR, I_OFLOW);
    
}

/*
 * dv3d - Divide Using Three Decimal Operands
 */
// XXX need to put in divide checks, etc ...

void dv3d(EISstruct *e)
{
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = (bool)bitfieldExtract36(e->op0, 35, 1);  // 4-bit data sign character control
    e->T = (bool)bitfieldExtract36(e->op0, 26, 1);  // truncation bit
    e->R = (bool)bitfieldExtract36(e->op0, 25, 1);  // rounding bit
    
    e->srcTN = e->TN1;    // type of chars in src
    e->srcCN = e->CN1;    // starting at char pos CN
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
//    switch(e->srcTN2)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            break;
//    }
    
    e->dstTN = e->TN3;    // type of chars in dst
    e->dstCN = e->CN3;    // starting at char pos CN
//    switch(e->dstTN)
//    {
//        case CTN4:
//            e->dstAddr = e->YChar43;
//            break;
//        case CTN9:
//            e->dstAddr = e->YChar93;
//            break;
//    }
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, n3, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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
    
    
    decNumber *op3 = decNumberDivide(&_3, op2, op1, &set);  // Yes, they're switched
    
    bool Ovr = false, Trunc = false;
    
    int SF = calcSF(e->SF1, e->SF2, e->SF3);
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N3, e->S3, SF, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    //printf("%s\r\n", res);
    
    // now write to memory in proper format.....
    switch(e->S3)
    {
        case CSFL:
            n3 = e->N3 - 1; // need to account for the sign
            if (e->dstTN == CTN4)
                n3 -= 2;    // 2 4-bit digit exponent
            else
                n3 -= 1;    // 1 9-bit digit exponent
            break;
            
        case CSLS:
        case CSTS:
            n3 = e->N3 - 1; // 1 sign
            break;
            
        case CSNS:
            n3 = e->N3;     // no sign
            break;  // no sign wysiwyg
    }
    
    //word18 dstAddr = e->dstAddr;
    int pos = e->dstCN;
    
    // 1st, take care of any leading sign .......
    switch(e->S3)
    {
        case CSFL:  // floating-point, leading sign.
        case CSLS:  // fixed-point, leading sign
            switch(e->dstTN)
            {
                case CTN4:
                    if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    break;
            }
            break;
            
        case CSTS:  // nuttin' to do here .....
        case CSNS:
            break;  // no sign wysiwyg
    }
    
    // 2nd, write the characteristic .....
    for(int i = 0 ; i < n3 ; i++)
        switch(e->dstTN)
        {
            case CTN4:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i] - '0');
                break;
            case CTN9:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[i]);
                break;
            }
    
    // 3rd, take care of any trailing sign or exponent ...
    switch(e->S3)
    {
        case CSTS:  // write trailing sign ....
            switch(e->dstTN)
            {
                case CTN4:
                    if (e->P) //If TN2 and S2 specify a 4-bit signed number and P = 1, then the 13(8) plus sign character is placed appropriately if the result of the operation is positive.
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                break;
            }
            break;
            
        case CSFL:  // floating-point, leading sign.
            // write the exponent
            switch(e->dstTN)
            {
                case CTN4:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (op3->exponent >> 4) & 0xf); // upper 4-bits
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  op3->exponent       & 0xf); // lower 4-bits
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, op3->exponent & 0xff);    // write 8-bit exponent
                    break;
            }
            break;
            
        case CSLS:  // fixed point, leading sign - already done
        case CSNS:  // fixed point, unsigned - nuttin' needed to do
            break;
    }
    
    // set flags, etc ...
    if (e->S3 == CSFL)
    {
        if (op3->exponent > 127)
            SETF(rIR, I_EOFL);
        if (op3->exponent < -128)
            SETF(rIR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), rIR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), rIR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, rIR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
        //doFault(0,0,"ad2d truncation(overflow) fault");
        SETF(rIR, I_OFLOW);
    
    if (Ovr)
        SETF(rIR, I_OFLOW);
    
    //    doFault(0,0,"ad2d overflow fault");
    
}

/*
 * cmpn - Compare Numeric
 */
void cmpn(EISstruct *e)
{
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
//    switch(e->srcTN)
//    {
//        case CTN4:
//            e->srcAddr = e->YChar41;
//            break;
//        case CTN9:
//            e->srcAddr = e->YChar91;
//            break;
//    }
    
    e->srcTN2 = e->TN2;    // type of chars in dst
    e->srcCN2 = e->CN2;    // starting at char pos CN
    
//    switch(e->srcTN2)
//    {
//        case CTN4:
//            e->srcAddr2 = e->YChar42;
//            break;
//        case CTN9:
//            e->srcAddr2 = e->YChar92;
//            break;
//    }
    
    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1, _2, _3;
    
    int n1, n2, sc1, sc2;
    
    EISloadInputBufferNumeric(e, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(e, 2);   // according to MF2
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

    SCF(cSigned == 0, rIR, I_ZERO);
    SCF(cSigned == 1, rIR, I_NEG);
    SCF(cMag == 1, rIR, I_CARRY);
}
