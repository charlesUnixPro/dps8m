/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Take an file from extract_tape_files that mfile as identified as a Multics
// backup file (".archive"), and extract fhe segments.

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "bit36.h"

#ifdef __MINGW64__
#define open(x,y,args...) open(x, y|O_BINARY,##args)
#define creat(x,y) open(x, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, y)
#undef isprint
#define isprint(c) (c>=0x20 && c<=0x7f)
#endif


static int restore_cnt = 0;
static char path [4097]; // sanatized top_level_dir/dir_name
static char filename [4097]; // sanatized elem_name
static char fullname [4097]; // sanatized top_level_dir/dir_name/elem_name
static char mkcmd [4097];
static char * top_level_dir;



int main (int argc, char * argv [])
  {
    int fd;
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

    //int rc = get_mst_record (fd);
    static uint8_t comp_header [100];
    ssize_t sz = read (fd, comp_header, sizeof (comp_header));

    if (sz == 0)
      {
        printf ("empty file\n");
        exit (1);
      }
    if (sz != sizeof (comp_header))
      {
        printf ("can't read comp_header\n");
        exit (1);
      }

/* Each component header is 100 bytes long and has the following fields:

    Field     Length  Offset

    header_begin   8       0
    pad1           4       8
    name          32      12
    timeup        16      44
    mode           4      60
    time          16      64
    pad            4      80
    bit_count      8      84
    header_end     8      92
*/

    static char archive_header_begin[8] =
                    {014, 012, 012, 012, 017, 012, 011, 011};
    static char    archive_header_end[8] =
                    {017, 017, 017, 017, 012, 012, 012, 012};

    static char notice [] = "\r\n\r\n\r\nHistorical Background";

    if (memcmp (comp_header, notice, sizeof (notice)) == 0)
      {
        printf ("Notice at file end found\n");
        exit (1);
      }
        
    if (memcmp (comp_header, archive_header_begin, sizeof (archive_header_begin)) != 0)
      {
        printf ("Not a Multics backup format\n");
        exit (1);
      }

    lseek (fd, 0, SEEK_SET);
    // Process all files in backup
    // do while( ªEOF );
    for (;;) 
      {
        //if (! is_file_hdr())
        sz = read (fd, comp_header, sizeof (comp_header));
printf ("sz %ld\n", sz);
        if (sz == 0)
          break;
        if (sz != sizeof (comp_header))
          {
            printf ("can't read comp_header\n");
            exit (1);
          }
        if (memcmp (comp_header, archive_header_begin, sizeof (archive_header_begin)) != 0)
          {
for (int i = 0; i < 100; i ++) printf (" %d", comp_header [i]);
printf ("\n");
            printf ("Not a Multics backup format (1)\n");
            exit (1);
          }
        if (memcmp (comp_header + 92, archive_header_end, sizeof (archive_header_end)) != 0)
          {



            printf ("Not a Multics backup format (2)\n");
            exit (1);
          }

        char elem_name [33];
        int i;
        for (i = 0; i < 32 && comp_header [i + 12] != ' '; i ++)
          elem_name [i] = comp_header [i + 12];
        elem_name [i] = '\0';
printf ("elem_name %s\n", elem_name);

        uint8_t bit_count_str [9];
        memcpy (bit_count_str, comp_header + 84, 8);
        bit_count_str [8] = '\0';
        long bitcnt = atol ((char *) bit_count_str); // in 9-bit bytes
printf ("bitcnt %ld\n", bitcnt);

        // Number of bytes in the file we are looking at
        long blength = (bitcnt + 8) / 9;
printf ("length in bytes %ld\n", blength);

        // Number of 36-bit words in the original file
        long length = (bitcnt + 35L) / 36L;
        // Number of bits in the original file
        long maximum_bitcnt = length * 36L;
        // Number of 9bit bytes in the original file
        long maximum_byte_cnt = maximum_bitcnt / 9;

        long padding = maximum_byte_cnt - blength;
printf ("padding %ld\n", padding);

        strcpy (path, top_level_dir);
        strcat (path, "/");

        // Build filename
        strcpy (filename, elem_name);
        size_t l = strlen (filename);
        for (int i = 0; i < l; i ++)
          if (filename [i] == '/')
            filename [i] = '+';

        restore_cnt ++;

        if (bitcnt == 0)
          {
            printf ("create empty file () %s%s\n", path, filename);
          }

        //long cnt = (bitcnt + 8) / 9; /* Num of bytes rounded UP */
//printf ("cnt %ld\n", cnt);

#ifndef __MINGW64__
        sprintf (mkcmd, "mkdir -p %s", path);
#else
        static char dosname [4097]; 
        strcpy (dosname, path);
        for (int i = 0; i < strlen (dosname); i ++)
          if (dosname [i] == '/')
            dosname [i] = '\\';
        sprintf (mkcmd, "cmd /e:on /c mkdir %s", dosname);
#endif
        int rc = system (mkcmd);
        if (rc)
          printf ("mkdir returned %d %s\n", rc, mkcmd);

        strcpy (fullname, path);
        strcat (fullname, filename);
        int fdout = open (fullname, O_WRONLY | O_CREAT | O_TRUNC, 0664);
        if (fdout < 0)
          {
            printf ("can't open file for writing\n");
            exit (1);
          }

        uint8_t buf [256];
        while (blength > 0)
          {
            int chunk = blength > 256 ? 256 : blength;
            sz = read (fd, buf, chunk);
            if (sz == 0)
              {
                printf ("eof while reading chunk\n");
                exit (1);
              }
            if (sz != chunk)
              {
                printf ("can't chunk %d %ld\n", chunk, sz);
                exit (1);
              }
//printf ("chunked %d\n", chunk);
            sz = write (fdout, buf, chunk);
            if (sz != chunk)
              {
                printf ("error writing chunk\n");
                exit (1);
              }
            blength -= chunk;
          }
        lseek (fd, padding, SEEK_CUR);
//printf ("maximum_bitcnt %ld bitcnt %ld\n", maximum_bitcnt, bitcnt);
        //long trailing_bits = (maximum_bitcnt - bitcnt) * 8L / 9L;
        //long trailing_bytes = trailing_bits * 7L / 8L;
//printf ("trailing_bytes %ld\n", trailing_bytes);
        //lseek (fd, trailing_bytes, SEEK_CUR);
        close (fdout);
      }
    printf ("%d files restored\n", restore_cnt);
    return 0;
  }
