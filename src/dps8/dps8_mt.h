void mt_init(void);
t_stat cable_mt (int mt_unit_num, int iom_unit_num, int chan_num, int dev_code);
int get_mt_numunits (void);
int mt_iom_cmd(chan_devinfo* devinfop);
int mt_iom_io(int iom_unit_num, int chan, int dev_code, t_uint64 *wordp, int* majorp, int* subp);


