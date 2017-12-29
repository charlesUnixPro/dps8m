/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2017 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include <stdio.h>
#include <ctype.h>
#include <netdb.h>

#include "dps8.h"
#include "dps8_socket_dev.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_iom.h"
#include "dps8_cable.h"

static struct {
    const char *name;
    int code;
} errnos[] = {
    {"        ", 0},
    #include "errnos.h"
};
#define N_ERRNOS (sizeof (errnos) / sizeof (errnos[0]))


#define N_SK_UNITS 1 // default

static t_stat sk_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                              UNUSED int val, UNUSED const void * desc)
  {
    sim_printf("Number of socket units in system is %d\n", sk_dev.numunits);
    return SCPE_OK;
  }

static t_stat sk_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_SK_UNITS_MAX)
      return SCPE_ARG;
    sk_dev.numunits = (uint32) n;
    return SCPE_OK;
  }

static MTAB sk_mod [] =
  {
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO | MTAB_VALR, /* mask */
      0,            /* match */
      "NUNITS",     /* print string */
      "NUNITS",         /* match string */
      sk_set_nunits, /* validation routine */
      sk_show_nunits, /* display routine */
      "Number of socket units in the system", /* value descriptor */
      NULL          // help
    },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };


UNIT sk_unit [N_SK_UNITS_MAX] =
  {
    [0 ... (N_SK_UNITS_MAX -1)] =
      {
        UDATA ( NULL, 0, 0), 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL
      },
  };

static DEBTAB sk_dt [] =
  {
    { "NOTIFY", DBG_NOTIFY, NULL },
    { "INFO", DBG_INFO, NULL },
    { "ERR", DBG_ERR, NULL },
    { "WARN", DBG_WARN, NULL },
    { "DEBUG", DBG_DEBUG, NULL },
    { "ALL", DBG_ALL, NULL }, // don't move as it messes up DBG message
    { NULL, 0, NULL }
  };

#define SK_UNIT_NUM(uptr) ((uptr) - sk_unit)

static t_stat sk_reset (DEVICE * dptr)
  {
    return SCPE_OK;
  }


DEVICE sk_dev = {
    "SK",             /* name */
    sk_unit,          /* units */
    NULL,             /* registers */
    sk_mod,           /* modifiers */
    N_SK_UNITS,       /* #units */
    10,               /* address radix */
    31,               /* address width */
    1,                /* address increment */
    8,                /* data radix */
    9,                /* data width */
    NULL,             /* examine routine */
    NULL,             /* deposit routine */
    sk_reset,         /* reset routine */
    NULL,             /* boot routine */
    NULL,             /* attach routine */
    NULL,             /* detach routine */
    NULL,             /* context */
    DEV_DEBUG,        /* flags */
    0,                /* debug control flags */
    sk_dt,            /* debug flag names */
    NULL,             /* memory size change */
    NULL,             /* logical name */
    NULL,             // attach help
    NULL,             // help
    NULL,             // help context
    NULL,             // device description
    NULL
};




void sk_init(void)
  {
    //memset(sk_states, 0, sizeof(sk_states));
  }

static void set_error_str (word36 * error_str, const char * str)
  {
    char work [8];
    strncpy (work, "        ", 8);
    strncpy (work, str, 8);
    error_str[0] = 0;
    error_str[1] = 0;
    putbits36_8 (error_str + 0,  1, (word8) work [0]);
    putbits36_8 (error_str + 0, 10, (word8) work [1]);
    putbits36_8 (error_str + 0, 19, (word8) work [2]);
    putbits36_8 (error_str + 0, 28, (word8) work [3]);
    putbits36_8 (error_str + 1,  1, (word8) work [4]);
    putbits36_8 (error_str + 1, 10, (word8) work [5]);
    putbits36_8 (error_str + 1, 19, (word8) work [6]);
    putbits36_8 (error_str + 1, 28, (word8) work [7]);
  }

static void set_error (word36 * error_str, int _errno)
  {
    if (errno == 0)
      return;
    for (uint i = 0; i < N_ERRNOS; i ++)
      {
        if (errnos[i].code == _errno)
          {
            set_error_str (error_str, errnos[i].name);
            return;
          }
      }
    char huh [256];
    sprintf (huh, "E%d", _errno);
    huh[8] = 0;
    set_error_str (error_str, huh);
  }

static void skt_socket (word36 * buffer)
  {
// /* Data block for socket() call */
// dcl 1 SOCKETDEV_socket_data aligned,
//       2 domain fixed bin,   // 0
//       2 type fixed bin,     // 1
//       2 protocol fixed bin; // 2
//       2 fd fixed bin;       // 3
//       2 errno char(8);      // 4,5

    int domain =   (int) buffer[0];
    int type =     (int) buffer[1];
    int protocol = (int) buffer[2];
    int pid =      (int) buffer[3];

sim_printf ("socket() domain   %d\n", domain);
sim_printf ("socket() type     %d\n", type);
sim_printf ("socket() protocol %d\n", protocol);
sim_printf ("socket() pid      %d\n", pid);

    int _errno = 0;
    int fd = -1;

    if (domain != AF_INET)       // Only AF_INET
      {
        _errno = EAFNOSUPPORT;
      }
    else if (type != SOCK_STREAM && type != (SOCK_STREAM|SOCK_NONBLOCK)) // Only SOCK_STREAM or SOCK_STREAM + SOCK_NONBLOCK
      {
        _errno = EPROTOTYPE;
      }
    else if (protocol != 0) // Only IP
      {
        _errno = EPROTONOSUPPORT;
      }
    else 
      {
        fd = socket ((int) buffer[0], (int) buffer[1], (int) buffer[2]);
sim_printf ("socket() returned %d\n", fd);
        if (fd < 0)
          {
sim_printf ("errno %d\n", errno);
            _errno = errno;
          }
      }
    // sign extend int into word36
    buffer[3] = ((word36) ((word36s) fd)) & MASK36; // fd
    //buffer[5] = ((word36) ((word36s) _errno)) & MASK36; // errno
    set_error (& buffer[4], _errno);
  }

static void skt_gethostbyname (word36 * buffer)
  {
// dcl 1 SOCKETDEV_gethostbyname_data aligned,
//       2 name char varying (255),  
//       3 addr fixed uns bin (32),
//       3 errno char(8);
//
//
//       len:36                    //  0
//       c1: 9, c2: 9, c3:9, c4:9  //  1
//       ...
//       c253: 9, c254: 9, c255: 9, pad: 9, //63
//       addr: 32, pad: 4,          // 65
//       errno: 72                   // 66, 67
//

    word9 cnt = getbits36_9 (buffer [0], 27);

#if 0
    sim_printf ("strlen: %hu\n", cnt);
    sim_printf ("name: \"");
    for (uint i = 0; i < cnt; i ++)
      {
         uint wordno = (i+4) / 4;
         uint offset = ((i+4) % 4) * 9;
         word9 ch = getbits36_9 (buffer[wordno], offset);
         if (isgraph (ch))
            sim_printf ("%c", ch);
         else
            sim_printf ("\\%03o", ch);
      }
    sim_printf ("\"\n");
#endif

    if (cnt > 256)
      {
        sim_warn ("socket$gethostbyname() clipping cnt from %u to 256\n", cnt);
        cnt = 256;
      }

    unsigned char name [257];
    for (uint i = 0; i < cnt; i ++)
      {
         uint wordno = (i+4) / 4;
         uint offset = ((i+4) % 4) * 9;
         word9 ch = getbits36_9 (buffer[wordno], offset);
         name [i] = (unsigned char) (ch & 255);
      }
    name[cnt] = 0;

    struct hostent * hostent = gethostbyname ((char *)name);
sim_printf ("gethostbyname returned %p\n", hostent);
    if (hostent)
      {
sim_printf ("addr_len %d\n", hostent->h_length);
sim_printf ("%hhu.%hhu.%hhu.%hhu\n", hostent->h_addr_list[0][0],hostent->h_addr_list[0][1], hostent->h_addr_list[0][2],hostent->h_addr_list[0][3]);

        //buffer[65] = (* (uint32_t *) (hostent->h_addr_list[0])) << 4;
        // Get the octets in the right order 
        putbits36_8 (& buffer[65],  0, (word8) (((unsigned char) (hostent->h_addr_list[0][0])) & 0xff));
        putbits36_8 (& buffer[65],  8, (word8) (((unsigned char) (hostent->h_addr_list[0][1])) & 0xff));
        putbits36_8 (& buffer[65], 16, (word8) (((unsigned char) (hostent->h_addr_list[0][2])) & 0xff));
        putbits36_8 (& buffer[65], 24, (word8) (((unsigned char) (hostent->h_addr_list[0][3])) & 0xff));
        //buffer[66] = 0; // errno
        set_error (& buffer[66], 0);
      }
    else
      {
sim_printf ("h_errno %d\n", h_errno);
        switch (h_errno)
          {
            case HOST_NOT_FOUND: set_error_str (& buffer[66], "HOST_NOT_FOUND"); break;
            case NO_DATA: set_error_str (& buffer[66], "NO_DATA"); break;
            case NO_RECOVERY: set_error_str (& buffer[66], "NO_RECOVERY"); break;
            case TRY_AGAIN: set_error_str (& buffer[66], "TRY_AGAIN"); break;
            default: set_error_str (& buffer[66], "EHUH"); break;
          }
      }
  }

static void skt_bind (word36 * buffer)
  {
// dcl 1 SOCKETDEV_bind_data aligned,
//       2 socket fixed bin,              // 0
//       2 sockaddr_in,
//         3 sin_family fixed bin,        // 1
//         3 sin_port fixed uns bin (16), // 2
//         3 sin_addr,                    // 3
//           4 octets (4) fixed bin(8) unsigned unal,
//       2 errno char(8);               // 4,5

// /* Expecting tally to be 6 */
// /* sockaddr is from the API parameter */
// /* pid is the Process ID of the calling process */
// /* errno, errno are the values returned by the host socket() call */

//https://www.tutorialspoint.com/unix_sockets/socket_server_example.htm

    int socket_fd = (int) buffer[0];
    int sin_family = (int) buffer[1];
    uint sin_port = (uint) getbits36_16 (buffer [2], 0);
    word8 octet [4] = { getbits36_8 (buffer [3], 0),
                        getbits36_8 (buffer [3], 8),
                        getbits36_8 (buffer [3], 16),
                        getbits36_8 (buffer [3], 24)};
    uint32_t addr = (uint32_t) octet[0];
    addr <<= 8;
    addr |= (uint32_t) octet[1];
    addr <<= 8;
    addr |= (uint32_t) octet[2];
    addr <<= 8;
    addr |= (uint32_t) octet[3];

sim_printf ("bind() socket     %d\n", socket_fd);
sim_printf ("bind() sin_family %d\n", sin_family);
sim_printf ("bind() sin_port   %u\n", sin_port);
sim_printf ("bind() s_addr     %hhu.%hhu.%hhu.%hhu\n", octet[0], octet[1], octet[2], octet[3]);
sim_printf ("bind() s_addr     %08x\n", addr);
  //(buffer [3] >> (36 - 1 * 8)) & MASK8,
  //(buffer [3] >> (36 - 2 * 8)) & MASK8,
  //(buffer [3] >> (36 - 3 * 8)) & MASK8,
  //(buffer [3] >> (36 - 4 * 8)) & MASK8),
sim_printf ("bind() pid        %012llo\n", buffer [4]);

    struct sockaddr_in serv_addr;
    bzero ((char *) & serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl (addr);
    serv_addr.sin_port = htons (sin_port);

    int _errno = 0;
    int rc = bind (socket_fd, (struct sockaddr *) & serv_addr, sizeof (serv_addr));
sim_printf ("bind() returned %d\n", rc);

    if (rc < 0)
      {
sim_printf ("errno %d\n", errno);
        _errno = errno;
      }
    //buffer[5] = ((word36) ((word36s) _errno)) & MASK36; // errno
    set_error (& buffer[4], _errno);
  }

static void skt_listen (word36 * buffer)
  {
// dcl 1 SOCKETDEV_listen_data aligned,
//       2 sockfd fixed bin,  // 0
//       3 backlog fixed bin, // 1
//       2 rc fixed bin;      // 3
//       2 errno char(8);   // 4, 5
// 
// /* Tally 5 */
// /* In: */
// /*   sockfd */
// /*   backlog */
// /*   pid */
// /* Out: */
// /*   fd */
// /*   errno */

    int socket_fd = (int) buffer[0];
    int backlog = (int) buffer[1];
sim_printf ("listen() socket     %d\n", socket_fd);
sim_printf ("listen() backlog    %d\n", backlog   );

    int _errno = 0;
    int rc = listen (socket_fd, backlog);
sim_printf ("listen() returned %d\n", rc);

    if (rc < 0)
      {
sim_printf ("errno %d\n", errno);
        _errno = errno;
      }
    buffer[2] = ((word36) ((word36s) rc)) & MASK36; // rc
    set_error (& buffer[3], _errno);
  }

static int sk_cmd (uint iom_unit_idx, uint chan)
  {
    iomChanData_t * p = & iomChanData[iom_unit_idx][chan];

    sim_debug (DBG_DEBUG, & sk_dev, "IDCW_DEV_CODE %d\n", p->IDCW_DEV_CODE);
    //struct device * d = & cables -> cablesFromIomToDev [iom_unit_idx].devices[chan][p->IDCW_DEV_CODE];
    //uint devUnitIdx = d->devUnitIdx;
    //UNIT * unitp = & sk_unit[devUnitIdx];
sim_printf ("device %u\n", p->IDCW_DEV_CODE);
    switch (p -> IDCW_DEV_CMD)
      {
        case 0: // CMD 00 Request status -- controller status, not tape drive
          {
            p -> stati = 04000; // have_status = 1
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Request status: %04o\n", __func__, p -> stati);
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Request status control: %o\n", __func__, p -> IDCW_CONTROL);
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Request status channel command: %o\n", __func__, p -> IDCW_CHAN_CMD);
          }
          break;

        case 01:               // CMD 01 -- socket()
          {
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: socket_dev_$socket\n", __func__);

            bool ptro, send, uff;
            int rc = iomListService (iom_unit_idx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p->stati = 05001; // BUG: arbitrary error code; config switch
                sim_warn ("%s list service failed\n", __func__);
                return -1;
              }
            if (uff)
              {
                sim_warn ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_warn ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return 1;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_warn ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            uint tally = p -> DDCW_TALLY;
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & sk_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

            if (tally != 6)
              {
                sim_warn ("socket_dev socket call expected tally of 6; got %d\n", tally);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, false);

            skt_socket (buffer);

            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, true);
          }
          break;

        case 02:               // CMD 02 -- bind()
          {
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: socket_dev_$socket\n", __func__);

            bool ptro, send, uff;
            int rc = iomListService (iom_unit_idx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p->stati = 05001; // BUG: arbitrary error code; config switch
                sim_warn ("%s list service failed\n", __func__);
                return -1;
              }
            if (uff)
              {
                sim_warn ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_warn ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return 1;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_warn ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            uint tally = p -> DDCW_TALLY;
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & sk_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

            if (tally != 6)
              {
                sim_warn ("socket_dev bind call expected tally of 6; got %d\n", tally);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, false);

            skt_bind (buffer);

            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, true);

          }
          break;

        case 03:               // CMD 03 -- Debugging
          {
            sim_printf ("socket_dev received command 3\r\n");
            p -> stati = 04000;
          }
          break;

        case 04:               // CMD 04 -- gethostbyname()
          {
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: socket_dev_$gethostbyname\n", __func__);

            bool ptro, send, uff;
            int rc = iomListService (iom_unit_idx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p->stati = 05001; // BUG: arbitrary error code; config switch
                sim_warn ("%s list service failed\n", __func__);
                return -1;
              }
            if (uff)
              {
                sim_warn ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_warn ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return 1;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_warn ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            uint tally = p -> DDCW_TALLY;
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & sk_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

sim_printf ("tally %d\n", tally);
            if (tally != 68)
              {
                sim_warn ("socket_dev gethostbyname call expected tally of 68; got %d\n", tally);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }
            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, false);

            skt_gethostbyname (buffer);

            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, true);

          }
          break;

        case 05:               // CMD 02 -- listen()
          {
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: socket_dev_$listen\n", __func__);

            bool ptro, send, uff;
            int rc = iomListService (iom_unit_idx, chan, & ptro, & send, & uff);
            if (rc < 0)
              {
                p->stati = 05001; // BUG: arbitrary error code; config switch
                sim_warn ("%s list service failed\n", __func__);
                return -1;
              }
            if (uff)
              {
                sim_warn ("%s ignoring uff\n", __func__); // XXX
              }
            if (! send)
              {
                sim_warn ("%s nothing to send\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return 1;
              }
            if (p -> DCW_18_20_CP == 07 || p -> DDCW_22_23_TYPE == 2)
              {
                sim_warn ("%s expected DDCW\n", __func__);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            uint tally = p -> DDCW_TALLY;
            if (tally == 0)
              {
                sim_debug (DBG_DEBUG, & sk_dev,
                           "%s: Tally of zero interpreted as 010000(4096)\n",
                           __func__);
                tally = 4096;
              }

            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Tally %d (%o)\n", __func__, tally, tally);

            if (tally != 5)
              {
                sim_warn ("socket_dev listen call expected tally of 5; got %d\n", tally);
                p -> stati = 05001; // BUG: arbitrary error code; config switch
                return -1;
              }

            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, false);

            skt_listen (buffer);

            iomIndirectDataService (iom_unit_idx, chan, buffer,
                                    & words_processed, true);

          }
          break;
        case 040:               // CMD 040 -- Reset Status
          {
            p -> stati = 04000;
            sim_debug (DBG_DEBUG, & sk_dev,
                       "%s: Reset status is %04o.\n",
                       __func__, p -> stati);
            return 0;
          }

        default:
          {
            p -> stati = 04501;
            p -> chanStatus = chanStatIncorrectDCW;
            sim_warn ("%s: Unknown command 0%o\n", __func__, p -> IDCW_DEV_CMD);
          }
          break;

      } // IDCW_DEV_CMD

    sim_debug (DBG_DEBUG, & sk_dev, "stati %04o\n", p -> stati);

#if 0
    if (p -> IDCW_CONTROL == 3) // marker bit set
      {
        send_marker_interrupt (iom_unit_idx, (int) chan);
      }
#endif
    return 2; // don't contine down the dcw list.
  }


int sk_iom_cmd (uint iom_unit_idx, uint chan)
  {
    iomChanData_t * p = & iomChanData [iom_unit_idx] [chan];
// Is it an IDCW?

    int rc = 0;
    if (p -> DCW_18_20_CP == 7)
      {
        rc = sk_cmd (iom_unit_idx, chan);
      }
    else // DDCW/TDCW
      {
        sim_warn ("%s expected IDCW\n", __func__);
        return -1;
      }
    return rc; //  don't contine down the dcw list.

  }

