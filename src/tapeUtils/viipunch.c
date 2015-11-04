// Convert p72 to/from 7 punch
//
//  viipunch -f    convert word36 7punch stdin to word36 stdout XXX broken; needs to be p72
//  viipunch -t    convert p72 stdin to p72 7punch80 stdout 

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint64_t word36;

#define MASKBITS(x)     ( ~(~((uint64_t)0)<<x) )   // lower (x) bits all ones

//
//  getbit
//     Get a single bit. offset can be bigger when word size
//

static uint8_t getbit (void * bits, int offset)
  {
    int offsetInWord = offset % 36;
    int revOffsetInWord = 35 - offsetInWord;
    int offsetToStartOfWord = offset - offsetInWord;
    int revOffset = offsetToStartOfWord + revOffsetInWord;

    uint8_t * p = (uint8_t *) bits;
    unsigned int byte_offset = revOffset / 8;
    unsigned int bit_offset = revOffset % 8;
    // flip the byte back
    bit_offset = 7 - bit_offset;

    uint8_t byte = p [byte_offset];
    byte >>= bit_offset;
    byte &= 1;
    //printf ("offset %d, byte_offset %d, bit_offset %d, byte %x, bit %x\n", offset, byte_offset, bit_offset, p [byte_offset], byte);
    return byte;
  }

//
// extr
//    Get a string of bits (up to 64)
//

static uint64_t extr (void * bits, int offset, int nbits)
  {
    uint64_t n = 0;
    int i;
    for (i = nbits - 1; i >= 0; i --)
      {
        n <<= 1;
        n |= getbit (bits, i + offset);
        //printf ("%012lo\n", n);
      }
    return n;
  }

//  getbits36 (data, starting bit, number of bits)

static inline word36 getbits36(word36 x, uint i, uint n) {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)n+1;
    if (shift < 0 || shift > 35) {
        fprintf (stderr, "getbits36: bad args (%012lo,i=%d,n=%d)\n", x, i, n);
        return 0;
    } else
        return (x >> (unsigned) shift) & ~ (~0U << n);
}

static inline word36 setbits36(word36 x, uint p, uint n, word36 val)
{
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        fprintf (stderr, "setbits36: bad args (%012lo,pos=%d,n=%d)\n", x, p, n);
        return 0;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    word36 result = (x & ~ mask) | ((val&MASKBITS(n)) << (36 - p - n));
    return result;
}

static inline void putbits36 (word36 * x, uint p, uint n, word36 val)
  {
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35)
      {
        fprintf (stderr, "putbits36: bad args (%012lo,pos=%d,n=%d)\n", * x, p, n);
        return;
      }
    word36 mask = ~ (~0U << n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    * x = (* x & ~mask) | ((val & MASKBITS (n)) << (36 - p - n));
    return;
  }
    

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

void put36 (word36 val, uint8_t * bits, uint woffset)
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
      }
    else
      {
        p [0]  = (val >> 28) & 0xff;
        p [1]  = (val >> 20) & 0xff;
        p [2]  = (val >> 12) & 0xff;
        p [3]  = (val >>  4) & 0xff;
        p [4] &=               0x0f;
        p [4] |= (val <<  4) & 0xf0;
      }
  }

// The card image is 80 columns * 12 rows; 960 bits
// which is 26 and 2/3 36-bit words. 
// The IO system adds 12 bits of padding to make 27 36-bit words, for a total 972 bits.
// The punch emulator writes blocks of word36 [27]

#define cardWords36 27
static word36 card [cardWords36];

static int getCardImage (void)
  {
    memset (card, 0, sizeof (card));
    ssize_t n = read (STDIN_FILENO, card, sizeof (card));
    if (n < 0)
      {
        perror ("read");
        exit (1);
      }
    if (n == 0)
      return 0;
    if (n != sizeof (card))
      fprintf (stderr, "short read %ld\n", n);
//fprintf (stderr, "%012lo\n", card [0]);
    return 1;
  }

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


static void from (void)
  {
    word36 seqno = 0;
    while (getCardImage ())
      {
        word36 seven = getbits36 (card [0],  0,  3);       
        word36 five =  getbits36 (card [0],  9,  3);       
        // skip flip cards
        if (seven != 07 || five != 05)
          continue;

        word36 tag =   getbits36 (card [0], 18,  3);       
        //fprintf (stderr, "%lo %lo\n", seven, five);
        //fprintf (stderr, "%lo\n", tag);

        word36 cnthi = getbits36 (card [0],  3,  6);       
        word36 cntlo = getbits36 (card [0], 12,  6);       
        word36 cnt = (cnthi << 6) | cntlo;
        //fprintf (stderr, "%lo\n", cnt);

        word36 seq   = getbits36 (card [0], 21, 15);       
        //fprintf (stderr, "%lo\n", seq);
        if (seq != seqno)
          {
            fprintf (stderr, "out of seq; found %ld, expected %ld\n", seq, seqno);
            exit (1);
          }
        seqno ++;

#define MASK36 0777777777777lu
#define BIT37 01000000000000lu

        word36 cksm = card [1];
        if (cksm)
          {
             word36 check = card [0];
             word36 carry = 0;
             for (int i = 2; i <= 23; i ++)
               {
                 check += card [i] + carry;
                 if (check & BIT37)
                   {
                     check &= MASK36;
                     carry = 1;
                   }
                 else
                   carry = 0;
               }
             check += carry;
             check &= MASK36;
             if (check != cksm)
               fprintf (stderr, "checksum %012lo, expected %012lo\n", check, cksm);
          }
        if (tag) // last card
          {
            word36 bitcnt = card [2];
          }
        else // data card
          {
            if (cnt < 027)
              {
                for (unsigned int i = 0; i < cnt; i ++)
                   write (STDOUT_FILENO, & card [2 + i], sizeof (word36));
              }
            else
              {
                for (unsigned int i = 0; i < cnt; i ++)
                   write (STDOUT_FILENO, & card [2], sizeof (word36));
              }
          }
        if (tag)
          break;
      }
  }


static int inputWordCnt = 0;

// 1: ok
// 0: eof
static int getWord36 (word36 * wp)
  {
    static uint8_t b9 [9];
    if ((inputWordCnt & 1) == 0) // even word?
      {
        memset (b9, 0, sizeof (b9));
        // Even word; get first 5 bytes (word36 plus 4 bits of the next word
        ssize_t n = read (STDIN_FILENO, b9, 5);
        if (n < 0)
          {
            perror ("read");
            exit (1);
          }
        if (n == 0)
          return 0;
        if (n != 5)
          {
            fprintf (stderr, "short read of 5 got %ld\n", n);
            //exit (1);
          }
//printf ("b9e %6d %03o %03o %03o %03o %03o %03o %03o %03o %03o\n", inputWordCnt, b9 [0], b9 [1], b9 [2], b9 [3], b9 [4], b9 [5], b9 [6], b9 [7], b9 [8]);
        * wp = extr36 (b9, 0);
     }
   else
     {
       // Odd word; get the remaining 32 bits
        ssize_t n = read (STDIN_FILENO, b9 + 5, 4);
        if (n < 0)
          {
            perror ("read2");
            exit (1);
          }
        if (n == 0)
          return 0;
        if (n != 4)
          {
            fprintf (stderr, "short read of 4 got %ld\n", n);
            //exit (1);
          }
//printf ("b9o %6d %03o %03o %03o %03o %03o %03o %03o %03o %03o\n", inputWordCnt, b9 [0], b9 [1], b9 [2], b9 [3], b9 [4], b9 [5], b9 [6], b9 [7], b9 [8]);
        * wp = extr36 (b9, 1);
      }
    inputWordCnt ++;
    return 1;
  }


// Only write 80 columns
#define use80

#ifdef use80
// 80 columns * 12 bits / 8 bits/byte

#define writeCardBufferSz (80 * 12 / 8)
// Add 2 bytes to allow put36 to write column 81 when
// doing columns 79, 80, 81.
static uint8_t writeCardBuffer [writeCardBufferSz + 2];

static void writeCard (void)
  {
    for (int i = 0; i < cardWords36; i ++)
      put36 (card [i], & writeCardBuffer [0], i);
    write (STDOUT_FILENO, writeCardBuffer, writeCardBufferSz);
  }

#else
static int writeCardCnt = 0;
// cardWords36 (27) * 2: two cards
//             * 4.5; 4.5 bytes/word36
#define writeCardBufferSz (cardWords36 * 2 * 9 / 2)

static uint8_t writeCardBuffer [writeCardBufferSz];

static void flushCard (void)
  {
    if ((writeCardCnt & 1) == 1) // even word? write the entire buffer
      {
//fprintf (stderr, "full write\n");
        write (STDOUT_FILENO, writeCardBuffer, writeCardBufferSz);
      }
    else // write the first half of the buffer, rounding up.
      {
        write (STDOUT_FILENO, writeCardBuffer, (writeCardBufferSz + 1) / 2);
//fprintf (stderr, "half write\n");
      }
  }

static void writeCard (void)
  {
    if ((writeCardCnt & 1) == 0) // even word?
      {
        for (int i = 0; i < cardWords36; i ++)
          put36 (card [i], & writeCardBuffer [0], i);
      }
    else
      {
        for (int i = 0; i < cardWords36; i ++)
          put36 (card [i], & writeCardBuffer [0], cardWords36 + i);
        flushCard ();
      }
    writeCardCnt ++;
  }
#endif
// Convert packed72 to 7punch
static void to (void)
  {
    unsigned int cardSeqNo = 0;
    while (1)
      {
        // 22 data words/card
#define wperc 22
        int nw;
        word36 buffer [wperc];
        for (nw = 0; nw < wperc; nw ++)
          {
            if (getWord36 (buffer + nw) == 0)
              break;
          }
#if 0
        printf ("%d\n", nw);
        for (int i = 0; i < nw; i ++)
          {
            printf ("  %3d %012lo\n", i, buffer [i]);
          }
#endif
        memset (card, 0, sizeof (card));
        if (nw)
          {
            putbits36 (& card [0],  0,  3,              07);  // seven
            putbits36 (& card [0],  3,  6, (nw >> 6) & 077);  // cnthi
            putbits36 (& card [0],  9,  3,              05);  // five
            putbits36 (& card [0], 12,  6, (nw >> 0) & 077);  // cntlo
            putbits36 (& card [0], 18,  3,               0);  // tag
            putbits36 (& card [0], 21, 15,       cardSeqNo);  // seq
fprintf (stderr, "seq %d nw %d %012lo\n", cardSeqNo, nw, card [0]);
            for (int i = 0; i < nw; i ++)
              {
                card [2 + i] = buffer [i]; // data
              }
          }
        else // nw = 0; end-of-data
          {
            putbits36 (& card [0],  0,  3,             07);  // seven
            putbits36 (& card [0],  3,  6, (nw > 6) & 077);  // cnthi
            putbits36 (& card [0],  9,  3,             05);  // five
            putbits36 (& card [0], 12,  6, (nw > 0) & 077);  // cntlo
            putbits36 (& card [0], 18,  3,             07);  // tag
            putbits36 (& card [0], 21, 15,      cardSeqNo);  // seq
fprintf (stderr, "last seq %d\n", cardSeqNo);
            card [2] = inputWordCnt * 36; // bitcnt
          }

        word36 check = card [0];
        word36 carry = 0;
        for (int i = 2; i <= 23; i ++)
          {
            check += card [i] + carry;
            if (check & BIT37)
              {
            check &= MASK36;
                carry = 1;
              }
            else
              carry = 0;
          }
        check += carry;
        check &= MASK36;
        //card [1] = check;
// write the card image
        
        writeCard ();

        cardSeqNo ++;
        if (nw == 0)
          break;
      }
#ifndef use80
    flushCard ();
#endif
  }

int main (int argc, char * argv [])
  {
    if (strcmp (argv [1], "-f") == 0)
      from ();
    else if (strcmp (argv [1], "-t") == 0)
      to ();
    else
      fprintf (stderr, "viipunch [ -f | -t ]\n");
  }
