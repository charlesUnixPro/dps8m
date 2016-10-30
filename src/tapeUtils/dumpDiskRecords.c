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

// extract bits into a number
#include <stdint.h>

#include "bit36.h"

#define sect_per_cyl 255 
#define sect_per_rec 2
#define number_of_sv 3

static int r2s (int rec, int sv)
  {
    int usable = (sect_per_cyl / sect_per_rec) * sect_per_rec;
    int unusable = sect_per_cyl - usable;
    int sect = rec * sect_per_rec;
    sect = sect + (sect / usable) * unusable; 

    int sect_offset = sect % sect_per_cyl;
    sect = (sect - sect_offset) * number_of_sv + sv * sect_per_cyl +
            sect_offset;
    return sect;
  }

#define SECTOR_SZ_IN_W36 512
#define SECTOR_SZ_IN_BYTES ((36 * SECTOR_SZ_IN_W36) / 8)

typedef uint8_t record [sect_per_rec * SECTOR_SZ_IN_BYTES];

static void readRecord (int fd, int rec, int sv, record * data)
  {
    int sect = r2s (rec, sv);
//fprintf (stderr, "rr rec %d sect %d\n", rec, sect);
    off_t n = lseek (fd, sect * SECTOR_SZ_IN_BYTES, SEEK_SET);
//fprintf (stderr, "rr rec %d sect %d offset %d %o\n", rec, sect, sect * 512, sect * 512);
    if (n == (off_t) -1)
      { fprintf (stderr, "2\n"); exit (1); }
    ssize_t r = read (fd, data, sizeof (record));
    if (r != sizeof (record))
      { fprintf (stderr, "3\n"); exit (1); }
  }


static void printRecord (int fd, int sv, int r)
  {
printf ("sv %c rec %06o %6d.\n", sv + 'a', r, r);
            record data;
            readRecord (fd, r, sv, & data);
            for (uint w = 0; w < 512; w ++)
              {
                uint64_t w1 = extr (& data [w * 9], 0, 36);
                uint64_t w2 = extr (& data [w * 9], 36, 36);
                printf ("%08o   %012lo   %012lo   \"", w * 2, w1, w2);

                static int byteorder [8] = { 3, 2, 1, 0, 7, 6, 5, 4 };
                for (int j = 0; j < 8; j ++)
                  {
                    uint64_t c = extr (data + w * 9, byteorder [j] * 9, 9);
                    if (isprint (c))
                      printf ("%c", (char) c);
                    else
                      printf ("\\%03lo", c);
                  }
                printf ("\n");
              } // w
  }

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
    if (argc == 4)
      {
        char svc = argv [2] [0];
        if (svc != 'a' &&  svc != 'b' && svc != 'c')
          {
            printf ("can't parse sv\n");
            exit (1);
          }
        int sv = svc - 'a';
        int rec = strtol (argv [3], NULL, 0);
        printRecord (fd, sv, rec);
        return 0;
      }
// fs_dev.rec_per_dev 224790.
// rec/sv 74930 
    for (uint sv = 0; sv < number_of_sv; sv ++)
      {
        for (uint r = 0; r < 224790; r ++)
          {
printf ("sv %c rec %06o %6d.\n", sv + 'a', r, r);
            record data;
            readRecord (fd, r, sv, & data);
            for (uint w = 0; w < 512; w += 2)
              {
                uint64_t w1 = extr (& data [w * 9], 0, 36);
                uint64_t w2 = extr (& data [w * 9], 36, 36);
                printf ("%08o   %012lo   %012lo   \"", w, w1, w2);

                static int byteorder [8] = { 3, 2, 1, 0, 7, 6, 5, 4 };
                for (int j = 0; j < 8; j ++)
                  {
                    uint64_t c = extr (data + w * 9, byteorder [j] * 9, 9);
                    if (isprint (c))
                      printf ("%c", (char) c);
                    else
                      printf ("\\%03lo", c);
                  }
                printf ("\n");
              } // w
          } // r
      }  // sv
    return 0;
  }
