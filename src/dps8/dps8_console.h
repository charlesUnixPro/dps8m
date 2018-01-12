/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern UNIT opc_unit [N_OPC_UNITS_MAX];
extern DEVICE opc_dev;

void console_init(void);
void console_attn_idx (int opc_unit_idx);
int add_opc_autoinput (int32 flag, const char * cptr);
int clear_opc_autoinput (int32 flag, const char * cptr);
int opc_iom_cmd (uint iomUnitIdx, uint chan);
int check_attn_key (void);
void consoleProcess (void);
t_stat set_console_port (UNUSED int32 arg, const char * buf);
t_stat set_console_pw (UNUSED int32 arg, const char * buf);
void startRemoteConsole (void);
