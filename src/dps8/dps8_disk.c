//
//  dps8_disk.c
//  dps8
//
//  Created by Harry Reed on 6/16/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>

#include "dps8.h"

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

//#include "hw6180.h"
//#include "bitstream.h"

extern iom_t iom;

/*
 * disk_init()
 *
 */

void disk_init()
{
    // Nothing needed
}

/*
 * disk_iom_cmd()
 *
 */

int disk_iom_cmd(chan_devinfo* devinfop)
{
    const char* moi = "DISK::iom_cmd";
    
    int chan = devinfop->chan;
    int dev_cmd = devinfop->dev_cmd;
    int dev_code = devinfop->dev_code;
    int* majorp = &devinfop->major;
    int* subp = &devinfop->substatus;
    
    log_msg(DEBUG_MSG, moi, "Chan 0%o, dev-cmd 0%o, dev-code 0%o\n",
            chan, dev_cmd, dev_code);
    
    devinfop->is_read = 1;  // FIXME
    devinfop->time = -1;
    
    // Major codes are 4 bits...
    
    if (chan < 0 || chan >= ARRAY_SIZE(iom.channels)) {
        devinfop->have_status = 1;
        *majorp = 05;   // Real HW could not be on bad channel
        *subp = 2;
        log_msg(ERR_MSG, moi, "Bad channel %d\n", chan);
        cancel_run(STOP_BUG);
        return 1;
    }
    
    DEVICE* devp = iom.channels[chan].dev;
    if (devp == NULL || devp->units == NULL) {
        devinfop->have_status = 1;
        *majorp = 05;
        *subp = 2;
        log_msg(ERR_MSG, moi, "Internal error, no device and/or unit for channel 0%o\n", chan);
        cancel_run(STOP_BUG);
        return 1;
    }
    if (dev_code < 0 || dev_code >= devp->numunits) {
        devinfop->have_status = 1;
        *majorp = 05;   // Command Reject
        *subp = 2;      // Invalid Device Code
        log_msg(ERR_MSG, moi, "Bad dev unit-num 0%o (%d decimal)\n", dev_code, dev_code);
        cancel_run(STOP_BUG);
        return 1;
    }
    UNIT* unitp = &devp->units[dev_code];
    
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
            log_msg(NOTIFY_MSG, moi, "Reset Status.\n");
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
            log_msg(ERR_MSG, moi, "DISK devices not implemented.\n");
            devinfop->have_status = 1;
            *majorp = 05;       // Command reject
            *subp = 1;          // invalid opcode
            log_msg(ERR_MSG, moi, "Unknown command 0%o\n", dev_cmd);
            cancel_run(STOP_BUG);
            return 1;
        }
    }
    return 1;   // not reached
}

// ============================================================================


int disk_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp)
{
    const char* moi = "DISK::iom_io";
    // log_msg(DEBUG_MSG, moi, "Chan 0%o\n", chan);
    
    if (chan < 0 || chan >= ARRAY_SIZE(iom.channels)) {
        *majorp = 05;   // Real HW could not be on bad channel
        *subp = 2;
        log_msg(ERR_MSG, moi, "Bad channel %d\n", chan);
        return 1;
    }
    
    DEVICE* devp = iom.channels[chan].dev;
    if (devp == NULL || devp->units == NULL) {
        *majorp = 05;
        *subp = 2;
        log_msg(ERR_MSG, moi, "Internal error, no device and/or unit for channel 0%o\n", chan);
        return 1;
    }
    UNIT* unitp = devp->units;
    // BUG: no dev_code
    
    *majorp = 013;  // MPC Device Data Alert
    *subp = 02;     // Inconsistent command
    log_msg(ERR_MSG, moi, "Unimplemented.\n");
    cancel_run(STOP_BUG);
    return 1;
}
