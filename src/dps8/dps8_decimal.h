#define DECUSE64    1
#define DECSUBSET   1
#define DECDPUN     8
#define DECBUFFER   32
    
#define DECNUMDIGITS    64
    
#include "decNumber.h"        // base number library
#include "decNumberLocal.h"   // decNumber local types, etc.

#define PRINTDEC(msg, dn) \
    { \
        if_sim_debug (DBG_TRACEEXT, & cpu_dev) /**/ \
        { \
            char temp[256]; \
            decNumberToString(dn, temp); \
            sim_printf("%s:'%s'\n", msg, temp);   \
        } \
    }
#define PRINTALL(msg, dn, set) \
    { \
        if_sim_debug (DBG_TRACEEXT, & cpu_dev) /**/ \
        sim_printf("%s:'%s E%d'\n", msg, getBCDn(dn, set->digits), dn->exponent);   \
    }


decContext * decContextDefaultDPS8(decContext *context);
decNumber * decBCD9ToNumber(const word9 *bcd, Int length, const Int scale, decNumber *dn);
char *formatDecimal(decContext *set, decNumber *r, int tn, int n, int s, int sf, bool R, bool *OVR, bool *TRUNC);
void mp2d (void);
void mp3d (void);
void dv2d (void);
void dv3d (void);


