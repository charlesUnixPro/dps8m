/*
 Copyright 2016 by Jean-Michel Merliot

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#ifndef DPS8_MATH128
#define DPS8_MATH128

#if __SIZEOF_LONG__ < 8

#include "dps8_math128.h"

void __udivmodti3(UTItype div, UTItype dvd,UTItype *result,UTItype *remain);

UTItype __umodti3(UTItype div, UTItype dvd);

UTItype __udivti3(UTItype div, UTItype dvd)
{
	UTItype result,remain;

	__udivmodti3(div,dvd,&result,&remain);

	return result;
}

void __udivmodti3(UTItype div, UTItype dvd,UTItype *result,UTItype *remain)
{
	UTItype z1 = dvd;
	UTItype z2 = (UTItype)1;

	*result = (UTItype)0;
	*remain = div;

	if( z1 == (UTItype)0)
		1/0;

	while( z1 < *remain )
	{
		z1 <<= 1 ;
		z2 <<= 1;
	}

	do 
	{
		if( *remain >= z1 )
		{
			*remain -= z1;
			*result += z2;
		}
		z1 >>= 1;
		z2 >>= 1;
	} while( z2 );

}

TItype __divti3(TItype div, TItype dvd)
{
	int sign=1;

	if (div < (TItype)0)
	{
		sign = -1;
		div = -div;
	}

	if (dvd < (TItype)0) 
	{
		sign = -sign;
		dvd = -dvd;
	}

	if (sign > 0)
		return (TItype)__udivti3(div,dvd);
	else
		return -((TItype)__udivti3(div,dvd));
}

TItype __modti3(TItype div, TItype dvd)
{
        int sign=1;

        if (div < (TItype)0)
	{
                sign = -1;
		div = -div;
	}

        if (dvd < (TItype)0)
	{
                sign = -sign;
		dvd = -dvd;
	}

        if (sign > 0)
                return (TItype)__umodti3(div,dvd);
        else
		return ((TItype)0-(TItype)__umodti3(div,dvd));
}

UTItype __umodti3(UTItype div, UTItype dvd)
{
	UTItype result,remain;

	__udivmodti3(div,dvd,&result,&remain);

	return remain;
}

TItype __multi3 (TItype u, TItype v)
{
	TItype result = (TItype)0;
	int sign = 1;

	if(u<0)
	{
		sign = -1;
		u = -u;
	}

	while (u!=(TItype)0)
	{
		if( u&(TItype)1 )
			result += v;
		u>>=1;
		v<<=1;
	}

	if ( sign < 0 )
		return -result;
	else
		return result;

}
#endif

#endif
