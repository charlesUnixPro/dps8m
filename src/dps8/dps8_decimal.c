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
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_decimal.h"
#include "dps8_eis.h"
#include "dps8_utils.h"
#include "dps8_faults.h"

//void write49(EISstruct *e, word18 *dstAddr, int *pos, int tn, int c49);
//void loadInputBufferNumeric(EISstruct *e, int k);

//void printBCD(decNumber *a, decContext *set, int width);
//decNumber *decBCDToNumber(const uint8_t *bcd, int length, const int scale, decNumber *dn);
//uint8_t * decBCDFromNumber(uint8_t *bcd, int length, int *scale, const decNumber *dn);


/* ------------------------------------------------------------------ */
/* HWR 6/28/14 18:54 derived from ......                              */
/*     decContextDefault(...)                                         */
/*                                                                    */
/* decContextDefaultDPS8 -- initialize a context structure            */
/*                                                                    */
/* Similar to decContextDefault EXCEPT digits are set to 65 for our   */
/* dps8 simulator (add additional features as required                */
/*                                                                    */
/* ------------------------------------------------------------------ */
decContext * decContextDefaultDPS8(decContext *context)
{
    decContextDefault(context, DEC_INIT_BASE);
    context->traps=0;

    context->digits = 65;
    
    return context;
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
//#ifndef QUIET_UNUSED
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
    for (first=bcd; *first==0 && first <= last;) first++;
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
//#endif

decNumber * decBCD9ToNumber(const word9 *bcd, Int length, const Int scale, decNumber *dn)
{
    const word9 *last=bcd+length-1;  // -> last byte
    const word9 *first;              // -> first non-zero byte
    uInt  nib;                       // work nibble
    Unit  *up=dn->lsu;               // output pointer
    Int   digits;                    // digits count
    Int   cut=0;                     // phase of output
    
    decNumberZero(dn);               // default result
    //last = &bcd[length-1];
    //nib = *last & 0x0f;                // get the sign
    //if (nib==DECPMINUS || nib==DECPMINUSALT) dn->bits=DECNEG;
    //else if (nib<=9) return NULL;   // not a sign nibble
    
    // skip leading zero bytes [final byte is always non-zero, due to sign]
    //for (first=bcd; *first==0;) first++;
    
    //Also, a bug in decBCD9ToNumber; in the input is all zeros, the skip leading zeros code wanders off the end of the input buffer....
    for (first=bcd; *first==0 && first <= last;) first++;
    
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
    
    //PRINTDEC("decBCDFromNumber()", dn);
    
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
    sim_printf ("Bcd: %c", decNumberIsNegative(a) ? '-' : '+');
    
    for(int n = 0 ; n < width-1 ; n += 1)
        sim_printf ("%d", bcd[n]);
    sim_printf ("  scale=%d\n", -(a->exponent));
}
#endif

static unsigned char *getBCD(decNumber *a)
{
    static uint8_t bcd[256];
    memset(bcd, 0, sizeof(bcd));
    int scale;
    
    decBCDFromNumber(bcd, a->digits, &scale, a);
    for(int i = 0 ; i < a->digits ; i += 1 )
        bcd[i] += '0';
    
    return (unsigned char *) bcd;
    
    
}
static char *getBCDn(decNumber *a, int digits)
{
    static uint8_t bcd[256];
    memset(bcd, 0, sizeof(bcd));
    int scale;
    
    decBCDFromNumber(bcd, digits, &scale, a);
    for(int i = 0 ; i < digits ; i += 1 )
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
    //sim_printf ("calcSF(): How'd we get here?\n");
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
    
    int adjLen = n;             // adjLen is the adjusted allowed length of the result taking into account signs and/or exponent
    switch (s)
    {
        case CSFL:              // we have a leading sign and a trailing exponent.
            if (tn == CTN9)
                adjLen -= 2;    // a sign and an 1 9-bit exponent
            else
                adjLen -= 3;    // a sign and 2 4-bit digits making up the exponent
            break;
        case CSLS:
        case CSTS:              // take sign into assount. One less char to play with
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
    
    decNumber _sf;  // scaling factor
    {
        //decNumberTrim(r);   // clean up any trailing 0's
        
#ifndef SPEED
        int scale;
        char out[256], out2[256];
        
        if_sim_debug (DBG_TRACEEXT, & cpu_dev)
        {
            bzero(out, sizeof(out));
            bzero(out2, sizeof(out2));
            
            decBCDFromNumber((uint8_t *)out, r->digits, &scale, r);
            for(int i = 0 ; i < r->digits ; i += 1 )
                out[i] += '0';
            sim_printf("formatDecimal(DEBUG): out[]: '%s'\n", out);
        }
#endif
        
        if (s != CSFL)// && sf != 0)
        {
            decNumberFromInt32(&_sf, sf);
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "formatDecimal(s != CSFL a): %s r->digits=%d r->exponent=%d\n", getBCD(r), r->digits, r->exponent);
            r2 = decNumberRescale(&_r2, r, &_sf, set);
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "formatDecimal(s != CSFL b): %s r2->digits=%d r2->exponent=%d\n", getBCD(r2), r2->digits, r2->exponent);
        }
        else
            //*r2 = *r;
            decNumberCopy(r2, r);
        
#ifndef SPEED
        if_sim_debug (DBG_TRACEEXT, & cpu_dev)
        {
            decBCDFromNumber((uint8_t *)out2, r2->digits, &scale, r2);
            for(int i = 0 ; i < r2->digits ; i += 1 )
                out2[i] += '0';
            
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "formatDecimal: adjLen=%d E=%d SF=%d S=%s TN=%s digits(r2)=%s E2=%d\n", adjLen, r->exponent, sf, CS[s], CTN[tn],out2, r2->exponent);
        }
#endif
    }
    
    int scale;
    
    static uint8_t out[256];
    
    bzero(out, sizeof(out));
    
    //bool ovr = (r->digits-sf) > adjLen;     // is integer portion too large to fit?
    bool ovr = r2->digits > adjLen;          // is integer portion too large to fit?
    bool trunc = r->digits > r2->digits;     // did we loose something along the way?
    
    // now let's check for overflows
    if (!ovr && !trunc)
    {
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "formatDecimal(OK): r->digits(%d) <= adjLen(%d) r2->digits(%d)\n", r->digits, adjLen, r2->digits);
        if (s == CSFL)
            if (r2->digits < adjLen)
            {
                PRINTDEC("Value 1", r2)
                
                decNumber _s, *sc;
                int rescaleFactor = r2->exponent - (adjLen - r2->digits);
                sc = decNumberFromInt32(&_s, rescaleFactor);
                
                PRINTDEC("Value sc", sc)
                if (rescaleFactor > (adjLen - r2->digits))
                    r2 = decNumberRescale(r2, r2, sc, set);
                
                PRINTDEC("Value 2", r2)
            }
        decBCDFromNumber(out, adjLen, &scale, r2);
        for(int i = 0 ; i < adjLen ; i += 1 )
            out[i] += '0';
        //sim_printf("out[ot]='%s'\n", out);
    }
    else
    {
        ovr = false;
        trunc = false;
        
        // if we get here then we have either overflow or truncation....
        
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "formatDecimal(!OK%s): r2->digits %d adjLen %d\n", R ? " R" : "", r2->digits, adjLen);
        
        // so, what do we do?
        if (R)
        {
            // NB even with rounding you can have an overflow...
            
            // if we're in rounding mode then we just make things fit and everything is OK - except if we have an overflow.
            
            decNumber *ro = r2; //(s == CSFL ? r : r2);
            
            int safe = set->digits;
            
            if (ro->digits > adjLen)    //(adjLen + 1))
            {
                //set->digits = ro->digits + sf + 1;
                sim_debug (DBG_TRACEEXT, & cpu_dev,
                           "formatDecimal(!OK R1): ro->digits %d adjLen %d\n", ro->digits, adjLen);

                set->digits = adjLen;
                decNumberPlus(ro, ro, set);
                
                decBCDFromNumber(out, set->digits, &scale, ro);
                for(int i = 0 ; i < set->digits ; i += 1 )
                    out[i] += '0';
                
                // HWR 24 Oct 2013
                char temp[256];
                strcpy(temp, (char *) out+set->digits-adjLen);
                strcpy((char *) out, temp);
                
                //strcpy(out, out+set->digits-adjLen); // this generates a SIGABRT - probably because of overlapping strings.
                
                //sim_debug (DBG_TRACEEXT, & cpu_dev, "R OVR\n");
                //ovr = true; breaks ET MVN 5
            }
            else
            {
                sim_debug (DBG_TRACEEXT, & cpu_dev,
                         "formatDecimal(!OK R2): ro->digits %d adjLen %d\n", ro->digits, adjLen);

                if (s==CSFL)
                {
                    
                    set->digits = adjLen;
                    decNumberPlus(ro, ro, set);
                    
                    decBCDFromNumber(out, adjLen, &scale, ro);
                    for(int i = 0 ; i < adjLen ; i += 1 )
                        out[i] += '0';
                    out[adjLen] = 0;
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "formatDecimal(!OK R2a): %s\n", out);

                }
                else
                {
                    int dig = set->digits;
                    set->digits = adjLen;
                    ro = decNumberPlus(ro, ro, set);    // round to adjLen digits
                    decBCDFromNumber((uint8_t *)out, adjLen, &scale, ro);
                    set->digits = dig;

                    
//                    decNumber _i;
//                    decNumber *i = decNumberToIntegralValue(&_i, ro, set);
//                    decBCDFromNumber((uint8_t *)out, adjLen, &scale, i);
                    
                    for(int j = 0 ; j < adjLen; j += 1 )
                        out[j] += '0';
                    
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "formatDecimal(!OK R2b): %s\n", out);
                }
                ovr = false;    // since we've rounded we can have no overflow ?????
            }
            sim_debug (DBG_TRACEEXT, & cpu_dev, "formatDecimal(R3): digits:'%s'\n", out);
            
            set->digits = safe;
            
            // display int of number
            
#ifndef SPEED
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
#endif
        }
        else
        {
            // if we're not in rounding mode then we can either have a truncation or an overflow
            
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
                    
//                } else if ((r2->digits-sf) > adjLen)     // HWR 18 July 2014 was (r->digits > adjLen)
                } else if ((r2->digits) > adjLen)     // HWR 18 July 2014 was (r->digits > adjLen)
                {
                    // OVR
                    decBCDFromNumber(out, r2->digits, &scale, r2);
                    for(int i = 0 ; i < r2->digits ; i += 1 )
                        out[i] += '0';
                    out[r2->digits] = 0;
                    
                    // HWR 24 Oct 2013
                    char temp[256];
                    strcpy(temp, (char *) out+r2->digits-adjLen);
                    strcpy((char *) out, temp);
                    //strcpy(out, out+r->digits-adjLen); // this generates a SIGABRT - probably because of overlapping strings.
                    
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "OVR\n");
                    ovr = true;
                }
                else
                    sim_printf("formatDecimal(?): How'd we get here?\n");
            }
        }
    }
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "formatDecimal(END): ovrflow=%d trunc=%d R=%d out[]='%s'\n", ovr, trunc, R, out);
    *OVR = ovr;
    *TRUNC = trunc;
    
    decNumberCopy(r, r2);
    return (char *) out;
}

// If the lhs is less than the rhs in the total order then the number will be set to the value -1. If they are equal, then number is set to 0. If the lhs is greater than the rhs then the number will be set to the value 1.
int decCompare(decNumber *lhs, decNumber *rhs, decContext *set)
{
    decNumber _cmp, *cmp;
    cmp = decNumberCompareTotal(&_cmp, lhs, rhs, set);
    
    if (decNumberIsZero(cmp))
        return 0;   // lhs == rhs
    
    if (decNumberIsNegative(cmp))
        return -1;  // lhs < rhs
    
    return 1;       // lhs > rhs
}
int decCompareMAG(decNumber *lhs, decNumber *rhs, decContext *set)
{
    decNumber _cmpm, *cmpm;
    cmpm = decNumberCompareTotalMag(&_cmpm, lhs, rhs, set);
    
    if (decNumberIsZero(cmpm))
        return 0;   // lhs == rhs
    
    if (decNumberIsNegative(cmpm))
        return -1;  // lhs < rhs
    
    return 1;       // lhs > rhs
}

int findFirstDigit(unsigned char *bcd)
{
    int i = 0;
    while (bcd[i] == '0' && bcd[i])
        i += 1;
    
    return i;
}

/*
 * output formatting for DV?X (divide) instructions ....
 */
static char *formatDecimalDIV(decContext *set, decNumber *r, int tn, int n, int s, int sf, bool R, decNumber *num, decNumber *den, bool *OVR, bool *TRUNC)
{

//    decNumber _cmp, *cmp;
//    cmp = decNumberCompareTotal(&_cmp, num, den, set);
//    bool bAdj = decNumberIsNegative(cmp);  // if denominator is > numerator then remove leading 0
//    //    if (bAdj)
//    sim_printf("bAdj == %d denom > num\n", decNumberToInt32(cmp, set));
    
//    decNumber _cmpm, *cmpm;
//    cmpm = decNumberCompareTotalMag(&_cmpm, num, den, set);
//    //    bool bNgtD = !decNumberIsZero(cmpm) && !decNumberIsNegative(cmpm);
//    bool bNgtD = decNumberIsNegative(cmpm);  // if denominator is > numerator then remove leading 0
//    //    if (bNgtD)
//    sim_printf("bNgtD == %d |denom > num|\n", decNumberToInt32(cmpm, set));

    bool bDgtN = false;

// this is the sane way to do it.....
//    bool bDgtN = decCompare(num, den, set) == -1;
//    if (s == CSFL && bDgtN)
//        sim_printf("den > num\n");

        // 1) Floating-point quotient
        //NQ = N2, but if the divisor is greater than the dividend after operand alignment, the leading zero digit produced is counted and the effective precision of the result is reduced by one.
    if (s == CSFL)
    {
        //sim_printf("floating result ...\n");
        
//            decNumber _4, _5, _6a, _6b, *op4, *op5, *op6a, *op6b;
//            
//            // we want to make exponents the same so as to align the operands.
//            // ... which one has priority? dividend or divisor? >punt<
//            
//            op4 = decNumberReduce(&_4, num, set);
//            op5 = decNumberReduce(&_5, den, set);
//
////            op4 = decNumberCopy(&_4, num);
////            op5 = decNumberCopy(&_5, den);
//            
//            op4->exponent = 0;
//            op5->exponent = 0;
//            //op6a = decNumberQuantize(&_6a, op4, op5, set);
//            //op6b = decNumberQuantize(&_6b, op5, op4,  set);
//            
//            //PRINTALL("align 4 (num/dividend)", op4, set);
//            //PRINTALL("align 5 (den/divisor) ", op5, set);
//            
//            PRINTDEC("align 4 (num/dividend)", op4);
//            PRINTDEC("align 5 (den/divisor) ", op5);
//            //PRINTDEC("align 6a (nd)         ", op6a);
//            //PRINTDEC("align 6b (dn)         ", op6b);
//            
//            decNumber _cmp, *cmp;
//            cmp = decNumberCompareTotal(&_cmp, op4, op5, set);
//            bool bAdj2 = decNumberIsNegative(cmp);  // if denominator is > numerator then remove leading 0
//            //    if (bAdj)
//            sim_printf("bAdj2 == %d\n", decNumberToInt32(cmp, set));
//            
//            
//        }
        
        //  The dividend mantissa C(AQ) is shifted right and the dividend exponent
        //  C(E) increased accordingly until
        //  | C(AQ)0,63 | < | C(Y-pair)8,71 |
        //  | numerator | < |  denominator  |
        //  | dividend  | < |    divisor    |
        
        // start by determining the characteristic(s) of dividend / divisor
        
        decNumber _dend, _dvsr, *dividend, *divisor;

        dividend = decNumberCopy(&_dend, num);
        divisor = decNumberCopy(&_dvsr, den);
        
        // set exponents to zero to yield the characteristic
        dividend->exponent = 0;
        divisor->exponent = 0;
//        
//        decNumber _one, *one = decNumberFromInt32(&_one, -1);
//        int c = decCompare(dividend, divisor, set);
//        sim_printf("c0 = %d\n", c);

        
        // we want to do a funky fractional alignment here so we can compare the mantissa's
        
        unsigned char *c1 = getBCD(num);
        int f1 = findFirstDigit(c1);
        dividend = decBCDToNumber(c1+f1, 63, 63, &_dend);
        PRINTDEC("aligned dividend", dividend);
        
        unsigned char *c2 = getBCD(den);
        int f2 = findFirstDigit(c2);
        divisor = decBCDToNumber(c2+f2, 63, 63, &_dvsr);
        PRINTDEC("aligned divisor", divisor);
        
        
//        PRINTALL("BCD 1 num/dividend", dividend, set);
//        PRINTALL("BCD 1 den/divisor ", divisor, set);
//
//            decNumberReduce(dividend, dividend, set);
//            decNumberReduce(divisor, divisor, set);
//
//        PRINTDEC("dividend", dividend);
//        PRINTDEC("divisor ", divisor);
//        PRINTALL("BCD 2 num/dividend", dividend, set);
//        PRINTALL("BCD 2 den/divisor ", divisor, set);

        
        if (decCompareMAG(dividend, divisor, set) == -1)
        {
           // sim_printf("dividend < divisor (aligned)\n");
            bDgtN = true;
        }
            
    }

    if (s == CSFL)
        sf = 0;
    
    // XXX what happens if we try to write a negative number to an unsigned field?????
    // Detection of a character outside the range [0,11]8 in a digit position or a character outside the range [12,17]8 in a sign position causes an illegal procedure fault.
    
    // adjust output length according to type ....
    //This implies that an unsigned fixed-point receiving field has a minimum length of 1 character; a signed fixed-point field, 2 characters; and a floating-point field, haracters.
    
    int adjLen = n;             // adjLen is the adjusted allowed length of the result taking into account signs and/or exponent
    switch (s)
    {
        case CSFL:              // we have a leading sign and a trailing exponent.
            if (tn == CTN9)
                adjLen -= 2;    // a sign and an 1 9-bit exponent
            else
                adjLen -= 3;    // a sign and 2 4-bit digits making up the exponent
            break;
        case CSLS:
        case CSTS:              // take sign into assount. One less char to play with
            adjLen -= 1;
            break;
        case CSNS:
            break;          // no sign to worry about. Use everything
    }
    
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "\nformatDecimal: adjLen=%d SF=%d S=%s TN=%s\n", adjLen, sf, CS[s], CTN[tn]);
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "formatDecimal: %s  r->digits=%d  r->exponent=%d\n", getBCD(r), r->digits, r->exponent);
    
    PRINTDEC("fd(1:r):", r);
    PRINTALL("pa(1:r):", r, set);
    
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
    
    decNumber _sf;  // scaling factor
    {
        //decNumberTrim(r);   // clean up any trailing 0's
        
        
#ifndef SPEED
        int scale;
        char out[256], out2[256];
        if_sim_debug (DBG_TRACEEXT, & cpu_dev)
        {
            bzero(out, sizeof(out));
            bzero(out2, sizeof(out2));
            
            decBCDFromNumber((uint8_t *)out, r->digits, &scale, r);
            for(int i = 0 ; i < r->digits ; i += 1 )
                out[i] += '0';
            sim_printf("formatDecimal(DEBUG): out[]: '%s'\n", out);
        }
#endif
        
        if (s != CSFL)// && sf != 0)
        {
            decNumberFromInt32(&_sf, sf);
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "formatDecimal(s != CSFL a): %s r->digits=%d r->exponent=%d\n", getBCD(r), r->digits, r->exponent);
            r2 = decNumberRescale(&_r2, r, &_sf, set);
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "formatDecimal(s != CSFL b): %s r2->digits=%d r2->exponent=%d\n", getBCD(r2), r2->digits, r2->exponent);
        }
        else
            //*r2 = *r;
            decNumberCopy(r2, r);
        
        PRINTDEC("fd(2:r2):", r2);

#ifndef SPEED
        if_sim_debug (DBG_TRACEEXT, & cpu_dev)
        {
            decBCDFromNumber((uint8_t *)out2, r2->digits, &scale, r2);
            for(int i = 0 ; i < r2->digits ; i += 1 )
                out2[i] += '0';
            
            sim_debug (DBG_TRACEEXT, & cpu_dev,
                       "formatDecimal: adjLen=%d E=%d SF=%d S=%s TN=%s digits(r2)=%s E2=%d\n", adjLen, r->exponent, sf, CS[s], CTN[tn],out2, r2->exponent);
        }
#endif
    }
    
    
    
    int scale;
    
    static uint8_t out[256];
    
    bzero(out, sizeof(out));
    
    //bool ovr = (r->digits-sf) > adjLen;     // is integer portion too large to fit?
    bool ovr = r2->digits > adjLen;          // is integer portion too large to fit?
    bool trunc = r->digits > r2->digits;     // did we loose something along the way?
    
    
    //sim_printf("GD2:'%s'\n", getBCD(r2));
    
    // now let's check for overflows
    if (!ovr && !trunc)
    {
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "formatDecimal(OK): r->digits(%d) <= adjLen(%d) r2->digits(%d)\n", r->digits, adjLen, r2->digits);
        if (s == CSFL)
        {
            if (r2->digits < adjLen)
            {
                PRINTDEC("Value 1a", r2)
                
                decNumber _s, *sc;
                int rescaleFactor = r2->exponent - (adjLen - r2->digits);
                sc = decNumberFromInt32(&_s, rescaleFactor);
                
                PRINTDEC("Value sc", sc)
                if (rescaleFactor > (adjLen - r2->digits))
                    r2 = decNumberRescale(r2, r2, sc, set);
                
                PRINTDEC("Value 2a", r2)
            } else {
                PRINTDEC("Value 1b", r2)
            }
            
            // if it's floating justify it ...
            /// <remark>
            /// The dps88 and afterwards would generate a quotient with the maximim number of significant digits.
            /// Not so the dps8. According the manuals "if the divisor is greater than the dividend after operand alignment,
            ///    the leading zero digit produced is counted and the effective precision of the result is reduced by one."
            /// No problem. However, according to eis_tester
            ///             desc 1 -sd l -sf 1 -nn 8;
            ///             desc 2 -sd t -sf 2 -nn 8;
            ///             desc 3 -sd f -nn 8;
            ///
            ///             data 1 "+" (5)"0" "58";
            ///             data 2 "000" "1234" "+";
            ///             data 3 "+" "021275" 376;
            ///            +0001234(00) / +0000058(0) = +021275 e-2
            /// by as yet an unknown algorithm
            /// <remark/>
            // ... if bAdj then we leave a (single?) leading 0
            
            if (!decNumberIsZero(r2))
            {
                char *q = getBCDn(r2, adjLen) ;
                int lz = 0; // leading 0's
                while (*q)
                {
                    //sim_printf("fj:'%s'\n", q);

                    if (*q == '0')
                    {
                        lz += 1;
                        q += 1;
                    }
                    else
                        break;
                }
                
                if (lz)
                {
                    decNumber _1;
                    decNumberFromInt32(&_1, lz);
                    decNumberShift(r2, r2, &_1, set);
                    r2->exponent -= lz;
                }
            }
        }
        
        
        decBCDFromNumber(out, adjLen, &scale, r2);
        
        for(int i = 0 ; i < adjLen ; i += 1 )
            out[i] += '0';
        
        // add leading 0 and reduce precision if needed
        if (bDgtN)
        {
            for(int i = adjLen - 1 ; i >= 0 ; i -= 1 )
                out[i + 1] = out[i];
            out[adjLen] = 0;
            out[0] = '0';
            r2->exponent += 1;
        }

        
        //sim_printf("out[ot A]='%s'\n", out);
    }
    else
    {
        PRINTDEC("r2(a):", r2);

        ovr = false;
        trunc = false;
        
        // if we get here then we have either overflow or truncation....
        
        sim_debug (DBG_TRACEEXT, & cpu_dev,
                   "formatDecimal(!OK%s): r2->digits %d adjLen %d\n", R ? " R" : "", r2->digits, adjLen);
        
        // so, what do we do?
        if (R)
        {
            // NB even with rounding you can have an overflow...
            
            // if we're in rounding mode then we just make things fit and everything is OK - except if we have an overflow.
            
            decNumber *ro = r2; //(s == CSFL ? r : r2);
            
            int safe = set->digits;
            
            if (ro->digits > adjLen)    //(adjLen + 1))
            {
                //set->digits = ro->digits + sf + 1;
                sim_debug (DBG_TRACEEXT, & cpu_dev,
                           "formatDecimal(!OK R1): ro->digits %d adjLen %d\n", ro->digits, adjLen);
                
                set->digits = adjLen;
                decNumberPlus(ro, ro, set);
                
                decBCDFromNumber(out, set->digits, &scale, ro);
                for(int i = 0 ; i < set->digits ; i += 1 )
                    out[i] += '0';
                
                // HWR 24 Oct 2013
                char temp[256];
                strcpy(temp, (char *) out+set->digits-adjLen);
                strcpy((char *) out, temp);
                
                //strcpy(out, out+set->digits-adjLen); // this generates a SIGABRT - probably because of overlapping strings.
                
                //sim_debug (DBG_TRACEEXT, & cpu_dev, "R OVR\n");
                //ovr = true; breaks ET MVN 5
            }
            else
            {
                sim_debug (DBG_TRACEEXT, & cpu_dev,
                           "formatDecimal(!OK R2): ro->digits %d adjLen %d\n", ro->digits, adjLen);
                
                if (s==CSFL)
                {
                    
                    set->digits = adjLen;
                    decNumberPlus(ro, ro, set);
                    
                    decBCDFromNumber(out, adjLen, &scale, ro);
                    for(int i = 0 ; i < adjLen ; i += 1 )
                        out[i] += '0';
                    out[adjLen] = 0;
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "formatDecimal(!OK R2a): %s\n", out);
                    
                }
                else
                {
                    int dig = set->digits;
                    set->digits = adjLen;
                    ro = decNumberPlus(ro, ro, set);    // round to adjLen digits
                    decBCDFromNumber((uint8_t *)out, adjLen, &scale, ro);
                    set->digits = dig;
                    
                    for(int j = 0 ; j < adjLen; j += 1 )
                        out[j] += '0';
                    
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "formatDecimal(!OK R2b): %s\n", out);
                }
                ovr = false;    // since we've rounded we can have no overflow ?????
            }
            sim_debug (DBG_TRACEEXT, & cpu_dev, "formatDecimal(R3): digits:'%s'\n", out);
            
            set->digits = safe;
            
            // display int of number
            
#ifndef SPEED
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
#endif
        }
        else
        {
            // if we're not in rounding mode then we can either have a truncation or an overflow
            
            if (s == CSFL)
            {
                enum rounding safeR = decContextGetRounding(set);         // save rounding mode
                decContextSetRounding(set, DEC_ROUND_DOWN);     // Round towards 0 (truncation).

                PRINTDEC("out[1a]:", r2);

                int safe = set->digits;
                set->digits = adjLen;
                decNumberPlus(r2, r2, set);

                PRINTDEC("out[1b]:", r2);

                decBCDFromNumber(out, r2->digits, &scale, r2);
                for(int i = 0 ; i < adjLen ; i += 1 )
                    out[i] += '0';
                out[adjLen] = 0;

                
                // 1) Floating-point quotient
                //  NQ = N3, but if the divisor is greater than the dividend after operand alignment, the leading zero digit produced is counted and the effective precision of the result is reduced by one.
                // -or-
                // With the divisor (den) greater than the dividend (num), the algorithm generates a leading zero in the quotient. 

                if (bDgtN)
                {
                    for(int i = adjLen - 1 ; i >= 0 ; i -= 1 )
                        out[i + 1] = out[i];
                    out[adjLen] = 0;
                    out[0] = '0';
                    r2->exponent += 1;
                }

                set->digits = safe;
                decContextSetRounding(set, safeR);              // restore rounding mode
                
                sim_debug (DBG_TRACEEXT, & cpu_dev, "CSFL TRUNC\n");
                //sim_printf("out[1c %lu]='%s'\n", strlen(out), out);

            }
            else
            {
                if (r2->digits < r->digits)
                {
                    enum rounding safeR = decContextGetRounding(set);         // save rounding mode
                    decContextSetRounding(set, DEC_ROUND_DOWN);     // Round towards 0 (truncation).
                    
                    // re-rescale r with an eye towards truncation notrounding
                    
                    r2 = decNumberRescale(r2, r, &_sf, set);
                    
                    trunc = true;

                    if (r2->digits <= adjLen)
                    {
                        decBCDFromNumber(out, adjLen, &scale, r2);
                        for(int i = 0 ; i < adjLen; i += 1 )
                            out[i] += '0';
                        out[adjLen] = 0;
                        trunc = false;
                    }
                    else
                    {
                        decBCDFromNumber(out, r2->digits, &scale, r2);
                        for(int i = 0 ; i < r2->digits; i += 1 )
                            out[i] += '0';
                        out[r2->digits] = 0;
                        
                        memcpy(out, out + strlen((char *) out) - adjLen, adjLen);
                        out[adjLen] = 0;
                        
                        ovr = true;
                        trunc = false;
                    }
                    decContextSetRounding(set, safeR);              // restore rounding mode
                    //sim_printf("out[ot %u]='%s'\n", strlen(out), out);
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "TRUNC\n");
                    
                    //                } else if ((r2->digits-sf) > adjLen)     // HWR 18 July 2014 was (r->digits > adjLen)
                } else if ((r2->digits) > adjLen)     // HWR 18 July 2014 was (r->digits > adjLen)
                {
                    // OVR
                    decBCDFromNumber(out, r2->digits, &scale, r2);
                    for(int i = 0 ; i < r2->digits ; i += 1 )
                        out[i] += '0';
                    out[r2->digits] = 0;
                    
                    // HWR 24 Oct 2013
                    char temp[256];
                    strcpy(temp, (char *) out+r2->digits-adjLen);
                    strcpy((char *) out, temp);
                    //strcpy(out, out+r->digits-adjLen); // this generates a SIGABRT - probably because of overlapping strings.
                    
                    sim_debug (DBG_TRACEEXT, & cpu_dev, "OVR\n");
                    ovr = true;
                }
                else
                    sim_printf("formatDecimal(?): How'd we get here?\n");
            }
        }
    }
    sim_debug (DBG_TRACEEXT, & cpu_dev,
               "formatDecimal(END): ovrflow=%d trunc=%d R=%d out[]='%s'\n", ovr, trunc, R, out);
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
// CANFAULT
void ad2d (void)
{
    DCDstruct * i = & currentInstruction;
    EISstruct *e = &i->e;
    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
#ifdef EIS_CACHE
    setupOperandDescriptorCache(3, e);
#endif
    
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    decContextDefaultDPS8(&set);
    
    set.traps=0;
    
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif

    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"ad2d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"ad2d overflow fault");
    }
}


/*
 * ad3d - Add Using Three Decimal Operands
 */
// CANFAULT
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    decContextDefaultDPS8(&set);
    set.traps=0;
    
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
    
    int jf = calcSF(e->SF1, e->SF2, e->SF3);    // justification factor
    
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N3, e->S3, e->SF3, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3) && (int) strlen(res) < jf && !e->R)  // may need to move to formatDecimal()
        Trunc = true;
    
    // If S3 indicates a fixed-point format, the results are stored using scale factor SF3, which causes leading or trailing zeros (4 bits - 0000, 9 bits - 000110000) to be supplied and/or most significant digit overflow or least significant digit truncation to occur.
    
    
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
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif
    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"ad3d truncation(overflow) fault");
    }
    
    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"ad3d overflow fault");
    }
}

/*
 * sb2d - Subtract Using Two Decimal Operands
 */
// CANFAULT
void sb2d (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
#ifdef EIS_CACHE
    setupOperandDescriptorCache(3, e);
#endif
    
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    decContextDefaultDPS8(&set);
    set.traps=0;
    
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
    
    decNumber *op3 = decNumberSubtract(&_3, op2, op1, &set);
    
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif

    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"sb2d truncation (overflow) fault");
    }
    
    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"sb2d overflow fault");
    }
}

/*
 * sb3d - Subtract Using Three Decimal Operands
 */
// CANFAULT
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    decContextDefaultDPS8(&set);
    
    set.traps=0;
    
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
    
    decNumber *op3 = decNumberSubtract(&_3, op2, op1, &set);
    
    bool Ovr = false, Trunc = false;
    
    char *res = formatDecimal(&set, op3, e->dstTN, e->N3, e->S3, e->SF3, e->R, &Ovr, &Trunc);
    
    if (decNumberIsZero(op3))
        op3->exponent = 127;
    
    //sim_printf("%s\r\n", res);
    
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
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                else
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                break;
            case CTN9:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif

    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"sb3d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"sb3d overflow fault");
    }
}

/*
 * mp2d - Multiply Using Two Decimal Operands
 */
// CANFAULT
void mp2d (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
#ifdef EIS_CACHE
    setupOperandDescriptorCache(3, e);
#endif
    
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    decContextDefaultDPS8(&set);
    
    set.traps=0;
    
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
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                else
                    //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? 015 :  014);  // default +
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                break;
            case CTN9:
                //write49(e, &dstAddr, &pos, e->dstTN, decNumberIsNegative(op3) ? '-' : '+');
                EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif

    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"mp2d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"mp2d overflow fault");
    }
    
}

/*
 * mp3d - Multiply Using Three Decimal Operands
 */
// CANFAULT
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    //set.traps=0;
    decContextDefaultDPS8(&set);
    
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
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                else
                    EISwrite49(&e->ADDR3, &pos, e->dstTN,  (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                break;
            case CTN9:
                EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
        //sim_printf("exp=%d\n", op3->exponent);
        if (op3->exponent > 127)
            SETF(cu.IR, I_EOFL);
        if (op3->exponent < -128)
            SETF(cu.IR, I_EUFL);
    }
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif

    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"mp3d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"mp3d overflow fault");
    }
}

/*
 * dv2d - Divide Using Two Decimal Operands
 */
// XXX need to put in divide checks, etc ...
// CANFAULT
void dv2d (void)
{
    DCDstruct * ins = & currentInstruction;
    EISstruct *e = &ins->e;

    setupOperandDescriptor(1, e);
    setupOperandDescriptor(2, e);
#ifdef EIS_CACHE
    setupOperandDescriptorCache(3, e);
#endif
    
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    decContextDefaultDPS8(&set);
    set.traps=0;
    
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
    decNumber *op1 = decBCD9ToNumber(e->inBuffer, n1, sc1, &_1);    // divisor
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
    
    decNumber *op2 = decBCD9ToNumber(e->inBuffer, n2, sc2, &_2);    // dividend
    if (e->sign == -1)
        op2->bits = DECNEG;
    if (e->S2 == CSFL)
        op2->exponent = e->exponent;
    
    decNumber *op3 = decNumberDivide(&_3, op2, op1, &set);  // Quotient (Yes, they're reversed)

    PRINTDEC("op2", op2);
    PRINTDEC("op1", op1);
    PRINTDEC("op3", op3);

    // In a floating-point divide operation, the required number of quotient digits is determined as follows. With the divisor greater than the dividend, the algorithm generates a leading zero in the quotient. This characteristic of the algorithm is taken into account along with rounding requirements when determining the required number of digits for the quotient, so that the resulting quotient contains as many significant digits as specified by the quotient operand descriptor.
    
    
    
    bool Ovr = false, Trunc = false;
    
    //char *res = formatDecimal(&set, op3, e->dstTN, e->N2, e->S2, e->SF2, e->R, &Ovr, &Trunc);
    char *res = formatDecimalDIV(&set, op3, e->dstTN, e->N2, e->S2, e->SF2, e->R, op2, op1, &Ovr, &Trunc);
    
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF
    
#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif

    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"dv2d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"dv2d overflow fault");
    }
}

/*
 * dv3d - Divide Using Three Decimal Operands
 */
// XXX need to put in divide checks, etc ...

void dv3d (void)
// CANFAULT
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
    //decContextDefault(&set, DEC_INIT_BASE);         // initialize
    decContextDefaultDPS8(&set);
    set.traps=0;
    //set.digits=65;
    
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
    
    PRINTDEC("op2", op2);
    PRINTDEC("op1", op1);
    PRINTDEC("op3", op3);
    
    bool Ovr = false, Trunc = false;
     
    //int SF = calcSF(e->SF1, e->SF2, e->SF3);
    
    char *res = formatDecimalDIV(&set, op3, e->dstTN, e->N3, e->S3, e->SF3, e->R, op2, op1, &Ovr, &Trunc);
    
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  013);  // special +
                    else
                        EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? 015 :  014);  // default +
                    break;
                case CTN9:
                    EISwrite49(&e->ADDR3, &pos, e->dstTN, (decNumberIsNegative(op3) && !decNumberIsZero(op3)) ? '-' : '+');
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
    
    SCF((decNumberIsNegative(op3) && !decNumberIsZero(op3)), cu.IR, I_NEG);  // set negative indicator if op3 < 0
    SCF(decNumberIsZero(op3), cu.IR, I_ZERO);     // set zero indicator if op3 == 0
    
    SCF(!e->R && Trunc, cu.IR, I_TRUNC); // If the truncation condition exists without rounding, then ON; otherwise OFF

#ifdef EIS_CACHE
    cleanupOperandDescriptor(1, e);
    cleanupOperandDescriptor(2, e);
    cleanupOperandDescriptor(3, e);
#endif
    
    if (Trunc)
    {
        SETF(cu.IR, I_TRUNC);
        if (e -> T && ! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"dv3d truncation(overflow) fault");
    }

    if (Ovr)
    {
        SETF(cu.IR, I_OFLOW);
        if (! TSTF (cu.IR, I_OMASK))
            doFault(FAULT_OFL, 0,"dv3d overflow fault");
    }
}




