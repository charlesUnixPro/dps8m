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


// extract bits into a number
#include <stdint.h>

#include "bit36.h"

static void trimTrailingSpaces (char * str)
  {
    char * end = str + strlen(str) - 1;
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
    //printf ("nwords %lu\n", nwords);
    ssize_t residue = flen % 9u;
    if (residue && residue != 5)
      fprintf (stderr, "residue %lu\n", residue);
      
    ssize_t nchars = nwords * 4u;
    uint8_t * big = malloc (nchars);
    uint8_t * bigp = big;

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
        for (j = 0; j < nch; j ++)
          {
            uint64_t c = extr (bytes, byteorder [j] * 9, 9);
            * (bigp ++) = c & 0177;
          }
      }

    char dname [strlen (argv [2]) + 8];
    strcpy (dname, argv [2]);
    strcat (dname, "/./");
    makeDirs (dname);

    for (uint i = 0; i < nchars; /* i ++ */)
      {
        // printf ("top %u %u\n", i, i %4u);
        if (i % 4u)
          {
            fprintf (stderr, "phase error\n");
            exit (1);
          }
        static uint8_t hdr [8] = "\014\012\012\012\017\012\011\011";
        if (strncmp ((char *) big + i, (char *) hdr, 8))
          {
            fprintf (stderr, "missing hdr\n");
            exit (1);
          }
        //uint hdrStart = i;

        char segname [32 + 1];
        strncpy (segname, (char *) (big + i + 12u), 32);
        segname [32] = '\0';
        trimTrailingSpaces (segname);
        //printf ("%s\n", segname);
        for (i += 014u; i < nchars; i += 4u)
          {
            //printf ("try %d (%u)\n", i, i);
            static uint8_t trlr [8] = "\017\017\017\017\012\012\012\012";
            if (strncmp ((char *) big + i, (char *) trlr, 8) == 0)
              break;
          }
        if (i >= nchars)
          {
            fprintf (stderr, "missing trlr\n");
            exit (1);
          }
        i += 8u; // skip trailer
        //printf ("hdr len %u (%o)\n", i - hdrStart, i - hdrStart);

        // find end
        uint j;
        for (j = i; j < nchars; j += 4)
          {
//printf ("j %u\n", j);
            if (strncmp ((char *) big + j, (char *) hdr, 8) == 0)
              break;
          }
        // j now points to the next hdr or just past the buffer end;
        //printf ("component %d - %d (%o - %o)\n", i, j - 1, (i / 4), ((j - 1) / 4));
        uint last = j - 1;
        while (big [last] == 0)
          last --;
        //printf ("trimmed %u\n", j - last);
        if (j - last > 7)
          fprintf (stderr, "trimmed %u\n", j - last);

        char fname [strlen (dname) + 32 + 3];
        strcpy (fname, dname);
        strcat (fname, segname);

        int fdout;
        fdout = creat (fname, 0777);
        if (fdout < 0)
          {
            fprintf (stderr, "can't open output file <%s>\n", fname);
            exit (1);
          }
        write (fdout, big + i, (last - i) + 1u);
        close (fdout);

        i = j;
        // printf ("i now %u %u\n", i, i % 4u);
      }        
    return 0;
  }

