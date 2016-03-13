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

	if (div < 0)
	{
		sign = -1;
		div = -div;
	}

	if (dvd < 0) 
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

        if (div < 0)
	{
                sign = -1;
		div = -div;
	}

        if (dvd < 0)
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

	while (u)
	{
		if( u&1 )
			result += v;
		u>>=1;
		v<<=1;
	}

	return result;
}
#endif

#endif
