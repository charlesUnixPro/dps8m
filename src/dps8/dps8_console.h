void console_init(void);
t_stat cable_opcon (int con_unit_num, int iom_unit_num, int chan_num, int dev_code);
int con_iom_fault(int chan, bool pre);

extern DEVICE opcon_dev;


