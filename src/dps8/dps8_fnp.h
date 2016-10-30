/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern UNIT fnp_unit [N_FNP_UNITS_MAX];
extern DEVICE fnpDev;
extern DEVICE mux_dev;
void fnpInit(void);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
int lookupFnpLink (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (int fnpUnitNum, char *arg3);
void fnpToCpuQueueMsg (int fnpUnitNum, char * msg);
int fnpIOMCmd (uint iomUnitIdx, uint chan);
