#ifndef DPS8_HW_CONSTS_H
#define DPS8_HW_CONSTS_H

//
// Memory
//


#define PASIZE          24                              /*!< phys addr width */
#define PAMASK          ((1U << PASIZE) - 1U)

#define MAXMEMSIZE      (1U << PASIZE)                  /*!< maximum memory */
#define INIMEMSIZE      MAXMEMSIZE                      /*!< 2**18 */
#define MEMSIZE         INIMEMSIZE                      /*!< fixed, KISS */

// The minimum allocation size of a SCU is 64K (2^16) 
// (2 banks of 32K). Call it an SCPAGE
#define SCPAGE (1U << 16)

// Maximum memory size is MAXMEMSIZE, number of
// scpages is:
#define N_SCPAGES ((MAXMEMSIZE) / (SCPAGE))


//
// Memory addressing
//


#define VASIZE          18                              /*!< virtual addr width */
#define AMASK           ((1U << VASIZE) - 1U)             /*!< virtual addr mask */
#define SEGSIZE         (1U << VASIZE)                   ///< size of segment in words


//
// Words
//


#define MAX18           0777777U
#define MAX18POS        0377777U                           /*!<  2**17-1 */
#define MAX18NEG        0400000U                           /*!< -2**17 */  
#define SIGN18          0400000U
#define MASK36          0777777777777LLU                   /*!< data mask */
#define DMASK           MASK36
#define MASK18          0777777U                     ///< 18-bit data mask
#define WMASK           MASK18                             // WORDNO mask
#define MASKLO18        0000000777777LLU
#define MASKHI18        0777777000000LLU
#define SIGN36          0400000000000LLU                   /*!< sign bit of a 36-bit word */
//#define SIGN            SIGN36
#define SIGNEX          0100000000000LLU                   /*!< extended sign helper for mpf/mpy */
#define MASK15          077777U
#define SMASK           MASK15                             // Segment number mask
#define SIGN15          040000U                            ///< sign mask 15-bit number
#define SIGNMASK15      0xffff8000U                        ///< mask to sign exterd a 15-bit number to a 32-bit integer
#define MAGMASK         0377777777777LLU                   /*!< magnitude mask */
#define ONES            0777777777777LLU
#define NEG136          0777777777777LLU                   ///< -1
#define MAXPOS          0377777777777LLU                   ///<  2**35-1
#define MAXNEG          0400000000000LLU                   ///< -2**35
#define MAX36           0777777777777LLU                   ///< 2**36
#define MAX72           (((word72)1U << 72) - 1U)           ///< 72 1's

#define CARRY          01000000000000LLU                   ///< carry from 2 36-bit additions/subs
#define SIGNEXT    0xfffffff000000000LLU        ///< mask to sign extend a 36 => 64-bit negative
#define ZEROEXT         0777777777777LLU        ///< mask to zero extend a 36 => 64-bit int
#define SIGNMASK18      037777000000U        ///< mask to sign extend a 18 => 32-bit negative
#define ZEROEXT18       0777777U                ///< mask to zero extend a 18 => 32-bit int
#define ZEROEXT72       (((word72)1U << 72) - 1U)  ///< mask to zero extend a 72 => 128 int
#define SIGN72           ((word72)1U << 71)
#define MASK72          ZEROEXT72

#define SIGN64         ((uint64)1U << 63)

#define MASK3           03U
#define MASK6           077U

#define SIGN8          0400U         //sign mask 8-bit number
#define SIGNMASK8      0xffffff40U                        ///< mask to sign extend a 8-bit number to a 32-bit integer
#define SIGNEXT8(x)    (((x) & SIGN8) ? ((x) | SIGNMASK8) : ((x) & ~SIGNMASK8))  ///< sign extend a 8-bit word to 18/32-bit word
#define MASK8           0377U                              ///< 8-bit mask

#define SIGN12          0x800U                             ///< sign mask 12-bit number
#define SIGNMASK12      0xfffff800U                        ///< mask to sign extend a 12-bit number to a 32-bit integer
#define SIGNEXT12(x)    (((x) & SIGN12) ? ((x) | SIGNMASK12) : ((x) & ~SIGNMASK12))  ///< sign extend a 12-bit word to 18/32-bit word
#define MASK12          07777U

#define SIGNEXT18(x)    (((x) & SIGN18) ? ((x) | SIGNMASK18) : ((x) & ~SIGNMASK18)) ///< sign extend a 18-bit word to 64-bit
#define SIGNEXT36(x)    (((x) & SIGN36) ? ((x) |    SIGNEXT) : ((x) & ~SIGNEXT)) ///< sign extend a 36-bit word to 64-bit
#define SIGNEXT15(x)    (((x) & SIGN15) ? ((x) | SIGNMASK15) : ((x) & ~SIGNMASK15)) ///< sign extend a 15-bit word to 18/32-bit word

#define SIGN6           0040U     ///< sign bit of 6-bit signed numfer (e.g. Scaling Factor)
#define SIGNMASK6       0340U     ///< sign mask for 6-bit number
#define SIGNEXT6(x)     (int8)(((x) & SIGN6) ? ((x) | SIGNMASK6) : (x)) ///< sign extend a 6-bit word to 8-bit char

#define MASKBITS(x) ( ~(~((t_uint64)0)<<x) ) // lower (x) bits all ones

#define GETHI36(a)      ((word18) ((a >> 18) & MASK18))
#define GETLO36(a)      ((word18) (a & MASK18))
#define SETHI36(a,b)	(((a) &= MASKLO18), ((a) |= ((((word36)(b) & MASKLO18) << 18))))
#define SETLO36(a,b)    (((a) &= MASKHI18), ((a) |= ((word36)(b) & MASKLO18)))
#define GETHI(a)        GETHI36(a)
#define GETLO(a)        GETLO36(a)
#define SETHI(a,b)      SETHI36((a),(b))
#define SETLO(a,b)      SETLO36((a),(b))

#define GETHI72(a)      ((word36) ((a >> 36) & MASK36))
#define GETLO72(a)      ((word36) (a & MASK36))
#define SETHI72(a,b)	(a &= MASK36, a |= ((((word72)(b) & MASK36)) << 36))
#define SETLO72(a,b)	(a &= MASK36 << 36, a |= ((word72)(b) & MASK36))



// the 2 following may need some work........
#define EXTMSK72        (((word72)-1) - ZEROEXT72)
#define SIGNEXT72(x)    (((x) & SIGN72) ? ((x) | EXTMSK72) : ((x) & ~EXTMSK72)) ///< sign extend a 72-bit word to 128-bit

#define SETS36(x)         ((x) | SIGN36)
#define CLRS36(x)         ((x) & ~SIGN36)
#define TSTS36(x)         ((x) & SIGN36)

//
// Instruction format
//

#define INST_V_TAG      0                              ///< Tag
#define INST_M_TAG      077U
#define INST_V_A        6                              ///< Indirect via pointer
#define INST_M_A        1U
#define INST_V_I        7                              ///< Interrupt Inhibit
#define INST_M_I        1U
#define INST_V_OP       9                              /*!< opcode */
#define INST_M_OP       0777U
#define INST_V_OPX      8                              /*!< opcode etension */
#define INST_M_OPX      1U

#define INST_V_ADDR     18                              ///< Address
#define INST_M_ADDR     0777777U
#define INST_V_OFFSET   18                              ///< Offset (Bit29=1)
#define INST_M_OFFSET   077777U
#define INST_V_PRN      33                              ///< n of PR[n] (Bit29=1)
#define INST_M_PRN      07U


#define GET_TAG(x)      ((int32) ( (x)              & INST_M_TAG ))
#define GET_A(x)        ((int32) (((x) >> INST_V_A) & INST_M_A   ))
#define GET_I(x)        ((int32) (((x) >> INST_V_I) & INST_M_I   ))
#define GET_OP(x)       ((int32) (((x) >> INST_V_OP) & INST_M_OP ))
#define GET_OPX(x)      ((bool) (((x) >> INST_V_OPX) & INST_M_OPX))

#define GET_OFFSET(x)   ((word15) (((x) >> INST_V_OFFSET) & INST_M_OFFSET))
#define GET_PRN(x)      ((word3) (((x) >> INST_V_PRN) & INST_M_PRN))

#define GET_TM(x)       ((int32)(GET_TAG(x) & 060U))
#define GET_TD(x)       ((int32)(GET_TAG(x) & 017U))

//#define GET_ADDR(x)     ((a8) ((x) & AMASK))
#define GET_ADDR(x)     ((uint32) (((x) >> INST_V_ADDR) & INST_M_ADDR))

// tag defines ...
#define TAG_R       0U     ///< The contents of the register specified in C(Td) are added to the current computed address,
#define TAG_RI      1U     /*!< The contents of the register specified in C(Td) are added to the current computed address, C(TPR.CA), to form the modified computed address as for
register modification. The modified C(TPR.CA) is then used to fetch an indirect word. The TAG field of the indirect word specifies the next step in computed address formation. The use of du or dl as the designator in this modification type will cause an illegal procedure, illegal modifier, fault. */
#define TAG_IT      2U /*!< The indirect word at C(TPR.CA) is fetched and the modification performed according to the variation specified in C(Td) of the instruction word and the contents of the indirect word. This modification type allows automatic incrementing and decrementing of addresses and tally counting. */
#define TAG_IR      3U


#define _TD(tag) ((tag) & 017U)
#define _TM(tag) ((tag) & 060U)

enum {
    TD_N	= 000U,
    TD_AU	= 001U,
    TD_QU	= 002U,
    TD_DU	= 003U,
    TD_IC	= 004U,
    TD_AL	= 005U,
    TD_QL	= 006U,
    TD_DL	= 007U,
    TD_X0	= 010U,
    TD_X1	= 011U,
    TD_X2	= 012U,
    TD_X3	= 013U,
    TD_X4	= 014U,
    TD_X5	= 015U,
    TD_X6	= 016U,
    TD_X7	= 017U
};

enum {
    TM_R	= 000U,
    TM_RI	= 020U,
    TM_IT	= 040U,  // HWR - next 2 had values swapped
    TM_IR	= 060U
};

/*! see AL39, pp 6-13, tbl 6-3 */
enum {
    IT_F1	= 000U,
    IT_SD	= 004U,
    IT_SCR	= 005U,
    IT_F2	= 006U,
    IT_F3	= 007U,
    IT_CI	= 010U,
    IT_I	= 011U,
    IT_SC	= 012U,
    IT_AD	= 013U,
    IT_DI	= 014U,
    IT_DIC	= 015U,
    IT_ID	= 016U,
    IT_IDC	= 017U,
    
    // not really IT, but they're in it's namespace
    SPEC_ITP  = 001U,
    SPEC_ITS  = 003U
};

#define GET_TB(tag) ((tag) & 040U)
#define GET_CF(tag) ((tag) & 007U)

#define _TB(tag) GET_TB(tag)
#define _CF(tag) GET_CF(tag)

#define TB6	000U ///< 6-bit characters
#define TB9	040U ///< 9-bit characters

//
// ITS/ITP
//

#define ISITP(x)                (((x) & INST_M_TAG) == 041U)
#define GET_ITP_PRNUM(Ypair)    ((word3)((Ypair[0] >> 33) & 07U))
#define GET_ITP_WORDNO(Ypair)   ((word18)((Ypair[1] >> 18) & WMASK))
#define GET_ITP_BITNO(Ypair)    ((word3)((Ypair[1] >> 9) & 077U))
#define GET_ITP_MOD(Ypair)      (GET_TAG(Ypair[1]))

#define ISITS(x)                (((x) & INST_M_TAG) == 043U)
#define GET_ITS_SEGNO(Ypair)    ((word15)((Ypair[0] >> 18) & SMASK))
#define GET_ITS_RN(Ypair)       ((word3)((Ypair[0] >> 15) & 07))
#define GET_ITS_WORDNO(Ypair)   ((word18)((Ypair[1] >> 18) & WMASK))
#define GET_ITS_BITNO(Ypair)    ((word3)((Ypair[1] >> 9) & 077))
#define GET_ITS_MOD(Ypair)      (GET_TAG(Ypair[1]))

//
// Indicator register bits
//

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

#define F_A             (1LLU << F_V_A)
#define F_B             (1LLU << F_V_B)
#define F_C             (1LLU << F_V_C)
#define F_D             (1LLU << F_V_D)
#define F_E             (1LLU << F_V_E)
#define F_F             (1LLU << F_V_F)
#define F_G             (1LLU << F_V_G)
#define F_H             (1LLU << F_V_H)
#define F_I             (1LLU << F_V_I)
#define F_J             (1LLU << F_V_J)
#define F_K             (1LLU << F_V_K)
#define F_L             (1LLU << F_V_L)
#define F_M             (1LLU << F_V_M)
#define F_N             (1LLU << F_V_N)
#define F_O             (1LLU << F_V_O)

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

//
//  floating-point constants
//

#define FLOAT36MASK     01777777777LLU               ///< user to extract mantissa from single precision C(CEAQ)
#define FLOAT72MASK     01777777777777777777777LLU   ///< use to extract mastissa from double precision C(EAQ)
#define FLOAT72SIGN     (1LLU << 63)                 ///< mantissa sign mask for full precision C(EAQ)
// XXX beware the 72's are not what they seem!


//
// Faults
//

#define N_FAULT_GROUPS 7
#if 0
// group6 faults generated by APU

#define CUACV0    35U   ///< 15.Illegal ring order (ACV0=IRO)
#define CUACV1    34U   ///< 3. Not in execute bracket (ACV1=OEB)
#define CUACV2    33U   ///< 6. No execute permission (ACV2=E-OFF)
#define CUACV3    32U   ///< 1. Not in read bracket (ACV3=ORB)
#define CUACV4    31U   ///< 4. No read permission (ACV4=R-OFF)
#define CUACV5    30U   ///< 2. Not in write bracket (ACV5=OWB)
#define CUACV6    29U   ///< 5. No write permission (ACV6=W-OFF)
#define CUACV7    28U   ///< 8. Call limiter fault (ACV7=NO GA)
#define CUACV8    27U   ///< 16.Out of call brackets (ACV8=OCB)
#define CUACV9    26U   ///< 9. Outward call (ACV9=OCALL)
#define CUACV10   25U   ///< 10.Bad outward call (ACV10=BOC)
#define CUACV11   24U   ///< 11.Inward return (ACV11=INRET) XXX ??
#define CUACV12   23U   ///< 7. Invalid ring crossing (ACV12=CRT)
#define CUACV13   22U   ///< 12.Ring alarm (ACV13=RALR)
#define CUAME     21U   ///< 13.Associative memory error XXX ??
#define CUACV15   20U   ///< 14.Out of segment bounds (ACV15=OOSB)

#endif
/*!
 * Access violation faults ...
 */
//enum enumACV {
//    ACV0 = (1U << 0),   ///< 15.Illegal ring order (ACV0=IRO)
//    ACV1 = (1U << 1),   ///< 3. Not in execute bracket (ACV1=OEB)
//    ACV2 = (1U << 2),   ///< 6. No execute permission (ACV2=E-OFF)
//    ACV3 = (1U << 3),   ///< 1. Not in read bracket (ACV3=ORB)
//    ACV4 = (1U << 4),   ///< 4. No read permission (ACV4=R-OFF)
//    ACV5 = (1U << 5),   ///< 2. Not in write bracket (ACV5=OWB)
//    ACV6 = (1U << 6),   ///< 5. No write permission (ACV6=W-OFF)
//    ACV7 = (1UU << 7),   ///< 8. Call limiter fault (ACV7=NO GA)
//    ACV8 = (1U << 8),   ///< 16.Out of call brackets (ACV8=OCB)
//    ACV9 = (1U << 9),   ///< 9. Outward call (ACV9=OCALL)
//    ACV10= (1U << 10),   ///< 10.Bad outward call (ACV10=BOC)
//    ACV11= (1U << 11),   ///< 11.Inward return (ACV11=INRET) XXX ??
//    ACV12= (1U << 12),   ///< 7. Invalid ring crossing (ACV12=CRT)
//    ACV13= (1U << 13),   ///< 12.Ring alarm (ACV13=RALR)
//    AME  = (1U << 14), ///< 13.Associative memory error XXX ??
//    ACV15 =(1U << 15), ///< 14.Out of segment bounds (ACV15=OOSB)
//    
//    ACDF0  = ( 1U << 16), ///< directed fault 0
//    ACDF1  = ( 1U << 17), ///< directed fault 1
//    ACDF2  = ( 1U << 18), ///< directed fault 2
//    ACDF3  = ( 1U << 19), ///< directed fault 3
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


#define FAULT_SDF    0U ///< shutdown fault
#define FAULT_STR    1U ///< store fault
#define FAULT_MME    2U ///< master mode entry
#define FAULT_F1     3U ///< fault tag 1
#define FAULT_TRO    4U ///< timer runout fault
#define FAULT_CMD    5U ///< command
#define FAULT_DRL    6U ///< derail
#define FAULT_LUF    7U ///< lockup
#define FAULT_CON    8U ///< connect
#define FAULT_PAR    9U ///< parity
#define FAULT_IPR   10U ///< illegal proceedure
#define FAULT_ONC   11U ///< operation not complete
#define FAULT_SUF   12U ///< startup
#define FAULT_OFL   13U ///< overflow
#define FAULT_DIV   14U ///< divide check
#define FAULT_EXF   15U ///< execute
#define FAULT_DF0   16U ///< directed fault 0
#define FAULT_DF1   17U ///< directed fault 1
#define FAULT_DF2   18U ///< directed fault 2
#define FAULT_DF3   19U ///< directed fault 3
#define FAULT_ACV   20U ///< access violation
#define FAULT_MME2  21U ///< Master mode entry 2
#define FAULT_MME3  22U ///< Master mode entry 3
#define FAULT_MME4  23U ///< Master mode entry 4
#define FAULT_F2    24U ///< fault tag 2
#define FAULT_F3    25U ///< fault tag 3
#define FAULT_UN1   26U ///< unassigned
#define FAULT_UN2   27U ///< unassigned
#define FAULT_UN3   28U ///< unassigned
#define FAULT_UN4   29U ///< unassigned
#define FAULT_UN5   30U ///< unassigned
#define FAULT_TRB   31U ///< Trouble

#define FAULTBASE_MASK  07740U       ///< mask off all but top 7 msb


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
    ACV0 = (1U << 0),   ///< 15.Illegal ring order (ACV0=IRO)
    ACV1 = (1U << 1),   ///< 3. Not in execute bracket (ACV1=OEB)
    ACV2 = (1U << 2),   ///< 6. No execute permission (ACV2=E-OFF)
    ACV3 = (1U << 3),   ///< 1. Not in read bracket (ACV3=ORB)
    ACV4 = (1U << 4),   ///< 4. No read permission (ACV4=R-OFF)
    ACV5 = (1U << 5),   ///< 2. Not in write bracket (ACV5=OWB)
    ACV6 = (1U << 6),   ///< 5. No write permission (ACV6=W-OFF)
    ACV7 = (1U << 7),   ///< 8. Call limiter fault (ACV7=NO GA)
    ACV8 = (1U << 8),   ///< 16.Out of call brackets (ACV8=OCB)
    ACV9 = (1U << 9),   ///< 9. Outward call (ACV9=OCALL)
    ACV10= (1U << 10),   ///< 10.Bad outward call (ACV10=BOC)
    ACV11= (1U << 11),   ///< 11.Inward return (ACV11=INRET) XXX ??
    ACV12= (1U << 12),   ///< 7. Invalid ring crossing (ACV12=CRT)
    ACV13= (1U << 13),   ///< 12.Ring alarm (ACV13=RALR)
    AME  = (1U << 14), ///< 13.Associative memory error XXX ??
    ACV15 =(1U << 15), ///< 14.Out of segment bounds (ACV15=OOSB)
    
    ACDF0  = ( 1U << 16), ///< directed fault 0
    ACDF1  = ( 1U << 17), ///< directed fault 1
    ACDF2  = ( 1U << 18), ///< directed fault 2
    ACDF3  = ( 1U << 19), ///< directed fault 3
    
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


//
// Interrupts
//

#define N_INTERRUPTS 32

//
// Memory map
//

#define IOM_MBX_LOW 01200
#define IOM_MBX_LEN 02200
#define DN355_MBX_LOW 03400
#define DN355_MBX_LEN 03000


#endif // DPS8_HW_CONSTS_H
