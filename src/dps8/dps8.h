/**
 * \file dps8.h
 * \project dps8
 * \author Harry Reed
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#ifndef dps8_h
#define dps8_h


#ifndef VM_DPS8
#define VM_DPS8        1
#define EMULATOR_ONLY  1    /*!< for emcall instruction, etc ... */
#endif

#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include <sys/time.h>

#include <setjmp.h>     // for setjmp/longjmp used by interrupts & faults

#ifndef USE_INT64
#define USE_INT64
#endif

#include "sim_defs.h"                                   /* simulator defns */

// patch supplied by Dave Jordan (jordandave@gmail.com) 29 Nov 2012
#ifdef __MINGW32__
#include <stdint.h>
typedef t_uint64    u_int64_t;
#endif

/* Data types */

typedef uint32          a8;                           ///< DSP8 addr (18/24b)
typedef t_uint64        d8;                           ///< DSP8 data (36b)

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

/* general manipulation */

// Abort codes, used to sort out longjmp's back to the main loop.
// Codes > 0 are simulator stop codes
// Codes < 0 are internal aborts
// Code  = 0 stops execution for an interrupt check
//

//#define STOP_HALT       1                               /*!< halted */
//#define STOP_IBKPT      2                               /*!< breakpoint */
//#define STOP_ILLEG      3                               /*!< illegal instr */
//#define STOP_ILLINT     4                               /*!< illegal intr inst */
//#define STOP_PAGINT     5                               /*!< page fail in intr */
//#define STOP_ZERINT     6                               /*!< zero vec in intr */
//#define STOP_NXMPHY     7                               /*!< nxm on phys ref */
//#define STOP_IND        8                               /*!< indirection loop */
//#define STOP_XCT        9                               /*!< XCT loop */
//#define STOP_ILLIOC     10                              /*!< invalid UBA num */
//#define STOP_ASTOP      11                              /*!< address stop */
//#define STOP_UNKNOWN    12                              /*!< unknown stop  */
//#define PAGE_FAIL       -1                              /*!< page fail */
//#define INTERRUPT       -2                              /*!< interrupt */
//#define ABORT(x)        longjmp (save_env, (x))         /*!< abort */
//#define IORETURN(f,v)   ((f)? (v): SCPE_OK)             /*!< cond error return */


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

// tag defines ...
#define TAG_R       0     ///< The contents of the register specified in C(Td) are added to the current computed address,
#define TAG_RI      1     /*!< The contents of the register specified in C(Td) are added to the current computed address, C(TPR.CA), to form the modified computed address as for
register modification. The modified C(TPR.CA) is then used to fetch an indirect word. The TAG field of the indirect word specifies the next step in computed address formation. The use of du or dl as the designator in this modification type will cause an illegal procedure, illegal modifier, fault. */
#define TAG_IT      2 /*!< The indirect word at C(TPR.CA) is fetched and the modification performed according to the variation specified in C(Td) of the instruction word and the contents of the indirect word. This modification type allows automatic incrementing and decrementing of addresses and tally counting. */
#define TAG_IR      3

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


/* Flags (stored in their own word) */

#define F_V_A           17      ///< Zero
#define F_V_B           16      ///< Negative
#define F_V_C           15      ///< Carry
#define F_V_D           14      ///< Overflow
#define F_V_E           13      ///< Exponent Overflow
#define F_V_F           12      ///< Exponent Underflow
#define F_V_G           11      ///< Overflow Mask
#define F_V_H           10      ///< Tally Runout
#define F_V_I            9      ///< Parity Error
#define F_V_J            8      ///< Parity Mask
#define F_V_K            7      ///< Not BAR mode
#define F_V_L            6      ///< Truncation
#define F_V_M            5      ///< Mid Instruction Interrupt Fault
#define F_V_N            4      ///< Absolute Mode
#define F_V_O            3      ///< Hex Mode

#define F_A             (1 << F_V_A)
#define F_B             (1 << F_V_B)
#define F_C             (1 << F_V_C)
#define F_D             (1 << F_V_D)
#define F_E             (1 << F_V_E)
#define F_F             (1 << F_V_F)
#define F_G             (1 << F_V_G)
#define F_H             (1 << F_V_H)
#define F_I             (1 << F_V_I)
#define F_J             (1 << F_V_J)
#define F_K             (1 << F_V_K)
#define F_L             (1 << F_V_L)
#define F_M             (1 << F_V_M)
#define F_N             (1 << F_V_N)
#define F_O             (1 << F_V_O)

#define SETF(flags, x)         flags = (flags |  (x))
#define CLRF(flags, x)         flags = (flags & ~(x))
#define TSTF(flags, x)         (flags & (x))
#define SCF(cond, flags, x)    { if ((cond)) SETF(flags, x); else CLRF(flags, x); }

#define SETBIT(dst, bitno)      ((dst) | (1 << (bitno)))
//#define SETBIT(dst, bitno)      ((dst) = (1 << (bitno)))
#define CLRBIT(dst, bitno)      ((dst) & ~(1 << (bitno)))
#define TSTBIT(dst, bitno)      ((dst) &  (1 << (bitno)))

/* IR flags */

#define I_HEX   F_O     /*!< base-16 exponent */ ///< 0000010
#define I_ABS	F_N     /*!< absolute mode */ ///< 0000020
#define I_MIIF	F_M     /*!< mid-instruction interrupt fault */ ///< 0000040
#define I_TRUNC F_L     /*!< truncation */ ///< 0000100
#define I_NBAR	F_K     /*!< not BAR mode */ ///< 0000200
#define I_PMASK	F_J     /*!< parity mask */ ///< 0000400
#define I_PAR	F_I     /*!< parity error */ ///< 0001000
#define I_TALLY	F_H     /*!< tally runout */ ///< 0002000
#define I_OMASK F_G     /*!< overflow mask */ ///< 0004000
#define I_EUFL	F_F     /*!< exponent underflow */ ///< 0010000
#define I_EOFL	F_E     /*!< exponent overflow */ ///< 0020000
#define I_OFLOW	F_D     /*!< overflow */ ///< 0040000
#define I_CARRY	F_C     /*!< carry */ ///< 0100000
#define I_NEG	F_B     /*!< negative */ ///< 0200000
#define I_ZERO	F_A     /*!< zero */ ///< 0400000

/* floating-point constants */
#define FLOAT36MASK     01777777777LL               ///< user to extract mantissa from single precision C(CEAQ)
#define FLOAT72MASK     01777777777777777777777LL   ///< use to extract mastissa from double precision C(EAQ)
#define FLOAT72SIGN     (1LL << 63)                 ///< mantissa sign mask for full precision C(EAQ)
// XXX beware the 72's are not what they seem!


/* scp Debug flags */
#define DBG_TRACE       (1 << 0)    ///< instruction trace
#define DBG_MSG         (1 << 1)    ///< misc output

#define DBG_REGDUMPAQI  (1 << 2)    ///< A/Q/IR register dump
#define DBG_REGDUMPIDX  (1 << 3)    ///< index register dump
#define DBG_REGDUMPPR   (1 << 4)    ///< pointer registers dump
#define DBG_REGDUMPADR  (1 << 5)    ///< address registers dump
#define DBG_REGDUMPPPR  (1 << 6)    ///< PPR register dump
#define DBG_REGDUMPDSBR (1 << 7)    ///< descritptor segment base register dump
#define DBG_REGDUMPFLT  (1 << 8)    ///< C(EAQ) floating-point register dump

#define DBG_REGDUMP     (DBG_REGDUMPAQI | DBG_REGDUMPIDX | DBG_REGDUMPPR | DBG_REGDUMPADR | DBG_REGDUMPPPR | DBG_REGDUMPDSBR | DBG_REGDUMPFLT)

#define DBG_ADDRMOD     (1 << 9)    ///< follow address modifications
#define DBG_APPENDING   (1 << 10)   ///< follow appending unit operations
#define DBG_TRACEEXT    (1 << 11)   ///< extended instruction trace

/* Global data */

extern t_bool sim_idle_enab;


extern a8 saved_PC;
extern int32 flags;

// h6180 stuff
/* [map] designates mapping into 36-bit word from DPS-8 proc manual */

/* GE-625/635 */

extern word36	rA;	/*!< accumulator */
extern word36	rQ;	/*!< quotient */
extern word8	rE;	/*!< exponent [map: rE, 28 0's] */

extern word18	rX[8];	/*!< index */

//extern word18	rBAR;	/*!< base address [map: BAR, 18 0's] */
///< format: 9b base, 9b bound
extern struct _bar {
    word9 BASE;     ///< Contains the 9 high-order bits of an 18-bit address relocation constant. The low-order bits are generated as zeros.
    word9 BOUND;    ///< Contains the 9 high-order bits of the unrelocated address limit. The low- order bits are generated as zeros. An attempt to access main memory beyond this limit causes a store fault, out of bounds. A value of 0 is truly 0, indicating a null memory range.
} BAR;

//extern word18	rIC;	/*!< instruction counter */
#define rIC (PPR.IC)
extern int XECD; /*!< for out-of-line XEC,XED,faults, etc w/o rIC fetch */
extern word36 XECD1; /*!< XEC instr / XED instr#1 */
extern word36 XECD2; /*!< XED instr#2 */

extern word18	rIR;	/*!< indicator [15b] [map: 18 x's, rIR w/ 3 0's] */
extern word27	rTR;	/*!< timer [map: TR, 9 0's] */

extern word18	rY;     /*!< address operand */
extern word8	rTAG;	/*!< instruction tag */

///* GE-645 */
//
//extern word36	rDBR;	/*!< descriptor base */
//extern word27	rABR[8];/*!< address base */
//
///* H6180; L68; DPS-8M */
//
extern word8	rRALR;	/*!< ring alarm [3b] [map: 33 0's, RALR] */
//extern word36	rPRE[8];/*!< pointer, even word */
//extern word36	rPRO[8];/*!< pointer, odd word */
//extern word27	rAR[8];	/*!< address [24b] [map: ARn, 12 0's] */
//extern word36	rPPRE;	/*!< procedure pointer, even word */
//extern word36	rPPRO;	/*!< procedure pointer, odd word */
//extern word36	rTPRE;	/*!< temporary pointer, even word */
//extern word36	rTPRO;	/*!< temporary pointer, odd word */
//extern word36	rDSBRE;	/*!< descriptor segment base, even word */
//extern word36	rDSBRO;	/*!< descriptor segment base, odd word */
//extern word36	mSE[16];/*!< word-assoc-mem: seg descrip regs, even word */
//extern word36	mSO[16];/*!< word-assoc-mem: seg descrip regs, odd word */
//extern word36	mSP[16];/*!< word-assoc-mem: seg descrip ptrs */
//extern word36	mPR[16];/*!< word-assoc-mem: page tbl regs */
//extern word36	mPP[16];/*!< word-assoc-mem: page tbl ptrs */
//extern word36	rFRE;	/*!< fault, even word */
//extern word36	rFRO;	/*!< fault, odd word */
//extern word36	rMR;	/*!< mode */
//extern word36	rCMR;	/*!< cache mode */
//extern word36	hCE[16];/*!< history: control unit, even word */
//extern word36	hCO[16];/*!< history: control unit, odd word */
//extern word36	hOE[16];/*!< history: operations unit, even word */
//extern word36	hOO[16];/*!< history: operations unit, odd word */
//extern word36	hDE[16];/*!< history: decimal unit, even word */
//extern word36	hDO[16];/*!< history: decimal unit, odd word */
//extern word36	hAE[16];/*!< history: appending unit, even word */
//extern word36	hAO[16];/*!< history: appending unit, odd word */
//extern word36	rSW[5];	/*!< switches */
// end h6180 stuff


extern struct _ppr {
    word3   PRR; ///< The number of the ring in which the process is executing. It is set to the effective ring number of the procedure segment when control is transferred to the procedure.
    word15  PSR; ///< The segment number of the procedure being executed.
    word1   P;  ///< A flag controlling execution of privileged instructions. Its value is 1 (permitting execution of privileged instructions) if PPR.PRR is 0 and the privileged bit in the segment descriptor word (SDW.P) for the procedure is 1; otherwise, its value is 0.
    word18  IC;  ///< The word offset from the origin of the procedure segment to the current instruction. (same as rIC)
} PPR;

extern struct _tpr {
    word3   TRR; ///< The current effective ring number
    word15  TSR; ///< The current effective segment number
    word6   TBR; ///< The current bit offset as calculated from ITS and ITP pointer pairs.
    word18  CA;  ///< The current computed address relative to the origin of the segment whose segment number is in TPR.TSR
} TPR;

//extern struct _prn {
//    word15  SNR;    ///< The segment number of the segment containing the data item described by the pointer register.
//    word3   RNR;    ///< The final effective ring number value calculated during execution of the instruction that last loaded the PR.
//    word18  WORDNO; ///< The offset in words from the base or origin of the segment to the data item.
//    word6   BITNO;  ///< The number of the bit within PRn.WORDNO that is the first bit of the data item. Data items aligned on word boundaries always have the value 0. Unaligned data items may have any value in the range [1,35].
//} PR[8];
//
//extern struct _arn {
//    word18  WORDNO; ///< The offset in words relative to the current addressing base referent (segment origin, BAR.BASE, or absolute 0 depending on addressing mode) to the word containing the next data item element.
//    word2   CHAR;   ///< The number of the 9-bit byte within ARn.WORDNO containing the first bit of the next data item element.
//    word4   BITNO;  ///< The number of the bit within ARn.CHAR that is the first bit of the next data item element.
//} AR[8];

/////

/*!
 The terms "pointer register" and "address register" both apply to the same physical hardware. The distinction arises from the manner in which the register is used and in the interpretation of the register contents. "Pointer register" refers to the register as used by the appending unit and "address register" refers to the register as used by the decimal unit.
 The three forms are compatible and may be freely intermixed. For example, PRn may be loaded in pointer register form with the Effective Pointer to Pointer Register n (eppn) instruction, then modified in pointer register form with the Effective Address to Word/Bit Number of Pointer Register n (eawpn) instruction, then further modified in address register form (assuming character size k) with the Add k-Bit Displacement to Address Register (akbd) instruction, and finally invoked in operand descriptor form by the use of MF.AR in an EIS multiword instruction .
 The reader's attention is directed to the presence of two bit number registers, PRn.BITNO and ARn.BITNO. Because the Multics processor was implemented as an enhancement to an existing design, certain apparent anomalies appear. One of these is the difference in the handling of unaligned data items by the appending unit and decimal unit. The decimal unit handles all unaligned data items with a 9-bit byte number and bit offset within the byte. Conversion from the description given in the EIS operand descriptor is done automatically by the hardware. The appending unit maintains compatibility with the earlier generation Multics processor by handling all unaligned data items with a bit offset from the prior word boundary; again with any necessary conversion done automatically by the hardware. Thus, a pointer register, PRn, may be loaded from an ITS pointer pair having a pure bit offset and modified by one of the EIS address register instructions (a4bd, s9bd, etc.) using character displacement counts. The automatic conversion performed ensures that the pointer register, PRi, and its matching address register, ARi, both describe the same physical bit in main memory.
 
     XXX Subtle differences between the interpretation of PR/AR. Need to take this into account.
 
     * For Pointer Registers:
       - PRn.WORDNO The offset in words from the base or origin of the segment to the data item.
       - PRn.BITNO The number of the bit within PRn.WORDNO that is the first bit of the data item. Data items aligned on word boundaries always have the value 0. Unaligned data items may have any value in the range [1,35].
 
     * For Address Registers:
       - ARn.WORDNO The offset in words relative to the current addressing base referent (segment origin, BAR.BASE, or absolute 0 depending on addressing mode) to the word containing the next data item element.
       - ARn.CHAR The number of the 9-bit byte within ARn.WORDNO containing the first bit of the next data item element.
       - ARn.BITNO The number of the bit within ARn.CHAR that is the first bit of the next data item element.
 */
extern struct _par {
    word15  SNR;    ///< The segment number of the segment containing the data item described by the pointer register.
    word3   RNR;    ///< The final effective ring number value calculated during execution of the instruction that last loaded the PR.
    word6   BITNO;  ///< The number of the bit within PRn.WORDNO that is the first bit of the data item. Data items aligned on word boundaries always have the value 0. Unaligned data items may have any value in the range [1,35].
    word18  WORDNO; ///< The offset in words from the base or origin of the segment to the data item.
                        // The offset in words relative to the current addressing base referent (segment origin, BAR.BASE, or absolute 0 depending on addressing mode) to the word containing the next data item element.
    word2   CHAR;   ///< The number of the 9-bit byte within ARn.WORDNO containing the first bit of the next data item element.
} PAR[8];

#define AR    PAR   // XXX remember there are subtle differences between AR/PR.BITNO
#define PR    PAR

/////

extern struct _dsbr {
    word24  ADDR;   ///< If DSBR.U = 1, the 24-bit absolute main memory address of the origin of the current descriptor segment; otherwise, the 24-bit absolute main memory address of the page table for the current descriptor segment.
    word14  BND;    ///< The 14 most significant bits of the highest Y-block16 address of the descriptor segment that can be addressed without causing an access violation, out of segment bounds, fault.
    word1   U;      ///< A flag specifying whether the descriptor segment is unpaged (U = 1) or paged (U = 0).
    word12  STACK;  ///< The upper 12 bits of the 15-bit stack base segment number. It is used only during the execution of the call6 instruction. (See Section 8 for a discussion of generation of the stack segment number.)
} DSBR;

//! The segment descriptor word (SDW) pair contains information that controls the access to a segment. The SDW for segment n is located at offset 2n in the descriptor segment whose description is currently loaded into the descriptor segment base register (DSBR).
extern struct _sdw {  ///< as used by APU
    word24  ADDR;   ///< The 24-bit absolute main memory address of the page table for the target segment if SDWAM.U = 0; otherwise, the 24-bit absolute main memory address of the origin of the target segment.
    word3   R1; ///< Upper limit of read/write ring bracket
    word3   R2; ///< Upper limit of read/execute ring bracket
    word3   R3; ///< Upper limit of call ring bracket
    word14  BOUND;  ///< The 14 high-order bits of the last Y-block16 address within the segment that can be referenced without an access violation, out of segment bound, fault.
    word1   R;  ///< Read permission bit. If this bit is set ON, read access requests are allowed.
    word1   E;  ///< Execute permission bit. If this bit is set ON, the SDW may be loaded into the procedure pointer register (PPR) and instructions fetched from the segment for execution.
    word1   W;  ///< Write permission bit. If this bit is set ON, write access requests are allowed.
    word1   P;  ///< Privileged flag bit. If this bit is set ON, privileged instructions from the segment may be executed if PPR.PRR is 0.
    word1   U;  ///< Unpaged flag bit. If this bit is set ON, the segment is unpaged and SDWAM.ADDR is the 24-bit absolute main memory address of the origin of the segment. If this bit is set OFF, the segment is paged and SDWAM.ADDR is the 24-bit absolute main memory address of the page table for the segment.
    word1   G;  ///< Gate control bit. If this bit is set OFF, calls and transfers into the segment must be to an offset no greater than the value of SDWAM.CL as described below.
    word1   C;  ///< Cache control bit. If this bit is set ON, data and/or instructions from the segment may be placed in the cache memory.
    word14  CL; ///< Call limiter (entry bound) value. If SDWAM.G is set OFF, transfers of control into the segment must be to segment addresses no greater than this value.
    word15  POINTER;    ///< The effective segment number used to fetch this SDW from main memory.
    word1   F;          ///< Full/empty bit. If this bit is set ON, the SDW in the register is valid. If this bit is set OFF, a hit is not possible. All SDWAM.F bits are set OFF by the instructions that clear the SDWAM.
    word6   USE;        ///< Usage count for the register. The SDWAM.USE field is used to maintain a strict FIFO queue order among the SDWs. When an SDW is matched, its USE value is set to 15 (newest) on the DPS/L68 and to 63 on the DPS 8M, and the queue is reordered. SDWs newly fetched from main memory replace the SDW with USE value 0 (oldest) and the queue is reordered.
    
    //bool    _initialized; ///< for emulator use. When true SDWAM entry has been initialized/used ...
    
} SDWAM[64], *SDW;
typedef struct _sdw _sdw;

//* in-core SDW (i.e. not cached, or in SDWAM)
struct _sdw0 {
    // even word
    word24  ADDR;   ///< The 24-bit absolute main memory address of the page table for the target segment if SDWAM.U = 0; otherwise, the 24-bit absolute main memory address of the origin of the target segment.
    word3   R1;     ///< Upper limit of read/write ring bracket
    word3   R2;     ///< Upper limit of read/execute ring bracket
    word3   R3;     ///< Upper limit of call ring bracket
    word1   F;      ///< Directed fault flag
                    // * 0 = page not in main memory; execute directed fault FC
                    // * 1 = page is in main memory
    word2   FC;     ///< directed fault number for page fault.
    
    // odd word
    word14  BOUND;  ///< The 14 high-order bits of the last Y-block16 address within the segment that can be referenced without an access violation, out of segment bound, fault.
    word1   R;  ///< Read permission bit. If this bit is set ON, read access requests are allowed.
    word1   E;  ///< Execute permission bit. If this bit is set ON, the SDW may be loaded into the procedure pointer register (PPR) and instructions fetched from the segment for execution.
    word1   W;  ///< Write permission bit. If this bit is set ON, write access requests are allowed.
    word1   P;  ///< Privileged flag bit. If this bit is set ON, privileged instructions from the segment may be executed if PPR.PRR is 0.
    word1   U;  ///< Unpaged flag bit. If this bit is set ON, the segment is unpaged and SDWAM.ADDR is the 24-bit absolute main memory address of the origin of the segment. If this bit is set OFF, the segment is paged and SDWAM.ADDR is the 24-bit absolute main memory address of the page table for the segment.
    word1   G;  ///< Gate control bit. If this bit is set OFF, calls and transfers into the segment must be to an offset no greater than the value of SDWAM.CL as described below.
    word1   C;  ///< Cache control bit. If this bit is set ON, data and/or instructions from the segment may be placed in the cache memory.
    word14  EB; ///< Entry bound. Any call into this segment must be to an offset less than EB if G=0
} SDW0;
typedef struct _sdw0 _sdw0;


//! ptw as used by APU
extern struct _ptw {
    word18  ADDR;   ///< The 18 high-order bits of the 24-bit absolute main memory address of the page.
    word1   M;      ///< Page modified flag bit. This bit is set ON whenever the PTW is used for a store type instruction. When the bit changes value from 0 to 1, a special extra cycle is generated to write it back into the PTW in the page table in main memory.
    word15  POINTER;///< The effective segment number used to fetch this PTW from main memory.
    word12  PAGENO; ///< The 12 high-order bits of the 18-bit computed address (TPR.CA) used to fetch this PTW from main memory.
    word1   F;      ///< Full/empty bit. If this bit is set ON, the PTW in the register is valid. If this bit is set OFF, a hit is not possible. All PTWAM.F bits are set OFF by the instructions that clear the PTWAM.
    word6   USE;    ///< Usage count for the register. The PTWAM.USE field is used to maintain a strict FIFO queue order among the PTWs. When an PTW is matched its USE value is set to 15 (newest) on the DPS/L68 and to 63 on the DPS 8M, and the queue is reordered. PTWs newly fetched from main memory replace the PTW with USE value 0 (oldest) and the queue is reordered.
    
    //bool    _initialized; ///< for emulator use. When true PTWAM entry has been initialized/used ...
    
} PTWAM[64], *PTW;
typedef struct _ptw _ptw;

//! in-core PTW
extern struct _ptw0 {
    word18  ADDR;   ///< The 18 high-order bits of the 24-bit absolute main memory address of the page.
    word1   U;      // * 1 = page has been used (referenced)
    word1   M;      // * 1 = page has been modified
    word1   F;      ///< Directed fault flag
                    // * 0 = page not in main memory; execute directed fault FC
                    // * 1 = page is in main memory
    word2   FC;     ///< directed fault number for page fault.
    
} PTW0;
typedef struct _ptw0 _ptw0;


#define STOP_UNK    0
#define STOP_UNIMP  1
#define STOP_DIS    2
#define STOP_BKPT   3
#define STOP_INVOP  4
#define STOP_5      5

// not really STOP codes, but get returned from instruction loops
#define CONT_TRA    -1  ///< encountered a transfer instruction dont bump rIC

//! some breakpoint stuff ...
enum eMemoryAccessType {
    Unknown          = 0,
    InstructionFetch,
    IndirectRead,
    //IndirectWrite,  // XXX ????
    //IndirectReadIR   = 3,
    //IndirectReadRI   = 4,
    DataRead,
    DataWrite,
    OperandRead,
    OperandWrite,
    Call6Operand,
    RTCDOperand,
    
    
    // for EIS read operations
    viaPR,      // EIS data access vis PR
};

typedef enum eMemoryAccessType MemoryAccessType;

#define MA_IF  0   /* fetch */
#define MA_ID  1   /* indirect */
#define MA_RD  2   /* data read */
#define MA_WR  3   /* data write */

#define GETCHAR(src, pos) (word36)(((word36)src >> (word36)((5 - pos) * 6)) & 077)      ///< get 6-bit char @ pos
#define GETBYTE(src, pos) (word36)(((word36)src >> (word36)((3 - pos) * 9)) & 0777)     ///< get 9-bit byte @ pos

void putByte(word36 *dst, word9 data, int posn);
word9 getByte(int posn, word36 src);

void putChar(word36 *dst, word6 data, int posn);

#define GET_TALLY(src) (((src) >> 6) & 07777)   // 12-bits
#define GET_DELTA(src)  ((src) & 077)           // 6-bits

#define max(a,b)    max2((a),(b))
#define max2(a,b)   ((a) > (b) ? (a) : (b))
#define max3(a,b,c) max((a), max((b),(c)))

#define min(a,b)    min2((a),(b))
#define min2(a,b)   ((a) < (b) ? (a) : (b))
#define min3(a,b,c) min((a), min((b),(c)))

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw);
t_stat cpu_reset (DEVICE *dptr);

extern uint32 sim_brk_summ, sim_brk_types, sim_brk_dflt;

extern word36 *M;

extern DEVICE cpu_dev;
extern FILE *sim_deb;
//extern void sim_debug (uint32 dbits, DEVICE* dptr, const char* fmt, ...);

extern const char *sim_stop_messages[];

// ******* h6180 stuff *******

// address modification stuff

/* what about faults?? */
void addrmod(word36 i);

#define _TD(tag) ((tag) & 017)
#define _TM(tag) ((tag) & 060)

enum {
    TD_N	= 000,
    TD_AU	= 001,
    TD_QU	= 002,
    TD_DU	= 003,
    TD_IC	= 004,
    TD_AL	= 005,
    TD_QL	= 006,
    TD_DL	= 007,
    TD_X0	= 010,
    TD_X1	= 011,
    TD_X2	= 012,
    TD_X3	= 013,
    TD_X4	= 014,
    TD_X5	= 015,
    TD_X6	= 016,
    TD_X7	= 017
};

enum {
    TM_R	= 000,
    TM_RI	= 020,
    TM_IT	= 040,  // HWR - next 2 had values swapped
    TM_IR	= 060
};

/*! see AL39, pp 6-13, tbl 6-3 */
enum {
    IT_F1	= 000,
    IT_SD	= 004,
    IT_SCR	= 005,
    IT_F2	= 006,
    IT_F3	= 007,
    IT_CI	= 010,
    IT_I	= 011,
    IT_SC	= 012,
    IT_AD	= 013,
    IT_DI	= 014,
    IT_DIC	= 015,
    IT_ID	= 016,
    IT_IDC	= 017,
    
    // not really IT, but they're in it's namespace
    SPEC_ITP  = 001,
    SPEC_ITS  = 003
};

#define GET_TB(tag) ((tag) & 040)
#define GET_CF(tag) ((tag) & 007)

#define _TB(tag) GET_TB(tag)
#define _CF(tag) GET_CF(tag)

#define TB6	000 ///< 6-bit characters
#define TB9	040 ///< 9-bit characters


//! Basic + EIS opcodes .....
struct opCode {
    const char *mne;    ///< mnemonic
    int32 flags;        ///< various and sundry flags
    int32 mods;         ///< disallowed addr mods
    int32 ndes;         ///< number of operand descriptor words for instruction (mw EIS)
    int32 opcode;       ///< opcode # (if needed)
};
typedef struct opCode opCode;

struct adrMods {
    const char *mod;    ///< mnemonic
    int   Td;           ///< Td value
    int   Flags;
};
typedef struct adrMods adrMods;

// instruction decode information
struct EISstruct;   // forward reference
typedef struct EISstruct EISstruct;

/// Instruction decode structure. Used to represent instrucion information
struct DCDstruct
{
    word36 IWB;         ///< instruction working buffer
    opCode *iwb;        ///< opCode *
    int32  opcode;      ///< opcode
    bool   opcodeX;     ///< opcode extension
    word18 address;     ///< bits 0-17 of instruction XXX replace with rY
    bool   a;           ///< bit-29 - address via pointer register. Usually.
    bool   i;           ///< interrupt inhinit bit.
    word6  tag;         ///< instruction tag XXX replace with rTAG
    
    bool    stiTally;   ///< for sti instruction
    
    EISstruct *e;       ///< info if instruction is a MW EIS instruction
};
typedef struct DCDstruct DCDstruct; // decoded instruction info ......

extern DCDstruct *currentInstruction;

DCDstruct *newDCDstruct();
void freeDCDstruct(DCDstruct *p);


// opcocde metadata (flag) ...
// XXX change this to an enum
#define READ_OPERAND    (1 << 0)   ///< fetches/reads operand (CA) from memory
#define STORE_OPERAND   (1 << 1)   ///< stores/writes operand to memory (its a STR-OP)
#define RMW             (READ_OPERAND | STORE_OPERAND) ///< a Read-Modify-Write instruction
#define READ_YPAIR      (1 << 2)   ///< fetches/reads Y-pair operand (CA) from memory
#define STORE_YPAIR     (1 << 3)   ///< stores/writes Y-pair operand to memory
#define READ_YBLOCK8    (1 << 4)   ///< fetches/reads Y-block8 operand (CA) from memory
#define STORE_YBLOCK8   (1 << 5)   ///< stores/writes Y-block8 operand to memory
#define READ_YBLOCK16   (1 << 6)   ///< fetches/reads Y-block16 operands from memory
#define STORE_YBLOCK16  (1 << 7)   ///< fetches/reads Y-block16 operands from memory
#define TRANSFER_INS    (1 << 8)   ///< a transfer instruction
#define TSPN_INS        (1 << 9)   ///< a TSPn instruction
#define CALL6_INS       (1 << 10)  ///< a call6 instruction
#define PREPARE_CA      (1 << 11)  ///< just prepare TPR.CA for instruction
#define NO_RPT          (1 << 12)  ///< Repeat instructions not allowed
#define IGN_B29         (1 << 13)  ///< Bit-29 has an instruction specific meaning. Ignore.
#define NO_TAG          (1 << 14)  ///< tag is interpreted differently and for addressing purposes is effectively 0


extern	word8	tTB;	/*!< char size indicator (TB6=6-bit,TB9=9-bit) [3b] */
extern	word8	tCF;	/*!< character position field [3b] */

extern word6 Td, Tm;
extern word4 CT_HOLD;
extern word36 CY;

/* what about faults? */
//void addrmodreg();
//void addrmodregind();
//void addrmodindreg(word8 td);
//void addrmodindtal();

#define IS_NONE(tag) (!(tag))
/*! non-tally: du or dl */
#define IS_DD(tag) ((_TM(tag) != 040) && \
    ((_TD(tag) == 003) || (_TD(tag) == 007)))
/*! tally: ci, sc, or scr */
#define IS_CSS(tag) ((_TM(tag) == 040) && \
    ((_TD(tag) == 050) || (_TD(tag) == 052) || \
    (_TD(tag) == 045)))
#define IS_DDCSS(tag) (IS_DD(tag) || IS_CSS(tag))
/*! just dl or css */
#define IS_DCSS(tag) (((_TM(tag) != 040) && (_TD(tag) == 007)) || IS_CSS(tag))

// operations stuff

/*! Y of instruc word */
#define Y(i) (i & 0777777000000LL)
/*! X from opcodes in instruc word */
#define OPSX(i) ((i & 0007000) >> 9)
/*! X from OP_* enum, and X from  */
#define X(i) (i & 07)

enum { OP_1	= 00001,
    OP_E	= 00002,
    OP_BAR	= 00003,
    OP_IC	= 00004,
    OP_A	= 00005,
    OP_Q	= 00006,
    OP_AQ	= 00007,
    OP_IR	= 00010,
    OP_TR	= 00011,
    OP_REGS	= 00012,
    
    /* 645/6180 */
    OP_CPR	= 00021,
    OP_DBR	= 00022,
    OP_PTP	= 00023,
    OP_PTR	= 00024,
    OP_RA	= 00025,
    OP_SDP	= 00026,
    OP_SDR	= 00027,
    
    OP_X	= 01000
};

/* this is dependent on the previous enum's ordering */
#define OPSAQ(i) (((i & 0003000) >> 9) + 4)

#define F_COMP	0000001
#define F_OFLOW	0000002
#define F_STORE	0000004
#define F_CARRY	0000010
#define F_NOT	0000020
#define F_AND	0000040
#define F_EA	0000100
#define F_CLEAR 0000200
#define F_LOWER 0000400
#define F_COND  0001000
#define F_CONDQ 0002000
#define F_CHARS 0004000
#define F_BYTES 0010000
#define F_ONE	0020000
#define F_TWO	0040000
#define F_LOW	0100000

// RAW, core stuff ...
int core_read(word24 addr, word36 *data);
int core_write(word24 addr, word36 data);
int core_read2(word24 addr, word36 *even, d8 *odd);
int core_write2(word24 addr, word36 even, d8 odd);
int core_readN(word24 addr, word36 *data, int n);
int core_writeN(word24 addr, word36 *data, int n);
int core_read72(word24 addr, word72 *dst);

// Memory ops that use the appending unit (as necessary) ...
t_stat Read (DCDstruct *i, word24 addr, word36 *dat, enum eMemoryAccessType acctyp, int32 Tag);
t_stat Write (DCDstruct *i, word24 addr, word36 dat, enum eMemoryAccessType acctyp, int32 Tag);
t_stat Read2 (DCDstruct *i, word24 addr, word36 *datEven, word36 *datOdd, enum eMemoryAccessType acctyp, int32 Tag);
t_stat Write2 (DCDstruct *i, word24 addr, word36 datEven, word36 datOdd, enum eMemoryAccessType acctyp, int32 Tag);
t_stat ReadN (DCDstruct *i, int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag);
t_stat ReadNnoalign (DCDstruct *i, int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag);
t_stat WriteN (DCDstruct *i, int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag);
t_stat Read72(DCDstruct *i, word24 addr, word72 *dst, enum eMemoryAccessType acctyp, int32 Tag); // needs testing
t_stat ReadYPair (DCDstruct *i, word24 addr, word36 *Ypair, enum eMemoryAccessType acctyp, int32 Tag);

word18 getBARaddress(word18 addr);
word36 doAppendCycle(DCDstruct *i, MemoryAccessType accessType, word6 Tdes, word36 writeData, word36 *readData);

enum eCAFoper {
    unknown = 0,
    readCY,
    writeCY,
    readCYpair,
    writeCYpair,
    prepareCA
};
typedef enum eCAFoper eCAFoper;

void doComputedAddressFormation(DCDstruct *, eCAFoper action);

word24 doFinalAddressCalculation(DCDstruct *i, MemoryAccessType accessType, word15 segno, word18 offset, word36 *ACVfaults);
extern bool didITSITP; ///< true after an ITS/ITP processing

//void doAddrModPtrReg(word18 inst);
void doAddrModPtrReg(DCDstruct *);
void doPtrReg();        ///< used by EIS stuff

//word36 getOperand();

//
// EIS stuff ...
//

// Numeric operand descriptors

#ifdef AS8  // !!!!!

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

#else

// Numeric operand descriptors

// AL39 Table 4-3. Alphanumeric Data Type (TA) Codes
typedef enum _CTA
{
    CTA9 = 0,       ///< 9-bit bytes
    CTA6 = 1,       ///< 6-bit characters
    CTA4 = 2        ///< 4-bit decimal
} _CTA;

//#define CTA9   0
//#define CTA6   1
//#define CTA4   2


// TN - Type Numeric AL39 Table 4-3. Alphanumeric Data Type (TN) Codes
typedef enum _CTN
{
    CTN9 = 0,   ///< 9-bit
    CTN4 = 1    ///< 4-bit
} _CTN;
//#define CTN9    0   ///< 9-bit
//#define CTN4    1   ///< 4-bit

// S - Sign and Decimal Type (AL39 Table 4-4. Sign and Decimal Type (S) Codes)

#define CSFL    0   ///< Floating-point, leading sign
#define CSLS    1   ///< Scaled fixed-point, leading sign
#define CSTS    2   ///< Scaled fixed-point, trailing sign
#define CSNS    3   ///< Scaled fixed-point, unsigned

#endif

#define MFkAR   0x40 ///< Address register flag. This flag controls interpretation of the ADDRESS field of the operand descriptor just as the "A" flag controls interpretation of the ADDRESS field of the basic and EIS single-word instructions.
#define MFkPR   0x40 ///< ""
#define MFkRL   0x20 ///< Register length control. If RL = 0, then the length (N) field of the operand descriptor contains the length of the operand. If RL = 1, then the length (N) field of the operand descriptor contains a selector value specifying a register holding the operand length. Operand length is interpreted as units of the data size (1-, 4-, 6-, or 9-bit) given in the associated operand descriptor.
#define MFkID   0x10 ///< Indirect descriptor control. If ID = 1 for Mfk, then the kth word following the instruction word is an indirect pointer to the operand descriptor for the kth operand; otherwise, that word is the operand descriptor.

#define MFkREGMASK  0xf

struct MOPstruct;   ///< forward reference
typedef struct MOPstruct MOPstruct;


// EIS instruction take on a life of their own. Need to take into account RNR/SNR/BAR etc.
typedef enum eisDataType
{
    eisUnknown = 0, ///< uninitialized
    eisTA = 1,      ///< type alphanumeric
    eisTN = 2,      ///< type numeric
    eisBIT = 3      ///< bit string
} eisDataType;

enum eRW
{
    eRWreadBit = 0,
    eRWwriteBit
};

typedef enum eRW eRW;


// address of an EIS operand
typedef struct EISaddr
{
    word18  address;    ///< 18-bit virtual address
    //word18  lastAddress;  // memory acccesses are not expesive these days - >sheesh<
    
    word36  data;
    bool    bit;
    bool    incr;      // when true increment bit address
    bool    decr;      // when true decrement bit address
    eRW     mode;
    
    // for type of data being address by this object
    
    eisDataType _type;   ///< type of data - alphunumeric/numeric
    
    int     TA;   ///< type of Alphanumeric chars in src
    int     TN;   ///< type of Numeric chars in src
    int     SZ;   ///< size of chars in src (4-, 6-, or 9-bits)
    //int     CN;   ///< char pos CN
    //int     BIT;  ///< bitno
    int     cPos;
    int     bPos;
    
    // for when using AR/PR register addressing
    word15  SNR;        ///< The segment number of the segment containing the data item described by the pointer register.
    word3   RNR;        ///< The effective ring number value calculated during execution of the instruction that last loaded
    
    //bool    bUsesAR;    ///< true when indirection via AR/PR is involved (TPR.{TRR,TSR} already set up)
    
    MemoryAccessType    mat;    // memory access type for operation
    
    EISstruct *e;      
} EISaddr;

struct EISstruct
{
    DCDstruct *ins;    ///< instruction ins from whence we came
    
    word36  op0;    ///< 1st instruction word

    word36  op[3];   ///< raw operand descriptors
#define OP1 op[0]   ///< 1st descriptor (2nd ins word)
#define OP2 op[1]   ///< 2nd descriptor (3rd ins word)
#define OP3 op[2]   ///< 3rd descriptor (4th ins word)
    
    bool    P;      ///< 4-bit data sign character control
    bool    T;      ///< Truncation fault enable bit
    bool    F;      ///< for CMPB etc fill bit
    bool    R;      ///> Round enable bit
    
    int     BOLR;   ///< Boolean result control field
    
    int     MF[3];
#define MF1     MF[0]   ///< Modification field for operand descriptor 1
#define MF2     MF[1]   ///< Modification field for operand descriptor 2
#define MF3     MF[2]   ///< Modification field for operand descriptor 3

    word18  YChar9[3];
#define YChar91 YChar9[0]
#define YChar92 YChar9[1]
#define YChar93 YChar9[2]
    word18  YChar6[3];
#define YChar61 YChar6[0]
#define YChar62 YChar6[1]
#define YChar63 YChar6[2]
    word18  YChar4[3];
#define YChar41 YChar4[0]
#define YChar42 YChar4[1]
#define YChar43 YChar4[2]

    word18  YBit[3];
#define YBit1 YBit[0]
#define YBit2 YBit[1]
#define YBit3 YBit[2]

    int    CN[3];
#define CN1 CN[0]
#define CN2 CN[1]
#define CN3 CN[2]

    int    C[3];
#define C1 C[0]
#define C2 C[1]
#define C3 C[2]

    int    B[3];
#define B1 B[0]
#define B2 B[1]
#define B3 B[2]

    int     N[3];
#define N1  N[0]
#define N2  N[1]
#define N3  N[2]
    
//    word18  YCharn[3];
//#define YCharn1 YCharn[0]
//#define YCharn2 YCharn[1]
//#define YCharn3 YCharn[3]

    int    TN[3];     ///< type numeric
#define TN1 TN[0]
#define TN2 TN[1]
#define TN3 TN[2]

    int    TA[3];  ///< type alphanumeric
#define TA1 TA[0]
#define TA2 TA[1]
#define TA3 TA[2]

    int    S[3];   ///< Sign and decimal type of number
#define S1  S[0]
#define S2  S[1]
#define S3  S[2]

    int    SF[3];  ///< scale factor
#define SF1 SF[0]
#define SF2 SF[1]
#define SF3 SF[2]

    //! filled in after MF1/2/3 are parsed and examined.
//    int     L[3];
//#define L1  L[0]    ///< length of MF1 operand
//#define L2  L[1]    ///< length of MF2 operand
//#define L3  L[2]    ///< length of MF3 operand
    
    // XXX not certain what to do with these just yet ...
    int32 effBITNO;
    int32 effCHAR;
    int32 effWORDNO;
    
    word18 _flags;  ///< flags set during operation
    word18 _faults; ///< faults generated by instruction
    
    word72s x, y, z;///< a signed, 128-bit integers for playing with .....
 
    char    buff[2560]; ///< a buffer to play with (should be big enough for most of our needs)
    char    *p, *q;    ///< pointers to use/abuse for EIS
    
    
    // Stuff for Micro-operations and Edit instructions...
    
    word9   editInsertionTable[8];     // 8 9-bit chars
    
    int     mop9;       ///< current micro-operation (entire)
    int     mop;        ///< current micro-operation 5-bit code
    int     mopIF;      ///< current mocri-operation IF field
    MOPstruct *m;   ///< pointer to current MOP struct
    
    word9   inBuffer[64]; ///< decimal unit input buffer
    word9   *in;          ///< pointer to current read position in inBuffer
    word9   outBuffer[64];///< output buffer
    word9   *out;         ///< pointer to current write position in outBuffer;
    int     outPos;       ///< current character posn on output buffer word
    
    int     exponent;   ///< For decimal floating-point (evil)
    int     sign;       ///< For signed decimal (1, -1)
    
    EISaddr *mopAddress;   ///< mopAddress, pointer to addr[0], [1], or [2]
    
    //word18  mopAddr;    ///< address of micro-operations
    int     mopTally;   ///< number of micro-ops
    int     mopCN;      ///< starting at char pos CN
    
    int     mopPos;     ///< current mop char posn
    
    // Edit Flags
    // The processor provides the following four edit flags for use by the micro operations.
    
    bool    mopES; // End Suppression flag; initially OFF, set ON by a micro operation when zero-suppression ends.
    bool    mopSN; // Sign flag; initially set OFF if the sending string has an alphanumeric descriptor or an unsigned numeric descriptor. If the sending string has a signed numeric descriptor, the sign is initially read from the sending string from the digit position defined by the sign and the decimal type field (S); SN is set OFF if positive, ON if negative. If all digits are zero, the data is assumed positive and the SN flag is set OFF, even when the sign is negative.
    bool    mopZ;  // Zero flag; initially set ON and set OFF whenever a sending string character that is not decimal zero is moved into the receiving string.
    bool    mopBZ; // Blank-when-zero flag; initially set OFF and set ON by either the ENF or SES micro operation. If, at the completion of a move (L1 exhausted), both the Z and BZ flags are ON, the receiving string is filled with character 1 of the edit insertion table.

    EISaddr addr[3];
    
#define     ADDR1       addr[0]
    //word18  srcAddr; ///< address of sending string
    int     srcTally;///< number of chars in src (max 63)
    int     srcTA;   ///< type of Alphanumeric chars in src
    int     srcTN;   ///< type of Numeric chars in src
    int     srcSZ;   ///< size of chars in src (4-, 6-, or 9-bits)
    int     srcCN;   ///< starting at char pos CN

#define     ADDR2       addr[1]
    //word18  srcAddr2; ///< address of sending string (2nd)
    int     srcTA2;   ///< type of Alphanumeric chars in src (2nd)
    int     srcTN2;   ///< type of Numeric chars in src (2nd)
    int     srcSZ2;   ///< size of chars in src (4-, 6-, or 9-bits) (2nd)
    int     srcCN2;   ///< starting at char pos CN (2nd)

#define     ADDR3       addr[2]
    //word18  dstAddr; ///< address of receiving string
    int     dstTally;///< number of chars in dst (max 63)
    int     dstTA;   ///< type of Alphanumeric chars in dst
    int     dstTN;   ///< type of Numeric chars in dst
    int     dstSZ;   ///< size of chars in dst (4-, 6-, or 9-bits)
    int     dstCN;   ///< starting at char pos CN
    
    bool    mvne;    ///< for MSES micro-op. True when mvne, false when mve
    
    // I suppose we *could* tie function pointers to this, but then it'd be *too* much like cfront, eh?
};

//typedef struct EISstruct EISstruct;

struct MOPstruct
{
    char *mopName;             // name of microoperation
    int (*f)(EISstruct *e);    // pointer to mop() [returns character to be stored]
};


int mopINSM (EISstruct *);
int mopENF  (EISstruct *);
int mopSES  (EISstruct *);
int mopMVZB (EISstruct *);
int mopMVZA (EISstruct *);
int mopMFLS (EISstruct *);
int mopMFLC (EISstruct *);
int mopINSB (EISstruct *);
int mopINSA (EISstruct *);
int mopINSN (EISstruct *);
int mopINSP (EISstruct *);
int mopIGN  (EISstruct *);
int mopMVC  (EISstruct *);
int mopMSES (EISstruct *);
int mopMORS (EISstruct *);
int mopLTE  (EISstruct *);
int mopCHT  (EISstruct *);

void setupOperandDescriptor(int k, EISstruct *e);

void btd(EISstruct *e);
void dtb(EISstruct *e);
void mvne(EISstruct *e);
void mve(EISstruct *e);
void mlr(EISstruct *e);
void mrl(EISstruct *e);
void mvt(EISstruct *e);
void scm(EISstruct *e);
void scmr(EISstruct *e);
void tct(EISstruct *e);
void tctr(EISstruct *e);
void cmpc(EISstruct *e);
void scd(EISstruct *e);
void scdr(EISstruct *e);
void cmpb(EISstruct *e);
void csl(EISstruct *e);
void csr(EISstruct *e);
void sztl(EISstruct *e);
void sztr(EISstruct *e);

void ad2d(EISstruct *e);
void ad3d(EISstruct *e);
void sb2d(EISstruct *e);
void sb3d(EISstruct *e);
void mp2d(EISstruct *e);
void mp3d(EISstruct *e);
void dv2d(EISstruct *e);
void dv3d(EISstruct *e);
void cmpn(EISstruct *e);


// fault stuff ...
#define FAULT_SDF   000 ///< shutdown fault
#define FAULT_STR   002 ///< store fault
#define FAULT_MME   004 ///< master mode entry
#define FAULT_F1    006 ///< fault tag 1
#define FAULT_TRO   010 ///< timer runout fault
#define FAULT_CMD   012 ///< command
#define FAULT_DRL   014 ///< derail
#define FAULT_LUF   016 ///< lockup
#define FAULT_COM   020 ///< connect
#define FAULT_PAR   022 ///< parity
#define FAULT_IPR   024 ///< illegal proceedure
#define FAULT_ONC   026 ///< operation not complete
#define FAULT_SUF   030 ///< startup
#define FAULT_OFL   032 ///< overflow
#define FAULT_DIV   034 ///< divide check
#define FAULT_EXF   036 ///< execute
#define FAULT_DF0   040 ///< directed fault 1
#define FAULT_DF1   042 ///< directed fault 1
#define FAULT_DF2   044 ///< directed fault 1
#define FAULT_DF3   046 ///< directed fault 1
#define FAULT_ACV   050 ///< access violation
#define FAULT_MME2  052 ///< Master mode entry 2
#define FAULT_MME3  054 ///< Master mode entry 3
#define FAULT_MME4  056 ///< Master mode entry 4
#define FAULT_F2    060 ///< fault tag 2
#define FAULT_F3    062 ///< fault tag 3
#define FAULT_UN1   064 ///< unassigned
#define FAULT_UN2   066 ///< unassigned
#define FAULT_UN3   070 ///< unassigned
#define FAULT_UN4   072 ///< unassigned
#define FAULT_UN5   074 ///< unassigned
#define FAULT_TRB   076 ///< Trouble

#define FAULTBASE_MASK  07740       ///< mask off all but top 7 msb

void doFault(int faultNumber, int faultGroup, char *faultMsg); ///< fault handler

// group6 faults generated by APU

#define CUACV0    35   ///< 15.Illegal ring order (ACV0=IRO)
#define CUACV1    34   ///< 3. Not in execute bracket (ACV1=OEB)
#define CUACV2    33   ///< 6. No execute permission (ACV2=E-OFF)
#define CUACV3    32   ///< 1. Not in read bracket (ACV3=ORB)
#define CUACV4    31   ///< 4. No read permission (ACV4=R-OFF)
#define CUACV5    30   ///< 2. Not in write bracket (ACV5=OWB)
#define CUACV6    29   ///< 5. No write permission (ACV6=W-OFF)
#define CUACV7    28   ///< 8. Call limiter fault (ACV7=NO GA)
#define CUACV8    27   ///< 16.Out of call brackets (ACV8=OCB)
#define CUACV9    26   ///< 9. Outward call (ACV9=OCALL)
#define CUACV10   25   ///< 10.Bad outward call (ACV10=BOC)
#define CUACV11   24   ///< 11.Inward return (ACV11=INRET) XXX ??
#define CUACV12   23   ///< 7. Invalid ring crossing (ACV12=CRT)
#define CUACV13   22   ///< 12.Ring alarm (ACV13=RALR)
#define CUAME     21   ///< 13.Associative memory error XXX ??
#define CUACV15   20   ///< 14.Out of segment bounds (ACV15=OOSB)

/*!
 * Access violation faults ...
 */
enum enumACV {
    ACV0 = (1 << 0),   ///< 15.Illegal ring order (ACV0=IRO)
    ACV1 = (1 << 1),   ///< 3. Not in execute bracket (ACV1=OEB)
    ACV2 = (1 << 2),   ///< 6. No execute permission (ACV2=E-OFF)
    ACV3 = (1 << 3),   ///< 1. Not in read bracket (ACV3=ORB)
    ACV4 = (1 << 4),   ///< 4. No read permission (ACV4=R-OFF)
    ACV5 = (1 << 5),   ///< 2. Not in write bracket (ACV5=OWB)
    ACV6 = (1 << 6),   ///< 5. No write permission (ACV6=W-OFF)
    ACV7 = (1 << 7),   ///< 8. Call limiter fault (ACV7=NO GA)
    ACV8 = (1 << 8),   ///< 16.Out of call brackets (ACV8=OCB)
    ACV9 = (1 << 9),   ///< 9. Outward call (ACV9=OCALL)
    ACV10= (1 << 10),   ///< 10.Bad outward call (ACV10=BOC)
    ACV11= (1 << 11),   ///< 11.Inward return (ACV11=INRET) XXX ??
    ACV12= (1 << 12),   ///< 7. Invalid ring crossing (ACV12=CRT)
    ACV13= (1 << 13),   ///< 12.Ring alarm (ACV13=RALR)
    AME  = (1 << 14), ///< 13.Associative memory error XXX ??
    ACV15 =(1 << 15), ///< 14.Out of segment bounds (ACV15=OOSB)
    
    ACDF0  = ( 1 << 16), ///< directed fault 0
    ACDF1  = ( 1 << 17), ///< directed fault 1
    ACDF2  = ( 1 << 18), ///< directed fault 2
    ACDF3  = ( 1 << 19), ///< directed fault 3
    
    IRO = ACV0,
    OEB = ACV1,
    E_OFF = ACV2,
    ORB = ACV3,
    R_OFF = ACV4,
    OWB = ACV5,
    W_OFF = ACV6,
    NO_GA = ACV7,
    OCB = ACV8,
    OCALL = ACV9,
    BOC = ACV10,
    INRET = ACV11,
    CRT = ACV12,
    RALR = ACV13,
    OOSB = ACV15
};



extern enum _processor_cycle_type {
    UNKNOWN_CYCLE = 0,
    APPEND_CYCLE,
    CA_CYCLE,
    OPERAND_STORE,
    DIVIDE_EXECUTION,
    FAULT,
    INDIRECT_WORD_FETCH,
    RTCD_OPERAND_FETCH,
    SEQUENTIAL_INSTRUCTION_FETCH,
    INSTRUCTION_FETCH,
    APU_DATA_MOVEMENT,
    ABORT_CYCLE,
    FAULT_CYCLE
} processorCycle;

extern enum _processor_addressing_mode {
    UNKNOWN_MODE = 0,
    ABSOLUTE_MODE,
    APPEND_MODE,
    BAR_MODE
} processorAddressingMode;


//! Appending unit stuff .......

//Once segno and offset are formed in TPR.SNR and TPR.CA, respectively, the process of generating the 24-bit absolute main memory address can involve a number of different and distinct appending unit cycles.
//The operation of the appending unit is shown in the flowchart in Figure 5-4. This flowchart assumes that directed faults, store faults, and parity faults do not occur.
//A segment boundary check is made in every cycle except PSDW. If a boundary violation is detected, an access violation, out of segment bounds, fault is generated and the execution of the instruction interrupted. The occurrence of any fault interrupts the sequence at the point of occurrence. The operating system software should store the control unit data for possible later continuation and attempt to resolve the fault condition.
//The value of the associative memories may be seen in the flowchart by observing the number of appending unit cycles bypassed if an SDW or PTW is found in the associative memories.
//There are nine different appending unit cycles that involve accesses to main memory. Two of these (FANP, FAP) generate the 24-bit absolute main memory address and initiate a main memory access for the operand, indirect word, or instruction pair; five (NSDW, PSDW, PTW, PTW2, and DSPTW) generate a main memory access to fetch an SDW or PTW; and two (MDSPTW and MPTW) generate a main memory access to update page status bits (PTW.U and PTW.M) in a PTW. The cycles are defined in Table 5-1.

enum _appendingUnit_cycle_type {
    APPUNKNOWN = 0,    ///< unknown
    
    FANP,       ///< Final address nonpaged.
                // Generates the 24-bit absolute main memory address and initiates a main memory access to an unpaged segment for operands, indirect words, or instructions.
    FAP,        ///< Final address paged
                // Generates the 24-bit absolute main memory address and initiates a main memory access to a paged segment for operands, indirect words, or instructions.
    NSDW,       ///< Nonpaged SDW Fetch
                // Fetches an SDW from an unpaged descriptor segment.
    PSDW,       ///< Paged SDW Fetch
                // Fetches an SDW from a paged descriptor segment.
    PTWfetch,   ///< PTW fetch
                // Fetches a PTW from a page table other than a descriptor segment page table and sets the page accessed bit (PTW.U).
    PTW2,       ///< Prepage PTW fetch
                // Fetches the next PTW from a page table other than a descriptor segment page table during hardware prepaging for certain uninterruptible EIS instructions. This cycle does not load the next PTW into the appending unit. It merely assures that the PTW is not faulted (PTW.F = 1) and that the target page will be in main memory when and if needed by the instruction.
    DSPTW,      ///< Descriptor segment PTW fetch
                // Fetches a PTW from a descriptor segment page table.
    MDSPTW,     ///< Modify DSPTW
                // Sets the page accessed bit (PTW.U) in the PTW for a page in a descriptor segment page table. This cycle always immediately follows a DSPTW cycle.
    MPTW        ///< Modify PTW
                //Sets the page modified bit (PTW.M) in the PTW for a page in other than a descriptor segment page table.
};

extern struct opCode NonEISopcodes[01000], EISopcodes[01000];
extern struct adrMods extMods[0100]; ///< extended address modifiers
extern char *moc[040];               ///< micro operation codes

extern char GEBcdToASCII[64];   ///< GEBCD => ASCII map
extern char ASCIIToGEBcd[128];  ///< ASCII => GEBCD map

void doAddrMods(d8);
t_stat spec_disp (FILE *st, UNIT *uptr, int value, void *desc);
char * dumpFlags(word18 flags);
char *disAssemble(word36 instruction);
//struct opCode *getIWBInfo();
struct opCode *getIWBInfo(DCDstruct *i);

word36 compl36(word36 op1, word18 *flags);
word18 compl18(word18 op1, word18 *flags);

void copyBytes(int posn, word36 src, word36 *dst);
void copyChars(int posn, word36 src, word36 *dst);

word36 bitfieldInsert36(word36 a, word36 b, int c, int d);
word72 bitfieldInsert72(word72 a, word72 b, int c, int d);
word36 bitfieldExtract36(word36 a, int b, int c);
word72 bitfieldExtract72(word72 a, int b, int c);

// single precision fp stuff...
double float36ToIEEEdouble(float36 f36);
float36 IEEEdoubleTofloat36(double f);
// double precision stuff ...
long double EAQToIEEElongdouble();
float72 IEEElongdoubleToFloat72(long double f);
void IEEElongdoubleToEAQ(long double f0);

void ufa();
void ufs();
void fno();
void fnoEAQ(word8 *E, word36 *A, word36 *Q);

void fneg();
void ufm();
void fdv();
void fdi();
void frd();
void fcmp();
void fcmg();

void dufa();
void dufs();
void dufm();
void dfdv();
void dfdi();
void dfrd();
void dfcmp();
void dfcmg();

void dvf();

void dfstr(word36 *Ypair);
void fstr(word36 *CY);


word36 AddSub36 (char op, bool isSigned, word36 op1, word36 op2, word18 flagsToSet, word18 *flags);
word36 AddSub36b(char op, bool isSigned, word36 op1, word36 op2, word18 flagsToSet, word18 *flags);
word18 AddSub18b(char op, bool isSigned, word18 op1, word18 op2, word18 flagsToSet, word18 *flags);
word72 AddSub72b(char op, bool isSigned, word72 op1, word72 op2, word18 flagsToSet, word18 *flags);
word72 convertToWord72(word36 even, word36 odd);
void convertToWord36(word72 src, word36 *even, word36 *odd);

void cmp36(word36 op1, word36 op2, word18 *flags);
void cmp36wl(word36 A, word36 Y, word36 Q, word18 *flags);
void cmp18(word18 op1, word18 op2, word18 *flags);
void cmp72(word72 op1, word72 op2, word18 *flags);

void emCall(DCDstruct *);  ///< execute locally defined "emulator call" instruction (emcall)
char *getModString(int32 tag);

int strmask(char *str, char *mask);
char *strlower(char *q);

t_stat loadUnpagedSegment(int segno, word24 addr, word18 count);

extern word24 finalAddress; ///< final 24-bit address for appending unit

extern t_stat loadSpecial(char *buff);

extern bool adrTrace;   ///< when true perform address modification tracing
extern bool apndTrace;  ///< when true do appending unit tracing

extern t_uint64 cpuCycles; ///< # of instructions executed in this run...

extern char *strSDW0(_sdw0 *SDW);
extern char *strSDW(_sdw *SDW);
extern char *strDSBR();

int removeSegment(char *seg);
int removeSegdef(char *seg, char *sym);
int removeSegref(char *seg, char *sym);
int resolveLinks(void);
int loadDeferredSegments(void);
int getAddress(int, int);  // return the 24-bit absolute address of segment + offset
bool getSegmentAddressString(int addr, char *msg);
t_stat createLOT();    // create link offset table segment
_sdw0 *fetchSDW(word15 segno);

// loader stuff ...
struct segdef          // definitions for externally available symbols
{
    char    *symbol;    ///< name of externallay available symbol
    int     value;      ///< address of value in segment
    int     relType;    ///< relocation type (RFU)
    
    int     segno;      ///< when filled-in is the segment # where the segdef is found (default=-1)
    
    struct segdef  *next;
    struct segdef  *prev;
};
typedef struct segdef segdef;

struct segref      // references to external symbols in this segment
{
    char    *segname;   ///< name of segment external symbol resides
    char    *symbol;    ///< name of extern symbol
    int     value;      ///< address of ITS pair in segment
    int     relType;    ///< relocation type (RFU)
    
    int     segno;      ///< when filled-in is the segment # where the segref is to be found (default=-1)
    
    bool    snapped;    ///< true when link has been filled in with a correct ITS pointer
    
    struct segref  *next;
    struct segref  *prev;
};
typedef struct segref segref;

struct segment
{
    char    *name;  ///< name of this segment
    word36  *M;     ///< contents of this segment
    int     size;   ///< size of this segment in 36-bit words
    
    segdef *defs;   ///< symbols available to other segments
    segref *refs;   ///< external symbols needed by this segment
    
    int     segno;  ///< segment# segment is assigned
    
    int     linkOffset; ///< link offset in segment
    int     linkSize;   ///< size of segments linkage section
    
    struct segment *next;
    struct segment *prev;
};
typedef struct segment segment;

segment *findSegment(char *segname);
segment *findSegmentNoCase(char *segname);  // same as above, but case insensitive

segdef *findSegdef(char *seg, char *sgdef);
segdef *findSegdefNoCase(char *seg, char *sgdef);




extern t_stat dumpSDWAM();

/* XXX these OUGHT to return fault codes! (and interrupt numbers???) */
void iom_cioc(word8 conchan);
void iom_cioc_cow(word18 cowaddr);
int iom_setup(const char *confname);

extern int console_chan;

// Assembler/simulator stuff ...

// relocation codes
enum relocationCodes
{
    relAbsolute     = 0,    // Absolute - does not relocate (a)
    relText         = 020,  // Text - uses text section relocation counter (0)
    relTextN        = 021,  // Similar to text (cf. AK92-2 pg. 1-24) (1)
    relLink18       = 022,  // (2)
    relLink18N      = 023,  // (3)
    relLink15       = 024,  // (4)
    relDefinition   = 025,  // (5)
    relSymbol       = 026,  // (6)
    relSymbolN      = 027,  // (7)
    relInt18        = 030,  // (8)
    relInt15        = 031,  // (9)
    relSelf         = 032,  // L
    relExpAbs       = 036,
    relEscape       = 037   // (*)
};


#endif
