/*
 Copyright 2012-2013 by Harry Reed

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

char *arg1, *arg2, *arg3, *arg4, *arg5, *arg6, *arg7, *arg8, *arg9, *arg10;


void doBss(FILE *out, int nPass, word18 *addr, char *line, char *label, char *op, char *arg0, char *args[32])
{
    // if 1 arg just reserve number of words
    // if 2 args set name to value of IC and reserve words
    
    if (debug) printf("arg1=<%s> arg2=<%s>\n", arg1, arg2);
    
    if (arg1 && arg2) {
        word18 gap = (word18)strtol(arg2, NULL, 0);  ///< was 10);
        if (debug) printf("  BSS of %o\n", gap);
        
        symtab *s = getsym(arg1);
        if (nPass == 1) {
            if (s == NULL)
            {
                s = addsym(arg1, *addr);
                if (debug) printf("adding symbol (BSS) %s = %6o (%06o)\n", arg1, *addr, s->value);
            } else
                fprintf(stderr, "found duplicate symbol <%s>\n", arg1);
        } else // pass 2
        {
            if (s == NULL)
            {
                fprintf(stderr, "undeclared sysmbol <%s> in pass2\n", arg1);
            } else {
                if (*addr != s->value)
                    fprintf(stderr, "Phase error for symbol <%s> 1:%06o 2:%06o", arg1, s->value, *addr);
                
            }
            fprintf(out, "%6.6o xxxx 000000000000 %s\n", *addr, line);   //op);
            
        }
        *addr += gap;
        
    } else if (arg1 && !arg2) {
        word18 gap = (word18)strtol(arg1, NULL, 0);  ///< was 10);
        if (debug) printf("  BSS of %d\n", gap);
        if (out)
            fprintf(out, "%6.6o xxxx 000000000000 %s\n", *addr, line);   //op);
        
        *addr += gap;
    } else if (!arg1 && arg2) {
        word18 gap = (word18)strtol(arg2, NULL, 0);  ///< was 10);
        if (debug) printf("  BSS of %d\n", gap);
        if (out)
            fprintf(out, "%6.6o xxxx 000000000000 %s\n", *addr, line);   //op);
        
        *addr += gap;
    }
}

/// The TALLY pseudo-operation is used to generate the tally word referenced for Indirect then Tally (IT) address modification. TALLY is associated with the processing of 6-bit character strings using CI, SC, and SCR modification and word arrays using I, ID, and DI modification.
void doTally(FILE *out, int nPass, word18 *addr, char *line, char *label, char *op, char *arg0, char *args[32])
{
    if (nPass == 2)
    {
        int byte_offset = arg3 ? Eval(arg3) % 6 : 0; ///< 0 - 5 allowed
        int count       = arg2 ? Eval(arg2) & 07777 : 0;
        int taddr       = arg1 ? Eval(arg1) & AMASK : 0;
        
        word36 tb = (taddr << 18) | (count << 6) | byte_offset;
        fprintf(out, "%6.6o xxxx %012llo %s \n", (*addr)++, tb, line);
    }
    else
        (*addr)++;

}

/// The TALLYB pseudo-operation is used to generate the tally word referenced for Indirect then Tally (IT) address modification. TALLYB is associated with the processing of 9-bit byte strings using CI, SC, and SCR modification.
void doTallyB(FILE *out, int nPass, word18 *addr, char *line, char *label, char *op, char *arg0, char *args[32])
{
    if (nPass == 2)
    {
        int byte_offset = arg3 ? Eval(arg3) & 3 : 0;    ///< 0 - 3 allowed
        int count       = arg2 ? Eval(arg2) & 07777 : 0;
        int taddr       = arg1 ? Eval(arg1) & AMASK : 0;
        
        word36 tb = (taddr << 18) | (count << 6) | (1 << 5) | byte_offset;
        fprintf(out, "%6.6o xxxx %012llo %s \n", (*addr)++, tb, line);
    }
    else
        (*addr)++;
    
}

extern word8 getmod(const char *arg_in) ;

/// The TALLYC pseudo-operation is used to generate the tally word referenced for Indirect then Tally (IT) address modification. TALLYC is associated with the processing of indirect references to tag fields using IDC and DIC modification.
void doTallyC(FILE *out, int nPass, word18 *addr, char *line, char *label, char *op, char *arg0, char *args[32])
{
    
    if (nPass == 2)
    {
        int mod         = arg3 ? getmod(arg3) : 0;
        int count       = arg2 ? Eval(arg2) & 07777 : 0;
        int taddr       = arg1 ? Eval(arg1) & AMASK : 0;
        
        word36 tb = (taddr << 18) | (count << 6) | mod;
        fprintf(out, "%6.6o xxxx %012llo %s \n", (*addr)++, tb, line);
    }
    else
        (*addr)++;

}

/// The TALLYD pseudo-operation is used to generate the tally word referenced for Indirect then Tally (IT) address modification. TALLYD is associated with the processing of indirect references using I, AD, SD, ID, and DI modification.
void doTallyD(FILE *out, int nPass, word18 *addr, char *line, char *label, char *op, char *arg0, char *args[32])
{
    if (nPass == 2)
    {
        int delta       = arg3 ? Eval(arg3) & 077   : 0;
        int count       = arg2 ? Eval(arg2) & 07777 : 0;
        int taddr       = arg1 ? Eval(arg1) & AMASK : 0;
        
        word36 tb = (taddr << 18) | (count << 6) | delta;
        fprintf(out, "%6.6o xxxx %012llo %s \n", (*addr)++, tb, line);
    }
    else
        (*addr)++;
}

/// The DEC pseudo-operation is used to generate object word(s) containing binary numbers from decimal representations.
void doDec(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
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
    char *token;
    int term = 0;
    while ((token = Strsep(&arg0, ",")) != NULL)
    {
        if (strchr(arg1, '.'))
        {
            char cExpType = 'e';    ///< a regular Ee exponent specifier
            
            /// this *is* a float. Except if it's not (e.g. in a string.....)
            char *e = NULL;
            if ((e = strpbrk(token, "Dd")))
            {
                // a "D" or 'd' specifier (double)
                cExpType = tolower(*e);
                *e = 'e';   // change it to an 'e' so strtold() will recognize it ...
            }
            
            char *end_ptr;
            long double fltVal = strtold(token, &end_ptr);
            if (end_ptr != arg1)
            {
                word36 YPair[2];
                IEEElongdoubleToYPair(fltVal, YPair);
                
                if (out)
                {
                    if (term == 0)
                    {
                        fprintf(out, "%6.6o xxxx %012llo %s\n", (*addr)++, YPair[0] & DMASK, inLine);
                        if (cExpType == 'd')
                            fprintf(out, "%6.6o xxxx %012llo \n", (*addr)++, YPair[1] & DMASK);
                        
                    }
                    else
                    {
                        fprintf(out, "%6.6o xxxx %012llo\n", (*addr)++, YPair[0] & DMASK);
                        if (cExpType == 'd')
                            fprintf(out, "%6.6o xxxx %012llo \n", (*addr)++, YPair[1] & DMASK);
                    }
                }
                else
                    cExpType == 'd' ? (*addr += 2) : (*addr += 1);
                
            }
            
        }
        else
        {
            word36 v64 = strtoll(token, NULL, 0);   // allow dec, octal, hex
            if (out)
            {
                if (term == 0)
                    fprintf(out, "%6.6o xxxx %012llo %s\n", (*addr)++, v64 & DMASK, inLine);
                else
                    fprintf(out, "%6.6o xxxx %012llo\n", (*addr)++, v64 & DMASK);
            }
            else
                (*addr)++;
        }
        term++;
    }
}

void doOct(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    int term=0;
    char *token;
    
    if (strchr(arg0, '"'))
        *strchr(arg0, '"') = '\0';
    while ((token = Strsep(&arg0, ",")) != NULL)
    {
        /**
         * The word is stored in its real form and is not complemented if a minus sign is present. The sign applies to bit 0 only.
         */
        word36s v64 = strtoll(token, NULL, 8);
        bool sign = v64 < 0;
        if (sign)
        {
            v64 = labs(v64);
            v64 |= SIGN36;
        }
        
        if (out)
        {
            if (term == 0)
                fprintf(out, "%6.6o xxxx %012llo %s\n", (*addr)++, v64 & DMASK, inLine);
            else
                fprintf(out, "%6.6o xxxx %012llo\n", (*addr)++, v64 & DMASK);
        }
        else
            (*addr)++;
        term++;
    }
}

void doArg(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (nPass == 2)
    {
        word18 arg;
        word8 mod;
        
        // XXX allow for PR| syntax here .....
        bool bit29 = false;
        
        if(arg1)
        {
            if (strchr(arg1, '|'))
                arg = encodePR(arg1, &bit29);
            else
                arg = Eval(arg1) & AMASK;

            //arg = Eval(arg1) & 0777777;   //strtol(arg1, NULL, 8);
            arg &= 0777777;   //strtol(arg1, NULL, 8);
        }
        else
            arg = 0;
        //if(arg < 0)
        //    arg += 01000000;
        if(arg2) {
            mod = getmod(arg2);
        } else
            mod = 0;
        if (debug) printf("  ARG %o,[%o]\n", arg, mod);
        //fprintf(out, "%6.6o xxxx %6.6o0000%2.2o %s\n", (*addr)++, arg, mod, inLine);   //op);
        fprintf(out, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, arg, bit29, mod, inLine);   //

    }
    else // pass2
        (*addr)++;
}

void doZero(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    word18 hi, lo;

    // arg1 can be a literal.......
    // XXX literals should probably be moved into ExprEval() so anybody can use them
    if (arg1[0] == '=')
    {
        literal *l = doLiteral(arg1, *addr);
        if (l)
            hi = l->addr & AMASK;
        else
            fprintf(stderr, "INTERNAL ERROR: doZero(): file %s :literal *l == NULL @ line %d\n", LEXCurrentFilename(), lineno);
    }
    
    if (nPass == 2) {
        if (arg1)
        {
            // arg1 can be a literal.......
            // XXX literals should probably be moved into ExprEval() so anybody can use them
            if (arg1[0] != '=')
                hi = (word18)Eval(arg1);    // strtol(arg1, NULL, 8);
        }
        else
            hi = 0;
        
        if (arg2)
            lo = (word18)Eval(arg2);    //strtol(arg2, NULL, 8);
        else
            lo = 0;
        //if(hi < 0)
        //    hi += 01000000;
        //if(lo < 0)
        //    lo += 01000000;
        
        hi &= 0777777;  // keep to 18-bits
        lo &= 0777777;  // keep to 18-bits
        
        if (debug) printf("  ZERO %6.6o %6.6o\n", hi, lo);
        if (out) fprintf(out, "%6.6o xxxx %6.6o%6.6o %s\n", (*addr)++, hi, lo, inLine);   //op);
    } else
        (*addr)++;
    
}

void doVfd(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    // XXX: need to make provisions for multipls, sequential vfd's that an cross word boundaries
    // XXX: need to make a/h width fields bits not chars count
    
    char *argLine = strdup(arg0);
    
    if (strchr(argLine, '"'))
        *strchr(argLine, '"') = '\0';  // remove any " comments (XXX: need to check for literals????)
    
    if (nPass == 1)
    {
        int nVfdWords = 0;  ///< total number of vfd words used for this vfd .....
        int vfdBits = 0;    ///< total number of bits used for this vfd ...
        
        char *token;
        while ((token = Strsep(&argLine, ",")) != NULL)
            // XXX needs to be re-worked. e.g. vfd a8/ *+-$,.0  will not encode properly
        {
            char *t0 = strdup(token);
            if (index(t0, '/'))
            {
                char *c2 = index(t0, '/');
                *c2 = 0;
                
                int w;
                
                char *tWidth = strchop(strdup(t0));      ///< width
                if (tolower(tWidth[0]) == 'o')
                    w = (int)strtol(tWidth + 1, NULL, 0);
                else if (tolower(tWidth[0]) == 'a')
                    w = (int)strtol(tWidth + 1, NULL, 0) * 9;   ///< ascii bytes are 9-bits each here.
                else if (tolower(tWidth[0]) == 'h')
                    w = (int)strtol(tWidth + 1, NULL, 0) * 9;   ///< GE BCD chars are 9-bits each here.
                else
                    w = (int)strtol(tWidth, NULL, 0);
                
                free(tWidth);
                
                vfdBits += w;   // accumulate total number of vfd bits 
            }
            free (t0);
        }
        
        nVfdWords = vfdBits / 36;   // how many 36-bits words does this vfd represent?
        if (vfdBits % 36)           // ... any left overs?
            nVfdWords += 1;
        
        //printf("VFDwords=%d <%s>\n", nVfdWords, arg0);
        
        *addr += nVfdWords;
    }
    if (nPass == 2)
    {
        word36 vfdWords[128];
        int vfdWord = 0;    ///< current vfd word
        
        word36 vfd = 0;     ///< final value of vfd
        int bitPos = 36;    ///< start at far left of word ...
        
        char *token;
        while ((token = Strsep(&argLine, ",")) != NULL)
        {
            // XXX needs to be re-worked. e.g. vfd a8/ *+-$,.0  will not encode properly
            
            ///< tokens will be of the form a/b
            char *t0 = strdup(token);
            if (index(t0, '/'))
            {
                char *c2 = index(t0, '/');
                *c2 = 0;
                
                /// XXX: if vfd tWidth begins with [oO] then the values are a logical expression.
                ///                                [aA] then the values are ascii character constants
                ///                                [hH] then the values are GEBCD character constants
                ///                                nothing then values are arithmetic expressions
                
                char *tWidth = strchop(strdup(t0));      ///< width
                char *tValue =        (strdup(c2+1));    ///< value
                
                int w, cnt;
                word36 v;
                
                if (tolower(tWidth[0]) == 'o')
                {
                    w = (int)strtol(tWidth + 1, NULL, 0);
                    v = boolEval(tValue);
                }
                else if (tolower(tWidth[0]) == 'a')
                {
                    cnt = (int)strtol(tWidth + 1, NULL, 0);
                    w = cnt * 9;    //(int)strtol(tWidth + 1, NULL, 0) * 9;
                    v = 0;
                    char *p = tValue;
                    //while (*p)
                    for(int n = 0 ; (n < cnt) && (*p) ; n ++)
                        v = (v << 9) | *p++;
                }
                else if (tolower(tWidth[0]) == 'h') // 6-bit gebcd packed into 9-bits
                {
                    cnt = (int)strtol(tWidth + 1, NULL, 0);
                    w = cnt * 9;
                    v = 0;
                    char *p = tValue;
                    
                    //while (*p)
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
                    w = (int)strtol(tWidth, NULL, 0);
                    v = Eval(tValue);
                }
                
                if (w > 36)
                    fprintf(stderr, "WARNING: doVfd(): term width exceeds 36-bits (%d) - %s\n", w, token);
                
                if (bitPos - w < 0)
                {
                    vfdWords[vfdWord++] = vfd;
                    vfd = 0;
                    bitPos = 36;
                }
               
                bitPos -= w;
                
                word36 mask = bitMask36(w);
                
                vfd = bitfieldInsert36(vfd, v & mask, bitPos, w);
                
                free(tWidth);
                free(tValue);
            }
            free (t0);
        }
        vfdWords[vfdWord++] = vfd;
        for(int n = 0 ; n < vfdWord ; n += 1)
            fprintf(out, "%6.6o xxxx %012llo %s\n", (*addr)++, vfdWords[n], n ? "" : inLine);
        
        //fprintf(out, "%6.6o xxxx %012llo %s\n", (*addr)++, vfd, inLine);
    }// else
     //   (*addr)++;
    
    free(argLine);
}

void doEven(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if(*addr % 2) {
        if (debug) printf("  EVEN align\n");
        if (nPass == 2)
            fprintf(out, "%6.6o xxxx 000000011000 %s \"(allocating 1 nop)\n", (*addr)++, inLine);   //op);
        else
            (*addr) += 1;
    }
}


void doOdd(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if(!(*addr % 2)) {
        if (debug) printf("  ODD align\n");
        if (nPass == 2)
            fprintf(out, "%6.6o xxxx 000000011000 %s \"(allocating 1 nop)\n", (*addr)++, inLine);   //op);
        else
            (*addr) += 1;
    }
}

void doEight(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if(*addr % 8) {
        if (debug) printf("  EIGHT align\n");
        int inc = *addr % 8;
        
        // pad with NOP's 000000011000
        if (out)
        {
            fprintf(out, "%6.6o xxxx 000000011000 %s \"(allocating %d nop's)\n", (*addr)++, inLine, 8-inc);   //op);
            for(int n = 0 ; n < 8 - inc - 1; n++)
                fprintf(out, "%6.6o xxxx 000000011000\n", (*addr)++);
        } else
            (*addr) += 8 - inc;
    }
}
void doSixtyfour(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
   /// align addr to next 64-word boundary (useful for block instructions)
    if(*addr % 8) {
        if (debug) printf("  64T align\n");
        int inc = *addr % 64;
        
        // pad with NOP's 000000011000
        if (out)
        {
            fprintf(out, "%6.6o xxxx 000000011000 %s \"(allocating %d nop's)\n", (*addr)++, inLine, 64-inc);   //op);
            for(int n = 0 ; n < 64 - inc - 1; n++)
                fprintf(out, "%6.6o xxxx 000000011000\n", (*addr)++);
        } else
            *addr += 64 - inc;
    }
}
void doOrg(FILE *out, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (arg1)
    {
        word18 org = (word18)strtol(arg1, NULL, 8); // Eval ????
        if (org < *addr)
        {
            if (nPass == 2)
                fprintf(stderr, "WARNING: 'org' location is less that current address. Ignoring\n");
            return;
        }
        *addr = org;    //(word18)strtol(arg1, NULL, 8);
        if (debug) printf("  ORG at %o\n", *addr);
    }
}

void doAcc(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
   
    char sep = arg0[0];                 ///< 1st char is string seperator
    char *next = strchr(arg0 + 1, sep); ///< next occurence in arg0
    
    int strLength = (int)(next - arg0) - 1;
    if (strLength == -1)
    {
        fprintf(stderr, "acc: Illegal/malformed string <%c>\n", sep);
        return;
    }
    
    /// copy string to a buffer so we can play with it
    char copy[256], *d = copy;
    for(char *p = arg0 + 1; p < next; p++)
        *d++ = *p;
    *d++ = '\0';
    char cstyle[256];
    strexp(cstyle, copy);
    
    strLength = (int)strlen(cstyle);
    
    //printf("strlen = %d+1 %012llo\n", strLength, (word36)'s' << 27LL);
    if (strLength)
    {
        /// convert 8-bit chars to 9-bit bytes in 36-bit words. Oh, Joy!
        int nWords = 0;
        nWords = (strLength+1) / 4 + ((strLength+1) % 4 ? 1 : 0); // +1 because of initial length char
        if (*(next+1) == ',')   // a size specifier
        {
//            char exp[256];
//            int n = sscanf(last+2, "%s", exp);
//            if (n == 1) {
//                int nChars = strtod(exp, 0);
//                if (nChars > strLength)
//                    nWords = (nChars+1) / 4 + ((nChars+1) % 4 ? 1 : 0);
//                else if (nChars < strLength)
//                {
//                    fprintf(stderr, "acc/aci: specified string length less than actual\n");
//                    return;
//                    // XXX TODO pad with <spaces>
//                }
//            }
            int nChars = 0;
            int n = sscanf(next + 2, "%i", &nChars);
            if (n == 1) {
                if (nChars > strLength)
                    nWords = (nChars) / 4 + ((nChars) % 4 ? 1 : 0);
                else if (nChars < strLength)
                {
                    fprintf(stderr, "acc/aci: specified string length less than actual\n");
                    return;
                }
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
            if (oct)
                fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, words[i], i == 0 ? inLine : "");
            else
                (*addr)++;
    }
}


/**
 * very similiar to ALM aci except only works on a single line (may need to change that later)
 * and C-style escapes are supported
 */
void doAci(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    char sep = arg0[0];                 ///< 1st char is string seperator
    char *next = strchr(arg0 + 1, sep); ///< next occurence in arg0
    
    int strLength = (int)(next - arg0) - 1;
    if (strLength == -1)
    {
        fprintf(stderr, "aci: Illegal/malformed string <%c>\n", sep);
        return;
    }
    
    /// copy string to a buffer so we can play with it
    char copy[256], *d = copy;
    for(char *p = arg0 + 1; p < next; p++)
        *d++ = *p;
    *d++ = '\0';
    char cstyle[256];
    strexp(cstyle, copy);
        
    strLength = (int)strlen(cstyle);
    
    if (strLength)
    {
        /// convert 8-bit chars to 9-bit bytes in 36-bit words. Oh, Joy!
        int nWords = (strLength) / 4 + ((strLength) % 4 ? 1 : 0);
        
        if (*(next + 1) == ',')   // a size specifier
        {
//            char exp[256];
//            int n = sscanf(next + 2, "%s", exp);
//            if (n == 1) {
//                int nChars = strtod(exp, 0);
//                if (nChars > strLength)
//                    nWords = (nChars) / 4 + ((nChars) % 4 ? 1 : 0);
//                else if (nChars < strLength)
//                {
//                    fprintf(stderr, "aci: specified string length less than actual\n");
//                    return;
//                }
//            }
            int nChars = 0;
            int n = sscanf(next + 2, "%i", &nChars);
            if (n == 1) {
                if (nChars > strLength)
                    nWords = (nChars) / 4 + ((nChars) % 4 ? 1 : 0);
                else if (nChars < strLength)
                {
                    fprintf(stderr, "aci: specified string length less than actual\n");
                    return;
                }
            }

        }
        
        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 3;
        //for(char *p = arg0 + 1; p < last; p++)
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
            if (oct)
                fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, words[i], i == 0 ? inLine : "");
            else
                (*addr)++;
    }
    else
    {
        if (oct)
            fprintf(oct, "%6.6o xxxx %012llo aci %c%c\n", (*addr)++, 0LL, sep, sep);
        else
            (*addr)++;
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

void doAc4(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    char sep = arg0[0];                 ///< 1st char is string seperator
    char *next = strchr(arg0 + 1, sep); ///< next occurence in arg0
    
    int strLength = (int)(next - arg0) - 1;
    if (strLength == -1)
    {
        fprintf(stderr, "ac4: Illegal/malformed string <%c>\n", sep);
        return;
    }
    
    /// copy string to a buffer so we can play with it
    char copy[256], *d = copy;
    for(char *p = arg0 + 1; p < next; p++)
        *d++ = *p;
    *d++ = '\0';
    //char cstyle[256];
    //strexp(cstyle, copy);
    
    strLength = (int)strlen(copy);
    
    if (strLength)
    {
        /// convert 8-bit chars to 4-bit decimal digits in 36-bit words. Oh, Joy!
        int nWords = (strLength) / 8 + ((strLength) % 8 ? 1 : 0);
        
        if (*(next + 1) == ',')   // a size specifier
        {
            int nChars = 0;
            int n = sscanf(next + 2, "%i", &nChars);
            if (n == 1) {
                if (nChars > strLength)
                    nWords = (nChars) / 8 + ((nChars) % 8 ? 1 : 0);
                else if (nChars < strLength)
                {
                    // XXX: this is not the way alm does it. Fix.
                    fprintf(stderr, "ac4: specified string length less than actual\n");
                    return;
                }
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
            if (oct)
                fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, fixupDecimals(words[i]), i == 0 ? inLine : "");
            else
                (*addr)++;
    }
    else
    {
        if (oct)
            fprintf(oct, "%6.6o xxxx %012llo aci %c%c\n", (*addr)++, 0LL, sep, sep);
        else
            (*addr)++;
    }
}

void doBci(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    char sep = arg0[0];                 ///< 1st char is string seperator
    char *next = strchr(arg0 + 1, sep); ///< last occurence in arg0
    
    int strLength = (int)(next - arg0 - 1);
    if (strLength == -1)
    {
        fprintf(stderr, "WARNING(bci): Illegal/malformed string <%c>\n", sep);
        return;
    }
    
    if (strLength)
    {
        /// convert 8-bit chars to 6-bit GEBCD's packed into 36-bit words. Oh, Joy!
        int nWords = (strLength) / 6 + ((strLength) % 6 ? 1 : 0);
        
        if (*(next + 1) == ',')   // a size specifier
        {
//            char exp[256];
//            int n = sscanf(last+2, "%s", exp);
//            if (n == 1) {
//                int nChars = strtod(exp, 0);
//                if (nChars > strLength)
//                    nWords = (nChars) / 6 + ((nChars) % 6 ? 1 : 0);
//                else if (nChars < strLength)
//                {
//                    fprintf(stderr, "bci: specified string length less than actual\n");
//                    return;
//                }
//            }
            int nChars = 0;
            int n = sscanf(next + 2, "%i", &nChars);
            if (n == 1) {
                if (nChars > strLength)
                    nWords = (nChars) / 6 + ((nChars) % 6 ? 1 : 0);
                else if (nChars < strLength)
                {
                    fprintf(stderr, "WARNING(bci): specified string length less than actual\n");
                    return;
                }
            }
        }
        
        word36 words[nWords];
        memset(words, 0, sizeof(words));
        
        word36 *w = words;
        int nPos = 5;
        for(char *p = arg0 + 1; p < next; p++)
        {
            int q = ASCIIToGEBcd[*p];
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
            if (oct)
                fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, words[i], i == 0 ? inLine : "");
            else
                (*addr)++;
    }
    else
    {
        if (oct)
            fprintf(oct, "%6.6o xxxx %012llo bci %c%c\n", (*addr)++, 0LL, sep, sep);
        else
            (*addr)++;
    }
}

void doEqu(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    //printf("equ %s, %s\n", arg1, arg2);
    symtab *s = getsym(arg1);
    
    word18 val = Eval(arg2) & 0777777;
    if (nPass == 1)
    {
        if (s == NULL)
        {
            s = addsym(arg1, val);
            if (debug) printf("Adding symbol %s = %6o\n", arg1, s->value);
        } else
            fprintf(stderr, "Found duplicate symbol <%s>\n", arg1);
    } else { // pass 2
        if (s == NULL)
            fprintf(stderr, "Undeclared symbol <%s> in pass 2!!\n", arg1);
        else
            if (val != s->value)
                fprintf(stderr, "Phase error for symbol <%s> 1:%06o 2:%06o\n", arg1, val, s->value);
    }
}
void doBool(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    //printf("bool %s, %s\n", arg1, arg2);
    symtab *s = getsym(arg1);
    
    word18 val = boolEval(arg2) & 0777777;
    if (nPass == 1)
    {
        if (s == NULL)
        {
            s = addsym(arg1, val);
            if (debug) printf("Adding symbol %s = %6o\n", arg1, s->value);
        } else
            fprintf(stderr, "Found duplicate symbol <%s>\n", arg1);
    } else { // pass 2
        if (s == NULL)
            fprintf(stderr, "Undeclared symbol <%s> in pass 2!!\n", arg1);
        else
            if (val != s->value)
                fprintf(stderr, "Phase error for symbol <%s> 1:%06o 2:%06o\n", arg1, val, s->value);
    }
}

void doInclude(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (debug)
        printf("Including contents of <%s> ...\n", arg1);
    LEXgetPush(arg1);
}

void doItp(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    /// prno, offset, tag
    if (nPass == 2)
    {
        int prnum = arg1 ? Eval(arg1) & 03 : 0;
        int offset = arg2 ? (Eval(arg2) & 07777777) : 0;
        word36 even = ((word36)prnum << 33) | 041;
        word36 odd = (word36)(offset << 18) | (arg3 ? getmod(arg3) : 0);
        
        if (*addr % 2 == 0)
        {
            // even
            fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, even, inLine);
            fprintf(oct, "%6.6o xxxx %012llo\n", (*addr)++, odd);
        }
        else
        {
            // odd
            fprintf(stderr, "WARNING: ITP word-pair must begin on an even boundary. Ignoring\n");
            *addr += 2;
        }
    }
    else
        *addr += 2;
}

void doIts(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    /// segno, offset, tag
    if (nPass == 2)
    {
        int segno = Eval(arg1) & 077777;
        int offset = (Eval(arg2) & 07777777);
        word36 even = ((word36)segno << 18) | 043;
        word36 odd = (word36)(offset << 18) | (arg3 ? getmod(arg3) : 0);
        
        if ((*addr) % 2 == 0)
        {
            // even
            fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, even, inLine);
            fprintf(oct, "%6.6o xxxx %012llo\n", (*addr)++, odd);
        }
        else
        {
            // odd
            fprintf(stderr, "WARNING: ITS word-pair must begin on an even boundary. Ignoring\n");
            *addr += 2;
        }
    }
    else
        *addr += 2;
}

void doNull(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
}

/**
 * inserts padding (nop) to an <expression> word boundary
 */
void doMod(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1 && nPass == 1)
    {
        fprintf(stderr, "\"mod\" pseudo op takes 1 argument.\n");
        return;
    }
    
    int32 mod = (int32)Eval(arg1);
    
    for(int n = 0 ; *addr % (mod ? mod : 1) ; n++)
        if (nPass == 2)
            fprintf(oct, "%6.6o xxxx 000000011000 %s\n", (*addr)++, inLine);
        else
            (*addr)++;
        
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
 * These are primarily the reasons I implemented the callx, exit, push & pop instructions for this emulator.
 */

int isNotCommaOrLParen(int ch)
{
    bool end = ch == ',' || ch == '(';
    return !end;
}

/**
 * this rather nasty piece of code parses the call instruction. Returns info in sub, mod, args, errs, & ei
 * (elements are NULL if no arg present)
 */
void
parseCall(char *sub, char *mod, char (*args)[256], char (*errs)[256], char *ei, char *src)
{
    /// remove leading/trailing spaces ......
    char *s = strchop(strdup(src));
    
    if (debug) printf("s = <%s>\n", s);
    
    strcpy(sub, "");
    strcpyWhile(sub, s, isNotCommaOrLParen);
    strchop(sub);
    
    if (debug) printf("sub = <%s>\n", sub);
    
    s += strlen(sub) ;
    s = strchop(s); // remove any leading/trailing white space
    
    if (debug) printf("s1=<%s>\n", s);
    
    strcpy(mod, "");
    if (*s == ',') { // found a comma. So, extract mod.....
        strcpyWhile(mod, s + 1, isNotCommaOrLParen);
        strchop(mod);
        
        // look for LParen
        while (*s && *s != '(')
            s++;
        
        if (debug)printf("s2=<%s>\n", s);
    }
    if (debug) printf("mod = <%s>\n", mod);
    
    // get contents of ()
    if (debug) printf("s3=<%s>\n", s);
    int nParen = 1;
    s++;
    char a[256] = {0}, *al = a;
    while (*s && nParen > 0)
    {
        switch (*s)
        {
            case '\0':
                continue;
            case '(':
                nParen += 1;
                break;
            case ')':
                nParen -= 1;
                break;
            default:
                *al++ = *s;
                *al = '\0';
                break;
        }
        s += 1;
    }
    strchop(s);
    if (debug) printf("s4=<%s>\n", s);
    
    if (debug) printf("a = <%s>\n", a);
    // a now contains contents of ()
    
    strchop(s); // remove leading and trailing space
    
    /// look for single quote.....
    char *ap = strchr(s, '\'');
    char er[256] = {0};
    
    if (ap)    // look for ending quoted string (E.I.).
    {
        memset (ei, 0, 256);
        strncpy(er, s, ap - s); // Err Returns
        strcpy (ei, ap);        // E.I.
    }
    else
    {
        strcpy(er, s);  // error returns
        strcpy(ei, ""); // no E.I.
    }
    if (debug) printf("er = <%s>\n", er);
    if (debug) printf("ei = <%s>\n", ei);
    
    /// Now copy ArgList, ErrList, and E.I. to appropriate places ....
    
    char *token;
    int i;
    char *p;
    
    // 1st arg list
    memset(args, 0, sizeof(char[32][256]));
    if (strlen(a))
    {
        i = 0;
        p = a;
        while ((token = Strsep(&p, ",")) != NULL)
            strcpy(args[i++], strchop(token));
    }
    
    // 2nd error list
    memset(errs, 0, sizeof(char[32][256]));
    if (strlen(er))
    {
        i = 0;
        p = er;
        while ((token = Strsep(&p, ",")) != NULL)
            strcpy(errs[i++], strchop(token));
    }
    
    // now remove 's from E.I.
    while ((p = strchr(ei, '\'')))
        *p = ' ';
    strchop(ei);
    
    if (debug)
    {
        printf("Args ...\n");
        int n = 0;
        while (args[n][0])
        {
            fprintf(stderr, "   Arg %d = <%s>\n", n, args[n]);
            n += 1;
        }
    
        printf("Err Returns ...\n");
        n = 0;
        while (errs[n][0])
        {
            fprintf(stderr, "   Err %d = <%s>\n", n, errs[n]);
            n += 1;
        }
    }
}

/// The CALL pseudo-operation is used to generate the standard subroutine calling sequence. (Absolute mode only)
void doCall(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
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
    
    /// Try simplest 1st. - CALL SUB,MOD
    
    /// if we have no 3rd arg and arg2 has no (), chances are good we have the simple form.
    /// Somebody let me know if this is wrong/incorrect (if there is anybody who cares enough to look at this code.)
    if ((arg1 && !arg2 && !strpbrk(arg1, "()")) || (arg1 && arg2 && !arg3 && !strpbrk(arg2, "()")))
    {
        if (nPass == 1)
        {
            *addr += 3;
            return;
        }
    
        ///  Easy ......
        ///  AAAAA   CALL     SUB,MOD or AAAAA   CALL     SUB
        ///  *
        ///  AAAAA   TSX1     SUB,MOD
        ///          TRA      *+2
        ///          ZERO
        ///

        word18 sub = Eval(arg1) & AMASK;    ///< SUB

        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++,     sub, getOpcode("TSX1"), arg2 ? getmod(arg2) : 0, inLine);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",     *addr,  *addr + 2, getOpcode("TRA"), 0); *addr += 1;
        fprintf(oct, "%6.6o xxxx 000000000000\n",    (*addr)++);    // ZERO
    }
    else if ((arg1 && strpbrk(arg1, "()")) || (arg1 && arg2 && strpbrk(arg2, "()")))

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
        
        if (strchr(arg0, '"'))
            *strchr(arg0, '"') = 0;
        
        char sub[256], mod[256];
        char args[32][256];
        char errs[32][256];
        char ei[256];

        parseCall(sub, mod, args, errs, ei, arg0);
        
        if (strlen(sub) == 0)
        {
            fprintf(stderr, "ERROR: CALL must have a destimation. None given.");
            return;
        }
        
        int n = 0;
        while (args[n] && strlen(args[n]))
            n += 1;
        
        int m = 0;
        while (errs[m] && strlen(errs[m]))
            m += 1;
        
        if (nPass == 1)
        {
            *addr += 3 + n + m;
            return;
        }
        
        word18 SUB = Eval(sub) & AMASK;                         ///< SUB
        word18 EI = (ei && strlen(ei)) ? (Eval(ei) & AMASK) : 0;  ///< E.I.
        
        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, SUB,           getOpcode("TSX1"), getmod(mod), inLine);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, *addr + 2 + n + m, getOpcode("TRA"), 0); *addr += 1;
        fprintf(oct, "%6.6o xxxx %06o000000\n",      (*addr)++, EI);    // ZERO 0,E.I.
                                                                        // The E.I. is almost certainly wrong. I have no idea what it was or how it was used
        // arguments ....
        if (n)
        {
            n = 0;
            while (args[n][0])
            {
                /**
                 * If arg can be resolved as a number store it inline, no pointer. Or should we represent a number as a literal?
                 * THe former would make things consistant.
                 */
                word18 An;
                
                /// a Valid integer?
                char *end_ptr;
                An = (word18)strtol(args[n], &end_ptr, 0) & AMASK;  // octal, decimal or hex
                
                // XXX check for literal
                if (end_ptr == args[n]) // No, just evaluate if for now.
                    An = Eval(args[n]) & AMASK;
                
                
                fprintf(oct, "%6.6o xxxx %06o000000\n",    (*addr)++, An);    // ARG An
                n += 1;
            }
        }
        
        // error returns ...
        if (m)
        {
            m -= 1;
            while (m >= 0) 
            {
                symtab *e = getsym(errs[m]);
                if (e == NULL)
                {
                    fprintf(stderr, "ERROR: doCall(): undefined symbol <%s>\n", errs[m]);
                    return;
                }
                word18 Em = e->value & AMASK;
                
                //word18 Em = Eval(errs[m]) & AMASK;
                //fprintf(oct, "%6.6o xxxx %06o000000\n",    (*addr)++, Em);    // ARG Em
                fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, Em, getOpcode("TRA"), 0); // ARG Em

                m -= 1;
            }
        }
    }
}

/// The SAVE pseudo-operation is used to produce instructions necessary to save specified index registers and the contents of the error linkage index register.
void doSave(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
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
    
    if (debug) fprintf(stderr, "SAVE: 1 Addr=%o\n", *addr);
    if (label == NULL)
    {
        fprintf(stderr, "ERROR: SAVE pseudo-op must have a label. None found.\n");
        return;
    }
    
    if (!arg1)  // no missing fields allowed. If 1st is missing then all are missing.
    {
        if (nPass == 1)
        {
            *addr += 5;
            if (debug) fprintf(stderr, "SAVE: 2 Addr=%o\n", *addr);
            return;
        }

        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", *addr, *addr + 3,  getOpcode("TRA"), 0, inLine); *addr += 1;
        fprintf(oct, "%6.6o xxxx 000000000000\n",    (*addr)++);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, (*addr - 1) & AMASK, getOpcode("RET"), 0); *addr += 1;
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, (*addr - 2) & AMASK, getOpcode("STI"), 0); *addr += 1;
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    *addr, (*addr - 3) & AMASK, getOpcode("STX1"), 0); *addr += 1;
        if (debug) fprintf(stderr, "SAVE: 3 Addr=%o\n", *addr);
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
    ///          STI      BBBBB+1
    ///          STX1     BBBBB+1
    ///          STX(i1)  BBBBB+2
    ///             .
    ///             .
    ///             .
    ///          STX(in)  BBBBB+n+1
    else
    {
        /// count the args .....
        int n = 0, nIdx[9] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
        for(int i = 0 ; i < 32 ; i++)
        {
            if (args[i])
            {
                if (nPass == 2)
                {
                    word36 xVal = Eval(args[i]);
                    if (xVal > 7)
                    {
                        fprintf(stderr, "ERROR: SAVE parameter %d out-of-range. Must be between 0 and 7 is (%lld)\n", i, xVal);
                        return;
                    }
                    nIdx[n++] = xVal & 07;
                } else
                    n += 1;
            }
            if (n > 8)
            {
                fprintf(stderr, "ERROR: Maximium of 8 arguments allowed for SAVE pseudoop. %d found.\n", n);
                return;
            }
        }
        if (nPass == 1)
        {
            *addr += 5 + 2 * n;
            if (debug) fprintf(stderr, "SAVE: 4 Addr=%o\n", *addr);
            return;
        }
        
        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", *addr, *addr + 3 + n, getOpcode("TRA"), 0, inLine); *addr += 1;
        fprintf(oct, "%6.6o xxxx 000000000000\n",    (*addr)++);
        int i = 0;
        while (nIdx[i] != -1)
        {
            char ldx[32];
            sprintf(ldx, "LDX%d", nIdx[i]);
            fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, i, getOpcode(ldx), getmod("DU"));
            i += 1;
        }
        
        symtab *s = getsym(label);
        word18 BBBBB = s->value;
        
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("RET"), 0);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("STI"), 0);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("STX1"), 0);

        i = 0;
        while (nIdx[i] != -1)
        {
            char stx[32];
            sprintf(stx, "STX%d", nIdx[i]);
            fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 2 + i, getOpcode(stx), 0);
            i += 1;
        }
        if (debug) fprintf(stderr, "SAVE: 5 Addr=%o\n", *addr);
    }
}

/// The RETURN pseudo-operation is used for exit from a subroutine.
void doReturn(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    /// 1.    The first subfield is required. This subfield must contain a symbol defined by its presence in the location field of a SAVE pseudo-operation. The instructions generated by a RETURN pseudo-operation must make reference to a SAVE instruction within the same subroutine.
    /// 2.    The second subfield is optional. If present, it specifies the error return to be made. For example, if the second subfield contains a value of k, the return is made to the kth error return.
    
    /// RETURN  BBBBB
    ///      TRA     BBBBB+2
    
    /// RETURN  BBBBB,k
    ///      LDX1    BBBBB+1,*
    ///      SBLX1   k,DU
    ///      STX1    BBBBB+1
    ///      TRA     BBBBB+2
    
    if (!arg1)
    {
        fprintf(stderr, "ERROR: RETURN pseudo-op must have at least one parameter\n");
        return;
    }
    if (arg1 && !arg2)
    {
        if (nPass == 1)
        {
            *addr += 1;
            return;
        }
        
        /// generate:
        ///      TRA     BBBBB+2
        symtab *s = getsym(arg1);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, s->value + 2, getOpcode("TRA"), 0, inLine);
    }
    else if (arg1 && arg2)
    {
        if (nPass == 1)
        {
            *addr += 4;
            return;
        }
        /// Generate:
        ///      LDX1    BBBBB+1,*
        ///      SBLX1   k,DU
        ///      STX1    BBBBB+1
        ///      TRA     BBBBB+2

        symtab *s = getsym(arg1);
        word18 BBBBB = s->value;
        
        word18 k = (Eval(arg2)) & AMASK;
        ///< XXX HWR added - 1 25 Dec25 2012 - Either docs are wrong or I implemented it wrong.
        ///< To return to the kth Error routine need to subtract 1 from k - if k is to begin at 1.
        ///< still has weirdness .......
        fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, BBBBB + 1, getOpcode("LDX1"), getmod("*"), inLine);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, k,         getOpcode("SBLX1"), getmod("DU"));
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 1, getOpcode("STX1"), 0);
        fprintf(oct, "%6.6o xxxx %06o%04o%02o\n",    (*addr)++, BBBBB + 2, getOpcode("TRA"), 0);
    }
    
}

void doEnd(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    //while(LEXgetPop() != -1)
    //    ;
    
    // we want to end the current file not the whole assembly
    LEXgetPop();

}

//void doErlk(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
//{
//}

/*
 * the pesudoOps that follow are unique to this assembler.
 */

/**
 * EIS descriptors ...
 */

void doDesc4a (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc4a() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc4a = doDescn(arg1, arg2, 4);
        desc4a |= CTA4;
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc4a, inLine);
    }
}
void doDesc6a (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc6a() must have at least 1 arguments.\n");
        return;
    }

    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc6a = doDescn(arg1, arg2, 6);
        desc6a |= CTA6;
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc6a, inLine);
    }

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
void doDesc9a (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc9a() must have at least 1 argument.\n");
        return;
    }

    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc9a = doDescn(arg1, arg2, 9);
        desc9a |= CTA9;
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc9a, inLine);
    }

}

void doDesc4fl (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc4fl() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc4fl = doDescn(arg1, arg2, 4);
        desc4fl &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc4fl |= CTN4;
        desc4fl |= CSFL;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc4fl |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc4fl, inLine);
    }
}
void doDesc4ls (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc4ls() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc4ls = doDescn(arg1, arg2, 4);
        desc4ls &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc4ls |= CTN4;
        desc4ls |= CSLS;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc4ls |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc4ls, inLine);
    }
}
void doDesc4ns (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc4ns() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc4ns = doDescn(arg1, arg2, 4);
        desc4ns &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc4ns |= CTN4;
        desc4ns |= CSNS;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc4ns |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc4ns, inLine);
    }
}
void doDesc4ts (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc4ts() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc4ts = doDescn(arg1, arg2, 4);
        desc4ts &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc4ts |= CTN4;
        desc4ts |= CSTS;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc4ts |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc4ts, inLine);
    }
}
void doDesc9fl (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc9fl() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc9fl = doDescn(arg1, arg2, 9);
        desc9fl &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc9fl |= CTN9;
        desc9fl |= CSFL;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc9fl |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc9fl, inLine);
    }

}
void doDesc9ls (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc9ls() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc9ls = doDescn(arg1, arg2, 9);
        desc9ls &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc9ls |= CTN9;
        desc9ls |= CSLS;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc9ls |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc9ls, inLine);
    }
}
void doDesc9ns (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc9ns() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc9ns = doDescn(arg1, arg2, 9);
        desc9ns &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc9ns |= CTN9;
        desc9ns |= CSNS;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc9ns |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc9ns, inLine);
    }

}
void doDesc9ts (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDesc9ts() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 desc9ts = doDescn(arg1, arg2, 9);
        desc9ts &= 0777777700077LL; // Keep address & CN. Truncate length to 6-bits.
        
        desc9ts |= CTN9;
        desc9ts |= CSTS;
        
        int scaleFactor = arg3 ? ((int)Eval(arg3) & 077) : 0;
        desc9ts |= scaleFactor << 6;
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, desc9ts, inLine);
    }
}

void doDescb (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    /// XXX: not certain if this is correct. Dunno what to do with the B field cf AL39 Fig 4-8.
    /// XXX: fix when it becomes clear what to do.
    
    if (!arg1)
    {
        fprintf(stderr, "ERROR: doDescb() must have at least 1 argument.\n");
        return;
    }
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word36 descb = doDescBs(arg1, arg2);
        descb |= CTA9;
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, descb, inLine);
    }
}

void doNDSC9 (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    /// upto 6 parameters
    /// XXX:  NDSC9    LOCSYM,CN,N,S,SF,AM
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word18 locsym = arg1 ? (Eval(arg1) & AMASK) : 0;
        int cn = arg2 ? (Eval(arg2) &  07) : 0;
        int n  = arg3 ? (Eval(arg3) & 077) : 0;
        int s  = arg4 ? (Eval(arg4) &  03) : 0;
        int sf = arg5 ? (Eval(arg5) & 077) : 0;
        int am = arg6 ? (Eval(arg6) & 017) : 0;     ///< address register #
        
        word36 ndsc9 = 0;
        if (!arg6)
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
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, ndsc9, inLine);
    }
}

void doNDSC4 (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    // upto 6 parameters
    // XXX:  NDSC4    LOCSYM,CN,N,S,SF,AM
    
    if (nPass == 1)
        *addr += 1;
    else
    {
        word18 locsym = arg1 ? (Eval(arg1) & AMASK) : 0;
        int cn = arg2 ? (Eval(arg2) &  07) : 0;
        int n  = arg3 ? (Eval(arg3) & 077) : 0;
        int s  = arg4 ? (Eval(arg4) &  03) : 0;
        int sf = arg5 ? (Eval(arg5) & 077) : 0;
        int am = arg6 ? (Eval(arg6) & 017) : 0;     // address register #
        
        word36 ndsc4 = 0;
        if (!arg6)
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
        
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, ndsc4, inLine);
    }
}

void doBDSC (FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (nPass == 1)
        *addr += 1;
    else
    {
        word18 locsym = arg1 ? (Eval(arg1) & AMASK) : 0;
        int n  = arg2 ? (Eval(arg2) & 07777) : 0;
        int c  = arg3 ? (Eval(arg3) &    03) : 0;
        int b  = arg4 ? (Eval(arg4) &   017) : 0;
        int am = arg5 ? (Eval(arg5) &   017) : 0;     // address register #
        
        word36 bdsc = 0;
        if (!arg5)
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
                
        fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, bdsc, inLine);
    }
}
/**
 * special instructions - rpt, rpd, rpl, etc ...
 */


// rpt instruction
void doRPT(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    /*
     * supports 2 formats ....
     *
     * rpt  tally, delta, term1, term2, ..., termN
     * rpt  ,delta
     *
     * save/don't forget I bit (interrupt inhibit) for later
     */
    
    if (nPass == 1)
    {
        *addr += 1;     // just consume 1 address
        return;
    }

    word36 w = 000000520200LL;  // basic RPT instruction
    int tally = arg1 ? (Eval(arg1) &  0377) : 0;    // 8-bit tally
    int delta = arg2 ? (Eval(arg2) &  077) : 0;     // 6-bit delta
    
    /*
     * look for terminal conditons
     */
    int terms = 0;  // terminal conditions
    for(int t = 2; args[t] && t < 9; t += 1)   // only consecutive fields (up to 7)
    {
        char term[256];
        strcpy(term, args[t]);
        strchop(term);
        
        if (!strcasecmp(term, "tov"))        // on overflow
        {
            terms |= (1 << 18); // set bit-17
        }
        else if (!strcasecmp(term, "tnc"))   // on not carry
        {
            terms |= (1 << 19); // set bit-16
        }
        else if (!strcasecmp(term, "trc"))   // on carry
        {
            terms |= (1 << 20); // set bit-15
        }
        else if (!strcasecmp(term, "tpl"))   // on negative off
        {
            terms |= (1 << 21); // set bit-14
        }
        else if (!strcasecmp(term, "tmi"))   // on negative (minus)
        {
            terms |= (1 << 22); // set bit-13
        }
        else if (!strcasecmp(term, "tnz"))   // on zero off
        {
            terms |= (1 << 23); // set bit-12
        }
        //            else if (!strcasecmp(args[t], "tpnz"))  // on zero off
        //            {   // negative and zero both off
        //                terms |= (5 << 23); // set bits-12 & 14
        //            }
        //            else if (!strcasecmp(args[t], "tmoz"))  // on minus or zero
        //            {   // negative and zero both on
        //                terms |= (5 << 24);                 // set bits-11 & 13
        //            }
        else if (!strcasecmp(term, "tze"))   // on zero on
        {
            terms |= (1 << 24); // set bit-11
        }
        else
        {
            fprintf(stderr, "WARNING: unrecognized terminal condition for RPT/RPTX <%s>\n", term);
            return;
        }
    }
    //                 dst, src,  off, bits
    w = bitfieldInsert36(w, tally, 28, 8);          // insert tally
    w = bitfieldInsert36(w, delta,  0, 6);          // insert delta

    w |= terms;

    if (!strcasecmp(op, "rpt"))
        w = bitfieldInsert36(w, 1, 25, 1);          // set C

    fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, w, inLine);
}

void doRPD(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{}
void doRPL(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{}

// do address register special stuff ...
void doARS(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (nPass == 1)
    {
        *addr += 1;     // just consume 1 address
        return;
    }
    
    bool bit29 = true;  // the "normal" instruction (e.g. non x version) has bit-29 set
    
    op = strlower(op);                  // convert to lower-case
    char *x = strrchr(op, 'x');         // the x or non-x version?
    if (x == (op + strlen(op) - 1))
        {
            *x = 0;                     // remove 'x' so we can find it in the opcode table ...
            bit29 = false;
        }
    
    word18 arg = 0;
    
    char *v = strchr(arg1, '|');
    if (v)  // a pointer register
        arg = encodePR(arg1, NULL);
    else
        arg = Eval(arg1) & 0777777;
    
    word8 mod = 0;
    if (arg2)
        mod = getmod(arg2);
    else
        mod = 0;
    
    ///<             addr         arg addline  mod  inLine
    // fprintf(oct, "%6.6lo xxxx %6.6lo%3.3lo0%2.2o %s\n", addr++, arg, addline(op, arg1, arg2), mod, inLine);   //
    fprintf(oct, "%6.6o xxxx %06o%04o%02o %s\n", (*addr)++, arg, getOpcode(op) | bit29, mod, inLine);   //
}

char *segName = NULL;

// specifies again the object segment name as it appears in the object segment.
void doName(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
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
int doSegment_segno = -1;

void doSegment(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    switch (nPass)
    {
        case 1:
            if (doSegment_segno != -1)
            {
                fprintf(stderr, "Warning: only one (1) \"segment\" directive allowed per assembly. Ignoring\n");
                return;
            }
            doSegment_segno = strtol(arg1, NULL, 0) & 077777;
            break;
    }
}

/*
 * sedgef name1, name2, ...,nameN. Makes the labels name1 through nameN available to the linker for referencing from
 * outside programs using the symbolic names name1 ... nameN. Such incoming refrences go driectly to the lable names so
 * the segdef pseudoop is ususlly used for defining external static data. For program entry points, tge "entry" pseudoop
 * is usually used.
 */

struct segDefs
{
    char    *name;      // name to make external
    word18  value;      // value of name
} aSegDefs[256];        // only 256 allowed. Increase if we need more
typedef struct segDefs segDefs;

segDefs *sd = aSegDefs; // pointer to segdefs

void doSegdef(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    if (nPass == 1)
    {
        for(int i = 0 ; i < 32 ; i++)
        {
            char *a = args[i];
            if (!a || strlen(a) == 0)
                continue;
            
            if (strlen(a) == 0)
                continue;
            
            if (sd > &aSegDefs[255])
            {
                fprintf(stderr, "WARNING: too many segdefs [max 256 allowed.] Ignoring.\n");
                return;
            }
            
            if (debug) printf("adding external segdef symbol '%s'\n", a);

            sd->name = strdup(a);
            sd += 1;
        }
    }
}


/*
 * usage: segref segmentname, name1, name2, ...., nameN
 *
 * makes labels name1, name2, ..., nameN as external symbols referencing the entry points name1, name2, ..., nameN
 * in segment segname. -- define external references in segname, with implicit pr4 reference.
 */
//struct segRefs
//{
//    char    *name;      // name to make external
//    word18  value;      // value of name
//} aSegRefs[256];        // only 256 allowed. Increase if we need more
//typedef struct segRefs segRefs;

extern int linkCount;

void doSegref(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    
    if (nPass == 1)
    {
        if (arg1 == NULL)
        {
            fprintf(stderr, "ERROR: doSegref(): must have a 'segement name' as the first parameter @ line %d.. Ignoring directive.\n", lineno);
            return;
        }
        
        for(int i = 1 ; i < 32 ; i++)
        {
            char *a = args[i];
            if (!a || strlen(a) == 0)
                continue;
            
            if (strlen(a) == 0)
                continue;
            
            symtab *sym = getsym(a);
            if (sym)
            {
                fprintf(stderr, "ERROR: doSegdef(): symbol '%s' already defined @ line %d. Ignoring.\n", a, lineno);
                continue;
            }
            else
            {
                sym = addsym(a, 0);
                
                sym->segname = strdup(arg1);
                sym->extname = strdup(a);
                
                if (debug) printf("adding segref symbol '%s'\n", a);
                
                linkCount += 1;
            }
        }
    }
}

/*
 * emit segment related directives (if any)
 */
void emitSegment(FILE *oct)
{    
    if (segName)
        fprintf(oct, "!SEGNAME %s\n", segName);
    
    if (doSegment_segno != -1)
        fprintf(oct, "!SEGMENT %d\n", doSegment_segno);

    // process segdefs
    if (sd > aSegDefs)
    {
        for(segDefs *s = aSegDefs; s < sd; s++)
        {
            symtab *sym = getsym(s->name);
            if (!sym)
                fprintf(stderr, "ERROR: emitSegment(): segdef: name '%s' not found @ line %d.\n", s->name, lineno);
            else
            {
                if (sym->segname || sym->extname)
                    fprintf(stderr, "ERROR: emitSegment(): segdef name '%s' must not be external @ line %d. Ignoring\n", s->name, lineno);
                else
                {
                    s->value = sym->value;  // in case we need it later
                    fprintf(oct, "!SEGDEF %s %06o\n", sym->name, sym->value);
                }
            }
        }
    }
    
    // process segrefs
    int i = 0;
    while (Symtab[i].name != NULL)
	{
        char temp[256];
        
        if (Symtab[i].segname)
        {
            sprintf(temp, "%s %s %06o", Symtab[i].segname, Symtab[i].name, Symtab[i].value);
            fprintf(oct, "!SEGREF %s\n", temp);
            
        }
        i += 1;
    }

}


/**
 * Work In Progress ...
 * this non-standard pop tells the loader the entry point for this assembly
 *
 * usage:
 *      go entry   " tells the loader to set this assembly's "go" address to entry
 *
 */

char *doGo_AddrString = NULL;

void emitGo(FILE *oct)
{
    /// emit go directive (if any)
    if (doGo_AddrString)
    {
        fprintf(oct, "!GO %06o\n", (word18)Eval(doGo_AddrString) & 0777777);
        doGo_AddrString = NULL;
    }
}

void doGo(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
   
    switch (nPass)
    {
        case 1:
            doGo_AddrString = strdup(arg1);
            break;
        case 2:
            if (doGo_AddrString)
            {
                fprintf(stderr, "Warning: only one (1) \"go\" directive allowed per assembly. Ignoring\n");
                return;
            }
            break;
    }

}

word8 getmod(const char *arg_in);

#ifdef DEPRECIATED
void doFmt1(FILE *oct, int nPass, word18 *addr, char *inLine, char *label, char *op, char *arg0, char *args[32])
{
    
    switch (nPass)
    {
        case 1:
            (*addr)++;
            break;
        case 2:
        {
            word18 entry = (arg1 ? Eval(arg1) : 0) & AMASK;
            word4      t = (arg2 ? getmod(arg2) : 0) & 017;
            word14   blk = (arg3 ? Eval(arg3) : 0) & 07777;
            
            word36 fmt1 = (entry << 18) | (blk << 6) | t;
            
            fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, fmt1, inLine);

        }
        break;
    }
    
}
#endif

/**
 * pseudo-op framework ......
 */

pseudoOp pseudoOps[] =
{
    {"bss",         1, doBss     },
    {"tally",       0, doTally   },
    {"tallyb",      0, doTallyB  },
    {"tallyc",      0, doTallyC  },
    {"tallyd",      0, doTallyD  },
    {"dec",         0, doDec     },
    {"oct",         0, doOct     },
    {"arg",         0, doArg     },
    {"zero",        0, doZero    },
    {"vfd",         0, doVfd     },
    {"even",        0, doEven    },
    {"odd",         0, doOdd     },
    {"eight",       0, doEight   },
    {"sixtyfour",   0, doSixtyfour    },
    {"org",         0, doOrg     },
    {"acc",         0, doAcc     },
    {"aci",         0, doAci     },
    {"ac4",         0, doAc4     },
    {"equ",         2, doEqu     },
    {"bool",        2, doBool    },
    {"include",     1, doInclude },
    {"its",         3, doIts     },
    {"itp",         3, doItp     },
    {"null",        0, doNull    },
    {"mod",         0, doMod     },
    {"bci",         0, doBci     },
    
    {"call",        0, doCall    },
    {"save",        0, doSave    },
    {"return",      0, doReturn  },
//  {"erlk",        0, doErlk    },
    {"end",         0, doEnd     },

    /// for alm versions of EIS descriptors
    {"desc4a",      0, doDesc4a  },
    {"desc6a",      0, doDesc6a  },
    {"desc9a",      0, doDesc9a  },
    {"desc4fl",     0, doDesc4fl },
    {"desc4ls",     0, doDesc4ls },
    {"desc4ns",     0, doDesc4ns },
    {"desc4ts",     0, doDesc4ts },
    {"desc9fl",     0, doDesc9fl },
    {"desc9ls",     0, doDesc9ls },
    {"desc9ns",     0, doDesc9ns },
    {"desc9ts",     0, doDesc9ts },
    {"descb",       0, doDescb   },
    
    /// Honeywell/Bull versions of EIS operand descriptors.
    {"ndsc9",       0, doNDSC9   },
    {"ndsc4",       0, doNDSC4   },
    {"bdsc",        0, doBDSC    },
    
    // Wierd instruction formats - rpt, rpd, rpl. Not pseudoops, but we'll treat them specially.
    {"rpt",         0, doRPT    },
    {"rptx",        0, doRPT    },
    
    {"awd",         0, doARS    },  //Add word displacement to Address Regsiter
    {"awdx",        0, doARS    },
    {"a4bd",        0, doARS    },  //Add 4-bit Displacement to Address Register
    {"a4bdx",       0, doARS    },  //Add 4-bit Displacement to Address Register
    {"a6bd",        0, doARS    },  //Add 6-bit Displacement to Address Register
    {"a6bdx",       0, doARS    },  //Add 6-bit Displacement to Address Register
    {"a9bd",        0, doARS    },  //Add 9-bit Displacement to Address Register
    {"a9bdx",       0, doARS    },  //Add 9-bit Displacement to Address Register
    {"abd",         0, doARS    },  //Add  bit Displacement to Address Register
    {"abdx",        0, doARS    },  //Add  bit Displacement to Address Register
    
    {"swd",         0, doARS    },  //Subtract word displacement from Address Regsiter
    {"swdx",        0, doARS    },
    {"s4bd",        0, doARS    },  //Subtract 4-bit Displacement from Address Register
    {"s4bdx",       0, doARS    },  //Subtract 4-bit Displacement from Address Register
    {"s6bd",        0, doARS    },  //Subtract 6-bit Displacement from Address Register
    {"s6bdx",       0, doARS    },  //Subtract 6-bit Displacement from Address Register
    {"s9bd",        0, doARS    },  //Subtract 9-bit Displacement from Address Register
    {"s9bdx",       0, doARS    },  //Subtract 9-bit Displacement from Address Register
    {"sbd",         0, doARS    },  //Subtract  bit Displacement from Address Register
    {"sbdx",        0, doARS    },  //Subtract  bit Displacement from Address Register
 
    //{"rpd",          0, doRPD    },
    //{"rpdx",         0, doRPD    },
    //{"rpl",          0, doRPL    },
    //{"rplx",         0, doRPL    },
    
    
    /// experimental stuff ...
    //{".entry",      0, doFmt1    },   ///< depreciated

    {"segment",     0, doSegment },
    {"go",          0, doGo      },
    
    {"name",        1, doName    },     ///< segment name directive
    {"segdef",      1, doSegdef  },     ///< segdef directive
    {"segref",      2, doSegref  },
    
    
    
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

bool doPseudoOp(char *line, char *label, char *op, char *arg0, char *args[32], FILE *out, int nPass, word18 *addr)
{
    pseudoOp *p = findPop(op);
    if (p == NULL)
        return false;

    if (debug) printf("dpPseudoOp(%s) <%s> <%s> <%s> <%s> <%s>\n", op, arg0, arg1, arg2, arg3, arg4);

    arg1 = args[0];
    arg2 = args[1];
    arg3 = args[2];
    arg4 = args[3];
    arg5 = args[4];
    arg6 = args[5];
    arg7 = args[6];
    arg8 = args[7];
    arg9 = args[8];
    arg10 = args[9];
    
    p->f(out, nPass, addr, line, label, op, arg0, args);
    
    return true;
}
