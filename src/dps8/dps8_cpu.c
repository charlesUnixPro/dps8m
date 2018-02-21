/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2018 by Charles Anthony
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
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_ins.h"
#include "dps8_loader.h"
#include "dps8_math.h"
#include "dps8_iefp.h"
#include "dps8_console.h"
#include "dps8_fnp2.h"
#include "dps8_crdrdr.h"
#include "dps8_absi.h"
#ifdef M_SHARED
#include "shm.h"
#endif
#include "dps8_opcodetable.h"
#include "sim_defs.h"
#if defined(THREADZ) || defined(LOCKLESS)
#include "threadz.h"
#if defined(LOCKLESS) && defined(__FreeBSD__)
#include <machine/atomic.h>
#endif

__thread uint current_running_cpu_idx;
#endif

#define DBG_CTR cpu.cycleCnt

// XXX Use this when we assume there is only a single cpu unit
#define ASSUME0 0

// CPU data structures

static UNIT cpu_unit [N_CPU_UNITS_MAX] =
  {
    [0 ... N_CPU_UNITS_MAX-1] =
      {
        UDATA (NULL, UNIT_FIX|UNIT_BINK, MEMSIZE), 
        0, 0, 0, 0, 0, NULL, NULL, NULL, NULL
     }
  };

#define UNIT_IDX(uptr) ((uptr) - cpu_unit)

static t_stat cpu_show_config (UNUSED FILE * st, UNIT * uptr, 
                               UNUSED int val, UNUSED const void * desc)
  {
    long cpu_unit_idx = UNIT_IDX (uptr);
    if (cpu_unit_idx < 0 || cpu_unit_idx >= N_CPU_UNITS_MAX)
      {
        sim_warn ("error: invalid unit number %ld\n", cpu_unit_idx);
        return SCPE_ARG;
      }

    sim_msg ("CPU unit number %ld\n", cpu_unit_idx);

    sim_msg ("Fault base:               %03o(8)\n",
                cpus[cpu_unit_idx].switches.FLT_BASE);
    sim_msg ("CPU number:               %01o(8)\n",
                cpus[cpu_unit_idx].switches.cpu_num);
    sim_msg ("Data switches:            %012"PRIo64"(8)\n",
                cpus[cpu_unit_idx].switches.data_switches);
    sim_msg ("Address switches:         %06o(8)\n",
                cpus[cpu_unit_idx].switches.addr_switches);
    for (int i = 0; i < N_CPU_PORTS; i ++)
      {
        sim_msg ("Port%c enable:             %01o(8)\n",
                    'A' + i, cpus[cpu_unit_idx].switches.enable [i]);
        sim_msg ("Port%c init enable:        %01o(8)\n",
                    'A' + i, cpus[cpu_unit_idx].switches.init_enable [i]);
        sim_msg ("Port%c assignment:         %01o(8)\n",
                    'A' + i, cpus[cpu_unit_idx].switches.assignment [i]);
        sim_msg ("Port%c interlace:          %01o(8)\n",
                    'A' + i, cpus[cpu_unit_idx].switches.assignment [i]);
        sim_msg ("Port%c store size:         %01o(8)\n",
                    'A' + i, cpus[cpu_unit_idx].switches.store_size [i]);
      }
    sim_msg ("Processor mode:           %s [%o]\n", 
                cpus[cpu_unit_idx].switches.proc_mode ? "Multics" : "GCOS",
                cpus[cpu_unit_idx].switches.proc_mode);
    sim_msg ("Processor speed:          %02o(8)\n", 
                cpus[cpu_unit_idx].switches.proc_speed);
    sim_msg ("DIS enable:               %01o(8)\n", 
                cpus[cpu_unit_idx].switches.dis_enable);
    sim_msg ("Steady clock:             %01o(8)\n", 
                scu [0].steady_clock);
    sim_msg ("Halt on unimplemented:    %01o(8)\n", 
                cpus[cpu_unit_idx].switches.halt_on_unimp);
    sim_msg ("Disable SDWAM/PTWAM:      %01o(8)\n", 
                cpus[cpu_unit_idx].switches.disable_wam);
    sim_msg ("Report faults:            %01o(8)\n", 
                cpus[cpu_unit_idx].switches.report_faults);
    sim_msg ("TRO faults enabled:       %01o(8)\n", 
                cpus[cpu_unit_idx].switches.tro_enable);
    sim_msg ("Y2K enabled:              %01o(8)\n", 
                scu [0].y2k);
    sim_msg ("drl fatal enabled:        %01o(8)\n", 
                cpus[cpu_unit_idx].switches.drl_fatal);
    sim_msg ("useMap:                   %d\n",
                cpus[cpu_unit_idx].switches.useMap);
    sim_msg ("Disable cache:            %01o(8)\n",
                cpus[cpu_unit_idx].switches.disable_cache);

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

static t_stat cpu_set_config (UNIT * uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc)
  {
// XXX Minor bug; this code doesn't check for trailing garbage

    long cpu_unit_idx = UNIT_IDX (uptr);
    if (cpu_unit_idx < 0 || cpu_unit_idx >= N_CPU_UNITS_MAX)
      {
        sim_warn ("error: cpu_set_config: invalid unit number %ld\n",
                    cpu_unit_idx);
        return SCPE_ARG;
      }

    static int port_num = 0;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse (__func__, cptr, cpu_config_list,
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
          cpus[cpu_unit_idx].switches.FLT_BASE = (uint) v;
        else if (strcmp (p, "num") == 0)
          cpus[cpu_unit_idx].switches.cpu_num = (uint) v;
        else if (strcmp (p, "data") == 0)
          cpus[cpu_unit_idx].switches.data_switches = (word36) v;
        else if (strcmp (p, "address") == 0)
          cpus[cpu_unit_idx].switches.addr_switches = (word18) v;
        else if (strcmp (p, "mode") == 0)
          cpus[cpu_unit_idx].switches.proc_mode = (uint) v;
        else if (strcmp (p, "speed") == 0)
          cpus[cpu_unit_idx].switches.proc_speed = (uint) v;
        else if (strcmp (p, "port") == 0)
          port_num = (int) v;
        else if (strcmp (p, "assignment") == 0)
          cpus[cpu_unit_idx].switches.assignment [port_num] = (uint) v;
        else if (strcmp (p, "interlace") == 0)
          cpus[cpu_unit_idx].switches.interlace [port_num] = (uint) v;
        else if (strcmp (p, "enable") == 0)
          cpus[cpu_unit_idx].switches.enable [port_num] = (uint) v;
        else if (strcmp (p, "init_enable") == 0)
          cpus[cpu_unit_idx].switches.init_enable [port_num] = (uint) v;
        else if (strcmp (p, "store_size") == 0)
          cpus[cpu_unit_idx].switches.store_size [port_num] = (uint) v;
        else if (strcmp (p, "dis_enable") == 0)
          cpus[cpu_unit_idx].switches.dis_enable = (uint) v;
        else if (strcmp (p, "steady_clock") == 0)
          scu [0].steady_clock = (uint) v;
        else if (strcmp (p, "halt_on_unimplemented") == 0)
          cpus[cpu_unit_idx].switches.halt_on_unimp = (uint) v;
        else if (strcmp (p, "disable_wam") == 0)
          cpus[cpu_unit_idx].switches.disable_wam = (uint) v;
        else if (strcmp (p, "report_faults") == 0)
          cpus[cpu_unit_idx].switches.report_faults = (uint) v;
        else if (strcmp (p, "tro_enable") == 0)
          cpus[cpu_unit_idx].switches.tro_enable = (uint) v;
        else if (strcmp (p, "y2k") == 0)
          scu [0].y2k = (uint) v;
        else if (strcmp (p, "drl_fatal") == 0)
          cpus[cpu_unit_idx].switches.drl_fatal = (uint) v;
        else if (strcmp (p, "useMap") == 0)
          cpus[cpu_unit_idx].switches.useMap = v;
        else if (strcmp (p, "disable_cache") == 0)
          cpus[cpu_unit_idx].switches.disable_cache = v;
        else
          {
            sim_warn ("error: cpu_set_config: invalid cfgparse rc <%d>\n",
                        rc);
            cfgparse_done (& cfg_state);
            return SCPE_ARG; 
          }
      } // process statements
    cfgparse_done (& cfg_state);

    return SCPE_OK;
  }

static t_stat cpu_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                               UNUSED int val, UNUSED const void * desc)
  {
    sim_msg ("Number of CPUs in system is %d\n", cpu_dev.numunits);
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

static char * cycle_str (cycles_e cycle)
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

static void set_cpu_cycle (cycles_e cycle)
  {
    sim_debug (DBG_CYCLE, & cpu_dev, "Setting cycle to %s\n",
               cycle_str (cycle));
    cpu.cycle = cycle;
  }

// DPS8M Memory of 36 bit words is implemented as an array of 64 bit words.
// Put state information into the unused high order bits.
#define MEM_UNINITIALIZED (1LLU<<62)
#ifdef LOCKLESS
#define MEM_LOCKED_BIT    61
#define MEM_LOCKED        (1LLU<<MEM_LOCKED_BIT)
#endif

uint set_cpu_idx (UNUSED uint cpu_idx)
  {
    uint prev = current_running_cpu_idx;
#if defined(THREADZ) || defined(LOCKLESS)
    current_running_cpu_idx = cpu_idx;
#endif
#ifdef ROUND_ROBIN
    current_running_cpu_idx = cpu_idx;
#endif
    cpup = & cpus [current_running_cpu_idx];
    return prev;
  }

static void cpu_reset_unit_idx (UNUSED uint cpun, bool clear_mem)
  {
    uint save = set_cpu_idx (cpun);
    if (clear_mem)
      {
#ifdef SCUMEM
        for (int cpu_port_num = 0; cpu_port_num < N_CPU_PORTS; cpu_port_num ++)
          {
            if (get_scu_in_use (current_running_cpu_idx, cpu_port_num))
              {
                uint sci_unit_idx = get_scu_idx (current_running_cpu_idx, cpu_port_num);
                for (uint i = 0; i < SCU_MEM_SIZE; i ++)
                  scu [sci_unit_idx].M[i] = MEM_UNINITIALIZED;
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
//#if defined(THREADZ) || defined(LOCKLESS)
//    clock_gettime (CLOCK_BOOTTIME, & cpu.rTRTime);
//#endif
#if ISOLTS
    cpu.shadowTR = 0;
    cpu.rTRlsb = 0;
#endif
    cpu.rTR = MASK27;
    cpu.rTRticks = 0;
 
    set_addr_mode (ABSOLUTE_mode);
    SET_I_NBAR;
    
    cpu.CMR.luf = 3;    // default of 16 mS
    cpu.cu.SD_ON = cpu.switches.disable_wam ? 0 : 1;
    cpu.cu.PT_ON = cpu.switches.disable_wam ? 0 : 1;
 
    set_cpu_cycle (FETCH_cycle);

    cpu.wasXfer = false;
    cpu.wasInhibited = false;

    cpu.interrupt_flag = false;
    cpu.g7_flag = false;

    cpu.faultRegister [0] = 0;
    cpu.faultRegister [1] = 0;

#ifdef RAPRx
    cpu.apu.lastCycle = UNKNOWN_CYCLE;
#endif

    memset (& cpu.PPR, 0, sizeof (struct ppr_s));

    setup_scbank_map ();

    tidy_cu ();
    set_cpu_idx (save);
#ifdef TEST_OLIN
          cmpxchg ();
#endif
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
    fence ();
#endif
  }

static t_stat simh_cpu_reset_and_clear_unit (UNUSED UNIT * uptr, 
                                             UNUSED int32 value,
                                             UNUSED const char * cptr, 
                                             UNUSED void * desc)
  {
    long cpu_unit_idx = UNIT_IDX (uptr);
#ifdef ISOLTS
    cpu_state_t * cpun = cpus + cpu_unit_idx;
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
    cpu_reset_unit_idx ((uint) cpu_unit_idx, true);
#endif
    return SCPE_OK;
  }

static t_stat simh_cpu_reset_unit (UNIT * uptr, 
                                   UNUSED int32 value,
                                   UNUSED const char * cptr, 
                                   UNUSED void * desc)
  {
    long cpu_unit_idx = UNIT_IDX (uptr);
    cpu_reset_unit_idx ((uint) cpu_unit_idx, false); // no clear memory
    return SCPE_OK;
  }

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
      simh_cpu_reset_unit,       // validation routine
      NULL,                      // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },

    {
      MTAB_unit_value,           // mask
      0,                         // match
      "INITIALIZE",              // print string
      "INITIALIZE",              // match string
      simh_cpu_reset_unit,       // validation routine
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
      simh_cpu_reset_and_clear_unit,  // validation routine
      NULL,                      // display routine
      NULL,                      // value descriptor
      NULL                       // help
    },

    {
      MTAB_unit_value,           // mask
      0,                         // match 
      "IAC",                     // print string
      "IAC",                     // match string
      simh_cpu_reset_and_clear_unit,  // validation routine
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
    "Simulation stop",         // STOP_STOP
    "Breakpoint",              // STOP_BKPT
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

static bool watch_bits [MEMSIZE];

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



char * str_SDW0 (char * buf, sdw0_s * SDW)
  {
    sprintf (buf, "ADDR=%06o R1=%o R2=%o R3=%o F=%o FC=%o BOUND=%o R=%o "
             "E=%o W=%o P=%o U=%o G=%o C=%o EB=%o",
             SDW->ADDR, SDW->R1, SDW->R2, SDW->R3, SDW->DF,
             SDW->FC, SDW->BOUND, SDW->R, SDW->E, SDW->W,
             SDW->P, SDW->U, SDW->G, SDW->C, SDW->EB);
    return buf;
  }

static t_stat cpu_boot (UNUSED int32 cpu_unit_idx, UNUSED DEVICE * dptr)
  {
    sim_warn ("Try 'BOOT IOMn'\n");
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
        // This will happen during SIMH early initialization before
        // the cables are run.
        if (! cables->cpu_to_scu[current_running_cpu_idx][port_num].in_use)
          {
            //sim_warn ("%s SCU not cabled\n", __func__);
            continue;
          }
        uint scu_unit_idx = cables->cpu_to_scu[current_running_cpu_idx][port_num].scu_unit_idx;

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
                    sim_warn ("scbank overlap scpg %d (%o) old port %d "
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
                sim_warn ("scpg too big port %d scpg %d (%o), "
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

static void get_serial_number (void)
  {
    bool havesn = false;
    FILE * fp = fopen ("./serial.txt", "r");
    while (fp && ! feof (fp))
      {
        char buffer [81] = "";
        fgets (buffer, sizeof (buffer), fp);
        uint cpun, sn;
        if (sscanf (buffer, "sn: %u", & cpu.switches.serno) == 1)
          {
            sim_msg ("Serial number is %u\n", cpu.switches.serno);
            havesn = true;
          }
        else if (sscanf (buffer, "sn%u: %u", & cpun, & sn) == 2)
          {
            if (cpun < N_CPU_UNITS_MAX)
              {
                cpus[cpun].switches.serno = sn;
                sim_msg ("Serial number of CPU %u is %u\n",
                            cpun, cpus[cpun].switches.serno);
                havesn = true;
              }
          }
      }
    if (! havesn)
      {
        sim_msg ("Please register your system at "
                    "https://ringzero.wikidot.com/wiki:register\n");
        sim_msg ("or create the file 'serial.txt' containing the line "
                    "'sn: 0'.\n");
      }
    if (fp)
      fclose (fp);
  }


#ifdef STATS
static void do_stats (void)
  {
    static struct timespec stats_time;
    static bool first = true;
    if (first)
      {
        first = false;
        clock_gettime (CLOCK_BOOTTIME, & stats_time);
        sim_msg ("stats started\r\n");
      }
    else
      {
        struct timespec now, delta;
        clock_gettime (CLOCK_BOOTTIME, & now);
        timespec_diff (& stats_time, & now, & delta);
        stats_time = now;
        sim_msg ("stats %6ld.%02ld\r\n", delta.tv_sec,
                    delta.tv_nsec / 10000000);

        sim_msg ("Instruction counts\r\n");
        for (uint i = 0; i < 8; i ++)
          {
            sim_msg (" %9lld", cpus[i].instrCnt);
            cpus[i].instrCnt = 0;
          }
        sim_msg ("\r\n");

        sim_msg ("\r\n");
      }
  }
#endif

#ifndef NO_EV_POLL
// The 100Hz timer as expired; poll I/O

static void ev_poll_cb (uv_timer_t * UNUSED handle)
  {
    // Call the one hertz stuff every 100 loops
    static uint oneHz = 0;
    if (oneHz ++ >= sys_opts.sys_slow_poll_interval) // ~ 1Hz
      {
        oneHz = 0;
        rdrProcessEvent (); 
#ifdef STATS
        do_stats ();
#endif
        cpu.instrCntT0 = cpu.instrCntT1;
        cpu.instrCntT1 = cpu.instrCnt;

      }
    fnpProcessEvent (); 
    consoleProcess ();
    machine_room_process ();
#ifndef __MINGW64__
    absiProcessEvent ();
#endif
    PNL (panel_process_event ());
  }
#endif

    
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
        sim_fatal ("create M failed\n");
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
        sim_fatal ("create cpus failed\n");
      }
#endif

    memset (& watch_bits, 0, sizeof (watch_bits));

    set_cpu_idx (0);

    memset (cpus, 0, sizeof (cpu_state_t) * N_CPU_UNITS_MAX);
    cpus [0].switches.FLT_BASE = 2; // Some of the UnitTests assume this

    get_serial_number ();

#ifndef NO_EV_POLL
    ev_poll_loop = uv_default_loop ();
    uv_timer_init (ev_poll_loop, & ev_poll_handle);
    // 10 ms == 100Hz
    uv_timer_start (& ev_poll_handle, ev_poll_cb, sys_opts.sys_poll_interval, sys_opts.sys_poll_interval);
#endif

    // TODO: reset *all* other structures to zero
    
    cpu.instrCnt = 0;
    cpu.cycleCnt = 0;
    for (int i = 0; i < N_FAULTS; i ++)
      cpu.faultCnt [i] = 0;
    
    
#ifdef MATRIX
    initializeTheMatrix ();
#endif
  }

static void cpu_reset (void)
  {
    for (uint i = 0; i < N_CPU_UNITS_MAX; i ++)
      {
        cpu_reset_unit_idx (i, true);
      }

    set_cpu_idx (0);

    sim_debug (DBG_INFO, & cpu_dev, "CPU reset: Running\n");

  }

static t_stat sim_cpu_reset (UNUSED DEVICE *dptr)
  {
    //memset (M, -1, MEMSIZE * sizeof (word36));

    // Fill DPS8M memory with zeros, plus a flag only visible to the emulator
    // marking the memory as uninitialized.


    cpu_reset ();
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
static word18 dummy_IC;
#endif

static REG cpu_reg[] =
  {
    // IC must be the first; see sim_PC.
#ifdef M_SHARED
    { ORDATA (IC, dummy_IC, VASIZE), 0, 0, 0 }, 
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
    & cpu_ex,       // examine routine
    & cpu_dep,      // deposit routine
    & sim_cpu_reset,// reset routine
    & cpu_boot,     // boot routine
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
#if defined(THREADZ) || defined(LOCKLESS)
__thread cpu_state_t * restrict cpup;
#else
cpu_state_t * restrict cpup; 
#endif
#ifdef ROUND_ROBIN
uint current_running_cpu_idx;
#endif

// Scan the SCUs; it one has an interrupt present, return the fault pair
// address for the highest numbered interrupt on that SCU. If no interrupts
// are found, return 1.
 
// Called with SCU lock set

static uint get_highest_intr (void)
  {
    uint fp = 1;
    for (uint scu_unit_idx = 0; scu_unit_idx < N_SCU_UNITS_MAX; scu_unit_idx ++)
      {
        if (cpu.events.XIP [scu_unit_idx])
          {
            fp = scu_get_highest_intr (scu_unit_idx); // CALLED WITH SCU LOCK
            if (fp != 1)
              break;
          }
      }
    return fp;
  }

bool sample_interrupts (void)
  {
    cpu.lufCounter = 0;
    for (uint scu_unit_idx = 0; scu_unit_idx < N_SCU_UNITS_MAX; scu_unit_idx ++)
      {
        if (cpu.events.XIP [scu_unit_idx])
          {
            return true;
          }
      }
    return false;
  }

t_stat simh_hooks (void)
  {
    int reason = 0;

    if (breakEnable && stop_cpu)
      return STOP_STOP;

#ifdef ISOLTS
    if (current_running_cpu_idx == 0)
#endif
    // check clock queue 
    if (sim_interval <= 0)
      {
        reason = sim_process_event ();
        if ((! breakEnable) && reason == SCPE_STOP)
          reason = SCPE_OK;
        if (reason)
          return reason;
      }
        
    sim_interval --;

#if !defined(THREADZ) && !defined(LOCKLESS)
// This is needed for BCE_TRAP in install scripts
    // sim_brk_test expects a 32 bit address; PPR.IC into the low 18, and
    // PPR.PSR into the high 12
    if (sim_brk_summ &&
        sim_brk_test ((cpu.PPR.IC & 0777777) |
                      ((((t_addr) cpu.PPR.PSR) & 037777) << 18),
                      SWMASK ('E')))  /* breakpoint? */
      return STOP_BKPT; /* stop simulation */
#ifndef SPEED
    if (sim_deb_break && cpu.cycleCnt >= sim_deb_break)
      return STOP_BKPT; /* stop simulation */
#endif
#endif

    return reason;
  }       


#ifdef PANEL
static void panel_process_event (void)
  {
    // INITIALIZE pressed; treat at as a BOOT.
    if (cpu.panelInitialize && cpu.DATA_panel_s_trig_sw == 0) 
      {
         // Wait for release
         while (cpu.panelInitialize) 
           ;
         if (cpu.DATA_panel_init_sw)
           cpu_reset_unit_idx (ASSUME0, true); // INITIALIZE & CLEAR
         else
           cpu_reset_unit_idx (ASSUME0, false); // INITIALIZE
         // XXX Until a boot switch is wired up
         do_boot ();
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
            cpu_reset_unit_idx (ASSUME0, false);
            cpu.cu.IWB = cpu.switches.data_switches;
            set_cpu_cycle (EXEC_cycle);
          }
         else // EXECUTE FAULT
          {
            setG7fault (current_running_cpu_idx, FAULT_EXF, fst_zero);
          }
      }
  }
#endif


#if defined(THREADZ) || defined(LOCKLESS)
// The hypervisor CPU for the threadz model
t_stat sim_instr (void)
  {
    t_stat reason = 0;

    static bool inited = false;
    if (! inited)
      {
        inited = true;

#ifdef IO_THREADZ
// Create channel threads

        for (uint iom_unit_idx = 0; iom_unit_idx < N_IOM_UNITS_MAX; iom_unit_idx ++)
          {
            for (uint chan_num = 0; chan_num < MAX_CHANNELS; chan_num ++)
              {
                if (get_ctlr_in_use (iom_unit_idx, chan_num))
                  {
                    enum ctlr_type_e ctlr_type = 
                      cables->iom_to_ctlr[iom_unit_idx][chan_num].ctlr_type;
                    createChnThread (iom_unit_idx, chan_num,
                                     ctlr_type_strs [ctlr_type]);
                    chnRdyWait (iom_unit_idx, chan_num);
                  }
              }
          }

// Create IOM threads

        for (uint iom_unit_idx = 0;
             iom_unit_idx < N_IOM_UNITS_MAX;
             iom_unit_idx ++)
          {
            createIOMThread (iom_unit_idx);
            iomRdyWait (iom_unit_idx);
          }
#endif

// Create CPU threads

        //for (uint cpu_idx = 0; cpu_idx < N_CPU_UNITS_MAX; cpu_idx ++)
        for (uint cpu_idx = 0; cpu_idx < cpu_dev.numunits; cpu_idx ++)
          {
            createCPUThread (cpu_idx);
          }
      }

    do
      {
        reason = 0;
        // Process deferred events and breakpoints
        reason = simh_hooks ();
        if (reason)
          {
            break;
          }

#if 0
// Check for CPU 0 stopped
        if (! cpuThreadz[0].run)
          {
            sim_msg ("CPU 0 stopped\n");
            return STOP_STOP;
          }
#endif
#if 1
// Check for all CPUs stopped

        uint n_running = 0;
        for (uint i = 0; i < cpu_dev.numunits; i ++)
          {
            struct cpuThreadz_t * p = & cpuThreadz[i];
            if (p->run)
              n_running ++;
          }
        if (! n_running)
          return STOP_STOP;
#endif

// Loop runs at 1000Hhz

        lock_libuv ();
        uv_run (ev_poll_loop, UV_RUN_NOWAIT);
        unlock_libuv ();
        PNL (panel_process_event ());

        int con_unit_idx = check_attn_key ();
        if (con_unit_idx != -1)
          console_attn_idx (con_unit_idx);

        usleep (1000); // 1000 us == 1 ms == 1/1000 sec.
      }
    while (reason == 0);
#ifdef HDBG
    hdbgPrint ();
#endif
    return reason;
  }
#endif

#if !defined(THREADZ) && !defined(LOCKLESS)
#ifndef NO_EV_POLL
static uint fast_queue_subsample = 0;
#endif
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

#if defined(THREADZ) || defined(LOCKLESS)
void * cpu_thread_main (void * arg)
  {
    int myid = * (int *) arg;
    set_cpu_idx ((uint) myid);
    
    sim_msg ("CPU %c thread created\n", 'a' + myid);

    setSignals ();
    threadz_sim_instr ();
    return NULL;
  }
#endif // THREADZ

static void do_LUF_fault (void)
  {
    CPT (cpt1U, 16); // LUF
    cpu.lufCounter = 0;
    cpu.lufOccurred = false;
#ifdef ISOLTS
// This is a hack to fix ISOLTS 776. ISOLTS checks that the TR has
// decremented by the LUF timeout value. To implement this, we set
// the TR to the expected value.

// LUF  time
//  0    2ms
//  1    4ms
//  2    8ms
//  3   16ms
// units
// you have: 2ms
// units
// You have: 512000Hz
// You want: 1/2ms
//    * 1024
//    / 0.0009765625
//
//  TR = 1024 << LUF
    cpu.shadowTR = (word27) cpu.TR0 - (1024u << (is_priv_mode () ? 4 : cpu.CMR.luf));


// That logic fails for test 785. 
//
// set slave mode, LUF time 16ms.
// loop for 15.9 ms.
// set master mode.
// loop for 15.9 ms. The LUF should be noticed, and lufOccurred set.
// return to slave mode. The LUF should fire, with the timer register
// being set for 31.1 ms. 
// With out accurate cycle timing or simply fudging the results, I don't
// see how to fix this one.

#endif
    doFault (FAULT_LUF, fst_zero, "instruction cycle lockup");
  }

#if !defined(THREADZ) && !defined(LOCKLESS)
#define threadz_sim_instr sim_instr
#endif

/*
 * addr_modes_e get_addr_mode()
 *
 * Report what mode the CPU is in.
 * This is determined by examining a couple of IR flags.
 *
 * TODO: get_addr_mode() probably belongs in the CPU source file.
 *
 */

static void set_temporary_absolute_mode (void)
  {
    CPT (cpt1L, 20); // set temp. abs. mode
    cpu.secret_addressing_mode = true;
    cpu.cu.XSF = false;
sim_debug (DBG_TRACEEXT, & cpu_dev, "set_temporary_absolute_mode bit 29 sets XSF to 0\n");
    //cpu.went_appending = false;
  }

static bool clear_temporary_absolute_mode (void)
  {
    CPT (cpt1L, 21); // clear temp. abs. mode
    cpu.secret_addressing_mode = false;
    return cpu.cu.XSF;
    //return cpu.went_appending;
  }

t_stat threadz_sim_instr (void)
  {
//cpu.have_tst_lock = false;

    t_stat reason = 0;
      
#if !defined(THREADZ) && !defined(LOCKLESS)
    set_cpu_idx (0);
#ifdef M_SHARED
// simh needs to have the IC statically allocated, so a placeholder was
// created. Copy the placeholder in so the IC can be set by simh.

    cpus [0].PPR.IC = dummy_IC;
#endif

#ifdef ROUND_ROBIN
    cpu.isRunning = true;
    set_cpu_idx (cpu_dev.numunits - 1);

setCPU:;
    uint current = current_running_cpu_idx;
    uint c;
    for (c = 0; c < cpu_dev.numunits; c ++)
      {
        set_cpu_idx (c);
        if (cpu.isRunning)
          break;
      }
    if (c == cpu_dev.numunits)
      {
        sim_msg ("All CPUs stopped\n");
        goto leave;
      }
    set_cpu_idx ((current + 1) % cpu_dev.numunits);
    if (! cpu . isRunning)
      goto setCPU;
#endif
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
            set_cpu_cycle (SYNC_FAULT_RTN_cycle);
            break;
        case JMP_STOP:
            reason = STOP_STOP;
            goto leave;
        case JMP_REFETCH:

            // Not necessarily so, but the only times
            // this path is taken is after an RCU returning
            // from an interrupt, which could only happen if
            // was xfer was false; or in a DIS cycle, in
            // which case we want it false so interrupts 
            // can happen.
            cpu.wasXfer = false;
             
            set_cpu_cycle (FETCH_cycle);
            break;
        case JMP_RESTART:
            set_cpu_cycle (EXEC_cycle);
            break;
        default:
          sim_warn ("longjmp value of %d unhandled\n", val);
            goto leave;
      }

    // Main instruction fetch/decode loop 

    DCDstruct * ci = & cpu.currentInstruction;

    do
      {
        reason = 0;

#if !defined(THREADZ) && !defined(LOCKLESS)
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

        //if ((! cpu.wasInhibited) && fast_queue_subsample ++ > 1024) // ~ 1KHz
        //static uint fastQueueSubsample = 0;
        if (fast_queue_subsample ++ > sys_opts.sys_poll_check_rate) // ~ 1KHz
          {
            fast_queue_subsample = 0;
            uv_run (ev_poll_loop, UV_RUN_NOWAIT);
            PNL (panel_process_event ());
          }
#else
        static uint slowQueueSubsample = 0;
        if (slowQueueSubsample ++ > 1024000) // ~ 1Hz
          {
            slowQueueSubsample = 0;
            rdrProcessEvent ();
            cpu.instrCntT0 = cpu.instrCntT1;
            cpu.instrCntT1 = cpu.instrCnt;
          }
        static uint queueSubsample = 0;
        if (queueSubsample ++ > 10240) // ~ 100Hz
          {
            queueSubsample = 0;
            fnpProcessEvent ();
            consoleProcess ();
            machine_room_process ();
            absiProcessEvent ();
            PNL (panel_process_event ());
          }
#endif
        cpu.cycleCnt ++;
#endif // ! THREADZ

#ifdef TEST_OLIN
          cmpxchg ();
#endif
#ifdef TEST_FENCE
    fence ();
#endif
#ifdef THREADZ
        // If we faulted somewhere with the memory lock set, clear it.
        unlock_mem_force ();

        // wait on run/switch
        cpuRunningWait ();

#endif // THREADZ
#ifdef LOCKLESS
	core_unlock_all();
	// wait on run/switch
        cpuRunningWait ();
#endif

        int con_unit_idx = check_attn_key ();
        if (con_unit_idx != -1)
          console_attn_idx (con_unit_idx);

#ifndef NO_EV_POLL
#if !defined(THREADZ) && !defined(LOCKLESS)
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
                      setG7fault (current_running_cpu_idx, FAULT_TRO, fst_zero);
                  }
              }
          }
#endif
#endif
#endif

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
            setG7fault (current_running_cpu_idx, FAULT_TRO, fst_zero);
          }

        sim_debug (DBG_CYCLE, & cpu_dev, "Cycle is %s\n",
                   cycle_str (cpu.cycle));

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
#ifdef HDBG
                hdbgIntr (intr_pair_addr);
#endif
                cpu.cu.FI_ADDR = (word5) (intr_pair_addr / 2);
                cu_safe_store ();
                // XXX the whole interrupt cycle should be rewritten as an xed
                // instruction pushed to IWB and executed 

                CPT (cpt1U, 1); // safe store complete
                // Temporary absolute mode
                set_temporary_absolute_mode ();

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
                        set_cpu_cycle (INTERRUPT_EXEC_cycle);
                        break;
                      } // int_pair != 1
                  } // interrupt_flag

                // If we get here, there was no interrupt

                CPT (cpt1U, 5); // interrupt pair spurious
                cpu.interrupt_flag = false;
                clear_temporary_absolute_mode ();
                // Restores addressing mode 
                cu_safe_restore ();
                // We can only get here if wasXfer was
                // false, so we can assume it still is.
                cpu.wasXfer = false;
// The only place cycle is set to INTERRUPT_cycle in FETCH_cycle; therefore
// we can safely assume that is the state that should be restored.
                set_cpu_cycle (FETCH_cycle);
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
                    get_BAR_address (cpu.PPR.IC);

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
                    set_cpu_cycle (INTERRUPT_cycle);
                    break;
                  }

// "While in absolute mode or privileged mode the lockup fault is signalled at
// the end of the time limit set in the lockup timer but is not recognized
// until the 32 millisecond limit. If the processor returns to normal mode or
// BAR mode after the fault has been signalled but before the 32 millisecond
// limit, the fault is recognized before any instruction in the new mode is
// executed."

                cpu.lufCounter ++;
#if 1
                if (cpu.lufCounter > luf_limits[cpu.CMR.luf])
                  {
                    if (tmp_priv_mode)
                      {
                        // In priv. mode the LUF is noted but not executed
                        cpu.lufOccurred = true;
                      }
                    else
                      {
                        do_LUF_fault ();
                      }
                  } // lufCounter > luf_limit


                // After 32ms, the LUF fires regardless of priv.
                if (cpu.lufCounter > luf_limits[4])
                  {
                    do_LUF_fault ();
                  }

                // If the LUF occured in priv. mode and we left priv. mode,
                // fault.
                if (! tmp_priv_mode && cpu.lufOccurred)
                  {
                    do_LUF_fault ();
                  }
#else
                if ((tmp_priv_mode && cpu.lufCounter > luf_limits[4]) || 
                    (! tmp_priv_mode && 
                     cpu.lufCounter > luf_limits[cpu.CMR.luf]))
                  {
                    CPT (cpt1U, 16); // LUF
                    cpu.lufCounter = 0;
#ifdef ISOLTS
// This is a hack to fix ISOLTS 776. ISOLTS checks that the TR has
// decremented by the LUF timeout value. To implement this, we set
// the TR to the expected value.

// LUF  time
//  0    2ms
//  1    4ms
//  2    8ms
//  3   16ms
// units
// you have: 2ms
// units
// You have: 512000Hz
// You want: 1/2ms
//    * 1024
//    / 0.0009765625
//
//  TR = 1024 << LUF
                   cpu.shadowTR = (word27) cpu.TR0 - (1024u << (is_priv_mode () ? 4 : cpu.CMR.luf));
#endif
                    doFault (FAULT_LUF, fst_zero, "instruction cycle lockup");
                  }
#endif
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
                    //clr_went_appending (); // XXX not sure this is the right
                                           //  place
                    cpu.cu.XSF = 0;
sim_debug (DBG_TRACEEXT, & cpu_dev, "fetchCycle bit 29 sets XSF to 0\n");
                    cpu.cu.TSN_VALID [0] = 0;
                    PNL (cpu.prepare_state = ps_PIA);
                    PNL (L68_ (cpu.INS_FETCH = true;))
                    fetchInstruction (cpu.PPR.IC);
                  }

                CPT (cpt1U, 21); // go to exec cycle
                advanceG7Faults ();
                set_cpu_cycle (EXEC_cycle);
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

                add_CU_history ();

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
                      set_cpu_cycle (FETCH_cycle);
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

                        if (!clear_temporary_absolute_mode ())
                          {
                            // didn't go appending
                            sim_debug (DBG_TRACEEXT, & cpu_dev,
                                       "setting ABS mode\n");
                            CPT (cpt1U, 10); // temporary absolute mode
                            set_addr_mode (ABSOLUTE_mode);
                          }
                        else
                          {
                            // went appending
                            sim_debug (DBG_TRACEEXT, & cpu_dev,
                                       "not setting ABS mode\n");
                          }

                      } // fault or interrupt


                    //if (TST_I_ABS && get_went_appending ())
                    if (TST_I_ABS && cpu.cu.XSF)
                      {
                        set_addr_mode (APPEND_mode);
                      }

                    set_cpu_cycle (FETCH_cycle);
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
#if defined(THREADZ) || defined(LOCKLESS)

// XXX If interupt inhibit set, then sleep forever instead of TRO
                    // rTR is 512KHz; sleepCPU is in 1Mhz
                    //   rTR * 1,000,000 / 512,000
                    //   rTR * 1000 / 512
                    //   rTR * 500 / 256
                    //   rTR * 250 / 128
                    //   rTR * 125 / 64

#ifdef NO_TIMEWAIT
                    //usleep (sys_opts.sys_poll_interval * 1000/*10000*/);
                    struct timespec req, rem;
                    uint ms = sys_opts.sys_poll_interval;
                    long int nsec = (long int) ms * 1000 * 1000;
                    req.tv_nsec = nsec;
                    req.tv_sec += req.tv_nsec / 1000000000;
                    req.tv_nsec %= 1000000000;
                    int rc = nanosleep (& req, & rem);
                    // Awakened early?
                    if (rc == -1)
                      {
                         ms = (uint) (rem.tv_nsec / 1000 + req.tv_sec * 1000);
                      }
                    word27 ticks = ms * 512;
                    if (cpu.rTR <= ticks)
                      {
                        if (cpu.switches.tro_enable)
                          setG7fault (current_running_cpu_idx, FAULT_TRO,
                                      fst_zero);
                      }
                    cpu.rTR = (cpu.rTR - ticks) & MASK27;
#else // !NO_TIMEWAIT
		    unsigned long left = cpu.rTR * 125u / 64u;
		    lock_scu();
		    if (!sample_interrupts()) {
		        left = sleepCPU (left);
		    }
		    unlock_scu();
                    if (left)
                      {
                        cpu.rTR = (word27) (left * 64 / 125);
                      }
                    else
                      {
                        if (cpu.switches.tro_enable)
                          setG7fault (current_running_cpu_idx, FAULT_TRO,
                                      fst_zero);
                        cpu.rTR = 0;
                      }
#endif // !NO_TIMEWAIT
                    cpu.rTRticks = 0;
                    break;
#else // !THREADZ
                    //usleep (10000);
                    usleep (sys_opts.sys_poll_interval * 1000/*10000*/);
#ifndef NO_EV_POLL
                    // Trigger I/O polling
                    uv_run (ev_poll_loop, UV_RUN_NOWAIT);
                    fast_queue_subsample = 0;
#else // NO_EV_POLL
                    // this ignores the amount of time since the last poll;
                    // worst case is the poll delay of 1/50th of a second.
                    slowQueueSubsample += 10240; // ~ 1Hz
                    queueSubsample += 10240; // ~100Hz
#endif // NO_EV_POLL

                    sim_interval = 0;
#endif // !THREADZ
                    // Timer register runs at 512 KHz
                    // 512000 is 1 second
                    // 512000/100 -> 5120  is .01 second
         
                    cpu.rTRticks = 0;
                    // Would we have underflowed while sleeping?
                    //if ((cpu.rTR & ~ MASK27) || cpu.rTR <= 5120)
                    //if (cpu.rTR <= 5120)

                    // Timer register runs at 512 KHz
                    // 512Khz / 512 is millisecods
                    if (cpu.rTR <= sys_opts.sys_poll_interval * 512)
                    
                      {
                        if (cpu.switches.tro_enable)
                          setG7fault (current_running_cpu_idx, FAULT_TRO,
                                      fst_zero);
                      }
                    cpu.rTR = (cpu.rTR - sys_opts.sys_poll_interval * 512) & MASK27;
#endif // ! ROUND_ROBIN
                    break;
                  }

                cpu.wasXfer = false;

                if (ret < 0)
                  {
                    sim_warn ("executeInstruction returned %d?\n", ret);
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
                    set_cpu_cycle (FETCH_cycle);
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
                    clear_temporary_absolute_mode ();
                    cu_safe_restore ();
                    cpu.wasXfer = false;
                    CPT (cpt1U, 12); // cu restored
                    set_cpu_cycle (FETCH_cycle);
                    clearFaultCycle ();
                    // cu_safe_restore should have restored CU.IWB, so
                    // we can determine the instruction length.
                    // decode_instruction() restores ci->info->ndes
                    decode_instruction (IWB_IRODD, & cpu.currentInstruction);

                    cpu.PPR.IC += ci->info->ndes;
                    cpu.PPR.IC ++;
                    break;
                  }

// If we just did the odd word of a interrupt pair

                if (cpu.cycle == INTERRUPT_EXEC_cycle &&
                    (! cpu.cu.xde) &&
                    cpu.cu.xdo)
                  {
                    clear_temporary_absolute_mode ();
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
                    set_cpu_cycle (FETCH_cycle);
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
                set_cpu_cycle (FETCH_cycle);
              }
              break;

            case SYNC_FAULT_RTN_cycle:
              {
                CPT (cpt1U, 29); // sync. fault return
                cpu.wasXfer = false; 
                // cu_safe_restore should have restored CU.IWB, so
                // we can determine the instruction length.
                // decode_instruction() restores ci->info->ndes
                decode_instruction (IWB_IRODD, & cpu.currentInstruction);

                cpu.PPR.IC += ci->info->ndes;
                cpu.PPR.IC ++;
                cpu.wasXfer = false; 
                set_cpu_cycle (FETCH_cycle);
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
                set_temporary_absolute_mode ();

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
                set_cpu_cycle (FAULT_EXEC_cycle);

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

#if defined(THREADZ) || defined(LOCKLESS)
    setCPURun (current_running_cpu_idx, false);
#endif

#ifdef HDBG
    hdbgPrint ();
#endif
    sim_msg ("\ncycles = %llu\n", cpu.cycleCnt);
    sim_msg ("\ninstructions = %llu\n", cpu.instrCnt);

#if 0
    for (int i = 0; i < N_FAULTS; i ++)
      {
        if (cpu.faultCnt [i])
          sim_msg  ("%s faults = %ld\n",
                      faultNames [i], cpu.faultCnt [i]);
      }
#endif

#ifdef M_SHARED
// simh needs to have the IC statically allocated, so a placeholder was
// created. Update the placeholder in so the IC can be seen by simh, and
// restarting sim_instr doesn't lose the place.

    set_cpu_idx (0);
    dummy_IC = cpu.PPR.IC;
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

int operand_size (void)
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

t_stat read_operand (word18 addr, _processor_cycle_type cyctyp)
  {
    CPT (cpt1L, 6); // read_operand

#ifdef THREADZ
    if (cyctyp == OPERAND_READ)
      {
        DCDstruct * i = & cpu.currentInstruction;
#if 1
        if (RMWOP (i))
#else
        if ((i -> opcode == 0034 && ! i -> opcodeX) ||  // ldac
            (i -> opcode == 0032 && ! i -> opcodeX) ||  // ldqc
            (i -> opcode == 0354 && ! i -> opcodeX) ||  // stac
            (i -> opcode == 0654 && ! i -> opcodeX) ||  // stacq
            (i -> opcode == 0214 && ! i -> opcodeX))    // sznc
#endif
          {
            lock_rmw ();
          }
      }
#endif

    switch (operand_size ())
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

t_stat write_operand (word18 addr, UNUSED _processor_cycle_type cyctyp)
  {
    switch (operand_size ())
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
            //for (uint j = 0 ; j < 32 ; j += 1)
                //Write (addr + j, cpu.Yblock32[j], OPERAND_STORE);
            Write32 (addr, cpu.Yblock32);
            break;
      }
    
#ifdef THREADZ
    if (cyctyp == OPERAND_STORE)
      {
        DCDstruct * i = & cpu.currentInstruction;
        if (RMWOP (i))
          unlock_mem ();
      }
#endif
    return SCPE_OK;
    
  }

t_stat set_mem_watch (int32 arg, const char * buf)
  {
    if (strlen (buf) == 0)
      {
        if (arg)
          {
            sim_warn ("no argument to watch?\n");
            return SCPE_ARG;
          }
        sim_msg ("Clearing all watch points\n");
        memset (& watch_bits, 0, sizeof (watch_bits));
        return SCPE_OK;
      }
    char * end;
    long int n = strtol (buf, & end, 0);
    if (* end || n < 0 || n >= MEMSIZE)
      {
        sim_warn ("invalid argument to watch?\n");
        return SCPE_ARG;
      }
    watch_bits [n] = arg != 0;
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

#ifdef SCUMEM
#ifndef SPEED
static uint get_scu_unit_idx (word24 addr, word24 * offset)
  {
    int cpu_port_num = lookup_cpu_mem_map (addr, offset);
    if (cpu_port_num < 0) // Can't happen, we passed nem_check above
      { 
        sim_warn ("cpu_port_num < 0");
        doFault (FAULT_STR, fst_str_nea,  __func__);
      }
    return cables->cpu_to_scu [current_running_cpu_idx][cpu_port_num].scu_unit_idx;
  }
#endif
#endif

#if !defined(SPEED) || !defined(INLINE_CORE)
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
#ifndef SPEED
      nem_check (addr,  "core_read nem");
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
#ifdef SCUMEM
    word24 offset;
    uint scu_unit_idx = get_scu_unit_idx (addr, & offset);
    LOCK_MEM_RD;
    *data = scu [scu_unit_idx].M[offset] & DMASK;
    UNLOCK_MEM;
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o read   %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr,
                    * data, ctx);
      }
#else
#ifndef LOCKLESS
    if (M[addr] & MEM_UNINITIALIZED)
      {
        sim_debug (DBG_WARN, & cpu_dev,
                   "Unitialized memory accessed at address %08o; "
                   "IC is 0%06o:0%06o (%s(\n",
                   addr, cpu.PPR.PSR, cpu.PPR.IC, ctx);
      }
#endif
#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o read   %08o %012"PRIo64" "
                    "(%s)\n",
                    cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr, M [addr],
                    ctx);
        traceInstruction (0);
      }
#endif
#ifdef LOCKLESS
    word36 v;
    __storeload_barrier();
    v = atomic_load_acq_64((volatile u_long *)&M[addr]);
    if (v & MEM_LOCKED)
      sim_warn ("core_read: addr %x was locked\n", addr);
    *data = v & DMASK;
#else
    LOCK_MEM_RD;
    *data = M[addr] & DMASK;
    UNLOCK_MEM;
#endif

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

#ifdef LOCKLESS
int32 core_read_lock (word24 addr, word36 *data, const char * ctx)
{
    int i = 1000000000;
    while ( atomic_testandset_64((volatile u_long *)&M[addr], MEM_LOCKED_BIT) == 1 && i > 0) {
      //sim_warn ("core_read_lock: locked addr %x\n", addr);
      i--;
    }
    if (i == 0) {
      sim_warn ("core_read_lock: locked %x addr %x deadlock\n", cpu.locked_addr, addr);
    }
    if (cpu.locked_addr != 0) {
      sim_warn ("core_read_lock: locked %x addr %x\n", cpu.locked_addr, addr);
      core_unlock_all();
    }
    cpu.locked_addr = addr;
    __storeload_barrier();
    *data = atomic_load_acq_64((volatile u_long *)&M[addr]) & DMASK;
    return 0;
}
#endif

#if !defined(SPEED) || !defined(INLINE_CORE)
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
#ifndef SPEED
      nem_check (addr,  "core_write nem");
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
#ifdef SCUMEM
    word24 offset;
    uint sci_unit_idx = get_scu_unit_idx (addr, & offset);
    LOCK_MEM_WR;
    scu[sci_unit_idx].M[offset] = data & DMASK;
    UNLOCK_MEM;
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o write   %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    scu[sci_unit_idx].M[offset], ctx);
      }
#else
#ifdef LOCKLESS
    int i = 1000000000;
    while ( atomic_testandset_64((volatile u_long *)&M[addr], MEM_LOCKED_BIT) == 1 && i > 0) {
      //sim_warn ("core_read_lock: locked addr %x\n", addr);
      i--;
    }
    if (i == 0) {
      sim_warn ("core_write: locked %x addr %x deadlock\n", cpu.locked_addr, addr);
    }
    __storeload_barrier();
    atomic_store_rel_64((volatile u_long *)&M[addr], data & DMASK);
#else
    LOCK_MEM_WR;
    M[addr] = data & DMASK;
    UNLOCK_MEM;
#endif
#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o write  %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    M [addr], ctx);
        traceInstruction (0);
      }
#endif
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

#ifdef LOCKLESS
int core_write_unlock (word24 addr, word36 data, const char * ctx)
{
    if (cpu.locked_addr != addr)
      {
       sim_warn ("core_write_unlock: locked %x addr %x\n", cpu.locked_addr, addr);
       core_unlock_all();
      }
      
    __storeload_barrier();
    atomic_store_rel_64((volatile u_long *)&M[addr], data & DMASK);
    cpu.locked_addr = 0;
    return 0;
}

int core_unlock_all ()
{
  if (cpu.locked_addr != 0) {
      sim_warn ("core_unlock_all: locked %x\n", cpu.locked_addr);
      __storeload_barrier();
      atomic_store_rel_64((volatile u_long *)&M[cpu.locked_addr], M[cpu.locked_addr] & DMASK);
      cpu.locked_addr = 0;
  }
  return 0;
}
#endif

#if !defined(SPEED) || !defined(INLINE_CORE)
int core_write_zone (word24 addr, word36 data, const char * ctx)
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
#ifndef SPEED
      nem_check (addr,  "core_write_zone nem");
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
#ifdef SCUMEM
    word24 offset;
    uint sci_unit_idx = get_scu_unit_idx (addr, & offset);
    LOCK_MEM_WR;
    scu[sci_unit_idx].M[offset] = (scu[sci_unit_idx].M[offset] & ~cpu.zone) |
                              (data & cpu.zone);
    UNLOCK_MEM;
    cpu.useZone = false; // Safety
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o writez %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    scu[sci_unit_idx].M[offset], ctx);
      }
#else
#ifdef LOCKLESS
    word36 v;
    core_read_lock(addr,  &v, ctx);
    v = (v & ~cpu.zone) | (data & cpu.zone);
    core_write_unlock(addr, v, ctx);
#else
    LOCK_MEM_WR;
    M[addr] = (M[addr] & ~cpu.zone) | (data & cpu.zone);
    UNLOCK_MEM;
#endif
    cpu.useZone = false; // Safety
#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o writez %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    M [addr], ctx);
        traceInstruction (0);
      }
#endif
#endif
#ifdef TR_WORK_MEM
    cpu.rTRticks ++;
#endif
    sim_debug (DBG_CORE, & cpu_dev,
               "core_write_zone %08o %012"PRIo64" (%s)\n",
                addr, data, ctx);
    PNL (trackport (addr, data));
    return 0;
  }
#endif

#if !defined(SPEED) || !defined(INLINE_CORE)
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
#ifndef SPEED
    nem_check (addr,  "core_read2 nem");
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
#ifdef SCUMEM
    word24 offset;
    uint sci_unit_idx = get_scu_unit_idx (addr, & offset);
    LOCK_MEM_RD;
    *even = scu [sci_unit_idx].M[offset++] & DMASK;
    UNLOCK_MEM;
#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr,
                    * even, ctx);
      }
#endif

    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr, * even, ctx);
    LOCK_MEM_RD;
    *odd = scu [sci_unit_idx].M[offset] & DMASK;
    UNLOCK_MEM;
#ifndef SPEED
    if (watch_bits [addr+1])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr+1,
                    * odd, ctx);
      }
#endif

    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr+1, * odd, ctx);
#else
#ifndef LOCKLESS
    if (M[addr] & MEM_UNINITIALIZED)
      {
        sim_debug (DBG_WARN, & cpu_dev,
                   "Unitialized memory accessed at address %08o; "
                   "IC is 0%06o:0%06o (%s)\n",
                   addr, cpu.PPR.PSR, cpu.PPR.IC, ctx);
      }
#endif
#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr, 
                    M [addr], ctx);
        traceInstruction (0);
      }
#endif
#ifdef LOCKLESS
    word36 v;
    __storeload_barrier();
    v = atomic_load_acq_64((volatile u_long *)&M[addr]);
    if (v & MEM_LOCKED)
      sim_warn ("core_read2: even addr %x was locked\n", addr);
    *even = v & DMASK;
    addr++;
#else
    LOCK_MEM_RD;
    *even = M[addr++] & DMASK;
    UNLOCK_MEM;
#endif
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr - 1, * even, ctx);

    // if the even address is OK, the odd will be
    //nem_check (addr,  "core_read2 nem");
#ifndef LOCKLESS
    if (M[addr] & MEM_UNINITIALIZED)
      {
        sim_debug (DBG_WARN, & cpu_dev,
                   "Unitialized memory accessed at address %08o; "
                   "IC is 0%06o:0%06o (%s)\n",
                    addr, cpu.PPR.PSR, cpu.PPR.IC, ctx);
      }
#endif
#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o read2  %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr,
                    M [addr], ctx);
        traceInstruction (0);
      }
#endif
#ifdef LOCKLESS
    __storeload_barrier();
    v = atomic_load_acq_64((volatile u_long *)&M[addr]);
    if (v & MEM_LOCKED)
      sim_warn ("core_read2: odd addr %x was locked\n", addr);
    *odd = v & DMASK;
#else
    LOCK_MEM_RD;
    *odd = M[addr] & DMASK;
    UNLOCK_MEM;
#endif
    sim_debug (DBG_CORE, & cpu_dev,
               "core_read2 %08o %012"PRIo64" (%s)\n",
                addr, * odd, ctx);
#endif
#ifdef TR_WORK_MEM
    cpu.rTRticks ++;
#endif
    PNL (trackport (addr - 1, * even));
    return 0;
  }
#endif

#if !defined(SPEED) || !defined(INLINE_CORE)
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
#ifndef SPEED
      nem_check (addr,  "core_write2 nem");
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

#ifdef SCUMEM
    word24 offset;
    uint sci_unit_idx = get_scu_unit_idx (addr, & offset);
    scu [sci_unit_idx].M[offset++] = even & DMASK;
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr,
                    even, ctx);
      }
    LOCK_MEM_WR;
    scu [sci_unit_idx].M[offset] = odd & DMASK;
    UNLOCK_MEM;
    if (watch_bits [addr+1])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr+1,
                    odd, ctx);
      }
#else
#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr,
                    even, ctx);
        traceInstruction (0);
      }
#endif
#ifdef LOCKLESS
    int i = 1000000000;
    while ( atomic_testandset_64((volatile u_long *)&M[addr], MEM_LOCKED_BIT) == 1 && i > 0) {
      //sim_warn ("core_read_lock: locked addr %x\n", addr);
      i--;
    }
    if (i == 0) {
      sim_warn ("core_write2: even locked %x addr %x deadlock\n", cpu.locked_addr, addr);
    }
    __storeload_barrier();
    atomic_store_rel_64((volatile u_long *)&M[addr], even & DMASK);
    addr++;
#else
    LOCK_MEM_WR;
    M[addr++] = even & DMASK;
    UNLOCK_MEM;
#endif
    sim_debug (DBG_CORE, & cpu_dev,
               "core_write2 %08o %012"PRIo64" (%s)\n",
                addr - 1, even, ctx);

    // If the even address is OK, the odd will be
    //nem_check (addr,  "core_write2 nem");

#ifndef SPEED
    if (watch_bits [addr])
      {
        sim_msg ("WATCH [%"PRId64"] %05o:%06o write2 %08o %012"PRIo64" "
                    "(%s)\n", cpu.cycleCnt, cpu.PPR.PSR, cpu.PPR.IC, addr,
                    odd, ctx);
        traceInstruction (0);
      }
#endif
#ifdef LOCKLESS
    i = 1000000000;
    while ( atomic_testandset_64((volatile u_long *)&M[addr], MEM_LOCKED_BIT) == 1 && i > 0) {
      //sim_warn ("core_read_lock: locked addr %x\n", addr);
      i--;
    }
    if (i == 0) {
      sim_warn ("core_write2: odd locked %x addr %x deadlock\n", cpu.locked_addr, addr);
    }
    __storeload_barrier();
    atomic_store_rel_64((volatile u_long *)&M[addr], odd & DMASK);
    addr++;
#else
    LOCK_MEM_WR;
    M[addr] = odd & DMASK;
    UNLOCK_MEM;
#endif
#endif
#ifdef TR_WORK_MEM
    cpu.rTRticks ++;
#endif
    PNL (trackport (addr - 1, even));
    sim_debug (DBG_CORE, & cpu_dev,
               "core_write2 %08o %012"PRIo64" (%s)\n",
                addr, odd, ctx);
    return 0;
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

void decode_instruction (word36 inst, DCDstruct * p)
  {
    CPT (cpt1L, 17); // instruction decoder
    memset (p, 0, sizeof (struct DCDstruct));

    p->opcode  = GET_OP (inst);   // get opcode
    p->opcodeX = GET_OPX(inst);   // opcode extension
    p->opcode10 = p->opcode | (p->opcodeX ? 01000 : 0);
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

addr_modes_e get_addr_mode (void)
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

void set_addr_mode (addr_modes_e mode)
  {
//    cpu.cu.XSF = false;
//sim_debug (DBG_TRACEEXT, & cpu_dev, "set_addr_mode bit 29 sets XSF to 0\n");
    //cpu.went_appending = false;
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
word18 get_BAR_address (word18 addr)
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

static void add_history (uint hset, word36 w0, word36 w1)
  {
    //if (cpu.MR.emr)
      {
        cpu.history [hset] [cpu.history_cyclic[hset]] [0] = w0;
        cpu.history [hset] [cpu.history_cyclic[hset]] [1] = w1;
        cpu.history_cyclic[hset] = (cpu.history_cyclic[hset] + 1) % N_HIST_SIZE;
      }
  }

void add_history_force (uint hset, word36 w0, word36 w1)
  {
    cpu.history [hset] [cpu.history_cyclic[hset]] [0] = w0;
    cpu.history [hset] [cpu.history_cyclic[hset]] [1] = w1;
    cpu.history_cyclic[hset] = (cpu.history_cyclic[hset] + 1) % N_HIST_SIZE;
  }

#ifdef DPS8M
void add_CU_history (void)
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
    add_history (CU_HIST_REG, w0, w1);
  }

void add_DUOU_history (word36 flags, word18 ICT, word9 RS_REG, word9 flags2)
  {
    word36 w0 = flags, w1 = 0;
    w1 |= (ICT & MASK18) << 18;
    w1 |= (RS_REG & MASK9) << 9;
    w1 |= flags2 & MASK9;
    add_history (DU_OU_HIST_REG, w0, w1);
  }

void add_APU_history (word15 ESN, word21 flags, word24 RMA, word3 RTRR, word9 flags2)
  {
    word36 w0 = 0, w1 = 0;
    w0 |= (ESN & MASK15) << 21;
    w0 |= flags & MASK21;
    w1 |= (RMA & MASK24) << 12;
    w1 |= (RTRR & MASK3) << 9;
    w1 |= flags2 & MASK9;
    add_history (APU_HIST_REG, w0, w1);
  }

void add_EAPU_history (word18 ZCA, word18 opcode)
  {
    word36 w0 = 0;
    w0 |= (ZCA & MASK18) << 18;
    w0 |= opcode & MASK18;
    add_history (EAPU_HIST_REG, w0, 0);
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

void add_CU_history (void)
  {
    CPT (cpt1L, 24); // add cu hist
// XXX strobe on opcode match
    if (cpu.skip_cu_hist)
      return;
    if (! cpu.MR_cache.emr)
      return;
    if (! cpu.MR_cache.ihr)
      return;

//IF1 if (cpu.MR.hrhlt) sim_msg ("%u\n", cpu.history_cyclic[CU_HIST_REG]);
//IF1 sim_msg ("%u\n", cpu.history_cyclic[CU_HIST_REG]);
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

    add_history (CU_HIST_REG, w0, w1);

    // Check for overflow
    CPTUR (cptUseMR);
    if (cpu.MR.hrhlt && cpu.history_cyclic[CU_HIST_REG] == 0)
      {
        //cpu.history_cyclic[CU_HIST_REG] = 15;
        if (cpu.MR.ihrrs)
          {
            cpu.MR.ihr = 0;
          }
//IF1 sim_msg ("trapping......\n");
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

void add_DU_history (void)
  {
    CPT (cpt1L, 25); // add du hist
    PNL (add_history (DU_HIST_REG, cpu.du.cycle1, cpu.du.cycle2);)
  }
     

void add_OU_history (void)
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
         (word10) ~opcodes10 [cpu.ou.RS].reg_use);)

    // 51-53 0

    // 54-71 ICT TRACKER
    putbits36_18 (& w1, 54 - 36, cpu.PPR.IC);

    add_history (OU_HIST_REG, w0, w1);
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

// XXX add_APU_history

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

void add_APU_history (enum APUH_e op)
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

    add_history (APU_HIST_REG, w0, w1);
  }

#endif

#if defined(THREADZ) || defined(LOCKLESS)
static pthread_mutex_t debug_lock = PTHREAD_MUTEX_INITIALIZER;

static const char * get_dbg_verb (uint32 dbits, DEVICE * dptr)
  {
    static const char * debtab_none    = "DEBTAB_ISNULL";
    static const char * debtab_nomatch = "DEBTAB_NOMATCH";
    const char * some_match = NULL;
    int32 offset = 0;

    if (dptr->debflags == 0)
      return debtab_none;

    dbits &= dptr->dctrl;     /* Look for just the bits tha matched */

    /* Find matching words for bitmask */

    while (dptr->debflags[offset].name && (offset < 32))
      {
        if (dptr->debflags[offset].mask == dbits)   /* All Bits Match */
          return dptr->debflags[offset].name;
        if (dptr->debflags[offset].mask & dbits)
          some_match = dptr->debflags[offset].name;
        offset ++;
      }
    return some_match ? some_match : debtab_nomatch;
  }

void dps8_sim_debug (uint32 dbits, DEVICE * dptr, unsigned long long cnt, const char* fmt, ...)
  {
    pthread_mutex_lock (& debug_lock);
    if (sim_deb && dptr && (dptr->dctrl & dbits))
      {
        const char * debug_type = get_dbg_verb (dbits, dptr);
        char stackbuf[STACKBUFSIZE];
        int32 bufsize = sizeof (stackbuf);
        char * buf = stackbuf;
        va_list arglist;
        int32 i, j, len;

        buf [bufsize-1] = '\0';

        while (1)
          {                 /* format passed string, args */
            va_start (arglist, fmt);
#if defined(NO_vsnprintf)
            len = vsprintf (buf, fmt, arglist);
#else                                                   /* !defined(NO_vsnprintf) */
            len = vsnprintf (buf, (unsigned long) bufsize-1, fmt, arglist);
#endif                                                  /* NO_vsnprintf */
            va_end (arglist);

/* If the formatted result didn't fit into the buffer, then grow the buffer and try again */

            if ((len < 0) || (len >= bufsize-1)) 
              {
                if (buf != stackbuf)
                  free (buf);
                bufsize = bufsize * 2;
                if (bufsize < len + 2)
                  bufsize = len + 2;
                buf = (char *) malloc ((unsigned long) bufsize);
                if (buf == NULL)                            /* out of memory */
                  return;
                buf[bufsize-1] = '\0';
                continue;
              }
            break;
          }

/* Output the formatted data expanding newlines where they exist */

        for (i = j = 0; i < len; ++i)
          {
            if ('\n' == buf[i])
              {
                if (i >= j) 
                  {
                    if ((i != j) || (i == 0))
                      {
                          fprintf (sim_deb, "DBG(%lld) %o: %s %s %.*s\r\n", cnt, current_running_cpu_idx, dptr->name, debug_type, i-j, &buf[j]);
                      }
                  }
                j = i + 1;
              }
          }

/* Set unterminated flag for next time */

        if (buf != stackbuf)
          free (buf);
      }
    pthread_mutex_unlock (& debug_lock);
  }
#endif
