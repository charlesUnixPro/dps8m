/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern DEVICE pun_dev;
extern UNIT pun_unit [N_PUN_UNITS_MAX];

void pun_init(void);
int pun_iom_cmd (uint iomUnitIdx, uint chan);
