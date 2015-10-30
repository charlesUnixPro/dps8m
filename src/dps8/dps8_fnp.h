extern UNIT fnp_unit [N_FNP_UNITS_MAX];
extern DEVICE fnpDev;
extern DEVICE mux_dev;
void fnpInit(void);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (char *nodename, char *id, char *arg3);
void fnpQueueMsg (char * msg);
int fnpIOMCmd (uint iomUnitIdx, uint chan);
