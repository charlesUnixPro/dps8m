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

// XXX Remember to free client->data when closing connection
// XXX There is a lurking bug in fnpProcessEvent(). A second 'input' messages
// XXX from a particular line could be placed in mailbox beforme the first is
// XXX processed. This could lead to the messages being picked up by MCS in
// XXX the wrong order. The quick fix is to use just a single mbx; a better
// XXX is to track the line # associated with an busy mailbox, and requeue
// XXX any message that from a line that is in a busy mailbox. I wonder how
// XXX the real DN355 dealt with this?

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_fnp2.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_faults.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "fnptelnet.h"
#include "fnpuv.h"
#include "utlist.h"
#include "uthash.h"
#include "sim_defs.h"
#include "sim_tmxr.h"
#include <regex.h>
#include "threadz.h"

__thread static bool havelock = false;

static t_stat fnpShowConfig (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetConfig (UNIT * uptr, int value, const char * cptr, void * desc);
static t_stat fnpShowNUnits (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetNUnits (UNIT * uptr, int32 value, const char * cptr, void * desc);
static t_stat fnpShowIPCname (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetIPCname (UNIT * uptr, int32 value, const char * cptr, void * desc);
static t_stat fnpShowService (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetService (UNIT * uptr, int32 value, const char * cptr, void * desc);
static t_stat fnpAttach (UNIT * uptr, const char * cptr);
static t_stat fnpDetach (UNIT *uptr);

static int findMbx (uint fnpUnitIdx);

#define N_FNP_UNITS 1 // default

UNIT fnp_unit [N_FNP_UNITS_MAX] = {
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
    {UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL}
};

static DEBTAB fnpDT [] =
  {
    { "TRACE", DBG_TRACE, NULL },
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

static MTAB fnpMod [] =
  {
    {
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "CONFIG",     /* print string */
      "CONFIG",         /* match string */
      fnpSetConfig,         /* validation routine */
      fnpShowConfig, /* display routine */
      NULL,          /* value descriptor */
      NULL   // help string
    },

    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      fnpSetNUnits, /* validation routine */
      fnpShowNUnits, /* display routine */
      "Number of FNP units in the system", /* value descriptor */
      NULL          // help
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_VALR | MTAB_NC, /* mask */ 
      0,            /* match */ 
      "IPC_NAME",     /* print string */
      "IPC_NAME",         /* match string */
      fnpSetIPCname, /* validation routine */
      fnpShowIPCname, /* display routine */
      "Set the device IPC name", /* value descriptor */
      NULL          // help
    },
    {
      MTAB_XTD | MTAB_VUN | MTAB_VALR | MTAB_NC, /* mask */ 
      0,            /* match */ 
      "SERVICE",     /* print string */
      "SERVICE",         /* match string */
      fnpSetService, /* validation routine */
      fnpShowService, /* display routine */
      "Set the device IPC name", /* value descriptor */
      NULL          // help
    },

    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };

#define FNP_UNIT_IDX(uptr) ((uptr) - fnp_unit)

static t_stat fnpReset (DEVICE * dptr);

DEVICE fnpDev = {
    "FNP",           /* name */
    fnp_unit,          /* units */
    NULL,             /* registers */
    fnpMod,           /* modifiers */
    N_FNP_UNITS,       /* #units */
    10,               /* address radix */
    31,               /* address width */
    1,                /* address increment */
    8,                /* data radix */
    9,                /* data width */
    NULL,             /* examine routine */
    NULL,             /* deposit routine */
    fnpReset,         /* reset routine */
    NULL,             /* boot routine */
    fnpAttach,             /* attach routine */
    fnpDetach,             /* detach routine */
    NULL,             /* context */
    DEV_DEBUG,        /* flags */
    0,                /* debug control flags */
    fnpDT,            /* debug flag names */
    NULL,             /* memory size change */
    NULL,             /* logical name */
    NULL,             // attach help
    NULL,             // help
    NULL,             // help context
    NULL,             // device description
    NULL
};

static int telnet_port = 6180;

struct fnpUnitData fnpUnitData [N_FNP_UNITS_MAX];

//
// The FNP communicates with Multics with in-memory mailboxes
//

struct dn355_submailbox
  {
    word36 word1; // dn355_no; is_hsla; la_no; slot_no
    word36 word2; // cmd_data_len; op_code; io_cmd
    word36 command_data [3];
    word36 word6; // data_addr, word_cnt;
    word36 pad3 [2];
  };

struct fnp_submailbox // 28 words
  {
                                                                 // AN85
    word36 word1; // dn355_no; is_hsla; la_no; slot_no    // 0      word0
    word36 word2; // cmd_data_len; op_code; io_cmd        // 1      word1
    word36 mystery [26];                                         // word2...
  };

struct mailbox
  {
    word36 dia_pcw;
    word36 mailbox_requests;
    word36 term_inpt_mpx_wd;
    word36 last_mbx_req_count;
    word36 num_in_use;
    word36 mbx_used_flags;
    word36 crash_data [2];
    struct dn355_submailbox dn355_sub_mbxes [8];
    struct fnp_submailbox fnp_sub_mbxes [4];
  };
#define MAILBOX_WORDS (sizeof (struct mailbox) / sizeof (word36))
#define TERM_INPT_MPX_WD (offsetof (struct mailbox, term_inpt_mpx_wd) / sizeof (word36))

static inline void l_putbits36_1 (word36 volatile * x, uint p, word1 val)
{
    const int n = 1;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_1: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    if (! havelock)
      lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    if (! havelock)
      unlock_mem ();
}

static inline void l_putbits36_3 (word36 volatile * x, uint p, word3 val)
{
    const int n = 3;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_3: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    if (! havelock)
      lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    if (! havelock)
      unlock_mem ();
}

static inline void l_putbits36_6 (word36 volatile * x, uint p, word6 val)
{
    const int n = 6;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_6: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    if (! havelock)
      lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    if (! havelock)
      unlock_mem ();
}

static inline void l_putbits36_9 (word36 volatile * x, uint p, word9 val)
{
    const int n = 9;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_9: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    if (! havelock)
      lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    if (! havelock)
      unlock_mem ();
}

static inline void l_putbits36_12 (word36 volatile * x, uint p, word12 val)
{
    const int n = 12;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_12: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    if (! havelock)
      lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    if (! havelock)
      unlock_mem ();
}

static inline void l_putbits36_18 (word36 volatile * x, uint p, word18 val)
{
    const int n = 18;
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("l_putbits36_18: bad args (%012"PRIo64",pos=%d)\n", *x, p);
        return;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    word36 smask = mask << (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    if (! havelock)
      lock_mem ();
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
    if (! havelock)
      unlock_mem ();
}

static void setTIMW (uint mailboxAddress, int mbx)
  {
    uint timwAddress = mailboxAddress + TERM_INPT_MPX_WD;
    l_putbits36_1 (& M [timwAddress], (uint) mbx, 1);
  }

#if 0
//
// Which IOM is FNPn connected to?
//

int lookupFnpsIomUnitNumber (int fnpUnitIdx)
  {
    return cables -> cablesFromIomToFnp [fnpUnitIdx] . iomUnitIdx;
  }
#endif

//
// Once-only initialization
//

void fnpInit(void)
  {
    // 0 sets set service to service_undefined
    memset(fnpUnitData, 0, sizeof(fnpUnitData));
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      {
        cables -> cablesFromIomToFnp [i] . iomUnitIdx = -1;
      }
    //fnpuvInit (telnet_port);
    fnpTelnetInit ();
  }

static t_stat fnpReset (UNUSED DEVICE * dptr)
  {
    //for (int i = 0; i < (int) dptr -> numunits; i ++)
      //{
        //sim_cancel (& fnp_unit [i]);
      //}
    return SCPE_OK;
  }

//
// Locate an available fnp_submailbox
//

static int findMbx (uint fnpUnitIdx)
  {
    struct fnpUnitData * fudp = & fnpUnitData [fnpUnitIdx];
    for (uint i = 0; i < 4; i ++)
      if (! fudp -> fnpMBXinUse [i])
        return (int) i;
    return -1;
  }

static void notifyCS (int mbx, int fnpno, int lineno)
  {
#ifdef FNPDBG
sim_printf ("notifyCS mbx %d\n", mbx);
#endif
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_3 (& smbxp -> word1, 0, (word3) fnpno); // dn355_no XXX
    l_putbits36_1 (& smbxp -> word1, 8, 1); // is_hsla XXX
    l_putbits36_3 (& smbxp -> word1, 9, 0); // la_no XXX
    l_putbits36_6 (& smbxp -> word1, 12, (word6) lineno); // slot_no XXX
    l_putbits36_18 (& smbxp -> word1, 18, 256); // blocks available XXX

    fudp->fnpMBXinUse [mbx] = true;
    setTIMW (fudp->mailboxAddress, mbx + 8);
    send_terminate_interrupt ((uint) cables -> cablesFromIomToFnp [fnpno] . iomUnitIdx, (uint) cables -> cablesFromIomToFnp [fnpno] . chan_num);
  }

static void fnp_rcd_ack_echnego_init (int mbx, int fnpno, int lineno)
  {
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len
    l_putbits36_9 (& smbxp -> word2, 18, 70); // op_code ack_echnego_init
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd


    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_line_disconnected (int mbx, int fnpno, int lineno)
  {
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len
    l_putbits36_9 (& smbxp -> word2, 18, 0101); // op_code cmd_data_len
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd


    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_input_in_mailbox (int mbx, int fnpno, int lineno)
  {
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);
//sim_printf ("fnp_rcd_input_in_mailbox nPos %d\n", linep->nPos);
    l_putbits36_9 (& smbxp -> word2, 9, (word9) linep->nPos); // n_chars
    l_putbits36_9 (& smbxp -> word2, 18, 0102); // op_code input_in_mailbox
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd


// data goes in mystery [0..24]

//sim_printf ("short in; line %d tally %d\n", lineno, linep->nPos);
#if 0
{ sim_printf ("IN:  ");
for (int i = 0; i < linep->nPos; i ++)
//sim_printf ("%c", isgraph (linep->buffer [i]) ? linep->buffer [i] : '.');
if (isgraph (linep->buffer [i]))
  sim_printf ("%c", linep->buffer [i]);
else
  sim_printf ("\\%03o", linep->buffer [i]);

sim_printf ("\n");
}
#endif
    int j = 0;
    for (int i = 0; i < linep->nPos + 3; i += 4)
      {
        word36 v = 0;
        if (i < linep->nPos)
          l_putbits36_9 (& v, 0, linep->buffer [i]);
        if (i + 1 < linep->nPos)
          l_putbits36_9 (& v, 9, linep->buffer [i + 1]);
        if (i + 2 < linep->nPos)
          l_putbits36_9 (& v, 18, linep->buffer [i + 2]);
        if (i + 3 < linep->nPos)
          l_putbits36_9 (& v, 27, linep->buffer [i + 3]);
//sim_printf ("%012"PRIo64"\n", v);
        smbxp -> mystery [j ++] = v;
      }

// command_data is at mystery[25]?

    // temporary until the logic is in place XXX
    int outputChainPresent = 0;

    l_putbits36_1 (& smbxp -> mystery [25], 16, (word1) outputChainPresent);
    l_putbits36_1 (& smbxp -> mystery [25], 17, linep->input_break ? 1 : 0);


#if 0
    sim_printf ("    %012"PRIo64"\n", smbxp -> word1);
    sim_printf ("    %012"PRIo64"\n", smbxp -> word2);
    for (int i = 0; i < 26; i ++)
      sim_printf ("    %012"PRIo64"\n", smbxp -> mystery [i]);
    sim_printf ("interrupting!\n"); 
#endif

    fudp->lineWaiting [mbx] = true;
    fudp->fnpMBXlineno [mbx] = lineno;
    linep->waitForMbxDone=true;
    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_accept_input (int mbx, int fnpno, int lineno)
  {
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);
    //sim_printf ("accept_input mbx %d fnpno %d lineno %d nPos %d\n", mbx, fnpno, lineno, linep->nPos);

    l_putbits36_18 (& smbxp -> word2, 0, (word18) linep->nPos); // cmd_data_len XXX
    l_putbits36_9 (& smbxp -> word2, 18, 0112); // op_code accept_input
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    // Not in AN85...
    // It looks like we need to build the DCW list, and let CS fill in the
    // addresses. I have no idea what the limit on the tally is; i.e. when
    // does the data need to be split up into multiple buffers?
    smbxp -> mystery [0] = 1; // n_buffers?

    // DCW for buffer
    smbxp -> mystery [1] = 0;
    l_putbits36_12 (& smbxp -> mystery [1], 24, (word12) linep->nPos);

    // Command_data after n_buffers and 24 dcws
    // temporary until the logic is in place XXX
    int outputChainPresent = 0;

    l_putbits36_1 (& smbxp -> mystery [25], 16, (word1) outputChainPresent);
    l_putbits36_1 (& smbxp -> mystery [25], 17, linep->input_break ? 1 : 0);

    fudp -> fnpMBXlineno [mbx] = lineno;
    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_line_break (int mbx, int fnpno, int lineno)
  {
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_9 (& smbxp -> word2, 9, 0); // cmd_data_len XXX
    l_putbits36_9 (& smbxp -> word2, 18, 0113); // op_code line_break
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_send_output (int mbx, int fnpno, int lineno)
  {
#ifdef FNPDBG
sim_printf ("send_output\n");
#endif
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_9 (& smbxp -> word2, 9, 0); // cmd_data_len XXX
    l_putbits36_9 (& smbxp -> word2, 18, 0105); // op_code send_output
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_acu_dial_failure (int mbx, int fnpno, int lineno)
  {
    //sim_printf ("acu_dial_failure %d %d %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len XXX
    l_putbits36_9 (& smbxp -> word2, 18, 82); // op_code acu_dial_failure
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_accept_new_terminal (int mbx, int fnpno, int lineno)
  {
    //sim_printf ("accept_new_terminal %d %d %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len XXX
    l_putbits36_9 (& smbxp -> word2, 18, 64); // op_code accept_new_terminal
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    smbxp -> mystery [0] = 1; // (word36) termType;  XXX
    smbxp -> mystery [1] = 0; // (word36) chanBaud;  XXX

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_wru_timeout (int mbx, int fnpno, int lineno)
  {
    //sim_printf ("wru_timeout %d %d %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox volatile * mbxp = (struct mailbox volatile *) & M [fudp->mailboxAddress];
    struct fnp_submailbox volatile * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    l_putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len XXX
    l_putbits36_9 (& smbxp -> word2, 18, 0114); // op_code wru_timeout
    l_putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

// Process an input character according to the line discipline.
// Return true if buffer should be shipped to the CS

static inline bool processInputCharacter (struct t_line * linep, unsigned char kar)
  {

// telnet sends keyboard returns as CR/NUL. Drop the null when we see it;
    uvClientData * p = linep->client->data;
    //sim_printf ("kar %03o isTelnet %d was CR %d is Null %d\n", kar, !!p->telnetp, linep->was_CR, kar == 0);
//sim_printf ("%03o %c\n", kar, isprint(kar)? kar : '#');
    if (p && p->telnetp && linep->was_CR && kar == 0)
      {
        //sim_printf ("dropping nul\n");
        linep->was_CR = false;
        return false;
      }
    linep->was_CR = kar == 015;
    //sim_printf ("was CR %d\n", linep->was_CR);

//sim_printf ("%03o %c\n", kar, isgraph (kar) ? kar : '.');
    if (linep->service == service_login)
      {
        if (linep->echoPlex)
          {
            // echo \r, \n & \t
    
            // echo a CR when a LF is typed
            if (linep->crecho && kar == '\n')
              {
                fnpuv_start_writestr (linep->client, "\r\n");
              }
    
            // echo and inserts a LF in the users input stream when a CR is typed
            else if (linep->lfecho && kar == '\r')
              {
                fnpuv_start_writestr (linep->client, "\r\n");
              }
    
            // echo the appropriate number of spaces when a TAB is typed
            else if (linep->tabecho && kar == '\t')
              {
                // since nPos starts at 0 this'll work well with % operator
                int nCol = linep->nPos;
                // for now we use tabstops of 1,11,21,31,41,51, etc...
                nCol += 10;                  // 10 spaces/tab
                int nSpaces = 10 - (nCol % 10);
                for(int i = 0 ; i < nSpaces ; i += 1)
                  fnpuv_start_writestr (linep->client, " ");
              }
    
            // XXX slightly bogus logic here..
            // ^R ^U ^H DEL LF CR FF ETX 
            else if (kar == '\022'  || kar == '\025' || kar == '\b' ||
                     kar == 127     || kar == '\n'   || kar == '\r' ||
                     kar == '\f'    || kar == '\003')
            {
              // handled below
            }
    
            // echo character
            else
            {
                fnpuv_start_write (linep->client, (char *) & kar, 1);
            }
        } // if echoPlex
    
        // send of each and every character
        if (linep->breakAll)
          {
            linep->buffer[linep->nPos ++] = kar;
            linep->buffer[linep->nPos] = 0;
            linep->input_break = true;
            linep->accept_input = 1;
            return true;
          }
    
        if ((linep-> frame_begin != 0 &&
             linep-> frame_begin == kar) ||
            (linep-> frame_end != 0 &&
             linep-> frame_end == kar))
          {
#if 0
            // Framing chars are dropped. Is that right?.
            if (linep->nPos != 0)
              {
                linep->accept_input = 1;
                linep->input_break = true;
                return true;
              }
            // Frame character on an empty frame; keep going.
            return false;
#else
// XXX This code assumes that only 'frame_end' is in play, as in Kermit behavior
            linep->buffer[linep->nPos++] = kar;
            // Pad to frame size with nulls
            int frsz = (int) linep->block_xfer_in_frame_sz;
            while ((size_t) linep->nPos < sizeof (linep->buffer) && linep->nPos < frsz)
              linep->buffer[linep->nPos++] = 0;
            linep->accept_input = 1;
            linep->input_break = true;
//sim_printf ("set nPos to %d\n", (int) linep->nPos);
            return true;
#endif
          }
    
        // Multics seems to want CR changed to LF
        if (kar == '\r')
          kar = '\n';
    
        switch (kar)
          {
            case '\n':          // NL
            case '\r':          // CR
            case '\f':          // FF
              {
                kar = '\n';     // translate to NL
                linep->buffer[linep->nPos++] = kar;
                linep->buffer[linep->nPos] = 0;
                linep->accept_input = 1;
                linep->input_break = true;
//sim_printf ("processInputCharacter sees NL; sets input_break\n");
                return true;
              }
    
            case 0x03:          // ETX (^C) // line break
              {
                if (linep->handleQuit)
                  {
                    linep->line_break=true;
                    // Treating line break as out of band, but pausing
                    // buffer processing. Not sure this makes any difference
                    // as the processing will resume on the next processing loop
                    return true;
                  }
              }
              break;
    
            case '\b':  // backspace
            case 127:   // delete
              {
                if (linep->nPos > 0)
                  {
                    fnpuv_start_writestr (linep->client, "\b \b");    // remove char from line
                    linep->nPos -= 1;                 // back up buffer pointer
                    linep->buffer[linep->nPos] = 0;     // remove char from buffer
                  }
                else 
                 {
                    // remove char from line
                    fnpuv_start_writestr (linep->client, "\a");
                  }
                return false;
              }
    
            case 21:    // ^U kill
              {
                linep->nPos = 0;
                linep->buffer[linep->nPos] = 0;
                fnpuv_start_writestr (linep->client, "^U\r\n");
                return false;
              }
    
            case 0x12:  // ^R
              {
                fnpuv_start_writestr (linep->client, "^R\r\n");       // echo ^R
                fnpuv_start_write (linep->client, (char *) linep->buffer, linep->nPos);
                return false;
              }

            default:
                break;
          }

       }

    // Just a character in cooked mode; append it to the buffer
    linep->buffer[linep->nPos++] = kar;
    linep->buffer[linep->nPos] = 0;

    // If we filled the buffer, move it along

    if (
        // Dialup or slave and inBuffer exhausted
        (linep->service != service_login && linep->inUsed >= linep->inSize) ||

        // Internal buffer full
        (size_t) linep->nPos >= sizeof (linep->buffer) ||

        // block xfer buffer size met
        (linep->block_xfer_out_frame_sz != 0 && linep->nPos >= (int) linep->block_xfer_out_frame_sz) ||

        // 'listen' command buffer size met
        (linep->inputBufferSize != 0 && linep->nPos >= (int) linep->inputBufferSize))
      {
        linep->accept_input = 1;
        linep->input_break = false;
        // To make IMFT work...
        if (linep->service == service_slave || linep->service == service_autocall)
          linep->input_break = true;

        return true;
      }
    return false; 
  }

static void fnpProcessBuffer (struct t_line * linep)
  {
    // The connection could have closed when we were not looking
    if (! linep->client)
      {
        if (linep->inBuffer)
          free (linep->inBuffer);
        linep->inBuffer = NULL;
        linep->inSize = 0;
        linep->inUsed = 0;
        return;
      }

    while (linep->inBuffer && linep->inUsed < linep->inSize)
       {
         unsigned char c = linep->inBuffer [linep->inUsed ++];
//sim_printf ("processing %d/%d %o '%c'\n", linep->inUsed-1, linep->inSize, c, isprint (c) ? c : '?');

         if (linep->inUsed >= linep->inSize)
           {
             free (linep->inBuffer);
             linep->inBuffer = NULL;
             linep->inSize = 0;
             linep->inUsed = 0;
             // The connection could have been closed when we weren't looking
             if (linep->client)
               fnpuv_read_start (linep->client);
           }
         if (processInputCharacter (linep, c))
           break;
       }
  }

static void fnpProcessBuffers (void)
  {
    for (int fnpno = 0; fnpno < N_FNP_UNITS_MAX; fnpno ++)
      {
        for (int lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];

            // If an accept_input request is posted, then buffer is busy.
            if (linep->accept_input)
              continue;

            // If a input command ack is pending, then buffer is busy.
            if (linep->input_reply_pending)
              continue;

            // If no data to process
            if (!linep->inBuffer)
               continue;

            fnpProcessBuffer (linep);
          }
      }
  }

//
// Called @ 100Hz to process FNP background events
//

void fnpProcessEvent (void)
  {
    // Run the libuv event loop once.
    // Handles tcp connections, drops, read data, write data done.
    fnpuvProcessEvent ();

    // Move characters from inBuffer to buffer, based on line discipline
    // and data availibility

    fnpProcessBuffers ();

    // Look for posted requests
    for (int fnpno = 0; fnpno < N_FNP_UNITS_MAX; fnpno ++)
      {
        int mbx = findMbx ((uint) fnpno);
        if (mbx == -1)
          continue;
        for (int lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];

            // Need to send a 'send_output' command to CS?

            if (linep -> send_output)
              {
                //linep -> send_output = false;
                linep->send_output --;
                if (linep->send_output == 0)
                  fnp_rcd_send_output (mbx, fnpno, lineno);
              }

            // Need to send a 'line_break' command to CS?

            else if (linep -> line_break)
              {
                fnp_rcd_line_break (mbx, fnpno, lineno);
                linep -> line_break = false;
              }

            // Need to send an 'acu_dial_failure' command to CS?

            else if (linep->acu_dial_failure)
              {
                fnp_rcd_acu_dial_failure (mbx, fnpno, lineno);
                linep->acu_dial_failure = false;
              }

            // Need to send an 'accept_new_terminal' command to CS?

// linep->listen is check here for the case of a connection that was
// made before Multics was booted to the point of setting listen.
// If the accept_new_terminal call is made then, Multics rejects
// the connection and sends a disconnect order. By checking 'listen',
// the accept requests hangs around until Multics is ready.

            else if (linep->listen && linep->accept_new_terminal)
              {
                fnp_rcd_accept_new_terminal (mbx, fnpno, lineno);
                linep->accept_new_terminal = false;
              }

            // Need to send an 'ack_echnego_init' command to CS?

            else if (linep -> ack_echnego_init)
              {
                fnp_rcd_ack_echnego_init (mbx, fnpno, lineno);
                linep -> ack_echnego_init = false;
                //linep -> send_output = true;
                linep -> send_output = SEND_OUTPUT_DELAY;
              }

            // Need to send an 'line_disconnected' command to CS?

            else if (linep -> line_disconnected)
              {
                fnp_rcd_line_disconnected (mbx, fnpno, lineno);
                linep -> line_disconnected = false;
                linep -> listen = false;
              }

            // Need to send an 'wru_timeout' command to CS?

            else if (linep -> wru_timeout)
              {
                fnp_rcd_wru_timeout (mbx, fnpno, lineno);
                linep -> wru_timeout = false;
              }

            // Need to send an 'accept_input' or 'input_in_mailbox' command to CS?

            else if (linep->accept_input && ! linep->waitForMbxDone)
              {
                if (linep->accept_input == 1)
                  {
#if 0
{
  sim_printf ("\n nPos %d:", linep->nPos);
  for (int i = 0; i < linep->nPos; i ++)
     if (isprint (linep->buffer [i]))
      sim_printf ("%c", linep->buffer [i]);
     else
      sim_printf ("\\%03o", linep->buffer [i]);
  //for (unsigned char * p = linep->buffer; *p; p ++)
     //if (isprint (*p))
      //sim_printf ("%c", *p);
     //else
      //sim_printf ("\\%03o", *p);
   sim_printf ("\n");
}
#endif
// There is a bufferful of data that needs to be sent to the CS.
// If the buffer has < 101 characters, use the 'input_in_mailbox'
// command; otherwise use the 'accept_input/input_accepted'
// sequence.

#if 0
                    fnp_rcd_accept_input (mbx, fnpno, lineno);
                    //linep->input_break = false;
                    linep->input_reply_pending = true;
                    // accept_input cleared below
#else
                    if (linep->nPos > 100)
                      {
                        fnp_rcd_accept_input (mbx, fnpno, lineno);
#ifdef FNPDBG
sim_printf ("accept_input\n");
#endif
                        //linep->input_break = false;
                        linep->input_reply_pending = true;
                        // accept_input cleared below
                      }
                    else
                      {
                        fnp_rcd_input_in_mailbox (mbx, fnpno, lineno);
#ifdef FNPDBG
sim_printf ("input_in_mailbox\n");
#endif
                        linep->nPos = 0;
                        // accept_input cleared below
                      }
#endif
                  }
                linep->accept_input --;
              }

            else
              {
                continue;
              }

            // One of the request processes may have consumed the
            // mailbox; make sure one is still available

            mbx = findMbx ((uint) fnpno);
            if (mbx == -1)
              goto nombx;
          } // for lineno
nombx:;
      } // for fnpno

#ifdef TUN
    fnpTUNProcessEvent ();
#endif
  }

static t_stat fnpShowNUnits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                              UNUSED int val, UNUSED const void * desc)
  {
    sim_printf("Number of FNP units in system is %d\n", fnpDev . numunits);
    return SCPE_OK;
  }

static t_stat fnpSetNUnits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_FNP_UNITS_MAX)
      return SCPE_ARG;
    fnpDev . numunits = (uint32) n;
    //return fnppSetNunits (uptr, value, cptr, desc);
    return SCPE_OK;
  }

//    ATTACH FNPn llll:w.x.y.z:rrrr - connect via UDP to an external FNP

static t_stat fnpAttach (UNIT * uptr, const char * cptr)
  {
    char * pfn;

    // If we're already attached, then detach ...
    if ((uptr -> flags & UNIT_ATT) != 0)
      detach_unit (uptr);

    // Make a copy of the "file name" argument.  fnp_udp_create() actually modifies
    // the string buffer we give it, so we make a copy now so we'll have
    // something to display in the "SHOW FNPn ..." command.
    pfn = (char *) calloc (CBUFSIZE, sizeof (char));
    if (pfn == NULL)
      return SCPE_MEM;
    strncpy (pfn, cptr, CBUFSIZE);

// Create the connection...



    uptr -> flags |= UNIT_ATT;
    uptr -> filename = pfn;
    return SCPE_OK;
  }

// Detach (connect) ...
static t_stat fnpDetach (UNIT * uptr)
  {
    if ((uptr -> flags & UNIT_ATT) == 0)
      return SCPE_OK;
    uptr -> flags &= ~(unsigned int) UNIT_ATT;
    free (uptr -> filename);
    uptr -> filename = NULL;
    return SCPE_OK;
  }


static t_stat fnpShowIPCname (UNUSED FILE * st, UNIT * uptr,
                              UNUSED int val, UNUSED const void * desc)
  {   
    long n = FNP_UNIT_IDX (uptr);
    if (n < 0 || n >= N_FNP_UNITS_MAX)
      return SCPE_ARG;
    sim_printf("FNP IPC name is %s\n", fnpUnitData [n] . ipcName);
    return SCPE_OK;
  }   

static t_stat fnpSetIPCname (UNIT * uptr, UNUSED int32 value,
                             UNUSED const char * cptr, UNUSED void * desc)
  {
    long n = FNP_UNIT_IDX (uptr);
    if (n < 0 || n >= N_FNP_UNITS_MAX)
      return SCPE_ARG;
    if (cptr)
      {
        strncpy (fnpUnitData [n] . ipcName, cptr, MAX_DEV_NAME_LEN - 1);
        fnpUnitData [n] . ipcName [MAX_DEV_NAME_LEN - 1] = 0;
      }
    else
      fnpUnitData [n] . ipcName [0] = 0;
    return SCPE_OK;
  }

static t_stat fnpShowService (UNUSED FILE * st, UNIT * uptr,
                              UNUSED int val, UNUSED const void * desc)
  {   
    long devnum = FNP_UNIT_IDX (uptr);
    if (devnum < 0 || devnum >= N_FNP_UNITS_MAX)
      return SCPE_ARG;
    for (uint linenum = 0; linenum < MAX_LINES; linenum ++)
      {
        enum service_types st = fnpUnitData[devnum].MState.line[linenum].service;
        switch (st)
          {
            case service_undefined:
              sim_printf("%c.%03d undefined\r\n", 'a' + (int) devnum, linenum);
              break;
            case service_login:
              sim_printf("%c.%03d login\r\n", 'a' + (int) devnum, linenum);
              break;
            case service_autocall:
              sim_printf("%c.%03d autocall\r\n", 'a' + (int) devnum, linenum);
              break;
            case service_slave:
              sim_printf("%c.%03d slave\r\n", 'a' + (int) devnum, linenum);
              break;
            default:
              sim_printf("%d.%03d ERR (%u)\r\n", 'a' + (int) devnum, linenum, st);
              break;
          }
      }
    return SCPE_OK;
  }

static t_stat fnpSetService (UNIT * uptr, UNUSED int32 value,
                             const char * cptr, UNUSED void * desc)
  {
    long devnum = FNP_UNIT_IDX (uptr);
    if (devnum < 0 || devnum >= N_FNP_UNITS_MAX)
      return SCPE_ARG;
    // set fnp3 service=30=autocall
    // set fnp3 service=31=slave
    uint linenum;
    char sn [strlen (cptr)];
    int nr = sscanf (cptr, "%u=%s", & linenum, sn);
    if (nr != 2)
      return SCPE_ARG;
    if (linenum >= MAX_LINES)
      return SCPE_ARG;
    if (strcasecmp (sn, "undefined") == 0)
      fnpUnitData[devnum].MState.line[linenum].service = service_undefined;
    else if (strcasecmp (sn, "login") == 0)
      fnpUnitData[devnum].MState.line[linenum].service = service_login;
    else if (strcasecmp (sn, "autocall") == 0)
      fnpUnitData[devnum].MState.line[linenum].service = service_autocall;
    else if (strncasecmp (sn, "slave=", 6) == 0)
      {
        uint pn;
        int nr2 = sscanf (sn, "slave=%u", & pn);
        if (nr2 != 1)
          return SCPE_ARG;
        if (pn >= 65535)
          return SCPE_ARG;
        fnpUnitData[devnum].MState.line[linenum].service = service_slave;
        fnpUnitData[devnum].MState.line[linenum].port = (int) pn;
      }
    else
      return SCPE_ARG;
    return SCPE_OK;
  }

static t_stat fnpShowConfig (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                             UNUSED const void * desc)
  {
    long fnpUnitIdx = FNP_UNIT_IDX (uptr);
    //if (fnpUnitIdx >= (long) fnpDev.numunits)
    if (fnpUnitIdx >= (long) N_FNP_UNITS_MAX)
      {
        sim_debug (DBG_ERR, & fnpDev, 
                   "fnpShowConfig: Invalid unit number %ld\n", fnpUnitIdx);
        sim_printf ("error: invalid unit number %ld\n", fnpUnitIdx);
        return SCPE_ARG;
      }

    sim_printf ("FNP unit number %ld\n", fnpUnitIdx);
    struct fnpUnitData * fudp = fnpUnitData + fnpUnitIdx;

    sim_printf ("FNP Mailbox Address:         %04o(8)\n", fudp -> mailboxAddress);
 
    return SCPE_OK;
  }


static config_list_t fnp_config_list [] =
  {
    /*  0 */ { "mailbox", 0, 07777, NULL },
    { NULL, 0, 0, NULL }
  };

static t_stat fnpSetConfig (UNIT * uptr, UNUSED int value, const char * cptr, UNUSED void * desc)
  {
    uint fnpUnitIdx = (uint) FNP_UNIT_IDX (uptr);
    //if (fnpUnitIdx >= fnpDev . numunits)
    if (fnpUnitIdx >= N_FNP_UNITS_MAX)
      {
        sim_debug (DBG_ERR, & fnpDev, "fnpSetConfig: Invalid unit number %d\n", fnpUnitIdx);
        sim_printf ("error: fnpSetConfig: invalid unit number %d\n", fnpUnitIdx);
        return SCPE_ARG;
      }

    struct fnpUnitData * fudp = fnpUnitData + fnpUnitIdx;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("fnpSetConfig", cptr, fnp_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 

            case -1: // done
              break;

            case 0: // mailbox
              fudp -> mailboxAddress = (uint) v;
              break;

            default:
              sim_debug (DBG_ERR, & fnpDev, "fnpSetConfig: Invalid cfgparse rc <%d>\n", rc);
              sim_printf ("error: fnpSetConfig: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG; 
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }

#if 0
t_stat fnpLoad (UNUSED int32 arg, const char * buf)
  {
    FILE * fileref = fopen (buf, "r");
    if (! fileref)
    {
        sim_printf("Couldn't open %s\n", buf);
        return SCPE_ARG;
    }

    char buff [1024];
    bool havename = false;
    uint devnum = 0;
    uint linenum = 0;
    while (fgets (buff, sizeof (buff), fileref))
      {
        char * p = trim (buff);   // trim leading and trailing whitespace
        if (p [0] == '#')  // '#' as first non-white charater is comment line
          continue;
        if (p [0] == 0)          // blank line
          continue;;
        
        char * first  = trim (Strtok (p, ":"));  // stuff to the left of ':'
        char * second = trim (Strtok (NULL, ":;")); // stuff to the right of ':'
        char dev;
        if (strcmp (first, "name") == 0)
          {
            int n = sscanf (second, "%c.h%u", & dev, & linenum);
            if (n != 2 || dev < 'a' || dev > 'h' || linenum > MAX_LINES)
              {
                sim_printf ("fnpLoad skipping '%s'; n %d dev %c, linenum %u\n", buff, n, dev, linenum);
                continue;
              }
            devnum = (uint) (dev - 'a');
            havename = true;
            // CMF format sets the default service to login
            fnpUnitData[devnum].MState.line[linenum].service = service_login;                    
          }
        else if (havename && second && strcmp (first, "service") == 0)
          {
            if (strcmp (second, "login") == 0)
              fnpUnitData[devnum].MState.line[linenum].service = service_login;
            else if (strcmp (second, "autocall") == 0)
              fnpUnitData[devnum].MState.line[linenum].service = service_autocall;                   
            else if (strcmp (second, "slave") == 0)
              fnpUnitData[devnum].MState.line[linenum].service = service_slave;                   
            else if (strcmp (second, "offline") == 0)
              fnpUnitData[devnum].MState.line[linenum].service = service_undefined;                   
            else
              sim_printf ("service type '%s' not recognized; skipping\n", second);
          }
// This is not part of the CMF language, but I need away to set some additional
// parameters
        else if (havename && second && strcmp(first, "port") == 0)
        {
            trim (second);
            char * end;
            long port = strtol (second, & end, 0);
            if (* end || port < 0 || port >= 65535)
              {
                sim_printf ("can't parse fromport '%s'; ignored\n", second);
              }
            else
              {
                fnpUnitData[devnum].MState.line[linenum].port = (int) port;
              }
//sim_printf ("%s fromport %d\n", current->multics.name, current->multics.fromport);
        }

        else if (strcmp (first, "end;") == 0)
          {
            break;
          }


// Ingored
        else if (strcmp (first, "Service") == 0 ||
                 strcmp (first, "Charge") == 0 ||
                 strcmp (first, "Terminal_type") == 0 ||
                 strcmp (first, "Line_type") == 0 ||
                 strcmp (first, "Baud") == 0 ||
                 strcmp (first, "FNP_required_up_time") == 0 ||
                 strcmp (first, "FNP") == 0 ||
                 strcmp (first, "type") == 0 ||
                 strcmp (first, "memory") == 0 ||
                 strcmp (first, "lsla") == 0 ||
                 strcmp (first, "hsla") == 0 ||
                 strcmp (first, "image") == 0 ||
                 strcmp (first, "service") == 0 ||
                 strcmp (first, "attributes") == 0)
          {
            // Ignored
          }
        else
          sim_printf ("fnpLoad '%s' not recognized; skipping\n", buff);
      }     
    fclose (fileref);
    return SCPE_OK;
  }
#endif

t_stat fnpServerPort (UNUSED int32 arg, const char * buf)
  {
    int n = atoi (buf);
    if (n < 1 || n > 65535)
      return SCPE_ARG;
    telnet_port = n;
    sim_printf ("FNP telnet server port set to %d\n", n);
    //fnpuvInit (telnet_port);
    return SCPE_OK;
  }

t_stat fnpStart (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_printf ("FNP force start\n");
    fnpuvInit (telnet_port);
    return SCPE_OK;
  }


#define PROMPT  "HSLA Port ("

void fnpConnectPrompt (uv_tcp_t * client)
  {
    fnpuv_start_writestr (client, PROMPT);
    bool first = true;
    for (int fnpno = 0; fnpno < N_FNP_UNITS_MAX; fnpno ++)
      {
        for (int lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];
            if (linep->service == service_login && ! linep->client)
              {
                if (! first)
                  fnpuv_start_writestr (client, ",");
                char name [16];
                first = false;
                sprintf (name, "%c.h%03d", 'a' + fnpno, lineno);
                fnpuv_start_writestr (client, name);
              }
          }
      }
    fnpuv_start_writestr (client, ")? ");
  }

void processLineInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread)
  {
    uvClientData * p = (uvClientData *) client->data;
    uint fnpno = p -> fnpno;
    uint lineno = p -> lineno;
    if (fnpno >= N_FNP_UNITS_MAX || lineno >= MAX_LINES)
      {
        sim_printf ("bogus client data\n");
        return;
      }
//sim_printf ("assoc. %d.%d nread %ld <%*s>\n", fnpno, lineno, nread, (int) nread, buf);
//{for (int i = 0; i < nread; i ++) sim_printf (" %03o", buf[i]);
 //sim_printf ("\n");
//}

    if (! fnpUnitData[fnpno].MState.accept_calls)
      {
        fnpuv_start_writestr (client, "Multics is not accepting calls\r\n");
        return;
      }
    struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];
    if (! linep->listen)
      {
        fnpuv_start_writestr (client, "Multics is not listening to this line\r\n");
        return;
      }

// By design, inBuffer overun shouldn't happen, but it has been seen in IMFT.
// (When the TCP backs up, the buffers are merged so that larger and larger 
// reads occur. When the backedup buffer exceeds 65536, libev calls the read
// callback twice in a row, once with the first 65536, and the next with the
// remaining.
// Cope with it my realloc'ing the buffer and appending the new data. Ugh.
    if (linep->inBuffer)
      {
        sim_warn ("inBuffer overrun\n");
        unsigned char * new = realloc (linep->inBuffer, (unsigned long) (linep->inSize + nread));
        if (! new)
          {
            sim_warn ("inBuffer realloc fail; dropping data\n");
            goto done;
          }
        memcpy (new + linep->inSize, buf, (unsigned long) nread);
        linep->inSize += nread;
        linep->inBuffer = new;
      }
    else
      {
        linep->inBuffer = malloc ((unsigned long) nread);
        if (! linep->inBuffer)
          {
            sim_warn ("inBuffer malloc fail;  dropping data\n");
            goto done;
          }
        memcpy (linep->inBuffer, buf, (unsigned long) nread);
        linep->inSize = (uint) nread;
        linep->inUsed = 0;
      }

done:;
    // Prevent further reading until this buffer is consumed
    fnpuv_read_stop (client);
  }

void processUserInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread)
  {
    uvClientData * p = (uvClientData *) client->data;
    for (ssize_t nchar = 0; nchar < nread; nchar ++)
      {
        unsigned char kar = buf [nchar];

        if (kar == 0x1b || kar == 0x03)             // ESCape ('\e') | ^C
          {
            close_connection ((uv_stream_t *) client);
            return;
          }

        // buffer too full for anything more?
        if (p->nPos >= sizeof(p->buffer))
          {
            // yes. Only allow \n, \r, ^H, ^R
            switch (kar)
              {
                case '\b':  // backspace
                case 127:   // delete
                  {
                    if (p->nPos)
                      {
                        fnpuv_start_writestr (client, "\b \b");    // remove char from line
                        p->buffer[p->nPos] = 0;     // remove char from buffer
                        p->nPos -= 1;                 // back up buffer pointer
                      }
                  }
                  break;

                case '\n':
                case '\r':
                  {
                    p->buffer[p->nPos] = 0;
                    goto check;
                  }

                case 0x12:  // ^R
                  {
                    fnpuv_start_writestr (client, "^R\r\n");       // echo ^R
                    fnpConnectPrompt (client);
                    fnpuv_start_writestr (client, p->buffer);
                  }
                 break;

                default:
                  break;
              } // switch kar
            continue; // process next character in buffer
          } // if buffer full

        if (isprint (kar))   // printable?
          {
            fnpuv_start_write (client, (char *) & kar, 1);
            p->buffer[p->nPos++] = (char) kar;
          }
        else
          {
            switch (kar)
              {
                case '\b':  // backspace
                case 127:   // delete
                  {
                    if (p->nPos)
                      {
                        fnpuv_start_writestr (client, "\b \b");    // remove char from line
                        p->buffer[p->nPos] = 0;     // remove char from buffer
                        p->nPos -= 1;                 // back up buffer pointer
                      }
                  }
                  break;

                case '\n':
                case '\r':
                  {
                    p->buffer[p->nPos] = 0;
                    goto check;
                  }

                case 0x12:  // ^R
                  {
                    fnpuv_start_writestr (client, "^R\r\n");       // echo ^R
                    fnpConnectPrompt (client);
                    fnpuv_start_writestr (client, p->buffer);
                  }
                  break;

                default:
                  break;
              } // switch kar
          } // not printable
      } // for nchar
    return;

check:;
    char cpy [p->nPos + 1];
    memcpy (cpy, p->buffer, p->nPos);
    cpy [p->nPos] = 0;
    trim (cpy);
    sim_printf ("<%s>", cpy);
    p->nPos = 0;
    fnpuv_start_writestr (client, "\r\n");


    uint fnpno = 0;
    uint lineno = 0;

    if (strlen (cpy))
      {
        char fnpcode;
        int cnt = sscanf (cpy, "%c.h%u", & fnpcode, & lineno);
//sim_printf ("cnt %d fnpcode %c lineno %d\n", cnt, fnpcode, lineno);
        if (cnt != 2 || fnpcode < 'a' || fnpcode > 'h' || lineno >= MAX_LINES)
          {
            fnpuv_start_writestr (client, "can't parse\r\n");
            goto reprompt;
          }
        fnpno = (uint) (fnpcode - 'a');
        if (fnpUnitData[fnpno].MState.line[lineno].service != service_login ||
            fnpUnitData[fnpno].MState.line[lineno].client)
          {
            fnpuv_start_writestr (client, "not availible\r\n");
            goto reprompt;
          }
        goto associate;
      }
    else
      {
        for (fnpno = 0; fnpno < N_FNP_UNITS_MAX; fnpno ++)
          {
            for (lineno = 0; lineno < MAX_LINES; lineno ++)
              {
                if (fnpUnitData[fnpno].MState.line[lineno].service == service_login &&
                    ! fnpUnitData[fnpno].MState.line[lineno].client)
                  {
                    goto associate;
                  }
              }
          }
        fnpuv_start_writestr (client, "not available\r\n");
        goto reprompt;
      }
reprompt:;
    fnpConnectPrompt (client);
    return;

associate:;

    fnpUnitData[fnpno].MState.line[lineno].client = client;
//sim_printf ("associated %c.%03d %p\n", fnpno + 'a', lineno, client);
    p -> assoc = true;
    p -> fnpno = fnpno;
    p -> lineno = lineno;
    // Only enable read when Multics can accept it.
    //uv_read_stop ((uv_stream_t *) client);

    char buf2 [1024];

    struct sockaddr name;
    int namelen = sizeof (name);
    int ret = uv_tcp_getpeername (client, & name, & namelen);
    if (ret < 0)
      {
        sim_printf ("CONNECT (addr err %d) to %c.h%03d\n", ret, fnpno +'a', lineno);
      }
    else
      {
        struct sockaddr_in * p = (struct sockaddr_in *) & name;
        sim_printf ("CONNECT %s to %c.h%03d\n", inet_ntoa (p -> sin_addr), fnpno +'a', lineno);
      }

    sprintf (buf2, "Attached to line %c.h%03d\r\n", fnpno +'a', lineno);
    fnpuv_start_writestr (client, buf2);

    if (! fnpUnitData[fnpno].MState.accept_calls)
      fnpuv_start_writestr (client, "Multics is not accepting calls\r\n");
    else if (! fnpUnitData[fnpno].MState.line[lineno].listen)
      fnpuv_start_writestr (client, "Multics is not listening to this line\r\n");

    fnpUnitData[fnpno].MState.line[lineno].accept_new_terminal = true;
    fnpUnitData[fnpno].MState.line[lineno].was_CR = false;
    ltnRaw (p->telnetp);
  }

void startFNPListener (void)
  {
    fnpuvInit (telnet_port);
  }

