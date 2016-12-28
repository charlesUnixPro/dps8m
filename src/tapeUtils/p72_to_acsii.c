/*
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// p72_to_ascii p72File asciiFile

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>

#ifdef __MINGW64__
#define mkdir(x,mode) mkdir(x)
#define open(x,y,args...) open(x, y|O_BINARY,##args)
#define creat(x,y) open(x, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, y)
#undef isprint
#define isprint(c) (c>=0x20 && c<=0x7f)
#endif


// extract bits into a number
#include <stdint.h>

#include "bit36.h"

static void makeDirs (char * name)
  {
    //printf (">%s\n", name);
    char * outname = strdup (name);
    char * outdir = dirname (outname);
    if (strlen (outdir) && strcmp (outdir, "."))
      {
        makeDirs (outdir);
        mkdir (outdir, 0777);
      }
    free (outname);
  }

int main (int argc, char * argv [])
  {
    if (argc != 3)
      {
        fprintf (stderr, "Usage: p72_to_ascii p72File asciiFile\n");
        exit (1);
      }
    int fdin;
    fdin = open (argv [1], O_RDONLY);
    if (fdin < 0)
      {
        fprintf (stderr, "can't open input file <%s>\n", argv [1]);
        exit (1);
      }
    makeDirs (argv [2]);
    int fdout;
    fdout = creat (argv [2], 0777);
    if (fdout < 0)
      {
        fprintf (stderr, "can't open output file <%s>\n", argv [2]);
        exit (1);
      }
    
    for (;;)
      {
        ssize_t sz;
// 72 bits at a time; 2 dps8m words == 9 bytes
        uint8_t bytes [9];
        memset (bytes, 0, 9);
        sz = read (fdin, bytes, 9);
        if (sz == 0)
          break;
// bytes bits  bits/ 9
//   1     8     0
//   2    16     1
//   3    24     2
//   4    32     3
//   5    40     4
//   6    48     5
//   7    56     6
//   8    64     7
//   9    72     8

        int nch = sz - 1;

        int j;

        static int byteorder [8] = { 3, 2, 1, 0, 7, 6, 5, 4 };
        char buf [8];
        for (j = 0; j < nch; j ++)
          {
            uint64_t c = extr (bytes, byteorder [j] * 9, 9);
            //if (isprint (c))
              //printf ("%c", (char) c);
            //else
              //printf ("\\%03lo", c);
            buf [j] = c & 0177;
            if (buf [j])
              write (fdout, & buf [j], 1);
          }
        //printf ("\n");
        //write (fdout, buf, nch);
      }
    close (fdin);
    close (fdout);
    return 0;
  }

