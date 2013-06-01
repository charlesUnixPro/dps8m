
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
    while (2 * segno < (16 * (DSBR.BND + 1)))
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

_sdw0 *fetchSDW(word15 segno)
{
    word36 SDWeven, SDWodd;
    
    core_read2(DSBR.ADDR + 2 * segno, &SDWeven, &SDWodd);
    
    // even word
    
    _sdw0 *SDW = calloc(1, sizeof(_sdw0));
    
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
    
    int nParams = sscanf(buf, "%s %s %s %s", cmds[0], cmds[1], cmds[2], cmds[3]);
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
    
    int nParams = sscanf(buf, "%s %s %s %s", cmds[0], cmds[1], cmds[2], cmds[3]);
    if (nParams == 2 && !strcasecmp(cmds[0], "segment") && !strcasecmp(cmds[1], "table"))
        return dpsCmd_InitUnpagedSegmentTable();
    if (nParams == 1 && !strcasecmp(cmds[0], "sdwam"))
        return dpsCmd_InitSDWAM();
    //if (nParams == 2 && !strcasecmp(cmds[0], "stack"))
    //    return createStack((int)strtoll(cmds[1], NULL, 8));
    
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
    if (nParams == 2 && !strcasecmp(cmds[0], "remove"))
        return removeSegment(cmds[1]);
    if (nParams == 4 && !strcasecmp(cmds[1], "segref") && !strcasecmp(cmds[2], "remove"))
        return removeSegref(cmds[0], cmds[3]);
    if (nParams == 4 && !strcasecmp(cmds[1], "segdef") && !strcasecmp(cmds[2], "remove"))
        return removeSegdef(cmds[0], cmds[3]);
    
    return SCPE_ARG;
}

//! custom command "segments" - stuff to do with deferred segments
t_stat dpsCmd_Segments (int32 arg, char *buf)
{
    bool bVerbose = !sim_quiet;

    char cmds [8][32];
    memset(cmds, 0, sizeof(cmds));  // clear cmds buffer
    
    /*
     * segments resolve
     * segments load deferred
     * segments remove ???
     */
    int nParams = sscanf(buf, "%s %s %s %s", cmds[0], cmds[1], cmds[2], cmds[3]);
    //if (nParams == 2 && !strcasecmp(cmds[0], "segment") && !strcasecmp(cmds[1], "table"))
    //    return dpsCmd_InitUnpagedSegmentTable();
    //if (nParams == 1 && !strcasecmp(cmds[0], "sdwam"))
    //    return dpsCmd_InitSDWAM();
    if (nParams == 1 && !strcasecmp(cmds[0], "resolve"))
        return resolveLinks(bVerbose);    // resolve external reverences in deferred segments
   
    if (nParams == 2 && !strcasecmp(cmds[0], "load") && !strcasecmp(cmds[1], "deferred"))
        return loadDeferredSegments(bVerbose);    // load all deferred segments
    
    if (nParams == 2 && !strcasecmp(cmds[0], "remove"))
        return removeSegment(cmds[1]);

    if (nParams == 2 && !strcasecmp(cmds[0], "lot") && !strcasecmp(cmds[1], "create"))
        return createLOT(bVerbose);
    if (nParams == 2 && !strcasecmp(cmds[0], "lot") && !strcasecmp(cmds[1], "snap"))
        return snapLOT(bVerbose);

    if (nParams == 3 && !strcasecmp(cmds[0], "create") && !strcasecmp(cmds[1], "stack"))
    {
        int _n = (int)strtoll(cmds[2], NULL, 8);
        return createStack(_n, bVerbose);
    }
    return SCPE_ARG;
}

CTAB dps8_cmds[] =
{
    {"DPSINIT", dpsCmd_Init,        0, "dps8/m initialize stuff ...\n"},
    {"DPSDUMP", dpsCmd_Dump,        0, "dps8/m dump stuff ...\n"},
    {"SEGMENT", dpsCmd_Segment,     0, "dps8/m segment stuff ...\n"},
    {"SEGMENTS", dpsCmd_Segments,   0, "dps8/m segments stuff ...\n"},
    
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
        static char addspec[256];
        strcpy(addspec, cptr);
        
        *strchr(addspec, '|') = ' ';
        
        char seg[256], off[256];
        int params = sscanf(addspec, "%s %s", seg, off);
        if (params != 2)
        {
            printf("parse_addr(): illegal number of parameters\n");
            *optr = cptr;   // signal error
            return 0;
        }
        
        // determine if segment is numeric or symbolic...
        char *endp;
        int segno = (int)strtoll(seg, &endp, 8);
        if (endp == seg)
        {
            // not numeric...
            segment *s = findSegmentNoCase(seg);
            if (s == NULL)
            {
                printf("parse_addr(): segment '%s' not found\n", seg);
                *optr = cptr;   // signal error

                return 0;
            }
            segno = s->segno;
        }
        
        // XXX Add ability to use PR/AR syntax ...
        
        // determine if offset is numeric or symbolic entry point/segdef...
        int offset = (int)strtoll(off, &endp, 8);
        if (endp == off)
        {
            // not numeric...
            segdef *s = findSegdefNoCase(seg, off);
            if (s == NULL)
            {
                printf("parse_addr(): entrypoint '%s' not found in segment '%s'", off, seg);
                *optr = cptr;   // signal error

                return 0;
            }
            offset = s->value;
        }
        
        // if we get here then seg contains a segment# and offset.
        // So, fetch the actual address given the segment & offset ...
        // ... and return this absolute, 24-bit address
        
        t_addr absAddr = getAddress(segno, offset);
        
        // TODO: only luckily does this work FixMe
        *optr = endp;   //cptr + strlen(cptr);
        
        return absAddr;
    }
    
    // No, determine absolute address given by cptr
    return (t_addr)strtol(cptr, optr, 8);
}

static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr simh_addr)
{
    char temp[256];
    bool bFound = getSegmentAddressString(simh_addr, temp);
    if (bFound)
        fprintf(stream, "%s (%08o)", temp, simh_addr);
    else
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
    return SCPE_ARG;
}

/*!  – Based on the switch variable, parse character string cptr for a symbolic value val at the specified addr
 in unit uptr.
 */
t_stat parse_sym (char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sswitch)
{
    return SCPE_ARG;
}

