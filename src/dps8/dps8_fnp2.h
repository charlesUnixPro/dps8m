#define encodeline(fnp,line) ((fnp) * MAX_LINES + (line))
#define decodefnp(coded) ((coded) / MAX_LINES)
#define decodeline(coded) ((coded) % MAX_LINES)
#define noassoc -1

extern UNIT fnp_unit [N_FNP_UNITS_MAX];
extern DEVICE fnpDev;
extern DEVICE mux_dev;
void fnpInit(void);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
int lookupFnpLink (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (int fnpUnitNum, char *arg3);
void fnpToCpuQueueMsg (int fnpUnitNum, char * msg);
int fnpIOMCmd (uint iomUnitIdx, uint chan);
t_stat fnpServerPort (int32 arg, char * buf);
void fnpConnectPrompt (void * client);
void processUserInput (void * client, char * buf, ssize_t nread);
void processLineInput (void * client, char * buf, ssize_t nread);
