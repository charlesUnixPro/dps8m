/*
Defines 128 bits Integer for 32 bits platform
*/

#ifndef __int128_t
typedef          int TItype     __attribute__ ((mode (TI)));
typedef unsigned int UTItype    __attribute__ ((mode (TI)));

typedef TItype __int128_t ;
typedef UTItype __uint128_t ;
#endif
