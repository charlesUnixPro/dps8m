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
    enum { no_mode, print_editted_mode, ignore_mode } io_mode;
    char device_name [MAX_DEV_NAME_LEN];
    int prtfile; // fd
    bool last;
    int chan;
    uint mask;
    uint ext;
  } prt_state [N_PRT_UNITS_MAX];

static int findPrtUnit (int iomUnitIdx, int chan_num, int dev_code)
  {
    for (int i = 0; i < N_PRT_UNITS_MAX; i ++)
      {
        if (iomUnitIdx == cables -> cablesFromIomToPrt [i] . iomUnitIdx &&
            chan_num     == cables -> cablesFromIomToPrt [i] . chan_num     &&
            dev_code     == cables -> cablesFromIomToPrt [i] . dev_code)
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

static int prt_cmd (UNIT * unitp, pcw_t * pcwp, bool * disc)
  {
#ifdef IOM2
    int prt_unit_num = PRT_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToPrt [prt_unit_num] . iomUnitIdx;
    struct prt_state * prt_statep = & prt_state [prt_unit_num];
    * disc = false;

    int chan = pcwp-> chan;

    iomChannelData_ * chan_data = & iomChannelData [iomUnitIdx] [chan];
#if 0
    if (chan_data -> ptp)
      sim_printf ("PTP in prt\n");
#endif
    chan_data -> stati = 0;

    switch (pcwp -> dev_cmd)
      {
        case 000: // CMD 00 Request status
          {
            chan_data -> stati = 04000;
            prt_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & prt_dev, "Request status %d\n", prt_unit_num);
            chan_data -> initiate = true;
          }
          break;



        case 001: // CMD 001 -- load image buffer
          {
#if 0
            // Get the DDCW
sim_printf ("load image buffer\n");
            dcw_t dcw;
            int rc = iomListService (iomUnitIdx, chan, & dcw, NULL);

            if (rc)
              {
                sim_printf ("list service failed\n");
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncomplete;
                break;
              }
            if (dcw . type != ddcw)
              {
                sim_printf ("not ddcw? %d\n", dcw . type);
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            uint type = dcw.fields.ddcw.type;
            uint tally = dcw.fields.ddcw.tally;
            uint daddr = dcw.fields.ddcw.daddr;
            // uint cp = dcw.fields.ddcw.cp;
            if (pcwp -> mask)
              daddr |= ((pcwp -> ext) & MASK6) << 18;
            if (type == 0) // IOTD
              * disc = true;
            else if (type == 1) // IOTP
              * disc = false;
            else
              {
                sim_printf ("uncomfortable with this\n"); // XXX
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & prt_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & prt_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);


            // We don't actually have a print chain, so just pretend we loaded the image data
            chan_data -> stati = 04000; 
#else
            sim_debug (DBG_NOTIFY, & prt_dev, "load image buffer %d\n", prt_unit_num);
            chan_data -> isRead = false;
            prt_statep -> io_mode = ignore_mode;
            chan_data -> initiate = true;
            chan_data -> stati = 04000;
#endif
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
#if 0
            // Get the DDCW
sim_printf ("load vfc image\n");
            dcw_t dcw;
            int rc = iomListService (iomUnitIdx, chan, & dcw, NULL);

            if (rc)
              {
                sim_printf ("list service failed\n");
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncomplete;
                break;
              }
            if (dcw . type != ddcw)
              {
                sim_printf ("not ddcw? %d\n", dcw . type);
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            uint type = dcw.fields.ddcw.type;
            uint tally = dcw.fields.ddcw.tally;
            uint daddr = dcw.fields.ddcw.daddr;
            // uint cp = dcw.fields.ddcw.cp;
            if (pcwp -> mask)
              daddr |= ((pcwp -> ext) & MASK6) << 18;
            if (type == 0) // IOTD
              * disc = true;
            else if (type == 1) // IOTP
              * disc = false;
            else
              {
                sim_printf ("uncomfortable with this\n"); // XXX
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & prt_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & prt_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

            // We don't actually have a print chain, so just pretend we loaded the image data
            chan_data -> stati = 04000; 
#else
            sim_debug (DBG_NOTIFY, & prt_dev, "load vfc image %d\n", prt_unit_num);
            chan_data -> isRead = false;
            prt_statep -> io_mode = ignore_mode;
            chan_data -> initiate = true;
            chan_data -> stati = 04000;
#endif
          }
          break;




        case 034: // CMD 034 -- print edited ascii
          {
#if 0
            // Get the DDCW
sim_printf ("print edited ASCII\n");
            dcw_t dcw;
            int rc = iomListService (iomUnitIdx, chan, & dcw, NULL);

            if (rc)
              {
                sim_printf ("list service failed\n");
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncomplete;
                break;
              }
            if (dcw . type != ddcw)
              {
                sim_printf ("not ddcw? %d\n", dcw . type);
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            uint type = dcw.fields.ddcw.type;
            uint tally = dcw.fields.ddcw.tally;
            uint daddr = dcw.fields.ddcw.daddr;
            // uint cp = dcw.fields.ddcw.cp;
            if (pcwp -> mask)
              daddr |= ((pcwp -> ext) & MASK6) << 18;
            if (type == 0) // IOTD
              * disc = true;
            else if (type == 1) // IOTP
              * disc = false;
            else
              {
                sim_printf ("uncomfortable with this\n"); // XXX
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }
sim_printf ("disc is %s\n", * disc ? "true" : "false");
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & prt_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & prt_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

            uint8  asciiBuffer [tally * 4 + 1];
            memset (asciiBuffer, 0, sizeof (asciiBuffer));
            uint words_processed = 0;

            word36 buffer [tally];
            if (chan_data -> ptp)
              {
                // copy from core into buffer
                indirectDataService (iomUnitIdx, chan, daddr, tally, buffer,
                                     idsTypeW36, false, & chan_data -> isOdd);
              }
            else
              {
                // copy from core into buffer
                for (uint i = 0; i < tally; i ++)
                  {
                    fetch_abs_word (daddr + i, buffer + i, __func__);
                  }
              }

#if 1
for (uint i = 0; i < tally; i ++)
   sim_printf (" %012llo", buffer [i]);
sim_printf ("\n");
#endif

#if 0
            sim_printf ("<");
            for (uint i = 0; i < tally * 4; i ++)
              {
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

#if 0
            if (prt_state [prt_unit_num] . prtfile != -1)
               write (prt_state [prt_unit_num] . prtfile, bytes, tally * 4);
#else
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
                    //uint8 n = bytes [i] & 0177;
                    write (prt_state [prt_unit_num] . prtfile, & ff, 1);
                  }
                else if (ch)
                  {
                    write (prt_state [prt_unit_num] . prtfile, & ch, 1);
                  }
              }
#endif

            if (prt_state [prt_unit_num] . last)
              {
                close (prt_state [prt_unit_num] . prtfile);
                prt_state [prt_unit_num] . prtfile = -1;
                prt_state [prt_unit_num] . last = false;
              }
            // Check for slew to bottom of page
            prt_state [prt_unit_num] . last = tally == 1 && buffer [0] == 0014011000000;


            chan_data -> tallyResidue = 0;
            chan_data -> isOdd = 0;
            chan_data -> stati = 04000; 
#else
            sim_debug (DBG_NOTIFY, & prt_dev, "print edited ascii %d\n", prt_unit_num);
            chan_data -> isRead = false;
            prt_statep -> io_mode = ignore_mode;
            chan_data -> initiate = true;
            chan_data -> stati = 04000;
#endif
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
            prt_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & prt_dev, "Reset status %d\n", prt_unit_num);
            chan_data -> initiate = true;
            * disc = true;
          }
          break;


        default:
          {
sim_printf ("prt daze %o\n", pcwp -> dev_cmd);
            chan_data -> stati = 04501; // cmd reject, invalid opcode
            prt_statep -> io_mode = no_mode;
            chan_data -> chanStatus = chanStatIncorrectDCW;
            * disc = true;
          }
          break;
      
      }
#endif
    return 0;
  }

#ifdef IOM2
static int prt_ddcw (UNIT * unitp, dcw_t * ddcwp)
  {
    int prt_unit_num = PRT_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToDsk [prt_unit_num] . iomUnitIdx;

    struct prt_state * prt_statep = & prt_state [prt_unit_num];
    iomChannelData_ * chan_data = & iomChannelData [iomUnitIdx] [prt_statep -> chan];
    switch (prt_statep -> io_mode)
      {
        case no_mode:
          {
            sim_debug (DBG_ERR, & prt_dev, "DDCW when io_mode == no_mode\n");
            chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
            chan_data -> chanStatus = chanStatIncorrectDCW;
          }
          break;

        case ignore_mode:
          {
#if 0
            uint tally = ddcwp -> fields.ddcw.tally;
            uint daddr = ddcwp -> fields.ddcw.daddr;
            if (prt_statep -> mask)
              daddr |= ((prt_statep -> ext) & MASK6) << 18;
            // uint cp = ddcwp -> fields.ddcw.cp;

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }
#endif

            chan_data -> stati = 00000; // Channel ready
          }
          break;

        case print_editted_mode:
          {
            uint tally = ddcwp -> fields.ddcw.tally;
            uint daddr = ddcwp -> fields.ddcw.daddr;
            if (prt_statep -> mask)
              daddr |= ((prt_statep -> ext) & MASK6) << 18;
            // uint cp = ddcwp -> fields.ddcw.cp;

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }
          }
          break;

      }
    return 0;
  }
#endif

int prt_iom_cmd (UNIT * unitp, pcw_t * pcwp)
  {
#ifdef IOM2
    int prt_unit_num = PRT_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToPrt [prt_unit_num] . iomUnitIdx;

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

    // uint chanloc = mbx_loc (iomUnitIdx, pcwp -> chan);
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
    //uint ctrl = 2;
    //while ((! disc) && ctrl == 2)
    int ptro = 0;
    for (;;)
      {
sim_printf ("perusing channel mbx lpw....\n");
        dcw_t dcw;
        int rc = iomListService (iomUnitIdx, pcwp -> chan, & dcw, & ptro);
        if (rc)
          {
sim_printf ("list service denies!\n");
            break;
          }
sim_printf ("persuing got type %d %012llo\n", dcw . type, dcw . raw);
        if (dcw . type == idcw)
          {

            // The dcw does not necessarily have the same dev_code as the pcw....
sim_printf ("pcw dev_code %o\n", dcw . fields . instr. dev_code);
            prt_unit_num = findPrtUnit (iomUnitIdx, pcwp -> chan, dcw . fields . instr. dev_code);
sim_printf ("pcw prt_unit_num %d\n", prt_unit_num);
            if (prt_unit_num < 0)
              {
// 04502 : COMMAND REJECTED, invalid device code
                iomChannelData_ * chan_data = & iomChannelData [iomUnitIdx] [pcwp -> chan];
                chan_data -> stati = 04502; 
                chan_data -> dev_code = dcw . fields . instr. dev_code;
                chan_data -> chanStatus = chanStatInvalidInstrPCW;
                //status_service (iomUnitIdx, pcwp -> chan, false);
                break;
              }
            unitp = & prt_unit [prt_unit_num];
            prt_cmd (unitp, & dcw . fields . instr, & disc);
#if 0
            ctrl = dcw . fields . instr . control;
sim_printf ("set ctrl to %d\n", ctrl);
sim_printf ("disc is %s\n", disc ? "true" : "false");
            if (ctrl != 2 && disc)
              break;
#endif
          }
        else if (dcw . type == ddcw)
          {
sim_printf ("data dcw\n");
            unitp = & prt_unit [prt_unit_num];
            prt_ddcw (unitp, & dcw);
sim_printf ("data dcw type %d\n", dcw . fields . ddcw . type);
            if (dcw . fields . ddcw . type == 0) // IOTD
              break;
          }
      }
//sim_printf ("prt interrupts\n");
    send_terminate_interrupt (iomUnitIdx, pcwp -> chan);
#endif
    return 1;
  }

static t_stat prt_svc (UNIT * unitp)
  {
#ifdef IOM2
    int prtUnitNum = PRT_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToPrt [prtUnitNum] . iomUnitIdx;
    int chanNum = cables -> cablesFromIomToPrt [prtUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitIdx] [chanNum] . pcw;
    prt_iom_cmd (unitp, pcwp);
#endif
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


