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
#include "dps8_utils.h"
#include "dps8_faults.h"
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


#define N_PRU_UNITS 1 // default

static t_stat urp_reset (DEVICE * dptr);
static t_stat urp_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat urp_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat urp_show_device_name (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat urp_set_device_name (UNIT * uptr, int32 value, char * cptr, void * desc);

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE )
UNIT urp_unit [N_URP_UNITS_MAX] =
  {
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL}
  };

#define URPUNIT_NUM(uptr) ((uptr) - urp_unit)

static DEBTAB urp_dt [] =
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
      "DEVICE_NAME",     /* print string */
      "DEVICE_NAME",         /* match string */
      urp_set_device_name, /* validation routine */
      urp_show_device_name, /* display routine */
      "Select the boot drive", /* value descriptor */
      NULL          // help
    },

    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };


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
    NULL          // description
};

#define MAX_DEV_NAME_LEN 64
static struct urp_state
  {
    char device_name [MAX_DEV_NAME_LEN];
  } urp_state [N_URP_UNITS_MAX];

/*
 * urp_init()
 *
 */

// Once-only initialization

void urp_init (void)
  {
    memset (urp_state, 0, sizeof (urp_state));
    //for (int i = 0; i < N_URP_UNITS_MAX; i ++)
      //urp_state [i] . urpfile = -1;
  }

static t_stat urp_reset (DEVICE * dptr)
  {
    for (uint i = 0; i < dptr -> numunits; i ++)
      {
        // sim_urp_reset (& urp_unit [i]);
        sim_cancel (& urp_unit [i]);
      }
    return SCPE_OK;
  }

#ifndef QUIET_UNUSED
// Given an array of word36 and a 9bit char offset, return the char

static word9 gc (word36 * b, uint os)
  {
    uint wordno = os / 4;
    uint charno = os % 4;
    return (word9) getbits36_9 (b [wordno], charno * 9);
  }
#endif

static int urp_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    UNIT * unitp = & urp_unit [devUnitIdx];
    int urp_unit_num = (int) URPUNIT_NUM (unitp);
    //int iomUnitIdx = cables -> cablesFromIomToPun [urp_unit_num] . iomUnitIdx;

    sim_debug (DBG_TRACE, & urp_dev, "urp_cmd CHAN_CMD %o DEV_CODE %o DEV_CMD %o COUNT %o\n", p -> IDCW_CHAN_CMD, p -> IDCW_DEV_CODE, p -> IDCW_DEV_CMD, p -> IDCW_COUNT);

    switch (p -> IDCW_DEV_CMD)
      {
#if 0
        case 000: // CMD 00 Request status
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & urp_dev, "Request status %d\n", urp_unit_num);
          }
          break;


        case 001: // CMD 001 -- load image buffer
          {
            sim_debug (DBG_NOTIFY, & urp_dev, "load image buffer\n");
            p -> isRead = false;
            // Get the DDCW

            bool ptro, send, uff;

            int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
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

            // We don't actually have a print chain, so just pretend we loaded the image data
            p -> stati = 04000; 
          }
          break;



// load_vfc: entry (pip, pcip, iop, rcode);
// 
// dcl 1 vfc_image aligned,                                    /* print VFC image */
//    (2 lpi fixed bin (8),                                    /* lines per inch */
//     2 image_length fixed bin (8),                           /* number of lines represented by image */
//     2 toip,                                                 /* top of inside page info */
//       3 line fixed bin (8),                                 /* line number */
//       3 pattern bit (9),                                    /* VFC pattern */
//     2 boip,                                                 /* bottom of inside page info */
//       3 line fixed bin (8),                                 /* line number */
//       3 pattern bit (9),                                    /* VFC pattern */
//     2 toop,                                                 /* top of outside page info */
//       3 line fixed bin (8),                                 /* line number */
//       3 pattern bit (9),                                    /* VFC pattern */
//     2 boop,                                                 /* bottom of outside page info */
//       3 line fixed bin (8),                                 /* line number */
//       3 pattern bit (9),                                    /* VFC pattern */
//     2 pad bit (18)) unal;                                   /* fill out last word */
// 
// dcl (toip_pattern init ("113"b3),                           /* top of inside page pattern */
//      toop_pattern init ("111"b3),                           /* top of outside page pattern */
//      bop_pattern init ("060"b3))                            /* bottom of page pattern */
//      bit (9) static options (constant);


        case 005: // CMD 001 -- load vfc image
          {
            sim_debug (DBG_NOTIFY, & urp_dev, "load vfc image\n");
            p -> isRead = false;

            // Get the DDCW

            bool ptro, send, uff;

            int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
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

            // We don't actually have VFC, so just pretend we loaded the image data
            p -> stati = 04000; 
          }
          break;



        case 034: // CMD 034 -- print edited ascii
          {
            p -> isRead = false;
            p -> initiate = false;

// The EURC MPC printer controller sets the number of DCWs in the IDCW and
// ignores the IOTD bits in the DDCWs.

            uint ddcwCnt = p -> IDCW_COUNT;
            // Process DDCWs

            bool ptro, send, uff;
            for (uint ddcwIdx = 0; ddcwIdx < ddcwCnt; ddcwIdx ++)
              {
                int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
                if (rc < 0)
                  {
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    sim_printf ("%s list service failed\n", __func__);
                    return -1;
                  }
                if (uff)
                  {
                    sim_printf ("%s ignoring uff\n", __func__); // XXX
                  }
                if (! send)
                  {
                    sim_printf ("%s nothing to send\n", __func__);
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    return 1;
                  }
                if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
                  {
                    sim_printf ("%s expected DDCW\n", __func__);
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    return -1;
                  }

                uint tally = p -> DDCW_TALLY;
                sim_debug (DBG_DEBUG, & urp_dev,
                           "%s: Tally %d (%o)\n", __func__, tally, tally);

                if (tally == 0)
                  tally = 4096;

                // Copy from core to buffer
                word36 buffer [tally];
                uint wordsProcessed = 0;
                iomIndirectDataService (iomUnitIdx, chan, buffer,
                                        & wordsProcessed, false);


#if 0
for (uint i = 0; i < tally; i ++)
   sim_printf (" %012"PRIo64"", buffer [i]);
sim_printf ("\n");
#endif

#if 0
              sim_printf ("<");
                for (uint i = 0; i < tally * 4; i ++) {
                uint wordno = i / 4;
                uint charno = i % 4;
                uint ch = (buffer [wordno] >> ((3 - charno) * 9)) & 0777;
                if (isprint (ch))
                  sim_printf ("%c", ch);
                else
                  sim_printf ("\\%03o", ch);
              }
                sim_printf (">\n");
#endif

                if (urp_state [urp_unit_num] . urpfile == -1)
                  openPunFile (urp_unit_num, buffer, tally);

                uint8 bytes [tally * 4];
                for (uint i = 0; i < tally * 4; i ++)
                  {
                    uint wordno = i / 4;
                    uint charno = i % 4;
                    bytes [i] = (buffer [wordno] >> ((3 - charno) * 9)) & 0777;
                  }

                for (uint i = 0; i < tally * 4; i ++)
                  {
                    uint8 ch = bytes [i];
                    if (ch == 037) // insert n spaces
                      {
                        const uint8 spaces [128] = "                                                                                                                                ";
                        i ++;
                        uint8 n = bytes [i] & 0177;
                        write (urp_state [urp_unit_num] . urpfile, spaces, n);
                      }
                    else if (ch == 013) // insert n new lines
                      {
                        const uint8 newlines [128] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
                        i ++;
                        uint8 n = bytes [i] & 0177;
                        if (n)
                          write (urp_state [urp_unit_num] . urpfile, newlines, n);
                        else
                          {
                            const uint cr = '\r';
                            write (urp_state [urp_unit_num] . urpfile, & cr, 1);
                          }
                      }
                    else if (ch == 014) // slew
                      {
                        const uint8 ff = '\f';
                        i ++;
                        write (urp_state [urp_unit_num] . urpfile, & ff, 1);
                      }
                    else if (ch)
                      {
                        write (urp_state [urp_unit_num] . urpfile, & ch, 1);
                      }
                  }

#if 0
                if (urp_state [urp_unit_num] . last)
                  {
                    close (urp_state [urp_unit_num] . urpfile);
                    urp_state [urp_unit_num] . urpfile = -1;
                    urp_state [urp_unit_num] . last = false;
                  }
                // Check for slew to bottom of page
                urp_state [urp_unit_num] . last = tally == 1 && buffer [0] == 0014011000000;
#else
                if (eoj (buffer, tally))
                  {
                    //sim_printf ("urp end of job\n");
                    close (urp_state [urp_unit_num] . urpfile);
                    urp_state [urp_unit_num] . urpfile = -1;
                    //urp_state [urp_unit_num] . last = false;
                  }
#endif
            } // for (ddcwIdx)

            p -> tallyResidue = 0;
            p -> stati = 04000; 
          }
          break;
#endif
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

            int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
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

            int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
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
            sim_debug (DBG_NOTIFY, & urp_dev, "Reset status %d\n", urp_unit_num);
          }
          break;

        default:
          {
            sim_warn ("urp daze %o\n", p -> IDCW_DEV_CMD);
            p -> stati = 04501; // cmd reject, invalid opcode
            p -> chanStatus = chanStatIncorrectDCW;
          }
          break;
        }   

    if (p -> IDCW_CONTROL == 3) // marker bit set
      {
        send_marker_interrupt (iomUnitIdx, (int) chan);
      }

    if (p -> IDCW_CHAN_CMD == 0)
      return 2; // don't do DCW list
    return 0;
  }

// 1 ignored command
// 0 ok
// -1 problem
int urp_iom_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {
        return urp_cmd (iomUnitIdx, chan);
      }
    sim_printf ("%s expected IDCW\n", __func__);
    return -1;
  }

static t_stat urp_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED void * desc)
  {
    sim_printf("Number of URPunits in system is %d\n", urp_dev . numunits);
    return SCPE_OK;
  }

static t_stat urp_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_URP_UNITS_MAX)
      return SCPE_ARG;
    urp_dev . numunits = (uint32) n;
    return SCPE_OK;
  }

static t_stat urp_show_device_name (UNUSED FILE * st, UNIT * uptr,
                                       UNUSED int val, UNUSED void * desc)
  {
    int n = (int) URPUNIT_NUM (uptr);
    if (n < 0 || n >= N_URP_UNITS_MAX)
      return SCPE_ARG;
    sim_printf("Card punch device name is %s\n", urp_state [n] . device_name);
    return SCPE_OK;
  }

static t_stat urp_set_device_name (UNUSED UNIT * uptr, UNUSED int32 value,
                                    UNUSED char * cptr, UNUSED void * desc)
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


