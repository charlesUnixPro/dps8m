/*
 Copyright 2015-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#define NOLINK  (-1)
#define PFLG_FINAL 00001
int udp_create (const char * premote, int32_t * plink);
int udp_release (int32_t link);
int udp_send (int32_t link, uint16_t * pdata, uint16_t count, uint16_t flags);
int udp_receive (int32_t link, uint16_t * pdata, uint16_t maxbufg);

