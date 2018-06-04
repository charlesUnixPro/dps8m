/* Copyright (c) 2007-2013 Michael Mondy
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

//#ifdef __MINGW64__
//#include <inttypes.h>
//#include "signal_gnu.h"
//#endif
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#ifndef __MINGW64__
#include <termios.h>
#endif

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

#include "libtelnet.h"

#define DBG_CTR 1

//#define ASSUME0 0

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
static int opc_autoinput_set (UNIT *uptr, int32 val, const char *cptr,
                                void *desc);
static int opc_autoinput_show (FILE *st, UNIT *uptr, int val,
                                 const void *desc);

static t_stat opc_set_config (UNUSED UNIT *  uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc);
static t_stat opc_show_config (UNUSED FILE * st, UNUSED UNIT * uptr,
                               UNUSED int  val, UNUSED const void * desc);
static MTAB opc_mtab[] = {
    { MTAB_XTD | MTAB_VUN | MTAB_NC, /* mask */
        0,            /* match */
        "AUTOINPUT",  /* print string */
        "AUTOINPUT",  /* match pstring */
        opc_autoinput_set, 
        opc_autoinput_show, 
        NULL, 
        NULL },
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

//#define OPC_UNIT_NUM 0
#define OPC_UNIT_NUM(uptr) ((uptr) - opc_unit)

static t_stat opc_svc (UNIT * unitp);

UNIT opc_unit[N_OPC_UNITS_MAX] =
  {
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& opc_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL }
  };

#define OPC_UNIT_NUM(uptr) ((uptr) - opc_unit)

DEVICE opc_dev = {
    "OPC",       /* name */
    opc_unit,    /* units */
    NULL,          /* registers */
    opc_mtab,     /* modifiers */
    N_OPC_UNITS, /* #units */
    10,            /* address radix */
    8,             /* address width */
    1,             /* address increment */
    8,             /* address width */
    8,             /* data width */
    NULL,          /* examine routine */
    NULL,          /* deposit routine */
    opc_reset,   /* reset routine */
    NULL,          /* boot routine */
    NULL,          /* attach routine */
    NULL,          /* detach routine */
    NULL,          /* context */
    DEV_DEBUG,     /* flags */
    0,             /* debug control flags */
    opc_dt,      /* debug flag names */
    NULL,          /* memory size change */
    NULL,          /* logical name */
    NULL,          // help
    NULL,          // attach help
    NULL,          // help context
    NULL,          // description
    NULL
};

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

#include <ctype.h>
typedef struct opc_state_t
  {
    // Hangs off the device structure
    enum console_mode { opc_no_mode, opc_read_mode, opc_write_mode } io_mode;
    // SIMH console library has only putc and getc; the SIMH terminal
    // library has more features including line buffering.
#define bufsize 81
    unsigned char buf[bufsize];
    unsigned char *tailp;
    unsigned char *readp;
    bool have_eol;
    unsigned char *auto_input;
    unsigned char *autop;
#ifdef ATTN_HACK
    bool once_per_boot;
#endif
    
    // stuff saved from the Read ASCII command
    time_t startTime;
    uint tally;
    uint daddr;
    UNIT * unitp;
    int chan;

#ifdef ATTN_HACK
    int attn_hack;
#endif

    bool attn_pressed;
    bool simh_attn_pressed;
#define simh_buffer_sz 4096
    char simh_buffer[simh_buffer_sz];
    int simh_buffer_cnt;

    uv_access console_access;

 } opc_state_t;

static opc_state_t console_state[N_OPC_UNITS_MAX];

//-- #define N_LINES 4


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


#if 0
//#ifndef __MINGW64__
static void quit_sig_hndlr (int UNUSED signum)
  {
    //printf ("quit\n");
    csp->attn_pressed = true;
  }
//#endif
#endif

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

#if 0
//#ifndef __MINGW64__
    // The quit signal is used has the console ATTN key
    struct sigaction quit_action;
    quit_action.sa_handler = quit_sig_hndlr;
    quit_action.sa_flags = SA_RESTART;
    sigemptyset (& quit_action.sa_mask);
    sigaction (SIGQUIT, & quit_action, NULL);
//#endif
#endif
    //uv_open_console (ASSUME0, console_port);
}

static int opc_autoinput_set (UNUSED UNIT * uptr, UNUSED int32 val,
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
        //csp->auto_input = strdup (cptr);
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
    //csp->auto_input = strdup (cptr);
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

#if 0
    if (csp->auto_input == NULL)
      sim_debug (DBG_NOTIFY, & opc_dev,
                 "%s: No auto-input exists.\n", __func__);
    else
      sim_debug (DBG_NOTIFY, & opc_dev,
        "%s: Auto-input is/was: %s\n", __func__, csp->auto_input);
 #endif
    if (csp->auto_input)
      sim_print ("Autoinput: '%s'\n", csp->auto_input); 
    else
      sim_print ("Autoinput: NULL\n");
    return SCPE_OK;
  }
 
static t_stat console_attn (UNUSED UNIT * uptr);

static UNIT attn_unit[N_OPC_UNITS_MAX] = 
  {
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL },
    { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL }
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
// It appears that Cygwin doesn't grok "%ms"
#if 0
    char * label = NULL;
    char * with = NULL;
    char * drive = NULL;
// 1750.1  RCP: Mount Reel 12.3EXEC_CF0019_1 without ring on tapa_01 
    int rc = sscanf (text, "%*d.%*d RCP: Mount Reel %ms %ms ring on %ms",
                & label, & with, & drive);
#endif
    size_t len = strlen (text);
    char label [len + 1];
    char with [len + 1];
    char drive [len + 1];
    //char whom [len];
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



#if 0
    if (label)
      free (label);
    if (with)
      free (with);
    if (drive)
      free (drive);
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
    uint tally = csp->tally;
    uint daddr = csp->daddr;
    uint ctlr_port_num = 0; // Consoles are single ported
    uint iomUnitIdx = cables->opc_to_iom[conUnitIdx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->opc_to_iom[conUnitIdx][ctlr_port_num].chan_num;
    iom_chan_data_t * p = & iom_chan_data[iomUnitIdx][chan_num];

// XXX this should be iom_indirect_data_service
    p->charPos = tally % 4;

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

    while (tally && csp->readp < csp->tailp)
      {
        uint charno;
        for (charno = 0; charno < 4; ++ charno)
          {
            if (csp->readp >= csp->tailp)
              break;
            unsigned char c = (unsigned char) (* csp->readp ++);
            word36 w;
            iom_core_read (iomUnitIdx, daddr, & w, __func__);
            putbits36_9 (& w, charno * 9, c);
            iom_core_write (iomUnitIdx, daddr, w, __func__);
          }
        // cp = charno % 4;

        daddr ++;
        tally --;
      }
    if (csp->readp < csp->tailp)
      {
        sim_debug (DBG_WARN, & opc_dev,
                   "opc_iom_io: discarding %d characters from end of line\n",
                    (int) (csp->tailp - csp->readp));
      }
    csp->readp = csp->buf;
    csp->tailp = csp->buf;
    csp->io_mode = opc_no_mode;

    p->stati = (word12) stati;
    send_terminate_interrupt (iomUnitIdx, chan_num);
  }


static void console_putchar (int conUnitIdx, char ch);
static void console_putstr (int conUnitIdx, char * str);

static int opc_cmd (uint iomUnitIdx, uint chan)
  {
    iom_chan_data_t * p = & iom_chan_data[iomUnitIdx][chan];
    uint con_unit_idx = get_ctlr_idx (iomUnitIdx, chan);
    UNIT * unitp = & opc_unit[con_unit_idx];
    opc_state_t * csp = console_state + con_unit_idx;

    if (p->PCW_63_PTP)
      {
        sim_warn ("PTP in console\n");
        return -1;
      }

    p->dev_code = p->IDCW_DEV_CODE;
    p->stati = 0;

    //int conUnitIdx = (int) d->devUnitIdx;

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
                return -1;
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
                return  -1;
              }
            if (p->DCW_18_20_CP == 07 || p->DDCW_22_23_TYPE == 2)
              {
                sim_warn ("console read expected DDCW\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncorrectDCW;
                return -1;
              }

            if (rc)
              {
                sim_warn ("list service failed\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncomplete;
                return -1;
              }

            if (p->DDCW_22_23_TYPE != 0 &&
                p->DDCW_22_23_TYPE != 1) //IOTD, IOTP
              {
sim_warn ("uncomfortable with this\n");
                p->stati = 05001; // BUG: arbitrary error code; config switch
                p->chanStatus = chanStatIncorrectDCW;
                return -1;
              }

            uint tally = p->DDCW_TALLY;
            uint daddr = p->DDCW_ADDR;

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }
            csp->startTime = time (NULL);
            csp->tally = tally;
            csp->daddr = daddr;
            csp->unitp = unitp;
            csp->chan = (int) chan;

          }
          //break;
          return 3; // command in progress; do not send terminate interrupt


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
                    return -1;
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
                    return  -1;
                  }
                if (p->DCW_18_20_CP == 07 || p->DDCW_22_23_TYPE == 2)
                  {
                    sim_warn ("console write expected DDCW\n");
                    p->stati = 05001; // BUG: arbitrary error code; 
                                        //config switch
                    p->chanStatus = chanStatIncorrectDCW;
                    return -1;
                  }

                if (rc)
                  {
                    sim_warn ("list service failed\n");
                    p->stati = 05001; // BUG: arbitrary error code;
                                        // config switch
                    p->chanStatus = chanStatIncomplete;
                    return -1;
                  }

                uint tally = p->DDCW_TALLY;
                uint daddr = p->DDCW_ADDR;

// We would hope that number of valid characters in the last word
// would be in DCW_18_20_CP, but it seems to reliably be zero.

                if (p->DDCW_22_23_TYPE != 0 &&
                    p->DDCW_22_23_TYPE != 1) //IOTD, IOTP
                  {
sim_warn ("uncomfortable with this\n");
                    // BUG: arbitrary error code; config switch
                    p->stati = 05001;
                    p->chanStatus = chanStatIncorrectDCW;
                    return -1;
                  }

                if (tally == 0)
                  {
                    sim_debug (DBG_DEBUG, & iom_dev,
                               "%s: Tally of zero interpreted as "
                               "010000(4096)\n",
                               __func__);
                    tally = 4096;
                  }

                word36 w0, w1, w2;
                iom_core_read (iomUnitIdx, daddr + 0, & w0, __func__);
                iom_core_read (iomUnitIdx, daddr + 1, & w1, __func__);
                iom_core_read (iomUnitIdx, daddr + 2, & w2, __func__);
#ifdef ATTN_HACK
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
#if defined(THREADZ) || defined(LOCKLESS)
                        // 1K ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 1000);
#else
                        // 4M ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 4000000);
#endif
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
#if defined(THREADZ) || defined(LOCKLESS)
                        // 1K ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 1000);
#else
                        // 4M ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 4000000);
#endif
                        csp->once_per_boot = true;
                      }
                  }
#endif // ATTN_HACK

                // Tally is in words, not chars.
    
                char text[tally * 4 + 1];
                * text = 0;
                char * textp = text;
#ifndef __MINGW64__
                newlineOff ();
#endif
// XXX this should be iom_indirect_data_service
                sim_print (""); // force text color reset
                while (tally)
                  {
                    //word36 datum = M[daddr ++];
                    word36 datum;
                    iom_core_read (iomUnitIdx, daddr, & datum, __func__);
                    daddr ++;
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
#if defined(THREADZ) || defined(LOCKLESS)
                        // 1K ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 1000);
#else
                        // 4M ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 4000000);
#endif
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
#if defined(THREADZ) || defined(LOCKLESS)
                        // 1K ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 1000);
#else
                        // 4M ~= 1 sec
                        sim_activate (& attn_unit[con_unit_idx], 4000000);
#endif
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

            break;
          }
      }
    return 0;
  }

static void consoleProcessIdx (int conUnitIdx)
  {
// Simplifying logic here; if we have autoinput, then process it and skip
// the keyboard checks, we'll get them on the next cycle.
    opc_state_t * csp = console_state + conUnitIdx;
    if (csp->io_mode == opc_read_mode &&
        csp->autop != NULL)
      {
        if (csp->autop == '\0')
          {
            free (csp->auto_input);
            csp->auto_input = NULL;
            csp->autop = NULL;
            return;
          }
        int announce = 1;
        sim_print (""); // force text color reset
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

   // read mode and nothing typed
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

    int c;

    c = sim_poll_kbd ();
    if (c == SCPE_OK)
      c = accessGetChar (& csp->console_access);

// XXX replace attn key signal with escape char check here
// XXX check for escape to scpe (^E?)
    if (breakEnable && stop_cpu)
      {
        console_putstr (conUnitIdx,  "Got <sim stop>\r\n");
        return;
      }
    if (c == SCPE_OK)
        return; // no input
// Windows doesn't handle ^E as a signal; need to explictily test for it.
    if (breakEnable && c == SCPE_STOP)
      {
        console_putstr (conUnitIdx,  "Got <sim stop>\r\n");
        stop_cpu = 1;
        return; // User typed ^E to stop simulation
      }
    if (breakEnable && c == SCPE_BREAK)
      {
        console_putstr (conUnitIdx,  "Got <sim break>\r\n");
        return; // User typed ^E to stop simulation
      }
    if (c < SCPE_KFLAG)
      {
        return; // Should be impossible
      }
    c -= SCPE_KFLAG;    // translate to ascii

    if (c == 0) // no char
      return;

    if (c == 023) // ^S simh command
      {
        if (! csp->simh_attn_pressed)
          {
            csp->simh_attn_pressed = true;
            csp->simh_buffer_cnt = 0;
            console_putstr (conUnitIdx,  "SIMH> ");
          }
        return;
      }

    if (csp->simh_attn_pressed)
      {
        if (c == '\177' || c == '\010')  // backspace/del
          {
            if (csp->simh_buffer_cnt > 0)
              {
                -- csp->simh_buffer_cnt;
                csp->simh_buffer[csp->simh_buffer_cnt] = 0;
                console_putstr (conUnitIdx,  "\b \b");
              }
            return;
          }

        if (c == '\022')  // ^R
          {
            console_putstr (conUnitIdx,  "^R\r\nSIMH> ");
            for (int i = 0; i < csp->simh_buffer_cnt; i ++)
              console_putchar (conUnitIdx, (char) (csp->simh_buffer[i]));
            return;
          }

        if (c == '\025')  // ^U
          {
            console_putstr (conUnitIdx,  "^U\r\nSIMH> ");
            csp->simh_buffer_cnt = 0;
            return;
          }

        if (c == '\012' || c == '\015')  // CR/LF
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

        if (c == '\033' || c == '\004' || c == '\032')  // ESC/^D/^Z
          {
            console_putstr (conUnitIdx,  "\r\nSIMH cancel\r\n");
            // Empty input buffer
            csp->simh_buffer_cnt = 0;
            csp->simh_buffer[0] = 0;
            csp->simh_attn_pressed = false;
            return;
          }

        if (isprint (c))
          {
            // silently drop buffer overrun
            if (csp->simh_buffer_cnt + 1 >= simh_buffer_sz)
              return;
            csp->simh_buffer[csp->simh_buffer_cnt ++] = (char) c;
            console_putchar (conUnitIdx, (char) c);
            return;
          }
        return;
      }

    if (csp->io_mode != opc_read_mode)
      {
        if (c == '\033') // escape
          csp->attn_pressed = true;
        return;
      }
    if (c == '\177' || c == '\010')  // backspace/del
      {
        if (csp->tailp > csp->buf)
          {
            * csp->tailp = 0;
            -- csp->tailp;
            console_putstr (conUnitIdx,  "\b \b");
          }
        return;
      }

    if (c == '\022')  // ^R
      {
        console_putstr (conUnitIdx,  "^R\r\nM-> ");
        for (unsigned char * p = csp->buf; p < csp->tailp; p ++)
          console_putchar (conUnitIdx, (char) (*p));
        return;
      }

    if (c == '\025')  // ^U
      {
        console_putstr (conUnitIdx,  "^U\r\nM-> ");
        csp->tailp = csp->buf;
        return;
      }

    if (c == '\012' || c == '\015')  // CR/LF
      {
        console_putstr (conUnitIdx,  "\r\n");
        sendConsole (conUnitIdx, 04000); // Normal status
        return;
      }

    if (c == '\033' || c == '\004' || c == '\032')  // ESC/^D/^Z
      {
        console_putstr (conUnitIdx,  "\r\n");
        // Empty input buffer
        csp->readp = csp->buf;
        csp->tailp = csp->buf;
        sendConsole (conUnitIdx, 04310); // Null line, status operator
                                         // distracted
        console_putstr (conUnitIdx,  "CONSOLE: RELEASED\n");
        return;
      }

    if (isprint (c))
      {
        // silently drop buffer overrun
        if (csp->tailp >= csp->buf + sizeof (csp->buf))
          return;

        sim_print (""); // force text color reset
        * csp->tailp ++ = (unsigned char) c;
        console_putchar (conUnitIdx, (char) c);
        return;
      }
    // ignore other chars...
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

    opc_cmd (iomUnitIdx, chan);

    //send_terminate_interrupt (iomUnitIdx, chan);

    return 2;
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
    int n = atoi (cptr);
    if (n < 1 || n > N_OPC_UNITS_MAX)
      return SCPE_ARG;
    opc_dev.numunits = (uint32) n;
    return SCPE_OK;
  }



#ifdef ATTN_HACK
static config_value_list_t cfg_on_off[] =
  {
    { "off", 0 },
    { "on", 1 },
    { "disable", 0 },
    { "enable", 1 },
    { NULL, 0 }
  };
#endif

static config_list_t opc_config_list[] =
  {
#ifdef ATTN_HACK
    /* 0 */ { "attn_hack", 0, 1, cfg_on_off },
#endif
   { NULL, 0, 0, NULL }
  };

static t_stat opc_set_config (UNUSED UNIT *  uptr, UNUSED int32 value,
                              const char * cptr, UNUSED void * desc)
  {
#ifdef ATTN_HACK
    int devUnitIdx = (int) OPC_UNIT_NUM (uptr);
    opc_state_t * csp = console_state + devUnitIdx;
#endif
// XXX Minor bug; this code doesn't check for trailing garbage
    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfg_parse (__func__, cptr, opc_config_list,
                           & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfg_parse_done (& cfg_state);
              return SCPE_ARG;

            case -1: // done
              break;

#ifdef ATTN_HACK
            case  0: // attn_hack
              csp->attn_hack = (int) v;
              break;
#endif
    
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
    sim_print (""); // Force text color reset
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

