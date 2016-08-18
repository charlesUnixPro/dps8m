// The FNP <--> libuv interface
//
// Every libuv TCP connection has a uv_tcp_t object.
// 
// The uv_tcp_t object has a 'user data' field defined as "void * data".
//
// This code stores a pointer to a "struct uvClientData" in 'data'; this
// structure allows this code to determine which HSLA line the connection
// has been made with.
//
// The uvClientData structure contains:
//
//    bool assoc
//    uint fnpno;
//    uint lineno;
//
//       If 'assoc' is true, the TCP connection is associated with a particular
//       HSLA line as specified by 'fnpno' and 'lineno'. A dialup line is 
//       associated by the user entering the FNP name and line number in the
//       multiplexor dial in dialog. For slave and dialout lines, 'assoc'
//       is always true as the assoication is predetermined by the FNP
//       configuration.
//
//    char buffer [1024];
//    size_t nPos;
//
//      Line canonicalization buffer used by the multiplexor dial in dialog.
//
// Each HSLA line is identified by a 'fnpno', 'lineno' pair. 'fnpno' indexes
// into the 'fnpUnitData' data structure and 'lineno' into 
// 'fnpUnitData[fnpno].MState.line[]'.
//
// The 'line' structure contains:
//
//   enum service_types service;
//
//     This tracks the Multics' line configuration of 'dialup', 'dialout', and
//     'slave'.
//
//   uv_tcp_t * client;
//
//     If this is non-null, the HSLA has a TCP connection.
//
//   telnet_t * telnetp;
//
//     If this is non-null, Telnet is enabled on the connection.
//
//   uv_tcp_t server;
//
//     This is used by the 'slave' logic and holds the handle of the
//     TCP server port for the slave line.
//
//     The data field of 'server' points to a uvClientData structure; this
//     field is used by the incoming connection handler to distinguish the 
//     slave and dialup server ports. (If null, dialup; else slave.)
//
//   uv_connect_t doConnect;
//
//     libuv uses a uv_connect_t object to create outbound connections; 
//     the dialout code uses 'doConnect' for this.
//
//   int port;
//
//     The port number that a slave line listens on, as set by Devices.txt.
//


// Dialup logic.
//
// The code assumes a single port number for all dialup lines, even for the 
// case of multiple FNPs.
//
// 'fnpuvInit()' is called when by the 'fnpserverport' command logic. It
// creates a TCP server listen port, assigning 'on_new_connection()' as
// the connection callback handler.
//
// When a connection is made to the listen port, libuv invokes the 
// 'on_new_connection()' callback handler.
//
// The handler creates a 'uv_tcp_t' object 'client' and accepts the 
// connection. 
//
// It then checks the 'data' field on the listener object to see if this
// is a dialup or slave listener. For the case of dialup, a 'uvClientData'
// object is created and its 'assoc' field is set to false to flag the
// the connection as not yet being associated with an HSLA line; the
// libtelnet data structure is intialized and the initial Telnet negotiations
// are started. 'client->data' is set the the 'uvClientData' object, 
// reading is enabled on 'client', and the HSLA line selection dialog
// prompt is sent down the dialup connection.
//
// When input data is ready on the connection (user input), libuv allocates
// a buffer, fills it with the data calls the 'fuv_read_cb()' callback handler.
//
// The handler looks at the 'data' field of the connection to access the 
// 'uvClientData' structure; if the sturcture indicates that Telnet 
// processing is to be done, it passes the data to libtelent handler.
// Otherwise, it checks the 'assoc' field; if true the data is passed to the 
// FNP incoming data handler; if false, to the multiplexor line selection
// dialog handler. The libtelnet handler, after stripping and processing
// Telnet overhead passes any remaining data on in the same manner.
// The data buffer is then freed, and the callback returns.
//
// Since the FNP incoming data handler is dependent on Multics accepting
// the input data before it can safely dispose of the data, it disables
// reading on the connection, and reenables it when Multics has accepted the
// data.

// Data are written to the TCP connections with 'fnpuv_start_write()'. The
// 'uvClientData' is inspected for telnet usage; if so, the data is passed
// to libtelnet for telnet packaging and is sent on to 
// 'fnpuv_start_write_actual'. If telnet is not in play, the data is sent
// directly on to 'fnpuv_start_write_actual()' There, a buffer is allocated
// and the data copied and a 'uv_write_t' request object is created and
// initalized data the buffer address and length, and a write request
// is send to libuv.
//
// When the write is complete, libuv calls the write callback 'fuv_write_cb()',
// which frees the data buffers and destroys the write request object.

// Dialout logic
//
// When the FNP receives a 'dial_out' command, it calls 'fnpuv_dial_out()'.
// The dial string is parsed for IP address, port number and telnet flag.
// An 'uv_tcp_t' object constructed and its address stored in the HSLA
// 'client' field. An 'uvClientData' object is created, associated with
// the dialout line HSLA, and if the Telnet flag is set, the Telnet processor
// is initialized. The TCP connect is initiated and the procedure returns.
//
// When the connection succeeds or times out, the 'on_do_connect()' callback 
// is called. on_do_connect retrieves the 'uvClientData'. If the connection
// succeeded, the connection data field is set to the 'uvClientData'; read is
// enabled on the connection, and the 'accept_new_terminal' flag is set which
// will cause an 'accept_new_terminal' command to be send to Multics.
//






#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uv.h>

#include "dps8.h"
#include "dps8_utils.h"
#include "dps8_fnp2.h"
#include "fnpuv.h"
#include "fnptelnet.h"

//#define TEST

#define USE_REQ_DATA

// Making it up...
#define DEFAULT_BACKLOG 16

static uv_loop_t * loop = NULL;
static uv_tcp_t du_server;

//
// alloc_buffer: libuv callback handler to allocate buffers for incomingd data.
//

static void alloc_buffer (UNUSED uv_handle_t * handle, size_t suggested_size, 
                          uv_buf_t * buf)
  {
    * buf = uv_buf_init ((char *) malloc (suggested_size), (uint) suggested_size);
  }


// read callback for connections that are associated with an HSLA line;
// forward the data to the FNP input handler

void fnpuv_associated_readcb (uv_tcp_t * client,
                           ssize_t nread,
                           unsigned char * buf)
  {
    //printf ("assoc. <%*s>\n", (int) nread, buf->base);
    processLineInput (client, buf, nread);
  }

// read callback for connections that are no tassociated with an HSLA line;
// forward the data to the line selection dialog handler

void fnpuv_unassociated_readcb (uv_tcp_t * client,
                           ssize_t nread,
                           unsigned char * buf)
  {
    //printf ("unaassoc. <%*s>\n", (int) nread, buf->base);
    processUserInput (client, buf, nread);
  } 

static void fuv_close_cb (uv_handle_t * stream)
  {
    free (stream);
  }

// teardown a connection

void close_connection (uv_stream_t* stream)
  {
    uvClientData * p = (uvClientData *) stream->data;
    // If stream->data, the stream is associated with a Multics line.
    // Tear down that association
    if (p)
      {
        if (p->assoc)
          {
            sim_printf ("DISCONNECT %c.d%03d\n", p->fnpno+'a', p->lineno);
            struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
            linep -> line_disconnected = true;
          }
        else
          {
            sim_printf ("DISCONNECT\n");
          }

        // Clean up allocated data
        if (p->telnetp)
          {
            telnet_free (p->telnetp);
            // telnet_free frees self
            //free (p->telnetp);
            p->telnetp = NULL;
          }
        if (p->assoc)
          {
            struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
            if (linep->client)
              {
// This is a long winded way to free (stream->data)

                //free (linep->client);
                linep->client = NULL;
              }
          }
        free (stream->data);
        stream->data = NULL;
      }
    if (! uv_is_closing ((uv_handle_t *) stream))
      uv_close ((uv_handle_t *) stream, fuv_close_cb);
  }

//
// fuv_read_cb: libuv read complete callback
//
//   Cleanup on read error.
//   Forward data to appropriate handler.

static void fuv_read_cb (uv_stream_t* stream,
                         ssize_t nread,
                         const uv_buf_t* buf)
  {
    if (nread < 0)
      {
        if (nread == UV_EOF)
          {
            close_connection (stream);
          }
      }
    else if (nread > 0)
      {
        uvClientData * p = (uvClientData *) stream->data;
        if (p)
          {
            if (p->telnetp)
              {
                telnet_recv (p->telnetp, buf->base, (size_t) nread);
              }
            else
              {
                if (p -> assoc)
                  {
                    fnpuv_associated_readcb ((uv_tcp_t *) stream, nread, (unsigned char *) buf->base);
                  }
                else
                  {
                    fnpuv_unassociated_readcb ((uv_tcp_t *) stream, nread, (unsigned char *) buf->base);
                  }
              }
          }
      }

    if (buf->base)
        free (buf->base);
  }

//
// fuv_write_cb: libuv write complete callback
//
//   Cleanup on error
//   Free buffers
//

static void fuv_write_cb (uv_write_t * req, int status)
  {
    if (status < 0)
      {
        if (status == -ECONNRESET)
          {
            // This occurs when the other end disconnects; not an "error"
          }
        else
          {
            sim_warn ("fuv_write_cb status %d (%s)\n", -status, strerror (-status));
          }

        // connection reset by peer
        close_connection (req->handle);
      }

#ifdef USE_REQ_DATA
//sim_printf ("freeing bufs %p\n", req->data);
    free (req->data);
#else
    unsigned int nbufs = req->nbufs;
    uv_buf_t * bufs = req->bufs;
    //if (! bufs)
#if 0
    if (nbufs > ARRAY_SIZE(req->bufsml))
      bufs = req->bufsml;
#endif
//sim_printf ("fuv_write_cb req %p req->data %p bufs %p nbufs %u\n", req, req->data, bufs, nbufs); 
    for (unsigned int i = 0; i < nbufs; i ++)
      {
        if (bufs && bufs[i].base)
          {
//sim_printf ("freeing bufs%d %p\n", i, bufs[i].base);
            free (bufs[i].base);
            //bufp -> base = NULL;
          }
        if (req->bufsml[i].base)
          {
//sim_printf ("freeing bufsml%d %p@%p\n", i, req->bufsml[i].base, & req->bufsml[i].base);
            free (req->bufsml[i].base);
          }
      }
#endif

    // the buf structure is copied; do not free.
//sim_printf ("freeing req %p\n", req);
    free (req);
  }

// Create and start a write request

void fnpuv_start_write_actual (uv_tcp_t * client, char * data, ssize_t datalen)
  {
    if (! client || uv_is_closing ((uv_handle_t *) client))
      return;
    uv_write_t * req = (uv_write_t *) malloc (sizeof (uv_write_t));
    // This makes sure that bufs*.base and bufsml*.base are NULL
    memset (req, 0, sizeof (uv_write_t));
    uv_buf_t buf = uv_buf_init ((char *) malloc ((unsigned long) datalen), (uint) datalen);
//sim_printf ("allocated req %p data %p\n", req, buf.base);
#ifdef USE_REQ_DATA
    req->data = buf.base;
#endif
//sim_printf ("fnpuv_start_write_actual req %p buf.base %p\n", req, buf.base);
    memcpy (buf.base, data, (unsigned long) datalen);
    int ret = uv_write (req, (uv_stream_t *) client, & buf, 1, fuv_write_cb);
// There seems to be a race condition when Mulitcs signals a disconnect_line;
// We close the socket, but Mulitcs is still writing its goodbye text trailing
// NULs.
// If the socket has been closed, write will return BADF; just ignore it.
    if (ret < 0 && ret != -EBADF)
      sim_printf ("uv_write returns %d\n", ret);
  }

//
// Write data to a connection, doing Telnet if needed.
//

void fnpuv_start_write (uv_tcp_t * client, char * data, ssize_t datalen)
  {
    if (! client || uv_is_closing ((uv_handle_t *) client))
      return;
    uvClientData * p = (uvClientData *) client->data;
    if (! p)
      return;
    if (p->telnetp)
      {
        telnet_send (p->telnetp, data, (size_t) datalen);
      }
    else
      {
        fnpuv_start_write_actual (client, data, datalen);
      }
  }

// C-string wrapper for fnpuv_start_write

void fnpuv_start_writestr (uv_tcp_t * client, char * data)
  {
    fnpuv_start_write (client, data, (ssize_t) strlen (data));
  }

//
// Enable reading on connection
//

void fnpuv_read_start (uv_tcp_t * client)
  {
    if (! client || uv_is_closing ((uv_handle_t *) client))
      return;
    uv_read_start ((uv_stream_t *) client, alloc_buffer, fuv_read_cb);
  }

//
// Disable reading on connection
//

void fnpuv_read_stop (uv_tcp_t * client)
  {
    if (! client || uv_is_closing ((uv_handle_t *) client))
      return;
    uv_read_stop ((uv_stream_t *) client);
  }

//
// Connection callback handler for dialup connections
//

static void on_new_connection (uv_stream_t * server, int status)
  {
    if (status < 0)
      {
        fprintf (stderr, "New connection error %s\n", uv_strerror (status));
        // error!
        return;
      }

    uv_tcp_t * client = (uv_tcp_t *) malloc (sizeof (uv_tcp_t));

#if 0
    // if server->data is non-null, this is a slave server; else a dialup
    // server
    if (server->data)
      {
        uvClientData * p = (uvClientData *) server->data;
        struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
sim_printf ("slave connection to %d.%d\n", p->fnpno, p->lineno); 
        linep->client = client;
      }
#endif
    uv_tcp_init (loop, client);
    if (uv_accept (server, (uv_stream_t *) client) == 0)
      {

        // if server->data is non-null, this is a slave server; else a dialup
        // server
        if (server->data)
          {
            uvClientData * p = (uvClientData *) server->data;
            struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
#if 1
            // Slave servers only handle a single connection at a time
            if (linep->client)
              {
                uv_close ((uv_handle_t *) client, fuv_close_cb);
sim_printf ("dropping 2nd slave\n");
                return;
              }
#endif
            linep->client = client;
          }
        
        struct sockaddr name;
        int namelen = sizeof (name);
        int ret = uv_tcp_getpeername (client, & name, & namelen);
        if (ret < 0)
          {
            sim_printf ("CONNECT; addr err %d\n", ret);
          }
        else
          {
            struct sockaddr_in * p = (struct sockaddr_in *) & name;
            sim_printf ("CONNECT %s\n", inet_ntoa (p -> sin_addr));
          }

        uvClientData * p = (uvClientData *) malloc (sizeof (uvClientData));
        if (! p)
          {
             sim_warn ("uvClientData malloc failed\n");
             return;
          }
        p -> assoc = false;
        p -> nPos = 0;

        // dialup connections are routed through libtelent
        if (! server->data)
          {
            p->telnetp = ltnConnect (client);

            if (! p->telnetp)
              {
                 sim_warn ("ltnConnect failed\n");
                 return;
              }
          }
        else
          {
            p->telnetp = NULL;
            uvClientData * q = (uvClientData *) server->data;
            p->fnpno = q->fnpno;
            p->lineno = q->lineno;
            p->assoc = true;
          }
        client->data = p;
        fnpuv_read_start (client);
        if (! server->data)
          fnpConnectPrompt (client);
        else
          {
            uvClientData * p = (uvClientData *) server->data;
            struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
            linep->accept_new_terminal = true;
          }
      }
    else
      {
        uv_close ((uv_handle_t *) client, fuv_close_cb);
      }
  }

//
// Setup the dialup listener
//

void fnpuvInit (int telnet_port)
  {

    // Initialize the server socket
    loop = uv_default_loop ();
    uv_tcp_init (loop, & du_server);

// XXX to do clean shutdown
// uv_loop_close (loop);

    // Flag the this server as being the dialup server
    du_server.data = NULL;

    // Bind and listen
    struct sockaddr_in addr;
sim_printf ("listening to %d\n", telnet_port);
    uv_ip4_addr ("0.0.0.0", telnet_port, & addr);
    uv_tcp_bind (& du_server, (const struct sockaddr *) & addr, 0);
    int r = uv_listen ((uv_stream_t *) & du_server, DEFAULT_BACKLOG, 
                       on_new_connection);
    if (r)
     {
        fprintf (stderr, "Listen error %s\n", uv_strerror (r));
      }
  }

// Make a single pass through the libev event queue.

void fnpuvProcessEvent (void)
  {
    // UV_RUN_NOWAIT: Poll for i/o once but don’t block if there are no
    // pending callbacks. Returns zero if done (no active handles or
    // requests left), or non-zero if more callbacks are expected (meaning
    // you should run the event loop again sometime in the future).

    // Note that uv_run returns non-zero if that are any active_handles 
    // (e.g. TCP connection listener open); that means a non-zero
    // return does not mean i/o is pending.
    if (! loop)
      return;
    /* int ret = */ uv_run (loop, UV_RUN_NOWAIT);
  }

#if 0
static void do_readcb (uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf)
  {
    if (nread < 0)
      {
        if (nread == UV_EOF)
          {
            uv_close ((uv_handle_t *) stream, fuv_close_cb);
          }
      }
    else if (nread > 0)
      {
// This moves the data to buffer, which skips the processInputCharacter
// break scanning.
#if 0
        uvClientData * p = (uvClientData *) stream->data;
        struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
        size_t room = sizeof (linep->buffer) - linep->nPos;
        if (nread > room)
          {
            sim_printf ("do_readcb cropping oversize data from %d to %d\n",
              nread, room);
            nread = room;
          }
        memcpy (linep->buffer, buf->base, nread);
        linep->nPos += nread;
#endif

        processLineInput ((uv_tcp_t*) stream, buf->base, nread);
      }

    if (buf->base)
      free (buf->base);
  }
#endif

//
// dialout line connection callback
//

static void on_do_connect (uv_connect_t * server, int status)
  {
    sim_printf ("dialout connect\n");
    uvClientData * p = (uvClientData *) server->handle->data;
    struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
    if (status < 0)
      {
        sim_printf ("Dial-out connection error %s\n", uv_strerror (status));
        //sim_printf ("%p\n", p);
        //sim_printf ("%d.%d\n", p->fnpno, p->lineno);
        linep->acu_dial_failure = true;
        return;
      }

    uv_read_start ((uv_stream_t *) linep->client, alloc_buffer, fuv_read_cb);
    linep->listen = true;
    linep->accept_new_terminal = true;
    linep->client->data = p;
    if (linep->telnetp)
      {
        ltnDialout (linep->telnetp);
      }
  }

//
// Perform a dialout 
//

void fnpuv_dial_out (uint fnpno, uint lineno, word36 d1, word36 d2, word36 d3)
  {
    if (! loop)
      return;
    sim_printf ("received dial_out %c.h%03d %012llo %012llo %012llo\n", fnpno+'a', lineno, d1, d2, d3);
    struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];
    uint d01 = (d1 >> 30) & 017;
    uint d02 = (d1 >> 24) & 017;
    uint d03 = (d1 >> 18) & 017;
    uint d04 = (d1 >> 12) & 017;
    uint d05 = (d1 >>  6) & 017;
    uint d06 = (d1 >>  0) & 017;
    uint d07 = (d2 >> 30) & 017;
    uint d08 = (d2 >> 24) & 017;
    uint d09 = (d2 >> 18) & 017;
    uint d10 = (d2 >> 12) & 017;
    uint d11 = (d2 >>  6) & 017;
    uint d12 = (d2 >>  0) & 017;
    uint flags = (d3 >> 30) & 017;
    uint p1 = (d3 >> 24) & 017;
    uint p2 = (d3 >> 18) & 017;
    uint p3 = (d3 >> 12) & 017;
    uint p4 = (d3 >>  6) & 017;
    uint p5 = (d3 >>  0) & 017;

    uint oct1 = d01 * 100 + d02 * 10 + d03;
    uint oct2 = d04 * 100 + d05 * 10 + d06;
    uint oct3 = d07 * 100 + d08 * 10 + d09;
    uint oct4 = d10 * 100 + d11 * 10 + d12;

    uint port = ((((p1 * 10) + p2) * 10 + p3) * 10 + p4) * 10 + p5;

// Flags
//   1    Use Telnet
//   2    ?
//   4

    char ipaddr [256];
    sprintf (ipaddr, "%d.%d.%d.%d", oct1, oct2, oct3, oct4);
    printf ("calling %s:%d\n", ipaddr,port);

    struct sockaddr_in dest;
    uv_ip4_addr(ipaddr, (int) port, &dest);

    linep->client = (uv_tcp_t *) malloc (sizeof (uv_tcp_t));
    uv_tcp_init (loop, linep->client);


    uvClientData * p = (uvClientData *) malloc (sizeof (uvClientData));
    if (! p)
      {
         sim_warn ("uvClientData malloc failed\n");
         return;
      }
    p->assoc = true;
    p->nPos = 0;
    if (flags & 1)
      {
        p->telnetp = ltnConnect (linep->client);

        if (! p->telnetp)
          {
              sim_warn ("ltnConnect failed\n");
          }
      }
    else
      {
        p->telnetp = NULL; // Mark this line as 'not a telnet connection'
      }
    p->fnpno = fnpno;
    p->lineno = lineno;

    linep->client->data = p;

    uv_tcp_connect (& linep->doConnect, linep->client, (const struct sockaddr *) & dest, on_do_connect);
  }

#if 0
static void on_slave_connect (uv_stream_t * server, int status)
  {
    sim_printf ("slave connect\n");
    uvClientData * p = (uvClientData *) server->data;
    struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
    if (status < 0)
      {
        sim_printf ("Slave connection error %s\n", uv_strerror (status));
        //linep->acu_dial_failure = true;
        return;
      }

    uv_read_start ((uv_stream_t *) & linep->client, alloc_buffer, do_readcb);
    linep->accept_new_terminal = true;
  }
#endif


//
// Start a slave line connection listener.
//

void fnpuv_open_slave (uint fnpno, uint lineno)
  {
    if (! loop)
      return;
    sim_printf ("fnpuv_open_slave %d.%d\n", fnpno, lineno);
    struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];

    // Do we already have a listening port (ie not first time)?
    if (linep->server.data)
      return;

    uv_tcp_init (loop, & linep->server);

    // Mark this server has being a slave server
    // XXX This does not get freed during a normal shutdown, as Multics
    // XXX doesn't tell idle slave lines anything. The emulator shutdown
    // XXX needs to call an FNP cleanup routine that frees this.

    uvClientData * p = (uvClientData *) malloc (sizeof (uvClientData));
    if (! p)
      {
         sim_warn ("uvClientData malloc failed\n");
         return;
      }
    p->assoc = false;
    p->nPos = 0;
    p->fnpno = fnpno;
    p->lineno = lineno;
    linep->server.data = p;
    linep->client = NULL;

    struct sockaddr_in addr;
    uv_ip4_addr ("0.0.0.0", linep->port, & addr);
    uv_tcp_bind (& linep->server, (const struct sockaddr *) & addr, 0);
sim_printf ("listening on port %d\n", linep->port);
    int r = uv_listen ((uv_stream_t *) & linep->server, 0, 
                       on_new_connection);
    if (r)
     {
        fprintf (stderr, "Listen error %s\n", uv_strerror (r));
      }

// It should be possible to run a peer-to-peer TCP instead of client server,
// but it's not clear to me.

#if 0
    linep->client = (uv_tcp_t *) malloc (sizeof (uv_tcp_t));
    uv_tcp_init (loop, linep->client);

    uvClientData * p = (uvClientData *) malloc (sizeof (uvClientData));
    if (! p)
      {
         sim_warn ("uvClientData malloc failed\n");
         return;
      }
    p->assoc = false;
    p->telnetp = NULL;
    p->fnpno = fnpno;
    p->lineno = lineno;

    linep->client->data = p;

    struct sockaddr_in addr;
    uv_ip4_addr ("0.0.0.0", linep->port, & addr);
    uv_tcp_bind (linep->client, (const struct sockaddr *) & addr, 0);
sim_printf ("listening on port %d\n", linep->port);
    int r = uv_listen ((uv_stream_t *) linep->client, 0, 
                       on_slave_connect);
    if (r)
     {
        fprintf (stderr, "Listen error %s\n", uv_strerror (r));
      }
#endif
  }

#ifdef TEST
int main (int argc, char * argv [])
  {
    loop = uv_default_loop ();

    uv_tcp_init (loop, & server);

    struct sockaddr_in addr;
    uv_ip4_addr ("0.0.0.0", get_dialin_telnet_port (), & addr);

    uv_tcp_bind (& server, (const struct sockaddr *) & addr, 0);
    int r = uv_listen ((uv_stream_t *) & server, DEFAULT_BACKLOG, 
                       on_new_connection);
    if (r)
     {
        fprintf (stderr, "Listen error %s\n", uv_strerror (r));
        return 1;
      }
    //return uv_run (loop, UV_RUN_DEFAULT);
    while (1)
      {
        int ret = uv_run (loop, UV_RUN_NOWAIT);
        printf ("ping %d\n", ret);
        sleep (1);
      }
  }
#endif
