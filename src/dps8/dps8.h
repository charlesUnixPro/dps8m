/**
 * \file dps8.h
 * \project dps8
 * \author Harry Reed
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#ifndef DPS8_H
#define DPS8_H


#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#include <sys/time.h>

#include <setjmp.h>     // for setjmp/longjmp used by interrupts & faults

#ifndef EMULATOR_ONLY
#define EMULATOR_ONLY 1
#endif

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
typedef uint8       word5;
typedef uint8       word6;
typedef uint8       word7;
typedef uint8       word8;
typedef int8        word8s; // signed 8-bit quantity
typedef uint16      word9;
typedef uint16      word10;
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

typedef unsigned int uint;  // efficient unsigned int, at least 32 bits

#include "dps8_simh.h"
#include "dps8_hw_consts.h"



#define SETF(flags, x)         flags = ((flags) |  (x))
#define CLRF(flags, x)         flags = ((flags) & ~(x))
#define TSTF(flags, x)         ((flags) & (x))
#define SCF(cond, flags, x)    { if ((cond)) SETF((flags), x); else CLRF((flags), x); }

#define SETBIT(dst, bitno)      ((dst) | (1LLU << (bitno)))
#define CLRBIT(dst, bitno)      ((dst) & ~(1LLU << (bitno)))
#define TSTBIT(dst, bitno)      ((dst) &  (1LLU << (bitno)))

/////


extern enum _processor_cycle_type {
    UNKNOWN_CYCLE = 0,
    APPEND_CYCLE,
    CA_CYCLE,
    OPERAND_STORE,
    OPERAND_READ,
    DIVIDE_EXECUTION,
    FAULT,
    INDIRECT_WORD_FETCH,
    RTCD_OPERAND_FETCH,
    //SEQUENTIAL_INSTRUCTION_FETCH,
    INSTRUCTION_FETCH,
    APU_DATA_MOVEMENT,
    ABORT_CYCLE,
    FAULT_CYCLE,
    EIS_OPERAND_DESCRIPTOR,  // change later for real MW EIS operand descriptors
    EIS_OPERAND_STORE,
    EIS_OPERAND_READ
    
} processorCycle;
typedef enum _processor_cycle_type _processor_cycle_type;

enum _processor_addressing_mode {
    UNKNOWN_MODE = 0,
    ABSOLUTE_MODE,
    APPEND_MODE,
    BAR_MODE
};


//! some breakpoint stuff ...
enum eMemoryAccessType {
    Unknown          = 0,
    InstructionFetch,
    IndirectRead,
    //IndirectWrite,
    DataRead,
    DataWrite,
    OperandRead,
    OperandWrite,
    
//    APUDataRead,        // append operations from absolute mode
//    APUDataWrite,
//    APUOperandRead,
//    APUOperandWrite,

    Call6Operand,
    RTCDOperand = RTCD_OPERAND_FETCH,
    
    
    // for EIS read operations
    viaPR,      // EIS data access vis PR
    PrepareCA,
};

typedef enum eMemoryAccessType MemoryAccessType;

#define MA_IF  0   /* fetch */
#define MA_ID  1   /* indirect */
#define MA_RD  2   /* data read */
#define MA_WR  3   /* data write */

#define GETCHAR(src, pos) (word36)(((word36)src >> (word36)((5 - pos) * 6)) & 077)      ///< get 6-bit char @ pos
#define GETBYTE(src, pos) (word36)(((word36)src >> (word36)((3 - pos) * 9)) & 0777)     ///< get 9-bit byte @ pos

#define YPAIRTO72(ypair)    (((((word72)(ypair[0] & DMASK)) << 36) | (ypair[1] & DMASK)) & MASK72)



#define GET_TALLY(src) (((src) >> 6) & MASK12)   // 12-bits
#define GET_DELTA(src)  ((src) & MASK6)           // 6-bits

#define max(a,b)    max2((a),(b))
#define max2(a,b)   ((a) > (b) ? (a) : (b))
#define max3(a,b,c) max((a), max((b),(c)))

#define min(a,b)    min2((a),(b))
#define min2(a,b)   ((a) < (b) ? (a) : (b))
#define min3(a,b,c) min((a), min((b),(c)))

// opcode metadata (flag) ...
typedef enum opc_flag
  {
    READ_OPERAND    = (1U <<  0),  // fetches/reads operand (CA) from memory
    STORE_OPERAND   = (1U <<  1),  // stores/writes operand to memory (its a STR-OP)
#define RMW             (READ_OPERAND | STORE_OPERAND) ///< a Read-Modify-Write instruction
    READ_YPAIR      = (1U <<  2),  // fetches/reads Y-pair operand (CA) from memory
    STORE_YPAIR     = (1U <<  3),  // stores/writes Y-pair operand to memory
    READ_YBLOCK8    = (1U <<  4),  // fetches/reads Y-block8 operand (CA) from memory
    NO_RPT          = (1U <<  5),  // Repeat instructions not allowed
//#define NO_RPD          (1U << 6)
    NO_RPL          = (1U <<  7),
//#define NO_RPX          (NO_RPT | NO_RPD | NO_RPL)
    READ_YBLOCK16   = (1U <<  8),  // fetches/reads Y-block16 operands from memory
    STORE_YBLOCK16  = (1U <<  9),  // fetches/reads Y-block16 operands from memory
    TRANSFER_INS    = (1U << 10), // a transfer instruction
    TSPN_INS        = (1U << 11), // a TSPn instruction
    CALL6_INS       = (1U << 12), // a call6 instruction
    PREPARE_CA      = (1U << 13), // prepare TPR.CA for instruction
    STORE_YBLOCK8   = (1U << 14), // stores/writes Y-block8 operand to memory
    IGN_B29         = (1U << 15), // Bit-29 has an instruction specific meaning. Ignore.
    NO_TAG          = (1U << 16), // tag is interpreted differently and for addressing purposes is effectively 0
    PRIV_INS        = (1U << 17), // priveleged instruction
    NO_BAR          = (1U << 18), // not allowed in BAR mode
    NO_XEC          = (1U << 19), // can't be executed via xec/xed
    NO_XED          = (1U << 20), // No execution via XED instruction

// EIS operand types

#define EOP_ALPHA 1U

// bits 21, 22
    EOP1_ALPHA      = (EOP_ALPHA << 21),
    EOP1_MASK       = (3U << 21),
#define EOP1_SHIFT 21

// bits 23, 24
    EOP2_ALPHA      = (EOP_ALPHA << 23),
    EOP2_MASK       = (3U << 23),
#define EOP2_SHIFT 23

// bits 25, 26
    EOP3_ALPHA      = (EOP_ALPHA << 25),
    EOP3_MASK       = (3U << 25)
#define EOP3_SHIFT 25

  } opc_flag;


// opcode metadata (disallowed) modifications
typedef enum opc_mod
  {
    NO_DU                 = (1U << 0),   ///< No DU modification allowed (Can these 2 be combined into 1?)
    NO_DL                 = (1U << 1),   ///< No DL modification allowed
#define NO_DUDL         (NO_DU | NO_DL)    

    NO_CI                 = (1U << 2),   ///< No character indirect modification (can these next 3 be combined?_
    NO_SC                 = (1U << 3),   ///< No sequence character modification
    NO_SCR                = (1U << 4),   ///< No sequence character reverse modification
#define NO_CSS          (NO_CI | NO_SC | NO_SCR)

#define NO_DLCSS        (NO_DU   | NO_CSS)
#define NO_DDCSS        (NO_DUDL | NO_CSS)

    ONLY_AU_QU_AL_QL_XN   = (1U << 5)    ///< None except au, qu, al, ql, xn
  } opc_mod;

// None except au, qu, al, ql, xn for MF1 and REG
// None except du, au, qu, al, ql, xn for MF2
// None except au, qu, al, ql, xn for MF1, MF2, and MF3


#define IS_NONE(tag) (!(tag))
/*! non-tally: du or dl */
#define IS_DD(tag) ((_TM(tag) != 040U) && \
    ((_TD(tag) == 003U) || (_TD(tag) == 007U)))
/*! tally: ci, sc, or scr */
#define IS_CSS(tag) ((_TM(tag) == 040U) && \
    ((_TD(tag) == 050U) || (_TD(tag) == 052U) || \
    (_TD(tag) == 045U)))
#define IS_DDCSS(tag) (IS_DD(tag) || IS_CSS(tag))
/*! just dl or css */
#define IS_DCSS(tag) (((_TM(tag) != 040U) && (_TD(tag) == 007U)) || IS_CSS(tag))

//! Basic + EIS opcodes .....
struct opCode {
    const char *mne;    ///< mnemonic
    opc_flag flags;        ///< various and sundry flags
    opc_mod mods;         ///< disallowed addr mods
    int32 ndes;         ///< number of operand descriptor words for instruction (mw EIS)
};
typedef struct opCode opCode;

// instruction decode information
struct EISstruct;   // forward reference
typedef struct EISstruct EISstruct;




// operations stuff

/*! Y of instruc word */
#define Y(i) (i & MASKHI18)
/*! X from opcodes in instruc word */
#define OPSX(i) ((i & 0007000LLU) >> 9)
/*! X from OP_* enum, and X from  */
#define X(i) (i & 07U)

enum { OP_1	= 00001U,
    OP_E	= 00002U,
    OP_BAR	= 00003U,
    OP_IC	= 00004U,
    OP_A	= 00005U,
    OP_Q	= 00006U,
    OP_AQ	= 00007U,
    OP_IR	= 00010U,
    OP_TR	= 00011U,
    OP_REGS	= 00012U,
    
    /* 645/6180 */
    OP_CPR	= 00021U,
    OP_DBR	= 00022U,
    OP_PTP	= 00023U,
    OP_PTR	= 00024U,
    OP_RA	= 00025U,
    OP_SDP	= 00026U,
    OP_SDR	= 00027U,
    
    OP_X	= 01000U
};


enum eCAFoper {
    unknown = 0,
    readCY,
    writeCY,
    rmwCY,      // Read-Modify-Write 
//    readCYpair,
//    writeCYpair,
//    readCYblock8,
//    writeCYblock8,
//    readCYblock16,
//    writeCYblock16,

    prepareCA,
};
typedef enum eCAFoper eCAFoper;

#define READOP(i)  ((bool) (i->info->flags & ( READ_OPERAND |  READ_YPAIR |  READ_YBLOCK8 |  READ_YBLOCK16)) )
#define WRITEOP(i) ((bool) (i->info->flags & (STORE_OPERAND | STORE_YPAIR | STORE_YBLOCK8 | STORE_YBLOCK16)) )
#define RMWOP(i)   ((bool) READOP(i) && WRITEOP(i)) // if it's both read and write it's a RMW

#define TRANSOP(i) ((bool) (i->info->flags & (TRANSFER_INS) ))

//
// EIS stuff ...
//

// Numeric operand descriptors


// AL39 Table 4-3. Alphanumeric Data Type (TA) Codes
typedef enum _CTA
{
    CTA9 = 0U,       ///< 9-bit bytes
    CTA6 = 1U,       ///< 6-bit characters
    CTA4 = 2U        ///< 4-bit decimal
} _CTA;

//#define CTA9   0
//#define CTA6   1
//#define CTA4   2


// TN - Type Numeric AL39 Table 4-3. Alphanumeric Data Type (TN) Codes
typedef enum _CTN
{
    CTN9 = 0U,   ///< 9-bit
    CTN4 = 1U    ///< 4-bit
} _CTN;
//#define CTN9    0   ///< 9-bit
//#define CTN4    1   ///< 4-bit

// S - Sign and Decimal Type (AL39 Table 4-4. Sign and Decimal Type (S) Codes)

#define CSFL    0U   ///< Floating-point, leading sign
#define CSLS    1U   ///< Scaled fixed-point, leading sign
#define CSTS    2U   ///< Scaled fixed-point, trailing sign
#define CSNS    3U   ///< Scaled fixed-point, unsigned


#define MFkAR   0x40U ///< Address register flag. This flag controls interpretation of the ADDRESS field of the operand descriptor just as the "A" flag controls interpretation of the ADDRESS field of the basic and EIS single-word instructions.
#define MFkPR   0x40U ///< ""
#define MFkRL   0x20U ///< Register length control. If RL = 0, then the length (N) field of the operand descriptor contains the length of the operand. If RL = 1, then the length (N) field of the operand descriptor contains a selector value specifying a register holding the operand length. Operand length is interpreted as units of the data size (1-, 4-, 6-, or 9-bit) given in the associated operand descriptor.
#define MFkID   0x10U ///< Indirect descriptor control. If ID = 1 for Mfk, then the kth word following the instruction word is an indirect pointer to the operand descriptor for the kth operand; otherwise, that word is the operand descriptor.

#define MFkREGMASK  0xfU


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

typedef struct DCDstruct DCDstruct;

// Misc constants and macros

#define ARRAY_SIZE(a) ( sizeof(a) / sizeof((a)[0]) )


#endif // ifdef DPS8_H
