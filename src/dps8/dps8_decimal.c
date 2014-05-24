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

#include <stdio.h>

#include "dps8.h"
#include "dps8_cpu.h"
#include "dps8_decimal.h"
#include "dps8_eis.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_faults.h"

#define DECUSE64    1
#define DECSUBSET   1
#define DECDPUN     8
#define DECBUFFER   32

#define DECNUMDIGITS    64

#include "decNumber.h"        // base number library
#include "decNumberLocal.h"   // decNumber local types, etc.



//void write49(EISstruct *e, word18 *dstAddr, int *pos, int tn, int c49);
//void loadInputBufferNumeric(EISstruct *e, int k);

//void printBCD(decNumber *a, decContext *set, int width);
//decNumber *decBCDToNumber(const uint8_t *bcd, int length, const int scale, decNumber *dn);
//uint8_t * decBCDFromNumber(uint8_t *bcd, int length, int *scale, const decNumber *dn);

#define PRINTDEC(msg, dn) \
    { \
        char temp[256]; \
        decNumberToString(dn, temp); \
        sim_printf("%s:'%s'\n", msg, temp);   \
    }


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
#ifndef QUIET_UNUSED
static decNumber * decBCDToNumber(const uByte *bcd, Int length, const Int scale, decNumber *dn)
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
    for (;last >= bcd;) {                            // forever
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
#endif

static decNumber * decBCD9ToNumber(const word9 *bcd, Int length, const Int scale, decNumber *dn)
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
    digits=(Int)(last-first)+1;              // calculate digits ..
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
    for (;last >= bcd;) {                            // forever
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
static uint8_t * decBCDFromNumber(uint8_t *bcd, int length, int *scale, const decNumber *dn) {
    const Unit *up=dn->lsu;     // Unit array pointer
    uByte obyte=0, *out;          // current output byte, and where it goes
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
            obyte=nib & 255U;
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


#ifndef QUIET_UNUSED
static void printBCD(decNumber *a, decContext *set, int width)
{
    uint8_t bcd[256];
    memset(bcd, 0, sizeof(bcd));
    
    decNumberGetBCD(a, bcd);
    fprintf(stderr, "Bcd: %c", decNumberIsNegative(a) ? '-' : '+');
    
    for(int n = 0 ; n < width-1 ; n += 1)
        fprintf(stderr, "%d", bcd[n]);
    fprintf(stderr, "  scale=%d\n", -(a->exponent));
}
#endif

static char *getBCD(decNumber *a)
{
    static uint8_t bcd[256];
    memset(bcd, 0, sizeof(bcd));
    int scale;
    
    decBCDFromNumber(bcd, a->digits, &scale, a);
    for(int i = 0 ; i < a->digits ; i += 1 )
        bcd[i] += '0';
    
    return (char *) bcd;
    
    
}


#ifndef QUIET_UNUSED
static int calcOD(int lengthOfDividend,
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
#endif

static const char *CS[] = {"CSFL", "CSLS", "CSTS", "CSNS"};
static const char *CTN[] = {"CTN9", "CTN4"};

static int calcSF(int sf1, int sf2, int sf3)
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

static char *formatDecimal(decContext *set, decNumber *r, int tn, int n, int s, int sf, bool R, bool *OVR, bool *TRUNC)
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
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "\nformatDecimal: adjLen=%d SF=%d S=%s TN=%s\n", adjLen, sf, CS[s], CTN[tn]);
    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "formatDecimal: %s  r->digits=%d  r->exponent=%d\n", getBCD(r), r->digits, r->exponent);
    
    if (adjLen < 1)
    {
        // adjusted length is too small for anything but sign and/or exponent
        //*OVR = 1;
        
        // XXX what do we fill in here? Sign and exp?
        *OVR = true;
        return (char *)"";
    }
    
    // scale result (if not floating)
    
    decNumber _r2;
    decNumberZero(&_r2);
    
    decNumber *r2 = &_r2;
    
    decNumber _sf;
    
    {
        decNumberTrim(r);   // clean up any trailing 0's
        
        int scale;
        char out[256], out2[256];

        if_sim_debug (DBG_TRACEEXT, & cpu_dev)
        {
            bzero(out, sizeof(out));
            bzero(out2, sizeof(out2));
        
            decBCDFromNumber((uint8_t *)out, r->digits, &scale, r);
            for(int i = 0 ; i < r->digits ; i += 1 )
                out[i] += '0';
        }
        
        if (s != CSFL)
        {
            decNumberFromInt32(&_sf, sf);
            
            r2 = decNumberRescale(&_r2, r, &_sf, set);
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "formatDecimal: %s r2->digits=%d r2->exponent=%d\n", getBCD(r2), r2->digits, r2->exponent);
        }
        else
            *r2 = *r;
        
        // this breaks things so dont do it
        //decNumberReduce(r2, r2, set);   // clean up any trailing 0's
        //decNumberTrim(r2);   // clean up any trailing 0's
        
        if_sim_debug (DBG_TRACEEXT, & cpu_dev)
        {
            decBCDFromNumber((uint8_t *)out2, r2->digits, &scale, r2);
            for(int i = 0 ; i < r2->digits ; i += 1 )
                out2[i] += '0';
        
            sim_debug (DBG_TRACEEXT, & cpu_dev,
              "formatDecimal: adjLen=%d E=%d SF=%d S=%s TN=%s digits(r2)=%s E2=%d\n", adjLen, r->exponent, sf, CS[s], CTN[tn],out2, r2->exponent);
        }
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
        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "formatDecimal(OK): r->digits(%d) <= adjLen(%d) r2->digits(%d)\n", r->digits, adjLen, r2->digits);
        if (s == CSFL)
            if (r2->digits < adjLen)
            {
                //PRINTDEC("Value 1", r2)
                
                decNumber _s, *sc;
                int rescaleFactor = r2->exponent - (adjLen - r2->digits);
                sc = decNumberFromInt32(&_s, rescaleFactor); //r2->exponent - (adjLen - r2->digits));
                //sc = decNumberFromInt32(&_s, abs(r2->exponent - (adjLen - r2->digits)));
                //PRINTDEC("Value sc", sc)

                if (rescaleFactor > (adjLen - r2->digits))
                    r2 = decNumberRescale(r2, r2, sc, set);
                
                //PRINTDEC("Value 2", r2)
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
        
        sim_debug (DBG_TRACEEXT, & cpu_dev,
          "formatDecimal(%s): r->digits %d > adjLen %d\n", R ? "R" : "", r->digits, adjLen);
        
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
                
                // HWR 24 Oct 2013
                char temp[256];
                strcpy(temp, (char *) out+set->digits-adjLen);
                strcpy((char *) out, temp);
                //strcpy(out, out+set->digits-adjLen); // this generates a SIGABRT - probably because of overlapping strings.
                
                sim_debug (DBG_TRACEEXT, & cpu_dev, "R OVR\n");
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
            
            if_sim_debug (DBG_TRACEEXT, & cpu_dev)
            {
                decNumber _i;
                decNumber *i = decNumberToIntegralValue(&_i, ro, set);
                char outi[256];
                bzero(outi, sizeof(outi));
                decBCDFromNumber((uint8_t *)outi, adjLen, &scale, i);
                for(int j = 0 ; j < adjLen; j += 1 )
                    outi[j] += '0';
                sim_debug (DBG_TRACEEXT, & cpu_dev, "i=%s\n", outi);
            }
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
                enum rounding safeR = decContextGetRounding(set);         // save rounding mode
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
                
                sim_debug (DBG_TRACEEXT, & cpu_dev, "CSFL TRUNC\n");
                trunc = true;
            }
            else
            {
                if (r2->digits < r->digits)
                {
                    enum rounding safeR = decContextGetRounding(set);         // save rounding mode
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
                    
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "TRUNC\n");
                    trunc = true;
                    
                } else if (r->digits > adjLen)
                {
                    // OVR
                    decBCDFromNumber(out, r->digits, &scale, r);
                    for(int i = 0 ; i < r->digits ; i += 1 )
                        out[i] += '0';
                    out[r->digits] = 0;
                    
                    // HWR 24 Oct 2013
                    char temp[256];
                    strcpy(temp, (char *) out+r->digits-adjLen);
                    strcpy((char *) out, temp);
                    //strcpy(out, out+r->digits-adjLen); // this generates a SIGABRT - probably because of overlapping strings.
                    
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "OVR\n");
                    ovr = true;
                }
                else
                    sim_printf("formatDecimal(): How'd we get here?\n");
            }
        }
    }
    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "formatDecimal: ovrflow=%d trunc=%d R=%d\n", ovr, trunc, R);
    *OVR = ovr;
    *TRUNC = trunc;
    
    decNumberCopy(r, r2);
    return (char *) out;
}

/*
 * decimal EIS instructions ... 
 */


#define ASC(x)  ((x) + '0')

/*
 * ad2d - Add Using Two Decimal Operands
 */
void ad2d (void)
{
    DCDstruct * i = & currentInstruction;
    EISstruct *e = &i->e;
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    
    e->P = bitfieldExtract36(e->op0, 35, 1) != 0;  // 4-bit data sign character control
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;  // truncation bit
    e->R = bitfieldExtract36(e->op0, 25, 1) != 0;  // rounding bit
    
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
    
    int n1 = 0, n2 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(i, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(i, 2);   // according to MF2
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
    for(int j = 0 ; j < n2 ; j++)
        switch(e->dstTN)
        {
            case CTN4:
                //write49(e, &dstAddr, &pos, e->dstTN, res[j] - '0');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[j] - '0');
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, res[j]);
                EISwrite49(&e->ADDR3, &pos, e->dstTN, res[j]);
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"ad2d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"ad2d overflow fault");
    }
}


/*
 * ad3d - Add Using Three Decimal Operands
 */
void ad3d (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = bitfieldExtract36(e->op0, 35, 1) != 0;  // 4-bit data sign character control
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;  // truncation bit
    e->R = bitfieldExtract36(e->op0, 25, 1) != 0;  // rounding bit
    
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
    
    int n1 = 0, n2 = 0, n3 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"ad3d truncation(overflow) fault");
    }
    
    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"ad3d overflow fault");
    }
}

/*
 * sb2d - Subtract Using Two Decimal Operands
 */
void sb2d (void)
{
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
    
    int n1 = 0, n2 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
            doFault(overflow_fault, 0,"sb2d truncation (overflow) fault");
    }
    
    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"sb2d overflow fault");
    }
}

/*
 * sb3d - Subtract Using Three Decimal Operands
 */
void sb3d (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = bitfieldExtract36(e->op0, 35, 1) != 0;  // 4-bit data sign character control
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;  // truncation bit
    e->R = bitfieldExtract36(e->op0, 25, 1) != 0;  // rounding bit
    
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
    
    int n1 = 0, n2 = 0, n3 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"sb3d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"sb3d overflow fault");
    }
}

/*
 * mp2d - Multiply Using Two Decimal Operands
 */
void mp2d (void)
{
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
    
    int n1 = 0, n2 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"mp2d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"mp2d overflow fault");
    }
    
}

/*
 * mp3d - Multiply Using Three Decimal Operands
 */
void mp3d (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = bitfieldExtract36(e->op0, 35, 1) != 0;  // 4-bit data sign character control
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;  // truncation bit
    e->R = bitfieldExtract36(e->op0, 25, 1) != 0;  // rounding bit
    
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
    
    int n1 = 0, n2 = 0, n3 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"mp3d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"mp3d overflow fault");
    }
}


/*
 * dv2d - Divide Using Two Decimal Operands
 */
// XXX need to put in divide checks, etc ...
void dv2d (void)
{
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
    
    int n1 = 0, n2 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"dv2d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"dv2d overflow fault");
    }
}

/*
 * dv3d - Divide Using Three Decimal Operands
 */
// XXX need to put in divide checks, etc ...

void dv3d (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
    setupOperandDescriptor(3, e);
    
    parseNumericOperandDescriptor(1, e);
    parseNumericOperandDescriptor(2, e);
    parseNumericOperandDescriptor(3, e);
    
    e->P = bitfieldExtract36(e->op0, 35, 1) != 0;  // 4-bit data sign character control
    e->T = bitfieldExtract36(e->op0, 26, 1) != 0;  // truncation bit
    e->R = bitfieldExtract36(e->op0, 25, 1) != 0;  // rounding bit
    
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
    
    int n1 = 0, n2 = 0, n3 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    //PRINTDEC("op1", op1);
    
    if (e->sign == -1)
        op1->bits = DECNEG;
    if (e->S1 == CSFL)
        op1->exponent = e->exponent;
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
    
    // TODO: Need to check/implement this
    // The number of required quotient digits, NQ, is determined before division begins as follows:
    //  1) Floating-point quotient
    //      NQ = N3, but if the divisor is greater than the dividend after operand alignment, the leading zero digit produced is counted and
    //      the effective precision of the result is reduced by one.
    //  2) Fixed-point quotient
    //    NQ = (N2-LZ2+1) - (N1-LZ1) + (E2-E1-SF3)
    //    ￼where: Nn = given operand field length
    //        LZn = leading zero count for operand n
    //        En = exponent of operand n
    //        SF3 = scaling factor of quotient
    // 3) Rounding
    //    If rounding is specified (R = 1), then one extra quotient digit is produced.
    
    
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
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF(decNumberIsNegative(op3), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"dv3d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(overflow_fault, 0,"dv3d overflow fault");
    }
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
    
    int n1 = 0, n2 = 0, sc1 = 0, sc2 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    
    EISloadInputBufferNumeric(ins, 2);   // according to MF2
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
    SCF(cMag == 1, cu.IR, I_CARRY);
}

/*
 * mvn - move numeric (initial version was deleted by house gnomes)
 */
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
    
    sim_debug (DBG_TRACEEXT, & cpu_dev, "mvn(1): TN1 %d CN1 %d N1 %d TN2 %d CN2 %d N2 %d\n", e->TN1, e->CN1, e->N1, e->TN2, e->CN2, e->N2);
    sim_debug (DBG_TRACEEXT, & cpu_dev, "mvn(2): OP1 %012llo OP2 %012llo\n", e->OP1, e->OP2);

    decContext set;
    decContextDefault(&set, DEC_INIT_BASE);         // initialize
    
    decNumber _1;
    
    int n1 = 0, n2 = 0, sc1 = 0;
    
    EISloadInputBufferNumeric(ins, 1);   // according to MF1
    
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
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
      "n1 %d sc1 %d\n",
      n1, sc1);

    decNumber *op1 = decBCD9ToNumber(e->inBuffer, n1, sc1, &_1);
    if (e->sign == -1)
        op1->bits = DECNEG;
    if (e->S1 == CSFL)
        op1->exponent = e->exponent;
    if (decNumberIsZero(op1))
        op1->exponent = 127;
    
    bool Ovr = false, Trunc = false;
    
    int SF = calcSF(e->SF1, e->SF2, 0); 
    
    char *res = formatDecimal(&set, op1, e->dstTN, e->N2, e->S2, SF, e->R, &Ovr, &Trunc);
    
    //printf("%s\r\n", res);
    
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
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
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
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, decNumberIsNegative(op1) ? 015 : 013);  // special +
                    else
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, decNumberIsNegative(op1) ? 015 : 014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR2, &pos, e->dstTN, decNumberIsNegative(op1) ? '-' : '+');
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
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, decNumberIsNegative(op1) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR2, &pos, e->dstTN, decNumberIsNegative(op1) ? 015 :  014);  // default +
                    break;
            
                case CTN9:
                    EISwrite49(&e->ADDR2, &pos, e->dstTN, decNumberIsNegative(op1) ? '-' : '+');
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
    
    SCF(decNumberIsNegative(op1), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op1), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
    if (e->T && Trunc)
    {
        SETF(cu.IR, I_OFLOW);
        doFault(overflow_fault, 0,"mvn truncation(overflow) fault");
    }
    
    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
          doFault(overflow_fault, 0,"mvn overflow fault");
    }

}


