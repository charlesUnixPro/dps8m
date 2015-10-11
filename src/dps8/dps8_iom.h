typedef enum 
  {
    cm_LPW_init_state, // No TDCWs encountered; state is:
                       //    PCW64 (pcw64_pge): on   PAGE CHAN
                       //    PCW64 (pcw64_pge): off  EXT MODE CHAN
    cm_real_LPW_real_DCW,
    cm_ext_LPW_real_DCW,
    cm_paged_LPW_seg_DCW
  } chanMode_t;

typedef enum chanStat
  {
    chanStatNormal = 0,
    chanStatUnexpectedPCW = 1,
    chanStatInvalidInstrPCW = 2,
    chanStatIncorrectDCW = 3,
    chanStatIncomplete = 4,
    chanStatUnassigned = 5,
    chanStatParityErrPeriph = 6,
    chanStatParityErrBus = 7
  } chanStat;

typedef struct
  {

// scratch pad

    // packed LPW
    word36 LPW; 
    // unpacked LPW
    word18 LPW_DCW_PTR;
    word1 LPW_18_RES;
    word1 LPW_19_REL;
    word1 LPW_20_AE;
    word1 LPW_21_NC;
    word1 LPW_22_TAL;
    word1 LPW_23_REL;
    word12 LPW_TALLY;

    // packed LPWX
    word36 LPWX;
    // unpacked LPWX
    word9 LPWX_BOUND; // MOD 2 (pg B16) 0-2^19; ie val = LPX_BOUND * 2
    word9 LPWX_SIZE;  // MOD 1 (pg B16) 0-2^18


    // packed PCW
    word36 PCW0, PCW1;
    // unpacked PCW
    word6 PCW_CHAN;
    word6 PCW_AE;
    word18 PCW_PAGE_TABLE_PTR;
    word1 PCW_PTP;
    word1 PCW_PGE;
    word1 PCW_AUX;
    word1 PCW_M; // XXX see 3.2.2, pg 25

    // packed DCW
    word36 DCW;
    // unpacked DCW
    // TDCW only
    word18 TDCW_DATA_ADDRESS; 
    word1  TDCW_34_RES;
    word1  TDCW_35_REL;
    // TDCW, PCW 64 = 0
    word1  TDCW_33_EC;
    // TDCW, PCW 64 = 1
    word1  TDCW_31_SEG;
    word1  TDCW_32_PDTA;
    word1  TDCW_33_PDCW;
    // IDCW only
    word6  IDCW_DEV_CMD;
    word6  IDCW_DEV_CODE;
    word2  IDCW_CONTROL;
    // DDCW only
    word18 DDCW_ADDR;
    word12 DDCW_TALLY;
    word2  DDCW_22_23_TYPE; // '2' indicates TDCW
    // xDCW
    word3  DCW_18_20_CP; // '7' indicates IDCW

    word36 PTW_DCW;  // pg B8.
    word36 PTW_LPW;  // pg B6.

    chanMode_t chanMode;

    // Information accumulated for status service.
    word12 stati;
    uint dev_code;
    word6 recordResidue;
    word12 tallyResidue;
    word3 charPos;
    bool isRead;
    bool isOdd;
    bool initiate;

    chanStat chanStatus;

    bool lsFirst;

  } iomChanData_t;
extern iomChanData_t iomChanData [N_IOM_UNITS_MAX] [MAX_CHANNELS];
extern DEVICE iom_dev;

// Indirect data service data type
typedef enum 
  {   
    idsTypeW36  // Incoming data is array of word36 
  } idsType;


int send_special_interrupt (uint iomUnitIdx, uint chanNum, uint devCode, 
                            word8 status0, word8 status1);
//
// iomCmd returns:
//
//  0: ok
//  1; ignored cmd, drop connect.
//  2: did command, don't do DCW list
//  3; command pending, don't sent terminate interrupt
// -1: error

typedef int iomCmd (uint iomUnitIdx, uint chan);
void indirectDataService (uint iomUnitNum, int chanNum, uint daddr, uint tally, 
                          void * data, idsType type, bool write, bool * odd);
int iomListService (uint iomUnitIdx, uint chan,
                           bool * ptro, bool * sendp, bool * uffp);
int send_terminate_interrupt (uint iomUnitIdx, uint chanNum);
