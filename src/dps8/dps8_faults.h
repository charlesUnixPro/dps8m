struct _fault_register {
    // even word
    bool    ill_op;     // IPR fault. An illegal operation code has been detected.
    bool    ill_mod;    // IPR fault. An illegal address modifier has been detected.
    bool    ill_slv;    // IPR fault. An illegal BAR mode procedure has been encountered.
    bool    ill_proc;   // IPR fault. An illegal procedure other than the three above has been encountered.
    bool    nem;        // ONC fault. A nonexistent main memory address has been requested.
    bool    oob;        // STR fault. A BAR mode boundary violation has occurred.
    bool    ill_dig;    // IPR fault. An illegal decimal digit or sign has been detected by the decimal unit.
    bool    proc_paru;  // PAR fault. A parity error has been detected in the upper 36 bits of data. (Yeah, right)
    bool    proc_parl;  // PAR fault. A parity error has been detected in the lower 36 bits of data. (Yeah, right)
    bool    con_a;      // CON fault. A $CONNECT signal has been received through port A.
    bool    con_b;      // CON fault. A $CONNECT signal has been received through port B.
    bool    con_c;      // CON fault. A $CONNECT signal has been received through port C.
    bool    con_d;      // CON fault. A $CONNECT signal has been received through port D.
    bool    da_err;     // ONC fault. Operation not complete. Processor/system controller interface sequence error 1 has been detected. (Yeah, right)
    bool    da_err2;    // ONC fault. Operation not completed. Processor/system controller interface sequence error 2 has been detected.
    int     ia_a;       // Coded illegal action, port A. (See Table 3-2)
    int     ia_b;       // Coded illegal action, port B. (See Table 3-2)
    int     ia_c;       // Coded illegal action, port C. (See Table 3-2)
    int     ia_d;       // Coded illegal action, port D. (See Table 3-2)
    bool    cpar_div;   // A parity error has been detected in the cache memory directory. (Not likely)
    bool    cpar_str;   // PAR fault. A data parity error has been detected in the cache memory.
    bool    cpar_ia;    // PAR fault. An illegal action has been received from a system controller during a store operation with cache memory enabled.
    bool    cpar_blk;   // PAR fault. A cache memory parity error has occurred during a cache memory data block load.
    
    // odd word
    //      Cache Duplicate Directory WNO Buffer Overflow
    bool    port_a;
    bool    port_b;
    bool    port_c;
    bool    port_d;
    
    bool    cpd;  // Cache Primary Directory WNO Buffer Overflow
    // Write Notify (WNO) Parity Error on Port A, B, C, or D.
    
    //      Cache Duplicate Directory Parity Error
    bool    level_0;
    bool    level_1;
    bool    level_2;
    bool    level_3;
    
    // Cache Duplicate Directory Multiple Match
    bool    cdd;
    
    bool    par_sdwam;  // A parity error has been detected in the SDWAM.
    bool    par_ptwam;  // A parity error has been detected in the PTWAM.
};

struct dps8faults
{
    int         fault_number;
    int         fault_address;
    const char *fault_mnemonic;
    const char *fault_name;
    int         fault_priority;
    int         fault_group;
    bool        fault_pending;        // when true fault is pending and waiting to be processed
};
typedef struct dps8faults dps8faults;
void check_events (void);
void clearFaultCycle (void);
void emCallReportFault (void);

void cu_safe_restore (void);

void doFault(DCDstruct *, _fault faultNumber, _fault_subtype faultSubtype, const char *faultMsg) ///< fault handler
#ifdef __GNUC__
  __attribute__ ((noreturn))
#endif
;
bool bG7Pending();

