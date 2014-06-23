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

typedef struct
  {
    uint32 dcw_ptr;     // bits 0..17
    word1 ires;    // bit 18; IDCW restrict
    word1 hrel;    // bit 19; hardware relative addressing
    word1 ae;      // bit 20; address extension
    word1 nc;      // bit 21; no tally; zero means update tally
    word1 trunout; // bit 22; signal tally runout?
    word1 srel;    // bit 23; software relative addressing; not for Multics!
    uint32 tally;    // bits 24..35
    // following not valid for paged mode; see B15; but maybe IOM-B non existant
    uint32 lbnd;
    uint32 size;
    uint32 idcw;    // ptr to most recent dcw, idcw, ...
  } lpw_t;
 
typedef struct dcw_t
  {
    enum { ddcw, tdcw, idcw } type;
    union
      {
        pcw_t instr;
        struct
          {
            uint daddr; // data address; 18 bits at 0..17);
            uint cp;    // char position; 3 bits 18..20
            uint tctl;  // tally control; 1 bit at 21
            uint type;  // 2 bits at 22..23
            uint tally; // 12 bits at 24..35
          } ddcw;
        struct {
            uint addr;
            bool ec;  // extension control
            bool i;   // IDCW control
            bool r;   // relative addressing control
          } xfer;
      } fields;
  } dcw_t;

enum chan_type { chan_type_CPI, chan_type_PSI };
typedef enum chan_type chan_type;

// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
enum dev_type { DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, DEVT_MPC };

int send_terminate_interrupt(int iom_unit_num, uint chan);
char * dcw2text (const dcw_t * p);
void fetch_and_parse_dcw (int iom_unit_num, uint chan, dcw_t *p, uint32 addr, int read_only);
char * lpw2text (const lpw_t * p, int conn);
void iom_fault (int iom_unit_num, uint chan, const char * who, int is_sys, int signal);
void fetch_and_parse_lpw (lpw_t * p, uint addr, bool is_conn);
typedef int iom3_cmd (int iom_unit_num, UNIT * unitp, pcw_t * p, 
                      uint mbx_addr, word12 * stati,
                      word6 * rcount, word12 * residue, word3 * char_pos,
                      word1 * is_read);
typedef int iom_cmd (UNIT * unitp, pcw_t * p, word12 * stati, bool * need_data, bool * is_read);
typedef int iom_io (UNIT * unitp, uint chan, uint dev_code, uint * tally, uint * cp, word36 * wordp, word12 * stati);
t_stat cable_to_iom (int iom_unit_num, int chan_num, int dev_code, enum dev_type dev_type, chan_type ctype, int dev_unit_num, DEVICE * devp, UNIT * unitp, iom_cmd * iom_cmd, iom_io * iom_io, iom3_cmd * iom3_cmd);
void iom_init(void);
void iom_interrupt(int iom_unit_num);
t_stat iom_svc(UNIT* up);
t_stat channel_svc(UNIT *up);
t_stat cable_iom (int iom_unit_num, int iom_port_num, int scu_unit_num, int scu_port_num);

