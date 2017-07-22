/*
 Copyright 2016 by Jean-Michel Merliot

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 *//*
Defines 128 bits Integer for 32 bits platform
*/

#ifdef NEED_128

#define cast_128(x) (* (uint128 *) & (x))
#define cast_s128(x) (* (int128 *) & (x))

bool iszero_128 (uint128 w);
bool isnonzero_128 (uint128 w);
bool iseq_128 (uint128 a, uint128 b);
bool isgt_128 (uint128 a, uint128 b);
bool islt_128 (uint128 a, uint128 b);
bool isge_128 (uint128 a, uint128 b);
bool islt_s128 (int128 a, int128 b);
bool isgt_s128 (int128 a, int128 b);
uint128 and_128 (uint128 a, uint128 b);
int128 and_s128 (int128 a, uint128 b);
uint128 or_128 (uint128 a, uint128 b);
uint128 xor_128 (uint128 a, uint128 b);
uint128 add_128 (uint128 a, uint128 b);
uint128 subtract_128 (uint128 a, uint128 b);
uint128 multiply_128 (uint128 a, uint128 b);
int128 multiply_s128 (int128 a, int128 b);
uint128 divide_128_16 (uint128 a, uint16_t b, uint16_t * rem);
uint128 divide_128_32 (uint128 a, uint32_t b, uint32_t * rem);
uint128 divide_128 (uint128 a, uint128 b, uint128 * rem);
uint128 complement_128 (uint128 a);
uint128 negate_128 (uint128 a);
int128 negate_s128 (int128 a);
uint128 lshift_128 (uint128 a, unsigned int n);
int128 lshift_s128 (int128 a, unsigned int n);
uint128 rshift_128 (uint128 a, unsigned int n);
int128 rshift_s128 (int128 a, unsigned int n);
#else

/* if (sizeof(long) < 8), I expect we're on a 32 bit system */

#if __SIZEOF_LONG__ < 8 && ! defined (__MINGW64__)

typedef          int TItype     __attribute__ ((mode (TI)));
typedef unsigned int UTItype    __attribute__ ((mode (TI)));

typedef TItype __int128_t ;
typedef UTItype __uint128_t ;
#endif
#endif
