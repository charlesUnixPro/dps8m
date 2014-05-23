void console_init(void);
t_stat cable_opcon (int iom_unit_num, int chan_num);
int con_iom_fault(int chan, bool pre);

extern DEVICE opcon_dev;


