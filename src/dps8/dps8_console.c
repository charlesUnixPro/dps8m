//
//  dps8_console.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>

#include "dps8.h"
#include "dps8_iom.h"

/*
 console.c -- operator's console
 
 CAVEAT
 This code has not been updated to use the generalized async handling
 that was added to the IOM code.  Instead it blocks.
 
 See manual AN87.  See also mtb628.
 
 */

/* config switch -- The bootload console has a 30-second timer mechanism. When
reading from the console, if no character is typed within 30 seconds, the read
operation is terminated. The timer is controlled by an enable switch, must be
set to enabled during Multics and BCE */

MTAB opcon_mod[] = {
    { MTAB_XTD | MTAB_VDV | MTAB_VALO | MTAB_NC,
        0, NULL, "AUTOINPUT",
        opcon_autoinput_set, opcon_autoinput_show, NULL },
    { 0 }
};


static DEBTAB opcon_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };


// Multics only supports a single operator console

#define N_OPCON_UNITS 1
#define OPCON_UNIT_NUM 0

UNIT opcon_unit [N_OPCON_UNITS] = {{ UDATA(NULL, 0, 0) }};

static t_stat opcon_reset (DEVICE * dptr);

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
    NULL           /* logical name */
};

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

//-- #include <ctype.h>
//-- #include <time.h>
//-- #include <unistd.h>
//-- //#include "hw6180.h"
//-- 
typedef struct s_console_state
  {
    // Hangs off the device structure
    enum { no_mode, read_mode, write_mode } io_mode;
    // SIMH console library has only putc and getc; the SIMH terminal
    // library has more features including line buffering.
    char buf[81];
    char *tailp;
    char *readp;
    flag_t have_eol;
    char *auto_input;
    char *autop;
 } con_state_t;

// We only support a single console instance, so this should be okay.

static con_state_t console_state;

//-- #define N_LINES 4

static struct
  {
    int iom_unit_num;
    int chan_num;
  } cables_from_ioms_to_con [N_OPCON_UNITS];

// Buffer to accumulate Write ASCII
#define writeBufSize 4096
static word36 writeBuf [writeBufSize];
static int writeBufCnt;

//-- static void check_keyboard(int chan);
//-- 

static int con_iom_cmd (UNIT * unitp, pcw_t * p, word12 * stati, bool * need_data);
static int con_iom_io (UNIT * unitp, int chan, int dev_code, uint * tally, t_uint64 * wordp, word12 * stati);

//-- // ============================================================================

static t_stat opcon_reset (DEVICE * dptr)
  {
    console_state . io_mode = no_mode;
    console_state . tailp = console_state . buf;
    console_state . readp = console_state . buf;
    console_state . have_eol = 0;
    console_state . auto_input = NULL;
    console_state . autop = NULL;
    return SCPE_OK;
  }

void console_init()
{
    for (int i = 0; i < N_OPCON_UNITS; i ++)
      cables_from_ioms_to_con [i] . iom_unit_num = -1;
}

t_stat cable_opcon (int iom_unit_num, int chan_num)
  {
    if (cables_from_ioms_to_con [OPCON_UNIT_NUM] . iom_unit_num != -1)
      {
        sim_printf ("cable_opcon: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, 0, DEVT_CON, chan_type_CPI, OPCON_UNIT_NUM, & opcon_dev, & opcon_unit [OPCON_UNIT_NUM], con_iom_cmd, con_iom_io);
    if (rc)
      return rc;

    cables_from_ioms_to_con [OPCON_UNIT_NUM] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_con [OPCON_UNIT_NUM] . chan_num = chan_num;

    return SCPE_OK;
  }

int opcon_autoinput_set(UNIT *uptr, int32 val, char *cptr, void *desc)
{
//--     DEVICE *devp = find_opcon();
//--     if (devp == NULL)
//--         return 1;
//--     chan_devinfo* devinfop = devp->ctxt;
//--     struct s_console_state *con_statep = devinfop->statep;
//--     if (con_statep->auto_input) {
//--         sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Discarding prior auto-input., __func__\n");
//--         free(con_statep->auto_input);
//--     }
//--     if (cptr) {
//--         con_statep->auto_input = strdup(cptr);
//--         sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Auto-input now: %s\n", cptr, __func__);
//--     } else {
//--         con_statep->auto_input = NULL;
//--         sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Auto-input disabled.\n", __func__);
//--     }
//--     con_statep->autop = con_statep->auto_input;
    return SCPE_OK;
}

int opcon_autoinput_show(FILE *st, UNIT *uptr, int val, void *desc)
{
//--     sim_debug (DBG_NOTIFY, & opcon_dev, "%s: FILE=%p, uptr=%p, val=%d,desc=%p\n",
//--             __func__, st, uptr, val, desc);
//--     
//--     DEVICE *devp = find_opcon();
//--     if (devp == NULL)
//--         return 1;
//--     chan_devinfo* devinfop = devp->ctxt;
//--     struct s_console_state *con_statep = devinfop->statep;
//--     if (con_statep->auto_input == NULL)
//--         sim_debug (DBG_NOTIFY, & opcon_dev, "%s: No auto-input exists.\n", __func__);
//--     else
//--         sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Auto-input is/was: %s\n", __func__, con_statep->auto_input);
//--     
    return SCPE_OK;
}
//-- 
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
//--     struct s_console_state *con_statep = devinfop->statep;
//--     if (dev_code != 0) {
//--         // Consoles don't have units
//--         *majorp = 05;
//--         *subp = 1;
//--         sim_debug (DBG_ERR, & opcon_dev, "con_check_args: Bad dev unit-num 0%o (%d decimal)\n", dev_code, dev_code);
//--         return 1;
//--     }
//--     if (con_statep == NULL) {
//--         if ((con_statep = malloc(sizeof(struct s_console_state))) == NULL) {
//--             sim_debug (DBG_ERR, & opcon_dev, "con_check_args: Internal error, malloc failed.\n");
//--             return 1;
//--         }
//--         devinfop->statep = con_statep;
//--         con_statep->io_mode = no_mode;
//--         con_statep->tailp = con_statep->buf;
//--         con_statep->readp = con_statep->buf;
//--         con_statep->have_eol = 0;
//--         con_statep->auto_input = NULL;
//--         con_statep->autop = NULL;
//--     }
//--     *statepp = con_statep;
//--     return 0;
//-- }
//-- 
//-- 
//-- // ============================================================================
//-- 

/*
 * con_iom_cmd()
 *
 * Handle a device command.  Invoked by the IOM while processing a PCW
 * or IDCW.
 */

static int con_iom_cmd (UNIT * unitp, pcw_t * p, word12 * stati, bool * need_data)
  {
    * need_data = false;
//--     int chan = opcon_unit -> u3;
//--     int dev_code = opcon_unit -> u4;
//--     //int iom_unit_num = opcon_unit -> u5;
//-- 
//--     sim_debug (DBG_DEBUG, & opcon_dev, "%s: Chan 0%o, dev-cmd 0%o, dev-code 0%o\n", __func__, chan, dev_cmd, dev_code);
//--     
//--     // FIXME: Should Major be added to 040? and left shifted 6? Ans: it's 4 bits
//--     
//--     DEVICE* devp;
//--     con_state_t* con_statep;
//--     if (con_check_args("CON::iom_cmd", chan, dev_code, majorp, subp, &devp, &con_statep) != 0)
//--         return 1;
//--     
//--     check_keyboard(chan);
//--     
    switch (p -> dev_cmd)
      {
        case 0: // CMD 00 Request status
          {
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Status request cmd received",
                       __func__);
            * stati = 04000;
            return 0;
        }

//--         case 023:               // Read ASCII
//--             con_statep->io_mode = read_mode;
//--             sim_debug (DBG_NOTIFY, & opcon_dev, "%s: Read ASCII command received\n", __func__);
//--             if (con_statep->tailp != con_statep->buf)
//--               {
//--                 sim_debug (DBG_WARN, & opcon_dev, "%s: Discarding previously buffered input.\n", __func__);
//--               }
//--             // TODO: discard any buffered chars from SIMH?
//--             con_statep->tailp = con_statep->buf;
//--             con_statep->readp = con_statep->buf;
//--             con_statep->have_eol = 0;
//--             *majorp = 00;
//--             *subp = 0;
//--             // breakpoint not helpful as cmd is probably in a list with an IO
//--             // cancel_run(STOP_IBKPT);
//--             // sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_cmd: Auto-breakpoint for read.\n");
//--             return 0;

        case 033:               // Write ASCII
            console_state . io_mode = write_mode;
            writeBufCnt = 0;

            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Write ASCII cmd received\n", __func__);
            if (console_state . tailp != console_state . buf)
              {
                sim_debug (DBG_WARN, & opcon_dev, "con_iom_cmd: Might be discarding previously buffered input.\n");
              }
            * stati = 00;
            * need_data = true;
            return 0;

        case 040:               // Reset
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Reset cmd received\n", __func__);
            console_state . io_mode = no_mode;
            * stati = 04000;
            return 0;

        case 051:               // Write Alert -- Ring Bell
            // AN70-1 says only console channels respond to this command
            sim_printf ("CONSOLE: ALERT\n");
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Write Alert cmd received\n", __func__);
            sim_putchar('\a');
            * stati = 04000;
            return 0;

        case 057:               // Read ID (according to AN70-1)
            // FIXME: No support for Read ID; appropriate values are not known
            // [CAC] Looking at the bootload console code, it seems more 
            // concerned about the device responding, rather then the actual
            // returned value. Make some thing up.
            sim_debug (DBG_NOTIFY, & opcon_dev,
                       "%s: Read ID received\n", __func__);
            //*majorp = 05;
            //*subp = 1;
            * stati = 04500;
            return 0;

        default:
          {
            * stati = 04501; // command reject, invalid instruction code
            sim_debug (DBG_ERR, & opcon_dev, "%s: Unknown command 0%o\n",
                       __func__, p -> dev_cmd);
            return 1;
          }
      }
    return 1;   // not reached
  }


/*
 * con_iom_io()
 *
 * Handle an I/O request.  Invoked by the IOM while processing a DDCW.
 */

static int con_iom_io (UNIT * unitp, int chan, int dev_code, uint * tally, t_uint64 * wordp, word12 * stati)
{
    sim_debug (DBG_DEBUG, & opcon_dev, "%s: Chan 0%o\n", __func__, chan);
// XXX
//--     
//--     DEVICE* devp;
//--     con_state_t* con_statep;
//--     const int dev_code = 0;
//--     if (con_check_args("CON::iom_cmd", chan, dev_code, majorp, subp, &devp, &con_statep) != 0)
//--         return 1;
//--     
//--     check_keyboard(chan);
    
    switch (console_state . io_mode)
      {
        case no_mode:
          sim_debug (DBG_ERR, & opcon_dev, "%s: Console is uninitialized\n", __func__);
          * stati = 05001;       // 05 -- Command Reject 01 Invalid Instruction Code
          return 1;
            
//--         case read_mode: {
//--             // Read keyboard if we don't have an EOL from the operator
//--             // yet
//--             if (! con_statep->have_eol) {
//--                 // We won't return anything to the IOM until the operator
//--                 // has finished entering a full line and pressed ENTER.
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Starting input loop for channel %d (%#o)\n", chan, chan);
//--                 time_t now = time(NULL);
//--                 while (time(NULL) < now + 30 && ! con_statep->have_eol) {
//--                     check_keyboard(chan);
//--                     usleep(100000);       // FIXME: blocking
//--                 }
//--                 // Impossible to both have EOL and have buffer overflow
//--                 if (con_statep->tailp >= con_statep->buf + sizeof(con_statep->buf)) {
//--                     *majorp = 03;       // 03 -- Data Alert
//--                     *subp = 040;        // 10 -- Message length alert
//--                     sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: buffer overflow\n");
//--                     cancel_run(STOP_BKPT);
//--                     return 1;
//--                 }
//--                 if (! con_statep->have_eol) {
//--                     *majorp = 03;       // 03 -- Data Alert
//--                     *subp = 010;        // 10 -- Operator distracted (30 sec timeout)
//--                     sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Operator distracted (30 second timeout\n");
//--                     cancel_run(STOP_BKPT);
//--                 }
//--             }
//--             // We have an EOL from the operator
//--             sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Transfer for channel %d (%#o)\n", chan, chan);
//--             // bce_command_processor_ expects multiples chars per word
//--             for (int charno = 0; charno < 4; ++charno) {
//--                 if (con_statep->readp >= con_statep->tailp)
//--                     break;
//--                 unsigned char c = *con_statep->readp++;
//--                 *wordp = setbits36(*wordp, charno * 9, 9, c);
//--             }
//--             if (1) {
//--                 char msg[17];
//--                 for (int charno = 0; charno < 4; ++charno) {
//--                     unsigned char c = getbits36(*wordp, charno * 9, 9);
//--                     if (c <= 0177 && isprint(c))
//--                         sprintf(msg+charno*4, " '%c'", c);
//--                     else
//--                         sprintf(msg+charno*4, "\\%03o", c);
//--                 }
//--                 msg[16] = 0;
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "con_iom_io: Returning word %012llo: %s\n", *wordp, msg);
//--             }
//--             int ret;
//--             if (con_statep->readp == con_statep->tailp) {
//--                 con_statep->readp = con_statep->buf;
//--                 con_statep->tailp = con_statep->buf;
//--                 // con_statep->have_eol = 0;
//--                 sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: Entire line now transferred.\n");
//--                 ret = 1;    // FIXME: out of band request to return
//--             } else {
//--                 sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: %ld chars remain to be transfered.\n", con_statep->tailp - con_statep->readp);
//--                 ret = 0;
//--             }
//--             *majorp = 0;
//--             *subp = 0;
//--             sim_debug (DBG_WARN, & opcon_dev, "con_iom_io: Auto breakpoint.\n");
//--             cancel_run(STOP_BKPT);
//--             return ret;
//--         }
            
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
            int nchars = 0;
            word36 datum;
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
                tally --;
              }
            * stati = 0;
            return 0;
          }
            
        default:
          sim_debug (DBG_ERR, & opcon_dev, "%s: Console is in unknown mode %d\n", __func__, console_state . io_mode);
          * stati = 0501;
          return 1;
      }
    return 0;
  }

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
//-- 
//-- /*
//--  * check_keyboard()
//--  *
//--  * Check simulated keyboard and transfer input to buffer.
//--  *
//--  * FIXME: We allow input even when the console is not in input mode (but we're
//--  * not really connected via a half-duplex channel either).
//--  *
//--  * TODO: Schedule this to run even when no console I/O is pending -- this
//--  * will allow the user to see type-ahead feedback.
//--  */
//-- 
//-- static void check_keyboard(int chan)
//-- {
//--     if (chan < 0 || chan >= max_channels) {
//--         sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: Bad channel\n");
//--         return;
//--     }
//--     DEVICE* devp = get_iom_channel_dev (cables_from_ioms_to_con [OPCON_UNIT_NUM] . iom_unit_num, chan, 0, NULL);
//--     if (devp == NULL) {
//--         sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: No device\n");
//--         return;
//--     }
//--     chan_devinfo* devinfop = devp->ctxt;
//--     if (devinfop == NULL) {
//--         sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: No device info\n");
//--         return;
//--     }
//--     struct s_console_state *con_statep = devinfop->statep;
//--     if (con_statep == NULL) {
//--         sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: No state\n");
//--         return;
//--     }
//--     
//--     int announce = 1;
//--     for (;;) {
//--         if (con_statep->tailp >= con_statep->buf + sizeof(con_statep->buf)) {
//--             sim_debug (DBG_WARN, & opcon_dev, "check_keyboard: Buffer full; ignoring keyboard.\n");
//--             return;
//--         }
//--         if (con_statep->have_eol)
//--             return;
//--         int c;
//--         if (con_statep->io_mode == read_mode && con_statep->autop != NULL) {
//--             if (announce) {
//--                 const char *msg = "[auto-input] ";
//--                 for (const char *s = msg; *s != 0; ++s)
//--                     sim_putchar(*s);
//--                 announce = 0;
//--             }
//--             c = *(con_statep->autop);
//--             if (c == 0) {
//--                 con_statep->have_eol = 1;
//--                 free(con_statep->auto_input);
//--                 con_statep->auto_input = NULL;
//--                 con_statep->autop = NULL;
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got auto-input EOL for channel %d (%#o)\n", chan, chan);
//--                 return;
//--             }
//--             ++ con_statep->autop;
//--             if (isprint(c))
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Used auto-input char '%c'\n", c);
//--             else
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Used auto-input char '\\%03o'\n", c);
//--         } else {
//--             c = sim_poll_kbd();
//--             if (c == SCPE_OK)
//--                 return; // no input
//--             if (c == SCPE_STOP) {
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got <sim stop>\n");
//--                 return; // User typed ^E to stop simulation
//--             }
//--             if (c < SCPE_KFLAG) {
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Bad char\n");
//--                 return; // Should be impossible
//--             }
//--             c -= SCPE_KFLAG;    // translate to ascii
//--             
//--             if (isprint(c))
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got char '%c'\n", c);
//--             else
//--                 sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got char '\\%03o'\n", c);
//--             
//--             // FIXME: We don't allow user to set editing characters
//--             if (c == '\177' || c == '\010') {
//--                 if (con_statep->tailp > con_statep->buf) {
//--                     -- con_statep->tailp;
//--                     sim_putchar(c);
//--                 }
//--             }
//--         }
//--         if (c == '\014') {  // Form Feed, \f, ^L
//--             sim_putchar('\r');
//--             sim_putchar('\n');
//--             for (const char *p = con_statep->buf; p < con_statep->tailp; ++p)
//--                 sim_putchar(*p);
//--         } else if (c == '\012' || c == '\015') {
//--             // 012: New line, '\n', ^J
//--             // 015: Carriage Return, \r, ^M
//-- #if 0
//--             // Transfer a NL to the buffer
//--             // bce_command_processor_ looks for newlines but not CRs
//--             *con_statep->tailp++ = 012;
//-- #endif
//--             // sim_putchar(c);
//--             sim_putchar('\r');
//--             sim_putchar('\n');
//--             con_statep->have_eol = 1;
//--             sim_debug (DBG_NOTIFY, & opcon_dev, "check_keyboard: Got EOL for channel %d (%#o)\n", chan, chan);
//--             return;
//--         } else {
//--             *con_statep->tailp++ = c;
//--             sim_putchar(c);
//--         }
//--     }
//-- }
