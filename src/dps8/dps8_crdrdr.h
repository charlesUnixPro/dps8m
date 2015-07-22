extern DEVICE crdrdr_dev;
extern UNIT crdrdr_unit [N_CRDRDR_UNITS_MAX];

void crdrdr_init(void);
int crdrdr_iom_cmd (UNIT * unitp, pcw_t * pcwp);

