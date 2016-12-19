/*
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Make a UNIX tree into a Multics dump tape, convert ASCII to p72.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef __MINGW64__
#include <dirent_gnu.h>
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <stdbool.h>

#ifdef __MINGW64__
#define open(x,y,args...) open(x, y|O_BINARY,##args)
#define creat(x,y) open(x, O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, y)
#endif



// Each tape block 
//
// 1024 word data

#define datasz_bytes (1024 * 36 / 8)
#define lbl_datasz_bytes ((1024 + 16) * 36 / 8)

// Layout of data as read from simh tape format
//
//   bits: buffer of bits from a simh tape. The data is
//   packed as 2 36 bit words in 9 eight bit bytes (2 * 36 == 7 * 9)
//   The of the bytes in bits is
//      byte     value
//       0       most significant byte in word 0
//       1       2nd msb in word 0
//       2       3rd msb in word 0
//       3       4th msb in word 0
//       4       upper half is 4 least significant bits in word 0
//               lower half is 4 most significant bit in word 1
//       5       5th to 13th most signicant bits in word 1
//       6       ...
//       7       ...
//       8       least significant byte in word 1
//

// Data conversion routines
//
//  'bits' is the packed72  bit stream
//    it is assumed to start at an even word36 address
//
//   extr36
//     extract the word36 at woffset
//


typedef __uint128_t word72;
typedef uint64_t word36;
typedef uint32_t word18;
typedef uint8_t word1;
typedef unsigned int uint;

#define MASK36 0777777777777l
#define BIT37 01000000000000l


static word36 extr36 (uint8_t * bits, uint woffset)
  {
    uint isOdd = woffset % 2;
    uint dwoffset = woffset / 2;
    uint8_t * p = bits + dwoffset * 9;

    uint64_t w;
    if (isOdd)
      {
        w  = ((uint64_t) (p [4] & 0xf)) << 32;
        w |=  (uint64_t) (p [5]) << 24;
        w |=  (uint64_t) (p [6]) << 16;
        w |=  (uint64_t) (p [7]) << 8;
        w |=  (uint64_t) (p [8]);
      }
    else
      {
        w  =  (uint64_t) (p [0]) << 28;
        w |=  (uint64_t) (p [1]) << 20;
        w |=  (uint64_t) (p [2]) << 12;
        w |=  (uint64_t) (p [3]) << 4;
        w |= ((uint64_t) (p [4]) >> 4) & 0xf;
      }
    // mask shouldn't be neccessary but is robust
    return (word36) (w & MASK36);
  }

static void put36 (word36 val, uint8_t * bits, uint woffset)
  {
    uint isOdd = woffset % 2;
    uint dwoffset = woffset / 2;
    uint8_t * p = bits + dwoffset * 9;

    if (isOdd)
      {
        p [4] &=               0xf0;
        p [4] |= (val >> 32) & 0x0f;
        p [5]  = (val >> 24) & 0xff;
        p [6]  = (val >> 16) & 0xff;
        p [7]  = (val >>  8) & 0xff;
        p [8]  = (val >>  0) & 0xff;
        //w  = ((uint64_t) (p [4] & 0xf)) << 32;
        //w |=  (uint64_t) (p [5]) << 24;
        //w |=  (uint64_t) (p [6]) << 16;
        //w |=  (uint64_t) (p [7]) << 8;
        //w |=  (uint64_t) (p [8]);
      }
    else
      {
        p [0]  = (val >> 28) & 0xff;
        p [1]  = (val >> 20) & 0xff;
        p [2]  = (val >> 12) & 0xff;
        p [3]  = (val >>  4) & 0xff;
        p [4] &=               0x0f;
        p [4] |= (val <<  4) & 0xf0;
        //w  =  (uint64_t) (p [0]) << 28;
        //w |=  (uint64_t) (p [1]) << 20;
        //w |=  (uint64_t) (p [2]) << 12;
        //w |=  (uint64_t) (p [3]) << 4;
        //w |= ((uint64_t) (p [4]) >> 4) & 0xf;
      }
  }


static int tapefd = -1;
 

static uint8_t blk [datasz_bytes];

static void writeMark (void)
  {
    const int32_t zero = 0;
    write (tapefd, & zero, sizeof (zero));
  }

static void writeBlk (void * blk, int32_t nbytes)
  {
#if 1
    // Broken simh requires even blk sizes
    if (nbytes % 2)
      nbytes ++;
    write (tapefd, & nbytes, sizeof (int32_t));
    write (tapefd, blk, nbytes);
    write (tapefd, & nbytes, sizeof (int32_t));
#else
    const int32_t blksiz = datasz_bytes;
    write (tapefd, & blksiz, sizeof (blksiz));
    write (tapefd, & blk [0], sizeof (blk));
    write (tapefd, & blksiz, sizeof (blksiz));
#endif
  }

static void zeroBlk (void)
  {
    memset (blk, -1, sizeof (blk));
  }

static uint8_t lblk [lbl_datasz_bytes];

static void setLabel (void)
  {
    // installation id
    put36 (0124145163164l, & lblk [0],  8); // "Test"
    put36 (0040040040040l, & lblk [0],  9); // "    "
    put36 (0040040040040l, & lblk [0], 10); // "    "
    put36 (0040040040040l, & lblk [0], 11); // "    "
    put36 (0040040040040l, & lblk [0], 12); // "    "
    put36 (0040040040040l, & lblk [0], 13); // "    "
    put36 (0040040040040l, & lblk [0], 14); // "    "
    put36 (0040040040040l, & lblk [0], 15); // "    "
    // tape reel id
    put36 (0124145163164l, & lblk [0], 16); // "Test"
    put36 (0040040040040l, & lblk [0], 17); // "    "
    put36 (0040040040040l, & lblk [0], 18); // "    "
    put36 (0040040040040l, & lblk [0], 19); // "    "
    put36 (0040040040040l, & lblk [0], 20); // "    "
    put36 (0040040040040l, & lblk [0], 21); // "    "
    put36 (0040040040040l, & lblk [0], 22); // "    "
    put36 (0040040040040l, & lblk [0], 23); // "    "
    // volume set id
    put36 (0124145163164l, & lblk [0], 24); // "Test"
    put36 (0040040040040l, & lblk [0], 25); // "    "
    put36 (0040040040040l, & lblk [0], 26); // "    "
    put36 (0040040040040l, & lblk [0], 27); // "    "
    put36 (0040040040040l, & lblk [0], 28); // "    "
    put36 (0040040040040l, & lblk [0], 29); // "    "
    put36 (0040040040040l, & lblk [0], 30); // "    "
    put36 (0040040040040l, & lblk [0], 31); // "    "
  }

static word72 uid0 = 0000001506575ll;
static word72 uid1 = 0750560343164ll;
static word72 cumulative = 0;

static void cksum (word36 * a, word1 * carry, word36 val, bool rotate)
  {
    //word36 val = extr36 (& blk [0], idx);
//printf ("in %12lo %o %12lo\n", * a, * carry, val);
    (* a) += val + * carry;
    (* carry) = ((* a) & BIT37) ? 1 : 0;
    (* a) &= MASK36;
    if (rotate)
      {
        (* a) <<= 1;
        (* a) |= ((* a) & BIT37) ? 1 : 0;
        (* a) &= MASK36;
      }
//printf ("out %12lo %o\n", * a, * carry);
  }
     
static void setHdr (word18 ndatabits, 
                    word1 label, word1 eor, word1 padding)
  {
    //word36 w3 = ((word36) recno) << 18 | filno;
    word36 w3 = 0;
    word36 w4 = ((word36) ndatabits) << 18 | 0110000;
    word1 admin = label | eor;
    word1 admin2 = padding;
    word36 w5 = ((word36) admin)   << (35 -  0) |
                ((word36) label)   << (35 -  1) |
                ((word36) eor)     << (35 -  2) |
                ((word36) admin2)  << (35 - 14) |
                ((word36) padding) << (35 - 16) |
                ((word36) 1)       << (35 - 26); // version
    put36 (0670314355245l, & lblk [0], 0);
    put36 (uid0,           & lblk [0], 1);
    put36 (uid1,           & lblk [0], 2);
    put36 (w3,             & lblk [0], 3);
    put36 (w4,             & lblk [0], 4);
    put36 (w5,             & lblk [0], 5);
    put36 (0,              & lblk [0], 6);
    put36 (0512556146073l, & lblk [0], 7);

    put36 (0107463422532l, & lblk [0], 0 + 8 + 1024);
    put36 (uid0,           & lblk [0], 1 + 8 + 1024);
    put36 (uid1,           & lblk [0], 2 + 8 + 1024);
    cumulative += ndatabits;
    put36 (cumulative,     & lblk [0], 3 + 8 + 1024);
    put36 (0777777777777l, & lblk [0], 4 + 8 + 1024);
    //put36 (filno,          & lblk [0], 5 + 8 + 1024);
    put36 (0,              & lblk [0], 5 + 8 + 1024);
    //put36 (recno,          & lblk [0], 6 + 8 + 1024);
    put36 (0,              & lblk [0], 6 + 8 + 1024);
    put36 (0265221631704l, & lblk [0], 7 + 8 + 1024);

    word36 a = 0;
    word1 carry = 0;

    for (int i = 0; i < 8; i ++)
      if (i != 6)
        cksum (& a, & carry, extr36 (& lblk [0], i), true);
    for (int i = 0; i < 8; i ++)
      cksum (& a, & carry, extr36 (& lblk [0], i + 8 + 1024), true);
    cksum (& a, & carry, 0, false);
    put36 (a,              & lblk [0], 6);
    
    uid1 += 4;
  }

static void writeLabel (void)
  {
    memset (lblk, -1, sizeof (lblk));
    setLabel ();
    setHdr (0001540, 1, 0, 1);
    const int32_t blksiz = lbl_datasz_bytes;
    write (tapefd, & blksiz, sizeof (blksiz));
    write (tapefd, & lblk [0], sizeof (lblk));
    write (tapefd, & blksiz, sizeof (blksiz));
    writeMark ();
  }

static void usage (void)
  {
    printf ("cdump root tapename\n");
    exit (1);
  }

// offset: offset into blk
// str: zero terminated string
// len: number of words in blk;

static void putStr (int offset, char * str, int len)
  {
//printf ("putstr %s\n", str);
    int sl = strlen (str);
    for (int os = 0; os < len; os ++)
      {
        word36 w = 0;
        for (int i = 0; i < 4; i ++)
          {
            if (os * 4 + i < sl)
              {
                w |= ((word36) str [os * 4 + i]) << ((3 - i) * 9);
//printf ("  %012lo\n", w);
              }
            else
              w |= ((word36) ' ') << ((3 - i) * 9);
          }
        put36 (w, & blk [0], offset + os);
      }
  }

enum type { dir = 0, seg = 1, end = 2 };
static void setBckupHdr (enum type t, char * mpath, char * name,
                         word36 bitcnt)
  {
    put36 (t, & blk [0], 0);
    put36 (bitcnt, & blk [0], 1);
    put36 (strlen (mpath), & blk [0], 2);  // dlen
    putStr (3, mpath, 42);                // dname
    put36 (strlen (name),  & blk [0], 45); // elen
    putStr (46, name, 8);                  // ename
// 54 words
  }

static void dumpDir (char * mpath, char * fpath, char * name)
  {
    //printf ("DIR:  %s>%s\n", mpath, name);
    // header

    zeroBlk ();
    setBckupHdr (dir, mpath, name, 0);
    writeBlk (& blk [0], 54 * 36 / 8);
    writeMark ();
  }

static ssize_t readASCII (int datafd, void * buf, size_t count)
  {
    // count in the number of bytes in the p72 buffer
    // How many ASCII 8 bit bytes will fit?
    // count * 8 # of bits
    //   / 9 # of 9-bit chars
    size_t count9 = (count * 8) / 9;
    uint8_t abuf [count9];

    // Get a buffer full of ASCII

    ssize_t nr = read (datafd, abuf, count9);
    if (nr == -1) // error
      {
        perror ("data read");
        exit (1);
      }
    if (nr == 0) // EOF
      return 0;

    // Convert to ASCII 9 and pack into buf
    for (int os = 0; os < nr; os += 4)
      {
        word36 w = 0;
        for (int i = 0; i < 4; i ++)
          {
            if (os + i < nr)
              {
                w |= ((word36) abuf [os + i]) << ((3 - i) * 9);
//printf ("  %012lo\n", w);
              }
            else
              w |= ((word36) '\000') << ((3 - i) * 9);
          }
//printf ("put36 %012lo %p %d\n", w, abuf, os);
        put36 (w, buf, os / 4);
      }

    // How many bytes of packed72 did we generate?
    //  nr # of bytes of ASCII
    //  * 9 # of bits of ASCII9
    //  / 8 # of bytes of packed73
    
    return nr * 9 / 8;
  }

static void dumpASCIIFile (char * mpath, char * fpath, char * name)
  {
    //printf ("FILE: %s>%s\n", mpath, name);
    char fullname [strlen (fpath) + strlen (name) + 2];
    strcpy (fullname, fpath);
    strcat (fullname, "/");
    strcat (fullname, name);
//printf ("%s\n", fullname);

    struct stat buf;
    if (stat (fullname, & buf) != 0)
      {
        perror ("stat");
        exit (1);
      }
//printf ("%s %ld\n", fullname, buf . st_size);

// Calculate the size of the segment that the ASCII file will occupy.

    off_t sz = buf . st_size;
    uint64_t bits = sz * 9;

    // header

    zeroBlk ();
    setBckupHdr (seg, mpath, name, bits);
    writeBlk (& blk [0], 54 * 36 / 8);

    // data

    int datafd = open (fullname, O_RDONLY, 0);
    if (datafd == -1)
      {
        perror ("data open");
        exit (1);
      }
    while (1)
      {
        zeroBlk ();
        //ssize_t nr = read (datafd, & blk [0], datasz_bytes);
        ssize_t nr = readASCII (datafd, & blk [0], datasz_bytes);
        if (nr == -1) // error
          {
            perror ("data read");
            exit (1);
          }
        if (nr == 0) // EOF
          break;
        writeBlk (& blk [0], nr);
      }
    close (datafd);
    writeMark ();
  }

// depth first
//   a/
//   a/b/
//   a/b/c
//   a/b/d
//   a/e/
//   a/e/f

static void process (DIR * dirp, char * mpath, char * fpath)
  {
    struct dirent * entry;

    // loop over looking for directories
    while ((entry = readdir (dirp)))
      {
        if (strcmp (entry -> d_name, ".") == 0)
          continue;
        if (strcmp (entry -> d_name, "..") == 0)
          continue;
        if (entry -> d_type == DT_DIR)
          {
            //printf ("DIR:  %s>%s\n", mpath, entry -> d_name);
            char new_mpath [strlen (mpath) + strlen (entry -> d_name) + 2];
            strcpy (new_mpath, mpath);
            strcat (new_mpath, ">");
            strcat (new_mpath, entry -> d_name);
            char new_fpath [strlen (fpath) + strlen (entry -> d_name) + 2];
            strcpy (new_fpath, fpath);
            strcat (new_fpath, "/");
            strcat (new_fpath, entry -> d_name);
//printf ("new_fpath %s\n", new_fpath);
            DIR * new_dirp = opendir (new_fpath);
            if (! new_dirp)
              {
                perror ("opendir");
                exit (1);
              }
            dumpDir (mpath, fpath, entry -> d_name);
            process (new_dirp, new_mpath, new_fpath);
            closedir (new_dirp);
          }
      }
    // loop over looking for files
    rewinddir (dirp);
    while ((entry = readdir (dirp)))
      {
        if (entry -> d_name [0] == '.')
          continue;
        //if (strcmp (entry -> d_name, ".") == 0)
          //continue;
        //if (strcmp (entry -> d_name, "..") == 0)
          //continue;
        if (entry -> d_type == DT_REG)
          {
//printf ("<%s> %d %lu\n", entry -> d_name, entry -> d_reclen, strlen (entry -> d_name)); 
            dumpASCIIFile (mpath, fpath, entry -> d_name);
          }
      }
  }

int main (int argc, char * argv [])
  {

    if (argc != 3)
      usage ();
    
    DIR * dirp = opendir (argv [1]);
    if (! dirp)
      {
        perror ("opendir");
        exit (1);
      }

    tapefd = open (argv [2], O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (tapefd == -1)
      {
        perror ("tape open");
        exit (1);
      }

    writeLabel ();

    char * root = strdup (argv [1]);
    char * bn = basename (root);
    char rn [strlen (bn) + 1];
    //strcpy (rn, ">");
    strcpy (rn, "");
    strcat (rn, bn);
    dumpDir ("", "", rn);
    process (dirp, rn, argv [1]);
    //process (dirp, "", argv [1]);

    closedir (dirp);

    // trailer

    zeroBlk ();
    setBckupHdr (end, "", "", 0);
    writeBlk (& blk [0], 54 * 36 / 8);

    writeMark ();
  }
