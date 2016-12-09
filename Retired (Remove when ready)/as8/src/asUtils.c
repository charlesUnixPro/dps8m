/*
 Copyright 2012-2013 by Harry Reed

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later. 
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file asUtils.c
 * \project as8
 * \author Harry Reed
 * \date 10/6/12
 *  Created by Harry Reed on 10/6/12.
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "as.h"
#include <math.h>

/*
 * simple symbol table management stuff ...
 */

/**
 * symbol support. Yup, it's simple. KISS. Modern machines aren't contstrained by such things
 * as speed, memory utilization and effecientcy anymore!! :)
 */
void initSymtab()
{
    for(int i = 0; i < sizeof(Symtab) / sizeof(struct symtab); i++)
        Symtab[i].name = NULL;
}

symtab* getsym(const char *sym)
{
    
    if (sym == NULL)
    {
        fprintf(stderr, "ERROR: getsym(): sym == NULL\n");
        return NULL;
    }
    
    char s[256];
    strcpy(s, sym);
    strchop(s);
    
    /* XXX */
    for(int i = 0; i < sizeof(Symtab) / sizeof(struct symtab); i++)
    {
        if (Symtab[i].name == NULL)
            return NULL;
        if (strcmp(Symtab[i].name, s) == 0)
            return &Symtab[i];
    }
    return NULL;
}

struct symtab *addsym(const char *sym, word18 value) {
    symtab *s = getsym(sym);
    if (s != NULL)
        return NULL;    // symbol already defined
    
    int i = 0;
    while (Symtab[i].name != NULL)
        i++;
    if (i >= sizeof(Symtab) / sizeof(struct symtab))
        return NULL;
    
    Symtab[i].name = strdup(sym);
    Symtab[i].value = value;
    
    Symtab[i].segname = NULL;
    Symtab[i].extname = NULL;
    
    return &Symtab[i];
}

void dumpSymtab()
{
    printf("======== Symbol Table ========\n");
    
    int i = 0;
    while (Symtab[i].name != NULL)
	{
        char temp[256];
        
        if (Symtab[i].segname)
            sprintf(temp, "%s$%s", Symtab[i].segname, Symtab[i].name);
        else
            sprintf(temp, "%s", Symtab[i].name);
        
        
        printf("%-10s %06o   ", temp, Symtab[i].value);
        i++;
        if (i % 4 == 0)
            printf("\n");
	}
    if (i % 4)
        printf("\n");
}

/**
 * literal table stuff ...
 * (like symbol table stuff it's rather primitive, but functional)
 */
void initliteralPool()
{
    for(int i = 0; i < sizeof(literalPool) / sizeof(struct literal); i++)
        literalPool[i].name = NULL;
}

/// get literal associated with this source address
literal* getliteral(word18 srcAddr)
{
    //char lName[256];
    //sprintf(lName, LITERAL_FMT, srcAddr);
    //printf("looking for lit <%s>\n", lit);
    for(int i = 0; i < sizeof(literalPool) / sizeof(struct literal); i++)
    {
        if (literalPool[i].name == NULL)
            return NULL;
        //if (strcmp(literalPool[i].name, lName) == 0)
        //    return &literalPool[i];
        if (literalPool[i].srcAddr == srcAddr)
            return &literalPool[i];
        
    }
    return NULL;
}

literal *addliteral(word18 srcAddr, word18 addr, eLiteralType litType, char *text)
{
    char lName[256];
    
    switch (litType)
    {
        case litUnknown:
        case litGeneric:
            sprintf(lName, LITERAL_FMT, srcAddr);
            break;
        case litSingle:
            sprintf(lName, FLITERAL_FMT, srcAddr);
            break;
        case litDouble:
            sprintf(lName, DLITERAL_FMT, srcAddr);
            break;
        case litScaledFixed:
            sprintf(lName, SCFIXPNT_FMT, srcAddr);
            break;
    }


    literal *l = getliteral(srcAddr);
    if (l != NULL)
        return NULL;
    int i = 0;
    while (literalPool[i].name != NULL && i < sizeof(literalPool) / sizeof(struct literal))
        i++;
    if (i >= sizeof(literalPool) / sizeof(struct literal))
        return NULL;
    
    literalPool[i].name = strdup(lName);
    literalPool[i].srcAddr = srcAddr;
    literalPool[i].addr = addr;
    literalPool[i].litType = litType;
    //printf("Literal added:i=%d addr=%p <%s>\n", i, &literalPool[i], literalPool[i].name);
    if (text)
        literalPool[i].text = strdup(text);
    else
        literalPool[i].text = NULL;
        
    return &literalPool[i];
}

void dumpliteralPool()
{
    printf("======== literal pool ========\n");
    int i = 0;
    while (literalPool[i].name != NULL)
	{
        literal *l = &literalPool[i];
        
        // add more here ......
        switch (l->litType)
        {
            case litUnknown:
            case litGeneric:
            case litScaledFixed:
                printf("%-9s %06o %012llo ", l->name, l->addr, l->value);
                break;
            case litSingle:
            case litDouble:
                printf("%-9s %06o %12Lg ", l->name, l->addr, l->f);
                break;
        }

        i++;
        if (i % 2 == 0)
            printf("\n");
	}
    if (i % 4)
        printf("\n");
}

void fillLiteralPool(word18 *addr)
{
    int i = 0;
    literal *l = NULL;
    while ((l = &literalPool[i++])->name != NULL)
    {
        // align double-precision literals to an even boundary....
        if (l->litType == litDouble)
            if (*addr % 2)
            {
                (*addr) += 1;
                if (verbose)
                    printf("Aligning double-precision literal %s to even boundary (%08o)\n", l->name, *addr);
            }
        
        l->addr = (*addr)++;
        if (l->litType == litDouble)
            *addr += 1;
        
        //printf("Adding literal ... %s\n", l->name);
    }

}

/**
 * write literap pool to output stream "oct" ...
 */
void writeLiteralPool(FILE *oct, word18 *addr)
{
    if (literalPool[0].name == NULL)
        return; // no literals
    
    word36 Ypair[2], lVal;
    
    word18 maxAddr = 0;
    int i = 0;
    literal *l = NULL;
    while ((l = &literalPool[i++])->name != NULL)
    {
        word18 litAddr = l->addr;
        switch (l->litType)
        {
            case litUnknown:
            case litGeneric:
                lVal = l->value;
                //fprintf(oct, "%6.6o xxxx %012llo lit %s\n", (*addr)++, lVal, l->name);
                fprintf(oct, "%6.6o xxxx %012llo lit %s (%s)\n", litAddr++, lVal, l->name, l->text ? l->text : "");
                break;
            case litScaledFixed:
                lVal = l->value;
                fprintf(oct, "%6.6o xxxx %012llo slit %s (%s)\n", litAddr++, lVal, l->name, l->text ? l->text : "");
                break;

            case litSingle:
                IEEElongdoubleToYPair(l->f, Ypair);
                //fprintf(oct, "%6.6o xxxx %012llo elit %s (%Lg)\n", (*addr)++, Ypair[0] & DMASK, l->name, l->f);
                fprintf(oct, "%6.6o xxxx %012llo elit %s (%Lg)\n", litAddr++, Ypair[0] & DMASK, l->name, l->f);
                break;
            case litDouble:
                IEEElongdoubleToYPair(l->f, Ypair);
                //fprintf(oct, "%6.6o xxxx %012llo dlit %s (%Lg)\n", (*addr)++, Ypair[0] & DMASK, l->name, l->f);
                //fprintf(oct, "%6.6o xxxx %012llo\n",               (*addr)++, Ypair[1] & DMASK);
                fprintf(oct, "%6.6o xxxx %012llo dlit %s (%Lg)\n", litAddr++, Ypair[0] & DMASK, l->name, l->f);
                fprintf(oct, "%6.6o xxxx %012llo\n",               litAddr++, Ypair[1] & DMASK);
                
                break;
        }
        maxAddr = max(maxAddr, litAddr);
    }
    *addr = maxAddr;
}

#if NOTUSED
/**
 *
 * Types of literals to support ...
 *  fixed-point numbers
 *  floating-point numbers
 *  hollerith literals
 *  ascii literals
 * Eventually:
 *  itp(), its(), vfd(), tally's()
 *  aci/bci/string literals
 *
 
 */
literal* doLiteral(char *args, word18 addr)
{
    /// a literal .....
    if (args[0] != '=')
        return NULL;
    
    literal *l = getliteral(addr);
    if (l)  // already defined ... as in pass2?
        return l;

    // nope. Lets define it
    
    char buff[2014], *arg1 = buff;        // a buffer to play with
    strcpy(buff, args);
    
    arg1 += 1;  // skip over ='s
  
    char *safe = arg1;  // save arg1 for possible later use
    
    // let's see what we have .....
    
    // just a generic numeric literal?
    char *end_ptr;
    word36 lVal = strtoll(arg1, &end_ptr, 0);
    if (*end_ptr == '\0')
    {
        // whole literal accepted by strtoll();
        
        l = addliteral(addr, -1, litGeneric, args); // literal addresses will be filled in after pass1
        l->value = lVal & DMASK;                    // keep to 36-bits
        
        return l;
    }
    
    // an octal literal
    if (arg1[0] == 'O' || arg1[0] == 'o')
    {
        arg1 += 1;
        
        /**
         * The word is stored in its real form and is not complemented if a minus sign is present. The sign applies to bit 0 only.
         */
        word36s v64 = strtoll(arg1, NULL, 8);
        bool sign = v64 < 0;
        if (sign)
        {
            v64 = labs(v64);
            v64 |= SIGN36;
        }
        
        l = addliteral(addr, -1, litGeneric, args);   // literal addresses will be filled in after pass1
        l->value = v64 & DMASK;                // keep to 36-bits

        return l;
    }
    
    
    // first, let' see if we can extract a size specification from before whatever it is ...
    // NB: size will be invalid for things like fixed-point literals, etc, But correct for Hollerith & asciii, etc.
    
    int size = 0;
    if (isdigit(arg1[0]))   // 1st char *is* a decimal digit
    {
        size = (int)strtoll(arg1, &end_ptr, 10);    // size spec will be a base-10 number (I think)
        arg1 = end_ptr;
    }
    
    /// An alphanumeric literal consists of the letters H or nH (Hollerith), A or nA (ASCII), where n is a character count, followed by the data. If no count is specified, a literal of exactly six 6-bit characters or four 9-bit characters, including blanks, is assumed to follow the letter H or A. If a count exists, the n characters following the letter H or A are to be used as the literal. If the value n is not a multiple of six or four, the last partial word will be left-justified and space-filled. The value n may range from 1 through 53. (Embedded blanks do not terminate scanning of the fields by the assembler.)
    
    // an ascii literal
    // XXX the literal (=1a,) wont work because of parseLine()!!! :-) come up with a fix if it becomes a problem.
    
    if (arg1[0] == 'a' || arg1[0] == 'A')
    {
        arg1 += 1;
        if (size == 0)
            size = 4;
        
        if (size < 0 || size > 4)
        {
            fprintf(stderr, "WARNING: line %d : alphanumeric literal character count out-of-range (%d). Setting to default 4.\n", lineno, size);
            size = 4;
        }
       
        
        // we have a situation where "=a" can mean "=a    " but because of parseline() the trailing ' 's get chopped off.
        // so pad stuff with ' ' when appropriate
        
        word36 val = 0;
        for(int n = 0 ; n < 4 ; n ++)
            if (n < size && *arg1)
                val = (val << 9) | *arg1++;
            else
                val = (val << 9) | ' ';
        
        l = addliteral(addr, -1, litGeneric, args);   // literal addresses will be filled in after pass1
        l->value = val & DMASK;    // keep to 36-bits
     
        return l;
    }
    // a Alphanumeric (Hollerith) literal?
    // XXX the literal (=1h,) wont work because of parseLine()!!! :-) come up with a fix if it becomes a problem.
    
    if (arg1[0] == 'h' || arg1[0] == 'H')
    {
        arg1 += 1;
        if (size == 0)
            size = 6;
        
        if (size < 0 || size > 6)
        {
            fprintf(stderr, "WARNING: line %d : alphanumeric literal character count out-of-range (%d). Setting to default 6.\n", lineno, size);
            size = 6;
        }
        
        // we have a situation where "=h" can mean "=h      " but because of parseline() the trailing ' 's get chopped off.
        // so pad stuff with ' ' when appropriate
        
        word36 val = 0;
        for(int n = 0 ; n < 6 ; n ++)
            if (n < size && *arg1)
                val = (val << 6) | ASCIIToGEBcd[*arg1++];
            else
                val = (val << 6) | ASCIIToGEBcd[' '];
        
        l = addliteral(addr, -1, litGeneric, args);   // literal addresses will be filled in after pass1
        l->value = val & DMASK;    // keep to 36-bits
        
        return l;
    }

    /// Fixed-Point
    /// A fixed-point number possesses the same characteristics as the floating-point number with one exception: it must have a third part present. This is the binary scale factor denoted by the letter B followed by a signed or unsigned integer. The binary point is initially assumed at the left-hand part of the word between bit positions 0 and 1. It is then adjusted by the specified binary scale factor with plus implying a shift to the right and with minus, a shift to the left. Double-precision fixed-point follows the rules of double-precision floating-point with the addition of the binary scale factor. AF52-1 GMAP Course hase some good examples .....
    // A binary scale factor beginning with "b" indicates fixed point and forces conversion from floating point. The binary point in a literal with a binary scale factor is positioned to the right of the bit indicated by a decimal integer following the "b". (AK92-2. pg.6-12)
    
    if (strpbrk(arg1, "Bb") && !strpbrk(arg1, ".DdEe"))  // a fixed-point constant?
    {
        /*
          of the format MbN
         
         lda     =5b17               " 000005 000000
         
         
         lda     =22.5b5             " 264000 000000
 (ok)    22.5 = 012550000000
                01255       s
                000 001 01  0 101 101
                   E=5        10 110 1--   (34-5 => 29*'0' 36-len of mantissa)
                010110.10000000
         
         lda     =1.2e1b32           " 000000 000140
         
(ok)     000 000 000 000 000 000 000 000 000 001 100 . 000 (DP to the R of bit 32)

         12. =  010600000000
         12. =  0106
                000 001 00 | 0 | 11
                   E=4     | 0 | 110 000 0     (34-32 => 2*0's = 1
         
         lda     =1.95d1b37          " 000000 000004
                                     " 700000 000000
         
         dec     1.95d1     012470000000 000000000000
         
                            012470000000 000000000000
                            000 001 01 | 0 100 111 000 ...
                                E=5
         
         0                                             3 3                                             7
                                                       5 6                                             1
         000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000 000
(ok)                                                 100 11.1 (DP to the R of Bit 37)
         
         
         
         mpy     =.3012056b-10       " 232157 173225
                                     " 010 011 010 001 101 111 001 111 011 010 010 101
         " 776464336366     dec     .3012056
         111 111 110 100 110 100 011 011 110 011 110 110
         111 111 11 0 | 100 110 100 011 011 110 011 110 110
                    0   100 110 100 011 011 110 011 110 110 ... 10 010 101 (232157 173225)
         
         
         mpy     =.301205b-1         " 232157 052606
                                     " 010 011 010 001 101 111 000 101 010 110 000 110
         
         " 776464336125     dec     .301205
         111 111 110 100 110 100 011 011 110 001 010 101
         111 111 11 | 0 100 110 100 011 011 110 001 010 101
                      0 100 110 100 011 011 110 001 010 101 ... 100 00 110 (232157 052606)
         
         */
        char *end_ptr;
        word36 nVal = strtoll(arg1+1, &end_ptr, 10);
        if (end_ptr != arg1+1)
        {
            // of the form MbN
            word36 mVal = size;
            
            // right number of bits = 35-N
            word36 MbN = mVal << (35-nVal);
            
            l = addliteral(addr, -1, litScaledFixed, args);   // literal addresses will be filled in after pass1
            l->value = MbN;
            return l;
        }
        else
            fprintf(stderr, "doLiteral(): Error is evaluating scaled literal '%s'\n", safe);
    }

    arg1 = safe;    // we no longer need or want a size spec. So, restore pointer ...
    

    if (strchr(arg1, '.') || strpbrk(arg1, "EeDd"))
    {
        char cExpType = 'e';    ///< a regular Ee exponent specifier
        
        /// this *is* a float. Except if it's not (e.g. in a string.....)
        char *e = NULL;
        if ((e = strpbrk(arg1, "Dd")))
        {
            // a "D" or 'd' specifier (double)
            cExpType = tolower(*e);
            *e = 'e';   // change it to an 'e' so strtold() will recognize it ...
        }
        
        char *end_ptr;
        long double fltVal = strtold(arg1, &end_ptr);
        if (end_ptr != arg1)
        {
            l = addliteral(addr, -1, cExpType == 'd' ? litDouble : litSingle, args);   // literal addresses will be filled in after pass1
            l->f = fltVal;
            return l;
        }
    }
    
    lVal = strtoll(arg1, NULL, 0);
    
    l = addliteral(addr, -1, litGeneric, args);   // literal addresses will be filled in after pass1
    l->value = lVal & DMASK;                // keep to 36-bits
    
    return l;
}
#endif

/**
 * get 18-bit version of argument for DU/DL literals ....
 */
word18 get18(char *arg, int dudl)
{
    char *safe = strdup(arg);  ///< save arg1 for possible later use
    
    if (arg[0] == '=')
        arg += 1;   // skip =

    char *end_ptr;
    word36 lVal = strtoll(arg, &end_ptr, 0);
    if (*end_ptr == '\0')
        // whole literal accepted by strtoll();
        return lVal & AMASK;
    
    // an octal literal
    if (arg[0] == 'O' || arg[0] == 'o')
    {
        arg += 1;
        
        /**
         * The word is stored in its real form and is not complemented if a minus sign is present. The sign applies to bit 0 only.
         */
///< the "real" value makes no sense in this (18-bit) context.
//        word36s v64 = strtoll(arg, NULL, 8);
//        bool sign = v64 < 0;
//        if (sign)
//        {
//            v64 = labs(v64);
//            v64 |= SIGN36;
//        }

        return strtoll(arg, NULL, 8) & AMASK;   // this can only be an octal number
    }
    
    /// first, let' see if we can extract a size specification from before whatever it is ...
    /// NB: size will be invalid for things like fixed-point literals, etc, But correct for Hollerith & asciii, etc.

    int size = 0;
    if (isdigit(arg[0]))   // 1st char *is* a decimal digit
    {
        size = (int)strtoll(arg, &end_ptr, 10);    // size spec will be a base-10 number (I think)
        arg = end_ptr;
    }
    
    /// an ascii literal
    /// XXX the literal (=1a,) wont work because of parseLine()!!! :-) come up with a fix if it becomes a problem.
    
    if (arg[0] == 'a' || arg[0] == 'A')
    {
        arg += 1;
        if (size == 0)
            size = 2;
        
        if (size > 2)
        {
            fprintf(stderr, "WARNING: ASCII DU/DL literal (%s) >2 Bytes. Truncating.\n", safe);
            size = 2;
        }
        
        word18 val = 0;

        switch (size)
        {
            case 1:
                val = *arg << 9;
                break;
            case 2:
            {
                int c1 = *arg++;
                int c2 = 0;
                if (*arg)
                {
                    c2 = *arg++;
                    val = c1 << 9 | c2;
                }
                else
                {
                    val = c1 << 9;
                }
                break;
            }
        }

        //for(int n = 0 ; n < size ; n ++)
        //    if (n < size && *arg)
        //        val = (val << 9) | *arg++;
        //    else
        //        val = (val << 9) | ' ';

        //for(int n = 0 ; n < size ; n ++)
        //    val = (val << 9) | *arg++;
        
        return val & AMASK;   // this can only be an octal number
    }

    /// XXX the literal (=1h ) wont work because of parseLine()!!! :-) come up with a fix if it becomes a problem.
    if (arg[0] == 'h' || arg[0] == 'H')
    {
        arg += 1;
        if (size == 0)
            size = 3;
        
        if (size > 3)
        {
            fprintf(stderr, "WARNING: Hollerith/BCD DU/DL literal (%s) >3 Chars. Truncating.\n", safe);
            size = 3;
        }
        
        word18 val = 0;
        //for(int n = 0 ; n < size ; n ++)
        //    val = (val << 6) | (ASCIIToGEBcd[*arg++] & 077);
    
        //for(int n = 0 ; n < size ; n ++)
        //    if (n < size && *arg)
        //        val = (val << 6) | ASCIIToGEBcd[*arg++];
        //    else
        //        val = (val << 6) | ASCIIToGEBcd[' '];
        switch (size)
        {
            case 1:
                val = ASCIIToGEBcd[*arg++] << 12;
                break;
                
            case 2:
            {
                int c1 = ASCIIToGEBcd[*arg++];
                int c2 = ASCIIToGEBcd[*arg++];
                
                val = (c1 << 12) | (c2 << 6);
                break;
            }

            case 3:
            {
                int c1 = ASCIIToGEBcd[*arg++];
                int c2 = ASCIIToGEBcd[*arg++];
                int c3 = ASCIIToGEBcd[*arg++];
                
                val = (c1 << 12) | (c2 << 6) | c3;
                break;
            }
        }
    
        return val & AMASK;   // this can only be an octal number
    }

    /// A decimal integer is a signed or unsigned string of digits. It is differentiated from the other decimal types by the absence of a decimal point, the letter B, the letter E, the letter D (, the letter X, or the letters XD. - X ignored for now)
    if (!strpbrk(arg, ".BbEeDd"))
        return strtoll(arg, NULL, 0) & AMASK;   // this can be a octal, decimal or hex number

    
    if (strchr(arg, '.') || strpbrk(arg, "EeDd"))   // a float?
    {
        char cExpType = 'e';    ///< a regular Ee exponent specifier
        
        /// this *is* a float. Except if it's not (e.g. in a string.....)
        char *e = NULL;
        if ((e = strpbrk(arg, "Dd")))
        {
            // a "D" or 'd' specifier (double)
            cExpType = tolower(*e);
            *e = 'e';   // change it to an 'e' so strtold() will recognize it ...
        }
        
        char *end_ptr;
        long double fltVal = strtold(arg, &end_ptr);
        if (end_ptr != arg)
        {
            word36 YPair[2];
            IEEElongdoubleToYPair(fltVal, YPair);
            
            return (YPair[0] >> 18) & AMASK;    // return 18-bit half-precision float
        }
    }
    if (strpbrk(arg, "Bb"))   // a scaled fixed-point?
    {
        
//        wamontgomery@ieee.org (http://home.comcast.net/~wamontgomery)
//
//         Most of my assembly experience with the Honeywell/GE line was at Dartmouth, where we used a different assembler with different literal formats, but I think this is the same notation PL/1 used, which I always found a bit confusing.  The way I thought of it was that NbM specifies N with 35-M zero bits to the right.  (If N isn't an integer but has a decimal point, 35-M is where the integer part starts, and the conversion of whatever is after the decimal point begins there.)  What makes it a bit tricky in these instructions is that du and dl modification take 18 bit values, and apparently alm uses the lower half of the value specified (which is why "M" is bigger than 17 in all these examples, otherwise the whole bottom half would be zeros.
//         
//         So, for example, 63b23 has 35-23 or 12 zero bits (four 0 octal digits), and then 63 converts to 77 in octal.
//         7b25 has 10 zero bits, or 3 octal digits plus one more.  That means the "7" will start one bit left.  Shift 7 in octal left and you get 16.  18b25 converts similarly (18 in octal is 22, shift left 1 and you get 44
//         
//         Etc.
//         
//         Like I said, it gets more complicated if you specify fractional values this way
        
         
//         
//         Hi Harry,
//         Well based upon the octal data generated to the right of the instruction codes, I think it works as follows.
//         
//         ·         Consider the instruction operand as holding a bin(35) value: a signed, binary value stored in 36 bits.
//         o   Bits are numbered from left, with bit position 0 holding the sign in our bin(35) value.
//         o   So =1b35  is a constant placing a decimal 1 in the right-most bit of our bin(35) value:   000000000001 in octal
//         §  But instructions only have room for an 18-bit unsigned operand, so only the right-most 6 octal digits are used:  000001
//         §  That means the scale factor will range from:  19 to 35
//         ·         So =1b32 is:  000010 (in octal); and that is what is show in the octal data generated by the instruction
//         ·         =7b25 puts an decimal 7 followed by 10 binary digit positions of 0; in binary:  000 001 110 000 000 000  = 016000 in octal
//         ·         =2b25 puts an decimal 2 followed by 10 binary digit positions of 0; in binary:  000 000 100 000 000 000  = 004000 in octal
//         ·         =18b25 puts a decimal 18 (octal: 22) followed by 10 binary 0s:                               000 100 100 000 000 000  = 044000 in octal
//         ·         =63b23 puts a decimal 63 (octal: 77) followed by 12 binary 0s:                               111 111 000 000 000 000  = 770000 in octal
//         
//         
//         Gary
//         gary_dixon@q.com
//         
        
        char *end_ptr;
        word36 nVal = strtoll(arg+1, &end_ptr, 10);
        if (nVal <= 0)
        {
            fprintf(stderr, "WARNING: ilegal literal N value - %d @ line %d\n", (int)nVal, lineno);
            return 0;
        }
        if (end_ptr != arg)
        {
            // of the form MbN
            word36 mVal = size;
            
            // right number of bits = 35-N
            word36 MbN = mVal << (35-nVal);
            
            return GETLO(MbN);  
        }
        
    }

    lVal = strtoll(arg, NULL, 0);
    return lVal & AMASK;
}

/**
 * external linkage section ...
 * (like symbol table stuff it's rather primitive, but functional)
 */
//void initlinkPool()
//{
//    for(int i = 0; i < sizeof(linkPool) / sizeof(linkPair); i++)
//        linkPool[i].segname = NULL;
//}

// get literal associated with this source address
//linkPair* getlinkPair(char *extname)
//{
//    for(int i = 0; i < sizeof(linkPool) / sizeof(linkPair); i++)
//    {
//        if (linkPool[i].segname == NULL)
//            return NULL;
//        if (strcmp(linkPool[i].extname, extname) == 0)
//            return &linkPool[i];
//    }
//    return NULL;
//}

//linkPair *addlink(char *segname, char *extname)
//{
//    linkPair *l = getlinkPair(extname);
//    if (l != NULL)
//        return NULL;
//    int i = 0;
//    while (linkPool[i].segname != NULL && i < sizeof(literalPool) / sizeof(struct literal))
//        i++;
//    if (i >= sizeof(linkPool) / sizeof(linkPair))
//        return NULL;
//    
//    linkPool[i].segname = strdup(segname);
//    linkPool[i].extname = strdup(extname);
//    linkPool[i].addr = -1;
//    
//    return &linkPool[i];
//}

extern int linkCount;
extern word18 linkAddr;

void dumpextRef()
{
    if (linkCount == 0)
        return;
    
    printf("======== External References ========\n");
    
    int i = 0;
    symtab *s = Symtab;
    
    while (s->name)
	{
        if (s->segname && s->extname)
        {
        
        printf("%-10s %-10s %08o  ", s->segname, s->extname, s->value);
        
        i++;
        if (i % 2 == 0)
            printf("\n");
        }
        s += 1;
	}
    if (i % 4)
        printf("\n");
}

/*
 * fill-in external segrefs before beginning pass 2
 */
void fillExtRef(word18 *addr)
{
    if (!linkCount)
        return;
    
    symtab *s = Symtab;
    
    if ((*addr) % 2)    // linkage (ITS) pairs must be on an even boundary
        *addr += 1;
    
    linkAddr = *addr;    // offset of linkage section
    
    int n = 0;
    while (s->name)
    {
        if (s->segname && s->extname)
        {
            s->value = (s->value + *addr) & AMASK;
            *addr += 2;
        }
        s += 1;
    }
}

/**
 * write literap pool to output stream "oct" ...
 */
void writeExtRef(FILE *oct, word18 *addr)
{
    word18 maxAddr = 0;

    int i = 0;
    symtab *s = Symtab;

    if (linkCount)
    {
        if ((*addr) % 2)    // ITS pairs must be on an even boundary
            *addr += 1;
    
        if (linkAddr && *addr != linkAddr)
            fprintf(stderr, "writeExtRef(): Phase error for linkage section %06o != %06o\n", *addr, linkAddr);
    }
    else return;

    while (s->name)
    {
        if (s->segname && s->extname)
        {

            word18 lnkAddr = s->value;
            
            if (*addr != s->value)
                fprintf(stderr, "writeextRef(): Phase error for %s/%s\n", s->segname, s->extname);
            
            int segno = 0;  // filled in by loader
            int offset = 0; // filled in by loader
            word36 even = ((word36)segno << 18) | 046;  // unsnapped link
            word36 odd = (word36)(offset << 18);    // no modifications (yet)| (arg3 ? getmod(arg3) : 0);
        
            char desc[256];
            sprintf(desc, "link %s|%s", s->segname, s->extname);
        
            fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, even, desc);
            fprintf(oct, "%6.6o xxxx %012llo\n", (*addr)++, odd);

            maxAddr = max(maxAddr, lnkAddr);
        }
        s += 1;
    }
    *addr = maxAddr;
}

/**
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
    
    //fprintf(stderr,"sign=%d f0=%Lf mant=%Lf exp=%d\n", sign, f0, mant, exp);
    
    word72 result = 0;
    
    // now let's examine the mantissa and assign bits as necessary...
    
    long double bitval = 0.5;    ///< start at 1/2 and go down .....
    for(int n = 62 ; n >= 0 && mant > 0; n -= 1)
    {
        if (mant >= bitval)
        {
            result = bitfieldInsert72(result, 1, n, 1);
            mant -= bitval;
            //fprintf(stderr, "Inserting a bit @ %d %012llo %012llo\n", n , (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
        }
        bitval /= 2.0;
    }
    //fprintf(stderr, "n=%d mant=%f\n", n, mant);
    
    //fprintf(stderr, "result=%012llo %012llo\n", (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
    // if f is < 0 then take 2-comp of result ...
    if (sign)
    {
        result = -result & (((word72)1 << 64) - 1);
        //fprintf(stderr, "-result=%012llo %012llo\n", (word36)((result >> 36) & DMASK), (word36)(result & DMASK));
    }
    //! insert exponent ...
    int e = (int)exp;
    result = bitfieldInsert72(result, e & 0377, 64, 8);    ///< & 0777777777777LL;
    
    // XXX TODO test for exp under/overflow ...
    
    return result;
}

void IEEElongdoubleToYPair(long double f0, word36 *Ypair)
{
    float72 r = IEEElongdoubleToFloat72(f0);
    
    Ypair[0] = (r >> 36) & DMASK;
    Ypair[1] = r & DMASK;
}

void IEEElongdoubleToYPairOLD(long double f0, word36 *Ypair)
{
    if (f0 == 0)
    {
        Ypair[0] = 0400000000000LL;
        Ypair[1] = 0;
        return;
    }
    
    bool sign = f0 < 0 ? true : false;
    long double f = fabsl(f0);
    
    int exp;
    long double mant = frexpl(f, &exp);
    
    if (exp > 127)
    {
        fprintf(stderr, "WARNING: exponent overflow (%Lg)\n", f0);
        exp = 127;
    }
    else if (exp < -128)
    {
        fprintf(stderr, "WARNING: exponent underflow (%Lg)\n", f0);
        exp = -128;
    }
    
    uint64 result = 0;
    
    /// now let's examine the mantissa and assign bits as necessary...
    
    long double bitval = 0.5;    ///< start at 1/2 and go down .....
    for(int n = 62 ; n >= 0 && mant > 0; n -= 1)
    {
        if (mant >= bitval)
        {
            //result = bitfieldInsert72(result, 1, n, 1);
            result |= ((word36)1 << n);
            
            mant -= bitval;
        }
        bitval /= 2.0;
    }

    // if f is < 0 then take 2-comp of result ...
    if (sign)
        result = -result;

    // insert exponent ...
    Ypair[0] = (exp & 0377) << 28;
    // and mantissa
    Ypair[0] |= (result >> 36) & 01777777777LL;
    Ypair[1] = (result << 8) & 0777777777400LL;
}

/*
 * Stuff for include file support ...
 */
#define MAXINCL	64					///< maximum include nesting
#define FILEtos	FILEstack[FILEsp]	///< we have a stack of FILE*'s

IOstack FILEstack[MAXINCL];
int  FILEsp = -1;		/*!< (FILE*) stack pointer		*/

extern char *srcFile;

char *
LEXCurrentFilename()
{
    
	if (FILEsp < 0)
		return "<No Current File>";
    
	return FILEtos.fname;
}

int
LEXgetc(void)
{
	static int first_call = true;
    
	char c;
    
	if (first_call) {		// on 1st call do special processing
		first_call = false;	/* no longer the 1st call	*/
		FILEsp = -1;			// reset file stack pointer
		LEXgetPush(inFile);	// push configuration file onto stack
        
        if (debug)
            fprintf(stderr, "Reading source code from %s...", inFile);
	}
    
	do {
		// read file on top of stack (if possible)
		if (FILEsp < 0)
			c = EOF;					// nothing left to read
		else
			c = fgetc(FILEtos.fp);		/* get character from TOS	*/
        
		if (c == EOF) { /* no more left. Close file	*/
			if (LEXgetPop() == -1)
            {
                first_call = true;  // reset
				return EOF;
			}
            else
				continue;
		} else
			return c;
	} while (1);
    
}

char *
LEXfgets(char *line, int nChars)
{
    /// XXX modify LEXfgets to generate lineno/include specs (e.g. 1-1, 52-153, 1, 4) ala alm) 6-digits wide
	static int first_call = true;
    
	if (first_call) {		// on 1st call do special processing
		first_call = false;	/* no longer the 1st call	*/
		FILEsp = -1;			// reset file stack pointer
		LEXgetPush(inFile);	// push configuration file onto stack
        
        if (debug)
            fprintf(stderr, "Reading source code from %s...\n", inFile);
	}
    
	do {
        char *c = 0;
		// read file on top of stack (if possible)
		if (FILEsp < 0)
			c = NULL;					// nothing left to read
		else
			c = fgets(line, nChars, FILEtos.fp);		/* get line from TOS	*/
        
		if (c == NULL) { /* no more left. Close file	*/
			if (LEXgetPop() == -1)
            {
                first_call = true;  // reset
				return NULL;
			}
			else
				continue;
		} else
			return c;
	} while (1);
    
}

int
LEXgetPop(void)
{
	if (FILEsp < 0)
		return -1;
    
	if (FILEtos.fp) {			/* close file if not NULL	*/
		fclose(FILEtos.fp);
		FILEtos.fp = NULL;
	}
    
	free (FILEtos.fname);	// no longer needed
	FILEtos.fname = NULL;
    
	FILEsp--;
    
	lineno = FILEtos.lineno;	// restore lineno
    
	return FILEsp;
}


/**
 * try to open an include file...
 */
void
LEXgetPush(char *fileToOpen)
{
    char filename[1024];
    strcpy(filename, fileToOpen);
    
	FILE *isrc;
    
	if (FILEsp >= MAXINCL - 1) {
		fprintf(stderr, "ERROR: Too many nested include's! Sorry.\n");
		return;
	}
    
	if (filename == NULL) {
		fprintf(stderr, "LEXgetPush(\"%s\"): filename == NULL!\n", filename);
		return;
	}
    
	if (filename[0] == '"')	// && filename[strlen(filename)-1] == '"')
		stripquotes(filename);

    // try to open file as specified, dir where src file is and along include path ...

	isrc = fopen(filename, "r");		/* try to open requested file	*/

    if (isrc)
        goto onOpen;
    
    /// see if srcFile has a path associated with it. If so, try it ...
    char *p = strrchr(srcFile, '/');
    if (p)
    {
        // extract directory component of src file and try to open file there ...
        memset(filename, 0, sizeof(filename));
        int n = (int)(p - srcFile) + 1;
        strncpy(filename, srcFile, n);
        
        strcat(filename, fileToOpen);
        
        if (debug) printf("Trying ... %d <%s>\n", n, filename);
        
        isrc = fopen(filename, "r");		/* try to open requested file	*/
        
        if (isrc)
            goto onOpen;
    }

    char *path;
    char *ip = strdup(includePath);
    while ((path = Strsep(&ip, ";")) != NULL)
    {
        if (strlen(strchop(path)) == 0)    // empty path, just ignore
            continue;
        
        strcpy(filename, path);
        strcat(filename, "/");  // start with path name
        strcat(filename, fileToOpen);
        strchop(filename);      // remove any leading/trailing whitespace
        
        if (debug)
            fprintf(stderr, "LEXgetPush(): Trying ... <%s>\n", filename);
        
        isrc = fopen(filename, "r");
        if (isrc)
            break;  // success
    }
    if (isrc == NULL)
    {
		fprintf(stderr, "WARNING: Unable to open file \"%s\". Sorry!\n", filename);
		exit(5);
	}
 
onOpen:;
    
	/** .include file is open, so set up to use it ... */
	FILEstack[++FILEsp].fp = isrc;		/* make it TOS	*/
	FILEtos.lineno = lineno;			// save lineno
	FILEtos.fname = strdup(filename);	// save filename
	lineno = 0;                         // reset lineno for new file
}



char *strrev(char *str) /// in-place strrev()
{
    char *p1, *p2;
    
    if (! str || ! *str)
        return str;
    for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
    {
        *p1 ^= *p2;
        *p2 ^= *p1;
        *p1 ^= *p2;
    }
    return str;
}

char *
stripquotes(char *s)
{
	char *p;
    
	while ((p = strchr(s, '"')))
		*p = ' ';
	strchop(s);
    
	return s;
}
/**
 * Removes the trailing spaces from a string.
 */
char *strclip(char *s)
{
	int index;
    
	for (index = strlen(s) - 1; index >= 0 &&
         (s[index] == ' ' || s[index] == '\t'); index--) {
		s[index] = '\0';
	}
	return(s);
}
/** ------------------------------------------------------------------------- */
char *strpreclip(char *s)
/**
 *	Removes the leading spaces from a string.
 */
{
	char *p;
	if (s == NULL)
		return NULL;
    
	for (p = s; (*p == ' ' || *p == '\t') && *p != '\0'; p++)
		;
    
	strcpy(s, p);
	return(s);
}

/** ------------------------------------------------------------------------- */

char *strchop(char *s)
{
	return strclip(strpreclip(s));
}

/** ------------------------------------------------------------------------- */

/*  state definitions  */
#define	STAR	0
#define	NOTSTAR	1
#define	RESET	2

int strmask(char *str, char *mask)
/**
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


/** -------------------------------------------------------------------------- */

char *strwrap(char *text, int *row, int width)
/**
 Takes a string and copies it into a new string.
 Lines are wrapped around if they are longer than width.
 returns a pointer to the space allocated for the output.
 or NULL if out of memory.
 
 'text'	the string to word wrap
 'row'	the number of rows of text in out
 'width'	the width of the text rows
 
 Wrap algorithm:
 Start at beginning of the string.
 Check each character and increment a counter.
 If char is a space remember the location as a valid break point
 If the char is a '\n' end the line and start a new line
 If WIDTH is reached end the line at the last valid break point
 and start a new line.
 If the valid break point is at the beginning of the line
 hyphenate and continue.
 */
{
	char	*string;			/*!< the output string */
	char	*line;				/*!< start of current line */
	char   	*brk; 				/*!< most recent break in the text */
	char   	*t, *s;
	size_t  len;
	bool	done = false;
    
	*row = 0;
    
	/* allocate string space; assume the worst */
	len = strlen(text);
    
	len = (len > 0x7fff) ? 0xffff : len * 2 + 1;	// stupid PC's...
    
    //	if ((string = (char *) malloc(len)) == NULL)
	if ((string = malloc(len)) == NULL)
		return(NULL);
    
	if (*text == '\0' || width < 2) {
		strcpy(string, text);
		return(string);
	}
    
	*string = '\0';
	line = string;
    
	for (t = text; !done; ) {
		for(brk = s = line; (s - line) < width; t++, s++) {
			*s = *t;
			if (*t == '\n' || *t == '\0') {
				brk = s;
				break;
			} else if (*t == ' ') {
				brk = s;
			}
		}
        
		if (brk == line && *t != '\n' && *t != '\0') {
			/* one long word... */
			s--;
			t--;
			*s = '\n';
			*(s + 1) = '\0';
			line = s + 1;
		} else if (*t == '\n') {
			*s = '\n';
			*(s + 1) = '\0';
			t++;
			if (*t == '\0') {
				done = true;
			} else {
				line = s + 1;
			}
		}
		else if (*t == '\0') {
			*s = '\0';
			done = true;
		} else {
			/* chop off last word */
			t = t - (s - brk) + 1;
			*brk = '\n';
			*(brk + 1) = '\0';
			line = brk + 1;
		}
        
		(*row)++;
	}
    
	return(string);
}
/** ------------------------------------------------------------------------- */

/**
 * strtok() with string quoting...
 * (implemented as a small fsm, kinda...
 * (add support for embedded " later, much later...)
 */
#define NORMAL 		1
#define IN_STRING	2
#define EOB			3

char *
Strtok(char *line, char *sep)
{
    
	static char *p;		/*!< current pointer position in input line	*/
	static int state = NORMAL;
    
	char *q;			/*!< beginning of current field			*/
    
	if (line) {			/* 1st invocatio						*/
		p = line;
		state = NORMAL;
	}
    
	q = p;
	while (state != EOB) {
		switch (state) {
			case NORMAL:
				switch (*p) {
					case 0:				///< at end of buffer
						state = EOB;	// set state to "end Of Buffer
                        return q;
                        
                    case '"':		///< beginning of a quoted string
                        state = IN_STRING;	// we're in a string
                        p++;
                        continue;
                        
                    default:    ///< only a few special characters
                        if (strchr(sep, *p) == NULL) {	// not a sep
                            p++;				// goto next char
                            continue;
                        } else {
                            *p++ = (char)NULL;	/* ... iff >0	*/
                            while (*p && strchr(sep, *p))	/* skip over seperator(s)*/
                                p++;
                            return q;	/* return field		*/
                        }
                }
                
			case IN_STRING:
                if (*p == 0) {		  /*!< incomplete quoted string	*/
                    state = EOB;
                    return q;
                }
                
                if (*p != '"') { // not end of line and still in a string
                    p++;
                    continue;
                }
                state = NORMAL;			/* end of quoted string	*/
                p++;
                
                continue;
                
			case EOB:					/*!< just in case	*/
				state = NORMAL;
				return NULL;
                
			default:
				fprintf(stderr, "(Strtok):unknown state - %d",state);
				state = EOB;
				return NULL;
        }
        
	}
    
	return NULL;		/* no more fields in buffer		*/
    
}


//char *Strdup(char *s, int size)
//{
//	if (s == NULL) {
//		fprintf(stderr, "(Strdup):s == NULL");
//		return 0;
//	}
//    
//	char *d = malloc(size <= 0 ? strlen(s)+1 : size);
//	if (d)
//		strcpy(d, s);
//	else
//		fprintf(stderr, "(Strdup):d == NULL:<%s>", s);
//	return d;
//}

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

/**
 * Expand C style escape sequences ...
 * added octal/hex escapes 21 Nov 96, HWR
 * replaced sscanf with strtol 25 Nov 12, HWR
 */
char *
strexp(char *d, char *s)
{
    char *r = d;
	long val;
	char *end_ptr;
    
    do {
h:      switch (*s) {
            case '\\' : /*!< An escape sequence */
                s++;
                switch (*s) {
                    case '0':	///< an octal or hex
                    case '1':	///< a decimal digit
                    case '2':	///< a decimal digit
                    case '3':	///< a decimal digit
                    case '4':	///< a decimal digit
                    case '5':	///< a decimal digit
                    case '6':	///< a decimal digit
                    case '7':	///< a decimal digit
                    case '8':	///< a decimal digit
                    case '9':	///< a decimal digit
                        val = strtoll(s, &end_ptr, 0); // allows for octal, decimal and hex
                        if (end_ptr == s)
                            fprintf(stderr, "strexp(%s): strtoll conversion error", s);
                        else
                            s = end_ptr;
                        *d++ = val & 0xff;
                        goto h;
                    case 'a' :  /*!< A bell       */
                        *d++ = '\a';
                        break;
                    case 'b' :  /*!< Backspace    */
                        *d++ = '\b';
                        break;
                    case 'f' :  /*!< A Form feed  */
                        *d++ = '\f';
                        break;
                    case 'n' :  /*!< a nl <CR><LF> */
                        //		       *d++ = '\r';    
                        *d++ = '\n';
                        break;
                    case 'r' :  /*!< A Carriage return    */
                        *d++ = '\r';
                        break;
                    case 't' : /*!< A tab         */
                        *d++ = '\t';
                        break;
                    case 'v' :
                        *d++ = '\v';
                        break;
                    case '\\' :
                        *d++ = '\\';
                        break;
                    case '"':
                        *d++ = '"';
                        break;
                    case '\'':
                        *d++ = '\'';
                        break;
                }
                break;
            default : *d++ = *s;
        }
    } while (*s++);
    return (r);
}

/** ------------------------------------------------------------------------- */
/** copy a string while predicate function returns non-0                      */

int strcpyWhile(char *dst, char *src, int (*f)(int))
{
    int n = 0;
    *dst = '\0';
    while (*src && f(*src))
    {
        *dst++ = *src++;
        *dst = '\0';
        n++;
    }
    return n;
}


char *Strsep(char **stringp, const char *delim)
{
    char *s = *stringp;
    char *e;
    
    if (!s)
        return NULL;
    
    e = strpbrk(s, delim);
    if (e)
        *e++ = '\0';
    
    *stringp = e;
    return s;
}

/**
 a - Bitfield to insert bits into.
 b - Bit pattern to insert.
 c - Bit offset number.
 d = Number of bits to insert.
 
 Description
 
 Returns the result of inserting bits B at offset C of length D in the bitfield A.
 
 NB: This would've been much easier to use of I changed, 'c', the bit offset' to reflect the dps8s 36bit word!! Oh, well.
 */
word36 bitfieldInsert36(word36 a, word36 b, int c, int d)
{
    word36 mask = ~(0xffffffffffffffffLL << d) << c;
    mask = ~mask;
    a &= mask;
    return (a | (b << c)) & DMASK;
}

word36 bitMask36(int length)
{
    return ~(0xffffffffffffffffLL << length);
}

/*!
 a -  Bitfield to extract bits from.
 b -  Bit offset number. Bit offsets start at 0.
 c - Number of bits to extract.
 
 Description
 
 Returns bits from offset b of length c in the bitfield a.
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

struct PRtab {
    char *alias;    ///< pr alias
    int   n;        ///< number alias represents ....
} _prtab[] = {
    
    {"pr0", 0}, ///< pr0 - 7
    {"pr1", 1},
    {"pr2", 2},
    {"pr3", 3},
    {"pr4", 4},
    {"pr5", 5},
    {"pr6", 6},
    {"pr7", 7},
    
    // from: ftp://ftp.stratus.com/vos/multics/pg/mvm.html
    {"ap",  0},
    {"ab",  1},
    {"bp",  2},
    {"bb",  3},
    {"lp",  4},
    {"lb",  5},
    {"sp",  6},
    {"sb",  7},
    
    {0,     0}
    
};

int getPRn(char *s)
{
    for(int n = 0 ; _prtab[n].alias; n++)
        if (strcasecmp(_prtab[n].alias, s) == 0)
            return _prtab[n].n;
    
    return -1;
}

