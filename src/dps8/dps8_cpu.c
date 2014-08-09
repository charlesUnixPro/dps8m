 /**
 * \file dps8_cpu.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"
#include "dps8_addrmods.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_ins.h"
#include "dps8_loader.h"
#include "dps8_math.h"
#include "dps8_scu.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_iefp.h"
#include "dps8_faults.h"
#ifdef MULTIPASS
#include "dps8_mp.h"
#endif

// XXX Use this when we assume there is only a single cpu unit
#define ASSUME0 0

static void cpu_reset_array (void);
static bool clear_TEMPORARY_ABSOLUTE_mode (void);
static void set_TEMPORARY_ABSOLUTE_mode (void);
static void setCpuCycle (cycles_t cycle);

// CPU data structures

#define N_CPU_UNITS 1

// The DPS8M had only 4 ports

static UNIT cpu_unit [N_CPU_UNITS] = {{ UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL }};
#define UNIT_NUM(uptr) ((uptr) - cpu_unit)

static t_stat cpu_show_config(FILE *st, UNIT *uptr, int val, void *desc);
static t_stat cpu_set_config (UNIT * uptr, int32 value, char * cptr, void * desc);
static int cpu_show_stack(FILE *st, UNIT *uptr, int val, void *desc);

static MTAB cpu_mod[] = {
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO /* | MTAB_VALR */, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      cpu_set_config,         /* validation routine */
      cpu_show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL // help
    },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_NC,
      0, "STACK", NULL,
      NULL, cpu_show_stack, NULL, NULL },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
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
    { "INTR",       DBG_INTR        },
    { "CORE",       DBG_CORE        },
    { "CYCLE",      DBG_CYCLE       },
    { "CAC",        DBG_CAC         },
    { NULL,         0               }
};

// This is part of the simh interface
const char *sim_stop_messages[] = {
    "Unknown error",           // STOP_UNK
    "Unimplemented Opcode",    // STOP_UNIMP
    "DIS instruction",         // STOP_DIS
    "Breakpoint",              // STOP_BKPT
    "BUG",                     // STOP_BUG
    "Fault cascade",           // STOP_FLT_CASCADE
    "Halt",                    // STOP_HALT
    "Illegal Opcode",          // STOP_ILLOP
};

/* End of simh interface */

/* Processor configuration switches 
 *
 * From AM81-04 Multics System Maintainance Procedures
 *
 * "A level 68 IOM system may contain a maximum of 7 CPUs, 4 IOMs, 8 SCUs and 
 * 16MW of memory
 * [CAC]: but AN87 says multics only supports two IOMs
 * 
 * ASSIGNMENT: 3 toggle switches determine the base address of the SCU 
 * connected to the port. The base address (in KW) is the product of this 
 * number and the value defined by the STORE SIZE patch plug for the port.
 *
 * ADDRESS RANGE: toggle FULL/HALF. Determines the size of the SCU as full or 
 * half of the STORE SIZE patch.
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

static int is_eis[1024];    // hack

// XXX PPR.IC oddly incremented. ticket #6
// xec_side_effect is used to record the number of EIS operand descriptor
// words consumed during XEC processing; this allows the PPR.IC to be
// correctly incremented.
int xec_side_effect; // hack

void init_opcodes (void)
  {
    memset(is_eis, 0, sizeof(is_eis));
    
#define IS_EIS(opc) is_eis [(opc << 1) | 1] = 1;
    IS_EIS (opcode1_cmpc);
    IS_EIS (opcode1_scd);
    IS_EIS (opcode1_scdr);
    IS_EIS (opcode1_scm);
    IS_EIS (opcode1_scmr);
    IS_EIS (opcode1_tct);
    IS_EIS (opcode1_tctr);
    IS_EIS (opcode1_mlr);
    IS_EIS (opcode1_mrl);
    IS_EIS (opcode1_mve);
    IS_EIS (opcode1_mvt);
    IS_EIS (opcode1_cmpn);
    IS_EIS (opcode1_mvn);
    IS_EIS (opcode1_mvne);
    IS_EIS (opcode1_csl);
    IS_EIS (opcode1_csr);
    IS_EIS (opcode1_cmpb);
    IS_EIS (opcode1_sztl);
    IS_EIS (opcode1_sztr);
    IS_EIS (opcode1_btd);
    IS_EIS (opcode1_dtb);
    IS_EIS (opcode1_dv3d);
  }


/*
 * initialize segment table according to the contents of DSBR ...
 */

static t_stat dpsCmd_InitUnpagedSegmentTable ()
  {
    if (DSBR . U == 0)
      {
        sim_printf  ("Cannot initialize unpaged segment table because DSBR.U says it is \"paged\"\n");
        return SCPE_OK;    // need a better return value
      }
    
    if (DSBR . ADDR == 0) // DSBR *probably* not initialized. Issue warning and ask....
      {
        if (! get_yn ("DSBR *probably* uninitialized (DSBR.ADDR == 0). Proceed anyway [N]?", FALSE))
          {
            return SCPE_OK;
          }
      }
    
    word15 segno = 0;
    while (2 * segno < (16 * (DSBR.BND + 1)))
      {
        //generate target segment SDW for DSBR.ADDR + 2 * segno.
        word24 a = DSBR.ADDR + 2 * segno;
        
        // just fill with 0's for now .....
        core_write ((a + 0) & PAMASK, 0);
        core_write ((a + 1) & PAMASK, 0);
        
        segno ++; // onto next segment SDW
      }
    
    if ( !sim_quiet)
      sim_printf("zero-initialized segments 0 .. %d\n", segno - 1);
    return SCPE_OK;
  }

static t_stat dpsCmd_InitSDWAM ()
  {
    memset (SDWAM, 0, sizeof (SDWAM));
    
    if (! sim_quiet)
      sim_printf ("zero-initialized SDWAM\n");
    return SCPE_OK;
  }

// Assumes unpaged DSBR

_sdw0 *fetchSDW (word15 segno)
  {
    word36 SDWeven, SDWodd;
    
    core_read2 ((DSBR . ADDR + 2 * segno) & PAMASK, & SDWeven, & SDWodd);
    
    // even word
    static _sdw0 _s;
    
    _sdw0 *SDW = &_s;
    memset (SDW, 0, sizeof (_s));
    
    SDW -> ADDR = (SDWeven >> 12) & 077777777;
    SDW -> R1 = (SDWeven >> 9) & 7;
    SDW -> R2 = (SDWeven >> 6) & 7;
    SDW -> R3 = (SDWeven >> 3) & 7;
    SDW -> F = TSTBIT(SDWeven, 2);
    SDW -> FC = SDWeven & 3;
    
    // odd word
    SDW -> BOUND = (SDWodd >> 21) & 037777;
    SDW -> R = TSTBIT(SDWodd, 20);
    SDW -> E = TSTBIT(SDWodd, 19);
    SDW -> W = TSTBIT(SDWodd, 18);
    SDW -> P = TSTBIT(SDWodd, 17);
    SDW -> U = TSTBIT(SDWodd, 16);
    SDW -> G = TSTBIT(SDWodd, 15);
    SDW -> C = TSTBIT(SDWodd, 14);
    SDW -> EB = SDWodd & 037777;
    
    return SDW;
  }

static char * strDSBR (void)
  {
    static char buff [256];
    sprintf (buff, "DSBR: ADDR=%06o BND=%05o U=%o STACK=%04o", DSBR.ADDR, DSBR.BND, DSBR.U, DSBR.STACK);
    return buff;
  }

static void printDSBR (void)
  {
    sim_printf ("%s\n", strDSBR ());
  }


char * strSDW0 (_sdw0 * SDW)
  {
    static char buff [256];
    
    //if (SDW->ADDR == 0 && SDW->BOUND == 0) // need a better test
    if (! SDW -> F) 
      sprintf (buff, "*** Uninitialized ***");
    else
      sprintf (buff, "ADDR=%06o R1=%o R2=%o R3=%o F=%o FC=%o BOUND=%o R=%o E=%o W=%o P=%o U=%o G=%o C=%o EB=%o",
               SDW -> ADDR, SDW -> R1, SDW -> R2, SDW -> R3, SDW -> F,
               SDW -> FC, SDW -> BOUND, SDW -> R, SDW -> E, SDW -> W,
               SDW -> P, SDW -> U, SDW -> G, SDW -> C, SDW -> EB);
    return buff;
 }

static void printSDW0 (_sdw0 *SDW)
  {
    sim_printf ("%s\n", strSDW0 (SDW));
  }

static t_stat dpsCmd_DumpSegmentTable()
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
                    for (word18 offset = 0; offset < 16u * (SDW0.BOUND + 1u); offset += 1024u)
                    {
                        word24 y2 = offset % 1024;
                        word24 x2 = (offset - y2) / 1024;

                        // 10. Fetch the target segment PTW(x2) from SDW(segno).ADDR + x2.

                        word36 PTWx2;
                        core_read ((SDW0 . ADDR + x2) & PAMASK, & PTWx2);

                        struct _ptw0 PTW_2;
                        PTW_2.ADDR = GETHI(PTWx2);
                        PTW_2.U = TSTBIT(PTWx2, 9);
                        PTW_2.M = TSTBIT(PTWx2, 6);
                        PTW_2.F = TSTBIT(PTWx2, 2);
                        PTW_2.FC = PTWx2 & 3;

                         sim_printf ("        %06o  Addr %06o U %o M %o F %o FC %o\n", 
                                     offset, PTW_2.ADDR, PTW_2.U, PTW_2.M, PTW_2.F, PTW_2.FC);

                      }
                  }
            }
        }
    }

    return SCPE_OK;
}

//! custom command "dump"
t_stat dpsCmd_Dump (UNUSED int32 arg, char *buf)
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
t_stat dpsCmd_Init (UNUSED int32 arg, char *buf)
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
t_stat dpsCmd_Segment (UNUSED int32  arg, char *buf)
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
t_stat dpsCmd_Segments (UNUSED int32 arg, char *buf)
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

static void ic_history_init(void);
/*! Reset routine */
static t_stat cpu_reset_mm (UNUSED DEVICE * dptr)
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
    
    setCpuCycle (FETCH_cycle);

    sys_stats . total_cycles = 0;
    for (int i = 0; i < N_FAULTS; i ++)
      sys_stats . total_faults [i] = 0;

//#if FEAT_INSTR_STATS
    memset(&sys_stats, 0, sizeof(sys_stats));
//#endif
    
    return 0;
}

static t_stat cpu_boot (UNUSED int32 unit_num, UNUSED DEVICE * dptr)
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
    for (uint pg = 0; pg < N_SCPAGES; pg ++)
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

        for (uint pg = 0; pg < sz; pg ++)
          {
            uint scpg = base + pg;
            if (scpg < N_SCPAGES)
              scpage_map [scpg] = port_num;
          }
      }
    for (uint pg = 0; pg < N_SCPAGES; pg ++)
      sim_debug (DBG_DEBUG, & cpu_dev, "setup_scpage_map: %d:%d\n", pg, scpage_map [pg]);
  }

int query_scpage_map (word24 addr)
  {
    uint scpg = addr / SCPAGE;
    if (scpg < N_SCPAGES)
      return scpage_map [scpg];
    return -1;
  }

// called once initialization

void cpu_init (void)
{
  memset (& switches, 0, sizeof (switches));
  switches . FLT_BASE = 2; // Some of the UnitTests assume this
}

// DPS8 Memory of 36 bit words is implemented as an array of 64 bit words.
// Put state information into the unused high order bits.
#define MEM_UNINITIALIZED 0x4000000000000000LLU

static t_stat cpu_reset (DEVICE *dptr)
{
    if (M)
        free(M);
    
    M = (word36 *) calloc (MEMSIZE, sizeof (word36));
    if (M == NULL)
        return SCPE_MEM;
    
    //memset (M, -1, MEMSIZE * sizeof (word36));

    // Fill DPS8 memory with zeros, plus a flag only visible to the emulator
    // marking the memory as uninitialized.

    for (uint i = 0; i < MEMSIZE; i ++)
      M [i] = MEM_UNINITIALIZED;

    rA = 0;
    rQ = 0;
    
    PPR.IC = 0;
    PPR.PRR = 0;
    PPR.PSR = 0;
    PPR.P = 1;
    RSDWH_R1 = 0;

    rTR = 0;
 
    processorCycle = UNKNOWN_CYCLE;
    //processorAddressingMode = ABSOLUTE_MODE;
    set_addr_mode(ABSOLUTE_mode);
    
    sim_brk_types = sim_brk_dflt = SWMASK ('E');

    sys_stats . total_cycles = 0;
    for (int i = 0; i < N_FAULTS; i ++)
      sys_stats . total_faults [i] = 0;
    
    CMR.luf = 3;    // default of 16 mS
    
    // XXX free up previous deferred segments (if any)
    
    
    cpu_reset_mm(dptr);

    cpu_reset_array ();

    setup_scpage_map ();

    initializeTheMatrix();

    tidy_cu ();

    cpu . wasXfer = false;
    cpu . wasInhibited = false;

    cpu . interrupt_flag = false;
    cpu . g7_flag = false;

    return SCPE_OK;
}

/*! Memory examine */
//  t_stat examine_routine (t_val *eval_array, t_addr addr, UNIT *uptr, int32 switches) – Copy 
//  sim_emax consecutive addresses for unit uptr, starting at addr, into eval_array. The switch 
//  variable has bit<n> set if the n’th letter was specified as a switch to the examine command. 
// Not true...

static t_stat cpu_ex (t_value *vptr, t_addr addr, UNUSED UNIT * uptr, UNUSED int32 sw)
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
static t_stat cpu_dep (t_value val, t_addr addr, UNUSED UNIT * uptr, 
                       UNUSED int32 sw)
{
    if (addr >= MEMSIZE) return SCPE_NXM;
    M[addr] = val & DMASK;
    return SCPE_OK;
}


enum _processor_cycle_type processorCycle;                  ///< to keep tract of what type of cycle the processor is in
//enum _processor_addressing_mode processorAddressingMode;    ///< what addressing mode are we using
//enum _processor_operating_mode processorOperatingMode;      ///< what operating mode

// h6180 stuff
/* [map] designates mapping into 36-bit word from DPS-8 proc manual */

/* GE-625/635 */

word36 rA;      /*!< accumulator */
word36 rQ;      /*!< quotient */
word8  rE;      /*!< exponent [map: rE, 28 0's] */

word18 rX[8];   /*!< index */


word27 rTR; /*!< timer [map: TR, 9 0's] */
word24 rY;     /*!< address operand */
word8 rTAG; /*!< instruction tag */

word8 tTB; /*!< char size indicator (TB6=6-bit,TB9=9-bit) [3b] */
word8 tCF; /*!< character position field [3b] */


///* H6180; L68; DPS-8M */
//
word3 rRALR; /*!< ring alarm [3b] [map: 33 0's, RALR] */

// end h6180 stuff

struct _tpr TPR;    ///< Temporary Pointer Register
struct _ppr PPR;    ///< Procedure Pointer Register
struct _par PAR[8]; ///< pointer/address resisters
struct _bar BAR;    ///< Base Address Register
struct _dsbr DSBR;  ///< Descriptor Segment Base Register


// XXX given this is not real hardware we can eventually remove the SDWAM -- I think. But for now just leave it in.
// For the DPS 8M processor, the SDW associative memory will hold the 64 MRU SDWs and have a 4-way set associative organization with LRU replacement.

struct _sdw  SDWAM[64], *SDW = &SDWAM[0];    ///< Segment Descriptor Word Associative Memory & working SDW
struct _sdw0 SDW0;  ///< a SDW not in SDWAM

struct _ptw PTWAM[64], *PTW = &PTWAM[0];    ///< PAGE TABLE WORD ASSOCIATIVE MEMORY and working PTW
struct _ptw0 PTW0;  ///< a PTW not in PTWAM (PTWx1)

word3    RSDWH_R1; // Track the ring number of the last SDW

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
    BITFNAM(MIIF,  1, z1),   /*!< mid-instruction interrupt fault */ ///< 0000040
    BITFNAM(TRUNC, 1, z1),    /*!< truncation */ ///< 0000100
    BITFNAM(NBAR,  1, z1),   /*!< not BAR mode */ ///< 0000200
    BITFNAM(PMASK, 1, z1),   /*!< parity mask */ ///< 0000400
    BITFNAM(PAR,   1, z1),    /*!< parity error */ ///< 0001000
    BITFNAM(TALLY, 1, z1),   /*!< tally runout */ ///< 0002000
    BITFNAM(OMASK, 1, z1),    /*!< overflow mask */ ///< 0004000
    BITFNAM(EUFL,  1, z1),   /*!< exponent underflow */ ///< 0010000
    BITFNAM(EOFL,  1, z1),   /*!< exponent overflow */ ///< 0020000
    BITFNAM(OFLOW, 1, z1),   /*!< overflow */ ///< 0040000
    BITFNAM(CARRY, 1, z1),   /*!< carry */ ///< 0100000
    BITFNAM(NEG,   1, z1),    /*!< negative */ ///< 0200000
    BITFNAM(ZERO,  1, z1),   /*!< zero */ ///< 0400000
    { NULL, 0, 0, NULL, NULL }
};

static REG cpu_reg[] = {
    { ORDATA (IC, PPR.IC, VASIZE), 0, 0 },
    //{ ORDATA (IR, cu.IR, 18), 0, 0 },
    { ORDATADF (IR, cu.IR, 18, "Indicator Register", dps8_IR_bits), 0, 0 },
    
    //    { FLDATA (Zero, cu.IR, F_V_A), 0, 0 },
    //    { FLDATA (Negative, cu.IR, F_V_B), 0, 0 },
    //    { FLDATA (Carry, cu.IR, F_V_C), 0, 0 },
    //    { FLDATA (Overflow, cu.IR, F_V_D), 0, 0 },
    //    { FLDATA (ExpOvr, cu.IR, F_V_E), 0, 0 },
    //    { FLDATA (ExpUdr, cu.IR, F_V_F), 0, 0 },
    //    { FLDATA (OvrMask, cu.IR, F_V_G), 0, 0 },
    //    { FLDATA (TallyRunOut, cu.IR, F_V_H), 0, 0 },
    //    { FLDATA (ParityErr, cu.IR, F_V_I), 0, 0 }, ///< Yeah, right!
    //    { FLDATA (ParityMask, cu.IR, F_V_J), 0, 0 },
    //    { FLDATA (NotBAR, cu.IR, F_V_K), 0, 0 },
    //    { FLDATA (Trunc, cu.IR, F_V_L), 0, 0 },
    //    { FLDATA (MidInsFlt, cu.IR, F_V_M), 0, 0 },
    //    { FLDATA (AbsMode, cu.IR, F_V_N), 0, 0 },
    //    { FLDATA (HexMode, cu.IR, F_V_O), 0, 0 },
    
    { ORDATA (A, rA, 36), 0, 0 },
    { ORDATA (Q, rQ, 36), 0, 0 },
    { ORDATA (E, rE, 8), 0, 0 },
    
    { ORDATA (X0, rX[0], 18), 0, 0 },
    { ORDATA (X1, rX[1], 18), 0, 0 },
    { ORDATA (X2, rX[2], 18), 0, 0 },
    { ORDATA (X3, rX[3], 18), 0, 0 },
    { ORDATA (X4, rX[4], 18), 0, 0 },
    { ORDATA (X5, rX[5], 18), 0, 0 },
    { ORDATA (X6, rX[6], 18), 0, 0 },
    { ORDATA (X7, rX[7], 18), 0, 0 },
    
    { ORDATA (PPR.IC,  PPR.IC,  18), 0, 0 },
    { ORDATA (PPR.PRR, PPR.PRR,  3), 0, 0 },
    { ORDATA (PPR.PSR, PPR.PSR, 15), 0, 0 },
    { ORDATA (PPR.P,   PPR.P,    1), 0, 0 },
    
    { ORDATA (DSBR.ADDR,  DSBR.ADDR,  24), 0, 0 },
    { ORDATA (DSBR.BND,   DSBR.BND,   14), 0, 0 },
    { ORDATA (DSBR.U,     DSBR.U,      1), 0, 0 },
    { ORDATA (DSBR.STACK, DSBR.STACK, 12), 0, 0 },
    
    { ORDATA (BAR.BASE,  BAR.BASE,  9), 0, 0 },
    { ORDATA (BAR.BOUND, BAR.BOUND, 9), 0, 0 },
    
    //{ ORDATA (FAULTBASE, rFAULTBASE, 12), 0, 0 }, ///< only top 7-msb are used
    
    { ORDATA (PR0.SNR, PR[0].SNR, 18), 0, 0 },
    { ORDATA (PR1.SNR, PR[1].SNR, 18), 0, 0 },
    { ORDATA (PR2.SNR, PR[2].SNR, 18), 0, 0 },
    { ORDATA (PR3.SNR, PR[3].SNR, 18), 0, 0 },
    { ORDATA (PR4.SNR, PR[4].SNR, 18), 0, 0 },
    { ORDATA (PR5.SNR, PR[5].SNR, 18), 0, 0 },
    { ORDATA (PR6.SNR, PR[6].SNR, 18), 0, 0 },
    { ORDATA (PR7.SNR, PR[7].SNR, 18), 0, 0 },
    
    { ORDATA (PR0.RNR, PR[0].RNR, 18), 0, 0 },
    { ORDATA (PR1.RNR, PR[1].RNR, 18), 0, 0 },
    { ORDATA (PR2.RNR, PR[2].RNR, 18), 0, 0 },
    { ORDATA (PR3.RNR, PR[3].RNR, 18), 0, 0 },
    { ORDATA (PR4.RNR, PR[4].RNR, 18), 0, 0 },
    { ORDATA (PR5.RNR, PR[5].RNR, 18), 0, 0 },
    { ORDATA (PR6.RNR, PR[6].RNR, 18), 0, 0 },
    { ORDATA (PR7.RNR, PR[7].RNR, 18), 0, 0 },
    
    //{ ORDATA (PR0.BITNO, PR[0].PBITNO, 18), 0, 0 },
    //{ ORDATA (PR1.BITNO, PR[1].PBITNO, 18), 0, 0 },
    //{ ORDATA (PR2.BITNO, PR[2].PBITNO, 18), 0, 0 },
    //{ ORDATA (PR3.BITNO, PR[3].PBITNO, 18), 0, 0 },
    //{ ORDATA (PR4.BITNO, PR[4].PBITNO, 18), 0, 0 },
    //{ ORDATA (PR5.BITNO, PR[5].PBITNO, 18), 0, 0 },
    //{ ORDATA (PR6.BITNO, PR[6].PBITNO, 18), 0, 0 },
    //{ ORDATA (PR7.BITNO, PR[7].PBITNO, 18), 0, 0 },
    
    //{ ORDATA (AR0.BITNO, PR[0].ABITNO, 18), 0, 0 },
    //{ ORDATA (AR1.BITNO, PR[1].ABITNO, 18), 0, 0 },
    //{ ORDATA (AR2.BITNO, PR[2].ABITNO, 18), 0, 0 },
    //{ ORDATA (AR3.BITNO, PR[3].ABITNO, 18), 0, 0 },
    //{ ORDATA (AR4.BITNO, PR[4].ABITNO, 18), 0, 0 },
    //{ ORDATA (AR5.BITNO, PR[5].ABITNO, 18), 0, 0 },
    //{ ORDATA (AR6.BITNO, PR[6].ABITNO, 18), 0, 0 },
    //{ ORDATA (AR7.BITNO, PR[7].ABITNO, 18), 0, 0 },
    
    { ORDATA (PR0.WORDNO, PR[0].WORDNO, 18), 0, 0 },
    { ORDATA (PR1.WORDNO, PR[1].WORDNO, 18), 0, 0 },
    { ORDATA (PR2.WORDNO, PR[2].WORDNO, 18), 0, 0 },
    { ORDATA (PR3.WORDNO, PR[3].WORDNO, 18), 0, 0 },
    { ORDATA (PR4.WORDNO, PR[4].WORDNO, 18), 0, 0 },
    { ORDATA (PR5.WORDNO, PR[5].WORDNO, 18), 0, 0 },
    { ORDATA (PR6.WORDNO, PR[6].WORDNO, 18), 0, 0 },
    { ORDATA (PR7.WORDNO, PR[7].WORDNO, 18), 0, 0 },
    
    //{ ORDATA (PR0.CHAR, PR[0].CHAR, 18), 0, 0 },
    //{ ORDATA (PR1.CHAR, PR[1].CHAR, 18), 0, 0 },
    //{ ORDATA (PR2.CHAR, PR[2].CHAR, 18), 0, 0 },
    //{ ORDATA (PR3.CHAR, PR[3].CHAR, 18), 0, 0 },
    //{ ORDATA (PR4.CHAR, PR[4].CHAR, 18), 0, 0 },
    //{ ORDATA (PR5.CHAR, PR[5].CHAR, 18), 0, 0 },
    //{ ORDATA (PR6.CHAR, PR[6].CHAR, 18), 0, 0 },
    //{ ORDATA (PR7.CHAR, PR[7].CHAR, 18), 0, 0 },
    
    /*
     { ORDATA (EBR, ebr, EBR_N_EBR), 0, 0 },
     { FLDATA (PGON, ebr, EBR_V_PGON), 0, 0 },
     { FLDATA (T20P, ebr, EBR_V_T20P), 0, 0 },
     { ORDATA (UBR, ubr, 36), 0, 0 },
     { GRDATA (CURAC, ubr, 8, 3, UBR_V_CURAC), REG_RO },
     { GRDATA (PRVAC, ubr, 8, 3, UBR_V_PRVAC) },
     { ORDATA (SPT, spt, 36), 0, 0 },
     { ORDATA (CST, cst, 36), 0, 0 },
     { ORDATA (PUR, pur, 36), 0, 0 },
     { ORDATA (CSTM, cstm, 36), 0, 0 },
     { ORDATA (HSB, hsb, 36), 0, 0 },
     { ORDATA (DBR1, dbr1, PASIZE), 0, 0 },
     { ORDATA (DBR2, dbr2, PASIZE), 0, 0 },
     { ORDATA (DBR3, dbr3, PASIZE), 0, 0 },
     { ORDATA (DBR4, dbr4, PASIZE), 0, 0 },
     { ORDATA (PCST, pcst, 36), 0, 0 },
     { ORDATA (PIENB, pi_enb, 7), 0, 0 },
     { FLDATA (PION, pi_on, 0), 0, 0 },
     { ORDATA (PIACT, pi_act, 7), 0, 0 },
     { ORDATA (PIPRQ, pi_prq, 7), 0, 0 },
     { ORDATA (PIIOQ, pi_ioq, 7), REG_RO },
     { ORDATA (PIAPR, pi_apr, 7), REG_RO },
     { ORDATA (APRENB, apr_enb, 8), 0, 0 },
     { ORDATA (APRFLG, apr_flg, 8), 0, 0 },
     { ORDATA (APRLVL, apr_lvl, 3), 0, 0 },
     { ORDATA (RLOG, rlog, 10), 0, 0 },
     { FLDATA (F1PR, its_1pr, 0), 0, 0 },
     { BRDATA (PCQ, pcq, 8, VASIZE, PCQ_SIZE), REG_RO+REG_CIRC },
     { ORDATA (PCQP, pcq_p, 6), REG_HRO },
     { DRDATA (INDMAX, ind_max, 8), PV_LEFT + REG_NZ },
     { DRDATA (XCTMAX, xct_max, 8), PV_LEFT + REG_NZ },
     { ORDATA (WRU, sim_int_char, 8), 0, 0 },
     { FLDATA (STOP_ILL, stop_op0, 0), 0, 0 },
     { BRDATA (REG, acs, 8, 36, AC_NUM * AC_NBLK) },
     */
    
    { NULL, NULL, 0, 0, 0, 0, NULL, NULL, 0, 0 }
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
    NULL,           /*!< logical name */
    NULL,           // help
    NULL,           // attach help
    NULL,           // help context
    NULL            // description
};

static t_stat reason;

jmp_buf jmpMain;        ///< This is where we should return to from a fault or interrupt (if necessary)

DCDstruct currentInstruction;

//static EISstruct E;

events_t events;
switches_t switches;
// the following two should probably be combined
cpu_state_t cpu;
ctl_unit_data_t cu;
du_unit_data_t du;


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
    for (uint scuUnitNum = 0; scuUnitNum < N_SCU_UNITS_MAX; scuUnitNum ++)
      {
        if (events . XIP [scuUnitNum])
          {
            return scuGetHighestIntr (scuUnitNum);
          }
      }
    return -1;
  }

#if 0
static uint get_highest_intr (void)
  {
// XXX In theory there needs to be interlocks on this?
    for (int int_num = N_INTERRUPTS - 1; int_num >= 0; int_num --)
      for (uint scu_num = 0; scu_num < N_SCU_UNITS_MAX; scu_num ++)
        if (events . interrupts [scu_num] [int_num])
          {
            events . interrupts [scu_num] [int_num] = 0;
            scu_clear_interrupt (scu_num, int_num);

            int cnt = 0;
            for (uint i = 0; i < N_INTERRUPTS; i ++)
              for (uint s = 0; s < N_SCU_UNITS_MAX; s ++)
                if (events . interrupts [s] [i])
                  {
                    cnt ++;
                    break;
//sim_printf ("%u %u\n", s, i);
                  }
            events . int_pending = !!cnt;

            cnt = 0;
            for (int i = 0; i < N_FAULT_GROUPS; i ++)
              if (events . fault [i])
                {
                  cnt ++;
                  break;
//sim_printf ("%u %u\n", s, i);
                }
            //events . any = !! cnt;
            events . fault_pending = !! cnt;
            //sim_printf ("int num %d (%o), pair_addr %o pend %d any %d\n", int_num, int_num, int_num * 2, events . int_pending, events . any);
            return int_num * 2;
          }
    return 1;
  }
#endif

bool sample_interrupts (void)
  {
    for (uint scuUnitNum = 0; scuUnitNum < N_SCU_UNITS_MAX; scuUnitNum ++)
      {
        if (events . XIP [scuUnitNum])
          {
            return true;
          }
      }
    return false;
  }

t_stat simh_hooks (void)
  {
    int reason = 0;
    // check clock queue 
    if (sim_interval <= 0)
      {
//int32 int0 = sim_interval;
        reason = sim_process_event ();
//sim_printf ("int delta %d\n", sim_interval - int0);
        if (reason)
          return reason;
      }
        
    sim_interval --;

    // breakpoint? 
    //if (sim_brk_summ && sim_brk_test (PPR.IC, SWMASK ('E')))
    // sim_brk_test expects a 32 bit address; PPR.IC into the low 18, and
    // PPR.PSR into the high 12
    if (sim_brk_summ &&
        sim_brk_test ((PPR.IC & 0777777) |
                      ((((t_addr) PPR.PSR) & 037777) << 18),
                      SWMASK ('E')))  /* breakpoint? */
      return STOP_BKPT; /* stop simulation */
    if (sim_deb_break && sim_timell () >= sim_deb_break)
      return STOP_BKPT; /* stop simulation */

    return reason;
  }       

static char * cycleStr (cycles_t cycle)
  {
    switch (cycle)
      {
        case ABORT_cycle:
          return "ABORT_cycle";
        case FAULT_cycle:
          return "FAULT_cycle";
        case EXEC_cycle:
          return "EXEC_cycle";
        case FAULT_EXEC_cycle:
          return "FAULT_EXEC_cycle";
        case FAULT_EXEC2_cycle:
          return "FAULT_EXEC2_cycle";
        case INTERRUPT_cycle:
          return "INTERRUPT_cycle";
        case INTERRUPT_EXEC_cycle:
          return "INTERRUPT_EXEC_cycle";
        case INTERRUPT_EXEC2_cycle:
          return "INTERRUPT_EXEC2_cycle";
        case FETCH_cycle:
          return "FETCH_cycle";
#if 0
        default:
          sim_printf ("setCpuCycle: cpu . cycle %d?\n", cpu . cycle);
          return "XXX unknown cycle";
#endif
     }
  }

static void setCpuCycle (cycles_t cycle)
  {
    sim_debug (DBG_CYCLE, & cpu_dev, "Setting cycle to %s\n",
               cycleStr (cycle));
    cpu . cycle = cycle;
  }
//
// Okay, lets treat this as a state machine
//
//  INTERRUPT_cycle
//     clear interrupt, load interrupt pair into instruction buffer
//     set INTERRUPT_EXEC_cycle
//  INTERRUPT_EXEC_cycle
//     execute instruction in instruction buffer
//     if (! transfer) set INTERUPT_EXEC2_cycle 
//     else set FETCH_cycle
//  INTERRUPT_EXEC2_cycle
//     execute odd instruction in instruction buffer
//     set INTERUPT_EXEC2_cycle 
//
//  FAULT_cycle
//     fetch fault pair into instruction buffer
//     set FAULT_EXEC_cycle
//  FAULT_EXEC_cycle
//     execute instructions in instruction buffer
//     if (! transfer) set FAULT_EXE2_cycle 
//     else set FETCH_cycle
//  FAULT_EXEC2_cycle
//     execute odd instruction in instruction buffer
//     set FETCH_cycle
//
//  FETCH_cycle
//     fetch instruction into instruction buffer
//     set EXEC_cycle
//
//  EXEC_cycle
//     execute instruction in instruction buffer
//     if (repeat conditions) keep cycling
//     if (pair) set EXEC2_cycle
//     else set FETCH_cycle
//  EXEC2_cycle
//     execute odd instruction in instruction buffer
//
//  XEC_cycle
//     load instruction into instruction buffer
//     set EXEC_cyvle
//
//  XED_cycle
//     load instruction pair into instruction buffer
//     set EXEC_cyvle
//  
// other extant cycles:
//  ABORT_cycle

static word36 instr_buf [2];

// This is part of the simh interface
t_stat sim_instr (void)
  {
#ifdef USE_IDLE
    sim_rtcn_init (0, 0);
#endif

    // Heh. This needs to be static; longjmp resets the value to NULL
    //static DCDstruct _ci;
    static DCDstruct * ci = & currentInstruction;
    
    // This allows long jumping to the top of the state machine
    int val = setjmp(jmpMain);

    switch (val)
    {
        case JMP_ENTRY:
        case JMP_REENTRY:
            reason = 0;
            break;
        case JMP_NEXT:
            goto nextInstruction;
#if 0
        case JMP_RETRY:
            goto jmpRetry;
        case JMP_TRA:
            goto jmpTra;
        case JMP_INTR:
            goto jmpIntr;
#endif
        case JMP_STOP:
            reason = STOP_HALT;
            goto leave;
        case JMP_SYNC_FAULT_RETURN:
            goto syncFaultReturn;
        case JMP_REFETCH:

            // Not necessarily so, but the only times
            // this path is taken is after an RCU returning
            // from an interrupt, which could only happen if
            // was xfer was false; or in a DIS cycle, in
            // which case we want it false so interrupts 
            // can happen.
            cpu . wasXfer = false;
             
            setCpuCycle (FETCH_cycle);
            break;
        case JMP_RESTART:
            setCpuCycle (EXEC_cycle);
            break;
        default:
          sim_printf ("longjmp value of %d unhandled\n", val);
            goto leave;
    }

    // Main instruction fetch/decode loop 

    do
      {

        reason = 0;

        // Process deferred events and breakpoints
        reason = simh_hooks ();
        if (reason)
          //return reason;
          break;

#ifdef MULTIPASS
       if (multipassStatsPtr) 
         {
           multipassStatsPtr -> A = rA;
           multipassStatsPtr -> Q = rQ;
           multipassStatsPtr -> E = rE;
           for (int i = 0; i < 8; i ++)
             {
               multipassStatsPtr -> X [i] = rX [i];
               multipassStatsPtr -> PAR [i] = PAR [i];
             }
           multipassStatsPtr -> IR = cu . IR;
           multipassStatsPtr -> TR = rTR;
           multipassStatsPtr -> RALR = rRALR;
         }
#endif

        // Manage the timer register // XXX this should be sync to the EXECUTE cycle, not the
                                     // simh clock clyce; move down...
                                     // Acutally have FETCH jump to EXECUTE
                                     // instead of breaking.

        rTR = (rTR - 1) & MASK27;
        if (rTR == MASK27) // passing thorugh 0...
          {
            if (switches . tro_enable)
              setG7fault (timer_fault, 0);
          }

        sim_debug (DBG_CYCLE, & cpu_dev, "Cycle switching to %s\n",
                   cycleStr (cpu . cycle));
        switch (cpu . cycle)
          {
            case INTERRUPT_cycle:
              {
                // In the INTERRUPT CYCLE, the processor safe-stores
                // the Control Unit Data (see Section 3) into 
                // program-invisible holding registers in preparation 
                // for a Store Control Unit (scu) instruction, enters 
                // temporary absolute mode, and forces the current 
                // ring of execution C(PPR.PRR) to
                // 0. It then issues an XEC system controller command 
                // to the system controller on the highest priority 
                // port for which there is a bit set in the interrupt 
                // present register.  

                uint intr_pair_addr = get_highest_intr ();
                cu . FI_ADDR = intr_pair_addr / 2;
                cu_safe_store ();

                // Temporary absolute mode
                set_TEMPORARY_ABSOLUTE_mode ();

                // Set to ring 0
                PPR . PRR = 0;
                TPR . TRR = 0;

                // Check that an interrupt is actually pending
                if (cpu . interrupt_flag)
                  {
                    // clear interrupt, load interrupt pair into instruction 
                    // buffer; set INTERRUPT_EXEC_cycle.

                    // In the h/w this is done later, but doing it now allows 
                    // us to avoid clean up for no interrupt pending.

                    if (intr_pair_addr != 1) // no interrupts 
                      {

                        sim_debug (DBG_INTR, & cpu_dev, "intr_pair_addr %u\n", 
                                   intr_pair_addr);

                        if_sim_debug (DBG_INTR, & cpu_dev) 
                          traceInstruction (DBG_INTR);

#ifdef MULTIPASS
                        if (multipassStatsPtr)
                          {
                            multipassStatsPtr -> intr_pair_addr = intr_pair_addr;
                          }
#endif
                        // get interrupt pair
                        core_read2 (intr_pair_addr, instr_buf, instr_buf + 1);

                        cpu . interrupt_flag = false;
                        setCpuCycle (INTERRUPT_EXEC_cycle);
                        break;
                      } // int_pair != 1
                  } // interrupt_flag

                // If we get here, there was no interrupt

                cpu . interrupt_flag = false;
                clear_TEMPORARY_ABSOLUTE_mode ();
                // Restores addressing mode 
                cu_safe_restore ();
                // We can only get here if wasXfer was
                // false, so we can assume it still is.
                cpu . wasXfer = false;
// The only place cycle is set to INTERRUPT_cycle in FETCH_cycle; therefore
// we can safely assume that is the state that should be restored.
                setCpuCycle (FETCH_cycle);
              }
              break;

            case INTERRUPT_EXEC_cycle:
            case INTERRUPT_EXEC2_cycle:
              {
                //     execute instruction in instruction buffer
                //     if (! transfer) set INTERUPT_EXEC2_cycle 

                if (cpu . cycle == INTERRUPT_EXEC_cycle)
                  cu . IWB = instr_buf [0];
                else
                  cu . IWB = instr_buf [1];

                if (GET_I (cu . IWB))
                  cpu . wasInhibited = true;

                t_stat ret = executeInstruction ();

                if (ret > 0)
                  {
                     reason = ret;
                     break;
                  }

                if (ret == CONT_TRA)
                  {
                     cpu . wasXfer = true; 
                     setCpuCycle (FETCH_cycle);
                     if (!clear_TEMPORARY_ABSOLUTE_mode ())
                       set_addr_mode (ABSOLUTE_mode);
                     break;
                  }

                if (cpu . cycle == INTERRUPT_EXEC_cycle)
                  {
                    setCpuCycle (INTERRUPT_EXEC2_cycle);
                    break;
                  }
                clear_TEMPORARY_ABSOLUTE_mode ();
                cu_safe_restore ();
// The only place cycle is set to INTERRUPT_cycle in FETCH_cycle; therefore
// we can safely assume that is the state that should be restored.
                // We can only get here if wasXfer was
                // false, so we can assume it still is.
                cpu . wasXfer = false;
                setCpuCycle (FETCH_cycle);
              }
              break;

            case FETCH_cycle:
              {
// "If the interrupt inhibit bit is not set in the currect instruction 
// word at the point of the next sequential instruction pair virtual
// address formation, the processor samples the [group 7 and interrupts]."

// Since we do not do concurrent instruction fetches, we must remember 
// the inhibit bits (cpu . wasInhibited).


// If the instruction pair virtual address being formed is the result of a 
// transfer of control condition or if the current instruction is 
// Execute (xec), Execute Double (xed), Repeat (rpt), Repeat Double (rpd), 
// or Repeat Link (rpl), the group 7 faults and interrupt present lines are 
// not sampled.

                if ((! cpu . wasInhibited) &&
                    (PPR . IC % 2) == 0 &&
                    (! cpu . wasXfer) &&
                    (! (cu . xde | cu . xdo | cu . rpt | cu . rd)))
                  {
                    cpu . interrupt_flag = sample_interrupts ();
                    cpu . g7_flag = bG7Pending ();
                  }

                // The cpu . wasInhibited accumulates across the even and 
                // odd intruction. If the IC is even, reset it for
                // the next pair.

                if ((PPR . IC % 2) == 0)
                  cpu . wasInhibited = false;

                if (cpu . interrupt_flag)
                  {
// This is the only place cycle is set to INTERRUPT_cycle; therefore
// return from interrupt can safely assume the it should set the cycle
// to FETCH_cycle.
                    setCpuCycle (INTERRUPT_cycle);
                    break;
                  }
                if (cpu . g7_flag)
                  {
                    cpu . g7_flag = false;
                    doG7Fault ();
                  }
#if 0
                if (cpu . interrupt_flag && 
                    ((PPR . IC % 2) == 0) &&
                    (! (cu . xde | cu . xdo | cu . rpt | cu . rd)))
                  {
// This is the only place cycle is set to INTERRUPT_cycle; therefore
// return from interrupt can safely assume the it should set the cycle
// to FETCH_cycle.
                    setCpuCycle (INTERRUPT_cycle);
                    break;
                  }
                if (cpu . g7_flag)
                  {
                    cpu . g7_flag = false;
                    //setCpuCycle (FAULT_cycle);
                    doG7Fault ();
                  }
#endif

                // If we have done the even of an XED, do the odd
                if (cu . xde == 0 && cu . xdo == 1)
                  {
                    // Get the odd
                    cu . IWB = cu . IRODD;
                    cu . xde = cu . xdo = 0; // and done
                  }
                // If we have done neither of the XED
                else if (cu . xde == 1 && cu . xdo == 1)
                  {
                    cu . xde = 0; // do the odd next time
                    cu . xdo = 1;
                  }
                // If were nave not yet done the XEC
                else if (cu . xde == 1)
                  {
                    cu . xde = cu . xdo = 0; // and done
                  }
                else
                  {
                    processorCycle = INSTRUCTION_FETCH;
                    // fetch next instruction into current instruction struct
                    clr_went_appending (); // XXX not sure this is the right place
                    fetchInstruction (PPR . IC);
                  }


#ifdef MULTIPASS
                if (multipassStatsPtr)
                  {
                    multipassStatsPtr -> PPR = PPR;
                  }
#endif
#if 0
                // XXX The conditions are more rigorous: see AL39, pg 327
           // ci is not set up yet; check the inhibit bit in the IWB!
                //if (PPR.IC % 2 == 0 && // Even address
                    //ci -> i == 0) // Not inhibited
                //if (GET_I (cu . IWB) == 0) // Not inhibited
// If the instruction pair virtual address being formed is the result of a 
// transfer of control condition or if the current instruction is 
// Execute (xec), Execute Double (xed), Repeat (rpt), Repeat Double (rpd), 
// or Repeat Link (rpl), the group 7 faults and interrupt present lines are 
// not sampled.
                if (PPR.IC % 2 == 0 && // Even address
                    GET_I (cu . IWB) == 0 &&  // Not inhibited
                    (! (cu . xde | cu . xdo | cu . rpt | cu . rd)))
                  {
                    cpu . interrupt_flag = sample_interrupts ();
                    cpu . g7_flag = bG7Pending ();
                  }
                else
                  {
                    cpu . interrupt_flag = false;
                    cpu . g7_flag = false;
                  }
#endif

                setCpuCycle (EXEC_cycle);
              }
              break;

            case EXEC_cycle:
              {
                xec_side_effect = 0;

                if (GET_I (cu . IWB))
                  cpu . wasInhibited = true;

                t_stat ret = executeInstruction ();

                if (ret > 0)
                  {
                     reason = ret;
                     break;
                  }
                if (ret == CONT_TRA)
                  {
                    cu . xde = cu . xdo = 0;
                    cpu . wasXfer = true;
                    setCpuCycle (FETCH_cycle);
                    break;   // don't bump PPR.IC, instruction already did it
                  }
                cpu . wasXfer = false;

                if (ret < 0)
                  {
                    sim_printf ("execute instruction returned %d?\n", ret);
                    break;
                  }

                if ((! cu . repeat_first) && (cu . rpt || (cu . rd & (PPR.IC & 1))))
                  {
                    if (! cu . rpt)
                      -- PPR.IC;
                    cpu . wasXfer = false; 
                    setCpuCycle (FETCH_cycle);
                    break;
                  }

#if 0
                if (cu . xde == 1 && cu . xdo == 1) // we just did the even of an XED
                  {
                    setCpuCycle (FETCH_cycle);
                    break;
                  }
                if (cu . xde) // We just did a xec or xed instruction
                  {
                    setCpuCycle (FETCH_cycle);
                    break;
                  }
#endif
                if (cu . xde || cu . xdo) // we are starting or are in an XEC/XED
                  {
                    cpu . wasXfer = false; 
                    setCpuCycle (FETCH_cycle);
                    break;
                  }

                cu . xde = cu . xdo = 0;
                PPR.IC ++;
                if (ci->info->ndes > 0)
                  PPR.IC += ci->info->ndes;
                PPR.IC += xec_side_effect;
                xec_side_effect = 0;

                cpu . wasXfer = false; 
                setCpuCycle (FETCH_cycle);
                break;

nextInstruction:;
syncFaultReturn:;
                PPR.IC ++;
                xec_side_effect = 0;
                cpu . wasXfer = false; 
                setCpuCycle (FETCH_cycle);
              }
              break;

            case FAULT_cycle:
              {
#if 0
                // Interrupts need to be processed at the beginning of the
                // FAULT CYCLE as part of the H/W 'fetch instruction pair.'

                cpu . interrupt_flag = sample_interrupts ();
                cpu . g7_flag = bG7Pending ();
#endif
                // In the FAULT CYCLE, the processor safe-stores the Control
                // Unit Data (see Section 3) into program-invisible holding
                // registers in preparation for a Store Control Unit ( scu)
                // instruction, then enters temporary absolute mode, forces the
                // current ring of execution C(PPR.PRR) to 0, and generates a
                // computed address for the fault trap pair by concatenating
                // the setting of the FAULT BASE switches on the processor
                // configuration panel with twice the fault number (see Table
                // 7-1).  This computed address and the operation code for the
                // Execute Double (xed) instruction are forced into the
                // instruction register and executed as an instruction. Note
                // that the execution of the instruction is not done in a
                // normal EXECUTE CYCLE but in the FAULT CYCLE with the
                // processor in temporary absolute mode.

                //sim_debug (DBG_FAULT, & cpu_dev, "fault cycle [%lld]\n", sim_timell ());
    
                if (switches . report_faults == 1 ||
                    (switches . report_faults == 2 &&
                     cpu . faultNumber == FAULT_OFL))
                  {
                    emCallReportFault ();
                    clearFaultCycle ();
                    cpu . wasXfer = false; 
                    setCpuCycle (FETCH_cycle);
                    PPR.IC += ci->info->ndes;
                    PPR.IC ++;
                    break;
                  }

                cu_safe_store ();

                // Temporary absolute mode
                set_TEMPORARY_ABSOLUTE_mode ();

                // Set to ring 0
                PPR . PRR = 0;
                TPR . TRR = 0;

                // (12-bits of which the top-most 7-bits are used)
                int fltAddress = (switches.FLT_BASE << 5) & 07740;

                // absolute address of fault YPair
                word24 addr = fltAddress +  2 * cpu . faultNumber;
  
#ifdef MULTIPASS
                if (multipassStatsPtr)
                  {
                    multipassStatsPtr -> faultNumber = cpu . faultNumber;
                  }
#endif
                core_read2 (addr, instr_buf, instr_buf + 1);

                setCpuCycle (FAULT_EXEC_cycle);

                break;
              }

            case FAULT_EXEC_cycle:
            case FAULT_EXEC2_cycle:
              {
                //     execute instruction in instruction buffer
                //     if (! transfer) set INTERUPT_EXEC2_cycle 

                if (cpu . cycle == FAULT_EXEC_cycle)
                  cu . IWB = instr_buf [0];
                else
                  cu . IWB = instr_buf [1];

                if (GET_I (cu . IWB))
                  cpu . wasInhibited = true;

                t_stat ret = executeInstruction ();

                if (ret > 0)
                  {
                     reason = ret;
                     break;
                  }

                if (ret == CONT_TRA)
                  {
                    cpu . wasXfer = true; 
                    setCpuCycle (FETCH_cycle);
                    clearFaultCycle ();
                    if (!clear_TEMPORARY_ABSOLUTE_mode ())
                      set_addr_mode (ABSOLUTE_mode);
                    break;
                  }
                if (cpu . cycle == FAULT_EXEC_cycle)
                  {
                    setCpuCycle (FAULT_EXEC2_cycle);
                    break;
                  }
                // Done with FAULT_EXEC2_cycle
                // Restores cpu.cycle and addressing mode
                clear_TEMPORARY_ABSOLUTE_mode ();
                cu_safe_restore ();
                cpu . wasXfer = false; 
                setCpuCycle (FETCH_cycle);
                clearFaultCycle ();

// XXX Is this needed? Are EIS instructions allowed in fault pairs?

                // cu_safe_restore should have restored CU.IWB, so
                // we can determine the instruction length.
                // decodeInstruction() restores ci->info->ndes
                decodeInstruction (cu . IWB, & currentInstruction);

                PPR.IC += ci->info->ndes;
                PPR.IC ++;
                break;
              }

            default:
              {
                sim_printf ("cpu . cycle %d?\n", cpu . cycle);
                return SCPE_UNK;
              }
          }  // switch (cpu . cycle)

      } while (reason == 0);

leave:
    sim_printf("\r\nsimCycles = %lld\n", sim_timell ());
    sim_printf("\r\ncpuCycles = %lld\n", sys_stats . total_cycles);
    for (int i = 0; i < N_FAULTS; i ++)
      {
        if (sys_stats . total_faults [i])
          sim_printf("%s faults = %lld\n", faultNames [i], sys_stats . total_faults [i]);
     }
    
    return reason;
  }



#if 0
static uint32 bkpt_type[4] = { SWMASK ('E') , SWMASK ('N'), SWMASK ('R'), SWMASK ('W') };
#endif

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
#ifndef QUIET_UNUSED
t_stat ReadNnoalign (int n, word24 addr, word36 *Yblock, enum eMemoryAccessType acctyp, int32 Tag)
{
#if 0
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
#endif
        for (int j = 0 ; j < n ; j ++)
            Read (addr + j, Yblock + j, EIS_OPERAND_READ, Tag);
    
    return SCPE_OK;
}
#endif

int OPSIZE (void)
{
    DCDstruct * i = & currentInstruction;
    if (i->info->flags & (READ_OPERAND | STORE_OPERAND))
        return 1;
    else if (i->info->flags & (READ_YPAIR | STORE_YPAIR))
        return 2;
    else if (i->info->flags & (READ_YBLOCK8 | STORE_YBLOCK8))
        return 8;
    else if (i->info->flags & (READ_YBLOCK16 | STORE_YBLOCK16))
        return 16;
    else if (i->info->flags & (READ_YBLOCK32 | STORE_YBLOCK32))
        return 32;
    return 0;
}

// read instruction operands
t_stat ReadOP (word18 addr, _processor_cycle_type cyctyp, bool b29)
{
    DCDstruct * i = & currentInstruction;
#if 0
        if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
            return STOP_BKPT;
        else
#endif
        
    // rtcd is an annoying edge case; ReadOP is called before the instruction
    // is executed, so it's setting processorCycle to RTCD_OPERAND_FETCH is
    // too late. Special case it here my noticing that this is an RTCD
    // instruction
    if (cyctyp == OPERAND_READ && i -> opcode == 0610 && ! i -> opcodeX)
    {
        addr &= 0777776;   // make even
        Read (addr + 0, Ypair + 0, RTCD_OPERAND_FETCH, b29);
        Read (addr + 1, Ypair + 1, RTCD_OPERAND_FETCH, b29);
        return SCPE_OK;
    }

    switch (OPSIZE ())
    {
        case 1:
            Read (addr, &CY, cyctyp, b29);
            return SCPE_OK;
        case 2:
            addr &= 0777776;   // make even
            Read (addr + 0, Ypair + 0, cyctyp, b29);
            Read (addr + 1, Ypair + 1, cyctyp, b29);
            break;
        case 8:
            addr &= 0777770;   // make on 8-word boundary
            for (int j = 0 ; j < 8 ; j += 1)
                Read (addr + j, Yblock8 + j, cyctyp, b29);
            break;
        case 16:
            addr &= 0777760;   // make on 16-word boundary
            for (int j = 0 ; j < 16 ; j += 1)
                Read (addr + j, Yblock16 + j, cyctyp, b29);
            
            break;
        case 32:
            addr &= 0777760;   // make on 16-word boundary // XXX don't know
            for (int j = 0 ; j < 32 ; j += 1)
                Read (addr + j, Yblock16 + j, cyctyp, b29);
            
            break;
    }
    //TPR.CA = addr;  // restore address
    
    return SCPE_OK;

}

// write instruction operands
t_stat WriteOP(word18 addr, UNUSED _processor_cycle_type cyctyp, bool b29)
{
#if 0
    if (sim_brk_summ && sim_brk_test (addr, bkpt_type[acctyp]))
        return STOP_BKPT;
    else
#endif
        
    switch (OPSIZE ())
    {
        case 1:
            Write (addr, CY, OPERAND_STORE, b29);
            return SCPE_OK;
        case 2:
            addr &= 0777776;   // make even
            Write (addr + 0, Ypair[0], OPERAND_STORE, b29);
            Write (addr + 1, Ypair[1], OPERAND_STORE, b29);
            break;
        case 8:
            addr &= 0777770;   // make on 8-word boundary
            for (int j = 0 ; j < 8 ; j += 1)
                Write (addr + j, Yblock8[j], OPERAND_STORE, b29);
            break;
        case 16:
            addr &= 0777760;   // make on 16-word boundary
            for (int j = 0 ; j < 16 ; j += 1)
                Write (addr + j, Yblock16[j], OPERAND_STORE, b29);
            break;
        case 32:
            addr &= 0777760;   // make on 16-word boundary // XXX don't know
            for (int j = 0 ; j < 32 ; j += 1)
                Write (addr + j, Yblock32[j], OPERAND_STORE, b29);
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
    if(addr >= MEMSIZE)
      doFault (op_not_complete_fault, nem, "core_read nem");
    if (M[addr] & MEM_UNINITIALIZED)
      {
        sim_debug (DBG_WARN, & cpu_dev, "Unitialized memory accessed at address %08o; IC is 0%06o:0%06o\n", addr, PPR.PSR, PPR.IC);
      }
    *data = M[addr] & DMASK;
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read  %08o %012llo\n",
                addr, * data);
    return 0;
}

int core_write(word24 addr, word36 data) {
    if(addr >= MEMSIZE)
      doFault (op_not_complete_fault, nem, "core_write nem");
    M[addr] = data & DMASK;
    sim_debug (DBG_CORE, & cpu_dev,
               "core_write %08o %012llo\n",
                addr, data);
    return 0;
}

int core_read2(word24 addr, word36 *even, word36 *odd) {
    if(addr >= MEMSIZE)
      doFault (op_not_complete_fault, nem, "core_read2 nem");
    if(addr & 1) {
        sim_debug(DBG_MSG, &cpu_dev,"warning: subtracting 1 from pair at %o in core_read2\n", addr);
        addr &= ~1; /* make it an even address */
    }
    if (M[addr] & MEM_UNINITIALIZED)
    {
        sim_debug (DBG_WARN, & cpu_dev, "Unitialized memory accessed at address %08o; IC is 0%06o:0%06o\n", addr, PPR.PSR, PPR.IC);
    }
    *even = M[addr++] & DMASK;
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012llo\n",
                addr - 1, * even);
    if (M[addr] & MEM_UNINITIALIZED)
    {
        sim_debug (DBG_WARN, & cpu_dev, "Unitialized memory accessed at address %08o; IC is 0%06o:0%06o\n", addr, PPR.PSR, PPR.IC);
    }
    *odd = M[addr] & DMASK;
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012llo\n",
                addr, * odd);
    return 0;
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
    if(addr >= MEMSIZE)
      doFault (op_not_complete_fault, nem, "core_write2 nem");
    if(addr & 1) {
        sim_debug(DBG_MSG, &cpu_dev, "warning: subtracting 1 from pair at %o in core_write2\n", addr);
        addr &= ~1; /* make it even a dress, or iron a skirt ;) */
    }
    M[addr++] = even;
    M[addr] = odd;
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


#ifndef QUIET_UNUSED
//=============================================================================

/*
 * encode_instr()
 *
 * Convert an instr_t struct into a  36-bit word.
 *
 */

void encode_instr(const instr_t *ip, word36 *wordp)
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
#endif


#endif // MM
    

/*
 * instruction fetcher ...
 * fetch + decode instruction at 18-bit address 'addr'
 */

/*
 * instruction decoder .....
 *
 * if dst is not NULL place results into dst, if dst is NULL plae results into global currentInstruction
 */
void decodeInstruction (word36 inst, DCDstruct * p)
{
    p->opcode  = GET_OP(inst);  // get opcode
    p->opcodeX = GET_OPX(inst); // opcode extension
    p->address = GET_ADDR(inst);// address field from instruction
    p->a       = GET_A(inst);   // "A" - the indirect via pointer register flag
    p->i       = GET_I(inst);   // "I" - inhibit interrupt flag
    p->tag     = GET_TAG(inst); // instruction tag
    
    p->info = getIWBInfo(p);     // get info for IWB instruction
    
    // HWR 18 June 2013 
    //p->info->opcode = p->opcode;
    //p->IWB = inst;
    
    // HWR 21 Dec 2013
    if (p->info->flags & IGN_B29)
        p->a = 0;   // make certain 'a' bit is valid always

    if (p->info->ndes > 0)
    {
        p->a = 0;
        p->tag = 0;
        if (p->info->ndes > 1)
        {
            memset(&p->e, 0, sizeof(EISstruct)); // clear out e
            p->e.op0 = inst;
        }
    }
#ifdef MULTIPASS
    if (multipassStatsPtr)
      multipassStatsPtr -> inst = inst;
#endif
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

// Append cycle should always set this up
#if 0
            if (SDW->P && PPR.PRR == 0)
            {
                PPR.P = 1;
                return 1;
            }
#endif
            // [CAC] This generates a lot of traffic because is_priv_mode
            // is frequntly called to get the state, and not just to trap
            // priviledge violations.
            //sim_debug (DBG_FAULT, & cpu_dev, "is_priv_mode: not privledged; SDW->P: %d; PPR.PRR: %d\n", SDW->P, PPR.PRR);
            break;
        default:
            break;
    }
    
    return 0;
}


static bool secret_addressing_mode;
// XXX loss of data on page fault: ticket #5
static bool went_appending; // we will go....

void set_went_appending (void)
  {
    went_appending = true;
  }

void clr_went_appending (void)
  {
    went_appending = false;
  }

bool get_went_appending (void)
  {
    return went_appending;
  }

/*
 * addr_modes_t get_addr_mode()
 *
 * Report what mode the CPU is in.
 * This is determined by examining a couple of IR flags.
 *
 * TODO: get_addr_mode() probably belongs in the CPU source file.
 *
 */

static void set_TEMPORARY_ABSOLUTE_mode (void)
{
    secret_addressing_mode = true;
    went_appending = false;
}

static bool clear_TEMPORARY_ABSOLUTE_mode (void)
{
    secret_addressing_mode = false;
    return went_appending;
}

addr_modes_t get_addr_mode(void)
{
    //if (secret_addressing_mode)
      //{ sim_debug (DBG_TRACE, & cpu_dev, "SECRET\n"); }
    if (secret_addressing_mode)
        return ABSOLUTE_mode; // This is not the mode you are looking for

    //if (went_appending)
      //{ sim_debug (DBG_TRACE, & cpu_dev, "WENT\n"); }
    if (went_appending)
        return APPEND_mode;
    //if (IR.abs_mode)
    if (TSTF(cu.IR, I_ABS))
        return ABSOLUTE_mode;
    
    //if (IR.not_bar_mode == 0)
    if (! TSTF(cu.IR, I_NBAR))
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
    went_appending = false;
// Temporary hack to fix fault/intr pair address mode state tracking
//   1. secret_addressing_mode is only set in fault/intr pair processing.
//   2. Assume that the only set_addr_mode that will occur is the b29 special
//   case or ITx.
    //if (secret_addressing_mode && mode == APPEND_mode)
      //set_went_appending ();

    secret_addressing_mode = false;
    if (mode == ABSOLUTE_mode) {
        SETF(cu.IR, I_ABS);
        SETF(cu.IR, I_NBAR);
        
        PPR.P = 1;
        
        sim_debug (DBG_DEBUG, & cpu_dev, "APU: Setting absolute mode.\n");
    } else if (mode == APPEND_mode) {
        if (! TSTF (cu.IR, I_ABS) && TSTF (cu.IR, I_NBAR))
          sim_debug (DBG_DEBUG, & cpu_dev, "APU: Keeping append mode.\n");
        else
           sim_debug (DBG_DEBUG, & cpu_dev, "APU: Setting append mode.\n");
        CLRF(cu.IR, I_ABS);
        
        SETF(cu.IR, I_NBAR);
    } else if (mode == BAR_mode) {
        CLRF(cu.IR, I_ABS);
        CLRF(cu.IR, I_NBAR);
        
        sim_debug (DBG_WARN, & cpu_dev, "APU: Setting bar mode.\n");
    } else {
        sim_debug (DBG_ERR, & cpu_dev, "APU: Unable to determine address mode.\n");
        cancel_run(STOP_BUG);
    }
}


//=============================================================================

/*
 ic_hist - Circular queue of instruction history
 Used for display via cpu_show_history()
 */

static int ic_hist_max = 0;
static int ic_hist_ptr;
static int ic_hist_wrapped;
enum hist_enum { h_instruction, h_fault, h_intr };
struct ic_hist_t {
    addr_modes_t addr_mode;
    uint seg;
    uint ic;
    //enum hist_enum { instruction, fault, intr } htype;
    enum hist_enum htype;
    union {
        //int intr;
        int fault;
        //instr_t instr;
    } detail;
};

typedef struct ic_hist_t ic_hist_t;

static ic_hist_t *ic_hist;

static void ic_history_init(void)
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
        bool inuse;
        int scu_unit_num; // 
        DEVICE * devp;
      } ports [N_CPU_PORTS];

  } cpu_array [N_CPU_UNITS_MAX];

int query_scu_unit_num (int cpu_unit_num, int cpu_port_num)
  {
    if (cpu_array [cpu_unit_num] . ports [cpu_port_num] . inuse)
      return cpu_array [cpu_unit_num] . ports [cpu_port_num] . scu_unit_num;
    return -1;
  }

// XXX when multiple cpus are supported, merge this into cpu_reset

static void cpu_reset_array (void)
  {
    for (int i = 0; i < N_CPU_UNITS_MAX; i ++)
      for (int p = 0; p < N_CPU_UNITS; p ++)
        cpu_array [i] . ports [p] . inuse = false;
  }

// A scu is trying to attach a cable to us
//  to my port cpu_unit_num, cpu_port_num
//  from it's port scu_unit_num, scu_port_num
//

t_stat cable_to_cpu (int cpu_unit_num, int cpu_port_num, int scu_unit_num, 
                     UNUSED int scu_port_num)
  {
    if (cpu_unit_num < 0 || cpu_unit_num >= (int) cpu_dev . numunits)
      {
        sim_printf ("cable_to_cpu: cpu_unit_num out of range <%d>\n", cpu_unit_num);
        return SCPE_ARG;
      }

    if (cpu_port_num < 0 || cpu_port_num >= N_CPU_PORTS)
      {
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
     
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . inuse = true;
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . scu_unit_num = scu_unit_num;
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . devp = devp;

    setup_scpage_map ();

    return SCPE_OK;
  }

static t_stat cpu_show_config (UNUSED FILE * st, UNIT * uptr, 
                               UNUSED int val, UNUSED void * desc)
{
    int unit_num = UNIT_NUM (uptr);
    if (unit_num < 0 || unit_num >= (int) cpu_dev . numunits)
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
    sim_printf("Bullet time:              %01o(8)\n", switches . bullet_time);
    sim_printf("Disable kbd bkpt:         %01o(8)\n", switches . disable_kbd_bkpt);
    sim_printf("Report faults:            %01o(8)\n", switches . report_faults);
    sim_printf("TRO faults enabled:       %01o(8)\n", switches . tro_enable);
    sim_printf("Y2K enabled:              %01o(8)\n", switches . y2k);
    sim_printf("drl fatal enabled:        %01o(8)\n", switches . drl_fatal);
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
//           disable_wam = n
//           bullet_time = n
//           disable_kbd_bkpt = n
//           report_faults = n
//               n = 0 don't
//               n = 1 report
//               n = 2 report overflow
//           tro_enable = n
//           y2k
//           drl_fatal

static config_value_list_t cfg_multics_fault_base [] =
  {
    { "multics", 2 },
    { NULL, 0 }
  };

static config_value_list_t cfg_on_off [] =
  {
    { "off", 0 },
    { "on", 1 },
    { "disable", 0 },
    { "enable", 1 },
    { NULL, 0 }
  };

static config_value_list_t cfg_cpu_mode [] =
  {
    { "gcos", 0 },
    { "multics", 1 },
    { NULL, 0 }
  };

static config_value_list_t cfg_port_letter [] =
  {
    { "a", 0 },
    { "b", 1 },
    { "c", 2 },
    { "d", 3 },
    { NULL, 0 }
  };

static config_value_list_t cfg_interlace [] =
  {
    { "off", 0 },
    { "2", 2 },
    { "4", 4 },
    { NULL, 0 }
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
    { NULL, 0 }
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
    /* 23 */ { "bullet_time", 0, 1, cfg_on_off },
    /* 24 */ { "disable_kbd_bkpt", 0, 1, cfg_on_off },
    /* 25 */ { "report_faults", 0, 2, NULL },
    /* 26 */ { "tro_enable", 0, 1, cfg_on_off },
    /* 27 */ { "y2k", 0, 1, cfg_on_off },
    /* 28 */ { "drl_fatal", 0, 1, cfg_on_off },
    { NULL, 0, 0, NULL }
  };

static t_stat cpu_set_config (UNIT * uptr, UNUSED int32 value, char * cptr, 
                              UNUSED void * desc)
  {
// XXX Minor bug; this code doesn't check for trailing garbage

    int cpu_unit_num = UNIT_NUM (uptr);
    if (cpu_unit_num < 0 || cpu_unit_num >= (int) cpu_dev . numunits)
      {
        sim_debug (DBG_ERR, & cpu_dev, "cpu_set_config: Invalid unit number %d\n", cpu_unit_num);
        sim_printf ("error: cpu_set_config: invalid unit number %d\n", cpu_unit_num);
        return SCPE_ARG;
      }

    static int port_num = 0;

    config_state_t cfg_state = { NULL, NULL };

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

            case 23: // BULLET_TIME
              switches . bullet_time = v;
              break;

            case 24: // DISABLE_KBD_BKPT
              switches . disable_kbd_bkpt = v;
              break;

            case 25: // REPORT_FAULTS
              switches . report_faults = v;
              break;

            case 26: // TRO_ENABLE
              switches . tro_enable = v;
              break;

            case 27: // Y2K
              switches . y2k = v;
              break;

            case 28: // DRL_FATAL
              switches . drl_fatal = v;
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

static int words2its (word36 word1, word36 word2, struct _par * prp)
  {
    if ((word1 & MASKBITS(6)) != 043)
      {
        return 1;
      }
    prp->SNR = getbits36(word1, 3, 15);
    prp->WORDNO = getbits36(word2, 0, 18);
    prp->RNR = getbits36(word2, 18, 3);  // not strictly correct; normally merged with other ring regs
    prp->BITNO = getbits36(word2, 57 - 36, 6);
    return 0;
  }   

static int stack_to_entry (unsigned abs_addr, struct _par * prp)
  {
    // Looks into the stack frame maintained by Multics and
    // returns the "current" entry point address that's
    // recorded in the stack frame.  Abs_addr should be a 24-bit
    // absolute memory location.
    return words2its (M [abs_addr + 026], M [abs_addr + 027], prp);
  }


static void print_frame (
    int seg,        // Segment portion of frame pointer address.
    int offset,     // Offset portion of frame pointer address.
    int addr)       // 24-bit address corresponding to above seg|offset
  {
    // Print a single stack frame for walk_stack()
    // Frame pointers can be found in PR[6] or by walking a process's stack segment

    struct _par  entry_pr;
    sim_printf ("stack trace: ");
    if (stack_to_entry (addr, & entry_pr) == 0)
      {
         sim_printf ("\t<TODO> entry %o|%o  ", entry_pr.SNR, entry_pr.WORDNO);

//---        const seginfo& seg = segments(entry_pr.PR.snr);
//---        map<int,linkage_info>::const_iterator li_it = seg.find_entry(entry_pr.wordno);
//---        if (li_it != seg.linkage.end()) {
//---            const linkage_info& li = (*li_it).second;
//---            sim_printf ("\t%s  ", li.name.c_str());
//---        } else
//---            sim_printf ("\tUnknown entry %o|%o  ", entry_pr.PR.snr, entry_pr.wordno);
      }
    else
      sim_printf ("\tUnknowable entry {%llo,%llo}  ", M [addr + 026], M [addr + 027]);
    sim_printf ("(stack frame at %03o|%06o)\n", seg, offset);

#if 0
    char buf[80];
    out_msg("prev_sp: %s; ",    its2text(buf, addr+020));
    out_msg("next_sp: %s; ",    its2text(buf, addr+022));
    out_msg("return_ptr: %s; ", its2text(buf, addr+024));
    out_msg("entry_ptr: %s\n",  its2text(buf, addr+026));
#endif
}

static int walk_stack (int output, UNUSED void * frame_listp /* list<seg_addr_t>* frame_listp */)
    // Trace through the Multics stack frames
    // See stack_header.incl.pl1 and http://www.multicians.org/exec-env.html
{

    if (PAR [6].SNR == 077777 || (PAR [6].SNR == 0 && PAR [6].WORDNO == 0)) {
        sim_printf ("%s: Null PR[6]\n", __func__);
        return 1;
    }

    // PR6 should point to the current stack frame.  That stack frame
    // should be within the stack segment.
    int seg = PAR [6].SNR;

    uint curr_frame;
    char * msg;
    //t_stat rc = computeAbsAddrN (& curr_frame, seg, PAR [6].WORDNO);
    int rc = dbgLookupAddress (seg, PAR [6].WORDNO, & curr_frame, & msg);
    if (rc)
      {
        sim_printf ("%s: Cannot convert PR[6] == %#o|%#o to absolute memory address because %s.\n",
            __func__, PAR [6].SNR, PAR [6].WORDNO, msg);
        return 1;
      }

    // The stack header will be at offset 0 within the stack segment.
    int offset = 0;
    word24 hdr_addr;  // 24bit main memory address
    //if (computeAbsAddrN (& hdr_addr, seg, offset))
    if (dbgLookupAddress (seg, offset, & hdr_addr, & msg))
      {
        sim_printf ("%s: Cannot convert %03o|0 to absolute memory address becuase %s.\n", __func__, seg, msg);
        return 1;
      }

    struct _par stack_begin_pr;
    if (words2its (M [hdr_addr + 022], M [hdr_addr + 023], & stack_begin_pr))
      {
        sim_printf ("%s: Stack header seems invalid; no stack_begin_ptr at %03o|22\n", __func__, seg);
        if (output)
            sim_printf ("%s: Stack Trace: Stack header seems invalid; no stack_begin_ptr at %03o|22\n", __func__, seg);
        return 1;
      }

    struct _par stack_end_pr;
    if (words2its (M [hdr_addr + 024], M [hdr_addr + 025], & stack_end_pr))
      {
        //if (output)
          sim_printf ("%s: Stack Trace: Stack header seems invalid; no stack_end_ptr at %03o|24\n", __func__, seg);
        return 1;
      }

    if (stack_begin_pr . SNR != seg || stack_end_pr . SNR != seg)
      {
        //if (output)
            sim_printf ("%s Stack Trace: Stack header seems invalid; stack frames are in another segment.\n", __func__);
        return 1;
      }

    struct _par lot_pr;
    if (words2its (M [hdr_addr + 026], M [hdr_addr + 027], & lot_pr))
      {
        //if (output)
          sim_printf ("%s: Stack Trace: Stack header seems invalid; no LOT ptr at %03o|26\n", __func__, seg);
        return 1;
      }
    // TODO: sanity check LOT ptr

    if (output)
      sim_printf ("%s: Stack Trace via back-links in current stack frame:\n", __func__);
    uint framep = stack_begin_pr.WORDNO;
    uint prev = 0;
    int finished = 0;
#if 0
    int need_hist_msg = 0;
#endif
    // while(framep <= stack_end_pr.WORDNO)
    for (;;)
      {
        // Might find ourselves in a different page while moving from frame to frame...
        // BUG: We assume a stack frame doesn't cross page boundries
        uint addr;
        //if (computeAbsAddrN (& addr, seg, framep))
        if (dbgLookupAddress (seg, offset, & addr, & msg))
          {
            if (finished)
              break;
            //if (output)
              sim_printf ("%s: STACK Trace: Cannot convert address of frame %03o|%06o to absolute memory address because %s.\n", __func__, seg, framep, msg);
            return 1;
          }

        // Sanity check
        if (prev != 0)
          {
            struct _par prev_pr;
            if (words2its (M [addr + 020], M [addr + 021], & prev_pr) == 0)
              {
                if (prev_pr . WORDNO != prev)
                  {
                    if (output)
                      sim_printf ("%s: STACK Trace: Stack frame's prior ptr, %03o|%o is bad.\n", __func__, seg, prev_pr . WORDNO);
                  }
              }
          }
        prev = framep;
        // Print the current frame
        if (finished && M [addr + 022] == 0 && M [addr + 024] == 0 && M [addr + 026] == 0)
          break;
#if 0
        if (need_hist_msg) {
            need_hist_msg = 0;
            out_msg("stack trace: ");
            out_msg("Recently popped frames (aka where we recently returned from):\n");
        }
#endif
        if (output)
          print_frame (seg, framep, addr);
//---        if (frame_listp)
//---            (*frame_listp).push_back(seg_addr_t(seg, framep));

        // Get the next one
        struct _par next;
        if (words2its (M [addr + 022], M [addr + 023], & next))
          {
            if (! finished)
              if (output)
                sim_printf ("STACK Trace: no next frame.\n");
            break;
          }
        if (next . SNR != seg)
          {
            if (output)
              sim_printf ("STACK Trace: next frame is in a different segment (next is in %03o not %03o.\n", next.SNR, seg);
            break;
          }
        if (next . WORDNO == stack_end_pr . WORDNO)
          {
            finished = 1;
            break;
#if 0
            need_hist_msg = 1;
            if (framep != PAR [6].WORDNO)
                out_msg("Stack Trace: Stack may be garbled...\n");
            // BUG: Infinite loop if enabled and garbled stack with "Unknowable entry {0,0}", "Unknown entry 15|0  (stack frame at 062|000000)", etc
#endif
          }
        if (next . WORDNO < stack_begin_pr . WORDNO || next . WORDNO > stack_end_pr . WORDNO)
          {
            if (! finished)
              //if (output)
                sim_printf ("STACK Trace: DEBUG: next frame at %#o is outside the expected range of %#o .. %#o for stack frames.\n", next.WORDNO, stack_begin_pr.WORDNO, stack_end_pr.WORDNO);
            if (! output)
              return 1;
          }

        // Use the return ptr in the current frame to print the source line.
        if (! finished && output)
          {
            struct _par return_pr;
            if (words2its (M [addr + 024], M [addr + 025], & return_pr) == 0)
              {
//---                 where_t where;
                int offset = return_pr . WORDNO;
                if (offset > 0)
                    -- offset;      // call was from an instr prior to the return point
                char * compname;
                word18 compoffset;
                char * where = lookupAddress (return_pr . SNR, offset, & compname, & compoffset);
                if (where)
                  {
                    sim_printf ("%s\n", where);
                    listSource (compname, compoffset, 0);
                  }

//---                 if (seginfo_find_all(return_pr.SNR, offset, &where) == 0) {
//---                     out_msg("stack trace: ");
//---                     if (where.line_no >= 0) {
//---                         // Note that if we have a source line, we also expect to have a "proc" entry and file name
//---                         out_msg("\t\tNear %03o|%06o in %s\n",
//---                             return_pr.SNR, return_pr.WORDNO, where.entry);
//---                         // out_msg("\t\tSource:  %s, line %5d:\n", where.file_name, where.line_no);
//---                         out_msg("stack trace: ");
//---                         out_msg("\t\tLine %d of %s:\n", where.line_no, where.file_name);
//---                         out_msg("stack trace: ");
//---                         out_msg("\t\tSource:  %s\n", where.line);
//---                     } else
//---                         if (where.entry_offset < 0)
//---                             out_msg("\t\tNear %03o|%06o", return_pr.SNR, return_pr.WORDNO);
//---                         else {
//---                             int off = return_pr.WORDNO - where.entry_offset;
//---                             char sign = (off < 0) ? '-' : '+';
//---                             if (sign == '-')
//---                                 off = - off;
//---                             out_msg("\t\tNear %03o|%06o %s %c%#o\n", return_pr.SNR, return_pr.WORDNO, where.entry, sign, off);
//---                         }
//---                 }
              }
          }
        // Advance
        framep = next . WORDNO;
    }

//---    if (output)
//---      {
//---        out_msg("stack trace: ");
//---        out_msg("Current Location:\n");
//---        out_msg("stack trace: ");
//---        print_src_loc("\t", get_addr_mode(), PPR.PSR, PPR.IC, &cu.IR);
//---
//---        log_any_io(0);      // Output of source/location info doesn't count towards requiring re-display of source
//---    }

    return 0;
}

static int cmd_stack_trace (UNUSED int32 arg, UNUSED char * buf)
  {
    walk_stack (1, NULL);
    sim_printf ("\n");
//---     trace_all_stack_hist ();
//---     sim_printf ("\n");
//---     dump_autos ();
//---     sim_printf ("\n");

    //float secs = (float) sys_stats.total_msec / 1000;
    //out_msg("Stats: %.1f seconds: %lld cycles at %.0f cycles/sec, %lld instructions at %.0f instr/sec\n",
        //secs, sys_stats.total_cycles, sys_stats.total_cycles/secs, sys_stats.total_instr, sys_stats.total_instr/secs);

    return 0;
  }


static int cpu_show_stack (UNUSED FILE * st, UNUSED UNIT * uptr, 
                           UNUSED int val, UNUSED void * desc)
  {
    // FIXME: use FILE *st
    return cmd_stack_trace(0, NULL);
  }


