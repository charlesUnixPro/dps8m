//
// \file dps8_iom.c
// \project dps8
// \date 9/21/12
//  Adapted by Harry Reed on 9/21/12.
//  27Nov13 CAC - Reference document is
//    431239854 600B IOM Spec Jul75
//    This is a 600B IOM emulator. 
//

// XXX Use this when we assume there is only a single unit
#define ASSUME0 0
#define ASSUME_CPU_0 0

// iom.c -- emulation of an I/O Multiplexer
// 
// See: Document 43A239854 -- 6000B I/O Multiplexer
// (43A239854_600B_IOM_Spec_Jul75.pdf)
// 
// See AN87 which specifies some details of portions of PCWs that are
// interpreted by the channel boards and not the IOM itself.
// 
// See also: http://www.multicians.org/fjcc5.html -- Communications
// and Input/Output Switching in a Multiplex Computing System
// 
// See also: Patents: 4092715, 4173783, 1593312

 
// Copyright (c) 2007-2013 Michael Mondy
// 
// This software is made available under the terms of the
// ICU License -- ICU 1.8.1 and later.
// See the LICENSE file at the top-level directory of this distribution and
// at http://example.org/project/LICENSE.

 

// 
// 3.10 some more physical switches
// 
// Config switch: 3 positions -- Standard GCOS, Extended GCOS, Multics
// 
// Note that all M[addr] references are absolute (The IOM has no access to
// the CPU's appending hardware.)
//
// IOM BASE ADDRESS: 12 toggle switches: These are needed 
//
 

// AN87 Sect. 3, IOM MAILBOX LAYOUT
//    "Multics currently allows two IOs and requires that the INTERRUPT BASE
//     for both be set to 1200(8). The IOM BASE settings required are 1400(8)
//     for IOM A and 2000(8) for IMB B."
//
//  43A239854 600B IOM Spec: 3.12.3 Base Addresses
//    "Channel Mailbox Base Address - Twelve switches on the IOM configuration
//     panel provide bits 6-17 of the channel mailbox base address. The address 
//     extension, bits 0-5, and bits 18-23 of this base address are zero.
//
//     "Interrupt Multiplex Base Address - Bits 0-11 of the Interrupt Multiplex
//      Base Address are the same as Bits 0-11 of the Channel Mailbox Address and
//      are controlled by the same set of switches. Nine switches on the IOM
//      configuration panel provide bits 12-18, 22, and 23 of the Interrupt
//      Multiplex Base Address. Bits 19-21 of the base address are zero."
//
//     According to 43A239854, section 3.5: the two low bits of the interrupt 
//     base address are the IOM ID; but AN87 says that they should be zero for
//     both unit A and unit B.
//
//  43A239854 600B IOM Spec: 3.5.1 Channel Mailbox Base Addresses Switches
//     "The channel mailbox base address switches are ORed with the channel
//      number. Therefore to avoid ambiguity when channel numbers greater
//      than 15 are used, some of the low oreder bits of the channel mailbox
//      may be required to be zero."
//
//     
//
// 
// CAC: Notes on channel numbers
//     43A239854 3.4 CHANNEL NUMBERING
//
//       The channel number is a 9-bit binary number... Only the low-order
//       6 bits are used. [max_channels = 64]
//
//       Channel numbers are used for followin purposes in the IOM:
//
//       o A channel reognizes that a PCW is intended for it on the basis
//         of channel number;
//
//       o The IOM Central uses the channel number supplied by the channel
//         to determive the location of control word mailboxes in core
//         store or in scratchpad;
//
//       o The IOM Central places the channel number in the system fault
//         word when a system fault is detected and indicated so that the
//         software will know which channel was affected.
//
//       o The IOM Central uses the channel number to set a bit in the IMW.
//
//       Channel numbers 010(8) - 077(8) may be assigned to payload channels,
//       and channel numbers 000(8) - 007(8) are reserved for assignment
//       to overhead channels:
//
//           Channel No.      Overhead Channel Assigned
//           -----------      -------------------------
//
//              0             Illegal use - not assigned
//              1             Fault Channel
//              2             Connect Channel
//              3             Snapshot Channel
//              4             Wraparound Channel
//              5             Bootload Channel
//              6             Special Status Channel
//              7             Scratchpad Access Channel

 
// Each channel has assigned to it four 36 bit mailbox words.
//    0: List Point Word (LPW)
//    1: List Pointer Word Extension
//    2: Status Control Word (SCW)
//    3: Working Storage for Data Control Word (DCW)
//
// The LPW, LP extension and DCW for each channel will normally be stored
// in scratchpad memory
//
// Data Channels (aka payload channels)
//
// A data channel can use either of two basic methods of data transfer;
// direct or indirect. When the direct method of data transfer is used,
// the data channel specifies the core location (address) for each
// storage access. When the indrect method of data transfer is used, the
// IOM Central relieves the data channel of the task of addressing at
// the price of additional accesses by the IOM to: (a) core. of (b)
// scratchpad storage. The indirect channel must, however, specify its
// channel number, which determines the address for the data transfer
// and a tally specifying the amount of data to be transferred.
//
// Each type of interface to a peripheral control unit requires a 
// particular type of data channel.
//
// Data can be transferred between the IOM Central and either core
// storage or an indirect channel in byte sizes of 9. 36, and 72 bits.
//
// The connect channel controls the distribution of instructions which
// initiate the operation of any addressable channel.
//
// Definitions
//
// Tally Runout (TRO): A TRO is a fault, defined as an exhausted LPW
// tally field (the contents of LPW bits 24-35 equal to zero), and
// LPW bits 21 and 22 set to 0, 1. respectively, at the time the
// LPW it taken from its mailbox in core or in scratchpad.
//
// Pre-Tally Runout (PTRO): A PTRO is defined as a tally in either a DCW
// or and LPW which will be reduced to zero by the current channel service
// (DCW during data service, LPW during list service).
//
// List Pointer Word (LPW): A  word containing an address "pointing" to
// a list of control words, either DCW's or PCW's. NOTE: Only a LPW for a
// Connect Channel may legally point to a PCW>
//
// Data Control Word (DCW): A word contains an address indicating the
// first or current word of data in a list of data. IDCW, TDCW, IOTP, IONTP,
// and IOTD are variations of DCW's
//
// Peripheral Control Word (PCW): A word containing the number of the
// channel to be connected and, for CPI, in instruction of operation to
// be performed by a peripheral subsystem.
//
// Status Control Word (SCW): A word containing an address indicating
// the first or current empty position in a list of status words used
// by a particular channel.
//
// First List Service: A flat supplied by the channel to the IOM on the
// first list service following a connect. It directs the IOM ot access the
// core LPW followin a connect. It directs the IOM to access the core LPW
// ad LPWE mailboxes wherher of not this channel has a scratchpad.
//
// [N.B. these notes are taken from the 6000B manual; from the context
// provided in the following note, and elsewher in the manual, it would 
// appear that the 'B' means that it supports 24 bit addressing for 
// Multics, up from 18 bit for the 6000]
//
// Control word mailboxes, interrypt multiplex words, PCW list, 
// system fault word lists, and status queries will reside in the
// first 256K core. DCW's may reside in eithr the first 256K
// block or in the same 256K block as the data (depending upon
// LPW bit 20).
//
// Transactions which caurs address to be incremented will not be allowed
// to occur accoss modulo 256K (absolute) boundaries. That is, oveerflow
// past 2 to the 18 is not allowed.
//
// The "address extension" (i.e., the 6 most significant bits of a 24-bit
// address field) will be used for all data transfers and, depending on
// LPW 20, may also be used when fetching a DCW during a list service.
// It will be obtained from the following sources:
//
// 1) From bits (12-17) of a PCW directed to a data channel in the
//    Extended GCOS mode.
// 2) During a list service from bits (12-17) of an IDCW directed to a 
//    data channel provided IDCW 21=1.
//
// Progam interrupts
//
// The IOM must inform the processor of the completion of a previously
// initiated activity or of the occurance of other special events such
// as special interrupts from a peripheral, marker interrupts* from
// a channel, termination of an activity because of abnormal status, of
// fault conditions requiring attention of the software.
//
// * The marker interrupt indicated the normal completion of a PCW or an
// IDCW in a DCW list. This interrupt is under control of the Marker
// and Continue bits in the PCW or IDCW.
//
// LPW and LPW extension
//
// The LPW and LPWext is used to define the location and length of a list of
//   o PCW's to be issued by the connect channel, or
//   o DCW's to control the operation of a data channel.
// The LPW and LPErxt also includes fields which contol
//   o the ipdating of the LPW
//   o checking of DCW's for validity
//
//  LPW:
//   bit(s)
//    0-17  DCW Pointer
//      18  RES: Restrict IDCW's and Changing AE Flag
//      19  REL: IOM-controll Image of Rel Bit
//      20  AE:  Address Extension flag:
//              0 - DCW is in first 256K
//              1 - DEV's in Address Extension block
//              MBZ for all overhead channels (not checked)
//              MBZ for GCOS mode
//  LPWX
//     0-8  Lower Bound
//    9-17  Size
//   17-35  Pointer to first DCW of Most Recent Instruction
//
//  DCW Pointer: Provides the least significant 18 bits of a 24 bit address
//  of the DCW or PCW list. Updating of this field is controlled by the NC
//  (no change), TAL (Tally Control) and tally fields as shown in Table 3.2.1.
//  When updating is called for, the IOM Central increments this field by one
// (two if the channel is the connect channel) during each list service.
//
// Restricted Bit (LPW 18) - Provides the software with a way to restrict
// the use of Instruction DCW's by users without having to scan all DCW
// lists. If this bit is one and an Instruction DCW is encountered by
// the IOM Central, it will abort the I/O transaction and indicate a User
// Fault: Instruction DCW in Restricted Mode to the channel. If bit 18 is
// a zero, the list is unrestricted and Instruction DCW's will be allowed.
// Ignored for the connect channel; reserved for future use.


#include <stdio.h>
#include <sys/time.h>

#include "dps8.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "dps8_scu.h"
#include "dps8_iom.h"
#include "dps8_sys.h"
 
static t_stat iom_boot (int32 unit_num, DEVICE * dptr);

// Much of this is from AN87 as 43A23985 lacked details of 0..11 and 22..36

typedef struct dcw_t
  {
    enum { ddcw, tdcw, idcw } type;
    union
      {
        pcw_t instr;
        struct
          {
            uint daddr; // data address; 18 bits at 0..17);
            uint cp;    // char position; 3 bits 18..20
            uint tctl;  // tally control; 1 bit at 21
            uint type;  // 2 bits at 22..23
            uint tally; // 12 bits at 24..35
          } ddcw;
        struct {
            uint addr;
            bool ec;  // extension control
            bool i;   // IDCW control
            bool r;   // relative addressing control
          } xfer;
      } fields;
  } dcw_t;

typedef struct
  {
    uint32 dcw_ptr;     // bits 0..17
    word1 ires;    // bit 18; IDCW restrict
    word1 hrel;    // bit 19; hardware relative addressing
    word1 ae;      // bit 20; address extension
    word1 nc;      // bit 21; no tally; zero means update tally
    word1 trunout; // bit 22; signal tally runout?
    word1 srel;    // bit 23; software relative addressing; not for Multics!
    uint32 tally;    // bits 24..35
    // following not valid for paged mode; see B15; but maybe IOM-B non existant
    uint32 lbnd;
    uint32 size;
    uint32 idcw;    // ptr to most recent dcw, idcw, ...
  } lpw_t;
 

//-- // Channel Status Word -- from AN87, 3-11
//-- typedef struct {
//--     int chan;       // not part of the status word; simulator only
//--     int major;
//--     int substatus;
//--     // even/odd bit
//--     // status marker bit
//--     // soft, 2 bits set to zero by hw
//--     // initiate bit
//--     // chan_stat; 3 bits; 1=busy, 2=invalid chan, 3=incorrect dcw, 4=incomplete
//--     // iom_stat; 3 bits; 1=tro, 2=2tdcw, 3=bndry, 4=addr ext, 5=idcw,
//--     int addr_ext;   // BUG: not maintained
//--     int rcount; // 3 bits; residue in (from) PCW or last IDCW count (chan-data)
//--     // addr;    // addr of *next* data word to be transmitted
//--     // char_pos
//--     bool read;    // was last or current operation a read or a write
//--     // type;    // 1 bit
//--     // dcw_residue; // residue in tally of last dcw
//--     bool power_off;
//-- } chan_status_t;
//-- 
//-- typedef enum {
//--     chn_idle,       // Channel ready to receive a PCW from connect channel
//--     chn_pcw_rcvd,   // PCW received from connect channel
//--     chn_pcw_sent,   // PCW (not IDCW) sent to device
//--     chn_pcw_done,   // Received results from device
//--     chn_cmd_sent,   // A command was sent to a device
//--     chn_io_sent,    // A io transfer is in progress
//--     chn_need_status,// Status service needed
//--     chn_err,        // BUG: may not need this state
//-- } chn_state;
//-- 

static t_stat iom_show_mbx (FILE * st, UNIT * uptr, int val, void * desc);
static t_stat iom_show_config (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat iom_set_config (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat iom_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat iom_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat iom_reset(DEVICE *dptr);

// Hardware limit
#define N_IOM_UNITS_MAX 4
// Default
#define N_IOM_UNITS 1

#define N_IOM_PORTS 8

// The number of devices that a dev_code can address (6 bit number)

#define N_DEV_CODES 64

#define IOM_CONNECT_CHAN 2U

static UNIT iom_unit [N_IOM_UNITS_MAX] =
  {
    { UDATA(NULL /*&iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA(NULL /*&iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA(NULL /*&iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA(NULL /*&iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL }
  };

#define IOM_UNIT_NUM(uptr) ((uptr) - iom_unit)

static MTAB iom_mod [] =
  {
    {
       MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_NC, /* mask */
      0,            /* match */
      "MBX",        /* print string */
      NULL,         /* match string */
      NULL,         /* validation routine */
      iom_show_mbx, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      iom_set_config,         /* validation routine */
      iom_show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      iom_set_nunits, /* validation routine */
      iom_show_nunits, /* display routine */
      "Number of IOM units in the system", /* value descriptor */
      NULL   // help string
    },
    {
      0, 0, NULL, NULL, 0, 0, NULL, NULL
    }
  };

static DEBTAB iom_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { "TRACE", DBG_TRACE },
    { NULL, 0 }
  };

static REG iom_reg [] =
  {
//     { DRDATA (OS, config_sw_os, 1) },
    { NULL, NULL, 0, 0, 0, 0, NULL, NULL, 0, 0 }
  };

DEVICE iom_dev =
  {
    "IOM",       /* name */
    iom_unit,    /* units */
    iom_reg,     /* registers */
    iom_mod,     /* modifiers */
    N_IOM_UNITS, /* #units */
    10,          /* address radix */
    8,           /* address width */
    1,           /* address increment */
    8,           /* data radix */
    8,           /* data width */
    NULL,        /* examine routine */
    NULL,        /* deposit routine */
    iom_reset,   /* reset routine */
    iom_boot,    /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    DEV_DEBUG,   /* flags */
    0,           /* debug control flags */
    iom_dt,      /* debug flag names */
    NULL,        /* memory size change */
    NULL,        /* logical name */
    NULL,        // help
    NULL,        // attach help
    NULL,        // help context
    NULL         // description
  };


static t_stat boot_svc (UNIT * unitp);
static UNIT boot_channel_unit [N_IOM_UNITS_MAX] =
  {
    { UDATA (& boot_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL},
    { UDATA (& boot_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL},
    { UDATA (& boot_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL},
    { UDATA (& boot_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL}
  };


static void fetch_abs_word (word24 addr, word36 *data)
  {
    core_read (addr, data);
  }

static void store_abs_word (word24 addr, word36 data)
  {
    core_write (addr, data);
  }

static void fetch_abs_pair (word24 addr, word36 * even, word36 * odd)
  {
    core_read2 (addr, even, odd);
  }

static void store_abs_pair (word24 addr, word36 even, word36 odd)
  {
    core_write2 (addr, even, odd);
  }



//   Interrupt base 1400(8)  (IOM Base Address)
//                       001 100 000 000     < Multics value
// sw:           xxx xxx xxx xxx
//                    11 111 111 112 222
//       012 345 678 901 234 567 890 123
//
//    Switch setting for multics: 14(8)






//  Mailbox base (unit A) 1400(8) (Address of Interrupt Multiplex Word)
//                       001 100 000 000
// sw:                   xxx xxx x    xx
//                    11 111 111 112 222
//       012 345 678 901 234 567 890 123
//
//    Switch setting for multics: 001 100 000
//




enum config_sw_os_ { CONFIG_SW_STD_GCOS, CONFIG_SW_EXT_GCOS, CONFIG_SW_MULTICS };

// Boot device: CARD/TAPE;
enum config_sw_blct_ { CONFIG_SW_BLCT_CARD, CONFIG_SW_BLCT_TAPE };


struct unit_data
  {
    // Configuration switches
    
    // Interrupt multiplex base address: 12 toggles
    word12 config_sw_iom_base_address;

    // Mailbox base aka IOM base address: 9 toggles
    // Note: The IOM number is encoded in the lower two bits
    word9 config_sw_multiplex_base_address;

    // OS: Three position switch: GCOS, EXT GCOS, Multics
    enum config_sw_os_ config_sw_os; // = CONFIG_SW_MULTICS;

    // Bootload device: Toggle switch CARD/TAPE
    enum config_sw_blct_ config_sw_bootload_card_tape; // = CONFIG_SW_BLCT_TAPE; 

    // Bootload tape IOM channel: 6 toggles
    word6 config_sw_bootload_magtape_chan; // = 0; 

    // Bootload cardreader IOM channel: 6 toggles
    word6 config_sw_bootload_cardrdr_chan; // = 1;

    // Bootload: pushbutton
 
    // Sysinit: pushbutton

    // Bootload SCU port: 3 toggle AKA "ZERO BASE S.C. PORT NO"
    // "the port number of the SC through which which connects are to
    // be sent to the IOM
    word3 config_sw_bootload_port; // = 0; 

    // 8 Ports: CPU/IOM connectivity

    // Port configuration: 3 toggles/port 
    // Which SCU number is this port attached to 
    uint config_sw_port_addr [N_IOM_PORTS]; // = { 0, 1, 2, 3, 4, 5, 6, 7 }; 

    // Port interlace: 1 toggle/port
    uint config_sw_port_interlace [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Port enable: 1 toggle/port
    uint config_sw_port_enable [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Port system initialize enable: 1 toggle/port // XXX What is this
    uint config_sw_port_sysinit_enable [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Port half-size: 1 toggle/port // XXX what is this
    uint config_sw_port_halfsize [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 }; 
    // Port store size: 1 8 pos. rotary/port
    uint config_sw_port_storesize [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Hacks
    uint boot_skip;

    // other switches:
    //   alarm disable
    //   test/normal

  };

static struct unit_data unit_data [N_IOM_UNITS_MAX];

//-- /*
//--  NOTES on data structures
//--  
//--  SIMH has DEVICES.
//--  DEVICES have UNITs.
//--  UNIT member u3 holds the channel number.  Used by channel service routine
//--  which is called only with a UNIT ptr. Note that UNIT here refers to a 
//--  tape or disk or whatever, not a IOM UNIT.
//--  The iom_t IOM struct includes an array of channels:
//--  type
//--  DEVICE *dev; // ptr into sim_devices[]
//--  UNIT *board
//--  
//--  The above provides a channel_t per channel and a way to find it
//--  given only a UNIT ptr.
//--  The channel_t:
//--  Includes list-service flag/count, state info, major/minor status,
//--  most recent dcw, handle for sim-activate
//--  Includes IDCW/PCW info: cmd, code, chan-cmd, chan-data, etc
//--  Includes ptr to devinfo
//--  Another copy of IDCW/PCW stuff: dev-cmd, dev-code, chan-data
//--  Includes status major/minor & have-status flag
//--  Includes read/write flag
//--  Includes time for queuing
//--  */
//-- 
//-- 
//-- /*
//--  TODO -- partially done
//--  
//--  Following is prep for async...
//--  
//--  Have list service update DCW in mbx (and not just return *addrp)
//--  Note that we do write LPW in list_service().
//--  [done] Give channel struct a "scratchpad" LPW
//--  Give channel struct a "scratchpad" DCW
//--  Note that "list service" should "send" pcw/dcw to channel...
//--  
//--  Leave connect channel as immediate
//--  Make do_payload_channel() async.
//--  Need state info for:
//--  have a dcw to process?
//--  DCW sent to device?
//--  Has device sent back results yet?
//--  Move most local vars to chan struct
//--  Give device functions a way to report status
//--  Review flow charts
//--  New function:
//--  ms_to_interval()
//--  */
//-- 
//-- 
//-- typedef struct {
//--     int chan;
//--     int dev_code;
//--     // BUG/TODO: represent "masked" state
//--     chn_state state;
//--     int n_list;     // could be flag for first_list, but counter aids debug
//--     //bool need_indir_svc;  // Note: Currently equivalent to forcing control=2
//--     bool xfer_running;    // Set to true if an IDCW has chn cmd other than 2; causes same behavior as control=2
//--     bool payload; // False if PCW
//-- 
//--     bool have_status;         // from device
//--     chan_status_t status;
//--     UNIT* unitp;    // used for sim_activate() timing; BUG: steal from chn DEV
//--     // pcw_t pcw;           // received from the connect channel
//--     dcw_t dcw;      // most recent (in progress) dcw
//--     int control;    // Indicates next action; mostly from PCW/IDCW ctrl fields
//--     int err;        // BUG: temporary hack to replace "ret" auto vars...
//--     chan_devinfo devinfo;
//--     lpw_t lpw;
//-- } channel_t;

// We are abstracting away the MPCs, so we need to map dev_code 
// of channels


typedef struct
  {
    uint iom_num;
    int ports [N_IOM_PORTS]; // CPU/IOM connectivity; designated a..h; negative to disable
    int scu_port; // which port on the SCU(s) are we connected to?
    struct channels
      {
        enum dev_type type;
        enum chan_type ctype;
        DEVICE * dev; // attached device; points into sim_devices[]
        int dev_unit_num; // Which unit of the attached device
        // (tape_dev, disk_dev, etc.)
        // The channel "boards" do *not* point into the UNIT array of the
        // IOM entry within sim_devices[]. These channel "boards" are used
        // only for simulation of async operation (that is as arguments for
        // sim_activate()). Since they carry no state information, they
        // are dynamically allocated by the IOM as needed.
        UNIT * board; // represents the channel; See comment just above
//--         channel_t channel_state;
     
        iom_cmd * iom_cmd;
        iom_io * iom_io;
      } channels [max_channels] [N_DEV_CODES];
  } iom_t;

static iom_t iom [N_IOM_UNITS_MAX];

enum iom_sys_faults
  {
    // List from 4.5.1; descr from AN87, 3-9
    iom_no_fault = 0,
    iom_ill_chan = 01,      // PCW to chan with chan number >= 40
    // iom_ill_ser_req=02,
    // chan requested svc with a service code of zero or bad chan #
    iom_256K_of=04,
    // 256K overflow -- address decremented to zero, but not tally
    iom_lpw_tro_conn = 05,
    // tally was zero for an update LPW (LPW bit 21==0) when the LPW was
    // fetched for the connect channel
    iom_not_pcw_conn=06,    // DCW for conn channel had bits 18..20 != 111b
    // iom_cp1_data=07,     // DCW was TDCW or had bits 18..20 == 111b
    iom_ill_tly_cont = 013,
    // LPW bits 21-22 == 00 when LPW was fetched for the connect channel
    // 14 LPW had bit 23 on in Multics mode
  };

// AN87-00A pg 3-12

// IOM channel status

enum iom_central_status 
  {
    iom_cs_normal = 00,
    //  tally was zero for an update LPW (LPW bit 21==0) when the LPW
    //  was fetched and TRO-signal (bit 22) is on
    iom_cs_lpw_tro = 01,
    iom_cs_two_tdcws = 02,
    iom_cs_bndy_vio = 03,
    iom_cs_idcw_in_res_mode = 05,
    iom_cs_cp_discrepancy = 06,
  };

//-- #if 0
//-- // from AN87, 3-8
//-- typedef struct {
//--     int channel;    // 9 bits
//--     int serv_req;   // 5 bits; see AN87, 3-9
//--     int ctlr_fault; // 4 bits; SC ill action codes, AN87 sect II
//--     int io_fault;   // 6 bits; see enum iom_sys_faults
//-- } sys_fault_t;
//-- #endif
//-- 



static uint mbx_loc (int iom_unit_num, uint chan_num);
static void iom_fault (int iom_unit_num, uint chan, const char * who, int is_sys, int signal);
static int send_flags_to_channel (void);
static int dev_send_idcw (int iom_unit_num, uint chan, uint dev_code, pcw_t * pcwp, word12 * stati, bool * need_data, bool * is_read);
static int do_payload_channel (int iom_unit_num, pcw_t * pcw);
static int do_ddcw (int iom_unit_num, uint chan, uint dev_code, word24 addr, dcw_t * dcwp, int * control, word12 * stati);
static int lpw_write (uint chan, word24 chanloc, const lpw_t* lpw);
static int do_connect_chan (int iom_unit_num);
static char * lpw2text (const lpw_t * p, int conn);
static char* pcw2text(const pcw_t *p);
static char* dcw2text(const dcw_t *p);
static void fetch_and_parse_lpw (lpw_t * p, uint addr, bool is_conn);
static void decode_idcw(int iom_unit_num, pcw_t *p, bool is_pcw, word36 word0, word36 word1);
static void fetch_and_parse_dcw (int iom_unit_num, uint chan, dcw_t *p, uint32 addr, int read_only);
static int status_service(int iom_unit_num, uint chan, uint dev_code, word12 stati, word6 rcount, word12 residue, word3 char_pos, bool is_read);
enum iom_imw_pics;
static int send_general_interrupt (int iom_unit_num, uint chan, enum iom_imw_pics pic);
static int send_terminate_interrupt (int iom_unit_num, uint chan);

static struct
  {
    int scu_unit_num;
    int scu_port_num;
  } cables_from_scus [N_IOM_UNITS_MAX] [N_IOM_PORTS];

t_stat cable_iom (int iom_unit_num, int iom_port_num, int scu_unit_num, int scu_port_num)
  {
    if (iom_unit_num < 0 || iom_unit_num >= (int) iom_dev . numunits)
      {
        sim_printf ("cable_iom: iom_unit_num out of range <%d>\n", iom_unit_num);
        return SCPE_ARG;
      }

    if (iom_port_num < 0 || iom_port_num >= N_IOM_PORTS)
      {
        sim_printf ("cable_iom: iom_port_num out of range <%d>\n", iom_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_scus [iom_unit_num] [iom_port_num] . scu_unit_num != -1)
      {
        sim_printf ("cable_iom: port in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_scu (scu_unit_num, scu_port_num, iom_unit_num, iom_port_num);
    if (rc)
      return rc;

    cables_from_scus [iom_unit_num] [iom_port_num] . scu_unit_num = scu_unit_num;
    cables_from_scus [iom_unit_num] [iom_port_num] . scu_port_num = scu_port_num;

    return SCPE_OK;
  }


// cable_to_iom
//
// a peripheral is trying to attach a cable
//  to my port [iom_unit_num, chan_num, dev_code]
//  from their simh dev [dev_type, dev_unit_num]
//
// Verify that the port is unused; attach this end of the cable

t_stat cable_to_iom (int iom_unit_num, int chan_num, int dev_code, enum dev_type dev_type, chan_type ctype, int dev_unit_num, DEVICE * devp, UNIT * unitp, iom_cmd * iom_cmd, iom_io * iom_io)
  {
    if (iom_unit_num < 0 || iom_unit_num >= (int) iom_dev . numunits)
      {
        sim_printf ("cable_to_iom: iom_unit_num out of range <%d>\n", iom_unit_num);
        return SCPE_ARG;
      }

    if (chan_num < 0 || chan_num >= max_channels)
      {
        sim_printf ("cable_to_iom: chan_num out of range <%d>\n", chan_num);
        return SCPE_ARG;
      }

    if (dev_code < 0 || dev_code >= N_DEV_CODES)
      {
        sim_printf ("cable_to_iom: dev_code out of range <%d>\n", dev_code);
        return SCPE_ARG;
      }

    if (iom [iom_unit_num] . channels [chan_num] [dev_code] . type != DEVT_NONE)
      {
        sim_printf ("cable_to_iom: socket in use\n");
        return SCPE_ARG;
      }

    iom [iom_unit_num] . channels [chan_num] [dev_code] . type = dev_type;
    iom [iom_unit_num] . channels [chan_num] [dev_code] . ctype = ctype;
    iom [iom_unit_num] . channels [chan_num] [dev_code] . dev_unit_num = dev_unit_num;

    iom [iom_unit_num] . channels [chan_num] [dev_code] . dev = devp;
    iom [iom_unit_num] . channels [chan_num] [dev_code] . board  = unitp;
    iom [iom_unit_num] . channels [chan_num] [dev_code] . iom_cmd  = iom_cmd;
    iom [iom_unit_num] . channels [chan_num] [dev_code] . iom_io  = iom_io;

    return SCPE_OK;
  }


#ifndef QUIET_UNUSED
/*
 * get_iom_channel_dev ()
 *
 * Given an IOM unit number and an IOM channel number, return
 * a DEVICE * to the device connected to that IOM channel, and
 * the unit number for that device. Note: unit number is not
 * the same as device code.
 *
 */

static DEVICE * get_iom_channel_dev (uint iom_unit_num, int chan, int dev_code, int * unit_num)
  {
    // Some devices (eg console) and not unit aware, and ignore this
    // value
    if (unit_num)
      * unit_num = iom [iom_unit_num] .channels [chan] [dev_code] . dev_unit_num;

    return iom [iom_unit_num] .channels [chan] [dev_code] . dev;
  }
#endif

static uint mbx_loc (int iom_unit_num, uint chan_num)
  {
    word12 base = unit_data [iom_unit_num] . config_sw_iom_base_address;
    word24 base_addr = ((word24) base) << 6; // 01400
    word24 mbx = base_addr + 4 * chan_num;
    sim_debug (DBG_INFO, & iom_dev, "%s: IOM %c, chan %d is %012o\n",
      __func__, 'A' + iom_unit_num, chan_num, mbx);
    return mbx;
  }

/*
 * init_memory_iom()
 *
 * Load a few words into memory.   Simulates pressing the BOOTLOAD button
 * on an IOM or equivalent.
 *
 * All values are from bootload_tape_label.alm.  See the comments at the
 * top of that file.  See also doc #43A239854.
 *
 * NOTE: The values used here are for an IOM, not an IOX.
 * See init_memory_iox() below.
 *
 */

static void init_memory_iom (uint unit_num)
  {
    // The presence of a 0 in the top six bits of word 0 denote an IOM boot
    // from an IOX boot
    
    // " The channel number ("Chan#") is set by the switches on the IOM to be
    // " the channel for the tape subsystem holding the bootload tape. The
    // " drive number for the bootload tape is set by switches on the tape
    // " MPC itself.
    
    sim_debug (DBG_INFO, & iom_dev,
      "%s: Performing load of eleven words from IOM %c bootchannel to memory.\n",
      __func__, 'A' + unit_num);

    word12 base = unit_data [unit_num] . config_sw_iom_base_address;

    // bootload_io.alm insists that pi_base match
    // template_slt_$iom_mailbox_absloc

    //uint pi_base = unit_data [unit_num] . config_sw_multiplex_base_address & ~3;
    word36 pi_base = (((word36) unit_data [unit_num] . config_sw_multiplex_base_address)  << 3) |
                     (((word36) (unit_data [unit_num] . config_sw_iom_base_address & 07700U)) << 6) ;
    word3 iom_num = ((word36) unit_data [unit_num] . config_sw_multiplex_base_address) & 3; 
    word36 cmd = 5;       // 6 bits; 05 for tape, 01 for cards
    word36 dev = 0;            // 6 bits: drive number
    
    // Maybe an is-IMU flag; IMU is later version of IOM
    word36 imu = 0;       // 1 bit
    
    // Description of the bootload channel from 43A239854
    //    Legend
    //    BB - Bootload channel #
    //    C - Cmd (1 or 5)
    //    N - IOM #
    //    P - Port #
    //    XXXX00 - Base Addr -- 01400
    //    XXYYYY0 Program Interrupt Base
    
    enum config_sw_blct_ bootdev = unit_data [unit_num] . config_sw_bootload_card_tape;

    word6 bootchan;
    if (bootdev == CONFIG_SW_BLCT_CARD)
      bootchan = unit_data [unit_num] . config_sw_bootload_cardrdr_chan;
    else // CONFIG_SW_BLCT_TAPE
      bootchan = unit_data [unit_num] . config_sw_bootload_magtape_chan;


    // 1

    word36 dis0 = 0616200;

    // system fault vector; DIS 0 instruction (imu bit not mentioned by 
    // 43A239854)

    M [010 + 2 * iom_num] = (imu << 34) | dis0;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012llo\n",
      010 + 2 * iom_num, (imu << 34) | dis0);

    // Zero other 1/2 of y-pair to avoid msgs re reading uninitialized
    // memory (if we have that turned on)

    M [010 + 2 * iom_num + 1] = 0;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012o\n",
      010 + 2 * iom_num + 1, 0);
    

    // 2

    // terminate interrupt vector (overwritten by bootload)

    M [030 + 2 * iom_num] = dis0;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012llo\n",
      030 + 2 * iom_num, dis0);


    // 3

    // XXX CAC: Clang -Wsign-conversion claims 'base<<6' is int
    word24 base_addr = (word24) base << 6; // 01400
    
    // tally word for sys fault status
    M [base_addr + 7] = ((word36) base_addr << 18) | 02000002;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012llo\n",
       base_addr + 7, ((word36) base_addr << 18) | 02000002);


    // 4


    // ??? Fault channel DCW
    // bootload_tape_label.alm says 04000, 43A239854 says 040000.  Since 
    // 43A239854 says "no change", 40000 is correct; 4000 would be a 
    // large tally

    // Connect channel LPW; points to PCW at 000000
    M [base_addr + 010] = 040000;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012o\n",
      base_addr + 010, 040000);

    // 5

    word24 mbx = base_addr + 4 * bootchan;

    // Boot device LPW; points to IDCW at 000003

    M [mbx] = 03020003;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012o\n",
      mbx, 03020003);


    // 6

    // Second IDCW: IOTD to loc 30 (startup fault vector)

    M [4] = 030 << 18;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012o\n",
      4, 030 << 18);
    

    // 7

    // Default SCW points at unused first mailbox.
    // T&D tape overwrites this before the first status is saved, though.
    // CAC: But the status is never saved, only a $CON occurs, never
    // a status service

    // SCW

    M [mbx + 2] = ((word36)base_addr << 18);
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012llo\n",
      mbx + 2, ((word36)base_addr << 18));
    

    // 8

    // 1st word of bootload channel PCW

    M [0] = 0720201;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012o\n",
      0, 0720201);
    

    // 9

    // "SCU port" 
    word3 port = unit_data [unit_num] . config_sw_bootload_port; // 3 bits;
    
    // Why does bootload_tape_label.alm claim that a port number belongs in 
    // the low bits of the 2nd word of the PCW?  The lower 27 bits of the 
    // odd word of a PCW should be all zero.

    // [CAC] Later, bootload_tape_label.alm does:
    //
    //     cioc    bootload_info+1 " port # stuck in PCW
    //     lda     0,x5            " check for status
    //
    // So this is a bootloader kludge to pass the bootload SCU number
    // 

    // [CAC] From Rev01.AN70.archive:
    //  In BOS compatibility mode, the BOS BOOT command simulates the IOM,
    //  leaving the same information.  However, it also leaves a config deck
    //  and flagbox (although bce has its own flagbox) in the usual locations.
    //  This allows Bootload Multics to return to BOS if there is a BOS to
    //  return to.  The presence of BOS is indicated by the tape drive number
    //  being non-zero in the idcw in the "IOM" provided information.  (This is
    //  normally zero until firmware is loaded into the bootload tape MPC.)

    // 2nd word of PCW pair

    M [1] = ((word36) (bootchan) << 27) | port;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012llo\n",
      1, ((word36) (bootchan) << 27) | port);
    

    // 10

    // following verified correct; instr 362 will not yield 1572 with a 
    // different shift

   // word after PCW (used by program)

    M [2] = ((word36) base_addr << 18) | (pi_base) | iom_num;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012llo\n",
      2,  ((word36) base_addr << 18) | (pi_base) | iom_num);
    

    // 11

    // IDCW for read binary

    M [3] = (cmd << 30) | (dev << 24) | 0700000;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012llo\n",
      3, (cmd << 30) | (dev << 24) | 0700000);
    
  }

static t_stat boot_svc (UNIT * unitp)
  {
    int unit_num = unitp - boot_channel_unit;
    // the docs say press sysinit, then boot; simh doesn't have an
    // explicit "sysinit", so we ill treat  "reset iom" as sysinit.
    // The docs don't say what the behavior is is you dont press sysinit
    // first so we wont worry about it.

/*
   431239854 600B IOM Spec Jul75

      3.10 BOOTLOAD CHANNEL

      The bootload channel will consist of 11 words of read only storage/
      IOM configuration switches. It will be placed into operation when
      the BOOTLOAD pushbutton at either the System console or the bootload
      section of the Configuration Panel is depressed.

      When the bootload channel is placed into operation, it will cause the
      11 words to be written into core storage and will activate the connect
      channel in the same manner as a $CON signal from the system controller.
      The connect channel will cause a PCW to be sent to the channel 
      specified as the boot channel by the configuration switches and will 
      iniate transfer of one record for that channel.

      The following configuration switches from the IOM configuration panel
      will be accessible to the bootload channel so that the setting of
      these switches can be stored by the bootload channel as part of the
      bootload program (see Figure 3.10a):

      o  Port (the numveb of the system controller port to which the IOM is
         connected)
      o  IOM Mailbox Base Address
      o  IOM Interrupt Multiplex Base Address
      o  IOM Number... 0, 1, 2 or 3
      o  Card/Tape Selector
      o  Mag Tape Channel Number
      o  Card Reader Channel Number

     The 6000B boot program shall work on either a CPU or PSI channel.  It
     has the following characteristics:

     1.  The program is highly dependent on the configuration panel switches.

     2.  The program performs the following functions:

         a. The system fault vector, terminate fault vector, boot device's
            SCW and the system fault channels (status) DCW are set up to
            stop the program if the BOOT is unsuccessful (device offline,
            system fault, etc.) and to indicate why it failed.

         b. The connect channel LPW and the boot channel LPW and DCW are
            set up to read (binary) the first record starting at location 30(8).
            This will overlay the terminate interrupt vector and thereby
            cause the processor to start executing the code from the 
            first record upon receipt of the terminate from reading that
            record.

     3.  The connect channel PCW is treated differently by the CPI channel
         and the PSI channel. The CPI channel does a store status, The
         PSI channel goes into startup.

     The bootload channel is assigned a channel number of 5, but will not
     respond to PCW's directed to channel 5.  Operation of the bootload
     channel will be initiated only by an operator pressing the BOOTLOAD
     pushbutton on the IOM configuration channel or on the system console
     (after first pressing the System Initialize pushbutton). The boot-
     load channel will make no use of the mailbox words set aside for
     channel 5.

     The reading of the first record of magnetic tape or card without
     processor intervention facilitates primitive instruction testing
     by T&D software, without requiring a (possible) sick processor to
     actively initiate its own testing.

     Figures 3.10a and 3.10b respectively show which configuration panel
     switches are used in BOOTing, what the boot program looks like.

       Octets       Function/Switches                     Range
       ------       -----------------                     -----

         BB         Bootload Channel Number           10(8) - 77(8)

         C          Command/based on Bootload
                    Source Switch                      1(8) or 5(8)

         N          IOM Number                         0(8) - 3(8)

         P          Port Number                        0(8) - 3(8)

       XXXX00       Base Address                  000000(8) - 777700(8)

       XXYYY0       Program Interrupt Base        000000(8) - 777740(8)
                    (First 6 bits same as Base Address)

                         Figure 3.10a

         Bootload Program Fields Supplied by Configuration Switches




       Word               Address         Data       Location              Purpose
       ----               -------         ----       --------              -------

         1        000010(8) + (Nx2)   000000616200   System Fault Vector   Disconnect if get a system fault
                                                                           when loading or executing BOOT
                                                                           program

         2        000030(8) + (Nx2)   000000616200   Terminate Int Vector 1st record will begin here.  Dis-
                                                                          connect if record not read
                                                                          (device offline, etc.)

         3        XXXX00(8) + 07(8)   XXXX02000002   Fault channel DCW    Tally word for storing Syetem
                                                                          fault status at Base Address +
                                                                          2 (in case stop at word #1)

         4        XXXX00(8) + 10(8)  000000040000    Connect channel LPW  Upon receiving connect, do a list
                                                                          service (with no change) to PCW
                                                                          pair starting at location 000000(8)

         5        XXXX00(8) + (BBx4) 000003020003    BOOT device LPW      2 DCWs for BOOT device starts at
                                                                          location 000003(8)

         6        000004(8)          000030000000    2nd DCW              IOTD

         7    XXXX00(8) + (BBx4) + 2 XXXX00000000    BOOT device SCW      Tally word for storing BOOT device
                                                                          status at Base Address (in case
                                                                          stop at word #2)

         8        000000(8)          000000720201    1st word of PCW pair for PSI: initiate channel
                                                                          for CPI: request status (1 time)
                                                                                   and continue

         9        000001(8)          0BB00000000P    2nd word of PCW pair BOOT device channel #, port #

        10        000002(8)          XXXX00XXYYYN    next word after PCW  Base Address, Program Interrupt
                                                                          Base, IOM #

        11        000003(8)          0C0000700000    IDCW                 Read binary command (C = 1 if
                                                                          card reader, C = 5 if tape)


                                Figure 3.10.b - BOOT Program

*/

    sim_debug (DBG_DEBUG, & iom_dev, "%s: starting on IOM %c\n",
      __func__, 'A' + unit_num);

    // initialize memory with boot program
    init_memory_iom ((uint)unit_num);

    // simulate $CON
    iom_interrupt (unit_num);

    sim_debug (DBG_DEBUG, &iom_dev, "%s finished\n", __func__);

#if 0
//  Hack to make t4d testing easier. Advence the tape after booting to
//  allow skipping over working test blocks

    if (unit_data [unit_num] . boot_skip)
      {
        int skip = unit_data [unit_num] . boot_skip;
        int tape_unit_num;
        /* DEVICE * tapedevp = */ get_iom_channel_dev (unit_num, 
            unit_data [unit_num] . config_sw_bootload_magtape_chan,
            0, /* dev_code */
            & tape_unit_num);
        sim_printf ("boot_skip: Tape %d skips %d record(s)\n", tape_unit_num, skip);
        for (int i = 0; i < unit_data [unit_num] . boot_skip; i ++)
          {
            t_mtrlnt tbc;
            sim_tape_sprecf (& mt_unit [tape_unit_num], & tbc);
          }
      }
#endif

    // returning OK from the simh BOOT command causes simh to start the CPU
    return SCPE_OK;
  }

static t_stat iom_boot (int32 unit_num, DEVICE * __attribute__((unused)) dptr)
  {
    sim_activate (& boot_channel_unit [unit_num], sys_opts . iom_times . boot_time );

    //sim_printf ("Faking DIS\n");
    //cpu . cycle = DIS_cycle;

    // returning OK from the simh BOOT command causes simh to start the CPU
    return SCPE_OK;
  }

// Map memory to port
static int iom_scpage_map [N_IOM_UNITS_MAX] [N_SCPAGES];

static void setup_iom_scpage_map (void)
  {
    sim_debug (DBG_DEBUG, & cpu_dev,
      "%s: setup_iom_scpage_map: SCPAGE %d N_SCPAGES %d MAXMEMSIZE %d\n", 
      __func__, SCPAGE, N_SCPAGES, MAXMEMSIZE);

    for (int iom_unit_num = 0; iom_unit_num < (int) iom_dev . numunits; iom_unit_num ++)
      {
        // Initalize to unmapped
        for (int pg = 0; pg < (int) N_SCPAGES; pg ++)
          iom_scpage_map [iom_unit_num] [pg] = -1;
    
        struct unit_data * p = unit_data + iom_unit_num;
        // For each port (which is connected to a SCU
        for (int port_num = 0; port_num < N_IOM_PORTS; port_num ++)
          {
            if (! p -> config_sw_port_enable [port_num])
              continue;
            // Calculate the amount of memory in the SCU in words
            uint store_size = p -> config_sw_port_storesize [port_num];
            uint sz = 1 << (store_size + 16);
    
            // Calculate the base address of the memor in wordsy
            uint assignment = switches . assignment [port_num];
            uint base = assignment * sz;
    
            // Now convert to SCPAGES
            sz = sz / SCPAGE;
            base = base / SCPAGE;
    
            sim_debug (DBG_DEBUG, & cpu_dev,
              "%s: unit:%d port:%d ss:%u as:%u sz:%u ba:%u\n",
              __func__, iom_unit_num, port_num, store_size, assignment, sz, 
              base);
    
            for (uint pg = 0; pg < sz; pg ++)
              {
                uint scpg = base + pg;
                if (/*scpg >= 0 && */ scpg < N_SCPAGES)
                  iom_scpage_map [iom_unit_num] [scpg] = port_num;
              }
          }
      }
    for (int iom_unit_num = 0; iom_unit_num < (int) iom_dev . numunits; iom_unit_num ++)
        for (int pg = 0; pg < (int) N_SCPAGES; pg ++)
          sim_debug (DBG_DEBUG, & cpu_dev, "%s: %d:%d\n", 
            __func__, pg, iom_scpage_map [iom_unit_num] [pg]);
  }
   
static int query_iom_scpage_map (int iom_unit_num, word24 addr)
  {
    uint scpg = addr / SCPAGE;
    if (scpg < N_SCPAGES)
      return iom_scpage_map [iom_unit_num] [scpg];
    return -1;
  }


//-- /*
//--  * iom_svc()
//--  *
//--  *    Service routine for SIMH events for the IOM itself
//--  */
//-- 
//-- t_stat iom_svc(UNIT *up)
//-- {
//--     sim_debug (DBG_INFO, &iom_dev, "iom_svc: service started.\n");
//--     iom_interrupt(IOM_UNIT_NUM (up));
//--     return SCPE_OK;
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- /*
//--  * channel_svc()
//--  *
//--  *    Service routine for devices such as tape drives that may be connected to the IOM
//--  */
//-- 
//-- t_stat channel_svc (UNIT * up)
//--   {
//--     int chan = up -> u3;
//--     int dev_code = up -> u4;
//--     int iom_unit_num = up -> u5;
//-- 
//--     sim_debug (DBG_NOTIFY, & iom_dev, "channel_svc: service starting for IOM %c channel %d, dev_code %d\n", 'A' + iom_unit_num, chan, dev_code);
//-- 
//--     channel_t *chanp = get_chan (iom_unit_num, chan, dev_code);
//--     if (chanp == NULL)
//--         return SCPE_ARG;
//-- 
//--     // FIXME: It might be more realistic for the service routine to to call
//--     // device specific routines.  However, instead, we have the device do
//--     // all the work ahead of time and the queued service routine is just
//--     // reporting that the work is done.
//--     chanp->status.major = chanp->devinfo . major;
//--     chanp->status.substatus = chanp->devinfo . substatus;
//--     chanp->status.rcount = chanp->devinfo . chan_data;
//--     chanp->status.read = chanp->devinfo . is_read;
//--     chanp->have_status = 1;
//-- 
//--     do_payload_channel(iom_unit_num, chanp);
//-- 
//--     return SCPE_OK;
//-- }
//-- 

/*
 * iom_init()
 *
 *  Once-only initialization
 */

void iom_init (void)
  {
    sim_debug (DBG_INFO, & iom_dev, "%s: running.\n", __func__);

    memset(&iom, 0, sizeof(iom));

    for (int unit_num = 0; unit_num < N_IOM_UNITS_MAX; unit_num ++)
      {
    
        for (int i = 0; i < N_IOM_PORTS; ++i)
          {
            iom [unit_num] . ports [i] = -1;
          }
        
        for (int chan = 0; chan < max_channels; ++ chan)
          {
            for (int dev_code = 0; dev_code < N_DEV_CODES; dev_code ++)
              {
                iom [unit_num] . channels [chan] [dev_code] . type = DEVT_NONE;
              }
          }
      }

    for (int i = 0; i < N_IOM_UNITS_MAX; i ++)
      for (int p = 0; p < N_IOM_PORTS; p ++)
      cables_from_scus [i] [p] . scu_unit_num = -1;
  }

/*
 * iom_reset()
 *
 *  Reset -- Reset to initial state -- clear all device flags and cancel any
 *  any outstanding timing operations. Used by SIMH's RESET, RUN, and BOOT
 *  commands
 *
 *  Note that all reset()s run after once-only init().
 *
 */

static t_stat iom_reset(DEVICE * __attribute__((unused)) dptr)
  {
    sim_debug (DBG_INFO, & iom_dev, "%s: running.\n", __func__);

    for (int unit_num = 0; unit_num < (int) iom_dev . numunits; unit_num ++)
      {
        for (int chan = 0; chan < max_channels; ++ chan)
          {
            for (int dev_code = 0; dev_code < N_DEV_CODES; dev_code ++)
              {
                DEVICE * devp = iom [unit_num] . channels [chan] [dev_code] . dev;
                if (devp)
                  {
                    if (devp -> units == NULL)
                      {
                        sim_debug (DBG_ERR, & iom_dev, 
                          "%s: Device on IOM %c channel %d dev_code %d does not have any units.\n",
                          __func__,  'A' + unit_num, chan, dev_code);
                      }
                  }
              }
          }
      }
    

    setup_iom_scpage_map ();

    return SCPE_OK;
  }

/*
 * iom_interrupt()
 *
 * Top level interface to the IOM for the SCU.   Simulates receipt of a $CON
 * signal from a SCU.  The $CON signal is sent from an SCU when a CPU executes
 * a CIOC instruction asking the SCU to signal the IOM.
 */

void iom_interrupt (int iom_unit_num)
  {
    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c starting.\n",
      __func__, 'A' + iom_unit_num);
    sim_debug (DBG_TRACE, & iom_dev, "\nIOM starting.\n");

    //iom_show_mbx (NULL, iom_unit + iom_unit_num, 0, "");

    int ret = do_connect_chan (iom_unit_num);
    if (ret)
      {
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: do_connect_chan returned %d\n", __func__, ret);
      }

    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c finished.\n",
      __func__, 'A' + iom_unit_num);
  }

//
// do_connect_chan ()
//
// Process the "connect channel".  This is what the IOM does when it
// receives a $CON signal.
//
// Only called by iom_interrupt() just above.
//
// The connect channel requests one or more "list services" and processes the
// resulting PCW control words.
//
 
static int do_connect_chan (int iom_unit_num)
  {
    lpw_t lpw; // Channel scratch pad
    bool ptro = false;
    bool first_list = true;

    uint chanloc = mbx_loc (iom_unit_num, IOM_CONNECT_CHAN);

    do // while (!ptro)
      {

        // 4.3.1a: FIRST_SERVICE == YES?

        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: Get connect channel LPW @ %#06o.\n",
                   __func__, chanloc);
        fetch_and_parse_lpw (& lpw, chanloc, true);
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: Connect LPW at %#06o: %s\n", 
                    __func__, chanloc, lpw2text (& lpw, true));
        sim_debug (DBG_TRACE, & iom_dev,
                   "Connect LPW at %#06o: %s\n", 
                    chanloc, lpw2text (& lpw, true));

        // 4.3.1a: CONNECT_CHANNEL = YES
    
        // 4.3.1a: LPW 21,22 == 00?

        if (lpw . nc == 0 && lpw . trunout == 0)
          {
             iom_fault (iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, iom_ill_tly_cont);
             return 1;
          }
   
        // 4.3.1a: LPW 21,22 == 01 AND TALLY == 0?

        if (lpw . nc == 0 && lpw . trunout == 1 && lpw . tally == 0)
          {
             iom_fault (iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, 
                        iom_lpw_tro_conn);
             return 1;
          }
   
        // 4.3.1a: LPW 21,22 == 01 AND TALLY > 1 AND OVERFLOW?

        if (lpw . nc == 0 && lpw . trunout == 1 && lpw . tally > 1)
          {
             if (lpw . dcw_ptr  + lpw . tally >= 01000000)
               iom_fault (iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, iom_256K_of);
             return 1;
          }
   
        // 4.3.1a: LPW 21,22 == 01 AND TALLY == 1?

        if (lpw . nc == 0 && lpw . trunout == 1 && lpw . tally == 1)
          {
            ptro = true;
            sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (a)\n", __func__);
          }

        // 4.3.1a: LPW 21,22 == 1X?

        if (lpw . nc == 1)
          {
            ptro = true;
            sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (b)\n", __func__);
          }


        // 4.3.1a: PULL PCW FROM CORE

        sim_debug (DBG_DEBUG, & iom_dev, "%s: PCW @ 0%06o\n", 
                   __func__, lpw . dcw_ptr);
        
        pcw_t pcw;
        word36 word0, word1;
    
        (void) fetch_abs_pair (lpw . dcw_ptr, & word0, & word1);
        decode_idcw (iom_unit_num, & pcw, 1, word0, word1);
    
        sim_debug (DBG_INFO, & iom_dev, "%s: PCW is: %s\n", 
                   __func__, pcw2text (& pcw));
        sim_debug (DBG_TRACE, & iom_dev, "PCW is: %s\n", 
                   pcw2text (& pcw));

        // BUG/TODO: Should these be user faults, not system faults?

        // 4.3.1b: PCW 18-20 != 111

        if (pcw . cp != 07)
          {
            iom_fault (iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, iom_not_pcw_conn);
            return 1;
          }
        
        // 4.3.1b: ILLEGAL CHANNEL NUMBER?

        if (/*pcw . chan < 0 ||*/ pcw . chan >= max_channels)
          {
            iom_fault(iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, iom_ill_chan);
            return 1;
          }
    
//--     if (pcw.mask)
//--       {
//--         // BUG: set mask flags for channel?
//--         sim_debug (DBG_ERR, & iom_dev, "%s: PCW Mask not implemented\n", __func__);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--       }

        // 4.3.1b: SEND PCW TO CHANNEL

        //dcw . type = idcw;
        //dcw . fields . instr = pcw;

        int ret = do_payload_channel (iom_unit_num, & pcw);
        if (ret)
          {
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: do_payload_channel returned %d\n", __func__, ret);
          }

        // 4.3.1c: D

        ret = send_flags_to_channel ();
        if (ret)
          {
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: send_flags_to_channel returned %d\n", __func__, ret);
          }

        // 4.3.1c: LPW 21

        if (lpw . nc == 0)
          {
            // 4.3.1c: LPW 21 == 0

            // 4.3.1c: UPDATE LPW ADDRESS & TALLY

            -- lpw . tally;
            lpw . dcw_ptr += 2; // pcw is two words

            // 4.3.1c: IDCW OR FIRST LIST

            if (pcw . cp == 7 || first_list)
              {
                // 4.3.1c: IDCW OR FIRST LIST == YES
                // 4.3.1c: WRITE LPW & LPW EXT. INTO MAILBOXES (scratch and core)
                lpw_write (IOM_CONNECT_CHAN, chanloc, & lpw);
              }
            else
              {
                // 4.3.1c: IDCW OR FIRST LIST == NO
                // 4.3.1c: WRITE LPW INTO MAILBOX (scratch and code)

                lpw_write (IOM_CONNECT_CHAN, chanloc, & lpw);
              }
          }
        else
          {
            // 4.3.1c: LPW 21 == 1 (NO UPDATE)

            // 4.3.1c: IDCW OR FIRST LIST

            if (pcw . cp == 7 || first_list)
              {
                // 4.3.1c: IDCW OR FIRST LIST == YES
                // 4.3.1c: WRITE LPW & LPWE INTO MAILBOXES (scratch and core)

                lpw_write (IOM_CONNECT_CHAN, chanloc, & lpw);
              }
            else
              {
                // 4.3.1c: IDCW OR FIRST LIST == NO
                // 4.3.1c: TCDW?

                if (pcw . cp != 7 && pcw . mask == 0 && pcw . control == 2)
                  {
                    // 4.3.1c: TCDW == YES

                    // 4.3.1c: WRITE LPW & LPWE INTO MAILBOX (scratch and core)

                    lpw_write (IOM_CONNECT_CHAN, chanloc, & lpw);
                  }
              }
          }
        first_list = false;
 
      }
    while (! ptro);

    return 0;
  }

static int do_payload_channel (int iom_unit_num, pcw_t * pcwp)
  {

// It is not clear how the payload channel controls looping; the 4.3.1
// does not set ptro or tro if not connect channel. Lets just try looping
// until the tally is 0.
//
// Or until a IOTD (do IO and disconnect is done.

// From AN70, System Initialization PLM May 84
// "[The CSU6001] requires all it's device commands to be specified in the
// PCW, and ignores IDCWS."
//
// However, the DDCWs need to be processed by the list service.
// Assuming the CSU6001 is representative of CPI devices, adding code
// to break out of the list service after a single need_data loop if
// channel is a CPI; this should fix the console multiple command issue.

    int ret;
    uint chan = pcwp -> chan;
    bool need_data = false; // this is how a device asks for list service to continue
    bool is_read = false;
    bool is_cpi = iom [iom_unit_num] . channels [chan] [pcwp -> dev_code] . ctype == chan_type_CPI;
    word12 stati = 0;

#if 0
    if (is_cpi)
      {
        // 3. The connect channel PCW is treated differently by the CPI channel
        // and the PSI channel. The CPI channel does a store status, The
        // PSI channel goes into startup.

        word12 stati = 0;

        ret = dev_send_idcw (iom_unit_num, chan, pcwp -> dev_code, pcwp,
                             & stati, & need_data);
        if (ret)
          {
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: dev_send_idcw returned %d\n", __func__, ret);
          }

        if (stati & 04000)
          {
             status_service (iom_unit_num, chan, pcwp -> dev_code, stati, pcwp -> chan_data);
          }

        if (! need_data)
          {
            send_terminate_interrupt (iom_unit_num, chan);

            return 0;
          }
      }
#else
    {
      // 3. The connect channel PCW is treated differently by the CPI channel
      // and the PSI channel. The CPI channel does a store status, The
      // PSI channel goes into startup.

      //word12 stati = 0;

      ret = dev_send_idcw (iom_unit_num, chan, pcwp -> dev_code, pcwp,
                           & stati, & need_data, & is_read);
      //sim_printf ("dev_send_idcw p(0) stati %04o control %o ret %d\n", stati, pcwp -> control, ret);
      if (ret)
        {
          sim_debug (DBG_DEBUG, & iom_dev,
                     "%s: dev_send_idcw returned %d\n", __func__, ret);
        }

#if 0
      if (! need_data &&
          pcwp -> control == 0)
        {
          if (ret == 0)
            {
              status_service (iom_unit_num, chan, pcwp -> dev_code, stati, 0 /*rcount*/, 0 /*residue*/, 0 /*char_pos*/, 0 /*is_read*/);
              send_terminate_interrupt (iom_unit_num, chan);
            }
          return ret;
        }
#endif
#if 0
      if (! need_data && iom [iom_unit_num] . channels [chan] [pcwp -> dev_code] . ctype == chan_type_CPI)
        {
          send_terminate_interrupt (iom_unit_num, chan);

          return 0;
        }
#endif
      }
#endif

    lpw_t lpw; // Channel scratch pad
    bool ptro = false;
    bool first_list = true;
    int user_fault_flag = iom_cs_normal;
    int tdcw_count = 0;
    //int disconnect = 0;

    // for idcw
    //word12 stati = 0;

    uint chanloc = mbx_loc (iom_unit_num, chan);

    word3 char_pos = 0;
    word6 rcount = 0;
    word12 residue = 0;
    

    do // while (!ptro)
      {
        if (is_cpi && ! need_data)
          break;
        need_data = false; // This resets the CPI need_data, but that's okay,
                           // because we will process at least one DCW

        // 4.3.1a: FIRST_SERVICE?

        if (first_list)
          {
            // 4.3.1a: FIRST_SERVICE == YES
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: Get channel LPW @ %#06o.\n",
                       __func__, chanloc);
            fetch_and_parse_lpw (& lpw, chanloc, false);
            sim_debug (DBG_DEBUG, & iom_dev,
                       "LPW at %#06o: %s\n", chanloc,
                       lpw2text (& lpw, false));
            sim_debug (DBG_TRACE, & iom_dev,
                       "Payload LPW at %#06o: %s\n", 
                        chanloc, lpw2text (& lpw, true));
          }

        // 4.3.1a: CONNECT_CHANNEL == NO
        
        // 4.3.1a: LPW 21,22 == ?
    
        if (lpw . nc == 0 && lpw . trunout == 0)
          {
            // 4.3.1a: LPW 21,22 == 00
            // 4.3.1a: 256K OVERFLOW?
            if (lpw . ae == 0 && lpw . dcw_ptr  + lpw . tally >= 01000000)
              {
                iom_fault (iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, iom_256K_of);
                return 1;
              }
            //if (lpw . tally <= 1)
              //{
                //ptro = true;
                //sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (d)\n", __func__);
              //}
          }
    
        if (lpw . nc == 0 && lpw . trunout == 1)
          {
    A:
            // 4.3.1a: LPW 21,22 == 01
    
            // 4.3.1a: TALLY?
    
            if (lpw . tally == 0)
              {
                // 4.3.1a: TALLY == 0
                // 4.3.1a: SET USER FAULT FLAG
                iom_fault (iom_unit_num, chan, __func__, 0, 1);
              }
            else if (lpw . tally > 1)
              {
                if (lpw . ae == 0 && lpw . dcw_ptr  + lpw . tally >= 01000000)
                  {
                    iom_fault (iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, iom_256K_of);
                    return 1;
                  }
              }
            else // tally == 1
              {
// Turning this code on does not fix bug of 2nd IDCW @ 78
// because the tall is 3
                //ptro = true;
                //sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (a)\n", __func__);
              }

          }
    
        //if (lpw . nc == 1)
          //{
            //ptro = true;
            //sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (b)\n", __func__);
          //}

        // 4.3.1a: LPW 20?
    
        uint dcw_addr = lpw . dcw_ptr;
    
        if (lpw . ae)
          {
            // 4.3.1a: LPW 20 == 1
            // XXX IF STD_GCOS
    
            dcw_addr += lpw . lbnd;
          }
        else
          {
            // 4.3.1a: LPW 20 == 1
    
            // 4.3.1a: PULL DCW FROM CORE USING ADDRESS EXTENSION = ZEROS
            // dcw_addr += 0;
          }
    
        sim_debug (DBG_DEBUG, & iom_dev, "%s: DCW @ 0%06o\n", 
                   __func__, dcw_addr);
        
        dcw_t dcw;
    // XXX check 1 bit; read_only
        fetch_and_parse_dcw (iom_unit_num, chan, & dcw, dcw_addr, 1);
        sim_debug (DBG_INFO, & iom_dev, "%s: DCW is: %s\n",
                   __func__, dcw2text (& dcw));
        sim_debug (DBG_TRACE, & iom_dev, "DCW is: %s\n",
                   dcw2text (& dcw));
    
        //if (dcw . type == ddcw && dcw . control == 0)
          //ptro = true;

        // 4.3.1b: C
    
        // 4.3.1b: IDCW?
        
        stati = 0;
    
        if (dcw . type == idcw)
          {
            // 4.3.1b: IDCW == YES
    
            // 4.3.1b: LPW 18 RES?
    
            if (lpw . ires)
              {
                // 4.3.1b: LPW 18 RES == YES
                user_fault_flag = iom_cs_idcw_in_res_mode;
              }
            else
              {
                lpw . idcw = dcw_addr;
                // SEND IDCW TO CHANNEL
                need_data = false;
                dev_send_idcw (iom_unit_num, chan, pcwp -> dev_code, & dcw . fields . instr, & stati, & need_data, & is_read);
                //sim_printf ("dev_send_idcw p(1) stati %04o control %o\n", stati, dcw . fields . instr . control);
                char_pos = 0;
                rcount = dcw . fields . instr . chan_data;
                residue = 0;
                // The IOM boot IDCW has a control of 0, but that means that
                // the IOTD is ignored.
                // Exit DCW loop if non-zero major status
                if ((stati & 04000) && // have status
                    ((stati & 03700) != 0))
                  {
//sim_printf ("cac terminate 7\n");
                    sim_debug (DBG_DEBUG, & iom_dev,
                               "%s: IDCW returns non-zero status(%04o); terminating DCW loop\n",
                               __func__, stati);
                    //status_service (iom_unit_num, chan, pcwp -> dev_code, stati, pcwp -> chan_data);
                    //status_service (iom_unit_num, chan, pcwp -> dev_code, stati, dcw . type == idcw ? dcw . fields . instr . chan_data : dcw . fields . ddcw . tally, is_read);
                    //status_service (iom_unit_num, chan, pcwp -> dev_code, stati, dcw . type == idcw ? dcw . fields . instr . dev_code : dcw . fields . ddcw . tally, is_read);
                    status_service (iom_unit_num, chan, pcwp -> dev_code, stati, rcount, residue, char_pos, is_read);
                    break;
                  }
                // terminate?
// The code that searches for the console has an IDCW reset with terminate,
// follow by the IDCW alert. Why does the reset not cause a terminate?
// Guess: terminate condition is control == 0 && major status != 0?
                //if (dcw . fields . instr . control == 0)
                if (dcw . fields . instr . control == 0 &&
                    (stati & 04000) && // have status
                    ((stati & 03700) != 0))
                  {
//sim_printf ("cac terminate 8\n");
                    ptro = true;
                    sim_debug (DBG_TRACE, & iom_dev, "IDCW terminate\n");
                  }
#if 1 // this sends 20184 looping with no I/O
                if (dcw . fields . instr . control == 0 &&
                    (stati & 04000)) // have status
                  {
                    ptro = true;
                    sim_debug (DBG_TRACE, & iom_dev, "IDCW terminate\n");
//sim_printf ("cac terminate\n");
                  }
#endif

              }
//sim_printf ("cac terminate D\n");
            goto D;
          }
    
        // 4.3.1b: IDCW == NO
    
        // 4.3.1b: LPW 23 REL?
    
    // XXX missing the lpw.srel |= dcw.srel) somewhere???
    
        uint addr;
        if (dcw . type == ddcw)
          addr = dcw . fields . ddcw . daddr;
        else // we know it is not a idcw, so it must be a tdcw
          addr = dcw . fields . xfer . addr;
    
        if (lpw . srel != 0)
          {
            // 4.3.1b: LPW 23 REL == 1
            
            // 4.2.1b: BOUNDARY ERROR?
            if ((lpw . ae || pcwp -> ext))
              {
                uint sz = lpw . size;
                if (sz == 0)
                  {
                    sim_debug (DBG_INFO, & iom_dev, "%s: LPW size is zero; interpreting as 4096\n", __func__);
                    sz = 010000;    // 4096
                  }
                if (addr >= sz)
                  {
                    // 4.2.1b: BOUNDARY ERROR?
                    user_fault_flag = iom_cs_bndy_vio;
                    goto user_fault;
                  }
                // 4.2.1b: ABSOUTIZE ADDRESS
                addr = lpw . lbnd + addr;
              }
          }
    
        // 4.3.1b: TDCW?
    
        if (dcw . type == tdcw)
          {
            // 4.3.1b: TDCW == YES
    
            // 4.3.1b: SECOND TDCW?
            if (tdcw_count)
              {
                user_fault_flag = iom_cs_two_tdcws;
                goto user_fault;
              }
    
            // 4.3.1b: PUT ADDRESS IN LPW
            //         OR TDCW 33, 34, 35 INTO LPW 20, 18, 23
            //         DECREMENT TALLY
    
            lpw . dcw_ptr = addr;
            lpw . ae |= dcw . fields . xfer . ec;
            lpw . ires |= dcw . fields . xfer . i;
            lpw . srel |= dcw . fields . xfer . r;
            lpw . tally --;            
    
            // 4.3.1b: AC CHANGE ERROR?
            //         LPW 18 == 1 AND DCW 33 = 1
    
            if (lpw . ires && dcw . fields . xfer . ec)
              {
                // 4.3.1b: AC CHANGE ERROR == YES
                user_fault_flag = iom_cs_idcw_in_res_mode;
                goto user_fault;
              }
    
            // 4.3.1b: GOTO A
    
            first_list = false;
            goto A;
          }
    
        // 4.3.1b: TDCW == NO
    
        // 4.3.1b: CP VIOLATION?
    
        // 43A239854 3.2.3.3 "The byte size, defined by the channel, determines
        // what CP vaues are valid..."
    
        // XXX character position;
        // if (cp decrepancy)
        //   {
        //      user_fault_flag = iom_cs_cp_discrepancy;
        //      goto user_fault;
        //   }
    
    user_fault:
    
        if (user_fault_flag)
         {
           dcw . fields . ddcw . cp = 07;
           user_fault_flag = iom_cs_normal;
         }
    
        // 4.3.1b: SEND DCW TO CHANNEL
    
        // XXX SEND DCW TO CHANNEL
    
        int control = 0;

        // do_ddcw does the indirect list service; returns control = 0 if
        // IOTD (do I/O and disconnect), or 2 otherwise.
        // return code of 1 for no iom_io callback set, or result or iom_io
        // callback: 1 for internal error or data exhausted.
        ret = do_ddcw (iom_unit_num, chan, pcwp -> dev_code, addr, & dcw, & control, & stati);
        rcount = 0;
        residue = dcw . fields . ddcw . tally;
        char_pos = dcw . fields . ddcw . cp;

        if (control == 0)
          {
            ptro = true;
            sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (c)\n", __func__);
          }

        sim_debug (DBG_DEBUG, & iom_dev, "%s: do_ddcw returns %d\n", __func__, ret);
    
        // 4.3.1c: D
    D:;
    
        // 4.3.1c: SEND FLAGS TO CHANNEL
//XXX         channel_fault (chan);
    
        // XXX SEND FLAGS TO CHANNEL
    
        // 4.3.1c: LPW 21?
    
        if (lpw . nc == 0)
          {
    
            // 4.3.1c: LPW 21 == 0 (UPDATE)
    
            // 4.3.1c: UPDATE LPW ADDRESS AND TALLY
    
            -- lpw . tally;
            lpw . dcw_ptr ++;
    
          }
          
        // 4.3.1c: IDCW OR FIRST_LIST?
    
        if (dcw . type == idcw || first_list)
    
          {
            // 4.3.1c: IDCW OR FIRST_LIST == YES
    
            // 4.3.1c:  WRITE LPW AND LPWX INTO MAILBOXES (scratch and core)
    
            lpw_write (chan, chanloc, & lpw);
    
            goto end;
          }
        else
          {
            // 4.3.1c: IDCW OR FIRST_LIST == NO
    
            // TDCW?
    
            if (lpw . nc == 0 || dcw . type == tdcw)
              {
                // 4.3.1c:  WRITE LPW INTO MAILBOX (scratch and core)
    
                lpw_write (chan, chanloc, & lpw);
              }
          }
    end:

#if 0
// XXX I am guessing that one does status service after each dcw

        if (stati & 04000)
          {
             //status_service (iom_unit_num, chan, pcwp -> dev_code, stati, pcwp -> chan_data);
             //if (dcw . type != idcw) sim_printf ("faking chan_data\n");
// The docs indicate the tally residue should be used here, but that
// breaks t4d (badly).

#if 1
// this one works for 20184
             status_service (iom_unit_num, chan, pcwp -> dev_code, stati, dcw . type == idcw ? dcw . fields . instr . chan_data : 0);
#elif 0
// this one works for t4d
             status_service (iom_unit_num, chan, pcwp -> dev_code, stati, dcw . type == idcw ? dcw . fields . instr . chan_data : 1);
#else
// this one matches the docs
             status_service (iom_unit_num, chan, pcwp -> dev_code, stati, dcw . type == idcw ? dcw . fields . instr . chan_data : dcw . fields . ddcw . tally);
#endif
          }
#endif

        first_list = false;
      }
    //while (lpw . nc == 0 &&  ! (lpw . trunout && lpw . tally <= 0) && ! disconnect);
    while (! ptro || need_data);

    status_service (iom_unit_num, chan, pcwp -> dev_code, stati, rcount, residue, char_pos, is_read);
    //status_service (iom_unit_num, chan, pcwp -> dev_code, stati, pcwp -> dev_code, is_read);
    //sim_debug (DBG_DEBUG, & iom_dev, "%s: left list service; tally %d, disconnect %d\n", __func__, lpw . tally, disconnect);
    sim_debug (DBG_DEBUG, & iom_dev, "%s: left list service; tally %d\n",
               __func__, lpw . tally);
    send_terminate_interrupt (iom_unit_num, chan);

    return 0;
  }

static int send_flags_to_channel (void)
  {
    // XXX
    return 0;
  }

//-- static int do_connect_chan (int iom_unit_num)
//--   {
//--     // TODO: We don't allow a condition where it is possible to generate
//--     // channel status #1 "unexpected PCW (connect while busy)"
//--     
//--     // debug_connect_channel (iom_unit_num);
//--     iom_show_mbx (NULL, iom_unit + iom_unit_num, 0, "");
//-- 
//--     int ptro = 0;   // pre-tally-run-out, e.g. end of list
//--     int addr;
//--     int ret = 0;
//--     while (ptro == 0)
//--       {
//--         sim_debug (DBG_DEBUG, &iom_dev, "%s: Doing list service for Connect Channel\n", __func__);
//-- 
//--         ret = list_service(iom_unit_num, IOM_CONNECT_CHAN, 0, 1, &ptro, &addr);
//--         if (ret == 0)
//--           {
//--             sim_debug (DBG_DEBUG, & iom_dev, "%s: Return code zero from Connect Channel list service, doing pcwp\n", __func__);
//--             ret = send_channel_pcw(iom_unit_num, IOM_CONNECT_CHAN, 0, addr);
//--           }
//--         else
//--           {
//--             sim_debug (DBG_DEBUG, & iom_dev, "%s: Return code non-zero from Connect Channel list service, skipping pcwp\n", __func__);
//--             break;
//--           }
//--         // Note: list-service updates LPW in core -- (but has a BUG in
//--         // that it *always* writes.
//--         // BUG: Stop if tro system fault occured
//--       }
//--     return ret;
//--   }
//-- 
//-- 
//-- // ============================================================================
//-- 
//-- /*
//--  * get_chan()
//--  *
//--  * Return pointer to channel info.
//--  *
//--  * This is a wrapper for an implementation likely to change...
//--  *
//--  */
//-- 
//-- static channel_t* get_chan(int iom_unit_num, int chan, int dev_code)
//-- {
//--     //static channel_t channels[max_channels];
//--     
//--     if (chan < 0 || chan >= 040 || chan >= max_channels  ||
//--         dev_code < 0 || dev_code >= N_DEV_CODES) {
//--         // TODO: Would ill-ser-req be more appropriate?
//--         // Probably depends on whether caller is the iom and
//--         // is issuing a pcw or if the caller is a channel requesting svc
//--         iom_fault(iom_unit_num, chan, NULL, 1, iom_ill_chan);
//--         return NULL;
//--     }
//-- 
//--     return & iom [iom_unit_num]  . channels [chan] [dev_code] . channel_state;
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- /*
//--  * send_channel_pcw()
//--  *
//--  * Send a PCW (Peripheral Control Word) to a payload channel.
//--  *
//--  * Only called by do_connect_chan() just above.
//--  *
//--  * PCWs are retrieved by the connect channel and sent to "payload" channels.
//--  * This is the only way to initiate operation of any channel other than the
//--  * connect channel.
//--  *
//--  * The PCW indicates a physical channel and usually specifies a command to
//--  * be sent to the peripheral on that channel.
//--  *
//--  * Only the connect channel has lists that contain PCWs. (Other channels
//--  * use IDCWs (Instruction Data Control Words) to send commands to devices).
//--  *
//--  */
//-- 
//-- static int send_channel_pcw(int iom_unit_num, int chan, int dev_code, int addr)
//--   {
//--     sim_debug (DBG_DEBUG, &iom_dev, "%s: PCW for IOM %c chan %d, dev_code %d, addr %#o\n", __func__, 'A' + iom_unit_num, chan, dev_code, addr);
//-- 
//--     pcw_t pcw;
//--     word36 word0, word1;
//-- 
//--     (void) fetch_abs_pair(addr, &word0, &word1);
//--     decode_idcw(iom_unit_num, &pcw, 1, word0, word1);
//-- 
//--     sim_debug (DBG_INFO, & iom_dev, "%s: PCW is: %s\n", __func__, pcw2text(&pcw));
//--     
//--     // BUG/TODO: Should these be user faults, not system faults?
//--     
//--     if (pcw.chan < 0 || pcw.chan >= 040)
//--       {  // 040 == 32 decimal
//--         iom_fault(iom_unit_num, chan, "send channel pcw", 1, iom_ill_chan);
//--         return 1;
//--       }
//-- 
//--     if (pcw.cp != 07)
//--       {
//--         iom_fault(iom_unit_num, chan, "send channel pcw", 1, iom_not_pcw_conn);
//--         return 1;
//--       }
//--     
//--     if (pcw.mask)
//--       {
//--         // BUG: set mask flags for channel?
//--         sim_debug (DBG_ERR, & iom_dev, "%s: PCW Mask not implemented\n", __func__);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--       }
//-- 
//--     return activate_chan (iom_unit_num, pcw.chan, dev_code, &pcw);
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- /*
//--  * activate_chan()
//--  *
//--  * Send a PCW to a channel to start a sequence of operations.
//--  *
//--  * Called only by the connect channel's handle_pcw() just above.
//--  *
//--  * However, note that the channel being processed is the one specified
//--  * in the PCW, not the connect channel.
//--  *
//--  */
//-- 
//-- 
//-- static int activate_chan (int iom_unit_num, int chan, int dev_code, pcw_t* pcwp)
//--   {
//--     sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM '%c', channel %02o, dev_code %d\n",
//--       __func__, 'A' + iom_unit_num, chan, dev_code);
//-- 
//--     channel_t * chanp = get_chan (iom_unit_num, chan, dev_code);
//--     if (chanp == NULL)
//--         return 1;
//--     
//--     if (chanp -> state != chn_idle)
//--       {
//--         // Issue user channel fault #1 "unexpected PCW (connect while busy)"
//--         iom_fault (iom_unit_num, chan, "activate_chan", 0, 1);
//--         return 1;
//--       }
//--     
//--     // Devices used by the IOM must have a ctxt with devinfo.
//--     DEVICE* devp = iom [iom_unit_num] . channels [chan] [dev_code] . dev;
//--     if (devp != NULL)
//--       {
//--         chan_devinfo * devinfop = devp -> ctxt;
//--         if (devinfop == NULL)
//--           {
//--             devinfop = & chanp -> devinfo;
//--             devp -> ctxt = devinfop;
//--           }
//--         else if (devinfop != & chanp->devinfo)
//--           {
//--             sim_debug (DBG_ERR, & iom_dev, "%s: Channel %u dev_code %d and device mismatch with %d %d and %d %d\n", __func__, chan, dev_code, devinfop -> chan, devinfop -> dev_code, chanp -> devinfo . chan, chanp -> devinfo . dev_code);
//--             cancel_run (STOP_BUG);
//--           }
//--       }
//--     
//--     chanp -> n_list = 0;      // first list flag (and debug counter)
//--     chanp -> err = 0;
//--     chanp -> state = chn_pcw_rcvd;
//--     chanp -> xfer_running = 0;
//--     chanp -> payload = false; // Track if PCW or DCW
//--     // Receive the PCW
//--     chanp -> dcw . type = idcw;
//--     chanp -> dcw . fields . instr = * pcwp;
//--     
//-- #if 0
//--     // T&D tape does *not* expect us to cache original SCW, it expects us to
//--     // use the SCW which is memory at the time status is needed (this may
//--     // be a SCW value loaded from tape, not the value that was there when
//--     // we were invoked).
//--     unit chanloc = mbx_loc (iom_unit_num, chan);
//--     int scw = chanloc + 2;
//--     if (scw % 2 == 1) { // 3.2.4
//--          sim_debug (DBG_WARN, & iom_dev, "%s: SCW address 0%o is not even\n", __func__, scw);
//--         -- scw;         // force y-pair behavior
//--     }
//--     (void) fetch_abs_word(scw, &chanp->scw);
//--     sim_debug (DBG_DEBUG, & iom_dev, "%s: Caching SCW value %012llo from address %#o for channel %d.\n",
//--             __func__, chanp->scw, scw, chan);
//-- #endif
//--     
//--     // TODO: allow sim_activate on the channel instead of do_payload_channel()
//--     int ret = do_payload_channel (iom_unit_num, chanp);
//--     return ret;
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- 
//-- /*
//--  * do_payload_channel()
//--  *
//--  * Runs all the phases of a channel's operation back-to-back.  Terminates
//--  * when the channel is finished or when the channel queues an activity
//--  * via sim_activate().
//--  *
//--  */
//-- 
//-- static int do_payload_channel(int iom_unit_num, channel_t *chanp)
//-- {
//--     
//--     const int chan = chanp->chan;
//--     const int dev_code = chanp->dev_code;
//--     
//-- #if 0
//--     if (chanp->state != chn_pcw_rcvd) {
//--         sim_debug (DBG_ERR, & iom_dev, "%s: Channel isn't in pcw-rcvd.\n", __func__);
//--         cancel_run(STOP_BUG);
//--     }
//-- #endif
//--     
//--     /*
//--      * Now loop
//--      */
//--     
//--     int ret = 0;
//--     sim_debug (DBG_INFO, & iom_dev, "%s: Starting run_channel() loop.\n", __func__);
//--     for (;;) {
//--         // sim_debug (DBG_INFO, & iom_dev, "%s: Running channel.\n", __func__);
//--         if (run_channel(iom_unit_num, chan, dev_code) != 0) {
//--             // Often expected...
//--             sim_debug (DBG_NOTIFY, & iom_dev, "%s: Channel has non-zero return.\n", __func__);
//--         }
//--         if (chanp -> xfer_running) {
//--             sim_debug (DBG_INFO, & iom_dev, "%s: Channel has xfer_running set..\n", __func__);
//--         }
//--         if (chanp->state == chn_err) {
//--             sim_debug (DBG_WARN, & iom_dev, "%s: Channel is in an error state.\n", __func__);
//--             ret = 1;
//--             // Don't break -- we need to get status
//--         } else if (chanp->state == chn_idle) {
//--             sim_debug (DBG_INFO, & iom_dev, "%s: Channel is now idle.\n", __func__);
//--             break;
//--         } else if (chanp->err) {
//--             sim_debug (DBG_WARN, & iom_dev, "%s: Channel has error flag set.\n", __func__);
//--             ret = 1;
//--             // Don't break -- we need to get status
//--         } else if (chanp->state == chn_need_status) {
//--             sim_debug (DBG_INFO, & iom_dev, "%s: Channel needs to do a status service.\n", __func__);
//--         } else if (chanp->have_status) {
//--             sim_debug (DBG_INFO, & iom_dev, "%s: Channel has status from device.\n", __func__);
//--         } else {
//--             // activity should be pending
//--             break;
//--         }
//--     };
//--     
//--     // Note that the channel may have pending work.  If so, the
//--     // device will have set have_status false and will have queued an activity.
//--     // When the device activates, it'll queue a run for the channel.
//--     
//--     sim_debug (DBG_INFO, & iom_dev, "%s: Finished.\n", __func__);
//--     return ret;
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- static const char* chn_state_text(chn_state s)
//-- {
//--     static const char* states[] = {
//--         "idle", "pcw rcvd", "pcw sent", "pcw done", "cmd sent", "io sent", "need status svc", "err"
//--     };
//--     // enum is known to be valid
//--     // return (s >= 0 && s < ARRAY_SIZE(states)) ? states[s] : "unknown";
//--     return states [s];
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- static void print_chan_state(const char* moi, channel_t* chanp)
//-- {
//--     sim_debug (DBG_DEBUG, & iom_dev, "%s: Channel %d: dev_code = %d, state = %s (%d), have status = %c, err = %c; xfer-running = %c; n-svcs = %d.\n",
//--             __func__,
//--             chanp->chan,
//--             chanp->dev_code,
//--             chn_state_text(chanp->state),
//--             chanp->state,
//--             chanp->have_status ? 'Y' : 'N',
//--             chanp->err ? 'Y' : 'N',
//--             chanp->xfer_running ? 'Y' : 'N',
//--             chanp->n_list);
//--     // FIXME: Maybe dump chanp->lpw
//-- }
//-- // ============================================================================
//-- 
//-- /*
//--  * run_channel()
//--  *
//--  * Simulates the operation of a channel.  Channels run asynchrounsly from
//--  * the IOM central.  Calling this function represents performing one iteration
//--  * of the channel's cycle of operations.  This function will need to be called
//--  * several times before a channel is finished with a task.
//--  *
//--  * Normal usage is for the channel flags to initially be set for notificaton of
//--  * a PCW being sent from the IOM central (via the connect channel).  On the
//--  * first call, run_channel() will send the PCW to the device.
//--  *
//--  * The sending of the PCW and various other invoked operations may not complete
//--  * immediately.  On the second and subsequent calls, run_channel() will first
//--  * check the status of any previously queued but now complete device operation.
//--  * After the first call, the next few calls will perform list services and
//--  * dispatch DCWs.  The last call will be for a status service, after which the
//--  * channel will revert to an idle state.
//--  *
//--  * Called both by activate_channel->do_payload_channel() just after a CIOC instruction
//--  * and also called by channel_svc()->do_payload_channel() as queued operations
//--  * complete.
//--  *
//--  * Note that this function is *not* used to run the connect channel.  The
//--  * connect channel is handled as a special case by do_connect_chan().
//--  *
//--  * Note from 43A239854_600B_IOM_Spec_Jul75.pdf, 3.2.1:
//--  *    The payload channel never gets a PTRO [tally run out] indication.  It
//--  *    must determine when to stop requesting list service on the basis of
//--  *    information in DCW or on the basis of some external influence.
//--  *
//--  * This code is probably not quite correct; the nuances around the looping
//--  * controls may be wrong...
//--  *
//--  */
//-- 
//-- static int run_channel(int iom_unit_num, int chan, int dev_code)
//-- {
//--     sim_debug (DBG_INFO, & iom_dev, "%s: Starting for IOM %c channel %d (%#o) devcode %d\n", __func__, 'A' + iom_unit_num, chan, chan, dev_code);
//--     
//--     channel_t* chanp = get_chan(iom_unit_num, chan, dev_code);
//--     if (chanp == NULL)
//--         return 1;
//--     print_chan_state("run_channel", chanp);
//--     
//--     if (chanp->state == chn_idle && ! chanp->err) {
//--         sim_debug (DBG_WARN, & iom_dev, "%s: Channel %d is idle.\n", __func__, chan);
//--         cancel_run(STOP_WARN);
//--         return 0;
//--     }
//--     
//--     int first_list = chanp->n_list == 0;
//--     
//--     // =========================================================================
//--     
//--     /*
//--      * First, check the status of any prior command for error conditions
//--      */
//--     
//--     if (chanp->state == chn_cmd_sent || chanp->state == chn_err) {
//--         // Channel is busy.
//--         
//--         // We should not still be waiting on the attached device (we're not
//--         // re-invoked until status is available).
//--         // Nor should we be invoked for an idle channel.
//--         if (! chanp->have_status && chanp->state != chn_err && ! chanp->err) {
//--             sim_debug (DBG_WARN, & iom_dev, "%s: Channel %d activate, but still waiting on device.\n", __func__, chan);
//--             cancel_run(STOP_WARN);
//--             return 0;
//--         }
//--         
//--         // If the attached device has terminated operations, we'll need
//--         // to do a status service and finish off the current connection
//--         
//--         if (chanp->status.major != 0) {
//--             // Both failed DCW loops or a failed initial PCW are caught here
//--             sim_debug (DBG_INFO, & iom_dev, "%s: Channel %d reports non-zero major status; terminating DCW loop and performing status service.\n", __func__, chan);
//--             chanp->state = chn_need_status;
//--         } else if (chanp->err || chanp->state == chn_err) {
//--             sim_debug (DBG_NOTIFY, & iom_dev, "%s: Channel %d reports internal error; doing status.\n", __func__, chanp->chan);
//--             chanp->state = chn_need_status;
//--         } else if (chanp->control == 0 && ! first_list) {
//--             int is_idle;
//--             if (chanp->xfer_running) {
//--                 if (iom [iom_unit_num] .channels[chan][dev_code] . type != DEVT_DISK) {
//--                     is_idle = 0;
//--                     sim_debug (DBG_INFO, & iom_dev, "%s: Channel %d almost out of work, but non-DISK channels get another list svc for in-progress transfers.\n", __func__, chan);
//--                 } else {
//--                     is_idle = 1;
//--                     sim_debug (DBG_INFO, & iom_dev, "%s: Channel %d shows transfer in progress, but it's a disk channel, so it's deemed out of work.\n", __func__, chan);
//--                     cancel_run(STOP_BKPT /* STOP_IBKPT */);
//--                 }
//--             } else
//--                 is_idle = 1;
//--             if (is_idle) {
//--                 // no work left
//--                 // FIXME: Should we handle this case in phase two below (may affect marker
//--                 // interrupts) ?
//--                 sim_debug (DBG_INFO, & iom_dev, "%s: Channel %d out of work; doing status.\n", __func__, chanp->chan);
//--                 chanp->state = chn_need_status;
//--             }
//--         }
//-- 
//--         // BUG: enable.   BUG: enabling kills the boot...
//--         // CAC: enabling this prevents the status from being written.
//--         // CAC: I have a theory that the IOM doesn't write the status
//--         // on boot, and the IOX does. This explains why the boot program
//--         // checks the high bit of the status to decide if this is an
//--         // IOX, yet writing the status will set set the high bit.
//--         // Theory support: 43A239854 600B Engineering Product Spec.:
//--         //   3.7 CONNECT CHANNEL
//--         //      "... The connect channel does not interrupt or store status."
//--         // The only I/O operation is the simulated $CON to start the transfer.
//--         //   3.2.5 Channel and Device Status Words
//--         //      "When the IOM Central performs a status service for a
//--         //       channel, it will store two words into the y-pair defined
//--         //       by the SW for the channel.:
//-- 
//--         // OTOH, it does need to generate an interrupt! 
//--         // From mst_boot_label.alm:
//--         // " Assuming an MST with this program written on its label is 
//--         // " mounted and ready on the selected tape  drive,  the  IOM  (or  
//--         // " equivalant) hardwired  bootload  program will read in the first 
//--         // " record on the tape (the label record), starting at location 
//--         // " 30 (8) absolute. When the record  has  been  completely  read  
//--         // " in,  a terminate  interrupt  is  executed.
//-- 
//-- // [CAC] Setting to 0 allows the 'fake interrupt' in iom_boot to work
//-- // setting to 1 allows this code to do the terminate interrupt
//--         chanp->have_status = 1;  // we just processed it
//--         
//--         // If we reach this point w/o resetting the state to chn_need_status,
//--         // we're busy and the channel hasn't terminated operations, so we don't
//--         // need to do a status service.  We'll handle the non-terminal pcw/dcw
//--         // completion in phase two below.
//--     }
//--     
//--     // =========================================================================
//--     
//--     /*
//--      * First of four phases -- send any PCW command to the device
//--      */
//--     
//--     if (chanp->state == chn_pcw_rcvd) {
//--         sim_debug (DBG_INFO, & iom_dev, "%s: Received a PCW from connect channel.\n", __func__);
//--         chanp->control = chanp->dcw.fields.instr.control;
//--         pcw_t *p = &chanp->dcw.fields.instr;
//--         chanp->have_status = 0;
//--         int ret = dev_send_idcw(iom_unit_num, chan, dev_code, p);
//--         // Note: dev_send_idcw will either set state=chn_cmd_sent or do iom_fault()
//--         if (ret != 0) {
//--             sim_debug (DBG_NOTIFY, & iom_dev, "run_channel: Device on channel %d did not like our PCW -- non zero return.\n", __func__, chan);
//--             // dev_send_idcw() will have done an iom_fault() or gotten
//--             // a non-zero major code from a device
//--             // BUG: Put channel in a fault state or mask
//--             chanp->state = chn_err;
//--             return 1;
//--         }
//--         // FIXME: we could probably just do a return(0) here and skip the code below
//--         if (chanp->have_status) {
//--             sim_debug (DBG_INFO, & iom_dev, "%s: Device took PCW instantaneously...\n", __func__);
//--             if (chanp->state != chn_cmd_sent) {
//--                 sim_debug (DBG_WARN, & iom_dev, "%s: Bad state after sending PCW to channel.\n", __func__);
//--                 print_chan_state("run_channel", chanp);
//--                 chanp->state = chn_err;
//--                 cancel_run(STOP_BUG);
//--                 return 1;
//--             }
//--         } else {
//--             // The PCW resulted in a call to sim_activate().
//-- // XXX Uh; no it didn't we have sim_activate turned off....
//--             return 0;
//--         }
//--     }
//--     
//--     /*
//--      * Second of four phases
//--      *     The device didn't finish operations (and we didn't do a status
//--      *     service).
//--      *     The channel needs to loop requesting list service(s) and
//--      *     dispatching the resulting DCWs.  We'll handle one iteration
//--      *     of the looping process here and expect to be called later to
//--      *     handle any remaining iterations.
//--      */
//--     
//--     /*
//--      *  BUG: need to implement 4.3.3:
//--      *      Indirect Data Service says DCW is written back to the *mailbox* ?
//--      *      (Probably because the central can't find words in the middle
//--      *      of lists?)
//--      */
//--     
//--     /*
//--      * Need to do something re:
//--      * 4.3.3a At the end of an indirect data service, the IOM sends tally info
//--      * to the channel.
//--      */
//-- 
//--     
//--     
//--     if (chanp->state == chn_cmd_sent) {
//--         // 3. The connect channel PCW is treated differently by the CPI channel
//--         // and the PSI channel. The CPI channel does a store status. The.
//--         // PSI channel goes into startup.
//-- 
//--         // XXX chan here is wrong; I can't tell if it is from the PCW or the conect channel
//--         if (! chanp -> payload && 
//--             iom [iom_unit_num] . channels [chan] [dev_code] . ctype == chan_type_CPI &&
//--             chanp -> have_status)
//--           {
//-- sim_printf (">>>> setting need _status\n");
//--             chanp->state = chn_need_status;
//--             return 0;
//--           }
//-- sim_printf (">>>> not setting need _status\n");
//--         sim_debug (DBG_DEBUG, & iom_dev, "%s: In channel loop for state %s\n", __func__, chn_state_text(chanp->state));
//--         int ret = 0;
//--         
//--         int need_ls = chanp->control == 2 || first_list;
//--         if (! need_ls && chanp->xfer_running) {
//--             if (iom [iom_unit_num] .channels[chan][dev_code] . type != DEVT_DISK) {
//--                 sim_debug (DBG_INFO, & iom_dev, "%s: Doing a list service due to in-progess transfer on a non-DISK channel.\n", __func__);
//--                 need_ls = 1;
//--             } else
//--                 sim_debug (DBG_INFO, & iom_dev, "%s: Not doing a list service in spite of an in-progess transfer for a non-TAPE channel.\n", __func__);
//--         }
//--         if (need_ls) {
//--             // Do a list service
//--             chanp->xfer_running = 0; // TODO: move this to where transfers actually happen?
//--             int addr;
//--             sim_debug (DBG_DEBUG, & iom_dev, "%s: Asking for %s list service (svc # %d).\n", first_list ? "first" : "another", chanp->n_list + 1, __func__);
//--             if (list_service(iom_unit_num, chan, dev_code, first_list, NULL, &addr) != 0) {
//--                 ret = 1;
//--                 sim_debug (DBG_WARN, & iom_dev, "%s: List service indicates failure\n", __func__);
//--             } else {
//--                 ++ chanp->n_list;
//--                 sim_debug (DBG_DEBUG, & iom_dev, "%s: List service yields DCW at addr 0%o\n", __func__, addr);
//--                 chanp->control = -1;
//--                 // Send request to device
//--                 ret = do_dcw(iom_unit_num, chan, dev_code, addr, &chanp->control/*, &chanp->need_indir_svc */);
//--                 sim_debug (DBG_DEBUG, & iom_dev, "%s: Back from latest do_dcw (at %0o); control = %d; have-status = %d\n", __func__, addr, chanp->control, chanp->have_status);
//--                 if (ret != 0) {
//--                     sim_debug (DBG_NOTIFY, & iom_dev, "%s: do_dcw returns non-zero.\n", __func__);
//--                 }
//--             }
//--         } else if (chanp->control == 3) {
//--             // BUG: set marker interrupt and proceed (list service)
//--             // Marker interrupts indicate normal completion of
//--             // a PCW or IDCW
//--             // PCW control == 3
//--             // See also: 3.2.7, 3.5.2, 4.3.6
//--             // See also 3.1.3
//-- #if 0
//--             sim_debug (DBG_ERR, & iom_dev, "%s: Set marker not implemented\n", __func__);
//--             ret = 1;
//-- #else
//--             // Boot tape never requests marker interrupts...
//--             ret = send_marker_interrupt(iom_unit_num, chan);
//--             if (ret == 0) {
//--                 sim_debug (DBG_NOTIFY, & iom_dev, "%s: Asking for a list service due to set-marker-interrupt-and-proceed.\n", __func__);
//--                 chanp->control = 2;
//--             }
//-- #endif
//--         } else {
//--             sim_debug (DBG_ERR, & iom_dev, "%s: Bad PCW/DCW control, %d\n", __func__, chanp->control);
//--             cancel_run(STOP_BUG);
//--             ret = 1;
//--         }
//--         return ret;
//--     }
//--     
//--     // =========================================================================
//--     
//--     /*
//--      * Third and Fourth phases
//--      */
//--     
//--     if (chanp->state == chn_need_status) {
//--         int ret = 0;
//--         if (chanp->err || chanp->state == chn_err)
//--             ret = 1;
//--         /*
//--          * Third of four phases -- request a status service
//--          */
//--         // BUG: skip status service if system fault exists
//--         sim_debug (DBG_DEBUG, & iom_dev, "%s: Requesting Status service\n", __func__);
//--         status_service(iom_unit_num, chan, dev_code);
//--         
//--         /*
//--          * Fourth of four phases
//--          *
//--          * 3.0 -- Following the status service, the channel will request the
//--          * IOM to do a multiplex interrupt service.
//--          *
//--          */
//--         sim_debug (DBG_INFO, &iom_dev, "run_channel: Sending terminate interrupt.\n");
//--         if (send_terminate_interrupt(iom_unit_num, chan))
//--             ret = 1;
//--         chanp->state = chn_idle;
//--         // BUG: move setting have_status=0 to after early check for err
//--         chanp->have_status = 0; // we just processed it
//--         return ret;
//--     }
//--     
//--     return 0;
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- #if 0
//-- static int list_service_orig(int chan, int first_list, int *ptro)
//-- {
//--     
//--     // Flowchart 256K overflow checks relate to 18bit offset & tally incr
//--     // TRO -- tally runout, a fault (not matching CPU fault names)
//--     //     user fault: lpw tr0 sent to channel (sys fault lpw tr0 for conn chan)
//--     // PTRO -- pre-tally runout -- only used internally?
//--     // page 82 etc for status
//--     
//--     // addr upper bits -- from: PCW in extended GCOS mode, or IDCW for list
//--     // service (2.1.2)
//--     // paging mode given by PTP (page table ptr) in second word of PCW (for
//--     // IOM-B)
//--     // 3 modes: std gcos; ext gcos, paged
//--     // paged: all acc either paged or extneded gcos, not relative moe
//--     
//-- }
//-- #endif
//-- 
//-- 
//-- // ============================================================================
//-- 
//-- /*
//--  * list_service()
//--  *
//--  * Perform a list service for a channel.
//--  *
//--  * Examines the LPW (list pointer word).  Returns the 24-bit core address
//--  * of the next PCW or DCW.
//--  *
//--  * Called by do_connect_chan() for the connect channel or by run_channel()
//--  * for other channels.
//--  *
//--  * This code is probably not quite correct.  In particular, there appear
//--  * to be cases where the LPW will specify the next action to be taken,
//--  * but only a copy of the LPW in the IOM should be updated and not the
//--  * LPW in main core.
//--  *
//--  */
//-- 
//-- static int list_service(int iom_unit_num, int chan, int dev_code, int first_list, int *ptro, int *addrp)
//-- {
//--     // Core address of next PCW or DCW is returned in *addrp.  Pre-tally-runout
//--     // is returned in *ptro.
//--     
//--     uint chanloc = mbx_loc (iom_unit_num, chan);
//--     
//--     channel_t* chanp = get_chan(iom_unit_num, chan, dev_code);
//--     if (chanp == NULL) {
//--         return 1;   // we're faulted
//--     }
//--     lpw_t* lpwp = &chanp->lpw;
//--     
//--     *addrp = -1;
//--     sim_debug (DBG_DEBUG, &iom_dev, "list_service: Starting %s list service for LPW for channel %0o(%d dec) dev_code %d at addr %0o\n",
//--             (first_list) ? "first" : "another", chan, chan, dev_code, chanloc);
//--     // Load LPW from main memory on first list, otherwise continue to use scratchpad
//--     if (first_list)
//--         fetch_and_parse_lpw(lpwp, chanloc, chan == IOM_CONNECT_CHAN);
//--     sim_debug (DBG_DEBUG, &iom_dev, "list_service: LPW: %s\n", lpw2text(lpwp, chan == IOM_CONNECT_CHAN));
//--     
//--     if (lpwp->srel) {
//--         sim_debug (DBG_ERR, &iom_dev, "list_service: LPW with bit 23 (SREL) on is invalid for Multics mode\n");
//--         iom_fault(iom_unit_num, chan, "list_service", 1, 014);   // TODO: want enum
//--         cancel_run(STOP_BUG);
//--         return 1;
//--     }
//--     if (first_list) {
//--         lpwp->hrel = lpwp->srel;
//--     }
//--     if (lpwp->ae != lpwp->hrel) {
//--         sim_debug (DBG_WARN, &iom_dev, "list_service: AE does not match HREL\n");
//--         cancel_run(STOP_BUG);
//--     }
//--     
//--     // Check for TRO or PTRO at time that LPW is fetched -- not later
//--     
//--     if (ptro != NULL)
//--         *ptro = 0;
//--     int addr = lpwp->dcw;
//--     if (chan == IOM_CONNECT_CHAN) {
//--         if (lpwp->nc == 0 && lpwp->trunout == 0) {
//--             sim_debug (DBG_WARN, &iom_dev, "list_service: Illegal tally connect channel\n");
//--             iom_fault(iom_unit_num, chan, "list_service", 1, iom_ill_tly_cont);
//--             cancel_run(STOP_WARN);
//--             return 1;
//--         }
//--         if (lpwp->nc == 0 && lpwp->trunout == 1)
//--             if (lpwp->tally == 0) {
//--                 sim_debug (DBG_WARN, &iom_dev, "list_service: TRO on connect channel\n");
//--                 iom_fault(iom_unit_num, chan, "list_service", 1, iom_lpw_tro_conn);
//--                 cancel_run(STOP_WARN);
//--                 return 1;
//--             }
//--         if (lpwp->nc == 1) {
//--             // we're not updating tally, so pretend it's at zero
//--             if (ptro != NULL)
//--                 *ptro = 1;  // forced, see pg 23
//--         }
//--         *addrp = addr;  // BUG: force y-pair
//--         sim_debug (DBG_DEBUG, &iom_dev, "list_service: Expecting that connect channel will pull DCW from core\n");
//--     } else {
//--         // non connect channel
//--         // first, do an addr check for overflow
//--         int overflow = 0;
//--         if (lpwp->ae) {
//--             int sz = lpwp->size;
//--             if (lpwp->size == 0) {
//--                 sim_debug (DBG_INFO, &iom_dev, "list_service: LPW size is zero; interpreting as 4096\n");
//--                 sz = 010000;    // 4096
//--             }
//--             if (addr >= sz)     // BUG: was >
//--                 overflow = 1; // signal or record below
//--             else
//--                 addr = lpwp->lbnd + addr ;
//--         }
//--         // see flowchart 4.3.1b
//--         if (lpwp->nc == 0 && lpwp->trunout == 0) {
//--             if (overflow) {
//--                 iom_fault(iom_unit_num, chan, "list-service", 1, iom_256K_of);
//--                 return 1;
//--             }
//--         }
//--         if (lpwp->nc == 0 && lpwp->trunout == 1) {
//--             // BUG: Chart not fully handled (nothing after (C) except T-DCW detect)
//--             for (;;) {
//--                 if (lpwp->tally == 0) {
//--                     sim_debug (DBG_WARN, &iom_dev, "list_service: TRO on channel 0%o\n", chan);
//--                     iom_fault(iom_unit_num, chan, "list-service", 0, iom_cs_lpw_tro);
//--                     cancel_run(STOP_WARN);
//--                     // user fault, no return
//--                     break;
//--                 }
//--                 if (lpwp->tally > 1) {
//--                     if (overflow) {
//--                         iom_fault(iom_unit_num, chan, "list-service", 1, iom_256K_of);
//--                         return 1;
//--                     }
//--                 }
//--                 // Check for T-DCW
//--                 word36 word;
//--                 (void) fetch_abs_word(addr, &word);
//--                 int t = getbits36(word, 18, 3);
//--                 if (t == 2) {
//--                     uint next_addr = word >> 18;
//--                     sim_debug (DBG_ERR, &iom_dev, "list_service: Transfer-DCW not implemented; addr would be %06o; E,I,R = 0%llo\n", next_addr, word & 07);
//--                     return 1;
//--                 } else
//--                     break;
//--             }
//--         }
//--         *addrp = addr;
//--         // if in GCOS mode && lpwp->ae) fault;  // bit 20
//--         // next: channel should pull DCW from core
//--         sim_debug (DBG_DEBUG, &iom_dev, "list_service: Expecting that channel 0%o will pull DCW from core\n", chan);
//--     }
//--     
//--     word36 word;
//--     (void) fetch_abs_word(addr, &word);
//--     int cp = getbits36(word, 18, 3);
//--     if (cp == 7) {
//--         // BUG: update idcw fld of lpw
//--     }
//--     
//--     // int ret;
//--     
//--     //-------------------------------------------------------------------------
//--     // ALL THE FOLLOWING HANDLED BY PART "D" of figure 4.3.1b and 4.3.1c
//--     //          if (pcw.chan == IOM_CONNECT_CHAN) {
//--     //              sim_debug (DBG_DEBUG, &iom_dev, "list_service: channel does not return status.\n");
//--     //              return ret;
//--     //          }
//--     //          // BUG: need to write status to channel (temp: todo chan==036)
//--     // update LPW for chan (not 2)
//--     // update DCWs as used
//--     // SCW in mbx
//--     // last, send an interrupt (still 3.0)
//--     // However .. conn chan does not interrupt, store status, use dcw, or use
//--     // scw
//--     //-------------------------------------------------------------------------
//--     
//--     // Part "D" of 4.3.1c
//--     
//--     // BUG BUG ALL THE FOLLOWING IS BOTH CORRECT AND INCORRECT!!! Section 3.0
//--     // BUG BUG states that LPW for CONN chan is updated in core after each
//--     // BUG BUG chan is given PCW
//--     // BUG BUG Worse, below is prob for channels listed in dcw/pcw, not conn
//--     
//--     // BUG: No need to send channel flags?
//--     
//-- #ifndef QUIET_UNUSED
//--     int write_lpw = 0;
//--     int write_lpw_ext = 0;
//-- #endif
//--     int write_any = 1;
//--     if (lpwp->nc == 0) {
//--         if (lpwp->trunout == 1) {
//--             if (lpwp->tally == 1) {
//--                 if (ptro != NULL)
//--                     *ptro = 1;
//--             } else if (lpwp->tally == 0) {
//--                 write_any = 0;
//--                 if (chan == IOM_CONNECT_CHAN)
//--                     iom_fault(iom_unit_num, chan, "list-service", 1, iom_lpw_tro_conn);
//--                 else
//--                     iom_fault(iom_unit_num, chan, "list-service", 0, iom_cs_bndy_vio);  // BUG: might be wrong
//--             }
//--         }
//--         if (write_any) {
//--             -- lpwp->tally;
//--             if (chan == IOM_CONNECT_CHAN)
//--                 lpwp->dcw += 2; // pcw is two words
//--             else
//--                 ++ lpwp->dcw;       // dcw is one word
//--         }
//--     } else  {
//--         // note: ptro forced earlier
//--         write_any = 0;
//--     }
//--     
//-- #ifndef QUIET_UNUSED
//--     int did_idcw = 0;   // BUG
//--     int did_tdcw = 0;   // BUG
//--     if (lpwp->nc == 0) {
//--         write_lpw = 1;
//--         if (did_idcw || first_list)
//--             write_lpw_ext = 1;
//--     } else {
//--         // no update
//--         if (did_idcw || first_list) {
//--             write_lpw = 1;
//--             write_lpw_ext = 1;
//--         } else if (did_tdcw)
//--             write_lpw = 1;
//--     }
//-- #endif
//--     //if (pcw.chan != IOM_CONNECT_CHAN) {
//--     //  ; // BUG: write lpw
//--     //}
//--     lpw_write(chan, chanloc, lpwp);     // BUG: we always write LPW
//--     
//--     sim_debug (DBG_DEBUG, &iom_dev, "list_service: returning\n");
//--     return 0;   // BUG: unfinished
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- /*
//--  * do_dcw()
//--  *
//--  * Called by do_payload_channel() when a DCW (Data Control Word) is seen.
//--  *
//--  * DCWs may specify a command to be sent to a device or may specify
//--  * I/O transfer(s).
//--  *
//--  * *controlp will be set to 0, 2, or 3 -- indicates terminate,
//--  * proceed (request another list services), or send marker interrupt
//--  * and proceed
//--  */
//-- 
//-- static int do_dcw(int iom_unit_num, int chan, int dev_code, int addr, int *controlp/*, bool *need_indir_svc */)
//-- {
//--     sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c, chan %d, dev_code %d, addr 0%o\n", __func__, 'A' + iom_unit_num, chan, dev_code, addr);
//--     channel_t* chanp = get_chan(iom_unit_num, chan, dev_code);
//--     word36 word;
//--     (void) fetch_abs_word(addr, &word);
//--     if (word == 0) {
//--         sim_debug (DBG_ERR, & iom_dev, "%s: DCW of all zeros is legal but useless (unless you want to dump first 4K of memory).\n", __func__);
//--         sim_debug (DBG_ERR, & iom_dev, "%s: Disallowing legal but useless all zeros DCW at address %08o.\n", __func__, addr);
//--         cancel_run(STOP_BUG);
//--         if (chanp != NULL)
//--             chanp->state = chn_err;
//--         return 1;
//--     }
//--     dcw_t dcw;
//--     parse_dcw(iom_unit_num, chan, dev_code, &dcw, addr, 0);
//--     
//--     if (dcw.type == idcw) {
//--         // instr dcw
//--         dcw.fields.instr.chan = chan;   // Real HW would not populate
//--         chanp -> payload = true;
//--         // Payload (non connect?) channels don't look at the tally; whether
//--         // to continue doing list services or not is given by the control
//--         // words.  However, lists sometimes have an I-DCW with a control
//--         // of 0 (terminate at end of I/O op) but with an IO-DCW after said I-DCW
//--         sim_debug (DBG_INFO, & iom_dev, "%s: %s\n", __func__, dcw2text(&dcw));
//--         *controlp = dcw.fields.instr.control;
//--         int ret = dev_send_idcw(iom_unit_num, chan, dev_code, &dcw.fields.instr);
//--         if (ret != 0)
//--           {
//--             sim_debug (DBG_DEBUG, & iom_dev, "%s: dev-send-pcw returns %d.\n", __func__, ret);
//--           }
//--         if (dcw.fields.instr.chan_cmd != 02) {
//--             if (chanp != NULL && dcw.fields.instr.control == 0 && chanp->have_status && M[addr+1] == 0) {
//--                 // This is no longer seen
//--                 sim_debug (DBG_WARN, & iom_dev, "%s: Ignoring need to set xfer-running flag because next dcw is zero.\n", __func__);
//--                 // cancel_run(STOP_BUG);
//--                 cancel_run(STOP_BKPT /* STOP_IBKPT */);
//--             } else
//--                 chanp->xfer_running = 1;
//--         }
//--         return ret;
//--     } else if (dcw.type == tdcw) {
//--         uint next_addr = word >> 18;
//--         sim_debug (DBG_ERR, & iom_dev, "%s: Transfer-DCW not implemented; addr would be %06o; E,I,R = 0%llo\n", __func__, next_addr, word & 07);
//--         return 1;
//--     } else  if (dcw.type == ddcw) {
//--         // IOTD, IOTP, or IONTP -- i/o (non) transfer
//--         chanp -> payload = true;
//--         int ret = do_ddcw(iom_unit_num, chan, dev_code, addr, &dcw, controlp);
//--         return ret;
//--     } else {
//--         sim_debug (DBG_ERR, & iom_dev, "%s: Unknown DCW type\n", __func__);
//--         return 1;
//--     }
//-- }

/*
 * dev_send_idcw()
 *
 * Send a PCW (Peripheral Control Word) or an IDCW (Instruction Data Control
 * Word) to a device.   PCWs and IDCWs are typically used for sending
 * commands to devices but not for requesting I/O transfers between the
 * device and the IOM.  PCWs and IDCWs typically cause a device to move
 * data between a devices internal buffer and physical media.  DDCW I/O
 * transfer requests are used to move data between the IOM and the devices
 * buffers.  PCWs are only used by the connect channel; the other channels
 * use IDCWs.  IDCWs are essentially a one word version of the PCW.  A PCW
 * has a second word that only provides a channel number (which is used by
 * the connect channel to figure out which channel to send the PCW to).
 *
 * Note that we don't generate marker interrupts here; instead we
 * expect the caller to handle.
 *
 * See dev_io() below for handling of Data DCWS and I/O transfers.
 *
 * The various devices are implemented in other source files.  The IOM
 * expects two routines for each device: one to handle commands and
 * one to handle I/O transfers.
 *
 * Note: we always set chan_status.rcount to p->chan_data -- we don't
 * send/receive chan_data to/from any currently implemented devices...
 */

static int dev_send_idcw (int iom_unit_num, uint chan, uint dev_code, pcw_t * pcwp, word12 * stati, bool * need_data, bool * is_read)
  {
    sim_debug (DBG_INFO, & iom_dev,
               "%s: Starting for channel IOM %c, 0%o(%d), dev_code %d.  PCW: %s\n", 
               __func__, 'A' + iom_unit_num, chan, chan, dev_code, 
               pcw2text (pcwp));
    
    DEVICE* devp = iom [iom_unit_num] .channels [chan] [dev_code] . dev;
    if (devp == NULL)
      {
        // BUG: no device connected; what's the appropriate fault code(s) ?
//--         chanp->status.power_off = 1;
        * stati |= 06000; // have_status, power off
        sim_debug (DBG_WARN, & iom_dev,
                   "%s: No device connected to channel %#o(%d)\n",
                   __func__, chan, chan);
        iom_fault(iom_unit_num, chan, __func__, 0, 0);
        return 1;
    }
    
    if (pcwp->chan_data != 0)
      {
        sim_debug (DBG_INFO, & iom_dev, "%s: Chan data is %o (%d)\n",
                __func__, pcwp->chan_data, pcwp->chan_data);
      }

    iom_cmd * iom_cmd = iom [iom_unit_num] .channels [chan] [dev_code] . iom_cmd;
    if (! iom_cmd)
      {
        // BUG: no device connected; what's the appropriate fault code(s) ?
        * stati |= 03000; // have_status, power off
        sim_debug (DBG_ERR, & iom_dev,
                   "%s: iom_cmd on channel %#o (%d) dev_code %d is missing.\n",
                   __func__, chan, chan, dev_code);
//--             iom_fault(iom_unit_num, chan, "list-service", 0, 0);
//--             cancel_run(STOP_WARN);
        return 1;
      }

    UNIT * unitp = iom [iom_unit_num] .channels [chan] [dev_code] . board;

    * stati = 0;
    * need_data = false;
    * is_read = false;

    int rc = iom_cmd (unitp, pcwp, stati, need_data, is_read);

    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: iom_cmd returns rc:%d, stati:%04o\n",
               __func__, rc, * stati);

//--     int ret;
//--     switch(type) {
//--         case DEVT_NONE:
//--             // BUG: no device connected; what's the appropriate fault code(s) ?
//--             chanp->status.power_off = 1;
//--             sim_debug (DBG_WARN, & iom_dev, "%s: Device on channel %#o (%d) dev_code %d is missing.\n", __func__, chan, chan, dev_code);
//--             iom_fault(iom_unit_num, chan, "list-service", 0, 0);
//--             cancel_run(STOP_WARN);
//--             return 1;
//--         case DEVT_TAPE:
//--             ret = mt_iom_cmd(devinfop);
//--             break;
//--         case DEVT_CON: {
//--             ret = con_iom_cmd(pcwp->dev_cmd, &chanp->status.major, &chanp->status.substatus, &chanp->have_status);
//--             //chanp->state = chn_cmd_sent;
//--             //chanp->have_status = 1;     // FIXME: con_iom_cmd should set this
//--             chanp->status.rcount = pcwp->chan_data;
//--             sim_debug (DBG_DEBUG, & iom_dev, "CON returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
//--             //return ret; // caller must choose between our return and the chan_status.{major,substatus}
//--             break;
//--         }
//--         case DEVT_DISK:
//--             ret = disk_iom_cmd(devinfop);
//--             sim_debug (DBG_INFO, &iom_dev, "dev_send_idcw: DISK returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
//--             break;
//--         default:
//--             sim_debug (DBG_ERR, &iom_dev, "dev_send_idcw: Unknown device type 0%o\n", iom [iom_unit_num] .channels[chan][dev_code] .type);
//--             iom_fault(iom_unit_num, chan, "list-service", 1, 0); // BUG: need to pick a fault code
//--             cancel_run(STOP_BUG);
//--             return 1;
//--     }
//--     
//--     if (devinfop != NULL) {
//--         chanp->state = chn_cmd_sent;
//--         chanp->have_status = devinfop->have_status;
//--         if (devinfop->have_status) {
//--             // Device performed request immediately
//--             chanp->status.major = devinfop->major;
//--             chanp->status.substatus = devinfop->substatus;
//--             chanp->status.rcount = devinfop->chan_data;
//--             chanp->status.read = devinfop->is_read;
//--             sim_debug (DBG_DEBUG, &iom_dev, "dev_send_idcw: Device returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
//--         } else if (devinfop->time >= 0) {
//--             // Device asked us to queue a delayed status return.  FIXME: Should queue the work, not
//--             // the reporting.
//--             int si = sim_interval;
//--             if (sim_activate(chanp->unitp, devinfop->time) == SCPE_OK) {
//--                 sim_debug (DBG_DEBUG, &iom_dev, "dev_send_idcw: Sim interval changes from %d to %d.  Q count is %d.\n", si, sim_interval, sim_qcount());
//--                 sim_debug (DBG_DEBUG, &iom_dev, "dev_send_idcw: Device will be returning major code 0%o substatus 0%o in %d time units.\n", devinfop->major, devinfop->substatus, devinfop->time);
//--             } else {
//--                 chanp->err = 1;
//--                 sim_debug (DBG_ERR, &iom_dev, "dev_send_idcw: Cannot queue.\n");
//--             }
//--         } else {
//--             // BUG/TODO: allow devices to have their own queuing
//--             sim_debug (DBG_ERR, &iom_dev, "dev_send_idcw: Device neither returned status nor queued an activity.\n");
//--             chanp->err = 1;
//--             cancel_run(STOP_BUG);
//--         }
//--         return ret; // caller must choose between our return and the status.{major,substatus}
//--     }
//--     
    return rc;
  }

//-- /*
//--  * dev_io()
//--  *
//--  * Send an I/O transfer request to a device.
//--  *
//--  * Called only by do_ddcw() as part of handling data DCWs (data control words).
//--  * This function sends or receives a single word.  See do_ddcw() for full
//--  * details of the handling of data DCWs including looping over multiple words.
//--  *
//--  * See dev_send_idcw() above for handling of PCWS and non I/O command requests.
//--  *
//--  * The various devices are implemented in other source files.  The IOM
//--  * expects two routines for each device: one to handle commands and
//--  * one to handle I/O transfers.
//--  *
//--  * BUG: We return zero to do_ddcw() even after a failed transfer.   This
//--  * causes addresses and tallys to become incorrect.   For example, we
//--  * return zero when the console operator is "distracted". -- fixed
//--  */
//-- 
//-- static int dev_io(int iom_unit_num, int chan, int dev_code, word36 *wordp)
//-- {
//--     channel_t* chanp = get_chan(iom_unit_num, chan, dev_code);
//--     if (chanp == NULL)
//--         return 1;
//--     
//--     DEVICE* devp = iom [iom_unit_num] .channels[chan][dev_code] .dev;
//--     // if (devp == NULL || devp->units == NULL)
//--     if (devp == NULL) {
//--         // BUG: no device connected, what's the fault code(s) ?
//--         sim_debug (DBG_WARN, &iom_dev, "dev_io: No device connected to chan 0%o dev_code %d\n", chan, dev_code);
//--         chanp->status.power_off = 1;
//--         iom_fault(iom_unit_num, chan, "dev_io", 0, 0);
//--         cancel_run(STOP_WARN);
//--         return 1;
//--     }
//--     chanp->status.power_off = 0;
//--     
//--     switch(iom [iom_unit_num] .channels[chan][dev_code] .type) {
//--         case DEVT_NONE:
//--             // BUG: no device connected, what's the fault code(s) ?
//--             chanp->status.power_off = 1;
//--             iom_fault(iom_unit_num, chan, "dev_io", 0, 0);
//--             sim_debug (DBG_WARN, &iom_dev, "dev_io: Device on channel %#o (%d) dev_code %d is missing.\n", chan, chan, dev_code);
//--             cancel_run(STOP_WARN);
//--             return 1;
//--         case DEVT_TAPE: {
//--             int ret = mt_iom_io(iom_unit_num, chan, dev_code, wordp, &chanp->status.major, &chanp->status.substatus);
//--             if (ret != 0 || chanp->status.major != 0)
//--               {
//--                 sim_debug (DBG_DEBUG, &iom_dev, "dev_io: MT returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
//--               }
//--             return ret; // caller must choose between our return and the status.{major,substatus}
//--         }
//--         case DEVT_CON: {
//--             int ret = con_iom_io(chan, wordp, &chanp->status.major, &chanp->status.substatus);
//--             if (ret != 0 || chanp->status.major != 0)
//--               {
//--                 sim_debug (DBG_DEBUG, &iom_dev, "dev_io: CON returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
//--               }
//--             return ret; // caller must choose between our return and the status.{major,substatus}
//--         }
//--         case DEVT_DISK: {
//--             int ret = disk_iom_io(chan, wordp, &chanp->status.major, &chanp->status.substatus);
//--             // TODO: uncomment & switch to DEBUG: if (ret != 0 || chanp->status.major != 0)
//--             sim_debug (DBG_INFO, &iom_dev, "dev_io: DISK returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
//--             return ret; // caller must choose between our return and the status.{major,substatus}
//--         }
//--         default:
//--             sim_debug (DBG_ERR, &iom_dev, "dev_io: Unknown device type 0%o\n", iom [iom_unit_num] .channels[chan][dev_code] .type);
//--             iom_fault(iom_unit_num, chan, "dev I/O", 1, 0); // BUG: need to pick a fault code
//--             cancel_run(STOP_BUG);
//--             return 1;
//--     }
//--     return -1;  // not reached
//-- }
//-- 
//-- // ============================================================================

/*
 * do_ddcw()
 *
 * Process "data" DCWs (Data Control Words).   This function handles DCWs
 * relating to I/O transfers: IOTD, IOTP, and IONTP.
 *
 */

static int do_ddcw (int iom_unit_num, uint chan, uint dev_code, word24 addr, dcw_t *dcwp, int *control, word12 * stati)
  {
    sim_debug (DBG_DEBUG, & iom_dev, "%s: DDCW: %012llo: %s\n",
               __func__, M[addr], dcw2text(dcwp));
    
    // impossible for (cp == 7); see do_dcw
    
    // AE, the upper 6 bits of data addr (bits 0..5) from LPW
    // if rel == 0; abs; else absolutize & boundry check
    // BUG: Need to munge daddr
    
    uint type = dcwp->fields.ddcw.type;
    uint daddr = dcwp->fields.ddcw.daddr;
    uint tally = dcwp->fields.ddcw.tally;   // FIXME?
    uint cp = dcwp->fields.ddcw.cp;

    word36 word = 0;
// XXX CAC IOM should pass the DPS8M address, not the real memory address
// It passes the real memory address so that it can do both direct and
// indirect lisst service; fix the xxx_iom_io interface to support both
// services, and pass in the appropriate values (& of the direct word, dps8
// address of the indirect words
    word36 * wordp = (type == 3) ? & word : M + daddr;    // 2 impossible; see do_dcw
    if (type == 3 && tally != 1)
      {
        sim_debug (DBG_ERR, &iom_dev, "%s: Type is 3, but tally is %d\n",
                   __func__, tally);
      }
    int ret;
    if (tally == 0)
      {
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: Tally of zero interpreted as 010000(4096)\n",
                   __func__);
        tally = 4096;
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: I/O Request(s) starting at addr 0%o; tally = zero->%d, cp %o\n",
                   __func__, daddr, tally, cp);
      }
    else
      {
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: I/O Request(s) starting at addr 0%o; tally = %d, cp %o\n", 
                    __func__, daddr, tally, cp);
      }


    iom_io * iom_io = iom [iom_unit_num] .channels [chan] [dev_code] . iom_io;
    if (! iom_io)
      {
        // BUG: no device connected; what's the appropriate fault code(s) ?
        sim_debug (DBG_ERR, & iom_dev,
                   "%s: iom_cmd on channel %#o (%d) dev_code %d is missing.\n",
                   __func__, chan, chan, dev_code);
        return 1;
      }

    UNIT * unitp = iom [iom_unit_num] . channels [chan] [dev_code] . board;

    // Indirect List Service
    // The major status word is 4 bits; the 5th bit will be used here to
    // also track the power-off bit 
    ret = iom_io (unitp, chan, dev_code, & tally, & cp, wordp, stati);
    if (ret != 0)
      {
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: Device for chan 0%o(%d) dev_code %d returns non zero (out of band return)\n", 
                   __func__, chan, chan, dev_code);
      }
    // BUG: BUG: We increment daddr & tally even if device didn't do the
    // transfer, e.g. when the console operator is "distracted".  This
    // is because dev_io() returns zero on failed transfers
    // -- fixed in dev_io()
    // -- ++daddr;    // todo: remove from loop
    // -- if (type != 3)
    // --     ++wordp;
    // --     if (--tally <= 0)
    // --         break;

    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: Last I/O Request was to/from addr 0%o; tally now %d\n", 
               __func__,  daddr, tally);
    // set control ala PCW as method to indicate terminate or proceed
    if (type == 0)
     {
        // This DCW is an IOTD -- do I/O and disconnect.  So, we'll
        // return a code zero --  don't ask for another list service
        *control = 0;
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: DDCW disconnect\n", __func__);
      }
    else
     {
        // Tell caller to ask for another list service.
        // Guessing '2' for proceed -- only PCWs and IDCWS should generate
        // marker interrupts?
#if 0
        *control = 3;
#else
        *control = 2;
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: DDCW proceed\n", __func__);
#endif
      }

// According to 4.3.3a, these are inside the indirect list service

    // 4.3.3a UPDATE TALLY AND ADDR
    dcwp->fields.ddcw.daddr = daddr;
    dcwp->fields.ddcw.tally = tally;
    dcwp->fields.ddcw.cp = cp;

    // 4.4.3a: INDICATE TALLY INFORMATION TO CHANNEL

// XXX Isn't this 4.3.1c D: SEND FLAGS TO CHANNEL?
//XXX         channel_fault (chan, false);

   // update dcw
//-- #if 0
//--     // Assume that DCW is only in scratchpad (bootload_tape_label.alm rd_tape reuses same DCW on each call)
//--     M[addr] = setbits36(M[addr], 0, 18, daddr);
//--     M[addr] = setbits36(M[addr], 24, 12, tally);
//--     sim_debug (DBG_DEBUG, &iom_dev, "do_ddcw: Data DCW update: %012llo: addr=%0o, tally=%d\n", M[addr], daddr, tally);
//-- #endif
    return ret;
  }

/*
 * decode_idcw()
 *
 * Decode an idcw word or pcw word pair
 */

static void decode_idcw(int iom_unit_num, pcw_t *p, bool is_pcw, word36 word0, word36 word1)
{
    p->dev_cmd = getbits36(word0, 0, 6);
    p->dev_code = getbits36(word0, 6, 6);
    p->ext = getbits36(word0, 12, 6);
    p->cp = getbits36(word0, 18, 3);
    p->mask = getbits36(word0, 21, 1);
    p->control = getbits36(word0, 22, 2);
    p->chan_cmd = getbits36(word0, 24, 6);
    p->chan_data = getbits36(word0, 30, 6);
    if (is_pcw) {
        p->chan = getbits36(word1, 3, 6);
        uint x = getbits36(word1, 9, 27);
        if (x != 0 &&  unit_data [iom_unit_num] . config_sw_os != CONFIG_SW_MULTICS) {
            sim_debug (DBG_ERR, &iom_dev, "decode_idcw: Page Table Pointer for model IOM-B detected\n");
            cancel_run(STOP_BUG);
        }
    } else {
        p->chan = (uint)-1;
    }
}

/*
 * pcw2text()
 *
 * Display pcw_t
 */

static char * pcw2text (const pcw_t * p)
  {
    // WARNING: returns single static buffer
    static char buf[200];
    sprintf(buf, "[dev-cmd=0%o, dev-code=0%o, ext=0%o, mask=%d, ctrl=0%o, chan-cmd=0%o, chan-data=0%o, chan=0%o]",
            p->dev_cmd, p->dev_code, p->ext, p->mask, p->control, p->chan_cmd, p->chan_data, p->chan);
    return buf;
  }


/*
 * fetch_and_parse_dcw()
 *
 * Parse word at "addr" into a dcw_t.
 */

static void fetch_and_parse_dcw (int iom_unit_num, uint chan, dcw_t * p, uint32 addr, int __attribute__((unused)) read_only)
  {
// XXX do the read_only thang
// XXX ticket #4
    word36 word;

    sim_debug (DBG_DEBUG, & iom_dev, "%s: addr: 0%06o\n", __func__, addr);
    (void) fetch_abs_word (addr, & word);
    sim_debug (DBG_DEBUG, & iom_dev, "%s: dcw: 0%012llo\n", __func__, word);

    uint cp = getbits36 (word, 18, 3);
    if (cp == 7U)
      {
        p -> type = idcw;
        decode_idcw (iom_unit_num, & p -> fields . instr, 0, word, 0ll);
        p -> fields . instr . chan = chan; // Real HW would not populate
//--         if (p->fields.instr.mask && ! read_only) {
//--             // Bit 21 is extension control (EC), not a mask
//--        channel_t* chanp = get_chan(iom_unit_num, chan, dev_code);
//--             if (! chanp)
//--                 return;
//--             if (chanp->lpw.srel) {
//--                 // Impossible, SREL is always zero for multics
//--                 // For non-multics, this would be allowed and we'd check
//--                 sim_debug (DBG_ERR, &iom_dev, "parse_dcw: I-DCW bit EC set but the LPW SREL bit is also set.");
//--                 cancel_run(STOP_BUG);
//--                 return;
//--             }
//--             sim_debug (DBG_WARN, &iom_dev, "WARN: parse_dcw: Channel %d: Replacing LPW AE %#o with %#o\n", chan, chanp->lpw.ae, p->fields.instr.ext);
//--             chanp->lpw.ae = p->fields.instr.ext;
//--             cancel_run(STOP_BKPT /* STOP_IBKPT */);
//--         }
      }
    else
      {
        uint type = getbits36 (word, 22, 2);
        if (type == 2U)
          {
            // transfer
            p -> type = tdcw;
            p -> fields . xfer . addr = word >> 18;
            p -> fields . xfer . ec = (word >> 2) & 1;
            p -> fields . xfer . i = (word >> 1) & 1;
            p -> fields . xfer . r = word  & 1;
          }
        else
          {
            p -> type = ddcw;
            p -> fields . ddcw . daddr = getbits36 (word, 0, 18);
            p -> fields . ddcw . cp = cp;
            p -> fields . ddcw . tctl = getbits36 (word, 21, 1);
            p -> fields . ddcw . type = type;
            p -> fields . ddcw . tally = getbits36 (word, 24, 12);
          }
      }
  }

/*
 * dcw2text()
 *
 * Display a dcw_t
 *
 */

static char * dcw2text (const dcw_t * p)
  {
    // WARNING: returns single static buffer
    static char buf[200];
    if (p->type == ddcw)
      {
        uint dtype = p->fields.ddcw.type;
        const char* type =
        (dtype == 0) ? "IOTD" :
        (dtype == 1) ? "IOTP" :
        (dtype == 2) ? "transfer" :
        (dtype == 3) ? "IONTP" :
        "<illegal>";
        sprintf(buf, "D-DCW: type=%d(%s), addr=%06o, cp=0%o, tally=0%o(%d) tally-ctl=%d",
                dtype, type, p->fields.ddcw.daddr, p->fields.ddcw.cp, p->fields.ddcw.tally,
                p->fields.ddcw.tally, p->fields.ddcw.tctl);
      }
    else if (p->type == tdcw)
        sprintf(buf, "T-DCW: ...");
    else if (p->type == idcw)
        sprintf(buf, "I-DCW: %s", pcw2text(&p->fields.instr));
    else
        strcpy(buf, "<not a dcw>");
    return buf;
  }

//-- 
//-- /*
//--  * print_dcw()
//--  *
//--  * Display a DCS
//--  *
//--  */
//-- 
//-- #ifndef QUIET_UNUSED
//-- static char* print_dcw(t_addr addr)
//-- {
//--     // WARNING: returns single static buffer
//--     dcw_t dcw;
//--     parse_dcw(iom_unit_num, -1, dev_code, &dcw, addr, 1);
//--     return dcw2text(&dcw);
//-- }
//-- #endif
//-- 

/*
 * lpw2text()
 *
 * Display an LPW
 */

static char * lpw2text (const lpw_t * p, int conn)
  {
    // WARNING: returns single static buffer
    static char buf [180];
    sprintf(buf, "[dcw=0%o ires=%d hrel=%d ae=%d nc=%d trun=%d srel=%d tally=0%o]",
            p->dcw_ptr, p->ires, p->hrel, p->ae, p->nc, p->trunout, p->srel, p->tally);
    if (! conn)
      sprintf(buf+strlen(buf), " [lbnd=0%o size=0%o(%d) idcw=0%o]",
                p->lbnd, p->size, p->size, p->idcw);
    return buf;
  }

/*
 * fetch_and_parse_lpw()
 *
 * Parse the words at "addr" into a lpw_t.
 */

static void fetch_and_parse_lpw (lpw_t * p, uint addr, bool is_conn)
  {
    word36 word0;
    fetch_abs_word (addr, & word0);
    p ->  dcw_ptr = word0 >> 18;
    p -> ires = getbits36 (word0, 18, 1);
    p -> hrel = getbits36 (word0, 19, 1);
    p -> ae = getbits36 (word0, 20, 1);
    p -> nc = getbits36 (word0, 21, 1);
    p -> trunout = getbits36 (word0, 22, 1);
    p -> srel = getbits36 (word0, 23, 1);
    p -> tally = getbits36 (word0, 24, 12);    // initial value treated as unsigned
    // p->tally = bits2num (getbits36 (word0, 24, 12), 12);
    
    if (! is_conn)
      {
        // Ignore 2nd word on connect channel
        // following not valid for paged mode; see B15; but maybe IOM-B non existant
        word36 word1;
        fetch_abs_word (addr +1, & word1);
        p -> lbnd = getbits36 (word1, 0, 9);
        p -> size = getbits36 (word1, 9, 9);
        p -> idcw = getbits36 (word1, 18, 18);
      }
    else
      {
        p -> lbnd = (uint)-1;
        p -> size = (uint)-1;
        p -> idcw = (uint)-1;
      }
  }

//-- 
//-- /*
//--  * print_lpw()
//--  *
//--  * Display a LPW along with its channel number
//--  *
//--  */
//-- 
//-- #ifndef QUIET_UNUSED
//-- static char* print_lpw(t_addr addr)
//-- {
//--     lpw_t temp;
//--     uint chan = (addr - mbx_loc (iom_unit_num, chan)) / 4;
//--     fetch_and_parse_lpw(&temp, addr, chan == IOM_CONNECT_CHAN);
//--     static char buf[160];
//--     sprintf(buf, "Chan 0%o -- %s", chan, lpw2text(&temp, chan == IOM_CONNECT_CHAN));
//--     return buf;
//-- }
//-- #endif
//-- 

//
// lpw_write()
//
// Write an LPW into main memory
///

static int lpw_write (uint chan, word24 chanloc, const lpw_t * p)
  {
    sim_debug (DBG_DEBUG, & iom_dev, "lpw_write: Chan 0%o: Addr 0%o had %012llo %012llo\n", chan, chanloc, M [chanloc], M [chanloc + 1]);

    word36 word0 = 0;
    word0 = setbits36 (0, 0, 18, p -> dcw_ptr);
    word0 = setbits36 (word0, 18,  1, p -> ires);
    word0 = setbits36 (word0, 19,  1, p -> hrel);
    word0 = setbits36 (word0, 20,  1, p -> ae);
    word0 = setbits36 (word0, 21,  1, p -> nc);
    word0 = setbits36 (word0, 22,  1, p -> trunout);
    word0 = setbits36 (word0, 23,  1, p -> srel);
    word0 = setbits36 (word0, 24, 12, p -> tally);
    store_abs_word (chanloc, word0);
    
    int is_conn = chan == IOM_CONNECT_CHAN;
    if (! is_conn)
      {
        word36 word1 = setbits36 (0, 0, 9, p -> lbnd);
        word1 = setbits36(word1, 9, 9, p -> size);
        word1 = setbits36(word1, 18, 18, p -> idcw);
        store_abs_word (chanloc + 1, word1);
      }
    sim_debug (DBG_DEBUG, & iom_dev, "lpw_write: Chan 0%o: Addr 0%o now %012llo %012llo\n", chan, chanloc, M [chanloc], M [chanloc + 1]);
    return 0;
  }

//-- 
//-- #if 0
//-- 
//-- /*
//--  * send_chan_flags
//--  *
//--  * Stub
//--  */
//-- 
//-- static int send_chan_flags()
//-- {
//--     sim_debug (DBG_NOTIFY, &iom_dev, "send_chan_flags: send_chan_flags() unimplemented\n");
//--     return 0;
//-- }
//-- 
//-- #endif
//-- 

/*
 * status_service()
 *
 * Write status info into a status mailbox.
 *
 * BUG: Only partially implemented.
 * WARNING: The diag tape will crash because we don't write a non-zero
 * value to the low 4 bits of the first status word.  See comments
 * at the top of mt.c. [CAC] Not true. The IIOC writes those bits to
 * tell the bootloader code whether the boot came from an IOM or IIOC.
 * The connect channel does not write status bits. The disg tape crash
 * was due so some other issue.
 *
 */

static int status_service(int iom_unit_num, uint chan, uint dev_code, word12 stati, word6 rcount, word12 residue, word3 char_pos, bool is_read)
  {
    // See page 33 and AN87 for format of y-pair of status info
    
//--     channel_t* chanp = get_chan(iom_unit_num, chan, dev_code);
//--     if (chanp == NULL)
//--         return 1;

    // BUG: much of the following is not tracked
    
    word36 word1, word2;
    word1 = 0;
    //word1 = setbits36(word1, 0, 1, 1);
    //word1 = setbits36(word1, 1, 1, power_off);
    //word1 = setbits36(word1, 2, 4, major);
    //word1 = setbits36(word1, 6, 6, sub);
    word1 = setbits36(word1, 0, 12, stati);

    word1 = setbits36(word1, 12, 1, 1); // BUG: even/odd
    word1 = setbits36(word1, 13, 1, 1); // BUG: marker int
    word1 = setbits36(word1, 14, 2, 0);
    word1 = setbits36(word1, 16, 1, 0); // BUG: initiate flag
    word1 = setbits36(word1, 17, 1, 0);
#if 0
    // BUG: Unimplemented status bits:
    word1 = setbits36(word1, 18, 3, chan_status.chan_stat);
    word1 = setbits36(word1, 21, 3, chan_status.iom_stat);
    word1 = setbits36(word1, 24, 6, chan_status.addr_ext);
#endif
    word1 = setbits36(word1, 30, 6, rcount);
    
    word2 = 0;
#if 0
    // BUG: Unimplemented status bits:
    word2 = setbits36(word2, 0, 18, chan_status.addr);
    word2 = setbits36(word2, 21, 1, chanp->status.read);
    word2 = setbits36(word2, 22, 2, chan_status.type);
    word2 = setbits36(word2, 24, 12, chan_status.dcw_residue);
#endif
    word2 = setbits36(word2, 18, 3, char_pos);
    word2 = setbits36(word2, 21, 1, is_read);
    word2 = setbits36(word2, 24, 12, residue);
    
    // BUG: need to write to mailbox queue
    
    // T&D tape does *not* expect us to cache original SCW, it expects us to
    // use the SCW loaded from tape.
    
#if 1
    uint chanloc = mbx_loc (iom_unit_num, chan);
    word24 scw = chanloc + 2;
    word36 sc_word;
    fetch_abs_word (scw, & sc_word);
    word18 addr = getbits36 (sc_word, 0, 18);   // absolute
    // BUG: probably need to check for y-pair here, not above
    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: Writing status for chan %d dev_code %d to 0%o=>0%o\n",
               __func__, chan, dev_code, scw, addr);
    sim_debug (DBG_TRACE, & iom_dev,
               "Writing status for chan %d dev_code %d to 0%o=>0%o\n",
               chan, dev_code, scw, addr);
#else
    word36 sc_word = chanp->scw;
    int addr = getbits36(sc_word, 0, 18);   // absolute
    if (addr % 2 == 1) {    // 3.2.4
        sim_debug (DBG_WARN, &iom_dev, "status_service: Status address 0%o is not even\n", addr);
        // store_abs_pair() call below will fix address
    }
    sim_debug (DBG_DEBUG, &iom_dev, "status_service: Writing status for chan %d to %#o\n",
            chan, addr);
#endif
    sim_debug (DBG_DEBUG, & iom_dev, "%s: Status: 0%012llo 0%012llo\n",
               __func__, word1, word2);
    sim_debug (DBG_TRACE, & iom_dev, "Status: 0%012llo 0%012llo\n",
               word1, word2);
    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: Status: 0%04o, (12)e/o=%c, (13)marker=%c, (14..15)Z, 16(Z?), 17(Z)\n",
               __func__, stati,
            '1', // BUG
            'Y');   // BUG
    uint lq = getbits36 (sc_word, 18, 2);
    uint tally = getbits36 (sc_word, 24, 12);
#if 1
    if (lq == 3)
      {
        sim_debug (DBG_WARN, &iom_dev, "%s: SCW for channel %d has illegal LQ\n",
                __func__, chan);
        lq = 0;
      }
#endif
    store_abs_pair (addr, word1, word2);

    if (tally > 0 || (tally == 0 && lq != 0))
      {
        switch (lq)
          {
            case 0:
              // list
              if (tally != 0)
                {
                  addr += 2;
                  -- tally;
                }
              break;

            case 1:
              // 4 entry (8 word) queue
              if (tally % 8 == 1 || tally % 8 == -1)
                addr -= 8;
              else
                addr += 2;
              -- tally;
              break;

            case 2:
              // 16 entry (32 word) queue
              if (tally % 32 == 1 || tally % 32 == -1)
                addr -= 32;
              else
                addr += 2;
              -- tally;
              break;
          }

#if 0 // XXX CAC this code makes no sense
        if (tally < 0 && tally == - (1 << 11) - 1) // 12bits => -2048 .. 2047
          {
            sim_debug (DBG_WARN, & iom_dev,
                       "%s: Tally SCW address 0%o wraps to zero\n",
                       __func__, tally);
            tally = 0;
          }
#else
       // tally is 12 bit math
       if (tally & ~07777U)
          {
            sim_debug (DBG_WARN, & iom_dev,
                       "%s: Tally SCW address 0%o under/over flow\n",
                       __func__, tally);
            tally &= 07777;
          }
#endif
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: Updating SCW from: %012llo\n",
                   __func__, sc_word);
        sc_word = setbits36 (sc_word, tally, 24, 12);
        sc_word = setbits36 (sc_word, addr, 0, 18);
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s:                to: %012llo\n",
                   __func__, sc_word);
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s:                at: %06o\n",
                   __func__, scw);
        store_abs_word (scw, sc_word);
      }

    // BUG: update SCW in core
    return 0;
  }

//-- 
//-- /*
//--  * iom_fault()
//--  *
//--  * Handle errors internal to the IOM.
//--  *
//--  * BUG: Not implemented.
//--  *
//--  */

static void iom_fault (int iom_unit_num, uint chan, const char* who, int is_sys, int signal)
  {
    // TODO:
    // For a system fault:
    // Store the indicated fault into a system fault word (3.2.6) in
    // the system fault channel -- use a list service to get a DCW to do so
    // For a user fault, use the normal channel status mechanisms
    
    // sys fault masks channel
    
    // signal gets put in bits 30..35, but we need fault code for 26..29
    
    // BUG: mostly unimplemented
    
    if (who == NULL)
        who = "unknown";
    sim_debug (DBG_WARN, &iom_dev, "%s: Fault for IOM %c channel %d in %s: is_sys=%d, signal=%d\n", who, 'A' + iom_unit_num, chan, who, is_sys, signal);
    sim_debug (DBG_ERR, &iom_dev, "%s: Not setting status word.\n", who);
  }


#define AN70
#ifndef AN70  // [CAC] AN70-1 May84 disagrees

enum iom_imw_pics
  {
    // These are for bits 19..21 of an IMW address.  This is normally
    // the Interrupt Level from a Channel's Program Interrupt Service
    // Request.  We can deduce from the map in 3.2.7 that certain
    // special case PICs must exist; these are listed in this enum.
    // Note that the terminate pic of 011b concatenated with the IOM number
    // of zero 00b (two bits) yields interrupt number 01100b or 12 decimal.
    // Interrupt 12d has trap words at location 030 in memory.  This
    // is the location that the bootload tape header is written to.
    imw_overhead_pic = 1,   // IMW address ...001xx (where xx is IOM #)
    imw_terminate_pic = 3,  // IMW address ...011xx
    imw_marker_pic = 5,     // IMW address ...101xx
    imw_special_pic = 7     // IMW address ...111xx
  };

#else

/* From AN70-1 May84
 *  ... The IOM determines an interrupt
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
 *   3  3     3   3   3
 *   1  2     3   4   5
 *  ---------------------
 *  | pic | group | iom |
 *  -----------------------------
 *       2       1     2
 */

enum iom_imw_pics
  {
    imw_system_fault_pic = 0,
    imw_terminate_pic = 1,
    imw_marker_pic = 2,
    imw_special_pic = 3
  };

#endif

//-- /*
//--  * send_marker_interrupt()
//--  *
//--  * Send a "marker" interrupt to the CPU.
//--  *
//--  * Channels send marker interrupts to indicate normal completion of
//--  * a PCW or IDCW if the control field of the PCW/IDCW has a value
//--  * of three.
//--  */
//-- 
//-- #if 1
//-- static int send_marker_interrupt(int iom_unit_num, int chan)
//-- {
//--     return send_general_interrupt(iom_unit_num, chan, imw_marker_pic);
//-- }
//-- #endif

/*
 * send_terminate_interrupt()
 *
 * Send a "terminate" interrupt to the CPU.
 *
 * Channels send a terminate interrupt after doing a status service.
 *
 */

static int send_terminate_interrupt(int iom_unit_num, uint chan)
  {
    return send_general_interrupt (iom_unit_num, chan, imw_terminate_pic);
  }


/*
 * send_general_interrupt()
 *
 * Send an interrupt from the IOM to the CPU.
 *
 */

static int send_general_interrupt(int iom_unit_num, uint chan, enum iom_imw_pics pic)
  {
    uint imw_addr;
#ifdef AN70
    uint chan_group = chan < 32 ? 1 : 0;
    uint chan_in_group = chan & 037;
    uint interrupt_num = (uint) iom_unit_num | (chan_group << 2) | ((uint) pic << 3);
#else
    imw_addr = iom [iom_unit_num] . iom_num; // 2 bits
    imw_addr |= pic << 2;   // 3 bits
    uint interrupt_num = imw_addr;
#endif
    // Section 3.2.7 defines the upper bits of the IMW address as
    // being defined by the mailbox base address switches and the
    // multiplex base address switches.
    // However, AN-70 reports that the IMW starts at 01200.  If AN-70 is
    // correct, the bits defined by the mailbox base address switches would
    // have to always be zero.  We'll go with AN-70.  This is equivalent to
    // using bit value 0010100 for the bits defined by the multiplex base
    // address switches and zeros for the bits defined by the mailbox base
    // address switches.
    //imw_addr += 01200;  // all remaining bits
    uint pi_base = unit_data [iom_unit_num] . config_sw_multiplex_base_address & ~3;
#ifdef AN70
    imw_addr = (pi_base << 3) | interrupt_num;
#else
    uint iom = unit_data [iom_unit_num] . config_sw_multiplex_base_address & 3; // 2 bits; only IOM 0 would use vector 030
    imw_addr +=  (pi_base << 3) | iom;
#endif

    sim_debug (DBG_INFO, &iom_dev, "send_general_interrupt: IOM %c, channel %d (%#o), level %d; Interrupt %d (%#o).\n", 'A' + iom_unit_num, chan, chan, pic, interrupt_num, interrupt_num);
    word36 imw;
    (void) fetch_abs_word(imw_addr, &imw);
    // The 5 least significant bits of the channel determine a bit to be
    // turned on.
#ifdef AN70
    sim_debug (DBG_DEBUG, &iom_dev, "send_general_interrupt: IMW at %#o was %012llo; setting bit %d\n", imw_addr, imw, chan_in_group);
    imw = setbits36(imw, chan_in_group, 1, 1);
#else
    sim_debug (DBG_DEBUG, &iom_dev, "send_general_interrupt: IMW at %#o was %012llo; setting bit %d\n", imw_addr, imw, chan & 037);
    imw = setbits36(imw, chan & 037, 1, 1);
#endif
    sim_debug (DBG_INFO, &iom_dev, "send_general_interrupt: IMW at %#o now %012llo\n", imw_addr, imw);
    (void) store_abs_word(imw_addr, imw);
    
// XXX this should call scu_svc
// XXX Why on earth is this calling config_sw_bootload_port; we need to
// interrupy the SCU that generated the iom_interrupt.
// "The IOM accomplishes [interrupting]  by setting a predetermined program
// interrupt cell in the system controller that contains the IOM base address. 
// "The system controller then causes a program interrupt in a processor so
// that appropriate action can be taken by the software.
// I am taking this to mean that the interrupt is sent to the SCU containing
// the base address

    //uint bl_scu = unit_data [iom_unit_num] . config_sw_bootload_port;
    uint base = unit_data [iom_unit_num] . config_sw_iom_base_address;
    uint base_addr = base << 6; // 01400
    // XXX this is wrong; I believe that the SCU unit number should be
    // calculated from the Port Configuration Address Assignment switches
    // For now, however, the same information is in the CPU config. switches, so
    // this should result in the same values.
    int cpu_port_num = query_iom_scpage_map (iom_unit_num, base_addr);
    int scu_unit_num;
    if (cpu_port_num >= 0)
      scu_unit_num = query_scu_unit_num (ASSUME_CPU_0, cpu_port_num);
    else
      scu_unit_num = 0;
    // XXX Print warning
    if (scu_unit_num < 0)
      scu_unit_num = 0;
    return scu_set_interrupt ((uint)scu_unit_num, interrupt_num);
  }

static void iom_show_channel_mbx (int iom_unit_num, uint chan)
  {
    sim_printf ("    Channel %o:%o mbx\n", iom_unit_num, chan);
    uint chanloc = mbx_loc (iom_unit_num, chan);
    sim_printf ("    chanloc %06o\n", chanloc);
    lpw_t lpw;
    fetch_and_parse_lpw (& lpw, chanloc, chan == IOM_CONNECT_CHAN);
    lpw . hrel = lpw . srel;
    sim_printf ("    LPW at %06o: %s\n", chanloc, lpw2text (& lpw, chan == IOM_CONNECT_CHAN));
    
    uint32 addr = lpw . dcw_ptr;
    if (lpw . tally == 0)
      lpw . tally = 10;

    // This isn't quite right, but sufficient for debugging
    uint control = 2;
    for (int i = 0; i < (int) lpw.tally && control == 2; ++ i)
      {
        if (i > 4096)
          break;
        dcw_t dcw;
        fetch_and_parse_dcw (iom_unit_num, chan, & dcw, addr, 1);
        if (dcw . type == idcw)
          {
            sim_printf ("    IDCW %d at %06o : %s\n", i, addr, dcw2text (& dcw));
            control = dcw . fields . instr . control;
          }
        else if (dcw . type == tdcw)
          {
            sim_printf ("    TDCW %d at %06o: <transfer> -- not implemented\n", i, addr);
            break;
          }
        else if (dcw . type == ddcw)
          {
            sim_printf ("    DDCW %d at %06o: %s\n", i, addr, dcw2text (& dcw));
            control = dcw . fields . instr . control;
          }
        ++ addr;
        //if (control != 2)
          //{
            //if (i == lpw . tally)
              //sim_printf ("    -- end of list --\n");
            //else
              //sim_printf ("    -- end of list (because dcw control != 2) --\n");
          // }
      }
    sim_printf ("\n");
  }

static int iom_show_mbx (FILE * __attribute__((unused)) st, UNIT * uptr, int __attribute__((unused)) val, void * __attribute__((unused)) desc)
  {
    int iom_unit_num = IOM_UNIT_NUM (uptr);

    // show connect channel
    // show list
    //  ret = list_service(IOM_CONNECT_CHAN, 1, &ptro, &addr);
    //      ret = send_channel_pcw(IOM_CONNECT_CHAN, addr);
    
    uint chan = IOM_CONNECT_CHAN;
    uint chanloc = mbx_loc (iom_unit_num, chan);
    sim_printf ("\nConnect channel is channel %d at %#06o\n", chan, chanloc);
    lpw_t lpw;
    fetch_and_parse_lpw (& lpw, chanloc, chan == IOM_CONNECT_CHAN);
    lpw . hrel = lpw . srel;
    sim_printf ("LPW at %#06o: %s\n", chanloc, lpw2text (& lpw, chan == IOM_CONNECT_CHAN));
    
    uint32 addr = lpw . dcw_ptr;
    pcw_t pcw;
    word36 word0, word1;
    (void) fetch_abs_pair (addr, & word0, & word1);
    decode_idcw (iom_unit_num, & pcw, 1, word0, word1);
    sim_printf ("PCW at %#06o: %s\n", addr, pcw2text (& pcw));
    chan = pcw . chan;
    sim_printf ("Channel %#o (%d):\n", chan, chan);

    iom_show_channel_mbx (iom_unit_num, chan);
    addr += 2;  // skip PCW
    
    if (lpw . tally == 0)
      lpw . tally = 10;

    // This isn't quite right, but sufficient for debugging
    uint control = 2;
    for (int i = 0; i < (int) lpw.tally && control == 2; ++ i)
      {
        if (i > 4096)
          break;
        dcw_t dcw;
        fetch_and_parse_dcw (iom_unit_num, chan, & dcw, addr, 1);
        if (dcw . type == idcw)
          {
            //dcw.fields.instr.chan = chan; // Real HW would not populate
            sim_printf ("IDCW %d at %06o : %s\n", i, addr, dcw2text (& dcw));
            control = dcw . fields . instr . control;
          }
        else if (dcw . type == tdcw)
          {
            sim_printf ("TDCW %d at %06o: <transfer> -- not implemented\n", i, addr);
            control = dcw . fields . instr . control;
            break;
          }
        else if (dcw . type == ddcw)
          {
            sim_printf ("DDCW %d at %06o: %s\n", i, addr, dcw2text (& dcw));
            control = dcw . fields . instr . control;
          }
        ++ addr;
        // if (control != 2)
          // {
            // if (i == lpw . tally)
              // sim_printf ("-- end of list --\n");
            // else
              // sim_printf ("-- end of list (because dcw control != 2) --\n");
          // }
      }
    sim_printf ("\n");
    return 0;
  }

static t_stat iom_show_nunits (FILE * __attribute__((unused)) st, UNIT * __attribute__((unused)) uptr, int __attribute__((unused)) val, void * __attribute__((unused)) desc)
  {
    sim_printf("Number of IOM units in system is %d\n", iom_dev . numunits);
    return SCPE_OK;
  }

static t_stat iom_set_nunits (UNIT * __attribute__((unused)) uptr, int32 __attribute__((unused)) value, char * cptr, void * __attribute__((unused)) desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_IOM_UNITS_MAX)
      return SCPE_ARG;
    if (n > 2)
      sim_printf ("Warning: Multics supports 2 IOMs maximum\n");
    iom_dev . numunits = (uint32) n;
    return SCPE_OK;
  }

static t_stat iom_show_config(FILE *__attribute__((unused)) st, UNIT *uptr, int __attribute__((unused)) val, void *__attribute__((unused)) desc)
  {
    int unit_num = IOM_UNIT_NUM (uptr);
    if (unit_num < 0 || unit_num >= (int) iom_dev . numunits)
      {
        sim_debug (DBG_ERR, & iom_dev, "iom_show_config: Invalid unit number %d\n", unit_num);
        sim_printf ("error: invalid unit number %d\n", unit_num);
        return SCPE_ARG;
      }

    sim_printf ("IOM unit number %d\n", unit_num);
    struct unit_data * p = unit_data + unit_num;

    char * os = "<out of range>";
    switch (p -> config_sw_os)
      {
        case CONFIG_SW_STD_GCOS:
          os = "Standard GCOS";
          break;
        case CONFIG_SW_EXT_GCOS:
          os = "Extended GCOS";
          break;
        case CONFIG_SW_MULTICS:
          os = "Multics";
          break;
      }
    char * blct = "<out of range>";
    switch (p -> config_sw_bootload_card_tape)
      {
        case CONFIG_SW_BLCT_CARD:
          blct = "CARD";
          break;
        case CONFIG_SW_BLCT_TAPE:
          blct = "TAPE";
          break;
      }

    sim_printf("Allowed Operating System: %s\n", os);
    sim_printf("IOM Base Address:         %03o(8)\n", p -> config_sw_iom_base_address);
    sim_printf("Multiplex Base Address:   %04o(8)\n", p -> config_sw_multiplex_base_address);
    sim_printf("Bootload Card/Tape:       %s\n", blct);
    sim_printf("Bootload Tape Channel:    %02o(8)\n", p -> config_sw_bootload_magtape_chan);
    sim_printf("Bootload Card Channel:    %02o(8)\n", p -> config_sw_bootload_cardrdr_chan);
    sim_printf("Bootload Port:            %02o(8)\n", p -> config_sw_bootload_port);
    sim_printf("Port Address:            ");
    int i;
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %03o", p -> config_sw_port_addr [i]);
    sim_printf ("\n");
    sim_printf("Port Interlace:          ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> config_sw_port_interlace [i]);
    sim_printf ("\n");
    sim_printf("Port Enable:             ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> config_sw_port_enable [i]);
    sim_printf ("\n");
    sim_printf("Port Sysinit Enable:     ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> config_sw_port_sysinit_enable [i]);
    sim_printf ("\n");
    sim_printf("Port Halfsize:           ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> config_sw_port_halfsize [i]);
    sim_printf ("\n");
    sim_printf("Port Storesize:           ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> config_sw_port_storesize [i]);
    sim_printf ("\n");
    sim_printf("Boot skip:                %02o(8)\n", p -> boot_skip);
    
    return SCPE_OK;
  }

//
// set iom0 config=<blah> [;<blah>]
//
//    blah = iom_base=n
//           multiplex_base=n
//           os=gcos | gcosext | multics
//           boot=card | tape
//           tapechan=n
//           cardchan=n
//           scuport=n
//           port=n   // set port number for below commands
//             addr=n
//             interlace=n
//             enable=n
//             initenable=n
//             halfsize=n
//             storesize=n
//          bootskip=n // Hack: forward skip n records after reading boot record

static config_value_list_t cfg_os_list [] =
  {
    { "gcos", CONFIG_SW_STD_GCOS },
    { "gcosext", CONFIG_SW_EXT_GCOS },
    { "multics", CONFIG_SW_MULTICS },
    { NULL, 0 }
  };

static config_value_list_t cfg_boot_list [] =
  {
    { "card", CONFIG_SW_BLCT_CARD },
    { "tape", CONFIG_SW_BLCT_TAPE },
    { NULL, 0 }
  };

static config_value_list_t cfg_base_list [] =
  {
    { "multics", 014 },
    { "multics1", 014 }, // boot iom
    { "multics2", 020 },
    { "multics3", 024 },
    { "multics4", 030 },
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

static config_list_t iom_config_list [] =
  {
    /*  0 */ { "os", 1, 0, cfg_os_list },
    /*  1 */ { "boot", 1, 0, cfg_boot_list },
    /*  2 */ { "iom_base", 0, 07777, cfg_base_list },
    /*  3 */ { "multiplex_base", 0, 0777, NULL },
    /*  4 */ { "tapechan", 0, 077, NULL },
    /*  5 */ { "cardchan", 0, 077, NULL },
    /*  6 */ { "scuport", 0, 07, NULL },
    /*  7 */ { "port", 0, N_IOM_PORTS - 1, NULL },
    /*  8 */ { "addr", 0, 7, NULL },
    /*  9 */ { "interlace", 0, 1, NULL },
    /* 10 */ { "enable", 0, 1, NULL },
    /* 11 */ { "initenable", 0, 1, NULL },
    /* 12 */ { "halfsize", 0, 1, NULL },
    /* 13 */ { "store_size", 0, 7, cfg_size_list },

// Hacks

    /* 14 */ { "bootskip", 0, 100000, NULL }, // t4d testing hack (doesn't help)
    { NULL, 0, 0, NULL }
  };

static t_stat iom_set_config (UNIT * uptr, int32 __attribute__((unused)) value, char * cptr, void * __attribute__((unused)) desc)
  {
    int unit_num = IOM_UNIT_NUM (uptr);
    if (unit_num < 0 || unit_num >= (int) iom_dev . numunits)
      {
        sim_debug (DBG_ERR, & iom_dev, "iom_set_config: Invalid unit number %d\n", unit_num);
        sim_printf ("error: iom_set_config: invalid unit number %d\n", unit_num);
        return SCPE_ARG;
      }

    struct unit_data * p = unit_data + unit_num;

    static uint port_num = 0;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("iom_set_config", cptr, iom_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 

            case -1: // done
              break;

            case 0: // OS
              p -> config_sw_os = v;
              break;

            case 1: // BOOT
              p -> config_sw_bootload_card_tape = v;
              break;

            case 2: // IOM_BASE
              p -> config_sw_iom_base_address = v;
              break;

            case 3: // MULTIPLEX_BASE
              p -> config_sw_multiplex_base_address = v;
              break;

            case 4: // TAPECHAN
              p -> config_sw_bootload_magtape_chan = v;
              break;

            case 5: // CARDCHAN
              p -> config_sw_bootload_cardrdr_chan = v;
              break;

            case 6: // SCUPORT
              p -> config_sw_bootload_port = v;
              break;

            case 7: // PORT
              port_num = v;
              break;

#if 0
                // all of the remaining assume a valid value in port_num
                if (/* port_num < 0 || */ port_num > 7)
                  {
                    sim_debug (DBG_ERR, & iom_dev, "iom_set_config: cached PORT value out of range: %d\n", port_num);
                    sim_printf ("error: iom_set_config: cached PORT value out of range: %d\n", port_num);
                    break;
                  } 
#endif
            case 8: // ADDR
              p -> config_sw_port_addr [port_num] = v;
              break;

            case 9: // INTERLACE
              p -> config_sw_port_interlace [port_num] = v;
              break;

            case 10: // ENABLE
              p -> config_sw_port_enable [port_num] = v;
              break;

            case 11: // INITENABLE
              p -> config_sw_port_sysinit_enable [port_num] = v;
              break;

            case 12: // HALFSIZE
              p -> config_sw_port_halfsize [port_num] = v;
              break;

            case 13: // STORE_SIZE
              p -> config_sw_port_storesize [port_num] = v;
              break;

            case 14: // BOOTSKIP
              p -> boot_skip = v;
              break;

            default:
              sim_debug (DBG_ERR, & iom_dev, "iom_set_config: Invalid cfgparse rc <%d>\n", rc);
              sim_printf ("error: iom_set_config: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }

