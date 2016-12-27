/*
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

typedef unsigned int uint;

// dcl 1 card aligned,           /* 7punch card declaration */
//     2 w0,                     /* first word */
//      (3 seven bit (3),        /* "111"b */                      //  0  0  3
//       3 cnthi bit (6),        /* high-order word count */       //  0  3  6
//       3 five bit (3),         /* "101"b */                      //  0  9  3
//       3 cntlo bit (6),        /* low-order word count */        //  0 12  6
//       3 tag bit (3),          /* non-zero on last card */       //  0 18  3
//       3 seq bit (15)) unal,   /* card sequence number */        //  0 21 15
//     2 cksm bit (36),          /* checksum */                    //  1  0 36
//     2 data bit (792),         /* data words */                  //  2  0 22 words
//    (2 blank (3) bit (12),     /* blank field */                 // 24  0 36
//     2 id (5) bit (12)) unal;  /* sequence number field */       // 25  0 60

#define MASK36 0777777777777lu
typedef uint64_t word36;

//  getbits36 (data, starting bit, number of bits)

static inline word36 getbits36(word36 x, uint i, uint n) {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)n+1;
    if (shift < 0 || shift > 35) {
        fprintf (stderr, "getbits36: bad args (%012"PRIo64",i=%d,n=%d)\n", x, i, n);
        return 0;
    } else
        return (x >> (unsigned) shift) & ~ (~0U << n);
}

// These are doing end switching?
word36 extr36 (uint8_t * bits, uint woffset)
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
    return (word36) (w & 0777777777777ULL);
  }

// The card image is 80 columns * 12 rows; 960 bits
// which is 26 and 2/3 36-bit words. 
// The IO system adds 12 bits of padding to make 27 36-bit words, for a total 972 bits.
// The punch emulator writes blocks of word36 [27]

#define cardWords36 27
static word36 card [cardWords36];

#define use80
#ifdef use80
// 80 columns * 12 bits / 8 bits/byte

#define readCardBufferSz (80 * 12 / 8)
// Add 2 bytes to allow extr36 to read column 81 when
// doing columns 79, 80, 81.
static uint8_t readCardBuffer [readCardBufferSz + 2];

static int getCard (void)
  {
    ssize_t n = read (STDIN_FILENO, readCardBuffer, readCardBufferSz);
    if (n < 0)
      {
        perror ("read");
        exit (1);
      }
    if (n == 0)
      return 0;
    if (n != readCardBufferSz)
      {
        fprintf (stderr, "read even short %ld %d\n", n, (readCardBufferSz + 1) / 2);
        exit (1);
      }
    for (int i = 0; i < cardWords36; i ++)
      card [i] = extr36 (readCardBuffer, i);
    return 1;
  }


#else
static int readCardCnt = 0;

// cardWords36 (27) * 2: two cards
//             * 4.5; 4.5 bytes/word36
#define readCardBufferSz (cardWords36 * 2 * 9 / 2)

static uint8_t readCardBuffer [readCardBufferSz];

static int getCard (void)
  {
    if ((readCardCnt & 1) == 0) // read even card
      {
        ssize_t n = read (STDIN_FILENO, readCardBuffer, (readCardBufferSz + 1) / 2); 
printf ("read even %d\n", (readCardBufferSz + 1) / 2);
        if (n < 0)
          {
            perror ("read even");
            exit (1);
          }
        if (n == 0)
          return 0;
        if (n != (readCardBufferSz + 1) / 2)
          {
            fprintf (stderr, "read even short %ld %d\n", n, (readCardBufferSz + 1) / 2);
            exit (1);
          }
        for (int i = 0; i < cardWords36; i ++)
          card [i] = extr36 (readCardBuffer, i);
      }
    else
      {
        ssize_t n = read (STDIN_FILENO, readCardBuffer + (readCardBufferSz + 1) / 2, readCardBufferSz / 2);
printf ("read odd %d\n", readCardBufferSz / 2);
        if (n < 0)
          {
            perror ("read odd");
            exit (1);
          }
        if (n == 0)
          return 0;
        if (n != readCardBufferSz / 2)
          {
            fprintf (stderr, "read odd short %ld %d\n", n, readCardBufferSz / 2);
            exit (1);
          }
        for (int i = 0; i < cardWords36; i ++)
          card [i] = extr36 (readCardBuffer, cardWords36 + i);
      }
for (int i = 0; i < readCardBufferSz; i ++) 
{
  if (i % 16 == 0)
    printf ("\n%6d", readCardCnt);
printf (" %03o", readCardBuffer [i]);
}
printf ("\n");
    readCardCnt ++;
    return 1;
  }
#endif

static void displayCard (void)
  {
    for (uint i = 0; i < 27; i ++)
      printf ("  %012"PRIo64"\n", card [i]);
    for (uint row = 0; row < 12; row ++)
      {
        for (uint col = 0; col < 80; col ++)
          {
            // 3 cols/word
            uint wordno = col / 3;
            uint fieldno = col % 3;
            word36 bit = getbits36 (card [wordno], fieldno * 12 + row, 1); 
            if (bit)
              printf ("*");
            else
              printf (" ");
          }
        printf ("\n");
      }
    printf ("\n");

#if 1
    for (uint row = 0; row < 12; row ++)
      {
        for (int col = 79; col >= 0; col --)
          {
            // 3 cols/word
            uint wordno = col / 3;
            uint fieldno = col % 3;
            word36 bit = getbits36 (card [wordno], fieldno * 12 + row, 1); 
            if (bit)
              printf ("*");
            else
              printf (" ");
          }
        printf ("\n");
      }
    printf ("\n");
#endif

    word36 seven = getbits36 (card [0],  0,  3);
    word36 five =  getbits36 (card [0],  9,  3);
    // skip flip cards
    if (seven != 07 || five != 05)
      printf ("not 7punch card");

    word36 tag =   getbits36 (card [0], 18,  3);
    printf ("tag %"PRIo64"\n", tag);

    word36 cnthi = getbits36 (card [0],  3,  6);
    word36 cntlo = getbits36 (card [0], 12,  6);
    word36 cnt = (cnthi << 6) | cntlo;
    printf ("cnt %"PRId64" %"PRIo64"\n", cnt, cnt);

    word36 seq   = getbits36 (card [0], 21, 15);
    printf ("seq %"PRIo64"\n", seq);

  }

int main (int argc, char * argv [])
  {
    while (getCard ())
      displayCard ();
    return 0;
  }
