/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2017 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//
//  dps8_scu.c  4MW SCU Emulator
//  dps8
//
//  Created by Harry Reed on 6/15/13.
//  Original portions Copyright (c) 2013 Harry Reed. All rights reserved.
//
//  Derived (& originally stolen) from .....

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

/*
 * scu.c -- System Controller
 *
 * See AN70, section 8 and GB61.
 *
 * There were a few variations of SCs and SCUs:
 * SCU -- Series 60 Level 66 Controller
 * SC -- Level 68 System Controller
 * 4MW SCU -- A later version of the Level 68 SC
 *
 * SCUs control access to memory.
 * Each SCU owns a certain range of absolute memory.
 * This emulator allows the CPU to access memory directly however.
 * SCUs contain clocks.
 * SCUS also contain facilities which allow CPUS and IOMs to communicate.
 * CPUs or IOMS request access to memory via the SCU.
 * CPUs use the CIOC instr to talk to IOMs and other CPUs via a SCU.
 * IOMs use interrupts to ask a SCU to signal a CPU.
 * Other Interesting instructions:
 * read system controller reg and set system controller reg (RSCR & SSCR)
 *  
 */


/*
 * Physical Details & Interconnection -- AN70, section 8.
 * 
 * SCUs have 8 ports.
 * Active modules (CPUs and IOMs) have up to four of their ports
 * connected to SCU ports.
 *
 * The 4MW SCU has eight on/off switches to enable or disable
 * the ports.  However, the associated registers allow for
 * values of enabled, disabled, and program control.
 *
 * SCUs have stores (memory banks).
 *
 * SCUs have four sets of registers controlling interrupts.  Only two
 * of these sets, designated "A" and "B" are used.  Each set has:
 * Execute interrupt mask register -- 32 bits; enables/disables
 * the corresponding execute interrupt cell
 * Interrupt mask assignment register -- 9 bits total
 * two parts: assigned bit, set of assigned ports (8 bits)
 * In Multics, only one CPU will be assigned in either mask
 * and no CPU appears in both.   Earlier hardware versions had
 * four 10-position rotary switches.  Later hardware versions had
 * two 9-position (0..7 and off) rotary switches.
 *  
 * Config panel -- level 68 6000 SCU
 * -- from AM81
 * store A and store B
 * 3 position rotary switch: on line, maint, off line
 * size: 32K, 64K, 128K, 256K
 * exec interrupt mask assignment
 * four 10-position rotary switches (A through D): off, 0, .. 7, M
 * One switch for each program interrupt register
 * Assign mask registers to system ports
 * Normally assign one mask reg to each CPU
 * 
 *   AM81:
 *     "        The EXECUTE INTERRUPT MASK ASSIGNMENT (EIMA) rotary switches
 *      determine where interrupts sent to memory are directed.  The four EIMA 
 *      rotary switches, one for each program interrupt register, are used to 
 *      assign mask registers to system ports. The normal settings assign one 
 *      mask register to each CPU configured.
 *
 *      Each switch assigns mask registers as follows:
 *
 *          Position
 *            OFF     Unassigned
 *              0     Assigned to port 0
 *                ...
 *              7     Assigned to port 7
 *              M     Assigned to maintenance panel
 *
 *      Assignment of a mask register to a system port designates the 
 *      port as a control port, and that port receives interrupt present 
 *      signals. Up to four system ports can be designated as control 
 *      ports. The normal settings assign one mask register to each CPU
 *      configured."
 *
 *
 *   
 * Config panel -- Level 68 System Controller UNIT (4MW SCU)
 * -- from AM81
 * Store A, A1, B, B1 (online/offline)
 * LWR Store Size
 * PORT ENABLE
 * Eight on/off switches
 * Should be on for each port connected to a configured CPU
 * mask/port assignment
 * Two rotary switches (A & B); set to (off, 0..7)
 * See EXEC INTERRUPT on the 6000 SCU
 * When booting, one should be set to the port connected to
 * the bootload CPU.   The other should be off.
 * 
 * If memory port B of CPU C goes to SCU D, then memory port B of all
 * other CPUs *and* IOMs must go to SCU D. -- AN70, 8-4.
 * 
 * The base address of the SCU is the actual memory size * the port
 * assignment. -- AN70, 8-6.
 *
 *  43A239854 6000B Eng. Prod. Spec, 3.2.7 Interrupt Multiplex Word:
 *    "The IOM has the ability to set any of the 32 program interrupt
 *     cells located in the system controller containing the base address
 *     of the IOM. It should be noted that for any given IOM identity
 *     switch setting, the IOM can set only 8 of these program interrupt
 *     cells."
 *
 */


/*
 * === Initialization and Booting -- Part 1 -- Operator's view
 * 
 * Booting Instructions (GB61)
 * First boot the BCE OS (Bootload command Environment).  See below.
 * A config deck is used
 * Bootload SCU is the one with a base addr of zero.
 * BCE is on a BCE/Multics System tape
 * Booted from tape into the system via bootload console
 
 */

/*
 * 58009906 (DPS8)
 * When CPU needs to address the SCU (for a write/read data cycle,
 * for example), the ETMCM board int the CU of the CPU issues a $INT
 * to the SCU.  This signal is sent ... to the SCAMX active port
 * control board in the SCU
 */

// How?  If one of the 32 interrupt cells is set in one of the SCs,
// our processor will have the interrupt present (XIP) line active.
// Perhaps faults are flagged in the same way via the SXC system
// controller command.

// TEMPORARY
// Each SCU owns a certain range of absolute memory.
// CPUs use the cioc instr to talk to IOMs and other CPUs via a SCU.
// IOMs use interrupts to ask a SCU to signal a CPU.
// read system controller reg and set system controller reg (rscr & sscr)
// Bootload SCU is the one with a base addr of zero.
// 58009906
// When CPU needs to address the SCU (for a write/read data cycle,
// for example), the ETMCM board int the CU of the CPU issues a $INT
// to the SCU.  This signal is sent ... to the SCAMX active port
// control board in the
// -----------------------
// How?  If one of the 32 interrupt cells is set in one of the SCs,
// our processor will have the interrupt present (XIP) line active.
// Perhaps faults are flagged in the same way via the SXC system
// controller command.

/*
 * *** More (new) notes ***
 * 
 * instr rmcm -- read mem controller mask register
 * ... for the selected controller, if the processor has a mask register
 * assigned ..
 * instr smcm -- set  mem controller mask register
 * ... for the selected controller, if the processor has a mask register
 * assigned, set it to C(AQ)
 * instr smic
 * turn on interrupt cells (any of 0..31)
 * instr cioc -- connect i/o channel, pg 173
 * SC addressed by Y sends a connect signal to the port specified
 * by C(Y)33,35
 * instr rscr & sscr -- Read/Store System Controller Register, pg 170
 * 
 * 32 interrupt cells ... XIP
 * mask info
 * 8 mask registers
 * 58009906
 * =============
 * 
 * AM81
 * Every active device (CPU, IOM) must be able to access all SCUs
 * Every SCU must have the same active device on the same SCU, so
 * all SCUs must have the same PORT ENABLE settings
 * Every active device must have the same SCU on the same port,
 * so all active devices will have the same config panel settings.
 * Ports must correspond -- port A on every CPU and IOM must either
 * be connected tothe same SCU or not connected to any SCU.
 * IOMs should be on lower-numbered SCU ports than CPUs.
 * Multics can have 16MW words of memory.
 * CPUs have 8 ports, a..h.
 * SCUs have 8 ports, 0..7.
 * 
 * 
 * Level 68 6000 SCU Configuration Panel
 *   system control and monitor (cont&mon/mon/ofF)
 *   system boot control (on/off)
 *   alarm (disable/normal)
 *   maintainance panel mode (test/normal)
 *   store a
 *      mode (offline/maint/online)
 *      size (32k, 64k, 128k, 256k)
 *   store b
 *      mode (offline/maint/online)
 *      size (32k, 64k, 128k, 256k)
 *   execute interrupt mask assigment
 *      (A through D; off/0/1/2/3/4/5/6/7/m)
 *   [CAC] I interperet this as CPU [A..D] is connected to my port [0..7]
 *   address control
 *      lower store (a/b)
 *      offset (off, 16k, 32k, 64k)
 *      interlace (on/off)
 *   cycle port priority (on/off)
 *   port control (8 toogles) (enabled/prog cont/disable)
 *
 * The EXECUTE INTERRUPT MASK ASSIGNMENT (EIMA) rotary switches
 * determine where interrupts sent to memory are directed. The four EIMA 
 * rotary switches, one for each program interrupt register, are used to 
 * assign mask registers to system ports. The normal settings assign one 
 * mask register to each CPU configured.
 * 
 *  Assignment of a mask register to a system port designates the port as a 
 *  control port, and that port receives interrupt present signals. Up to four 
 *  system ports can be designated as control ports. The normal settings 
 *  assign one mask register to each cpu configured.
 *
 *
 *
 * Configuration rules for Multics:
 *
 *   1. Each CPU in the system must be connected to each SCU in the system
 *
 *   2. Each IOM in the system must be connected to each SCU in the system
 *
 *   3. Each SCU in the system must be connected to every CPU and IOM in the 
 *      system.
 *
 *   4. Corresponding ports on all CPUs and IOMs must be connected to the same
 *      SCU. For example, port A on every CPU and IOM must be connected to the
 *      same SCU or not connected to any SCU.
 *
 *   5. Corresponding ports on all SCUs must be connected to the same active 
 *      device (CPU or IOM). For example, if port 0 on any SCU is connected to 
 *      IOM A, then port 0 on all SCUs must be connected to IOM A.
 *
 *   6. IOMs should be connected to lower-number SCU ports the CPUs.
 *
 *   These rules are illustrated in Figure 3-5, where the port numbers for a 
 *   small Multics system of 2 CPUS, 3 SCUs and 2 IOMs have been indicated
 *
 *       
 *
 *
 *                    -----------------                      -----------------
 *                    |               |                      |               |
 *                    |     CPU A     |                      |     CPU B     |
 *                    |               |                      |               |
 *                    -----------------                      -----------------
 *                    | A | B | C | D |                      | A | B | C | D |
 *                    -----------------                      -----------------
 *                      |   |   |                              |   |   |
 *                      |   |   |                              |   |   -----------------
 *                      |   |   |                              |   |                   |
 *                      |   |   -------------------------------)---)----------------   |
 *                      |   |                                  |   |               |   |
 *   --------------------   -----------------                  |   |               |   |
 *   |                                      |                  |   |               |   |
 *   |   -----------------------------------)-------------------   |               |   |
 *   |   |                                  |                      |               |   |
 *   |   |                                  |   --------------------               |   |
 *   |   |                                  |   |                                  |   |
 * -----------------                      -----------------                      -----------------
 * | 7 | 6 | 5 | 4 |                      | 7 | 6 | 5 | 4 |                      | 7 | 6 | 5 | 4 |
 * -----------------                      -----------------                      -----------------
 * |               |                      |               |                      |               |
 * |     SCU C     |                      |     SCU B     |                      |     SCU A     |
 * |               |                      |               |                      |               |
 * -----------------                      -----------------                      -----------------
 * | 3 | 2 | 1 | 0 |                      | 3 | 2 | 1 | 0 |                      | 3 | 2 | 1 | 0 |
 * -----------------                      -----------------                      -----------------
 *           |   |                                  |   |                                  |   |
 *           |   |                                  |   -----------                        |   |
 *           |   |                                  |             |                        |   |
 *           |   -----------------------------------)---------    |                        |   |
 *           |                                      |        |    |                        |   |
 *           ----------    --------------------------        |    |                        |   |
 *                    |    |                                 |    |                        |   |
 *                    |    |   ------------------------------)----)-------------------------   |
 *                    |    |   |                             |    |                            |
 *                    |    |   |                             |    |  ---------------------------
 *                    |    |   |                             |    |  |
 *                   -----------------                      -----------------
 *                   | A | B | C | D |                      | A | B | C | D |
 *                   -----------------                      -----------------
 *                   |               |                      |               |
 *                   |     IOM A     |                      |     IOM B     |
 *                   |               |                      |               |
 *                   -----------------                      -----------------
 *
 * 
 *
 *"During bootload, Multics requires a contiguous section of memory beginning at
 * absolute address 0 and sufficiently large to contain all routines and data
 * structures used during the first phase of Multics initialization (i.e. 
 * collection 1).
 * The size of the section required varies among Multics release, and it also
 * depends on the size of the SST segment, which is dependent on the parameters
 * specified by the site on the SST config card. ... However
 * 512 KW is adequate for all circumstances. There can be no "holes" in memory
 * within this region. Beyond this region, "holes" can exist in memory."
 *
 *
 */

/*
 * From AN70-1 May84, pg 86 (8-6)
 *
 * RSCR SC_CFG bits 9-11 lower store size
 *
 * A DPS-8 SCU may have up to four store units attached to it. If this
 * is the case, two store units form a pair of units. The size of a 
 * pair of units (or a single unit) is 32K * 2 ** (lower store size)
 * above.
 */

/*
 * From AN70-1 May84, pg 86 (8-6)
 *
 * SCU ADDRESSING
 *
 *       There are three ways in which an SCU is addressed.  In the 
 * normal mode of operation (memory reading and writing), an active
 * unit (IOM or CPU) translates an absolute address into a memory
 * port (on it) and a relative memory address within the memory
 * described by the memory port. The active module sends the
 * address to the SCU on the proper memory port. If the active
 * module is enabled by the port enable mask in the referenced SCU,
 * the SCU will take the address given to it and provide the
 * necessary memory access.
 *
 *      The other two ways pertain to reading/setting control
 * registers in the SCU itself. For each of these, it is still
 * necessary to specify somehow the memory port on the CPU whose SCU
 * registers are desired. For the RMCM, SMCM and SMIC instructions,
 * this consists of providing a virtual address to the processor for
 * which bits 1 and 2 are the memory port desired.
 *
 *      The rscr and sscr instructions, though key off the final
 * absolute address to determine the SCI (or SCU store unit)
 * desired. Thus, software needs a way to translate a memory port
 * number into an absolute address to reach the SCU. This is done
 * with the paged segment scas, generated by int_scas (and
 * init_scu). scas has a page corresponding to each SCU and to each
 * store unit in each SCU. pmut$rscr and pmut$sscr use the memory
 * port number desired to generate a virtual address into scas whose
 * absolute address (courtesy of the ptws for sca) just happen to
 * describe memory within that SCU.
 *
 *       The cioc instruction (discussed below) also depends on the
 * final absolute addres of the target operand to identify the SCU
 * to perform the operation. In the case of the cioc instruction,
 * though, the has no particular impact in Multics software. All
 * target operands for the cioc instruction when referencing IOMs
 * are in the low order SCU. When referencing CPUS, the SCU
 * performing the connecting has no real bearing.
 *
 * Inter-module communication
 *
 *       As mentioned earlier, communication between active modules
 * (CPUs and IOMs can only be performed through SCUs.
 *
 *       CPUs communicate to IOMs and other CPUs via the cioc
 * (connect i/o channel) instruction. The operand of the instruction
 * is a word in memory. The SCU containing this operand is the SCU
 * that performs the connect function. The word fetched from memory
 * contains in its low order bits the identity of a port on the SCU
 * to which this connection is to be sent. This only succeeds if the
 * target port is enabled (port enable mask) on the SCU. When the
 * target of the connection is an IOM; this generates a connect strobe
 * to the IOM. The IOM examines its mailbox in memory to determine
 * its course of action. When the target of the connect is another
 * CPU, this generates a connect fault in the target processor. The
 * target processor determines what course to follow on the basis
 * of information in memory analyzed by software. When a connect is
 * sent to a process (including the processor issuing the connect),
 * the connect is deferred until the processor stops
 * executing inhibited code (instructions with the inhibit bit set).
 *
 *       Signals sent from an IOM to a CPU are much more involved.
 * The basic flow is as follows. The IOM determines an interrupt
 * number. (The interrupt number is a five bit value, from 0 to 31.
 * The high order bits are the interrupt level.
 *
 * 0 - system fault
 * 1 - terminate
 * 2 - marker
 * 3 - special
 *
 * The low order three bits determines the IOM and IOM channel 
 * group.
 *
 * 0 - IOM 0 channels 32-63
 * 1 - IOM 1 channels 32-63
 * 2 - IOM 2 channels 32-63
 * 3 - IOM 3 channels 32-63
 * 4 - IOM 0 channels 0-31
 * 5 - IOM 1 channels 0-31
 * 6 - IOM 2 channels 0-31
 * 7 - IOM 3 channels 0-31
 *
 * It also takes the channel number in the group (0-31 meaning
 * either channels 0-31 to 32-63) and sets the <channel number>th
 * bit in the <interrupt number>th memory location in the interrupt
 * mask word (IMW) array in memory. It then generates a word with
 * the <interrupt number>th bit set and sends this to the bootload
 * SCU with the SC (set execute cells) SCU command. This sets the
 * execute interrupt cell register in the SCU and sends an XIP
 * (execute interrupt present) signal to various processors
 * connected to the SCU. (The details of this are covered in the
 * next section.) One of the processors (the first to get to it)
 * sends an XEC (execute interrupt cells) SCU command to the SCU who
 * generated the XIP signal. The SCU provides the interrupt number
 * to the processor, who uses it to determine the address of a fault
 * pair in memory for the "fault" caused by this interrupt. The 
 * processing of the XEC command acts upon the highest priority
 * (lowest number) bit in the execute interrupt cell register, and
 * also resets this bit in the register.
 *
 * Interrupts Masks and Assignment
 *
 *       The mechanism for determining which processors are candidates
 * for receiving an interrupt from an IOM is an involved 
 * topic. First of all, a processor will not be interrupted as long
 * as it is executing inhibited instructions (instructions with the 
 * inhibit bit set). Beyond this, though, lies the question of
 * interrupt masks and mask assignment.
 *
 *       Internal to the SCU are two sets of registers (A and B),
 * each set consisting of the execute interrupt mask register and
 * the interrupt mask assignment register. Each execute interrupt
 * mask register is 32 bits long, with each bit enabling the 
 * corresponding bit in the execute interrupt cell register. Each
 * interrupt mask assignment register has two parts, an assigned bit
 * and a set of ports to which it is assigned (8 bits). When a bit
 * is set in the execute  interrupt sells register, the SCU ands this
 * bit with the corresponding bit in each of the execute interrupt
 * mask registers. If the corresponding bit of execute interrupt
 * mask register A, for example, is on, the SCU then looks at the A
 * interrupt mask assignment register. If this register is not
 * assigned (enable), no further action takes place in regards to 
 * the A registers. (The B registers are still considered) (in
 * parallel, by the way).) If the register is assigned (enabled)
 * then interrupts will be send to all ports (processors) whose
 * corresponding bit is set in the interrupt mask assignment
 * register. This, only certain interrupts are allowed to be
 * signalled at any given time (base on the contents of the execute
 * interrupt mask registers) and only certain processors will
 * receive these interrupts (as controlled by the interrupt mask
 * assignment registers).
 *
 *       In Multics, only one processor is listed in each of the two
 * interrupt mask assignment registers, and no processor appears in
 * both. Thus there is a one for one correspondence between 
 * interrupt masks that are assigned (interrupt mask registers whose
 * assigned (enabled) bit is on) and processors who have an
 * interrupt mask (SCU port number appears in an interrupt mask
 * register). So, at any one time only two processors 
 * are eligible to receive interrupts. Other processors need not
 * worry about masking interrupts.
 *
 *       The contents of the interrupt mask registers may be
 * obtained with the SCU configuration information with the rscr
 * instruction and set with the sscr instruction.
 *
 *  bits   meaning
 *
 * 00-07   ports assigned to mask A (interrupt mask assignment A)
 * 08-08   mask A is unassigned (disabled)
 * 36-43   ports assigned to mask B (interrupt mask assignment B)
 * 44-44   mask B is unassigned (disabled)
 *
 *       The contents of a execute interrupt mask register are
 * obtained with the rmcm or the rscr instruction and set with the
 * smcm or the sscr instruction. The rmcm and smcm instruction only
 * work if the processor making the request has a mask register
 * assigned to it. If not, rmcm returns zero (no interrupt are
 * enabled to it) and a smcm is ignored (actually the port mask
 * setting is still done). The rscr and sscr instructions allow the
 * examing/setting of the execute interrupt mask register for any
 * port on a SCU; these have the same effect as smcm and rmcm if the
 * SCU port being referenced does not have a mask assigned to it.
 * The format of the data returned by these instructions is as 
 * follows.
 *
 *  bits   meaning
 * 00-15   execute interrupt mask register 00-15
 * 32-35   SCU port mask 0-3
 * 36-51   execute interrupt mask register 16-31
 * 68-71   SCU port mask 4-7
 *
 */

// SCU numbering:
//
// AM81-04, pg 49: "... the ports are listed in order of increasing base 
//    address, which corresponds to the order of mem config cards."
// pg 97: "mem port size state ... port as a value (a through h) that
//        corresponds to the number of the active module port to which the
//        system controller is connected.
//
// From this, I conclude;
//   The SCU connected to port A (0) is SCUA, 1 B, 2 C, etc.
//   SCUA starts at address 0, and the SCUs are sorted by increasing addresses.
//

// ============================================================================

#include <sys/time.h>
#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#ifdef THREADZ
#include "threadz.h"
#endif

#define DBG_CTR 1

static t_stat scu_show_nunits (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat scu_set_nunits (UNIT * uptr, int32 value, const char * cptr, 
                              void * desc);
static t_stat scu_show_state (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat scu_show_config(FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat scu_set_config (UNIT * uptr, int32 value, const char * cptr, 
                              void * desc);
static void deliverInterrupts (uint scuUnitIdx);

scu_t scu [N_SCU_UNITS_MAX];

#define N_SCU_UNITS 1 // Default

static UNIT scu_unit [N_SCU_UNITS_MAX] =
  {
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL }
  };

#define UNIT_NUM(uptr) ((uptr) - scu_unit)

static MTAB scu_mod [] =
  {
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      (char *) "CONFIG",     /* print string */
      (char *) "CONFIG",         /* match string */
      scu_set_config,         /* validation routine */
      scu_show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL /* help */
    },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      (char *) "NUNITS",     /* print string */
      (char *) "NUNITS",         /* match string */
      scu_set_nunits, /* validation routine */
      scu_show_nunits, /* display routine */
      (char *) "Number of SCU units in the system", /* value descriptor */
      NULL /* help */
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      (char *) "STATE",     /* print string */
      (char *) "STATE",         /* match string */
      NULL, /* validation routine */
      scu_show_state, /* display routine */
      (char *) "SCU unit internal state", /* value descriptor */
      NULL /* help */
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      (char *) "RESET",     /* print string */
      (char *) "RESET",         /* match string */
      scu_reset_unit, /* validation routine */
      NULL, /* display routine */
      (char *) "reset SCU unit", /* value descriptor */
      NULL /* help */
    },
    {
      0, 0, NULL, NULL, NULL, NULL, NULL, NULL
    }
  };


//static t_stat scu_reset (DEVICE *dptr);

static DEBTAB scu_dt [] =
  {
    { (char *) "TRACE",  DBG_TRACE, NULL },
    { (char *) "NOTIFY", DBG_NOTIFY, NULL },
    { (char *) "INFO",   DBG_INFO, NULL },
    { (char *) "ERR",    DBG_ERR, NULL },
    { (char *) "WARN",   DBG_WARN, NULL },
    { (char *) "DEBUG",  DBG_DEBUG, NULL },
    { (char *) "INTR",   DBG_INTR, NULL        },
    { (char *) "ALL",    DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

DEVICE scu_dev = {
    (char *) "SCU",  /* name */
    scu_unit,        /* units */
    NULL,            /* registers */
    scu_mod,         /* modifiers */
    N_SCU_UNITS,     /* #units */
    10,              /* address radix */
    8,               /* address width */
    1,               /* address increment */
    8,               /* data radix */
    8,               /* data width */
    NULL,            /* examine routine */
    NULL,            /* deposit routine */
    & scu_reset,     /* reset routine */
    NULL,            /* boot routine */
    NULL,            /* attach routine */
    NULL,            /* detach routine */
    NULL,            /* context */
    DEV_DEBUG,       /* flags */
    0,               /* debug control flags */
    scu_dt,          /* debug flag names */
    NULL,            /* memory size change */
    NULL,            /* logical name */
    NULL,            /* help */
    NULL,            /* attach_help */
    NULL,            /* help_ctx */
    NULL,            /* description */
    NULL
};

enum { MODE_MANUAL = 0, MODE_PROGRAM = 1 };

// Hardware configuration switches

// sscr and other instructions override these settings

static struct config_switches
  {
    uint mode; // program or manual
    uint port_enable [N_SCU_PORTS];  // enable/disable
    uint mask_enable [N_ASSIGNMENTS]; // enable/disable
    uint mask_assignment [N_ASSIGNMENTS]; // assigned port number
    uint lower_store_size; // In K words, power of 2; 32 - 4096
    uint cyclic; // 7 bits
    uint nea; // 8 bits
    uint onl; // 4 bits
    uint interlace; // 1 bit
    uint lwr; // 1 bit
  } config_switches [N_SCU_UNITS_MAX];

static void dumpIR (char * ctx, uint scuUnitIdx)
  {
    scu_t * up = scu + scuUnitIdx;

    sim_debug (DBG_DEBUG, & scu_dev, 
               "%s A: mask %011o enable %o assignment %o\n", 
               ctx, up -> exec_intr_mask [0], up -> mask_enable [0], 
               up -> mask_assignment [0]);
    sim_debug (DBG_DEBUG, & scu_dev, 
               "%s B: mask %011o enable %o assignment %o\n", 
               ctx, up -> exec_intr_mask [1], up -> mask_enable [1], 
               up -> mask_assignment [1]);

    sim_debug (DBG_DEBUG, & scu_dev, "%s enabled ports:", ctx);
    for (uint p = 0; p < N_SCU_PORTS; p ++)
      if (up -> port_enable [p])
        {
          sim_debug (DBG_DEBUG, & scu_dev, " %d", p);
        }
    sim_debug (DBG_DEBUG, & scu_dev, "\n");

    sim_debug (DBG_DEBUG, & scu_dev, "%s set cells:", ctx);
    for (uint i = 0; i < N_CELL_INTERRUPTS; i ++)
      if (up -> cells [i])
        {
          sim_debug (DBG_DEBUG, & scu_dev, " %d", i);
        }
    sim_debug (DBG_DEBUG, & scu_dev, "\n");
#if 0
 {
    scu_t * up = scu + scuUnitIdx;

    sim_printf (
               "%s A: mask %011o enable %o assignment %o\n", 
               ctx, up -> exec_intr_mask [0], up -> mask_enable [0], 
               up -> mask_assignment [0]);
    sim_printf (
               "%s B: mask %011o enable %o assignment %o\n", 
               ctx, up -> exec_intr_mask [1], up -> mask_enable [1], 
               up -> mask_assignment [1]);

    sim_printf ("%s enabled ports:", ctx);
    for (uint p = 0; p < N_SCU_PORTS; p ++)
      if (up -> port_enable [p])
        {
          sim_printf (" %d", p);
        }
    sim_printf ("\n");

    sim_printf ("%s set cells:", ctx);
    for (uint i = 0; i < N_CELL_INTERRUPTS; i ++)
      if (up -> cells [i])
        {
          sim_printf (" %d", i);
        }
    sim_printf ("\n");
  }
#endif
   }

void scuUnitReset (int scuUnitIdx)
  {
    scu_t * up = scu + scuUnitIdx;
    struct config_switches * sw = config_switches + scuUnitIdx;

    for (int i = 0; i < N_SCU_PORTS; i ++)
      {
        up -> port_enable [i] = sw -> port_enable [i];
      }

    for (int i = 0; i < N_ASSIGNMENTS; i ++)
      {
        up -> mask_enable [i] = sw -> mask_enable [i];
        up -> mask_assignment [i] = sw -> mask_assignment [i];
      }
    up -> lower_store_size = sw -> lower_store_size;
    up -> cyclic = sw -> cyclic;
    up -> nea = sw -> nea;
    up -> onl = sw -> onl;
    up -> interlace = sw -> interlace;
    up -> lwr = sw -> lwr;

   
#ifdef SCUMEM
    memset (up->M, 0, sizeof (up->M));
#endif

// This is to allow the CPU reset to update the memory map. IAC clears the 
// attached SCUs; they clear the attached IOMs.

    for (uint portNum = 0; portNum < N_SCU_PORTS; portNum ++)
      {
        struct ports * portp = & scu [scuUnitIdx] . ports [portNum];
        if (portp->type != ADEV_IOM)
          continue;
        //if (! scu [scuUnitIdx] . port_enable [scu_port_num])
          //continue;
        iomUnitResetIdx ((uint) portp->devIdx);
      }

// CAC - These settings were reversed engineer from the code instead
// of from the documentation. In case of issues, try fixing these, not the
// code.

    for (int i = 0; i < N_ASSIGNMENTS; i ++)
      {
// XXX Hack for t4d
        up -> exec_intr_mask [i] = 037777777777;
      }
  }

t_stat scu_reset (UNUSED DEVICE * dptr)
  {
    // On reset, instantiate the config switch settings

    for (int scuUnitIdx = 0; scuUnitIdx < N_SCU_UNITS_MAX; scuUnitIdx ++)
      scuUnitReset (scuUnitIdx);
    return SCPE_OK;
  }


// ============================================================================

#ifdef THREADZ
static pthread_mutex_t clock_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

// The SCU clock is 52 bits long; fits in t_uint64
static uint64 getSCUclock (uint scuUnitIdx)
  {
#ifdef THREADZ
    pthread_mutex_lock (& clock_lock);
#endif

// The emulator supports two clock models: steady and real
// In steady mode the time of day is coupled to the instruction clock,
// allowing reproducible behavior. In real, the clock is
// coupled to the actual time-of-day.

    if (scu [0] . steady_clock)
      {
        // The is a bit of code that is waiting for 5000 ms; this
        // fools into going faster
#ifdef NEED_128
        uint128 big = construct_128 (0, cpu.instrCnt);
        // Sync up the clock and the TR; see wiki page "CAC 08-Oct-2014"
        //big *= 4u;
        big = lshift_128 (big, 2);
        if (scu [0] . bullet_time)
          big = multiply_128 (big, construct_128 (0, 10000u));

        //big += scu [0] . elapsed_days * 1000000llu * 60llu * 60llu * 24llu; 
        uint128 days = construct_128 (0, scu[0].elapsed_days);
        days = multiply_128 (days, construct_128 (0, 1000000));
        days = multiply_128 (days, construct_128 (0, 60 * 60 * 24));
        big = add_128 (big, days);
#else
        __uint128_t big = cpu.instrCnt;
        // Sync up the clock and the TR; see wiki page "CAC 08-Oct-2014"
        big *= 4u;
        //big /= 100u;
        if (scu [0] . bullet_time)
          big *= 10000;

        big += scu [0] . elapsed_days * 1000000llu * 60llu * 60llu * 24llu; 
#endif

        // Boot time

// load_fnp is complaining that FNP core image is more than 5 years old; try 
// moving the 'boot time' back to MR12.3 release date. (12/89 according to 
// http://www.multicians.org/chrono.html

        // date -d "1990-01-01 00:00:00 -9" +%s
        // 631184400
        uint64 UnixSecs = 631184400;

#ifdef NEED_128
        uint64 UnixuSecs = UnixSecs * 1000000llu + big.l;
#else
        uint64 UnixuSecs = UnixSecs * 1000000llu + (uint64) big;
#endif
        // now determine uSecs since Jan 1, 1901 ...
        uint64 MulticsuSecs = 2177452800000000llu + UnixuSecs;

        // The casting to uint show be okay; both are 64 bit, so if if userCorrection 
        // is <0, it will come out in the wash ok.
        MulticsuSecs += (uint64) scu [scuUnitIdx] . userCorrection;

        // The get calendar clock function is guaranteed to return
        // different values on successive calls. 

        if (scu [scuUnitIdx] . lastTime >= MulticsuSecs)
          {
            sim_debug (DBG_TRACE, & scu_dev, "finagle clock\n");
            MulticsuSecs = scu [scuUnitIdx] . lastTime + 1;
          }
        scu [scuUnitIdx] . lastTime = MulticsuSecs;
        goto done;
      }

    // The calendar clock consists of a 52-bit register which counts
    // microseconds and is readable as a double-precision integer by a
    // single instruction from any central processor. This rate is in
    // the same order of magnitude as the instruction processing rate of
    // the GE-645, so that timing of 10-instruction subroutines is
    // meaningful. The register is wide enough that overflow requires
    // several tens of years; thus it serves as a calendar containing
    // the number of microseconds since 0000 GMT, January 1, 1901
    ///  Secs from Jan 1, 1901 to Jan 1, 1970 - 2 177 452 800
    //   Seconds
    /// uSecs from Jan 1, 1901 to Jan 1, 1970 - 2 177 452 800 000 000
    //  uSeconds
 
    struct timeval now;
    gettimeofday(& now, NULL);
                
    if (scu [0] . y2k) // subtract 20 years....
      {
        // Back the clock up to just after the MR12.3 release (12/89
        // according to http://www.multicians.org/chrono.html

        // ticks at MR12.3 release
        // date -d "1990-01-01 00:00:00 -9" +%s
        // 631184400

        // 12.3 was released 12/89
        // 12.4 was released 12/90
        // 12.5 was released 11/92

#if 0 
        // 12.3 release was 25 years ago
        // date --date='25 years ago' +%s; date +%s
        // 645852697
        // 1434771097
        now . tv_sec -= (1434771097 - 645852697);
#elif 0
        // 12.5 release was 22 years ago

        // date --date='22 years ago' +%s ; date +%s
        // 744420783
        // 1438644783
        now . tv_sec -= (1438644783 - 744420783);
#else
// Hmm. Manifest constants are a bad idea. Today (2017-04-01) it was
// discovered that the 12.3 5 year FNP window had expired. Compute a 
// new constant....

        // 12.5 release was 22^H^H 24 years ago

        // date --date='22 years ago' +%s ; date +%s
        // 744420783
        // 1438644783
        //now . tv_sec -= (1438644783 - 744420783);

        // $ date --date='24 years ago' +%s ; date +%s
        // 733691934
        // 1491074334
        now . tv_sec -= (1491074334 - 733691934);

#endif

      }
    uint64 UnixSecs = (uint64) now.tv_sec;
    uint64 UnixuSecs = UnixSecs * 1000000LL + (uint64) now.tv_usec;
   
    static uint64 lastUnixuSecs = 0;
    if (UnixuSecs < lastUnixuSecs)
      {
        sim_warn ("gettimeofday() went backwards %llu uS\n", 
                  lastUnixuSecs - UnixuSecs);
      }
    lastUnixuSecs = UnixuSecs;

    // now determine uSecs since Jan 1, 1901 ...
    uint64 MulticsuSecs = 2177452800000000LL + UnixuSecs;
 
    // Correction factor from the set time command

    // The casting to uint show be okay; both are 64 bit, so if if userCorrection 
    // is <0, it will come out in the wash ok.
    MulticsuSecs += (uint64) scu [scuUnitIdx] . userCorrection;

    if (scu [scuUnitIdx] . lastTime >= MulticsuSecs)
        MulticsuSecs = scu [scuUnitIdx] . lastTime + 1;
    scu [scuUnitIdx] . lastTime = MulticsuSecs;

done:
#ifdef THREADZ
    pthread_mutex_unlock (& clock_lock);
#endif

    return scu [scuUnitIdx] . lastTime;

  }


//static char pcellb [N_CELL_INTERRUPTS + 1];
static char * pcells (uint scuUnitIdx, char * buf)
  {
    for (uint i = 0; i < N_CELL_INTERRUPTS; i ++)
      {
        if (scu [scuUnitIdx] . cells [i])
          buf [i] = '1';
        else
          buf [i] = '0';
      }
    buf [N_CELL_INTERRUPTS] = '\0';
    return buf;
  }

t_stat scu_smic (uint scuUnitIdx, uint UNUSED cpuUnitIdx, 
                 uint UNUSED cpu_port_num, word36 rega)
  {
#ifdef THREADZ
    lock_scu ();
#endif
// smic can set cells but not reset them...
#if 1
    if (getbits36_1 (rega, 35))
      {
        for (uint i = 0; i < 16; i ++)
          {
            if (getbits36_1 (rega, i))
              scu [scuUnitIdx].cells [i + 16] = 1;
          }
        char pcellb [N_CELL_INTERRUPTS + 1];
        sim_debug (DBG_TRACE, & scu_dev,
                   "SMIC low: Unit %u Cells: %s\n", 
                   scuUnitIdx, pcells (scuUnitIdx, pcellb));
      }
    else
      {
        for (uint i = 0; i < 16; i ++)
          {
            if (getbits36_1 (rega, i))
              scu [scuUnitIdx].cells [i] = 1;
          }
        char pcellb [N_CELL_INTERRUPTS + 1];
        sim_debug (DBG_TRACE, & scu_dev,
                   "SMIC high: Unit %d Cells: %s\n",
                   scuUnitIdx, pcells (scuUnitIdx, pcellb));
      }
#else
    if (getbits36_1 (rega, 35))
      {
        for (uint i = 0; i < 16; i ++)
          {
            scu [scuUnitIdx] . cells [i + 16] = getbits36_1 (rega, i) ? 1 : 0;
          }
        char pcellb [N_CELL_INTERRUPTS + 1];
        sim_debug (DBG_TRACE, & scu_dev,
                   "SMIC low: Unit %u Cells: %s\n", 
                   scuUnitIdx, pcells (scuUnitIdx, pcellb));
      }
    else
      {
        for (uint i = 0; i < 16; i ++)
          {
            scu [scuUnitIdx] . cells [i] = 
              getbits36_1 (rega, i) ? 1 : 0;
          }
        char pcellb [N_CELL_INTERRUPTS + 1];
        sim_debug (DBG_TRACE, & scu_dev,
                   "SMIC high: Unit %d Cells: %s\n",
                   scuUnitIdx, pcells (scuUnitIdx, pcellb));
      }
#endif
    dumpIR ("smic", scuUnitIdx);
    deliverInterrupts (scuUnitIdx);
#ifdef THREADZ
    unlock_scu ();
#endif
    return SCPE_OK;
  }

              

// system controller and the function to be performed as follows:
//
//  Effective  Function
//  Address
//  y0000x     C(system controller mode register) → C(AQ)
//  y0001x     C(system controller configuration switches) → C(AQ)
//  y0002x     C(mask register assigned to port 0) → C(AQ)
//  y0012x     C(mask register assigned to port 1) → C(AQ)
//  y0022x     C(mask register assigned to port 2) → C(AQ)
//  y0032x     C(mask register assigned to port 3) → C(AQ)
//  y0042x     C(mask register assigned to port 4) → C(AQ)
//  y0052x     C(mask register assigned to port 5) → C(AQ)
//  y0062x     C(mask register assigned to port 6) → C(AQ)
//  y0072x     C(mask register assigned to port 7) → C(AQ)
//  y0003x     C(interrupt cells) → C(AQ)
//
//  y0004x
//    or       C(calendar clock) → C(AQ)
//  y0005x
//
//  y0006x
//    or C(store unit mode register) → C(AQ)
//  y0007x
//
// where: y = value of C(TPR.CA)0,2 (C(TPR.CA)1,2 for the DPS 8M 
// processor) used to select the system controller
// x = any octal digit
//

t_stat scu_sscr (uint scuUnitIdx, UNUSED uint cpuUnitIdx, 
                 UNUSED uint cpu_port_num, word18 addr, 
                 word36 rega, word36 regq)
  {
    sim_debug (DBG_DEBUG, & scu_dev, "sscr SCU unit %o\n", scuUnitIdx);

    // Only valid for a 4MW SCU

    if (scuUnitIdx >= scu_dev . numunits)
      {
// XXX should this be a store fault?
        sim_warn ("%s: scuUnitIdx out of range %d\n",
                   __func__, scuUnitIdx);
        return SCPE_OK;
      }

    // BCE uses clever addressing schemes to select SCUs; it appears we need
    // to be more selecting in picking out the function bits;
    //uint function = (addr >> 3) & 07777;
    uint function = (addr >> 3) & 07;

    // See scs.incl.pl1
    
    if (config_switches [scuUnitIdx] . mode != MODE_PROGRAM)
      {
        sim_warn ("%s: SCU mode is 'MANUAL', not 'PROGRAM' -- sscr "
                   "not allowed to set switches.\n", 
                   __func__);
// XXX [CAC] Setting an unassigned register generates a STORE FAULT;
// this probably should as well
        return SCPE_OK;
      }
    
// Not used by 4MW
 
    switch (function)
      {
        case 00000: // Set system controller mode register
          {
#ifdef THREADZ
            lock_scu ();
#endif
            scu [scuUnitIdx] . id = (word4) getbits36_4 (regq, 50 - 36);
            scu [scuUnitIdx] . modeReg = getbits36_18 (regq, 54 - 36);
#ifdef THREADZ
            unlock_scu ();
#endif
          }
          break;

        case 00001: // Set system controller configuration register 
                    // (4MW SCU only)
          {
            sim_debug (DBG_DEBUG, & scu_dev, "sscr 1 %d A: %012"PRIo64" Q: %012"PRIo64"\n", scuUnitIdx, rega, regq);
#ifdef THREADZ
            lock_scu ();
#endif
            scu_t * up = scu + scuUnitIdx;
            for (int maskab = 0; maskab < 2; maskab ++)
              {
                word9 mask = ((maskab ? regq : rega) >> 27) & 0377;
                if (mask & 01)
                  {
                    up -> mask_enable [maskab] = 0;
                    sim_debug (DBG_DEBUG, & scu_dev, "sscr %u mask disable  %d\n", scuUnitIdx, maskab);
                  }
                else
                  {
                    up -> mask_enable [maskab] = 1;
                    sim_debug (DBG_DEBUG, & scu_dev, "sscr %u mask enable  %d\n", scuUnitIdx, maskab);
                    for (int pn = 0; pn < N_SCU_PORTS; pn ++)
                      {
                        if ((2 << (N_SCU_PORTS - 1 - pn)) & mask)
                          {
                            up -> mask_assignment [maskab] = (uint) pn;
                            break;
                          }
                      }
         
                  }
                sim_debug (DBG_INTR, & scu_dev,
                           "SCU%u SSCR1 mask %c enable set to %u assigned to port %u\n",
                           scuUnitIdx, 'a' + maskab, up->mask_enable[maskab],
                           up->mask_assignment[maskab]);
              }
            // AN87-00A, pg 2-5, 2-6 specifiy which fields are and are not settable.
 
            //if (up -> lower_store_size != ((rega >> 24) & 07))
              //sim_printf ("??? The CPU tried to change the SCU store size\n");
            up -> lower_store_size = (rega >> 24) & 07;
            up -> cyclic = (regq >> 8) & 0177;
            up -> nea = (rega >> 6) & 0377;
            up -> onl = (rega >> 20) & 017;
            up -> interlace = (rega >> 5) & 1;
            up -> lwr = (rega >> 4) & 1;
            up -> port_enable [0] = (rega >> 3) & 01;
            up -> port_enable [1] = (rega >> 2) & 01;
            up -> port_enable [2] = (rega >> 1) & 01;
            up -> port_enable [3] = (rega >> 0) & 01;
            up -> port_enable [4] = (regq >> 3) & 01;
            up -> port_enable [5] = (regq >> 2) & 01;
            up -> port_enable [6] = (regq >> 1) & 01;
            up -> port_enable [7] = (regq >> 0) & 01;

#ifdef THREADZ
            unlock_scu ();
#endif
            // XXX A, A1, B, B1, INT, LWR not implemented. (AG87-00A pgs 2-5. 2-6)
            break;
          }

        case 00002: // Set mask register port 0
        //case 00012: // Set mask register port 1
        //case 00022: // Set mask register port 2
        //case 00032: // Set mask register port 3
        //case 00042: // Set mask register port 4
        //case 00052: // Set mask register port 5
        //case 00062: // Set mask register port 6
        //case 00072: // Set mask register port 7
          {
#ifdef THREADZ
            lock_scu ();
#endif
            uint port_num = (addr >> 6) & 07;
            sim_debug (DBG_DEBUG, & scu_dev, "Set mask register port %d to "
                       "%012"PRIo64",%012"PRIo64"\n", 
                       port_num, rega, regq);

            // Find mask reg assigned to specified port
            int mask_num = -1;
            uint n_masks_found = 0;
            for (int p = 0; p < N_ASSIGNMENTS; p ++)
              {
                //if (scup -> interrupts [p] . mask_assign . unassigned)
                if (scu [scuUnitIdx] . mask_enable [p] == 0)
                  continue;
                //if (scup -> interrupts [p] . mask_assign . port == port_num)
                if (scu [scuUnitIdx ] . mask_assignment [p] == port_num)
                  {
                    if (n_masks_found == 0)
                      mask_num = p;
                    n_masks_found ++;
                  }
              }
    
            if (! n_masks_found)
              {
// According to bootload_tape_label.alm, this condition is OK
                sim_debug (DBG_WARN, & scu_dev, 
                           "%s: No masks assigned to cpu on port %d\n", 
                           __func__, port_num);
#ifdef THREADZ
                unlock_scu ();
#endif
                return SCPE_OK;
              }

            if (n_masks_found > 1)
              {
                // Not legal for Multics
                sim_debug (DBG_WARN, & scu_dev, 
                           "%s: Multiple masks assigned to cpu on port %d\n", 
                           __func__, port_num);
              }
    
            // See AN87
            //scup -> interrupts[mask_num].exec_intr_mask = 0;
            scu [scuUnitIdx] . exec_intr_mask [mask_num] = 0;
            scu [scuUnitIdx] . exec_intr_mask [mask_num] |= 
              ((word32) getbits36_16(rega, 0) << 16);
            scu [scuUnitIdx] . exec_intr_mask [mask_num] |= 
              getbits36_16(regq, 0);
#if 0
            sim_debug (DBG_DEBUG, & scu_dev,
                       "%s: PIMA %c: EI mask set to %s\n", 
                       __func__, mask_num + 'A', 
                       bin2text(scu [scuUnitIdx] . exec_intr_mask [mask_num], 
                       N_CELL_INTERRUPTS));
            //sim_printf ("sscr  exec_intr_mask %012o\n", 
                          //scu [scuUnitIdx] . exec_intr_mask [mask_num]);
#endif
            sim_debug (DBG_TRACE, & scu_dev, 
                       "SSCR Set mask unit %u port %u mask_num %u "
                       "mask 0x%08x\n", 
                       scuUnitIdx, port_num, mask_num, 
                       scu [scuUnitIdx] . exec_intr_mask [mask_num]);
            dumpIR ("sscr set mask", scuUnitIdx);
            scu [scuUnitIdx] . mask_enable [mask_num] = 1;
            sim_debug (DBG_INTR, & scu_dev,
                       "SCU%u SSCR2 exec_intr mask %c set to 0x%08x and enabled.\n",
                       scuUnitIdx, 'a' + mask_num, 
                       scu[scuUnitIdx].exec_intr_mask[mask_num]);

            deliverInterrupts (scuUnitIdx);
#ifdef THREADZ
            unlock_scu ();
#endif
          }
          break;

        case 00003: // Set interrupt cells
          {
#ifdef THREADZ
            lock_scu ();
#endif
            for (uint i = 0; i < 16; i ++)
              {
                scu [scuUnitIdx] . cells [i] = 
                  getbits36_1 (rega, i) ? 1 : 0;
                scu [scuUnitIdx] . cells [i + 16] = 
                  getbits36_1 (regq, i) ? 1 : 0;
              }
            char pcellb [N_CELL_INTERRUPTS + 1];
            sim_debug (DBG_TRACE, & scu_dev, 
                       "SSCR Set int. cells: Unit %u Cells: %s\n", 
                       scuUnitIdx, pcells (scuUnitIdx, pcellb));
            sim_debug (DBG_INTR, & scu_dev, 
                       "SCU%u SSCR3  Set int. cells %s\n", 
                       scuUnitIdx, pcells (scuUnitIdx, pcellb));
            dumpIR ("sscr set interrupt cells", scuUnitIdx);
            deliverInterrupts (scuUnitIdx);
#ifdef THREADZ
            unlock_scu ();
#endif
          }
          break;

        case 00004: // Set calendar clock (4MW SCU only)
        case 00005: 
          {
            // AQ: 20-35 clock bits 0-15, 36-71 clock bits 16-51
            word16 b0_15 = (word16) getbits36_16 (cpu . rA, 20);
            word36 b16_51 = cpu . rQ;
            uint64 newClk = (((uint64) b0_15) << 36) | b16_51;
#ifdef THREADZ
            lock_scu ();
#endif
            scu [scuUnitIdx] . userCorrection = (int64) (newClk - getSCUclock (scuUnitIdx));
#ifdef THREADZ
            unlock_scu ();
#endif
            //sim_printf ("sscr %o\n", function);
          }
          break;

        case 00006: // Set unit mode register
        case 00007: 
          // ticket 34
          // XXX See notes in AL39 sscr re: store unit selection
          //sim_printf ("sscr %o\n", function);
          sim_warn ("sscr set unit mode register\n");
          //return STOP_UNIMP;
          return SCPE_OK;

        default:
          sim_warn ("sscr unhandled code\n");
          //return STOP_UNIMP;
          return SCPE_OK;
          //sim_printf ("sscr %o\n", function);
      }
    return SCPE_OK;
  }

t_stat scu_rscr (uint scuUnitIdx, uint cpuUnitIdx, word18 addr, 
                 word36 * rega, word36 * regq)
  {
    // Only valid for a 4MW SCU

    if (scuUnitIdx >= scu_dev . numunits)
      {
        sim_warn ("%s: scuUnitIdx out of range %d\n", 
                   __func__, scuUnitIdx);
        return SCPE_OK;
      }

    // BCE uses clever addressing schemes to select SCUs; it appears we need
    // to be more selecting in picking out the function bits;
    //uint function = (addr >> 3) & 07777;
    uint function = (addr >> 3) & 07;

    //sim_printf ("rscr %o\n", function);

    // See scs.incl.pl1
    
    switch (function)
      {
        case 00000: // Read system controller mode register
          {
            // AN-87
            // 0..0 -> A
            // 0..0 -> Q 36-49 (0-13)
            // ID -> Q 50-53 (14-17)
            // MODE REG -> Q 54-71 (18-35)
            //
            //  ID: 0000  8034, 8035
            //      0001  Level 68 SC
            //      0010  Level 66 SCU
            // CAC: According to scr.incl.pl1. 0010 is a 4MW SCU
            // MODE REG: these fields are only used by T&D
            * rega = 0;
            //* regq = 0000002000000; // ID = 0010
            * regq = 0;
#ifdef THREADZ
            lock_scu ();
#endif
            putbits36_4 (regq, 50 - 36, scu [scuUnitIdx] . id);
            putbits36_18 (regq, 54 - 36, scu [scuUnitIdx] . modeReg);
#ifdef THREADZ
            unlock_scu ();
#endif
            break;
          }

        case 00001: // Read system controller configuration register
          {
            // AN-87, scr.incl.pl1
            //
            // SCU:
            // reg A:
            //   MASK A | SIZE | A | A1 | B | B1 | PORT | 0 | MOD | NEA |
            //   INT | LWR | PMR 0-3
            // reg Q:
            //   MASK B | not used | CYCLIC PRIOR | not used | PMR 4-7
            //
            //   MASK A/B (9 bits): EIMA switch setting for mask A/B. The
            //    assigned port corresponds to the but position within the
            //    field. A bit in position 9 indicates that the mask is
            //    not assigned.
            // From scr.incl.pl1:
            // 400 => assigned to port 0
            //  .
            //  .
            // 002 => assigned to port 7
            // 001 => mask off */

            //
            //  SIZE (3 bits): Size of lower store
            //    000 = 32K ... 111 = 4M
            //
            //  A A1 B B1 (1 bit): store unit A/A1/B/B1 online
            //
            //  PORT (4 bits): Port number of the SCU port through which
            //    the RSCR instruction was received
            //
            //struct config_switches * sw = config_switches + scuUnitIdx;
sim_printf ("rscr 1 %d\n", scuUnitIdx);
            sim_debug (DBG_DEBUG, & scu_dev, "rscr 1 %d\n", scuUnitIdx);
#ifdef THREADZ
            lock_scu ();
#endif
            scu_t * up = scu + scuUnitIdx;
            word9 maskab [2];
            for (int i = 0; i < 2; i ++)
              {
                if (up -> mask_enable [i])
                  {
                    maskab [i] = (2 << (N_SCU_PORTS - 1 - 
                                        up -> mask_assignment [i])) & 0377;
                  }
                else
                  maskab [i] = 0001;
              }

            int scu_port_num = -1; // The port that the rscr instruction was
                                   // received on

            for (int pn = 0; pn < N_SCU_PORTS; pn ++)
              {
                for (int sn = 0; sn < N_SCU_SUBPORTS; sn ++)
                  {
                    if (kables->scu_to_cpu[scuUnitIdx][pn][sn].cpu_unit_idx == (int) cpuUnitIdx)
                     {
                        scu_port_num = pn;
                        goto gotit;
                      }
                  }
              }
gotit:;

            //sim_printf ("scu_port_num %d\n", scu_port_num);

            if (scu_port_num < 0)
              {
#ifdef THREADZ
                unlock_scu ();
#endif
                sim_warn ("%s: can't find cpu port in the snarl of cables; "
                           "scu_unit_no %d, cpuUnitIdx %d\n", 
                           __func__, scuUnitIdx, cpuUnitIdx);
                return SCPE_OK;
              }

            // AN87, pg 2-5
            word36 a, q;

            a = 0;
// (data, starting bit position, number of bits, value)
            putbits36_9 (& a,  0,  maskab [0]);
            putbits36_3 (& a,  9,  (word3) up -> lower_store_size);
            putbits36_4 (& a, 12,  (word4) up -> onl); // A, A1, B, B1 online
            putbits36_4 (& a, 16,  (word4) scu_port_num);
            putbits36_1 (& a, 21,  (word1) config_switches[scuUnitIdx].mode);
            putbits36_8 (& a, 22,  (word8) up -> nea);
            putbits36_1 (& a, 30,  (word1) up -> interlace);
            putbits36_1 (& a, 31,  (word1) up -> lwr);
            // XXX INT, LWR not implemented. (AG87-00A pgs 2-5. 2-6)
            // interlace <- 0
            // lower <- 0
            // Looking at scr_util.list, I *think* the port order
            // 0,1,2,3.
            putbits36_1 (& a, 32,  (word1) up -> port_enable [0]);
            putbits36_1 (& a, 33,  (word1) up -> port_enable [1]);
            putbits36_1 (& a, 34,  (word1) up -> port_enable [2]);
            putbits36_1 (& a, 35,  (word1) up -> port_enable [3]);
            * rega = a;

            q = 0;
            putbits36_9 (& q,  0,  maskab [1]);
            // cyclic prior <- 0
            putbits36_7 (& q, 57-36, (word7) up -> cyclic & MASK7);
            // Looking at scr_util.list, I *think* the port order
            // 0,1,2,3.
            putbits36_1 (& q, 32,  (word1) up -> port_enable [4]);
            putbits36_1 (& q, 33,  (word1) up -> port_enable [5]);
            putbits36_1 (& q, 34,  (word1) up -> port_enable [6]);
            putbits36_1 (& q, 35,  (word1) up -> port_enable [7]);
            * regq = q;

#ifdef THREADZ
            unlock_scu ();
#endif
sim_printf ("rscr 1 %d A: %012"PRIo64" Q: %012"PRIo64"\n", scuUnitIdx, * rega, * regq);
            sim_debug (DBG_DEBUG, & scu_dev, "rscr 1 %d A: %012"PRIo64" Q: %012"PRIo64"\n", scuUnitIdx, * rega, * regq);
            break;
          }

        case 00002: // mask register
          {
            uint portNum = (addr >> 6) & MASK3;
#ifdef THREADZ
            lock_scu ();
#endif
            scu_t * up = scu + scuUnitIdx;
            uint maskContents = 0;
            if (up -> mask_assignment [0] == portNum)
              {
                maskContents = up -> exec_intr_mask [0];
              }
            else if (up -> mask_assignment [1] == portNum)
              {
                maskContents = up -> exec_intr_mask [1];
              }
            maskContents &= MASK32;

            * rega = 0;
            putbits36 (rega,  0, 16, (maskContents >> 16) & MASK16);
            putbits36 (rega, 32,  1, up -> port_enable [0]);
            putbits36 (rega, 33,  1, up -> port_enable [1]);
            putbits36 (rega, 34,  1, up -> port_enable [2]);
            putbits36 (rega, 35,  1, up -> port_enable [3]);

            * regq = 0;
            putbits36 (rega,  0, 16, (maskContents >>  0) & MASK16);
            putbits36 (regq, 32,  1, up -> port_enable [4]);
            putbits36 (regq, 33,  1, up -> port_enable [5]);
            putbits36 (regq, 34,  1, up -> port_enable [6]);
            putbits36 (regq, 35,  1, up -> port_enable [7]);

#ifdef THREADZ
            unlock_scu ();
#endif
            sim_debug (DBG_TRACE, & scu_dev,
                       "RSCR mask unit %u port %u assigns %u %u mask 0x%08x\n",
                       scuUnitIdx, portNum, up -> mask_assignment [0],
                       up -> mask_assignment [1],
                       maskContents);
          }
          break;

        case 00003: // Interrupt cells
          {
#ifdef THREADZ
            lock_scu ();
#endif
            scu_t * up = scu + scuUnitIdx;
            // * rega = up -> exec_intr_mask [0];
            // * regq = up -> exec_intr_mask [1];
            for (uint i = 0; i < N_CELL_INTERRUPTS; i ++)
              {
                word1 cell = up -> cells [i] ? 1 : 0;
                if (i < 16)
                  putbits36_1 (rega, i, cell);
                else
                  putbits36_1 (regq, i - 16, cell);
              }
#ifdef THREADZ
            unlock_scu ();
#endif
          }
          break;

        case 00004: // Get calendar clock (4MW SCU only)
        case 00005: 
          {
            uint64 clk = getSCUclock (scuUnitIdx);
            cpu . rQ =  clk & 0777777777777;     // lower 36-bits of clock
            cpu . rA = (clk >> 36) & 0177777;    // upper 16-bits of clock
            HDBGRegA ();
          }
        break;

        case 00006: // SU Mode register
        case 00007: // SU Mode register
          {
            //sim_printf ("rscr SU Mode Register%o\n", function);

// Completely undocumented...
//   scr.incl.alm
//"         Structure scr_su
//"
//          equ       scr_su_size,2
//
//
//          equ       scr_su.ZAC_line_word,1
//          equ       scr_su.ZAC_line_shift,30
//          bool      scr_su.ZAC_line_mask,000077
//          equ       scr_su.syndrome_word,1
//          equ       scr_su.syndrome_shift,22
//          bool      scr_su.syndrome_mask,000377
//          equ       scr_su.identification_word,1
//          equ       scr_su.identification_shift,18
//          bool      scr_su.identification_mask,000017
//          equ       scr_su.EDAC_disabled_word,1
//          bool      scr_su.EDAC_disabled,400000   " DL
//          equ       scr_su.MINUS_5_VOLT_margin_word,1
//"         equ       scr_su.MINUS_5_VOLT_margin_shift,11
//          bool      scr_su.MINUS_5_VOLT_margin_mask,000003
//          equ       scr_su.PLUS_5_VOLT_margin_word,1
//          equ       scr_su.PLUS_5_VOLT_margin_shift,9
//          bool      scr_su.PLUS_5_VOLT_margin_mask,000003
//          equ       scr_su.spare_margin_word,1
//          equ       scr_su.spare_margin_shift,7
//          bool      scr_su.spare_margin_mask,000003
//          equ       scr_su.PLUS_19_VOLT_margin_word,1
//"         equ       scr_su.PLUS_19_VOLT_margin_shift,5
//          bool      scr_su.PLUS_19_VOLT_margin_mask,000003
//          equ       scr_su.SENSE_strobe_margin_word,1
//"         equ       scr_su.SENSE_strobe_margin_shift,2
//          bool      scr_su.SENSE_strobe_margin_mask,000003
//"         equ       scr_su.maint_functions_enabled_word,1
//          bool      scr_su.maint_functions_enabled,000001 " DL

//                 1   1      1   2    2    2       2     3   3        3  3
//   0     6       4   8      9   3    5    7       9     1   2        4  5
//  ------------------------------------------------------------------------------
//  | ZAC | synd | id | EDAC | 0 | -5 | +5 | spare | +19 | 0 | sense | 0 | maint |
//  ------------------------------------------------------------------------------
//       6      8    4      1   4    2    2      2      2   1       2   1       1

// Okay, it looks safe to return 0.

            * rega = 0; 
            * regq = 0;
          }
          break;

        default:
          sim_warn ("rscr %o\n", function);
          return SCPE_OK;
      }
    return SCPE_OK;
  }

int scu_cioc (uint cpuUnitIdx, uint scuUnitIdx, uint scu_port_num, 
              uint expander_command, uint sub_mask)
  {
    sim_debug (DBG_DEBUG, & scu_dev, 
               "scu_cioc: Connect from %o sent to "
               "unit %o port %o exp %o mask %03o\n", 
               cpuUnitIdx, scuUnitIdx, scu_port_num,
              expander_command, sub_mask);

#ifdef THREADZ
    lock_scu ();
#endif
    struct ports * portp = & scu [scuUnitIdx] . ports [scu_port_num];

    int rc = 0;
    if (! scu [scuUnitIdx] . port_enable [scu_port_num])
      {
        sim_debug (DBG_ERR, & scu_dev, 
                   "scu_cioc: Connect sent to disabled port; dropping\n");
        sim_debug (DBG_ERR, & scu_dev, 
                   "scu_cioc: scuUnitIdx %u scu_port_num %u\n",
                   scuUnitIdx, scu_port_num);
        rc = 1;
        goto done;
      }

    if (expander_command == 1) // "set subport enables"
      {
        for (uint i = 0; i < N_SCU_SUBPORTS; i++)
          {
            portp->subport_enables [i] = !! (sub_mask & (0200u >> i));
          }
        goto done;
      }

    if (expander_command == 2) // "set xipmask"
      {
        int cnt = 0;
        int val = -1;
        for (uint i = 0; i < N_SCU_SUBPORTS; i++)
          {
            portp->xipmask [i] = !! (sub_mask & (0200u >> i));
            if (portp->xipmask [i])
              {
                val = (int) i;
                cnt ++;
              }
          }
        if (cnt > 1)
          {
            sim_warn ("xip mask cnt > 1\n");
            val = -1;
          }
        portp->xipmaskval = val;
        goto done;
      }

    if (portp -> type == ADEV_IOM)
      {
        int iomUnitIdx = portp->devIdx;
#ifdef THREADZ
        unlock_scu ();
        iom_interrupt (scuUnitIdx, (uint) iomUnitIdx);
        return 0;
#else // ! THREADZ
        if (sys_opts . iom_times . connect < 0)
          {
            iom_interrupt (scuUnitIdx, (uint) iomUnitIdx);
            goto done;
          }
        else
          {
//sim_printf ("scu_cioc: Queuing an IOM in %d cycles "
//"(for the connect channel) %u %d\n", 
//sys_opts . iom_times . connect, scuUnitIdx, iomUnitNum);
            sim_debug (DBG_INFO, & scu_dev, 
                       "scu_cioc: Queuing an IOM in %d cycles "
                       "(for the connect channel)\n", 
                       sys_opts . iom_times . connect);
            // Stash the iom_interrupt call parameters
            iom_dev.units[iomUnitIdx].u3 = (int32) scuUnitIdx;
            iom_dev.units[iomUnitIdx].u4 = (int32) iomUnitIdx;
            int rc;
            if ((rc = sim_activate (& iom_dev . units [iomUnitIdx], 
                sys_opts . iom_times.connect)) != SCPE_OK) 
              {
                sim_warn ("sim_activate failed (%d)\n", rc); 
                goto done;
              }
            goto done;
          }
#endif // ! THREADZ
      }
    else if (portp -> type == ADEV_CPU)
      {

#if 1
// by subport_enables
        if (portp->is_exp)
          {
            for (uint sn = 0; sn < N_SCU_SUBPORTS; sn ++)
              {
                if (portp->subport_enables[sn])
                  {
                    if (! kables->scu_to_cpu[scuUnitIdx][scu_port_num][sn].in_use)
                      {
                        sim_warn ("Can't find CPU to interrupt\n");
//sim_printf ("scu_cioc: Connect from %o sent to unit %o port %o exp %o mask %03o\n", cpuUnitIdx, scuUnitIdx, scu_port_num, expander_command, sub_mask);
//sim_printf ("is_exp %u xipmaskval %o\n", portp->is_exp, portp->xipmaskval);
                        continue;
                      }
                    uint cpuUnitIdx = kables->scu_to_cpu[scuUnitIdx][scu_port_num][sn].cpu_unit_idx;
                    setG7fault ((uint) cpuUnitIdx, FAULT_CON, fst_zero);
                  }
              }
          }
        else
          {
            if (! kables->scu_to_cpu[scuUnitIdx][scu_port_num][0].in_use)
              {
                sim_warn ("Can't find CPU to interrupt\n");
//sim_printf ("scu_cioc: Connect from %o sent to unit %o port %o exp %o mask %03o\n", cpuUnitIdx, scuUnitIdx, scu_port_num, expander_command, sub_mask);
//sim_printf ("is_exp %u xipmaskval %o\n", portp->is_exp, portp->xipmaskval);
                rc = 1;
                goto done;
              }
            uint cpuUnitIdx = kables->scu_to_cpu[scuUnitIdx][scu_port_num][0].cpu_unit_idx;
            setG7fault ((uint) cpuUnitIdx, FAULT_CON, fst_zero);
          }
#else
// by xipmaskval
        int cpuUnitIdx = -1;
        if (portp->is_exp)
          {
            cpuUnitIdx = cables ->cablesFromCpus[scuUnitIdx][scu_port_num][portp->xipmaskval].cpuUnitIdx;
          }
        else
          {
            cpuUnitIdx = cables ->cablesFromCpus[scuUnitIdx][scu_port_num][0].cpuUnitIdx;
          }
        if (cpuUnitIdx < 0)
          {
            sim_warn ("Can't find CPU to interrupt\n");
sim_printf ("scu_cioc: Connect from %o sent to unit %o port %o exp %o mask %03o\n", cpuUnitIdx, scuUnitIdx, scu_port_num, expander_command, sub_mask);
sim_printf ("is_exp %u xipmaskval %o\n", portp->is_exp, portp->xipmaskval);
            rc = 1;
            goto done;
          }
        setG7fault (cpuUnitIdx, FAULT_CON, (_fault_subtype) {.bits=0});
#endif
        goto done;
      }
    else
      {
        sim_debug (DBG_ERR, & scu_dev, 
                   "scu_cioc: Connect sent to not-an-IOM or CPU; dropping\n");
        rc = 1;
        goto done;
      }
done:
#ifdef THREADZ
    unlock_scu ();
#endif
    return rc;
}


// =============================================================================

// The SXC (set execute cells) SCU command.

// From AN70:
//  It then generates a word with
// the <interrupt number>th bit set and sends this to the bootload
// SCU with the SC (set execute cells) SCU command. 
//

int scu_set_interrupt (uint scuUnitIdx, uint inum)
  {
    const char* moi = "SCU::interrupt";
    
    if (inum >= N_CELL_INTERRUPTS) 
      {
        sim_debug (DBG_WARN, & scu_dev, 
                   "%s: Bad interrupt number %d\n", moi, inum);
        return 1;
      }
    
#ifdef THREADZ
    lock_scu ();
#endif
    scu [scuUnitIdx] . cells [inum] = 1;
    dumpIR ("scu_set_interrupt", scuUnitIdx);
    deliverInterrupts (scuUnitIdx);
#ifdef THREADZ
    unlock_scu ();
#endif
    return 0;
}

// Either an interrupt has arrived on a port, or a mask register has
// been updated. Bring the CPU up date on the interrupts.

// threadz notes:
//
// deliverInterrupts is called either from a CPU instruction or from
// IOM set_general_interrupt on the IOM thread.
//
// potential race condiions: 
//   CPU variables: XIP
//   SCU variables: cells, mask_enable, exec_intr_mask, mask assignment


// Always called with SCU lock set

static void deliverInterrupts (uint scuUnitIdx)
  {
    sim_debug (DBG_DEBUG, & scu_dev, "deliverInterrupts %o\n", scuUnitIdx);
    for (uint cpun = 0; cpun < cpu_dev.numunits; cpun ++)
      {
        cpus[cpun].events.XIP[scuUnitIdx] = false;
      }

    for (uint inum = 0; inum < N_CELL_INTERRUPTS; inum ++)
      {
        if (! scu [scuUnitIdx] . cells [inum])
          continue; //
        sim_debug (DBG_DEBUG, & scu_dev, "trying to deliver %d\n", inum);
        sim_debug (DBG_INTR, & scu_dev, "scu %u trying to deliver %d\n", scuUnitIdx, inum);


        for (uint pima = 0; pima < N_ASSIGNMENTS; pima ++) // A, B
          {
            //sim_debug (DBG_DEBUG, & scu_dev, "trying inum %u pima %u enable %u\n", inum, pima, scu [scuUnitIdx] . mask_enable [pima]);
            if (scu [scuUnitIdx] . mask_enable [pima] == 0)
              continue;
            uint mask = scu [scuUnitIdx] . exec_intr_mask [pima];
            uint port = scu [scuUnitIdx] . mask_assignment [pima];
            //sim_debug (DBG_DEBUG, & scu_dev, "mask %u port %u type %u cells %o\n", mask, port, scu [scuUnitIdx] . ports [port] . type, scu [scuUnitIdx] . cells [inum]);
            if (scu [scuUnitIdx].ports [port].type != ADEV_CPU)
              continue;
            if ((mask & (1u << (31 - inum))) != 0)
              {
                uint sn = 0;
                if (scu[scuUnitIdx].ports[port].is_exp)
                  sn = (uint) scu[scuUnitIdx].ports[port].xipmaskval;
                if (! kables->scu_to_cpu[scuUnitIdx][port][sn].in_use)
                  sim_err ("bad scuUnitIdx %u\n", scuUnitIdx);
                uint cpuUnitIdx = kables->scu_to_cpu[scuUnitIdx][port][sn].cpu_unit_idx;
#ifdef THREADZ
                cpus[cpuUnitIdx].events.XIP[scuUnitIdx] = true;
#ifdef HDBG
                hdbgIntrSet (inum, cpuUnitIdx, scuUnitIdx);
#endif
                setCPURun ((uint) cpuUnitIdx, true);
                wakeCPU ((uint) cpuUnitIdx);
                sim_debug (DBG_DEBUG, & scu_dev,
                           "interrupt set for CPU %d SCU %d\n",
                           cpuUnitIdx, scuUnitIdx);
#else // ! THREADZ
//if (cpuUnitIdx && ! cpu.isRunning) sim_printf ("starting CPU %c\n", cpuUnitIdx + 'A');
#ifdef ROUND_ROBIN
                cpus[cpuUnitIdx].isRunning = true;
#endif
                cpus[cpuUnitIdx].events.XIP[scuUnitIdx] = true;
sim_debug (DBG_DEBUG, & scu_dev, "interrupt set for CPU %d SCU %d\n", cpuUnitIdx, scuUnitIdx);
                sim_debug (DBG_INTR, & scu_dev,
                           "XIP set for SCU %d\n", scuUnitIdx);
#endif // ! THREADZ
              }
          }
      }
  }

// Scan a SCU for interrupts from highest to lowest. If an interrupt is
// present, clear it, update the interrupt state bits and return the fault
// pair address for the interrupt (2 * interrupt number). If no interrupt
// is present, return 1.
//

uint scuGetHighestIntr (uint scuUnitIdx)
  {
#ifdef THREADZ
    lock_scu ();
#endif
    for (int inum = N_CELL_INTERRUPTS - 1; inum >= 0; inum --)
      {
        for (uint pima = 0; pima < N_ASSIGNMENTS; pima ++) // A, B
          {
            if (scu [scuUnitIdx] . mask_enable [pima] == 0)
              continue;
            uint mask = scu [scuUnitIdx] . exec_intr_mask [pima];
            uint port = scu [scuUnitIdx] . mask_assignment [pima];
            if (scu [scuUnitIdx] . ports [port] . type != ADEV_CPU)
              continue;
            if (scu [scuUnitIdx] . cells [inum] &&
                (mask & (1u << (31 - inum))) != 0)
              {
                scu [scuUnitIdx] . cells [inum] = false;
                dumpIR ("scuGetHighestIntr", scuUnitIdx);
                deliverInterrupts (scuUnitIdx);
#ifdef THREADZ
                unlock_scu ();
#endif
                return (uint) inum * 2;
              }
          }
      }
#ifdef THREADZ
    unlock_scu ();
#endif
    return 1;
  }

// ============================================================================

static t_stat scu_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                               UNUSED int val, const UNUSED void * desc)
  {
    sim_printf("Number of SCU units in system is %d\n", scu_dev . numunits);
    return SCPE_OK;
  }

static t_stat scu_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, 
                              const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_SCU_UNITS_MAX)
      return SCPE_ARG;
    scu_dev . numunits = (uint) n;
    return SCPE_OK;
  }

static t_stat scu_show_state (UNUSED FILE * st, UNIT *uptr, UNUSED int val, 
                              UNUSED const void * desc)
  {
    long scuUnitIdx = UNIT_NUM (uptr);
    if (scuUnitIdx < 0 || scuUnitIdx >= (int) scu_dev . numunits)
      {
        sim_debug (DBG_ERR, & scu_dev, 
                   "scu_show_state: Invalid unit number %ld\n", 
                   scuUnitIdx);
        sim_printf ("error: invalid unit number %ld\n", scuUnitIdx);
        return SCPE_ARG;
      }

    sim_printf ("SCU unit number %ld\n", scuUnitIdx);
    scu_t * scup = scu + scuUnitIdx;
    sim_printf ("    Mode %s\n", config_switches[scuUnitIdx].mode ? "PROGRAM" : "MANUAL");

    for (int i = 0; i < N_SCU_PORTS; i ++)
      {
        struct ports * pp = scup -> ports + i;

        sim_printf ("    Port %d %s devIdx %d dev_port %d type %s\n",
                    i, scup->port_enable[i] ? "ENABLE " : "DISABLE",
                    pp->devIdx, pp->dev_port[XXX_TEMP_SCU_SUBPORT],
                    pp->type == ADEV_NONE ? "NONE" :
                    pp->type == ADEV_CPU ? "CPU" :
                    pp->type == ADEV_IOM ? "IOM" :
                    "<enum broken>");
      }
    for (int i = 0; i < N_ASSIGNMENTS; i ++)
      {
        //struct interrupts * ip = scup -> interrupts + i;

        sim_printf ("    Cell %c\n", 'A' + i);
        sim_printf ("        exec_intr_mask %012o\n", 
                    scup -> exec_intr_mask [i]);
        sim_printf ("        mask_enable %s\n", 
                    scup -> mask_enable [i] ? "ENABLE" : "DISABLE");
        sim_printf ("        mask_assignment %d\n", 
                    scup -> mask_assignment [i]);
        sim_printf ("        cells ");
        for (int j = 0; j < N_CELL_INTERRUPTS; j ++)
          sim_printf("%d", scup -> cells [j]);
        sim_printf ("\n");
      }
    sim_printf("Lower store size: %d\n", scup -> lower_store_size);
    sim_printf("Cyclic: %03o\n", scup -> cyclic);
    sim_printf("NEA: %03o\n", scup -> nea);
    sim_printf("Online: %02o\n", scup -> onl);
    sim_printf("Interlace: %o\n", scup -> interlace);
    sim_printf("Lower: %o\n", scup -> lwr);
    sim_printf("ID: %o\n", scup -> id);
    sim_printf("modeReg: %06o\n", scup -> modeReg);
    sim_printf("Elapsed days: %d\n", scup -> elapsed_days);
    sim_printf("Steady clock: %d\n", scup -> steady_clock);
    sim_printf("Bullet time: %d\n", scup -> bullet_time);
    sim_printf("Y2K enabled: %d\n", scup -> y2k);
    return SCPE_OK;
  }

static t_stat scu_show_config (UNUSED FILE * st, UNUSED UNIT * uptr, 
                               UNUSED int val, UNUSED const void * desc)
{
    static const char * map [N_SCU_PORTS] = 
      {
        "0", "1", "2", "3", "4", "5", "6", "7" 
      };
    long scuUnitIdx = UNIT_NUM (uptr);
    if (scuUnitIdx < 0 || scuUnitIdx >= (int) scu_dev . numunits)
      {
        sim_debug (DBG_ERR, & scu_dev, 
                   "scu_show_config: Invalid unit number %ld\n", 
                   scuUnitIdx);
        sim_printf ("error: invalid unit number %ld\n", scuUnitIdx);
        return SCPE_ARG;
      }

    sim_printf ("SCU unit number %ld\n", scuUnitIdx);

    struct config_switches * sw = config_switches + scuUnitIdx;

    const char * mode = "<out of range>";
    switch (sw -> mode)
      {
        case MODE_PROGRAM:
          mode = "Program";
          break;
        case MODE_MANUAL:
          mode = "Manual";
          break;
      }

    sim_printf ("Mode:                     %s\n", mode);
    sim_printf ("Port Enable:             ");
    for (int i = 0; i < N_SCU_PORTS; i ++)
      sim_printf (" %3o", sw -> port_enable [i]);
    sim_printf ("\n");
    for (int i = 0; i < N_ASSIGNMENTS; i ++)
      {
        sim_printf ("Mask %c:                   %s\n", 
                    'A' + i, 
                    sw -> mask_enable [i] ? (map [sw -> mask_assignment [i]]) : "Off");
      }
    sim_printf ("Lower Store Size:        %o\n", sw -> lower_store_size);
    sim_printf ("Cyclic:                  %03o\n", sw -> cyclic);
    sim_printf ("Non-existent address:    %03o\n", sw -> nea);

    return SCPE_OK;
  }

//
// set scu0 config=<blah> [;<blah>]
//
//    blah =
//           mode=  manual | program
//           mask[A|B] = off | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7
//           portN = enable | disable
//           lwrstoresize = 32 | 64 | 128 | 256 | 512 | 1024 | 2048 | 4096
//           cyclic = n
//           nea = n
//
//      o  nea is not implemented; will read as "nea off"
//      o  Multics sets cyclic priority explicitly; config
//         switches are ignored.
//      o  STORE A, A1, B, B1 ONLINE/OFFLINE not implemented;
//         will always read online.
//      o  store size if not enforced; a full memory complement
//         is provided.
//      o  interlace not implemented; will read as 'off'
//      o  LOWER STORE A/B not implemented.
//      o  MASK is 'MASK/PORT ASSIGNMENT' analagous to the
//         'EXECUTE INTERRUPT MASK ASSIGNMENT of a 6000 SCU

static config_value_list_t cfg_mode_list [] =
  {
    { "manual", 0 },
    { "program", 1 },
    { NULL, 0 }
  };

static config_value_list_t cfg_mask_list [] =
  {
    { "off", -1 },
    { NULL, 0 }
  };

static config_value_list_t cfg_able_list [] =
  {
    { "disable", 0 },
    { "enable", 1 },
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

static config_value_list_t cfg_on_off [] =
  {
    { "off", 0 },
    { "on", 1 },
    { "disable", 0 },
    { "enable", 1 },
    { NULL, 0 }
  };

static config_list_t scu_config_list [] =
  {
    /*  0 */ { "mode", 1, 0, cfg_mode_list },
    /*  1 */ { "maska", 0, N_SCU_PORTS - 1, cfg_mask_list },
    /*  2 */ { "maskb", 0, N_SCU_PORTS - 1, cfg_mask_list },
    /*  3 */ { "port0", 1, 0, cfg_able_list },
    /*  4 */ { "port1", 1, 0, cfg_able_list },
    /*  5 */ { "port2", 1, 0, cfg_able_list },
    /*  6 */ { "port3", 1, 0, cfg_able_list },
    /*  7 */ { "port4", 1, 0, cfg_able_list },
    /*  8 */ { "port5", 1, 0, cfg_able_list },
    /*  9 */ { "port6", 1, 0, cfg_able_list },
    /* 10 */ { "port7", 1, 0, cfg_able_list },
    /* 11 */ { "lwrstoresize", 0, 7, cfg_size_list },
    /* 12 */ { "cyclic", 0, 0177, NULL },
    /* 13 */ { "nea", 0, 0377, NULL },
    /* 14 */ { "onl", 0, 017, NULL }, // mask: 8 a_online, 4 a1_online, 2 b_online, 1, b1_online
    /* 15 */ { "int", 0, 1, NULL },
    /* 16 */ { "lwr", 0, 1, NULL },

    // Hacks

    /* 17 */ { "elapsed_days", 0, 20000, NULL },
    /* 18 */ { "steady_clock", 0, 1, cfg_on_off },
    /* 19 */ { "bullet_time", 0, 1, cfg_on_off },
    /* 20 */ { "y2k", 0, 1, cfg_on_off },

    { NULL, 0, 0, NULL }
  };

static t_stat scu_set_config (UNIT * uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc)
  {
    long scuUnitIdx = UNIT_NUM (uptr);
    if (scuUnitIdx < 0 || scuUnitIdx >= (int) scu_dev . numunits)
      {
        sim_debug (DBG_ERR, & scu_dev, 
                   "scu_set_config: Invalid unit number %ld\n", scuUnitIdx);
        sim_printf ("error: scu_set_config: invalid unit number %ld\n", 
                    scuUnitIdx);
        return SCPE_ARG;
      }

    struct config_switches * sw = config_switches + scuUnitIdx;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("scu_set_config", cptr, scu_config_list, 
                           & cfg_state, & v);
        if (rc == -1) // done
          break;

        if (rc == -2) // error
          {
            cfgparse_done (& cfg_state);
            return SCPE_ARG; 
          }

        const char * p = scu_config_list [rc] . name;
        if (strcmp (p, "mode") == 0)
          sw -> mode = (uint) v;
        else if (strcmp (p, "maska") == 0)
          {
            if (v == -1)
              sw -> mask_enable [0] = false;
            else
              {
                sw -> mask_enable [0] = true;
                sw -> mask_assignment [0] = (uint) v;
              }
          }
        else if (strcmp (p, "maskb") == 0)
          {
            if (v == -1)
              sw -> mask_enable [1] = false;
            else
              {
                sw -> mask_enable [1] = true;
                sw -> mask_assignment [1] = (uint) v;
              }
          }
        else if (strcmp (p, "port0") == 0)
          sw -> port_enable [0] = (uint) v;
        else if (strcmp (p, "port1") == 0)
          sw -> port_enable [1] = (uint) v;
        else if (strcmp (p, "port2") == 0)
          sw -> port_enable [2] = (uint) v;
        else if (strcmp (p, "port3") == 0)
          sw -> port_enable [3] = (uint) v;
        else if (strcmp (p, "port4") == 0)
          sw -> port_enable [4] = (uint) v;
        else if (strcmp (p, "port5") == 0)
          sw -> port_enable [5] = (uint) v;
        else if (strcmp (p, "port6") == 0)
          sw -> port_enable [6] = (uint) v;
        else if (strcmp (p, "port7") == 0)
          sw -> port_enable [7] = (uint) v;
        else if (strcmp (p, "lwrstoresize") == 0)
          sw -> lower_store_size = (uint) v;
        else if (strcmp (p, "cyclic") == 0)
          sw -> cyclic = (uint) v;
        else if (strcmp (p, "nea") == 0)
          sw -> nea = (uint) v;
        else if (strcmp (p, "onl") == 0)
          sw -> onl = (uint) v;
        else if (strcmp (p, "int") == 0)
          sw -> interlace = (uint) v;
        else if (strcmp (p, "lwr") == 0)
          sw -> lwr = (uint) v;
        else if (strcmp (p, "elapsed_days") == 0)
          scu [scuUnitIdx].elapsed_days = (uint) v;
        else if (strcmp (p, "steady_clock") == 0)
          scu [scuUnitIdx].steady_clock = (uint) v;
        else if (strcmp (p, "bullet_time") == 0)
          scu [scuUnitIdx].bullet_time = (uint) v;
        else if (strcmp (p, "y2k") == 0)
          scu [scuUnitIdx].y2k = (uint) v;
        else
          {
            sim_debug (DBG_ERR, & scu_dev, 
                       "scu_set_config: Invalid cfgparse rc <%d>\n", rc);
            sim_printf ("error: scu_set_config: invalid cfgparse rc <%d>\n", 
                         rc);
            cfgparse_done (& cfg_state);
            return SCPE_ARG; 
          }
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }

t_stat scu_reset_unit (UNIT * uptr, UNUSED int32 value, UNUSED const char * cptr, 
                       UNUSED void * desc)
  {
    uint scuUnitIdx = (uint) (uptr - scu_unit);
    scuUnitReset ((int) scuUnitIdx);
    return SCPE_OK;
  }

void scu_init (void)
  {
    // One time only initializations

    for (int u = 0; u < N_SCU_UNITS_MAX; u ++)
      {
        for (int p = 0; p < N_SCU_PORTS; p ++)
          {
            for (int s = 0; s < N_SCU_SUBPORTS; s ++)
              {
                scu[u].ports[p].dev_port[s] = -1;
                scu[u].ports[p].subport_enables[s] = false;
                scu[u].ports[p].xipmask[s] = false;
                scu[u].ports[p].xipmaskval = -1;
              }
            scu[u].ports[p].type = ADEV_NONE;
            scu[u].ports[p].is_exp = false;
          }

        //  ID: 0000  8034, 8035
        //      0001  Level 68 SC
        //      0010  Level 66 SCU
        scu [u] . id = 0b0010;
        scu [u] . modeReg = 0; // used by T&D
        scu [u] . elapsed_days = 0;
      }

  }

t_stat scu_rmcm (uint scuUnitIdx, uint cpuUnitIdx, word36 * rega, 
                 word36 * regq)
  {
    scu_t * up = scu + scuUnitIdx;

    // Assume no mask register assigned
    * rega = 0;
    * regq = 0;

    // Which port is cpuUnitIdx connected to? (i.e. which port did the 
    // command come in on?
    int scu_port_num = -1; // The port that the rscr instruction was
                           // received on

    for (int pn = 0; pn < N_SCU_PORTS; pn ++)
      {
        for (int sn = 0; sn < N_SCU_SUBPORTS; sn ++)
          {
            if (kables->scu_to_cpu[scuUnitIdx][pn][sn].cpu_unit_idx == (int) cpuUnitIdx)
              {
                scu_port_num = pn;
                goto gotit;
              }
          }
      }

gotit:;

    //sim_printf ("rmcm scu_port_num %d\n", scu_port_num);

    if (scu_port_num < 0)
      {
        sim_warn ("%s: can't find cpu port in the snarl of cables; "
                  "scu_unit_no %d, cpuUnitIdx %d\n", 
                  __func__, scuUnitIdx, cpuUnitIdx);
        sim_debug (DBG_ERR, & scu_dev, 
                   "%s: can't find cpu port in the snarl of cables; "
                   "scu_unit_no %d, cpuUnitIdx %d\n", 
                   __func__, scuUnitIdx, cpuUnitIdx);
        // Non 4MWs do a store fault
        return SCPE_OK;
      }

    // A reg:
    //  0          15  16           31  32       35
    //    IER 0-15        00000000        PER 0-3 
    // Q reg:
    //  0          15  16           31  32       35
    //    IER 16-32       00000000        PER 4-7 

    sim_debug (DBG_TRACE, & scu_dev, "rmcm selected scu port %u\n", scu_port_num);
#ifdef THREADZ
    lock_scu ();
#endif
    uint maskContents = 0;
    if (up -> mask_assignment [0] == (uint) scu_port_num)
      {
        maskContents = up -> exec_intr_mask [0];
        sim_debug (DBG_TRACE, & scu_dev, "rmcm got mask %011o from pima A\n", maskContents);
      }
    else if (up -> mask_assignment [1] == (uint) scu_port_num)
      {
        maskContents = up -> exec_intr_mask [1];
        sim_debug (DBG_TRACE, & scu_dev, "rmcm got mask %011o from pima B\n", maskContents);
      }
    maskContents &= MASK32;

    * rega = 0;
    putbits36_16 (rega,  0, (maskContents >> 16) & MASK16);
    putbits36_1 (rega, 32,  (word1) up -> port_enable [0]);
    putbits36_1 (rega, 33,  (word1) up -> port_enable [1]);
    putbits36_1 (rega, 34,  (word1) up -> port_enable [2]);
    putbits36_1 (rega, 35,  (word1) up -> port_enable [3]);

    * regq = 0;
    putbits36_16 (regq,  0, (maskContents >>  0) & MASK16);
    putbits36_1 (regq, 32,  (word1) up -> port_enable [4]);
    putbits36_1 (regq, 33,  (word1) up -> port_enable [5]);
    putbits36_1 (regq, 34,  (word1) up -> port_enable [6]);
    putbits36_1 (regq, 35,  (word1) up -> port_enable [7]);

#ifdef THREADZ
    unlock_scu ();
#endif
    sim_debug (DBG_TRACE, & scu_dev, "RMCM returns %012"PRIo64" %012"PRIo64"\n", 
               * rega, * regq);
    dumpIR ("rmcm", scuUnitIdx);
    return SCPE_OK;
  }

t_stat scu_smcm (uint scuUnitIdx, uint cpuUnitIdx, word36 rega, word36 regq)
  {
    sim_debug (DBG_TRACE, & scu_dev, 
              "SMCM SCU unit %d CPU unit %d A %012"PRIo64" Q %012"PRIo64"\n",
               scuUnitIdx, cpuUnitIdx, rega, regq);

    scu_t * up = scu + scuUnitIdx;

    // Which port is cpuUnitIdx connected to? (i.e. which port did the 
    // command come in on?
    int scu_port_num = -1; // The port that the rscr instruction was
                           // received on

    for (int pn = 0; pn < N_SCU_PORTS; pn ++)
      {
        for (int sn = 0; sn < N_SCU_SUBPORTS; sn ++)
          {
            if (kables->scu_to_cpu[scuUnitIdx][pn][sn].cpu_unit_idx == (int) cpuUnitIdx)
              {
                scu_port_num = pn;
                goto gotit;
              }
          }
      }
gotit:;

    //sim_printf ("rmcm scu_port_num %d\n", scu_port_num);

    if (scu_port_num < 0)
      {
        sim_warn ("%s: can't find cpu port in the snarl of cables; "
                   "scu_unit_no %d, cpuUnitIdx %d\n", 
                   __func__, scuUnitIdx, cpuUnitIdx);
        return SCPE_OK;
      }

    sim_debug (DBG_TRACE, & scu_dev, "SMCM SCU port num %d\n", scu_port_num);

    // A reg:
    //  0          15  16           31  32       35
    //    IER 0-15        00000000        PER 0-3 
    // Q reg:
    //  0          15  16           31  32       35
    //    IER 16-32       00000000        PER 4-7 

    uint imask =
      ((uint) getbits36_16(rega, 0) << 16) |
      ((uint) getbits36_16(regq, 0) <<  0);
#ifdef THREADZ
    lock_scu ();
#endif
    if (up -> mask_assignment [0] == (uint) scu_port_num)
      {
        up -> exec_intr_mask [0] = imask;
        sim_debug (DBG_TRACE, & scu_dev, "SMCM intr mask 0 set to %011o\n",
                   imask);
      }
    else if (up -> mask_assignment [1] == (uint) scu_port_num)
      {
        up -> exec_intr_mask [1] = imask;
        sim_debug (DBG_TRACE, & scu_dev, "SMCM intr mask 1 set to %011o\n",
                   imask);
      }

    scu [scuUnitIdx] . port_enable [0] = (uint) getbits36_1 (rega, 32);
    scu [scuUnitIdx] . port_enable [1] = (uint) getbits36_1 (rega, 33);
    scu [scuUnitIdx] . port_enable [2] = (uint) getbits36_1 (rega, 34);
    scu [scuUnitIdx] . port_enable [3] = (uint) getbits36_1 (rega, 35);
    scu [scuUnitIdx] . port_enable [4] = (uint) getbits36_1 (regq, 32);
    scu [scuUnitIdx] . port_enable [5] = (uint) getbits36_1 (regq, 33);
    scu [scuUnitIdx] . port_enable [6] = (uint) getbits36_1 (regq, 34);
    scu [scuUnitIdx] . port_enable [7] = (uint) getbits36_1 (regq, 35);

    dumpIR ("smcm", scuUnitIdx);
    deliverInterrupts (scuUnitIdx);
#ifdef THREADZ
    unlock_scu ();
#endif
    
    return SCPE_OK;
  }
