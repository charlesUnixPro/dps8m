
// simh only explicitly supports a single cpu

#define N_CPU_UNITS 1
#define CPU_UNIT_NUM 0

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
    ABORT_cycle /* = ABORT_CYCLE */,
    FAULT_cycle /* = FAULT_CYCLE */,
    EXEC_cycle,
    FAULT_EXEC_cycle,
    FAULT_EXEC2_cycle,
    INTERRUPT_cycle,
    INTERRUPT_EXEC_cycle,
    INTERRUPT_EXEC2_cycle,
    FETCH_cycle = INSTRUCTION_FETCH,
    SYNC_FAULT_RTN_cycle,
    // CA FETCH OPSTORE, DIVIDE_EXEC
  } cycles_t;

#ifndef QUIET_UNUSED
// MF fields of EIS multi-word instructions -- 7 bits 

typedef struct
  {
    bool ar;
    bool rl;
    bool id;
    uint reg;  // 4 bits
  } eis_mf_t;
#endif

// [map] designates mapping into 36-bit word from DPS-8 proc manual

extern word36   rA;     // accumulator
extern word36   rQ;     // quotient
extern word8    rE;     // exponent [map: rE, 28 0's]

extern word18   rX [8]; // index
#ifndef REAL_TR
extern word27   rTR;    // timer [map: TR, 9 0's]
#endif
extern word24   rY;     // address operand
extern word8    rTAG;   // instruction tag
extern word8    tTB;    // char size indicator (TB6=6-bit,TB9=9-bit) [3b]
extern word8    tCF;    // character position field [3b]
extern word3    rRALR;  // ring alarm [3b] [map: 33 0's, RALR]
extern word3    RSDWH_R1; // Track the ring number of the last SDW

extern struct _tpr
  {
    word3   TRR; // The current effective ring number
    word15  TSR; // The current effective segment number
    word6   TBR; // The current bit offset as calculated from ITS and ITP 
                 // pointer pairs.
    word18  CA;  // The current computed address relative to the origin of the 
                 // segment whose segment number is in TPR . TSR
  } TPR;

extern struct _ppr
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
  } PPR;

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

extern struct _par
  {
    word15  SNR;    // The segment number of the segment containing the data 
                    //item described by the pointer register.
    word3   RNR;    // The final effective ring number value calculated during
                    // execution of the instruction that last loaded the PR.

    // To get the correct behavior, the ARn . BITNO and CHAR need to be kept in
    // sync. BITNO is the canonical value; access routines for AR [n] . BITNO
    // and . CHAR are provided

    // AL-39 defines the AR format (by implication of the "data as stored by 
    // SARn" as:
    //  18/WORDNO, 2/CHAR, 4/BITNO, 12/0
    //
    // and define the PR register ("odd word of ITS pointer pair") as:
    //  18/WORDNO, 6/BITNO
    //
    // We use this to deduce the BITNO <-> CHAR/BITNO mapping.

    word6   BITNO;  // The number of the bit within PRn . WORDNO that is the 
                    // first bit of the data item. Data items aligned on word 
                    // boundaries always have the value 0. Unaligned data
                    //  items may have any value in the range [1,35].
    word18  WORDNO; // The offset in words from the base or origin of the 
                    // segment to the data item.
  } PAR [8];

// N.B. remember there are subtle differences between AR/PR . BITNO

#define AR    PAR
#define PR    PAR

// Support code to access ARn . BITNO and CHAR

#define GET_AR_BITNO(n) (PAR [n] . BITNO % 9)
#define GET_AR_CHAR(n) (PAR [n] . BITNO / 9)
#define SET_AR_BITNO(n, b) PAR [n] . BITNO = (GET_AR_CHAR [n] * 9 + ((b) & 017))
#define SET_AR_CHAR(n, c) PAR [n] . BITNO = (GET_AR_BITNO [n] + ((c) & 03) * 9)
#define SET_AR_CHAR_BIT(n, c, b) PAR [n] . BITNO = (((c) & 03) * 9 + ((b) & 017))

extern struct _bar
  {
    word9 BASE;     // Contains the 9 high-order bits of an 18-bit address 
                    // relocation constant. The low-order bits are generated 
                    // as zeros.
    word9 BOUND;    // Contains the 9 high-order bits of the unrelocated 
                    // address limit. The low- order bits are generated as 
                    // zeros. An attempt to access main memory beyond this 
                    // limit causes a store fault, out of bounds. A value of 
                    // 0 is truly 0, indicating a null memory range.
  } BAR;

extern struct _dsbr
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
  } DSBR;

// The segment descriptor word (SDW) pair contains information that controls
// the access to a segment. The SDW for segment n is located at offset 2n in
// the descriptor segment whose description is currently loaded into the
// descriptor segment base register (DSBR).

extern struct _sdw
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
    word14  CL;      // Call limiter (entry bound) value. If SDWAM . G is set
                     //  OFF, transfers of control into the segment must be to
                     //  segment addresses no greater than this value.
    word15  POINTER; // The effective segment number used to fetch this SDW
                     //  from main memory.
    word1   F;       // Full/empty bit. If this bit is set ON, the SDW in the
                     //  register is valid. If this bit is set OFF, a hit is
                     //  not possible. All SDWAM . F bits are set OFF by the
                     //  instructions that clear the SDWAM.
    word6   USE;     // Usage count for the register. The SDWAM . USE field is
                     //  used to maintain a strict FIFO queue order among the
                     //  SDWs. When an SDW is matched, its USE value is set to
                     //  15 (newest) on the DPS/L68 and to 63 on the DPS 8M,
                     //  and the queue is reordered. SDWs newly fetched from
                     //  main memory replace the SDW with USE value 0 (oldest)
                     //  and the queue is reordered.
  }
#ifdef SPEED
     SDWAM0, * SDW;
#else
     SDWAM [64], * SDW;
#endif

typedef struct _sdw _sdw;

// in-core SDW (i.e. not cached, or in SDWAM)

extern struct _sdw0
  {
    // even word
    word24  ADDR;    // The 24-bit absolute main memory address of the page
                     //  table for the target segment if SDWAM . U = 0;
                     //  otherwise, the 24-bit absolute main memory address of
                     //  the origin of the target segment.
    word3   R1;      // Upper limit of read/write ring bracket
    word3   R2;      // Upper limit of read/execute ring bracket
    word3   R3;      // Upper limit of call ring bracket
    word1   F;       // Directed fault flag
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
} SDW0;

typedef struct _sdw0 _sdw0;


// PTW as used by APU

extern struct _ptw
 {
    word18  ADDR;    // The 18 high-order bits of the 24-bit absolute
                     //  main memory address of the page.
    word1   M;       // Page modified flag bit. This bit is set ON whenever
                     //  the PTW is used for a store type instruction. When
                     //  the bit changes value from 0 to 1, a special
                     //  extra cycle is generated to write it back into the
                     //  PTW in the page table in main memory.
    word15  POINTER; // The effective segment number used to fetch this PTW
                     //  from main memory.
    word12  PAGENO;  // The 12 high-order bits of the 18-bit computed
                     //  address (TPR . CA) used to fetch this PTW from main
                     //  memory.
    word1   F;       // Full/empty bit. If this bit is set ON, the PTW in
                     //  the register is valid. If this bit is set OFF, a
                     //  hit is not possible. All PTWAM . F bits are set OFF
                     //  by the instructions that clear the PTWAM.
    word6   USE;     // Usage count for the register. The PTWAM . USE field
                     //  is used to maintain a strict FIFO queue order
                     //  among the PTWs. When an PTW is matched its USE 
                     // value is set to 15 (newest) on the DPS/L68 and to
                     //  63 on the DPS 8M, and the queue is reordered.
                     //  PTWs newly fetched from main memory replace the
                     //  PTW with USE value 0 (oldest) and the queue is
                     //  reordered.
    
  }
#ifdef SPEED
     PTWAM0, * PTW;
#else
     PTWAM [64], * PTW;
#endif

typedef struct _ptw _ptw;

// in-core PTW

extern struct _ptw0
  {
    word18  ADDR;   // The 18 high-order bits of the 24-bit absolute main
                    //  memory address of the page.
    word1   U;      // * 1 = page has been used (referenced)
    word1   M;      // * 1 = page has been modified
    word1   F;      // Directed fault flag
                    // * 0 = page not in main memory; execute directed fault FC
                    // * 1 = page is in main memory
    word2   FC;     // Directed fault number for page fault.
    
  } PTW0;

typedef struct _ptw0 _ptw0;

//
// Cache Mode Regsiter
//

struct _cache_mode_register
  {
    word15   cache_dir_address;
    word1    par_bit;
    word1    lev_ful;
    word1    csh1_on;
    word1    csh2_on;
    word1    opnd_on; // DPS8, but not DPS8M
    word1    inst_on;
    word1    csh_reg;
    word1    str_asd;
    word1    col_ful;
    word2    rro_AB;
    word1    bypass_cache;
    word2    luf;       // LUF value
                        // 0   1   2   3
                        // Lockup time
                        // 2ms 4ms 8ms 16ms
                        // The lockup timer is set to 16ms when the 
                        // processor is initialized.
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

extern DEVICE cpu_dev;
extern jmp_buf jmpMain;   // This is where we should return to from a fault to 
                          // retry an instruction

typedef struct MOPstruct MOPstruct;

// address of an EIS operand
typedef struct EISaddr
{
    word18  address;    // 18-bit virtual address
    //word18  lastAddress;  // memory acccesses are not expesive these days - >sheesh<
    
    word36  data;
    word1    bit;
    bool    incr;      // when true increment bit address
    bool    decr;      // when true decrement bit address
    eRW     mode;
    
    // for type of data being address by this object
    
    // eisDataType _type;   // type of data - alphunumeric/numeric
    
    int     TA;   // type of Alphanumeric chars in src
    int     TN;   // type of Numeric chars in src
    int     cPos;
    int     bPos;
    
    // for when using AR/PR register addressing
    word15  SNR;        // The segment number of the segment containing the data item described by the pointer register.
    word3   RNR;        // The effective ring number value calculated during execution of the instruction that last loaded
    
    //bool    bUsesAR;    // true when indirection via AR/PR is involved (TPR.{TRR,TSR} already set up)
    
    MemoryAccessType    mat;    // memory access type for operation

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
    word36 cachedWord;
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

    uint   TA [3];          // type alphanumeric
#define TA1 TA [0]
#define TA2 TA [1]
#define TA3 TA [2]

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
    word9   outBuffer [64]; // output buffer
    word9   *out;           // pointer to current write position in outBuffer;
    
    int     exponent;       // For decimal floating-point (evil)
    int     sign;           // For signed decimal (1, -1)
    
    EISaddr *mopAddress;    // mopAddress, pointer to addr [0], [1], or [2]
    
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
    bool   a;             // bit-29 - address via pointer register. Usually.
    bool   i;             // interrupt inhinit bit.
    word6  tag;           // instruction tag
    
    word18 stiTally;      // for sti instruction
  };

extern DCDstruct currentInstruction;
extern EISstruct currentEISinstruction;

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

extern events_t events;

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
    uint invert_absolute; // If non-zero, invert the sense of the
                          // ABSOLUTE bit in the STI instruction
    uint b29_test;        // If non-zero, enable untested code
    uint dis_enable;      // If non-zero, DIS works
    uint auto_append_disable; // If non-zero, bit29 does not force APPEND_mode
    uint lprp_highonly;   // If non-zero lprp only sets the high bits
    uint steady_clock;    // If non-zero the clock is tied to the cycle counter
    uint degenerate_mode; // If non-zero use the experimental ABSOLUTE mode
    uint append_after;
    uint super_user;
    uint epp_hack;
    uint halt_on_unimp;   // If non-zero, halt CPU on unimplemented instruction
                          // instead of faulting
    uint disable_wam;     // If non-zero, disable PTWAM, STWAM
    uint bullet_time;
    uint disable_kbd_bkpt;
    uint report_faults;   // If set, faults are reported and ignored
    uint tro_enable;   // If set, Timer runout faults are generated.
    uint y2k;
    uint drl_fatal;
    uint trlsb; // Timer Register least significent bits: the number of 
                // instructions that make a timer quantum.
    uint serno;
  } switches_t;

extern switches_t switches;


// More emulator state variables for the cpu
// These probably belong elsewhere, perhaps control unit data or the
// cu-history regs...

typedef struct
  {
    cycles_t cycle;
    uint IC_abs; // translation of odd IC to an absolute address; see
                 // ADDRESS of cu history
    bool irodd_invalid;
                // cached odd instr invalid due to memory write by even instr

    // The following are all from the control unit history register:

    bool trgo;               // most recent instruction caused a transfer?
    bool ic_odd;             // executing odd pair?
    bool poa;                // prepare operand address
    uint opcode;             // currently executing opcode
    struct
      {
        bool fhld; // An access violation or directed fault is waiting.
                   // AL39 mentions that the APU has this flag, but not
                   // where scpr stores it
      } apu_state;

    bool interrupt_flag;     // an interrupt is pending in this cycle
    bool g7_flag;            // a g7 fault is pending in this cycle;
    _fault faultNumber;      // fault number saved by doFault
    _fault_subtype subFault; // saved by doFault

    bool wasXfer;  // The previous instruction was a transfer

    bool wasInhibited; // One or both of the previous instruction 
                       // pair was interrupr inhibited.
  } cpu_state_t;

extern cpu_state_t cpu;

// Control unit data (288 bits) 

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
                   // 19    XSF External segment flag -- not implemented
                   // 20    SDWAMM Match on SDWAM -- not implemented
    word1 SD_ON;   // 21    SDWAM enabled
                   // 22    PTWAMM Match on PTWAM -- not implemented
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
#else
    word12 APUCycleBits;
#endif

                   // 33-35 FCT   Fault counter - counts retries

    /* word 1 */
                   //               AVF Access Violation Fault
                   //               SF  Store Fault
                   //               IPF Illegal Procedure Fault
                   //
    word1 IRO_ISN; //  0    IRO       AVF Illegal Ring Order
                   //       ISN       SF  Illegal segment number
    word1 OEB_IOC; //  1    ORB       AVF Out of execute bracket [sic] should be OEB?
                   //       ICC       IPF Illegal op code
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
    word1 PTWAM_ER;// 11    PTWAM_ER  AVF PTWAM error
    word1 CRT;     // 12    CRT       AVF Cross ring transfer
    word1 RALR;    // 13    RALR      AVF Ring alarm
    word1 SWWAM_ER;// 14    SWWAM_ER  AVF SDWAM error
    word1 OOSB;    // 15    OOSB      AVF Out of segment bounds
    word1 PARU;    // 16    PARU      Parity fault - processor parity upper
    word1 PARL;    // 17    PARL      Parity fault - processor parity lower
    word1 ONC1;    // 18    ONC1      Operation not complete fault error #1
    word1 ONC2;    // 19    ONC2      Operation not complete fault error #2
    word4 IA;      // 20-23 IA        System controll illegal action lines
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
#ifdef ABUSE_CT_HOLD2
    word3 rpdHack;   // 0-2
#endif
#ifdef ABUSE_CT_HOLD
    word1 coFlag;    // 3
    word1 coSize;    // 4
    word3 coOffset;  // 5-7
#endif
                   // 18-21 TSNA     Pointer register number for non-EIS operands or
                   //                EIS Operand #1
                   //                  18-20 PRNO Pointer register number
                   //                  21       PRNO is valid
                   // 22-25 TSNB     Pointer register number for EIS operand #2
                   //                  22-24 PRNO Pointer register number
                   //                  25       PRNO is valid
                   // 26-29 TSNC     Pointer register number for EIS operand #2
                   //                  26-28 PRNO Pointer register number
                   //                  29       PRNO is valid
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
                   // 21    RL  Execute a Repeat Link (rpl) instruction
                   // 22    POT Prepare operand tally
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
    uint CT_HOLD;  // 30-35 CT HOLD contents of the "remember modifier" register

    
    
    /* word 6 */
    word36 IWB;

    /* word 7 */
    word36 IRODD; /* Instr holding register; odd word of last pair fetched */
    
 } ctl_unit_data_t;

extern ctl_unit_data_t cu;

// Control unit data (288 bits) 

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
    word1 JMP;        // 33-35  3   JMP     Descriptor count; number of words 
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

  } du_unit_data_t;

extern du_unit_data_t du;



// XXX when multiple cpus are supported, make the cpu  data structure
// an array and merge the unit state info into here; coding convention
// is the name should be 'cpu' (as is 'iom' and 'scu'); but that name
// is taken. It should probably be merged into here, and then this
// should then be renamed.

#define N_CPU_UNITS_MAX 1

word36 faultRegister [2];


//extern int stop_reason;     // sim_instr return value for JMP_STOP
//void cancel_run (t_stat reason);
bool sample_interrupts (void);
t_stat simh_hooks (void);
int OPSIZE (void);
t_stat ReadOP (word18 addr, _processor_cycle_type cyctyp, bool b29);
t_stat WriteOP (word18 addr, _processor_cycle_type acctyp, bool b29);

#ifdef SPEED
static inline int core_read (word24 addr, word36 *data, UNUSED const char * ctx)
  {
    *data = M[addr] & DMASK;
    return 0;
  }
static inline int core_write (word24 addr, word36 data, UNUSED const char * ctx)
  {
    M[addr] = data & DMASK;
    return 0;
  }
static inline int core_read2 (word24 addr, word36 *even, word36 *odd, UNUSED const char * ctx)
  {
    *even = M[addr++] & DMASK;
    *odd = M[addr] & DMASK;
    return 0;
  }
static inline int core_write2 (word24 addr, word36 even, word36 odd, UNUSED const char * ctx)
  {
    M[addr++] = even;
    M[addr] = odd;
    return 0;
  }
#else
int core_read (word24 addr, word36 *data, const char * ctx);
int core_write (word24 addr, word36 data, const char * ctx);
int core_read2 (word24 addr, word36 *even, word36 *odd, const char * ctx);
int core_write2 (word24 addr, word36 even, word36 odd, const char * ctx);
int core_readN (word24 addr, word36 *data, int n, const char * ctx);
int core_writeN (word24 addr, word36 *data, int n, const char * ctx);
int core_read72 (word24 addr, word72 *dst, const char * ctx);
#endif

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
t_stat dpsCmd_Dump (int32 arg, char *buf);
t_stat dpsCmd_Init (int32 arg, char *buf);
t_stat dpsCmd_Segment (int32 arg, char *buf);
t_stat dpsCmd_Segments (int32 arg, char *buf);
t_stat dumpKST (int32 arg, char * buf);
t_stat memWatch (int32 arg, char * buf);
_sdw0 *fetchSDW (word15 segno);
char *strSDW0 (_sdw0 *SDW);
int query_scbank_map (word24 addr);
void cpu_init (void);
void setup_scbank_map (void);
