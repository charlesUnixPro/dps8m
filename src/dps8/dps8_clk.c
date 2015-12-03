#if 0
#include "dps8.h"
#include "dps8_clk.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"

#define IDLE_CLK 3
#define IDLE_CLK_HZ 50

static t_stat idle_clk_svc (UNIT * uptr);
static t_stat idle_clk_reset (DEVICE * dptr);

static UNIT idle_clk_unit = { UDATA (& idle_clk_svc, UNIT_IDLE, 50), 0, 0, 0, 0, 0, NULL, NULL };

static DEBTAB clk_dt [] =
  {
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

DEVICE idle_clk_dev =
  {
    "IDLECLK",
    & idle_clk_unit,
    NULL, // registers
    NULL, // modifiers
    1, // # units
    0, // address radix
    8, // address width
    4, // address increment
    0, // data radix
    32, // data width
    NULL, // examine routine
    NULL, // deposit routine
    & idle_clk_reset, // reset routine
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

static t_stat idle_clk_reset (DEVICE * dptr)
  {
    /* init timer */
    sim_rtcn_init (dptr->units-> wait, IDLE_CLK);
    /* activate unit */
    sim_activate_after (dptr->units, 1000000 / IDLE_CLK_HZ);
    sim_register_clock_unit (dptr->units);
    return SCPE_OK;
  }

static t_stat idle_clk_svc (UNIT * uptr)
  {
#if 0 // debug code
static time_t t0 = 0;
static int cnt = 0;
if (! t0) t0 = time (NULL);
time_t t = time (NULL);
if (t == t0)
  {
    cnt ++;
  }
else
  {
    sim_printf ("ping %d\n", cnt);
    t0 = t;
    cnt = 0;
  }
#endif

    sim_rtcn_calb (IDLE_CLK_HZ, IDLE_CLK); /* calibrate 50Hz clock */
    sim_activate_after (uptr, 1000000/ IDLE_CLK_HZ); /* reactivate unit */
    setPollingFlag ();
    if_sim_debug (DBG_DEBUG, & idle_clk_dev)
      {
        static uint ctr = 0;
        if (ctr ++ % 50 == 0)
          sim_printf ("tock\n");
      }
    return SCPE_OK;
  }
#endif
