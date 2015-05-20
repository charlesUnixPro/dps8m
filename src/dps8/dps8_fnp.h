extern DEVICE fnpDev;
void fnpInit(void);
t_stat cableFNP (int fnp_unit_num, int iom_unit_num, int chan_num, int dev_code);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (char *nodename, char *id, char *arg3);
