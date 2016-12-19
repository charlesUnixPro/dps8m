/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

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

#ifdef __MINGW64__
#define open(x,y,args...) open(x, y|O_BINARY,##args)
#define creat(x,y) open(x, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, y)
#endif


#define SECTOR_SZ_IN_W36 512
#define SECTOR_SZ_IN_BYTES ((36 * SECTOR_SZ_IN_W36) / 8)

static int diskFD;
static uint8_t dbuf [SECTOR_SZ_IN_BYTES];

static void mseek (uint secno)
  {
    lseek (diskFD, secno * SECTOR_SZ_IN_BYTES, SEEK_SET);
  }

static void mread (uint secno, uint8_t * buf)
  {
    mseek (secno);
    read (diskFD, buf, SECTOR_SZ_IN_BYTES);
  }

static void mread36 (uint secno, word36 * buf36)
  {
    mread (secno, dbuf);
    for (int i = 0; i < SECTOR_SZ_IN_W36; i ++)
      buf36 [i] = extr36 (dbuf, i);
  }

static void dump (word36 * sectorBuffer)
  {
    for (int i = 0; i < SECTOR_SZ_IN_W36; i += 2)
      {
        printf ("%4d %012lo %012lo \"", i, sectorBuffer [i], sectorBuffer [i + 1]);
        for (int j = 0; j < 8; j ++)
          {
            word9 c;
            if (j < 4)
              c = sectorBuffer [i] >> ((3 - j) * 9);
            else
              c = sectorBuffer [i + 1] >> ((3 - (j - 4)) * 9);
            c &= 0177;
            if (isprint (c))
              printf ("%c", (char) c);
            else
              printf ("\\%03o", c);
          }
        printf ("\n");
      }
  }

static void prints (word36 * sectorBuffer, int cnt)
  {
    for (int i = 0; i < cnt; i ++)
      {
        int woff = i / 4;
        int coff = i % 4;
        word9 c = sectorBuffer [woff] >> ((3 - coff) * 9);
        c &= 0177;
        if (isprint (c))
          printf ("%c", (char) c);
        else
          printf ("\\%03o", c);
      }
  }

int main (int argc, char * argv [])
  {
    diskFD = open ("20184.dsk", O_RDONLY);
    word36 sectorBuffer [SECTOR_SZ_IN_W36];
    mread36 (0, sectorBuffer);

    //dump (sectorBuffer);
#define mlabelOffset (5 * 64)
    int os = mlabelOffset;
    printf ("Label: ");
    prints (& sectorBuffer [os], 32);
    printf ("\n");
    os += 32 / 4;

    printf ("version: %ld\n", sectorBuffer [os ++]);

    printf ("mfg serial: ");
    prints (& sectorBuffer [os], 32);
    printf ("\n");
    os += 32 / 4;

    printf ("pv name: ");
    prints (& sectorBuffer [os], 32);
    printf ("\n");
    os += 32 / 4;

    printf ("lv name: ");
    prints (& sectorBuffer [os], 32);
    printf ("\n");
    os += 32 / 4;

    printf ("pvid: %12lo\n", sectorBuffer [os ++]);

    printf ("lvid: %12lo\n", sectorBuffer [os ++]);

    printf ("root pvid: %12lo\n", sectorBuffer [os ++]);

    printf ("time_registered: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("n_pv_in_lv: %12ld\n", sectorBuffer [os ++]);

    printf ("vol_size: %12ld\n", sectorBuffer [os ++]);

    printf ("votc_size: %12ld\n", sectorBuffer [os ++]);

    printf ("bits: %12lo\n", sectorBuffer [os ++]);

    printf ("max_access_class: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("min_access_class: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("password: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("number_of_sv: %12ld\n", sectorBuffer [os ++]);

    printf ("this_sv: %12ld\n", sectorBuffer [os ++]);

    printf ("sub_vol_name: ");
    prints (& sectorBuffer [os], 1);
    printf ("\n");
    os += 1;

    os += 13; // pad1

    printf ("time_mounted: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("time_map_updated: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("volmap_version: %12ld\n", sectorBuffer [os ++]);

    os += 1; // pad6

    printf ("time_salvaged: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("time_unmounted: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("last_pvtx: %12ld\n", sectorBuffer [os ++]);

    os += 2; // pad1a

    printf ("err_hist_size: %12ld\n", sectorBuffer [os ++]);

    printf ("time_last_dump 1: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("time_last_dump 2: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("time_last_dump 3: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    printf ("time_last_reloaded: %12lo %12lo\n", sectorBuffer [os], sectorBuffer [os + 1]);
    os += 2;

    os += 40; // pad2

    printf ("root_here %d\n", (sectorBuffer [os] & 0400000000000) ? 1 : 0);
    printf ("root_vtocx %ld\n", (sectorBuffer [os] & 0377777777777));
    os += 1;

    printf ("shutdown_state: %12ld\n", sectorBuffer [os ++]);


  }

