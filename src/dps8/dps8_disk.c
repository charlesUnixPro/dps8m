//
//  dps8_disk.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

// source/library_dir_dir/system_library_1/source/bound_volume_rldr_ut_.s.archive/rdisk_.pl1

#include <stdio.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_disk.h"
#include "dps8_sys.h"
#include "dps8_utils.h"

//-- // XXX We use this where we assume there is only one unit
//-- #define ASSUME0 0
//-- 
/*
 disk.c -- disk drives
 
//--  This is just a sketch; the emulator does not yet handle disks.
 
 See manual AN87
 
 */

/*
 Copyright (c) 2007-2013 Michael Mondy
 
 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at http://example.org/project/LICENSE.
 */


#define M3381_SECTORS 6895616
// records per subdev: 74930 (127 * 590)
// number of sub-volumes: 3
// records per dev: 3 * 74930 = 224790
// cyl/sv: 590
// cyl: 1770 (3*590)
// rec/cyl 127
// tracks/cyl 15
// sector size: 512
// sectors: 451858
// data: 3367 MB, 3447808 KB, 6895616 sectors,
//  3530555392 bytes, 98070983 records?

#define N_DISK_UNITS_MAX 16
#define N_DISK_UNITS 1 // default

//-- // extern t_stat disk_svc(UNIT *up);


static t_stat disk_reset (DEVICE * dptr);
static t_stat disk_show_nunits (FILE *st, UNIT *uptr, int val, void *desc);
static t_stat disk_set_nunits (UNIT * uptr, int32 value, char * cptr, void * desc);
static int disk_iom_cmd (UNIT * unitp, pcw_t * pcwp, word12 * stati, bool * need_data, bool * is_read);
static int disk_iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati);

static UNIT disk_unit [N_DISK_UNITS_MAX] =
  {
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL},
    {UDATA (NULL, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS), 0, 0, 0, 0, 0, NULL, NULL}
  };

#define DISK_UNIT_NUM(uptr) ((uptr) - disk_unit)

static DEBTAB disk_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY },
    { "INFO", DBG_INFO },
    { "ERR", DBG_ERR },
    { "WARN", DBG_WARN },
    { "DEBUG", DBG_DEBUG },
    { "ALL", DBG_ALL }, // don't move as it messes up DBG message
    { NULL, 0 }
  };

#define UNIT_WATCH UNIT_V_UF

static MTAB disk_mod [] =
  {
    { UNIT_WATCH, 1, "WATCH", "WATCH", 0, 0, NULL, NULL },
    { UNIT_WATCH, 0, "NOWATCH", "NOWATCH", 0, 0, NULL, NULL },
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      disk_set_nunits, /* validation routine */
      disk_show_nunits, /* display routine */
      "Number of DISK units in the system", /* value descriptor */
      NULL // Help
    },
    { 0, 0, NULL, NULL, 0, 0, NULL, NULL }
  };


// No disks known to multics had more than 2^24 sectors...
DEVICE disk_dev = {
    "DISK",       /*  name */
    disk_unit,    /* units */
    NULL,         /* registers */
    disk_mod,     /* modifiers */
    N_DISK_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    disk_reset,   /* reset */
    NULL,         /* boot */
    attach_unit,  /* attach */
    detach_unit,  /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    disk_dt,      /* debug flag names */
    NULL,         /* memory size change */
    NULL,         /* logical name */
    NULL,         // help
    NULL,         // attach help
    NULL,         // attach context
    NULL          // description
};

static struct disk_state
  {
    enum { no_mode, seek512_mode, read_mode } io_mode;
  } disk_state [N_DISK_UNITS_MAX];

static struct
  {
    int iom_unit_num;
    int chan_num;
    int dev_code;
  } cables_from_ioms_to_disk [N_DISK_UNITS_MAX];


/*
 * disk_init()
 *
 */

// Once-only initialization

void disk_init (void)
  {
    memset (disk_state, 0, sizeof (disk_state));
    for (int i = 0; i < N_DISK_UNITS_MAX; i ++)
      cables_from_ioms_to_disk [i] . iom_unit_num = -1;
  }

static t_stat disk_reset (DEVICE * dptr)
  {
    for (uint i = 0; i < dptr -> numunits; i ++)
      {
        // sim_disk_reset (& disk_unit [i]);
        sim_cancel (& disk_unit [i]);
      }
    return SCPE_OK;
  }

t_stat cable_disk (int disk_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (disk_unit_num < 0 || disk_unit_num >= (int) disk_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_disk: disk_unit_num out of range <%d>\n", disk_unit_num);
        sim_printf ("cable_disk: disk_unit_num out of range <%d>\n", disk_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_disk [disk_unit_num] . iom_unit_num != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_disk: socket in use\n");
        sim_printf ("cable_disk: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_DISK, chan_type_PSI, disk_unit_num, & disk_dev, & disk_unit [disk_unit_num], disk_iom_cmd, disk_iom_io);
    if (rc)
      return rc;

    cables_from_ioms_to_disk [disk_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_disk [disk_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_disk [disk_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static int disk_iom_cmd (UNIT * unitp, pcw_t * pcwp, word12 * stati, bool * need_data, bool * is_read)
  {
// First call to disk to 20184:
//
// Connect channel LPW at 001410: [dcw=01412 ires=0 hrel=0 ae=0 nc=0 trun=1 srel=0 tally=01]
// Connect channel PCW at 001412: [dev-cmd=040, dev-code=00, ext=00, mask=0, ctrl=02, chan-cmd=02, chan-data=01, chan=013]
// Payload Channel 013 (11):
//     Channel 0:13 mbx
//     chanloc 001454
//     LPW at 001454: [dcw=0345162 ires=0 hrel=0 ae=0 nc=0 trun=0 srel=0 tally=00] [lbnd=00 size=00(0) idcw=00]
//     IDCW 0 at 345162 : I-DCW: [dev-cmd=040, dev-code=00, ext=00, mask=1, ctrl=00, chan-cmd=00, chan-data=00, chan=013]
//     -- control !=2
//     DDCW 1 at 345163: D-DCW: type=0(IOTD), addr=000000, cp=00, tally=00(0) tally-ctl=0
//     -- control !=2
// 40 is reset status

// second access:
//  pcw is reset status
//  idcw is 030 "execute device command (DLI)" according to AB87
//              SEEK_512 according to source/library_dir_dir/system_library_tools/source/bound_io_tools_.s.archive/exercise_disk.pl1

// seek command constrution

//  dcw.address = rel (addr (seek_data));


    * need_data = false;
    int disk_unit_num = DISK_UNIT_NUM (unitp);
    int iom_unit_num = cables_from_ioms_to_disk [disk_unit_num] . iom_unit_num;
    struct disk_state * disk_statep = & disk_state [disk_unit_num];

    sim_debug (DBG_DEBUG, & disk_dev, "%s: IOM %c, Chan 0%o, dev-cmd 0%o, dev-code 0%o\n",
            __func__, 'A' + iom_unit_num, pcwp -> chan, pcwp -> dev_cmd, pcwp -> dev_code);

    // XXX do right when write
    * is_read = true;

        // idcw.command values:
        //  000 request status -- from disk_init
        //  022 read status register -- from disk_init
        //  023 read ascii
        //  025 read -- disk_control.list
        //  030 seek512 -- disk_control.list, dctl.alm
        //  031 write -- disk_control.list
        //  033 write ascii
        //  042 restore access arm -- from disk_init
        //  051 write alert
        //  057 maybe read id
        //  072 unload -- disk_control.list

    switch (pcwp -> dev_cmd)
      {
        case 000: // CMD 00 REQUEST STATUS
          {
            * is_read = true; // XXX I don't really understand the semantics
                               // of is_read
            disk_statep -> io_mode = no_mode;
            * need_data = false;
            * stati = 04000;
            sim_debug (DBG_NOTIFY, & disk_dev, "request status\n");
            return 0;
          }


        case 025: // CMD 25 READ
          {
            * is_read = true; // XXX I don't really understand the semantics
                               // of is_read
            disk_statep -> io_mode = read_mode;
            * need_data = true;
            sim_debug (DBG_NOTIFY, & disk_dev, "request status\n");
            return 0;
          }


        case 030: // CMD 30 SEEK_512
          {
            * is_read = false; // XXX I don't really understand the semantics
                               // of is_read, but a seek is closer to a write
                               // then a read.
            disk_statep -> io_mode = seek512_mode;
            * need_data = true;
            * stati = 04000;
            sim_debug (DBG_NOTIFY, & disk_dev, "seek512\n");
            return 0;
          }

        case 040: // CMD 40 Reset status
          {
            * stati = 04000;
            disk_statep -> io_mode = no_mode;
            sim_debug (DBG_NOTIFY, & disk_dev, "Reset status\n");
            return 0;
          }

        default:
          {
            * stati = 04501;
            sim_debug (DBG_ERR, & disk_dev,
                       "%s: Unknown command 0%o\n", __func__, pcwp -> dev_cmd);
            return 1;
          }
      }
    // return 1;   // not reached
  }

static int disk_iom_io (UNIT * __attribute__((unused)) unitp, uint __attribute__((unused)) chan, uint __attribute__((unused)) dev_code, uint * __attribute__((unused)) tally, uint * __attribute__((unused)) cp, word36 * __attribute__((unused)) wordp, word12 * __attribute__((unused)) stati)
  {
sim_printf ("disk_iom_io called\n");
    //int disk_unit_num = DISK_UNIT_NUM (unitp);
    int disk_unit_num = DISK_UNIT_NUM (unitp);
    struct disk_state * disk_statep = & disk_state [disk_unit_num];
    if (disk_statep -> io_mode == seek512_mode)
      {
        sim_printf ("seek512_mode; tally %u\n", * tally);
        * stati = 04000; // ok
        return 0;
      }
    else if (disk_statep -> io_mode == read_mode)
      {
        sim_printf ("read_mode; tally %u\n", * tally);
        * stati = 04000; // ok
        return 0;
      }
    else
      {
        sim_printf ("disk_iom_io called w/mode %d\n", disk_statep -> io_mode);
        * stati = 05302; // MPC Device Data Alert Inconsistent command
        return 1;
      }
    // return 0; // not reached
  }

static t_stat disk_show_nunits (FILE * __attribute__((unused)) st, UNIT * __attribute__((unused)) uptr, int __attribute__((unused)) val, void * __attribute__((unused)) desc)
  {
    sim_printf("Number of DISK units in system is %d\n", disk_dev . numunits);
    return SCPE_OK;
  }

static t_stat disk_set_nunits (UNIT * __attribute__((unused)) uptr, int32 __attribute__((unused)) value, char * cptr, void * __attribute__((unused)) desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_DISK_UNITS_MAX)
      return SCPE_ARG;
    disk_dev . numunits = n;
    return SCPE_OK;
  }

//-- /*
//--  * disk_iom_cmd()
//--  *
//--  */
//-- 
//-- int disk_iom_cmd(chan_devinfo* devinfop)
//-- {
//--     int iom_unit_num = devinfop -> iom_unit_num;
//--     int chan = devinfop->chan;
//--     int dev_cmd = devinfop->dev_cmd;
//--     int dev_code = devinfop->dev_code;
//--     int* majorp = &devinfop->major;
//--     int* subp = &devinfop->substatus;
//--     
//--     sim_debug(DBG_DEBUG, & disk_dev, "disk_iom_cmd: IOM %c, Chan 0%o, dev-cmd 0%o, dev-code 0%o\n",
//--             'A' + iom_unit_num, chan, dev_cmd, dev_code);
//--     
//--     devinfop->is_read = 1;  // FIXME
//--     devinfop->time = -1;
//--     
//--     // Major codes are 4 bits...
//--     
//--     if (chan < 0 || chan >= max_channels) {
//--         devinfop->have_status = 1;
//--         *majorp = 05;   // Real HW could not be on bad channel
//--         *subp = 2;
//--         sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Bad channel %d\n", chan);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--     }
//--     
//--     int dev_unit_num;
//--     DEVICE* devp = get_iom_channel_dev (iom_unit_num, chan, ASSUME0, & dev_unit_num);
//--     if (devp == NULL || devp->units == NULL) {
//--         devinfop->have_status = 1;
//--         *majorp = 05;
//--         *subp = 2;
//--         sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Internal error, no device and/or unit for channel 0%o\n", chan);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--     }
//-- // XXX bogus check, dev_code is not a unit number
//-- #if 0
//--     if (dev_code < 0 || dev_code >= devp->numunits) {
//--         devinfop->have_status = 1;
//--         *majorp = 05;   // Command Reject
//--         *subp = 2;      // Invalid Device Code
//--         sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Bad dev unit-num 0%o (%d decimal)\n", dev_code, dev_code);
//--         cancel_run(STOP_BUG);
//--         return 1;
//--     }
//-- #endif
//-- 
//-- #ifndef QUIET_UNUSED
//--     UNIT* unitp = &devp->units[dev_unit_num];
//-- #endif
//--     
//--     // TODO: handle cmd etc for given unit
//--     
//--     switch(dev_cmd) {
//--             // idcw.command values:
//--             //  000 request status -- from disk_init
//--             //  022 read status register -- from disk_init
//--             //  023 read ascii
//--             //  025 read -- disk_control.list
//--             //  030 seek512 -- disk_control.list
//--             //  031 write -- disk_control.list
//--             //  033 write ascii
//--             //  042 restore access arm -- from disk_init
//--             //  051 write alert
//--             //  057 maybe read id
//--             //  072 unload -- disk_control.list
//--         case 040:       // CMD 40 -- Reset Status
//--             sim_debug(DBG_NOTIFY, & disk_dev, "disk_iom_cmd: Reset Status.\n");
//--             *majorp = 0;
//--             *subp = 0;
//--             //
//--             //devinfop->time = -1;
//--             //devinfop->have_status = 1;
//--             //
//--             devinfop->time = 4;
//--             //devinfop->time = 10000;
//--             devinfop->have_status = 0;
//--             //
//--             return 0;
//--         default: {
//--             sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: DISK devices not implemented.\n");
//--             devinfop->have_status = 1;
//--             *majorp = 05;       // Command reject
//--             *subp = 1;          // invalid opcode
//--             sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Unknown command 0%o\n", dev_cmd);
//--             cancel_run(STOP_BUG);
//--             return 1;
//--         }
//--     }
//--     return 1;   // not reached
//-- }
//-- 
//-- // ============================================================================
//-- 
//-- 
//-- int disk_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp)
//-- {
//--     // sim_debug(DBG_DEBUG, & disk_dev, "disk_iom_io: Chan 0%o\n", chan);
//--     
//--     if (chan < 0 || chan >= max_channels) {
//--         *majorp = 05;   // Real HW could not be on bad channel
//--         *subp = 2;
//--         sim_debug(DBG_ERR, & disk_dev, "disk_iom_io: Bad channel %d\n", chan);
//--         return 1;
//--     }
//--     
//--     int dev_unit_num;
//--     DEVICE* devp = get_iom_channel_dev (ASSUME0, chan, ASSUME0, & dev_unit_num);
//--     if (devp == NULL || devp->units == NULL) {
//--         *majorp = 05;
//--         *subp = 2;
//--         sim_debug(DBG_ERR, & disk_dev, "disk_iom_io: Internal error, no device and/or unit for channel 0%o\n", chan);
//--         return 1;
//--     }
//-- #ifndef QUIET_UNUSED
//--     UNIT* unitp = devp->units[dev_unit_num];
//-- #endif
//--     // BUG: no dev_code
//--     
//--     *majorp = 013;  // MPC Device Data Alert
//--     *subp = 02;     // Inconsistent command
//--     sim_debug(DBG_ERR, & disk_dev, "disk_iom_io: Unimplemented.\n");
//--     cancel_run(STOP_BUG);
//--     return 1;
//-- }
//-- 
//-- 
