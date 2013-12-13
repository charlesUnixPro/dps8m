// Take an file from extract_tape_files that mfile as identified as a Multics
// backup file, and extract fhe segments.

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

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
static int blk_no = 0;

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
    blk_no ++;
    return 0;
  }

struct mst_record_36
  {
    word36 c1;
    word36 uid [2];
    word36 rec_within_file__phy_file;
    word36 data_bits_used__data_bit_len;
    word36 flags__header_version__repeat_count;
    word36 checksum;
    word36 c2;

    word36 data [mst_datasz_word36];

    word36 t_c1;
    word36 t_uid [2];
    word36 tot_data_bits;
    word36 pad_pattern;
    word36 reel_num__tot_file;
    word36 tot_rec;
    word36 t_c2;
  };

int main (int argc, char * argv [])
  {
    int fd;
    if (argc != 2)
      {
        printf ("scan <where from>\n");
        exit (1);
      }
    fd = open (argv [1], O_RDONLY);
    if (fd < 0)
      {
        printf ("can't open tape\n");
        exit (1);
      }

    // Process all records in file

    uint blk_num = 0;

    for (;;) 
      {
        int rc = get_mst_record (fd);
        if (rc < 0)
          break;
        blk_num ++;

        struct mst_record_36 * p = (struct mst_record_36 *) blk_word36;

        word36 c1 = p -> c1;
        word36 c2 = p -> c2;
        word36 t_c1 = p -> t_c1;
        word36 t_c2 = p -> t_c2;

        if (c1 != header_c1)
          {
            printf ("c1 wrong %012lo\n", c1);
            continue;
          }
        if (c2 != header_c2)
          {
            printf ("c2 wrong %012lo\n", c2);
            continue;
          }
        if (t_c1 != trailer_c1)
          {
            printf ("t1 wrong %012lo\n", t_c1);
            continue;
          }
        if (t_c2 != trailer_c2)
          {
            printf ("t2 wrong %012lo\n", t_c2);
            continue;
          }
        word36 rec_within_file = (p -> rec_within_file__phy_file >> 18) & 0777777;
        word36 phy_file = (p -> rec_within_file__phy_file) & 0777777;
        word36 data_bits_used = (p -> data_bits_used__data_bit_len >> 18) & 0777777;
        word36 data_bit_len = (p -> data_bits_used__data_bit_len) & 0777777;
        word36 flags = (p -> flags__header_version__repeat_count >> 12) & 077777777;
        word36 header_version = (p -> flags__header_version__repeat_count >> 9) & 07;
        word36 repeat_count = (p -> flags__header_version__repeat_count >> 0) & 0777;

        if (blk_num % 16 == 1)
          printf ("     blk    rec#   file#   #bits     len     flags ver repeat\n");

        printf ("%8u%8lu%8lu%8lu%8lu%10lu%4lu%5lu\n", 
                blk_num, rec_within_file, phy_file, data_bits_used, data_bit_len, 
                flags, header_version, repeat_count);
        if (flags)
          {
            printf ("             flags:");
            if (flags & 040000000) printf (" admin");
            if (flags & 020000000) printf (" label");
            if (flags & 010000000) printf (" eor");
            if (flags & 007776000) printf (" pad1");
            if (flags & 000001000) printf (" set");
            if (flags & 000000400) printf (" repeat");
            if (flags & 000000200) printf (" padded");
            if (flags & 000000100) printf (" eot");
            if (flags & 000000040) printf (" drain");
            if (flags & 000000020) printf (" continue");
            if (flags & 000000017) printf (" pad2");
            printf ("\n");
          }
      }
  }
