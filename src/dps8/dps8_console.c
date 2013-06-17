//
//  dps8_console.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>

#include "dps8.h"

/*
 console.c -- operator's console
 
 CAVEAT
 This code has not been updated to use the generalized async handling
 that was added to the IOM code.  Instead it blocks.
 
 See manual AN87.  See also mtb628.
 
 */
/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

#include <ctype.h>
#include <time.h>
#include <unistd.h>
//#include "hw6180.h"

extern iom_t iom;
extern DEVICE *sim_devices[];

typedef struct s_console_state {
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

static void check_keyboard(int chan);

// ============================================================================

void console_init()
{
}

// ============================================================================

static DEVICE* find_opcon()
{
    const char* moi = "opcon";
    
    DEVICE *devp = NULL;
    for (DEVICE **devpp = sim_devices; *devpp != NULL; ++devpp) {
        if (strcmp((*devpp)->name, "OPCON") == 0) {
            if (devp == NULL)
                devp = *devpp;
            else {
                log_msg(ERR_MSG, moi, "Multiple OPCON devices found.\n");
                return NULL;
            }
        }
    }
    
    if (devp == NULL) {
        log_msg(ERR_MSG, moi, "No OPCON devices found.\n");
        return NULL;
    }
    chan_devinfo* devinfop = devp->ctxt;
    if (devinfop == NULL) {
        // log_msg(ERR_MSG, moi, "Internal error, no context info for OPCON.\n");
        // return NULL;
        log_msg(INFO_MSG, moi, "Creating OPCON devinfo.\n");
        devinfop = malloc(sizeof(*devinfop));
        if (devinfop == NULL)
            return NULL;
        devinfop->chan = -1;
        devinfop->statep = NULL;
        devp->ctxt = devinfop;
    }
    struct s_console_state *con_statep = devinfop->statep;
    if (con_statep == NULL) {
        if ((con_statep = malloc(sizeof(struct s_console_state))) == NULL) {
            log_msg(ERR_MSG, moi, "Internal error, malloc failed.\n");
            return NULL;
        }
        devinfop->statep = con_statep;
        con_statep->io_mode = no_mode;
        con_statep->tailp = con_statep->buf;
        con_statep->readp = con_statep->buf;
        con_statep->have_eol = 0;
        con_statep->auto_input = NULL;
        con_statep->autop = NULL;
    }
    return devp;
}

// ============================================================================

int opcon_autoinput_set(UNIT *uptr, int32 val, char *cptr, void *desc)
{
    DEVICE *devp = find_opcon();
    if (devp == NULL)
        return 1;
    chan_devinfo* devinfop = devp->ctxt;
    struct s_console_state *con_statep = devinfop->statep;
    if (con_statep->auto_input) {
        log_msg(NOTIFY_MSG, "opcon", "Discarding prior auto-input.\n");
        free(con_statep->auto_input);
    }
    if (cptr) {
        con_statep->auto_input = strdup(cptr);
        log_msg(NOTIFY_MSG, "opcon", "Auto-input now: %s\n", cptr);
    } else {
        con_statep->auto_input = NULL;
        log_msg(NOTIFY_MSG, "opcon", "Auto-input disabled.\n");
    }
    con_statep->autop = con_statep->auto_input;
    return 0;
}

// ============================================================================

int opcon_autoinput_show(FILE *st, UNIT *uptr, int val, void *desc)
{
    log_msg(NOTIFY_MSG, "opcon_autoinput_show", "FILE=%p, uptr=%p, val=%d,desc=%p\n",
            st, uptr, val, desc);
    
    DEVICE *devp = find_opcon();
    if (devp == NULL)
        return 1;
    chan_devinfo* devinfop = devp->ctxt;
    struct s_console_state *con_statep = devinfop->statep;
    if (con_statep->auto_input == NULL)
        log_msg(NOTIFY_MSG, "opcon", "No auto-input exists.\n");
    else
        log_msg(NOTIFY_MSG, "opcon", "Auto-input is/was: %s\n", con_statep->auto_input);
    
    return 0;
}

// ============================================================================

/*
 * con_check_args()
 *
 * Internal function to do sanity checks
 */

static int con_check_args(const char* moi, int chan, int dev_code, int* majorp, int* subp, DEVICE **devpp, con_state_t **statepp)
{
    
    if (chan < 0 || chan >= ARRAY_SIZE(iom.channels)) {
        *majorp = 05;   // Real HW could not be on bad channel
        *subp = 1;
        log_msg(ERR_MSG, moi, "Bad channel %d\n", chan);
        return 1;
    }
    
    *devpp = iom.channels[chan].dev;
    DEVICE *devp = *devpp;
    if (devpp == NULL) {
        *majorp = 05;
        *subp = 1;
        log_msg(ERR_MSG, moi, "Internal error, no device for channel 0%o\n", chan);
        return 1;
    }
    chan_devinfo* devinfop = devp->ctxt;
    if (devinfop == NULL) {
        *majorp = 05;
        *subp = 1;
        log_msg(ERR_MSG, moi, "Internal error, no device info for channel 0%o\n", chan);
        return 1;
    }
    struct s_console_state *con_statep = devinfop->statep;
    if (dev_code != 0) {
        // Consoles don't have units
        *majorp = 05;
        *subp = 1;
        log_msg(ERR_MSG, moi, "Bad dev unit-num 0%o (%d decimal)\n", dev_code, dev_code);
        return 1;
    }
    if (con_statep == NULL) {
        if ((con_statep = malloc(sizeof(struct s_console_state))) == NULL) {
            log_msg(ERR_MSG, moi, "Internal error, malloc failed.\n");
            return 1;
        }
        devinfop->statep = con_statep;
        con_statep->io_mode = no_mode;
        con_statep->tailp = con_statep->buf;
        con_statep->readp = con_statep->buf;
        con_statep->have_eol = 0;
        con_statep->auto_input = NULL;
        con_statep->autop = NULL;
    }
    *statepp = con_statep;
    return 0;
}


// ============================================================================


/*
 * con_iom_cmd()
 *
 * Handle a device command.  Invoked by the IOM while processing a PCW
 * or IDCW.
 */

int con_iom_cmd(int chan, int dev_cmd, int dev_code, int* majorp, int* subp)
{
    log_msg(DEBUG_MSG, "CON::iom_cmd", "Chan 0%o, dev-cmd 0%o, dev-code 0%o\n", chan, dev_cmd, dev_code);
    
    // FIXME: Should Major be added to 040? and left shifted 6? Ans: it's 4 bits
    
    DEVICE* devp;
    con_state_t* con_statep;
    if (con_check_args("CON::iom_cmd", chan, dev_code, majorp, subp, &devp, &con_statep) != 0)
        return 1;
    
    check_keyboard(chan);
    
    switch(dev_cmd) {
        case 0: {               // CMD 00 Request status
            log_msg(NOTIFY_MSG, "CON::iom_cmd", "Status request cmd received");
            *majorp = 0;
            *subp = 0;
            return 0;
        }
        case 023:               // Read ASCII
            con_statep->io_mode = read_mode;
            log_msg(NOTIFY_MSG, "CON::iom_cmd", "Read ASCII command received\n");
            if (con_statep->tailp != con_statep->buf)
                log_msg(WARN_MSG, "CON::iom_cmd", "Discarding previously buffered input.\n");
            // TODO: discard any buffered chars from SIMH?
            con_statep->tailp = con_statep->buf;
            con_statep->readp = con_statep->buf;
            con_statep->have_eol = 0;
            *majorp = 00;
            *subp = 0;
            // breakpoint not helpful as cmd is probably in a list with an IO
            // cancel_run(STOP_IBKPT);
            // log_msg(NOTIFY_MSG, "CON::iom_cmd", "Auto-breakpoint for read.\n");
            return 0;
        case 033:               // Write ASCII
            con_statep->io_mode = write_mode;
            log_msg(NOTIFY_MSG, "CON::iom_cmd", "Write ASCII cmd received\n");
            if (con_statep->tailp != con_statep->buf)
                log_msg(WARN_MSG, "CON::iom_cmd", "Might be discarding previously buffered input.\n");
            *majorp = 00;
            *subp = 0;
            return 0;
        case 040:               // Reset
            log_msg(NOTIFY_MSG, "CON::iom_cmd", "Reset cmd received\n");
            con_statep->io_mode = no_mode;
            *majorp = 0;
            *subp = 0;
            return 0;
        case 051:               // Write Alert -- Ring Bell
            // AN70-1 says only console channels respond to this command
            out_msg("CONSOLE: ALERT\n");
            log_msg(NOTIFY_MSG, "CON::iom_cmd", "Write Alert cmd received\n");
            sim_putchar('\a');
            *majorp = 0;
            *subp = 0;
            return 0;
        case 057:               // Read ID (according to AN70-1)
            // FIXME: No support for Read ID; appropriate values are not known
            log_msg(ERR_MSG, "CON::iom_cmd", "Read ID unimplemented\n");
            *majorp = 05;
            *subp = 1;
            return 1;
        default: {
            *majorp = 05;   // command reject
            *subp = 1;      // invalid instruction code
            log_msg(ERR_MSG, "CON::iom_cmd", "Unknown command 0%o\n", dev_cmd);
            cancel_run(STOP_BUG);
            return 1;
        }
    }
    return 1;   // not reached
}

// ============================================================================

/*
 * con_iom_io()
 *
 * Handle an I/O request.  Invoked by the IOM while processing a DDCW.
 */

int con_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp)
{
    const char *moi = "CON::iom_io";
    log_msg(DEBUG_MSG, "CON::iom_io", "Chan 0%o\n", chan);
    
    DEVICE* devp;
    con_state_t* con_statep;
    const int dev_code = 0;
    if (con_check_args("CON::iom_cmd", chan, dev_code, majorp, subp, &devp, &con_statep) != 0)
        return 1;
    
    check_keyboard(chan);
    
    switch (con_statep->io_mode) {
        case no_mode:
            log_msg(ERR_MSG, "CON::iom_io", "Console is uninitialized\n");
            *majorp = 05;       // 05 -- Command Reject
            *subp = 1;          // 01 Invalid Instruction Code
            return 1;
            
        case read_mode: {
            // Read keyboard if we don't have an EOL from the operator
            // yet
            if (! con_statep->have_eol) {
                // We won't return anything to the IOM until the operator
                // has finished entering a full line and pressed ENTER.
                log_msg(NOTIFY_MSG, moi, "Starting input loop for channel %d (%#o)\n", chan, chan);
                time_t now = time(NULL);
                while (time(NULL) < now + 30 && ! con_statep->have_eol) {
                    check_keyboard(chan);
                    usleep(100000);       // FIXME: blocking
                }
                // Impossible to both have EOL and have buffer overflow
                if (con_statep->tailp >= con_statep->buf + sizeof(con_statep->buf)) {
                    *majorp = 03;       // 03 -- Data Alert
                    *subp = 040;        // 10 -- Message length alert
                    log_msg(NOTIFY_MSG, "CON::iom_io", "buffer overflow\n");
                    cancel_run(STOP_BKPT);
                    return 1;
                }
                if (! con_statep->have_eol) {
                    *majorp = 03;       // 03 -- Data Alert
                    *subp = 010;        // 10 -- Operator distracted (30 sec timeout)
                    log_msg(NOTIFY_MSG, "CON::iom_io", "Operator distracted (30 second timeout\n");
                    cancel_run(STOP_BKPT);
                }
            }
            // We have an EOL from the operator
            log_msg(NOTIFY_MSG, moi, "Transfer for channel %d (%#o)\n", chan, chan);
            // bce_command_processor_ expects multiples chars per word
            for (int charno = 0; charno < 4; ++charno) {
                if (con_statep->readp >= con_statep->tailp)
                    break;
                unsigned char c = *con_statep->readp++;
                *wordp = setbits36(*wordp, charno * 9, 9, c);
            }
            if (1) {
                char msg[17];
                for (int charno = 0; charno < 4; ++charno) {
                    unsigned char c = getbits36(*wordp, charno * 9, 9);
                    if (c <= 0177 && isprint(c))
                        sprintf(msg+charno*4, " '%c'", c);
                    else
                        sprintf(msg+charno*4, "\\%03o", c);
                }
                msg[16] = 0;
                log_msg(NOTIFY_MSG, moi, "Returning word %012llo: %s\n", *wordp, msg);
            }
            int ret;
            if (con_statep->readp == con_statep->tailp) {
                con_statep->readp = con_statep->buf;
                con_statep->tailp = con_statep->buf;
                // con_statep->have_eol = 0;
                log_msg(WARN_MSG, moi, "Entire line now transferred.\n");
                ret = 1;    // FIXME: out of band request to return
            } else {
                log_msg(WARN_MSG, moi, "%d chars remain to be transfered.\n", con_statep->tailp - con_statep->readp);
                ret = 0;
            }
            *majorp = 0;
            *subp = 0;
            log_msg(WARN_MSG, moi, "Auto breakpoint.\n");
            cancel_run(STOP_BKPT);
            return ret;
        }
            
        case write_mode: {
            
            char buf[40];   // max four "\###" sequences
            *buf = 0;
            t_uint64 word = *wordp;
            if ((word >> 36) != 0) {
                log_msg(ERR_MSG, "CON::iom_io", "Word %012llo has more than 36 bits.\n", word);
                cancel_run(STOP_BUG);
                word &= MASK36;
            }
            int err = 0;
            for (int i = 0; i < 4; ++i) {
                uint c = word >> 27;
                word = (word << 9) & MASKBITS(36);
                if (c <= 0177 && isprint(c)) {
                    sprintf(buf+strlen(buf), "%c", c);
                    err |= sim_putchar(c);
                } else {
                    sprintf(buf+strlen(buf), "\\%03o", c);
                    // WARNING: may send junk to the console.
                    // Char 0177 is used by Multics as non-printing padding
                    // (typically after a CRNL as a delay; see syserr_real.pl1).
                    if (c != 0 && c != 0177)
                        err |= sim_putchar(c);
                }
            }
            if (err)
                log_msg(WARN_MSG, "CON::iom_io", "Error writing to CONSOLE\n");
            out_msg("CONSOLE: %s\n", buf);
            
            *majorp = 0;
            *subp = 0;
            
            return 0;
        }
            
        default:
            log_msg(ERR_MSG, "CON::iom_io", "Console is in unknown mode %d\n", con_statep->io_mode);
            *majorp = 05;
            *subp = 1;
            return 1;
    }
}

// ============================================================================

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

static void check_keyboard(int chan)
{
    const char* moi = "CON::input";
    
    if (chan < 0 || chan >= ARRAY_SIZE(iom.channels)) {
        log_msg(WARN_MSG, moi, "Bad channel\n");
        return;
    }
    DEVICE* devp = iom.channels[chan].dev;
    if (devp == NULL) {
        log_msg(WARN_MSG, moi, "No device\n");
        return;
    }
    chan_devinfo* devinfop = devp->ctxt;
    if (devinfop == NULL) {
        log_msg(WARN_MSG, moi, "No device info\n");
        return;
    }
    struct s_console_state *con_statep = devinfop->statep;
    if (con_statep == NULL) {
        log_msg(WARN_MSG, moi, "No state\n");
        return;
    }
    
    int announce = 1;
    for (;;) {
        if (con_statep->tailp >= con_statep->buf + sizeof(con_statep->buf)) {
            log_msg(WARN_MSG, moi, "Buffer full; ignoring keyboard.\n");
            return;
        }
        if (con_statep->have_eol)
            return;
        int c;
        if (con_statep->io_mode == read_mode && con_statep->autop != NULL) {
            if (announce) {
                const char *msg = "[auto-input] ";
                for (const char *s = msg; *s != 0; ++s)
                    sim_putchar(*s);
                announce = 0;
            }
            c = *(con_statep->autop);
            if (c == 0) {
                con_statep->have_eol = 1;
                free(con_statep->auto_input);
                con_statep->auto_input = NULL;
                con_statep->autop = NULL;
                log_msg(NOTIFY_MSG, moi, "Got auto-input EOL for channel %d (%#o)\n", chan, chan);
                return;
            }
            ++ con_statep->autop;
            if (isprint(c))
                log_msg(NOTIFY_MSG, moi, "Used auto-input char '%c'\n", c);
            else
                log_msg(NOTIFY_MSG, moi, "Used auto-input char '\\%03o'\n", c);
        } else {
            c = sim_poll_kbd();
            if (c == SCPE_OK)
                return; // no input
            if (c == SCPE_STOP) {
                log_msg(NOTIFY_MSG, moi, "Got <sim stop>\n");
                return; // User typed ^E to stop simulation
            }
            if (c < SCPE_KFLAG) {
                log_msg(NOTIFY_MSG, moi, "Bad char\n");
                return; // Should be impossible
            }
            c -= SCPE_KFLAG;    // translate to ascii
            
            if (isprint(c))
                log_msg(NOTIFY_MSG, moi, "Got char '%c'\n", c);
            else
                log_msg(NOTIFY_MSG, moi, "Got char '\\%03o'\n", c);
            
            // FIXME: We don't allow user to set editing characters
            if (c == '\177' || c == '\010') {
                if (con_statep->tailp > con_statep->buf) {
                    -- con_statep->tailp;
                    sim_putchar(c);
                }
            }
        }
        if (c == '\014') {  // Form Feed, \f, ^L
            sim_putchar('\r');
            sim_putchar('\n');
            for (const char *p = con_statep->buf; p < con_statep->tailp; ++p)
                sim_putchar(*p);
        } else if (c == '\012' || c == '\015') {
            // 012: New line, '\n', ^J
            // 015: Carriage Return, \r, ^M
#if 0
            // Transfer a NL to the buffer
            // bce_command_processor_ looks for newlines but not CRs
            *con_statep->tailp++ = 012;
#endif
            // sim_putchar(c);
            sim_putchar('\r');
            sim_putchar('\n');
            con_statep->have_eol = 1;
            log_msg(NOTIFY_MSG, moi, "Got EOL for channel %d (%#o)\n", chan, chan);
            return;
        } else {
            *con_statep->tailp++ = c;
            sim_putchar(c);
        }
    }
}
