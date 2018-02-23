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
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_cable.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"

#include "udplib.h"

#define DBG_CTR 1

#define N_ABSI_UNITS 1 // default

static t_stat absi_reset (DEVICE * dptr);
static t_stat absi_show_nunits (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat absi_set_nunits (UNIT * uptr, int32 value, const char * cptr, void * desc);
static t_stat absiAttach (UNIT *uptr, const char *cptr);
static t_stat absiDetach (UNIT *uptr);

#define UNIT_FLAGS ( UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | \
                     UNIT_IDLE )
UNIT absi_unit[N_ABSI_UNITS_MAX] =
  {
    {UDATA (NULL, UNIT_FLAGS, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL}
  };

#define ABSIUNIT_NUM(uptr) ((uptr) - absi_unit)

static DEBTAB absi_dt[] =
  {
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

#define UNIT_WATCH UNIT_V_UF

static MTAB absi_mod[] =
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
    NULL,         // description
    NULL
};

static struct absi_state
  {
    int link;
  } absi_state[N_ABSI_UNITS_MAX];

/*
 * absi_init()
 *
 */

// Once-only initialization

void absi_init (void)
  {
    memset (absi_state, 0, sizeof (absi_state));
    for (int i = 0; i < N_ABSI_UNITS_MAX; i ++)
      absi_state[i].link = NOLINK;
  }

static t_stat absi_reset (UNUSED DEVICE * dptr)
  {
    //absiResetRX (0);
    //absiResetTX (0);
    return SCPE_OK;
  }

static int absi_cmd (uint iomUnitIdx, uint chan)
  {
    iom_chan_data_t * p = &iom_chan_data[iomUnitIdx][chan];
// sim_printf ("absi_cmd CHAN_CMD %o DEV_CODE %o DEV_CMD %o COUNT %o\n", 
//p->IDCW_CHAN_CMD, p->IDCW_DEV_CODE, p->IDCW_DEV_CMD, p->IDCW_COUNT);
    sim_debug (DBG_TRACE, & absi_dev, 
               "absi_cmd CHAN_CMD %o DEV_CODE %o DEV_CMD %o COUNT %o\n", 
               p->IDCW_CHAN_CMD, p->IDCW_DEV_CODE, p->IDCW_DEV_CMD, 
               p->IDCW_COUNT);


    switch (p->IDCW_DEV_CMD)
      {
        case 000: // CMD 00 Request status
          {
            p->stati = 04000;
sim_printf ("absi request status\n");
          }
          break;

        case 001: // CMD 01 Read
          {
            p->stati = 04000;
sim_printf ("absi read\n");
          }
          break;

        case 011: // CMD 11 Write
          {
            p->stati = 04000;
sim_printf ("absi write\n");
          }
          break;

        case 020: // CMD 20 Host switch down
          {
            p->stati = 04000;
sim_printf ("absi host switch down\n");
          }
          break;

        case 040: // CMD 40 Reset status
          {
            p->stati = 04000;
sim_printf ("absi reset status\n");
          }
          break;

        case 060: // CMD 60 Host switch up
          {
            p->stati = 04000;
sim_printf ("absi host switch up\n");
          }
          break;

        default:
          {
            sim_warn ("absi daze %o\n", p->IDCW_DEV_CMD);
            p->stati = 04501; // cmd reject, invalid opcode
            p->chanStatus = chanStatIncorrectDCW;
          }
          break;
      }

    if (p->IDCW_CONTROL == 3) // marker bit set
      {
sim_printf ("absi marker\n");
        send_marker_interrupt (iomUnitIdx, (int) chan);
      }

    if (p->IDCW_CHAN_CMD == 0)
      return 2; // don't do DCW list
    return 0;
  }

// 1 ignored command
// 0 ok
// -1 problem
int absi_iom_cmd (uint iomUnitIdx, uint chan)
  {
    iom_chan_data_t * p = & iom_chan_data[iomUnitIdx][chan];
// Is it an IDCW?

    if (p->DCW_18_20_CP == 7)
      {
        return absi_cmd (iomUnitIdx, chan);
      }
    sim_printf ("%s expected IDCW\n", __func__);
    return -1;
  }

void absiProcessEvent (void)
  {
#define psz 17000
    uint16_t pkt[psz];
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
                printf ("  %06o  %04x  ", pkt[i], pkt[i]);
                for (int b = 0; b < 16; b ++)
                  printf ("%c", pkt[i] & (1 << (16 - b)) ? '1' : '0');
                printf ("\n");
              }
            // Send a NOP reply
            //int16_t reply[2] = 0x0040
            int rc = udp_send (absi_state[unit].link, pkt, (uint16_t) sz, 
                               PFLG_FINAL);
            if (rc < 0)
              {
                printf ("udp_send failed\n");
              }
          }
      }
  }

static t_stat absi_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED const void * desc)
  {
    sim_printf ("Number of ABSIunits in system is %d\n", absi_dev.numunits);
    return SCPE_OK;
  }

static t_stat absi_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_ABSI_UNITS_MAX)
      return SCPE_ARG;
    absi_dev.numunits = (uint32) n;
    return SCPE_OK;
  }

t_stat absiAttach (UNIT * uptr, const char * cptr)
  {
    int unitno = (int) (uptr - absi_unit);

    //    ATTACH HIn llll:w.x.y.z:rrrr - connect via UDP to a remote simh host

    t_stat ret;
    char * pfn;
    //uint16 imp = 0; // we only support a single attachment to a single IMP

    // If we're already attached, then detach ...
    if ((uptr->flags & UNIT_ATT) != 0)
      detach_unit (uptr);

    // Make a copy of the "file name" argument.  udp_create() actually modifies
    // the string buffer we give it, so we make a copy now so we'll have
    // something to display in the "SHOW HIn ..." command.
    pfn = (char *) calloc (CBUFSIZE, sizeof (char));
    if (pfn == NULL)
      return SCPE_MEM;
    strncpy (pfn, cptr, CBUFSIZE);

    // Create the UDP connection.
    ret = udp_create (cptr, & absi_state[unitno].link);
    if (ret != SCPE_OK)
      {
        free (pfn);
        return ret;
      }

    uptr->flags |= UNIT_ATT;
    uptr->filename = pfn;
    return SCPE_OK;
  }

// Detach (connect) ...
t_stat absiDetach (UNIT * uptr)
  {
    int unitno = (int) (uptr - absi_unit);
    t_stat ret;
    if ((uptr->flags & UNIT_ATT) == 0)
      return SCPE_OK;
    if (absi_state[unitno].link == NOLINK)
      return SCPE_OK;

    ret = udp_release (absi_state[unitno].link);
    if (ret != SCPE_OK)
      return ret;
    absi_state[unitno].link = NOLINK;
    uptr->flags &= ~ (unsigned int) UNIT_ATT;
    free (uptr->filename);
    uptr->filename = NULL;
    return SCPE_OK;
  }




