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

t_stat load_oct (FILE *fileref, int32 segno, int32 ldaddr, bool bDeferred);

bool bdeferLoad = false;    // defer load to after symbol resolution

struct segdef          // definitions for externally available symbols
{
    char    *symbol;    ///< name of externallay available symbol
    int     value;      ///< address of value in segment
    int     relType;    ///< relocation type (RFU)
    
    struct segdef  *next;
    struct segdef  *prev;
};
typedef struct segdef segdef;

segdef *newSegdef(char *sym, int val)
{
    segdef *p = malloc(sizeof(segdef));
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


struct segref      // references to external symbols in this segment
{
    char    *segname;   ///< name of segment external symbol resides
    char    *symbol;    ///< name of extern symbol
    int     value;      ///< address of ITS pair in segment
    int     relType;    ///< relocation type (RFU)
    
    struct segref  *next;
    struct segref  *prev;
};
typedef struct segref segref;

segref *newSegref(char *seg, char *sym, int val)
{
    segref *p = malloc(sizeof(segref));
    if (seg && strlen(seg) > 0)
        p->segname = strdup(seg);
    if (sym && strlen(sym) > 0)
        p->symbol = strdup(sym);
    p->value = val;

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

struct segment
{
    char    *name;      ///< name of this segment
    word36  *M;         ///< contents of this segment
    int     size;       ///< size of this segment in 36-bit words
    
    segdef *defs;      ///< symbols available to other segments
    segref *refs;      ///< external symbols needed by this segment
    
    int     segno;      ///< segment @ segment is assign
    
    struct segment *next;
    struct segment *prev;

};
typedef struct segment segment;

segment *newSegment(char *name, int size)
{
    segment *s = malloc(sizeof(segment));
    if (name && strlen(name) > 0)
        s->name = strdup(name);
    if (size > 0)
        s->M = malloc(sizeof(word36) * size);
    else
        s->M = NULL;
    s->size = size;
    
    s->defs = NULL;
    s->refs = NULL;
    
    s->segno = -1;
    
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
    
    if (s->M)
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

/*!
 * scan & process source file for any !directives that need to be processed, e.g. !segment, !go, etc....
 */
t_stat scanFile(FILE *f, bool bDeferred)
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
            
        // process !go
        if (strcasecmp(args[0], "!go") == 0)
        {
            long addr = strtol(args[1], NULL, 0);
            rIC = addr & AMASK;
            
            if (rIC)
                fprintf(stderr, "!GO address: %06lo\n", addr);
        }
        
        else
            
        // process !segname name
        if (bDeferred && strcasecmp(args[0], "!segname") == 0)
        {
            if (args[1] == NULL || strlen(args[1]) == 0)
                continue;
            
            // make a new segment
            segment *s = newSegment(args[1], objSize);
           
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
            
        // process !segdef symbol value
        if (bDeferred && strcasecmp(args[0], "!segdef") == 0)
        {
            char symbol[256];
            int value;
            
            sscanf(buff, "%*s %s %i", symbol, &value);
            
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
            
            printf("segdef created for segment %s, symbol'%s' - %o\n", segments->name, symbol, value);
        }

        else
                
        // process !segref segname symbol
        if (bDeferred && strcasecmp(args[0], "!segref") == 0)
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
            printf("segref created for segment '%s' symbol:%s addr:%o\n", segment, symbol, addr);
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
t_stat load_oct (FILE *fileref, int32 segno, int32 ldaddr, bool bDeferred)
{
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
                    printf("ERROR: load_oct(bDeferred): attempted load into segment %s location %06o (max %06o)\n", currSegment->name, maddr, currSegment->size);
                    return SCPE_NXM;
                }
                else
                    currSegment->M[maddr] = data & DMASK;
                words++;
            }
        }
        printf("%d (%06o) words loaded into segment %s\n", words, words, currSegment->name);

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
    fprintf(stderr, "Writing SDW to %o\n", sdwaddress);
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
    
    // load file into segment?
    // Syntax load file.oct segment xxx address addr
    if (flag == 0 && strlen(cptr) && strmask(strlower(cptr), "seg*"))
    {
        char s[1024], *end_ptr, w[1024], s2[1024];
        
        long n = sscanf(cptr, "%*s %s %s %s", s, w, s2);
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
    }
    else
    // Syntax load file.oct deferred
    if (flag == 0 && strlen(cptr) && strmask(strlower(cptr), "def*"))
        bDeferred = true;
    
    
    // process any special directives from assebmly
    scanFile(fileref, bDeferred);
    

    switch (fmt) {                                          /* case fmt */
        case FMT_O:                                         /*!< OCT */
            return load_oct (fileref, segno, ldaddr, bDeferred);
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



