/**
 * \file dps8_sys.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"

word36 *M = NULL;                                          /*!< memory */

char sim_name[] = "dps-8/m";

/* CPU data structures
 
 cpu_dev      CPU device descriptor
 cpu_unit     CPU unit
 cpu_reg      CPU register list
 cpu_mod      CPU modifier list
 */

extern REG cpu_reg[]; /*!< CPU register list */
REG *sim_PC = &cpu_reg[0];

int32 sim_emax = 4; ///< some EIS can take up to 4-words

/*! CPU unit */
UNIT cpu_unit = { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE) };

/*! CPU modifier list */
MTAB cpu_mod[] = {
    { UNIT_V_UF, 0, "STD", "STD", NULL },
    //{ MTAB_XTD|MTAB_VDV, 0, "SPECIAL", NULL, NULL, &spec_disp },
    { 0 }
};

static DEBTAB cpu_dt[] = {
    { "TRACE",      DBG_TRACE       },
    { "TRACEEX",    DBG_TRACEEXT    },
    { "MESSAGES",   DBG_MSG         },
    { "REGDUMPAQI", DBG_REGDUMPAQI  },
    { "REGDUMPIDX", DBG_REGDUMPIDX  },
    { "REGDUMPPR",  DBG_REGDUMPPR   },
    { "REGDUMPADR", DBG_REGDUMPADR  },
    { "REGDUMPPPR", DBG_REGDUMPPPR  },
    { "REGDUMPDSBR",DBG_REGDUMPDSBR },
    { "REGDUMPFLT", DBG_REGDUMPFLT  },
    { "REGDUMP",    DBG_REGDUMP     }, // don't move as it messes up DBG message
    { "ADDRMOD",    DBG_ADDRMOD     },
    { "APPENDING",  DBG_APPENDING   },
    { NULL,         0               }
};

/*! CPU device descriptor */
DEVICE cpu_dev = {
    "CPU",          /*!< name */
    &cpu_unit,      /*!< units */
    cpu_reg,        /*!< registers */
    cpu_mod,        /*!< modifiers */
    1,              /*!< #units */
    8,              /*!< address radix */
    PASIZE,         /*!< address width */
    1,              /*!< addr increment */
    8,              /*!< data radix */
    36,             /*!< data width */
    &cpu_ex,        /*!< examine routine */
    &cpu_dep,       /*!< deposit routine */
    &cpu_reset,     /*!< reset routine */
    NULL,           /*!< boot routine */
    NULL,           /*!< attach routine */
    NULL,           /*!< detach routine */
    NULL,           /*!< context */
    DEV_DEBUG,      /*!< device flags */
    0,              /*!< debug control flags */
    cpu_dt,         /*!< debug flag names */
    NULL,           /*!< memory size change */
    NULL            /*!< logical name */
};


DEVICE *sim_devices[] = {
    &cpu_dev,
    NULL
};

const char *sim_stop_messages[] = {
    "Unknown error",
    "Unimplemented Opcode",
    "DIS instruction",
    "Breakpoint",
    "Invalid Opcode",
    "Stop code - 5",
};

//t_stat spec_disp (FILE *st, UNIT *uptr, int value, void *desc)
//{
//    printf("In spec_disp()....\n");
//    return SCPE_OK;
//}


/*! Reset routine */
t_stat cpu_reset (DEVICE *dptr)
{
    if (M)
        free(M);
    
    M = (word36 *) calloc (MEMSIZE, sizeof (word36));
    if (M == NULL)
        return SCPE_MEM;
    
    rIC = 0;
    rA = 0;
    rQ = 0;
    XECD = 0;
    
    PPR.IC = 0;
    PPR.PRR = 0;
    PPR.PSR = 0;
    PPR.P = 1;
    
    processorCycle = UNKNOWN_CYCLE;
    processorAddressingMode = ABSOLUTE_MODE;
    
    sim_brk_types = sim_brk_dflt = SWMASK ('E');

    cpuCycles = 0;
    
    // XXX free up previous deferred segments (if any)
    
    return SCPE_OK;
}

/*! Memory examine */
t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
    if (addr >= MEMSIZE)
        return SCPE_NXM;
    if (vptr != NULL)
        *vptr = M[addr] & DMASK;
    return SCPE_OK;
}

/*! Memory deposit */
t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
    if (addr >= MEMSIZE) return SCPE_NXM;
    M[addr] = val & DMASK;
    return SCPE_OK;
}


/*!
 * initialize segment table according to the contents of DSBR ...
 */
t_stat dpsCmd_InitUnpagedSegmentTable ()
{
    if (DSBR.U == 0)
    {
        fprintf(stderr, "Cannot initialize unpaged segment table because DSBR.U says it is \"paged\"\n");
        return SCPE_OK;    // need a better return value
    }
    
    if (DSBR.ADDR == 0) // DSBR *probably* not initialized. Issue warning and ask....
        if (!get_yn ("DSBR *probably* uninitialized (DSBR.ADDR == 0). Proceed anyway [N]?", FALSE))
            return SCPE_OK;
    
    
    word15 segno = 0;
    while (2 * segno < 16 * (DSBR.BND + 1))
    {
        //generate target segment SDW for DSBR.ADDR + 2 * segno.
        word24 a = DSBR.ADDR + 2 * segno;
        
        // just fill with 0's for now .....
        core_write(a + 0, 0);
        core_write(a + 1, 0);
        
        segno += 1; // onto next segment SDW
    }
    
    fprintf(stderr, "zero-initialized segments 0 .. %d\n", segno - 1);
    return SCPE_OK;
}

t_stat dpsCmd_InitSDWAM ()
{
    memset(SDWAM, 0, sizeof(SDWAM));
    
    fprintf(stderr, "zero-initialized SDWAM\n");
    return SCPE_OK;
}

PRIVATE
_sdw0 *fetchSDW(word15 segno)
{
    word36 SDWeven, SDWodd;
    
    core_read2(DSBR.ADDR + 2 * segno, &SDWeven, &SDWodd);
    
    // even word
    
    _sdw0 *SDW = malloc(sizeof(_sdw0));
    
    SDW->ADDR = (SDWeven >> 12) & 077777777;
    SDW->R1 = (SDWeven >> 9) & 7;
    SDW->R2 = (SDWeven >> 6) & 7;
    SDW->R3 = (SDWeven >> 3) & 7;
    SDW->F = TSTBIT(SDWeven, 2);
    SDW->FC = SDWeven & 3;
    
    // odd word
    SDW->BOUND = (SDWodd >> 21) & 037777;
    SDW->R = TSTBIT(SDWodd, 20);
    SDW->E = TSTBIT(SDWodd, 19);
    SDW->W = TSTBIT(SDWodd, 18);
    SDW->P = TSTBIT(SDWodd, 17);
    SDW->U = TSTBIT(SDWodd, 16);
    SDW->G = TSTBIT(SDWodd, 15);
    SDW->C = TSTBIT(SDWodd, 14);
    SDW->EB = SDWodd & 037777;
    
    return SDW;
}

char *strDSBR()
{
    static char buff[256];
    sprintf(buff, "DSBR: ADDR=%06o BND=%05o U=%o STACK=%04o", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK);
    return buff;
}

PRIVATE
void printDSBR(FILE *f)
{
    fprintf(f, "%s\n", strDSBR());
}


char *strSDW0(_sdw0 *SDW)
{
    static char buff[256];
    
    //if (SDW->ADDR == 0 && SDW->BOUND == 0) // need a better test
    if (!SDW->F) 
        sprintf(buff, "*** Uninitialized ***");
    else
        sprintf(buff, "ADDR=%06o R1=%o R2=%o R3=%o F=%o FC=%o BOUND=%o R=%o E=%o W=%o P=%o U=%o G=%o C=%o EB=%o",
                SDW->ADDR, SDW->R1, SDW->R2, SDW->R3, SDW->F, SDW->FC, SDW->BOUND, SDW->R, SDW->E, SDW->W,
                SDW->P, SDW->U, SDW->G, SDW->C, SDW->EB);
    return buff;
}

PRIVATE
void printSDW0(FILE *f, _sdw0 *SDW)
{
    fprintf(f, "%s\n", strSDW0(SDW));
}

t_stat dpsCmd_DumpSegmentTable()
{
    fprintf(stderr, "*** Descriptor Segment Base Register (DSBR) ***\n");
    printDSBR(stderr);
    fprintf(stderr, "*** Descriptor Segment Table ***\n");
    for(word15 segno = 0; 2 * segno < 16 * (DSBR.BND + 1); segno += 1)
    {
        fprintf(stderr, "Seg %d - ", segno);
        _sdw0 *s = fetchSDW(segno);
        printSDW0(stderr, s);
        
        free(s);
    }
    return SCPE_OK;
}

//! custom command "dump"
t_stat dpsCmd_Dump (int32 arg, char *buf)
{
    char cmds [256][256];
    memset(cmds, 0, sizeof(cmds));  // clear cmds buffer
    
    int nParams = sscanf(buf, "%s %s %s %s", &cmds[0][0], &cmds[1][0], &cmds[2][0], &cmds[3][0]);
    if (nParams == 2 && !strcasecmp(cmds[0], "segment") && !strcasecmp(cmds[1], "table"))
        return dpsCmd_DumpSegmentTable();
    if (nParams == 1 && !strcasecmp(cmds[0], "sdwam"))
        return dumpSDWAM();
    
    return SCPE_OK;
}

//! custom command "init"
t_stat dpsCmd_Init (int32 arg, char *buf)
{
    char cmds [8][32];
    memset(cmds, 0, sizeof(cmds));  // clear cmds buffer
    
    int nParams = sscanf(buf, "%s %s %s %s", &cmds[0][0], &cmds[1][0], &cmds[2][0], &cmds[3][0]);
    if (nParams == 2 && !strcasecmp(cmds[0], "segment") && !strcasecmp(cmds[1], "table"))
        return dpsCmd_InitUnpagedSegmentTable();
    if (nParams == 1 && !strcasecmp(cmds[0], "sdwam"))
        return dpsCmd_InitSDWAM();
    
    return SCPE_OK;
}

//! custom command "segment" - stuff to do with deferred segments
t_stat dpsCmd_Segment (int32 arg, char *buf)
{
    char cmds [8][32];
    memset(cmds, 0, sizeof(cmds));  // clear cmds buffer
    
    /*
      cmds   0     1      2     3
     segment ??? remove
     segment ??? segref remove ????
     segment ??? segdef remove ????
     
     */
    int nParams = sscanf(buf, "%s %s %s %s %s", cmds[0], cmds[1], cmds[2], cmds[3], cmds[4]);
    if (nParams == 2 && !strcasecmp(cmds[1], "remove"))
        return removeSegment(cmds[0]);
    if (nParams == 4 && !strcasecmp(cmds[1], "segref") && !strcasecmp(cmds[2], "remove"))
        return removeSegref(cmds[0], cmds[3]);
    if (nParams == 4 && !strcasecmp(cmds[1], "segdef") && !strcasecmp(cmds[2], "remove"))
        return removeSegdef(cmds[0], cmds[3]);
    
    return SCPE_OK;
}

//! custom command "segments" - stuff to do with deferred segments
t_stat dpsCmd_Segments (int32 arg, char *buf)
{
    char cmds [8][32];
    memset(cmds, 0, sizeof(cmds));  // clear cmds buffer
    
    /*
     * segments resolve
     */
    int nParams = sscanf(buf, "%s %s %s %s", cmds[0], cmds[1], cmds[2], cmds[3]);
    //if (nParams == 2 && !strcasecmp(cmds[0], "segment") && !strcasecmp(cmds[1], "table"))
    //    return dpsCmd_InitUnpagedSegmentTable();
    //if (nParams == 1 && !strcasecmp(cmds[0], "sdwam"))
    //    return dpsCmd_InitSDWAM();
    if (nParams == 1 && !strcasecmp(cmds[0], "resolve"))
        return resolveLinks();    // resolve external reverences in deferred segments
   
    if (nParams == 2 && !strcasecmp(cmds[0], "load") && !strcasecmp(cmds[1], "deferred"))
        return loadDeferredSegments();    // load all deferred segments
    
    return SCPE_OK;
}

CTAB dps8_cmds[] =
{
    {"DPSINIT", dpsCmd_Init,        0, "dps8/m initialize stuff ..."},
    {"DPSDUMP", dpsCmd_Dump,        0, "dps8/m dump stuff ..."},
    {"SEGMENT", dpsCmd_Segment,     0, "dps8/m segment stuff ..."},
    {"SEGMENTS", dpsCmd_Segments,   0, "dps8/m segments stuff ..."},
    
    { NULL, NULL, 0, NULL}
};


/*!
 \brief special dps8 VM commands ....
 
 For greater flexibility, SCP provides some optional interfaces that can be used to extend its command input, command processing, and command post-processing capabilities. These interfaces are strictly optional
 and are off by default. Using them requires intimate knowledge of how SCP functions internally and is not recommended to the novice VM writer.
 
 Guess I shouldn't use these then :)
 */

static t_addr parse_addr(DEVICE *dptr, char *cptr, char **optr);
static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr addr);

extern CTAB *sim_vm_cmd;
void dps8_init(void)    //CustomCmds(void)
{
    // special dps8 initialization stuff that cant be done in reset, etc .....
    sim_vm_parse_addr = parse_addr;
    sim_vm_fprint_addr = fprint_addr;

    sim_vm_cmd = dps8_cmds;
}

void (*sim_vm_init) (void) = &dps8_init;    //CustomCmds;

extern segment *findSegment(char *);

static t_addr parse_addr(DEVICE *dptr, char *cptr, char **optr)
{
    // a segment reference?
    if (strchr(cptr, '|'))
    {
        char addspec[256];
        strcpy(addspec, cptr);
        
        *strchr(addspec, '|') = ' ';
        
        char seg[256], off[256];
        int params = sscanf(addspec, "%s %s", seg, off);
        if (params != 2)
        {
            printf("parse_addr(): illegal number of parameters");
            return 0;
        }
        
        // determine if segment is numeric or symbolic...
        char *endp;
        int segno = (int)strtoll(seg, &endp, 0);
        if (endp == seg) // XXX don't think this is right
        {
            // not numeric...
            segment *s = findSegment(seg);
            if (s == NULL)
            {
                printf("parse_addr(): segment '%s' not found", seg);
                return 0;
            }
            segno = s->segno;
        }
        
        // determine if offset is numeric or symbolic entry point/segdef...
        int offset = (int)strtoll(off, &endp, 0);
        if (endp == off)
        {
            // not numeric...
            segdef *s = findSegdef(seg, off);
            if (s == NULL)
            {
                printf("parse_addr(): entrypoint '%s' not found in segment '%s'", off, seg);
                return 0;
            }
            offset = s->value;
        }
        
        // if we get here then seg contains a segment# and offset.
        // So, fetch the actual address given the segment & offset ...
        // ... and return this absolute, 24-bit address
        
        t_addr absAddr = getAddress(segno, offset);
        
        *optr = cptr + strlen(cptr);
        
        return absAddr;
    }
    
    // No, determine absolute address given by cptr
    return (t_addr)strtol(cptr, optr, 8);
}

static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr simh_addr)
{
    fprintf(stream, "%06o", simh_addr);
}

/*! Based on the switch variable, symbolically output to stream ofile the data in array val at the specified addr
 in unit uptr.
 */
t_stat fprint_sym (FILE *ofile, t_addr addr, t_value *val, UNIT *uptr, int32 sswitch)
{
    if (uptr == &cpu_unit)
    {
        fprintf(ofile, "%012llo", *val);
        return SCPE_OK;
    }
    else if (sswitch & SIM_SW_REG)
    {
        // Print register
        REG* regp = (void*) uptr;
        char *reg_name = regp->name;
    
        if (!strcasecmp(reg_name, "ir"))
            fprintf(ofile, "%s%06o", dumpFlags(rIR), (word18)*val);
        
        return SCPE_OK;
    } else
        return SCPE_ARG;
}

/*!  – Based on the switch variable, parse character string cptr for a symbolic value val at the specified addr
 in unit uptr.
 */
t_stat parse_sym (char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sswitch)
{
    return SCPE_ARG;
}

#if LATER
/*
 * Some of the following is stolen (w/ modifications) from Michael Mondy's emulator
 *
 * https://github.com/MichaelMondy/multics-emul/wiki
 *
 * Copyright (c) 2007-2013 Michael Mondy
 *
 * This software is made available under the terms of the
 * ICU License -- ICU 1.8.1 and later.
 * See the LICENSE file at the top-level directory of this distribution and
 * at http://example.org/project/LICENSE.
 *
 */

//=============================================================================

/* These are from SIMH, but not listed in sim_defs.h */
extern t_addr (*sim_vm_parse_addr)(DEVICE *, char *, char **);
extern void (*sim_vm_fprint_addr)(FILE *, DEVICE *, t_addr);
extern uint32 sim_brk_types, sim_brk_dflt, sim_brk_summ; /* breakpoint info */

/*
 * fprint_sym()
 *
 * Called by SIMH to print a memory location.
 *
 */

t_stat fprint_sym (FILE *ofile, t_addr simh_addr, t_value *val, UNIT *uptr, int32 sw)
{
    // log_msg(INFO_MSG, "SYS:fprint_sym", "addr is %012llo; val-ptr is %p, uptr is %p\n", simh_addr, val, uptr);
    
    if (uptr == &cpu_unit) {
        // memory request -- print memory specified by SIMH
        addr_modes_t mode;
        unsigned segno;
        unsigned offset;
        if (addr_simh_to_emul(simh_addr, &mode, &segno, &offset) != 0)
            return SCPE_ARG;
        unsigned abs_addr;
        if (addr_any_to_abs(&abs_addr, mode, segno, offset) != 0)
            return SCPE_ARG;
        // note that parse_addr() was called by SIMH to determine the absolute addr.
        static int need_init = 1;
        if (need_init) {
            need_init = 0;
            prior_lineno = 0;
            prior_line = NULL;
        }
        /* First print matching source line if we're dumping instructions */
        if (sw & SWMASK('M')) {
            // M -> instr -- print matching source line if we have one
            const char* line;
            int lineno;
            seginfo_find_line(segno, offset, &line, &lineno);
            if (line != NULL && prior_lineno != lineno && prior_line != line) {
                fprintf(ofile, "\r\n");
                fprint_addr(ofile, NULL, simh_addr);    // UNIT doesn't include a reference to a DEVICE
                fprintf(ofile, ":\t");
                fprintf(ofile, "Line %d: %s\n", lineno, line);
                // fprintf(ofile, "%06o:\t", abs_addr); // BUG: print seg|offset too
                fprint_addr(ofile, NULL, simh_addr);    // UNIT doesn't include a reference to a DEVICE
                fprintf(ofile, ":\t");
                prior_lineno = lineno;
                prior_line = line;
            }
        }
        /* Next, we always output the numeric value */
        t_addr alow = abs_addr;
        t_addr ahi = abs_addr;
        fprintf(ofile, "%012llo", Mem[abs_addr]);
        if (sw & SWMASK('S') || (sw & SWMASK('X'))) {
            // SDWs are two words
            ++ ahi;
            fprintf(ofile, " %012llo", Mem[ahi]);
        }
        /* User may request (A)scii in addition to another format */
        if (sw & SWMASK('A')) {
            for (t_addr a = alow; a <= ahi; ++ a) {
                t_uint64 word = Mem[a];
                fprintf(ofile, " ");
                for (int i = 0; i < 4; ++i) {
                    uint c = word >> 27;
                    word = (word << 9) & MASKBITS(36);
                    if (c <= 0177 && isprint(c)) {
                        fprintf(ofile, "  '%c'", c);
                    } else {
                        fprintf(ofile, " \\%03o", c);
                    }
                }
            }
        }
        /* See if any other format was requested (but don't bother honoring multiple formats */
        if (sw & SWMASK('A')) {
            // already done
        } else if (sw & SWMASK('L')) {
            // L -> LPW
            fprintf(ofile, " %s", print_lpw(abs_addr));
        } else if (sw & SWMASK('M')) {
            // M -> instr
            char *instr = print_instr(Mem[abs_addr]);
            fprintf(ofile, " %s", instr);
        } else if (sw & SWMASK('P') || (sw & SWMASK('Y'))) {
            // P/Y -> PTW
            char *s = print_ptw(Mem[abs_addr]);
            fprintf(ofile, " %s", s);
        } else if (sw & SWMASK('S') || (sw & SWMASK('X'))) {
            // S/X -> SDW
            char *s = print_sdw(Mem[abs_addr], Mem[abs_addr+1]);
            fprintf(ofile, " %s", s);
        } else if (sw & SWMASK('W')) {
            // W -> DCW
            char *s = print_dcw(abs_addr);
            fprintf(ofile, " %s", s);
        } else if (sw) {
            return SCPE_ARG;
        } else {
            // we already printed the numeric value, so nothing else to do
        }
        fflush(ofile);
        return SCPE_OK;
    } else if (sw & SIM_SW_REG) {
        // Print register
        REG* regp = (void*) uptr;
        // NOTE: We could also check regp->name to detect which registers should have special formatting
        if (regp && (regp->flags&REG_USER2)) {
            // PR registers
            // NOTE: Another implementation would be to have the value of each register always be its
            // index -- e.g. saved_ar_pr[5] would hold value 5.  Then the examine and deposit routines
            // could simply operate on the associated AR_PR registers.
            AR_PR_t pr;
            pr.PR.snr = *val & 077777;          // 15 bits
            pr.PR.rnr = (*val >> 15) & 07;      //  3 bits
            pr.PR.bitno = (*val >> 18) & 077;   //  6 bits
            pr.wordno = (*val >> 24);           // 18 bits
            pr.AR.charno = pr.PR.bitno / 9;
            pr.AR.bitno = pr.PR.bitno % 9;
            fprintf(ofile, "[ring %0o, address %0o|%0o, bitno %d]", pr.PR.rnr, pr.PR.snr, pr.wordno, pr.PR.bitno);
            fflush(ofile);
            return SCPE_OK;
        } else if (regp && (regp->flags&REG_USER1)) {
            // IR register
            fprintf(ofile, "%s", bin2text(*val, 18));
            IR_t ir;
            load_IR(&ir, *val);
            fprintf(ofile, " %s", ir2text(&ir));
            fflush(ofile);
            return SCPE_OK;
        } else if (regp && strcmp(regp->name,"PPR") == 0) {
            PPR_t ppr;
            load_PPR(*val, &ppr);
            fprintf(ofile, "[ring %0o, address %0o|%0o, priv %d]", ppr.PRR, ppr.PSR, ppr.IC, ppr.P);
            fflush(ofile);
            return SCPE_OK;
        } else
            return SCPE_ARG;
    } else
        return SCPE_ARG;
}

//=============================================================================

t_stat parse_sym (char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sw)
{
    log_msg(ERR_MSG, "SYS::parse_sym", "unimplemented\n");
    return SCPE_ARG;
}


static int inline is_octal_digit(char x)
{
    return isdigit(x) && x != '8' && x != '9';
}

//=============================================================================

/*
 * parse_addr()
 *
 * SIMH calls this function to parse an address.
 *
 * We return a packed format that encodes addressing mode, segment, and offset.
 *
 */
void out_msg(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    
    FILE *stream = (sim_log != NULL) ? sim_log : stdout;
    crnl_out(stream, format, ap);
    if (sim_deb != NULL)
        crnl_out(sim_deb, format, ap);
}

static t_addr parse_addr(DEVICE *dptr, char *cptr, char **optr)
{
    
    // Obsolete comments:
    //      SIMH wants the absolute address.   However, SIMH may pass the
    //      resulting address to fprint_sym() or other simulator functions
    //      that need to know the segment and offset.   So, we set the
    //      following globals to in order to communicate that info:
    //          int last_parsed_seg, int last_parsed_offset, t_addr
    //          last_parsed_addr
    
    // BUG/TODO: cleanup the last_parsed gunk that's no longer needed
    addr_modes_t last_parsed_mode;
    int last_parsed_seg;
    int last_parsed_offset;
    t_addr last_parsed_addr;
    
    char *cptr_orig = cptr;
    char debug_strp[1000]; strcpy(debug_strp, cptr);
    *optr = cptr;
    int force_abs = 0;
    int force_seg = 0;
    
    char *offsetp;
    int seg = -1;
    int pr = -1;
    unsigned int offset = 0;
    if ((offsetp = strchr(cptr, '|')) != NULL || ((offsetp = strchr(cptr, '$')) != NULL)) {
        // accept an octal segment number
        force_seg = 1;
        //log_msg(WARN_MSG, "parse_addr", "arg is '%s'\n", cptr);
        if (cptr[0] == 'P' && cptr[1] == 'R' && is_octal_digit(cptr[2]) && cptr+3 == offsetp) {
            // handle things like pr4|2,x7
            pr = cptr[2] - '0'; // BUG: ascii only
            seg = AR_PR[pr].PR.snr;
            offset = AR_PR[pr].wordno;
            //log_msg(WARN_MSG, "parse_addr", "PR[%d] uses 0%o|0%o\n", pr, seg, offset);
            cptr += 4;
        } else {
            if (!is_octal_digit(*cptr)) {
                out_msg("ERROR: Non octal digit starting at: %s\n.", cptr);
                return 0;
            }
            sscanf(cptr, "%o", (unsigned int *) &seg);
            cptr += strspn(cptr, "01234567");
            if (cptr != offsetp) {
                out_msg("DEBUG: parse_addr: non octal digit within: %s\n.", cptr);
                return 0;
            }
            ++cptr;
        }
    } else
        if (*cptr == '#' || *cptr == '=') {     // SIMH won't let us use '=', so we provide '#'
            force_abs = 1;  // ignore TPR.TRS, interpret as absolute mode reference
            ++ cptr;
        }
    
    if (*cptr == 'X' && is_octal_digit(cptr[1]) && cptr[2] == '*') {
        int n = cptr[1] - '0';  // BUG: ascii only
        offset += reg_X[n];
        cptr += 3;
    } else {
        unsigned int off;
        sscanf(cptr, "%o", &off);
        offset += off;
        cptr += strspn(cptr, "01234567");
    }
    int mod_x = 0;
    if (cptr[0] == ',' && cptr[1] == 'X' && is_octal_digit(cptr[2])) {
        int n = cptr[2] - '0';  // BUG: ascii only
        mod_x = reg_X[n];
        offset += mod_x;
        cptr += 3;
    }
#if 0
    int is_indir;
    if ((is_indir = cptr[0] == ',' && cptr[1] == '*'))
        cptr += 2;
#endif
    
    prior_line = NULL;
    
    // uint addr;
    if (force_abs || (seg == -1 && get_addr_mode() == ABSOLUTE_mode)) {
        last_parsed_mode = ABSOLUTE_mode;
        *optr = cptr;
        // addr = offset;
        last_parsed_seg = -1;
        last_parsed_offset = offset;
        // last_parsed_addr = addr;
    } else {
        last_parsed_mode = APPEND_mode;
        last_parsed_seg = (seg == -1) ? TPR.TSR : seg;
        last_parsed_offset = offset;
        *optr = cptr;
    }
    
#if 0
    // This is too simple -- need to handle ITS/ITP tag fields, etc
    if (is_indir) {
        t_uint64 word;
        if (fetch_abs_word(addr, &word) != 0)
            return 0;
        addr = word & MASKBITS(24);
    }
#endif
    
#if 0
    if (last_parsed_mode == APPEND_mode)
        log_msg(INFO_MSG, "SYS::parse_addr", "String '%s' is %03o|%06o\n", debug_strp, last_parsed_seg, last_parsed_offset);
    else
        log_msg(INFO_MSG, "SYS::parse_addr", "String '%s' is %08o\n", debug_strp, last_parsed_offset);
    log_msg(INFO_MSG, "SYS::parse_addr", "Used %d chars; residue is '%s'.\n", cptr - cptr_orig, *optr);
#endif
    return addr_emul_to_simh(last_parsed_mode, last_parsed_seg, last_parsed_offset);
}

//=============================================================================

/*
 * fprint_addr()
 *
 * Called by SIMH to display an address
 *
 * Note that all addresses given by the simulator to SIMH are in a packed
 * format.
 */

static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr simh_addr)
{
    // log_msg(INFO_MSG, "SYS:fprint_addr", "Device is %s; addr is %012llo; dptr is %p\n", dptr->name, simh_addr, dptr);
    
    addr_modes_t mode;
    unsigned segno;
    unsigned offset;
    if (addr_simh_to_emul(simh_addr, &mode, &segno, &offset) != 0)
        fprintf(stream, "<<<%08llo>>>", simh_addr);
    else
        if (mode == APPEND_mode)
            fprintf(stream, "%03o|%06o", segno, offset);
        else if (mode == BAR_mode)
            fprintf(stream, "BAR<<<%llo->%08o>>>", simh_addr, offset);  // BUG
        else
            fprintf(stream, "%08o", offset);
}

//=============================================================================

/*
 * addr_emul_to_simh()
 *
 * Encode an address for handoff to SIMH.
 *
 * The emulator gives SIMH a "packed" address form that encodes mode,
 * segment, and offset
 *
 */

t_uint64 addr_emul_to_simh(addr_modes_t mode, unsigned segno, unsigned offset)
{
    if (mode == APPEND_mode) {
        if (offset >> 18 != 0) {
            log_msg(NOTIFY_MSG, "SYS::addr", "EMUL %03o|%06o overflows 18-bit offset.\n", segno, offset);
            cancel_run(STOP_BUG);
        }
        if (segno >> 15 != 0) {
            log_msg(NOTIFY_MSG, "SYS::addr", "EMUL %03o|%06o overflows 15-bit segment.\n", segno, offset);
            cancel_run(STOP_BUG);
        }
    } else {
        if (offset >> 24 != 0) {
            log_msg(NOTIFY_MSG, "SYS::addr", "EMUL %08o overflows 24-bit address.\n", offset);
            cancel_run(STOP_BUG);
        }
        segno = 0;
    }
    
    t_uint64 addr = offset & MASKBITS(24);
    addr |= (t_uint64) (segno & MASKBITS(15)) << 25;
    addr |= (t_uint64) (mode & MASKBITS(2)) << 41;
#if 0
    if (mode == APPEND_mode)
        log_msg(INFO_MSG, "SYS::addr", "EMUL %03o|%06o packs to %012llo\n", segno, offset, addr);
    else
        log_msg(INFO_MSG, "SYS::addr", "EMUL %08o packs to %012llo\n", offset, addr);
#endif
    return addr;
}

//=============================================================================

/*
 * addr_simh_to_emul()
 *
 * Decode an address returned by SIMH.
 *
 * The emulator gives SIMH a "packed" address form that encodes mode,
 * segment, and offset.
 *
 */

int addr_simh_to_emul(t_uint64 addr, addr_modes_t *modep, unsigned *segnop, unsigned *offsetp)
{
    if (((addr >> 24) & 1) != 0) {
        log_msg(NOTIFY_MSG, "SYS::addr", "SIMH %012llo has an overflow on offset #.\n", addr);
        return 1;
    }
    if (((addr >> 40) & 1) != 0) {
        log_msg(NOTIFY_MSG, "SYS::addr", "SIMH %012llo has an overflow on segment #.\n", addr);
        return 1;
    }
    *offsetp = addr & MASKBITS(24);
    *segnop = (addr >> 25) & MASKBITS(15);
    *modep = (addr >> 41) & MASKBITS(2);
    if (*modep == APPEND_mode)
        if (*offsetp >> 18 != 0) {
            log_msg(NOTIFY_MSG, "SYS::addr", "SIMH %012llo aka %03o|%06o overflows 18-bit offset.\n", addr, *segnop, *offsetp);
            return 1;
        }
#if 0
    if (*modep == APPEND_mode)
        log_msg(INFO_MSG, "SYS::addr", "SIMH %012llo is %03o|%06o\n", addr, *segnop, *offsetp);
    else
        log_msg(INFO_MSG, "SYS::addr", "SIMH %012llo is %08o\n", addr, *offsetp);
#endif
    return 0;
}

//=============================================================================
#endif
