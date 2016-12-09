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

extern DEVICE disk_dev;
extern UNIT disk_unit [N_DISK_UNITS_MAX];

void disk_init(void);
void loadDisk (uint driveNumber, char * diskFilename);
t_stat attachDisk (char * label);
int disk_iom_cmd (uint iomUnitIdx, uint chan);


