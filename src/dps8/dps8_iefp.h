t_stat Read (word18 addr, word36 *dat, _processor_cycle_type cyctyp, bool b29);
t_stat Write (word18 addr, word36 dat, _processor_cycle_type cyctyp, bool b29);
t_stat Write8 (word18 address, word36 * data, _processor_cycle_type cyctyp, bool b29);
t_stat Read8 (word18 address, word36 * result, _processor_cycle_type cyctyp, bool b29);
t_stat WritePage (word18 address, word36 * data, _processor_cycle_type cyctyp, bool b29);
t_stat ReadPage (word18 address, word36 * result, _processor_cycle_type cyctyp, bool b29);

