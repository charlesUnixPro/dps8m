#include <uv.h>
#include <libtelnet.h>

#define encodeline(fnp,line) ((fnp) * MAX_LINES + (line))
#define decodefnp(coded) ((coded) / MAX_LINES)
#define decodeline(coded) ((coded) % MAX_LINES)
#define noassoc -1

extern UNIT fnp_unit [N_FNP_UNITS_MAX];
extern DEVICE fnpDev;
extern DEVICE mux_dev;

#define MAX_LINES  64  /*  max number of FNP lines - hardware  */

//
// MState_t state of an FNP
// 

// memset(0) sets service to serivce_undefined (0)
enum service_types {service_undefined = 0, service_login, service_autocall, service_slave};

typedef struct
  {
    t_bool accept_calls;
    struct t_line
      {
        // From the CMF database
        enum service_types service;

        // libuv hook
        uv_tcp_t * client;

        // libtelnet hook
        telnet_t * telnetp; // telnet_t *

        // State as set by FNP commands
        t_bool listen;
        int inputBufferSize;
        int ctrlStrIdx;
        t_bool breakAll;
        t_bool can;         // performs standard canonicalization when on (default on)
        t_bool capo;        // outputs all lowercase chars in uppercase
        t_bool ctl_char;    // specifies that ASCII control characters that do not cause carriage or paper motion are to be accepted as input
        t_bool _default;    // same as saying erkl, can, ^rawi, *rawc, ^Wake_tbl, and esc
        t_bool handleQuit;
        t_bool fullDuplex;
        t_bool echoPlex;    // echoes all characters types on the terminal
        t_bool erkl;        // performs "erase" and "kill" processing
        t_bool esc;         // performs escape processing
        t_bool crecho;      // echos a CR when a LF is typed
        t_bool lfecho;      // echos and inserts  a LF in the users input stream when a CR is typed
        t_bool tabecho;     // echos the appropriate number of spaces when a TAB is typed
        t_bool tabs;        // inserts tabs in output in place of spaces when appropriate. If tabs mode is off, all tab characters are mapped into the appropriate number of spaces
        t_bool replay;
        t_bool polite;
        t_bool prefixnl;
        t_bool eight_bit_out;
        t_bool eight_bit_in;
        t_bool odd_parity;
        t_bool output_flow_control;
        t_bool input_flow_control;
        int block_xfer_in_frame, block_xfer_out_of_frame;
        int delay_table [6];
#define FC_STR_SZ 4
        int inputSuspendLen;
        char inputSuspendStr [4];
        int inputResumeLen;
        char inputResumeStr [4];
        int outputSuspendLen;
        char outputSuspendStr [4];
        int outputResumeLen;
        char outputResumeStr [4];
        int frame_begin;
        int frame_end;
        bool echnego [256];
        uint echnego_len;

        // Pending requests
        bool line_break;
        bool send_output;
        bool accept_new_terminal;
        bool line_disconnected;
        bool ack_echnego_init;
        bool acu_dial_failure;
        bool wru_timeout;
        uint accept_input; // If non-zero, the number of centiseconds until
                          // an accept_input message should be sent; this is
                          // deal with 'reject_request' retries.
        bool input_reply_pending;
        // Part of 'accent_input'
        bool input_break;

        // Buffer being assembled for sending to Multics
        char buffer[1024];   // line buffer for initial device selection and line discipline
        int nPos;           // position where *next* user input is to be stored

        // Incoming data from the connection
        char * inBuffer;
        int inSize; // Number of bytes in inBuffer
        int inUsed; // Number of consumed bytes in buffer


        // Dialout/slave hooks
        uv_tcp_t doSocket;
        uv_connect_t doConnect; 

        // Slave hooks
        int port;

      } line [MAX_LINES];
  } t_MState;

#define MAX_DEV_NAME_LEN 64

// Indexed by sim unit number
struct fnpUnitData
  {
//-    enum { no_mode, read_mode, write_mode, survey_mode } io_mode;
//-    uint8 * bufp;
//-    t_mtrlnt tbc; // Number of bytes read into buffer
//-    uint words_processed; // Number of Word36 processed from the buffer
//-    int rec_num; // track tape position
    uint mailboxAddress;
    bool fnpIsRunning;
    bool fnpMBXinUse [4];  // 4 FNP submailboxes
    int fnpMBXlineno [4]; // Which HSLA line is using the mbx
    char ipcName [MAX_DEV_NAME_LEN];

    t_MState MState;
  };

extern struct fnpUnitData fnpUnitData [N_FNP_UNITS_MAX];

void fnpInit(void);
int lookupFnpsIomUnitNumber (int fnpUnitNum);
int lookupFnpLink (int fnpUnitNum);
void fnpProcessEvent (void); 
t_stat diaCommand (int fnpUnitNum, char *arg3);
void fnpToCpuQueueMsg (int fnpUnitNum, char * msg);
int fnpIOMCmd (uint iomUnitIdx, uint chan);
t_stat fnpServerPort (int32 arg, char * buf);
void fnpConnectPrompt (uv_tcp_t * client);
void processUserInput (uv_tcp_t * client, char * buf, ssize_t nread);
void processLineInput (uv_tcp_t * client, char * buf, ssize_t nread);
