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

