long double EAQToIEEElongdouble(void);
#ifndef QUIET_UNUSED
float72 IEEElongdoubleToFloat72(long double f);
void IEEElongdoubleToEAQ(long double f0);
#endif
#ifndef QUIET_UNUSED
double float36ToIEEEdouble(float36 f36);
float36 IEEEdoubleTofloat36(double f);
#endif
void ufa (void);
void ufs (void);
void fno (void);
void fnoEAQ(word8 *E, word36 *A, word36 *Q);

void fneg (void);
void ufm (void);
void fdv (void);
void fdi (void);
void frd (void);
void fcmp(void);
void fcmg(void);

void dufa (void);
void dufs (void);
void dufm (void);
void dfdv (void);
void dfdi (void);
void dfrd (void);
void dfcmp (void);
void dfcmg (void);

void dvf (void);

void dfstr (word36 *Ypair);
void fstr(word36 *CY);


