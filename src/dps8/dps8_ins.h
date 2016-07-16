void tidy_cu (void);
void cu_safe_store(void);
void initializeTheMatrix (void);
void addToTheMatrix (uint32 opcode, bool opcodeX, bool a, word6 tag);
t_stat displayTheMatrix (int32 arg, char * buf);
t_stat prepareComputedAddress (void);   // new
void cu_safe_restore(void);
void fetchInstruction(word18 addr);
t_stat executeInstruction (void);
void doRCU (void) NO_RETURN;
void traceInstruction (uint flag);
bool tstOVFfault (void);

#ifdef REAL_TR
void setTR (word27 val);
word27 getTR (bool * runout);
void ackTR (void);
#endif

bool chkOVF (void);
