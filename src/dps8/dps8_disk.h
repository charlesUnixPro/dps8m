extern DEVICE disk_dev;
extern UNIT disk_unit [N_DISK_UNITS_MAX];

void disk_init(void);
void loadDisk (uint driveNumber, char * diskFilename);
t_stat attachDisk (char * label);
int disk_iom_cmd (uint iomUnitIdx, uint chan);


