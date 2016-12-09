/*
 Copyright 2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#define FNP_NOLINK  (-1)
#define FNP_MAXDATA      32768      // longest possible packet (in bytes)

// Flags used by dialout
#define UF_CALL 1
#define UF_ANS  2


int fnp_udp_create (char * premote, int32_t * plink);
int fnp_udp_release (int32_t link);
int fnp_udp_send (int32_t link, char * pdata, uint16_t count, uint16_t flags);
int fnp_udp_receive (int32_t link, char * pdata, uint16_t maxbufg, uint16_t * flags);

