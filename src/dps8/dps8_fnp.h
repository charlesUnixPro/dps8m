extern UNIT fnp_unit [N_FNP_UNITS_MAX];
extern DEVICE fnpDev;
void fnpInit(void);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (char *nodename, char *id, char *arg3);
int fnpIOMCmd (UNIT * unitp, pcw_t * pcwp);
