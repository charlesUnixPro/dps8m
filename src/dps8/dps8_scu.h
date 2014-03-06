extern DEVICE scu_dev;
extern UNIT scu_unit [];
int scu_set_interrupt(uint scu_unit_num, uint inum);
t_stat cable_to_scu (int scu_unit_num, int scu_port_num, int iom_unit_num, int iom_port_num);
t_stat cable_scu (int scu_unit_num, int scu_port_num, int cpu_unit_num, int cpu_port_num);
void scu_init (void);
t_stat scu_sscr (uint scu_unit_num, uint cpu_unit_num, word36 addr, word36 rega, word36 regq);
t_stat scu_rscr (uint scu_unit_num, uint cpu_unit_num, word36 addr, word36 * rega, word36 * regq);
int scu_cioc (uint scu_unit_num, uint scu_port_num);
t_stat scu_rmcm (uint scu_unit_num, uint cpu_unit_num, word36 * rega, word36 * regq);
t_stat scu_smcm (uint scu_unit_num, uint cpu_unit_num, word36 rega, word36 regq);


