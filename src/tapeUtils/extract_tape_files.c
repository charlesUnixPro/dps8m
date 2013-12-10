#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bit36.h"


typedef uint16_t word9;
typedef uint64_t word36;
typedef unsigned int uint;

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })



// The tape file is in simh format; each physical record is ecoded
//
//     int32 record_len
//     byte * record_len
//     int32 record_len
//  or
//     int32 0   // tape mark
//



// MST -- Standard record format
//
// MULTICS SYSTEM-PROGRAMMERS' MANUAL Section BB.3.01
// PUBLISHED: 06/02/67
//
// Each physical consists of a 256 word (9216-bit) data space
// enclosed by an eight-word header and an eight-word trailer. The total
// record length is then 272 words (9792 bits).
//
// hdr      8 word36 =   288 bits =   36 bytes
// data   256 word36 =  9216 bits = 1152 bytes
// trlr     8 word36 =   288 bits =   36 bytes
//
// hdr      8 word36 =   288 bits =   36 bytes
// data  1024 word36 = 36864 bits = 4608 bytes
// trlr     8 word36 =   288 bits =   36 bytes
//       ----          -----        ----
//
// total 1040 word36 = 38440 bits = 4680 bytes
//

#define mst_blksz_bytes 4680
#define mst_blksz_ascii 4160 // 4680 * 9 / 8
#define mst_blksz_word36 1040
#define mst_datasz_bytes 4608
// Header, trailer

#define header_c1  0670314355245
#define header_c2  0512556146073
#define trailer_c1 0107463422532
#define trailer_c2 0265221631704

#define header_sz_words 8
#define data_sz_words 1024

// Label


//
// the block will be read into these buffers
//
//   blk:        the packed bit stream from the tape
//   blk_ascii:  unpacked from 9bit and placed in 8bit chars

static uint8_t blk [mst_blksz_bytes];
static uint8_t blk_ascii [mst_blksz_ascii];

static int blk_num = 0;
static uint32_t rec_num;
static uint32_t file_num;
static uint32_t nbits;
static uint32_t admin;
static char * top_level_dir;




// ret: >0 sizeread
//      0 tapemark 
//      -1 eof
//      -2 file format error
//      -3 buffer overrun

static int read_simh_blk (int fd, void * buf, ssize_t buflen)
  {
    ssize_t sz;
    uint32_t blksiz, blksiz2;

    sz = read (fd, & blksiz, sizeof (blksiz));

    if (sz == 0)
      return -1;

    if (sz != sizeof (blksiz))
      {
        //printf ("can't read blksiz\n");
        return -2;
      }

    //printf ("blksiz %d\n", blksiz);

    if (! blksiz)
      {
        //printf ("tapemark\n");
        return 0;
      }

    if (blksiz <= buflen)
      {
        sz = read (fd, blk, blksiz);
        if (sz != blksiz)
          {
            //printf ("can't read blk\n");
            return -2;
          }
      }
    else
      lseek (fd, blksiz, SEEK_CUR);


    sz = read (fd, & blksiz2, sizeof (blksiz2));

    if (sz != sizeof (blksiz2))
      {
        //printf ("can't read blksiz2\n");
        return -2;
      }

    if (blksiz != blksiz2)
      {
        //printf ("can't sync\n");
        return -2;
      }
    return blksiz;
  }


//
// return 0  ok
//        1 tapemark
//        -1 EOF

static int read_mst_blk (int fd)
  {
    int rc = read_simh_blk (fd, blk, mst_blksz_bytes);
    if (rc == 0)
      return 1;
    if (rc < 0)
      return -1;

    if (rc != mst_blksz_bytes)
      {
        printf ("can't read blk\n");
        exit (1);
      }

    {
      uint8_t * pa = blk_ascii;
      for (uint i = 0; i < mst_blksz_word36; i ++)
        {
          word9 w9 = extr9 (blk, i);
          * pa ++ = w9 & 0xff;
        }
    }

    blk_num ++;

    word36 w1 = extr36 (blk, 0);
    //word36 w2 = extr36 (blk, 1);
    //word36 w3 = extr36 (blk, 2);
    word36 w4 = extr36 (blk, 3);
    word36 w5 = extr36 (blk, 4);
    word36 w6 = extr36 (blk, 5);
    //word36 w7 = extr36 (blk, 6);
    word36 w8 = extr36 (blk, 7);

    word36 t1 = extr36 (blk, header_sz_words + data_sz_words + 0);
    //word36 t2 = extr36 (blk, 18 + data_sz_words + );
    //word36 t3 = extr36 (blk, header_sz_words + data_sz_words + 2);
    //word36 t4 = extr36 (blk, header_sz_words + data_sz_words + 3);
    //word36 t5 = extr36 (blk, header_sz_words + data_sz_words + 4);
    //word36 t6 = extr36 (blk, header_sz_words + data_sz_words + 5);
    //word36 t7 = extr36 (blk, header_sz_words + data_sz_words + 6);
    word36 t8 = extr36 (blk, header_sz_words + data_sz_words + 7);

    if (w1 != header_c1)
      {
        printf ("c1 wrong %012lo\n", w1);
      }
    if (w8 != header_c2)
      {
        printf ("c2 wrong %012lo\n", w8);
      }
    if (t1 != trailer_c1)
      {
        printf ("t1 wrong %012lo\n", t1);
      }
    if (t8 != trailer_c2)
      {
        printf ("t2 wrong %012lo\n", t8);
      }

    word36 totbits = w5 & 0777777UL;
    if (totbits != 36864) // # of 9-bit bytes
      {
        printf ("totbits wrong %ld\n", totbits);
      }

    rec_num = (w4 >> 18) & 0777777UL;
    file_num = w4 & 0777777UL;
    nbits = (w5 >> 18) & 0777777UL;
    admin = w6 & 0400000000000UL;

    blk_num ++;
    //printf ("blk_num %6u  rec_num %6u  file_num %6u  nbits %6u  admin %u\n",
      //blk_num, rec_num, file_num, nbits, admin); 
    return 0;
 }

static void print_string (char * msg, int offset, int len)
  {
    uint8_t * p = blk_ascii + offset;
    printf ("%s", msg);
    for (int i = 0; i < len; i ++)
      printf ("%c", isprint (p [i]) ? p [i] : '?');
    printf ("\n");
 }



int main (int argc, char * argv [])
  {
    int fd;
    int num_files = 0;
    if (argc != 3)
      {
        printf ("extract <where from> <where to>\n");
        exit (1);
      }
    top_level_dir = argv [2];
    fd = open (argv [1], O_RDONLY);
    if (fd < 0)
      {
        printf ("can't open tape\n");
        exit (1);
      }

#ifdef SPECIAL_CASE_LABEL
    printf ("Processing label...\n");
    int rc = read_mst_blk (fd);
    if (rc)
      {
        printf ("no label\n");
        exit (1);
      }
    print_string ("Installation ID: ", 32, 32);
    print_string ("Tape Reel ID:    ", 64, 32);
    print_string ("Volume Set ID:   ", 96, 32);
          
    rc = read_mst_blk (fd);
    if (rc != 1)
      {
        printf ("Expected tape mark\n");
        exit (1);
      }
#endif 
    int seg_num = 0;
    for (;;) // every tape marked region
      {
        seg_num ++;
        printf ("Processing segment %d...\n", seg_num);
        int rc = read_mst_blk (fd);
        if (rc == -1)
          {
            printf ("End of tape\n");
            break;
          }
        if (rc == 1)
          {
            printf ("Empty segment\n");
            continue;
          }
        if (rc)
          {
            printf ("Oops\n");
            exit (rc);
          }
#ifndef SPECIAL_CASE_LABEL
        if (seg_num == 1)
          {
            print_string ("Installation ID: ", 32, 32);
            print_string ("Tape Reel ID:    ", 64, 32);
            print_string ("Volume Set ID:   ", 96, 32);
          }
#endif
        num_files ++;
        char file_name [1025];
        sprintf (file_name, "tape_file_%08d.dat", seg_num);
        int fdout = open (file_name, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (fdout < 0)
          {
            printf ("can't open %s\n", file_name);
            exit (1);
          }
        ssize_t n_written = 0;
        for (;;) // for every record
          {
#ifdef DATA_ONLY
#define SZ mst_datasz_bytes // data only
#define OS 36
#else
#define SZ mst_blksz_bytes  // hdr, data, trlr
#define OS 0
#endif
            // hdr, data, trailer
            ssize_t cnt = write (fdout, blk + OS, SZ);
            if (cnt != SZ)
              {
                printf ("error writing\n");
                exit (1);
              }
            n_written += cnt;
            rc = read_mst_blk (fd);
            if (rc == -1)
              break;
            if (rc == 1)
              {
                break;
              }
            if (rc)
              {
                printf ("Oops\n");
                close (fdout);
                exit (rc);
              }
          }
        printf ("%ld bytes written\n", n_written);
        close (fdout);
        if (rc == -1)
          break;
      }
    printf ("%d blocks read\n", blk_num);
    return 0;
  }

