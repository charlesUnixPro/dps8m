t_stat fxe (int32 arg, char * buf);
t_stat fxeDump (int32 __attribute__((unused)) arg,
                char * __attribute__((unused)) buf);
void fxeFaultHandler (void);
char * lookupFXESegmentAddress (word18 segno, word18 offset, 
                                char * * compname, word18 * compoffset);
void fxeSetCall6Trap (void);
void fxeCall6TrapRestore (void);
