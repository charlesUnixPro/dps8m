/**
 * \file dps8_loader.c
 * \project dps8
 * \date 11/14/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"

#include "utlist.h" // for linked list ops

/* Master loader */
#define FMT_O   1
#define FMT_S   2
#define FMT_E   3
#define FMT_9   9

t_stat load_oct (FILE *fileref, int32 segno, int32 ldaddr, bool bDeferred, bool bVerbose);

bool bdeferLoad = false;    // defer load to after symbol resolution

segdef *newSegdef(char *sym, int val)
{
    segdef *p = calloc(1, sizeof(segdef));
    p->symbol = strdup(sym);
    p->value = val;
    
    p->next = NULL;
    p->prev = NULL;
    
    return p;
}

void freeSegdef(segdef *p)
{
    if (p->symbol)
        free(p->symbol);
    p->symbol = NULL;
}


segref *newSegref(char *seg, char *sym, int val)
{
    segref *p = calloc(1, sizeof(segref));
    if (seg && strlen(seg) > 0)
        p->segname = strdup(seg);
    if (sym && strlen(sym) > 0)
        p->symbol = strdup(sym);
    p->value = val;     // address in segment to put ITS pair
    p->snapped = false; // not snapped yet
    
    p->segno = -1;

    p->next = NULL;
    p->prev = NULL;
    
    return p;
}

void freeSegref(segref *p)
{
    if (p->segname)
        free(p->segname);
    p->segname = NULL;
    if (p->symbol)
        free(p->symbol);
    p->symbol = NULL;
}

segment *newSegment(char *name, int size, bool bDeferred)
{
    segment *s = calloc(1, sizeof(segment));
    if (name && strlen(name) > 0)
        s->name = strdup(name);
    if (bDeferred && size > 0)
        s->M = calloc(1, sizeof(word36) * size);
    else
        s->M = NULL;
    s->size = size;
    
    s->defs = NULL;
    s->refs = NULL;
    
    s->segno = -1;
    s->ldaddr = -1;
    
    s->deferred = false; 
    s->linkOffset = -1;
    s->linkSize = 0;
    
    s->next = NULL;
    s->prev = NULL;
    
    return s;
}


void freeSegment(segment *s)
{
    segdef *d, *dtmp;
    DL_FOREACH_SAFE(s->defs, d, dtmp)
    {
        freeSegdef(d);
        DL_DELETE(s->defs, d);
    }
    
    segref *r, *rtmp;
    DL_FOREACH_SAFE(s->refs, r, rtmp)
    {
        freeSegref(r);
        DL_DELETE(s->refs, r);
    }
    
    if (s->deferred && s->M)
        free(s->M);
    if (s->name)
        free(s->name);
}

segment *segments = NULL;   // segment for current load unit
segment *currSegment = NULL;

// remove segment from list of deferred segments

int removeSegment(char *seg)
{
    segment *el, *tmp;
    
    if (strcmp(seg, "*") == 0)  // all?
    {
        DL_FOREACH_SAFE(segments, el, tmp)
        {
            printf("Removing segment '%s'...\n", el->name);
            freeSegment(el);
            DL_DELETE(segments, el);
        }
        return SCPE_OK;
    }
    else
    {
        DL_FOREACH_SAFE(segments, el, tmp)
            if (strcmp(seg, el->name) == 0)
            {
                printf("Removing segment '%s'...\n", el->name);
                freeSegment(el);
                DL_DELETE(segments, el);
                return SCPE_OK;
            }
    }
    
    return SCPE_ARG;
}

int removeSegdef(char *seg, char *sym)
{
    segment *sel;
    DL_FOREACH(segments, sel)
    {
        // look for segment
        if (strcmp(seg, sel->name) == 0)
        {
            // now, delete segdef ...
            segdef *del, *dtmp;
            if (strcmp(sym, "*") == 0)
            {
                DL_FOREACH_SAFE(sel->defs, del, dtmp)
                {
                    printf("Removing segdef '%s$%s'...\n", sel->name, del->symbol);

                    freeSegdef(del);
                    DL_DELETE(sel->defs, del);
                }
                return SCPE_OK;
            }
            else
            {
                DL_FOREACH_SAFE(sel->defs, del, dtmp)
                {
                    if (strcmp(del->symbol, sym) == 0)
                    {
                        printf("Removing segdef '%s$%s'...\n", sel->name, del->symbol);

                        freeSegdef(del);
                        DL_DELETE(sel->defs, del);
                        return SCPE_OK;
                    }
                }
            }
        }
    }
    return SCPE_ARG;
}

int removeSegref(char *seg, char *sym)
{
    segment *sel;
    DL_FOREACH(segments, sel)
    {
        // look for segment
        if (strcmp(seg, sel->name) == 0)
        {
            // now, delete segref ...
            segref *del, *dtmp;
            if (strcmp(sym, "*") == 0)
            {
                DL_FOREACH_SAFE(sel->refs, del, dtmp)
                {
                    printf("Removing segref '%s$%s'...\n", sel->name, del->symbol);

                    freeSegref(del);
                    DL_DELETE(sel->refs, del);
                }
                return SCPE_OK;
            }
            else
            {
                DL_FOREACH_SAFE(sel->refs, del, dtmp)
                {
                    if (strcmp(del->symbol, sym) == 0)
                    {
                        printf("Removing serdef '%s$%s'...\n", sel->name, del->symbol);
   
                        freeSegref(del);
                        DL_DELETE(sel->refs, del);
                        return SCPE_OK;
                    }
                }
            }
        }
    }
    return SCPE_ARG;
}


int objSize = -1;

int segNamecmp(segment *a, segment *b)
{
    return strcmp(a->name, b->name);
}
int segdefNamecmp(segdef *a, segdef *b)
{
    return strcmp(a->symbol, b->symbol);
}
int segrefNamecmp(segref *a, segref *b)
{
    return strcmp(a->symbol, b->symbol);
}

segment *findSegment(char *segname)
{
    segment *sg;
    DL_FOREACH(segments, sg)
        if (!strcmp(sg->name, segname))
            return sg;
    return NULL;
}
segment *findSegmentNoCase(char *segname)
{
    segment *sg;
    DL_FOREACH(segments, sg)
    if (!strcasecmp(sg->name, segname))
        return sg;
    return NULL;
}

segdef *findSegdef(char *seg, char *sgdef)
{
    segment *s = findSegment(seg);
    if (!s)
        return NULL;
    
    segdef *sd;
    DL_FOREACH(s->defs, sd)
        if (strcmp(sd->symbol, sgdef) == 0)
            return sd;
    
    return NULL;
}
segdef *findSegdefNoCase(char *seg, char *sgdef)
{
    segment *s = findSegmentNoCase(seg);
    if (!s)
        return NULL;
    
    segdef *sd;
    DL_FOREACH(s->defs, sd)
    if (strcasecmp(sd->symbol, sgdef) == 0)
        return sd;
    
    return NULL;
}

PRIVATE
void makeITS(int segno, int offset, int tag, word36 *Ypair)
{
    word36 even = 0, odd = 0;
    
    even = ((word36)segno << 18) | 043;
    odd = (word36)(offset << 18) | (tag & 077);
    
    Ypair[0] = even;
    Ypair[1] = odd;
}

//PRIVATE
//_sdw0 *fetchSDW(int segno)
//{
//    int sdwAddr = DSBR.ADDR + (2 * segno);
//    
//    static _sdw0 SDW0;
//    
//    word36 SDWeven = M[sdwAddr + 0];
//    word36 SDWodd  = M[sdwAddr + 1];
//
//    // even word
//    SDW0.ADDR = (SDWeven >> 12) & 077777777;
//    SDW0.R1 = (SDWeven >> 9) & 7;
//    SDW0.R2 = (SDWeven >> 6) & 7;
//    SDW0.R3 = (SDWeven >> 3) & 7;
//    SDW0.F = TSTBIT(SDWeven, 2);
//    SDW0.FC = SDWeven & 3;
//    
//    // odd word
//    SDW0.BOUND = (SDWodd >> 21) & 037777;
//    SDW0.R = TSTBIT(SDWodd, 20);
//    SDW0.E = TSTBIT(SDWodd, 19);
//    SDW0.W = TSTBIT(SDWodd, 18);
//    SDW0.P = TSTBIT(SDWodd, 17);
//    SDW0.U = TSTBIT(SDWodd, 16);
//    SDW0.G = TSTBIT(SDWodd, 15);
//    SDW0.C = TSTBIT(SDWodd, 14);
//    SDW0.EB = SDWodd & 037777;
//
//    return &SDW0;
//}

int getAddress(int segno, int offset)
{
    // XXX Do we need to 1st check SDWAM for segment entry?
    
    
    // get address of in-core segment descriptor word from DSBR
    _sdw0 *s = fetchSDW(segno);
    
    return (s->ADDR + offset) & 0xffffff; // keep to 24-bits
}

/*
 * for a given 24-bit address see if it can be mapped to a segment + offset
 */
bool getSegmentAddressString(int addr, char *msg)
{
    // look for address inside a defined segment ...
    segment *s;
    DL_FOREACH(segments, s)
    {
        int segno = s->segno;       // get segment number from list of known "deferred" segments
        
        _sdw0 *s0 = fetchSDW(segno);
        int startAddr = s0->ADDR;   // get start address
        //if (addr >= startAddr && addr <= startAddr + (s0->BOUND << 4) - 1)
        if (addr >= startAddr && addr <= startAddr + s->size)
            {
                // addr is within bounds of this segment.....
                int offset = addr - startAddr;
                sprintf(msg, "%s|%o", s->name, offset);
                
                return true;
            }
    }
    
    // if we get here then address is not contained within one of the deferred segments
    return false;
}

//PRIVATE
//int getMaxSegno()
//{
//    int maxSegno = -1;
//    segment *sg1;
//    
//    DL_FOREACH(segments, sg1)
//        maxSegno = max(maxSegno, sg1->segno);
//    
//    return maxSegno;
//}

//PRIVATE
//void writeSDW(int segno, _sdw0 *s0)
//{
//    int addr = DSBR.ADDR + (2 * segno);
//    
//    // write a _sdw to memory
//    
//    word36 even = 0, odd = 0;
//    even = bitfieldInsert36(even, s0->ADDR, 12, 24);
//    even = bitfieldInsert36(even, s0->R1, 9, 3);
//    even = bitfieldInsert36(even, s0->R2, 6, 3);
//    even = bitfieldInsert36(even, s0->R3, 3, 3);
//    even = bitfieldInsert36(even, s0->F,  2, 1);
//    even = bitfieldInsert36(even, s0->FC, 0, 2);
//    
//    odd = bitfieldInsert36(odd, s0->BOUND, 21, 14);
//    odd = bitfieldInsert36(odd, s0->R, 20, 1);
//    odd = bitfieldInsert36(odd, s0->E, 19, 1);
//    odd = bitfieldInsert36(odd, s0->W, 18, 1);
//    odd = bitfieldInsert36(odd, s0->P, 17, 1);
//    odd = bitfieldInsert36(odd, s0->U, 16, 1);
//    odd = bitfieldInsert36(odd, s0->G, 15, 1);
//    odd = bitfieldInsert36(odd, s0->C, 14, 1);
//    odd = bitfieldInsert36(odd, s0->EB, 0, 14);
//
//    M[addr + 0] = even;
//    M[addr + 1] = odd;
//}

/*
 * try to resolve external references for all deferred segments
 */

const int StartingSegment = 8;

int resolveLinks(bool bVerbose)
{
    int segno = StartingSegment - 1;//-1;     // current segment number
    
    if (bVerbose) printf("Examining segments ... ");

    // determine maximum segment no 
    segment *sg1;
    DL_FOREACH(segments, sg1)
        segno = max(segno, sg1->segno);
    segno += 1; // ... and one more. This will be our starting segment number.
    
    if (bVerbose) printf("segment numbering begins at: %d\n", segno);
    
    DL_FOREACH(segments, sg1)
    {
        if (bVerbose) printf("    Processing segment %s...\n", sg1->name);

        if (sg1->segno == -1)
            sg1->segno = segno ++;  // assign segment # to segment
     
        //printf("(assigned as segment #%d)\n", sg1->segno);
        
        segref *sr;
        DL_FOREACH(sg1->refs, sr)
        {
            if (bVerbose) printf("        Resolving segref %s$%s...", sr->segname, sr->symbol);
            
            // now loop through all segrefs trying to find the segdef needed
            
            bool bFound = false;
            segment *sg2;
            DL_FOREACH(segments, sg2)
            {
                if (strcmp(sg2->name, sr->segname))
                    continue;

                if (sg2->segno == -1)
                {
                    sg2->segno = segno ++;
                    //printf("assigned segment # %d to segment %s\n", sg2->segno, sg2->name);
                }
                
                segdef *sd;
                DL_FOREACH(sg2->defs, sd)
                {                    
                    if (strcmp(sd->symbol, sr->symbol) == 0)
                    {
                        bFound = true;
                        if (bVerbose) printf("found %s (%06o)\n", sr->symbol, sr->value);
                        
                        word36 *Ypair = &sg1->M[sr->value];
                        makeITS(sg2->segno, sd->value, 0, Ypair);   // "snap" link for segref in sg1
                        if (bVerbose) printf("            ITS Pair: [even:%012llo, odd:%012llo]\n", Ypair[0], Ypair[1]);
                    }
                }
            }
            if (bVerbose && bFound == false)
                printf("not found\n");
        }
    }
    
    return 0;
}

PRIVATE
int loadDeferredSegment(segment *sg, int addr24)
{
    printf("    loading %s as segment# %d\n", sg->name, sg->segno);
        
    int segno = sg->segno;
        
    word18 segwords = sg->size;
    
    memcpy(M + addr24, sg->M, sg->size * sizeof(word36));
    
    DSBR.BND = 037777;  // temporary max bound ...
    
    if (loadUnpagedSegment(segno, addr24, segwords) == SCPE_OK)
        printf("      %d (%06o) words loaded into segment %d (%o) at address %06o\n", segwords, segwords, segno, segno, addr24);
    else
        printf("      Error loading segment %d (%o)\n", segno, segno);
    
    // update in-code SDW to reflect segment info
    // Done in loadUnpagedSegment()
    
//    _sdw0 *s0 = fetchSDW(segno);
//    
//    s0->ADDR = addr24; // 24-bit absolute address
//    
//    int bound = segwords;
//    bound += bound % 16;
//    
//    s0->BOUND = ((bound-1) >> 4) & 037777; ///< The 14 high-order bits of the last Y-block16 address within the segment that can be referenced without an access violation, out of segment bound, fault
//    s0->R1 = s0->R2 = s0->R3 = 0;
//    // XXX probably need to fill in more
//    
//    writeSDW(segno, s0);
    
    return 0;
}

/*!
 * load/add deferred segments into memory and set-up segment table for appending mode operation ...
 */
int loadDeferredSegments(bool bVerbose)
{
    // First, check to see if DSBR is set up .....
    if (DSBR.ADDR == 0) // DSBR *probably* not initialized. Issue warning and ask....
        if (!get_yn ("DSBR *probably* uninitialized (DSBR.ADDR == 0). Proceed anyway [N]?", FALSE))
            return -1;

    if (bVerbose) printf("Loading deferred segments ...\n");
    
    int ldaddr = DSBR.ADDR + 65536;     // load segments *after* SDW table
    if (ldaddr % 16)
        ldaddr += 16 - (ldaddr % 16);   // adjust to 16-word boundary
    
    int maxSegno = -1;
    
    segment *sg;
    DL_FOREACH(segments, sg)
    {
        if (!sg->deferred)
            continue;
        
        // use specified address or no?
       
        //int lda = sg->ldaddr == -1 ? ldaddr : sg->ldaddr;
        
        sg->ldaddr = ldaddr;
        //loadDeferredSegment(sg, lda);
        loadDeferredSegment(sg, ldaddr);
        int segno = sg->segno;

        // set PR4 to point to LOT
        // ToDo: probably better to just set up the initial stack.header
        if (strcmp(sg->name, LOT) == 0)
        {
            PR[4].BITNO = 0;
            PR[4].CHAR = 0;
            PR[4].SNR = segno;
            PR[4].WORDNO = 0;
            int n = 4;
            if (bVerbose) printf("LOT=>PR[%d]: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\n", n, PR[n].SNR, PR[n].RNR, PR[n].WORDNO, PR[n].BITNO);
        }
        
        // bump next load address to a 16-word boundary
        //if (sg->ldaddr == -1)
        //{
            word18 segwords = sg->size;
            ldaddr += segwords;
            if (ldaddr % 16)
                ldaddr += 16 - (ldaddr % 16);
        //}
        maxSegno = max2(maxSegno, segno);
    }

    // adjust DSBR.BND to reflect highest segment address
    DSBR.BND = (2 * maxSegno) / 16;

    return 0;
}

/*
 * create a linkage Offset Table segment ...
 */
t_stat createLOT(bool bVerbose)
{
    segment *s;
    
    // see if lot$ already exists ...
    DL_FOREACH(segments, s)
    {
       if (!strcmp(s->name, LOT))
       {
           // remove it and re-create it
           //printf("Linkage Offset Table (lot$) segment already exists. Try 'segment lot$ remove'\n");
           //return SCPE_ARG;
           DL_DELETE(segments, s);
           break;
       }
    }
    
    // if we get here we're free to create the lot$ segment ...
    
    // determine maximum segment number ...
    int maxSeg = -1;
    int numSeg = 0;     // number of segments
    
    DL_FOREACH(segments, s)
    {
        maxSeg = max2(maxSeg, s->segno);
        numSeg += 1;
    }
    
    // create a lot segment ...
    //segment *lot = newSegment(LOT, maxSeg + 1, true);
    segment *lot = newSegment(LOT, 255, true);  // sooo wrong
    //lot->segno = maxSeg + 1;
    
    lot->defs = newSegdef(LOT, 0);
    lot->deferred = true;
    
    // Now go through each segment getting the linkage address and filling in the LOT table with the address (in sprn/lprn packed pointer format)
    
    // C(PRn.BITNO) → C(Y)0,5
    // C(PRn.SNR)3,14 → C(Y)6,17
    // C(PRn.WORDNO) → C(Y)18,35
    
//    DL_FOREACH(segments, s)
//    {
//        word36 pp = 0;
//        if (s->linkOffset != -1)
//        {
//            pp = bitfieldInsert36(pp, s->linkOffset, 0, 18);    // link address (0-based offset)
//            pp = bitfieldInsert36(pp, s->segno, 18, 12);        // 12-bit(?) segment #
//        }
//        lot->M[s->segno] = pp & DMASK;
//    }

    DL_APPEND(segments, lot);
    
    printf("%s segment created with %d sparse entries.\n", LOT, numSeg);
    
    return SCPE_OK;
}

t_stat snapLOT(bool bVerbose)
{
    segment *s, *lot;
    bool bFound = false;
    // see if lot$ already exists ...
    DL_FOREACH(segments, lot)
    {
        if (!strcmp(lot->name, LOT))
        {
            bFound = true;
            break;
        }
    }
    if (!bFound)
    {
        printf("snapLOT(): segment '%s' not found!\n", LOT);
        return SCPE_UNK;
    }
    
    // if we get here we're free to fill in lot$ segment ...
        
    // Now go through each segment getting the linkage address and filling in the LOT table with the address (in sprn/lprn packed pointer format)
    
    if (bVerbose) printf("snapping %s links", LOT);
    
    DL_FOREACH(segments, s)
    {
        if (!strcmp(s->name, LOT))
            continue;
        
        word36 pp = 0;
        if (s->linkOffset != -1)
        {
            // C(PRn.BITNO) → C(Y)0,5
            // C(PRn.SNR)3,14 → C(Y)6,17
            // C(PRn.WORDNO) → C(Y)18,35
            
            pp = bitfieldInsert36(pp, s->linkOffset, 0, 18);    // link address (0-based offset)
            pp = bitfieldInsert36(pp, s->segno, 18, 12);        // 12-bit(?) segment #
            
            //lot->M[lot->ldaddr + s->segno] = pp & DMASK;
            M[lot->ldaddr + s->segno] = pp & DMASK; // LOT is in-core

            if (bVerbose) printf("%o %o %012llo.", lot->ldaddr, s->segno, pp);
            //printf(".");
        }
    }
    if (bVerbose) printf("\n");
    
    return SCPE_OK;
}

t_stat createStack(int n, bool bVerbose)
{
    if (n < 0 || n > 7)
        return SCPE_ARG;
    
    char name[32];
    sprintf(name, "stack_%d", n);
    
    segment *s;
    
    // see if stack_n segment already exists ...
    DL_FOREACH(segments, s)
    {
        if (!strcmp(s->name, name))
        {
            DL_DELETE(segments, s);
            break;
        }
    }
    
    // if we get here we're free to create the stack_n segment ...
    
    // determine maximum segment number ...
    int maxSeg = -1;

    DL_FOREACH(segments, s)
        maxSeg = max2(maxSeg, s->segno);
    
    // create a stack_0 segment ...
    segment *stk = newSegment(name, 48 * 1024, true);
    stk->defs = newSegdef(name, 0);
    stk->deferred = true;
    
    // DSBR.STACK: The upper 12 bits of the 15-bit stack base segment number. Only used by call6 instruction
    // since this segment will be stored in DSBR.STACK we need to make certain that the segment # has the form xxxx0
    stk->segno = maxSeg + 1;
    if ((stk->segno % 8) != n)
        stk->segno += 8 - (stk->segno % 8) + n;
    
    DSBR.STACK = stk->segno >> 3;
    
    DL_APPEND(segments, stk);
    
    printf("%s segment created as segment# %d (%o) [DSBR.STACK=%04o]\n", name, stk->segno, stk->segno, DSBR.STACK);
    
    return SCPE_OK;
}

/*
 * setup faux execution environment ...
 */
t_stat setupFXE()
{
    
}

/*!
 * scan & process source file for any !directives that need to be processed, e.g. !segment, !go, etc....
 */
t_stat scanDirectives(FILE *f, bool bDeferred, bool bVerbose)
{
    long curpos = ftell(f);
    
    rewind(f);

    objSize = -1;
    currSegment = NULL;
    
    char buff[1024], *c;
    while((c = fgets(buff, sizeof(buff), f)) != NULL)
    {
        if (buff[0] != '!')
            continue;
        
        char args[4][256];
        memset(args, 0, sizeof(args));
        
        sscanf(buff, "%s %s %s %s", args[0], args[1], args[2], args[3]);

        // process !segment
        if (strcasecmp(args[0], "!segment") == 0)
        {
            if (currSegment)
                currSegment->segno = (int)strtol(args[1], NULL, 0);
        }
        
        else
            
        // process !go (depreciate?)
        if (strcasecmp(args[0], "!go") == 0)
        {
            long addr = strtol(args[1], NULL, 0);
            rIC = addr & AMASK;
            
            if (rIC)
                fprintf(stderr, "!GO address: %06lo\n", addr);
        }
        
        else
            
        // process !segname name
        //if (bDeferred && strcasecmp(args[0], "!segname") == 0)
        if (strcasecmp(args[0], "!segname") == 0)
        {
            if (args[1] == NULL || strlen(args[1]) == 0)
                continue;
            
            // make a new segment
            segment *s = newSegment(args[1], objSize, bDeferred);
           
            // see if segment already exists
            if (segments)
            {
                segment *elt;
                
                DL_SEARCH(segments, elt, s, segNamecmp);
                
                if (elt)
                {
                    fprintf(stderr, "segment '%s' already loaded. Use 'segment remove'\n", elt->name);
                    freeSegment(s);
                    continue;
                }
            }
            // ... and add it to the listt
            DL_APPEND(segments, s);
            currSegment = s;
            
            printf("segment created for '%s'\n", s->name);
        }
        
        else
            
        //if (bDeferred && strcasecmp(args[0], "!linkage") == 0)
        if (strcasecmp(args[0], "!linkage") == 0)
        {
            // e.g. !LINKAGE 004456 4
            int laddr = -1, lsize = 0;
            
            sscanf(buff, "%*s %o %o", &laddr, &lsize);
            if (currSegment)
            {
                currSegment->linkOffset = laddr;
                currSegment->linkSize = lsize;
            }
        }

        else
            
        // process !segdef symbol value
        //if (bDeferred && !strcasecmp(args[0], "!segdef"))
        if (!strcasecmp(args[0], "!segdef"))
        {
            char symbol[256];
            int value;
            
            sscanf(buff, "%*s %s %o", symbol, &value);
            
            segdef *s = newSegdef(symbol, value);
            // see if segdef already exists
            if (segments->defs)
            {
                segdef *elt;
                
                DL_SEARCH(currSegment->defs, elt, s, segdefNamecmp);
                
                if (elt)
                {
                    fprintf(stderr, "symbol '%s' already loaded for segment '%s'. Use 'segment segdef remove'\n", elt->symbol, currSegment->name);
                    freeSegdef(s);
                    continue;
                }
            }
            DL_APPEND(currSegment->defs, s);
            
            printf("segdef created for segment %s, symbol '%s', addr:%06o\n", segments->name, symbol, value);
        }

        //else if (bDeferred && !strcasecmp(args[0], "!entry"))
        else if (!strcasecmp(args[0], "!entry"))
        {
            // very similiar to segdef
            
            char symbol[256];
            int value;
            
            sscanf(buff, "%*s %s %*s %o", symbol, &value);
            
            segdef *s = newSegdef(symbol, value);
            // see if segdef already exists
            if (segments->defs)
            {
                segdef *elt;
                
                DL_SEARCH(currSegment->defs, elt, s, segdefNamecmp);
                
                if (elt)
                {
                    fprintf(stderr, "segdef/entrypoint '%s' already found for segment '%s'. Use 'segment segdef remove'\n", elt->symbol, currSegment->name);
                    freeSegdef(s);
                    continue;
                }
            }
            DL_APPEND(currSegment->defs, s);
            
            printf("entrypoint created for segment %s, symbol '%s', addr:%06o\n", segments->name, symbol, value);
        }
        
        else

        // process !segref segname symbol
        //if (bDeferred && strcasecmp(args[0], "!segref") == 0)
        if (strcasecmp(args[0], "!segref") == 0)
        {
            char segment[256], symbol[256];
            int addr;
            
            sscanf(buff, "%*s %s %s %i", segment, symbol, &addr);
            
            segref *s = newSegref(segment, symbol, addr);
            
            // see if segref already exists
            if (currSegment->refs)
            {
                segref *elt;
                
                DL_SEARCH(currSegment->refs, elt, s, segrefNamecmp);
                
                if (elt)
                {
                    fprintf(stderr, "symbol '%s' already referenced in segment '%s'. Use 'segment segref remove'\n", elt->symbol, currSegment->name);
                    freeSegref(s);
                    continue;
                }
            }
            DL_APPEND(currSegment->refs, s);
            printf("segref created for segment '%s' symbol:%s, addr:%06o\n", segment, symbol, addr);
        }
        
        else
        // process !maxaddr
        if (strcasecmp(args[0], "!size") == 0)
        {
            objSize = (int)strtol(args[1], NULL, 8);
            //if (segments && segments->size < 0)
            //    segments->size = maxaddr;
            //if (segments && maxaddr > 0)
            //    segments->M = malloc(sizeof(word36) * maxaddr);
        }
    }
    
    fseek(f, curpos, SEEK_SET);    // restore original position
    return SCPE_OK;
}


/*!
 * "standard" simh/dps8 loader (.oct files) ....
 * Will do real binary files - later.
 */
t_stat load_oct (FILE *fileref, int32 segno, int32 ldaddr, bool bDeferred, bool bVerbose)
{
    /*
     * we'll support the following type of loads
     */
    char buff[132] = "";
    char *c;
    //int line = 0;
    int words = 0;
    
    if (bDeferred)  // a deferred segment load
    {
        while((c = fgets(buff, sizeof(buff), fileref)) != NULL)
        {
            if (buff[0] == '!' || buff[0] == '*')
                continue;
            
            word24 maddr; ///< 18 bits
            word36 data;  ///< 36 bits
            
            int n = sscanf(c, "%o %*s %llo", &maddr, &data);
            if (n == 2)
            {
                if (maddr > currSegment->size)
                {
                    printf("ERROR: load_oct(deferred): attempted load into segment %s location %06o (max %06o)\n", currSegment->name, maddr, currSegment->size);
                    return SCPE_NXM;
                }
                else
                    currSegment->M[maddr] = data & DMASK;
                words++;
            }
            currSegment->ldaddr = ldaddr;
            currSegment->segno = segno;
            currSegment->deferred = true;
        }
        if (segno != -1)
            sprintf(buff, " as segment %d", segno);
        else
            strcpy(buff, "");
        
        printf("%d (%06o) words loaded into segment %s%s\n", words, words, currSegment->name, buff);
    }
    else
    if (segno == -1)// just do an absolute load
    {
        while((c = fgets(buff, sizeof(buff), fileref)) != NULL)
        {
            if (buff[0] == '!' || buff[0] == '*')
                continue;
            
            word24 maddr; ///< 18 bits
            word36 data;  ///< 36 bits
            
            int n = sscanf(c, "%o %*s %llo", &maddr, &data);
            if (n == 2)
            {
                if (currSegment && currSegment->M == NULL)
                    currSegment->M = &M[maddr];

                if (maddr > MAXMEMSIZE)
                    return SCPE_NXM;
                else
                    M[maddr] = data & DMASK;
                words++;
            }
        }
        printf("%d (%06o) words loaded\n", words, words);
    }
    else
    {
        // load data into segment segno @ M[ldaddr]
        word24 maxaddr = 0;
        
        while((c = fgets(buff, sizeof(buff), fileref)) != NULL)
        {
            if (buff[0] == '!' || buff[0] == '*')
                continue;
            
            word24 maddr; 	///< 24-bits
            word36 data;  	///< 36 bits
            
            int n = sscanf(c, "%o %*s %llo", &maddr, &data);
            if (n == 2)
            {
                if (currSegment && currSegment->M == NULL)
                {
                    currSegment->M = &M[ldaddr];
                    currSegment->segno = segno;
                }
                if (maddr > MAXMEMSIZE)
                    return SCPE_NXM;
                else
                    M[ldaddr + maddr] = data & DMASK;
                //fprintf(stderr, "laddr:%d maddr:%d\n", maddr, maddr);
                words++;
                maxaddr = max(maddr, maxaddr);
            }
        }
        word18 segwords = (objSize == -1) ? maxaddr + 1 : objSize;  // words in segment
        //fprintf(stderr, "segwords:%d maxaddr:%d\n", segwords, maxaddr);
        
        if (loadUnpagedSegment(segno, ldaddr, segwords) == SCPE_OK)
            printf("%d (%06o) words loaded into segment %d(%o) at address %06o\n", words, words, segno, segno, ldaddr);
        else
            printf("Error loading segment %d (%o)\n", segno, segno);
    }
    
    return SCPE_OK;
}

/*
 * stuff todo with loading segments ....
 */

/*!
 * Create sdw0. Create an in-core SDW ...
 */
_sdw0* createSDW0(word24 addr, word3 R1, word3 R2, word3 R3, word1 F, word3 FC, word14 BOUND, word1 R, word1 E, word1 W, word1 P, word1 U, word1 G, word1 C, word14 EB)
{
    static _sdw0 SDW0;
    
    // even word
    SDW0.ADDR = addr & 077777777;
    SDW0.R1   = R1 & 7;
    SDW0.R2   = R2 & 7;
    SDW0.R3   = R3 & 7;
    SDW0.F    =  F & 1;
    SDW0.FC   = FC & 3;
    
    // odd word
    SDW0.BOUND = BOUND & 037777;
    SDW0.R     = R & 1;
    SDW0.E     = E & 1;
    SDW0.W     = W & 1;
    SDW0.P     = P & 1;
    SDW0.U     = U & 1;
    SDW0.G     = G & 1;
    SDW0.C     = C & 1;
    SDW0.EB    = EB & 037777;
    
    return &SDW0;
}

void writeSDW0toYPair(_sdw0 *p, word36 *yPair)
{
    word36 even, odd;
    
    even = p->ADDR << 12 | p->R1 << 9 | p->R2 << 6 | p->R3 << 3 | p->F << 2 | p->FC;
    
    odd = p->BOUND << 21 | p->R << 20 | p->E << 19 | p->W << 18 | p->P << 17 | p->U << 16 | p->G << 15 | p->C << 14 | p->EB;
    
    yPair[0] = even;
    yPair[1] = odd;
}


/*!
 * load an unpaged segment into memory. Assume all R/W/X permissions given.
 */
t_stat loadUnpagedSegment(int segno, word24 addr, word18 count)
{
    if (2 * segno >= 16 * (DSBR.BND + 1))    // segment out of range
    {
        char msg[256];
        sprintf(msg, "Segment %d is not within DSBR.BND (%d) Adjust [Y]?", segno, DSBR.BND);
        if (get_yn (msg, TRUE) == 0)    // No, don't adjust
            return SCPE_MEM;
        
        DSBR.BND = (2 * (segno)) >> 4;
        //fprintf(stderr, "DSBR.BND set to %o\n", DSBR.BND);
    }
    
    const word3 R1 = 0;     ///< ring brackets
    const word3 R2 = 0;
    const word3 R3 = 0;
    const word1 F = true;   ///< segment is in main memory
    const word3 FC = 2;
          word14 BOUND = (count >> 4) & 037777;    ///< 14 high-order bits of the largest 18-bit modulo 16 offset that may be accessed without causing a descriptor violation, out of segment bounds, fault.
          if (count & 017)  ///< if not an integral multiple add 1
              BOUND += 1;
    const word1 R = true;   ///< read permission
    const word1 E = true;   ///< execute permission
    const word1 W = true;   ///< write permission
    const word1 P = true;   ///< allow Privileged instructions
    const word1 U = true;   ///< segment is unpaged
    const word1 G = true;   ///< any legal segment offset may be called
    const word1 C = true;   ///< allow caching (who cares?)
    const word14 EB = count;    ///< Any call into this segment must be to an offset less than EB if G=0
    
    //fprintf(stderr, "B:%d count:%d\n", BOUND, count);
    
    _sdw0 *s = createSDW0(addr, R1, R2, R3, F, FC, BOUND, R, E, W, P, U, G, C, EB);
    word36 yPair[2];
    writeSDW0toYPair(s, yPair);
    
    word24 sdwaddress = DSBR.ADDR + (2 * segno);
    fprintf(stderr, "Writing SDW to address %08o (DSBR.ADDR+2*%d offset) \n", sdwaddress, segno);
    // write sdw to segment table
    core_write2(sdwaddress, yPair[0], yPair[1]);
    
    return SCPE_OK;
}

t_stat sim_load (FILE *fileref, char *cptr, char *fnam, int flag)
{
    size_t fmt;
    extern int32 sim_switches;
    
    int32 segno = -1;
    int32 ldaddr = -1;
    
    bool bDeferred = false; // a deferred load

    fmt = 0;                                                /* no fmt */
    if (sim_switches & SWMASK ('O'))                        /* -o? */
        fmt = FMT_O;
    else if (match_ext (fnam, "OCT"))                       /* .OCT? */
        fmt = FMT_O;
    else {
        //wc = fxread (&data, sizeof (d8), 1, fileref);      /* read hdr */
        //if (wc == 0)                                        /* error? */
        //    return SCPE_FMT;
        //if (LRZ (data) == EXE_DIR)                          /* EXE magic? */
        //    fmt = FMT_E;
        //else if (TSTS (data))                               /* SAV magic? */
        //    fmt = FMT_S;
        //fseek (fileref, 0, SEEK_SET);                       /* rewind */
    }

    bool bVerbose = false;
    if (sim_switches & SWMASK ('V'))                        /* -v? */
        bVerbose = true;

   /*
    * Absolute:
    *  load file.oct
    * Unpaged, non-deferred
    *  load file.oct segment ? address ?
    * Unpages, deferred
    *  load file.oct segment ? address ? deferred
    *
    */
    // load file into segment?
    // Syntax load file.oct segment xxx address addr
    if (flag == 0 && strlen(cptr) && strmask(strlower(cptr), "seg*"))
    {
        char s[128], *end_ptr, w[128], s2[128], sDef[128];
        
        strcpy(s, "");
        strcpy(w, "");
        strcpy(s2, "");
        strcpy(sDef, "");
        
        long n = sscanf(cptr, "%*s %s %s %s %s", s, w, s2, sDef);
        if (!strmask(w, "addr*"))
        {
            fprintf(stderr, "sim_load(): No/illegal destination specifier was found\n");
            return SCPE_FMT;
        }
        segno = (int32)strtol(s, &end_ptr, 0); // allows for octal, decimal and hex
        
        if (end_ptr == s)
        {
            fprintf(stderr, "sim_load(): No segment # was found\n");
            return SCPE_FMT;
        }
        
        ldaddr = (word24)strtol(s2, &end_ptr, 0); // allows for octal, decimal and hex
        
        if (end_ptr == s2)
        {
            fprintf(stderr, "sim_load(): No load address was found\n");
            return SCPE_FMT;
        }
        if (n == 4 && strmask(strlower(sDef), "def*"))
            bDeferred = true;     
    }
    else
    // Syntax load file.oct deferred
    if (flag == 0 && strlen(cptr) && strmask(strlower(cptr), "def*"))
        bDeferred = true;
    
    // syntax: load file.oct as segment xxx deferred
    else if (flag == 0 && strlen(cptr) && strmask(strlower(cptr), "as*"))
    {
        char s[1024], *end_ptr, sn[1024], def[1024];
        
        long n = sscanf(cptr, "%*s %s %s %s", s, sn, def);

        if (!strmask(s, "seg*"))
        {
            fprintf(stderr, "sim_load(): segment keyword not found\n");
            return SCPE_ARG;
        }
        
        segno = (int32)strtol(sn, &end_ptr, 0); // allows for octal, decimal and hex
        if (end_ptr == sn)
        {
            fprintf(stderr, "sim_load(): No segment # was found\n");
            return SCPE_ARG;
        }
        
        if (strmask(def, "def*"))
            bDeferred = true;
        else
        {
            fprintf(stderr, "sim_load(): deferred keyword not found\n");
            return SCPE_ARG;
        }
        
        // check for already loaded segment 'segno'
        segment *sg;
        DL_FOREACH(segments, sg)
        {
            if (sg->segno == segno)
            {
                fprintf(stderr, "sim_load(): segment %d already loaded\n", segno);
                return SCPE_ARG;
            }
        }
    }
    
    // process any special directives from assembly
    scanDirectives(fileref, bDeferred, bVerbose);
    
    switch (fmt) {                                          /* case fmt */
        case FMT_O:                                         /*!< OCT */
            return load_oct (fileref, segno, ldaddr, bDeferred, bVerbose);
            break;
            //case FMT_S:                                         /*!< SAV */
            //  return load_sav (fileref);
            //    break;
            //case FMT_E:                                         /*!< EXE */
            //   return load_exe (fileref);
            //    break;
    }
    
    printf ("Can't determine load file format\n");
    return SCPE_FMT;
}



