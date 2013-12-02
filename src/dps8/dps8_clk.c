
#include "dps8.h"

#define N_CLK_UNITS 1
static t_stat clk_svc(UNIT *up);
UNIT TR_clk_unit [N_CLK_UNITS] = {{ UDATA(&clk_svc, UNIT_IDLE, 0) }};

static DEBTAB clk_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
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
    NULL,        /* reset routine */
    NULL,        /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    DEV_DEBUG,   /* flags */
    0,           /* debug control flags */
    clk_dt,      /* debug flag names */
    NULL,        /* memory size change */
    NULL         /* logical name */
};


static t_stat clk_svc(UNIT *up)
{
    // only valid for TR
    (void) sim_rtcn_calb (CLK_TR_HZ, TR_CLK);   // calibrate clock
    uint32 t = sim_is_active(&TR_clk_unit[0]);
    sim_debug (DBG_INFO, & clk_dev, "clk_svc: TR has %d time units left\n", t);
    return 0;
}

#define reg_TR rTR

#ifndef QUIET_UNUSED
static int activate_timer (void)
{
    uint32 t;
    sim_debug (DBG_DEBUG, & clk_dev, "clk_svc: TR has %d time units left\n", t);
    sim_debug (DBG_DEBUG, & clk_dev, "activate_timer: TR is %lld %#llo.\n", reg_TR, reg_TR);
    if (bit_is_neg(reg_TR, 27)) {
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
    
    (void) sim_rtcn_init(CLK_TR_HZ, TR_CLK);
    sim_activate(&TR_clk_unit[0], reg_TR);
    if ((t = sim_is_active(&TR_clk_unit[0])) == 0)
        sim_debug (DBG_DEBUG, & TR_clk_unit, "activate_timer: TR is not running\n", t);
    else
        sim_debug (DBG_DEBUG, & TR_clk_unit, "activate_timer: TR is now running with %d time units left.\n", t);
    return 0;
}
#endif

#ifdef QUIET_UNUSED
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

