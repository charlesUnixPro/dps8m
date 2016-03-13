#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "dps8.h"
#include "dps8_utils.h"
#include "dps8_fnp.h"
#include "fnp_cmds.h"
#include "fnp_2.h"
#include "fnp_mux.h"
#include "utlist.h"

void fTCPinit (int lineno)
  {
    // Multics has issued a 'listen' command to the line. Use it as a
    // signal to make sure every thing is tidy.
    MState.line[lineno].fTCP.nbytes = 0;
    MState.line[lineno].fTCP.sockfd = -1;
  }

static void reply (int lineno, char * msg)
  {
    sendInputLine (lineno, msg, strlen (msg), true);
  }

static void fTCPcmd (int lineno, struct fTCP_t * s)
  {
    s->inbuf [s->nbytes] = 0;
    //sim_printf ("fTCP '%s'\n", s->inbuf);
    char copy [strlen (s->inbuf) + 1];
    char * savptr;
    strcpy (copy, s->inbuf);

    char * token = strtok_r (copy, " ", & savptr);

    if (token == NULL)
      return;

    if (strcmp (token, "c_socket") == 0)
      {

        // "c_socket address portno"
        //sim_printf ("doing client socket command\n");

        // parse address
        token = strtok_r (NULL, " ", & savptr);
        if (token == NULL)
          {
            reply (lineno, "error address missing\n");
            return;
          }

        char addr [strlen (token) + 1];
        strcpy (addr, token);

        // parse portno
        token = strtok_r (NULL, " ", & savptr);
        if (token == NULL)
          {
            reply (lineno, "error portno missing\n");
            return;
          }
        s->portno = atoi (token);
        if (s->portno <= 0)
          {
            reply (lineno, "error invalid portno\n");
            return;
          }

        // create the socket
        s->sockfd = socket (AF_INET, SOCK_STREAM, 0);
        if (s->sockfd < 0)
          {
            char msg [257];
            sprintf (msg, "error socket() errno %d\n", errno);
            reply (lineno, msg);
            return;
          }

        // lookup address
        // XXX This is bad; gethostbyname can take a long time and will
        // XXX stall the emulator
        s->server = gethostbyname (addr);
        if (s->server == NULL)
          {
            reply (lineno, "error invalid address\n");
            close (s->sockfd);
            s->sockfd = -1;
            return;
          }

        // connect
        bzero (& s->serv_addr, sizeof (s->serv_addr));
        s->serv_addr.sin_family = AF_INET;
        bcopy (s->server->h_addr,
               & s->serv_addr.sin_addr.s_addr,
               s->server->h_length);
        s->serv_addr.sin_port = htons (s->portno);
        int rc = connect (s->sockfd,
                          (struct sockaddr *) & s->serv_addr,
                          sizeof (s->serv_addr)); 
        if (rc < 0)
          {
            char msg [257];
            sprintf (msg, "error connect() errno %d\n", errno);
            reply (lineno, msg);
            close (s->sockfd);
            s->sockfd = -1;
            return;
          }
        reply (lineno, "ok");
      }
    // write "string"
    else if (strcmp (token, "write") == 0)
      {
        // parse string
        char * quote = savptr;
        while (* quote && * quote == ' ') // skip leading spaces
          quote ++;
        if (* quote != '"')
          {
            reply (lineno, "error can't find open quote\n");
            return;
          }
        quote ++;

        if (strlen (quote) == 0)
          {
            reply (lineno, "error can't find close quote\n");
            return;
          }
        char * last = quote + strlen (quote) - 1;
        while (last >= quote && * last == ' ')
          last --;
        if (* last != '"')
          {
            reply (lineno, "error can't find close quote\n");
            return;
          }
        * last = 0;
        char * escaped = strdupesc (quote);
        if (escaped == NULL)
          {
            reply (lineno, "error strdupesc failed\n");
            return;
          }

        uint len = strlen (escaped);
        
        // XXX Can block
        int n = write (s->sockfd, escaped, len);
        if (n < 0)
          {
            char msg [257];
            sprintf (msg, "error write() errno %d\n", errno);
            reply (lineno, msg);
            close (s->sockfd);
            s->sockfd = -1;
            return;
          }

        free (escaped);
        reply (lineno, "ok\n");
      }
    else if (strcmp (token, "read") == 0)
      {
        fd_set input;
        FD_ZERO (& input);
        FD_SET (s->sockfd, & input);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        int n = select (s->sockfd + 1, & input, NULL, NULL, & timeout);
        if (n < 0)
          {
            reply (lineno, "error select failed\n");
            return;
          }
        if (n == 0)
          {
            reply (lineno, "read 0 \"\"\n");
            return;
          }
        char buffer [256];
        bzero (buffer, 256);
        n = read (s-> sockfd, buffer, 255);
        if (n < 0)
          {
            char msg [257];
            sprintf (msg, "error read() errno %d\n", errno);
            reply (lineno, msg);
            return;
          }
        char msg [512];
        sprintf (msg, "read %lu \"%s\"\n", strlen (buffer), buffer);
        reply (lineno, msg);
      }
    else
      {
        reply (lineno, "error unrecognized command\n");
      }
  }

void fTCP (int lineno, int nbytes, char * data)
  {
    //sim_printf ("fTCP: %d %o\n", nbytes, data [0]);

    struct fTCP_t * s = & MState.line[lineno].fTCP;
    if (s->nbytes + nbytes >= (int) sizeof (s->inbuf))
      {
        reply (lineno, "error inbuf overflow\n");
        s->nbytes = 0; // discard what we have
        return;
      }
    int where = 0;
    while (1)
      {
        for ( ; where < nbytes; where ++)
          {
            if (data[where] == '\r' ||
                data[where] == '\n')
              break;
            s->inbuf[s->nbytes++] = data[where];
          }
        if (where < nbytes) // found an EOL
          {
            fTCPcmd (lineno, s);
            s->nbytes = 0;
            where ++; // discard the EOL
          }
        else
          break;
      }


   
  }
