/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Unpack a tape record to a oct file
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "bit36.h"

// upack origin dat oct
int main (int argc, char * argv [])
  {
    FILE *fopen(const char *path, const char *mode);

    uint origin = strtol (argv [1], NULL, 0);
    int fin = open (argv [2], O_RDONLY);
    if (fin < 0)
      {
        fprintf (stderr, "can't open input file\n");
        exit (1);
      }
    off_t sz = lseek (fin, 0, SEEK_END);
    lseek (fin, 0, SEEK_SET);
    
    // assuming even

    int sz36 = sz * 8 / 36;

    FILE * fout = fopen (argv [3], "w");
    if (fout < 0)
      {
        fprintf (stderr, "can't open output file\n");
        exit (1);
      }

    fprintf (fout, "!SIZE  %06o\n", origin + sz36);

    for (int i = 0; ; i += 2)
      {
        uint8_t Ypair [9];
        ssize_t nread = read (fin, Ypair, sizeof (Ypair));
        if (nread == 0)
          break;
        if (nread != sizeof (Ypair))
          {
            fprintf (stderr, "couldn't read\n");
            exit (1);
          }
        word36 Y;
        Y = extr36 (Ypair, 0);
        fprintf (fout, "%06o xxxx %012lo\toct\t%012lo\n", origin + i, Y, Y);
        Y = extr36 (Ypair, 1);
        fprintf (fout, "%06o xxxx %012lo\toct\t%012lo\n", origin + i + 1, Y, Y);
      }
#if 0
    char buf [256];
    fgets (buf, 256, fin);
    if (strncmp (buf, "!SIZE", 5))
      {
        printf ("can't find !SIZE\n");
        exit (1);
      }
    // !SIZE is right; it is the last loaded + 1; it doesn't take ORG into accout
    long size = strtol (buf + 6, NULL, 8);
    uint8_t * outbuf = malloc ((size * 9 + 1) / 2);

    int wc = 0;
    while (! feof (fin))
      {
        char * rv = fgets (buf, 256, fin);
        if (! rv)
          continue;
        if (buf [0] < '0' || buf [0] > '7')
          continue;
        word36 w36 = strtoll (buf + 12, NULL, 8);
        put36 (w36, outbuf, wc);
        wc ++;
      }
    write (fout, outbuf, (wc * 9 + 1) / 2);
#endif
    close (fin);
    fclose (fout);
    return 0;
  }
