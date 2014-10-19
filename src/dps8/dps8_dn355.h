extern DEVICE dn355_dev;

void dn355_init(void);
t_stat cable_dn355 (int dn355_unit_num, int iom_unit_num, int chan_num, int dev_code);

