// simple macro preprocessor for as8

//     macro   name
//       #1 #2
//     endm
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
char *strndup(const char *s, size_t n);

#define bufsize 1025
static char buf [bufsize];

#define S_MACRO "macro"
#define L_MACRO (sizeof (S_MACRO) - 1)

#define S_ENDM "endm"
#define L_ENDM (sizeof (S_ENDM) - 1)

#define CS_NAME "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxy0123456789_"

#define MAX_ARGS 9
#define MAX_MACROS 1024

static struct macro
  {
    char * name;
    char * args [MAX_ARGS];
    int n_args;
    char * body;
  } macros [MAX_MACROS];
static int n_macros = 0;

static void die (char * msg)
  {
    fprintf (stderr, "die: %s\n", msg);
    exit (1);
  }

static void erase (char * buf, size_t howmuch)
  { 
    memmove (buf, buf + howmuch, strlen (buf + howmuch) + 1); // include the NUL
  }

static void expand (char ** buf, size_t where, char * what)
  {
    size_t what_len = strlen (what);
    size_t tail_len = strlen ((* buf) + where) + 1;
    // make room
    * buf = realloc (* buf, strlen (* buf) + what_len + 1);
    // move the end to the end
    memmove ((* buf) + where + what_len, (* buf) + where, tail_len);
  }

int main (int argc, char * argv [])
  {
    bool defining = false;

    for (;;)
      {
        if (! fgets (buf, sizeof (buf), stdin))
          break;

        bool make_comment = false;
        size_t buf_len = strlen (buf);

        // Find the comment start if any
        char * comment_ptr = index (buf, '"');
       
        size_t scan_len;
        if (comment_ptr)
          scan_len = comment_ptr - buf;
        else
          scan_len = buf_len - 1; // less the newline
        char * expbuf = strndup (buf, scan_len);

        //char * p = expbuf;
        //char * end_p = buf + scan_len;
        int bufidx = 0;

#define SKIPWS while (expbuf [bufidx] == ' ' || expbuf [bufidx] == '\t' || expbuf [bufidx] == '\n') bufidx ++;

        // Skip over leading whitespace
        SKIPWS;

        if (! defining)
          {

            // macro definition

            if (strncasecmp (expbuf + bufidx, S_MACRO, L_MACRO) == 0)
              {
                bufidx += L_MACRO;

                // Defining a macro
                if (n_macros == MAX_MACROS)
                  die ("too many macros");

                // Get the name
                SKIPWS;
                size_t name_len = strspn (expbuf + bufidx, CS_NAME);
                if (! name_len)
                  die ("empty macro name");

                macros [n_macros] . name = strndup (expbuf + bufidx, name_len);
                bufidx += name_len;
                SKIPWS;
                if (expbuf [bufidx])
                  die ("junk at end of statement");

                macros [n_macros] . body = NULL;

                defining = true;
                make_comment = true;
              }

            else
              {
                // scan for macro call and expand them
rescan:
                for (int m = 0; m < n_macros; m ++)
                  {
                    struct macro * mp = macros + m;
                    char * where = strstr (expbuf + bufidx, mp -> name);
                    if (where)
                      {
                        // Save the start location for rescan
                        int where_idx = where - expbuf;
                        bufidx = where_idx;
                        // Expand the macro

                        // printf ("first <%s>\n", expbuf);
                        // erase the call
                        size_t call_len = strlen (mp -> name);
                        erase (expbuf + bufidx, call_len);
                        // printf ("erase <%s>\n", expbuf);

                        // process args
                        int n_args = 0;
                        for (;;)
                          {
                            SKIPWS;
                            if (expbuf [bufidx])
                              {
                                if (n_args == MAX_ARGS)
                                  die ("too many args");
                                char * comma_ptr = index (expbuf + bufidx, ',');
                                int arg_len;
                                if (comma_ptr)
                                  arg_len = comma_ptr - (expbuf + bufidx);
                                else
                                  arg_len = strlen (expbuf + bufidx);
                                // copy the argument value
                                mp -> args [n_args] = strndup (expbuf + bufidx, arg_len);
                                n_args ++;
                                // erase the argument
                                if (comma_ptr)
                                  erase (expbuf + bufidx, arg_len + 1);
                                else
                                  erase (expbuf + bufidx, arg_len);
                              }
                            else
                              break;
                          }
                        mp -> n_args = n_args;

                        // Copy the body
                        char * body = strdup (mp -> body);

                        // Replace argument references
                        for (;;)
                          {
                            char * arg_ref = index (body, '#');
                            if (! arg_ref)
                              break;
                            char arg_digit = * (arg_ref + 1);
                            if (arg_digit < '1' || arg_digit > '9')
                              die ("invalid argument number");
                            int arg_num = arg_digit - '1';
                            if (arg_num > mp -> n_args)
                              die ("argument number too big");
                            int arg_idx = arg_ref - body;
                            // erase the reference
                            erase (body + arg_idx, 2);

                            // make room for the arg value
                            expand (& body, arg_idx, mp -> args [arg_num]);
                        
                            // paste the arg value
                            memmove (body + arg_idx, mp -> args [arg_num], strlen (mp -> args [arg_num]));
                          }

                        // make room for the body
                        expand (& expbuf, where_idx, body);
                        // printf ("expand <%s>\n", expbuf);
                        
                        // paste the body
                        memmove (expbuf + where_idx, body, strlen (body));
                        // printf ("paste <%s>\n", expbuf);

                        // rescan
                        bufidx = where_idx;
                        goto rescan;
                      } // if where
                  } // for m
              } // scan
          } // if !defining
        else // defining
          {
            if (strncasecmp (expbuf + bufidx, S_ENDM, L_ENDM) == 0)
              {
                bufidx += L_ENDM;

                SKIPWS;
                if (expbuf [bufidx])
                  die ("junk at end of statement");

                n_macros ++;
                defining = false;
              }
            else
              {
                if (macros [n_macros] . body)
                  {
                    macros [n_macros] . body =
                      realloc (macros [n_macros] . body,
                               strlen (macros [n_macros] . body) + buf_len - 1);
                    strcat (macros [n_macros] . body, expbuf);
                  }
                else
                  {
                    macros [n_macros] . body = strdup (expbuf);
                  }
                strcat (macros [n_macros] . body, "\n");
                //printf ("body <%s>\n", macros [n_macros] . body);
              }
            make_comment = true;
          }       
        if (make_comment)
          printf ("\"%s%s", expbuf, comment_ptr ? comment_ptr : "\n");
        else
          printf ("%s%s", expbuf, comment_ptr ? comment_ptr : "\n");
        free (expbuf);
      }
    return 0;
  }
