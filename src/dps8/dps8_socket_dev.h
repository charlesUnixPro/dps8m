/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2017 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern UNIT sk_unit [N_SK_UNITS_MAX];
extern DEVICE sk_dev;
void sk_init(void);
int sk_iom_cmd (uint iomUnitIdx, uint chan);
void sk_process_event (void);
