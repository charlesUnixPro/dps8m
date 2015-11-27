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
#include <stdbool.h>

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

static int total_blocks_written;
static word18 recno = 0;
static word18 filno = 0;

static void writeMark (void)
  {
    const int32_t zero = 0;
    write (tapefd, & zero, sizeof (zero));
  }

static void writeBlk (void)
  {
    const int32_t blksiz = blksz_bytes;
    write (tapefd, & blksiz, sizeof (blksiz));
    write (tapefd, & blk [0], sizeof (blk));
    write (tapefd, & blksiz, sizeof (blksiz));
    total_blocks_written ++;
    recno ++;
    if (total_blocks_written % 128 == 1)
      {
        writeMark ();
        recno = 1;
        filno ++;
      }
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
     
static word72 uid0 = 0000001506575ll;
static word72 uid1 = 0750560343164ll;
static word72 cumulative = 0;

static void setHdr (word18 ndatabits, 
                    word1 label, word1 eor, word1 padding)
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
    cumulative += ndatabits;
    put36 (cumulative,     & blk [0], 3 + 8 + 1024);
    put36 (0777777777777l, & blk [0], 4 + 8 + 1024);
    put36 (filno,          & blk [0], 5 + 8 + 1024);
    put36 (recno,          & blk [0], 6 + 8 + 1024);
    put36 (0265221631704l, & blk [0], 7 + 8 + 1024);

    word36 a = 0;
    word1 carry = 0;

    for (int i = 0; i < 8; i ++)
      if (i != 6)
        cksum (& a, & carry, extr36 (& blk [0], i), true);
    for (int i = 0; i < 8; i ++)
      cksum (& a, & carry, extr36 (& blk [0], i + 8 + 1024), true);
    cksum (& a, & carry, 0, false);
    put36 (a,              & blk [0], 6);
    
    uid1 += 4;
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




// dcl 1 br (1000) based aligned,   /* branch array returned by list_dir */
//     2 (vtoc_error bit (1),       /* Vtoc error on this entry */
//      pad1 bit (1), uid bit (70),
//      pad2 bit (20), dtu bit (52),
//      pad3 bit (20), dtm bit (52),
//      pad4 bit (20), dtd bit (52),
//      pad5 bit (20), dtbm bit (52),
//      access_class bit (72),
//      dirsw bit (1), optionsw bit (2), bc bit (24), consistsw bit (2), mode bit (5), usage bit (2),
//      usagect bit (17), nomore bit (1), (cl, ml) bit (9),
//      acct bit (36),
//     (hlim, llim) bit (17),
//      multiple_class bit (1), pad7 bit (1),
//     (rb1, rb2, rb3) bit (6), pad8 bit (18),
//     (pad9, namerp) bit (18),
//      ix bit (18), dump_me bit (1), nnames bit (17)) unaligned;  /* ix is pointer to i'th (sorted) entry. */





// Pointers in backup_preamble_header are relative to the start of the header
// (dlen), so the offset in the block is 8 (mst header) + 32 (backup header)

#define BPH_OFFSET 40

static void setBckupHdr (char * mpath, char * name,
                         word36 bitcnt)
  {

    word36 segcnt = (bitcnt + 35) / 36;
    putStr (8 + 00000, " z z z z z z z z z z z z z z z zThis is the beginning of a backup logical record.        z z z z z z z z z z z z z z z z", 30);
    put36 (1024 - 32,      & blk [0], 8 + 00036); // hdrcnt
    put36 (segcnt,         & blk [0], 8 + 00037); // segcnt
    put36 (strlen (mpath), & blk [0], 8 + 00040);  // dlen
    putStr (8 + 00041, mpath, 42);                // dname
    put36 (strlen (name),  & blk [0], 8 + 00113); // elen
    putStr (8 + 00114, name, 8);                  // ename
    put36 (bitcnt,         & blk [0], 8 + 00124); // bitcnt
    put36 (19,             & blk [0], 8 + 00125); // record type sec_seg
    put36 (0,              & blk [0], 8 + 00126); // dtd
    put36 (0,              & blk [0], 8 + 00127); // dtd
    putStr (8 + 00130, "mdump", 8);               // dump procedure id
    put36 (0000232000000,  & blk [0], 8 + 00140); // bp  branch ptr
    put36 (1,              & blk [0], 8 + 00141); // bc  branch cnt
    put36 (0,              & blk [0], 8 + 00142); // lp  link ptr
    put36 (0,              & blk [0], 8 + 00143); // lc  link cnt
    put36 (0,              & blk [0], 8 + 00144); // aclp  acl ptr
    put36 (0,              & blk [0], 8 + 00145); // aclc  acl cnt
    put36 (0,              & blk [0], 8 + 00146); // actind  file activity indicator
    put36 (0,              & blk [0], 8 + 00147); // actime  file activity time

    put36 (0,              & blk [0], 8 + 00150); // unknown  "Clayton."
    put36 (0,              & blk [0], 8 + 00151); // unknown
    put36 (0,              & blk [0], 8 + 00152); // unknown  "SysAdmin"
    put36 (0,              & blk [0], 8 + 00153); // unknown
    put36 (0,              & blk [0], 8 + 00154); // unknown  ".a      "
    put36 (0,              & blk [0], 8 + 00155); // unknown
    put36 (0,              & blk [0], 8 + 00156); // unknown  "        "
    put36 (0,              & blk [0], 8 + 00157); // unknown

    put36 (0,              & blk [0], 8 + 00160); // unknown  000000776000
    put36 (0,              & blk [0], 8 + 00161); // unknown
    put36 (0,              & blk [0], 8 + 00162); // unknown
    put36 (0,              & blk [0], 8 + 00163); // unknown
    put36 (0,              & blk [0], 8 + 00164); // unknown
    put36 (0,              & blk [0], 8 + 00165); // unknown  000000000002
    put36 (0,              & blk [0], 8 + 00166); // unknown  000274000000
    put36 (0,              & blk [0], 8 + 00167); // unknown

    put36 (0,              & blk [0], 8 + 00170); // unknown
    put36 (0,              & blk [0], 8 + 00171); // unknown
    put36 (0,              & blk [0], 8 + 00172); // unknown
    put36 (0,              & blk [0], 8 + 00173); // unknown
    put36 (0,              & blk [0], 8 + 00174); // unknown
    put36 (0,              & blk [0], 8 + 00175); // unknown
    put36 (0,              & blk [0], 8 + 00176); // unknown
    put36 (0,              & blk [0], 8 + 00177); // unknown

    put36 (0,              & blk [0], 8 + 00200); // unknown
    put36 (0,              & blk [0], 8 + 00201); // unknown
    put36 (0,              & blk [0], 8 + 00202); // unknown
    put36 (0,              & blk [0], 8 + 00203); // unknown
    put36 (0,              & blk [0], 8 + 00204); // unknown
    put36 (0,              & blk [0], 8 + 00205); // unknown
    put36 (0,              & blk [0], 8 + 00206); // unknown
    put36 (0,              & blk [0], 8 + 00207); // unknown

    put36 (0,              & blk [0], 8 + 00210); // unknown
    put36 (0,              & blk [0], 8 + 00211); // unknown
    put36 (0,              & blk [0], 8 + 00212); // unknown
    put36 (0,              & blk [0], 8 + 00213); // unknown
    put36 (0,              & blk [0], 8 + 00214); // unknown
    put36 (0,              & blk [0], 8 + 00215); // unknown
    put36 (0,              & blk [0], 8 + 00216); // unknown
    put36 (0,              & blk [0], 8 + 00217); // unknown

    put36 (0,              & blk [0], 8 + 00220); // unknown
    put36 (0,              & blk [0], 8 + 00221); // unknown
    put36 (0,              & blk [0], 8 + 00222); // unknown
    put36 (0,              & blk [0], 8 + 00223); // unknown
    put36 (0,              & blk [0], 8 + 00224); // unknown
    put36 (0,              & blk [0], 8 + 00225); // unknown
    put36 (0,              & blk [0], 8 + 00226); // unknown
    put36 (0,              & blk [0], 8 + 00227); // unknown

    put36 (0,              & blk [0], 8 + 00230); // unknown "Clayton."
    put36 (0,              & blk [0], 8 + 00231); // unknown
    put36 (0,              & blk [0], 8 + 00232); // unknown "SysAdmin"
    put36 (0,              & blk [0], 8 + 00233); // unknown
    put36 (0,              & blk [0], 8 + 00234); // unknown ".a      "
    put36 (0,              & blk [0], 8 + 00235); // unknown
    put36 (0,              & blk [0], 8 + 00236); // unknown "        "
    put36 (0,              & blk [0], 8 + 00237); // unknown

    put36 (0,              & blk [0], 8 + 00240); // unknown 000000000001
    put36 (0,              & blk [0], 8 + 00241); // unknown 775600000000
    put36 (0,              & blk [0], 8 + 00242); // unknown 000160000000
    put36 (0,              & blk [0], 8 + 00243); // unknown
    put36 (0,              & blk [0], 8 + 00244); // unknown
    put36 (0,              & blk [0], 8 + 00245); // unknown 000010000000
    put36 (0,              & blk [0], 8 + 00246); // unknown 000150000000
    put36 (0,              & blk [0], 8 + 00247); // unknown

    put36 (0,              & blk [0], 8 + 00250); // unknown
    put36 (0,              & blk [0], 8 + 00251); // unknown
    put36 (0,              & blk [0], 8 + 00252); // unknown
    put36 (0,              & blk [0], 8 + 00253); // unknown
    put36 (0,              & blk [0], 8 + 00254); // unknown
    put36 (0,              & blk [0], 8 + 00255); // unknown
    put36 (0,              & blk [0], 8 + 00256); // unknown
    put36 (0,              & blk [0], 8 + 00257); // unknown

    put36 (0,              & blk [0], 8 + 00260); // unknown
    put36 (0,              & blk [0], 8 + 00261); // unknown
    put36 (0,              & blk [0], 8 + 00262); // unknown
    put36 (0,              & blk [0], 8 + 00263); // unknown
    put36 (0,              & blk [0], 8 + 00264); // unknown
    put36 (0,              & blk [0], 8 + 00265); // unknown 000000000005
    put36 (0,              & blk [0], 8 + 00266); // unknown 
    put36 (0,              & blk [0], 8 + 00267); // unknown 000005000000

    put36 (0,              & blk [0], 8 + 00270); // unknown 000002000026
    put36 (0,              & blk [0], 8 + 00271); // unknown 001000777750

    // br (1); bp points here

    put36 (0,              & blk [0], 8 + 00272); // vtoc_error, pad, uid
    put36 (0,              & blk [0], 8 + 00273); // uid cont.
    put36 (0,              & blk [0], 8 + 00274); // pad2, dtu
    put36 (0,              & blk [0], 8 + 00275); // dtu cont.
    put36 (0,              & blk [0], 8 + 00276); // pad3, dtm
    put36 (0,              & blk [0], 8 + 00277); // dtm cont.
    put36 (0,              & blk [0], 8 + 00300); // pad3, dtd
    put36 (0,              & blk [0], 8 + 00301); // dtd cont.
    put36 (0,              & blk [0], 8 + 00302); // pad3, dtbm
    put36 (0,              & blk [0], 8 + 00303); // dtbm cont.
    put36 (0,              & blk [0], 8 + 00304); // access class
    put36 (0,              & blk [0], 8 + 00305); // access class cont.
    // irsw bit (1), optionsw bit (2), bc bit (24), consistsw bit (2), 
    // mode bit (5), usage bit (2)
    put36 ((bitcnt << 9) | 050, & blk [0], 8 + 00306);
    put36 (0000000001377,  & blk [0], 8 + 00307); // usagect bit (17), nomore bit (1), (cl, ml) bit (9)
    put36 (0,              & blk [0], 8 + 00310); // acct
    put36 (0,              & blk [0], 8 + 00311); // (hlim, llim) bit (17), multiple_class bit (1), pad7 bit (1)
    put36 (0040404000000,  & blk [0], 8 + 00312); // (rb1, rb2, rb3) bit (6), pad8 bit (18)
    put36 (0000000000260,  & blk [0], 8 + 00313); // (pad9, namerp) bit (18) namerp points to 8+314
// XXX what is ix?
    put36 (0000323400001,  & blk [0], 8 + 00314); // x bit (18), dump_me bit (1), nnames bit (17)



    put36 (0,              & blk [0], 8 + 00315); // unknown
    put36 (0,              & blk [0], 8 + 00316); // unknown 000026000014
    put36 (0,              & blk [0], 8 + 00317); // unknown 001000777722
    
// relp 260, br name, pointed to by namerp
    put36 (0000100000000,  & blk [0], 8 + 00320); // size bit (17) [32 here]
    putStr (8 + 00321, name, 8); //  string character (32);



    put36 (0,              & blk [0], 8 + 00331); // unknown
    put36 (0,              & blk [0], 8 + 00332); // unknown 000014000026
    put36 (0,              & blk [0], 8 + 00333); // unknown 001000777706
    put36 (0,              & blk [0], 8 + 00334); // unknown 000000000001
    put36 (0,              & blk [0], 8 + 00335); // unknown 162157157164 "root"
    put36 (0,              & blk [0], 8 + 00336); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00337); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00340); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00341); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00342); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00343); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00344); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00345); // unknown 162157157164 "root"
    put36 (0,              & blk [0], 8 + 00346); // unknown 063040040040 "3   "
    put36 (0,              & blk [0], 8 + 00347); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00350); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00351); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00352); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00353); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00354); // unknown 040040040040
    put36 (0,              & blk [0], 8 + 00355); // unknown 552342127741
    put36 (0,              & blk [0], 8 + 00356); // unknown 265304163531
    put36 (0,              & blk [0], 8 + 00357); // unknown 000000000000
    put36 (0,              & blk [0], 8 + 00360); // unknown 000026000030
    put36 (0,              & blk [0], 8 + 00361); // unknown 001000777660

    // relp 0372 aclp
    putStr (8 + 00362, "Clayton.SysAdmin.*              ", 8);
    put36 (0500000000000,  & blk [0], 8 + 00372); 
    put36 (0,              & blk [0], 8 + 00373); // unknown 0 AIM?
    put36 (0,              & blk [0], 8 + 00374); // unknown 0 AIM?

    // 2nd ACL, not used here
    
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
    setHdr (0, 0, 0, 0);
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
    setBckupHdr (mpath, name, bits);
    setHdr (0110000, 0, 0, 0);
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
        if (nr != datasz_bytes)
          {
printf ("nr %lu\n", nr);
            // round up to 256 word
            // 256 * 36 / 8 = 1152
            nr += 1152;
            size_t modulus = nr % 1152;
            nr -= modulus;
printf ("nr %lu\n", nr);
          }
        setHdr (nr * 8, 0, 0, 0);
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
        if (entry -> d_name [0] == '.')
          continue;
        //if (strcmp (entry -> d_name, ".") == 0)
          //continue;
        //if (strcmp (entry -> d_name, "..") == 0)
          //continue;
        if (entry -> d_type == DT_REG)
          {
//printf ("<%s> %d %lu\n", entry -> d_name, entry -> d_reclen, strlen (entry -> d_name)); 
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

    tapefd = open (argv [2], O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (tapefd == -1)
      {
        perror ("tape open");
        exit (1);
      }

    // write label
    zeroBlk ();
    setLabel ();
    setHdr (0001540, 1, 0, 1);
    writeBlk ();
    // write mark
    //writeMark (); handled in write block


    char * root = strdup (argv [1]);
    char * bn = basename (root);
    char rn [strlen (bn) + 1];
    strcpy (rn, ">");
    strcat (rn, bn);
    process (dirp, rn, argv [1]);
    //process (dirp, "", argv [1]);

    closedir (dirp);

    writeMark ();

    zeroBlk ();
    setHdr (0, 0, 1, 1);
    writeBlk ();

    writeMark ();
    writeMark ();
  }
