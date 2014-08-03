//
//  dps8_dn355.c
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//

#include <stdio.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_dn355.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */


// AN87, Chapter 6

#define N_DN355_UNITS_MAX 1
#define N_DN355_UNITS 1 // default

static t_stat dn355_reset (DEVICE * dptr);
static t_stat dn355_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat dn355_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static int dn355_iom_cmd (UNIT * unitp, pcw_t * pcwp);
static t_stat dn355_svc (UNIT *);
 
#define UNIT_FLAGS (UNIT_DISABLE)
static UNIT dn355_unit [N_DN355_UNITS_MAX] =
  {
    {UDATA (& dn355_svc, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL},
  };

#define DN355_UNIT_NUM(uptr) ((uptr) - dn355_unit)

static DEBTAB dn355_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

static MTAB dn355_mod [] =
  {
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      dn355_set_nunits, /* validation routine */
      dn355_show_nunits, /* display routine */
      "Number of DN355 units in the system", /* value descriptor */
      NULL // Help
    },
    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };


DEVICE dn355_dev =
  {
    "DN355",       /*  name */
    dn355_unit,    /* units */
    NULL,         /* registers */
    dn355_mod,     /* modifiers */
    N_DN355_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    dn355_reset,   /* reset */
    NULL,         /* boot */
    NULL,         /* attach */
    NULL,         /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    dn355_dt,      /* debug flag names */
    NULL,         /* memory size change */
    NULL,         /* logical name */
    NULL,         // help
    NULL,         // attach help
    NULL,         // attach context
    NULL          // description
  };

//++ static struct dn355_state
//++   {
//++     enum { no_mode, seek512_mode, read_mode } io_mode;
//++     uint seekPosition;
//++   } dn355_state [N_DN355_UNITS_MAX];

static struct
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  } cables_from_ioms_to_dn355 [N_DN355_UNITS_MAX];

static int findDN355Unit (int iom_unit_num, int chan_num, int dev_code)
  {
    for (int i = 0; i < N_DN355_UNITS_MAX; i ++)
      {
        if (iom_unit_num == cables_from_ioms_to_dn355 [i] . iom_unit_num &&
            chan_num     == cables_from_ioms_to_dn355 [i] . chan_num     &&
            dev_code     == cables_from_ioms_to_dn355 [i] . dev_code)
          return i;
      }
    return -1;
  }

/*
 * dn355_init()
 *
 */

// Once-only initialization

void dn355_init (void)
  {
    //memset (dn355_state, 0, sizeof (dn355_state));
    for (int i = 0; i < N_DN355_UNITS_MAX; i ++)
      cables_from_ioms_to_dn355 [i] . iom_unit_num = -1;
  }

static t_stat dn355_reset (DEVICE * dptr)
  {
    for (uint i = 0; i < dptr -> numunits; i ++)
      {
        // sim_dn355_reset (& dn355_unit [i]);
        sim_cancel (& dn355_unit [i]);
      }
    return SCPE_OK;
  }

t_stat cable_dn355 (int dn355_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (dn355_unit_num < 0 || dn355_unit_num >= (int) dn355_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_dn355: dn355_unit_num out of range <%d>\n", dn355_unit_num);
        sim_printf ("cable_dn355: dn355_unit_num out of range <%d>\n", dn355_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_dn355 [dn355_unit_num] . iom_unit_num != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_dn355: socket in use\n");
        sim_printf ("cable_dn355: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_DN355, chan_type_PSI, dn355_unit_num, & dn355_dev, & dn355_unit [dn355_unit_num], dn355_iom_cmd);
    if (rc)
      return rc;

    cables_from_ioms_to_dn355 [dn355_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_dn355 [dn355_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_dn355 [dn355_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static int dn355_cmd (UNIT * unitp, pcw_t * pcwp, bool * UNUSED disc)
  {
    int dn355_unit_num = DN355_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_dn355 [dn355_unit_num] . iom_unit_num;
//++     struct dn355_state * dn355_statep = & dn355_state [dn355_unit_num];
    word12 stati = 0;
    word6 rcount = 0;
    word12 residue = 0;
    word3 char_pos = 0;
    bool is_read = true;

//++     * disc = false;
//++ 
    int chan = pcwp-> chan;
//sim_printf ("dn355_cmd %o [%lld]\n", pcwp -> dev_cmd, sim_timell ());
    switch (pcwp -> dev_cmd)
      {
//++         case 000: // CMD 00 Request status
//++           {
//++             stati = 04000;
//++             dn355_statep -> io_mode = no_mode;
//++             sim_debug (DBG_NOTIFY, & dn355_dev, "Request status\n");
//++           }
//++           break;
//++ 
//++         case 022: // CMD 22 Read Status Resgister
//++           {
//++             sim_debug (DBG_NOTIFY, & dn355_dev, "Read Status Register\n");
//++             // Get the DDCW
//++             dcw_t dcw;
//++             int rc = iomListService (iom_unit_num, chan, & dcw, NULL);
//++ 
//++             if (rc)
//++               {
//++                 sim_printf ("list service failed\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ //sim_printf ("read  got type %d\n", dcw . type);
//++             if (dcw . type != ddcw)
//++               {
//++                 sim_printf ("not ddcw? %d\n", dcw . type);
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ 
//++             uint type = dcw.fields.ddcw.type;
//++             uint tally = dcw.fields.ddcw.tally;
//++             uint daddr = dcw.fields.ddcw.daddr;
//++             if (pcwp -> mask)
//++               daddr |= ((pcwp -> ext) & MASK6) << 18;
//++             // uint cp = dcw.fields.ddcw.cp;
//++ 
//++             if (type == 0) // IOTD
//++               * disc = true;
//++             else if (type == 1) // IOTP
//++               * disc = false;
//++             else
//++               {
//++ sim_printf ("uncomfortable with this\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ 
//++             if (tally != 4)
//++               {
//++                 sim_debug (DBG_ERR, &iom_dev, 
//++                   "%s: RSR expected tally of 4, is %d\n",
//++                    __func__, tally);
//++               }
//++ #if 0
//++             if (type == 3 && tally != 1)
//++               {
//++                 sim_debug (DBG_ERR, &iom_dev, "%s: Type is 3, but tally is %d\n",
//++                            __func__, tally);
//++               }
//++ #endif
//++             if (tally == 0)
//++               {
//++                 sim_debug (DBG_DEBUG, & iom_dev,
//++                            "%s: Tally of zero interpreted as 010000(4096)\n",
//++                            __func__);
//++                 tally = 4096;
//++               }
//++ 
//++ // XXX need status register data format 
//++             for (uint i = 0; i < tally; i ++)
//++               M [daddr + i] = 0;
//++ 
//++             stati = 04000;
//++           }
//++           break;
//++ 
//++         case 025: // CMD 25 READ
//++           {
//++             sim_debug (DBG_NOTIFY, & dn355_dev, "Read\n");
//++ //sim_printf ("dn355 read [%lld]\n", sim_timell ());
//++             // Get the DDCW
//++             dcw_t dcw;
//++             int rc = iomListService (iom_unit_num, chan, & dcw, NULL);
//++ 
//++             if (rc)
//++               {
//++                 sim_printf ("list service failed\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ //sim_printf ("read  got type %d\n", dcw . type);
//++             if (dcw . type != ddcw)
//++               {
//++                 sim_printf ("not ddcw? %d\n", dcw . type);
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ 
//++             uint type = dcw.fields.ddcw.type;
//++             uint tally = dcw.fields.ddcw.tally;
//++             uint daddr = dcw.fields.ddcw.daddr;
//++             if (pcwp -> mask)
//++               daddr |= ((pcwp -> ext) & MASK6) << 18;
//++             // uint cp = dcw.fields.ddcw.cp;
//++ 
//++             if (type == 0) // IOTD
//++               * disc = true;
//++             else if (type == 1) // IOTP
//++               * disc = false;
//++             else
//++               {
//++ sim_printf ("uncomfortable with this\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ #if 0
//++             if (type == 3 && tally != 1)
//++               {
//++                 sim_debug (DBG_ERR, &iom_dev, "%s: Type is 3, but tally is %d\n",
//++                            __func__, tally);
//++               }
//++ #endif
//++             if (tally == 0)
//++               {
//++                 sim_debug (DBG_DEBUG, & iom_dev,
//++                            "%s: Tally of zero interpreted as 010000(4096)\n",
//++                            __func__);
//++                 tally = 4096;
//++               }
//++ 
//++ //sim_printf ("tally %d\n", tally);
//++ 
//++             rc = fseek (unitp -> fileref, 
//++                         dn355_statep -> seekPosition * SECTOR_SZ_IN_BYTES,
//++                         SEEK_SET);
//++             if (rc)
//++               {
//++                 sim_printf ("fseek (read) returned %d, errno %d\n", rc, errno);
//++                 stati = 04202; // attn, seek incomplete
//++                 break;
//++               }
//++ 
//++             // Convert from word36 format to packed72 format
//++ 
//++             // round tally up to sector boundary
//++             
//++             // this math assumes tally is even.
//++            
//++             uint tallySectors = (tally + SECTOR_SZ_IN_W36 - 1) / 
//++                                 SECTOR_SZ_IN_W36;
//++             uint tallyWords = tallySectors * SECTOR_SZ_IN_W36;
//++             //uint tallyBytes = tallySectors * SECTOR_SZ_IN_BYTES;
//++             uint p72ByteCnt = (tallyWords * 36) / 8;
//++             uint8 buffer [p72ByteCnt];
//++             memset (buffer, 0, sizeof (buffer));
//++             rc = fread (buffer, SECTOR_SZ_IN_BYTES,
//++                         tallySectors,
//++                         unitp -> fileref);
//++ 
//++             if (rc == 0) // eof; reading a sector beyond the high water mark.
//++               {
//++                 // okay; buffer was zero, so just pretend that a zero filled
//++                 // sector was read (ala demand page zero)
//++               }
//++             else if (rc != (int) tallySectors)
//++               {
//++                 sim_printf ("read returned %d, errno %d\n", rc, errno);
//++                 stati = 04202; // attn, seek incomplete
//++                 break;
//++               }
//++ //sim_printf ("tallySectors %u\n", tallySectors);
//++ //sim_printf ("p72ByteCnt %u\n", p72ByteCnt);
//++ //for (uint i = 0; i < p72ByteCnt; i += 9)
//++ //{ word36 w1 = extr (& buffer [i / 9], 0, 36);
//++   //word36 w2 = extr (& buffer [i / 9], 36, 36);
//++   //sim_printf ("%5d %012llo %012llo\n", i * 2 / 9, w1, w2);
//++ //}
//++ //sim_printf ("read seekPosition %d\n", dn355_statep -> seekPosition);
//++ //sim_printf ("buffer 0...\n");
//++ //for (uint i = 0; i < 9; i ++) sim_printf (" %03o", buffer [i]);
//++ //sim_printf ("\n");
//++             dn355_statep -> seekPosition += tallySectors;
//++ 
//++             uint wordsProcessed = 0;
//++             for (uint i = 0; i < tally; i ++)
//++               extractWord36FromBuffer (buffer, p72ByteCnt, & wordsProcessed,
//++                                        & M [daddr + i]);
//++ //for (uint i = 0; i < tally; i ++) sim_printf ("%8o %012llo\n", daddr + i, M [daddr + i]);
//++             stati = 04000;
//++           }
//++           break;
//++ 
//++         case 030: // CMD 30 SEEK_512
//++           {
//++             sim_debug (DBG_NOTIFY, & dn355_dev, "Seek512\n");
//++ //sim_printf ("dn355 seek512 [%lld]\n", sim_timell ());
//++             // Get the DDCW
//++ 
//++             dcw_t dcw;
//++             int rc = iomListService (iom_unit_num, chan, & dcw, NULL);
//++ 
//++             if (rc)
//++               {
//++                 sim_printf ("list service failed\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ //sim_printf ("seek  got type %d\n", dcw . type);
//++             if (dcw . type != ddcw)
//++               {
//++                 sim_printf ("not ddcw? %d\n", dcw . type);
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ 
//++             uint type = dcw.fields.ddcw.type;
//++             uint tally = dcw.fields.ddcw.tally;
//++             uint daddr = dcw.fields.ddcw.daddr;
//++             if (pcwp -> mask)
//++               daddr |= ((pcwp -> ext) & MASK6) << 18;
//++             // uint cp = dcw.fields.ddcw.cp;
//++ 
//++             if (type == 0) // IOTD
//++               * disc = true;
//++             else if (type == 1) // IOTP
//++               * disc = false;
//++             else
//++               {
//++ sim_printf ("uncomfortable with this\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ #if 0
//++             if (type == 3 && tally != 1)
//++               {
//++                 sim_debug (DBG_ERR, &iom_dev, "%s: Type is 3, but tally is %d\n",
//++                            __func__, tally);
//++               }
//++ #endif
//++             if (tally == 0)
//++               {
//++                 sim_debug (DBG_DEBUG, & iom_dev,
//++                            "%s: Tally of zero interpreted as 010000(4096)\n",
//++                            __func__);
//++                 tally = 4096;
//++               }
//++ 
//++             // Seek specific processing
//++ 
//++             if (tally != 1)
//++               {
//++                 sim_printf ("dn355 seek dazed by tally %d != 1\n", tally);
//++                 stati = 04510; // Cmd reject, invalid inst. seq.
//++                 break;
//++               }
//++ 
//++             word36 seekData = M [daddr];
//++ //sim_printf ("seekData %012llo\n", seekData);
//++ // Observations about the seek/write stream
//++ // the stream is seek512 followed by a write 1024.
//++ // the seek data is:  000300nnnnnn
//++ // lets assume the 3 is a copy of the seek cmd # as a data integrity check.
//++ // highest observed n during vol. inoit. 272657(8) 95663(10)
//++ //
//++ 
//++ // dn355_control.pl1: 
//++ //   quentry.sector = bit (sector, 21);  /* Save the dn355 device address. */
//++ // suggests seeks are 21 bits.
//++ //  
//++             dn355_statep -> seekPosition = seekData & MASK21;
//++ //sim_printf ("seek seekPosition %d\n", dn355_statep -> seekPosition);
//++             stati = 00000; // Channel ready
//++           }
//++           break;
//++ 
//++         case 031: // CMD 31 WRITE
//++           {
//++             sim_debug (DBG_NOTIFY, & dn355_dev, "Write\n");
//++ //sim_printf ("dn355 write [%lld]\n", sim_timell ());
//++             // Get the DDCW
//++ 
//++             dcw_t dcw;
//++             int rc = iomListService (iom_unit_num, chan, & dcw, NULL);
//++ 
//++             if (rc)
//++               {
//++                 sim_printf ("list service failed\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ //sim_printf ("write got type %d\n", dcw . type);
//++             if (dcw . type != ddcw)
//++               {
//++                 sim_printf ("not ddcw? %d\n", dcw . type);
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ 
//++             uint type = dcw.fields.ddcw.type;
//++             uint tally = dcw.fields.ddcw.tally;
//++             uint daddr = dcw.fields.ddcw.daddr;
//++             if (pcwp -> mask)
//++               daddr |= ((pcwp -> ext) & MASK6) << 18;
//++             // uint cp = dcw.fields.ddcw.cp;
//++ 
//++             if (type == 0) // IOTD
//++               * disc = true;
//++             else if (type == 1) // IOTP
//++               * disc = false;
//++             else
//++               {
//++ sim_printf ("uncomfortable with this\n");
//++                 stati = 05001; // BUG: arbitrary error code; config switch
//++                 break;
//++               }
//++ #if 0
//++             if (type == 3 && tally != 1)
//++               {
//++                 sim_debug (DBG_ERR, &iom_dev, "%s: Type is 3, but tally is %d\n",
//++                            __func__, tally);
//++               }
//++ #endif
//++             if (tally == 0)
//++               {
//++                 sim_debug (DBG_DEBUG, & iom_dev,
//++                            "%s: Tally of zero interpreted as 010000(4096)\n",
//++                            __func__);
//++                 tally = 4096;
//++               }
//++ 
//++ //sim_printf ("tally %d\n", tally);
//++ 
//++             rc = fseek (unitp -> fileref, 
//++                         dn355_statep -> seekPosition * SECTOR_SZ_IN_BYTES,
//++                         SEEK_SET);
//++ //sim_printf ("write seekPosition %d\n", dn355_statep -> seekPosition);
//++             if (rc)
//++               {
//++                 sim_printf ("fseek (write) returned %d, errno %d\n", rc, errno);
//++                 stati = 04202; // attn, seek incomplete
//++                 break;
//++               }
//++ 
//++             // Convert from word36 format to packed72 format
//++ 
//++             // round tally up to sector boundary
//++             
//++             // this math assumes tally is even.
//++            
//++             uint tallySectors = (tally + SECTOR_SZ_IN_W36 - 1) / 
//++                                 SECTOR_SZ_IN_W36;
//++             uint tallyWords = tallySectors * SECTOR_SZ_IN_W36;
//++             //uint tallyBytes = tallySectors * SECTOR_SZ_IN_BYTES;
//++             uint p72ByteCnt = (tallyWords * 36) / 8;
//++             uint8 buffer [p72ByteCnt];
//++             memset (buffer, 0, sizeof (buffer));
//++             uint wordsProcessed = 0;
//++             for (uint i = 0; i < tally; i ++)
//++               insertWord36toBuffer (buffer, p72ByteCnt, & wordsProcessed,
//++                                     M [daddr + i]);
//++ 
//++             rc = fwrite (buffer, SECTOR_SZ_IN_BYTES,
//++                          tallySectors,
//++                          unitp -> fileref);
//++                        
//++             if (rc != (int) tallySectors)
//++               {
//++                 sim_printf ("fwrite returned %d, errno %d\n", rc, errno);
//++                 stati = 04202; // attn, seek incomplete
//++                 break;
//++               }
//++ 
//++             dn355_statep -> seekPosition += tallySectors;
//++ 
//++             stati = 04000;
//++           }
//++ //exit(1);
//++           break;
//++ 
        default:
          {
sim_printf ("dn355 daze %o\n", pcwp -> dev_cmd);
            stati = 04501; // cmd reject, invalid opcode
          }
          break;

      }
    status_service (iom_unit_num, chan, pcwp -> dev_code, stati, rcount, residue, char_pos, is_read);

    return 0;
  }

static int dn355_iom_cmd (UNIT * unitp, pcw_t * pcwp)
  {
    int dn355_unit_num = DN355_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_dn355 [dn355_unit_num] . iom_unit_num;

    // First, execute the command in the PCW, and then walk the 
    // payload channel mbx looking for IDCWs.

    // uint chanloc = mbx_loc (iom_unit_num, pcwp -> chan);
    //lpw_t lpw;
    //fetch_and_parse_lpw (& lpw, chanloc, false);

// Ignore a CMD 051 in the PCW
    if (pcwp -> dev_cmd == 051)
      return 1;
    bool disc;
    dn355_cmd (unitp, pcwp, & disc);

    // ctrl of the pcw is observed to be 0 even when there are idcws in the
    // list so ignore that and force it to 2.
    //uint ctrl = pcwp -> control;
    uint ctrl = 2;
    if (disc)
      ctrl = 0;
//sim_printf ("starting list; disc %d, ctrl %d\n", disc, ctrl);

    // It looks like the dn355 controller ignores IOTD and olny obeys ctrl...
    //while ((! disc) && ctrl == 2)
    int ptro = 0;
#ifdef PTRO
    while (ctrl == 2 && ! ptro)
#else
    while (ctrl == 2)
#endif
      {
//sim_printf ("perusing channel mbx lpw....\n");
        dcw_t dcw;
        int rc = iomListService (iom_unit_num, pcwp -> chan, & dcw, & ptro);
        if (rc)
          {
//sim_printf ("list service denies!\n");
            break;
          }
//sim_printf ("persuing got type %d\n", dcw . type);
        if (dcw . type != idcw)
          {
// 04501 : COMMAND REJECTED, invalid command
            status_service (iom_unit_num, pcwp -> chan, dcw . fields . instr. dev_code, 04501, 0, 0, 0, true);
            break;
          }

// The dcw does not necessarily have the same dev_code as the pcw....

        dn355_unit_num = findDN355Unit (iom_unit_num, pcwp -> chan, dcw . fields . instr. dev_code);
        if (dn355_unit_num < 0)
          {
// 04502 : COMMAND REJECTED, invalid device code
            status_service (iom_unit_num, pcwp -> chan, dcw . fields . instr. dev_code, 04502, 0, 0, 0, true);
            break;
          }
        unitp = & dn355_unit [dn355_unit_num];
        dn355_cmd (unitp, & dcw . fields . instr, & disc);
        ctrl = dcw . fields . instr . control;
      }
//sim_printf ("dn355 interrupts\n");
    send_terminate_interrupt (iom_unit_num, pcwp -> chan);

    return 1;
  }

static t_stat dn355_svc (UNIT * unitp)
  {
    int dn355UnitNum = DN355_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_dn355 [dn355UnitNum] . iom_unit_num;
    int chanNum = cables_from_ioms_to_dn355 [dn355UnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    dn355_iom_cmd (unitp, pcwp);
    return SCPE_OK;
  }

static t_stat dn355_show_nunits (FILE * UNUSED st, UNIT * UNUSED uptr, int UNUSED val, void * UNUSED desc)
  {
    sim_printf("Number of DN355 units in system is %d\n", dn355_dev . numunits);
    return SCPE_OK;
  }

static t_stat dn355_set_nunits (UNIT * UNUSED uptr, int32 UNUSED value, char * cptr, void * UNUSED desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_DN355_UNITS_MAX)
      return SCPE_ARG;
    dn355_dev . numunits = n;
    return SCPE_OK;
  }

