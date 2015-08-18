extern DEVICE prt_dev;
extern UNIT prt_unit [N_PRT_UNITS_MAX];

void prt_init(void);
int prt_iom_cmd (UNIT * unitp, pcw_t * pcwp);

