/**
 * \file dps8_iefp.c
 * \project dps8
 * \date 01/11/14
 * \copyright Copyright (c) 2014 Harry Reed. All rights reserved.
 */

#include "dps8.h"

/*
 * Code that handles the main
 *   instruction fetch cycle
 *   execution execution cycle
 *   fault processing cycle
 *   page fault handler
 * LOOP
 */

#ifndef QUIET_UNUSED
DCDstruct *ci = 0;

EISstruct e;

enum eIEFPState
{
    eIEFPUnknown = 0,
    eIEFPInstructionFetch,
    eIEFPExecute,
    eIEFPFaultProcess,
    eIEFPPageFault,
};
typedef enum eIEFPState IEFPState;


IEFPState iefpState = eIEFPUnknown;
#endif

#ifndef QUIET_UNUSED
PRIVATE
t_stat IEFPInstructionFetch()
{
    ci = fetchInstruction(PPR.IC, currentInstruction);    // fetch instruction into current instruction struct
    
    // XXX The conditions are more rigorous: see AL39, pg 327
    if (PPR.IC % 2 == 0 && // Even address
            ci -> i == 0) // Not inhibited
        cpu . interrupt_flag = sample_interrupts ();
    else
        cpu . interrupt_flag = false;
    
    return SCPE_OK;
}
#endif

#ifndef QUIET_UNUSED
PRIVATE
IEFPState NextState()
{
    switch(iefpState)
    {
        case eIEFPUnknown:
            return eIEFPInstructionFetch;
        case eIEFPInstructionFetch:
            return eIEFPExecute;
        case eIEFPExecute:
            return eIEFPInstructionFetch;
        case eIEFPFaultProcess:     // not handled (yet)
            return eIEFPUnknown;
        case eIEFPPageFault:        // not handled (yet)
            return eIEFPUnknown;
        default:
            sim_printf("NextState(): Unknown/unhandled iefpState %d\n", iefpState);
            return eIEFPUnknown;
    }
  
}
#endif

#ifndef QUIET_UNUSED
PRIVATE
t_stat IEFPExecuteInstruction()
{
    t_stat ret = executeInstruction(ci);
    
    if (ret)
    {
        if (ret > 0)
        {
            return ret;
        } else {
            switch (ret)
            {
                case CONT_TRA:
                    return SCPE_OK;   // don't bump PPR.IC, instruction already did it
                case CONT_FAULT:
                {
                    // XXX Instruction faulted.
                }
                break;
            }
        }
    }
    
    // XXX Remove this when we actually can wait for an interrupt
    if (ci->opcode == 0616) // DIS
    {
        return STOP_DIS;
    }
    
#ifndef QUIET_UNUSED
jmpNext:;
#endif
    // doesn't seem to work as advertized
    if (sim_poll_kbd())
        return STOP_BKPT;
    
    // XXX: what if sim stops during XEC/XED? if user wants to re-step
    // instruc, is this logic OK?
    if(XECD == 1) {
        XECD = 2;
    } else if(XECD == 2) {
        XECD = 0;
    } else if (cpu . cycle != DIS_cycle) // XXX maybe cycle == FETCH_cycle
    
    
    PPR.IC += 1;
    
    // is this a multiword EIS?
    // XXX: no multiword EIS for XEC/XED/fault, right?? -MCW
    if (ci->info->ndes > 0)
        PPR.IC += ci->info->ndes;
    
    return SCPE_OK;
}
#endif

#ifndef QUIET_UNUSED
PRIVATE
t_stat doIEFP()
{
    switch(iefpState)
    {
        case eIEFPUnknown:
            ci = currentInstruction;
            ci->e = &e;
            return SCPE_OK;
        
        case eIEFPInstructionFetch:
            return IEFPInstructionFetch();;
        
        case eIEFPExecute:
            return IEFPExecuteInstruction();
        
        case eIEFPFaultProcess:
        case eIEFPPageFault:
            return SCPE_OK;
        
        default:
            sim_printf("doIETF(): Unknown/unhandled iefpState %d\n", iefpState);
            return SCPE_UNK;
    }
    
}
#endif

#ifndef QUIET_UNUSED
/*
 * Herewith is the main loop ...
 */
t_stat doIEFPLoop()
{
    t_stat lastResult = SCPE_OK;
    while ((lastResult = doIEFP()) == SCPE_OK)
        iefpState = NextState();
    
    return lastResult;
}
#endif


// new Read/Write stuff ...

t_stat Read(DCDstruct *i, word18 address, word36 *result, _processor_cycle_type cyctyp, bool b29)
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
            finalAddress = doAppendCycle(i, address, cyctyp);
            core_read(finalAddress, result);
        
            sim_debug(DBG_APPENDING, &cpu_dev, "Read(Actual) Read: finalAddress=%08o readData=%012llo\n", finalAddress, *result);
        
            return SCPE_OK;
    }
    
    return SCPE_UNK;
}

t_stat Write(DCDstruct *i, word18 address, word36 data, _processor_cycle_type cyctyp, bool b29)
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
            finalAddress = doAppendCycle(i, address, cyctyp);
            core_write(finalAddress, data);
        
            sim_debug(DBG_APPENDING, &cpu_dev, "Write(Actual) Write: finalAddress=%08o data=%012llo\n", finalAddress, data);
        
            return SCPE_OK;
    }
    
    return SCPE_UNK;
}


