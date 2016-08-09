void tidy_cu (void);
void cu_safe_store(void);
#ifdef MATRIX
void initializeTheMatrix (void);
void addToTheMatrix (uint32 opcode, bool opcodeX, bool a, word6 tag);
#endif
t_stat displayTheMatrix (int32 arg, char * buf);
t_stat prepareComputedAddress (void);   // new
void cu_safe_restore(void);
void fetchInstruction(word18 addr);
t_stat executeInstruction (void);
void doRCU (void) NO_RETURN;
void traceInstruction (uint flag);
bool tstOVFfault (void);
bool chkOVF (void);
