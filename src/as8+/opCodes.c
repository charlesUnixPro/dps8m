/**
 * \file opCodes.c
 * \project as8
 * \author Harry Reed
 * \date 03/29/13
 *  Created by Harry Reed on 03/29/13.
 * \copyright Copyright (c) 2012-2013 Harry Reed. All rights reserved.
 */

#include "as.h"

#include "y.tab.h"

void dumpOutput(FILE *f)
{
    if (debug) fprintf(f, "==== output =====\n");

    outRec *r;
    
    // 1st, dump any directives ....
    DL_FOREACH(as8output, r)
    {
        if (r->recType == outRecDirective)
        {
            if (r->dirStr)
                fprintf(f, "%s\n", r->dirStr);
        }
    }

    DL_FOREACH(as8output, r)
    {
        if (r->recType != outRecDirective)
            fprintf(f, "%6.6o xxxx %012llo %s%s", r->address, r->data, r->src, strchr(r->src, '\n') ? "" : "\n");
    }
}

word18 getOpcode(const char *op)    /// , const char *arg1, const char *arg2) {
{
    opCode *s;
    HASH_FIND_STR(InstructionTable, op, s);
    if (s)
    {
        return s->opcode;   //encoding;
    }
    return 0;
}
word18 getEncoding(const char *op)    /// , const char *arg1, const char *arg2) {
{
    opCode *s;
    HASH_FIND_STR(InstructionTable, op, s);
    if (s)
        return s->encoding;
    return 0;
}


opCode *findOpcode(char *op)
{
    //char *o = strlower(op);
    opCode *s = NULL;
    
    HASH_FIND_STR(InstructionTable, op, s);  /* s: output pointer */
    
    return s;
}

int getmod(const char *arg_in) {
    word8 mod;
    char arg[64]; ///< a little bigger
    int n;
    int c1, c2;
    mod = 0;
    for(c1 = c2 = 0 ; arg_in[c1] && (c2 < sizeof(arg)) ; ++c1) {
        if(arg_in[c1] != 'x' && arg_in[c1] != 'X') {
            /* filter out x's */
            //if (debug) printf("getmod filter: \"%s\"[%d]: %c -> [%d]\n", arg_in, c1, arg_in[c1], c2);
            arg[c2++] = arg_in[c1];
        }
    }
    arg[c2] = '\0';
    
    if (strcmp(arg, "*") == 0)    // HWR 7 Nov 2012 Allow * for an alias for n*
        strcpy(arg, "n*");
    
    for(n = 0 ; n < 0100 ; ++n)
    {
        if (extMods[n].mod && !strcasecmp(arg, extMods[n].mod))
        {
            mod = n;
            return mod;
        }
    }
    return -1;  // modifier not found
    
    //if (debug) printf("MOD %s [%s] = %o\n", arg, arg_in, mod);
    
    // XXX allow for a symbol to be a modifier
    //return mod;
}

// process all OPCODE instructions

void doOpcode(struct opnd *o)
{   
    if (nPass == 1)
    {
        //if (debug) printf("doOpcode(1):addr=%06o\n", addr);
        addr += 1;      // just bump address counter
        return;
    }
    if (nPass != 2)     // only pass 1 | 2 allowed
        return;
    
    outRec *p = newoutRec();
    p->address = addr;
    
    p->data = o->o->encoding;   // initialize basic instruction
    p->data |= o->lo;           // or-in modifier
    p->data |= ((word36)o->hi << 18);
    if (o->bit29)
        p->data |= (1LL << 6);    // set bit-29
    
    p->src = strdup(LEXline);
    
    DL_APPEND(as8output, p);
    
    //if (debug) printf("doOpcode(2):addr=%06o\n", addr);
    addr += 1;
}

void doMWEis(opCode *op, tuple *head)
{
    if (nPass == 1)
    {
        addr += 1;
        return;
    }
    
    bool bEnableFault = false;
    bool bAscii = false;
    bool bRound = false;
    
    bool bT = false;
    bool bP= false;
    bool bR = false;

    int boolValue = 0;
    int fillValue = 0;
    int maskValue = 0;

    int MFk[3] = {0, 0, 0};
    
    // iterate through the list of modifiers and options ...
    tuple *t, *t2;

    int idx = 0;
    DL_FOREACH(head, t)
    {
        int retVal = 0;
        
        switch (t->a.c)
        {
            case 'm':       // an EIS modification field
                // a MFk modifier
                DL_FOREACH(t->b.t, t2)
                {
                    char *reg = t2->a.p;
                    if (strlen(reg) == 0)
                        continue;
                    if (!strcasecmp(reg, "ar"))  // an AR present?
                        retVal |= MFkAR;    //0x40;
                    else if (!strcasecmp(reg, "pr"))  // an PR present?
                        retVal |= MFkPR;    //0x40;
                    else if (!strcasecmp(reg, "rl"))  // an RL present?
                        retVal |= MFkRL;    //0x20;
                    else if (!strcasecmp(reg, "id"))  // an id present?
                        retVal |= MFkID;    //0x10;
                    else
                    {
                        int mfreg = findMfReg(reg);
                        if (mfreg == -1)
                            yyprintf("unknown mf modifier/register <%s>", reg);
                        
                        retVal |= mfreg;
                    }
                }
                break;
            case 'o':       // a keyword/option
                // an option
                switch(t->b.t->a.c)
                {
                    case '1':
                        if (!strcasecmp(t->b.t->b.p, "ascii"))
                            bAscii = true;
                        else if (!strcasecmp(t->b.t->b.p, "enablefault"))
                            bEnableFault = true;
                        else if (!strcasecmp(t->b.t->b.p, "round"))
                            bRound = true;
                        else if (!strcasecmp(t->b.t->b.p, "p"))
                            bP = true;
                        else if (!strcasecmp(t->b.t->b.p, "t"))
                            bT = true;
                        else if (!strcasecmp(t->b.t->b.p, "r"))
                            bR = true;
                        else
                            yyprintf("unknown option <%s>", t->b.t->b.p);
                        break;
                    case '2':
                        if (!strcasecmp(t->b.t->b.p, "bool"))
                            boolValue = (int)t->b.t->c.i36;
                        else if (!strcasecmp(t->b.t->b.p, "fill"))
                            fillValue = (int)t->b.t->c.i36;
                        else if (!strcasecmp(t->b.t->b.p, "mask"))
                            maskValue = (int)t->b.t->c.i36;
                        else
                            yyprintf("unknown option %s()", t->b.t->b.p);
                        break;
                }
                break;
        }

        MFk[idx] = retVal;

        idx += 1;
    }
    
    word36 i = 0;   ///< 1 st instruction word
    // should we do error checking here or not? Add later if needed
    
    // insert MFk's
    i = MFk[0];     // MF1
    if (idx > 0 && MFk[1] >= 0)
    {
        i = bitfieldInsert36(i, MFk[1], 18, 7);
        if (idx > 1 && MFk[2] >= 0)
            i = bitfieldInsert36(i, MFk[2], 27, 7);
    }
    
    // insert opcode
    //i = bitfieldInsert36(i, op->opcode << 1 | 1, 8, 9);
    i |= op->encoding;
    
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
    
    outas8(i, addr, LEXline);
    
    addr += 1;
}


void doRPT(opCode *o, word36 tally, word36 delta, tuple *tcnds)
{
    /*
     * supports 2 formats ....
     *
     * rpt(x)  tally, delta, term1, term2, ..., termN
     * rpt(x)  ,delta
     *
     * save/don't forget I bit (interrupt inhibit) for later
     */
    
    if (nPass == 1)
    {
        addr += 1;     // just consume 1 address
        return;
    }
    
    word36 w = o->encoding;   //000000520200LL;  // basic RPT instruction
    
    /*
     * look for terminal conditons
     */
    tuple *t;
    int terms = 0;          // accumulated terminal conditions
    DL_FOREACH(tcnds, t)
    {
        char *term = t->a.p;
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
            yyprintf("unrecognized terminal condition <%s> for RPT/RPTX", term);
            return;
        }
    }
    //                  dst, src,        off, bits
    w = bitfieldInsert36(w, tally & 0377, 28, 8);       // insert 8-bit tally
    
    // RPT 6-bit delta
    // RPD 6-bit delta
    // RPL no delta
    if (o->opcode != 0500)                              // RPL has no delta
        w = bitfieldInsert36(w, delta & 077,   0, 6);   // insert 6-bit delta
    
    w |= terms;
    
    if (!strcasecmp(o->mne, "rpt") || !strcasecmp(o->mne, "rpl"))
        w = bitfieldInsert36(w, 1, 25, 1);          // C = 1
    else if (!strcasecmp(o->mne, "rpd"))
        w = bitfieldInsert36(w, 7, 25, 3);          // A = B = C == 1
    else if (!strcasecmp(o->mne, "rpda"))
        w = bitfieldInsert36(w, 5, 25, 3);          // A = C == 1, B == 0
    else if (!strcasecmp(o->mne, "rpda"))
        w = bitfieldInsert36(w, 3, 25, 3);          // A == 0, B = C == 1

    //{"rpd",  OPCODERPT, 0, 0, false, 0560, 0560200},     // A = B = C == 1
  	//{"rpdx", OPCODERPT, 0, 0, false, 0560, 0560200},     // A = B = C == 0
   	//{"rpda", OPCODERPT, 0, 0, false, 0560, 0560200},     // A = C == 1, B == 0
  	//{"rpdb", OPCODERPT, 0, 0, false, 0560, 0560200},     // A == 0, B = C == 1
	//{"rpt",  OPCODERPT, 0, 0, false, 0520, 0520200},    // C == 1
    //{"rptx", OPCODERPT, 0, 0, false, 0520, 0520200},    // C == 0
    //{"rpl",  OPCODERPT, 0, 0, false, 0500, 0500200},    // C == 1
  	//{"rplx", OPCODERPT, 0, 0, false, 0500, 0500200},    // C == 0


    
    
    //fprintf(oct, "%6.6o xxxx %012llo %s \n", (*addr)++, w, inLine);
    outas8(w, addr, LEXline);
    
    addr += 1;
}

void doSTC(opCode *o, word36 a, word36 b, int pr)
{
    if (nPass == 2)
    {
        word36 w = o->encoding | (b & 077);
        
        //if (o->opcode == 0551 || o->opcode == 0552) // stba/stbq
        //    o->encoding | (b & 077);
        //else

        if (pr > 0) // no pr specified
        {
            w |= ((pr & 07) << 15) | (b & 077777);
            w |= (1LL << 6);    // set bit-29
        }
        else
            SETHI(w, a & AMASK);
    
        outas8(w, addr, LEXline);
    }
    addr += 1;
}


void doARS(opCode *o, word36 ptr, word36 offset, word36 modifier)
{
    if (nPass == 2)
    {
        word36 w = o->encoding;
        w = bitfieldInsert36(w, ptr, 33, 3);
        w = bitfieldInsert36(w, offset, 18, 15);
        w = bitfieldInsert36(w, modifier, 0, 4);
    
        // if mnemonic does not end with x then set A (bit 29)
        if (o->mne[strlen(o->mne)-1] != 'x')
            w = bitfieldInsert36(w, 1, 6, 1);
        
        outas8(w, addr, LEXline);
    }
    addr += 1;
}


outRec *newoutRec(void)
{
    outRec *p = calloc(1, sizeof(outRec));  // calloc() zeros-out memory
    p->recType = outRecUnknown;
    p->direct = NULL;
    p->dlst = NULL;
    p->src = NULL;
    
    return p;
}


outRec *outas8(word36 data, word18 address, char *srctext)
{
    outRec *p = newoutRec();
    p->recType = outRecDefault;
    
    p->address = address;
    
    p->data = data;
    p->src = srctext ? strdup(srctext) : "";
    
    // remove all internal '\n's from srctext
    char *q = strchr(p->src, '\n');
    if (q)
    {
        while (q)
        {
            strcpy(q, q + 1);
            q = strchr(p->src, '\n');
        }
    }
    DL_APPEND(as8output, p);
    
    return p;
}

outRec *outas8Str(char *s36, word18 address, char *srctext)
{
    word36 data = strtoll(s36, NULL, 8);
    return outas8(data, address, srctext);
}

extern list *newList();

/*
 * write a directive to the output stream ...
 */
outRec *outas8Direct(char *dir, ...)
{
    outRec *p = newoutRec();
    p->recType = outRecDirective;
    
    p->direct = strdup(dir);
    
    va_list listPointer;
    
    va_start( listPointer, dir );
    
    if (!strcasecmp(dir, "size"))
    {
        // size of assembly in word18 words
        list *l = newList();
        l->i18 = va_arg( listPointer, word18 );
        DL_APPEND(p->dlst, l);
        
        asprintf(&p->dirStr, "!SIZE %06o", l->i18);
        
    } else
        fprintf(stderr, "outas8Direct(): unhandled directive <%s>\n", dir);
    
    va_end( listPointer );

    DL_APPEND(as8output, p);
    
    return p;
}
