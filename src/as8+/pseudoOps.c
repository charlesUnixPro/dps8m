/*
 Copyright 2012-2015 by Harry Reed

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later. 
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file pseudoOps.c
 * \project as8
 * \author Harry Reed
 * \date 11/6/12
 *  Created by Harry Reed on 11/6/12.
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "as.h"

#include "y.tab.h"

void setState(int state);

extern int yylineno;
extern char* yytext;

//char *arg1, *arg2, *arg3, *arg4, *arg5, *arg6, *arg7, *arg8, *arg9, *arg10;

// XXX remove these when ready 
//word36 Eval(char *s) { return 0; }
//word36 boolEval(char *s) { return 0; }

void doOptions(tuple *tt)
{
    tuple *t;
    DL_FOREACH(tt, t)
    {
        if (!strcasecmp(t->a.p, "multics"))
            callingConvention = 2;
        else if (!strcasecmp(t->a.p, "honeywell"))
            callingConvention = 1;
        else
            yyprintf("unknown option <%s>", t->a.p);
    }
}

void doBss(char *symbol, word36 size)
{
    if (size < 1)
    {
        yyprintf("doBss(): size=%d\n", size);
        return;
    }
    // if 1 arg just reserve number of words
    // if 2 args set name to value of IC and reserve words
    if (symbol && size) {
        symtab *s = getsym(symbol);
        if (nPass == 1) {
            if (s == NULL)
            {
                s = addsym(symbol, addr);
                if (debug) printf("adding symbol (BSS) %s = %6o (%06o)\n", symbol, addr, (word18)s->value);
            } else
                yyprintf("found duplicate symbol <%s>\n", symbol);
        } else // pass 2
        {
            if (s == NULL)
            {
                yyprintf("undeclared sysmbol <%s> in pass2\n", symbol);
            } else {
                if (addr != s->value)
                    yyprintf("phase error for symbol <%s> 1:%06o 2:%06o", symbol, s->value, addr);
                
            }
            //fprintf(out, "%6.6o xxxx 000000000000 %s\n", *addr, line);   //op);
            outas8data(0LL, addr,LEXline);
        }
        addr += size;
    }
    else if (!symbol && size) {
        if (nPass == 2)
            outas8data(0LL, addr,LEXline);
        addr += size;
    }
}

void doTally(pseudoOp *p, list *args)
{
    if (nPass == 1)
    {
        addr += 1;
        return;
    }
    
    // count them args!
    int nArgs = 0;
    word36 Arg[3] = {0,0,0};
    list *l = args;
    while (l)
    {
        Arg[nArgs] = l->i36;   // get parameter value
        l = l->next;
        nArgs += 1;
    }
    
    word36 tally = 0;
    if (!strcasecmp(p->name, "tally"))
    {
        int byte_offset = Arg[2] & 07; ///< 0 - 5 allowed
        int count       = Arg[1] & 07777;
        int taddr       = Arg[0] & AMASK;
        
        tally = (taddr << 18) | (count << 6) | byte_offset;
    } else if (!strcasecmp(p->name, "tallyb"))
    {
        int byte_offset = Arg[2] & 3;    ///< 0 - 3 allowed
        int count       = Arg[1] & 07777;
        int taddr       = Arg[0] & AMASK;
        
        tally = (taddr << 18) | (count << 6) | (1 << 5) | byte_offset;
    } else if (!strcasecmp(p->name, "tallyc"))
    {
        int mod         = Arg[2] & 077;
        int count       = Arg[1] & 07777;
        int taddr       = Arg[0] & AMASK;
        
        tally = (taddr << 18) | (count << 6) | mod;
    } else if (!strcasecmp(p->name, "tallyd"))
    {
        int delta       = Arg[2] & 077;
        int count       = Arg[1] & 07777;
        int taddr       = Arg[0] & AMASK;
        
        tally = (taddr << 18) | (count << 6) | delta;
    }
    
    outas8data(tally, addr, LEXline);
    
    addr += 1;
}

extern int getmod(const char *arg_in) ;

/// The DEC pseudo-operation is used to generate object word(s) containing binary numbers from decimal representations.
void doDec(list *lst)
{
    /**
     * Need to support these additional specifications ....
     *
     *
     (1) Fixed point. The presence of a scale factor with or without a decimal point and/or exponent indicates that a fixed- point number is to be generated.
     (2) Floating point. The presence of a decimal point and/or exponent without a scale factor indicates that a floating-point number is to be generated. The letters X or XD indicate that a single-precision or double-precision hexadecimal floating-point number is to be generated.
     (3) Integer. If no scale factor, decimal point, and/or exponent is present, an integer is generated.
         Exponent. An exponent is represented by the letters D or E, followed by an optional sign and one or two digits. The letter D indicates that two words are to be generated. A plus sign or no sign indicates multiplication by a power of 10.
         Scale factor. The scale factor is represented by the letter B, followed by an optional sign and one or two digits. A plus sign or no sign indicates that the digit(s) represents the bit position of the first bit in the fractional part; a minus sign indicates that the fractional part initiates the specified number of bits to the left of the word.
     */
    
    int addrIncr = 0;   // default address increment is 1-word
    word36 ypair[2] = {0, 0};
    int term = 0;
    list *l;
    DL_FOREACH(lst, l)
    {
        switch (l->whatAmI)
        {
            case lstI36:
            case lstSingle:
                addrIncr = 1;
                break;
            case lstDouble:
            case lstI72:
                addrIncr = 2;
                break;
            default:
                yyprintf("doDec(): unhandled 'whatAmI' (%d)", l->whatAmI);
                continue;
        }
        if (nPass == 2)
        {
            switch (l->whatAmI)
            {
                case lstI36:
                    outas8data(l->i36 & DMASK, addr, term == 0 ? LEXline : NULL);
                    break;
                case lstI72:
                    ypair[0] = (l->i72 >> 36) & DMASK;
                    ypair[1] = l->i72 & DMASK;
                    
                    outas8data(ypair[0] & DMASK, addr,   term == 0 ? LEXline : NULL);
                    outas8data(ypair[1] & DMASK, addr+1, NULL);
                    break;

                case lstSingle:
                    IEEElongdoubleToYPair(l->r, ypair);
                    outas8data(ypair[0] & DMASK, addr, term == 0 ? LEXline : NULL);
                    break;
                case lstDouble:
                    IEEElongdoubleToYPair(l->r, ypair);
                    outas8data(ypair[0] & DMASK, addr,   term == 0 ? LEXline : NULL);
                    outas8data(ypair[1] & DMASK, addr+1, NULL);
                    break;
                default:
                    yyprintf("doDec(): unhandled 'whatAmI' (%d)", l->whatAmI);
                    continue;
            }
        }
        addr += addrIncr;
        term++;
    }
}

void doOct(list *l)
{
    list *el;
    
    int term = 0;
    DL_FOREACH(l, el)
    {
        /**
         * The word is stored in its real form and is not complemented if a minus sign is present. The sign applies to bit 0 only.
         */
        word36s v64 = el->i36;
        bool sign = v64 < 0; // XXX methinks that this is quite wrong .....
        if (sign)
        {
            v64 = labs(v64);
            v64 |= SIGN36;
        }
        
        if (nPass == 2)
        {
            if (term == 0)
                outas8data(v64 & DMASK, addr, LEXline);
            else
                outas8data(v64 & DMASK, addr, "\n");
        }
        
        addr += 1;
        term++;
    }
}

void doArg(struct opnd *o)
{
    if (nPass == 1)
    {
        addr += 1;      // just bump address counter
        return;
    }
    if (nPass != 2)     // only pass 1 | 2 allowed
        return;
    
    outRec *p = newoutRec();
    p->address = addr;
    
    p->data = 0;                // no opcode
    p->data |= o->lo;           // or-in modifier
    p->data |= ((word36)o->hi << 18);
    if (o->bit29)
        p->data |= (1LL << 6);    // set bit-29
    
    p->src = strdup(LEXline);
    
    DL_APPEND(as8output, p);
    
    addr += 1;
}


void doZero(word36 hi, word36 lo)
{
    if (nPass == 1)
    {
        addr += 1;
        return;
    }
    
    hi &= AMASK;  // keep to 18-bits
    lo &= AMASK;  // keep to 18-bits
    
    if (debug) printf("  ZERO %6.6o %6.6o\n", (word18)hi, (word18)lo);
    
    char w[32];
    sprintf(w, "%6.6o%6.6o", (word18)hi, (word18)lo);
    outas8Strd(w, addr, LEXline);
    
    addr += 1;
}

#if DELETE_WHEN_READY
int _doVfdOrig(tuple *list, word36 *data)
{
    // low-order Li bits of data are used, padded if needed on the left.
    int nBits = 0;
    
    tuple *l;
    DL_FOREACH(list, l)
        nBits += l->b.i;
    
    int nWords = nBits / 36;
    nWords += nBits % 36 ? 1 : 0;
    
    if (nPass == 2)
    {
        //word36 data[nWords];            // where the vfd bits live
        //memset(data, 0, sizeof(data));  // make everything 0
        
        //word36 *d = data;   // pointer to destination word
        //int bpos = 0;       // dps8 bit position to write
        
        int vfdWord = 0;    ///< current vfd word
        
        word36 vfd = 0;     ///< final value of vfd
        int bitPos = 36;    ///< start at far left of word ...
        
        int w;              ///< width of field to encode
        word36 v;           ///< value to encode

        int cnt;            ///< current bit position of insertion

        // now encode bits ...
        DL_FOREACH(list, l)
        {
            if (l->a.c == 'o')
            {
                w = l->b.i;
                v = l->c.i36;
            }
            else if (l->a.c == 'a')
            {
                cnt = (int)strlen(l->c.p);
                w = l->b.i;
                v = 0;
                char *p = l->c.p;
                
                for(int n = 0 ; (n < cnt) && (*p) ; n ++)
                    v = (v << 9) | *p++;
            }
            else if (l->a.c == 'h') // 6-bit gebcd packed into 9-bits
            {
                cnt = (int)strlen(l->c.p);
                w = l->b.i;
                v = 0;
                char *p = l->c.p;
                
                for(int n = 0 ; (n < cnt) && (*p) ; n ++)
                {
                    int c6 = ASCIIToGEBcd[*p++];
                    if (c6 == -1)   // invalid GEBCD?
                        c6 = 017;   // a GEBCD ? - probably ought to change
                    v = (v << 9) | (c6 & 077);
                }
            }
            else
            {
                w = l->b.i;
                v = l->c.i36;
            }
            
            if (w > 36)
                yyprintf("_doVfd(): term width exceeds 36-bits (%d)", w);
            
            // at this point w contains up to 36-bits of data. Insert it into the data array at bit bitPos...
            if (bitPos - w < 0)
            {
                /*
                 * v is too big to fit in what's left of the current data word. Place what can fit into the current
                 * data[] word and continue with the next...
                 */
                
                // determine how much (if any) we can fit into the rest of the current word
                int x = bitPos - w;
                int howMuch = w + x;    // this is how many bits we can fit into the current word.
                
                bitPos -= howMuch;
                word36 mask = bitMask36(howMuch);
                word36 dataToInsert = (v >> (x + howMuch)) & mask;
                vfd = bitfieldInsert36(vfd, dataToInsert, bitPos, howMuch);
                
                // store into current word and go to next
                data[vfdWord++] = vfd;
                
                vfd = 0;
                bitPos = 36;
                
                // how much has slopped over to the next word?
                int slop = w - howMuch;
                if (slop)
                {
                    bitPos -= slop;
                 
                    mask = bitMask36(slop);
                    vfd = bitfieldInsert36(vfd, v & mask, bitPos, slop);
                }                
            } else {
                bitPos -= w;
                word36 mask = bitMask36(w);
                vfd = bitfieldInsert36(vfd, v & mask, bitPos, w);
            }
        }
        
        // last word
        data[vfdWord++] = vfd;        
    }

    return nWords;
}
#endif

int _doVfd(tuple *list, word36 *data)
{
    // low-order Li bits of data are used, padded if needed on the left.
    int nBits = 0;
    
    tuple *l;
    DL_FOREACH(list, l)
    nBits += l->b.i;
    
    int nWords = nBits / 36;
    nWords += nBits % 36 ? 1 : 0;
    
    if (nPass == 2)
    {
        //word36 data[nWords];            // where the vfd bits live
        //memset(data, 0, sizeof(data));  // make everything 0
        
        //word36 *d = data;   // pointer to destination word
        //int bpos = 0;       // dps8 bit position to write
        
        int vfdWord = 0;    ///< current vfd word
        
        word36 vfd = 0;     ///< final value of vfd
        int bitPos = 36;    ///< start at far left of word ...
        
        int w;              ///< width of field to encode
        word72s v;          ///< value to encode (72-bits for large negative #'s
        
        int cnt;            ///< current bit position of insertion
        
        // now encode bits ...
        DL_FOREACH(list, l)
        {
            if (l->a.c == 'o')      // an 'o' field
            {
                w = l->b.i;
                v = (word72s)l->c.i36;
            }
            else if (l->a.c == 'a') // an 'a' field
            {
                cnt = (int)strlen(l->c.p);
                w = l->b.i;
                v = 0;
                char *p = l->c.p;
                
                for(int n = 0 ; (n < cnt) && (*p) ; n ++)
                    v = (v << 9) | *p++;
            }
            else if (l->a.c == 'h') // 6-bit gebcd packed into 9-bits
            {
                cnt = (int)strlen(l->c.p);
                w = l->b.i;
                v = 0;
                char *p = l->c.p;
                
                for(int n = 0 ; (n < cnt) && (*p) ; n ++)
                {
                    int c6 = ASCIIToGEBcd[(int)(*p++)];
                    if (c6 == -1)   // invalid GEBCD?
                        c6 = 017;   // a GEBCD ? - probably ought to change
                    v = (v << 9) | (c6 & 077);
                }
            }
            else
            {
                w = l->b.i;
                v = (word72s)l->c.i36;
            }
            
            if (w > 72)
                yyprintf("_doVfd(): term width exceeds 72-bits (%d)", w);
            
            // at this point w contains up to 36-bits of data. Insert it into the data array at bit bitPos...
            if (bitPos - w < 0)
            {
                /*
                 * v is too big to fit in what's left of the current data word. Place what can fit into the current
                 * data[] word and continue with the next...
                 */
                
                // determine how much (if any) we can fit into the rest of the current word
                int x = bitPos - w;
                int howMuch = w + x;    // this is how many bits we can fit into the current word.
                
                bitPos -= howMuch;
                word36 mask = bitMask36(howMuch);
                word36 dataToInsert = (v >> (x + howMuch)) & mask;
                vfd = bitfieldInsert36(vfd, dataToInsert, bitPos, howMuch);
                
                // store into current word and go to next
                data[vfdWord++] = vfd;
                
                vfd = 0;
                bitPos = 36;
                
                // how much has slopped over to the next word?
                int slop = w - howMuch;
                if (slop)
                {
                    bitPos -= slop;
                    
                    mask = bitMask36(slop);
                    vfd = bitfieldInsert36(vfd, v & mask, bitPos, slop);
                }
            } else {
                bitPos -= w;
                word36 mask = bitMask36(w);
                vfd = bitfieldInsert36(vfd, v & mask, bitPos, w);
            }
        }
        
        // last word
        data[vfdWord++] = vfd;
    }
    
    return nWords;
}



void doVfd(tuple *list)
{
    word36 data[256];
    memset(data, 0, sizeof(data));  // make everything 0

    int nWords = _doVfd(list, data);
    if (nWords > 256)
    {
        yyerror ("VFD data too large. Max 256-words");
        return;
    }
    if (nPass == 2)
    {
        for(int n = 0 ; n < nWords ; n += 1)
            if (!n)
                outas8data(data[n], addr + n, LEXline);
            else
                outas8data(data[n], addr + n, "\n");
    }
    addr += nWords;
}

void
doPop0(pseudoOp *p)
{
    char temp[256];
    
    if (strcmp(p->name, "even") == 0)
    {
        if (addr % 2) {
            if (debug) printf("  EVEN align\n");
            if (nPass == 2)
            {
                sprintf(temp, "%s \"(allocating 1 nop)\n", LEXline);
                outas8ins(000000011000LL, addr, temp);
            }
            addr += 1;
        }
    } else if (strcmp(p->name, "odd") == 0)
    {
        if (!(addr % 2)) {
            if (debug) printf("  ODD align\n");
            if (nPass == 2)
            {
                sprintf(temp, "%s \"(allocating 1 nop)\n", LEXline);
                outas8ins(000000011000LL, addr, temp);
            }
            addr += 1;
        }
    } else if (strcmp(p->name, "eight") == 0)
    {
        if (addr % 8) {
            if (debug) printf("  EIGHT align\n");
            int inc = addr % 8;
            
            // pad with NOP's 000000011000
            if (nPass == 2)
            {
                sprintf(temp, "%s \"(allocating %d nop's)\n", LEXline, 8-inc);
                outas8ins(000000011000LL, addr++, temp);
                for(int n = 0 ; n < 8 - inc - 1; n++)
                    outas8ins(000000011000LL, addr++, "");
            } else
                addr += 8 - inc;
        }
    } else if (strcmp(p->name, "sixteen") == 0)
    {
        if (addr % 16) {
            if (debug) printf("  SIXTEEN align\n");
            int inc = addr % 16;
            
            // pad with NOP's 000000011000
            if (nPass == 2)
            {
                sprintf(temp, "%s \"(allocating %d nop's)\n", LEXline, 16-inc);
                outas8ins(000000011000LL, addr++, temp);
                for(int n = 0 ; n < 16 - inc - 1; n++)
                    outas8ins(000000011000LL, addr++, "");
            } else
                addr += 16 - inc;
        }
    } else if (strcmp(p->name, "sixtyfour") == 0)
    {
        if (addr % 64) {
            if (debug) printf("  64 align\n");
            int inc = addr % 64;
            
            // pad with NOP's 000000011000
            if (nPass == 2)
            {
                sprintf(temp, "%s \"(allocating %d nop's)\n", LEXline, 64-inc);
                outas8ins(000000011000LL, addr++, temp);
                for(int n = 0 ; n < 64 - inc - 1; n++)
                    outas8ins(000000011000LL, addr++, "");
            } else
                addr += 64 - inc;
        }
    } else if (strcmp(p->name, "end") == 0)
    {
        //LEXgetPop();
        LEXfseekEOF();
    } else if (strcmp(p->name, "getlp") == 0)
    {
        // Sets the pointer register pr4 to point to the linkage section. This can be used with segdef to simulate the effect of entry, This operator can use pointer register pr2, index registers 0 and 7, and the A and Q registers, and requires pr6 and pr7 to be set up properly.
        // (This is actually a call to the entry operator.)

        if (nPass == 2)
        {
            //outas8(0700046272120LL, addr, LEXline);
            sprintf(temp, "%06o%06o", 0700046,  getEncoding("tsp2") | (word18)BIT(29) | getmod("*"));
            outas8Stri(temp, addr, LEXline);
        }
        addr += 1;

    } else
        yyprintf("unhandled pseudoop <%s>", p->name);

}

void doOrg(word36 loc)
{
    word18 org = -1;
    if (loc == -1)
        {
            yyerror("undefined expression in org");
            return;
        }
    org = loc & AMASK;
    
    addr = org;
    if (debug) printf("  ORG at %o\n", addr);
}

#ifdef DEPRECIATED
void doAcc(char *str, int sz)
{
    char cstyle[1024];
    strexp(cstyle, str);
    
    int strLength = (int)strlen(cstyle);
    
    if (strLength)
    {
        /// convert 8-bit chars to 9-bit bytes in 36-bit words. Oh, Joy!
        int nWords = 0;
        nWords = (strLength+1) / 4 + ((strLength+1) % 4 ? 1 : 0); // +1 because of initial length char
        if (sz > 0)
        {
            if (sz > strLength)
                nWords = (sz) / 4 + ((sz) % 4 ? 1 : 0);
            else if (sz < strLength)
            {
                yyprintf("acc: specified string length %d less than actual %d", sz, strLength);
                return;
            }
        }

        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 2;                   ///< start at 2nd byte in word;
        *w = ((word36)strLength << 27); // store string length in w0-8
        
        //for(char *p = arg0 + 1; p < next; p++)
        for(char *p = cstyle; *p; p++)
        {
            word36 q = *p;
            word36 nShift = (9 * (nPos--));
            
            *w |= (q << nShift);
            
            if (nPos < 0) {
                w++;        // next word
                //*w = 0;
                nPos = 3;   // 1st byte in word
            }
        }
        while (nPos >= 0)
        {
            word36 nShift = (9 * (nPos--));
            
            *w |= (' ' << nShift);
        }

        for(int i = 0 ; i < nWords; i++)
        {
            if (nPass == 2)
                //fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, words[i], i == 0 ? inLine : "");
                outas8data(words[i], addr, i == 0 ? LEXline : "");
        
            addr++;
        }
    }
}
#endif
/*
 * very similar to doAcc() except a pascal style string for str is used
 */
void doAccP(char *str, int sz)
{
    char pstyle[1024];
    strexpP(pstyle, str);
    
    //int strLength = (int)strlen(cstyle);
    int strLength = pstyle[0] & 0xff;
    
    if (strLength)
    {
        /// convert 8-bit chars to 9-bit bytes in 36-bit words. Oh, Joy!
        int nWords = (strLength+1) / 4 + ((strLength+1) % 4 ? 1 : 0); // +1 because of initial length char
        if (sz > 0)
        {
            if (sz > strLength)
                nWords = (sz) / 4 + ((sz) % 4 ? 1 : 0);
            else if (sz < strLength)
            {
                yyprintf("acc: specified string length %d less than actual %d", sz, strLength);
                return;
            }
        }
        
        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 2;                   ///< start at 2nd byte in word;
        *w = ((word36)strLength << 27); ///< store string length in w0-8
        
        char *p = pstyle + 1;
        for(int i = 0 ; i < strLength ; i++)
        {
            word36 q = *p++;
            word36 nShift = (9 * (nPos--));
            
            *w |= ((q & 0xff) << nShift);
            
            if (nPos < 0) {
                w++;        // next word
                //*w = 0;
                nPos = 3;   // 1st byte in word
            }
        }
        while (nPos >= 0)
        {
            word36 nShift = (9 * (nPos--));
            
            *w |= (' ' << nShift);
        }
        
        for(int i = 0 ; i < nWords; i++)
        {
            if (nPass == 2)
                //fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, words[i], i == 0 ? inLine : "");
                outas8data(words[i], addr, i == 0 ? LEXline : "");
            
            addr++;
        }
    }
}


/**
 * very similiar to ALM aci except only works on a single line (may need to change that later)
 * and C-style escapes are supported
 */
#ifdef DEPRECIATED
void doAci(char *str, int sz)
{
    char cstyle[256];
    strexp(cstyle, str);
        
    int strLength = (int)strlen(cstyle);
    
    if (strLength)
    {
        /// convert 8-bit chars to 9-bit bytes in 36-bit words. Oh, Joy!
        int nWords = (strLength) / 4 + ((strLength) % 4 ? 1 : 0);
        
        if (sz > 0)   // a size specifier
        {
            int nChars = sz;
            if (nChars > strLength)
                nWords = (nChars) / 4 + ((nChars) % 4 ? 1 : 0);
            else if (nChars < strLength)
            {
                yyprintf("aci: specified string length less than actual");
                return;
            }
        }
        
        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 3;
        for(char *p = cstyle; *p; p++)
        {
            word36 q = *p & 0xff;   ///< keep to 8-bits because of sign-extension
            word36 nShift = (9 * (nPos--));
            
            *w |= (q << nShift);
            
            if (nPos < 0) {
                w++;        // next word
                nPos = 3;   // 1st byte in word
            }
        }
        
        for(int i = 0 ; i < nWords; i++)
        {
            if (nPass == 2)
                outas8data(words[i], addr, i == 0 ? LEXline : "\n");
            addr++;
        }
    }
    else
    {
        if (nPass == 2)
            outas8data(0LL, addr, LEXline);
        
        addr++;
    }
}
#endif

/*
 * very similar to doAci() except a pascal style string is passed for str
 */
void doAciP(char *str, int sz)
{
    char pstyle[256];
    strexpP(pstyle, str);
    
    int strLength = pstyle[0] & 0xff;
    
    if (strLength)
    {
        /// convert 8-bit chars to 9-bit bytes in 36-bit words. Oh, Joy!
        int nWords = (strLength) / 4 + ((strLength) % 4 ? 1 : 0);
        
        if (sz > 0)   // a size specifier
        {
            int nChars = sz;
            if (nChars > strLength)
                nWords = (nChars) / 4 + ((nChars) % 4 ? 1 : 0);
            else if (nChars < strLength)
            {
                yyprintf("aci: specified string length less than actual");
                return;
            }
        }
        
        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 3;
        
        //for(char *p = cstyle; *p; p++)
        char *p = pstyle + 1;
        for(int i = 0 ; i < strLength ; i++)
        {
            word36 q = *p++ & 0xff;   ///< keep to 8-bits because of sign-extension
            word36 nShift = (9 * (nPos--));
            
            *w |= (q << nShift);
            
            if (nPos < 0) {
                w++;        // next word
                nPos = 3;   // 1st byte in word
            }
        }
        
        for(int i = 0 ; i < nWords; i++)
        {
            if (nPass == 2)
                outas8data(words[i], addr, i == 0 ? LEXline : "\n");
            addr++;
        }
    }
    else
    {
        if (nPass == 2)
            outas8data(0LL, addr, LEXline);
        
        addr++;
    }
}


void doBci(char *str, int sz)
{
    int strLength = (int)strlen(str);
    if (strLength)
    {
        /// convert 8-bit chars to 6-bit GEBCD's packed into 36-bit words. Oh, Joy!
        int nWords = (strLength) / 6 + ((strLength) % 6 ? 1 : 0);
        
        if (sz > 0)   // a size specifier
        {
            int nChars = sz;
            if (nChars > strLength)
                nWords = (nChars) / 6 + ((nChars) % 6 ? 1 : 0);
            else if (nChars < strLength)
            {
                yyprintf("bci specified string length less than actual");
                return;
            }
        }
        
        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 5;
        for(char *p = str; *p; p++)
        {
            int q = ASCIIToGEBcd[(int)(*p)];
            if (q == -1)
            {
                fprintf(stderr, "WARNING(bci): ASCII character '%c' (%o) not supported in GEBCD. Ignoring.\n", q, q);
                q = ASCIIToGEBcd[' '];  // set to space
            }
            word36 nShift = (6 * (nPos--));
            
            *w |= ((word36)q << nShift);
            
            if (nPos < 0) {
                w++;        // next word
                nPos = 5;   // 1st char in word
            }
        }
        
        // pad rest with spaces ...
        while (nPos >= 0)
        {
            word36 nShift = (6 * (nPos--));
            
            *w |= (ASCIIToGEBcd[' '] << nShift);
        }
        
        for(int i = 0 ; i < nWords; i++)
        {
            if (nPass == 2)
                outas8data(words[i], addr, i == 0 ? LEXline : "\n");
            addr++;
        }
    }
    else
    {
        outas8data(0LL, addr, LEXline);
        addr++;
    }
}

// fix up 8 4-bit decimal digits in 32-bits to 4-bit digits in 36-bits.
//  bbbb bbbb  bbbb bbbb  bbbb bbbb  bbbb bbbb becomes ....
// 0bbbb bbbb 0bbbb bbbb 0bbbb bbbb 0bbbb bbbb
word36 fixupDecimals(word36 src)
{
    int digits[8];
    
    for(int n = 0 ; n < 8 ; n++)
        digits[n] = (int)bitfieldExtract36(src, (n * 4), 4);
    
    word36 fixed = 0;
    fixed = bitfieldInsert36(fixed, digits[0], 0, 4);
    fixed = bitfieldInsert36(fixed, digits[1], 4, 4);
    
    fixed = bitfieldInsert36(fixed, digits[2], 9, 4);
    fixed = bitfieldInsert36(fixed, digits[3], 13, 4);
    
    fixed = bitfieldInsert36(fixed, digits[4], 18, 4);
    fixed = bitfieldInsert36(fixed, digits[5], 22, 4);
    
    fixed = bitfieldInsert36(fixed, digits[6], 27, 4);
    fixed = bitfieldInsert36(fixed, digits[7], 31, 4);
    
    return fixed;
}

void doAc4(char *str, int sz)
{
    /// copy string to a buffer so we can play with it
    char copy[256];
    strcpy(copy, str);
    
    int strLength = (int)strlen(copy);
    
    if (strLength)
    {
        /// convert 8-bit chars to 4-bit decimal digits in 36-bit words. Oh, Joy!
        int nWords = (strLength) / 8 + ((strLength) % 8 ? 1 : 0);
        
        if (sz > 0)   // a size specifier
        {
            int nChars = sz;

            if (nChars > strLength)
                nWords = (nChars) / 8 + ((nChars) % 8 ? 1 : 0);
            else if (nChars < strLength)
            {
                    // XXX: this is not the way alm does it. Fix.
                    yyprintf("ac4: specified string length less than actual");
                    return;
            }        
        }
        
        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 7;
        //for(char *p = arg0 + 1; p < last; p++)
        for(char *p = copy; *p; p++)
        {
            word36 q = *p & 0xf;   ///< keep to 4-bits
            word36 nShift = (4 * (nPos--));
            // NB: need todo a fixup of words because of the funky nature of 4-bit digits in the DPS8
            
            *w |= (q << nShift);
            
            // XXX: TO male it like EDEC P
            //*w <<= 4;
            //*w |= q;
            
            if (nPos < 0) {
                w++;        // next word
                nPos = 7;   // 1st 4-bit digit in word
            }
        }
        
        for(int i = 0 ; i < nWords; i++)
        {
            if (nPass == 2)
                outas8data(fixupDecimals(words[i]), addr, i == 0 ? LEXline : "\n");
            addr++;
        }
    }
    else
    {
        outas8data(0LL, addr, LEXline);
        addr++;
    }
}

void doStrop(pseudoOp *p, char *str, expr *val)
{
    int sz = val ? (int)val->value : 0;
    if (strcasecmp(p->name, "acc") == 0)
        //return doAcc(str, sz);
        return doAccP(str, sz);
    if (strcasecmp(p->name, "aci") == 0)
        //return doAci(str, sz);
        return doAciP(str, sz);
    if (strcasecmp(p->name, "bci") == 0)
        return doBci(str, sz);
    if (strcasecmp(p->name, "ac4") == 0)
        return doAc4(str, sz);
}


void doBoolEqu(char *sym, expr *val)
{
    symtab *s = getsym(sym);
    
    if (nPass == 1)
    {
        if (s == NULL)
        {
            s = addsymx(sym, val);
            if (debug) printf("Adding symbol %s = %6llo\n", sym, s->value);
        } else
            yyprintf("Found duplicate symbol <%s>", sym);
    } else { // pass 2
        if (s == NULL)
            yyprintf("Undeclared symbol <%s> in pass 2!!", sym);
        else
            if (val->value != s->value)
                yyprintf("Phase error for symbol <%s> 1:%06o 2:%06o", sym, val, s->value);
    }
}

void doITSITP(pseudoOp *p, word36 arg1, word36 off, word36 tag)
{
    /// segno, offset, tag
    if (nPass == 2)
    {
        word36 even = 0, odd = 0;
        
        if (!strcasecmp(p->name, "its"))
        {
            int segno = arg1 & 077777;
            even = ((word36)segno << 18) | 043;
        }
        else if (!strcasecmp(p->name, "itp"))
        {
            int prnum = arg1 & 03;
            even = ((word36)prnum << 33) | 041;
        }
        
        int offset = (off & AMASK);
        
        odd = (word36)(offset << 18) | (tag & 077);
        
        char temp[32];
        if ((addr) % 2)
        {
            // odd
            fprintf(stderr, "WARNING: ITS/ITP word-pair must begin on an even boundary. Padding with 1 nop\n");

            sprintf(temp, "%s \"(allocating 1 nop)\n", LEXline);
            outas8ins(000000011000LL, addr, temp);
            
            addr += 1;
        }
        
        // even
        //fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, even, inLine);
        outas8data(even, addr, LEXline);
        addr += 1;
            
        //fprintf(oct, "%6.6o xxxx %012llo\n", (*addr)++, odd);
        outas8data(odd, addr, "");
        addr += 1;

    }
    else
        addr += 2;

}


/**
 * inserts padding (nop) to an <expression> word boundary
 */
void doMod(word36 mod)
{
    int cnt = (int)mod - (int)(addr % mod);
    char *p;
    asprintf(&p, "%s \"(allocating %d nop's)\n", LEXline, cnt);

    for(int n = 0 ; addr % (mod ? mod : 1) ; n++)
    {
        if (nPass == 2)
        {
            //fprintf(oct, "%6.6o xxxx 000000011000 %s\n", (*addr)++, inLine);
            outas8ins(000000011000LL, addr, !n ? p : "");
        }
        addr += 1;
    }
}

/**
 * Pseudo-ops to implement the standard calling convention (in absolute mode.)
 *
 * NB: These are so 60's-ish. NO stack, NO recursion ... no fun. :-) AND usage is *poorly* documented. Really not certain
 *     how error returns work. I think I can intuit it from some examples I've seen, but no docs on how to write
 *     standard subroutines.
 *
 * John Couleur may have been a brilliant enginer, but he was no computer architect. (20/20 hindsight)
 *
 */

/// The CALL pseudo-operation is used to generate the (Honeywell) standard subroutine calling sequence. (Absolute mode only)
void doHCall(char *sub, word36 mod, list *args, list *errs)  // for call pseudoop
{
    /// 1. The first subfield in the variable field of the instruction is separated from the next n subfields by a left parenthesis. This subfield contains the symbol which identifies the subroutine being called. It is possible to modify this symbol by separating the symbol and the modifier with a comma.
    /// 2. The next n subfields are separated from the first subfield by a left parenthesis and from subfield n+1 by a right parenthesis. Therefore, the next n subfields are contained in parentheses and are separated from each other by commas. The contents of these subfields are arguments used in the subroutine being called.
    /// 3. The next m subfields are separated from the previous subfields by a right parenthesis and from each other by commas. These subfields are used to define locations for error returns from the subroutine. If no error returns are needed, m = 0.
    /// 4. The last subfield is used to contain an identifier for the instruction. This identifier is used when a trace of the path of the program is made. The identifier may be an expression contained in apostrophes. Thus the last subfield is separated from the previous subfields by an apostrophe. If the last subfield is omitted, the assembly program will provide an identifier which is the assigned alter number of the CALL pseudo-operation itself.
    /// 5. If the variable field of the CALL instruction cannot be contained on a single line, it may be continued onto succeeding lines by use of the ETC pseudo-operation. This is done by terminating the variable field of the CALL instruction with a comma. The next subfield is then placed as the first subfield of the ETC pseudo-operation. Subsequent subfields may be continued onto following lines in the same manner.
    /// 6. When a CALL to an external subprogram appears within a headed section, the external subprogram must be identified by a 6-character symbol (immune to HEAD).
    /// 7. If a CALL is being used to access an internally defined subroutine, the subroutine must be placed ahead of the CALL in the program stream. Also, a SYMDEF pseudo-operation with the symbol identifying the subroutine in its variable field must be placed ahead of the CALL in the program stream. If a SAVE pseudo-operation is placed at the beginning of the subroutine, a SYMDEF is automatically provided. However, one internal subroutine entered with a SAVE statement logically ending with a RETURN statement should not call another internal subroutine that is also entered with a SAVE statement, unless .E.L.. is copied to another location before the CALL and restored to .E.L.. from that location after the CALL has been completed.
    
    /// In the following examples, the calling sequences generated by the pseudo-operation are listed below the CALL pseudo-operation. For clarification purposes, AAAAA defines the location of the CALL instruction; SUB is the name of the subroutine called; MOD is an address modifier; A1 through An are arguments; E1 through Em define error returns; E.I. is an identifier; and .E.L.. defines a location where error linkage information is stored. The number sequences 1,2,...,n and 1,2,...,m designate argument positions only.
    
    ///  The following example is in the absolute mode:
    ///  1       8        16
    ///  -----------------------------------------------------
    ///  AAAAA   CALL     SUB,MOD
    ///  *
    ///  AAAAA   TSX1     SUB,MOD
    ///          TRA      *+2
    ///          ZERO
    ///
    
    ///  AAAAA   CALL     SUB,MOD(A1,A2,..,An)E1,E2,...,Em'E.I.' .
    ///  *
    ///  AAAAA   TSX1     SUB,MOD
    ///          TRA      *+2+n+m
    ///          ZERO     O,E.I.
    ///          ARG      A1
    ///          ARG      A2
    ///          .
    ///          .
    ///          .
    ///          ARG      An
    ///          TRA      Em
    ///          .
    ///          .
    ///          .
    ///          TRA      E2
    ///          TRA      E1
    
    
    
    /// We can have an arbitrary number of "arguments" given error returns and subroutine arguments.
    
    /// So, let's get crackin'.
    
    char w[32];     // for output words
    
    /// Try simplest 1st. - CALL SUB,MOD
    
    /// if we have no 3rd arg and arg2 has no (), chances are good we have the simple form.
    /// Somebody let me know if this is wrong/incorrect (if there is anybody who cares enough to look at this code.)
    if (args == NULL && errs == NULL)   // no args or error returns
    {
        if (nPass == 1)
        {
            addr += 3;
            return;
        }
        
        ///  Easy ......
        ///  AAAAA   CALL     SUB,MOD or AAAAA   CALL     SUB
        ///  *
        ///  AAAAA   TSX1     SUB,MOD
        ///          TRA      *+2
        ///          ZERO
        ///
        
        symtab *s = getsym(sub);
        if (s == NULL)
        {
            yyprintf("doCall(): symbol <%s> not found", sub);
            return;
        }
        
        word18 sub = GETLO(s->value); // keep to 18-bits
        
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++,     sub, getOpcode("TSX1"), arg2 ? getmod(arg2) : 0, inLine);
        sprintf(w, "%06o%06o", sub,  getEncoding("tsx1") | (word18)mod);
        outas8Stri(w, addr, LEXline);
        addr += 1;
        
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",     *addr,  *addr + 2, getOpcode("TRA"), 0); *addr += 1;
        sprintf(w, "%06o%06o", addr + 2, getEncoding("tra") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        //fprintf(oct, "%6.6o xxxx 000000000000\n",    (*addr)++);    // ZERO
        outas8data(0, addr, "");
        addr += 1;
        
        return;
    }
    
    if (args)
    {   ///  Hard ......
        ///  AAAAA   CALL     SUB,MOD(A1,A2,..,An)E1,E2,...,Em'E.I.' or CALL SUB (A1,A2,..,An)E1,E2,...,Em'E.I.' .
        ///  *
        ///  AAAAA   TSX1     SUB,MOD
        ///          TRA      *+2+n+m
        ///          ZERO     0, E.I.
        ///          ARG      A1
        ///          ARG      A2
        ///          .
        ///          .
        ///          .
        ///          ARG      An
        ///          TRA      Em
        ///          .
        ///          .
        ///          .
        ///          TRA      E2
        ///          TRA      E1
        
        word36 Args[32];
        word36 Errs[32];
        
        list *l = args;
        int n = 0;
        while (l)
        {
            Args[n] = l->i36 & AMASK;   // keep to 18-bits (for now)
            n += 1;
            l = l->next;
        }
        
        int m = 0;
        if (errs)
        {
            l = errs->l;        // list of error expressions
            while (l)
            {
                Errs[m] = l->i36 & AMASK;
                m += 1;
                l = l->next;
            }
        }
        
        if (nPass == 1)
        {
            addr += 3 + n + m;
            return;
        }
        
        symtab *s = getsym(sub);
        if (s == NULL)
        {
            yyprintf("doCall(): symbol <%s> not found", sub);
            return;
        }
        
        word18 SUB = s->value & AMASK;                         ///< SUB
        word18 EI = 0;
        
        if (errs && errs->p)
        {
            s = getsym(errs->p);
            if (s == NULL)
            {
                yyprintf("doCall(): E.I. symbol <%s> not found", errs->p);
                return;
            }
            EI = s->value & AMASK;
        }
        
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, SUB,           getOpcode("TSX1"), getmod(mod), inLine);
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, *addr + 2 + n + m, getOpcode("TRA"), 0); *addr += 1;
        //fprintf(oct, "%6.6o xxxx %06o000000\n",      (*addr)++, EI);    // ZERO 0,E.I.
        
        sprintf(w, "%06o%06o", SUB, getEncoding("tsx1") | (word18)mod);
        outas8Stri(w, addr, LEXline);
        addr += 1;
        
        sprintf(w, "%06o%06o", addr + 2 + n + m, getEncoding("tra") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        sprintf(w, "%06o000000", EI);    // ZERO 0,E.I.
        outas8Strd(w, addr, "");
        addr += 1;
        // The E.I. is almost certainly wrong. I have no idea what it was or how it was used
        
        // arguments ....
        if (n)
        {
            for(int n2 = 0 ; n2 < n ; n2 += 1)
            {
                /**
                 * If arg can be resolved as a number store it inline, no pointer. Or should we represent a number as a literal?
                 * THe former would make things consistant.
                 */
                word36 An = Args[n2] << 18;
                
                /// a Valid integer?
                //char *end_ptr;
                //An = (word18)strtol(args[n], &end_ptr, 0) & AMASK;  // octal, decimal or hex
                
                // XXX check for literal
                //if (end_ptr == args[n]) // No, just evaluate if for now.
                //    An = Eval(args[n]) & AMASK;
                
                //fprintf(oct, "%6.6o xxxx %06o000000\n",    (*addr)++, An);    // ARG An
                outas8data(An, addr, ""); // ARG An
                addr += 1;
                
            }
        }
        
        // error returns ...
        if (m)
        {
            m -= 1;
            while (m >= 0)
            {
                word18 Em = Errs[m] & AMASK;
                
                //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, Em, getOpcode("TRA"), 0); // ARG Em
                sprintf(w, "%06o%06o", Em, getEncoding("tra") | 0);
                outas8Stri(w, addr, "");
                addr += 1;
                
                m -= 1;
            }
        }
    }
}

/*
 * multics style procedure call ...
 */
extern int litPosInLine;

void doMCall(expr *entry, word36 mod, expr *arg)  // for call pseudoop
{
    if (nPass == 1)
    {
        addr += 1;
        
        // add a =0 literal
        // set up arg as a =0 literal
        litPosInLine += 1;          // bump literal in line position counter
        doNumericLiteral(10, 0);    // create a =0 literal

        addr += 6;

        return;
    }
    
    /*
     * OBJECT CODE
     *  spri    pr6|0
     *  epp0    arglist (if no arg then points to a literal =0)
     *  epp2    entrypoint
     *  sreg    pr6|32
     *  tsp4    pr7|stack_hdeaer.call_op,*
     * OPERATORS
     *  spri4   pr6|stack_frame.return_ptr
     *  sti     pr6|stack_frame.return_ptr+1
     *  epp4    pr6|stack_frame.lp_ptr,*
     *  call6   pr2|0
     * OBJECT CODE
     *  lpri    pr6|0
     *  lreg    pr6|32
     */

    outas8ins(0600000254100LL, addr, LEXline);      // spri pr6|0
    addr += 1;

    outas8ins(0600040753100LL, addr, "");          // sreg pr6|32
    addr += 1;

    
    char w[256];
    if (arg)
        sprintf(w, "%06o%06o", (word18)arg->value & AMASK, getEncoding("epp0") | (arg->bit29 ? (1 << 6) : 0));
    else
    {
        // set up arg as a =0 literal
        litPosInLine += 1;                          // bump literal in line position counter
        literal *arg0 = doNumericLiteral(10, 0);    // create a =0 literal
        sprintf(w, "%06o%06o", arg0->addr, getEncoding("epp0"));
    }
    outas8Stri(w, addr, NULL);
        
    addr += 1;
    
    //sprintf(w, "%06o%06o", (4 << 15) | (word18)(entry->value & 077777) & AMASK,  getEncoding("epp2") | (word18)mod | (entry->bit29 ? (1 << 6) : 0));
    //outas8Stri(w, addr, NULL);
    //addr += 1;
    
    int ep = (word18)(entry->value & 077777);
    sprintf(w, "%o%05o%06o", 4, ep & 077777,  getEncoding("epp2") | (word18)BIT(29) | (word18)020);
    outas8Stri(w, addr, NULL);
    addr += 1;

    
    
    //outas8ins(0600040753100LL, addr, NULL);
    //addr += 1;

    outas8ins(0700036670120LL, addr, NULL);
    addr += 1;

    outas8ins(0600000173100LL, addr, NULL);
    addr += 1;

    outas8ins(0600040073100LL, addr, NULL);
    addr += 1;    
}


//void doCall(char *sub, word36 mod, list *args, list *errs)  // for call pseudoop
//{
//    if (callingConvention == 1)
//        doHCall(sub, mod, args, errs);
//    else
//        doMCall(sub, mod, args, errs);
//}


/// The SAVE pseudo-operation is used to produce instructions necessary to save specified index registers and the contents of the error linkage index register.
void doSave(list *reglist)                           // for save pseudoop
{
    char w[32];     // for output strings
    
    /// 1. The symbol in the location field of the SAVE instruction is used by the RETURN instruction as a reference. (This symbol is treated by the assembler as if it had been coded in the variable field of a SYMDEF instruction when the assembler is in the relocatable mode.)
    
    /// 2. The subfields in the variable field, if present, each contain an integer 0-7. Thus, each subfield specifies one index register to be saved.
    
    /// 3. When the SAVE variable field is blank, the following coding is generated:
    /// BBBBB    SAVE
    /// *
    /// BBBBB    TRA     *+3
    ///          ZERO
    ///          RET     *-1
    ///          STI     *-2
    ///          STX1    *-3
    
    if (lastLabel == NULL || (word18)(lastLabel->value) != addr)
    {
        yyprintf("SAVE pseudo-op must have a label; or label address mismatch");
        return;
    }
    
    if (debug) fprintf(stderr, "SAVE: 1 Addr=%o\n", addr);

    if (!reglist)  // variable field is blank
    {
        if (nPass == 1)
        {
            addr += 5;
            if (debug) fprintf(stderr, "SAVE: 2 Addr=%o\n", addr);
            return;
        }
        
//        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", *addr, *addr + 3,  getOpcode("TRA"), 0, inLine); *addr += 1;
//        fprintf(oct, "%6.6o xxxx 000000000000\n",    (*addr)++);
//        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, (*addr - 1) & AMASK, getOpcode("RET"), 0); *addr += 1;
//        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, (*addr - 2) & AMASK, getOpcode("STI"), 0); *addr += 1;
//        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, (*addr - 3) & AMASK, getOpcode("STX1"), 0); *addr += 1;
        
        sprintf(w, "%06o%06o", addr + 3,  getEncoding("tra") | 0);
        outas8Stri(w, addr, LEXline);
        addr += 1;
        
        outas8data(0, addr, "");
        addr += 1;
        
        sprintf(w, "%06o%06o",  (addr - 1) & AMASK, getEncoding("ret") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        sprintf(w, "%06o%06o",  (addr - 2) & AMASK, getEncoding("sti") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        sprintf(w, "%06o%06o", (addr - 3) & AMASK, getEncoding("stx1") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
                
        if (debug) fprintf(stderr, "SAVE: 3 Addr=%o\n", addr);
        return;
    }
    
    /// When it's not blank
    /// BBBBB    SAVE     i1, i2, ...in
    /// *
    /// BBBBB    TRA      *+3+n
    ///          ZERO
    ///          LDX(i1)  **,DU
    ///          LDX(i2)  **,DU
    ///              .
    ///              .
    ///              .
    ///          LDX(i1)  **,DU
    ///          RET      BBBBB+1
    ///          STI      BBBBB+1       // <== This worked fine on everything except the multics machines where this can, because of the ABS indicatior,
    ///          STX1     BBBBB+1       // <== appear to be an indirect word
    ///          STX(i1)  BBBBB+2
    ///             .
    ///             .
    ///             .
    ///          STX(in)  BBBBB+n+1
    else
    {
        /// count the args .....
        int n = 0, nIdx[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        
        list *l = reglist;
        while (l)
        {
            if (nPass == 2)
            {
                word36s xVal = l->i36;
                if (xVal < 0 || xVal > 7)
                {
                    yyprintf("SAVE parameter %d out-of-range. Must be between 0 and 7 is (%lld)\n", l->i36, xVal);
                    return;
                }
                nIdx[n++] = xVal & 07;
            } else
                n += 1;
            if (n > 8)
            {
                yyprintf("SAVE: Maximium of 8 arguments allowed for SAVE pseudoop. At least %d found.\n", n);
                return;
            }
            l =  l->next;
        }
        if (nPass == 1)
        {
            addr += 5 + 2 * n;
            if (debug) fprintf(stderr, "SAVE: 4 Addr=%o\n", addr);
            return;
        }
        
//        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", *addr, *addr + 3 + n, getOpcode("TRA"), 0, inLine); *addr += 1;
//        fprintf(oct, "%6.6o xxxx 000000000000\n",    (*addr)++);
        sprintf(w, "%06o%06o", addr + 3 + n, getEncoding("tra") | 0);
        outas8Stri(w, addr, LEXline);
        addr += 1;
        
        outas8data(0, addr, "");
        addr += 1;
        
        int i = 0;
        while (nIdx[i] != -1)
        {
            char ldx[32];
            sprintf(ldx, "ldx%d", nIdx[i]);
            //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, i, getOpcode(ldx), getmod("DU"));
            sprintf(w, "%06o%06o", i, getEncoding(ldx) | getmod("du"));
            outas8Stri(w, addr, "");
            addr += 1;
            i += 1;
        }
        
        word18 BBBBB = (word18)(lastLabel->value);
        
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("RET"), 0);
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("STI"), 0);
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("STX1"), 0);
        sprintf(w, "%06o%06o", BBBBB + 1, getEncoding("ret") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        sprintf(w, "%06o%06o",  BBBBB + 1, getEncoding("sti") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        sprintf(w, "%06o%06o", BBBBB + 1, getEncoding("stx1") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        i = 0;
        while (nIdx[i] != -1)
        {
            char stx[32];
            sprintf(stx, "stx%d", nIdx[i]);
            //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 2 + i, getOpcode(stx), 0);
            sprintf(w, "%06o%06o", BBBBB + 2 + i, getEncoding(stx) | 0);
            outas8Stri(w, addr, "");
            addr += 1;
            
            i += 1;
        }
        if (debug) fprintf(stderr, "SAVE: 5 Addr=%o\n", addr);
    }
}

/// The RETURN pseudo-operation is used for exit from a subroutine.
void doReturn(char *BBBBB, word36 k)                  // for return pseudoop
{
    char w[32];
    
    /// 1.    The first subfield is required. This subfield must contain a symbol defined by its presence in the location field of a SAVE pseudo-operation. The instructions generated by a RETURN pseudo-operation must make reference to a SAVE instruction within the same subroutine.
    /// 2.    The second subfield is optional. If present, it specifies the error return to be made. For example, if the second subfield contains a value of k, the return is made to the kth error return.
    
    /// RETURN  BBBBB
    ///      TRA     BBBBB+2
    
    /// RETURN  BBBBB,k
    ///      LDX1    BBBBB+1,*  // <== This worked fine on everything except the multics machines where this can, because of the ABS indicatior, appear to be an indirect word
    ///      SBLX1   k,DU
    ///      STX1    BBBBB+1
    ///      TRA     BBBBB+2
    
    symtab *s = getsym(BBBBB);
    if (!s)
    {
        yyprintf("RETURN symbol <%s> not found", BBBBB);
        return;
    }

    if (!k)
    {
        if (nPass == 1)
        {
            addr += 1;
            return;
        }
        
        /// generate:
        ///      TRA     BBBBB+2
        
        
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, s->value + 2, getOpcode("TRA"), 0, inLine);
        sprintf(w, "%06o%06o", (word18)s->value + 2, getEncoding("tra") | 0);
        outas8Stri(w, addr, LEXline);
        addr += 1;
    }
    else if (k)
    {
        if (nPass == 1)
        {
            addr += 4;
            return;
        }
        /// Generate:
        ///      LDX1    BBBBB+1,* (LDX1    BBBBB+1,I)
        ///      SBLX1   k,DU
        ///      STX1    BBBBB+1
        ///      TRA     BBBBB+2
        
        word18 BBBBB = (word18)s->value;
        
        ///< XXX HWR added - 25 Dec25 2012 - Either docs are wrong or I implemented it wrong.
        ///< To return to the kth Error routine need to subtract 1 from k - if k is to begin at 1.
        ///< still has weirdness .......
        ///< XXX HWR 25 June 2013. k is correct. No comment about the above comment. Heh, heh,
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, BBBBB + 1, getOpcode("LDX1"), getmod("*"), inLine);
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, k,         getOpcode("SBLX1"), getmod("DU"));
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("STX1"), 0);
        //fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 2, getOpcode("TRA"), 0);
        
        //sprintf(w, "%06o%06o", BBBBB + 1, getEncoding("ldx1") | getmod("*"));
        sprintf(w, "%06o%06o", BBBBB + 1, getEncoding("ldx1") | getmod("i")); // similiar to * but ignoted the tag field
        outas8Stri(w, addr, LEXline);
        addr += 1;
        
        sprintf(w, "%06o%06o",  (word18)k & AMASK, getEncoding("sblx1") | getmod("DU"));
        outas8Stri(w, addr, "");
        addr += 1;
        
        sprintf(w, "%06o%06o", BBBBB + 1, getEncoding("stx1") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
        
        sprintf(w, "%06o%06o", BBBBB + 2, getEncoding("tra") | 0);
        outas8Stri(w, addr, "");
        addr += 1;
    }
    
}

void doEnd(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    // we want to end the current file not the whole assembly
    LEXgetPop();
}

//void doErlk(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
//{
//}

/**
 * EIS descriptors ...
 */


word36 doDesc4a(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc4a = 0;
    
    if (nPass == 2)
    {
        desc4a = length & 07777;
        desc4a = bitfieldInsert36(desc4a, offset & 07,     15, 3);    // offset
        if (ar == -1)
            desc4a = bitfieldInsert36(desc4a, address & AMASK, 18, 18);   // address
        else
        {
            desc4a = bitfieldInsert36(desc4a, address & 077777, 18, 15);   // offset
            desc4a = bitfieldInsert36(desc4a, ar & 07, 33, 3);   // ar
        }
        desc4a |= CTA4;
        outas8data(desc4a, addr, LEXline);

    }
    addr += 1;
    return desc4a;
}
word36 doDesc6a(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc6a = 0;
    
    if (nPass == 2)
    {
        desc6a = length & 07777;
        desc6a = bitfieldInsert36(desc6a, offset & 07, 15, 3);    // offset
        if (ar == -1)
            desc6a = bitfieldInsert36(desc6a, address & AMASK, 18, 18);   // address
        else
        {
            desc6a = bitfieldInsert36(desc6a, address & 077777, 18, 15);   // offset
            desc6a = bitfieldInsert36(desc6a, ar & 07, 33, 3);   // ar
        }
        desc6a |= CTA6;
        outas8data(desc6a, addr, LEXline);
    }
    addr += 1;
    return desc6a;
}
word36 doDesc9a(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc9a = 0;
    
    if (nPass == 2)
    {
        desc9a = length & 07777;
        desc9a = bitfieldInsert36(desc9a, (offset << 1) & 06, 15, 3);    // offset
        if (ar == -1)
            desc9a = bitfieldInsert36(desc9a, address & AMASK, 18, 18);         // address
        else
        {
            desc9a = bitfieldInsert36(desc9a, address & 077777, 18, 15);   // offset
            desc9a = bitfieldInsert36(desc9a, ar & 07, 33, 3);   // ar
        }
        desc9a |= CTA9;
        outas8data(desc9a, addr, LEXline);
    }
    addr += 1;
    return desc9a;
}
word36 doDesc4fl(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc4fl = 0;
    
    if (nPass == 2)
    {
        desc4fl = length & 077;  // 6-bit length
    
        desc4fl = bitfieldInsert36(desc4fl, scale & 077, 6, 6);         // scale
        desc4fl = bitfieldInsert36(desc4fl, offset & 07, 15, 3);        // offset
        if (ar == -1)
            desc4fl = bitfieldInsert36(desc4fl, address & AMASK, 18, 18);   // address
        else
        {
            desc4fl = bitfieldInsert36(desc4fl, address & 077777, 18, 15);   // offset
            desc4fl = bitfieldInsert36(desc4fl, ar & 07, 33, 3);   // ar
        }
        desc4fl |= CTN4;
        desc4fl |= CSFL;
    
        outas8data(desc4fl, addr, LEXline);
    }
    addr += 1;
    return desc4fl;
}
word36 doDesc4ls(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc4ls= 0;
    
    if (nPass == 2)
    {
        desc4ls = length & 077;  // 6-bit length
        //desc4fl &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc4ls = bitfieldInsert36(desc4ls, scale & 077, 6, 6);         // scale
        desc4ls = bitfieldInsert36(desc4ls, offset & 07, 15, 3);        // offset
        if (ar == -1)
            desc4ls = bitfieldInsert36(desc4ls, address & AMASK, 18, 18);   // address
        else
        {
            desc4ls = bitfieldInsert36(desc4ls, address & 077777, 18, 15);   // offset
            desc4ls = bitfieldInsert36(desc4ls, ar & 07, 33, 3);   // ar
        }
        desc4ls |= CTN4;
        desc4ls |= CSLS;
        
        outas8data(desc4ls, addr, LEXline);
    }
    addr += 1;
    return desc4ls;
}
word36 doDesc4ns(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc4ns= 0;
    
    if (nPass == 2)
    {
        desc4ns = length & 077;  // 6-bit length
        //desc4fl &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc4ns = bitfieldInsert36(desc4ns, scale & 077, 6, 6);         // scale
        desc4ns = bitfieldInsert36(desc4ns, offset & 07, 15, 3);        // offset
        if (ar == -1)
            desc4ns = bitfieldInsert36(desc4ns, address & AMASK, 18, 18);   // address
        else
        {
            desc4ns = bitfieldInsert36(desc4ns, address & 077777, 18, 15);   // offset
            desc4ns = bitfieldInsert36(desc4ns, ar & 07, 33, 3);   // ar
        }
        desc4ns |= CTN4;
        desc4ns |= CSNS;

        outas8data(desc4ns, addr, LEXline);
    }
    addr += 1;
    return desc4ns;
}
word36 doDesc4ts(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc4ts= 0;
    
    if (nPass == 2)
    {
        desc4ts = length & 077;  // 6-bit length
        desc4ts = bitfieldInsert36(desc4ts, scale & 077, 6, 6);         // scale
        desc4ts = bitfieldInsert36(desc4ts, offset & 07, 15, 3);        // offset
        if (ar == -1)
            desc4ts = bitfieldInsert36(desc4ts, address & AMASK, 18, 18);   // address
        else
        {
            desc4ts = bitfieldInsert36(desc4ts, address & 077777, 18, 15);   // offset
            desc4ts = bitfieldInsert36(desc4ts, ar & 07, 33, 3);   // ar
        }
        desc4ts |= CTN4;
        desc4ts |= CSTS;
        
        outas8data(desc4ts, addr, LEXline);
    }
    addr += 1;
    return desc4ts;
}
word36 doDesc9fl(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc9fl = 0;
    
    if (nPass == 2)
    {
        desc9fl = length & 077;  // 6-bit length
        
        desc9fl = bitfieldInsert36(desc9fl, scale & 077, 6, 6);         // scale
        desc9fl = bitfieldInsert36(desc9fl, (offset << 1) & 06, 15, 3);        // offset
        if (ar == -1)
            desc9fl = bitfieldInsert36(desc9fl, address & AMASK, 18, 18);   // address
        else
        {
            desc9fl = bitfieldInsert36(desc9fl, address & 077777, 18, 15);   // offset
            desc9fl = bitfieldInsert36(desc9fl, ar & 07, 33, 3);   // ar
        }
        desc9fl |= CTN9;
        desc9fl |= CSFL;
        
        outas8data(desc9fl, addr, LEXline);
    }
    addr += 1;
    return desc9fl;
}
word36 doDesc9ls(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc9ls= 0;
    
    if (nPass == 2)
    {
        desc9ls = length & 077;  // 6-bit length
        desc9ls = bitfieldInsert36(desc9ls, scale & 077, 6, 6);         // scale
        desc9ls = bitfieldInsert36(desc9ls, (offset << 1) & 06, 15, 3);        // offset
        if (ar == -1)
            desc9ls = bitfieldInsert36(desc9ls, address & AMASK, 18, 18);   // address
        else
        {
            desc9ls = bitfieldInsert36(desc9ls, address & 077777, 18, 15);   // offset
            desc9ls = bitfieldInsert36(desc9ls, ar & 07, 33, 3);   // ar
        }
        desc9ls |= CTN9;
        desc9ls |= CSLS;
        
        outas8data(desc9ls, addr, LEXline);
    }
    addr += 1;
    return desc9ls;
}
word36 doDesc9ns(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc9ns= 0;
    
    if (nPass == 2)
    {
        desc9ns = length & 077;  // 6-bit length
        //desc4fl &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc9ns = bitfieldInsert36(desc9ns, scale & 077, 6, 6);         // scale
        desc9ns = bitfieldInsert36(desc9ns, (offset << 1) & 06, 15, 3);        // offset
        if (ar == -1)
            desc9ns = bitfieldInsert36(desc9ns, address & AMASK, 18, 18);   // address
        else
        {
            desc9ns = bitfieldInsert36(desc9ns, address & 077777, 18, 15);   // offset
            desc9ns = bitfieldInsert36(desc9ns, ar & 07, 33, 3);   // ar
        }
        desc9ns |= CTN9;
        desc9ns |= CSNS;
        
        outas8data(desc9ns, addr, LEXline);
    }
    addr += 1;
    return desc9ns;
}
word36 doDesc9ts(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 desc9ts= 0;
    
    if (nPass == 2)
    {
        desc9ts = length & 077;  // 6-bit length
        desc9ts = bitfieldInsert36(desc9ts, scale & 077, 6, 6);         // scale
        desc9ts = bitfieldInsert36(desc9ts, (offset << 1) & 06, 15, 3);        // offset
        if (ar == -1)
            desc9ts = bitfieldInsert36(desc9ts, address & AMASK, 18, 18);   // address
        else
        {
            desc9ts = bitfieldInsert36(desc9ts, address & 077777, 18, 15);   // offset
            desc9ts = bitfieldInsert36(desc9ts, ar & 07, 33, 3);   // ar
        }
        desc9ts |= CTN9;
        desc9ts |= CSTS;
        
        outas8data(desc9ts, addr, LEXline);
    }
    addr += 1;
    return desc9ts;
}
word36 doDescb(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    word36 descb = 0;
    
    word18 a = 0, b = 0;
    
    if (nPass == 2)
    {
        if (ar == -1)
            a = address & AMASK;
        else
        {
            a = SIGNEXT15(address & 077777);   // offset
            a |= (ar & 07) << 15;   // ar
        }

        if (offset)
        {
            int c = offset & 077;
            if (c < 0 || c > 35)
            {
                yyprintf("doDescb(): C out-of-range (%d)\n", c);
                return 0;
            }
            
            int charPos = c / 9;
            int bitPos = c % 9;
            
            b = (charPos << 16) | (bitPos << 12);
        }

        if (length)
        {
            
            /// is it a register designation?
            //int n = findMfReg(length);
            //if (n != -1)
            //    b |= n & 017;
            //else
            {
                b |= (int)length & 07777; // 12-bits,
            }
        }
        descb = (word36)a << 18 | b;
 
        
        //descb = length & 07777;  // 12-bit length
        //descb = bitfieldInsert36(descb, scale & 077, 6, 6);         // scale
        //descb = bitfieldInsert36(descb, (offset << 1) & 06, 15, 3);        // offset
        //descb = bitfieldInsert36(descb, address & AMASK, 18, 18);   // address
                
        outas8data(descb, addr, LEXline);
    }
    addr += 1;
    return descb;
}

void doDescriptor(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar)
{
    //return p->f(p, address, offset, length, scale, ar);
    if (strcmp(p->name, "desc4a") == 0)
        doDesc4a(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc6a") == 0)
        doDesc6a(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc9a") == 0)
        doDesc9a(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc4fl") == 0)
        doDesc4fl(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc4ls") == 0)
        doDesc4ls(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc4ns") == 0)
        doDesc4ns(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc4ts") == 0)
        doDesc4ts(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc9fl") == 0)
        doDesc9fl(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc9ls") == 0)
        doDesc9ls(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc9ns") == 0)
        doDesc9ns(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "desc9ts") == 0)
        doDesc9ts(p, address, offset, length, scale, ar);
    else if (strcmp(p->name, "descb") == 0)
        doDescb(p, address, offset, length, scale, ar);
}

void doNDSC9 (word18 *args)
{
    /// upto 6 parameters
    /// XXX:  NDSC9    LOCSYM,CN,N,S,SF,AM
    
    if (nPass == 1)
        addr += 1;
    else
    {
        word18 locsym = args[0] ? (args[0] & AMASK) : 0;
        int cn = args[1] ? (args[1] &  07) : 0;
        int n  = args[2] ? (args[2] & 077) : 0;
        int s  = args[3] ? (args[3] &  03) : 0;
        int sf = args[4] ? (args[4] & 077) : 0;
        int am = args[5] ? (args[5] & 017) : 0;     ///< address register #
        
        word36 ndsc9 = 0;
        //if (!arg6)
        if (!args[5])
        {
            ndsc9 = (locsym << 18) & DMASK;
        }
        else
        {
            // ar + offset
            locsym &= 077777;                       // offset
            ndsc9 = (((word36)am << 15) | locsym) <<  18;   // pr# + offset
        }
        
        ndsc9 = bitfieldInsert36(ndsc9, cn << 1, 15, 3);
        ndsc9 = bitfieldInsert36(ndsc9, n,   0, 6);
        ndsc9 = bitfieldInsert36(ndsc9, s,  12, 2);
        ndsc9 = bitfieldInsert36(ndsc9, sf,  6, 6);
        
        //ndsc4 |= (1LL << 14);
        
        //fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, ndsc9, inLine);
        outas8data(ndsc9, addr, LEXline);
        addr += 1;
    }
}

void doNDSC4 (word18 *args)
{
    // upto 6 parameters
    // XXX:  NDSC4    LOCSYM,CN,N,S,SF,AM
    
    if (nPass == 1)
        addr += 1;
    else
    {
        word18 locsym = args[0] ? (args[0] & AMASK) : 0;
        int cn = args[1] ? (args[1] &  07) : 0;
        int n  = args[2] ? (args[2] & 077) : 0;
        int s  = args[3] ? (args[3] &  03) : 0;
        int sf = args[4] ? (args[4] & 077) : 0;
        int am = args[5] ? (args[5] & 017) : 0;     // address register #
        
        word36 ndsc4 = 0;
        //if (!arg6)
        if (!args[5])
        {
            ndsc4 = (locsym << 18) & DMASK;
        }
        else
        {
            // ar + offset
            locsym &= 077777;                       // offset
            ndsc4 = (((word36)am << 15) | locsym) <<  18;   // pr# + offset
        }
        
        ndsc4 = bitfieldInsert36(ndsc4, cn, 15, 3);
        ndsc4 = bitfieldInsert36(ndsc4, n,   0, 6);
        ndsc4 = bitfieldInsert36(ndsc4, s,  12, 2);
        ndsc4 = bitfieldInsert36(ndsc4, sf,  6, 6);
        
        ndsc4 |= (1LL << 14);
        
        //fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, ndsc4, inLine);
        outas8data(ndsc4, addr, LEXline);
        addr += 1;
    }
}

void doBDSC (word18 *args)
{
    if (nPass == 1)
        addr += 1;
    else
    {
        word18 locsym = args[0] ? (args[0] & AMASK) : 0;
        int n  = args[1] ? (args[1] & 07777) : 0;
        int c  = args[2] ? (args[2] &    03) : 0;
        int b  = args[3] ? (args[3] &   017) : 0;
        int am = args[4] ? (args[4] &   017) : 0;     // address register #
        
        word36 bdsc = 0;
        //if (!arg5)
        if (!args[4])
        {
            bdsc = (locsym << 18) & DMASK;
        }
        else
        {
            // ar + offset
            locsym &= 077777;                       // offset
            bdsc = (((word36)am << 15) | locsym) <<  18;    // pr# + offset
        }
        
        bdsc = bitfieldInsert36(bdsc, n,   0, 12);
        bdsc = bitfieldInsert36(bdsc, c,  16,  2);
        bdsc = bitfieldInsert36(bdsc, b,  12,  4);
        
        //fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, bdsc, inLine);
        outas8data(bdsc, addr, LEXline);
        addr += 1;
    }
}

void doDescriptor2(pseudoOp *p, list *argList)
{
    if (nPass == 1)
    {
        addr += 1;
        return;
    }
    
    word18 args[32];
    memset(args, 0, sizeof(args));
    
    list *l;
    int n = 0;
    DL_FOREACH(argList, l)
    {
        args[n++] = l->i36 & AMASK;
        if (n > 31)
        {
            yyerror("doDescriptor2(): argument count > 32");
            return;
        }
    }
    
    if (strcmp(p->name, "ndsc9") == 0)
        doNDSC9(args);
    else if (strcmp(p->name, "ndsc4") == 0)
        doNDSC4(args);
    else if (strcmp(p->name, "bdsc") == 0)
        doBDSC(args);

}

/*
 "000233  aa   500000 000001    	desc9a	index|0,1
 "001555  aa  6 00244 00 0040	desc9a	pr6|164,32
 "002212  aa  6 00676 40 0040	desc9a	pr6|446(2),32
 "000222  aa  6 00121 01 0003	desc9ls	pr6|81,3,0
 "000004  aa   200001 400006    	desc9a	pr2|1(2),6
 "000374  aa  6 00110 00 0017	desc9a	pr6|72,x7
 "002345  aa  2 00003 60 0005	desc9a	pr2|3(3),al
 "001011  aa  7 77777 60 0001	desc9a	pr7|-1(3),1
 "001162  aa  5 00003 20 0006	desc9a	pr5|3(1),ql
 */

/**
 * special instructions - rpt, rpd, rpl, etc ...
 */


char *segName = NULL;

// specifies again the object segment name as it appears in the object segment.
void doName(char *arg1)
{
    if (nPass == 1)
    {
        if (segName)
            fprintf(stderr, "WARNING: attempted redefinition of segment Name ('%s'). Ignoring\n", arg1);
        else
            segName = strdup(arg1);
    }
}


/**
 * Work In Progress ...
 * this non-standard pop tells the loader that this assembly is a segment to be loaded accordingly
 *
 * usage:
 *      segment segno   " tells that this assembly is to be loaded as segment segno
 *      go      address " tells simh that the entry point (go address) is 'address'
 */
//int doSegment_segno = -1;
//
//void doSegment(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
//{
//    switch (nPass)
//    {
//        case 1:
//            if (doSegment_segno != -1)
//            {
//                fprintf(stderr, "Warning: only one (1) \"segment\" directive allowed per assembly. Ignoring\n");
//                return;
//            }
//            doSegment_segno = strtol(arg1, NULL, 0) & 077777;
//            break;
//    }
//}

/*
 * sedgef name1, name2, ...,nameN. Makes the labels name1 through nameN available to the linker for referencing from
 * outside programs using the symbolic names name1 ... nameN. Such incoming refrences go driectly to the lable names so
 * the segdef pseudoop is ususlly used for defining external static data. For program entry points, tge "entry" pseudoop
 * is usually used.
 */

struct segDef
{
    char    *name;      // name to make external
    word18  value;      // value of name
    
    struct segDef *prev;
    struct segDef *next;
};
typedef struct segDef segDef;

segDef *segDefs =  NULL; // pointer to segdefs

segDef * newsegDef()
{
    return (segDef*)calloc(1, sizeof(segDef));
}

void doSegdef(list *lst)
{
    if (nPass == 1)
    {
        list *lel;
        DL_FOREACH(lst, lel)
        {
            if (debug) printf("adding external segdef symbol '%s'\n", lel->p);
            
            
            // probably should check for dup segdefs
            segDef *sd;
            DL_FOREACH(segDefs, sd)
            {
                if (!strcmp(sd->name, lel->p))
                {
                    yyprintf("WARNING: duplicate SEGDEF %s - ignoring", sd->name);
                    goto c1;    // heh, heh, sometimes, only a goto will do! :-)
                }
                    
            }
            
            segDef *s = newsegDef();
            s->name = lel->p;
            s->value = -1;  // no value set yet
            
            DL_APPEND(segDefs, s);
c1:;    }
    }
}

void emitSegdefs()
{
    // process segdefs
    segDef *s;
    DL_FOREACH(segDefs, s)
    {
        symtab *sym = getsym(s->name);
        if (!sym)
            yyprintf("segdef name '%s' not found ", s->name);
        else
        {
            if (sym->segname || sym->extname)
                yyprintf("segdef segdef name '%s' must not be external", s->name);
            else
            {
                s->value = (word18)sym->Value->value;  // in case we need it later
                //fprintf(oct, "!SEGDEF %s %06llo\n", sym->name, sym->value);
                outas8Direct("segdef", s->name, s->value);
            }
        }
    }
}

/*
 * usage: segref segmentname, name1, name2, ...., nameN
 *
 * makes labels name1, name2, ..., nameN as external symbols referencing the entry points name1, name2, ..., nameN
 * in segment segname. -- define external references in segname, with implicit pr4 reference.
 */
struct segRef
{
    char    *name;      // name to make external
    char    *segname;
    //word18  value;    // value of name
    expr    *Value;     // offset in link section
    expr    *offset;    // offset from segname|name in words
    
    symtab  *sym;       // symtab entry associated with the segRef
    
    struct segRef *prev;
    struct segRef *next;
} *segRefs = NULL;
typedef struct segRef segRef;

segRef *newsegRef()
{
    return (segRef*)calloc(1, sizeof(segRef));
}

extern int linkCount;   // running total of links in linkage section

void doSegref(list *lst)
{
    int cnt = 0;
    list *lel;
    
    DL_FOREACH(lst, lel)
    cnt += 1;
    
    if (nPass == 1)
    {
        if (cnt < 2)
        {
            yyerror("must have a 'segement name' as the first parameter");
            return;
        }
        
        char *segname = NULL;
        
        DL_FOREACH(lst, lel)
        {
            char *a = lel->p;
            if (!segname)
            {
                segname = a;
                continue;
            }
            
            symtab *sym = getsym(a);
            if (sym)
            {
                yyprintf("segdef: symbol '%s' already defined", a);
                continue;
            }
            else
            {
                expr *e = newExpr();
                e->type = eExprSegRef;
                e->lc = ".ext.";
                e->bit29 = true;            // symbol will be referenced via pr4
                e->value = 2 * linkCount;   // offset into link section
                
                //sym = addsym(a, 0);
                sym = addsymx(a, e);
                sym->segname = segname;
                sym->extname = a;
                sym->Value = e;
                sym->value = e->value;
                
                if (debug) printf("Adding segref symbol '%s'\n", a);
                
                segRef *el = newsegRef();
                el->name = a;
                el->segname = segname;
                //el->value = 2 * linkCount;  // each link takes up 2 words
                el->sym = sym;
                
                el->Value = e;
                
                DL_APPEND(segRefs, el);

                linkCount += 1;
            }
        }
    }
}

/*
 * search sumbols to se if a symbol with an external reference has already been defined ...
 */
segRef *findExtRef(tuple *t)
{
    segRef *sg;
    DL_FOREACH(segRefs, sg)
    {
        if (sg->offset && !t->c.e)
            continue;
        if (!sg->offset && t->c.e)
            continue;

        if (sg->offset && t->c.e)
        {
            if (!strcmp(sg->segname, t->a.p) && !strcmp(sg->name, t->b.p) && sg->offset->value == t->c.e->value)
                return sg;
        }
        else
            if (!strcmp(sg->segname, t->a.p) && !strcmp(sg->name, t->b.p))
                return sg;
    }
    return NULL;
}

expr *getExtRef(tuple *t)
{
    // see if segref already exists for this segment/offset
    segRef *sg = findExtRef(t);
    if (sg)
        return sg->Value;
    
    if (nPass == 1)
    {
        expr *e = newExpr();
        e->type = eExprLink;
        e->lc = ".ext.";            // an external symbol
        e->value = 2 * linkCount;   // offset into link section
                
        segRef *el = newsegRef();
        //el->name = s;
        //el->segname = t->a.p;
        el->name = t->b.p;
        el->segname = t->a.p;
        
        el->offset = t->c.e;
        
        //el->value = 2 * linkCount;  // each link takes up 2 words
        el->Value = e;
                
        DL_APPEND(segRefs, el);
                
        linkCount += 1;
        
        return el->Value;
    }
    return NULL;
}

void doLinkOld(char *s, tuple *t)
{
    if (nPass == 1)
    {
        symtab *sym = getsym(s);
        if (sym)
        {
            yyprintf("link: symbol '%s' already defined", s);
            return;
        }
        else
        {
            expr *e = NULL;
            // see if segref already exists for this segment/offset
            
            segRef *sg = findExtRef(t);
            if (sg)
            {
                e = sg->Value;

                sym = addsymx(s, e);
                sym->segname = t->a.p;      // segment name
                sym->extname = t->b.p;      // name in segment
                sym->Value = e;
                sym->value = e->value;
                sym->value = e->value;
            }
            else
            {
                e = newExpr();
                e->type = eExprLink;
                e->lc = ".link.";            // an external symbol
                //e->bit29 = true;           // symbol will be references via pr4
                e->value = 2 * linkCount;   // offset into link section
    
                //sym = addsym(a, 0);
                sym = addsymx(s, e);
                sym->segname = t->a.p;      // segment name
                sym->extname = t->b.p;      // name in segment
                sym->Value = e;
                sym->value = e->value;
                
                if (debug) printf("Adding link symbol '%s'\n", s);

                segRef *el = newsegRef();
                //el->name = s;
                //el->segname = t->a.p;
                el->name = t->b.p;
                el->segname = t->a.p;

                //el->value = 2 * linkCount;  // each link takes up 2 words
                el->sym = sym;
                
                el->Value = e;
                
                DL_APPEND(segRefs, el);
            
                linkCount += 1;
            }
        }
    }
}

/*
 *  "link name, extexpression" defines the symbol name with the value equal to the offset from lp to the link pair generated for the external expression extexpression. The name is not an external symbol, so an instruction should refer to this link by: pr4|name,* 
 */
void doLink(char *s, tuple *t)
{
    if (nPass == 1)
    {
        symtab *sym = getsym(s);
        if (sym)
        {
            yyprintf("link: symbol '%s' already defined", s);
            return;
        }
        else
        {
            expr *e = newExpr();
            e->type = eExprLink;
            e->lc = ".link.";            // an external symbol

            // see if segref already exists for this segment/offset ... 
            segRef *sg = findExtRef(t);
            if (sg)
            {
                // if so, get it's value ...
                e->value = sg->Value->value;
                
                sym = addsymx(s, e);
                sym->segname = t->a.p;      // segment name
                sym->extname = t->b.p;      // name in segment
                sym->Value = e;
                sym->value = e->value;
            }
            else
            {
                e->value = 2 * linkCount;   // offset into link section
                
                sym = addsymx(s, e);
                sym->segname = t->a.p;      // segment name
                sym->extname = t->b.p;      // name in segment
                sym->Value = e;
                sym->value = e->value;
                
                if (debug) printf("Adding link symbol '%s'\n", s);
                
                segRef *el = newsegRef();
                el->name = t->b.p;
                el->segname = t->a.p;
                el->offset = t->c.e;
                
                el->sym = sym;
                
                el->Value = e;
                
                DL_APPEND(segRefs, el);
                
                linkCount += 1;
            }
        }
    }
}

/*
 * fill-in external segrefs before beginning pass 2
 */
extern int linkCount;
extern word18 linkAddr;

void fillExtRef()
{
    if (!linkCount)     // and s required?
        return;
    
    if ((addr) % 2)    // linkage (ITS) pairs must be on an even boundary
        addr += 1;
    
    linkAddr = addr;    // offset of linkage section
    
//    segRef *s;
//    DL_FOREACH(segRefs, s)
//    {
//        //s->value = (s->value + linkAddr) & AMASK;
//        s->Value->value = (s->Value->value + linkAddr) & AMASK;
//        
//        addr += 2;
//    }
    addr += 2 * linkCount;
}

/*
 * emit segref directives ...
 */
void emitSegrefs()
{
    // process segrefs
    if (!linkCount)
        return;
    
    outas8Direct("linkage", linkAddr, linkCount);

    segRef *s;
    DL_FOREACH(segRefs, s)
        outas8Direct("segref", s->segname, s->name, s->Value->value + linkAddr, s->offset ? s->offset->value : 0);
}

/*
 * write segment references to linkage section ...
 */
void writeSegrefs()
{
    // process segrefs
    
    word18 maxAddr = 0;
    
    if (linkCount)
    {
        if ((addr) % 2)    // ITS pairs must be on an even boundary
            addr += 1;
            
        if (linkAddr && addr != linkAddr)
                yyprintf("phase error for linkage section %06o != %06o\n", addr, linkAddr);
    }
    
    segRef *s;
    DL_FOREACH(segRefs, s)
    {
        int segno = 0;                                      // filled in by loader
        int offset = s->offset ? (int)s->offset->value : 0; // filled in by loader
        word36 even = ((word36)segno << 18) | 043;          // ITS addressing
        word36 odd = (word36)(offset << 18) & DMASK;        // no modifications (yet)| (arg3 ? getmod(arg3) : 0);
                
        char desc[256];
        if (s->offset)
            sprintf(desc, "link %s$%s%+d", s->segname, s->name, (int)s->offset->value);
        else
            sprintf(desc, "link %s$%s", s->segname, s->name);
        
        outas8data(even, addr++, desc);
        outas8data(odd,  addr++, NULL);
        
        maxAddr = max(maxAddr, linkAddr);
    }
    addr = maxAddr;
}

/*
 * emit segment related directives (if any)
 */
//void emitSegment(FILE *oct)
//{
//    if (segName)
//        fprintf(oct, "!SEGNAME %s\n", segName);
//    
//    if (doSegment_segno != -1)
//        fprintf(oct, "!SEGMENT %d\n", doSegment_segno);
//    
//    // process segdefs
//    //for(segDefs *s = aSegDefs; s < sd; s++)
//    segDef *s;
//    DL_FOREACH(segDefs, s)
//    {
//        symtab *sym = getsym(s->name);
//        if (!sym)
//            yyprintf("ERROR: emitSegment(): segdef: name '%s' not found @ line %d.\n", s->name, yylineno);
//        else
//        {
//            if (sym->segname || sym->extname)
//                yyprintf("ERROR: emitSegment(): segdef name '%s' must not be external @ line %d. Ignoring\n", s->name, yylineno);
//            else
//            {
//                s->value = (word18)sym->value;  // in case we need it later
//                fprintf(oct, "!SEGDEF %s %06llo\n", sym->name, sym->value);
//            }
//        }
//    }
//    
//    // process segrefs
//    
//    int i = 0;
//    
//    symtab *sy = Symtab;
//    while (sy)
//    {
//        char temp[256];
//        
//        if (sy->segname)
//        {
//            sprintf(temp, "%s %s %06llo", sy->segname, sy->name, sy->value & AMASK);
//            fprintf(oct, "!SEGREF %s\n", temp);
//        }
//        i += 1;
//        sy = (symtab*)sy->hh.next;
//    }
//}

/*
 * Multics specific C/S/R code ...
 */

struct stackTemp
{
    char    *name;      // name of temporary
    int     size;       // size of temporary
    int     space;      // actual space taken up by temporary (incl align padding)
    int     align;      // alignment modulus/boundary to which align temporary
    word18  offset;     // address/offset on stack
    symtab *sym;        // pointer to symbol table entry for temporary
    
    struct stackTemp *prev;
    struct stackTemp *next;
    
} *stackTemps = NULL;

typedef struct stackTemp stackTemp;

stackTemp * newStackTemp()
{
    return (stackTemp*)calloc(1, sizeof(stackTemp));
}

/*
 * return how many words are used in temporaries ...
 */
int getTempSize()
{
    int size = 0;
    
    stackTemp *el;
    DL_FOREACH(stackTemps, el)
        size += el->space;
    
    return size;
}

/*
 * fill in temporary stack addresses ...
 */
void fillinTemps()
{
    word18 init_address = 40;       // 1st position of temporaries on stack
    word18 tempAddr = init_address;
    
    stackTemp *el;
    DL_FOREACH(stackTemps, el)
    {
        int padding = 0;    // how much alignment passing temporary needs
        
        // make certain that temporary is aligned on proper boundary
        switch(el->align)
        {
            case 1:
                break;  // always aligned
            case 2:     // align on even boundary
                if (tempAddr % 2)
                    tempAddr += (padding = 1);
                break;
            case 8:     // align on 8-word boundary
                if (tempAddr % 8)
                    tempAddr += (padding = (8 - (tempAddr % 8)));
                break;
        }
        
        el->offset = el->sym->value = tempAddr;
        el->space = el->size + padding;         // space = size + padding
        el->sym->value = el->offset;
        el->sym->Value->value = el->offset;

        
        tempAddr += el->space;
        // XXX what if tempAddr is larger than 15-bits?
    }
}

void doTemp(pseudoOp *p, tuple *lst)        // for TEMP pseudo-ops
{
    int mult = 0;                       // size multiplier; 1, 2, 8
    if (!strcasecmp(p->name, "temp"))
        mult = 1;
    else if (!strcasecmp(p->name, "tempd"))
        mult = 2;
    else if (!strcasecmp(p->name, "temp8"))
        mult = 8;
    else
    {
        yyprintf("unknown/unhandled temporasry pseudoop <%s>", p->name);
        return;
    }
    
    tuple *t;
    DL_FOREACH(lst, t)
    {
        char *_name = t->a.p;   // name of temporary
        int _sz = t->b.i;       // size of this temporary in 1, 2 or 8 word chunks
        
        // check to see if name already exists ...
        symtab *s = getsym(_name);
        switch (nPass)
        {
            case 1:
                if (s == NULL)
                {
                    s = addsym(_name, 0);
                    
                    s->defnLine = yylineno;
                    s->defnFile = strdup(LEXCurrentFilename());
                    s->Value->type = eExprTemporary;
                    s->Value->lc = ".stack.";   // ????
                    
                    if (debug) printf("Adding temporary %s <%s>:%d - ", _name, s->defnFile, s->defnLine);
                }
                else
                {
                    yyprintf("found duplicate symbol/temporary <%s>", _name);
                    return;
                }
                
                // add to list of stack temps
                
                stackTemp *t = newStackTemp();
                t->name = s->name;
                t->size = mult * _sz;
                t->align = mult;    // alignment modulus
                t->offset = -1;      // offset is TBD
                t->sym = s;
                t->sym->temp = t;
                
                if (debug) printf("size:%d\n", t->size);
                
                DL_APPEND(stackTemps, t);
                
                break;
            case 2:
                if (s == NULL)
                {
                    yyprintf("undeclared symbol/temporary <%s> in pass 2", _name);
                    return;
                }
                break;
        }
    }
}


struct entryName
{
    char    *name;      // name to make external
    word18  intValue;   // where in this segment 'name'  begins
    word18  extValue;   // entrypoint for external calls
    
    symtab  *sym;       // symbol that represents entry point
    
    struct entryName *prev;
    struct entryName *next;
};// *entryNames = NULL; //this causes entryNames to change during pass2

typedef struct entryName entryName;

PRIVATE entryName *entryNames = NULL;   // this does not! Compiler bug?

entryName * newEntryName()
{
    return (entryName*)calloc(1, sizeof(entryName));
}

//entryName *getEntryPoint(char *entrypoint)
//{
//    entryName *n;
//    DL_FOREACH(entryNames, n)
//        if (strcmp(n->name, entrypoint) == 0)
//            return n;
//    return NULL;
//}


void doEntry(list *lst)                           // multics CSR Entry pseudo-op
{
    // set up entry-points for symbols in entry list
    if (nPass == 1)
    {
        list *lel;
        DL_FOREACH(lst, lel)
        {
            if (debug) printf("Adding entry name symbol '%s'\n", lel->p);
            
            // check for dup entry names
            if (entryNames)
            {
                entryName *en;
                DL_FOREACH(entryNames, en)
                {
                    if (!strcmp(lel->p, en->name))
                    {
                        yyprintf("duplicate entry name found for entrypoint '%s'", en->name);
                        return;
                    }
                }
            }
            entryName *e = newEntryName();
            e->name = lel->p;
            e->intValue = -1;
            e->extValue = -1;
            
            DL_APPEND(entryNames, e);
        }
    }
}


// fill in entry sequences
int fillinEntrySequences()
{
    int beginningOfEntrySequences = 0;     // offset in entry section ...
    
    entryName *e;
    DL_FOREACH(entryNames, e)
    {
        if (debug) printf("filling-in entry info for symbol '%s'\n", e->name);
        
        char *name = e->name;   // get name of entry point
        symtab *sym = getsym(name);
        if (sym == NULL)
        {
            // Uh-Oh, symbol not found!
            yyprintf("entry name not found for symbol '%s'", e->name);
            continue;
        }
        // check symbol type, better be relative
//        if (sym->Value->type != eExprRelative)
//        {
//            yyprintf("entry name <%s> is not a relocatable type", e->name);
//            return;
//        }
        if (nPass == 1)
        {
            e->sym = sym;                                           // symbol table entry for entry name
            e->intValue = (word18)sym->Value->value;                // this is the symbols interal entry point
            e->extValue = addr + beginningOfEntrySequences;         // this is the symbols external entry point
        } else {
            if (e->intValue != sym->Value->value)                   // this is the symbols interal entry point
                yyprintf("phase error for entrypoint <%s> intValue %06o/%06o", sym->name, e->intValue, sym->Value->value);
            if (e->extValue != addr + beginningOfEntrySequences)    // this is the symbols interal entry point
                yyprintf("phase error for entrypoint <%s> extValue %06o/%06o", sym->name,  e->extValue, beginningOfEntrySequences);
        }
        beginningOfEntrySequences += 2;                             // each entry sequence takes uf 2 words
    }

    if (debug) printf("%d words used by entry sequences ...\n", beginningOfEntrySequences);
    
    return beginningOfEntrySequences;
}

void writeEntrySequences()
{
    fillinEntrySequences(); // consistency check for pass2
    
    entryName *e;
    DL_FOREACH(entryNames, e)
    {
        if (debug) printf("creating entry sequence for symbol '%s'\n", e->name);
        
        char w[256];
        sprintf(w, "Entry Sequence for %s (%06o)", e->name, e->intValue);
        outas8ins(0700046272120LL, addr, w);
        addr += 1;
        
        sprintf(w, "%06o%06o", e->intValue,  getEncoding("tra"));
        outas8Stri(w, addr, "");
        addr += 1;
    }
}

void dumpEntrySequences()
{
    entryName *e;
    printf("entryNames=%p\n", entryNames);
    DL_FOREACH(entryNames, e)
        printf("dumping entry sequence for symbol '%s'\n", e->name);
    
}

void emitEntryDirectives()
{
    entryName *e;
    DL_FOREACH(entryNames, e)
        outas8Direct("entry", e->name, e->intValue, e->extValue);
}


const int stack_frame_min_length = 48;  // 060

void doPush(word36 size)                       // multics CSR Push Pseudo-op
{
    if (nPass == 1)
    {
        addr += 2;
        return;
    }
    
    // push      {expression} -- generate code to push a stack frame of
    // expression words; if omitted, enough words for all temp
    // pseudo-ops are allocated.
    
    //  000000  aa   000060 6270 00     52  absadr:	push
    //  000001  aa  7 00040 2721 20
    
    /*
     * OBJECT CODE:
     *      eax7    stack_frame_size
     *      tsp2    pr7|stack_header.push_op,*
     */
    char w[256];
    
    // XXX fixme. If there is no arg then push allocates just enough to hold all temp, tempd and temp8 storage, else
    // just allocate space specified by size
    int stack_frame_size = stack_frame_min_length;
    
    if (size == 0)
    {
        // get size of temporaries needed ....
        int temp_sz = getTempSize();
        if (temp_sz > 8)
            stack_frame_size += temp_sz - 8;
    } else
        stack_frame_size = (int)size;
    
    // HWR 14 March 2014
    // round to nearest 8-words(because this is the way alm seems todo it)
    if (stack_frame_size % 8)
        stack_frame_size += 8 - (stack_frame_size % 8);

    sprintf(w, "%06o%06o", (word18)(stack_frame_size & AMASK), getEncoding("eax7"));
    outas8Stri(w, addr, LEXline);
    addr += 1;
    
    sprintf(w, "%06o%06o", 0700040,  getEncoding("tsp2") | (word18)BIT(29) | (word18)020);
    outas8Stri(w, addr, "");
    addr += 1;
}

void doReturn0()            // multics CSR Return pseudo-op
{
    if (nPass == 1)
    {
        addr += 1;
        return;
    }
    
    // 000072  aa  7 00042 7101 20    155  	return
    //              000042    1-47  	equ	stack_header.return_op_ptr,34		ptr to standard return operator
    /*
     * OBJECT CODE:
     *      tra pr7|stack_header.return_op,*
     *
     return_mac:
     even			"see note at label 'alm_return' of pl1_operators_
     sprisp	sb|stack_header.stack_end_ptr   reset stack end pointer
     eppsp	sp|stack_frame.prev_sp,*        pop stack
     fast_return:
     epbpsb	sp|0                            set sb up
     eppap	sp|stack_frame.operator_ptr,*   set up operator pointer
     ldi     sp|stack_frame.return_ptr+1     restore indicators for caller
     rtcd	sp|stack_frame.return_ptr       continue execution after call
     
     */
    
    char w[256];
    
    sprintf(w, "%06o%06o", 0700042,  getEncoding("tra") | (word18)BIT(29) | (word18)020);
    outas8Stri(w, addr, LEXline);
    addr += 1;
}

void doShortCall(char *entrypoint)      // multics CSR Short_Call pseudo-op
{
    if (nPass == 1)
    {
        addr += 3;
        return;
    }
    
    /*
     OBJECT CODE:
     epp2    entrypoint
     tsp4    pr7|stack_header.call_op,*
     OPERATORS:
     (same as call)
     OBJECT CODE:
     epp4    pr6|stack_frame.lp_ptr,*
     
     
     000200  4a  4 00026 3521 20         	short_call rcp_check_attach_lv_$rcp_check_attach_lv_(ap|0)
     000201  aa  7 00036 6701 20
     000202  aa  6 00030 3701 20
     */
    
    char w[256];
    symtab *eps = getsym(entrypoint);
    if (eps == NULL)
    {
        yyprintf("undefined symbol '%s'", entrypoint);
        return;
    }
    word18 ep = (word18)eps->Value->value;
    
    sprintf(w, "%o%05o%06o", 4, ep & 077777,  getEncoding("epp2") | (word18)BIT(29) | (word18)020);
    outas8Stri(w, addr, LEXline);
    addr += 1;
    sprintf(w, "%06o%06o", 0700036,  getEncoding("tsp4") | (word18)BIT(29) | (word18)020);
    outas8Stri(w, addr, NULL);
    addr += 1;
    sprintf(w, "%06o%06o", 0600030,  getEncoding("epp4") | (word18)BIT(29) | (word18)020);
    outas8Stri(w, addr, NULL);
    addr += 1;
}

void doShortReturn()                           // multics CSR Short_Return pseudo-op
{
    if (nPass == 1)
    {
        addr += 1;
        return;
    }
    
    //  000044    1-48  	equ	stack_header.ret_no_pop_op_ptr,36	ptr: stand. return/ no pop operator
    
    // 7 00044 7101 20
    /*
     * OBJECT CODE:
     *      tra     pr7|stack_header.short_return_op,*
     * OPERATORS:
     *      epbp7   pr6|0
     *      epp0    pr6|stack_frame.operator_ptr,*
     *      ldi     pr6|stack_frame.return_ptr+1
     *      rtcd    pr6|stack_frame.return_ptr
     *
     * fast_return:
     epbpsb	sp|0                            set sb up
     eppap	sp|stack_frame.operator_ptr,*   set up operator pointer
     ldi    sp|stack_frame.return_ptr+1     restore indicators for caller
     rtcd	sp|stack_frame.return_ptr       continue execution after call
     */
    
    char w[256];
    
    sprintf(w, "%06o%06o", 0700044,  getEncoding("tra") | (word18)BIT(29) | (word18)020);
    outas8Stri(w, addr, LEXline);
    addr += 1;
}

void doInhibit(char *o)
{
    if (!strcasecmp(o, "on"))
        bInhibit = true;
    else if (!strcasecmp(o, "off"))
        bInhibit = false;
    else
        yyerror("must specify either ON or OFF for inhibit");
}

/**
 * Work In Progress ...
 * this non-standard pop tells the loader the entry point for this assembly
 *
 * usage:
 *      entrypoint entry   " tells the loader to set this assembly's "go" address to entry
 *
 */

//char *doGo_AddrString = NULL;
expr *entryPoint = NULL;

//void emitGo(FILE *oct)
//{
//    /// emit go directive (if any)
//    if (entryPoint)
//    {
//        if (debug) fprintf(stderr, "!GO %06o\n", (word18)(entryPoint->value & 0777777));
//       
//        entryPoint = NULL;
//    }
//}

void doEntryPoint(expr *entry)
{
   
    switch (nPass)
    {
        case 1:
            break;
        case 2:
            if (entryPoint)
            {
                yyerror("Warning: only one (1) \"entrypoint\" directive allowed per assembly. Ignoring\n");
                return;
            }
            entryPoint = entry;
            if (strcmp(entry->lc, ".text."))
            {
                yyprintf("only .text. \"entrypoint\" allowed (%s). Ignoring", entry->lc);
                return;
            }
            outas8Direct("entrypoint", (word18)(entryPoint->value & 0777777));
            
            break;
    }

}

/*
 * do DUP/DUPEND processing ...
 */
extern bool bGetFromElsewhere;
extern int nDupCount;
extern list* dupList;

void doDup(expr *e)
{
    if (e == NULL)  // dupend
    {
        bGetFromElsewhere = true;
        setState(0);
        return;
    }
    
    if (e->type != eExprAbsolute)
    {
        yyerror("argument to DUP must be an absolute value");
        return;
    }
    nDupCount = (int)e->value;
    dupList = 0;
    setState(1);    // lexical tie-in. Set start state to InDUP
}


int getmod(const char *arg_in);

/**
 * pseudo-op framework ......
 */

pseudoOp pseudoOps[] =
{
    {"bss",         0,              NULL,      BSS },
    {"tally",       0,              NULL,    TALLY },
    {"tallyb",      0,              NULL,    TALLY },
    {"tallyc",      0,              NULL,    TALLY },
    {"tallyd",      0,              NULL,    TALLY },
    {"dec",         epDEC,          NULL,      DEC },
    {"oct",         epOCT,          NULL,      OCT },
    {"arg",         0,              NULL,      ARG },
    {"zero",        0,              NULL,     ZERO },
    {"vfd",         epVFD,          NULL,      VFD },
    {"even",        0,              NULL, PSEUDOOP },
    {"odd",         0,              NULL, PSEUDOOP },
    {"eight",       0,              NULL, PSEUDOOP },
    {"sixteen",     0,              NULL, PSEUDOOP },
    {"sixtyfour",   0,              NULL, PSEUDOOP },
    {"org",         0,              NULL,      ORG },
    {"acc",         epStringArgs,   NULL,    STROP },
    {"aci",         epStringArgs,   NULL,    STROP },
    {"ac4",         epStringArgs,   NULL,    STROP },
    {"equ",         epDEC,          NULL,      EQU },
    {"bool",        epOCT,          NULL,     BOOL },
//    {"include",     0, doInclude }, // handled in scanner
    {"its",         0,              NULL,      ITS },
    {"itp",         0,              NULL,      ITP },
    {"null",        0,              NULL,   NULLOP },
    {"rem",         0,              NULL,   NULLOP },
    {"mod",         0,              NULL,      MOD },
    
    {"bci",         epStringArgs,   NULL,    STROP },
    
    // Honeywell/GE subroutine call/save/return
    {"call",        0,              NULL,     CALL },
    {"save",        0,              NULL,     SAVE },
    {"return",      0,              NULL,   RETURN },

    // multics C/S/R
    {"short_call",  epUnknown,      NULL,   SHORT_CALL   },
    {"short_return",epUnknown,      NULL,   SHORT_RETURN },
    {"entry",       epUnknown,      NULL,   ENTRY  },
    {"push",        epUnknown,      NULL,   PUSH   },
    
    {"temp",        epUnknown,      NULL,   TEMP   },
    {"temp8",       epUnknown,      NULL,   TEMP   },
    {"tempd",       epUnknown,      NULL,   TEMP   },
    {"getlp",       epUnknown,      NULL, PSEUDOOP },

    
    //  {"erlk",        0, doErlk    },
    {"end",         0,              NULL, PSEUDOOP },

    /// for alm versions of EIS descriptors
    {"desc4a",      epDESC,         NULL,     DESC },
    {"desc6a",      epDESC,         NULL,     DESC },
    {"desc9a",      epDESC,         NULL,     DESC },
    {"desc4fl",     epDESC,         NULL,     DESC },
    {"desc4ls",     epDESC,         NULL,     DESC },
    {"desc4ns",     epDESC,         NULL,     DESC },
    {"desc4ts",     epDESC,         NULL,     DESC },
    {"desc9fl",     epDESC,         NULL,     DESC },
    {"desc9ls",     epDESC,         NULL,     DESC },
    {"desc9ns",     epDESC,         NULL,     DESC },
    {"desc9ts",     epDESC,         NULL,     DESC },
    {"descb",       epDESC,         NULL,     DESC },
    
    /// Honeywell/Bull versions of EIS operand descriptors.
    {"ndsc9",       0,              NULL,    DESC2 },
    {"ndsc4",       0,              NULL,    DESC2 },
    {"bdsc",        0,              NULL,    DESC2 },
    
    // Wierd instruction formats - rpt, rpd, rpl. Not pseudoops, but we'll treat them specially.
//    {"rpt",         0, doRPT    },
//    {"rptx",        0, doRPT    },
//    
//    {"awd",         0, doARS    },  //Add word displacement to Address Regsiter
//    {"awdx",        0, doARS    },
//    {"a4bd",        0, doARS    },  //Add 4-bit Displacement to Address Register
//    {"a4bdx",       0, doARS    },  //Add 4-bit Displacement to Address Register
//    {"a6bd",        0, doARS    },  //Add 6-bit Displacement to Address Register
//    {"a6bdx",       0, doARS    },  //Add 6-bit Displacement to Address Register
//    {"a9bd",        0, doARS    },  //Add 9-bit Displacement to Address Register
//    {"a9bdx",       0, doARS    },  //Add 9-bit Displacement to Address Register
//    {"abd",         0, doARS    },  //Add  bit Displacement to Address Register
//    {"abdx",        0, doARS    },  //Add  bit Displacement to Address Register
//    
//    {"swd",         0, doARS    },  //Subtract word displacement from Address Regsiter
//    {"swdx",        0, doARS    },
//    {"s4bd",        0, doARS    },  //Subtract 4-bit Displacement from Address Register
//    {"s4bdx",       0, doARS    },  //Subtract 4-bit Displacement from Address Register
//    {"s6bd",        0, doARS    },  //Subtract 6-bit Displacement from Address Register
//    {"s6bdx",       0, doARS    },  //Subtract 6-bit Displacement from Address Register
//    {"s9bd",        0, doARS    },  //Subtract 9-bit Displacement from Address Register
//    {"s9bdx",       0, doARS    },  //Subtract 9-bit Displacement from Address Register
//    {"sbd",         0, doARS    },  //Subtract  bit Displacement from Address Register
//    {"sbdx",        0, doARS    },  //Subtract  bit Displacement from Address Register
 
    //{"rpd",          0, doRPD    },
    //{"rpdx",         0, doRPD    },
    //{"rpl",          0, doRPL    },
    //{"rplx",         0, doRPL    },
    
    
    /// experimental stuff ...
    //{".entry",      0, doFmt1    },   ///< depreciated

    //{"segment",     0, doSegment },
    //{"go",          0, doGo      },
    
    {"name",        0, NULL,    NAME  },     ///< segment name directive
    {"segdef",      0, NULL,    SEGDEF},     ///< segdef directive
    {"segref",      0, NULL,    SEGREF},
    {"link",        0, NULL,    LINK},
    {"inhibit",     0, NULL,    INHIBIT},
    
    {"entrypoint",  epUnknown,  NULL,   ENTRYPOINT  },
    
    {"dup",         epDup,      NULL,   DUP     },
    
    { 0, 0, 0 } ///< end marker do not remove
};

pseudoOp*
findPop(char *name)
{
    pseudoOp *p = pseudoOps;
    while (p->name)
    {
        if (strcasecmp(p->name, name) == 0)
            return p;
        p++;
    }
    return NULL;
}
