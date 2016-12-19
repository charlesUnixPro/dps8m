/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Take an file from extract_tape_files that mfile as identified as a Multics
// backup file, and list the contents

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifdef __MINGW64__
#define open(x,y,args...) open(x, y|O_BINARY,##args)
#define creat(x,y) open(x, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, y)
#endif

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


#include "mst.h"
#include "bit36.h"

typedef uint16_t word9;
typedef uint64_t word36;
typedef unsigned int uint;

static uint8_t blk [mst_blksz_bytes];
static uint8_t blk_ascii [mst_blksz_word9];
static word9 blk_word9 [mst_blksz_word9];
static word36 blk_word36 [mst_blksz_word36];


static int get_mst_record (int fd)
  {
    ssize_t sz = read (fd, blk, mst_blksz_bytes);
    if (sz == 0)
      return -1;
    if (sz != mst_blksz_bytes)
      {
        printf ("can't read blk\n");
        exit (1);
      }

    word36 * p36 = blk_word36;
    for (uint i = 0; i < mst_blksz_word36; i ++)
      {
        * p36 ++ = extr36 (blk, i);
      }
    uint8_t * pa = blk_ascii;
    word9 * p9 = blk_word9;
    for (uint i = 0; i < mst_blksz_word9; i ++)
      {
        word9 w9 = extr9 (blk, i);
        * pa ++ = w9 & 0xff;
        * p9 ++ = w9;
      }
    return 0;
  }

struct mst_record_36
  {
    word36 thdr [mst_header_sz_word36];
    word36 data [mst_datasz_word36];
    word36 trlr [mst_trailer_sz_word36];
  };

struct file_hdr_36
  { 
    word36 thdr [mst_header_sz_word36];
    word36 zz1 [8];
    word36 hc [14];
    word36 zz2 [8];
    word36 hdrcnt; // word count for header  // mxload:preamble_length in word36
    word36 segcnt; // word count for data  // mxload:segment_length
    word36 dlen; // bin(17)
    word36 dname [42];
    word36 elen; // bin(17)
    word36 ename [8];
    word36 bitcnt;
    word36 record_type;
    word36 dtd [2];
    word36 dumper_id [8];
  } __attribute__ ((packed));

struct file_hdr_9
  {
    char thdr [mst_header_sz_word9];
    char zz1 [32];
    char hc [56];
    char zz2 [32];
    char hdrcnt [4]; // word36
    char segcnt [4]; // word36
    char dlen [4]; // word36
    char dname [168];
    char elen [4]; // word36
    char ename [32];
    char bitcnt [4]; // word36
    char record_type [4]; // word36
    char dtd [8]; // word72
    char dumper_id [32];
  };
#define const_hc "This is the beginning of a backup logical record.       "
#define const_zz " z z z z z z z z z z z z z z z z"

struct theader_9
  { 
    char thdr [32];
    char zz1 [32];
    char hc [56];
    char zz2 [32];
  };

static char * record_type [] =
  {
    "00", "NDC seg", "NDC dir", "NDC dirlst", "04",
    "05", "06",      "07",      "08",         "09",
    "10", "11",      "12",      "13",         "14",
    "15", "16",      "17",      "18",         "SEG",
    "DIR"
  };

static char dir_name [169];
static char elem_name [33];
static word36 record_typ;
static word36 bit_count;

static int is_file_hdr (void)
  {
    struct theader_9 * p_hdr9 = (struct theader_9 *) blk_ascii;
    return strncmp (p_hdr9 -> zz1, const_zz, 32) == 0 &&
           strncmp (p_hdr9 -> hc, const_hc, 56) == 0 &&
           strncmp (p_hdr9 -> zz2, const_zz, 32) == 0;
  }


int main (int argc, char * argv [])
  {
    int fd;
    if (argc != 2)
      {
        printf ("restore_toc <where from>\n");
        exit (1);
      }
    fd = open (argv [1], O_RDONLY);
    if (fd < 0)
      {
        printf ("can't open tape\n");
        exit (1);
      }

    for (;;)
      {
        int rc = get_mst_record (fd);
        if (rc < 0)
          break;
        if (!is_file_hdr ())
          continue;

        struct file_hdr_9 * fh9p = (struct file_hdr_9 *) blk_ascii;
        struct file_hdr_36 * fh36p = (struct file_hdr_36 *) blk_word36;
        uint sc = fh36p -> segcnt;
        // printf ("hc %u sc %u\n", hc, sc);
        // Bit_count is the number of word9 in the data
        bit_count = fh36p -> bitcnt & 077777777;

        uint maximum_bitcnt = sc * 36;
        if (bit_count == 0 || bit_count > maximum_bitcnt)
          bit_count = maximum_bitcnt;

        uint dlen = (fh36p -> dlen) & 0377777; // bin(17)
        uint elen = (fh36p -> elen) & 0377777; // bin(17)
        if (dlen > 168)
          {
            printf ("truncating dlen");
            dlen = 168;
         }
        if (elen > 32)
          {
            printf ("truncating elen");
            elen = 32;
          }
        strncpy (dir_name, fh9p -> dname, dlen);
        dir_name [dlen] = '\0';
        strncpy (elem_name, fh9p -> ename, elen);
        elem_name [elen] = '\0';

        record_typ = (fh36p ->  record_type) & 0377777; // bin(17);
        char * rt = "(unknown)";
        if (record_typ > 0 && record_typ < 21)
          rt = record_type [record_typ];
        printf ("%8lu %-10s %s>%s\n", bit_count, rt, dir_name, elem_name);
      }
  }
