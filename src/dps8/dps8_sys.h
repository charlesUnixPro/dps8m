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

// System-wide info and options not tied to a specific CPU, IOM, or SCU
typedef struct {
    int clock_speed;
    // Instructions rccl and rscr allow access to a hardware clock.
    // If zero, the hardware clock returns the real time of day.
    // If non-zero, the clock starts at an arbitrary date and ticks at
    // a rate approximately equal to the given number of instructions
    // per second.
    // Delay times are in cycles; negative for immediate
    struct {
        int connect;    // Delay between CIOC instr & connect channel operation
        int chan_activate;  // Time for a list service to send a DCW
        int boot_time; // delay between CPU start and IOM starting boot process
        int terminate_time; // delay between CPU start and IOM starting boot process
    } iom_times;
    struct {
        int read;
        int xfer;
    } mt_times;
    bool warn_uninit; // Warn when reading uninitialized memory
} sysinfo_t;

// Statistics
typedef struct {
    struct {
        uint nexec;
        uint nmsec; // FIXME: WARNING: if 32 bits, only good for ~47 days :-)
    } instr[1024];
    uint64 total_cycles;      // Used for statistics and for simulated clock
    uint64 total_faults [N_FAULTS];
    uint64 total_instr;
    uint64 total_msec;
    uint n_instr;       // Reset to zero on each call to sim_instr()
} stats_t;


extern word36 *M;
extern stats_t sys_stats;
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

char * lookupAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset);
void listSource (char * compname, word18 offset, uint dflag);
//t_stat computeAbsAddrN (word24 * absAddr, int segno, uint offset);

t_stat brkbrk (int32 arg, const char * buf);
void scpProcessEvent (void);
t_stat scpCommand (UNUSED char *nodename, UNUSED char *id, char *arg3);

