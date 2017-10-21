/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#ifndef QUIET_UNUSED
struct _fault_register {
    // even word
    bool    ILL_OP;     // IPR fault. An illegal operation code has been detected.
    bool    ILL_MOD;    // IPR fault. An illegal address modifier has been detected.
    bool    ILL_SLV;    // IPR fault. An illegal BAR mode procedure has been encountered.
    bool    ILL_PROC;   // IPR fault. An illegal procedure other than the three above has been encountered.
    bool    NEM;        // ONC fault. A nonexistent main memory address has been requested.
    bool    OOB;        // STR fault. A BAR mode boundary violation has occurred.
    bool    ILL_DIG;    // IPR fault. An illegal decimal digit or sign has been detected by the decimal unit.
    bool    PROC_PARU;  // PAR fault. A parity error has been detected in the upper 36 bits of data. (Yeah, right)
    bool    PROC_PARL;  // PAR fault. A parity error has been detected in the lower 36 bits of data. (Yeah, right)
    bool    CON_A;      // CON fault. A $CONNECT signal has been received through port A.
    bool    CON_B;      // CON fault. A $CONNECT signal has been received through port B.
    bool    CON_C;      // CON fault. A $CONNECT signal has been received through port C.
    bool    CON_D;      // CON fault. A $CONNECT signal has been received through port D.
    bool    DA_ERR;     // ONC fault. Operation not complete. Processor/system controller interface sequence error 1 has been detected. (Yeah, right)
    bool    DA_ERR2;    // ONC fault. Operation not completed. Processor/system controller interface sequence error 2 has been detected.
    int     IA_A;       // Coded illegal action, port A. (See Table 3-2)
    int     IA_B;       // Coded illegal action, port B. (See Table 3-2)
    int     IA_C;       // Coded illegal action, port C. (See Table 3-2)
    int     IA_D;       // Coded illegal action, port D. (See Table 3-2)
    bool    CPAR_DIV;   // A parity error has been detected in the cache memory directory. (Not likely)
    bool    CPAR_STR;   // PAR fault. A data parity error has been detected in the cache memory.
    bool    CPAR_IA;    // PAR fault. An illegal action has been received from a system controller during a store operation with cache memory enabled.
    bool    CPAR_BLK;   // PAR fault. A cache memory parity error has occurred during a cache memory data block load.
    
    // odd word
    //      Cache Duplicate Directory WNO Buffer Overflow
    bool    PORT_A;
    bool    PORT_B;
    bool    PORT_C;
    bool    PORT_D;
    
    bool    CPD;  // Cache Primary Directory WNO Buffer Overflow
    // Write Notify (WNO) Parity Error on Port A, B, C, or D.
    
    //      Cache Duplicate Directory Parity Error
    bool    LEVEL_0;
    bool    LEVEL_1;
    bool    LEVEL_2;
    bool    LEVEL_3;
    
    // Cache Duplicate Directory Multiple Match
    bool    CDD;
    
    bool    PAR_SDWAM;  // A parity error has been detected in the SDWAM.
    bool    PAR_PTWAM;  // A parity error has been detected in the PTWAM.
};
#endif

#ifndef QUIET_UNUSED
struct dps8faults
{
    int         fault_number;
    int         fault_address;
    const char *fault_mnemonic;
    const char *fault_name;
    int         fault_priority;
    int         fault_group;
};
typedef struct dps8faults dps8faults;
#endif

extern char * faultNames [N_FAULTS];
void check_events (void);
void clearFaultCycle (void);
void emCallReportFault (void);

void cu_safe_restore (void);

void doG7Fault(bool allowTR) NO_RETURN;

extern const _fault_subtype fst_zero;
extern const _fault_subtype fst_acv9;
extern const _fault_subtype fst_acv15;
extern const _fault_subtype fst_ill_mod;
extern const _fault_subtype fst_ill_proc;
extern const _fault_subtype fst_ill_dig;
extern const _fault_subtype fst_ill_op;
extern const _fault_subtype fst_str_oob;
extern const _fault_subtype fst_str_nea;
extern const _fault_subtype fst_str_ptr;
extern const _fault_subtype fst_cmd_lprpn;
extern const _fault_subtype fst_cmd_ctl;
extern const _fault_subtype fst_onc_nem;
 
void doFault (_fault faultNumber, _fault_subtype faultSubtype, 
              const char * faultMsg) NO_RETURN;
void dlyDoFault (_fault faultNumber, _fault_subtype subFault, 
                const char * faultMsg);
bool bG7PendingNoTRO (void);
bool bG7Pending (void);
void setG7fault (uint cpuNo, _fault faultNo, _fault_subtype subFault);
//void doG7Fault (void);
void clearTROFault (void);
void advanceG7Faults (void);
#ifdef L68
void set_FFV_fault (uint f_fault_no);
void do_FFV_fault (uint fault_number, const char * fault_msg);
#endif


