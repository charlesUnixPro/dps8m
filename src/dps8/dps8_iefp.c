/**
 * \file dps8_iefp.c
 * \project dps8
 * \date 01/11/14
 * \copyright Copyright (c) 2014 Harry Reed. All rights reserved.
 */

#include "dps8.h"
#include "dps8_bar.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_sys.h"
#include "dps8_iefp.h"
#include "dps8_utils.h"

word24 iefpFinalAddress;

// new Read/Write stuff ...

t_stat Read(word18 address, word36 *result, _processor_cycle_type cyctyp, bool b29)
{
    //word24 finalAddress;
    iefpFinalAddress = address;

    if (b29 || get_went_appending ())
        //<generate address from  pRn and offset in address>
        //core_read (address, * result);
        goto B29;
    
    switch (get_addr_mode())
    {
        case ABSOLUTE_MODE:
        
            setAPUStatus (apuStatus_FABS);
            core_read(address, result);
            return SCPE_OK;
        
        case BAR_MODE:
        
            setAPUStatus (apuStatus_FABS); // XXX maybe...
            iefpFinalAddress = getBARaddress(address);
        
            core_read(iefpFinalAddress, result);

            return SCPE_OK;
        
        case APPEND_MODE:
            //    <generate address from procedure base registers>
B29:        //iefpFinalAddress = doAppendRead(i, accessType, address);
            iefpFinalAddress = doAppendCycle(address, cyctyp);
            core_read(iefpFinalAddress, result);
        
            sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Read(Actual) Read: iefpFinalAddress=%08o readData=%012llo\n", iefpFinalAddress, *result);
        
            return SCPE_OK;
    }
    
    return SCPE_UNK;
}

t_stat Write(word18 address, word36 data, _processor_cycle_type cyctyp, bool b29)
{
    //word24 finalAddress;
    iefpFinalAddress = address;

    if (b29 || get_went_appending ())
        //<generate address from  pRn and offset in address>
        //core_read (address, * result);
        goto B29;
    
    
    switch (get_addr_mode())
    {
        case ABSOLUTE_MODE:
        
            setAPUStatus (apuStatus_FABS);
            core_write(address, data);
            return SCPE_OK;
        
        case BAR_MODE:
        
            iefpFinalAddress = getBARaddress(address);
            setAPUStatus (apuStatus_FABS); // XXX maybe...
            core_write(iefpFinalAddress, data);
        
            return SCPE_OK;
        
        case APPEND_MODE:
            //    <generate address from procedure base registers>
B29:        //iefpFinalAddress = doAppendDataWrite(i, address);
            iefpFinalAddress = doAppendCycle(address, cyctyp);
            core_write(iefpFinalAddress, data);
        
            sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Write(Actual) Write: iefpFinalAddress=%08o data=%012llo\n", iefpFinalAddress, data);
        
            return SCPE_OK;
    }
    
    return SCPE_UNK;
}


