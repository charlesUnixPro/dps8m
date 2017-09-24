/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//
// IOM rework, try # 6
//

/**
 * \file dps8_iom.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

// XXX when updating the LPW, the DCW_PTR points to the last IDCW, not the
// DDCW/TDCW

// XXX review all device command loops for marker interrupts
//
// Implementation notes:
//
// The IOM can be configured to operate in GCOS, Extended GCOS, and Paged modes.
// Only the Paged mode is supported.
//
// Backup list service not implemented
//
// Wraparound channal not implemented
// Snapshot channal not implemented
// Scratchpad Access channal not implemented

// Direct data service 

// Pg: B20 "Address Extension bits will be used for core selection when the 
//          "the IOM is [...] in Paged mode for a non-paged channel.

// Definitions:
//  unit number -- ambiguous: May indicate either the Multics 
//                 designation for IOMA, IOMB, etc. or the simh
//                 UNIT table offset.
//    unit_idx --  always refers to the simh unit table offset.
//
//

// Functional layout:

// CPU emulator:
//   CIOC command -> SCU cable routing to IOM emulator "iom_connect"
//
// IOM emulator:
//   iom_connect performs the connect channel operations of fetching
//   the PCW, parsing out the channel number and initial DCW.
//
//   The channel number and initial DCW are passed to doPayloadChannel
//
//   Using the cable tables, doPayloadChannel determines the handler and
//   unit_idx for that channel number and calls 
//     iom_unit [channel_number] . iomCmd (unit_idx, DCW)
//   It then walks the channel DCW list, passing DCWs to the handler. 
//


// Table 3.2.1 Updating of LPW address and tally and indication of PTRO and TRO
//
//  case   LPW 21   LPW 22  TALLY  UPDATE   INDICATE    INDICATE
//         NC       TALLY   VALUE  ADDR &   PTRO TO     TRO 
//                                 TALLY    CONN. CH.   FAULT
//    a.     0       0       any    yes       no         no
//    b.     0       1       > 2    yes       no         no
//                            1     yes       yes        no
//                            0     no        no         yes
//    c.     1       x       any    no        yes        no
//
// Case a. is GCOS common Peripheral Interface payloads channels;
// a tally of 0 is not a fault since the tally field will not be
// used for that purpose.
//
// Case b. is the configuration expected of standard operation with real-time
// applications and/or direct data channels.
//
// Case c. will be used normally only with the connect channel servicing CPI
// type payload channels. [CAC: "INDICATE PTRO TO CONN. CH." means that the
// connect channel should not call list service to obtain additional PCW's]
//
// If case a. is used with the connect channels, a "System Fault: Tally
// Control Error, Connect Channel" will be generated. Case c. will be
// used in the bootload program.




// LPW bits
//
//  0-17  DCW pointer
//  18    RES - Restrict IDCW's [and] changing AE flag
//  19    REL - IOM controlled image of REL bit
//  20    AE - Address extension flag
//  21    NC
//  22    TAL 
//  23    REL
//  24-35 TALLY
//

// LPWX (sometimes called LPWE)
//
//  0-8   Lower bound
//  9-17  Size
//  18-35 Pointer to DCW of most recent instruction
//

// PCW bits
//
// word 1
//   0-11 Channel information
//  12-17 Address extension
//  18-20 111
//  21    M
//  22-35 Channel Information
//
// word 2
//  36-38 SBZ
//  39-44 Channel Number
//  45-62 Page table ptr
//  63    PTP
//  64    PGE
//  65    AUX
//  66-21 not used


// IDCW
//
//   0-11 Channel information
//  12-17 Address extension
//  18-20 111
//  21    EC
//  22-35 Channel Information
//


// TDCW
//
// Paged, PCW 64 = 0
//
//   0-17 DCW pointer
//  18-20 Not 111
//  21    0
//  22    1
//  23    0
//  24-32 SBZ
//  33    EC
//  34    RES
//  35    REL
//
// Paged, PCW 64 = 1
//
//   0-17 DCW pointer
//  18-20 Not 111
//  21    0
//  22    1
//  23    0
//  24-30 SBZ
//  31    SEG
//  32    PDTA
//  33    PDCW
//  34    RES
//  35    REL
//

// Due to lack of documentation, chan_cmd is largely ignored
//
// iom_chan_control_words.incl.pl1
//
//   SINGLE_RECORD       init ("00"b3),
//   NONDATA             init ("02"b3),
//   MULTIRECORD         init ("06"b3),
//   SINGLE_CHARACTER    init ("10"b3)
//   
// bound_tolts_/mtdsim_.pl1
//
//    idcw.chan_cmd = "40"b3;			/* otherwise set special cont. cmd */
//
// bound_io_tools/exercise_disk.pl1
//
//   idcw.chan_cmd = INHIB_AUTO_RETRY;		/* inhibit mpc auto retries */
//   dcl     INHIB_AUTO_RETRY       bit (6) int static init ("010001"b);  // 021
//
// poll_mpc.pl1:
//  /* Build dcw list to get statistics from EURC MPC */
//  idcw.chan_cmd = "41"b3;			/* Indicate special controller command */
//  /* Build dcw list to get configuration and statistics from DAU MSP */
//  idcw.chan_cmd = "30"b3;                           /* Want list in dev# order */
//
// tape_ioi_io.pl1:
//   idcw.chan_cmd = "03"b3;			/* data security erase */
//   dcw.chan_cmd = "30"b3;			/* use normal values, auto-retry */


#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_scu.h"
#include "dps8_console.h"
#include "dps8_fnp2.h"

#define ASSUME_CPU_0 0

// Nomenclature
//
//  IDX index   refers to emulator unit
//  NUM         refers to the number that the unit is configured as ("IOMA,
//              IOMB,..."). Encoded in the low to bits of configSwMultiplexBaseAddress

// Default
#define N_IOM_UNITS 1

#define IOM_UNIT_IDX(uptr) ((uptr) - iomUnit)


static inline void iom_core_read (word24 addr, word36 *data, UNUSED const char * ctx)
  {
    * data = M [addr] & DMASK;
  }

static inline void iom_core_read2 (word24 addr, word36 *even, word36 *odd, UNUSED const char * ctx)
  {
    * even = M [addr ++] & DMASK;
    * odd =  M [addr]    & DMASK;
  }

static inline void iom_core_write (word24 addr, word36 data, UNUSED const char * ctx)
  {
    M [addr] = data & DMASK;
  }

static inline void iom_core_write2 (word24 addr, word36 even, word36 odd, UNUSED const char * ctx)
  {
    M [addr ++] = even;
    M [addr] =    odd;
  }

static t_stat iom_action (UNIT *up);

static UNIT iomUnit [/*IDX*/N_IOM_UNITS_MAX] =
  {
    { UDATA (iom_action, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (iom_action, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (iom_action, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (iom_action, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL }
  };

static t_stat iomShowMbx (FILE * st, UNIT * uptr, int val, const void * desc);
static t_stat iomShowConfig (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat iomSetConfig (UNIT * uptr, int value, const char * cptr, void * desc);
static t_stat iomShowUnits (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat iomSetUnits (UNIT * uptr, int value, const char * cptr, void * desc);
static t_stat iomReset (DEVICE * dptr);
static t_stat iomBoot (int unitNum, DEVICE * dptr);


static MTAB iomMod [] =
  {
    {
       MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_NC, /* mask */
      0,            /* match */
      "MBX",        /* print string */
      NULL,         /* match string */
      NULL,         /* validation routine */
      iomShowMbx, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      iomSetConfig,         /* validation routine */
      iomShowConfig, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      iomSetUnits, /* validation routine */
      iomShowUnits, /* display routine */
      "Number of IOM units in the system", /* value descriptor */
      NULL   // help string
    },
    {
      0, 0, NULL, NULL, 0, 0, NULL, NULL
    }
  };

static DEBTAB iomDT [] =
  {
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "TRACE", DBG_TRACE, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

DEVICE iom_dev =
  {
    "IOM",       /* name */
    iomUnit,     /* units */
    NULL,        /* registers */
    iomMod,      /* modifiers */
    N_IOM_UNITS, /* #units */
    10,          /* address radix */
    8,           /* address width */
    1,           /* address increment */
    8,           /* data radix */
    8,           /* data width */
    NULL,        /* examine routine */
    NULL,        /* deposit routine */
    iomReset,    /* reset routine */
    iomBoot,     /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    DEV_DEBUG,   /* flags */
    0,           /* debug control flags */
    iomDT,       /* debug flag names */
    NULL,        /* memory size change */
    NULL,        /* logical name */
    NULL,        // help
    NULL,        // attach help
    NULL,        // help context
    NULL,        // description
    NULL
  };

static t_stat bootSvc (UNIT * unitp);
static UNIT bootChannelUnit [N_IOM_UNITS_MAX] =
  {
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL}
  };

#define IOM_CONNECT_CHAN 2U
#define IOM_SPECIAL_STATUS_CHAN 6U

iomChanData_t iomChanData [/*IDX*/N_IOM_UNITS_MAX] [MAX_CHANNELS];



typedef enum iomStat_t
  {
    iomStatNormal = 0,
    iomStatLPWTRO = 1,
    iomStat2TDCW = 2,
    iomStatBoundaryError = 3,
    iomStatAERestricted = 4,
    iomStatIDCWRestricted = 5,
    iomStatCPDiscrepancy = 6,
    iomStatParityErr = 7
  } iomStat_t;


enum configSwOs_t
  {
    CONFIG_SW_STD_GCOS, 
    CONFIG_SW_EXT_GCOS,
    CONFIG_SW_MULTICS  // "Paged"
  };
    

// Boot device: CARD/TAPE;
enum configSwBlCT_t { CONFIG_SW_BLCT_CARD, CONFIG_SW_BLCT_TAPE };




typedef struct
  {
    // Configuration switches

    // Interrupt multiplex base address: 12 toggles
    word12 configSwIomBaseAddress;
            
    // Mailbox base aka IOM base address: 9 toggles
    // Note: The IOM number is encoded in the lower two bits
    word9 configSwMultiplexBaseAddress;
            
    // OS: Three position switch: GCOS, EXT GCOS, Multics
    enum configSwOs_t configSwOS; // = CONFIG_SW_MULTICS;

    // Bootload device: Toggle switch CARD/TAPE
    enum configSwBlCT_t configSwBootloadCardTape; // = CONFIG_SW_BLCT_TAPE; 

    // Bootload tape IOM channel: 6 toggles
    word6 configSwBootloadMagtapeChan; // = 0; 

    // Bootload cardreader IOM channel: 6 toggles
    word6 configSwBootloadCardrdrChan; // = 1;

    // Bootload: pushbutton

    // Sysinit: pushbutton

    // Bootload SCU port: 3 toggle AKA "ZERO BASE S.C. PORT NO"
    // "the port number of the SC through which which connects are to
    // be sent to the IOM
    word3 configSwBootloadPort; // = 0; 

    // 8 Ports: CPU/IOM connectivity
    // Port configuration: 3 toggles/port 
    // Which SCU number is this port attached to 
    uint configSwPortAddress [N_IOM_PORTS]; // = { 0, 1, 2, 3, 4, 5, 6, 7 }; 

    // Port interlace: 1 toggle/port
    uint configSwPortInterface [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Port enable: 1 toggle/port
    uint configSwPortEnable [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Port system initialize enable: 1 toggle/port // XXX What is this
    uint configSwPortSysinitEnable [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // Port half-size: 1 toggle/port // XXX what is this
    uint configSwPortHalfsize [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 }; 
    // Port store size: 1 8 pos. rotary/port
    uint configSwPortStoresize [N_IOM_PORTS]; // = { 0, 0, 0, 0, 0, 0, 0, 0 };

    // other switches:
    //   alarm disable
    //   test/normal
    iomStat_t iomStatus;

    uint invokingScuUnitNum; // the unit number of the SCU that did the connect.
  } iomUnitData_t;

static iomUnitData_t iomUnitData [N_IOM_UNITS_MAX];

typedef enum iomSysFaults_t
  {
    // List from 4.5.1; descr from AN87, 3-9
    iomNoFlt = 000,
    iomIllChanFlt = 001,    // PCW to chan with chan number >= 40
                            // or channel without scratchpad installed
    iomIllSrvReqFlt = 002,  // A channel requested a serice request code
                            // of zero, a channel number of zero, or
                            // a channel number >= 40
    // =003,                // Parity error scratchpad
    iomBndryVioFlt = 003,   // pg B36
    iom256KFlt = 004,       // 256K overflow -- address decremented to zero, 
                            // but not tally
    iomLPWTRPConnFlt = 005, // tally was zero for an update LPW (LPW bit 
                            // 21==0) when the LPW was fetched for the 
                            // connect channel
    iomNotPCWFlt = 006,     // DCW for conn channel had bits 18..20 != 111b
    iomCP1Flt = 007,        // DCW was TDCW or had bits 18..20 == 111b
    // = 010,               // CP/CS-BAD-DATA DCW fetch for a 9 bit channel
                            // contained an illegal character position
    // = 011,               // NO-MEM-RES No response to an interrupt from
                            // a system controller within 16.5 usec.
    // = 012,               // PRTY-ERR-MEM Parity error on the read
                            // data when accessing a system controller.
    iomIllTalCChnFlt = 013, // LPW bits 21-22 == 00 when LPW was fetched 
                            // for the connect channel
    // = 014,               // PTP-Fault: PTP-Flag= zero or PTP address
                            // overflow.
    iomPTWFlagFault = 015,  // PTW-Flag-Fault: Page Present flag zero, or
                            // Write Control Bit 0, or Housekeeping bit set,
    // = 016,               // ILL-LPW-STD LPW had bit 20 on in GCOS mode
    // = 017,               // NO-PRT-SEL No port selected during attempt
                            // to access memory.
  } iomSysFaults_t;

typedef enum iomFaultServiceRequest
  { 
    // Combined SR/M/D
    iomFsrFirstList =  (1 << 2) | 2,
    iomFsrList =       (1 << 2) | 0,
    iomFsrStatus =     (2 << 2) | 0,
    iomFsrIntr =       (3 << 2) | 0,
    iomFsrSPIndLoad =  (4 << 2) | 0,
    iomFsrDPIndLoad =  (4 << 2) | 1,
    iomFsrSPIndStore = (5 << 2) | 0,
    iomFsrDPIndStore = (5 << 2) | 1,
    iomFsrSPDirLoad =  (6 << 2) | 0,
    iomFsrDPDirLoad =  (6 << 2) | 1,
    iomFsrSPDirStore = (7 << 2) | 0,
    iomFsrDPDirStore = (7 << 2) | 1
  } iomFaultServiceRequest;

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

enum iomImwPics
  {
    imwSystemFaultPic = 0,
    imwTerminatePic = 1,
    imwMarkerPic = 2,
    imwSpecialPic = 3
  };

static int send_general_interrupt (uint iomUnitIdx, uint chan, enum iomImwPics pic);

// Map memory to port
// -1 -- no mapping
// iomScbankMap is indexed by IDX because the data are
// based on the configuration switches associated with
// the physical IOM

static int iomScbankMap [/*IDX*/N_IOM_UNITS_MAX] [N_SCBANKS];

static void setupIOMScbankMap (void)
  {
    sim_debug (DBG_DEBUG, & cpu_dev,
      "%s: setupIOMScbankMap: SCBANK %d N_SCBANKS %d MEM_SIZE_MAX %d\n", 
      __func__, SCBANK, N_SCBANKS, MEM_SIZE_MAX);

    for (uint iomUnitIdx = 0; iomUnitIdx < iom_dev . numunits; iomUnitIdx ++)
      {
        // Initalize to unmapped
        for (int pg = 0; pg < (int) N_SCBANKS; pg ++)
          iomScbankMap [iomUnitIdx] [pg] = -1;
    
        iomUnitData_t * p = iomUnitData + iomUnitIdx;
        // For each port (which is connected to a SCU
        for (int port_num = 0; port_num < N_IOM_PORTS; port_num ++)
          {
            if (! p -> configSwPortEnable [port_num])
              continue;
            // Calculate the amount of memory in the SCU in words
            uint store_size = p -> configSwPortStoresize [port_num];
#ifdef DPS8M
            uint store_table [8] = 
              { 32768, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304 };
#endif
#ifdef L68
            uint store_table [8] = 
              { 32768, 65536, 4194304, 131072, 524288, 1048576, 2097152, 262144 };
#endif
            //uint sz = 1 << (store_size + 16);
            uint sz = store_table [store_size];
    
            // Calculate the base address of the memory in words
            uint assignment = p -> configSwPortAddress [port_num];
            //uint assignment = cpu.switches.assignment [port_num];
            uint base = assignment * sz;
    
            // Now convert to SCBANKs
            sz = sz / SCBANK;
            base = base / SCBANK;
    
            sim_debug (DBG_DEBUG, & cpu_dev,
              "%s: unit:%u port:%d ss:%u as:%u sz:%u ba:%u\n",
              __func__, iomUnitIdx, port_num, store_size, assignment, sz, 
              base);
    
            for (uint pg = 0; pg < sz; pg ++)
              {
                uint scpg = base + pg;
                if (/*scpg >= 0 && */ scpg < N_SCBANKS)
                  iomScbankMap [iomUnitIdx] [scpg] = port_num;
              }
          }
      }
    for (uint iomUnitIdx = 0; iomUnitIdx < iom_dev . numunits; iomUnitIdx ++)
        for (int pg = 0; pg < (int) N_SCBANKS; pg ++)
          sim_debug (DBG_DEBUG, & cpu_dev, "%s: %d:%d\n", 
            __func__, pg, iomScbankMap [iomUnitIdx] [pg]);
  }

#if 0   
static int queryIomScbankMap (uint iomUnitIdx, word24 addr)
  {
    uint scpg = addr / SCBANK;
    if (scpg < N_SCBANKS)
      return iomScbankMap [iomUnitIdx] [scpg];
    return -1;
  }
#endif

static uint mbxLoc (uint iomUnitIdx, uint chan)
  {
// IDX is correct here as computation is based on physical unit 
// configuration switches
    word12 base = iomUnitData [iomUnitIdx] . configSwIomBaseAddress;
    word24 base_addr = ((word24) base) << 6; // 01400
    word24 mbx = base_addr + 4 * chan;
    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c, chan %d is %012o\n",
      __func__, 'A' + iomUnitIdx, chan, mbx);
    return mbx;
  }



/*
 * status_service ()
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

// According to gtss_io_status_words.incl.pl1
//
//,     3 WORD1
//,       4 Termination_indicator         bit(01)unal
//,       4 Power_bit                     bit(01)unal
//,       4 Major_status                  bit(04)unal
//,       4 Substatus                     bit(06)unal
//,       4 PSI_channel_odd_even_ind      bit(01)unal
//,       4 Marker_bit_interrupt          bit(01)unal
//,       4 Reserved                      bit(01)unal
//,       4 Lost_interrupt_bit            bit(01)unal
//,       4 Initiate_interrupt_ind        bit(01)unal
//,       4 Abort_indicator               bit(01)unal
//,       4 IOM_status                    bit(06)unal
//,       4 Address_extension_bits        bit(06)unal
//,       4 Record_count_residue          bit(06)unal
//
//,      3 WORD2
//,       4 Data_address_residue          bit(18)unal
//,       4 Character_count               bit(03)unal
//,       4 Read_Write_control_bit        bit(01)unal
//,       4 Action_code                   bit(02)unal
//,       4 Word_count_residue            bit(12)unal

// iom_stat.incl.pl1
//
// (2 t bit (1),             /* set to "1"b by IOM */
//  2 power bit (1),         /* non-zero if peripheral absent or power off */
//  2 major bit (4),         /* major status */
//  2 sub bit (6),           /* substatus */
//  2 eo bit (1),            /* even/odd bit */
//  2 marker bit (1),        /* non-zero if marker status */
//  2 soft bit (2),          /* software status */
//  2 initiate bit (1),      /* initiate bit */
//  2 abort bit (1),         /* software abort bit */
//  2 channel_stat bit (3),  /* IOM channel status */
//  2 central_stat bit (3),  /* IOM central status */
//  2 mbz bit (6),
//  2 rcount bit (6),        /* record count residue */
//
//  2 address bit (18),      /* DCW address residue */
//  2 char_pos bit (3),      /* character position residue */
//  2 r bit (1),             /* non-zero if reading */
//  2 type bit (2),          /* type of last DCW */
//  2 tally bit (12)) unal;  /* DCW tally residue */

// Searching the Multics source indicates that
//   eo is  used by tape
//   marker is used by tape and printer
//   soft is set by tolts as /* set "timeout" */'
//   soft is reported by tape, but not used.
//   initiate is used by tape and printer
//   abort is reported by tape, but not used
//   char_pos is used by tape, console   

// tape_ioi_io.pl1  "If the initiate bit is set in the status, no data was 
//                   transferred (no tape movement occurred)."

// pg B26: "The DCW residues stored in the status in page mode will
// represent the next absoulute address (bits 6-23) of data prior to
// the application of the Page Table Word"

static int status_service (uint iomUnitIdx, uint chan, bool marker)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    // See page 33 and AN87 for format of y-pair of status info
    
    // BUG: much of the following is not tracked
    
    word36 word1, word2;
    word1 = 0;
    putbits36_12 (& word1, 0, p -> stati);
    // isOdd can be set to zero; see 
    //   http://ringzero.wikidot.com/wiki:cac-2015-10-22
    //putbits36_1 (& word1, 12, p -> isOdd ? 0 : 1);
    putbits36_1 (& word1, 13, marker ? 1 : 0);
    putbits36_2 (& word1, 14, 0); // software status
    putbits36_1 (& word1, 16, p -> initiate ? 1 : 0);
    putbits36_1 (& word1, 17, 0); // software abort bit
    putbits36_3 (& word1, 18, p -> chanStatus);
    putbits36_3 (& word1, 21, iomUnitData [iomUnitIdx] . iomStatus);
#if 0
    // BUG: Unimplemented status bits:
    putbits36_6 (& word1, 24, chan_status.addr_ext);
#endif
    putbits36_6 (& word1, 30, p -> recordResidue);
    
    word2 = 0;
#if 0
    // BUG: Unimplemented status bits:
    putbits36_18 (& word2, 0, chan_status.addr);
    putbits36_2 (& word2, 22, chan_status.type);
#endif
    putbits36_3 (& word2, 18, p -> charPos);
    putbits36_1 (& word2, 21, p -> isRead ? 1 : 0);
    putbits36_12 (& word2, 24, p -> tallyResidue);
    
    // BUG: need to write to mailbox queue
    
    // T&D tape does *not* expect us to cache original SCW, it expects us to
    // use the SCW loaded from tape.
    
    uint chanloc = mbxLoc (iomUnitIdx, chan);
    word24 scwAddr = chanloc + 2;
    word36 scw;
    iom_core_read (scwAddr, & scw, __func__);
    sim_debug (DBG_DEBUG, & iom_dev,
               "SCW chan %02o %012"PRIo64"\n", chan, scw);
    word18 addr = getbits36_18 (scw, 0);   // absolute
    uint lq = getbits36_2 (scw, 18);
    uint tally = getbits36_12 (scw, 24);

    sim_debug (DBG_DEBUG, & iom_dev, "%s: Status tally %d (%o) lq %o\n",
               __func__, tally, tally, lq);
    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: Writing status for chan %d dev_code %d to 0%o=>0%o\n",
               __func__, chan, p -> dev_code, scwAddr, addr);
    sim_debug (DBG_TRACE, & iom_dev,
               "Writing status for chan %d dev_code %d to 0%o=>0%o\n",
               chan, p -> dev_code, scwAddr, addr);
    sim_debug (DBG_DEBUG | DBG_TRACE, & iom_dev, "%s: Status: 0%012"PRIo64" 0%012"PRIo64"\n",
               __func__, word1, word2);
    if (lq == 3)
      {
        sim_debug (DBG_WARN, &iom_dev, 
                   "%s: SCW for channel %d has illegal LQ\n",
                   __func__, chan);
        lq = 0;
      }
//sim_printf ("status %d %08o %012"PRIo64" %012"PRIo64"\n", chan, addr, word1, word2);
    iom_core_write2 (addr, word1, word2, __func__);

    if (tally > 0 || (tally == 0 && lq != 0))
      {
        switch (lq)
          {
            case 0:
              // list
              if (tally != 0)
                {
                  tally --;
                  addr += 2;
                }
              break;

            case 1:
              // 4 entry (8 word) queue
              if (tally % 8 == 1 /* || tally % 8 == -1 */)
                addr -= 8;
              else
                addr += 2;
              tally --;
              break;

            case 2:
              // 16 entry (32 word) queue
              if (tally % 32 == 1 /* || tally % 32 == -1 */)
                addr -= 32;
              else
                addr += 2;
              tally --;
              break;
          }

       // tally is 12 bit math
       if (tally & ~07777U)
          {
            sim_debug (DBG_WARN, & iom_dev,
                       "%s: Tally SCW address 0%o under/over flow\n",
                       __func__, tally);
            tally &= 07777;
          }

        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: Updating SCW from: %012"PRIo64"\n",
                   __func__, scw);
        putbits36_12 (& scw, 24, (word12) tally);
        putbits36_18 (& scw, 0, addr);
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s:                to: %012"PRIo64"\n",
                   __func__, scw);
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s:                at: %06o\n",
                   __func__, scwAddr);
        iom_core_write (scwAddr, scw, __func__);
      }

    // BUG: update SCW in core
    return 0;
  }

static word24 UNUSED buildAUXPTWaddress (uint iomUnitIdx, int chan)
  {

//    0      5 6            16 17  18              21  22  23
//   ---------------------------------------------------------
//   | Zeroes | IOM Base Address  |                          |
//   ---------------------------------------------------------
//                         |    Channel Number       | 1 | 1 |
//                         -----------------------------------
// XXX Assuming 16 and 17 are or'ed. Pg B4 doesn't specify

    word12 IOMBaseAddress = iomUnitData [iomUnitIdx] . configSwIomBaseAddress;
    word24 addr = (((word24) IOMBaseAddress) & MASK12) << 6;
    addr |= ((uint) chan & MASK6) << 2;
    addr |= 03;
    return addr;
  }

static word24 buildDDSPTWaddress (word18 PCW_PAGE_TABLE_PTR, word8 pageNumber)
  {
//    0      5 6        15  16  17  18                       23
//   ----------------------------------------------------------
//   | Page Table Pointer | 0 | 0 |                           |
//   ----------------------------------------------------------
// or
//                        -------------------------------------
//                        |  Direct chan addr 6-13            |
//                        -------------------------------------

    word24 addr = (((word24) PCW_PAGE_TABLE_PTR) & MASK18) << 6;
    addr += pageNumber;
    return addr;
  }

// Fetch Direct Data Service Page Table Word

static void fetchDDSPTW (uint iomUnitIdx, int chan, word18 addr)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    word24 pgte = buildDDSPTWaddress (p -> PCW_PAGE_TABLE_PTR, 
                                      (addr >> 10) & MASK8);
    iom_core_read (pgte, & p -> PTW_DCW, __func__);
  }

static word24 buildIDSPTWaddress (word18 PCW_PAGE_TABLE_PTR, word1 seg, word8 pageNumber)
  {
//    0      5 6        15  16  17  18                       23
//   ----------------------------------------------------------
//   | Page Table Pointer         |                           |
//   ----------------------------------------------------------
// plus
//                    ----
//                    | S |
//                    | E |
//                    | G |
//                    -----
// plus
//                        -------------------------------------
//                        |         DCW 0-7                   |
//                        -------------------------------------

    word24 addr = (((word24) PCW_PAGE_TABLE_PTR) & MASK18) << 6;
    addr += (((word24) seg) & 01) << 8;
    addr += pageNumber;
//sim_printf ("    %06o %o %03o %08o\n", PCW_PAGE_TABLE_PTR, seg, pageNumber, addr);
    return addr;
  }

// Fetch Indirect Data Service Page Table Word

static void fetchIDSPTW (uint iomUnitIdx, int chan, word18 addr)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    word24 pgte = buildIDSPTWaddress (p -> PCW_PAGE_TABLE_PTR, 
                                      p -> SEG, 
                                      (addr >> 10) & MASK8);
    iom_core_read (pgte, & p -> PTW_DCW, __func__);
//sim_printf ("       %08o %012"PRIo64"\n", pgte, p -> PTW_DCW);
  }


static word24 buildLPWPTWaddress (word18 PCW_PAGE_TABLE_PTR, word1 seg, word8 pageNumber)
  {

//    0      5 6        15  16  17  18                       23
//   ----------------------------------------------------------
//   | Page Table Pointer         |                           |
//   ----------------------------------------------------------
// plus
//                    ----
//                    | S |
//                    | E |
//                    | G |
//                    -----
// plus
//                        -------------------------------------
//                        |         LPW 0-7                   |
//                        -------------------------------------

    word24 addr = (((word24) PCW_PAGE_TABLE_PTR) & MASK18) << 6;
    addr += (((word24) seg) & 01) << 8;
    addr += pageNumber;
    return addr;
  }

static void fetchLPWPTW (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    word24 addr = buildLPWPTWaddress (p -> PCW_PAGE_TABLE_PTR, 
                                      p -> SEG,
                                      (p -> LPW_DCW_PTR >> 10) & MASK6);
    iom_core_read (addr, & p -> PTW_LPW, __func__);
  }

// 'write' means periperal write; i.e. the peripheral is writing to core after
// reading media.

void iomDirectDataService (uint iomUnitIdx, uint chan, word36 * data,
                           bool write)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    uint daddr = p -> DDCW_ADDR;
    switch (p -> chanMode)
      {
        // DCW EXT
        case cm1e:
        case cm2e:
        case cm1:
          daddr |= (uint) p -> ADDR_EXT << 18;
          break;

        case cm2:
        case cm3b:
          {
sim_warn ("iomDirectDataService DCW paged\n");
          }
          break;

        case cm3a:
        case cm4:
        case cm5:
          {
//sim_printf ("iomDirectDataService DCW segmented\n");
            fetchDDSPTW (iomUnitIdx, (int) chan, daddr);
            daddr = ((uint) getbits36_14 (p -> PTW_DCW, 4) << 10) | (daddr & MASK8);
          }
          break;
      }

    if (write)
      iom_core_write (daddr, * data, __func__);
    else
      iom_core_read (daddr, data, __func__);
  }

// 'tally' is the transfer size request by Multics.
// For write, '*cnt' is the number of words in 'data'.
// For read, '*cnt' will be set to the number of words transfered.
// Caller responsibility to allocate 'data' large enough to accommodate
// 'tally' words.
void iomIndirectDataService (uint iomUnitIdx, uint chan, word36 * data,
                             uint * cnt, bool write)
{
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    uint tally = p -> DDCW_TALLY;
    uint daddr = p -> DDCW_ADDR;
    if (tally == 0)
      {
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: Tally of zero interpreted as 010000(4096)\n",
                   __func__);
        tally = 4096;
      }
    p -> tallyResidue = (word12) tally;

    if (write)
      {
        uint c = * cnt;
        while (p -> tallyResidue)
          {
            if (c == 0)
              break; // read buffer exhausted; returns w/tallyResidue != 0
   
            if (p -> PCW_63_PTP)
              {
                fetchIDSPTW (iomUnitIdx, (int) chan, daddr);
                word24 addr = ((word24) (getbits36_14 (p -> PTW_DCW, 4) << 10)) | (daddr & MASK10);
                iom_core_write (addr, * data, __func__);
//sim_printf (" %o %08o %08o %012"PRIo64" %012"PRIo64"\n", p -> SEG, daddr, addr, * data, p -> PTW_DCW);
              }
            else
              {
                if (daddr > MASK18) // 256K overflow
                  {
                    sim_warn ("iomIndirectDataService 256K ovf\n"); // XXX
                    daddr &= MASK18;
                  }
// If PTP is not set, we are in cm1e or cm2e. Both are 'EXT DCW', so
// we can elide the mode check here.
                uint daddr2 = daddr | (uint) p -> ADDR_EXT << 18;
                iom_core_write (daddr2, * data, __func__);
              }
            daddr ++;
            data ++;
            p -> tallyResidue --;
            c --;
          }
      }
    else // read
      {
        uint c = 0;
        while (p -> tallyResidue)
          {
// XXX assuming DCW_ABS
            if (daddr > MASK18) // 256K overflow
              {
                sim_warn ("iomIndirectDataService 256K ovf\n"); // XXX
                daddr &= MASK18;
              }
            if (p -> PCW_63_PTP)
              {
                fetchIDSPTW (iomUnitIdx, (int) chan, daddr);
                word24 addr = ((word24) (getbits36_14 (p -> PTW_DCW, 4) << 10)) | (daddr & MASK10);
                iom_core_read (addr, data, __func__);
              }
            else
              {
// If PTP is not set, we are in cm1e or cm2e. Both are 'EXT DCW', so
// we can elide the mode check here.
                uint daddr2 = daddr | (uint) p -> ADDR_EXT << 18;
                iom_core_read (daddr2, data, __func__);
              }
            daddr ++;
            p -> tallyResidue --;
            data ++;
            c ++;
          }
        * cnt = c;
      }
}

static void updateChanMode (uint iomUnitIdx, uint chan, bool tdcw)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    if (chan == IOM_CONNECT_CHAN)
      {
//sim_printf ("chan %d to cm1\n", chan);
        p -> chanMode = cm1;
        return;
      }

    if (! tdcw)
      {
        if (p -> PCW_64_PGE == 0)
          {
    
            if (p -> LPW_20_AE == 0)
              {
//sim_printf ("chan %d to cm1e\n", chan);
                    p -> chanMode = cm1e;  // AE 0
              } 
            else
              {
//sim_printf ("chan %d to cm2e\n", chan);
                    p -> chanMode = cm2e;  // AE 1
              } 
               
          }
        else
          {
            if (p -> LPW_20_AE == 0)
              {
    
                if (p -> LPW_23_REL == 0)
                  {
//sim_printf ("chan %d to cm1\n", chan);
                    p -> chanMode = cm1;  // AE 0, REL 0
                  }
                else
                  {
//sim_printf ("chan %d to cm3a\n", chan);
                    p -> chanMode = cm3a;  // AE 0, REL 1
                  }
    
              }
            else  // AE 1
              {
    
                if (p -> LPW_23_REL == 0)
                  {
//sim_printf ("chan %d to cm3b\n", chan);
                    p -> chanMode = cm3b;  // AE 1, REL 0
                  }
                else
                  {
//sim_printf ("chan %d to cm4\n", chan);
                    p -> chanMode = cm4;  // AE 1, REL 1
                  }
    
              }
          }
      }
    else // tdcw
      {
        switch (p -> chanMode)
          {
            case cm1:
              if (p -> TDCW_32_PDTA)
                {
                  p -> chanMode = cm2;
                  break;
                }
              if (p -> TDCW_33_PDCW)
                if (p -> TDCW_35_REL)
                  p -> chanMode = cm4; // 33, 35
                else
                  p -> chanMode = cm3b; // 33, !35
              else
                if (p -> TDCW_35_REL)
                  p -> chanMode = cm3a; // !33, 35
                else
                  p -> chanMode = cm2; // !33, !35
              break;

            case cm2:
              if (p -> TDCW_33_PDCW)
                if (p -> TDCW_35_REL)
                  p -> chanMode = cm4; // 33, 35
                else
                  p -> chanMode = cm3b; // 33, !35
              else
                if (p -> TDCW_35_REL)
                  p -> chanMode = cm3a; // !33, 35
                else
                  p -> chanMode = cm2; // !33, !35
              break;

            case cm3a:
              if (p -> TDCW_33_PDCW)
                p -> chanMode = cm4;
              break;

            case cm3b:
              if (p -> TDCW_35_REL)
                p -> chanMode = cm4;
              break;

            case cm4:
              p -> chanMode = cm5;
              break;

            case cm5:
              break;

            case cm1e:
              {
                if (p -> chanMode == cm1e && p -> TDCW_33_EC)
                  p -> chanMode = cm2e;
              }
              break;

            case cm2e:
              break;
          } // switch
      } // tdcw
  }

static void writeLPW (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];

    uint chanLoc = mbxLoc (iomUnitIdx, chan);
    iom_core_write (chanLoc, p -> LPW, __func__);
    if (chan != IOM_CONNECT_CHAN)
      iom_core_write (chanLoc + 1, p -> LPWX, __func__);
  }

static void fetchAndParseLPW (uint iomUnitIdx, uint chan)
  {
    //uint chanloc = mbxLoc (iomUnitIdx, chan);
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];

    uint chanLoc = mbxLoc (iomUnitIdx, chan);

    iom_core_read (chanLoc, & p -> LPW, __func__);
    sim_debug (DBG_DEBUG, & iom_dev, "lpw %012"PRIo64"\n", p -> LPW);

    p -> LPW_DCW_PTR = getbits36_18 (p -> LPW,  0);
    p -> LPW_18_RES =  getbits36_1 (p -> LPW, 18);
    p -> LPW_19_REL =  getbits36_1 (p -> LPW, 19);
    p -> LPW_20_AE =   getbits36_1 (p -> LPW, 20);
    p -> LPW_21_NC =   getbits36_1 (p -> LPW, 21);
    p -> LPW_22_TAL =  getbits36_1 (p -> LPW, 22);
    p -> LPW_23_REL =  getbits36_1 (p -> LPW, 23);
    p -> LPW_TALLY =   getbits36_12 (p -> LPW, 24);


    if (chan == IOM_CONNECT_CHAN)
      {
        p -> LPWX = 0;
        p -> LPWX_BOUND = 0;
        p -> LPWX_SIZE = 0;
      }
    else
      {
        iom_core_read (chanLoc + 1, & p -> LPWX, __func__);
        p -> LPWX_BOUND = getbits36_18 (p -> LPWX, 0);
        p -> LPWX_SIZE = getbits36_18 (p -> LPWX, 18);
      }   
    updateChanMode (iomUnitIdx, chan, false);

#if 0
// Analyzer

#ifndef SPEED
    if_sim_debug (DBG_DEBUG, & iom_dev)
      {
        if (p -> LPW_21_NC)
          {
            printf ("case c: bootload/connect channel servicing CPI\n");
          }
        else
          {
            if (p -> LPW_22_TAL)
              {
                printf ("case b: standard\n");
              }
            else
                  {
                printf ("case a: GCOS CPI\n");
              }
          }
      }
#endif
#endif
  }

static void unpackDCW (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    p -> DCW_18_20_CP =      getbits36_3 (p -> DCW, 18);

    if (p -> DCW_18_20_CP == 07) // IDCW
      { 
        p -> IDCW_DEV_CMD =      getbits36_6 (p -> DCW,  0);
        p -> IDCW_DEV_CODE =     getbits36_6 (p -> DCW,  6);
        if (p -> LPW_23_REL)
          p -> IDCW_EC = 0;
        else
          p -> IDCW_EC =         getbits36_1 (p -> DCW, 21);
        if (p -> IDCW_EC)
          p -> SEG = 1; // pat. step 45
        p -> IDCW_CONTROL =      getbits36_2 (p -> DCW, 22);
        p -> IDCW_CHAN_CMD =     getbits36_6 (p -> DCW, 24);
        p -> IDCW_COUNT =        getbits36_6 (p -> DCW, 30);
      }
    else // TDCW or DDCW
      {
        p -> TDCW_DATA_ADDRESS = getbits36_18 (p -> DCW,  0);
        p -> TDCW_31_SEG =       getbits36_1 (p -> DCW, 31);
        p -> TDCW_32_PDTA =      getbits36_1 (p -> DCW, 32);
        p -> TDCW_33_PDCW =      getbits36_1 (p -> DCW, 33);
        p -> TDCW_33_EC =        getbits36_1 (p -> DCW, 33);
        p -> TDCW_34_RES =       getbits36_1 (p -> DCW, 34);
        p -> TDCW_35_REL =       getbits36_1 (p -> DCW, 35);

        p -> DDCW_TALLY =        getbits36_12 (p -> DCW, 24);
        p -> DDCW_ADDR =         getbits36_18 (p -> DCW,  0);
        p -> DDCW_22_23_TYPE =   getbits36_2 (p -> DCW, 22);
      }
  }

static void packDCW (uint iomUnitIdx, uint chan)
  {
    // DCW_18_20_CP is the only field ever changed.
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    putbits36_3 (& p -> DCW, 18, p -> DCW_18_20_CP);
  }

static void packLPW (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    putbits36_18 (& p-> LPW,  0, p -> LPW_DCW_PTR);
    putbits36_1 (& p-> LPW, 18, p -> LPW_18_RES);
    putbits36_1 (& p-> LPW, 19, p -> LPW_19_REL);
    putbits36_1 (& p-> LPW, 20, p -> LPW_20_AE);
    putbits36_1 (& p-> LPW, 21, p -> LPW_21_NC);
    putbits36_1 (& p-> LPW, 22, p -> LPW_22_TAL);
    putbits36_1 (& p-> LPW, 23, p -> LPW_23_REL);
    putbits36_12 (& p-> LPW, 24, p -> LPW_TALLY);
  }

static void fetchAndParsePCW (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    iom_core_read2 (p -> LPW_DCW_PTR, & p -> PCW0, & p -> PCW1, __func__);
//sim_printf ("%012"PRIo64" %012"PRIo64"\n", p -> PCW0, p ->  PCW1);
    p -> PCW_CHAN = getbits36_6 (p -> PCW1, 3);
    p -> PCW_AE = getbits36_6 (p -> PCW0, 12);
    p -> PCW_21_MSK = getbits36_1 (p -> PCW0, 21);
    p -> PCW_PAGE_TABLE_PTR = getbits36_18 (p -> PCW1, 9);
    p -> PCW_63_PTP = getbits36_1 (p -> PCW1, 27);
    p -> PCW_64_PGE = getbits36_1 (p -> PCW1, 28);
//sim_printf ("PCW_64_PGE %u\n", p -> PCW_64_PGE);
//sim_printf ("%d %p\n", chan, p);
    p -> PCW_65_AUX = getbits36_1 (p -> PCW1, 29);
    if (p -> PCW_65_AUX)
      sim_warn ("PCW_65_AUX\n");
    p -> DCW = p -> PCW0;
    unpackDCW (iomUnitIdx, chan);

  }
 
static void fetchAndParseDCW (uint iomUnitIdx, uint chan, UNUSED bool read_only)
  {
    iomChanData_t * p =  & iomChanData [iomUnitIdx] [chan];

    word24 addr = p -> LPW_DCW_PTR & MASK18;

#if 0
    switch (p -> chanMode)
      {
        case cm_LPW_init_state:
          {
            // DCW_PTR is in real mode.
            if (p -> LPW_20_AE)
              {
                addr |= ((word24) p -> LPWX_BOUND << 18);
                // XXX check bound?
              }
          }
          break;
        default:
          {
sim_err ("unhandled fetchAndParseDCW\n");
          }
          //break;
      }

    iom_core_read (addr, & p -> DCW, __func__);
#endif
    switch (p -> chanMode)
      {
        // LPW ABS
        case cm1:
        case cm1e:
          {
            iom_core_read (addr, & p -> DCW, __func__);
          }
          break;

        // LPW EXT
        case cm2e:
          {
            addr |= ((word24) p -> LPWX_BOUND << 18);
            iom_core_read (addr, & p -> DCW, __func__);
          }
          break;

        case cm2:
        case cm3b:
          {
            sim_warn ("fetchAndParseDCW LPW paged\n");
          }
          break;

        case cm3a:
        case cm4:
        case cm5:
          {
//sim_printf ("fetchAndParseDCW LPW segmented\n");
//sim_printf ("DCW is seg; addr = %o lbnd = %o size = %o\n",
//                    p -> LPW_DCW_PTR,
//                    p -> LPWX_BOUND,
//                    p -> LPWX_SIZE);
//sim_printf ("ptPtr %o\n", p -> PCW_PAGE_TABLE_PTR);
            fetchLPWPTW (iomUnitIdx, chan);
//sim_printf ("PTW %012"PRIo64"\n", p -> PTW_LPW);
            // Calculate effective address
            // PTW 4-17 || LPW 8-17
            word24 addr_ = ((word24) (getbits36_14 (p -> PTW_LPW, 4) << 10)) | ((p -> LPW_DCW_PTR) & MASK10);
//sim_printf ("addr now %08o\n", addr_);
            iom_core_read (addr_, & p -> DCW, __func__);
//sim_printf ("dcw now %012"PRIo64"\n", p -> DCW);
          }
          break;
      }
    unpackDCW (iomUnitIdx, chan);
  }

static void iomFault (uint iomUnitIdx, uint chan, UNUSED const char * who,
                      iomFaultServiceRequest req,
                      iomSysFaults_t signal)
  {
//sim_printf ("iomFault %s\n", who);
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    // TODO:
    // For a system fault:
    // Store the indicated fault into a system fault word (3.2.6) in
    // the system fault channel -- use a list service to get a DCW to do so
    // For a user fault, use the normal channel status mechanisms

    // sys fault masks channel

    // signal gets put in bits 30..35, but we need fault code for 26..29

    // BUG: mostly unimplemented

    // User Faults: Pg 78: "A user fault is an abnormal conditions that can
    // be caused by a user program operating in the slave mode in the 
    // processor."
    //  and
    // "User faults can be detected by the IOM Central of by a channel. If
    // a user fault is detected by the IOM Central, the fault is indicated to
    // the channel, and the channel is responsible for reporting the user
    // fault as status is its regular status queue.

    // This code only handles system faults
    //

    word36 faultWord = 0;
    putbits36_9 (& faultWord, 9, (word9) chan);
    putbits36_5 (& faultWord, 18, req);
    // IAC, bits 26..29
    putbits36_6 (& faultWord, 30, signal);

    uint mbx = mbxLoc (iomUnitIdx, chan);

    fetchAndParseDCW (iomUnitIdx, chan, false);
    if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
      {
        sim_warn ("%s: expected a DDCW; fail\n", __func__);
        return;
      }
    // No address extension or paging nonsense for channels 0-7. 
    uint addr = p -> DDCW_ADDR;
    iom_core_write (addr, faultWord, __func__);

    send_general_interrupt (iomUnitIdx, 1, imwSystemFaultPic);

    word36 ddcw;
    iom_core_read (mbx, & ddcw, __func__);
    // incr addr
    putbits36_18 (& ddcw, 0, (getbits36_18 (ddcw, 0) + 1u) & MASK18);
    // decr tally
    putbits36_12 (& ddcw, 24, (getbits36_12 (ddcw, 24) - 1u) & MASK12);
    iom_core_write (mbx, ddcw, __func__);
  }

// 0 ok
// -1 fault
// There is a path through the code where no DCW is sent (IDCW && LPW_18_RES)
// Does the -1 return cover that?

int iomListService (uint iomUnitIdx, uint chan,
                           bool * ptro, bool * sendp, bool * uffp)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];

// initialize

    bool isConnChan = chan == IOM_CONNECT_CHAN;
    * ptro = false; // assume not PTRO
    bool uff = false; // user fault flag
    bool send = false;

// Figure 4.3.1

// START

    // FIRST SERVICE?

    if (p -> lsFirst)
      {
        // PULL LPW AND LPW EXT. FROM CORE MAILBOX

        fetchAndParseLPW (iomUnitIdx, chan);
        p -> wasTDCW = false;
        p -> SEG = 0; // pat. FIG. 2, step 44
      }
    // else lpw and lpwx are in chanData;

    // CONNECT CHANNEL?

    if (isConnChan)
      { // connect channel

        // LPW 21 (NC), 22 (TAL) is {00, 01, 1x}?

        if (p -> LPW_21_NC == 0 && p -> LPW_22_TAL == 0)
              {
                iomFault (iomUnitIdx, IOM_CONNECT_CHAN, __func__,
                          p -> lsFirst ? iomFsrFirstList : iomFsrList, 
                          iomIllTalCChnFlt);
                return -1;
              }

        if (p -> LPW_21_NC == 0 && p -> LPW_22_TAL == 1)
          { // 01

            // TALLY is {0, 1, >1}?

            if (p -> LPW_TALLY == 0)
              {
                iomFault (iomUnitIdx, IOM_CONNECT_CHAN, __func__,
                          p -> lsFirst ? iomFsrFirstList : iomFsrList, 
                          iomIllTalCChnFlt);
                return -1;
              }
 
            if (p -> LPW_TALLY > 1)
              { // 00

                // 256K OVERFLOW?
                if (p -> LPW_20_AE == 0 && 
                    (((word36) p -> LPW_DCW_PTR) + ((word36) p -> LPW_TALLY)) >
                    01000000llu)
                  {
                    iomFault (iomUnitIdx, IOM_CONNECT_CHAN, __func__,
                              p -> lsFirst ? iomFsrFirstList : iomFsrList, 
                              iom256KFlt);
                    return -1;
                  }
              }
            else if (p -> LPW_TALLY == 1)
              * ptro = true;
          }
        else // 1x
          * ptro = true;

        // PULL PCW FROM CORE

// B

        fetchAndParsePCW (iomUnitIdx, chan); // fills in DCW*

        // PCW 18-20 == 111?
        if (p -> DCW_18_20_CP != 07u)
          {
            iomFault (iomUnitIdx, IOM_CONNECT_CHAN, __func__,
                      p -> lsFirst ? iomFsrFirstList : iomFsrList, 
                      iomNotPCWFlt);
            return -1;
          }

        // SELECT CHANNEL

        // chan = p -> PCW_CHAN;

// detect unused slot as fault
// if (no handler in slot)
//  goto FAULT;

       // SEND PCW TO CHANNEL

        // p -> DCW = p -> DCW;
        send = true;

        goto D;
      }




    // Not connect channel

    // LPW 21 (NC), 22 (TAL) is {00, 01, 1x}?

    if (p -> LPW_21_NC == 0 && p -> LPW_22_TAL == 0)
      {
// XXX see pat. 46-51 re: SEG
        // 256K OVERFLOW?
        if (p -> LPW_20_AE == 0 && 
            (((word36) p -> LPW_DCW_PTR) + ((word36) p -> LPW_TALLY)) >
            01000000llu)
          {
            iomFault (iomUnitIdx, IOM_CONNECT_CHAN, __func__,
                      p -> lsFirst ? iomFsrFirstList : iomFsrList, 
                      iom256KFlt);
            return -1;
          }
       }
    else if (p -> LPW_21_NC == 0 && p -> LPW_22_TAL == 1)
      { // 01
A:;
        // TALLY is {0, 1, >1}?

        if (p -> LPW_TALLY == 0)
          {
            uff = true;
          }
        else if (p -> LPW_TALLY > 1)
          { // 00

// XXX see pat. 46-51 re: SEG
            // 256K OVERFLOW?
            if (p -> LPW_20_AE == 0 && 
                (((word36) p -> LPW_DCW_PTR) + ((word36) p -> LPW_TALLY)) >
                01000000llu)
              {
                iomFault (iomUnitIdx, IOM_CONNECT_CHAN, __func__,
                          p -> lsFirst ? iomFsrFirstList : iomFsrList, 
                          iom256KFlt);
                return -1;
              }
          }
      }

    // LPW 20? -- LPW_20 checked by fetchAndParseDCW

    // PULL DCW FROM CORE
    fetchAndParseDCW (iomUnitIdx, chan, false);

// C

    // IDCW ?

    if (p -> DCW_18_20_CP == 7)
      {

        // LPW_18_RES?

        if (p -> LPW_18_RES)
          {
            // SET USER FAULT FLAG
            uff = true; // XXX Why? uff isn't not examinded later.
            // send = false; implicit...
          }
        else
          {
            // SEND IDCW TO CHANNEL
            // p -> DCW = p -> DCW;
            send = true;
          }
        goto D;
      }

// Not IDCW

// pg B16: "If the IOM is paged [yes] and PCW bit 64 is off, LPW bit 23
//          is ignored by the hardware. If bit 64 is set, LPW bit 23 causes
//          the data to become segmented."
// Handled in fetchAndParseLPW

    // LPW 23 REL?

#if 0
// XXX see pat. 46-51 re: SEG
    if (p -> LPW_23_REL)
      {
        // BOUNDARY ERROR

#if 0 // XXX
        if (boundary error)
          {
            uff = true;
            goto uffSet;
          }

        // ABSOLUTIZE ADDRESS

        absolutize address;
#else
//sim_printf ("LPW_23_REL fail\n");
#endif
      }
#endif

    // TDCW ?

    if (p -> DCW_18_20_CP != 7 && p -> DDCW_22_23_TYPE == 2)
      {
//sim_printf (">>>>>>>>>> TDCW\n");
        // SECOND TDCW?
        if (p -> wasTDCW)
          {
//sim_printf ("2nd TDCW\n");
            uff = true;
            goto uffSet;
          }
        p -> wasTDCW = true;

        // PUT ADDRESS N LPW

        p -> LPW_DCW_PTR = p -> TDCW_DATA_ADDRESS;
        // OR TDCW 33, 34, 35 INTO LPW 20, 18, 23
// XXX is 33 bogus? it's semantics change based on PCW 64...
// should be okay; pg B21 says that this is correct; implies that the
// semantics are handled in the LPW code. Check...
        p -> LPW_20_AE |= p -> TDCW_33_EC; // TDCW_33_PDCW
        p -> LPW_18_RES |= p -> TDCW_34_RES;
        p -> LPW_23_REL |= p -> TDCW_35_REL;
        
// Pg B21: (TDCW_31_SEG)
// "SEG = This bit furnishes the 19th address bit (MSD) of a TDCW address
//  used for locating the DCW list in a 512 word page table. It will have
//  meaning only in the TDCW whre:
//   (a) the DCW list is already paged and the TDCW calls for the
//       DCW [to be] segmented
//  or
//   (b) the data is already segmented and the TDCW calls for the
//       DCW list to be paged
//  or
//   (c) neither data is segmented nor DCW list is paged and the
//       TDCW calls for both.
//  and
//   (d) an auxiliary PTW in not being used.



//   (a) the DCW list is already paged   --  LPW PAGED: 3b, 4
//       and the TDCW calls for the
//       DCW [list to be] segmented      --  LPW SEG:       5


//   (b) the data is already segmented   --  DCW SEG:   3a, 4
//       and the TDCW calls for the
//       DCW list to be paged            --  DCW PAGED:  2, 3b


//   (c) neither data is segmented       -- DCW !SEG     1, 2, 3b
//       nor DCW list is paged           -- LPW !PAGED   1, 2, 3a
//                                       --              1, 2
//       and the TDCW calls for both.    -- DCW SEG & LPW PAGED
//                                                       4

//put that wierd SEG logic in here

        if (p -> TDCW_31_SEG)
          sim_warn ("TDCW_31_SEG\n");

        updateChanMode (iomUnitIdx, chan, true);


        // Decrement tally
        p -> LPW_TALLY = (p -> LPW_TALLY - 1u) & MASK12;

        packLPW (iomUnitIdx, chan);

        // AC CHANGE ERROR? (LPW 18 == 1 && DCW 33 == 1)

        if (p -> LPW_18_RES && p -> TDCW_33_EC) // same as TDCW_33_PDCW
          {
//sim_printf ("AC CHANGE ERROR\n");
            uff = true;
            goto uffSet;
          }
          
//sim_printf ("going to A\n");
        goto A;
      }

    p -> wasTDCW = false;
    // NOT TDCW

    // CP VIOLATION?

    // 43A239854 3.2.3.3 "The byte size, defined by the channel, determines
    // what CP vaues are valid..."

    // If we get here, the DCW is not a IDCW and not a TDCW, therefore 
    // it must be a DDCW. If the list service knew the sub-word size
    // size of the device, it could check the for valid values. Let
    // the device handler do that later.

    // if (cp decrepancy)
    //   {
    //      user_fault_flag = iomCsCpDiscrepancy;
    //      goto user_fault;
    //   }
    // if (cp violation)
    //   {
    //     uff = true;
    //     goto uffSet;
    //   }

    // USER FAULT FLAT SET?

    if (uff)
      {
uffSet:;
        // PUT 7 into DCW 18-20
        p -> DCW_18_20_CP = 07u;
        packDCW (iomUnitIdx, chan);
      }

    // WRITE DCW IN [SCRATCH PAD] MAILBOX

    // p -> DVW  = P -> DCW;

    // SEND DCW TO CHANNEL

    // p -> DCW = p -> DCW;
    send = true;

    // goto D;

D:;

    // SEND FLAGS TO CHANNEL

    * uffp = uff;
    * sendp = send;

    // LPW 21 ?

    if (p -> LPW_21_NC == 0) // UPDATE
     {
        // UPDATE LPW ADDRESS & TALLY
        p -> LPW_DCW_PTR = (p -> LPW_DCW_PTR + 1u) & MASK18;       
        p -> LPW_TALLY = (p -> LPW_TALLY - 1u) & MASK12;       
        packLPW (iomUnitIdx, chan);
     }

    // IDCW OR FIRST LIST
    if (p -> DDCW_22_23_TYPE == 07u || p -> lsFirst)
      {
        // WRITE LPW & LPW EXT. INTO BOTH SCRATCHPAD AND CORE MAILBOXES
        // scratch pad
        // p -> lpw = p -> lpw
        // core
        writeLPW (iomUnitIdx, chan);
      }
    else
      {
        if (p -> LPW_21_NC == 0 || 
            (p -> DCW_18_20_CP != 7 && p -> DDCW_22_23_TYPE == 2))
          {
            // WRITE LPW INTO BOTH SCRATCHPAD AND CORE MAILBOXES
            // scratch pad
            // p -> lpw = p -> lpw
            // core
            writeLPW (iomUnitIdx, chan);
          }
      }

    p -> lsFirst = false;
    // END

    return 0;

  }

// 0 ok
// -1 uff
static int doPayloadChan (uint iomUnitIdx, uint chan)
  {
// A dubious assumption being made is that the device code will always
// be in bits 6-12 of the DCw. Normally, the controller would
// decipher the device code and route to the device, but we
// have elided the controllers and must do that ourselves.

// Loop logic
//
//   The DCW list can be terminated two ways; either by having the 
//   tally run out, or a IDCW with control set to zero.
//
//   The device handler will absorb DDCWs
//   
// loop until:
//
//  listService sets ptro, indicating that no more DCWs are availible. or
//     control is 0, indicating last IDCW

    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];


// Copy the PCW from the connect channel

    {
      iomChanData_t * q = & iomChanData [iomUnitIdx] [IOM_CONNECT_CHAN];
      p -> PCW0 =               q -> PCW0;
      p -> PCW1 =               q -> PCW1;
      p -> PCW_CHAN =           q -> PCW_CHAN;
      p -> PCW_AE =             q -> PCW_AE;
      p -> PCW_PAGE_TABLE_PTR = q -> PCW_PAGE_TABLE_PTR;
      p -> PCW_63_PTP =         q -> PCW_63_PTP;
      p -> PCW_64_PGE =         q -> PCW_64_PGE;
      p -> PCW_65_AUX =         q -> PCW_65_AUX;
      p -> PCW_21_MSK =         q -> PCW_21_MSK;
    }

    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];

// A device command of 051 in the PCW is only meaningful to the operator console;
// all other channels should ignore it. We use (somewhat bogusly) a chanType of
// chanTypeCPI to indicate the operator console.
    if (d -> ctype != chanTypeCPI && p -> IDCW_DEV_CMD == 051)
      return 0;

    if (! d -> iomCmd)
      {
#if 0
        // XXX: no device connected; what's the appropriate fault code (s) ?
        sim_debug (DBG_WARN, & iom_dev,
                   "%s: No device connected to channel %#o (%d)\n",
                   __func__, chan, chan);
        iomFault (iomUnitIdx, chan, __func__, 0, 0);
#else
        p -> stati = 06000; // t, power off/missing
#endif
        goto done;
      }

// 3.2.2. "Bits 12-17 [of the PCW] contain the address extension which is maintained by
//         the channel for subsequent use by the IOM in generating a 24-bit
//         address for list of data services for the extended address modes."
// see also 3.2.3.1
    p -> ADDR_EXT = p -> PCW_AE;

    p -> lsFirst = true;

    p -> recordResidue = 0;
    p -> tallyResidue = 0;
    p -> isRead = true;
    p -> charPos = 0;
// As far as I can tell, initiate false means that an IOTx succeeded in
// transferring data; assume it didn't since that is the most common
// code path.
    p -> initiate = true;
    p -> chanStatus = chanStatNormal;

//sim_printf ("chan %d (%o) control %d\n", chan, chan, p -> IDCW_CONTROL);
// Send the PCW's DCW
    //sim_printf ("PCW chan %d (%o) control %d\n", chan, chan, p -> IDCW_CONTROL);
    int rc = d -> iomCmd (iomUnitIdx, chan);

//
// iomCmd returns:
//
//  0: ok
//  1; ignored cmd, drop connect.
//  2: did command, don't do DCW list
//  3; command pending, don't sent terminate interrupt
// -1: error

    if (rc == 1) // handler ignored command; used to be used for 051, now unused.
      {
        sim_debug (DBG_DEBUG, & iom_dev, "handler ignored cmd\n");
        return 0;
      }

    if (rc == 2) // handler doesn't want the dcw list
      {
        sim_debug (DBG_DEBUG, & iom_dev, "handler don't want no stinking dcws\n");
        goto done;
      }

    if (rc == 3) // handler still processing command, don't set
                 // terminate intrrupt.
      {
        sim_debug (DBG_DEBUG, & iom_dev, "handler processing cmd\n");
        return 0;
      }

    if (rc)
      {
// 04501 : COMMAND REJECTED, invalid command
        p -> stati = 04501;
        p -> dev_code = getbits36_6 (p -> DCW, 6);
        p -> chanStatus = chanStatInvalidInstrPCW;
        sim_warn ("doPayloadChan handler error\n");
        goto done;
      }

#if 0
// The boot tape loader sends PCWs with control == 0 when it shouldn't
// BCE sends disk PCWs with control == 0 when it shouldn't

    //if (p -> IDCW_CONTROL == 0)
    if (p -> IDCW_CONTROL == 0 && d -> type != DEVT_TAPE && d -> type != DEVT_DISK)
      {
        //sim_printf ("ctrl == 0 in chan %d (%o) PCW\n", chan, chan);
        goto done;
      }
#endif

    bool ptro, send, uff;

    do
      {
        int rc2 = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
        if (rc2 < 0)
          {
// XXX set status flags
            sim_warn ("doPayloadChan list service failed\n");
            return -1;
          }
        if (uff)
          {
            sim_warn ("doPayloadChan ignoring uff\n"); // XXX
          }
        if (! send)
          {
            sim_warn ("doPayloadChan nothing to send\n");
            return 1;
          }

        if (p -> DCW_18_20_CP != 07) // Not IDCW
          {
// 04501 : COMMAND REJECTED, invalid command
            p -> stati = 04501;
            p -> dev_code = getbits36_6 (p -> DCW, 6);
            p -> chanStatus = chanStatInvalidInstrPCW;
            sim_warn ("doPayloadChan expected IDCW %d (%o)\n", chan, chan);
            goto done;
          }

// 3.2.3.1 "If EC - Bit 21 = 1, The channel will replace the present address
//          extension with the new address extension in bits 12-17. ... In
//          Multics and NSA systems, EC is inhibited from the payload channel
//          if LPW bit 23 = 1.

        if (p -> LPW_23_REL == 0 && p -> IDCW_EC == 1)
          p -> ADDR_EXT = getbits36_6 (p -> DCW, 12);

        p -> recordResidue = 0;
        p -> tallyResidue = 0;
        p -> isRead = true;
        p -> charPos = 0;
        p -> chanStatus = chanStatNormal;


// The device code is per IDCW; look up the device for this IDCW

        d = & cables -> cablesFromIomToDev [iomUnitIdx] .  devices [chan] [p -> IDCW_DEV_CODE];
        if (! d -> iomCmd)
          {
            p -> stati = 06000; // t, power off/missing
            goto done;
          }
// Send the DCW list's DCW

        rc2 = d -> iomCmd (iomUnitIdx, chan);

        if (rc2 == 3) // handler still processing command, don't set
                     // terminate intrrupt.
          {
            sim_debug (DBG_DEBUG, & iom_dev, "handler processing cmd\n");
            return 0;
          }

        if (rc2 || p -> IDCW_CONTROL == 0) 
          ptro = true; 
      } while (! ptro);
 
done:;
    send_terminate_interrupt (iomUnitIdx, chan);

    return 0;
  }

// doConnectChan ()
//
// Process the "connect channel".  This is what the IOM does when it
// receives a $CON signal.
//
// Only called by iom_interrupt ()
//
// The connect channel requests one or more "list services" and processes the
// resulting PCW control words.
//

static int doConnectChan (uint iomUnitIdx)
  {
    // ... the connect channel obtains a list service from the IOM Central.
    // During this service the IOM Central will do a double precision read
    // from the core, under the control of the LPW for the connect channel.
    //
    // If the IOM Central does not indicate a PTRO during the list service,
    // the connect channel obtains anther list service.
    //
    // The connect channel does interrupt or store status.
    //
    // The DCW and SCW mailboxes for the connect channel are not used by
    // the IOM.

    iomChanData_t * p = & iomChanData [iomUnitIdx] [IOM_CONNECT_CHAN];
    p -> lsFirst = true;
    bool ptro, send, uff;
    do
      {
        // Fetch the next PCW
        int rc = iomListService (iomUnitIdx, IOM_CONNECT_CHAN, & ptro, & send, & uff);
        if (rc < 0)
          {
            sim_warn ("connect channel connect failed\n");
            return -1;
          }
        if (uff)
          {
            sim_warn ("connect channel ignoring uff\n"); // XXX
          }
        if (! send)
          {
            sim_warn ("connect channel nothing to send\n");
          }
        else
          {
            // Copy the PCW's DCW to the payload channel
            iomChanData_t * q = & iomChanData [iomUnitIdx] [p -> PCW_CHAN];
            q -> DCW = p -> DCW;
            unpackDCW (iomUnitIdx, p -> PCW_CHAN);
            doPayloadChan (iomUnitIdx, p -> PCW_CHAN);
          }
      } while (! ptro);
    return 0; // XXX
  }

/*
 * send_general_interrupt ()
 *
 * Send an interrupt from the IOM to the CPU.
 *
 */

static int send_general_interrupt (uint iomUnitIdx, uint chan, enum iomImwPics pic)
  {

    uint imw_addr;
    uint chan_group = chan < 32 ? 1 : 0;
    uint chan_in_group = chan & 037;
    uint interrupt_num = iomUnitIdx | (chan_group << 2) | ((uint) pic << 3);
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
    uint pi_base = iomUnitData [iomUnitIdx] . configSwMultiplexBaseAddress & ~3u;
    imw_addr = (pi_base << 3) | interrupt_num;

    sim_debug (DBG_NOTIFY, & iom_dev, 
               "%s: IOM %c, channel %d (%#o), level %d; "
               "Interrupt %d (%#o).\n", 
               __func__, 'A' + iomUnitIdx, chan, chan, pic, interrupt_num, 
               interrupt_num);
    word36 imw;
    iom_core_read (imw_addr, &imw, __func__);
    // The 5 least significant bits of the channel determine a bit to be
    // turned on.
    sim_debug (DBG_DEBUG, & iom_dev, 
               "%s: IMW at %#o was %012"PRIo64"; setting bit %d\n", 
               __func__, imw_addr, imw, chan_in_group);
    putbits36_1 (& imw, chan_in_group, 1);
    sim_debug (DBG_INFO, & iom_dev, 
               "%s: IMW at %#o now %012"PRIo64"\n", __func__, imw_addr, imw);
    (void) iom_core_write (imw_addr, imw, __func__);
    
// XXX this should call scu_svc

// XXX shouldn't it interrupt the SCU that invoked the connect?
#if 1
    return scu_set_interrupt (iomUnitData [iomUnitIdx] . invokingScuUnitNum, interrupt_num);
#else
    uint base = iomUnitData [iomUnitIdx] . configSwIomBaseAddress;
    uint base_addr = base << 6; // 01400
    // XXX this is wrong; I believe that the SCU unit number should be
    // calculated from the Port Configuration Address Assignment switches
    // For now, however, the same information is in the CPU config. switches, so
    // this should result in the same values.

    int cpu_port_num = queryIomScbankMap (iomUnitIdx, base_addr);
    int scuUnitNum;
    if (cpu_port_num >= 0)
      scuUnitNum = query_scu_unit_num (ASSUME_CPU_0, cpu_port_num);
    else
      scuUnitNum = 0;
    // XXX Print warning
    if (scuUnitNum < 0)
      scuUnitNum = 0;
    return scu_set_interrupt ((uint)scuUnitNum, interrupt_num);
#endif
  }

/*
 * send_marker_interrupt ()
 *
 * Send a "marker" interrupt to the CPU.
 *
 * Channels send marker interrupts to indicate normal completion of
 * a PCW or IDCW if the control field of the PCW/IDCW has a value
 * of three.
 */

int send_marker_interrupt (uint iomUnitIdx, int chan)
{
    status_service (iomUnitIdx, (uint) chan, true);
    return send_general_interrupt (iomUnitIdx, (uint) chan, imwMarkerPic);
}

/*
 * send_special_interrupt ()
 *
 * Send a "special" interrupt to the CPU.
 *
 */

int send_special_interrupt (uint iomUnitIdx, uint chan, uint devCode, 
                            word8 status0, word8 status1)
  {
    uint chanloc = mbxLoc (iomUnitIdx, IOM_SPECIAL_STATUS_CHAN);

// Multics uses an 12(8) word circular queue, managed by clever manipulation
// of the LPW and DCW.
// Rather then goes through the mechanics of parsing the LPW and DCW,
// we will just assume that everything is set up the way we expect,
// and update the circular queue.
    word36 lpw;
    iom_core_read (chanloc + 0, & lpw, __func__);

    word36 dcw;
    iom_core_read (chanloc + 3, & dcw, __func__);

    word36 status = 0400000000000;   
    status |= (((word36) chan) & MASK6) << 27;
    status |= (((word36) devCode) & MASK8) << 18;
    status |= (((word36) status0) & MASK8) <<  9;
    status |= (((word36) status1) & MASK8) <<  0;
    iom_core_write ((dcw >> 18) & MASK18, status, __func__);

    uint tally = dcw & MASK12;
    if (tally > 1)
      {
        dcw -= 01llu;  // tally --
        dcw += 01000000llu; // addr ++
      }
    else
      dcw = 001320010012llu; // reset to beginning of queue
    iom_core_write (chanloc + 3, dcw, __func__);

    send_general_interrupt (iomUnitIdx, IOM_SPECIAL_STATUS_CHAN, imwSpecialPic);
    return 0;
  }

/*
 * send_terminate_interrupt ()
 *
 * Send a "terminate" interrupt to the CPU.
 *
 * Channels send a terminate interrupt after doing a status service.
 *
 */

int send_terminate_interrupt (uint iomUnitIdx, uint chan)
  {
    status_service (iomUnitIdx, chan, false);
    send_general_interrupt (iomUnitIdx, chan, imwTerminatePic);
    return 0;
  }

void iom_interrupt (uint scuUnitNum, uint iomUnitIdx)
  {
    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c starting.\n",
               __func__, 'A' + iomUnitIdx);

    iomUnitData [iomUnitIdx] . invokingScuUnitNum = scuUnitNum;

    int ret = doConnectChan (iomUnitIdx);

    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: IOM %c finished; doConnectChan returned %d.\n",
               __func__, 'A' + iomUnitIdx, ret);
    // XXX doConnectChan return value ignored
  }
 

//
// iomReset ()
//
//  Reset -- Reset to initial state -- clear all device flags and cancel any
//  any outstanding timing operations. Used by SIMH's RESET, RUN, and BOOT
//  commands
//
//  Note that all reset ()s run after once-only init ().
//
//

static t_stat iomReset (UNUSED DEVICE * dptr)
  {
    sim_debug (DBG_INFO, & iom_dev, "%s: running.\n", __func__);

    for (uint iomUnitIdx = 0; iomUnitIdx < iom_dev . numunits; iomUnitIdx ++)
      {
        for (uint chan = 0; chan < MAX_CHANNELS; ++ chan)
          {
            for (uint dev_code = 0; dev_code < N_DEV_CODES; dev_code ++)
              {
                DEVICE * devp = cables -> cablesFromIomToDev [iomUnitIdx] . devices [chan] [dev_code] . dev;
                if (devp)
                  {
                    if (devp -> units == NULL)
                      {
                        sim_warn ("%s: Device on IOM %c channel %d dev_code %d does not have any units.\n",
                          __func__,  'A' + iomUnitIdx, chan, dev_code);
                      }
                  }
              }
          }
      }
    

    setupIOMScbankMap ();

    return SCPE_OK;
  }

/*
 * iom_init ()
 *
 *  Once-only initialization
 */

void iom_init (void)
  {
    sim_debug (DBG_INFO, & iom_dev, "%s: running.\n", __func__);
  }

/*
 * initMemoryIOM ()
 *
 * Load a few words into memory.   Simulates pressing the BOOTLOAD button
 * on an IOM or equivalent.
 *
 * All values are from bootload_tape_label.alm.  See the comments at the
 * top of that file.  See also doc #43A239854.
 *
 * NOTE: The values used here are for an IOM, not an IOX.
 * See init_memory_iox () below.
 *
 */

static void initMemoryIOM (uint iomUnitIdx)
  {
    // The presence of a 0 in the top six bits of word 0 denote an IOM boot
    // from an IOX boot
    
    // " The channel number ("Chan#") is set by the switches on the IOM to be
    // " the channel for the tape subsystem holding the bootload tape. The
    // " drive number for the bootload tape is set by switches on the tape
    // " MPC itself.
    
    sim_debug (DBG_INFO, & iom_dev,
      "%s: Performing load of eleven words from IOM %c bootchannel to memory.\n",
      __func__, 'A' + iomUnitIdx);

    word12 base = iomUnitData [iomUnitIdx] . configSwIomBaseAddress;

    // bootload_io.alm insists that pi_base match
    // template_slt_$iom_mailbox_absloc

    //uint pi_base = iomUnitData [iomUnitIdx] . configSwMultiplexBaseAddress & ~3;
    word36 pi_base = (((word36) iomUnitData [iomUnitIdx] . configSwMultiplexBaseAddress)  << 3) |
                     (((word36) (iomUnitData [iomUnitIdx] . configSwIomBaseAddress & 07700U)) << 6) ;
    word3 iom_num = ((word36) iomUnitData [iomUnitIdx] . configSwMultiplexBaseAddress) & 3; 
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
    
    enum configSwBlCT_t bootdev = iomUnitData [iomUnitIdx] . configSwBootloadCardTape;

    word6 bootchan;
    if (bootdev == CONFIG_SW_BLCT_CARD)
      bootchan = iomUnitData [iomUnitIdx] . configSwBootloadCardrdrChan;
    else // CONFIG_SW_BLCT_TAPE
      bootchan = iomUnitData [iomUnitIdx] . configSwBootloadMagtapeChan;


    // 1

    word36 dis0 = 0616200;

    // system fault vector; DIS 0 instruction (imu bit not mentioned by 
    // 43A239854)

    M [010 + 2 * iom_num] = (imu << 34) | dis0;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012"PRIo64"\n",
      010 + 2 * iom_num, (imu << 34) | dis0);

    // Zero other 1/2 of y-pair to avoid msgs re reading uninitialized
    // memory (if we have that turned on)

    M [010 + 2 * iom_num + 1] = 0;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012o\n",
      010 + 2 * iom_num + 1, 0);
    

    // 2

    // terminate interrupt vector (overwritten by bootload)

    M [030 + 2 * iom_num] = dis0;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012"PRIo64"\n",
      030 + 2 * iom_num, dis0);


    // 3

    // XXX CAC: Clang -Wsign-conversion claims 'base<<6' is int
    word24 base_addr = (word24) base << 6; // 01400
    
    // tally word for sys fault status
    M [base_addr + 7] = ((word36) base_addr << 18) | 02000002;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012"PRIo64"\n",
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

    word24 mbx = base_addr + 4u * bootchan;

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
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012"PRIo64"\n",
      mbx + 2, ((word36)base_addr << 18));
    

    // 8

    // 1st word of bootload channel PCW

    M [0] = 0720201;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012o\n",
      0, 0720201);
    

    // 9

    // "SCU port" 
    word3 port = iomUnitData [iomUnitIdx] . configSwBootloadPort; // 3 bits;
    
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
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012"PRIo64"\n",
      1, ((word36) (bootchan) << 27) | port);
    

    // 10

    // following verified correct; instr 362 will not yield 1572 with a 
    // different shift

   // word after PCW (used by program)

    M [2] = ((word36) base_addr << 18) | (pi_base) | iom_num;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012"PRIo64"\n",
      2,  ((word36) base_addr << 18) | (pi_base) | iom_num);
    

    // 11

    // IDCW for read binary

    M [3] = (cmd << 30) | (dev << 24) | 0700000;
    sim_debug (DBG_INFO, & iom_dev, "M [%08o] <= %012"PRIo64"\n",
      3, (cmd << 30) | (dev << 24) | 0700000);
    
  }

static t_stat bootSvc (UNIT * unitp)
  {
    uint iomUnitIdx = (uint) (unitp - bootChannelUnit);
    // the docs say press sysinit, then boot; simh doesn't have an
    // explicit "sysinit", so we ill treat  "reset iom" as sysinit.
    // The docs don't say what the behavior is is you dont press sysinit
    // first so we wont worry about it.

    sim_debug (DBG_DEBUG, & iom_dev, "%s: starting on IOM %c\n",
      __func__, 'A' + iomUnitIdx);

    // initialize memory with boot program
    initMemoryIOM (iomUnitIdx);

    // This is needed to reset the interrupt mask registers; Multics tampers
    // with runtime values, and mucks up rebooting on multi-CPU systems.
    scu_reset (NULL);

    // Start the remote console listener
    startRemoteConsole ();

    // Start the FNP dialup listener
    startFNPListener ();

    // simulate $CON
    iom_interrupt (0 /*ASSUME0*/, iomUnitIdx);

    sim_debug (DBG_DEBUG, &iom_dev, "%s finished\n", __func__);

    // returning OK from the simh BOOT command causes simh to start the CPU
    return SCPE_OK;
  }

#ifdef PANEL
void doBoot (void)
  {
    bootSvc (& bootChannelUnit [0]);
  }
#endif

static t_stat iomBoot (int unitNum, UNUSED DEVICE * dptr)
  {
    if (unitNum < 0 || unitNum >= (int) iom_dev . numunits)
      {
        sim_printf ("iomBoot: Invalid unit number %d\n", unitNum);
        return SCPE_ARG;
      }
    uint iomUnitIdx = (uint) unitNum;
#if 0
    // initialize memory with boot program
    initMemoryIOM ((uint)iomUnitIdx);

    // simulate $CON
    iom_interrupt (0 /*ASSUME0*/, iomUnitIdx);

#else
    sim_activate (& bootChannelUnit [iomUnitIdx], sys_opts . iom_times . boot_time );
    // returning OK from the simh BOOT command causes simh to start the CPU
#endif
    return SCPE_OK;
  }

static t_stat iomShowMbx (UNUSED FILE * st, 
                            UNUSED UNIT * uptr, UNUSED int val, 
                            UNUSED const void * desc)
  {
    return SCPE_OK;
  }


static t_stat iomShowUnits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED const void * desc)
  {
    sim_printf ("Number of IOM units in system is %d\n", iom_dev . numunits);
    return SCPE_OK;
  }

static t_stat iomSetUnits (UNUSED UNIT * uptr, UNUSED int value, const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_IOM_UNITS_MAX)
      return SCPE_ARG;
    if (n > 2)
      sim_printf ("Warning: Multics supports 2 IOMs maximum\n");
    iom_dev . numunits = (unsigned) n;
    return SCPE_OK;
  }

static t_stat iomShowConfig (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                             UNUSED const void * desc)
  {
    uint iomUnitIdx = (uint) IOM_UNIT_IDX (uptr);
    if (iomUnitIdx >= iom_dev . numunits)
      {
        sim_printf ("error: invalid unit number %u\n", iomUnitIdx);
        return SCPE_ARG;
      }

    sim_printf ("IOM unit number %u\n", iomUnitIdx);
    iomUnitData_t * p = iomUnitData + iomUnitIdx;

    char * os = "<out of range>";
    switch (p -> configSwOS)
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
    switch (p -> configSwBootloadCardTape)
      {
        case CONFIG_SW_BLCT_CARD:
          blct = "CARD";
          break;
        case CONFIG_SW_BLCT_TAPE:
          blct = "TAPE";
          break;
      }

    sim_printf ("Allowed Operating System: %s\n", os);
    sim_printf ("IOM Base Address:         %03o(8)\n", p -> configSwIomBaseAddress);
    sim_printf ("Multiplex Base Address:   %04o(8)\n", p -> configSwMultiplexBaseAddress);
    sim_printf ("Bootload Card/Tape:       %s\n", blct);
    sim_printf ("Bootload Tape Channel:    %02o(8)\n", p -> configSwBootloadMagtapeChan);
    sim_printf ("Bootload Card Channel:    %02o(8)\n", p -> configSwBootloadCardrdrChan);
    sim_printf ("Bootload Port:            %02o(8)\n", p -> configSwBootloadPort);
    sim_printf ("Port Address:            ");
    int i;
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %03o", p -> configSwPortAddress [i]);
    sim_printf ("\n");
    sim_printf ("Port Interlace:          ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortInterface [i]);
    sim_printf ("\n");
    sim_printf ("Port Enable:             ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortEnable [i]);
    sim_printf ("\n");
    sim_printf ("Port Sysinit Enable:     ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortSysinitEnable [i]);
    sim_printf ("\n");
    sim_printf ("Port Halfsize:           ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortHalfsize [i]);
    sim_printf ("\n");
    sim_printf ("Port Storesize:           ");
    for (i = 0; i < N_IOM_PORTS; i ++)
      sim_printf (" %3o", p -> configSwPortStoresize [i]);
    sim_printf ("\n");
    
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

    { NULL, 0, 0, NULL }
  };

static t_stat iomSetConfig (UNIT * uptr, UNUSED int value, const char * cptr, UNUSED void * desc)
  {
    uint iomUnitIdx = (uint) IOM_UNIT_IDX (uptr);
    if (iomUnitIdx >= iom_dev . numunits)
      {
        sim_printf ("error: iomSetConfig: invalid unit number %d\n", iomUnitIdx);
        return SCPE_ARG;
      }

    iomUnitData_t * p = iomUnitData + iomUnitIdx;

    static uint port_num = 0;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("iomSetConfig", cptr, iom_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 

            case -1: // done
              break;

            case 0: // OS
              p -> configSwOS = (enum configSwOs_t) v;
              break;

            case 1: // BOOT
              p -> configSwBootloadCardTape = (enum configSwBlCT_t) v;
              break;

            case 2: // IOM_BASE
              p -> configSwIomBaseAddress = (word12) v;
              break;

            case 3: // MULTIPLEX_BASE
              p -> configSwMultiplexBaseAddress = (word9) v;
              break;

            case 4: // TAPECHAN
              p -> configSwBootloadMagtapeChan = (word6) v;
              break;

            case 5: // CARDCHAN
              p -> configSwBootloadCardrdrChan = (word6) v;
              break;

            case 6: // SCUPORT
              p -> configSwBootloadPort = (word3) v;
              break;

            case 7: // PORT
              port_num = (uint) v;
              break;

#if 0
                // all of the remaining assume a valid value in port_num
                if (/* port_num < 0 || */ port_num > 7)
                  {
                    sim_printf ("error: iomSetConfig: cached PORT value out of range: %d\n", port_num);
                    break;
                  } 
#endif
            case 8: // ADDR
              p -> configSwPortAddress [port_num] = (uint) v;
              break;

            case 9: // INTERLACE
              p -> configSwPortInterface [port_num] = (uint) v;
              break;

            case 10: // ENABLE
              p -> configSwPortEnable [port_num] = (uint) v;
              break;

            case 11: // INITENABLE
              p -> configSwPortSysinitEnable [port_num] = (uint) v;
              break;

            case 12: // HALFSIZE
              p -> configSwPortHalfsize [port_num] = (uint) v;
              break;

            case 13: // STORE_SIZE
              p -> configSwPortStoresize [port_num] = (uint) v;
              break;

            default:
              sim_printf ("error: iomSetConfig: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }

// Used by scu_cioc() to schedule connects

static t_stat iom_action (UNIT *up)
  {
    // Recover the stash parameters
    uint scuUnitNum = (uint) (up -> u3);
    uint iomUnitIdx = (uint) (up -> u4);
//sim_printf ("int %u %u\n", scuUnitNum, iomUnitIdx);
    iom_interrupt (scuUnitNum, iomUnitIdx);
    return SCPE_OK;
  }
