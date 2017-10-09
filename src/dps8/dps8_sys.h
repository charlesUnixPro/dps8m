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

// Statistics
typedef struct {
    uint64 total_cycles;      // Used for statistics and for simulated clock
    uint64 total_faults [N_FAULTS];
} stats_t;


#ifndef SCUMEM
extern word36 volatile *M;
#endif
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
t_stat brkbrk (int32 arg, const char * buf);

