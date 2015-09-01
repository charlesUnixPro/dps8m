#define DECUSE64    1
#define DECSUBSET   1
#define DECDPUN     8
#define DECBUFFER   32
    
#define DECNUMDIGITS    64
    
#include "decNumber.h"        // base number library
#include "decNumberLocal.h"   // decNumber local types, etc.


decNumber * decBCD9ToNumber(const word9 *bcd, Int length, const Int scale, decNumber *dn);
void ad2d (void);
void ad3d (void);
void sb2d (void);
void sb3d (void);
void mp2d (void);
void mp3d (void);
void dv2d (void);
void dv3d (void);
void cmpn (void);
void mvn (void);


