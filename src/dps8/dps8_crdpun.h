/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern DEVICE crdpun_dev;
extern UNIT crdpun_unit [N_CRDPUN_UNITS_MAX];

void crdpun_init(void);
int crdpun_iom_cmd (uint iomUnitIdx, uint chan);
