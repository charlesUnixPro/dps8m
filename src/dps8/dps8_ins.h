// XXX these ought to moved to DCDstruct 
extern word36 CY;
extern word36 Ypair[2];
extern word36 Yblock8[8];
extern word36 Yblock16[16];
extern word36 Yblock32[32];

void tidy_cu (void);
void cu_safe_store(void);
void initializeTheMatrix (void);
void addToTheMatrix (uint32 opcode, bool opcodeX, bool a, word6 tag);
t_stat displayTheMatrix (int32 arg, char * buf);
t_stat prepareComputedAddress (void);   // new
void cu_safe_restore(void);
void fetchInstruction(word18 addr);
t_stat executeInstruction (void);
void doRCU (bool fxeTrap) NO_RETURN;
void traceInstruction (uint flag);


