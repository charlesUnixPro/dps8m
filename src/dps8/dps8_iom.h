extern DEVICE iom_dev;
// I/O Multiplexer
enum { max_channels = 32 }; // enums are more constant than consts...

//-- // Used to communicate between the IOM and devices
//-- typedef struct {
//--     int iom_unit_num;
//--     int chan;
//--     void* statep; // For use by device specific code
//--     int dev_cmd; // 6 bits
//--     // XXX Caution; this is the unit number from the IOM, it is NOT the
//--     // same as unit number. E.g.
//--     //    dev_code       simh
//--     //      0            cardreader[0]
//--     //      1            tape[0]
//--     //      2            tape[1]
//--     //      3            disk[0]
//-- 
//--     int dev_code; // 6 bits
//--     int chan_data; // 6 bits; often some sort of count
//--     bool have_status; // set to true by the device when operation is complete
//--     int major;
//--     int substatus;
//--     bool is_read;
//--     int time; // request by device for queuing via sim_activate()
//-- } chan_devinfo;

typedef struct pcw_s
  {
    uint dev_cmd;    // 6 bits; 0..5
    uint dev_code;   // 6 bits; 6..11
    uint ext;        // 6 bits; 12..17; address extension
    uint cp;         // 3 bits; 18..20, must be all ones
    uint mask;    // extension control or mask; 1 bit; bit 21
    uint control;    // 2 bits; bit 22..23
    uint chan_cmd;   // 6 bits; bit 24..29;
    // AN87 says: 00 single record xfer, 02 non data xfer,
    // 06 multi-record xfer, 10 single char record xfer
    uint chan_data;  // 6 bits; bit 30..35; often some sort of count
    //
    uint chan;       // 6 bits; bits 3..8 of word 2
  } pcw_t;

enum chan_type { chan_type_CPI, chan_type_PSI };
typedef enum chan_type chan_type;

// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
enum dev_type { DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, DEVT_MPC };

typedef int iom_cmd (UNIT * unitp, pcw_t * p, word12 * stati, bool * need_data, bool * is_read);
typedef int iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati);
t_stat cable_to_iom (int iom_unit_num, int chan_num, int dev_code, enum dev_type dev_type, chan_type ctype, int dev_unit_num, DEVICE * devp, UNIT * unitp, iom_cmd * iom_cmd, iom_io * iom_io);
void iom_init(void);
void iom_interrupt(int iom_unit_num);
t_stat channel_svc(UNIT *up);
t_stat cable_iom (int iom_unit_num, int iom_port_num, int scu_unit_num, int scu_port_num);

