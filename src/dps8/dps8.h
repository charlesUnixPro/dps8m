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

#include "sim_tape.h"

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

extern uint64 sim_deb_start;
#undef sim_debug
#define sim_debug(dbits, dptr, ...) if (cpuCycles >= sim_deb_start && sim_deb && ((dptr)->dctrl & dbits)) _sim_debug (dbits, dptr, __VA_ARGS__); else (void)0

#define PRIVATE static

/* Architectural constants */

#define PASIZE          24                              /*!< phys addr width */
#define MAXMEMSIZE      (1 << PASIZE)                   /*!< maximum memory */
#define INIMEMSIZE      MAXMEMSIZE /* 001000000 */                      /*!< 2**18 */
#define PAMASK          ((1 << PASIZE) - 1)
#define MEMSIZE         INIMEMSIZE                      /*!< fixed, KISS */
#define MEM_ADDR_NXM(x) ((x) >= MEMSIZE)
#define VASIZE          18                              /*!< virtual addr width */
#define AMASK           ((1 << VASIZE) - 1)             /*!< virtual addr mask */
#define SEGSIZE         (1 << VASIZE)                   ///< size of segment in words
#define MAX18POS        0377777                           /*!<  2**17-1 */
#define MAX18NEG        0400000                           /*!< -2**17 */  
#define SIGN18          0400000
#define DMASK           0777777777777LL                   /*!< data mask */
#define MASK36 DMASK
#define MASK18          0777777                     ///< 18-bit data mask
#define SIGN36          0400000000000LL                   /*!< sign bit of a 36-bit word */
#define SIGN            SIGN36
#define SIGNEX          0100000000000LL                   /*!< extended sign helper for mpf/mpy */
#define SIGN15          040000                            ///< sign mask 15-bit number
#define SIGNMASK15      0xffff8000                        ///< mask to sign exterd a 15-bit number to a 32-bit integer
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

#define SIGNEXT18(x)    (((x) & SIGN18) ? ((x) | SIGNMASK18) : (x)) ///< sign extend a 18-bit word to 64-bit
#define SIGNEXT36(x)    (((x) & SIGN  ) ? ((x) |    SIGNEXT) : (x)) ///< sign extend a 36-bit word to 64-bit
#define SIGNEXT15(x)    (((x) & SIGN15) ? ((x) | SIGNMASK15) : (x)) ///< sign extend a 15-bit word to 18/32-bit word

#define SIGN6           0040     ///< sign bit of 6-bit signed numfer (e.g. Scaling Factor)
#define SIGNMASK6       0340     ///< sign mask for 6-bit number
#define SIGNEXT6(x)     (int8)(((x) & SIGN6) ? ((x) | SIGNMASK6) : (x)) ///< sign extend a 6-bit word to 8-bit char

#define MASKBITS(x) ( ~(~((t_uint64)0)<<x) ) // lower (x) bits all ones



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


#define GET_TAG(x)      ((int32) ( (x)              & INST_M_TAG ))
#define GET_A(x)        ((int32) (((x) >> INST_V_A) & INST_M_A   ))
#define GET_I(x)        ((int32) (((x) >> INST_V_I) & INST_M_I   ))
#define GET_OP(x)       ((int32) (((x) >> INST_V_OP) & INST_M_OP ))
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

#define SETF(flags, x)         flags = ((flags) |  (x))
#define CLRF(flags, x)         flags = ((flags) & ~(x))
#define TSTF(flags, x)         ((flags) & (x))
#define SCF(cond, flags, x)    { if ((cond)) SETF((flags), x); else CLRF((flags), x); }

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
#define I_PERR	F_I     /*!< parity error */ ///< 0001000
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
#define DBG_WARN        (1 << 12)   
#define DBG_DEBUG       (1 << 13)   
#define DBG_INFO        (1 << 14)   
#define DBG_NOTIFY      (1 << 15)   
#define DBG_ERR         (1 << 16)   
#define DBG_ALL (DBG_NOTIFY | DBG_INFO | DBG_ERR | DBG_DEBUG | DBG_WARN | DBG_ERR )
#define DBG_FAULT       (1 << 17)  ///< follow fault handling

/* Global data */

// h6180 stuff
/* [map] designates mapping into 36-bit word from DPS-8 proc manual */

/* GE-625/635 */

extern word36	rA;	/*!< accumulator */
extern word36	rQ;	/*!< quotient */
extern word8	rE;	/*!< exponent [map: rE, 28 0's] */

extern word18	rX[8];	/*!< index */

#define reg_A   rA
#define reg_Q   rQ
#define reg_X   rX

//extern word18	rBAR;	/*!< base address [map: BAR, 18 0's] */
///< format: 9b base, 9b bound
extern struct _bar {
    word9 BASE;     ///< Contains the 9 high-order bits of an 18-bit address relocation constant. The low-order bits are generated as zeros.
    word9 BOUND;    ///< Contains the 9 high-order bits of the unrelocated address limit. The low- order bits are generated as zeros. An attempt to access main memory beyond this limit causes a store fault, out of bounds. A value of 0 is truly 0, indicating a null memory range.
} BAR;

//extern word12 rFAULTBASE;  ///< fault base (12-bits of which the top-most 7-bits are used)
#define rFAULTBASE switches.FLT_BASE

//extern word18	rIC;	/*!< instruction counter */
#define rIC (PPR.IC)
extern int XECD; /*!< for out-of-line XEC,XED,faults, etc w/o rIC fetch */
extern word36 XECD1; /*!< XEC instr / XED instr#1 */
extern word36 XECD2; /*!< XED instr#2 */

//extern word18	rIR;	/*!< indicator [15b] [map: 18 x's, rIR w/ 3 0's] */
#define rIR (cu.IR)
extern word27	rTR;	/*!< timer [map: TR, 9 0's] */

extern word18	ry;     /*!< address operand */
extern word24	rY;     /*!< address operand */
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
extern struct _sdw0 {
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



// Abort codes, used to sort out longjmp's back to the main loop.
// Codes > 0 are simulator stop codes
// Codes < 0 are internal aborts
// Code  = 0 stops execution for an interrupt check (XXX Don't know if I like this or not)
// XXX above is not entirely correct (anymore).


#define STOP_UNK    0
#define STOP_UNIMP  1
#define STOP_DIS    2
#define STOP_BKPT   3
#define STOP_INVOP  4
#define STOP_5      5
#define STOP_BUG    6
#define STOP_WARN   7
#define STOP_FLT_CASCADE   8
extern const char *sim_stop_messages[];


// not really STOP codes, but get returned from instruction loops
#define CONT_TRA    -1  ///< encountered a transfer instruction dont bump rIC
#define CONT_FAULT  -2  ///< instruction encountered some kind of fault

//! some breakpoint stuff ...
enum eMemoryAccessType {
    Unknown          = 0,
    InstructionFetch,
    IndirectRead,
    IndirectWrite,
    DataRead,
    DataWrite,
    OperandRead,
    OperandWrite,
    
    APUDataRead,        // append operations from absolute mode
    APUDataWrite,
    APUOperandRead,
    APUOperandWrite,

    Call6Operand,
    RTCDOperand,
    
    
    // for EIS read operations
    viaPR,      // EIS data access vis PR
    PrepareCA,
};

typedef enum eMemoryAccessType MemoryAccessType;

#define MA_IF  0   /* fetch */
#define MA_ID  1   /* indirect */
#define MA_RD  2   /* data read */
#define MA_WR  3   /* data write */

extern word36 Ypair[2];        ///< 2-words

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
t_stat cpu_boot (int32 unit_num, DEVICE *dptr);

extern uint32 sim_brk_summ, sim_brk_types, sim_brk_dflt;

extern DEVICE cpu_dev;
extern DEVICE iom_dev;
extern DEVICE tape_dev;
extern DEVICE *sim_devices[];
extern UNIT mt_unit [];
extern UNIT cpu_unit [];
extern FILE *sim_deb;

void _sim_debug (uint32 dbits, DEVICE* dptr, const char* fmt, ...)
#ifdef __GNUC__
  __attribute__ ((format (printf, 3, 4)))
#endif
;


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
    
    word18 stiTally;    ///< for sti instruction
    
    EISstruct *e;       ///< info: if instruction is a MW EIS instruction
};
typedef struct DCDstruct DCDstruct; // decoded instruction info ......

extern DCDstruct *currentInstruction;

//DCDstruct *newDCDstruct(void);
void freeDCDstruct(DCDstruct *p);


// opcode metadata (flag) ...
// XXX change this to an enum?
#define READ_OPERAND    (1 << 0)   ///< fetches/reads operand (CA) from memory
#define STORE_OPERAND   (1 << 1)   ///< stores/writes operand to memory (its a STR-OP)
#define RMW             (READ_OPERAND | STORE_OPERAND) ///< a Read-Modify-Write instruction
#define READ_YPAIR      (1 << 2)   ///< fetches/reads Y-pair operand (CA) from memory
#define STORE_YPAIR     (1 << 3)   ///< stores/writes Y-pair operand to memory
#define READ_YBLOCK8    (1 << 4)   ///< fetches/reads Y-block8 operand (CA) from memory
#define NO_RPT          (1 << 5)   ///< Repeat instructions not allowed
//#define NO_RPD          (1 << 6)
#define NO_RPL          (1 << 7)
//#define NO_RPX          (NO_RPT | NO_RPD | NO_RPL)
#define READ_YBLOCK16   (1 << 8)   ///< fetches/reads Y-block16 operands from memory
#define STORE_YBLOCK16  (1 << 9)   ///< fetches/reads Y-block16 operands from memory
#define TRANSFER_INS    (1 << 10)  ///< a transfer instruction
#define TSPN_INS        (1 << 11)  ///< a TSPn instruction
#define CALL6_INS       (1 << 12)  ///< a call6 instruction
#define PREPARE_CA      (1 << 13)  ///< prepare TPR.CA for instruction
#define STORE_YBLOCK8   (1 << 14)  ///< stores/writes Y-block8 operand to memory
#define IGN_B29         (1 << 15)  ///< Bit-29 has an instruction specific meaning. Ignore.
#define NO_TAG          (1 << 16)  ///< tag is interpreted differently and for addressing purposes is effectively 0
#define PRIV_INS        (1 << 17)  ///< priveleged instruction
#define NO_BAR          (1 << 18)  ///< not allowed in BAR mode
#define NO_XEC          (1 << 19)  ///< can't be executed via xec/xed
#define NO_XED          (1 << 20)  ///< No execution via XED instruction

// There are three modes of main memory addressing (absolute mode, append mode, and BAR mode),
// and two modes of instruction execution (normal mode and privileged mode).
//#define NO_BAR          (1 << 20)    ///< BAR mode not allowed
//#define NO_NORMAL       (1 << 21)    ///< No NORMAL mode
//#define NO_BARNORM      (NO_BAR | NO_NORMAL)



// opcode metadata (disallowed) modifications
// XXX change to an enum as time permits?
#define NO_DU           (1 << 0)    ///< No DU modification allowed (Can these 2 be combined into 1?)
#define NO_DL           (1 << 1)    ///< No DL modification allowed
#define NO_DUDL         (NO_DU | NO_DL)    

#define NO_CI           (1 << 2)    ///< No character indirect modification (can these next 3 be combined?_
#define NO_SC           (1 << 3)    ///< No sequence character modification
#define NO_SCR          (1 << 4)    ///< No sequence character reverse modification
#define NO_CSS          (NO_CI | NO_SC | NO_SCR)

#define NO_DLCSS        (NO_DU   | NO_CSS)
#define NO_DDCSS        (NO_DUDL | NO_CSS)

#define ONLY_AU_QU_AL_QL_XN     (1 << 5)    ///< None except au, qu, al, ql, xn

// None except au, qu, al, ql, xn for MF1 and REG
// None except du, au, qu, al, ql, xn for MF2
// None except au, qu, al, ql, xn for MF1, MF2, and MF3

// XXX add these



extern	word8	tTB;	/*!< char size indicator (TB6=6-bit,TB9=9-bit) [3b] */
extern	word8	tCF;	/*!< character position field [3b] */

extern word6 Td, Tm;
//extern word4 CT_HOLD;

// XXX these ought to moved to DCDstruct 
extern word36 CY;
extern word36 YPair[2];

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


enum eCAFoper {
    unknown = 0,
    readCY,
    writeCY,
    readCYpair,
    writeCYpair,
    prepareCA
};
typedef enum eCAFoper eCAFoper;

word24 doFinalAddressCalculation(DCDstruct *i, MemoryAccessType accessType, word15 segno, word18 offset, word36 *ACVfaults);
extern bool didITSITP; ///< true after an ITS/ITP processing

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

void btd(DCDstruct *i);
void dtb(DCDstruct *i);
void mvne(DCDstruct *i);
void mve(DCDstruct *i);
void mlr(DCDstruct *i);
void mrl(DCDstruct *i);
void mvt(DCDstruct *i);
void scm(DCDstruct *i);
void scmr(DCDstruct *i);
void tct(DCDstruct *i);
void tctr(DCDstruct *i);
void cmpc(DCDstruct *i);
void scd(DCDstruct *i);
void scdr(DCDstruct *i);
void cmpb(DCDstruct *i);
void csl(DCDstruct *i);
void csr(DCDstruct *i);
void sztl(DCDstruct *i);
void sztr(DCDstruct *i);

void ad2d(DCDstruct *i);
void ad3d(DCDstruct *i);
void sb2d(DCDstruct *i);
void sb3d(DCDstruct *i);
void mp2d(DCDstruct *i);
void mp3d(DCDstruct *i);
void dv2d(DCDstruct *i);
void dv3d(DCDstruct *i);
void cmpn(DCDstruct *i);


// fault stuff ...
#define FAULT_SDF   0 ///< shutdown fault
#define FAULT_STR   1 ///< store fault
#define FAULT_MME   2 ///< master mode entry
#define FAULT_F1    3 ///< fault tag 1
#define FAULT_TRO   4 ///< timer runout fault
#define FAULT_CMD   5 ///< command
#define FAULT_DRL   6 ///< derail
#define FAULT_LUF   7 ///< lockup
#define FAULT_CON   8 ///< connect
#define FAULT_PAR   9 ///< parity
#define FAULT_IPR   10 ///< illegal proceedure
#define FAULT_ONC   11 ///< operation not complete
#define FAULT_SUF   12 ///< startup
#define FAULT_OFL   13 ///< overflow
#define FAULT_DIV   14 ///< divide check
#define FAULT_EXF   15 ///< execute
#define FAULT_DF0   16 ///< directed fault 0
#define FAULT_DF1   17 ///< directed fault 1
#define FAULT_DF2   18 ///< directed fault 2
#define FAULT_DF3   19 ///< directed fault 3
#define FAULT_ACV   20 ///< access violation
#define FAULT_MME2  21 ///< Master mode entry 2
#define FAULT_MME3  22 ///< Master mode entry 3
#define FAULT_MME4  23 ///< Master mode entry 4
#define FAULT_F2    24 ///< fault tag 2
#define FAULT_F3    25 ///< fault tag 3
#define FAULT_UN1   26 ///< unassigned
#define FAULT_UN2   27 ///< unassigned
#define FAULT_UN3   28 ///< unassigned
#define FAULT_UN4   29 ///< unassigned
#define FAULT_UN5   30 ///< unassigned
#define FAULT_TRB   31 ///< Trouble

#define FAULTBASE_MASK  07740       ///< mask off all but top 7 msb


// 32 fault codes exist.  Fault handling and interrupt handling are
// similar.
enum _fault {
    // Note that these numbers are decimal, not octal.
    // Faults not listed are not generated by the emulator.
    shutdown_fault = FAULT_SDF,
    store_fault = FAULT_STR,
    mme1_fault = FAULT_MME,
    f1_fault = FAULT_F1,
    timer_fault = FAULT_TRO,
    cmd_fault = FAULT_CMD,
    derail_fault = FAULT_DRL,
    lockup_fault = FAULT_LUF,
    connect_fault = FAULT_CON,
    parity_fault = FAULT_PAR,
    illproc_fault = FAULT_IPR,
    op_not_complete_fault = FAULT_ONC,
    startup_fault = FAULT_SUF,
    overflow_fault = FAULT_OFL,
    div_fault = FAULT_DIV,
    dir_flt0_fault = FAULT_DF0,
    dir_flt1_fault = FAULT_DF1,
    dir_flt2_fault = FAULT_DF2,
    dir_flt3_fault = FAULT_DF3,
    acc_viol_fault = FAULT_ACV,
    mme2_fault = FAULT_MME2,
    mme3_fault = FAULT_MME3,
    mme4_fault = FAULT_MME4,
    f2_fault = FAULT_F2,
    f3_fault = FAULT_F3,
    trouble_fault = FAULT_TRB
    //
    // oob_fault=32 // out-of-band, simulator only
};
typedef enum _fault _fault;

struct _fault_register {
    // even word
    bool    ill_op;     // IPR fault. An illegal operation code has been detected.
    bool    ill_mod;    // IPR fault. An illegal address modifier has been detected.
    bool    ill_slv;    // IPR fault. An illegal BAR mode procedure has been encountered.
    bool    ill_proc;   // IPR fault. An illegal procedure other than the three above has been encountered.
    bool    nem;        // ONC fault. A nonexistent main memory address has been requested.
    bool    oob;        // STR fault. A BAR mode boundary violation has occurred.
    bool    ill_dig;    // IPR fault. An illegal decimal digit or sign has been detected by the decimal unit.
    bool    proc_paru;  // PAR fault. A parity error has been detected in the upper 36 bits of data. (Yeah, right)
    bool    proc_parl;  // PAR fault. A parity error has been detected in the lower 36 bits of data. (Yeah, right)
    bool    con_a;      // CON fault. A $CONNECT signal has been received through port A.
    bool    con_b;      // CON fault. A $CONNECT signal has been received through port B.
    bool    con_c;      // CON fault. A $CONNECT signal has been received through port C.
    bool    con_d;      // CON fault. A $CONNECT signal has been received through port D.
    bool    da_err;     // ONC fault. Operation not complete. Processor/system controller interface sequence error 1 has been detected. (Yeah, right)
    bool    da_err2;    // ONC fault. Operation not completed. Processor/system controller interface sequence error 2 has been detected.
    int     ia_a;       // Coded illegal action, port A. (See Table 3-2)
    int     ia_b;       // Coded illegal action, port B. (See Table 3-2)
    int     ia_c;       // Coded illegal action, port C. (See Table 3-2)
    int     ia_d;       // Coded illegal action, port D. (See Table 3-2)
    bool    cpar_div;   // A parity error has been detected in the cache memory directory. (Not likely)
    bool    cpar_str;   // PAR fault. A data parity error has been detected in the cache memory.
    bool    cpar_ia;    // PAR fault. An illegal action has been received from a system controller during a store operation with cache memory enabled.
    bool    cpar_blk;   // PAR fault. A cache memory parity error has occurred during a cache memory data block load.
    
    // odd word
    //      Cache Duplicate Directory WNO Buffer Overflow
    bool    port_a;
    bool    port_b;
    bool    port_c;
    bool    port_d;
    
    bool    cpd;  // Cache Primary Directory WNO Buffer Overflow
    // Write Notify (WNO) Parity Error on Port A, B, C, or D.
    
    //      Cache Duplicate Directory Parity Error
    bool    level_0;
    bool    level_1;
    bool    level_2;
    bool    level_3;
    
    // Cache Duplicate Directory Multiple Match
    bool    cdd;
    
    bool    par_sdwam;  // A parity error has been detected in the SDWAM.
    bool    par_ptwam;  // A parity error has been detected in the PTWAM.
};

enum _fault_subtype {
    fault_subtype_unknown = 0,
    fault_subtype_not_specified = 0,
    no_fault_subtype = 0,
    ill_op,     // IPR fault. An illegal operation code has been detected.
    ill_mod,    // IPR fault. An illegal address modifier has been detected.
    ill_slv,    // IPR fault. An illegal BAR mode procedure has been encountered.
    ill_proc,   // IPR fault. An illegal procedure other than the three above has been encountered.
    nem,        // ONC fault. A nonexistent main memory address has been requested.
    oob,        // STR fault. A BAR mode boundary violation has occurred.
    ill_dig,    // IPR fault. An illegal decimal digit or sign has been detected by the decimal unit.
    proc_paru,  // PAR fault. A parity error has been detected in the upper 36 bits of data. (Yeah, right)
    proc_parl,  // PAR fault. A parity error has been detected in the lower 36 bits of data. (Yeah, right)
    con_a,      // CON fault. A $CONNECT signal has been received through port A.
    con_b,      // CON fault. A $CONNECT signal has been received through port B.
    con_c,      // CON fault. A $CONNECT signal has been received through port C.
    con_d,      // CON fault. A $CONNECT signal has been received through port D.
    da_err,     // ONC fault. Operation not complete. Processor/system controller interface sequence error 1 has been detected. (Yeah, right)
    da_err2,    // ONC fault. Operation not completed. Processor/system controller interface sequence error 2 has been detected.
    cpar_dir,   // A parity error has been detected in the cache memory directory. (Not likely)
    cpar_str,   // PAR fault. A data parity error has been detected in the cache memory.
    cpar_ia,    // PAR fault. An illegal action has been received from a system controller during a store operation with cache memory enabled.
    cpar_blk,   // PAR fault. A cache memory parity error has occurred during a cache memory data block load.
    
    // odd word
    //      Cache Duplicate Directory WNO Buffer Overflow
    port_a,
    port_b,
    port_c,
    port_d,
    
    cpd,  // Cache Primary Directory WNO Buffer Overflow
    // Write Notify (WNO) Parity Error on Port A, B, C, or D.
    
    //      Cache Duplicate Directory Parity Error
    level_0,
    level_1,
    level_2,
    level_3,
    
    // Cache Duplicate Directory Multiple Match
    cdd,
    
    par_sdwam,  // A parity error has been detected in the SDWAM.
    par_ptwam,  // A parity error has been detected in the PTWAM.
    
    // Access violation fault subtypes
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
typedef enum _fault_subtype _fault_subtype;


void fault_gen(int f);  // depreciate when ready
void doFault(DCDstruct *, _fault faultNumber, _fault_subtype faultSubtype, char *faultMsg); ///< fault handler
void doG7Faults();
bool G7Pending();

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
//enum enumACV {
//    ACV0 = (1 << 0),   ///< 15.Illegal ring order (ACV0=IRO)
//    ACV1 = (1 << 1),   ///< 3. Not in execute bracket (ACV1=OEB)
//    ACV2 = (1 << 2),   ///< 6. No execute permission (ACV2=E-OFF)
//    ACV3 = (1 << 3),   ///< 1. Not in read bracket (ACV3=ORB)
//    ACV4 = (1 << 4),   ///< 4. No read permission (ACV4=R-OFF)
//    ACV5 = (1 << 5),   ///< 2. Not in write bracket (ACV5=OWB)
//    ACV6 = (1 << 6),   ///< 5. No write permission (ACV6=W-OFF)
//    ACV7 = (1 << 7),   ///< 8. Call limiter fault (ACV7=NO GA)
//    ACV8 = (1 << 8),   ///< 16.Out of call brackets (ACV8=OCB)
//    ACV9 = (1 << 9),   ///< 9. Outward call (ACV9=OCALL)
//    ACV10= (1 << 10),   ///< 10.Bad outward call (ACV10=BOC)
//    ACV11= (1 << 11),   ///< 11.Inward return (ACV11=INRET) XXX ??
//    ACV12= (1 << 12),   ///< 7. Invalid ring crossing (ACV12=CRT)
//    ACV13= (1 << 13),   ///< 12.Ring alarm (ACV13=RALR)
//    AME  = (1 << 14), ///< 13.Associative memory error XXX ??
//    ACV15 =(1 << 15), ///< 14.Out of segment bounds (ACV15=OOSB)
//    
//    ACDF0  = ( 1 << 16), ///< directed fault 0
//    ACDF1  = ( 1 << 17), ///< directed fault 1
//    ACDF2  = ( 1 << 18), ///< directed fault 2
//    ACDF3  = ( 1 << 19), ///< directed fault 3
//    
//    IRO = ACV0,
//    OEB = ACV1,
//    E_OFF = ACV2,
//    ORB = ACV3,
//    R_OFF = ACV4,
//    OWB = ACV5,
//    W_OFF = ACV6,
//    NO_GA = ACV7,
//    OCB = ACV8,
//    OCALL = ACV9,
//    BOC = ACV10,
//    INRET = ACV11,
//    CRT = ACV12,
//    RALR = ACV13,
//    OOSB = ACV15
//};



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

enum _processor_addressing_mode {
    UNKNOWN_MODE = 0,
    ABSOLUTE_MODE,
    APPEND_MODE,
    BAR_MODE,
    TEMPORARY_ABSOLUTE_MODE
};// processorAddressingMode;

enum _processor_operating_mode {
    UNKNOWN_OPERATING_MODE = 0,
    NORMAL_MODE,
    PRIVILEGED_MODE,
} processorOperatingMode;
typedef enum _processor_addressing_mode _processor_addressing_mode;

extern bool bPuls2;

/* 
 * Cache Mode Regsiter
 *
 * (dont know where else to put this)
 */

struct _cache_mode_register
{
    word15     cache_dir_address;  // Probably not used by simulator
    word1    par_bit;            // "
    word1    lev_ful;
    word1    csh1_on;
    word1    csh2_on;
    word1    opnd_on;
    word1    inst_on; // DPS8, but not DPS8M
    word1    csh_reg;
    word1    str_asd;
    word1    col_ful;
    word2     rro_AB;
    word2     luf;        // LUF value
                        // 0   1   2   3
                        // Lockup time
                        // 2ms 4ms 8ms 16ms
                        // The lockup timer is set to 16ms when the processor is initialized.
};
typedef struct _cache_mode_register _cache_mode_register;
extern _cache_mode_register CMR;

typedef struct mode_registr
  {
    word1 cuolin;
    word1 solin;
    word1 sdpap;
    word1 separ;
    word2 tm;
    word2 vm;
    word1 hrhlt;
    word1 hrxfr;
    word1 ihr;
    word1 ihrrs;
    word1 mrgctl;
    word1 hexfp;
    word1 emr;
  } _mode_register;
extern _mode_register MR;

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

#define getbit18(x,n)  ((((x) >> (17-n)) & 1) != 0) // return nth bit of an 18bit half word

t_uint64 getbits36(t_uint64 x, int i, unsigned n);
t_uint64 setbits36(t_uint64 x, int p, unsigned n, t_uint64 val);


// single precision fp stuff...
double float36ToIEEEdouble(float36 f36);
float36 IEEEdoubleTofloat36(double f);
// double precision stuff ...
long double EAQToIEEElongdouble(void);
float72 IEEElongdoubleToFloat72(long double f);
void IEEElongdoubleToEAQ(long double f0);

void ufa(DCDstruct *);
void ufs(DCDstruct *);
void fno(DCDstruct *);
void fnoEAQ(word8 *E, word36 *A, word36 *Q);

void fneg(DCDstruct *);
void ufm(DCDstruct *);
void fdv(DCDstruct *);
void fdi(DCDstruct *);
void frd(DCDstruct *);
void fcmp(DCDstruct *);
void fcmg(DCDstruct *);

void dufa(DCDstruct *);
void dufs(DCDstruct *);
void dufm(DCDstruct *);
void dfdv(DCDstruct *);
void dfdi(DCDstruct *);
void dfrd(DCDstruct *);
void dfcmp(DCDstruct *);
void dfcmg(DCDstruct *);

void dvf(DCDstruct *);

void dfstr(DCDstruct *, word36 *Ypair);
void fstr(DCDstruct *, word36 *CY);


#ifndef QUIET_UNUSED
word36 AddSub36 (char op, bool isSigned, word36 op1, word36 op2, word18 flagsToSet, word18 *flags);
#endif
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

extern word24 finalAddress; ///< final 24-bit address for appending unit

extern t_stat loadSpecial(char *buff);

extern bool adrTrace;   ///< when true perform address modification tracing
extern bool apndTrace;  ///< when true do appending unit tracing

//extern t_uint64 cpuCycles; ///< # of instructions executed in this run...
#define cpuCycles sys_stats.total_cycles

extern jmp_buf jmpMain;     ///< This is where we should return to from a fault to retry an instruction
extern int stop_reason;     ///< sim_instr return value for JMP_STOP
#define JMP_RETRY       1   ///< retry instruction
#define JMP_NEXT        2   ///< goto next sequential instruction
#define JMP_TRA         3   ///< treat return as if it were a TRA instruction with rIC already set to where to jump to
#define JMP_STOP        4   ///< treat return as if it were an attempt to unravel the stack and gracefully exit out of sim_instr

/*
 * Stuff to do with the local loader/binder/linker
 */


int removeSegment(char *seg);
int removeSegdef(char *seg, char *sym);
int removeSegref(char *seg, char *sym);
int resolveLinks(bool);
int loadDeferredSegments(bool);
int getAddress(int, int);  // return the 24-bit absolute address of segment + offset
bool getSegmentAddressString(int addr, char *msg);
t_stat createLOT(bool);     // create link offset table segment
t_stat snapLOT(bool);       // fill in link offset table segment
t_stat createStack(int, bool);    // create ring n stack

#define LOT "lot_"

//t_stat createStack(int n);  // xreate stack for ring n

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
    int     offset;     ///< if ext reference is an offset from segname/symbol (display purposes only for now)
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
    
    bool    deferred; ///< if true segment is deferred, not loaded into memory
    
    int     segno;  ///< segment# segment is assigned
    int     ldaddr; ///< address where to load segment
    
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


// End loader stuff ...


/*
 * MMs stuff ...
 */

/*
 * bit#_is_neg()
 *
 * Functions to determine if bit-36, bit-18, or bit-n word's MSB is
 * on.
 */

#define bit36_is_neg(x) (((x) & (((t_uint64)1)<<35)) != 0)

#define bit18_is_neg(x) (((x) & (((t_uint64)1)<<17)) != 0)

#define bit_is_neg(x,n) (((x) & (((t_uint64)1)<<((n)-1))) != 0)

/*
 * SCU and IOM stuff (originally taken from Michael Mondy's work).....
 */


// ============================================================================
// === Misc constants and macros

// Clocks
#ifdef USE_IDLE
#define CLK_TR_HZ (512*1024) // should be 512 kHz, but we'll use 512 Hz for now
#else
#define CLK_TR_HZ (512*1) // should be 512 kHz, but we'll use 512 Hz for now
#endif

#define TR_CLK 1 /* SIMH allows clock ids 0..7 */

// Memory
#define IOM_MBX_LOW 01200
#define IOM_MBX_LEN 02200
#define DN355_MBX_LOW 03400
#define DN355_MBX_LEN 03000

#define ARRAY_SIZE(a) ( sizeof(a) / sizeof((a)[0]) )

enum { seg_bits = 15}; // number of bits in a segment number
enum { n_segments = 1 << seg_bits}; // why does c89 treat enums as more constant than consts?


// simh only explicitly supports a single cpu
#define N_CPU_UNITS 1
#define CPU_UNIT_NUM 0

// Devices connected to a SCU
enum active_dev { ADEV_NONE, ADEV_CPU, ADEV_IOM };

// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
enum dev_type { DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, DEVT_MPC };

typedef unsigned int uint;  // efficient unsigned int, at least 32 bits
typedef unsigned flag_t;    // efficient unsigned flag

// The CPU supports 3 addressing modes
// [CAC] I tell a lie: 4 modes....
typedef enum { ABSOLUTE_mode = ABSOLUTE_MODE, APPEND_mode = APPEND_MODE, BAR_mode = BAR_MODE, TEMPORARY_ABSOLUTE_mode = TEMPORARY_ABSOLUTE_MODE} addr_modes_t;



// The CPU supports a privileged/non-privileged flag (dependent upon
// addressing mode, ring of operation, and settings of the active
// segment when in the appending (segmented) address mode.)
typedef enum { NORMAL_mode, PRIV_mode } instr_modes_t;


// The control unit of the CPU is always in one of several states. We
// don't currently use all of the states used in the physical CPU.
// The FAULT_EXEC cycle did not exist in the physical hardware.
typedef enum {
    ABORT_cycle = ABORT_CYCLE,
    FAULT_cycle = FAULT_CYCLE,
    EXEC_cycle,
    FAULT_EXEC_cycle,
    INTERRUPT_cycle,
    FETCH_cycle = INSTRUCTION_FETCH,
    DIS_cycle // A pseudo cycle for handling the DIS instruction
    // CA FETCH OPSTORE, DIVIDE_EXEC
} cycles_t;

// More emulator state variables for the cpu
// These probably belong elsewhere, perhaps control unit data or the
// cu-history regs...
typedef struct {
    cycles_t cycle;
    uint IC_abs; // translation of odd IC to an absolute address; see ADDRESS of cu history
    flag_t irodd_invalid; // cached odd instr invalid due to memory write by even instr
    uint read_addr; // last absolute read; might be same as CA for our purposes...; see APU RMA
    // flag_t instr_fetch; // true during an instruction fetch
    /* The following are all from the control unit history register: */
    flag_t trgo; // most recent instruction caused a transfer?
    flag_t ic_odd; // executing odd pair?
    flag_t poa; // prepare operand address
    uint opcode; // currently executing opcode
    struct {
        flag_t fhld; // An access violation or directed fault is waiting. AL39 mentions that the APU has this flag, but not where scpr stores it
    } apu_state;

    bool interrupt_flag;
} cpu_state_t;

#if 0
/* Indicator register (14 bits [only positions 18..32 have meaning]) */
typedef struct {
    uint zero;              // bit 18
    uint neg;               // bit 19
    uint carry;             // bit 20; see AL39, 3-6
    uint overflow;          // bit 21
    uint exp_overflow;      // bit 22   (used only by ldi/stct1)
    uint exp_underflow;     // bit 23   (used only by ldi/stct1)
    uint overflow_mask;     // bit 24
    uint tally_runout;      // bit 25
    uint parity_error;      // bit 26   (used only by ldi/stct1)
    uint parity_mask;       // bit 27
    uint not_bar_mode;      // bit 28
    uint truncation;        // bit 29
    uint mid_instr_intr_fault;  // bit 30
    uint abs_mode;          // bit 31
    uint hex_mode;          // bit 32
} IR_t;
#endif

/* MF fields of EIS multi-word instructions -- 7 bits */
typedef struct {
    flag_t ar;
    flag_t rl;
    flag_t id;
    uint reg;  // 4 bits
} eis_mf_t;

/* Format of a 36 bit instruction word */
typedef struct {
    uint addr;    // 18 bits at  0..17; 18 bit offset or seg/offset pair
    uint opcode;  // 10 bits at 18..27
    uint inhibit; //  1 bit  at 28
    union {
        struct {
            uint pr_bit;  // 1 bit at 29; use offset[0..2] as pointer reg
            uint tag;     // 6 bits at 30..35 */
        } single;
        eis_mf_t mf1;     // from bits 29..35 of EIS instructions
    } mods;
    flag_t is_eis_multiword;  // set true for relevent opcodes
    
    t_uint64 *wordEven; // HWR
    
} instr_t;

// extern IR_t IR;                // Indicator register

/* Control unit data (288 bits) */
typedef struct {
    /*
     NB: Some of the data normally stored here is represented
     elsewhere -- e.g.,the PPR is a variable outside of this
     struct.   Other data is live and only stored here.
     */
    /*      This is a collection of flags and registers from the
     appending unit and the control unit.  The scu and rcu
     instructions store and load these values to an 8 word
     memory block.
     The CU data may only be valid for use with the scu and
     rcu instructions.
     Comments indicate format as stored in 8 words by the scu
     instruction.
     */
    
    /* NOTE: PPR (procedure pointer register) is a combination of registers:
     From the Appending Unit
     PRR bits [0..2] of word 0
     PSR bits [3..17] of word 0
     P   bit 18 of word 0
     From the Control Unit
     IC  bits [0..17] of word 4
     */
    
#if 0
    
    /* First we list some registers we either don't use or that we have represented elsewhere */
    
    /* word 0 */
    // PPR portions copied from Appending Unit
    uint PPR_PRR;       /* Procedure ring register; 3 bits @ 0[0..2] */
    uint PPR_PSR;       /* Procedure segment register; 15 bits @ 0[3..17] */
    uint PPR_P;         /* Privileged bit; 1 bit @ 0[18] */
    // uint64 word0bits; /* Word 0, bits 18..32 (all for the APU) */
    uint FCT;           /* Fault counter; 3 bits at 0[33..35]; */
    
    /* word 1 */
    //uint64 word1bits; /* Word1, bits [0..19] and [35] */
    
    uint IA;        /* 4 bits @ 1[20..23] */
    uint IACHN;     /* 3 bits @ 1[24..26] */
    uint CNCHN;     /* 3 bits @ 1[27..29] */
    uint FIADDR     /* 5 bits @ 1[30..34] */
    
    /* word 2 */
    uint TPR_TRR;   // 3 bits @ 2[0..2];  temporary ring register
    uint TPR_TSR;   // 15 bits @ 2[3..17]; temporary segment register
    // unused: 10 bits at 2[18..27]
    // uint cpu_no; // 2 bits at 2[28..29]; from maint panel switches
    
    /* word 3 */
    
    /* word 4 */
    // IC belongs to CU
    int IC;         // 18 bits at 4[0..17]; instruction counter aka ilc
    // copy of IR bits 14 bits at 4[18..31]
    // unused: 4 bits at 4[32..36];
    
    /* word 5 */
    uint CA;        // 18 bits at 5[0..17]; computed address value (offset) used in the last address preparation cycle
    // cu bits for repeats, execute double, restarts, etc
#endif
    
    /* Above was documentation on all physical control unit data.
     * Below are the members we actually implement here.  Missing
     * members are either not (yet) emulated or are handled outside
     * of this control unit data struct.
     */
    
    /* word 0, continued */
    flag_t SD_ON;   // SDWAM enabled
    flag_t PT_ON;   // PTWAM enabled
    
    /* word 1, continued  */
    struct {
        unsigned oosb:1;    // out of segment bounds
        unsigned ocall:1;   // outward call
        // unsigned boc:1;      // bad outward call
        // unsigned ocb:1;      // out of call brackets
    } word1flags;
    flag_t instr_fetch;     // our usage of this may match PI-AP
    
    /* word 2, continued */
    uint delta;     // 6 bits at 2[30..35]; addr increment for repeats
    
    /* word 5, continued */
    flag_t rpts;        // just executed a repeat instr;  bit 12 in word one of the CU history register
    flag_t repeat_first;        // "RF" flag -- first cycle of a repeat instruction; We also use with xed
    flag_t rpt;     // execute an rpt instruction
    uint CT_HOLD;   // 6 bits at 5[30..35]; contents of the "remember modifier" register
    flag_t xde;     // execute even instr from xed pair
    flag_t xdo;     // execute even instr from xed pair
    
    /* word 6 */
    //instr_t IR;     /* Working instr register; addr & tag are modified */
    word18 IR;     /* Working instr register; addr & tag are modified */
    uint tag;       // td portion of instr tag (we only update this for rpt instructions which is the only time we need it)
    
    /* word 7 */
    // instr_t IRODD;   // Instr holding register; odd word of last pair fetched
    t_uint64 IRODD; /* Instr holding register; odd word of last pair fetched */
    
} ctl_unit_data_t;


// Emulator-only interrupt and fault info
typedef struct {
    flag_t xed; // executed xed for a fault handler
    flag_t any; // true if any of the below are true
    flag_t int_pending;
    int low_group; // Lowest group-number fault preset
    uint32 group7; // bitmask for multiple group 7 faults
    int fault[7]; // only one fault in groups 1..6 can be pending
    flag_t interrupts[32];
} events_t;


#define N_CPU_PORTS 4
// Physical Switches
typedef struct {
    // Switches on the Processor's maintenance and configuration panels
    uint FLT_BASE; // normally 7 MSB of 12bit fault base addr
    uint cpu_num;  // zero for CPU 'A', one for 'B' etc.
    word36 data_switches;
    //uint port_enable; // 4 bits; enable ports A-D
    //word36 port_config; // Read by rsw instruction; format unknown
    //uint port_interlace; // 4 bits  Read by rsw instruction; 
    uint assignment [N_CPU_PORTS];
    uint interlace [N_CPU_PORTS]; // 0/2/4
    uint enable [N_CPU_PORTS];
    uint init_enable [N_CPU_PORTS];
    uint store_size [N_CPU_PORTS]; // 0-7 encoding 32K-4M
    uint proc_mode; // 1 bit  Read by rsw instruction; format unknown
    uint proc_speed; // 4 bits Read by rsw instruction; format unknown
    uint invert_absolute; // If non-zero, invert the sense of the ABSOLUTE bit in the STI instruction
    uint b29_test; // If non-zero, enable untested code
    uint dis_enable; // If non-zero, DIS works
    uint auto_append_disable; // If non-zero, bit29 does not force APPEND_mode
    uint lprp_highonly; // If non-zero lprp only sets the high bits
    uint steady_clock; // If non-zero the clock is tied to the cycle counter
    uint degenerate_mode; // If non-zero use the experimental ABSOLUTE mode
    uint append_after; // 
} switches_t;

// System-wide info and options not tied to a specific CPU, IOM, or SCU
typedef struct {
    int clock_speed;
    // Instructions rccl and rscr allow access to a hardware clock.
    // If zero, the hardware clock returns the real time of day.
    // If non-zero, the clock starts at an arbitrary date and ticks at
    // a rate approximately equal to the given number of instructions
    // per second.
    // Delay times are in cycles; negative for immediate
    struct {
        int connect;    // Delay between CIOC instr & connect channel operation
        int chan_activate;  // Time for a list service to send a DCW
    } iom_times;
    struct {
        int read;
        int xfer;
    } mt_times;
    flag_t warn_uninit; // Warn when reading uninitialized memory
} sysinfo_t;

// Statistics
typedef struct {
    struct {
        uint nexec;
        uint nmsec; // FIXME: WARNING: if 32 bits, only good for ~47 days :-)
    } instr[1024];
    t_uint64 total_cycles;      // Used for statistics and for simulated clock
    t_uint64 total_instr;
    t_uint64 total_msec;
    uint n_instr;       // Reset to zero on each call to sim_instr()
} stats_t;



extern sysinfo_t sys_opts;

extern events_t events;
extern switches_t switches;
// the following two should probably be combined
extern cpu_state_t cpu;
extern ctl_unit_data_t cu;
extern stats_t sys_stats;
extern int is_eis[];

extern flag_t fault_gen_no_fault;

//extern int fault2group[32];
//extern int fault2prio[32];

/* XXX these OUGHT to return fault codes! (and interrupt numbers???) */
void iom_cioc(word8 conchan);
void iom_cioc_cow(word18 cowaddr);
int iom_setup(const char *confname);

extern int console_chan;

void cancel_run(t_stat reason);



// MM's opcode stuff ...

extern char *op0text[512];
extern char *op1text[512];
extern char *opcodes2text[1024];

// Opcodes with low bit (bit 27) == 0.  Enum value is value of upper 9 bits.
typedef enum {
	opcode0_mme    = 0001, // (1 decimal)
	opcode0_drl    = 0002, // (2 decimal)
	opcode0_mme2   = 0004, // (4 decimal)
	opcode0_mme3   = 0005, // (5 decimal)
	opcode0_mme4   = 0007, // (7 decimal)
	opcode0_nop    = 0011, // (9 decimal)
	opcode0_puls1  = 0012, // (10 decimal)
	opcode0_puls2  = 0013, // (11 decimal)
	opcode0_cioc   = 0015, // (13 decimal)
	opcode0_adlx0  = 0020, // (16 decimal)
	opcode0_adlx1  = 0021, // (17 decimal)
	opcode0_adlx2  = 0022, // (18 decimal)
	opcode0_adlx3  = 0023, // (19 decimal)
	opcode0_adlx4  = 0024, // (20 decimal)
	opcode0_adlx5  = 0025, // (21 decimal)
	opcode0_adlx6  = 0026, // (22 decimal)
	opcode0_adlx7  = 0027, // (23 decimal)
	opcode0_ldqc   = 0032, // (26 decimal)
	opcode0_adl    = 0033, // (27 decimal)
	opcode0_ldac   = 0034, // (28 decimal)
	opcode0_adla   = 0035, // (29 decimal)
	opcode0_adlq   = 0036, // (30 decimal)
	opcode0_adlaq  = 0037, // (31 decimal)
	opcode0_asx0   = 0040, // (32 decimal)
	opcode0_asx1   = 0041, // (33 decimal)
	opcode0_asx2   = 0042, // (34 decimal)
	opcode0_asx3   = 0043, // (35 decimal)
	opcode0_asx4   = 0044, // (36 decimal)
	opcode0_asx5   = 0045, // (37 decimal)
	opcode0_asx6   = 0046, // (38 decimal)
	opcode0_asx7   = 0047, // (39 decimal)
	opcode0_adwp0  = 0050, // (40 decimal)
	opcode0_adwp1  = 0051, // (41 decimal)
	opcode0_adwp2  = 0052, // (42 decimal)
	opcode0_adwp3  = 0053, // (43 decimal)
	opcode0_aos    = 0054, // (44 decimal)
	opcode0_asa    = 0055, // (45 decimal)
	opcode0_asq    = 0056, // (46 decimal)
	opcode0_sscr   = 0057, // (47 decimal)
	opcode0_adx0   = 0060, // (48 decimal)
	opcode0_adx1   = 0061, // (49 decimal)
	opcode0_adx2   = 0062, // (50 decimal)
	opcode0_adx3   = 0063, // (51 decimal)
	opcode0_adx4   = 0064, // (52 decimal)
	opcode0_adx5   = 0065, // (53 decimal)
	opcode0_adx6   = 0066, // (54 decimal)
	opcode0_adx7   = 0067, // (55 decimal)
	opcode0_awca   = 0071, // (57 decimal)
	opcode0_awcq   = 0072, // (58 decimal)
	opcode0_lreg   = 0073, // (59 decimal)
	opcode0_ada    = 0075, // (61 decimal)
	opcode0_adq    = 0076, // (62 decimal)
	opcode0_adaq   = 0077, // (63 decimal)
	opcode0_cmpx0  = 0100, // (64 decimal)
	opcode0_cmpx1  = 0101, // (65 decimal)
	opcode0_cmpx2  = 0102, // (66 decimal)
	opcode0_cmpx3  = 0103, // (67 decimal)
	opcode0_cmpx4  = 0104, // (68 decimal)
	opcode0_cmpx5  = 0105, // (69 decimal)
	opcode0_cmpx6  = 0106, // (70 decimal)
	opcode0_cmpx7  = 0107, // (71 decimal)
	opcode0_cwl    = 0111, // (73 decimal)
	opcode0_cmpa   = 0115, // (77 decimal)
	opcode0_cmpq   = 0116, // (78 decimal)
	opcode0_cmpaq  = 0117, // (79 decimal)
	opcode0_sblx0  = 0120, // (80 decimal)
	opcode0_sblx1  = 0121, // (81 decimal)
	opcode0_sblx2  = 0122, // (82 decimal)
	opcode0_sblx3  = 0123, // (83 decimal)
	opcode0_sblx4  = 0124, // (84 decimal)
	opcode0_sblx5  = 0125, // (85 decimal)
	opcode0_sblx6  = 0126, // (86 decimal)
	opcode0_sblx7  = 0127, // (87 decimal)
	opcode0_sbla   = 0135, // (93 decimal)
	opcode0_sblq   = 0136, // (94 decimal)
	opcode0_sblaq  = 0137, // (95 decimal)
	opcode0_ssx0   = 0140, // (96 decimal)
	opcode0_ssx1   = 0141, // (97 decimal)
	opcode0_ssx2   = 0142, // (98 decimal)
	opcode0_ssx3   = 0143, // (99 decimal)
	opcode0_ssx4   = 0144, // (100 decimal)
	opcode0_ssx5   = 0145, // (101 decimal)
	opcode0_ssx6   = 0146, // (102 decimal)
	opcode0_ssx7   = 0147, // (103 decimal)
	opcode0_adwp4  = 0150, // (104 decimal)
	opcode0_adwp5  = 0151, // (105 decimal)
	opcode0_adwp6  = 0152, // (106 decimal)
	opcode0_adwp7  = 0153, // (107 decimal)
	opcode0_sdbr   = 0154, // (108 decimal)
	opcode0_ssa    = 0155, // (109 decimal)
	opcode0_ssq    = 0156, // (110 decimal)
	opcode0_sbx0   = 0160, // (112 decimal)
	opcode0_sbx1   = 0161, // (113 decimal)
	opcode0_sbx2   = 0162, // (114 decimal)
	opcode0_sbx3   = 0163, // (115 decimal)
	opcode0_sbx4   = 0164, // (116 decimal)
	opcode0_sbx5   = 0165, // (117 decimal)
	opcode0_sbx6   = 0166, // (118 decimal)
	opcode0_sbx7   = 0167, // (119 decimal)
	opcode0_swca   = 0171, // (121 decimal)
	opcode0_swcq   = 0172, // (122 decimal)
	opcode0_lpri   = 0173, // (123 decimal)
	opcode0_sba    = 0175, // (125 decimal)
	opcode0_sbq    = 0176, // (126 decimal)
	opcode0_sbaq   = 0177, // (127 decimal)
	opcode0_cnax0  = 0200, // (128 decimal)
	opcode0_cnax1  = 0201, // (129 decimal)
	opcode0_cnax2  = 0202, // (130 decimal)
	opcode0_cnax3  = 0203, // (131 decimal)
	opcode0_cnax4  = 0204, // (132 decimal)
	opcode0_cnax5  = 0205, // (133 decimal)
	opcode0_cnax6  = 0206, // (134 decimal)
	opcode0_cnax7  = 0207, // (135 decimal)
	opcode0_cmk    = 0211, // (137 decimal)
	opcode0_absa   = 0212, // (138 decimal)
	opcode0_epaq   = 0213, // (139 decimal)
	opcode0_sznc   = 0214, // (140 decimal)
	opcode0_cnaa   = 0215, // (141 decimal)
	opcode0_cnaq   = 0216, // (142 decimal)
	opcode0_cnaaq  = 0217, // (143 decimal)
	opcode0_ldx0   = 0220, // (144 decimal)
	opcode0_ldx1   = 0221, // (145 decimal)
	opcode0_ldx2   = 0222, // (146 decimal)
	opcode0_ldx3   = 0223, // (147 decimal)
	opcode0_ldx4   = 0224, // (148 decimal)
	opcode0_ldx5   = 0225, // (149 decimal)
	opcode0_ldx6   = 0226, // (150 decimal)
	opcode0_ldx7   = 0227, // (151 decimal)
	opcode0_lbar   = 0230, // (152 decimal)
	opcode0_rsw    = 0231, // (153 decimal)
	opcode0_ldbr   = 0232, // (154 decimal)
	opcode0_rmcm   = 0233, // (155 decimal)
	opcode0_szn    = 0234, // (156 decimal)
	opcode0_lda    = 0235, // (157 decimal)
	opcode0_ldq    = 0236, // (158 decimal)
	opcode0_ldaq   = 0237, // (159 decimal)
	opcode0_orsx0  = 0240, // (160 decimal)
	opcode0_orsx1  = 0241, // (161 decimal)
	opcode0_orsx2  = 0242, // (162 decimal)
	opcode0_orsx3  = 0243, // (163 decimal)
	opcode0_orsx4  = 0244, // (164 decimal)
	opcode0_orsx5  = 0245, // (165 decimal)
	opcode0_orsx6  = 0246, // (166 decimal)
	opcode0_orsx7  = 0247, // (167 decimal)
	opcode0_spri0  = 0250, // (168 decimal)
	opcode0_spbp1  = 0251, // (169 decimal)
	opcode0_spri2  = 0252, // (170 decimal)
	opcode0_spbp3  = 0253, // (171 decimal)
	opcode0_spri   = 0254, // (172 decimal)
	opcode0_orsa   = 0255, // (173 decimal)
	opcode0_orsq   = 0256, // (174 decimal)
	opcode0_lsdp   = 0257, // (175 decimal)
	opcode0_orx0   = 0260, // (176 decimal)
	opcode0_orx1   = 0261, // (177 decimal)
	opcode0_orx2   = 0262, // (178 decimal)
	opcode0_orx3   = 0263, // (179 decimal)
	opcode0_orx4   = 0264, // (180 decimal)
	opcode0_orx5   = 0265, // (181 decimal)
	opcode0_orx6   = 0266, // (182 decimal)
	opcode0_orx7   = 0267, // (183 decimal)
	opcode0_tsp0   = 0270, // (184 decimal)
	opcode0_tsp1   = 0271, // (185 decimal)
	opcode0_tsp2   = 0272, // (186 decimal)
	opcode0_tsp3   = 0273, // (187 decimal)
	opcode0_ora    = 0275, // (189 decimal)
	opcode0_orq    = 0276, // (190 decimal)
	opcode0_oraq   = 0277, // (191 decimal)
	opcode0_canx0  = 0300, // (192 decimal)
	opcode0_canx1  = 0301, // (193 decimal)
	opcode0_canx2  = 0302, // (194 decimal)
	opcode0_canx3  = 0303, // (195 decimal)
	opcode0_canx4  = 0304, // (196 decimal)
	opcode0_canx5  = 0305, // (197 decimal)
	opcode0_canx6  = 0306, // (198 decimal)
	opcode0_canx7  = 0307, // (199 decimal)
	opcode0_eawp0  = 0310, // (200 decimal)
	opcode0_easp0  = 0311, // (201 decimal)
	opcode0_eawp2  = 0312, // (202 decimal)
	opcode0_easp2  = 0313, // (203 decimal)
	opcode0_cana   = 0315, // (205 decimal)
	opcode0_canq   = 0316, // (206 decimal)
	opcode0_canaq  = 0317, // (207 decimal)
	opcode0_lcx0   = 0320, // (208 decimal)
	opcode0_lcx1   = 0321, // (209 decimal)
	opcode0_lcx2   = 0322, // (210 decimal)
	opcode0_lcx3   = 0323, // (211 decimal)
	opcode0_lcx4   = 0324, // (212 decimal)
	opcode0_lcx5   = 0325, // (213 decimal)
	opcode0_lcx6   = 0326, // (214 decimal)
	opcode0_lcx7   = 0327, // (215 decimal)
	opcode0_eawp4  = 0330, // (216 decimal)
	opcode0_easp4  = 0331, // (217 decimal)
	opcode0_eawp6  = 0332, // (218 decimal)
	opcode0_easp6  = 0333, // (219 decimal)
	opcode0_lca    = 0335, // (221 decimal)
	opcode0_lcq    = 0336, // (222 decimal)
	opcode0_lcaq   = 0337, // (223 decimal)
	opcode0_ansx0  = 0340, // (224 decimal)
	opcode0_ansx1  = 0341, // (225 decimal)
	opcode0_ansx2  = 0342, // (226 decimal)
	opcode0_ansx3  = 0343, // (227 decimal)
	opcode0_ansx4  = 0344, // (228 decimal)
	opcode0_ansx5  = 0345, // (229 decimal)
	opcode0_ansx6  = 0346, // (230 decimal)
	opcode0_ansx7  = 0347, // (231 decimal)
	opcode0_epp0   = 0350, // (232 decimal)
	opcode0_epbp1  = 0351, // (233 decimal)
	opcode0_epp2   = 0352, // (234 decimal)
	opcode0_epbp3  = 0353, // (235 decimal)
	opcode0_stac   = 0354, // (236 decimal)
	opcode0_ansa   = 0355, // (237 decimal)
	opcode0_ansq   = 0356, // (238 decimal)
	opcode0_stcd   = 0357, // (239 decimal)
	opcode0_anx0   = 0360, // (240 decimal)
	opcode0_anx1   = 0361, // (241 decimal)
	opcode0_anx2   = 0362, // (242 decimal)
	opcode0_anx3   = 0363, // (243 decimal)
	opcode0_anx4   = 0364, // (244 decimal)
	opcode0_anx5   = 0365, // (245 decimal)
	opcode0_anx6   = 0366, // (246 decimal)
	opcode0_anx7   = 0367, // (247 decimal)
	opcode0_epp4   = 0370, // (248 decimal)
	opcode0_epbp5  = 0371, // (249 decimal)
	opcode0_epp6   = 0372, // (250 decimal)
	opcode0_epbp7  = 0373, // (251 decimal)
	opcode0_ana    = 0375, // (253 decimal)
	opcode0_anq    = 0376, // (254 decimal)
	opcode0_anaq   = 0377, // (255 decimal)
	opcode0_mpf    = 0401, // (257 decimal)
	opcode0_mpy    = 0402, // (258 decimal)
	opcode0_cmg    = 0405, // (261 decimal)
	opcode0_lde    = 0411, // (265 decimal)
	opcode0_rscr   = 0413, // (267 decimal)
	opcode0_ade    = 0415, // (269 decimal)
	opcode0_ufm    = 0421, // (273 decimal)
	opcode0_dufm   = 0423, // (275 decimal)
	opcode0_fcmg   = 0425, // (277 decimal)
	opcode0_dfcmg  = 0427, // (279 decimal)
	opcode0_fszn   = 0430, // (280 decimal)
	opcode0_fld    = 0431, // (281 decimal)
	opcode0_dfld   = 0433, // (283 decimal)
	opcode0_ufa    = 0435, // (285 decimal)
	opcode0_dufa   = 0437, // (287 decimal)
	opcode0_sxl0   = 0440, // (288 decimal)
	opcode0_sxl1   = 0441, // (289 decimal)
	opcode0_sxl2   = 0442, // (290 decimal)
	opcode0_sxl3   = 0443, // (291 decimal)
	opcode0_sxl4   = 0444, // (292 decimal)
	opcode0_sxl5   = 0445, // (293 decimal)
	opcode0_sxl6   = 0446, // (294 decimal)
	opcode0_sxl7   = 0447, // (295 decimal)
	opcode0_stz    = 0450, // (296 decimal)
	opcode0_smic   = 0451, // (297 decimal)
	opcode0_scpr   = 0452, // (298 decimal)
	opcode0_stt    = 0454, // (300 decimal)
	opcode0_fst    = 0455, // (301 decimal)
	opcode0_ste    = 0456, // (302 decimal)
	opcode0_dfst   = 0457, // (303 decimal)
	opcode0_fmp    = 0461, // (305 decimal)
	opcode0_dfmp   = 0463, // (307 decimal)
	opcode0_fstr   = 0470, // (312 decimal)
	opcode0_frd    = 0471, // (313 decimal)
	opcode0_dfstr  = 0472, // (314 decimal)
	opcode0_dfrd   = 0473, // (315 decimal)
	opcode0_fad    = 0475, // (317 decimal)
	opcode0_dfad   = 0477, // (319 decimal)
	opcode0_rpl    = 0500, // (320 decimal)
	opcode0_bcd    = 0505, // (325 decimal)
	opcode0_div    = 0506, // (326 decimal)
	opcode0_dvf    = 0507, // (327 decimal)
	opcode0_fneg   = 0513, // (331 decimal)
	opcode0_fcmp   = 0515, // (333 decimal)
	opcode0_dfcmp  = 0517, // (335 decimal)
	opcode0_rpt    = 0520, // (336 decimal)
	opcode0_fdi    = 0525, // (341 decimal)
	opcode0_dfdi   = 0527, // (343 decimal)
	opcode0_neg    = 0531, // (345 decimal)
	opcode0_cams   = 0532, // (346 decimal)
	opcode0_negl   = 0533, // (347 decimal)
	opcode0_ufs    = 0535, // (349 decimal)
	opcode0_dufs   = 0537, // (351 decimal)
	opcode0_sprp0  = 0540, // (352 decimal)
	opcode0_sprp1  = 0541, // (353 decimal)
	opcode0_sprp2  = 0542, // (354 decimal)
	opcode0_sprp3  = 0543, // (355 decimal)
	opcode0_sprp4  = 0544, // (356 decimal)
	opcode0_sprp5  = 0545, // (357 decimal)
	opcode0_sprp6  = 0546, // (358 decimal)
	opcode0_sprp7  = 0547, // (359 decimal)
	opcode0_sbar   = 0550, // (360 decimal)
	opcode0_stba   = 0551, // (361 decimal)
	opcode0_stbq   = 0552, // (362 decimal)
	opcode0_smcm   = 0553, // (363 decimal)
	opcode0_stc1   = 0554, // (364 decimal)
	opcode0_ssdp   = 0557, // (367 decimal)
	opcode0_rpd    = 0560, // (368 decimal)
	opcode0_fdv    = 0565, // (373 decimal)
	opcode0_dfdv   = 0567, // (375 decimal)
	opcode0_fno    = 0573, // (379 decimal)
	opcode0_fsb    = 0575, // (381 decimal)
	opcode0_dfsb   = 0577, // (383 decimal)
	opcode0_tze    = 0600, // (384 decimal)
	opcode0_tnz    = 0601, // (385 decimal)
	opcode0_tnc    = 0602, // (386 decimal)
	opcode0_trc    = 0603, // (387 decimal)
	opcode0_tmi    = 0604, // (388 decimal)
	opcode0_tpl    = 0605, // (389 decimal)
	opcode0_ttf    = 0607, // (391 decimal)
	opcode0_rtcd   = 0610, // (392 decimal)
	opcode0_rcu    = 0613, // (395 decimal)
	opcode0_teo    = 0614, // (396 decimal)
	opcode0_teu    = 0615, // (397 decimal)
	opcode0_dis    = 0616, // (398 decimal)
	opcode0_tov    = 0617, // (399 decimal)
	opcode0_eax0   = 0620, // (400 decimal)
	opcode0_eax1   = 0621, // (401 decimal)
	opcode0_eax2   = 0622, // (402 decimal)
	opcode0_eax3   = 0623, // (403 decimal)
	opcode0_eax4   = 0624, // (404 decimal)
	opcode0_eax5   = 0625, // (405 decimal)
	opcode0_eax6   = 0626, // (406 decimal)
	opcode0_eax7   = 0627, // (407 decimal)
	opcode0_ret    = 0630, // (408 decimal)
	opcode0_rccl   = 0633, // (411 decimal)
	opcode0_ldi    = 0634, // (412 decimal)
	opcode0_eaa    = 0635, // (413 decimal)
	opcode0_eaq    = 0636, // (414 decimal)
	opcode0_ldt    = 0637, // (415 decimal)
	opcode0_ersx0  = 0640, // (416 decimal)
	opcode0_ersx1  = 0641, // (417 decimal)
	opcode0_ersx2  = 0642, // (418 decimal)
	opcode0_ersx3  = 0643, // (419 decimal)
	opcode0_ersx4  = 0644, // (420 decimal)
	opcode0_ersx5  = 0645, // (421 decimal)
	opcode0_ersx6  = 0646, // (422 decimal)
	opcode0_ersx7  = 0647, // (423 decimal)
	opcode0_spri4  = 0650, // (424 decimal)
	opcode0_spbp5  = 0651, // (425 decimal)
	opcode0_spri6  = 0652, // (426 decimal)
	opcode0_spbp7  = 0653, // (427 decimal)
	opcode0_stacq  = 0654, // (428 decimal)
	opcode0_ersa   = 0655, // (429 decimal)
	opcode0_ersq   = 0656, // (430 decimal)
	opcode0_scu    = 0657, // (431 decimal)
	opcode0_erx0   = 0660, // (432 decimal)
	opcode0_erx1   = 0661, // (433 decimal)
	opcode0_erx2   = 0662, // (434 decimal)
	opcode0_erx3   = 0663, // (435 decimal)
	opcode0_erx4   = 0664, // (436 decimal)
	opcode0_erx5   = 0665, // (437 decimal)
	opcode0_erx6   = 0666, // (438 decimal)
	opcode0_erx7   = 0667, // (439 decimal)
	opcode0_tsp4   = 0670, // (440 decimal)
	opcode0_tsp5   = 0671, // (441 decimal)
	opcode0_tsp6   = 0672, // (442 decimal)
	opcode0_tsp7   = 0673, // (443 decimal)
	opcode0_lcpr   = 0674, // (444 decimal)
	opcode0_era    = 0675, // (445 decimal)
	opcode0_erq    = 0676, // (446 decimal)
	opcode0_eraq   = 0677, // (447 decimal)
	opcode0_tsx0   = 0700, // (448 decimal)
	opcode0_tsx1   = 0701, // (449 decimal)
	opcode0_tsx2   = 0702, // (450 decimal)
	opcode0_tsx3   = 0703, // (451 decimal)
	opcode0_tsx4   = 0704, // (452 decimal)
	opcode0_tsx5   = 0705, // (453 decimal)
	opcode0_tsx6   = 0706, // (454 decimal)
	opcode0_tsx7   = 0707, // (455 decimal)
	opcode0_tra    = 0710, // (456 decimal)
	opcode0_call6  = 0713, // (459 decimal)
	opcode0_tss    = 0715, // (461 decimal)
	opcode0_xec    = 0716, // (462 decimal)
	opcode0_xed    = 0717, // (463 decimal)
	opcode0_lxl0   = 0720, // (464 decimal)
	opcode0_lxl1   = 0721, // (465 decimal)
	opcode0_lxl2   = 0722, // (466 decimal)
	opcode0_lxl3   = 0723, // (467 decimal)
	opcode0_lxl4   = 0724, // (468 decimal)
	opcode0_lxl5   = 0725, // (469 decimal)
	opcode0_lxl6   = 0726, // (470 decimal)
	opcode0_lxl7   = 0727, // (471 decimal)
	opcode0_ars    = 0731, // (473 decimal)
	opcode0_qrs    = 0732, // (474 decimal)
	opcode0_lrs    = 0733, // (475 decimal)
	opcode0_als    = 0735, // (477 decimal)
	opcode0_qls    = 0736, // (478 decimal)
	opcode0_lls    = 0737, // (479 decimal)
	opcode0_stx0   = 0740, // (480 decimal)
	opcode0_stx1   = 0741, // (481 decimal)
	opcode0_stx2   = 0742, // (482 decimal)
	opcode0_stx3   = 0743, // (483 decimal)
	opcode0_stx4   = 0744, // (484 decimal)
	opcode0_stx5   = 0745, // (485 decimal)
	opcode0_stx6   = 0746, // (486 decimal)
	opcode0_stx7   = 0747, // (487 decimal)
	opcode0_stc2   = 0750, // (488 decimal)
	opcode0_stca   = 0751, // (489 decimal)
	opcode0_stcq   = 0752, // (490 decimal)
	opcode0_sreg   = 0753, // (491 decimal)
	opcode0_sti    = 0754, // (492 decimal)
	opcode0_sta    = 0755, // (493 decimal)
	opcode0_stq    = 0756, // (494 decimal)
	opcode0_staq   = 0757, // (495 decimal)
	opcode0_lprp0  = 0760, // (496 decimal)
	opcode0_lprp1  = 0761, // (497 decimal)
	opcode0_lprp2  = 0762, // (498 decimal)
	opcode0_lprp3  = 0763, // (499 decimal)
	opcode0_lprp4  = 0764, // (500 decimal)
	opcode0_lprp5  = 0765, // (501 decimal)
	opcode0_lprp6  = 0766, // (502 decimal)
	opcode0_lprp7  = 0767, // (503 decimal)
	opcode0_arl    = 0771, // (505 decimal)
	opcode0_qrl    = 0772, // (506 decimal)
	opcode0_lrl    = 0773, // (507 decimal)
	opcode0_gtb    = 0774, // (508 decimal)
	opcode0_alr    = 0775, // (509 decimal)
	opcode0_qlr    = 0776, // (510 decimal)
	opcode0_llr    = 0777  // (511 decimal)
} opcode0_t;

// Opcodes with low bit (bit 27) == 1.  Enum value is value of upper 9 bits.
typedef enum {
	opcode1_mve    = 0020, // (16 decimal)
	opcode1_mvne   = 0024, // (20 decimal)
	opcode1_csl    = 0060, // (48 decimal)
	opcode1_csr    = 0061, // (49 decimal)
	opcode1_sztl   = 0064, // (52 decimal)
	opcode1_sztr   = 0065, // (53 decimal)
	opcode1_cmpb   = 0066, // (54 decimal)
	opcode1_mlr    = 0100, // (64 decimal)
	opcode1_mrl    = 0101, // (65 decimal)
	opcode1_cmpc   = 0106, // (70 decimal)
	opcode1_scd    = 0120, // (80 decimal)
	opcode1_scdr   = 0121, // (81 decimal)
	opcode1_scm    = 0124, // (84 decimal)
	opcode1_scmr   = 0125, // (85 decimal)
	opcode1_sptr   = 0154, // (108 decimal)
	opcode1_mvt    = 0160, // (112 decimal)
	opcode1_tct    = 0164, // (116 decimal)
	opcode1_tctr   = 0165, // (117 decimal)
	opcode1_lptr   = 0173, // (123 decimal)
	opcode1_ad2d   = 0202, // (130 decimal)
	opcode1_sb2d   = 0203, // (131 decimal)
	opcode1_mp2d   = 0206, // (134 decimal)
	opcode1_dv2d   = 0207, // (135 decimal)
	opcode1_ad3d   = 0222, // (146 decimal)
	opcode1_sb3d   = 0223, // (147 decimal)
	opcode1_mp3d   = 0226, // (150 decimal)
	opcode1_dv3d   = 0227, // (151 decimal)
	opcode1_lsdr   = 0232, // (154 decimal)
	opcode1_spbp0  = 0250, // (168 decimal)
	opcode1_spri1  = 0251, // (169 decimal)
	opcode1_spbp2  = 0252, // (170 decimal)
	opcode1_spri3  = 0253, // (171 decimal)
	opcode1_ssdr   = 0254, // (172 decimal)
	opcode1_lptp   = 0257, // (175 decimal)
	opcode1_mvn    = 0300, // (192 decimal)
	opcode1_btd    = 0301, // (193 decimal)
	opcode1_cmpn   = 0303, // (195 decimal)
	opcode1_dtb    = 0305, // (197 decimal)
	opcode1_easp1  = 0310, // (200 decimal)
	opcode1_eawp1  = 0311, // (201 decimal)
	opcode1_easp3  = 0312, // (202 decimal)
	opcode1_eawp3  = 0313, // (203 decimal)
	opcode1_easp5  = 0330, // (216 decimal)
	opcode1_eawp5  = 0331, // (217 decimal)
	opcode1_easp7  = 0332, // (218 decimal)
	opcode1_eawp7  = 0333, // (219 decimal)
	opcode1_epbp0  = 0350, // (232 decimal)
	opcode1_epp1   = 0351, // (233 decimal)
	opcode1_epbp2  = 0352, // (234 decimal)
	opcode1_epp3   = 0353, // (235 decimal)
	opcode1_epbp4  = 0370, // (248 decimal)
	opcode1_epp5   = 0371, // (249 decimal)
	opcode1_epbp6  = 0372, // (250 decimal)
	opcode1_epp7   = 0373, // (251 decimal)
	opcode1_sareg  = 0443, // (291 decimal)
	opcode1_spl    = 0447, // (295 decimal)
	opcode1_lareg  = 0463, // (307 decimal)
	opcode1_lpl    = 0467, // (311 decimal)
	opcode1_a9bd   = 0500, // (320 decimal)
	opcode1_a6bd   = 0501, // (321 decimal)
	opcode1_a4bd   = 0502, // (322 decimal)
	opcode1_abd    = 0503, // (323 decimal)
	opcode1_awd    = 0507, // (327 decimal)
	opcode1_s9bd   = 0520, // (336 decimal)
	opcode1_s6bd   = 0521, // (337 decimal)
	opcode1_s4bd   = 0522, // (338 decimal)
	opcode1_sbd    = 0523, // (339 decimal)
	opcode1_swd    = 0527, // (343 decimal)
	opcode1_camp   = 0532, // (346 decimal)
	opcode1_ara0   = 0540, // (352 decimal)
	opcode1_ara1   = 0541, // (353 decimal)
	opcode1_ara2   = 0542, // (354 decimal)
	opcode1_ara3   = 0543, // (355 decimal)
	opcode1_ara4   = 0544, // (356 decimal)
	opcode1_ara5   = 0545, // (357 decimal)
	opcode1_ara6   = 0546, // (358 decimal)
	opcode1_ara7   = 0547, // (359 decimal)
	opcode1_sptp   = 0557, // (367 decimal)
	opcode1_aar0   = 0560, // (368 decimal)
	opcode1_aar1   = 0561, // (369 decimal)
	opcode1_aar2   = 0562, // (370 decimal)
	opcode1_aar3   = 0563, // (371 decimal)
	opcode1_aar4   = 0564, // (372 decimal)
	opcode1_aar5   = 0565, // (373 decimal)
	opcode1_aar6   = 0566, // (374 decimal)
	opcode1_aar7   = 0567, // (375 decimal)
	opcode1_trtn   = 0600, // (384 decimal)
	opcode1_trtf   = 0601, // (385 decimal)
	opcode1_tmoz   = 0604, // (388 decimal)
	opcode1_tpnz   = 0605, // (389 decimal)
	opcode1_ttn    = 0606, // (390 decimal)
	opcode1_arn0   = 0640, // (416 decimal)
	opcode1_arn1   = 0641, // (417 decimal)
	opcode1_arn2   = 0642, // (418 decimal)
	opcode1_arn3   = 0643, // (419 decimal)
	opcode1_arn4   = 0644, // (420 decimal)
	opcode1_arn5   = 0645, // (421 decimal)
	opcode1_arn6   = 0646, // (422 decimal)
	opcode1_arn7   = 0647, // (423 decimal)
	opcode1_spbp4  = 0650, // (424 decimal)
	opcode1_spri5  = 0651, // (425 decimal)
	opcode1_spbp6  = 0652, // (426 decimal)
	opcode1_spri7  = 0653, // (427 decimal)
	opcode1_nar0   = 0660, // (432 decimal)
	opcode1_nar1   = 0661, // (433 decimal)
	opcode1_nar2   = 0662, // (434 decimal)
	opcode1_nar3   = 0663, // (435 decimal)
	opcode1_nar4   = 0664, // (436 decimal)
	opcode1_nar5   = 0665, // (437 decimal)
	opcode1_nar6   = 0666, // (438 decimal)
	opcode1_nar7   = 0667, // (439 decimal)
	opcode1_sar0   = 0740, // (480 decimal)
	opcode1_sar1   = 0741, // (481 decimal)
	opcode1_sar2   = 0742, // (482 decimal)
	opcode1_sar3   = 0743, // (483 decimal)
	opcode1_sar4   = 0744, // (484 decimal)
	opcode1_sar5   = 0745, // (485 decimal)
	opcode1_sar6   = 0746, // (486 decimal)
	opcode1_sar7   = 0747, // (487 decimal)
	opcode1_sra    = 0754, // (492 decimal)
	opcode1_lar0   = 0760, // (496 decimal)
	opcode1_lar1   = 0761, // (497 decimal)
	opcode1_lar2   = 0762, // (498 decimal)
	opcode1_lar3   = 0763, // (499 decimal)
	opcode1_lar4   = 0764, // (500 decimal)
	opcode1_lar5   = 0765, // (501 decimal)
	opcode1_lar6   = 0766, // (502 decimal)
	opcode1_lar7   = 0767, // (503 decimal)
	opcode1_lra    = 0774  // (508 decimal)
} opcode1_t;

// Function prototypes

/* dps8_addrmods.c */

word18 getCr(word4 Tdes);
void doComputedAddressFormation(DCDstruct *, eCAFoper action);

/* dps8_append.c */

void doAddrModPtrReg(DCDstruct *);
void doPtrReg (DCDstruct *);        ///< used by EIS stuff
char *strSDW0(_sdw0 *SDW);
char *strSDW(_sdw *SDW);
char *strDSBR(void);
t_stat dumpSDWAM (void);
bool doITSITP(DCDstruct *i, word36 indword, word6 Tag);
word36 doAppendCycle(DCDstruct *i, MemoryAccessType accessType, word6 Tdes, word36 writeData, word36 *readData);

/* dps8_bar.c */

word18 getBARaddress(word18 addr);

/* dps8_clk.c */

extern UNIT TR_clk_unit [];
extern DEVICE clk_dev;

/* dps8_console.c */

int opcon_autoinput_set(UNIT *uptr, int32 val, char *cptr, void *desc);
int opcon_autoinput_show(FILE *st, UNIT *uptr, int val, void *desc);
int con_iom_cmd(int chan, int dev_cmd, int dev_code, int* majorp, int* subp);
int con_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp);
void console_init(void);
t_stat cable_opcon (int iom_unit_num, int chan_num);
extern DEVICE opcon_dev;
extern UNIT opcon_unit [];

/* dps8_cpu.c */

void init_opcodes (void);
void encode_instr(const instr_t *ip, t_uint64 *wordp);
DCDstruct *decodeInstruction(word36 inst, DCDstruct *dst);     // decode instruction into structure
DCDstruct *fetchInstruction(word18 addr, DCDstruct *dst);      // fetch (+ decode) instrcution at address
t_stat dpsCmd_Dump (int32 arg, char *buf);
t_stat dpsCmd_Init (int32 arg, char *buf);
t_stat dpsCmd_Segment (int32 arg, char *buf);
t_stat dpsCmd_Segments (int32 arg, char *buf);

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

// RAW, core stuff ...
int core_read(word24 addr, word36 *data);
int core_write(word24 addr, word36 data);
int core_read2(word24 addr, word36 *even, d8 *odd);
int core_write2(word24 addr, word36 even, d8 odd);
int core_readN(word24 addr, word36 *data, int n);
int core_writeN(word24 addr, word36 *data, int n);
int core_read72(word24 addr, word72 *dst);

int is_priv_mode(void);
addr_modes_t get_addr_mode(void);
void set_addr_mode(addr_modes_t mode);

void ic_history_init(void);
t_stat cable_to_cpu (int scu_unit_num, int scu_port_num, int iom_unit_num, int iom_port_num);

/* dps8_append.c */

void do_ldbr (word36 * Ypair);
void do_sdbr (word36 * Ypair);
void do_camp (word36 Y);
void do_cams (word36 Y);

/* dps8_decimal.c */

void parseNumericOperandDescriptor(int k, EISstruct *e);

void EISwrite49(EISaddr *p, int *pos, int tn, int c49);
void EISloadInputBufferNumeric(DCDstruct *i, int k);

/* dps8_faults.c */

void check_events (void);

/* dps8_ins.c */

void cu_safe_store(void);
t_stat executeInstruction(DCDstruct *ci);
t_stat doXED(word36 *Ypair);
void cu_safe_restore (void);
void initializeTheMatrix (void);
void addToTheMatrix (int32 opcode, bool opcodeX, bool a, word6 tag);
t_stat displayTheMatrix (int32 arg, char * buf);

/* dps8_iom.c */

DEVICE * get_iom_channel_dev (uint iom_unit_num, int chan, int dev_code, int * unit_num);
extern UNIT iom_unit [];

void iom_init(void);
t_stat iom_boot(int32 unit_num, DEVICE *dptr);
void iom_interrupt(int iom_unit_num);
t_stat iom_svc(UNIT* up);
t_stat iom_reset(DEVICE *dptr);
t_stat iom_boot(int32 unit_num, DEVICE *dptr);
t_stat channel_svc(UNIT *up);
int get_iom_numunits (void);
t_stat cable_to_iom (int iom_unit_num, int chan_num, int dev_code, enum dev_type dev_type, int dev_unit_num);
t_stat cable_iom (int iom_unit_num, int iom_port_num, int scu_unit_num, int scu_port_num);

/* dps8_mpc.c */

#if 0
void mpc_init (void);
extern DEVICE mpc_dev;
t_stat cable_to_mpc (int mpc_unit_num, int dev_code, enum dev_type  mpc_dev_type, int mt_unit_num);
t_stat cable_mpc (int mpc_unit_num, int iom_unit_num, int chan_num);
#endif

/* dps8_scu.c */

extern DEVICE scu_dev;
extern UNIT scu_unit [];
int scu_set_interrupt(uint scu_unit_num, uint inum);
t_stat cable_to_scu (int scu_unit_num, int scu_port_num, int iom_unit_num, int iom_port_num);
t_stat cable_scu (int scu_unit_num, int scu_port_num, int cpu_unit_num, int cpu_port_num);
void scu_init (void);
t_stat scu_sscr (uint cpu_unit_num, word36 addr, word18 rega, word18 regq);
int scu_cioc (uint scu_unit_num, uint scu_port_num);

/* dps8_sys.c */

extern word36 *M;

/* dps8_utils.c */

int bitfieldInsert(int a, int b, int c, int d);
int bitfieldExtract(int a, int b, int c);
char *bin2text(t_uint64 word, int n);
void sim_printf( const char * format, ... )    // not really simh, by my impl
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;



#endif
