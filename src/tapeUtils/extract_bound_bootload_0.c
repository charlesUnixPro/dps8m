/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// The file 20184.tap.00000002.dat contains a collection of pieces of multics
// the first part of which is loaded by boot_tape_label.
// I have no documentation on the format of the data, but observation of
// the boot loader behavior:
//
//   each tape block:
//       1040 words:
//                8 MST header
//               16 loader header
//             1008 data
//                8 MST trailer
//
// Hmmm.. not quite right... only the first block has the loader header?
//
//   first tape block:
//       1040 words:
//                8 MST header
//               16 loader header
//             1008 data
//                8 MST trailer
//
//   add'l tape blocks:
//       1040 words:
//                8 MST header
//             1024 data
//                8 MST trailer
//
// Nope, still 16 words off at 2048 words in
// Try:
//
//   first tape block:
//       1040 words:
//                8 MST header
//               16 loader header
//             1008 data
//                8 MST trailer
//
//   add'l tape blocks:
//       1040 words:
//             1040 data
//
// Extract the data from each block and concatanate it
// The bootloader loads 10 blocks before jumping into the code;
// assume that is the length of bound_bootload_0
//

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "mst.h"
#include "bit36.h"

static uint8_t buf [mst_blksz_bytes];
static word36 address, tally;
// Careful, the data is loaded to 060001, NOT 060000
           word36 buf_addr = 060001;

static void rd_tape (int fin)
  {

printf ("\n\n");
    ssize_t rc = read (fin, buf, mst_blksz_bytes);
    if (rc != mst_blksz_bytes)
      {
        printf ("can't read block\n");
        exit (1);
      }

// Initialize address and tally
    // " compute prb pointer
    //  060005   ldq     prb.mstrh+mstr_header.data_bits_used
    word36 q = extr36 (buf, 060005 - buf_addr);
printf ("q %06lo\n", q);
    //           div     36,du  " number of words in QL
    q = ((q >> 18) & 0777777) / 36;
    // adq     1,dl            " plus one
// It works when I don't do this; something about tally that I am not grasping
//    q ++;
printf ("q %06lo\n", q);
    //  qls     18-12           " in Q(18:29)
    tally = q;
    //  60011 adq     prb.data,du     " whole pointer
    address = 060011 - buf_addr; // offset into buf
    //  stq     prb.prb_it
    // address and tally now set up
  }

int main (int argc, char * argv [])
  {
    int fin = open (argv [1], O_RDONLY);
    if (fin < 0)
      {
        printf ("can't open input file\n");
        exit (1);
      }

    int fout = open (argv [2], O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (fout < 0)
      {
        printf ("can't open output file\n");
        exit (1);
      }


    rd_tape (fin);

    // 060011 lda     prb.data  " less SLTE size
    word36 prb_data = extr36 (buf, 060011 - buf_addr); 
printf ("prb_data %06lo\n", prb_data);

    // ada 1,dl " and control word
    prb_data += 1;
printf ("and ctrl wd %06lo\n", prb_data);
    // als 18 // shift into address field
    // asa     prb.prb_it      " adjust address                
    address += prb_data;
printf ("address adjust %06lo (%06lo)\n", address, address + buf_addr);

    // arl     12
    // neg     0
    // asa     prb.prb_it " and tally
    tally -= prb_data;
printf ("tally adjust %06lo\n", tally);

    // lxl4    prb.prb_it,id   " bound_bootload_0 control word 
    /* word18 */ uint32_t idx4 = extr36 (buf, address) & 0777777;
    address ++;
    tally --;
printf ("ctl wd %06o addr %06lo tally %06lo\n", idx4, address, tally);

next_record:;

    if (idx4 < tally)
      tally = idx4;

    // assuming address is 72 bit aligned and tally even
    // copy from prb to code segment
printf ("write address %06lo, tally %06lo\n", address, tally);
    write (fout, buf + address * 36 / 8, tally * 36 / 8);

    idx4 -= tally;

printf ("ctl wd %06o addr %06lo tally %06lo\n", idx4, address, tally);
    if (idx4 > 0)
      {
        rd_tape (fin);
        goto next_record;
      }
    close (fin);
    close (fout);
    return 0;
  }
