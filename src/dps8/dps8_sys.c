/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2017 by Charles Anthony
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
#include <ctype.h>

#ifdef __APPLE__
#include <pthread.h>
#endif

#include "dps8.h"
#include "dps8_console.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_ins.h"
#include "dps8_loader.h"
#include "dps8_math.h"
#include "dps8_mt.h"
#include "dps8_disk.h"
#include "dps8_append.h"
#include "dps8_fnp2.h"
#include "dps8_crdrdr.h"
#include "dps8_crdpun.h"
#include "dps8_prt.h"
#include "dps8_urp.h"
#include "dps8_absi.h"
#include "utlist.h"
#if defined(THREADZ) || defined(LOCKLESS)
#include "threadz.h"
#endif

#ifdef PANEL
#include "panelScraper.h"
#endif

#define DBG_CTR 1

#define ASSUME0 0

// Strictly speaking, memory belongs in the SCU.
// We will treat memory as viewed from the CPU and elide the
// SCU configuration that maps memory across multiple SCUs.
// I would guess that multiple SCUs helped relieve memory
// contention across multiple CPUs, but that is a level of
// emulation that will be ignored.
// Building with SCUMEM defined puts the memory in the SCUs.

#ifndef SCUMEM
word36 vol * M = NULL;                                          // memory
#endif

#ifdef TEST_OLIN
int64_t cmpxchg_data;
#endif

#ifdef TEST_FENCE
pthread_mutex_t fenceLock = PTHREAD_MUTEX_INITIALIZER;
#endif
//
// These are part of the simh interface
//

#ifdef DPS8M
char sim_name[] = "DPS8/M";
#endif
#ifdef L68
char sim_name[] = "L68";
#endif
int32 sim_emax = 4; // some EIS can take up to 4-words
static void dps8_init(void);
void (*sim_vm_init) (void) = & dps8_init;    // CustomCmds;

#ifdef TESTING
static t_addr parse_addr(DEVICE *dptr, const char *cptr, const char **optr);
static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr addr);
#endif // TESTING

//
// Session ID is used for shared memory name identification
//

#ifndef __MINGW64__
static pid_t dps8m_sid; // Session id
#endif

//
// simh hooks
//

#ifndef LOADER
// sim_load not supported
t_stat sim_load (FILE *fileref, const char *cptr, const char *fnam, int flag)
  {
    return SCP_UNK;
  }
#endif

////////////////////////////////////////////////////////////////////////////////
//
// simh Commands
//

// 
// System configuration commands
//


// Script to string cables and set switches

static char * default_base_system_script [] =
  {
    // ;
    // ; Configure test system
    // ;
    // ; CPU, IOM * 2, MPC, TAPE * 16, DISK * 16, SCU * 4, OPC, FNP, URP * 3,
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

//
// IOM channel assignments
//
// IOM A
//  
//  012 MTP0
//  013 IPC0 port 0
//  014 MSP0 port 0
//  015 URP0
//  016 URP1
//  017 URP2
//  020 FNPD
//  021 FNPA
//  022 FNPB
//  023 FNPC
//  024 FNPE
//  025 FNPF
//  026 FNPG
//  027 FNPH
//  032 ABSI0
//  036 OPC0
//
// IOM B
//
//  013 IPC0 port 1
//  014 MSP0 port 1

    // ; Disconnect everything...
    "cable_ripout",

    "set cpu nunits=6",
    "set iom nunits=2",
    // ; 16 drives plus a placeholder for the controller
    "set tape nunits=17",
    "set mtp nunits=1",
    // ; 4 3381 drives; 2 controllers
    // ; 4 d501 drives; 2 controller
    // ; 4 d451 drives; same controller has d501s
    "set ipc nunits=2",
    "set msp nunits=2",
    "set disk nunits=12",
    "set scu nunits=4",
    "set opc nunits=1",
    "set fnp nunits=8",
    "set urp nunits=3",
    "set rdr nunits=1",
    "set pun nunits=1",
    "set prt nunits=1",
    "set absi nunits=1",

#if 0
#ifndef __MINGW64__

    // ;Create card reader queue directory
    "! if [ ! -e /tmp/rdra ]; then mkdir /tmp/rdra; fi",
#else
    "! mkdir %TEMP%\\rdra",
#endif
#endif

// CPU0

    "set cpu0 config=faultbase=Multics",

    "set cpu0 config=num=0",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu0 config=data=024000717200",

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

    "set cpu0 config=port=A",
    "set cpu0   config=assignment=0",
    "set cpu0   config=interlace=0",
    "set cpu0   config=enable=1",
    "set cpu0   config=init_enable=1",
    "set cpu0   config=store_size=4M",
 
    "set cpu0 config=port=B",
    "set cpu0   config=assignment=1",
    "set cpu0   config=interlace=0",
    "set cpu0   config=enable=1",
    "set cpu0   config=init_enable=1",
    "set cpu0   config=store_size=4M",

    "set cpu0 config=port=C",
    "set cpu0   config=assignment=2",
    "set cpu0   config=interlace=0",
    "set cpu0   config=enable=1",
    "set cpu0   config=init_enable=1",
    "set cpu0   config=store_size=4M",

    "set cpu0 config=port=D",
    "set cpu0   config=assignment=3",
    "set cpu0   config=interlace=0",
    "set cpu0   config=enable=1",
    "set cpu0   config=init_enable=1",
    "set cpu0   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu0 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu0 config=speed=0",

    "set cpu0 config=dis_enable=enable",
    "set cpu0 config=steady_clock=disable",
    "set cpu0 config=halt_on_unimplemented=disable",
    "set cpu0 config=disable_wam=disable",
    "set cpu0 config=tro_enable=enable",
    "set cpu0 config=y2k=disable",

// CPU1

    "set cpu1 config=faultbase=Multics",

    "set cpu1 config=num=1",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu1 config=data=024000717200",

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

    "set cpu1 config=port=A",
    "set cpu1   config=assignment=0",
    "set cpu1   config=interlace=0",
    "set cpu1   config=enable=1",
    "set cpu1   config=init_enable=1",
    "set cpu1   config=store_size=4M",
 
    "set cpu1 config=port=B",
    "set cpu1   config=assignment=1",
    "set cpu1   config=interlace=0",
    "set cpu1   config=enable=1",
    "set cpu1   config=init_enable=1",
    "set cpu1   config=store_size=4M",

    "set cpu1 config=port=C",
    "set cpu1   config=assignment=2",
    "set cpu1   config=interlace=0",
    "set cpu1   config=enable=1",
    "set cpu1   config=init_enable=1",
    "set cpu1   config=store_size=4M",

    "set cpu1 config=port=D",
    "set cpu1   config=assignment=3",
    "set cpu1   config=interlace=0",
    "set cpu1   config=enable=1",
    "set cpu1   config=init_enable=1",
    "set cpu1   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu1 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu1 config=speed=0",

    "set cpu1 config=dis_enable=enable",
    "set cpu1 config=steady_clock=disable",
    "set cpu1 config=halt_on_unimplemented=disable",
    "set cpu1 config=disable_wam=disable",
    "set cpu1 config=tro_enable=enable",
    "set cpu1 config=y2k=disable",


// CPU2

    "set cpu2 config=faultbase=Multics",

    "set cpu2 config=num=2",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu2 config=data=024000717200",

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

    "set cpu2 config=port=A",
    "set cpu2   config=assignment=0",
    "set cpu2   config=interlace=0",
    "set cpu2   config=enable=1",
    "set cpu2   config=init_enable=1",
    "set cpu2   config=store_size=4M",
 
    "set cpu2 config=port=B",
    "set cpu2   config=assignment=1",
    "set cpu2   config=interlace=0",
    "set cpu2   config=enable=1",
    "set cpu2   config=init_enable=1",
    "set cpu2   config=store_size=4M",

    "set cpu2 config=port=C",
    "set cpu2   config=assignment=2",
    "set cpu2   config=interlace=0",
    "set cpu2   config=enable=1",
    "set cpu2   config=init_enable=1",
    "set cpu2   config=store_size=4M",

    "set cpu2 config=port=D",
    "set cpu2   config=assignment=3",
    "set cpu2   config=interlace=0",
    "set cpu2   config=enable=1",
    "set cpu2   config=init_enable=1",
    "set cpu2   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu2 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu2 config=speed=0",

    "set cpu2 config=dis_enable=enable",
    "set cpu2 config=steady_clock=disable",
    "set cpu2 config=halt_on_unimplemented=disable",
    "set cpu2 config=disable_wam=disable",
    "set cpu2 config=tro_enable=enable",
    "set cpu2 config=y2k=disable",


// CPU3

    "set cpu3 config=faultbase=Multics",

    "set cpu3 config=num=3",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu3 config=data=024000717200",

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

    "set cpu3 config=port=A",
    "set cpu3   config=assignment=0",
    "set cpu3   config=interlace=0",
    "set cpu3   config=enable=1",
    "set cpu3   config=init_enable=1",
    "set cpu3   config=store_size=4M",
 
    "set cpu3 config=port=B",
    "set cpu3   config=assignment=1",
    "set cpu3   config=interlace=0",
    "set cpu3   config=enable=1",
    "set cpu3   config=init_enable=1",
    "set cpu3   config=store_size=4M",

    "set cpu3 config=port=C",
    "set cpu3   config=assignment=2",
    "set cpu3   config=interlace=0",
    "set cpu3   config=enable=1",
    "set cpu3   config=init_enable=1",
    "set cpu3   config=store_size=4M",

    "set cpu3 config=port=D",
    "set cpu3   config=assignment=3",
    "set cpu3   config=interlace=0",
    "set cpu3   config=enable=1",
    "set cpu3   config=init_enable=1",
    "set cpu3   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu3 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu3 config=speed=0",

    "set cpu3 config=dis_enable=enable",
    "set cpu3 config=steady_clock=disable",
    "set cpu3 config=halt_on_unimplemented=disable",
    "set cpu3 config=disable_wam=disable",
    "set cpu3 config=tro_enable=enable",
    "set cpu3 config=y2k=disable",


// CPU4

    "set cpu4 config=faultbase=Multics",

    "set cpu4 config=num=4",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu4 config=data=024000717200",

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

    "set cpu4 config=port=A",
    "set cpu4   config=assignment=0",
    "set cpu4   config=interlace=0",
    "set cpu4   config=enable=1",
    "set cpu4   config=init_enable=1",
    "set cpu4   config=store_size=4M",
 
    "set cpu4 config=port=B",
    "set cpu4   config=assignment=1",
    "set cpu4   config=interlace=0",
    "set cpu4   config=enable=1",
    "set cpu4   config=init_enable=1",
    "set cpu4   config=store_size=4M",

    "set cpu4 config=port=C",
    "set cpu4   config=assignment=2",
    "set cpu4   config=interlace=0",
    "set cpu4   config=enable=1",
    "set cpu4   config=init_enable=1",
    "set cpu4   config=store_size=4M",

    "set cpu4 config=port=D",
    "set cpu4   config=assignment=3",
    "set cpu4   config=interlace=0",
    "set cpu4   config=enable=1",
    "set cpu4   config=init_enable=1",
    "set cpu4   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu4 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu4 config=speed=0",

    "set cpu4 config=dis_enable=enable",
    "set cpu4 config=steady_clock=disable",
    "set cpu4 config=halt_on_unimplemented=disable",
    "set cpu4 config=disable_wam=disable",
    "set cpu4 config=tro_enable=enable",
    "set cpu4 config=y2k=disable",


// CPU5

    "set cpu5 config=faultbase=Multics",

    "set cpu5 config=num=5",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu5 config=data=024000717200",

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

    "set cpu5 config=port=A",
    "set cpu5   config=assignment=0",
    "set cpu5   config=interlace=0",
    "set cpu5   config=enable=1",
    "set cpu5   config=init_enable=1",
    "set cpu5   config=store_size=4M",
 
    "set cpu5 config=port=B",
    "set cpu5   config=assignment=1",
    "set cpu5   config=interlace=0",
    "set cpu5   config=enable=1",
    "set cpu5   config=init_enable=1",
    "set cpu5   config=store_size=4M",

    "set cpu5 config=port=C",
    "set cpu5   config=assignment=2",
    "set cpu5   config=interlace=0",
    "set cpu5   config=enable=1",
    "set cpu5   config=init_enable=1",
    "set cpu5   config=store_size=4M",

    "set cpu5 config=port=D",
    "set cpu5   config=assignment=3",
    "set cpu5   config=interlace=0",
    "set cpu5   config=enable=1",
    "set cpu5   config=init_enable=1",
    "set cpu5   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu5 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu5 config=speed=0",

    "set cpu5 config=dis_enable=enable",
    "set cpu5 config=steady_clock=disable",
    "set cpu5 config=halt_on_unimplemented=disable",
    "set cpu5 config=disable_wam=disable",
    "set cpu5 config=tro_enable=enable",
    "set cpu5 config=y2k=disable",


#if 0
// CPU6

    "set cpu6 config=faultbase=Multics",

    "set cpu6 config=num=6",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu6 config=data=024000717200",

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

    "set cpu6 config=port=A",
    "set cpu6   config=assignment=0",
    "set cpu6   config=interlace=0",
    "set cpu6   config=enable=1",
    "set cpu6   config=init_enable=1",
    "set cpu6   config=store_size=4M",
 
    "set cpu6 config=port=B",
    "set cpu6   config=assignment=1",
    "set cpu6   config=interlace=0",
    "set cpu6   config=enable=1",
    "set cpu6   config=init_enable=1",
    "set cpu6   config=store_size=4M",

    "set cpu6 config=port=C",
    "set cpu6   config=assignment=2",
    "set cpu6   config=interlace=0",
    "set cpu6   config=enable=1",
    "set cpu6   config=init_enable=1",
    "set cpu6   config=store_size=4M",

    "set cpu6 config=port=D",
    "set cpu6   config=assignment=3",
    "set cpu6   config=interlace=0",
    "set cpu6   config=enable=1",
    "set cpu6   config=init_enable=1",
    "set cpu6   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu6 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu6 config=speed=0",

    "set cpu6 config=dis_enable=enable",
    "set cpu6 config=steady_clock=disable",
    "set cpu6 config=halt_on_unimplemented=disable",
    "set cpu6 config=disable_wam=disable",
    "set cpu6 config=tro_enable=enable",
    "set cpu6 config=y2k=disable",
#endif

#if 0 // Until the port expander code is working

// CPU7

    "set cpu7 config=faultbase=Multics",

    "set cpu7 config=num=7",
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    "set cpu7 config=data=024000717200",

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

    "set cpu7 config=port=A",
    "set cpu7   config=assignment=0",
    "set cpu7   config=interlace=0",
    "set cpu7   config=enable=1",
    "set cpu7   config=init_enable=1",
    "set cpu7   config=store_size=4M",
 
    "set cpu7 config=port=B",
    "set cpu7   config=assignment=1",
    "set cpu7   config=interlace=0",
    "set cpu7   config=enable=1",
    "set cpu7   config=init_enable=1",
    "set cpu7   config=store_size=4M",

    "set cpu7 config=port=C",
    "set cpu7   config=assignment=2",
    "set cpu7   config=interlace=0",
    "set cpu7   config=enable=1",
    "set cpu7   config=init_enable=1",
    "set cpu7   config=store_size=4M",

    "set cpu7 config=port=D",
    "set cpu7   config=assignment=3",
    "set cpu7   config=interlace=0",
    "set cpu7   config=enable=1",
    "set cpu7   config=init_enable=1",
    "set cpu7   config=store_size=4M",

    // ; 0 = GCOS 1 = VMS
    "set cpu7 config=mode=Multics",
    // ; 0 = 8/70
    "set cpu7 config=speed=0",

    "set cpu7 config=dis_enable=enable",
    "set cpu7 config=steady_clock=disable",
    "set cpu7 config=halt_on_unimplemented=disable",
    "set cpu7 config=disable_wam=disable",
    "set cpu7 config=tro_enable=enable",
    "set cpu7 config=y2k=disable",
#endif


// IOM0

    "set iom0 config=iom_base=Multics",
    "set iom0 config=multiplex_base=0120",
    "set iom0 config=os=Multics",
    "set iom0 config=boot=tape",
    "set iom0 config=tapechan=012",
    "set iom0 config=cardchan=011",
    "set iom0 config=scuport=0",

    "set iom0 config=port=0",
    "set iom0   config=addr=0",
    "set iom0   config=interlace=0",
    "set iom0   config=enable=1",
    "set iom0   config=initenable=0",
    "set iom0   config=halfsize=0",
    "set iom0   config=store_size=4M",

    "set iom0 config=port=1",
    "set iom0   config=addr=1",
    "set iom0   config=interlace=0",
    "set iom0   config=enable=1",
    "set iom0   config=initenable=0",
    "set iom0   config=halfsize=0",
    "set iom0   config=store_size=4M",

    "set iom0 config=port=2",
    "set iom0   config=addr=2",
    "set iom0   config=interlace=0",
    "set iom0   config=enable=1",
    "set iom0   config=initenable=0",
    "set iom0   config=halfsize=0",
    "set iom0   config=store_size=4M",

    "set iom0 config=port=3",
    "set iom0   config=addr=3",
    "set iom0   config=interlace=0",
    "set iom0   config=enable=1",
    "set iom0   config=initenable=0",
    "set iom0   config=halfsize=0",
    "set iom0   config=store_size=4M",

    "set iom0 config=port=4",
    "set iom0   config=enable=0",

    "set iom0 config=port=5",
    "set iom0   config=enable=0",

    "set iom0 config=port=6",
    "set iom0   config=enable=0",

    "set iom0 config=port=7",
    "set iom0   config=enable=0",

// IOM1

    "set iom1 config=iom_base=Multics2",
    "set iom1 config=multiplex_base=0121",
    "set iom1 config=os=Multics",
    "set iom1 config=boot=tape",
    "set iom1 config=tapechan=012",
    "set iom1 config=cardchan=011",
    "set iom1 config=scuport=0",

    "set iom1 config=port=0",
    "set iom1   config=addr=0",
    "set iom1   config=interlace=0",
    "set iom1   config=enable=1",
    "set iom1   config=initenable=0",
    "set iom1   config=halfsize=0;",

    "set iom1 config=port=1",
    "set iom1   config=addr=1",
    "set iom1   config=interlace=0",
    "set iom1   config=enable=1",
    "set iom1   config=initenable=0",
    "set iom1   config=halfsize=0;",

    "set iom1 config=port=2",
    "set iom1   config=enable=0",
    "set iom1 config=port=3",
    "set iom1   config=enable=0",
    "set iom1 config=port=4",
    "set iom1   config=enable=0",
    "set iom1 config=port=5",
    "set iom1   config=enable=0",
    "set iom1 config=port=6",
    "set iom1   config=enable=0",
    "set iom1 config=port=7",
    "set iom1   config=enable=0",

#if 0

// IOM2

    "set iom2 config=iom_base=Multics2",
    "set iom2 config=multiplex_base=0121",
    "set iom2 config=os=Multics",
    "set iom2 config=boot=tape",
    "set iom2 config=tapechan=012",
    "set iom2 config=cardchan=011",
    "set iom2 config=scuport=0",

    "set iom2 config=port=0",
    "set iom2   config=addr=0",
    "set iom2   config=interlace=0",
    "set iom2   config=enable=1",
    "set iom2   config=initenable=0",
    "set iom2   config=halfsize=0;",

    "set iom2 config=port=1",
    "set iom2   config=addr=1",
    "set iom2   config=interlace=0",
    "set iom2   config=enable=1",
    "set iom2   config=initenable=0",
    "set iom2   config=halfsize=0;",

    "set iom2 config=port=2",
    "set iom2   config=enable=0",
    "set iom2 config=port=3",
    "set iom2   config=enable=0",
    "set iom2 config=port=4",
    "set iom2   config=enable=0",
    "set iom2 config=port=5",
    "set iom2   config=enable=0",
    "set iom2 config=port=6",
    "set iom2   config=enable=0",
    "set iom2 config=port=7",
    "set iom2   config=enable=0",


// IOM3

    "set iom3 config=iom_base=Multics2",
    "set iom3 config=multiplex_base=0121",
    "set iom3 config=os=Multics",
    "set iom3 config=boot=tape",
    "set iom3 config=tapechan=012",
    "set iom3 config=cardchan=011",
    "set iom3 config=scuport=0",

    "set iom3 config=port=0",
    "set iom3   config=addr=0",
    "set iom3   config=interlace=0",
    "set iom3   config=enable=1",
    "set iom3   config=initenable=0",
    "set iom3   config=halfsize=0;",

    "set iom3 config=port=1",
    "set iom3   config=addr=1",
    "set iom3   config=interlace=0",
    "set iom3   config=enable=1",
    "set iom3   config=initenable=0",
    "set iom3   config=halfsize=0;",

    "set iom3 config=port=2",
    "set iom3   config=enable=0",
    "set iom3 config=port=3",
    "set iom3   config=enable=0",
    "set iom3 config=port=4",
    "set iom3   config=enable=0",
    "set iom3 config=port=5",
    "set iom3   config=enable=0",
    "set iom3 config=port=6",
    "set iom3   config=enable=0",
    "set iom3 config=port=7",
    "set iom3   config=enable=0",
#endif

// SCU0

    "set scu0 config=mode=program",
    "set scu0 config=port0=enable",
    "set scu0 config=port1=enable",
    "set scu0 config=port2=enable",
    "set scu0 config=port3=enable",
    "set scu0 config=port4=enable",
    "set scu0 config=port5=enable",
    "set scu0 config=port6=enable",
    "set scu0 config=port7=enable",
    "set scu0 config=maska=7",
    "set scu0 config=maskb=off",
    "set scu0 config=lwrstoresize=7",
    "set scu0 config=cyclic=0040",
    "set scu0 config=nea=0200",
    "set scu0 config=onl=014",
    "set scu0 config=int=0",
    "set scu0 config=lwr=0",

// SCU1

    "set scu1 config=mode=program",
    "set scu1 config=port0=enable",
    "set scu1 config=port1=enable",
    "set scu1 config=port2=enable",
    "set scu1 config=port3=enable",
    "set scu1 config=port4=enable",
    "set scu1 config=port5=enable",
    "set scu1 config=port6=enable",
    "set scu1 config=port7=enable",
    "set scu1 config=maska=off",
    "set scu1 config=maskb=off",
    "set scu1 config=lwrstoresize=7",
    "set scu1 config=cyclic=0040",
    "set scu1 config=nea=0200",
    "set scu1 config=onl=014",
    "set scu1 config=int=0",
    "set scu1 config=lwr=0",

// SCU2

    "set scu2 config=mode=program",
    "set scu2 config=port0=enable",
    "set scu2 config=port1=enable",
    "set scu2 config=port2=enable",
    "set scu2 config=port3=enable",
    "set scu2 config=port4=enable",
    "set scu2 config=port5=enable",
    "set scu2 config=port6=enable",
    "set scu2 config=port7=enable",
    "set scu2 config=maska=off",
    "set scu2 config=maskb=off",
    "set scu2 config=lwrstoresize=7",
    "set scu2 config=cyclic=0040",
    "set scu2 config=nea=0200",
    "set scu2 config=onl=014",
    "set scu2 config=int=0",
    "set scu2 config=lwr=0",

// SCU3

    "set scu3 config=mode=program",
    "set scu3 config=port0=enable",
    "set scu3 config=port1=enable",
    "set scu3 config=port2=enable",
    "set scu3 config=port3=enable",
    "set scu3 config=port4=enable",
    "set scu3 config=port5=enable",
    "set scu3 config=port6=enable",
    "set scu3 config=port7=enable",
    "set scu3 config=maska=off",
    "set scu3 config=maskb=off",
    "set scu3 config=lwrstoresize=7",
    "set scu3 config=cyclic=0040",
    "set scu3 config=nea=0200",
    "set scu3 config=onl=014",
    "set scu3 config=int=0",
    "set scu3 config=lwr=0",

#if 0
// SCU4

    "set scu4 config=mode=program",
    "set scu4 config=port0=enable",
    "set scu4 config=port1=enable",
    "set scu4 config=port2=enable",
    "set scu4 config=port3=enable",
    "set scu4 config=port4=enable",
    "set scu4 config=port5=enable",
    "set scu4 config=port6=enable",
    "set scu4 config=port7=enable",
    "set scu4 config=maska=off",
    "set scu4 config=maskb=off",
    "set scu4 config=lwrstoresize=7",
    "set scu4 config=cyclic=0040",
    "set scu4 config=nea=0200",
    "set scu4 config=onl=014",
    "set scu4 config=int=0",
    "set scu4 config=lwr=0",

// SCU5

    "set scu5 config=mode=program",
    "set scu5 config=port0=enable",
    "set scu5 config=port1=enable",
    "set scu5 config=port2=enable",
    "set scu5 config=port3=enable",
    "set scu5 config=port4=enable",
    "set scu5 config=port5=enable",
    "set scu5 config=port6=enable",
    "set scu5 config=port7=enable",
    "set scu5 config=maska=off",
    "set scu5 config=maskb=off",
    "set scu5 config=lwrstoresize=7",
    "set scu5 config=cyclic=0040",
    "set scu5 config=nea=0200",
    "set scu5 config=onl=014",
    "set scu5 config=int=0",
    "set scu5 config=lwr=0",

// SCU6

    "set scu6 config=mode=program",
    "set scu6 config=port0=enable",
    "set scu6 config=port1=enable",
    "set scu6 config=port2=enable",
    "set scu6 config=port3=enable",
    "set scu6 config=port4=enable",
    "set scu6 config=port5=enable",
    "set scu6 config=port6=enable",
    "set scu6 config=port7=enable",
    "set scu6 config=maska=off",
    "set scu6 config=maskb=off",
    "set scu6 config=lwrstoresize=7",
    "set scu6 config=cyclic=0040",
    "set scu6 config=nea=0200",
    "set scu6 config=onl=014",
    "set scu6 config=int=0",
    "set scu6 config=lwr=0",

// SCU7

    "set scu7 config=mode=program",
    "set scu7 config=port0=enable",
    "set scu7 config=port1=enable",
    "set scu7 config=port2=enable",
    "set scu7 config=port3=enable",
    "set scu7 config=port4=enable",
    "set scu7 config=port5=enable",
    "set scu7 config=port6=enable",
    "set scu7 config=port7=enable",
    "set scu7 config=maska=off",
    "set scu7 config=maskb=off",
    "set scu7 config=lwrstoresize=7",
    "set scu7 config=cyclic=0040",
    "set scu7 config=nea=0200",
    "set scu7 config=onl=014",
    "set scu7 config=int=0",
    "set scu7 config=lwr=0",
#endif

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

    "set fnp0 config=mailbox=03400",
    "set fnp0 ipc_name=fnp-a",
    "set fnp1 config=mailbox=03700",
    "set fnp1 ipc_name=fnp-b",
    "set fnp2 config=mailbox=04200",
    "set fnp2 ipc_name=fnp-c",
    "set fnp3 config=mailbox=04500",
    "set fnp3 ipc_name=fnp-d",
    "set fnp4 config=mailbox=05000",
    "set fnp4 ipc_name=fnp-e",
    "set fnp5 config=mailbox=05300",
    "set fnp5 ipc_name=fnp-f",
    "set fnp6 config=mailbox=05600",
    "set fnp6 ipc_name=fnp-g",
    "set fnp7 config=mailbox=06100",
    "set fnp7 ipc_name=fnp-h",


    //XXX"set mtp0 boot_drive=1",
    // ; Attach tape MPC to IOM 0, chan 012, dev_code 0
    "set mtp0 boot_drive=0",
    "set mtp0 name=MTP0",
    // ; Attach TAPE unit 0 to IOM 0, chan 012, dev_code 1
    "cable IOM0 012 MTP0",
    "cable MTP0 1 TAPE1",
    "set tape1 name=tapa_01",
    "cable MTP0 2 TAPE2",
    "set tape2 name=tapa_02",
    "cable MTP0 3 TAPE3",
    "set tape3 name=tapa_03",
    "cable MTP0 4 TAPE4",
    "set tape4 name=tapa_04",
    "cable MTP0 5 TAPE5",
    "set tape5 name=tapa_05",
    "cable MTP0 6 TAPE6",
    "set tape6 name=tapa_06",
    "cable MTP0 7 TAPE7",
    "set tape7 name=tapa_07",
    "cable MTP0 8 TAPE8",
    "set tape8 name=tapa_08",
    "cable MTP0 9 TAPE9",
    "set tape9 name=tapa_09",
    "cable MTP0 10 TAPE10",
    "set tape10 name=tapa_10",
    "cable MTP0 11 TAPE11",
    "set tape11 name=tapa_11",
    "cable MTP0 12 TAPE12",
    "set tape12 name=tapa_12",
    "cable MTP0 13 TAPE13",
    "set tape13 name=tapa_13",
    "cable MTP0 14 TAPE14",
    "set tape14 name=tapa_14",
    "cable MTP0 15 TAPE15",
    "set tape15 name=tapa_15",
    "cable MTP0 16 TAPE16",
    "set tape16 name=tapa_16",


// 4 3381 disks

    "set ipc0 name=IPC0",
    "cable IOM0 013 IPC0",
    "cable IOM1 013 IPC0 1",
    // ; Attach DISK unit 0 to IPC0 dev_code 0",
    "cable IPC0 0 DISK0",
    "set disk0 type=3381",
    "set disk0 name=dska_00",
    // ; Attach DISK unit 1 to IPC0 dev_code 1",
    "cable IPC0 1 DISK1",
    "set disk1 type=3381",
    "set disk1 name=dska_01",
    // ; Attach DISK unit 2 to IPC0 dev_code 2",
    "cable IPC0 2 DISK2",
    "set disk2 type=3381",
    "set disk2 name=dska_02",
    // ; Attach DISK unit 3 to IPC0 dev_code 3",
    "cable IPC0 3 DISK3",
    "set disk3 type=3381",
    "set disk3 name=dska_03",

// 4 d501 disks + 4 d451 disks

    "set msp0 name=MSP0",
    "cable IOM0 014 MSP0 0",
    "cable IOM1 014 MSP0 1",

    // ; Attach DISK unit 4 to MSP0 dev_code 1",
    "cable MSP0 1 DISK4",
    "set disk4 type=d501",
    "set disk4 name=dskb_01",
    // ; Attach DISK unit 5 to MSP0 dev_code 2",
    "cable MSP0 2 DISK5",
    "set disk5 type=d501",
    "set disk5 name=dskb_02",
    // ; Attach DISK unit 6 to MSP0 dev_code 3",
    "cable MSP0 3 DISK6",
    "set disk6 type=d501",
    "set disk6 name=dskb_03",
    // ; Attach DISK unit 7 to MSP0 dev_code 4",
    "cable MSP0 4 DISK7",
    "set disk7 type=d501",
    "set disk7 name=dskb_04",

    // ; Attach DISK unit 8 to MSP0 dev_code 5",
    "cable MSP0 5 DISK8",
    "set disk8 type=d451",
    "set disk8 name=dskb_05",
    // ; Attach DISK unit 9 to MSP0 dev_code 6",
    "cable MSP0 6 DISK9",
    "set disk9 type=d451",
    "set disk9 name=dskb_06",
    // ; Attach DISK unit 10 to MSP0 dev_code 7",
    "cable MSP0 7 DISK10",
    "set disk10 type=d451",
    "set disk10 name=dskb_07",
    // ; Attach DISK unit 11 to MSP0 dev_code 8",
    "cable MSP0 8 DISK11",
    "set disk11 type=d451",
    "set disk11 name=dskb_08",

    // ; Attach OPC unit 0 to IOM A, chan 036, dev_code 0
    "cable IOMA 036 opc0",
    // No devices for console, so no 'cable OPC0 # CONx'

    // ;;;
    // ;;; FNP
    // ;;;

    // ; Attach FNP unit 3 (d) to IOM A, chan 020, dev_code 0
    "cable IOMA 020 FNPD",
    // ; Attach FNP unit 0 (a) to IOM A, chan 021, dev_code 0
    "cable IOMA 021 FNPA",
    // ; Attach FNP unit 1 (b) to IOM A, chan 022, dev_code 0
    "cable IOMA 022 FNPB",
    // ; Attach FNP unit 2 (c) to IOM A, chan 023, dev_code 0
    "cable IOMA 023 FNPC",
    // ; Attach FNP unit 4 (e) to IOM A, chan 024, dev_code 0
    "cable IOMA 024 FNPE",
    // ; Attach FNP unit 5 (f) to IOM A, chan 025, dev_code 0
    "cable IOMA 025 FNPF",
    // ; Attach FNP unit 6 (g) to IOM A, chan 026, dev_code 0
    "cable IOMA 026 FNPG",
    // ; Attach FNP unit 7 (h) to IOM A, chan 027, dev_code 0
    "cable IOMA 027 FNPH",

    // ;;;
    // ;;; MPC
    // ;;;

    // ; Attach MPC unit 0 to IOM 0, char 015, dev_code 0
    "cable IOM0 015 URP0",
    "set urp0 name=urpa",

    // ; Attach RDR unit 0 to IOM 0, chan 015, dev_code 1
    "cable URP0 1 RDR0",
    "set rdr0 name=rdra",

    // ; Attach MPC unit 1 to IOM 0, char 016, dev_code 0
    "cable IOM0 016 URP1",
    "set urp1 name=urpb",

    // ; Attach PUN unit 0 to IOM 0, chan 016, dev_code 1
    "cable URP1 1 PUN0",
    "set pun0 name=puna",

    // ; Attach MPC unit 2 to IOM 0, char 017, dev_code 0
    "cable IOM0 017 URP2",
    "set urp2 name=urpc",

    // ; Attach PRT unit 0 to IOM 0, chan 017, dev_code 1
    "set prt0 name=prta",
    "cable URP2 1 PRT0",

#if 0
    // ; Attach PRT unit 1 to IOM 0, chan 017, dev_code 2
    "set prt1 name=prtb",
    "cable URP2 2 PRT1",

    // ; Attach PRT unit 2 to IOM 0, chan 017, dev_code 3
    "set prt2 name=prtc",
    "cable URP2 3 PRT2",

    // ; Attach PRT unit 3 to IOM 0, chan 017, dev_code 4
    "cable URP2 4 PRT3",
    "set prt3 name=prtd",

    // ; Attach PRT unit 4 to IOM 0, chan 017, dev_code 5
    "cable URP2 5 PRT4",
    "set prt4 name=prte",

    // ; Attach PRT unit 5 to IOM 0, chan 017, dev_code 6
    "cable URP2 6 PRT5",
    "set prt5 name=prtf",

    // ; Attach PRT unit 6 to IOM 0, chan 017, dev_code 7
    "cable URP2 7 PRT6",
    "set prt6 name=prtg",

    // ; Attach PRT unit 7 to IOM 0, chan 017, dev_code 8
    "cable URP2 8 PRT7",
    "set prt7 name=prth",

    // ; Attach PRT unit 8 to IOM 0, chan 017, dev_code 9
    "cable URP2 9 PRT8",
    "set prt8 name=prti",

    // ; Attach PRT unit 9 to IOM 0, chan 017, dev_code 10
    "cable URP2 10 PRT9",
    "set prt9 name=prtj",

    // ; Attach PRT unit 10 to IOM 0, chan 017, dev_code 11
    "cable URP2 11 PRT10",
    "set prt10 name=prtk",

    // ; Attach PRT unit 11 to IOM 0, chan 017, dev_code 12
    "cable URP2 12 PRT11",
    "set prt11 name=prtl",

    // ; Attach PRT unit 12 to IOM 0, chan 017, dev_code 13
    "cable URP2 13 PRT12",
    "set prt12 name=prtm",

    // ; Attach PRT unit 13 to IOM 0, chan 017, dev_code 14
    "cable URP2 14 PRT13",
    "set prt13 name=prtn",

    // ; Attach PRT unit 14 to IOM 0, chan 017, dev_code 15
    "cable URP2 15 PRT14",
    "set prt14 name=prto",

    // ; Attach PRT unit 15 to IOM 0, chan 017, dev_code 16
    "set prt15 name=prtp",

    // ; Attach PRT unit 16 to IOM 0, chan 017, dev_code 17
    "set prt16 name=prtq",
#endif

    // ; Attach ABSI unit 0 to IOM 0, chan 032, dev_code 0
    "cable IOM0 032 ABSI0",

    // ; Attach IOM unit 0 port A (0) to SCU unit 0, port 0
    "cable SCU0 0 IOM0 0", // SCU0 port 0 IOM0 port 0

    // ; Attach IOM unit 0 port B (1) to SCU unit 1, port 0
    "cable SCU1 0 IOM0 1", // SCU1 port 0 IOM0 port 1

    // ; Attach IOM unit 0 port C (2) to SCU unit 2, port 0
    "cable SCU2 0 IOM0 2", // SCU2 port 0 IOM0 port 2

    // ; Attach IOM unit 0 port D (3) to SCU unit 3, port 0
    "cable SCU3 0 IOM0 3", // SCU3 port 0 IOM0 port 3

    // ; Attach IOM unit 1 port A (0) to SCU unit 0, port 1
    "cable SCU0 1 IOM1 0", // SCU0 port 0 IOM0 port 0

    // ; Attach IOM unit 1 port B (1) to SCU unit 1, port 1
    "cable SCU1 1 IOM1 1", // SCU1 port 0 IOM0 port 1

    // ; Attach IOM unit 1 port C (2) to SCU unit 2, port 1
    "cable SCU2 1 IOM1 2", // SCU2 port 0 IOM0 port 2

    // ; Attach IOM unit 1 port D (3) to SCU unit 3, port 1
    "cable SCU3 1 IOM1 3", // SCU3 port 0 IOM0 port 3

// SCU0 --> CPU0-6

    // ; Attach SCU unit 0 port 7 to CPU unit A (1), port 0
    "cable SCU0 7 CPU0 0",

    // ; Attach SCU unit 0 port 6 to CPU unit B (1), port 0
    "cable SCU0 6 CPU1 0",

    // ; Attach SCU unit 0 port 5 to CPU unit C (2), port 0
    "cable SCU0 5 CPU2 0",

    // ; Attach SCU unit 0 port 4 to CPU unit D (3), port 0
    "cable SCU0 4 CPU3 0",

    // ; Attach SCU unit 0 port 3 to CPU unit E (4), port 0
    "cable SCU0 3 CPU4 0",

    // ; Attach SCU unit 0 port 2 to CPU unit F (5), port 0
    "cable SCU0 2 CPU5 0",

// SCU1 --> CPU0-6

    // ; Attach SCU unit 1 port 7 to CPU unit A (1), port 1
    "cable SCU1 7 CPU0 1",

    // ; Attach SCU unit 1 port 6 to CPU unit B (1), port 1
    "cable SCU1 6 CPU1 1",

    // ; Attach SCU unit 1 port 5 to CPU unit C (2), port 1
    "cable SCU1 5 CPU2 1",

    // ; Attach SCU unit 1 port 4 to CPU unit D (3), port 1
    "cable SCU1 4 CPU3 1",

    // ; Attach SCU unit 1 port 3 to CPU unit E (4), port 0
    "cable SCU1 3 CPU4 1",

    // ; Attach SCU unit 1 port 2 to CPU unit F (5), port 0
    "cable SCU1 2 CPU5 1",

// SCU2 --> CPU0-6

    // ; Attach SCU unit 2 port 7 to CPU unit A (1), port 2
    "cable SCU2 7 CPU0 2", 

    // ; Attach SCU unit 2 port 6 to CPU unit B (1), port 2
    "cable SCU2 6 CPU1 2",

    // ; Attach SCU unit 2 port 5 to CPU unit C (2), port 2
    "cable SCU2 5 CPU2 2",

    // ; Attach SCU unit 2 port 4 to CPU unit D (3), port 2
    "cable SCU2 4 CPU3 2",

    // ; Attach SCU unit 2 port 3 to CPU unit E (4), port 0
    "cable SCU2 3 CPU4 2",

    // ; Attach SCU unit 2 port 2 to CPU unit F (5), port 0
    "cable SCU2 2 CPU5 2",

    // ; Attach SCU unit 2 port 1 to CPU unit G (6), port 0
    "cable SCU2 1 CPU6 2",


// SCU3 --> CPU0-6

    // ; Attach SCU unit 3 port 7 to CPU unit A (1), port 3
    "cable SCU3 7 CPU0 3",

    // ; Attach SCU unit 3 port 6 to CPU unit B (1), port 3
    "cable SCU3 6 CPU1 3",

    // ; Attach SCU unit 3 port 5 to CPU unit C (2), port 3
    "cable SCU3 5 CPU2 3",

    // ; Attach SCU unit 3 port 4 to CPU unit D (3), port 3
    "cable SCU3 4 CPU3 3",

    // ; Attach SCU unit 3 port 3 to CPU unit E (4), port 0
    "cable SCU3 3 CPU4 3",

    // ; Attach SCU unit 3 port 2 to CPU unit F (5), port 0
    "cable SCU3 2 CPU5 3",

    // ; Attach SCU unit 3 port 1 to CPU unit G (6), port 0
    "cable SCU3 1 CPU6 3",


    "set cpu0 reset",
    "set scu0 reset",
    "set scu1 reset",
    "set scu2 reset",
    "set scu3 reset",
    "set iom0 reset",

#if defined(THREADZ) || defined(LOCKLESS)
    "set cpu nunits=6",
#else
#ifdef ISOTLTS
    "set cpu nunits=2",
#else
    "set cpu nunits=1",
#endif // ISOLTS
#endif // THREADZ
    // "set sys config=activate_time=8",
    // "set sys config=terminate_time=8",
#ifdef FNPDBG
    "set sys config=connect_time=4000",
#else
    "set sys config=connect_time=-1",
#endif

#if 0
    "fnpload Devices.txt",
#endif
    "fnpserverport 6180"
  }; // default_base_system_script

// Execute a line of script

static void do_ini_line (char * text)
  {
    //sim_msg ("<%s?\n", text);
    char gbuf[257];
    const char * cptr = get_glyph (text, gbuf, 0); /* get command glyph */
    CTAB *cmdp;
    if ((cmdp = find_cmd (gbuf)))            /* lookup command */
      {
        t_stat stat = cmdp->action (cmdp->arg, cptr); /* if found, exec */
        if (stat != SCPE_OK)
          sim_warn ("%s: %s\n", sim_error_text (SCPE_UNK), text);
      }
    else
      sim_warn ("%s: %s\n", sim_error_text (SCPE_UNK), text);
  }

// Execute the base system script; this strings the cables
// and sets the switches

static t_stat set_default_base_system (UNUSED int32 arg, UNUSED const char * buf)
  {
    int n_lines = sizeof (default_base_system_script) / sizeof (char *);
    for (int line = 0; line < n_lines; line ++)
      do_ini_line (default_base_system_script [line]);
    return SCPE_OK;
  }

// Control access to the 'machine room' HTTP server

static t_stat set_machine_room_port (UNUSED int32 arg, const char * buf)
  {
    int n = atoi (buf);
    if (n < 0 || n > 65535) // 0 is 'disable'
      return SCPE_ARG;
    sys_opts.machine_room_access.port = n;
    sim_msg ("Machine room port set to %d\n", n);
    return SCPE_OK;
  }

static t_stat set_machine_room_pw (UNUSED int32 arg, UNUSED const char * buf)
  {
    if (strlen (buf) == 0)
      {
        sim_warn ("no password\n");
        sys_opts.machine_room_access.pw[0] = 0;
        return SCPE_OK;
      }
    char token[strlen (buf)];
    //sim_msg ("<%s>\n", buf);
    int rc = sscanf (buf, "%s", token);
    if (rc != 1)
      return SCPE_ARG;
    if (strlen (token) > PW_SIZE)
      return SCPE_ARG;
    strcpy (sys_opts.machine_room_access.pw, token);
    //sim_msg ("<%s>\n", token);
    return SCPE_OK;
  }

// Skip records on the boot tape.
// The T&D tape first record is for testing DPS8s, the
// second record (1st record / tape mark / 2nd record)
// is for testing DPS8/Ms.

static t_stat boot_skip (int32 UNUSED arg, const char * UNUSED buf)
  {
    uint32 skipped;
    return sim_tape_sprecsf (& mt_unit[0], 1, & skipped);
  }
  
// Simulate pressing the 'EXECUTE FAULT' button. Used as an
// emergency interrupt of Multics if it hangs and becomes
// unresponive to the operators console.

static t_stat do_execute_fault (UNUSED int32 arg,  UNUSED const char * buf)
  {
    // Assume bootload CPU
    setG7fault (0, FAULT_EXF, fst_zero);
    return SCPE_OK;
  }

static t_stat set_sys_polling_interval (UNUSED int32 arg, const char * buf)
  {
    int n = atoi (buf);
    if (n < 1 || n > 1000) // 1 millisecond to 1 second
      {
        sim_printf ("POLL %d: must be 1 (1 millisecond) to 1000 (1 second)\r\n", n);
        return SCPE_ARG;
      }
    sim_printf ("Polling set to %d milliseconds\r\n", n);
    sys_opts.sys_poll_interval = (uint) n;
    return SCPE_OK;
  }

static t_stat set_sys_slow_polling_interval (UNUSED int32 arg, const char * buf)
  {
    int n = atoi (buf);
    if (n < 1 || n > 1000) // 1 - slow poll every pool; 1000 - slow poll every 1000 polls
      {
        sim_printf ("SLOWPOLL %d: must be 1 (1 slow poll per pol) to 1000 (1 slow poll per 1000 polls)\r\n", n);
        return SCPE_ARG;
      }
    sim_printf ("Slow polling set to %d polls\r\n", n);
    sys_opts.sys_slow_poll_interval = (uint) n;
    return SCPE_OK;
  }

static t_stat set_sys_poll_check_rate (UNUSED int32 arg, const char * buf)
  {
    int n = atoi (buf);
    if (n < 1 || n > 1024*1024) // 1 - poll check rate in CPY cycles: 1 - check every cycle; 1024 check every 1024 cycles
      {
        sim_printf ("CHECKPOLL %d: must be 1 (check every cycle) to 1048576 (ckeck every million cycles\r\n", n);
        return SCPE_ARG;
      }
    sim_printf ("Poll check rate set to %d CPU cycles\r\n", n);
    sys_opts.sys_poll_check_rate = (uint) n;
    return SCPE_OK;
  }

//
// Debugging commands
//

#ifdef TESTING

// Filter settings for our customized sim_debug

// Start debug output at CPU cycle N.
uint64 sim_deb_start = 0;
// Stop debug outout at CPU cycle N.
uint64 sim_deb_stop = 0;
// Break to simh prompt at CPU cycle N.
uint64 sim_deb_break = 0;
// Enable CPU sim_debug iff PPR.PSR == N
uint64 sim_deb_segno = NO_SUCH_SEGNO;
// Enable CPU sim_debug iff PPR.PRR == N
uint64 sim_deb_ringno = NO_SUCH_RINGNO;
// Supress CPU sim_debug calls that pass all
// of the filters after N times
uint64 sim_deb_skip_limit = 0;
// Supress the first N CPU sim_debug calls
// that pass all of the filters
uint64 sim_deb_skip_cnt = 0;
// Supress sim_debug until the MME instruction
// has been executed N times
uint64 sim_deb_mme_cntdwn = 0;
// Supress CPU sim_debug unless CPU number bit set
uint dbgCPUMask = 0377; // default all 8 on

// Supress CPU sim_debug unless BAR mode
bool sim_deb_bar = false;

// Set the various filters

static t_stat dps_debug_mme_cntdwn (UNUSED int32 arg, const char * buf)
  {
    sim_deb_mme_cntdwn = strtoull (buf, NULL, 0);
    sim_msg ("Debug MME countdown set to %"PRId64"\n", sim_deb_mme_cntdwn);
    return SCPE_OK;
  }

static t_stat dps_debug_skip (UNUSED int32 arg, const char * buf)
  {
    sim_deb_skip_cnt = 0;
    sim_deb_skip_limit = strtoull (buf, NULL, 0);
    sim_msg ("Debug skip set to %"PRId64"\n", sim_deb_skip_limit);
    return SCPE_OK;
  }

static t_stat dps_debug_start (UNUSED int32 arg, const char * buf)
  {
    sim_deb_start = strtoull (buf, NULL, 0);
    sim_msg ("Debug set to start at cycle: %"PRId64"\n", sim_deb_start);
    return SCPE_OK;
  }

static t_stat dps_debug_stop (UNUSED int32 arg, const char * buf)
  {
    sim_deb_stop = strtoull (buf, NULL, 0);
    sim_msg ("Debug set to stop at cycle: %"PRId64"\n", sim_deb_stop);
    return SCPE_OK;
  }

static t_stat dps_debug_break (UNUSED int32 arg, const char * buf)
  {
    sim_deb_break = strtoull (buf, NULL, 0);
    if (buf[0] == '+')
      sim_deb_break += sim_deb_start;
    sim_msg ("Debug set to break at cycle: %"PRId64"\n", sim_deb_break);
    return SCPE_OK;
  }

static t_stat dps_debug_segno (UNUSED int32 arg, const char * buf)
  {
    sim_deb_segno = strtoull (buf, NULL, 0);
    sim_msg ("Debug set to segno %"PRIo64"\n", sim_deb_segno);
    return SCPE_OK;
  }

static t_stat dps_debug_ringno (UNUSED int32 arg, const char * buf)
  {
    sim_deb_ringno = strtoull (buf, NULL, 0);
    sim_msg ("Debug set to ringno %"PRIo64"\n", sim_deb_ringno);
    return SCPE_OK;
  }

static t_stat dps_debug_bar (int32 arg, UNUSED const char * buf)
  {
    sim_deb_bar = arg;
    if (arg)
      sim_msg ("Debug set BAR %"PRIo64"\n", sim_deb_ringno);
    else
      sim_msg ("Debug unset BAR %"PRIo64"\n", sim_deb_ringno);
    return SCPE_OK;
  }

#if 0
t_stat computeAbsAddrN (word24 * abs_addr, int segno, uint offset)
  {
    word24 res;

    if (get_addr_mode () != APPEND_mode)
      {
        sim_warn ("CPU not in append mode\n");
        return SCPE_ARG;
      }

    if (cpu.DSBR.U == 1) // Unpaged
      {
        if (2 * (uint) /*TPR.TSR*/ segno >= 16 * ((uint) cpu.DSBR.BND + 1))
          {
            sim_warn ("DSBR boundary violation.\n");
            return SCPE_ARG;
          }

        // 2. Fetch the target segment SDW from cpu.DSBR.ADDR + 2 * segno.

        word36 SDWe, SDWo;
        core_read ((cpu.DSBR.ADDR + 2U * /*TPR.TSR*/ (uint) segno) & PAMASK,
                   & SDWe, __func__);
        core_read ((cpu.DSBR.ADDR + 2U * /*TPR.TSR*/ (uint) segno  + 1) & PAMASK, 
                   & SDWo, __func__);

        // 3. If SDW.DF = 0, then generate directed fault n where n is given in
        // SDW.FC. The value of n used here is the value assigned to define a
        // missing segment fault or, simply, a segment fault.

        // abs_addr doesn't care if the page isn't resident


        // 4. If offset >= 16 * (SDW.BOUND + 1), then generate an access
        // violation, out of segment bounds, fault.

        word14 BOUND = (SDWo >> (35 - 14)) & 037777;
        if (/*TPR.CA*/ offset >= 16 * (BOUND + 1))
          {
            sim_warn ("SDW boundary violation.\n");
            return SCPE_ARG;
          }

        // 5. If the access bits (SDW.R, SDW.E, etc.) of the segment are
        //    incompatible with the reference, generate the appropriate access 
        //    violation fault.

        // abs_addr doesn't care

        // 6. Generate 24-bit absolute main memory address SDW.ADDR + offset.

        word24 ADDR = (SDWe >> 12) & 077777760;
        res = (word24) ADDR + (word24) /*TPR.CA*/ offset;
        res &= PAMASK; //24 bit math
        //res <<= 12; // 24:12 format

      }
    else
      {
        //word15 segno = TPR.TSR;
        //word18 offset = TPR.CA;

        // 1. If 2 * segno >= 16 * (cpu.DSBR.BND + 1), then generate an access 
        // violation, out of segment bounds, fault.

        if (2 * (uint) segno >= 16 * ((uint) cpu.DSBR.BND + 1))
          {
            sim_warn ("DSBR boundary violation.\n");
            return SCPE_ARG;
          }

        // 2. Form the quantities:
        //       y1 = (2 * segno) modulo 1024
        //       x1 = (2 * segno ­ y1) / 1024

        word24 y1 = (2 * (uint) segno) % 1024;
        word24 x1 = (2 * (uint) segno - y1) / 1024;

        // 3. Fetch the descriptor segment PTW(x1) from DSBR.ADR + x1.

        word36 PTWx1;
        core_read ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);

        ptw_s PTW1;
        PTW1.ADDR = GETHI(PTWx1);
        PTW1.U = TSTBIT(PTWx1, 9);
        PTW1.M = TSTBIT(PTWx1, 6);
        PTW1.DF = TSTBIT(PTWx1, 2);
        PTW1.FC = PTWx1 & 3;

        // 4. If PTW(x1).DF = 0, then generate directed fault n where n is 
        // given in PTW(x1).FC. The value of n used here is the value 
        // assigned to define a missing page fault or, simply, a
        // page fault.

        if (!PTW1.DF)
          {
            sim_warn ("!PTW1.DF\n");
            return SCPE_ARG;
          }

        // 5. Fetch the target segment SDW, SDW(segno), from the 
        // descriptor segment page at PTW(x1).ADDR + y1.

        word36 SDWeven, SDWodd;
        core_read2(((PTW1.ADDR << 6) + y1) & PAMASK, & SDWeven, & SDWodd,
                    __func__);

        sdw0_s SDW0; 
        // even word
        SDW0.ADDR = (SDWeven >> 12) & PAMASK;
        SDW0.R1 = (SDWeven >> 9) & 7;
        SDW0.R2 = (SDWeven >> 6) & 7;
        SDW0.R3 = (SDWeven >> 3) & 7;
        SDW0.DF = TSTBIT(SDWeven, 2);
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

        // 6. If SDW(segno).DF = 0, then generate directed fault n where 
        // n is given in SDW(segno).FC.
        // This is a segment fault as discussed earlier in this section.

        if (!SDW0.DF)
          {
            sim_warn ("!SDW0.DF\n");
            return SCPE_ARG;
          }

        // 7. If offset >= 16 * (SDW(segno).BOUND + 1), then generate an 
        // access violation, out of segment bounds, fault.

        if (((offset >> 4) & 037777) > SDW0.BOUND)
          {
            sim_warn ("SDW boundary violation\n");
            return SCPE_ARG;
          }

        // 8. If the access bits (SDW(segno).R, SDW(segno).E, etc.) of the 
        // segment are incompatible with the reference, generate the 
        // appropriate access violation fault.

        // Only the address is wanted, so no check

        if (SDW0.U == 0)
          {
            // Segment is paged
            // 9. Form the quantities:
            //    y2 = offset modulo 1024
            //    x2 = (offset - y2) / 1024

            word24 y2 = offset % 1024;
            word24 x2 = (offset - y2) / 1024;
    
            // 10. Fetch the target segment PTW(x2) from SDW(segno).ADDR + x2.

            word36 PTWx2;
            core_read ((SDW0.ADDR + x2) & PAMASK, & PTWx2, __func__);
    
            ptw_s PTW2;
            PTW2.ADDR = GETHI(PTWx2);
            PTW2.U = TSTBIT(PTWx2, 9);
            PTW2.M = TSTBIT(PTWx2, 6);
            PTW2.DF = TSTBIT(PTWx2, 2);
            PTW2.FC = PTWx2 & 3;

            // 11.If PTW(x2).DF = 0, then generate directed fault n where n is 
            // given in PTW(x2).FC. This is a page fault as in Step 4 above.

            // abs_addr only wants the address; it doesn't care if the page is
            // resident

            // if (!PTW2.DF)
            //   {
            //     sim_debug (DBG_APPENDING, & cpu_dev, "absa fault !PTW2.DF\n");
            //     // initiate a directed fault
            //     doFault(FAULT_DF0 + PTW2.FC, 0, "ABSA !PTW2.DF");
            //   }

            // 12. Generate the 24-bit absolute main memory address 
            // PTW(x2).ADDR + y2.

            res = (((word24) PTW2.ADDR) << 6)  + (word24) y2;
            res &= PAMASK; //24 bit math
            //res <<= 12; // 24:12 format
          }
        else
          {
            // Segment is unpaged
            // SDW0.ADDR is the base address of the segment
            res = (word24) SDW0.ADDR + offset;
            res &= PAMASK; //24 bit math
            res <<= 12; // 24:12 format
          }
      }

    * abs_addr = res;
    return SCPE_OK;
  }
#endif

// Translate seg:offset to absolute address

static t_stat abs_addr_n (int segno, uint offset)
  {
    word24 res;

    //t_stat rc = computeAbsAddrN (& res, segno, offset);
    if (dbgLookupAddress ((word18) segno, offset, & res, NULL))
      return SCPE_ARG;

    sim_msg ("Address is %08o\n", res);
    return SCPE_OK;
  }

// ABS segno:offset
// simh command to translate segno:offset to absolute address

static t_stat abs_addr (UNUSED int32 arg, const char * buf)
  {
    int segno;
    uint offset;
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    return abs_addr_n (segno, offset);
  }

// LOAD_SYSTEM_BOOK <filename>

// Read a system_book segment, extracting segment names and numbers
// and component names, offsets, and lengths

#define BOOT_SEGMENTS_MAX 1024
#define BOOT_COMPONENTS_MAX 4096
#define BOOK_SEGMENT_NAME_LEN 33

static struct book_segment
  {
    char * segname;
    int segno;
  } book_segments[BOOT_SEGMENTS_MAX];

static int n_book_segments = 0;

static struct book_component
  {
    char * compname;
    int book_segment_number;
    uint txt_start, txt_length;
    int intstat_start, intstat_length, symbol_start, symbol_length;
  } book_components[BOOT_COMPONENTS_MAX];

static int n_book_components = 0;

static int lookup_book_segment (char * name)
  {
    for (int i = 0; i < n_book_segments; i ++)
      if (strcmp (name, book_segments[i].segname) == 0)
        return i;
    return -1;
  }

static int add_book_segment (char * name, int segno)
  {
    int n = lookup_book_segment (name);
    if (n >= 0)
      return n;
    if (n_book_segments >= BOOT_SEGMENTS_MAX)
      return -1;
    book_segments[n_book_segments].segname = strdup (name);
    book_segments[n_book_segments].segno = segno;
    n = n_book_segments;
    n_book_segments ++;
    return n;
  }
 
static int add_book_component (int segnum, char * name, uint txt_start,
                               uint txt_length, int intstat_start, 
                               int intstat_length, int symbol_start,
                               int symbol_length)
  {
    if (n_book_components >= BOOT_COMPONENTS_MAX)
      return -1;
    book_components[n_book_components].compname = strdup (name);
    book_components[n_book_components].book_segment_number = segnum;
    book_components[n_book_components].txt_start = txt_start;
    book_components[n_book_components].txt_length = txt_length;
    book_components[n_book_components].intstat_start = intstat_start;
    book_components[n_book_components].intstat_length = intstat_length;
    book_components[n_book_components].symbol_start = symbol_start;
    book_components[n_book_components].symbol_length = symbol_length;
    int n = n_book_components;
    n_book_components ++;
    return n;
  }
 

// Given a segno:offset, try to translate to
// component name and offset in the component

// Warning: returns ptr to static buffer
static char * lookup_system_book_address (word18 segno, word18 offset,
                                         char * * compname, word18 * compoffset)
  {
    static char buf[129];
    int i;

    for (i = 0; i < n_book_segments; i ++)
      if (book_segments[i].segno == (int) segno)
        break;

    if (i >= n_book_segments)
      return NULL;

    int best = -1;
    uint bestoffset = 0;

    for (int j = 0; j < n_book_components; j ++)
      {
        if (book_components[j].book_segment_number != i)
          continue;
        if (book_components[j].txt_start <= offset &&
            book_components[j].txt_start + book_components[j].txt_length > offset)
          {
            sprintf (buf, "%s:%s+0%0o", book_segments[i].segname,
              book_components[j].compname,
              offset - book_components[j].txt_start);
            if (compname)
              * compname = book_components[j].compname;
            if (compoffset)
              * compoffset = offset - book_components[j].txt_start;
            return buf;
          }
        if (book_components[j].txt_start <= offset &&
            book_components[j].txt_start > bestoffset)
          {
            best = j;
            bestoffset = book_components[j].txt_start;
          }
      }

    if (best != -1)
      {
        // Didn't find a component track bracketed the offset; return the
        // component that was before the offset
        if (compname)
          * compname = book_components[best].compname;
        if (compoffset)
          * compoffset = offset - book_components[best].txt_start;
        sprintf (buf, "%s:%s+0%0o", book_segments[i].segname,
          book_components[best].compname,
          offset - book_components[best].txt_start);
        return buf;
      }

    // Found a segment, but it had no components. Return the segment name
    // as the component name

    if (compname)
      * compname = book_segments[i].segname;
    if (compoffset)
      * compoffset = offset;
    sprintf (buf, "%s:+0%0o", book_segments[i].segname,
             offset);
    return buf;
  }

// Given a segno and offset, find the component name and its
// offset in the segment

char * lookup_address (word18 segno, word18 offset, char * * compname,
                       word18 * compoffset)
  {
    if (compname)
      * compname = NULL;
    if (compoffset)
      * compoffset = 0;

    // Magic numbers!
    // Multics seems to have a copy of hpchs_ (segno 0162) in segment 0322;
    // This little tweak allows source code level tracing for segment 0322,
    // and has no operational significance to the emulator
    // Hmmm. What is happening is that these segments are being loaded into
    // ring 4, and assigned segment #'s; the assigned number will vary 
    // depending on the exact sequence of events.
    if (segno == 0322)
      segno = 0162;
    if (segno == 0310)
      segno = 041;
    if (segno == 0314)
      segno = 041;
    if (segno == 0313)
      segno = 040;
    if (segno == 0317)
      segno = 0161;

#if 0
    // Hack to support formline debugging
#define IOPOS 02006 // interpret_op_ptr_ offset
    if (segno == 0371)
      {
        if (offset < IOPOS)
          {
            if (compname)
              * compname = "find_condition_info_";
            if (compoffset)
              * compoffset = offset;
            static char buf[129];
            sprintf (buf, "bound_debug_util_:find_condition_info_+0%0o", 
                  offset - 0);
            return buf;
          }
        else
          {
            if (compname)
              * compname = "interpret_op_ptr_";
            if (compoffset)
              * compoffset = offset - IOPOS;
            static char buf[129];
            sprintf (buf, "bound_debug_util_:interpret_op_ptr_+0%0o", 
                  offset - IOPOS);
            return buf;
          }

      }
#endif

    char * ret = lookup_system_book_address (segno, offset, compname, compoffset);
#ifndef SCUMEM
    if (ret)
      return ret;
    ret = lookupSegmentAddress (segno, offset, compname, compoffset);
#endif
    return ret;
  }

// Given a segment name and component name, return the
// components segment number and offset

// Warning: returns ptr to static buffer
static int lookup_system_book_name (char * segname, char * compname, long * segno,
                                    long * offset)
  {
    int i;
    for (i = 0; i < n_book_segments; i ++)
      if (strcmp (book_segments[i].segname, segname) == 0)
        break;
    if (i >= n_book_segments)
      return -1;

    for (int j = 0; j < n_book_components; j ++)
      {
        if (book_components[j].book_segment_number != i)
          continue;
        if (strcmp (book_components[j].compname, compname) == 0)
          {
            * segno = book_segments[i].segno;
            * offset = (long) book_components[j].txt_start;
            return 0;
          }
      }

   return -1;
 }

static char * source_search_path = NULL;

// Given a component name and an offset in the component,
// find the listing file of the component and try to
// print the source code line that generated the code at
// component:offset

void list_source (char * compname, word18 offset, uint dflag)
  {
    const int offset_str_len = 10;
    //char offset_str[offset_str_len + 1];
    char offset_str[17];
    sprintf (offset_str, "    %06o", offset);

    char path[(source_search_path ? strlen (source_search_path) : 1) + 
               1 + // "/"
               (compname ? strlen (compname) : 1) +
                1 + strlen (".list") + 1];
    char * searchp = source_search_path ? source_search_path : ".";
    // find <search path>/<compname>.list
    while (* searchp)
      {
        size_t pathlen = strcspn (searchp, ":");
        strncpy (path, searchp, pathlen);
        path[pathlen] = '\0';
        if (searchp[pathlen] == ':')
          searchp += pathlen + 1;
        else
          searchp += pathlen;

        if (compname)
          {
            strcat (path, "/");
            strcat (path, compname);
          }
        strcat (path, ".list");
        //sim_msg ("<%s>\n", path);
        FILE * listing = fopen (path, "r");
        if (listing)
          {
            char line[1025];
            if (feof (listing))
              goto fileDone;
            fgets (line, 1024, listing);
            if (strncmp (line, "ASSEMBLY LISTING", 16) == 0)
              {
                // Search ALM listing file
                // sim_msg ("found <%s>\n", path);

                // ALM listing files look like:
                //     000226  4a  4 00010 7421 20  \tstx2]tbootload_0$entry_stack_ptr,id
                while (! feof (listing))
                  {
                    fgets (line, 1024, listing);
                    if (strncmp (line, offset_str, (size_t) offset_str_len) == 0)
                      {
                        if (! dflag)
                          sim_msg ("%s", line);
                        else
                          sim_debug (dflag, & cpu_dev, "%s", line);
                        //break;
                      }
                    if (strcmp (line, "\fLITERALS\n") == 0)
                      break;
                  }
              } // if assembly listing
            else if (strncmp (line, "\tCOMPILATION LISTING", 20) == 0)
              {
                // Search PL/I listing file

                // PL/I files have a line location table
                //     "   LINE    LOC      LINE    LOC ...."

                bool foundTable = false;
                while (! feof (listing))
                  {
                    fgets (line, 1024, listing);
                    if (strncmp (line, "   LINE    LOC", 14) != 0)
                      continue;
                    foundTable = true;
                    // Found the table
                    // Table lines look like
                    //     "     13 000705       275 000713  ...
                    // But some times
                    //     "     10 000156   21   84 000164
                    //     "      8 000214        65 000222    4   84 000225    
                    //
                    //     "    349 001442       351 001445       353 001454    1    9 001456    1   11 001461    1   12 001463    1   13 001470
                    //     " 1   18 001477       357 001522       361 001525       363 001544       364 001546       365 001547       366 001553

                    //  I think the numbers refer to include files...
                    //   But of course the format is slightly off...
                    //    table    ".1...18
                    //    listing  ".1....18
                    int best = -1;
                    char bestLines[8] = {0, 0, 0, 0, 0, 0, 0};
                    while (! feof (listing))
                      {
                        int loc[7];
                        char linenos[7][8];
                        memset (linenos, 0, sizeof (linenos));
                        fgets (line, 1024, listing);
                        // sometimes the leading columns are blank...
                        while (strncmp (line,
                                        "                 ", 8 + 6 + 3) == 0)
                          memmove (line, line + 8 + 6 + 3,
                                   strlen (line + 8 + 6 + 3));
                        // deal with the extra numbers...

                        int cnt = sscanf (line,
                          // " %d %o %d %o %d %o %d %o %d %o %d %o %d %o", 
                          "%8c%o%*3c"
                          "%8c%o%*3c"
                          "%8c%o%*3c"
                          "%8c%o%*3c"
                          "%8c%o%*3c"
                          "%8c%o%*3c"
                          "%8c%o", 
                          (char *) & linenos[0], & loc[0], 
                          (char *) & linenos[1], & loc[1], 
                          (char *) & linenos[2], & loc[2], 
                          (char *) & linenos[3], & loc[3], 
                          (char *) & linenos[4], & loc[4], 
                          (char *) & linenos[5], & loc[5], 
                          (char *) & linenos[6], & loc[6]);
                        if (! (cnt == 2 || cnt == 4 || cnt == 6 ||
                               cnt == 8 || cnt == 10 || cnt == 12 ||
                               cnt == 14))
                          break; // end of table
                        int n;
                        for (n = 0; n < cnt / 2; n ++)
                          {
                            if (loc[n] > best && loc[n] <= (int) offset)
                              {
                                best = loc[n];
                                memcpy (bestLines, linenos[n],
                                        sizeof (bestLines));
                              }
                          }
                        if (best == (int) offset)
                          break;
                      }
                    if (best == -1)
                      goto fileDone; // Not found in table

                    //   But of course the format is slightly off...
                    //    table    ".1...18
                    //    listing  ".1....18
                    // bestLines "21   84 "
                    // listing   " 21    84 "
                    char searchPrefix[10];
                    searchPrefix[0] = ' ';
                    searchPrefix[1] = bestLines[0];
                    searchPrefix[2] = bestLines[1];
                    searchPrefix[3] = ' ';
                    searchPrefix[4] = bestLines[2];
                    searchPrefix[5] = bestLines[3];
                    searchPrefix[6] = bestLines[4];
                    searchPrefix[7] = bestLines[5];
                    searchPrefix[8] = bestLines[6];
                    // ignore trailing space; some times its a tab
                    // searchPrefix[ 9] = bestLines[ 7];
                    searchPrefix[9] = '\0';

                    // Look for the line in the listing
                    rewind (listing);
                    while (! feof (listing))
                      {
                        fgets (line, 1024, listing);
                        if (strncmp (line, "\f\tSOURCE", 8) == 0)
                          goto fileDone; // end of source code listing
                        char prefix[10];
                        strncpy (prefix, line, 9);
                        prefix[9] = '\0';
                        if (strcmp (prefix, searchPrefix) != 0)
                          continue;
                        // Got it
                        if (!dflag)
                          sim_msg ("%s", line);
                        else
                          sim_debug (dflag, & cpu_dev, "%s", line);
                        //break;
                      }
                    goto fileDone;
                  } // if table start
                if (! foundTable)
                  {
                    // Can't find the LINE/LOC table; look for listing
                    rewind (listing);
                    while (! feof (listing))
                      {
                        fgets (line, 1024, listing);
                        if (strncmp (line,
                                     offset_str + 4,
                                     offset_str_len - 4) == 0)
                          {
                            if (! dflag)
                              sim_msg ("%s", line);
                            else
                              sim_debug (dflag, & cpu_dev, "%s", line);
                            //break;
                          }
                        //if (strcmp (line, "\fLITERALS\n") == 0)
                          //break;
                      }
                  } // if ! tableFound
              } // if PL/I listing
                        
fileDone:
            fclose (listing);
          } // if (listing)
      }
  }

// STK 

#ifndef SCUMEM
static t_stat stack_trace (UNUSED int32 arg,  UNUSED const char * buf)
  {
    char * msg;

    word15 icSegno = cpu.PPR.PSR;
    word18 icOffset = cpu.PPR.IC;
    
    sim_msg ("Entry ptr   %05o:%06o\n", icSegno, icOffset);
    
    char * compname;
    word18 compoffset;
    char * where = lookup_address (icSegno, icOffset,
                                   & compname, & compoffset);
    if (where)
      {
        sim_msg ("%05o:%06o %s\n", icSegno, icOffset, where);
        list_source (compname, compoffset, 0);
      }
    sim_msg ("\n");

    // According to AK92
    //
    //  pr0/ap operator segment pointer
    //  pr6/sp stack frame pointer
    //  pr4/lp linkage section for the executing procedure
    //  pr7/sb stack base

    word15 fpSegno = cpu.PR[6].SNR;
    word18 fpOffset = cpu.PR[6].WORDNO;

    for (uint frameNo = 1; ; frameNo ++)
      {
        sim_msg ("Frame %d %05o:%06o\n", 
                    frameNo, fpSegno, fpOffset);

        word24 fp;
        if (dbgLookupAddress (fpSegno, fpOffset, & fp, & msg))
          {
            sim_msg ("can't lookup fp (%05o:%06o) because %s\n",
                    fpSegno, fpOffset, msg);
            break;
          }
    
        word15 prevfpSegno = (word15) ((M[fp + 16] >> 18) & MASK15);
        word18 prevfpOffset = (word18) ((M[fp + 17] >> 18) & MASK18);
    
        sim_msg ("Previous FP %05o:%06o\n", prevfpSegno, prevfpOffset);
    
        word15 returnSegno = (word15) ((M[fp + 20] >> 18) & MASK15);
        word18 returnOffset = (word18) ((M[fp + 21] >> 18) & MASK18);
    
        sim_msg ("Return ptr  %05o:%06o\n", returnSegno, returnOffset);
    
        if (returnOffset == 0)
          {
            if (frameNo == 1)
              {
                // try rX[7] as the return address
                sim_msg ("guessing X7 has a return address....\n");
                where = lookup_address (icSegno, cpu.rX[7] - 1,
                                        & compname, & compoffset);
                if (where)
                  {
                    sim_msg ("%05o:%06o %s\n", icSegno, cpu.rX[7] - 1, where);
                    list_source (compname, compoffset, 0);
                  }
              }
          }
        else
          {
            where = lookup_address (returnSegno, returnOffset - 1,
                                    & compname, & compoffset);
            if (where)
              {
                sim_msg ("%05o:%06o %s\n",
                            returnSegno, returnOffset - 1, where);
                list_source (compname, compoffset, 0);
              }
          }

        word15 entrySegno = (word15) ((M[fp + 22] >> 18) & MASK15);
        word18 entryOffset = (word18) ((M[fp + 23] >> 18) & MASK18);
    
        sim_msg ("Entry ptr   %05o:%06o\n", entrySegno, entryOffset);
    
        where = lookup_address (entrySegno, entryOffset,
                                & compname, & compoffset);
        if (where)
          {
            sim_msg ("%05o:%06o %s\n", entrySegno, entryOffset, where);
            list_source (compname, compoffset, 0);
          }
    
        word15 argSegno = (word15) ((M[fp + 26] >> 18) & MASK15);
        word18 argOffset = (word18) ((M[fp + 27] >> 18) & MASK18);
        sim_msg ("Arg ptr     %05o:%06o\n", argSegno, argOffset);
    
        word24 ap;
        if (dbgLookupAddress (argSegno, argOffset, & ap, & msg))
          {
            sim_msg ("can't lookup arg ptr (%05o:%06o) because %s\n",
                    argSegno, argOffset, msg);
            goto skipArgs;
          }
    
        word16 argCount = (word16) ((M[ap + 0] >> 19) & MASK17);
        word18 callType = (word18) (M[ap + 0] & MASK18);
        word16 descCount = (word16) ((M[ap + 1] >> 19) & MASK17);
        sim_msg ("arg_count   %d\n", argCount);
        switch (callType)
          {
            case 0u:
              sim_msg ("call_type Quick internal call\n");
              break;
            case 4u:
              sim_msg ("call_type Inter-segment\n");
              break;
            case 8u:
              sim_msg ("call_type Enviroment pointer\n");
              break;
            default:
              sim_msg ("call_type Unknown (%o)\n", callType);
              goto skipArgs;
              }
        sim_msg ("desc_count  %d\n", descCount);
    
#if 0
        if (descCount)
          {
            // XXX walk descriptor and arg list together
          }
        else
#endif
          {
            for (uint argno = 0; argno < argCount; argno ++)
              {
                uint argnoos = ap + 2 + argno * 2;
                word15 argnoSegno = (word15) ((M[argnoos] >> 18) & MASK15);
                word18 argnoOffset = (word18) ((M[argnoos + 1] >> 18) & MASK18);
                word24 argnop;
                if (dbgLookupAddress (argnoSegno, argnoOffset, & argnop, & msg))
                  {
                    sim_msg ("can't lookup arg%d ptr (%05o:%06o) because %s\n",
                                argno, argSegno, argOffset, msg);
                    continue;
                  }
                word36 argv = M[argnop];
                sim_msg ("arg%d value   %05o:%06o[%08o] "
                            "%012"PRIo64" (%"PRIu64")\n", 
                            argno, argSegno, argOffset, argnop, argv, argv);
                sim_msg ("\n");
             }
         }
skipArgs:;

        sim_msg ("End of frame %d\n\n", frameNo);

        if (prevfpSegno == 077777 && prevfpOffset == 1)
          break;
        fpSegno = prevfpSegno;
        fpOffset = prevfpOffset;
      }
    return SCPE_OK;
  }
#endif

static t_stat list_source_at (UNUSED int32 arg, UNUSED const char *  buf)
  {
    // list seg:offset
    int segno;
    uint offset;
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    char * compname;
    word18 compoffset;
    char * where = lookup_address ((word18) segno, offset,
                                   & compname, & compoffset);
    if (where)
      {
        sim_msg ("%05o:%06o %s\n", segno, offset, where);
        list_source (compname, compoffset, 0);
      }
    return SCPE_OK;
  }

static t_stat load_system_book (UNUSED int32 arg, UNUSED const char * buf)
  {
// Quietly ignore if not debug enabled
#ifndef SPEED
    // Multics 12.5 assigns segment number to collection 3 starting at 0244.
    uint c3 = 0244;

#define bufSz 257
    char filebuf[bufSz];
    int current = -1;

    FILE * fp = fopen (buf, "r");
    if (! fp)
      {
        sim_msg ("error opening file %s\n", buf);
        return SCPE_ARG;
      }
    for (;;)
      {
        char * bufp = fgets (filebuf, bufSz, fp);
        if (! bufp)
          break;
        //sim_msg ("<%s\n>", filebuf);
        char name[BOOK_SEGMENT_NAME_LEN];
        int segno, p0, p1, p2;

        // 32 is BOOK_SEGMENT_NAME_LEN - 1
        int cnt = sscanf (filebuf, "%32s %o  (%o, %o, %o)", name, & segno, 
          & p0, & p1, & p2);
        if (filebuf[0] != '\t' && cnt == 5)
          {
            //sim_msg ("A: %s %d\n", name, segno);
            int rc = add_book_segment (name, segno);
            if (rc < 0)
              {
                sim_warn ("error adding segment name\n");
                fclose (fp);
                return SCPE_ARG;
              }
            continue;
          }
        else
          {
            // Check for collection 3 segment
            // 32 is BOOK_SEGMENT_NAME_LEN - 1
            cnt = sscanf (filebuf, "%32s  (%o, %o, %o)", name, 
              & p0, & p1, & p2);
            if (filebuf[0] != '\t' && cnt == 4)
              {
                if (strstr (name, "fw.") || strstr (name, ".ec"))
                  continue;
                //sim_msg ("A: %s %d\n", name, segno);
                int rc = add_book_segment (name, (int) (c3 ++));
                if (rc < 0)
                  {
                    sim_warn ("error adding segment name\n");
                    fclose (fp);
                    return SCPE_ARG;
                  }
                continue;
              }
          }
        cnt = sscanf (filebuf, "Bindmap for >ldd>h>e>%32s", name);
        if (cnt == 1)
          {
            //sim_msg ("B: %s\n", name);
            //int rc = add_book_segment (name);
            int rc = lookup_book_segment (name);
            if (rc < 0)
              {
                // The collection 3.0 segments do not have segment numbers,
                // and the 1st digit of the 3-tuple is 1, not 0. Ignore
                // them for now.
                current = -1;
                continue;
                //sim_warn ("error adding segment name\n");
                //return SCPE_ARG;
              }
            current = rc;
            continue;
          }

        uint txt_start, txt_length;
        int intstat_start, intstat_length, symbol_start, symbol_length;
        cnt = sscanf (filebuf, "%32s %o %o %o %o %o %o", name, & txt_start,
                      & txt_length, & intstat_start, & intstat_length,
                      & symbol_start, & symbol_length);

        if (cnt == 7)
          {
            //sim_msg ("C: %s\n", name);
            if (current >= 0)
              {
                add_book_component (current, name, txt_start, txt_length,
                                    intstat_start, intstat_length, symbol_start,
                                    symbol_length);
              }
            continue;
          }

        cnt = sscanf (filebuf, "%32s %o  (%o, %o, %o)", name, & segno, 
          & p0, & p1, & p2);
        if (filebuf[0] == '\t' && cnt == 5)
          {
            //sim_msg ("D: %s %d\n", name, segno);
            int rc = add_book_segment (name, segno);
            if (rc < 0)
              {
                sim_warn ("error adding segment name\n");
                fclose (fp);
                return SCPE_ARG;
              }
            continue;
          }

      }
    fclose (fp);
#if 0
    for (int i = 0; i < n_book_segments; i ++)
      { 
        sim_msg ("  %-32s %6o\n", book_segments[i].segname,
                    book_segments[i].segno);
        for (int j = 0; j < n_book_components; j ++)
          {
            if (book_components[j].book_segment_number == i)
              {
                printf ("    %-32s %6o %6o %6o %6o %6o %6o\n",
                  book_components[j].compname, 
                  book_components[j].txt_start, 
                  book_components[j].txt_length, 
                  book_components[j].intstat_start, 
                  book_components[j].intstat_length, 
                  book_components[j].symbol_start, 
                  book_components[j].symbol_length);
              }
          }
      }
#endif
#endif
    return SCPE_OK;
  }

static t_stat add_system_book_entry (UNUSED int32 arg, const char * buf)
  {
    // asbe segname compname seg txt_start txt_len intstat_start intstat_length 
    // symbol_start symbol_length
    char segname[BOOK_SEGMENT_NAME_LEN];
    char compname[BOOK_SEGMENT_NAME_LEN];
    uint segno;
    uint txt_start, txt_len;
    uint  intstat_start, intstat_length;
    uint  symbol_start, symbol_length;

    // 32 is BOOK_SEGMENT_NAME_LEN - 1
    if (sscanf (buf, "%32s %32s %o %o %o %o %o %o %o", 
                segname, compname, & segno, 
                & txt_start, & txt_len, & intstat_start, & intstat_length, 
                & symbol_start, & symbol_length) != 9)
      return SCPE_ARG;

    int idx = add_book_segment (segname, (int) segno);
    if (idx < 0)
      return SCPE_ARG;

    if (add_book_component (idx, compname, txt_start, txt_len, (int) intstat_start,
                           (int) intstat_length, (int) symbol_start, 
                           (int) symbol_length) < 0)
      return SCPE_ARG;

    return SCPE_OK;
  }

// LSB n:n   given a segment number and offset, return a segment name,
//           component and offset in that component
//     sname:cname+offset
//           given a segment name, component name and offset, return
//           the segment number and offset
   
static t_stat lookup_system_book (UNUSED int32  arg, const char * buf)
  {
    char w1[strlen (buf)];
    char w2[strlen (buf)];
    char w3[strlen (buf)];
    long segno, offset;

    size_t colon = strcspn (buf, ":");
    if (buf[colon] != ':')
      return SCPE_ARG;

    strncpy (w1, buf, colon);
    w1[colon] = '\0';
    //sim_msg ("w1 <%s>\n", w1);

    size_t plus = strcspn (buf + colon + 1, "+");
    if (buf[colon + 1 + plus] == '+')
      {
        strncpy (w2, buf + colon + 1, plus);
        w2[plus] = '\0';
        strcpy (w3, buf + colon + 1 + plus + 1);
      }
    else
      {
        strcpy (w2, buf + colon + 1);
        strcpy (w3, "");
      }
    //sim_msg ("w1 <%s>\n", w1);
    //sim_msg ("w2 <%s>\n", w2);
    //sim_msg ("w3 <%s>\n", w3);

    char * end1;
    segno = strtol (w1, & end1, 8);
    char * end2;
    offset = strtol (w2, & end2, 8);

    if (* end1 == '\0' && * end2 == '\0' && * w3 == '\0')
      { 
        // n:n
        char * ans = lookup_address ((word18) segno, (word18) offset, NULL, NULL);
        sim_warn ("%s\n", ans ? ans : "not found");
      }
    else
      {
        if (* w3)
          {
            char * end3;
            offset = strtol (w3, & end3, 8);
            if (* end3 != '\0')
              return SCPE_ARG;
          }
        else
          offset = 0;
        long comp_offset;
        int rc = lookup_system_book_name (w1, w2, & segno, & comp_offset);
        if (rc)
          {
            sim_warn ("not found\n");
            return SCPE_OK;
          }
        sim_msg ("0%o:0%o\n", (uint) segno, (uint) (comp_offset + offset));
        abs_addr_n  ((int) segno, (uint) (comp_offset + offset));
      }
/*
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    char * ans = lookup_address (segno, offset);
    sim_msg ("%s\n", ans ? ans : "not found");
*/
    return SCPE_OK;
  }

#ifndef SCUMEM
static t_stat virtAddrN (uint address)
  {
    if (cpu.DSBR.U) {
        for(word15 segno = 0; 2u * segno < 16u * (cpu.DSBR.BND + 1u); segno += 1)
        {
            sdw0_s *s = fetchSDW(segno);
            if (address >= s -> ADDR && address < s -> ADDR + s -> BOUND * 16u)
              sim_msg ("  %06o:%06o\n", segno, address - s -> ADDR);
        }
    } else {
        for(word15 segno = 0;
            2u * segno < 16u * (cpu.DSBR.BND + 1u);
            segno += 512u)
        {
            word24 y1 = (2u * segno) % 1024u;
            word24 x1 = (2u * segno - y1) / 1024u;
            word36 PTWx1;
            core_read ((cpu.DSBR.ADDR + x1) & PAMASK, & PTWx1, __func__);

            ptw_s PTW1;
            PTW1.ADDR = GETHI(PTWx1);
            PTW1.U = TSTBIT(PTWx1, 9);
            PTW1.M = TSTBIT(PTWx1, 6);
            PTW1.DF = TSTBIT(PTWx1, 2);
            PTW1.FC = PTWx1 & 3;
           
            if (PTW1.DF == 0)
                continue;
            //sim_msg ("%06o  Addr %06o U %o M %o DF %o FC %o\n", 
            //            segno, PTW1.ADDR, PTW1.U, PTW1.M, PTW1.DF, PTW1.FC);
            //sim_msg ("    Target segment page table\n");
            for (word15 tspt = 0; tspt < 512u; tspt ++)
            {
                word36 SDWeven, SDWodd;
                core_read2(((PTW1.ADDR << 6) + tspt * 2u) & PAMASK, & SDWeven,
                           & SDWodd, __func__);
                sdw0_s SDW0;
                // even word
                SDW0.ADDR = (SDWeven >> 12) & PAMASK;
                SDW0.R1 = (SDWeven >> 9) & 7u;
                SDW0.R2 = (SDWeven >> 6) & 7u;
                SDW0.R3 = (SDWeven >> 3) & 7u;
                SDW0.DF = TSTBIT(SDWeven, 2);
                SDW0.FC = SDWeven & 3u;

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

                if (SDW0.DF == 0)
                    continue;
                //sim_msg ("    %06o Addr %06o %o,%o,%o F%o BOUND %06o "
                //          "%c%c%c%c%c\n",
                //          tspt, SDW0.ADDR, SDW0.R1, SDW0.R2, SDW0.R3, SDW0.F,
                //          SDW0.BOUND, SDW0.R ? 'R' : '.', SDW0.E ? 'E' : '.', 
                //          SDW0.W ? 'W' : '.', SDW0.P ? 'P' : '.',
                //          SDW0.U ? 'U' : '.');
                if (SDW0.U == 0)
                {
                    for (word18 offset = 0;
                         offset < 16u * (SDW0.BOUND + 1u);
                         offset += 1024)
                    {
                        word24 y2 = offset % 1024;
                        word24 x2 = (offset - y2) / 1024;

                        // 10. Fetch the target segment PTW(x2) from
                        //     SDW(segno).ADDR + x2.

                        word36 PTWx2;
                        core_read ((SDW0.ADDR + x2) & PAMASK, & PTWx2, __func__);

                        ptw_s PTW2;
                        PTW2.ADDR = GETHI(PTWx2);
                        PTW2.U = TSTBIT(PTWx2, 9);
                        PTW2.M = TSTBIT(PTWx2, 6);
                        PTW2.DF = TSTBIT(PTWx2, 2);
                        PTW2.FC = PTWx2 & 3;

                        //sim_msg ("        %06o  Addr %06o U %o M %o F %o "
                        //            "FC %o\n", 
                        //            offset, PTW2.ADDR, PTW2.U, PTW2.M, PTW2.F,
                        //            PTW2.FC);
                        if (address >= PTW2.ADDR + offset &&
                            address < PTW2.ADDR + offset + 1024)
                          sim_msg ("  %06o:%06o\n", tspt, (address - offset) - PTW2.ADDR);

                      }
                  }
                else
                  {
                    if (address >= SDW0.ADDR &&
                        address < SDW0.ADDR + SDW0.BOUND * 16u)
                      sim_msg ("  %06o:%06o\n", tspt, address - SDW0.ADDR);
                  }
            }
        }
    }

    return SCPE_OK;

  }
#endif

// VIRTUAL address

#ifndef SCUMEM
static t_stat virt_address (UNUSED int32 arg, const char * buf)
  {
    uint address;
    if (sscanf (buf, "%o", & address) != 1)
      return SCPE_ARG;
    return virtAddrN (address);
  }
#endif

// search path is path:path:path....

static t_stat set_search_path (UNUSED int32 arg, UNUSED const char * buf)
  {
// Quietly ignore if debugging not enabled
#ifndef SPEED
    if (source_search_path)
      free (source_search_path);
    source_search_path = strdup (buf);
#endif
    return SCPE_OK;
  }

// Hook for gdb
//
// The idea is that if you want to set a gdb breakpoint for a particulary 
// complex condition, you can add a test for the condtion to the emulator
// code and call brkbrk() when the condition is met; by doing a gdb 
// 'b brkbrk', gdb will see when the condition is met.
//

t_stat brkbrk (UNUSED int32 arg, UNUSED const char *  buf)
  {
    //list_source (buf, 0);
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
    char reformatted[strlen (buf) + 20];
    sprintf (reformatted, "0%04o%06o%s", segno, offset, buf + where);
    //printf (">> <%s>\n", reformatted);
    t_stat rc = brk_cmd (arg, reformatted);
    return rc;
  }

#ifdef DVFDBG
static t_stat dfx1entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// divide_fx1, divide_fx3
    sim_msg ("dfx1entry\n");
    sim_msg ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_msg ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu.PR[6].SNR, 046, & pa, & msg))
      {
        sim_msg ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("text segno %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
sim_msg ("%05o:%06o\n", cpu.PR[2].SNR, cpu.rX[0]);
//dbgStackTrace ();
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.rX[0], & pa, & msg))
      {
        sim_msg ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("scale %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.PR[2].WORDNO, & pa, & msg))
      {
        sim_msg ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("divisor %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
    return SCPE_OK;
  }

static t_stat dfx1exit (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_msg ("dfx1exit\n");
    sim_msg ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_msg ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    return SCPE_OK;
  }

static t_stat dv2scale (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_msg ("dv2scale\n");
    sim_msg ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    return SCPE_OK;
  }

static t_stat dfx2entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// divide_fx2
    sim_msg ("dfx2entry\n");
    sim_msg ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_msg ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu.PR[6].SNR, 046, & pa, & msg))
      {
        sim_msg ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("text segno %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
#if 0
sim_msg ("%05o:%06o\n", cpu.PR[2].SNR, cpu.rX[0]);
//dbgStackTrace ();
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.rX[0], & pa, & msg))
      {
        sim_msg ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("scale ptr %012"PRIo64" (%llu)\n", M[pa], M[pa]);
        if ((M[pa] & 077) == 043)
          {
            word15 segno = (M[pa] >> 18u) & MASK15;
            word18 offset = (M[pa + 1] >> 18u) & MASK18;
            word24 ipa;
            if (dbgLookupAddress (segno, offset, & ipa, & msg))
              {
                sim_msg ("divisor address lookup failed because %s\n", msg);
              }
            else
              {
                sim_msg ("scale %012"PRIo64" (%llu)\n", M[ipa], M[ipa]);
              }
          }
      }
#endif
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.PR[2].WORDNO, & pa, & msg))
      {
        sim_msg ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("divisor %012"PRIo64" (%llu)\n", M[pa], M[pa]);
        sim_msg ("divisor %012"PRIo64" (%llu)\n", M[pa + 1], M[pa + 1]);
      }
    return SCPE_OK;
  }

static t_stat mdfx3entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// operator to form mod(fx2,fx1)
// entered with first arg in q, bp pointing at second

// divide_fx1, divide_fx2
    sim_msg ("mdfx3entry\n");
    //sim_msg ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_msg ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu.PR[6].SNR, 046, & pa, & msg))
      {
        sim_msg ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("text segno %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
//sim_msg ("%05o:%06o\n", cpu.PR[2].SNR, cpu.rX[0]);
//dbgStackTrace ();
#if 0
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.rX[0], & pa, & msg))
      {
        sim_msg ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("scale %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
#endif
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.PR[2].WORDNO, & pa, & msg))
      {
        sim_msg ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("divisor %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
    return SCPE_OK;
  }

static t_stat smfx1entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// operator to form mod(fx2,fx1)
// entered with first arg in q, bp pointing at second

// divide_fx1, divide_fx2
    sim_msg ("smfx1entry\n");
    //sim_msg ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_msg ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu.PR[6].SNR, 046, & pa, & msg))
      {
        sim_msg ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("text segno %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
sim_msg ("%05o:%06o\n", cpu.PR[2].SNR, cpu.rX[0]);
//dbgStackTrace ();
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.rX[0], & pa, & msg))
      {
        sim_msg ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("scale %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
    if (dbgLookupAddress (cpu.PR[2].SNR, cpu.PR[2].WORDNO, & pa, & msg))
      {
        sim_msg ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_msg ("divisor %012"PRIo64" (%llu)\n", M[pa], M[pa]);
      }
    return SCPE_OK;
  }
#endif // DVFDBG

// SEARCHMEMORY value

#ifndef SCUMEM
static t_stat search_memory (UNUSED int32 arg, const char * buf)
  {
    word36 value;
    if (sscanf (buf, "%"PRIo64"", & value) != 1)
      return SCPE_ARG;
    
    uint i;
    for (i = 0; i < MEMSIZE; i ++)
      if ((M[i] & DMASK) == value)
        sim_msg ("%08o\n", i);
    return SCPE_OK;
  }
#endif

static t_stat set_dbg_cpu_mask (int32 UNUSED arg, const char * UNUSED buf)
  {
    uint msk;
    int cnt = sscanf (buf, "%u", & msk);
    if (cnt != 1)
      {
        sim_msg ("Huh?\n");
        return SCPE_ARG;
      }
    sim_msg ("mask set to %u\n", msk);
    dbgCPUMask = msk;
    return SCPE_OK;
  }
  
#endif // TESTING

//
// Misc. commands
//

#ifdef LAUNCH
#define MAX_CHILDREN 256
static int nChildren = 0;
static pid_t childrenList[MAX_CHILDREN];

static void cleanupChildren (void)
  {
    printf ("cleanupChildren\n");
    for (int i = 0; i < nChildren; i ++)
      {
#ifndef __MINGW64__
        printf ("  kill %d\n", childrenList[i]);
        kill (childrenList[i], SIGHUP);
#else
        TerminateProcess((HANDLE)childrenList[i], 1);
        CloseHandle((HANDLE)childrenList[i]);
#endif
      }
  }

static void addChild (pid_t pid)
  {
    if (nChildren >= MAX_CHILDREN)
      return;
    childrenList[nChildren ++] = pid;
    if (nChildren == 1)
     atexit (cleanupChildren);
  }

static t_stat launch (int32 UNUSED arg, const char * buf)
  {
#ifndef __MINGW64__
    wordexp_t p;
    int rc = wordexp (buf, & p, WRDE_SHOWERR | WRDE_UNDEF);
    if (rc)
      {
        sim_msg ("wordexp failed %d\n", rc);
        return SCPE_ARG;
      }
    //for (uint i = 0; i < p.we_wordc; i ++)
      //sim_msg ("    %s\n", p.we_wordv[i]);
    pid_t pid = fork ();
    if (pid == -1) // parent, fork failed
      {
        sim_msg ("fork failed\n");
        return SCPE_ARG;
      }
    if (pid == 0)  // child
      {
        execv (p.we_wordv[0], & p.we_wordv[1]);
        sim_msg ("exec failed\n");
        exit (1);
      }
    addChild (pid);
    wordfree (& p);
#else
     STARTUPINFO si;
     PROCESS_INFORMATION pi;
 
     memset( &si, 0, sizeof(si) );
     si.cb = sizeof(si);
     memset( &pi, 0, sizeof(pi) );
 
     if( !CreateProcess( NULL, (LPSTR)buf, NULL, NULL, FALSE, 0, NULL, NULL,
                         &si, &pi ) ) 
     {
         sim_msg ("fork failed\n");
         return SCPE_ARG;
     }
     addChild ((pid_t)pi.hProcess);
#endif
    return SCPE_OK;
  }
#endif

#ifdef PANEL
static t_stat scraper (UNUSED int32 arg, const char * buf)
  {
    if (strcasecmp (buf, "start") == 0)
      return panelScraperStart ();
    if (strcasecmp (buf, "stop") == 0)
      return panelScraperStop ();
    if (strcasecmp (buf, "msg") == 0)
      {
        return panelScraperMsg (NULL);
      }
    if (strncasecmp (buf, "msg ", 4) == 0)
      {
        const char * p = buf + 4;
        while (* p == ' ')
          p ++;
        return panelScraperMsg (p);
      }
    sim_msg ("err: scraper start|stop|msg\n");
    return SCPE_ARG;
  }
#endif

////////////////////////////////////////////////////////////////////////////////
//
// simh Command table
//

static CTAB dps8_cmds[] =
  {

// Loader not supported by default

#ifdef LOADER
    {"DPSINIT",             dpsCmd_Init,              0, "dpsinit: dps8/m initialize stuff ...\n", NULL, NULL},
    {"DPSDUMP",             dpsCmd_Dump,              0, "dpsdump: dps8/m dump stuff ...\n", NULL, NULL},
    {"SEGMENT",             dpsCmd_Segment,           0, "segment: dps8/m segment stuff ...\n", NULL, NULL},
    {"SEGMENTS",            dpsCmd_Segments,          0, "segments: dps8/m segments stuff ...\n", NULL, NULL},
#endif

//
// System configuration
//

    {"DEFAULT_BASE_SYSTEM", set_default_base_system,  0, "default_base_system: Set configuration to defaults\n", NULL, NULL},

    {"CABLE",               sys_cable,                0, "cable: String a cable\n" , NULL, NULL},
    {"UNCABLE",             sys_cable,                1, "uncable: Unstring a cable\n" , NULL, NULL},
    {"CABLE_RIPOUT",        sys_cable_ripout,         0, "cable: Unstring all cables\n" , NULL, NULL},
    {"CABLE_SHOW",          sys_cable_show,           0, "cable: Show cables\n" , NULL, NULL},

    {"FNPSERVERPORT",       set_fnp_server_port,      0, "fnpserverport: Set the FNP dialin telnet port number\n", NULL, NULL},
    {"FNPSERVER3270PORT",   set_fnp_3270_server_port, 0, "fnpserver3270port: Set the FNP 3270 port number\n", NULL, NULL},

    {"CONSOLEPORT",         set_console_port,         0, "consoleport: Set the Operator Console port number\n", NULL, NULL},
    {"CONSOLEPW",           set_console_pw,           0, "consolepw: Set the Operator Console port password\n", NULL, NULL},
    {"CONSOLEPORT1",        set_console_port,         1, "consoleport1: Set the CPU-B Operator Console port number\n", NULL, NULL},
    {"CONSOLEPW1",          set_console_pw,           1, "consolepw1: Set the CPU-B Operator Console port password\n", NULL, NULL},

    {"MACHINEROOMPORT",     set_machine_room_port,    0, "machineroomport: set the machine room port number\n", NULL, NULL},
    {"MACHINEROOMPW",       set_machine_room_pw,      0, "machineroompW: set the machine room port password\n", NULL, NULL},

//
// System contol
//

    {"SKIPBOOT",            boot_skip,                0, "skipboot: Skip forward on boot tape\n", NULL, NULL},
    {"FNPSTART",            fnp_start,                0, "fnpstart: Force immediate FNP initialization\n", NULL, NULL},
    {"MOUNT",               mount_tape,               0, "mount: Mount tape image and signal Mulitcs\n", NULL, NULL },
    {"XF",                  do_execute_fault,         0, "xf: Execute fault: Press the execute fault button\n", NULL, NULL},
    {"POLL",                set_sys_polling_interval, 0, "Set polling interval in milliseconds", NULL, NULL },
    {"SLOWPOLL",           set_sys_slow_polling_interval, 0, "Set slow polling interval in polling intervals", NULL, NULL },
    {"CHECKPOLL",          set_sys_poll_check_rate, 0, "Set slow polling interval in polling intervals", NULL, NULL },

//
// Debugging
//

#ifdef TESTING
    {"DBGMMECNTDWN",        dps_debug_mme_cntdwn,     0, "dbgmmecntdwn: Enable debug after n MMEs\n", NULL, NULL},
    {"DBGSKIP",             dps_debug_skip,           0, "dbgskip: Skip first n TRACE debugs\n", NULL, NULL},
    {"DBGSTART",            dps_debug_start,          0, "dbgstart: Limit debugging to N > Cycle count\n", NULL, NULL},
    {"DBGSTOP",             dps_debug_stop,           0, "dbgstop: Limit debugging to N < Cycle count\n", NULL, NULL},
    {"DBGBREAK",            dps_debug_break,          0, "dbgstop: Break when N >= Cycle count\n", NULL, NULL},
    {"DBGSEGNO",            dps_debug_segno,          0, "dbgsegno: Limit debugging to PSR == segno\n", NULL, NULL},
    {"DBGRINGNO",           dps_debug_ringno,         0, "dbgsegno: Limit debugging to PRR == ringno\n", NULL, NULL},
    {"DBGBAR",              dps_debug_bar,            1, "dbgbar: Limit debugging to BAR mode\n", NULL, NULL},
    {"NODBGBAR",            dps_debug_bar,            0, "dbgbar: Limit debugging to BAR mode\n", NULL, NULL},
#ifdef HDBG
    {"HDBG",                hdbg_size,                0, "hdbg: set history buffer size\n", NULL, NULL},
    {"PHDBG",               hdbg_print,               0, "phdbg: display history size\n", NULL, NULL},
#endif
    {"ABSOLUTE",            abs_addr,                 0, "abs: Compute the absolute address of segno:offset\n", NULL, NULL},
#ifndef SCUMEM
    {"STK",                 stack_trace,              0, "stk: Print a stack trace\n", NULL, NULL},
#endif
    {"LIST",                list_source_at,           0, "list segno:offet: List source for an address\n", NULL, NULL},
    {"LD_SYSTEM_BOOK",      load_system_book,         0, "load_system_book: Load a Multics system book for symbolic debugging\n", NULL, NULL},
    {"ASBE",                add_system_book_entry,    0, "asbe: Add an entry to the system book\n", NULL, NULL},
    {"LOOKUP_SYSTEM_BOOK",  lookup_system_book,       0, "lookup_system_book: Lookup an address or symbol in the Multics system book\n", NULL, NULL},
    {"LSB",                 lookup_system_book,       0, "lsb: Lookup an address or symbol in the Multics system book\n", NULL, NULL},
#ifndef SCUMEM
    {"VIRTUAL",             virt_address,             0, "virtual: Compute the virtural address(es) of segno:offset\n", NULL, NULL},
#endif
    {"SPATH",               set_search_path,          0, "spath: Set source code search path\n", NULL, NULL},
    {"BT2",                 boot2,                    0, "boot2: Boot CPU-B\n", NULL, NULL},
    {"TEST",                brkbrk,                   0, "test: GDB hook\n", NULL, NULL},
// copied from scp.c
#define SSH_ST          0                               /* set */
#define SSH_SH          1                               /* show */
#define SSH_CL          2                               /* clear */
    {"SBREAK",              sbreak,               SSH_ST, "sbreak: Set a breakpoint with segno:offset syntax\n", NULL, NULL},
    {"NOSBREAK",            sbreak,               SSH_CL, "nosbreak: Unset an SBREAK\n", NULL, NULL},
#ifdef DVFDBG
    // dvf debugging
    {"DFX1ENTRY",           dfx1entry,                0, "\n", NULL, NULL},
    {"DFX2ENTRY",           dfx2entry,                0, "\n", NULL, NULL},
    {"DFX1EXIT",            dfx1exit,                 0, "\n", NULL, NULL},
    {"DV2SCALE",            dv2scale,                 0, "\n", NULL, NULL},
    {"MDFX3ENTRY",          mdfx3entry,               0, "\n", NULL, NULL},
    {"SMFX1ENTRY",          smfx1entry,               0, "\n", NULL, NULL},
#endif
    // doesn't work
    //{"DUMPKST",             dumpKST,                  0, "dumpkst: dump the Known Segment Table\n", NULL},
    {"WATCH",               set_mem_watch,            1, "watch: Watch memory location\n", NULL, NULL},
    {"NOWATCH",             set_mem_watch,            0, "watch: Unwatch memory location\n", NULL, NULL},
#ifndef SCUMEM
    {"SEARCHMEMORY",        search_memory,            0, "searchmemory: Search memory for value\n", NULL, NULL},
#endif
    {"DBGCPUMASK",          set_dbg_cpu_mask,         0, "dbgcpumask: Set per CPU debug enable", NULL, NULL},
#endif // TESTING

//
// Statistics
//

#ifdef MATRIX
    {"DISPLAYMATRIX",       display_the_matrix,         0, "displaymatrix: Display instruction usage counts\n", NULL, NULL},
#endif


//
// Console scripting
//

    {"AUTOINPUT",           add_opc_autoinput,      0, "autoinput: Set console auto-input\n", NULL, NULL},
    {"AI",                  add_opc_autoinput,      0, "ai: Set console auto-input\n", NULL, NULL},
    {"AUTOINPUT2",          add_opc_autoinput,      1, "autoinput2: Set CPU-B console auto-input\n", NULL, NULL},
    {"AI2",                 add_opc_autoinput,      1, "ai2: Set console CPU-B auto-input\n", NULL, NULL},
    {"CLRAUTOINPUT",        clear_opc_autoinput,    0, "clrautoinput: Clear console auto-input\n", NULL, NULL},
    {"CLRAUTOINPUT2",       clear_opc_autoinput,    1, "clrautoinput1: Clear CPU-B console auto-input\n", NULL, NULL},


//
// Misc.
//

#ifdef LAUNCH
    {"LAUNCH",              launch,                   0, "launch: Launch subprocess\n", NULL, NULL},
#endif

#ifdef PANEL
    {"SCRAPER",             scraper,                  0, "scraper: Control scraper\n", NULL, NULL},
#endif
    { NULL,                 NULL,                     0, NULL, NULL, NULL}
  }; // dps8_cmds

#ifndef __MINGW64__
static void usr1_signal_handler (UNUSED int sig)
  {
    sim_msg ("USR1 signal caught; pressing the EXF button\n");
    // Assume the bootload CPU
    setG7fault (ASSUME0, FAULT_EXF, fst_zero);
    return;
  }
#endif

// Once-only initialization; invoked by simh

static void dps8_init (void)
  {
#include "dps8.sha1.txt"
    sim_msg ("%s emulator (git %8.8s)\n", sim_name, COMMIT_ID);
#ifdef TESTING
    sim_msg ("#### TESTING BUILD ####\n");
#else
    sim_msg ("Production build\n");
#endif
#ifdef ISOLTS
    sim_msg ("#### ISOLTS BUILD ####\n");
#endif
#ifdef NEED_128
    sim_msg ("#### NEED_128 BUILD ####\n");
#endif
#ifdef WAM
    sim_msg ("#### WAM BUILD ####\n");
#endif
#ifdef HDBG
    sim_msg ("#### HDBG BUILD ####\n");
#endif
#ifdef ROUND_ROBIN
    sim_msg ("#### ROUND_ROBIN BUILD ####\n");
#endif

    // special dps8 initialization stuff that cant be done in reset, etc .....

#ifdef TESTING
    // These are part of the simh interface
    sim_vm_parse_addr = parse_addr;
    sim_vm_fprint_addr = fprint_addr;
#endif // TESTING

    sim_vm_cmd = dps8_cmds;

    // This is needed to make sbreak work
    sim_brk_types = sim_brk_dflt = SWMASK ('E');

#ifndef __MINGW64__
    // Create a session for this dps8m system instance.
    dps8m_sid = setsid ();
    if (dps8m_sid == (pid_t) -1)
      dps8m_sid = getsid (0);
#ifdef DPS8M
    sim_msg ("DPS8M system session id is %d\n", dps8m_sid);
#endif
#ifdef L68
    sim_msg ("L68 system session id is %d\n", dps8m_sid);
#endif
#endif

#ifndef __MINGW64__
    // Wire the XF button to signal USR1
    signal (SIGUSR1, usr1_signal_handler);
#endif

    // sets connect to 0
    memset (& sys_opts, 0, sizeof (sys_opts));
    // sys_poll_interval 10 ms (100 Hz)
    sys_opts.sys_poll_interval = 10;
    // sys_slow_poll_interval 100 polls (1 Hz)
    sys_opts.sys_slow_poll_interval = 100;
    // sys_poll_check_rate in CPU cycles
    sys_opts.sys_poll_check_rate = 1024;

    init_opcodes();
    sysCableInit ();
    iom_init ();
    disk_init ();
    mt_init ();
    fnpInit ();
    console_init (); // must come after fnpInit due to libuv initiailization
    //mpc_init ();
    scu_init ();
    cpu_init ();
    rdr_init ();
    pun_init ();
    prt_init ();
    urp_init ();
#ifndef __MINGW64__
    absi_init ();
#endif
    set_default_base_system (0, NULL);
#ifdef PANEL
    panelScraperInit ();
#endif
#if defined(THREADZ) || defined(LOCKLESS)
    initThreadz ();
#endif
  }




#ifdef TESTING 
static struct pr_table
  {
    char  * alias;    // pr alias
    int   n;          // number alias represents ....
  } _prtab[] =
  {
    {"pr0", 0}, ///< pr0 - 7
    {"pr1", 1},
    {"pr2", 2},
    {"pr3", 3},
    {"pr4", 4},
    {"pr5", 5},
    {"pr6", 6},
    {"pr7", 7},

    {"pr[0]", 0}, ///< pr0 - 7
    {"pr[1]", 1},
    {"pr[2]", 2},
    {"pr[3]", 3},
    {"pr[4]", 4},
    {"pr[5]", 5},
    {"pr[6]", 6},
    {"pr[7]", 7},
    
    // from: ftp://ftp.stratus.com/vos/multics/pg/mvm.html
    {"ap",  0},
    {"ab",  1},
    {"bp",  2},
    {"bb",  3},
    {"lp",  4},
    {"lb",  5},
    {"sp",  6},
    {"sb",  7},
    
    {0,     0}
  };

static t_addr parse_addr (UNUSED DEVICE * dptr, const char *cptr,
                          const char **optr)
  {
#ifdef SCUMEM
    return 0;
#else
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
            sim_warn("parse_addr(): illegal number of parameters\n");
            *optr = cptr;   // signal error
            return 0;
        }
        
        // determine if segment is numeric or symbolic...
        char *endp;
        word18 PRoffset = 0;   // offset from PR[n] register (if any)
        int segno = (int)strtoll(seg, &endp, 8);
        if (endp == seg)
        {
            // not numeric...
            // 1st, see if it's a PR or alias thereof
            struct pr_table *prt = _prtab;
            while (prt->alias)
            {
                if (strcasecmp(seg, prt->alias) == 0)
                {
                    segno = cpu.PR[prt->n].SNR;
                    PRoffset = cpu.PR[prt->n].WORDNO;
                    break;
                }
                
                prt += 1;
            }
            
            if (!prt->alias)    // not a PR or alias
            {
              return 0;
                segment *s = findSegmentNoCase(seg);
                if (s == NULL)
                {
                    sim_warn ("parse_addr(): segment '%s' not found\n", seg);
                    *optr = cptr;   // signal error
                    
                    return 0;
                }
                segno = s->segno;
            }
        }
        
        // determine if offset is numeric or symbolic entry point/segdef...
        uint offset = (uint)strtoll(off, &endp, 8);
        if (endp == off)
        {
            // not numeric...
            return 0;
            segdef *s = findSegdefNoCase(seg, off);
            if (s == NULL)
            {
                sim_warn ("parse_addr(): entrypoint '%s' not found in segment '%s'", off, seg);
                *optr = cptr;   // signal error

                return 0;
            }
            offset = (uint) s->value;
        }
        
        // if we get here then seg contains a segment# and offset.
        // So, fetch the actual address given the segment & offset ...
        // ... and return this absolute, 24-bit address
        
        word24 abs_addr = (word24) getAddress(segno, (int) (offset + PRoffset));
        
        // TODO: only luckily does this work FixMe
        *optr = endp;   //cptr + strlen(cptr);
        
        return abs_addr;
    }
    else
    {
        // a PR or alias thereof
        int segno = 0;
        word24 offset = 0;
        struct pr_table *prt = _prtab;
        while (prt->alias)
        {
            if (strncasecmp(cptr, prt->alias, strlen(prt->alias)) == 0)
            {
                segno = cpu.PR[prt->n].SNR;
                offset = cpu.PR[prt->n].WORDNO;
                break;
            }
            
            prt += 1;
        }
        if (prt->alias)    // a PR or alias
        {
            word24 abs_addr = (word24) getAddress(segno, (int) offset);
            *optr = cptr + strlen(prt->alias);
        
            return abs_addr;
        }
    }
    
    // No, determine absolute address given by cptr
    return (t_addr)strtol(cptr, (char **) optr, 8);
#endif // !SCUMEM
}
#endif // TESTING

#ifdef TESTING 
static void fprint_addr (FILE * stream, UNUSED DEVICE *  dptr, t_addr simh_addr)
{
#ifdef SCUMEM
    fprintf(stream, "%06o", simh_addr);
#else
    char temp[256];
    bool bFound = getSegmentAddressString((int)simh_addr, temp);
    if (bFound)
        fprintf(stream, "%s (%08o)", temp, simh_addr);
    else
        fprintf(stream, "%06o", simh_addr);
#endif
}
#endif // TESTING

// This is part of the simh interface
// Based on the switch variable, symbolically output to stream ofile the data in
//  array val at the specified addr in unit uptr.  
// simh "fprint_sym" – Based on the switch variable, symbolically output to
// stream ofile the data in array val at the specified addr in unit uptr.

t_stat fprint_sym (UNUSED FILE * ofile, UNUSED t_addr addr,
                   UNUSED t_value *val, UNUSED UNIT *uptr, int32 UNUSED sw)
{
#ifdef TESTING
// XXX Bug: assumes single cpu
// XXX CAC: This seems rather bogus; deciding the output format based on the
// address of the UNIT? Would it be better to use sim_unit.u3 (or some such 
// as a word width?

    if (!((uint) sw & SWMASK ('M')))
        return SCPE_ARG;
    
    if (uptr == &cpu_dev.units[0])
    {
        word36 word1 = *val;
        char buf[256];
        // get base syntax
        char *d = disAssemble(buf, word1);
        
        fprintf(ofile, "%s", d);
        
        // decode instruction
        DCDstruct ci;
        DCDstruct * p = & ci;
        decode_instruction (word1, p);
        
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
#endif
    return SCPE_ARG;
}

// This is part of the simh interface
//  – Based on the switch variable, parse character string cptr for a 
//  symbolic value val at the specified addr in unit uptr.

t_stat parse_sym (UNUSED const char * cptr, UNUSED t_addr addr,
                  UNUSED UNIT * uptr, UNUSED t_value * val, UNUSED int32 sswitch)
  {
    return SCPE_ARG;
  }

// from MM

sysinfo_t sys_opts;

static t_stat sys_show_config (UNUSED FILE * st, UNUSED UNIT * uptr, 
                               UNUSED int  val, UNUSED const void * desc)
  {
    sim_msg ("IOM connect time:         %d\n",
                sys_opts.iom_times.connect);
    return SCPE_OK;
}

static config_value_list_t cfg_timing_list[] =
  {
    { "disable", -1 },
    { NULL, 0 }
  };

bool breakEnable = false;

static t_stat sys_set_break (UNUSED UNIT *  uptr, int32 value, 
                             UNUSED const char * cptr, UNUSED void * desc)
  {
    breakEnable = !! value;
    return SCPE_OK;
  }

static t_stat sys_show_break (UNUSED FILE * st, UNUSED UNIT * uptr, 
                              UNUSED int  val, UNUSED const void * desc)
  {
    sim_msg ("BREAK %s\r\n", breakEnable ? "ON" : "OFF" );
    return SCPE_OK;
  }

static config_value_list_t cfg_on_off [] =
  {
    { "off", 0 },
    { "on", 1 },
    { "disable", 0 },
    { "enable", 1 },
    { NULL, 0 }
  };

static config_list_t sys_config_list[] =
  {
    { "connect_time", -1, 100000, cfg_timing_list },
    { "color", 0, 1, cfg_on_off },
    { NULL, 0, 0, NULL }
 };

static t_stat sys_set_config (UNUSED UNIT *  uptr, UNUSED int32 value, 
                              const char * cptr, UNUSED void * desc)
  {
    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("sys_set_config", cptr, sys_config_list, & cfg_state,
                           & v);
        if (rc == -1) // done
          {
            break;
          }
        if (rc == -2) // error
          {
            cfgparse_done (& cfg_state);
            return SCPE_ARG;
          }

        const char * p = sys_config_list[rc].name;
        if (strcmp (p, "connect_time") == 0)
          sys_opts.iom_times.connect = (int) v;
        else if (strcmp (p, "color") == 0)
          sys_opts.no_color = ! v;
        else
          {
            sim_msg ("error: sys_set_config: invalid cfgparse rc <%d>\n", rc);
            cfgparse_done (& cfg_state);
            return SCPE_ARG;
          } 
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }



static MTAB sys_mod[] =
  {
    {
      MTAB_dev_value, /* mask */
      0,            /* match */
      (char *) "CONFIG",     /* print string */
      (char *) "CONFIG",         /* match string */
      sys_set_config,         /* validation routine */
      sys_show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL,            /* help */
    },
    {
      MTAB_dev_novalue, /* mask */
      1,            /* match */
      (char *) "BREAK",     /* print string */
      (char *) "BREAK",         /* match string */
      sys_set_break,         /* validation routine */
      sys_show_break, /* display routine */
      NULL,          /* value descriptor */
      NULL,            /* help */
    },
    {
      MTAB_dev_novalue, /* mask */
      0,            /* match */
      (char *) "NOBREAK",     /* print string */
      (char *) "NOBREAK",         /* match string */
      sys_set_break,         /* validation routine */
      sys_show_break, /* display routine */
      NULL,          /* value descriptor */
      NULL,            /* help */
    },
    MTAB_eol
  };



static t_stat sys_reset (UNUSED DEVICE  * dptr)
  {
    return SCPE_OK;
  }

static DEVICE sys_dev = {
    "SYS",       /* name */
    NULL,        /* units */
    NULL,        /* registers */
    sys_mod,     /* modifiers */
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
DEVICE * sim_devices[] =
  {
    & cpu_dev, // dev[0] is special to simh; it is the 'default device'
    & iom_dev,
    & tape_dev,
    & mtp_dev,
    & fnp_dev,
    & dsk_dev,
    & ipc_dev,
    & msp_dev,
    & scu_dev,
    // & mpc_dev,
    & opc_dev,
    & sys_dev,
    & urp_dev,
    & rdr_dev,
    & pun_dev,
    & prt_dev,
#ifndef __MINGW64__
    & absi_dev,
#endif
    NULL
  };


////////////////////////////////////////////////////////////////////////////////
//
// Machine room
//
// The 'machine room' is an HTTP server embedded into the emulator. Eventually
// it will enable viewing the phyiscal status to the system, and controlling
// some physical aspects, such as mounting tapes.
//

static unsigned char favicon [] =
  {
    0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x20, 0x20, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0xa8, 0x08,
    0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x68, 0x05,
    0x00, 0x00, 0xce, 0x08, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x40, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x40, 0x20, 0x00, 0x00, 0x80, 0x20, 0x00, 0x00, 0xff, 0x20, 0x00, 0x00, 0x00, 0x40,
    0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0xff, 0x40, 0x00, 0x00, 0x00, 0x60,
    0x00, 0x00, 0x40, 0x60, 0x00, 0x00, 0x80, 0x60, 0x00, 0x00, 0xff, 0x60, 0x00, 0x00, 0x00, 0x80,
    0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x00, 0xa0,
    0x00, 0x00, 0x40, 0xa0, 0x00, 0x00, 0x80, 0xa0, 0x00, 0x00, 0xff, 0xa0, 0x00, 0x00, 0x00, 0xc0,
    0x00, 0x00, 0x40, 0xc0, 0x00, 0x00, 0x80, 0xc0, 0x00, 0x00, 0xff, 0xc0, 0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x40, 0xff, 0x00, 0x00, 0x80, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
    0x20, 0x00, 0x40, 0x00, 0x20, 0x00, 0x80, 0x00, 0x20, 0x00, 0xff, 0x00, 0x20, 0x00, 0x00, 0x20,
    0x20, 0x00, 0x40, 0x20, 0x20, 0x00, 0x80, 0x20, 0x20, 0x00, 0xff, 0x20, 0x20, 0x00, 0x00, 0x40,
    0x20, 0x00, 0x40, 0x40, 0x20, 0x00, 0x80, 0x40, 0x20, 0x00, 0xff, 0x40, 0x20, 0x00, 0x00, 0x60,
    0x20, 0x00, 0x40, 0x60, 0x20, 0x00, 0x80, 0x60, 0x20, 0x00, 0xff, 0x60, 0x20, 0x00, 0x00, 0x80,
    0x20, 0x00, 0x40, 0x80, 0x20, 0x00, 0x80, 0x80, 0x20, 0x00, 0xff, 0x80, 0x20, 0x00, 0x00, 0xa0,
    0x20, 0x00, 0x40, 0xa0, 0x20, 0x00, 0x80, 0xa0, 0x20, 0x00, 0xff, 0xa0, 0x20, 0x00, 0x00, 0xc0,
    0x20, 0x00, 0x40, 0xc0, 0x20, 0x00, 0x80, 0xc0, 0x20, 0x00, 0xff, 0xc0, 0x20, 0x00, 0x00, 0xff,
    0x20, 0x00, 0x40, 0xff, 0x20, 0x00, 0x80, 0xff, 0x20, 0x00, 0xff, 0xff, 0x20, 0x00, 0x00, 0x00,
    0x40, 0x00, 0x40, 0x00, 0x40, 0x00, 0x80, 0x00, 0x40, 0x00, 0xff, 0x00, 0x40, 0x00, 0x00, 0x20,
    0x40, 0x00, 0x40, 0x20, 0x40, 0x00, 0x80, 0x20, 0x40, 0x00, 0xff, 0x20, 0x40, 0x00, 0x00, 0x40,
    0x40, 0x00, 0x40, 0x40, 0x40, 0x00, 0x80, 0x40, 0x40, 0x00, 0xff, 0x40, 0x40, 0x00, 0x00, 0x60,
    0x40, 0x00, 0x40, 0x60, 0x40, 0x00, 0x80, 0x60, 0x40, 0x00, 0xff, 0x60, 0x40, 0x00, 0x00, 0x80,
    0x40, 0x00, 0x40, 0x80, 0x40, 0x00, 0x80, 0x80, 0x40, 0x00, 0xff, 0x80, 0x40, 0x00, 0x00, 0xa0,
    0x40, 0x00, 0x40, 0xa0, 0x40, 0x00, 0x80, 0xa0, 0x40, 0x00, 0xff, 0xa0, 0x40, 0x00, 0x00, 0xc0,
    0x40, 0x00, 0x40, 0xc0, 0x40, 0x00, 0x80, 0xc0, 0x40, 0x00, 0xff, 0xc0, 0x40, 0x00, 0x00, 0xff,
    0x40, 0x00, 0x40, 0xff, 0x40, 0x00, 0x80, 0xff, 0x40, 0x00, 0xff, 0xff, 0x40, 0x00, 0x00, 0x00,
    0x60, 0x00, 0x40, 0x00, 0x60, 0x00, 0x80, 0x00, 0x60, 0x00, 0xff, 0x00, 0x60, 0x00, 0x00, 0x20,
    0x60, 0x00, 0x40, 0x20, 0x60, 0x00, 0x80, 0x20, 0x60, 0x00, 0xff, 0x20, 0x60, 0x00, 0x00, 0x40,
    0x60, 0x00, 0x40, 0x40, 0x60, 0x00, 0x80, 0x40, 0x60, 0x00, 0xff, 0x40, 0x60, 0x00, 0x00, 0x60,
    0x60, 0x00, 0x40, 0x60, 0x60, 0x00, 0x80, 0x60, 0x60, 0x00, 0xff, 0x60, 0x60, 0x00, 0x00, 0x80,
    0x60, 0x00, 0x40, 0x80, 0x60, 0x00, 0x80, 0x80, 0x60, 0x00, 0xff, 0x80, 0x60, 0x00, 0x00, 0xa0,
    0x60, 0x00, 0x40, 0xa0, 0x60, 0x00, 0x80, 0xa0, 0x60, 0x00, 0xff, 0xa0, 0x60, 0x00, 0x00, 0xc0,
    0x60, 0x00, 0x40, 0xc0, 0x60, 0x00, 0x80, 0xc0, 0x60, 0x00, 0xff, 0xc0, 0x60, 0x00, 0x00, 0xff,
    0x60, 0x00, 0x40, 0xff, 0x60, 0x00, 0x80, 0xff, 0x60, 0x00, 0xff, 0xff, 0x60, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x40, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xff, 0x00, 0x80, 0x00, 0x00, 0x20,
    0x80, 0x00, 0x40, 0x20, 0x80, 0x00, 0x80, 0x20, 0x80, 0x00, 0xff, 0x20, 0x80, 0x00, 0x00, 0x40,
    0x80, 0x00, 0x40, 0x40, 0x80, 0x00, 0x80, 0x40, 0x80, 0x00, 0xff, 0x40, 0x80, 0x00, 0x00, 0x60,
    0x80, 0x00, 0x40, 0x60, 0x80, 0x00, 0x80, 0x60, 0x80, 0x00, 0xff, 0x60, 0x80, 0x00, 0x00, 0x80,
    0x80, 0x00, 0x40, 0x80, 0x80, 0x00, 0x80, 0x80, 0x80, 0x00, 0xff, 0x80, 0x80, 0x00, 0x00, 0xa0,
    0x80, 0x00, 0x40, 0xa0, 0x80, 0x00, 0x80, 0xa0, 0x80, 0x00, 0xff, 0xa0, 0x80, 0x00, 0x00, 0xc0,
    0x80, 0x00, 0x40, 0xc0, 0x80, 0x00, 0x80, 0xc0, 0x80, 0x00, 0xff, 0xc0, 0x80, 0x00, 0x00, 0xff,
    0x80, 0x00, 0x40, 0xff, 0x80, 0x00, 0x80, 0xff, 0x80, 0x00, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00,
    0xa0, 0x00, 0x40, 0x00, 0xa0, 0x00, 0x80, 0x00, 0xa0, 0x00, 0xff, 0x00, 0xa0, 0x00, 0x00, 0x20,
    0xa0, 0x00, 0x40, 0x20, 0xa0, 0x00, 0x80, 0x20, 0xa0, 0x00, 0xff, 0x20, 0xa0, 0x00, 0x00, 0x40,
    0xa0, 0x00, 0x40, 0x40, 0xa0, 0x00, 0x80, 0x40, 0xa0, 0x00, 0xff, 0x40, 0xa0, 0x00, 0x00, 0x60,
    0xa0, 0x00, 0x40, 0x60, 0xa0, 0x00, 0x80, 0x60, 0xa0, 0x00, 0xff, 0x60, 0xa0, 0x00, 0x00, 0x80,
    0xa0, 0x00, 0x40, 0x80, 0xa0, 0x00, 0x80, 0x80, 0xa0, 0x00, 0xff, 0x80, 0xa0, 0x00, 0x00, 0xa0,
    0xa0, 0x00, 0x40, 0xa0, 0xa0, 0x00, 0x80, 0xa0, 0xa0, 0x00, 0xff, 0xa0, 0xa0, 0x00, 0x00, 0xc0,
    0xa0, 0x00, 0x40, 0xc0, 0xa0, 0x00, 0x80, 0xc0, 0xa0, 0x00, 0xff, 0xc0, 0xa0, 0x00, 0x00, 0xff,
    0xa0, 0x00, 0x40, 0xff, 0xa0, 0x00, 0x80, 0xff, 0xa0, 0x00, 0xff, 0xff, 0xa0, 0x00, 0x00, 0x00,
    0xc0, 0x00, 0x40, 0x00, 0xc0, 0x00, 0x80, 0x00, 0xc0, 0x00, 0xff, 0x00, 0xc0, 0x00, 0x00, 0x20,
    0xc0, 0x00, 0x40, 0x20, 0xc0, 0x00, 0x80, 0x20, 0xc0, 0x00, 0xff, 0x20, 0xc0, 0x00, 0x00, 0x40,
    0xc0, 0x00, 0x40, 0x40, 0xc0, 0x00, 0x80, 0x40, 0xc0, 0x00, 0xff, 0x40, 0xc0, 0x00, 0x00, 0x60,
    0xc0, 0x00, 0x40, 0x60, 0xc0, 0x00, 0x80, 0x60, 0xc0, 0x00, 0xff, 0x60, 0xc0, 0x00, 0x00, 0x80,
    0xc0, 0x00, 0x40, 0x80, 0xc0, 0x00, 0x80, 0x80, 0xc0, 0x00, 0xff, 0x80, 0xc0, 0x00, 0x00, 0xa0,
    0xc0, 0x00, 0x40, 0xa0, 0xc0, 0x00, 0x80, 0xa0, 0xc0, 0x00, 0xff, 0xa0, 0xc0, 0x00, 0x00, 0xc0,
    0xc0, 0x00, 0x40, 0xc0, 0xc0, 0x00, 0x80, 0xc0, 0xc0, 0x00, 0xff, 0xc0, 0xc0, 0x00, 0x00, 0xff,
    0xc0, 0x00, 0x40, 0xff, 0xc0, 0x00, 0x80, 0xff, 0xc0, 0x00, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x40, 0x00, 0xff, 0x00, 0x80, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x20,
    0xff, 0x00, 0x40, 0x20, 0xff, 0x00, 0x80, 0x20, 0xff, 0x00, 0xff, 0x20, 0xff, 0x00, 0x00, 0x40,
    0xff, 0x00, 0x40, 0x40, 0xff, 0x00, 0x80, 0x40, 0xff, 0x00, 0xff, 0x40, 0xff, 0x00, 0x00, 0x60,
    0xff, 0x00, 0x40, 0x60, 0xff, 0x00, 0x80, 0x60, 0xff, 0x00, 0xff, 0x60, 0xff, 0x00, 0x00, 0x80,
    0xff, 0x00, 0x40, 0x80, 0xff, 0x00, 0x80, 0x80, 0xff, 0x00, 0xff, 0x80, 0xff, 0x00, 0x00, 0xa0,
    0xff, 0x00, 0x40, 0xa0, 0xff, 0x00, 0x80, 0xa0, 0xff, 0x00, 0xff, 0xa0, 0xff, 0x00, 0x00, 0xc0,
    0xff, 0x00, 0x40, 0xc0, 0xff, 0x00, 0x80, 0xc0, 0xff, 0x00, 0xff, 0xc0, 0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x40, 0xff, 0xff, 0x00, 0x80, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xc0,
    0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff,
    0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xff, 0xc0,
    0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0,
    0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0,
    0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xc0,
    0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xc0, 0xc0, 0xff, 0xc0, 0xc0,
    0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff,
    0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0,
    0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xc0, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xff, 0xc0, 0xc0, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xc0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00,
    0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0xcc, 0xff, 0xff, 0x00, 0x99, 0xff,
    0xff, 0x00, 0x66, 0xff, 0xff, 0x00, 0x33, 0xff, 0xff, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0xcc,
    0xff, 0x00, 0xcc, 0xcc, 0xff, 0x00, 0x99, 0xcc, 0xff, 0x00, 0x66, 0xcc, 0xff, 0x00, 0x33, 0xcc,
    0xff, 0x00, 0x00, 0xcc, 0xff, 0x00, 0xff, 0x99, 0xff, 0x00, 0xcc, 0x99, 0xff, 0x00, 0x99, 0x99,
    0xff, 0x00, 0x66, 0x99, 0xff, 0x00, 0x33, 0x99, 0xff, 0x00, 0x00, 0x99, 0xff, 0x00, 0xff, 0x66,
    0xff, 0x00, 0xcc, 0x66, 0xff, 0x00, 0x99, 0x66, 0xff, 0x00, 0x66, 0x66, 0xff, 0x00, 0x33, 0x66,
    0xff, 0x00, 0x00, 0x66, 0xff, 0x00, 0xff, 0x33, 0xff, 0x00, 0xcc, 0x33, 0xff, 0x00, 0x99, 0x33,
    0xff, 0x00, 0x66, 0x33, 0xff, 0x00, 0x33, 0x33, 0xff, 0x00, 0x00, 0x33, 0xff, 0x00, 0xff, 0x00,
    0xff, 0x00, 0xcc, 0x00, 0xff, 0x00, 0x99, 0x00, 0xff, 0x00, 0x66, 0x00, 0xff, 0x00, 0x33, 0x00,
    0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0xff, 0xcc, 0x00, 0xcc, 0xff, 0xcc, 0x00, 0x99, 0xff,
    0xcc, 0x00, 0x66, 0xff, 0xcc, 0x00, 0x33, 0xff, 0xcc, 0x00, 0x00, 0xff, 0xcc, 0x00, 0xff, 0xcc,
    0xcc, 0x00, 0xcc, 0xcc, 0xcc, 0x00, 0x99, 0xcc, 0xcc, 0x00, 0x66, 0xcc, 0xcc, 0x00, 0x33, 0xcc,
    0xcc, 0x00, 0x00, 0xcc, 0xcc, 0x00, 0xff, 0x99, 0xcc, 0x00, 0xcc, 0x99, 0xcc, 0x00, 0x99, 0x99,
    0xcc, 0x00, 0x66, 0x99, 0xcc, 0x00, 0x33, 0x99, 0xcc, 0x00, 0x00, 0x99, 0xcc, 0x00, 0xff, 0x66,
    0xcc, 0x00, 0xcc, 0x66, 0xcc, 0x00, 0x99, 0x66, 0xcc, 0x00, 0x66, 0x66, 0xcc, 0x00, 0x33, 0x66,
    0xcc, 0x00, 0x00, 0x66, 0xcc, 0x00, 0xff, 0x33, 0xcc, 0x00, 0xcc, 0x33, 0xcc, 0x00, 0x99, 0x33,
    0xcc, 0x00, 0x66, 0x33, 0xcc, 0x00, 0x33, 0x33, 0xcc, 0x00, 0x00, 0x33, 0xcc, 0x00, 0xff, 0x00,
    0xcc, 0x00, 0xcc, 0x00, 0xcc, 0x00, 0x99, 0x00, 0xcc, 0x00, 0x66, 0x00, 0xcc, 0x00, 0x33, 0x00,
    0xcc, 0x00, 0x00, 0x00, 0xcc, 0x00, 0xff, 0xff, 0x99, 0x00, 0xcc, 0xff, 0x99, 0x00, 0x99, 0xff,
    0x99, 0x00, 0x66, 0xff, 0x99, 0x00, 0x33, 0xff, 0x99, 0x00, 0x00, 0xff, 0x99, 0x00, 0xff, 0xcc,
    0x99, 0x00, 0xcc, 0xcc, 0x99, 0x00, 0x99, 0xcc, 0x99, 0x00, 0x66, 0xcc, 0x99, 0x00, 0x33, 0xcc,
    0x99, 0x00, 0x00, 0xcc, 0x99, 0x00, 0xff, 0x99, 0x99, 0x00, 0xcc, 0x99, 0x99, 0x00, 0x99, 0x99,
    0x99, 0x00, 0x66, 0x99, 0x99, 0x00, 0x33, 0x99, 0x99, 0x00, 0x00, 0x99, 0x99, 0x00, 0xff, 0x66,
    0x99, 0x00, 0xcc, 0x66, 0x99, 0x00, 0x99, 0x66, 0x99, 0x00, 0x66, 0x66, 0x99, 0x00, 0x33, 0x66,
    0x99, 0x00, 0x00, 0x66, 0x99, 0x00, 0xff, 0x33, 0x99, 0x00, 0xcc, 0x33, 0x99, 0x00, 0x99, 0x33,
    0x99, 0x00, 0x66, 0x33, 0x99, 0x00, 0x33, 0x33, 0x99, 0x00, 0x00, 0x33, 0x99, 0x00, 0xff, 0x00,
    0x99, 0x00, 0xcc, 0x00, 0x99, 0x00, 0x99, 0x00, 0x99, 0x00, 0x66, 0x00, 0x99, 0x00, 0x33, 0x00,
    0x99, 0x00, 0x00, 0x00, 0x99, 0x00, 0xff, 0xff, 0x66, 0x00, 0xcc, 0xff, 0x66, 0x00, 0x99, 0xff,
    0x66, 0x00, 0x66, 0xff, 0x66, 0x00, 0x33, 0xff, 0x66, 0x00, 0x00, 0xff, 0x66, 0x00, 0xff, 0xcc,
    0x66, 0x00, 0xcc, 0xcc, 0x66, 0x00, 0x99, 0xcc, 0x66, 0x00, 0x66, 0xcc, 0x66, 0x00, 0x33, 0xcc,
    0x66, 0x00, 0x00, 0xcc, 0x66, 0x00, 0xff, 0x99, 0x66, 0x00, 0xcc, 0x99, 0x66, 0x00, 0x99, 0x99,
    0x66, 0x00, 0x66, 0x99, 0x66, 0x00, 0x33, 0x99, 0x66, 0x00, 0x00, 0x99, 0x66, 0x00, 0xff, 0x66,
    0x66, 0x00, 0xcc, 0x66, 0x66, 0x00, 0x99, 0x66, 0x66, 0x00, 0x66, 0x66, 0x66, 0x00, 0x33, 0x66,
    0x66, 0x00, 0x00, 0x66, 0x66, 0x00, 0xff, 0x33, 0x66, 0x00, 0xcc, 0x33, 0x66, 0x00, 0x99, 0x33,
    0x66, 0x00, 0x66, 0x33, 0x66, 0x00, 0x33, 0x33, 0x66, 0x00, 0x00, 0x33, 0x66, 0x00, 0xff, 0x00,
    0x66, 0x00, 0xcc, 0x00, 0x66, 0x00, 0x99, 0x00, 0x66, 0x00, 0x66, 0x00, 0x66, 0x00, 0x33, 0x00,
    0x66, 0x00, 0x00, 0x00, 0x66, 0x00, 0xff, 0xff, 0x33, 0x00, 0xcc, 0xff, 0x33, 0x00, 0x99, 0xff,
    0x33, 0x00, 0x66, 0xff, 0x33, 0x00, 0x33, 0xff, 0x33, 0x00, 0x00, 0xff, 0x33, 0x00, 0xff, 0xcc,
    0x33, 0x00, 0xcc, 0xcc, 0x33, 0x00, 0x99, 0xcc, 0x33, 0x00, 0x66, 0xcc, 0x33, 0x00, 0x33, 0xcc,
    0x33, 0x00, 0x00, 0xcc, 0x33, 0x00, 0xff, 0x99, 0x33, 0x00, 0xcc, 0x99, 0x33, 0x00, 0x99, 0x99,
    0x33, 0x00, 0x66, 0x99, 0x33, 0x00, 0x33, 0x99, 0x33, 0x00, 0x00, 0x99, 0x33, 0x00, 0xff, 0x66,
    0x33, 0x00, 0xcc, 0x66, 0x33, 0x00, 0x99, 0x66, 0x33, 0x00, 0x66, 0x66, 0x33, 0x00, 0x33, 0x66,
    0x33, 0x00, 0x00, 0x66, 0x33, 0x00, 0xff, 0x33, 0x33, 0x00, 0xcc, 0x33, 0x33, 0x00, 0x99, 0x33,
    0x33, 0x00, 0x66, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x00, 0x33, 0x33, 0x00, 0xff, 0x00,
    0x33, 0x00, 0xcc, 0x00, 0x33, 0x00, 0x99, 0x00, 0x33, 0x00, 0x66, 0x00, 0x33, 0x00, 0x33, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x33, 0x00, 0xff, 0xff, 0x00, 0x00, 0xcc, 0xff, 0x00, 0x00, 0x99, 0xff,
    0x00, 0x00, 0x66, 0xff, 0x00, 0x00, 0x33, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0xcc,
    0x00, 0x00, 0xcc, 0xcc, 0x00, 0x00, 0x99, 0xcc, 0x00, 0x00, 0x66, 0xcc, 0x00, 0x00, 0x33, 0xcc,
    0x00, 0x00, 0x00, 0xcc, 0x00, 0x00, 0xff, 0x99, 0x00, 0x00, 0xcc, 0x99, 0x00, 0x00, 0x99, 0x99,
    0x00, 0x00, 0x66, 0x99, 0x00, 0x00, 0x33, 0x99, 0x00, 0x00, 0x00, 0x99, 0x00, 0x00, 0xff, 0x66,
    0x00, 0x00, 0xcc, 0x66, 0x00, 0x00, 0x99, 0x66, 0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x33, 0x66,
    0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0xff, 0x33, 0x00, 0x00, 0xcc, 0x33, 0x00, 0x00, 0x99, 0x33,
    0x00, 0x00, 0x66, 0x33, 0x00, 0x00, 0x33, 0x33, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0xff, 0x00,
    0x00, 0x00, 0xcc, 0x00, 0x00, 0x00, 0x99, 0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x33, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xee, 0x00, 0x00, 0x00, 0xdd, 0x00, 0x00, 0x00, 0xbb, 0x00, 0x00, 0x00,
    0xaa, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00,
    0x44, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0xee, 0x00, 0x00, 0x00, 0xdd,
    0x00, 0x00, 0x00, 0xbb, 0x00, 0x00, 0x00, 0xaa, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x77,
    0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x11,
    0x00, 0x00, 0xee, 0x00, 0x00, 0x00, 0xdd, 0x00, 0x00, 0x00, 0xbb, 0x00, 0x00, 0x00, 0xaa, 0x00,
    0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00, 0x44, 0x00,
    0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0xee, 0xee, 0xee, 0x00, 0xdd, 0xdd,
    0xdd, 0x00, 0xbb, 0xbb, 0xbb, 0x00, 0xaa, 0xaa, 0xaa, 0x00, 0x88, 0x88, 0x88, 0x00, 0x77, 0x77,
    0x77, 0x00, 0x55, 0x55, 0x55, 0x00, 0x44, 0x44, 0x44, 0x00, 0x22, 0x22, 0x22, 0x00, 0x11, 0x11,
    0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8,
    0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xd8, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8,
    0xd8, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xd8, 0xd8, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0xd8, 0xd8, 0xd8, 0xd8,
    0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0xd8, 0x00, 0xd8, 0xd8, 0x00,
    0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0xd8, 0xd8, 0x00, 0x00, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8,
    0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0xd8, 0xd8, 0x00, 0xd8, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0xd8, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00,
    0xd8, 0x00, 0xd8, 0x00, 0xd8, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0xd8, 0xd8, 0xd8, 0xd8,
    0x00, 0x00, 0xd8, 0x00, 0xd8, 0x00, 0x00, 0x00, 0xd8, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xd8, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0xd8, 0xd8, 0xd8, 0xd8,
    0xd8, 0xd8, 0xd8, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xd8, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0xd8, 0xd8, 0xd8, 0xd8,
    0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };


#define W(x) accessStartWriteStr (sys_opts.machine_room_access.client, x)

static void http_do_get (char * uri)
  {
    char buf [4096];
    if (strcmp (uri, "/") == 0)
      {
        W ("HTTP/1.1 200 OK\r\n");
        W ("\r\n");
        W ("<!DOCTYPE html>\r\n");
        W ("<html>\r\n");
        W ("<head>\r\n");
        //W ("<title>Page Title</title>\r\n");
        sprintf (buf, "<title>%s</title>\r\n", sim_name); W (buf);
        W ("</head>\r\n");
        W ("<body>\r\n");
        //W ("<span style=\"font-family: monospace;\">\r\n");
        W ("<h5>CPU</h5>\r\n");
        //W ("<p>This is a paragraph.</p>\r\n");
        for (uint i = 0; i < cpu_dev.numunits; i ++)
          {
            sprintf (buf,
                     "<p>%c serial # %u MIPS %4.2f %s%s PPR %05o:%06o R%u P%u</p>\r\n",
                     cpus[i].switches.cpu_num + 'A',
                     cpus[i].switches.serno,
                     (cpus[i].instrCntT1 - cpu.instrCntT0) / 1000000.0,
                     get_addr_mode () == ABSOLUTE_mode ? "ABS" : "APP",
                     get_bar_mode () ? "BAR" : "",
                     cpus[i].PPR.PSR,
                     cpus[i].PPR.IC,
                     cpus[i].PPR.PRR,
                     cpus[i].PPR.P);
            W (buf);
//#define A(x) (getbits36_1 (cpu.rA, x) ? "&bull" : " ")
#define A(x) (getbits36_1 (cpu.rA, x) ? "1" : "0")
            sprintf (buf,
             "<p>   A %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s</p>\r\n",
             A ( 0), A ( 1), A ( 2), A ( 3), A ( 4), A ( 5), A ( 6), A ( 7), A ( 8), 
             A ( 9), A (10), A (11), A (12), A (13), A (14), A (15), A (16), A (17), 
             A (18), A (19), A (20), A (21), A (22), A (23), A (24), A (25), A (26), 
             A (27), A (28), A (29), A (30), A (31), A (32), A (33), A (34), A (35));
            W (buf);
//#define Q(x) (getbits36_1 (cpu.rQ, x) ? "&bull" : " ")
#define Q(x) (getbits36_1 (cpu.rQ, x) ? "1" : "0")
            sprintf (buf,
             "<p>   Q %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s</p>\r\n",
             Q ( 0), Q ( 1), Q ( 2), Q ( 3), Q ( 4), Q ( 5), Q ( 6), Q ( 7), Q ( 8), 
             Q ( 9), Q (10), Q (11), Q (12), Q (13), Q (14), Q (15), Q (16), Q (17), 
             Q (18), Q (19), Q (20), Q (21), Q (22), Q (23), Q (24), Q (25), Q (26), 
             Q (27), Q (28), Q (29), Q (30), Q (31), Q (32), Q (33), Q (34), Q (35));
            W (buf);
          }

        W ("<h5>TAPE</h5>\r\n");
        for (uint i = 0; i < tape_dev.numunits; i ++)
          {
            UNIT * u = mt_unit + i;
            if (! u->fileref)
              continue;
            sprintf (buf,
                     "<p>%s %s record %d</p>\r\n",
                     tape_states[i].device_name,
                     u->filename,
                     tape_states[i].rec_num);
            W (buf);
          }
        //W ("</span>\r\n");
        W ("</body>\r\n");
        W ("</html>\r\n");
        W ("\r\n");
      }
    else if (strcmp (uri, "/favicon.ico") == 0)
      {
        W ("HTTP/1.1 200 OK\r\n");
        W ("\r\n");
        accessStartWrite (sys_opts.machine_room_access.client, (char *) favicon, sizeof (favicon));
      }
    else
      sim_warn ("http_do_get ? <%s>\r\n", uri);
    accessCloseConnection ((uv_stream_t *) sys_opts.machine_room_access.client);
  }

static void http_do (void)
  {
    switch (sys_opts.httpRequest)
      {
        case hrGet:
          {
            http_do_get (sys_opts.http_get_URI);
          }
        break;

        default:
          {
            sim_warn ("%s dazed and confused: %d\n", __func__, sys_opts.httpState);
          }
          break;
      }
  }

static void http_parse_get (char * buf)
  {
    char * p = buf;
    while (* p == ' ')
      p ++;
    char * q = p;
    while (* q && * q != ' ')
      q ++;
    size_t l = (size_t) (q - p);
    if (l <= 0)
      return;
    if (l >= MR_BUFFER_SZ)
      l = MR_BUFFER_SZ - 1;
    strncpy (sys_opts.http_get_URI, p, l);
    sys_opts.http_get_URI[l] = 0;
    //sim_msg ("uri <%s>\r\n", sys_opts.http_get_URI);
  }

static void http_parse (char * buf)
  {
    char * p = buf;
    while (* p == ' ')
      p ++;
    char * q = p;
    while (* q && * q != ' ')
      q ++;
    size_t l = (size_t) (q - p);

    switch (sys_opts.httpState)
      {
        case hsInitial:
          {
            // Get header field name
            if (l == 3 && strncmp (buf, "GET", 3) == 0)
              {
                http_parse_get (q);
                sys_opts.httpRequest = hrGet;
                sys_opts.httpState = hsFields;
              }
            else
              sim_warn ("%s ignoring <%s>\n", __func__, buf);
          }
          break;

        case hsFields:
          {
            //if (strcmp (buf, "\r\n") == 0)
            if (l == 0)
              {
                //sim_msg ("end of fields");
                http_do ();
                sys_opts.httpState = hsInitial;
              }
          }
          break;

        default:
          {
            sim_warn ("%s dazed and confused: %d\n", __func__, sys_opts.httpState);
          }
          break;
      }
  }

static void machine_room_connected (UNUSED uv_tcp_t * client)
  {
  }

void machine_room_process (void)
  {
    uv_access * access = & sys_opts.machine_room_access;
    //int c = accessGetChar (access);
    int c;
    while ((c = accessGetChar (access)) != SCPE_OK)
      {
        if (c < SCPE_KFLAG)
          {
            //sim_warn ("Bad char\n");
            continue; // Should be impossible
          }
        c -= SCPE_KFLAG;    // translate to ascii

        if (c == 0) // no char
          continue;

        //sim_msg ("%c", c);

        if (c == '\177' || c == '\010')  // backspace/del
          {
            if (sys_opts.mr_buffer_cnt > 0)
              {
                sys_opts.mr_buffer_cnt --;
                sys_opts.mr_buffer[sys_opts.mr_buffer_cnt] = 0;
                //console_putstr (conUnitIdx,  "\b \b");
              }
            continue;
          }

        //if (c == '\022')  // ^R
        //  {
        //    console_putstr (conUnitIdx,  "^R\r\nSIMH> ");
        //    for (int i = 0; i < sys_opts.mr_buffer_cnt; i ++)
        //      console_putchar (conUnitIdx, (char) (access->mr_buffer[i]));
        //    return;
        //  }

        if (c == '\025')  // ^U
          {
            //console_putstr (conUnitIdx,  "^U\r\nSIMH> ");
            sys_opts.mr_buffer_cnt = 0;
            continue;
          }

        //if (c == '\012' || c == '\015')  // CR/LF
        if (c == '\012')  // CR
          {
            sys_opts.mr_buffer[sys_opts.mr_buffer_cnt] = 0;
            //sim_msg ("recvd: <%s>\r\n", sys_opts.mr_buffer);
            http_parse (sys_opts.mr_buffer);

            sys_opts.mr_buffer_cnt = 0;
            sys_opts.mr_buffer[0] = 0;
            //access->simh_attn_pressed = false;
            continue;
          }

        if (c == '\033' || c == '\004' || c == '\032')  // ESC/^D/^Z
          {
            //console_putstr (conUnitIdx,  "\r\nSIMH cancel\r\n");
            // Empty input buffer
            sys_opts.mr_buffer_cnt = 0;
            sys_opts.mr_buffer [0] = 0;
            //access->simh_attn_pressed = false;
            continue;
          }

        if (isprint (c))
          {
            // silently drop buffer overrun
            if (sys_opts.mr_buffer_cnt + 1 >= MR_BUFFER_SZ)
              continue;
            sys_opts.mr_buffer[sys_opts.mr_buffer_cnt ++] = (char) c;
            //console_putchar (conUnitIdx, (char) c);
            continue;
          }
      }
  }


static void machine_room_connect_prompt (uv_tcp_t * client)
  {
    accessStartWriteStr (client, "password: \r\n");
    uv_access * access = (uv_access *) client->data;
    access->pwPos = 0;
  }


void start_machine_room (void)
  {
    sys_opts.machine_room_access.connectPrompt = machine_room_connect_prompt;
    sys_opts.machine_room_access.connected = machine_room_connected;
    sys_opts.machine_room_access.useTelnet = false;
    sys_opts.mr_buffer_cnt = 0;
    sys_opts.httpState = hsInitial;
    sys_opts.httpRequest = hrNone;
    uv_open_access (& sys_opts.machine_room_access);
  }

