//
//  dps8_prt.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_prt.h"
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


#define N_PRT_UNITS 1 // default

static t_stat prt_reset (DEVICE * dptr);
static t_stat prt_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat prt_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat prt_show_device_name (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat prt_set_device_name (UNIT * uptr, int32 value, char * cptr, void * desc);

static t_stat prt_svc (UNIT *);

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE )
UNIT prt_unit [N_PRT_UNITS_MAX] =
  {
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (& prt_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL}
  };

#define PRT_UNIT_NUM(uptr) ((uptr) - prt_unit)

static DEBTAB prt_dt [] =
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

static MTAB prt_mod [] =
  {
    { UNIT_WATCH, 1, "WATCH", "WATCH", 0, 0, NULL, NULL },
    { UNIT_WATCH, 0, "NOWATCH", "NOWATCH", 0, 0, NULL, NULL },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      prt_set_nunits, /* validation routine */
      prt_show_nunits, /* display routine */
      "Number of PRT units in the system", /* value descriptor */
      NULL // Help
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_VALR | MTAB_NC, /* mask */
      0,            /* match */
      "DEVICE_NAME",     /* print string */
      "DEVICE_NAME",         /* match string */
      prt_set_device_name, /* validation routine */
      prt_show_device_name, /* display routine */
      "Select the boot drive", /* value descriptor */
      NULL          // help
    },

    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };


DEVICE prt_dev = {
    "PRT",       /*  name */
    prt_unit,    /* units */
    NULL,         /* registers */
    prt_mod,     /* modifiers */
    N_PRT_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    prt_reset,   /* reset */
    NULL,         /* boot */
    NULL,         /* attach */
    NULL,         /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    prt_dt,      /* debug flag names */
    NULL,         /* memory size change */
    NULL,         /* logical name */
    NULL,         // help
    NULL,         // attach help
    NULL,         // attach context
    NULL          // description
};

#define MAX_DEV_NAME_LEN 64
static struct prt_state
  {
    char device_name [MAX_DEV_NAME_LEN];
  } prt_state [N_PRT_UNITS_MAX];

static int findCrdrdrUnit (int iomUnitNum, int chan_num, int dev_code)
  {
    for (int i = 0; i < N_PRT_UNITS_MAX; i ++)
      {
        if (iomUnitNum == cables -> cablesFromIomToCrdRdr [i] . iomUnitNum &&
            chan_num     == cables -> cablesFromIomToCrdRdr [i] . chan_num     &&
            dev_code     == cables -> cablesFromIomToCrdRdr [i] . dev_code)
          return i;
      }
    return -1;
  }

/*
 * prt_init()
 *
 */

// Once-only initialization

void prt_init (void)
  {
    memset (prt_state, 0, sizeof (prt_state));
  }

static t_stat prt_reset (DEVICE * dptr)
  {
    for (uint i = 0; i < dptr -> numunits; i ++)
      {
        // sim_prt_reset (& prt_unit [i]);
        sim_cancel (& prt_unit [i]);
      }
    return SCPE_OK;
  }

static int prt_cmd (UNIT * unitp, pcw_t * pcwp, bool * disc)
  {
    int prt_unit_num = PRT_UNIT_NUM (unitp);
    int iomUnitNum = cables -> cablesFromIomToCrdRdr [prt_unit_num] . iomUnitNum;
    //struct prt_state * prt_statep = & prt_state [prt_unit_num];
    * disc = false;

    int chan = pcwp-> chan;
sim_printf ("prt_cmd %o [%lld]\n", pcwp -> dev_cmd, sim_timell ());
    iomChannelData_ * chan_data = & iomChannelData [iomUnitNum] [chan];
    if (chan_data -> ptp)
      sim_printf ("PTP in prt\n");
    chan_data -> stati = 0;

    switch (pcwp -> dev_cmd)
      {
#if 0
        case 000: // CMD 00 Request status
          {
            chan_data -> stati = 04000;
            sim_debug (DBG_NOTIFY, & prt_dev, "Request status %d\n", prt_unit_num);
            chan_data -> initiate = true;
          }
          break;

        case 001: // CMD 01 Read binary
          {
            sim_debug (DBG_NOTIFY, & prt_dev, "Read binary %d\n", prt_unit_num);
            // Get the DDCW
            dcw_t dcw;
            int rc = iomListService (iomUnitNum, chan, & dcw, NULL);

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


            //uint wordsProcessed = 0;
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
            status_service (iomUnitNum, pcwp -> chan, false);
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
            //prt_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & prt_dev, "Reset status %d\n", prt_unit_num);
            chan_data -> initiate = true;
          }
          break;

#endif

        default:
          {
sim_printf ("prt daze %o\n", pcwp -> dev_cmd);
            chan_data -> stati = 04501; // cmd reject, invalid opcode
            chan_data -> chanStatus = chanStatIncorrectDCW;
          }
          break;
      
      }

    return 0;
  }

int prt_iom_cmd (UNIT * unitp, pcw_t * pcwp)
  {
    int prt_unit_num = PRT_UNIT_NUM (unitp);
    int iomUnitNum = cables -> cablesFromIomToCrdRdr [prt_unit_num] . iomUnitNum;

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

    // uint chanloc = mbx_loc (iomUnitNum, pcwp -> chan);
    //lpw_t lpw;
    //fetch_and_parse_lpw (& lpw, chanloc, false);

// Ignore a CMD 051 in the PCW
    if (pcwp -> dev_cmd == 051)
      return 1;
    bool disc;
    prt_cmd (unitp, pcwp, & disc);

    // ctrl of the pcw is observed to be 0 even when there are idcws in the
    // list so ignore that and force it to 2.
    //uint ctrl = pcwp -> control;
    uint ctrl = 2;
    if (disc)
      ctrl = 0;
//sim_printf ("starting list; disc %d, ctrl %d\n", disc, ctrl);

    // It looks like the prt controller ignores IOTD and olny obeys ctrl...
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
        int rc = iomListService (iomUnitNum, pcwp -> chan, & dcw, & ptro);
        if (rc)
          {
sim_printf ("list service denies!\n");
            break;
          }
sim_printf ("persuing got type %d\n", dcw . type);
        if (dcw . type != idcw)
          {
// 04501 : COMMAND REJECTED, invalid command
            iomChannelData_ * chan_data = & iomChannelData [iomUnitNum] [pcwp -> chan];
            chan_data -> stati = 04501; 
            chan_data -> dev_code = dcw . fields . instr. dev_code;
            chan_data -> chanStatus = chanStatInvalidInstrPCW;
            //status_service (iomUnitNum, pcwp -> chan, false);
            break;
          }

// The dcw does not necessarily have the same dev_code as the pcw....

        prt_unit_num = findCrdrdrUnit (iomUnitNum, pcwp -> chan, dcw . fields . instr. dev_code);
        if (prt_unit_num < 0)
          {
// 04502 : COMMAND REJECTED, invalid device code
            iomChannelData_ * chan_data = & iomChannelData [iomUnitNum] [pcwp -> chan];
            chan_data -> stati = 04502; 
            chan_data -> dev_code = dcw . fields . instr. dev_code;
            chan_data -> chanStatus = chanStatInvalidInstrPCW;
            //status_service (iomUnitNum, pcwp -> chan, false);
            break;
          }
        unitp = & prt_unit [prt_unit_num];
        prt_cmd (unitp, & dcw . fields . instr, & disc);
        ctrl = dcw . fields . instr . control;
      }
sim_printf ("prt interrupts\n");
    send_terminate_interrupt (iomUnitNum, pcwp -> chan);

    return 1;
  }

static t_stat prt_svc (UNIT * unitp)
  {
    int prtUnitNum = PRT_UNIT_NUM (unitp);
    int iomUnitNum = cables -> cablesFromIomToCrdRdr [prtUnitNum] . iomUnitNum;
    int chanNum = cables -> cablesFromIomToCrdRdr [prtUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    prt_iom_cmd (unitp, pcwp);
    return SCPE_OK;
  }


static t_stat prt_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED void * desc)
  {
    sim_printf("Number of PRT units in system is %d\n", prt_dev . numunits);
    return SCPE_OK;
  }

static t_stat prt_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_PRT_UNITS_MAX)
      return SCPE_ARG;
    prt_dev . numunits = n;
    return SCPE_OK;
  }

static t_stat prt_show_device_name (UNUSED FILE * st, UNIT * uptr,
                                       UNUSED int val, UNUSED void * desc)
  {
    int n = PRT_UNIT_NUM (uptr);
    if (n < 0 || n >= N_PRT_UNITS_MAX)
      return SCPE_ARG;
    sim_printf("Card reader device name is %s\n", prt_state [n] . device_name);
    return SCPE_OK;
  }

static t_stat prt_set_device_name (UNUSED UNIT * uptr, UNUSED int32 value,
                                    UNUSED char * cptr, UNUSED void * desc)
  {
    int n = PRT_UNIT_NUM (uptr);
    if (n < 0 || n >= N_PRT_UNITS_MAX)
      return SCPE_ARG;
    if (cptr)
      {
        strncpy (prt_state [n] . device_name, cptr, MAX_DEV_NAME_LEN - 1);
        prt_state [n] . device_name [MAX_DEV_NAME_LEN - 1] = 0;
      }
    else
      prt_state [n] . device_name [0] = 0;
    return SCPE_OK;
  }

#if 0
void prtCardReady (int unitNum)
  {
    send_special_interrupt (cables -> cablesFromIomToCrdRdr [unitNum] . iomUnitNum,
                            cables -> cablesFromIomToCrdRdr [unitNum] . chan_num,
                            cables -> cablesFromIomToCrdRdr [unitNum] . dev_code,
                            0000, 0001 /* tape drive to ready */);
  }
#endif

