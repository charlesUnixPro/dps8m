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
          }
        //printf ("\n");
        write (fdout, buf, nch);
      }
    close (fdin);
    close (fdout);
    return 0;
  }

