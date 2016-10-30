/*
 Copyright 2012-2013 by Harry Reed

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later. 
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file asMisc.c
 * \project as8
 * \author Harry Reed
 * \date 12/29/12
 *  Created by Harry Reed on 12/29/12.
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "as.h"

/// a strcpy for overlapping strings (if needed)
char* strcpyO(char *a, char *b)
{
    if (a == NULL || b == NULL)
    {
        return NULL;
    }
    
    memmove(a, b, strlen(b) + 1);
    return a;
}

/**
 * some code to handle multi-word EIS parsing, etc ...
 */

struct adrMods mfReg[] = {    /// address modifiers w/ extended info for EIS MFk
    {"n",  0},
    {"au", 1},
    {"qu", 2},
    {"du", 3},  ///< The du modifier is permitted only in the second operand descriptor of the scd, scdr, scm, and scmr
                ///< instructions to specify that the test character(s) reside(s) in bits 0-18 of the operand descriptor.
    {"ic", 4},
    {"al", 5},    {"a", 5},     // some time asame encoding
    {"ql", 6},
    
    {"x0",  8},    //{"0",  8},
    {"x1",  9},    //{"1",  9},
    {"x2", 10},    //{"2", 10},
    {"x3", 11},    //{"3", 11},
    {"x4", 12},    //{"4", 12},
    {"x5", 13},    //{"5", 13},
    {"x6", 14},    //{"6", 14},
    {"x7", 15},    //{"7", 15}
};

int findMfReg(char *reg)
{
    //if (strlen(reg) > 0 && !strcasecmp(reg, "du"))
    //    printf("%s\n", reg);
    for(int n = 0 ; n < sizeof(mfReg) / sizeof(mfReg[0]); n += 1)
        if (!strcasecmp(reg, mfReg[n].mod))
            return mfReg[n].Td;
    return -1;
}

int decodeMFk(char *arg)
{
    char *tmp = strdup(arg);

    int retVal = 0;
    
    char *u;
    while ((u = strpbrk(tmp, "()")))    // remove any leading, trailing ()'s
        *u = ' ';

    char *token;
    while ((token = Strsep(&tmp, ",")) != NULL)
    {
        char reg[256];
        strcpy(reg, token);
        strchop(reg);
        if (strlen(reg) == 0)
            continue;
        
        if (!strcasecmp(reg, "ar"))  // an AR present?
            retVal |= MFkAR;    //0x40;
        if (!strcasecmp(reg, "pr"))  // an PR present?
            retVal |= MFkPR;    //0x40;
        
        if (!strcasecmp(reg, "rl"))  // an RL present?
            retVal |= MFkRL;    //0x20;

        if (!strcasecmp(reg, "id"))  // an id present?
            retVal |= MFkID;    //0x10;

        int result = findMfReg(reg);
        if (result != -1)
            retVal |= result;
        
        // XXX doest it always have to be a MFreg? can it ever be a number?
        
        //for(int n = 0 ; n < sizeof(mfReg) / sizeof(mfReg[0]); n += 1)
        //    if (!strcasecmp(reg, mfReg[n].mod))
        //        retVal |= mfReg[n].Td;
    }
    
    free(tmp);
    
    return retVal;
}



#define MAXMFk 8  ///< we should never have this many, but it's good for error checking

int  MFk[MAXMFk];
bool bEnableFault = false;
bool bRound = false;
bool bAscii = false;
bool bP = false;    ///< for btd instruction
bool bT = false;    ///< for Truncation fault enable bit

int boolValue = 0;
int fillValue = 0;
int maskValue = 0;


//#if DEPRECIATED
char *getParenthesizedGroup(char *src, char *dst, char **end_ptr)
{
    char *p = src;
    char *q = dst;
    
    // look for 1st '('
    while (*p && *p != '(')
        p += 1;
    
    if (*p == 0)    // end of string and no '('
        return 0;
    
    int pCount = 0; // start counting () groups.
    while (*p)
    {
        if (*p == '(')
            pCount += 1;
        else if (*p == ')')
            pCount -= 1;
        
        if (pCount == 0)
            break;
        
        *q++ = *p++;
        *q = 0;
    }
    
    if (end_ptr)
        *end_ptr = p;
    
    if (pCount)     // malformed/unmatched group
        return 0;
    
    return strcpyO(dst, dst + 1);    // XXX this *should* be portable
}





/**
 
 "csr",  "sztr",  "cmpb", 
 "scd",  "scdr",    "scmr",   "tct",
 "tctr", "ad2d",  "sb2d", "mp2d", "dv2d",  "ad3d",  "sb3d",  "mp3d",
 "dv3d", "mvn",   "btd",  "cmpn", "dtb"
 
 
 scm, mlr, cmpc, mvne, sztl
 
 "mve",  "mvne",  "csl",  "csr",  "sztl",  "sztr",  "cmpb",  "mlr",
 "mrl",  "cmpc",  "scd",  "scdr", "scm",   "scmr",  "mvt",   "tct",
 "tctr", "ad2d",  "sb2d", "mp2d", "dv2d",  "ad3d",  "sb3d",  "mp3d",
 "dv3d", "mvn",   "btd",  "cmpn", "dtb"
 
 2  "csl",   "csr",  "sztl", "sztr", "cmpb", "mlr",  "mrl",  "cmpc"
    "ad2d",  "sb2d", "mp2d", "dv2d", "mvn",  "btd",  "cmpn", "dtb"

 3  "mve",  "mvne", "scd",  "scdr", "scm",  "scmr", "mvt",  "tct"
    "tctr", "ad3d", "sb3d", "mp3d", "dv3d"

 */

word36 parseMwEISGood(opCode *op, char *args)
{
    // XXX refactor to only extract as many () groups as are present not how many are "required"
    // XXX because ndes can be deceiving
    
    char MFkstr[MAXMFk][256];

    int params = op->ndes; ///< number of operand descriptors needed/desired/would like. 
    
    for(int n = 0 ; n < MAXMFk; n += 1)
        {
            MFkstr[n][0] = 0;
            MFk[n] = -1;
        }
    
    bEnableFault= false;
    bRound = false;
    bAscii = false;
    boolValue = 0;
    fillValue = 0;
    maskValue = 0;
    bT = false;
    bP = false;
    
    char *p = strdup(args);
    
    // remove any nasty comments.....
    if (strchr(p, '\"'))
    {
        *strchr(p, '\"') = '\0';
        strchop(p);
    }
//    if (strlen(p) == 0)
//    {
//        free(p);
//        return 0;
//    }
    
    char *pSafe = p;
    
    for(int n = 0 ; n < params ; n += 1)
    {
        char buff[256];
        memset(buff, 0, sizeof(buff));
        
        char *end_ptr;
        char *g = getParenthesizedGroup(p, buff, &end_ptr);
        if (g)
            p = end_ptr;
        else
        {
            fprintf(stderr, "parseEIS(): malformed () group <%s>\n", p);
            //free (pSafe);
            //return 0;
        }
        strcpy(MFkstr[n], buff);
        MFk[n] = decodeMFk(MFkstr[n]);
        
        //printf("%d <%s> = 0%o (%d)\n", n+1, MFkstr[n], MFk[n], MFk[n]);
    }
    
    char *token;
    while ((token = Strsep(&p, ",")) != NULL)
    {
        /// if there are () in the rest of the string, remove them because they cause problems
        char *u;
        while ((u = strpbrk(token, "()")))
            *u = ' ';
        
        /// look for "fill"
        char *f = strcasestr(token, "fill");
        if (f)
            fillValue = (int)strtol(f + 4, NULL, 8);    // extract an octal #
        /// look for "bool"
        char *b = strcasestr(token, "bool");
        if (b)
            boolValue = (int)strtol(b + 4, NULL, 8);    // extract an octal #
        /// look for "mask"
        char *m = strcasestr(token, "mask");
        if (m)
            maskValue = (int)strtol(m + 4, NULL, 8);    // extract an octal #
        
        // look for "enablefault" keyword
        if (strcasestr(token,"enablefault"))
            bEnableFault = true;
 
        // look for "round" keyword
        if (strcasestr(token,"round"))
            bRound = true;
 
        // look for "ascii" keyword
        if (strcasestr(token,"ascii"))
            bAscii = true;
        
        
        // P & T have not been tested. Probably needs a bit of work. Sorry..... :-)
        // look for "P" keyword
        if (!strcasecmp(token,"p"))
            bP = true;
        
        // look for "T" keyword
        if (!strcasecmp(token,"t"))
            bT = true;
    }
    
    word36 i = 0;   ///< 1st instruction word
    // should we do error checking here or not? Add later if needed
    
    // insert MFk's
    // XXX assume source code is correct ????
    i = MFk[0];     // MF1
    if (params > 0 && MFk[1] >= 0)
    {
        i = bitfieldInsert36(i, MFk[1], 18, 7);
        if (params > 1 && MFk[2] >= 0)
            i = bitfieldInsert36(i, MFk[2], 27, 7);
    }
    
    // insert opcode
    i = bitfieldInsert36(i, op->opcode << 1 | 1, 8, 9);
    
    // The rest of the field placements although consistent seem to vary somewhat between instructions
    // fill(0|1) vs fill(000) vary fill9 is at bit, while fill1 is at bit 0
    // So, let's go through each Multiword EIS individually ... and fill stuff in
    
    // NB: this *is* the place to do error checking should we want to really bother. Perhaps, later...
    switch(op->opcode)
    {
        case 0060:  ///< "csl (2)"
        case 0061:  ///< "csr (2)"
        case 0064:  ///< "sztl (2)"
        case 0065:  ///< "sztr (2)"

            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (boolValue)      // BOLR Boolean result control field
                i = bitfieldInsert36(i, boolValue & 017, 27, 4);
            if (fillValue)      // F Fill bit for string extension
                i |= (1LL << 35);
            break;
            
        case 0066:  ///< "cmpb (2)"
            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (fillValue)            // F Fill bit for string extension
                i |= (1LL << 35);
            break;
            
        case 0100:  ///< "mlr (2)"
        case 0101:  ///< "mrl (2)"
        case 0160:  ///< "mvt (3)"
            
            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (fillValue)      // Fill character for string extension
                i = bitfieldInsert36(i, fillValue & 0777, 27, 9);
            break;
 
        case 0106:  ///< "cmpc (2)"
            if (fillValue)      // Fill character for string extension
                i = bitfieldInsert36(i, fillValue & 0777, 27, 9);
            break;

        case 0120:  ///< "scd (3)"
        case 0121:  ///< "scdr (3)"
        case 0164:  ///< "tct (3)"
        case 0165:  ///< "tctr (3)"
        case 0020:  ///< "mve (3)"
        case 0303:  ///< "cmpn (3)"
        case 0024:  ///< "mvne (3)"
        case 0305:  ///< "dtb (3)"

            /// nothing more really to do here
            break;
                        
        case 0202:  ///< "ad2d (2)"
        case 0222:  ///< "ad3d (3)"
        case 0203:  ///< "sb2d (2)"
        case 0223:  ///< "sb3d (3)"
        case 0206:  ///< "mp2d (2)"
        case 0226:  ///< "mp3d (3)"
        case 0207:  ///< "dv2d (2)"
        case 0227:  ///< "dv3d (3)"
        case 0300:  ///< "mvn (3)"

            if (bP)     //|| bAscii)       // XXX I think this is what 'ascii' represents
                i |= (1LL << 35);     // P bit
            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (bRound)         // R Rounding flag
                i |= (1LL << 25);
            break;
            
        case 0124:  ///< "scm (3)"
        case 0125:  ///< "scmr (3)"
            if (maskValue)      // MASK Comparison bit mask
                i = bitfieldInsert36(i, maskValue & 0777, 27, 9);
            break;

        case 0301:  ///< "btd (3)"
            if (bP)
                i |= (1LL << 35);    // P bit
            break;
    }
    
//    printf("fill = 0%o (%d)\n", fillValue, fillValue);
//    printf("bool = 0%o (%d)\n", boolValue, boolValue);
//    printf("mask = 0%o (%d)\n", maskValue, maskValue);
//    printf("enablefault = %s\n", bEnableFault ? "True" : "False");
//    printf("round = %s\n", bRound ? "True" : "False");
//    printf("ascii = %s\n", bAscii ? "True" : "False");
    
    free(pSafe);
    
    return i & DMASK;
}
//#endif


char *getParenthesizedGroups(char *src, char *dst, char **end_ptr)
{
    //fprintf(stderr, "src=%s\n", src);
    if (*src == 0)
        return 0;
    
    char *p = src;
    char *q = dst;
    
    
    if (*p == 0)    // end of string and no '('
        return 0;
    
    // look for 1st '('
    //while (*p && *p != '(')
    //    p += 1;
    // if 1st non white space char is not a ( then were done ....
    //while (*p && isspace(*p))
    //       p += 1;
    
    if (*p != '(')  // no more () groups left
    {
        if (end_ptr)
            *end_ptr = p;   // save last position
        return 0;
    }
    *q++ = *p++;
    *q = 0;
    
    int pCount = 1; // start counting () groups.
    while (*p)
    {
        if (*p == '(')
            pCount += 1;
        else if (*p == ')')
            pCount -= 1;
        
        *q++ = *p++;
        *q = 0;
        
        if (pCount == 0)
        {
            //if (*p)
            //    p += 1; // skip closing )
            break;
        }
    }

    if (pCount)     // malformed/unmatched group
        return 0;

    if (end_ptr)
        *end_ptr = p;   // save last position
        
    return src;
}

word36 parseMwEIS(opCode *op, char *args)
{
    // XXX refactor to only extract as many () groups as are present not how many are "required"
    // XXX because ndes can be deceiving
    
    //fprintf(stderr, "pMWEIS():%s\n", args);
    
    
    char MFkstr[MAXMFk][256];
    
    int params = op->ndes; ///< number of operand descriptors needed/desired/would like.
    
    for(int n = 0 ; n < MAXMFk; n += 1)
    {
        memset(MFkstr[n], 0, sizeof(MFkstr[n]));
        MFk[n] = 0;
    }
    
    bEnableFault= false;
    bRound = false;
    bAscii = false;
    boolValue = 0;
    fillValue = 0;
    maskValue = 0;
    bP = false;
    bT = false;
    
    
    char buff[1024];
    strcpy(buff, args);
    
    char *p = buff; //strdup(args);
    
    // remove any nasty comments.....
    if (strchr(p, '\"'))
    {
        *strchr(p, '\"') = '\0';
        strchop(p);
    }
    
    //char *pSafe = p;
    
    char *end_ptr;
    //char *g = 0;
    
    int nPg = 0;
    
    char pgrp[MAXMFk][256];
    memset(pgrp, 0, sizeof(pgrp));  // set everything to all 0's
    
    
    while (getParenthesizedGroups(p, pgrp[nPg], &end_ptr))   // extract all () goups
    //while (getParenthesizedGroups(p, MFkstr[nPg], &end_ptr))   // extract all () goups
    {
        // w let's skip any trailing white space 
        p = end_ptr;
        while (*p && (isspace(*p) || *p == ','))
            p++;
        
        if (*p == 0)    // end of line?
            break;
  
        nPg += 1;
    }
    
    
    p = end_ptr;

    //fprintf(stderr, "end_ptr = %s\n", p);

    for(int n = 0 ; n < params; n += 1)
    {
        
        strcpy(MFkstr[n], pgrp[n]);// +  1, strlen(pgrp[nPg])-2);
        MFk[n] = decodeMFk(MFkstr[n]);

        //fprintf(stderr, "p[%d]='%s' MF[%d]=%o\n", n, pgrp[n], n, MFk[n]);

    }
    
    //        strncpy(MFkstr[n], buff);
    //       p = end_ptr;
    
    
    //else
    //{
    //   fprintf(stderr, "parseEIS(): malformed () group <%s>\n", p);
    //    //free (pSafe);
    //    //return 0;
    //å}
    
    //
    // at this point we should only have optiona like mask(xx), fill, enablefault, etc.
    
    
//    for(int n = 0 ; n < params ; n += 1)
//    {
//        char buff[256];
//        memset(buff, 0, sizeof(buff));
//        
//        char *end_ptr;
//        char *g = getParenthesizedGroup(p, buff, &end_ptr);
//        if (g)
//            p = end_ptr;
//        else
//        {
//            fprintf(stderr, "parseEIS(): malformed () group <%s>\n", p);
//            //free (pSafe);
//            //return 0;
//        }
//        strcpy(MFkstr[n], buff);
//        MFk[n] = decodeMFk(MFkstr[n]);
//        
//        //printf("%d <%s> = 0%o (%d)\n", n+1, MFkstr[n], MFk[n], MFk[n]);
//    }
    
    char *token;
    while ((token = Strsep(&p, ",")) != NULL)
    {
        token = strchop(token);
        if (strlen(token) == 0)
            continue;
        
        //fprintf(stderr, "token = %s\n", token);
        
        /// look for "fill"
        char *f = strcasestr(token, "fill");
        if (f)
            fillValue = (int)strtol(strchr(f, '(') + 1, NULL, 8);    // extract an octal #
        /// look for "bool"
        char *b = strcasestr(token, "bool");
        if (b)
            boolValue = (int)strtol(strchr(b, '(') + 1, NULL, 8);    // extract an octal #
        /// look for "mask"
        char *m = strcasestr(token, "mask");
        if (m)
            maskValue = (int)strtol(strchr(m, '(') + 1, NULL, 8);    // extract an octal #
        
        // look for "enablefault" keyword
        // IS thes the same a T?
        if (strcasestr(token,"enablefault"))
            bEnableFault = true;
        
        // look for "round" keyword
        if (strcasestr(token,"round"))
            bRound = true;
        
        // look for "ascii" keyword
        if (strcasestr(token,"ascii"))
            bAscii = true;
        
        
        // P & T have not been tested. Probably needs a bit of work. Sorry..... :-)
        // look for "P" keyword
        if (!strcasecmp(token,"p"))
            bP = true;
        
        // look for "T" keyword
        if (!strcasecmp(token,"t"))
            bT = true;
    }
    
    word36 i = 0;   ///< 1st instruction word
    // should we do error checking here or not? Add later if needed
    
    // insert MFk's
    // XXX assume source code is correct ????
    i = MFk[0];     // MF1
    if (params > 0 && MFk[1] >= 0)
    {
        i = bitfieldInsert36(i, MFk[1], 18, 7);
        if (params > 1 && MFk[2] >= 0)
            i = bitfieldInsert36(i, MFk[2], 27, 7);
    }
    
    // insert opcode
    i = bitfieldInsert36(i, op->opcode << 1 | 1, 8, 9);
    
    // The rest of the field placements although consistent seem to vary somewhat between instructions
    // fill(0|1) vs fill(000) vary fill9 is at bit, while fill1 is at bit 0
    // So, let's go through each Multiword EIS individually ... and fill stuff in
    
    // NB: this *is* the place to do error checking should we want to really bother. Perhaps, later...
    switch(op->opcode)
    {
        case 0060:  ///< "csl (2)"
        case 0061:  ///< "csr (2)"
        case 0064:  ///< "sztl (2)"
        case 0065:  ///< "sztr (2)"
            
            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (boolValue)      // BOLR Boolean result control field
                i = bitfieldInsert36(i, boolValue & 017, 27, 4);
            if (fillValue)      // F Fill bit for string extension
                i |= (1LL << 35);
            break;
            
        case 0066:  ///< "cmpb (2)"
            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (fillValue)      // F Fill bit for string extension
                i |= (1LL << 35);
            break;
            
        case 0100:  ///< "mlr (2)"
        case 0101:  ///< "mrl (2)"
        case 0160:  ///< "mvt (3)"
            
            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (fillValue)      // Fill character for string extension
                i = bitfieldInsert36(i, fillValue & 0777, 27, 9);
            break;
            
        case 0106:  ///< "cmpc (2)"
            if (fillValue)      // Fill character for string extension
                i = bitfieldInsert36(i, fillValue & 0777, 27, 9);
            break;
            
        case 0120:  ///< "scd (3)"
        case 0121:  ///< "scdr (3)"
        case 0164:  ///< "tct (3)"
        case 0165:  ///< "tctr (3)"
        case 0020:  ///< "mve (3)"
        case 0303:  ///< "cmpn (3)"
        case 0024:  ///< "mvne (3)"
        case 0305:  ///< "dtb (3)"
            
            /// nothing more really to do here
            break;
            
        case 0202:  ///< "ad2d (2)"
        case 0222:  ///< "ad3d (3)"
        case 0203:  ///< "sb2d (2)"
        case 0223:  ///< "sb3d (3)"
        case 0206:  ///< "mp2d (2)"
        case 0226:  ///< "mp3d (3)"
        case 0207:  ///< "dv2d (2)"
        case 0227:  ///< "dv3d (3)"
        case 0300:  ///< "mvn (3)"
            
            if (bP || bAscii)
                i |= (1LL << 35);    // P bit
            if (bEnableFault || bT)   // T Truncation fault enable bit
                i |= (1LL << 26);
            if (bRound)         // R Rounding flag
                i |= (1LL << 25);
            break;
            
        case 0124:  ///< "scm (3)"
        case 0125:  ///< "scmr (3)"
            if (maskValue)      // MASK Comparison bit mask
                i = bitfieldInsert36(i, maskValue & 0777, 27, 9);
            break;
            
        case 0301:  ///< "btd (3)"
            if (bP || bAscii)
                i |= (1LL << 35);    // P bit
            break;
    }
    
    //    printf("fill = 0%o (%d)\n", fillValue, fillValue);
    //    printf("bool = 0%o (%d)\n", boolValue, boolValue);
    //    printf("mask = 0%o (%d)\n", maskValue, maskValue);
    //    printf("enablefault = %s\n", bEnableFault ? "True" : "False");
    //    printf("round = %s\n", bRound ? "True" : "False");
    //    printf("ascii = %s\n", bAscii ? "True" : "False");
    
    //free(pSafe);
    
    return i & DMASK;
}

/// how many occurences of char 'c' occur in the string ....
int howMany(char *src, int c)
{
    int n = 0;
    char *p = src;
    
    while ((p = strchr(p, c)))
    {
        p += 1;
        n += 1;
    }
    
    return n;
}

/// remove white characters from a string ....
char *
removeWhite(char *src)
{
    char *p = src;
    while ((p = strpbrk(p, " \t")))
        strcpy(p, p + 1);
    
    return src;
}

word36 doDescn(char *arg1, char *arg2, int n)
{
    /// printf("arg1=<%s> arg2=<%s>\n", arg1, arg2);
    
    /// arg1 format: address
    ///              address(offset)
    
    /// "address,length"
    /// "address(offset),length"
    /// "(address expression),length"
    /// "(address expression)(offset),length"
    
    word18 a = 0, b = 0;
    
    int nLP = howMany(arg1, '(');   ///< how many ( are there?
    int nRP = howMany(arg1, ')');   ///< how many ) are there?
    //printf("nLP=%d nRP=%d\n", nLP, nRP);
    
    if (nRP == 0 && nRP == 0)   // no () to be found in arg1
    {
        //fprintf(stderr, "1 <%s>\n", arg1);
        // XXX allow for PR| syntax here .....
        if (strchr(arg1, '|'))
            a = encodePR(arg1, NULL);
        else
            a = Eval(arg1) & AMASK;

        //a = Eval(arg1) & AMASK;
        b = 0;  // no offset
    }
    else if (nRP != nLP)        // an invalid, imbalanced address specifier
    {
        fprintf(stderr, "doDescn(): syntax error in 'address' specifier - imbalanced ()'s\n");
        return 0;
    }
    else
    {
        char offset[64];
        char address[64];
        memset(address, 0, sizeof(address));
        memset(offset,  0, sizeof(offset));
        
        ///  scan arg1 looking for 0 or 1 parenthesis groups.....
        
        char *p = arg1;
        char *q = address;
        
        if (*arg1 == '(') // address begins with a ( .....
        {
            /// scan/copy until we have final matching (). The next char had better be a (
            int pCount = 0; ///< start counting () groups.
            while (*p)
            {
                if (*p == '(')
                    pCount += 1;
                else if (*p == ')')
                    pCount -= 1;
            
                *q++ = *p++;
                *q = 0;
                
                if (pCount == 0)
                    break;
            }
            //fprintf(stderr, "2 address <%s>\n", address);

            strcpy(offset, p);
            //fprintf(stderr, "2 offset <%s>\n", offset);
        }
        else
        {
            // address does not begin with a (, so extract everything up until the offset (
            while (*p != '(')
            {
                *q++ = *p++;
                *q = 0;
            }
            //fprintf(stderr, "3 address <%s>\n", address);
            strcpy(offset, p);
            
            //fprintf(stderr, "3 offset <%s>\n", offset);
        }
        
        // XXX allow for PR| syntax here .....
        if (strchr(address, '|'))
            a = encodePR(address, NULL);
        else
            a = Eval(address) & AMASK;
        
        /// remove () from offset
        char *u;
        while ((u = strpbrk(offset, "()")))
            *u = ' ';
        strchop(offset);
        
        if (strlen(offset))
        {
            int cn = (int)strtol(offset, NULL, 0);
            if (cn < 0 || cn > 7)
            {
                fprintf(stderr, "doDescn(): CN out-of-range (%d)\n", cn);
                return 0;
            }
            if (n == 9)     // 9-bit position is shifted over 1 (AL39 Tbl 4.2)
                cn <<= 1;
            
            b = (cn & 07) << 15;
        }
    }
    
    // now to length specifier - finally!
    
    if (arg2)
    {
        char length[64];
        memset(length, 0, 64);
        strcpy(length, arg2);
        
        strchop(length);
        
        /// is it a register designation?
        int n = findMfReg(length);
        if (n != -1)
            b |= n & 017;
        else
        {
            b |= (int)Eval(length) & 07777;
        }
    } else
        b = 0;
    
    return ((word36)a << 18) | b;
}

word36 doDescBs(char *arg1, char *arg2)
{
    /// XXX: not certain if this is correct. Dunno what to do with the B field cf AL39 Fig 4-8.
    /// XXX: fix when it becomes clear what to do.
    // 
    
    word18 a = 0, b = 0;
    
    int nLP = howMany(arg1, '(');   ///< how many ( are there?
    int nRP = howMany(arg1, ')');   ///< how many ) are there?
    
    if (nRP == 0 && nRP == 0)   // no () to be found in arg1
    {
        // XXX allow for PR| syntax here .....
        if (strchr(arg1, '|'))
            a = encodePR(arg1, NULL);
        else
            a = Eval(arg1) & AMASK;
        b = 0;  // no offset
    }
    else if (nRP != nLP)        // an invalid, imbalanced address specifier
    {
        fprintf(stderr, "doDescb(): syntax error in 'address' specifier - imbalanced ()'s\n");
        return 0;
    }
    else
    {
        char offset[64];
        char address[64];
        memset(address, 0, sizeof(address));
        memset(offset,  0, sizeof(offset));
        
        /// scan arg1 looking for 0 or 1 parenthesis groups.....
        
        char *p = arg1;
        char *q = address;
        
        if (*arg1 == '(') // address begins with a ( .....
        {
            /// scan/copy until we have final matching (). THe next char had better be a (
            int pCount = 0; ///< start counting () groups.
            while (*p)
            {
                if (*p == '(')
                    pCount += 1;
                else if (*p == ')')
                    pCount -= 1;
                
                *q++ = *p++;
                *q = 0;
                
                if (pCount == 0)
                    break;
            }
            strcpy(offset, p);
        }
        else
        {
            // address does not begin with a (, so extract everything up until the offset (
            while (*p != '(')
            {
                *q++ = *p++;
                *q = 0;
            }
            strcpy(offset, p);
        }
        
        // XXX allow for PR| syntax here .....
        if (strchr(address, '|'))
            a = encodePR(address, NULL);
        else
            a = Eval(address) & AMASK;
        
        /// remove () from offset
        char *u;
        while ((u = strpbrk(offset, "()")))
            *u = ' ';
        strchop(offset);
        
        if (strlen(offset))
        {
            int c = (int)strtol(offset, NULL, 0);
            if (c < 0 || c > 35)
            {
                fprintf(stderr, "doDescb(): C out-of-range (%d)\n", c);
                return 0;
            }
            
            int charPos = c / 9;
            int bitPos = c % 9;
            
            b = (charPos << 16) | (bitPos << 12);
        }
    }
    
    // now to length specifier - finally!
    // NB: length specifier has been extended to encompass the B field.
    if (arg2)
    {
        char length[64];
        memset(length, 0, 64);
        strcpy(length, arg2);
        
        strchop(length);
        
        /// is it a register designation?
        int n = findMfReg(length);
        if (n != -1)
            b |= n & 017;
        else
        {
            b |= (int)Eval(length) & 07777; // 12-bits,
        }
    }
    return (word36)a << 18 | b;
}

word18 encodePR(char *arg, bool *bit29)
{
    /// is arg1 a PRn|xxx ???
    char *v = strchr(arg, '|');
    if (v)  // a pointer register
    {
        char buff[128];
        memset(buff, 0, sizeof(buff));
        strcpy(buff, arg);
        
        char *arg1 = buff;
        
        // format prX|offset
        v = strchr(arg1, '|');
        *v = ' ';   /// remove | make it a space so we can use sscanf() on it
        char pr[128] = "", off[128] = "";
        sscanf(arg1, "%s %s", pr, off);  // extract PR and offset
        
        /// pr0-7
        word36 n = getPRn(pr); ///< predefined pr?
        
        if (n == -1)
        {
            // it may be a symbol, or a digit
            n = Eval(pr);
            if (n > 7)
            {
                fprintf(stderr, "WARNING: encodePR(): invalid pointer register must be between 0 and 7\n");
                n = 0;  // make pr0
            }
        }
        
        word36 offset = Eval(off) & 077777;

        if (bit29)
            *bit29 = true;
        
        return ((word18)n << 15) | (word18)offset;
        
    } else
        return Eval(arg) & 0777777;

}

/**
 * do special interpass processing ....
 */

extern word18 addr;
extern word18 linkAddr;
extern int linkCount;

void doInterpass(FILE *out)
{
    // perform end of pass processing ...
    // fill in literals
    fillLiteralPool(&addr);
    
    // fill in ITS/link pairs
    
    fillExtRef(&addr);
    
    // write size of segment
    fprintf(out, "!SIZE %06o\n", addr);
    if (linkCount)
        fprintf(out, "!LINKAGE %06o %06o\n", linkAddr, linkCount);
    
    // emit any SEGMENT directive stuff
    emitSegment(out);
    
    // emit any !GO directive
    emitGo(out);
}

void doPostpass(FILE *out)
{
    writeLiteralPool(out, &addr);
    
    writeExtRef(out, &addr);
}
