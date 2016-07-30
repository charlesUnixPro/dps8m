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

void fnpuv_associated_readcb (uv_stream_t* stream,
                           ssize_t nread,
                           char * buf)
  {
    //printf ("assoc. <%*s>\n", (int) nread, buf->base);
    processLineInput (stream, buf, nread);
  }

void fnpuv_unassociated_readcb (uv_stream_t* stream,
                           ssize_t nread,
                           char * buf)
  {
    //printf ("unaassoc. <%*s>\n", (int) nread, buf->base);
    processUserInput (stream, buf, nread);
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

void fnpuv_start_write_actual (/* uv_tcp_t */ void  * client, char * data, ssize_t datalen)
  {
    uv_write_t * req = (uv_write_t *) malloc (sizeof (uv_write_t));
    uv_buf_t buf = uv_buf_init ((char *) malloc (datalen), datalen);
    req->data = & buf;
//sim_printf ("fnpuv_start_write_actual req %p buf.base %p\n", req, buf.base);
    memcpy (buf.base, data, datalen);
    int ret = uv_write (req, (uv_stream_t *) client, & buf, 1, writecb);
//sim_printf ("uv_write returns %d\n", ret);
  }

void fnpuv_start_write (/* uv_tcp_t */ void  * client, char * data, ssize_t datalen)
  {
    uvClientData * p = (uvClientData *) ((uv_stream_t *) client)->data;
    telnet_send (p->telnetp, data, datalen);
  }

void fnpuv_start_writestr (/* uv_tcp_t */ void  * client, char * data)
  {
    fnpuv_start_write (client, data, strlen (data));
  }

void fnpuv_read_start (void * client)
  {
    uv_read_start ((uv_stream_t *) client, alloc_buffer, readcb);
  }

void fnpuv_read_stop (void * client)
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
        fnpuv_read_start ((uv_stream_t *) client);
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



#
