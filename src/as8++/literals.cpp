/**
 * \file literals.c
 * \project as8
 * \author Harry Reed
 * \date 03/30/13
 *  Created by Harry Reed on 03/30/13.
 * \copyright Copyright (c) 2012-2013 Harry Reed. All rights reserved.
 */


#include "as.h"

extern int litPosInLine;

literal *litPool = NULL;

literal *doStringLiteral(int size, int kind, char *str)
{
    literal *l = getCurrentLiteral();
    if (l)  // as in pass 2
        return l;
    
    int strLength = (int)strlen(str);
    int nWords = 1; // at least 1 words
    
    char temp[256];
    
    switch (kind)
    {
        case 'a':
        case 'A':
        {
            sprintf(temp, "A literal (=%da%s)", strLength, str);
            l = addliteral(addr, -1, litString, temp);   // literal addresses will be filled in after pass1

            if (size == 0)
                size = 4;
            
            if (size > 4 * sizeof(l->values))
            {
                yyprintf("ASCII literal too large (max %d)\n", 4 * sizeof(l->values));
                return l;
            }
            
            if (strLength)
            {
                /// convert 8-bit chars to 9-bit bytes in 36-bit words. Oh, Joy!
                nWords = (strLength) / 4 + ((strLength) % 4 ? 1 : 0);
                l->nWords = nWords;
                
                memset(l->values, 0, sizeof(l->values));
                
                word36 *w = l->values;
                int nPos = 3;
                for(char *p = str; *p; p++)
                {
                    word36 q = *p & 0xff;   ///< keep to 8-bits because of sign-extension
                    word36 nShift = (9 * (nPos--));
                    
                    *w |= (q << nShift);
                    
                    if (nPos < 0) {
                        w++;        // next word
                        nPos = 3;   // 1st byte in word
                    }
                }
            }
        }
            break;
            
        case 'h':
        case 'H':
        {
            sprintf(temp, "H literal (=%dh%s)", strLength, str);
            l = addliteral(addr, -1, litString, temp);   // literal addresses will be filled in after pass1
            
            if (size == 0)
                size = 6;
            if (size > 6 * sizeof(l->values))
            {
                yyprintf("GEBCD/Hollerith literal too large (max %d)\n", 6 * sizeof(l->values));
                return l;
            }
            
            // XXXXXXX
            memset(l->values, 0, sizeof(l->values));
            
            int strLength = (int)strlen(str);
            if (strLength)
            {
                /// convert 8-bit chars to 6-bit GEBCD's packed into 36-bit words. Oh, Joy!
                int nWords = (strLength) / 6 + ((strLength) % 6 ? 1 : 0);
                l->nWords = nWords;
                
                word36 *w = l->values;
                int nPos = 5;
                for(char *p = str; *p; p++)
                {
                    int q = ASCIIToGEBcd[*p];
                    if (q == -1)
                        q = 0;  // illegal characters get mappes to 0
                    
                    word36 nShift = (6 * (nPos--));
                    
                    *w |= ((word36)q << nShift);
                    
                    if (nPos < 0) {
                        w++;        // next word
                        nPos = 5;   // 1st char in word
                    }
                }
                
                // pad rest with spaces ...
//                while (nPos >= 0)
//                {
//                    word36 nShift = (6 * (nPos--));
//                    
//                    *w |= (ASCIIToGEBcd[' '] << nShift);
//                }
            }
        }
            break;
            
        default:
        {
            yyprintf("doLiteral(): why are we here?");
        }
            break;
    }
    
    l->signature = makeLiteralSignature(l);
    return l;
}

literal *doNumericLiteral(int base, word36 val)
{
    literal *l = getCurrentLiteral();
    if (l)  // as is pass 2
        return l;

    char temp[256];
    switch (base)
    {
        case 8:
            sprintf(temp, "octal literal (=o%llo)", val & DMASK);
            break;
        case 10:
            sprintf(temp, "decimal literal (=%lld)", SIGNEXT36(val & DMASK));
            break;
    }
    
    l = addliteral(addr, -1, litGeneric, temp);   // literal addresses will be filled in after pass1
    l->values[0] = val & DMASK;    // keep to 36-bits
    
    l->signature = makeLiteralSignature(l);
    return l;
}
literal *doNumericLiteral72(word72 val)
{
    literal *l = getCurrentLiteral();
    if (l)  // as is pass 2
        return l;

    word36 values[2] = {0, 0};
    
    values[0] = (val >> 36) & DMASK;
    values[1] = val & DMASK;
    
    char temp[256];
    sprintf(temp, "72-bit, scaled fixed-point literal (%012llo %012llo)", values[0], values[1]);
    
    l = addliteral(addr, -1, litDoubleInt, temp);   // literal addresses will be filled in after pass1
    l->values[0] = values[0];
    l->values[1] = values[1];
    
    l->signature = makeLiteralSignature(l);
    return l;
}

literal *doFloatingLiteral(int sd, long double val)
{
    literal *l = getCurrentLiteral();
    if (l)  // as is pass 2
        return l;
    
    char temp[256];
    switch (sd)
    {
        case 1: // single-precision literal
            sprintf(temp, "single-precision literal (=%LF)", val);
            l = addliteral(addr, -1, litSingle, temp);   // literal addresses will be filled in after pass1
            break;
        case 2: // double-precision literal
            sprintf(temp, "double-precision literal (=%LF)", val);
            l = addliteral(addr, -1, litDouble, temp);   // literal addresses will be filled in after pass1
            break;
    }
    
    l->r = val; 
    
    l->signature = makeLiteralSignature(l);
    return l;
}

literal *doITSITPLiteral(int itsitp, word36 arg1, word36 off, word36 tag)
{
    literal *l = getCurrentLiteral();
    if (l)  // as is pass 2
        return l;
    
    word36 even = 0, odd = 0;
    char temp[256];
    
    if (itsitp == 043)
    {
        int segno = arg1 & 077777;
        even = ((word36)segno << 18) | 043;
        sprintf(temp, "its literal (=its(%05o,%06llo,%02llo))", segno, off & AMASK, tag & 077);
    }
    else if (itsitp == 041)
    {
        int prnum = arg1 & 03;
        even = ((word36)prnum << 33) | 041;
        sprintf(temp, "itp literal (=itp(%o,%06llo,%02llo))", prnum, off & AMASK, tag & 077);
    }
    
    int offset = (off & AMASK);
    odd = (word36)(offset << 18) | (tag & 077);
    
    l = addliteral(addr, -1, litITSITP, temp);   // literal addresses will be filled in after pass1
    
    l->values[0] = even;
    l->values[1] = odd;
    
    l->signature = makeLiteralSignature(l);
    return l;
}

literal *doVFDLiteral(tuple *vlist)
{
    word36 data[256];
    memset(data, 0, sizeof(data));  // make everything 0
    
    int nWords = _doVfd(vlist, data);
    if (nWords > 256)
    {
        yyerror ("VFD data too large. Max 256-words");
        nWords = 256;
    }

    literal *l = getCurrentLiteral();
    if (!l)
        l = addliteraln(addr, -1, litVFD, nWords,  "=v");   // literal addresses will be filled in after pass1
        
    for(int n = 0 ; n < nWords ; n += 1)
        l->values[n] = data[n];

    l->signature = makeLiteralSignature(l);
    return l;
}

/**
 * literal table stuff ...
 * (like symbol table stuff it's rather primitive, but functional)
 */
void initliteralPool()
{
    litPool = NULL;
}


// get literal associated with this source address
literal* getliteralByAddrAndPosn(word18 srcAddr, int seqNo)
{
    literalKey key;
    memset(&key, 0, sizeof(key));
    key.srcAddr = srcAddr;
    key.seqNo = seqNo;
    
    literal *l;
    //HASH_FIND_INT(litPool, &srcAddr, l);  /* srcAddr already in the hash? */
    HASH_FIND(hh, litPool, &key, sizeof(literalKey), l);

    return l;
}

literal* getliteralByName(char *name)
{
    for(literal *l = litPool; l != NULL; l = (literal*)l->hh.next)
        if (strcmp(l->name, name) == 0)
            return l;
    return NULL;
}

//literal* getliteralByAddrAndSeq(word18 addr, int seq)
//{
//    for(literal *l = litPool; l != NULL; l = l->hh.next)
//        if (l->key.srcAddr == addr && l->key.seqNo == seq)
//            return l;
//    return NULL;
//}

literal* getCurrentLiteral()
{    
    literalKey key;
    memset(&key, 0, sizeof(key));
    key.srcAddr = addr;
    key.seqNo = litPosInLine;
    
    literal *l;
    HASH_FIND(hh, litPool, &key, sizeof(literalKey), l);
    return l;
}

literal *getLiteralBySignature(char *signature)
{
    for(literal *l = litPool; l != NULL; l = (literal*)l->hh.next)
        if (strcmp(l->signature, signature) == 0)
            return l;
    return NULL;
}

list *newList();

list *getLiteralListBySignature(literal *sig)
{
    list *lst = NULL;
    for(literal *l = litPool; l != NULL; l = (literal*)l->hh.next)
        if (sig != l && strcmp(l->signature, sig->signature) == 0)
        {
            list *l2 = newList();
            
            l2->whatAmI = lstLiteral;
            l2->lit = l;
            
            DL_APPEND(lst, l2);
        }
    return lst;
}

int getLiteralSignatureCount(char *signature)
{
    int res = 0;
    for(literal *l = litPool; l != NULL; l = (literal*)l->hh.next)
        if (strcmp(l->signature, signature) == 0)
            res += 1;
    
    return res;
}

PRIVATE
literal *newLit()
{
    literal *l = (literal*)calloc(1, sizeof(literal));
    
    l->key.seqNo = litPosInLine; // set occurence in line
    
    return l;
}


literal *addliteraln(word18 srcAddr, word18 litAddr, eLiteralType litType, int nWords, char *text)
{
    if (nWords < 0)
        nWords = 1;     // most literals use 1 word
    
    char lName[256];
    switch (litType)
    {
        case litUnknown:
        case litGeneric:
            sprintf(lName, LITERAL_FMT, srcAddr, litPosInLine);
            break;
        case litSingle:
            sprintf(lName, FLITERAL_FMT, srcAddr, litPosInLine);
            break;
        case litDouble:
            sprintf(lName, DLITERAL_FMT, srcAddr, litPosInLine);
            nWords = 2;
            break;
        case litScaledFixed:
            sprintf(lName, SCFIXPNT_FMT, srcAddr, litPosInLine);
            break;
        case litDoubleInt:
            sprintf(lName, SCFIXPNT2_FMT, srcAddr, litPosInLine);
            break;
        case litString:
            sprintf(lName, STRLIT_FMT, srcAddr, litPosInLine);
            break;
        case litITSITP:
            sprintf(lName, ITLIT_FMT, srcAddr, litPosInLine);
            nWords = 2;
            break;
        case litVFD:
            sprintf(lName, VFDLIT_FMT, srcAddr, litPosInLine);
            break;
    }
        
    literal *l = getCurrentLiteral();
    if (l)
        return l;
    
    l = newLit();
    l->name = strdup(lName);
    l->key.srcAddr = srcAddr;       // where lit is referenced
    l->key.seqNo = litPosInLine;
    l->addr = litAddr;          // where literal lives in assembly
    l->litType = litType;
    l->nWords = nWords;              // usually takes up 1 word
    
    
    //printf("Literal added:addr=%p <%s>\n", l, l->name);
    if (text)
        l->text = strdup(text);
    else
        l->text = NULL;
    
    if (debug)
        fprintf(stderr, "Adding literal %s srcAddr %06o -- %s", lName, srcAddr, LEXline);

    
    // XXX needs to be changed since srcAddr is no longer unique ... this probably shouldn't work. By serendipitous luck, it does.
    //HASH_ADD_INT(litPool, srcAddr, l);
    HASH_ADD(hh, litPool, key, sizeof(literalKey), l);
    return l;
}

literal *addliteral(word18 srcAddr, word18 litAddr, eLiteralType litType, char *text)
{
    return addliteraln(srcAddr, litAddr, litType, -1, text);
}

void dumpliteralPool()
{
    printf("======== literal pool ========\n");
    
    int i = 0;
    literal *l, *tmp;
    HASH_ITER(hh, litPool, l, tmp)
	{        
        // add more here ......
        switch (l->litType)
        {
            case litUnknown:
            case litGeneric:
            case litScaledFixed:
                printf("%-9s %06o %012llo ", l->name, l->addr, l->values[0]);
                break;
            case litSingle:
            case litDouble:
                printf("%-9s %06o %12Lg ", l->name, l->addr, l->r);
                break;
            case litString:
                printf("%-9s %06o %012llo ", l->name, l->addr, l->values[0]);
                break;
            case litITSITP:
            case litDoubleInt:
                printf("%-9s %06o %012llo %012llo ", l->name, l->addr, l->values[0], l->values[1]);
                break;
            case litVFD:
                printf("%-9s %06o ", l->name, l->addr);
                for(int n = 0 ; n < l->nWords ; n += 1)
                    printf("%012llo ", l->values[n]);
                break;
            default:
                printf("dumpLiteralPool(): unhandled literal type =%d\n", l->litType);
                break;
        }
        
        i++;
        //if (i % 2 == 0)
            printf("\n");
	}

    //if (i % 2)
    //    printf("\n");
}

/*
 * scan literalPool looking for duplicate entries that can be consolidated/merged
 */
void checkLiteralPool()
{
    if (debug)
        printf("checking literal pool for duplicates ...\n");
    literal *l, *tmp;
    HASH_ITER(hh, litPool, l, tmp)
    {
        list *lst = getLiteralListBySignature(l);
        if (lst)
        {
            if (debug)
                printf("Found literals with same signature as <%s>(%s)\n", l->name, l->signature);

            list *el;
            DL_FOREACH(lst, el)
            {
                if (debug && l != el->lit)
                    printf("    <%s>(%s)\n", el->lit->name, el->lit->signature);
            }
        }
    }
}

/*
 * fill in litAddress of literal so it can be referenced in pass == 2
 */
void fillLiteralPool()
{
    literal *l, *tmp;
    HASH_ITER(hh, litPool, l, tmp)
    {
        // align double-precision literals to an even boundary....
        if (l->litType == litDouble || l->litType == litITSITP || l->litType == litDoubleInt)
            if (addr % 2)
            {
                addr += 1;
                if (verbose)
                    fprintf(stderr, "Aligning double-precision literal %s to even boundary (%08o)\n", l->name, addr);
            }
        
        l->addr = addr;
        addr += l->nWords;
    }    
}

/**
 * write literal pool to output stream "oct" ...
 */
void writeLiteralPool()
{
    word36 Ypair[2], lVal;
    
    word18 maxAddr = 0;
    
    literal *l, *tmp;
    HASH_ITER(hh, litPool, l, tmp)
    {
        word18 litAddr = l->addr;
        switch (l->litType)
        {
            case litUnknown:
            case litGeneric:
                lVal = l->values[0];
                outas8(lVal, litAddr++, l->text);
                break;
            case litScaledFixed:
                lVal = l->values[0];
                outas8(lVal, litAddr++, l->text);
                break;
            case litSingle:
                IEEElongdoubleToYPair(l->r, Ypair);
                outas8(Ypair[0] & DMASK, litAddr++, l->text);
                break;
            case litDouble:
                IEEElongdoubleToYPair(l->r, Ypair);
                outas8(Ypair[0] & DMASK, litAddr++, l->text);
                outas8(Ypair[1] & DMASK, litAddr++, "");
                break;
            case litString:
            case litVFD:
                for(int i = 0 ; i < l->nWords ; i += 1)
                {
                    lVal = l->values[i] & DMASK;
                    outas8(lVal, litAddr++, !i ? l->text : NULL);
                }
                break;
            case litITSITP:
            case litDoubleInt:
                outas8(l->values[0], litAddr++, l->text);
                outas8(l->values[1], litAddr++, NULL);
                break;
        }
        maxAddr = max(maxAddr, litAddr);
    }
    addr = maxAddr;
}

char *makeLiteralSignature(literal *l)
{
    word36 Ypair[2];
    char temp[4096];
    
    char *sig = NULL;
    
    switch (l->litType)
    {
        case litUnknown:
        case litGeneric:
        case litScaledFixed:
        //case litScaledReal:
            asprintf(&sig, "%012llo", l->values[0]);
            break;
        case litSingle:
            IEEElongdoubleToYPair(l->r, Ypair);
            asprintf(&sig, "%012llo" ,Ypair[0] & DMASK);
            break;
        case litDouble:
            IEEElongdoubleToYPair(l->r, Ypair);
            asprintf(&sig, "%012llo %012llo", Ypair[0] & DMASK, Ypair[1] & DMASK);
            break;
        case litString:
        case litVFD:
            memset(temp, 0, sizeof(temp));
            for(int i = 0 ; i < l->nWords ; i += 1)
            {
                char t[32];
                sprintf(t, "%012llo ", l->values[i] & DMASK);
                strcat(temp, t);
            }
            sig = strdup(trim(temp));
            break;
        case litITSITP:
        case litDoubleInt:
            asprintf(&sig, "%012llo %012llo", l->values[0] & DMASK, l->values[1] & DMASK);
            break;
        default:
            yyprintf("makeLiteralSignature(): unhandled literal <%s>", l->name);
            break;
    }
    return sig;
}

#if RFU
void makeLiteralSignatures()
{
    word36 Ypair[2];
    char temp[4096];
    
    literal *l, *tmp;
    HASH_ITER(hh, litPool, l, tmp)
    {
        switch (l->litType)
        {
            case litUnknown:
            case litGeneric:
                asprintf(&l->signature, "%012llo", l->value);
                break;
            case litScaledFixed:
                asprintf(&l->signature, "%012llo", l->value);
                break;
            case litSingle:
                IEEElongdoubleToYPair(l->r, Ypair);
                asprintf(&l->signature, "%012llo" ,Ypair[0] & DMASK);
                break;
            case litDouble:
                IEEElongdoubleToYPair(l->r, Ypair);
                asprintf(&l->signature, "%012llo %012llo", Ypair[0] & DMASK, Ypair[1] & DMASK);
                break;
            case litString:
            case litVFD:
                memset(temp, 0, sizeof(temp));
                for(int i = 0 ; i < l->nWords ; i += 1)
                {
                    char t[32];
                    sprintf(t, "%012llo ", l->values[i] & DMASK);
                    strcat(temp, t);
                }
                l->signature = strdup(trim(temp));
                break;
            case litITSITP:
            case litDoubleInt:
                asprintf(&l->signature, "%012llo %012llo", l->values[0] & DMASK, l->values[1] & DMASK);
                break;
            default:
                yyprintf("makeLiteralSignatures(): unhandled literal <%s>", l->name);
                break;
        }
    }
}
#endif

/**
 * get 18-bit version of argument for DU/DL literals ....
 */
word18 get18(literal *l, int dudl)
{
    // if a VFD, octal or fixed-point literal is used with du/dl modification then the lower 18-bits of the literal
    // are placed is the address field of the instruction. If any other type of literal is used with du/dl modification then
    // the upper 18-bits of the literal are placed in the address field of the instruction
    word36 lval = l->values[0];
    
    word18 val = 0;
    word36 Ypair[2] = {0, 0};
    
    switch (l->litType)
    {
        case litGeneric:     ///< just a generic literal representing an address
        case litScaledFixed: ///< scaled, fixed-point literal
        //case litScaledReal:  ///< scaled, floating-point literal
        case litDoubleInt:  ///< scaled, double-precision, fixed-point literal
        case litVFD:
            val = GETLO(lval);
            break;
            
        case litSingle:     ///< single precision fp literal
        case litDouble:     ///< double precision fp literal
            IEEElongdoubleToYPair(l->r, Ypair);
            val = GETHI(Ypair[0]);
            break;

        default:
            //litITS,
            //litITP
            val = GETHI(lval);
            break;
    }
    
    word18 sa = l->key.srcAddr;
    
    // and remove literal from literal pool because it's a du/dl immediate literal
    if (debug) fprintf(stderr, "get18(): deleting immediate %s literal for address %06o/%d\n", dudl == 3 ? "du" : "dl", l->key.srcAddr, l->key.seqNo);
    
    int cnt1 = HASH_COUNT(litPool);
    
    HASH_DEL(litPool, l);  /* l: pointer to deletee */
    free(l);               /* optional; it's up to you! */
    
    int cnt2 = HASH_COUNT(litPool);
    if (cnt2 != cnt1-1)
        yyprintf("du/dl literal delete count wrong %d/%d!!!", cnt1, cnt2);

    //l = getliteralByAddr(sa);
    l = getliteralByAddrAndPosn(sa, litPosInLine);
    if (l)
        yyprintf("du/dl literal <%s> not deleted!!!", l->name);
    
    return val;
}

