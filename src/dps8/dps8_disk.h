void disk_init(void);
int disk_iom_io(int chan, t_uint64 *wordp, int* majorp, int* subp);
int disk_iom_cmd(chan_devinfo* devinfop);

