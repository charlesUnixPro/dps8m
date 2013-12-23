// simple macro preprocessor for as8

//     macro   name
//       #1 #2
//     endm
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>

char *strndup(const char *s, size_t n);

#define bufsize 1025
static char buf [bufsize];

#define S_MACRO "macro"
#define L_MACRO (sizeof (S_MACRO) - 1)

#define S_ENDM "endm"
#define L_ENDM (sizeof (S_ENDM) - 1)

#define CS_NAME "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_"

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

#if 0
static void dumpstr (char * tag, char * str, size_t len)
  {
    fprintf (stderr, "%s <", tag);
    for (int i = 0; i < len; i ++)
      if (isprint (str [i]) || str [i] == '\n')
        fprintf (stderr, "%c", str [i]);
      else
        if (str [i] == '\t')
          fprintf (stderr, "\t");
        else
          fprintf (stderr, "\\%o", str [i]);
    fprintf (stderr, ">\n");
  }
#endif

int main (int argc, char * argv [])
  {

// Loop over lines, either 'defining' a macro (adding lines to the body)
// or 'scanning', looking for macro definitions or references

    bool defining = false;

    for (;;)
      {
        if (! fgets (buf, sizeof (buf), stdin))
          break;

//
// buf: line of text, with potential nl, zero terminated
//

        bool make_comment = false;
        size_t buf_len = strlen (buf);

// scan_len is the number of characters in buf that are not comment or nl

        size_t scan_len;

        // Find the comment start if any
        char * comment_ptr = index (buf, '"');
        if (comment_ptr)
          scan_len = comment_ptr - buf;
        else
          {
            scan_len = buf_len;
            if (buf [buf_len - 1] == '\n')
              scan_len --; // less the newline
          }

// expbuf contains a copy of the text to be preprocessed, no NL, zero-terminated

        char * expbuf = strndup (buf, scan_len);

// bufidx is an index into expbuf; walk it througn expbuf looking for macro 
// references
 
// DO NOT keep pointers into expbuf; expand() will realloc() it.

        int bufidx = 0;

// blank, tab, nl
#define SKIPWS while (expbuf [bufidx] == ' ' || expbuf [bufidx] == '\t' || expbuf [bufidx] == '\n') bufidx ++;
// blank, tab
#define SKIPSP while (expbuf [bufidx] == ' ' || expbuf [bufidx] == '\t') bufidx ++;

        // Skip over leading whitespace and NL (NL's may become pasted in)

        SKIPWS;

        if (! defining)
          {

            // macro definition?

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
                for (int i = 0; i < n_macros; i ++)
                  if (strcmp (macros [i] . name, macros [n_macros] . name) == 0)
                    die ("macro redefinition");
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
                // scan for macro calls and expand them
rescan:
                for (int m = 0; m < n_macros; m ++)
                  {
                    struct macro * mp = macros + m;
                    char * where = strstr (expbuf + bufidx, mp -> name);
                    if (where)
                      {
                        char * end = where + strlen (mp -> name);
                        size_t extra = strspn (end, CS_NAME);
                        if (extra)
                          where = NULL;
                      }

                    // if where is NULL, there is not a macro call at this
                    // location
                    // if where is NON-NULL it points to the character in
                    // expbuf immediately after macro call name

                    if (where)
                      {
                        // Convert pointer to index
                        int where_idx = where - expbuf;

                        // Save the start location for rescan
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
                            SKIPSP; // don't skip over nl when expanding nested macros
                            if (expbuf [bufidx] && expbuf [bufidx] != '\n')
                              {
                                if (n_args == MAX_ARGS)
                                  die ("too many args");
// When expanding an unnest macro call, the end of the argument list
// is the end of the buffer; when nested, it is the end of the line.
// comma_ptr needs to be smarter
                                //char * comma_ptr = index (expbuf + bufidx, ',');
                                //int arg_len;
                                //if (comma_ptr)
                                  //arg_len = comma_ptr - (expbuf + bufidx);
                                //else
                                  //arg_len = strlen (expbuf + bufidx);
                                int arg_len = strcspn (expbuf + bufidx, ",\n");
                                // copy the argument value
                                mp -> args [n_args] = strndup (expbuf + bufidx, arg_len);
                                n_args ++;
                                // erase the argument
                                //if (comma_ptr)
                                if (expbuf [bufidx + arg_len] == ',') 
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
                            //char * arg_ref = index (body, '#');
                            size_t arg_idx = strcspn (body, "#?");
                            //if (arg_ref)
                            if (body [arg_idx] == '#')
                              {
                                //char arg_digit = * (arg_ref + 1);
                                char arg_digit = body [arg_idx + 1];
                                if (arg_digit < '1' || arg_digit > '9')
                                  die ("invalid argument number");
                                int arg_num = arg_digit - '1';
                                if (arg_num >= mp -> n_args)
                                  die ("argument number too big");
                                //int arg_idx = arg_ref - body;
                                // erase the reference
                                erase (body + arg_idx, 2);

                                // make room for the arg value
                                expand (& body, arg_idx, mp -> args [arg_num]);
                        
                                // paste the arg value
                                memmove (body + arg_idx, mp -> args [arg_num], strlen (mp -> args [arg_num]));
                                //continue;
                              }
                            //arg_ref = index (body, '?');
                            //if (arg_ref)
                            else if (body [arg_idx] == '?')
                              {
                                //int arg_idx = arg_ref - body;
                                //char arg_digit = * (arg_ref + 1);
                                char arg_digit = body [arg_idx + 1];
                                if (arg_digit < '1' || arg_digit > '9')
                                  die ("invalid argument number");
                                int arg_num = arg_digit - '1';
                                char * close = index (body + arg_idx + 1, '?');
                                if (! close)
                                  die ("unclosed default");
                                int arg_len = close - (body + arg_idx) + 1;
                                //int arg_len;
                                if (arg_num >= mp -> n_args ||
                                    strlen (mp -> args [arg_num]) == 0)
                                  { // use default
                                    // find the closing '?'
                                    // get the default value
                                    char * def = strndup (body + arg_idx + 2,
                                                          arg_len - 3);
                                    erase (body + arg_idx, arg_len);
                                    // make room for the arg value
                                    expand (& body, arg_idx, def);

                                    // paste the arg value
                                    memmove (body + arg_idx, def, arg_len - 3);
                                    free (def);
                                  }
                                else if (arg_num < mp -> n_args)
                                  { // use definition
                                    //arg_len = 2;
                                    erase (body + arg_idx, arg_len);
                                    // make room for the arg value
                                    expand (& body, arg_idx, mp -> args [arg_num]);

                                    // paste the arg value
                                    memmove (body + arg_idx, mp -> args [arg_num], strlen (mp -> args [arg_num]));
                                  }
                                else
                                  die ("argument number too big");
                                //continue;
                              }
                            else
                              break;
                          }

                        // make room for the body
                        expand (& expbuf, where_idx, body);
                        // printf ("expand <%s>\n", expbuf);
                        
                        // paste the body
                        memmove (expbuf + where_idx, body, strlen (body));
                        // printf ("paste <%s>\n", expbuf);

                        free (body);

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
                    // Grow the body
                    macros [n_macros] . body =
                      realloc (macros [n_macros] . body,
                               strlen (macros [n_macros] . body) + buf_len + 1);
                    strcat (macros [n_macros] . body, buf);
                  }
                else
                  {
                    // Allocate the body
                    macros [n_macros] . body = malloc (buf_len + 1);
                    strcpy (macros [n_macros] . body, buf);
                  }
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
    if (defining)
      die ("no endm");
    return 0;
  }
