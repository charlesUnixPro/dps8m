/*
Defines 128 bits Integer for 32 bits platform
*/

/* if (sizeof(long) < 8), I expect we're on a 32 bit system */

#if __SIZEOF_LONG__ < 8
typedef          int TItype     __attribute__ ((mode (TI)));
typedef unsigned int UTItype    __attribute__ ((mode (TI)));

typedef TItype __int128_t ;
typedef UTItype __uint128_t ;
#endif
