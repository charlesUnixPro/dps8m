/**
 * \file dps8_iefp.c
 * \project dps8
 * \date 01/11/14
 * \copyright Copyright (c) 2014 Harry Reed. All rights reserved.
 */

#include "dps8.h"
#include "dps8_bar.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_iefp.h"
#include "dps8_utils.h"

word24 iefpFinalAddress;

// new Read/Write stuff ...

t_stat Read(word18 address, word36 *result, _processor_cycle_type cyctyp, bool b29)
{
    //word24 finalAddress;
    iefpFinalAddress = address;

    //bool isBAR = TSTF (CPU -> cu . IR, I_NBAR) ? false : true;
    bool isBAR = get_bar_mode();

    // XXX went appending in BAR mode?
    if (b29 || get_went_appending ())
    {
        //if (isBAR)
          //sim_printf ("went appending fired in BAR mode\n"); 
        goto B29;
    }

    switch (get_addr_mode())
    {
        case ABSOLUTE_mode:
        {
            if (isBAR)
            {
                setAPUStatus (apuStatus_FABS); // XXX maybe...
                iefpFinalAddress = getBARaddress(address);
        
                core_read(iefpFinalAddress, result, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Read (Actual) Read:       bar address=%08o  readData=%012llo\n", address, *result);
                return SCPE_OK;
            } else {
                setAPUStatus (apuStatus_FABS);
                core_read(address, result, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Read (Actual) Read:       abs address=%08o  readData=%012llo\n", address, *result);
                return SCPE_OK;
            }
        }

        case APPEND_mode:
        {
B29:;
            if (isBAR)
            {
                word18 barAddress = getBARaddress (address);
                iefpFinalAddress = doAppendCycle(barAddress, cyctyp);
                core_read(iefpFinalAddress, result, __func__);

                return SCPE_OK;
            } else {
                //    <generate address from procedure base registers>
                //iefpFinalAddress = doAppendRead(i, accessType, address);
                iefpFinalAddress = doAppendCycle(address, cyctyp);
                core_read(iefpFinalAddress, result, __func__);
                // XXX Don't trace Multics idle loop
                if (PPR.PSR != 061 && PPR.IC != 0307)
                  {
                    sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Read (Actual) Read:  iefpFinalAddress=%08o  readData=%012llo\n", iefpFinalAddress, *result);
                  }
            }
            return SCPE_OK;
        }
    }
    return SCPE_UNK;
}

t_stat Write(word18 address, word36 data, _processor_cycle_type cyctyp, bool b29)
{
    //word24 finalAddress;
    iefpFinalAddress = address;

    bool isBAR = TSTF (CPU -> cu . IR, I_NBAR) ? false : true;

    if (b29 || get_went_appending ())
        //<generate address from  pRn and offset in address>
        //core_read (address, * result, __func__);
        goto B29;
    
    
    switch (get_addr_mode())
    {
        case ABSOLUTE_mode:
        {
            if (isBAR)
            {
                iefpFinalAddress = getBARaddress(address);
                setAPUStatus (apuStatus_FABS); // XXX maybe...
                core_write(iefpFinalAddress, data, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Write(Actual) Write:      bar address=%08o writeData=%012llo\n", address, data);
                return SCPE_OK;
            } else {
                setAPUStatus (apuStatus_FABS);
                core_write(address, data, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Write(Actual) Write:      abs address=%08o writeData=%012llo\n", address, data);
                return SCPE_OK;
            }
        }

        case APPEND_mode:
        {
B29:
            if (isBAR)
            {
                word18 barAddress = getBARaddress (address);
                iefpFinalAddress = doAppendCycle(barAddress, cyctyp);
                core_write(iefpFinalAddress, data, __func__);
        
                sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Write(Actual) Write: iefpFinalAddress=%08o writeData=%012llo\n", iefpFinalAddress, data);
        
                return SCPE_OK;
            } else {
                //    <generate address from procedure base registers>
                //iefpFinalAddress = doAppendDataWrite(i, address);
                iefpFinalAddress = doAppendCycle(address, cyctyp);
                core_write(iefpFinalAddress, data, __func__);
        
                sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Write(Actual) Write: iefpFinalAddress=%08o writeData=%012llo\n", iefpFinalAddress, data);
        
                return SCPE_OK;
            }
        }
    }
    
    return SCPE_UNK;
}


