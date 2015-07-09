//
//  dps8_console.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_console.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_mt.h"

#define ASSUME0 0

/*
 console.c -- operator's console
 
 CAVEAT
 This code has not been updated to use the generalized async handling
 that was added to the IOM code.  Instead it blocks.
 
 See manual AN87.  See also mtb628.
 
 */

#define N_OPCON_UNITS_MAX 1
#define N_OPCON_UNITS 1 // default
/* config switch -- The bootload console has a 30-second timer mechanism. When
reading from the console, if no character is typed within 30 seconds, the read
operation is terminated. The timer is controlled by an enable switch, must be
set to enabled during Multics and BCE */

static t_stat opcon_reset (DEVICE * dptr);
static t_stat opcon_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat opcon_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static int opcon_autoinput_set(UNIT *uptr, int32 val, char *cptr, void *desc);
static int opcon_autoinput_show(FILE *st, UNIT *uptr, int val, void *desc);

static t_stat con_set_config (UNUSED UNIT *  uptr, UNUSED int32 value,
                              char * cptr, UNUSED void * desc);
static t_stat con_show_config (UNUSED FILE * st, UNUSED UNIT * uptr,
                               UNUSED int  val, UNUSED void * desc);
static MTAB opcon_mod[] = {
    { MTAB_XTD | MTAB_VDV | MTAB_VALO | MTAB_NC,
        0, NULL, "AUTOINPUT",
        opcon_autoinput_set, opcon_autoinput_show, NULL, NULL },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      opcon_set_nunits, /* validation routine */
      opcon_show_nunits, /* display routine */
      "Number of OPCON units in the system", /* value descriptor */
      NULL // Help
    },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO /* | MTAB_VALR */, /* mask */
      0,            /* match */
      (char *) "CONFIG",     /* print string */
      (char *) "CONFIG",         /* match string */
      con_set_config,         /* validation routine */
      con_show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL,            /* help */
    },
    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
};


static DEBTAB opcon_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };


// Multics only supports a single operator console

#define N_OPCON_UNITS 1
//#define OPCON_UNIT_NUM 0
#define OPCON_UNIT_NUM(uptr) ((uptr) - opcon_unit)

static t_stat opcon_svc (UNIT * unitp);

static UNIT opcon_unit [N_OPCON_UNITS] =
  {
    { UDATA (& opcon_svc, 0, 0), 0, 0, 0, 0, 0, NULL, NULL }
  };

DEVICE opcon_dev = {
    "OPCON",       /* name */
    opcon_unit,    /* units */
    NULL,          /* registers */
    opcon_mod,     /* modifiers */
    N_OPCON_UNITS, /* #units */
    10,            /* address radix */
    8,             /* address width */
    1,             /* address increment */
    8,             /* address width */
    8,             /* data width */
    NULL,          /* examine routine */
    NULL,          /* deposit routine */
    opcon_reset,   /* reset routine */
    NULL,          /* boot routine */
    NULL,          /* attach routine */
    NULL,          /* detach routine */
    NULL,          /* context */
    DEV_DEBUG,     /* flags */
    0,             /* debug control flags */
    opcon_dt,      /* debug flag names */
    NULL,          /* memory size change */
    NULL,          /* logical name */
    NULL,          // help
    NULL,          // attach help
    NULL,          // help context
    NULL           // description
};

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

#include <ctype.h>
typedef struct con_state_t
  {
    // Hangs off the device structure
    enum { no_mode, read_mode, write_mode } io_mode;
    // SIMH console library has only putc and getc; the SIMH terminal
    // library has more features including line buffering.
#define bufsize 81
    char buf[bufsize];
    char *tailp;
    char *readp;
    bool have_eol;
    char *auto_input;
    char *autop;
    bool once_per_boot;
    
    // stuff saved from the Read ASCII command
    time_t startTime;
    uint tally;
    uint daddr;
    UNIT * unitp;
    int chan;
 } con_state_t;

// We only support a single console instance, so this should be okay.

static con_state_t console_state;

//-- #define N_LINES 4

static struct
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  } cables_from_ioms_to_con [N_OPCON_UNITS];

static int attn_hack = 0;
static int mount_hack = 0;

static int con_iom_cmd (UNIT * unitp, pcw_t * p);

static t_stat opcon_reset (UNUSED DEVICE * dptr)
  {
    console_state . io_mode = no_mode;
    console_state . tailp = console_state . buf;
    console_state . readp = console_state . buf;
    console_state . once_per_boot = false;
    return SCPE_OK;
  }

static bool attn_pressed = false;

static void quit_sig_hndlr (int UNUSED signum)
  {
    //printf ("quit\n");
    attn_pressed = true;
  }

bool check_attn_key (void)
  {
    if (attn_pressed)
      {
         attn_pressed = false;
         return true;
      }
    return false;
  }

// Once-only initialation

void console_init()
{
    for (int i = 0; i < N_OPCON_UNITS; i ++)
      cables_from_ioms_to_con [i] . iom_unit_num = -1;
    opcon_reset (& opcon_dev);
    console_state . auto_input = NULL;
    console_state . autop = NULL;

    // The quit signal is used has the console ATTN key
    struct sigaction quit_action;
    quit_action . sa_handler = quit_sig_hndlr;
    quit_action . sa_flags = SA_RESTART;
    sigaction (SIGQUIT, & quit_action, NULL);

}

t_stat cable_opcon (int con_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (con_unit_num < 0 || con_unit_num >= (int) opcon_dev . numunits)
      {
        sim_printf ("cable_opcon: opcon_unit_num out of range <%d>\n", con_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_con [con_unit_num] . iom_unit_num != -1)
      {
        sim_printf ("cable_opcon: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_CON, chan_type_CPI, con_unit_num, & opcon_dev, & opcon_unit [con_unit_num], con_iom_cmd);
    if (rc)
      return rc;

    cables_from_ioms_to_con [con_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_con [con_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_con [con_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static int opcon_autoinput_set (UNUSED UNIT * uptr, UNUSED int32 val, char *  cptr, UNUSED void * desc)
  {
    if (cptr)
      {
        char * new = strdupesc (cptr);
        if (console_state . auto_input)
          {
            size_t nl = strlen (new);
            size_t ol = strlen (console_state . auto_input);

            char * old = realloc (console_state . auto_input, nl + ol + 1);
            strcpy (old + ol, new);
            console_state . auto_input = old;
            free (new);
          }
        else
          console_state . auto_input = new;
        //console_state . auto_input = strdup (cptr);
        sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Auto-input now: %s\n", __func__, cptr);
      }
    else
      {
        if (console_state . auto_input)
          free (console_state . auto_input);
        console_state . auto_input = NULL;
        sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Auto-input disabled.\n", __func__);
      }
    console_state . autop = console_state . auto_input;
    return SCPE_OK;
  }

int opconAutoinput (int32 flag, char *  cptr)
  {
    if (! flag)
      {
        char * new = strdupesc (cptr);
        if (console_state . auto_input)
          {
            size_t nl = strlen (new);
            size_t ol = strlen (console_state . auto_input);

            char * old = realloc (console_state . auto_input, nl + ol + 1);
            strcpy (old + ol, new);
            console_state . auto_input = old;
            free (new);
          }
        else
          console_state . auto_input = new;
        //console_state . auto_input = strdup (cptr);
        sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Auto-input now: %s\n", __func__, cptr);
      }
    else
      {
        if (console_state . auto_input)
          free (console_state . auto_input);
        console_state . auto_input = NULL;
        sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Auto-input disabled.\n", __func__);
      }
    console_state . autop = console_state . auto_input;
    return SCPE_OK;
  }

static int opcon_autoinput_show (UNUSED FILE * st, UNUSED UNIT * uptr, 
                                 UNUSED int val, UNUSED void * desc)
  {
    sim_debug (DBG_NOTIFY, & opcon_dev,
               "%s: FILE=%p, uptr=%p, val=%d,desc=%p\n",
               __func__, st, uptr, val, desc);

    if (console_state . auto_input == NULL)
      sim_debug (DBG_NOTIFY, & opcon_dev,
                 "%s: No auto-input exists.\n", __func__);
    else
      sim_debug (DBG_NOTIFY, & opcon_dev,
        "%s: Auto-input is/was: %s\n", __func__, console_state . auto_input);
  
    return SCPE_OK;
  }
 
t_stat console_attn (UNUSED UNIT * uptr)
  {
    send_special_interrupt (cables_from_ioms_to_con [ASSUME0] . iom_unit_num,
                            cables_from_ioms_to_con [ASSUME0] . chan_num, 
                            ASSUME0, 0, 0);
    return SCPE_OK;
  }

static t_stat mount_request (UNIT * uptr)
  {
    loadTape (uptr -> u3, uptr -> up7, uptr -> u4 != 0);
    return SCPE_OK;
  }

static UNIT attn_unit = 
  { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL };

static UNIT mount_unit = 
  { UDATA (& mount_request, 0, 0), 0, 0, 0, 0, 0, NULL, NULL };

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
    runtty . c_oflag &= ~OPOST; /* no output edit */
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

static void handleRCP (char * text)
  {
    //sim_printf ("<%s>\n", text);
    //for (uint i = 0; i < strlen (text); i ++)
      //sim_printf ("%02x ", text [i]);
    //sim_printf ("\n");

// It appears that Cygwin doesn't grok "%ms"
#if 0
    char * label = NULL;
    char * with = NULL;
    char * drive = NULL;
// 1750.1  RCP: Mount Reel 12.3EXEC_CF0019_1 without ring on tapa_01 
    int rc = sscanf (text, "%*d.%*d RCP: Mount Reel %ms %ms ring on %ms",
                & label, & with, & drive);
#else
    size_t len = strlen (text);
    char label [len];
    char with [len];
    char drive [len];
    int rc = sscanf (text, "%*d.%*d RCP: Mount Reel %s %s ring on %s",
                label, with, drive);
#endif
    if (rc == 3)
      {
        //sim_printf ("label %s %s ring on %s\n", label, with, drive);
        bool withring = (strcmp (with, "with") == 0);
        char labelDotTap [strlen (label) + 4];
        strcpy (labelDotTap, label);
        strcat (labelDotTap, ".tap");
sim_printf ("<%s>\n", labelDotTap);
        attachTape (labelDotTap, withring, drive);
      }
#if 0
    if (label)
      free (label);
    if (with)
      free (with);
    if (drive)
      free (drive);
#endif
  }

static void sendConsole (uint stati)
  {
    uint tally = console_state . tally;
    uint daddr = console_state . daddr;
    int con_unit_num = OPCON_UNIT_NUM (console_state . unitp);
    int iom_unit_num = cables_from_ioms_to_con [con_unit_num] . iom_unit_num;
    
    int chan = console_state . chan;

    iomChannelData_ * chan_data = & iomChannelData [iom_unit_num] [chan];
    while (tally && console_state . readp < console_state . tailp)
      {
        int charno;
        for (charno = 0; charno < 4; ++ charno)
          {
            if (console_state . readp >= console_state . tailp)
              break;
            unsigned char c = * console_state . readp ++;
            putbits36 (& M [daddr], charno * 9, 9, c);
          }
        // cp = charno % 4;

        daddr ++;
        tally --;
      }
    if (console_state . readp < console_state . tailp)
      {
        sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: discarding %ld characters from end of line\n", console_state . tailp - console_state . readp);
      }
    console_state . readp = console_state . buf;
    console_state . tailp = console_state . buf;
    console_state . io_mode = no_mode;

    chan_data -> stati = stati;
    send_terminate_interrupt (iom_unit_num, chan);
  }


// return value:
//   0 cmd completed; send terminate intr.
//   1 cmd in process; do not send terminate intr.

static int con_cmd (UNIT * UNUSED unitp, pcw_t * pcwp)
  {
    int con_unit_num = OPCON_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_con [con_unit_num] . iom_unit_num;
    
    int chan = pcwp-> chan;

    iomChannelData_ * chan_data = & iomChannelData [iom_unit_num] [chan];
    if (chan_data -> ptp)
      sim_err ("PTP in console\n");
    chan_data -> dev_code = pcwp -> dev_code;
    chan_data -> stati = 0;
    switch (pcwp -> dev_cmd)
      {
        case 0: // CMD 00 Request status
          {
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Status request cmd received",
                       __func__);
            chan_data -> stati = 04000;
            chan_data -> initiate = true;
          }
          break;

        case 023:               // Read ASCII
          {
            console_state . io_mode = read_mode;
            sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Read ASCII command received\n", __func__);
            if (console_state . tailp != console_state . buf)
              {
                sim_debug (DBG_WARN, & opcon_dev, "%s: Discarding previously buffered input.\n", __func__);
              }
            // TODO: discard any buffered chars from SIMH?
            console_state . tailp = console_state . buf;
            console_state . readp = console_state . buf;

            // Get the DDCW

            dcw_t dcw;
            int rc = iomListService (iom_unit_num, chan, & dcw, NULL);

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
            if (pcwp -> mask)
              daddr |= ((pcwp -> ext) & MASK6) << 18;
            // uint cp = dcw.fields.ddcw.cp;

            if (type != 0 && type != 1) //IOTD, IOTP
              {
sim_printf ("uncomfortable with this\n");
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }
            console_state . startTime = time (NULL);
            console_state . tally = tally;
            console_state . daddr = daddr;
            console_state . unitp = unitp;
            console_state . chan = chan;
          }
          //break;
          return 1; // command in progress; do not send terminate interrupt


        case 033:               // Write ASCII
          {
            chan_data -> isRead = false;
            console_state . io_mode = write_mode;

            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Write ASCII cmd received\n", __func__);
            if (console_state . tailp != console_state . buf)
              {
                sim_debug (DBG_WARN, & opcon_dev, "con_iom_cmd: Might be discarding previously buffered input.\n");
              }

            // Get the DDCW

            dcw_t dcw;
            int rc = iomListService (iom_unit_num, chan, & dcw, NULL);

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
            if (pcwp -> mask)
              daddr |= ((pcwp -> ext) & MASK6) << 18;
            // uint cp = dcw.fields.ddcw.cp;

            if (type != 0 && type != 1) //IOTD, IOTP
              {
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chan_data -> chanStatus = chanStatIncorrectDCW;
                break;
              }

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            //sim_printf ("CONSOLE: ");
            //sim_puts ("CONSOLE: ");


            // When the console prints out "Command:", press the Attention
            // key one second later
            if (attn_hack &&
                tally == 3 &&
                M [daddr + 0] == 0103157155155llu &&
                M [daddr + 1] == 0141156144072llu &&
                M [daddr + 2] == 0040177177177llu)
              {
                //sim_printf ("attn!\n");
                if (! console_state . once_per_boot)
                  {
                    sim_activate (& attn_unit, 4000000); // 4M ~= 1 sec
                    console_state . once_per_boot = true;
                  }
              }

            // When the console prints out "Ready", press the Attention
            // key one second later
            if (attn_hack &&
                tally == 2 &&
                M [daddr + 0] == 0122145141144llu &&
                M [daddr + 1] == 0171015012177llu)
              {
                //sim_printf ("attn!\n");
                if (! console_state . once_per_boot)
                  {
                    sim_activate (& attn_unit, 4000000); // 4M ~= 1 sec
                    console_state . once_per_boot = true;
                  }
              }


            // 1642.9  RCP: Mount Reel 12.3EXEC_CF0019_1 without ring on tapa_00 for Initialize
            // tally 21
            //    0 061066064062  1642
            //    1 056071040040  .9  
            //    2 122103120072  RCP:
            //    3 040115157165   Mou
            //    4 156164040122  nt R
            //    5 145145154040  eel 
            //    6 061062056063  12.3
            //    7 105130105103  EXEC
            //    8 137103106060  _CF0
            //    9 060061071137  019_
            //   10 061040167151  1 wi
            //   11 164150157165  thou
            //   12 164040162151  t ri
            //   13 156147040157  ng o
            //   14 156040164141  n ta
            //   15 160141137060  pa_0
            //   16 060040146157  0 fo
            //   17 162040111156  r In
            //   18 151164151141  itia
            //   19 154151172145  lize
            //   20 015012177177  ....

#if 0
            if (mount_hack &&
                tally > 6 &&
                M [daddr + 2] == 0122103120072llu && // RCP
                M [daddr + 3] == 0040115157165llu && //  Mou
                M [daddr + 4] == 0156164040122llu && // nt R
                M [daddr + 5] == 0145145154040llu)   // eel
              {
                if (M [daddr + 6] == 0061062056063llu && // 12/3 
                    M [daddr + 7] == 0105130105103llu && // EXEC
                    M [daddr + 8] == 0137103106060llu && // _CF0
                    M [daddr + 9] == 0060061071137llu && // 019_
                    (M [daddr + 10] & 0777777000000llu) == 0061040000000) // 1_
                  {
                    sim_printf ("loading 12.3EXEC_CF0019_1\n");
                    mount_unit . up7 = "88534.tap";
                    sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                  }
                else if (M [daddr + 6] == 0061062056063llu && // 12/3 
                         M [daddr + 7] == 0105130105103llu && // EXEC
                         M [daddr + 8] == 0137103106060llu && // _CF0
                         M [daddr + 9] == 0060061071137llu && // 019_
                         (M [daddr + 10] & 0777777000000llu) == 0062040000000) // 2_
                  {
                    sim_printf ("loading 12.3EXEC_DF0019_2\n");
                    mount_unit . up7 = "88631.tap";
                    sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                  }
                else if (M [daddr + 6] == 0061062056063llu && // 12/3 
                         M [daddr + 7] == 0105130105103llu && // EXEC
                         M [daddr + 8] == 0137104106060llu && // _DF0
                         M [daddr + 9] == 0060061071137llu && // 019_
                         (M [daddr + 10] & 0777777000000llu) == 0062040000000) // 2_
                  {
                    sim_printf ("loading 12.3EXEC_DF0019_2\n");
                    mount_unit . up7 = "88631.tap";
                    sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                  }
                else
                  {
                    sim_printf ("unrecognized mount_hack\n");
                    sim_printf ("  %012llo\n", M [daddr + 6]);
                    sim_printf ("  %012llo\n", M [daddr + 7]);
                    sim_printf ("  %012llo\n", M [daddr + 8]);
                    sim_printf ("  %012llo\n", M [daddr + 9]);
                    sim_printf ("  %012llo\n", M [daddr + 10]);
                    sim_printf ("  %012llo\n", M [daddr + 11]);
                  }
              }
#endif
            if (mount_hack &&
                tally > 6 &&
                M [daddr + 2] == 0122103120072llu && // RCP
                M [daddr + 3] == 0040115157165llu && //  Mou
                M [daddr + 4] == 0156164040122llu && // nt R
                M [daddr + 5] == 0145145154040llu)   // eel
              {
                switch (mount_hack)
                  {
                    case 1:
                      {
                        sim_printf ("loading 12.3EXEC_CF0019_1\n");
                        mount_unit . u3 = 1;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "88534.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 2:
                      {
                        sim_printf ("loading 12.3EXEC_DF0019_2.tap\n");
                        mount_unit . u3 = 2;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "88631.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 3:
                      {
                        sim_printf ("loading 12.3LDD_STANDARD_CF0019_1\n");
                        mount_unit . u3 = 1;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "88632.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 4:
                      {
                        sim_printf ("loading 12.3LDD_STANDARD_CF0019_2\n");
                        mount_unit . u3 = 2;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "88633.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 5:
                      {
                        sim_printf ("loading 12.3LDD_STANDARD_CF0019_3\n");
                        mount_unit . u3 = 3;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "88634.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 6:
                      {
                        sim_printf ("loading 12.3LDD_STANDARD_CF0019_4\n");
                        mount_unit . u3 = 4;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "88635.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 7:
                      {
                        sim_printf ("loading 12.3LDD_STANDARD_CF0019_5\n");
                        mount_unit . u3 = 5;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "88636.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 8:
                      {
                        sim_printf ("loading 12.3LDD_STANDARD_CF0019_6\n");
                        mount_unit . u3 = 6;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "99020.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 9:
                      {
                        sim_printf ("loading 12.3UNBUNDLED_DF0019_1\n");
                        mount_unit . u3 = 7;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "98570.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 10:
                      {
                        sim_printf ("loading 12.3UNBUNDLED_CF0019_2\n");
                        mount_unit . u3 = 8;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "99019.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 11:
                      {
                        sim_printf ("loading 12.3MISC_CF0019\n");
                        mount_unit . u3 = 9;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "93085.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 12:
                      {
                        sim_printf ("loading 12.5EXEC_CF0019_1\n");
                        mount_unit . u3 = 1;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "20185.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 13:
                      {
                        sim_printf ("loading 12.5LDD_STANDARD_CF0019_1\n");
                        mount_unit . u3 = 2;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "20186.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 14:
                      {
                        sim_printf ("loading 12.5UNBUNDLED_CF0019_1\n");
                        mount_unit . u3 = 3;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "20188.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 15:
                      {
                        sim_printf ("loading 12.5MISC_CF0015\n");
                        mount_unit . u3 = 4;
                        mount_unit . u4 = 1;
                        mount_unit . up7 = "20187.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    case 20:
                      {
                        sim_printf ("loading cac\n");
                        mount_unit . u3 = 1;
                        mount_unit . u4 = 0;
                        mount_unit . up7 = "cac.tap";
                        sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                      }
                      break;

                    default:
                      {
                        sim_printf ("out of mount hacks\n");
                      }
                      break;
                  }
                mount_hack ++;
              }

            // Tally is in words, not chars.

            char text [tally * 4 + 1];
            * text = 0;
            char * textp = text;
            newlineOff ();
            while (tally)
              {
                word36 datum = M [daddr ++];
                tally --;

                for (int i = 0; i < 4; i ++)
                  {
                    word36 wide_char = datum >> 27; // slide leftmost char into low byte
                    datum = datum << 9; // lose the leftmost char
                    char ch = wide_char & 0x7f;
                    if (ch != 0177 && ch != 0)
                      {
//if (ch == '\r') sim_printf ("hmm\n");
//if (ch == '\n') sim_printf ("er\n");
                        sim_putchar (ch);
                        * textp ++ = ch;
                      }
                  }
              }
            * textp ++ = 0;
            handleRCP (text);
            // sim_printf ("\n");
            newlineOn ();
            chan_data -> stati = 04000;
          }
          break;

        case 040:               // Reset
          {
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Reset cmd received\n", __func__);
            console_state . io_mode = no_mode;
            chan_data -> stati = 04000;
            chan_data -> initiate = true;
          }
          break;

        case 051:               // Write Alert -- Ring Bell
          {
            chan_data -> isRead = false;
            sim_printf ("CONSOLE: ALERT\n");
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Write Alert cmd received\n", __func__);
            sim_putchar('\a');
            chan_data -> stati = 04000;
          }
          break;

        case 057:               // Read ID (according to AN70-1)
          {
            // FIXME: No support for Read ID; appropriate values are not known
            // [CAC] Looking at the bootload console code, it seems more 
            // concerned about the device responding, rather then the actual
            // returned value. Make some thing up.
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Read ID received\n", __func__);
            chan_data -> stati = 04500;
          }
          break;

        default:
          {
            chan_data -> stati = 04501; // command reject, invalid instruction code
            sim_debug (DBG_ERR, & opcon_dev, "%s: Unknown command 0%o\n",
                       __func__, pcwp -> dev_cmd);
            chan_data -> chanStatus = chanStatIncorrectDCW;

            break;
          }
      }
    //status_service (iom_unit_num, chan, false);

    return 0; // send terminate interrupt
  }


void consoleProcess (void)
  {
// Simplifying logic here; if we have autoinput, then process it and skip
// the keyboard checks, we'll get them on the next cycle.
    if (console_state . io_mode == read_mode &&
        console_state . autop != NULL)
      {
        if (console_state . autop == '\0')
          {
            free(console_state . auto_input);
            console_state . auto_input = NULL;
            console_state . autop = NULL;
            return;
          }
        int announce = 1;
        for (;;)
          {
            if (console_state . tailp >= console_state . buf + sizeof(console_state . buf))
             {
                sim_debug (DBG_WARN, & opcon_dev, "getConsoleInput: Buffer full; flushin autoinput.\n");
                sendConsole (04000); // Normal status
                return;
              }
            int c = * (console_state . autop);
            if (c == 4) // eot
              {
                free(console_state . auto_input);
                console_state . auto_input = NULL;
                console_state . autop = NULL;
                // Empty input buffer
                console_state . readp = console_state . buf;
                console_state . tailp = console_state . buf;
                //sendConsole (04310); // operator distracted
                //sendConsole (04000); // Null line, status ok
                sendConsole (04310); // Null line, status operator distracted
                sim_printf ("CONSOLE: RELEASED\n");
                return;
              }
            if (c == 0)
              {
                free(console_state . auto_input);
                console_state . auto_input = NULL;
                console_state . autop = NULL;
                sim_debug (DBG_NOTIFY, & opcon_dev, "getConsoleInput: Got auto-input EOS\n");
                goto eol;
              }
            if (announce)
              {
                sim_printf ("[auto-input] ");
                announce = 0;
              }
            console_state . autop ++;

            if (isprint (c))
              sim_debug (DBG_NOTIFY, & opcon_dev, "getConsoleInput: Used auto-input char '%c'\n", c);
            else
              sim_debug (DBG_NOTIFY, & opcon_dev, "getConsoleInput: Used auto-input char '\\%03o'\n", c);

            if (c == '\012' || c == '\015')
              {
eol:
                sim_putchar ('\n');
                //sim_putchar ('\r');
                sim_debug (DBG_NOTIFY, & opcon_dev, "getConsoleInput: Got EOL\n");
                sendConsole (04000); // Normal status
                return;
              }
            else
              {
                * console_state . tailp ++ = c;
                sim_putchar (c);
              }
          } // for (;;)
      } // if (autop)

   // read mode and nothing typed
    if (console_state . io_mode == read_mode &&
        console_state . tailp == console_state . buf)
      {
        if (console_state . startTime + 30 < time (NULL))
          {
            sim_printf ("CONSOLE: TIMEOUT\n");
            console_state . readp = console_state . buf;
            console_state . tailp = console_state . buf;
            //sendConsole (04000); // Null line, status ok
            sendConsole (04310); // Null line, status operator distracted
          }
      }

    int c;

    c = sim_poll_kbd();
// XXX replace attn key signal with escape char check here
// XXX check for escape to scpe (^E?)

    if (c == SCPE_OK)
        return; // no input
    if (c == SCPE_STOP)
      {
        sim_printf ("Got <sim stop>\r\n");
        return; // User typed ^E to stop simulation
      }
    if (c == SCPE_BREAK)
      {
        sim_printf ("Got <sim break>\r\n");
        return; // User typed ^E to stop simulation
      }
    if (c < SCPE_KFLAG)
      {
        //sim_printf ("Bad char\r\n");
        return; // Should be impossible
      }
    c -= SCPE_KFLAG;    // translate to ascii

    if (c == 0) // no char
      return;

    if (console_state . io_mode != read_mode)
      {
        if (c == '\033') // escape
          attn_pressed = true;
        return;
      }
    //sim_printf ("<%02x>\r\n", c);
    if (c == '\177' || c == '\010')  // backspace/del
      {
        if (console_state . tailp > console_state . buf)
          {
            * console_state . tailp = 0;
            -- console_state . tailp;
            sim_putchar ('\b');
            sim_putchar (' ');
            sim_putchar ('\b');
          }
        return;
      }

    if (c == '\022')  // ^R
      {
        sim_putchar ('^');
        sim_putchar ('R');
        sim_putchar ('\r');
        sim_putchar ('\n');
        for (char * p = console_state . buf; p < console_state . tailp; p ++)
          sim_putchar (*p);
        return;
      }

    if (c == '\025')  // ^U
      {
        sim_putchar ('^');
        sim_putchar ('U');
        sim_putchar ('\r');
        sim_putchar ('\n');
        console_state . tailp = console_state . buf;
        return;
      }

    if (c == '\012' || c == '\015')  // CR/LF
      {
        sim_putchar ('\r');
        sim_putchar ('\n');
        //sim_printf ("send: <%s>\r\n", console_state . buf);
        sendConsole (04000); // Normal status
        return;
      }

    if (c == '\033' || c == '\004' || c == '\032')  // ESC/^D/^Z
      {
        sim_putchar ('\r');
        sim_putchar ('\n');
        // Empty input buffer
        console_state . readp = console_state . buf;
        console_state . tailp = console_state . buf;
        //sendConsole (04000); // Null line, status ok
        sendConsole (04310); // Null line, status operator distracted
        sim_printf ("CONSOLE: RELEASED\n");
        return;
      }

    if (isprint (c))
      {
        * console_state . tailp ++ = c;
        sim_putchar (c);
        return;
      }
    // ignore other chars...
    return;    
  }


/*
 * con_iom_cmd()
 *
 * Handle a device command.  Invoked by the IOM while processing a PCW
 * or IDCW.
 */

// The console is a CPI device; only the PCW command is executed.

static int con_iom_cmd (UNUSED UNIT * unitp, pcw_t * pcwp)
  {
    int con_unit_num = OPCON_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_con [con_unit_num] . iom_unit_num;

    // Execute the command in the PCW.

    // uint chanloc = mbx_loc (iom_unit_num, pcwp -> chan);

    con_cmd (unitp, pcwp);

    send_terminate_interrupt (iom_unit_num, pcwp -> chan);

    return 1;
  }

static t_stat opcon_svc (UNIT * unitp)
  {
    int conUnitNum = OPCON_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_con [conUnitNum] . iom_unit_num;
    int chanNum = cables_from_ioms_to_con [conUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    con_iom_cmd (unitp, pcwp);
 
    return SCPE_OK;
  }

static t_stat opcon_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, UNUSED int val, UNUSED void * desc)
  {
    sim_printf("Number of OPCON units in system is %d\n", opcon_dev . numunits);
    return SCPE_OK;
  }

static t_stat opcon_set_nunits (UNUSED UNIT * uptr, int32 UNUSED value, char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_OPCON_UNITS_MAX)
      return SCPE_ARG;
    opcon_dev . numunits = n;
    return SCPE_OK;
  }



static config_value_list_t cfg_on_off [] =
  {
    { "off", 0 },
    { "on", 1 },
    { "disable", 0 },
    { "enable", 1 },
    { NULL, 0 }
  };

static config_list_t con_config_list [] =
  {
    /* 0 */ { "attn_hack", 0, 1, cfg_on_off },
    /* 1 */ { "mount_hack", 0, 100, cfg_on_off },
   { NULL, 0, 0, NULL }
  };

static t_stat con_set_config (UNUSED UNIT *  uptr, UNUSED int32 value,
                              char * cptr, UNUSED void * desc)
  {
// XXX Minor bug; this code doesn't check for trailing garbage
    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("con_set_config", cptr, con_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG;

            case -1: // done
              break;

            case  0: // attn_hack
              attn_hack = v;
              break;
    
            case  1: // mount_hack
              mount_hack = v;
              break;

            default:
              sim_printf ("error: con_set_config: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG;
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }

static t_stat con_show_config (UNUSED FILE * st, UNUSED UNIT * uptr,
                               UNUSED int  val, UNUSED void * desc)
  {
    sim_printf ("Attn hack:  %d\n", attn_hack);
    sim_printf ("Mount hack: %d\n", mount_hack);
    return SCPE_OK;
  }

