/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file dps8_sys.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#ifndef __MINGW64__
#include <wordexp.h>
#include <signal.h>
#endif
#include <unistd.h>

#ifdef __APPLE__
#include <pthread.h>
#endif

#include "dps8.h"
#include "dps8_console.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_ins.h"
#include "dps8_iom.h"
#include "dps8_math.h"
#include "dps8_scu.h"
#include "dps8_mt.h"
#include "dps8_disk.h"
#include "dps8_utils.h"
#include "dps8_append.h"
#include "dps8_fnp2.h"
#include "dps8_crdrdr.h"
#include "dps8_crdpun.h"
#include "dps8_prt.h"
#include "dps8_urp.h"
#include "dps8_cable.h"
#include "dps8_absi.h"
#include "utlist.h"

// XXX Strictly speaking, memory belongs in the SCU
// We will treat memory as viewed from the CPU and elide the
// SCU configuration that maps memory across multiple SCUs.
// I would guess that multiple SCUs helped relieve memory
// contention across multiple CPUs, but that is a level of
// emulation that will be ignored.

word36 *M = NULL;                                          /*!< memory */


// These are part of the simh interface
#ifdef DPS8M
char sim_name[] = "DPS8M";
#endif
#ifdef L68
char sim_name[] = "L68";
#endif
int32 sim_emax = 4; ///< some EIS can take up to 4-words
static void dps8_init(void);
void (*sim_vm_init) (void) = & dps8_init;    //CustomCmds;

#ifndef __MINGW64__
static pid_t dps8m_sid; // Session id
#endif

#ifdef PANEL
void panelScraper (void);
#endif

static t_stat dps_debug_mme_cntdwn (UNUSED int32 arg, const char * buf);
static t_stat dps_debug_skip (int32 arg, const char * buf);
static t_stat dps_debug_start (int32 arg, const char * buf);
static t_stat dps_debug_stop (int32 arg, const char * buf);
static t_stat dps_debug_break (int32 arg, const char * buf);
static t_stat dps_debug_segno (int32 arg, const char * buf);
static t_stat dps_debug_ringno (int32 arg, const char * buf);
static t_stat dps_debug_bar (int32 arg, UNUSED const char * buf);

static t_stat sbreak (int32 arg, const char * buf);
static t_stat doEXF (UNUSED int32 arg,  UNUSED const char * buf);
static t_stat defaultBaseSystem (int32 arg, const char * buf);
static t_stat searchMemory (UNUSED int32 arg, const char * buf);
static t_stat bootSkip (int32 UNUSED arg, const char * UNUSED buf);
static t_stat setDbgCPUMask (int32 UNUSED arg, const char * UNUSED buf);

static CTAB dps8_cmds[] =
{
    {"CABLE",    sys_cable,       0, "cable String a cable\n" , NULL, NULL},
    {"CABLE_RIPOUT",    sys_cable_ripout,       0, "cable Unstring all cables\n" , NULL, NULL},
    {"DBGMMECNTDWN", dps_debug_mme_cntdwn, 0, "dbgmmecntdwn Enable debug after n MMEs\n", NULL, NULL},
    {"DBGSKIP", dps_debug_skip, 0, "dbgskip Skip first n TRACE debugs\n", NULL, NULL},
    {"DBGSTART", dps_debug_start, 0, "dbgstart Limit debugging to N > Cycle count\n", NULL, NULL},
    {"DBGSTOP", dps_debug_stop, 0, "dbgstop Limit debugging to N < Cycle count\n", NULL, NULL},
    {"DBGBREAK", dps_debug_break, 0, "dbgstop Break when N >= Cycle count\n", NULL, NULL},
    {"DBGSEGNO", dps_debug_segno, 0, "dbgsegno Limit debugging to PSR == segno\n", NULL, NULL},
    {"DBGRINGNO", dps_debug_ringno, 0, "dbgsegno Limit debugging to PRR == ringno\n", NULL, NULL},
    {"DBGBAR", dps_debug_bar, 1, "dbgbar Limit debugging to BAR mode\n", NULL, NULL},
    {"NODBGBAR", dps_debug_bar, 0, "dbgbar Limit debugging to BAR mode\n", NULL, NULL},
    {"TEST", brkbrk, 0, "test: internal testing\n", NULL, NULL},
// copied from scp.c
#define SSH_ST          0                               /* set */
#define SSH_SH          1                               /* show */
#define SSH_CL          2                               /* clear */
    {"SBREAK", sbreak, SSH_ST, "sbreak: Set a breakpoint with segno:offset syntax\n", NULL, NULL},
    {"NOSBREAK", sbreak, SSH_CL, "nosbreak: Unset an SBREAK\n", NULL, NULL},
    {"XF", doEXF, 0, "Execute fault: Press the execute fault button\n", NULL, NULL},
    {"WATCH", memWatch, 1, "watch: watch memory location\n", NULL, NULL},
    {"NOWATCH", memWatch, 0, "watch: watch memory location\n", NULL, NULL},
    {"AUTOINPUT", opconAutoinput, 0, "set console auto-input\n", NULL, NULL},
    {"CLRAUTOINPUT", opconAutoinput, 1, "clear console auto-input\n", NULL, NULL},
    
    {"SEARCHMEMORY", searchMemory, 0, "searchMemory: search memory for value\n", NULL, NULL},

    {"FNPLOAD", fnpLoad, 0, "fnpload: load Devices.txt into FNP", NULL, NULL},
    {"FNPSERVERPORT", fnpServerPort, 0, "fnpServerPort: set the FNP dialin telnter port number", NULL, NULL},
    {"SKIPBOOT", bootSkip, 0, "skip forward on boot tape", NULL, NULL},
    {"DEFAULT_BASE_SYSTEM", defaultBaseSystem, 0, "Set configuration to defaults", NULL, NULL},
    {"FNPSTART", fnpStart, 0, "Force early FNP initialization", NULL, NULL},
    {"DBGCPUMASK", setDbgCPUMask, 0, "Set per CPU debug enable", NULL, NULL},
    { NULL, NULL, 0, NULL, NULL, NULL}
};

/*!
 \brief special dps8 VM commands ....
 
 For greater flexibility, SCP provides some optional interfaces that can be used to extend its command input, command processing, and command post-processing capabilities. These interfaces are strictly optional
 and are off by default. Using them requires intimate knowledge of how SCP functions internally and is not recommended to the novice VM writer.
 
 Guess I shouldn't use these then :)
 */

static t_addr parse_addr(DEVICE *dptr, const char *cptr, const char **optr);
static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr addr);

#ifndef __MINGW64__
static void usr1SignalHandler (UNUSED int sig)
  {
    sim_printf ("USR1 signal caught; pressing the EXF button\n");
    // Assume the bootload CPU
    setG7fault (0, FAULT_EXF, fst_zero);
    return;
  }
#endif

// Once-only initialization

static void dps8_init(void)
{
#include "dps8.sha1.txt"
#ifdef DPS8M
    sim_printf ("DPS8/M emulator (git %8.8s)\n", COMMIT_ID);
#endif
#ifdef L68
    sim_printf ("L68 emulator (git %8.8s)\n", COMMIT_ID);
#endif
#ifdef TESTING
    sim_printf ("#### TESTING BUILD ####\n");
#else
    sim_printf ("Production build\n");
#endif
#ifdef ISOLTS
    sim_printf ("#### ISOLTS BUILD ####\n");
#endif

    // special dps8 initialization stuff that cant be done in reset, etc .....

    // These are part of the simh interface
    sim_vm_parse_addr = parse_addr;
    sim_vm_fprint_addr = fprint_addr;

    sim_vm_cmd = dps8_cmds;

#ifndef __MINGW64__
    // Create a session for this dps8m system instance.
    dps8m_sid = setsid ();
    if (dps8m_sid == (pid_t) -1)
      dps8m_sid = getsid (0);
#ifdef DPS8M
    sim_printf ("DPS8M system session id is %d\n", dps8m_sid);
#endif
#ifdef L68
    sim_printf ("L68 system session id is %d\n", dps8m_sid);
#endif
#endif

#ifndef __MINGW64__
    // Wire the XF button to signal USR1
    signal (SIGUSR1, usr1SignalHandler);
#endif

    init_opcodes();
    sysCableInit ();
    iom_init ();
    console_init ();
    disk_init ();
    mt_init ();
    fnpInit ();
    //mpc_init ();
    scu_init ();
    cpu_init ();
    crdrdr_init ();
    crdpun_init ();
    prt_init ();
    urp_init ();
#ifndef __MINGW64__
    absi_init ();
#endif
    defaultBaseSystem (0, NULL);
#ifdef PANEL
    panelScraper ();
#endif
}

uint64 sim_deb_start = 0;
uint64 sim_deb_stop = 0;
uint64 sim_deb_break = 0;
uint64 sim_deb_segno = NO_SUCH_SEGNO;
uint64 sim_deb_ringno = NO_SUCH_RINGNO;
uint64 sim_deb_skip_limit = 0;
uint64 sim_deb_skip_cnt = 0;
uint64 sim_deb_mme_cntdwn = 0;
uint dbgCPUMask = 0377; // default all 8 on

bool sim_deb_bar = false;

static t_stat dps_debug_mme_cntdwn (UNUSED int32 arg, const char * buf)
  {
    sim_deb_mme_cntdwn = strtoull (buf, NULL, 0);
    sim_printf ("Debug MME countdown set to %"PRId64"\n", sim_deb_mme_cntdwn);
    return SCPE_OK;
  }

static t_stat dps_debug_skip (UNUSED int32 arg, const char * buf)
  {
    sim_deb_skip_cnt = 0;
    sim_deb_skip_limit = strtoull (buf, NULL, 0);
    sim_printf ("Debug skip set to %"PRId64"\n", sim_deb_skip_limit);
    return SCPE_OK;
  }

static t_stat dps_debug_start (UNUSED int32 arg, const char * buf)
  {
    sim_deb_start = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to start at cycle: %"PRId64"\n", sim_deb_start);
    return SCPE_OK;
  }

static t_stat dps_debug_stop (UNUSED int32 arg, const char * buf)
  {
    sim_deb_stop = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to stop at cycle: %"PRId64"\n", sim_deb_stop);
    return SCPE_OK;
  }

static t_stat dps_debug_break (UNUSED int32 arg, const char * buf)
  {
    sim_deb_break = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to break at cycle: %"PRId64"\n", sim_deb_break);
    return SCPE_OK;
  }

static t_stat dps_debug_segno (UNUSED int32 arg, const char * buf)
  {
    sim_deb_segno = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to segno %"PRIo64"\n", sim_deb_segno);
    return SCPE_OK;
  }

static t_stat dps_debug_ringno (UNUSED int32 arg, const char * buf)
  {
    sim_deb_ringno = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to ringno %"PRIo64"\n", sim_deb_ringno);
    return SCPE_OK;
  }

static t_stat dps_debug_bar (int32 arg, UNUSED const char * buf)
  {
    sim_deb_bar = arg;
    if (arg)
      sim_printf ("Debug set BAR %"PRIo64"\n", sim_deb_ringno);
    else
      sim_printf ("Debug unset BAR %"PRIo64"\n", sim_deb_ringno);
    return SCPE_OK;
  }

t_stat brkbrk (UNUSED int32 arg, UNUSED const char *  buf)
  {
    //listSource (buf, 0);
    return SCPE_OK;
  }

// SEARCHMEMORY valye

static t_stat searchMemory (UNUSED int32 arg, const char * buf)
  {
    word36 value;
    if (sscanf (buf, "%"PRIo64"", & value) != 1)
      return SCPE_ARG;
    
    uint i;
    for (i = 0; i < MEMSIZE; i ++)
      if ((M [i] & DMASK) == value)
        sim_printf ("%08o\n", i);
    return SCPE_OK;
  }


// EXF

static t_stat doEXF (UNUSED int32 arg,  UNUSED const char * buf)
  {
    // Assume bootload CPU
    setG7fault (0, FAULT_EXF, fst_zero);
    return SCPE_OK;
  }

// SBREAK segno:offset

static t_stat sbreak (int32 arg, const char * buf)
  {
    //printf (">> <%s>\n", buf);
    int segno, offset;
    int where;
    int cnt = sscanf (buf, "%o:%o%n", & segno, & offset, & where);
    if (cnt != 2)
      {
        return SCPE_ARG;
      }
    char reformatted [strlen (buf) + 20];
    sprintf (reformatted, "0%04o%06o%s", segno, offset, buf + where);
    //printf (">> <%s>\n", reformatted);
    t_stat rc = brk_cmd (arg, reformatted);
    return rc;
  }


static t_addr parse_addr (UNUSED DEVICE * dptr, const char *cptr, const char **optr)
{
    // No, determine absolute address given by cptr
    return (t_addr)strtol(cptr, (char **) optr, 8);
}


static void fprint_addr (FILE * stream, UNUSED DEVICE *  dptr, t_addr simh_addr)
{
    fprintf(stream, "%06o", simh_addr);
}
 
// This is part of the simh interface
/*! Based on the switch variable, symbolically output to stream ofile the data in array val at the specified addr
 in unit uptr.

 * simh "fprint_sym" – Based on the switch variable, symbolically output to stream ofile the data in array val at the specified addr in unit uptr.
 */

t_stat fprint_sym (FILE * ofile, UNUSED t_addr  addr, t_value *val, 
                   UNIT *uptr, int32 sw)
{
// XXX Bug: assumes single cpu
// XXX CAC: This seems rather bogus; deciding the output format based on the
// address of the UNIT? Would it be better to use sim_unit.u3 (or some such 
// as a word width?

    if (!((uint) sw & SWMASK ('M')))
        return SCPE_ARG;
    
    if (uptr == &cpu_dev . units [0])
    {
        word36 word1 = *val;
        char buf [256];
        // get base syntax
        char *d = disAssemble(buf, word1);
        
        fprintf(ofile, "%s", d);
        
        // decode instruction
        DCDstruct ci;
        DCDstruct * p = & ci;
        decodeInstruction (word1, p);
        
        // MW EIS?
        if (p->info->ndes > 1)
        {
            // Yup, just output word values (for now)
            
            // XXX Need to complete MW EIS support in disAssemble()
            
            for(uint n = 0 ; n < p->info->ndes; n += 1)
                fprintf(ofile, " %012"PRIo64"", val[n + 1]);
          
            return (t_stat) -p->info->ndes;
        }
        
        return SCPE_OK;

        //fprintf(ofile, "%012"PRIo64"", *val);
        //return SCPE_OK;
    }
    return SCPE_ARG;
}

// This is part of the simh interface
/*!  – Based on the switch variable, parse character string cptr for a symbolic value val at the specified addr
 in unit uptr.
 */
t_stat parse_sym (UNUSED const char * cptr, UNUSED t_addr addr, UNUSED UNIT * uptr, 
                  UNUSED t_value * val, UNUSED int32 sswitch)
{
    return SCPE_ARG;
}


static t_stat sys_reset (UNUSED DEVICE  * dptr)
  {
    return SCPE_OK;
  }

static DEVICE sys_dev = {
    "SYS",       /* name */
    NULL,        /* units */
    NULL,        /* registers */
    NULL,     /* modifiers */
    0,           /* #units */
    8,           /* address radix */
    PASIZE,      /* address width */
    1,           /* address increment */
    8,           /* data radix */
    36,          /* data width */
    NULL,        /* examine routine */
    NULL,        /* deposit routine */
    & sys_reset, /* reset routine */
    NULL,        /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    0,           /* flags */
    0,           /* debug control flags */
    0,           /* debug flag names */
    NULL,        /* memory size change */
    NULL,        /* logical name */
    NULL,        /* help */
    NULL,        /* attach_help */
    NULL,        /* help_ctx */
    NULL,        /* description */
    NULL
};


// This is part of the simh interface
DEVICE * sim_devices [] =
  {
    & cpu_dev, // dev[0] is special to simh; it is the 'default device'
    & iom_dev,
    & tape_dev,
    & fnpDev,
    & disk_dev,
    & scu_dev,
    // & mpc_dev,
    & opcon_dev,
    & sys_dev,
    // & ipc_dev,  // for fnp IPC
    & urp_dev,
    & crdrdr_dev,
    & crdpun_dev,
    & prt_dev,
#ifndef __MINGW64__
    & absi_dev,
#endif
    NULL
  };

static void doIniLine (char * text)
  {
    //sim_printf ("<%s?\n", text);
    char gbuf [257];
    const char * cptr = get_glyph (text, gbuf, 0); /* get command glyph */
    CTAB *cmdp;
    if ((cmdp = find_cmd (gbuf)))            /* lookup command */
      {
        t_stat stat = cmdp->action (cmdp->arg, cptr); /* if found, exec */
        if (stat != SCPE_OK)
          sim_printf ("%s: %s\n", sim_error_text (SCPE_UNK), text);
      }
    else
      sim_printf ("%s: %s\n", sim_error_text (SCPE_UNK), text);
  }

static t_stat defaultBaseSystem (UNUSED int32 arg, UNUSED const char * buf)
  {

    // ;
    // ; Configure test system
    // ;
    // ; CPU, IOM * 2, MPC, TAPE * 16, DISK * 16, SCU * 4, OPCON, FNP, URP * 3,
    // ; PRT, RDR, PUN
    // ;
    // ;
    // ; From AN70-1 System Initialization PLM May 84, pg 8-4:
    // ;
    // ; All CPUs and IOMs must share the same layout of port assignments to
    // ; SCUs. Thus, if memory port B of CPU C goes to SCU D, the memory port
    // ; B of all other CPUs and IOMs must go to SCU D. All CPUs and IOMs must
    // ; describe this SCU the same; all must agree in memory sizes. Also, all
    // ; SCUs must agree on port assignments of CPUs and IOMs. This, if port 3 
    // ; of SCU C goes to CPU A, the port 3 of all other SCUs must also go to
    // ; CPU A.
    // ;
    // ; Pg. 8-6:
    // ;
    // ; The actual memory size of the memory attached to the SCU attached to
    // ; the processor port in questions is 32K * 2 ** (encoded memory size).
    // ; The port assignment couples with the memory size to determine the base 
    // ; address of the SCU connected to the specified CPU port (absoulte
    // ; address of the first location in the memory attached to that SCU). The 
    // ; base address of the SCU is the (actual memory size) * (port assignment).
    // ;
    // ; Pg. 8-6
    // ;
    // ; [bits 09-11 lower store size]
    // ;
    // ; A DPS-8 SCU may have up to four store units attached to it. If this is
    // ; the case, two stores units form a pair of units. The size of a pair of
    // ; units (or a single unit) is 32K * 2 ** (lower store size) above.
    // ;
    // ;
    // ;
    // ; Looking at bootload_io, it would appear that Multics is happier with
    // ; IOM0 being the bootload IOM, despite suggestions elsewhere that was
    // ; not a requirement.

    // ; Disconnect everything...
    doIniLine ("cable_ripout");

    doIniLine ("set cpu nunits=8");
    doIniLine ("set iom nunits=2");
    // ; 16 drives plus the controller
    doIniLine ("set tape nunits=17");
    // ; 16 drives; no controller
    doIniLine ("set disk nunits=16");
    doIniLine ("set scu nunits=4");
    doIniLine ("set opcon nunits=1");
    doIniLine ("set fnp nunits=8");
    doIniLine ("set urp nunits=3");
    doIniLine ("set crdrdr nunits=1");
    doIniLine ("set crdpun nunits=1");
    doIniLine ("set prt nunits=17");
#ifndef __MINGW64__
    doIniLine ("set absi nunits=1");

    // ;Create card reader queue directory
    doIniLine ("! if [ ! -e /tmp/rdra ]; then mkdir /tmp/rdra; fi");
#else
    doIniLine ("! mkdir %TEMP%\\rdra");
#endif









    doIniLine ("set cpu0 config=faultbase=Multics");

    doIniLine ("set cpu0 config=num=0");
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    doIniLine ("set cpu0 config=data=024000717200");

    // ; enable ports 0 and 1 (scu connections)
    // ; portconfig: ABCD
    // ;   each is 3 bits addr assignment
    // ;           1 bit enabled 
    // ;           1 bit sysinit enabled
    // ;           1 bit interlace enabled (interlace?)
    // ;           3 bit memory size
    // ;              0 - 32K
    // ;              1 - 64K
    // ;              2 - 128K
    // ;              3 - 256K
    // ;              4 - 512K
    // ;              5 - 1M
    // ;              6 - 2M
    // ;              7 - 4M  

    doIniLine ("set cpu0 config=port=A");
    doIniLine ("set cpu0   config=assignment=0");
    doIniLine ("set cpu0   config=interlace=0");
    doIniLine ("set cpu0   config=enable=1");
    doIniLine ("set cpu0   config=init_enable=1");
    doIniLine ("set cpu0   config=store_size=4M");
 
    doIniLine ("set cpu0 config=port=B");
    doIniLine ("set cpu0   config=assignment=1");
    doIniLine ("set cpu0   config=interlace=0");
    doIniLine ("set cpu0   config=enable=1");
    doIniLine ("set cpu0   config=init_enable=1");
    doIniLine ("set cpu0   config=store_size=4M");

    doIniLine ("set cpu0 config=port=C");
    doIniLine ("set cpu0   config=assignment=2");
    doIniLine ("set cpu0   config=interlace=0");
    doIniLine ("set cpu0   config=enable=1");
    doIniLine ("set cpu0   config=init_enable=1");
    doIniLine ("set cpu0   config=store_size=4M");

    doIniLine ("set cpu0 config=port=D");
    doIniLine ("set cpu0   config=assignment=3");
    doIniLine ("set cpu0   config=interlace=0");
    doIniLine ("set cpu0   config=enable=1");
    doIniLine ("set cpu0   config=init_enable=1");
    doIniLine ("set cpu0   config=store_size=4M");

    // ; 0 = GCOS 1 = VMS
    doIniLine ("set cpu0 config=mode=Multics");
    // ; 0 = 8/70
    doIniLine ("set cpu0 config=speed=0");

    doIniLine ("set cpu0 config=dis_enable=enable");
    doIniLine ("set cpu0 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu config=disable_wam=enable");
    doIniLine ("set cpu0 config=tro_enable=enable");



    doIniLine ("set cpu1 config=faultbase=Multics");
    doIniLine ("set cpu1 config=num=1");
    doIniLine ("set cpu1 config=data=024000717200");

    doIniLine ("set cpu1 config=port=A");
    doIniLine ("set cpu1   config=assignment=0");
    doIniLine ("set cpu1   config=interlace=0");
    doIniLine ("set cpu1   config=enable=1");
    doIniLine ("set cpu1   config=init_enable=1");
    doIniLine ("set cpu1   config=store_size=4M");
 
    doIniLine ("set cpu1 config=port=B");
    doIniLine ("set cpu1   config=assignment=1");
    doIniLine ("set cpu1   config=interlace=0");
    doIniLine ("set cpu1   config=enable=1");
    doIniLine ("set cpu1   config=init_enable=1");
    doIniLine ("set cpu1   config=store_size=4M");

    doIniLine ("set cpu1 config=port=C");
    doIniLine ("set cpu1   config=assignment=2");
    doIniLine ("set cpu1   config=interlace=0");
    doIniLine ("set cpu1   config=enable=1");
    doIniLine ("set cpu1   config=init_enable=1");
    doIniLine ("set cpu1   config=store_size=4M");

    doIniLine ("set cpu1 config=port=D");
    doIniLine ("set cpu1   config=assignment=3");
    doIniLine ("set cpu1   config=interlace=0");
    doIniLine ("set cpu1   config=enable=1");
    doIniLine ("set cpu1   config=init_enable=1");
    doIniLine ("set cpu1   config=store_size=4M");

    doIniLine ("set cpu1 config=mode=Multics");
    doIniLine ("set cpu1 config=speed=0");

    doIniLine ("set cpu1 config=dis_enable=enable");
    doIniLine ("set cpu1 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu1 config=tro_enable=enable");



    doIniLine ("set cpu2 config=faultbase=Multics");
    doIniLine ("set cpu2 config=num=2");
    doIniLine ("set cpu2 config=data=024000717200");

    doIniLine ("set cpu2 config=port=A");
    doIniLine ("set cpu2   config=assignment=0");
    doIniLine ("set cpu2   config=interlace=0");
    doIniLine ("set cpu2   config=enable=1");
    doIniLine ("set cpu2   config=init_enable=1");
    doIniLine ("set cpu2   config=store_size=4M");
 
    doIniLine ("set cpu2 config=port=B");
    doIniLine ("set cpu2   config=assignment=1");
    doIniLine ("set cpu2   config=interlace=0");
    doIniLine ("set cpu2   config=enable=1");
    doIniLine ("set cpu2   config=init_enable=1");
    doIniLine ("set cpu2   config=store_size=4M");

    doIniLine ("set cpu2 config=port=C");
    doIniLine ("set cpu2   config=assignment=2");
    doIniLine ("set cpu2   config=interlace=0");
    doIniLine ("set cpu2   config=enable=1");
    doIniLine ("set cpu2   config=init_enable=1");
    doIniLine ("set cpu2   config=store_size=4M");

    doIniLine ("set cpu2 config=port=D");
    doIniLine ("set cpu2   config=assignment=3");
    doIniLine ("set cpu2   config=interlace=0");
    doIniLine ("set cpu2   config=enable=1");
    doIniLine ("set cpu2   config=init_enable=1");
    doIniLine ("set cpu2   config=store_size=4M");

    doIniLine ("set cpu2 config=mode=Multics");
    doIniLine ("set cpu2 config=speed=0");

    doIniLine ("set cpu2 config=dis_enable=enable");
    doIniLine ("set cpu2 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu2 config=tro_enable=enable");



    doIniLine ("set cpu3 config=faultbase=Multics");
    doIniLine ("set cpu3 config=num=3");
    doIniLine ("set cpu3 config=data=024000717200");

    doIniLine ("set cpu3 config=port=A");
    doIniLine ("set cpu3   config=assignment=0");
    doIniLine ("set cpu3   config=interlace=0");
    doIniLine ("set cpu3   config=enable=1");
    doIniLine ("set cpu3   config=init_enable=1");
    doIniLine ("set cpu3   config=store_size=4M");
 
    doIniLine ("set cpu3 config=port=B");
    doIniLine ("set cpu3   config=assignment=1");
    doIniLine ("set cpu3   config=interlace=0");
    doIniLine ("set cpu3   config=enable=1");
    doIniLine ("set cpu3   config=init_enable=1");
    doIniLine ("set cpu3   config=store_size=4M");

    doIniLine ("set cpu3 config=port=C");
    doIniLine ("set cpu3   config=assignment=2");
    doIniLine ("set cpu3   config=interlace=0");
    doIniLine ("set cpu3   config=enable=1");
    doIniLine ("set cpu3   config=init_enable=1");
    doIniLine ("set cpu3   config=store_size=4M");

    doIniLine ("set cpu3 config=port=D");
    doIniLine ("set cpu3   config=assignment=3");
    doIniLine ("set cpu3   config=interlace=0");
    doIniLine ("set cpu3   config=enable=1");
    doIniLine ("set cpu3   config=init_enable=1");
    doIniLine ("set cpu3   config=store_size=4M");

    doIniLine ("set cpu3 config=mode=Multics");
    doIniLine ("set cpu3 config=speed=0");

    doIniLine ("set cpu3 config=dis_enable=enable");
    doIniLine ("set cpu3 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu3 config=tro_enable=enable");



    doIniLine ("set cpu4 config=faultbase=Multics");
    doIniLine ("set cpu4 config=num=4");
    doIniLine ("set cpu4 config=data=024000717200");

    doIniLine ("set cpu4 config=port=A");
    doIniLine ("set cpu4   config=assignment=0");
    doIniLine ("set cpu4   config=interlace=0");
    doIniLine ("set cpu4   config=enable=1");
    doIniLine ("set cpu4   config=init_enable=1");
    doIniLine ("set cpu4   config=store_size=4M");
 
    doIniLine ("set cpu4 config=port=B");
    doIniLine ("set cpu4   config=assignment=1");
    doIniLine ("set cpu4   config=interlace=0");
    doIniLine ("set cpu4   config=enable=1");
    doIniLine ("set cpu4   config=init_enable=1");
    doIniLine ("set cpu4   config=store_size=4M");

    doIniLine ("set cpu4 config=port=C");
    doIniLine ("set cpu4   config=assignment=2");
    doIniLine ("set cpu4   config=interlace=0");
    doIniLine ("set cpu4   config=enable=1");
    doIniLine ("set cpu4   config=init_enable=1");
    doIniLine ("set cpu4   config=store_size=4M");

    doIniLine ("set cpu4 config=port=D");
    doIniLine ("set cpu4   config=assignment=3");
    doIniLine ("set cpu4   config=interlace=0");
    doIniLine ("set cpu4   config=enable=1");
    doIniLine ("set cpu4   config=init_enable=1");
    doIniLine ("set cpu4   config=store_size=4M");

    doIniLine ("set cpu4 config=mode=Multics");
    doIniLine ("set cpu4 config=speed=0");

    doIniLine ("set cpu4 config=dis_enable=enable");
    doIniLine ("set cpu4 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu4 config=tro_enable=enable");



    doIniLine ("set cpu5 config=faultbase=Multics");
    doIniLine ("set cpu5 config=num=5");
    doIniLine ("set cpu5 config=data=024000717200");

    doIniLine ("set cpu5 config=port=A");
    doIniLine ("set cpu5   config=assignment=0");
    doIniLine ("set cpu5   config=interlace=0");
    doIniLine ("set cpu5   config=enable=1");
    doIniLine ("set cpu5   config=init_enable=1");
    doIniLine ("set cpu5   config=store_size=4M");
 
    doIniLine ("set cpu5 config=port=B");
    doIniLine ("set cpu5   config=assignment=1");
    doIniLine ("set cpu5   config=interlace=0");
    doIniLine ("set cpu5   config=enable=1");
    doIniLine ("set cpu5   config=init_enable=1");
    doIniLine ("set cpu5   config=store_size=4M");

    doIniLine ("set cpu5 config=port=C");
    doIniLine ("set cpu5   config=assignment=2");
    doIniLine ("set cpu5   config=interlace=0");
    doIniLine ("set cpu5   config=enable=1");
    doIniLine ("set cpu5   config=init_enable=1");
    doIniLine ("set cpu5   config=store_size=4M");

    doIniLine ("set cpu5 config=port=D");
    doIniLine ("set cpu5   config=assignment=3");
    doIniLine ("set cpu5   config=interlace=0");
    doIniLine ("set cpu5   config=enable=1");
    doIniLine ("set cpu5   config=init_enable=1");
    doIniLine ("set cpu5   config=store_size=4M");

    doIniLine ("set cpu5 config=mode=Multics");
    doIniLine ("set cpu5 config=speed=0");

    doIniLine ("set cpu5 config=dis_enable=enable");
    doIniLine ("set cpu5 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu5 config=tro_enable=enable");



    doIniLine ("set cpu6 config=faultbase=Multics");
    doIniLine ("set cpu6 config=num=6");
    doIniLine ("set cpu6 config=data=024000717200");

    doIniLine ("set cpu6 config=port=A");
    doIniLine ("set cpu6   config=assignment=0");
    doIniLine ("set cpu6   config=interlace=0");
    doIniLine ("set cpu6   config=enable=1");
    doIniLine ("set cpu6   config=init_enable=1");
    doIniLine ("set cpu6   config=store_size=4M");
 
    doIniLine ("set cpu6 config=port=B");
    doIniLine ("set cpu6   config=assignment=1");
    doIniLine ("set cpu6   config=interlace=0");
    doIniLine ("set cpu6   config=enable=1");
    doIniLine ("set cpu6   config=init_enable=1");
    doIniLine ("set cpu6   config=store_size=4M");

    doIniLine ("set cpu6 config=port=C");
    doIniLine ("set cpu6   config=assignment=2");
    doIniLine ("set cpu6   config=interlace=0");
    doIniLine ("set cpu6   config=enable=1");
    doIniLine ("set cpu6   config=init_enable=1");
    doIniLine ("set cpu6   config=store_size=4M");

    doIniLine ("set cpu6 config=port=D");
    doIniLine ("set cpu6   config=assignment=3");
    doIniLine ("set cpu6   config=interlace=0");
    doIniLine ("set cpu6   config=enable=1");
    doIniLine ("set cpu6   config=init_enable=1");
    doIniLine ("set cpu6   config=store_size=4M");

    doIniLine ("set cpu6 config=mode=Multics");
    doIniLine ("set cpu6 config=speed=0");

    doIniLine ("set cpu6 config=dis_enable=enable");
    doIniLine ("set cpu6 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu6 config=tro_enable=enable");



    doIniLine ("set cpu7 config=faultbase=Multics");
    doIniLine ("set cpu7 config=num=7");
    doIniLine ("set cpu7 config=data=024000717200");

    doIniLine ("set cpu7 config=port=A");
    doIniLine ("set cpu7   config=assignment=0");
    doIniLine ("set cpu7   config=interlace=0");
    doIniLine ("set cpu7   config=enable=1");
    doIniLine ("set cpu7   config=init_enable=1");
    doIniLine ("set cpu7   config=store_size=4M");
 
    doIniLine ("set cpu7 config=port=B");
    doIniLine ("set cpu7   config=assignment=1");
    doIniLine ("set cpu7   config=interlace=0");
    doIniLine ("set cpu7   config=enable=1");
    doIniLine ("set cpu7   config=init_enable=1");
    doIniLine ("set cpu7   config=store_size=4M");

    doIniLine ("set cpu7 config=port=C");
    doIniLine ("set cpu7   config=assignment=2");
    doIniLine ("set cpu7   config=interlace=0");
    doIniLine ("set cpu7   config=enable=1");
    doIniLine ("set cpu7   config=init_enable=1");
    doIniLine ("set cpu7   config=store_size=4M");

    doIniLine ("set cpu7 config=port=D");
    doIniLine ("set cpu7   config=assignment=3");
    doIniLine ("set cpu7   config=interlace=0");
    doIniLine ("set cpu7   config=enable=1");
    doIniLine ("set cpu7   config=init_enable=1");
    doIniLine ("set cpu7   config=store_size=4M");

    doIniLine ("set cpu7 config=mode=Multics");
    doIniLine ("set cpu7 config=speed=0");

    doIniLine ("set cpu7 config=dis_enable=enable");
    doIniLine ("set cpu7 config=halt_on_unimplemented=disable");
    doIniLine ("set cpu7 config=tro_enable=enable");




    doIniLine ("set iom0 config=iom_base=Multics");
    doIniLine ("set iom0 config=multiplex_base=0120");
    doIniLine ("set iom0 config=os=Multics");
    doIniLine ("set iom0 config=boot=tape");
    doIniLine ("set iom0 config=tapechan=012");
    doIniLine ("set iom0 config=cardchan=011");
    doIniLine ("set iom0 config=scuport=0");

    doIniLine ("set iom0 config=port=0");
    doIniLine ("set iom0   config=addr=0");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=1");
    doIniLine ("set iom0   config=addr=1");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=2");
    doIniLine ("set iom0   config=addr=2");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=3");
    doIniLine ("set iom0   config=addr=3");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=4");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom0 config=port=5");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom0 config=port=6");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom0 config=port=7");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom1 config=iom_base=Multics2");
    doIniLine ("set iom1 config=multiplex_base=0121");
    doIniLine ("set iom1 config=os=Multics");
    doIniLine ("set iom1 config=boot=tape");
    doIniLine ("set iom1 config=tapechan=012");
    doIniLine ("set iom1 config=cardchan=011");
    doIniLine ("set iom1 config=scuport=0");

    doIniLine ("set iom1 config=port=0");
    doIniLine ("set iom1   config=addr=0");
    doIniLine ("set iom1   config=interlace=0");
    doIniLine ("set iom1   config=enable=1");
    doIniLine ("set iom1   config=initenable=0");
    doIniLine ("set iom1   config=halfsize=0;");

    doIniLine ("set iom1 config=port=1");
    doIniLine ("set iom1   config=addr=1");
    doIniLine ("set iom1   config=interlace=0");
    doIniLine ("set iom1   config=enable=1");
    doIniLine ("set iom1   config=initenable=0");
    doIniLine ("set iom1   config=halfsize=0;");

    doIniLine ("set iom1 config=port=2");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=3");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=4");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=5");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=6");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=7");
    doIniLine ("set iom1   config=enable=0");

    // ;echo
    // ;show iom0 config
    // ;echo
    // ;show iom1 config
    // ;echo

    doIniLine ("set scu0 config=mode=program");
    doIniLine ("set scu0 config=port0=enable");
    doIniLine ("set scu0 config=port1=enable");
    doIniLine ("set scu0 config=port2=enable");
    doIniLine ("set scu0 config=port3=enable");
    doIniLine ("set scu0 config=port4=enable");
    doIniLine ("set scu0 config=port5=enable");
    doIniLine ("set scu0 config=port6=enable");
    doIniLine ("set scu0 config=port7=enable");
    doIniLine ("set scu0 config=maska=7");
    doIniLine ("set scu0 config=maskb=off");
    doIniLine ("set scu0 config=lwrstoresize=7");
    doIniLine ("set scu0 config=cyclic=0040");
    doIniLine ("set scu0 config=nea=0200");
    doIniLine ("set scu0 config=onl=014");
    doIniLine ("set scu0 config=int=0");
    doIniLine ("set scu0 config=lwr=0");

    doIniLine ("set scu1 config=mode=program");
    doIniLine ("set scu1 config=port0=enable");
    doIniLine ("set scu1 config=port1=enable");
    doIniLine ("set scu1 config=port2=enable");
    doIniLine ("set scu1 config=port3=enable");
    doIniLine ("set scu1 config=port4=enable");
    doIniLine ("set scu1 config=port5=enable");
    doIniLine ("set scu1 config=port6=enable");
    doIniLine ("set scu1 config=port7=enable");
    doIniLine ("set scu1 config=maska=off");
    doIniLine ("set scu1 config=maskb=off");
    doIniLine ("set scu1 config=lwrstoresize=7");
    doIniLine ("set scu1 config=cyclic=0040");
    doIniLine ("set scu1 config=nea=0200");
    doIniLine ("set scu1 config=onl=014");
    doIniLine ("set scu1 config=int=0");
    doIniLine ("set scu1 config=lwr=0");

    doIniLine ("set scu2 config=mode=program");
    doIniLine ("set scu2 config=port0=enable");
    doIniLine ("set scu2 config=port1=enable");
    doIniLine ("set scu2 config=port2=enable");
    doIniLine ("set scu2 config=port3=enable");
    doIniLine ("set scu2 config=port4=enable");
    doIniLine ("set scu2 config=port5=enable");
    doIniLine ("set scu2 config=port6=enable");
    doIniLine ("set scu2 config=port7=enable");
    doIniLine ("set scu2 config=maska=off");
    doIniLine ("set scu2 config=maskb=off");
    doIniLine ("set scu2 config=lwrstoresize=7");
    doIniLine ("set scu2 config=cyclic=0040");
    doIniLine ("set scu2 config=nea=0200");
    doIniLine ("set scu2 config=onl=014");
    doIniLine ("set scu2 config=int=0");
    doIniLine ("set scu2 config=lwr=0");

    doIniLine ("set scu3 config=mode=program");
    doIniLine ("set scu3 config=port0=enable");
    doIniLine ("set scu3 config=port1=enable");
    doIniLine ("set scu3 config=port2=enable");
    doIniLine ("set scu3 config=port3=enable");
    doIniLine ("set scu3 config=port4=enable");
    doIniLine ("set scu3 config=port5=enable");
    doIniLine ("set scu3 config=port6=enable");
    doIniLine ("set scu3 config=port7=enable");
    doIniLine ("set scu3 config=maska=off");
    doIniLine ("set scu3 config=maskb=off");
    doIniLine ("set scu3 config=lwrstoresize=7");
    doIniLine ("set scu3 config=cyclic=0040");
    doIniLine ("set scu3 config=nea=0200");
    doIniLine ("set scu3 config=onl=014");
    doIniLine ("set scu3 config=int=0");
    doIniLine ("set scu3 config=lwr=0");

    // ; There are bugs in the FNP code that require sim unit number
    // ; to be the same as the Multics unit number; ie fnp0 == fnpa, etc.
    // ;
    // ; fnp a 3400
    // ; fnp b 3700
    // ; fnp c 4200
    // ; fnp d 4500
    // ; fnp e 5000
    // ; fnp f 5300
    // ; fnp g 5600
    // ; fnp h 6100

    doIniLine ("set fnp0 config=mailbox=03400");
    doIniLine ("set fnp0 ipc_name=fnp-a");
    doIniLine ("set fnp1 config=mailbox=03700");
    doIniLine ("set fnp1 ipc_name=fnp-b");
    doIniLine ("set fnp2 config=mailbox=04200");
    doIniLine ("set fnp2 ipc_name=fnp-c");
    doIniLine ("set fnp3 config=mailbox=04500");
    doIniLine ("set fnp3 ipc_name=fnp-d");
    doIniLine ("set fnp4 config=mailbox=05000");
    doIniLine ("set fnp4 ipc_name=fnp-e");
    doIniLine ("set fnp5 config=mailbox=05300");
    doIniLine ("set fnp5 ipc_name=fnp-f");
    doIniLine ("set fnp6 config=mailbox=05600");
    doIniLine ("set fnp6 ipc_name=fnp-g");
    doIniLine ("set fnp7 config=mailbox=06100");
    doIniLine ("set fnp7 ipc_name=fnp-h");


    // ;echo
    // ;show scu0 config
    // ;echo
    // ;show scu1 config
    // ;echo
    // ;echo
    // ;show fnp0 config

    doIniLine ("set tape0 boot_drive");

    // ;cable ripout

    // ; Attach tape MPC to IOM 0, chan 012, dev_code 0
    doIniLine ("cable tape,0,0,012,0");
    doIniLine ("set tape0 device_name=mpca");
    // ; Attach TAPE unit 0 to IOM 0, chan 012, dev_code 1
    doIniLine ("cable tape,1,0,012,1");
    doIniLine ("set tape1 device_name=tapa_01");
    doIniLine ("cable tape,2,0,012,2");
    doIniLine ("set tape2 device_name=tapa_02");
    doIniLine ("cable tape,3,0,012,3");
    doIniLine ("set tape3 device_name=tapa_03");
    doIniLine ("cable tape,4,0,012,4");
    doIniLine ("set tape4 device_name=tapa_04");
    doIniLine ("cable tape,5,0,012,5");
    doIniLine ("set tape5 device_name=tapa_05");
    doIniLine ("cable tape,6,0,012,6");
    doIniLine ("set tape6 device_name=tapa_06");
    doIniLine ("cable tape,7,0,012,7");
    doIniLine ("set tape7 device_name=tapa_07");
    doIniLine ("cable tape,8,0,012,8");
    doIniLine ("set tape8 device_name=tapa_08");
    doIniLine ("cable tape,9,0,012,9");
    doIniLine ("set tape9 device_name=tapa_09");
    doIniLine ("cable tape,10,0,012,10");
    doIniLine ("set tape10 device_name=tapa_10");
    doIniLine ("cable tape,11,0,012,11");
    doIniLine ("set tape11 device_name=tapa_11");
    doIniLine ("cable tape,12,0,012,12");
    doIniLine ("set tape12 device_name=tapa_12");
    doIniLine ("cable tape,13,0,012,13");
    doIniLine ("set tape13 device_name=tapa_13");
    doIniLine ("cable tape,14,0,012,14");
    doIniLine ("set tape14 device_name=tapa_14");
    doIniLine ("cable tape,15,0,012,15");
    doIniLine ("set tape15 device_name=tapa_15");
    doIniLine ("cable tape,16,0,012,16");
    doIniLine ("set tape16 device_name=tapa_16");

    // ; Attach DISK unit 0 to IOM 0, chan 013, dev_code 0");
    doIniLine ("cable disk,0,0,013,0");
    // ; Attach DISK unit 1 to IOM 0, chan 013, dev_code 1");
    doIniLine ("cable disk,1,0,013,1");
    // ; Attach DISK unit 2 to IOM 0, chan 013, dev_code 2");
    doIniLine ("cable disk,2,0,013,2");
    // ; Attach DISK unit 3 to IOM 0, chan 013, dev_code 3");
    doIniLine ("cable disk,3,0,013,3");
    // ; Attach DISK unit 4 to IOM 0, chan 013, dev_code 4");
    doIniLine ("cable disk,4,0,013,4");
    // ; Attach DISK unit 5 to IOM 0, chan 013, dev_code 5");
    doIniLine ("cable disk,5,0,013,5");
    // ; Attach DISK unit 6 to IOM 0, chan 013, dev_code 6");
    doIniLine ("cable disk,6,0,013,6");
    // ; Attach DISK unit 7 to IOM 0, chan 013, dev_code 7");
    doIniLine ("cable disk,7,0,013,7");
    // ; Attach DISK unit 8 to IOM 0, chan 013, dev_code 8");
    doIniLine ("cable disk,8,0,013,8");
    // ; Attach DISK unit 9 to IOM 0, chan 013, dev_code 9");
    doIniLine ("cable disk,9,0,013,9");
    // ; Attach DISK unit 10 to IOM 0, chan 013, dev_code 10");
    doIniLine ("cable disk,10,0,013,10");
    // ; Attach DISK unit 11 to IOM 0, chan 013, dev_code 11");
    doIniLine ("cable disk,11,0,013,11");
    // ; Attach DISK unit 12 to IOM 0, chan 013, dev_code 12");
    doIniLine ("cable disk,12,0,013,12");
    // ; Attach DISK unit 13 to IOM 0, chan 013, dev_code 13");
    doIniLine ("cable disk,13,0,013,13");
    // ; Attach DISK unit 14 to IOM 0, chan 013, dev_code 14");
    doIniLine ("cable disk,14,0,013,14");
    // ; Attach DISK unit 15 to IOM 0, chan 013, dev_code 15");
    doIniLine ("cable disk,15,0,013,15");

    // ; Attach OPCON unit 0 to IOM A, chan 036, dev_code 0
    doIniLine ("cable opcon,0,0,036,0");

    // ;;;
    // ;;; FNP
    // ;;;

    // ; Attach FNP unit 3 (d) to IOM A, chan 020, dev_code 0
    doIniLine ("cable fnp,3,0,020,0");

    // ; Attach FNP unit 0 (a) to IOM A, chan 021, dev_code 0
    doIniLine ("cable fnp,0,0,021,0");

    // ; Attach FNP unit 1 (b) to IOM A, chan 022, dev_code 0
    doIniLine ("cable fnp,1,0,022,0");

    // ; Attach FNP unit 2 (c) to IOM A, chan 023, dev_code 0
    doIniLine ("cable fnp,2,0,023,0");

    // ; Attach FNP unit 4 (e) to IOM A, chan 024, dev_code 0
    doIniLine ("cable fnp,4,0,024,0");

    // ; Attach FNP unit 5 (f) to IOM A, chan 025, dev_code 0
    doIniLine ("cable fnp,5,0,025,0");

    // ; Attach FNP unit 6 (g) to IOM A, chan 026, dev_code 0
    doIniLine ("cable fnp,6,0,026,0");

    // ; Attach FNP unit 7 (h) to IOM A, chan 027, dev_code 0
    doIniLine ("cable fnp,7,0,027,0");

    // ;;;
    // ;;; MPC
    // ;;;

    // ; Attach MPC unit 0 to IOM 0, char 015, dev_code 0
    doIniLine ("cable urp,0,0,015, 0");
    doIniLine ("set urp0 device_name=urpa");

    // ; Attach CRDRDR unit 0 to IOM 0, chan 015, dev_code 1
    doIniLine ("cable crdrdr,0,0,015,1");
    doIniLine ("set crdrdr0 device_name=rdra");

    // ; Attach MPC unit 1 to IOM 0, char 016, dev_code 0
    doIniLine ("cable urp,1,0,016, 0");
    doIniLine ("set urp1 device_name=urpb");

    // ; Attach CRDPUN unit 0 to IOM 0, chan 016, dev_code 1
    doIniLine ("cable crdpun,0,0,016,1");
    doIniLine ("set crdpun0 device_name=puna");

    // ; Attach MPC unit 2 to IOM 0, char 017, dev_code 0
    doIniLine ("cable urp,2,0,017,0");
    doIniLine ("set urp2 device_name=urpc");

    // ; Attach PRT unit 0 to IOM 0, chan 017, dev_code 1
    doIniLine ("cable prt,0,0,017,1");
    doIniLine ("set prt0 device_name=prta");

    // ; Attach PRT unit 1 to IOM 0, chan 017, dev_code 2
    doIniLine ("cable prt,1,0,017,2");
    doIniLine ("set prt1 device_name=prtb");

    // ; Attach PRT unit 2 to IOM 0, chan 017, dev_code 3
    doIniLine ("cable prt,2,0,017,3");
    doIniLine ("set prt2 device_name=prtc");

    // ; Attach PRT unit 3 to IOM 0, chan 017, dev_code 4
    doIniLine ("cable prt,3,0,017,4");
    doIniLine ("set prt3 device_name=prtd");

    // ; Attach PRT unit 4 to IOM 0, chan 017, dev_code 5
    doIniLine ("cable prt,4,0,017,5");
    doIniLine ("set prt4 device_name=prte");

    // ; Attach PRT unit 5 to IOM 0, chan 017, dev_code 6
    doIniLine ("cable prt,5,0,017,6");
    doIniLine ("set prt5 device_name=prtf");

    // ; Attach PRT unit 6 to IOM 0, chan 017, dev_code 7
    doIniLine ("cable prt,6,0,017,7");
    doIniLine ("set prt6 device_name=prtg");

    // ; Attach PRT unit 7 to IOM 0, chan 017, dev_code 8
    doIniLine ("cable prt,7,0,017,8");
    doIniLine ("set prt7 device_name=prth");

    // ; Attach PRT unit 8 to IOM 0, chan 017, dev_code 9
    doIniLine ("cable prt,8,0,017,9");
    doIniLine ("set prt8 device_name=prti");

    // ; Attach PRT unit 9 to IOM 0, chan 017, dev_code 10
    doIniLine ("cable prt,9,0,017,10");
    doIniLine ("set prt9 device_name=prtj");

    // ; Attach PRT unit 10 to IOM 0, chan 017, dev_code 11
    doIniLine ("cable prt,10,0,017,11");
    doIniLine ("set prt10 device_name=prtk");

    // ; Attach PRT unit 11 to IOM 0, chan 017, dev_code 12
    doIniLine ("cable prt,11,0,017,12");
    doIniLine ("set prt11 device_name=prtl");

    // ; Attach PRT unit 12 to IOM 0, chan 017, dev_code 13
    doIniLine ("cable prt,12,0,017,13");
    doIniLine ("set prt12 device_name=prtm");

    // ; Attach PRT unit 13 to IOM 0, chan 017, dev_code 14
    doIniLine ("cable prt,13,0,017,14");
    doIniLine ("set prt13 device_name=prtn");

    // ; Attach PRT unit 14 to IOM 0, chan 017, dev_code 15
    doIniLine ("cable prt,14,0,017,15");
    doIniLine ("set prt14 device_name=prto");

    // ; Attach PRT unit 15 to IOM 0, chan 017, dev_code 16
    doIniLine ("cable prt,15,0,017,16");
    doIniLine ("set prt15 device_name=prtp");

    // ; Attach PRT unit 16 to IOM 0, chan 017, dev_code 17
    doIniLine ("cable prt,16,0,017,17");
    doIniLine ("set prt16 device_name=prtq");


    // ; Attach ABSI unit 0 to IOM 0, chan 032, dev_code 0
    doIniLine ("cable absi,0,0,032,0");


    // ; Attach IOM unit 0 port A (0) to SCU unit 0, port 0
    doIniLine ("cable iom,0,0,0,0");

    // ; Attach IOM unit 0 port B (1) to SCU unit 1, port 0
    doIniLine ("cable iom,0,1,1,0");

    // ; Attach IOM unit 0 port C (2) to SCU unit 2, port 0
    doIniLine ("cable iom,0,2,2,0");

    // ; Attach IOM unit 0 port D (3) to SCU unit 3, port 0
    doIniLine ("cable iom,0,3,3,0");

    // ; Attach IOM unit 1 port A (0) to SCU unit 0, port 1
    doIniLine ("cable iom,1,0,0,1");

    // ; Attach IOM unit 1 port B (1) to SCU unit 1, port 1
    doIniLine ("cable iom,1,1,1,1");

    // ; Attach IOM unit 1 port C (2) to SCU unit 2, port 1
    doIniLine ("cable iom,1,2,2,1");

    // ; Attach IOM unit 1 port D (3) to SCU unit 3, port 1
    doIniLine ("cable iom,1,3,3,1");


    // ;;;
    // ;;; SCU 0 --> CPUs
    // ;;;

    // ; Attach SCU unit 0 port 7 to CPU unit A (0), port 0
    doIniLine ("cable scu,0,7,0,0");

    // ; Attach SCU unit 0 port 6 to CPU unit B (1), port 0
    doIniLine ("cable scu,0,6,1,0");

    // ; Attach SCU unit 0 port 5 to CPU unit C (2), port 0
    doIniLine ("cable scu,0,5,2,0");

    // ; Attach SCU unit 0 port 4 to CPU unit D (3), port 0
    doIniLine ("cable scu,0,4,3,0");

    // ;;;
    // ;;; SCU 1 --> CPUs
    // ;;;

    // ; Attach SCU unit 1 port 7 to CPU unit A (0), port 1
    doIniLine ("cable scu,1,7,0,1");

    // ; Attach SCU unit 1 port 6 to CPU unit B (1), port 1
    doIniLine ("cable scu,1,6,1,1");

    // ; Attach SCU unit 1 port 5 to CPU unit C (2), port 1
    doIniLine ("cable scu,1,5,2,1");

    // ; Attach SCU unit 1 port 4 to CPU unit D (3), port 1
    doIniLine ("cable scu,1,4,3,1");


    // ;;;
    // ;;; SCU 2 --> CPUs
    // ;;;

    // ; Attach SCU unit 2 port 7 to CPU unit A (0), port 2
    doIniLine ("cable scu,2,7,0,2");

    // ; Attach SCU unit 2 port 6 to CPU unit B (1), port 2
    doIniLine ("cable scu,2,6,1,2");

    // ; Attach SCU unit 2 port 5 to CPU unit C (2), port 2
    doIniLine ("cable scu,2,5,2,2");

    // ; Attach SCU unit 2 port 4 to CPU unit D (3), port 2
    doIniLine ("cable scu,2,4,3,2");

    // ;;;
    // ;;; SCU 3 --> CPUs
    // ;;;

    // ; Attach SCU unit 3 port 7 to CPU unit A (0), port 3
    doIniLine ("cable scu,3,7,0,3");

    // ; Attach SCU unit 3 port 6 to CPU unit B (1), port 3
    doIniLine ("cable scu,3,6,1,3");

    // ; Attach SCU unit 3 port 5 to CPU unit C (2), port 3
    doIniLine ("cable scu,3,5,2,3");

    // ; Attach SCU unit 3 port 4 to CPU unit D (3), port 3
    doIniLine ("cable scu,3,4,3,3");

    // ;cable show
    // ;cable verify





    doIniLine ("fnpload Devices.txt");
    doIniLine ("fnpserverport 6180");

    return SCPE_OK;
  }


static t_stat bootSkip (int32 UNUSED arg, const char * UNUSED buf)
  {
    uint32 skipped;
    return sim_tape_sprecsf (& mt_unit [0], 1, & skipped);
  }
  
static t_stat setDbgCPUMask (int32 UNUSED arg, const char * UNUSED buf)
  {
    uint msk;
    int cnt = sscanf (buf, "%u", & msk);
    if (cnt != 1)
      {
        sim_printf ("Huh?\n");
        return SCPE_ARG;
      }
    sim_printf ("mask set to %u\n", msk);
    dbgCPUMask = msk;
    return SCPE_OK;
  }
  

// This is part of the simh interface
t_stat sim_load (UNUSED FILE *fileref, UNUSED const char *cptr, UNUSED const char *fnam, UNUSED int flag)
  {
    return SCPE_IERR;
  }

