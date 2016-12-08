/*
 Copyright 2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

t_stat hdbg_size (int32 arg, UNUSED const char * buf);
#ifdef HDBG
void hdbgTrace (void);
void hdbgPrint (void);
void hdbgMRead (word24 addr, word36 data);
void hdbgMWrite (word24 addr, word36 data);
void hdbgFault (_fault faultNumber, _fault_subtype subFault,
                const char * faultMsg);
#endif
