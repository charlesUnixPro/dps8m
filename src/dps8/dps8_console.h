void console_init(void);
t_stat cable_opcon (int con_unit_num, int iom_unit_num, int chan_num, int dev_code);
t_stat console_attn (UNUSED UNIT * uptr);
int opconAutoinput (int32 flag, char *  cptr);

extern DEVICE opcon_dev;

bool check_attn_key (void);
void consoleProcess (void);

