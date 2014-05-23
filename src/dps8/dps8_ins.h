// XXX these ought to moved to DCDstruct 
extern word36 CY;
extern word36 Ypair[2];
extern word36 Yblock8[8];
extern word36 Yblock16[16];

void tidy_cu (void);
void cu_safe_store(void);
void initializeTheMatrix (void);
void addToTheMatrix (uint32 opcode, bool opcodeX, bool a, word6 tag);
t_stat displayTheMatrix (int32 arg, char * buf);
t_stat prepareComputedAddress(DCDstruct *ci);   // new
void cu_safe_restore(void);
DCDstruct * setupInstruction (void);
DCDstruct *fetchInstruction(word18 addr, DCDstruct *dst);      // fetch (+ decode) instrcution at address
t_stat executeInstruction(DCDstruct *ci);


