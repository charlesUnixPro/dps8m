/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/* dump the contents of a packed-72 bit file */

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
    int i = 0;
    int fd;
    fd = open (argv [1], O_RDONLY);
    if (fd < 0)
      {
        printf ("can't open file\n");
        exit (1);
      }
    for (;;)
      {
        ssize_t sz;
// 72 bits at a time; 2 dps8m words == 9 bytes
        uint8_t bytes [9];
        memset (bytes, 0, 9);
        sz = read (fd, bytes, 9);
        if (sz == 0)
          break;
        //if (sz != n) short read?
        uint64_t w1 = extr (bytes, 0, 36);
        uint64_t w2 = extr (bytes, 36, 36);
        // int text [8]; // 8 9-bit bytes in 2 words
if (i % 1024 == 0) printf ("sect %d rec %d\n", i / 512, i / 1024);
else if (i % 512 == 0) printf ("sect %d\n", i / 512);
        printf ("%08o   %012"PRIo64"   %012"PRIo64"   \"", i, w1, w2);
        i += 2;
        int j;

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
    return 0;
  }
