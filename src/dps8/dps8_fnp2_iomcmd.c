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

//
// This module contains the code that runs under the IOM channel thread
//

#define ASSUME0 0

#include <stdio.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_utils.h"
#include "dps8_fnp2.h"
#include "dps8_fnp2_iomcmd.h"
#include "fnpuv.h"

#define DBG_CTR 1

#ifdef THREADZ
#include "threadz.h"
#endif

static inline void fnp_core_read (word24 addr, word36 *data, UNUSED const char * ctx)
  {
#ifdef THREADZ
    lock_mem ();
#endif
#ifdef SCUMEM
    iom_core_read (addr, data, ctx);
#else
    * data = M [addr] & DMASK;
#endif
#ifdef THREADZ
    unlock_mem ();
#endif
  }

static inline void fnp_core_write (word24 addr, word36 data, UNUSED const char * ctx)
  {
#ifdef THREADZ
    lock_mem ();
#endif
#ifdef SCUMEM
    iom_core_write (addr, data, ctx);
#else
    M [addr] = data & DMASK;
#endif
#ifdef THREADZ
    unlock_mem ();
#endif
  }

static inline void fnp_core_read_n (word24 addr, word36 *data, uint n, UNUSED const char * ctx)
  {
#ifdef THREADZ
    lock_mem ();
#endif
    for (uint i = 0; i < n; i ++)
#ifdef SCUMEM
      iom_core_read (addr, data, ctx);
#else
      data [i] = M [addr + i] & DMASK;
#endif
#ifdef THREADZ
    unlock_mem ();
#endif
  }

#ifdef THREADZ
static inline void l_putbits36_1 (word36 vol * x, uint p, word1 val)
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
#ifdef THREADZ
    lock_mem ();
#endif
    * x = (* x & ~ smask) | (((word36) val & mask) << shift);
#ifdef THREADZ
    unlock_mem ();
#endif
}
#else
#define l_putbits36_1 putbits36_1
#endif


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

//
// As mailbox messages are processed, decoded data are stashed here
///

struct decoded_t
  {
    uint devUnitIdx;
    uint op_code;
    uint slot_no;
    struct dn355_submailbox vol * smbxp;
    struct fnp_submailbox vol * fsmbxp;
    struct mailbox vol * mbxp;
    struct fnpUnitData * fudp;
    iomChanData_t * p;
    uint cell;
  };

__thread static struct decoded_t decoded;

static void setTIMW (uint mailboxAddress, int mbx)
  {
    uint timwAddress = mailboxAddress + TERM_INPT_MPX_WD;
#if 1
    l_putbits36_1 (& M [timwAddress], (uint) mbx, 1);
#else
    lock_mem ();
//sim_printf ("new %p\n", & M [timwAddress]
    //word36 w = M [timwAddress];
    word36 w;
    fnp_core_read (timwAddress, & w, "setTIMW read");
    putbits36_1 (& w, (uint) mbx, 1);
    //M [timwAddress] = w;
    fnp_core_write (timwAddress, w, "setTIMW write");
    unlock_mem ();
#endif
  }

//
// Convert virtual address to physical
//

static uint virtToPhys (uint ptPtr, uint l66Address)
  {
    uint pageTable = ptPtr * 64u;
    uint l66AddressPage = l66Address / 1024u;

    word36 ptw;
#ifdef SCUMEM
    uint iomUnitIdx = (uint) cables->cablesFromIomToFnp [decoded.devUnitIdx].iomUnitIdx;
    iom_core_read (iomUnitIdx, pageTable + l66AddressPage, & ptw, "fnp_iom_cmd get ptw");
#else
    fnp_core_read (pageTable + l66AddressPage, & ptw, "fnp_iom_cmd get ptw");
#endif
    uint page = getbits36_14 (ptw, 4);
    uint addr = page * 1024u + l66Address % 1024u;
    return addr;
  }

//
// Debugging...
//

static void dmpmbx (uint mailboxAddress)
  {
    struct mailbox mbx;
    fnp_core_read_n (mailboxAddress, (word36 *) & mbx, MAILBOX_WORDS, "dmpmbx");
    sim_printf ("dia_pcw            %012"PRIo64"\n", mbx.dia_pcw);
    sim_printf ("mailbox_requests   %012"PRIo64"\n", mbx.mailbox_requests);
    sim_printf ("term_inpt_mpx_wd   %012"PRIo64"\n", mbx.term_inpt_mpx_wd);
    sim_printf ("last_mbx_req_count %012"PRIo64"\n", mbx.last_mbx_req_count);
    sim_printf ("num_in_use         %012"PRIo64"\n", mbx.num_in_use);
    sim_printf ("mbx_used_flags     %012"PRIo64"\n", mbx.mbx_used_flags);
    for (uint i = 0; i < 8; i ++)
      {
        sim_printf ("CS  mbx %d\n", i);
        sim_printf ("    word1        %012"PRIo64"\n",
                    mbx.dn355_sub_mbxes[i].word1);
        sim_printf ("    word2        %012"PRIo64"\n",
                    mbx.dn355_sub_mbxes[i].word2);
        sim_printf ("    command_data %012"PRIo64"\n",
                    mbx.dn355_sub_mbxes[i].command_data [0]);
        sim_printf ("                 %012"PRIo64"\n",
                    mbx.dn355_sub_mbxes[i].command_data [1]);
        sim_printf ("                 %012"PRIo64"\n",
                    mbx.dn355_sub_mbxes[i].command_data [2]);
        sim_printf ("    word6        %012"PRIo64"\n",
                    mbx.dn355_sub_mbxes[i].word6);
      }
    for (uint i = 0; i < 4; i ++)
      {
        sim_printf ("FNP mbx %d\n", i);
        sim_printf ("    word1        %012"PRIo64"\n",
                    mbx.fnp_sub_mbxes[i].word1);
        sim_printf ("    word2        %012"PRIo64"\n",
                    mbx.fnp_sub_mbxes[i].word2);
        sim_printf ("    mystery      %012"PRIo64"\n",
                    mbx.fnp_sub_mbxes[i].mystery [0]);
        sim_printf ("                 %012"PRIo64"\n",
                    mbx.fnp_sub_mbxes[i].mystery [1]);
        sim_printf ("                 %012"PRIo64"\n",
                    mbx.fnp_sub_mbxes[i].mystery [2]);
      }
        
  }

//
// wcd; Multics has sent a Write Control Data command to the FNP
//

static int wcd (void)
  {
#ifdef SCUMEM
    uint iomUnitIdx = (uint) cables->cablesFromIomToFnp[decoded.devUnitIdx].iomUnitIdx;
#endif

    struct t_line * linep = & decoded.fudp->MState.line[decoded.slot_no];
    sim_debug (DBG_TRACE, & fnp_dev, "[%u] wcd op_code %u 0%o\n", decoded.slot_no, decoded.op_code, decoded.op_code);
    switch (decoded.op_code)
      {
        case  1: // disconnect_this_line
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    disconnect_this_line\n", decoded.slot_no);
            if (linep->line_client && linep->service == service_login)
              fnpuv_start_writestr (linep->line_client, (unsigned char *) "Multics has disconnected you\r\n");
#ifdef DISC_DELAY
            // '1' --> disconnect on next poll
            linep -> line_disconnected = 1;
#else
            linep -> line_disconnected = true;
#endif
            linep -> listen = false;
            if (linep->line_client)
              {
                close_connection ((uv_stream_t *) linep->line_client);
              }
            
          }
          break;


        case  3: // dont_accept_calls
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    dont_accept_calls\n", decoded.slot_no);
            decoded.fudp->MState.accept_calls = false;
          }
          break;

        case  4: // accept_calls
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    accept_calls\n", decoded.slot_no);
            decoded.fudp->MState.accept_calls = true;
          }
          break;

        case  8: // set_framing_chars
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    set_framing_chars\n", decoded.slot_no);
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    dial out\n", decoded.slot_no);
            word36 command_data0 = decoded.smbxp -> command_data [0];
            word36 command_data1 = decoded.smbxp -> command_data [1];
            word36 command_data2 = decoded.smbxp -> command_data [2];
            //sim_printf ("XXX dial_out %d %012"PRIo64" %012"PRIo64" %012"PRIo64"", decoded.slot_no, command_data0, command_data1, command_data2);
            fnpuv_dial_out (decoded.devUnitIdx, decoded.slot_no, command_data0, command_data1, command_data2);
          }
          break;

        case 22: // line_control
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    line_control\n", decoded.slot_no);
            word36 command_data0 = decoded.smbxp -> command_data [0];
            word36 command_data1 = decoded.smbxp -> command_data [1];
            //word36 command_data2 = decoded.smbxp -> command_data [2];
            //sim_printf ("XXX line_control %d %012"PRIo64" %012"PRIo64" %012"PRIo64"\n", decoded.slot_no, command_data0, command_data1, command_data2);

// bisync_line_data.inc.pl1
            word18 op = getbits36_18 (command_data0, 0);
#ifdef FNP2_DEBUG
            word18 val1 = getbits36_18 (command_data0, 18);
            word18 val2 = getbits36_18 (command_data1, 0);
#endif
            //word18 val3 = getbits36_18 (command_data1, 18);
//sim_printf ("line_control %d op %d. %o\r\n", decoded.slot_no, op, op);
            switch (op)
              {
                case 1:
#ifdef FNP2_DEBUG
                  sim_printf ("SET_BID_LIMIT\n");
                  sim_printf ("    %u\n", val1);
#endif
                  break;
                case 2:
#ifdef FNP2_DEBUG
                  sim_printf ("ACCEPT_BID\n");
#endif
                  break;
                case 3:
#ifdef FNP2_DEBUG
                  sim_printf ("CONFIGURE\n");
                  if (val1 == 0)
                    sim_printf ("    non-transparent ASCII\n");
                  else if (val1 == 1)
                    sim_printf ("    non-transparent EBCDIC\n");
                  else if (val1 == 2)
                    sim_printf ("    transparent ASCII\n");
                  else if (val1 == 3)
                    sim_printf ("    transparent EBCDIC\n");
                  else 
                    sim_printf ("    unknown %u. %o\n", val1, val1);
#endif
                  break;
                case 4:
#ifdef FNP2_DEBUG
                  sim_printf ("SET_TTD_PARAMS\n");
                  sim_printf ("    ttd_time  %u\n", val1);
                  sim_printf ("    ttd_limit %u\n", val2);
#endif
                  break;
                case 5:
#ifdef FNP2_DEBUG
                  sim_printf ("REPORT_WRITE_STATUS\n");
#endif
                  break;
                case 6:
#ifdef FNP2_DEBUG
                  sim_printf ("SET_3270_MODE\n");
#endif
                  break;
                case 7:
                  {
#ifdef FNP2_DEBUG
                    sim_printf ("SET_POLLING_ADDR\n");
#endif
//word36 command_data2 = decoded.smbxp -> command_data [2];
//sim_printf ("XXX line_control %d %012"PRIo64" %012"PRIo64" %012"PRIo64"\n", decoded.slot_no, command_data0, command_data1, command_data2);
                    //word9 len = getbits36_9 (command_data0, 18);
                    word9 c1 = getbits36_9 (command_data0, 27);
                    //word9 c2 = getbits36_9 (command_data1, 0);
                    word9 c3 = getbits36_9 (command_data1, 9);
                    //word9 c4 = getbits36_9 (command_data1, 18);
#ifdef FNP2_DEBUG
                    //sim_printf ("    data_len %u\n", len);
                    sim_printf ("    char1 %u\n", c1);
                    //sim_printf ("    char2 %u\n", c2);
                    sim_printf ("    char3 %u\n", c3);
                    //sim_printf ("    char4 %u\n", c4);
#endif
                    fnpData.ibm3270ctlr[ASSUME0].pollCtlrChar = (unsigned char) (c1 & 0xff);
                    fnpData.ibm3270ctlr[ASSUME0].pollDevChar = (unsigned char) (c3 & 0xff);
                    fnpData.
                      fnpUnitData[decoded.devUnitIdx].
                        MState.
                          line[decoded.slot_no].
                            line_client = NULL;
                  }
                  break;
                case 8:
#ifdef FNP2_DEBUG
                  sim_printf ("START_POLL\n");
#endif
                  fnpuv3270Poll (true);
                  break;
                case 9:
                  {
#ifdef FNP2_DEBUG
                    sim_printf ("SET_SELECT_ADDR\n");
#endif
                    //word9 len = getbits36_9 (command_data0, 18);
                    word9 c1 = getbits36_9 (command_data0, 27);
                    //word9 c2 = getbits36_9 (command_data1, 0);
                    word9 c3 = getbits36_9 (command_data1, 9);
                    //word9 c4 = getbits36_9 (command_data1, 18);
#ifdef FNP2_DEBUG
                    //sim_printf ("    data_len %u\n", len);
                    sim_printf ("    char1 %u\n", c1);
                    //sim_printf ("    char2 %u\n", c2);
                    sim_printf ("    char3 %u\n", c3);
                    //sim_printf ("    char4 %u\n", c4);
#endif
                    fnpData.ibm3270ctlr[ASSUME0].selCtlrChar = (unsigned char) (c1 & 0xff);
                    fnpData.ibm3270ctlr[ASSUME0].selDevChar = (unsigned char) (c3 & 0xff);

                    // General Poll
                    if (fnpData.ibm3270ctlr[ASSUME0].selDevChar == 127)
                      {
                        fnpData.
                          fnpUnitData[decoded.devUnitIdx].
                            MState.
                              line[decoded.slot_no].
                                line_client = NULL;
                        break;
                      }
// Setup line_client to that wtx can locate the connection

                    // Find the client from the device selection call

                    uint stn_no;
                    for (stn_no = 0; stn_no < ADDR_MAP_ENTRIES; stn_no ++)
                      if (addr_map [stn_no] == fnpData.ibm3270ctlr[ASSUME0].selDevChar)
                        break;
                    if (stn_no >= ADDR_MAP_ENTRIES)
                      {
                        sim_warn ("SET_POLLING_ADDR couldn't find selDevChar %02hhx\r\n", fnpData.ibm3270ctlr[ASSUME0].selDevChar);
                        break;
                      }
                    fnpData.
                      fnpUnitData[decoded.devUnitIdx].
                        MState.
                          line[decoded.slot_no].
                            line_client = 
                                           fnpData.
                                             ibm3270ctlr[ASSUME0].
                                               stations[stn_no].
                                                 client;
                  }
                  break;
                case 10:
#ifdef FNP2_DEBUG
                  sim_printf ("STOP_AUTO_POLL\n");
#endif
                  break;
                case 11:
#ifdef FNP2_DEBUG
                  sim_printf ("SET_MASTER_SLAVE_MODE\n");
                  if (val1 == 0)
                    sim_printf ("    slave\n");
                  else if (val1 == 1)
                    sim_printf ("    master\n");
                  else 
                    sim_printf ("    unknown %u. %o\n", val1, val1);
#endif
                  break;
                case 12:
#ifdef FNP2_DEBUG
                  sim_printf ("SET_HASP_MODE\n");
#endif
                  break;
                case 13:
#ifdef FNP2_DEBUG
                  sim_printf ("SET_NAK_LIMIT\n");
                  sim_printf ("    %u\n", val1);
#endif
                  break;
                case 14:
#ifdef FNP2_DEBUG
                  sim_printf ("SET_HASP_TIMERS\n");
#endif
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
            linep->sync_msg_size = (uint) getbits36_18 (command_data0, 0);
            //sim_printf ("sync_msg_size %u\n", sz);
          }
          break;

        case 24: // set_echnego_break_table
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    set_echnego_break_table\n", decoded.slot_no);
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
#ifdef SCUMEM
                    iom_core_read (iomUnitIdx, dataAddrPhys + i, & echoTable [i], __func__);
#else
                    //echoTable [i] = M [dataAddrPhys + i];
                    fnp_core_read (dataAddrPhys + i, echoTable + i, "echo table");
#endif
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    start_negotiated_echo\n", decoded.slot_no);
            //word18 ctr = getbits36_18 (decoded.smbxp -> command_data [0], 0);
            //word18 screenleft = getbits36_18 (decoded.smbxp -> command_data [0], 18);

//sim_printf ("start_negotiated_echo ctr %d screenleft %d\n", ctr, screenleft);
          }
        case 26: // stop_negotiated_echo
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    stop_negotiated_echo\n", decoded.slot_no);
          }
          break;

        case 27: // init_echo_negotiation
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    init_echo_negotiation\n", decoded.slot_no);
            //linep -> send_output = true;
            linep -> send_output = SEND_OUTPUT_DELAY;
            linep -> ack_echnego_init = true;
          }
          break;

        case 30: // input_fc_chars
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    input_fc_chars\n", decoded.slot_no);
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    output_fc_chars\n", decoded.slot_no);
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    alter_parameters\n", decoded.slot_no);
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
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters fullduplex %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp full_duplex\n");
                    linep->fullDuplex = !! flag;
                  }
                  break;

                case  8: // Crecho
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters crecho %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp crecho\n");
                    linep->crecho = !! flag;
                  }
                  break;

                case  9: // Lfecho
                  {
                    //sim_printf ("fnp lfecho\n");
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters lfecho %u\n", decoded.slot_no, flag);
                    linep->lfecho = !! flag;
                  }
                  break;

                case 13: // Dumpoutput
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters dumpoutput\n", decoded.slot_no);
                    //sim_printf ("fnp dumpoutput\n");
                    // XXX ignored
                    //linep -> send_output = true;
                    linep -> send_output = SEND_OUTPUT_DELAY;
                  }
                  break;

                case 14: // Tabecho
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters tabecho %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp tabecho\n");
                    linep->tabecho = !! flag;
                  }
                  break;

                case 16: // Listen
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters listen %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp listen %p %d.%d %d\n", linep->line_client, decoded.devUnitIdx,decoded.slot_no, flag);
                    uint bufsz = getbits36_18 (decoded.smbxp->command_data[0], 18);
                    linep->listen = !! flag;
                    linep->inputBufferSize = bufsz;

                    if (linep->service == service_undefined)
                      linep->service = service_login;

                    if (linep->service == service_login && linep -> line_client)
                      {
                        fnpuv_start_writestr (linep->line_client,
                          linep->listen ?
                            (unsigned char *) "Multics is now listening to this line\r\n":
                            (unsigned char *) "Multics is no longer listening to this line\r\n");
                      }
                    if (linep->service == service_slave && ! linep -> line_client)
                      fnpuv_open_slave (decoded.devUnitIdx, decoded.slot_no);
                  }
                  break;

                case 17: // Hndlquit
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters handlequit%u \n", decoded.slot_no, flag);
                    //sim_printf ("fnp handle_quit %d\n", flag);
                    linep->handleQuit = !! flag;
                  }
                  break;

                case 18: // Chngstring
                  {
                    //sim_printf ("fnp Change control string\n");
                    uint idx =  getbits36_9 (decoded.smbxp -> command_data [0], 9);
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters chngstring %u\n", decoded.slot_no, flag);
                    linep->ctrlStrIdx = idx;
                  }
                  break;

                case 19: // Wru
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters wru\n", decoded.slot_no);
                    linep -> wru_timeout = true;
                  }
                  break;

                case 20: // Echoplex
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters echoplex %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp echoplex\n");
                    linep->echoPlex = !! flag;
                  }
                  break;

                case 22: // Dumpinput
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters dumpinput\n", decoded.slot_no);
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
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters replay %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp replay\n");
                    linep->replay = !! flag;
                  }
                  break;

                case 24: // Polite
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters polite %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp polite\n");
                    linep->polite = !! flag;
                  }
                  break;

                case 25: // Block_xfer
                  {
                    uint bufsiz1 = getbits36_18 (decoded.smbxp -> command_data [0], 18);
                    uint bufsiz2 = getbits36_18 (decoded.smbxp -> command_data [1], 0);
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters block_xfer %u %u\n", decoded.slot_no, bufsiz1, bufsiz2);
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
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters set_buffer_size %u\n", decoded.slot_no, flag);
                    linep->inputBufferSize = sz;
//sim_printf ("Set_buffer_size %u\n", sz);
                  }

                case 27: // Breakall
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters breakall %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp break_all\n");
                    linep->breakAll = !! flag;
                  }
                  break;

                case 28: // Prefixnl
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters prefixnl %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp prefixnl\n");
                    linep->prefixnl = !! flag;
                  }
                  break;

                case 29: // Input_flow_control
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters input_flow_control %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp input_flow_control\n");
                    linep->input_flow_control = !! flag;
                  }
                  break;

                case 30: // Output_flow_control
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters output_flow_control %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp output_flow_control\n");
                    linep->output_flow_control = !! flag;
                  }
                  break;

                case 31: // Odd_parity
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters odd_parity %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp odd_parity\n");
                    linep->odd_parity = !! flag;
                  }
                  break;

                case 32: // Eight_bit_in
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters eight_bit_in %u\n", decoded.slot_no, flag);
                    //sim_printf ("fnp eight_bit_in\n");
                    linep->eight_bit_in = !! flag;
                  }
                  break;

                case 33: // Eight_bit_out
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters eight_bit_out %u\n", decoded.slot_no, flag);
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
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters unimplemented\n", decoded.slot_no);
                    sim_printf ("fnp unimplemented subtype %d (%o)\n", subtype, subtype);
                    // doFNPfault (...) // XXX
                    return -1;
                  }

                default:
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        alter_parameters illegal\n", decoded.slot_no);
                    sim_printf ("fnp illegal subtype %d (%o)\n", subtype, subtype);
                    // doFNPfault (...) // XXX
                    return -1;
                  }
              } // switch (subtype)
          }
          break; // alter_parameters

        case 37: // set_delay_table
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    set_delay_table\n", decoded.slot_no);
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    report_meters\n", decoded.slot_no);
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    unimplemented opcode\n", decoded.slot_no);
            sim_warn ("fnp unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            //sim_debug (DBG_ERR, & fnp_dev, "fnp unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            //sim_printf ("fnp unimplemented opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            // doFNPfault (...) // XXX
            //return -1;
          }
        break;

        default:
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    illegal opcode\n", decoded.slot_no);
            sim_debug (DBG_ERR, & fnp_dev, "[%u]fnp illegal opcode %d (%o)\n", decoded.slot_no, decoded.op_code, decoded.op_code);
            sim_warn ("fnp illegal opcode %d (%o)\n", decoded.op_code, decoded.op_code);
            // doFNPfault (...) // XXX
            return -1;
          }
      } // switch decoded.op_code

    setTIMW (decoded.fudp->mailboxAddress, (int) decoded.cell);

#ifdef FNPDBG
sim_printf ("wcd sets the TIMW??\n");
#endif
    return 0;
  }

#ifdef TUN
static void tun_write (struct t_line * linep, uint16_t * data, uint tally)
  {
#if 0
    for (uint i = 0; i < tally; i ++)
      sim_printf ("%4o", data[i]);
    sim_printf ("\r\n");
#endif
// XXX this code is buggy; if a buffer is recieved with an embedded frame start, the embedded frame 
// XXX will be lost

    for (uint i = 0; i < tally; i ++)
      {
        // Check for start of frame...
        if (data [i] == 0x100)
          {
            linep->in_frame = true;
            linep->frameLen = 0;
            continue;
          }

        if (! linep->in_frame)
          continue;

        if (linep->frameLen >= 2+1500)
          {
            sim_printf ("inFrame overrun\n");
            break;
          }
        linep->frame[linep->frameLen ++] = (uint8_t) (data [i] & 0xff);
      }

// Is frame complete?

      if (linep->frameLen >= 2)
        {
          uint16_t target = (uint16_t) ((linep->frame[0] & 0xff) << 8) | (linep->frame[1]);
          if (target + 2 >= linep->frameLen)
            {
              sim_printf ("frame received\n");
              fnpuv_tun_write (linep);
              linep->in_frame = false;
            }
        }
  }
#endif

static void fnp_wtx_output (uint tally, uint dataAddr)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%u]rcd wtx_output\n", decoded.slot_no);
    struct t_line * linep = & decoded.fudp->MState.line[decoded.slot_no];


    uint wordOff = 0;
    word36 word = 0;
    uint lastWordOff = (uint) -1;
#ifdef TUN
    uint16_t data9 [tally];
#endif
    unsigned char data [tally];
    uint ptPtr = decoded.p -> PCW_PAGE_TABLE_PTR;

#if 0
{ sim_printf ("line %d tally %d\n", decoded.slot_no, tally);
  uint nw = (tally + 3) / 4;
  for (uint i = 0; i < nw; i ++)
    {
      uint wordAddr = virtToPhys (ptPtr, dataAddr + i);
      word = M [wordAddr];
      sim_printf (" %012"PRIo64"\n", word);
    }
}
#endif
#if 0
{
  uint bcnt = 0;
  word36 hi = 0, lo = 0;
  uint ndws = (tally + 8) / 9;

  for (uint i = 0; i < ndws * 8; i ++)
    {
      uint wordAddr = virtToPhys (ptPtr, dataAddr + i/4);
      word = M [wordAddr];
      uint os = (i%4) * 9 + 1;
      uint d = 0;
      if (i < tally)
        d = getbits36_8 (word, os);
      d &= MASK8;
      uint carry = getbits36_8 (lo, 8);
      carry &= MASK8;
      hi <<= 8;
      hi |= carry;
      lo <<= 8;
      lo |= d;
      bcnt ++;
      if (bcnt == 9)
        {
          sim_printf ("%012"PRIo64"\n%012"PRIo64"\n", hi, lo);
          hi = lo = bcnt = 0;
        }
    }
 }
#endif
#if 0
{ sim_printf ("line %d tally %d\n", decoded.slot_no, tally);
  uint bcnt = 0;
  word72 ac;
  uint ndws = (tally + 8) / 9;

  for (uint i = 0; i < ndws * 8; i ++)
    {
      uint wordAddr = virtToPhys (ptPtr, dataAddr + i/4);
      word = M [wordAddr];
      uint os = (i%4) * 9 + 1;
      uint d = 0;
      if (i < tally)
        d = getbits36_8 (word, os);
      d &= MASK8;
      ac <<= 8;
      ac |= d;
      bcnt ++;
      if (bcnt == 9)
        {
          sim_printf ("%012"PRIo64"\n%012"PRIo64"\n", (word36) ((ac >> 36)) & MASK36, (word36) (ac & MASK36));
          ac = bcnt = 0;
        }
    }
  if (bcnt)
    {
      while (bcnt < 9)
        {
          ac <<= 8;
          bcnt ++;
        }
      sim_printf ("%012"PRIo64"\n%012"PRIo64"\n", (word36) ((ac >> 36)) & MASK36, (word36) (ac & MASK36));
    }
 }
#endif
 
    for (uint i = 0; i < tally; i ++)
       {
         uint byteOff = i % 4;
         uint byte = 0;

         wordOff = i / 4;

         if (wordOff != lastWordOff)
           {
             lastWordOff = wordOff;
             uint wordAddr = virtToPhys (ptPtr, dataAddr + wordOff);
#ifdef SCUMEM
             uint iomUnitIdx = (uint) cables->cablesFromIomToFnp [decoded.devUnitIdx].iomUnitIdx;
             iom_core_read (iomUnitIdx, wordAddr, & word, __func__);
#else
             //word = M [wordAddr];
             fnp_core_read (wordAddr, & word, "fnp_wtx_output");
#endif
//sim_printf ("   %012"PRIo64"\n", M [wordAddr]);
           }
         byte = getbits36_9 (word, byteOff * 9);
         data [i] = byte & 0377;
#ifdef TUN
         data9 [i] = (uint16_t) byte;
#endif

//sim_printf ("   %03o %c\n", data [i], isgraph (data [i]) ? data [i] : '.');
       }
//#if 0
if_sim_debug (DBG_TRACE, & fnp_dev) {
{ sim_printf ("[%u][FNP emulator: OUT: '", decoded.slot_no);
for (uint i = 0; i < tally; i ++)
{
if (isgraph (data [i]))
sim_printf ("%c", data [i]);
else
sim_printf ("\\%03o", data [i]);
}
sim_printf ("']\n");
}
}
#ifdef TUN
    if (linep->is_tun && tally > 0)
      {
        tun_write (linep, data9, tally);
        return;
     }
#endif
    if (tally > 0 && linep->line_client)
      {
        if (! linep->line_client || ! linep->line_client->data)
          {
            sim_warn ("fnp_wtx_output bad client data\r\n");
            return;
          }
        uvClientData * p = linep->line_client->data;
        (* p->write_cb) (linep->line_client, data, tally);
      }
  }

static int wtx (void)
  {
    sim_debug (DBG_TRACE, & fnp_dev, "[%u]wtx op_code %u 0%o\n", decoded.slot_no, decoded.op_code, decoded.op_code);
//sim_printf ("wtx op_code %o (%d.) %c.h%03d\n", decoded.op_code, decoded.op_code, decoded.devUnitIdx+'a', decoded.slot_no);
    if (decoded.op_code != 012 && decoded.op_code != 014)
      {
        sim_debug (DBG_TRACE, & fnp_dev, "[%u]     unimplemented opcode\n", decoded.slot_no);
        sim_debug (DBG_ERR, & fnp_dev, "[%u]fnp wtx unimplemented opcode %d (%o)\n", decoded.slot_no, decoded.op_code, decoded.op_code);
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
#ifdef SCUMEM
        word36 dcw;
        uint iomUnitIdx = (uint) cables->cablesFromIomToFnp [decoded.devUnitIdx].iomUnitIdx;
        iom_core_read (iomUnitIdx, dcwAddrPhys, & dcw, __func__);
#else
        //word36 dcw = M [dcwAddrPhys];
        word36 dcw;
        fnp_core_read (dcwAddrPhys, & dcw, "dcw");
#endif
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

    setTIMW (decoded.fudp->mailboxAddress, (int) decoded.cell);

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

    struct input_sub_mbx * p = (struct input_sub_mbx *) decoded.fsmbxp;
    word9 n_buffers = getbits36_9 (p->n_buffers, 27);
    word24 addr0 = getbits36_24 (p->dcws[0], 0);
    word12 tally0 = getbits36_12 (p->dcws[0], 24);
    word24 addr1 = getbits36_24 (p->dcws[1], 0);
    word12 tally1 = getbits36_12 (p->dcws[1], 24);
    if (n_buffers > 2)
      sim_warn ("n_buffers > 2?\n");
    

    struct t_line * linep = & decoded.fudp->MState.line[decoded.slot_no];
    unsigned char * data = linep -> buffer;

if_sim_debug (DBG_TRACE, & fnp_dev) {
{ sim_printf ("[%u][FNP emulator: long IN: '", decoded.slot_no);
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
#ifdef SCUMEM
    uint iomUnitIdx = (uint) cables->cablesFromIomToFnp [decoded.devUnitIdx].iomUnitIdx;
#endif
//sim_printf ("long  in; line %d tally %d\n", decoded.slot_no, linep->nPos);
    for (int i = 0; i < tally0; i += 4)
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
#ifdef SCUMEM
        iom_core_write (iomUnitIdx, addr0, v, __func__);
#else
        //M [addr0 ++] = v;
        fnp_core_write (addr0, v, "rtx_input_accepted");
#endif
        //M [addr0 ++] = v;
        uint dcwAddrPhys = virtToPhys (decoded.p -> PCW_PAGE_TABLE_PTR, addr0);
        M [dcwAddrPhys] = v;
        addr0 ++;
      }

    if (n_buffers > 1 && linep->nPos > tally0)
      {
        for (int i = 0; i < tally1; i += 4)
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
#ifdef SCUMEM
            iom_core_write (iomUnitIdx, addr1, v, __func__);
#else
        //M [addr1 ++] = v;
            fnp_core_write (addr1, v, "rtx_input_accepted");
#endif
            //M [addr1 ++] = v;
            uint dcwAddrPhys = virtToPhys (decoded.p -> PCW_PAGE_TABLE_PTR, addr1);
            M [dcwAddrPhys] = v;
            addr1 ++;
          }
      }

    // temporary until the logic is in place XXX
    // This appears to only be used in tty_interrupt.pl1 as
    // rtx_info.output_in_fnp as part of echo negotiation:
    //    if ^rtx_info.output_in_fnp  /* if there's no output going on */
    // So apparently a flag indicating that there is output queued.
    word1 output_chain_present = 1;

    l_putbits36_1 (& p->command_data, 16, output_chain_present);
    l_putbits36_1 (& p->command_data, 17, linep->input_break ? 1 : 0);

    // Mark the line as ready to receive more data
    linep->input_reply_pending = false;
    linep->input_break = false;
    linep->nPos = 0;

    setTIMW (decoded.fudp->mailboxAddress, (int) decoded.cell);
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]rcd unimplemented\n", decoded.slot_no);
            sim_debug (DBG_ERR, & fnp_dev, "[%u]fnp unimplemented io_cmd %d\n", decoded.slot_no, io_cmd);
             sim_printf ("fnp unimplemented io_cmd %d\n", io_cmd);
            // doFNPfault (...) // XXX
            return -1;
          }
        default:
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]rcd illegal opcode\n", decoded.slot_no);
            sim_debug (DBG_ERR, & fnp_dev, "[%u]fnp illegal io_cmd %d\n", decoded.slot_no, io_cmd);
            sim_printf ("fnp illegal io_cmd %d\n", io_cmd);
            // doFNPfault (...) // XXX
            return -1;
          }
      } // switch (io_cmd)
    return 0;
  }

static int interruptL66_FNP_to_CS (void)
  {
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

    sim_debug (DBG_TRACE, & fnp_dev, "[%u]fnp interrupt\n", decoded.slot_no);
    switch (io_cmd)
      {
        case 2: // rtx (read transmission)
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    rtx\n", decoded.slot_no);
            switch (op_code)
              {
                case  5: // input_accepted
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        input_accepted\n", decoded.slot_no);
                    fnp_rtx_input_accepted ();
                  }
                  break;
                default:
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        illegal rtx ack\n", decoded.slot_no);
                  sim_warn ("rtx %d. %o ack ignored\n", op_code, op_code);
                  break;
              }
              break;
          }
        case 3: // wcd (write control data)
          {
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]    wcd\n", decoded.slot_no);
            switch (op_code)
              {
                case  0: // terminal_accepted
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        terminal accepted\n", decoded.slot_no);
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
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        disconnect_this_line\n", decoded.slot_no);
                    //sim_printf ("disconnect_this_line ack.\n");
                  }
                  break;

                case 14: // reject_request_temp
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        reject_request_temp\n", decoded.slot_no);
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
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        unimplemented opcode\n", decoded.slot_no);
                    sim_debug (DBG_ERR, & fnp_dev, "[%u]fnp reply unimplemented opcode %d (%o)\n", decoded.slot_no, op_code, op_code);
                    sim_printf ("fnp reply unimplemented opcode %d (%o)\n", op_code, op_code);
                    // doFNPfault (...) // XXX
                    return -1;
                  }

                default:
                  {
                    sim_debug (DBG_TRACE, & fnp_dev, "[%u]        illegal opcode\n", decoded.slot_no);
                    sim_debug (DBG_ERR, & fnp_dev, "[%u]fnp reply illegal opcode %d (%o)\n", decoded.slot_no, op_code, op_code);
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
            sim_debug (DBG_TRACE, & fnp_dev, "[%u]        illegal io_cmd\n", decoded.slot_no);
            sim_debug (DBG_ERR, & fnp_dev, "[%u]illegal/unimplemented io_cmd (%d) in fnp submbx\n", decoded.slot_no, io_cmd);
            sim_printf ("illegal/unimplemented io_cmd (%d) in fnp submbx\n", io_cmd);
            // doFNPfault (...) // XXX
            return -1;
          }
      } // switch (io_cmd)
    return 0;
  }

static int interruptL66_CS_done (void)
  {
    uint mbx = decoded.cell - 12;
    if (! decoded.fudp -> fnpMBXinUse [mbx])
      {
        sim_debug (DBG_ERR, & fnp_dev, "odd -- Multics marked an unused mbx as unused? cell %d (mbx %d)\n", decoded.cell, mbx);
        sim_debug (DBG_ERR, & fnp_dev, "  %d %d %d %d\n", decoded.fudp -> fnpMBXinUse [0], decoded.fudp -> fnpMBXinUse [1], decoded.fudp -> fnpMBXinUse [2], decoded.fudp -> fnpMBXinUse [3]);
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
    decoded.p = & iomChanData [iomUnitIdx] [chan];
    decoded.devUnitIdx = get_ctlr_idx (iomUnitIdx, chan);
    decoded.fudp = & fnpData.fnpUnitData [decoded.devUnitIdx];
#ifdef SCUMEM
    word24 offset;
    int scuUnitNum =  queryIomScbankMap (iomUnitIdx, decoded.fudp->mailboxAddress, & offset);
    int scuUnitIdx = kables->iom_to_scu[iomUnitIdx][scuUnitNum].scu_unit_idx;
    decoded.mbxp = (struct mailbox vol *) & scu [scuUnitIdx].M [decoded.fudp->mailboxAddress];
#else
    decoded.mbxp = (struct mailbox vol *) & M [decoded.fudp -> mailboxAddress];
#endif
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
        sim_debug (DBG_ERR, & fnp_dev, "fnp illegal cell number %d\n", decoded.cell);
        sim_printf ("fnp illegal cell number %d\n", decoded.cell);
        // doFNPfault (...) // XXX
        return -1;
      }
    return 0;
  }

static void fnpcmdBootload (uint devUnitIdx)
  {
    sim_printf("Received BOOTLOAD command...\n");
    fnpData.fnpUnitData[devUnitIdx].MState.accept_calls = false;
    for (uint lineno = 0; lineno < MAX_LINES; lineno ++)
      {
        fnpData.fnpUnitData[devUnitIdx].MState.line [lineno] . listen = false;
        if (fnpData.fnpUnitData[devUnitIdx].MState.line [lineno].line_client)
          {
            fnpuv_start_writestr (fnpData.fnpUnitData[devUnitIdx].MState.line [lineno].line_client,
              (unsigned char *) "The FNP has been restarted\r\n");
          }
        if (fnpData.fnpUnitData[devUnitIdx].MState.line[lineno].service == service_3270)
          {
#ifdef FNP2_DEBUG
sim_printf ("3270 controller found at unit %u line %u\r\n", devUnitIdx, lineno);
#endif
// XXX assuming only single controller
            if (fnpData.ibm3270ctlr[ASSUME0].configured)
              {
                sim_warn ("Too many 3270 controllers configured");
              }
            else
              {
                memset (& fnpData.ibm3270ctlr[ASSUME0], 0, sizeof (struct ibm3270ctlr_s));
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
    uint devUnitIdx = get_ctlr_idx (iomUnitIdx, chan);
    struct fnpUnitData * fudp = & fnpData.fnpUnitData [devUnitIdx];

// 60132445 FEP Coupler EPS
// 2.2.1 Control Intercommunication
//
// "In Level 66 momory, at a location known to the coupler and
// to Level 6 software is a mailbox area consisting to an Overhead
// mailbox and 7 Channel mailboxes."

    bool ok = true;
    struct mailbox vol * mbxp = (struct mailbox vol *) & M [fudp -> mailboxAddress];

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
#ifdef THREADZ
        lock_libuv ();
#endif
        fnpcmdBootload (devUnitIdx);
#ifdef THREADZ
        unlock_libuv ();
#endif
        fudp -> fnpIsRunning = true;
      }
    else if (command == 071) // interrupt L6
      {
#ifdef THREADZ
        lock_libuv ();
#endif
        ok = interruptL66 (iomUnitIdx, chan) == 0;
#ifdef THREADZ
        unlock_libuv ();
#endif
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
        fnp_core_write (fudp -> mailboxAddress, 0, "fnp_iom_cmd clear dia_pcw");
        putbits36_1 (& bootloadStatus, 0, 1); // real_status = 1
        putbits36_3 (& bootloadStatus, 3, 0); // major_status = BOOTLOAD_OK;
        putbits36_8 (& bootloadStatus, 9, 0); // substatus = BOOTLOAD_OK;
        putbits36_17 (& bootloadStatus, 17, 0); // channel_no = 0;
        fnp_core_write (fudp -> mailboxAddress + 6, bootloadStatus, "fnp_iom_cmd set bootload status");
      }
    else
      {
        dmpmbx (fudp->mailboxAddress);
// 3 error bit (1) unaligned, /* set to "1"b if error on connect */
        putbits36_1 (& dia_pcw, 18, 1); // set bit 18
        fnp_core_write (fudp -> mailboxAddress, dia_pcw, "fnp_iom_cmd set error bit");
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
            sim_debug (DBG_NOTIFY, & fnp_dev, "Request status\n");
          }
          break;

        default:
          {
            p -> stati = 04501;
            sim_debug (DBG_ERR, & fnp_dev,
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
 * fnp_iom_cmd()
 *
 */

// 1 ignored command
// 0 ok
// -1 problem

int fnp_iom_cmd (uint iomUnitIdx, uint chan)
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

