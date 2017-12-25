/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2017 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_socket_dev.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"

#define N_SK_UNITS 1 // default

static t_stat sk_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                              UNUSED int val, UNUSED const void * desc)
  {
    sim_printf("Number of TAPE units in system is %d\n", sk_dev.numunits);
    return SCPE_OK;
  }

static t_stat sk_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_MT_UNITS_MAX)
      return SCPE_ARG;
    sk_dev.numunits = (uint32) n;
    return SCPE_OK;
  }

static MTAB sk_mod [] =
  {
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      sk_set_nunits, /* validation routine */
      sk_show_nunits, /* display routine */
      "Number of TAPE units in the system", /* value descriptor */
      NULL          // help
    },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };


UNIT sk_unit [N_SK_UNITS_MAX] = {
    {UDATA ( NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
};

static DEBTAB sk_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

#define SK_UNIT_NUM(uptr) ((uptr) - sk_unit)

static t_stat sk_reset (DEVICE * dptr)
  {
    return SCPE_OK;
  }


DEVICE sk_dev = {
    "SK",             /* name */
    sk_unit,          /* units */
    NULL,             /* registers */
    sk_mod,           /* modifiers */
    N_SK_UNITS,       /* #units */
    10,               /* address radix */
    31,               /* address width */
    1,                /* address increment */
    8,                /* data radix */
    9,                /* data width */
    NULL,             /* examine routine */
    NULL,             /* deposit routine */
    sk_reset,         /* reset routine */
    NULL,             /* boot routine */
    NULL,             /* attach routine */
    NULL,             /* detach routine */
    NULL,             /* context */
    DEV_DEBUG,        /* flags */
    0,                /* debug control flags */
    sk_dt,            /* debug flag names */
    NULL,             /* memory size change */
    NULL,             /* logical name */
    NULL,             // attach help
    NULL,             // help
    NULL,             // help context
    NULL,             // device description
    NULL
};




void sk_init(void)
  {
    //memset(sk_states, 0, sizeof(sk_states));
  }

static int sk_cmd (uint iom_unit_idx, uint chan)
  {
    iomChanData_t * p = & iomChanData[iom_unit_idx][chan];

    sim_debug (DBG_DEBUG, & sk_dev, "IDCW_DEV_CODE %d\n", p->IDCW_DEV_CODE);
    //struct device * d = & cables -> cablesFromIomToDev [iom_unit_idx].devices[chan][p->IDCW_DEV_CODE];
    //uint devUnitIdx = d->devUnitIdx;
    //UNIT * unitp = & sk_unit[devUnitIdx];

    switch (p -> IDCW_DEV_CMD)
      {
        case 0: // CMD 00 Request status -- controler status, not tape drive
          {
            p -> stati = 04000; // have_status = 1
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Request status: %04o\n", __func__, p -> stati);
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Request status control: %o\n", __func__, p -> IDCW_CONTROL);
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Request status channel command: %o\n", __func__, p -> IDCW_CHAN_CMD);
          }
          break;

        case 03:               // CMD 03 -- Debugging
          {
            sim_printf ("socket_dev received command 3\r\n");
            p -> stati = 04000;
          }
         break;

        case 01:               // CMD 01 -- socket()
          {
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: socket_dev_$socket\n", __func__);

            bool ptro, send, uff;
            int rc = iomListService (iom_unit_idx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p->stati = 05001; // BUG: arbitrary error code; config switch
                sim_warn ("%s list service failed\n", __func__);
                return -1;
              }
            if (uff)
              {
                sim_warn ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_warn ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return 1;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_warn ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            uint tally = p -> DDCW_TALLY;
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & sk_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

            if (tally != 6)
              {
                sim_warn ("socket_dev socket call expected tally of 6; get %d\n", tally);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, false);

sim_printf ("socket() domain   %012llo\n", buffer [0]);
sim_printf ("socket() type     %012llo\n", buffer [1]);
sim_printf ("socket() protocol %012llo\n", buffer [2]);
sim_printf ("socket() pid      %012llo\n", buffer [3]);

          }

        case 02:               // CMD 01 -- bind()
          {
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: socket_dev_$socket\n", __func__);

            bool ptro, send, uff;
            int rc = iomListService (iom_unit_idx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p->stati = 05001; // BUG: arbitrary error code; config switch
                sim_warn ("%s list service failed\n", __func__);
                return -1;
              }
            if (uff)
              {
                sim_warn ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_warn ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return 1;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_warn ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            uint tally = p -> DDCW_TALLY;
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & sk_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

            if (tally != 6)
              {
                sim_warn ("socket_dev bind call expected tally of 6; get %d\n", tally);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, false);

sim_printf ("bind() sin_family %012llo\n", buffer [0]);
sim_printf ("bind() port       %012o\n",   getbits36_16 (buffer [1], 0));
//sim_printf ("bind() s_addr     %012llo\n", getibits36_32 (buffer [2], 0));
//sim_printf ("bind() s_addr     %012llo\n", (buffer [2] >> 4) & MASK32);
sim_printf ("bind() s_addr     %llu.%llu.%llu.%llu\n",
  (buffer [2] >> (36 - 1 * 8)) & MASK8,
  (buffer [2] >> (36 - 2 * 8)) & MASK8,
  (buffer [2] >> (36 - 3 * 8)) & MASK8,
  (buffer [2] >> (36 - 4 * 8)) & MASK8),
sim_printf ("bind() pid        %012llo\n", buffer [3]);

          }

        default:
          {
            p -> stati = 04501;
            p -> chanStatus = chanStatIncorrectDCW;
            sim_warn ("%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
          }
          break;

      } // IDCW_DEV_CMD

    sim_debug (DBG_DEBUG, & sk_dev, "stati %04o\n", p -> stati);

#if 0
    if (p -> IDCW_CONTROL == 3) // marker bit set
      {
        send_marker_interrupt (iom_unit_idx, (int) chan);
      }
#endif
    return 0;
  }


int sk_iom_cmd (uint iom_unit_idx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iom_unit_idx] [chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {
        sk_cmd (iom_unit_idx, chan);
      }
    else // DDCW/TDCW
      {
        sim_warn ("%s expected IDCW\n", __func__);
        return -1;
      }
    return 3; // command pending, don't sent terminate interrupt
  }

