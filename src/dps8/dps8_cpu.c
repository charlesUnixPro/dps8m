 /**
 * \file dps8_cpu.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"
#include "dps8_utils.h"

// XXX Use this when we assume there is only a single unit
#define ASSUME0 0

void cpu_reset_array (void);

/* CPU data structures
 
 cpu_dev      CPU device descriptor
 cpu_unit     CPU unit
 cpu_reg      CPU register list
 cpu_mod      CPU modifier list
 */

#define N_CPU_UNITS 1
// The DPS8M had only 4 ports

UNIT cpu_unit [N_CPU_UNITS] = {{ UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE) }};
#define UNIT_NUM(uptr) ((uptr) - cpu_unit)
static t_stat cpu_show_config(FILE *st, UNIT *uptr, int val, void *desc);
static t_stat cpu_set_config (UNIT * uptr, int32 value, char * cptr, void * desc);
/*! CPU modifier list */
MTAB cpu_mod[] = {
    /* { UNIT_V_UF, 0, "STD", "STD", NULL }, */
    //{ MTAB_XTD|MTAB_VDV, 0, "SPECIAL", NULL, NULL, &spec_disp },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO /* | MTAB_VALR */, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      cpu_set_config,         /* validation routine */
      cpu_show_config, /* display routine */
      NULL          /* value descriptor */
    },
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

    { "NOTIFY",     DBG_NOTIFY      },
    { "INFO",       DBG_INFO        },
    { "ERR",        DBG_ERR         },
    { "WARN",       DBG_WARN        },
    { "DEBUG",      DBG_DEBUG       },
    { "ALL",        DBG_ALL         }, // don't move as it messes up DBG message

    { "FAULT",      DBG_FAULT       },
    { "INTR",       DBG_INTR       },

    { NULL,         0               }
};

// This is part of the simh interface
const char *sim_stop_messages[] = {
    "Unknown error",           // STOP_UNK
    "Unimplemented Opcode",    // STOP_UNIMP
    "DIS instruction",         // STOP_DIS
    "Breakpoint",              // STOP_BKPT
    "Invalid Opcode",          // STOP_INVOP
    "Stop code - 5",           // STOP_5
    "BUG",                     // STOP_BUG
    "WARNING",                  // STOP_WARN
    "Fault cascade",           // STOP_FLT_CASCADE
    "Halt",                    // STOP_HALT
};

/* End of simh interface */

/* Processor configuration switches 
 *
 * From AM81-04 Multics System Maintainance Procedures
 *
 * "A level 68 IOM system may contain a maximum of 7 CPUs, 4 IOMs, 8 SCUs and 16MW of memory
 * [CAC]: but AN87 says multics only supports two IOMs
 * 
 * ASSIGNMENT: 3 toggle switches determine the base address of the SCU connected
 * to the port. The base address (in KW) is the product of this number and the value
 * defined by the STORE SIZE patch plug for the port.
 *
 * ADDRESS RANGE: toggle FULL/HALF. Determines the size of the SCU as full or half
 * of the STORE SIZE patch.
 *
 * PORT ENABLE: (4? toggles)
 *
 * INITIALIZE ENABLE: (4? toggles) These switches enable the receipt of an 
 * initialize signal from the SCU connected to the ports. This signal is used
 * during the first part of bootload to set all CPUs to a known (idle) state.
 * The switch for each port connected to an SCU should be ON, otherwise off.
 *
 * INTERLACE: ... All INTERLACE switches should be OFF for Multics operation.
 *

 */


//t_stat spec_disp (FILE *st, UNIT *uptr, int value, void *desc)
//{
//    printf("In spec_disp()....\n");
//    return SCPE_OK;
//}

/*
 * init_opcodes()
 *
 * This initializes the is_eis[] array which we use to detect whether or
 * not an instruction is an EIS instruction.
 *
 * TODO: Change the array values to show how many operand words are
 * used.  This would allow for better symbolic disassembly.
 *
 * BUG: unimplemented instructions may not be represented
 */

int is_eis[1024];    // hack

void init_opcodes (void)
{
    memset(is_eis, 0, sizeof(is_eis));
    
    is_eis[(opcode1_cmpc<<1)|1] = 1;
    is_eis[(opcode1_scd<<1)|1] = 1;
    is_eis[(opcode1_scdr<<1)|1] = 1;
    is_eis[(opcode1_scm<<1)|1] = 1;
    is_eis[(opcode1_scmr<<1)|1] = 1;
    is_eis[(opcode1_tct<<1)|1] = 1;
    is_eis[(opcode1_tctr<<1)|1] = 1;
    is_eis[(opcode1_mlr<<1)|1] = 1;
    is_eis[(opcode1_mrl<<1)|1] = 1;
    is_eis[(opcode1_mve<<1)|1] = 1;
    is_eis[(opcode1_mvt<<1)|1] = 1;
    is_eis[(opcode1_cmpn<<1)|1] = 1;
    is_eis[(opcode1_mvn<<1)|1] = 1;
    is_eis[(opcode1_mvne<<1)|1] = 1;
    is_eis[(opcode1_csl<<1)|1] = 1;
    is_eis[(opcode1_csr<<1)|1] = 1;
    is_eis[(opcode1_cmpb<<1)|1] = 1;
    is_eis[(opcode1_sztl<<1)|1] = 1;
    is_eis[(opcode1_sztr<<1)|1] = 1;
    is_eis[(opcode1_btd<<1)|1] = 1;
    is_eis[(opcode1_dtb<<1)|1] = 1;
    is_eis[(opcode1_dv3d<<1)|1] = 1;
}


/*!
 * initialize segment table according to the contents of DSBR ...
 */
t_stat dpsCmd_InitUnpagedSegmentTable ()
{
    if (DSBR.U == 0)
    {
        sim_printf("Cannot initialize unpaged segment table because DSBR.U says it is \"paged\"\n");
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
    
    if (!sim_quiet) sim_printf("zero-initialized segments 0 .. %d\n", segno - 1);
    return SCPE_OK;
}

t_stat dpsCmd_InitSDWAM ()
{
    memset(SDWAM, 0, sizeof(SDWAM));
    
    if (!sim_quiet) sim_printf("zero-initialized SDWAM\n");
    return SCPE_OK;
}

// Assumes unpaged DSBR

_sdw0 *fetchSDW(word15 segno)
{
    word36 SDWeven, SDWodd;
    
    core_read2(DSBR.ADDR + 2 * segno, &SDWeven, &SDWodd);
    
    // even word
    static _sdw0 _s;
    
    _sdw0 *SDW = &_s;   //calloc(1, sizeof(_sdw0));
    memset(SDW, 0, sizeof(_s));
    
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

char *strDSBR(void)
{
    static char buff[256];
    sprintf(buff, "DSBR: ADDR=%06o BND=%05o U=%o STACK=%04o", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK);
    return buff;
}

PRIVATE
void printDSBR()
{
    sim_printf("%s\n", strDSBR());
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
void printSDW0(_sdw0 *SDW)
{
    sim_printf("%s\n", strSDW0(SDW));
}

t_stat dpsCmd_DumpSegmentTable()
{
    sim_printf("*** Descriptor Segment Base Register (DSBR) ***\n");
    printDSBR();
    if (DSBR.U) {
        sim_printf("*** Descriptor Segment Table ***\n");
        for(word15 segno = 0; 2 * segno < 16 * (DSBR.BND + 1); segno += 1)
        {
            sim_printf("Seg %d - ", segno);
            _sdw0 *s = fetchSDW(segno);
            printSDW0(s);
            
            //free(s); no longer needed
        }
    } else {
        sim_printf("*** Descriptor Segment Table (Paged) ***\n");
        sim_printf("Descriptor segment pages\n");
        for(word15 segno = 0; 2 * segno < 16 * (DSBR.BND + 1); segno += 512)
        {
            word24 y1 = (2 * segno) % 1024;
            word24 x1 = (2 * segno - y1) / 1024;
            word36 PTWx1;
            core_read ((DSBR . ADDR + x1) & PAMASK, & PTWx1);

            struct _ptw0 PTW1;
            PTW1.ADDR = GETHI(PTWx1);
            PTW1.U = TSTBIT(PTWx1, 9);
            PTW1.M = TSTBIT(PTWx1, 6);
            PTW1.F = TSTBIT(PTWx1, 2);
            PTW1.FC = PTWx1 & 3;
           
            if (PTW1.F == 0)
                continue;
            sim_printf ("%06o  Addr %06o U %o M %o F %o FC %o\n", 
                        segno, PTW1.ADDR, PTW1.U, PTW1.M, PTW1.F, PTW1.FC);
            sim_printf ("    Target segment page table\n");
            for (word15 tspt = 0; tspt < 512; tspt ++)
            {
                word36 SDWeven, SDWodd;
                core_read2(((PTW1 . ADDR << 6) + tspt * 2) & PAMASK, & SDWeven, & SDWodd);
                struct _sdw0 SDW0;
                // even word
                SDW0.ADDR = (SDWeven >> 12) & PAMASK;
                SDW0.R1 = (SDWeven >> 9) & 7;
                SDW0.R2 = (SDWeven >> 6) & 7;
                SDW0.R3 = (SDWeven >> 3) & 7;
                SDW0.F = TSTBIT(SDWeven, 2);
                SDW0.FC = SDWeven & 3;

                // odd word
                SDW0.BOUND = (SDWodd >> 21) & 037777;
                SDW0.R = TSTBIT(SDWodd, 20);
                SDW0.E = TSTBIT(SDWodd, 19);
                SDW0.W = TSTBIT(SDWodd, 18);
                SDW0.P = TSTBIT(SDWodd, 17);
                SDW0.U = TSTBIT(SDWodd, 16);
                SDW0.G = TSTBIT(SDWodd, 15);
                SDW0.C = TSTBIT(SDWodd, 14);
                SDW0.EB = SDWodd & 037777;

                if (SDW0.F == 0)
                    continue;
                sim_printf ("    %06o Addr %06o %o,%o,%o F%o BOUND %06o %c%c%c%c%c\n",
                          tspt, SDW0.ADDR, SDW0.R1, SDW0.R2, SDW0.R3, SDW0.F, SDW0.BOUND, SDW0.R ? 'R' : '.', SDW0.E ? 'E' : '.', SDW0.W ? 'W' : '.', SDW0.P ? 'P' : '.', SDW0.U ? 'U' : '.');
                //for (word18 offset = 0; ((offset >> 4) & 037777) <= SDW0 . BOUND; offset += 1024)
                if (SDW0.U == 0)
                {
                    for (word18 offset = 0; offset < 16 * (SDW0.BOUND + 1); offset += 1024)
                    {
                        word24 y2 = offset % 1024;
                        word24 x2 = (offset - y2) / 1024;

                        // 10. Fetch the target segment PTW(x2) from SDW(segno).ADDR + x2.

                        word36 PTWx2;
                        core_read ((SDW0 . ADDR + x2) & PAMASK, & PTWx2);

                        struct _ptw0 PTW2;
                        PTW2.ADDR = GETHI(PTWx2);
                        PTW2.U = TSTBIT(PTWx2, 9);
                        PTW2.M = TSTBIT(PTWx2, 6);
                        PTW2.F = TSTBIT(PTWx2, 2);
                        PTW2.FC = PTWx2 & 3;

                         sim_printf ("        %06o  Addr %06o U %o M %o F %o FC %o\n", 
                                     offset, PTW2.ADDR, PTW2.U, PTW2.M, PTW2.F, PTW2.FC);

                      }
                  }
            }
        }
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

/*! Reset routine */
t_stat cpu_reset_mm (DEVICE *dptr)
{
    
#ifdef USE_IDLE
    sim_set_idle (cpu_unit, 512*1024, NULL, NULL);
#endif
    sim_debug (DBG_INFO, & cpu_dev, "CPU reset: Running\n");
    
    ic_history_init();
    
    memset(&events, 0, sizeof(events));
    memset(&cpu, 0, sizeof(cpu));
    memset(&cu, 0, sizeof(cu));
    memset(&PPR, 0, sizeof(PPR));
    cu.SD_ON = 1;
    cu.PT_ON = 1;
    cpu.ic_odd = 0;
    
    // TODO: reset *all* other structures to zero
    
    set_addr_mode(ABSOLUTE_mode);
    
    cpu.cycle = FETCH_cycle;

//#if FEAT_INSTR_STATS
    memset(&sys_stats, 0, sizeof(sys_stats));
//#endif
    
    return 0;
}

t_stat cpu_boot (int32 unit_num, DEVICE *dptr)
{
    // The boot button on the cpu is conneted to the boot button on the IOM
    // XXX is this true? Which IOM is it connected to?
    //return iom_boot (ASSUME0, & iom_dev);
    sim_printf ("Try 'BOOT IOMn'\n");
    return SCPE_ARG;
}

// Map memory to port
static int scpage_map [N_SCPAGES];

static void setup_scpage_map (void)
  {
    sim_debug (DBG_DEBUG, & cpu_dev, "setup_scpage_map: SCPAGE %d N_SCPAGES %d MAXMEMSIZE %d\n", SCPAGE, N_SCPAGES, MAXMEMSIZE);

    // Initalize to unmapped
    for (int pg = 0; pg < N_SCPAGES; pg ++)
      scpage_map [pg] = -1; 

    // For each port (which is connected to a SCU
    for (int port_num = 0; port_num < N_CPU_PORTS; port_num ++)
      {
        if (! switches . enable [port_num])
          continue;
        // Calculate the amount of memory in the SCU in words
        uint store_size = switches . store_size [port_num];
        uint sz = 1 << (store_size + 16);

        // Calculate the base address of the memor in wordsy
        uint assignment = switches . assignment [port_num];
        uint base = assignment * sz;

        // Now convert to SCPAGES
        sz = sz / SCPAGE;
        base = base / SCPAGE;

        sim_debug (DBG_DEBUG, & cpu_dev, "setup_scpage_map: port:%d ss:%u as:%u sz:%u ba:%u\n", port_num, store_size, assignment, sz, base);

	for (int pg = 0; pg < sz; pg ++)
          {
            int scpg = base + pg;
            if (scpg >= 0 && scpg < N_SCPAGES)
              scpage_map [scpg] = port_num;
          }
      }
    for (int pg = 0; pg < N_SCPAGES; pg ++)
      sim_debug (DBG_DEBUG, & cpu_dev, "setup_scpage_map: %d:%d\n", pg, scpage_map [pg]);
  }

int query_scpage_map (word24 addr)
  {
    uint scpg = addr / SCPAGE;
    if (scpg < N_SCPAGES)
      return scpage_map [scpg];
    return -1;
  }

t_stat cpu_reset (DEVICE *dptr)
{
    if (M)
        free(M);
    
    M = (word36 *) calloc (MEMSIZE, sizeof (word36));
    if (M == NULL)
        return SCPE_MEM;
    
    //memset (M, -1, MEMSIZE * sizeof (word36));

    // Fill DPS8 memory with zeros, plus a flag only visible to the emulator
    // marking the memory as uninitialized.

    for (int i = 0; i < MEMSIZE; i ++)
      M [i] = MEM_UNINITIALIZED;

    rIC = 0;
    rA = 0;
    rQ = 0;
    XECD = 0;
    
    PPR.IC = 0;
    PPR.PRR = 0;
    PPR.PSR = 0;
    PPR.P = 1;
    
    processorCycle = UNKNOWN_CYCLE;
    //processorAddressingMode = ABSOLUTE_MODE;
    set_addr_mode(ABSOLUTE_mode);
    
    sim_brk_types = sim_brk_dflt = SWMASK ('E');

    cpuCycles = 0;
    
    CMR.luf = 3;    // default of 16 mS
    
    // XXX free up previous deferred segments (if any)
    
    
    cpu_reset_mm(dptr);

    cpu_reset_array ();

    setup_scpage_map ();

    initializeTheMatrix();

    return SCPE_OK;
}

/*! Memory examine */
//  t_stat examine_routine (t_val *eval_array, t_addr addr, UNIT *uptr, int32 switches) – Copy 
//  sim_emax consecutive addresses for unit uptr, starting at addr, into eval_array. The switch 
//  variable has bit<n> set if the n’th letter was specified as a switch to the examine command. 
// Not true...

t_stat cpu_ex (t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
    if (addr>= MEMSIZE)
        return SCPE_NXM;
    if (vptr != NULL)
      {
        *vptr = M[addr] & DMASK;
      }
    return SCPE_OK;
}

/*! Memory deposit */
t_stat cpu_dep (t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
    if (addr >= MEMSIZE) return SCPE_NXM;
    M[addr] = val & DMASK;
    return SCPE_OK;
}


enum _processor_cycle_type processorCycle;                  ///< to keep tract of what type of cycle the processor is in
//enum _processor_addressing_mode processorAddressingMode;    ///< what addressing mode are we using
enum _processor_operating_mode processorOperatingMode;      ///< what operating mode

// h6180 stuff
/* [map] designates mapping into 36-bit word from DPS-8 proc manual */

/* GE-625/635 */

word36	rA;	/*!< accumulator */
word36	rQ;	/*!< quotient */
word8	rE;	/*!< exponent [map: rE, 28 0's] */

word18	rX[8];	/*!< index */



//word18	rBAR;	/*!< base address [map: BAR, 18 0's] */
/* format: 9b base, 9b bound */

int XECD; /*!< for out-of-line XEC,XED,faults, etc w/o rIC fetch */
word36 XECD1; /*!< XEC instr / XED instr#1 */
word36 XECD2; /*!< XED instr#2 */


//word18	rIC;	/*!< instruction counter */
// same as PPR.IC
 //word18	rIR;	/*!< indicator [15b] [map: 18 x's, rIR w/ 3 0's] */
//IR_t IR;        // Indicator register   (until I can map MM IR to my rIR)


word27	rTR;	/*!< timer [map: TR, 9 0's] */

//word18	ry;     /*!< address operand */
word24	rY;     /*!< address operand */
word8	rTAG;	/*!< instruction tag */

word8	tTB;	/*!< char size indicator (TB6=6-bit,TB9=9-bit) [3b] */
word8	tCF;	/*!< character position field [3b] */

/* GE-645 */


//word36	rDBR;	/*!< descriptor base */
//!<word27	rABR[8];/*!< address base */
//
///* H6180; L68; DPS-8M */
//
word8	rRALR;	/*!< ring alarm [3b] [map: 33 0's, RALR] */
//word36	rPRE[8];/*!< pointer, even word */
//word36	rPRO[8];/*!< pointer, odd word */
//word27	rAR[8];	/*!< address [24b] [map: ARn, 12 0's] */
//word36	rPPRE;	/*!< procedure pointer, even word */
//word36	rPPRO;	/*!< procedure pointer, odd word */
//word36	rTPRE;	/*!< temporary pointer, even word */
//word36	rTPRO;	/*!< temporary pointer, odd word */
//word36	rDSBRE;	/*!< descriptor segment base, even word */
//word36	rDSBRO;	/*!< descriptor segment base, odd word */
//word36	mSE[16];/*!< word-assoc-mem: seg descrip regs, even word */
//word36	mSO[16];/*!< word-assoc-mem: seg descrip regs, odd word */
//word36	mSP[16];/*!< word-assoc-mem: seg descrip ptrs */
//word36	mPR[16];/*!< word-assoc-mem: page tbl regs */
//word36	mPP[16];/*!< word-assoc-mem: page tbl ptrs */
//word36	rFRE;	/*!< fault, even word */
//word36	rFRO;	/*!< fault, odd word */
//word36	rMR;	/*!< mode */
//word36	rCMR;	/*!< cache mode */
//word36	hCE[16];/*!< history: control unit, even word */
//word36	hCO[16];/*!< history: control unit, odd word */
//word36	hOE[16];/*!< history: operations unit, even word */
//word36	hOO[16];/*!< history: operations unit, odd word */
//word36	hDE[16];/*!< history: decimal unit, even word */
//word36	hDO[16];/*!< history: decimal unit, odd word */
//word36	hAE[16];/*!< history: appending unit, even word */
//word36	hAO[16];/*!< history: appending unit, odd word */
//word36	rSW[5];	/*!< switches */

//word12 rFAULTBASE;  ///< fault base (12-bits of which the top-most 7-bits are used)

// end h6180 stuff

struct _tpr TPR;    ///< Temporary Pointer Register
struct _ppr PPR;    ///< Procedure Pointer Register
//struct _prn PR[8];  ///< Pointer Registers
//struct _arn AR[8];  ///< Address Registers
struct _par PAR[8]; ///< pointer/address resisters
struct _bar BAR;    ///< Base Address Register
struct _dsbr DSBR;  ///< Descriptor Segment Base Register


// XXX given this is not real hardware we can eventually remove the SDWAM -- I think. But for now just leave it in.
// For the DPS 8M processor, the SDW associative memory will hold the 64 MRU SDWs and have a 4-way set associative organization with LRU replacement.

struct _sdw  SDWAM[64], *SDW = &SDWAM[0];    ///< Segment Descriptor Word Associative Memory & working SDW
struct _sdw0 SDW0;  ///< a SDW not in SDWAM

struct _ptw PTWAM[64], *PTW = &PTWAM[0];    ///< PAGE TABLE WORD ASSOCIATIVE MEMORY and working PTW
struct _ptw0 PTW0;  ///< a PTW not in PTWAM (PTWx1)

_cache_mode_register CMR;
_mode_register MR;

/*
 * register stuff ...
 */
static const char *z1[] = {"0", "1"};
static BITFIELD dps8_IR_bits[] = {    
    BITNCF(3),
    BITFNAM(HEX,   1, z1),    /*!< base-16 exponent */ ///< 0000010
    BITFNAM(ABS,   1, z1),    /*!< absolute mode */ ///< 0000020
    BITFNAM(MIIF,  1, z1),	  /*!< mid-instruction interrupt fault */ ///< 0000040
    BITFNAM(TRUNC, 1, z1),    /*!< truncation */ ///< 0000100
    BITFNAM(NBAR,  1, z1),	  /*!< not BAR mode */ ///< 0000200
    BITFNAM(PMASK, 1, z1),	  /*!< parity mask */ ///< 0000400
    BITFNAM(PAR,   1, z1),    /*!< parity error */ ///< 0001000
    BITFNAM(TALLY, 1, z1),	  /*!< tally runout */ ///< 0002000
    BITFNAM(OMASK, 1, z1),    /*!< overflow mask */ ///< 0004000
    BITFNAM(EUFL,  1, z1),	  /*!< exponent underflow */ ///< 0010000
    BITFNAM(EOFL,  1, z1),	  /*!< exponent overflow */ ///< 0020000
    BITFNAM(OFLOW, 1, z1),	  /*!< overflow */ ///< 0040000
    BITFNAM(CARRY, 1, z1),	  /*!< carry */ ///< 0100000
    BITFNAM(NEG,   1, z1),    /*!< negative */ ///< 0200000
    BITFNAM(ZERO,  1, z1),	  /*!< zero */ ///< 0400000
    ENDBITS
};

static REG cpu_reg[] = {
    { ORDATA (IC, rIC, VASIZE) },
    //{ ORDATA (IR, rIR, 18) },
    { ORDATADF (IR, rIR, 18, "Indicator Register", dps8_IR_bits) },
    
    //    { FLDATA (Zero, rIR, F_V_A) },
    //    { FLDATA (Negative, rIR, F_V_B) },
    //    { FLDATA (Carry, rIR, F_V_C) },
    //    { FLDATA (Overflow, rIR, F_V_D) },
    //    { FLDATA (ExpOvr, rIR, F_V_E) },
    //    { FLDATA (ExpUdr, rIR, F_V_F) },
    //    { FLDATA (OvrMask, rIR, F_V_G) },
    //    { FLDATA (TallyRunOut, rIR, F_V_H) },
    //    { FLDATA (ParityErr, rIR, F_V_I) }, ///< Yeah, right!
    //    { FLDATA (ParityMask, rIR, F_V_J) },
    //    { FLDATA (NotBAR, rIR, F_V_K) },
    //    { FLDATA (Trunc, rIR, F_V_L) },
    //    { FLDATA (MidInsFlt, rIR, F_V_M) },
    //    { FLDATA (AbsMode, rIR, F_V_N) },
    //    { FLDATA (HexMode, rIR, F_V_O) },
    
    { ORDATA (A, rA, 36) },
    { ORDATA (Q, rQ, 36) },
    { ORDATA (E, rE, 8) },
    
    { ORDATA (X0, rX[0], 18) },
    { ORDATA (X1, rX[1], 18) },
    { ORDATA (X2, rX[2], 18) },
    { ORDATA (X3, rX[3], 18) },
    { ORDATA (X4, rX[4], 18) },
    { ORDATA (X5, rX[5], 18) },
    { ORDATA (X6, rX[6], 18) },
    { ORDATA (X7, rX[7], 18) },
    
    { ORDATA (PPR.IC,  PPR.IC,  18) },
    { ORDATA (PPR.PRR, PPR.PRR,  3) },
    { ORDATA (PPR.PSR, PPR.PSR, 15) },
    { ORDATA (PPR.P,   PPR.P,    1) },
    
    { ORDATA (DSBR.ADDR,  DSBR.ADDR,  24) },
    { ORDATA (DSBR.BND,   DSBR.BND,   14) },
    { ORDATA (DSBR.U,     DSBR.U,      1) },
    { ORDATA (DSBR.STACK, DSBR.STACK, 12) },
    
    { ORDATA (BAR.BASE,  BAR.BASE,  9) },
    { ORDATA (BAR.BOUND, BAR.BOUND, 9) },
    
    //{ ORDATA (FAULTBASE, rFAULTBASE, 12) }, ///< only top 7-msb are used
    
    { ORDATA (PR0.SNR, PR[0].SNR, 18) },
    { ORDATA (PR1.SNR, PR[1].SNR, 18) },
    { ORDATA (PR2.SNR, PR[2].SNR, 18) },
    { ORDATA (PR3.SNR, PR[3].SNR, 18) },
    { ORDATA (PR4.SNR, PR[4].SNR, 18) },
    { ORDATA (PR5.SNR, PR[5].SNR, 18) },
    { ORDATA (PR6.SNR, PR[6].SNR, 18) },
    { ORDATA (PR7.SNR, PR[7].SNR, 18) },
    
    { ORDATA (PR0.RNR, PR[0].RNR, 18) },
    { ORDATA (PR1.RNR, PR[1].RNR, 18) },
    { ORDATA (PR2.RNR, PR[2].RNR, 18) },
    { ORDATA (PR3.RNR, PR[3].RNR, 18) },
    { ORDATA (PR4.RNR, PR[4].RNR, 18) },
    { ORDATA (PR5.RNR, PR[5].RNR, 18) },
    { ORDATA (PR6.RNR, PR[6].RNR, 18) },
    { ORDATA (PR7.RNR, PR[7].RNR, 18) },
    
    //{ ORDATA (PR0.BITNO, PR[0].PBITNO, 18) },
    //{ ORDATA (PR1.BITNO, PR[1].PBITNO, 18) },
    //{ ORDATA (PR2.BITNO, PR[2].PBITNO, 18) },
    //{ ORDATA (PR3.BITNO, PR[3].PBITNO, 18) },
    //{ ORDATA (PR4.BITNO, PR[4].PBITNO, 18) },
    //{ ORDATA (PR5.BITNO, PR[5].PBITNO, 18) },
    //{ ORDATA (PR6.BITNO, PR[6].PBITNO, 18) },
    //{ ORDATA (PR7.BITNO, PR[7].PBITNO, 18) },
    
    //{ ORDATA (AR0.BITNO, PR[0].ABITNO, 18) },
    //{ ORDATA (AR1.BITNO, PR[1].ABITNO, 18) },
    //{ ORDATA (AR2.BITNO, PR[2].ABITNO, 18) },
    //{ ORDATA (AR3.BITNO, PR[3].ABITNO, 18) },
    //{ ORDATA (AR4.BITNO, PR[4].ABITNO, 18) },
    //{ ORDATA (AR5.BITNO, PR[5].ABITNO, 18) },
    //{ ORDATA (AR6.BITNO, PR[6].ABITNO, 18) },
    //{ ORDATA (AR7.BITNO, PR[7].ABITNO, 18) },
    
    { ORDATA (PR0.WORDNO, PR[0].WORDNO, 18) },
    { ORDATA (PR1.WORDNO, PR[1].WORDNO, 18) },
    { ORDATA (PR2.WORDNO, PR[2].WORDNO, 18) },
    { ORDATA (PR3.WORDNO, PR[3].WORDNO, 18) },
    { ORDATA (PR4.WORDNO, PR[4].WORDNO, 18) },
    { ORDATA (PR5.WORDNO, PR[5].WORDNO, 18) },
    { ORDATA (PR6.WORDNO, PR[6].WORDNO, 18) },
    { ORDATA (PR7.WORDNO, PR[7].WORDNO, 18) },
    
    //{ ORDATA (PR0.CHAR, PR[0].CHAR, 18) },
    //{ ORDATA (PR1.CHAR, PR[1].CHAR, 18) },
    //{ ORDATA (PR2.CHAR, PR[2].CHAR, 18) },
    //{ ORDATA (PR3.CHAR, PR[3].CHAR, 18) },
    //{ ORDATA (PR4.CHAR, PR[4].CHAR, 18) },
    //{ ORDATA (PR5.CHAR, PR[5].CHAR, 18) },
    //{ ORDATA (PR6.CHAR, PR[6].CHAR, 18) },
    //{ ORDATA (PR7.CHAR, PR[7].CHAR, 18) },
    
    /*
     { ORDATA (EBR, ebr, EBR_N_EBR) },
     { FLDATA (PGON, ebr, EBR_V_PGON) },
     { FLDATA (T20P, ebr, EBR_V_T20P) },
     { ORDATA (UBR, ubr, 36) },
     { GRDATA (CURAC, ubr, 8, 3, UBR_V_CURAC), REG_RO },
     { GRDATA (PRVAC, ubr, 8, 3, UBR_V_PRVAC) },
     { ORDATA (SPT, spt, 36) },
     { ORDATA (CST, cst, 36) },
     { ORDATA (PUR, pur, 36) },
     { ORDATA (CSTM, cstm, 36) },
     { ORDATA (HSB, hsb, 36) },
     { ORDATA (DBR1, dbr1, PASIZE) },
     { ORDATA (DBR2, dbr2, PASIZE) },
     { ORDATA (DBR3, dbr3, PASIZE) },
     { ORDATA (DBR4, dbr4, PASIZE) },
     { ORDATA (PCST, pcst, 36) },
     { ORDATA (PIENB, pi_enb, 7) },
     { FLDATA (PION, pi_on, 0) },
     { ORDATA (PIACT, pi_act, 7) },
     { ORDATA (PIPRQ, pi_prq, 7) },
     { ORDATA (PIIOQ, pi_ioq, 7), REG_RO },
     { ORDATA (PIAPR, pi_apr, 7), REG_RO },
     { ORDATA (APRENB, apr_enb, 8) },
     { ORDATA (APRFLG, apr_flg, 8) },
     { ORDATA (APRLVL, apr_lvl, 3) },
     { ORDATA (RLOG, rlog, 10) },
     { FLDATA (F1PR, its_1pr, 0) },
     { BRDATA (PCQ, pcq, 8, VASIZE, PCQ_SIZE), REG_RO+REG_CIRC },
     { ORDATA (PCQP, pcq_p, 6), REG_HRO },
     { DRDATA (INDMAX, ind_max, 8), PV_LEFT + REG_NZ },
     { DRDATA (XCTMAX, xct_max, 8), PV_LEFT + REG_NZ },
     { ORDATA (WRU, sim_int_char, 8) },
     { FLDATA (STOP_ILL, stop_op0, 0) },
     { BRDATA (REG, acs, 8, 36, AC_NUM * AC_NBLK) },
     */
    
    { NULL }
};

/*
 * simh interface
 */

REG *sim_PC = &cpu_reg[0];

/*! CPU device descriptor */
DEVICE cpu_dev = {
    "CPU",          /*!< name */
    cpu_unit,       /*!< units */
    cpu_reg,        /*!< registers */
    cpu_mod,        /*!< modifiers */
    N_CPU_UNITS,    /*!< #units */
    8,              /*!< address radix */
    PASIZE,         /*!< address width */
    1,              /*!< addr increment */
    8,              /*!< data radix */
    36,             /*!< data width */
    &cpu_ex,        /*!< examine routine */
    &cpu_dep,       /*!< deposit routine */
    &cpu_reset,     /*!< reset routine */
    &cpu_boot,           /*!< boot routine */
    NULL,           /*!< attach routine */
    NULL,           /*!< detach routine */
    NULL,           /*!< context */
    DEV_DEBUG,      /*!< device flags */
    0,              /*!< debug control flags */
    cpu_dt,         /*!< debug flag names */
    NULL,           /*!< memory size change */
    NULL            /*!< logical name */
};

//word36 IWB;         ///< instruction working buffer
//opCode *iwb = NULL; ///< opCode *
//int32  opcode;      ///< opcode
//bool   opcodeX;     ///< opcode extension
//word18 address;     ///< bits 0-17 of instruction XXX replace with rY
//bool   a;           ///< bin-29 - indirect addressing mode?
//bool   i;           ///< interrupt inhinit bit.
//word6  tag;         ///< instruction tag XXX replace with rTAG

//word18 stiTally;    ///< for sti instruction


static DCDstruct *newDCDstruct(void);

static t_stat reason;

jmp_buf jmpMain;        ///< This is where we should return to from a fault or interrupt (if necessary)

static DCDstruct _currentInstruction;
DCDstruct *currentInstruction  = &_currentInstruction;;

static EISstruct E;
//EISstruct *e = &E;

events_t events;
switches_t switches;
// the following two should probably be combined
cpu_state_t cpu;
ctl_unit_data_t cu;


// This is an out-of-band flag for the APU. User commands to
// display or modify memory can invoke much the APU. Howeveer, we don't
// want interactive attempts to access non-existant memory locations
// to register a fault.
flag_t fault_gen_no_fault;

int stop_reason; // sim_instr return value for JMP_STOP

/*
 *  cancel_run()
 *
 *  Cancel_run can be called by any portion of the code to let
 *  sim_instr() know that it should stop looping and drop back
 *  to the SIMH command prompt.
 */

static int cancel;

void cancel_run(t_stat reason)
{
    // Maybe we should generate an OOB fault?
    
    (void) sim_cancel_step();
    if (cancel == 0 || reason < cancel)
        cancel = reason;
    //sim_debug (DBG_DEBUG, & cpu_dev, "CU: Cancel requested: %d\n", reason);
}

static uint get_highest_intr (void)
  {
// XXX In theory there needs to be interlocks on this?
    for (int int_num = 32 - 1; int_num >= 0; int_num --) // XXX Magic number
      if (events . interrupts [int_num])
        {
          events . interrupts [int_num] = 0;

          int cnt = 0;
          for (int i = 0; i < 32; i ++) // XXX Magic number
            if (events . interrupts [i])
              cnt ++;
          events . int_pending = !!cnt;

          for (int i = 0; i < 7; i ++) // XXX Magic number
            if (events . fault [i])
              cnt ++;
          events . any = !! cnt;

          return int_num * 2;
        }
    return 1;
  }

bool sample_interrupts (void)
  {
    return events . int_pending;
  }

t_stat doIEFPLoop();

// This is part of the simh interface
t_stat sim_instr (void)
{

#ifdef USE_IDLE
    sim_rtcn_init (0, 0);
#endif

    // Heh. This needs to be static; longjmp resets the value to NULL
    static DCDstruct *ci = NULL;
    
    /* Main instruction fetch/decode loop */
    adrTrace  = (cpu_dev.dctrl & DBG_ADDRMOD  ) && sim_deb; // perform address mod tracing
    apndTrace = (cpu_dev.dctrl & DBG_APPENDING) && sim_deb; // perform APU tracing
    
    cpu . interrupt_flag = false;

    //currentInstruction = &_currentInstruction;
    currentInstruction->e = &E;
    
    int val = setjmp(jmpMain);    // here's our main fault/interrupt return. Back to executing instructions....
    switch (val)
    {
        case 0:
            reason = 0;
            //cpuCycles = 0;
            break;
        case JMP_NEXT:
            goto jmpNext;
        case JMP_RETRY:
            goto jmpRetry;
        case JMP_TRA:
            goto jmpTra;
//        case JMP_INTR:
//            goto jmpIntr;
        case JMP_STOP:
            return stop_reason;

    }
    
    
//    if (val)
//    {
//        // if we're here, we're returning from a fault etc and we want to retry an instruction
//        goto retry;
//    } else {
//        reason = 0;
//        cpuCycles = 0;  // XXX This probably needs to be moved so our fault handler won't reset cpuCycles back to 0
//    }
    
    do {
jmpRetry:;
        /* loop until halted */
        if (sim_interval <= 0) {                                /* check clock queue */
            if ((reason = sim_process_event ()))
                break;
        }
        
        sim_interval --;
        //if (sim_brk_summ && sim_brk_test (rIC, SWMASK ('E'))) {    /* breakpoint? */
        // sim_brk_test expects a 32 bit address; PPR.IC into the low 18, and
        // PPR.PSR into the high 12
        if (sim_brk_summ && sim_brk_test ((rIC & 0777777) | ((((t_addr) PPR.PSR) & 037777) << 18), SWMASK ('E'))) {    /* breakpoint? */
            reason = STOP_BKPT;                        /* stop simulation */
            break;
        }

        // stop after a given number ot cpuCycles ...
        if (sim_brk_summ && sim_brk_test (cpuCycles, SWMASK ('C'))) {    /* breakpoint? */
            reason = STOP_BKPT;                        /* stop simulation */
            break;
        }

        //reason = doIEFPLoop();
#if 1
        if (cpu . cycle == DIS_cycle)
          {
            cpu . interrupt_flag = sample_interrupts ();
            if (cpu . interrupt_flag)
              goto dis_entry;
            continue;
          }
#endif
        // do group 7 fault processing
        if (G7Pending && (rIR & 1) == 0)    // only process g7 fauts if available and on even instruction boundary
            doG7Faults();
            
        //
        // fetch instruction
        //processorCycle = SEQUENTIAL_INSTRUCTION_FETCH;
        processorCycle = INSTRUCTION_FETCH;
        

        ci = fetchInstruction(rIC, currentInstruction);    // fetch instruction into current instruction struct
        
        if (currentInstruction -> IWB == 0777777777777U &&
           switches . halt_on_unimp)
          return STOP_UNIMP;

// XXX The conditions are more rigorous: see AL39, pg 327
        if (rIC % 2 == 0 && // Even address
            ci -> i == 0) // Not inhibited
          cpu . interrupt_flag = sample_interrupts ();
        else
          cpu . interrupt_flag = false;

        // XXX: what if sim stops during XEC/XED? if user wants to re-step
        // instruc, is this logic OK?
    
//        if(XECD == 1) {
//          ci->IWB = XECD1;
//        } else if(XECD == 2) {
//          ci->IWB = XECD2;
//        }
//
        
        t_stat ret = executeInstruction(ci);

        if (! ret)
         {
           if (cpu . interrupt_flag)
              {
                sim_debug (DBG_INTR, & cpu_dev, "Handling interrupt_flag\n");
                // We should do this later, but doing it now allows us to
                // avoid clean up for no interrupt pending.

                uint intr_pair_addr;
dis_entry:;
jmpIntr:;
                intr_pair_addr = get_highest_intr ();
                if (intr_pair_addr != 1) // no interrupts 
                  {
                    sim_debug (DBG_INTR, & cpu_dev, "intr_pair_addr %u\n", intr_pair_addr);

		    // In the INTERRUPT CYCLE, the processor safe-stores the
		    // Control Unit Data (see Section 3) into program-invisible
		    // holding registers in preparation for a Store Control
		    // Unit (scu) instruction, enters temporary absolute mode,
		    // and forces the current ring of execution C(PPR.PRR) to
		    // 0. It then issues an XEC system controller command to
		    // the system controller on the highest priority port for
		    // which there is a bit set in the interrupt present
		    // register.  

                    cu_safe_store ();

                    // addr_modes_t am = get_addr_mode();  // save address mode

		    // Temporary absolute mode
		    set_addr_mode (TEMPORARY_ABSOLUTE_mode);

		    // Set to ring 0
		    PPR . PRR = 0;

                    // get intr_pair_addr

                    // get interrupt pair
                    word36 faultPair[2];
                    core_read2(intr_pair_addr, faultPair, faultPair+1);

                    // Don't! T4D says the IC remains pointing at the faulting
                    // instructiion
                    //PPR.IC = intr_pair_addr;

                    cpu . interrupt_flag = false;

                    t_stat xrv = doXED(faultPair);

                    sim_debug (DBG_MSG, & cpu_dev, "leaving DIS_cycle\n");
                    sim_printf ("leaving DIS_cycle\n");
                    cpu . cycle = FETCH_cycle;

                    if (xrv == CONT_TRA)
                    {
                        set_addr_mode(ABSOLUTE_mode);
                        sim_debug (DBG_INTR, & cpu_dev, "Interrupt pair transfers\n");
                        longjmp(jmpMain, JMP_TRA);      // execute transfer instruction
                    }
    
                    // XXX more better to do the safe_restore, and get the saved mode from the restored data; but remember that the SECRET_TEMPORARY has to be cleared

                    clear_TEMPORARY_ABSOLUTE_mode ();
                    // set_addr_mode(am);      // If no transfer of control takes place, the processor returns to the mode in effect at the time of the fault and resumes normal sequential execution with the instruction following the faulting instruction (C(PPR.IC) + 1).

                    cu_safe_restore ();

                    if (xrv == CONT_INTR)
                    {
                        longjmp(jmpMain, JMP_INTR);    
                    }

                    if (0 && xrv)                         // TODO: need to put test in to retry instruction (i.e. when executing restartable MW EIS?)
                        longjmp(jmpMain, JMP_RETRY);    // retry instruction
                    longjmp(jmpMain, JMP_RETRY);     // execute next instruction

                    ret = xrv;
                } // int_pair != 1
            } // interrupt_flag
             
        } // if (!ret)

        if (ret)
        {
            if (ret > 0)
            {
                reason = ret;
                break;
            } else {
                switch (ret)
                {
                    case CONT_TRA:
jmpTra:                 continue;   // don't bump rIC, instruction already did it
                    case CONT_FAULT:
                    {
                        // XXX Instruction faulted.
                    }
                    break;
                }
            }
        }
        
#if 0
        // XXX Remove this when we actually can wait for an interrupt
        if (ci->opcode == 0616) { // DIS
            reason = STOP_DIS;
            break;
        }
#endif

jmpNext:;
        // doesn't seem to work as advertized
        if (sim_poll_kbd())
            reason = STOP_BKPT;
       
        // XXX: what if sim stops during XEC/XED? if user wants to re-step
        // instruc, is this logic OK?
        if(XECD == 1) {
          XECD = 2;
        } else if(XECD == 2) {
          XECD = 0;
        } else if (cpu . cycle != DIS_cycle) // XXX maybe cycle == FETCH_cycle
          rIC += 1;
        
        // is this a multiword EIS?
        // XXX: no multiword EIS for XEC/XED/fault, right?? -MCW
        if (ci->info->ndes > 0)
          rIC += ci->info->ndes;
        
    } while (reason == 0);
    
    sim_printf("\r\ncpuCycles = %lld\n", cpuCycles);
    
    return reason;
}


#if 0
static void setDegenerate()
{
    sim_debug (DBG_DEBUG, & cpu_dev, "setDegenerate\n");
    //TPR.TRR = 0;
    //TPR.TSR = 0;
    //TPR.TBR = 0;
    
    //PPR.PRR = 0;
    //PPR.PSR = 0;
    
    //PPR.P = 1;
}
#endif

#if 0
static uint32 bkpt_type[4] = { SWMASK ('E') , SWMASK ('N'), SWMASK ('R'), SWMASK ('W') };
#endif

#define DOITSITP(indword, Tag) ((_TM(Tag) == TM_IR || _TM(Tag) == TM_RI) && (ISITP(indword) || ISITS(indword)))
//bool DOITSITP(indword, Tag)
//{
//    return ((GET_TM(Tag) == TM_IR || GET_TM(Tag) == TM_RI) && (ISITP(indword) || ISITS(indword)));
//}

//PRIVATE
//t_stat doAbsoluteRead(DCDstruct *i, word24 addr, word36 *dat, MemoryAccessType accessType, int32 Tag)
//{
//    sim_debug(DBG_TRACE, &cpu_dev, "doAbsoluteRead(Entry): accessType=%d IWB=%012llo A=%d\n", accessType, i->IWB, GET_A(i->IWB));
//    
//    rY = addr;
//    TPR.CA = addr;  //XXX for APU
//    
//    switch (accessType)
//    {
//        case InstructionFetch:
//            core_read(addr, dat);
//            break;
//        default:
//            //if (i->a)
//                doAppendCycle(i, accessType, Tag, -1, dat);
//            //else
//            //    core_read(addr, dat);
//            break;
//    }
//    return SCPE_OK;
//}

/*!
 cd@libertyhaven.com - sez ....
 If the instruction addresses a block of four words, the target of the instruction is supposed to be an address that is aligned on a four-word boundary (0 mod 4). If not, the processor will grab the four-word block containing that address that begins on a four-word boundary, even if it has to go back 1 to 3 words. Analogous explanation for 8, 16, and 32 cases.
 
 olin@olinsibert.com - sez ...
 It means that the appropriate low bits of the address are forced to zero. So it's the previous words, not the succeeding words, that are used to satisfy the request.
 
 -- Olin

 */

//
// read N words in a non-aligned fashion for EIS
//
// XXX here is where we probably need to to the prepage thang...
t_stat ReadNnoalign (DCDstruct *i, int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag)
{
#if 0
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
#endif
        for (int j = 0 ; j < n ; j ++)
            Read(i, addr + j, Yblock + j, OPERAND_READ, Tag);
    
    return SCPE_OK;
}

int OPSIZE(DCDstruct *i)
{
    if (i->info->flags & (READ_OPERAND | STORE_OPERAND))
        return 1;
    else if (i->info->flags & (READ_YPAIR | STORE_YPAIR))
        return 2;
    else if (i->info->flags & (READ_YBLOCK8 | STORE_YBLOCK8))
        return 8;
    else if (i->info->flags & (READ_YBLOCK16 | STORE_YBLOCK16))
        return 16;
    return 0;
}

// read instruction operands
t_stat ReadOP(DCDstruct *i, word18 addr, _processor_cycle_type cyctyp, bool b29)
{
#if 0
        if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
            return STOP_BKPT;
        else
#endif
        
    switch (OPSIZE(i))
    {
        case 1:
            Read(i, addr, &CY, cyctyp, b29);
            return SCPE_OK;
        case 2:
            addr &= 0777776;   // make even
            Read(i, addr + 0, Ypair + 0, cyctyp, b29);
            Read(i, addr + 1, Ypair + 1, cyctyp, b29);
            break;
        case 8:
            addr &= 0777770;   // make on 8-word boundary
            for (int j = 0 ; j < 8 ; j += 1)
                Read(i, addr + j, Yblock8 + j, cyctyp, b29);
            break;
        case 16:
            addr &= 0777760;   // make on 16-word boundary
            for (int j = 0 ; j < 16 ; j += 1)
                Read(i, addr + j, Yblock16 + j, cyctyp, b29);
            
            break;
    }
    //TPR.CA = addr;  // restore address
    
    return SCPE_OK;

}

// write instruction operands
t_stat WriteOP(DCDstruct *i, word18 addr, _processor_cycle_type cyctyp, bool b29)
{
#if 0
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
#endif
        
    switch (OPSIZE(i))
    {
        case 1:
            Write(i, addr, CY, OPERAND_STORE, b29);
            return SCPE_OK;
        case 2:
            addr &= 0777776;   // make even
            Write(i, addr + 0, Ypair[0], OPERAND_STORE, b29);
            Write(i, addr + 1, Ypair[1], OPERAND_STORE, b29);
            break;
        case 8:
            addr &= 0777770;   // make on 8-word boundary
            for (int j = 0 ; j < 8 ; j += 1)
                Write(i, addr + j, Yblock8[j], OPERAND_STORE, b29);
            break;
        case 16:
            addr &= 0777760;   // make on 16-word boundary
            for (int j = 0 ; j < 16 ; j += 1)
                Write(i, addr + j, Yblock16[j], OPERAND_STORE, b29);
            break;
    }
    //TPR.CA = addr;  // restore address
    
    return SCPE_OK;
    
}

/*!
 * "Raw" core interface ....
 */
int32 core_read(word24 addr, word36 *data)
{
    if(addr >= MEMSIZE) {
        *data = 0;
        return -1;
    } else {
        if (M[addr] & MEM_UNINITIALIZED)
        {
            sim_debug (DBG_WARN, & cpu_dev, "Unitialized memory accessed at address %08o; IC is 0%06o:0%06o\n", addr, PPR.PSR, PPR.IC);
        }
        *data = M[addr] & DMASK;
    }
    return 0;
}

int core_write(word24 addr, word36 data) {
    if(addr >= MEMSIZE) {
        return -1;
    } else {
        M[addr] = data & DMASK;
    }
//printf ("cac %06o:%06o store %012llo @ %08o\r\n", PPR.PSR, PPR.IC, data, addr);
    return 0;
}

int core_read2(word24 addr, word36 *even, word36 *odd) {
    if(addr >= MEMSIZE) {
        *even = 0;
        *odd = 0;
        return -1;
    } else {
        if(addr & 1) {
            sim_debug(DBG_MSG, &cpu_dev,"warning: subtracting 1 from pair at %o in core_read2\n", addr);
            addr &= ~1; /* make it an even address */
        }
        if (M[addr] & MEM_UNINITIALIZED)
        {
            sim_debug (DBG_WARN, & cpu_dev, "Unitialized memory accessed at address %08o; IC is 0%06o:0%06o\n", addr, PPR.PSR, PPR.IC);
        }
        *even = M[addr++] & DMASK;
        if (M[addr] & MEM_UNINITIALIZED)
        {
            sim_debug (DBG_WARN, & cpu_dev, "Unitialized memory accessed at address %08o; IC is 0%06o:0%06o\n", addr, PPR.PSR, PPR.IC);
        }
        *odd = M[addr] & DMASK;
        return 0;
    }
}
//
////! for working with CY-pairs
//int core_read72(word24 addr, word72 *dst) // needs testing
//{
//    word36 even, odd;
//    if (core_read2(addr, &even, &odd) == -1)
//        return -1;
//    *dst = ((word72)even << 36) | (word72)odd;
//    return 0;
//}
//
int core_write2(word24 addr, word36 even, word36 odd) {
    if(addr >= MEMSIZE) {
        return -1;
    } else {
        if(addr & 1) {
            sim_debug(DBG_MSG, &cpu_dev, "warning: subtracting 1 from pair at %o in core_write2\n", addr);
            addr &= ~1; /* make it even a dress, or iron a skirt ;) */
        }
        M[addr++] = even;
        M[addr] = odd;
    }
    return 0;
}
////! for working with CY-pairs
//int core_write72(word24 addr, word72 src) // needs testing
//{
//    word36 even = (word36)(src >> 36) & DMASK;
//    word36 odd = ((word36)src) & DMASK;
//    
//    return core_write2(addr, even, odd);
//}
//
//int core_readN(word24 addr, word36 *data, int n)
//{
//    addr %= n;  // better be an even power of 2, 4, 8, 16, 32, 64, ....
//    for(int i = 0 ; i < n ; i++)
//        if(addr >= MEMSIZE) {
//            *data = 0;
//            return -1;
//        } else {
//            *data++ = M[addr++];
//        }
//    return 0;
//}
//
//int core_writeN(a8 addr, d8 *data, int n)
//{
//    addr %= n;  // better be an even power of 2, 4, 8, 16, 32, 64, ....
//    for(int i = 0 ; i < n ; i++)
//        if(addr >= MEMSIZE) {
//            return -1;
//        } else {
//            M[addr++] = *data++;
//        }
//    return 0;
//}

//#define MM
#if 1   //def MM


//=============================================================================

/*
 * encode_instr()
 *
 * Convert an instr_t struct into a  36-bit word.
 *
 */

void encode_instr(const instr_t *ip, t_uint64 *wordp)
{
    *wordp = setbits36(0, 0, 18, ip->addr);
#if 1
    *wordp = setbits36(*wordp, 18, 10, ip->opcode);
#else
    *wordp = setbits36(*wordp, 18, 9, ip->opcode & 0777);
    *wordp = setbits36(*wordp, 27, 1, ip->opcode >> 9);
#endif
    *wordp = setbits36(*wordp, 28, 1, ip->inhibit);
    if (! is_eis[ip->opcode&MASKBITS(10)]) {
        *wordp = setbits36(*wordp, 29, 1, ip->mods.single.pr_bit);
        *wordp = setbits36(*wordp, 30, 6, ip->mods.single.tag);
    } else {
        *wordp = setbits36(*wordp, 29, 1, ip->mods.mf1.ar);
        *wordp = setbits36(*wordp, 30, 1, ip->mods.mf1.rl);
        *wordp = setbits36(*wordp, 31, 1, ip->mods.mf1.id);
        *wordp = setbits36(*wordp, 32, 4, ip->mods.mf1.reg);
    }
}



#endif // MM
    
static DCDstruct *newDCDstruct(void)
{
    DCDstruct *p = malloc(sizeof(DCDstruct));
    p->e = malloc(sizeof(EISstruct));
    
    return p;
}

void freeDCDstruct(DCDstruct *p)
{
    if (!p)
        return; // Uh-Uh...
    
    if (p->e)
        free(p->e);
    free(p);
}

/*
 * instruction fetcher ...
 * fetch + decode instruction at 18-bit address 'addr'
 */
#ifdef OLD_WAY
DCDstruct *fetchInstructionOLD(word18 addr, DCDstruct *i)  // fetch instrcution at address
{
    DCDstruct *p = (i == NULL) ? newDCDstruct() : i;

// XXX experimental code; there may be a better way to do this, especially
// if a pointer to a malloc is getting zapped
// Yep, I was right
// HWR doesn't make sense. DCDstruct * is not really malloc()'d .. it's a global that needs to be cleared before each use. Why does the memset break gcc code?
    
    //memset (p, 0, sizeof (struct DCDstruct));
// Try the obivous ones
    p->opcode  = 0;
    p->opcodeX = 0;
    p->address = 0;
    p->a       = 0;
    p->i       = 0;
    p->tag     = 0;
    
    p->iwb = 0;
    p->IWB = 0;
    
    Read(p, addr, &p->IWB, InstructionFetch, 0);
    
    cpu.read_addr = addr;
    
    return decodeInstruction(p->IWB, p);
}
#endif

/*
 * instruction decoder .....
 *
 * if dst is not NULL place results into dst, if dst is NULL plae results into global currentInstruction
 */
DCDstruct *decodeInstruction(word36 inst, DCDstruct *dst)     // decode instruction into structure
{
    DCDstruct *p = dst == NULL ? newDCDstruct() : dst;
    
    p->opcode  = GET_OP(inst);  // get opcode
    p->opcodeX = GET_OPX(inst); // opcode extension
    p->address = GET_ADDR(inst);// address field from instruction
    p->a       = GET_A(inst);   // "A" - the indirect via pointer register flag
    p->i       = GET_I(inst);   // "I" - inhibit interrupt flag
    p->tag     = GET_TAG(inst); // instruction tag
    
    p->info = getIWBInfo(p);     // get info for IWB instruction
    
    // HWR 18 June 2013 
    p->info->opcode = p->opcode;
    p->IWB = inst;
    
    // HWR 21 Dec 2013
    if (p->info->flags & IGN_B29)
        p->a = 0;   // make certain 'a' bit is valid always

    if (p->info->ndes > 0)
    {
        p->a = 0;
        p->tag = 0;
        if (p->info->ndes > 1)
        {
            memset(p->e, 0, sizeof(EISstruct)); // clear out e
            p->e->op0 = p->IWB;
        }
    }
    return p;
}

// MM stuff ...

    /*
     * is_priv_mode()
     *
     * Report whether or or not the CPU is in privileged mode.
     * True if in absolute mode or if priv bit is on in segment TPR.TSR
     * The processor executes instructions in privileged mode when forming addresses in absolute mode
     * or when forming addresses in append mode and the segment descriptor word (SDW) for the segment in execution specifies a privileged procedure
     * and the execution ring is equal to zero.
     *
     * PPR.P A flag controlling execution of privileged instructions.
     *
     * Its value is 1 (permitting execution of privileged instructions) if PPR.PRR is 0 and the privileged bit in the segment descriptor word (SDW.P) for the procedure is 1; otherwise, its value is 0.
     */
    
    int is_priv_mode(void)
    {
        // TODO: fix this when time permits
        
        // something has already set .P
        if (PPR.P)
            return 1;
        
        switch (get_addr_mode())
        {
            case ABSOLUTE_mode:
                PPR.P = 1;
                return 1;
            
            case APPEND_mode:
                // XXX This is probably too simplistic, but it's a start
                
                if (switches . super_user)
                    return 1;

                if (SDW->P && PPR.PRR == 0)
                {
                    PPR.P = 1;
                    return 1;
                }
                sim_debug (DBG_FAULT, & cpu_dev, "is_priv_mode: not privledged; SDW->P: %d; PPR.PRR: %d\n", SDW->P, PPR.PRR);
                break;
            default:
                break;
        }
        
        //if (!TSTF(rIR, I_ABS))       //IR.abs_mode)
        //    return 1;
        
//        SDW_t *SDWp = get_sdw();    // Get SDW for segment TPR.TSR
//        if (SDWp == NULL) {
//            if (cpu.apu_state.fhld) {
//                // TODO: Do we need to check cu.word1flags.oosb and other flags to
//                // know what kind of fault to gen?
//                fault_gen(acc_viol_fault);
//                cpu.apu_state.fhld = 0;
//            }
//            sim_debug (DGB_WARN, & cpu_dev, "APU is-priv-mode: Segment does not exist?!?\n");
//            cancel_run(STOP_BUG);
//            return 0;   // arbitrary
//        }
//        if (SDWp->priv)
//            return 1;
//        if(opt_debug>0)
//            sim_debug (DBG_DEBUG, & cpu_dev, "APU: Priv check fails for segment %#o.\n", TPR.TSR);
//        return 0;
        
        return 0;
    }


static bool secret_addressing_mode;
/*
 * addr_modes_t get_addr_mode()
 *
 * Report what mode the CPU is in.
 * This is determined by examining a couple of IR flags.
 *
 * TODO: get_addr_mode() probably belongs in the CPU source file.
 *
 */

void clear_TEMPORARY_ABSOLUTE_mode (void)
{
    secret_addressing_mode = false;
}

addr_modes_t get_addr_mode(void)
{
    if (secret_addressing_mode)
        return ABSOLUTE_mode; // This is not the mode you are looking for

    //if (IR.abs_mode)
    if (TSTF(rIR, I_ABS))
        return ABSOLUTE_mode;
    
    //if (IR.not_bar_mode == 0)
    if (! TSTF(rIR, I_NBAR))
        return BAR_mode;
    
    return APPEND_mode;
}


/*
 * set_addr_mode()
 *
 * Put the CPU into the specified addressing mode.   This involves
 * setting a couple of IR flags and the PPR priv flag.
 *
 */

void set_addr_mode(addr_modes_t mode)
{
    secret_addressing_mode = false;
    if (mode == ABSOLUTE_mode) {
        SETF(rIR, I_ABS);
        SETF(rIR, I_NBAR);
        
        PPR.P = 1;
        
#if 0
        if (switches . degenerate_mode)
          setDegenerate();
#endif 
        sim_debug (DBG_DEBUG, & cpu_dev, "APU: Setting absolute mode.\n");
    } else if (mode == APPEND_mode) {
        if (! TSTF (rIR, I_ABS) && TSTF (rIR, I_NBAR))
          sim_debug (DBG_DEBUG, & cpu_dev, "APU: Keeping append mode.\n");
        else
           sim_debug (DBG_DEBUG, & cpu_dev, "APU: Setting append mode.\n");
        CLRF(rIR, I_ABS);
        
        SETF(rIR, I_NBAR);
        
    } else if (mode == BAR_mode) {
        CLRF(rIR, I_ABS);
        CLRF(rIR, I_NBAR);
        
        sim_debug (DBG_WARN, & cpu_dev, "APU: Setting bar mode.\n");
    } else if (mode == TEMPORARY_ABSOLUTE_mode) {
        PPR.P = 1;
        secret_addressing_mode = true;
        
#if 0
        if (switches . degenerate_mode)
          setDegenerate();
#endif
        
        sim_debug (DBG_DEBUG, & cpu_dev, "APU: Setting temporary absolute mode.\n");

    } else {
        sim_debug (DBG_ERR, & cpu_dev, "APU: Unable to determine address mode.\n");
        cancel_run(STOP_BUG);
    }
    //processorAddressingMode = mode;
}


//=============================================================================

/*
 ic_hist - Circular queue of instruction history
 Used for display via cpu_show_history()
 */

static int ic_hist_max = 0;
static int ic_hist_ptr;
static int ic_hist_wrapped;
enum hist_enum { h_instruction, h_fault, h_intr } htype;
struct ic_hist_t {
    addr_modes_t addr_mode;
    uint seg;
    uint ic;
    //enum hist_enum { instruction, fault, intr } htype;
    enum hist_enum htype;
    union {
        int intr;
        int fault;
        instr_t instr;
    } detail;
};

typedef struct ic_hist_t ic_hist_t;

static ic_hist_t *ic_hist;

void ic_history_init(void)
{
    ic_hist_wrapped = 0;
    ic_hist_ptr = 0;
    if (ic_hist != NULL)
        free(ic_hist);
    if (ic_hist_max < 60)
        ic_hist_max = 60;
    ic_hist = (ic_hist_t*) malloc(sizeof(*ic_hist) * ic_hist_max);
}

// XXX when multiple cpus are supported, make the cpu  data structure
// an array and merge the unit state info into here; coding convention
// is the name should be 'cpu' (as is 'iom' and 'scu'); but that name
// is taken. It should probably be merged into here, and then this
// should then be renamed.

#define N_CPU_UNITS_MAX 1

static struct
  {
    struct
      {
        flag_t inuse;
        int scu_unit_num; // 
        DEVICE * devp;
        UNIT * unitp;
      } ports [N_CPU_PORTS];

  } cpu_array [N_CPU_UNITS_MAX];

// XXX when multiple cpus are supported, merge this into cpu_reset

int query_scu_unit_num (int cpu_unit_num, int cpu_port_num)
  {
    return cpu_array [cpu_unit_num] . ports [cpu_port_num] . scu_unit_num;
  }

void cpu_reset_array (void)
  {
    for (int i = 0; i < N_CPU_UNITS_MAX; i ++)
      for (int p = 0; p < N_CPU_UNITS; p ++)
        cpu_array [i] . ports [p] . inuse = false;
  }

// A scu is trying to attach a cable to us
//  to my port cpu_unit_num, cpu_port_num
//  from it's port scu_unit_num, scu_port_num
//

t_stat cable_to_cpu (int cpu_unit_num, int cpu_port_num, int scu_unit_num, int scu_port_num)
  {
    if (cpu_unit_num < 0 || cpu_unit_num >= cpu_dev . numunits)
      {
        //sim_debug (DBG_ERR, & sys_dev, "cable_to_cpu: cpu_unit_num out of range <%d>\n", cpu_unit_num);
        sim_printf ("cable_to_cpu: cpu_unit_num out of range <%d>\n", cpu_unit_num);
        return SCPE_ARG;
      }

    if (cpu_port_num < 0 || cpu_port_num >= N_CPU_PORTS)
      {
        //sim_debug (DBG_ERR, & sys_dev, "cable_to_cpu: cpu_port_num out of range <%d>\n", cpu_port_num);
        sim_printf ("cable_to_cpu: cpu_port_num out of range <%d>\n", cpu_port_num);
        return SCPE_ARG;
      }

    if (cpu_array [cpu_unit_num] . ports [cpu_port_num] . inuse)
      {
        //sim_debug (DBG_ERR, & sys_dev, "cable_to_cpu: socket in use\n");
        sim_printf ("cable_to_cpu: socket in use\n");
        return SCPE_ARG;
      }

    DEVICE * devp = & scu_dev;
    UNIT * unitp = & scu_unit [scu_unit_num];
     
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . inuse = true;
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . scu_unit_num = scu_unit_num;

    cpu_array [cpu_unit_num] . ports [cpu_port_num] . devp = devp;
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . unitp  = unitp;

    unitp -> u3 = cpu_port_num;
    unitp -> u4 = 0;
    unitp -> u5 = cpu_unit_num;

    setup_scpage_map ();

    return SCPE_OK;
  }

static t_stat cpu_show_config(FILE *st, UNIT *uptr, int val, void *desc)
{
    int unit_num = UNIT_NUM (uptr);
    if (unit_num < 0 || unit_num >= cpu_dev . numunits)
      {
        sim_debug (DBG_ERR, & cpu_dev, "cpu_show_config: Invalid unit number %d\n", unit_num);
        sim_printf ("error: invalid unit number %d\n", unit_num);
        return SCPE_ARG;
      }

    sim_printf ("CPU unit number %d\n", unit_num);

    sim_printf("Fault base:               %03o(8)\n", switches . FLT_BASE);
    sim_printf("CPU number:               %01o(8)\n", switches . cpu_num);
    sim_printf("Data switches:            %012llo(8)\n", switches . data_switches);
    for (int i = 0; i < N_CPU_PORTS; i ++)
      {
        sim_printf("Port%c enable:             %01o(8)\n", 'A' + i, switches . enable [i]);
        sim_printf("Port%c init enable:        %01o(8)\n", 'A' + i, switches . init_enable [i]);
        sim_printf("Port%c assignment:         %01o(8)\n", 'A' + i, switches . assignment [i]);
        sim_printf("Port%c interlace:          %01o(8)\n", 'A' + i, switches . assignment [i]);
        sim_printf("Port%c store size:         %01o(8)\n", 'A' + i, switches . store_size [i]);
      }
    sim_printf("Processor mode:           %s [%o]\n", switches . proc_mode ? "Multics" : "GCOS", switches . proc_mode);
    sim_printf("Processor speed:          %02o(8)\n", switches . proc_speed);
    sim_printf("Invert Absolute:          %01o(8)\n", switches . invert_absolute);
    sim_printf("Bit 29 test code:         %01o(8)\n", switches . b29_test);
    sim_printf("DIS enable:               %01o(8)\n", switches . dis_enable);
    sim_printf("AutoAppend disable:       %01o(8)\n", switches . auto_append_disable);
    sim_printf("LPRPn set high bits only: %01o(8)\n", switches . lprp_highonly);
    sim_printf("Steady clock:             %01o(8)\n", switches . steady_clock);
    sim_printf("Degenerate mode:          %01o(8)\n", switches . degenerate_mode);
    sim_printf("Append after:             %01o(8)\n", switches . append_after);
    sim_printf("Super user:               %01o(8)\n", switches . super_user);
    sim_printf("EPP hack:                 %01o(8)\n", switches . epp_hack);
    sim_printf("Halt on unimplemented:    %01o(8)\n", switches . halt_on_unimp);
    sim_printf("Disable PTWAN/STWAM:      %01o(8)\n", switches . disable_wam);

    return SCPE_OK;
}

//
// set cpu0 config=<blah> [;<blah>]
//
//    blah =
//           faultbase = n
//           num = n
//           data = n
//           portenable = n
//           portconfig = n
//           portinterlace = n
//           mode = n
//           speed = n
//    Hacks:
//           invertabsolute = n
//           b29test = n // deprecated
//           dis_enable = n
//           auto_append_disable = n // still need for 20184, not for t4d
//           lprp_highonly = n // deprecated
//           steadyclock = on|off
//           degenerate_mode = n // deprecated
//           append_after = n
//           super_user = n
//           epp_hack = n
//           halt_on_unimplmented = n
//           disable_wam = 0

static config_value_list_t cfg_multics_fault_base [] =
  {
    { "multics", 2 },
    { NULL }
  };

static config_value_list_t cfg_on_off [] =
  {
    { "off", 0 },
    { "on", 1 },
    { "disable", 0 },
    { "enable", 1 },
    { NULL }
  };

static config_value_list_t cfg_cpu_mode [] =
  {
    { "gcos", 0 },
    { "multics", 1 },
    { NULL }
  };

static config_value_list_t cfg_port_letter [] =
  {
    { "a", 0 },
    { "b", 1 },
    { "c", 2 },
    { "d", 3 },
    { NULL }
  };

static config_value_list_t cfg_interlace [] =
  {
    { "off", 0 },
    { "2", 2 },
    { "4", 4 },
    { NULL }
  };

static config_value_list_t cfg_size_list [] =
  {
    { "32", 0 },
    { "64", 1 },
    { "128", 2 },
    { "256", 3 },
    { "512", 4 },
    { "1024", 5 },
    { "2048", 6 },
    { "4096", 7 },
    { "32K", 0 },
    { "64K", 1 },
    { "128K", 2 },
    { "256K", 3 },
    { "512K", 4 },
    { "1024K", 5 },
    { "2048K", 6 },
    { "4096K", 7 },
    { "1M", 5 },
    { "2M", 6 },
    { "4M", 7 },
    { NULL }
  };

static config_list_t cpu_config_list [] =
  {
    /*  0 */ { "faultbase", 0, 0177, cfg_multics_fault_base },
    /*  1 */ { "num", 0, 07, NULL },
    /*  2 */ { "data", 0, 0777777777777, NULL },
    /*  3 */ { "mode", 0, 01, cfg_cpu_mode }, 
    /*  4 */ { "speed", 0, 017, NULL }, // XXX use keywords
    /*  5 */ { "port", 0, N_CPU_PORTS - 1, cfg_port_letter },
    /*  6 */ { "assignment", 0, 7, NULL },
    /*  7 */ { "interlace", 0, 1, cfg_interlace },
    /*  8 */ { "enable", 0, 1, cfg_on_off },
    /*  9 */ { "init_enable", 0, 1, cfg_on_off },
    /* 10 */ { "store_size", 0, 7, cfg_size_list },

    // Hacks

    /* 11 */ { "invertabsolute", 0, 1, cfg_on_off }, 
    /* 12 */ { "b29test", 0, 1, cfg_on_off }, 
    /* 13 */ { "dis_enable", 0, 1, cfg_on_off }, 
    /* 14 */ { "auto_append_disable", 0, 1, cfg_on_off }, 
    /* 15 */ { "lprp_highonly", 0, 1, cfg_on_off }, 
    /* 16 */ { "steady_clock", 0, 1, cfg_on_off },
    /* 17 */ { "degenerate_mode", 0, 1, cfg_on_off },
    /* 18 */ { "append_after", 0, 1, cfg_on_off },
    /* 19 */ { "super_user", 0, 1, cfg_on_off },
    /* 20 */ { "epp_hack", 0, 1, cfg_on_off },
    /* 21 */ { "halt_on_unimplemented", 0, 1, cfg_on_off },
    /* 22 */ { "disable_wam", 0, 1, cfg_on_off },
    { NULL }
  };

static t_stat cpu_set_config (UNIT * uptr, int32 value, char * cptr, void * desc)
  {
// XXX Minor bug; this code doesn't check for trailing garbage

    int cpu_unit_num = UNIT_NUM (uptr);
    if (cpu_unit_num < 0 || cpu_unit_num >= cpu_dev . numunits)
      {
        sim_debug (DBG_ERR, & cpu_dev, "cpu_set_config: Invalid unit number %d\n", cpu_unit_num);
        sim_printf ("error: cpu_set_config: invalid unit number %d\n", cpu_unit_num);
        return SCPE_ARG;
      }

    static int port_num = 0;

    config_state_t cfg_state = { NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("cpu_set_config", cptr, cpu_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 

            case -1: // done
              break;

            case  0: // FAULTBASE
              switches . FLT_BASE = v;
              break;

            case  1: // NUM
              switches . cpu_num = v;
              break;

            case  2: // DATA
              switches . data_switches = v;
              break;

            case  3: // MODE
              switches . proc_mode = v;
              break;

            case  4: // SPEED
              switches . proc_speed = v;
              break;

            case  5: // PORT
              port_num = v;
              break;

            case  6: // ASSIGNMENT
              switches . assignment [port_num] = v;
              break;

            case  7: // INTERLACE
              switches . interlace [port_num] = v;
              break;

            case  8: // ENABLE
              switches . enable [port_num] = v;
              break;

            case  9: // INIT_ENABLE
              switches . init_enable [port_num] = v;
              break;

            case 10: // STORE_SIZE
              switches . store_size [port_num] = v;
              break;

            case 11: // INVERTABSOLUTE
              switches . invert_absolute = v;
              break;

            case 12: // B29TEST
              switches . b29_test = v;
              break;

            case 13: // DIS_ENABLE
              switches . dis_enable = v;
              break;

            case 14: // AUTO_APPEND_DISABLE
              switches . auto_append_disable = v;
              break;

            case 15: // LPRP_HIGHONLY
              switches . lprp_highonly = v;
              break;

            case 16: // STEADY_CLOCK
              switches . steady_clock = v;
              break;

            case 17: // DEGENERATE_MODE
              switches . degenerate_mode = v;
              break;

            case 18: // APPEND_AFTER
              switches . append_after = v;
              break;

            case 19: // SUPER_USER
              switches . super_user = v;
              break;

            case 20: // EPP_HACK
              switches . epp_hack = v;
              break;

            case 21: // HALT_ON_UNIMPLEMENTED
              switches . halt_on_unimp = v;
              break;

            case 22: // DISABLE_WAM
              switches . disable_wam = v;
              break;

            default:
              sim_debug (DBG_ERR, & cpu_dev, "cpu_set_config: Invalid cfgparse rc <%d>\n", rc);
              sim_printf ("error: cpu_set_config: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }


