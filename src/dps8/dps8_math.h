/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

long double EAQToIEEElongdouble(void);
double EAQToIEEEdouble(void);
#ifndef QUIET_UNUSED
float72 IEEElongdoubleToFloat72(long double f);
void IEEElongdoubleToEAQ(long double f0);
double float36ToIEEEdouble(float36 f36);
float36 IEEEdoubleTofloat36(double f);
#endif
void ufa (bool sub);
void ufs (void);
void fno (word8 * E, word36 * A, word36 * Q);

void fneg (void);
void ufm (void);
void fdv (void);
void fdi (void);
void frd (void);
void fcmp(void);
void fcmg(void);

//void dufa (void);
//void dufs (void);
void dufa (bool subtraact);
void dufm (void);
void dfdv (void);
void dfdi (void);
void dfrd (void);
void dfcmp (void);
void dfcmg (void);

void dvf (void);

void dfstr (word36 *Ypair);
void fstr(word36 *CY);


