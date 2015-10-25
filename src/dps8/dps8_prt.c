//
//  dps8_prt.c
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

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE )
UNIT prt_unit [N_PRT_UNITS_MAX] =
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
    enum { no_mode, print_editted_mode, ignore_mode } io_mode;
    char device_name [MAX_DEV_NAME_LEN];
    int prtfile; // fd
    bool last;
    int chan;
    uint mask;
    uint ext;
  } prt_state [N_PRT_UNITS_MAX];

/*
 * prt_init()
 *
 */

// Once-only initialization

void prt_init (void)
  {
    memset (prt_state, 0, sizeof (prt_state));
    for (int i = 0; i < N_PRT_UNITS_MAX; i ++)
      prt_state [i] . prtfile = -1;
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

static void openPrtFile (int prt_unit_num)
  {
    if (prt_state [prt_unit_num] . prtfile != -1)
      return;
    char template [129];
    sprintf (template, "prt%c.spool.XXXXXX", 'a' + prt_unit_num - 1);
    prt_state [prt_unit_num] . prtfile = mkstemp (template);
  }

// looking for lines "\037\014%%%%%\037\005"
static int eoj (word36 * buffer, uint tally)
  {
    if (tally < 3)
      return 0;
    if (getbits36 (buffer [0], 0, 9) != 037)
      return 0;
    if (getbits36 (buffer [0], 9, 9) != 014)
      return 0;
    word9 ch = getbits36 (buffer [0], 18, 9);
    if (ch < '0' || ch > '9')
      return 0;
    ch = getbits36 (buffer [0], 27, 9);
    if (ch < '0' || ch > '9')
      return 0;
    ch = getbits36 (buffer [1], 0, 9);
    if (ch < '0' || ch > '9')
      return 0;
    ch = getbits36 (buffer [1], 9, 9);
    if (ch < '0' || ch > '9')
      return 0;
    ch = getbits36 (buffer [1], 18, 9);
    if (ch < '0' || ch > '9')
      return 0;
    if (getbits36 (buffer [1], 27, 9) != 037)
      return 0;
    if (getbits36 (buffer [2], 0, 9) != 005)
      return 0;
    return 1;
  }

static int prt_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    UNIT * unitp = & prt_unit [devUnitIdx];
    int prt_unit_num = PRT_UNIT_NUM (unitp);
    //int iomUnitIdx = cables -> cablesFromIomToPrt [prt_unit_num] . iomUnitIdx;

    switch (p -> IDCW_DEV_CMD)
      {
        case 000: // CMD 00 Request status
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & prt_dev, "Request status %d\n", prt_unit_num);
          }
          break;


        case 001: // CMD 001 -- load image buffer
          {
            sim_debug (DBG_NOTIFY, & prt_dev, "load image buffer\n");
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
            sim_debug (DBG_NOTIFY, & prt_dev, "load vfc image\n");
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
                sim_debug (DBG_DEBUG, & prt_dev,
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
   sim_printf (" %012llo", buffer [i]);
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

                if (prt_state [prt_unit_num] . prtfile == -1)
                  openPrtFile (prt_unit_num);

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
                        write (prt_state [prt_unit_num] . prtfile, spaces, n);
                      }
                    else if (ch == 013) // insert n new lines
                      {
                        const uint8 newlines [128] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
                        i ++;
                        uint8 n = bytes [i] & 0177;
                        write (prt_state [prt_unit_num] . prtfile, newlines, n);
                      }
                    else if (ch == 014) // slew
                      {
                        const uint8 ff = '\f';
                        i ++;
                        write (prt_state [prt_unit_num] . prtfile, & ff, 1);
                      }
                    else if (ch)
                      {
                        write (prt_state [prt_unit_num] . prtfile, & ch, 1);
                      }
                  }

#if 0
                if (prt_state [prt_unit_num] . last)
                  {
                    close (prt_state [prt_unit_num] . prtfile);
                    prt_state [prt_unit_num] . prtfile = -1;
                    prt_state [prt_unit_num] . last = false;
                  }
                // Check for slew to bottom of page
                prt_state [prt_unit_num] . last = tally == 1 && buffer [0] == 0014011000000;
#else
                if (eoj (buffer, tally))
                  {
                    sim_printf ("prt end of job\n");
                    close (prt_state [prt_unit_num] . prtfile);
                    prt_state [prt_unit_num] . prtfile = -1;
                    prt_state [prt_unit_num] . last = false;
                  }
#endif
            } // for (ddcwIdx)

            p -> tallyResidue = 0;
            p -> stati = 04000; 
          }
          break;

        case 040: // CMD 40 Reset status
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & prt_dev, "Reset status %d\n", prt_unit_num);
          }
          break;


        default:
          {
sim_printf ("prt daze %o\n", p -> IDCW_DEV_CMD);
            p -> stati = 04501; // cmd reject, invalid opcode
            p -> chanStatus = chanStatIncorrectDCW;
          }
          break;
        }   

    if (p -> IDCW_CONTROL == 3) // marker bit set
      {
        send_marker_interrupt (iomUnitIdx, chan);
      }
    return 0;
  }

// 1 ignored command
// 0 ok
// -1 problem
int prt_iom_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {
        prt_cmd (iomUnitIdx, chan);
      }
    else // DDCW/TDCW
      {
        sim_printf ("%s expected IDCW\n", __func__);
        return -1;
      }
    return 0;
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


