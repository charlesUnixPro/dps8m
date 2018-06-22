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
//  dps8_console.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#ifndef __MINGW64__
#include <termios.h>
#endif
#include <ctype.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_console.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_cable.h"
#include "dps8_cpu.h"
#include "dps8_mt.h"  // attachTape
#include "dps8_disk.h"  // attachDisk
#include "dps8_utils.h"
#if defined(THREADZ) || defined(LOCKLESS)
#include "threadz.h"
#endif

#include "libtelnet.h"

#define DBG_CTR 1

#define ASSUME0 0

#define N_OPC_UNITS 1 // default

// config switch -- The bootload console has a 30-second timer mechanism. When
// reading from the console, if no character is typed within 30 seconds, the
// read operation is terminated. The timer is controlled by an enable switch,
// must be set to enabled during Multics and BCE 

static t_stat opc_reset (DEVICE * dptr);
static t_stat opc_show_nunits (FILE *st, UNIT *uptr, int val,
                                 const void *desc);
static t_stat opc_set_nunits (UNIT * uptr, int32 value, const char * cptr,
                                void * desc);
static t_stat opc_autoinput_set (UNIT *uptr, int32 val, const char *cptr,
                                void *desc);
static t_stat opc_autoinput_show (FILE *st, UNIT *uptr, int val,
                                 const void *desc);
static t_stat opc_set_config (UNUSED UNIT *  uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc);
static t_stat opc_show_config (UNUSED FILE * st, UNUSED UNIT * uptr,
                               UNUSED int  val, UNUSED const void * desc);

static MTAB opc_mtab[] =
  {
    {
       MTAB_XTD | MTAB_VUN | MTAB_NC, /* mask */
       0,            /* match */
       "AUTOINPUT",  /* print string */
       "AUTOINPUT",  /* match pstring */
       opc_autoinput_set, 
       opc_autoinput_show, 
       NULL, 
       NULL
    },

    {
      MTAB_XTD | MTAB_VDV | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      opc_set_nunits, /* validation routine */
      opc_show_nunits, /* display routine */
      "Number of OPC units in the system", /* value descriptor */
      NULL // Help
    },

    {
      MTAB_XTD | MTAB_VUN /* | MTAB_VALR */, /* mask */
      0,            /* match */
      (char *) "CONFIG",     /* print string */
      (char *) "CONFIG",         /* match string */
      opc_set_config,         /* validation routine */
      opc_show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL,            /* help */
    },

    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
};


static DEBTAB opc_dt[] =
  {
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

// Multics only supports a single operator console; but 
// it is possible to run multiple Multics instances in a
// cluster.

#define N_OPC_UNITS 1 // default

#define OPC_UNIT_NUM(uptr) ((uptr) - opc_unit)


// sim_activate counts in instructions, is dependent on the execution
// model
#if defined(THREADZ) || defined(LOCKLESS)
// 1K ~= 1 sec
#define ACTIVATE_1SEC 1000
#else
// 4M ~= 1 sec
#define ACTIVATE_1SEC 4000000
#endif

static t_stat opc_svc (UNIT * unitp);

UNIT opc_unit[N_OPC_UNITS_MAX] =
  {
    [0 ... N_OPC_UNITS_MAX - 1] =
      {
        UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL
      }
  };

#define OPC_UNIT_NUM(uptr) ((uptr) - opc_unit)

DEVICE opc_dev =
  {
    "OPC",         /* name */
    opc_unit,      /* units */
    NULL,          /* registers */
    opc_mtab,      /* modifiers */
    N_OPC_UNITS,   /* #units */
    10,            /* address radix */
    8,             /* address width */
    1,             /* address increment */
    8,             /* address width */
    8,             /* data width */
    NULL,          /* examine routine */
    NULL,          /* deposit routine */
    opc_reset,     /* reset routine */
    NULL,          /* boot routine */
    NULL,          /* attach routine */
    NULL,          /* detach routine */
    NULL,          /* context */
    DEV_DEBUG,     /* flags */
    0,             /* debug control flags */
    opc_dt,        /* debug flag names */
    NULL,          /* memory size change */
    NULL,          /* logical name */
    NULL,          // help
    NULL,          // attach help
    NULL,          // help context
    NULL,          // description
    NULL
  };

// Hangs off the device structure
typedef struct opc_state_t
  {
    enum console_mode { opc_no_mode, opc_read_mode, opc_write_mode } io_mode;
    // SIMH console library has only putc and getc; the SIMH terminal
    // library has more features including line buffering.
#define bufsize 81
    unsigned char buf[bufsize];
    unsigned char *tailp;
    unsigned char *readp;
    unsigned char *auto_input;
    unsigned char *autop;
#ifdef ATTN_HACK
    bool once_per_boot;
    int attn_hack;
#endif
    
    // stuff saved from the Read ASCII command
    time_t startTime;

    int autoaccept;

    bool attn_pressed;
    bool simh_attn_pressed;
#define simh_buffer_sz 4096
    char simh_buffer[simh_buffer_sz];
    int simh_buffer_cnt;

    uv_access console_access;

 } opc_state_t;

static opc_state_t console_state[N_OPC_UNITS_MAX];

//
// Typeahead buffer
//

#ifndef TA_BUFFER_SIZE
#define TA_BUFFER_SIZE 4096
#endif

static int ta_buffer[TA_BUFFER_SIZE];
static uint ta_cnt = 0;
static uint ta_next = 0;
static bool ta_ovf = false;

static void ta_flush (void)
  {
    ta_cnt = ta_next = 0;
    ta_ovf = false;
  }

static void ta_push (int c)
  {
    // discard overflow
    if (ta_cnt >= TA_BUFFER_SIZE)
      {
        if (! ta_ovf)
          sim_print ("typeahead buffer overflow");
        ta_ovf = true;
        return;
      }
    ta_buffer [ta_cnt ++] = c;
  }

static int ta_peek (void)
  {
    if (ta_next >= ta_cnt)
      return SCPE_OK;
    int c = ta_buffer[ta_next];
    return c;
  }

static int ta_get (void)
  {
    if (ta_next >= ta_cnt)
      return SCPE_OK;
    int c = ta_buffer[ta_next ++];
    if (ta_next >= ta_cnt)
      ta_flush ();
    return c;
  }

#if 0
static bool ta_scan (int c)
  {
    for (uint i = ta_next; i < ta_cnt; i ++)
      if (c == ta_buffer[i])
        return true;
    return false;
  }
#endif

static t_stat opc_reset (UNUSED DEVICE * dptr)
  {
    for (uint i = 0; i < dptr->numunits; i ++)
      {
        console_state[i].io_mode = opc_no_mode;
        console_state[i].tailp = console_state[i].buf;
        console_state[i].readp = console_state[i].buf;
#ifdef ATTN_HACK
        console_state[i].once_per_boot = false;
#endif
      }
    return SCPE_OK;
  }


int check_attn_key (void)
  {
    for (uint i = 0; i < opc_dev.numunits; i ++)
      {
        opc_state_t * csp = console_state + i;
        if (csp->attn_pressed)
          {
             csp->attn_pressed = false;
             return (int) i;
          }
      }
    return -1;
  }

// Once-only initialation

void console_init (void)
  {
    opc_reset (& opc_dev);
    for (uint i = 0; i < opc_dev.numunits; i ++)
      {
        opc_state_t * csp = console_state + i;
        csp->auto_input = NULL;
        csp->autop = NULL;
#ifdef ATTN_HACK
        csp->attn_hack = 0;
#endif
        csp->attn_pressed = false;
        csp->simh_attn_pressed = false;
        csp->simh_buffer_cnt = 0;
        strcpy (csp->console_access.pw, "MulticsRulez");
      }
}

static int opc_autoinput_set (UNIT * uptr, UNUSED int32 val,
                                const char *  cptr, UNUSED void * desc)
  {
    int devUnitIdx = (int) OPC_UNIT_NUM (uptr);
    opc_state_t * csp = console_state + devUnitIdx;

    if (cptr)
      {
        unsigned char * new = (unsigned char *) strdupesc (cptr);
        if (csp-> auto_input)
          {
            size_t nl = strlen ((char *) new);
            size_t ol = strlen ((char *) csp->auto_input);

            unsigned char * old = realloc (csp->auto_input, nl + ol + 1);
            strcpy ((char *) old + ol, (char *) new);
            csp->auto_input = old;
            free (new);
          }
        else
          csp->auto_input = new;
        sim_debug (DBG_NOTIFY, & opc_dev,
                   "%s: Auto-input now: %s\n", __func__, cptr);
      }
    else
      {
        if (csp->auto_input)
          free (csp->auto_input);
        csp->auto_input = NULL;
        sim_debug (DBG_NOTIFY, & opc_dev, 
                   "%s: Auto-input disabled.\n", __func__);
      }
    csp->autop = csp->auto_input;
    return SCPE_OK;
  }

int clear_opc_autoinput (int32 flag, UNUSED const char * cptr)
  {
    opc_state_t * csp = console_state + flag;
    if (csp->auto_input)
      free (csp->auto_input);
    csp->auto_input = NULL;
    sim_debug (DBG_NOTIFY, & opc_dev, "%s: Auto-input disabled.\n", __func__);
    csp->autop = csp->auto_input;
    return SCPE_OK;
  }

int add_opc_autoinput (int32 flag, const char * cptr)
  {
    opc_state_t * csp = console_state + flag;
    unsigned char * new = (unsigned char *) strdupesc (cptr);
    if (csp->auto_input)
      {
        size_t nl = strlen ((char *) new);
        size_t ol = strlen ((char *) csp->auto_input);

        unsigned char * old = realloc (csp->auto_input, nl + ol + 1);
        strcpy ((char *) old + ol, (char *) new);
        csp->auto_input = old;
        free (new);
      }
    else
      csp->auto_input = new;
    sim_debug (DBG_NOTIFY, & opc_dev,
               "%s: Auto-input now: %s\n", __func__, cptr);
    csp->autop = csp->auto_input;
    return SCPE_OK;
  }

static int opc_autoinput_show (UNUSED FILE * st, UNIT * uptr, 
                                 UNUSED int val, UNUSED const void * desc)
  {
    int conUnitIdx = (int) OPC_UNIT_NUM (uptr);
    opc_state_t * csp = console_state + conUnitIdx;
    sim_debug (DBG_NOTIFY, & opc_dev,
               "%s: FILE=%p, uptr=%p, val=%d,desc=%p\n",
               __func__, (void *) st, (void *) uptr, val, desc);

    if (csp->auto_input)
      sim_print ("Autoinput: '%s'\n", csp->auto_input); 
    else
      sim_print ("Autoinput: NULL\n");
    return SCPE_OK;
  }
 
static t_stat console_attn (UNUSED UNIT * uptr);

static UNIT attn_unit[N_OPC_UNITS_MAX] = 
  {
    [0 ... N_OPC_UNITS_MAX - 1] =
      {
        UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL
      }
  };

static t_stat console_attn (UNUSED UNIT * uptr)
  {
    uint con_unit_idx = (uint) (uptr - attn_unit);
    uint ctlr_port_num = 0; // Consoles are single ported
    uint iom_unit_idx = cables->opc_to_iom[con_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->opc_to_iom[con_unit_idx][ctlr_port_num].chan_num;
    uint dev_code = 0; // Only a single console on the controller

    send_special_interrupt (iom_unit_idx, chan_num, dev_code, 0, 0);
    return SCPE_OK;
  }

void console_attn_idx (int conUnitIdx)
  {
    console_attn (attn_unit + conUnitIdx);
  }

#ifndef __MINGW64__
static struct termios ttyTermios;
static bool ttyTermiosOk = false;

static void newlineOff (void)
  {
    if (! isatty (0))
      return;
    if (! ttyTermiosOk)
      {
        int rc = tcgetattr (0, & ttyTermios); /* get old flags */
        if (rc)
           return;
        ttyTermiosOk = true;
      }
    struct termios runtty;
    runtty = ttyTermios;
    runtty.c_oflag &= (unsigned int) ~OPOST; /* no output edit */
    tcsetattr (0, TCSAFLUSH, & runtty);
  }

static void newlineOn (void)
  {
    if (! isatty (0))
      return;
    if (! ttyTermiosOk)
      return;
    tcsetattr (0, TCSAFLUSH, & ttyTermios);
  }
#endif

static void handleRCP (char * text)
  {
    size_t len = strlen (text);
    char label [len + 1];
    char with [len + 1];
    char drive [len + 1];
    int rc = sscanf (text, "%*d.%*d RCP: Mount Reel %s %s ring on %s",
                label, with, drive);
    if (rc == 3)
      {
        bool withring = (strcmp (with, "with") == 0);
        char labelDotTap[strlen (label) + strlen (".tap") + 1];
        strcpy (labelDotTap, label);
        strcat (labelDotTap, ".tap");
        attachTape (labelDotTap, withring, drive);
        return;
      }

    rc = sscanf (text, "%*d.%*d RCP: Remount Reel %s %s ring on %s",
                label, with, drive);
    if (rc == 3)
      {
        bool withring = (strcmp (with, "with") == 0);
        char labelDotTap [strlen (label) + strlen (".tap") + 1];
        strcpy (labelDotTap, label);
        strcat (labelDotTap, ".tap");
        attachTape (labelDotTap, withring, drive);
        return;
      }

//  1629  as   dial_ctl_: Channel d.h000 dialed to Initializer

    if (console_state[ASSUME0].autoaccept)
      {
        rc = sscanf (text, "%*d  as   dial_ctl_: Channel %s dialed to Initializer",
                     label);
        if (rc == 1)
          {
            //sim_printf (" dial system <%s>\r\n", label);
            opc_autoinput_set (opc_unit + ASSUME0, 0, "accept ", NULL);
            opc_autoinput_set (opc_unit + ASSUME0, 0, label, NULL);
            opc_autoinput_set (opc_unit + ASSUME0, 0, "\r", NULL);
// XXX This is subject to race conditions
            if (console_state[ASSUME0].io_mode != opc_read_mode)
              console_state[ASSUME0].attn_pressed = true;
            return;
          }
      }

// Just because RCP has detached the drive, it doesn't mean that 
// it doesn't remeber what tape is on there, and expects to be there

#if 0
// 1236.8  RCP: Force Detached tapa_01 from Anthony.SysEng.a

    rc = sscanf (text, "%*d.%*d RCP: Force Detached %s",
                drive);
    if (rc == 1)
      {
        detachTape (drive);
        return;
      }


    rc = sscanf (text, "%*d.%*d RCP: Detached %s",
                drive);
    if (rc == 1)
      {
        detachTape (drive);
        return;
      }
#endif

#if 0
    rc = sscanf (text, "%*d.%*d RCP: Mount logical volume %s for %s",
                label, whom);
    if (rc == 2)
      {
        char labelDotDsk[strlen (label) + 4];
        strcpy (labelDotDsk, label);
        strcat (labelDotDsk, ".dsk");
        attachDisk (labelDotDsk);
      }
#endif
  }

#ifdef OSCAR
static void oscar (char * text)
  {
    char prefix[] = "log oscar ";
    if (strncmp (text, prefix, strlen (prefix)))
      return;
    //do_cmd (0, text + strlen (prefix));
    char * cptr = text + strlen (prefix);
    char gbuf[257];
    cptr = (char *) get_glyph (cptr, gbuf, 0);         /* get command glyph */
    if (strlen (gbuf) == 0)
      return;
    CTAB *cmdp;
    if ((cmdp = find_cmd (gbuf)))                       /* lookup command */
      {
        t_stat stat = cmdp->action (cmdp->arg, cptr);  /* if found, exec */
        if (stat == SCPE_OK)
          sim_msg ("oscar thinks that's ok.\n");
        else
          sim_warn ("oscar thinks %d\n", stat);
      }
    else
      sim_msg ("oscar says huh?\n");
  }
#endif

static void sendConsole (int conUnitIdx, word12 stati)
  {
    opc_state_t * csp = console_state + conUnitIdx;
    uint ctlr_port_num = 0; // Consoles are single ported
    uint iomUnitIdx = cables->opc_to_iom[conUnitIdx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->opc_to_iom[conUnitIdx][ctlr_port_num].chan_num;
    iom_chan_data_t * p = & iom_chan_data[iomUnitIdx][chan_num];
    uint tally = p->DDCW_TALLY;

    ASSURE (csp->io_mode == opc_read_mode);

#ifdef OSCAR
    char text[257];
    int ntext;
    for (ntext = 0; ntext < 256; ntext ++)
      {
        if (csp->readp + ntext >= csp->tailp)
          break;
        text[ntext] = (char) (* (csp->readp + ntext));
      }
    text[ntext] = 0;
    oscar (text);
#endif
    uint n_chars = (uint) (csp->tailp - csp->readp);
    uint n_words = (n_chars + 3) / 4;
    word36 buf[n_words];
    word36 * bufp = buf;

    while (tally && csp->readp < csp->tailp)
      {
        uint charno;

	* bufp = 0ul;
        for (charno = 0; charno < 4; ++ charno)
          {
            if (csp->readp >= csp->tailp)
              break;
            unsigned char c = (unsigned char) (* csp->readp ++);
            putbits36_9 (bufp, charno * 9, c);
          }

	bufp ++;
        tally --;
      }
    if (csp->readp < csp->tailp)
      {
        sim_debug (DBG_WARN, & opc_dev,
                   "opc_iom_io: discarding %d characters from end of line\n",
                    (int) (csp->tailp - csp->readp));
      }

    iom_indirect_data_service (iomUnitIdx, chan_num, buf, & n_words, true);

    p->charPos = n_chars % 4;
    p->stati = (word12) stati;

    csp->readp = csp->buf;
    csp->tailp = csp->buf;
    csp->io_mode = opc_no_mode;

    send_terminate_interrupt (iomUnitIdx, chan_num);
  }


static void console_putchar (int conUnitIdx, char ch);
static void console_putstr (int conUnitIdx, char * str);

static int opc_cmd (uint iomUnitIdx, uint chan)
  {
    iom_chan_data_t * p = & iom_chan_data[iomUnitIdx][chan];
    uint con_unit_idx = get_ctlr_idx (iomUnitIdx, chan);
    opc_state_t * csp = console_state + con_unit_idx;


    ASSURE(p -> DCW_18_20_CP == 07); // IDCW

    if (p->PCW_63_PTP)
      {
        sim_warn ("PTP in console\n");
        return IOM_CMD_ERROR;
      }

    p->dev_code = p->IDCW_DEV_CODE;
    p->stati = 0;

    switch (p->IDCW_DEV_CMD)
      {
        case 0: // CMD 00 Request status
          {
            sim_debug (DBG_NOTIFY, & opc_dev,
                       "%s: Status request cmd received",
                       __func__);
            p->stati = 04000;
          }
          break;

        case 023:               // Read ASCII
          {
            csp->io_mode = opc_read_mode;
            sim_debug (DBG_NOTIFY, & opc_dev, 
                       "%s: Read ASCII command received\n", __func__);
            if (csp->tailp != csp->buf)
              {
                sim_debug (DBG_WARN, & opc_dev,
                           "%s: Discarding previously buffered input.\n",
                           __func__);
              }
            // TODO: discard any buffered chars from SIMH?
            csp->tailp = csp->buf;
            csp->readp = csp->buf;

            // Get the DDCW

            bool ptro, send, uff;

            // We only expect one DCW, so no loop
            int rc = iom_list_service (iomUnitIdx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                sim_warn ("console read list service failed\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncomplete;
                return IOM_CMD_ERROR;
              }
            if (uff)
              {
                sim_warn ("console read ignoring uff\n"); // XXX
              }
            if (! send)
              {
                sim_warn ("console read nothing to send\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncomplete;
                return  IOM_CMD_ERROR;
              }
            if (p->DCW_18_20_CP == 07 || p->DDCW_22_23_TYPE == 2)
              {
                sim_warn ("console read expected DDCW\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncorrectDCW;
                return IOM_CMD_ERROR;
              }

            if (rc)
              {
                sim_warn ("list service failed\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncomplete;
                return IOM_CMD_ERROR;
              }

            if (p->DDCW_22_23_TYPE != 0 &&
                p->DDCW_22_23_TYPE != 1) //IOTD, IOTP
              {
sim_warn ("uncomfortable with this\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncorrectDCW;
                return IOM_CMD_ERROR;
              }

            uint tally = p->DDCW_TALLY;

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }
            csp->startTime = time (NULL);

          }
          //break;
          return IOM_CMD_PENDING; // command in progress; do not send terminate interrupt


        case 033:               // Write ASCII
          {
            p->isRead = false;
            csp->io_mode = opc_write_mode;

            sim_debug (DBG_NOTIFY, & opc_dev,
                       "%s: Write ASCII cmd received\n", __func__);
            if (csp->tailp != csp->buf)
              {
                sim_debug (DBG_WARN, & opc_dev,
                           "opc_iom_cmd: Might be discarding previously "
                           "buffered input.\n");
              }

            // Get the DDCWs

            bool ptro;
            bool send;
            bool uff;
            do
              {
                int rc = iom_list_service (iomUnitIdx, chan, & ptro, & send,
                                         & uff);
                if (rc < 0)
                  {
                    sim_warn ("console write list service failed\n");
                    p->stati = 05001; // BUG: arbitrary error code; 
                                        //config switch
                    p->chanStatus = chanStatIncomplete;
                    return IOM_CMD_ERROR;
                  }
                if (uff)
                  {
                    sim_warn ("console write ignoring uff\n"); // XXX
                  }
                if (! send)
                  {
                    sim_warn ("console write nothing to send\n");
                    p->stati = 05001; // BUG: arbitrary error code; 
                                        //config switch
                    p->chanStatus = chanStatIncomplete;
                    return  IOM_CMD_ERROR;
                  }
                if (p->DCW_18_20_CP == 07 || p->DDCW_22_23_TYPE == 2)
                  {
                    sim_warn ("console write expected DDCW\n");
                    p->stati = 05001; // BUG: arbitrary error code; 
                                        //config switch
                    p->chanStatus = chanStatIncorrectDCW;
                    return IOM_CMD_ERROR;
                  }

                if (rc)
                  {
                    sim_warn ("list service failed\n");
                    p->stati = 05001; // BUG: arbitrary error code;
                                        // config switch
                    p->chanStatus = chanStatIncomplete;
                    return IOM_CMD_ERROR;
                  }

                uint tally = p->DDCW_TALLY;
#ifdef ATTN_HACK
                uint daddr = p->DDCW_ADDR;
#endif

// We would hope that number of valid characters in the last word
// would be in DCW_18_20_CP, but it seems to reliably be zero.

                if (p->DDCW_22_23_TYPE != 0 &&
                    p->DDCW_22_23_TYPE != 1) //IOTD, IOTP
                  {
sim_warn ("uncomfortable with this\n");
                    // BUG: arbitrary error code; config switch
                    p->stati = 05001;
                    p->chanStatus = chanStatIncorrectDCW;
                    return IOM_CMD_ERROR;
                  }

                if (tally == 0)
                  {
                    sim_debug (DBG_DEBUG, & iom_dev,
                               "%s: Tally of zero interpreted as "
                               "010000(4096)\n",
                               __func__);
                    tally = 4096;
                  }

#ifdef ATTN_HACK
                word36 w0, w1, w2;
                iom_core_read (iomUnitIdx, daddr + 0, & w0, __func__);
                iom_core_read (iomUnitIdx, daddr + 1, & w1, __func__);
                iom_core_read (iomUnitIdx, daddr + 2, & w2, __func__);

                // When the console prints out "Command:", press the Attention
                // key one second later
                if (csp->attn_hack &&
                    tally == 3 &&
                    w0 == 0103157155155llu &&
                    w1 == 0141156144072llu &&
                    w2 == 0040177177177llu)
                  {
                    if (! csp->once_per_boot)
                      {
                        sim_activate (& attn_unit[con_unit_idx], ACTIVATE_1SEC);
                        csp->once_per_boot = true;
                      }
                  }

                // When the console prints out "Ready", press the Attention
                // key one second later
                if (csp->attn_hack &&
                    tally == 2 &&
                    w0 == 0122145141144llu &&
                    w1 == 0171015012177llu)
                  {
                    if (! csp->once_per_boot)
                      {
                        sim_activate (& attn_unit[con_unit_idx], ACTIVATE_1SEC);
                        csp->once_per_boot = true;
                      }
                  }
#endif // ATTN_HACK

		word36 buf[tally];
		iom_indirect_data_service (iomUnitIdx, chan, buf, & tally, false);

                // Tally is in words, not chars.
                char text[tally * 4 + 1];
                char * textp = text;
		word36 * bufp = buf;
                * textp = 0;
#ifndef __MINGW64__
                newlineOff ();
#endif
#ifdef COLOR
                sim_print (""); // force text color reset
#endif
                while (tally)
                  {
                    word36 datum = * bufp ++;
                    tally --;
    
                    for (int i = 0; i < 4; i ++)
                      {
                        word36 wide_char = datum >> 27; // slide leftmost char
                                                        //  into low byte
                        datum = datum << 9; // lose the leftmost char
                        char ch = wide_char & 0x7f;
                        if (ch != 0177 && ch != 0)
                          {
                            console_putchar ((int) con_unit_idx, ch);
                            * textp ++ = ch;
                          }
                      }
                  }
                * textp ++ = 0;

                // autoinput expect
                if (csp->autop && * csp->autop == 030)
                  {
                    //   ^xstring\0
                    //size_t expl = strlen ((char *) (csp->autop + 1));
                    //   ^xstring^x
                    size_t expl = strcspn ((char *) (csp->autop + 1), "\030");

                    if (strncmp (text, (char *) (csp->autop + 1), expl) == 0)
                      {
                        csp->autop += expl + 2;
                        sim_activate (& attn_unit[con_unit_idx], ACTIVATE_1SEC);
                      }
                  }
                // autoinput expect
                if (csp->autop && * csp->autop == 031)
                  {
                    //   ^ystring\0
                    //size_t expl = strlen ((char *) (csp->autop + 1));
                    //   ^ystring^y
                    size_t expl = strcspn ((char *) (csp->autop + 1), "\031");

                    char needle [expl + 1];
                    strncpy (needle, (char *) csp->autop + 1, expl);
                    needle [expl] = 0;
                    if (strstr (text, needle))
                      {
                        csp->autop += expl + 2;
                        sim_activate (& attn_unit[con_unit_idx], ACTIVATE_1SEC);
                      }
                  }
                handleRCP (text);
#ifndef __MINGW64__
                newlineOn ();
#endif
                p->stati = 04000;
                p->initiate = false;

               if (p->DDCW_22_23_TYPE != 0)
                 sim_warn ("curious... a console write with more than one "
                             "DDCW?\n");

             }
            while (p->DDCW_22_23_TYPE != 0); // while not IOTD
          }
	  break;

        case 040:               // Reset
          {
            sim_debug (DBG_NOTIFY, & opc_dev,
                       "%s: Reset cmd received\n", __func__);
            csp->io_mode = opc_no_mode;
            p->stati = 04000;
          }
          break;

        case 051:               // Write Alert -- Ring Bell
          {
            p->isRead = false;
            console_putstr ((int) con_unit_idx,  "CONSOLE: ALERT\r\n");
            sim_debug (DBG_NOTIFY, & opc_dev,
                       "%s: Write Alert cmd received\n", __func__);
            console_putchar ((int) con_unit_idx, '\a');
            p->stati = 04000;
          }
          break;

        case 057:               // Read ID (according to AN70-1)
          {
            // FIXME: No support for Read ID; appropriate values are not known
            //[CAC] Looking at the bootload console code, it seems more 
            // concerned about the device responding, rather then the actual
            // returned value. Make some thing up.
            sim_debug (DBG_NOTIFY, & opc_dev,
                       "%s: Read ID received\n", __func__);
            p->stati = 04500;
          }
          break;

        default:
          {
            p->stati = 04501; // command reject, invalid instruction code
            sim_debug (DBG_ERR, & opc_dev, "%s: Unknown command 0%o\n",
                       __func__, p->IDCW_DEV_CMD);
            p->chanStatus = chanStatIncorrectDCW;
          }
	  break;
      }
    return IOM_CMD_OK;
  }

static void consoleProcessIdx (int conUnitIdx)
  {
    opc_state_t * csp = console_state + conUnitIdx;
    int c;

//// Move data from keyboard buffers into type-ahead buffer

    for (;;)
      {
        c = sim_poll_kbd ();
        if (c == SCPE_OK)
          c = accessGetChar (& csp->console_access);

        // Check for stop signaled by simh

        if (breakEnable && stop_cpu)
          {
            console_putstr (conUnitIdx,  "Got <sim stop>\r\n");
            return;
          }

        // Check for ^E
        //   (Windows doesn't handle ^E as a signal; need to explictily test 
        //   for it.)

        if (breakEnable && c == SCPE_STOP)
          {
            console_putstr (conUnitIdx,  "Got <sim stop>\r\n");
            stop_cpu = 1;
            return; // User typed ^E to stop simulation
          }

        // Check for simh break

        if (breakEnable && c == SCPE_BREAK)
          {
            console_putstr (conUnitIdx,  "Got <sim stop>\r\n");
            stop_cpu = 1;
            return; // User typed ^E to stop simulation
          }

        // End of available input

        if (c == SCPE_OK)
          break;

        // simh sanity test

        if (c < SCPE_KFLAG)
          {
            sim_printf ("impossible %d %o\n", c, c);
            continue; // Should be impossible
          }

        // translate to ascii

        int ch = c - SCPE_KFLAG;

        // XXX This is subject to race conditions
        if (csp->io_mode != opc_read_mode)
          {
            if (ch == '\033' || ch == '\001') // escape or ^A
              {
                ta_flush ();
                csp->attn_pressed = true;
                continue;
              }
          }

        // ^S

        if (ch == 023) // ^S simh command
          {
            if (! csp->simh_attn_pressed)
              {
                csp->simh_attn_pressed = true;
                csp->simh_buffer_cnt = 0;
                console_putstr (conUnitIdx,  "SIMH> ");
              }
            continue;
          }

//// ^T

        if (ch == 024) // ^T
          {
            char buf[256];
            char cms[3] = "?RW";
            sprintf (buf, "^T attn %c %c\r\n",
                     console_state[0].attn_pressed+'0',
                     cms[console_state[0].io_mode]);
            console_putstr (conUnitIdx, buf);
            continue;
          }

//// In ^S mode (accumulating a simh command)?

        if (csp->simh_attn_pressed)
          {
            ta_get ();
            if (ch == '\177' || ch == '\010')  // backspace/del
              {
                if (csp->simh_buffer_cnt > 0)
                  {
                    -- csp->simh_buffer_cnt;
                    csp->simh_buffer[csp->simh_buffer_cnt] = 0;
                    console_putstr (conUnitIdx,  "\b \b");
                  }
                return;
              }

            //// simh ^R

            if (ch == '\022')  // ^R
              {
                console_putstr (conUnitIdx,  "^R\r\nSIMH> ");
                for (int i = 0; i < csp->simh_buffer_cnt; i ++)
                  console_putchar (conUnitIdx, (char) (csp->simh_buffer[i]));
                return;
              }

            //// simh ^U
    
            if (ch == '\025')  // ^U
              {
                console_putstr (conUnitIdx,  "^U\r\nSIMH> ");
                csp->simh_buffer_cnt = 0;
                return;
              }

            //// simh CR/LF

            if (ch == '\012' || ch == '\015')  // CR/LF
              {
                console_putstr (conUnitIdx,  "\r\n");
                csp->simh_buffer[csp->simh_buffer_cnt] = 0;

                char * cptr = csp->simh_buffer;
                char gbuf[simh_buffer_sz];
                cptr = (char *) get_glyph (cptr, gbuf, 0); /* get command glyph */
                if (strlen (gbuf))
                  {
                    CTAB *cmdp;
                    if ((cmdp = find_cmd (gbuf))) /* lookup command */
                      {
                        t_stat stat = cmdp->action (cmdp->arg, cptr);
                           /* if found, exec */
                        if (stat != SCPE_OK)
                          {
                            char buf[4096];
                            sprintf (buf, "SIMH returned %d '%s'\r\n", stat,
                                     sim_error_text (stat));
                            console_putstr (conUnitIdx,  buf);
                          }
                      }
                    else
                       console_putstr (conUnitIdx,
                                       "SIMH didn't recognize the command\r\n");
                  }
                csp->simh_buffer_cnt = 0;
                csp->simh_buffer[0] = 0;
                csp->simh_attn_pressed = false;
                return;
              }

            //// simh ESC/^D/^Z

            if (ch == '\033' || ch == '\004' || ch == '\032')  // ESC/^D/^Z
              {
                console_putstr (conUnitIdx,  "\r\nSIMH cancel\r\n");
                // Empty input buffer
                csp->simh_buffer_cnt = 0;
                csp->simh_buffer[0] = 0;
                csp->simh_attn_pressed = false;
                return;
              }

            //// simh isprint?

            if (isprint (ch))
              {
                // silently drop buffer overrun
                if (csp->simh_buffer_cnt + 1 >= simh_buffer_sz)
                  return;
                csp->simh_buffer[csp->simh_buffer_cnt ++] = (char) ch;
                console_putchar (conUnitIdx, (char) ch);
                return;
              }
            return;
          } // if (simh_attn_pressed)

        // Save the character

        ta_push (c);
      }

//// Check for stop signaled by simh

    if (breakEnable && stop_cpu)
      {
        console_putstr (conUnitIdx,  "Got <sim stop>\r\n");
        return;
      }


//// Console is reading and autoinput is ready
////   Move line of text from autoinput buffer to console buffer

    if (csp->io_mode == opc_read_mode &&
        csp->autop != NULL)
      {
        int announce = 1;
#ifdef COLOR
        sim_print (""); // force text color reset
#endif
        for (;;)
          {
            if (csp->tailp >= csp->buf + sizeof (csp->buf))
             {
                sim_debug (DBG_WARN, & opc_dev,
                           "getConsoleInput: Buffer full; flushing "
                           "autoinput.\n");
                sendConsole (conUnitIdx, 04000); // Normal status
                return;
              }
            unsigned char c = * (csp->autop);
            if (c == 4) // eot
              {
                free (csp->auto_input);
                csp->auto_input = NULL;
                csp->autop = NULL;
                // Empty input buffer
                csp->readp = csp->buf;
                csp->tailp = csp->buf;
                sendConsole (conUnitIdx, 04310); // Null line, status operator
                                                 // distracted
                console_putstr (conUnitIdx,  "CONSOLE: RELEASED\r\n");
                return;
              }
            if (c == 030 || c == 031) // ^X ^Y
              {
                // an expect string is in the autoinput buffer; wait for it 
                // to be processed
                return;
              }
            if (c == 0)
              {
                free (csp->auto_input);
                csp->auto_input = NULL;
                csp->autop = NULL;
                sim_debug (DBG_NOTIFY, & opc_dev,
                           "getConsoleInput: Got auto-input EOS\n");
                goto eol;
              }
            if (announce)
              {
                console_putstr (conUnitIdx,  "[auto-input] ");
                announce = 0;
              }
            csp->autop ++;

            if (isprint ((char) c))
              sim_debug (DBG_NOTIFY, & opc_dev,
                         "getConsoleInput: Used auto-input char '%c'\n", c);
            else
              sim_debug (DBG_NOTIFY, & opc_dev,
                         "getConsoleInput: Used auto-input char '\\%03o'\n", c);

            if (c == '\012' || c == '\015')
              {
eol:
                console_putstr (conUnitIdx,  "\r\n");
                sim_debug (DBG_NOTIFY, & opc_dev,
                           "getConsoleInput: Got EOL\n");
                sendConsole (conUnitIdx, 04000); // Normal status
                return;
              }
            else
              {
                * csp->tailp ++ = c;
                console_putchar (conUnitIdx, (char) c);
              }
          } // for (;;)
      } // if (autop)


//// Read mode and nothing in console buffer
////   Check for timeout

    if (csp->io_mode == opc_read_mode &&
        csp->tailp == csp->buf)
      {
        if (csp->startTime + 30 < time (NULL))
          {
            console_putstr (conUnitIdx,  "CONSOLE: TIMEOUT\r\n");
            csp->readp = csp->buf;
            csp->tailp = csp->buf;
            sendConsole (conUnitIdx, 04310); // Null line, status operator
                                             // distracted
          }
      }

//// Peek at the character in the typeahead buffer

    c = ta_peek ();

    // No data
    if (c == SCPE_OK)
        return; 

    // Convert from simh encoding to ASCII
    if (c < SCPE_KFLAG)
      {
        sim_printf ("impossible %d %o\n", c, c);
        return; // Should be impossible
      }

    // translate to ascii

    int ch = c - SCPE_KFLAG;

    // XXX This is subject to race conditions
    if (csp->io_mode != opc_read_mode)
      {
        if (ch == '\033' || ch == '\001') // escape or ^A
          {
            ta_get ();
            csp->attn_pressed = true;
          }
        return;
      }

    if (ch == '\177' || ch == '\010')  // backspace/del
      {
        ta_get ();
        if (csp->tailp > csp->buf)
          {
            * csp->tailp = 0;
            -- csp->tailp;
            console_putstr (conUnitIdx,  "\b \b");
          }
        return;
      }

    if (ch == '\022')  // ^R
      {
        ta_get ();
        console_putstr (conUnitIdx,  "^R\r\nM-> ");
        for (unsigned char * p = csp->buf; p < csp->tailp; p ++)
          console_putchar (conUnitIdx, (char) (*p));
        return;
      }

    if (ch == '\025')  // ^U
      {
        ta_get ();
        console_putstr (conUnitIdx,  "^U\r\nM-> ");
        csp->tailp = csp->buf;
        return;
      }

    if (ch == '\012' || ch == '\015')  // CR/LF
      {
        ta_get ();
        console_putstr (conUnitIdx,  "\r\n");
        sendConsole (conUnitIdx, 04000); // Normal status
        return;
      }

    if (ch == '\033' || ch == '\004' || ch == '\032')  // ESC/^D/^Z
      {
        ta_get ();
        console_putstr (conUnitIdx,  "\r\n");
        // Empty input buffer
        csp->readp = csp->buf;
        csp->tailp = csp->buf;
        sendConsole (conUnitIdx, 04310); // Null line, status operator
                                         // distracted
        console_putstr (conUnitIdx,  "CONSOLE: RELEASED\n");
        return;
      }

    if (isprint (ch))
      {
        ta_get ();
        // silently drop buffer overrun
        if (csp->tailp >= csp->buf + sizeof (csp->buf))
          return;

#ifdef COLOR
        sim_print (""); // force text color reset
#endif
        * csp->tailp ++ = (unsigned char) ch;
        console_putchar (conUnitIdx, (char) ch);
        return;
      }
    // ignore other chars...
    ta_get ();
    return;    
  }

void consoleProcess (void)
  {
    for (int conUnitIdx = 0; conUnitIdx < (int) opc_dev.numunits; conUnitIdx ++)
      consoleProcessIdx (conUnitIdx);
  }

/*
 * opc_iom_cmd ()
 *
 * Handle a device command.  Invoked by the IOM while processing a PCW
 * or IDCW.
 */

// The console is a CPI device; only the PCW command is executed.

int opc_iom_cmd (uint iomUnitIdx, uint chan)

  {
    // Execute the command in the PCW.

    // uint chanloc = mbx_loc (iomUnitIdx, pcwp->chan);

#if defined(THREADZ) || defined(LOCKLESS)
    lock_libuv ();
#endif

    int rc = opc_cmd (iomUnitIdx, chan);

#if defined(THREADZ) || defined(LOCKLESS)
    unlock_libuv ();
#endif
    // return rc;
    if (rc == IOM_CMD_PENDING)
      return rc;
    return IOM_CMD_NO_DCW;
  }

static t_stat opc_svc (UNIT * unitp)
  {
    int con_unit_idx = (int) OPC_UNIT_NUM (unitp);
    uint ctlr_port_num = 0; // Consoles are single ported
    uint iom_unit_idx = cables->opc_to_iom[con_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->opc_to_iom[con_unit_idx][ctlr_port_num].chan_num;
    opc_iom_cmd (iom_unit_idx, chan_num);
    return SCPE_OK;
  }

static t_stat opc_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr,
                                 UNUSED int val, UNUSED const void * desc)
  {
    sim_print ("Number of OPC units in system is %d\n", opc_dev.numunits);
    return SCPE_OK;
  }

static t_stat opc_set_nunits (UNUSED UNIT * uptr, int32 UNUSED value,
                                const char * cptr, UNUSED void * desc)
  {
    if (! cptr)
      return SCPE_ARG;
    int n = atoi (cptr);
    if (n < 1 || n > N_OPC_UNITS_MAX)
      return SCPE_ARG;
    opc_dev.numunits = (uint32) n;
    return SCPE_OK;
  }



static config_value_list_t cfg_on_off[] =
  {
    { "off", 0 },
    { "on", 1 },
    { "disable", 0 },
    { "enable", 1 },
    { NULL, 0 }
  };

static config_list_t opc_config_list[] =
  {
#ifdef ATTN_HACK
    /* 0 */ { "attn_hack", 0, 1, cfg_on_off },
#endif
   { "autoaccept", 0, 1, cfg_on_off },
   { NULL, 0, 0, NULL }
  };

static t_stat opc_set_config (UNUSED UNIT *  uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc)
  {
    int devUnitIdx = (int) OPC_UNIT_NUM (uptr);
    opc_state_t * csp = console_state + devUnitIdx;
// XXX Minor bug; this code doesn't check for trailing garbage
    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfg_parse (__func__, cptr, opc_config_list,
                           & cfg_state, & v);
        switch (rc)
          {
            if (rc == -1) // done
              break;

            if (rc == -2) // error
              {
                cfg_parse_done (& cfg_state);
                return SCPE_ARG;
              }

            const char * p = opc_config_list[rc].name;

#ifdef ATTN_HACK
            if (strcmp (p, "attn_hack") == 0)
              {
                csp->attn_hack = (int) v;
                break;
              }
#endif
            if (strcmp (p, "autoaccept") == 0)
              {
                csp->autoaccept = (int) v;
                break;
              }
    
            default:
              sim_warn ("error: opc_set_config: invalid cfg_parse rc <%d>\n",
                          rc);
              cfg_parse_done (& cfg_state);
              return SCPE_ARG;
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfg_parse_done (& cfg_state);
    return SCPE_OK;
  }

static t_stat opc_show_config (UNUSED FILE * st, UNUSED UNIT * uptr,
                               UNUSED int  val, UNUSED const void * desc)
  {
#ifdef ATTN_HACK
    int devUnitIdx = (int) OPC_UNIT_NUM (uptr);
    opc_state_t * csp = console_state + devUnitIdx;
    sim_msg ("Attn hack:  %d\n", csp->attn_hack);
#endif
    return SCPE_OK;
  }

t_stat set_console_port (int32 arg, const char * buf)
  {
    if (! buf)
      return SCPE_ARG;
    int n = atoi (buf);
    if (n < 0 || n > 65535) // 0 is 'disable'
      return SCPE_ARG;
    if (arg < 0 || arg >= N_OPC_UNITS_MAX)
      return SCPE_ARG;
    console_state[arg].console_access.port = n;
    sim_msg ("Console %d port set to %d\n", arg, n);
    return SCPE_OK;
  }

t_stat set_console_address (int32 arg, const char * buf)
  {
    if (arg < 0 || arg >= N_OPC_UNITS_MAX)
      return SCPE_ARG;
    if (console_state[arg].console_access.address)
      free (console_state[arg].console_access.address);
    console_state[arg].console_access.address = strdup (buf);
    sim_msg ("Console %d address set to %s\n", arg, console_state[arg].console_access.address);
    return SCPE_OK;
  }

t_stat set_console_pw (int32 arg, UNUSED const char * buf)
  {
    if (arg < 0 || arg >= N_OPC_UNITS_MAX)
      return SCPE_ARG;
    if (strlen (buf) == 0)
      {
        sim_msg ("no password\n");
        console_state[arg].console_access.pw[0] = 0;
        return SCPE_OK;
      }
    if (arg < 0 || arg >= N_OPC_UNITS_MAX)
      return SCPE_ARG;
    char token[strlen (buf)];
    int rc = sscanf (buf, "%s", token);
    if (rc != 1)
      return SCPE_ARG;
    if (strlen (token) > PW_SIZE)
      return SCPE_ARG;
    strcpy (console_state[arg].console_access.pw, token);
    return SCPE_OK;
  }

static void console_putstr (int conUnitIdx, char * str)
  {
#ifdef COLOR
    sim_print (""); // Force text color reset
#endif
    size_t l = strlen (str);
    for (size_t i = 0; i < l; i ++)
      sim_putchar (str[i]);
    opc_state_t * csp = console_state + conUnitIdx;
    if (csp->console_access.loggedOn)
      accessStartWrite (csp->console_access.client, str,
                           (ssize_t) l);
  }

static void console_putchar (int conUnitIdx, char ch)
  {
    sim_putchar (ch);
    opc_state_t * csp = console_state + conUnitIdx;
    if (csp->console_access.loggedOn)
      accessStartWrite (csp->console_access.client, & ch, 1);
  }

static void consoleConnectPrompt (uv_tcp_t * client)
  {
    accessStartWriteStr (client, "password: \r\n");
    uv_access * console_access = (uv_access *) client->data;
    console_access->pwPos = 0;
  }

void startRemoteConsole (void)
  {
    for (int conUnitIdx = 0; conUnitIdx < N_OPC_UNITS_MAX; conUnitIdx ++)
      {
        console_state[conUnitIdx].console_access.connectPrompt = consoleConnectPrompt;
        console_state[conUnitIdx].console_access.connected = NULL;
        console_state[conUnitIdx].console_access.useTelnet = true;
        uv_open_access (& console_state[conUnitIdx].console_access);
      }
  }

