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

// XXX There is a lurking bug in fnpProcessEvent(). A second 'input' messages
// XXX from a particular line could be placed in mailbox beforme the first is
// XXX processed. This could lead to the messages being picked up by MCS in
// XXX the wrong order. The quick fix is to use just a single mbx; a better
// XXX is to track the line # associated with an busy mailbox, and requeue
// XXX any message that from a line that is in a busy mailbox. I wonder how
// XXX the real DN355 dealt with this?

////
//
// 3270 station to CS data flow
//
// On connection:
//
//    Station number is assigned.
//    Connection is saved:
//      fnpData.ibm3270ctlr[ASSUME0].stations[stn_no].client = client;
//    Read callback regeistered.
//      client->data->read_cb = fnpuv_3270_readcb;
//    Telnet negotiation is started:
//      client->data->telnetp = ltnConnect3270 (client);
//    DPS8 banner sent:
//      fnp3270ConnectPrompt (client);
//
//  Read data callback:
//
//    fnpuv_3270_readcb calls process3270Input.
//    process3270Input appends the data to stn_in_buffer.
//
//  Read EOT callback:
//
//   evHandler calls fnpuv_recv_eor().
//   fnpuv_recv_eor() call fnpRecvEOR ().
//   fnpRecvEOR():
//     fnpData.ibm3270ctlr[ASSUME0].stations[p->stationNo].EORReceived = true;
//     fnpData.ibm3270ctlr[ASSUME0].stations[p->stationNo].hdr_sent = false;
//
//  fnpProcessEvent() event loop calls fnp_process_3270_event().
//    fnp_process_3270_event():
//      if polling
//        for each station
//          if (stnp->EORReceived)
//            stnp->EORReceived = false;
//            ctlrp->sending_stn_in_buffer = true;
//            fnpuv3270Poll (false);
//
//  fnpProcessEvent() event loop calls fnp_process_3270_event().
//    fnp_process_3270_event():
//      if (fnpData.ibm3270ctlr[ASSUME0].sending_stn_in_buffer)
//        send_stn_in_buffer ();



#define ASSUME0 0

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_scu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_cpu.h"
#include "dps8_fnp2.h"
#include "fnptelnet.h"
#include "fnpuv.h"
#include "dps8_utils.h"
#include "utlist.h"
#include "uthash.h"

#include "sim_defs.h"
#include "sim_tmxr.h"
#include <regex.h>

#define DBG_CTR 1

#if defined(THREADZ) || defined(LOCKLESS)
#include "threadz.h"
#endif

static t_stat fnpShowConfig (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetConfig (UNIT * uptr, int value, const char * cptr, void * desc);
static t_stat fnpShowStatus (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpShowNUnits (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetNUnits (UNIT * uptr, int32 value, const char * cptr, void * desc);
static t_stat fnpShowIPCname (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetIPCname (UNIT * uptr, int32 value, const char * cptr, void * desc);
static t_stat fnpShowService (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetService (UNIT * uptr, int32 value, const char * cptr, void * desc);

static int findMbx (uint fnpUnitIdx);

#define N_FNP_UNITS 1 // default

UNIT fnp_unit [N_FNP_UNITS_MAX] =
 {
    [0 ... N_FNP_UNITS_MAX - 1] =
      {
        UDATA (NULL, UNIT_DISABLE | UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL
      }
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
      MTAB_XTD | MTAB_VUN | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "STATUS",     /* print string */
      "STATUS",         /* match string */
      NULL,         /* validation routine */
      fnpShowStatus, /* display routine */
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

DEVICE fnp_dev = {
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
    NULL,             /* attach routine */
    NULL,             /* detach routine */
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

t_fnpData fnpData;

#define l_putbits36_1 putbits36_1
#define l_putbits36_3 putbits36_3
#define l_putbits36_6 putbits36_6
#define l_putbits36_9 putbits36_9
#define l_putbits36_12 putbits36_12
#define l_putbits36_18 putbits36_18

void setTIMW (uint iom_unit_idx, uint chan, word24 mailboxAddress, int mbx)
  {
    word24 timwAddress = mailboxAddress + TERM_INPT_MPX_WD;
    word36 data;
    iom_direct_data_service (iom_unit_idx, chan, timwAddress, & data, direct_read_clear);
    l_putbits36_1 (& data, (uint) mbx, 1);
    iom_direct_data_service (iom_unit_idx, chan, timwAddress, & data, direct_store);
  }

#ifdef SCUMEM
uint get_scu_unit_idx_iom (uint fnp_unit_idx, word24 addr, word24 * offset)
  {
    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
// XXX can query_IOM_SCU_bank_map return -1 here? if so, what to do?
// The address is known to reside in the bootload SCU; we can't get to here unless that is working.
    uint scu_unit_num = (uint) query_IOM_SCU_bank_map (iom_unit_idx, addr, offset);
    uint scu_unit_idx = cables->iom_to_scu[iom_unit_idx][scu_unit_num].scu_unit_idx;
    return scu_unit_idx;
  }
#endif

//
// Once-only initialization
//

void fnpInit(void)
  {
    // 0 sets set service to service_undefined
    memset(& fnpData, 0, sizeof(fnpData));
    fnpData.telnet_address = strdup ("0.0.0.0");
    fnpData.telnet_port = 6180;
    fnpData.telnet3270_port = 3270;
    fnpTelnetInit ();
    fnp3270Init ();
  }

static t_stat fnpReset (UNUSED DEVICE * dptr)
  {
#if 0
    for (int i = 0; i < (int) dptr -> numunits; i ++)
      {
        //sim_cancel (& fnp_unit [i]);
      }
#endif
    return SCPE_OK;
  }

//
// Locate an available fnp_submailbox
//

static int findMbx (uint fnpUnitIdx)
  {
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnpUnitIdx];
    for (uint i = 0; i < 4; i ++)
      if (! fudp -> fnpMBXinUse [i])
        return (int) i;
    return -1;
  }

static void notifyCS (uint mbx, int fnp_unit_idx, int lineno)
  {
#ifdef FNPDBG
sim_printf ("notifyCS mbx %d\n", mbx);
#endif
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_3 (& data, 0, (word3) fnp_unit_idx); // dn355_no XXX
    l_putbits36_1 (& data, 8, 1); // is_hsla XXX
    l_putbits36_3 (& data, 9, 0); // la_no XXX
    l_putbits36_6 (& data, 12, (word6) lineno); // slot_no XXX
    l_putbits36_18 (& data, 18, 256); // blocks available XXX
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD1, & data, direct_store);

    fudp->fnpMBXinUse [mbx] = true;

    setTIMW (iom_unit_idx, chan_num, fudp->mailboxAddress, (int)(mbx + 8));

    sim_debug (DBG_TRACE, & fnp_dev, "[%d]notifyCS %d %d\n", lineno, mbx, chan_num);
    send_general_interrupt (iom_unit_idx, chan_num, imwTerminatePic);
  }

static void fnp_rcd_ack_echnego_init (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd ack_echnego_init\n", lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 2); // cmd_data_len
    l_putbits36_9 (& data, 18, 70); // op_code ack_echnego_init
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_line_disconnected (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd line_disconnected\n", lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 2); // cmd_data_len
    l_putbits36_9 (& data, 18, 0101); // op_code cmd_data_len
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_input_in_mailbox (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd input_in_mailbox\n", lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    struct t_line * linep = & fudp->MState.line[lineno];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    uint n_chars = min(linep->nPos, 100);
    
//Sim_printf ("fnp_rcd_input_in_mailbox nPos %d\n", linep->nPos);
    word36 data = 0;
    l_putbits36_9 (& data, 9, (word9) n_chars); // n_chars
    l_putbits36_9 (& data, 18, 0102); // op_code input_in_mailbox
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

// data goes in mystery [0..24]

//sim_printf ("short in; line %d tally %d\n", lineno, linep->nPos);
#if 0
if_sim_debug (DBG_TRACE, & fnp_dev) {
{ sim_printf ("[[%d]FNP emulator: short IN: '", lineno);
for (int i = 0; i < linep->nPos; i ++)
{
if (isgraph (linep->buffer [i]))
sim_printf ("%c", linep->buffer [i]);
else
sim_printf ("\\%03o", linep->buffer [i]);
}
sim_printf ("']\n");
}
}
#endif
#if 0
{ sim_printf ("IN:  ");
for (int i = 0; i < n_chars; i ++)
//sim_printf ("%c", isgraph (linep->buffer [i]) ? linep->buffer [i] : '.');
if (isgraph (linep->buffer [i]))
  sim_printf ("%c", linep->buffer [i]);
else
  sim_printf ("\\%03o", linep->buffer [i]);

sim_printf ("\n");
}
#endif
    uint j = 0;
    for (int i = 0; i < n_chars; i += 4, j++)
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
	iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+MYSTERY+j, & v, direct_store);
      }

// command_data is at mystery[25]?

    // temporary until the logic is in place XXX
    int outputChainPresent = 0;

    data = 0;
    l_putbits36_1 (& data, 16, (word1) outputChainPresent);
    l_putbits36_1 (& data, 17, linep->input_break ? 1 : 0);
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+INP_COMMAND_DATA, & data, direct_store);

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
    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_line_status  (uint mbx, int fnp_unit_idx, int lineno)
  {
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    struct t_line * linep = & fudp->MState.line[lineno];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 2); // cmd_data_len
    l_putbits36_9 (& data, 18, 0124); // op_code accept_input
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+MYSTERY+0, & linep->lineStatus0, direct_store);
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+MYSTERY+1, & linep->lineStatus1, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_accept_input (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd accept_input\n", lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    struct t_line * linep = & fudp->MState.line[lineno];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    //sim_printf ("accept_input mbx %d fnp_unit_idx %d lineno %d nPos %d\n", mbx, fnp_unit_idx, lineno, linep->nPos);
    word36 data = 0;
    l_putbits36_18 (& data, 0, (word18) linep->nPos); // cmd_data_len XXX
    l_putbits36_9 (& data, 18, 0112); // op_code accept_input
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    // AN85 is just wrong. CS expects us to specify the number of buffers
    // and sizes.

    data = 1;
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+N_BUFFERS, & data, direct_store);
    // DCW for buffer (1)
    data = 0;
    l_putbits36_12 (& data, 24, (word12) linep->nPos);
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+DCWS+0, & data, direct_store);

    // temporary until the logic is in place XXX
    word1 output_chain_present = 1;

    data = 0;
    l_putbits36_1 (& data, 16, (word1) output_chain_present);
    l_putbits36_1 (& data, 17, linep->input_break ? 1 : 0);
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+INP_COMMAND_DATA, & data, direct_store);

    fudp -> fnpMBXlineno [mbx] = lineno;
    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_line_break (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd line_break\n", lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 0); // cmd_data_len XXX
    l_putbits36_9 (& data, 18, 0113); // op_code line_break
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_send_output (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd send_output\n", lineno);
#ifdef FNPDBG
sim_printf ("send_output\n");
#endif
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 0); // cmd_data_len XXX
    l_putbits36_9 (& data, 18, 0105); // op_code send_output
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_acu_dial_failure (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd acu_dial_failure\n", lineno);
    //sim_printf ("acu_dial_failure %d %d %d\n", mbx, fnp_unit_idx, lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 2); // cmd_data_len XXX
    l_putbits36_9 (& data, 18, 82); // op_code acu_dial_failure
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_accept_new_terminal (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd accept_new_terminal\n", lineno);
    //sim_printf ("accept_new_terminal %d %d %d\n", mbx, fnp_unit_idx, lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    struct t_line * linep = & fudp->MState.line[lineno];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 2); // cmd_data_len XXX
    l_putbits36_9 (& data, 18, 64); // op_code accept_new_terminal
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

//  pcb.line_type, dialup_info.line_type = bin (sub_mbx.command_data (1), 17);
//  if sub_mbx.command_data (2)
//    then pcb.baud_rate = baud_table (bin (sub_mbx.command_data (2), 17));

// declare   (LINE_MC            initial (-2),
//            LINE_TELNET        initial (-1),
//            LINE_UNKNOWN       initial (0),
//            LINE_ASCII         initial (1),
//            LINE_1050          initial (2),
//            LINE_2741          initial (3),
//            LINE_ARDS          initial (4),
//            LINE_SYNCH         initial (5),
//            LINE_G115          initial (6),
//            LINE_BSC           initial (7),
//            LINE_ETX           initial (8),
//            LINE_VIP           initial (9),
//            LINE_ASYNC1        initial (10),
//            LINE_ASYNC2        initial (11),
//            LINE_ASYNC3        initial (12),
//            LINE_SYNC1         initial (13),
//            LINE_SYNC2         initial (14),
//            LINE_SYNC3         initial (15),
//            LINE_POLLED_VIP    initial (16),
//            LINE_X25LAP        initial (17),
//            LINE_HDLC          initial (18),
//            LINE_COLTS         initial (19),
//            LINE_DSA           initial (20),
//            LINE_HASP_OPR      initial (21)
//          ) fixed bin internal static options (constant);

// dcl 1 dialup_info aligned, /* for use with DIALUP interrupt */
//     2 line_type fixed bin (9) unal uns,
//     2 buffer_pad fixed bin (9) unal uns, /* free space multiplexer would like in output bufs */
//     2 baud_rate fixed bin (18) unal uns,
//     2 max_buf_size fixed bin (9) unal uns,
//     2 receive_mode_device bit (1) unal, /* device must be told to enter receive mode */
//     2 pad bit (26) unal;

    data = 0;
    l_putbits36_9 (& data, 27, linep->lineType); // ??? 0 instead of 27 ?
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+MYSTERY+0, & data, direct_store);
    data = 0;
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+MYSTERY+1, & data, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

static void fnp_rcd_wru_timeout (uint mbx, int fnp_unit_idx, int lineno)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%d]rcd wru_timeout\n", lineno);
    //sim_printf ("wru_timeout %d %d %d\n", mbx, fnp_unit_idx, lineno);
    struct fnpUnitData_s * fudp = & fnpData.fnpUnitData [fnp_unit_idx];
    word24 fsmbx = fudp->mailboxAddress + FNP_SUB_MBXES + mbx*FNP_SUB_MBX_SIZE;

    uint ctlr_port_num = 0; // FNPs are single ported
    uint iom_unit_idx = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].iom_unit_idx;
    uint chan_num = cables->fnp_to_iom[fnp_unit_idx][ctlr_port_num].chan_num;

    word36 data = 0;
    l_putbits36_9 (& data, 9, 2); // cmd_data_len XXX
    l_putbits36_9 (& data, 18, 0114); // op_code wru_timeout
    l_putbits36_9 (& data, 27, 1); // io_cmd rcd
    iom_direct_data_service (iom_unit_idx, chan_num, fsmbx+WORD2, & data, direct_store);

    notifyCS (mbx, fnp_unit_idx, lineno);
  }

// Process an input character according to the line discipline.
// Return true if buffer should be shipped to the CS

static inline bool processInputCharacter (struct t_line * linep, unsigned char kar, UNUSED bool endOfBuffer)
  {
    if (! linep->line_client)
      {
        sim_warn ("processInputCharacter bad client\r\n");
        return false;
      }
#ifdef TUN
    // TUN doesn't have a client
    if (! linep->is_tun)
#endif
      {
// telnet sends keyboard returns as CR/NUL. Drop the null when we see it;
        uvClientData * p = linep->line_client->data;
        //sim_printf ("kar %03o isTelnet %d was CR %d is Null %d\n", kar, !!p->telnetp, linep->was_CR, kar == 0);
//sim_printf ("%03o %c\n", kar, isgraph(kar)? kar : '#');
        if (p && p->telnetp && linep->was_CR && kar == 0)
          {
            //sim_printf ("dropping nul\n");
            linep->was_CR = false;
            return false;
          }
        linep->was_CR = kar == 015;
        //sim_printf ("was CR %d\n", linep->was_CR);
      }

//sim_printf ("%03o %c\n", kar, isgraph (kar) ? kar : '.');
    if (linep->service == service_login)
      {
        if (linep->echoPlex)
          {
            // echo \r, \n & \t
    
            // echo a CR when a LF is typed
            if (linep->crecho && kar == '\n')
              {
                fnpuv_start_writestr (linep->line_client, (unsigned char *) "\r\n");
              }
    
            // echo and inserts a LF in the users input stream when a CR is typed
            else if (linep->lfecho && kar == '\r')
              {
                fnpuv_start_writestr (linep->line_client, (unsigned char *) "\r\n");
              }
    
            // echo the appropriate number of spaces when a TAB is typed
            else if (linep->tabecho && kar == '\t')
              {
                // since nPos starts at 0 this'll work well with % operator
                uint nCol = linep->nPos;
                // for now we use tabstops of 1,11,21,31,41,51, etc...
                nCol += 10;                  // 10 spaces/tab
                int nSpaces = 10 - ((int) nCol % 10);
                for(int i = 0 ; i < nSpaces ; i += 1)
                  fnpuv_start_writestr (linep->line_client, (unsigned char *) " ");
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
                unsigned char str [2] = { kar, 0 };
                fnpuv_start_writestr (linep->line_client, str);
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
                    fnpuv_start_writestr (linep->line_client, (unsigned char *) "\b \b");    // remove char from line
                    linep->nPos -= 1;                 // back up buffer pointer
                    linep->buffer[linep->nPos] = 0;     // remove char from buffer
                  }
                else 
                 {
                    // remove char from line
                    fnpuv_start_writestr (linep->line_client, (unsigned char *) "\a");
                  }
                return false;
              }
    
            case 21:    // ^U kill
              {
                linep->nPos = 0;
                linep->buffer[linep->nPos] = 0;
                fnpuv_start_writestr (linep->line_client, (unsigned char *) "^U\r\n");
                return false;
              }
    
            case 0x12:  // ^R
              {
                fnpuv_start_writestr (linep->line_client, (unsigned char *) "^R\r\n");       // echo ^R
                fnpuv_start_writestr (linep->line_client, linep->buffer);
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
        // Dial out or slave and inBuffer exhausted
        ((linep->service == service_autocall || linep->service == service_slave) && linep->inUsed >= linep->inSize) ||

        // Internal buffer full
        (size_t) linep->nPos >= sizeof (linep->buffer) ||

#if 0
        // block xfer buffer size met
        (linep->block_xfer_out_frame_sz != 0 && linep->nPos >= linep->block_xfer_out_frame_sz) ||

        // 'listen' command buffer size met
        (linep->inputBufferSize != 0 && linep->nPos >= (int) linep->inputBufferSize))
#endif
        ((linep->block_xfer_out_frame_sz != 0)
          ?
            // block xfer buffer size met
            (linep->nPos >= (int) linep->block_xfer_out_frame_sz)
          :
            // 'listen' command buffer size met
            (linep->inputBufferSize != 0 && linep->nPos >= (int) linep->inputBufferSize))
        )  
      {
        linep->accept_input = 1;
        linep->input_break = false;
        // To make IMFT work...
        if (linep->service == service_slave || linep->service == service_autocall)
          {
#ifdef TUN
            if (linep->is_tun)
              linep->input_break = endOfBuffer;
            else
#endif
              linep->input_break = true;
          }

        return true;
      }
    return false; 
  }

// The 3270 controller received a EOR

void fnpRecvEOR (uv_tcp_t * client)
  {
    if (! client || ! client->data)
      {
        sim_warn ("fnpRecvEOR bad client data\r\n");
        return;
      }
    uvClientData * p = client->data;
    fnpData.ibm3270ctlr[ASSUME0].stations[p->stationNo].EORReceived = true;
    fnpData.ibm3270ctlr[ASSUME0].stations[p->stationNo].hdr_sent = false;
  }

static void fnpProcessBuffer (struct t_line * linep)
  {
    // The connection could have closed when we were not looking
#ifdef TUN
    if ((! linep->is_tun) && ! linep->line_client)
#else
    if (! linep->line_client)
#endif
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
         bool eob = linep->inUsed >= linep->inSize;
         if (eob)
           {
             free (linep->inBuffer);
             linep->inBuffer = NULL;
             linep->inSize = 0;
             linep->inUsed = 0;
             // The connection could have been closed when we weren't looking
             if (linep->line_client)
               fnpuv_read_start (linep->line_client);
           }
         if (linep->service == service_3270)
           {
             linep->buffer[linep->nPos++] = c;
             linep->buffer[linep->nPos] = 0;
             continue;
           }
         if (processInputCharacter (linep, c, eob))
           break;
       }
  }

static void fnpProcessBuffers (void)
  {
    uint numunits = (uint) fnp_dev.numunits;
    for (uint fnp_unit_idx = 0; fnp_unit_idx < numunits; fnp_unit_idx ++)
      {
        if (! fnpData.fnpUnitData[fnp_unit_idx].fnpIsRunning)
          continue;
        for (uint lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            struct t_line * linep = & fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno];

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

//  dcl 1 line_stat aligned,
//      2 op fixed binary (17) unaligned,                       /* contains reason for status */
//      2 val (3) fixed binary (17) unaligned;


//  /* Values for line_stat.op */
//  
//  dcl (BID_FAILED                    initial (1),
//       BAD_BLOCK                     initial (2),
//       REVERSE_INTERRUPT             initial (3),
//       TOO_MANY_NAKS                 initial (4),
//       FNP_WRITE_STATUS              initial (5),
//       IBM3270_WRITE_COMPLETE        initial (6),
//       IBM3270_WACK_MESSAGE          initial (7),
//       IBM3270_WRITE_EOT             initial (8),
//       IBM3270_WRITE_ABORT           initial (9),
//       IBM3270_SELECT_FAILED         initial (10),
//       IBM3270_WACK_SELECT           initial (11),
//       IBM3270_NAK_OUTPUT            initial (12),
//       HASP_INIT_COMPLETE            initial (13),
//       HASP_FOREIGN_SWAB_RESET       initial (14))
//            fixed binary static options (constant);

// Send a message to Multics

void set_3270_write_complete (UNUSED uv_tcp_t * client)
  {
    //uvClientData * p = client->data;
//sim_printf ("set_3270_write_complete %p stn_no %d\r\n", p, p->stationNo);
#ifdef FNP2_DEBUG
sim_printf ("set_3270_write_complete\r\n");
#endif
    //fnpData.ibm3270ctlr[ASSUME0].stations[p->stationNo].write_complete = true;
    fnpData.ibm3270ctlr[ASSUME0].write_complete = true;
  }

static void send_3270_msg (uint ctlr_no, unsigned char * msg, size_t len, bool brk)
  {
#if 0
sim_printf ("send_3270_msg:");
for (size_t i = 0; i < len; i ++) sim_printf (" %02x", msg[i]);
sim_printf ("\r\n");
for (size_t i = 0; i < len; i ++) sim_printf (" %03o", msg[i]);
sim_printf ("\r\n");
#endif

    uint fnpno = fnpData.ibm3270ctlr[ctlr_no].fnpno;
    uint lineno = fnpData.ibm3270ctlr[ctlr_no].lineno;
    struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];
    if ((unsigned long) linep->nPos + len > sizeof (linep->buffer))
      sim_warn ("send_3270_msg overfull linep->buffer; dropping data\r\n");
    else
      {
        memcpy (linep->buffer + linep->nPos, msg, len);
        linep->nPos += len;
      }
#if 0
sim_printf ("send_3270_msg:");
for (size_t i = 0; i < linep->nPos; i ++) sim_printf (" %02x", linep->buffer[i]);
sim_printf ("\r\n");
for (size_t i = 0; i < linep->nPos; i ++) sim_printf (" %03o", linep->buffer[i]);
sim_printf ("\r\n");
#endif
    linep->force_accept_input = true;
    linep->accept_input = 1;
    linep->input_break = brk ? 1 : 0;
  }

const unsigned char addr_map [ADDR_MAP_ENTRIES] = 
  {
    0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f
  };

static void send_stn_in_buffer (void)
  {
#ifdef FNP2_DEBUG
sim_printf ("send_stn_in_buffer\r\n");
#endif

//dcl  1 text_msg unal based (textp),                         /* Format of normal text start */
//       2 stx char (1),
//       2 controller_address char (1),
//       2 device_address char (1),
//       2 aid char (1),                                      /* Reason for input (which key) */
//       2 cursor1 char (1),
//       2 cursor2 char (1);

// ibm3270_mpx expects: STX text_msg ETX
//
// x3270 sends aid, cursor1, cursor2

                     
//sim_printf ("sending rcvd data\r\n");

    uint fnpno = fnpData.ibm3270ctlr[ASSUME0].fnpno;
    uint lineno = fnpData.ibm3270ctlr[ASSUME0].lineno;
    struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];

    // Idle until buffer availible
    if (linep->accept_input)
      return;
    if (linep->input_reply_pending)
      return;

    struct ibm3270ctlr_s * ctlrp = & fnpData.ibm3270ctlr[ASSUME0];
    struct station_s * stnp = & fnpData.ibm3270ctlr[ASSUME0].stations[ctlrp->stn_no];

    uint left = linep->sync_msg_size;

    unsigned char * bufp = linep->buffer;

    * bufp ++  = 0x2; // STX
    left --;

    if (! stnp->hdr_sent)
      {
        * bufp ++  = addr_map [ASSUME0]; // Controller address
        left --;
        * bufp ++  = addr_map [ctlrp->stn_no]; // Station address
        left --;
        stnp->hdr_sent = true;
      }

    uint n_to_send = stnp->stn_in_size - stnp->stn_in_used;
    if (n_to_send > left)
      n_to_send = left;
    if (n_to_send)
      {
#ifdef FNP2_DEBUG
sim_printf ("handling in used %u %u\r\n", stnp->stn_in_used, n_to_send);
#endif
        //send_3270_msg (ASSUME0, stnp->stn_in_buffer + stnp->stn_in_used, n_to_send, false);
        //return;
        memcpy (bufp, stnp->stn_in_buffer + stnp->stn_in_used, n_to_send);
        bufp += n_to_send;
        stnp->stn_in_used += n_to_send;
        left -= n_to_send;
      }

    if (stnp->stn_in_used >= stnp->stn_in_size && left)
      {
#ifdef FNP2_DEBUG
sim_printf ("handling ETX\r\n");
#endif
        * bufp ++ = 0x3; // ETX
        left --;

        free (stnp->stn_in_buffer);
        stnp->stn_in_buffer = NULL;
        stnp->stn_in_size = 0;
        stnp->stn_in_used = 0;

        linep->input_break = 1;
        fnpData.ibm3270ctlr[ASSUME0].sending_stn_in_buffer = false;
        //unsigned char ETX = 0x3;
        //send_3270_msg (ASSUME0, & ETX, sizeof (ETX), true);
      }
    uint sz = (uint) (bufp - linep->buffer);
    if (sz)
      {
#ifdef FNP2_DEBUG
sim_printf ("I think data starts %02hhx\r\n", linep->buffer[0]);
#endif
        linep->force_accept_input = true;
        linep->accept_input = 1;
        linep->nPos = sz;
      }
    else
      {
        //ctlrp->sending_stn_in_buffer = true;
      }
  }

static void fnp_process_3270_event (void)
  {
    uint fnpno = fnpData.ibm3270ctlr[ASSUME0].fnpno;
    uint lineno = fnpData.ibm3270ctlr[ASSUME0].lineno;
    struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];

// Non-polling events

    if (fnpData.ibm3270ctlr[ASSUME0].sending_stn_in_buffer)
      {
        send_stn_in_buffer ();
        return;
      }

    if (fnpData.ibm3270ctlr[ASSUME0].write_complete)
      {
        fnpData.ibm3270ctlr[ASSUME0].write_complete = false;
        linep->lineStatus0 = 6llu << 18; // IBM3270_WRITE_COMPLETE
        linep->lineStatus1 = 0;
        linep->sendLineStatus = true;
      }

// Polling events

    if (! fnpData.du3270_poll)
     return;
    fnpData.du3270_poll --;
    if (fnpData.du3270_poll)
      return;
    struct ibm3270ctlr_s * ctlrp = & fnpData.ibm3270ctlr[ASSUME0];

#ifdef FNP2_DEBUG
    sim_printf ("3270 poll\n");
#endif
    //uint fnpno = fnpData.ibm3270ctlr[ASSUME0].fnpno;
    //uint lineno = fnpData.ibm3270ctlr[ASSUME0].lineno;
    //struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];
    //linep->lineStatus0 = 0;
    //linep->lineStatus1 = 0;
    if (ctlrp->pollDevChar == 127) // General poll
      {
        uint stn_cnt;
        for (stn_cnt = 0; stn_cnt < IBM3270_STATIONS_MAX; stn_cnt ++)
          {
            ctlrp->stn_no = (ctlrp->stn_no + 1) % IBM3270_STATIONS_MAX;
            struct station_s * stnp = & fnpData.ibm3270ctlr[ASSUME0].stations[ctlrp->stn_no];
            if (! stnp->client)
              continue;
            if (stnp->EORReceived)
              {
                stnp->EORReceived = false;
                ctlrp->sending_stn_in_buffer = true;
                fnpuv3270Poll (false);
                break;
              }
          }
        if (stn_cnt >= IBM3270_STATIONS_MAX)
          {
            // No response to poll; send EOT, stop polling
        
            unsigned char EOT = 0x37;
            send_3270_msg (ASSUME0, & EOT, 1, true);
            fnpuv3270Poll (false);
          }
      }
    else
      {
        // Specific poll
#ifdef FNP2_DEBUG
sim_printf("Specific poll\r\n");
#endif
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
    uint numunits = (uint) fnp_dev.numunits;
    for (uint fnp_unit_idx = 0; fnp_unit_idx < numunits; fnp_unit_idx ++)
      {
        if (! fnpData.fnpUnitData[fnp_unit_idx].fnpIsRunning)
          continue;
        int mbx = findMbx (fnp_unit_idx);
        if (mbx == -1)
          continue;
        for (int lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            struct t_line * linep = & fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno];

#ifdef DISC_DELAY
            // Disconnect pending?
            if (linep -> line_disconnected > 1)
              {
                // Buffer not empty?
                if (linep->inBuffer && linep->inUsed < linep->inSize)
                  {
                     // Reset timer
                     linep -> line_disconnected = DISC_DELAY;
                  }
                else
                  {
                    // Decrement timer
                    -- linep -> line_disconnected;
                  }
              }
#endif
            // Need to send a 'send_output' command to CS?

            if (linep -> send_output)
              {
                //linep -> send_output = false;
                linep->send_output --;
                if (linep->send_output == 0)
                  fnp_rcd_send_output ((uint)mbx, (int) fnp_unit_idx, lineno);
              }

            // Need to send a 'line_break' command to CS?

            else if (linep -> line_break)
              {
                fnp_rcd_line_break ((uint)mbx, (int) fnp_unit_idx, lineno);
                linep -> line_break = false;
              }

            // Need to send an 'acu_dial_failure' command to CS?

            else if (linep->acu_dial_failure)
              {
                fnp_rcd_acu_dial_failure ((uint)mbx, (int) fnp_unit_idx, lineno);
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
                fnp_rcd_accept_new_terminal ((uint)mbx, (int) fnp_unit_idx, lineno);
                linep->accept_new_terminal = false;
              }

            // Need to send an 'ack_echnego_init' command to CS?

            else if (linep -> ack_echnego_init)
              {
                fnp_rcd_ack_echnego_init ((uint)mbx, (int) fnp_unit_idx, lineno);
                linep -> ack_echnego_init = false;
                //linep -> send_output = true;
                linep -> send_output = SEND_OUTPUT_DELAY;
              }

            // Need to send an 'line_disconnected' command to CS?

#ifdef DISC_DELAY
            else if (linep -> line_disconnected == 1)
              {
                fnp_rcd_line_disconnected ((uint)mbx, (int) fnp_unit_idx, lineno);
                linep -> line_disconnected = 0;
                linep -> listen = false;
              }
#else
            else if (linep -> line_disconnected)
              {
                fnp_rcd_line_disconnected ((uint)mbx, (int) fnp_unit_idx, lineno);
                linep -> line_disconnected = false;
                linep -> listen = false;
              }
#endif

            // Need to send an 'wru_timeout' command to CS?

            else if (linep -> wru_timeout)
              {
                fnp_rcd_wru_timeout ((uint)mbx, (int) fnp_unit_idx, lineno);
                linep -> wru_timeout = false;
              }

            // Need to send an 'accept_input' or 'input_in_mailbox' command to CS?

            else if (linep->accept_input && ! linep->waitForMbxDone)
              {
                if (linep->accept_input == 1)
                  {
                    if (linep->nPos == 0) 
                      { 
                        sim_printf ("dropping nPos of 0");
                      }
                    else
                      {
                        //sim_printf ("\n nPos %d\n", linep->nPos);
#if 0
{
  sim_printf ("\n nPos %d:", linep->nPos);
  for (int i = 0; i < linep->nPos; i ++)
     if (isgraph (linep->buffer [i]))
      sim_printf ("%c", linep->buffer [i]);
     else
      sim_printf ("\\%03o", linep->buffer [i]);
  //for (unsigned char * p = linep->buffer; *p; p ++)
     //if (isgraph (*p))
      //sim_printf ("%c", *p);
     //else
      //sim_printf ("\\%03o", *p);
   sim_printf ("\r\n");
}
#endif
// There is a bufferful of data that needs to be sent to the CS.
// If the buffer has < 101 characters, use the 'input_in_mailbox'
// command; otherwise use the 'accept_input/input_accepted'
// sequence.

#if 0
                        fnp_rcd_accept_input (mbx, (int) fnp_unit_idx, lineno);
                        //linep->input_break = false;
                        linep->input_reply_pending = true;
                        // accept_input cleared below
#else
                        if (linep->force_accept_input || linep->nPos > 100)
                          {
                            fnp_rcd_accept_input ((uint)mbx, (int) fnp_unit_idx, lineno);
#ifdef FNPDBG
sim_printf ("accept_input\n");
#endif
                            //linep->input_break = false;
                            linep->input_reply_pending = true;
                            // accept_input cleared below
                          }
                        else
                          {
                            fnp_rcd_input_in_mailbox ((uint)mbx, (int) fnp_unit_idx, lineno);
#ifdef FNPDBG
sim_printf ("input_in_mailbox\n");
#endif
                            linep->nPos = 0;
                            // accept_input cleared below
                          }
#endif
                      }
                  }
                linep->accept_input --;
              }

            else if (linep->sendLineStatus)
              {
                linep->sendLineStatus = false;
                fnp_rcd_line_status ((uint)mbx, (int) fnp_unit_idx, lineno);
              }

            else
              {
                continue;
              }

            // One of the request processes may have consumed the
            // mailbox; make sure one is still available

            mbx = findMbx (fnp_unit_idx);
            if (mbx == -1)
              goto nombx;
          } // for lineno
nombx:;
      } // for fnp_unit_idx

#ifdef TUN
    fnpTUNProcessEvent ();
#endif
    fnp_process_3270_event ();
  }

static t_stat fnpShowNUnits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                              UNUSED int val, UNUSED const void * desc)
  {
    sim_printf("Number of FNP units in system is %d\n", fnp_dev . numunits);
    return SCPE_OK;
  }

static t_stat fnpSetNUnits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             const char * cptr, UNUSED void * desc)
  {
    if (! cptr)
      return SCPE_ARG;
    int n = atoi (cptr);
    if (n < 1 || n > N_FNP_UNITS_MAX)
      return SCPE_ARG;
    fnp_dev . numunits = (uint32) n;
    //return fnppSetNunits (uptr, value, cptr, desc);
    return SCPE_OK;
  }

static t_stat fnpShowIPCname (UNUSED FILE * st, UNIT * uptr,
                              UNUSED int val, UNUSED const void * desc)
  {   
    long n = FNP_UNIT_IDX (uptr);
    if (n < 0 || n >= N_FNP_UNITS_MAX)
      return SCPE_ARG;
    sim_printf("FNP IPC name is %s\n", fnpData.fnpUnitData [n] . ipcName);
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
        strncpy (fnpData.fnpUnitData [n] . ipcName, cptr, MAX_DEV_NAME_LEN - 1);
        fnpData.fnpUnitData [n] . ipcName [MAX_DEV_NAME_LEN - 1] = 0;
      }
    else
      fnpData.fnpUnitData [n] . ipcName [0] = 0;
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
        enum service_types st = fnpData.fnpUnitData[devnum].MState.line[linenum].service;
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
      fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_undefined;
    else if (strcasecmp (sn, "login") == 0)
      fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_login;
    else if (strcmp (sn, "ibm3270") == 0)
      {
        fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_3270;
        fnpData.ibm3270ctlr[ASSUME0].fnpno = (uint) devnum;
        fnpData.ibm3270ctlr[ASSUME0].lineno = linenum;
      }
    else if (strcasecmp (sn, "autocall") == 0)
      fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_autocall;
    else if (strncasecmp (sn, "slave=", 6) == 0)
      {
        uint pn;
        int nr2 = sscanf (sn, "slave=%u", & pn);
        if (nr2 != 1)
          return SCPE_ARG;
        if (pn >= 65535)
          return SCPE_ARG;
        fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_slave;
        fnpData.fnpUnitData[devnum].MState.line[linenum].port = (int) pn;
      }
    else
      return SCPE_ARG;
    return SCPE_OK;
  }

static t_stat fnpShowConfig (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                             UNUSED const void * desc)
  {
    long fnpUnitIdx = FNP_UNIT_IDX (uptr);
    if (fnpUnitIdx >= (long) N_FNP_UNITS_MAX)
      {
        sim_debug (DBG_ERR, & fnp_dev, 
                   "fnpShowConfig: Invalid unit number %ld\n", fnpUnitIdx);
        sim_printf ("error: invalid unit number %ld\n", fnpUnitIdx);
        return SCPE_ARG;
      }

    sim_printf ("FNP unit number %ld\n", fnpUnitIdx);
    struct fnpUnitData_s * fudp = fnpData.fnpUnitData + fnpUnitIdx;

    sim_printf ("FNP Mailbox Address:         %04o(8)\n", fudp -> mailboxAddress);
 
    return SCPE_OK;
  }


static t_stat fnpShowStatus (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                             UNUSED const void * desc)
  {
    long fnpUnitIdx = FNP_UNIT_IDX (uptr);
    if (fnpUnitIdx >= (long) fnp_dev.numunits)
      {
        sim_debug (DBG_ERR, & fnp_dev, 
                   "fnpShowStatus: Invalid unit number %ld\n", fnpUnitIdx);
        sim_printf ("error: invalid unit number %ld\n", fnpUnitIdx);
        return SCPE_ARG;
      }

    sim_printf ("FNP unit number %ld\n", fnpUnitIdx);
    struct fnpUnitData_s * fudp = fnpData.fnpUnitData + fnpUnitIdx;

    sim_printf ("mailboxAddress:              %04o\n", fudp->mailboxAddress);
    sim_printf ("fnpIsRunning:                %o\n", fudp->fnpIsRunning);
    sim_printf ("fnpMBXinUse:                 %o %o %o %o\n", fudp->fnpMBXinUse[0], fudp->fnpMBXinUse[1], fudp->fnpMBXinUse[2], fudp->fnpMBXinUse[3]);
    sim_printf ("lineWaiting:                 %o %o %o %o\n", fudp->lineWaiting[0], fudp->lineWaiting[1], fudp->lineWaiting[2], fudp->lineWaiting[3]);
    sim_printf ("fnpMBXlineno:                %o %o %o %o\n", fudp->fnpMBXlineno[0], fudp->fnpMBXlineno[1], fudp->fnpMBXlineno[2], fudp->fnpMBXlineno[3]);
    sim_printf ("accept_calls:                %o\n", fudp->MState.accept_calls);
    for (int l = 0; l < MAX_LINES; l ++)
      {
        sim_printf ("line: %d\n", l);
        sim_printf ("service:                     %d\n", fudp->MState.line[l].service);
        sim_printf ("line_client:                 %p\n", fudp->MState.line[l].line_client);
        sim_printf ("was_CR:                      %d\n", fudp->MState.line[l].was_CR);
        sim_printf ("listen:                      %d\n", fudp->MState.line[l].listen);
        sim_printf ("inputBufferSize:             %d\n", fudp->MState.line[l].inputBufferSize);
        sim_printf ("line_break:                  %d\n", fudp->MState.line[l].line_break);
        sim_printf ("send_output:                 %d\n", fudp->MState.line[l].send_output);
        sim_printf ("accept_new_terminal:         %d\n", fudp->MState.line[l].accept_new_terminal);
#if DISC_DELAY
        sim_printf ("line_disconnected:           %d\n", fudp->MState.line[l].line_disconnected);
#else
        sim_printf ("line_disconnected:           %c\n", fudp->MState.line[l].line_disconnected ? 'T' : 'F');
#endif
        sim_printf ("acu_dial_failure:            %d\n", fudp->MState.line[l].acu_dial_failure);
        sim_printf ("accept_input:                %d\n", fudp->MState.line[l].accept_input);
        sim_printf ("waitForMbxDone:              %d\n", fudp->MState.line[l].waitForMbxDone);
        sim_printf ("input_reply_pending:         %d\n", fudp->MState.line[l].input_reply_pending);
        sim_printf ("input_break:                 %d\n", fudp->MState.line[l].input_break);
        sim_printf ("nPos:                        %d\n", fudp->MState.line[l].nPos);
        sim_printf ("inBuffer:                    %p\n", fudp->MState.line[l].inBuffer);
        sim_printf ("inSize:                      %d\n", fudp->MState.line[l].inSize);
        sim_printf ("inUsed:                      %d\n", fudp->MState.line[l].inUsed);
        //sim_printf ("doConnect:                   %p\n", fudp->MState.line[l].doConnect);
        //sim_printf ("server:                      %p\n", fudp->MState.line[l].server);
        sim_printf ("port:                        %d\n", fudp->MState.line[l].port);

      }
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
    //if (fnpUnitIdx >= fnp_dev . numunits)
    if (fnpUnitIdx >= N_FNP_UNITS_MAX)
      {
        sim_debug (DBG_ERR, & fnp_dev, "fnpSetConfig: Invalid unit number %d\n", fnpUnitIdx);
        sim_printf ("error: fnpSetConfig: invalid unit number %d\n", fnpUnitIdx);
        return SCPE_ARG;
      }

    struct fnpUnitData_s * fudp = fnpData.fnpUnitData + fnpUnitIdx;

    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfg_parse ("fnpSetConfig", cptr, fnp_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfg_parse_done (& cfg_state);
              return SCPE_ARG; 

            case -1: // done
              break;

            case 0: // mailbox
              fudp -> mailboxAddress = (uint) v;
              break;

            default:
              sim_printf ("error: fnpSetConfig: invalid cfg_parse rc <%d>\n", rc);
              cfg_parse_done (& cfg_state);
              return SCPE_ARG; 
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfg_parse_done (& cfg_state);
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
            fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_login;                    
          }
        else if (havename && second && strcmp (first, "service") == 0)
          {
            if (strcmp (second, "login") == 0)
              fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_login;
            else if (strcmp (second, "autocall") == 0)
              fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_autocall;                   
            else if (strcmp (second, "slave") == 0)
              fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_slave;                   
            else if (strcmp (second, "offline") == 0)
              fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_undefined;                   
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
                fnpData.fnpUnitData[devnum].MState.line[linenum].port = (int) port;
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

t_stat set_fnp_server_port (UNUSED int32 arg, const char * buf)
  {
    if (! buf)
      return SCPE_ARG;
    int n = atoi (buf);
    if (n < 1 || n > 65535)
      return SCPE_ARG;
    fnpData.telnet_port = n;
    sim_printf ("FNP telnet server port set to %d\n", n);
    return SCPE_OK;
  }

t_stat set_fnp_server_address (UNUSED int32 arg, const char * buf)
  {
    if (fnpData.telnet_address)
      free (fnpData.telnet_address);
    fnpData.telnet_address = strdup (buf);
    sim_printf ("FNP telnet server address set to %s\n", fnpData.telnet_address);
    return SCPE_OK;
  }

t_stat set_fnp_3270_server_port (UNUSED int32 arg, const char * buf)
  {
    if (! buf)
      return SCPE_ARG;
    int n = atoi (buf);
    if (n < 1 || n > 65535)
      return SCPE_ARG;
    fnpData.telnet3270_port = n;
    sim_printf ("FNP telnet3270 server port set to %d\n", n);
    return SCPE_OK;
  }

t_stat fnp_start (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_printf ("FNP force start\n");
    fnpuvInit (fnpData.telnet_port, fnpData.telnet_address);
    fnpuv3270Init (fnpData.telnet3270_port);
    return SCPE_OK;
  }


#define PROMPT  "HSLA Port ("

void fnpConnectPrompt (uv_tcp_t * client)
  {
    fnpuv_start_writestr (client, (unsigned char *) PROMPT);
    bool first = true;
    uint numunits = (uint) fnp_dev.numunits;
    for (uint fnp_unit_idx = 0; fnp_unit_idx < numunits; fnp_unit_idx ++)
      {
        if (! fnpData.fnpUnitData[fnp_unit_idx].fnpIsRunning)
          continue;
        for (uint lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            struct t_line * linep = & fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno];
            if (! linep->listen)
              continue;
            if (linep->service == service_login && ! linep->line_client)
              {
                if (! first)
                  fnpuv_start_writestr (client, (unsigned char *) ",");
                char name [16];
                first = false;
                sprintf (name, "%c.h%03d", 'a' + fnp_unit_idx, lineno);
                fnpuv_start_writestr (client, (unsigned char *) name);
              }
          }
      }
    fnpuv_start_writestr (client, (unsigned char *) ")? ");
  }

// http://www8.cs.umu.se/~isak/Snippets/a2e.c

/*
**  ASCII <=> EBCDIC conversion functions
*/

const unsigned char a2e[256] = {
          0,  1,  2,  3, 55, 45, 46, 47, 22,  5, 37, 11, 12, 13, 14, 15,
         16, 17, 18, 19, 60, 61, 50, 38, 24, 25, 63, 39, 28, 29, 30, 31,
         64, 79,127,123, 91,108, 80,125, 77, 93, 92, 78,107, 96, 75, 97,
        240,241,242,243,244,245,246,247,248,249,122, 94, 76,126,110,111,
        124,193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,
        215,216,217,226,227,228,229,230,231,232,233, 74,224, 90, 95,109,
        121,129,130,131,132,133,134,135,136,137,145,146,147,148,149,150,
        151,152,153,162,163,164,165,166,167,168,169,192,106,208,161,  7,
         32, 33, 34, 35, 36, 21,  6, 23, 40, 41, 42, 43, 44,  9, 10, 27,
         48, 49, 26, 51, 52, 53, 54,  8, 56, 57, 58, 59,  4, 20, 62,225,
         65, 66, 67, 68, 69, 70, 71, 72, 73, 81, 82, 83, 84, 85, 86, 87,
         88, 89, 98, 99,100,101,102,103,104,105,112,113,114,115,116,117,
        118,119,120,128,138,139,140,141,142,143,144,154,155,156,157,158,
        159,160,170,171,172,173,174,175,176,177,178,179,180,181,182,183,
        184,185,186,187,188,189,190,191,202,203,204,205,206,207,218,219,
        220,221,222,223,234,235,236,237,238,239,250,251,252,253,254,255
};

const unsigned char e2a[256] = {
          0,  1,  2,  3,156,  9,134,127,151,141,142, 11, 12, 13, 14, 15,
         16, 17, 18, 19,157,133,  8,135, 24, 25,146,143, 28, 29, 30, 31,
        128,129,130,131,132, 10, 23, 27,136,137,138,139,140,  5,  6,  7,
        144,145, 22,147,148,149,150,  4,152,153,154,155, 20, 21,158, 26,
         32,160,161,162,163,164,165,166,167,168, 91, 46, 60, 40, 43, 33,
         38,169,170,171,172,173,174,175,176,177, 93, 36, 42, 41, 59, 94,
         45, 47,178,179,180,181,182,183,184,185,124, 44, 37, 95, 62, 63,
        186,187,188,189,190,191,192,193,194, 96, 58, 35, 64, 39, 61, 34,
        195, 97, 98, 99,100,101,102,103,104,105,196,197,198,199,200,201,
        202,106,107,108,109,110,111,112,113,114,203,204,205,206,207,208,
        209,126,115,116,117,118,119,120,121,122,210,211,212,213,214,215,
        216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,
        123, 65, 66, 67, 68, 69, 70, 71, 72, 73,232,233,234,235,236,237,
        125, 74, 75, 76, 77, 78, 79, 80, 81, 82,238,239,240,241,242,243,
         92,159, 83, 84, 85, 86, 87, 88, 89, 90,244,245,246,247,248,249,
         48, 49, 50, 51, 52, 53, 54, 55, 56, 57,250,251,252,253,254,255
};

#if 0
static char ASCIItoEBCDIC(const unsigned char c)
  {
    return a2e[c];
  }

static char EBCDICtoASCII(const unsigned char c)
  {
    return e2a[c];
  }
#endif

static void fnp3270Msg (uv_tcp_t * client, unsigned char * msg)
  {
//sim_printf ("%s", msg);
    size_t l = strlen ((char *) msg);
    unsigned char buf [l];
    for (uint i = 0; i < l; i ++)
      buf[i] = a2e[msg[i]];
// command  Erase write 245  (xf5)
// WCC      66 x42 0100 0010   Reset, KB restore
//  SBA     17 x11 
// 1st addr byte 64
// 2nd addr byte 64
// start field 29 x1D
// arg  96
//          29, 200, 133, 153, 131, 164, 147, 133 ??? 
    //unsigned char EW [] = {245, 66, 17, 64, 64 };
    unsigned char EW [] = {245, 0xc3, 17, 64, 64 };
    fnpuv_start_3270_write (client, EW, sizeof (EW));
    fnpuv_start_3270_write (client, buf, (ssize_t) l);
    fnpuv_send_eor (client);
  }

void fnp3270ConnectPrompt (uv_tcp_t * client)
  {
    if (! client || ! client->data)
      {
        sim_warn ("fnp3270ConnectPrompt bad client data\r\n");
        return;
      }
    uint fnpno = fnpData.ibm3270ctlr[ASSUME0].fnpno;
    uint lineno = fnpData.ibm3270ctlr[ASSUME0].lineno;
    //struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];
    uvClientData * p = client->data;
    p->assoc = true;
    p->fnpno = fnpno;
    p->lineno = lineno;
    //fnpData.fnpUnitData[fnpno].MState.line[lineno].line_client = client;

#if 1
    // Don't know ttype yet because Telnet negotiation won't
    // start until evPoll runs.
    unsigned char buf [256];
    sprintf ((char *) buf, "DPS8/M 3270 connection to %c.%03d.%d ttype %s\n", fnpno+'a',lineno, p->stationNo, p->ttype);
    fnpData.ibm3270ctlr[ASSUME0].selDevChar = addr_map[p->stationNo];
    fnp3270Msg (client, buf);
#endif
  }

void processLineInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread)
  {
    if (! client || ! client->data)
      {
        sim_warn ("processLineInput bad client data\r\n");
        return;
      }
    uvClientData * p = (uvClientData *) client->data;
    uint fnpno = p -> fnpno;
    uint lineno = p -> lineno;
    if (fnpno >= N_FNP_UNITS_MAX || lineno >= MAX_LINES)
      {
        sim_printf ("bogus client data\n");
        return;
      }
//sim_printf ("assoc. %d.%d nread %ld\n", fnpno, lineno, nread);
//{for (int i = 0; i < nread; i ++) sim_printf ("%c", isgraph (e2a[buf[i]]) ? e2a[buf[i]] : '.');
//sim_printf ("\n");
//for (int i = 0; i < nread; i ++) sim_printf (" %02x", buf[i]);
//sim_printf ("\r\n");
//}

    struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];

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

void process3270Input (uv_tcp_t * client, unsigned char * buf, ssize_t nread)
  {
    if (! client || ! client->data)
      {
        sim_warn ("process3270Input bad client data\r\n");
        return;
      }
    uvClientData * p = (uvClientData *) client->data;
    uint fnpno = p->fnpno;
    uint lineno = p->lineno;
    uint stn_no = p->stationNo;

    if (fnpno >= N_FNP_UNITS_MAX || lineno >= MAX_LINES)
      {
        sim_printf ("bogus client data\n");
        return;
      }
#ifdef FNP2_DEBUG
sim_printf ("process3270Input nread %ld\n", nread);
for (int i = 0; i < nread; i ++) sim_printf ("%c", isgraph (e2a[buf[i]]) ? e2a[buf[i]] : '.');
sim_printf ("\r\n");
for (int i = 0; i < nread; i ++) sim_printf (" %02x", buf[i]);
sim_printf ("\r\n");
#endif


    struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];
    if (! fnpData.fnpUnitData[fnpno].MState.accept_calls)
      {
        if (! linep->inBuffer)
          fnp3270Msg (client, (unsigned char *) "Multics is not accepting calls\r\n");
        return;
      }
    if (! linep->listen)
      {
        if (! linep->inBuffer)
          fnp3270Msg (client, (unsigned char *) "Multics is not listening to this line\r\n");
        return;
      }

// By design, inBuffer overun shouldn't happen, but it has been seen in IMFT.
// (When the TCP backs up, the buffers are merged so that larger and larger 
// reads occur. When the backedup buffer exceeds 65536, libev calls the read
// callback twice in a row, once with the first 65536, and the next with the
// remaining.
// Cope with it my realloc'ing the buffer and appending the new data. Ugh.

    struct station_s * stn_p = & fnpData.ibm3270ctlr[ASSUME0].stations[stn_no];
    if (stn_p->stn_in_buffer)
      {
        sim_warn ("stn_in_buffer overrun\n");
        unsigned char * new = realloc (stn_p->stn_in_buffer, (unsigned long) (stn_p->stn_in_size + nread));
        if (! new)
          {
            sim_warn ("stn_in_buffer realloc fail; dropping data\n");
            goto done;
          }
        memcpy (new + stn_p->stn_in_size, buf, (unsigned long) nread);
        stn_p->stn_in_size += nread;
        stn_p->stn_in_buffer = new;
      }
    else
      {
        stn_p->stn_in_buffer = malloc ((unsigned long) nread);
        if (! stn_p->stn_in_buffer)
          {
            sim_warn ("stn_in_buffer malloc fail;  dropping data\n");
            goto done;
          }
        memcpy (stn_p->stn_in_buffer, buf, (unsigned long) nread);
        stn_p->stn_in_size = (uint) nread;
        stn_p->stn_in_used = 0;
      }

#ifdef FNP2_DEBUG
sim_printf ("process3270Input stashed %lu bytes in stn %u; stn_in_size now %u\n", nread, stn_no, stn_p->stn_in_size);
#endif
done:;
    // Prevent further reading until this buffer is consumed
    // Rely on 3270 keyboard logic protocol to prevent buffer collision
    //fnpuv_read_stop (client);
  }

void reset_line (struct t_line * linep)
  {
    linep->was_CR = false;
    linep->inputBufferSize = 0;
    linep->ctrlStrIdx = 0;
    linep->breakAll = false;
    linep->handleQuit = false;
    linep->echoPlex = false;
    linep->crecho = false;
    linep->lfecho = false;
    linep->tabecho = false;
    linep->replay = false;
    linep->polite = false;
    linep->prefixnl = false;
    linep->eight_bit_out = false;
    linep->eight_bit_in = false;
    linep->odd_parity = false;
    linep->output_flow_control = false;
    linep->input_flow_control = false;
    linep->block_xfer_in_frame_sz = 0;
    linep->block_xfer_out_frame_sz = 0;
    memset (linep->delay_table, 0, sizeof (linep->delay_table));
    linep->inputSuspendLen = 0;
    memset (linep->inputSuspendStr, 0, sizeof (linep->inputSuspendStr));
    linep->inputResumeLen = 0;
    memset (linep->inputResumeStr, 0, sizeof (linep->inputResumeStr));
    linep->outputSuspendLen = 0;
    memset (linep->outputSuspendStr, 0, sizeof (linep->outputSuspendStr));
    linep->outputResumeLen = 0;
    memset (linep->outputResumeStr, 0, sizeof (linep->outputResumeStr));
    linep->frame_begin = 0;
    linep->frame_end = 0;
    memset (linep->echnego, 0, sizeof (linep->echnego));
    linep->line_break = false;
  }

void processUserInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread)
  {
    if (! client || ! client->data)
      {
        sim_warn ("processUserInput bad client data\r\n");
        return;
      }
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
                        fnpuv_start_writestr (client, (unsigned char *) "\b \b");    // remove char from line
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
                    fnpuv_start_writestr (client, (unsigned char *) "^R\r\n");       // echo ^R
                    fnpConnectPrompt (client);
                    fnpuv_start_writestr (client, (unsigned char *) p->buffer);
                  }
                 break;

                default:
                  break;
              } // switch kar
            continue; // process next character in buffer
          } // if buffer full

        if (isprint (kar))   // printable?
          {
            unsigned char str [2] = { kar, 0 };
            fnpuv_start_writestr (client, str);
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
                        fnpuv_start_writestr (client, (unsigned char *) "\b \b");    // remove char from line
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
                    fnpuv_start_writestr (client, (unsigned char *) "^R\r\n");       // echo ^R
                    fnpConnectPrompt (client);
                    fnpuv_start_writestr (client, (unsigned char *) p->buffer);
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
    fnpuv_start_writestr (client, (unsigned char *) "\r\n");


    uint fnp_unit_idx = 0;
    uint lineno = 0;

    if (strlen (cpy))
      {
        char fnpcode;
        int cnt = sscanf (cpy, "%c.h%u", & fnpcode, & lineno);
//sim_printf ("cnt %d fnpcode %c lineno %d\n", cnt, fnpcode, lineno);
        if (cnt != 2 || fnpcode < 'a' || fnpcode > 'h' || lineno >= MAX_LINES)
          {
            fnpuv_start_writestr (client, (unsigned char *) "can't parse\r\n");
            goto reprompt;
          }
        fnp_unit_idx = (uint) (fnpcode - 'a');
        if (fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].service != service_login ||
            fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].line_client)
          {
            fnpuv_start_writestr (client, (unsigned char *) "not availible\r\n");
            goto reprompt;
          }
        goto associate;
      }
    else
      {
        uint32 numunits = fnp_dev.numunits;
        for (fnp_unit_idx = 0; fnp_unit_idx < numunits; fnp_unit_idx ++)
          {
            if (! fnpData.fnpUnitData[fnp_unit_idx].fnpIsRunning)
              continue;
            for (lineno = 0; lineno < MAX_LINES; lineno ++)
              {
                if (fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].service == service_login &&
                    ! fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].line_client)
                  {
                    goto associate;
                  }
              }
          }
        fnpuv_start_writestr (client, (unsigned char *) "not available\r\n");
        goto reprompt;
      }
reprompt:;
    fnpConnectPrompt (client);
    return;

associate:;

    fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].line_client = client;
//sim_printf ("associated %c.%03d %p\n", fnp_unit_idx + 'a', lineno, client);
    p->assoc = true;
    p->fnpno = fnp_unit_idx;
    p->lineno = lineno;
    p->read_cb = fnpuv_associated_readcb;
    p->write_cb = fnpuv_start_write;
    p->write_actual_cb = fnpuv_start_write_actual;
    // Only enable read when Multics can accept it.
    //uv_read_stop ((uv_stream_t *) client);

    char buf2 [1024];

    struct sockaddr name;
    int namelen = sizeof (name);
    int ret = uv_tcp_getpeername (client, & name, & namelen);
    if (ret < 0)
      {
        sim_printf ("CONNECT (addr err %d) to %c.h%03d\n", ret, fnp_unit_idx +'a', lineno);
      }
    else
      {
        struct sockaddr_in * p = (struct sockaddr_in *) & name;
        sim_printf ("CONNECT %s to %c.h%03d\n", inet_ntoa (p -> sin_addr), fnp_unit_idx +'a', lineno);
      }

    sprintf (buf2, "Attached to line %c.h%03d\r\n", fnp_unit_idx +'a', lineno);
    fnpuv_start_writestr (client, (unsigned char *) buf2);

    if (! fnpData.fnpUnitData[fnp_unit_idx].MState.accept_calls)
      fnpuv_start_writestr (client, (unsigned char *) "Multics is not accepting calls\r\n");
    else if (! fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].listen)
      fnpuv_start_writestr (client, (unsigned char *) "Multics is not listening to this line\r\n");

    fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].lineType = 1 /* LINE_ASCII */;
    fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno].accept_new_terminal = true;
    reset_line (& fnpData.fnpUnitData[fnp_unit_idx].MState.line[lineno]);
    ltnRaw (p->telnetp);
  }

void startFNPListener (void)
  {
    fnpuvInit (fnpData.telnet_port, fnpData.telnet_address);
  }

