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
#include <inttypes.h>
#include <ctype.h>

#include "bit36.h"

#ifdef __MINGW64__
#define open(x,y,args...) open(x, y|O_BINARY,##args)
#define creat(x,y) open(x, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, y)
#undef isprint
#define isprint(c) (c>=0x20 && c<=0x7f)
#endif

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



static struct opCode NonEISopcodes[01000] = {
    /* 000 */
    {},
    {"mme", NO_RPT, 0},
    {"drl", NO_RPT, 0},
    {},
    {"mme2", NO_RPT, 0},
    {}, // XXX {"mme3", NO_RPT, 0},
    {},
    {"mme4", NO_RPT, 0},
    {},
    {"nop", PREPARE_CA, 0},
    {}, // XXX {"puls1", NO_RPT, 0},
    {"puls2", NO_RPT, 0},
    {},
    {"cioc", PRIV_INS | NO_RPT, 0},
    {},
    {},
    {"adlx0", READ_OPERAND, NO_CSS, 0},
    {"adlx1", READ_OPERAND, NO_CSS, 0},
    {"adlx2", READ_OPERAND, NO_CSS, 0},
    {"adlx3", READ_OPERAND, NO_CSS, 0},
    {"adlx4", READ_OPERAND, NO_CSS, 0},
    {"adlx5", READ_OPERAND, NO_CSS, 0},
    {"adlx6", READ_OPERAND, NO_CSS, 0},
    {"adlx7", READ_OPERAND, NO_CSS, 0},
    {},
    {},
    {}, // XXX {"ldqc", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"adl",  READ_OPERAND, NO_CSS, 0},
    {"ldac", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"adla", READ_OPERAND, 0},
    {"adlq", READ_OPERAND, 0},
    {"adlaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"asx0", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asx1", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {}, //  XXX {"asx2", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asx3", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asx4", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asx5", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asx6", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asx7", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"adwp0", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"adwp1", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"adwp2", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"adwp3", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"aos", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asa", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"asq", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"sscr", PREPARE_CA | PRIV_INS, NO_DDCSS, 0},
    {"adx0", READ_OPERAND, NO_CSS, 0},
    {"adx1", READ_OPERAND, NO_CSS, 0},
    {"adx2", READ_OPERAND, NO_CSS, 0},
    {"adx3", READ_OPERAND, NO_CSS, 0},
    {"adx4", READ_OPERAND, NO_CSS, 0},
    {"adx5", READ_OPERAND, NO_CSS, 0},
    {"adx6", READ_OPERAND, NO_CSS, 0},
    {"adx7", READ_OPERAND, NO_CSS, 0},
    {},
    {"awca", READ_OPERAND, 0},
    {"awcq", READ_OPERAND, 0},
    {"lreg", READ_OPERAND | READ_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {},
    {"ada", READ_OPERAND, 0},
    {"adq", READ_OPERAND, 0},
    {"adaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    
    /* 100 */
    {"cmpx0", READ_OPERAND, NO_CSS, 0},
    {"cmpx1", READ_OPERAND, NO_CSS, 0},
    {"cmpx2", READ_OPERAND, NO_CSS, 0},
    {"cmpx3", READ_OPERAND, NO_CSS, 0},
    {"cmpx4", READ_OPERAND, NO_CSS, 0},
    {"cmpx5", READ_OPERAND, NO_CSS, 0},
    {"cmpx6", READ_OPERAND, NO_CSS, 0},
    {"cmpx7", READ_OPERAND, NO_CSS, 0},
    {},
    {"cwl", READ_OPERAND, 0},
    {}, {}, {},
    {"cmpa", READ_OPERAND, 0},
    {"cmpq", READ_OPERAND, 0},
    {"cmpaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"sblx0", READ_OPERAND, NO_CSS, 0},
    {"sblx1", READ_OPERAND, NO_CSS, 0},
    {"sblx2", READ_OPERAND, NO_CSS, 0},
    {"sblx3", READ_OPERAND, NO_CSS, 0},
    {"sblx4", READ_OPERAND, NO_CSS, 0},
    {"sblx5", READ_OPERAND, NO_CSS, 0},
    {"sblx6", READ_OPERAND, NO_CSS, 0},
    {"sblx7", READ_OPERAND, NO_CSS, 0},
    {}, {}, {}, {}, {},
    {"sbla", READ_OPERAND, 0},
    {"sblq", READ_OPERAND, 0},
    {"sblaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"ssx0", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssx1", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssx2", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssx3", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssx4", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssx5", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssx6", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssx7", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"adwp4", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"adwp5", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"adwp6", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"adwp7", READ_OPERAND | NO_RPT, NO_DLCSS, 0},
    {"sdbr", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"ssa", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ssq", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {},
    {"sbx0", READ_OPERAND, NO_CSS, 0},
    {"sbx1", READ_OPERAND, NO_CSS, 0},
    {"sbx2", READ_OPERAND, NO_CSS, 0},
    {"sbx3", READ_OPERAND, NO_CSS, 0},
    {"sbx4", READ_OPERAND, NO_CSS, 0},
    {"sbx5", READ_OPERAND, NO_CSS, 0},
    {"sbx6", READ_OPERAND, NO_CSS, 0},
    {"sbx7", READ_OPERAND, NO_CSS, 0},
    {},
    {"swca", READ_OPERAND, 0},
    {"swcq", READ_OPERAND, 0},
    {"lpri", PREPARE_CA | READ_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {},
    {"sba", READ_OPERAND, 0},
    {"sbq", READ_OPERAND, 0},
    {"sbaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    /* 200 */
    {}, // XXX {"cnax0", READ_OPERAND, NO_CSS, 0},
    {}, // XXX {"cnax1", READ_OPERAND, NO_CSS, 0},
    {}, // XXX {"cnax2", READ_OPERAND, NO_CSS, 0},
    {}, // XXX {"cnax3", READ_OPERAND, NO_CSS, 0},
    {}, // XXX {"cnax4", READ_OPERAND, NO_CSS, 0},
    {}, // XXX {"cnax5", READ_OPERAND, NO_CSS, 0},
    {}, // XXX {"cnax6", READ_OPERAND, NO_CSS, 0},
    {}, // XXX {"cnax7", READ_OPERAND, NO_CSS, 0},
    {},
    {"cmk", READ_OPERAND, 0},
    {"absa", PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"epaq",   NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"sznc", STORE_OPERAND, NO_DDCSS, 0},
    {"cnaa", READ_OPERAND, 0},
    {"cnaq", READ_OPERAND, 0},
    {"cnaaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"ldx0", READ_OPERAND, NO_CSS, 0},  
    {"ldx1", READ_OPERAND, NO_CSS, 0},
    {"ldx2", READ_OPERAND, NO_CSS, 0},
    {"ldx3", READ_OPERAND, NO_CSS, 0},
    {"ldx4", READ_OPERAND, NO_CSS, 0},
    {"ldx5", READ_OPERAND, NO_CSS, 0},
    {"ldx6", READ_OPERAND, NO_CSS, 0},
    {"ldx7", READ_OPERAND, NO_CSS, 0},
    {"lbar", READ_OPERAND, NO_CSS, 0},
    {"rsw", PRIV_INS | NO_RPT | PREPARE_CA, 0},
    {"ldbr", PREPARE_CA | READ_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS,  0},
    {"rmcm", PRIV_INS, NO_DDCSS, 0},
    {"szn", READ_OPERAND, 0},
    {"lda", READ_OPERAND, 0},
    {"ldq", READ_OPERAND, 0},
    {"ldaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"orsx0", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsx1", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsx2", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsx3", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsx4", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsx5", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsx6", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsx7", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"spri0", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS,  0},
    {"spbp1", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS,  0},
    {"spri2", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS,  0},
    {"spbp3", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS,  0},
    {"spri", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS,  0},
    {"orsa", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"orsq", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {}, //{"lsdp", PREPARE_CA | READ_YBLOCK16, 0},    // not available on a dps8m
    {"orx0", READ_OPERAND, NO_CSS, 0},
    {"orx1", READ_OPERAND, NO_CSS, 0},
    {"orx2", READ_OPERAND, NO_CSS, 0},
    {"orx3", READ_OPERAND, NO_CSS, 0},
    {"orx4", READ_OPERAND, NO_CSS, 0},
    {"orx5", READ_OPERAND, NO_CSS, 0},
    {"orx6", READ_OPERAND, NO_CSS, 0},
    {"orx7", READ_OPERAND, NO_CSS, 0},
    {"tsp0", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp1", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp2", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp3", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {},
    {"ora", READ_OPERAND, 0},
    {"orq", READ_OPERAND, 0},
    {"oraq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
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
    {},
    {"cana", READ_OPERAND, 0},
    {"canq", READ_OPERAND, 0},
    {"canaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
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
    {},
    {"lca", READ_OPERAND, 0},
    {"lcq", READ_OPERAND, 0},
    {"lcaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"ansx0", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansx1", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansx2", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansx3", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansx4", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansx5", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansx6", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansx7", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"epp0", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp1", NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp2", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp3", NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"stac", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ansa", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"ansq", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"stcd", PREPARE_CA | STORE_YPAIR | NO_RPT, NO_DDCSS, 0},
    {"anx0", READ_OPERAND, NO_CSS, 0},
    {"anx1", READ_OPERAND, NO_CSS, 0},
    {"anx2", READ_OPERAND, NO_CSS, 0},
    {"anx3", READ_OPERAND, NO_CSS, 0},
    {"anx4", READ_OPERAND, NO_CSS, 0},
    {"anx5", READ_OPERAND, NO_CSS, 0},
    {"anx6", READ_OPERAND, NO_CSS, 0},
    {"anx7", READ_OPERAND, NO_CSS, 0},
    {"epp4", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp5", NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epp6", PREPARE_CA | NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"epbp7", NO_BAR | NO_RPT, NO_DDCSS, 0},
    {},
    {"ana", READ_OPERAND, 0},
    {"anq", READ_OPERAND, 0},
    {"anaq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    /* 400 */
    {},
    {"mpf", READ_OPERAND, NO_CSS, 0},
    {"mpy", READ_OPERAND, NO_CSS, 0},
    {}, {},
    {"cmg", READ_OPERAND, 0},
    {}, {}, {},
    {"lde", READ_OPERAND, NO_CSS, 0},
    {},
    {"rscr", PRIV_INS | NO_RPL, NO_DDCSS, 0},
    {},
    {"ade", READ_OPERAND, NO_CSS, 0},
    {}, {}, {},
    {"ufm", READ_OPERAND, NO_CSS, 0},
    {},
    {"dufm", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {},
    {"fcmg", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfcmg", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"fszn", READ_OPERAND, NO_CSS, 0},
    {"fld", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfld", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {},
    {"ufa", READ_OPERAND, NO_CSS, 0},
    {},
    {"dufa", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"sxl0", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"sxl1", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"sxl2", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"sxl3", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"sxl4", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"sxl5", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"sxl6", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"sxl7", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stz", STORE_OPERAND, NO_DUDL, 0},
    {"smic", PRIV_INS, NO_DDCSS, 0},
    {"scpr", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, 0},
    {},
    {"stt", STORE_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"fst", STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ste", READ_OPERAND | STORE_OPERAND, NO_DDCSS, 0},
    {"dfst", PREPARE_CA | STORE_YPAIR | NO_RPL, NO_DDCSS, 0},
    {},
    {"fmp", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfmp", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {}, {}, {}, {},
    {"fstr", STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"frd", NO_RPL, 0},
    {"dfstr", PREPARE_CA | STORE_YPAIR | NO_RPL, NO_DDCSS, 0},
    {"dfrd", NO_RPL, 0},
    {},
    {"fad", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfad", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    /* 500 */
    {}, // XXX {"rpl", READ_OPERAND | NO_RPT, 0},   // really wierd XXXX
    {}, {}, {}, {},
    {"bcd", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"div", READ_OPERAND, 0},
    {"dvf", READ_OPERAND, 0},
    {}, {}, {},
    {"fneg", NO_RPL, 0},
    {},
    {"fcmp", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfcmp", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {}, // XXX {"rpt", NO_RPT, 0, 0},
    {}, {}, {}, {},
    {"fdi", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfdi", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {},
    {"neg", NO_RPL, 0},
    {"cams", PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"negl", NO_RPL, 0},
    {},
    {"ufs", READ_OPERAND, NO_CSS, 0},
    {},
    {"dufs", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {"sprp0", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sprp1", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sprp2", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sprp3", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sprp4", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sprp5", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sprp6", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sprp7", STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"sbar",  STORE_OPERAND | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {/* XXX "stba", READ_OPERAND | STORE_OPERAND | NO_TAG | NO_RPT, 0 */},
    {"stbq", READ_OPERAND | STORE_OPERAND | NO_TAG | NO_RPT, 0},
    {"smcm", PRIV_INS | NO_RPL, NO_DDCSS, 0},
    {"stc1", STORE_OPERAND | NO_RPT, NO_DDCSS, 0},
    {}, {},
    {"ssdp", PREPARE_CA | STORE_YBLOCK16 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"rpd", NO_RPT, 0},
    {}, {}, {}, {},
    {"fdv", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfdv", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    {}, {}, {},
    {"fno", NO_RPL, 0},
    {},
    {"fsb", READ_OPERAND, NO_CSS, 0},
    {},
    {"dfsb", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    /* 600 */
    {"tze", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tnz", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tnc", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"trc", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tmi", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tpl", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {},
    {"ttf", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"rtcd", PREPARE_CA | READ_YPAIR | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {}, {},
    {"rcu", PREPARE_CA | READ_YBLOCK8 | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"teo", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"teu", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"dis", PRIV_INS | NO_RPT, 0},
    {"tov", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"eax0", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax1", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax2", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax3", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax4", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax5", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax6", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eax7", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"ret", READ_OPERAND | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {}, {},
    {"rccl", NO_BAR | NO_RPT, NO_DDCSS, 0},
    {"ldi", READ_OPERAND | NO_RPT, NO_CSS, 0},
    {"eaa", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"eaq", PREPARE_CA | NO_RPL, NO_DUDL, 0},
    {"ldt", READ_OPERAND | PRIV_INS | NO_RPT, NO_CSS, 0},
    {"ersx0", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersx1", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersx2", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersx3", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersx4", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersx5", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersx6", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersx7", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"spri4", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"spbp5", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"spri6", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"spbp7", PREPARE_CA | STORE_YPAIR | PRIV_INS | NO_RPT, NO_DDCSS, 0},
    {"stacq", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersa", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"ersq", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"scu", PREPARE_CA | STORE_YBLOCK8, 0},
    {"erx0", READ_OPERAND, 0},
    {"erx1", READ_OPERAND, 0},
    {"erx2", READ_OPERAND, 0},
    {"erx3", READ_OPERAND, 0},
    {"erx4", READ_OPERAND, 0},
    {"erx5", READ_OPERAND, 0},
    {"erx6", READ_OPERAND, 0},
    {"erx7", READ_OPERAND, 0},
    {"tsp4", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp5", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp6", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {"tsp7", PREPARE_CA | TRANSFER_INS | TSPN_INS | NO_RPT, NO_DDCSS, 0},
    {}, // XXX {"lcpr", READ_OPERAND | NO_TAG | PRIV_INS | NO_RPT | NO_BAR, 0},
    {"era", READ_OPERAND, 0},
    {"erq", READ_OPERAND, 0},
    {"eraq", PREPARE_CA | READ_YPAIR, NO_DDCSS, 0},
    /* 700 */
    {"tsx0", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx1", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx2", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx3", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx4", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx5", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx6", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tsx7", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"tra", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {}, {},
    {"call6", PREPARE_CA | TRANSFER_INS | CALL6_INS | NO_RPT, NO_DDCSS, 0},
    {},
    {"tss", PREPARE_CA | TRANSFER_INS | NO_RPT, NO_DDCSS, 0},
    {"xec", READ_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"xed", PREPARE_CA | READ_YPAIR | NO_RPT, NO_DDCSS, 0}, // ????
    {"lxl0", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl1", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl2", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl3", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl4", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl5", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl6", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {"lxl7", READ_OPERAND | NO_RPL, NO_CSS, 0},
    {},
    {"ars", NO_RPL, NO_DDCSS, 0},
    {"qrs", NO_RPL, NO_DDCSS, 0},
    {"lrs", NO_RPL, NO_DDCSS, 0},
    {},
    {"als", NO_RPL, NO_DDCSS, 0},
    {"qls", NO_RPL, NO_DDCSS, 0},
    {"lls", NO_RPL, NO_DDCSS, 0},
    {"stx0", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stx1", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stx2", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stx3", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stx4", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stx5", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stx6", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stx7", READ_OPERAND | STORE_OPERAND | NO_RPL, NO_DDCSS, 0},
    {"stc2", READ_OPERAND | STORE_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"stca", READ_OPERAND | STORE_OPERAND | NO_TAG | NO_RPT, 0},
    {"stcq", READ_OPERAND | STORE_OPERAND | NO_TAG | NO_RPT, 0},
    {"sreg", PREPARE_CA | STORE_YBLOCK8 | NO_RPT, NO_DDCSS, 0},
    {"sti", READ_OPERAND | STORE_OPERAND | NO_RPT, NO_DDCSS, 0},
    {"sta", STORE_OPERAND | NO_RPL, NO_DUDL, 0},
    {"stq", STORE_OPERAND | NO_RPL, NO_DUDL, 0},
    {"staq", PREPARE_CA | STORE_YPAIR, 0},
    {"lprp0", READ_OPERAND, 0},
    {"lprp1", READ_OPERAND, 0},
    {"lprp2", READ_OPERAND, 0},
    {"lprp3", READ_OPERAND, 0},
    {"lprp4", READ_OPERAND, 0},
    {"lprp5", READ_OPERAND, 0},
    {"lprp6", READ_OPERAND, 0},
    {"lprp7", READ_OPERAND, 0},
    {},
    {"arl", NO_RPL, NO_DDCSS, 0},
    {"qrl", NO_RPL, NO_DDCSS, 0},
    {"lrl", NO_RPL, NO_DDCSS, 0},
    {"gtb", NO_RPL, NO_DDCSS, 0},
    {"alr", NO_RPL, NO_DDCSS, 0},
    {"qlr", NO_RPL, NO_DDCSS, 0},
    {"llr", NO_RPL, NO_DDCSS, 0}

};

static struct opCode EISopcodes[01000] = {
     /* 000 - 017 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 020 - 037 */ {"mve", IGN_B29, 0, 3}, {}, {}, {}, {"mvne", IGN_B29, 0, 3}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 040 - 057 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 060 - 077 */ {/* XXX "csl", IGN_B29, 0, 2 */}, {/* XXX "csr", IGN_B29, 0, 2 */}, {}, {}, {/* XXX "sztl", IGN_B29, 0, 2 */},  {"sztr", IGN_B29, 0, 2},{/* XXX "cmpb", IGN_B29, 0, 2 */}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 100 - 117 */ {"mlr", IGN_B29, 0, 2}, {"mrl", IGN_B29, 0, 2}, {}, {}, {}, {}, {"cmpc", IGN_B29, 0, 2}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 120 - 137 */ {/* XXX "scd", IGN_B29, 0, 3 */}, {"scdr", IGN_B29, 0, 3}, {}, {}, {"scm", IGN_B29, 0, 3},{"scmr", IGN_B29, 0, 3}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 140 - 157 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {"sptr", 0}, {}, {}, {},
     /* 167 - 177 */ {"mvt", IGN_B29, 0, 3}, {}, {}, {}, {"tct", IGN_B29, 0, 3}, {"tctr", IGN_B29, 0, 3}, {}, {}, {}, {}, {}, {"lptr", 0}, {}, {}, {}, {},
     /* 200 - 217 */ {}, {}, {"ad2d", IGN_B29, 0, 2}, {"sb2d", IGN_B29, 0, 2}, {}, {}, {"mp2d", IGN_B29, 0, 2}, {"dv2d", IGN_B29, 0, 2}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 220 - 237 */ {}, {}, {"ad3d", IGN_B29, 0, 3}, {"sb3d", IGN_B29, 0, 3}, {}, {}, {"mp3d", IGN_B29, 0, 3}, {"dv3d", IGN_B29, 0, 3}, {}, {}, {"lsdr", 0}, {}, {}, {}, {}, {},
     /* 240 - 257 */ {}, {}, {}, {}, {}, {}, {}, {},
    {"spbp0", PREPARE_CA | STORE_YPAIR, 0},
    {"spri1", PREPARE_CA | STORE_YPAIR, 0},
    {}, // XXX {"spbp2", PREPARE_CA | STORE_YPAIR, 0},
    {"spri3", PREPARE_CA | STORE_YPAIR, 0},
    {"ssdr", 0}, {}, {}, {"lptp", 0},
     /* 260 - 277 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 300 - 317 */ {"mvn", IGN_B29, 0, 3}, {"btd", IGN_B29, 0, 2}, {}, {"cmpn", IGN_B29, 0, 2}, {}, {"dtb", IGN_B29, 0, 2}, {}, {}, {"easp1", PREPARE_CA, 0}, {"eawp1", PREPARE_CA, 0}, {"easp3", PREPARE_CA, 0}, {"eawp3", PREPARE_CA, 0}, {}, {}, {}, {},
     /* 320 - 337 */ {}, {}, {}, {}, {}, {}, {}, {}, {"easp5", PREPARE_CA, 0}, {"eawp5",PREPARE_CA, 0}, {"easp7", PREPARE_CA, 0}, {"eawp7", PREPARE_CA,  0}, {}, {}, {}, {},
     /* 340 - 357 */ {}, {}, {}, {}, {}, {}, {}, {}, {"epbp0", 0}, {"epp1", PREPARE_CA, 0}, {"epbp2", 0}, {"epp3", PREPARE_CA, 0}, {}, {}, {}, {},
     /* 360 - 377 */ {}, {}, {}, {}, {}, {}, {}, {}, {"epbp4", 0}, {"epp5", PREPARE_CA, 0}, {"epbp6", 0}, {"epp7", PREPARE_CA, 0}, {}, {}, {}, {},
     /* 400 - 417 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
#ifdef EMULATOR_ONLY
     /* 420 - 437 */
    {"emcall", IGN_B29, 0},  // we add a emulator call instruction for SIMH use ONLY! (opcode 0420(1))
  
#ifdef DEPRECIATED
    // with the lack of a stack it makes it very cumbersome to write decent code for reentrant subroutines.
    // Yes, I *might* be able to use the DI, ID tally modifications, but you need to setup a indirect stack word to reference -
    // and you couldn't easily do stack offsets, w/o another indirect word, etc.

    // I spoze' that if I added macro's to the assembler I probably could do this utilizing the native ISA,
    // but that *ain't* happening any time soon.

    // So, let's add some instructions.

    {"callx", READ_OPERAND | TRANSFER_INS, 0},// and in the spirit of AIM-072...
    {"exit",                 TRANSFER_INS, 0},
    
    // how 'bout push/pop a/q
    {"pusha", 0, 0}, // push A onto stack via Xn
    {"popa",  0, 0}, // pop word from stack via Xn into A
    {"pushq", 0, 0}, // push Q onto stack via Xn
    {"popq",  0, 0}, // pop word from stack via Xn into Q
#else
    {}, {}, {}, {}, {}, {},
#endif
    
    {}, {}, {}, {}, {}, {}, {}, {}, {},
    
    
#else
     /* 420 - 437 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
#endif
     /* 440 - 457 */ {}, {}, {}, {"sareg", PREPARE_CA | STORE_YBLOCK8, 0}, {}, {}, {}, {"spl", 0}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 460 - 477 */ {}, {}, {}, {"lareg", PREPARE_CA | READ_YBLOCK8, 0}, {}, {}, {}, {"lpl", 0}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 500 - 517 */ {"a9bd", IGN_B29, 0}, {"a6bd", IGN_B29, 0}, {"a4bd", IGN_B29, 0}, {"abd", IGN_B29, 0}, {}, {}, {}, {"awd", IGN_B29, 0}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 520 - 537 */ {"s9bd", IGN_B29, 0}, {"s6bd", IGN_B29, 0}, {"s4bd", IGN_B29, 0}, {"sbd", IGN_B29, 0}, {}, {}, {}, {"swd", IGN_B29, 0}, {}, {}, {"camp", 0}, {}, {}, {}, {}, {},
     /* 540 - 557 */
    {"ara0", RMW, 0},
    {"ara1", RMW, 0},
    {"ara2", RMW, 0},
    {"ara3", RMW, 0},
    {"ara4", RMW, 0},
    {"ara5", RMW, 0},
    {"ara6", RMW, 0},
    {"ara7", RMW, 0},
    {}, {}, {}, {}, {}, {}, {}, {"sptp", 0},
     /* 560 - 577 */
    {"aar0", READ_OPERAND, 0},
    {"aar1", READ_OPERAND, 0},
    {"aar2", READ_OPERAND, 0},
    {"aar3", READ_OPERAND, 0},
    {"aar4", READ_OPERAND, 0},
    {"aar5", READ_OPERAND, 0},
    {"aar6", READ_OPERAND, 0},
    {"aar7", READ_OPERAND, 0},
    {}, {}, {}, {}, {}, {}, {}, {},
    /* 600 - 617 */
    {"trtn", PREPARE_CA | TRANSFER_INS, 0},
    {"trtf", PREPARE_CA | TRANSFER_INS, 0}, {}, {},
    {"tmoz", PREPARE_CA | TRANSFER_INS, 0},
    {"tpnz", PREPARE_CA | TRANSFER_INS, 0},
    {"ttn",  PREPARE_CA | TRANSFER_INS, 0}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 620 - 637 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 640 - 657 */
    {"arn0", RMW, 0},
    {"arn1", RMW, 0},
    {"arn2", RMW, 0},
    {"arn3", RMW, 0},
    {"arn4", RMW, 0},
    {"arn5", RMW, 0},
    {"arn6", RMW, 0},
    {"arn7", RMW, 0},
    {"spbp4", PREPARE_CA | STORE_YPAIR, 0},
    {"spri5", PREPARE_CA | STORE_YPAIR, 0},
    {"spbp6", PREPARE_CA | STORE_YPAIR, 0},
    {"spri7", PREPARE_CA | STORE_YPAIR, 0}, {}, {}, {}, {},
     /* 660 - 677 */
    {"nar0", READ_OPERAND, 0},
    {"nar1", READ_OPERAND, 0},
    {"nar2", READ_OPERAND, 0},
    {"nar3", READ_OPERAND, 0},
    {"nar4", READ_OPERAND, 0},
    {"nar5", READ_OPERAND, 0},
    {"nar6", READ_OPERAND, 0},
    {"nar7", READ_OPERAND, 0},
    {}, {}, {}, {}, {}, {}, {}, {},
     /* 700 - 717 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 720 - 737 */ {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
     /* 740 - 757 */
    {"sar0", RMW, 0},
    {"sar1", RMW, 0},
    {"sar2", RMW, 0},
    {"sar3", RMW, 0},
    {"sar4", RMW, 0},
    {"sar5", RMW, 0},
    {"sar6", RMW, 0},
    {"sar7", RMW, 0},
    {}, {}, {}, {},
    {"sra",  STORE_OPERAND, 0},
    {}, {}, {},
     /* 760 - 777 */
    {"lar0", READ_OPERAND, 0},
    {"lar1", READ_OPERAND, 0},
    {"lar2", READ_OPERAND, 0},
    {"lar3", READ_OPERAND, 0},
    {"lar4", READ_OPERAND, 0},
    {"lar5", READ_OPERAND, 0},
    {"lar6", READ_OPERAND, 0},
    {"lar7", READ_OPERAND, 0},
    {}, {}, {}, {},
    {"lra", READ_OPERAND, 0},
    {}, {}, {}
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

char *disAssemble(word36 instruction)
{
    int32 opcode  = GET_OP(instruction);   ///< get opcode
    int32 opcodeX = GET_OPX(instruction);  ///< opcode extension
    a8    address = GET_ADDR(instruction);
    int32 a       = GET_A(instruction);
    //int32 i       = GET_I(instruction);
    int32 tag     = GET_TAG(instruction);

    static char result[132] = "???";
    strcpy(result, "???");

    // get mnemonic ...
    // non-EIS first
    if (!opcodeX)
    {
        if (NonEISopcodes[opcode].mne)
            strcpy(result, NonEISopcodes[opcode].mne);
    }
    else
    {
        // EIS second...
        if (EISopcodes[opcode].mne)
            strcpy(result, EISopcodes[opcode].mne);

        if (EISopcodes[opcode].ndes > 0)
        {
            // XXX need to reconstruct multi-word EIS instruction.

        }
    }

    char buff[64];

    if (a)
    {
        int n = (address >> 15) & 07;
        int offset = address & 077777;

        sprintf(buff, " pr%d|0%o", n, offset);
        strcat (result, buff);
        //return result; // strupr(result);
    }else{

        sprintf(buff, "\t0%06o", address);
        strcat (result, buff);
    }

    // get mod
    strcpy(buff, "");
    for(int n = 0 ; n < 0100 ; n++)
        if (extMods[n].mod)
            if(n == tag)
            {
                strcpy(buff, extMods[n].mod);
                break;
            }

    if (strlen(buff))
    {
        strcat(result, ",");
        strcat(result, buff);
    }

    return result; // strupr(result);
}


char BCD [64] =
  "01234567"
  "89[#@;>?"
  " abcdefg"
  "hi&.](<\\"
  "^jklmnop"
  "qr-$*);'"
  "+/stuvwx"
  "yz_,%=\"!";

int main (int argc, char * argv [])
  {
    uint offset = strtol (argv [1], NULL, 0);
    int fd = open (argv [2], O_RDONLY);
    if (fd < 0)
      {
        fprintf (stderr, "can't open tape\n");
        exit (1);
      }

    printf ("\torg\t0%0o\n", offset);
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
          printf ("\" %06o:\n", offset);
        for (int i = 0; i < 2; i ++)
          {
            word36 w = extr36 (Ypair, i);
#if 0
            if (w & 0200)
              printf ("\" inhibit on\n");
            printf ("\" %012"PRIo64" %s\n", w, disAssemble (w));
            if (w & 0200)
              printf ("\" inhibit on\n");
#else
            char asc [5], bcd [7];
            for (int i = 0; i < 4; i ++)
              {
                unsigned int ch = (unsigned int) ((w >> ((3 - i) * 9)) & 0777);
                asc [i] = isprint (ch) ? (char) ch : ' ';
              }
            asc [4] = 0;
            for (int i = 0; i < 6; i ++)
              {
                unsigned int ch = (unsigned int) ((w >> ((5 - i) * 6)) & 077);
                bcd [i] = BCD [ch];
              }
            bcd [6] = 0;

            char * d = disAssemble (w);
            if (d [0] == '?' ||
		strncmp (d, "scdr", 4) == 0 ||
		strncmp (d, "ad2d", 4) == 0 ||
		strncmp (d, "mvne", 4) == 0 ||
		strncmp (d, "mp2d", 4) == 0 ||
		strncmp (d, "mve", 3) == 0 ||
		strncmp (d, "mlr", 3) == 0 ||
		strncmp (d, "btd", 3) == 0 ||
		strncmp (d, "dv2d", 4) == 0 ||
		strncmp (d, "rpd", 3) == 0 ||
		strncmp (d, "mme", 3) == 0 ||
		strncmp (d, "drl", 3) == 0 ||
		strncmp (d, "ad3d", 4) == 0 ||
		strncmp (d, "stbq", 4) == 0 ||
		strncmp (d, "s4bd", 4) == 0 ||
		strncmp (d, "stca", 4) == 0 ||
		strncmp (d, "stcq", 4) == 0 ||
		strncmp (d, "sb2d", 4) == 0 ||
                w == 0252525252525 || // special cases for blk0
                w == 0474400022120 ||
                w == 0473326336333)
              {
                if (wasInhibit)
                  {
                    printf ("\tinhibit\toff\n");
                    wasInhibit = 0;
                  }
                //printf ("\toct\t%012lo\n", w);
                printf ("\toct\t%012"PRIo64" \"%s\" \"%s\"\n", w, asc, bcd);
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
                printf ("\t%s\t\" %012"PRIo64" \"%s\" \"%s\"\n", disAssemble (w), w, asc, bcd);
              }
#endif
          }
        offset += 2;
      }
    close (fd);
    return 0;
  }
