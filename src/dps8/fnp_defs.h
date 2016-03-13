//
//  mux_defs.h
//  fnp
//
//  Created by Harry Reed on 11/26/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.

//  derived from nova_defs.h: NOVA/Eclipse simulator definitions
//  Copyright (c) 1993-2012, Robert M. Supnik
//

#ifndef mux_mux_defs_h
#define mux_mux_defs_h

#include "dps8_simh.h"
#include "sim_tmxr.h"

#include "fnp_rev.h"

#define STR(s) #s

#define MAX_LINES     64                                /*  max number of FNP lines - hardware  */

#define SCP_QUIT        SCPC_OK + 1

#define MUX_S_RI        0x8000                          /*  Receiver Interrupt          */
#define MUX_S_TI        0x4000                          /*  Transmitter interrupt       */
#define MUX_S_LMASK     0x3F00                          /*  line mask                   */
#define MUX_S_DMASK     0x00FF                          /*  data mask (received char)   */

#define MUX_L_RXE       0x800000                        /*  receiver enabled?           */
#define MUX_L_RXBZ      0x400000                        /*  receiver busy?              */
#define MUX_L_RXDN      0x200000                        /*  receiver done?              */
#define MUX_L_TXE       0x080000                        /*  transmitter enabled?        */
#define MUX_L_TXBZ      0x040000                        /*  transmitter busy?           */
#define MUX_L_TXDN      0x020000                        /*  transmitter done?           */

#define MUX_L_BREAK     0x008000                        /*  BREAK character received    */
#define MUX_L_RING      0x004000                        /*  Ring interrupt              */
#define MUX_L_CD        0x002000                        /*  Carrier Detect              */
#define MUX_L_DTR       0x001000                        /*  Data Terminal Ready         */
/*  <0x00FF = character>        */

#define MUX_L_LOOPBK    0x00010000                      /*  loopback mode               */
#define MUX_L_OVRERR    0x00020000                      /*  overrun error               */
#define MUX_L_FRMERR    0x00040000                      /*  framing error               */
#define MUX_L_PARERR    0x00080000                      /*  parity error                */


/* CD, CTS, DSR, RI */
/*  <future>  */

#define MUX_L_MODEM     0x0080                          /*  <not yet used>      */
#define MUX_L_TELNET    0x0040                          /*  <not yet used>      */
#define MUX_L_AUTODIS   0x0020                          /*  <not yet used>      */
#define MUX_L_PARITY
#define MUX_L_7BIT
#define MUX_L_BAUD                                      /*  <4 bits>            */

#define MUX_L_DMASK     0x000FF                         /*  data mask (always 8 bits)   */


#define UNIT_V_8B   (UNIT_V_UF + 0)                     /* 8b output */
#define UNIT_8B     (1 << UNIT_V_8B)

#define MUX_MASTER_ACTIVE( desc )   ( (desc)->master )

#define MUX_LINE_EXTRACT( x )       (((x) & MUX_S_LMASK) >> 8)
#define MUX_SECT_EXTRACT( x )       ((x) & 0x0001)

#define MUX_LINE_TX_CHAR( line )    mux_tx_chr[ ((line) % MAX_LINES) ]
#define MUX_LINE_RX_CHAR( line )    (mux_status[ (line) ] & MUX_S_DMASK)
#define MUX_UNIT_ACTIVE( unitp )    ( (unitp)->conn )

#define MUX_LINE_BITS( line, bits ) (mux_status[ (line) ] & bits)

#define MUX_LINE_SET_BIT(   line, bit )  mux_status[ (line) ] |=  (bit)  ;
#define MUX_LINE_CLEAR_BIT( line, bit )  mux_status[ (line) ] &= ~(bit)  ;
#define MUX_LINE_BIT_SET( line, bit )   (mux_status[ (line) ] &   (bit))


#define MUX_SET_BUSY( x )    mux_busy = mux_busy |   (x)
#define MUX_CLR_BUSY( x )    mux_busy = mux_busy & (~(x))
#define MUX_SET_DONE( x )    mux_done = mux_done |   (x)
#define MUX_CLR_DONE( x )    mux_done = mux_done & (~(x))
#define MUX_UPDATE_INTR      mux_int_req = (mux_int_req & ~MUX_INT_CLK) | (mux_done & ~mux_disable)

#define MUX_IS_BUSY( x )    (mux_busy & (x))
#define MUX_IS_DONE( x )    (mux_done & (x))

#define MUX_INT_V_CLK       7                               /* clock */
#define MUX_INT_CLK         (1 << MUX_INT_V_CLK)

#define IOT_V_REASON    16                              /* set reason */
#define IORETURN(f,v)   ((f)? (v): SCPE_OK)             /* stop on error */


#define MUX_RETURN( status, data )   (int32)(((status) << IOT_V_REASON) | ((data) & 0x0FFFF) )

// for fnp() ...

enum fnpOP
{
    NOP = 0,    /* <no operation>  */
    RES,        /* read line and section requesting service  */
    SLS,        /* set line and section  */
    RBN,        /* return bus noise (from nova) */
    OMC,        /* output and modem control functions  */
    TXD,        /* transmit data  */
    BRK,        /* transmit break  */
    SMCS,       /*  set modem control status  */
    GMRS,       /*  get modem or receiver status  */
    SLA,        /*  set line attributes  */
    SDB,        /*  set device busy */
    CLEAR,
    SELECT,
    START,

    STCLK = START,      // clock start
    CLCLK = CLEAR,      // clock clear

    STTTO = START,
    CLTTO = CLEAR,

    STTTI = START,
    CLTTI = CLEAR,

    READ,
    WRITE,
    
    TTOWR = WRITE,
    TTIRD = READ,
};


#define DEV_TTI         010                             /* console input */
#define DEV_TTO         011                             /* console output */

#define INT_V_TTI       11                              /* keyboard */
#define INT_V_TTO       12                              /* terminal */
#define INT_V_TTI1      13                              /* second keyboard */
#define INT_V_TTO1      14                              /* second terminal */

#define INT_TTI         (1 << INT_V_TTI)
#define INT_TTO         (1 << INT_V_TTO)
#define INT_TTI1        (1 << INT_V_TTI1)
#define INT_TTO1        (1 << INT_V_TTO1)

#define DEV_FNPMUX         034                             /* ALM/ULM multiplexor */
#define FNP_INT_V_MUX       16                              /* ALM multiplexor */
#define FNP_INT_MUX         (1 << FNP_INT_V_MUX)
#define PI_MUX          0000002
#define PI_CLK          0000004

#define INT_V_STK       17                              /* stack overflow */
#define INT_DEV         ((1 << INT_V_STK) - 1)          /* device ints */

#define PI_TTI          0000002
#define PI_TTO          0000001

typedef struct {
    int32       dnum;                                   /* device number */
    int32       mask;                                   /* done/busy mask */
    int32       pi;                                     /* assigned pi bit */
    int32       (*routine)(int32, int32, int32);        /* dispatch routine */
} DIB;


extern UNIT mux_unit;
extern DEVICE clk_dev, mux_dev, tti_dev, tto_dev, ipc_dev;

int mux_update_status( DIB * dibp, TMXR * tmxr_desc );
int mux_tmxr_putc( int line, TMLN * lp, int kar );

t_stat mux_svc(UNIT * unitp );
t_stat mux_clear( t_bool flag );
t_stat mux_setnl( UNIT * uptr, int32 val, char * cptr, void * desc );
t_stat mux_reset(DEVICE *dptr);

int32 clk (int32 pulse, int32 code, int32 AC);
int32 mux( int32 pulse, int32 code, int32 AC );

t_stat OnMuxConnect(TMLN *m, int line);
t_stat OnMuxDisconnect(int line, int kar);
t_stat OnMuxRx(TMXR *tmxr, TMLN *tmln, int line, int kar);
t_stat OnMuxRxBreak(int line, int kar);
t_stat OnMuxStalled(int line, int kar);

t_stat MuxWrite(int line, int kar);

int32 tto (int32 pulse, int32 code, int32 AC);
int32 tti (int32 pulse, int32 code, int32 AC);

t_stat OnTTI(int iodata);


extern int32 mux_tmxr_poll;                                /* tmxr poll */
extern int32 mux_int_req, mux_busy, mux_done, mux_disable;
extern int32 mux_chars_Rx;   //saved_PC;

typedef struct
  {
    t_bool accept_calls;
    struct
      {
        int muxLineNum;

        bool isSlave;
        bool isfTCP;

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

        // fTCP buffers
        struct fTCP_t
          {
            char inbuf [4097]; // should be big enough for a encoded MTU?
            int nbytes; // Number of chars in inbuf.
            int portno;
            int sockfd;
            struct hostent * server;
            struct sockaddr_in serv_addr;
          } fTCP;
      } line [MAX_LINES];
  } t_MState;
extern t_MState MState;

#endif
