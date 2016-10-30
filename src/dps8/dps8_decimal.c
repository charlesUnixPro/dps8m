/*
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

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
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_decimal.h"
#include "dps8_eis.h"
#include "dps8_utils.h"

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
#if 1
/* ------------------------------------------------------------------ */
/* HWR 3/21/16 19:54 derived from ......                              */
/* decContextDefault(...)                                         */
/* */
/* decContextDefaultDPS8 -- initialize a context structure            */
/* */
/* Similar to decContextDefault EXCEPT digits are set to 126 for our  */
/* dps8 simulators mpXd instructions                                  */
/* */
/* ------------------------------------------------------------------ */
decContext * decContextDefaultDPS8_80(decContext *context)
{
    decContextDefault(context, DEC_INIT_BASE);
    context->traps=0;

    context->digits = 63 + 63;   // worse case for multiply

    return context;
}
#else
/* ------------------------------------------------------------------ */
/* HWR 6/28/14 18:54 derived from ......                              */
/*     decContextDefault(...)                                         */
/*                                                                    */
/* decContextDefaultDPS8 -- initialize a context structure            */
/*                                                                    */
/* Similar to decContextDefault EXCEPT digits are set to 80 for our   */
/* dps8 simulator (add additional features as required                */
/*                                                                    */
/* ------------------------------------------------------------------ */
decContext * decContextDefaultDPS8_80(decContext *context)
{
    decContextDefault(context, DEC_INIT_BASE);
    context->traps=0;
    
    context->digits = 80;   //63 * 63;  // worse case for multiply
    
    return context;
}
#endif


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
    for (first=bcd; *first==0 && first <= last;)
        first++;
    
    digits=(Int)(last-first)+1;              // calculate digits ..
    //if ((*first & 0xf0)==0) digits--;     // adjust for leading zero nibble
    if (digits!=0) 
       dn->digits=digits;     // count of actual digits [if 0,
    // leave as 1]
    
    // check the adjusted exponent; note that scale could be unbounded
    dn->exponent=-scale;                 // set the exponent
    if (scale>=0)                        // usual case
    {
        if ((dn->digits-scale-1)<-DECNUMMAXE)        // underflow
        {
            decNumberZero(dn);
            //return NULL;
            // XXX check subfault
            doFault (FAULT_IPR, (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC}, "decBCD9ToNumber underflow");
        }
    }
    else  // -ve scale; +ve exponent
    {
        // need to be careful to avoid wrap, here, also BADINT case
        if ((scale<-DECNUMMAXE)            // overflow even without digits
            || ((dn->digits-scale-1)>DECNUMMAXE))   // overflow
        {
            decNumberZero(dn);
            //return NULL;
            // XXX check subfault
            doFault (FAULT_IPR, (_fault_subtype) {.fault_ipr_subtype=FR_ILL_PROC}, "decBCD9ToNumber overflow");
        }
    }
    if (digits==0)
      return dn;             // result was zero
    
    // copy the digits to the number's units, starting at the lsu
    // [unrolled]
    for (;last >= bcd;)                             // forever
    {
        nib=(unsigned)(*last & 0x0f);
        // got a digit, in nib
        //if (nib>9) {decNumberZero(dn); return NULL;}    // bad digit
        if (nib > 9)
          doFault (FAULT_IPR, (_fault_subtype) {.fault_ipr_subtype=FR_ILL_DIG}, "decBCD9ToNumber ill digit");
        
        if (cut==0)
          *up=(Unit)nib;
        else
          *up=(Unit)(*up+nib*DECPOWERS[cut]);
        digits--;
        if (digits==0)
          break;               // got them all
        cut++;
        if (cut==DECDPUN)
        {
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



static const char *CS[] = {"CSFL", "CSLS", "CSTS", "CSNS"};
static const char *CTN[] = {"CTN9", "CTN4"};






char *formatDecimal(decContext *set, decNumber *r, int tn, int n, int s, int sf, bool R, bool *OVR, bool *TRUNC)
{
#if 1
   /*
     * this is for mp3d ISOLTS error (and perhaps others)
     */
    if (r->digits > 63 || r->digits > n)
    {
        static char out1 [132];
        bzero(out1, sizeof(out1));
        
        static char out2 [132];
        bzero(out2, sizeof(out2));
        
        int scale, adjLen = n;
        
        switch (s)
        {
            case CSFL:              // we have a leading sign and a trailing exponent.
                if (tn == CTN9)
                    adjLen -= 2;    // a sign and an 1 9-bit exponent
                else
                    adjLen -= 3;    // a sign and 2 4-bit digits making up the exponent
                break;              // until we have an example of what to do here, let's just ignore it and hope it goes away
            case CSLS:
            case CSTS:              // take sign into assount. One less char to play with
                adjLen -= 1;
                break;              // until we have an example of what to do here, let's just ignore it and hope it goes away (again)
            case CSNS:              // no sign to worry about. Use everything
                decBCDFromNumber((uint8_t *)out1, r->digits, &scale, r);
                for(int i = 0 ; i < r->digits ; i += 1)
                    out1[i] += '0';
                // now copy the lower n chars to out2
//                for(int i = 0 ; i < n ; i += 1)
//                {
//                    out2[i] = out1[i + r->digits - n];
//                }
                // memcpy
                memcpy(out2, out1 + r->digits - n, (unsigned long) n);
                
                *OVR = true;
                return (char *) out2;
        }
    }
#else
    /*
     * this is for mp3d ISOLTS error (and perhaps others)
     */
    if (r->digits > 63 || r->digits > n)
    {
        static char out1 [132];
        bzero(out1, sizeof(out1));
        
        static char out2 [132];
        bzero(out2, sizeof(out2));
        
        int scale, adjLen = n;
        
        switch (s)
        {
            case CSFL:              // we have a leading sign and a trailing exponent.
                if (tn == CTN9)
                    adjLen -= 2;    // a sign and an 1 9-bit exponent
                else
                    adjLen -= 3;    // a sign and 2 4-bit digits making up the exponent
                break;              // until we have an example of what to do here, let's just ignore it and hope it goes away
            case CSLS:
            case CSTS:              // take sign into assount. One less char to play with
                adjLen -= 1;
                break;              // until we have an example of what to do here, let's just ignore it and hope it goes away (again)
            case CSNS:              // no sign to worry about. Use everything
                decBCDFromNumber((uint8_t *)out1, r->digits, &scale, r);
                for(int i = 0 ; i < r->digits ; i += 1)
                    out1[i] += '0';
                // now copy the lower n chars to out2
//                for(int i = 0 ; i < n ; i += 1)
//                {
//                    out2[i] = out1[i + r->digits - n];
//                    sim_printf("out2[%d]:%s\n", i, out2);
//                }
                // memcpy
                memcpy(out2, out1 + r->digits - n, n);
                
                *OVR = true;
        }
        return (char *) out2;
    }
#endif

    
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

#ifndef QUIET_UNUSED
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
#endif

#if 0
int findFirstDigit(unsigned char *bcd)
{
    int i = 0;
    while (bcd[i] == '0' && bcd[i])
        i += 1;
    
    return i;
}
#endif







