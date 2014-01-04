//
//  dps8_disk.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>

#include "dps8.h"
#include "dps8_iom.h"
#include "dps8_disk.h"

// XXX We use this where we assume there is only one unit
#define ASSUME0 0

/*
 disk.c -- disk drives
 
 This is just a sketch; the emulator does not yet handle disks.
 
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

#define N_DISK_UNITS 1
// extern t_stat disk_svc(UNIT *up);
UNIT disk_unit [N_DISK_UNITS] = {{
    UDATA (&channel_svc, UNIT_FIX | UNIT_ATTABLE | UNIT_ROABLE | UNIT_DISABLE | UNIT_IDLE, M3381_SECTORS)
}};

// No disks known to multics had more than 2^24 sectors...
DEVICE disk_dev = {
    "DISK",       /*  name */
    disk_unit,    /* units */
    NULL,         /* registers */
    NULL,         /* modifiers */
    N_DISK_UNITS, /* #units */
    10,           /* address radix */
    24,           /* address width */
    1,            /* address increment */
    8,            /* data radix */
    36,           /* data width */
    NULL,         /* examine */
    NULL,         /* deposit */ 
    NULL,         /* reset */
    NULL,         /* boot */
    NULL,         /* attach */
    NULL,         /* detach */
    NULL,         /* context */
    DEV_DEBUG,    /* flags */
    0,            /* debug control flags */
    0,            /* debug flag names */
    NULL,         /* memory size change */
    NULL          /* logical name */
};



/*
 * disk_init()
 *
 */

void disk_init(void)
{
    // Nothing needed
}

/*
 * disk_iom_cmd()
 *
 */

int disk_iom_cmd(chan_devinfo* devinfop)
{
    int iom_unit_num = devinfop -> iom_unit_num;
    int chan = devinfop->chan;
    int dev_cmd = devinfop->dev_cmd;
    int dev_code = devinfop->dev_code;
    int* majorp = &devinfop->major;
    int* subp = &devinfop->substatus;
    
    sim_debug(DBG_DEBUG, & disk_dev, "disk_iom_cmd: IOM %c, Chan 0%o, dev-cmd 0%o, dev-code 0%o\n",
            'A' + iom_unit_num, chan, dev_cmd, dev_code);
    
    devinfop->is_read = 1;  // FIXME
    devinfop->time = -1;
    
    // Major codes are 4 bits...
    
    if (chan < 0 || chan >= max_channels) {
        devinfop->have_status = 1;
        *majorp = 05;   // Real HW could not be on bad channel
        *subp = 2;
        sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Bad channel %d\n", chan);
        cancel_run(STOP_BUG);
        return 1;
    }
    
    int dev_unit_num;
    DEVICE* devp = get_iom_channel_dev (iom_unit_num, chan, ASSUME0, & dev_unit_num);
    if (devp == NULL || devp->units == NULL) {
        devinfop->have_status = 1;
        *majorp = 05;
        *subp = 2;
        sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Internal error, no device and/or unit for channel 0%o\n", chan);
        cancel_run(STOP_BUG);
        return 1;
    }
// XXX bogus check, dev_code is not a unit number
#if 0
    if (dev_code < 0 || dev_code >= devp->numunits) {
        devinfop->have_status = 1;
        *majorp = 05;   // Command Reject
        *subp = 2;      // Invalid Device Code
        sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Bad dev unit-num 0%o (%d decimal)\n", dev_code, dev_code);
        cancel_run(STOP_BUG);
        return 1;
    }
#endif

#ifndef QUIET_UNUSED
    UNIT* unitp = &devp->units[dev_unit_num];
#endif
    
    // TODO: handle cmd etc for given unit
    
    switch(dev_cmd) {
            // idcw.command values:
            //  000 request status -- from disk_init
            //  022 read status register -- from disk_init
            //  023 read ascii
            //  025 read -- disk_control.list
            //  030 seek512 -- disk_control.list
            //  031 write -- disk_control.list
            //  033 write ascii
            //  042 restore access arm -- from disk_init
            //  051 write alert
            //  057 maybe read id
            //  072 unload -- disk_control.list
        case 040:       // CMD 40 -- Reset Status
            sim_debug(DBG_NOTIFY, & disk_dev, "disk_iom_cmd: Reset Status.\n");
            *majorp = 0;
            *subp = 0;
            //
            //devinfop->time = -1;
            //devinfop->have_status = 1;
            //
            devinfop->time = 4;
            //devinfop->time = 10000;
            devinfop->have_status = 0;
            //
            return 0;
        default: {
            sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: DISK devices not implemented.\n");
            devinfop->have_status = 1;
            *majorp = 05;       // Command reject
            *subp = 1;          // invalid opcode
            sim_debug(DBG_ERR, & disk_dev, "disk_iom_cmd: Unknown command 0%o\n", dev_cmd);
            cancel_run(STOP_BUG);
            return 1;
        }
    }
    return 1;   // not reached
}

// ============================================================================


int disk_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp)
{
    // sim_debug(DBG_DEBUG, & disk_dev, "disk_iom_io: Chan 0%o\n", chan);
    
    if (chan < 0 || chan >= max_channels) {
        *majorp = 05;   // Real HW could not be on bad channel
        *subp = 2;
        sim_debug(DBG_ERR, & disk_dev, "disk_iom_io: Bad channel %d\n", chan);
        return 1;
    }
    
    int dev_unit_num;
    DEVICE* devp = get_iom_channel_dev (ASSUME0, chan, ASSUME0, & dev_unit_num);
    if (devp == NULL || devp->units == NULL) {
        *majorp = 05;
        *subp = 2;
        sim_debug(DBG_ERR, & disk_dev, "disk_iom_io: Internal error, no device and/or unit for channel 0%o\n", chan);
        return 1;
    }
#ifndef QUIET_UNUSED
    UNIT* unitp = devp->units[dev_unit_num];
#endif
    // BUG: no dev_code
    
    *majorp = 013;  // MPC Device Data Alert
    *subp = 02;     // Inconsistent command
    sim_debug(DBG_ERR, & disk_dev, "disk_iom_io: Unimplemented.\n");
    cancel_run(STOP_BUG);
    return 1;
}
