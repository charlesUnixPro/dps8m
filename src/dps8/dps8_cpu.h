/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony
 Copyright 2015 by Eric Swenson

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// simh only explicitly supports a single cpu

#define N_CPU_UNITS 1 // Default

// JMP_ENTRY must be 0, which is the return value of the setjmp initial
// entry
#define JMP_ENTRY       0
#define JMP_REENTRY     1
#define JMP_RETRY       2   // retry instruction
#define JMP_NEXT        3   // goto next sequential instruction
#define JMP_TRA         4   // treat return as if it were a TRA instruction 
                            // with PPR . IC already set to where to jump to
#define JMP_STOP        5   // treat return as if it were an attempt to 
                            // unravel the stack and gracefully exit out of 
                            // sim_instr
#define JMP_INTR        6   // Interrupt detected during processing
#define JMP_SYNC_FAULT_RETURN 7
#define JMP_REFETCH 8
#define JMP_RESTART 9


// The CPU supports 3 addressing modes
// [CAC] I tell a lie: 4 modes...
// [CAC] I tell another lie: 5 modes...

typedef enum
  {
    ABSOLUTE_mode,
    APPEND_mode,
#if 0
    BAR_mode,
    APPEND_BAR_mode,
#endif
  } addr_modes_t;


// The control unit of the CPU is always in one of several states. We
// don't currently use all of the states used in the physical CPU.
// The FAULT_EXEC cycle did not exist in the physical hardware.

typedef enum
  {
    FAULT_cycle,
    EXEC_cycle,
    FAULT_EXEC_cycle,
    FAULT_EXEC2_cycle,
    INTERRUPT_cycle,
    INTERRUPT_EXEC_cycle,
    INTERRUPT_EXEC2_cycle,
    FETCH_cycle,
    SYNC_FAULT_RTN_cycle,
    // CA FETCH OPSTORE, DIVIDE_EXEC
  } cycles_t;

struct _tpr
  {
    word3   TRR; // The current effective ring number
    word15  TSR; // The current effective segment number
    word6   TBR; // The current bit offset as calculated from ITS and ITP 
                 // pointer pairs.
    word18  CA;  // The current computed address relative to the origin of the 
                 // segment whose segment number is in TPR . TSR
  };

struct _ppr
  {
    word3   PRR; // The number of the ring in which the process is executing. 
                 // It is set to the effective ring number of the procedure 
                 // segment when control is transferred to the procedure.
    word15  PSR; // The segment number of the procedure being executed.
    word1   P;   // A flag controlling execution of privileged instructions. 
                 // Its value is 1 (permitting execution of privileged 
                 // instructions) if PPR . PRR is 0 and the privileged bit in 
                 // the segment descriptor word (SDW . P) for the procedure is 
                 // 1; otherwise, its value is 0.
    word18  IC;  // The word offset from the origin of the procedure segment
                 //  to the current instruction. (same as PPR . IC)
  };

/////

// The terms "pointer register" and "address register" both apply to the same
// physical hardware. The distinction arises from the manner in which the
// register is used and in the interpretation of the register contents.
// "Pointer register" refers to the register as used by the appending unit and
// "address register" refers to the register as used by the decimal unit.
//
// The three forms are compatible and may be freely intermixed. For example,
// PRn may be loaded in pointer register form with the Effective Pointer to
// Pointer Register n (eppn) instruction, then modified in pointer register
// form with the Effective Address to Word/Bit Number of Pointer Register n
// (eawpn) instruction, then further modified in address register form
// (assuming character size k) with the Add k-Bit Displacement to Address
// Register (akbd) instruction, and finally invoked in operand descriptor form
// by the use of MF . AR in an EIS multiword instruction .
//
// The reader's attention is directed to the presence of two bit number
// registers, PRn . BITNO and ARn . BITNO. Because the Multics processor was
// implemented as an enhancement to an existing design, certain apparent
// anomalies appear. One of these is the difference in the handling of
// unaligned data items by the appending unit and decimal unit. The decimal
// unit handles all unaligned data items with a 9-bit byte number and bit
// offset within the byte. Conversion from the description given in the EIS
// operand descriptor is done automatically by the hardware. The appending unit
// maintains compatibility with the earlier generation Multics processor by
// handling all unaligned data items with a bit offset from the prior word
// boundary; again with any necessary conversion done automatically by the
// hardware. Thus, a pointer register, PRn, may be loaded from an ITS pointer
// pair having a pure bit offset and modified by one of the EIS address
// register instructions (a4bd, s9bd, etc.) using character displacement
// counts. The automatic conversion performed ensures that the pointer
// register, PRi, and its matching address register, ARi, both describe the
// same physical bit in main memory.
// 
// N.B. Subtle differences between the interpretation of PR/AR. Need to take
// this into account.
// 
//     * For Pointer Registers:
//       - PRn . WORDNO The offset in words from the base or origin of the 
//                    segment to the data item.
//       - PRn . BITNO The number of the bit within PRn . WORDNO that is the
//                   first bit of the data item. Data items aligned on word 
//                   boundaries always have the value 0. Unaligned data items 
//                   may have any value in the range [1,35].
// 
//     * For Address Registers:
//       - ARn . WORDNO The offset in words relative to the current addressing 
//                    base referent (segment origin, BAR . BASE, or absolute 0 
//                    depending on addressing mode) to the word containing the 
//                    next data item element.
//       - ARn . CHAR   The number of the 9-bit byte within ARn . WORDNO 
//                    containing the first bit of the next data item element.
//       - ARn . BITNO  The number of the bit within ARn . CHAR that is the
//                    first bit of the next data item element.
//

struct _par
  {
    word15  SNR;    // The segment number of the segment containing the data 
                    //item described by the pointer register.
    word3   RNR;    // The final effective ring number value calculated during
                    // execution of the instruction that last loaded the PR.

    word6  PR_BITNO;  // The number of the bit within PRn . WORDNO that is the 
                    // first bit of the data item. Data items aligned on word 
                    // boundaries always have the value 0. Unaligned data
                    //  items may have any value in the range [1,35].
    word2   AR_CHAR;
    word4   AR_BITNO;

    word18  WORDNO; // The offset in words from the base or origin of the 
                    // segment to the data item.
  };

// N.B. remember there are subtle differences between AR/PR . BITNO

#define AR    PAR
#define PR    PAR

struct _bar
  {
    word9 BASE;     // Contains the 9 high-order bits of an 18-bit address 
                    // relocation constant. The low-order bits are generated 
                    // as zeros.
    word9 BOUND;    // Contains the 9 high-order bits of the unrelocated 
                    // address limit. The low- order bits are generated as 
                    // zeros. An attempt to access main memory beyond this 
                    // limit causes a store fault, out of bounds. A value of 
                    // 0 is truly 0, indicating a null memory range.
  };

struct _dsbr
  {
    word24  ADDR;   // If DSBR . U = 1, the 24-bit absolute main memory address
                    //  of the origin of the current descriptor segment;
                    //  otherwise, the 24-bit absolute main memory address of
                    //  the page table for the current descriptor segment.
    word14  BND;    // The 14 most significant bits of the highest Y-block16
                    //  address of the descriptor segment that can be
                    //  addressed without causing an access violation, out of
                    //  segment bounds, fault.
    word1   U;      // A flag specifying whether the descriptor segment is 
                    // unpaged (U = 1) or paged (U = 0).
    word12  STACK;  // The upper 12 bits of the 15-bit stack base segment
                    // number. It is used only during the execution of the 
                    // call6 instruction. (See Section 8 for a discussion
                    //  of generation of the stack segment number.)
  };

// The segment descriptor word (SDW) pair contains information that controls
// the access to a segment. The SDW for segment n is located at offset 2n in
// the descriptor segment whose description is currently loaded into the
// descriptor segment base register (DSBR).

struct _sdw
  {
    word24  ADDR;    // The 24-bit absolute main memory address of the page
                     //  table for the target segment if SDWAM . U = 0;
                     //  otherwise, the 24-bit absolute main memory address
                     //  of the origin of the target segment.
    word3   R1;      // Upper limit of read/write ring bracket
    word3   R2;      // Upper limit of read/execute ring bracket
    word3   R3;      // Upper limit of call ring bracket
    word14  BOUND;   // The 14 high-order bits of the last Y-block16 address
                     //  within the segment that can be referenced without an
                     //  access violation, out of segment bound, fault.
    word1   R;       // Read permission bit. If this bit is set ON, read
                     //  access requests are allowed.
    word1   E;       // Execute permission bit. If this bit is set ON, the SDW
                     //  may be loaded into the procedure pointer register
                     //  (PPR) and instructions fetched from the segment for
                     //  execution.
    word1   W;       // Write permission bit. If this bit is set ON, write
                     //  access requests are allowed.
    word1   P;       // Privileged flag bit. If this bit is set ON, privileged
                     //  instructions from the segment may be executed if
                     //  PPR . PRR is 0.
    word1   U;       // Unpaged flag bit. If this bit is set ON, the segment
                     //  is unpaged and SDWAM . ADDR is the 24-bit absolute
                     //  main memory address of the origin of the segment. If
                     //  this bit is set OFF, the segment is paged andis
                     //  SDWAM . ADDR the 24-bit absolute main memory address of the page
                     //  table for the segment.
    word1   G;       // Gate control bit. If this bit is set OFF, calls and
                     //  transfers into the segment must be to an offset no
                     //  greater than the value of SDWAM . CL as described
                     //  below.
    word1   C;       // Cache control bit. If this bit is set ON, data and/or
                     //  instructions from the segment may be placed in the
                     //  cache memory.
    word14  EB;      // Call limiter (entry bound) value. If SDWAM . G is set
                     //  OFF, transfers of control into the segment must be to
                     //  segment addresses no greater than this value.
    word15  POINTER; // The effective segment number used to fetch this SDW
                     //  from main memory.
    word1   DF;      // Directed fault flag (called F in AL39).
                     //  * 0 = page not in main memory; execute directed fault
                     //        FC
                     //  * 1 = page is in main memory
    word2   FC;      // Directed fault number for page fault.
    word1   FE;      // Full/empty bit. If this bit is set ON, the SDW in the
                     //  register is valid. If this bit is set OFF, a hit is
                     //  not possible. All SDWAM . F bits are set OFF by the
                     //  instructions that clear the SDWAM.
#ifdef DPS8M
    word6   USE;
#endif
#ifdef L68
    word4   USE;
#endif
                     // Usage count for the register. The SDWAM . USE field is
                     //  used to maintain a strict FIFO queue order among the
                     //  SDWs. When an SDW is matched, its USE value is set to
                     //  15 (newest) on the DPS/L68 and to 63 on the DPS 8M,
                     //  and the queue is reordered. SDWs newly fetched from
                     //  main memory replace the SDW with USE value 0 (oldest)
                     //  and the queue is reordered.
  };

typedef struct _sdw _sdw;
typedef struct _sdw _sdw0;

#if 0
// in-core SDW (i.e. not cached, or in SDWAM)

struct _sdw0
  {
    // even word
    word24  ADDR;    // The 24-bit absolute main memory address of the page
                     //  table for the target segment if SDWAM . U = 0;
                     //  otherwise, the 24-bit absolute main memory address of
                     //  the origin of the target segment.
    word3   R1;      // Upper limit of read/write ring bracket
    word3   R2;      // Upper limit of read/execute ring bracket
    word3   R3;      // Upper limit of call ring bracket
    word1   DF;      // Directed fault flag (called F in AL39).
                     //  * 0 = page not in main memory; execute directed fault
                     //        FC
                     //  * 1 = page is in main memory
    word2   FC;      // Directed fault number for page fault.
    
    // odd word
    word14  BOUND;   // The 14 high-order bits of the last Y-block16 address
                     //  within the segment that can be referenced without an
                     //  access violation, out of segment bound, fault.
    word1   R;       // Read permission bit. If this bit is set ON, read
                     //  access requests are allowed.
    word1   E;       // Execute permission bit. If this bit is set ON, the SDW
                     //  may be loaded into the procedure pointer register
                     //  (PPR) and instructions fetched from the segment for
                     //  execution.
    word1   W;       // Write permission bit. If this bit is set ON, write
                     //  access requests are allowed.
    word1   P;       // Privileged flag bit. If this bit is set ON,
                     //  privileged instructions from the segment may be
                     //  executed if PPR . PRR is 0.
    word1   U;       // Unpaged flag bit. If this bit is set ON, the segment
                     //  is unpaged and SDWAM . ADDR is the 24-bit absolute
                     //  main memory address of the origin of the segment.
                     //  If this bit is set OFF, the segment is paged and
                     //  SDWAM . ADDR is the 24-bit absolute main memory
                     //  address of the page table for the segment.
    word1   G;       // Gate control bit. If this bit is set OFF, calls and
                     //  transfers into the segment must be to an offset no
                     //  greater than the value of SDWAM . CL as described
                     //  below.
    word1   C;       // Cache control bit. If this bit is set ON, data and/or
                     //  instructions from the segment may be placed in the
                     //  cache memory.
    word14  EB;      // Entry bound. Any call into this segment must be to
                     //  an offset less than EB if G=0
};

typedef struct _sdw0 _sdw0;
#endif


// PTW as used by APU

struct _ptw
 {
    word18  ADDR;    // The 18 high-order bits of the 24-bit absolute
                     //  main memory address of the page.
    word1   U;      // * 1 = page has been used (referenced)
    word1   M;      // Page modified flag bit. This bit is set ON whenever
                    //  the PTW is used for a store type instruction. When
                    //  the bit changes value from 0 to 1, a special
                    //  extra cycle is generated to write it back into the
                    //  PTW in the page table in main memory.
    word1   DF;     // Directed fault flag
                    // * 0 = page not in main memory; execute directed fault FC
                    // * 1 = page is in main memory
    word2   FC;     // Directed fault number for page fault.
    word15  POINTER; // The effective segment number used to fetch this PTW
                     //  from main memory.
    word12  PAGENO;  // The 12 high-order bits of the 18-bit computed
                     //  address (TPR . CA) used to fetch this PTW from main
                     //  memory.
    word1   FE;      // Full/empty bit. If this bit is set ON, the PTW in
                     //  the register is valid. If this bit is set OFF, a
                     //  hit is not possible. All PTWAM . F bits are set OFF
                     //  by the instructions that clear the PTWAM.
#ifdef DPS8M
    word6   USE;
#endif

#ifdef L68
    word4   USE;
#endif
                     // Usage count for the register. The PTWAM . USE field
                     //  is used to maintain a strict FIFO queue order
                     //  among the PTWs. When an PTW is matched its USE 
                     // value is set to 15 (newest) on the DPS/L68 and to
                     //  63 on the DPS 8M, and the queue is reordered.
                     //  PTWs newly fetched from main memory replace the
                     //  PTW with USE value 0 (oldest) and the queue is
                     //  reordered.
    
  };

typedef struct _ptw _ptw;
typedef struct _ptw _ptw0;

#if 0
// in-core PTW

struct _ptw0
  {
    word18  ADDR;   // The 18 high-order bits of the 24-bit absolute main
                    //  memory address of the page.
    word1   U;      // * 1 = page has been used (referenced)
    word1   M;      // * 1 = page has been modified
    word1   DF;     // Directed fault flag
                    // * 0 = page not in main memory; execute directed fault FC
                    // * 1 = page is in main memory
    word2   FC;     // Directed fault number for page fault.
    
  };

typedef struct _ptw0 _ptw0;
#endif

//
// Cache Mode Regsiter
//

struct _cache_mode_register
  {
    word15   cache_dir_address;
    word1    par_bit;
    word1    lev_ful;
    word1    csh1_on; // 1: The lower half of the cache memory is active and enabled as per the state of inst_on
    word1    csh2_on; // 1: The upper half of the cache memory is active and enabled as per the state of inst_on
#ifdef L68
    word1    opnd_on; // 1: The cache memory (if active) is used for operands.
#endif
    word1    inst_on; // 1: The cache memory (if active) is used for instructions.
    // When the cache-to-register mode flag (bit 59 of the cache mode register) is set ON, the
    // processor is forced to fetch the operands of all double-precision operations unit load operations
    // from the cache memory. Y0,12 are ignored, Y15,21 select a column, and Y13,14 select a level. All
    // other operations (e.g., instruction fetches, single-precision operands, etc.) are treated normally.
    word1    csh_reg;
    word1    str_asd;
    word1    col_ful;
    word2    rro_AB;
#ifdef DPS8M
    word1    bypass_cache;
#endif
    word2    luf;       // LUF value
                        // 0   1   2   3
                        // Lockup time
                        // 2ms 4ms 8ms 16ms
                        // The lockup timer is set to 16ms when the 
                        // processor is initialized.
  };

typedef struct _cache_mode_register _cache_mode_register;

#if 1
typedef struct mode_register
  {
    word36 r;
#ifdef L68
    word15 FFV;
    word1 OC_TRAP;
    word1 ADR_TRAP;
    word9 OPCODE;
    word1 OPCODEX;
#endif
    word1 sdpap;
    word1 separ;
    word1 emr;
    word1 hrhlt;
#ifdef DPS8M
    word1 hrxfr;
#endif
    word1 ihr;
    word1 ihrrs;
#ifdef DPS8M
    word1 hexfp;
#endif
  } _mode_register;
#else
typedef struct mode_register
  {
#ifdef L68
    word15 FFV;
#ifdef L68
    word1 isolts_tracks;
#endif
    word1 OC_TRAP;
    word1 ADR_TRAP;
    word10 OPCODE;
#endif
    word1 cuolin;
    word1 solin;
    word1 sdpap;
    word1 separ;
    word2 tm;
    word2 vm;
#ifdef L68
    word2 isolts_tracks2;
#endif
    word1 hrhlt;
#ifdef DPS8M
    word1 hrxfr;
#endif
#ifdef L68
    word1 hropc;
#endif
    word1 ihr;
    word1 ihrrs;
// XXX This bit is used to track the position of the NORMAL/TEST switch
//    word1 mrgctl;
#ifdef DPS8M
    word1 hexfp;
#endif
    word1 emr;
  } _mode_register;
#endif

extern DEVICE cpu_dev;

typedef struct MOPstruct MOPstruct;

// address of an EIS operand
typedef struct EISaddr
{
#ifndef EIS_PTR
    word18  address;    // 18-bit virtual address
#endif
    //word18  lastAddress;  // memory acccesses are not expesive these days - >sheesh<
    
    word36  data;
    word1    bit;
    bool    incr;      // when true increment bit address
    bool    decr;      // when true decrement bit address
    eRW     mode;
    
    // for type of data being address by this object
    
    // eisDataType _type;   // type of data - alphunumeric/numeric
    
#ifndef EIS_PTR3
    int     TA;   // type of Alphanumeric chars in src
#endif
    int     TN;   // type of Numeric chars in src
    int     cPos;
    int     bPos;
    
#ifndef EIS_PTR4
    // for when using AR/PR register addressing
    word15  SNR;        // The segment number of the segment containing the data item described by the pointer register.
    word3   RNR;        // The effective ring number value calculated during execution of the instruction that last loaded
    
    //bool    bUsesAR;    // true when indirection via AR/PR is involved (TPR.{TRR,TSR} already set up)
    
    MemoryAccessType    mat;    // memory access type for operation
#endif

    // Cache

    // There is a cache for each operand, but they do not cross check;
    // this means that if one of them has a cached dirty word, the
    // others will not check for a hit, and will use the old value.
    // AL39 warns that overlapping operands can cause unexpected behavior
    // due to caching issues, so the this behavior is closer to the actual
    // h/w then to the theoretical need for cache consistancy.

    // We don't need to cache mat or TPR because they will be constant
    // across an instruction.

    bool cacheValid;
    bool cacheDirty;
    //word36 cachedWord;
#define paragraphSz 8
#define paragraphMask 077777770
#define paragraphOffsetMask 07
    word36 cachedParagraph [paragraphSz];
    word18 cachedAddr;

} EISaddr;
typedef struct EISstruct
  {
    word36  op [3];         // raw operand descriptors
#define OP1 op [0]          // 1st descriptor (2nd ins word)
#define OP2 op [1]          // 2nd descriptor (3rd ins word)
#define OP3 op [2]          // 3rd descriptor (4th ins word)
    
    bool    P;              // 4-bit data sign character control
    
    uint    MF [3];
#define MF1    MF [0]      // Modification field for operand descriptor 1
#define MF2    MF [1]      // Modification field for operand descriptor 2
#define MF3    MF [2]      // Modification field for operand descriptor 3


    uint   CN [3];
#define CN1 CN [0]
#define CN2 CN [1]
#define CN3 CN [2]

    uint   WN [3];
#define WN1 WN [0]
#define WN2 WN [1]
#define WN3 CN [2]

    uint   C [3];
#define C1 C [0]
#define C2 C [1]
#define C3 C [2]

    uint   B [3];
#define B1 B [0]
#define B2 B [1]
#define B3 B [2]

    uint    N [3];
#define N1  N [0]
#define N2  N [1]
#define N3  N [2]
    
    uint   TN [3];          // type numeric
#define TN1 TN [0]
#define TN2 TN [1]
#define TN3 TN [2]

#ifdef EIS_PTR3
#define TA1 cpu.du.TAk[0]
#define TA2 cpu.du.TAk[1]
#define TA3 cpu.du.TAk[2]
#else
    uint   TA [3];          // type alphanumeric
#define TA1 TA [0]
#define TA2 TA [1]
#define TA3 TA [2]
#endif

    uint   S [3];           // Sign and decimal type of number
#define S1  S [0]
#define S2  S [1]
#define S3  S [2]

    int    SF [3];          // scale factor
#define SF1 SF [0]
#define SF2 SF [1]
#define SF3 SF [2]

    word18 _flags;          // flags set during operation
    word18 _faults;         // faults generated by instruction
    
    word72s x;              // a signed, 128-bit integers for playing with ...
 
    // Stuff for Micro-operations and Edit instructions...
    
    word9   editInsertionTable [8];     // 8 9-bit chars
    
    int     mopIF;          // current micro-operation IF field
    MOPstruct *m;           // pointer to current MOP struct
    
    word9   inBuffer [64];  // decimal unit input buffer
    word9   *in;            // pointer to current read position in inBuffer
    uint    inBufferCnt;    // number of characters in inBuffer
    word9   outBuffer [64]; // output buffer
    word9   *out;           // pointer to current write position in outBuffer;
    
    int     exponent;       // For decimal floating-point (evil)
    int     sign;           // For signed decimal (1, -1)
    
#ifdef EIS_PTR2
#define KMOP 1
#else
    EISaddr *mopAddress;    // mopAddress, pointer to addr [0], [1], or [2]
#endif
    
    int     mopTally;       // number of micro-ops
    int     mopPos;         // current mop char posn
    
    // Edit Flags
    // The processor provides the following four edit flags for use by the
    // micro operations.
    
    bool    mopES;          // End Suppression flag; initially OFF, set ON by
                            //  a micro operation when zero-suppression ends.
    bool    mopSN;          // Sign flag; initially set OFF if the sending
                            //  string has an alphanumeric descriptor or an
                            //  unsigned numeric descriptor. If the sending
                            //  string has a signed numeric descriptor, the
                            //  sign is initially read from the sending string
                            //  from the digit position defined by the sign
                            //  and the decimal type field (S); SN is set
                            //  OFF if positive, ON if negative. If all
                            //  digits are zero, the data is assumed positive
                            //  and the SN flag is set OFF, even when the
                            //  sign is negative.
    bool    mopZ;           // Zero flag; initially set ON. It is set OFF 
                            //  whenever a sending string character that is not
                            //  decimal zero is moved into the receiving string.
    bool    mopBZ;          // Blank-when-zero flag; initially set OFF and
                            //  set ON by either the ENF or SES micro
                            //  operation. If, at the completion of a move
                            //  (L1 exhausted), both the Z and BZ flags are
                            //  ON, the receiving string is filled with
                            //  character 1 of the edit insertion table.

    EISaddr addr [3];
    
#define     ADDR1       addr [0]
    int     srcTally;       // number of chars in src (max 63)
    int     srcTA;          // type of Alphanumeric chars in src
    int     srcSZ;          // size of chars in src (4-, 6-, or 9-bits)

#define     ADDR2       addr [1]

#define     ADDR3       addr [2]
    int     dstTally;       // number of chars in dst (max 63)
    int     dstSZ;          // size of chars in dst (4-, 6-, or 9-bits)
    
    bool    mvne;           // for MSES micro-op. True when mvne, false when mve
  } EISstruct;

// Instruction decode structure. Used to represent instrucion information

struct DCDstruct
  {
    opCode * info;        // opCode *
    uint32 opcode;        // opcode
    bool   opcodeX;       // opcode extension
    word18 address;       // bits 0-17 of instruction
    word1  b29;           // bit-29 - address via pointer register. Usually.
    bool   i;             // interrupt inhinit bit.
    word6  tag;           // instruction tag
    
    bool stiTally;      // for sti instruction
    bool restart;         // instruction is to be restarted
  };

// Emulator-only interrupt and fault info

typedef struct
  {
    //bool any;             // true if any of the below are true
    //bool int_pending;
    bool fault_pending;
    int fault [N_FAULT_GROUPS];
                          // only one fault in groups 1..6 can be pending
    //bool interrupts [N_SCU_UNITS_MAX] [N_INTERRUPTS];
    bool XIP [N_SCU_UNITS_MAX];
  } events_t;

// Physical Switches

typedef struct
  {
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

    // Emulator run-time options (virtual switches)
    uint dis_enable;      // If non-zero, DIS works
    uint halt_on_unimp;   // If non-zero, halt CPU on unimplemented instruction
                          // instead of faulting
    uint report_faults;   // If set, faults are reported and ignored
    uint tro_enable;   // If set, Timer runout faults are generated.
    uint drl_fatal;
    uint serno;
    bool useMap;
  } switches_t;

#ifdef L68
enum ou_cycle_e
  {
    ou_GIN = 0400,
    ou_GOS = 0200,
    ou_GD1 = 0100,
    ou_GD2 = 0040,
    ou_GOE = 0020,
    ou_GOA = 0010,
    ou_GOM = 0004,
    ou_GON = 0002,
    ou_GOF = 0001
  };
#endif

typedef struct
  {
    // Operations Unit/Address Modification
    bool directOperandFlag;
    word36 directOperand;
    word6 characterOperandSize;
    word6 characterOperandOffset;
    bool crflag;
#ifdef L68
    word2 eac;
    word1 RB1_FULL;
    word1 RP_FULL;
    word1 RS_FULL;
    word9 cycle;
    word1 STR_OP;
#endif
#ifdef PANEL
    word9 RS;
    word4 opsz;
    word10 reguse;
#endif
  } ou_unit_data_t;

// APU history operation parameter

enum APUH_e
  {
    APUH_FDSPTW = 1llu << (35 - 17),
    APUH_MDSPTW = 1llu << (35 - 18),
    APUH_FSDWP =  1llu << (35 - 19),
    APUH_FPTW =   1llu << (35 - 20),
    APUH_FPTW2 =  1llu << (35 - 21),
    APUH_MPTW =   1llu << (35 - 22),
    APUH_FANP =   1llu << (35 - 23),
    APUH_FAP =    1llu << (35 - 24)
  };

enum { 
//   AL39 pg 64 APU hist.
    apu_FLT = 1ll << (33 - 0),    //  0   l FLT Access violation or directed fault on this cycle
                                  //  1-2 a BSY    Data source for ESN
    apu_ESN_PSR = 0,              //                  00 PPR.PSR
    apu_ESN_SNR = 1ll << (33- 1), //                  01 PRn.SNR
    apu_ESN_TSR = 1ll << (33- 2), //                  10 TPR.TSR
                                  //                  11 not used
                                  //  3     PRAP
    apu_HOLD = 1ll <<  (33- 4),   //  4     HOLD  An access violation or directed fault is waiting
                                  //  5     FRIW
                                  //  6     XSF
                                  //  7     STF
    apu_TP_P = 1ll <<  (33- 8),   //  8     TP P    Guessing PPR.p set from SDW.P
    apu_PP_P = 1ll <<  (33- 9),   //  9     PP P    PPR.P?
                                  // 10     ?
                                  // 11     S-ON   Segment on?
                                  // 12     ZMAS
                                  // 13     SDMF   Seg. Descr. Modify?
                                  // 14     SFND
                                  // 15     ?
                                  // 16     P-ON   Page on?
                                  // 17     ZMAP
                                  // 18     PTMF
                                  // 19     PFND  
    apu_FDPT = 1ll << (33-20),    // 20   b FDPT   Fetch descriptor segment PTW
    apu_MDPT = 1ll << (33-21),    // 21   c MDPT   Modify descriptor segment PTW
    apu_FSDP = 1ll << (33-22),    // 22   d FSDP   Fetch SDW paged descr. seg.
    apu_FSDN = 1ll << (33-23),    // 23     FSDN   Fetch SDW non-paged 
    apu_FPTW = 1ll << (33-24),    // 24   e FPTW   Fetch PTW
    apu_MPTW = 1ll << (33-25),    // 25   g MPTW   Modify PTW
                                  // 26   f FPT2 // Fetch prepage
    apu_FAP  = 1ll << (33-27),    // 27   i FAP    Final address fetch from paged seg.
    apu_FANP = 1ll << (33-28),    // 28   h FANP   Final address fetch from non-paged segment
                                  // 29     FAAB   Final address absolute?
    apu_FA   = 1ll << (33-30),    // 30     FA     Final address?
                                  // 31     EAAU
    apu_PIAU = 1ll << (33-32)     // 32     PIAU   Instruction fetch?
                                  // 33     TGAU
  };

typedef struct
  {
#ifdef RALRx
    _processor_cycle_type lastCycle;
#endif
#ifdef PANEL
    word34 state;
#endif
  } apu_unit_data_t;

typedef struct
  {
    // NB: Some of the data normally stored here is represented
    // elsewhere -- e.g.,the PPR is a variable outside of this
    // struct.   Other data is live and only stored here.

    // This is a collection of flags and registers from the
    // appending unit and the control unit.  The scu and rcu
    // instructions store and load these values to an 8 word
    // memory block.
    //
    // The CU data may only be valid for use with the scu and
    // rcu instructions.
    //
    // Comments indicate format as stored in 8 words by the scu
    // instruction.
    
    // NOTE: PPR (procedure pointer register) is a combination of registers:
    //   From the Appending Unit
    //     PRR bits [0..2] of word 0
    //     PSR bits [3..17] of word 0
    //     P   bit 18 of word 0
    //   From the Control Unit
    //     IC  bits [0..17] of word 4
    
    /* word 0 */
                   // 0-2   PRR is stored in PPR
                   // 3-17  PSR is stored in PPR
                   // 18    P   is stored in PPR
    word1 XSF;     // 19    XSF External segment flag
    word1 SDWAMM;  // 20    SDWAMM Match on SDWAM
    word1 SD_ON;   // 21    SDWAM enabled
    word1 PTWAMM;  // 22    PTWAMM Match on PTWAM
    word1 PT_ON;   // 23    PTWAM enabled

#if 0
    word1 PI_AP;   // 24    PI-AP Instruction fetch append cycle
    word1 DSPTW;   // 25    DSPTW Fetch descriptor segment PTW
    word1 SDWNP;   // 26    SDWNP Fetch SDW non paged
    word1 SDWP;    // 27    SDWP  Fetch SDW paged
    word1 PTW;     // 28    PTW   Fetch PTW
    word1 PTW2;    // 29    PTW2  Fetch prepage PTW
    word1 FAP;     // 30    FAP   Fetch final address - paged
    word1 FANP;    // 31    FANP  Fetch final address - nonpaged
    word1 FABS;    // 32    FABS  Fetch final address - absolute
                   // 33-35 FCT   Fault counter - counts retries
#else
    word12 APUCycleBits;
#endif

    /* word 1 */
                   //               AVF Access Violation Fault
                   //               SF  Store Fault
                   //               IPF Illegal Procedure Fault
                   //
    word1 IRO_ISN; //  0    IRO       AVF Illegal Ring Order
                   //       ISN       SF  Illegal segment number
    word1 OEB_IOC; //  1    ORB       AVF Out of execute bracket [sic] should be OEB?
                   //       IOC       IPF Illegal op code
    word1 EOFF_IAIM;
                   //  2    E-OFF     AVF Execute bit is off
                   //       IA+IM     IPF Illegal address of modifier
    word1 ORB_ISP; //  3    ORB       AVF Out of read bracket
                   //       ISP       IPF Illegal slave procedure
    word1 ROFF_IPR;//  4    R-OFF     AVF Read bit is off
                   //       IPR       IPF Illegal EIS digit
    word1 OWB_NEA; //  5    OWB       AVF Out of write bracket
                   //       NEA       SF  Nonexistant address
    word1 WOFF_OOB;//  6    W-OFF     AVF Write bit is off
                   //       OOB       SF  Out of bounds (BAR mode)
    word1 NO_GA;   //  7    NO GA     AVF Not a gate
    word1 OCB;     //  8    OCB       AVF Out of call bracket
    word1 OCALL;   //  9    OCALL     AVF Outward call
    word1 BOC;     // 10    BOC       AVF Bad outward call
// PTWAM error is DPS8M only
    word1 PTWAM_ER;// 11    PTWAM_ER  AVF PTWAM error // inward return
    word1 CRT;     // 12    CRT       AVF Cross ring transfer
    word1 RALR;    // 13    RALR      AVF Ring alarm
// On DPS8M a SDWAM error, on DP8/L68 a WAM error
    word1 SDWAM_ER;// 14    SWWAM_ER  AVF SDWAM error
    word1 OOSB;    // 15    OOSB      AVF Out of segment bounds
    word1 PARU;    // 16    PARU      Parity fault - processor parity upper
    word1 PARL;    // 17    PARL      Parity fault - processor parity lower
    word1 ONC1;    // 18    ONC1      Operation not complete fault error #1
    word1 ONC2;    // 19    ONC2      Operation not complete fault error #2
    word4 IA;      // 20-23 IA        System control illegal action lines
    word3 IACHN;   // 24-26 IACHN     Illegal action processor port
    word3 CNCHN;   // 27-29 CNCHN     Connect fault - connect processor port
    word5 FI_ADDR; // 30-34 F/I ADDR  Modulo 2 fault/interrupt vector address
    word1 FLT_INT; // 35    F/I       0 = interrupt; 1 = fault

    /* word 2 */
                   //  0- 2 TRR
                   //  3-17 TSR
                   // 18-21 PTW
                   //                  18  PTWAM levels A, B enabled
                   //                  19  PTWAM levels C, D enabled
                   //                  20  PTWAM levels A, B match
                   //                  21  PTWAM levels C, D match
                   // 22-25 SDW
                   //                  22  SDWAM levels A, B enabled
                   //                  23  SDWAM levels C, D enabled
                   //                  24  SDWAM levels A, B match
                   //                  25  SDWAM levels C, D match
                   // 26             0
                   // 27-29 CPU      CPU Number
    word6 delta;   // 30-35 DELTA    addr increment for repeats
    
    /* word 3 */
                   //  0-17          0
                   // 18-21 TSNA     Pointer register number for non-EIS 
                   //                operands or EIS Operand #1
                   //                  18-20 PRNO Pointer register number
                   //                  21       PRNO is valid
                   // 22-25 TSNB     Pointer register number for EIS operand #2
                   //                  22-24 PRNO Pointer register number
                   //                  25       PRNO is valid
                   // 26-29 TSNC     Pointer register number for EIS operand #2
                   //                  26-28 PRNO Pointer register number
                   //                  29       PRNO is valid
    word3 TSN_PRNO [3];
    word1 TSN_VALID [3];
#define ISB29 (cpu.cu.TSN_VALID [0])

                   // 30-35 TEMP BIT Current bit offset (TPR . TBR)

    /* word 4 */
                   //  0-17 PPR . IC
    word18 IR;     // 18-35 Indicator register
                   //    18 ZER0
                   //    19 NEG
                   //    20 CARY
                   //    21 OVFL
                   //    22 EOVF
                   //    23 EUFL
                   //    24 OFLM
                   //    25 TRO
                   //    26 PAR
                   //    27 PARM
                   //    28 -BM
                   //    29 TRU
                   //    30 MIF
                   //    31 ABS
                   //    32 HEX [sic] Figure 3-32 is wrong.
                   // 33-35 0
                    
    /* word 5 */

                   //  0-17 COMPUTED ADDRESS (TPR . CA)
    word1 repeat_first; 
                   // 18    RF  First cycle of all repeat instructions
    word1 rpt;     // 19    RPT Execute an Repeat (rpt) instruction
    word1 rd;      // 20    RD  Execute an Repeat Double (rpd) instruction
    word1 rl;      // 21    RL  Execute a Repeat Link (rpl) instruction
    word1 pot;     // 22    POT Prepare operand tally
                   // 23    PON Prepare operand no tally
    //xde xdo
    // 0   0   no execute           -> 0 0
    // 1   0   execute XEC          -> 0 0
    // 1   1   execute even of XED  -> 0 1
    // 0   1   execute odd of XED   -> 0 0
    word1 xde;     // 24    XDE Execute instruction from Execute Double even pair
    word1 xdo;     // 25    XDO Execute instruction from Execute Double odd pair
                   // 26    ITP Execute ITP indirect cycle
    word1 rfi;     // 27    RFI Restart this instruction
                   // 28    ITS Execute ITS indirect cycle
    word1 FIF;     // 29    FIF Fault occured during instruction fetch
    word6 CT_HOLD; // 30-35 CT HOLD contents of the "remember modifier" register

    
    
    /* word 6 */
    word36 IWB;

    /* word 7 */
    word36 IRODD; /* Instr holding register; odd word of last pair fetched */
 } ctl_unit_data_t;

#define USE_IRODD (cpu.cu.rd && ((cpu. PPR.IC & 1) != 0)) 
#define IWB_IRODD (USE_IRODD ? cpu.cu.IRODD : cpu.cu.IWB)

#ifdef L68
enum du_cycle1_e
  {
    //  0 -FPOL Prepare operand length
    du1_nFPOL        = 0400000000000ll,
    //  1 -FPOP Prepare operand pointer
    du1_nFPOP        = 0200000000000ll,
    //  2 -NEED-DESC Need descriptor 
    du1_nNEED_DESC   = 0100000000000ll,
    //  3 -SEL-ADR Select address register
    du1_nSEL_DIR     = 0040000000000ll,
    //  4 -DLEN=DIRECT Length equals direct
    du1_nDLEN_DIRECT = 0020000000000ll,
    //  5 -DFRST Descriptor processed for first time
    du1_nDFRST       = 0010000000000ll,
    //  6 -FEXR Extended register modification
    du1_nFEXR        = 0004000000000ll,
    //  7 -DLAST-FRST Last cycle of DFRST
    du1_nLAST_DFRST  = 0002000000000ll,
    //  8 -DDU-LDEA Decimal unit load  (lpl?)
    du1_nDDU_LDEA    = 0001000000000ll,
    //  9 -DDU-STAE Decimal unit store (spl?)
    du1_nDDU_STEA    = 0000400000000ll,
    // 10 -DREDO Redo operation without pointer and length update
    du1_nDREDO       = 0000200000000ll,
    // 11 -DLVL<WD-SZ Load with count less than word size
    du1_nDLVL_WD_SZ  = 0000100000000ll,
    // 12 -EXH Exhaust
    du1_nEXH         = 0000040000000ll,
    // 13 DEND-SEQ End of sequence
    du1_DEND_SEQ     = 0000020000000ll,
    // 14 -DEND End of instruction
    du1_nEND         = 0000010000000ll,
    // 15 -DU=RD+WRT Decimal unit write-back
    du1_nDU_RD_WRT   = 0000004000000ll,
    // 16 -PTRA00 PR address bit 0
    du1_nPTRA00      = 0000002000000ll,
    // 17 -PTRA01 PR address bit 1
    du1_nPTRA01      = 0000001000000ll,
    // 18 FA/Il Descriptor l active
    du1_FA_I1        = 0000000400000ll,
    // 19 FA/I2 Descriptor 2 active
    du1_FA_I2        = 0000000200000ll,
    // 20 FA/I3 Descriptor 3 active
    du1_FA_I3        = 0000000100000ll,
    // 21 -WRD Word operation
    du1_nWRD         = 0000000040000ll,
    // 22 -NINE 9-bit character operation
    du1_nNINE        = 0000000020000ll,
    // 23 -SIX 6-bit character operation
    du1_nSIX         = 0000000010000ll,
    // 24 -FOUR 4-bit character operation
    du1_nFOUR        = 0000000004000ll,
    // 25 -BIT Bit operation
    du1_nBIT         = 0000000002000ll,
    // 26 Unused
    //               = 0000000001000ll,
    // 27 Unused
    //               = 0000000000400ll,
    // 28 Unused
    //               = 0000000000200ll,
    // 29 Unused
    //               = 0000000000100ll,
    // 30 FSAMPL Sample for mid-instruction interrupt
    du1_FSAMPL       = 0000000000040ll,
    // 31 -DFRST-CT Specified first count of a sequence
    du1_nDFRST_CT    = 0000000000020ll,
    // 32 -ADJ-LENGTH Adjust length
    du1_nADJ_LENTGH  = 0000000000010ll,
    // 33 -INTRPTD Mid-instruction interrupt
    du1_nINTRPTD     = 0000000000004ll,
    // 34 -INHIB Inhibit STC1 (force "STC0")
    du1_nINHIB       = 0000000000002ll,
    // 35 Unused
    //               = 0000000000001ll,
  };

enum du_cycle2_e
  {
    // 36 DUD Decimal unit idle
    du2_DUD          = 0400000000000ll,
    // 37 -GDLDA Descriptor load gate A
    du2_nGDLDA       = 0200000000000ll,
    // 38 -GDLDB Descriptor load gate B
    du2_nGDLDB       = 0100000000000ll,
    // 39 -GDLDC Descriptor load gate C
    du2_nGDLDC       = 0040000000000ll,
    // 40 NLD1 Prepare alignment count for first numeric operand load
    du2_NLD1         = 0020000000000ll,
    // 41 GLDP1 Numeric operand one load gate
    du2_GLDP1        = 0010000000000ll,
    // 42 NLD2 Prepare alignment count for second numeric operand load
    du2_NLD2         = 0004000000000ll,
    // 43 GLDP2 Numeric operand two load gate
    du2_GLDP2        = 0002000000000ll,
    // 44 ANLD1 Alphanumeric operand one load gate
    du2_ANLD1        = 0001000000000ll,
    // 45 ANLD2 Alphanumeric operand two load gate
    du2_ANLD2        = 0000400000000ll,
    // 46 LDWRT1 Load rewrite register one gate (XXX Guess indirect desc. MFkID)
    du2_LDWRT1       = 0000200000000ll,
    // 47 LDWRT2 Load rewrite register two gate (XXX Guess indirect desc. MFkID)
    du2_LDWRT2       = 0000100000000ll,
    // 50 -DATA-AVLDU Decimal unit data available
    du2_nDATA_AVLDU  = 0000040000000ll,
    // 49 WRT1 Rewrite register one loaded
    du2_WRT1         = 0000020000000ll,
    // 50 GSTR Numeric store gate
    du2_GSTR         = 0000010000000ll,
    // 51 ANSTR Alphanumeric store gate
    du2_ANSTR        = 0000004000000ll,
    // 52 FSTR-OP-AV Operand available to be stored
    du2_FSTR_OP_AV   = 0000002000000ll,
    // 53 -FEND-SEQ End sequence flag
    du2_nFEND_SEQ    = 0000001000000ll,
    // 54 -FLEN<128 Length less than 128
    du2_nFLEN_128    = 0000000400000ll,
    // 55 FGCH Character operation gate
    du2_FGCH         = 0000000200000ll,
    // 56 FANPK Alphanumeric packing cycle gate
    du2_FANPK        = 0000000100000ll,
    // 57 FEXMOP Execute MOP gate
    du2_FEXOP        = 0000000040000ll,
    // 58 FBLNK Blanking gate
    du2_FBLNK        = 0000000020000ll,
    // 59 Unused
    //               = 0000000010000ll,
    // 60 DGBD Binary to decimal execution gate
    du2_DGBD         = 0000000004000ll,
    // 61 DGDB Decimal to binary execution gate
    du2_DGDB         = 0000000002000ll,
    // 62 DGSP Shift procedure gate
    du2_DGSP         = 0000000001000ll,
    // 63 FFLTG Floating result flag
    du2_FFLTG        = 0000000000400ll,
    // 64 FRND Rounding flag
    du2_FRND         = 0000000000200ll,
    // 65 DADD-GATE Add/subtract execute gate
    du2_DADD_GATE    = 0000000000100ll,
    // 66 DMP+DV-GATE Multiply/divide execution gate
    du2_DMP_DV_GATE  = 0000000000040ll,
    // 67 DXPN-GATE Exponent network execution gate
    du2_DXPN_GATE    = 0000000000020ll,
    // 68 Unused
    //               = 0000000000010ll,
    // 69 Unused
    //               = 0000000000004ll,
    // 70 Unused
    //               = 0000000000002ll,
    // 71 Unused
    //               = 0000000000001ll,
  };

#define DU_CYCLE_GDLDA { clrmask (& cpu.du.cycle2, du2_nGDLDA); \
                    setmask (& cpu.du.cycle2, du2_nGDLDB | du2_nGDLDC); }
#define DU_CYCLE_GDLDB { clrmask (& cpu.du.cycle2, du2_nGDLDB); \
                    setmask (& cpu.du.cycle2, du2_nGDLDA | du2_nGDLDC); }
#define DU_CYCLE_GDLDC { clrmask (& cpu.du.cycle2, du2_nGDLDC); \
                    setmask (& cpu.du.cycle2, du2_nGDLDA | du2_nGDLDB); }
#define DU_CYCLE_FA_I1 setmask (& cpu.du.cycle1, du1_FA_I1)
#define DU_CYCLE_FA_I2 setmask (& cpu.du.cycle1, du1_FA_I2)
#define DU_CYCLE_FA_I3 setmask (& cpu.du.cycle1, du1_FA_I3)
#define DU_CYCLE_ANLD1 setmask (& cpu.du.cycle2, du2_ANLD1)
#define DU_CYCLE_ANLD2 setmask (& cpu.du.cycle2, du2_ANLD2)
#define DU_CYCLE_NLD1  setmask (& cpu.du.cycle2, du2_NLD1)
#define DU_CYCLE_NLD2  setmask (& cpu.du.cycle2, du2_NLD2)
#define DU_CYCLE_FRND  setmask (& cpu.du.cycle2, du2_FRND)
#define DU_CYCLE_DGBD  setmask (& cpu.du.cycle2, du2_DGBD)
#define DU_CYCLE_DGDB  setmask (& cpu.du.cycle2, du2_DGDB)
#define DU_CYCLE_DDU_LDEA clrmask (& cpu.du.cycle1, du1_nDDU_LDEA)
#define DU_CYCLE_DDU_STEA clrmask (& cpu.du.cycle1, du1_nDDU_STEA)
#define DU_CYCLE_END clrmask (& cpu.du.cycle1, du1_nEND)
#define DU_CYCLE_LDWRT1  setmask (& cpu.du.cycle2, du2_LDWRT1)
#define DU_CYCLE_LDWRT2  setmask (& cpu.du.cycle2, du2_LDWRT2)
#define DU_CYCLE_FEXOP  setmask (& cpu.du.cycle2, du2_FEXOP)
#define DU_CYCLE_ANSTR  setmask (& cpu.du.cycle2, du2_ANSTR)
#define DU_CYCLE_GSTR  setmask (& cpu.du.cycle2, du2_GSTR)
#define DU_CYCLE_FLEN_128  clrmask (& cpu.du.cycle2, du2_nFLEN_128)
#define DU_CYCLE_FDUD  { cpu.du.cycle1 = \
                      du1_nFPOL | \
                      du1_nFPOP | \
                      du1_nNEED_DESC | \
                      du1_nSEL_DIR | \
                      du1_nDLEN_DIRECT | \
                      du1_nDFRST | \
                      du1_nFEXR | \
                      du1_nLAST_DFRST | \
                      du1_nDDU_LDEA | \
                      du1_nDDU_STEA | \
                      du1_nDREDO | \
                      du1_nDLVL_WD_SZ | \
                      du1_nEXH | \
                      du1_nEND | \
                      du1_nDU_RD_WRT | \
                      du1_nWRD | \
                      du1_nNINE | \
                      du1_nSIX | \
                      du1_nFOUR | \
                      du1_nBIT | \
                      du1_nINTRPTD | \
                      du1_nINHIB; \
                    cpu.du.cycle2 = \
                      du2_DUD | \
                      du2_nGDLDA | \
                      du2_nGDLDB | \
                      du2_nGDLDC | \
                      du2_nDATA_AVLDU | \
                      du2_nFEND_SEQ | \
                      du2_nFLEN_128; \
                  }
#define DU_CYCLE_nDUD clrmask (& cpu.du.cycle2, du2_DUD) 
#endif

#ifdef PANEL
// Control points

#define CPT(R,C) cpu.cpt[R][C]=1
#define CPTUR(C) cpu.cpt[cpt5L][C]=1
#else
#define CPT(R,C)
#define CPTUR(C)
#endif

#if 0
#ifdef PANEL
// 6180 panel DU control flags with guessed meanings based on DU history 
// register bits.
// 
enum du_cycle1_e
  {
    du1_FDUD  = 01000000000000ll, // Decimal Unit Idle
    du1_GDLD  = 00400000000000ll, // Decimal Unit Load
    du1_GLP1  = 00200000000000ll, // PR address bit 0
    du1_GLP2  = 00100000000000ll, // PR address bit 1
    du1_GEA1  = 00040000000000ll, // Descriptor 1 active
    du1_GEM1  = 00020000000000ll, // 
    du1_GED1  = 00010000000000ll, // Prepare alignment count for first numeric operand load
    du1_GDB   = 00004000000000ll, // Decimal to binary gate
    du1_GBD   = 00002000000000ll, // Binary to decimal gate
    du1_GSP   = 00001000000000ll, // Shift procedure gate
    du1_GED2  = 00000400000000ll, // Prepare alignment count for second numeric operand load
    du1_GEA2  = 00000200000000ll, // Descriptor 2 active
    du1_GADD  = 00000100000000ll, // Add subtract execute gate
    du1_GCMP  = 00000040000000ll, // 
    du1_GMSY  = 00000020000000ll, // 
    du1_GMA   = 00000010000000ll, // 
    du1_GMS   = 00000004000000ll, // 
    du1_GQDF  = 00000002000000ll, // 
    du1_GQPA  = 00000001000000ll, // 
    du1_GQR1  = 00000000400000ll, // Load rewrite register one gate
    du1_GQR2  = 00000000200000ll, // Load rewrite register two gate
    du1_GRC   = 00000000100000ll, // 
    du1_GRND  = 00000000040000ll, // 
    du1_GCLZ  = 00000000020000ll, // Load with count less than word size
    du1_GEDJ  = 00000000010000ll, // ? is the GED3?
    du1_GEA3  = 00000000004000ll, // Descriptor 3 active
    du1_GEAM  = 00000000002000ll, // 
    du1_GEDC  = 00000000001000ll, // 
    du1_GSTR  = 00000000000400ll, // Decimal unit store
    du1_GSDR  = 00000000000200ll, // 
    du1_NSTR  = 00000000000100ll, // Numeric store gate
    du1_SDUD  = 00000000000040ll, //
    du1_U32   = 00000000000020ll, // ?
    du1_U33   = 00000000000010ll, // ?
    du1_U34   = 00000000000004ll, // ?
    du1_FLTG  = 00000000000002ll, // Floating result flag
    du1_FRND  = 00000000000001ll  // Rounding flag
  };

enum du_cycle2_e
  {
    du2_ALD1  = 01000000000000ll, // Alphanumeric operand one load gate
    du2_ALD2  = 00400000000000ll, // Alphanumeric operand two load gate
    du2_NLD1  = 00200000000000ll, // Numeric operand one load gate
    du2_NLD2  = 00100000000000ll, // Numeric operand two load gate
    du2_LWT1  = 00040000000000ll, // Load rewrite register one gate
    du2_LWT2  = 00020000000000ll, // Load rewrite register two gate
    du2_ASTR  = 00010000000000ll, // Alphanumeric store gate
    du2_ANPK  = 00004000000000ll, // Alphanumeric packing cycle gate
    du2_FGCH  = 00002000000000ll, // Character operation gate
    du2_XMOP  = 00001000000000ll, // Execute MOP
    du2_BLNK  = 00000400000000ll, // Blanking gate
    du2_U11   = 00000200000000ll, // 
    du2_U12   = 00000100000000ll, // 
    du2_CS_0  = 00000040000000ll, // 
    du2_CU_0  = 00000020000000ll, //  CS=0
    du2_FI_0  = 00000010000000ll, //  CU=0
    du2_CU_V  = 00000004000000ll, //  CU=V
    du2_UM_V  = 00000002000000ll, //  UM<V
    du2_U18   = 00000001000000ll, // ?
    du2_U19   = 00000000400000ll, // ?
    du2_U20   = 00000000200000ll, // ?
    du2_U21   = 00000000100000ll, // ?
    du2_U22   = 00000000040000ll, // ?
    du2_U23   = 00000000020000ll, // ?
    du2_U24   = 00000000010000ll, // ?
    du2_U25   = 00000000004000ll, // ?
    du2_U26   = 00000000002000ll, // ?
    du2_U27   = 00000000001000ll, // ?
    du2_L128  = 00000000000400ll, // L<128 Length less than 128
    du2_END_SEQ = 00000000000200ll, // End sequence flag
    du2_U29   = 00000000000100ll, // ?
    du2_U31   = 00000000000040ll, // ?
    du2_U32   = 00000000000020ll, // ?
    du2_U33   = 00000000000010ll, // ?
    du2_U34   = 00000000000004ll, // ?
    du2_U35   = 00000000000002ll, // ?
    du2_U36   = 00000000000001ll  // ?
  };
#endif
#endif

typedef struct du_unit_data_t
  {
    // Word 0
    
                      //  0- 8  9   Zeros
    word1 Z;          //     9  1   Z       All bit-string instruction results 
                      //                      are zero
    word1 NOP;        //    10  1   Ø       Negative overpunch found in 6-4 
                      //                      expanded move
    word24 CHTALLY;   // 12-35 24   CHTALLY The number of characters examined 
                      //                      by the scm, scmr, scd,
                      //                      scdr, tct, or tctr instructions
                      //                      (up to the interrupt or match)

    // Word 1

                      //  0-35 26   Zeros

    // Word 2

    // word24 D1_PTR; //  0-23 24   D1 PTR  Address of the last double-word
                      //                      accessed by operand descriptor 1;
                      //                      bits 17-23 (bit-address) valid
                      //                      only for initial access
                      //    24  1   Zero
    // word2 TA1;     // 25-26  2   TA1     Alphanumeric type of operand 
                      //                      descriptor 1
                      // 27-29  3   Zeroes
                      //    30  1   I       Decimal unit interrupted flag; a 
                      //                      copy of the mid-instruction
                      //                      interrupt fault indicator
    // word1 F1;      //    31  1   F1      First time; data in operand 
                      //                      descriptor 1 is valid
    // word1 A1;      //    32  1   A1      Operand descriptor 1 is active
                      // 33-35  3   Zeroes

    // Word 3

    word10 LEVEL1;    //  0- 9 10   LEVEL 1 Difference in the count of 
                      //                      characters loaded into the and 
                      //                      processor characters not acted 
                      //                      upon
    // word24 D1_RES; // 12-35 24   D1 RES  Count of characters remaining in
                      //                      operand descriptor 1

    // Word 4

    // word24 D2_PTR; //  0-23 24   D2 PTR  Address of the last double-word 
                      //                      accessed by operand descriptor 2;
                      //                      bits 17-23 (bit-address) valid
                      //                      only for initial access
                      //    24  1   Zero
    // word2 TA2;     // 25-26  2   TA2     Alphanumeric type of operand 
                      //                      descriptor 2
                      // 27-29  3   Zeroes
    word1 R;          //    30  1   R       Last cycle performed must be 
                      //                    repeated
    // word1 F2;      //    31  1   F2      First time; data in operand 
                      //                      descriptor 2 is valid
    // word1 A2;      //    32  1   A2      Operand descriptor 2 is active
                      // 33-35  3   Zeroes

    // Word 5

    word10 LEVEL2;    //  0- 9 10   LEVEL 2 Same as LEVEL 1, but used mainly 
                      //                      for OP 2 information
    // word24 D2_RES; // 12-35 24   D2 RES  Count of characters remaining in
                      //                      operand descriptor 2

    // Word 6

    // word24 D3_PTR; //  0-23 24   D3 PTR  Address of the last double-word 
                      //                      accessed by operand descriptor 3;
                      //                      bits 17-23 (bit-address) valid
                      //                      only for initial access
                      //    24  1   Zero
    // word2 TA3;     // 25-26  2   TA3     Alphanumeric type of operand 
                      //                      descriptor 3
                      // 27-29  3   Zeroes
                      //    30  1   R       Last cycle performed must be 
                      //                      repeated
                      //                    [XXX: what is the difference between
                      //                      this and word4.R]
    // word1 F3;      //    31  1   F3      First time; data in operand 
                      //                      descriptor 3 is valid
    // word1 A3;      //    32  1   A3      Operand descriptor 3 is active
    word3 JMP;        // 33-35  3   JMP     Descriptor count; number of words 
                      //                      to skip to find the next
                      //                      instruction following this 
                      //                      multiword instruction

    // Word 7

                      //  0-12 12   Zeroes
    // word24 D3_RES; // 12-35 24   D3 RES  Count of characters remaining in
                      //                      operand descriptor 3

    // Fields from above reorganized for generality
    word2 TAk [3];

// D_PTR is a word24 divided into a 18 bit address, and a 6-bit bitno/char 
// field

    word18 Dk_PTR_W [3];
#define D1_PTR_W Dk_PTR_W [0]
#define D2_PTR_W Dk_PTR_W [1]
#define D3_PTR_W Dk_PTR_W [2]

    word6 Dk_PTR_B [3];
#define D1_PTR_B Dk_PTR_B [0]
#define D2_PTR_B Dk_PTR_B [1]
#define D3_PTR_B Dk_PTR_B [2]

    word24 Dk_RES [3];
#define D_RES Dk_RES
#define D1_RES Dk_RES [0]
#define D2_RES Dk_RES [1]
#define D3_RES Dk_RES [2]

    word1 Fk [3];
//#define F Fk
#define F1 Fk [0]
#define F2 Fk [0]
#define F3 Fk [0]

    word1 Ak [3];

    // Working storage for EIS instruction processing.

    // These values must be restored on instruction restart
    word7 MF [3]; // Modifier fields for each instruction.

#ifdef ISOLTS
    // Image of LPL/SPL for ISOLTS compliance
    word36 image [8];
#endif

#ifdef PANEL
    word37 cycle1;
    word37 cycle2;
    word1 POL; // Prepare operand length
    word1 POP; // Prepare operand pointer
#endif
  } du_unit_data_t;

#ifdef PANEL
// prepare_state bits
enum
  {
    ps_PIA = 0200,
    ps_POA = 0100,
    ps_RIW = 0040,
    ps_SIW = 0020,
    ps_POT = 0010,
    ps_PON = 0004,
    ps_RAW = 0002,
    ps_SAW = 0001
  };
#endif

// History registers

// CU History register flag2 field bit

enum { CUH_XINT = 0100, CUH_IFT = 040, CUH_CRD = 020, CUH_MRD = 010,
       CUH_MSTO = 04, CUH_PIB = 02 };

#define N_CPU_UNITS_MAX 8

#ifdef DPS8M
#define N_WAM_ENTRIES 64
#define N_WAM_MASK 077
#endif
#ifdef L68
#define N_WAM_ENTRIES 16
#define N_WAM_MASK 017
#endif

typedef struct
  {
    jmp_buf jmpMain; // This is the entry to the CPU state machine
    cycles_t cycle;
    unsigned long long cycleCnt;
    unsigned long long instrCnt;
    unsigned long faultCnt [N_FAULTS];

    // The following are all from the control unit history register:

    bool interrupt_flag;     // an interrupt is pending in this cycle
    bool g7_flag;            // a g7 fault is pending in this cycle;
    _fault faultNumber;      // fault number saved by doFault
    _fault_subtype subFault; // saved by doFault

    bool wasXfer;  // The previous instruction was a transfer

    bool wasInhibited; // One or both of the previous instruction 
                       // pair was interrupr inhibited.

    bool isExec;  // The instruction being executed is the target of
                  // an XEC or XED instruction
    bool isXED; // The instruction being executed is the target of an
                // XEC instruction
    //bool isb29; // The instruction has a valid bit 29 set when fetched

    DCDstruct currentInstruction;
    EISstruct currentEISinstruction;

    events_t events;
    switches_t switches;
    ctl_unit_data_t cu;
    du_unit_data_t du;
    ou_unit_data_t ou;
    apu_unit_data_t apu;
    word36 faultRegister [2];

    word36 itxPair [2];

    word36   rA;     // accumulator
    word36   rQ;     // quotient
    word8    rE;     // exponent [map: rE, 28 0's]

    word18   rX [8]; // index
    word27   rTR;    // timer [map: TR, 9 0's]
    struct timespec rTRTime; // time when rTR was set
    uint     rTRsample;
    word24   rY;     // address operand
    word6    rTAG;   // instruction tag
    word3    rRALR;  // ring alarm [3b] [map: 33 0's, RALR]
    word3    RSDWH_R1; // Track the ring number of the last SDW
    fault_acv_subtype_  acvFaults;   // pending ACV faults

    word18 lnk;  // rpl link value

    struct _tpr TPR;   // Temporary Pointer Register
    struct _ppr PPR;   // Procedure Pointer Register
    struct _par PAR [8]; // pointer/address resisters
    struct _bar BAR;   // Base Address Register
    struct _dsbr DSBR; // Descriptor Segment Base Register
#ifndef WAM
    //_sdw SDWAM0; // Segment Descriptor Word Associative Memory
#else
    _sdw SDWAM [N_WAM_ENTRIES]; // Segment Descriptor Word Associative Memory
#ifdef L68
    word4 SDWAMR;
#endif
#ifdef DPS8M
    word6 SDWAMR;
#endif
#endif
    _sdw * SDW; // working SDW
    _sdw SDW0; // a SDW not in SDWAM
    _sdw _s;
#ifdef PANEL
    // Intermediate data collection for APU SCROLL 
    word18 lastPTWOffset;
// The L68 APU SCROLL 4U has an entry "ACSD"; I am interpreting it as
//  on: lastPTRAddr was a DSPTW
//  off: lastPTRAddr was a PTW
    bool lastPTWIsDS;
    word18 APUDataBusOffset;
    word24 APUDataBusAddr;
    word24 APUMemAddr;
    word1 panel4_red_ready_light_state;
    word1 panel7_enabled_light_state;
// The state of the panel switches
    volatile word15 APU_panel_segno_sw;
    volatile word1  APU_panel_enable_match_ptw_sw;
    volatile word1  APU_panel_enable_match_sdw_sw;
    volatile word1  APU_panel_scroll_select_ul_sw;
    volatile word4  APU_panel_scroll_select_n_sw;
    volatile word4  APU_panel_scroll_wheel_sw;
    volatile word18 APU_panel_addr_sw;
    volatile word18 APU_panel_enter_sw;
    volatile word18 APU_panel_display_sw;
    volatile word4  CP_panel_wheel_sw;
    volatile word4  DATA_panel_ds_sw;
    volatile word4  DATA_panel_d1_sw;
    volatile word4  DATA_panel_d2_sw;
    volatile word4  DATA_panel_d3_sw;
    volatile word4  DATA_panel_d4_sw;
    volatile word4  DATA_panel_d5_sw;
    volatile word4  DATA_panel_d6_sw;
    volatile word4  DATA_panel_d7_sw;
    volatile word4  DATA_panel_wheel_sw;
    volatile word4  DATA_panel_addr_stop_sw;
    volatile word1  DATA_panel_enable_sw;
    volatile word1  DATA_panel_validate_sw;
    volatile word1  DATA_panel_auto_fast_sw;
    volatile word1  DATA_panel_auto_slow_sw;
    volatile word4  DATA_panel_cycle_sw;
    volatile word1  DATA_panel_step_sw;
    volatile word1  DATA_panel_s_trig_sw;
    volatile word1  DATA_panel_execute_sw;
    volatile word1  DATA_panel_scope_sw;
    volatile word1  DATA_panel_init_sw;
    volatile word1  DATA_panel_exec_sw;
    volatile word4  DATA_panel_hr_sel_sw;
    volatile word4  DATA_panel_trackers_sw;
    volatile bool panelInitialize;

    // Intermediate data collection for DATA SCROLL
    bool portBusy;
    word2 portSelect;
    word36 portAddr [N_CPU_PORTS];
    word36 portData [N_CPU_PORTS];
    // Intermediate data collection for CU
    word36 IWRAddr;
    word7 dataMode; // 0100  9 bit
                    // 0040  6 bit
                    // 0020  4 bit
                    // 0010  1 bit
                    // 0004  36 bit
                    // 0002  alphanumeric
                    // 0001  numeric
    word8 prepare_state;
    bool DACVpDF;
    bool AR_F_E;
    bool INS_FETCH;
    // Control Points data acquisition
    word1 cpt [28] [36];
#endif
#define cpt1U  0  // Instruction processing tracking
#define cpt1L  1  // Instruction processing tracking
#define cpt2U  2  // Instruction execution tracking
#define cpt2L  3  // Instruction execution tracking
#define cpt3U  4  // Register usage
#define cpt3L  5  // Register usage
#define cpt4U  6
#define cpt4L  7
#define cpt5U  8
#define cpt5L  9
#define cpt6U  10
#define cpt6L  11
#define cpt7U  12
#define cpt7L  13
#define cpt8U  14
#define cpt8L  15
#define cpt9U  16
#define cpt9L  17
#define cpt10U 18
#define cpt10L 19
#define cpt11U 20
#define cpt11L 21
#define cpt12U 22
#define cpt12L 23
#define cpt13U 24
#define cpt13L 25
#define cpt14U 26
#define cpt14L 27

#define cptUseE    0
#define cptUseBAR  1
#define cptUseTR   2
#define cptUseRALR 3
#define cptUsePRn  4 // 4 - 11
#define cptUseDSBR 12
#define cptUseFR   13
#define cptUseMR   14
#define cptUseCMR  15
#define cptUseIR   16


    // Address Modification tally
    word12 AM_tally;

#ifndef WAM
    //_ptw PTWAM0;
#else
    _ptw PTWAM [N_WAM_ENTRIES];
#ifdef L68
    word4 PTWAMR;
#endif
#ifdef DPS8M
    word6 PTWAMR;
#endif
#endif
    _ptw * PTW;
    _ptw0 PTW0; // a PTW not in PTWAM (PTWx1)
    _cache_mode_register CMR;
    _mode_register MR;

    // G7 faults

    bool bTroubleFaultCycle;
    uint g7FaultsPreset;
    uint g7Faults;
    //_fault_subtype  g7SubFaultsPreset [N_FAULTS];
    _fault_subtype  g7SubFaults [N_FAULTS];

#ifdef L68
    // FFV faults

     uint FFV_faults_preset;
     uint FFV_faults;
     uint FFV_fault_number;
     bool is_FFV;
#endif

    word24 iefpFinalAddress;
    word36 CY;              // C(Y) operand data from memory
    word36 Ypair[2];        // 2-words
    word36 Yblock8[8];      // 8-words
    word36 Yblock16[16];    // 16-words
    word36 Yblock32[32];    // 32-words
    word36 scu_data[8];    // For SCU instruction
#ifndef EVPOLL
    uint rTRlsb;
#endif
    // XXX this is used to store the fault/interrupt pair, and really should be IBW/IRODD
    word36 instr_buf [2];
    uint64 lufCounter;
    bool secret_addressing_mode;
    bool went_appending; // we will go....
#if 0
    bool bar_attempt;
#endif
    // Map memory to port
    int scbank_map [N_SCBANKS];
    int scbank_pg_os [N_SCBANKS];

    uint history_cyclic [N_HIST_SETS]; // 0..63
    word36 history [N_HIST_SETS] [N_HIST_SIZE] [2];

    // Used by LCPR to prevent the LCPR instruction from being recorded
    // in the CU.
    bool skip_cu_hist;
    // Changes to the mode register history bits do not take affect until
    // the next instruction (ISOLTS 700 2a). Cache the values here so
    // that post register updates can see the old values.
    _mode_register MR_cache;
    
    // If the instruction wants overflow thrown after operand write
    bool dlyFlt;

    // Arguments for delayed overflow fault

    _fault dlyFltNum;
    _fault_subtype dlySubFltNum;
    const char * dlyCtx;

#ifdef ISOLTS
    uint shadowTR;
#endif
    // Set if this thread has set memlock
    bool havelock; // Vetinari 
  } cpu_state_t;

extern cpu_state_t cpus [N_CPU_UNITS_MAX];

extern __thread cpu_state_t * restrict cpup;
extern __thread uint thisCPUnum;
extern __thread uint thisIOMnum;
extern __thread uint thisChnNum;
#define cpu (* cpup)

uint setCPUnum (uint cpuNum);

// Support code to access ARn.BITNO, ARn.CHAR, PRn.BITNO

#define GET_PR_BITNO(n) (cpu.PAR[n].PR_BITNO)
#define GET_AR_BITNO(n) (cpu.PAR[n].AR_BITNO)
#define GET_AR_CHAR(n) (cpu.PAR[n].AR_CHAR)
static inline void SET_PR_BITNO (uint n, word6 b)
  {
     cpu.PAR[n].PR_BITNO = b;
     cpu.PAR[n].AR_BITNO = (b % 9) & MASK4;
     cpu.PAR[n].AR_CHAR = (b / 9) & MASK2;
  }
static inline void SET_AR_CHAR_BITNO (uint n, word2 c, word4 b)
  {
     cpu.PAR[n].PR_BITNO = c * 9 + b;
     cpu.PAR[n].AR_BITNO = b & MASK4;
     cpu.PAR[n].AR_CHAR = c & MASK2;
  }


bool sample_interrupts (void);
t_stat simh_hooks (void);
int OPSIZE (void);
t_stat ReadOP (word18 addr, _processor_cycle_type cyctyp);
t_stat WriteOP (word18 addr, _processor_cycle_type acctyp);

#ifdef PANEL
static inline void trackport (word24 a, word36 d)
  {
    // Simplifying assumption: 4 * 4MW SCUs
    word2 port = (a >> 22) & MASK2;
    cpu.portSelect = port;
    cpu.portAddr [port] = a;
    cpu.portData [port] = d;
    cpu.portBusy = false;
  }
#endif

#ifdef SPEED
static inline int core_read (word24 addr, word36 *data, UNUSED const char * ctx)
  {
    PNL (cpu.portBusy = true;)
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          {
            doFault (FAULT_STR, fst_str_nea, __func__);
          }
        addr = (uint) os + addr % SCBANK;
      }
#endif
#if 0 // XXX Controlled by TEST/NORMAL switch
#ifdef ISOLTS
    if (cpu.MR.sdpap)
      {
        sim_warn ("failing to implement sdpap\n");
        cpu.MR.sdpap = 0;
      }
    if (cpu.MR.separ)
      {
        sim_warn ("failing to implement separ\n");
        cpu.MR.separ = 0;
      }
#endif
#endif
    *data = M[addr] & DMASK;
    PNL (trackport (addr, * data);)
    return 0;
  }

static inline int core_write (word24 addr, word36 data, UNUSED const char * ctx)
  {
    PNL (cpu.portBusy = true;)
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          {
            doFault (FAULT_STR, fst_str_nea, __func__);
          }
        addr = (uint) os + addr % SCBANK;
      }
#endif
#ifdef ISOLTS
    if (cpu.MR.sdpap)
      {
        sim_warn ("failing to implement sdpap\n");
        cpu.MR.sdpap = 0;
      }
    if (cpu.MR.separ)
      {
        sim_warn ("failing to implement separ\n");
        cpu.MR.separ = 0;
      }
#endif
    M[addr] = data & DMASK;
    PNL (trackport (addr, data);)
    return 0;
  }
static inline int core_read2 (word24 addr, word36 *even, word36 *odd, UNUSED const char * ctx)
  {
    PNL (cpu.portBusy = true;)
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          {
            doFault (FAULT_STR, fst_str_nea, __func__);
          }
        addr = (uint) os + addr % SCBANK;
      }
#endif
#if 0 // XXX Controlled by TEST/NORMAL switch
#ifdef ISOLTS
    if (cpu.MR.sdpap)
      {
        sim_warn ("failing to implement sdpap\n");
        cpu.MR.sdpap = 0;
      }
    if (cpu.MR.separ)
      {
        sim_warn ("failing to implement separ\n");
        cpu.MR.separ = 0;
      }
#endif
#endif
    *even = M[addr++] & DMASK;
    PNL (trackport (addr - 1, * even);)
    *odd = M[addr] & DMASK;
    PNL (trackport (addr, * odd);)
    return 0;
  }
static inline int core_write2 (word24 addr, word36 even, word36 odd, UNUSED const char * ctx)
  {
    PNL (cpu.portBusy = true;)
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          {
            doFault (FAULT_STR, fst_str_nea, __func__);
          }
        addr = (uint) os + addr % SCBANK;
      }
#endif
#ifdef ISOLTS
    if (cpu.MR.sdpap)
      {
        sim_warn ("failing to implement sdpap\n");
        cpu.MR.sdpap = 0;
      }
    if (cpu.MR.separ)
      {
        sim_warn ("failing to implement separ\n");
        cpu.MR.separ = 0;
      }
#endif
    M[addr++] = even;
    PNL (trackport (addr - 1, even);)
    M[addr] = odd;
    PNL (trackport (addr, odd);)
    return 0;
  }
#else
int core_read (word24 addr, word36 *data, const char * ctx);
int core_write (word24 addr, word36 data, const char * ctx);
int core_read2 (word24 addr, word36 *even, word36 *odd, const char * ctx);
int core_write2 (word24 addr, word36 even, word36 odd, const char * ctx);
#endif
static inline void core_readN (word24 addr, word36 *data, uint n, UNUSED const char * ctx)
  {
    for (uint i = 0; i < n; i ++)
      core_read (addr + i, data + i, ctx);
  }
static inline void core_writeN (word24 addr, word36 *data, uint n, UNUSED const char * ctx)
  {
    for (uint i = 0; i < n; i ++)
      core_write (addr + i, data [i], ctx);
  }

int is_priv_mode (void);
void set_went_appending (void);
void clr_went_appending (void);
bool get_went_appending (void);
bool get_bar_mode (void);
addr_modes_t get_addr_mode (void);
void set_addr_mode (addr_modes_t mode);
int query_scu_unit_num (int cpu_unit_num, int cpu_port_num);
void init_opcodes (void);
void decodeInstruction (word36 inst, DCDstruct * p);
t_stat dpsCmd_Dump (int32 arg, const char *buf);
t_stat dpsCmd_Init (int32 arg, const char *buf);
t_stat dpsCmd_Segment (int32 arg, const char *buf);
t_stat dpsCmd_Segments (int32 arg, const char *buf);
//t_stat dumpKST (int32 arg, char * buf);
t_stat memWatch (int32 arg, const char * buf);
//_sdw0 *fetchSDW (word15 segno);
_sdw *fetchSDW (word15 segno);
//char *strSDW0 (_sdw0 *SDW);
char *strSDW0 (char * buff, _sdw *SDW);
int query_scbank_map (word24 addr);
void cpu_init (void);
void setup_scbank_map (void);
#ifdef DPS8M
//void addCUhist (word36 flags, word18 opcode, word24 address, word5 proccmd, word7 flags2);
void addCUhist (void);
void addDUOUhist (word36 flags, word18 ICT, word9 RS_REG, word9 flags2);
void addAPUhist (word15 ESN, word21 flags, word24 RMA, word3 RTRR, word9 flags2);
void addEAPUhist (word18 ZCA, word18 opcode);
#endif
#ifdef L68
//void addCUhist (word36 flags, word18 opcode, word18 address, word5 proccmd, word4 sel, word9 flags2);
void addCUhist (void);
// XXX addDUhist
void addOUhist (void);
void addDUhist (void);
void addAPUhist (enum APUH_e op);
#endif
void addHist (uint hset, word36 w0, word36 w1);
uint getCPUnum (void);
void addHistForce (uint hset, word36 w0, word36 w1);
uint getCPUnum (void);
t_stat threadz_sim_instr (void);
void * cpuThreadMain (void * arg);
