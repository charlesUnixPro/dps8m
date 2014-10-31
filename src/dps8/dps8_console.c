//
//  dps8_console.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>
#include <unistd.h>

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
    char buf[81];
    char *tailp;
    char *readp;
    bool have_eol;
    char *auto_input;
    char *autop;
    bool once_per_boot;

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

static void check_keyboard (void);

static int con_iom_cmd (UNIT * unitp, pcw_t * p);
#if 0
static int con_iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati);
#endif

static t_stat opcon_reset (UNUSED DEVICE * dptr)
  {
    console_state . io_mode = no_mode;
    console_state . tailp = console_state . buf;
    console_state . readp = console_state . buf;
    console_state . have_eol = false;
    console_state . once_per_boot = false;
    return SCPE_OK;
  }

// Once-only initialation

void console_init()
{
    for (int i = 0; i < N_OPCON_UNITS; i ++)
      cables_from_ioms_to_con [i] . iom_unit_num = -1;
    opcon_reset (& opcon_dev);
    console_state . auto_input = NULL;
    console_state . autop = NULL;
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
 
//-- // ============================================================================
//-- 
//-- /*
//--  * con_check_args()
//--  *
//--  * Internal function to do sanity checks
//--  */
//-- 
//-- static int con_check_args(const char* moi, int chan, int dev_code, int* majorp, int* subp, DEVICE **devpp, con_state_t **statepp)
//-- {
//--     
//--     if (chan < 0 || chan >= max_channels) {
//--         *majorp = 05;   // Real HW could not be on bad channel
//--         *subp = 1;
//--         sim_debug (DBG_ERR, & opcon_dev, "%s: Bad channel %d\n", moi, chan);
//--         return 1;
//--     }
//--     
//--     *devpp = get_iom_channel_dev (cables_from_ioms_to_con [OPCON_UNIT_NUM] . iom_unit_num, chan, 0, NULL);
//--     DEVICE *devp = *devpp;
//--     if (devpp == NULL) {
//--         *majorp = 05;
//--         *subp = 1;
//--         sim_debug (DBG_ERR, & opcon_dev, "con_check_args: Internal error, no device for channel 0%o\n", chan);
//--         return 1;
//--     }
//--     chan_devinfo* devinfop = devp->ctxt;
//--     if (devinfop == NULL) {
//--         *majorp = 05;
//--         *subp = 1;
//--         sim_debug (DBG_ERR, & opcon_dev, "con_check_args: Internal error, no device info for channel 0%o\n", chan);
//--         return 1;
//--     }
//--     con_state_t *console_state .  devinfop->statep;
//--     if (dev_code != 0) {
//--         // Consoles don't have units
//--         *majorp = 05;
//--         *subp = 1;
//--         sim_debug (DBG_ERR, & opcon_dev, "con_check_args: Bad dev unit-num 0%o (%d decimal)\n", dev_code, dev_code);
//--         return 1;
//--     }
//--     if (console_state . = NULL) {
//--         if ((console_state .  malloc(sizeof(con_state_t))) == NULL) {
//--             sim_debug (DBG_ERR, & opcon_dev, "con_check_args: Internal error, malloc failed.\n");
//--             return 1;
//--         }
//--         devinfop->statep = console_state . 
//--         console_state . io_mode = no_mode;
//--         console_state . tailp = console_state . buf;
//--         console_state . readp = console_state . buf;
//--         console_state . have_eol = false;
//--         console_state . auto_input = NULL;
//--         console_state . autop = NULL;
//--     }
//--     *statepp = console_state . 
//--     return 0;
//-- }
//-- 
//-- 
//-- // ============================================================================
//-- 

t_stat console_attn (UNUSED UNIT * uptr)
  {
    send_special_interrupt (cables_from_ioms_to_con [ASSUME0] . iom_unit_num,
                            cables_from_ioms_to_con [ASSUME0] . chan_num, 
                            ASSUME0, 0, 0);
    return SCPE_OK;
  }

static t_stat mount_request (UNIT * uptr)
  {
    loadTape (1, uptr -> up7);
    return SCPE_OK;
  }

static UNIT attn_unit = 
  { UDATA (& console_attn, 0, 0), 0, 0, 0, 0, 0, NULL, NULL };

static UNIT mount_unit = 
  { UDATA (& mount_request, 0, 0), 0, 0, 0, 0, 0, NULL, NULL };

static int con_cmd (UNIT * UNUSED unitp, pcw_t * pcwp)
  {
    int con_unit_num = OPCON_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_con [con_unit_num] . iom_unit_num;
    
    word6 rcount = 0;
    word12 residue = 0;
    word3 char_pos = 0;
    bool is_read = true;
    chanStat chanStatus = chanStatNormal;
    bool initiate = false;

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
            initiate = true;
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
            console_state . have_eol = false;

            // Read keyboard if we don't have an EOL from the operator
            // yet
            if (! console_state . have_eol)
              {
                // We won't return anything to the IOM until the operator
                // has finished entering a full line and pressed ENTER.
                sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Starting input loop for channel %d (%#o)\n", chan, chan);
                time_t now = time(NULL);
                while (time(NULL) < now + 30 && ! console_state . have_eol)
                  {
                    check_keyboard ();
                    usleep (100000);       // FIXME: blocking
                  }
                // Impossible to both have EOL and have buffer overflow
                if (console_state . tailp >= console_state . buf + sizeof(console_state . buf))
                  {
                    chan_data -> stati = 04340;
                    sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: buffer overflow\n");
                    break;
                  }
                if (! console_state . have_eol)
                  {
                    chan_data -> stati = 04310;
                    sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Operator distracted (30 second timeout)\n");
                  }
              }
            // We have an EOL from the operator
            sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Transfer for channel %d (%#o)\n", chan, chan);
            
            // Get the DDCW

            dcw_t dcw;
            int rc = iomListService (iom_unit_num, chan, & dcw, NULL);

            if (rc)
              {
                sim_printf ("list service failed\n");
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chanStatus = chanStatIncomplete;
                break;
              }
            if (dcw . type != ddcw)
              {
                sim_printf ("not ddcw? %d\n", dcw . type);
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chanStatus = chanStatIncorrectDCW;
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
                chanStatus = chanStatIncorrectDCW;
                break;
              }

            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & iom_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

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
            console_state . have_eol = false;

            chan_data -> stati = 04000;
          }
          break;


        case 033:               // Write ASCII
          {
            is_read = false;
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
                chanStatus = chanStatIncomplete;
                break;
              }
            if (dcw . type != ddcw)
              {
                sim_printf ("not ddcw? %d\n", dcw . type);
                chan_data -> stati = 05001; // BUG: arbitrary error code; config switch
                chanStatus = chanStatIncorrectDCW;
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
                chanStatus = chanStatIncorrectDCW;
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


#ifdef ATTN_HACK
            // When the console prints out "Command:", press the Attention
            // key one second later
            if (tally == 3 &&
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
#endif

#if 0
sim_printf ("\ntally %d\n", tally);
for (uint i = 0; i < tally; i ++)
  {
    word36 w = M [daddr + i];
    sim_printf ("  %012llo  ", w);
    for (int j = 0; j < 4; j ++)
      {
        uint ch = (w >> 27) & 0177;
        sim_printf ("%c", isprint (ch) ? ch : '.');
        w = (w << 9) & MASK36;
      }
    sim_printf ("\n");
  }
#endif

#ifdef MOUNT_HACK
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

            if (tally > 6 &&
                M [daddr + 2] == 0122103120072llu && // RCP
                M [daddr + 3] == 0040115157165llu && //  Mou
                M [daddr + 4] == 0156164040122llu && // nt R
                M [daddr + 5] == 0145145154040llu)   // eel
              {
                if (M [daddr + 6] == 0061062056063llu && // 12/3 
                    M [daddr + 7] == 0105130105103llu && // EXEC
                    M [daddr + 8] == 0137103106060llu && // _CF0
                    M [daddr + 9] == 0060061071137llu && // 019_
                    (M [daddr + 10] & 0777777000000llu) == 0061040000000) // 01
sim_printf ("loading 12.3EXEC_CF0019_1\n");
                mount_unit . up7 = "88534.tap";
                sim_activate (& mount_unit, 8000000); // 8M ~= 2 sec
                //sim_activate (& mount_unit, 800000000); // 8M ~= 200 sec
                //sim_tape_attach (getTapeUnit (0), "88534.tap");
                //tape_send_special_interrupt (0);
    //send_special_interrupt (cables_from_ioms_to_con [ASSUME0] . iom_unit_num,
                            //cables_from_ioms_to_con [ASSUME0] . chan_num);
              }

#endif

            // Tally is in words, not chars.

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
                      sim_putchar (ch);
#if 0
                    if (isprint (ch))
                      {
                        sim_printf ("%c", ch);
                      }
                    else
                      {
                        sim_printf ("\\%03o", ch);
                      }
#endif
                    //if (ch == '\r')
                      //sim_putchar ('\n');
                  }
              }
            // sim_printf ("\n");
            chan_data -> stati = 04000;
          }
          break;

        case 040:               // Reset
          {
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Reset cmd received\n", __func__);
            console_state . io_mode = no_mode;
            chan_data -> stati = 04000;
            initiate = true;
          }
          break;

        case 051:               // Write Alert -- Ring Bell
          {
            is_read = false;
            // AN70-1 says only console channels respond to this command
            //sim_printf ("CONSOLE: ALERT\n");
            sim_puts ("CONSOLE: ALERT\r\n");
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
            chanStatus = chanStatIncorrectDCW;

            break;
          }
      }
    status_service (iom_unit_num, chan, rcount, 
                    residue, char_pos, is_read, false, initiate, false,
                    chanStatus, iomStatNormal);

    return 0;
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
#if 1
    int conUnitNum = OPCON_UNIT_NUM (unitp);
    int iomUnitNum = cables_from_ioms_to_con [conUnitNum] . iom_unit_num;
    int chanNum = cables_from_ioms_to_con [conUnitNum] . chan_num;
    pcw_t * pcwp = & iomChannelData [iomUnitNum] [chanNum] . pcw;
    con_iom_cmd (unitp, pcwp);
#else
    int con_unit_num = OPCON_UNIT_NUM;
    int iom_unit_num = cables_from_ioms_to_con [con_unit_num] . iom_unit_num;
    word24 dcw_ptr = (word24) (unitp -> u3);
    pcw_t pcw;
    word36 word0, word1;
    
    (void) fetch_abs_pair (dcw_ptr, & word0, & word1);
    decode_idcw (iom_unit_num, & pcw, 1, word0, word1);
    con_iom_cmd (unitp, & pcw);
#endif
 
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


#if 0
/*
 * con_iom_io()
 *
 * Handle an I/O request.  Invoked by the IOM while processing a DDCW.
 */

static int con_iom_io (UNUSED UNIT * unitp, uint chan, UNUSED uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati)
{
    sim_debug (DBG_DEBUG, & opcon_dev, "%s: Chan 0%o\n", __func__, chan);
    
    if (* cp)
      {
        sim_debug (DBG_ERR, & opcon_dev, "%s: Ignoring initial non-zero cp (%u)\n", __func__, * cp);
      }

    * cp = 0;
    //check_keyboard ();
    
    switch (console_state . io_mode)
      {
        case no_mode:
          sim_debug (DBG_ERR, & opcon_dev, "%s: Console is uninitialized\n", __func__);
          * stati = 05001;       // 05 -- Command Reject 01 Invalid Instruction Code
          return 1;
            
        case read_mode:
          {
            // Read keyboard if we don't have an EOL from the operator
            // yet
            if (! console_state . have_eol)
              {
                // We won't return anything to the IOM until the operator
                // has finished entering a full line and pressed ENTER.
                sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Starting input loop for channel %d (%#o)\n", chan, chan);
                time_t now = time(NULL);
                while (time(NULL) < now + 30 && ! console_state . have_eol)
                  {
                    check_keyboard ();
                    usleep (100000);       // FIXME: blocking
                  }
                // Impossible to both have EOL and have buffer overflow
                if (console_state . tailp >= console_state . buf + sizeof(console_state . buf))
                  {
                    //*majorp = 03;       // 03 -- Data Alert
                    //*subp = 040;        // 10 -- Message length alert
                    * stati = 04340;
                    sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: buffer overflow\n");
//--                     cancel_run(STOP_BKPT);
                    return 1;
                  }
                if (! console_state . have_eol)
                  {
                    //*majorp = 03;       // 03 -- Data Alert
                    //*subp = 010;        // 10 -- Operator distracted (30 sec timeout)
                    * stati = 04310;
                    sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Operator distracted (30 second timeout\n");
//--                     cancel_run(STOP_BKPT);
                  }
              }
            // We have an EOL from the operator
            sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Transfer for channel %d (%#o)\n", chan, chan);
#if 0
            // bce_command_processor_ expects multiples chars per word
            for (int charno = 0; charno < 4; ++charno)
              {
                if (console_state . readp >= console_state . tailp)
                    break;
                unsigned char c = *console_state . readp++;
                putbits36(wordp, charno * 9, 9, c);
              }
            if (1)
              {
                char msg[17];
                for (int charno = 0; charno < 4; ++charno)
                  {
                    unsigned char c = getbits36(*wordp, charno * 9, 9);
                    if (c <= 0177 && isprint(c))
                        sprintf(msg+charno*4, " '%c'", c);
                    else
                        sprintf(msg+charno*4, "\\%03o", c);
                  }
                msg[16] = 0;
                sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Returning word %012llo: %s\n", *wordp, msg);
              }
            int ret;
            if (console_state . readp == console_state . tailp)
              {
                console_state . readp = console_state . buf;
                console_state . tailp = console_state . buf;
                // console_state . have_eol = false;
                sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: Entire line now transferred.\n");
                ret = 1;    // FIXME: out of band request to return
              }
            else
              {
                sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: %ld chars remain to be transfered.\n", console_state . tailp - console_state . readp);
                ret = 0;
              }
#else
            // bce_command_processor_ expects multiples chars per word
            
            //memset (wordp, 0, * tally * sizeof (* wordp));
            while (* tally && console_state . readp < console_state . tailp)
              {
                int charno;
                for (charno = 0; charno < 4; ++charno)
                  {
                    if (console_state . readp >= console_state . tailp)
                      break;
                    unsigned char c = * console_state . readp ++;
                    putbits36 (wordp, charno * 9, 9, c);
                  }
                * cp = charno % 4;
                if (1)
                  {
                    char msg[17];
                    for (int charno = 0; charno < 4; ++charno)
                      {
                        unsigned char c = getbits36(*wordp, charno * 9, 9);
                        if (c <= 0177 && isprint(c))
                            sprintf(msg+charno*4, " '%c'", c);
                        else
                            sprintf(msg+charno*4, "\\%03o", c);
                      }
                    msg[16] = 0;
                    sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Returning word %012llo: %s\n", *wordp, msg);
                  }

                wordp ++;
                (* tally) --;
              }
            if (console_state . readp < console_state . tailp)
              {
                sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: discarding %ld characters from end of line\n", console_state . tailp - console_state . readp);
              }
            console_state . readp = console_state . buf;
            console_state . tailp = console_state . buf;
            console_state . have_eol = false;
#endif
            * stati = 04000;
            return 0;
          }
            
        case write_mode:
          {
            
//-- #if 0
//--             char buf[40];   // max four "\###" sequences
//--             *buf = 0;
//--             t_uint64 word = *wordp;
//--             if ((word >> 36) != 0) {
//--                 sim_debug (DBG_ERR, & opcon_dev, "con_iom_io: Word %012llo has more than 36 bits.\n", word);
//--                 cancel_run(STOP_BUG);
//--                 word &= MASK36;
//--             }
//--             int err = 0;
//--             for (int i = 0; i < 4; ++i) {
//--                 uint c = word >> 27;
//--                 word = (word << 9) & MASKBITS(36);
//--                 if (c <= 0177 && isprint(c)) {
//--                     sprintf(buf+strlen(buf), "%c", c);
//--                     err |= sim_putchar(c);
//--                 } else {
//--                     sprintf(buf+strlen(buf), "\\%03o", c);
//--                     // WARNING: may send junk to the console.
//--                     // Char 0177 is used by Multics as non-printing padding
//--                     // (typically after a CRNL as a delay; see syserr_real.pl1).
//--                     if (c != 0 && c != 0177)
//--                         err |= sim_putchar(c);
//--                 }
//--             }
//--             if (err)
//--               {
//--                 sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: Error writing to CONSOLE\n");
//--               }
//--             sim_printf("CONSOLE: %s\n", buf);
//-- #else
//--             if (writeBufCnt >= writeBufSize)
//--               {
//--                 sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: writeBufOverflow; discarding word\n");
//--               }
//--             else
//--               {
//--                 writeBuf [writeBufCnt ++] =  *wordp;
//--               }
//-- #endif
//-- 
            sim_printf ("CONSOLE: ");
            //sim_printf ("tally %d\n", * tally);
            word36 datum;
// Tally is in words, not chars.
#if 0
            int nchars = 0;
            while (* tally)
              {
                if (nchars <= 0)
                  {
                    datum = * wordp ++;
                    nchars = 4;
                  }
                word36 wide_char = datum >> 27; // slide leftmost char into low byte
                datum = datum << 9; // lose the leftmost char
                nchars --;
                char ch = wide_char & 0x7f;
                sim_printf ("%c", ch);
                if (ch == '\r')
                  sim_printf ("\n");
                (* tally) --;
              }
#else
            while (* tally)
              {
                datum = * wordp ++;
                (* tally) --;

                for (int i = 0; i < 4; i ++)
                  {
                    word36 wide_char = datum >> 27; // slide leftmost char into low byte
                    datum = datum << 9; // lose the leftmost char
                    char ch = wide_char & 0x7f;
                    if (isprint (ch))
                      sim_printf ("%c", ch);
                    else
                      {
                        if (ch && ch != '\015' && ch != '\012' && ch != '\177')
                          sim_printf ("\\%03o", ch);
                      }
                    //if (ch == '\r')
                      //sim_printf ("\n");
                  }
              }
            sim_printf ("\n");
#endif
            * stati = 04000;
            return 0;
          }
            
        default:
          sim_debug (DBG_ERR, & opcon_dev, "%s: Console is in unknown mode %d\n", __func__, console_state . io_mode);
          * stati = 0501;
          return 1;
      }
    // return 0;
  }
#endif

//-- // The IOM will send a fault to the device for TRO and/or PTRO
//-- // pre ? PTRO : TRO
//-- int con_iom_fault(int chan, bool pre)
//--   {
//--     sim_printf ("con_iom_fault %s\n", pre ? "PTRO" : "TRO");
//-- 
//--     sim_printf ("writeBufCnt %d\n", writeBufCnt);
//-- 
//--     for (int i = 0; i < writeBufCnt; i ++)
//--       {
//--         sim_printf ("  %012llo\n", writeBuf [i]);
//--       }
//-- 
//--     return 0;
//--   }
//-- 
//-- // ============================================================================

/*
 * check_keyboard()
 *
 * Check simulated keyboard and transfer input to buffer.
 *
 * FIXME: We allow input even when the console is not in input mode (but we're
 * not really connected via a half-duplex channel either).
 *
 * TODO: Schedule this to run even when no console I/O is pending -- this
 * will allow the user to see type-ahead feedback.
 */

static void check_keyboard (void)
  {
    //DEVICE * devp = get_iom_channel_dev (cables_from_ioms_to_con [OPCON_UNIT_NUM] . iom_unit_num, chan, 0, NULL);
    //if (devp == NULL)
      //{
        //sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: No device\n");
        //return;
      //}
//--     chan_devinfo* devinfop = devp->ctxt;
//--     if (devinfop == NULL) {
//--         sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: No device info\n");
//--         return;
//--     }
//--     con_state_t *con_statep = devinfop->statep;
//--     if (con_statep == NULL) {
//--         sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: No state\n");
//--         return;
//--     }
//--     
    int announce = 1;
    for (;;)
      {
        if (console_state . tailp >= console_state . buf + sizeof(console_state . buf))
         {
            sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: Buffer full; ignoring keyboard.\n");
            return;
        }
        if (console_state . have_eol)
            return;
        int c;
        if (console_state . io_mode == read_mode && console_state . autop != NULL)
          {
            c = * (console_state . autop);
            if (c == 0)
              {
                //console_state . have_eol = true;
                free(console_state . auto_input);
                console_state . auto_input = NULL;
                console_state . autop = NULL;
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got auto-input EOS\n");
                goto poll; // return;
              }
            if (announce)
              {
                //sim_printf ("[auto-input] ");
                sim_puts ("[auto-input] ");
                announce = 0;
              }
            if (c == '\005')
              {
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got <sim stop>\n");
                return; // User typed ^E to stop simulation
              }
            ++ console_state . autop;
            if (isprint (c))
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Used auto-input char '%c'\n", c);
            else
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Used auto-input char '\\%03o'\n", c);
          }
        else
          {
poll:
            c = sim_poll_kbd();
            if (c == SCPE_OK)
                return; // no input
            if (c == SCPE_STOP)
              {
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got <sim stop>\n");
                return; // User typed ^E to stop simulation
              }
            if (c < SCPE_KFLAG)
              {
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Bad char\n");
                return; // Should be impossible
              }
            c -= SCPE_KFLAG;    // translate to ascii
            
            if (isprint (c))
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got char '%c'\n", c);
            else
                sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got char '\\%03o'\n", c);
            
            // FIXME: We don't allow user to set editing characters
            if (c == '\177' || c == '\010')
              {
                if (console_state . tailp > console_state . buf)
                  {
                    -- console_state . tailp;
                    sim_putchar (c);
                  }
              }
          }
        if (c == '\014')  // Form Feed, \f, ^L
          {
            sim_putchar('\n');
            sim_putchar('\r');
            for (const char * p = console_state . buf; p < console_state . tailp; ++p)
              sim_putchar (* p);
          }
        else if (c == '\012' || c == '\015')
          {
            // 012: New line, '\n', ^J
            // 015: Carriage Return, \r, ^M
#if 0
            // Transfer a NL to the buffer
            // bce_command_processor_ looks for newlines but not CRs
            *console_state . tailp++ = 012;
#endif
            // sim_putchar(c);
            sim_putchar ('\n');
            sim_putchar ('\r');
            console_state . have_eol = true;
            sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got EOL\n");
            return;
          }
        else
          {
            * console_state . tailp ++ = c;
            sim_putchar (c);
          }
      }
  }
