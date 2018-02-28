/*
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file dps8_loader.c
 * \project dps8
 * \date 11/14/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_cpu.h"
#include "dps8_loader.h"
#include "dps8_utils.h"

#include "utlist.h" // for linked list ops

#ifndef SCUMEM
/* Master loader */
#define FMT_O   1 // .oct
#define FMT_S   2 // .sav
#define FMT_E   3 // .exe
#define FMT_SEG 4
#define FMT_9   9

static t_stat load_oct (FILE *fileref, int32 segno, int32 ldaddr, bool bDeferred, bool bVerbose);
static t_stat load_simh (FILE * fileref, int32 segno, int32 ldaddr, 
                         bool bDeferred, bool bVerbose);
static t_stat loadUnpagedSegment(int segno, word24 addr, word18 count);

#ifndef QUIET_UNUSED
bool bdeferLoad = false;    // defer load to after symbol resolution
#endif

static segdef *newSegdef(char *sym, int val)
{
    segdef *p = calloc(1, sizeof(segdef));
    p->symbol = strdup(sym);
    p->value = val;
    
    p->next = NULL;
    p->prev = NULL;
    
    return p;
}

static void freeSegdef(segdef *p)
{
    if (p->symbol)
        free(p->symbol);
    p->symbol = NULL;
}


static segref *newSegref(char *seg, char *sym, int val, int off)
{
    segref *p = calloc(1, sizeof(segref));
    if (seg && strlen(seg) > 0)
        p->segname = strdup(seg);
    if (sym && strlen(sym) > 0)
        p->symbol = strdup(sym);
    p->value = val;     // address in segment to put ITS pair
    p->offset = off;
    
    p->snapped = false; // not snapped yet
    
    p->segno = -1;

    p->next = NULL;
    p->prev = NULL;
    
    return p;
}

static void freeSegref(segref *p)
{
    if (p->segname)
        free(p->segname);
    p->segname = NULL;
    if (p->symbol)
        free(p->symbol);
    p->symbol = NULL;
}

static segment *newSegment(char *name, int size, bool bDeferred)
{
    segment *s = calloc(1, sizeof(segment));
    if (name && strlen(name) > 0)
        s->name = strdup(name);
    if (bDeferred && size > 0)
        s->M = calloc(1, sizeof(word36) * (unsigned long) size);
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
    
    s->filename = NULL;

    s->next = NULL;
    s->prev = NULL;
    
    return s;
}


static void freeSegment(segment *s)
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
    if (s->filename)
        free(s->filename);
}

static segment *segments = NULL;   // segment for current load unit
static segment *currSegment = NULL;

// remove segment from list of deferred segments

int removeSegment(char *seg)
{
    segment *el, *tmp;
    
    if (strcmp(seg, "*") == 0)  // all?
    {
        DL_FOREACH_SAFE(segments, el, tmp)
        {
            sim_printf("Removing segment '%s'...\n", el->name);
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
                sim_printf("Removing segment '%s'...\n", el->name);
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
                    sim_printf("Removing segdef '%s$%s'...\n", sel->name, del->symbol);

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
                        sim_printf("Removing segdef '%s$%s'...\n", sel->name, del->symbol);

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
                    sim_printf("Removing segref '%s$%s'...\n", sel->name, del->symbol);

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
                        sim_printf("Removing serdef '%s$%s'...\n", sel->name, del->symbol);
   
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


static int objSize = -1;

static int segNamecmp(segment *a, segment *b)
{
    if (*a->name && *b->name)
      return strcmp(a->name, b->name);
    if (*a->name)
      return 1;
    return -1;
}

static int segdefNamecmp(segdef *a, segdef *b)
{
    return strcmp(a->symbol, b->symbol);
}

#ifndef QUIET_UNUSED
static int segrefNamecmp(segref *a, segref *b)
{
    return strcmp(a->symbol, b->symbol);
}
#endif 

#ifndef QUIET_UNUSED
segment *findSegment(char *segname)
{
    segment *sg;
    DL_FOREACH(segments, sg)
        if (!strcmp(sg->name, segname))
            return sg;
    return NULL;
}
#endif

segment *findSegmentNoCase(char *segname)
{
    segment *sg;
    DL_FOREACH(segments, sg)
    if (!strcasecmp(sg->name, segname))
        return sg;
    return NULL;
}

#ifndef QUIET_UNUSED
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
#endif
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

static void makeITS(int segno, int offset, int tag, word36 *Ypair)
{
    word36 even = 0, odd = 0;
    
    //word36 segoff = (bitfieldExtract36(Ypair[1], 18, 18) + offset) & AMASK;
    word36 segoff = (getbits36_18 (Ypair[1], 0) + (word18) offset) & AMASK;
    
    even = ((word36)segno << 18) | 043;
  //odd = (word36)(offset << 18) | (tag & 077);
    odd = (word36)(segoff << 18) | (tag & 077);
    
    Ypair[0] = even;
    Ypair[1] = odd;
}

// Assumes unpaged DSBR

sdw0_s *fetchSDW (word15 segno)
  {
    word36 SDWeven, SDWodd;
    
    core_read2 ((cpu.DSBR.ADDR + 2u * segno) & PAMASK, & SDWeven, & SDWodd,
                 __func__);
    
    // even word
    
    sdw0_s *SDW = & cpu._s;
    memset (SDW, 0, sizeof (cpu._s));
    
    SDW->ADDR = (SDWeven >> 12) & 077777777;
    SDW->R1 = (SDWeven >> 9) & 7;
    SDW->R2 = (SDWeven >> 6) & 7;
    SDW->R3 = (SDWeven >> 3) & 7;
    SDW->DF = TSTBIT (SDWeven, 2);
    SDW->FC = SDWeven & 3;
    
    // odd word
    SDW->BOUND = (SDWodd >> 21) & 037777;
    SDW->R = TSTBIT (SDWodd, 20);
    SDW->E = TSTBIT (SDWodd, 19);
    SDW->W = TSTBIT (SDWodd, 18);
    SDW->P = TSTBIT (SDWodd, 17);
    SDW->U = TSTBIT (SDWodd, 16);
    SDW->G = TSTBIT (SDWodd, 15);
    SDW->C = TSTBIT (SDWodd, 14);
    SDW->EB = SDWodd & 037777;
    
    return SDW;
  }
#endif // ndef SCUMEM

#ifndef SCUMEM
int getAddress(int segno, int offset)
{
    // XXX Do we need to 1st check SDWAM for segment entry?
    
    
    // get address of in-core segment descriptor word from DSBR
    sdw0_s *s = fetchSDW ((word15) segno);
    
    return (s->ADDR + (word18) offset) & 0xffffff; // keep to 24-bits
}
#endif // !SCUMEM

#ifndef SCUMEM
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
        
        sdw0_s *s0 = fetchSDW ((word15) segno);
        int startAddr = (int) s0->ADDR;   // get start address
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

//static
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

//static
//void writeSDW(int segno, sdw0_s *s0)
//{
//    int addr = DSBR.ADDR + (2 * segno);
//    
//    // write a sdw0_s to memory
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

static const int StartingSegment = 8;

int resolveLinks(bool bVerbose)
{
    int segno = StartingSegment - 1;//-1;     // current segment number
    
    if (bVerbose) sim_printf("Examining segments ... ");

    // determine maximum segment no 
    segment *sg1;
    DL_FOREACH(segments, sg1)
        segno = max(segno, sg1->segno);
    segno += 1; // ... and one more. This will be our starting segment number.
    
    if (bVerbose) sim_printf("segment numbering begins at: %d\n", segno);
    
    DL_FOREACH(segments, sg1)
    {
        if (bVerbose) sim_printf("    Processing segment %s...\n", sg1->name);

        if (sg1->segno == -1)
            sg1->segno = segno ++;  // assign segment # to segment
     
        //printf("(assigned as segment #%d)\n", sg1->segno);
        
        segref *sr;
        DL_FOREACH(sg1->refs, sr)
        {
            if (bVerbose) sim_printf("        Resolving segref %s$%s...", sr->segname, sr->symbol);
            
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
                        if (bVerbose)
                        {
                        if (sr->offset)
                            sim_printf("found %s%+d (%06o)\n", sr->symbol, sr->offset, sr->value);
                        else
                            sim_printf("found %s (%06o)\n", sr->symbol, sr->value);
                        }
                        word36 *Ypair = &sg1->M[sr->value];
                        makeITS(sg2->segno, sd->value, 0, Ypair);   // "snap" link for segref in sg1
                        if (bVerbose) sim_printf("            ITS Pair: [even:%012"PRIo64", odd:%012"PRIo64"]\n", Ypair[0], Ypair[1]);
                    }
                }
            }
            if (bVerbose && bFound == false)
                sim_printf("not found\n");
        }
    }
    
    return 0;
}

static int loadDeferredSegment(segment *sg, int addr24)
{
    if (!sim_quiet) sim_printf("    loading %s as segment# 0%o\n", sg->name, sg->segno);
        
    int segno = sg->segno;
        
    word18 segwords = (word18) sg->size;
    
    memcpy((void *) M + addr24, sg->M, (unsigned long) sg->size * sizeof(word36));
    
    cpu . DSBR.BND = 037777;  // temporary max bound ...
    
    if (loadUnpagedSegment(segno, (word24) addr24, segwords) == SCPE_OK)
    {
        if (!sim_quiet) sim_printf("      %d (%06o) words loaded into segment %d (%o) at address %06o\n", segwords, segwords, segno, segno, addr24);
    }
    else
    {
        if (!sim_quiet) sim_printf("      Error loading segment %d (%o)\n", segno, segno);
    }
    // update in-code SDW to reflect segment info
    // Done in loadUnpagedSegment()
    
//    sdw0_s *s0 = fetchSDW(segno);
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
    if (cpu . DSBR.ADDR == 0) // DSBR *probably* not initialized. Issue warning and ask....
        if (!get_yn ("DSBR *probably* uninitialized (DSBR.ADDR == 0). Proceed anyway [N]?", FALSE))
            return -1;

    if (bVerbose) sim_printf("Loading deferred segments ...\n");
    
    int ldaddr = (int) cpu.DSBR.ADDR + 65536;     // load segments *after* SDW table
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

        // set PR4/7 to point to LOT
        if (strcmp(sg->name, LOT) == 0)
        {
            //cpu.PR[4].BITNO = 0;
            //cpu.PR[4].CHAR = 0;
            SET_PR_BITNO (4, 0);
            cpu.PR[4].SNR = (word15) segno;
            cpu.PR[4].WORDNO = 0;
            
            cpu.PR[5] = cpu . PR[4];
            
            int n = 4;
            if (bVerbose) sim_printf("LOT => PR[%d]: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\n", n, cpu . PR[n].SNR, cpu . PR[n].RNR, cpu . PR[n].WORDNO, GET_PR_BITNO (n));
            n = 5;
            if (bVerbose) sim_printf("LOT => cpu . PR[%d]: SNR=%05o RNR=%o WORDNO=%06o BITNO:%02o\n", n, cpu . PR[n].SNR, cpu . PR[n].RNR, cpu . PR[n].WORDNO, GET_PR_BITNO(n));

        }
        
        // bump next load address to a 16-word boundary
        //if (sg->ldaddr == -1)
        //{
            word18 segwords = (word18) sg->size;
            ldaddr += (int) segwords;
            if (ldaddr % 16)
                ldaddr += 16 - (ldaddr % 16);
        //}
        maxSegno = max2(maxSegno, segno);
    }

    // adjust DSBR.BND to reflect highest segment address
    cpu.DSBR.BND = (word14) ((2 * maxSegno) / 16);

    return 0;
}

/*
 * create a linkage Offset Table segment ...
 */
t_stat createLOT(UNUSED bool bVerbose)
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
    segment *lot = newSegment(LOT, 07777, true);
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
    
    if (!sim_quiet) sim_printf("%s segment created with %d sparse entries.\n", LOT, numSeg);
    
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
        sim_printf("snapLOT(): segment '%s' not found!\n", LOT);
        return SCPE_UNK;
    }
    
    // if we get here we're free to fill in lot$ segment ...
        
    // Now go through each segment getting the linkage address and filling in the LOT table with the address (in sprn/lprn packed pointer format)
    
    if (bVerbose) sim_printf("snapping %s links...\n", LOT);
    
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
            
            //pp = bitfieldInsert36(pp, s->linkOffset, 0, 18);    // link address (0-based offset)
            //pp = bitfieldInsert36(pp, s->segno, 18, 12);        // 12-bit(?) segment #
            putbits36_18 (& pp, 18, (word18) s->linkOffset);
            putbits36_15 (& pp, 3, (word15) s->segno);
            //lot->M[lot->ldaddr + s->segno] = pp & DMASK;
            M[lot->ldaddr + s->segno] = pp & DMASK; // LOT is in-core

            if (bVerbose)
                printf("\t%o + %o => %012"PRIo64"\n", lot->ldaddr, s->segno, pp);
                //sim_printf(".");
        }
    }
    //if (bVerbose) sim_printf("\n");
//    sim_printf("Dumping _lot ....\n");
//    for(int n = 0 ; n < 0777777 + 1; n += 1)
//    {
//        word36 c = M[lot->ldaddr + n]; // LOT is in-core
//        if (c)
//        {
//            sim_printf("%06o %012"PRIo64"\n", n, c);
//        }
//
//    }
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
    
    cpu.DSBR.STACK = (word12) stk->segno >> 3;
    
    DL_APPEND(segments, stk);
    
    if (bVerbose) sim_printf("%s segment created as segment# %d (%o) [DSBR.STACK=%04o]\n", name, stk->segno, stk->segno, cpu . DSBR.STACK);
    
    return SCPE_OK;
}

#ifndef QUIET_UNUSED
/*
 * setup faux execution environment ...
 */
t_stat setupFXE()
{
    return SCPE_OK;
}
#endif

/*!
 * scan & process source file for any !directives that need to be processed, e.g. !segment, !go, etc....
 */
static t_stat scanDirectives(FILE *f, const char * fnam, bool bDeferred, 
                             UNUSED bool bVerbose)
{
    long curpos = ftell(f);
    
    rewind(f);

    objSize = -1;
    currSegment = NULL;
    
    char buff[1024];
    while(fgets(buff, sizeof(buff), f) != NULL)
    {
        if (buff[0] != '!')
            continue;
        
        char args[4][256];
        memset(args, 0, sizeof(args));
        
        sscanf(buff, "%s %s %s %s", args[0], args[1], args[2], args[3]);

        // process !segment (depreciate?)
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
            cpu.PPR.IC = (word18) addr & AMASK;
            
            if (cpu.PPR.IC)
                sim_printf("!GO address: %06lo\n", addr);
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
            s -> filename = strdup (fnam);
   
            // see if segment already exists
            if (segments)
            {
                segment *elt;
                
                DL_SEARCH(segments, elt, s, segNamecmp);
                
                if (elt)
                {
                    sim_printf("segment '%s' already loaded. Use 'segment remove'\n", elt->name);
                    freeSegment(s);
                    continue;
                }
            }
            // ... and add it to the listt
            DL_APPEND(segments, s);
            currSegment = s;
            
            if (!sim_quiet) sim_printf("segment created for '%s'\n", s->name);
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
            if (!currSegment)
            {
                sim_printf("Oops! No 'currentSegment'. Have you perhaps forgotten a 'name' directive?");
                return SCPE_UNK;
            }
            
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
                    sim_printf("symbol '%s' already loaded for segment '%s'. Use 'segment segdef remove'\n", elt->symbol, currSegment->name);
                    freeSegdef(s);
                    continue;
                }
            }
            DL_APPEND(currSegment->defs, s);
            
            if (!sim_quiet) sim_printf("segdef created for segment %s, symbol '%s', addr:%06o\n", currSegment->name, symbol, value);
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
                    sim_printf("segdef/entrypoint '%s' already found for segment '%s'. Use 'segment segdef remove'\n", elt->symbol, currSegment->name);
                    freeSegdef(s);
                    continue;
                }
            }
            DL_APPEND(currSegment->defs, s);
            
            if (!sim_quiet) sim_printf("entrypoint created for segment %s, symbol '%s', addr:%06o\n", segments->name, symbol, value);
        }
        
        else

        // process !segref segname symbol
        //if (bDeferred && strcasecmp(args[0], "!segref") == 0)
        if (strcasecmp(args[0], "!segref") == 0)
        {
            char segment[256], symbol[256];
            int addr, offset = 0;
            
            sscanf(buff, "%*s %s %s %i %d", segment, symbol, &addr, &offset);
            
            // XXX a ? is treated same as segment$segment
            if (strcmp(symbol, "?") == 0)
                strcpy(symbol, segment);
            
            segref *s = newSegref(segment, symbol, addr, offset);
            
            // see if segref already exists
//            if (currSegment->refs)
//            {
//                segref *elt;
//                
//                DL_SEARCH(currSegment->refs, elt, s, segrefNamecmp);
//                
//                if (elt)
//                {
//                    sim_printf ("symbol '%s' already referenced in segment '%s'. Use 'segment segref remove'\n", elt->symbol, currSegment->name);
//                    freeSegref(s);
//                    continue;
//                }
//            }
            DL_APPEND(currSegment->refs, s);
            if (!sim_quiet)
            {
                if (offset)
                    sim_printf("segref created for segment '%s' symbol:%s%+d, addr:%06o\n", segment, symbol, offset, addr);
                else
                    sim_printf("segref created for segment '%s' symbol:%s, addr:%06o\n", segment, symbol, addr);
            }
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
static t_stat load_oct (FILE *fileref, int32 segno, int32 ldaddr, 
                        bool bDeferred, UNUSED bool bVerbose)
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
            
            int n = sscanf(c, "%o %*s %"PRIo64"", &maddr, &data);
            if (n == 2)
            {
                if ((int) maddr > currSegment->size)
                {
                    sim_printf("ERROR: load_oct(deferred): attempted load into segment %s location %06o (max %06o)\n", currSegment->name, maddr, currSegment->size);
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
        
        if (!sim_quiet) sim_printf("%d (%06o) words loaded into segment %s%s\n", words, words, currSegment->name, buff);
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
            
            int n = sscanf(c, "%o %*s %"PRIo64"", &maddr, &data);
            if (n == 2)
            {
                if (currSegment && currSegment->M == NULL)
                    currSegment->M = (word36 *) &M[maddr];

                if (maddr > MEMSIZE)
                    return SCPE_NXM;
                else
                    M[maddr+(unsigned int) ldaddr] = data & DMASK;
                words++;
            }
        }
        if (!sim_quiet) sim_printf("%d (%06o) words loaded\n", words, words);
    }
    else
    {
        // load data into segment segno @ M[ldaddr]
        word24 maxaddr = 0;
        
        while((c = fgets(buff, sizeof(buff), fileref)) != NULL)
        {
            if (buff[0] == '!' || buff[0] == '*')
                continue;
            
            word24 maddr;       ///< 24-bits
            word36 data;        ///< 36 bits
            
            int n = sscanf(c, "%o %*s %"PRIo64"", &maddr, &data);
            if (n == 2)
            {
                if (currSegment && currSegment->M == NULL)
                {
                    currSegment->M = (word36 *) &M[ldaddr];
                    currSegment->segno = segno;
                }
                if (maddr > MEMSIZE)
                    return SCPE_NXM;
                else
                    M[(unsigned int) ldaddr + maddr] = data & DMASK;
                //sim_printf ("laddr:%d maddr:%d\n", maddr, maddr);
                words++;
                maxaddr = max(maddr, maxaddr);
            }
        }
        word18 segwords = (objSize == -1) ? maxaddr + 1 : (word18) objSize;  // words in segment
        //sim_printf ("segwords:%d maxaddr:%d\n", segwords, maxaddr);
        if (loadUnpagedSegment(segno, (word24) ldaddr, segwords) == SCPE_OK)
        {
            if (!sim_quiet) sim_printf("%d (%06o) words loaded into segment %d(%o) at address %06o\n", words, words, segno, segno, ldaddr);
        }
        else
        {
            if (!sim_quiet) sim_printf("Error loading segment %d (%o)\n", segno, segno);
        }
    }
    
    return SCPE_OK;
}

//
// "simh" simh/dps8 loader (.seg files) ....
//

static t_stat load_simh (FILE *fileref, int32 segno, int32 ldaddr, 
                         bool bDeferred, UNUSED bool bVerbose)
  {
    char buff[132] = "";
    int fno = fileno (fileref);
    lseek (fno, 0, SEEK_SET);

    // 72 bits at a time; 2 dps8m words == 9 bytes
    uint8 bytes [9];

    int words = 0;
    
    if (bDeferred)  // a deferred segment load
    {
        word24 maddr = 0;
        while (read (fno, bytes, 9))
          {
            word36 w1 = extr36 (bytes, 0);
            word36 w2 = extr36 (bytes, 1);

            if ((int) maddr + 2 > currSegment->size)
              {
                sim_printf("ERROR: load_bin(deferred): attempted load into segment %s location %06o (max %06o)\n", currSegment->name, maddr, currSegment->size);
                return SCPE_NXM;
              }
            currSegment->M[maddr++] = w1 & DMASK;
            currSegment->M[maddr++] = w2 & DMASK;
            words += 2;
          }
        currSegment->ldaddr = ldaddr;
        currSegment->segno = segno;
        currSegment->deferred = true;
        if (segno != -1)
            sprintf(buff, " as segment %d", segno);
        else
            strcpy(buff, "");
        
        if (!sim_quiet) sim_printf("%d (%06o) words loaded into segment %s%s\n", words, words, currSegment->name, buff);
      }
    else if (segno == -1)// just do an absolute load
      {
        word24 maddr = 0;
        while (read (fno, bytes, 9))
          {
            word36 w1 = extr36 (bytes, 0);
            word36 w2 = extr36 (bytes, 1);

            if (currSegment && currSegment->M == NULL)
              currSegment->M = (word36 *) &M[maddr];

            if (maddr > MEMSIZE)
              return SCPE_NXM;

            M [maddr + (unsigned int) ldaddr] = w1 & DMASK;
            maddr ++;
            words++;
            M [maddr + (unsigned int) ldaddr] = w2 & DMASK;
            maddr ++;
            words++;
          }
        if (!sim_quiet) sim_printf("%d (%06o) words loaded\n", words, words);
    }
    else
    {
        word24 maxaddr = 0;
        
        word24 maddr = 0;
        while (read (fno, bytes, 9))
          {
            word36 w1 = extr36 (bytes, 0);
            word36 w2 = extr36 (bytes, 1);

            if (currSegment && currSegment->M == NULL)
              currSegment->M = (word36 *) &M[maddr];

            if (maddr > MEMSIZE)
              return SCPE_NXM;

            M [maddr + (unsigned int) ldaddr] = w1 & DMASK;
            maddr ++;
            words++;
            M [maddr + (unsigned int) ldaddr] = w2 & DMASK;
            maxaddr = maddr;
            maddr ++;
            words++;
          }
        word18 segwords = (objSize == -1) ? maxaddr + 1 : (word18) objSize;  // words in segment
        //sim_printf ("segwords:%d maxaddr:%d\n", segwords, maxaddr);
        if (loadUnpagedSegment(segno, (word24) ldaddr, segwords) == SCPE_OK)
        {
            if (!sim_quiet) sim_printf("%d (%06o) words loaded into segment %d(%o) at address %06o\n", words, words, segno, segno, ldaddr);
        }
        else
        {
            if (!sim_quiet) sim_printf("Error loading segment %d (%o)\n", segno, segno);
        }
    }
    
    return SCPE_OK;
}

/*
 * stuff todo with loading segments ....
 */

/*!
 * Create sdw0. Create an in-core SDW ...
 */
static sdw0_s* createSDW0(word24 addr, word3 R1, word3 R2, word3 R3, word1 F, word3 FC, word14 BOUND, word1 R, word1 E, word1 W, word1 P, word1 U, word1 G, word1 C, word14 EB)
{
    static sdw0_s SDW0;
    
    // even word
    SDW0.ADDR = addr & 077777777;
    SDW0.R1   = R1 & 7;
    SDW0.R2   = R2 & 7;
    SDW0.R3   = R3 & 7;
    SDW0.DF   =  F & 1;
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

static void writeSDW0toYPair(sdw0_s *p, word36 *yPair)
{
    word36 even, odd;
    
    even = (word36) p->ADDR << 12 | (word36) p->R1 << 9 | (word36) p->R2 << 6 | (word36) p->R3 << 3 | (word36) p->DF << 2 | (word36) p->FC;
    
    odd = (word36) p->BOUND << 21 | (word36) p->R << 20 | (word36) p->E << 19 | (word36) p->W << 18 | (word36) p->P << 17 | (word36) p->U << 16 | (word36) p->G << 15 | (word36) p->C << 14 | (word36) p->EB;
    
    yPair[0] = even;
    yPair[1] = odd;
}


/*!
 * load an unpaged segment into memory. Assume all R/W/X permissions given.
 */
static t_stat loadUnpagedSegment(int segno, word24 addr, word18 count)
{
    if (2 * segno >= 16 * (cpu . DSBR.BND + 1))    // segment out of range
    {
        char msg[256];
        sprintf(msg, "Segment %d is not within DSBR.BND (%d) Adjust [Y]?", segno, cpu . DSBR.BND);
        if (get_yn (msg, TRUE) == 0)    // No, don't adjust
            return SCPE_MEM;
        
        cpu . DSBR.BND = (word14) (2 * (segno)) >> 4;
        //sim_printf ("DSBR.BND set to %o\n", cpu . DSBR.BND);
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
    const word14 EB = (word14) count;    ///< Any call into this segment must be to an offset less than EB if G=0
    
    //sim_printf ("B:%d count:%d\n", BOUND, count);
    
    sdw0_s *s = createSDW0(addr, R1, R2, R3, F, FC, BOUND, R, E, W, P, U, G, C, EB);
    word36 yPair[2];
    writeSDW0toYPair(s, yPair);
    
    word24 sdwaddress = cpu.DSBR.ADDR + (word24) (2 * segno);
    if (!sim_quiet) sim_printf("Writing SDW to address %08o (DSBR.ADDR+2*%d offset) \n", sdwaddress, segno);
    // write sdw to segment table
    core_write2(sdwaddress, yPair[0], yPair[1], __func__);
    
    return SCPE_OK;
}

// Warning: returns ptr to static buffer
char * lookupSegmentAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset)
{
    static char buf [129];
    segment *s;
    DL_FOREACH(segments, s)
    {
        if (s -> segno == (int) segno)
        {
            if (compname)
                * compname = s -> name;
            if (compoffset)
                * compoffset = 0;  
            sprintf (buf, "%s:+0%0o", s -> name, offset);
            return buf;
        }
    }
    return NULL;
}

static t_stat sim_dump (FILE *fileref, UNUSED const char * cptr, UNUSED const char * fnam, 
                 UNUSED int flag)
{
    size_t rc = fwrite ((word36 *) M, sizeof (word36), MEMSIZE, fileref);
    if (rc != MEMSIZE)
    {
        sim_printf ("fwrite returned %ld; expected %d\n", (long) rc, MEMSIZE);
        return SCPE_IOERR;  
    }
    return SCPE_OK;
}
#endif // ndef SCUMEM
// This is part of the simh interface
t_stat sim_load (FILE *fileref, const char *cptr, const char *fnam, int flag)
{
#ifdef SCUMEM
    return SCPE_ARG;
#else
    if (flag)
        return sim_dump (fileref, cptr, fnam, flag);
      
    size_t fmt;
    
    int32 segno = -1;
    int32 ldaddr = 0;
    
    bool bDeferred = false; // a deferred load

    fmt = 0;                                                /* no fmt */
    if (sim_switches & (int32) SWMASK ('O'))                        /* -o? */
        fmt = FMT_O;
    else if (match_ext (fnam, "OCT"))                       /* .OCT? */
        fmt = FMT_O;
    else if (match_ext (fnam, "SEG"))                       /* .OCT? */
        fmt = FMT_SEG;
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
    if (sim_switches & (int32) SWMASK ('V'))                        /* -v? */
        bVerbose = true;

   /*
    * Absolute:
    *  load file.oct
    * Absolute at offset:
    *  load file.oct address ?
    * Unpaged, non-deferred
    *  load file.oct segment ? address ?
    * Unpages, deferred
    *  load file.oct segment ? address ? deferred
    *
    */
    // load file at offset ?
    char ccpy [strlen (cptr)+1];
    strcpy (ccpy, cptr);
    // Syntax load file.oct address addr
    if (flag == 0 && strlen(ccpy) && strmask(strlower(ccpy), "addr*"))
    {
        char s[128], *end_ptr, w[128], s2[128], sDef[128];
        
        strcpy(s, "");
        strcpy(w, "");
        strcpy(s2, "");
        strcpy(sDef, "");
        
        /* long n = */ sscanf(ccpy, "%*s %s", s);
        ldaddr = (int32)strtol(s, &end_ptr, 0); // allows for octal, decimal and hex
        
        if (end_ptr == s)
        {
            sim_printf("sim_load(): No load address was found\n");
            return SCPE_FMT;
        }
    }

    // load file into segment?
    // Syntax load file.oct segment xxx address addr
    else if (flag == 0 && strlen(ccpy) && strmask(strlower(ccpy), "seg*"))
    {
        char s[128], *end_ptr, w[128], s2[128], sDef[128];
        
        strcpy(s, "");
        strcpy(w, "");
        strcpy(s2, "");
        strcpy(sDef, "");
        
        long n = sscanf(ccpy, "%*s %s %s %s %s", s, w, s2, sDef);
        if (!strmask(w, "addr*"))
        {
            sim_printf("sim_load(): No/illegal destination specifier was found\n");
            return SCPE_FMT;
        }
        segno = (int32)strtol(s, &end_ptr, 0); // allows for octal, decimal and hex
        
        if (end_ptr == s)
        {
            sim_printf("sim_load(): No segment # was found\n");
            return SCPE_FMT;
        }
        
        ldaddr = (int32)strtol(s2, &end_ptr, 0); // allows for octal, decimal and hex
        
        if (end_ptr == s2)
        {
            sim_printf("sim_load(): No load address was found\n");
            return SCPE_FMT;
        }
        if (n == 4 && strmask(strlower(sDef), "def*"))
            bDeferred = true;     
    }
    else
    // Syntax load file.oct deferred
    if (flag == 0 && strlen(ccpy) && strmask(strlower(ccpy), "def*"))
        bDeferred = true;
    
    // syntax: load file.oct as segment xxx deferred
    else if (flag == 0 && strlen(ccpy) && strmask(strlower(ccpy), "as*"))
    {
        char s[1024], *end_ptr, sn[1024], def[1024];
        
        sscanf(ccpy, "%*s %s %s %s", s, sn, def);

        if (!strmask(s, "seg*"))
        {
            sim_printf("sim_load(): segment keyword not found\n");
            return SCPE_ARG;
        }
        
        segno = (int32)strtol(sn, &end_ptr, 0); // allows for octal, decimal and hex
        if (end_ptr == sn)
        {
            sim_printf("sim_load(): No segment # was found\n");
            return SCPE_ARG;
        }
        
        if (strmask(def, "def*"))
            bDeferred = true;
        else
        {
            sim_printf("sim_load(): deferred keyword not found\n");
            return SCPE_ARG;
        }
        
        // check for already loaded segment 'segno'
        segment *sg;
        DL_FOREACH(segments, sg)
        {
            if (sg->segno == segno)
            {
                sim_printf("sim_load(): segment %d already loaded\n", segno);
                return SCPE_ARG;
            }
        }
    }
    
    // process any special directives from assembly
    scanDirectives(fileref, fnam, bDeferred, bVerbose);
    
    switch (fmt) {                                          /* case fmt */
        case FMT_O:                                         /*!< OCT */
            return load_oct (fileref, segno, ldaddr, bDeferred, bVerbose);
            // break;

        case FMT_SEG:                                         /*!< OCT */
            return load_simh (fileref, segno, ldaddr, bDeferred, bVerbose);
        //case FMT_S:                                         /*!< SAV */
            //  return load_sav (fileref);
            //    break;

        //case FMT_E:                                         /*!< EXE */
            //   return load_exe (fileref);
            //    break;
    }
    
    sim_printf ("Can't determine load file format\n");
    return SCPE_FMT;
#endif
}

static void printSDW0 (sdw0_s *SDW)
  {
    char buf [256];
    sim_printf ("%s\n", str_SDW0 (buf, SDW));
  }

static char * strDSBR (char * buf)
  {
    sprintf (buf, "DSBR: ADDR=%06o BND=%05o U=%o STACK=%04o",
             cpu.DSBR.ADDR, cpu.DSBR.BND, cpu.DSBR.U, cpu.DSBR.STACK);
    return buf;
  }

static void printDSBR (void)
  {
    char buf [256];
    sim_printf ("%s\n", strDSBR (buf));
  }

#ifndef SCUMEM
static t_stat dpsCmd_DumpSegmentTable (void)
  {
    sim_printf ("*** Descriptor Segment Base Register (DSBR) ***\n");
    printDSBR ();
    if (cpu.DSBR.U)
      {
        sim_printf ("*** Descriptor Segment Table ***\n");
        for (word15 segno = 0; 2 * segno < 16 * (cpu.DSBR.BND + 1); segno += 1)
          {
            sim_printf ("Seg %d - ", segno);
            sdw0_s *s = fetchSDW (segno);
            printSDW0 (s);
          }
      }
    else
     {
        sim_printf ("*** Descriptor Segment Table (Paged) ***\n");
        sim_printf ("Descriptor segment pages\n");
        for (word15 segno = 0; 2 * segno < 16 * (cpu.DSBR.BND + 1);
             segno += 512)
          {
            word24 y1 = (2u * segno) % 1024u;
            word24 x1 = (2u * segno - y1) / 1024u;
            word36 PTWx1;
            core_read ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);

            ptw0_s PTW1;
            PTW1.ADDR = GETHI (PTWx1);
            PTW1.U = TSTBIT (PTWx1, 9);
            PTW1.M = TSTBIT (PTWx1, 6);
            PTW1.DF = TSTBIT (PTWx1, 2);
            PTW1.FC = PTWx1 & 3;
           
            //if (PTW1.DF == 0)
            //    continue;
            sim_printf ("%06o  Addr %06o U %o M %o F %o FC %o\n", 
                        segno, PTW1.ADDR, PTW1.U, PTW1.M, PTW1.DF, PTW1.FC);
            sim_printf ("    Target segment page table\n");
            for (word15 tspt = 0; tspt < 512; tspt ++)
              {
                word36 SDWeven, SDWodd;
                core_read2 (((PTW1.ADDR << 6) + tspt * 2u) & PAMASK,
                             & SDWeven, & SDWodd, __func__);
                sdw0_s SDW0;
                // even word
                SDW0.ADDR = (SDWeven >> 12) & PAMASK;
                SDW0.R1 = (SDWeven >> 9) & 7;
                SDW0.R2 = (SDWeven >> 6) & 7;
                SDW0.R3 = (SDWeven >> 3) & 7;
                SDW0.DF = TSTBIT (SDWeven, 2);
                SDW0.FC = SDWeven & 3;

                // odd word
                SDW0.BOUND = (SDWodd >> 21) & 037777;
                SDW0.R = TSTBIT (SDWodd, 20);
                SDW0.E = TSTBIT (SDWodd, 19);
                SDW0.W = TSTBIT (SDWodd, 18);
                SDW0.P = TSTBIT (SDWodd, 17);
                SDW0.U = TSTBIT (SDWodd, 16);
                SDW0.G = TSTBIT (SDWodd, 15);
                SDW0.C = TSTBIT (SDWodd, 14);
                SDW0.EB = SDWodd & 037777;

                //if (SDW0.DF == 0)
                //    continue;
                sim_printf ("    %06o Addr %06o %o,%o,%o F%o BOUND %06o "
                            "%c%c%c%c%c\n",
                            tspt, SDW0.ADDR, SDW0.R1, SDW0.R2, SDW0.R3,
                            SDW0.DF, SDW0.BOUND, SDW0.R ? 'R' : '.',
                            SDW0.E ? 'E' : '.', SDW0.W ? 'W' : '.',
                            SDW0.P ? 'P' : '.', SDW0.U ? 'U' : '.');
                if (SDW0.U == 0)
                  {
                    for (word18 offset = 0; offset < 16u * (SDW0.BOUND + 1u);
                          offset += 1024u)
                      {
                        word24 y2 = offset % 1024;
                        word24 x2 = (offset - y2) / 1024;

                        // 10. Fetch the target segment PTW(x2) from
                        //     SDW(segno).ADDR + x2.

                        word36 PTWx2;
                        core_read ((SDW0.ADDR + x2) & PAMASK, & PTWx2,
                                    __func__);

                        ptw0_s PTW_2;
                        PTW_2.ADDR = GETHI (PTWx2);
                        PTW_2.U = TSTBIT (PTWx2, 9);
                        PTW_2.M = TSTBIT (PTWx2, 6);
                        PTW_2.DF = TSTBIT (PTWx2, 2);
                        PTW_2.FC = PTWx2 & 3;

                         sim_printf ("        %06o  Addr %06o U %o M %o F %o "
                                     "FC %o\n", 
                                     offset, PTW_2.ADDR, PTW_2.U, PTW_2.M,
                                     PTW_2.DF, PTW_2.FC);

                      }
                  }
              }
          }
      }

    return SCPE_OK;
  }
#endif

//! custom command "dump"
t_stat dpsCmd_Dump (UNUSED int32 arg, const char *buf)
  {
    char cmds [256][256];
    memset (cmds, 0, sizeof (cmds));  // clear cmds buffer
    
    int nParams = sscanf (buf, "%s %s %s %s",
                          cmds[0], cmds[1], cmds[2], cmds[3]);
#ifndef SCUMEM
    if (nParams == 2 &&
        ! strcasecmp (cmds[0], "segment") &&
        ! strcasecmp (cmds[1], "table"))
        return dpsCmd_DumpSegmentTable ();
#endif
#ifdef WAM
#ifdef L68
    if (nParams == 1 && !strcasecmp (cmds[0], "sdwam"))
        return dump_sdwam ();
#endif
#endif
    
    return SCPE_OK;
  }

/*
 * initialize segment table according to the contents of DSBR ...
 */

static t_stat dpsCmd_InitUnpagedSegmentTable ()
  {
    if (cpu.DSBR.U == 0)
      {
        sim_printf  ("Cannot initialize unpaged segment table because "
                     "DSBR.U says it is \"paged\"\n");
        return SCPE_OK;    // need a better return value
      }
    
    if (cpu.DSBR.ADDR == 0) // DSBR *probably* not initialized. Issue warning 
                            // and ask....
      {
        if (! get_yn ("DSBR *probably* uninitialized (DSBR.ADDR == 0). "
                      "Proceed anyway [N]?", FALSE))
          {
            return SCPE_OK;
          }
      }
    
    word15 segno = 0;
    while (2 * segno < (16 * (cpu.DSBR.BND + 1)))
      {
        //generate target segment SDW for DSBR.ADDR + 2 * segno.
        word24 a = cpu.DSBR.ADDR + 2u * segno;
        
        // just fill with 0's for now .....
        core_write ((a + 0) & PAMASK, 0, __func__);
        core_write ((a + 1) & PAMASK, 0, __func__);
        
        segno ++; // onto next segment SDW
      }
    
    if ( !sim_quiet)
      sim_printf ("zero-initialized segments 0 .. %d\n", segno - 1);
    return SCPE_OK;
  }

#ifdef WAM
static t_stat dpsCmd_InitSDWAM ()
  {
    for (uint i = 0; i < N_CPU_UNITS_MAX; i ++)
      {
        memset (cpus[i].SDWAM, 0, sizeof (cpu.SDWAM));
      }
    
    if (! sim_quiet)
      sim_printf ("zero-initialized SDWAM\n");
    return SCPE_OK;
  }
#endif

// custom command "init"

t_stat dpsCmd_Init (UNUSED int32 arg, const char *buf)
  {
    char cmds [8][32];
    memset (cmds, 0, sizeof (cmds));  // clear cmds buffer
    
    int nParams = sscanf (buf, "%s %s %s %s",
                          cmds[0], cmds[1], cmds[2], cmds[3]);
    if (nParams == 2 &&
        ! strcasecmp (cmds[0], "segment") &&
        ! strcasecmp (cmds[1], "table"))
        return dpsCmd_InitUnpagedSegmentTable ();
#ifdef WAM
    if (nParams == 1 && !strcasecmp (cmds[0], "sdwam"))
        return dpsCmd_InitSDWAM ();
#endif
    return SCPE_OK;
  }

// custom command "segment" - stuff to do with deferred segments

t_stat dpsCmd_Segment (UNUSED int32  arg, const char *buf)
  {
#ifndef SCUMEM
    char cmds [8][32];
    memset (cmds, 0, sizeof (cmds));  // clear cmds buffer
    
    /*
      cmds   0     1      2     3
     segment ??? remove
     segment ??? segref remove ????
     segment ??? segdef remove ????
     */
    int nParams = sscanf (buf, "%s %s %s %s %s",
                          cmds[0], cmds[1], cmds[2], cmds[3], cmds[4]);
    if (nParams == 2 &&
        ! strcasecmp (cmds[0], "remove"))
        return removeSegment (cmds[1]);
    if (nParams == 4 &&
        ! strcasecmp (cmds[1], "segref") &&
        ! strcasecmp (cmds[2], "remove"))
        return removeSegref (cmds[0], cmds[3]);
    if (nParams == 4 &&
        ! strcasecmp (cmds[1], "segdef") &&
        ! strcasecmp (cmds[2], "remove"))
        return removeSegdef (cmds[0], cmds[3]);
#endif
    return SCPE_ARG;
  }

// custom command "segments" - stuff to do with deferred segments

t_stat dpsCmd_Segments (UNUSED int32 arg, const char *buf)
  {
#ifndef SCUMEM
    bool bVerbose = ! sim_quiet;

    char cmds [8][32];
    memset (cmds, 0, sizeof (cmds));  // clear cmds buffer
    
    /*
     * segments resolve
     * segments load deferred
     * segments remove ???
     */
    int nParams = sscanf (buf, "%s %s %s %s",
                          cmds[0], cmds[1], cmds[2], cmds[3]);
    if (nParams == 1 &&
        ! strcasecmp (cmds[0], "resolve"))
        // resolve external reverences in deferred segments
        return resolveLinks (bVerbose);
   
    if (nParams == 2 &&
        ! strcasecmp (cmds[0], "load") &&
        ! strcasecmp (cmds[1], "deferred"))
        return loadDeferredSegments (bVerbose);    // load all deferred segments
    
    if (nParams == 2 &&
        ! strcasecmp (cmds[0], "remove"))
        return removeSegment (cmds[1]);

    if (nParams == 2 &&
        ! strcasecmp (cmds[0], "lot") &&
        ! strcasecmp (cmds[1], "create"))
        return createLOT (bVerbose);
    if (nParams == 2 &&
        ! strcasecmp (cmds[0], "lot") &&
        ! strcasecmp (cmds[1], "snap"))
        return snapLOT (bVerbose);

    if (nParams == 3 &&
        ! strcasecmp (cmds[0], "create") &&
        ! strcasecmp (cmds[1], "stack"))
      {
        int _n = (int)strtoll (cmds[2], NULL, 8);
        return createStack (_n, bVerbose);
      }
#endif
    return SCPE_ARG;
  }


