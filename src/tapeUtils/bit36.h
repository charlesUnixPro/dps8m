/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

typedef uint16_t word9;
typedef uint32_t word18;
typedef uint64_t word36;
typedef uint64_t word36;
typedef unsigned int uint;
typedef __uint128_t word72;

word36 extr36 (uint8_t * bits, uint woffset);
word9 extr9 (uint8_t * bits, uint coffset);
word18 extr18 (uint8_t * bits, uint coffset);
uint8_t getbit (void * bits, int offset);
uint64_t extr (void * bits, int offset, int nbits);
void put36 (word36 val, uint8_t * bits, uint woffset);
