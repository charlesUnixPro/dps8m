typedef struct pcw_t
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

// Much of this is from AN87 as 43A23985 lacked details of 0..11 and 22..36

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
 
// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
enum dev_type { DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, DEVT_MPC, DEVT_DN355 };
typedef enum chan_type { chan_type_CPI, chan_type_PSI } chan_type;

typedef int iom_cmd (UNIT * unitp, pcw_t * p);
extern DEVICE iom_dev;

void iom_interrupt (int iom_unit_num);
void iom_init (void);
t_stat cable_iom (int iom_unit_num, int iom_port_num, int scu_unit_num, int scu_port_num);
t_stat cable_to_iom (int iom_unit_num, int chan_num, int dev_code, enum dev_type dev_type, chan_type ctype, int dev_unit_num, DEVICE * devp, UNIT * unitp, iom_cmd * iom_cmd);
int iomListService (int iom_unit_num, int chan_num, dcw_t * dcwp, word24 * dcw_addr);
uint mbx_loc (int iom_unit_num, uint chan_num);
void fetch_and_parse_lpw (lpw_t * p, uint addr, bool is_conn);
int status_service(int iom_unit_num, uint chan, uint dev_code, word12 stati, word6 rcount, word12 residue, word3 char_pos, bool is_read);
int send_terminate_interrupt (int iom_unit_num, uint chan);
void decode_idcw (int iom_unit_num, pcw_t *p, bool is_pcw, 
                  word36 word0, word36 word1);
void fetch_abs_pair (word24 addr, word36 * even, word36 * odd);
int iomListServiceNoParse (int iom_unit_num, int chan_num, word36 * wp);
void fetch_abs_word (word24 addr, word36 *data);
void store_abs_word (word24 addr, word36 data);
