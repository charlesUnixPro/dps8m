//
// \file dps8_iom.c
// \project dps8
// \date 9/21/12
//  Adapted by Harry Reed on 9/21/12.
//  27Nov13 CAC - Reference document is
//    431239854 6000B IOM Spec Jul75
//    This is a 6000B IOM emulator. 
//

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


#include <stdio.h>
#include <sys/time.h>

#include "dps8.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "dps8_scu.h"
#include "dps8_iom.h"
#include "dps8_sys.h"
 
#define ASSUME_CPU_0 0

// Default
#define N_IOM_UNITS 1

#define N_IOM_PORTS 8

// The number of devices that a dev_code can address (6 bit number)

#define N_DEV_CODES 64

#define IOM_CONNECT_CHAN 2U

////////////////////////////////////////////////////////////////////////////////
//
// simh interface
//
////////////////////////////////////////////////////////////////////////////////

#define IOM_UNIT_NUM(uptr) ((uptr) - iomUnit)

// static t_stat iom_svc (UNIT * unitp);
static UNIT iomUnit [N_IOM_UNITS_MAX] =
  {
    { UDATA (NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA (NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA (NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA (NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL }
  };

static t_stat iomShowMbx (FILE * st, UNIT * uptr, int val, void * desc);
//static t_stat iomAnalyzeMbx (FILE * st, UNIT * uptr, int val, void * desc);
static t_stat iomShowConfig (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat iomSetConfig (UNIT * uptr, int value, char * cptr, void * desc);
static t_stat iomShowUnits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat iomSetUnits (UNIT * uptr, int value, char * cptr, void * desc);
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
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { "TRACE", DBG_TRACE },
    { NULL, 0 }
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
    NULL         // description
  };

static t_stat bootSvc (UNIT * unitp);
static UNIT bootChannelUnit [N_IOM_UNITS_MAX] =
  {
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL},
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL},
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL},
    { UDATA (& bootSvc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL}
  };


////////////////////////////////////////////////////////////////////////////////
//
// Data structures
//
////////////////////////////////////////////////////////////////////////////////

enum configSwOs_
  { 
    CONFIG_SW_STD_GCOS, 
    CONFIG_SW_EXT_GCOS, 
    CONFIG_SW_MULTICS  // "Paged"
  };

// Boot device: CARD/TAPE;
enum configSwBlCT_ { CONFIG_SW_BLCT_CARD, CONFIG_SW_BLCT_TAPE };

struct unitData
  {
    // Configuration switches
    
    // Interrupt multiplex base address: 12 toggles
    word12 configSwIomBaseAddress;

    // Mailbox base aka IOM base address: 9 toggles
    // Note: The IOM number is encoded in the lower two bits
    word9 configSwMultiplexBaseAddress;

    // OS: Three position switch: GCOS, EXT GCOS, Multics
    enum configSwOs_ configSwOS; // = CONFIG_SW_MULTICS;

    // Bootload device: Toggle switch CARD/TAPE
    enum configSwBlCT_ configSwBootloadCardTape; // = CONFIG_SW_BLCT_TAPE; 

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

  };

static struct unitData unitData [N_IOM_UNITS_MAX];


static struct iom
  {
    struct channels
      {
        enum dev_type type;
        enum chan_type ctype;
        DEVICE * dev; // attached device; points into sim_devices[]
        uint devUnitNum; // Which unit of the attached device
        UNIT * board; 
        iomCmd * iomCmd;
      } channels [MAX_CHANNELS] [N_DEV_CODES];
    word27 pageTablePtr [MAX_CHANNELS];
  } iom [N_IOM_UNITS_MAX];

static struct
  {
    bool inuse;
    int scuUnitNum;
    int scuPortNum;
  } cablesFromScus [N_IOM_UNITS_MAX] [N_IOM_PORTS];




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

static int send_general_interrupt (uint iomUnitNum, uint chanNum, enum iomImwPics pic);

////////////////////////////////////////////////////////////////////////////////
//
// emulator interface
//
////////////////////////////////////////////////////////////////////////////////

void fetch_abs_word (word24 addr, word36 *data)
  {
    core_read (addr, data);
  }

void store_abs_word (word24 addr, word36 data)
  {
    core_write (addr, data);
  }

void fetch_abs_pair (word24 addr, word36 * even, word36 * odd)
  {
    core_read2 (addr, even, odd);
  }

static void store_abs_pair (word24 addr, word36 even, word36 odd)
  {
    core_write2 (addr, even, odd);
  }

////////////////////////////////////////////////////////////////////////////////
//
// IOM data structure
//
////////////////////////////////////////////////////////////////////////////////

// IOM channel status

enum iomCentralStatus 
  {
    iomCsNormal = 00,
    //  tally was zero for an update LPW (LPW bit 21==0) when the LPW
    //  was fetched and TRO-signal (bit 22) is on
    iomCsLpwTro = 01,
    iomCsTwoTdcws = 02,
    iomCsBndyVio = 03,
    iomCsIdcwInResMode = 05,
    iomCsCpDiscrepancy = 06,
  };

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

typedef enum iomSysFaults
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
  } iomSysFaults;

static void iomFault (uint iomUnitNum, uint chanNum, const char * who, 
                      int is_sys, iomFaultServiceRequest req, 
                      iomSysFaults signal);


uint mbx_loc (uint iomUnitNum, uint chanNum)
  {
    word12 base = unitData [iomUnitNum] . configSwIomBaseAddress;
    word24 base_addr = ((word24) base) << 6; // 01400
    word24 mbx = base_addr + 4 * chanNum;
    sim_debug (DBG_INFO, & iom_dev, "%s: IOM %c, chan %d is %012o\n",
      __func__, 'A' + iomUnitNum, chanNum, mbx);
    return mbx;
  }

static word24 UNUSED buildAUXPTWaddress (uint iomUnitNum, int chanNum)
  {

//    0      5 6            16 17  18              21  22  23
//   ---------------------------------------------------------
//   | Zeroes | IOM Base Address  |                          |
//   ---------------------------------------------------------
//                         |    Channel Number       | 1 | 1 |
//                         -----------------------------------
// XXX Assuming 16 and 17 are or'ed.

    word12 IOMBaseAddress = unitData [iomUnitNum] . configSwIomBaseAddress;
    word24 addr = (IOMBaseAddress & MASK12) << 6;
    addr |= (chanNum & MASK6) << 2;
    addr |= 03;
    return addr;
  }

static word24 buildDDSPTWaddress (word18 ptPtr, word6 pageNumber)
  {

//    0      5 6            16 17  18               21  22  23
//   ----------------------------------------------------------
//   | Page Table Pointer         |                           |
//   ----------------------------------------------------------
//                         | Direct Channel Addr 6-13 | 1 | 1 |
//                         ------------------------------------

    word24 addr = (ptPtr & MASK18) << 6;
    addr |= (pageNumber & MASK6) << 2;
    addr |= 03;
    return addr;
  }

static void UNUSED fetchDDSPTW (uint iomUnitNum, int chanNum, word6 pageNumber)
  {
    word24 addr = buildDDSPTWaddress (iomChannelData [iomUnitNum] [chanNum] . ptPtr, pageNumber);
    fetch_abs_word (addr, & iomChannelData [iomUnitNum] [chanNum] . PTW_DCW);
  }

static word24 buildIDSPTWaddress (word18 ptPtr, word1 seg, word8 pageNumber)
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

    word24 addr = (ptPtr & MASK12) << 6;
    addr += (seg & 01) << 8;
    addr += pageNumber;
    return addr;
  }

static void UNUSED fetchIDSPTW (uint iomUnitNum, int chanNum, word1 seg, word6 pageNumber)
  {
    word24 addr = buildIDSPTWaddress (iomChannelData [iomUnitNum] [chanNum] . ptPtr, seg, pageNumber);
    fetch_abs_word (addr, & iomChannelData [iomUnitNum] [chanNum] . PTW_DCW);
  }


static word24 buildLPWPTWaddress (word18 ptPtr, word1 seg, word8 pageNumber)
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

    word24 addr = (ptPtr & MASK12) << 6;
    addr += (seg & 01) << 8;
    addr += pageNumber;
    return addr;
  }

static void UNUSED fetchLPWPTW (uint iomUnitNum, int chanNum, word1 seg, word6 pageNumber)
  {
    word24 addr = buildLPWPTWaddress (iomChannelData [iomUnitNum] [chanNum] . ptPtr, seg, pageNumber);
    fetch_abs_word (addr, & iomChannelData [iomUnitNum] [chanNum] . PTW_LPW);
  }

//
// fetch_and_parse_lpw ()
//
// Parse the words at "addr" into a lpw_t.
//

void fetch_and_parse_lpw (lpw_t * p, uint addr, bool is_conn)
  {
    word36 word0;
    fetch_abs_word (addr, & word0);
sim_printf ("lpw %012llo\n", word0);
    p -> dcw_ptr = word0 >> 18;
    p -> ires = getbits36 (word0, 18, 1);
    p -> hrel = getbits36 (word0, 19, 1);
    p -> ae = getbits36 (word0, 20, 1);
    p -> nc = getbits36 (word0, 21, 1);
    p -> trunout = getbits36 (word0, 22, 1);
    p -> srel = getbits36 (word0, 23, 1);
    p -> tally = getbits36 (word0, 24, 12); // initial value treated as unsigned
    
    if (! is_conn)
      {
        word36 word1;
        fetch_abs_word (addr +1, & word1);
#if 0
        // Ignore 2nd word on connect channel
        // following not valid for paged mode; see B15; but maybe IOM-B non existant
sim_printf ("lpw2 %012llo\n", word1);
        p -> lbnd = getbits36 (word1, 0, 9);
        p -> size = getbits36 (word1, 9, 9);
        p -> idcw = getbits36 (word1, 18, 18);
#else
        // XXX Assume Multics/Paged mode
        p -> lbnd = getbits36 (word1, 0, 18);
        p -> size = getbits36 (word1, 18, 18);

#endif
      }
    else
      {
        p -> lbnd = 0;
        p -> size = 0;
        // XXX Not in paged mode
        //p -> idcw = (uint)-1;
      }
  }

//
// decode_idcw ()
//
// Decode an idcw word or pcw word pair
//

void decode_idcw (uint iomUnitNum, pcw_t *p, bool is_pcw, 
                  word36 word0, word36 word1)
  {
    p -> dev_cmd = getbits36 (word0, 0, 6);
    p -> dev_code = getbits36 (word0, 6, 6);
    p -> ext = getbits36 (word0, 12, 6);
    p -> cp = getbits36 (word0, 18, 3);
    p -> mask = getbits36 (word0, 21, 1);
    p -> control = getbits36 (word0, 22, 2);
    p -> chan_cmd = getbits36 (word0, 24, 6);
    p -> chan_data = getbits36 (word0, 30, 6);
    if (is_pcw)
      {
        p -> chan = getbits36 (word1, 3, 6);
        p -> ptPtr = getbits36 (word1, 9, 18);
        p -> ptp = getbits36 (word1, 27, 1);
        if (p -> ptp != 0 &&
            unitData [iomUnitNum] . configSwOS != CONFIG_SW_MULTICS)
          {
            sim_debug (DBG_ERR, &iom_dev, 
                       "%s: Page Table Pointer for model IOM-B detected\n",
                       __func__);
          }
        p -> pge = getbits36 (word1, 28, 1);
        p -> aux = getbits36 (word1, 29, 1);
if (p -> ptp)
sim_printf ("IOMB pcw ptPtr %06o pge %o aux %o\n", p -> ptPtr, p -> pge, p -> aux);
if (p -> ptp)
    iomFault (iomUnitNum, 2, "cac", 1, iomFsrList, 016);
        if (p -> pge)
          {
            sim_debug (DBG_ERR, & iom_dev, "%s: AUX set in PCW; fail\n",
                       __func__);
          }

sim_printf ("IOMB pcw ptPtr %06o pge %o aux %o\n", p -> ptPtr, p -> pge, p -> aux);
        // Don't need to bounds check p->chan; getbits36 masked it to 6 bits,
        // so it must be 0..MAX_CHANNELS - 1
        iomChannelData [iomUnitNum] [p -> chan] . ptPtr = p -> ptPtr;
        iomChannelData [iomUnitNum] [p -> chan] . ptp = p -> ptp;
        iomChannelData [iomUnitNum] [p -> chan] . pge = p -> pge;
        iomChannelData [iomUnitNum] [p -> chan] . aux = p -> aux;
      }
    else
      {
        p -> chan = 0;
        p -> ptPtr = 0;
        p -> ptp = 0;
        p -> pge = 0;
        p -> aux = 0;
      }
  }

//
// pcw2text ()
//
// Display pcw_t
//

static char * pcw2text (const pcw_t * p)
  {
    // WARNING: returns single static buffer
    static char buf[200];
    sprintf (buf, "[dev-cmd=0%o, dev-code=0%o, ext=0%o, mask=%u, ctrl=0%o, "
                  "chan-cmd=0%o, chan-data=0%o, chan=0%o]",
             p -> dev_cmd, p -> dev_code, p -> ext, p -> mask, p -> control, 
             p -> chan_cmd, p -> chan_data, p -> chan);
    return buf;
  }

//
// lpw2text ()
//
// Display an LPW
//

static char * lpw2text (const lpw_t * p, int conn)
  {
    // WARNING: returns single static buffer
    static char buf [180];
    sprintf (buf,
            "[dcw=0%o ires=%d hrel=%d ae=%d nc=%d trun=%d srel=%d tally=0%o]",
            p -> dcw_ptr, p -> ires, p -> hrel, p -> ae, p -> nc, 
            p -> trunout, p -> srel, p -> tally);
    if (! conn)
      //sprintf (buf + strlen (buf), " [lbnd=0%o size=0%o (%u) idcw=0%o]",
                //p -> lbnd, p -> size, p- >s ize, p -> idcw);
      sprintf (buf + strlen (buf), " [lbnd=0%o size=0%o (%u)]",
                p -> lbnd, p -> size, p -> size);
    return buf;
  }

//
// fetchAndParseDCW ()
//
// Parse word at "addr" into a dcw_t.
//

static void fetchAndParseDCW (uint iomUnitNum, uint chanNum, dcw_t * p, 
                              uint32 addr, int UNUSED read_only)
  {
// XXX do the read_only thang
// XXX ticket #4
    word36 word;

    sim_debug (DBG_DEBUG, & iom_dev, "%s: addr: 0%06o\n", __func__, addr);
    fetch_abs_word (addr, & word);
    sim_debug (DBG_DEBUG, & iom_dev, "%s: dcw: 0%012llo\n", __func__, word);
sim_printf ("dcw: %012llo\n", word);

    uint cp = getbits36 (word, 18, 3);
    if (cp == 7U)
      {
        p -> type = idcw;
        decode_idcw (iomUnitNum, & p -> fields . instr, 0, word, 0ll);
        p -> fields . instr . chan = chanNum; // Real HW would not populate
//--         if (p -> fields.instr.mask && ! read_only) {
//--             // Bit 21 is extension control (EC), not a mask
//--        channel_t* chanp = get_chan (iomUnitNum, chanNum, dev_code);
//--             if (! chanp)
//--                 return;
//--             if (chanp -> lpw . srel) {
//--                 // Impossible, SREL is always zero for multics
//--                 // For non-multics, this would be allowed and we'd check
//--                 sim_debug (DBG_ERR, &iom_dev, "parse_dcw: I-DCW bit EC set but the LPW SREL bit is also set.");
//--                 cancel_run (STOP_BUG);
//--                 return;
//--             }
//--             sim_debug (DBG_WARN, &iom_dev, "WARN: parse_dcw: Channel %d: Replacing LPW AE %#o with %#o\n", chanNum, chanp->lpw.ae, p->fields.instr.ext);
//--             chanp->lpw.ae = p->fields.instr.ext;
//--             cancel_run (STOP_BKPT /* STOP_IBKPT */);
//--         }
      }
    else
      {
        uint type = getbits36 (word, 22, 2);
        if (type == 2U)
          {
            // transfer
            p -> type = tdcw;
            p -> fields . xfer . addr = getbits36 (word, 0, 18);
            p -> fields . xfer . seg = getbits36 (word, 31, 1);
            p -> fields . xfer . pdta = getbits36 (word, 32, 1);
            p -> fields . xfer . pdcw = getbits36 (word, 33, 1);
            p -> fields . xfer . res = getbits36 (word, 34, 1);
            p -> fields . xfer . rel = getbits36 (word, 35, 1);
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

//
// dcw2text ()
//
// Display a dcw_t
//
//

static char * dcw2text (const dcw_t * p)
  {
    // WARNING: returns single static buffer
    static char buf[200];
    if (p -> type == ddcw)
      {
        uint dtype = p -> fields . ddcw . type;
        const char* type =
        (dtype == 0) ? "IOTD" :
        (dtype == 1) ? "IOTP" :
        (dtype == 2) ? "transfer" :
        (dtype == 3) ? "IONTP" :
        "<illegal>";
        sprintf (buf, "D-DCW: type=%u (%s), addr=%06o, cp=0%o, tally=0%o "
                      "(%d) tally-ctl=%d",
                 dtype, type, p -> fields . ddcw . daddr, 
                 p -> fields . ddcw . cp, p -> fields . ddcw . tally,
                 p -> fields . ddcw . tally, p -> fields . ddcw . tctl);
      }
    else if (p -> type == tdcw)
      {
        sprintf (buf, "T-DCW: ...");
      }
    else if (p -> type == idcw)
      {
        sprintf (buf, "I-DCW: %s", pcw2text (&p -> fields . instr));
      }
    else
      {
        strcpy (buf, "<not a dcw>");
      }
    return buf;
  }

//
// writeLPW ()
//
// Write an LPW into main memory
///

static int writeLPW (uint chanNum, word24 chanloc, const lpw_t * p, bool UNUSED x)
  {
    sim_debug (DBG_DEBUG, & iom_dev, 
               "%s: Chan 0%o: Addr 0%o had %012llo %012llo\n", 
               __func__, chanNum, chanloc, M [chanloc], M [chanloc + 1]);

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
    
    // In non-paged mode, the LPWx is writtne to reflect p->idcw update;
    // we only support paged mode.
#ifdef SUPRPORT_GCOS
    int is_conn = chanNum == IOM_CONNECT_CHAN;
    if (! is_conn && x)
      {
#if 0
        word36 word1 = setbits36 (0, 0, 9, p -> lbnd);
        word1 = setbits36 (word1, 9, 9, p -> size);
        word1 = setbits36 (word1, 18, 18, p -> idcw);
#else
        // Paged mode
        word36 word1 = setbits36 (0, 0, 18, p -> lbnd);
        word1 = setbits36 (word1, 18, 18, p -> size);
#endif
        store_abs_word (chanloc + 1, word1);
      }
#endif
    sim_debug (DBG_DEBUG, & iom_dev, 
               "%s: Chan 0%o: Addr 0%o now %012llo %012llo\n", 
               __func__, chanNum, chanloc, M [chanloc], M [chanloc + 1]);
    return 0;
  }

// Map memory to port
// -1 -- no mapping
static int iomScpageMap [N_IOM_UNITS_MAX] [N_SCPAGES];

static void setup_iom_scpage_map (void)
  {
    sim_debug (DBG_DEBUG, & cpu_dev,
      "%s: setup_iom_scpage_map: SCPAGE %d N_SCPAGES %d MAXMEMSIZE %d\n", 
      __func__, SCPAGE, N_SCPAGES, MAXMEMSIZE);

    for (uint iomUnitNum = 0; iomUnitNum < iom_dev . numunits; iomUnitNum ++)
      {
        // Initalize to unmapped
        for (int pg = 0; pg < (int) N_SCPAGES; pg ++)
          iomScpageMap [iomUnitNum] [pg] = -1;
    
        struct unitData * p = unitData + iomUnitNum;
        // For each port (which is connected to a SCU
        for (int port_num = 0; port_num < N_IOM_PORTS; port_num ++)
          {
            if (! p -> configSwPortEnable [port_num])
              continue;
            // Calculate the amount of memory in the SCU in words
            uint store_size = p -> configSwPortStoresize [port_num];
            uint sz = 1 << (store_size + 16);
    
            // Calculate the base address of the memor in wordsy
            uint assignment = switches . assignment [port_num];
            uint base = assignment * sz;
    
            // Now convert to SCPAGES
            sz = sz / SCPAGE;
            base = base / SCPAGE;
    
            sim_debug (DBG_DEBUG, & cpu_dev,
              "%s: unit:%u port:%d ss:%u as:%u sz:%u ba:%u\n",
              __func__, iomUnitNum, port_num, store_size, assignment, sz, 
              base);
    
            for (uint pg = 0; pg < sz; pg ++)
              {
                uint scpg = base + pg;
                if (/*scpg >= 0 && */ scpg < N_SCPAGES)
                  iomScpageMap [iomUnitNum] [scpg] = port_num;
              }
          }
      }
    for (uint iomUnitNum = 0; iomUnitNum < iom_dev . numunits; iomUnitNum ++)
        for (int pg = 0; pg < (int) N_SCPAGES; pg ++)
          sim_debug (DBG_DEBUG, & cpu_dev, "%s: %d:%d\n", 
            __func__, pg, iomScpageMap [iomUnitNum] [pg]);
  }
   
static int queryIomScpageMap (uint iomUnitNum, word24 addr)
  {
    uint scpg = addr / SCPAGE;
    if (scpg < N_SCPAGES)
      return iomScpageMap [iomUnitNum] [scpg];
    return -1;
  }

/*
 * iom_init ()
 *
 *  Once-only initialization
 */

void iom_init (void)
  {
    sim_debug (DBG_INFO, & iom_dev, "%s: running.\n", __func__);

    // sets iom [iomUnitNum] . channels [chanNum] [dev_code] . type to DEVT_NONE
    memset (& iom, 0, sizeof (iom));
    memset (cablesFromScus, 0, sizeof (cablesFromScus));
  }

////////////////////////////////////////////////////////////////////////////////
//
// IOM BOOT code
//
////////////////////////////////////////////////////////////////////////////////

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

/*
 * init_memory_iom ()
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

static void init_memory_iom (uint iomUnitNum)
  {
    // The presence of a 0 in the top six bits of word 0 denote an IOM boot
    // from an IOX boot
    
    // " The channel number ("Chan#") is set by the switches on the IOM to be
    // " the channel for the tape subsystem holding the bootload tape. The
    // " drive number for the bootload tape is set by switches on the tape
    // " MPC itself.
    
    sim_debug (DBG_INFO, & iom_dev,
      "%s: Performing load of eleven words from IOM %c bootchannel to memory.\n",
      __func__, 'A' + iomUnitNum);

    word12 base = unitData [iomUnitNum] . configSwIomBaseAddress;

    // bootload_io.alm insists that pi_base match
    // template_slt_$iom_mailbox_absloc

    //uint pi_base = unitData [iomUnitNum] . configSwMultiplexBaseAddress & ~3;
    word36 pi_base = (((word36) unitData [iomUnitNum] . configSwMultiplexBaseAddress)  << 3) |
                     (((word36) (unitData [iomUnitNum] . configSwIomBaseAddress & 07700U)) << 6) ;
    word3 iom_num = ((word36) unitData [iomUnitNum] . configSwMultiplexBaseAddress) & 3; 
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
    
    enum configSwBlCT_ bootdev = unitData [iomUnitNum] . configSwBootloadCardTape;

    word6 bootchan;
    if (bootdev == CONFIG_SW_BLCT_CARD)
      bootchan = unitData [iomUnitNum] . configSwBootloadCardrdrChan;
    else // CONFIG_SW_BLCT_TAPE
      bootchan = unitData [iomUnitNum] . configSwBootloadMagtapeChan;


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
    word3 port = unitData [iomUnitNum] . configSwBootloadPort; // 3 bits;
    
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

////////////////////////////////////////////////////////////////////////////////
//
// IOM emulator
//
////////////////////////////////////////////////////////////////////////////////

static void iomFault (uint iomUnitNum, uint chanNum, const char * who, 
                      int is_sys, iomFaultServiceRequest req, 
                      iomSysFaults signal)
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
    sim_debug (DBG_WARN, & iom_dev, 
               "%s: Fault for IOM %c channel %d in %s: is_sys=%d, signal=%d\n", 
               who, 'A' + iomUnitNum, chanNum, who, is_sys, signal);
    sim_debug (DBG_WARN, & iom_dev, 
               "%s: Not setting status word.\n", who);

    if (is_sys)
      {
        word36 faultWord = 0;
        putbits36 (& faultWord, 9, 9, chanNum);
        putbits36 (& faultWord, 18, 5, req);
        // IAC, bits 26..29
        putbits36 (& faultWord, 30, 6, signal);
    
        uint mbx = mbx_loc (iomUnitNum, chanNum);
        dcw_t dcw;
        fetchAndParseDCW (iomUnitNum, chanNum, & dcw, mbx + 3, 0);
        if (dcw . type != ddcw)
          {
            sim_debug (DBG_ERR, & iom_dev,
                       "%s: expected a DDCW; fail\n", __func__);
sim_printf ("%s: expected a DDCW; fail\n", __func__);
            return;
          }
        // XXX Assuming no address extension or paging non-sense
        uint addr = dcw . fields . ddcw . daddr;
sim_printf ("%s: storing fault code @ %06o\n", __func__, addr);
        store_abs_word (addr, faultWord);

        send_general_interrupt (iomUnitNum, 1, imwSystemFaultPic);

        word36 ddcw;
        fetch_abs_word (mbx, & ddcw);
        // incr addr
        putbits36 (& ddcw, 0, 18, (getbits36 (ddcw, 0, 18) + 1) & MASK18);
        // decr tally
        putbits36 (& ddcw, 24, 12, (getbits36 (ddcw, 24, 12) - 1) & MASK12);
        store_abs_word (mbx, ddcw);
      }
    else
      {
sim_printf ("iom user fault ignored"); // XXX
      }
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

int status_service (uint iomUnitNum, uint chanNum, uint dev_code, word12 stati, 
                    word6 rcount, word12 residue, word3 char_pos, bool is_read)
  {
    // See page 33 and AN87 for format of y-pair of status info
    
//--     channel_t* chanp = get_chan (iomUnitNum, chanNum, dev_code);
//--     if (chanp == NULL)
//--         return 1;

    // BUG: much of the following is not tracked
    
    word36 word1, word2;
    word1 = 0;
    //word1 = setbits36 (word1, 0, 1, 1);
    //word1 = setbits36 (word1, 1, 1, power_off);
    //word1 = setbits36 (word1, 2, 4, major);
    //word1 = setbits36 (word1, 6, 6, sub);
    word1 = setbits36 (word1, 0, 12, stati);

    word1 = setbits36 (word1, 12, 1, 1); // BUG: even/odd
    word1 = setbits36 (word1, 13, 1, 1); // BUG: marker int
    word1 = setbits36 (word1, 14, 2, 0);
    word1 = setbits36 (word1, 16, 1, 0); // BUG: initiate flag
    word1 = setbits36 (word1, 17, 1, 0);
#if 0
    // BUG: Unimplemented status bits:
    word1 = setbits36 (word1, 18, 3, chan_status.chan_stat);
    word1 = setbits36 (word1, 21, 3, chan_status.iom_stat);
    word1 = setbits36 (word1, 24, 6, chan_status.addr_ext);
#endif
    word1 = setbits36 (word1, 30, 6, rcount);
    
    word2 = 0;
#if 0
    // BUG: Unimplemented status bits:
    word2 = setbits36 (word2, 0, 18, chan_status.addr);
    word2 = setbits36 (word2, 21, 1, chanp->status.read);
    word2 = setbits36 (word2, 22, 2, chan_status.type);
    word2 = setbits36 (word2, 24, 12, chan_status.dcw_residue);
#endif
    word2 = setbits36 (word2, 18, 3, char_pos);
    word2 = setbits36 (word2, 21, 1, is_read);
    word2 = setbits36 (word2, 24, 12, residue);
    
    // BUG: need to write to mailbox queue
    
    // T&D tape does *not* expect us to cache original SCW, it expects us to
    // use the SCW loaded from tape.
    
#if 1
    uint chanloc = mbx_loc (iomUnitNum, chanNum);
    word24 scw = chanloc + 2;
    word36 sc_word;
    fetch_abs_word (scw, & sc_word);
    word18 addr = getbits36 (sc_word, 0, 18);   // absolute
    // BUG: probably need to check for y-pair here, not above
    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: Writing status for chanNum %d dev_code %d to 0%o=>0%o\n",
               __func__, chanNum, dev_code, scw, addr);
    sim_debug (DBG_TRACE, & iom_dev,
               "Writing status for chanNum %d dev_code %d to 0%o=>0%o\n",
               chanNum, dev_code, scw, addr);
#else
    word36 sc_word = chanp->scw;
    int addr = getbits36 (sc_word, 0, 18);   // absolute
    if (addr % 2 == 1) {    // 3.2.4
        sim_debug (DBG_WARN, &iom_dev, "status_service: Status address 0%o is not even\n", addr);
        // store_abs_pair () call below will fix address
    }
    sim_debug (DBG_DEBUG, &iom_dev, "status_service: Writing status for chan %d to %#o\n",
            chanNum, addr);
#endif
    sim_debug (DBG_DEBUG, & iom_dev, "%s: Status: 0%012llo 0%012llo\n",
               __func__, word1, word2);
    sim_debug (DBG_TRACE, & iom_dev, "Status: 0%012llo 0%012llo\n",
               word1, word2);
    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: Status: 0%04o, (12)e/o=%c, (13)marker=%c, "
               "(14..15)Z, 16(Z?), 17(Z)\n",
               __func__, stati,
               '1', // BUG
               'Y');   // BUG
    uint lq = getbits36 (sc_word, 18, 2);
    uint tally = getbits36 (sc_word, 24, 12);
#if 1
    if (lq == 3)
      {
        sim_debug (DBG_WARN, &iom_dev, 
                   "%s: SCW for channel %d has illegal LQ\n",
                   __func__, chanNum);
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
                  tally --;
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
        sc_word = setbits36 (sc_word, 24, 12, tally);
        sc_word = setbits36 (sc_word, 0, 18, addr);
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

/*
 * send_general_interrupt ()
 *
 * Send an interrupt from the IOM to the CPU.
 *
 */

static int send_general_interrupt (uint iomUnitNum, uint chanNum, enum iomImwPics pic)
  {
    uint imw_addr;
    uint chan_group = chanNum < 32 ? 1 : 0;
    uint chan_in_group = chanNum & 037;
    uint interrupt_num = iomUnitNum | (chan_group << 2) | ((uint) pic << 3);
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
    uint pi_base = unitData [iomUnitNum] . configSwMultiplexBaseAddress & ~3;
    imw_addr = (pi_base << 3) | interrupt_num;

    sim_debug (DBG_NOTIFY, & iom_dev, 
               "%s: IOM %c, channel %d (%#o), level %d; "
               "Interrupt %d (%#o).\n", 
               __func__, 'A' + iomUnitNum, chanNum, chanNum, pic, interrupt_num, 
               interrupt_num);
    word36 imw;
    (void) fetch_abs_word (imw_addr, &imw);
    // The 5 least significant bits of the channel determine a bit to be
    // turned on.
    sim_debug (DBG_DEBUG, & iom_dev, 
               "%s: IMW at %#o was %012llo; setting bit %d\n", 
               __func__, imw_addr, imw, chan_in_group);
    imw = setbits36 (imw, chan_in_group, 1, 1);
    sim_debug (DBG_INFO, & iom_dev, 
               "%s: IMW at %#o now %012llo\n", __func__, imw_addr, imw);
    (void) store_abs_word (imw_addr, imw);
    
// XXX this should call scu_svc

    uint base = unitData [iomUnitNum] . configSwIomBaseAddress;
    uint base_addr = base << 6; // 01400
    // XXX this is wrong; I believe that the SCU unit number should be
    // calculated from the Port Configuration Address Assignment switches
    // For now, however, the same information is in the CPU config. switches, so
    // this should result in the same values.
    int cpu_port_num = queryIomScpageMap (iomUnitNum, base_addr);
    int scuUnitNum;
    if (cpu_port_num >= 0)
      scuUnitNum = query_scu_unit_num (ASSUME_CPU_0, cpu_port_num);
    else
      scuUnitNum = 0;
    // XXX Print warning
    if (scuUnitNum < 0)
      scuUnitNum = 0;
    return scu_set_interrupt ((uint)scuUnitNum, interrupt_num);
  }

#if 0
/*
 * send_marker_interrupt ()
 *
 * Send a "marker" interrupt to the CPU.
 *
 * Channels send marker interrupts to indicate normal completion of
 * a PCW or IDCW if the control field of the PCW/IDCW has a value
 * of three.
 */

static int send_marker_interrupt (uint iomUnitNum, int chanNum)
{
    return send_general_interrupt (uiomUnitNum, chanNum, imwMarkerPic);
}
#endif

/*
 * send_terminate_interrupt ()
 *
 * Send a "terminate" interrupt to the CPU.
 *
 * Channels send a terminate interrupt after doing a status service.
 *
 */

int send_terminate_interrupt (uint iomUnitNum, uint chanNum)
  {
    return send_general_interrupt (iomUnitNum, chanNum, imwTerminatePic);
  }


//
// iomListService
//   NOT connect channel!

int iomListService (uint iomUnitNum, int chanNum, dcw_t * dcwp, int * ptro)
  {

    lpw_t lpw; 
    int user_fault_flag = iomCsNormal;
    int tdcw_count = 0;

    uint chanloc = mbx_loc (iomUnitNum, chanNum);

    // Eliding scratchpad, so always first service.

    // 4.3.1a: FIRST_SERVICE == YES

    fetch_and_parse_lpw (& lpw, chanloc, false);

//sim_printf ("initing ptro to 0\n");
    if (ptro)
      * ptro = 0;

    // 4.3.1a: CONNECT_CHANNEL == NO
        
    // 4.3.1a: LPW 21,22 == ?
    
//sim_printf ("nc %d trunout %d\n", lpw . nc, lpw . trunout);
//sim_printf ("tally %d\n", lpw . tally);

    if (lpw . nc == 0 && lpw . trunout == 0)
      {
        // 4.3.1a: LPW 21,22 == 00
        // 4.3.1a: 256K OVERFLOW?
        if (lpw . ae == 0 && lpw . dcw_ptr  + lpw . tally >= 01000000)
          {
// XXX firstList ? iomFsrFirstList : iomFsrList
            iomFault (iomUnitNum, IOM_CONNECT_CHAN, __func__, 1, 
                       iomFsrList, iom_256K_of);
            return 1;
          }
      }
    
    if (lpw . nc == 0 && lpw . trunout == 1)
      {
    A:
//sim_printf ("tally %d\n", lpw . tally);
        // 4.3.1a: LPW 21,22 == 01
    
        // 4.3.1a: TALLY?
    
        if (lpw . tally == 0)
          {
            if (ptro)
              * ptro = 1;
            // 4.3.1a: TALLY == 0
            // 4.3.1a: SET USER FAULT FLAG
            //iomFault (iomUnitNum, chanNum, __func__, 0, 1);
            return 1;
          }
        else if (lpw . tally > 1)
          {
            if (lpw . ae == 0 && lpw . dcw_ptr  + lpw . tally >= 01000000)
              {
// XXX firstList ? iomFsrFirstList : iomFsrList
                iomFault (iomUnitNum, IOM_CONNECT_CHAN, __func__, 1, 
                          iomFsrList, iom_256K_of);
                return 1;
              }
          }
        else // tally == 1
          {
// Turning this code on does not fix bug of 2nd IDCW @ 78
// because the tally is 3
//sim_printf ("setting ptro to 1\n");
            if (ptro)
              * ptro = 1;
            //sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (a)\n", __func__);
          }

      }
    
    // 4.3.1a: LPW 20?
    
    uint dcw_addr = lpw . dcw_ptr;
    
    if (lpw . ae)
      {
        // 4.3.1a: LPW 20 == 1
        // XXX IF STD_GCOS
    
sim_printf ("adding addressExtension %o to dcw_addr %o\n", 
            iomChannelData [iomUnitNum] [chanNum] . addressExtension, dcw_addr);
        dcw_addr += 
          iomChannelData [iomUnitNum] [chanNum] . addressExtension;
        dcw_addr &= MASK18;
      }
    else
      {
        // 4.3.1a: LPW 20 == 1
 
        // 4.3.1a: PULL DCW FROM CORE USING ADDRESS EXTENSION = ZEROS
        // dcw_addr += 0;
      }
    
    sim_debug (DBG_DEBUG, & iom_dev, "%s: DCW @ 0%06o\n", 
               __func__, dcw_addr);
        
    // XXX check 1 bit; read_only
    fetchAndParseDCW (iomUnitNum, chanNum, dcwp, dcw_addr, 1);

    // 4.3.1b: C
    
    sim_debug (DBG_DEBUG, & iom_dev, "%s: DCW type %o\n", 
               __func__, dcwp -> type);
    // 4.3.1b: IDCW?
        
sim_printf ("dcw type %o\n", dcwp -> type);
    if (dcwp -> type == idcw)
      {
        // 4.3.1b: IDCW == YES
    
        // 4.3.1b: LPW 18 RES?
    
        if (lpw . ires)
          {
            // 4.3.1b: LPW 18 RES == YES
            user_fault_flag = iomCsIdcwInResMode;
          }

        // In this context, mask is ec.
        if (dcwp -> fields . instr . mask)
          {
sim_printf ("setting addressExtension to %o from IDCW\n",
  dcwp -> fields . instr . ext);
            iomChannelData [iomUnitNum] [chanNum] . addressExtension =
              dcwp -> fields . instr . ext;
          }

        goto D;
      }
    
    // 4.3.1b: IDCW == NO
    
    // 4.3.1b: LPW 23 REL?
    
    // XXX missing the lpw.srel |= dcw.srel) somewhere???
    
    uint addr;
    if (dcwp -> type == ddcw)
      addr = dcwp -> fields . ddcw . daddr;
    else // we know it is not a idcw, so it must be a tdcw
      addr = dcwp -> fields . xfer . addr;
 
    if (lpw . srel != 0)
      {
        // 4.3.1b: LPW 23 REL == 1
        
// I don't have the original PCW handy, so elide...
#if 0
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
                user_fault_flag = iomCsBndyVio;
                goto user_fault;
              }
            // 4.2.1b: ABSOUTIZE ADDRESS
            addr = lpw . lbnd + addr;
          }
#endif
      }
    
    // 4.3.1b: TDCW?
    
    if (dcwp -> type == tdcw)
      {
        sim_debug (DBG_DEBUG, & iom_dev, "%s: TDCW\n", __func__);
        // 4.3.1b: TDCW == YES
 
        // 4.3.1b: SECOND TDCW?
        if (tdcw_count)
          {
            user_fault_flag = iomCsTwoTdcws;
            goto user_fault;
          }
 
        // 4.3.1b: PUT ADDRESS IN LPW
        //         OR TDCW 33, 34, 35 INTO LPW 20, 18, 23
        //         DECREMENT TALLY
sim_printf ("transfer to %o\n", addr); 
        lpw . dcw_ptr = addr;
        // Not for Paged
        //lpw . ae |= dcwp -> fields . xfer . ec;
        lpw . ires |= dcwp -> fields . xfer . res;
        lpw . srel |= dcwp -> fields . xfer . rel;
        lpw . tally --;            
 
        // 4.3.1b: AC CHANGE ERROR?
        //         LPW 18 == 1 AND DCW 33 = 1
 
#if 0 // XXX paged mode?
        if (lpw . ires && dcwp -> fields . xfer . ec)
          {
            // 4.3.1b: AC CHANGE ERROR == YES
            user_fault_flag = iomCsIdcwInResMode;
            goto user_fault;
          }
#endif 
        // 4.3.1b: GOTO A
    
        goto A;
      }
    
    // 4.3.1b: TDCW == NO
    
    // 4.3.1b: CP VIOLATION?
    
    // 43A239854 3.2.3.3 "The byte size, defined by the channel, determines
    // what CP vaues are valid..."
    
    // XXX character position;
    // if (cp decrepancy)
    //   {
    //      user_fault_flag = iomCsCpDiscrepancy;
    //      goto user_fault;
        //   }
    
    user_fault:
    
    if (user_fault_flag)
     {
       dcwp -> fields . ddcw . cp = 07;
       user_fault_flag = iomCsNormal;
     }
 
    // 4.3.1b: SEND DCW TO CHANNEL
 
    // 4.3.1c: D
D:;
    
    // 4.3.1c: SEND FLAGS TO CHANNEL
//XXX         channel_fault (chanNum);
    
    // XXX SEND FLAGS TO CHANNEL
    
    // 4.3.1c: LPW 21?
    
    if (lpw . nc == 0)
      {
    
        // 4.3.1c: LPW 21 == 0 (UPDATE)
    
        // 4.3.1c: UPDATE LPW ADDRESS AND TALLY
    
        lpw . tally --;
        lpw . dcw_ptr ++;
        if (lpw . tally == 0)
          {
            if (ptro)
              * ptro = 1;
          }
      }
    else
      {
        if (ptro)
          * ptro = 1;
      }

          
    // 4.3.1c: IDCW OR FIRST_LIST?
    
    // Always first list

    // 4.3.1c:  WRITE LPW AND LPWX INTO MAILBOXES (scratch and core)
    
    writeLPW (chanNum, chanloc, & lpw, true);

    return 0;
  }

static int send_flags_to_channel (void)
  {
    // XXX
    return 0;
  }

//
// doPayloadChannel
// return 0 ok, != 0 error
//

static int doPayloadChannel (uint iomUnitNum, word24 dcw_ptr)
  {
    pcw_t pcw;
    word36 word0, word1;
    
    (void) fetch_abs_pair (dcw_ptr, & word0, & word1);
sim_printf ("pcw %012llo %012llo\n", word0, word1);
    decode_idcw (iomUnitNum, & pcw, 1, word0, word1);
    uint chanNum = pcw . chan;
    uint dev_code = pcw . dev_code;
    DEVICE * devp = iom [iomUnitNum] . channels [chanNum] [dev_code] . dev;

sim_printf ("setting addressExtension to %o from PCW\n",
  pcw . ext);
    iomChannelData [iomUnitNum] [chanNum] . addressExtension = pcw . ext;

//if (chanNum == 012) iomShowMbx (NULL, iomUnit + iomUnitNum, 0, "");
//if (chanNum == 012) sim_printf ("[%lld]\n", sim_timell ());

    if_sim_debug (DBG_DEBUG, & iom_dev)
      iomShowMbx (NULL, iomUnit + iomUnitNum, 0, "");
//iomAnalyzeMbx (NULL, iomUnit + iomUnitNum, 0, "");

    sim_debug (DBG_NOTIFY, & iom_dev, "IOM dispatch to chan %o\n", chanNum);
    if (devp == NULL)
      {
        // BUG: no device connected; what's the appropriate fault code (s) ?
//--         chanp->status.power_off = 1;
        sim_debug (DBG_WARN, & iom_dev,
                   "%s: No device connected to channel %#o (%d)\n",
                   __func__, chanNum, chanNum);
        iomFault (iomUnitNum, chanNum, __func__, 0, 0, 0);
        return 1;
      }
    

    UNIT * unitp = iom [iomUnitNum] .channels [chanNum] [dev_code] . board;

    // Stash a local copy of the PCW so that it is still valid at activation
    // time
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    * pcwp = pcw;
    //(void) fetch_abs_pair (dcw_ptr, & word0, & word1);
    //decode_idcw (iomUnitNum, pcwp, 1, word0, word1);
    //unitp -> up7 = (void *) pcwp;
    sim_activate (unitp, sys_opts . iom_times . chan_activate);
    return 0;
  }

//
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
 
static int doConnectChan (uint iomUnitNum)
  {
    lpw_t * lpwp = & iomChannelData [iomUnitNum] [IOM_CONNECT_CHAN] . lpw;
    bool ptro = false;
    bool * first_list = & iomChannelData [iomUnitNum] [IOM_CONNECT_CHAN] . firstList;
    * first_list = true;

    uint chanloc = mbx_loc (iomUnitNum, IOM_CONNECT_CHAN);

    do // while (!ptro)
      {

        // 4.3.1a: FIRST_SERVICE == YES?
        if (* first_list)
          {
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: Get connect channel LPW @ %#06o.\n",
                       __func__, chanloc);
            fetch_and_parse_lpw (lpwp, chanloc, true);
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: Connect LPW at %#06o: %s\n", 
                        __func__, chanloc, lpw2text (lpwp, true));
            sim_debug (DBG_TRACE, & iom_dev,
                       "Connect LPW at %#06o: %s\n", 
                        chanloc, lpw2text (lpwp, true));
          }

        // 4.3.1a: CONNECT_CHANNEL = YES
    
        // 4.3.1a: LPW 21,22 == 00?

        if (lpwp -> nc == 0 && lpwp -> trunout == 0)
          {
             iomFault (iomUnitNum, IOM_CONNECT_CHAN, __func__, 1, 
             first_list ? iomFsrFirstList : iomFsrList, iom_ill_tly_cont);
             return 1;
          }
   
        // 4.3.1a: LPW 21,22 == 01 AND TALLY == 0?

        if (lpwp -> nc == 0 && lpwp -> trunout == 1 && lpwp -> tally == 0)
          {
             iomFault (iomUnitNum, IOM_CONNECT_CHAN, __func__, 1, 
                       first_list ? iomFsrFirstList : iomFsrList,
                       iom_lpw_tro_conn);
             return 1;
          }
   
        // 4.3.1a: LPW 21,22 == 01 AND TALLY > 1 AND OVERFLOW?

        if (lpwp -> nc == 0 && lpwp -> trunout == 1 && lpwp -> tally > 1)
          {
             if (lpwp -> dcw_ptr  + lpwp -> tally >= 01000000)
               iomFault (iomUnitNum, IOM_CONNECT_CHAN, __func__, 1, 
                        first_list ? iomFsrFirstList : iomFsrList,
                        iom_256K_of);
             return 1;
          }
   
        // 4.3.1a: LPW 21,22 == 01 AND TALLY == 1?

        if (lpwp -> nc == 0 && lpwp -> trunout == 1 && lpwp -> tally == 1)
          {
            ptro = true;
            sim_debug (DBG_DEBUG, & iom_dev, 
                       "%s: forcing ptro (a)\n", __func__);
          }

        // 4.3.1a: LPW 21,22 == 1X?

        if (lpwp -> nc == 1)
          {
            ptro = true;
            sim_debug (DBG_DEBUG, & iom_dev, 
                       "%s: forcing ptro (b)\n", __func__);
          }


        // 4.3.1a: PULL PCW FROM CORE

        sim_debug (DBG_DEBUG, & iom_dev, "%s: PCW @ 0%06o\n", 
                   __func__, lpwp -> dcw_ptr);
        
        pcw_t pcw;
        word36 word0, word1;
    
        (void) fetch_abs_pair (lpwp -> dcw_ptr, & word0, & word1);
        decode_idcw (iomUnitNum, & pcw, 1, word0, word1);
    
        sim_debug (DBG_INFO, & iom_dev, "%s: PCW is: %s\n", 
                   __func__, pcw2text (& pcw));
        sim_debug (DBG_TRACE, & iom_dev, "PCW is: %s\n", 
                   pcw2text (& pcw));
        // BUG/TODO: Should these be user faults, not system faults?

        // 4.3.1b: PCW 18-20 != 111

        if (pcw . cp != 07)
          {
            iomFault (iomUnitNum, IOM_CONNECT_CHAN, __func__, 1, 
                      first_list ? iomFsrFirstList : iomFsrList,
                      iom_not_pcw_conn);
            return 1;
          }
        
        // 4.3.1b: ILLEGAL CHANNEL NUMBER?

        if (pcw . chan >= MAX_CHANNELS)
          {
            iomFault (iomUnitNum, IOM_CONNECT_CHAN, __func__, 1, 
                      first_list ? iomFsrFirstList : iomFsrList,
                      iom_ill_chan);
            return 1;
          }
    
// This is not an issue as of 'bce (boot)' as it as only been seen in
// rewind commands, which don't request service to begin with.
// XXX ticket 22

        if (pcw.mask)
          {
            // BUG: set mask flags for channel?
            sim_debug (DBG_ERR, & iom_dev, 
                       "%s: PCW Mask not implemented\n", __func__);
          }

        // 4.3.1b: SEND PCW TO CHANNEL

        //dcwp -> type = idcw;
        //dcwp -> fields . instr = pcw;

        int ret = doPayloadChannel (iomUnitNum, lpwp -> dcw_ptr);
        if (ret)
          {
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: doPayloadChannel returned %d\n", __func__, ret);
          }

        // 4.3.1c: D

        ret = send_flags_to_channel ();
        if (ret)
          {
            sim_debug (DBG_DEBUG, & iom_dev,
                       "%s: send_flags_to_channel returned %d\n", __func__, ret);
          }

        // 4.3.1c: LPW 21

        if (lpwp -> nc == 0)
          {
            // 4.3.1c: LPW 21 == 0

            // 4.3.1c: UPDATE LPW ADDRESS & TALLY

            (lpwp -> tally) --;
            lpwp -> dcw_ptr += 2; // pcw is two words

            // 4.3.1c: IDCW OR FIRST LIST

            if (pcw . cp == 7 || * first_list)
              {
                // 4.3.1c: IDCW OR FIRST LIST == YES
                // 4.3.1c: WRITE LPW & LPW EXT. INTO MAILBOXES (scratch and core)
                writeLPW (IOM_CONNECT_CHAN, chanloc, lpwp, true);
              }
            else
              {
                // 4.3.1c: IDCW OR FIRST LIST == NO
                // 4.3.1c: WRITE LPW INTO MAILBOX (scratch and code)

                writeLPW (IOM_CONNECT_CHAN, chanloc, lpwp, false);
              }
          }
        else
          {
            // 4.3.1c: LPW 21 == 1 (NO UPDATE)

            // 4.3.1c: IDCW OR FIRST LIST

            if (pcw . cp == 7 || * first_list)
              {
                // 4.3.1c: IDCW OR FIRST LIST == YES
                // 4.3.1c: WRITE LPW & LPWx INTO MAILBOXES (scratch and core)

                writeLPW (IOM_CONNECT_CHAN, chanloc, lpwp, true);
              }
            else
              {
                // 4.3.1c: IDCW OR FIRST LIST == NO
                // 4.3.1c: TCDW?

                if (pcw . cp != 7 && pcw . mask == 0 && pcw . control == 2)
                  {
                    // 4.3.1c: TCDW == YES

                    // 4.3.1c: WRITE LPW & LPWE INTO MAILBOX (scratch and core)

                    writeLPW (IOM_CONNECT_CHAN, chanloc, lpwp, true);
                  }
              }
          }
        * first_list = false;
 
      }
    while (! ptro);

    return 0;
  }

void iom_interrupt (uint iomUnitNum)
  {
    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c starting.\n",
               __func__, 'A' + iomUnitNum);
    sim_debug (DBG_TRACE, & iom_dev, "\nIOM starting.\n");

    if_sim_debug (DBG_TRACE, & iom_dev)
      {
        sim_printf ("[%lld]\n", sim_timell ());
        iomShowMbx (NULL, iomUnit + iomUnitNum, 0, "");
      }
    int ret = doConnectChan (iomUnitNum);
    if (ret)
      {
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: doConnectChan returned %d\n", __func__, ret);
      }

    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c finished.\n",
               __func__, 'A' + iomUnitNum);
  }

////////////////////////////////////////////////////////////////////////////////
//
// Cabling interface
//
////////////////////////////////////////////////////////////////////////////////

// cable_to_iom
//
// a peripheral is trying to attach a cable
//  to my port [iomUnitNum, chanNum, dev_code]
//  from their simh dev [dev_type, devUnitNum]
//
// Verify that the port is unused; attach this end of the cable

t_stat cable_to_iom (uint iomUnitNum, int chanNum, int dev_code, 
                     enum dev_type dev_type, chan_type ctype, 
                     uint devUnitNum, DEVICE * devp, UNIT * unitp, 
                     iomCmd * iomCmd)
  {
    if (iomUnitNum >= iom_dev . numunits)
      {
        sim_printf ("cable_to_iom: iomUnitNum out of range <%u>\n", iomUnitNum);
        return SCPE_ARG;
      }

    if (chanNum < 0 || chanNum >= MAX_CHANNELS)
      {
        sim_printf ("cable_to_iom: chanNum out of range <%d>\n", chanNum);
        return SCPE_ARG;
      }

    if (dev_code < 0 || dev_code >= N_DEV_CODES)
      {
        sim_printf ("cable_to_iom: dev_code out of range <%d>\n", dev_code);
        return SCPE_ARG;
      }

    if (iom [iomUnitNum] . channels [chanNum] [dev_code] . type != DEVT_NONE)
      {
        sim_printf ("cable_to_iom: socket in use\n");
        return SCPE_ARG;
      }

    iom [iomUnitNum] . channels [chanNum] [dev_code] . type = dev_type;
    iom [iomUnitNum] . channels [chanNum] [dev_code] . ctype = ctype;
    iom [iomUnitNum] . channels [chanNum] [dev_code] . devUnitNum = devUnitNum;

    iom [iomUnitNum] . channels [chanNum] [dev_code] . dev = devp;
    iom [iomUnitNum] . channels [chanNum] [dev_code] . board  = unitp;
    iom [iomUnitNum] . channels [chanNum] [dev_code] . iomCmd  = iomCmd;

    return SCPE_OK;
  }

t_stat cable_iom (uint iomUnitNum, int iomPortNum, int scuUnitNum, int scuPortNum)
  {
    if (iomUnitNum >= iom_dev . numunits)
      {
        sim_printf ("cable_iom: iomUnitNum out of range <%d>\n", iomUnitNum);
        return SCPE_ARG;
      }

    if (iomPortNum < 0 || iomPortNum >= N_IOM_PORTS)
      {
        sim_printf ("cable_iom: iomPortNum out of range <%d>\n", iomUnitNum);
        return SCPE_ARG;
      }

    if (cablesFromScus [iomUnitNum] [iomPortNum] . inuse)
      {
        sim_printf ("cable_iom: port in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_scu (scuUnitNum, scuPortNum, iomUnitNum, iomPortNum);
    if (rc)
      return rc;

    cablesFromScus [iomUnitNum] [iomPortNum] . inuse = true;
    cablesFromScus [iomUnitNum] [iomPortNum] . scuUnitNum = scuUnitNum;
    cablesFromScus [iomUnitNum] [iomPortNum] . scuPortNum = scuPortNum;

    return SCPE_OK;
  }

////////////////////////////////////////////////////////////////////////////////
//
// simh interface
//
////////////////////////////////////////////////////////////////////////////////

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

    for (uint iomUnitNum = 0; iomUnitNum < iom_dev . numunits; iomUnitNum ++)
      {
        for (uint chanNum = 0; chanNum < MAX_CHANNELS; ++ chanNum)
          {
            for (uint dev_code = 0; dev_code < N_DEV_CODES; dev_code ++)
              {
                DEVICE * devp = iom [iomUnitNum] . channels [chanNum] [dev_code] . dev;
                if (devp)
                  {
                    if (devp -> units == NULL)
                      {
                        sim_debug (DBG_ERR, & iom_dev, 
                          "%s: Device on IOM %c channel %d dev_code %d does not have any units.\n",
                          __func__,  'A' + iomUnitNum, chanNum, dev_code);
                      }
                  }
              }
          }
      }
    

    setup_iom_scpage_map ();

    return SCPE_OK;
  }

static t_stat bootSvc (UNIT * unitp)
  {
    uint iomUnitNum = unitp - bootChannelUnit;
    // the docs say press sysinit, then boot; simh doesn't have an
    // explicit "sysinit", so we ill treat  "reset iom" as sysinit.
    // The docs don't say what the behavior is is you dont press sysinit
    // first so we wont worry about it.

    sim_debug (DBG_DEBUG, & iom_dev, "%s: starting on IOM %c\n",
      __func__, 'A' + iomUnitNum);

    // initialize memory with boot program
    init_memory_iom (iomUnitNum);

    // simulate $CON
    iom_interrupt (iomUnitNum);

    sim_debug (DBG_DEBUG, &iom_dev, "%s finished\n", __func__);

    // returning OK from the simh BOOT command causes simh to start the CPU
    return SCPE_OK;
  }

static t_stat iomBoot (int unitNum, UNUSED DEVICE * dptr)
  {
    if (unitNum < 0 || unitNum >= (int) iom_dev . numunits)
      {
        sim_debug (DBG_ERR, & iom_dev, "iomBoot: Invalid unit number %d\n", unitNum);
        sim_printf ("error: invalid unit number %d\n", unitNum);
        return SCPE_ARG;
      }
    uint iomUnitNum = unitNum;
#if 0
    // initialize memory with boot program
    init_memory_iom ((uint)iomUnitNum);

    // simulate $CON
    iom_interrupt (iomUnitNum);

#else
    sim_activate (& bootChannelUnit [iomUnitNum], sys_opts . iom_times . boot_time );
    // returning OK from the simh BOOT command causes simh to start the CPU
#endif
    return SCPE_OK;
  }

static void iom_show_channel_mbx (uint iomUnitNum, uint chanNum)
  {
    sim_printf ("    Channel %o:%o mbx\n", iomUnitNum, chanNum);
    uint chanloc = mbx_loc (iomUnitNum, chanNum);
    sim_printf ("    chanloc %06o\n", chanloc);
    lpw_t lpw;
    fetch_and_parse_lpw (& lpw, chanloc, chanNum == IOM_CONNECT_CHAN);
    lpw . hrel = lpw . srel;
    sim_printf ("    LPW at %06o: %s\n", chanloc, lpw2text (& lpw, chanNum == IOM_CONNECT_CHAN));
    
    uint32 addr = lpw . dcw_ptr;
    if (lpw . tally == 0)
      lpw . tally = 5;

    // This isn't quite right, but sufficient for debugging
    uint control = 2;
#if 0
    for (int i = 0; i < (int) lpw.tally && control == 2; ++ i)
#else
    for (int i = 0; i < (int) lpw.tally; ++ i)
#endif
      {
        if (i > 4096)
          break;
        dcw_t dcw;
        fetchAndParseDCW (iomUnitNum, chanNum, & dcw, addr, 1);
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
#if 1
        if (control != 2)
          {
            if (i == (int) lpw . tally)
              sim_printf ("    -- end of list --\n");
            else
              sim_printf ("    -- end of list (because dcw control != 2) --\n");
          }
#endif
      }
    sim_printf ("\n");
  }

#if 0
static int decipher (uint chanNum, uint cmd)
  {
    if (chanNum == 012 && cmd == 0)
      sim_printf ("IDCW TAPE Request Status\n");
    else if (chanNum == 012 && cmd == 051)
      sim_printf ("IDCW TAPE Reset Device Status\n");
    else if (chanNum == 012 && cmd == 040)
      sim_printf ("IDCW TAPE Reset Status\n");
    else if (chanNum == 012 && cmd == 057)
      sim_printf ("IDCW TAPE Survey Devices\n");
    else if (chanNum == 012 && cmd == 070)
      sim_printf ("IDCW TAPE Rewind\n");
    else if (chanNum == 012 && cmd == 005)
      sim_printf ("IDCW TAPE Read\n");

    else if (chanNum == 013 && cmd == 025)
      sim_printf ("IDCW DISK Read\n");
    else if (chanNum == 013 && cmd == 030)
      sim_printf ("IDCW DISK Seek512\n");
    else if (chanNum == 013 && cmd == 032)
      sim_printf ("IDCW DISK Write\n");
    else if (chanNum == 013 && cmd == 040)
      sim_printf ("IDCW DISK Reset Status\n");

    else if (chanNum == 036 && cmd == 023)
      sim_printf ("IDCW CONS Read\n");
    else if (chanNum == 036 && cmd == 033)
      sim_printf ("IDCW CONS Write\n");
    else if (chanNum == 036 && cmd == 051)
      sim_printf ("IDCW CONS Write Alert\n");
    else if (chanNum == 036 && cmd == 057)
      sim_printf ("IDCW CONS Read ID\n");
    else
      return 0;
    return 1;
  }
#endif

#if 0
static t_stat iomAnalyzeMbx (FILE * UNUSED st,
                            UNIT * uptr, int UNUSED val,
                            void * UNUSED desc)
  {
    sim_printf (">IOM\n");
    uint iomUnitNum = IOM_UNIT_NUM (uptr);

    uint chanNum = IOM_CONNECT_CHAN;
    uint chanloc = mbx_loc (iomUnitNum, chanNum);
    //sim_printf ("\nConnect channel is channel %d at %#06o\n", chanNum, chanloc);

    lpw_t lpw;
    fetch_and_parse_lpw (& lpw, chanloc, chanNum == IOM_CONNECT_CHAN);

    // These combinations indicate a signal a single CMD
    if (! ((lpw . nc == 0 && lpw . tally == 1) ||
           (lpw . nc == 1)))
      {
        sim_printf (" XXX connect LPW ptr %08o nc %o tal %o tally %d\n",
                    lpw . dcw_ptr, lpw . nc, lpw . trunout, lpw . tally);
      }

    lpw . hrel = lpw . srel;

    //sim_printf ("LPW at %#06o: %s\n", chanloc, lpw2text (& lpw, true));
    
    uint32 addr = lpw . dcw_ptr;

    pcw_t pcw;
    word36 word0, word1;
    (void) fetch_abs_pair (addr, & word0, & word1);
    decode_idcw (iomUnitNum, & pcw, 1, word0, word1);
    //sim_printf ("PCW at %#06o: %s\n", addr, pcw2text (& pcw));
    uint cmd = pcw . dev_cmd;
    chanNum = pcw . chan;

    if (chanNum != 036 && cmd == 051) // console search
      {
        sim_printf ("IDCW console search\n");
        goto quit;
      }

    if (! decipher (chanNum, cmd))
      sim_printf ("PCW chan %02o cmd %02o dev %02o\n",
                  pcw . chan, pcw . dev_cmd, pcw . dev_code);

    chanloc = mbx_loc (iomUnitNum, chanNum);

    fetch_and_parse_lpw (& lpw, chanloc, chanNum == IOM_CONNECT_CHAN);
    lpw . hrel = lpw . srel;
    //sim_printf ("    LPW at %06o: %s\n", chanloc, lpw2text (& lpw, chanNum == IOM_CONNECT_CHAN));
    sim_printf (" payload LPW ptr %08o nc %o tal %o tally %d\n",
                lpw . dcw_ptr, lpw . nc, lpw . trunout, lpw . tally);

#if 0
    int tally = lpw . tally;
    if (lpw . nc)
      {
        if (tally == 0)
          tally = 1;
      }
    else
      {
        if (lpw . trunout == 0 && tally == 0)
          {
            tally = 1;
          }
      }

#endif

    addr = lpw . dcw_ptr;
    //for (int i = 0; i < tally; i ++)
    for (int i = 0; i < 16; i ++)
      {
        // This isn't quite right, but sufficient for debugging
        bool disc = false;
        dcw_t dcw;
        fetchAndParseDCW (iomUnitNum, chanNum, & dcw, addr, 1);
        if (dcw . type == idcw)
          {
            uint cmd = dcw . fields . instr . dev_cmd;
            //sim_printf ("chanNum %o cmd %o\n", chanNum, cmd);
            if (! decipher (chanNum, cmd))
              sim_printf ("    IDCW %d at %06o : %s\n", i, addr, dcw2text (& dcw));
              sim_printf ("            ....IDCW %d at %06o : %s\n", i, addr, dcw2text (& dcw));
            sim_printf ("control %o\n", dcw . fields . instr . control);
            if (dcw . fields . instr . control == 0 &&
                dcw . fields . instr . mask == 1)
              disc = true;
            //if (dcw . fields . instr . control == 0)
              //disc = true;
          }
        else if (dcw . type == tdcw)
          {
            sim_printf ("    TDCW %d at %06o\n", i, addr);
            addr = dcw . fields . xfer . addr;
            sim_printf ("transfer to %o\n", addr);
          }
        else if (dcw . type == ddcw)
          {
            sim_printf ("    DDCW %d at %06o: %s\n", i, addr, dcw2text (& dcw));
            //if (dcw . fields . ddcw . type == 0)
              //disc = true;
            //control = dcw . fields . instr . control;
          }
        addr ++;
#if 0
        if (control != 2)
          {
            if (i == (int) lpw . tally)
              sim_printf ("    -- end of list --\n");
            else
              sim_printf ("    -- end of list (because dcw control != 2) --\n");
          }
#endif
        if (disc)
          break;
      }

quit:;
    sim_printf ("\n");
    return SCPE_OK;
  
  }
#endif

static t_stat iomShowMbx (UNUSED FILE * st, 
                            UNIT * uptr, UNUSED int val, 
                            UNUSED void * desc)
  {
    uint iomUnitNum = IOM_UNIT_NUM (uptr);

    uint chanNum = IOM_CONNECT_CHAN;
    uint chanloc = mbx_loc (iomUnitNum, chanNum);
    sim_printf ("\nConnect channel is channel %d at %#06o\n", chanNum, chanloc);

    lpw_t lpw;
    fetch_and_parse_lpw (& lpw, chanloc, chanNum == IOM_CONNECT_CHAN);
    lpw . hrel = lpw . srel;
    sim_printf ("LPW at %#06o: %s\n", chanloc, lpw2text (& lpw, true));
    
    uint32 addr = lpw . dcw_ptr;

    pcw_t pcw;
    word36 word0, word1;
    (void) fetch_abs_pair (addr, & word0, & word1);
    decode_idcw (iomUnitNum, & pcw, 1, word0, word1);
    sim_printf ("PCW at %#06o: %s\n", addr, pcw2text (& pcw));


    chanNum = pcw . chan;
    sim_printf ("Channel %#o (%d):\n", chanNum, chanNum);

    iom_show_channel_mbx (iomUnitNum, chanNum);
    addr += 2;  // skip PCW
    
    if (lpw . tally == 0)
      lpw . tally = 3;

    // This isn't quite right, but sufficient for debugging
    uint control = 2;
    for (int i = 0; i < (int) lpw.tally && control == 2; ++ i)
      {
        if (i > 4096)
          break;
        dcw_t dcw;
        fetchAndParseDCW (iomUnitNum, chanNum, & dcw, addr, 1);
        if (dcw . type == idcw)
          {
            //dcw.fields.instr.chan = chanNum; // Real HW would not populate
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
        addr ++;
        if (control != 2)
          {
            if (i == (int) lpw . tally)
              sim_printf ("-- end of list --\n");
            else
              sim_printf ("-- end of list (because dcw control != 2) --\n");
          }
      }
    sim_printf ("\n");
    return SCPE_OK;
  }

static t_stat iomShowUnits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED void * desc)
  {
    sim_printf ("Number of IOM units in system is %d\n", iom_dev . numunits);
    return SCPE_OK;
  }

static t_stat iomSetUnits (UNUSED UNIT * uptr, UNUSED int value, char * cptr, UNUSED void * desc)
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
                             UNUSED void * desc)
  {
    uint iomUnitNum = IOM_UNIT_NUM (uptr);
    if (iomUnitNum >= iom_dev . numunits)
      {
        sim_debug (DBG_ERR, & iom_dev, 
                   "iomShowConfig: Invalid unit number %d\n", iomUnitNum);
        sim_printf ("error: invalid unit number %u\n", iomUnitNum);
        return SCPE_ARG;
      }

    sim_printf ("IOM unit number %u\n", iomUnitNum);
    struct unitData * p = unitData + iomUnitNum;

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

static t_stat iomSetConfig (UNIT * uptr, UNUSED int value, char * cptr, UNUSED void * desc)
  {
    uint iomUnitNUm = IOM_UNIT_NUM (uptr);
    if (iomUnitNUm >= iom_dev . numunits)
      {
        sim_debug (DBG_ERR, & iom_dev, "iomSetConfig: Invalid unit number %d\n", iomUnitNUm);
        sim_printf ("error: iomSetConfig: invalid unit number %d\n", iomUnitNUm);
        return SCPE_ARG;
      }

    struct unitData * p = unitData + iomUnitNUm;

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
              p -> configSwOS = v;
              break;

            case 1: // BOOT
              p -> configSwBootloadCardTape = v;
              break;

            case 2: // IOM_BASE
              p -> configSwIomBaseAddress = v;
              break;

            case 3: // MULTIPLEX_BASE
              p -> configSwMultiplexBaseAddress = v;
              break;

            case 4: // TAPECHAN
              p -> configSwBootloadMagtapeChan = v;
              break;

            case 5: // CARDCHAN
              p -> configSwBootloadCardrdrChan = v;
              break;

            case 6: // SCUPORT
              p -> configSwBootloadPort = v;
              break;

            case 7: // PORT
              port_num = v;
              break;

#if 0
                // all of the remaining assume a valid value in port_num
                if (/* port_num < 0 || */ port_num > 7)
                  {
                    sim_debug (DBG_ERR, & iom_dev, "iomSetConfig: cached PORT value out of range: %d\n", port_num);
                    sim_printf ("error: iomSetConfig: cached PORT value out of range: %d\n", port_num);
                    break;
                  } 
#endif
            case 8: // ADDR
              p -> configSwPortAddress [port_num] = v;
              break;

            case 9: // INTERLACE
              p -> configSwPortInterface [port_num] = v;
              break;

            case 10: // ENABLE
              p -> configSwPortEnable [port_num] = v;
              break;

            case 11: // INITENABLE
              p -> configSwPortSysinitEnable [port_num] = v;
              break;

            case 12: // HALFSIZE
              p -> configSwPortHalfsize [port_num] = v;
              break;

            case 13: // STORE_SIZE
              p -> configSwPortStoresize [port_num] = v;
              break;

            default:
              sim_debug (DBG_ERR, & iom_dev, "iomSetConfig: Invalid cfgparse rc <%d>\n", rc);
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

