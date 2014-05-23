long double EAQToIEEElongdouble(void);
#ifndef QUIET_UNUSED
float72 IEEElongdoubleToFloat72(long double f);
void IEEElongdoubleToEAQ(long double f0);
#endif
#ifndef QUIET_UNUSED
double float36ToIEEEdouble(float36 f36);
float36 IEEEdoubleTofloat36(double f);
#endif
void ufa(DCDstruct *);
void ufs(DCDstruct *);
void fno(DCDstruct *);
void fnoEAQ(word8 *E, word36 *A, word36 *Q);

void fneg(DCDstruct *);
void ufm(DCDstruct *);
void fdv(DCDstruct *);
void fdi(DCDstruct *);
void frd(DCDstruct *);
void fcmp(DCDstruct *);
void fcmg(DCDstruct *);

void dufa(DCDstruct *);
void dufs(DCDstruct *);
void dufm(DCDstruct *);
void dfdv(DCDstruct *);
void dfdi(DCDstruct *);
void dfrd(DCDstruct *);
void dfcmp(DCDstruct *);
void dfcmg(DCDstruct *);

void dvf(DCDstruct *);

void dfstr(DCDstruct *, word36 *Ypair);
void fstr(DCDstruct *, word36 *CY);


