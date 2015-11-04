extern UNIT opcon_unit [N_OPCON_UNITS_MAX];
extern DEVICE opcon_dev;

void console_init(void);
t_stat console_attn (UNUSED UNIT * uptr);
int opconAutoinput (int32 flag, char *  cptr);
int con_iom_cmd (uint iomUnitIdx, uint chan);
bool check_attn_key (void);
void consoleProcess (void);
