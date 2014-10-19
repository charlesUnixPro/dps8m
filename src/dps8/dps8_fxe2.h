extern DEVICE fxe2_dev;
t_stat fxe2 (int32 arg, char * buf);
t_stat fxe2Dump (int32 arg, char * buf);
void fxe2FaultHandler (void);
char * lookupFXE2SegmentAddress (word18 segno, word18 offset, 
                                char * * compname, word18 * compoffset);
void fxe2SetCall6Trap (void);
void fxe2Call6TrapRestore (void);
