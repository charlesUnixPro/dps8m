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
#undef isprint
#define isprint(c) (c>=0x20 && c<=0x7f)
#endif


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
        printf ("blksiz %d\n", blksiz);

        if (! blksiz)
          {
            printf ("tapemark\n");
          }
        else
          {
// 72 bits at a time; 2 dps8m words == 9 bytes
            int i;
            uint8_t bytes [9];
            for (i = 0; i < blksiz; i += 9)
              {
                int n = 9;
                if (i + 9 > blksiz)
                  n = blksiz - i;
                memset (bytes, 0, 9);
                sz = read (fd, bytes, n);
//{ int jj; for (jj = 0; jj < 72; jj ++) { uint64_t k = extr (bytes, jj, 1); printf ("%ld", k); } printf ("\n"); }
//printf ("%02X %02X %02X %02X %02X\n", bytes [0], bytes [1], bytes [2], bytes [3], bytes [4]);
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
//printf ("%012lo\n", w1);
                uint64_t w2 = extr (bytes, 36, 36);
                // int text [8]; // 8 9-bit bytes in 2 words
                printf ("%04d:%04o   %012"PRIo64"   %012"PRIo64"   \"", blkno, i * 2 / 9, w1, w2);
                int j;
//{printf ("\n<<%012lo>>\n", (* (uint64_t *) bytes) & 0777777777777); }

                static int byteorder [8] = { 3, 2, 1, 0, 7, 6, 5, 4 };
                for (j = 0; j < 8; j ++)
                  {
                    uint64_t c = extr (bytes, byteorder [j] * 9, 9);
                    if (isprint (c))
                      printf ("%c", (char) c);
                    else
                      printf ("\\%03o", c);
                  }
                printf ("\n");
              }
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
          }
      }
    return 0;
  }
