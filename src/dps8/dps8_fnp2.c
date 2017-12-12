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

#define ASSUME0 0

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_fnp2.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "fnptelnet.h"
#include "fnpuv.h"
#include "utlist.h"
#include "uthash.h"

#include "sim_defs.h"
#include "sim_tmxr.h"
#include <regex.h>

static t_stat fnpShowConfig (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetConfig (UNIT * uptr, int value, const char * cptr, void * desc);
static t_stat fnpShowNUnits (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetNUnits (UNIT * uptr, int32 value, const char * cptr, void * desc);
static t_stat fnpShowIPCname (FILE *st, UNIT *uptr, int val, const void *desc);
static t_stat fnpSetIPCname (UNIT * uptr, int32 value, const char * cptr, void * desc);

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

static inline void fnp_core_read (word24 addr, word36 *data, UNUSED const char * ctx)
  {
    * data = M [addr] & DMASK;
  }

static inline void fnp_core_write (word24 addr, word36 data, UNUSED const char * ctx)
  {
    M [addr] = data & DMASK;
  }



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


//
// Convert virtual address to physical
//

static uint virtToPhys (uint ptPtr, uint l66Address)
  {
    uint pageTable = ptPtr * 64u;
    uint l66AddressPage = l66Address / 1024u;

    word36 ptw;
    fnp_core_read (pageTable + l66AddressPage, & ptw, "fnpIOMCmd get ptw");
    uint page = getbits36_14 (ptw, 4);
    uint addr = page * 1024u + l66Address % 1024u;
    return addr;
  }


//
// Which IOM is FNPn connected to?
//

int lookupFnpsIomUnitNumber (int fnpUnitIdx)
  {
    return cables -> cablesFromIomToFnp [fnpUnitIdx] . iomUnitIdx;
  }

//
// Once-only initialization
//

void fnpInit(void)
  {
    // 0 sets set service to service_undefined
    memset(& fnpData, 0, sizeof(fnpData));
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      {
        cables -> cablesFromIomToFnp [i] . iomUnitIdx = -1;
      }
    fnpData.telnet_port = 6180;
    fnpData.telnet3270_port = 3270;
    fnpTelnetInit ();
    fnp3270Init ();
  }

static t_stat fnpReset (DEVICE * dptr)
  {
    for (int i = 0; i < (int) dptr -> numunits; i ++)
      {
        sim_cancel (& fnp_unit [i]);
      }
    return SCPE_OK;
  }

//
// Debugging...
//

static void dmpmbx (uint mailboxAddress)
  {
    struct mailbox * mbxp = (struct mailbox *) & M [mailboxAddress];
    sim_printf ("dia_pcw            %012"PRIo64"\n", mbxp -> dia_pcw);
    sim_printf ("mailbox_requests   %012"PRIo64"\n", mbxp -> mailbox_requests);
    sim_printf ("term_inpt_mpx_wd   %012"PRIo64"\n", mbxp -> term_inpt_mpx_wd);
    sim_printf ("last_mbx_req_count %012"PRIo64"\n", mbxp -> last_mbx_req_count);
    sim_printf ("num_in_use         %012"PRIo64"\n", mbxp -> num_in_use);
    sim_printf ("mbx_used_flags     %012"PRIo64"\n", mbxp -> mbx_used_flags);
    for (uint i = 0; i < 8; i ++)
      {
        sim_printf ("CS  mbx %d\n", i);
        struct dn355_submailbox * smbxp = & (mbxp -> dn355_sub_mbxes [i]);
        sim_printf ("    word1        %012"PRIo64"\n", smbxp -> word1);
        sim_printf ("    word2        %012"PRIo64"\n", smbxp -> word2);
        sim_printf ("    command_data %012"PRIo64"\n", smbxp -> command_data [0]);
        sim_printf ("                 %012"PRIo64"\n", smbxp -> command_data [1]);
        sim_printf ("                 %012"PRIo64"\n", smbxp -> command_data [2]);
        sim_printf ("    word6        %012"PRIo64"\n", smbxp -> word6);
      }
    for (uint i = 0; i < 4; i ++)
      {
        sim_printf ("FNP mbx %d\n", i);
        struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [i]);
        sim_printf ("    word1        %012"PRIo64"\n", smbxp -> word1);
        sim_printf ("    word2        %012"PRIo64"\n", smbxp -> word2);
        sim_printf ("    mystery      %012"PRIo64"\n", smbxp -> mystery [0]);
        sim_printf ("                 %012"PRIo64"\n", smbxp -> mystery [1]);
        sim_printf ("                 %012"PRIo64"\n", smbxp -> mystery [2]);
      }
        
  }

//
// Locate an available fnp_submailbox
//

static int findMbx (uint fnpUnitIdx)
  {
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpUnitIdx];
    for (uint i = 0; i < 4; i ++)
      if (! fudp -> fnpMBXinUse [i])
        return (int) i;
    return -1;
  }

//
// As mailbox messages are processed, decoded data are stashed here
///

static struct
  {
    uint devUnitIdx;
    uint op_code;
    uint slot_no;
    struct dn355_submailbox * smbxp;
    struct fnp_submailbox * fsmbxp;
    struct fnpUnitData * fudp;
    iomChanData_t * p;
    struct mailbox * mbxp;
    uint cell;
  } decoded;


//
// wcd; Multics has sent a Write Control Data command to the FNP
//

static int wcd (void)
  {
sim_printf ("wcd %d. %o\n", decoded.op_code, decoded.op_code);
    struct t_line * linep = & decoded.fudp->MState.line[decoded.slot_no];
    switch (decoded.op_code)
      {
        case  1: // disconnect_this_line
          {
            if (linep->client && linep->service == service_login)
              fnpuv_start_writestr (linep->client, (unsigned char *) "Multics has disconnected you\r\n");
            linep -> line_disconnected = true;
            linep -> listen = false;
            if (linep->client)
              {
                close_connection ((uv_stream_t *) linep->client);
              }
            
          }
          break;


        case  3: // dont_accept_calls
          {
            decoded.fudp->MState.accept_calls = false;
          }
          break;

        case  4: // accept_calls
          {
            decoded.fudp->MState.accept_calls = true;
          }
          break;

        case  8: // set_framing_chars
          {
            //sim_printf ("fnp set framing characters\n");
            word36 command_data0 = decoded.smbxp -> command_data [0];
            uint d1 = getbits36_9 (command_data0, 0);
            uint d2 = getbits36_9 (command_data0, 9);
            linep->frame_begin = d1;
            linep->frame_end = d2;
          }
          break;

        case 12: // dial out
          {
            word36 command_data0 = decoded.smbxp -> command_data [0];
            word36 command_data1 = decoded.smbxp -> command_data [1];
            word36 command_data2 = decoded.smbxp -> command_data [2];
            //sim_printf ("XXX dial_out %d %012"PRIo64" %012"PRIo64" %012"PRIo64"", decoded.slot_no, command_data0, command_data1, command_data2);
            fnpuv_dial_out (decoded.devUnitIdx, decoded.slot_no, command_data0, command_data1, command_data2);
          }
          break;

        case 22: // line_control
          {
            word36 command_data0 = decoded.smbxp -> command_data [0];
            word36 command_data1 = decoded.smbxp -> command_data [1];
            //word36 command_data2 = decoded.smbxp -> command_data [2];
            //sim_printf ("XXX line_control %d %012"PRIo64" %012"PRIo64" %012"PRIo64"\n", decoded.slot_no, command_data0, command_data1, command_data2);

// bisync_line_data.inc.pl1
            word18 op = getbits36_18 (command_data0, 0);
            word18 val1 = getbits36_18 (command_data0, 18);
            word18 val2 = getbits36_18 (command_data1, 0);
            //word18 val3 = getbits36_18 (command_data1, 18);
            switch (op)
              {
                case 1:
                  sim_printf ("SET_BID_LIMIT\n");
                  sim_printf ("    %u\n", val1);
                  break;
                case 2:
                  sim_printf ("ACCEPT_BID\n");
                  break;
                case 3:
                  sim_printf ("CONFIGURE\n");
                  if (val1 == 0)
                    sim_printf ("    non-transparent ASCII\n");
                  else if (val1 == 1)
                    sim_printf ("    non-transparent ASCII\n");
                  else 
                    sim_printf ("    unknown %u. %o\n", val1, val1);
                  break;
                case 4:
                  sim_printf ("SET_TTD_PARAMS\n");
                  sim_printf ("    ttd_time  %u\n", val1);
                  sim_printf ("    ttd_limit %u\n", val2);
                  break;
                case 5:
                  sim_printf ("REPORT_WRITE_STATUS\n");
                  break;
                case 6:
                  sim_printf ("SET_3270_MODE\n");
                  break;
                case 7:
                  {
                    sim_printf ("SET_POLLING_ADDR\n");
//word36 command_data2 = decoded.smbxp -> command_data [2];
//sim_printf ("XXX line_control %d %012"PRIo64" %012"PRIo64" %012"PRIo64"\n", decoded.slot_no, command_data0, command_data1, command_data2);
                    //word9 len = getbits36_9 (command_data0, 18);
                    word9 c1 = getbits36_9 (command_data0, 27);
                    //word9 c2 = getbits36_9 (command_data1, 0);
                    word9 c3 = getbits36_9 (command_data1, 9);
                    //word9 c4 = getbits36_9 (command_data1, 18);
                    //sim_printf ("    data_len %u\n", len);
                    sim_printf ("    char1 %u\n", c1);
                    //sim_printf ("    char2 %u\n", c2);
                    sim_printf ("    char3 %u\n", c3);
                    //sim_printf ("    char4 %u\n", c4);
                    fnpData.ibm3270ctlr[ASSUME0].pollCtlrChar = (unsigned char) (c1 & 0xff);
                    fnpData.ibm3270ctlr[ASSUME0].pollDevChar = (unsigned char) (c3 & 0xff);
                  }
                  break;
                case 8:
                  //sim_printf ("START_POLL\n");
                  fnpuv3270Poll (true);
                  break;
                case 9:
                  {
                    sim_printf ("SET_SELECT_ADDR\n");
                    //word9 len = getbits36_9 (command_data0, 18);
                    word9 c1 = getbits36_9 (command_data0, 27);
                    //word9 c2 = getbits36_9 (command_data1, 0);
                    word9 c3 = getbits36_9 (command_data1, 9);
                    //word9 c4 = getbits36_9 (command_data1, 18);
                    //sim_printf ("    data_len %u\n", len);
                    sim_printf ("    char1 %u\n", c1);
                    //sim_printf ("    char2 %u\n", c2);
                    sim_printf ("    char3 %u\n", c3);
                    //sim_printf ("    char4 %u\n", c4);
                    fnpData.ibm3270ctlr[ASSUME0].selCtlrChar = (unsigned char) (c1 & 0xff);
                    fnpData.ibm3270ctlr[ASSUME0].selDevChar = (unsigned char) (c3 & 0xff);
                  }
                  break;
                case 10:
                  sim_printf ("STOP_AUTO_POLL\n");
                  break;
                case 11:
                  sim_printf ("SET_MASTER_SLAVE_MODE\n");
                  if (val1 == 0)
                    sim_printf ("    slave\n");
                  else if (val1 == 1)
                    sim_printf ("    master\n");
                  else 
                    sim_printf ("    unknown %u. %o\n", val1, val1);
                  break;
                case 12:
                  sim_printf ("SET_HASP_MODE\n");
                  break;
                case 13:
                  sim_printf ("SET_NAK_LIMIT\n");
                  sim_printf ("    %u\n", val1);
                  break;
                case 14:
                  sim_printf ("SET_HASP_TIMERS\n");
                  break;
                default:
                  sim_printf ("unknown %u. %o\n", op, op);
                  break;
              }
  
#if 0
        sim_printf ("received line_control %d %012"PRIo64" %012"PRIo64" %012"PRIo64"\n", lineno, d1, d2, d3);
        sim_printf ("  dce_or_dte  %"PRIo64"\n", getbits36 (d1, 0, 1));
        sim_printf ("  lap_or_lapb %"PRIo64"\n", getbits36 (d1, 1, 1));
        sim_printf ("  disc_first  %"PRIo64"\n", getbits36 (d1, 2, 1));
        sim_printf ("  trace_off   %"PRIo64"\n", getbits36 (d1, 3, 1));
        sim_printf ("  activation_order %09"PRIo64"\n", getbits36 (d1, 9, 9));
        sim_printf ("  frame_size %"PRIo64" %"PRId64".\n", getbits36 (d1, 18, 18), getbits36 (d1, 18, 18));
        sim_printf ("  K  %"PRIo64" %"PRId64".\n", getbits36 (d2,  0, 9), getbits36 (d2,  0, 9));
        sim_printf ("  N2 %"PRIo64" %"PRId64".\n", getbits36 (d2,  9, 9), getbits36 (d2,  9, 9));
        sim_printf ("  T1 %"PRIo64" %"PRId64".\n", getbits36 (d2, 18, 9), getbits36 (d2, 18, 9));
        sim_printf ("  T3 %"PRIo64" %"PRId64".\n", getbits36 (d2, 27, 9), getbits36 (d2, 27, 9));
#endif

          }
          break;

        case 23: // sync_msg_size
          {
            word36 command_data0 = decoded.smbxp -> command_data [0];
            word18 sz = getbits36_18 (command_data0, 0);
            sim_printf ("sync_msg_size %u\n", sz);
          }
          break;
  
        case 24: // set_echnego_break_table
          {
            //sim_printf ("fnp set_echnego_break_table\n");
            word36 word6 = decoded.smbxp -> word6;
            uint data_addr = getbits36_18 (word6, 0);
            uint data_len = getbits36_18 (word6, 18);

            //sim_printf ("set_echnego_break_table %d addr %06o len %d\n", decoded.slot_no, data_addr, data_len);

#define echoTableLen 8
            if (data_len != echoTableLen && data_len != 0)
              {
                sim_printf ("set_echnego_break_table data_len !=8 (%d)\n", data_len);
                break;
              }

            word36 echoTable [echoTableLen];
            if (data_len == 0)
              {
                // Assuming that this means set everything to zeroes.
                memset (echoTable, 0, sizeof (echoTable));
              }
            else
              {
                // We are going to assume that the table doesn't cross a
                // page boundary, and only lookup the table start address.
                uint dataAddrPhys = virtToPhys (decoded.p -> PCW_PAGE_TABLE_PTR, data_addr);
                //sim_printf ("dataAddrPhys %06o\n", dataAddrPhys);
                for (uint i = 0; i < echoTableLen; i ++)
                  {
                    echoTable [i] = M [dataAddrPhys + i];
                    //sim_printf ("   %012"PRIo64"\n", echoTable [i]);
                  }
              }
            for (int i = 0; i < 256; i ++)
              {
                int wordno = i / 32;
                int bitno = i % 32;
                int bitoffset = bitno > 16 ? 35 - bitno : 33 - bitno; 
                linep->echnego[i] = !!(echoTable[wordno] & (1u << bitoffset));
              }
            linep->echnego_len = data_len;
          }
          break;

        case 25: // start_negotiated_echo
          {
            //word18 ctr = getbits36_18 (decoded.smbxp -> command_data [0], 0);
            //word18 screenleft = getbits36_18 (decoded.smbxp -> command_data [0], 18);

//sim_printf ("start_negotiated_echo ctr %d screenleft %d\n", ctr, screenleft);
          }
        case 26: // stop_negotiated_echo
          {
          }
          break;

        case 27: // init_echo_negotiation
          {
            //linep -> send_output = true;
            linep -> send_output = SEND_OUTPUT_DELAY;
            linep -> ack_echnego_init = true;
          }
          break;

        case 30: // input_fc_chars
          {
            word36 suspendStr = decoded.smbxp -> command_data [0];
            linep->inputSuspendStr[0] = getbits36_8 (suspendStr, 10);
            linep->inputSuspendStr[1] = getbits36_8 (suspendStr, 19);
            linep->inputSuspendStr[2] = getbits36_8 (suspendStr, 28);
            uint suspendLen = getbits36_9 (suspendStr, 0);
            if (suspendLen > 3)
              {
                //sim_printf ("input_fc_chars truncating suspend %d to 3\n", suspendLen);
                suspendLen = 3;
              }
            linep->inputSuspendLen = suspendLen;

            word36 resumeStr = decoded.smbxp -> command_data [0];
            linep->inputResumeStr[0] = getbits36_8 (resumeStr, 10);
            linep->inputResumeStr[1] = getbits36_8 (resumeStr, 19);
            linep->inputResumeStr[2] = getbits36_8 (resumeStr, 28);
            uint resumeLen = getbits36_9 (resumeStr, 0);
            if (resumeLen > 3)
              {
                //sim_printf ("input_fc_chars truncating suspend %d to 3\n", suspendLen);
                resumeLen = 3;
              }
            linep->inputResumeLen = resumeLen;
          }
          break;

        case 31: // output_fc_chars
          {
            //sim_printf ("fnp output_fc_chars\n");

            word36 suspendStr = decoded.smbxp -> command_data [0];
            linep->outputSuspendStr[0] = getbits36_8 (suspendStr, 10);
            linep->outputSuspendStr[1] = getbits36_8 (suspendStr, 19);
            linep->outputSuspendStr[2] = getbits36_8 (suspendStr, 28);
            uint suspendLen = getbits36_9 (suspendStr, 0);
            if (suspendLen > 3)
              {
                //sim_printf ("output_fc_chars truncating suspend %d to 3\n", suspendLen);
                suspendLen = 3;
              }
            linep->outputSuspendLen = suspendLen;

            word36 resumeStr = decoded.smbxp -> command_data [0];
            linep->outputResumeStr[0] = getbits36_8 (resumeStr, 10);
            linep->outputResumeStr[1] = getbits36_8 (resumeStr, 19);
            linep->outputResumeStr[2] = getbits36_8 (resumeStr, 28);
            uint resumeLen = getbits36_9 (resumeStr, 0);
            if (resumeLen > 3)
              {
                //sim_printf ("output_fc_chars truncating suspend %d to 3\n", suspendLen);
                resumeLen = 3;
              }
            linep->outputResumeLen = resumeLen;
          }
          break;

        case 34: // alter_parameters
          {
            //sim_printf ("fnp alter parameters\n");
            // The docs insist the subype is in word2, but I think
            // it is in command data...
            uint subtype = getbits36_9 (decoded.smbxp -> command_data [0], 0);
            uint flag = getbits36_1 (decoded.smbxp -> command_data [0], 17);
            //sim_printf ("  subtype %d\n", subtype);
            switch (subtype)
              {
                case  3: // Fullduplex
                  {
                    //sim_printf ("fnp full_duplex\n");
                    linep->fullDuplex = !! flag;
                  }
                  break;

                case  8: // Crecho
                  {
                    //sim_printf ("fnp crecho\n");
                    linep->crecho = !! flag;
                  }
                  break;

                case  9: // Lfecho
                  {
                    //sim_printf ("fnp lfecho\n");
                    linep->lfecho = !! flag;
                  }
                  break;

                case 13: // Dumpoutput
                  {
                    //sim_printf ("fnp dumpoutput\n");
                    // XXX ignored
                    //linep -> send_output = true;
                    linep -> send_output = SEND_OUTPUT_DELAY;
                  }
                  break;

                case 14: // Tabecho
                  {
                    //sim_printf ("fnp tabecho\n");
                    linep->tabecho = !! flag;
                  }
                  break;

                case 16: // Listen
                  {
                    //sim_printf ("fnp listen %p %d.%d %d\n", linep->client, decoded.devUnitIdx,decoded.slot_no, flag);
                    uint bufsz = getbits36_18 (decoded.smbxp->command_data[0], 18);
                    linep->listen = !! flag;
                    linep->inputBufferSize = bufsz;

                    if (linep->service == service_login && linep -> client)
                      {
                        fnpuv_start_writestr (linep->client,
                          linep->listen ?
                            (unsigned char *) "Multics is now listening to this line\r\n":
                            (unsigned char *) "Multics is no longer listening to this line\r\n");
                      }
                    if (linep->service == service_slave && ! linep -> client)
                      fnpuv_open_slave (decoded.devUnitIdx, decoded.slot_no);
                  }
                  break;

                case 17: // Hndlquit
                  {
                    //sim_printf ("fnp handle_quit %d\n", flag);
                    linep->handleQuit = !! flag;
                  }
                  break;

                case 18: // Chngstring
                  {
                    //sim_printf ("fnp Change control string\n");
                    uint idx =  getbits36_9 (decoded.smbxp -> command_data [0], 9);
                    linep->ctrlStrIdx = idx;
                  }
                  break;

                case 19: // Wru
                  {
                    linep -> wru_timeout = true;
                  }
                  break;

                case 20: // Echoplex
                  {
                    //sim_printf ("fnp echoplex\n");
                    linep->echoPlex = !! flag;
                  }
                  break;

                case 22: // Dumpinput
                  {
// XXX
// dump input should discard whatever input buffers it can

                    //sim_printf ("fnp dump input\n");
        // dump the input
       //int muxLineNo = MState[fnpUnitNum].line [lineno] . muxLineNum;
       //sim_printf ("dumping mux line %d\n");
       //ttys [muxLineNo] . nPos = 0; 
                  }
                  break;

                case 23: // Replay
                  {
                    //sim_printf ("fnp replay\n");
                    linep->replay = !! flag;
                  }
                  break;

                case 24: // Polite
                  {
                    //sim_printf ("fnp polite\n");
                    linep->polite = !! flag;
                  }
                  break;

                case 25: // Block_xfer
                  {
                    uint bufsiz1 = getbits36_18 (decoded.smbxp -> command_data [0], 18);
                    uint bufsiz2 = getbits36_18 (decoded.smbxp -> command_data [1], 0);
                    linep->block_xfer_out_frame_sz = bufsiz1;
                    linep->block_xfer_in_frame_sz = bufsiz2;
//sim_printf ("in frame sz %u out frame sz %u\n", linep->block_xfer_in_frame_sz, linep->block_xfer_out_frame_sz);
                    //sim_printf ("fnp block_xfer %d %d\n", bufsiz1, bufsiz2);
                  }
                  break;

                case 26: // Set_buffer_size
                  {
                    // Word 2: Bit 17 is "1"b.
                    //uint mb1 = getbits36_1  (decoded.smbxp -> command_data [0], 17);
                    // Bits 18...35 contain the size, in characters,
                    // of input buffers to be allocated for the 
                    // channel.
                    uint sz =  getbits36_18 (decoded.smbxp -> command_data [0], 18);
                    linep->inputBufferSize = sz;
//sim_printf ("Set_buffer_size %u\n", sz);
                  }

                case 27: // Breakall
                  {
                    //sim_printf ("fnp break_all\n");
                    linep->breakAll = !! flag;
                  }
                  break;

                case 28: // Prefixnl
                  {
                    //sim_printf ("fnp prefixnl\n");
                    linep->prefixnl = !! flag;
                  }
                  break;

                case 29: // Input_flow_control
                  {
                    //sim_printf ("fnp input_flow_control\n");
                    linep->input_flow_control = !! flag;
                  }
                  break;

                case 30: // Output_flow_control
                  {
                    //sim_printf ("fnp output_flow_control\n");
                    linep->output_flow_control = !! flag;
                  }
                  break;

                case 31: // Odd_parity
                  {
                    //sim_printf ("fnp odd_parity\n");
                    linep->odd_parity = !! flag;
                  }
                  break;

                case 32: // Eight_bit_in
                  {
                    //sim_printf ("fnp eight_bit_in\n");
                    linep->eight_bit_in = !! flag;
                  }
                  break;

                case 33: // Eight_bit_out
                  {
                    //sim_printf ("fnp eight_bit_out\n");
                    linep->eight_bit_out = !! flag;
                  }
                  break;

                case  1: // Breakchar
                case  2: // Nocontrol
                case  4: // Break
                case  5: // Errormsg
                case  6: // Meter
                case  7: // Sensepos
                case 10: // Lock
                case 11: // Msg
                case 12: // Upstate
                case 15: // Setbusy
                case 21: // Xmit_hold
                  {
                    sim_printf ("fnp unimplemented subtype %d (%o)\n", subtype, subtype);
                    // doFNPfault (...) // XXX
                    return -1;
                  }

                default:
                  {
                    sim_printf ("fnp illegal subtype %d (%o)\n", subtype, subtype);
                    // doFNPfault (...) // XXX
                    return -1;
                  }
              } // switch (subtype)
          }
          break; // alter_parameters

        case 37: // set_delay_table
          {
            //sim_printf ("fnp set delay table\n");
            word36 command_data0 = decoded.smbxp -> command_data [0];
            uint d1 = getbits36_18 (command_data0, 0);
            uint d2 = getbits36_18 (command_data0, 18);

            word36 command_data1 = decoded.smbxp -> command_data [1];
            uint d3 = getbits36_18 (command_data1, 0);
            uint d4 = getbits36_18 (command_data1, 18);

            word36 command_data2 = decoded.smbxp -> command_data [2];
            uint d5 = getbits36_18 (command_data2, 0);
            uint d6 = getbits36_18 (command_data2, 18);

            linep->delay_table[0] = d1;
            linep->delay_table[1] = d2;
            linep->delay_table[2] = d3;
            linep->delay_table[3] = d4;
            linep->delay_table[4] = d5;
            linep->delay_table[5] = d6;
          }
          break;

//  dcl  fnp_chan_meterp pointer;
//  dcl  FNP_CHANNEL_METERS_VERSION_1 fixed bin int static options (constant) init (1);
//  
//  dcl 1 fnp_chan_meter_struc based (fnp_chan_meterp) aligned,
//      2 version fixed bin,
//      2 flags,
//        3 synchronous bit (1) unaligned,
//        3 reserved bit (35) unaligned,
//      2 current_meters like fnp_channel_meters,
//      2 saved_meters like fnp_channel_meters;
//  


//  dcl 1 fnp_channel_meters based aligned,
struct fnp_channel_meters
  {
//      2 header,
struct header
  {
//        3 dia_request_q_len fixed bin (35),                             /* cumulative */
word36 dia_request_q_len;
//        3 dia_rql_updates fixed bin (35),                     /* updates to above */
word36 dia_rql_updates;
//        3 pending_status fixed bin (35),                      /* cumulative */
word36 pending_status;
//        3 pending_status_updates fixed bin (35),              /* updates to above */
word36 pending_status_updates;
//        3 output_overlaps fixed bin (18) unsigned unaligned,  /* output chained to already-existing chain */
//        3 parity_errors fixed bin (18) unsigned unaligned,    /* parity on the channel */
word36 output_overlaps___parity_errors;
//        3 software_status_overflows fixed bin (18) unsigned unaligned,
//        3 hardware_status_overflows fixed bin (18) unsigned unaligned,
word36 software_status_overflows___hardware_status_overflows;
//        3 input_alloc_failures fixed bin (18) unsigned unaligned,
//        3 dia_current_q_len fixed bin (18) unsigned unaligned,          /* current length of dia request queue */
word36 input_alloc_failures___dia_current_q_len;
//        3 exhaust fixed bin (35),
word36 exhaust;
//        3 software_xte fixed bin (18) unsigned unaligned,
//        3 pad bit (18) unaligned,
word36 software_xte___sync_or_async;
  } header;
//      2 sync_or_async (17) fixed bin;                         /* placeholder for meters for sync or async channels */
word36 sync_or_async;
  };

//  
//  dcl 1 fnp_sync_meters based aligned,
//      2 header like fnp_channel_meters.header,
//      2 input,
//        3 message_count fixed bin (35),                       /* total number of messages */
//        3 cum_length fixed bin (35),                          /* total cumulative length in characters */
//        3 min_length fixed bin (18) unsigned unaligned,       /* length of shortest message */
//        3 max_length fixed bin (18) unsigned unaligned,       /* length of longest message */
//      2 output like fnp_sync_meters.input,
//      2 counters (8) fixed bin (35),
//      2 pad (3) fixed bin;
//  
//  dcl 1 fnp_async_meters based aligned,
struct fnp_async_meters
  {
//      2 header like fnp_channel_meters.header,
//      2 pre_exhaust fixed bin (35),
word36 pre_exhaust;
//      2 echo_buf_overflow fixed bin (35),                     /* number of times echo buffer has overflowed */
word36 echo_buf_overflow;
//      2 bell_quits fixed bin (18) unsigned unaligned,
//      2 padb bit (18) unaligned,
word36 bell_quits___pad;
//      2 pad (14) fixed bin;
word36 pad;
  };
//  
        case 36: // report_meters
          {
            //sim_printf ("fnp report_meters\n");
// XXX Do nothing, the requset will timeout...
          }
          break;

        case  0: // terminal_accepted
        case  2: // disconnect_all_lines
        case  5: // input_accepted
        case  6: // set_line_type
        case  7: // enter_receive
        case  9: // blast
        case 10: // accept_direct_output
        case 11: // accept_last_output
        //case 13: // ???
        case 14: // reject_request_temp
        //case 15: // ???
        case 16: // terminal_rejected
        case 17: // disconnect_accepted
        case 18: // init_complete
        case 19: // dump_mem
        case 20: // patch_mem
        case 21: // fnp_break
        //case 24: // set_echnego_break_table
        //case 25: // start_negotiated_echo
        //case 26: // stop_negotiated_echo
        //case 27: // init_echo_negotiation
        //case 28: // ???
        case 29: // break_acknowledged
        //case 32: // ???
        //case 33: // ???
        case 35: // checksum_error
          {
            sim_warn ("fnp unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            //sim_debug (DBG_ERR, & fnpDev, "fnp unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            //sim_printf ("fnp unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            // doFNPfault (...) // XXX
            //return -1;
          }
        break;

        default:
          {
            sim_debug (DBG_ERR, & fnpDev, "fnp illegal opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            sim_warn ("fnp illegal opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            // doFNPfault (...) // XXX
            return -1;
          }
      } // switch decoded.op_code

    // Set the TIMW

    putbits36_1 (& decoded.mbxp -> term_inpt_mpx_wd, decoded.cell, 1);
#ifdef FNPDBG
sim_printf ("wcd sets the TIMW??\n");
#endif
    return 0;
  }

static void notifyCS (int mbx, int fnpno, int lineno)
  {
//sim_printf ("notifyCS mbx %d\n", mbx);
#ifdef FNPDBG
sim_printf ("notifyCS mbx %d\n", mbx);
#endif
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_3 (& smbxp -> word1, 0, (word3) fnpno); // dn355_no XXX
    putbits36_1 (& smbxp -> word1, 8, 1); // is_hsla XXX
    putbits36_3 (& smbxp -> word1, 9, 0); // la_no XXX
    putbits36_6 (& smbxp -> word1, 12, (word6) lineno); // slot_no XXX
    putbits36_18 (& smbxp -> word1, 18, 256); // blocks available XXX

    fudp->fnpMBXinUse [mbx] = true;
    // Set the TIMW
    putbits36_1 (& mbxp -> term_inpt_mpx_wd, (uint) mbx + 8, 1);
    send_terminate_interrupt ((uint) cables -> cablesFromIomToFnp [fnpno] . iomUnitIdx, (uint) cables -> cablesFromIomToFnp [fnpno] . chan_num);
  }

static void fnp_rcd_ack_echnego_init (int mbx, int fnpno, int lineno)
  {
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len
    putbits36_9 (& smbxp -> word2, 18, 70); // op_code ack_echnego_init
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd


    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_line_disconnected (int mbx, int fnpno, int lineno)
  {
//sim_printf ("fnp_rcd_line_disconnected mbx %d fnp %d lineno %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len
    putbits36_9 (& smbxp -> word2, 18, 0101); // op_code cmd_data_len
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd


    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_input_in_mailbox (int mbx, int fnpno, int lineno)
  {
//sim_printf ("fnp_rcd_input_in_mailbox mbx %d fnp %d lineno %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);
//sim_printf ("fnp_rcd_input_in_mailbox nPos %d\n", linep->nPos);
    putbits36_9 (& smbxp -> word2, 9, (word9) linep->nPos); // n_chars
    putbits36_9 (& smbxp -> word2, 18, 0102); // op_code input_in_mailbox
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd


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
          putbits36_9 (& v, 0, linep->buffer [i]);
        if (i + 1 < linep->nPos)
          putbits36_9 (& v, 9, linep->buffer [i + 1]);
        if (i + 2 < linep->nPos)
          putbits36_9 (& v, 18, linep->buffer [i + 2]);
        if (i + 3 < linep->nPos)
          putbits36_9 (& v, 27, linep->buffer [i + 3]);
//sim_printf ("%012"PRIo64"\n", v);
        smbxp -> mystery [j ++] = v;
      }

// command_data is at mystery[25]?

    // temporary until the logic is in place XXX
    int outputChainPresent = 0;

    putbits36_1 (& smbxp -> mystery [25], 16, (word1) outputChainPresent);
    putbits36_1 (& smbxp -> mystery [25], 17, linep->input_break ? 1 : 0);


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

static void fnp_rcd_line_status  (int mbx, int fnpno, int lineno)
  {
sim_printf ("fnp_rcd_line_status  mbx %d fnp %d lineno %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_18 (& smbxp -> word2, 0, 2); // cmd_data_len
    putbits36_9 (& smbxp -> word2, 18, 0124); // op_code accept_input
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    smbxp -> mystery [0] = linep->lineStatus0;
    smbxp -> mystery [1] = linep->lineStatus1;

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_accept_input (int mbx, int fnpno, int lineno)
  {
//sim_printf ("fnp_rcd_accept_input mbx %d fnp %d lineno %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);
sim_printf ("accept_input mbx %d fnpno %d lineno %d nPos %d\n", mbx, fnpno, lineno, linep->nPos);

    putbits36_18 (& smbxp -> word2, 0, (word18) linep->nPos); // cmd_data_len XXX
    putbits36_9 (& smbxp -> word2, 18, 0112); // op_code accept_input
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    // Not in AN85...
    // It looks like we need to build the DCW list, and let CS fill in the
    // addresses. I have no idea what the limit on the tally is; i.e. when
    // does the data need to be split up into multiple buffers?
    smbxp -> mystery [0] = 1; // n_buffers?

    // DCW for buffer
    smbxp -> mystery [1] = 0;
    putbits36_12 (& smbxp -> mystery [1], 24, (word12) linep->nPos);

    // Command_data after n_buffers and 24 dcws
    // temporary until the logic is in place XXX
    int outputChainPresent = 0;

    putbits36_1 (& smbxp -> mystery [25], 16, (word1) outputChainPresent);
    putbits36_1 (& smbxp -> mystery [25], 17, linep->input_break ? 1 : 0);

    fudp -> fnpMBXlineno [mbx] = lineno;
    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_line_break (int mbx, int fnpno, int lineno)
  {
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_9 (& smbxp -> word2, 9, 0); // cmd_data_len XXX
    putbits36_9 (& smbxp -> word2, 18, 0113); // op_code line_break
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_send_output (int mbx, int fnpno, int lineno)
  {
sim_printf ("fnp_rcd_send_output mbx %d fnp %d lineno %d\n", mbx, fnpno, lineno);
#ifdef FNPDBG
sim_printf ("send_output\n");
#endif
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_9 (& smbxp -> word2, 9, 0); // cmd_data_len XXX
    putbits36_9 (& smbxp -> word2, 18, 0105); // op_code send_output
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_acu_dial_failure (int mbx, int fnpno, int lineno)
  {
    //sim_printf ("acu_dial_failure %d %d %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len XXX
    putbits36_9 (& smbxp -> word2, 18, 82); // op_code acu_dial_failure
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_accept_new_terminal (int mbx, int fnpno, int lineno)
  {
//sim_printf ("accept_new_terminal %d %d %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len XXX
    putbits36_9 (& smbxp -> word2, 18, 64); // op_code accept_new_terminal
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

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

    smbxp->mystery [0] = 0; 
    smbxp->mystery [1] = 0;
    putbits36_9 (smbxp->mystery + 0, 27, linep->lineType);

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_rcd_wru_timeout (int mbx, int fnpno, int lineno)
  {
    //sim_printf ("wru_timeout %d %d %d\n", mbx, fnpno, lineno);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [fnpno];
    //struct t_line * linep = & fudp->MState.line[lineno];
    struct mailbox * mbxp = (struct mailbox *) & M [fudp->mailboxAddress];
    struct fnp_submailbox * smbxp = & (mbxp -> fnp_sub_mbxes [mbx]);

    putbits36_9 (& smbxp -> word2, 9, 2); // cmd_data_len XXX
    putbits36_9 (& smbxp -> word2, 18, 0114); // op_code wru_timeout
    putbits36_9 (& smbxp -> word2, 27, 1); // io_cmd rcd

    notifyCS (mbx, fnpno, lineno);
  }

static void fnp_wtx_output (uint tally, uint dataAddr)
  {
sim_printf ("fnp_wtx_output tally %u\n", tally);
    struct t_line * linep = & decoded.fudp->MState.line[decoded.slot_no];
    uint wordOff = 0;
    word36 word = 0;
    uint lastWordOff = (uint) -1;
    unsigned char data [tally];
    uint ptPtr = decoded.p -> PCW_PAGE_TABLE_PTR;
 
    for (uint i = 0; i < tally; i ++)
       {
         uint byteOff = i % 4;
         uint byte = 0;

         wordOff = i / 4;

         if (wordOff != lastWordOff)
           {
             lastWordOff = wordOff;
             uint wordAddr = virtToPhys (ptPtr, dataAddr + wordOff);
             word = M [wordAddr];
           }
         byte = getbits36_9 (word, byteOff * 9);
         data [i] = byte & 0377;
       }

    if (tally > 0)
      {
        uvClientData * p = linep->client->data;
        (* p->write_cb) (linep->client, data, tally);
      }
  }

static int wtx (void)
  {
sim_printf ("wtx op_code %o (%d.) %c.h%03d\n", decoded.op_code, decoded.op_code, decoded.devUnitIdx+'a', decoded.slot_no);
    if (decoded.op_code != 012 && decoded.op_code != 014)
      {
        sim_debug (DBG_ERR, & fnpDev, "fnp wtx unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
         sim_printf ("fnp wtx unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
        // doFNPfault (...) // XXX
        return -1;
      }
// op_code is 012
    uint dcwAddr = getbits36_18 (decoded.smbxp -> word6, 0);
    uint dcwCnt = getbits36_9 (decoded.smbxp -> word6, 27);
    //uint sent = 0;

    // For each dcw
    for (uint i = 0; i < dcwCnt; i ++)
      {
        // The address of the dcw in the dcw list
        uint dcwAddrPhys = virtToPhys (decoded.p -> PCW_PAGE_TABLE_PTR, dcwAddr + i);

        // The dcw
        //word36 dcw = M [dcwAddrPhys + i];
        word36 dcw = M [dcwAddrPhys];
        //sim_printf ("  %012"PRIo64"\n", dcw);

        // Get the address and the tally from the dcw
        uint dataAddr = getbits36_18 (dcw, 0);
        uint tally = getbits36_9 (dcw, 27);
        //sim_printf ("%6d %012o\n", tally, dataAddr);
        if (! tally)
          continue;
        fnp_wtx_output (tally, dataAddr);
        //sent += tally;
      } // for each dcw

    // Set the TIMW

    putbits36_1 (& decoded.mbxp -> term_inpt_mpx_wd, decoded.cell, 1);

#if 0
    //decoded.fudp->MState.line[decoded.slot_no].send_output = true;
    // send is the number of characters sent; 9600 baud is 100 cps, and
    // the FNP is polled at about 100HZ, or about the rate it takes to send
    // a character.
    // 100 CPS is too slow; bump up to 1000 CPS
    sent /= 10;
    sent ++; // Make sure it isn't zero.
    decoded.fudp->MState.line[decoded.slot_no].send_output = sent;
#else
    decoded.fudp->MState.line[decoded.slot_no].send_output = SEND_OUTPUT_DELAY;
#endif
    return 0;
  }

static void fnp_rtx_input_accepted (void)
  {
// AN85-01 pg 232 A-6 
//
//  Input Accepted (005)
//
//    Purpose:
//      Resopone to an accept input operation bu providing the addreess
//      (in the circular buffer) to which input is sent.
//
//    Associated Data:
//      Word 5: Bits 0..17 contain the beginning absolute address of the 
//      portion of the circular buffer into which the input is to be placed.
//
//      Bits 18...35 contain the number of characters to be placed in the
//      specified location.
//    
//      Word 4: If non-zero, contains the addess and tally as described 
//      above for the remaining data. This word is only used if the input
//      request required a wraparound of the circular buffer.
//

    word9 n_buffers = getbits36_9 (decoded.fsmbxp->mystery[0], 27);
    word24 addr0 = getbits36_24 (decoded.fsmbxp->mystery[1], 0);
    word12 tally0 = getbits36_12 (decoded.fsmbxp->mystery[1], 24);
    word24 addr1 = getbits36_24 (decoded.fsmbxp->mystery[2], 0);
    word12 tally1 = getbits36_12 (decoded.fsmbxp->mystery[2], 24);
    if (n_buffers > 2)
      sim_warn ("n_buffers > 2?\n");
    

    struct t_line * linep = & decoded.fudp->MState.line[decoded.slot_no];
    unsigned char * data = linep -> buffer;

//sim_printf ("long  in; line %d tally %d\n", decoded.slot_no, linep->nPos);
    for (int i = 0; i < tally0 + 3; i += 4)
      {
        word36 v = 0;
        if (i < tally0)
          putbits36_9 (& v, 0, data [i]);
        if (i + 1 < tally0)
          putbits36_9 (& v, 9, data [i + 1]);
        if (i + 2 < tally0)
          putbits36_9 (& v, 18, data [i + 2]);
        if (i + 3 < tally0)
          putbits36_9 (& v, 27, data [i + 3]);
//sim_printf ("%012"PRIo64"\n", v);
        M [addr0 ++] = v;
      }

    for (int i = 0; i < tally1 + 3; i += 4)
      {
        word36 v = 0;
        if (i < tally1)
          putbits36_9 (& v, 0, data [tally0 + i]);
        if (i + 1 < tally1)
          putbits36_9 (& v, 9, data [tally0 + i + 1]);
        if (i + 2 < tally1)
          putbits36_9 (& v, 18, data [tally0 + i + 2]);
        if (i + 3 < tally1)
          putbits36_9 (& v, 27, data [tally0 + i + 3]);
//sim_printf ("%012"PRIo64"\n", v);
        M [addr1 ++] = v;
      }

// command_data is at mystery[25]?

    // temporary until the logic is in place XXX
    int outputChainPresent = 0;

    putbits36_1 (& decoded.fsmbxp->mystery[25], 16, (word1) outputChainPresent);
    putbits36_1 (& decoded.fsmbxp->mystery[25], 17, linep->input_break ? 1 : 0);
//sim_printf ("fnp_rtx_input_accepted input_break %d\n", linep->input_break ? 1 : 0);

    // Mark the line as ready to receive more data
    linep->input_reply_pending = false;
    linep->input_break = false;
    linep->nPos = 0;

    // Set the TIMW
    putbits36_1 (& decoded.mbxp -> term_inpt_mpx_wd, decoded.cell, 1);
    send_terminate_interrupt ((uint) cables -> cablesFromIomToFnp [decoded.devUnitIdx] . iomUnitIdx, (uint) cables -> cablesFromIomToFnp [decoded.devUnitIdx] . chan_num);
  }

static int interruptL66_CS_to_FNP (void)
  {
    decoded.smbxp = & (decoded.mbxp -> dn355_sub_mbxes [decoded.cell]);

    word36 word2 = decoded.smbxp -> word2;
    //uint cmd_data_len = getbits36_9 (word2, 9);
    decoded.op_code = getbits36_9 (word2, 18);
    uint io_cmd = getbits36_9 (word2, 27);

    word36 word1 = decoded.smbxp -> word1;
    decoded.slot_no = getbits36_6 (word1, 12);

sim_printf ("io_cmd %u\n", io_cmd);
#ifdef FNPDBG
sim_printf ("io_cmd %u\n", io_cmd);
#endif
    switch (io_cmd)
      {
#if 0
        case 2: // rtx (read transmission)
          {
            int ret = rtx ();
            if (ret)
              return ret;
          }
          break;
#endif
        case 3: // wcd (write control data)
          {
            int ret = wcd ();
            if (ret)
              return ret;
          }
          break;

        case 4: // wtx (write text)
          {
            int ret = wtx ();
            if (ret)
              return ret;
          }
          break;

        case 1: // rcd (read contol data)
          {
            sim_debug (DBG_ERR, & fnpDev, "fnp unimplemented io_cmd %d\n", io_cmd);
             sim_printf ("fnp unimplemented io_cmd %d\n", io_cmd);
            // doFNPfault (...) // XXX
            return -1;
          }
        default:
          {
            sim_debug (DBG_ERR, & fnpDev, "fnp illegal io_cmd %d\n", io_cmd);
            sim_printf ("fnp illegal io_cmd %d\n", io_cmd);
            // doFNPfault (...) // XXX
            return -1;
          }
      } // switch (io_cmd)
    return 0;
  }

static int interruptL66_FNP_to_CS (void)
  {
sim_printf ("interruptL66_FNP_to_CS\n");
    // The CS has updated the FNP sub mailbox; this acknowleges processing
    // of the FNP->CS command that was in the submailbox

    uint mbx = decoded.cell - 8;
    decoded.fsmbxp = & (decoded.mbxp -> fnp_sub_mbxes [mbx]);
#if 0
    sim_printf ("fnp smbox %d update\n", decoded.cell);
    sim_printf ("    word1 %012"PRIo64"\n", decoded.fsmbxp -> word1);
    sim_printf ("    word2 %012"PRIo64"\n", decoded.fsmbxp -> word2);
    sim_printf ("    word3 %012"PRIo64"\n", decoded.fsmbxp -> mystery[0]);
    sim_printf ("    word4 %012"PRIo64"\n", decoded.fsmbxp -> mystery[1]);
    sim_printf ("    word5 %012"PRIo64"\n", decoded.fsmbxp -> mystery[2]);
#endif
    word36 word2 = decoded.fsmbxp -> word2;
    //uint cmd_data_len = getbits36_9 (word2, 9);
    uint op_code = getbits36_9 (word2, 18);
    uint io_cmd = getbits36_9 (word2, 27);

    word36 word1 = decoded.fsmbxp -> word1;
    //uint dn355_no = getbits36_3 (word1, 0);
    //uint is_hsla = getbits36_1 (word1, 8);
    //uint la_no = getbits36_3 (word1, 9);
    decoded.slot_no = getbits36_6 (word1, 12);
    //uint terminal_id = getbits36_18 (word1, 18);

    switch (io_cmd)
      {
        case 2: // rtx (read transmission)
          {
            switch (op_code)
              {
                case  5: // input_accepted
                  {
                    fnp_rtx_input_accepted ();
                  }
                  break;
                default:
                  sim_warn ("rtx %d. %o ack ignored\n", op_code, op_code);
                  break;
              }
              break;
          }
        case 3: // wcd (write control data)
          {
            switch (op_code)
              {
                case  0: // terminal_accepted
                  {
sim_printf ("terminal_accepted\n");
                    // outputBufferThreshold Ignored
                    //word36 command_data0 = decoded.fsmbxp -> mystery [0];
                    //uint outputBufferThreshold = getbits36_18 (command_data0, 0);
                    //sim_printf ("  outputBufferThreshold %d\n", outputBufferThreshold);

                    // Prime the pump
                    //decoded.fudp->MState.line[decoded.slot_no].send_output = true;
                    decoded.fudp->MState.line[decoded.slot_no].send_output = SEND_OUTPUT_DELAY;
                  }
                  break;

                case  1: // disconnect_this_line
                  {
                    //sim_printf ("disconnect_this_line ack.\n");
                  }
                  break;

                case 14: // reject_request_temp
                  {
                    //sim_printf ("fnp reject_request_temp\n");
                    // Retry in one second;
                    decoded.fudp->MState.line[decoded.slot_no].accept_input = 100;
                  }
                  break;

                case  2: // disconnect_all_lines
                case  3: // dont_accept_calls
                case  4: // accept_calls
                case  5: // input_accepted
                case  6: // set_line_type
                case  7: // enter_receive
                case  8: // set_framing_chars
                case  9: // blast
                case 10: // accept_direct_output
                case 11: // accept_last_output
                case 12: // dial
                //case 13: // ???
                //case 15: // ???
                case 16: // terminal_rejected
                case 17: // disconnect_accepted
                case 18: // init_complete
                case 19: // dump_mem
                case 20: // patch_mem
                case 21: // fnp_break
                case 22: // line_control
                case 23: // sync_msg_size
                case 24: // set_echnego_break_table
                case 25: // start_negotiated_echo
                case 26: // stop_negotiated_echo
                case 27: // init_echo_negotiation
                //case 28: // ???
                case 29: // break_acknowledged
                case 30: // input_fc_chars
                case 31: // output_fc_chars
                //case 32: // ???
                //case 33: // ???
                case 34: // alter_parameters
                case 35: // checksum_error
                case 36: // report_meters
                case 37: // set_delay_table
                  {
                    sim_debug (DBG_ERR, & fnpDev, "fnp reply unimplemented opcode %d (%o)\n", op_code, op_code);
                    sim_printf ("fnp reply unimplemented opcode %d (%o)\n", op_code, op_code);
                    // doFNPfault (...) // XXX
                    return -1;
                  }

                default:
                  {
                    sim_debug (DBG_ERR, & fnpDev, "fnp reply illegal opcode %d (%o)\n", op_code, op_code);
                    sim_printf ("fnp reply illegal opcode %d (%o)\n", op_code, op_code);
                    // doFNPfault (...) // XXX
                    return -1;
                  }
              } // switch op_code

            // Set the TIMW

            // Not sure... XXX 
            //putbits36_1 (& mbxp -> term_inpt_mpx_wd, cell, 1);
            // No; the CS has told us it has updated the mbx, and
            // we need to read it; we have done so, so we are finished
            // with the mbx, and can mark it so.
            decoded.fudp->fnpMBXinUse [mbx] = false;

          } // case wcd
          break;

        default:
          {
            sim_debug (DBG_ERR, & fnpDev, "illegal/unimplemented io_cmd (%d) in fnp submbx\n", io_cmd);
            sim_printf ("illegal/unimplemented io_cmd (%d) in fnp submbx\n", io_cmd);
            // doFNPfault (...) // XXX
            return -1;
          }
      } // switch (io_cmd)
    return 0;
  }

static int interruptL66_CS_done (void)
  {
//sim_printf ("interruptL66_CS_done\n");
    uint mbx = decoded.cell - 12;
    if (! decoded.fudp -> fnpMBXinUse [mbx])
      {
        sim_debug (DBG_ERR, & fnpDev, "odd -- Multics marked an unused mbx as unused? cell %d (mbx %d)\n", decoded.cell, mbx);
        sim_debug (DBG_ERR, & fnpDev, "  %d %d %d %d\n", decoded.fudp -> fnpMBXinUse [0], decoded.fudp -> fnpMBXinUse [1], decoded.fudp -> fnpMBXinUse [2], decoded.fudp -> fnpMBXinUse [3]);
      }
    else
      {
#ifdef FNPDBG
sim_printf ("Multics marked cell %d (mbx %d) as unused; was %o\n", decoded.cell, mbx, decoded.fudp -> fnpMBXinUse [mbx]);
#endif
        decoded.fudp -> fnpMBXinUse [mbx] = false;
        if (decoded.fudp->lineWaiting[mbx])
          {
            struct t_line * linep = & fnpData.fnpUnitData[decoded.devUnitIdx].MState.line[decoded.fudp->fnpMBXlineno[mbx]];
#ifdef FNPDBG
sim_printf ("clearing wait; was %d\n", linep->waitForMbxDone);
#endif
            linep->waitForMbxDone = false;
          }
#ifdef FNPDBG
sim_printf ("  %d %d %d %d\n", decoded.fudp->fnpMBXinUse [0], decoded.fudp->fnpMBXinUse [1], decoded.fudp->fnpMBXinUse [2], decoded.fudp->fnpMBXinUse [3]);
#endif
      }
    return 0;
  }

static int interruptL66 (uint iomUnitIdx, uint chan)
  {
//sim_printf ("interruptL66_CS_done\n");
    decoded.p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
      devices [chan] [decoded.p -> IDCW_DEV_CODE];
    decoded.devUnitIdx = d -> devUnitIdx;
    decoded.fudp = & fnpData.fnpUnitData [decoded.devUnitIdx];
    decoded.mbxp = (struct mailbox *) & M [decoded.fudp -> mailboxAddress];
    word36 dia_pcw = decoded.mbxp -> dia_pcw;

// AN85, pg 13-5
// When the CS has control information or output data to send
// to the FNP, it fills in a submailbox as described in Section 4
// and sends an interrupt over the DIA. This interrupt is handled 
// by dail as described above; when the submailbox is read, the
// transaction control word is set to "submailbox read" so that when
// the I/O completes and dtrans runs, the mailbox decoder (decmbx)
// is called. the I/O command in the submail box is either WCD (for
// control information) or WTX (for output data). If it is WCD,
// decmbx dispatches according to a table of operation codes and
// setting a flag in the IB and calling itest, the "test-state"
// entry of the interpreter. n a few cases, the operation requires
// further DIA I/O, but usually all that remains to be does is to
// "free" the submailbox by turning on the corresponding bit in the
// mailbox terminate interrupt multiplex word (see Section 4) and
// set the transaction control word accordingly. When the I/O to
// update TIMW terminates, the transaction is complete.
//
// If the I/O command is WTX, the submailbox contains the
// address and length of a 'pseudo-DCW" list containing the
// addresses and tallies of data buffers in tty_buf. In this case,
// dia_man connects to a DCW list to read them into a reserved area
// in dia_man. ...


// interrupt level (in "cell"):
//
// mbxs 0-7 are CS -> FNP
// mbxs 8--11 are FNP -> CS
//
//   0-7 Multics has placed a message for the FNP in mbx 0-7.
//   8-11 Multics has updated mbx 8-11
//   12-15 Multics is done with mbx 8-11  (n - 4).

    decoded.cell = getbits36_6 (dia_pcw, 24);
#ifdef FNPDBG
sim_printf ("CS interrupt %u\n", decoded.cell);
#endif
    if (decoded.cell < 8)
      {
        interruptL66_CS_to_FNP ();
      }
    else if (decoded.cell >= 8 && decoded.cell <= 11)
      {
        interruptL66_FNP_to_CS ();
      }
    else if (decoded.cell >= 12 && decoded.cell <= 15)
      {
        interruptL66_CS_done ();
      }
    else
      {
        sim_debug (DBG_ERR, & fnpDev, "fnp illegal cell number %d\n", decoded.cell);
        sim_printf ("fnp illegal cell number %d\n", decoded.cell);
        // doFNPfault (...) // XXX
        return -1;
      }
    return 0;
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
                fnpuv_start_writestr (linep->client, (unsigned char *) "\r\n");
              }
    
            // echo and inserts a LF in the users input stream when a CR is typed
            else if (linep->lfecho && kar == '\r')
              {
                fnpuv_start_writestr (linep->client, (unsigned char *) "\r\n");
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
                  fnpuv_start_writestr (linep->client, (unsigned char *) " ");
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
                fnpuv_start_writestr (linep->client, str);
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
                    fnpuv_start_writestr (linep->client, (unsigned char *) "\b \b");    // remove char from line
                    linep->nPos -= 1;                 // back up buffer pointer
                    linep->buffer[linep->nPos] = 0;     // remove char from buffer
                  }
                else 
                 {
                    // remove char from line
                    fnpuv_start_writestr (linep->client, (unsigned char *) "\a");
                  }
                return false;
              }
    
            case 21:    // ^U kill
              {
                linep->nPos = 0;
                linep->buffer[linep->nPos] = 0;
                fnpuv_start_writestr (linep->client, (unsigned char *) "^U\r\n");
                return false;
              }
    
            case 0x12:  // ^R
              {
                fnpuv_start_writestr (linep->client, (unsigned char *) "^R\r\n");       // echo ^R
                fnpuv_start_writestr (linep->client, linep->buffer);
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
        // Dialout or slave and inBuffer exhausted
        ((linep->service == service_autocall || linep->service == service_slave) && linep->inUsed >= linep->inSize) ||

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

// The 3270 controller received a EOR

void fnpRecvEOR (uv_tcp_t * client)
  {
    uvClientData * p = client->data;
    //fnpData.fnpUnitData[p->fnpno].MState.line[p->lineno].accept_input = 1;
    fnpData.ibm3270ctlr[ASSUME0].stations[p->stationNo].EORReceived = true;
sim_printf ("EORReceived set for stn %u\r\n", p->stationNo);
  }

static void fnpProcessBuffer (struct t_line * linep)
  {
    // The connection could have closed when we were not looking
    if (! linep->client)
      {
sim_printf ("discarding\n");
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
         if (linep->service == service_3270)
           {
             linep->buffer[linep->nPos++] = c;
             linep->buffer[linep->nPos] = 0;
             continue;
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
            struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];

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
        linep->nPos += (int) len;
      }
    linep->accept_input = linep->input_break = brk ? 1 : 0;
  }

const unsigned char addr_map [ADDR_MAP_ENTRIES] = 
  {
    0x40, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f
  };

static void fnp_process_3270_event (void)
  {
    if (! fnpData.du3270_poll)
     return;
    fnpData.du3270_poll --;
    if (fnpData.du3270_poll)
      return;
    struct ibm3270ctlr_s * ctlrp = & fnpData.ibm3270ctlr[ASSUME0];

    sim_printf ("3270 poll\n");
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
                 fnpuv3270Poll (false);
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

                 unsigned char hdr [3];
                 hdr [0]  = 0x2; // STX
                 hdr [1]  = addr_map [ASSUME0]; // Controller address
                 hdr [2]  = addr_map [ctlrp->stn_no]; // Station address
                 send_3270_msg (ASSUME0, hdr, sizeof (hdr), false);

                 if (stnp->stn_in_size)
                   {
                     send_3270_msg (ASSUME0, stnp->stn_in_buffer, stnp->stn_in_size, false);
                     stnp->stn_in_size = 0;
                     free (stnp->stn_in_buffer);
                     stnp->stn_in_buffer = NULL;
                   }

                 unsigned char ETX = 0x3;
                 send_3270_msg (ASSUME0, & ETX, sizeof (ETX), true);
                 fnpuv3270Poll (false);
                 break;
               }
          }
        if (stn_cnt >= IBM3270_STATIONS_MAX)
          {
            // No response to poll; send EOT, stop polling
        
// dcl 1 line_stat aligned,
//     2 op fixed binary (17) unaligned,                       /* contains reason for status */
//     2 val (3) fixed binary (17) unaligned;

//sim_printf ("sending idle EOT\r\n");
            unsigned char EOT = 0x37;
            send_3270_msg (ASSUME0, & EOT, 1, true);
            fnpuv3270Poll (false);
          }
      }
    else
      {
        // Specific poll
sim_printf("Specific poll\r\n");
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
            struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];

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
sim_printf ("\r\n nPos %d:", linep->nPos);
for (int i = 0; i < linep->nPos; i ++) sim_printf (" %02x", linep->buffer[i]);
sim_printf ("\r\n");
for (int i = 0; i < linep->nPos; i ++) sim_printf (" %03x", linep->buffer[i]);
sim_printf ("\r\n");
#endif

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

            else if (linep->sendLineStatus)
              {
                linep->sendLineStatus = false;
                fnp_rcd_line_status (mbx, fnpno, lineno);
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
    fnp_process_3270_event ();
  }

static void fnpcmdBootload (uint devUnitIdx)
  {
    sim_printf("Received BOOTLOAD command...\n");
    fnpData.fnpUnitData[devUnitIdx].MState.accept_calls = false;
    for (uint lineno = 0; lineno < MAX_LINES; lineno ++)
      {
        fnpData.fnpUnitData[devUnitIdx].MState.line [lineno] . listen = false;
        if (fnpData.fnpUnitData[devUnitIdx].MState.line[lineno].client &&
            fnpData.fnpUnitData[devUnitIdx].MState.line[lineno].service == service_login)
          {
            fnpuv_start_writestr (fnpData.fnpUnitData[devUnitIdx].MState.line [lineno].client,
              (unsigned char *) "The FNP has been restarted\r\n");
          }
        if (fnpData.fnpUnitData[devUnitIdx].MState.line[lineno].service == service_3270)
          {
sim_printf ("3270 controller found at unit %u line %u\r\n", devUnitIdx, lineno);
// XXX assuming only single controller
            if (fnpData.ibm3270ctlr[ASSUME0].configured)
              {
                sim_warn ("Too many 3270 controllers configured");
              }
            else
              {
                fnpData.ibm3270ctlr[ASSUME0].configured = true;
                fnpData.ibm3270ctlr[ASSUME0].fnpno = devUnitIdx;
                fnpData.ibm3270ctlr[ASSUME0].lineno = lineno;
                
                // 3270 controller connects immediately
                fnpData.fnpUnitData[devUnitIdx].MState.line[lineno].lineType  = 7 /* LINE_BSC */;
                fnpData.fnpUnitData[devUnitIdx].MState.line[lineno].accept_new_terminal = true;
              }
          }
      }
    fnpuvInit (fnpData.telnet_port);
    fnpuv3270Init (fnpData.telnet3270_port);
  }

static void processMBX (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    struct device * d = & cables -> cablesFromIomToDev [iomUnitIdx] .
      devices [chan] [p -> IDCW_DEV_CODE];
    uint devUnitIdx = d -> devUnitIdx;
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [devUnitIdx];

// 60132445 FEP Coupler EPS
// 2.2.1 Control Intercommunication
//
// "In Level 66 momory, at a location known to the coupler and
// to Level 6 software is a mailbox area consisting to an Overhead
// mailbox and 7 Channel mailboxes."

    bool ok = true;
    struct mailbox * mbxp = (struct mailbox *) & M [fudp -> mailboxAddress];

    word36 dia_pcw;
    dia_pcw = mbxp -> dia_pcw;
//sim_printf ("mbx %08o:%012"PRIo64"\n", fudp -> mailboxAddress, dia_pcw);

// Mailbox word 0:
//
//   0-17 A
//     18 I
//  19-20 MBZ
//  21-22 RFU
//     23 0
//  24-26 B
//  27-29 D Channel #
//  30-35 C Command
//
//                          A6-A23    A0-A2     A3-A5
// Operation          C         A        B        D
// Interrupt L6      071       ---      Int.     Level
// Bootload L6       072    L66 Addr  L66 Addr  L66 Addr
//                           A6-A23    A0-A2     A3-A5
// Interrupt L66     073      ---      ---     Intr Cell
// Data Xfer to L66  075    L66 Addr  L66 Addr  L66 Addr
//                           A6-A23    A0-A2     A3-A5
// Data Xfer to L6   076    L66 Addr  L66 Addr  L66 Addr
//                           A6-A23    A0-A2     A3-A5

// 
// fnp_util.pl1:
//    075 tandd read
//    076 tandd write

// mbx word 1: mailbox_requests fixed bin
//          2: term_inpt_mpx_wd bit (36) aligned
//          3: last_mbx_req_count fixed bin
//          4: num_in_use fixed bin
//          5: mbx_used_flags
//                used (0:7) bit (1) unaligned
//                pad2 bit (28) unaligned
//          6,7: crash_data
//                fault_code fixed bin (18) unal unsigned
//                ic fixed bin (18) unal unsigned
//                iom_fault_status fixed bin (18) unal unsigned
//                fault_word fixed bin (18) unal unsigned
//
//    crash_data according to dn355_boot_interrupt.pl1:
//
//   dcl  1 fnp_boot_status aligned based (stat_ptr),            /* structure of bootload status */
//          2 real_status bit (1) unaligned,                     /* must be "1"b in valid status */
//          2 pad1 bit (2) unaligned,
//          2 major_status bit (3) unaligned,
//          2 pad2 bit (3) unaligned,
//          2 substatus fixed bin (8) unal,                      /* code set by 355, only interesting if major_status is 4 */
//          2 channel_no fixed bin (17) unaligned;               /* channel no. of LSLA in case of config error */
//    only 34 bits???
// major_status:
//  dcl  BOOTLOAD_OK fixed bin int static options (constant) init (0);
//  dcl  CHECKSUM_ERROR fixed bin int static options (constant) init (1);
//  dcl  READ_ERROR fixed bin int static options (constant) init (2);
//  dcl  GICB_ERROR fixed bin int static options (constant) init (3);
//  dcl  INIT_ERROR fixed bin int static options (constant) init (4);
//  dcl  UNWIRE_STATUS fixed bin int static options (constant) init (5);
//  dcl  MAX_STATUS fixed bin int static options (constant) init (5);
 

// 3.5.1 Commands Issued by Central System
//
// In the issuing of an order by the Central System to the Coupler, the 
// sequence occurs:
//
// 1. The L66 program creates a LPW and Pcw for the Central System Connect
// channel. It also generates and stores a control word containing a command
// int he L66 maillbox. A Connect is then issued to the L66 IOM.
//
// 2. The Connect Channel accesses the PCW to get the channel number of
// the Direct Channel that the coupler is attached to. the direct Channel
// sends a signelto the Coupler that a Connect has been issued.
//
// 3. The Coupler now reads the content of the L66 mailbox, obtaining the
// control word. If the control word is legel, the Coupler will write a
// word of all zeros into the mailbox.
//

// 4.1.1.2 Transfer Control Word.
// The transfer control word, which is pointed to by the 
// mailbox word in l66 memory on Op Codes 72, 7, 76 contains
// a starting address which applies to L6 memory an a Tally
// of the number of 36 bit words to be transfered. The l66
// memory locations to/from which the transfers occur are
// those immediately follwoing the location where this word
// was obtained.
//
//    00-02  001
//    03-17 L6 Address
//       18 P
//    19-23 MBZ
//    24-25 Tally
//
//     if P = 0 the l6 address:
//        00-07 00000000
//        08-22 L6 address (bits 3-17)
//           23 0
//     if P = 1
//        00-14 L6 address (bits 3-17)
//        15-23 0
//

    //uint chanNum = getbits36_6 (dia_pcw, 24);
    uint command = getbits36_6 (dia_pcw, 30);
    word36 bootloadStatus = 0;

    if (command == 000) // reset
      {
#ifdef FNPDBG
sim_printf ("reset??\n");
#endif
      }
    else if (command == 072) // bootload
      {
        fnpcmdBootload (devUnitIdx);
        fudp -> fnpIsRunning = true;
      }
    else if (command == 071) // interrupt L6
      {
        ok = interruptL66 (iomUnitIdx, chan) == 0;
      }
    else if (command == 075) // data xfer from L6 to L66
      {
        // Build the L66 address from the PCW
        //   0-17 A
        //  24-26 B
        //  27-29 D Channel #
        // Operation          C         A        B        D
        // Data Xfer to L66  075    L66 Addr  L66 Addr  L66 Addr
        //                           A6-A23    A0-A2     A3-A5
        // These don't seem to be right; M[L66Add] is always 0.
        //word24 A = (word24) getbits36_18 (dia_pcw,  0);
        //word24 B = (word24) getbits36_3  (dia_pcw, 24);
        //word24 D = (word24) getbits36_3  (dia_pcw, 29);
        //word24 L66Addr = (B << (24 - 3)) | (D << (24 - 3 - 3)) | A;


        // According to fnp_util:
        //  dcl  1 a_dia_pcw aligned based (mbxp),                      /* better declaration than the one used when MCS is running */
        //         2 address fixed bin (18) unsigned unaligned,
        //         2 error bit (1) unaligned,
        //         2 pad1 bit (3) unaligned,
        //         2 parity bit (1) unaligned,
        //         2 pad2 bit (1) unaligned,
        //         2 pad3 bit (3) unaligned,                            /* if we used address extension this would be important */
        //         2 interrupt_level fixed bin (3) unsigned unaligned,
        //         2 command bit (6) unaligned;
        //
        //   a_dia_pcw.address = address;
        //


        //word24 L66Addr = (word24) getbits36_18 (dia_pcw, 0);
        //sim_printf ("L66 xfer\n");
        //sim_printf ("PCW  %012"PRIo64"\n", dia_pcw);
        //sim_printf ("L66Addr %08o\n", L66Addr);
        //sim_printf ("M[] %012"PRIo64"\n", M[L66Addr]);

        // 'dump_mpx d'
        //L66 xfer
        //PCW  022002000075
        //L66Addr 00022002
        //M[] 000000401775
        //L66 xfer
        //PCW  022002000075
        //L66Addr 00022002
        //M[] 003772401775
        //L66 xfer
        //PCW  022002000075
        //L66Addr 00022002
        //M[] 007764401775
        //
        // The contents of M seem much more reasonable, bit still don't match
        // fnp_util$setup_dump_ctl_word. The left octet should be '1', not '0';
        // bit 18 should be 0 not 1. But the offsets and tallies match exactly.
        // Huh... Looking at 'dump_6670_control' control instead, it matches 
        // correctly. Apparently fnp_util thinks the FNP is a 6670, not a 335.
        // I can't decipher the call path, so I don't know why; but looking at
        // multiplexer_types.incl.pl1, I would guess that by MR12.x, all FNPs 
        // were 6670s.
        //
        // So:
        //
        //   dcl  1 dump_6670_control aligned based (data_ptr),          /* word used to supply DN6670 address and tally for fdump */
        //          2 fnp_address fixed bin (18) unsigned unaligned,
        //          2 unpaged bit (1) unaligned,
        //          2 mbz bit (5) unaligned,
        //          2 tally fixed bin (12) unsigned unaligned;

        // Since the data is marked 'paged', and I don't understand the
        // the paging mechanism or parameters, I'm going to punt here and
        // not actually transfer any data.
#ifdef FNPDBG
sim_printf ("data xfer??\n");
#endif

      }
    else
      {
        sim_warn ("bogus fnp command %d (%o)\n", command, command);
        ok = false;
      }

    if (ok)
      {
#ifdef FNPDBG
dmpmbx (fudp->mailboxAddress);
#endif
        fnp_core_write (fudp -> mailboxAddress, 0, "fnpIOMCmd clear dia_pcw");
        putbits36_1 (& bootloadStatus, 0, 1); // real_status = 1
        putbits36_3 (& bootloadStatus, 3, 0); // major_status = BOOTLOAD_OK;
        putbits36_8 (& bootloadStatus, 9, 0); // substatus = BOOTLOAD_OK;
        putbits36_17 (& bootloadStatus, 17, 0); // channel_no = 0;
        fnp_core_write (fudp -> mailboxAddress + 6, bootloadStatus, "fnpIOMCmd set bootload status");
      }
    else
      {
        dmpmbx (fudp->mailboxAddress);
// 3 error bit (1) unaligned, /* set to "1"b if error on connect */
        putbits36_1 (& dia_pcw, 18, 1); // set bit 18
        fnp_core_write (fudp -> mailboxAddress, dia_pcw, "fnpIOMCmd set error bit");
      }
  }

static int fnpCmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
    p -> stati = 0;
//sim_printf ("fnp cmd %d\n", p -> IDCW_DEV_CMD);
    switch (p -> IDCW_DEV_CMD)
      {
        case 000: // CMD 00 Request status
          {
//sim_printf ("fnp cmd request status\n");
#if 0
            if (findPeer ("fnp-d"))
              p -> stati = 04000;
            else
              p -> stati = 06000; // Have status; power off?
#else
              p -> stati = 04000;
#endif
            //disk_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & fnpDev, "Request status\n");
          }
          break;

        default:
          {
            p -> stati = 04501;
            sim_debug (DBG_ERR, & fnpDev,
                       "%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
#ifdef FNPDBG
sim_printf ("%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
#endif
            break;
          }
      }

    //status_service (iomUnitIdx, chan, false);
    processMBX (iomUnitIdx, chan);

//sim_printf ("end of list service; sending terminate interrupt\n");
    //send_terminate_interrupt (iomUnitIdx, chan);
    return 2; // did command, don't want more
  }

    
/*
 * fnpIOMCmd()
 *
 */

// 1 ignored command
// 0 ok
// -1 problem

int fnpIOMCmd (uint iomUnitIdx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iomUnitIdx] [chan];
// Is it an IDCW?

    if (p -> DCW_18_20_CP == 7)
      {
        return fnpCmd (iomUnitIdx, chan);
      }
    // else // DDCW/TDCW
    sim_printf ("%s expected IDCW\n", __func__);
    return -1;
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



static t_stat fnpShowIPCname (UNUSED FILE * st, UNIT * uptr,
                              UNUSED int val, UNUSED const void * desc)
  {   
    long n = FNP_UNIT_IDX (uptr);
    if (n < 0 || n >= N_FNP_UNITS_MAX)
      return SCPE_ARG;
    sim_printf("FNP IPC name is %s\n", fnpData.fnpUnitData [n] . ipcName);
    return SCPE_OK;
  }   

static t_stat fnpSetIPCname (UNUSED UNIT * uptr, UNUSED int32 value,
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

static t_stat fnpShowConfig (UNUSED FILE * st, UNIT * uptr, UNUSED int val, 
                             UNUSED const void * desc)
  {
    long fnpUnitIdx = FNP_UNIT_IDX (uptr);
    if (fnpUnitIdx >= (long) fnpDev.numunits)
      {
        sim_debug (DBG_ERR, & fnpDev, 
                   "fnpShowConfig: Invalid unit number %ld\n", fnpUnitIdx);
        sim_printf ("error: invalid unit number %ld\n", fnpUnitIdx);
        return SCPE_ARG;
      }

    sim_printf ("FNP unit number %ld\n", fnpUnitIdx);
    struct fnpUnitData * fudp = fnpData.fnpUnitData + fnpUnitIdx;

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
    if (fnpUnitIdx >= fnpDev . numunits)
      {
        sim_debug (DBG_ERR, & fnpDev, "fnpSetConfig: Invalid unit number %d\n", fnpUnitIdx);
        sim_printf ("error: fnpSetConfig: invalid unit number %d\n", fnpUnitIdx);
        return SCPE_ARG;
      }

    struct fnpUnitData * fudp = fnpData.fnpUnitData + fnpUnitIdx;

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
            if (strcmp (second, "ibm3270") == 0)
              fnpData.fnpUnitData[devnum].MState.line[linenum].service = service_3270;
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

t_stat fnpServerPort (UNUSED int32 arg, const char * buf)
  {
    int n = atoi (buf);
    if (n < 1 || n > 65535)
      return SCPE_ARG;
    fnpData.telnet_port = n;
    sim_printf ("FNP telnet server port set to %d\n", n);
    //fnpuvInit (fnpData.telnet_port);
    return SCPE_OK;
  }

t_stat fnpServer3270Port (UNUSED int32 arg, const char * buf)
  {
    int n = atoi (buf);
    if (n < 1 || n > 65535)
      return SCPE_ARG;
    fnpData.telnet3270_port = n;
    sim_printf ("FNP telnet3270 server port set to %d\n", n);
    return SCPE_OK;
  }

t_stat fnpStart (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_printf ("FNP force start\n");
    fnpuvInit (fnpData.telnet_port);
    fnpuv3270Init (fnpData.telnet3270_port);
    return SCPE_OK;
  }


#define PROMPT  "HSLA Port ("

void fnpConnectPrompt (uv_tcp_t * client)
  {
    fnpuv_start_writestr (client, (unsigned char *) PROMPT);
    bool first = true;
    for (int fnpno = 0; fnpno < N_FNP_UNITS_MAX; fnpno ++)
      {
        for (int lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];
            if (linep->service == service_login && ! linep->client)
              {
                if (! first)
                  fnpuv_start_writestr (client, (unsigned char *) ",");
                char name [16];
                first = false;
                sprintf (name, "%c.h%03d", 'a' + fnpno, lineno);
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

void fnp3270Msg (uv_tcp_t * client, unsigned char * msg)
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
    unsigned char EW [] = {245, 66, 17, 64, 64 };
    fnpuv_start_3270_write (client, EW, sizeof (EW));
    fnpuv_start_3270_write (client, buf, (ssize_t) l);
    fnpuv_send_eor (client);
  }

void fnp3270ConnectPrompt (uv_tcp_t * client)
  {
    uint fnpno = fnpData.ibm3270ctlr[ASSUME0].fnpno;
    uint lineno = fnpData.ibm3270ctlr[ASSUME0].lineno;
    //struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];
    uvClientData * p = client->data;
    p->assoc = true;
    p->fnpno = fnpno;
    p->lineno = lineno;
    fnpData.fnpUnitData[fnpno].MState.line[lineno].client = client;

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
    uvClientData * p = (uvClientData *) client->data;
    uint fnpno = p -> fnpno;
    uint lineno = p -> lineno;
    if (fnpno >= N_FNP_UNITS_MAX || lineno >= MAX_LINES)
      {
        sim_printf ("bogus client data\n");
        return;
      }
//sim_printf ("assoc. %d.%d nread %ld\n", fnpno, lineno, nread);
//{for (int i = 0; i < nread; i ++) sim_printf ("%c", isprint (e2a[buf[i]]) ? e2a[buf[i]] : '.');
//sim_printf ("\n");
//for (int i = 0; i < nread; i ++) sim_printf (" %02x", buf[i]);
//sim_printf ("\r\n");
//}

    struct t_line * linep = & fnpData.fnpUnitData[fnpno].MState.line[lineno];
    if (! fnpData.fnpUnitData[fnpno].MState.accept_calls)
      {
        if (linep->service == service_login)
          fnpuv_start_writestr (client, (unsigned char *) "Multics is not accepting calls\r\n");
        return;
      }
    if (! linep->listen)
      {
        if (linep->service == service_login)
          fnpuv_start_writestr (client, (unsigned char *) "Multics is not listening to this line\r\n");
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

void process3270Input (uv_tcp_t * client, unsigned char * buf, ssize_t nread)
  {
    uvClientData * p = (uvClientData *) client->data;
    uint fnpno = p -> fnpno;
    uint lineno = p -> lineno;
    uint stn_no = p->stationNo;

    if (fnpno >= N_FNP_UNITS_MAX || lineno >= MAX_LINES)
      {
        sim_printf ("bogus client data\n");
        return;
      }
//sim_printf ("assoc. %d.%d nread %ld\n", fnpno, lineno, nread);
//{for (int i = 0; i < nread; i ++) sim_printf ("%c", isprint (e2a[buf[i]]) ? e2a[buf[i]] : '.');
//sim_printf ("\n");
#if 0
for (int i = 0; i < nread; i ++) sim_printf (" %02x", buf[i]);
sim_printf ("\r\n");
#endif
//}

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
        //stn_p->stn_in_used = 0;
      }

sim_printf ("process3270Input stashed %lu bytes in stn %u\n", nread, stn_no);
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


    uint fnpno = 0;
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
        fnpno = (uint) (fnpcode - 'a');
        if (fnpData.fnpUnitData[fnpno].MState.line[lineno].service != service_login ||
            fnpData.fnpUnitData[fnpno].MState.line[lineno].client)
          {
            fnpuv_start_writestr (client, (unsigned char *) "not availible\r\n");
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
                if (fnpData.fnpUnitData[fnpno].MState.line[lineno].service == service_login &&
                    ! fnpData.fnpUnitData[fnpno].MState.line[lineno].client)
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

    fnpData.fnpUnitData[fnpno].MState.line[lineno].client = client;
//sim_printf ("associated %c.%03d %p\n", fnpno + 'a', lineno, client);
    p->assoc = true;
    p->fnpno = fnpno;
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
        sim_printf ("CONNECT (addr err %d) to %c.h%03d\n", ret, fnpno +'a', lineno);
      }
    else
      {
        struct sockaddr_in * p = (struct sockaddr_in *) & name;
        sim_printf ("CONNECT %s to %c.h%03d\n", inet_ntoa (p -> sin_addr), fnpno +'a', lineno);
      }

    sprintf (buf2, "Attached to line %c.h%03d\r\n", fnpno +'a', lineno);
    fnpuv_start_writestr (client, (unsigned char *) buf2);

    if (! fnpData.fnpUnitData[fnpno].MState.accept_calls)
      fnpuv_start_writestr (client, (unsigned char *) "Multics is not accepting calls\r\n");
    else if (! fnpData.fnpUnitData[fnpno].MState.line[lineno].listen)
      fnpuv_start_writestr (client, (unsigned char *) "Multics is not listening to this line\r\n");

    fnpData.fnpUnitData[fnpno].MState.line[lineno].lineType = 1 /* LINE_ASCII */;
    fnpData.fnpUnitData[fnpno].MState.line[lineno].accept_new_terminal = true;
    fnpData.fnpUnitData[fnpno].MState.line[lineno].was_CR = false;
    ltnRaw (p->telnetp);
  }
