/**
 * \file dps8_iefp.c
 * \project dps8
 * \date 01/11/14
 * \copyright Copyright (c) 2014 Harry Reed. All rights reserved.
 */

#include "dps8.h"
#include "dps8_append.h"
#include "dps8_bar.h"
#include "dps8_cpu.h"
#include "dps8_sys.h"
#include "dps8_iefp.h"


// new Read/Write stuff ...

t_stat Read(word18 address, word36 *result, _processor_cycle_type cyctyp, bool b29)
{
    word24 finalAddress;
    if (b29)
        //<generate address from  pRn and offset in address>
        //core_read (address, * result);
        goto B29;
    
    switch (get_addr_mode())
    {
        case ABSOLUTE_MODE:
        
            core_read(address, result);
            return SCPE_OK;
        
        case BAR_MODE:
        
            finalAddress = getBARaddress(address);
        
            core_read(finalAddress, result);

            return SCPE_OK;
        
        case APPEND_MODE:
            //    <generate address from procedure base registers>
B29:        //finalAddress = doAppendRead(i, accessType, address);
            finalAddress = doAppendCycle(address, cyctyp);
            core_read(finalAddress, result);
        
            sim_debug(DBG_APPENDING, &cpu_dev, "Read(Actual) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *result);
        
            return SCPE_OK;
    }
    
    return SCPE_UNK;
}

t_stat Write(word18 address, word36 data, _processor_cycle_type cyctyp, bool b29)
{
    word24 finalAddress;
    if (b29)
        //<generate address from  pRn and offset in address>
        //core_read (address, * result);
        goto B29;
    
    
    switch (get_addr_mode())
    {
        case ABSOLUTE_MODE:
        
            core_write(address, data);
            return SCPE_OK;
        
        case BAR_MODE:
        
            finalAddress = getBARaddress(address);
            core_write(finalAddress, data);
        
            return SCPE_OK;
        
        case APPEND_MODE:
            //    <generate address from procedure base registers>
B29:        //finalAddress = doAppendDataWrite(i, address);
            finalAddress = doAppendCycle(address, cyctyp);
            core_write(finalAddress, data);
        
            sim_debug(DBG_APPENDING, &cpu_dev, "Write(Actual) Write: finalAddress=%08o data=%012llo\n", finalAddress, data);
        
            return SCPE_OK;
    }
    
    return SCPE_UNK;
}


