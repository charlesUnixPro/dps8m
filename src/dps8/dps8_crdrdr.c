//
//  dps8_crdrdr.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_crdrdr.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_cable.h"

//-- // XXX We use this where we assume there is only one unit
//-- #define ASSUME0 0
//-- 
 
/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */


#define N_CRDRDR_UNITS 1 // default

static t_stat crdrdr_reset (DEVICE * dptr);
static t_stat crdrdr_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat crdrdr_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat crdrdr_show_device_name (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat crdrdr_set_device_name (UNIT * uptr, int32 value, char * cptr, void * desc);

static t_stat crdrdr_svc (UNIT *);

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE )
UNIT crdrdr_unit [N_CRDRDR_UNITS_MAX] =
  {
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& crdrdr_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL}
  };

#define CRDRDR_UNIT_NUM(uptr) ((uptr) - crdrdr_unit)

static DEBTAB crdrdr_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

#define UNIT_WATCH UNIT_V_UF

static MTAB crdrdr_mod [] =
  {
    { UNIT_WATCH, 1, "WATCH", "WATCH", 0, 0, NULL, NULL },
    { UNIT_WATCH, 0, "NOWATCH", "NOWATCH", 0, 0, NULL, NULL },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      crdrdr_set_nunits, /* validation routine */
      crdrdr_show_nunits, /* display routine */
      "Number of CRDRDR units in the system", /* value descriptor */
      NULL // Help
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_VALR | MTAB_NC, /* mask */
      0,            /* match */
      "DEVICE_NAME",     /* print string */
      "DEVICE_NAME",         /* match string */
      crdrdr_set_device_name, /* validation routine */
      crdrdr_show_device_name, /* display routine */
      "Select the boot drive", /* value descriptor */
      NULL          // help
    },

    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };


// No crdrdrs known to multics had more than 2^24 sectors...
DEVICE crdrdr_dev = {
    "CRDRDR",       /*  name */
    crdrdr_unit,    /* units */
    NULL,         /* registers */
    crdrdr_mod,     /* modifiers */
    N_CRDRDR_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    crdrdr_reset,   /* reset */
    NULL,         /* boot */
    NULL,         /* attach */
    NULL,         /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    crdrdr_dt,      /* debug flag names */
    NULL,         /* memory size change */
    NULL,         /* logical name */
    NULL,         // help
    NULL,         // attach help
    NULL,         // attach context
    NULL          // description
};

#define MAX_DEV_NAME_LEN 64
static struct crdrdr_state
  {
    char device_name [MAX_DEV_NAME_LEN];
  } crdrdr_state [N_CRDRDR_UNITS_MAX];

static int findCrdrdrUnit (int iom_unit_num, int chan_num, int dev_code)
  {
    for (int i = 0; i < N_CRDRDR_UNITS_MAX; i ++)
      {
        if (iom_unit_num == cables_from_ioms_to_crdrdr [i] . iom_unit_num &&
            chan_num     == cables_from_ioms_to_crdrdr [i] . chan_num     &&
            dev_code     == cables_from_ioms_to_crdrdr [i] . dev_code)
          return i;
      }
    return -1;
  }

/*
 * crdrdr_init()
 *
 */

// Once-only initialization

void crdrdr_init (void)
  {
    memset (crdrdr_state, 0, sizeof (crdrdr_state));
  }

static t_stat crdrdr_reset (DEVICE * dptr)
  {
    for (uint i = 0; i < dptr -> numunits; i ++)
      {
        // sim_crdrdr_reset (& crdrdr_unit [i]);
        sim_cancel (& crdrdr_unit [i]);
      }
    return SCPE_OK;
  }

// http://homepage.cs.uiowa.edu/~jones/cards/codes.html
// General Electric
// 
// General Electric used the following collating sequence on their machines,
// including the GE 600 (the machine on which Multics was developed); this is
// largely upward compatable from the IBM 026 commercial character set, and it
// shows strong influence from the IBM 1401 character set while supporting the
// full ASCII character set, with 64 printable characters, as it was understood
// in the 1960's.
// 
// GE   &-0123456789ABCDEFGHIJKLMNOPQR/STUVWXYZ[#@:>?+.](<\^$*);'_,%="!
//      ________________________________________________________________
//     /&-0123456789ABCDEFGHIJKLMNOPQR/STUVWXYZ #@:>V .¤(<§ $*);^±,%='"
// 12 / O           OOOOOOOOO                        OOOOOO
// 11|   O                   OOOOOOOOO                     OOOOOO
//  0|    O                           OOOOOOOOO                  OOOOOO
//  1|     O        O        O        O
//  2|      O        O        O        O       O     O     O     O
//  3|       O        O        O        O       O     O     O     O
//  4|        O        O        O        O       O     O     O     O
//  5|         O        O        O        O       O     O     O     O
//  6|          O        O        O        O       O     O     O     O
//  7|           O        O        O        O       O     O     O     O
//  8|            O        O        O        O OOOOOOOOOOOOOOOOOOOOOOOO
//  9|             O        O        O        O
//   |__________________________________________________________________
// In the above, the 0-8-2 punch shown as _ should be printed as an assignment
// arrow, and the 11-8-2 punch shown as ^ should be printed as an up-arrow.
// This conforms to the evolution of of these ASCII symbols from the time GE
// adopted this character set and the present.


static void asciiToH (char * str, uint * hstr)
  {
    char haystack [] = "&-0123456789ABCDEFGHIJKLMNOPQR/STUVWXYZ[#@:>?+.](<\\^$*);'_,%=\"!";
    uint table [] =
      {
        0b100000000000, // &
        0b010000000000, // -
        0b001000000000, // 0
        0b000100000000, // 1
        0b000010000000, // 2
        0b000001000000, // 3
        0b000000100000, // 4
        0b000000010000, // 5
        0b000000001000, // 6
        0b000000000100, // 7
        0b000000000010, // 8
        0b000000000001, // 9

        0b100100000000, // A
        0b100010000000, // B
        0b100001000000, // C
        0b100000100000, // D
        0b100000010000, // E
        0b100000001000, // F
        0b100000000100, // G
        0b100000000010, // H
        0b100000000001, // I

        0b010100000000, // J
        0b010010000000, // K
        0b010001000000, // L
        0b010000100000, // M
        0b010000010000, // N
        0b010000001000, // O
        0b010000000100, // P
        0b010000000010, // Q
        0b010000000001, // R

        0b001100000000, // /
        0b001010000000, // S
        0b001001000000, // T
        0b001000100000, // U
        0b001000010000, // V
        0b001000001000, // W
        0b001000000100, // X
        0b001000000010, // Y
        0b001000000001, // Z

        0b000010000010, // [
        0b000001000010, // #
        0b000000100010, // @
        0b000000010010, // :
        0b000000001010, // >
        0b000000000110, // ?

        0b100010000010, // +
        0b100001000010, // .
        0b100000100010, // ]
        0b100000010010, // (
        0b100000001010, // <
        0b100000000110, // backslash

        0b010010000010, // ^
        0b010001000010, // $
        0b010000100010, // *
        0b010000010010, // )
        0b010000001010, // ;
        0b010000000110, // '

        0b001010000010, // _
        0b001001000010, // ,
        0b001000100010, // %
        0b001000010010, // =
        0b001000001010, // "
        0b001000000110, // !
      };
    for (char * p = str; * p; p ++)
      {
        uint h = 0b000000000110; // ?
        char * q = index (haystack, toupper (* p));
        if (q)
          h = table [q - haystack];
        * hstr ++ = h;
      }
 }

static int crdrdr_cmd (UNIT * unitp, pcw_t * pcwp, bool * disc)
  {
    int crdrdr_unit_num = CRDRDR_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_crdrdr [crdrdr_unit_num] . iom_unit_num;
    struct crdrdr_state * crdrdr_statep = & crdrdr_state [crdrdr_unit_num];
    * disc = false;

    int chan = pcwp-> chan;
sim_printf ("crdrdr_cmd %o [%lld]\n", pcwp -> dev_cmd, sim_timell ());
    iomChannelData_ * chan_data = & iomChannelData [iom_unit_num] [chan];
    if (chan_data -> ptp)
      sim_printf ("PTP in crdrdr\n");
    chan_data -> stati = 0;

    switch (pcwp -> dev_cmd)
      {
        case 000: // CMD 00 Request status
          {
            chan_data -> stati = 04000;
            sim_debug (DBG_NOTIFY, & crdrdr_dev, "Request status %d\n", crdrdr_unit_num);
            chan_data -> initiate = true;
          }
          break;

        case 001: // CMD 01 Read binary
          {
            sim_debug (DBG_NOTIFY, & crdrdr_dev, "Read binary %d\n", crdrdr_unit_num);
            // Get the DDCW
            dcw_t dcw;
            int rc = iomListService (iom_unit_num, chan, & dcw, NULL);

            if (rc)
              {
                sim_printf ("list service failed\n");
                chan_data -> stati = 04504; // BUG: Comand reject, parity,IDCW/LC#
                chan_data -> chanStatus = chanStatIncomplete;
                break;
              }
//sim_printf ("read  got type %d\n", dcw . type);
            if (dcw . type != ddcw)
              {
                sim_printf ("not ddcw? %d\n", dcw . type);
                chan_data -> stati = 04504; // BUG: Comand reject, parity,IDCW/LC#
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            uint type = dcw.fields.ddcw.type;
            uint tally = dcw.fields.ddcw.tally;
            uint daddr = dcw.fields.ddcw.daddr;
            if (pcwp -> mask)
              daddr |= ((pcwp -> ext) & MASK6) << 18;
            // uint cp = dcw.fields.ddcw.cp;

            if (type == 0) // IOTD
              * disc = true;
            else if (type == 1) // IOTP
              * disc = false;
            else
              {
sim_printf ("uncomfortable with this\n");
                chan_data -> stati = 04504; // BUG: Comand reject, parity,IDCW/LC#
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }
#if 0
            if (type == 3 && tally != 1)
              {
                sim_debug (DBG_ERR, &iom_dev, "%s: Type is 3, but tally is %d\n",
                           __func__, tally);
              }
#endif
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

sim_printf ("tally %d\n", tally);


            uint wordsProcessed = 0;
            for (uint i = 0; i < tally; i ++)
              {
                word36 w;
#if 0
                extractWord36FromBuffer (buffer, p72ByteCnt, & wordsProcessed,
                                         & w);
#else
w=i;
#endif
                store_abs_word (daddr + i, w, "Card reader read");
                chan_data -> isOdd = (daddr + i) % 2;
              }
//for (uint i = 0; i < tally; i ++) sim_printf ("%8o %012llo\n", daddr + i, M [daddr + i]);
            chan_data -> stati = 04000;
            chan_data -> tallyResidue = tally;

// hopper empty
            chan_data -> stati = 04201;
            status_service (iom_unit_num, pcwp -> chan, false);
          }
          break;












// dcl  1 io_status_word based (io_status_word_ptr) aligned,       /* I/O status information */
//   (
//   2 t bit (1),              /* set to "1"b by IOM */
//   2 power bit (1),          /* non-zero if peripheral absent or power off */
//   2 major bit (4),          /* major status */
//   2 sub bit (6),            /* substatus */
//   2 eo bit (1),             /* even/odd bit */
//   2 marker bit (1),         /* non-zero if marker status */
//   2 soft bit (2),           /* software status */
//   2 initiate bit (1),       /* initiate bit */
//   2 abort bit (1),          /* software abort bit */
//   2 channel_stat bit (3),   /* IOM channel status */
//   2 central_stat bit (3),   /* IOM central status */
//   2 mbz bit (6),
//   2 rcount bit (6)
//   ) unaligned;              /* record count residue */

        case 040: // CMD 40 Reset status
          {
            chan_data -> stati = 04000;
            //crdrdr_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & crdrdr_dev, "Reset status %d\n", crdrdr_unit_num);
            chan_data -> initiate = true;
          }
          break;



        default:
          {
sim_printf ("crdrdr daze %o\n", pcwp -> dev_cmd);
            chan_data -> stati = 04501; // cmd reject, invalid opcode
            chan_data -> chanStatus = chanStatIncorrectDCW;
          }
          break;
      
      }

    return 0;
  }

int crdrdr_iom_cmd (UNIT * unitp, pcw_t * pcwp)
  {
    int crdrdr_unit_num = CRDRDR_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_crdrdr [crdrdr_unit_num] . iom_unit_num;

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

    // uint chanloc = mbx_loc (iom_unit_num, pcwp -> chan);
    //lpw_t lpw;
    //fetch_and_parse_lpw (& lpw, chanloc, false);

// Ignore a CMD 051 in the PCW
    if (pcwp -> dev_cmd == 051)
      return 1;
    bool disc;
    crdrdr_cmd (unitp, pcwp, & disc);

    // ctrl of the pcw is observed to be 0 even when there are idcws in the
    // list so ignore that and force it to 2.
    //uint ctrl = pcwp -> control;
    uint ctrl = 2;
    if (disc)
      ctrl = 0;
//sim_printf ("starting list; disc %d, ctrl %d\n", disc, ctrl);

    // It looks like the crdrdr controller ignores IOTD and olny obeys ctrl...
    //while ((! disc) && ctrl == 2)
    int ptro = 0;
#ifdef PTRO
    while (ctrl == 2 && ! ptro)
#else
    while (ctrl == 2)
#endif
      {
sim_printf ("perusing channel mbx lpw....\n");
        dcw_t dcw;
        int rc = iomListService (iom_unit_num, pcwp -> chan, & dcw, & ptro);
        if (rc)
          {
sim_printf ("list service denies!\n");
            break;
          }
sim_printf ("persuing got type %d\n", dcw . type);
        if (dcw . type != idcw)
          {
// 04501 : COMMAND REJECTED, invalid command
            iomChannelData_ * chan_data = & iomChannelData [iom_unit_num] [pcwp -> chan];
            chan_data -> stati = 04501; 
            chan_data -> dev_code = dcw . fields . instr. dev_code;
            chan_data -> chanStatus = chanStatInvalidInstrPCW;
            //status_service (iom_unit_num, pcwp -> chan, false);
            break;
          }

// The dcw does not necessarily have the same dev_code as the pcw....

        crdrdr_unit_num = findCrdrdrUnit (iom_unit_num, pcwp -> chan, dcw . fields . instr. dev_code);
        if (crdrdr_unit_num < 0)
          {
// 04502 : COMMAND REJECTED, invalid device code
            iomChannelData_ * chan_data = & iomChannelData [iom_unit_num] [pcwp -> chan];
            chan_data -> stati = 04502; 
            chan_data -> dev_code = dcw . fields . instr. dev_code;
            chan_data -> chanStatus = chanStatInvalidInstrPCW;
            //status_service (iom_unit_num, pcwp -> chan, false);
            break;
          }
        unitp = & crdrdr_unit [crdrdr_unit_num];
        crdrdr_cmd (unitp, & dcw . fields . instr, & disc);
        ctrl = dcw . fields . instr . control;
      }
sim_printf ("crdrdr interrupts\n");
    send_terminate_interrupt (iom_unit_num, pcwp -> chan);

    return 1;
  }

static t_stat crdrdr_svc (UNIT * unitp)
  {
    int crdrdrUnitNum = CRDRDR_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_crdrdr [crdrdrUnitNum] . iom_unit_num;
    int chanNum = cables_from_ioms_to_crdrdr [crdrdrUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    crdrdr_iom_cmd (unitp, pcwp);
    return SCPE_OK;
  }


static t_stat crdrdr_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED void * desc)
  {
    sim_printf("Number of CRDRDR units in system is %d\n", crdrdr_dev . numunits);
    return SCPE_OK;
  }

static t_stat crdrdr_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_CRDRDR_UNITS_MAX)
      return SCPE_ARG;
    crdrdr_dev . numunits = n;
    return SCPE_OK;
  }

static t_stat crdrdr_show_device_name (UNUSED FILE * st, UNIT * uptr,
                                       UNUSED int val, UNUSED void * desc)
  {
    int n = CRDRDR_UNIT_NUM (uptr);
    if (n < 0 || n >= N_CRDRDR_UNITS_MAX)
      return SCPE_ARG;
    sim_printf("Card reader device name is %s\n", crdrdr_state [n] . device_name);
    return SCPE_OK;
  }

static t_stat crdrdr_set_device_name (UNUSED UNIT * uptr, UNUSED int32 value,
                                    UNUSED char * cptr, UNUSED void * desc)
  {
    int n = CRDRDR_UNIT_NUM (uptr);
    if (n < 0 || n >= N_CRDRDR_UNITS_MAX)
      return SCPE_ARG;
    if (cptr)
      {
        strncpy (crdrdr_state [n] . device_name, cptr, MAX_DEV_NAME_LEN - 1);
        crdrdr_state [n] . device_name [MAX_DEV_NAME_LEN - 1] = 0;
      }
    else
      crdrdr_state [n] . device_name [0] = 0;
    return SCPE_OK;
  }


