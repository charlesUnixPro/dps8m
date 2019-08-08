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
//  dps8_urp.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_urp.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_cable.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"

#define DBG_CTR 1

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


#define N_PRU_UNITS 1 // default

static struct urp_state
  {
    char device_name [MAX_DEV_NAME_LEN];
  } urp_state [N_URP_UNITS_MAX];

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE )
UNIT urp_unit [N_URP_UNITS_MAX] =
  {
    [0 ... N_URP_UNITS_MAX-1] =
      {
        UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL
      }
  };

#define URPUNIT_NUM(uptr) ((uptr) - urp_unit)

static DEBTAB urp_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

static t_stat urp_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED const void * desc)
  {
    sim_printf("Number of URPunits in system is %d\n", urp_dev . numunits);
    return SCPE_OK;
  }

static t_stat urp_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, const char * cptr, UNUSED void * desc)
  {
    if (! cptr)
      return SCPE_ARG;
    int n = atoi (cptr);
    if (n < 1 || n > N_URP_UNITS_MAX)
      return SCPE_ARG;
    urp_dev . numunits = (uint32) n;
    return SCPE_OK;
  }

static t_stat urp_show_device_name (UNUSED FILE * st, UNIT * uptr,
                                       UNUSED int val, UNUSED const void * desc)
  {
    int n = (int) URPUNIT_NUM (uptr);
    if (n < 0 || n >= N_URP_UNITS_MAX)
      return SCPE_ARG;
    sim_printf("Card punch device name is %s\n", urp_state [n] . device_name);
    return SCPE_OK;
  }

static t_stat urp_set_device_name (UNUSED UNIT * uptr, UNUSED int32 value,
                                    UNUSED const char * cptr, UNUSED void * desc)
  {
    int n = (int) URPUNIT_NUM (uptr);
    if (n < 0 || n >= N_URP_UNITS_MAX)
      return SCPE_ARG;
    if (cptr)
      {
        strncpy (urp_state [n] . device_name, cptr, MAX_DEV_NAME_LEN - 1);
        urp_state [n] . device_name [MAX_DEV_NAME_LEN - 1] = 0;
      }
    else
      urp_state [n] . device_name [0] = 0;
    return SCPE_OK;
  }

#define UNIT_WATCH UNIT_V_UF

static MTAB urp_mod [] =
  {
    { UNIT_WATCH, 1, "WATCH", "WATCH", 0, 0, NULL, NULL },
    { UNIT_WATCH, 0, "NOWATCH", "NOWATCH", 0, 0, NULL, NULL },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      urp_set_nunits, /* validation routine */
      urp_show_nunits, /* display routine */
      "Number of URPunits in the system", /* value descriptor */
      NULL // Help
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_VALR | MTAB_NC, /* mask */
      0,            /* match */
      "NAME",     /* print string */
      "NAME",         /* match string */
      urp_set_device_name, /* validation routine */
      urp_show_device_name, /* display routine */
      "Set the device name", /* value descriptor */
      NULL          // help
    },

    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };

static t_stat urp_reset (UNUSED DEVICE * dptr)
  {
    return SCPE_OK;
  }


DEVICE urp_dev = {
    "URP",       /*  name */
    urp_unit,    /* units */
    NULL,         /* registers */
    urp_mod,     /* modifiers */
    N_PRU_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    urp_reset,   /* reset */
    NULL,         /* boot */
    NULL,         /* attach */
    NULL,         /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    urp_dt,      /* debug flag names */
    NULL,         /* memory size change */
    NULL,         /* logical name */
    NULL,         // help
    NULL,         // attach help
    NULL,         // attach context
    NULL,         // description
    NULL
};

/*
 * urp_init()
 *
 */

// Once-only initialization

void urp_init (void)
  {
    memset (urp_state, 0, sizeof (urp_state));
  }


static int urp_cmd (uint iomUnitIdx, uint chan)
  {
    iom_chan_data_t * p = & iom_chan_data [iomUnitIdx] [chan];
    uint ctlr_unit_idx = get_ctlr_idx (iomUnitIdx, chan);
    uint devUnitIdx = cables->urp_to_urd[ctlr_unit_idx][p->IDCW_DEV_CODE].unit_idx;
    UNIT * unitp = & urp_unit [devUnitIdx];
    int urp_unit_num = (int) URPUNIT_NUM (unitp);
    //int iomUnitIdx = cables -> cablesFromIomToPun [urp_unit_num] . iomUnitIdx;

    sim_debug (DBG_TRACE, & urp_dev, "urp_cmd CHAN_CMD %o DEV_CODE %o DEV_CMD %o COUNT %o\n", p -> IDCW_CHAN_CMD, p -> IDCW_DEV_CODE, p -> IDCW_DEV_CMD, p -> IDCW_COUNT);

    switch (p -> IDCW_DEV_CMD)
      {
        case 000: // CMD 00 Request status
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & urp_dev, "Request status %d\n", urp_unit_num);
          }
          break;

        case 006: // CMD 005 Initiate read data xfer (load_mpc.pl1)
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & urp_dev, "Initiate read data xfer %d\n", urp_unit_num);

            // Get the DDCW

            bool ptro, send, uff;

            int rc = iom_list_service (iomUnitIdx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                sim_printf ("%s list service failed\n", __func__);
                break;
              }
            if (uff)
              {
                sim_printf ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_printf ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                break;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_printf ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                break;
              }

            // We don't use the DDCW, so just pretend we do.
            p -> stati = 04000; 
          }
          break;


// 011 punch binary
// 031 set diagnostic mode

        case 031: // CMD 031 Set Diagnostic Mode (load_mpc.pl1)
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & urp_dev, "Set Diagnostic Mode %d\n", urp_unit_num);

            // Get the DDCW

            bool ptro, send, uff;

            int rc = iom_list_service (iomUnitIdx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                sim_printf ("%s list service failed\n", __func__);
                break;
              }
            if (uff)
              {
                sim_printf ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_printf ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                break;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_printf ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                break;
              }

            // We don't use the DDCW, so just pretend we do.
            p -> stati = 04000; 
          }
          break;

        case 040: // CMD 40 Reset status
          {
            p -> stati = 04000;
            p -> initiate = false;
            p -> isRead = false;
            sim_debug (DBG_NOTIFY, & urp_dev, "Reset status %d\n", urp_unit_num);
          }
          break;

        default:
          {
            p -> stati = 04501; // cmd reject, invalid opcode
            p -> chanStatus = chanStatIncorrectDCW;
            if (p->IDCW_DEV_CMD != 051) // ignore bootload console probe
              sim_warn ("urp daze %o\n", p -> IDCW_DEV_CMD);
          }
          return IOM_CMD_ERROR;
        }   

    if (p -> IDCW_CONTROL == 3) // marker bit set
      {
        send_marker_interrupt (iomUnitIdx, (int) chan);
      }

    if (p -> IDCW_CHAN_CMD == 0)
      return IOM_CMD_NO_DCW; // don't do DCW list
    return IOM_CMD_OK;
  }

// 1 ignored command
// 0 ok
// -1 problem
int urp_iom_cmd (uint iomUnitIdx, uint chan)
  {
    iom_chan_data_t * p = & iom_chan_data [iomUnitIdx] [chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {
        uint dev_code = p->IDCW_DEV_CODE;
        if (dev_code == 0)
          return urp_cmd (iomUnitIdx, chan);
        uint urp_unit_idx = cables->iom_to_ctlr[iomUnitIdx][chan].ctlr_unit_idx;
        iom_cmd_t * cmd =  cables->urp_to_urd[urp_unit_idx][dev_code].iom_cmd;
        if (! cmd)
          {
            sim_warn ("URP can't find divice handler\n");
            return IOM_CMD_ERROR;
          }
        return cmd (iomUnitIdx, chan);
      }
    sim_printf ("%s expected IDCW\n", __func__);
    return IOM_CMD_ERROR;
  }


