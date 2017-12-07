/*
 Copyright 2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// XXX remember to call telnet_free on disconnect
#include <stdio.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_fnp2.h"
#include "fnpuv.h"
#include "fnptelnet.h"

static const telnet_telopt_t my_telopts[] = {
    { TELNET_TELOPT_SGA,       TELNET_WILL, TELNET_DO   },
    { TELNET_TELOPT_ECHO,      TELNET_WILL, TELNET_DONT },

    //{ TELNET_TELOPT_TTYPE,     TELNET_WONT, TELNET_DONT },
    //{ TELNET_TELOPT_COMPRESS2, TELNET_WONT, TELNET_DO   },
    //{ TELNET_TELOPT_ZMP,       TELNET_WONT, TELNET_DO   },
    //{ TELNET_TELOPT_MSSP,      TELNET_WONT, TELNET_DO   },
    { TELNET_TELOPT_BINARY,    TELNET_WILL, TELNET_DO   },
    //{ TELNET_TELOPT_NAWS,      TELNET_WONT, TELNET_DONT },
    { -1, 0, 0 }
  };

static const telnet_telopt_t my_3270telopts[] = {
    { TELNET_TELOPT_TTYPE,     TELNET_WILL, TELNET_DO },
    { TELNET_TELOPT_BINARY,    TELNET_WILL, TELNET_DO   },
    { TELNET_TELOPT_EOR,       TELNET_WILL, TELNET_DO   },
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
                fnpuv_associated_readcb (client, (ssize_t) event->data.size, (unsigned char *)event->data.buffer);
              }
            else
              {
                fnpuv_unassociated_readcb (client, (ssize_t) event->data.size, (unsigned char *)event->data.buffer);
              }
          }
          break;

        case TELNET_EV_SEND:
          {
            //sim_printf ("evHandler: send %zu <%s>\n", event->data.size, event->data.buffer);
            fnpuv_start_write_actual (client, (char *) event->data.buffer, (ssize_t) event->data.size);
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
            else if (event->neg.telopt == TELNET_TELOPT_EOR)
              {
                // DO EOR
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

        case TELNET_EV_WILL:
          {
            if (event->neg.telopt == TELNET_TELOPT_BINARY)
              {
                // WILL BINARY
              }
            else if (event->neg.telopt == TELNET_TELOPT_TTYPE)
              {
                // WILL TTYPE
              }
            else if (event->neg.telopt == TELNET_TELOPT_EOR)
              {
                // WILL EOR
              }
            else
              {
                sim_printf ("evHandler WILL %d\n", event->neg.telopt);
              }
          }
          break;

        case TELNET_EV_WONT:
          {
            sim_printf ("evHandler WONT %d\n", event->neg.telopt);
          }
          break;

        case TELNET_EV_ERROR:
          {
            sim_warn ("libtelnet evHandler error <%s>\n", event->error.msg);
          }
          break;

        case TELNET_EV_IAC:
          {
            if (event->iac.cmd == 243 || // BRK
                event->iac.cmd == 244) // IP
              {
                //sim_printf ("BRK\n");
                uvClientData * p = (uvClientData *) client->data;
                if (p -> assoc)
                  {
                    fnpuv_associated_brk (client);
                  }
                else
                  sim_warn ("liblnet dropping unassociated BRK\n");
              }
            else
              sim_warn ("liblnet unhandle IAC event %d\n", event->iac.cmd);
          }
          break;

        case TELNET_EV_TTYPE:
          {
            // 
          }
          break;

        case TELNET_EV_SUBNEGOTIATION:
          {
            // 
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
        telnet_negotiate (p, q->us, (unsigned char) q->telopt);
        q ++;
      }
    return p;
  }

void * ltnConnect3270 (uv_tcp_t * client)
  {
    void * p = (void *) telnet_init (my_3270telopts, evHandler, 0, client);
    if (! p)
      {
        sim_warn ("telnet_init failed\n");
      }
#if 0
    const telnet_telopt_t * q = my_3270telopts;
    while (q->telopt != -1)
      {
        if (q->us)
          telnet_negotiate (p, q->us, (unsigned char) q->telopt);
        if (q->him)
          telnet_negotiate (p, q->him, (unsigned char) q->telopt);
        q ++;
      }
#endif

// This behavior is copied from Hercules.
    telnet_negotiate (p, TELNET_DO, (unsigned char) TELNET_TELOPT_TTYPE);
    telnet_begin_sb (p, TELNET_TELOPT_TTYPE);
    const char ttype [1] = { 1 };
    telnet_send (p, ttype, 1);
    telnet_finish_sb (p);
    telnet_negotiate (p, TELNET_WILL, (unsigned char) TELNET_TELOPT_BINARY);
    telnet_negotiate (p, TELNET_DO, (unsigned char) TELNET_TELOPT_BINARY);
    telnet_negotiate (p, TELNET_WILL, (unsigned char) TELNET_TELOPT_EOR);
    telnet_negotiate (p, TELNET_DO, (unsigned char) TELNET_TELOPT_EOR);

    return p;
  }

void ltnEOR (telnet_t * tclient)
  {
    telnet_iac (tclient, TELNET_EOR);
  }

void ltnRaw (telnet_t * UNUSED tclient)
  {
    //telnet_negotiate (tclient, TELNET_WILL, TELNET_TELOPT_SGA);
    //telnet_negotiate (tclient, TELNET_WILL, TELNET_TELOPT_ECHO);
  }

void ltnDialout (telnet_t * UNUSED tclient)
  {
    // dialout telnet: We are a teletype. What settings should we be doing?
    //telnet_negotiate (tclient, TELNET_WILL, TELNET_TELOPT_SGA);
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

void fnp3270Init (void)
  {
  }


