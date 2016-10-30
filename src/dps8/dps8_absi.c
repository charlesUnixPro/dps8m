/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2015-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//
//  dps8_absi.c
//

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_absi.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_cable.h"

#include "udplib.h"

//-- // XXX We use this where we assume there is only one unit
//-- #define ASSUME0 0
//-- 
 
#define N_ABSI_UNITS 1 // default

static t_stat absi_reset (DEVICE * dptr);
static t_stat absi_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat absi_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static t_stat absiAttach (UNIT *uptr, char *cptr);
static t_stat absiDetach (UNIT *uptr);

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE )
UNIT absi_unit [N_ABSI_UNITS_MAX] =
  {
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL}
  };

#define ABSIUNIT_NUM(uptr) ((uptr) - absi_unit)

static DEBTAB absi_dt [] =
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

static MTAB absi_mod [] =
  {
    { UNIT_WATCH, 1, "WATCH", "WATCH", 0, 0, NULL, NULL },
    { UNIT_WATCH, 0, "NOWATCH", "NOWATCH", 0, 0, NULL, NULL },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      absi_set_nunits, /* validation routine */
      absi_show_nunits, /* display routine */
      "Number of ABSIunits in the system", /* value descriptor */
      NULL // Help
    },

    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };


DEVICE absi_dev = {
    "ABSI",       /*  name */
    absi_unit,    /* units */
    NULL,         /* registers */
    absi_mod,     /* modifiers */
    N_ABSI_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    absi_reset,   /* reset */
    NULL,         /* boot */
    absiAttach,         /* attach */
    absiDetach,         /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    absi_dt,      /* debug flag names */
    NULL,         /* memory size change */
    NULL,         /* logical name */
    NULL,         // help
    NULL,         // attach help
    NULL,         // attach context
    NULL          // description
};

static struct absi_state
  {
    int link;
  } absi_state [N_ABSI_UNITS_MAX];

/*
 * absi_init()
 *
 */

// Once-only initialization

void absi_init (void)
  {
    memset (absi_state, 0, sizeof (absi_state));
    for (int i = 0; i < N_ABSI_UNITS_MAX; i ++)
      absi_state [i] . link = NOLINK;
  }

static t_stat absi_reset (UNUSED DEVICE * dptr)
  {
    //absiResetRX (0);
    //absiResetTX (0);

    //for (uint i = 0; i < dptr -> numunits; i ++)
      //{
        //sim_cancel (& absi_unit [i]);
      //}
    // if ((uptr->flags & UNIT_ATT) != 0) sim_activate(uptr, uptr->wait);
    return SCPE_OK;
  }

static int absi_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    //struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
    //                  devices [chan] [p -> IDCW_DEV_CODE];
    //uint devUnitIdx = d -> devUnitIdx;
    //UNIT * unitp = & absi_unit [devUnitIdx];
    //int absi_unit_num = ABSIUNIT_NUM (unitp);

sim_printf ("absi_cmd CHAN_CMD %o DEV_CODE %o DEV_CMD %o COUNT %o\n", p -> IDCW_CHAN_CMD, p -> IDCW_DEV_CODE, p -> IDCW_DEV_CMD, p -> IDCW_COUNT);
    sim_debug (DBG_TRACE, & absi_dev, "absi_cmd CHAN_CMD %o DEV_CODE %o DEV_CMD %o COUNT %o\n", p -> IDCW_CHAN_CMD, p -> IDCW_DEV_CODE, p -> IDCW_DEV_CMD, p -> IDCW_COUNT);


    switch (p -> IDCW_DEV_CMD)
      {
        case 000: // CMD 00 Request status
          {
            p -> stati = 04000;
sim_printf ("absi request status\n");
          }
          break;

        case 001: // CMD 01 Read
          {
            p -> stati = 04000;
sim_printf ("absi read\n");
          }
          break;

        case 011: // CMD 11 Write
          {
            p -> stati = 04000;
sim_printf ("absi write\n");
          }
          break;

        case 020: // CMD 20 Host switch down
          {
            p -> stati = 04000;
sim_printf ("absi host switch down\n");
          }
          break;

        case 040: // CMD 40 Reset status
          {
            p -> stati = 04000;
sim_printf ("absi reset status\n");
          }
          break;

        case 060: // CMD 60 Host switch up
          {
            p -> stati = 04000;
sim_printf ("absi host switch up\n");
          }
          break;

        default:
          {
            sim_warn ("absi daze %o\n", p -> IDCW_DEV_CMD);
            p -> stati = 04501; // cmd reject, invalid opcode
            p -> chanStatus = chanStatIncorrectDCW;
          }
          break;
      }

#if 0
    switch (p -> IDCW_DEV_CMD)
      {
#if 0
        case 000: // CMD 00 Request status
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & absi_dev, "Request status %d\n", absi_unit_num);
          }
          break;


        case 001: // CMD 001 -- load image buffer
          {
            sim_debug (DBG_NOTIFY, & absi_dev, "load image buffer\n");
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
            sim_debug (DBG_NOTIFY, & absi_dev, "load vfc image\n");
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
                sim_debug (DBG_DEBUG, & absi_dev,
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

                if (absi_state [absi_unit_num] . urpfile == -1)
                  openPunFile (absi_unit_num, buffer, tally);

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
                        write (absi_state [absi_unit_num] . urpfile, spaces, n);
                      }
                    else if (ch == 013) // insert n new lines
                      {
                        const uint8 newlines [128] = "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
                        i ++;
                        uint8 n = bytes [i] & 0177;
                        if (n)
                          write (absi_state [absi_unit_num] . urpfile, newlines, n);
                        else
                          {
                            const uint cr = '\r';
                            write (absi_state [absi_unit_num] . urpfile, & cr, 1);
                          }
                      }
                    else if (ch == 014) // slew
                      {
                        const uint8 ff = '\f';
                        i ++;
                        write (absi_state [absi_unit_num] . urpfile, & ff, 1);
                      }
                    else if (ch)
                      {
                        write (absi_state [absi_unit_num] . urpfile, & ch, 1);
                      }
                  }

#if 0
                if (absi_state [absi_unit_num] . last)
                  {
                    close (absi_state [absi_unit_num] . urpfile);
                    absi_state [absi_unit_num] . urpfile = -1;
                    absi_state [absi_unit_num] . last = false;
                  }
                // Check for slew to bottom of page
                absi_state [absi_unit_num] . last = tally == 1 && buffer [0] == 0014011000000;
#else
                if (eoj (buffer, tally))
                  {
                    //sim_printf ("urp end of job\n");
                    close (absi_state [absi_unit_num] . urpfile);
                    absi_state [absi_unit_num] . urpfile = -1;
                    //absi_state [absi_unit_num] . last = false;
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
            sim_debug (DBG_NOTIFY, & absi_dev, "Request status %d\n", absi_unit_num);
          }
          break;

        case 006: // CMD 005 Initiate read data xfer (load_mpc.pl1)
          {
            p -> stati = 04000;
            sim_debug (DBG_NOTIFY, & absi_dev, "Initiate read data xfer %d\n", absi_unit_num);

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
            sim_debug (DBG_NOTIFY, & absi_dev, "Set Diagnostic Mode %d\n", absi_unit_num);

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
            sim_debug (DBG_NOTIFY, & absi_dev, "Reset status %d\n", absi_unit_num);
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
#endif

    if (p -> IDCW_CONTROL == 3) // marker bit set
      {
sim_printf ("absi marker\n");
        send_marker_interrupt (iomUnitIdx, (int) chan);
      }

    if (p -> IDCW_CHAN_CMD == 0)
      return 2; // don't do DCW list
    return 0;
  }

// 1 ignored command
// 0 ok
// -1 problem
int absi_iom_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {
        return absi_cmd (iomUnitIdx, chan);
      }
    sim_printf ("%s expected IDCW\n", __func__);
    return -1;
  }

void absiProcessEvent (void)
  {
#define psz 17000
    uint16_t pkt [psz];
    for (uint32 unit = 0; unit < absi_dev.numunits; unit ++)
      {
        if (absi_state[unit].link == NOLINK)
          continue;
        //int sz = udp_receive ((int) unit, pkt, psz);
        int sz = udp_receive (absi_state[unit].link, pkt, psz);
        if (sz < 0)
          {
            printf ("udp_receive failed\n");
          }
        else if (sz == 0)
          {
            //printf ("udp_receive 0\n");
          }
        else
          {
            for (int i = 0; i < sz; i ++)
              {
                printf ("  %06o  %04x  ", pkt [i], pkt [i]);
                for (int b = 0; b < 16; b ++)
                  printf ("%c", pkt [i] & (1 << (16 - b)) ? '1' : '0');
                printf ("\n");
              }
            // Send a NOP reply
            //int16_t reply [2] = 0x0040
            int rc = udp_send (absi_state[unit].link, pkt, (uint16_t) sz, PFLG_FINAL);
            if (rc < 0)
              {
                printf ("udp_send failed\n");
              }
          }
      }
  }

static t_stat absi_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED void * desc)
  {
    sim_printf("Number of ABSIunits in system is %d\n", absi_dev . numunits);
    return SCPE_OK;
  }

static t_stat absi_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_ABSI_UNITS_MAX)
      return SCPE_ARG;
    absi_dev . numunits = (uint32) n;
    return SCPE_OK;
  }

t_stat absiAttach (UNIT * uptr, char * cptr)
  {
    int unitno = (int) (uptr - absi_unit);

    //    ATTACH HIn llll:w.x.y.z:rrrr - connect via UDP to a remote simh host

    t_stat ret;
    char * pfn;
    //uint16 imp = 0; // we only support a single attachment to a single IMP

    // If we're already attached, then detach ...
    if ((uptr -> flags & UNIT_ATT) != 0)
      detach_unit (uptr);

    //   Make a copy of the "file name" argument.  udp_create() actually modifies
    // the string buffer we give it, so we make a copy now so we'll have something
    // to display in the "SHOW HIn ..." command.
    pfn = (char *) calloc (CBUFSIZE, sizeof (char));
    if (pfn == NULL)
      return SCPE_MEM;
    strncpy (pfn, cptr, CBUFSIZE);

    // Create the UDP connection.
    ret = udp_create (cptr, & absi_state [unitno] . link);
    if (ret != SCPE_OK)
      {
        free (pfn);
        return ret;
      }

    uptr -> flags |= UNIT_ATT;
    uptr -> filename = pfn;
    return SCPE_OK;
  }

// Detach (connect) ...
t_stat absiDetach (UNIT * uptr)
  {
    int unitno = (int) (uptr - absi_unit);
    t_stat ret;
    if ((uptr -> flags & UNIT_ATT) == 0)
      return SCPE_OK;
    if (absi_state [unitno] . link == NOLINK)
      return SCPE_OK;

    ret = udp_release (absi_state [unitno] . link);
    if (ret != SCPE_OK)
      return ret;
    absi_state [unitno] . link = NOLINK;
    uptr -> flags &= ~(unsigned int) UNIT_ATT;
    free (uptr -> filename);
    uptr -> filename = NULL;
    return SCPE_OK;
  }




