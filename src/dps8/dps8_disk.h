extern DEVICE disk_dev;

void disk_init(void);
t_stat cable_disk (int disk_unit_num, int iom_unit_num, int chan_num, int dev_code);

