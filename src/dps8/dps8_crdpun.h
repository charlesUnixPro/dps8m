extern DEVICE crdpun_dev;
extern UNIT crdpun_unit [N_CRDPUN_UNITS_MAX];

void crdpun_init(void);
int crdpun_iom_cmd (uint iomUnitIdx, uint chan);
