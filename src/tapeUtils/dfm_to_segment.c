/*
 Copyright 2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// dfm_to_segment dfmFile Dir

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <inttypes.h>

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
        fprintf (stderr, "Usage: dfm_to_segment dfmFile dir\n");
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

    word36 rec_type;
    word36 rec_len;

    for (uint i = 0; i < nwords; /* i ++ */)
      {
        rec_type = big [i];
        rec_len = big [i+1] & 07777777; // iox_$read_key: bin(21)
        if (!rec_len) break;

        i += 2;
        uint leng = rec_len >> 2;

        char segname [32 + 1];
        sprintf(segname,"seg%s.%07"PRIo64,argv[1],i);
        printf ("%s\n", segname);
        
//printf ("%d\n", leng);
printf ("start %d %o leng %d %o rectype %o\n", i, i, leng, leng, rec_type);
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

        word36 blklen = 0;
        uint wordswritten = 0;
        uint8_t bytes [9];
        for (uint w = 0; w < leng; )
          {
            if (!blklen)    {   // skip tape block header
               if (w==leng-1) break;
               blklen = big [i+w] & 0777777;
               printf ("ofs %o bsn %o len %o\n", i+w, big [i+w] >> 18, blklen);
               w ++;
            }
            put36 (big [i + w], bytes, wordswritten &1);
            //printf("%llo %d\n",big [i + w],wordswritten &1);
            w++;
            blklen --;
            if (wordswritten &1)
            {  
               //printf("%llo %llo\n",extr (bytes, 0, 36),extr (bytes, 36, 36));
               write (fdout, bytes, 9);
            }
            wordswritten ++;
          }
        if (wordswritten &1) {
             printf ("write odd word\n");
             put36 (0, bytes, 1);
             write (fdout, bytes, 5);
        }
        fflush(stdout);
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
        sprintf (bcbuf, "bitcnt: %"PRId64"\n", wordswritten*36);
        write (fdout, bcbuf, strlen (bcbuf));
        close (fdout);

        i += leng + (leng&1);

        rec_type = big [i];
        rec_len = big [i+1] & 07777777; // TODO

        while (rec_type != 01500000) {
            // skip library label
            printf("skip ofs %o\n", i);
            fflush(stdout);
            i += (rec_len >> 2) + (rec_len%4?3:2);
            if (big[i]==0) i++;
            rec_type = big [i];
            rec_len = big [i+1] & 07777777;
        }

        //printf("%llo\n",rec_id);
        //printf("%llo\n",rec_len);

        int nch = rec_len;

        int j;

        i += 2;

        word36 *bigp = big + i;

        //printf("%llu %llu %llu\n",big,bigp,i);

        static int byteorder [8] = { 3, 2, 1, 0, 7, 6, 5, 4 };
        for (j = 0; j < nch; j ++)
          {
            uint8_t bytes [9];
            if ((j&7)==0) {
                put36 (*bigp++, bytes, 0);
                put36 (*bigp++, bytes, 1);
            }
            uint64_t c = extr (bytes, byteorder [j&7] * 9, 9);
            bcbuf[j] = c & 0177;
            //printf("%d\n",c & 0177);
          }
        bcbuf[j] = 0;
        printf("%d %s",j,bcbuf);

        i += nch/4 + (nch%4?1:0);
        if (big[i]==0) i++;

      }        
    return 0;
  }
