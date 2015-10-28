extern DEVICE prt_dev;
extern UNIT prt_unit [N_PRT_UNITS_MAX];

void prt_init(void);
int prt_iom_cmd (uint iomUnitIdx, uint chan);
