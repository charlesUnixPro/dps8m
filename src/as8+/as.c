/*
 Copyright 2012-2015 by Harry Reed

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later. 
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file as.c
 * \project as8
 * \author Harry Reed
 * \date 1/8/13
*/

#include "as.h"

extern bool verbose;


word18 addr = 0;

word18 linkAddr = 0;    // address of linkage section in this assembly
int linkCount = 0;      // # of segrefs (links) for this assembly

int nPass = -1;

outRec *as8output = NULL;

int pass(int n, FILE *src, FILE *oct)
{
    if (verbose)
        fprintf(stderr, "Pass: %d\n", n);
        
    yylineno = 0;
    addr = 0; /* xxx */
    nPass = n;
    bInhibit = false;   // allow for interrupts
    
    return yyparse();
}
    
char *srcFile = "";

extern char *outFile;  ///< NULL;

int asMain(int argc, char **argv)
{
    FILE *src, *oct;

    src = fopen(argv[0], "r");
    if (!src)
    {
        fprintf(stderr, "cannot open %s for reading\n", argv[0]);
        exit(1);
    }
    
    inFile = argv[0];   // for LEXgets/c
    srcFile = strdup(inFile);
    
    char *bnIn = strdup(basename(srcFile));
    char *bnOut = strdup(basename(outFile));
    
    if (strcmp(bnIn, bnOut) == 0)
    {
        fprintf(stderr, "output file <%s> *may* over write input file <%s>.\n", bnIn, bnOut);
        exit(1);        
    }
    
    oct = fopen(outFile, "w");
    if (!oct)
    {
        fprintf(stderr, "cannot open %s for writing\n", outFile);
        exit(1);
    }
    
    initInstructionTable();

    initSymtab();
    initliteralPool();

    // pass 1 - just build symbol table - more. or less
    pass(1, src, oct);
    
    if (yyErrorCount)
    {
        fprintf(stderr, "%d Error%s detected.\n", yyErrorCount, yyErrorCount == 1 ? "" : "s");
        fclose(oct);
        fclose(src);
        return -1;
    }
    rewind(src);
    
    doInterpass(oct);   // do any special interpass processing

    // pass 2 - do code generation
    //dumpEntrySequences();
    
    pass(2, src, oct);
    
    //dumpEntrySequences();
    
    if (yyErrorCount)
        fprintf(stderr, "%d Error%s detected.\n", yyErrorCount, yyErrorCount == 1 ? "" : "s");
    
    doPostpass(oct);   // do any special post-pass processing

    if (verbose)
    {
        dumpSymtab(true);
        dumpliteralPool();
        //dumpextRef();
    }
    
    dumpOutput(oct);
    
    fclose(oct);
    fclose(src);
    
    //exit(0);
    return 0;
}

/**
 * do special interpass processing ....
 */

extern char *segName;

extern word18 addr;
extern word18 linkAddr;
extern int linkCount;

extern int nTotalLiteralWords;  // how many word the literals take up
extern int nCurrentLitAddress;

void doInterpass(FILE *out)
{
    // perform end of pass 1 processing ...
    
    // make literal signature
    //makeLiteralSignatures();
    
    // generate any entry sequences
    addr += fillinEntrySequences();
    
    //dumpEntrySequences();
    
    // fill in literals
    fillLiteralPool();
    
    // fill in temporaries
    fillinTemps();
    
    // fill in ITS/link pairs
    fillExtRef();
    
    //    nCurrentLitAddress = addr;
    //
    //    addr += nTotalLiteralWords;
    
    // write out info directive(s)...
    outas8Direct("info");
    
    // write size of segment
    if (debug) fprintf(stderr, "!SIZE %06o\n", addr);
    outas8Direct("size", addr);
    
    // write segment name (if any)
    if (segName)
    {
        if (debug) fprintf(stderr, "!NAME %s\n", segName);
        outas8Direct("name", segName);
    }
    //dumpEntrySequences();
    
    // write segdefs
    emitSegdefs();
    
    // write entry sequences (if any)
    emitEntryDirectives();
    
    // write segment references (if any)
    emitSegrefs();
    
    //if (linkCount)
    //    fprintf(out, "!LINKAGE %06o %06o\n", linkAddr, linkCount);
    
    // emit any SEGMENT directive stuff
    //emitSegment(out);
    
    // emit any !GO directive
    //emitGo(out);
    
    //dumpEntrySequences();
    
}

void doPostpass(FILE *out)
{
    writeEntrySequences();
    
    checkLiteralPool();
    
    writeLiteralPool();
    
    writeSegrefs();
    
    //   writeExtRef(out, &addr);
}

