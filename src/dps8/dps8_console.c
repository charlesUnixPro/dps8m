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
#include "dps8_mt.h"  // attachTape
#include "dps8_disk.h"  // attachDisk
#include "dps8_cable.h"

#define ASSUME0 0

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

UNIT opcon_unit [N_OPCON_UNITS_MAX] =
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
    unsigned char buf[bufsize];
    unsigned char *tailp;
    unsigned char *readp;
    bool have_eol;
    unsigned char *auto_input;
    unsigned char *autop;
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

static int attn_hack = 0;

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
    opcon_reset (& opcon_dev);
    console_state . auto_input = NULL;
    console_state . autop = NULL;

    // The quit signal is used has the console ATTN key
    struct sigaction quit_action;
    quit_action . sa_handler = quit_sig_hndlr;
    quit_action . sa_flags = SA_RESTART;
    sigaction (SIGQUIT, & quit_action, NULL);

}

static int opcon_autoinput_set (UNUSED UNIT * uptr, UNUSED int32 val, char *  cptr, UNUSED void * desc)
  {
    if (cptr)
      {
        unsigned char * new =(unsigned char *) strdupesc (cptr);
        if (console_state . auto_input)
          {
            size_t nl = strlen ((char *) new);
            size_t ol = strlen ((char *) console_state . auto_input);

            unsigned char * old = realloc (console_state . auto_input, nl + ol + 1);
            strcpy ((char *) old + ol, (char *) new);
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
        unsigned char * new = (unsigned char *) strdupesc (cptr);
        if (console_state . auto_input)
          {
            size_t nl = strlen ((char *) new);
            size_t ol = strlen ((char *) console_state . auto_input);

            unsigned char * old = realloc (console_state . auto_input, nl + ol + 1);
            strcpy ((char *) old + ol, (char *) new);
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
               __func__, (void *) st, (void *) uptr, val, desc);

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
    send_special_interrupt ((uint) cables -> cablesFromIomToCon [ASSUME0] . iomUnitIdx,
                            (uint) cables -> cablesFromIomToCon [ASSUME0] . chan_num, 
                            ASSUME0, 0, 0);
    return SCPE_OK;
  }

static UNIT attn_unit = 
  { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL };

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
    runtty . c_oflag &= (unsigned int) ~OPOST; /* no output edit */
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
#endif
    size_t len = strlen (text);
    char label [len];
    char with [len];
    char drive [len];
    //char whom [len];
    int rc = sscanf (text, "%*d.%*d RCP: Mount Reel %s %s ring on %s",
                label, with, drive);
    if (rc == 3)
      {
        //sim_printf ("label %s %s ring on %s\n", label, with, drive);
        bool withring = (strcmp (with, "with") == 0);
        char labelDotTap [strlen (label) + strlen (".tap") + 1];
        strcpy (labelDotTap, label);
        strcat (labelDotTap, ".tap");
sim_printf ("<%s>\n", labelDotTap);
        attachTape (labelDotTap, withring, drive);
        return;
      }

    rc = sscanf (text, "%*d.%*d RCP: Remount Reel %s %s ring on %s",
                label, with, drive);
    if (rc == 3)
      {
        //sim_printf ("label %s %s ring on %s\n", label, with, drive);
        bool withring = (strcmp (with, "with") == 0);
        char labelDotTap [strlen (label) + 4];
        strcpy (labelDotTap, label);
        strcat (labelDotTap, ".tap");
sim_printf ("<%s>\n", labelDotTap);
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
        //sim_printf ("label %s %s ring on %s\n", label, with, drive);
        detachTape (drive);
        return;
      }


    rc = sscanf (text, "%*d.%*d RCP: Detached %s",
                drive);
    if (rc == 1)
      {
        //sim_printf ("label %s %s ring on %s\n", label, with, drive);
        detachTape (drive);
        return;
      }
#endif

#if 0
    rc = sscanf (text, "%*d.%*d RCP: Mount logical volume %s for %s",
                label, whom);
    if (rc == 2)
      {
        //sim_printf ("label %s %s ring on %s\n", label, with, drive);
        char labelDotDsk [strlen (label) + 4];
        strcpy (labelDotDsk, label);
        strcat (labelDotDsk, ".dsk");
sim_printf ("<%s>\n", labelDotDsk);
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

static void sendConsole (word12 stati)
  {
    uint tally = console_state . tally;
    uint daddr = console_state . daddr;
    int con_unit_num = (int) OPCON_UNIT_NUM (console_state . unitp);
    int iomUnitIdx = cables -> cablesFromIomToCon [con_unit_num] . iomUnitIdx;
    
    int chan = console_state . chan;
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
// XXX this should be iomIndirectDataService
    p -> charPos = tally % 4;
    while (tally && console_state . readp < console_state . tailp)
      {
        uint charno;
        for (charno = 0; charno < 4; ++ charno)
          {
            if (console_state . readp >= console_state . tailp)
              break;
            unsigned char c = (unsigned char) (* console_state . readp ++);
            putbits36 (& M [daddr], charno * 9, 9, c);
          }
        // cp = charno % 4;

        daddr ++;
        tally --;
      }
    if (console_state . readp < console_state . tailp)
      {
        sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: discarding %d characters from end of line\n", (int) (console_state . tailp - console_state . readp));
      }
    console_state . readp = console_state . buf;
    console_state . tailp = console_state . buf;
    console_state . io_mode = no_mode;

    p -> stati = stati;
    send_terminate_interrupt ((uint) iomUnitIdx, (uint) chan);
  }



static int con_cmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
                      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    UNIT * unitp = & opcon_unit [devUnitIdx];

    if (p -> PCW_63_PTP)
      {
        sim_warn ("PTP in console\n");
        return -1;
      }

    p -> dev_code = p -> IDCW_DEV_CODE;
    p -> stati = 0;

    switch (p -> IDCW_DEV_CMD)
      {
        case 0: // CMD 00 Request status
          {
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Status request cmd received",
                       __func__);
            p -> stati = 04000;
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

            bool ptro, send, uff;

            // We only expect one DCW, so no loop
            int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                sim_printf ("console read list service failed\n");
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                p -> chanStatus = chanStatIncomplete;
                return -1;
              }
            if (uff)
              {
                sim_printf ("console read ignoring uff\n"); // XXX
              }
            if (! send)
              {
                sim_printf ("console read nothing to send\n");
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                p -> chanStatus = chanStatIncomplete;
                return  -1;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_printf ("console read expected DDCW\n");
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                p -> chanStatus = chanStatIncorrectDCW;
                return -1;
              }

            if (rc)
              {
                sim_printf ("list service failed\n");
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                p -> chanStatus = chanStatIncomplete;
                return -1;
              }

            if (p -> DDCW_22_23_TYPE != 0 && p -> DDCW_22_23_TYPE != 1) //IOTD, IOTP
              {
sim_printf ("uncomfortable with this\n");
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                p -> chanStatus = chanStatIncorrectDCW;
                return -1;
              }

            uint tally = p -> DDCW_TALLY;
            uint daddr = p -> DDCW_ADDR;

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
            console_state . chan = (int) chan;

          }
          //break;
          return 3; // command in progress; do not send terminate interrupt


        case 033:               // Write ASCII
          {
            p -> isRead = false;
            console_state . io_mode = write_mode;

            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Write ASCII cmd received\n", __func__);
            if (console_state . tailp != console_state . buf)
              {
                sim_debug (DBG_WARN, & opcon_dev, "con_iom_cmd: Might be discarding previously buffered input.\n");
              }

            // Get the DDCWs

            bool ptro;
            bool send;
            bool uff;
            do
              {
                int rc = iomListService (iomUnitIdx, chan, & ptro, & send, & uff);
                if (rc < 0)
                  {
                    sim_printf ("console write list service failed\n");
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    p -> chanStatus = chanStatIncomplete;
                    return -1;
                  }
                if (uff)
                  {
                    sim_printf ("console write ignoring uff\n"); // XXX
                  }
                if (! send)
                  {
                    sim_printf ("console write nothing to send\n");
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    p -> chanStatus = chanStatIncomplete;
                    return  -1;
                  }
                if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
                  {
                    sim_printf ("console write expected DDCW\n");
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    p -> chanStatus = chanStatIncorrectDCW;
                    return -1;
                  }

                if (rc)
                  {
                    sim_printf ("list service failed\n");
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    p -> chanStatus = chanStatIncomplete;
                    return -1;
                  }

                uint tally = p -> DDCW_TALLY;
                uint daddr = p -> DDCW_ADDR;

// We would hope that number of valid characters in the last word
// would be in DCW_18_20_CP, but it seems to reliably be zero.

                if (p -> DDCW_22_23_TYPE != 0 && p -> DDCW_22_23_TYPE != 1) //IOTD, IOTP
                  {
sim_printf ("uncomfortable with this\n");
                    p -> stati = 05001; // BUG: arbitrary error code; config switch
                    p -> chanStatus = chanStatIncorrectDCW;
                    return -1;
                  }

                if (tally == 0)
                  {
                    sim_debug (DBG_DEBUG, & iom_dev,
                               "%s: Tally of zero interpreted as 010000(4096)\n",
                               __func__);
                    tally = 4096;
                  }

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

//sim_printf ("%012llo %012llo\n", M [daddr + 0], M [daddr + 1]);
                // Tally is in words, not chars.
    
                char text [tally * 4 + 1];
                * text = 0;
                char * textp = text;
                newlineOff ();
// XXX this should be iomIndirectDataService
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
                p -> stati = 04000;
                p -> initiate = false;

               if (p -> DDCW_22_23_TYPE != 0)
                 sim_printf ("curious... a console write with more than one DDCW?\n");

             }
            while (p -> DDCW_22_23_TYPE != 0); // while not IOTD
          }
    
          break;

        case 040:               // Reset
          {
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Reset cmd received\n", __func__);
            console_state . io_mode = no_mode;
            p -> stati = 04000;
          }
          break;

        case 051:               // Write Alert -- Ring Bell
          {
            p -> isRead = false;
            sim_printf ("CONSOLE: ALERT\n");
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Write Alert cmd received\n", __func__);
            sim_putchar('\a');
            p -> stati = 04000;
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
            p -> stati = 04500;
          }
          break;

        default:
          {
            p -> stati = 04501; // command reject, invalid instruction code
            sim_debug (DBG_ERR, & opcon_dev, "%s: Unknown command 0%o\n",
                       __func__, p -> IDCW_DEV_CMD);
            p -> chanStatus = chanStatIncorrectDCW;

            break;
          }
      }
    return 0;
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
            unsigned char c = * (console_state . autop);
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

            if (isprint ((char) c))
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
    if (stop_cpu)
      return;
    if (c == SCPE_OK)
        return; // no input
    if (c == SCPE_STOP)
      {
        sim_printf ("Got <sim stop>\n");
        return; // User typed ^E to stop simulation
      }
    if (c == SCPE_BREAK)
      {
        sim_printf ("Got <sim break>\n");
        return; // User typed ^E to stop simulation
      }
    if (c < SCPE_KFLAG)
      {
        //sim_printf ("Bad char\n");
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
    //sim_printf ("<%02x>\n", c);
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
        //sim_putchar ('\r');
        sim_putchar ('\n');
        for (unsigned char * p = console_state . buf; p < console_state . tailp; p ++)
          sim_putchar ((int32) (*p));
        return;
      }

    if (c == '\025')  // ^U
      {
        sim_putchar ('^');
        sim_putchar ('U');
        //sim_putchar ('\r');
        sim_putchar ('\n');
        console_state . tailp = console_state . buf;
        return;
      }

    if (c == '\012' || c == '\015')  // CR/LF
      {
        //sim_putchar ('\r');
        sim_putchar ('\n');
        //sim_printf ("send: <%s>\r\n", console_state . buf);
        sendConsole (04000); // Normal status
        return;
      }

    if (c == '\033' || c == '\004' || c == '\032')  // ESC/^D/^Z
      {
        //sim_putchar ('\r');
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
        // silently drop buffer overrun
        if (console_state . tailp >= console_state . buf + sizeof(console_state . buf))
          return;

        * console_state . tailp ++ = (unsigned char) c;
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

int con_iom_cmd (uint iomUnitIdx, uint chan)

  {
    // Execute the command in the PCW.

    // uint chanloc = mbx_loc (iomUnitIdx, pcwp -> chan);

    con_cmd (iomUnitIdx, chan);

    //send_terminate_interrupt (iomUnitIdx, chan);

    return 2;
  }

static t_stat opcon_svc (UNIT * unitp)
  {
    int conUnitNum = (int) OPCON_UNIT_NUM (unitp);
    int iomUnitIdx = cables -> cablesFromIomToCon [conUnitNum] . iomUnitIdx;
    int chan = cables -> cablesFromIomToCon [conUnitNum] . chan_num;
    con_iom_cmd ((uint) iomUnitIdx, (uint) chan);
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
    opcon_dev . numunits = (uint32) n;
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
              attn_hack = (int) v;
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
    return SCPE_OK;
  }

