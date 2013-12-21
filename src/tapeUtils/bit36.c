#include <stdint.h>
#include "bit36.h"
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

// Multics humor: this is idiotic


// Data conversion routines
//
//  'bits' is the packed bit stream read from the simh tape
//    it is assumed to start at an even word36 address
//
//   extr36
//     extract the word36 at woffset
//

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
    // mask shouldn't be neccessary but is robust
  }

//
//   extr9
//     extract the word9 at coffset
//
//   | 012345678 | 012345678 |012345678 | 012345678 | 012345678 | 012345678 | 012345678 | 012345678 |
//     0       1          2         3          4          5          6          7          8
//     012345670   123456701  234567012   345670123   456701234   567012345   670123456   701234567  
//

word9 extr9 (uint8_t * bits, uint coffset)
  {
    uint charNum = coffset % 8;
    uint dwoffset = coffset / 8;
    uint8_t * p = bits + dwoffset * 9;

    word9 w;
    switch (charNum)
      {
        case 0:
          w = ((((word9) p [0]) << 1) & 0776) | ((((word9) p [1]) >> 7) & 0001);
          break;
        case 1:
          w = ((((word9) p [1]) << 2) & 0774) | ((((word9) p [2]) >> 6) & 0003);
          break;
        case 2:
          w = ((((word9) p [2]) << 3) & 0770) | ((((word9) p [3]) >> 5) & 0007);
          break;
        case 3:
          w = ((((word9) p [3]) << 4) & 0760) | ((((word9) p [4]) >> 4) & 0017);
          break;
        case 4:
          w = ((((word9) p [4]) << 5) & 0740) | ((((word9) p [5]) >> 3) & 0037);
          break;
        case 5:
          w = ((((word9) p [5]) << 6) & 0700) | ((((word9) p [6]) >> 2) & 0077);
          break;
        case 6:
          w = ((((word9) p [6]) << 7) & 0600) | ((((word9) p [7]) >> 1) & 0177);
          break;
        case 7:
          w = ((((word9) p [7]) << 8) & 0400) | ((((word9) p [8]) >> 0) & 0377);
          break;
      }
    // mask shouldn't be neccessary but is robust
    return w & 0777U;
  }

//
//  getbit
//     Get a single bit. offset can be bigger when word size
//

uint8_t getbit (void * bits, int offset)
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

uint64_t extr (void * bits, int offset, int nbits)
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


