//
//  fnp_ipc.h
//  fnp
//
//  Created by Harry Reed on 12/2/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//

#ifndef __fnp__fnp_ipc__
#define __fnp__fnp_ipc__

#include <stdio.h>


#include "sim_defs.h"

#include "zyre.h"


#define IPC_GROUP    "MulticsIPC"       // name of zyre group

extern int32 ipc_enable, ipc_verbose, ipc_trace;
extern DEVICE ipc_dev;

#define DBG_IPCTRACE        1
#define DBG_IPCVERBOSE      2

enum enum_ipc_funcs
{
    ipcUnknown = 0, // unknown IPC state
    ipcEnable,      // local IPC enable
    ipcDisable,     // local IPC disable
    ipcEnter,       // another peer has ENTERed the IPC group
    ipcExit,        // a peer has EXITed the IPC group
    ipcShoutRx,     // we've received a SHOUT (broadcast) message
    ipcShoutTx,     // we're SHOUTing (broadcasting) a IPC message to all connected peers
    ipcWhisperRx,   // we've received a WHISPER (peer-to-peer) messsage
    ipcWhisperTx,   // we're WHISPERing a IPC message to a peer
    ipcTest,        // perform IPC test mode
};
typedef enum enum_ipc_funcs ipc_funcs;

t_stat  ipc (ipc_funcs, char *arg1, char *arg2, char *arg3, int32 arg4);

#ifdef VM_FNP
#define ipc_printf(...) sim_printf (__VA_ARGS__)
#endif
#ifdef VM_DPS8
#define ipc_printf(...) sim_debug (DBG_IPCVERBOSE, &ipc_dev, __VA_ARGS__)
#endif

#endif /* defined(__fnp__fnp_ipc__) */
