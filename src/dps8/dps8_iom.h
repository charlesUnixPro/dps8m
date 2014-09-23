typedef struct pcw_t
  {
    // Word 1
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

    // Word 2
    uint chan;       // 6 bits; bits 3..8 of word 2
    uint ptPtr;        // 18 bits; bits 9..26 of word 2
    uint ptp;    // 1 bit; bit 27 of word 2 uint pge;    // 1 bit; bit 28 of word 2
    uint pge;    // 1 bit; bit 28 of word 2
    uint aux;    // 1 bit; bit 29 of word 2
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
            // seg and pdta valid if paged and pcw 64 == 1
            bool seg; // 1 bit at 31
            bool pdta; // 1 bit at 32
            // 'pdcw' if paged and pcw 64 == 0; otherwise 'ec'
            bool pdcw;  // extension control; ec 1 bit at 33
            bool res;   // IDCW control; res 1 bit at 34
            bool rel;   // relative addressing control; 1 bit at 35
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
    // Using Paged mode LPWx
    uint32 lbnd;
    uint32 size;
    // Olny in GCOS, GCOS ext.
    // uint32 idcw;    // ptr to most recent dcw, idcw, ...
  } lpw_t;
 
// This is the 'scratchpad'

typedef struct iomChannelData_
  {
    bool firstList;
    pcw_t pcw; // The pcw at the time of the CIOC.
    word6 addressExtension;
    word18 ptPtr;
    word1 ptp;
    word1 pge;
    word1 aux;
    lpw_t lpw;
    word36 PTW_LPW; // DCW List page table word
    word36 PTW_DCW;  // Data page table word
#if 0
    enum { cm_REAL_LPW_REAL_DCW_1A,
           cm_EXT_LPW_REAL_DCW_1B,
           cm_REAL_LPW_PAGED_DCW_2,
           cm_REAL_LPW_SEG_DCW_3A,
           cm_PAGED_LPW_PAGED_DCW_3B,
           cm_PAGED_LPW_SEG_DCW_4,
           cm_SEG_LPW_SED_DCW_5 } chan_mode;
#endif
    enum 
      {
        cm_LPW_init_state, // No TDCWs encountered; state is:
                           //    PCW64 (pge): on   PAGE CHAN
                           //    PCW64 (pge): off  EXT MODE CHAN
        cm_real_LPW_real_DCW,
        cm_ext_LPW_real_DCW,
        cm_paged_LPW_seg_DCW
      } chan_mode;
  } iomChannelData_;

iomChannelData_ iomChannelData [N_IOM_UNITS_MAX] [MAX_CHANNELS];

// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
enum dev_type { DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, DEVT_MPC, DEVT_DN355 };
typedef enum chan_type { chan_type_CPI, chan_type_PSI } chan_type;

typedef int iomCmd (UNIT * unitp, pcw_t * p);
extern DEVICE iom_dev;

void iom_interrupt (uint iomUnitNum);
void iom_init (void);
t_stat cable_iom (uint iomUnitNum, int iomPortNum, int scuUnitNum, int scuPortNum);
t_stat cable_to_iom (uint iomUnitNum, int chanNum, int dev_code, enum dev_type dev_type, chan_type ctype, uint devUnitNum, DEVICE * devp, UNIT * unitp, iomCmd * iomCmd);
int iomListService (uint iomUnitNum, int chanNum, dcw_t * dcwp, int * ptro);
int iomListServiceTape (uint iomUnitNum, int chanNum, dcw_t * dcwp, bool * ptro);
uint mbx_loc (uint iomUnitNum, uint chanNum);
//void fetch_and_parse_lpw (lpw_t * p, uint addr, bool is_conn);
int status_service(uint iomUnitNum, uint chan, uint dev_code, word12 stati, word6 rcount, word12 residue, word3 char_pos, bool is_read);
int send_terminate_interrupt (uint iomUnitNum, uint chan);
void decode_idcw (uint iomUnitNum, pcw_t *p, bool is_pcw, 
                  word36 word0, word36 word1);
void fetch_abs_pair (word24 addr, word36 * even, word36 * odd);
int iomListServiceNoParse (uint iomUnitNum, int chanNum, word36 * wp);
void fetch_abs_word (word24 addr, word36 *data);
void store_abs_word (word24 addr, word36 data);
