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
void console_attn_idx (int con_unit_idx);
int opconAutoinput (int32 flag, const char * cptr);
int opconClearAutoinput (int32 flag, const char * cptr);
int con_iom_cmd (uint iomUnitIdx, uint chan);
int check_attn_key (void);
void consoleProcess (void);
t_stat consolePort (UNUSED int32 arg, const char * buf);
t_stat consolePW (UNUSED int32 arg, const char * buf);
void startRemoteConsole (void);
