/**
 * \file dps8_iom.c
 * \project dps8
 * \date 9/21/12
 *  Adapted by Harry Reed on 9/21/12.
 */

#include <stdio.h>

#include "dps8.h"

#if ORIG
// console stuff

/* bootload_console$poll_for_console (used when there is no config deck)
 * tries channels 8-63 (10o-77o) on the tape IOM.
 * iom_setup (for now) sets this to 8.  It should be specified in
 * iom.conf
 */

int console_chan = 0;


// iom stuff ...

word18 iom_pos;

/* These are from iom_word_macros */

void decode_pcw(word36 pcw1, word36 pcw2) {
    word8 command, device, extension, chan_ctrl, chan_cmd, chan_data;
    word8 channel;
    int mask;
    printf("pcw ");
    /*
     if(((pcw1 & 0700000) != 0700000) ||
     (pcw2 & 0700000000000LL) || (pcw2 & 0000777777777LL))
     printf("(not actually a pcw) ");
     */
    command = (pcw1 & 0770000000000LL) >> 30;
    device = (pcw1 & 0007700000000LL) >> 24;
    extension = (pcw1 & 0000077000000LL) >> 18;
    mask = (pcw1 & 0040000) >> 14;
    chan_ctrl = (pcw1 & 0030000) >> 12;
    chan_cmd = (pcw1 & 0007700) >> 6;
    chan_data = pcw1 & 0000077;
    channel = (pcw2 & 0077000000000LL) >> 27;
    printf("cmd:%d ch:%d dev:%d %s %s chdat:%d ext:%d%s\n",
           command, channel, device,
           !chan_cmd ? "rec" :
           (chan_cmd == 2 ? "nondat" :
            (chan_cmd == 6 ? "multi" :
             (chan_cmd == 8 ? "char" : "?"))),
           !chan_ctrl ? "term" :
           (chan_ctrl == 2 ? "proceed" :
            (chan_ctrl == 3 ? "marker" : "?")),
           chan_data, extension, mask ? " +mask" : "");
}

void decode_ddcw(word36 ddcw) {
    word8 char_offset, op_type;
    int tally_type;
    word36 address;
    word18 tally;
    address = (ddcw & 0777777000000LL) >> 18;
    char_offset = (ddcw & 0000000700000LL) >> 15;
    tally_type = (ddcw & 0000000040000LL) >> 14;
    op_type = (ddcw & 0000000030000LL) >> 12;
    tally = ddcw & 0000000007777LL;
    printf("ddcw addr:%6.6llo tally:%4.4lo(%s) op:%s choff:%d\n",
           address, tally, tally_type ? "ch" : "word",
           !op_type ? "iotd" :
           (op_type == 1 ? "iotp" :
            (op_type == 3 ? "iontp" : "?")), char_offset);
}

void decode_idcw(word36 idcw) {
    word8 command, device, extension, chan_ctrl, chan_cmd, chan_data;
    int mask;
    printf("idcw ");
    /*
     if((idcw & 0700000) != 0700000)
     printf("(not actually an idcw) ");
     */
    command = (idcw & 0770000000000LL) >> 30;
    device = (idcw & 0007700000000LL) >> 24;
    extension = (idcw & 0000077000000LL) >> 18;
    mask = (idcw & 0040000) >> 14;
    chan_ctrl = (idcw & 0030000) >> 12;
    chan_cmd = (idcw & 0007700) >> 6;
    chan_data = idcw & 0000077;
    printf("cmd:%d dev:%d %s %s chdat:%d ext:%d%s\n",
           command, device,
           !chan_cmd ? "rec" :
           (chan_cmd == 2 ? "nondat" :
            (chan_cmd == 6 ? "multi" :
             (chan_cmd == 8 ? "char" : "?"))),
           !chan_ctrl ? "term" :
           (chan_ctrl == 2 ? "proceed" :
            (chan_ctrl == 3 ? "marker" : "?")),
           chan_data, extension, mask ? " +mask" : "");
}

void decode_tdcw(word36 tdcw) {
    word8 bits;
    word36 address;
    printf("tdcw ");
    /*
     if((tdcw & 0777770) != 0030000)
     printf("(not actually a tdcw) ");
     */
    address = (tdcw & 0777777000000LL) >> 18;
    bits = tdcw & 0000007;
    printf("addr:%6.6llo bits:%d\n", address, bits);
}

/* These are from AN87, sec 3 */

void decode_lpw(word36 lpw1, word36 lpw2) {
    word36 dcw_addr, idcwp;
    word18 bound, base, tally;
    int res, iom_rel, se, nc, tal, rel;
    dcw_addr = (lpw1 & 0777777000000LL) >> 18;
    res = (lpw1 & 0400000) >> 17;
    iom_rel = (lpw1 & 0400000) >> 16;
    se = (lpw1 & 0400000) >> 15;
    nc = (lpw1 & 0400000) >> 14;
    tal = (lpw1 & 0400000) >> 13; /* tal and rel might be swapped */
    rel = (lpw1 & 0400000) >> 12;
    tally = lpw1 & 0007777;
    base = (lpw2 & 0777000000000LL) >> 27;
    bound = (lpw2 & 0000777000000LL) >> 18;
    idcwp = lpw2 & 0777777;
    printf(
           "lpw dcw:%6.6llo%s%s%s%s%s%s tally:%4.4lo %3.3lo %3.3lo %6.6llo\n",
           dcw_addr, res ? " res" : "", iom_rel ? " iom_rel" : "",
           se ? " se" : "", nc ? " nc" : "", tal ? " tal" : "", rel ? " rel" : "",
           tally, base, bound, idcwp);
}

void decode_scw(word36 scw) {
    word18 address, tally;
    int lq;
    printf("scw ");
    if(scw & 0170000)
        printf("(not an scw) ");
    address = (scw & 0777777000000LL) >> 18;
    tally = scw & 0007777;
    lq = (scw & 0600000) >> 16;
    printf("addr:%9.9lo lq:%d tally:%6.6lo\n", address, lq, tally);
}

/* Actual operations */
/* XXX: these OUGHT to return fault codes or interrupt numbers! */

/* XXX: this is static for now */
FILE *chan40;

word36 rec[2048]; /* XXX - max record size? */
word36 reclen; /* must match maketape.c */
word36 is_eof;

int iom_setup(const char *confname) {
    FILE *conf;
    conf = fopen(confname, "r");
    if(!conf) {
        fprintf(stderr, "could not open %s for reading\n", confname);
        return(1);
    }
    /* XXX: parse conf file */
    fclose(conf);
    /* XXX: this is static for now */
    chan40 = fopen("boot.tape", "r+b");
    if(!chan40) {
        fputs("could not open boot.tape\n", stderr);
        return(1);
    }
    console_chan = 010;
    return 0;
}

void iom_dcw(word18 dcwaddr) {
    word36 dcw;
    core_read(dcwaddr, &dcw);
    if((dcw & 0700000) == 0700000) {
        decode_idcw(dcw);
        /* read record? */
        if(((dcw & 0770000000000LL) >> 30) == 5) {
            /* this is sort of hairy */
            fread(&reclen, sizeof(reclen), 1, chan40);
            fread(&is_eof, sizeof(is_eof), 1, chan40);
            reclen -= 2 * sizeof(word36);
            fread(&rec, reclen, 1, chan40);
            reclen /= sizeof(word36); /* bytes to words */
            printf("Read record of %lld words\n", reclen);
        }
    } else {
        if((dcw & 0030000) == 0020000) {
        } else {
            word18 addr;
            decode_ddcw(dcw);
            decode_tdcw(dcw);
            addr = GETHI(dcw);
            /* IOTD? */
            if(!(dcw & 0030000)) {
                word18 pos;
                word36 xed1, xed2;
                for(pos = 0 ; pos < reclen ; ++pos) {
                    core_write(addr + pos, &rec[pos]);
                }
                printf("Transmitted record of %lld words to %6.6lo\n",
                       reclen, addr);
                /* disconnect? interrupt ??? */
                core_read2(30 /* decimal */, &xed1, &xed2);
// XXX                execdouble(xed1, xed2);
            }
        }
    }
}

void iom_pcw(word18 pcwaddr) {
    word36 pcw1, pcw2;
    word8 channel;
    word18 mboxaddr;
    word36 lpw, lpwext, scw;
    word18 tally, which_dcw;
    word18 dcwaddr;
    core_read2(pcwaddr, &pcw1, &pcw2);
    decode_pcw(pcw1, pcw2);
    /* XXX */
    channel = (pcw2 & 0077000000000LL) >> 27;
    mboxaddr = iom_pos + 4 * channel;
    core_read2(mboxaddr, &lpw, &lpwext);
    decode_lpw(lpw, lpwext);
    tally = lpw & 0007777;
    core_read(mboxaddr + 2, &scw);
    decode_scw(scw);
    /* ?? dcw = core[mboxaddr + 3]; */
    dcwaddr = (lpw & 0777777000000LL) >> 18;
    for(which_dcw = 0 ; which_dcw < tally ; ++which_dcw) {
        iom_dcw(dcwaddr + which_dcw);
    }
}

void iom_cioc(word8 conchan) {
    word18 lpwaddr, lpwtarget, tally;
    word36 lpw, lpwext;
    lpwaddr = iom_pos + 4 * conchan;
    core_read2(lpwaddr, &lpw, &lpwext);
    decode_lpw(lpw, lpwext);
    lpwtarget = (lpw & 0777777000000LL) >> 18;
    tally = lpw & 0007777;
    /* Special behavior for connect channel 2 */
    if(conchan == 2)
        iom_pcw(lpwtarget);
    else
        iom_dcw(lpwtarget);
}

void iom_cioc_cow(word18 cowaddr) {
    word36 cow;
    word8 chanptr, memport;
    core_read(cowaddr, &cow);
    chanptr = (cow & 0000070) >> 3;
    memport = cow & 0000007;
    printf("Connect COW: chanptr=%o memport=%o\n", chanptr, memport);
    /* XXX: what next? */
}

#endif

/*
 iom.c -- emulation of an I/O Multiplexer
 
 See: Document 43A239854 -- 6000B I/O Multiplexer
 (43A239854_600B_IOM_Spec_Jul75.pdf)
 
 See AN87 which specifies some details of portions of PCWs that are
 interpreted by the channel boards and not the IOM itself.
 
 See also: http://www.multicians.org/fjcc5.html -- Communications
 and Input/Output Switching in a Multiplex Computing System
 
 See also: Patents: 4092715, 4173783, 1593312
 
 Changes needed to support multiple IOMs:
 Hang an iom_t off of a DEVICE instead of using global "iom".
 Remove assumptions re IOM "A" (perhaps just IOM_A_xxx #defines).
 Move the few non extern globals into iom_t.  This includes the
 one hidden in get_chan().
 */
/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */

/*
 
 3.10 some more physical switches
 
 Config switch: 3 positions -- Standard GCOS, Extended GCOS, Multics
 
 Note that all Mem[addr] references are absolute (The IOM has no access to
 the CPU's appending hardware.)
 */


/*
 NOTES on data structures
 
 SIMH has DEVICES.
 DEVICES have UNITs.
 UNIT member u3 holds the channel number.  Used by channel service routine
 which is called only with a UNIT ptr.
 
 The iom_t IOM struct includes an array of channels:
 type
 DEVICE *dev; // ptr into sim_devices[]
 UNIT *board
 
 The above provides a channel_t per channel and a way to find it
 given only a UNIT ptr.
 The channel_t:
 Includes list-service flag/count, state info, major/minor status,
 most recent dcw, handle for sim-activate
 Includes IDCW/PCW info: cmd, code, chan-cmd, chan-data, etc
 Includes ptr to devinfo
 Another copy of IDCW/PCW stuff: dev-cmd, dev-code, chan-data
 Includes status major/minor & have-status flag
 Includes read/write flag
 Includes time for queuing
 */

/*
 TODO -- partially done
 
 Following is prep for async...
 
 Have list service update DCW in mbx (and not just return *addrp)
 Note that we do write LPW in list_service().
 [done] Give channel struct a "scratchpad" LPW
 Give channel struct a "scratchpad" DCW
 Note that "list service" should "send" pcw/dcw to channel...
 
 Leave connect channel as immediate
 Make do_channel() async.
 Need state info for:
 have a dcw to process?
 DCW sent to device?
 Has device sent back results yet?
 Move most local vars to chan struct
 Give device functions a way to report status
 Review flow charts
 New function:
 ms_to_interval()
 */


//#include "hw6180.h"
#include <sys/time.h>
//#include "iom.hincl"

extern cpu_ports_t cpu_ports;
extern scu_t scu;
extern iom_t iom;

// ============================================================================
// === Typedefs

enum iom_sys_faults {
    // List from 4.5.1; descr from AN87, 3-9
    iom_no_fault = 0,
    iom_ill_chan = 01,      // PCW to chan with chan number >= 40
    // iom_ill_ser_req=02,
    // chan requested svc with a service code of zero or bad chan #
    iom_256K_of=04,
    // 256K overflow -- address decremented to zero, but not tally
    iom_lpw_tro_conn = 05,
    // tally was zero for an update LPW (LPW bit 21==0) when the LPW was
    // fetched for the connect channel
    iom_not_pcw_conn=06,    // DCW for conn channel had bits 18..20 != 111b
    // iom_cp1_data=07,     // DCW was TDCW or had bits 18..20 == 111b
    iom_ill_tly_cont = 013,
    // LPW bits 21-22 == 00 when LPW was fetched for the connect channel
    // 14 LPW had bit 23 on in Multics mode
};

enum iom_user_faults {  // aka central status
    // from 4.5.2
    iom_lpw_tro = 1,
    //  tally was zero for an update LPW (LPW bit 21==0) when the LPW
    //  was fetched and TRO-signal (bit 22) is on
    iom_bndy_vio = 03,
};

#if 0
// from AN87, 3-8
typedef struct {
    int channel;    // 9 bits
    int serv_req;   // 5 bits; see AN87, 3-9
    int ctlr_fault; // 4 bits; SC ill action codes, AN87 sect II
    int io_fault;   // 6 bits; see enum iom_sys_faults
} sys_fault_t;
#endif

// ============================================================================
// === Static globals

// #define MAXCHAN 64

#define IOM_A_MBX 01400     /* location of mailboxes for IOM A */
#define IOM_CONNECT_CHAN 2

// ============================================================================
// === Internal functions

//static void dump_iom(void);
//static void dump_iom_mbx(int base, int i);
static void iom_fault(int chan, const char* who, int is_sys, int signal);
static int list_service(int chan, int first_list, int *ptro, int *addr);
static int send_channel_pcw(int chan, int addr);
static int do_channel(channel_t* chanp);
static int do_dcw(int chan, int addr, int *control, flag_t *need_indir_svc);
static int do_ddcw(int chan, int addr, dcw_t *dcwp, int *control);
static int lpw_write(int chan, int chanloc, const lpw_t* lpw);
static int do_connect_chan(void);
static char* lpw2text(const lpw_t *p, int conn);
static char* pcw2text(const pcw_t *p);
static char* dcw2text(const dcw_t *p);
static void parse_lpw(lpw_t *p, int addr, int is_conn);
//static void parse_pcw(pcw_t *p, int addr, int ext);
static void decode_idcw(pcw_t *p, flag_t is_pcw, t_uint64 word0, t_uint64 word1);
static void parse_dcw(int chan, dcw_t *p, int addr, int read_only);
static int dev_send_idcw(int chan, pcw_t *p);
static int status_service(int chan);
//static int send_chan_flags();
static int send_general_interrupt(int chan, int pic);
static int send_terminate_interrupt(int chan);
// static int send_marker_interrupt(int chan);
static int activate_chan(int chan, pcw_t* pcw);
static channel_t* get_chan(int chan);
static int run_channel(int chan);

// ============================================================================

/*
 * iom_svc()
 *
 *    Service routine for SIMH events for the IOM itself
 */

t_stat iom_svc(UNIT *up)
{
    log_msg(INFO_MSG, "IOM::service", "Starting!\n");
    iom_interrupt();
    return 0;
}

// ============================================================================

/*
 * channel_svc()
 *
 *    Service routine for devices such as tape drives that may be connected to the IOM
 */

t_stat channel_svc(UNIT *up)
{
    int chan = up->u3;
    log_msg(NOTIFY_MSG, "IOM::channel-svc", "Starting for channel %d!\n", chan);
    channel_t *chanp = get_chan(chan);
    if (chanp == NULL)
        return SCPE_ARG;
    if (chanp->devinfop == NULL) {
        log_msg(WARN_MSG, "IOM::channel-svc", "No context info for channel %d.\n", chan);
    } else {
        // FIXME: It might be more realistic for the service routine to to call
        // device specific routines.  However, instead, we have the device do
        // all the work ahead of time and the queued service routine is just
        // reporting that the work is done.
        chanp->status.major = chanp->devinfop->major;
        chanp->status.substatus = chanp->devinfop->substatus;
        chanp->status.rcount = chanp->devinfop->chan_data;
        chanp->status.read = chanp->devinfop->is_read;
        chanp->have_status = 1;
    }
    do_channel(chanp);
    return 0;
}

// ============================================================================

/*
 * iom_init()
 *
 *  Once-only initialization
 */

void iom_init()
{
    log_msg(INFO_MSG, "IOM::init", "Running.\n");
    
    memset(&iom, 0, sizeof(iom));
    for (int i = 0; i < ARRAY_SIZE(iom.ports); ++i) {
        iom.ports[i] = -1;
    }
    for (int i = 0; i < ARRAY_SIZE(iom.channels); ++i) {
        iom.channels[i].type = DEVT_NONE;
    }
    
    for (int chan = 0; chan < max_channels; ++chan) {
        channel_t* chanp = get_chan(chan);
        if (chanp != NULL) {
            chanp->chan = chan;
            chanp->status.chan = chan;  // BUG/TODO: remove this member
            chanp->unitp = NULL;
            chanp->state = chn_idle;
            memset(&chanp->lpw, 0, sizeof(chanp->lpw));
            // DEVICEs ctxt pointers used to point at chanp->devinfo,
            // but now both are ptrs to the same object so that either
            // may do the allocation
            if (chanp->devinfop == NULL) {
                // chanp->devinfop = malloc(sizeof(*(chanp->devinfop)));
            }
            if (chanp->devinfop != NULL) {
                chanp->devinfop->chan = chan;
                chanp->devinfop->statep = NULL;
            }
        }
    }
}

// ============================================================================

/*
 * iom_reset()
 *
 *  Reset -- Reset to initial state -- clear all device flags and cancel any
 *  any outstanding timing operations. Used by SIMH's RESET, RUN, and BOOT
 *  commands
 *
 *  Note that all reset()s run after once-only init().
 *
 */

t_stat iom_reset(DEVICE *dptr)
{
    
    const char* moi = "IOM::reset";
    log_msg(INFO_MSG, moi, "Running.\n");
    
    for (int chan = 0; chan < max_channels; ++chan) {
        channel_t* chanp = get_chan(chan);
        if (chanp->unitp != NULL) {
            sim_cancel(chanp->unitp);
            free(chanp->unitp);
            chanp->unitp = NULL;
        }
        chanp->state = chn_idle;
        memset(&chanp->lpw, 0, sizeof(chanp->lpw));
        // BUG/TODO: flag channels as "masked"
    }
    
    for (int chan = 0; chan < ARRAY_SIZE(iom.channels); ++chan) {
        DEVICE *devp = iom.channels[chan].dev;
        if (devp) {
            if (devp->units == NULL) {
                log_msg(ERR_MSG, moi, "Device on channel %d does not have any units.\n", chan);
            } else
                devp->units->u3 = chan;
        }
    }
    
    return 0;
}

// ============================================================================

/*
 * iom_interrupt()
 *
 * Top level interface to the IOM for the SCU.   Simulates receipt of a $CON
 * signal from a SCU.  The $CON signal is sent from an SCU when a CPU executes
 * a CIOC instruction asking the SCU to signal the IOM.
 */

void iom_interrupt()
{
    // Actually, the BUS would give us more than just the channel:
    //      for program interrupt service
    //          interrupt level
    //          addr ext (0-2)
    //          channel number
    //          servicer request code
    //          addr ext (3-5)
    //      for status service signals
    //          ...
    //      for data or list service
    //          direct data addr
    //          addr ext (0-2)
    //          channel number
    //          service request code
    //          mode
    //          DP
    //          chan size
    //          addr ext (3-5)
    
    extern DEVICE cpu_dev;
    ++ opt_debug; ++ cpu_dev.dctrl;
    log_msg(DEBUG_MSG, "IOM::CIOC::intr", "Starting\n");
    do_connect_chan();
    log_msg(DEBUG_MSG, "IOM::CIOC::intr", "Finished\n");
    log_msg(DEBUG_MSG, NULL, "\n");
    -- opt_debug; -- cpu_dev.dctrl;
}

// ============================================================================

/*
 * do_connect_chan()
 *
 * Process the "connect channel".  This is what the IOM does when it
 * receives a $CON signal.
 *
 * Only called by iom_interrupt() just above.
 *
 * The connect channel requests one or more "list services" and processes the
 * resulting PCW control words.
 */

static int do_connect_chan()
{
    const char *moi = "IOM::conn-chan";
    
    // TODO: We don't allow a condition where it is possible to generate
    // channel status #1 "unexpected PCW (connect while busy)"
    
    int ptro = 0;   // pre-tally-run-out, e.g. end of list
    int addr;
    int ret = 0;
    while (ptro == 0) {
        log_msg(DEBUG_MSG, moi, "Doing list service for Connect Channel\n");
        ret = list_service(IOM_CONNECT_CHAN, 1, &ptro, &addr);
        if (ret == 0) {
            log_msg(DEBUG_MSG, moi, "Return code zero from Connect Channel list service, doing pcw\n");
            log_msg(DEBUG_MSG, NULL, "\n");
            ret = send_channel_pcw(IOM_CONNECT_CHAN, addr);
        } else {
            log_msg(DEBUG_MSG, moi, "Return code non-zero from Connect Channel list service, skipping pcw\n");
            break;
        }
        // Note: list-service updates LPW in core -- (but has a BUG in
        // that it *always* writes.
        // BUG: Stop if tro system fault occured
    }
    return ret;
}


// ============================================================================

/*
 * get_chan()
 *
 * Return pointer to channel info.
 *
 * This is a wrapper for an implementation likely to change...
 *
 */

static channel_t* get_chan(int chan)
{
    static channel_t channels[max_channels];
    
    if (chan < 0 || chan >= 040 || chan >= max_channels) {
        // TODO: Would ill-ser-req be more appropriate?
        // Probably depends on whether caller is the iom and
        // is issuing a pcw or if the caller is a channel requesting svc
        iom_fault(chan, NULL, 1, iom_ill_chan);
        return NULL;
    }
    return &channels[chan];
}

// ============================================================================

/*
 * send_channel_pcw()
 *
 * Send a PCW (Peripheral Control Word) to a payload channel.
 *
 * Only called by do_connect_chan() just above.
 *
 * PCWs are retrieved by the connect channel and sent to "payload" channels.
 * This is the only way to initiate operation of any channel other than the
 * connect channel.
 *
 * The PCW indicates a physical channel and usually specifies a command to
 * be sent to the peripheral on that channel.
 *
 * Only the connect channel has lists that contain PCWs. (Other channels
 * use IDCWs (Instruction Data Control Words) to send commands to devices).
 *
 */

static int send_channel_pcw(int chan, int addr)
{
    const char *moi = "IOM::send-pcw";
    
    log_msg(DEBUG_MSG, moi, "PCW for chan %d, addr %#o\n", chan, addr);
    pcw_t pcw;
    t_uint64 word0, word1;
    (void) fetch_abs_pair(addr, &word0, &word1);
    decode_idcw(&pcw, 1, word0, word1);
    log_msg(INFO_MSG, moi, "PCW is: %s\n", pcw2text(&pcw));
    
    // BUG/TODO: Should these be user faults, not system faults?
    
    if (pcw.chan < 0 || pcw.chan >= 040) {  // 040 == 32 decimal
        iom_fault(chan, moi, 1, iom_ill_chan);
        return 1;
    }
    if (pcw.cp != 07) {
        iom_fault(chan, moi, 1, iom_not_pcw_conn);
        return 1;
    }
    
    if (pcw.mask) {
        // BUG: set mask flags for channel?
        log_msg(ERR_MSG, moi, "PCW Mask not implemented\n");
        cancel_run(STOP_BUG);
        return 1;
    }
    return activate_chan(pcw.chan, &pcw);
}

// ============================================================================

/*
 * activate_chan()
 *
 * Send a PCW to a channel to start a sequence of operations.
 *
 * Called only by the connect channel's handle_pcw() just above.
 *
 * However, note that the channel being processed is the one specified
 * in the PCW, not the connect channel.
 *
 */


static int activate_chan(int chan, pcw_t* pcwp)
{
    const char *moi = "IOM::activate-chan";
    
    channel_t *chanp = get_chan(chan);
    if (chanp == NULL)
        return 1;
    
    if (chanp->state != chn_idle) {
        // Issue user channel fault #1 "unexpected PCW (connect while busy)"
        iom_fault(chan, moi, 0, 1);
        return 1;
    }
    
    // Devices used by the IOM must have a ctxt with devinfo.
    DEVICE* devp = iom.channels[chan].dev;
    if (devp != NULL) {
        chan_devinfo* devinfop = devp->ctxt;
        if (devinfop == NULL) {
            // devinfop = &chanp->devinfo;
            if (chanp->devinfop == NULL) {
                devinfop = malloc(sizeof(*devinfop));
                devinfop->chan = chan;
                devinfop->statep = NULL;
                chanp->devinfop = devinfop;
            } else
                if (chanp->devinfop->chan == -1) {
                    chanp->devinfop->chan = chan;
                    log_msg(NOTIFY_MSG, moi, "OPCON found on channel %#o\n", chan);
                }
            devinfop = chanp->devinfop;
            devp->ctxt = devinfop;
        } else if (chanp->devinfop == NULL) {
            chanp->devinfop = devinfop;
        } else if (devinfop != chanp->devinfop) {
            log_msg(ERR_MSG, moi, "Channel %u and device mismatch with %d and %d\n", chan, devinfop->chan, chanp->devinfop->chan);
            cancel_run(STOP_BUG);
        }
    }
    
    chanp->n_list = 0;      // first list flag (and debug counter)
    chanp->err = 0;
    chanp->state = chn_pcw_rcvd;
    
    // Receive the PCW
    chanp->dcw.type = idcw;
    chanp->dcw.fields.instr = *pcwp;
    
#if 0
    // T&D tape does *not* expect us to cache original SCW, it expects us to
    // use the SCW which is memory at the time status is needed (this may
    // be a SCW value loaded from tape, not the value that was there when
    // we were invoked).
    int chanloc = IOM_A_MBX + chan * 4;
    int scw = chanloc + 2;
    if (scw % 2 == 1) { // 3.2.4
        log_msg(WARN_MSG, "IOM::status", "SCW address 0%o is not even\n", scw);
        -- scw;         // force y-pair behavior
    }
    (void) fetch_abs_word(scw, &chanp->scw);
    log_msg(DEBUG_MSG, moi, "Caching SCW value %012llo from address %#o for channel %d.\n",
            chanp->scw, scw, chan);
#endif
    
    // TODO: allow sim_activate on the channel instead of do_channel()
    int ret = do_channel(chanp);
    return ret;
}

// ============================================================================


/*
 * do_channel()
 *
 * Runs all the phases of a channel's operation back-to-back.  Terminates
 * when the channel is finished or when the channel queues an activity
 * via sim_activate().
 *
 */

int do_channel(channel_t *chanp)
{
    const char *moi = "IOM::do-channel";
    
    const int chan = chanp->chan;
    
#if 0
    if (chanp->state != chn_pcw_rcvd) {
        log_msg(ERR_MSG, moi, "Channel isn't in pcw-rcvd.\n");
        cancel_run(STOP_BUG);
    }
#endif
    
    /*
     * Now loop
     */
    
    int ret = 0;
    log_msg(INFO_MSG, moi, "Starting run_channel() loop.\n");
    for (;;) {
        // log_msg(INFO_MSG, moi, "Running channel.\n");
        if (run_channel(chan) != 0) {
            // Often expected...
            log_msg(NOTIFY_MSG, moi, "Channel has non-zero return.\n");
        }
        if (chanp->state == chn_err) {
            log_msg(WARN_MSG, moi, "Channel is in an error state.\n");
            ret = 1;
            // Don't break -- we need to get status
        } else if (chanp->state == chn_idle) {
            log_msg(INFO_MSG, moi, "Channel is now idle.\n");
            break;
        } else if (chanp->err) {
            log_msg(WARN_MSG, moi, "Channel has error flag set.\n");
            ret = 1;
            // Don't break -- we need to get status
        } else if (chanp->state == chn_need_status)
            log_msg(INFO_MSG, moi, "Channel needs to do a status service.\n");
        else if (chanp->have_status)
            log_msg(INFO_MSG, moi, "Channel has status from device.\n");
        else {
            // activity should be pending
            break;
        }
    };
    
    // Note that the channel may have pending work.  If so, the
    // device will have set have_status false and will have queued an activity.
    // When the device activates, it'll queue a run for the channel.
    
    log_msg(INFO_MSG, moi, "Finished\n");
    return ret;
}

// ============================================================================

static void print_chan_state(const char* moi, channel_t* chanp)
{
    log_msg(DEBUG_MSG, moi, "Channel %d: state = %s (%d), have status = %c, err = %c; n-svcs = %d.\n",
            chanp->chan,
            chn_state_text(chanp->state),
            chanp->state,
            chanp->have_status ? 'Y' : 'N',
            chanp->err ? 'Y' : 'N',
            chanp->n_list);
    // FIXME: Maybe dump chanp->lpw
}
// ============================================================================

/*
 * run_channel()
 *
 * Simulates the operation of a channel.  Channels run asynchrounsly from
 * the IOM central.  Calling this function represents performing one iteration
 * of the channel's cycle of operations.  This function will need to be called
 * several times before a channel is finished with a task.
 *
 * Normal usage is for the channel flags to initially be set for notificaton of
 * a PCW being sent from the IOM central (via the connect channel).  On the
 * first call, run_channel() will send the PCW to the device.
 *
 * The sending of the PCW and various other invoked operations may not complete
 * immediately.  On the second and subsequent calls, run_channel() will first
 * check the status of any previously queued but now complete device operation.
 * After the first call, the next few calls will perform list services and
 * dispatch DCWs.  The last call will be for a status service, after which the
 * channel will revert to an idle state.
 *
 * Called both by activate_channel->do_channel() just after a CIOC instruction
 * and also called by channel_svc()->do_channel() as queued operations
 * complete.
 *
 * Note that this function is *not* used to run the connect channel.  The
 * connect channel is handled as a special case by do_connect_chan().
 *
 * This code is probably not quite correct; the nuances around the looping
 * controls may be wrong...
 *
 */

static int run_channel(int chan)
{
    const char* moi = "IOM::channel";
    
    log_msg(DEBUG_MSG, NULL, "\n");
    log_msg(INFO_MSG, moi, "Starting for channel %d (%#o)\n", chan, chan);
    
    channel_t* chanp = get_chan(chan);
    if (chanp == NULL)
        return 1;
    print_chan_state(moi, chanp);
    
    if (chanp->state == chn_idle && ! chanp->err) {
        log_msg(WARN_MSG, moi, "Channel %d is idle.\n", chan);
        cancel_run(STOP_WARN);
        return 0;
    }
    
    int first_list = chanp->n_list == 0;
    
    // =========================================================================
    
    /*
     * First, check the status of any prior command for error conditions
     */
    
    if (chanp->state == chn_cmd_sent || chanp->state == chn_err) {
        // Channel is busy.
        
        // We should not still be waiting on the attached device (we're not
        // re-invoked until status is available).
        // Nor should we be invoked for an idle channel.
        if (! chanp->have_status && chanp->state != chn_err && ! chanp->err) {
            log_msg(WARN_MSG, moi, "Channel %d activate, but still waiting on device.\n", chan);
            cancel_run(STOP_WARN);
            return 0;
        }
        
        // If the attached device has terminated operations, we'll need
        // to do a status service and finish off the current connection
        
        if (chanp->status.major != 0) {
            // Both failed DCW loops or a failed initial PCW are caught here
            log_msg(INFO_MSG, moi, "Channel %d reports non-zero major status; terminating DCW loop and performing status service.\n", chan);
            chanp->state = chn_need_status;
        } else if (chanp->err || chanp->state == chn_err) {
            log_msg(NOTIFY_MSG, moi, "Channel %d reports internal error; doing status.\n", chanp->chan);
            chanp->state = chn_need_status;
        } else if (chanp->control == 0 && ! chanp->need_indir_svc && ! first_list) {
            // no work left
            // FIXME: Should we handle this case in phase two below (may affect marker
            // interrupts) ?
            log_msg(INFO_MSG, moi, "Channel %d out of work; doing status.\n", chanp->chan);
            chanp->state = chn_need_status;
        }
        
        // BUG: enable.   BUG: enabling kills the boot...
        // chanp->have_status = 0;  // we just processed it
        
        // If we reach this point w/o resetting the state to chn_need_status,
        // we're busy and the channel hasn't terminated operations, so we don't
        // need to do a status service.  We'll handle the non-terminal pcw/dcw
        // completion in phase two below.
    }
    
    // =========================================================================
    
    /*
     * First of four phases -- send any PCW command to the device
     */
    
    if (chanp->state == chn_pcw_rcvd) {
        log_msg(INFO_MSG, moi, "Received a PCW from connect channel.\n");
        chanp->control = chanp->dcw.fields.instr.control;
        pcw_t *p = &chanp->dcw.fields.instr;
        chanp->have_status = 0;
        int ret = dev_send_idcw(chan, p);
        // Note: dev_send_idcw will either set state=chn_cmd_sent or do iom_fault()
        if (ret != 0) {
            log_msg(NOTIFY_MSG, moi, "Device on channel %d did not like our PCW -- non zero return.\n", chan);
            // dev_send_idcw() will have done an iom_fault() or gotten
            // a non-zero major code from a device
            // BUG: Put channel in a fault state or mask
            chanp->state = chn_err;
            return 1;
        }
        // FIXME: we could probably just do a return(0) here and skip the code below
        if (chanp->have_status) {
            log_msg(INFO_MSG, moi, "Device took PCW instantaneously...\n");
            if (chanp->state != chn_cmd_sent) {
                log_msg(WARN_MSG, moi, "Bad state after sending PCW to channel.\n");
                print_chan_state(moi, chanp);
                chanp->state = chn_err;
                cancel_run(STOP_BUG);
                return 1;
            }
        } else {
            // The PCW resulted in a call to sim_activate().
            return 0;
        }
    }
    
    /*
     * Second of four phases
     *     The device didn't finish operations (and we didn't do a status
     *     service).
     *     The channel needs to loop requesting list service(s) and
     *     dispatching the resulting DCWs.  We'll handle one iteration
     *     of the looping process here and expect to be called later to
     *     handle any remaining iterations.
     */
    
    /*
     *  BUG: need to implement 4.3.3:
     *      Indirect Data Service says DCW is written back to the *mailbox* ?
     *      (Probably because the central can't find words in the middle
     *      of lists?)
     */
    
    
    
    if (chanp->state == chn_cmd_sent) {
        log_msg(DEBUG_MSG, moi, "In channel loop for state %s\n", chn_state_text(chanp->state));
        int ret = 0;
        
        if (chanp->control == 2 || chanp->need_indir_svc || first_list) {
            // Do a list service
            chanp->need_indir_svc = 0;
            int addr;
            log_msg(DEBUG_MSG, moi, "Asking for %s list service (svc # %d).\n", first_list ? "first" : "another", chanp->n_list + 1);
            if (list_service(chan, first_list, NULL, &addr) != 0) {
                ret = 1;
                log_msg(WARN_MSG, moi, "List service indicates failure\n");
            } else {
                ++ chanp->n_list;
                log_msg(DEBUG_MSG, moi, "List service yields DCW at addr 0%o\n", addr);
                chanp->control = -1;
                // Send request to device
                ret = do_dcw(chan, addr, &chanp->control, &chanp->need_indir_svc);
                log_msg(DEBUG_MSG, moi, "Back from latest do_dcw (at %0o); control = %d; have-status = %d\n", addr, chanp->control, chanp->have_status);
                if (ret != 0) {
                    log_msg(NOTIFY_MSG, moi, "do_dcw returns non-zero.\n");
                }
            }
        } else if (chanp->control == 3) {
            // BUG: set marker interrupt and proceed (list service)
            // Marker interrupts indicate normal completion of
            // a PCW or IDCW
            // PCW control == 3
            // See also: 3.2.7, 3.5.2, 4.3.6
            // See also 3.1.3
#if 1
            log_msg(ERR_MSG, moi, "Set marker not implemented\n");
            ret = 1;
#else
            // Boot tape never requests marker interrupts...
            ret = send_marker_interrupt(chan);
            if (ret == 0) {
                log_msg(NOTIFY_MSG, moi, "Asking for a list service due to set-marker-interrupt-and-proceed.\n");
                chanp->control = 2;
            }
#endif
        } else {
            log_msg(ERR_MSG, moi, "Bad PCW/DCW control, %d\n", chanp->control);
            cancel_run(STOP_BUG);
            ret = 1;
        }
        return ret;
    }
    
    // =========================================================================
    
    /*
     * Third and Fourth phases
     */
    
    if (chanp->state == chn_need_status) {
        int ret = 0;
        if (chanp->err || chanp->state == chn_err)
            ret = 1;
        /*
         * Third of four phases -- request a status service
         */
        // BUG: skip status service if system fault exists
        log_msg(DEBUG_MSG, moi, "Requesting Status service\n");
        status_service(chan);
        
        /*
         * Fourth of four phases
         *
         * 3.0 -- Following the status service, the channel will request the
         * IOM to do a multiplex interrupt service.
         *
         */
        log_msg(INFO_MSG, moi, "Sending terminate interrupt.\n");
        if (send_terminate_interrupt(chan))
            ret = 1;
        chanp->state = chn_idle;
        // BUG: move setting have_status=0 to after early check for err
        chanp->have_status = 0; // we just processed it
        return ret;
    }
    
    return 0;
}

// ============================================================================

#if 0
static int list_service_orig(int chan, int first_list, int *ptro)
{
    
    // Flowchart 256K overflow checks relate to 18bit offset & tally incr
    // TRO -- tally runout, a fault (not matching CPU fault names)
    //     user fault: lpw tr0 sent to channel (sys fault lpw tr0 for conn chan)
    // PTRO -- pre-tally runout -- only used internally?
    // page 82 etc for status
    
    // addr upper bits -- from: PCW in extended GCOS mode, or IDCW for list
    // service (2.1.2)
    // paging mode given by PTP (page table ptr) in second word of PCW (for
    // IOM-B)
    // 3 modes: std gcos; ext gcos, paged
    // paged: all acc either paged or extneded gcos, not relative moe
    
}
#endif


// ============================================================================

/*
 * list_service()
 *
 * Perform a list service for a channel.
 *
 * Examines the LPW (list pointer word).  Returns the 24-bit core address
 * of the next PCW or DCW.
 *
 * Called by do_connect_chan() for the connect channel or by run_channel()
 * for other channels.
 *
 * This code is probably not quite correct.  In particular, there appear
 * to be cases where the LPW will specify the next action to be taken,
 * but only a copy of the LPW in the IOM should be updated and not the
 * LPW in main core.
 *
 */

static int list_service(int chan, int first_list, int *ptro, int *addrp)
{
    // Core address of next PCW or DCW is returned in *addrp.  Pre-tally-runout
    // is returned in *ptro.
    
    int chanloc = IOM_A_MBX + chan * 4;
    const char* moi = "IOM::list-service";
    
    channel_t* chanp = get_chan(chan);
    if (chanp == NULL) {
        return 1;   // we're faulted
    }
    lpw_t* lpwp = &chanp->lpw;
    
    *addrp = -1;
    log_msg(DEBUG_MSG, moi, "Starting %s list service for LPW for channel %0o(%d dec) at addr %0o\n",
            (first_list) ? "first" : "another", chan, chan, chanloc);
    // Load LPW from main memory on first list, otherwise continue to use scratchpad
    if (first_list)
        parse_lpw(lpwp, chanloc, chan == IOM_CONNECT_CHAN);
    log_msg(DEBUG_MSG, moi, "LPW: %s\n", lpw2text(lpwp, chan == IOM_CONNECT_CHAN));
    
    if (lpwp->srel) {
        log_msg(ERR_MSG, moi, "LPW with bit 23 (SREL) on is invalid for Multics mode\n");
        iom_fault(chan, moi, 1, 014);   // TODO: want enum
        cancel_run(STOP_BUG);
        return 1;
    }
    if (first_list) {
        lpwp->hrel = lpwp->srel;
    }
    if (lpwp->ae != lpwp->hrel) {
        log_msg(WARN_MSG, moi, "AE does not match HREL\n");
        cancel_run(STOP_BUG);
    }
    
    // Check for TRO or PTRO at time that LPW is fetched -- not later
    
    if (ptro != NULL)
        *ptro = 0;
    int addr = lpwp->dcw;
    if (chan == IOM_CONNECT_CHAN) {
        if (lpwp->nc == 0 && lpwp->trunout == 0) {
            log_msg(WARN_MSG, moi, "Illegal tally connect channel\n");
            iom_fault(chan, moi, 1, iom_ill_tly_cont);
            cancel_run(STOP_WARN);
            return 1;
        }
        if (lpwp->nc == 0 && lpwp->trunout == 1)
            if (lpwp->tally == 0) {
                log_msg(WARN_MSG, moi, "TRO on connect channel\n");
                iom_fault(chan, moi, 1, iom_lpw_tro_conn);
                cancel_run(STOP_WARN);
                return 1;
            }
        if (lpwp->nc == 1) {
            // we're not updating tally, so pretend it's at zero
            if (ptro != NULL)
                *ptro = 1;  // forced, see pg 23
        }
        *addrp = addr;  // BUG: force y-pair
        log_msg(DEBUG_MSG, moi, "Expecting that connect channel will pull DCW from core\n");
    } else {
        // non connect channel
        // first, do an addr check for overflow
        int overflow = 0;
        if (lpwp->ae) {
            int sz = lpwp->size;
            if (lpwp->size == 0) {
                log_msg(INFO_MSG, "IOM::list-sevice", "LPW size is zero; interpreting as 4096\n");
                sz = 010000;    // 4096
            }
            if (addr >= sz)     // BUG: was >
                overflow = 1; // signal or record below
            else
                addr = lpwp->lbnd + addr ;
        }
        // see flowchart 4.3.1b
        if (lpwp->nc == 0 && lpwp->trunout == 0) {
            if (overflow) {
                iom_fault(chan, moi, 1, iom_256K_of);
                return 1;
            }
        }
        if (lpwp->nc == 0 && lpwp->trunout == 1) {
            // BUG: Chart not fully handled (nothing after (C) except T-DCW detect)
            for (;;) {
                if (lpwp->tally == 0) {
                    log_msg(WARN_MSG, moi, "TRO on channel 0%o\n", chan);
                    iom_fault(chan, moi, 0, iom_lpw_tro);
                    cancel_run(STOP_WARN);
                    // user fault, no return
                    break;
                }
                if (lpwp->tally > 1) {
                    if (overflow) {
                        iom_fault(chan, moi, 1, iom_256K_of);
                        return 1;
                    }
                }
                // Check for T-DCW
                t_uint64 word;
                (void) fetch_abs_word(addr, &word);
                int t = getbits36(word, 18, 3);
                if (t == 2) {
                    uint next_addr = word >> 18;
                    log_msg(ERR_MSG, moi, "Transfer-DCW not implemented; addr would be %06o; E,I,R = 0%o\n", next_addr, word & 07);
                    return 1;
                } else
                    break;
            }
        }
        *addrp = addr;
        // if in GCOS mode && lpwp->ae) fault;  // bit 20
        // next: channel should pull DCW from core
        log_msg(DEBUG_MSG, moi, "Expecting that channel 0%o will pull DCW from core\n", chan);
    }
    
    t_uint64 word;
    (void) fetch_abs_word(addr, &word);
    int cp = getbits36(word, 18, 3);
    if (cp == 7) {
        // BUG: update idcw fld of lpw
    }
    
    // int ret;
    
    //-------------------------------------------------------------------------
    // ALL THE FOLLOWING HANDLED BY PART "D" of figure 4.3.1b and 4.3.1c
    //          if (pcw.chan == IOM_CONNECT_CHAN) {
    //              log_msg(DEBUG_MSG, "IOM::pcw", "Connect channel does not return status.\n");
    //              return ret;
    //          }
    //          // BUG: need to write status to channel (temp: todo chan==036)
    // update LPW for chan (not 2)
    // update DCWs as used
    // SCW in mbx
    // last, send an interrupt (still 3.0)
    // However .. conn chan does not interrupt, store status, use dcw, or use
    // scw
    //-------------------------------------------------------------------------
    
    // Part "D" of 4.3.1c
    
    // BUG BUG ALL THE FOLLOWING IS BOTH CORRECT AND INCORRECT!!! Section 3.0
    // BUG BUG states that LPW for CONN chan is updated in core after each
    // BUG BUG chan is given PCW
    // BUG BUG Worse, below is prob for channels listed in dcw/pcw, not conn
    
    // BUG: No need to send channel flags?
    
    int write_lpw = 0;
    int write_lpw_ext = 0;
    int write_any = 1;
    if (lpwp->nc == 0) {
        if (lpwp->trunout == 1) {
            if (lpwp->tally == 1) {
                if (ptro != NULL)
                    *ptro = 1;
            } else if (lpwp->tally == 0) {
                write_any = 0;
                if (chan == IOM_CONNECT_CHAN)
                    iom_fault(chan, moi, 1, iom_lpw_tro_conn);
                else
                    iom_fault(chan, moi, 0, iom_bndy_vio);  // BUG: might be wrong
            }
        }
        if (write_any) {
            -- lpwp->tally;
            if (chan == IOM_CONNECT_CHAN)
                lpwp->dcw += 2; // pcw is two words
            else
                ++ lpwp->dcw;       // dcw is one word
        }
    } else  {
        // note: ptro forced earlier
        write_any = 0;
    }
    
    int did_idcw = 0;   // BUG
    int did_tdcw = 0;   // BUG
    if (lpwp->nc == 0) {
        write_lpw = 1;
        if (did_idcw || first_list)
            write_lpw_ext = 1;
    } else {
        // no update
        if (did_idcw || first_list) {
            write_lpw = 1;
            write_lpw_ext = 1;
        } else if (did_tdcw)
            write_lpw = 1;
    }
    //if (pcw.chan != IOM_CONNECT_CHAN) {
    //  ; // BUG: write lpw
    //}
    lpw_write(chan, chanloc, lpwp);     // BUG: we always write LPW
    
    log_msg(DEBUG_MSG, moi, "returning\n");
    return 0;   // BUG: unfinished
}

// ============================================================================

/*
 * do_dcw()
 *
 * Called by do_channel() when a DCW (Data Control Word) is seen.
 *
 * DCWs may specify a command to be sent to a device or may specify
 * I/O transfer(s).
 *
 * *controlp will be set to 0, 2, or 3 -- indicates terminate,
 * proceed (request another list services), or send marker interrupt
 * and proceed
 */

static int do_dcw(int chan, int addr, int *controlp, flag_t *need_indir_svc)
{
    log_msg(DEBUG_MSG, "IOM::dcw", "chan %d, addr 0%o\n", chan, addr);
    t_uint64 word;
    (void) fetch_abs_word(addr, &word);
    if (word == 0) {
        log_msg(ERR_MSG, "IOM::dcw", "DCW of all zeros is legal but useless (unless you want to dump first 4K of memory).\n");
        log_msg(ERR_MSG, "IOM::dcw", "Disallowing legal but useless all zeros DCW at address %08o.\n", addr);
        cancel_run(STOP_BUG);
        channel_t* chanp = get_chan(chan);
        if (chanp != NULL)
            chanp->state = chn_err;
        return 1;
    }
    dcw_t dcw;
    parse_dcw(chan, &dcw, addr, 0);
    
    if (dcw.type == idcw) {
        // instr dcw
        dcw.fields.instr.chan = chan;   // Real HW would not populate
        log_msg(DEBUG_MSG, "IOM::DCW", "%s\n", dcw2text(&dcw));
        *controlp = dcw.fields.instr.control;
        int ret = dev_send_idcw(chan, &dcw.fields.instr);
        if (ret != 0)
            log_msg(DEBUG_MSG, "IOM::dcw", "dev-send-pcw returns %d.\n", ret);
        if (dcw.fields.instr.chan_cmd != 02) {
            channel_t* chanp = get_chan(chan);
            if (chanp != NULL && dcw.fields.instr.control == 0 && chanp->have_status && M[addr+1] == 0) {
                log_msg(WARN_MSG, "IOM::dcw", "Ignoring need to set need-indirect service flag because next dcw is zero.\n");
                // cancel_run(STOP_BUG);
                cancel_run(STOP_BKPT);
            } else
                *need_indir_svc = 1;
        }
        return ret;
    } else if (dcw.type == tdcw) {
        uint next_addr = word >> 18;
        log_msg(ERR_MSG, "IOW::DCW", "Transfer-DCW not implemented; addr would be %06o; E,I,R = 0%o\n", next_addr, word & 07);
        return 1;
    } else  if (dcw.type == ddcw) {
        // IOTD, IOTP, or IONTP -- i/o (non) transfer
        int ret = do_ddcw(chan, addr, &dcw, controlp);
        return ret;
    } else {
        log_msg(ERR_MSG, "IOW::DCW", "Unknown DCW type\n");
        return 1;
    }
}

// ============================================================================

/*
 * dev_send_idcw()
 *
 * Send a PCW (Peripheral Control Word) or an IDCW (Instruction Data Control
 * Word) to a device.   PCWs and IDCWs are typically used for sending
 * commands to devices but not for requesting I/O transfers between the
 * device and the IOM.  PCWs and IDCWs typically cause a device to move
 * data betweeen a devices internal buffer and physical media.  DDCW I/O
 * transfer requests are used to move data between the IOM and the devices
 * buffers.  PCWs are only used by the connect channel; the other channels
 * use IDCWs.  IDCWs are essentially a one word version of the PCW.  A PCW
 * has a second word that only provides a channel number (which is used by
 * the connect channel to figure out which channel to send the PCW to).
 *
 * Note that we don't generate marker interrupts here; instead we
 * expect the caller to handle.
 *
 * See dev_io() below for handling of Data DCWS and I/O transfers.
 *
 * The various devices are implemented in other source files.  The IOM
 * expects two routines for each device: one to handle commands and
 * one to handle I/O transfers.
 *
 * Note: we always set chan_status.rcount to p->chan_data -- we don't
 * send/receive chan_data to/from any currently implemented devices...
 */

static int dev_send_idcw(int chan, pcw_t *p)
{
    const char* moi = "IOM::dev-send-idcw";
    
    channel_t* chanp = get_chan(chan);
    if (chanp == NULL)
        return 1;
    
    log_msg(INFO_MSG, moi, "Starting for channel 0%o(%d).  PCW: %s\n", chan, chan, pcw2text(p));
    
    DEVICE* devp = iom.channels[chan].dev;  // FIXME: needs to be per-unit, not per-channel
    // if (devp == NULL || devp->units == NULL)
    if (devp == NULL) {
        // BUG: no device connected; what's the appropriate fault code(s) ?
        chanp->status.power_off = 1;
        log_msg(WARN_MSG, moi, "No device connected to channel %#o(%d); Auto breakpoint.\n", chan, chan);
        iom_fault(chan, moi, 0, 0);
        cancel_run(STOP_BKPT);
        return 1;
    }
    chanp->status.power_off = 0;
    
    if (p->chan_data != 0)
        log_msg(INFO_MSG, moi, "Chan data is %o (%d)\n",
                p->chan_data, p->chan_data);
    
    enum dev_type type = iom.channels[chan].type;
    
    chan_devinfo* devinfop = NULL;
    if (type == DEVT_TAPE || type == DEVT_DISK) {
        // FIXME: devinfo probably needs to partially be per UNIT, not per channel
        devinfop = devp->ctxt;
        if (devinfop == NULL) {
            devinfop = malloc(sizeof(*devinfop));
            if (devinfop == NULL) {
                cancel_run(STOP_BUG);
                return 1;
            }
            devp->ctxt = devinfop;
            devinfop->chan = p->chan;
            devinfop->statep = NULL;
        }
        if (devinfop->chan != p->chan) {
            log_msg(ERR_MSG, moi, "Device on channel %#o (%d) has missing or bad context.\n", chan, chan);
            cancel_run(STOP_BUG);
            return 1;
        }
        devinfop->dev_cmd = p->dev_cmd;
        devinfop->dev_code = p->dev_code;
        devinfop->chan_data = p->chan_data;
        devinfop->have_status = 0;
    }
    
    int ret;
    switch(type) {
        case DEVT_NONE:
            // BUG: no device connected; what's the appropriate fault code(s) ?
            chanp->status.power_off = 1;
            log_msg(WARN_MSG, moi, "Device on channel %#o (%d) is missing.\n", chan, chan);
            iom_fault(chan, moi, 0, 0);
            cancel_run(STOP_WARN);
            return 1;
        case DEVT_TAPE:
            ret = mt_iom_cmd(devinfop);
            break;
        case DEVT_CON: {
            int ret = con_iom_cmd(p->chan, p->dev_cmd, p->dev_code, &chanp->status.major, &chanp->status.substatus);
            chanp->state = chn_cmd_sent;
            chanp->have_status = 1;     // FIXME: con_iom_cmd should set this
            chanp->status.rcount = p->chan_data;
            log_msg(DEBUG_MSG, moi, "CON returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
            return ret; // caller must choose between our return and the chan_status.{major,substatus}
        }
        case DEVT_DISK:
            ret = disk_iom_cmd(devinfop);
            log_msg(INFO_MSG, moi, "DISK returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
            break;
        default:
            log_msg(ERR_MSG, moi, "Unknown device type 0%o\n", iom.channels[chan].type);
            iom_fault(chan, moi, 1, 0); // BUG: need to pick a fault code
            cancel_run(STOP_BUG);
            return 1;
    }
    
    if (devinfop != NULL) {
        chanp->state = chn_cmd_sent;
        chanp->have_status = devinfop->have_status;
        if (devinfop->have_status) {
            // Device performed request immediately
            chanp->status.major = devinfop->major;
            chanp->status.substatus = devinfop->substatus;
            chanp->status.rcount = devinfop->chan_data;
            chanp->status.read = devinfop->is_read;
            log_msg(DEBUG_MSG, moi, "Device returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
        } else if (devinfop->time >= 0) {
            // Device asked us to queue a delayed status return.  FIXME: Should queue the work, not
            // the reporting.
            extern int32 sim_interval;
            int si = sim_interval;
            if (sim_activate(devp->units, devinfop->time) == SCPE_OK) {
                log_msg(DEBUG_MSG, moi, "Sim interval changes from %d to %d.  Q count is %d.\n", si, sim_interval, sim_qcount());
                log_msg(DEBUG_MSG, moi, "Device will be returning major code 0%o substatus 0%o in %d time units.\n", devinfop->major, devinfop->substatus, devinfop->time);
            } else {
                chanp->err = 1;
                log_msg(ERR_MSG, moi, "Cannot queue.\n");
            }
        } else {
            // BUG/TODO: allow devices to have their own queuing
            log_msg(ERR_MSG, moi, "Device neither returned status nor queued an activity.\n");
            chanp->err = 1;
            cancel_run(STOP_BUG);
        }
        return ret; // caller must choose between our return and the status.{major,substatus}
    }
    
    return -1;  // not reached
}

// ============================================================================


/*
 * dev_io()
 *
 * Send an I/O transfer request to a device.
 *
 * Called only by do_ddcw() as part of handling data DCWs (data control words).
 * This function sends or receives a single word.  See do_ddcw() for full
 * details of the handling of data DCWs including looping over multiple words.
 *
 * See dev_send_idcw() above for handling of PCWS and non I/O command requests.
 *
 * The various devices are implemented in other source files.  The IOM
 * expects two routines for each device: one to handle commands and
 * one to handle I/O transfers.
 *
 * BUG: We return zero to do_ddcw() even after a failed transfer.   This
 * causes addresses and tallys to become incorrect.   For example, we
 * return zero when the console operator is "distracted". -- fixed
 */

static int dev_io(int chan, t_uint64 *wordp)
{
    const char* moi = "IOM::dev-io";
    
    channel_t* chanp = get_chan(chan);
    if (chanp == NULL)
        return 1;
    
    DEVICE* devp = iom.channels[chan].dev;
    // if (devp == NULL || devp->units == NULL)
    if (devp == NULL) {
        // BUG: no device connected, what's the fault code(s) ?
        log_msg(WARN_MSG, "IOM::dev-io", "No device connected to chan 0%o\n", chan);
        chanp->status.power_off = 1;
        iom_fault(chan, moi, 0, 0);
        cancel_run(STOP_WARN);
        return 1;
    }
    chanp->status.power_off = 0;
    
    switch(iom.channels[chan].type) {
        case DEVT_NONE:
            // BUG: no device connected, what's the fault code(s) ?
            chanp->status.power_off = 1;
            iom_fault(chan, moi, 0, 0);
            log_msg(WARN_MSG, "IOM::dev-io", "Device on channel %#o (%d) is missing.\n", chan, chan);
            cancel_run(STOP_WARN);
            return 1;
        case DEVT_TAPE: {
            int ret = mt_iom_io(chan, wordp, &chanp->status.major, &chanp->status.substatus);
            if (ret != 0 || chanp->status.major != 0)
                log_msg(DEBUG_MSG, "IOM::dev-io", "MT returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
            return ret; // caller must choose between our return and the status.{major,substatus}
        }
        case DEVT_CON: {
            int ret = con_iom_io(chan, wordp, &chanp->status.major, &chanp->status.substatus);
            if (ret != 0 || chanp->status.major != 0)
                log_msg(DEBUG_MSG, "IOM::dev-io", "CON returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
            return ret; // caller must choose between our return and the status.{major,substatus}
        }
        case DEVT_DISK: {
            int ret = disk_iom_io(chan, wordp, &chanp->status.major, &chanp->status.substatus);
            // TODO: uncomment & switch to DEBUG: if (ret != 0 || chanp->status.major != 0)
            log_msg(INFO_MSG, "IOM::dev-io", "DISK returns major code 0%o substatus 0%o\n", chanp->status.major, chanp->status.substatus);
            return ret; // caller must choose between our return and the status.{major,substatus}
        }
        default:
            log_msg(ERR_MSG, "IOM::dev-io", "Unknown device type 0%o\n", iom.channels[chan].type);
            iom_fault(chan, moi, 1, 0); // BUG: need to pick a fault code
            cancel_run(STOP_BUG);
            return 1;
    }
    return -1;  // not reached
}

// ============================================================================

/*
 * do_ddcw()
 *
 * Process "data" DCWs (Data Control Words).   This function handles DCWs
 * relating to I/O transfers: IOTD, IOTP, and IONTP.
 *
 * Called only by do_dcw() which handles all types of DCWs.
 */

static int do_ddcw(int chan, int addr, dcw_t *dcwp, int *control)
{
    
    channel_t* chanp = get_chan(chan);
    if (chanp == NULL)
        return 1;
    
    log_msg(DEBUG_MSG, "IOW::DO-DDCW", "%012llo: %s\n", M[addr], dcw2text(dcwp));
    
    // impossible for (cp == 7); see do_dcw
    
    // AE, the upper 6 bits of data addr (bits 0..5) from LPW
    // if rel == 0; abs; else absolutize & boundry check
    // BUG: Need to munge daddr
    
    uint type = dcwp->fields.ddcw.type;
    uint daddr = dcwp->fields.ddcw.daddr;
    uint tally = dcwp->fields.ddcw.tally;   // FIXME?
    t_uint64 word = 0;
    t_uint64 *wordp = (type == 3) ? &word : M + daddr;    // 2 impossible; see do_dcw
    if (type == 3 && tally != 1)
        log_msg(ERR_MSG, "IOM::DDCW", "Type is 3, but tally is %d\n", tally);
    int ret;
    if (tally == 0) {
        log_msg(DEBUG_MSG, "IOM::DDCW", "Tally of zero interpreted as 010000(4096)\n");
        tally = 4096;
        log_msg(DEBUG_MSG, "IOM::DDCW", "I/O Request(s) starting at addr 0%o; tally = zero->%d\n", daddr, tally);
    } else
        log_msg(DEBUG_MSG, "IOM::DDCW", "I/O Request(s) starting at addr 0%o; tally = %d\n", daddr, tally);
    for (;;) {
        ret = dev_io(chan, wordp);
        if (ret != 0)
            log_msg(DEBUG_MSG, "IOM::DDCW", "Device for chan 0%o(%d) returns non zero (out of band return)\n", chan, chan);
        if (ret != 0 || chanp->status.major != 0)
            break;
        // BUG: BUG: We increment daddr & tally even if device didn't do the
        // transfer, e.g. when the console operator is "distracted".  This
        // is because dev_io() returns zero on failed transfers
        // -- fixed in dev_io()
        ++daddr;    // todo: remove from loop
        if (type != 3)
            ++wordp;
        if (--tally <= 0)
            break;
    }
    log_msg(DEBUG_MSG, "IOM::DDCW", "Last I/O Request was to/from addr 0%o; tally now %d\n", daddr, tally);
    // set control ala PCW as method to indicate terminate or proceed
    if (type == 0) {
        // This DCW is an IOTD -- do I/O and disconnect.  So, we'll
        // return a code zero --  don't ask for another list service
        *control = 0;
    } else {
        // Tell caller to ask for another list service.
        // Guessing '2' for proceed -- only PCWs and IDCWS should generate
        // marker interrupts?
#if 0
        *control = 3;
#else
        *control = 2;
#endif
    }
    // update dcw
#if 0
    // Assume that DCW is only in scratchpad (bootload_tape_label.alm rd_tape reuses same DCW on each call)
    Mem[addr] = setbits36(Mem[addr], 0, 18, daddr);
    Mem[addr] = setbits36(Mem[addr], 24, 12, tally);
    log_msg(DEBUG_MSG, "IOM::DDCW", "Data DCW update: %012llo: addr=%0o, tally=%d\n", Mem[addr], daddr, tally);
#endif
    return ret;
}

// ============================================================================

/*
 * decode_idcw()
 *
 * Decode an idcw word or pcw word pair
 */

static void decode_idcw(pcw_t *p, flag_t is_pcw, t_uint64 word0, t_uint64 word1)
{
    p->dev_cmd = getbits36(word0, 0, 6);
    p->dev_code = getbits36(word0, 6, 6);
    p->ext = getbits36(word0, 12, 6);
    p->cp = getbits36(word0, 18, 3);
    p->mask = getbits36(word0, 21, 1);
    p->control = getbits36(word0, 22, 2);
    p->chan_cmd = getbits36(word0, 24, 6);
    p->chan_data = getbits36(word0, 30, 6);
    if (is_pcw) {
        p->chan = getbits36(word1, 3, 6);
        uint x = getbits36(word1, 9, 27);
        if (x != 0) {
            // BUG: Should only check if in GCOS or EXT GCOS Mode
            log_msg(ERR_MSG, "IOM::pcw", "Page Table Pointer for model IOM-B detected\n");
            cancel_run(STOP_BUG);
        }
    } else {
        p->chan = -1;
    }
}

// ============================================================================

/*
 * pcw2text()
 *
 * Display pcw_t
 */

static char* pcw2text(const pcw_t *p)
{
    // WARNING: returns single static buffer
    static char buf[200];
    sprintf(buf, "[dev-cmd=0%o, dev-code=0%o, ext=0%o, mask=%d, ctrl=0%o, chan-cmd=0%o, chan-data=0%o, chan=0%o]",
            p->dev_cmd, p->dev_code, p->ext, p->mask, p->control, p->chan_cmd, p->chan_data, p->chan);
    return buf;
}

// ============================================================================

/*
 * parse_dcw()
 *
 * Parse word at "addr" into a dcw_t.
 */

static void parse_dcw(int chan, dcw_t *p, int addr, int read_only)
{
    t_uint64 word;
    (void) fetch_abs_word(addr, &word);
    int cp = getbits36(word, 18, 3);
    const char* moi = "IOM::DCW-parse";
    
    if (cp == 7) {
        p->type = idcw;
        decode_idcw(&p->fields.instr, 0, word, 0);
        // p->fields.instr.chan = chan; // Real HW would not populate
        p->fields.instr.chan = -1;
        if (p->fields.instr.mask && ! read_only) {
            // Bit 21 is extension control (EC), not a mask
            channel_t* chanp = get_chan(chan);
            if (! chanp)
                return;
            if (chanp->lpw.srel) {
                // Impossible, SREL is always zero for multics
                // For non-multics, this would be allowed and we'd check
                log_msg(ERR_MSG, "IOW::DCW", "I-DCW bit EC set but the LPW SREL bit is also set.");
                cancel_run(STOP_BUG);
                return;
            }
            log_msg(WARN_MSG, "IOW::DCW", "Channel %d: Replacing LPW AE %#o with %#o\n", chan, chanp->lpw.ae, p->fields.instr.ext);
            chanp->lpw.ae = p->fields.instr.ext;
            cancel_run(STOP_BKPT);
        }
    } else {
        int type = getbits36(word, 22, 2);
        if (type == 2) {
            // transfer
            p->type = tdcw;
            p->fields.xfer.addr = word >> 18;
            p->fields.xfer.ec = (word >> 2) & 1;
            p->fields.xfer.i = (word >> 1) & 1;
            p->fields.xfer.r = word  & 1;
        } else {
            p->type = ddcw;
            p->fields.ddcw.daddr = getbits36(word, 0, 18);
            p->fields.ddcw.cp = cp;
            p->fields.ddcw.tctl = getbits36(word, 21, 1);
            p->fields.ddcw.type = type;
            p->fields.ddcw.tally = getbits36(word, 24, 12);
        }
    }
    // return 0;
}

// ============================================================================

/*
 * dcw2text()
 *
 * Display a dcw_t
 *
 */

static char* dcw2text(const dcw_t *p)
{
    // WARNING: returns single static buffer
    static char buf[200];
    if (p->type == ddcw) {
        int dtype = p->fields.ddcw.type;
        const char* type =
        (dtype == 0) ? "IOTD" :
        (dtype == 1) ? "IOTP" :
        (dtype == 2) ? "transfer" :
        (dtype == 3) ? "IONTP" :
        "<illegal>";
        sprintf(buf, "D-DCW: type=%d(%s), addr=%06o, cp=0%o, tally=0%o(%d) tally-ctl=%d",
                dtype, type, p->fields.ddcw.daddr, p->fields.ddcw.cp, p->fields.ddcw.tally,
                p->fields.ddcw.tally, p->fields.ddcw.tctl);
    }
    else if (p->type == tdcw)
        sprintf(buf, "T-DCW: ...");
    else if (p->type == idcw)
        sprintf(buf, "I-DCW: %s", pcw2text(&p->fields.instr));
    else
        strcpy(buf, "<not a dcw>");
    return buf;
}

// ============================================================================

/*
 * print_dcw()
 *
 * Display a DCS
 *
 */

char* print_dcw(t_addr addr)
{
    // WARNING: returns single static buffer
    dcw_t dcw;
    parse_dcw(-1, &dcw, addr, 1);
    return dcw2text(&dcw);
}

// ============================================================================

/*
 * lpw2text()
 *
 * Display an LPW
 */

static char* lpw2text(const lpw_t *p, int conn)
{
    // WARNING: returns single static buffer
    static char buf[80];
    sprintf(buf, "[dcw=0%o ires=%d hrel=%d ae=%d nc=%d trun=%d srel=%d tally=0%o]",
            p->dcw, p->ires, p->hrel, p->ae, p->nc, p->trunout, p->srel, p->tally);
    if (!conn)
        sprintf(buf+strlen(buf), " [lbnd=0%o size=0%o(%d) idcw=0%o]",
                p->lbnd, p->size, p->size, p->idcw);
    return buf;
}

// ============================================================================

/*
 * parse_lpw()
 *
 * Parse the words at "addr" into a lpw_t.
 */

static void parse_lpw(lpw_t *p, int addr, int is_conn)
{
    t_uint64 word0;
    (void) fetch_abs_word(addr, &word0);
    p->dcw = word0 >> 18;
    p->ires = getbits36(word0, 18, 1);
    p->hrel = getbits36(word0, 19, 1);
    p->ae = getbits36(word0, 20, 1);
    p->nc = getbits36(word0, 21, 1);
    p->trunout = getbits36(word0, 22, 1);
    p->srel = getbits36(word0, 23, 1);
    p->tally = getbits36(word0, 24, 12);    // initial value treated as unsigned
    // p->tally = bits2num(getbits36(word0, 24, 12), 12);
    
    if (!is_conn) {
        // Ignore 2nd word on connect channel
        // following not valid for paged mode; see B15; but maybe IOM-B non existant
        // BUG: look at what bootload does & figure out if they expect 6000-B
        t_uint64 word1;
        (void) fetch_abs_word(addr +1, &word1);
        p->lbnd = getbits36(word1, 0, 9);
        p->size = getbits36(word1, 9, 9);
        p->idcw = getbits36(word1, 18, 18);
    } else {
        p->lbnd = -1;
        p->size = -1;
        p->idcw = -1;
    }
}

// ============================================================================

/*
 * print_lpw()
 *
 * Display a LPW along with its channel number
 *
 */

char* print_lpw(t_addr addr)
{
    lpw_t temp;
    int chan = (addr - IOM_A_MBX) / 4;
    parse_lpw(&temp, addr, chan == IOM_CONNECT_CHAN);
    static char buf[160];
    sprintf(buf, "Chan 0%o -- %s", chan, lpw2text(&temp, chan == IOM_CONNECT_CHAN));
    return buf;
}

// ============================================================================

/*
 * lpw_write()
 *
 * Write an LPW into main memory
 */

int lpw_write(int chan, int chanloc, const lpw_t* p)
{
    log_msg(DEBUG_MSG, "IOM::lpw_write", "Chan 0%o: Addr 0%o had %012llo %012llo\n", chan, chanloc, M[chanloc], M[chanloc+1]);
    lpw_t temp;
    parse_lpw(&temp, chanloc, chan == IOM_CONNECT_CHAN);
    //log_msg(DEBUG_MSG, "IOM::lpw_write", "Chan 0%o: Addr 0%o had: %s\n", chan, chanloc, lpw2text(&temp, chan == IOM_CONNECT_CHAN));
    //log_msg(DEBUG_MSG, "IOM::lpw_write", "Chan 0%o: Addr 0%o new: %s\n", chan, chanloc, lpw2text(p, chan == IOM_CONNECT_CHAN));
    t_uint64 word0 = 0;
    //word0 = setbits36(0, 0, 18, p->dcw & MASK18);
    word0 = setbits36(0, 0, 18, p->dcw);
    word0 = setbits36(word0, 18, 1, p->ires);
    word0 = setbits36(word0, 19, 1, p->hrel);
    word0 = setbits36(word0, 20, 1, p->ae);
    word0 = setbits36(word0, 21, 1, p->nc);
    word0 = setbits36(word0, 22, 1, p->trunout);
    word0 = setbits36(word0, 23, 1, p->srel);
    //word0 = setbits36(word0, 24, 12, p->tally & MASKBITS(12));
    word0 = setbits36(word0, 24, 12, p->tally);
    (void) store_abs_word(chanloc, word0);
    
    int is_conn = chan == 2;
    if (!is_conn) {
        t_uint64 word1 = setbits36(0, 0, 9, p->lbnd);
        word1 = setbits36(word1, 9, 9, p->size);
        word1 = setbits36(word1, 18, 18, p->idcw);
        (void) store_abs_word(chanloc+1, word1);
    }
    log_msg(DEBUG_MSG, "IOM::lpw_write", "Chan 0%o: Addr 0%o now %012llo %012llo\n", chan, chanloc, M[chanloc], M[chanloc+1]);
    return 0;
}

// ============================================================================

#if 0

/*
 * send_chan_flags
 *
 * Stub
 */

static int send_chan_flags()
{
    log_msg(NOTIFY_MSG, "IOM", "send_chan_flags() unimplemented\n");
    return 0;
}

#endif

// ============================================================================

/*
 * status_service()
 *
 * Write status info into a status mailbox.
 *
 * BUG: Only partially implemented.
 * WARNING: The diag tape will crash because we don't write a non-zero
 * value to the low 4 bits of the first status word.  See comments
 * at the top of mt.c.
 *
 */

static int status_service(int chan)
{
    // See page 33 and AN87 for format of y-pair of status info
    
    channel_t* chanp = get_chan(chan);
    if (chanp == NULL)
        return 1;
    
    // BUG: much of the following is not tracked
    
    t_uint64 word1, word2;
    word1 = 0;
    word1 = setbits36(word1, 0, 1, 1);
    word1 = setbits36(word1, 1, 1, chanp->status.power_off);
    word1 = setbits36(word1, 2, 4, chanp->status.major);
    word1 = setbits36(word1, 6, 6, chanp->status.substatus);
    word1 = setbits36(word1, 12, 1, 1); // BUG: even/odd
    word1 = setbits36(word1, 13, 1, 1); // BUG: marker int
    word1 = setbits36(word1, 14, 2, 0);
    word1 = setbits36(word1, 16, 1, 0); // BUG: initiate flag
    word1 = setbits36(word1, 17, 1, 0);
#if 0
    // BUG: Unimplemented status bits:
    word1 = setbits36(word1, 18, 3, chan_status.chan_stat);
    word1 = setbits36(word1, 21, 3, chan_status.iom_stat);
    word1 = setbits36(word1, 24, 6, chan_status.addr_ext);
#endif
    word1 = setbits36(word1, 30, 6, chanp->status.rcount);
    
    word2 = 0;
#if 0
    // BUG: Unimplemented status bits:
    word2 = setbits36(word2, 0, 18, chan_status.addr);
    word2 = setbits36(word2, 18, 3, chan_status.char_pos);
#endif
    word2 = setbits36(word2, 21, 1, chanp->status.read);
#if 0
    word2 = setbits36(word2, 22, 2, chan_status.type);
    word2 = setbits36(word2, 24, 12, chan_status.dcw_residue);
#endif
    
    // BUG: need to write to mailbox queue
    
    // T&D tape does *not* expect us to cache original SCW, it expects us to
    // use the SCW loaded from tape.
    
#if 1
    int chanloc = IOM_A_MBX + chan * 4;
    int scw = chanloc + 2;
    t_uint64 sc_word;
    (void) fetch_abs_word(scw, &sc_word);
    int addr = getbits36(sc_word, 0, 18);   // absolute
    // BUG: probably need to check for y-pair here, not above
    log_msg(DEBUG_MSG, "IOM::status", "Writing status for chan %d to 0%o=>0%o\n", chan, scw, addr);
#else
    t_uint64 sc_word = chanp->scw;
    int addr = getbits36(sc_word, 0, 18);   // absolute
    if (addr % 2 == 1) {    // 3.2.4
        log_msg(WARN_MSG, "IOM::status", "Status address 0%o is not even\n", addr);
        // store_abs_pair() call below will fix address
    }
    log_msg(DEBUG_MSG, "IOM::status", "Writing status for chan %d to %#o\n",
            chan, addr);
#endif
    log_msg(DEBUG_MSG, "IOM::status", "Status: 0%012llo 0%012llo\n",
            word1, word2);
    log_msg(DEBUG_MSG, "IOM::status", "Status: (0)t=Y, (1)pow=%d, (2..5)major=0%02o, (6..11)substatus=0%02o, (12)e/o=%c, (13)marker=%c, (14..15)Z, 16(Z?), 17(Z)\n",
            chanp->status.power_off, chanp->status.major, chanp->status.substatus,
            '1', // BUG
            'Y');   // BUG
    int lq = getbits36(sc_word, 18, 2);
    int tally = getbits36(sc_word, 24, 12);
#if 1
    if (lq == 3) {
        log_msg(WARN_MSG, "IOM::status", "SCW for channel %d has illegal LQ\n",
                chan);
        lq = 0;
    }
#endif
    store_abs_pair(addr, word1, word2);
    switch(lq) {
        case 0:
            // list
            if (tally != 0) {
                addr += 2;
                -- tally;
            }
            break;
        case 1:
            // 4 entry (8 word) queue
            if (tally % 8 == 1 || tally % 8 == -1)
                addr -= 8;
            else
                addr += 2;
            -- tally;
            break;
        case 2:
            // 16 entry (32 word) queue
            if (tally % 32 == 1 || tally % 32 == -1)
                addr -= 32;
            else
                addr += 2;
            -- tally;
            break;
    }
    if (tally < 0 && tally == - (1 << 11) - 1) {    // 12bits => -2048 .. 2047
        log_msg(WARN_MSG, "IOM::status", "Tally SCW address 0%o wraps to zero\n", tally);
        tally = 0;
    }
    // BUG: update SCW in core
    return 0;
}

// ============================================================================

/*
 * iom_fault()
 *
 * Handle errors internal to the IOM.
 *
 * BUG: Not implemented.
 *
 */

static void iom_fault(int chan, const char* who, int is_sys, int signal)
{
    // TODO:
    // For a system fault:
    // Store the indicated fault into a system fault word (3.2.6) in
    // the system fault channel -- use a list service to get a DCW to do so
    // For a user fault, use the normal channel status mechanisms
    
    // sys fault masks channel
    
    // signal gets put in bits 30..35, but we need fault code for 26..29
    
    // BUG: mostly unimplemented
    
    if (who == NULL)
        who = "unknown";
    log_msg(WARN_MSG, "IOM", "Fault for channel %d in %s: is_sys=%d, signal=%d\n", chan, who, is_sys, signal);
    log_msg(ERR_MSG, "IOM", "Not setting status word.\n");
    // cancel_run(STOP_WARN);
}

// ============================================================================

enum iom_imw_pics {
    // These are for bits 19..21 of an IMW address.  This is normally
    // the Interrupt Level from a Channel's Program Interrupt Service
    // Request.  We can deduce from the map in 3.2.7 that certain
    // special case PICs must exist; these are listed in this enum.
    // Note that the terminate pic of 011b concatenated with the IOM number
    // of zero 00b (two bits) yields interrupt number 01100b or 12 decimal.
    // Interrupt 12d has trap words at location 030 in memory.  This
    // is the location that the bootload tape header is written to.
    imw_overhead_pic = 1,   // IMW address ...001xx (where xx is IOM #)
    imw_terminate_pic = 3,  // IMW address ...011xx
    imw_marker_pic = 5,     // IMW address ...101xx
    imw_special_pic = 7     // IMW address ...111xx
};

/*
 * send_marker_interrupt()
 *
 * Send a "marker" interrupt to the CPU.
 *
 * Channels send marker interrupts to indicate normal completion of
 * a PCW or IDCW if the control field of the PCW/IDCW has a value
 * of three.
 */

#if 0
static int send_marker_interrupt(int chan)
{
    return send_general_interrupt(chan, imw_marker_pic);
}
#endif

/*
 * send_terminate_interrupt()
 *
 * Send a "terminate" interrupt to the CPU.
 *
 * Channels send a terminate interrupt after doing a status service.
 *
 */

static int send_terminate_interrupt(int chan)
{
    return send_general_interrupt(chan, imw_terminate_pic);
}

// ============================================================================

/*
 * send_general_interrupt()
 *
 * Send an interrupt from the IOM to the CPU.
 *
 */

static int send_general_interrupt(int chan, int pic)
{
    const char* moi = "IOM::send-interrupt";
    
    int imw_addr;
    imw_addr = iom.iom_num; // 2 bits
    imw_addr |= pic << 2;   // 3 bits
    int interrupt_num = imw_addr;
    // Section 3.2.7 defines the upper bits of the IMW address as
    // being defined by the mailbox base address switches and the
    // multiplex base address switches.
    // However, AN-70 reports that the IMW starts at 01200.  If AN-70 is
    // correct, the bits defined by the mailbox base address switches would
    // have to always be zero.  We'll go with AN-70.  This is equivalent to
    // using bit value 0010100 for the bits defined by the multiplex base
    // address switches and zeros for the bits defined by the mailbox base
    // address switches.
    imw_addr += 01200;  // all remaining bits
    
    log_msg(INFO_MSG, moi, "Channel %d (%#o), level %d; Interrupt %d (%#o).\n", chan, chan, pic, interrupt_num, interrupt_num);
    t_uint64 imw;
    (void) fetch_abs_word(imw_addr, &imw);
    // The 5 least significant bits of the channel determine a bit to be
    // turned on.
    log_msg(DEBUG_MSG, moi, "IMW at %#o was %012llo; setting bit %d\n", imw_addr, imw, chan & 037);
    imw = setbits36(imw, chan & 037, 1, 1);
    log_msg(INFO_MSG, moi, "IMW at %#o now %012llo\n", imw_addr, imw);
    (void) store_abs_word(imw_addr, imw);
    
    return scu_set_interrupt(interrupt_num);
}

// ============================================================================

int iom_show_mbx(FILE *st, UNIT *uptr, int val, void *desc)
{
    const char* moi = "IOM::show";
    if (desc != NULL)
        log_msg(NOTIFY_MSG, moi, "FILE=%p, uptr=%p, val=%d, desc=%p\n",
                st, uptr, val, desc);
    else
        log_msg(NOTIFY_MSG, moi, "FILE=%p, uptr=%p, val=%d, desc=%p %s\n",
                st, uptr, val, desc, desc);
    
    // show connect channel
    // show list
    //  ret = list_service(IOM_CONNECT_CHAN, 1, &ptro, &addr);
    //      ret = send_channel_pcw(IOM_CONNECT_CHAN, addr);
    
    int chan = IOM_CONNECT_CHAN;
    int chanloc = IOM_A_MBX + chan * 4;
    out_msg("Connect channel is channel %d at %#06o\n", chan, chanloc);
    lpw_t lpw;
    parse_lpw(&lpw, chanloc, chan == IOM_CONNECT_CHAN);
    lpw.hrel = lpw.srel;
    out_msg("LPW at %#06o: %s\n", chanloc, lpw2text(&lpw, chan == IOM_CONNECT_CHAN));
    
    int addr = lpw.dcw;
    pcw_t pcw;
    t_uint64 word0, word1;
    (void) fetch_abs_pair(addr, &word0, &word1);
    decode_idcw(&pcw, 1, word0, word1);
    out_msg("PCW at %#06o: %s\n", addr, pcw2text(&pcw));
    chan = pcw.chan;
    out_msg("Channel %#o (%d):\n", chan, chan);
    addr += 2;  // skip PCW
    
    // This isn't quite right, but sufficient for debugging
    int control = 2;
    for (int i = 0; i < lpw.tally || control == 2; ++i) {
        if (i > 4096) break;
        dcw_t dcw;
        parse_dcw(chan, &dcw, addr, 1);
        if (dcw.type == idcw) {
            //dcw.fields.instr.chan = chan; // Real HW would not populate
            out_msg("DCW %d at %06o : %s\n", i, addr, dcw2text(&dcw));
            control = dcw.fields.instr.control;
        } else if (dcw.type == tdcw) {
            out_msg("DCW %d at %06o: <transfer> -- not implemented\n", i, addr);
            break;
        } else if (dcw.type == ddcw) {
            out_msg("DCW %d at %06o: %s\n", i, addr, dcw2text(&dcw));
            if (dcw.fields.ddcw.type == 0)
                control = 0;
        }
        ++addr;
        if (control != 2) {
            if (i == lpw.tally)
                out_msg("-- end of list --\n");
            else
                out_msg("-- end of list (because dcw control != 2) --\n");
        }
    }
    
    return 0;
}

