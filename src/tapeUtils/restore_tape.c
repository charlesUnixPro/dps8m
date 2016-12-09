/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Restore the contents of a Multics backup tape

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "simhtapes.h"

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

static int argc_copy;
static char * * argv_copy;
static int next_tape;
static int n_blocks = 0;

// The tape file is in simh format; each physical record is ecoded
//
//     int32 record_len
//     byte * record_len
//     int32 record_len
//  or
//     int32 0   // tape mark
//
#define max_blksiz 18432 // largest observed in the wild; make larger if needed

//
// the block will be read into these buffers
//
//   tape_blk:        the packed bit stream from the tape
//   tape_blk_ascii:  unpacked from 9bit and placed in 8bit chars

//static uint8_t tape_blk [max_blksiz /*mst_blksz_bytes*/];
//static uint8_t tape_blk_ascii [mst_blksz_word9];
static int blksiz;
static int blk_num = 0;
static uint32_t rec_num;
static uint32_t file_num;
static uint32_t nbits;
static uint32_t admin;
static char * top_level_dir;


//
// return 0  ok
//        1 tapemark
//        -1 EOF
//        -2 buffer overrun
//        -3 not an MST block

static int read_mst_blk (int fd)
  {
    blksiz = read_simh_blk (fd, blk, max_blksiz /*mst_blksz_bytes*/);
    n_blocks ++;
    if (blksiz == 0) // tapemar
      return 1;
    if (blksiz == -3) // buffer overrun
      return -2;
    if (blksiz < 0) // EOF or ERROR
      return -1;

    if (blksiz != mst_blksz_bytes)
      {
        printf ("not an MST blk\n");
        return -3;
      }

// Check to see if the next block(s) are rewrites
    off_t where = lseek (fd, 0, SEEK_CUR);

    for (;;)
      {
        uint8_t nxt [mst_blksz_bytes];
        int rc = read_simh_blk (fd, nxt, mst_blksz_bytes);
        if (rc == -1) // eof; so no next block
          {
            // not a repeat; put the file position back
            where = lseek (fd, where, SEEK_SET);
            break;
          }
        if (rc == 1) // tape mark; keep looking
          continue;

        word36 w6 = extr36 (nxt, 5);     // flags
        //printf ("flags %036lo\n", w6);
        word36 repeat = w6 & (1LU << 20);
        if (repeat)
          {
            printf ("repeat\n");
            memcpy (blk, nxt, sizeof (blk));
            where = lseek (fd, 0, SEEK_CUR);
          }
        else
          {
            // not a repeat; put the file position back
            where = lseek (fd, where, SEEK_SET);
            break;
          }
      }
    {
      uint8_t * pa = blk_ascii;
      for (uint i = 0; i < mst_blksz_word9; i ++)
        {
          word9 w9 = extr9 (blk, i);
          * pa ++ = w9 & 0xff;
        }
    }

    blk_num ++;

    word36 w1 = extr36 (blk, 0);     // c1
    //word36 w2 = extr36 (blk, 1);   // uid
    //word36 w3 = extr36 (blk, 2);   // uid
    word36 w4 = extr36 (blk, 3);     // re_within_file, phy_file
    word36 w5 = extr36 (blk, 4);     // data_bits_used, data_bit_len
    word36 w6 = extr36 (blk, 5);     // flags
    //word36 w7 = extr36 (blk, 6);   // checksum
    word36 w8 = extr36 (blk, 7);     // c2

    word36 t1 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 0);
    //word36 t2 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 1);
    //word36 t3 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 2);
    //word36 t4 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 3);
    //word36 t5 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 4);
    //word36 t6 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 5);
    //word36 t7 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 6);
    word36 t8 = extr36 (blk, mst_header_sz_word36 + mst_datasz_word36 + 7);

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

// return 0  ok
//        1 tapemark
//        -1 EOF (no more tapes)
//        -2 buffer overrun
//        -3 not an MST block

static int open_next_tape (int * fd)
  {
    if (* fd != -1)
      {
        close (* fd);
      }
    if (next_tape >= argc_copy)
      {
        // no more tapes
        return -1;
      }
    * fd = open (argv_copy [next_tape], O_RDONLY);
    if (* fd < 0)
      {
        printf ("can't open tape\n");
        exit (1);
      }

    printf ("Reading tape file: %s\n", argv_copy [next_tape]);

    next_tape ++;
    n_blocks = 0;

    // Process label

    //printf ("Processing label...\n");
    printf ("Tape label information:\n");
    int rc = read_mst_blk (* fd);
    if (rc)
      {
        printf ("no label\n");
        exit (1);
      }
    //print_string ("Installation ID: ", 32, 32);
    //print_string ("Tape Reel ID:    ", 64, 32);
    //print_string ("Volume Set ID:   ", 96, 32);
    print_string ("  ", 32, 32);
    print_string ("  ", 64, 32);
    print_string ("  ", 96, 32);
          
    rc = read_mst_blk (* fd);
    if (rc != 1)
      {
        printf ("Expected tape mark\n");
        exit (1);
      }
    return 0;
  }

// return 0  ok
//        1 tapemark
//        -1 EOF (no more tapes)
//        -2 buffer overrun
//        -3 not an MST block

static int n_records = 0;

static int get_mst_record (int * fd)
  {
    if (* fd == -1)
      {
next:;
        int rc = open_next_tape (fd);
        if (rc < 0)
          return -1; // no more tapes
        n_records = 0;
      }
again:;
    int rc = read_mst_blk (* fd);
    // Every 128 blocks is a tape mark
    if (rc == 1 && n_records % 128 == 0)
        goto again;
    if (rc == 1)
      goto next;
    if (rc == -1)
      goto next;
    if (rc)
      return rc;
    n_records ++;
    if (blksiz != mst_blksz_bytes)
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

static int restore_cnt = 0;
static char path [4097]; // sanatized top_level_dir/dir_name
static char filename [4097]; // sanatized elem_name
static char dirname [4097]; // sanatized dir_name
static char fullname [4097]; // sanatized top_level_dir/dir_name/elem_name
static char dosname [4097]; 
static char asciiname [4097]; 
static char mdname [4097]; 
static char mkcmd [4097];
static char * top_level_dir;
static char dir_name [169];
static char elem_name [33];
static word36 record_typ;
static word36 bit_count;
static word36 data_start;

static int is_file_hdr (void)
  {
    struct theader_9 * p_hdr9 = (struct theader_9 *) blk_ascii;
    return strncmp (p_hdr9 -> zz1, const_zz, 32) == 0 &&
           strncmp (p_hdr9 -> hc, const_hc, 56) == 0 &&
           strncmp (p_hdr9 -> zz2, const_zz, 32) == 0;
  }


// n is the word9 offset into the data region to start from
// chan_cnt is the number of word9's to write
// assumes n is a multiple of 8 (word72 aligned)
//

static void write_binary_data (int fdout, uint n, uint char_cnt)
  {
    uint x = (n * 9) / 8;
    //uint cc = (char_cnt * 9) / 8; // restore.pli loses trailing  bits
    uint cc = ((char_cnt * 9) + 7) / 8;
    int rc = write (fdout, blk + mst_header_sz_bytes + x, cc);
    if (rc != cc)
      {
         printf ("write failed\n");
         exit (1);
      }
  }

static void write_ascii_data (int fdout, uint n, uint char_cnt)
  {
    int rc = write (fdout, blk_ascii + mst_header_sz_word9 + n, char_cnt);
    if (rc != char_cnt)
      {
         printf ("write failed\n");
         exit (1);
      }
  }

static int check_ASCII (uint n, uint cnt)
  {
    for (int i = 0; i < cnt; i ++)
      {
        word9 w9 = blk_word9 [mst_header_sz_word9 + n + i];
        if (w9 >127)
          return 0;
        if (w9 < 32 &&
            w9 != '\000' &&
            w9 != '\t' &&
            w9 != '\n' &&
            w9 != '\v' &&
            w9 != '\f' &&
            w9 != '\r')
          return 0;
      }
    return 1;
  }

int main (int argc, char * argv [])
  {
    int fd;
    if (argc < 3)
      {
        printf ("extract <path to restore to> [list of .tap files]\n");
        exit (1);
      }
    top_level_dir = argv [1];

    argc_copy = argc;
    argv_copy = argv;
    next_tape = 2;
    fd = -1;

    int rc = get_mst_record (& fd);
    if (rc < 0)
      {
        printf ("empty file\n");
        exit (1);
      }
    if (! is_file_hdr ())
      {
        printf ("Not a Multics backup format\n");
        exit (1);
      }

    // Process all files in backup
    // do while( ªEOF );
    for (;;) 
      {
        if (! is_file_hdr())
          {
            //printf ("searching for header block\n");
            for (;;) // do while (! eof)
              {
                int rc = get_mst_record (& fd);
                if (rc < 0)
                  goto eof;
                if (is_file_hdr())
                  break;
              }
          }
        struct file_hdr_9 * fh9p = (struct file_hdr_9 *) blk_ascii;
        struct file_hdr_36 * fh36p = (struct file_hdr_36 *) blk_word36;
        uint hc = fh36p -> hdrcnt;
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
        //printf ("%8lu %-10s %s>%s\n", bit_count, rt, dir_name, elem_name);
        printf ("%5d %-15s %8lu %s>%s\n", n_blocks, rt, bit_count, dir_name, elem_name);

        /* Skip over the rest of the header and      */
        /* segment information.                      */
        /* (32 is the length of the preamble)        */

        /* This is the number of words in the header */

// this is the restore.pli code: file has junk at beginning
// Apparently, the preamble allocates itself in 256 word36 chunks;

        word36 wh = hc + 32 + (256 - 1);
        wh = wh - wh % 256;
        /* This is the number of words in the segments */
        word36 ws = sc + (mst_datasz_word36 - 1);
        ws = ws - ws % mst_datasz_word36;

        /* May be extra data in segment */
        int seg_cnt = ws;

        while (wh > mst_datasz_word36)      /* Find the right record */
          {
            //printf ("whittling down wh %lu\n", wh);
            int rc = get_mst_record (& fd);
            if (rc < 0)
              {
                printf ("unexpected eof 1\n");
                exit (1);
              }
            wh -= mst_datasz_word36;
          }

        // p = addr(MstBlkData) + stg(NULL->mstr_header);

        data_start = wh; // ??? offset from start of data segment in word36
        //printf ("data_start %u\n", data_start);
        if (record_typ != 19)
          {
            get_mst_record (& fd);
            if (rc < 0)
              goto eof;
            continue; // not a segment
          }
        // Build path from top_level_dir and dir_name

        strcpy (path, top_level_dir);
        strcat (path, "/");

        if (dir_name [0] == '>')
          strcpy (dirname, dir_name + 1);
        else
          strcpy (dirname, dir_name);
        size_t l = strlen (dirname);
        for (int i = 0; i < l; i ++)
          if (dirname [i] == '/')
            dirname [i] = '+';
        for (int i = 0; i < l; i ++)
          if (dirname [i] == '>')
            dirname [i] = '/';

        strcat (path, dirname);
        strcat (path, "/");

        // Build filename
        strcpy (filename, elem_name);
        l = strlen (filename);
        for (int i = 0; i < l; i ++)
          if (filename [i] == '/')
            filename [i] = '+';

        restore_cnt ++;

        if (bit_count == 0)
          {
            printf ("create empty file () %s%s\n", path, filename);
          }

        uint char_in_blk = (mst_datasz_word36 - data_start) * 4; /* Char in 1st blk */
        uint n_words_in_blk = mst_datasz_word36 - data_start;       /* Word in 1st blk */
        /* The data will always start on an even word boundary */
        uint cnt = bit_count/9;              /* Num of characters rounded DOWN */
        uint cntx = min (char_in_blk, cnt);       /* Characters to check */
        // restore.pl1 decides that if the first block of the file
        // contains isprint() | NUL, HT, LF, VT, FF, CR means
        // the file is ascii; we'll just always extract binary, and
        // let the user post-process as needed
        // int isASCII = check_ASCII( pData, DataStart*4, cntx );

        int isASCII = 0; // check_ASCII (data_start * 4, cntx);

        cnt = (bit_count + 8) / 9; /* Num of characters rounded UP */

        sprintf (mkcmd, "mkdir -p %s", path);
        int rc = system (mkcmd);
        if (rc)
          printf ("mkdir returned %d %s\n", rc, mkcmd);

        strcpy (fullname, path);
        strcat (fullname, filename);

        char ext [4096];
        ext [0] = 0;

        for (int i = strlen (filename) - 1; i >= 0; i --)
          {
            if (filename [i] == '.')
              {
                strcpy (ext, filename + i);
                break;
              }
          }

        strcpy (dosname, fullname);
        for (int i = 0; i < strlen (dosname); i ++)
          if (dosname [i] == '/')
            dosname [i] = '\\';

        printf ("      Creating BINARY file %s (%s)\n", dosname, ext);

        int fdout = open (fullname, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (fdout < 0)
          {
            printf ("can't open file for writing\n");
            exit (1);
          }

        int fdouta = -1;
        if (isASCII)
          {
            strcpy (asciiname, fullname);
            strcat (asciiname, ".ascii");
            printf ("      Creating ASCII file %s (%s)\n", dosname, ext);
            fdouta = open (asciiname, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            if (fdouta < 0)
              {
                printf ("can't open file for writing\n");
                exit (1);
              }
          }

        /*-------------------------*/
        /* Restore the File        */
        /*-------------------------*/

        while (seg_cnt > 0)
          {
            cntx = min (char_in_blk, cnt); /* Characters to write */
            write_binary_data (fdout, data_start * 4, cntx);
            if (isASCII)
              write_ascii_data (fdouta, data_start * 4, cntx);
            cnt = cnt - cntx;              /* Character left to write */
            seg_cnt = seg_cnt - n_words_in_blk;
            if (seg_cnt <= 0)
              break;
            int rc = get_mst_record (& fd);
            if (rc < 0)
              {
                printf ("unexpected eof 2\n");
                exit (1);
              }
            char_in_blk = 4096;
            n_words_in_blk = 1024;
            data_start=0;
          }
        off_t bw = lseek (fdout, 0, SEEK_CUR); 
        //printf ("%ld bytes written; %ld bits,%.1f word36, %.1f word9\n", bw, bw * 8, ((float) bw) * 8 / 36, ((float) bw) * 8 / 9);
        close (fdout);
        if (isASCII)
          close (fdouta);

        // The size of the segment may be larger than the bit count indicates
        // read any extra data here
        while (seg_cnt > 0)
          {
            int rc = get_mst_record (& fd);
            if (rc < 0)
              {
                printf ("unexpected eof 2\n");
                exit (1);
              }
            seg_cnt -= mst_datasz_word36;
          }


        // Save the metadata.

        strcpy (mdname, path);
        strcat (mdname, ".");
        strcat (mdname, filename);
        strcat (mdname, ".md");
        int fdoutm = open (mdname, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (fdoutm < 0)
          {
            printf ("can't open file for writing\n");
            exit (1);
          }
        char mbuf [256];
        sprintf (mbuf, "bitcnt: %ld\n", bit_count);
        write (fdoutm, mbuf, strlen (mbuf));
        close (fdoutm);

        rc = get_mst_record (& fd);
        if (rc < 0)
          break;
      }
  eof:
    printf ("%d files restored\n", restore_cnt);
  }
