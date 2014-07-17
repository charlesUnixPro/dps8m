// Appending unit stuff .......

// Once segno and offset are formed in TPR.SNR and TPR.CA, respectively, the
// process of generating the 24-bit absolute main memory address can involve a
// number of different and distinct appending unit cycles.
//
// The operation of the appending unit is shown in the flowchart in Figure 5-4.
// This flowchart assumes that directed faults, store faults, and parity faults
// do not occur.
//
// A segment boundary check is made in every cycle except PSDW. If a boundary
// violation is detected, an access violation, out of segment bounds, fault is
// generated and the execution of the instruction interrupted. The occurrence
// of any fault interrupts the sequence at the point of occurrence. The
// operating system software should store the control unit data for possible
// later continuation and attempt to resolve the fault condition.
//
// The value of the associative memories may be seen in the flowchart by
// observing the number of appending unit cycles bypassed if an SDW or PTW is
// found in the associative memories.
//
// There are nine different appending unit cycles that involve accesses to main
// memory. Two of these (FANP, FAP) generate the 24-bit absolute main memory
// address and initiate a main memory access for the operand, indirect word, or
// instruction pair; five (NSDW, PSDW, PTW, PTW2, and DSPTW) generate a main
// memory access to fetch an SDW or PTW; and two (MDSPTW and MPTW) generate a
// main memory access to update page status bits (PTW.U and PTW.M) in a PTW.
// The cycles are defined in Table 5-1.

enum _appendingUnit_cycle_type {
    apuCycle_APPUNKNOWN = 0,    // unknown
    
    apuCycle_FANP,       // Final address nonpaged.
                // Generates the 24-bit absolute main memory address and 
                // initiates a main memory access to an unpaged segment for 
                // operands, indirect words, or instructions.

    apuCycle_FAP,        // Final address paged
                // Generates the 24-bit absolute main memory address and 
                // initiates a main memory access to a paged segment for 
                // operands, indirect words, or instructions.

    apuCycle_NSDW,       // Nonpaged SDW Fetch
                // Fetches an SDW from an unpaged descriptor segment.

    apuCycle_PSDW,       // Paged SDW Fetch
                // Fetches an SDW from a paged descriptor segment.

    apuCycle_PTWfetch,   // PTW fetch
                // Fetches a PTW from a page table other than a descriptor 
                // segment page table and sets the page accessed bit (PTW.U).

    apuCycle_PTW2,       // Prepage PTW fetch
                // Fetches the next PTW from a page table other than a 
                // descriptor segment page table during hardware prepaging for 
                // certain uninterruptible EIS instructions. This cycle does 
                // not load the next PTW into the appending unit. It merely 
                // assures that the PTW is not faulted (PTW.F = 1) and that 
                // the target page will be in main memory when and if needed 
                // by the instruction.

    apuCycle_DSPTW,      // Descriptor segment PTW fetch
                // Fetches a PTW from a descriptor segment page table.

    apuCycle_MDSPTW,     // Modify DSPTW
                // Sets the page accessed bit (PTW.U) in the PTW for a page 
                // in a descriptor segment page table. This cycle always 
                // immediately follows a DSPTW cycle.

    apuCycle_MPTW        // Modify PTW
                // Sets the page modified bit (PTW.M) in the PTW for a page 
                // in other than a descriptor segment page table.
};


typedef enum apuStatusBits
  {
    apuStatus_PI_AP,
    apuStatus_DSPTW,
    apuStatus_SDWNP,
    apuStatus_SDWP,
    apuStatus_PTW,
    apuStatus_PTW2,
    apuStatus_FAP,
    apuStatus_FANP,
    apuStatus_FABS,
    apuStatus_MDSPTW,
    apuStatus_MPTW,
  } apuStatusBits;

void setAPUStatus (apuStatusBits status);

void doPtrReg (void);        ///< used by EIS stuff
t_stat dumpSDWAM (void);
void acvFault(_fault_subtype acvfault, char * msg) NO_RETURN;
word24 doAppendCycle(word18 address, _processor_cycle_type thisCycle);
void do_ldbr (word36 * Ypair);
void do_sdbr (word36 * Ypair);
void do_camp (word36 Y);
void do_cams (word36 Y);



