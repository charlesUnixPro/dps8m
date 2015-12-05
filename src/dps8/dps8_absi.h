extern DEVICE absi_dev;
extern UNIT absi_unit [N_ABSI_UNITS_MAX];

void absi_init(void);
int absi_iom_cmd (uint iomUnitIdx, uint chan);
