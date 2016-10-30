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

#include "dps8.h"
#include "dps8_clk.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"

#define TR_CLK 1 /* SIMH allows clock ids 0..7 */

#ifdef USE_IDLE
#define CLK_TR_HZ (512*1024) // should be 512 kHz, but we'll use 512 Hz for now
#else
#define CLK_TR_HZ (512*1) // should be 512 kHz, but we'll use 512 Hz for now
#endif

#define N_CLK_UNITS 1
static t_stat clk_svc(UNIT *up);
UNIT TR_clk_unit [N_CLK_UNITS] = {{ UDATA(&clk_svc, UNIT_IDLE, 0), 0, 0, 0, 0, 0, NULL, NULL }};

static DEBTAB clk_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

DEVICE clk_dev = {
    "CLK",       /* name */
    TR_clk_unit,    /* units */
    NULL,        /* registers */
    NULL,        /* modifiers */
    N_CLK_UNITS, /* #units */
    10,          /* address radix */
    8,           /* address width */
    1,           /* address increment */
    8,           /* data radix */
    8,           /* data width */
    NULL,        /* examine routine */
    NULL,        /* deposit routine */
#ifdef USE_IDLE
    activate_timer, /* reset routine */
#else
    NULL,        /* reset routine */
#endif
    NULL,        /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    DEV_DEBUG,   /* flags */
    0,           /* debug control flags */
    clk_dt,      /* debug flag names */
    NULL,        /* memory size change */
    NULL,        /* logical name */
    NULL,        // help
    NULL,        // attach help
    NULL,        // help context
    NULL         // description
};


static t_stat clk_svc (UNUSED UNIT * up)
{
    // only valid for TR
#ifdef USE_IDLE
    sim_activate (& TR_clk_unit [0], sim_rtcn_init(CLK_TR_HZ, TR_CLK));
#else
    (void) sim_rtcn_calb (CLK_TR_HZ, TR_CLK);   // calibrate clock
#endif
    uint32 t = sim_is_active(&TR_clk_unit[0]);
    sim_debug (DBG_INFO, & clk_dev, "clk_svc: TR has %d time units left\n", t);
    return 0;
}

#ifndef QUIET_UNUSED
static int activate_timer (void)
{
    uint32 t;
    sim_debug (DBG_DEBUG, & clk_dev, "clk_svc: TR has %d time units left\n", t);
    sim_debug (DBG_DEBUG, & clk_dev, "activate_timer: TR is %lld %#llo.\n", rTR, rTR);
    if (bit_is_neg(rTR, 27)) {
        if ((t = sim_is_active(&TR_clk_unit[0])) != 0)
            sim_debug (DBG_DEBUG, & clk_dev, "activate_timer: TR cancelled with %d time units left.\n", t);
        else
            sim_debug (DBG_DEBUG, & clk_dev, "activate_timer: TR loaded with negative value, but it was alread stopped.\n", t);
        sim_cancel(&TR_clk_unit[0]);
        return 0;
    }
    if ((t = sim_is_active(&TR_clk_unit[0])) != 0) {
        sim_debug (DBG_DEBUG, & clk_dev, "activate_timer: TR was still running with %d time units left.\n", t);
        sim_cancel(&TR_clk_unit[0]);   // BUG: do we need to cancel?
    }
    
#ifdef USE_IDLE
    if (! sim_is_active (& TR_clk_unit [0]))
      sim_activate (& TR_clk_unit[ 0], sim_rtcn_init(CLK_TR_HZ, TR_CLK));
#else
    (void) sim_rtcn_init(CLK_TR_HZ, TR_CLK);
    sim_activate(&TR_clk_unit[0], rTR);
#endif
    if ((t = sim_is_active(&TR_clk_unit[0])) == 0)
        sim_debug (DBG_DEBUG, & TR_clk_unit, "activate_timer: TR is not running\n", t);
    else
        sim_debug (DBG_DEBUG, & TR_clk_unit, "activate_timer: TR is now running with %d time units left.\n", t);
    return 0;
}
#endif

#ifndef QUIET_UNUSED
t_stat XX_clk_svc(UNIT *up)
{
    // only valid for TR
#if 0
    tmr_poll = sim_rtcn_calb (clk_tps, TMR_CLK);            /* calibrate clock */
    sim_activate (&clk_unit, tmr_poll);                     /* reactivate unit */
    tmxr_poll = tmr_poll * TMXR_MULT;                       /* set mux poll */
    todr_reg = todr_reg + 1;                                /* incr TODR */
    if ((tmr_iccs & TMR_CSR_RUN) && tmr_use_100hz)          /* timer on, std intvl? */
        tmr_incr (TMR_INC);                                 /* do timer service */
    return 0;
#else
    return 2;
#endif
}
#endif

