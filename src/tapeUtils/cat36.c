
/* dump the contents of a packed-72 bit file */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>

// extract bits into a number
#include <stdint.h>

#include "bit36.h"


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
        //uint64_t w1 = extr (bytes, 0, 36);
        //uint64_t w2 = extr (bytes, 36, 36);
        // int text [8]; // 8 9-bit bytes in 2 words
        //printf ("%08o   %012lo   %012lo   \"", i, w1, w2);
        i += 2;
        int j;

        static int byteorder [8] = { 3, 2, 1, 0, 7, 6, 5, 4 };
        for (j = 0; j < 8; j ++)
          {
            uint64_t c = extr (bytes, byteorder [j] * 9, 9);
            if (isprint (c) || c == '\n')
              printf ("%c", (char) c);
            else
              printf ("\\%03lo", c);
          }
      }
    printf ("\n");
    return 0;
  }
