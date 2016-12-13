/*
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// p72archive_to_ascii p72File asciiDir

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
#endif   

// extract bits into a number
#include <stdint.h>

#include "bit36.h"

static void trimTrailingSpaces (char * str)
  {
    char * end = str + strlen((char *) str) - 1;
    while (end > str && isspace (* end))
      end --;
    * (end + 1) = 0;
  }

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
        fprintf (stderr, "can't open input file\n");
        exit (1);
      }

    off_t flen = lseek (fdin, 0, SEEK_END);
    lseek (fdin, 0, SEEK_SET);

    ssize_t nwords = (flen * 2) / 9u;
    printf ("nwords %lu\n", nwords);
    ssize_t residue = flen % 9u;
    if (residue && residue != 5)
      fprintf (stderr, "residue %lu\n", residue);
      
    word36 * big = (word36 *) malloc (sizeof (word36) * nwords);
    for (int i = 0; i < nwords; i += 2)
      {
        ssize_t sz;
// 72 bits at a time; 2 dps8m words == 9 bytes
        uint8_t bytes [9];
        memset (bytes, 0, 9);
        sz = read (fdin, bytes, 9);
        if (sz == 0)
          break;
        big [i] = extr (bytes, 0, 36);
        big [i + 1] = extr (bytes, 36, 36);
      }

    char dname [strlen (argv [2]) + 8];
    strcpy (dname, argv [2]);
    strcat (dname, "/./");
    makeDirs (dname);

    for (uint i = 0; i < nwords; /* i ++ */)
      {
        //printf ("top %012lo\n", big [i]);
        if (big [i]     != 0014012012012lu ||
            big [i + 1] != 0017012011011lu)
          {
            fprintf (stderr, "missing hdr\n");
            exit (1);
          }

        char segname [32 + 1];
        //strncpy (segname, (char *) (big + i + 12u), 32);
        for (uint j = 0; j < 32; j ++)
          {
            uint k = j + 12u;
            uint nw = k / 4u;
            uint nc = k % 4u;
            uint shift = (3 - nc) * 9;
            segname [j] = (big [i + nw] >> shift) & 0177;
//printf ("%d %d %d %d %d %012lo %o\n", i, j, k, nw, k % 4, big [i + nw], segname [j]);
          }
        segname [32] = '\0';
        trimTrailingSpaces (segname);
        printf ("%s\n", segname);

//for (int xx = 0; xx < 32; xx ++) printf ("   %012lo\n", big [i + xx]);
printf ("bitcnt %012lo%012lo\n", big [i + 21], big [i + 22]);
        char bc [9];
        bc [0] = (big [i + 21] >> 27) & 0177;
        bc [1] = (big [i + 21] >> 18) & 0177;
        bc [2] = (big [i + 21] >>  9) & 0177;
        bc [3] = (big [i + 21] >>  0) & 0177;
        bc [4] = (big [i + 22] >> 27) & 0177;
        bc [5] = (big [i + 22] >> 18) & 0177;
        bc [6] = (big [i + 22] >>  9) & 0177;
        bc [7] = (big [i + 22] >>  0) & 0177;
        bc [8] = 0;
//printf ("bitcnt %s\n", bc);
        for (i += 3; i < nwords; i ++)
          {
            //printf ("try %d (%u)\n", i, i);
            if (big [i]    == 0017017017017lu &&
               big [i + 1] == 0012012012012lu)
              break;
          }
        if (i >= nwords)
          {
            fprintf (stderr, "missing trlr\n");
            exit (1);
          }
        i += 2u; // skip trailer
        //printf ("hdr len %u (%o)\n", i - hdrStart, i - hdrStart);

        // find end
        uint j;
        for (j = i; j < nwords; j ++)
          {
            if (big [j]     == 0014012012012lu &&
                big [j + 1] == 0017012011011lu)
//printf ("j %u\n", j);
              break;
          }

        // j now points to the next hdr or just past the buffer end;
        //printf ("component %d - %d (%o - %o)\n", i, j - 1, (i / 4), ((j - 1) / 4));
        uint last = j - 1;
        //while (big [last] == 0)
          //last --;
        //printf ("trimmed %u\n", j - last);
        //if (j - last > 7)
          //fprintf (stderr, "trimmed %u\n", j - last);

        uint leng = ((last - i) + 1u);

//printf ("%d\n", leng);
printf ("start %d %o leng %d %o\n", i, i, leng, leng);
        char fname [strlen (dname) + 32 + 3];
        strcpy (fname, dname);
        strcat (fname, segname);

        int fdout;
        fdout = creat (fname, 0664);
        if (fdout < 0)
          {
            fprintf (stderr, "can't open output file <%s>\n", fname);
            exit (1);
          }
        //write (fdout, big + i, (last - i) + 1u);
        for (int w = 0; w < leng; w += 2)
          {
            uint8_t bytes [9];
            put36 (big [i + w], bytes, 0);
            put36 (big [i + w + 1], bytes, 1);
            write (fdout, bytes, 9);
          }
        close (fdout);

        char mdfname [strlen (dname) + 32 + 3 + 8];
        strcpy (mdfname, dname);
        strcat (mdfname, ".");
        strcat (mdfname, segname);
        strcat (mdfname, ".md");
         
        fdout = creat (mdfname, 0664);
        if (fdout < 0)
          {
            fprintf (stderr, "can't open output file <%s>\n", fname);
            exit (1);
          }
        char bcbuf [128];
        sprintf (bcbuf, "bitcnt: %s\n", bc);
        write (fdout, bcbuf, strlen (bcbuf));
        close (fdout);

        i = j;
         printf ("i now %u\n", i);
      }        
    return 0;
  }

