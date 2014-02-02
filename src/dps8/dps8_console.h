void console_init(void);
t_stat cable_opcon (int iom_unit_num, int chan_num);
int con_iom_cmd(chan_devinfo * devinfop, int chan, int dev_cmd, int dev_code, int* majorp, int* subp);
int con_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp);
int con_iom_fault(int chan, bool pre);


