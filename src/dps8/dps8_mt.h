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

extern UNIT mt_unit [N_MT_UNITS_MAX];
extern DEVICE tape_dev;
void mt_init(void);
int get_mt_numunits (void);
//UNIT * getTapeUnit (uint driveNumber);
//void tape_send_special_interrupt (uint driveNumber);
void loadTape (uint driveNumber, char * tapeFilename, bool ro);
t_stat attachTape (char * label, bool withring, char * drive);
t_stat detachTape (char * drive);
int mt_iom_cmd (uint iomUnitIdx, uint chan);
t_stat mountTape (int32 arg, const char * buf);

