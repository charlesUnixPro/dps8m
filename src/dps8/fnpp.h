void fnppInit (void);
t_stat fnppSetNunits (UNUSED UNIT * uptr, UNUSED int32 value,
                             char * cptr, UNUSED void * desc);
t_stat fnppReset (UNUSED DEVICE * dptr);
#if 0
int fnppIDCW (UNIT * unitp, uint unitNumber);
int fnppIOTx (UNIT * unitp, uint unitNumber);
#endif
int fnppCIOC (UNUSED UNIT * unitp, uint chanNum);

