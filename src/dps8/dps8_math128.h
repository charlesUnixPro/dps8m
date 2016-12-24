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

/* if (sizeof(long) < 8), I expect we're on a 32 bit system */

#if __SIZEOF_LONG__ < 8 && ! defined (__MINGW64__)
typedef          int TItype     __attribute__ ((mode (TI)));
typedef unsigned int UTItype    __attribute__ ((mode (TI)));

typedef TItype __int128_t ;
typedef UTItype __uint128_t ;
#endif
