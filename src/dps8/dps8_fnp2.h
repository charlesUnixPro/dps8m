/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//#ifdef __MINGW64__
//#ifndef SRWLOCK
//typedef PVOID RTL_SRWLOCK;
//typedef RTL_SRWLOCK SRWLOCK, *PSRWLOCK;
//#endif 
//#endif
#include <uv.h>
#include "libtelnet.h"

#define encodeline(fnp,line) ((fnp) * MAX_LINES + (line))
#define decodefnp(coded) ((coded) / MAX_LINES)
#define decodeline(coded) ((coded) % MAX_LINES)
#define noassoc -1

extern UNIT fnp_unit [N_FNP_UNITS_MAX];
extern DEVICE fnp_dev;

#define MAX_LINES  96  /*  max number of FNP lines - hardware  */

//
// MState_t state of an FNP
// 

// memset(0) sets service to serivce_undefined (0)
enum service_types {service_undefined = 0, service_login, service_3270, service_autocall, service_slave};

typedef struct t_MState
  {
    t_bool accept_calls;
    // 60132445 FEP Coupler Spec Nov77 - Unknown.pdf
    // pg 59 (sheet 56):
    //   bit       0: CS BAR
    //             1: BT INH
    //        2 -  7: RFU
    //        8 - 15: Special L6 Intpr Level
    word16 configRegA;
    struct t_line
      {
        // From the CMF database
        enum service_types service;

        // libuv hook
        // For non-multiplexed lines, the connection to the remote is stored here; 
        // For multiplexed lines (3270), the connection to the currenty selected station is stored here. Used by wtx.
        uv_tcp_t * line_client;

        // libtelnet hook
        bool was_CR;

        // State as set by FNP commands
        t_bool listen;
        uint inputBufferSize;
        uint ctrlStrIdx;
        t_bool breakAll;
        t_bool handleQuit;
        t_bool fullDuplex;
        t_bool echoPlex;    // echoes all characters types on the terminal
        t_bool crecho;      // echos a CR when a LF is typed
        t_bool lfecho;      // echos and inserts  a LF in the users input stream when a CR is typed
        t_bool tabecho;     // echos the appropriate number of spaces when a TAB is typed
        t_bool replay;
        t_bool polite;
        t_bool prefixnl;
        t_bool eight_bit_out;
        t_bool eight_bit_in;
        t_bool odd_parity;
        t_bool output_flow_control;
        t_bool input_flow_control;
        uint block_xfer_in_frame_sz, block_xfer_out_frame_sz;
        uint delay_table [6];
#define FC_STR_SZ 4
        uint inputSuspendLen;
        unsigned char inputSuspendStr [4];
        uint inputResumeLen;
        unsigned char inputResumeStr [4];
        uint outputSuspendLen;
        unsigned char outputSuspendStr [4];
        uint outputResumeLen;
        unsigned char outputResumeStr [4];
        uint frame_begin;
        uint frame_end;
        bool echnego [256];
        uint echnego_len;
        uint sync_msg_size;
        // Pending requests
        bool line_break;
#ifdef FNPDBG
#define SEND_OUTPUT_DELAY 100
#else
#define SEND_OUTPUT_DELAY 2
#endif
        uint send_output;
        bool accept_new_terminal;
#ifdef DISC_DELAY
        uint line_disconnected;
#else
        bool line_disconnected;
#endif
        bool ack_echnego_init;
        bool acu_dial_failure;
        bool sendLineStatus;
        bool wru_timeout;
        uint accept_input; // If non-zero, the number of centiseconds until
                          // an accept_input message should be sent; this is
                          // deal with 'reject_request' retries.
        bool waitForMbxDone; // If set, the line has sent input to the CS, 
                             // but the CS has not completed the mbx transaction;
                             // in order to prevent input data reordering, serialize
                             // the commands by waiting for this to clear before
                             // sending the next input.
        bool input_reply_pending;
        // Part of 'accept_input'
        bool input_break;

        // Buffer being assembled for sending to Multics
        unsigned char buffer[1024];   // line buffer for initial device selection and line discipline
        int nPos;           // position where *next* user input is to be stored

        // Incoming data from the connection
        unsigned char * inBuffer;
        uint inSize; // Number of bytes in inBuffer
        uint inUsed; // Number of consumed bytes in buffer


        // Dialout hooks
        uv_connect_t doConnect; 

        // Slave hooks
        uv_tcp_t server;
        int port;

#ifdef TUN
        // TUN hook
        bool is_tun;
        int tun_fd;
        bool in_frame;
        uint8_t frame [2+1500];
        uint frameLen;
#endif

        word9 lineType;
        word36 lineStatus0, lineStatus1;
        bool sendEOT;
      } line [MAX_LINES];
  } t_MState;

// for now, one controller

#define IBM3270_CONTROLLERS_MAX 1
#define IBM3270_STATIONS_MAX 32

struct ibm3270ctlr_s
  {
    bool configured;
    uint fnpno;
    uint lineno;
    // polling and selection addresses

    unsigned char pollCtlrChar;
    unsigned char pollDevChar;
    unsigned char selCtlrChar;
    unsigned char selDevChar;
    bool sending_stn_in_buffer;
    uint stn_no;
    struct station_s
      {
        uv_tcp_t * client;
        bool EORReceived;
        bool hdr_sent;
        unsigned char * stn_in_buffer;
        uint stn_in_size; // Number of bytes in inBuffer
        uint stn_in_used;
        //uint stn_in_used; // Number of consumed bytes in buffer
      } stations [IBM3270_STATIONS_MAX];
    // Although this is nominally a per/station event, Multics will not
    // resume polling until after the write is complete, so only
    // one event would be pending at any time; moving it out of the
    // 'stations' structure makes it easier for the emulator event
    // loops to see.
    bool write_complete;
  };

#define MAX_DEV_NAME_LEN 64

// Indexed by sim unit number
struct fnpUnitData_s
  {
    uint mailboxAddress;
    bool fnpIsRunning;
    bool fnpMBXinUse [4];  // 4 FNP submailboxes
    bool lineWaiting [4]; // If set, fnpMBXlineno is waiting for the mailbox to be marked clear.
    int fnpMBXlineno [4]; // Which HSLA line is using the mbx
    char ipcName [MAX_DEV_NAME_LEN];

    t_MState MState;
  };

typedef struct s_fnpData
  {
    struct fnpUnitData_s fnpUnitData [N_FNP_UNITS_MAX];
    struct ibm3270ctlr_s ibm3270ctlr [IBM3270_CONTROLLERS_MAX];
    char * telnet_address;
    int telnet_port;
    int telnet3270_port;
    uv_loop_t * loop;
    uv_tcp_t du_server;
    bool du_server_inited;
    uv_tcp_t du3270_server;
    bool du3270_server_inited;
    int du3270_poll;
  } t_fnpData;

extern t_fnpData fnpData;

// dn355_mailbox.incl.pl1 
//   input_sub_mbx
//       pad1:8, line_number:10, n_free_buffers:18
//       n_chars:18, op_code:9, io_cmd:9
//       n_buffers 
//       { abs_addr:24, tally:12 } [24]
//       command_data

struct input_sub_mbx
  {
    word36 word1; // dn355_no; is_hsla; la_no; slot_no    // 0      word0
    word36 word2; // cmd_data_len; op_code; io_cmd        // 1      word1
    word36 n_buffers;
    word36 dcws [24];
    word36 command_data;
  };


extern const unsigned char a2e [256];
extern const unsigned char e2a [256];
#define ADDR_MAP_ENTRIES 32
// map station number to selDevChar
// addr_map [stn_no] == selDevChar
extern const unsigned char addr_map [ADDR_MAP_ENTRIES];

void fnpInit(void);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
int lookupFnpLink (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (int fnpUnitNum, char *arg3);
void fnpToCpuQueueMsg (int fnpUnitNum, char * msg);
int fnp_iom_cmd (uint iomUnitIdx, uint chan);
t_stat set_fnp_server_port (int32 arg, const char * buf);
t_stat set_fnp_server_address (int32 arg, const char * buf);
t_stat set_fnp_3270_server_port (int32 arg, const char * buf);
t_stat fnp_start (UNUSED int32 arg, UNUSED const char * buf);
void fnpConnectPrompt (uv_tcp_t * client);
void fnp3270ConnectPrompt (uv_tcp_t * client);
void processUserInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread);
void processLineInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread);
void fnpRecvEOR (uv_tcp_t * client);
void process3270Input (uv_tcp_t * client, unsigned char * buf, ssize_t nread);
void set_3270_write_complete (uv_tcp_t * client);
void startFNPListener (void);
void setTIMW (uint iom_unit_idx, uint mailboxAddress, int mbx);
void reset_line (struct t_line * linep);
#ifdef SCUMEM
uint get_scu_unit_idx_iom (uint fnp_unit_idx, word24 addr, word24 * offset);
static inline void * fnp_M_addr (int fnp_unit_idx, uint addr)
  {
    word24 offset;
    uint scuUnitIdx = get_scu_unit_idx_iom ((uint) fnp_unit_idx, addr, & offset);
    return & scu[scuUnitIdx].M[offset];
  }
#else
static inline vol word36 * fnp_M_addr (UNUSED int fnp_unit_idx, uint addr)
  {
    return & M[addr];
  }
#endif

#ifdef LOCKLESS

static inline void fnp_core_read_lock (UNUSED int fnp_unit_idx, vol word36 *M_addr, word36 *data, UNUSED const char * ctx)
  {
    word24 addr = (word24)(M_addr - M);
    LOCK_CORE_WORD(addr);
    word36 v;
    LOAD_ACQ_CORE_WORD(v, addr);
    * data = v & DMASK;
  }

static inline void fnp_core_write (UNUSED int fnp_unit_idx, vol word36 *M_addr, word36 data, UNUSED const char * ctx)
  {
    word24 addr = (word24)(M_addr - M);
    LOCK_CORE_WORD(addr);
    STORE_REL_CORE_WORD(addr, data);
  }

static inline void fnp_core_write_unlock (UNUSED int fnp_unit_idx, vol word36 *M_addr, word36 data, UNUSED const char * ctx)
  {
    word24 addr = (word24)(M_addr - M);
    STORE_REL_CORE_WORD(addr, data);
  }
#else // LOCKLESS
static inline void fnp_core_read_lock (UNUSED int fnp_unit_idx, vol word36 *M_addr, word36 *data, UNUSED const char * ctx)
  {
    *data = *M_addr & DMASK;
  }

static inline void fnp_core_write (UNUSED int fnp_unit_idx, vol word36 *M_addr, word36 data, UNUSED const char * ctx)
  {
    *M_addr = data & DMASK;
  }

#define fnp_core_write_unlock fnp_core_write

#endif // LOCKLESS
