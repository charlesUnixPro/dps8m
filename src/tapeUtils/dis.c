/*
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "bit36.h"

typedef uint64_t word36;
typedef int32_t int32;
typedef uint32_t uint32;
typedef uint64_t t_uint64;

typedef uint32          a8;                           ///< DSP8 addr (18/24b)
typedef t_uint64        d8;                           ///< DSP8 data (36b)

//! Basic + EIS opcodes .....
struct opCode {
    const char *mne;    ///< mnemonic
    int32 flags;        ///< various and sundry flags
    int32 mods;         ///< disallowed addr mods
    int32 ndes;         ///< number of operand descriptor words for instruction (mw EIS)
    int32 opcode;       ///< opcode # (if needed)
};
typedef struct opCode opCode;


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

#define GET_ADDR(x)     ((int32) (((x) >> INST_V_ADDR) & INST_M_ADDR))

/* some MW EIS macros... */
#define GET_MF1(x)      ((x) & 0177ULL) // rightmost 7 bits
#define GET_MF2(x)      GET_MF1(((x) >> 18))    
#define GET_MF3(x)      GET_MF1(((x) >> 28))



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
    EOP3_MASK       = (3U << 25),
#define EOP3_SHIFT 25
    
    READ_YBLOCK32   = (1U << 26),  // fetches/reads Y-block16 operands from memory
    STORE_YBLOCK32  = (1U << 27),  // fetches/reads Y-block16 operands from memory
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


#define _EIS_ NO_TAG | NO_XED | NO_RPT | IGN_B29

struct opCode NonEISopcodes[01000] = {
    /* 000 */
    {NULL, 0, 0, 0},
    {"mme", NO_RPT, 0, 0},// XXX "MODIFICATIONS: All, but none affect instruction execution."
    {"drl", NO_RPT, 0, 0},// XXX "MODIFICATIONS: All, but none affect instruction execution."
    {NULL, 0, 0, 0},
    {"mme2", NO_BAR | NO_RPT, 0, 0},// XXX "MODIFICATIONS: All, but none affect instruction execution."
    {"mme3", NO_BAR | NO_RPT, 0, 0},// XXX "MODIFICATIONS: All, but none affect instruction execution."
    {NULL, 0, 0, 0},
    {"mme4", NO_BAR | NO_RPT, 0, 0},// XXX "MODIFICATIONS: All, but none affect instruction execution."
    {NULL, 0, 0, 0},
    {"nop", PREPARE_CA | NO_RPT, 0, 0},
    {"puls1", PREPARE_CA | NO_RPT, 0, 0},
    {"puls2", PREPARE_CA | NO_RPT, 0, 0},
    {NULL, 0, 0, 0},
    {"cioc", READ_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"adlx0", READ_OPERAND, NO_CSS, 0},
    {"adlx1", READ_OPERAND, NO_CSS, 0},
    {"adlx2", READ_OPERAND, NO_CSS, 0},
    {"adlx3", READ_OPERAND, NO_CSS, 0},
    {"adlx4", READ_OPERAND, NO_CSS, 0},
    {"adlx5", READ_OPERAND, NO_CSS, 0},
    {"adlx6", READ_OPERAND, NO_CSS, 0},
    {"adlx7", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"ldqc", RMW, NO_DDCSS, 0},
    {"adl", READ_OPERAND, NO_CSS, 0},
    {"ldac", RMW, NO_DDCSS, 0},
    {"adla", READ_OPERAND, 0, 0},
    {"adlq", READ_OPERAND, 0, 0},
    {"adlaq", READ_YPAIR, NO_DDCSS, 0},
    {"asx0", RMW | NO_RPL, NO_DDCSS, 0},
    {"asx1", RMW | NO_RPL, NO_DDCSS, 0},
    {"asx2", RMW | NO_RPL, NO_DDCSS, 0},
    {"asx3", RMW | NO_RPL, NO_DDCSS, 0},
    {"asx4", RMW | NO_RPL, NO_DDCSS, 0},
    {"asx5", RMW | NO_RPL, NO_DDCSS, 0},
    {"asx6", RMW | NO_RPL, NO_DDCSS, 0},
    {"asx7", RMW | NO_RPL, NO_DDCSS, 0},
    {"adwp0", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"adwp1", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"adwp2", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"adwp3", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"aos", RMW | NO_RPL, NO_DDCSS, 0},
    {"asa", RMW | NO_RPL, NO_DDCSS, 0},
    {"asq", RMW | NO_RPL, NO_DDCSS, 0},
    {"sscr", PREPARE_CA | PRIV_INS, NO_DDCSS, 0},
    {"adx0", READ_OPERAND, NO_CSS, 0},
    {"adx1", READ_OPERAND, NO_CSS, 0},
    {"adx2", READ_OPERAND, NO_CSS, 0},
    {"adx3", READ_OPERAND, NO_CSS, 0},
    {"adx4", READ_OPERAND, NO_CSS, 0},
    {"adx5", READ_OPERAND, NO_CSS, 0},
    {"adx6", READ_OPERAND, NO_CSS, 0},
    {"adx7", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"awca", READ_OPERAND, 0, 0},
    {"awcq", READ_OPERAND, 0, 0},
    {"lreg", READ_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"ada", READ_OPERAND, 0, 0},
    {"adq", READ_OPERAND, 0, 0},
    {"adaq", READ_YPAIR, NO_DDCSS, 0},
    
    /* 100 */
    {"cmpx0", READ_OPERAND, NO_CSS, 0},
    {"cmpx1", READ_OPERAND, NO_CSS, 0},
    {"cmpx2", READ_OPERAND, NO_CSS, 0},
    {"cmpx3", READ_OPERAND, NO_CSS, 0},
    {"cmpx4", READ_OPERAND, NO_CSS, 0},
    {"cmpx5", READ_OPERAND, NO_CSS, 0},
    {"cmpx6", READ_OPERAND, NO_CSS, 0},
    {"cmpx7", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"cwl", READ_OPERAND, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"cmpa", READ_OPERAND, 0, 0},
    {"cmpq", READ_OPERAND, 0, 0},
    {"cmpaq", READ_YPAIR, NO_DDCSS, 0},
    {"sblx0", READ_OPERAND, NO_CSS, 0},
    {"sblx1", READ_OPERAND, NO_CSS, 0},
    {"sblx2", READ_OPERAND, NO_CSS, 0},
    {"sblx3", READ_OPERAND, NO_CSS, 0},
    {"sblx4", READ_OPERAND, NO_CSS, 0},
    {"sblx5", READ_OPERAND, NO_CSS, 0},
    {"sblx6", READ_OPERAND, NO_CSS, 0},
    {"sblx7", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"sbla", READ_OPERAND, 0, 0},
    {"sblq", READ_OPERAND, 0, 0},
    {"sblaq", READ_YPAIR, NO_DDCSS, 0},
    {"ssx0", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssx1", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssx2", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssx3", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssx4", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssx5", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssx6", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssx7", RMW | NO_RPL, NO_DDCSS, 0},
    {"adwp4", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"adwp5", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"adwp6", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"adwp7", READ_OPERAND | NO_BAR | NO_RPT, NO_DLCSS, 0},
    {"sdbr", STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"ssa", RMW | NO_RPL, NO_DDCSS, 0},
    {"ssq", RMW | NO_RPL, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"sbx0", READ_OPERAND, NO_CSS, 0},
    {"sbx1", READ_OPERAND, NO_CSS, 0},
    {"sbx2", READ_OPERAND, NO_CSS, 0},
    {"sbx3", READ_OPERAND, NO_CSS, 0},
    {"sbx4", READ_OPERAND, NO_CSS, 0},
    {"sbx5", READ_OPERAND, NO_CSS, 0},
    {"sbx6", READ_OPERAND, NO_CSS, 0},
    {"sbx7", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"swca", READ_OPERAND, 0, 0},
    {"swcq", READ_OPERAND, 0, 0},
    {"lpri", READ_YBLOCK16 | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"sba", READ_OPERAND, 0, 0},
    {"sbq", READ_OPERAND, 0, 0},
    {"sbaq", READ_YPAIR, NO_DDCSS, 0},
    
    /* 200 */
    {"cnax0", READ_OPERAND, NO_CSS, 0},
    {"cnax1", READ_OPERAND, NO_CSS, 0},
    {"cnax2", READ_OPERAND, NO_CSS, 0},
    {"cnax3", READ_OPERAND, NO_CSS, 0},
    {"cnax4", READ_OPERAND, NO_CSS, 0},
    {"cnax5", READ_OPERAND, NO_CSS, 0},
    {"cnax6", READ_OPERAND, NO_CSS, 0},
    {"cnax7", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"cmk", READ_OPERAND, 0, 0},
    // XXX AL-39 seems wrong w.r.t absa; it makes no sense as privileged.
    {"absa", PREPARE_CA /*| PRIV_INS*/ | NO_RPT, NO_DDCSS, 0},
    {"epaq", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sznc", RMW, NO_DDCSS, 0},
    {"cnaa", READ_OPERAND, 0, 0},
    {"cnaq", READ_OPERAND, 0, 0},
    {"cnaaq", READ_YPAIR, NO_DDCSS, 0},
    {"ldx0", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"ldx1", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"ldx2", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"ldx3", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"ldx4", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"ldx5", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"ldx6", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"ldx7", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lbar", READ_OPERAND | NO_RPT | NO_BAR, NO_CSS, 0},
    {"rsw", PREPARE_CA | PRIV_INS | NO_RPT, 0, 0},
    {"ldbr", READ_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"rmcm", PRIV_INS, NO_DDCSS, 0},
    {"szn", READ_OPERAND, 0, 0},
    {"lda", READ_OPERAND, 0, 0},
    {"ldq", READ_OPERAND, 0, 0},
    {"ldaq", READ_YPAIR, NO_DDCSS, 0},
    {"orsx0", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsx1", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsx2", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsx3", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsx4", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsx5", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsx6", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsx7", RMW | NO_RPL, NO_DDCSS, 0},
    {"spri0", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spbp1", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spri2", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spbp3", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spri", STORE_YBLOCK16 | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"orsa", RMW | NO_RPL, NO_DDCSS, 0},
    {"orsq", RMW | NO_RPL, NO_DDCSS, 0},
    {"lsdp", READ_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},    // not available on a dps8m
    {"orx0", READ_OPERAND, NO_CSS, 0},
    {"orx1", READ_OPERAND, NO_CSS, 0},
    {"orx2", READ_OPERAND, NO_CSS, 0},
    {"orx3", READ_OPERAND, NO_CSS, 0},
    {"orx4", READ_OPERAND, NO_CSS, 0},
    {"orx5", READ_OPERAND, NO_CSS, 0},
    {"orx6", READ_OPERAND, NO_CSS, 0},
    {"orx7", READ_OPERAND, NO_CSS, 0},
    {"tsp0", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp1", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp2", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp3", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"ora", READ_OPERAND, 0, 0},
    {"orq", READ_OPERAND, 0, 0},
    {"oraq", READ_YPAIR, NO_DDCSS, 0},
    
    /* 300 */
    {"canx0", READ_OPERAND, NO_CSS, 0},
    {"canx1", READ_OPERAND, NO_CSS, 0},
    {"canx2", READ_OPERAND, NO_CSS, 0},
    {"canx3", READ_OPERAND, NO_CSS, 0},
    {"canx4", READ_OPERAND, NO_CSS, 0},
    {"canx5", READ_OPERAND, NO_CSS, 0},
    {"canx6", READ_OPERAND, NO_CSS, 0},
    {"canx7", READ_OPERAND, NO_CSS, 0},
    {"eawp0", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"easp0", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"eawp2", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"easp2", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"cana", READ_OPERAND, 0, 0},
    {"canq", READ_OPERAND, 0, 0},
    {"canaq", READ_YPAIR, NO_DDCSS, 0},
    {"lcx0", READ_OPERAND, NO_CSS, 0},
    {"lcx1", READ_OPERAND, NO_CSS, 0},
    {"lcx2", READ_OPERAND, NO_CSS, 0},
    {"lcx3", READ_OPERAND, NO_CSS, 0},
    {"lcx4", READ_OPERAND, NO_CSS, 0},
    {"lcx5", READ_OPERAND, NO_CSS, 0},
    {"lcx6", READ_OPERAND, NO_CSS, 0},
    {"lcx7", READ_OPERAND, NO_CSS, 0},
    {"eawp4", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"easp4", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"eawp6", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"easp6", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"lca", READ_OPERAND, 0, 0},
    {"lcq", READ_OPERAND, 0, 0},
    {"lcaq", READ_YPAIR, NO_DDCSS, 0},
    {"ansx0", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansx1", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansx2", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansx3", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansx4", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansx5", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansx6", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansx7", RMW | NO_RPL, NO_DDCSS, 0},
    {"epp0", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp1", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp2", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp3", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"stac", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansa", RMW | NO_RPL, NO_DDCSS, 0},
    {"ansq", RMW | NO_RPL, NO_DDCSS, 0},
    {"stcd", STORE_YPAIR | NO_RPT, NO_DDCSS, 0},
    {"anx0", READ_OPERAND, NO_CSS, 0},
    {"anx1", READ_OPERAND, NO_CSS, 0},
    {"anx2", READ_OPERAND, NO_CSS, 0},
    {"anx3", READ_OPERAND, NO_CSS, 0},
    {"anx4", READ_OPERAND, NO_CSS, 0},
    {"anx5", READ_OPERAND, NO_CSS, 0},
    {"anx6", READ_OPERAND, NO_CSS, 0},
    {"anx7", READ_OPERAND, NO_CSS, 0},
    {"epp4", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp5", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp6", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp7", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"ana", READ_OPERAND, 0, 0},
    {"anq", READ_OPERAND, 0, 0},
    {"anaq", READ_YPAIR, NO_DDCSS, 0},
    
    /* 400 */
    {NULL, 0, 0, 0},
    {"mpf", READ_OPERAND, NO_CSS, 0},
    {"mpy", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"cmg", READ_OPERAND, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"lde", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"rscr", PREPARE_CA | PRIV_INS | NO_RPL, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"ade", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"ufm", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dufm", READ_YPAIR, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"fcmg", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfcmg", READ_YPAIR, NO_DDCSS, 0},
    {"fszn", READ_OPERAND, NO_CSS, 0},
    {"fld", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfld", READ_YPAIR, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"ufa", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dufa", READ_YPAIR, NO_DDCSS, 0},
    {"sxl0", RMW | NO_RPL, NO_DDCSS, 0},
    {"sxl1", RMW | NO_RPL, NO_DDCSS, 0},
    {"sxl2", RMW | NO_RPL, NO_DDCSS, 0},
    {"sxl3", RMW | NO_RPL, NO_DDCSS, 0},
    {"sxl4", RMW | NO_RPL, NO_DDCSS, 0},
    {"sxl5", RMW | NO_RPL, NO_DDCSS, 0},
    {"sxl6", RMW | NO_RPL, NO_DDCSS, 0},
    {"sxl7", RMW | NO_RPL, NO_DDCSS, 0},
    {"stz", STORE_OPERAND | NO_RPL, NO_DUDL, 0},
    {"smic", PREPARE_CA | PRIV_INS, NO_DDCSS, 0},
    {"scpr", STORE_YPAIR | NO_TAG | PRIV_INS | NO_RPT, 0, 0},
    {NULL, 0, 0, 0},
    {"stt", STORE_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"fst", STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ste", RMW, NO_DDCSS, 0},
    {"dfst", STORE_YPAIR | NO_RPL, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"fmp", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfmp", READ_YPAIR, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"fstr", STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"frd", NO_RPL, 0, 0}, // XXX "MODIFICATIONS: All, but none affect instruction execution."
    {"dfstr", STORE_YPAIR | NO_RPL, NO_DDCSS, 0},
    {"dfrd", NO_RPL, 0, 0}, // XXX "MODIFICATIONS: All, but none affect instruction execution."
    {NULL, 0, 0, 0},
    {"fad", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfad", READ_YPAIR, NO_DDCSS, 0},
    
    /* 500 */
    {"rpl", NO_TAG | NO_RPT, 0, 0},   // really wierd XXX verify PREPARE_CA
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"bcd", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"div", READ_OPERAND, 0, 0},
    {"dvf", READ_OPERAND, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"fneg", NO_RPL, 0, 0}, // XXX "MODIFICATIONS: All, but none affect instruction execution."
    {NULL, 0, 0, 0},
    {"fcmp", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfcmp", READ_YPAIR, NO_DDCSS, 0},
    {"rpt", NO_TAG | NO_RPT, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"fdi", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfdi", READ_YPAIR, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"neg", NO_RPL, 0, 0}, // XXX "MODIFICATIONS: All, but none affect instruction execution."
    {"cams", PREPARE_CA | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"negl", NO_RPL, 0, 0}, // XXX "MODIFICATIONS: All, but none affect instruction execution."
    {NULL, 0, 0, 0},
    {"ufs", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dufs", READ_YPAIR, NO_DDCSS, 0},
    {"sprp0", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sprp1", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sprp2", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sprp3", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sprp4", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sprp5", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sprp6", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sprp7", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sbar", RMW, NO_DDCSS, 0},
    {"stba", RMW | NO_TAG | NO_RPT, 0, 0},
    {"stbq", RMW | NO_TAG | NO_RPT, 0, 0},
    {"smcm", PREPARE_CA | PRIV_INS | NO_RPL, NO_DDCSS, 0},
    {"stc1", STORE_OPERAND | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"ssdp", STORE_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"rpd", NO_TAG | NO_RPT, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"fdv", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfdv", READ_YPAIR, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"fno", NO_RPL, 0, 0}, // XXX "MODIFICATIONS: All, but none affect instruction execution."
    {NULL, 0, 0, 0},
    {"fsb", READ_OPERAND, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"dfsb", READ_YPAIR, NO_DDCSS, 0},
    /* 600 */
    {"tze", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tnz", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tnc", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"trc", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tmi", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tpl", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"ttf", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"rtcd", READ_YPAIR | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"rcu", READ_YBLOCK8 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"teo", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"teu", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"dis", PRIV_INS | NO_RPT, 0, 0},// XXX "MODIFICATIONS: All, but none affect instruction execution."
    {"tov", /* PREPARE_CA | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"eax0", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax1", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax2", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax3", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax4", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax5", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax6", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax7", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"ret", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"rccl", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"ldi", READ_OPERAND | NO_RPT, NO_CSS, 0},
    {"eaa", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eaq", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"ldt", READ_OPERAND | PRIV_INS | NO_RPT, NO_CSS, 0},
    {"ersx0", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersx1", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersx2", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersx3", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersx4", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersx5", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersx6", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersx7", RMW | NO_RPL, NO_DDCSS, 0},
    {"spri4", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spbp5", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spri6", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spbp7", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"stacq", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersa", RMW | NO_RPL, NO_DDCSS, 0},
    {"ersq", RMW | NO_RPL, NO_DDCSS, 0},
    {"scu", STORE_YBLOCK8 | PRIV_INS | NO_RPT, 0, 0},
    {"erx0", READ_OPERAND, NO_CSS, 0},
    {"erx1", READ_OPERAND, NO_CSS, 0},
    {"erx2", READ_OPERAND, NO_CSS, 0},
    {"erx3", READ_OPERAND, NO_CSS, 0},
    {"erx4", READ_OPERAND, NO_CSS, 0},
    {"erx5", READ_OPERAND, NO_CSS, 0},
    {"erx6", READ_OPERAND, NO_CSS, 0},
    {"erx7", READ_OPERAND, NO_CSS, 0},
    {"tsp4", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp5", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp6", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp7", READ_OPERAND | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"lcpr", READ_OPERAND | NO_TAG | PRIV_INS | NO_RPT, 0, 0},
    {"era", READ_OPERAND, 0, 0},
    {"erq", READ_OPERAND, 0, 0},
    {"eraq", READ_YPAIR, NO_DDCSS, 0},
    
    /* 700 */
    {"tsx0", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx1", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx2", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx3", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx4", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx5", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx6", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx7", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tra", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    // CALL6 must fetch the destination instruction to force doAppendCycle
    // to do all of the ring checks and processing.
    //{"call6", PREPARE_CA | TRANSFER_INS | CALL6_INS | NO_RPT, NO_DDCSS, 0},
    {"call6", READ_OPERAND | TRANSFER_INS | CALL6_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"tss", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"xec", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"xed", READ_YPAIR | NO_RPT, NO_DDCSS, 0}, // ????
    {"lxl0", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl1", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl2", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl3", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl4", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl5", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl6", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl7", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {NULL, 0, 0, 0},
    {"ars", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"qrs", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"lrs", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"als", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"qls", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"lls", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"stx0", RMW | NO_RPL, NO_DDCSS, 0},
    {"stx1", RMW | NO_RPL, NO_DDCSS, 0},
    {"stx2", RMW | NO_RPL, NO_DDCSS, 0},
    {"stx3", RMW | NO_RPL, NO_DDCSS, 0},
    {"stx4", RMW | NO_RPL, NO_DDCSS, 0},
    {"stx5", RMW | NO_RPL, NO_DDCSS, 0},
    {"stx6", RMW | NO_RPL, NO_DDCSS, 0},
    {"stx7", RMW | NO_RPL, NO_DDCSS, 0},
    {"stc2", RMW | NO_RPT, NO_DDCSS, 0},
    {"stca", RMW | NO_TAG | NO_RPT, 0, 0},
    {"stcq", RMW | NO_TAG | NO_RPT, 0, 0},
    {"sreg", STORE_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {"sti", RMW | NO_RPT, NO_DDCSS, 0},
    {"sta", STORE_OPERAND | NO_RPL, NO_DUDL, 0},
    {"stq", STORE_OPERAND | NO_RPL, NO_DUDL, 0},
    {"staq", STORE_YPAIR | NO_RPL, NO_DDCSS, 0},
    {"lprp0", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"lprp1", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"lprp2", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"lprp3", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"lprp4", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"lprp5", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"lprp6", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"lprp7", READ_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {"arl", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"qrl", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"lrl", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"gtb", NO_TAG | NO_RPL, 0, 0},
    {"alr", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"qlr", PREPARE_CA | NO_RPL, NO_DDCSS, 0},
    {"llr", PREPARE_CA | NO_RPL, NO_DDCSS, 0}
    
};

struct opCode EISopcodes[01000] = {
    /* 000 - 017 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 020 - 037 */
    {"mve", NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"mvne", NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 040 - 057 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 060 - 077 */
    {"csl", NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {"csr", NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"sztl", NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {"sztr", NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {"cmpb", NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 100 - 117 */
    {"mlr",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {"mrl",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"cmpc",  _EIS_ | EOP1_ALPHA | EOP2_ALPHA, 0, 2},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 120 - 137 */
    {"scd",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {"scdr",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"scm",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {"scmr", IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 140 - 157 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"sptr", STORE_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 167 - 177 */
    {"mvt",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"tct",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {"tctr",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"lptr", READ_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 200 - 217 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"ad2d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {"sb2d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"mp2d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {"dv2d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 220 - 237 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"ad3d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {"sb3d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"mp3d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {"dv3d",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 3},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"lsdr", READ_YBLOCK32 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 240 - 257 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"spbp0", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spri1", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spbp2", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spri3", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"ssdr", STORE_YBLOCK32 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"lptp", READ_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    /* 260 - 277 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 300 - 317 */
    {"mvn",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {"btd",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {"cmpn",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {"dtb",  NO_TAG | NO_XED | NO_RPT | IGN_B29, 0, 2},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"easp1", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"eawp1", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"easp3", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"eawp3", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 320 - 337 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"easp5", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"eawp5", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"easp7", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"eawp7", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 340 - 357 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"epbp0", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp1", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp2", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp3", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 360 - 377 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"epbp4", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp5", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp6", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp7", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 400 - 417 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
#if EMULATOR_ONLY
    /* 420 - 437 */
    {"emcall", IGN_B29, 0, 0},  // 420 we add a emulator call instruction for SIMH use ONLY! (opcode 0420(1))
    {"fxe",  0, 0, 0}, // 421 fxe fault handler
    
#ifdef DEPRECIATED
    // with the lack of a stack it makes it very cumbersome to write decent code for reentrant subroutines.
    // Yes, I *might* be able to use the DI, ID tally modifications, but you need to setup a indirect stack word to reference -
    // and you couldn't easily do stack offsets, w/o another indirect word, etc.
    
    // I spoze' that if I added macro's to the assembler I probably could do this utilizing the native ISA,
    // but that *ain't* happening any time soon.
    
    // So, let's add some instructions.
    
    {"callx", READ_OPERAND | TRANSFER_INS, 0, 0},// 422 and in the spirit of AIM-072...
    {"exit",                 TRANSFER_INS, 0, 0}, // 423
    
    // how 'bout push/pop a/q
    {"pusha", 0, 0, 0}, // 424 push A onto stack via Xn
    {"popa",  0, 0, 0}, // 425 pop word from stack via Xn into A
    {"pushq", 0, 0, 0}, // 426 push Q onto stack via Xn
    {"popq",  0, 0, 0}, // 427 pop word from stack via Xn into Q
#else // !DEPRECIATED
    {NULL, 0, 0, 0}, // 422
    {NULL, 0, 0, 0}, // 423
    {NULL, 0, 0, 0}, // 424
    {NULL, 0, 0, 0}, // 425
    {NULL, 0, 0, 0}, // 426
    {NULL, 0, 0, 0}, // 427
#endif
    
    {NULL, 0, 0, 0}, // 430
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0}, // 437
    
    
#else
    /* 420 - 437 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
#endif
    /* 440 - 457 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"sareg", STORE_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"spl", STORE_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 460 - 477 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"lareg", READ_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"lpl", READ_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 500 - 517 */
    {"a9bd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {"a6bd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {"a4bd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {"abd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"awd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 520 - 537 */
    {"s9bd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {"s6bd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {"s4bd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {"sbd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"swd", IGN_B29 | NO_RPT, ONLY_AU_QU_AL_QL_XN, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"camp", PREPARE_CA | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 540 - 557 */
    {"ara0", RMW | NO_RPT, NO_DDCSS, 0},
    {"ara1", RMW | NO_RPT, NO_DDCSS, 0},
    {"ara2", RMW | NO_RPT, NO_DDCSS, 0},
    {"ara3", RMW | NO_RPT, NO_DDCSS, 0},
    {"ara4", RMW | NO_RPT, NO_DDCSS, 0},
    {"ara5", RMW | NO_RPT, NO_DDCSS, 0},
    {"ara6", RMW | NO_RPT, NO_DDCSS, 0},
    {"ara7", RMW | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"sptp", STORE_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    /* 560 - 577 */
    {"aar0", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"aar1", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"aar2", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"aar3", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"aar4", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"aar5", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"aar6", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"aar7", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 600 - 617 */
    {"trtn", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"trtf", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"tmoz", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tpnz", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"ttn", /* READ_OPERAND | */ PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 620 - 637 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 640 - 657 */
    {"arn0", RMW | NO_RPT, NO_DDCSS, 0},
    {"arn1", RMW | NO_RPT, NO_DDCSS, 0},
    {"arn2", RMW | NO_RPT, NO_DDCSS, 0},
    {"arn3", RMW | NO_RPT, NO_DDCSS, 0},
    {"arn4", RMW | NO_RPT, NO_DDCSS, 0},
    {"arn5", RMW | NO_RPT, NO_DDCSS, 0},
    {"arn6", RMW | NO_RPT, NO_DDCSS, 0},
    {"arn7", RMW | NO_RPT, NO_DDCSS, 0},
    {"spbp4", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spri5", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spbp6", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"spri7", STORE_YPAIR | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 660 - 677 */
    {"nar0", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"nar1", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"nar2", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"nar3", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"nar4", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"nar5", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"nar6", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"nar7", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 700 - 717 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 720 - 737 */
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 740 - 757 */
    {"sar0", RMW | NO_RPT, NO_DDCSS, 0},
    {"sar1", RMW | NO_RPT, NO_DDCSS, 0},
    {"sar2", RMW | NO_RPT, NO_DDCSS, 0},
    {"sar3", RMW | NO_RPT, NO_DDCSS, 0},
    {"sar4", RMW | NO_RPT, NO_DDCSS, 0},
    {"sar5", RMW | NO_RPT, NO_DDCSS, 0},
    {"sar6", RMW | NO_RPT, NO_DDCSS, 0},
    {"sar7", RMW | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"sra", STORE_OPERAND | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    /* 760 - 777 */
    {"lar0", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"lar1", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"lar2", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"lar3", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"lar4", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"lar5", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"lar6", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"lar7", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {"lra", READ_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0},
    {NULL, 0, 0, 0}
};

struct adrMods {
    const char *mod;    ///< mnemonic
    int   Td;           ///< Td value
    int   Flags;
};
typedef struct adrMods adrMods;

struct adrMods extMods[0100] = {    ///< address modifiers w/ extended info
    /* R */
    {NULL, 0},
    {"au", 1},
    {"qu", 2},
    {"du", 3},
    {"ic", 4},
    {"al", 5},
    {"ql", 6},
    {"dl", 7},
    {"0",  8},
    {"1",  9},
    {"2", 10},
    {"3", 11},
    {"4", 12},
    {"5", 13},
    {"6", 14},
    {"7", 15},

    /* RI */
    {"n*",  16},
    {"au*", 17},
    {"qu*", 18},
    {NULL,  19},
    {"ic*", 20},
    {"al*", 21},
    {"ql*", 22},
    {NULL,  23},
    {"0*",  24},
    {"1*",  25},
    {"2*",  26},
    {"3*",  27},
    {"4*",  28},
    {"5*",  29},
    {"6*",  30},
    {"7*",  31},
    
    /* IT */
    {"f1",  32},
    {"itp", 33},
    {NULL,  34},
    {"its", 35},
    {"sd",  36},
    {"scr", 37},
    {"f2",  38},
    {"f3",  39},
    {"ci",  40},
    {"i",   41},
    {"sc",  42},
    {"ad",  43},
    {"di",  44},
    {"dic", 45},
    {"id",  46},
    {"idc", 47},
    
    /* IR */
    {"*n",  48},
    {"*au", 49},
    {"*qu", 50},
    {"*du", 51},
    {"*ic", 52},
    {"*al", 53},
    {"*ql", 54},
    {"*dl", 55},
    {"*0",  56},
    {"*1",  57},
    {"*2",  58},
    {"*3",  59},
    {"*4",  60},
    {"*5",  61},
    {"*6",  62},
    {"*7",  63},
};

static char *mfxArray[] = {    ///< address modifiers w/ extended info
    "",
    "au",
    "qu",
    "IPR",
    "ic",
    "a",
    "q",
    "IPR",
    "x0",
    "x1",
    "x2",
    "x3",
    "x4",
    "x5",
    "x6",
    "x7",
};

static
char *decomposeMF(int mfn)
{
    static char buff[256];
    buff[0] = 0;
    
    if (mfn & 0x40)
        strcat(buff, "ar");
    
    if (mfn & 0x20)
    {
        if (strlen(buff))
            strcat(buff, ",");
        strcat(buff, "rl");
    }
    if (mfn & 0x10)
    {
        if (strlen(buff))
            strcat(buff, ",");
        strcat(buff, "id");
    }
    
    // make certain there is something there
    if (mfn & 0xf)
    {
        if (strlen(buff))
            strcat(buff, ",");
        strcat(buff, mfxArray[mfn & 0xf]);
    }
    return buff;
}


/*
 * a formatted strcat() ...
 */
#include <stdarg.h>

static char *
strcatf(char *dst, const char * format, ... )
{
    if (!dst)
        return dst;
        
    char buf[1024];
        
    va_list args;
    va_start (args, format);
    vsprintf (buf, format, args);
    va_end (args);
        
    strcat(dst, buf);
        
    return dst;
}

// disassemble an ARG descriptor
char *disassembleARG(word36 descriptor)
{
    static char buff[64];
    
    int address = (descriptor >> 18) & 0777777; // address
    int a = (descriptor >> 7) & 1;  // A-bit
    
    char adr[32];
    if (a)  // A bit set
        sprintf(adr, "pr%d|%05o", (address >> 15) & 07, (address) & 077777);
    else
        sprintf(adr, "%06o", address);
    
    char tag[32];
    int reg = descriptor & 037;
    sprintf(tag, "%s", reg ? extMods[reg].mod : "");
    
    sprintf(buff, "arg\t%s,%s", adr, tag);
    return buff;
    
}

// disassemble a numeric decriptor
char *disassembleDESCn(word36 descriptor, int MFk)
{
    static char buff[132];
    buff[0] = 0;

    int n       =  descriptor        & 077;     // length
    int tn      = (descriptor >> 14) & 1;
    int cn      = (descriptor >> 15) & 07;      // offset
    int address = (descriptor >> 18) & 0777777; // address

    // an indirect descriptor?
    if (MFk & 0x10)
    {
        strcpy(buff, disassembleARG(descriptor));
        return buff;
    }
    
    // address(offset),length
    char adr[32];
    if (MFk & 0x40) // AR bit set
        sprintf(adr, "pr%d|%05o", (address >> 15) & 07, (address) & 077777);
    else
        sprintf(adr, "%06o", address);
    
    char len[32];
    if (MFk & 0x20) // RL bit set
        sprintf(len, "%s", mfxArray[n & 0xf]);
    else
        sprintf(len, "%02o", n);
    
    sprintf(buff, "desc%cn\t%s(%o),%s", tn ? '4' : '9', adr, cn, len);
    return buff;
}

// disassemble a FP numeric decriptor
char *disassembleDESCf(word36 descriptor, int MFk)
{
    static char buff[132];
    buff[0] = 0;
    
    int n       =  descriptor        & 077;     // length
    int sf      = (descriptor >>  6) & 077;
    int s       = (descriptor >> 12) & 03;
    int tn      = (descriptor >> 14) & 1;
    int cn      = (descriptor >> 15) & 07;      // offset
    int address = (descriptor >> 18) & 0777777; // address
    
    // an indirect descriptor?
    if (MFk & 0x10)
    {
        strcpy(buff, disassembleARG(descriptor));
        return buff;
    }
    
    // address(offset),length
    char adr[32];
    if (MFk & 0x40) // AR bit set
        sprintf(adr, "pr%d|%05o", (address >> 15) & 07, (address) & 077777);
    else
        sprintf(adr, "%06o", address);
    
    char len[32];
    if (MFk & 0x20) // RL bit set
        sprintf(len, "%s", mfxArray[n & 0xf]);
    else
        sprintf(len, "%02o", n);
    
    char _tn[16];
    int _cn;
    switch(tn)
    {
        case 0:
            strcpy(_tn, "9");
            _cn = (cn >> 1) & 03;
            break;
        case 1:
            strcpy(_tn, "4");
            _cn = cn;
            break;
    }

    char _s[16];
    switch (s)
    {
        case 0:
            strcpy(_s, "fl");    // floating-point, leading sign
            break;
        case 1:
            strcpy(_s, "ls");   // Scaled fixed-point, leading sign
            break;
        case 2:
            strcpy(_s, "ts");   // Scaled fixed-point, trailing sign
            break;
        case 3:
            strcpy(_s, "ns"); // Scaled fixed-point, unsigned
            break;
    }
    
    sprintf(buff, "desc%s%s\t%s(%o),%s,%o", _tn, _s, adr, cn, len, sf);
    return buff;
}


// disassemble an alphanumeric descriptor
char *disassembleDESCa(word36 descriptor, int MFk)
{
    static char buff[132];
    buff[0] = 0;
    
    int n       =  descriptor       & 07777;     // length
    int ta      = (descriptor >> 13) & 03;
    int cn      = (descriptor >> 15) & 07;      // offset
    int address = (descriptor >> 18) & 0777777; // address
    
    // an indirect descriptor?
    if (MFk & 0x10)
    {
        strcpy(buff, disassembleARG(descriptor));
        return buff;
    }
    
    // address(offset),length
    char adr[32];
    if (MFk & 0x40) // AR bit set
        sprintf(adr, "pr%d|%05o", (address >> 15) & 07, (address) & 077777);
    else
        sprintf(adr, "%06o", address);
    
    char len[32];
    if (MFk & 0x20) // RL bit set
        sprintf(len, "%s", mfxArray[n & 0xf]);
    else
        sprintf(len, "%04o", n);
    
    char _ta[16];
    int _cn;
    switch(ta)
    {
        case 0:
            strcpy(_ta, "9");
            _cn = (cn >> 1) & 03;
            break;
        case 1:
            strcpy(_ta, "6");
            _cn = cn;
            break;
        case 2:
            strcpy(_ta, "4");
            _cn = cn;
            break;
        default:
            strcpy(_ta, "?");
            _cn = 0;
            break;
    }
    sprintf(buff, "desc%sa\t%s(%o),%s", _ta, adr, _cn, len);
    return buff;

}

char *disassembleDESCb(word36 descriptor, int MFk)
{
    static char buff[132];
    buff[0] = 0;
    
    int n       =  descriptor        & 07777;   // length
    int b       = (descriptor >> 12) & 0xf;
    int c       = (descriptor >> 16) & 03;
    int offset  = (c * 9) + b;     // offset is in bits
    int address = (descriptor >> 18) & 0777777; // address
    
    
    // an indirect descriptor?
    if (MFk & 0x10)
    {
        strcpy(buff, disassembleARG(descriptor));
        return buff;
    }
    
    // address(offset),length
    char adr[32];
    if (MFk & 0x40) // AR bit set
        sprintf(adr, "pr%d|%05o", (address >> 15) & 07, (address) & 077777);
    else
        sprintf(adr, "%06o", address);
    
    char len[32];
    if (MFk & 0x20) // RL bit set
        sprintf(len, "%s", mfxArray[n & 0xf]);
    else
        sprintf(len, "%02o", n);
    
    sprintf(buff, "descb\t%s(%o),%s", adr, offset, len);
    return buff;
}

char *disAssembleMW(word36* ins)
{
    word36 instruction = *ins;
    
    int32 opcode  = GET_OP(instruction);   ///< get opcode
    int32 opcodeX = GET_OPX(instruction);  ///< opcode extension
    
    static char result[1320] = "???";
    strcpy(result, "???");

    int mf1 = 0, mf2 = 0, mf3 = 0;
    int fill = 0, mask = 0, bolr = 0, t = 0;
    int f = 0, p = 0, r = 0;
    
    if (!opcodeX)
    {
        return "non-EIS!";
    } else {
        // EIS second...
        if (EISopcodes[opcode].ndes == 0)
            return "non-MW-EIS!";

        if (EISopcodes[opcode].mne)
            strcpy(result, EISopcodes[opcode].mne);
        else
            return "cannot find EIS opcode!";
    }

    switch (opcode)
    {
        case 0106: // cmpc
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            fill = (instruction >> 28) & 0777;
            strcatf(result, "\t(%s),(%s),fill(%o) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), fill, instruction);
            strcatf(result, "\t%s\t\"%012llo\n", disassembleDESCa(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s\t\"%012llo\n", disassembleDESCa(ins[2], mf2), ins[2]);
            break;
        case 0120: // scd
        case 0121: // scdr
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            strcatf(result, "\t(%s),(%s) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCa(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCa(ins[2], mf2), ins[2]);
            break;
        case 0124: // scm
        case 0125: // scmr
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            mask = (instruction >> 28) & 0777;
            strcatf(result, "\t(%s),(%s),mask(%o) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), mask, instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCn(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCn(ins[2], mf2), ins[2]);
            strcatf(result, "\t%s \"%012llo\n", disassembleARG(ins[3]), ins[3]);
            break;
        case 0164: // tct
        case 0165: // tctr
            mf1 = GET_MF1(instruction);
            strcatf(result, "\t(%s) \"%012llo\n", decomposeMF(mf1), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCa(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleARG(ins[2]), ins[2]);
            strcatf(result, "\t%s \"%012llo\n", disassembleARG(ins[3]), ins[3]);
            break;
        case 0100: // mlr
        case 0101: // mrl
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            fill = (instruction >> 28) & 0777;
            strcatf(result, "\t(%s),(%s),fill(%o) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), fill, instruction);
            strcatf(result, "\t%s\t\"%012llo\n", disassembleDESCa(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s\t\"%012llo\n", disassembleDESCa(ins[2], mf2), ins[2]);
            break;
        case 0020: // mve
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            mf3 = GET_MF3(instruction);
            strcatf(result, "\t(%s),(%s),(%s) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), decomposeMF(mf3), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCn(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCn(ins[2], mf2), ins[2]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCn(ins[3], mf2), ins[3]);
            break;
        case 0160: // mvt
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            mf3 = GET_MF3(instruction);
            strcatf(result, "\t(%s),(%s),(%s) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), decomposeMF(mf3), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCn(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCn(ins[2], mf2), ins[2]);
            strcatf(result, "\t%s \"%012llo\n", disassembleARG(ins[3]), ins[3]);
            break;
        case 0303: // cmpn
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            strcatf(result, "\t(%s),(%s) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[2], mf2), ins[2]);
            break;
        case 0300: // mvn
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            strcatf(result, "\t(%s),(%s) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[2], mf2), ins[2]);
            break;
        case 0025: // mvne
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            mf3 = GET_MF3(instruction);
            strcatf(result, "\t(%s),(%s),(%s) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), decomposeMF(mf3), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCa(ins[2], mf2), ins[2]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCa(ins[3], mf3), ins[3]);
            break;
        case 0060: // csl
        case 0061: // csr
        case 0064: // sztl
        case 0065: // sztr
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            t = (instruction >> 27) & 1;
            bolr = (instruction >> 28) & 017;
            f = (instruction >> 35) & 1;
            strcatf(result, "\t(%s),(%s),bolr(%o)%s%s \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2),  bolr, t ? ",enablefault": "", f ? ",fill(1)" : ",fill(0)", instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCb(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCb(ins[2], mf2), ins[2]);
            break;
        case 0066: // cmpb
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            t = (instruction >> 27) & 1;
            f = (instruction >> 35) & 1;
            strcatf(result, "\t(%s),(%s)%s%s \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2),  t ? ",enablefault": "", f ? ",fill(1)" : ",fill(0)", instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCb(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCb(ins[2], mf2), ins[2]);
            break;
        case 0301: // btd
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            t = (instruction >> 27) & 1;
            p = (instruction >> 35) & 1;
            strcatf(result, "\t(%s),(%s)%s%s \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2),  t ? ",enablefault": "", p ? ",P" : "", instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCa(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[2], mf2), ins[2]);
            break;
        case 0305: // dtb
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            sprintf(result, "\t(%s),(%s) \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCa(ins[2], mf2), ins[2]);
            break;
        case 0202: // ad2d
        case 0203: // sb2d
        case 0206: // mp2d
        case 0207: // dv2d
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            r = (instruction >> 26) & 1;
            t = (instruction >> 27) & 1;
            p = (instruction >> 35) & 1;
            strcatf(result, "\t(%s),(%s)%s%s%s \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2),  r ? ",round" : "", t ? ",enablefault": "", p ? ",P" : "", instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[2], mf2), ins[2]);
            break;
        case 0222: // ad3d
        case 0226: // mp3d
        case 0227: // dv3d
            mf1 = GET_MF1(instruction);
            mf2 = GET_MF2(instruction);
            mf3 = GET_MF3(instruction);
            r = (instruction >> 26) & 1;
            t = (instruction >> 27) & 1;
            p = (instruction >> 35) & 1;
            strcatf(result, "\t(%s),(%s),(%s)%s%s%s \"%012llo\n", decomposeMF(mf1), decomposeMF(mf2), decomposeMF(mf3),  r ? ",round" : "", t ? ",enablefault": "", p ? ",P" : "", instruction);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[1], mf1), ins[1]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[2], mf2), ins[2]);
            strcatf(result, "\t%s \"%012llo\n", disassembleDESCf(ins[3], mf3), ins[3]);
            break;
        default:
            strcatf(result, "Unhandled MW EIS \"%012llo", instruction);
            break;
    }
    
    return result;
}

char *disAssembleSW(word36 instruction)
{
    // XXX need to reconstruct multi-word EIS instruction.
    int32 opcode  = GET_OP(instruction);   ///< get opcode
    int32 opcodeX = GET_OPX(instruction);  ///< opcode extension
    a8    address = GET_ADDR(instruction);
    int32 a       = GET_A(instruction);
    int32 tag     = GET_TAG(instruction);
    
    static char result[132] = "???";
    strcpy(result, "???");
    
    if (!opcodeX)
    {
        if (NonEISopcodes[opcode].mne)
            strcpy(result, NonEISopcodes[opcode].mne);
        
    } else {
        // EIS second...
        if (EISopcodes[opcode].mne)
            strcpy(result, EISopcodes[opcode].mne);
    }
    
    if (a)
    {
        int n = (address >> 15) & 07;
        int offset = address & 077777;
        
        strcatf(result, "\tpr%d|0%o", n, offset);
    } else {
        strcatf(result, "\t0%06o", address);
    }
    
    // get mod
    if (extMods[tag].mod)
    {
        strcat(result, ",");
        strcat(result, extMods[tag].mod);
    }
    
    strcatf(result, " \"%012llo", instruction);
    
    return result;
}

#include <ctype.h>

char *disAssembleOCT(word36 instruction)
{
    static char buff[132];
    buff[0] = 0;
    
    // at this point 'instruction' contains no instruction. Binary orperhaps ascii?
    bool isPrint = true;
    for(int i = 3 ; i >= 0 ; i -= 1)
    {
        int temp9 = (instruction >> (9 * i)) & 0777;    // convert to 8-bit ascii
        if (!isprint(temp9) || (temp9 & 0400))          // non-printable or 9th bit set
        {
            isPrint = false;
            break;
        }
    }
    if (isPrint)
    {
        char temp[32];
        for(int i = 3 ; i >= 0 ; i -= 1)
        {
            int temp9 = (instruction >> (9 * i)) & 0x7f;    // convert to 8-bit ascii
            temp[i] = temp9;
        }
        temp[4] = 0;
        strcatf(buff, "\tacc\t'%s'", temp);
        
    } else
        strcatf(buff, "\toct\t%012llo", instruction & 0777777777777LL);
    return buff;
}

char *disAssemble(word36 *ins, int *addl)
{
    word36 instruction = *ins & 0777777777777LL;
    
    int32 opcode  = GET_OP(instruction);   ///< get opcode
    int32 opcodeX = GET_OPX(instruction);  ///< opcode extension

    static char result[1320] = "???";
    strcpy(result, "???");

    *addl = 0;  // typically no additional words
    
    // get mnemonic ...
    // non-EIS first
    if (!opcodeX)
    {
        if (NonEISopcodes[opcode].mne)
            strcpy(result, disAssembleSW(instruction));
    } else {
        // EIS second...
        if (EISopcodes[opcode].mne)
        {
            strcpy(result, EISopcodes[opcode].mne);
    
            if (EISopcodes[opcode].ndes > 0)
            {
                strcpy(result, disAssembleMW(ins));
                *addl = EISopcodes[opcode].ndes;
            }
            else
                strcpy(result, disAssembleSW(instruction));
        }
    }
    return result; // strupr(result);
}

void disassembleMemory(word36 *mem, int nWords, long *offset)
{
    printf ("\torg\t0%0lo\n", *offset);

    int wasInhibit = 0;

    
    int size;
    
    for(int n = 0 ; n < nWords ; n += 1 + size, *offset += 1 + size)
    {
        if (n % 8 == 0)
            printf ("\" %06lo:\n", *offset);
        
        //for (;;)
        {
            word36 w = mem[n] & 0777777777777LL;
#if 0
            if (w & 0200)
                printf ("\" inhibit on\n");
            printf ("\" %012lo %s\n", w, disAssemble (w));
            if (w & 0200)
                printf ("\" inhibit on\n");
#else
            char * d = disAssemble (mem + n, &size);
//            if (d [0] == '?' ||
//                    strncmp (d, "scdr", 4) == 0 ||
//                    w == 0252525252525 || // special cases for blk0
//                    w == 0474400022120 ||
//                    w == 0473326336333)
//            {
            if (d [0] == '?')
            {
                if (wasInhibit)
                {
                    printf ("\tinhibit\toff\n");
                    wasInhibit = 0;
                }
                //printf ("\toct\t%012llo\n", w);
                printf("%s\t\" %012llo\n", disAssembleOCT(w), w);
            }
            else
            {
                if (w & 0200)
                {
                    if (! wasInhibit)
                        printf ("\tinhibit\ton\n");
                    wasInhibit = 1;
                }
                else
                {
                    if (wasInhibit)
                        printf ("\tinhibit\toff\n");
                    wasInhibit = 0;
                }
                printf ("\t%s\n", disAssemble (mem + n, &size));
            }
#endif
        }
    }
}

word36 M[256 * 1024];   // allow for 1 segfment of 36-bit words

int readTape (char *tape, word36 *mem)
{
    int fd = open (tape, O_RDONLY);
    if (fd < 0)
    {
        fprintf (stderr, "can't open tape %s\n", tape);
        exit (1);
    }
    
    int n = 0;
    
    for (;;)
    {
        uint8_t Ypair [9] = {0,0,0,0,0,0,0,0,0};
        
        ssize_t sz = read (fd, Ypair, 9);
        if (sz <= 0)
            break;
        
        if (sz != 9)
            fprintf (stderr, "partial read? (%lu)\n", sz);
      
        for (int i = 0; i < 2; i ++)
        {
            word36 w = extr36 (Ypair, i);
            mem[n++] = w;
        }
    }
    close (fd);
    
    return n;
}

#if OLD
int mainOLD (int argc, char * argv [])
{
    long offset = strtol (argv [1], NULL, 0);
    int fd = open (argv [2], O_RDONLY);
    if (fd < 0)
    {
        fprintf (stderr, "can't open tape %s\n", argv[2]);
        exit (1);
    }
    
    printf ("\torg\t0%06lo\n", offset);
    int wasInhibit = 0;
    for (;;)
    {
        uint8_t Ypair [9];
        ssize_t sz = read (fd, Ypair, 9);
        if (sz <= 0)
            break;
        if (sz != 9)
            fprintf (stderr, "partial read? (%lu)\n", sz);
        if (offset % 8 == 0)
            printf ("\" %06lo:\n", offset);
        for (int i = 0; i < 2; i ++)
        {
            word36 w = extr36 (Ypair, i);
#if 0
            if (w & 0200)
                printf ("\" inhibit on\n");
            printf ("\" %012lo %s\n", w, disAssemble (w));
            if (w & 0200)
                printf ("\" inhibit on\n");
#else
            char * d = disAssemble (w);
            if (d [0] == '?' ||
                strncmp (d, "scdr", 4) == 0 ||
                w == 0252525252525 || // special cases for blk0
                w == 0474400022120 ||
                w == 0473326336333)
            {
                if (wasInhibit)
                {
                    printf ("\tinhibit\toff\n");
                    wasInhibit = 0;
                }
                //printf ("\toct\t%012llo\n", w);
                printf("%s\t\"%012llo\n", disAssembleOCT(w), w);
            }
            else
            {
                if (w & 0200)
                {
                    if (! wasInhibit)
                        printf ("\tinhibit\ton\n");
                    wasInhibit = 1;
                }
                else
                {
                    if (wasInhibit)
                        printf ("\tinhibit\toff\n");
                    wasInhibit = 0;
                }
                printf ("\t%s\t\" %012llo\n", disAssemble (w), w);
            }
#endif
        }
        offset += 2;
    }
    close (fd);
    return 0;
}
#endif

#include <unistd.h>

void usage(int argc, char **argv)
{
    printf("Usage: %s [-f offset] InpuFile(s)\n", argv[0]);
}

int main (int argc, char * argv [])
{
    //int aflag = 0;      // append mode
    long offset = 0;    // offset used for disassembly
    
    char ch, *endptr;
    
    while ((ch = getopt(argc, argv, "f:v")) != -1) {
        switch (ch) {
            //case 'a':
            //    aflag = 1;  // append all inputs files together
            //    break;
            case 'f':
                //  If endptr is not NULL, strtol() stores the address of the first invalid
                //  character in *endptr.  If there were no digits at all, however, strtol()
                //  stores the original value of str in *endptr.  (Thus, if *str is not `\0'
                //  but **endptr is `\0' on return, the entire string was valid.)
                
                offset = strtol (optarg, &endptr, 0);
                if (*endptr != 0)
                {
                    (void)fprintf(stderr, "%s: %s: invalid offset\n", argv[0], optarg);
                    exit(1);
                }
                break;
            case 'v':
                printf("Hello World! This is dis v0.1 (build: " __TIME__ " " __DATE__ ") * * *\n");
                usage(argc, argv);
                break;
            case '?':
            default:
                usage(argc, argv);
        }
    }
    argc -= optind;
    argv += optind;
    
    for(int n = 0 ; n < argc ; n++)
    {
        word36 mem[256 * 1024];
        int nWords = readTape(argv[n], mem);
        
        printf("\" file %d: %s\n", n+1, argv[n]);
        
        disassembleMemory(mem, nWords, &offset);
    }
    return 0;
}


