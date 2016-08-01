// XXX remember ot call telnet_free on disconnect
#include <stdio.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_cpu.h"
#include "dps8_fnp2.h"
#include "fnpuv.h"
#include "fnptelnet.h"

static const telnet_telopt_t my_telopts[] = {
    // THe initail HSLA port dialog is in line mode; switch
    // to character mode after association is done.
    //{ TELNET_TELOPT_SGA,       TELNET_WILL, TELNET_DO   },
    //{ TELNET_TELOPT_ECHO,      TELNET_WILL, TELNET_DONT },

    //{ TELNET_TELOPT_TTYPE,     TELNET_WONT, TELNET_DONT },
    //{ TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DO   },
    //{ TELNET_TELOPT_ZMP,       TELNET_WONT, TELNET_DO   },
    //{ TELNET_TELOPT_MSSP,      TELNET_WONT, TELNET_DO   },
    { TELNET_TELOPT_BINARY,    TELNET_WILL, TELNET_DO   },
    //{ TELNET_TELOPT_NAWS,      TELNET_WONT, TELNET_DONT },
    { -1, 0, 0 }
  };


static void evHandler (UNUSED telnet_t *telnet, telnet_event_t *event, void *user_data)
  {
    uv_tcp_t * client = (uv_tcp_t *) user_data;
    switch (event->type)
      {
        case TELNET_EV_DATA:
          {
            uvClientData * p = (uvClientData *) client->data;
            if (p -> assoc)
              {
                fnpuv_associated_readcb (client, event->data.size, (char *)event->data.buffer);
              }
            else
              {
                fnpuv_unassociated_readcb (client, event->data.size, (char *)event->data.buffer);
              }
          }
          break;

        case TELNET_EV_SEND:
          {
            //sim_printf ("evHandler: send %zu <%s>\n", event->data.size, event->data.buffer);
            fnpuv_start_write_actual (client, (char *) event->data.buffer, event->data.size);
          }
          break;

        case TELNET_EV_DO:
          {
            if (event->neg.telopt == TELNET_TELOPT_BINARY)
              {
                // DO Binary
              }
            else if (event->neg.telopt == TELNET_TELOPT_SGA)
              {
                // DO Suppress Go Ahead
              }
            else if (event->neg.telopt == TELNET_TELOPT_ECHO)
              {
                // DO Suppress Echo
              }
            else
              {
                sim_printf ("evHandler DO %d\n", event->neg.telopt);
              }
          }
          break;

        case TELNET_EV_DONT:
          {
            sim_printf ("evHandler DONT %d\n", event->neg.telopt);
          }
          break;

        case TELNET_EV_ERROR:
          {
            sim_warn ("libtelnet evHandler error <%s>\n", event->error.msg);
          }
          break;

        default:
          sim_printf ("evHandler: unhandled event %d\n", event->type);
          break;
      }

  }

void * ltnConnect (uv_tcp_t * client)
  {
    void * p = (void *) telnet_init (my_telopts, evHandler, 0, client);
    if (! p)
      {
        sim_warn ("telnet_init failed\n");
      }
    const telnet_telopt_t * q = my_telopts;
    while (q->telopt != -1)
      {
        telnet_negotiate (p, q->us, q->telopt);
        q ++;
      }
    return p;
  }

void ltnRaw (telnet_t * tclient)
  {
    telnet_negotiate (tclient, TELNET_WILL, TELNET_TELOPT_SGA);
    telnet_negotiate (tclient, TELNET_WILL, TELNET_TELOPT_ECHO);
  }

void fnpTelnetInit (void)
  {
#if 0
    for (int fnpno = 0; fnpno < N_FNP_UNITS_MAX; fnpno ++)
      {
        for (int lineno = 0; lineno < MAX_LINES; lineno ++)
          {
            fnpUnitData[fnpno].MState.line[lineno].telnetp = telnet_init (my_telopts, evHandler, 0, NULL);
            if (! fnpUnitData[fnpno].MState.line[lineno].telnetp)
              {
                sim_err ("telnet_init failed\n");
              }
          }
      }
#endif
  }


