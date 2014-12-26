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

#include "uthash.h"

#define IPC_GROUP    "MulticsIPC"       // default zyre group

#ifdef VM_FNP
#define IPC_NODE     "fnp"              // default node name
#endif
#ifdef VM_DPS8
#define IPC_NODE     "MulticsCS"        // default node name
#endif

extern int32 ipc_verbose, ipc_trace;


#define DBG_IPCTRACE        1
#define DBG_IPCVERBOSE      2

enum enum_ipc_funcs
{
    ipcUnknown = 0, // unknown IPC state
    ipcStart,       // local IPC start
    ipcStop,        // local IPC stop
    ipcRestart,     // local IPC restart
    ipcEnter,       // another peer has ENTERed the IPC group
    ipcExit,        // a peer has EXITed the IPC group
    ipcShoutRx,     // we've received a SHOUT (broadcast) message
    ipcShoutTx,     // we're SHOUTing (broadcasting) a IPC message to all connected peers
    ipcWhisperRx,   // we've received a WHISPER (peer-to-peer) messsage
    ipcWhisperTx,   // we're WHISPERing a IPC message to a peer
    ipcTest,        // perform IPC test mode
};
typedef enum enum_ipc_funcs ipc_funcs;

struct _IPC_peer {
    char *peerName;
    char *peerID;
    
    UT_hash_handle hh;
};
typedef struct _IPC_peer IPC_Peer;

extern IPC_Peer *Peers;

IPC_Peer *savePeer(char *name, char *id);
IPC_Peer *findPeer(char *name);
bool     removePeer(char *name);
int      deletePeers();

t_stat  ipc (ipc_funcs, char *arg1, char *arg2, char *arg3, int32 arg4);

bool isIPCRunning();


t_stat ipc_shout   (int32 arg, char *buf);
t_stat ipc_whisper (int32 arg, char *buf);

extern char fnpName[32];
extern char fnpGroup[32];

extern DEVICE ipc_dev;

#ifdef VM_FNP
#define ipc_printf(...) sim_printf (__VA_ARGS__)
#endif
#ifdef VM_DPS8
//#define ipc_printf(...) sim_debug (DBG_IPCVERBOSE, &ipc_dev, __VA_ARGS__)
#define ipc_printf(...) sim_printf (__VA_ARGS__)
#endif

#endif /* defined(__fnp__fnp_ipc__) */
