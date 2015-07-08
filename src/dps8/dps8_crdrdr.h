extern DEVICE crdrdr_dev;

void crdrdr_init(void);
t_stat cable_crdrdr (int crdrdr_unit_num, int iom_unit_num, int chan_num, int dev_code);

