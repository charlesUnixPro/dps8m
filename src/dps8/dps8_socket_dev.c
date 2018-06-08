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
#include <sys/ioctl.h>
#include <unistd.h>

#include "dps8.h"
#include "dps8_socket_dev.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_iom.h"
#include "dps8_cable.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"

#define DBG_CTR 1

static struct {
    const char *name;
    int code;
} errnos[] = {
    {"        ", 0},
    #include "errnos.h"
};
#define N_ERRNOS (sizeof (errnos) / sizeof (errnos[0]))

#define N_FDS 1024

static struct
  {
    int fd_unit[N_FDS]; // unit number that a FD is associated with; -1 is free.   
    bool fd_nonblock[N_FDS]; // socket() call had NON_BLOCK set
    struct
      {
        enum 
          {
            unit_idle = 0,
            unit_accept,
            unit_read
          } unit_state;
         //fd_set accept_fds;
         int accept_fd;
         int read_fd;
         uint read_buffer_sz;
      } unit_data[N_SK_UNITS_MAX];
  } sk_data;

#define N_SK_UNITS 64 // default

static t_stat sk_show_nunits (UNUSED FILE * st, UNUSED UNIT * uptr, 
                              UNUSED int val, UNUSED const void * desc)
  {
    sim_printf("Number of socket units in system is %d\n", skc_dev.numunits);
    return SCPE_OK;
  }

static t_stat sk_set_nunits (UNUSED UNIT * uptr, UNUSED int32 value, 
                             const char * cptr, UNUSED void * desc)
  {
    int n = atoi (cptr);
    if (n < 1 || n > N_SK_UNITS_MAX)
      return SCPE_ARG;
    skc_dev.numunits = (uint32) n;
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


DEVICE skc_dev = {
    "SKC",            /* name */
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
    // sets unit_state to unit_idle
    memset(& sk_data, 0, sizeof(sk_data));
    for (uint i = 0; i < N_FDS; i ++)
      sk_data.fd_unit[i] = -1;
    //for (uint i = 0; i < N_SK_UNITS_MAX; i ++)
      //FD_ZERO (& sk_data.unit_data[i].accept_fds);
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

static void skt_socket (int unit_num, word36 * buffer)
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

sim_printf ("socket() domain   %d\n", domain);
sim_printf ("socket() type     %d\n", type);
sim_printf ("socket() protocol %d\n", protocol);

    int _errno = 0;
    int fd = -1;

    if (domain != AF_INET)       // Only AF_INET
      {
sim_printf ("socket() domain EAFNOSUPPORT\n");
        _errno = EAFNOSUPPORT;
      }
    else if (type != SOCK_STREAM && type != (SOCK_STREAM|SOCK_NONBLOCK)) // Only SOCK_STREAM or SOCK_STREAM + SOCK_NONBLOCK
      {
sim_printf ("socket() type EPROTOTYPE\n");
        _errno = EPROTOTYPE;
      }
    else if (protocol != 0) // Only IP
      {
sim_printf ("socket() protocol EPROTONOSUPPORT\n");
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
        else if (fd < N_FDS)
          {
            sk_data.fd_unit[fd] = unit_num;
            sk_data.fd_nonblock[fd] = !! (type & SOCK_NONBLOCK);
          }
        else
          {
            close (fd);
            fd = -1;
            _errno = EMFILE;
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

        uint32_t addr = * ((uint32_t *) & hostent->h_addr_list[0][0]);
sim_printf ("addr %08x\n", addr);
        addr = ntohl (addr);
sim_printf ("addr %08x\n", addr);
        buffer[65] = ((word36) addr) << 4;
        // Get the octets in the right order 
        //putbits36_8 (& buffer[65],  0, (word8) (((unsigned char) (hostent->h_addr_list[0][0])) & 0xff));
        //putbits36_8 (& buffer[65],  8, (word8) (((unsigned char) (hostent->h_addr_list[0][1])) & 0xff));
        //putbits36_8 (& buffer[65], 16, (word8) (((unsigned char) (hostent->h_addr_list[0][2])) & 0xff));
        //putbits36_8 (& buffer[65], 24, (word8) (((unsigned char) (hostent->h_addr_list[0][3])) & 0xff));
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

static void skt_bind (int unit_num, word36 * buffer)
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

    // Does this socket belong to us?
    if (sk_data.fd_unit[socket_fd] != unit_num)
      {
        set_error (& buffer[4], EBADF);
        return;
      }

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
    set_error (& buffer[4], _errno);
  }

static void skt_listen (int unit_num, word36 * buffer)
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
// /* Out: */
// /*   fd */
// /*   errno */

    int socket_fd = (int) buffer[0];
    int backlog = (int) buffer[1];
sim_printf ("listen() socket     %d\n", socket_fd);
sim_printf ("listen() backlog    %d\n", backlog   );

    int rc = 0;
    int _errno = 0;
    // Does this socket belong to us?
    if (sk_data.fd_unit[socket_fd] != unit_num)
      {
sim_printf ("listen() socket doesn't belong to us\n");
sim_printf ("socket_fd %u fd_unit %u unit_num %u\n", socket_fd, sk_data.fd_unit[socket_fd], unit_num);
        _errno = EBADF;
        goto done;
      }

    int on = 1;
    rc = setsockopt (socket_fd, SOL_SOCKET,  SO_REUSEADDR,
                   (char *) & on, sizeof (on));
sim_printf ("listen() setsockopt returned %d\n", rc);
    if (rc < 0)
      {
        _errno = errno;
        goto done;
      }

    rc = ioctl (socket_fd, FIONBIO, (char *) & on);
sim_printf ("listen() ioctl returned %d\n", rc);
    if (rc < 0)
      {
        _errno = errno;
        goto done;
      }

    rc = listen (socket_fd, backlog);
sim_printf ("listen() returned %d\n", rc);

    if (rc < 0)
      {
sim_printf ("errno %d\n", errno);
        _errno = errno;
        goto done;
      }

done:
    buffer[2] = ((word36) ((word36s) rc)) & MASK36; // rc
    set_error (& buffer[3], _errno);
  }

static int skt_accept (int unit_num, word36 * buffer)
  {
// dcl 1 SOCKETDEV_accept_data aligned,
//       2 sockfd fixed bin,                           // 0
//       2 rc fixed bin,                               // 1
//       2 sockaddr_in,
//         3 sin_family fixed bin,                     // 2
//         3 sin_port fixed uns bin (16),              // 3
//         3 sin_addr,
//           4 octets (4) fixed bin(8) unsigned unal,  // 4
//       2 errno char(8);                              // 5, 6

    int socket_fd = (int) buffer[0];
sim_printf ("accept() socket     %d\n", socket_fd);
    // Does this socket belong to us?
    if (sk_data.fd_unit[socket_fd] != unit_num)
      {
        set_error (& buffer[4], EBADF);
        return 2; // send terminate interrupt
      }
    //FD_SET (socket_fd, & sk_data.unit_data[unit_num].accept_fds);
    sk_data.unit_data[unit_num].accept_fd = socket_fd;
    sk_data.unit_data[unit_num].unit_state = unit_accept;
    return 3; // don't send terminate interrupt
  }

static void skt_close (int unit_num, word36 * buffer)
  {
// dcl 1 SOCKETDEV_close_data aligned,
//       2 sockfd fixed bin,  // 0
//       2 rc fixed bin,      // 1
//       2 errno char(8);     // 2, 3
// 
// /* Tally 4 */
// /* In: */
// /*   sockfd */
// /* Out: */
// /*   rc */
// /*   errno */

    int socket_fd = (int) buffer[0];
sim_printf ("close() socket     %d\n", socket_fd);

    int rc = 0;
    int _errno = 0;
    // Does this socket belong to us?
    if (sk_data.fd_unit[socket_fd] != unit_num)
      {
sim_printf ("close() socket doesn't belong to us\n");
        _errno = EBADF;
        goto done;
      }
    sk_data.fd_unit[socket_fd] = -1;

    if (sk_data.unit_data[unit_num].unit_state == unit_accept &&
        sk_data.unit_data[unit_num].accept_fd == socket_fd)
      {
        sk_data.unit_data[unit_num].unit_state = unit_idle;
        sk_data.unit_data[unit_num].accept_fd = -1;
      }
    rc = close (socket_fd);

sim_printf ("close() close returned %d\n", rc);
    if (rc < 0)
      {
        _errno = errno;
        goto done;
      }

done:
    buffer[1] = ((word36) ((word36s) rc)) & MASK36; // rc
    set_error (& buffer[2], _errno);
  }

static int skt_read8 (int unit_num, uint tally, word36 * buffer)
  {
// dcl 1 SOCKETDEV_read_data8 aligned,
//       2 sockfd fixed bin,                    // 0
//       2 count  fixed bin, /* buffer size */  // 1
//       2 rc     fixed bin,                    // 2
//       2 errno  char(8),                      // 3,4
//       2 buffer char (0 refer (SOCKETDEV_read_data9.count); // 5,....

/* Tally >= 5 */
/* In: */
/*   sockfd */
/*   count */
/* Out: */
/*   rc */
/*   buffer */

    int socket_fd = (int) buffer[0];
sim_printf ("read8() socket     %d\n", socket_fd);

    int rc = 0;
    int _errno = 0;
    // Does this socket belong to us?
    if (sk_data.fd_unit[socket_fd] != unit_num)
      {
sim_printf ("read8() socket doesn't belong to us\n");
        set_error (& buffer[4], EBADF);
        return 2; // send terminate interrupt
      }
    sk_data.unit_data[unit_num].read_fd = socket_fd;
    sk_data.unit_data[unit_num].read_buffer_sz = tally;
    sk_data.unit_data[unit_num].unit_state = unit_read;
    return 3; // don't send terminate interrupt

    buffer[1] = ((word36) ((word36s) rc)) & MASK36; // rc
    set_error (& buffer[2], _errno);
  }

static int skt_write8 (uint iom_unit_idx, uint chan, int unit_num, uint tally, word36 * buffer)
  {
    iom_chan_data_t * p = & iom_chan_data[iom_unit_idx][chan];
// dcl 1 SOCKETDEV_write_data8 aligned,
//       2 sockfd fixed bin,                    // 0
//       2 count  fixed bin, /* buffer size */  // 1
//       2 rc     fixed bin,                    // 2
//       2 errno  char(8),                      // 3,4
//       2 buffer char (0 refer (SOCKETDEV_read_data9.count); // 5,....

    if (tally < 5)
      {
        p->stati = 050012; // BUG: arbitrary error code; config switch
        return -1;
      }

/* Tally >= 5 */
/* In: */
/*   sockfd */
/*   count */
/* Out: */
/*   rc */
/*   errno */
/*   buffer */

    int socket_fd = (int) buffer[0];
sim_printf ("write8() socket     %d\n", socket_fd);

    ssize_t rc = 0;
    int _errno = 0;
    // Does this socket belong to us?
    if (sk_data.fd_unit[socket_fd] != unit_num)
      {
sim_printf ("write8() socket doesn't belong to us\n");
        set_error (& buffer[4], EBADF);
        return 2; // send terminate interrupt
      }

   // Tally is at most 4096, so buffer words is at most 4096 - 5 => 4091
   // count (4 chars/word) is at most 4091 * 4 
    word36 count36 = buffer[1];
    if (count36 > (4091 * 4))
      {
        p->stati = 050012; // BUG: arbitrary error code; config switch
        return -1;
      }
    uint count = (uint) count36;
 
    uint count_words = (count + 3) / 4;
    if ((count_words + 5) > tally)
      {
        p->stati = 050012; // BUG: arbitrary error code; config switch
        return -1;
      }

    uint8_t netdata [count];
    for (uint n = 0; n < count; n ++)
      {
         uint wordno = (uint) n / 4;
         uint charno = (uint) n % 4;
         netdata[n] = getbits36_8 (buffer [5 + wordno], charno * 9 + 1);
//sim_printf ("%012llo %u %u %u %03u\n", buffer [5 + wordno], n, wordno, charno, netdata[n]);
      }

    rc = write (socket_fd, netdata, count);
    if (rc == -1)
      _errno = errno;

    buffer[2] = ((word36) ((word36s) rc)) & MASK36; // rc
    set_error (& buffer[3], _errno);
    return 2; // send terminate interrupt
  }

static int get_ddcw (iom_chan_data_t * p, uint iom_unit_idx, uint chan, bool * ptro, uint expected_tally, uint * tally)
  {
    bool send, uff;
    int rc = iom_list_service (iom_unit_idx, chan, ptro, & send, & uff);
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
        p->stati = 05001; // BUG: arbitrary error code; config switch
        return 1;
      }
    if (p->DCW_18_20_CP == 07 || p->DDCW_22_23_TYPE == 2)
      {
        sim_warn ("%s expected DDCW\n", __func__);
        p->stati = 05001; // BUG: arbitrary error code; config switch
        return -1;
      }

    * tally = p->DDCW_TALLY;
    if (* tally == 0)
      {
        sim_debug (DBG_DEBUG, & skc_dev,
                   "%s: Tally of zero interpreted as 010000(4096)\n",
                   __func__);
        * tally = 4096;
      }

    sim_debug (DBG_DEBUG, & skc_dev,
               "%s: Tally %d (%o)\n", __func__, * tally, * tally);

    if (expected_tally && * tally && * tally != expected_tally)
      {
        sim_warn ("socket_dev socket call expected tally of %d; got %d\n", expected_tally, * tally);
        p->stati = 05001; // BUG: arbitrary error code; config switch
        return -1;
      }
    return 0;
  }

static int sk_cmd (uint iom_unit_idx, uint chan)
  {
    iom_chan_data_t * p = & iom_chan_data[iom_unit_idx][chan];

    sim_debug (DBG_DEBUG, & skc_dev, "IDCW_DEV_CODE %d\n", p->IDCW_DEV_CODE);
    //struct device * d = & cables->cablesFromIomToDev [iom_unit_idx].devices[chan][p->IDCW_DEV_CODE];
    //uint devUnitIdx = d->devUnitIdx;
    //UNIT * unitp = & sk_unit[devUnitIdx];
sim_printf ("device %u\n", p->IDCW_DEV_CODE);
    bool ptro;
    switch (p->IDCW_DEV_CMD)
      {
        case 0: // CMD 00 Request status -- controller status, not device
          {
            p->stati = 04000; // have_status = 1
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: Request status: %04o\n", __func__, p->stati);
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: Request status control: %o\n", __func__, p->IDCW_CONTROL);
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: Request status channel command: %o\n", __func__, p->IDCW_CHAN_CMD);
          }
          break;

        case 01:               // CMD 01 -- socket()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$socket\n", __func__);
            const int expected_tally = 6;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [expected_tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            skt_socket ((int) p->IDCW_DEV_CODE, buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);
          }
          break;

        case 02:               // CMD 02 -- bind()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$bind\n", __func__);

            const int expected_tally = 6;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [expected_tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            skt_bind ((int) p->IDCW_DEV_CODE, buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);

          }
          break;

        case 03:               // CMD 03 -- Debugging
          {
            sim_printf ("socket_dev received command 3\r\n");
            p->stati = 04000;
          }
          break;

        case 04:               // CMD 04 -- gethostbyname()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$gethostbyname\n", __func__);

            const int expected_tally = 68;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [expected_tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            skt_gethostbyname (buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);

          }
          break;

        case 05:               // CMD 05 -- listen()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$listen\n", __func__);

            const int expected_tally = 5;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [expected_tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            skt_listen ((int) p->IDCW_DEV_CODE, buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);

          }
          break;

        case 06:               // CMD 06 -- accept()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$accept\n", __func__);

            const int expected_tally = 7;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [expected_tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            rc = skt_accept ((int) p->IDCW_DEV_CODE, buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);

            return rc; // 3:command pending, don't send terminate interrupt, or
                       // 2:sent terminate interrupt
          }
          break;

        case 07:               // CMD 07 -- close()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$close\n", __func__);

            const int expected_tally = 4;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [expected_tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            skt_close ((int) p->IDCW_DEV_CODE, buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);
          }
          break;

        case 8:               // CMD 8 -- read8()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$read8\n", __func__);

            const int expected_tally = 0;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            skt_read8 ((int) p->IDCW_DEV_CODE, tally, buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);
          }
          break;

        case 9:               // CMD 9 -- write8()
          {
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: socket_dev_$read8\n", __func__);

            const int expected_tally = 0;
            uint tally;
            int rc = get_ddcw (p, iom_unit_idx, chan, & ptro, expected_tally, & tally);
            if (rc)
              return rc;

            // Fetch parameters from core into buffer

            word36 buffer [tally];
            uint words_processed;
            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, false);

            skt_write8 (iom_unit_idx, chan, (int) p->IDCW_DEV_CODE, tally, buffer);

            iom_indirect_data_service (iom_unit_idx, chan, buffer,
                                       & words_processed, true);
          }
          break;

        case 040:               // CMD 040 -- Reset Status
          {
            p->stati = 04000;
            sim_debug (DBG_DEBUG, & skc_dev,
                       "%s: Reset status is %04o.\n",
                       __func__, p->stati);
            return 0;
          }

        default:
          {
            p->stati = 04501;
            p->chanStatus = chanStatIncorrectDCW;
            sim_warn ("%s: Unknown command 0%o\n", __func__, p->IDCW_DEV_CMD);
          }
          break;

      } // IDCW_DEV_CMD

    sim_debug (DBG_DEBUG, & skc_dev, "stati %04o\n", p->stati);

#if 0
    if (p->IDCW_CONTROL == 3) // marker bit set
      {
        send_marker_interrupt (iom_unit_idx, (int) chan);
      }
#endif
    return 2; // don't continue down the dcw list.
  }


int skc_iom_cmd (uint iom_unit_idx, uint chan)
  {
    iom_chan_data_t * p = & iom_chan_data[iom_unit_idx] [chan];
// Is it an IDCW?

    int rc = 0;
    if (p->DCW_18_20_CP == 7)
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

static void do_try_accept (uint unit_num)
  {
    struct sockaddr_in from;
    socklen_t size = sizeof (from);
    int _errno = 0;
    int fd = accept (sk_data.unit_data[unit_num].accept_fd, (struct sockaddr *) & from, & size);
    if (fd == -1)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return;
        _errno = errno;
      }
    else if (fd < N_FDS)
      {
        sk_data.fd_unit[fd] = (int) unit_num;
        sk_data.fd_nonblock[fd] = false ; // !! (type & SOCK_NONBLOCK);
      }
    else
      {
        close (fd);
        fd = -1;
        _errno = EMFILE;
      }
    word36 buffer [7];
    // sign extend int into word36
    buffer[0] = ((word36) ((word36s) sk_data.unit_data[unit_num].accept_fd)) & MASK36; 
    buffer[1] = ((word36) ((word36s) fd)) & MASK36; 
    buffer[2] = ((word36) ((word36s) from.sin_family)) & MASK36; 
    uint16_t port = ntohs (from.sin_port);
    putbits36_16 (& buffer[3], 0, port);
    uint32_t addr = ntohl (from.sin_addr.s_addr);
    buffer[4] = ((word36) addr) << 4;
    set_error (& buffer[5], _errno);
    // This makes me nervous; it is assuming that the decoded channel control
    // list data for the channel is intact, and that buffer is still in place.
    uint iom_unit_idx = (uint) cables->sk_to_iom[unit_num][0].iom_unit_idx;
    uint chan = (uint) cables->sk_to_iom[unit_num][0].chan_num;
    uint words_processed;
    iom_indirect_data_service (iom_unit_idx, chan, buffer,
                               & words_processed, true);
    iom_chan_data_t * p = & iom_chan_data[iom_unit_idx][chan];
    p->stati = 04000;
    send_terminate_interrupt (iom_unit_idx, chan);
  }

static void do_try_read (uint unit_num)
  {
    int _errno = 0;
    uint tally = sk_data.unit_data[unit_num].read_buffer_sz;
    uint tally_wds = (tally + 3) / 4;
    word36 buffer [tally_wds];
    uint8_t netdata [tally];
    ssize_t nread = read (sk_data.unit_data[unit_num].read_fd, & netdata, tally);
    if (nread == -1)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return;
        _errno = errno;
      }

    // sign extend int into word36
    buffer[0] = ((word36) ((word36s) sk_data.unit_data[unit_num].read_fd)) & MASK36; 
    buffer[1] = ((word36) (sk_data.unit_data[unit_num].read_buffer_sz)) & MASK36; 
    buffer[2] = ((word36) ((word36s) nread)) & MASK36; 
    set_error (& buffer[3], _errno);

    for (ssize_t n = 0; n < nread; n ++)
      {
         uint wordno = (uint) n / 4;
         uint charno = (uint) n % 4;
         putbits36_9 (& buffer [5 + wordno], charno * 9, (word9) netdata [n]);
      }

    // This makes me nervous; it is assuming that the decoded channel control
    // list data for the channel is intact, and that buffer is still in place.
    uint iom_unit_idx = (uint) cables->sk_to_iom[unit_num][0].iom_unit_idx;
    uint chan = (uint) cables->sk_to_iom[unit_num][0].chan_num;
    uint words_processed;
    iom_indirect_data_service (iom_unit_idx, chan, buffer,
                               & words_processed, true);
    send_terminate_interrupt (iom_unit_idx, chan);
  }

void sk_process_event (void)
  {
// Accepts
    for (uint unit_num = 0; unit_num < N_SK_UNITS_MAX; unit_num ++)
      {
        if (sk_data.unit_data[unit_num].unit_state == unit_accept)
          {
            do_try_accept (unit_num);
          }
        else if (sk_data.unit_data[unit_num].unit_state == unit_read)
          {
            do_try_read (unit_num);
          }
      }
  }


