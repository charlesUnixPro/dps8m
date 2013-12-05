//
//  dps8_scu.c
//  dps8
//
//  Created by Harry Reed on 6/15/13.
//  Original portions Copyright (c) 2013 Harry Reed. All rights reserved.
//
//  Derived (& originally stolen) from .....

// XXX This is used wherever a single unit only is assumed
#define ASSUME0 0

/*
 scu.c -- System Controller
 
 See AN70, section 8 and GB61.
 
 There were a few variations of SCs and SCUs:
 SCU -- Series 60 Level66 Controller
 SC -- Level 68 System Controller
 4MW SCU -- A later version of the L68 SC
 
 SCUs control access to memory.
 Each SCU owns a certain range of absolute memory.
 This emulator allows the CPU to access memory directly however.
 SCUs contain clocks.
 SCUS also contain facilites which allow CPUS and IOMs to communicate.
 CPUs or IOMS request access to memory via the SCU.
 CPUs use the cioc instr to talk to IOMs and other CPUs via a SCU.
 IOMs use interrupts to ask a SCU to signal a CPU.
 Other Interesting instructions:
 read system controller reg and set system controller reg (rscr & sscr)
 
 */
/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */


/*
 Physical Details & Interconnection -- AN70, section 8.
 
 SCUs have 8 ports.
 * Active modules (CPUs and IOMs) have up to four of their ports
 connected to SCU ports.
 * The 4MW SCU has eight on/off switches to enable or disable
 the ports.  However, the associated registers allow for
 values of enabled, disabled, and program control.
 
 SCUs have stores (memory banks).
 
 SCUs have four sets of registers controlling interrupts.  Only two
 of these sets, designated "A" and "B" are used.  Each set has:
 * Execute interrupt mask register -- 32 bits; enables/disables
 the corresponding execute interrupt cell
 * Interrupt mask assignment register -- 9 bits total
 two parts: assigned bit, set of assigned ports (8 bits)
 In Multics, only one CPU will be assigned in either mask
 and no CPU appears in both.   Earlier hardware versions had
 four 10-position rotary switches.  Later hardware versions had
 two 9-position (0..7 and off) rotary switches.
 
 Config panel -- level 68 6000 SCU
 -- from AM81
 store A and store B
 3 position rotary switch: on line, maint, off line
 size: 32k, 64k, 128k, 256k
 exec interrupt mask assignment
 four 10-position rotary switches (a through d): off, 0, .. 7, M
 One switch for each program interrupt register
 Assign mask registers to system ports
 Normally assign one mask reg to each CPU

   AM81:
     "        The EXECUE INTERRUPT MASK ASSIGNENT (EIMA) rotary switches
      determine where interrupts sent to memory are directed.  The four EIMA rotary
      switches, one for each program interrupt register, are used to assign mask registers to
      system ports. The normal settings assign one mask register to eah CPU configured.
      Each switch assigns mask registers as follows:

          Position
            OFF     Unassigned
              0     Assigned to port 0
                ...
              7     Assigned to port 7
              M     Assigned to maintainance panel

            Assignment of a mask register to a system port designates the port as a control
      port, and that port receives interrupt present signals. Up to four system ports can
      be designated as control ports. The normal settings assign one mask register to each CPY
      configured."


   
 Config panel -- Level 68 System Controller UNIT (4MW SCU)
 -- from AM81
 Store A, A1, B, B1 (online/offline)
 LWR Store Size
 PORT ENABLE
 Eight on/off switches
 Should be on for each port connected to a configured CPU
 mask/port assignment
 Two rotary switchs (A & B); set to (off, 0..7)
 See EXEC INTERRUPT on the 6000 SCU
 When booting, one should be set to the port connected to
 the bootload CPU.   The other should be off.
 
 If memory port B of CPU C goes to SCU D, then memory port B of all
 other CPUs *and* IOMs must go to SCU D. -- AN70, 8-4.
 
 The base address of the SCU is the actual memory size * the port
 assignment. -- AN70, 8-6.

  43a239854 6000B Eng. Prod. Spec, 3.2.7 Interrupt Multiplex Word:
    "The IOM has the ability to set any of the 32 program interrupt
     cells located in the system controller containing the base address
     of the IOM. It should be noted that for any given IOM identity
     switch setting, the IOM can set only 8 of these program interrupt
     cells."

    [CAC] XXX I don't understand "in the SC containing the base address
    of the IOM".

 */


/*
 The following comment is probably wrong:
 The term SCU is used throughout this code to match AL39, but the
 device emulated is closer to a Level 68 System Controller (SC) than
 to a Series 60 Level 66 Controller (SC).  The emulated device may
 be closer to a Level 68 4MW SCU than to an Level 68 6000 SCU.
 
 BUG/TODO: The above is probably wrong; we explicitly report an
 ID code for SCU via rscr 000001x.  It wouldn't hurt to review
 all the code to make sure we never act like a SC instead of an
 SCU.
 */


/*
 === Initialization and Booting -- Part 1 -- Operator's view
 
 Booting Instructions (GB61)
 First boot the BCE OS (Bootload command Environment).  See below.
 A config deck is used
 Bootload SCU is the one with a base addr of zero.
 BCE is on a BCE/Multics System tape
 Booted from tape into the system via bootload console
 
 */

/*
 58009906 (DPS8)
 When CPU needs to address the SCU (for a write/read data cycle,
 for example), the ETMCM board int the CU of the CPU issues a $INT
 to the SCU.  This signal is sent ... to the SCAMX active port
 control board in the SCU
 */

/*
 // How?  If one of the 32 interrupt cells is set in one of the SCs,
 // our processor will have the interrupt present (XIP) line active.
 // Perhaps faults are flagged in the same way via the SXC system
 // controller command.
 */

// =============================================================================
#if 0
TEMPORARY
Each SCU owns a certain range of absolute memory.
CPUs use the cioc instr to talk to IOMs and other CPUs via a SCU.
IOMs use interrupts to ask a SCU to signal a CPU.
read system controller reg and set system controller reg (rscr & sscr)
Bootload SCU is the one with a base addr of zero.
58009906
When CPU needs to address the SCU (for a write/read data cycle,
                                   for example), the ETMCM board int the CU of the CPU issues a $INT
to the SCU.  This signal is sent ... to the SCAMX active port
control board in the
-----------------------
// How?  If one of the 32 interrupt cells is set in one of the SCs,
// our processor will have the interrupt present (XIP) line active.
// Perhaps faults are flagged in the same way via the SXC system
// controller command.
#endif

/*
 *** More (new) notes ***
 
 instr rmcm -- read mem controller mask register
 ... for the selected controller, if the processor has a mask register
 assigned ..
 instr smcm -- set  mem controller mask register
 ... for the selected controller, if the processor has a mask register
 assigned, set it to C(AQ)
 instr smic
 turn on interrupt cells (any of 0..31)
 instr cioc -- connect i/o channel, pg 173
 SC addressed by Y sends a connect signal to the port specified
 by C(Y)33,35
 instr rscr & sscr -- Read/Store System Controller Register, pg 170
 
 32 interrupt cells ... XIP
 mask info
 8 mask registers
 58009906
 =============
 
 AM81
 Every active device (CPU, IOM) must be able to access all SCUs
 Every SCU must have the same active device on the same SCU, so
 all SCUs must have the same PORT ENABLE settings
 Every active device must have the same SCU on the same port,
 so all active devices will have the same config panel settings.
 Ports must correspond -- port A on every CPU and IOM must either
 be connected tothe same SCU or not connected to any SCU.
 IOMs should be on lower-numbered SCU ports than CPUs.
 Multics can have 16MW words of memory.
 CPUs have 8 ports, a..h.
 SCUs have 8 ports, 0..7.
 
 
Level 68 6000 SCU Configuration Panel
   system control and monitor (cont&mon/mon/ofF)
   system boot control (on/off)
   alarm (disable/normal)
   maintainance panel mode (test/normal)
   store a
      mode (offline/maint/online)
      size (32k, 64k, 128k, 256k)
   store b
      mode (offline/maint/online)
      size (32k, 64k, 128k, 256k)
   execute interrupt mask assigment
      (A through D; off/0/1/2/3/4/5/6/7/m)
   [CAC] I interperet this as CPU [A..D] is connected to my port [0..7]
   address control
      lower store (a/b)
      offset (off, 16k, 32k, 64k)
      interlace (on/off)
   cycle port priority (on/off)
   port control (8 toogles) (enabled/prog cont/disable)

 The EXECUTE INTERRUPT MASK ASSIGNMENT (EIMA) rotary switches
 determine where interrupts sent to memory are directed. The four EIMA rotary
 switches, one for each program interrupt register, are used to assign mask registers to
  system ports. The normal settings assign one mask register to each CPU configured.
 
  Assignment of a mask register to a system port designates the port as a control 
  port, and that port receives interrupt present signals. Up to four system ports can be
  designated as control ports. The normal settings assign one mask register to each cpu configured.



Configuration rules for Multics:

   1. Each CPU in the system must be connected to each SCU in the system

   2. Each IOM in the system must be connected to eacm SCU in the system

   3. Each SCU in the system must be connected to every CPU and IOM in the system.

   4. Corresponding ports on all CPUs and IOMs must be connected to the same
      SCU. For example, port A on every CPU and IOM must be connected to the
      same SCU or not connected to any SCU.

   5. Corresponding ports on all SCUs must be connected to the same active device
      (CPU or IOM). For example, if port 0 on any SCU is connected to IOM A,
      then port 0 on all SCUs must be connected to IOM A.

   6. IOMs should be connected to lower-number SCU ports the CPUs.

   These rules are illustrated in Figure 3-5, where the port numbers for a small Multics
   system of 2 CPUS, 3 SCUs and 2 IOMs have deen indicated

       


                    -----------------                      -----------------
                    |               |                      |               |
                    |     CPU A     |                      |     CPU B     |
                    |               |                      |               |
                    -----------------                      -----------------
                    | A | B | C | D |                      | A | B | C | D |
                    -----------------                      -----------------
                      |   |   |                              |   |   |
                      |   |   |                              |   |   -----------------
                      |   |   |                              |   |                   |
                      |   |   -------------------------------)---)----------------   |
                      |   |                                  |   |               |   |
   --------------------   -----------------                  |   |               |   |
   |                                      |                  |   |               |   |
   |   -----------------------------------)-------------------   |               |   |
   |   |                                  |                      |               |   |
   |   |                                  |   --------------------               |   |
   |   |                                  |   |                                  |   |
 -----------------                      -----------------                      -----------------
 | 7 | 6 | 5 | 4 |                      | 7 | 6 | 5 | 4 |                      | 7 | 6 | 5 | 4 |
 -----------------                      -----------------                      -----------------
 |               |                      |               |                      |               |
 |     SCU C     |                      |     SCU B     |                      |     SCU A     |
 |               |                      |               |                      |               |
 -----------------                      -----------------                      -----------------
 | 3 | 2 | 1 | 0 |                      | 3 | 2 | 1 | 0 |                      | 3 | 2 | 1 | 0 |
 -----------------                      -----------------                      -----------------
           |   |                                  |   |                                  |   |
           |   |                                  |   -----------                        |   |
           |   |                                  |             |                        |   |
           |   -----------------------------------)---------    |                        |   |
           |                                      |        |    |                        |   |
           ----------    --------------------------        |    |                        |   |
                    |    |                                 |    |                        |   |
                    |    |   ------------------------------)----)-------------------------   |
                    |    |   |                             |    |                            |
                    |    |   |                             |    |  ---------------------------
                    |    |   |                             |    |  |
                   -----------------                      -----------------
                   | A | B | C | D |                      | A | B | C | D |
                   -----------------                      -----------------
                   |               |                      |               |
                   |     IOM A     |                      |     IOM B     |
                   |               |                      |               |
                   -----------------                      -----------------

 
Abstracted from the addressing rules... Interesting.. A Multics system can support up to 16 MW of
memory. The memory is divided across the SCUs. One and only one SCU can have a base addres of 0. This
SCU is called the bootload SCU.

"During bootload, Multics requires a contiguous section of memory beginning at
 absolute address 0 and sufficently large to contain all routines and data
 structures used durng the first phase of Multics initialiation (i.e. collection 1).
 The size of the section required varies among Multics release, and it also
 depends on the size of the SST segment, which is dependent on the parameters
 specified by the tite on the sst config card. ... However
 512 KW is adequate for all circumstancces. There can be no "holes" in memory
 within this region. Beyond this region, "holes" can exist in memory."


 */

/*
 * From AN70-1 May84
 *
 * SCU ADDRESSING
 *
 *       There are three ways in which an SCU is addressed.  In the 
 * normal mode of operation (memory reading and writing), an active
 * unit (IOM or CPU) translates an absolute address into a memory
 * port (on it) and a relative memory address within the memory
 * described by the mempry port. The active module sends the
 * address to the SCU on the proper memory port. If the active
 * module is enabled by the port enable mask in the referenced SCU,
 * the SCU will take the address given to it and provide the
 * necessary memory access.
 *
 *      The other two ways pertain to reading/setting control
 * registers in the SCU itself. For each of these, it is still
 * necessary to specify somehow the memory port on the CPU whose SCU
 * registers are desired. For the rmcm, cmcm and smic intructions,
 * this consists of providing a virtual address to the processor for
 * which bits 1 and 2 are the memory port desired.
 *
 *      The rscr and sscr instructions, though key off the final
 * absoulte address to determine the SCI (or SCU store unit)
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
 * though, ths has no particalar impact in Multics software. All
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
 * executing inhihited code (instructions with the inhibit bit set).
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
 * THe low order three bits determines the IOM and IOM channel 
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
 * bit in the <interrupy number>th memory location in the interrupt
 * mask word (IMW) array in memory. It then generates a word with
 * the <interrupt number>th bit set and sends this to the bootload
 * SCU with the SC (set execute cells) SCU command. This sets the
 * execute interrupt cell register in the SCU and sends an XIP
 * (execute interrupt present) signal to various processors
 * connected to the SCU. (The details of this are covered in the
 * next scetion.) One of the processors (the first to get to it)
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
 *       The mechanism for determing which processors are candidates
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
 * and a set of ports to which it is assigned (8 bits). Whan a bit
 * is set in the execute  interrupt sells register, the SCU ands this
 * bit with the corresponding bit in each of the execute interrupt
 * mask registers. If the corresponding bit of execute interrupt
 * mask register A, for example, is on, the SCU then looks at the A
 * interrupt mask assignment register. If this register is not
 * assigned (enable), no further action takes place in regards to 
 * the A registers. (The B registers are still considered) (in
 * parallel, by the way).) If the register is assigned (enabled)
 * then interrupts will be send to all prots (prcoessors) whose
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
 * are eligble to receive interrupts. Other processors need not
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
 * obtained with the rmcm of the rscr instruction and set with the
 * smcm or the sscr instruction. The rmcm and smcm instruction only
 * work if the processor making the request has a mask register
 * assigned to it. If not, rmcm returns zero (no interrupt are
 * enabled to it) and a smcm is ignored (actually to port mask
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

// ============================================================================

 //#include "hw6180.h"
#include "dps8.h"
#include <sys/time.h>

static t_stat scu_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat scu_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat scu_show_config(FILE *st, UNIT *uptr, int val, void *desc);
static t_stat scu_set_config (UNIT * uptr, int32 value, char * cptr, void * desc);

//#define N_SCU_UNITS_MAX 4
#define N_SCU_UNITS_MAX 2 // DPS 8M only supports two SCUs

#define N_SCU_UNITS 1 // Default
UNIT scu_unit [N_SCU_UNITS_MAX] =
  {
    { UDATA(NULL, 0, 0) },
    { UDATA(NULL, 0, 0) }
  };

#define UNIT_NUM(uptr) ((uptr) - scu_unit)

static MTAB scu_mod [] =
  {
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      scu_set_config,         /* validation routine */
      scu_show_config, /* display routine */
      NULL          /* value descriptor */
    },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      scu_set_nunits, /* validation routine */
      scu_show_nunits, /* display routine */
      "Number of SCU units in the system" /* value descriptor */
    },
    {
      0
    }
  };


static t_stat scu_reset (DEVICE *dptr);

static DEBTAB scu_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

DEVICE scu_dev = {
    "SCU",       /* name */
    scu_unit,    /* units */
    NULL,     /* registers */
    scu_mod,     /* modifiers */
    N_SCU_UNITS, /* #units */
    10,          /* address radix */
    8,           /* address width */
    1,           /* address increment */
    8,           /* data radix */
    8,           /* data width */
    NULL,        /* examine routine */
    NULL,        /* deposit routine */
    &scu_reset,  /* reset routine */
    NULL,    /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    DEV_DEBUG,   /* flags */
    0,           /* debug control flags */
    scu_dt,           /* debug flag names */
    NULL,        /* memory size change */
    NULL         /* logical name */
};

#define N_SCU_PORTS 8
#define N_ASSIGNMENTS 2

enum { MODE_MANUAL = 0, MODE_PROGRAM = 1 };

// Hardware configuration switches

static struct config_switches
  {
    int mode; // program or manual
    int port_enable [N_SCU_PORTS];  // enable/disable
    int mask_enable [N_ASSIGNMENTS]; // enable/disable
    int mask_assignment [N_ASSIGNMENTS]; // assigned port number
  } config_switches [N_SCU_UNITS_MAX];

// System Controller

typedef struct {
    // Note that SCUs had no switches to designate SCU 'A' or 'B', etc.
    // Instead, SCU "A" is the one with base address switches set for 01400,
    // SCU "B" is the SCU with base address switches set to 02000, etc.
    // uint mem_base; // zero on boot scu
    // mode reg: mostly not stored here; returned by scu_get_mode_register()
    // int mode; // program/manual; if 1, sscr instruction can set some fields

    // CPU/IOM connectivity; designated 0..7
    // [CAC] really CPU/SCU and SCU/IOM connectivity
    struct {
        flag_t is_enabled;
        enum active_dev type; // type of connected device
        int idnum; // id # of connected dev, 0..7
        int dev_port; // which port on the connected device?
        DEVICE * devp; 
        UNIT * unitp;
    } ports[N_SCU_PORTS];
    
    
    /* The interrupt registers.
     Four exist; only two "A" and "B" used.
     Two parts -- execute interrupt mask register and a 9-bit mask assignment
     Currently missing: interrupt present
     */
    struct {
        flag_t avail; // Not physical. Does mask really exist?
        // Part 1 -- the execute interrupt mask register
        uint32 exec_intr_mask; // 32 bits, one for each intr or "cell"
        // Part 2 -- the interrupt mask assignment register -- 9 bits total
        struct {
            unsigned int raw; // 9 bits; raw mask; decoded below
            flag_t unassigned; // is it assigned to a port?
            // We only list one port -- Multics only allowed one port at a time
            // even though the SCU hardware would have allowed all 8 ports
            uint port; // port to which mask is assigned (0..7)
        } mask_assign; // eima_data[4];
    } interrupts[N_ASSIGNMENTS]; // In the actual h/w there are 4 cells, but
                                 // Multics only uses 2. By clamping here
                                 // I hope that bugs will be easier to
                                 // detect (assuming that multics doesn't
                                 // ever touch the missing pair.
} scu_t;

static scu_t scu [N_SCU_UNITS_MAX];

static t_stat scu_reset (DEVICE *dptr)
  {
    memset (& scu, 0, sizeof (scu));

    // On reset, instantiate the config switch settings

    for (int scu_unit_num = 0; scu_unit_num < N_SCU_UNITS_MAX; scu_unit_num ++)
      {
        scu_t * up = scu + scu_unit_num;
        struct config_switches * sw = config_switches + scu_unit_num;

        for (int i = 0; i < N_SCU_PORTS; i ++)
          {
            up ->  ports [i] . is_enabled = sw -> port_enable [i]; // port is enabled
            up ->  ports [i] . type = ADEV_NONE;   // type of connected device
            up ->  ports [i] . idnum = 0;        // id # of connected dev, 0..7
            up ->  ports [i] . dev_port = 0;     // which port on the connected device?
         }
   
// CAC - These settings were reversed engineer from the code instead
// of from the documentation. In case of issues, try fixing these, not the
// code.


        for (int i = 0; i < N_ASSIGNMENTS; i ++)
          {
            // XXX [CAC] I am not really sure what 'avail' is about; it
            // may be meant to represent the configartion switch? 
            up ->  interrupts [i] . avail = 1;
            up ->  interrupts [i] . exec_intr_mask = 0;
            up ->  interrupts [i] . mask_assign . raw = 0 /* 0720 */;
            up ->  interrupts [i] . mask_assign . unassigned = 
              sw -> mask_enable [i] ? 0 : 1;
            up ->  interrupts [i] . mask_assign . port = 
              sw -> mask_assignment [i];

            if (sw -> mask_enable [i])
              {
                int port = sw -> mask_assignment [i];
                up -> ports [port] . type = ADEV_CPU;
              }
          }
      }
    return SCPE_OK;
  }


// Physical ports on the CPU
typedef struct {
    // The ports[] array should indicate which SCU each of the CPU's 8
    // ports are connected to.
    int ports[8]; // SCU connectivity; designated a..h
    int scu_port; // What port num are we connected to (same for all SCUs)
} cpu_ports_t;

#ifndef QUIET_UNUSED
static cpu_ports_t cpu_ports;
static int pima_parse_raw(int pima, const char *moi);
static int scu_get_mask(t_uint64 addr, int port);
#endif


// ============================================================================

#ifndef QUIET_UNUSED
static const char* adev2text(enum active_dev type)
{
    static char* types[] = { "", "CPU", "IOM" };
    return (type >= ARRAY_SIZE(types)) ? "" : types[type];
}
#endif

// ============================================================================

#ifndef QUIET_UNUSED
static int scu_hw_arg_check(const char *tag, t_uint64 addr, int port)
{
    // Sanity check args
    // Verify that HW could have received signal
    
    if (port < 0 || port > 7) {
        sim_debug (DBG_ERR, &scu_dev, "%s: Port %d from sscr is out of range 0..7\n", tag, port);
        cancel_run(STOP_BUG);
        return 2;
    }
    
#if 0
    return 0;
#else
    // Verify that HW could have received signal
    
    // port-no that rscr instr came in on
    // We only have one CPU, so...
    int rcv_port = cpu_ports.scu_port;
    
    if (rcv_port < 0 || rcv_port > 7)  {
        sim_debug (DBG_ERR, &scu_dev, "%s: CPU is not connected to any port.  Port %d does nto exist on the SCU.\n", tag, rcv_port);
        cancel_run(STOP_WARN);
        return 1;
    }
    
    int cpu_port = scu[ASSUME0].ports[rcv_port].dev_port;    // which port on the CPU?
    
    // Verify that HW could have received signal
    if (cpu_port < 0 || cpu_port > 7) {
        sim_debug (DBG_ERR, &scu_dev, "%s: Port %d is connected to nonsense port %d of CPU %d\n", tag, rcv_port, cpu_port, scu[ASSUME0].ports[rcv_port].idnum);
        cancel_run(STOP_WARN);
        return 1;
    }
    // TODO: Check that cpu_ports.ports[cpu_port] is this SCU
    return 0;
#endif
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_set_mask(t_uint64 addr, int port)
{
    // BUG: addr should determine which SCU is selected
    // Implements part of the sscr instruction -- functions y00[0-7]2x
    
    const char* moi = "SCU::setmask";
    if (scu_hw_arg_check(moi, addr, port) > 0)
        return 1;
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    // Find mask reg assigned to specified port
    int port_pima = 0;
#ifndef QUIET_UNUSED
    int cpu_pima = 0;
#endif
    int cpu_found = 0;
    int port_found = 0;
    for (int p = 0; p < ARRAY_SIZE(scu[ASSUME0].interrupts); ++p) {
        if (! scu[ASSUME0].interrupts[p].avail)
            continue;
        if (scu[ASSUME0].interrupts[p].mask_assign.unassigned)
            continue;
        if (scu[ASSUME0].interrupts[p].mask_assign.port == port) {
            port_pima = p;
            if (port != rcv_port)
              {
                sim_debug (DBG_DEBUG, &scu_dev, "%s: Found MASK %d assigned to %s on port %d\n", moi, p, adev2text(scu[ASSUME0].ports[port].type), port);
              }
            ++ port_found;
        }
        if (scu[ASSUME0].interrupts[p].mask_assign.port == rcv_port) {
#ifndef QUIET_UNUSED
            cpu_pima = p;
#endif
             sim_debug (DBG_DEBUG, &scu_dev, "%s: Found MASK %d assigned to invoking CPU on port %d\n", moi, p, rcv_port);
            ++ cpu_found;
        }
    }
    
    if (! cpu_found) {
        sim_debug (DBG_WARN, &scu_dev, "%s: No masks assigned to cpu on port %d\n", moi, rcv_port);
        fault_gen(FAULT_STR);
        return 1;
    }
    if (cpu_found > 1) {
        // Not legal for Multics
        sim_debug (DBG_WARN, &scu_dev, "%s: Multiple masks assigned to cpu on port %d\n", moi, rcv_port);
        cancel_run(STOP_WARN);
    }
    if (! port_found) {
        sim_debug (DBG_DEBUG, &scu_dev, "%s: No masks assigned to port %d\n", moi, port);
        return 0;
    }
    if (port_found > 1)
      {
        sim_debug (DBG_WARN, &scu_dev, "%s: Multiple masks assigned to port %d\n", moi, rcv_port);
      }
    
    if (port_pima > 1) {
        sim_debug (DBG_ERR, &scu_dev, "%s: Cannot write to masks other than zero and one: %d\n", moi, port_pima);
        cancel_run(STOP_BUG);
        return 1;
    }
    
    // See AN87
    scu[ASSUME0].interrupts[port_pima].exec_intr_mask = 0;
    scu[ASSUME0].interrupts[port_pima].exec_intr_mask |= (getbits36(reg_A, 0, 16) << 16);
    scu[ASSUME0].interrupts[port_pima].exec_intr_mask |= getbits36(reg_Q, 0, 16);
    //sim_debug (DBG_DEBUG, &scu_dev, "%s: PIMA %c: EI mask set to %s\n", moi, port_pima + 'A', bin2text(scu[ASSUME0].interrupts[port_pima].exec_intr_mask, 32));
    return 0;
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_set_cpu_mask(t_uint64 addr)
{
    // BUG: addr should determine which SCU is selected
    
    if (scu_hw_arg_check("smcm", addr, 0) > 0)
        return 1;
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    return scu_set_mask(addr, rcv_port);
}
#endif



// =============================================================================

#ifndef QUIET_UNUSED
static int scu_get_cpu_mask(t_uint64 addr)
{
    // BUG: addr should determine which SCU is selected
    
    const char *moi = "SCU::rmcm";
    
    if (scu_hw_arg_check(moi, addr, 0) > 0)
        return 1;
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    reg_A = 0;
    reg_Q = 0;
    return scu_get_mask(addr, rcv_port);
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_get_mode_register(t_uint64 addr)
{
    // Implements part of the rscr instruction -- function  y0000x
    // BUG: addr should determine which SCU is selected
    
#if 1
    // BUG: is it really OK for all ports to be disabled?
    if (scu_hw_arg_check("get-mode-register", addr, 0) != 0)
      {
        sim_debug (DBG_ERR, &scu_dev, "get-mode-register: But proceeding anyway");
      }
#endif
    
#ifndef QUIET_UNUSED
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
#endif
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    
    // See scr.incl.pl1 and AN87 page 2-2
    
    // Note that rscr 0001X can only report an SC with a memory sizes of up
    // to 256 K-words, but rscr 0001X can report an SCU with up to 4MW.  So,
    // we identify ourselves as an 4MW SCU.
    
    reg_A = 0;  // first 50 bits are padding
    reg_Q = 0;
    reg_Q |= setbits36(reg_Q, 50-36, 4, 2); // id for a 4MW SCU (level 66 SCU)
    /*
     remaining bits are only for T&D test and diagnostics
     */
    // reg_Q |= setbits36(reg_Q, 54-36, 2, 0);  // TS strobe normal timing
    // reg_Q |= setbits36(reg_Q, 64-36, 2, 0);  // both 00b and 10b mean normal voltage
    // reg_Q |= setbits36(reg_Q, 70-36, 1, 0);  // SGR accepted
    
    return 0;
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_get_config_switches(t_uint64 addr)
{
    // Implements part of the rscr instruction -- function y0001x
    // Returns info appropriate to a 4MW SCU
    // BUG: addr should determine which SCU is selected
    
    const char *tag = "get-config-switches";
    const char *moi = "SCU::get-config-switches";
#if 1
    if (scu_hw_arg_check(tag, addr, 0) != 0)
      {
        sim_debug (DBG_ERR, &scu_dev, "scu_get_config_switches: But proceeding anyway");
      }
#endif
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    
    // See scr.incl.pl1
    reg_A = 0;
    // interrupt mask A port assignment
    reg_A = setbits36(reg_A, 0, 9, scu[ASSUME0].interrupts[0].mask_assign.raw);
    // We have 4 banks and can have 4M-words, so report banks size 1024K-words
    reg_A = setbits36(reg_A, 9, 3, 5);  // size of lower store -- 2^(5+5) == 1024 K-words
    reg_A = setbits36(reg_A, 12, 4, 017);   // all four stores online
    reg_A = setbits36(reg_A, 16, 4, rcv_port);  // requester's port #
    reg_A = setbits36(reg_A, 21, 1, scu[ASSUME0].mode);  // programmable
    reg_A = setbits36(reg_A, 22, 1, 0); // non-existent address logic enabled
    reg_A = setbits36(reg_A, 23, 7, 0); // nea size
    reg_A = setbits36(reg_A, 30, 1, 1); // internally interlaced
    reg_A = setbits36(reg_A, 31, 1, 0); // store B is lower?
    for (int i = 0; i < 4; ++ i) {
        int pima = 0;
        int port = i + pima * 4;
        int enabled = scu[ASSUME0].ports[port].is_enabled;
        reg_A = setbits36(reg_A, 32+i, 1, enabled); // enable masks for ports 0-3
        if (enabled)
          {
            sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is enabled, it points to port %d on %s %c.\n", moi, port, scu[ASSUME0].ports[port].dev_port, adev2text(scu[ASSUME0].ports[port].type), scu[ASSUME0].ports[port].idnum + 'A');
          }
        else
          {
            sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is disabled.\n", moi, port);
          }
    }
    
    reg_Q = 0;
    reg_Q = setbits36(reg_Q, 0, 9, scu[ASSUME0].interrupts[1].mask_assign.raw);
    reg_Q = setbits36(reg_Q, 57-36, 7, 0);  // cyclic port priority switches; BUG
    for (int i = 0; i < 4; ++ i) {
        int pima = 1;
        int port = i + pima * 4;
        int enabled = scu[ASSUME0].ports[port].is_enabled;
        reg_Q = setbits36(reg_Q, 68-36+i, 1, enabled);  // enable masks for ports 4-7
        if (enabled)
          {
            sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is enabled, it points to port %d on %s %c.\n", moi, port, scu[ASSUME0].ports[port].dev_port, adev2text(scu[ASSUME0].ports[port].type), scu[ASSUME0].ports[port].idnum + 'A');
          }
        else
          {
            sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is disabled.\n", moi, port);
          }
    }
    
    return 0;
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_set_config_switches(t_uint64 addr)
{
    // Implements part of the sscr instruction, function y0001x
    // Only valid for a 4MW SCU
    // BUG: addr should determine which SCU is selected
    const char* moi = "SCU::set-config-switches";
    
// XXX ASSUME0
    if (scu_hw_arg_check(moi, addr, 0) > 0)
        return 1;
#ifndef QUIET_UNUSED
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
#endif
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    
    // See scs.incl.pl1
    
    if (scu[ASSUME0].mode != MODE_PROGRAM) {
        sim_debug (DBG_WARN, &scu_dev, "%s: SCU mode is 'MANUAL', not 'PROGRAM' -- sscr not allowed to set switches.\n", moi);
// XXX [CAC] I think I read somewhere that this should generate a STORE FAULT
        cancel_run(STOP_BUG);
    }
    
    // get settings from reg A
    
    int pima = 0;
    scu[ASSUME0].interrupts[pima].mask_assign.raw = getbits36(reg_A, 0, 9);
    pima_parse_raw(pima, moi);
    
    // BUG: We don't allow changes to many of the settings
    //getbits36(reg_A, 9, 3);   // size of lower store -- 2^(5+5) == 1024 K-words
    //getbits36(reg_A, 12, 4);  // all four stores online
    // requester's port cannot be set from bits 16..20 of A
    // scu[ASSUME0].mode = getbits36(reg_A, 21, 1);  // programmable flag; not for sscr
    // getbits36(reg_A, 22, 1); // non-existent address logic enabled
    // getbits36(reg_A, 23, 7); // nea size
    // getbits36(reg_A, 30, 1); // internally interlaced
    // getbits36(reg_A, 31, 1); // store B is lower?
    // check enable masks -- see rscr reg a 32..36
    // Set enable masks for ports 0-3
    for (int i = 0; i < 4; ++ i)
      {
        int enabled = getbits36(reg_A, 32+i, 1);
        int port = i+pima*4;
        if (! enabled)
          {
            if (! scu[ASSUME0].ports[port].is_enabled)
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d still disabled.\n", moi, port);
              }
            else
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d now disabled.\n", moi, port);
              }
            scu[ASSUME0].ports[port].is_enabled = 0;
          }
        else
          {
            if (!scu[ASSUME0].ports[port].is_enabled)
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is now enabled; it points to port %d on %s %c.\n", moi, port, scu[ASSUME0].ports[port].dev_port, adev2text(scu[ASSUME0].ports[port].type), scu[ASSUME0].ports[port].idnum + 'A');
                scu[ASSUME0].ports[port].is_enabled = 1;
              }
            else
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is still enabled; it points to port %d on %s %c.\n", moi, port, scu[ASSUME0].ports[port].dev_port, adev2text(scu[ASSUME0].ports[port].type), scu[ASSUME0].ports[port].idnum + 'A');
              }
          }
      }
    
    
    // get settings from reg Q
    
    pima = 1;
    scu[ASSUME0].interrupts[pima].mask_assign.raw = getbits36(reg_Q, 0, 9);
    pima_parse_raw(pima, moi);
    
    // BUG  getbits36(reg_Q, 57-36, 7, 0);  // cyclic port priority switches; BUG
    for (int i = 0; i < 4; ++ i)
      {
        int enabled = getbits36(reg_Q, 32+i, 1);
        int port = i+pima*4;
        if (! enabled)
          {
            if (!scu[ASSUME0].ports[port].is_enabled)
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d still disabled.\n", moi, port);
              }
            else
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d now disabled.\n", moi, port);
              }
            scu[ASSUME0].ports[port].is_enabled = 0;
          }
        else
          {
            if (!scu[ASSUME0].ports[port].is_enabled)
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is now enabled; it points to port %d on %s %c.\n", moi, port, scu[ASSUME0].ports[port].dev_port, adev2text(scu[ASSUME0].ports[port].type), scu[ASSUME0].ports[port].idnum + 'A');
                scu[ASSUME0].ports[port].is_enabled = 1;
              }
            else
              {
                sim_debug (DBG_INFO, &scu_dev, "%s: Port %d is still enabled; it points to port %d on %s %c.\n", moi, port, scu[ASSUME0].ports[port].dev_port, adev2text(scu[ASSUME0].ports[port].type), scu[ASSUME0].ports[port].idnum + 'A');
              }
          }
      }
    
    sim_debug (DBG_NOTIFY, &scu_dev, "%s: Doing BUG check.\n", moi);
    int ret = 0;
    t_uint64 a = reg_A;
    t_uint64 q = reg_Q;
    scu_get_config_switches(addr);
    if (a == reg_A) {
        sim_debug (DBG_NOTIFY, &scu_dev, "%s: we handled reg A correctly\n", moi);
    } else {
        sim_debug (DBG_ERR, &scu_dev, "scu_get_config_switches: sscr specified reg A %012llo\n", a);
        sim_debug (DBG_ERR, &scu_dev, "scu_get_config_switches: we used              %012llo\n", reg_A);
        reg_A = a;
        ret = 1;
    }
    if (q == reg_Q) {
        sim_debug (DBG_NOTIFY, &scu_dev, "%s: we handled reg Q correctly\n", moi);
    } else {
        sim_debug (DBG_ERR, &scu_dev, "scu_get_config_switches: sscr specified reg Q %012llo\n", q);
        sim_debug (DBG_ERR, &scu_dev, "scu_get_config_switches: we used              %012llo\n", reg_Q);
        reg_Q = q;
        ret = 1;
    }
    
    if (ret == 0)
      {
        sim_debug (DBG_WARN, &scu_dev, "%s: Unfinished but OK.\n", moi);
      }
    else
      {
        sim_debug (DBG_ERR, &scu_dev, "scu_get_config_switches: Unfinished and incorrect.\n");
        cancel_run(STOP_BUG);
      }
    return ret;
  }
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_get_mask(t_uint64 addr, int port)
{
    // BUG: addr should determine which SCU is selected
    // Implements part of the rscr instruction, function y00[0-7]2x
    const char *moi = "SCU::get-mask";
    
    if (scu_hw_arg_check("getmask", addr, port) > 0)
        return 1;
#ifndef QUIET_UNUSED
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
#endif
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    // Find which of the 4 masks are assigned to the specified port
    // Unlike sscr scu_set_mask, we don't care about the CPU's port
    int port_pima = 0;
    int port_found = 0;
    for (int p = 0; p < ARRAY_SIZE(scu[ASSUME0].interrupts); ++p) {
        if (! scu[ASSUME0].interrupts[p].avail)
            continue;
        if (scu[ASSUME0].interrupts[p].mask_assign.unassigned)
            continue;
        if (scu[ASSUME0].interrupts[p].mask_assign.port == port) {
            port_pima = p;
            ++ port_found;
        }
    }
    
    if (! port_found) {
        // TODO: AL-39 doesn't say what to do if the port has no mask
        // assigned.   However, rmcm zeros register A and Q for a
        // similar case...
        reg_A = 0;
        reg_Q = 0;
        sim_debug (DBG_WARN, &scu_dev, "%s: No masks assigned to port %d\n", moi, port);
        return 0;
    }
    if (port_found > 1)
      {
        sim_debug (DBG_WARN, &scu_dev, "%s: Multiple masks assigned to port %d\n", moi, port);
      }
    
    sim_debug (DBG_INFO, &scu_dev, "%s: Found MASK %d assigned to port %d. Ports enabled on mask are:", moi, port_pima, port);
    // See AN87
    reg_A = setbits36(0, 0, 16, scu[ASSUME0].interrupts[port_pima].exec_intr_mask >> 16);
    unsigned mask = 0;
    for (int i = 0; i < 4; ++ i) {
        int enabled = scu[ASSUME0].ports[i].is_enabled;
        mask <<= 1;
        mask |= enabled;
        if (enabled)
          {
            sim_debug (DBG_INFO, &scu_dev, " %d", i);
          }
    }
    reg_A |= mask;
    
    reg_Q = setbits36(0, 0, 16, scu[ASSUME0].interrupts[port_pima].exec_intr_mask & MASKBITS(16));
    mask = 0;
    for (int i = 0; i < 4; ++ i) {
        int enabled = scu[ASSUME0].ports[i+4].is_enabled;
        if (enabled)
          {
            sim_debug (DBG_INFO, &scu_dev, " %d", i + 4);
          }
        mask <<= 1;
        mask |= enabled;
    }
    reg_Q |= mask;
    if ((reg_A & 017) == 0 && mask == 0)
      {
        sim_debug (DBG_INFO, &scu_dev, "none");
      }
    sim_debug (DBG_INFO, &scu_dev, "\n");
    
    return 0;
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_get_calendar(t_uint64 addr)
{
    // 52 bit clock
    // microseconds since 0000 GMT, Jan 1, 1901 // not 1900 which was a per century exception to leap years
    
    // BUG: addr should determine which SCU is selected
    
    if (scu_hw_arg_check("get-calendar", addr, 0) != 0)
        return 1;
#ifndef QUIET_UNUSED
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
#endif
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    
    t_uint64 now;
    if (sys_opts.clock_speed != 0) {
        // Clock starts at an arbitrary date and ticks at a rate of
        // approximaetly sys_opts.clock_speed instructions per second.
        
        t_uint64 i_cycles = sys_stats.total_cycles * 2 / 3; // fetch, exec, exec
        t_uint64 elapsed = i_cycles * 1000000 / sys_opts.clock_speed;
        
        // returned time is since 2009...
        now = (t_uint64) (2009 - 1901) * 365 * 24 * 3600;
        now = now * 1000000 + elapsed;
    } else {
        // Use real time
        
        uint32 msec = sim_os_msec();
        t_uint64 seconds = msec / 1000;
        msec -= seconds * 1000;
        seconds += (t_uint64) 69 * 365 * 24 * 3600;     // UNIX epoch is 1970, but Multics epoch is 1901
        now = seconds * 1000000 + msec * 1000;
    }
    
    reg_Q = now & MASK36;
    reg_A = (now >> 36) & MASK36;
    
    return 0;
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int scu_cioc(t_uint64 addr)
{
    // BUG: addr should determine which SCU is selected
    
    if (scu_hw_arg_check("cioc", addr, 0) != 0)
        return 1;
#ifndef QUIET_UNUSED
    // XXX these needs to be indexed by scu_unit_num
// XXX ASSUME0
    int rcv_port = cpu_ports.scu_port;  // port-no that instr came in on
#endif
// XXX ASSUME0
    // int cpu_no = scu[ASSUME0].ports[rcv_port].idnum;  // CPU 0->'A', 1->'B', etc
// XXX ASSUME0
    // int cpu_port = scu[ASSUME0].ports[rcv_port].devnum    // which port on the CPU?
    
    t_uint64 word;
    int ret;
    //if ((ret = fetch_word(TPR.CA, &word)) != 0) {
    if ((ret = Read(NULL, TPR.CA, &word, DataRead, 0)) != 0) { // HWR
        cancel_run(STOP_BUG);
        return ret;
    }
    int port = word & 7;
    sim_debug (DBG_INFO, &scu_dev, "\n");
    sim_debug (DBG_DEBUG, &scu_dev, "scu_cioc: Contents of %llo are: %llo => port %d\n", addr, word, port);
    // OK ... what's a connect signal (as opposed to an interrupt?
    // [CAC] Connects come from a CPU, interrupts from peripherals???
    // A connect signal does the following (AN70, 8-7):
    //  IOM target: connect strobe to IOM
    //  CPU target: connect fault
    
    // todo: check if enabled & not masked
    
    static int n_cioc = 0;
    {
        //static int n_cioc = 0;
        sim_debug (DBG_NOTIFY, &scu_dev, "scu_cioc: CIOC # %d\n", ++ n_cioc);
        if (n_cioc >= 306) {        // BUG: temp hack to increase debug level
            /* XXX Evil: ++ cpu_dev.dctrl */;
        }
    }
    sim_debug (DBG_DEBUG, &scu_dev, "scu_cioc: Connect sent to port %d => %d\n", port, scu[ASSUME0].ports[port]);
    
    // we only have one IOM, so signal it
    // todo: sanity check port connections
    if (sys_opts.iom_times.connect < 0)
        iom_interrupt(ASSUME0);
    else {
        sim_debug (DBG_INFO, &scu_dev, "scu_cioc: Queuing an IOM in %d cycles (for the connect channel)\n", sys_opts.iom_times.connect);
        if (sim_activate(&iom_dev.units[0], sys_opts.iom_times.connect) != SCPE_OK) {
            cancel_run(STOP_UNK);
            ret = 1;
        }
    }
    
    if (n_cioc >= 306) {
        /* XXX Evil: -- cpu_dev.dctrl */;
    }
    
    return ret;
}
#endif

// =============================================================================

#ifndef QUIET_UNUSED
static int pima_parse_raw(int pima, const char *moi)
{
    char pima_name = (pima == 0) ? 'A' : 'B';
    flag_t unassigned = scu[ASSUME0].interrupts[pima].mask_assign.raw & 1;
    if (unassigned) {
        scu[ASSUME0].interrupts[pima].mask_assign.unassigned = 1;
        sim_debug (DBG_NOTIFY, &scu_dev, "%s: Unassigning MASK %c.\n", moi, pima_name);
    }
    int found = 0;
    for (int p = 0; p < 8; ++p)
        if (((1<<(8-p) & scu[ASSUME0].interrupts[pima].mask_assign.raw)) != 0) {
            ++ found;
            scu[ASSUME0].interrupts[pima].mask_assign.port = p;
            sim_debug (DBG_NOTIFY, &scu_dev, "%s: Assigning port %d to MASK %c.\n", moi, p, pima_name);
        }
    if (unassigned) {
        if (found != 0) {
            sim_debug (DBG_WARN, &scu_dev, "%s: %d ports enabled for unassigned MASK %c: %#o\n", moi, found, pima_name, scu[ASSUME0].interrupts[pima].mask_assign.raw);
            cancel_run(STOP_WARN);
        }
        return found != 0;
    } else {
        scu[ASSUME0].interrupts[pima].mask_assign.unassigned = found == 0;
        if (found != 1) {
            sim_debug (DBG_WARN, &scu_dev, "%s: d ports enabled for MASK %c: %#o\n", moi, found, pima_name, scu[ASSUME0].interrupts[pima].mask_assign.raw);
            sim_debug (DBG_WARN, &scu_dev, "%s: Auto breakpoint.\n", moi);
            cancel_run(STOP_WARN);
        }
        return found != 1;
    }
}
#endif

// =============================================================================

int scu_set_interrupt(int inum)
{
    const char* moi = "SCU::interrupt";
    
    if (inum < 0 || inum > 31) {
        sim_debug (DBG_WARN, &scu_dev, "%s: Bad interrupt number %d\n", moi, inum);
        cancel_run(STOP_WARN);
        return 1;
    }
    
    for (int pima = 0; pima < ARRAY_SIZE(scu[ASSUME0].interrupts); ++pima) {
        if (! scu[ASSUME0].interrupts[pima].avail) {
            sim_debug (DBG_DEBUG, &scu_dev, "%s: PIMA %c: Mask is not available.\n",
                    moi, pima + 'A');
            continue;
        }
        if (scu[ASSUME0].interrupts[pima].mask_assign.unassigned) {
            sim_debug (DBG_DEBUG, &scu_dev, "%s: PIMA %c: Mask is not assigned.\n",
                    moi, pima + 'A');
            continue;
        }
        uint mask = scu[ASSUME0].interrupts[pima].exec_intr_mask;
        int port = scu[ASSUME0].interrupts[pima].mask_assign.port;
        if ((mask & (1<<inum)) == 0) {
            sim_debug (DBG_INFO, &scu_dev, "%s: PIMA %c: Port %d is masked against interrupts.\n",
                    moi, 'A' + pima, port);
            sim_debug (DBG_DEBUG, &scu_dev, "%s: Mask: %s\n", moi, bin2text(mask, 32));
        } else {
            if (scu[ASSUME0].ports[port].type != ADEV_CPU)
                sim_debug (DBG_WARN, &scu_dev, "%s: PIMA %c: Port %d should receive interrupt %d, but the device is not a cpu.\n",
                        moi, 'A' + pima, port, inum);
            else {
                //extern events_t events; // BUG: put in hdr file or hide behind an access function
                sim_debug (DBG_NOTIFY, &scu_dev, "%s: PIMA %c: Port %d (which is connected to port %d of CPU %d will receive interrupt %d.\n",
                        moi,
                       'A' + pima, port, scu[ASSUME0].ports[port].dev_port,
                        scu[ASSUME0].ports[port].idnum, inum);
                events.any = 1;
                events.int_pending = 1;
                events.interrupts[inum] = 1;
            }
        }
    }
    
    return 0;
}

// ============================================================================

static t_stat scu_show_nunits (FILE *st, UNIT *uptr, int val, void *desc)
  {
    sim_printf("Number of SCU units in system is %d\n", scu_dev . numunits);
    return SCPE_OK;
  }

static t_stat scu_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_SCU_UNITS_MAX)
      return SCPE_ARG;
    if (n > 2)
      sim_printf ("Warning: Multics supports 2 SCUs maximum\n");
    scu_dev . numunits = n;
    return SCPE_OK;
  }

static t_stat scu_show_config(FILE *st, UNIT *uptr, int val, void *desc)
{
    static char * map [N_SCU_PORTS] = {"0", "1", "2", "3", "4", "5", "6", "7" };
    int scu_unit_num = UNIT_NUM (uptr);
    if (scu_unit_num < 0 || scu_unit_num >= scu_dev . numunits)
      {
        sim_debug (DBG_ERR, & scu_dev, "scu_show_config: Invalid unit number %d\n", scu_unit_num);
        sim_printf ("error: invalid unit number %d\n", scu_unit_num);
        return SCPE_ARG;
      }

    sim_printf ("SCU unit number %d\n", scu_unit_num);
    struct config_switches * sw = config_switches + scu_unit_num;

    char * mode = "<out of range>";
    switch (sw -> mode)
      {
        case MODE_PROGRAM:
          mode = "Program";
          break;
        case MODE_MANUAL:
          mode = "Manual";
          break;
      }

    sim_printf("Mode:                     %s\n", mode);
    sim_printf("Port Enable:             ");
    for (int i = 0; i < N_SCU_PORTS; i ++)
      sim_printf (" %3o", sw -> port_enable [i]);
    sim_printf ("\n");
    for (int i = 0; i < N_ASSIGNMENTS; i ++)
      {
        sim_printf("Mask %c:                   %s\n", 'A' + i, sw -> mask_enable [i] ? (map [sw -> mask_assignment [i]]) : "Off");
      }
    return SCPE_OK;
  }

//
// set scu0 config=<blah> [;<blah>]
//
//    blah =
//           mode=  manual | program
//           mask[A|B] = off | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7
//           portN = enable | disable
//

static t_stat scu_set_config (UNIT * uptr, int32 value, char * cptr, void * desc)
  {
// XXX Minor bug; this code doesn't check for trailing garbage

    int scu_unit_num = UNIT_NUM (uptr);
    if (scu_unit_num < 0 || scu_unit_num >= scu_dev . numunits)
      {
        sim_debug (DBG_ERR, & scu_dev, "scu_set_config: Invalid unit number %d\n", scu_unit_num);
        sim_printf ("error: scu_set_config: invalid unit number %d\n", scu_unit_num);
        return SCPE_ARG;
      }

    struct config_switches * sw = config_switches + scu_unit_num;

    char * copy = strdup (cptr);
    char * start = copy;
    char * statement_save = NULL;
    for (;;) // process statements
      {
        char * statement;
        statement = strtok_r (start, ";", & statement_save);
        start = NULL;
        if (! statement)
          break;

        // process statement

        // extract name
        char * name_start = statement;
        char * name_save = NULL;
        char * name;
        name = strtok_r (name_start, "=", & name_save);
        if (! name)
          {
            sim_debug (DBG_ERR, & scu_dev, "scu_set_config: can't parse name\n");
            sim_printf ("error: scu_set_config: can't parse name\n");
            break;
          }

        // extract value
        char * value;
        value = strtok_r (NULL, "", & name_save);
        if (! value)
          {
            sim_debug (DBG_ERR, & scu_dev, "scu_set_config: can't parse value\n");
            sim_printf ("error: scu_set_config: can't parse value\n");
            break;
          }

        if (strcmp (name, "MODE") == 0)
          {
            if (strcmp (value, "PROGRAM") == 0)
              sw -> mode = 1;
            else if (strcmp (value, "MANUAL") == 0)
              sw -> mode = 0;
            else
              {
                sim_debug (DBG_ERR, & scu_dev, "scu_set_config: Invalid MODE value <%s>\n", value);
                sim_printf ("error: scu_set_config: invalid MODE value <%s>\n", value);
                break;
              }
          }
        else if (strmask (name, "MASK?") && 
                 (name [4] == 'A' || name [4] == 'B'))
          {
            if (strlen (value) == 0)
              {
                sim_debug (DBG_ERR, & scu_dev, "scu_set_config: missing value\n");
                sim_printf ("error: scu_set_config: missing value\n");
                break;
              }
            if (strcmp (value, "OFF") == 0)
              {
                sw -> mask_enable [name [4] - 'A'] = false;
              }
            else
              {
                char * endptr;
                long int n = strtol (value, & endptr, 0);
                if (* endptr)
                  {
                    sim_debug (DBG_ERR, & scu_dev, "scu_set_config: can't parse number <%s>\n", value);
                    sim_printf ("error: scu_set_config: can't parse number <%s>\n", value);
                    break;
                  } 
                if (n < 0 || n >= N_SCU_PORTS)
                  {
                    sim_debug (DBG_ERR, & scu_dev, "scu_set_config: MASK value out of range: %ld\n", n);
                    sim_printf ("error: scu_set_config: MASK value out of range: %ld\n", n);
                    break;
                  } 

                sw -> mask_enable [name [4] - 'A'] = true;
                sw -> mask_assignment [name [4] - 'A'] = n;
    
              }
          }
        else if (strmask (name, "PORT?") && 
                 (name [4] >= '0' || name [4] <= '7'))
          {
            if (strcmp (value, "ENABLE") == 0)
              sw -> port_enable [name [4] - '0'] = 1;
            else if (strcmp (value, "DISABLE") == 0)
              sw -> port_enable [name [4] - '0'] = 0;
            else
              {
                sim_debug (DBG_ERR, & scu_dev, "scu_set_config: Invalid PORT value <%s>\n", value);
                sim_printf ("error: scu_set_config: invalid PORT value <%s>\n", value);
                break;
              }
          }
        else
          {
            sim_debug (DBG_ERR, & scu_dev, "scu_set_config: Invalid switch name <%s>\n", name);
            sim_printf ("error: scu_set_config: invalid switch name <%s>\n", name);
            break;
          }
      } // process statements
    free (copy);

    return SCPE_OK;
  }

static struct
  {
    int cpu_unit_num;
    int cpu_port_num;
  } cables_from_cpus [N_SCU_UNITS_MAX] [N_SCU_PORTS];

t_stat cable_scu (int scu_unit_num, int scu_port_num, int cpu_unit_num, int cpu_port_num)
  {
    if (scu_unit_num < 0 || scu_unit_num >= scu_dev . numunits)
      {
        sim_debug (DBG_ERR, & scu_dev, "cable_scu: scu_unit_num out of range <%d>\n", scu_unit_num);
        sim_printf ("cable_scu: scu_unit_num out of range <%d>\n", scu_unit_num);
        return SCPE_ARG;
      }

    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
      {
        sim_debug (DBG_ERR, & scu_dev, "cable_scu: scu_port_num out of range <%d>\n", scu_unit_num);
        sim_printf ("cable_scu: scu_port_num out of range <%d>\n", scu_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_cpus [scu_unit_num] [scu_port_num] . cpu_unit_num != -1)
      {
        sim_debug (DBG_ERR, & tape_dev, "cable_scu: port in use\n");
        sim_printf ("cable_scu: port in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_cpu (cpu_unit_num, cpu_port_num, scu_unit_num, scu_port_num);
    if (rc)
      return rc;

    cables_from_cpus [scu_unit_num] [scu_port_num] . cpu_unit_num = cpu_unit_num;
    cables_from_cpus [scu_unit_num] [scu_port_num] . cpu_port_num = cpu_port_num;

    return SCPE_OK;
  }

// A iom is trying to attach a cable to us
//  to my port scu_unit_num, scu_port_num
//  from it's port iom_unit_num, iom_port_num
//

t_stat cable_to_scu (int scu_unit_num, int scu_port_num, int iom_unit_num, int iom_port_num)
  {
    if (scu_unit_num < 0 || scu_unit_num >= scu_dev . numunits)
      {
        sim_debug (DBG_ERR, & scu_dev, "cable_to_scu: scu_unit_num out of range <%d>\n", scu_unit_num);
        sim_printf ("cable_to_scu: scu_unit_num out of range <%d>\n", scu_unit_num);
        return SCPE_ARG;
      }

    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
      {
        sim_debug (DBG_ERR, & scu_dev, "cable_to_scu: scu_port_num out of range <%d>\n", scu_port_num);
        sim_printf ("cable_to_scu: scu_port_num out of range <%d>\n", scu_port_num);
        return SCPE_ARG;
      }

    if (scu [scu_unit_num] . ports [scu_port_num] . type != ADEV_NONE)
      {
        sim_debug (DBG_ERR, & scu_dev, "cable_to_scu: socket in use\n");
        sim_printf ("cable_to_scu: socket in use\n");
        return SCPE_ARG;
      }

    DEVICE * devp = & iom_dev;
    UNIT * unitp = & iom_unit [iom_unit_num];
     
    scu [scu_unit_num] . ports [scu_port_num] . type = ADEV_IOM;
    scu [scu_unit_num] . ports [scu_port_num] . dev_port = iom_unit_num;

    scu [scu_unit_num] . ports [scu_port_num] . devp = devp;
    scu [scu_unit_num] . ports [scu_port_num] . unitp  = unitp;

    unitp -> u3 = scu_port_num;
    unitp -> u4 = 0;
    unitp -> u5 = scu_unit_num;

    return SCPE_OK;
  }

void scu_init (void)
  {
    // One time only initialiations

    for (int u = 0; u < N_SCU_UNITS_MAX; u ++)
      for (int p = 0; p < N_SCU_PORTS; p ++)
        cables_from_cpus [u] [p] . cpu_unit_num = -1; // not connected
  }

