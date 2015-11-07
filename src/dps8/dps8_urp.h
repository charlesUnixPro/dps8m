extern DEVICE urp_dev;
extern UNIT urp_unit [N_URP_UNITS_MAX];

void urp_init(void);
int urp_iom_cmd (uint iomUnitIdx, uint chan);
