/*
 Copyright 2012-2013 by Harry Reed

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later. 
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file as.h
 * \project as8
 * \author Harry Reed
 * \date 9/29/12
 *  Created by Harry Reed on 9/29/12.
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#ifndef as_as_h
#define as_as_h

#include "as.h"

#define AS8         ///< we're compiling code for the assebler .....

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

#include <getopt.h>

#include <libgen.h>     // for basename

#include "dps8.h"

#define EMULATOR_ONLY  1    ///< for emcall instruction, etc ...

extern bool debug, verbose;
extern char *inFile;

void dumpextRef();
void writEextRef(FILE *oct, word18 *addr);
void fillExtRef(word18 *addr);

/* ***** symbol table stuff *****/

struct symtab
{
	char *name;	///< symbol name
	word18 value;
    char *segname;  ///< is symbol references an external symbol in another segment
    char *extname;  ///< name of symbol in external segment
    //linkPair *ext;    ///< if symbol refers to an external reference
    
} Symtab  [2048];
typedef struct symtab symtab;

void initSymtab();
symtab* getsym(const char *sym);
struct symtab *addsym(const char *sym, word18 value);
void dumpSymtab();

void IEEElongdoubleToYPair(long double f0, word36 *Ypair);


enum eLiteralType {
    litUnknown = 0,
    litGeneric,     ///< just a generic literal representing an address
    litSingle,      ///< single precision fp literal
    litDouble,      ///< double precision fp literal
    litScaledFixed  ///< scaled, fix-point literal
};
typedef enum eLiteralType eLiteralType;

struct literal
{
    char *name; ///< string of literal    ///< name of literal
    word36 value;
    long double f;      ///< not floating-point literals
    word18 srcAddr;     ///< where literal is defined in assembly
    word18 addr;
    
    eLiteralType litType;   ///< what kind of literal are we???
    char *text;         ///< text of literal
    
} literalPool[2048];
typedef struct literal literal;

void initliteralPool();
literal *getliteral(word18 srcAddr);
literal *addliteral(word18 srcAddr, word18 dstAddr, eLiteralType litType, char *text);
void dumpliteralPool();
literal *doLiteral(char *arg1, word18 addr);
void fillLiteralPool(word18 *addr);
void writeLiteralPool(FILE *oct, word18 *addr);
word18 get18(char *arg, int dudl);

#define LITERAL_FMT     "Lit%06o"     ///< format used to generate literals
#define FLITERAL_FMT    "FLit%06o"    ///< format used to generate single-precision fp literals
#define DLITERAL_FMT    "DLit%06o"    ///< format used to generate double-precision fp literals
#define SCFIXPNT_FMT    "SLit%06o"    ///< format used to generate scaled, fixed-point literals



// pseudo op stuff ...
word18 getOpcode(const char *op);   ///< , const char *arg1, const char *arg2);
opCode *getOpcodeEntry(const char *op);

// Multiword EIS stuff ...
word36 parseMwEIS(opCode *op, char *args);

word36 doDescn(char *arg1, char *arg2, int n);
word36 doDescBs(char *arg1, char *arg2); // XXX probably needs work. See comments.

word18 encodePR(char *arg1, bool *bit29);


struct pseudoOp
{
    char *name;     ///< name of pseudo op
    int nargs;      ///< number of non-optional arguments (0 means anything goes)
    void (*f)(FILE *out, int nPass, word18 *addr, char *line, char *label, char *op, char *arg0, char *args[32]); // pointer to implementation routine
};
typedef struct pseudoOp pseudoOp;

extern pseudoOp pseudoOps[];

bool doPseudoOp(char *line, char *label, char *op, char *argv0, char *args[32], FILE *out, int nPass, word18 *addr);

/*!
 * a new I/O structure ...
 */
struct IOstack {
    FILE *fp;               ///< a file pointer
    char *fname;            ///< the file name
    int lineno;             ///< linenumber
};
typedef struct IOstack IOstack;

extern char *includePath;

extern int32 lineno;

word36 bitfieldInsert36(word36 dst, word36 patternToInsert, int offset, int length);
word36 bitfieldExtract36(word36 a, int b, int c);
word36 bitMask36(int length);

word36 Eval    (char* expr);
word36 boolEval(char* Expr);    ///< logical expressions w/ octal constants


void  LEXgetPush(char *);
int   LEXgetPop(void);
int   LEXgetc(void);
char *LEXfgets(char *line, int nChars);
void  LEXgetPush(char *filename);
char *LEXCurrentFilename();

char *stripquotes(char *s);

char *strclip(char *s);
char *strpad(char *s, int len);
char *strpreclip(char *s);
char *strchop(char *s);

char *strexp(char *, char *);
char *Strdup(char *s, int size);
char *strrev(char *s);
int strcpyWhile(char *dst, char *src, int (*f)(int));

#define strsep  "Replaced w/ Strsep()"

char *Strsep(char **stringp, const char *delim);

int getPRn(char *s);

void doInterpass(FILE *out);    ///< for any special interpass processing ...
void doPostpass(FILE *out);     ///< for any special post-pass processing ...
void emitSegment(FILE *oct);    ///< emit segment directive
void emitGo(FILE *oct);         ///< emit go directive

#endif
