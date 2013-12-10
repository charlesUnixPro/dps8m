// Multics file command. Take a list of .tap files created by 
// extract_tape_files.c, look at the header of each, and try to determine
// what kind of file it is.

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>


#include "mst.h"
#include "bit36.h"

typedef uint16_t word9;
typedef uint64_t word36;
typedef unsigned int uint;

static uint8_t blk [mst_blksz_bytes];
static uint8_t blk_ascii [mst_blksz_ascii];
static word9 blk_word9 [mst_blksz_ascii];
static word36 blk_word36 [mst_blksz_word36];


static void print_string (char * msg, int offset, int len)
  {
    uint8_t * p = blk_ascii + offset;
    printf ("%s", msg);
    for (int i = 0; i < len; i ++)
      printf ("%c", isprint (p [i]) ? p [i] : '?');
    printf ("\n");
 }

struct mst_record_36
  {
    word36 thdr [mst_header_sz_words36];
    word36 data [mst_datasz_word36];
    word36 trlr [mst_trailer_sz_words36];
  };

struct mst_boot_record_36
  {
    word36 tvec [8];
    word36 thdr [mst_header_sz_words36];
    word36 data [mst_datasz_word36 - 8];
    word36 trlr [mst_trailer_sz_words36];
  };

#define const_hc "This is the beginning of a backup logical record.       "
#define const_zz " z z z z z z z z z z z z z z z z"

struct theader_8
  { 
    char thdr [32];
    char zz1 [32];
    char hc [56];
    char zz2 [32];
  };


struct theader_36
  { 
   word36 thdr [header_sz_words];
   word36 zz1 [8];
   word36 hc [14];
   word36 zz2 [8];
   word36 hdrcnt; // word count for header
   word36 segcnt; // word count for data
   word36 dlen;
   word36 dname [42];
   word36 elen;
   word36 ename [8];
   word36 bitcnt;
   word36 record_type;
   word36 dtd [2];
   word36 dumper_id [8];
 } __attribute__ ((packed));

static void mfile (char * fname)
  {
    printf ("%s\n", fname);
    int fd = open (fname, O_RDONLY);
    if (fd < 0)
      {
        printf ("  error opening; skipping\n");
        return;
      }

    // Read the first mst record
    int rc = read (fd, blk, mst_blksz_bytes);
    if (rc != mst_blksz_bytes)
      {
        printf ("  error reading; skipping\n");
        goto done;
      }
     
    {
      uint8_t * pa = blk_ascii;
      word9 * p9 = blk_word9;
      word36 * p36 = blk_word36;
      for (uint i = 0; i < mst_blksz_word36; i ++)
        {
          * p36 ++ = extr36 (blk, i);
          word9 w9 = extr9 (blk, i);
          * pa ++ = w9 & 0xff;
          * p9 ++ = w9;
        }
    }


   struct mst_record_36 * p = (struct mst_record_36 *) blk_word36;
   struct mst_boot_record_36 * boot_p = (struct mst_boot_record_36 *) blk_word36;
    if (p -> thdr [0] == header_c1 &&
        p -> thdr [7] == header_c2 &&
        p -> trlr [0] == trailer_c1 &&
        p -> trlr [7] == trailer_c2)
      {
        // Appears to be a valid mst_header

        word36 totbits = p -> thdr [4] & 0777777UL;
        if (totbits != 36864) // # of 9-bit bytes
          {
            printf ("  totbits wrong %ld\n", totbits);
          }

        uint rec_num = (p -> thdr [3] >> 18) & 0777777UL;
        uint file_num = p -> thdr [3] & 0777777UL;
        uint nbits = (p -> thdr [4] >> 18) & 0777777UL;
        uint admin = p -> thdr [5] & 0400000000000UL;

        struct theader_8 * p_hdr8 = (struct theader_8 *) blk_ascii;

        printf ("  mst header  rec_num %6u  file_num %6u  nbits %6u  admin %u\n",
          rec_num, file_num, nbits, admin); 

        if (strncmp (p_hdr8 -> zz1, const_zz, 32) == 0 &&
            strncmp (p_hdr8 -> hc, const_hc, 56) == 0 &&
            strncmp (p_hdr8 -> zz2, const_zz, 32) == 0)
         {
           printf ("  Multics backup format\n");
         }
      }
    else if (boot_p -> tvec [0] == 0000004235000 &&
             boot_p -> tvec [1] == 0000330710000 &&
             boot_p -> tvec [2] == 0000004235000 &&
             boot_p -> tvec [3] == 0000330710000 &&
             boot_p -> tvec [4] == 0000004235000 &&
             boot_p -> tvec [5] == 0000330710000 &&
             boot_p -> tvec [6] == 0000004235000 &&
             boot_p -> tvec [7] == 0000330710000)
      {
        //totbits = w5 & 0777777UL;

        if (boot_p -> thdr [0] == header_c1 &&
            boot_p -> thdr [7] == header_c2 &&
            boot_p -> trlr [0] == trailer_c1 &&
            boot_p -> trlr [7] == trailer_c2)
          {
            printf ("  bootable tape label\n");
            //if (totbits != 36864) // # of 9-bit bytes
              //{
                //printf ("  totbits wrong %ld\n", totbits);
              //}

            uint rec_num = (boot_p -> thdr [3] >> 18) & 0777777UL;
            uint file_num = boot_p -> thdr [3] & 0777777UL;
            uint nbits = (boot_p -> thdr [4] >> 18) & 0777777UL;
            uint admin = boot_p -> thdr [5] & 0400000000000UL;

            printf ("  mst boot header  rec_num %6u  file_num %6u  nbits %6u  admin %u\n",
              rec_num, file_num, nbits, admin); 

            print_string ("  Installation ID: ", 32 + 32, 32);
            print_string ("  Tape Reel ID:    ", 32 + 64, 32);
            print_string ("  Volume Set ID:   ", 32 + 96, 32);
          }
        else
          {
            printf ("  bootable tape label with broken MST header\n");
          }
      }
    else
      {
        printf ("  don't recognize it; skipping\n");
      }
  done:
    if (fd >= 0)
      close (fd);
    return;
  }

int main (int argc, char * argv [])
  {
    for (int i = 1; i < argc; i ++)
      {
        printf ("==================================\n");
        mfile (argv [i]);
      }
    
  }
