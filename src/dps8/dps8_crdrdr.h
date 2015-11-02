extern DEVICE crdrdr_dev;
extern UNIT crdrdr_unit [N_CRDRDR_UNITS_MAX];

void crdrdr_init(void);
int crdrdr_iom_cmd (uint iomUnitIdx, uint chan);
void crdrdrCardReady (int unitNum);
void rdrProcessEvent (void);

