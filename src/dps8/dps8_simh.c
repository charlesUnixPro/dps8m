/*
 Copyright 2018 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_simh.h"


#ifdef COLOR
//                FG BG
// Black          30 40
// Red            31 41
// Green          32 42
// Yellow         33 43
// Blue           34 44
// Magenta        35 45
// Cyan           36 46
// White          37 47
// Bright Black   90 100
// Bright Red     91 101
// Bright Green   92 102
// Bright Yellow  93 103
// Bright Blue    94 104
// Bright Magenta 95 105
// Bright Cyan    96 106
// Bright White   97 107
// Reset all       0

#define GRN "\033[32m"
#define WHT "\033[97m"
#define YEL "\033[33m"
#define RST "\033[0m"

void sim_msg (const char * fmt, ...)
  {
    char stackbuf[STACKBUFSIZE];
    int32 bufsize = sizeof (stackbuf);
    char * buf = stackbuf;
    int32 len;
    va_list arglist;

    /* format passed string, args */
    while (1) 
      {
        va_start (arglist, fmt);
#if defined(NO_vsnprintf)
        len = vsprintf (buf, fmt, arglist);
#else                                               /* !defined(NO_vsnprintf) */
        len = vsnprintf (buf, (unsigned long) bufsize-1, fmt, arglist);
#endif                                              /* NO_vsnprintf */
        va_end (arglist);

/* If the formatted result didn't fit into the buffer, then grow the buffer and try again */

        if ((len < 0) || (len >= bufsize-1))
          {
            if (buf != stackbuf)
                free (buf);
            bufsize = bufsize * 2;
            if (bufsize < len + 2)
                bufsize = len + 2;
            buf = (char *) malloc ((unsigned long) bufsize);
            if (buf == NULL)                            /* out of memory */
                return;
            buf[bufsize-1] = '\0';
            continue;
          }
        break;
      }

    if (! sys_opts.no_color)
      printf (GRN);

    if (sim_is_running)
      {
        char *c, *remnant = buf;

        while ((c = strchr(remnant, '\n')))
          {
            if ((c != buf) && (*(c - 1) != '\r'))
              printf("%.*s\r\n", (int)(c-remnant), remnant);
            else
              printf("%.*s\n", (int)(c-remnant), remnant);
            remnant = c + 1;
          }
        printf("%s", remnant);
      }
    else
      printf("%s", buf);

    if (! sys_opts.no_color)
      sim_printf (RST);

    if (sim_log && (sim_log != stdout))
      fprintf (sim_log, "%s", buf);
    if (sim_deb && (sim_deb != stdout) && (sim_deb != sim_log))
      fprintf (sim_deb, "%s", buf);

    if (buf != stackbuf)
      free (buf);
  }

void sim_warn (const char * fmt, ...)
  {
    char stackbuf[STACKBUFSIZE];
    int32 bufsize = sizeof (stackbuf);
    char * buf = stackbuf;
    int32 len;
    va_list arglist;

    /* format passed string, args */
    while (1) 
      {
        va_start (arglist, fmt);
#if defined(NO_vsnprintf)
        len = vsprintf (buf, fmt, arglist);
#else                                               /* !defined(NO_vsnprintf) */
        len = vsnprintf (buf, (unsigned long) bufsize-1, fmt, arglist);
#endif                                              /* NO_vsnprintf */
        va_end (arglist);

/* If the formatted result didn't fit into the buffer, then grow the buffer and try again */

        if ((len < 0) || (len >= bufsize-1))
          {
            if (buf != stackbuf)
                free (buf);
            bufsize = bufsize * 2;
            if (bufsize < len + 2)
                bufsize = len + 2;
            buf = (char *) malloc ((unsigned long) bufsize);
            if (buf == NULL)                            /* out of memory */
                return;
            buf[bufsize-1] = '\0';
            continue;
          }
        break;
      }

    if (! sys_opts.no_color)
      printf (YEL);

    if (sim_is_running)
      {
        char *c, *remnant = buf;

        while ((c = strchr(remnant, '\n')))
          {
            if ((c != buf) && (*(c - 1) != '\r'))
              printf("%.*s\r\n", (int)(c-remnant), remnant);
            else
              printf("%.*s\n", (int)(c-remnant), remnant);
            remnant = c + 1;
          }
        printf("%s", remnant);
      }
    else
      printf("%s", buf);

    if (! sys_opts.no_color)
      printf (RST);

    if (sim_log && (sim_log != stdout))
      fprintf (sim_log, "%s", buf);
    if (sim_deb && (sim_deb != stdout) && (sim_deb != sim_log))
      fprintf (sim_deb, "%s", buf);

    if (buf != stackbuf)
      free (buf);
  }

void sim_print (const char * fmt, ...)
  {
    char stackbuf[STACKBUFSIZE];
    int32 bufsize = sizeof (stackbuf);
    char * buf = stackbuf;
    int32 len;
    va_list arglist;

    /* format passed string, args */
    while (1) 
      {
        va_start (arglist, fmt);
#if defined(NO_vsnprintf)
        len = vsprintf (buf, fmt, arglist);
#else                                               /* !defined(NO_vsnprintf) */
        len = vsnprintf (buf, (unsigned long) bufsize-1, fmt, arglist);
#endif                                              /* NO_vsnprintf */
        va_end (arglist);

/* If the formatted result didn't fit into the buffer, then grow the buffer and try again */

        if ((len < 0) || (len >= bufsize-1))
          {
            if (buf != stackbuf)
                free (buf);
            bufsize = bufsize * 2;
            if (bufsize < len + 2)
                bufsize = len + 2;
            buf = (char *) malloc ((unsigned long) bufsize);
            if (buf == NULL)                            /* out of memory */
                return;
            buf[bufsize-1] = '\0';
            continue;
          }
        break;
      }

    if (! sys_opts.no_color)
      printf (RST);

    if (sim_is_running)
      {
        char *c, *remnant = buf;

        while ((c = strchr(remnant, '\n')))
          {
            if ((c != buf) && (*(c - 1) != '\r'))
              printf("%.*s\r\n", (int)(c-remnant), remnant);
            else
              printf("%.*s\n", (int)(c-remnant), remnant);
            remnant = c + 1;
          }
        printf("%s", remnant);
      }
    else
      printf("%s", buf);

    if (sim_log && (sim_log != stdout))
      fprintf (sim_log, "%s", buf);
    if (sim_deb && (sim_deb != stdout) && (sim_deb != sim_log))
      fprintf (sim_deb, "%s", buf);

    if (buf != stackbuf)
      free (buf);
  }
#endif
