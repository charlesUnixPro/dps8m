
extern UNIT mt_unit [N_MT_UNITS_MAX];
extern DEVICE tape_dev;
void mt_init(void);
int get_mt_numunits (void);
//UNIT * getTapeUnit (uint driveNumber);
//void tape_send_special_interrupt (uint driveNumber);
void loadTape (uint driveNumber, char * tapeFilename, bool ro);
t_stat attachTape (char * label, bool withring, char * drive);
t_stat detachTape (char * drive);
int mt_iom_cmd (uint iomUnitIdx, uint chan);
