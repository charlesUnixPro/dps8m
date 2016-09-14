t_stat hdbg_size (int32 arg, UNUSED char * buf);
#ifdef HDBG
void hdbgTrace (void);
void hdbgPrint (void);
void hdbgMRead (word24 addr, word36 data);
void hdbgMWrite (word24 addr, word36 data);
void hdbgFault (_fault faultNumber, _fault_subtype subFault,
                const char * faultMsg);
#endif
