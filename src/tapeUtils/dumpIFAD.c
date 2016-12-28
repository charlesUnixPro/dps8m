/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/*
tape image format:

<32 bit little-endian blksiz> <data> <32bit little-endian blksiz>

a single 32 bit word of zero represents a file mark
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>

// extract bits into a number
#include <stdint.h>

#include "bit36.h"

#ifdef __MINGW64__
#define open(x,y,args...) open(x, y|O_BINARY,##args)
#define creat(x,y) open(x, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, y)
#endif

#if 0
static char * bcd =
  "01234567"
  "89[#@;>?"
  " ABCDEFG"
  "HI&.](<\\"
  "^JKLMNOP"
  "QR-$*);'"
  "+/STUVWX"
  "YZ_,%=\"!";
#else
static char * bcd =
  "01234567"
  "89[#@;>?"
  " abcdefg"
  "hi&.](<\\"
  "^jklmnop"
  "qr-$*);'"
  "+/stuvwx"
  "yz_,%=\"!";
#endif

/* load_ident - int proc to load last 2 binary card images of hmpcj1 deck and extract the ident block */

static struct
  {
    int cptr;
  } dfm_data;

static void load_ident (word36 * mwbuf, int * glrp)
  {
    uint64_t media_code = (mwbuf [* glrp] >> 6) & 017;
    while (media_code == 1)
      {
        dfm_data.cptr = *glrp + 1;
        int psz = 4;
        int m = 1
  }

static void read_deck (word36 * mwbuf)
  {
    bool obj_card_found = false;
    bool id_ld = false;
    int cvp = 0;
    int cvp1 = 0;

    uint64_t blk_size = mwbuf [0] & 0777777;

    //printf ("blk_size %lu\n", blk_size);

    // get pointer to first logical record

    // 2 gc_phy_rec_data (0 refer (gc_phy_rec.bcw.blk_size)) bit (36);
    int glrp = 1;
    //char ocard [rsize * 6 + 1];
    char ocard [4096 * 6 + 1];
    memset (ocard, 0, sizeof (ocard));

    //for (int nwds = 0; nwrds < blk_size; )
      //{
    while (glrp < blk_size)
      {
// dcl 1 gc_log_rec based (lrptr) aligned,                     /* GCOS ssf logical record format */
//     2 rcw unaligned,                                        /* record control word */
//      (3 rsize fixed bin (18),                               /* size of rcd (-rcw) */
//       3 nchar_used fixed bin (2),                           /* characters used in last word */
//       3 file_mark fixed bin (4),                            /* file mark if rsize = 0 */
//       3 mbz1 fixed bin (2),
//       3 media_code fixed bin (4),                           /* file media code */
//       3 report_code fixed bin (6)) unsigned,                /* report code */
//     2 gc_log_rec_data (0 refer (gc_log_rec.rcw.rsize)) bit (36); /* logical record data */

        int glrb = glrp + 1;
        uint64_t rsize = (mwbuf [glrp] >> 18) & 0777777;
        //printf ("glrp %d rsize %ld\n", glrp, rsize);
        // bcd_obj bit (78) int static options (constant) init
      //("53202020202020462241252363"b3);                     /* "$      object" in bcd */
        uint64_t media_code = (mwbuf [glrp] >> 6) & 017;
        //printf ("media_code 0%"PRIo64" %ld.\n", media_code, media_code);

        if (media_code == 2) // bcd card image
          {
#if 0
            char card [13 + 1];
            for (i = 0; i < 13; i ++)
              {
                int wos = i / 6;
                int nos = i % 6;
                uint64_t bcdn = (mwbuf [glrb + wos] >> ((5 - nos) * 6)) & 077;
                //printf ("%c", bcd [bcdn]);
                card [i] = bcd [bcdn];
              }
            card [13] = 0;
#endif
            // $      object" in bcd
            if (mwbuf [glrb + 0] == 0532020202020 &&
                mwbuf [glrb + 1] == 0204622412523 &&
                ((mwbuf [glrb + 2] >> 30) & 077) == 063)
              {
                // object card

// dcl  1 o_card based (ocardp) aligned,                       /* template for an object card */
//        (2 pad1 char (15),
//        2 library char (6),                                  /* col 16 - either "hmpcj1" or "htnd  " */
//        2 ld_type char (1),                                  /* col 22, module type */
//        2 ss_type char (1),                                  /* col 23, subsystem type */
//        2 pad2 char (3),
//        2 m_applic char (1),                                 /* Multics applicability, non blank means not applicable */
//        2 pad3 char (15),
//        2 model char (6),                                    /* for hmpcj1 decks, controller model # */
//        2 version char (6),                                  /* for hmpcj1 decks, model version # */
//        2 pad4 char (5),
//        2 assem char (1),                                    /* "m" for mpc assembler, "g" for gmap */
//        2 call_name char (6),                                /* module call name, or gecall name */
//        2 ttl_date char (6),                                 /* date module assembled */
//        2 edit_name char (4),                                /* module edit name */
//        2 pad5 char (4)) unaligned;

                //printf ("card %s rsize %ld\n", card, rsize);
                for (int i = 0; i < rsize * 6; i ++)
                  {
                    int wos = i / 6;
                    int nos = i % 6;
                    uint64_t bcdn = (mwbuf [glrb + wos] >> ((5 - nos) * 6)) & 077;
                    //printf ("%c", bcd [bcdn]);
                    ocard [i] = bcd [bcdn];
                  }
                ocard [rsize*6] = 0;
                //printf ("ocard %s\n", ocard);
                obj_card_found = true;
              }
            else
              {
                //printf ("looking for hmpcj1\n");
                char library [7];
                strncpy (library, ocard + 15, 6);
                library [6] = 0;
                printf ("<%s>\n", library);
                if (strcmp (library, "hmpcj1") == 0 && ! id_ld)
                  {
                    id_ld = true;
                    if (cvp1 == 0)
                      cvp1 = cvp;
                    load_ident (mwbuf, & glrp);
                  }
              }
#if 0

        else if (media_code == 1) // binary card image
          {
          }
        else
          {
            printf ("invalid media_code 0%"PRIo64"\n", media_code);
            exit (2);
          }
#endif
          }
        glrp += rsize + 1;
      }
    if (glrp != blk_size + 1)
      printf ("glrp %d blk_size %ld\n", glrp, blk_size);
  }

int main (int argc, char * argv [])
  {
    int fd;
    int32_t blksiz;
    int32_t blksiz2;
    //fd = open ("20184.tap", O_RDONLY);
    fd = open (argv [1], O_RDONLY);
    if (fd < 0)
      {
        printf ("can't open tape\n");
        exit (1);
      }
    int blkno = 0;
    int blksn = 1;
    for (;;)
      {
        ssize_t sz;
        sz = read (fd, & blksiz, sizeof (blksiz));
        if (sz == 0)
          break;
        if (sz != sizeof (blksiz))
          {
            printf ("can't read blksiz\n");
            exit (1);
          }

        if (! blksiz)
          {
            //printf ("tapemark\n");
            blksn = 1;
          }
        else
          {

            printf ("tape block %d\n", blkno + 1);
// 72 bits at a time; 2 dps8m words == 9 bytes
            int mwsz = (blksiz + 8) / 9 * 2;
            uint64_t mwbuf [mwsz];

            int i;
            uint8_t bytes [9];
            for (i = 0; i < blksiz; i += 9)
              {
                int n = 9;
                if (i + 9 > blksiz)
                  n = blksiz - i;
                memset (bytes, 0, 9);
                sz = read (fd, bytes, n);
                if (sz == 0)
                  {
                    printf ("eof whilst skipping byte\n");
                    exit (1);
                  }
                if (sz != n)
                  {
                    printf ("can't skip bytes\n");
                    exit (1);
                  }
                uint64_t w1 = extr (bytes, 0, 36);
                uint64_t w2 = extr (bytes, 36, 36);
                mwbuf [(i / 9) * 2 + 0] = w1;
                mwbuf [(i / 9) * 2 + 1] = w2;

              }
            uint64_t bsn = (mwbuf [0] >> 18) & 0777777;
            if (blksn != bsn)
              printf ("blkno: %06o blksn: %06o bsn: %06"PRIo64"\n", blkno, blksn, bsn);
            printf ("bsn: %ld\n", bsn);

            read_deck (mwbuf);

            sz = read (fd, & blksiz2, sizeof (blksiz2));
            if (sz != sizeof (blksiz2))
              {
                printf ("can't read blksiz2\n");
                exit (1);
              }
            if (blksiz != blksiz2)
              {
                printf ("blksiz != blksiz2\n");
                exit (1);
              }
            blkno ++;
            blksn ++;
          }
      }
    return 0;
  }
