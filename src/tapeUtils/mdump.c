// Make a UNIX tree into a Multics dump tape

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>

// Each tape block 
//
//    8 word header
// 1024 word data
//    8 word trailer
//
// 1040 word36
//  520 word72

#define datasz_bytes (1024 * 36 / 8)

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
 

#define blksz_w72 520
#define blksz_bytes (blksz_w72 * 72 / 8)

//static word72 blk [blksz_w72];
static uint8_t blk [blksz_bytes];

static void writeBlk (void)
  {
    const int32_t blksiz = blksz_bytes;
    write (tapefd, & blksiz, sizeof (blksiz));
    write (tapefd, & blk [0], sizeof (blk));
    write (tapefd, & blksiz, sizeof (blksiz));
  }

static void writeMark (void)
  {
    const int32_t zero = 0;
    write (tapefd, & zero, sizeof (zero));
  }

static void zeroBlk (void)
  {
    memset (blk, -1, sizeof (blk));
  }

static void setLabel (void)
  {
    // installation id
    put36 (0124145163164l, & blk [0],  8); // "Test"
    put36 (0040040040040l, & blk [0],  9); // "    "
    put36 (0040040040040l, & blk [0], 10); // "    "
    put36 (0040040040040l, & blk [0], 11); // "    "
    put36 (0040040040040l, & blk [0], 12); // "    "
    put36 (0040040040040l, & blk [0], 13); // "    "
    put36 (0040040040040l, & blk [0], 14); // "    "
    put36 (0040040040040l, & blk [0], 15); // "    "
    // tape reel id
    put36 (0124145163164l, & blk [0], 16); // "Test"
    put36 (0040040040040l, & blk [0], 17); // "    "
    put36 (0040040040040l, & blk [0], 18); // "    "
    put36 (0040040040040l, & blk [0], 19); // "    "
    put36 (0040040040040l, & blk [0], 20); // "    "
    put36 (0040040040040l, & blk [0], 21); // "    "
    put36 (0040040040040l, & blk [0], 22); // "    "
    put36 (0040040040040l, & blk [0], 23); // "    "
    // volume set id
    put36 (0124145163164l, & blk [0], 24); // "Test"
    put36 (0040040040040l, & blk [0], 25); // "    "
    put36 (0040040040040l, & blk [0], 26); // "    "
    put36 (0040040040040l, & blk [0], 27); // "    "
    put36 (0040040040040l, & blk [0], 28); // "    "
    put36 (0040040040040l, & blk [0], 29); // "    "
    put36 (0040040040040l, & blk [0], 30); // "    "
    put36 (0040040040040l, & blk [0], 31); // "    "
  }

static void cksum (word36 * a, word1 * carry, word36 val)
  {
    //word36 val = extr36 (& blk [0], idx);
printf ("in %12lo %o %12lo\n", * a, * carry, val);
    (* a) += val + * carry;
    (* carry) = ((* a) & BIT37) ? 1 : 0;
    (* a) &= MASK36;
    (* a) <<= 1;
    (* a) |= ((* a) & BIT37) ? 1 : 0;
    (* a) &= MASK36;
printf ("out %12lo %o\n", * a, * carry);
  }
     
static word18 recno = 0;
static word18 filno = 0;
static word72 uid0 = 0000001506575ll;
static word72 uid1 = 0750560343164ll;


static void setHdr (word18 ndatabits, 
                    word1 label, word1 eor, word1 padding, 
                    word36 cumulative)
  {
    word36 w3 = ((word36) recno) << 18 | filno;
    word36 w4 = ((word36) ndatabits) << 18 | 0110000;
    word1 admin = label | eor;
    word1 admin2 = padding;
    word36 w5 = ((word36) admin)   << (35 -  0) |
                ((word36) label)   << (35 -  1) |
                ((word36) eor)     << (35 -  2) |
                ((word36) admin2)  << (35 - 14) |
                ((word36) padding) << (35 - 16) |
                ((word36) 1)       << (35 - 26); // version
    put36 (0670314355245l, & blk [0], 0);
    put36 (uid0,           & blk [0], 1);
    put36 (uid1,           & blk [0], 2);
    put36 (w3,             & blk [0], 3);
    put36 (w4,             & blk [0], 4);
    put36 (w5,             & blk [0], 5);
    put36 (0,              & blk [0], 6);
    put36 (0512556146073l, & blk [0], 7);

    put36 (0107463422532l, & blk [0], 0 + 8 + 1024);
    put36 (uid0,           & blk [0], 1 + 8 + 1024);
    put36 (uid1,           & blk [0], 2 + 8 + 1024);
    put36 (cumulative,     & blk [0], 3 + 8 + 1024);
    put36 (0777777777777l, & blk [0], 4 + 8 + 1024);
    put36 (filno,          & blk [0], 5 + 8 + 1024);
    put36 (recno,          & blk [0], 6 + 8 + 1024);
    put36 (0265221631704l, & blk [0], 7 + 8 + 1024);

    word36 a = 0;
    word1 carry = 0;

    for (int i = 0; i < 8; i ++)
      if (i != 6)
        cksum (& a, & carry, extr36 (& blk [0], i));
    for (int i = 0; i < 8; i ++)
      cksum (& a, & carry, extr36 (& blk [0], i + 8 + 1024));
    cksum (& a, & carry, 0);
    put36 (a,              & blk [0], 6);
    
    uid1 += 4;
    recno ++;
  }

static void usage (void)
  {
    printf ("mdump root tapename\n");
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

// dcl 1 header  static,     /* Backup logical record header */
//       2 zz1  character (32) initial (" z z z z z z z z z z z z z z z z"),
//       2 english character (56)
//          initial ("This is the beginning of a backup logical record."),
//       2 zz2  character (32) initial (" z z z z z z z z z z z z z z z z"),
//       2 hdrcnt  fixed binary,
//       2 segcnt  fixed binary;

// G.Dixon: NDC seg and NDC dir not used.
// dcl  ndc_segment fixed binary int static options(constant) initial (1);
//   /* Record of segment with NDC attributes */
// dcl  ndc_directory fixed binary int static options(constant) initial (2);
//   /* Record of directory with NDC attributes */
// dcl  ndc_directory_list fixed binary int static options(constant) initial (3);
//   /* Record of directory list with initial ACL */
// dcl  sec_seg fixed binary int static options(constant) initial(19);
//   /* Seg with security & call_limiter */
// dcl  sec_dir fixed binary int static options(constant) initial(20);
//   /* Dir with security & call_limiter */
// 

static void setBckupHdr (word36 hdrcnt, word36 segcnt, 
                         char * mpath, char * name,
                         word36 bitcnt)
  {

    put36 (0040172040172l, & blk [0],   8); // " z z"
    put36 (0040172040172l, & blk [0],   9); // " z z"
    put36 (0040172040172l, & blk [0],  10); // " z z"
    put36 (0040172040172l, & blk [0],  11); // " z z"
    put36 (0040172040172l, & blk [0],  12); // " z z"
    put36 (0040172040172l, & blk [0],  13); // " z z"
    put36 (0040172040172l, & blk [0],  14); // " z z"
    put36 (0040172040172l, & blk [0],  15); // " z z"
    put36 (0124150151163l, & blk [0],  16); // "This"
    put36 (0040151163040l, & blk [0],  17); // " is "
    put36 (0164150145040l, & blk [0],  18); // "the "
    put36 (0142145147151l, & blk [0],  19); // "begi"
    put36 (0156156151156l, & blk [0],  20); // "nnin"
    put36 (0147040157146l, & blk [0],  21); // "g of"
    put36 (0040141040142l, & blk [0],  22); // " a b"
    put36 (0141143153165l, & blk [0],  23); // "acku"
    put36 (0160040154157l, & blk [0],  24); // "p lo"
    put36 (0147151143141l, & blk [0],  25); // "gica"
    put36 (0154040162145l, & blk [0],  26); // "l re"
    put36 (0143157162144l, & blk [0],  27); // "cord"
    put36 (0056040040040l, & blk [0],  28); // ".   "
    put36 (0040040040040l, & blk [0],  29); // "    "
    put36 (0040172040172l, & blk [0],  30); // " z z"
    put36 (0040172040172l, & blk [0],  31); // " z z"
    put36 (0040172040172l, & blk [0],  32); // " z z"
    put36 (0040172040172l, & blk [0],  33); // " z z"
    put36 (0040172040172l, & blk [0],  34); // " z z"
    put36 (0040172040172l, & blk [0],  35); // " z z"
    put36 (0040172040172l, & blk [0],  36); // " z z"
    put36 (0040172040172l, & blk [0],  37); // " z z"
    put36 (hdrcnt,         & blk [0],  38);
    put36 (segcnt,         & blk [0],  39);
    put36 (strlen (mpath), & blk [0],  40); // dlen
    putStr (41, mpath, 42);                 // dname
    put36 (strlen (name),  & blk [0],  83); // elen
    putStr (84, name, 8);                   // ename
    put36 (bitcnt,         & blk [0],  92);
    put36 (19,             & blk [0],  93); // record type sec_seg
    putStr (94, "mdump", 8);                // dump procedure id
    put36 (0,              & blk [0], 102); // bp  branch ptr
    put36 (0,              & blk [0], 103); // bc  branch cnt
    put36 (0,              & blk [0], 104); // lp  link ptr
    put36 (0,              & blk [0], 105); // lc  link cnt
    put36 (0,              & blk [0], 106); // aclp  acl ptr
    put36 (0,              & blk [0], 107); // aclc  acl cnt
    put36 (0,              & blk [0], 108); // actind  file activity indicator
    put36 (0,              & blk [0], 109); // actime  file activity time

  }

static void dumpDir (char * mpath, char * fpath, char * name)
  {
#if 0
    printf ("DIR:  %s>%s\n", mpath, name);
    char fname [strlen (fpath) + strlen (name) + 2];
    strcpy (fname, fpath);
    strcat (fname, "/");
    strcat (fname, name);
//printf ("%s\n", fname);
    zeroBlk ();
    setBckupHdr (0, 0);
    setHdr (0, 0, 0, 0, 0);
    writeBlk ();
#endif
  }

static void dumpFile (char * mpath, char * fpath, char * name)
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

// Hopefully, the segment is a multiple of 36 bits; that is to say 4 1/2 bytes.

    off_t sz = buf . st_size;
    uint64_t bits = sz * 8;

    uint64_t remainder = bits % 36;
    // if the segment is an even number of words, remainder will be 0; odd
    // will be 4 due to UNIX padding the last word to a byte boundary.
    if (remainder != 0 && remainder != 4)
      printf ("Remainder error %s %ld %lu\n", fullname, buf . st_size, remainder);
    bits -= remainder;


    // backup header

    zeroBlk ();
    setBckupHdr (0, 0, mpath, name, bits);
    setHdr (0, 0, 0, 0, 0);
    writeBlk ();

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
        ssize_t nr = read (datafd, & blk [8], datasz_bytes);
        if (nr == -1) // error
          {
            perror ("data read");
            exit (1);
          }
        if (nr == 0) // EOF
          break;
        setHdr (0, 0, 0, 0, 0);
        writeBlk ();
      }
    close (datafd);
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
        if (strcmp (entry -> d_name, ".") == 0)
          continue;
        if (strcmp (entry -> d_name, "..") == 0)
          continue;
        if (entry -> d_type == DT_REG)
          {
            dumpFile (mpath, fpath, entry -> d_name);
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

    tapefd = open (argv [2], O_RDWR | O_CREAT, 0644);
    if (tapefd == -1)
      {
        perror ("tape open");
        exit (1);
      }

    // write label
    zeroBlk ();
    setLabel ();
    setHdr (0001540, 1, 0, 1, 0001540);
    writeBlk ();
    // write mark
    writeMark ();

    char * root = strdup (argv [1]);
    char * bn = basename (root);
    char rn [strlen (bn) + 1];
    strcpy (rn, ">");
    strcat (rn, bn);
    process (dirp, rn, argv [1]);
    //process (dirp, "", argv [1]);

    closedir (dirp);

  }
