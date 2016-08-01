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


// Making it up...
#define DEFAULT_BACKLOG 16

static uv_loop_t * loop;
static uv_tcp_t server;

static void alloc_buffer (UNUSED uv_handle_t * handle, size_t suggested_size, 
                          uv_buf_t * buf)
  {
    * buf = uv_buf_init ((char *) malloc (suggested_size), suggested_size);
  }

void fnpuv_associated_readcb (uv_tcp_t * client,
                           ssize_t nread,
                           char * buf)
  {
    //printf ("assoc. <%*s>\n", (int) nread, buf->base);
    processLineInput (client, buf, nread);
  }

void fnpuv_unassociated_readcb (uv_tcp_t * client,
                           ssize_t nread,
                           char * buf)
  {
    //printf ("unaassoc. <%*s>\n", (int) nread, buf->base);
    processUserInput (client, buf, nread);
  } 

static void readcb (uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf)
  {
    if (nread < 0)
      {
        if (nread == UV_EOF)
          {
            uv_close ((uv_handle_t *) stream, NULL);
          }
      }
    else if (nread > 0)
     {
        uvClientData * p = (uvClientData *) stream->data;
        telnet_recv (p->telnetp, buf->base, nread);
      }

    if (buf->base)
        free (buf->base);
  }

static void writecb (uv_write_t * req, int status)
  {
    if (status < 0)
      {
        sim_printf ("writecb status %d\n", status);
      }
    unsigned int nbufs = req->nbufs;
    uv_buf_t * bufs = req->bufs;
    //if (! bufs)
    if (nbufs > ARRAY_SIZE(req->bufsml))
      bufs = req->bufsml;
//sim_printf ("writecb req %p req->data %p bufs %p nbufs %u\n", req, req->data, bufs, nbufs); 
    for (unsigned int i = 0; i < nbufs; i ++)
      {
        if (bufs && bufs[i].base)
          {
            free (bufs[i].base);
            //bufp -> base = NULL;
          }
      }
    // the buf structure is copied; do not free.
    free (req);
  }

void fnpuv_start_write_actual (uv_tcp_t * client, char * data, ssize_t datalen)
  {
    uv_write_t * req = (uv_write_t *) malloc (sizeof (uv_write_t));
    uv_buf_t buf = uv_buf_init ((char *) malloc (datalen), datalen);
    req->data = & buf;
//sim_printf ("fnpuv_start_write_actual req %p buf.base %p\n", req, buf.base);
    memcpy (buf.base, data, datalen);
    int ret = uv_write (req, (uv_stream_t *) client, & buf, 1, writecb);
    if (ret < 0)
      sim_printf ("uv_write returns %d\n", ret);
  }

void fnpuv_start_write (uv_tcp_t * client, char * data, ssize_t datalen)
  {
    uvClientData * p = (uvClientData *) client->data;
    telnet_send (p->telnetp, data, datalen);
  }

void fnpuv_start_writestr (uv_tcp_t * client, char * data)
  {
    fnpuv_start_write (client, data, strlen (data));
  }

void fnpuv_read_start (uv_tcp_t * client)
  {
    uv_read_start ((uv_stream_t *) client, alloc_buffer, readcb);
  }

void fnpuv_read_stop (uv_tcp_t * client)
  {
    uv_read_stop ((uv_stream_t *) client);
  }

static void on_new_connection (uv_stream_t * server, int status)
  {
    if (status < 0)
      {
        fprintf (stderr, "New connection error %s\n", uv_strerror (status));
        // error!
        return;
      }

    uv_tcp_t * client = (uv_tcp_t *) malloc (sizeof (uv_tcp_t));
    uv_tcp_init (loop, client);
    if (uv_accept (server, (uv_stream_t *) client) == 0)
      {
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
        p -> telnetp = ltnConnect (client);
        if (! p)
          {
             sim_warn ("ltnConnect failed\n");
             return;
          }
        client->data = p;
        fnpuv_read_start (client);
        fnpConnectPrompt (client);
      }
    else
      {
        uv_close ((uv_handle_t *) client, NULL);
      }
  }

void fnpuvInit (int telnet_port)
  {
    loop = uv_default_loop ();

    uv_tcp_init (loop, & server);

    struct sockaddr_in addr;
    uv_ip4_addr ("0.0.0.0", telnet_port, & addr);
    uv_tcp_bind (& server, (const struct sockaddr *) & addr, 0);
    int r = uv_listen ((uv_stream_t *) & server, DEFAULT_BACKLOG, 
                       on_new_connection);
    if (r)
     {
        fprintf (stderr, "Listen error %s\n", uv_strerror (r));
      }
  }

void fnpuvProcessEvent (void)
  {
    // UV_RUN_NOWAIT: Poll for i/o once but don’t block if there are no
    // pending callbacks. Returns zero if done (no active handles or
    // requests left), or non-zero if more callbacks are expected (meaning
    // you should run the event loop again sometime in the future).

    /* int ret = */ uv_run (loop, UV_RUN_NOWAIT);
  }

static void do_readcb (uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf)
  {
    if (nread < 0)
      {
        if (nread == UV_EOF)
          {
            uv_close ((uv_handle_t *) stream, NULL);
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

static void on_do_connect (uv_connect_t * server, int status)
  {
    sim_printf ("dialout connect\n");
    uvClientData * p = (uvClientData *) server->data;
    struct t_line * linep = & fnpUnitData[p->fnpno].MState.line[p->lineno];
    if (status < 0)
      {
        sim_printf ("Dial-out connection error %s\n", uv_strerror (status));
        //sim_printf ("%p\n", p);
        //sim_printf ("%d.%d\n", p->fnpno, p->lineno);
        linep->acu_dial_failure = true;
        return;
      }

sim_printf ("setting accept on %d.%d\n", p->fnpno, p->lineno);
    uv_read_start ((uv_stream_t *) & linep->doSocket, alloc_buffer, do_readcb);
    linep->listen = true;
    linep->accept_new_terminal = true;
sim_printf ("%p:%d\n", & linep->accept_new_terminal, linep->accept_new_terminal);
sim_printf ("%d %d %p %p\n", p->fnpno, p->lineno, linep, & fnpUnitData[p->fnpno].MState.line[p->lineno]);
sim_printf ("%p %p\n", &(linep->accept_new_terminal), &(fnpUnitData[0].MState.line[0].accept_new_terminal));
  }

void fnpuv_dial_out (uint fnpno, uint lineno, word36 d1, word36 d2, word36 d3)
  {
    sim_printf ("received dial_out %c.h%03d %012llo %012llo %012llo\n", fnpno+'a', lineno, d1, d2, d3);
    struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];
    uint d01 = (d1 >> 30) & 07;
    uint d02 = (d1 >> 24) & 07;
    uint d03 = (d1 >> 18) & 07;
    uint d04 = (d1 >> 12) & 07;
    uint d05 = (d1 >>  6) & 07;
    uint d06 = (d1 >>  0) & 07;
    uint d07 = (d2 >> 30) & 07;
    uint d08 = (d2 >> 24) & 07;
    uint d09 = (d2 >> 18) & 07;
    uint d10 = (d2 >> 12) & 07;
    uint d11 = (d2 >>  6) & 07;
    uint d12 = (d2 >>  0) & 07;
    uint p1 = (d3 >> 30) & 07;
    uint p2 = (d3 >> 24) & 07;
    uint p3 = (d3 >> 18) & 07;
    uint p4 = (d3 >> 12) & 07;
    uint p5 = (d3 >>  6) & 07;
    uint p6 = (d3 >>  0) & 07;

    uint oct1 = d01 * 100 + d02 * 10 + d03;
    uint oct2 = d04 * 100 + d05 * 10 + d06;
    uint oct3 = d07 * 100 + d08 * 10 + d09;
    uint oct4 = d10 * 100 + d11 * 10 + d12;

    uint port = (((((p1 * 10) + p2) * 10 + p3) * 10 + p4) * 10 + p5) * 10 + p6;

    char ipaddr [256];
    sprintf (ipaddr, "%d.%d.%d.%d", oct1, oct2, oct3, oct4);
    printf ("calling %s:%d\n", ipaddr,port);

    struct sockaddr_in dest;
    uv_ip4_addr(ipaddr, port, &dest);

    uv_tcp_init (loop, & linep->doSocket);


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

    linep->doConnect.data = p;

    uv_tcp_connect (& linep->doConnect, & linep->doSocket, (const struct sockaddr *) & dest, on_do_connect);
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

    uv_read_start ((uv_stream_t *) & linep->doSocket, alloc_buffer, do_readcb);
    linep->accept_new_terminal = true;
  }
#endif

void fnpuv_open_slave (uint fnpno, uint lineno)
  {
    sim_printf ("fnpuv_open_slave %d.%d\n", fnpno, lineno);
#if 0
    struct t_line * linep = & fnpUnitData[fnpno].MState.line[lineno];
    uv_tcp_init (loop, & linep->doSocket);


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

    linep->doConnect.data = p;

    struct sockaddr_in addr;
    uv_ip4_addr ("0.0.0.0", linep->port, & addr);
    uv_tcp_bind (& server, (const struct sockaddr *) & addr, 0);
    int r = uv_listen ((uv_stream_t *) & server, DEFAULT_BACKLOG, 
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
    uv_ip4_addr ("0.0.0.0", 6180, & addr);

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
