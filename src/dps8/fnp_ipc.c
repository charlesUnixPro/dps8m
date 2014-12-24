//
//  fnp_ipc.c
//  fnp
//
//  Created by Harry Reed on 12/2/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//
#ifdef VM_FNP
#include "fnp_defs.h"

#include "fnp_mux.h"
#endif
#ifdef VM_DPS8
#include "sim_defs.h"
void sim_printf (const char * format, ...);
#endif

#include "fnp_ipc.h"

int32 ipc_enable = 0;
int32 ipc_verbose = 0;
int32 ipc_trace = 0;

t_stat ipc_svc (UNIT *uptr);
t_stat ipc_reset (DEVICE *dptr);
//t_stat ipc_attach(UNIT *unitp, char *cptr);
//t_stat ipc_detach(UNIT *unitp);

/* IPC data structures
 
 ipc_dev      IPC device descriptor
 ipc_unit     IPC unit descriptor
 ipc_reg      IPC register list
 */

#ifdef VM_FNP
DIB ipc_dib = { MUX_INT_CLK, MUX_INT_CLK, PI_CLK, &clk };
#endif

UNIT ipc_unit = { UDATA (&ipc_svc, UNIT_DISABLE, 0) };

//REG ipc_reg[] = {
//    { ORDATA (SELECT, clk_sel, 2) },
//    { FLDATA (BUSY, mux_busy, MUX_INT_V_CLK) },
//    { FLDATA (DONE, mux_done, MUX_INT_V_CLK) },
//    { FLDATA (DISABLE, ipc_enable, DEV_DIS) },
//    { FLDATA (VERBOSE, ipc_verbose, 0) },
//    { FLDATA (INT, mux_int_req, MUX_INT_V_CLK) },
//    { DRDATA (TIME0, clk_time[0], 24), REG_NZ + PV_LEFT },
//    { DRDATA (TIME1, clk_time[1], 24), REG_NZ + PV_LEFT },
//    { DRDATA (TIME2, clk_time[2], 24), REG_NZ + PV_LEFT },
//    { DRDATA (TIME3, clk_time[3], 24), REG_NZ + PV_LEFT },
//    { DRDATA (TPS0, clk_tps[0], 6), PV_LEFT + REG_HRO },
//    { NULL }
//};

//MTAB clk_mod[] = {
//    { MTAB_XTD|MTAB_VDV, 50, NULL, "50HZ",
//        &clk_set_freq, NULL, NULL },
//    { MTAB_XTD|MTAB_VDV, 60, NULL, "60HZ",
//        &clk_set_freq, NULL, NULL },
//    { MTAB_XTD|MTAB_VDV, 0, "LINE", NULL,
//        NULL, &clk_show_freq, NULL },
//    { 0 }
//};

static t_stat Test (FILE *st, UNIT *uptr, int32 val, void *desc);

MTAB ipc_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "TEST", NULL, NULL, &Test },
    { 0 }
};



DEBTAB ipc_dbg[] = {
    {"TRACE",   DBG_IPCTRACE  },
    {"VERBOSE", DBG_IPCVERBOSE},
    {0}
};


DEVICE ipc_dev = {
    "IPC",
    &ipc_unit,
    NULL,   //ipc_reg,
    ipc_mod,
    1, 0, 0, 0, 0, 0,
    NULL, NULL, &ipc_reset,
    NULL,
    NULL,   //&ipc_attach,// attach
    NULL,   //&ipc_detach,// detach
#ifdef VM_FNP
    &ipc_dib,
#else
    NULL,
#endif
    DEV_DIS | DEV_DISABLE | DEV_DEBUG,
    0,
    ipc_dbg
};

/*
 * perform IPC (zyre) self-test
 */
static
t_stat Test (FILE *st, UNIT *uptr, int32 val, void *desc)
{
    zyre_test(true);
    return SCPE_OK;
}

/* Unit service */

t_stat ipc_svc (UNIT *uptr)
{
    return SCPE_OK;
}

/* Reset routine */
t_stat ipc_reset (DEVICE *dptr)
{    
//    if ((dptr->flags & DEV_DIS) == 0)
//    {
//        if (dptr == &ipc_dev) ipc_unit.flags |= DEV_DIS;
//    }
    
    //sim_cancel (&ipc_unit);                                 /* deactivate unit */
    return SCPE_OK;
}

//t_stat ipc_attach(UNIT *unitp, char *cptr)
//{
//    /*  switches:   A       auto-disconnect
//     *              M       modem control
//     */
//    
//    if ( sim_switches & SWMASK('M') )                   /* modem control? */
//    {
//        sim_printf( "Modem control activated\n" ) ;
//        if ( sim_switches & SWMASK ('A') )              /* autodisconnect? */
//        {
//            sim_printf( "Auto disconnect activated\n" ) ;
//        }
//    }
//
//    sim_activate( unitp, mux_tmxr_poll ) ;
//    return ( SCPE_OK ) ;
//}
//
//t_stat ipc_detach( UNIT * unitp )
//{
//    sim_cancel( unitp ) ;
//    
//    return SCPE_OK;
//}

// zyre chat ...
#ifdef ZYRE_STUFF

//  --------------------------------------------------------------------------
//  Example Zyre distributed chat application
//
//  --------------------------------------------------------------------------
//  Copyright (c) 2010-2014 iMatix Corporation and Contributors
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
//  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//  --------------------------------------------------------------------------


//  This actor will listen and publish anything received
//  on the CHAT group

static void
chat_actor (zsock_t *pipe, void *args)
{
    //  Do some initialization
    char*   name = (char*) args;
    zyre_t *node = zyre_new (name);
    if (!node)
        return;                 //  Could not create new node
    //zyre_set_verbose (node);  // uncomment to watch the events
    zyre_start (node);
    zyre_join (node, "CHAT");
    zsock_signal (pipe, 0);     //  Signal "ready" to caller
    
    bool terminated = false;
    zpoller_t *poller = zpoller_new (pipe, zyre_socket (node), NULL);
    while (!terminated) {
        void *which = zpoller_wait (poller, -1); // no timeout
        if (which == pipe){
            zmsg_t *msg = zmsg_recv (which);
            if (!msg)
                break;              //  Interrupted
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM")) {
                terminated = true;
            }
            else
                if (streq (command, "SHOUT")) {
                    char *string = zmsg_popstr (msg);
                    zyre_shouts (node, "CHAT", "%s", string);
                }
                else {
                    puts ("E: invalid message to actor");
                    assert (false);
                }
            free (command);
            zmsg_destroy (&msg);
        }
        else if (which == zyre_socket (node)) {
            zmsg_t *msg = zmsg_recv (which);
            char *event = zmsg_popstr (msg);
            char *peer = zmsg_popstr (msg);
            char *name = zmsg_popstr (msg);
            char *group = zmsg_popstr (msg);
            char *message = zmsg_popstr (msg);
            
            if (streq (event, "ENTER")) {
                printf ("%s has joined the chat\n", name);
            }
            else if (streq (event, "EXIT")) {
                printf ("%s has left the chat\n", name);
            }
            if (streq (event, "SHOUT")) {
                printf ("%s: %s\n", name, message);
            }
            //printf ("Message from node\n");
            //printf ("event: %s peer: %s  name: %s\n  group: %s message: %s\n", event, peer, name, group, message);
            
            free (event);
            free (peer);
            free (name);
            free (group);
            free (message);
            zmsg_destroy (&msg);
        }
    }
    zpoller_destroy (&poller);
    
    // Notify peers that this peer is shutting down. Provide
    // a brief interval to ensure message is emitted.
    zyre_stop(node);
    zclock_sleep(100);
    
    zyre_destroy (&node);
}


int
mainC (int argc, char *argv[])
{
    if (argc < 2) {
        puts ("syntax: ./chat myname");
        exit (0);
    }
    zactor_t *actor = zactor_new (chat_actor, argv[1]);
    assert (actor);
    
    while (!zsys_interrupted) {
        char message [1024];
        if (!fgets( message, 1024, stdin))
            break;
        message[strlen(message)-1] = 0; // drop the trailing linefeed
        zstr_sendx (actor, "SHOUT", message, NULL);
    }
    
    zactor_destroy (&actor);
    
    return 0;
}
#endif
// End of ZMQ code ...

static zactor_t *actor = NULL;
static zyre_t *node = 0;
static zpoller_t *poller = 0;

static bool volatile terminated = false;

static void
ipc_actor (zsock_t *pipe, void *args)
{
    //  Do some initialization
    char* name = (char*) args;
    
    node = zyre_new (name);
    assert(node);
    if (!node)  //  Could not create new node
    {
        sim_printf("Couldn't create IPC node ... %s\n", name);
        return;
    }
    
    ipc_printf("Starting IPC node %s ...", name);

    if (ipc_verbose)
        zyre_set_verbose (node);  // uncomment to watch the events
    
    zyre_start (node);
    zyre_join (node, IPC_GROUP);
    zsock_signal (pipe, 0);     //  Signal "ready" to caller
    
    terminated = false;
    
    poller = zpoller_new (pipe, zyre_socket (node), NULL);
    assert(poller);
    
    ipc_printf(" done\n");

    while (!terminated)
    {
        void *which = zpoller_wait (poller, -1); // no timeout
        if (!terminated && which == pipe)
        {
            zmsg_t *msg = zmsg_recv (which);
            if (!msg)
                break;              //  Interrupted
            char *command = zmsg_popstr (msg);
            if (streq (command, "$TERM"))
            {
                terminated = true;
            }
            else
                if (streq (command, "SHOUT"))
                {
                    char *string = zmsg_popstr (msg);
                    zyre_shouts (node, IPC_GROUP, "%s", string);
                }
                else if (streq (command, "WHISPER"))
                {
                    char *string = zmsg_popstr (msg);
                    zyre_whispers (node, IPC_GROUP, "%s", string);
                } else
                {
                    sim_debug (DBG_IPCVERBOSE, &ipc_dev,"ipc_actor(): E: invalid message to actor");
                    //assert (false);
                }
            free (command);
            zmsg_destroy (&msg);
        }
        else if (!terminated && which == zyre_socket (node))
        {
            zmsg_t *msg = zmsg_recv (which);
            char *event = zmsg_popstr (msg);
            char *peer = zmsg_popstr (msg);
            char *name = zmsg_popstr (msg);
            char *group = zmsg_popstr (msg);
            char *message = zmsg_popstr (msg);  // change to zmsg_popmsg (zmsg_t *self) later
            
            if (streq (event, "ENTER"))
            {
                ipc(ipcEnter, name, peer, message, 0);
            }
            else if (streq (event, "EXIT"))
            {
                ipc(ipcExit, name, peer, 0, 0);
            }
            if (streq (event, "SHOUT"))
            {
                ipc(ipcShoutRx, name, peer, message, 0);
            }
            if (streq (event, "WHISPER"))
            {
                ipc(ipcWhisperRx, name, peer, group, 0);
            }

            if (ipc_verbose)
            {
                ipc_printf("Message from node\n");
                ipc_printf("event: %s peer: %s  name: %s group: %s message: %s\n", event, peer, name, group, message);
            }
            free (event);
            free (peer);
            free (name);
            free (group);
            free (message);
            zmsg_destroy (&msg);
        }
    }
}

void killIPC()
{
    terminated = true;                           // tell IPC actor to kill itself

    zactor_destroy (&actor);

    zpoller_destroy (&poller);
    
    // Notify peers that this peer is shutting down. Provide
    // a brief interval to ensure message is emitted.
    zyre_stop(node);
    zclock_sleep(100);
    
    zyre_destroy (&node);
    
    node = 0;
    poller = 0;
}

t_stat ipc (ipc_funcs fn, char *arg1, char *arg2, char *arg3, int32 arg4)
{
    switch (fn)
    {
        case ipcEnable:
            {
#ifdef VM_FNP
                int32 muxU = muxWhatUnitAttached();
                if (muxU == -1)
                    return SCPE_NOTATT;
            
                actor = zactor_new (ipc_actor, fnpNames[muxU]);
#endif
#ifdef VM_DPS8
                actor = zactor_new (ipc_actor, "MulticsCS");
#endif
                assert (actor);
            }
            break;
            
        case ipcDisable:
            ipc_printf("Stopping IPC ... ");
            killIPC();
            ipc_printf("done\n");
            break;
         
        case ipcEnter:
            //sim_debug (DBG_VERBOSE, &ipc_dev, "%s/%s has entered " STR(IPC_GROUP) "\n", arg1, arg2);
            ipc_printf("(ENTER)      %s/%s has entered " IPC_GROUP " from %s\n", arg1, arg2, arg3);
            break;
            
        case ipcExit:
            //sim_debug (DBG_VERBOSE, &ipc_dev, "%s has left " STR(IPC_GROUP) "\n", arg1);
            ipc_printf("(EXIT)       %s/%s has left " IPC_GROUP "\n", arg1, arg2);
            break;
            
        case ipcShoutTx:
            zstr_sendx (actor, "SHOUT", arg1, NULL);
            break;
            
        case ipcWhisperTx:
            zstr_sendx (actor, "WHISPER", arg1, NULL);
            break;
    
        case ipcShoutRx:
            //sim_debug (DBG_VERBOSE, &ipc_dev, "%s: %s\n", arg1, arg2);
            ipc_printf("(RX SHOUT)   %s/%s:<%s>\n", arg1, arg2, arg3);
            break;
        case ipcWhisperRx:
            //sim_debug (DBG_VERBOSE, &ipc_dev, "%s: %s\n", arg1, arg2);
            ipc_printf("(RX WHISPER) %s/%s:<%s>\n", arg1, arg2, arg3);
            break;

        case ipcTest:
            zyre_test(true);
            break;
            
        default:
            break;
    }
    return SCPE_OK;
}


