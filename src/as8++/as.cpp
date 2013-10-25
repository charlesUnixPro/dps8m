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
    pass(1, src, NULL);
    
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
    pass(2, src, NULL);
    
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

