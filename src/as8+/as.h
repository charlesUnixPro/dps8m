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


#define AS8         ///< we're compiling code for the assebler .....

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

#include <getopt.h>

#include <stdarg.h>
#include <libgen.h>     // for basename

//#include "dps8.h"
//#include "sim_defs.h"                                   /* simulator defns */

#include "uthash.h"
#include "utlist.h"


typedef signed char     int8;
typedef signed short    int16;
typedef signed int      int32;
typedef unsigned char   uint8;
typedef unsigned short  uint16;
typedef unsigned int    uint32;
typedef int             t_stat;                         /* status */
typedef int             t_bool;                         /* boolean */

/* 64b integers */

#if defined (__GNUC__)                                  /* GCC */
typedef signed long long        t_int64;
typedef unsigned long long      t_uint64;
#elif defined (_WIN32)                                  /* Windows */
typedef signed __int64          t_int64;
typedef unsigned __int64        t_uint64;
#elif (defined (__ALPHA) || defined (__ia64)) && defined (VMS) /* 64b VMS */
typedef signed __int64          t_int64;
typedef unsigned __int64        t_uint64;
#elif defined (__ALPHA) && defined (__unix__)           /* Alpha UNIX */
typedef signed long             t_int64;
typedef unsigned long           t_uint64;
#else                                                   /* default */
#define t_int64                 signed long long
#define t_uint64                unsigned long long
#endif                                                  /* end 64b */

#if defined (USE_INT64)                                 /* 64b data */
typedef t_int64         t_svalue;                       /* signed value */
typedef t_uint64        t_value;                        /* value */
#else                                                   /* 32b data */
typedef int32           t_svalue;
typedef uint32          t_value;
#endif                                                  /* end 64b data */

#if defined (USE_INT64) && defined (USE_ADDR64)         /* 64b address */
typedef t_uint64        t_addr;
#define T_ADDR_W        64
#else                                                   /* 32b address */
typedef uint32          t_addr;
#define T_ADDR_W        32
#endif                                                  /* end 64b address */


// patch supplied by Dave Jordan (jordandave@gmail.com) 29 Nov 2012
#ifdef __MINGW32__
#include <stdint.h>
typedef t_uint64    u_int64_t;
#endif

/* Data types */

typedef bool        word1;
typedef uint8       word2;
typedef uint8       word3;
typedef uint8       word4;
typedef uint8       word6;
typedef uint8       word8;
typedef int8        word8s; // signed 8-bit quantity
typedef uint16      word9;
typedef uint16      word12;
typedef uint16      word14;
typedef uint16      word15;
typedef uint32      word18;
typedef int32       word18s;
typedef uint32      word24;
typedef uint32      word27;
typedef t_uint64    word36;
typedef t_int64     word36s;
typedef __uint128_t word72;
typedef __int128_t  word72s;

typedef t_int64     int64;
typedef t_uint64    uint64;
typedef __uint128_t uint128;
typedef __int128_t  int128;

typedef word36      float36;    // single precision float
typedef word72      float72;    // double precision float


#define PRIVATE static

/* Architectural constants */

#define PASIZE          24                              /*!< phys addr width */
#define MAXMEMSIZE      (1 << PASIZE)                   /*!< maximum memory */
#define INIMEMSIZE      001000000                       /*!< 2**18 */
#define PAMASK          ((1 << PASIZE) - 1)
#define MEMSIZE         INIMEMSIZE                      /*!< fixed, KISS */
#define MEM_ADDR_NXM(x) ((x) >= MEMSIZE)
#define VASIZE          18                              /*!< virtual addr width */
#define AMASK           ((1 << VASIZE) - 1)             /*!< virtual addr mask */
#define SEGSIZE         (1 << VASIZE)                   ///< size of segment in words
//#define HIMASK          0777777000000LL                   /*!< left mask */
//#define HISIGN          0400000000000LL                   /*!< left sign */
//#define LOMASK          0000000777777LL                   /*!< right mask */
//#define LOSIGN          0000000400000LL                   /*!< right sign */
#define MAX18POS        0377777                           /*!<  2**17-1 */
#define MAX18NEG        0400000                           /*!< -2**17 */
#define SIGN18          0400000
#define DMASK           0777777777777LL                   /*!< data mask */
#define MASK36 DMASK
#define MASK18          0777777                     ///< 18-bit data mask
#define SIGN36          0400000000000LL                   /*!< sign */
#define SIGN            SIGN36
#define SIGNEX          0100000000000LL                   /*!< extended sign helper for mpf/mpy */
#define SIGN15          040000                            ///< sign mask 15-bit number
#define SIGNMASK15      0xffff0000                        ///< mask to sign exterd a 15-bit number to a 32-bit integer
#define MAGMASK         0377777777777LL                   /*!< magnitude mask */
#define ONES            0777777777777LL
#define NEG136          0777777777777LL                   ///< -1
#define MAXPOS          0377777777777LL                   ///<  2**35-1
#define MAXNEG          0400000000000LL                   ///< -2**35
#define MAX36           0777777777777LL                   ///< 2**36
#define MAX72           (((word72)1 << 72) - 1)           ///< 72 1's

#define CARRY          01000000000000LL                   ///< carry from 2 36-bit additions/subs
#define SIGNEXT    0xfffffff000000000LL        ///< mask to sign extend a 36 => 64-bit negative
#define ZEROEXT         0777777777777LL        ///< mask to zero extend a 36 => 64-bit int
#define SIGNMASK18      037777000000        ///< mask to sign extend a 18 => 32-bit negative
#define ZEROEXT18       0777777                ///< mask to zero extend a 18 => 32-bit int
#define ZEROEXT72       (((word72)1 << 72) - 1)  ///< mask to zero extend a 72 => 128 int
#define SIGN72           ((word72)1 << 71)
#define MASK72          ZEROEXT72

#define SIGN64         ((uint64)1 << 63)

#define MASK8           0377                              ///< 8-bit mask

#define SIGN12          0x800                             ///< sign mask 12-bit number
#define SIGNMASK12      0xfffff800                        ///< mask to sign extend a 12-bit number to a 32-bit integer
#define SIGNEXT12(x)    ((x) & SIGN12) ? ((x) | SIGNMASK12) : (x)  ///< sign extend a 12-bit word to 18/32-bit word

#define SIGNEXT18(x)    (((x) & SIGN18) ? ((x) | SIGNMASK18) : (x)) ///< sign extend a 36-bit word to 64-bit
#define SIGNEXT36(x)    (((x) & SIGN  ) ? ((x) |    SIGNEXT) : (x)) ///< sign extend a 36-bit word to 64-bit
#define SIGNEXT15(x)    (((x) & SIGN15) ? ((x) | SIGNMASK15) : (x)) ///< sign extend a 15-bit word to 18/32-bit word

#define SIGN6           0040     ///< sign bit of 6-bit signed numfer (e.g. Scaling Factor)
#define SIGNMASK6       0340     ///< sign mask for 6-bit number
#define SIGNEXT6(x)     (int8)(((x) & SIGN6) ? ((x) | SIGNMASK6) : (x)) ///< sign extend a 6-bit word to 8-bit char

// the 2 following may need some work........
#define EXTMSK72        (((word72)-1) - ZEROEXT72)
#define SIGNEXT72(x)    ((x) & SIGN72) ? ((x) | EXTMSK72) : (x) ///< sign extend a 72-bit word to 128-bit

#define SETS36(x)         ((x) | SIGN)
#define CLRS36(x)         ((x) & ~SIGN)
#define TSTS36(x)         ((x) & SIGN)

/* Instruction format */

#define INST_V_TAG      0                              ///< Tag
#define INST_M_TAG      077
#define INST_V_A        6                              ///< Indirect via pointer
#define INST_M_A        1
#define INST_V_I        7                              ///< Interrupt Inhibit
#define INST_M_I        1
#define INST_V_OP       9                              /*!< opcode */
#define INST_M_OP       0777
#define INST_V_OPX      8                              /*!< opcode etension */
#define INST_M_OPX      1

#define INST_V_ADDR     18                              ///< Address
#define INST_M_ADDR     0777777
#define INST_V_OFFSET   18                              ///< Offset (Bit29=1)
#define INST_M_OFFSET   077777
#define INST_V_PRN      33                              ///< n of PR[n] (Bit29=1)
#define INST_M_PRN      07


#define GET_TAG(x)      ((int32) ( (x)              & INST_M_TAG))
#define GET_A(x)        ((int32) (((x) >> INST_V_A) & INST_M_A))
#define GET_I(x)        ((int32) (((x) >> INST_V_I) & INST_M_I))
#define GET_OP(x)       ((int32) (((x) >> INST_V_OP) & INST_M_OP))
#define GET_OPX(x)      ((bool) (((x) >> INST_V_OPX) & INST_M_OPX))

#define GET_OFFSET(x)   ((word15) (((x) >> INST_V_OFFSET) & INST_M_OFFSET))
#define GET_PRN(x)      ((word3) (((x) >> INST_V_PRN) & INST_M_PRN))

#define GET_TM(x)       ((int32)(GET_TAG(x) & 060))
#define GET_TD(x)       ((int32)(GET_TAG(x) & 017))

//#define GET_ADDR(x)     ((a8) ((x) & AMASK))
#define GET_ADDR(x)     ((int32) (((x) >> INST_V_ADDR) & INST_M_ADDR))

//// tag defines ...
//#define TAG_R       0     ///< The contents of the register specified in C(Td) are added to the current computed address,
//#define TAG_RI      1     /*!< The contents of the register specified in C(Td) are added to the current computed address, C(TPR.CA), to form the modified computed address as for
//register modification. The modified C(TPR.CA) is then used to fetch an indirect word. The TAG field of the indirect word specifies the next step in computed address formation. The use of du or dl as the designator in this modification type will cause an illegal procedure, illegal modifier, fault. */
//#define TAG_IT      2 /*!< The indirect word at C(TPR.CA) is fetched and the modification performed according to the variation specified in C(Td) of the instruction word and the contents of the indirect word. This modification type allows automatic incrementing and decrementing of addresses and tally counting. */
//#define TAG_IR      3

// ITP stuff ...
#define ISITP(x)                (((x) & 077) == 041)
#define GET_ITP_PRNUM(Ypair)    ((word3)((Ypair[0] >> 33) & 3))
#define GET_ITP_WORDNO(Ypair)   ((word18)((Ypair[1] >> 18) & 0777777))
#define GET_ITP_BITNO(Ypair)    ((word3)((Ypair[1] >> 9) & 077))
#define GET_ITP_MOD(Ypair)      (GET_TAG(Ypair[1]))

// ITS Stuff ...
#define ISITS(x)                (((x) & 077) == 043)
#define GET_ITS_SEGNO(Ypair)    ((word15)((Ypair[0] >> 18) & 077777))
#define GET_ITS_RN(Ypair)       ((word3)((Ypair[0] >> 15) & 07))
#define GET_ITS_WORDNO(Ypair)   ((word18)((Ypair[1] >> 18) & 0777777))
#define GET_ITS_BITNO(Ypair)    ((word3)((Ypair[1] >> 9) & 077))
#define GET_ITS_MOD(Ypair)      (GET_TAG(Ypair[1]))

//#define GETHI(a)	((word18) ((a & 0777777000000LL) >> 18))
#define GETHI36(a)      ((word18) ((a >> 18) & 0777777))
#define GETLO36(a)      ((word18) (a & 0777777))
#define SETHI36(a,b)	(((a) &= 0777777LL), ((a) |= ((((word36)(b) & 0777777) << 18))))
#define SETLO36(a,b)    (((a) &= 0777777000000LL), ((a) |= ((word36)(b) & 0777777LL)))
#define GETHI(a)        GETHI36(a)
#define GETLO(a)        GETLO36(a)
#define SETHI(a,b)      SETHI36((a),(b))
#define SETLO(a,b)      SETLO36((a),(b))

#define GETHI72(a)      ((word36) ((a >> 36) & 0777777777777LL))
#define GETLO72(a)      ((word36) (a & 0777777777777LL))
#define SETHI72(a,b)	(a &= 0777777777777LL, a |= ((((word72)(b) & 0777777777777LL)) << 36))
#define SETLO72(a,b)	(a &= 0777777777777LL << 36, a |= ((word72)(b) & 0777777777777LL))

#define BIT(n)          (1LL << (35-n))


//! Basic + EIS opcodes .....
struct opCode {
    char *mne;    ///< mnemonic
    int32 syntax;       ///< yylex()/yyparse() terminal symbol
    int32 mods;         ///< disallowed addr mods
    int32 ndes;         ///< number of operand descriptor words for instruction (mw EIS)
    
    bool  eis;          ///< true if EIS
    int32 opcode;       ///< opcode # (if needed)
    word18 encoding;    ///< encoding for lower 18-bits of instruction
    
    UT_hash_handle hh;
};

typedef struct opCode opCode;

extern opCode *InstructionTable;

void initInstructionTable();
opCode *findOpcode(char *op);

word18 getOpcode(const char *op);   ///< , const char *arg1, const char *arg2);
opCode *getOpcodeEntry(const char *op);
word18 getEncoding(const char *op);    /// , const char *arg1, const char *arg2) {

struct adrMods {
    const char *mod;    ///< mnemonic
    int   Td;           ///< Td value
    int   Flags;
};
typedef struct adrMods adrMods;

#define EMULATOR_ONLY  1    ///< for emcall instruction, etc ...

extern int callingConvention;   // 1 = Honeywell, 2 = Multics

int asMain(int argc, char **argv);

extern int yyErrorCount;

//extern opCode NonEISopcodes[01000], EISopcodes[01000];
extern adrMods extMods[0100]; ///< extended address modifiers
extern char GEBcdToASCII[64];   ///< GEBCD => ASCII map
extern char ASCIIToGEBcd[128];  ///< ASCII => GEBCD map

// AL39 Table 4-3. Alphanumeric Data Type (TA) Codes
#define CTA9   0
#define CTA6   (1 << 13)
#define CTA4   (2 << 13)

// TN - Type Numeric AL39 Table 4-3. Alphanumeric Data Type (TA) Codes
#define CTN9           0   ///< 9-bit
#define CTN4    (1 << 14)  ///< 4-bit

// S - Sign and Decimal Type (AL39 Table 4-4. Sign and Decimal Type (S) Codes)

#define CSFL            0   ///< Floating-point, leading sign
#define CSLS    (1 << 12)   ///< Scaled fixed-point, leading sign
#define CSTS    (2 << 12)   ///< Scaled fixed-point, trailing sign
#define CSNS    (3 << 12)   ///< Scaled fixed-pointm unsigned

#define MFkAR   0x40 ///< Address register flag. This flag controls interpretation of the ADDRESS field of the operand descriptor just as the "A" flag controls interpretation of the ADDRESS field of the basic and EIS single-word instructions.
#define MFkPR   0x40 ///< ""
#define MFkRL   0x20 ///< Register length control. If RL = 0, then the length (N) field of the operand descriptor contains the length of the operand. If RL = 1, then the length (N) field of the operand descriptor contains a selector value specifying a register holding the operand length. Operand length is interpreted as units of the data size (1-, 4-, 6-, or 9-bit) given in the associated operand descriptor.
#define MFkID   0x10 ///< Indirect descriptor control. If ID = 1 for Mfk, then the kth word following the instruction word is an indirect pointer to the operand descriptor for the kth operand; otherwise, that word is the operand descriptor.

#define MFkREGMASK  0xf

//#define 0    (1 << 0)   ///< fetches/reads operand (CA) from memory
//#define 0   (1 << 1)   ///< stores/writes operand to memory (its a STR-OP)
//#define 0             (0) ///< a Read-Modify-Write instruction
//#define 0      (1 << 2)   ///< fetches/reads Y-pair operand (CA) from memory
//#define 0     (1 << 3)   ///< stores/writes Y-pair operand to memory
//#define 0    (1 << 4)   ///< fetches/reads Y-block8 operand (CA) from memory
//#define STORE_YBLOCK8   (1 << 5)   ///< stores/writes Y-block8 operand to memory
//#define 0   (1 << 6)   ///< fetches/reads Y-block16 operands from memory
//#define STORE_YBLOCK16  (1 << 7)   ///< fetches/reads Y-block16 operands from memory
//#define TRANSFER_INS    (1 << 8)   ///< a transfer instruction
//#define TSPN_INS        (1 << 9)   ///< a TSPn instruction
//#define CALL6_INS       (1 << 10)  ///< a call6 instruction
//#define 0      (1 << 11)  ///< just prepare TPR.CA for instruction
//#define NO_RPT          (1 << 12)  ///< Repeat instructions not allowed
//#define 0         (1 << 13)  ///< Bit-29 has an instruction specific meaning. Ignore.
//#define NO_TAG          (1 << 14)  ///< tag is interpreted differently and for addressing purposes is effectively 0

#define max(a,b)    max2((a),(b))
#define max2(a,b)   ((a) > (b) ? (a) : (b))
#define max3(a,b,c) max((a), max((b),(c)))

#define min(a,b)    min2((a),(b))
#define min2(a,b)   ((a) < (b) ? (a) : (b))
#define min3(a,b,c) min((a), min((b),(c)))


extern bool debug;
extern bool verbose;
extern char *inFile, *includePath, *outFile;

extern int yylineno;
extern char* yytext;

extern int nPass;   // # of pass
extern word18 addr; // current address of source line

void  LEXgetPush(char *);
int   LEXgetPop(void);
int   LEXgetc(void);
int   LEXgetcs(void);
char *LEXfgets(char *line, int nChars);
void  LEXgetPush(char *filename);
char *LEXCurrentFilename();
void  LEXfseekEOF();
extern char *LEXp;  // current source char
extern char LEXline[256];   // current source line
extern int FILEsp;          /*!< (FILE*) stack pointer		*/

int yyparse(), yylex();
int yyerror(const char* msg);
void yyprintf(const char *fmt, ...);

//typedef enum eNumericMode
//{
//    eNumDefault = 0,    // default octal, decimal, hex
//    eNumDecimal,        // decimal mode
//    eNumOctal,          // octal mode
//} eNumericMode;

/*
 * stuff todo with arithmetic/symbolic expressions
 */
struct literal; // forward reference

enum enumExpr
{
    eExprUnknown     = 0,   // unknown type (assume absolute)
    eExprAbsolute    = 1,   // an absolute expression (default)
    eExprRelocatable = 2,   // relocatable expression
    eExprRelative    = 2,   // relocatable expression
    eExprTemporary   = 4,   // a stack temporary
    eExprSegRef      = 5,   // referenced through a segref
    eExprLink        = 6,   // referenced via a link to an external
};
typedef enum enumExpr enumExpr;


struct expr
{
    word36 value;   // value of expression
    enumExpr type;  // type of expression
    char *lc;       // location counter (.text.,)
    
    bool bit29;     // true if bit29 should be set .... >HACK<
};
typedef struct expr expr;

expr *newExpr();
expr *add(expr *, expr *);
expr *subtract(expr *, expr *);
expr *multiply(expr *, expr *);
expr *divide(expr *, expr *);
expr *modulus(expr *, expr *);
expr *and(expr *, expr *);
expr *or(expr *, expr *);
expr *xor(expr *, expr *);
expr *andnot(expr *op1, expr *op2);
expr *not(expr *);
expr *neg(expr *);
expr *neg8(expr *);

expr *exprSymbolValue(char *s);
expr *exprWord36Value(word36 i36);
expr *exprLiteral(struct literal *l);
expr *exprPtrExpr(int ptr_reg, expr *e);


void dumpextRef();
void writEextRef(FILE *oct);
void fillExtRef();

struct stackTemp;       // forward reference

/* ***** symbol table stuff *****/

typedef struct symref
{
    int line;           // line # where symbol is referenced
    char *file;         // file where esymbol is referenced
    
    struct symrefs *prev;
    struct symrefs *next;
} symref;

struct symtab
{
	char *name;	///< symbol name
	word36 value;
    expr  *Value;    // expression value
    
    char *segname;  ///< is symbol references an external symbol in another segment
    char *extname;  ///< name of symbol in external segment
    
    //linkPair *ext;    ///< if symbol refers to an external reference
    int defnLine;   ///< line which symbol was defined (breaks for include files)
    char *defnFile; ///< file in which symbol is defined
    
    struct stackTemp *temp; ///< filld in if symbol is a stack temporary
    
    symref *xref;  ///< list of references to this symbol in assembly (RFU)
    
    UT_hash_handle hh;    
};  //Symtab  [2048];

typedef struct symtab symtab;

extern symtab *Symtab;  // symbol table

void initSymtab();
symtab* getsym(const char *sym);
struct symtab *addsym(char *sym, word36 value);
struct symtab *addsymx(char *sym, expr *value);

void dumpSymtab(bool bSort);

extern symtab *lastLabel;    // symbol table entry to last label defined

void IEEElongdoubleToYPair(long double f0, word36 *Ypair);

struct tuple;

typedef union _tuple
{
    int     i;
    word18  i18;
    word36  i36;
    word72  i72;
    char    c;
    char    *p;
    opCode  *o;
    struct  tuple *t;
    struct  expr *e;
} _tuple;

typedef struct tuple
{
    _tuple  a;
    _tuple  b;
    _tuple  c;
    
    struct tuple *prev;
    struct tuple *next;
} tuple;

// replace w/ _tuple - maybe
typedef enum listType {
    lstUnknown = 0,
    lstI36,
    lstI72,
    lstI18,
    lstSingle,
    lstDouble,
    lstTuple,
    lstList,
    lstLiteral
} listType;

struct literal;

struct list        // for holding a list of anything
{
    char    *p;
    
    word72  i72;
    word36  i36;
    word18  i18;

    long double r;
    
    tuple   *t;
    
    listType    whatAmI;
    
    struct list    *l;
    
    struct literal  *lit;
    
    struct list    *prev;
    struct list    *next;
    
};
typedef struct list list;


enum eLiteralType {
    litUnknown = 0,
    litGeneric,     ///< just a generic literal representing an address
    litSingle,      ///< single precision fp literal
    litDouble,      ///< double precision fp literal
    litDoubleInt,   ///< double precision, scaled filex-point integer
    litScaledFixed, ///< scaled, fixed-point literal
    //litScaledReal,  ///< scaled, floating-point literal
    litString,      ///< string literal
    litVFD,
    litITSITP
};
typedef enum eLiteralType eLiteralType;

typedef struct
{
    word18 srcAddr;     // source address
    int     seqNo;      // sequence number of literal on source line
} literalKey;

struct literal
{
    char *name;         ///< string of literal    ///< name of literal

    //int seqNo;          ///< sequence # I.E. the linear position of the literal in the source line. Typ 1.
    
    word36 values[256]; // 1024 ascii chars or 1280 GEBCD for string literals or even/odd pair for its/itp literal
    int nWords;         // number of 36-bit words this literal uses
    
    long double r;      ///< floating-point literals
    //word18 srcAddr;     ///< where address literal is defined/referenced in assembly
    word18 addr;        ///< address where literal value exists in memory

    literalKey  key;
    
    eLiteralType litType;   ///< what kind of literal are we???
    char *text;         ///< text of literal
    
    char *signature;    ///< string that will represent the *exact* value of the literal.
    
    UT_hash_handle hh;  // hash handle
} literalPool;

typedef struct literal literal;

extern literal *litPool;
extern int seqNo;

void initliteralPool();
//literal *getliteral(word18 srcAddr);
literal *getliteralByName(char *name);
//literal *getliteralByAddr(word18 addr);
literal *getliteralByAddrAndPosn(word18 addr, int seq);
literal* getCurrentLiteral(void);
literal *addliteral(word18 srcAddr, word18 dstAddr, eLiteralType litType, char *text);
literal *addliteraln(word18 srcAddr, word18 dstAddr, eLiteralType litType, int nwords, char *text);
char *makeLiteralSignature(literal *l);
void checkLiteralPool();

void dumpliteralPool();
literal *doLiteral(char *arg1, word18 addr);
void fillLiteralPool();
void writeLiteralPoolOLD(FILE *oct, word18 *addr);
//word18 get18(char *arg, int dudl);
word18 get18(literal *l, int dudl);

literal *doStringLiteral(int size, int kind, char *str);
literal *doNumericLiteral(int base, word36 val);
literal *doNumericLiteral72(word72 val);
literal *doFloatingLiteral(int sd, long double val);
literal *doITSITPLiteral(int itsitp, word36, word36, word36);
literal *doVFDLiteral(tuple *);

void makeLiteralSignatures();
void writeLiteralPool();

#define LITERAL_FMT     "Lit%06o/%d"    ///< format used to generate literals
#define FLITERAL_FMT    "FLt%06o/%d"    ///< format used to generate single-precision fp literals
#define DLITERAL_FMT    "DLt%06o/%d"    ///< format used to generate double-precision fp literals
#define SCFIXPNT_FMT    "SFP%06o/%d"    ///< format used to generate scaled, fixed-point literals
#define SCFIXPNT2_FMT   "SF2%06o/%d"    ///< format used to generate scaled, double-[recision fixed-point literals
#define STRLIT_FMT      "STR%06o/%d"    ///< format used to generate string literals
#define ITLIT_FMT       "ILt%06o/%d"    ///< format used to generate its/itp literals
#define VFDLIT_FMT      "VFD%06o/%d"    ///< format used to generate vfd literals


// Multiword EIS stuff ...
word36 parseMwEIS(opCode *op, char *args);

word36 doDescn(char *arg1, char *arg2, int n);
word36 doDescBs(char *arg1, char *arg2); // XXX probably needs work. See comments.

word18 encodePR(char *arg1, bool *bit29);

// for instruction &pseudoop special lexical tie-ins .....
enum pFlags
{
    epUnknown = 0,
    epStringArgs = 1,   // pseudoop has a string as it's argument
    epVFD = 2,          // for VFD
    epOCT = 4,          // for octal/logical expression(s) only
    epRPT = 8,          // for RPT style arguments
    epDEC = 16,         // decimal numbers only
    epDESC= 32,         // a descriptor descN
};

typedef enum pFlags pFlags;

enum yytokentype;

struct pseudoOp;
typedef struct pseudoOp pseudoOp;

struct pseudoOp
{
    char *name;     ///< name of pseudo op
    pFlags flags;   ///< flags
    void (*f)(FILE *out, int nPass, word18 *addr, char *line, char *label, char *op, char *arg0, char *args[32]); // pointer to implementation routine
    int token;
};

extern pseudoOp pseudoOps[];

pseudoOp *findPop(char *name);


// XXX the tuple and list data structires are a mess. They work well, but are not terribly elegant

int getmod(const char *arg_in);

typedef enum outRecType
{
    outRecUnknown = 0,
    outRecDefault,      // output to default segment
    outRecDirective,    // an operation directive
} outRecType;

struct outRec
{
    outRecType recType;
    
    word18  address;
    word36  data;
    int     relData;    // relocation data (RFU)
    
    char    *src;       // source line associated with output record
    
    char    *direct;    // directive
    list    *dlst;      // list directive args
    char    *dirStr;    // directive string as it will appreat in oct output
    
    struct outRec *prev;
    struct outRec *next;
};

typedef struct outRec outRec;

extern outRec *as8output;

outRec *newoutRec(void);    // functiion to create a new outRec
//outRec *outas8(word36 data, word18 address, char *srctext);
outRec *outas8data(word36 data, word18 address, char *srctext);
outRec *outas8ins(word36 data, word18 address, char *srctext);

//outRec *outas8Str(char *str, word18 address, char *srctext);    // data as 12-digit octal string
outRec *outas8Strd(char *str, word18 address, char *srctext);    // data as 12-digit octal string
outRec *outas8Stri(char *str, word18 address, char *srctext);    // data as 12-digit octal string


outRec *outas8Direct(char *dir, ...);

void dumpOutput(FILE *f);

// Operand/opcode processing stuff ...

struct opnd        // for OPCODE operand(s)
{
    opCode *o;     // opcode
    
    word18 hi;     // high order 18-bits
    word18 lo;     // low-order 18-bits
    
    expr *hix;      // expression representing hogh order 18-bits
    
    bool bit29;    // set bit-29
} opnd;


extern bool bInhibit;   // reflects inhibit flag

void doOpcode(struct opnd *o);
void doMWEis(opCode *o, tuple *t);
void doRPT(opCode *o, word36, word36, tuple *);
void doSTC(opCode *o, word36, word36, int);
void doARS(opCode *o, word36, word36, word36);

int findMfReg(char *reg);

void doOptions(tuple *);

bool doPseudoOp(char *line, char *label, char *op, char *argv0, char *args[32], FILE *out, int nPass, word18 *addr);
void doDescriptor(pseudoOp *p, word36 address, word36 offset, word36 length, word36 scale, int ar);
void doDescriptor2(pseudoOp *p, list *);
//void doBoolEqu(char *sym, word36 val);          // process BOOL/Equate pseudoop
void doBoolEqu(char *sym, expr *val);          // process BOOL/Equate pseudoop
void doStrop(pseudoOp *op, char *str, int sz);  // process acc/aci/bci/ac4 pseudoop
void doOct(list*);                              // process oct pseudoop
void doDec(list*);                              // process dec pseudoop
void doBss(char *, word36);                        // process BSS ( and BFS eventually)
void doPop0(pseudoOp *p);                       // for pseudooops with 0 args
void doVfd(tuple *t);
int _doVfd(tuple *list, word36 *data);
void doHCall(char *, word36, list *, list *);   // for call pseudoop
void doSave(list *);                            // for save pseudoop
void doReturn(char *, word36);                  // for return pseudoop
void doTally(pseudoOp *, list *);               // for TALLY pseudo-ops
void doName(char *);                            // NAME pseudoop
void doArg(struct opnd *o);
void doZero(word36, word36);                    // Zero
void doOrg(word36);
void doITSITP(pseudoOp *p, word36, word36, word36);
void doMod(word36);

void doMCall(expr *, word36, expr *);           // multics CSR Call pseudoop
void doEntry(list *);                           // multics CSR Entry pseudo-op
void doPush(word36);                            // multics CSR Push Pseudo-op
void doReturn0();                               // multics CSR Return pseudo-op
void doShortCall(char *);                       // multics CSR Short_Call pseudo-op
void doShortReturn();                           // multics CSR Short_Return pseudo-op

void doTemp(pseudoOp *, tuple *);               // for TEMP pseudo-ops

void doSegdef(list *);
void doSegref(list *);

void doLink(char *, tuple *);
void doInhibit(char *);

void emitSegdefs();
void emitSegrefs();
void writeSegrefs();

int fillinEntrySequences();     ///< fill in info for entry sequences
void writeEntrySequences();     ///< write entry section to output
void emitEntryDirectives();

void fillinTemps();

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

word72 bitfieldInsert72(word72 dst, word72 patternToInsert, int offset, int length);
word72 bitfieldExtract72(word72 a, int b, int c);

word36 Eval    (char* expr);
word36 boolEval(char* Expr);    ///< logical expressions w/ octal constants

char *stripquotes(char *s);

char *strclip(char *s);
char *strpad(char *s, int len);
char *strpreclip(char *s);
char *strchop(char *s);

char *trim(char *s);
char *ltrim(char *s);
char *rtrim(char *s);

char *strexp(char *, char *);
char *Strdup(char *s, int size);
char *strrev(char *s);
int strcpyWhile(char *dst, char *src, int (*f)(int));
char *strlower(char *q);

#define strsep  "Replaced w/ Strsep()"

char *Strsep(char **stringp, const char *delim);

int getPRn(char *s);

void doInterpass(FILE *out);    ///< for any special interpass processing ...
void doPostpass(FILE *out);     ///< for any special post-pass processing ...

void emitSegment(FILE *oct);    ///< emit segment directive
void emitGo(FILE *oct);         ///< emit go directive

expr *getExtRef(tuple *t);

#endif
