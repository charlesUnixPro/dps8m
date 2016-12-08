/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern UNIT opcon_unit [N_OPCON_UNITS_MAX];
extern DEVICE opcon_dev;

void console_init(void);
t_stat console_attn (UNUSED UNIT * uptr);
int opconAutoinput (int32 flag, const char *  cptr);
int con_iom_cmd (uint iomUnitIdx, uint chan);
bool check_attn_key (void);
void consoleProcess (void);
