/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony
 Copyright 2017 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

 /** * \file dps8_cpu.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include <unistd.h>

#include "dps8.h"
#include "dps8_addrmods.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_ins.h"
#include "dps8_loader.h"
#include "dps8_math.h"
#include "dps8_utils.h"
#include "dps8_iefp.h"
#include "dps8_console.h"
#include "dps8_fnp2.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_crdrdr.h"
#include "dps8_absi.h"
#ifdef M_SHARED
#include "shm.h"
#endif
#include "dps8_opcodetable.h"

#include "sim_defs.h"

// XXX Use this when we assume there is only a single cpu unit
#define ASSUME0 0

static void cpu_init_array (void);
static bool clear_TEMPORARY_ABSOLUTE_mode (void);
static void set_TEMPORARY_ABSOLUTE_mode (void);
static void setCpuCycle (cycles_t cycle);

// CPU data structures

// The DPS8M had only 4 ports

static UNIT cpu_unit [N_CPU_UNITS_MAX] =
  {
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL,
      NULL, NULL },
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL, 
     NULL, NULL },
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL, 
     NULL, NULL },
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL, 
     NULL, NULL },
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL, 
     NULL, NULL },
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL, 
     NULL, NULL },
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL, 
     NULL, NULL },
    { UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 0, 0, 0, 0, 0, NULL, NULL, 
     NULL, NULL }
  };
#define UNIT_NUM(uptr) ((uptr) - cpu_unit)

static t_stat cpu_show_config (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat cpu_set_config (UNIT * uptr, int32 value, const char * cptr,
                              void * desc);
static t_stat simhCpuResetAndClearUnit (UNIT * uptr, int32 value,
                                         const char * cptr, void * desc);
static t_stat simhCpuResetUnit (UNIT * uptr, UNUSED int32 value,
                                UNUSED const char * cptr, UNUSED void * desc);
#ifndef SCUMEM
#endif
static t_stat cpu_show_nunits (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat cpu_set_nunits (UNIT * uptr, int32 value, const char * cptr,
                               void * desc);

#ifndef NO_EV_POLL
static uv_loop_t * ev_poll_loop;
static uv_timer_t ev_poll_handle;
#endif

static MTAB cpu_mod[] =
  {
    {
      MTAB_unit_value,           // mask
      0,                         // match
      "CONFIG",                  // print string
      "CONFIG",                  // match string
      cpu_set_config,            // validation routine
      cpu_show_config,           // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },


// RESET  -- reset CPU
// INITIALIZE -- reset CPU

    {
      MTAB_unit_value,           // mask
      0,                         // match
      "RESET",                   // print string
      "RESET",                   // match string
      simhCpuResetUnit,          // validation routine
      NULL,                      // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },

    {
      MTAB_unit_value,           // mask
      0,                         // match
      "INITIALIZE",              // print string
      "INITIALIZE",              // match string
      simhCpuResetUnit,          // validation routine
      NULL,                      // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },

// INITAILIZEANDCLEAR -- reset CPU, clear Memory
// IAC -- reset CPU, clear Memory

    {
      MTAB_unit_value,           // mask
      0,                         // match
      "INITIALIZEANDCLEAR",      // print string
      "INITIALIZEANDCLEAR",      // match string
      simhCpuResetAndClearUnit,  // validation routine
      NULL,                      // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },

    {
      MTAB_unit_value,           // mask
      0,                         // match 
      "IAC",                     // print string
      "IAC",                     // match string
      simhCpuResetAndClearUnit,  // validation routine
      NULL,                      // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },

    {
      MTAB_dev_value,            // mask
      0,                         // match
      "NUNITS",                  // print string
      "NUNITS",                  // match string
      cpu_set_nunits,            // validation routine
      cpu_show_nunits,           // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },

    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };

static DEBTAB cpu_dt[] = 
  {
    { "TRACE",      DBG_TRACE, NULL       },
    { "TRACEEXT",   DBG_TRACEEXT, NULL    },
    { "MESSAGES",   DBG_MSG, NULL         },

    { "REGDUMPAQI", DBG_REGDUMPAQI, NULL  },
    { "REGDUMPIDX", DBG_REGDUMPIDX, NULL  },
    { "REGDUMPPR",  DBG_REGDUMPPR, NULL   },
    { "REGDUMPPPR", DBG_REGDUMPPPR, NULL  },
    { "REGDUMPDSBR",DBG_REGDUMPDSBR, NULL },
    { "REGDUMPFLT", DBG_REGDUMPFLT, NULL  },
    // don't move as it messes up DBG message
    { "REGDUMP",    DBG_REGDUMP, NULL     },

    { "ADDRMOD",    DBG_ADDRMOD, NULL     },
    { "APPENDING",  DBG_APPENDING, NULL   },

    { "NOTIFY",     DBG_NOTIFY, NULL      },
    { "INFO",       DBG_INFO, NULL        },
    { "ERR",        DBG_ERR, NULL         },
    { "WARN",       DBG_WARN, NULL        },
    { "DEBUG",      DBG_DEBUG, NULL       },
    // don't move as it messes up DBG message
    { "ALL",        DBG_ALL, NULL         },

    { "FAULT",      DBG_FAULT, NULL       },
    { "INTR",       DBG_INTR, NULL        },
    { "CORE",       DBG_CORE, NULL        },
    { "CYCLE",      DBG_CYCLE, NULL       },
    { "CAC",        DBG_CAC, NULL         },
    { "FINAL",      DBG_FINAL, NULL       },
    { "AVC",        DBG_AVC, NULL       },
    { NULL,         0, NULL               }
  };

// Assume CPU clock ~ 1MIPS. lockup time is 32 ms
#define LOCKUP_KIPS 1000
static uint64 luf_limits[] =
  {
     2000*LOCKUP_KIPS/1000,
     4000*LOCKUP_KIPS/1000,
     8000*LOCKUP_KIPS/1000,
    16000*LOCKUP_KIPS/1000,
    32000*LOCKUP_KIPS/1000
  };

// This is part of the simh interface
const char *sim_stop_messages[] =
  {
    "Unknown error",           // SCPE_OK
    "Unimplemented Opcode",    // STOP_UNIMP
    "DIS instruction",         // STOP_DIS
    "Breakpoint",              // STOP_BKPT
    "BUG",                     // STOP_BUG
    "Fault cascade",           // STOP_FLT_CASCADE
    "Halt",                    // STOP_HALT
    "Illegal Opcode",          // STOP_ILLOP
    "Simulation stop",         // STOP_STOP
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
 * init_opcodes ()
 *
 * This initializes the is_eis[] array which we use to detect whether or
 * not an instruction is an EIS instruction.
 *
 * TODO: Change the array values to show how many operand words are
 * used.  This would allow for better symbolic disassembly.
 *
 * BUG: unimplemented instructions may not be represented
 */

static bool watchBits [MEMSIZE];

static int is_eis[1024];    // hack

// XXX PPR.IC oddly incremented. ticket #6

void init_opcodes (void)
  {
    memset (is_eis, 0, sizeof (is_eis));
    
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


// Assumes unpaged DSBR

_sdw0 *fetchSDW (word15 segno)
  {
    word36 SDWeven, SDWodd;
    
    core_read2 ((cpu.DSBR.ADDR + 2u * segno) & PAMASK, & SDWeven, & SDWodd,
                 __func__);
    
    // even word
    
    _sdw0 *SDW = & cpu._s;
    memset (SDW, 0, sizeof (cpu._s));
    
    SDW->ADDR = (SDWeven >> 12) & 077777777;
    SDW->R1 = (SDWeven >> 9) & 7;
    SDW->R2 = (SDWeven >> 6) & 7;
    SDW->R3 = (SDWeven >> 3) & 7;
    SDW->DF = TSTBIT (SDWeven, 2);
    SDW->FC = SDWeven & 3;
    
    // odd word
    SDW->BOUND = (SDWodd >> 21) & 037777;
    SDW->R = TSTBIT (SDWodd, 20);
    SDW->E = TSTBIT (SDWodd, 19);
    SDW->W = TSTBIT (SDWodd, 18);
    SDW->P = TSTBIT (SDWodd, 17);
    SDW->U = TSTBIT (SDWodd, 16);
    SDW->G = TSTBIT (SDWodd, 15);
    SDW->C = TSTBIT (SDWodd, 14);
    SDW->EB = SDWodd & 037777;
    
    return SDW;
  }

static char * strDSBR (char * buf)
  {
    sprintf (buf, "DSBR: ADDR=%06o BND=%05o U=%o STACK=%04o",
             cpu.DSBR.ADDR, cpu.DSBR.BND, cpu.DSBR.U, cpu.DSBR.STACK);
    return buf;
  }


char * strSDW0 (char * buf, _sdw0 * SDW)
  {
    sprintf (buf, "ADDR=%06o R1=%o R2=%o R3=%o F=%o FC=%o BOUND=%o R=%o "
             "E=%o W=%o P=%o U=%o G=%o C=%o EB=%o",
             SDW->ADDR, SDW->R1, SDW->R2, SDW->R3, SDW->DF,
             SDW->FC, SDW->BOUND, SDW->R, SDW->E, SDW->W,
             SDW->P, SDW->U, SDW->G, SDW->C, SDW->EB);
    return buf;
  }

static t_stat cpu_boot (UNUSED int32 unit_num, UNUSED DEVICE * dptr)
  {
    // The boot button on the cpu is conneted to the boot button on the IOM
    // XXX is this true? Which IOM is it connected to?
    //return iom_boot (ASSUME0, & iom_dev);
    sim_printf ("Try 'BOOT IOMn'\n");
    return SCPE_ARG;
  }

void setup_scbank_map (void)
  {
    sim_debug (DBG_DEBUG, & cpu_dev,
               "setup_scbank_map: SCBANK %d N_SCBANKS %d MEM_SIZE_MAX %d\n",
               SCBANK, N_SCBANKS, MEM_SIZE_MAX);

    // Initalize to unmapped
    for (uint pg = 0; pg < N_SCBANKS; pg ++)
      {
        // The port number that the page of memory can be accessed through
        cpu.scbank_map [pg] = -1; 
        // The offset in M of the page of memory on the other side of the
        // port
        cpu.scbank_pg_os [pg] = -1; 
      }

    // For each port (which is connected to a SCU
    for (int port_num = 0; port_num < N_CPU_PORTS; port_num ++)
      {
        if (! cpu.switches.enable [port_num])
          continue;
        // Simplifing assumption: simh SCU unit 0 is the SCU with the
        // low 4MW of memory, etc...
        int scu_unit_idx = cables ->
          cablesFromScuToCpu[currentRunningCpuIdx].ports[port_num].scu_unit_idx;
        // Calculate the amount of memory in the SCU in words
        uint store_size = cpu.switches.store_size [port_num];
        // Map store size configuration switch (0-8) to memory size.
#ifdef DPS8M
        uint store_table [8] = 
          { 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304 };
#endif
#ifdef L68
#ifdef ISOLTS
// ISOLTS sez:
// for DPS88:
//   3. set store size switches to 2222.
// for L68:
//   3. remove the right free-edge connector on the 645pq wwb at slot ab28.
//
// During ISOLTS initialization, it requires that the memory switch be set to
// '3' for all eight ports; this corresponds to '2' for the DPS8M (131072)
// Then:
// isolts: a "lda 65536" (64k) failed to produce a store fault
//
// So it seems that the memory size is expected to be 64K, not 128K as per
// the swithes; presumably step 3 causes this. Fake it by tweaking store table:
//
        uint store_table [8] = 
          { 32768, 65536, 4194304, 65536, 524288, 1048576, 2097152, 262144 };
#else
        uint store_table [8] = 
          { 32768, 65536, 4194304, 131072, 524288, 1048576, 2097152, 262144 };
#endif
#endif
        uint sz = store_table [store_size];
        // Calculate the base address of the memory in words
        uint assignment = cpu.switches.assignment [port_num];
        uint base = assignment * sz;

        // Now convert to SCBANK (number of pages, page number)
        uint sz_pages = sz / SCBANK;
        uint scbase = base / SCBANK;

        sim_debug (DBG_DEBUG, & cpu_dev,
                   "setup_scbank_map: port:%d ss:%u as:%u sz_pages:%u ba:%u\n",
                   port_num, store_size, assignment, sz_pages, scbase);

        for (uint pg = 0; pg < sz_pages; pg ++)
          {
            uint scpg = scbase + pg;
            if (scpg < N_SCBANKS)
              {
                if (cpu.scbank_map [scpg] != -1)
                  {
                    sim_printf ("scbank overlap scpg %d (%o) old port %d "
                                "newport %d\n",
                                scpg, scpg, cpu.scbank_map [scpg], port_num);
                  }
                else
                  {
                    cpu.scbank_map [scpg] = port_num;
                    cpu.scbank_base [scpg] = base;
                    cpu.scbank_pg_os [scpg] =
                      (int) ((uint) scu_unit_idx * 4u * 1024u * 1024u +
                      scpg * SCBANK);
                  }
              }
            else
              {
                sim_printf ("scpg too big port %d scpg %d (%o), "
                            "limit %d (%o)\n",
                            port_num, scpg, scpg, N_SCBANKS, N_SCBANKS);
              }
          }
      }
    for (uint pg = 0; pg < N_SCBANKS; pg ++)
      sim_debug (DBG_DEBUG, & cpu_dev, "setup_scbank_map: %d:%d\n",
                 pg, cpu.scbank_map [pg]);
  }

#ifdef SCUMEM
int lookup_cpu_mem_map (word24 addr, word24 * offset)
  {
    uint scpg = addr / SCBANK;
    if (scpg < N_SCBANKS)
      {
        * offset = addr - cpu.scbank_base[scpg];
        return cpu.scbank_map[scpg];
      }
    return -1;
  }
#else
int lookup_cpu_mem_map (word24 addr)
  {
    uint scpg = addr / SCBANK;
    if (scpg < N_SCBANKS)
      {
        return cpu.scbank_map[scpg];
      }
    return -1;
  }
#endif
//
// serial.txt format
//
//      sn:  number[,number]
//
//  Additional numbers will be for multi-cpu systems.
//  Other fields to be added.

static void getSerialNumber (void)
  {
    bool havesn = false;
    FILE * fp = fopen ("./serial.txt", "r");
    while (fp && ! feof (fp))
      {
        char buffer [81] = "";
        fgets (buffer, sizeof (buffer), fp);
#ifdef ROUND_ROBIN
        uint cpun, sn;
        if (sscanf (buffer, "sn%u: %u", & cpun, & sn) == 2)
          {
            if (cpun < N_CPU_UNITS_MAX)
              {
                uint save = setCPUnum (cpun);
                cpu.switches.serno = sn;
                sim_printf ("Serial number of CPU %u is %u\n",
                            cpun, cpu.switches.serno);
                setCPUnum (save);
                havesn = true;
              }
          }
#else
        if (sscanf (buffer, "sn: %u", & cpu.switches.serno) == 1)
          {
            sim_printf ("Serial number is %u\n", cpu.switches.serno);
            havesn = true;
          }
#endif
      }
    if (! havesn)
      {
        sim_printf ("Please register your system at "
                    "https://ringzero.wikidot.com/wiki:register\n");
        sim_printf ("or create the file 'serial.txt' containing the line "
                    "'sn: 0'.\n");
      }
    if (fp)
      fclose (fp);
  }


#ifndef NO_EV_POLL
// The 100Hz timer as expired; poll I/O

static void ev_poll_cb (uv_timer_t * UNUSED handle)
  {
    // Call the one hertz stuff every 100 loops
    static uint oneHz = 0;
    if (oneHz ++ >= 100) // ~ 1Hz
      {
        oneHz = 0;
        rdrProcessEvent (); 
      }
    scpProcessEvent (); 
    fnpProcessEvent (); 
    consoleProcess ();
#ifndef __MINGW64__
    absiProcessEvent ();
#endif

#ifndef TR_WORK
// Update the TR

// The Timer register runs at 512Khz; in 1/100 of a second it
// decrements 5120.

// Will it pass through zero?

    if (cpu.rTR <= 5120)
      {
        if (cpu.switches.tro_enable)
        setG7fault (currentRunningCpuIdx, FAULT_TRO, fst_zero);
      }
    cpu.rTR -= 5120;
    cpu.rTR &= MASK27;
#if ISOLTS
    cpu.shadowTR = cpu.rTR;
    cpu.rTRlsb = 0;
#endif
#endif // TR_WORK
  }
#endif // NO_EV_POLL


    
// called once initialization

void cpu_init (void)
  {

// !!!! Do not use 'cpu' in this routine; usage of 'cpus' violates 'restrict'
// !!!! attribute

#ifndef SCUMEM
#ifdef M_SHARED
    if (! M)
      {
        M = (word36 *) create_shm ("M", getsid (0), MEMSIZE * sizeof (word36));
      }
#else
    if (! M)
      {
        M = (word36 *) calloc (MEMSIZE, sizeof (word36));
      }
#endif
    if (! M)
      {
        sim_printf ("create M failed\n");
        sim_err ("create M failed\n");
      }
#endif

#ifdef M_SHARED
    if (! cpus)
      {
        cpus = (cpu_state_t *) create_shm ("cpus", 
                                           getsid (0), 
                                           N_CPU_UNITS_MAX * 
                                             sizeof (cpu_state_t));
      }
    if (! cpus)
      {
        sim_printf ("create cpus failed\n");
        sim_err ("create cpus failed\n");
      }
#endif

    memset (& watchBits, 0, sizeof (watchBits));

    setCPUnum (0);

#ifdef ROUND_ROBIN
    memset (cpus, 0, sizeof (cpu_state_t) * N_CPU_UNITS_MAX);
    cpus [0].switches.FLT_BASE = 2; // Some of the UnitTests assume this
#else
    memset (& cpu, 0, sizeof (cpu));
    cpu.switches.FLT_BASE = 2; // Some of the UnitTests assume this
#endif
    cpu_init_array ();

    getSerialNumber ();

#ifndef NO_EV_POLL
    ev_poll_loop = uv_default_loop ();
    uv_timer_init (ev_poll_loop, & ev_poll_handle);
    // 10 ms == 100Hz
    uv_timer_start (& ev_poll_handle, ev_poll_cb, 10, 10);
#endif

    sim_brk_types = sim_brk_dflt = SWMASK ('E');
    // TODO: reset *all* other structures to zero
    
    memset (& sys_stats, 0, sizeof (sys_stats));
    sys_stats.total_cycles = 0;
    for (int i = 0; i < N_FAULTS; i ++)
      sys_stats.total_faults [i] = 0;
    
    
#ifdef MATRIX
    initializeTheMatrix ();
#endif
  }

// DPS8M Memory of 36 bit words is implemented as an array of 64 bit words.
// Put state information into the unused high order bits.
#define MEM_UNINITIALIZED 0x4000000000000000LLU

static void cpuResetUnitIdx (UNUSED uint cpun, bool clearMem)
  {
#ifdef ROUND_ROBIN
    uint save = setCPUnum (cpun);
#endif
    if (clearMem)
      {
#ifdef SCUMEM
        for (int cpu_port_num = 0; cpu_port_num < N_CPU_PORTS; cpu_port_num ++)
          {
            int scuUnitIdx =
                     queryScuUnitIdx ((int) currentRunningCpuIdx,
                                      (int) cpu_port_num);

            if (scuUnitIdx >= 0)
              {
                for (uint i = 0; i < SCU_MEM_SIZE; i ++)
                  scu [scuUnitIdx].M[i] = MEM_UNINITIALIZED;
              }
          }
#else
        for (uint i = 0; i < MEMSIZE; i ++)
          M [i] = MEM_UNINITIALIZED;
#endif
      }
    cpu.rA = 0;
    cpu.rQ = 0;
    
    cpu.PPR.IC = 0;
    cpu.PPR.PRR = 0;
    cpu.PPR.PSR = 0;
    cpu.PPR.P = 1;
    cpu.RSDWH_R1 = 0;
    cpu.rTR = MASK27;
#if ISOLTS
    cpu.shadowTR = 0;
    cpu.rTRlsb = 0;
#endif
#ifdef TR_WORK
    cpu.rTR = MASK27;
    cpu.rTRticks = 0;
#endif
 
    set_addr_mode (ABSOLUTE_mode);
    SET_I_NBAR;
    
    cpu.CMR.luf = 3;    // default of 16 mS
    cpu.cu.SD_ON = cpu.switches.disable_wam ? 0 : 1;
    cpu.cu.PT_ON = cpu.switches.disable_wam ? 0 : 1;
 
    setCpuCycle (FETCH_cycle);

    cpu.wasXfer = false;
    cpu.wasInhibited = false;

    cpu.interrupt_flag = false;
    cpu.g7_flag = false;

    cpu.faultRegister [0] = 0;
    cpu.faultRegister [1] = 0;

#ifdef RAPRx
    cpu.apu.lastCycle = UNKNOWN_CYCLE;
#endif

    memset (& cpu.PPR, 0, sizeof (struct _ppr));

    setup_scbank_map ();

    tidy_cu ();
#ifdef ROUND_ROBIN
    setCPUnum (save);
#endif
  }

static void cpuReset (void)
  {
    for (uint i = 0; i < N_CPU_UNITS_MAX; i ++)
      {
        cpuResetUnitIdx (i, true);
      }

#ifdef ROUND_ROBIN
    setCPUnum (0);
#endif

    sim_debug (DBG_INFO, & cpu_dev, "CPU reset: Running\n");

  }

static t_stat simhCpuReset (UNUSED DEVICE *dptr)
  {
    //memset (M, -1, MEMSIZE * sizeof (word36));

    // Fill DPS8M memory with zeros, plus a flag only visible to the emulator
    // marking the memory as uninitialized.


    cpuReset ();
    return SCPE_OK;
  }

/* Memory examine */
//  t_stat examine_routine (t_val *eval_array, t_addr addr, UNIT *uptr, int32
//  switches) 
//  Copy  sim_emax consecutive addresses for unit uptr, starting 
//  at addr, into eval_array. The switch variable has bit<n> set if the n'th 
//  letter was specified as a switch to the examine command. 
// Not true...

static t_stat cpu_ex (t_value *vptr, t_addr addr, UNUSED UNIT * uptr,
                      UNUSED int32 sw)
  {
    if (addr>= MEMSIZE)
        return SCPE_NXM;
    if (vptr != NULL)
      {
#ifdef SCUMEM
        word36 w;
        core_read (addr, & w, __func__);
        *vptr = w;
#else
        *vptr = M[addr] & DMASK;
#endif
      }
    return SCPE_OK;
  }

/* Memory deposit */

static t_stat cpu_dep (t_value val, t_addr addr, UNUSED UNIT * uptr, 
                       UNUSED int32 sw)
  {
    if (addr >= MEMSIZE) return SCPE_NXM;
#ifdef SCUMEM
    word36 w = val & DMASK;
    core_write (addr, w, __func__);
#else
    M[addr] = val & DMASK;
#endif
    return SCPE_OK;
  }



/*
 * register stuff ...
 */

#ifdef M_SHARED
// simh has to have a statically allocated IC to refer to.
static word18 dummyIC;
#endif

static REG cpu_reg[] =
  {
    // IC must be the first; see sim_PC.
#ifdef M_SHARED
    { ORDATA (IC, dummyIC, VASIZE), 0, 0, 0 }, 
#else
    { ORDATA (IC, cpus[0].PPR.IC, VASIZE), 0, 0, 0 },
#endif
    { NULL, NULL, 0, 0, 0, 0, NULL, NULL, 0, 0, 0 }
  };

/*
 * simh interface
 */

REG *sim_PC = & cpu_reg[0];

/* CPU device descriptor */

DEVICE cpu_dev =
  {
    "CPU",          // name
    cpu_unit,       // units
    cpu_reg,        // registers
    cpu_mod,        // modifiers
    N_CPU_UNITS,    // #units
    8,              // address radix
    PASIZE,         // address width
    1,              // addr increment
    8,              // data radix
    36,             // data width
    & cpu_ex,        // examine routine
    & cpu_dep,       // deposit routine
    & simhCpuReset,     // reset routine
    & cpu_boot,           // boot routine
    NULL,           // attach routine
    NULL,           // detach routine
    NULL,           // context
    DEV_DEBUG,      // device flags
    0,              // debug control flags
    cpu_dt,         // debug flag names
    NULL,           // memory size change
    NULL,           // logical name
    NULL,           // help
    NULL,           // attach help
    NULL,           // help context
    NULL,           // description
    NULL
  };

#ifdef M_SHARED
cpu_state_t * cpus = NULL;
#else
cpu_state_t cpus [N_CPU_UNITS_MAX];
#endif
cpu_state_t * restrict cpup; 

#ifdef ROUND_ROBIN
uint currentRunningCpuIdx;
#endif

// Scan the SCUs; it one has an interrupt present, return the fault pair
// address for the highest numbered interrupt on that SCU. If no interrupts
// are found, return 1.
 
static uint get_highest_intr (void)
  {
    for (uint scuUnitNum = 0; scuUnitNum < N_SCU_UNITS_MAX; scuUnitNum ++)
      {
        if (cpu.events.XIP [scuUnitNum])
          {
            uint fp = scuGetHighestIntr (scuUnitNum);
            if (fp != 1)
              return fp;
          }
      }
    return 1;
  }

bool sample_interrupts (void)
  {
    cpu.lufCounter = 0;
    for (uint scuUnitNum = 0; scuUnitNum < N_SCU_UNITS_MAX; scuUnitNum ++)
      {
        if (cpu.events.XIP [scuUnitNum])
          {
            return true;
          }
      }
    return false;
  }

t_stat simh_hooks (void)
  {
    int reason = 0;

    if (stop_cpu)
      return STOP_STOP;

#ifdef ISOLTS
    if (currentRunningCpuIdx == 0)
#endif
    // check clock queue 
    if (sim_interval <= 0)
      {
//int32 int0 = sim_interval;
        reason = sim_process_event ();
        if (reason)
          return reason;
      }
        
    sim_interval --;

// This is needed for BCE_TRAP in install scripts
    // sim_brk_test expects a 32 bit address; PPR.IC into the low 18, and
    // PPR.PSR into the high 12
    if (sim_brk_summ &&
        sim_brk_test ((cpu.PPR.IC & 0777777) |
                      ((((t_addr) cpu.PPR.PSR) & 037777) << 18),
                      SWMASK ('E')))  /* breakpoint? */
      return STOP_BKPT; /* stop simulation */
#ifndef SPEED
    if (sim_deb_break && sim_timell () >= sim_deb_break)
      return STOP_BKPT; /* stop simulation */
#endif

    return reason;
  }       

static char * cycleStr (cycles_t cycle)
  {
    switch (cycle)
      {
        //case ABORT_cycle:
          //return "ABORT_cycle";
        case FAULT_cycle:
          return "FAULT_cycle";
        case EXEC_cycle:
          return "EXEC_cycle";
        case FAULT_EXEC_cycle:
          return "FAULT_EXEC_cycle";
        case INTERRUPT_cycle:
          return "INTERRUPT_cycle";
        case INTERRUPT_EXEC_cycle:
          return "INTERRUPT_EXEC_cycle";
        case FETCH_cycle:
          return "FETCH_cycle";
        case SYNC_FAULT_RTN_cycle:
          return "SYNC_FAULT_RTN_cycle";
        default:
          return "unknown cycle";
      }
  }

static void setCpuCycle (cycles_t cycle)
  {
    sim_debug (DBG_CYCLE, & cpu_dev, "Setting cycle to %s\n",
               cycleStr (cycle));
    cpu.cycle = cycle;
  }


uint setCPUnum (UNUSED uint cpuNum)
  {
    uint prev = currentRunningCpuIdx;
#ifdef ROUND_ROBIN
    currentRunningCpuIdx = cpuNum;
#endif
    cpup = & cpus [currentRunningCpuIdx];
    return prev;
  }

uint getCPUnum (void)
  {
    return currentRunningCpuIdx;
  }

#ifdef PANEL
static void panelProcessEvent (void)
  {
    // INITIALIZE pressed; treat at as a BOOT.
    if (cpu.panelInitialize && cpu.DATA_panel_s_trig_sw == 0) 
      {
         // Wait for release
         while (cpu.panelInitialize) 
           ;
         if (cpu.DATA_panel_init_sw)
           cpuResetUnitIdx (ASSUME0, true); // INITIALIZE & CLEAR
         else
           cpuResetUnitIdx (ASSUME0, false); // INITIALIZE
         // XXX Until a boot switch is wired up
         doBoot ();
      }
    // EXECUTE pressed; EXECUTE PB set, EXECUTE FAULT set
    if (cpu.DATA_panel_s_trig_sw == 0 &&
        cpu.DATA_panel_execute_sw && // EXECUTE buttton
        cpu.DATA_panel_scope_sw && // 'EXECUTE PB/SCOPE REPEAT' set to PB
        cpu.DATA_panel_exec_sw == 0) // 'EXECUTE SWITCH/EXECUTE FAULT' 
                                     //  set to FAULT
      {
        // Wait for release
        while (cpu.DATA_panel_execute_sw) 
          ;

        if (cpu.DATA_panel_exec_sw) // EXECUTE SWITCH
          {
            cpuResetUnitIdx (ASSUME0, false);
            cpu.cu.IWB = cpu.switches.data_switches;
            setCpuCycle (EXEC_cycle);
          }
         else // EXECUTE FAULT
          {
            setG7fault (currentRunningCpuIdx, FAULT_EXF, fst_zero);
          }
      }
  }
#endif

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

#ifndef NO_EV_POLL
static uint fastQueueSubsample = 0;
#endif

// This is part of the simh interface
t_stat sim_instr (void)
  {
    t_stat reason = 0;
      
#ifdef M_SHARED
// simh needs to have the IC statically allocated, so a placeholder was
// created. Copy the placeholder in so the IC can be set by simh.

    setCPUnum (0);
    cpus [0].PPR.IC = dummyIC;
#endif

    setCPUnum (0);
#ifdef ROUND_ROBIN
    cpu.isRunning = true;
    setCPUnum (cpu_dev.numunits - 1);

setCPU:;
    uint current = currentRunningCpuIdx;
    uint c;
    for (c = 0; c < cpu_dev.numunits; c ++)
      {
        setCPUnum (c);
        if (cpu.isRunning)
          break;
      }
    if (c == cpu_dev.numunits)
      {
        sim_printf ("All CPUs stopped\n");
        goto leave;
      }
    setCPUnum ((current + 1) % cpu_dev.numunits);
    if (! cpu . isRunning)
      goto setCPU;
#endif

    // This allows long jumping to the top of the state machine
    int val = setjmp (cpu.jmpMain);

    switch (val)
      {
        case JMP_ENTRY:
        case JMP_REENTRY:
            reason = 0;
            break;
        case JMP_SYNC_FAULT_RETURN:
            setCpuCycle (SYNC_FAULT_RTN_cycle);
            break;
        case JMP_STOP:
            reason = STOP_HALT;
            goto leave;
        case JMP_REFETCH:

            // Not necessarily so, but the only times
            // this path is taken is after an RCU returning
            // from an interrupt, which could only happen if
            // was xfer was false; or in a DIS cycle, in
            // which case we want it false so interrupts 
            // can happen.
            cpu.wasXfer = false;
             
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

    DCDstruct * ci = & cpu.currentInstruction;

    do
      {
        reason = 0;

        // Process deferred events and breakpoints
        reason = simh_hooks ();
        if (reason)
          {
            break;
          }

#ifndef NO_EV_POLL
// The event poll is consuming 40% of the CPU according to pprof.
// We only want to process at 100Hz; yet we are testing at ~1MHz.
// If we only test every 1000 cycles, we shouldn't miss by more then
// 10%...

        //static uint fastQueueSubsample = 0;
        if (fastQueueSubsample ++ > 1024) // ~ 1KHz
          {
            fastQueueSubsample = 0;
            uv_run (ev_poll_loop, UV_RUN_NOWAIT);
            PNL (panelProcessEvent ());
          }
#else
        static uint slowQueueSubsample = 0;
        if (slowQueueSubsample ++ > 1024000) // ~ 1Hz
          {
            slowQueueSubsample = 0;
            rdrProcessEvent (); 
          }
        static uint queueSubsample = 0;
        if (queueSubsample ++ > 10240) // ~ 100Hz
          {
            queueSubsample = 0;
            scpProcessEvent (); 
            fnpProcessEvent (); 
            consoleProcess ();
            absiProcessEvent ();
            PNL (panelProcessEvent ());
          }
#endif

        int con_unit_idx = check_attn_key ();
        if (con_unit_idx != -1)
          console_attn_idx (con_unit_idx);

#ifndef NO_EV_POLL
#ifdef ISOLTS
        if (cpu.cycle != FETCH_cycle)
          {
            // Sync. the TR with the emulator clock.
            cpu.rTRlsb ++;
            if (cpu.rTRlsb >= 4)
              {
                cpu.rTRlsb = 0;
                cpu.shadowTR = (cpu.shadowTR - 1) & MASK27;
                if (cpu.shadowTR == 0) // passing thorugh 0...
                  {
                    if (cpu.switches.tro_enable)
                      setG7fault (currentRunningCpuIdx, FAULT_TRO, fst_zero);
                  }
              }
          }
#endif
#endif

#ifdef TR_WORK

// Check for TR underflow. The TR is stored in a uint32_t, but is 27 bits wide.
// The TR update code decrements the TR; if it passes through 0, the high bits
// will be set.

// If we assume a 1 MIPS reference platform, the TR would be decremented every
// two instructions (1/2 MHz)

#if 0
        cpu.rTR -= cpu.rTRticks * 100;
        //cpu.rTR -= cpu.rTRticks * 50;
        cpu.rTRticks = 0;
#else
#define TR_RATE 2

        cpu.rTR -= cpu.rTRticks / TR_RATE;
        cpu.rTRticks %= TR_RATE;

#endif


        if (cpu.rTR & ~MASK27)
          {
            cpu.rTR &= MASK27;
            if (cpu.switches.tro_enable)
            setG7fault (currentRunningCpuIdx, FAULT_TRO, fst_zero);
          }
#endif
        sim_debug (DBG_CYCLE, & cpu_dev, "Cycle is %s\n",
                   cycleStr (cpu.cycle));

        switch (cpu.cycle)
          {
            case INTERRUPT_cycle:
              {
                CPT (cpt1U, 0); // Interupt cycle
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
                cpu.cu.FI_ADDR = (word5) (intr_pair_addr / 2);
                cu_safe_store ();
                // XXX the whole interrupt cycle should be rewritten as an xed
                // instruction pushed to IWB and executed 

                CPT (cpt1U, 1); // safe store complete
                // Temporary absolute mode
                set_TEMPORARY_ABSOLUTE_mode ();

                // Set to ring 0
                cpu.PPR.PRR = 0;
                cpu.TPR.TRR = 0;

                // Check that an interrupt is actually pending
                if (cpu.interrupt_flag)
                  {
                    CPT (cpt1U, 2); // interrupt pending
                    // clear interrupt, load interrupt pair into instruction 
                    // buffer; set INTERRUPT_EXEC_cycle.

                    // In the h/w this is done later, but doing it now allows 
                    // us to avoid clean up for no interrupt pending.

                    if (intr_pair_addr != 1) // no interrupts 
                      {

                        CPT (cpt1U, 3); // interrupt identified
                        sim_debug (DBG_INTR, & cpu_dev, "intr_pair_addr %u\n", 
                                   intr_pair_addr);

#ifndef SPEED
                        if_sim_debug (DBG_INTR, & cpu_dev) 
                          traceInstruction (DBG_INTR);
#endif

                        // get interrupt pair
                        core_read2 (intr_pair_addr,
                                    & cpu.cu.IWB, & cpu.cu.IRODD, __func__);
                        cpu.cu.xde = 1;
                        cpu.cu.xdo = 1;


                        CPT (cpt1U, 4); // interrupt pair fetched
                        cpu.interrupt_flag = false;
                        setCpuCycle (INTERRUPT_EXEC_cycle);
                        break;
                      } // int_pair != 1
                  } // interrupt_flag

                // If we get here, there was no interrupt

                CPT (cpt1U, 5); // interrupt pair spurious
                cpu.interrupt_flag = false;
                clear_TEMPORARY_ABSOLUTE_mode ();
                // Restores addressing mode 
                cu_safe_restore ();
                // We can only get here if wasXfer was
                // false, so we can assume it still is.
                cpu.wasXfer = false;
// The only place cycle is set to INTERRUPT_cycle in FETCH_cycle; therefore
// we can safely assume that is the state that should be restored.
                setCpuCycle (FETCH_cycle);
              }
              break;

            case FETCH_cycle:
              {
#ifdef PANEL
                memset (cpu.cpt, 0, sizeof (cpu.cpt));
#endif
                CPT (cpt1U, 13); // fetch cycle

                PNL (L68_ (cpu.INS_FETCH = false;))

// "If the interrupt inhibit bit is not set in the currect instruction 
// word at the point of the next sequential instruction pair virtual
// address formation, the processor samples the [group 7 and interrupts]."

// Since XEx/RPx may overwrite IWB, we must remember 
// the inhibit bits (cpu.wasInhibited).


// If the instruction pair virtual address being formed is the result of a 
// transfer of control condition or if the current instruction is 
// Execute (xec), Execute Double (xed), Repeat (rpt), Repeat Double (rpd), 
// or Repeat Link (rpl), the group 7 faults and interrupt present lines are 
// not sampled.

// Group 7 Faults
// 
// Shutdown
//
// An external power shutdown condition has been detected. DC POWER shutdown
// will occur in approximately one millisecond.
//
// Timer Runout
//
// The timer register has decremented to or through the value zero. If the
// processor is in privileged mode or absolute mode, recognition of this fault
// is delayed until a return to normal mode or BAR mode. Counting in the timer
// register continues.  
//
// Connect
//
// A connect signal ($CON strobe) has been received from a system controller.
// This event is to be distinguished from a Connect Input/Output Channel (cioc)
// instruction encountered in the program sequence.

                // check BAR bound and raise store fault if above
                // pft 04d 10070, ISOLTS-776 06ad
                if (get_bar_mode ())
                    getBARaddress (cpu.PPR.IC);

                // Don't check timer runout if privileged
                // ISOLTS-776 04bcf, 785 02c
                // (but do if in a DIS instruction with bit28 clear)
                bool tmp_priv_mode = is_priv_mode ();
                bool noCheckTR = tmp_priv_mode && 
                                 ! (cpu.currentInstruction.opcode == 0616 &&
                                 ! cpu.currentInstruction.opcodeX &&
                                 ! GET_I (cpu.cu.IWB));
                if (! (cpu.cu.xde | cpu.cu.xdo |
                       cpu.cu.rpt | cpu.cu.rd | cpu.cu.rl))
                  {
                    if ((!cpu.wasInhibited) &&
                        (cpu.PPR.IC & 1) == 0 &&
                        (! cpu.wasXfer))
                      {
                        CPT (cpt1U, 14); // sampling interrupts
                        cpu.interrupt_flag = sample_interrupts ();
                        cpu.g7_flag =
                          noCheckTR ? bG7PendingNoTRO () : bG7Pending ();
                      }
                    cpu.wasInhibited = false;
                  }
                else 
                  {
                    // XEx at an odd location disables interrupt sampling 
                    // also for the next instruction pair. ISOLTS-785 02g, 
                    // 776 04g
                    // Set the inhibit flag
                    // (I assume RPx behaves in the same way)
                    if ((cpu.PPR.IC & 1) == 1)
                      {
                        cpu.wasInhibited = true;
                      }
                  }


// Multics executes a CPU connect instruction (which should eventually cause a
// connect fault) while interrupts are inhibited and an IOM interrupt is
// pending. Multics then executes a DIS instruction (Delay Until Interrupt
// Set). This should cause the processor to "sleep" until an interrupt is
// signaled. The DIS instruction sees that an interrupt is pending, sets
// cpu.interrupt_flag to signal that the CPU to service the interrupt and
// resumes the CPU.
//
// The CPU state machine sets up to fetch the next instruction. If checks to
// see if this instruction should check for interrupts or faults according to
// the complex rules (interrupts inhibited, even address, not RPT or XEC,
// etc.); it this case, the test fails as the next instruction is at an odd
// address. If the test had passed, the cpu.interrupt_flag would be set or
// cleared depending on the pending interrupt state data, AND the cpu.g7_flag
// would be set or cleared depending on the faults pending data (in this case,
// the connect fault).
//
// Because the flags were not updated, after the test, cpu.interrupt_flag is
// set (since the DIS instruction set it) and cpu.g7_flag is not set.
//
// Next, the CPU sees the that cpu.interrupt flag is set, and starts the
// interrupt cycle despite the fact that a higher priority g7 fault is pending.


// To fix this, check (or recheck) g7 if an interrupt is going to be faulted.
// Either DIS set interrupt_flag and FETCH_cycle didn't so g7 needs to be
// checked, or FETCH_cycle did check it when it set interrupt_flag in which
// case it is being rechecked here. It is [locally] idempotent and light
// weight, so this should be okay.

                if (cpu.interrupt_flag)
                  cpu.g7_flag = noCheckTR ? bG7PendingNoTRO () : bG7Pending ();

                if (cpu.g7_flag)
                  {
                      cpu.g7_flag = false;
                      cpu.interrupt_flag = false;
                      sim_debug (DBG_CYCLE, & cpu_dev,
                                 "call doG7Fault (%d)\n", !noCheckTR);
                      doG7Fault (!noCheckTR);
                  }
                if (cpu.interrupt_flag)
                  {
// This is the only place cycle is set to INTERRUPT_cycle; therefore
// return from interrupt can safely assume the it should set the cycle
// to FETCH_cycle.
                    CPT (cpt1U, 15); // interrupt
                    setCpuCycle (INTERRUPT_cycle);
                    break;
                  }

                cpu.lufCounter ++;
                if ((tmp_priv_mode && cpu.lufCounter > luf_limits[4]) || 
                    (! tmp_priv_mode && 
                     cpu.lufCounter > luf_limits[cpu.CMR.luf]))
                  {
                    CPT (cpt1U, 16); // LUF
                    cpu.lufCounter = 0;
                    doFault (FAULT_LUF, fst_zero, "instruction cycle lockup");
                  }

                // If we have done the even of an XED, do the odd
                if (cpu.cu.xde == 0 && cpu.cu.xdo == 1)
                  {
                    CPT (cpt1U, 17); // do XED odd
                    // Get the odd
                    cpu.cu.IWB = cpu.cu.IRODD;
                    // Do nothing next time
                    cpu.cu.xde = cpu.cu.xdo = 0;
                    cpu.isExec = true;
                    cpu.isXED = true;
                  }
                // If we have done neither of the XED
                else if (cpu.cu.xde == 1 && cpu.cu.xdo == 1)
                  {
                    CPT (cpt1U, 18); // do XED even
                    // Do the even this time and the odd the next time
                    cpu.cu.xde = 0;
                    cpu.cu.xdo = 1;
                    cpu.isExec = true;
                    cpu.isXED = true;
                  }
                // If we have not yet done the XEC
                else if (cpu.cu.xde == 1)
                  {
                    CPT (cpt1U, 19); // do XEC
                    // do it this time, and nothing next time
                    cpu.cu.xde = cpu.cu.xdo = 0;
                    cpu.isExec = true;
                    cpu.isXED = false;
                  }
                else
                  {
                    CPT (cpt1U, 20); // not XEC or RPx
                    cpu.isExec = false;
                    cpu.isXED = false;
                    // fetch next instruction into current instruction struct
#ifndef NOWENT
                    clr_went_appending (); // XXX not sure this is the right
                                           //  place
#endif
                    cpu.cu.XSF = 0; // Hmm. Is XSF == clr_went_appending ?
                    cpu.cu.TSN_VALID [0] = 0;
                    PNL (cpu.prepare_state = ps_PIA);
                    PNL (L68_ (cpu.INS_FETCH = true;))
                    fetchInstruction (cpu.PPR.IC);
                  }

                CPT (cpt1U, 21); // go to exec cycle
                advanceG7Faults ();
                setCpuCycle (EXEC_cycle);
              }
              break;

            case EXEC_cycle:
            case FAULT_EXEC_cycle:
            case INTERRUPT_EXEC_cycle:
              {
                CPT (cpt1U, 22); // exec cycle

                // The only time we are going to execute out of IRODD is
                // during RPD, at which time interrupts are automatically
                // inhibited; so the following can igore RPD harmelessly
                if (GET_I (cpu.cu.IWB))
                  cpu.wasInhibited = true;

                t_stat ret = executeInstruction ();
#ifdef TR_WORK_EXEC
               cpu.rTRticks ++;
#endif
                CPT (cpt1U, 23); // execution complete

                addCUhist ();

                if (ret > 0)
                  {
                     reason = ret;
                     break;
                  }

                if (ret == CONT_XEC)
                  {
                    CPT (cpt1U, 27); // XEx instruction
                    cpu.wasXfer = false; 
                    if (cpu.cycle == EXEC_cycle)
                      setCpuCycle (FETCH_cycle);
                    break;
                  }

                if (ret == CONT_TRA)
                  {
                    CPT (cpt1U, 24); // transfer instruction
                    cpu.cu.xde = cpu.cu.xdo = 0;
                    cpu.isExec = false;
                    cpu.isXED = false;
                    cpu.wasXfer = true;

                    if (cpu.cycle != EXEC_cycle) // fault or interrupt
                      {

                        clearFaultCycle ();

// BAR mode:  [NBAR] is set ON (taking the processor
// out of BAR mode) by the execution of any transfer instruction
// other than tss during a fault or interrupt trap.

                        if (! (cpu.currentInstruction.opcode == 0715 &&
                           cpu.currentInstruction.opcodeX == 0))
                          {
                            CPT (cpt1U, 9); // nbar set
                            SET_I_NBAR;
                          }

                        if (!clear_TEMPORARY_ABSOLUTE_mode ())
                          {
                            // didn't go appending
                            sim_debug (DBG_TRACE, & cpu_dev,
                                       "setting ABS mode\n");
                            CPT (cpt1U, 10); // temporary absolute mode
                            set_addr_mode (ABSOLUTE_mode);
                          }
                        else
                          {
                            // went appending
                            sim_debug (DBG_TRACE, & cpu_dev,
                                       "not setting ABS mode\n");
                          }

                      } // fault or interrupt


                    if (TST_I_ABS && get_went_appending ())
                      {
                        set_addr_mode (APPEND_mode);
                      }

                    setCpuCycle (FETCH_cycle);
                    break;   // don't bump PPR.IC, instruction already did it
                  }

                if (ret == CONT_DIS)
                  {
                    CPT (cpt1U, 25); // DIS instruction


// If we get here, we have encountered a DIS instruction in EXEC_cycle.
//
// We need to idle the CPU until one of the following conditions:
//
//  An external interrupt occurs.
//  The Timer Register underflows.
//  The emulator polled devices need polling.
//
// The external interrupt will only be posted to the CPU engine if the
// device poll posts an interrupt. This means that we do not need to
// detect the interrupts here; if we wake up and poll the devices, the 
// interrupt will be detected by the DIS instruction when it is re-executed.
//
// The Timer Register is a fast, high-precision timer but Multics uses it 
// in only two ways: detecting I/O lockup during early boot, and process
// quantum scheduling (1/4 second for Multics).
//
// Neither of these require high resolution or high accuracy.
//
// The goal of the polling code is sample at about 100Hz; updating the timer
// register at that rate should suffice.
//
//    sleep for 1/100 of a second
//    update the polling state to trigger a poll
//    update the timer register by 1/100 of a second
//    force the simh queues to process
//    continue processing
//


// The usleep logic is not smart enough w.r.t. ROUND_ROBIN/ISOLTS.
// The sleep should only happen if all running processors are in
// DIS mode.
#ifndef ROUND_ROBIN
                    // 1/100 is .01 secs.
                    // *1000 is 10  milliseconds
                    // *1000 is 10000 microseconds
                    // in uSec;
                    usleep (10000);

#ifndef NO_EV_POLL
                    // Trigger I/O polling
                    uv_run (ev_poll_loop, UV_RUN_NOWAIT);
                    fastQueueSubsample = 0;
#else
                    // this ignores the amount of time since the last poll;
                    // worst case is the poll delay of 1/50th of a second.
                    slowQueueSubsample += 10240; // ~ 1Hz
                    queueSubsample += 10240; // ~100Hz
#endif

                    sim_interval = 0;
                    // Timer register runs at 512 KHz
                    // 512000 is 1 second
                    // 512000/100 -> 5120  is .01 second
         
#ifdef TR_WORK
                    cpu.rTRticks = 0;
#endif
                    // Would we have underflowed while sleeping?
                    if ((cpu.rTR & ~ MASK27) || cpu.rTR <= 5120)
                      {
                        if (cpu.switches.tro_enable)
                          setG7fault (currentRunningCpuIdx, FAULT_TRO,
                                      fst_zero);
                      }
                    cpu.rTR = (cpu.rTR - 5120) & MASK27;
#endif
                    break;
                  }

                cpu.wasXfer = false;

                if (ret < 0)
                  {
                    sim_printf ("execute instruction returned %d?\n", ret);
                    break;
                  }

                if ((! cpu.cu.repeat_first) &&
                    (cpu.cu.rpt ||
                     (cpu.cu.rd && (cpu.PPR.IC & 1)) ||
                     cpu.cu.rl))
                  {
                    CPT (cpt1U, 26); // RPx instruction
                    if (cpu.cu.rd)
                      -- cpu.PPR.IC;
                    cpu.wasXfer = false; 
                    setCpuCycle (FETCH_cycle);
                    break;
                  }

                if (cpu.cycle == FAULT_EXEC_cycle)
                  {
                  }
// If we just did the odd word of a fault pair

                if (cpu.cycle == FAULT_EXEC_cycle &&
                    (! cpu.cu.xde) &&
                    cpu.cu.xdo)
                  {
                    clear_TEMPORARY_ABSOLUTE_mode ();
                    cu_safe_restore ();
                    cpu.wasXfer = false;
                    CPT (cpt1U, 12); // cu restored
                    setCpuCycle (FETCH_cycle);
                    clearFaultCycle ();
                    // cu_safe_restore should have restored CU.IWB, so
                    // we can determine the instruction length.
                    // decodeInstruction() restores ci->info->ndes
                    decodeInstruction (IWB_IRODD, & cpu.currentInstruction);

                    cpu.PPR.IC += ci->info->ndes;
                    cpu.PPR.IC ++;
                    break;
                  }

// If we just did the odd word of a interrupt pair

                if (cpu.cycle == INTERRUPT_EXEC_cycle &&
                    (! cpu.cu.xde) &&
                    cpu.cu.xdo)
                  {
                    clear_TEMPORARY_ABSOLUTE_mode ();
                    cu_safe_restore ();
// The only place cycle is set to INTERRUPT_cycle in FETCH_cycle; therefore
// we can safely assume that is the state that should be restored.
                    CPT (cpt1U, 12); // cu restored
                  }

// Even word of fault or interrupt pair

                if (cpu.cycle != EXEC_cycle && cpu.cu.xde)
                  {
                    // Get the odd
                    cpu.cu.IWB = cpu.cu.IRODD;
                    cpu.cu.xde = 0;
                    cpu.isExec = true;
                    break; // go do the odd word
                  }

                if (cpu.cu.xde || cpu.cu.xdo) // we are in an XEC/XED
                  {
                    CPT (cpt1U, 27); // XEx instruction
                    cpu.wasXfer = false; 
                    setCpuCycle (FETCH_cycle);
                    break;
                  }

                cpu.cu.xde = cpu.cu.xdo = 0;
                cpu.isExec = false;
                cpu.isXED = false;

                if (cpu.cycle != INTERRUPT_EXEC_cycle)
                  {
                    cpu.PPR.IC ++;
                    if (ci->info->ndes > 0)
                      cpu.PPR.IC += ci->info->ndes;
                  }

                CPT (cpt1U, 28); // enter fetch cycle
                cpu.wasXfer = false; 
                setCpuCycle (FETCH_cycle);
              }
              break;

            case SYNC_FAULT_RTN_cycle:
              {
                CPT (cpt1U, 29); // sync. fault return
                cpu.wasXfer = false; 
                // cu_safe_restore should have restored CU.IWB, so
                // we can determine the instruction length.
                // decodeInstruction() restores ci->info->ndes
                decodeInstruction (IWB_IRODD, & cpu.currentInstruction);

                cpu.PPR.IC += ci->info->ndes;
                cpu.PPR.IC ++;
                cpu.wasXfer = false; 
                setCpuCycle (FETCH_cycle);
              }
              break;

            case FAULT_cycle:
              {
                CPT (cpt1U, 30); // fault cycle
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

                // F(A)NP should never be stored when faulting.
                // ISOLTS-865 01a,870 02d
                // Unconditional reset of APU status to FABS breaks boot.
                // Checking for F(A)NP here is equivalent to checking that the
                // last append cycle has made it as far as H/I without a fault.
                // Also reset it on TRB fault. ISOLTS-870 05a
                if (cpu.cu.APUCycleBits & 060 || cpu.secret_addressing_mode)
                    setAPUStatus (apuStatus_FABS);

                // XXX the whole fault cycle should be rewritten as an xed 
                // instruction pushed to IWB and executed 

                // AL39: TRB fault doesn't safestore CUD - the original fault
                // CUD should be stored

                // ISOLTS-870 05a: CUD[5] and IWB are safe stored, possibly
                //  due to CU overlap

                // keep IRODD untouched if TRB occurred in an even location
                if (cpu.faultNumber != FAULT_TRB || cpu.cu.xde == 0)
                  {
                    cu_safe_store ();
                  }
                else
                  {
                    word36 tmpIRODD = cpu.scu_data[7];
                    cu_safe_store ();
                    cpu.scu_data[7] = tmpIRODD;
                  }
                CPT (cpt1U, 31); // safe store complete

                // Temporary absolute mode
                set_TEMPORARY_ABSOLUTE_mode ();

                // Set to ring 0
                cpu.PPR.PRR = 0;
                cpu.TPR.TRR = 0;

                // (12-bits of which the top-most 7-bits are used)
                uint fltAddress = (cpu.switches.FLT_BASE << 5) & 07740;
#ifdef L68
                if (cpu.is_FFV)
                  {
                    cpu.is_FFV = false;
                    CPTUR (cptUseMR);
                    // The high 15 bits
                    fltAddress = (cpu.MR.FFV & MASK15) << 3;
                  }
#endif

                // absolute address of fault YPair
                word24 addr = fltAddress + 2 * cpu.faultNumber;
  
                core_read2 (addr, & cpu.cu.IWB, & cpu.cu.IRODD, __func__);
                cpu.cu.xde = 1;
                cpu.cu.xdo = 1;

                CPT (cpt1U, 33); // set fault exec cycle
                setCpuCycle (FAULT_EXEC_cycle);

                break;
              }


          }  // switch (cpu.cycle)
      } 
#ifdef ROUND_ROBIN
    while (0);
   if (reason == 0)
     goto setCPU;
#else
    while (reason == 0);
#endif

leave:

#ifdef HDBG
    hdbgPrint ();
#endif
    sim_printf ("\nsimCycles = %"PRId64"\n", sim_timell ());
    sim_printf ("\ncpuCycles = %"PRId64"\n", sys_stats.total_cycles);
    for (int i = 0; i < N_FAULTS; i ++)
      {
        if (sys_stats.total_faults [i])
          sim_printf ("%s faults = %"PRId64"\n",
                      faultNames [i], sys_stats.total_faults [i]);
      }
    
#ifdef M_SHARED
// simh needs to have the IC statically allocated, so a placeholder was
// created. Update the placeholder in so the IC can be seen by simh, and
// restarting sim_instr doesn't lose the place.

    setCPUnum (0);
    dummyIC = cpu.PPR.IC;
#endif

    return reason;
  }

/*!
 cd@libertyhaven.com - sez ....
 If the instruction addresses a block of four words, the target of the
instruction is supposed to be an address that is aligned on a four-word
boundary (0 mod 4). If not, the processor will grab the four-word block
containing that address that begins on a four-word boundary, even if it has to
go back 1 to 3 words. Analogous explanation for 8, 16, and 32 cases.
 
 olin@olinsibert.com - sez ...
 It means that the appropriate low bits of the address are forced to zero. So
it's the previous words, not the succeeding words, that are used to satisfy the
request.
 
 -- Olin

 */

int OPSIZE (void)
  {
    DCDstruct * i = & cpu.currentInstruction;
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

t_stat ReadOP (word18 addr, _processor_cycle_type cyctyp)
  {
    CPT (cpt1L, 6); // ReadOP

    switch (OPSIZE ())
      {
        case 1:
            CPT (cpt1L, 7); // word
            Read (addr, & cpu.CY, cyctyp);
            return SCPE_OK;
        case 2:
            CPT (cpt1L, 8); // double word
            addr &= 0777776;   // make even
            Read2 (addr, cpu.Ypair, cyctyp);
            break;
        case 8:
            CPT (cpt1L, 9); // oct word
            addr &= 0777770;   // make on 8-word boundary
            Read8 (addr, cpu.Yblock8, false);
            break;
        case 16:
            CPT (cpt1L, 10); // 16 words
            addr &= 0777770;   // make on 8-word boundary
            Read16 (addr, cpu.Yblock16);
            break;
        case 32:
            CPT (cpt1L, 11); // 32 words
            addr &= 0777740;   // make on 32-word boundary
            for (uint j = 0 ; j < 32 ; j += 1)
                Read (addr + j, cpu.Yblock32 + j, cyctyp);
            
            break;
      }
    //cpu.TPR.CA = addr;  // restore address
    
    return SCPE_OK;

  }

// write instruction operands

t_stat WriteOP (word18 addr, UNUSED _processor_cycle_type cyctyp)
  {
    switch (OPSIZE ())
      {
        case 1:
            CPT (cpt1L, 12); // word
            Write (addr, cpu.CY, OPERAND_STORE);
            break;
        case 2:
            CPT (cpt1L, 13); // double word
            addr &= 0777776;   // make even
            Write2 (addr + 0, cpu.Ypair, OPERAND_STORE);
            break;
        case 8:
            CPT (cpt1L, 14); // 8 words
            addr &= 0777770;   // make on 8-word boundary
            Write8 (addr, cpu.Yblock8, false);
            break;
        case 16:
            CPT (cpt1L, 15); // 16 words
            addr &= 0777770;   // make on 8-word boundary
            Write16 (addr, cpu.Yblock16);
            break;
        case 32:
            CPT (cpt1L, 16); // 32 words
            addr &= 0777740;   // make on 32-word boundary
            for (uint j = 0 ; j < 32 ; j += 1)
                Write (addr + j, cpu.Yblock32[j], OPERAND_STORE);
            break;
      }
    
    return SCPE_OK;
    
  }

t_stat memWatch (int32 arg, const char * buf)
  {
    if (strlen (buf) == 0)
      {
        if (arg)
          {
            sim_printf ("no argument to watch?\n");
            return SCPE_ARG;
          }
        sim_printf ("Clearing all watch points\n");
        memset (& watchBits, 0, sizeof (watchBits));
        return SCPE_OK;
      }
    char * end;
    long int n = strtol (buf, & end, 0);
    if (* end || n < 0 || n >= MEMSIZE)
      {
        sim_printf ("invalid argument to watch?\n");
        return SCPE_ARG;
      }
    watchBits [n] = arg != 0;
    return SCPE_OK;
  }

/*!
 * "Raw" core interface ....
 */

#ifndef SPEED
static void nem_check (word24 addr, char * context)
  {
#ifdef SCUMEM
    word24 offset;
    if (lookup_cpu_mem_map (addr, & offset) < 0)
      {
        doFault (FAULT_STR, fst_str_nea,  context);
      }
#else
    if (lookup_cpu_mem_map (addr) < 0)
      {
        doFault (FAULT_STR, fst_str_nea,  context);
      }
#endif
  }
#endif

#ifndef SPEED
int32 core_read (word24 addr, word36 *data, const char * ctx)
  {
    PNL (cpu.portBusy = true;)
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          { 
            doFault (FAULT_STR, fst_str_nea,  __func__);
          }
        addr = (uint) os + addr % SCBANK;
      }
    else
#endif
      nem_check (addr,  "core_read nem");

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
#ifdef SCUMEM
    word24 offset;
    int scuUnitNum =  lookup_cpu_mem_map (addr, & offset);
    int scuUnitIdx = queryScuUnitIdx ((int) currentRunningCpuIdx, scuUnitNum);
    *data = scu [scuUnitIdx].M[offset] & DMASK;
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o read   %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr,
                    * data, ctx);
      }
#else
    if (M[addr] & MEM_UNINITIALIZED)
      {
        sim_debug (DBG_WARN, & cpu_dev,
                   "Unitialized memory accessed at address %08o; "
                   "IC is 0%06o:0%06o (%s(\n",
                   addr, cpu.PPR.PSR, cpu.PPR.IC, ctx);
      }
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o read   %08o %012"PRIo64" "
                    "(%s)\n",
                    sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr, M [addr],
                    ctx);
        traceInstruction (0);
      }
    *data = M[addr] & DMASK;
#endif
#ifdef TR_WORK_MEM
    cpu.rTRticks ++;
#endif
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read  %08o %012"PRIo64" (%s)\n",
                addr, * data, ctx);
    PNL (trackport (addr, * data));
    return 0;
  }
#endif

#ifndef SPEED
int core_write (word24 addr, word36 data, const char * ctx)
  {
    PNL (cpu.portBusy = true;)
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          { 
            doFault (FAULT_STR, fst_str_nea,  __func__);
          }
        addr = (uint) os + addr % SCBANK;
      }
    else
#endif
      nem_check (addr,  "core_write nem");
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
#ifdef SCUMEM
    word24 offset;
    int scuUnitNum =  lookup_cpu_mem_map (addr, & offset);
    int scuUnitIdx = queryScuUnitIdx ((int) currentRunningCpuIdx, scuUnitNum);
    scu [scuUnitIdx].M[offset] = data & DMASK;
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o write   %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    data & DMASK, ctx);
      }
#else
    M[addr] = data & DMASK;
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o write  %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    M [addr], ctx);
        traceInstruction (0);
      }
#endif
#ifdef TR_WORK_MEM
    cpu.rTRticks ++;
#endif
    sim_debug (DBG_CORE, & cpu_dev,
               "core_write %08o %012"PRIo64" (%s)\n",
                addr, data, ctx);
    PNL (trackport (addr, data));
    return 0;
  }
#endif

#ifndef SPEED
int core_read2 (word24 addr, word36 *even, word36 *odd, const char * ctx)
  {
    PNL (cpu.portBusy = true;)
    if (addr & 1)
      {
        sim_debug (DBG_MSG, & cpu_dev,
                   "warning: subtracting 1 from pair at %o in "
                   "core_read2 (%s)\n", addr, ctx);
        addr &= (word24)~1; /* make it an even address */
      }
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          { 
            doFault (FAULT_STR, fst_str_nea,  __func__);
          }
        addr = (uint) os + addr % SCBANK;
      }
    else
#endif
      nem_check (addr,  "core_read2 nem");

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
#ifdef SCUMEM
    word24 offset;
    int scuUnitNum = lookup_cpu_mem_map (addr, & offset);
    int scuUnitIdx = queryScuUnitIdx ((int) currentRunningCpuIdx, scuUnitNum);
    *even = scu [scuUnitIdx].M[offset++] & DMASK;
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr,
                    * even, ctx);
      }
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr, * even, ctx);
    *odd = scu [scuUnitIdx].M[offset] & DMASK;
    if (watchBits [addr+1])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr+1,
                    * odd, ctx);
      }

    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr+1, * odd, ctx);
#else
    if (M[addr] & MEM_UNINITIALIZED)
      {
        sim_debug (DBG_WARN, & cpu_dev,
                   "Unitialized memory accessed at address %08o; "
                   "IC is 0%06o:0%06o (%s)\n",
                   addr, cpu.PPR.PSR, cpu.PPR.IC, ctx);
      }
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    M [addr], ctx);
        traceInstruction (0);
      }
    *even = M[addr++] & DMASK;
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr - 1, * even, ctx);
    PNL (trackport (addr - 1, * even));

    // if the even address is OK, the odd will be
    //nem_check (addr,  "core_read2 nem");
    if (M[addr] & MEM_UNINITIALIZED)
      {
        sim_debug (DBG_WARN, & cpu_dev,
                   "Unitialized memory accessed at address %08o; "
                   "IC is 0%06o:0%06o (%s)\n",
                    addr, cpu.PPR.PSR, cpu.PPR.IC, ctx);
      }
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr,
                    M [addr], ctx);
        traceInstruction (0);
      }

    *odd = M[addr] & DMASK;
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr, * odd, ctx);
#endif
#ifdef TR_WORK_MEM
    cpu.rTRticks ++;
#endif
    PNL (trackport (addr, * odd));
    return 0;
  }
#endif

#ifndef SPEED
int core_write2 (word24 addr, word36 even, word36 odd, const char * ctx)
  {
    PNL (cpu.portBusy = true;)
    if (addr & 1)
      {
        sim_debug (DBG_MSG, & cpu_dev,
                   "warning: subtracting 1 from pair at %o in core_write2 "
                   "(%s)\n", addr, ctx);
        addr &= (word24)~1; /* make it even a dress, or iron a skirt ;) */
      }
#ifdef ISOLTS
    if (cpu.switches.useMap)
      {
        uint pgnum = addr / SCBANK;
        int os = cpu.scbank_pg_os [pgnum];
        if (os < 0)
          { 
            doFault (FAULT_STR, fst_str_nea,  __func__);
          }
        addr = (word24)os + addr % SCBANK;
      }
    else
#endif
      nem_check (addr,  "core_write2 nem");
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

#ifdef SCUMEM
    word24 offset;
    int scuUnitNum =  lookup_cpu_mem_map (addr, & offset);
    int scuUnitIdx = queryScuUnitIdx ((int) currentRunningCpuIdx, scuUnitNum);
    scu [scuUnitIdx].M[offset++] = even & DMASK;
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr,
                    even, ctx);
      }
    scu [scuUnitIdx].M[offset] = odd & DMASK;
    if (watchBits [addr+1])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr+1,
                    odd, ctx);
      }
#else
    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr,
                    even, ctx);
        traceInstruction (0);
      }
    M[addr++] = even & DMASK;
    PNL (trackport (addr - 1, even));
    sim_debug (DBG_CORE, & cpu_dev,
               "core_write2 %08o %012"PRIo64" (%s)\n",
                addr - 1, even, ctx);

    // If the even address is OK, the odd will be
    //nem_check (addr,  "core_write2 nem");

    if (watchBits [addr])
      {
        sim_printf ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", sim_timell (), cpu.PPR.PSR, cpu.PPR.IC, addr,
                    odd, ctx);
        traceInstruction (0);
      }
    M[addr] = odd & DMASK;
#endif
#ifdef TR_WORK_MEM
    cpu.rTRticks ++;
#endif
    PNL (trackport (addr, odd));
    sim_debug (DBG_CORE, & cpu_dev,
               "core_write2 %08o %012"PRIo64" (%s)\n",
                addr, odd, ctx);
    return 0;
  }
#endif

#ifndef QUIET_UNUSED
//=============================================================================

/*
 * encode_instr()
 *
 * Convert an instr_t struct into a  36-bit word.
 *
 */

void encode_instr (const instr_t *ip, word36 *wordp)
  {
    *wordp = 0;
    putbits36_18 (wordp, 0, ip->addr);
#if 1
    putbits36_10 (wordp, 18, ip->opcode);
#else
    putbits36_9 (*wordp, 18, ip->opcode & 0777);
    putbits36_1 (*wordp, 27, ip->opcode >> 9);
#endif
    putbits36_1 (wordp, 28, ip->inhibit);
    if (! is_eis[ip->opcode & MASKBITS (10)])
     {
        putbits36_1 (wordp, 29, ip->mods.single.pr_bit);
        putbits36_6 (wordp, 30, ip->mods.single.tag);
      }
    else
     {
        putbits36_1 (wordp, 29, ip->mods.mf1.ar);
        putbits36_1 (wordp, 30, ip->mods.mf1.rl);
        putbits36_1 (wordp, 31, ip->mods.mf1.id);
        putbits36_4 (wordp, 32, ip->mods.mf1.reg);
      }
  }
#endif

    

/*
 * instruction fetcher ...
 * fetch + decode instruction at 18-bit address 'addr'
 */

/*
 * instruction decoder .....
 *
 */

void decodeInstruction (word36 inst, DCDstruct * p)
  {
    CPT (cpt1L, 17); // instruction decoder
    memset (p, 0, sizeof (struct DCDstruct));

    p->opcode  = GET_OP (inst);   // get opcode
    p->opcodeX = GET_OPX(inst);   // opcode extension
    p->address = GET_ADDR (inst); // address field from instruction
    p->b29     = GET_A (inst);    // "A" the indirect via pointer register flag
    p->i       = GET_I (inst);    // "I" inhibit interrupt flag
    p->tag     = GET_TAG (inst);  // instruction tag
    
    p->info = getIWBInfo (p);     // get info for IWB instruction
    
    if (p->info->flags & IGN_B29)
        p->b29 = 0;   // make certain 'a' bit is valid always

    if (p->info->ndes > 0)
      {
        p->b29 = 0;
        p->tag = 0;
        if (p->info->ndes > 1)
          {
            memset (& cpu.currentEISinstruction, 0,
                    sizeof (cpu.currentEISinstruction)); 
          }
      }

    // Save the RFI
    p->restart = cpu.cu.rfi != 0;
    cpu.cu.rfi = 0;
    if (p->restart)
      {
        sim_debug (DBG_TRACE, & cpu_dev, "restart\n");
      }
  }

// MM stuff ...

//
// is_priv_mode()
//
// Report whether or or not the CPU is in privileged mode.
// True if in absolute mode or if priv bit is on in segment TPR.TSR
// The processor executes instructions in privileged mode when forming
// addresses in absolute mode or when forming addresses in append mode and the
// segment descriptor word (SDW) for the segment in execution specifies a
// privileged procedure and the execution ring is equal to zero.
//
// PPR.P A flag controlling execution of privileged instructions.
//
// Its value is 1 (permitting execution of privileged instructions) if PPR.PRR
// is 0 and the privileged bit in the segment descriptor word (SDW.P) for the
// procedure is 1; otherwise, its value is 0.
//
 
int is_priv_mode (void)
  {
    
// Back when it was ABS/APP/BAR, this test was right; now that
// it is ABS/APP,BAR/NBAR, check bar mode.
// Fixes ISOLTS 890 05a.
    if (get_bar_mode ())
      return 0;

// PPR.P is only relevant if we're in APPEND mode. ABSOLUTE mode ignores it.
    if (get_addr_mode () == ABSOLUTE_mode)
      return 1;
    else if (cpu.PPR.P)
      return 1;

    return 0;
  }

#ifndef NOWENT
void set_went_appending (void)
  {
    CPT (cpt1L, 18); // set went appending
    cpu.went_appending = true;
  }

void clr_went_appending (void)
  {
    CPT (cpt1L, 19); // clear went appending
    cpu.went_appending = false;
  }

bool get_went_appending (void)
  {
    return cpu.went_appending;
  }
#endif

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
    CPT (cpt1L, 20); // set temp. abs. mode
    cpu.secret_addressing_mode = true;
#ifdef NOWENT
    cpu.cu.XSF = false;
#else
    cpu.went_appending = false;
#endif
  }

static bool clear_TEMPORARY_ABSOLUTE_mode (void)
  {
    CPT (cpt1L, 21); // clear temp. abs. mode
    cpu.secret_addressing_mode = false;
#ifdef NOWENT
    return cpu.cu.XSF;
#else
    return cpu.went_appending;
#endif
  }

/* 
 * get_bar_mode: During fault processing, we do not want to fetch and execute
 * the fault vector instructions in BAR mode. We leverage the
 * secret_addressing_mode flag that is set in set_TEMPORARY_ABSOLUTE_MODE to
 * direct us to ignore the I_NBAR indicator register.
 */

bool get_bar_mode (void)
  {
    return ! (cpu.secret_addressing_mode || TST_I_NBAR);
  }

addr_modes_t get_addr_mode (void)
  {
    if (cpu.secret_addressing_mode)
        return ABSOLUTE_mode; // This is not the mode you are looking for

    // went_appending does not alter privileged state (only enables appending)
    // the went_appending check is only required by ABSA, AFAICT
    // pft 02b 013255, ISOLTS-860
    //if (cpu.went_appending)
    //    return APPEND_mode;

    if (TST_I_ABS)
      {
          return ABSOLUTE_mode;
      }
    else
      {
          return APPEND_mode;
      }
  }


/*
 * set_addr_mode()
 *
 * Put the CPU into the specified addressing mode.   This involves
 * setting a couple of IR flags and the PPR priv flag.
 *
 */

void set_addr_mode (addr_modes_t mode)
  {
#ifdef NOWENT
    cpu.cu.XSF = false;
#else
    cpu.went_appending = false;
#endif
// Temporary hack to fix fault/intr pair address mode state tracking
//   1. secret_addressing_mode is only set in fault/intr pair processing.
//   2. Assume that the only set_addr_mode that will occur is the b29 special
//   case or ITx.
    //if (secret_addressing_mode && mode == APPEND_mode)
      //set_went_appending ();

    cpu.secret_addressing_mode = false;
    if (mode == ABSOLUTE_mode)
      {
        CPT (cpt1L, 22); // set abs mode
        sim_debug (DBG_DEBUG, & cpu_dev, "APU: Setting absolute mode.\n");

        SET_I_ABS;
        cpu.PPR.P = 1;
        
      }
    else if (mode == APPEND_mode)
      {
        CPT (cpt1L, 23); // set append mode
        if (! TST_I_ABS && TST_I_NBAR)
          sim_debug (DBG_DEBUG, & cpu_dev, "APU: Keeping append mode.\n");
        else
           sim_debug (DBG_DEBUG, & cpu_dev, "APU: Setting append mode.\n");

        CLR_I_ABS;
      }
    else
      {
        sim_debug (DBG_ERR, & cpu_dev,
                   "APU: Unable to determine address mode.\n");
        sim_warn ("APU: Unable to determine address mode. Can't happen!\n");
      }
  }

/*
 * stuff to handle BAR mode ...
 */


/*
 * The Base Address Register provides automatic hardware Address relocation and
 * Address range limitation when the processor is in BAR mode.
 *
 * BAR.BASE: Contains the 9 high-order bits of an 18-bit address relocation
 * constant. The low-order bits are generated as zeros.
 *
 * BAR.BOUND: Contains the 9 high-order bits of the unrelocated address limit.
 * The low- order bits are generated as zeros. An attempt to access main memory
 * beyond this limit causes a store fault, out of bounds. A value of 0 is truly
 * 0, indicating a null memory range.
 * 
 * In BAR mode, the base address register (BAR) is used. The BAR contains an
 * address bound and a base address. All computed addresses are relocated by
 * adding the base address. The relocated address is combined with the
 * procedure pointer register to form the virtual memory address. A program is
 * kept within certain limits by subtracting the unrelocated computed address
 * from the address bound. If the result is zero or negative, the relocated
 * address is out of range, and a store fault occurs.
 */

// CANFAULT
word18 getBARaddress (word18 addr)
  {
    if (cpu . BAR.BOUND == 0)
        // store fault, out of bounds.
        doFault (FAULT_STR, fst_str_oob, "BAR store fault; out of bounds");

    // A program is kept within certain limits by subtracting the
    // unrelocated computed address from the address bound. If the result
    // is zero or negative, the relocated address is out of range, and a
    // store fault occurs.
    //
    // BAR.BOUND - CA <= 0
    // BAR.BOUND <= CA
    // CA >= BAR.BOUND
    //
    if (addr >= (((word18) cpu . BAR.BOUND) << 9))
        // store fault, out of bounds.
        doFault (FAULT_STR, fst_str_oob, "BAR store fault; out of bounds");
    
    word18 barAddr = (addr + (((word18) cpu . BAR.BASE) << 9)) & 0777777;
    return barAddr;
  }

//=============================================================================

int queryScuUnitIdx (int cpu_unit_num, int cpu_port_num)
  {
    struct cpuPort * pp = 
      & cables->cablesFromScuToCpu [cpu_unit_num].ports [cpu_port_num];
    if (pp->inuse)
      return pp->scu_unit_idx;
    return -1;
  }

// XXX when multiple cpus are supported, merge this into simhCpuReset

static void cpu_init_array (void)
  {
    for (int i = 0; i < N_CPU_UNITS_MAX; i ++)
      for (int p = 0; p < N_CPU_PORTS; p ++)
        cables->cablesFromScuToCpu [i].ports [p].inuse = false;
  }

static t_stat cpu_show_config (UNUSED FILE * st, UNIT * uptr, 
                               UNUSED int val, UNUSED const void * desc)
  {
    long unit_num = UNIT_NUM (uptr);
    if (unit_num < 0 || unit_num >= N_CPU_UNITS_MAX)
      {
        sim_printf ("error: invalid unit number %ld\n", unit_num);
        return SCPE_ARG;
      }

#ifdef ROUND_ROBIN
    uint save = setCPUnum ((uint) unit_num);
#endif

    sim_printf ("CPU unit number %ld\n", unit_num);

    sim_printf ("Fault base:               %03o(8)\n",
                cpu.switches.FLT_BASE);
    sim_printf ("CPU number:               %01o(8)\n",
                cpu.switches.cpu_num);
    sim_printf ("Data switches:            %012"PRIo64"(8)\n",
                cpu.switches.data_switches);
    sim_printf ("Address switches:         %06o(8)\n",
                cpu.switches.addr_switches);
    for (int i = 0; i < N_CPU_PORTS; i ++)
      {
        sim_printf ("Port%c enable:             %01o(8)\n",
                    'A' + i, cpu.switches.enable [i]);
        sim_printf ("Port%c init enable:        %01o(8)\n",
                    'A' + i, cpu.switches.init_enable [i]);
        sim_printf ("Port%c assignment:         %01o(8)\n",
                    'A' + i, cpu.switches.assignment [i]);
        sim_printf ("Port%c interlace:          %01o(8)\n",
                    'A' + i, cpu.switches.assignment [i]);
        sim_printf ("Port%c store size:         %01o(8)\n",
                    'A' + i, cpu.switches.store_size [i]);
      }
    sim_printf ("Processor mode:           %s [%o]\n", 
                cpu.switches.proc_mode ? "Multics" : "GCOS",
                cpu.switches.proc_mode);
    sim_printf ("Processor speed:          %02o(8)\n", 
                cpu.switches.proc_speed);
    sim_printf ("DIS enable:               %01o(8)\n", 
                cpu.switches.dis_enable);
    sim_printf ("Steady clock:             %01o(8)\n", 
                scu [0].steady_clock);
    sim_printf ("Halt on unimplemented:    %01o(8)\n", 
                cpu.switches.halt_on_unimp);
    sim_printf ("Disable SDWAM/PTWAM:      %01o(8)\n", 
                cpu.switches.disable_wam);
    sim_printf ("Report faults:            %01o(8)\n", 
                cpu.switches.report_faults);
    sim_printf ("TRO faults enabled:       %01o(8)\n", 
                cpu.switches.tro_enable);
    sim_printf ("Y2K enabled:              %01o(8)\n", 
                scu [0].y2k);
    sim_printf ("drl fatal enabled:        %01o(8)\n", 
                cpu.switches.drl_fatal);
    sim_printf ("useMap:                   %d\n",
                cpu.switches.useMap);
    sim_printf ("Disable cache:            %01o(8)\n",
                cpu.switches.disable_cache);

#ifdef ROUND_ROBIN
    setCPUnum (save);
#endif

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
//           dis_enable = n
//           steadyclock = on|off
//           halt_on_unimplmented = n
//           disable_wam = n
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
#ifdef L68
    { "e", 4 },
    { "f", 5 },
    { "g", 6 },
    { "h", 7 },
#endif
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
#ifdef L68
// rsw.incl.pl1
//
//  /* DPS and L68 memory sizes */
//  dcl  dps_mem_size_table (0:7) fixed bin (24) static options (constant) init
//      (32768, 65536, 4194304, 131072, 524288, 1048576, 2097152, 262144);
//  
//  Note that the third array element above, is changed incompatibly in MR10.0.
//  In previous releases, this array element was used to decode a port size of
//  98304 (96K). With MR10.0 it is now possible to address 4MW per CPU port, by
//  installing  FCO # PHAF183 and using a group 10 patch plug, on L68 and DPS
//  CPUs.

    { "32", 0 },    //   32768
    { "64", 1 },    //   65536
    { "4096", 2 },  // 4194304
    { "128", 3 },   //  131072
    { "512", 4 },   //  524288
    { "1024", 5 },  // 1048576
    { "2048", 6 },  // 2097152
    { "256", 7 },   //  262144

    { "32K", 0 },
    { "64K", 1 },
    { "4096K", 2 },
    { "128K", 3 },
    { "512K", 4 },
    { "1024K", 5 },
    { "2048K", 6 },
    { "256K", 7 },

    { "1M", 5 },
    { "2M", 6 },
    { "4M", 2 },
#endif // L68

#ifdef DPS8M
// These values are taken from the dps8_mem_size_table loaded by the boot tape.

    {    "32", 0 },
    {    "64", 1 },
    {   "128", 2 },
    {   "256", 3 }, 
    {   "512", 4 }, 
    {  "1024", 5 },
    {  "2048", 6 },
    {  "4096", 7 },

    {   "32K", 0 },
    {   "64K", 1 },
    {  "128K", 2 },
    {  "256K", 3 },
    {  "512K", 4 },
    { "1024K", 5 },
    { "2048K", 6 },
    { "4096K", 7 },

    { "1M", 5 },
    { "2M", 6 },
    { "4M", 7 },
#endif // DPS8M
    { NULL, 0 }

  };

static config_list_t cpu_config_list [] =
  {
    { "faultbase", 0, 0177, cfg_multics_fault_base },
    { "num", 0, 07, NULL },
    { "data", 0, 0777777777777, NULL },
    { "mode", 0, 01, cfg_cpu_mode }, 
    { "speed", 0, 017, NULL }, // XXX use keywords
    { "port", 0, N_CPU_PORTS - 1, cfg_port_letter },
    { "assignment", 0, 7, NULL },
    { "interlace", 0, 1, cfg_interlace },
    { "enable", 0, 1, cfg_on_off },
    { "init_enable", 0, 1, cfg_on_off },
    { "store_size", 0, 7, cfg_size_list },

    // Hacks

    { "dis_enable", 0, 1, cfg_on_off }, 
    // steady_clock was moved to SCU; keep here for script compatibility
    { "steady_clock", 0, 1, cfg_on_off },
    { "halt_on_unimplemented", 0, 1, cfg_on_off },
    { "disable_wam", 0, 1, cfg_on_off },
    { "report_faults", 0, 2, NULL },
    { "tro_enable", 0, 1, cfg_on_off },
    // y2k was moved to SCU; keep here for script compatibility
    { "y2k", 0, 1, cfg_on_off },
    { "drl_fatal", 0, 1, cfg_on_off },
    { "useMap", 0, 1, cfg_on_off },
    { "address", 0, 0777777, NULL },
    { "disable_cache", 0, 1, cfg_on_off },
    { NULL, 0, 0, NULL }
  };

static t_stat simhCpuResetAndClearUnit (UNUSED UNIT * uptr, 
                                        UNUSED int32 value,
                                        UNUSED const char * cptr, 
                                        UNUSED void * desc)
  {
    long cpu_unit_num = UNIT_NUM (uptr);
#ifdef ISOLTS
    cpu_state_t * cpun = cpus + cpu_unit_num;
    if (cpun->switches.useMap)
      {
        for (uint pgnum = 0; pgnum < N_SCBANKS; pgnum ++)
          {
            int os = cpun->scbank_pg_os [pgnum];
            if (os < 0)
              continue;
            for (uint addr = 0; addr < SCBANK; addr ++)
              M [addr + (uint) os] = MEM_UNINITIALIZED;
          }
      }
#else
    // Crashes console?
    cpuResetUnitIdx ((uint) cpu_unit_num, true);
#endif
    return SCPE_OK;
  }

static t_stat simhCpuResetUnit (UNIT * uptr, 
                            UNUSED int32 value,
                            UNUSED const char * cptr, 
                            UNUSED void * desc)
  {
    long cpuUnitIdx = UNIT_NUM (uptr);
    cpuResetUnitIdx ((uint) cpuUnitIdx, false); // no clear memory
    return SCPE_OK;
  }

static t_stat cpu_set_config (UNIT * uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc)
  {
// XXX Minor bug; this code doesn't check for trailing garbage

    long cpu_unit_num = UNIT_NUM (uptr);
    if (cpu_unit_num < 0 || cpu_unit_num >= N_CPU_UNITS_MAX)
      {
        sim_printf ("error: cpu_set_config: invalid unit number %ld\n",
                    cpu_unit_num);
        return SCPE_ARG;
      }

#ifdef ROUND_ROBIN
    uint save = setCPUnum ((uint) cpu_unit_num);
#endif

    static int port_num = 0;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("cpu_set_config", cptr, cpu_config_list,
                           & cfg_state, & v);
        if (rc == -1) // done
          {
            break;
          }
        if (rc == -2) // error
          {
            cfgparse_done (& cfg_state);
            return SCPE_ARG; 
          }

        const char * p = cpu_config_list [rc] . name;
        if (strcmp (p, "faultbase") == 0)
          cpu.switches.FLT_BASE = (uint) v;
        else if (strcmp (p, "num") == 0)
          cpu.switches.cpu_num = (uint) v;
        else if (strcmp (p, "data") == 0)
          cpu.switches.data_switches = (word36) v;
        else if (strcmp (p, "address") == 0)
          cpu.switches.addr_switches = (word18) v;
        else if (strcmp (p, "mode") == 0)
          cpu.switches.proc_mode = (uint) v;
        else if (strcmp (p, "speed") == 0)
          cpu.switches.proc_speed = (uint) v;
        else if (strcmp (p, "port") == 0)
          port_num = (int) v;
        else if (strcmp (p, "assignment") == 0)
          cpu.switches.assignment [port_num] = (uint) v;
        else if (strcmp (p, "interlace") == 0)
          cpu.switches.interlace [port_num] = (uint) v;
        else if (strcmp (p, "enable") == 0)
          cpu.switches.enable [port_num] = (uint) v;
        else if (strcmp (p, "init_enable") == 0)
          cpu.switches.init_enable [port_num] = (uint) v;
        else if (strcmp (p, "store_size") == 0)
          cpu.switches.store_size [port_num] = (uint) v;
        else if (strcmp (p, "dis_enable") == 0)
          cpu.switches.dis_enable = (uint) v;
        else if (strcmp (p, "steady_clock") == 0)
          scu [0].steady_clock = (uint) v;
        else if (strcmp (p, "halt_on_unimplemented") == 0)
          cpu.switches.halt_on_unimp = (uint) v;
        else if (strcmp (p, "disable_wam") == 0)
          cpu.switches.disable_wam = (uint) v;
        else if (strcmp (p, "report_faults") == 0)
          cpu.switches.report_faults = (uint) v;
        else if (strcmp (p, "tro_enable") == 0)
          cpu.switches.tro_enable = (uint) v;
        else if (strcmp (p, "y2k") == 0)
          scu [0].y2k = (uint) v;
        else if (strcmp (p, "drl_fatal") == 0)
          cpu.switches.drl_fatal = (uint) v;
        else if (strcmp (p, "useMap") == 0)
          cpu.switches.useMap = v;
        else if (strcmp (p, "disable_cache") == 0)
          cpu.switches.disable_cache = v;
        else
          {
            sim_printf ("error: cpu_set_config: invalid cfgparse rc <%d>\n",
                        rc);
            cfgparse_done (& cfg_state);
            return SCPE_ARG; 
          }
      } // process statements
    cfgparse_done (& cfg_state);

#ifdef ROUND_ROBIN
    setCPUnum (save);
#endif

    return SCPE_OK;
  }

static t_stat cpu_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                               UNUSED int val, UNUSED const void * desc)
  {
    sim_printf ("Number of CPUs in system is %d\n", cpu_dev.numunits);
    return SCPE_OK;
  }

static t_stat cpu_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_CPU_UNITS_MAX)
      return SCPE_ARG;
    cpu_dev.numunits = (uint32) n;
    return SCPE_OK;
  }

void addHist (uint hset, word36 w0, word36 w1)
  {
    //if (cpu.MR.emr)
      {
        cpu.history [hset] [cpu.history_cyclic[hset]] [0] = w0;
        cpu.history [hset] [cpu.history_cyclic[hset]] [1] = w1;
        cpu.history_cyclic[hset] = (cpu.history_cyclic[hset] + 1) % N_HIST_SIZE;
      }
  }

void addHistForce (uint hset, word36 w0, word36 w1)
  {
    cpu.history [hset] [cpu.history_cyclic[hset]] [0] = w0;
    cpu.history [hset] [cpu.history_cyclic[hset]] [1] = w1;
    cpu.history_cyclic[hset] = (cpu.history_cyclic[hset] + 1) % N_HIST_SIZE;
  }

#ifdef DPS8M
void addCUhist (void)
  {
    if (cpu.skip_cu_hist)
      return;
    if (! cpu.MR_cache.emr)
      return;
    if (! cpu.MR_cache.ihr)
      return;
    if (cpu.MR_cache.hrxfr && ! cpu.wasXfer)
      return;

    word36 flags = 0; // XXX fill out
    word5 proccmd = 0; // XXX fill out
    word7 flags2 = 0; // XXX fill out
    word36 w0 = 0, w1 = 0;
    w0 |= flags & 0777777000000;
    w0 |= IWB_IRODD & MASK18;
    w1 |= (cpu.iefpFinalAddress & MASK24) << 12;
    w1 |= (proccmd & MASK5) << 7;
    w1 |= flags2 & 0176;
    addHist (CU_HIST_REG, w0, w1);
  }

void addDUOUhist (word36 flags, word18 ICT, word9 RS_REG, word9 flags2)
  {
    word36 w0 = flags, w1 = 0;
    w1 |= (ICT & MASK18) << 18;
    w1 |= (RS_REG & MASK9) << 9;
    w1 |= flags2 & MASK9;
    addHist (DU_OU_HIST_REG, w0, w1);
  }

void addAPUhist (word15 ESN, word21 flags, word24 RMA, word3 RTRR, word9 flags2)
  {
    word36 w0 = 0, w1 = 0;
    w0 |= (ESN & MASK15) << 21;
    w0 |= flags & MASK21;
    w1 |= (RMA & MASK24) << 12;
    w1 |= (RTRR & MASK3) << 9;
    w1 |= flags2 & MASK9;
    addHist (APU_HIST_REG, w0, w1);
  }

void addEAPUhist (word18 ZCA, word18 opcode)
  {
    word36 w0 = 0;
    w0 |= (ZCA & MASK18) << 18;
    w0 |= opcode & MASK18;
    addHist (EAPU_HIST_REG, w0, 0);
    //cpu.eapu_hist[cpu.eapu_cyclic].ZCA = ZCA;
    //cpu.eapu_hist[cpu.eapu_cyclic].opcode = opcode;
    //cpu.history_cyclic[EAPU_HIST_REG] =
      //(cpu.history_cyclic[EAPU_HIST_REG] + 1) % N_HIST_SIZE;
  }
#endif // DPS8M

#ifdef L68

// According to ISOLTS
//
//   0 PIA
//   1 POA
//   2 RIW
//   3 SIW
//   4 POT
//   5 PON
//   6 RAW
//   7 SAW
//   8 TRGO
//   9 XDE
//  10 XDO
//  11 IC
//  12 RPTS
//  13 WI
//  14 AR F/E
//  15 XIP
//  16 FLT
//  17 COMPL. ADD BASE
//  18:23 OPCODE/TAG
//  24:29 ADDREG
//  30:34 COMMAND A/B/C/D/E
//  35:38 PORT A/B/C/D
//  39 FB XEC
//  40 INS FETCH
//  41 CU STORE
//  42 OU STORE
//  43 CU LOAD
//  44 OU LOAD
//  45 RB DIRECT
//  46 -PC BUSY
//  47 PORT BUSY

void addCUhist (void)
  {
    CPT (cpt1L, 24); // add cu hist
// XXX strobe on opcode match
    if (cpu.skip_cu_hist)
      return;
    if (! cpu.MR_cache.emr)
      return;
    if (! cpu.MR_cache.ihr)
      return;

//IF1 if (cpu.MR.hrhlt) sim_printf ("%u\n", cpu.history_cyclic[CU_HIST_REG]);
//IF1 sim_printf ("%u\n", cpu.history_cyclic[CU_HIST_REG]);
    word36 w0 = 0, w1 = 0;

    // 0 PIA
    // 1 POA
    // 2 RIW
    // 3 SIW
    // 4 POT
    // 5 PON
    // 6 RAW
    // 7 SAW
    PNL (putbits36_8 (& w0, 0, cpu.prepare_state);)
    // 8 TRG 
    putbits36_1 (& w0, 8, cpu.wasXfer);
    // 9 XDE
    putbits36_1 (& w0, 9, cpu.cu.xde);
    // 10 XDO
    putbits36_1 (& w0, 10, cpu.cu.xdo);
    // 11 IC
    putbits36_1 (& w0, 11, USE_IRODD?1:0);
    // 12 RPT
    putbits36_1 (& w0, 12, cpu.cu.rpt);
    // 13 WI Wait for instruction fetch XXX Not tracked
    // 14 ARF "AR F/E" Address register Full/Empty Address has valid data 
    PNL (putbits36_1 (& w0, 14, cpu.AR_F_E);)
    // 15 !XA/Z "-XIP NOT prepare interrupt address" 
    putbits36_1 (& w0, 15, cpu.cycle != INTERRUPT_cycle?1:0);
    // 16 !FA/Z Not tracked. (cu.-FL?)
    putbits36_1 (& w0, 16, cpu.cycle != FAULT_cycle?1:0);
    // 17 M/S  (master/slave, cu.-BASE?, NOT BAR MODE)
    putbits36_1 (& w0, 17, TSTF (cpu.cu.IR, I_NBAR)?1:0);
    // 18:35 IWR (lower half of IWB)
    putbits36_18 (& w0, 18, (word18) (IWB_IRODD & MASK18));

    // 36:53 CA
    putbits36_18 (& w1, 0, cpu.TPR.CA);
    // 54:58 CMD system controller command XXX
    // 59:62 SEL port select (XXX ignoring "only valid if port A-D is selected")
    PNL (putbits36_1 (& w1, 59-36, (cpu.portSelect == 0)?1:0);)
    PNL (putbits36_1 (& w1, 60-36, (cpu.portSelect == 1)?1:0);)
    PNL (putbits36_1 (& w1, 61-36, (cpu.portSelect == 2)?1:0);)
    PNL (putbits36_1 (& w1, 62-36, (cpu.portSelect == 3)?1:0);)
    // 63 XEC-INT An interrupt is present
    putbits36_1 (& w1, 63-36, cpu.interrupt_flag?1:0);
    // 64 INS-FETCH Perform an instruction fetch
    PNL (putbits36_1 (& w1, 64-36, cpu.INS_FETCH?1:0);)
    // 65 CU-STORE Control unit store cycle XXX
    // 66 OU-STORE Operations unit store cycle XXX
    // 67 CU-LOAD Control unit load cycle XXX
    // 68 OU-LOAD Operations unit load cycle XXX
    // 69 DIRECT Direct cycle XXX
    // 70 -PC-BUSY Port control logic not busy XXX
    // 71 BUSY Port interface busy XXX

    addHist (CU_HIST_REG, w0, w1);

    // Check for overflow
    CPTUR (cptUseMR);
    if (cpu.MR.hrhlt && cpu.history_cyclic[CU_HIST_REG] == 0)
      {
        //cpu.history_cyclic[CU_HIST_REG] = 15;
        if (cpu.MR.ihrrs)
          {
            cpu.MR.ihr = 0;
          }
//IF1 sim_printf ("trapping......\n");
        set_FFV_fault (4);
        return;
      }
  }

// du history register inputs(actual names)
// bit 00= fpol-cx;010       bit 36= fdud-dg;112
// bit 01= fpop-cx;010       bit 37= fgdlda-dc;010
// bit 02= need-desc-bd;000  bit 38= fgdldb-dc;010
// bit 03= sel-adr-bd;000    bit 39= fgdldc-dc;010
// bit 04= dlen=direct-bd;000bit 40= fnld1-dp;110
// bit 05= dfrst-bd;021      bit 41= fgldp1-dc;110
// bit 06= fexr-bd;010       bit 42= fnld2-dp;110
// bit 07= dlast-frst-bd;010 bit 43= fgldp2-dc;110
// bit 08= ddu-ldea-bd;000   bit 44= fanld1-dp;110
// bit 09= ddu-stea-bd;000   bit 45= fanld2-dp;110
// bit 10= dredo-bd;030      bit 46= fldwrt1-dp;110
// bit 11= dlvl<wd-sz-bg;000 bit 47= fldwrt2-dp;110
// bit 12= exh-bg;000        bit 48= data-avldu-cm;000
// bit 13= dend-seg-bd;111   bit 49= fwrt1-dp;110
// bit 14= dend-bd;000       bit 50= fgstr-dc;110
// bit 15= du=rd+wrt-bd;010  bit 51= fanstr-dp;110
// bit 16= ptra00-bd;000     bit 52= fstr-op-av-dg;010
// bit 17= ptra01-bd;000     bit 53= fend-seg-dg;010
// bit 18= fa/i1-bd;110      bit 54= flen<128-dg;010
// bit 19= fa/i2-bd;110      bit 55= fgch-dp;110
// bit 20= fa/i3-bd;110      bit 56= fanpk-dp;110
// bit 21= wrd-bd;000        bit 57= fexmop-dl;110
// bit 22= nine-bd;000       bit 58= fblnk-dp;100
// bit 23= six-bd;000        bit 59= unused
// bit 24= four-bd;000       bit 60= dgbd-dc;100
// bit 25= bit-bd;000        bit 61= dgdb-dc;100
// bit 26= unused            bit 62= dgsp-dc;100
// bit 27= unused            bit 63= ffltg-dc;110
// bit 28= unused            bit 64= frnd-dg;120
// bit 29= unused            bit 65= dadd-gate-dc;100
// bit 30= fsampl-bd;111     bit 66= dmp+dv-gate-db;100
// bit 31= dfrst-ct-bd;010   bit 67= dxpn-gate-dg;100
// bit 32= adj-lenint-cx;000 bit 68= unused
// bit 33= fintrptd-cx;010   bit 69= unused
// bit 34= finhib-stc1-cx;010bit 70= unused
// bit 35= unused            bit 71= unused

void addDUhist (void)
  {
    CPT (cpt1L, 25); // add du hist
    PNL (addHist (DU_HIST_REG, cpu.du.cycle1, cpu.du.cycle2);)
  }
     

void addOUhist (void)
  {
    CPT (cpt1L, 26); // add ou hist
    word36 w0 = 0, w1 = 0;
     
    // 0-16 RP
    //   0-8 OP CODE
    PNL (putbits36_9 (& w0, 0, cpu.ou.RS);)
    
    //   9 CHAR
    putbits36_1 (& w0, 9, cpu.ou.characterOperandSize ? 1 : 0);

    //   10-12 TAG 1/2/3
    putbits36_3 (& w0, 10, cpu.ou.characterOperandOffset);

    //   13 CRFLAG
    putbits36_1 (& w0, 13, cpu.ou.crflag);

    //   14 DRFLAG
    putbits36_1 (& w0, 14, cpu.ou.directOperandFlag ? 1 : 0);

    //   15-16 EAC
    putbits36_2 (& w0, 15, cpu.ou.eac);

    // 17 0
    // 18-26 RS REG
    PNL (putbits36_9 (& w0, 18, cpu.ou.RS);)

    // 27 RB1 FULL
    putbits36_1 (& w0, 27, cpu.ou.RB1_FULL);

    // 28 RP FULL
    putbits36_1 (& w0, 28, cpu.ou.RP_FULL);

    // 29 RS FULL
    putbits36_1 (& w0, 29, cpu.ou.RS_FULL);

    // 30-35 GIN/GOS/GD1/GD2/GOE/GOA
    putbits36_6 (& w0, 30, (word6) (cpu.ou.cycle >> 3));

    // 36-38 GOM/GON/GOF
    putbits36_3 (& w1, 36-36, (word3) cpu.ou.cycle);

    // 39 STR OP
    putbits36_1 (& w1, 39-36, cpu.ou.STR_OP);

    // 40 -DA-AV XXX

    // 41-50 stuvwyyzAB -A-REG -Q-REG -X0-REG .. -X7-REG
    PNL (putbits36_10 (& w1, 41-36,
         (word10) ~NonEISopcodes [cpu.ou.RS].reg_use);)

    // 51-53 0

    // 54-71 ICT TRACKER
    putbits36_18 (& w1, 54 - 36, cpu.PPR.IC);

    addHist (OU_HIST_REG, w0, w1);
  }

// According to ISOLTS
//  0:2 OPCODE RP
//  3 9 BIT CHAR
//  4:6 TAG 3/4/5
//  7 CR FLAG
//  8 DIR FLAG
//  9 RP15
// 10 RP16
// 11 SPARE
// 12:14 OPCODE RS
// 15 RB1 FULL
// 16 RP FULL
// 17 RS FULL
// 18 GIN
// 19 GOS
// 20 GD1
// 21 GD2
// 22 GOE
// 23 GOA
// 24 GOM
// 25 GON
// 26 GOF
// 27 STORE OP
// 28 DA NOT
// 29:38 COMPLEMENTED REGISTER IN USE FLAG A/Q/0/1/2/3/4/5/6/7
// 39 ?
// 40 ?
// 41 ? 
// 42:47 ICT TRACT

// XXX addAPUhist

//  0:5 SEGMENT NUMBER
//  6 SNR/ESN
//  7 TSR/ESN
//  8 FSDPTW
//  9 FPTW2
// 10 MPTW
// 11 FANP
// 12 FAP
// 13 AMSDW
// 14:15 AMSDW #
// 16 AMPTW
// 17:18 AMPW #
// 19 ACV/DF
// 20:27 ABSOLUTE MEMORY ADDRESS
// 28 TRR #
// 29 FLT HLD

void addAPUhist (enum APUH_e op)
  {
    CPT (cpt1L, 28); // add apu hist
    word36 w0 = 0, w1 = 0;
     
    w0 = op; // set 17-24 FDSPTW/.../FAP bits

    // 0-14 ESN 
    putbits36_15 (& w0, 0, cpu.TPR.TSR);
    // 15-16 BSY
    PNL (putbits36_1 (& w0, 15, (cpu.apu.state & apu_ESN_SNR) ? 1 : 0);)
    PNL (putbits36_1 (& w0, 16, (cpu.apu.state & apu_ESN_TSR) ? 1 : 0);)
    // 25 SDWAMM
    putbits36_1 (& w0, 25, cpu.cu.SDWAMM);
    // 26-29 SDWAMR
#ifdef WAM
    putbits36_4 (& w0, 26, cpu.SDWAMR);
#endif
    // 30 PTWAMM
    putbits36_1 (& w0, 30, cpu.cu.PTWAMM);
    // 31-34 PTWAMR
#ifdef WAM
    putbits36_4 (& w0, 31, cpu.PTWAMR);
#endif
    // 35 FLT
    PNL (putbits36_1 (& w0, 35, (cpu.apu.state & apu_FLT) ? 1 : 0);)

    // 36-59 ADD
    PNL (putbits36_24 (& w1,  0, cpu.APUMemAddr);)
    // 60-62 TRR
    putbits36_3 (& w1, 24, cpu.TPR.TRR);
    // 66 XXX Multiple match error in SDWAM
    // 70 Segment is encachable
    putbits36_1 (& w1, 34, cpu.SDW0.C);
    // 71 XXX Multiple match error in PTWAM

    addHist (APU_HIST_REG, w0, w1);
  }

#endif

