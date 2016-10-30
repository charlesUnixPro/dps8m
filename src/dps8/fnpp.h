/*
 Copyright 2014-2016 by Harry Reed
 Copyright 2014-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

void fnppInit (void);
t_stat fnppSetNunits (UNUSED UNIT * uptr, UNUSED int32 value,
                             char * cptr, UNUSED void * desc);
t_stat fnppReset (UNUSED DEVICE * dptr);
#if 0
int fnppIDCW (UNIT * unitp, uint unitNumber);
int fnppIOTx (UNIT * unitp, uint unitNumber);
#endif
int fnppCIOC (UNUSED UNIT * unitp, uint chanNum);

