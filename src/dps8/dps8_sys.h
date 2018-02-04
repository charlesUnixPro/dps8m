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

#include <uv.h>
#include "uvutil.h"

enum http_state_t 
  {
    hsInitial, // waiting for request
    hsFields, // reading fields
  };

enum http_request_t
  {
    hrNone,
    hrGet,
  };

// System-wide info and options not tied to a specific CPU, IOM, or SCU
typedef struct
  {
    // Delay times are in cycles; negative for immediate
    struct
      {
        int connect;    // Delay between CIOC instr & connect channel operation
        //int chan_activate;  // Time for a list service to send a DCW
        //int boot_time; // delay between CPU start and IOM starting boot process
        //int terminate_time; // delay between CPU start and IOM starting boot process
      } iom_times;
    // struct {
        // int read;
        // int xfer;
    // } mt_times;
    // bool warn_uninit; // Warn when reading uninitialized memory

#define MR_BUFFER_SZ 4096
    char mr_buffer[MR_BUFFER_SZ];
    int mr_buffer_cnt;

    uv_access machine_room_access;

// http parser

    enum http_state_t httpState;
    enum http_request_t httpRequest;
    char http_get_URI[MR_BUFFER_SZ];

    bool no_color;

    uint sys_poll_interval; // Polling interval in milliseconds
    uint sys_slow_poll_interval; // Polling interval in polling intervals
    uint sys_poll_check_rate; // Check for pooling interval rate in CPU cycles
} sysinfo_t;

#ifndef SCUMEM
extern word36 vol * M;
#endif
extern sysinfo_t sys_opts;
extern uint64 sim_deb_start;
extern uint64 sim_deb_stop;
extern uint64 sim_deb_break;
#define NO_SUCH_SEGNO ((uint64) -1ll)
#define NO_SUCH_RINGNO ((uint64) -1ll)
extern uint64 sim_deb_segno;
extern uint64 sim_deb_ringno;
extern uint64 sim_deb_skip_limit;
extern uint64 sim_deb_mme_cntdwn;
extern uint64 sim_deb_skip_cnt;
extern bool sim_deb_bar;
extern DEVICE *sim_devices[];
extern uint dbgCPUMask;
extern bool breakEnable;

char * lookup_address (word18 segno, word18 offset, char * * compname, word18 * compoffset);
void list_source (char * compname, word18 offset, uint dflag);
//t_stat computeAbsAddrN (word24 * absAddr, int segno, uint offset);

t_stat brkbrk (int32 arg, const char * buf);
void start_machine_room (void);
void machine_room_process (void);

