#ifndef DPS8_EM_CONSTS_H
#define DPS8_EM_CONSTS_H



//
// Memory
//

enum { MEMSIZE = MEM_SIZE_MAX };

//
// Card reader
//

enum { N_CRDRDR_UNITS_MAX = 16 };

//
// FNP
//

enum { N_FNP_UNITS_MAX = 16 };

//
// Disk
//

enum { N_DISK_UNITS_MAX = 17 };

//
// SCU
//

enum { N_SCU_UNITS_MAX = 4 };
enum { N_SCU_PORTS = 8 };
enum { N_ASSIGNMENTS = 2 };
// Number of interrupts in an interrupt cell register
enum { N_CELL_INTERRUPTS = 32 };

//
// Operator console
//

enum { N_OPCON_UNITS_MAX = 1 };

//
// IOM
//

enum { N_IOM_UNITS_MAX = 4 };
enum { MAX_CHANNELS = 64 };
enum { N_IOM_PORTS = 8 };
// The number of devices that a dev_code can address (6 bit number)
enum { N_DEV_CODES = 64 };


typedef struct pcw_t
  {
    // Word 1
    uint dev_cmd;    // 6 bits; 0..5
    uint dev_code;   // 6 bits; 6..11
    uint ext;        // 6 bits; 12..17; address extension
    uint cp;         // 3 bits; 18..20, must be all ones

// From iom_control.alm:
//  " At this point we would normally set idcw.ext_ctl.  This would allow IOM's
//  " to transfer to DCW lists which do not reside in the low 256K.
//  " Unfortunately, the PSIA does not handle this bit properly.
//  " As a result, we do not set the bit and put a kludge in pc_abs so that
//  " contiguous I/O buffers are always in the low 256K.
//  "
//  " lda       =o040000,dl         " set extension control in IDCW
//  " orsa      ab|0

    uint mask;    // extension control or mask; 1 bit; bit 21
    uint control;    // 2 bits; bit 22..23
    uint chan_cmd;   // 6 bits; bit 24..29;
    // AN87 says: 00 single record xfer, 02 non data xfer,
    // 06 multi-record xfer, 10 single char record xfer
    uint chan_data;  // 6 bits; bit 30..35; often some sort of count
    //

    // Word 2
    uint chan;       // 6 bits; bits 3..8 of word 2
    uint ptPtr;        // 18 bits; bits 9..26 of word 2
    uint ptp;    // 1 bit; bit 27 of word 2 
    uint pcw64_pge;    // 1 bit; bit 28 of word 2
    uint aux;    // 1 bit; bit 29 of word 2
  } pcw_t;

//
// CPU
//

enum { N_CPU_PORTS = 4 };

//
// Tape drive
//

// Survey devices only has 16 slots, so 16 drives plus the controller
enum { N_MT_UNITS_MAX = 17 };

#endif // DPS8_EM_CONSTS_H

