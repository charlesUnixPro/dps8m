/*
 Copyright 2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Largely borrowed from SIMH h316_udp.c 

/* h316_udp.c: IMP/TIP Modem and Host Interface socket routines using UDP

   Copyright (c) 2013 Robert Armstrong, bob@jfcl.com

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT ARMSTRONG BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Robert Armstrong shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Robert Armstrong.

*/

/*

  INTERFACE

   This module provides a simplified UDP socket interface.  These functions are
   implemented -

        fnp_udp_create      define a connection to the remote FNP
        fnp_udp_release     release a connection
        fnp_udp_send        send an FNP message to the other end
        fnp_udp_receive     receive (w/o blocking!) a message if available

   Note that each connection is assigned a unique "handle", a small integer,
   which is used as an index into our internal connection data table.  There
   is a limit on the maximum number of connections available, as set my the
   MAXLINKS parameter.  Also, notice that all links are intrinsically full
   duplex and bidirectional - data can be sent and received in both directions
   independently.  Real modems and host cards were exactly the same.

*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h> 
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <netdb.h>
#include <errno.h>


#include "fnp_udplib.h"

// Local constants ...
#define MAXLINKS        10      // maximum number of simultaneous connections
//   This constant determines the longest possible FNP data payload that can be
// sent.

// UDP connection data structure ...
//   One of these blocks is allocated for every simulated modem link. 
struct _FNP_UDP_LINK
  {
    bool  used;                 // TRUE if this FNP_UDP_LINK is in use
    char    rhost [64];
    char    rport [64];        // Remote host:port
    char    lport [64];            // Local port 
    int32_t   lportno;
    int32_t   rportno;
    int sock;
    uint32_t  rxsequence;           // next message sequence number for receive
    uint32_t  txsequence;           // next message sequence number for transmit
  };

typedef struct _FNP_UDP_LINK FNP_UDP_LINK;

//   This magic number is stored at the beginning of every UDP message and is
// checked on receive.  It's hardly foolproof, but its a simple attempt to
// guard against other applications dumping unsolicited UDP messages into our
// receiver socket...
// 'FNPx'
#define MAGIC   ((uint32_t) (((((('F' << 8) | 'N') << 8) | 'P') << 8) | 'x'))

// UDP wrapper data structure ...
//   This is the UDP packet which is actually transmitted or received.  It
// contains the actual FNP  packet, plus whatever additional information we
// need to keep track of things.  NOTE THAT ALL DATA IN THIS PACKET
// ARE SENT AND RECEIVED WITH NETWORK BYTE ORDER!
struct _FNP_UDP_PACKET {
  uint32_t  magic;                // UDP "magic number" (see above)
  uint32_t  sequence;             // UDP packet sequence number
  uint16_t  count;                // number of bytes to follow (flags * data)
  uint16_t  flags;                // 
  char      data[FNP_MAXDATA];        // and the actual FNP packet
};
typedef struct _FNP_UDP_PACKET FNP_UDP_PACKET;
#define FNP_UDP_HEADER_LEN  (2*sizeof(uint32_t) + sizeof(uint16_t))

static FNP_UDP_LINK fnp_udp_links [MAXLINKS];

int sim_parse_addr (const char * cptr, char * host, size_t hostlen, const char * default_host, char * port, size_t port_len, const char * default_port, const char * validate_addr);

static int fnp_udp_parse_remote (int link, char * premote)
  {
    // This routine will parse a remote address string in any of these forms -
    //
    //            llll:w.x.y.z:rrrr
    //            llll:name.domain.com:rrrr
    //            llll::rrrr
    //            w.x.y.z:rrrr
    //            name.domain.com:rrrr
    //
    // In all examples, "llll" is the local port number that we use for listening,
    // and "rrrr" is the remote port number that we use for transmitting.  The
    // local port is optional and may be omitted, in which case it defaults to the
    // same as the remote port.  This works fine if the other end is actually on
    // a different host, but don't try that with localhost - you'll be talking to
    // yourself!!  In both cases, "w.x.y.z" is a dotted IP for the remote machine
    // and "name.domain.com" is its name (which will be looked up to get the IP).
    // If the host name/IP is omitted then it defaults to "localhost".
  
    char * end;
    int32_t lportno, rport;
    char host [64], port [16];
    if (* premote == '\0')
      return -1;
    memset (fnp_udp_links [link] . lport, 0, sizeof (fnp_udp_links [link] . lport));
    memset (fnp_udp_links [link] . rhost, 0, sizeof (fnp_udp_links [link] . rhost));
    memset (fnp_udp_links [link] . rport, 0, sizeof (fnp_udp_links [link] . rport));
    // Handle the llll::rrrr case first
    if (2 == sscanf (premote, "%d::%d", & lportno, & rport))
      {
        if ((lportno < 1) || (lportno >65535) || (rport < 1) || (rport >65535))
         return -1;
        sprintf (fnp_udp_links [link] . lport, "%d", lportno);
        fnp_udp_links [link] . lportno =  lportno;
        sprintf (fnp_udp_links [link] . rhost, "localhost");
        sprintf (fnp_udp_links [link] . rport, "%d", rport);
        fnp_udp_links [link] . rportno = rport;
        return 0;
      }

    // Look for the local port number and save it away.
    lportno = (int) strtoul (premote, & end, 10);
    if ((* end == ':') && (lportno > 0))
      {
        sprintf (fnp_udp_links [link] . lport, "%d", lportno);
        fnp_udp_links [link] . lportno =  lportno;
        premote = end + 1;
      }
  
    if (sim_parse_addr (premote, host, sizeof (host), "localhost", port, sizeof (port), NULL, NULL) != -1 /*SCPE_OK*/)
      return -1;
    sprintf (fnp_udp_links [link] . rhost, "%s", host);
    sprintf (fnp_udp_links [link] . rport, "%s", port);
    fnp_udp_links [link] . rportno = atoi (port);
    if (fnp_udp_links [link] . lport [0] == '\0')
      {
        strcpy (fnp_udp_links [link] . lport, port);
        fnp_udp_links [link] . lportno =  atoi (port);
      }
    if ((strcmp (fnp_udp_links [link] . lport, port) == 0) &&
        (strcmp ("localhost", host) == 0))
      fprintf (stderr, "WARNING - use different transmit and receive ports!\n");
  
    return 0;
  }

static int fnp_udp_find_free_link (void)
{
  //   Find a free FNP_UDP_LINK block, initialize it and return its index.  If none
  // are free, then return -1 ...
  int i;
  for (i = 0;  i < MAXLINKS; i ++)
    {
      if (fnp_udp_links [i] . used == 0)
        {
          memset (& fnp_udp_links [i], 0, sizeof (FNP_UDP_LINK));
          return i;
        }
     }
  return FNP_NOLINK;
}

// return values
// 0: ok
// -1: out of links

int fnp_udp_create (char * premote, int * pln)
  {
    int rc;
    //   Create a logical UDP link to the specified remote system.  The "remote"
    // string specifies both the remote host name or IP and a port number.  The
    // port number is both the port we send datagrams to, and also the port we
    // listen on for incoming datagrams.  UDP doesn't have any real concept of a
    // "connection" of course, and this routine simply creates the necessary
    // sockets in this host. We have no way of knowing whether the remote host is
    // listening or even if it exists.
    //
    //   We return SCPE_OK if we're successful and an error code if we aren't. If
    // we are successful, then the ln parameter is assigned the link number,
    // which is a handle used to identify this connection to all future fnp_udp_xyz()
    //  calls.

    int link = fnp_udp_find_free_link ();
    if (link < 0)
      return -1; // out of links

    // Parse the remote name and set up the ipaddr and port ...
    if (fnp_udp_parse_remote (link, premote))
      return -2;

    int sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1)
      return -3;

    rc = fcntl (sock, F_SETFL, fcntl (sock, F_GETFL, 0) | O_NONBLOCK);
    if (sock == -1)
      return -4;

    struct sockaddr_in si_me;
    memset ((char *) & si_me, 0, sizeof (si_me));
 
    si_me . sin_family = AF_INET;
    si_me . sin_port = htons (fnp_udp_links [link] . lportno);
    si_me . sin_addr . s_addr = htonl (INADDR_ANY);
     
    rc = bind (sock, (struct sockaddr *) & si_me, sizeof (si_me));
    if (rc == -1)
      return -5;

// As I understand it, a connect on UDP sets the send address and limits
// recieving to the specifed address; creating a 'connection'; I am not
// sure the fnp_udplib wants that. The alternative is to use sendto().

printf ("rhost %s rport %s\n", fnp_udp_links [link] . rhost, fnp_udp_links [link] . rport);
    struct addrinfo * ai;
    rc = getaddrinfo (fnp_udp_links [link] . rhost, fnp_udp_links [link] . rport, NULL,
      & ai);
    if (rc == -1)
      return -6;

    rc = connect (sock, ai -> ai_addr, sizeof (struct sockaddr));
    if (rc == -1)
      {
        perror ("connect");
        return -7;
      }

    freeaddrinfo (ai);

    fnp_udp_links [link] . sock = sock;

    // All done - mark the TCP_LINK data as "used" and return the index.
     fnp_udp_links [link] . used = true;
     * pln = link;
printf ("link %d - listening on port %s and sending to %s:%s\n", link, fnp_udp_links [link] . lport, fnp_udp_links [link] . rhost, fnp_udp_links [link] . rport);

    return 0;
  }

int fnp_udp_release (int link)
  {
    //   Close a link that was created by fnp_udp_create() and release any resources
    // allocated to it.  We always return SCPE_OK unless the link specified is
    // already unused.
    if ((link < 0) || (link >= MAXLINKS))
      return -1;
    if (! fnp_udp_links [link] . used)
      return -1;
    //if (dptr != fnp_udp_links [link] . dptr)
      //return -1;

    //tmxr_detach_ln (&fnp_udp_lines[link]);
    close (fnp_udp_links [link] . sock);
    fnp_udp_links [link] . used = false;
    //sim_debug(IMP_DBG_UDP, dptr, "link %d - closed\n", link);
printf("link %d - closed\n", link);

    return 0;
  }

int fnp_udp_send (int link, char * pdata, uint16_t count, uint16_t flags)
  {
//sim_printf ("fnp_udp_send link %d\n", link);
    //   This routine does all the work of sending an FNP data packet.  pdata
    // is a pointer the FNP packet data,
    // count is the length of the data bytes, and pdest is
    // the destination socket.  There are two things worthy of note here - first,
    // notice that the datawords are sent in network order, so the remote simh
    // doesn't necessarily need to have the same endian-ness as this machine.
    // Second, notice that transmitting sockets are NOT set to non blocking so
    // this routine might wait, but we assume the wait will never be too long.

    FNP_UDP_PACKET pkt;
    int pktlen;
    uint16_t i;

    if ((link < 0) || (link >= MAXLINKS))
      return -1;
    if (! fnp_udp_links [link] . used)
      return -1;
    if ((pdata == NULL) || (count == 0) || (count > FNP_MAXDATA))
      return -1;
  
    //   Build the UDP packet, filling in our own header information and copying
    // the H316 words from memory.  REMEMBER THAT EVERYTHING IS IN NETWORK ORDER!
    pkt . magic = htonl (MAGIC);
    pkt . sequence = htonl (fnp_udp_links [link] . txsequence ++);
    pkt . count = htons (count);
    pkt . flags = htons (flags);
    for (i = 0; i < count; i ++)
      pkt . data [i] = (* pdata ++);
    pktlen = FNP_UDP_HEADER_LEN + count * sizeof (uint16_t);

    ssize_t rc = send (fnp_udp_links [link] . sock, & pkt, (size_t) pktlen, 0);
    if (rc == -1)
      {
        return -2;
      }
    //sim_debug(IMP_DBG_UDP, dptr, "link %d - packet sent (sequence=%d, length=%d)\n", link, ntohl(pkt.sequence), ntohs(pkt.count));
printf ("link %d - packet sent (sequence=%d, length=%d)\n", link, ntohl (pkt . sequence), ntohs (pkt . count));
    return 0;
  }

static int fnp_udp_receive_packet (int link, FNP_UDP_PACKET * ppkt, size_t pktsiz)
  {
    //   This routine will do the hard part of receiving a UDP packet.  If it's
    // successful the packet length, in bytes, is returned.  The receiver socket
    // is non-blocking, so if no packet is available then zero will be returned
    // instead.  Lastly, if a fatal socket I/O error occurs, -1 is returned.
    //
    //   Note that this routine only receives the packet - it doesn't handle any
    // of the checking for valid packets, unexpected packets, duplicate or out of
    // sequence packets.  That's strictly the caller's problem!

    ssize_t n = read (fnp_udp_links [link] . sock, ppkt, pktsiz);
    if (n < 0)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return 0;
        return -1;
      }
//printf ("fnp_udp_receive_packet returns %ld\n", n);
    return (int) n;
  }

int fnp_udp_receive (int link, char * pdata, uint16_t maxbuf, uint16_t * flags)
  {
    //   Receive an IMP packet from the virtual modem. pdata is a pointer usually
    // directly into H316 simulated memory) to where the IMP packet data should
    // be stored, and maxbuf is the maximum length of that buffer in H316 words
    // (not bytes!).  If a message is successfully received then this routine
    // returns the length, again in H316 words, of the IMP packet.  The caller
    // can detect buffer overflows by comparing this result to maxbuf.  If no
    // packets are waiting right now then zero is returned, and -1 is returned
    // in the event of any fatal socket I/O error.
    //
    //  This routine also handles checking for unsolicited messages and duplicate
    // or out of sequence messages.  All of these are unceremoniously discarded.
    //
    //   One final note - it's explicitly allowed for pdata to be null and/or
    // maxbuf to be zero.  In either case the received package is discarded, but
    // the actual length of the discarded package is still returned.
    FNP_UDP_PACKET pkt;
    int32_t pktlen, explen, implen, i;
    uint32_t magic, pktseq;
    if ((link < 0) || (link >= MAXLINKS))
      return -1;
    if (!fnp_udp_links [link] . used)
      return -1;
    //if (dptr != fnp_udp_links [link] . dptr)
      //return SCPE_IERR;
  
    while ((pktlen = fnp_udp_receive_packet (link, & pkt, sizeof (pkt))) > 0)
      {
        // First do some header checks for a valid UDP packet ...
        if (((size_t) pktlen) < FNP_UDP_HEADER_LEN)
          {
            //sim_debug(IMP_DBG_UDP, dptr, "link %d - received packet w/o header (length=%d)\n", link, pktlen);
            continue;
          }
        magic = ntohl (pkt . magic);
        if (magic != MAGIC)
          {
            //sim_debug(IMP_DBG_UDP, dptr, "link %d - received packet w/bad magic number (magic=%08x)\n", link, magic);
            continue;
          }
        implen = ntohs (pkt . count);
        explen = (int32_t) FNP_UDP_HEADER_LEN + implen * (int32_t) sizeof (uint16_t);
        if (explen != pktlen)
          {
            //sim_debug(IMP_DBG_UDP, dptr, "link %d - received packet length wrong (expected=%d received=%d)\n", link, explen, pktlen);
            continue;
          }
  
        //  Now the hard part = check the sequence number.  The rxsequence value is
        // the number of the next packet we expect to receive - that's the number
        // this packet should have.  If this packet's sequence is less than that,
        // then this packet is out of order or a duplicate and we discard it.  If
        // this packet is greater than that, then we must have missed one or two
        // packets.  In that case we MUST update rxsequence to match this one;
        // otherwise the two ends could never resynchronize after a lost packet.
        //
        //  And there's one final complication to worry about - if the simh on the
        // other end is restarted for some reason, then his sequence numbers will
        // reset to zero.  In that case we'll never recover synchronization without
        // special efforts.  The hack is to check for a packet sequence number of
        // zero and, if we find it, force synchronization.  This improves the
        // situation, but I freely admit that it's possible to think of a number of
        // cases where this also fails.  The only absolute solution is to implement
        // a more complicated system with non-IMP control messages exchanged between
        // the modem emulation on both ends.  That'd be nice, but I'll leave it as
        // an exercise for later.

        pktseq = ntohl (pkt . sequence);
        if ((pktseq == 0) && (fnp_udp_links [link] . rxsequence != 0))
          {
            //sim_debug(IMP_DBG_UDP, dptr, "link %d - remote modem restarted\n", link);
          }
        else if (pktseq < fnp_udp_links [link] . rxsequence)
          {
            //sim_debug(IMP_DBG_UDP, dptr, "link %d - received packet out of sequence 1 (expected=%d received=%d\n", link, fnp_udp_links[link].rxsequence, pktseq);
            continue;  // discard this packet!
          }
        else if (pktseq != fnp_udp_links [link] . rxsequence)
          {
            //sim_debug(IMP_DBG_UDP, dptr, "link %d - received packet out of sequence 2 (expected=%d received=%d\n", link, fnp_udp_links[link].rxsequence, pktseq);
          }
        fnp_udp_links [link] . rxsequence = pktseq + 1;
    
        // It's a valid packet - if there's no buffer then just discard it.
        if ((pdata == NULL) || (maxbuf == 0))
          {
            //sim_debug(IMP_DBG_UDP, dptr, "link %d - received packet discarded (no buffer available)\n", link);
            return implen;
          }
  
        // Copy the data to the H316 memory and we're done!
        //sim_debug (IMP_DBG_UDP, dptr, "link %d - packet received (sequence=%d, length=%d)\n", link, pktseq, pktlen);
printf ("link %d - packet received (sequence=%d, pktlen=%d)\n", link, pktseq, pktlen);
        for (i = 0;  i < (implen < maxbuf ? implen : maxbuf);  ++ i)
          * pdata ++ = pkt . data [i];
        * flags = ntohs (pkt . flags);
        return implen;
      }
  
    // Here if pktlen <= 0 ...
    return pktlen;
  }

//#define TEST
#ifdef TEST

#define CBUFSIZE        256
#define SCPE_ARG -1
#define SCPE_OK -1

/* sim_parse_addr       host:port

   Presumption is that the input, if it doesn't contain a ':' character is a port specifier.
   If the host field contains one or more colon characters (i.e. it is an IPv6 address), 
   the IPv6 address MUST be enclosed in square bracket characters (i.e. Domain Literal format)

   Inputs:
        cptr    =       pointer to input string
        default_host
                =       optional pointer to default host if none specified
        host_len =      length of host buffer
        default_port
                =       optional pointer to default port if none specified
        port_len =      length of port buffer
        validate_addr = optional name/addr which is checked to be equivalent
                        to the host result of parsing the other input.  This
                        address would usually be returned by sim_accept_conn.
   Outputs:
        host    =       pointer to buffer for IP address (may be NULL), 0 = none
        port    =       pointer to buffer for IP port (may be NULL), 0 = none
        result  =       status (SCPE_OK on complete success or SCPE_ARG if 
                        parsing can't happen due to bad syntax, a value is 
                        out of range, a result can't fit into a result buffer, 
                        a service name doesn't exist, or a validation name 
                        doesn't match the parsed host)
*/

int sim_parse_addr (const char *cptr, char *host, size_t host_len, const char *default_host, char *port, size_t port_len, const char *default_port, const char *validate_addr)
{
char gbuf[CBUFSIZE];
char *hostp, *portp;
char *endc;
unsigned long portval;

if ((cptr == NULL) || (*cptr == 0))
    return SCPE_ARG;
if ((host != NULL) && (host_len != 0))
    memset (host, 0, host_len);
if ((port != NULL) && (port_len != 0))
    memset (port, 0, port_len);
gbuf[sizeof(gbuf)-1] = '\0';
strncpy (gbuf, cptr, sizeof(gbuf)-1);
hostp = gbuf;                                           /* default addr */
portp = NULL;
if ((portp = strrchr (gbuf, ':')) &&                    /* x:y? split */
    (NULL == strchr (portp, ']'))) {
    *portp++ = 0;
    if (*portp == '\0')
        portp = (char *)default_port;
    }
else {                                                  /* No colon in input */
    portp = gbuf;                                       /* Input is the port specifier */
    hostp = (char *)default_host;                       /* host is defaulted if provided */
    }
if (portp != NULL) {
    portval = strtoul(portp, &endc, 10);
    if ((*endc == '\0') && ((portval == 0) || (portval > 65535)))
        return SCPE_ARG;                                /* numeric value too big */
    if (*endc != '\0') {
        struct servent *se = getservbyname(portp, "tcp");

        if (se == NULL)
            return SCPE_ARG;                            /* invalid service name */
        }
    }
if (port)                                               /* port wanted? */
    if (portp != NULL) {
        if (strlen(portp) >= port_len)
            return SCPE_ARG;                            /* no room */
        else
            strcpy (port, portp);
        }
if (hostp != NULL) {
    if (']' == hostp[strlen(hostp)-1]) {
        if ('[' != hostp[0])
            return SCPE_ARG;                            /* invalid domain literal */
        /* host may be the const default_host so move to temp buffer before modifying */
        strncpy(gbuf, hostp+1, sizeof(gbuf)-1);         /* remove brackets from domain literal host */
        hostp = gbuf;
        hostp[strlen(hostp)-1] = '\0';
        }
    }
if (host)                                               /* host wanted? */
    if (hostp != NULL) {
        if (strlen(hostp) >= host_len)
            return SCPE_ARG;                            /* no room */
        else
            strcpy (host, hostp);
        }
if (validate_addr) {
    struct addrinfo *ai_host, *ai_validate, *ai;
    int status;

    if (hostp == NULL)
        return SCPE_ARG;
    if (getaddrinfo(hostp, NULL, NULL, &ai_host))
        return SCPE_ARG;
    if (getaddrinfo(validate_addr, NULL, NULL, &ai_validate)) {
        freeaddrinfo (ai_host);
        return SCPE_ARG;
        }
    status = SCPE_ARG;
    for (ai = ai_host; ai != NULL; ai = ai->ai_next) {
        if ((ai->ai_addrlen == ai_validate->ai_addrlen) &&
            (ai->ai_family == ai_validate->ai_family) &&
            (0 == memcmp (ai->ai_addr, ai_validate->ai_addr, ai->ai_addrlen))) {
            status = SCPE_OK;
            break;
            }
        }
    if (status != SCPE_OK) {
        /* be generous and allow successful validations against variations of localhost addresses */
        if (((0 == strcmp("127.0.0.1", hostp)) &&
             (0 == strcmp("::1", validate_addr))) ||
            ((0 == strcmp("127.0.0.1", validate_addr)) &&
             (0 == strcmp("::1", hostp))))
            status = SCPE_OK;
        }
    freeaddrinfo (ai_host);
    freeaddrinfo (ai_validate);
    return status;
    }
return SCPE_OK;
}

int main (int argc, char * argv [])
  {
    int rc;
    int linkno;
    rc = fnp_udp_create ("4500::4426", & linkno);
    if (rc < 0)
      {
        printf ("fnp_udp_create failed\n");
        exit (1);
      }

    while (1)
      {
#define psz 17000
        uint16_t pkt [psz];
        rc = fnp_udp_receive (linkno, pkt, psz);
        if (rc < 0)
          {
            printf ("fnp_udp_receive failed\n");
            exit (1);
          }
        else if (rc == 0)
          {
            printf ("fnp_udp_receive 0\n");
            sleep (1);
          }
        else
          {
            for (int i = 0; i < rc; i ++)
              {
                printf ("  %06o  %04x  ", pkt [i], pkt [i]);
                for (int b = 0; b < 16; b ++)
                  printf ("%c", pkt [i] & (1 << b) ? '1' : '0');
                printf ("\n");
              }
          }
      }
  }
#endif
