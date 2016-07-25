#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uv.h>

#include "dps8.h"
#include "dps8_utils.h"
#include "dps8_fnp2.h"

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

static void associated_readcb (uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf)
  {
    //printf ("assoc. <%*s>\n", (int) nread, buf->base);
    processLineInput (stream, buf->base, nread);
  }

static void unassociated_readcb (uv_stream_t* stream,
                           ssize_t nread,
                           const uv_buf_t* buf)
  {
    //printf ("unaassoc. <%*s>\n", (int) nread, buf->base);
    processUserInput (stream, buf->base, nread);
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
        if (stream->data != (void *) noassoc)
          {
            associated_readcb (stream, nread, buf);
          }
        else
          {
            unassociated_readcb (stream, nread, buf);
          }
      }

    if (buf->base)
        free (buf->base);
  }

/* create a write_request type which contains a buffer and a write request */
typedef struct
  {
    uv_buf_t buffer;
    uv_write_t request;
  } write_req_t;

static void writecb (uv_write_t * req, int status)
  {
    if (status < 0)
      {
        sim_printf ("writecb status %d\n", status);
      }
// XXX free req, buf, buf -> base
    uv_buf_t * bufp = (uv_buf_t *) req->data;
//sim_printf ("bufp->base %p\n", bufp->base);
// If I comment out the sim_printf in fnpuv_start_write, this crashes with 
// ElectricFence Aborting: free(60001d34b): address not from malloc().

    //if (bufp && bufp -> base)
      //free (bufp -> base);
    // the buf structure is copied; do not free.
    //if (req->data)
      //free (req->data);
    free (req);
  }

void fnpuv_start_write (/* uv_tcp_t */ void  * client, char * data, ssize_t datalen)
  {
    uv_write_t * req = (uv_write_t *) malloc (sizeof (uv_write_t));
    uv_buf_t buf = uv_buf_init ((char *) malloc (datalen), datalen);
    req->data = & buf;
//sim_printf ("buf.base %p\n", buf.base);
    memcpy (buf.base, data, datalen);
    int ret = uv_write (req, (uv_stream_t *) client, & buf, 1, writecb);
  }

void fnpuv_start_writestr (/* uv_tcp_t */ void  * client, char * data)
  {
    fnpuv_start_write (client, data, strlen (data));
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
        uv_read_start ((uv_stream_t *) client, alloc_buffer, readcb);
        // Mark client as not connected to an HSLA line
        client->data = (void *) noassoc;
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
