//
//  fnp_mux.c
//  fnp
//
//  Created by Harry Reed on 11/26/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//
// derived from:
//  nova_qty.c: NOVA multiplexor (QTY/ALM) simulator
//  Copyright (c) 2000-2008, Robert M. Supnik
//  Written by Bruce Ray and used with his gracious permission.
//


#include "dps8.h"
#include "dps8_utils.h"

#include "sim_tmxr.h"

#include "fnp_defs.h"
#include "fnp_mux.h"
#include "fnp_2.h"

int32   mux_chars_Rx = 0;

static int32   mux_brkio   = SCPE_OK ;                         /*  default I/O status code     */
int32   mux_max     = MAX_LINES ;                         /*  max # QTY lines - user      */
                                                        /*  controllable                */
static int32   mux_mdm     = 0 ;                               /*  QTY modem control active?   */
static int32   mux_auto    = 0 ;                               /*  QTY auto disconnect active? */
static int32   mux_polls   = 0 ;                               /*  total 'qty_svc' polls       */

TMLN    mux_ldsc[ MAX_LINES ] = {                       /*  QTY line descriptors        */
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL },
   { 0, 0, NULL, 0, NULL, 0, 0, 0, 0, 0, 0, false, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, NULL, NULL, NULL, "", "", NULL, NULL, NULL, 0, false, 0, NULL, NULL, NULL }
 }; 
TMXR    mux_desc    = { MAX_LINES, NULL, 0, mux_ldsc, NULL, NULL, NULL, "", 0, 0, 0, 0, false, false } ;     /*  mux descriptor      */
static int32   mux_status[ MAX_LINES ] = { 0 } ;                 /*  QTY line status             */
                                                        /*  (must be at least 32 bits)  */
static int32   mux_tx_chr[ MAX_LINES ] = { 0 } ;                 /*  QTY line output character   */

static int     mux_section     = -1 ;               /*  current line "section" (0 = RCV, 1 = XMT)  */
static int     mux_line        = -1 ;               /*  current line [0-63]                        */
//static int     mux_diag_mode   =  0 ;               /*  <not yet supported>                        */
static int     mux_line_mask   = 0x003F ;           /*  maximum of 64 lines in this rev            */

int32 mux_int_req, mux_busy, mux_done, mux_disable;

UNIT mux_unit =
{
      UDATA (&mux_svc, (UNIT_DISABLE + UNIT_ATTABLE + UNIT_IDLE), 0), 0, 0, 0, 0, 0, NULL, NULL
} ;

static DIB mux_dib = { DEV_FNPMUX, FNP_INT_MUX, PI_MUX, &mux } ;

int mux_busy, mux_done, mux_disable, mux_int_req;

int32 mux( int32 oper, int32 oper2, int32 AC )
{
    int32       iodata ;
    int32       ioresult ;
    TMLN *      tmlnp ;
    int     a ;
    int     kar ;
    
    /*--------------------------------------------------------------*/
    /*  DG 425x[-compatible] "ALM" multiplexor instruction handler  */
    /*--------------------------------------------------------------*/
    
    ioresult= mux_brkio ;   /*  (assume returning I/O break value   */
    iodata  = 0 ;           /*  (assume 16-bit Nova/Eclipse bus)    */
    switch ( oper )
    {
        case NOP :    /*  <no operation>  */
            break ;
            
        case RES :    /*  read line and section requesting service  */
            iodata = mux_update_status( &mux_dib, &mux_desc ) ;
            mux_line = (MUX_LINE_EXTRACT(iodata) & mux_line_mask) ;
            /*  (mask with 'alm_line_mask' in case ALM mask is different than QTY */
            mux_section = 0 ;
            if ( ! ( iodata & MUX_S_RI) )
                if ( iodata & MUX_S_TI )
                {
                    mux_section = 1 ;                           /*  receiver quiet - transmitter done  */
                }
            iodata = (mux_line << 1) | mux_section ;
            break ;
            
        case SLS :    /*  set line and section  */
            mux_section = MUX_SECT_EXTRACT( AC ) ;
            mux_line    = MUX_LINE_EXTRACT( AC ) ;
            break ;
            
        case RBN :    /*  no ALM function - return bus noise in AC  */
            if ( mux_line < mux_max )
            {
                iodata = MUX_LINE_RX_CHAR( mux_line ) ;
            }
            break ;
            
        case OMC :    /*  output and modem control functions  */
            switch (oper2)  //(AC >> 14) & 03 )
            {
                case TXD:   /*  transmit data  */
                    if ( mux_line < mux_max )
                        if ( MUX_LINE_BIT_SET(mux_line,MUX_L_TXE) )
                        {
                             //int drv = (int32) (uptr - mux_dev.units);                    /* get drive # */
                             //int drv = muxWhatUnitAttached();
                             //uptr = mux_dev.units + drv;                              /* get unit */
                            
                            /*
                             perform any character translation:
                             7 bit/ 8 bit
                             parity generation
                             */
                            kar = AC & ((mux_unit.flags & UNIT_8B)? 0377: 0177) ;
                            /*  do any parity calculations also  */
                        
                            tmlnp = &mux_ldsc[ mux_line ] ;
                            a = mux_tmxr_putc( mux_line, tmlnp, kar ) ;
                            if ( a != SCPE_OK)
                            {
                                /*  do anything at this point?  */
                            }
                            mux_update_status( &mux_dib, &mux_desc ) ;
                        }
                    break ;
                
                case BRK :   /*  transmit break  */
                    if ( mux_line < mux_max )
                        if ( MUX_LINE_BIT_SET(mux_line,MUX_L_TXE) )
                        {
                            tmlnp = &mux_ldsc[ mux_line ] ;
                            /*
                             a = qty_tmxr_putc( alm_line, tmlnp, kar ) ;
                             if ( a != SCPE_OK)
                             {
                             }
                             */
                            mux_update_status( &mux_dib, &mux_desc ) ;
                        }
                    break ;
                
                case SMCS :   /*  set modem control status  */
                    break ;
                
                case NOP :   /*  unused  */
                    break ;
            }
            break ;
            
        case GMRS :    /*  get modem or receiver status  */
            if ( mux_line < mux_max )
            {
                if ( mux_section )
                {
                    /*  get modem section status  */
                    if ( mux_ldsc[ mux_line ].xmte )
                    {
                        iodata = 0035 ;                         /*  set CD, CTS, DSR, MDM flags  */
                    }
                }
                else
                {
                    /*  get receiver section status  */
                    iodata = 0 ;                                /*  receiver error status - no errors by default  */
                }
            }
            break ;
            
        case SLA :    /*  set line attributes  */
            switch ( (AC >> 14) & 03 )
            {
                case 00 :   /*  control line section  */
                    break ;
                
                case 01 :   /*  unused  */
                    break ;
                
                case 02 :   /*  set line characteristics  */
                    break ;
                
                case 03 :   /*  unused  */
                    break ;
            }
            break ;
            //
            //        case ioSKP :    /*  I/O skip test - should never come here  */
            //            break ;
            //
            //        default :
            //            /*  <illegal I/O operation value>  */
            //            break ;
            //    }
            //
            //    switch ( pulse )
            //    {
            //        case iopN : /*  <ignored (of course)>  */
            //            break ;
            
        case SDB : /*  set device busy
                    *  set all lines on board offline
                    *  clear each line's done
                    *  clear internal system
                    *  clear device busy
                    */
            for ( a = 0 ; a < mux_max ; ++a )
                if ( 1 /* (not yet optimized) */ )
                {
                    MUX_LINE_CLEAR_BIT( a, (MUX_L_RXBZ | MUX_L_RXDN | MUX_L_TXBZ | MUX_L_TXDN) ) ;
                }
            mux_update_status( &mux_dib, &mux_desc ) ;
            break ;
            
            //case iopP : /*  stop clock for all boards in off-line mode  */
            //    break ;
            
        case CLEAR :
            for ( a = 0 ; a < mux_max ; ++a )
                if ( 1 /* (not yet optimized) */ )
                {
                    MUX_LINE_CLEAR_BIT( a, (MUX_L_RXBZ | MUX_L_RXDN | MUX_L_TXBZ | MUX_L_TXDN) ) ;
                }
            mux_update_status( &mux_dib, &mux_desc ) ;
            break ;
            
        default :
            /*  <illegal pulse value>  */
            break ;
    }
    
    return ( MUX_RETURN( ioresult, iodata ) ) ;
}


static REG mux_reg[] =  /* */
{
//    { ORDATA (PC, saved_PC, 32) },
//    { ORDATA (BUF, mux_unit.buf, 8) },
//    { FLDATA (BUSY, mux_busy, FNP_INT_V_MUX) },
//    { FLDATA (DONE, mux_done, FNP_INT_V_MUX) },
//    { FLDATA (DISABLE, mux_disable, FNP_INT_V_MUX) },
//    { FLDATA (INT, mux_int_req, FNP_INT_V_MUX) },
//    { FLDATA (MDMCTL, mux_mdm,  0) },
//    { FLDATA (AUTODS, mux_auto, 0) },
//    { DRDATA (POLLS, mux_polls, 32) },
    { DRDATAD (CHARRX, mux_chars_Rx, 32, "Characters received on Multiplexer"), 0, 0 },
#if defined (SIM_ASYNCH_IO)
    { DRDATA (LATENCY, sim_asynch_latency, 32), PV_LEFT },
    { DRDATA (INST_LATENCY, sim_asynch_inst_latency, 32), PV_LEFT },
#endif
    { NULL, 0, 0, 0, 0, 0, NULL, NULL, 0, 0 }
} ;

static MTAB mux_mod[] =
{
    { UNIT_8B, 0, "7b", "7B", NULL, NULL, NULL, NULL },
    { UNIT_8B, UNIT_8B, "8b", "8B", NULL, NULL, NULL, NULL },
    { MTAB_XTD | MTAB_VDV, 1, NULL, "DISCONNECT",
        &tmxr_dscln, NULL, (void *)&mux_desc, NULL },
    { UNIT_ATT, UNIT_ATT, "connections", NULL,
        NULL, &tmxr_show_summ, (void *)&mux_desc, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, 1, "CONNECTIONS", NULL,
        NULL, &tmxr_show_cstat, (void *)&mux_desc, NULL },
    { MTAB_XTD | MTAB_VDV | MTAB_NMO, 0, "STATISTICS", NULL,
        NULL, &tmxr_show_cstat, (void *)&mux_desc, NULL },
    { MTAB_XTD | MTAB_VDV, 0, "LINES", "LINES",
        &mux_setnl, &tmxr_show_lines, (void *) &mux_desc, NULL },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
} ;

static t_stat mux_attach(UNIT *unitp, char *cptr)
{
//    int muxU = muxWhatUnitAttached();
//    if (muxU != -1)
//        return SCPE_ALATT;  // a mux unit is already attached. Only can have 1 MUX unix per instance of simh
//    
//    muxU = (int32) (unitp - mux_dev.units);                    /* get mux # */
    //sim_printf("Multiplexor attached as %s\n", fnpName);    //s[muxU]);
    sim_printf("Multiplexor attached\n");    //s[muxU]);
    
    //uptr = mux_dev.units + drv;
    
    t_stat  r ;
    int a ;
    
    /*  switches:   A       auto-disconnect
     *              M       modem control
     */
    
    mux_mdm = mux_auto = 0;                             /* modem ctl off */
    r = tmxr_attach( &mux_desc, unitp, cptr ) ;         /* attach QTY */
    if ( r != SCPE_OK )
    {
        return ( r ) ;                                  /* error! */
    }
    if ( (unsigned int) sim_switches & SWMASK('M') )                   /* modem control? */
    {
        mux_mdm = 1;
        sim_printf( "Modem control activated\n" ) ;
        if ( (unsigned int) sim_switches & SWMASK ('A') )              /* autodisconnect? */
        {
            mux_auto = 1 ;
            sim_printf( "Auto disconnect activated\n" ) ;
        }
    }
    mux_polls = 0 ;
    for ( a = 0 ; a < MAX_LINES ; ++a )
    {
        /*  QTY lines are always enabled - force RX and TX to 'enabled' */
        mux_status[ a ] = (MUX_L_RXE | MUX_L_TXE) ;
    }
    sim_activate( unitp, mux_tmxr_poll ) ;
    
    //memset(ttys, 0, sizeof(ttys));  // resetttys structure
           
    return ( SCPE_OK ) ;
}

t_stat do_mux_attach (char * attstr)
  {
    return mux_attach (& mux_unit, attstr);
  }

static t_stat mux_detach( UNIT * unitp )
{
    //int muxU1 = muxWhatUnitAttached();              // what is attached
    //int muxU2 = (int32) (unitp - mux_dev.units);    // what wants to be detached
    
    //if (muxU1 == -1 || muxU1 != muxU2)      // nothing is attached or requesting detach of non attached unit
    //    return SCPE_NOTATT;                 // no mux unit is attached.
    
    //sim_printf("Multiplexor %s detached\n", fnpName);   //s[muxU2]);
    sim_printf("Multiplexor detached\n");   //s[muxU2]);

    sim_cancel( unitp ) ;
    return ( tmxr_detach(&mux_desc,unitp) ) ;
}

t_stat mux_clear( UNUSED t_bool flag )
{
    int line ;
    
    for ( line = 0 ; line < mux_max ; ++line )
    {
        mux_ldsc[line].xmte = 0 ;
        mux_ldsc[line].rcve = 0 ;
        if ( ! mux_ldsc[line].conn )
        {
            mux_ldsc[line].xmte = 1 ;                   /* set xmt enb */
            mux_ldsc[line].rcve = 1 ;                   /* clr rcv enb */
        }
    }
    return ( SCPE_OK ) ;
}

t_stat mux_setnl( UNUSED UNIT * uptr, UNUSED int32 val, char * cptr, UNUSED void * desc )
{
    int32   newln, i, t ;
    
    t_stat  r ;
    if ( cptr == NULL )
    {
        return ( SCPE_ARG ) ;
    }
    newln = (int32) get_uint( cptr, 10, MAX_LINES, &r ) ;
    if ( (r != SCPE_OK) || (newln == mux_desc.lines) )
    {
        return ( r ) ;
    }
    if ( (newln == 0) || (newln > MAX_LINES) )
    {
        return ( SCPE_ARG ) ;
    }
    if ( newln < mux_desc.lines )
    {
        for ( i = newln, t = 0 ; i < mux_desc.lines ; ++i )
        {
            t = t | mux_ldsc[i].conn ;
        }
        if ( t && ! get_yn("This will disconnect users; proceed [N]?", FALSE) )
        {
            return ( SCPE_OK ) ;
        }
        for ( i = newln ; i < mux_desc.lines ; ++i )
        {
            if ( mux_ldsc[i].conn )
            {                                       /* reset line */
                tmxr_msg( mux_ldsc[i].conn, "\r\nOperator disconnected line\r\n" ) ;
                tmxr_reset_ln( &mux_ldsc[i] ) ;
            }
            mux_clear( TRUE ) ;                         /* reset mux */
        }
    }
    mux_max = mux_desc.lines = newln ;
    return ( SCPE_OK ) ;
}

int mux_tmxr_putc( int line, TMLN * lp, int kar )
{
    int     a ;
    
    /*----------------------------------------------*/
    /*  Send character to given QTY/telnet line.    */
    /*                                              */
    /*  enter:      line    QTY line #              */
    /*              lp      Telnet unit def ptr     */
    /*              kar     character to send       */
    /*                                              */
    /*  return:     SCPE_OK                         */
    /*              SCPE_STALL                      */
    /*              SCPE_LOST                       */
    /*----------------------------------------------*/
    
#if 0
    a = tmxr_putc_ln( lp, kar ) ;
#else
// XXX bad code; blocks the thread
    while (SCPE_STALL == (a = tmxr_putc_ln (lp, kar)))
      {
        if (lp->txbsz == tmxr_send_buffered_data (lp))
          usleep (100); // 10 ms
      }

#endif
    if ( a == SCPE_OK)
    {
        MUX_LINE_SET_BIT(   line, MUX_L_TXDN )
        MUX_LINE_CLEAR_BIT( line, MUX_L_TXBZ )
    }
#if 0
    else if ( a == SCPE_STALL )
    {
        /*
         (should we try to output the buffer
         and then regroup...?)
         */
        MUX_LINE_SET_BIT(   line, MUX_L_TXBZ )
        MUX_LINE_CLEAR_BIT( line, MUX_L_TXDN )
        MUX_LINE_TX_CHAR( line ) = kar ;
    
        OnMuxStalled(line, kar);
    }
#endif
    else if ( a == SCPE_LOST )
    {
        /*  no connection - hangup?  */
        MUX_LINE_SET_BIT(   line, MUX_L_TXBZ )
        MUX_LINE_CLEAR_BIT( line, MUX_L_TXDN )
        MUX_LINE_TX_CHAR( line ) = kar ;
        
        OnMuxDisconnect(line, 0);   // was 'kar');
    }
    return ( a ) ;
}

static int mux_update_xmti( TMXR * mp )
{
    int     line ;
    TMLN *      lp ;
    int     changes ;
    
    /*------------------------------------------------------*/
    /*  Search through connected telnet lines for any de-   */
    /*  ferred output activity.                             */
    /*                                                      */
    /*  enter:      mp      master telnet qty desc ptr      */
    /*                                                      */
    /*  return:     int     change count (0 = none seen)    */
    /*------------------------------------------------------*/
    
    /*  any TX DONE flags set
     *  any TX BUSY flags set
     */
    
    for ( changes = line = 0; line < mp->lines; ++line )
        if ( MUX_LINE_BIT_SET(line,MUX_L_TXBZ) )
            if ( (lp=mp->ldsc+line)->conn && lp->xmte )
            {
                /*  why are we busy?  buffer was full?  */
                /*  now some space available - try
                 *  to stuff pending character in
                 *  buffer and free up the world
                 */
                mux_tmxr_putc( line, lp, MUX_LINE_TX_CHAR(line) ) ;
                ++changes ;
            }
    return ( changes ) ;
}

static int mux_update_rcvi( TMXR * mp )
{
    int     line ;
    TMLN *  lp ;
    int32   datum ;
    int     changes ;
    
    /*------------------------------------------------------*/
    /*  Search through connected telnet lines for any input */
    /*  activity.                                           */
    /*                                                      */
    /*  enter:      mp      master telnet qty desc ptr      */
    /*                                                      */
    /*  return:     int     change count (0 = none seen)    */
    /*------------------------------------------------------*/
    
    for ( changes = line = 0; line < mp->lines; ++line )
        if ( (lp=mp->ldsc+line)->conn && lp->rcve )
            if ( (datum=tmxr_getc_ln(lp)) )
            {
                if ( datum & SCPE_BREAK )
                {
                    /*  what should we do here - set QTY_L_BREAK?  */
                    datum = datum & 0x00FF ;
                    
                    OnMuxRxBreak(line, datum);
                }
                else
                {
                    datum = datum & 0x00FF ;
                    
                    OnMuxRx(mp, lp, line, datum);
                }
                /*  <check parity, masking, forced parity, CR/LF xlation>  */
                
                MUX_LINE_CLEAR_BIT( line, (MUX_L_RXBZ | MUX_L_DMASK) ) ;
                MUX_LINE_SET_BIT( line, (MUX_L_RXDN | datum) ) ;
                ++changes ;
            }
    return ( changes ) ;
}

int mux_update_status( UNUSED DIB * dibp, UNUSED TMXR * tmxr_desc )
{
    int     line ;
    int     status ;
    int     txbusy ;
    
    /*----------------------------------------------*/
    /*  return global device status for current qty */
    /*  state.                                      */
    /*                                              */
    /*  Receiver interrupts have higher priority    */
    /*  than transmitter interrupts according to DG */
    /*  but this routine could be modified to use   */
    /*  different priority criteria.                */
    /*                                              */
    /*  Round-robin polling could also be used in   */
    /*  some future release rather than starting    */
    /*  with line 0 each time.                      */
    /*                                              */
    /*  Return <QTY_S_RI + line # + character> of   */
    /*  first waiting character, else return        */
    /*  <QTY_S_TI + line #> of first finished line  */
    /*  output, else return 0.                      */
    /*                                              */
    /*  This routine does -not- clear input line    */
    /*  BZ/DN flags; caller should do this.         */
    /*                                              */
    /*  Global device done and busy flags are       */
    /*  updated.                    */
    /*----------------------------------------------*/
    
    for ( txbusy = status = line = 0 ; line < mux_max ; ++line )
    {
        txbusy |= (MUX_LINE_BIT_SET(line,MUX_L_TXBZ)) ;
        if ( MUX_LINE_BIT_SET(line,MUX_L_RXDN) )
        {
            if ( ! status )
            {
                status = MUX_LINE_BITS( line, MUX_S_DMASK ) | MUX_S_RI ;
                status = status | (line << 8) ;
            }
            break ;
        }
        else if ( MUX_LINE_BIT_SET(line,MUX_L_TXDN) )
        {
            if ( ! (status & MUX_S_RI) )
                if ( ! (status & MUX_S_RI) )
                {
                    status = MUX_S_TI ;
                    status = status | (line << 8) ;
                }
        }
    }
    /*  <we could check each line for TX busy to set DEV_SET_BUSY)?>  */
    MUX_CLR_BUSY( FNP_INT_V_MUX ) ;
    MUX_CLR_DONE( FNP_INT_V_MUX ) ;
    if ( txbusy )
    {
        MUX_SET_BUSY( FNP_INT_V_MUX ) ;
    }
    if ( status & (MUX_S_RI | MUX_S_TI) )
    {
        MUX_SET_DONE( FNP_INT_V_MUX ) ;
    }
    MUX_UPDATE_INTR ;                                   /*  update final intr status  */
    return ( status ) ;
}  

t_stat mux_svc(UNIT * unitp )
{
    DIB *dibp = &mux_dib;
    
    int     line ;
    int     newln ;
    TMLN *      tmlnp ;
    
    ++mux_polls ;                                       /*  another time 'round the track  */
    newln = tmxr_poll_conn( &mux_desc ) ;               /*  anybody knocking at the door?  */
    if ( (newln >= 0) && mux_mdm )
    {
        if ( newln >= mux_max )
        {
            return SCPE_IERR;                               /*  WTF - sanity check failed, over?  */
        }
        else
        {
            line = newln ;                                  /*  handle modem control  */
            tmlnp =&mux_ldsc[ line ] ;
            tmlnp->rcve = tmlnp->xmte = 1 ;
            /*  do QTY_LINE_ bit fiddling and state machine
             *  manipulation with modem control signals
             */
            OnMuxConnect(tmlnp, line);
        }
    }
    
    tmxr_poll_rx( &mux_desc ) ;                         /*  poll input                          */
    while (mux_update_rcvi( &mux_desc )) ;                      /*  update receiver interrupt status    */
    
    tmxr_poll_tx( &mux_desc ) ;                         /*  poll output                         */
    mux_update_xmti( &mux_desc ) ;                      /*  update transmitter interrupt status */
    
    mux_update_status( dibp, &mux_desc ) ;              /*  update device status                */
    
    sim_activate( unitp, mux_tmxr_poll ) ;              /*  restart the bubble machine          */
    
    // check for any dropped connections ...
    TMXR *mp = &mux_desc;
    for (int i = 0; i < mp->lines; i++) {               /* loop thru lines */
        MUXTERMIO *tty = &ttys[i];                      // fetch tty connection info
        if (tty->state != eDisconnected)
        {
            TMLN *lp = tty->tmln;
            if (!lp->sock && !lp->conn)                 /*  not connected */
                OnMuxDisconnect(i, 1);                  // connection dropped
        }
    }
    
    return ( SCPE_OK ) ;
}

DEVICE mux_dev = {
    "MUX",      // name
    &mux_unit,  // unit
    mux_reg,    // registers
    mux_mod,    // modifiers
    1,         // numunits
    8,          // aradix
    36,         // width
    1,          // aincr
    8,          // dradix
    36,         // dwidth
    NULL,       // examine
    NULL,       // deposit
    &mux_reset, // reset
    NULL,       // boot
    &mux_attach,// attach
    &mux_detach,// detach
    &mux_dib,   // context 
    (DEV_DISABLE | DEV_MUX),
    0,          // dctrl
    NULL,       // devflags
    NULL,       // msize
    "MUX",      // lname
    NULL,       // help
    NULL,       // attach help
    NULL,       // help context
    NULL,       // description
};

int32 mux_tmxr_poll = 16000;

t_stat  mux_reset(UNUSED DEVICE *dptr)
{
    //DIB *dibp = &mux_dib;
    UNIT *unitp = &mux_unit;
    
//    if ((dptr->flags & DEV_DIS) == 0)
//    {
//        if (dptr == &mux_dev) mux_dev.flags |= DEV_DIS;
//    }
    mux_clear( TRUE ) ;
    MUX_CLR_BUSY( FNP_INT_V_MUX ) ;                              /*  clear busy  */
    MUX_CLR_DONE( FNP_INT_V_MUX ) ;                              /*  clear done, int */
    MUX_UPDATE_INTR ;
    if ( MUX_MASTER_ACTIVE(&mux_desc) )
    {
        sim_activate( unitp, mux_tmxr_poll ) ;
    }
    else
    {
        sim_cancel( unitp ) ;
    }
    
    for(int line = 0 ; line < MAX_LINES ; line += 1)
    {
        MUXTERMIO *tty = &ttys[line];   // fetch tty connection info
        tty->mux_line = -1;
        tty->state = eDisconnected;
        tty->tmln = NULL;
        if (tty->fmti)
            tty->fmti->inUse = false;   // multics device no longer in use
        tty->fmti = NULL;
    }
    return ( SCPE_OK ) ;
    
}

/*
 * return integer indicating what mux device is currently attached. 
 * (returns -1 if no mux attached)
 */
//int32 muxWhatUnitAttached()
//{
//    for(int n = 0 ; n < 4 ; n += 1)
//    {
//        UNIT *u = mux_dev.units + n;
//        
//        if (u->filename != NULL && strlen(u->filename) != 0)
//            return n;
//    }
//    return -1;  // no mux unit attached
//}
