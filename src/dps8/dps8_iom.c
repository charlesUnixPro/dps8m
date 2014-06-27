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

// Hardware limit
#define N_IOM_UNITS_MAX 4
// Default
#define N_IOM_UNITS 1

#define N_IOM_PORTS 8

// The number of devices that a dev_code can address (6 bit number)

#define N_DEV_CODES 64

#define IOM_CONNECT_CHAN 2U

#define MAX_CHANNELS 64

////////////////////////////////////////////////////////////////////////////////
//
// simh interface
//
////////////////////////////////////////////////////////////////////////////////

#define IOM_UNIT_NUM(uptr) ((uptr) - iom_unit)

// static t_stat iom_svc (UNIT * unitp);
static UNIT iom_unit [N_IOM_UNITS_MAX] =
  {
    { UDATA(NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA(NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA(NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL },
    { UDATA(NULL /*& iom_svc*/, 0, 0), 0, 0, 0, 0, 0, NULL, NULL }
  };

static t_stat iom_show_mbx (FILE * st, UNIT * uptr, int val, void * desc);
static t_stat iom_show_config (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat iom_set_config (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat iom_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat iom_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);

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

static t_stat iom_reset(DEVICE *dptr);
static t_stat iom_boot (int32 unit_num, DEVICE * dptr);

DEVICE iom_dev =
  {
    "IOM",       /* name */
    iom_unit,    /* units */
    NULL,        /* registers */
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


////////////////////////////////////////////////////////////////////////////////
//
// Data structures
//
////////////////////////////////////////////////////////////////////////////////

enum config_sw_os_
  { 
    CONFIG_SW_STD_GCOS, 
    CONFIG_SW_EXT_GCOS, 
    CONFIG_SW_MULTICS 
  };

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


typedef struct
  {
    uint iom_num;
    int ports [N_IOM_PORTS]; // CPU/IOM connectivity; designated a..h; 
                             // negative to disable
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
      } channels [MAX_CHANNELS] [N_DEV_CODES];
  } iom_t;

static iom_t iom [N_IOM_UNITS_MAX];

static struct
  {
    int scu_unit_num;
    int scu_port_num;
  } cables_from_scus [N_IOM_UNITS_MAX] [N_IOM_PORTS];

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

////////////////////////////////////////////////////////////////////////////////
//
// emulator interface
//
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
//
// IOM data structure
//
////////////////////////////////////////////////////////////////////////////////

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

typedef enum iom_sys_faults
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
  } iom_sys_faults;


uint mbx_loc (int iom_unit_num, uint chan_num)
  {
    word12 base = unit_data [iom_unit_num] . config_sw_iom_base_address;
    word24 base_addr = ((word24) base) << 6; // 01400
    word24 mbx = base_addr + 4 * chan_num;
    sim_debug (DBG_INFO, & iom_dev, "%s: IOM %c, chan %d is %012o\n",
      __func__, 'A' + iom_unit_num, chan_num, mbx);
    return mbx;
  }

//
// fetch_and_parse_lpw()
//
// Parse the words at "addr" into a lpw_t.
//

void fetch_and_parse_lpw (lpw_t * p, uint addr, bool is_conn)
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
    p -> tally = getbits36 (word0, 24, 12); // initial value treated as unsigned
    
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

//
// decode_idcw()
//
// Decode an idcw word or pcw word pair
//

static void decode_idcw (int iom_unit_num, pcw_t *p, bool is_pcw, 
                         word36 word0, word36 word1)
  {
    p->dev_cmd = getbits36(word0, 0, 6);
    p->dev_code = getbits36(word0, 6, 6);
    p->ext = getbits36(word0, 12, 6);
    p->cp = getbits36(word0, 18, 3);
    p->mask = getbits36(word0, 21, 1);
    p->control = getbits36(word0, 22, 2);
    p->chan_cmd = getbits36(word0, 24, 6);
    p->chan_data = getbits36(word0, 30, 6);
    if (is_pcw)
      {
        p->chan = getbits36(word1, 3, 6);
        uint x = getbits36(word1, 9, 27);
        if (x != 0 &&
            unit_data [iom_unit_num] . config_sw_os != CONFIG_SW_MULTICS)
          {
            sim_debug (DBG_ERR, &iom_dev, 
                       "decode_idcw: Page Table Pointer for model IOM-B detected\n");
          }
      }
    else
      {
        p->chan = (uint)-1;
      }
  }

//
// pcw2text()
//
// Display pcw_t
//

static char * pcw2text (const pcw_t * p)
  {
    // WARNING: returns single static buffer
    static char buf[200];
    sprintf(buf, "[dev-cmd=0%o, dev-code=0%o, ext=0%o, mask=%u, ctrl=0%o, chan-cmd=0%o, chan-data=0%o, chan=0%o]",
            p->dev_cmd, p->dev_code, p->ext, p->mask, p->control, p->chan_cmd, p->chan_data, p->chan);
    return buf;
  }

//
// lpw2text()
//
// Display an LPW
//

static char * lpw2text (const lpw_t * p, int conn)
  {
    // WARNING: returns single static buffer
    static char buf [180];
    sprintf(buf,
            "[dcw=0%o ires=%d hrel=%d ae=%d nc=%d trun=%d srel=%d tally=0%o]",
            p->dcw_ptr, p->ires, p->hrel, p->ae, p->nc, p->trunout, p->srel, 
            p->tally);
    if (! conn)
      sprintf(buf+strlen(buf), " [lbnd=0%o size=0%o(%d) idcw=0%o]",
                p->lbnd, p->size, p->size, p->idcw);
    return buf;
  }

//
// fetch_and_parse_dcw()
//
// Parse word at "addr" into a dcw_t.
//

static void fetch_and_parse_dcw (int iom_unit_num, uint chan, dcw_t * p, 
                                 uint32 addr, 
                                 int __attribute__((unused)) read_only)
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

//
// dcw2text()
//
// Display a dcw_t
//
//

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
        sprintf(buf, "D-DCW: type=%u(%s), addr=%06o, cp=0%o, tally=0%o(%d) tally-ctl=%d",
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
        
        for (int chan = 0; chan < MAX_CHANNELS; ++ chan)
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

////////////////////////////////////////////////////////////////////////////////
//
// IOM emulator
//
////////////////////////////////////////////////////////////////////////////////

static void iom_fault (int iom_unit_num, uint chan, const char* who, int is_sys, iom_sys_faults signal)
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

int status_service (int iom_unit_num, uint chan, uint dev_code, word12 stati, 
                    word6 rcount, word12 residue, word3 char_pos, bool is_read)
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
// interrupt the SCU that generated the iom_interrupt.
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

#if 0
/*
 * send_marker_interrupt()
 *
 * Send a "marker" interrupt to the CPU.
 *
 * Channels send marker interrupts to indicate normal completion of
 * a PCW or IDCW if the control field of the PCW/IDCW has a value
 * of three.
 */

static int send_marker_interrupt(int iom_unit_num, int chan)
{
    return send_general_interrupt(iom_unit_num, chan, imw_marker_pic);
}
#endif

/*
 * send_terminate_interrupt()
 *
 * Send a "terminate" interrupt to the CPU.
 *
 * Channels send a terminate interrupt after doing a status service.
 *
 */

int send_terminate_interrupt (int iom_unit_num, uint chan)
  {
    return send_general_interrupt (iom_unit_num, chan, imw_terminate_pic);
  }


//
// iomListService
//   NOT connect channel!

int iomListService (int iom_unit_num, int chan_num, dcw_t * dcwp)
  {

    lpw_t lpw; 
    int user_fault_flag = iom_cs_normal;
    int tdcw_count = 0;

    uint chanloc = mbx_loc (iom_unit_num, chan_num);

    // Eliding scratchpad, so always first service.

    // 4.3.1a: FIRST_SERVICE == YES

    fetch_and_parse_lpw (& lpw, chanloc, false);

    // 4.3.1a: CONNECT_CHANNEL == NO
        
    // 4.3.1a: LPW 21,22 == ?
    
    if (lpw . nc == 0 && lpw . trunout == 0)
      {
        // 4.3.1a: LPW 21,22 == 00
        // 4.3.1a: 256K OVERFLOW?
        if (lpw . ae == 0 && lpw . dcw_ptr  + lpw . tally >= 01000000)
          {
            iom_fault (iom_unit_num, IOM_CONNECT_CHAN, __func__, 1, 
                       iom_256K_of);
            return 1;
          }
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
            iom_fault (iom_unit_num, chan_num, __func__, 0, 1);
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
// because the tally is 3
            //ptro = true;
            //sim_debug (DBG_DEBUG, & iom_dev, "%s: forcing ptro (a)\n", __func__);
          }

      }
    
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
        
    // XXX check 1 bit; read_only
    fetch_and_parse_dcw (iom_unit_num, chan_num, dcwp, dcw_addr, 1);
    
    // 4.3.1b: C
    
    // 4.3.1b: IDCW?
        
    if (dcwp -> type == idcw)
      {
        // 4.3.1b: IDCW == YES
    
        // 4.3.1b: LPW 18 RES?
    
        if (lpw . ires)
          {
            // 4.3.1b: LPW 18 RES == YES
            user_fault_flag = iom_cs_idcw_in_res_mode;
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
                user_fault_flag = iom_cs_bndy_vio;
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
        lpw . ae |= dcwp -> fields . xfer . ec;
        lpw . ires |= dcwp -> fields . xfer . i;
        lpw . srel |= dcwp -> fields . xfer . r;
        lpw . tally --;            
 
        // 4.3.1b: AC CHANGE ERROR?
        //         LPW 18 == 1 AND DCW 33 = 1
 
        if (lpw . ires && dcwp -> fields . xfer . ec)
          {
            // 4.3.1b: AC CHANGE ERROR == YES
            user_fault_flag = iom_cs_idcw_in_res_mode;
            goto user_fault;
          }
    
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
    //      user_fault_flag = iom_cs_cp_discrepancy;
    //      goto user_fault;
        //   }
    
    user_fault:
    
    if (user_fault_flag)
     {
       dcwp -> fields . ddcw . cp = 07;
       user_fault_flag = iom_cs_normal;
     }
 
    // 4.3.1b: SEND DCW TO CHANNEL
 
    // 4.3.1c: D
D:;
    
    // 4.3.1c: SEND FLAGS TO CHANNEL
//XXX         channel_fault (chan_num);
    
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
    
    // Always first list

    // 4.3.1c:  WRITE LPW AND LPWX INTO MAILBOXES (scratch and core)
    
    lpw_write (chan_num, chanloc, & lpw);

    return 0;
  }

static int send_flags_to_channel (void)
  {
    // XXX
    return 0;
  }

// return 0 ok, != 0 error

static int do_payload_channel (int iom_unit_num, pcw_t * pcwp)
  {
    uint chan = pcwp -> chan;
    uint dev_code = pcwp -> dev_code;
    DEVICE * devp = iom [iom_unit_num] . channels [chan] [dev_code] . dev;

    if (devp == NULL)
      {
        // BUG: no device connected; what's the appropriate fault code(s) ?
//--         chanp->status.power_off = 1;
        sim_debug (DBG_WARN, & iom_dev,
                   "%s: No device connected to channel %#o(%d)\n",
                   __func__, chan, chan);
        iom_fault (iom_unit_num, chan, __func__, 0, 0);
        return 1;
      }
    
    iom_cmd * iom_cmd = iom [iom_unit_num] .channels [chan] [dev_code] . 
                        iom_cmd;
    if (! iom_cmd)
      {
        // BUG: no device connected; what's the appropriate fault code(s) ?
        sim_debug (DBG_ERR, & iom_dev,
                   "%s: iom_cmd on channel %#o (%d) dev_code %d is missing.\n",
                   __func__, chan, chan, dev_code);
        iom_fault (iom_unit_num, chan, __func__, 0, 0);
        return 1;
      }

    UNIT * unitp = iom [iom_unit_num] .channels [chan] [dev_code] . board;

    int rc = iom_cmd (unitp, pcwp);

    sim_debug (DBG_DEBUG, & iom_dev,
               "%s: iom_cmd returns rc:%d:\n",
               __func__, rc);

    return rc;
  }

//
// do_connect_chan ()
//
// Process the "connect channel".  This is what the IOM does when it
// receives a $CON signal.
//
// Only called by iom_interrupt()
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

        if (/*pcw . chan < 0 ||*/ pcw . chan >= MAX_CHANNELS)
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

        //dcwp -> type = idcw;
        //dcwp -> fields . instr = pcw;

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

void iom_interrupt (int iom_unit_num)
  {
    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c starting.\n",
      __func__, 'A' + iom_unit_num);
    sim_debug (DBG_TRACE, & iom_dev, "\nIOM starting.\n");

    if_sim_debug (DBG_TRACE, & iom_dev)
      {
        sim_printf ("[%lld]\n", sys_stats . total_cycles);
        iom_show_mbx (NULL, iom_unit + iom_unit_num, 0, "");
      }
    int ret = do_connect_chan (iom_unit_num);
    if (ret)
      {
        sim_debug (DBG_DEBUG, & iom_dev,
                   "%s: do_connect_chan returned %d\n", __func__, ret);
      }

    sim_debug (DBG_DEBUG, & iom_dev, "%s: IOM %c finished.\n",
      __func__, 'A' + iom_unit_num);
  }

////////////////////////////////////////////////////////////////////////////////
//
// Cabling interface
//
////////////////////////////////////////////////////////////////////////////////

// cable_to_iom
//
// a peripheral is trying to attach a cable
//  to my port [iom_unit_num, chan_num, dev_code]
//  from their simh dev [dev_type, dev_unit_num]
//
// Verify that the port is unused; attach this end of the cable

t_stat cable_to_iom (int iom_unit_num, int chan_num, int dev_code, 
                     enum dev_type dev_type, chan_type ctype, 
                     int dev_unit_num, DEVICE * devp, UNIT * unitp, 
                     iom_cmd * iom_cmd)
  {
    if (iom_unit_num < 0 || iom_unit_num >= (int) iom_dev . numunits)
      {
        sim_printf ("cable_to_iom: iom_unit_num out of range <%d>\n", iom_unit_num);
        return SCPE_ARG;
      }

    if (chan_num < 0 || chan_num >= MAX_CHANNELS)
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

    return SCPE_OK;
  }

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

////////////////////////////////////////////////////////////////////////////////
//
// simh interface
//
////////////////////////////////////////////////////////////////////////////////

//
// iom_reset()
//
//  Reset -- Reset to initial state -- clear all device flags and cancel any
//  any outstanding timing operations. Used by SIMH's RESET, RUN, and BOOT
//  commands
//
//  Note that all reset()s run after once-only init().
//
//

static t_stat iom_reset (DEVICE * __attribute__((unused)) dptr)
  {
    sim_debug (DBG_INFO, & iom_dev, "%s: running.\n", __func__);

    for (int unit_num = 0; unit_num < (int) iom_dev . numunits; unit_num ++)
      {
        for (int chan = 0; chan < MAX_CHANNELS; ++ chan)
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

static t_stat boot_svc (UNIT * unitp)
  {
    int unit_num = unitp - boot_channel_unit;
    // the docs say press sysinit, then boot; simh doesn't have an
    // explicit "sysinit", so we ill treat  "reset iom" as sysinit.
    // The docs don't say what the behavior is is you dont press sysinit
    // first so we wont worry about it.

    sim_debug (DBG_DEBUG, & iom_dev, "%s: starting on IOM %c\n",
      __func__, 'A' + unit_num);

    // initialize memory with boot program
    init_memory_iom ((uint)unit_num);

    // simulate $CON
    iom_interrupt (unit_num);

    sim_debug (DBG_DEBUG, &iom_dev, "%s finished\n", __func__);

    // returning OK from the simh BOOT command causes simh to start the CPU
    return SCPE_OK;
  }

static t_stat iom_boot (int32 unit_num, DEVICE * __attribute__((unused)) dptr)
  {
    sim_activate (& boot_channel_unit [unit_num], sys_opts . iom_times . boot_time );
    // returning OK from the simh BOOT command causes simh to start the CPU
    return SCPE_OK;
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

static t_stat iom_show_mbx (FILE * __attribute__((unused)) st, 
                            UNIT * uptr, int __attribute__((unused)) val, 
                            void * __attribute__((unused)) desc)
  {
    int iom_unit_num = IOM_UNIT_NUM (uptr);

    uint chan = IOM_CONNECT_CHAN;
    uint chanloc = mbx_loc (iom_unit_num, chan);
    sim_printf ("\nConnect channel is channel %d at %#06o\n", chan, chanloc);

    lpw_t lpw;
    fetch_and_parse_lpw (& lpw, chanloc, chan == IOM_CONNECT_CHAN);
    lpw . hrel = lpw . srel;
    sim_printf ("LPW at %#06o: %s\n", chanloc, lpw2text (& lpw, true));
    
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
      lpw . tally = 3;

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

