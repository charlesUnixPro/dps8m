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
extern DEVICE fnpDev;
extern DEVICE mux_dev;

#define MAX_LINES  64  /*  max number of FNP lines - hardware  */

//
// MState_t state of an FNP
// 

// memset(0) sets service to serivce_undefined (0)
enum service_types {service_undefined = 0, service_login, service_autocall, service_slave};

typedef struct
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
        uv_tcp_t * client;

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
        // Pending requests
        bool line_break;
#ifdef FNPDBG
#define SEND_OUTPUT_DELAY 100
#else
#define SEND_OUTPUT_DELAY 2
#endif
        uint send_output;
        bool accept_new_terminal;
        bool line_disconnected;
        bool ack_echnego_init;
        bool acu_dial_failure;
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
#endif

      } line [MAX_LINES];
  } t_MState;

#define MAX_DEV_NAME_LEN 64

// Indexed by sim unit number
struct fnpUnitData
  {
//-    enum { no_mode, read_mode, write_mode, survey_mode } io_mode;
//-    uint8 * bufp;
//-    t_mtrlnt tbc; // Number of bytes read into buffer
//-    uint words_processed; // Number of Word36 processed from the buffer
//-    int rec_num; // track tape position
    uint mailboxAddress;
    bool fnpIsRunning;
    bool fnpMBXinUse [4];  // 4 FNP submailboxes
    bool lineWaiting [4]; // If set, fnpMBXlineno is waiting for the mailbox to be marked clear.
    int fnpMBXlineno [4]; // Which HSLA line is using the mbx
    char ipcName [MAX_DEV_NAME_LEN];

    t_MState MState;
  };

extern struct fnpUnitData fnpUnitData [N_FNP_UNITS_MAX];

void fnpInit(void);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
int lookupFnpLink (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (int fnpUnitNum, char *arg3);
void fnpToCpuQueueMsg (int fnpUnitNum, char * msg);
int fnpIOMCmd (uint iomUnitIdx, uint chan);
t_stat fnpServerPort (int32 arg, const char * buf);
t_stat fnpStart (UNUSED int32 arg, UNUSED const char * buf);
void fnpConnectPrompt (uv_tcp_t * client);
void processUserInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread);
void processLineInput (uv_tcp_t * client, unsigned char * buf, ssize_t nread);
#if 0
t_stat fnpLoad (UNUSED int32 arg, const char * buf);
#endif
void startFNPListener (void);
