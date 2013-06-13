//
//  dps8_faults.c
//  dps8
//
//  Created by Harry Reed on 6/11/13.
//  Copyright (c) 2013 Harry Reed. All rights reserved.
//

#include <stdio.h>

#include "dps8.h"



/*
 
                                Table 7-1. List of Faults
 
 Decimal fault     Octal (1)      Fault   Fault name            Priority    Group
     number      fault address   mnemonic
        0      ;         0     ;      sdf  ;   Shutdown             ;   27     ;     7
        1      ;         2     ;      str  ;   Store                ;   10     ;     4
        2      ;         4     ;      mme  ;   Master mode entry 1  ;   11     ;     5
        3      ;         6     ;      f1   ;   Fault tag 1          ;   17     ;     5
        4      ;        10     ;      tro  ;   Timer runout         ;   26     ;     7
        5      ;        12     ;      cmd  ;   Command              ;   9      ;     4
        6      ;        14     ;      drl  ;   Derail               ;   15     ;     5
        7      ;        16     ;      luf  ;   Lockup               ;   5      ;     4
        8      ;        20     ;      con  ;   Connect              ;   25     ;     7
        9      ;        22     ;      par  ;   Parity               ;   8      ;     4
        10     ;        24     ;      ipr  ;   Illegal procedure    ;   16     ;     5
        11     ;        26     ;      onc  ;   Operation not complete ; 4      ;     2
        12     ;        30     ;      suf  ;   Startup              ;   1      ;     1
        13     ;        32     ;      ofl  ;   Overflow             ;   7      ;     3
        14     ;        34     ;      div  ;   Divide check         ;   6      ;     3
        15     ;        36     ;      exf  ;   Execute              ;   2      ;     1
        16     ;        40     ;      df0  ;   Directed fault 0     ;   20     ;     6
        17     ;        42     ;      df1  ;   Directed fault 1     ;   21     ;     6
        18     ;        44     ;      df2  ;   Directed fault 2     ;   22     ;     6
        19     ;        46     ;      df3  ;   Directed fault 3     ;   23     ;     6
        20     ;        50     ;      acv  ;   Access violation     ;   24     ;     6
        21     ;        52     ;      mme2 ;   Master mode entry 2  ;   12     ;     5
        22     ;        54     ;      mme3 ;   Master mode entry 3  ;   13     ;     5
        23     ;        56     ;      mme4 ;   Master mode entry 4  ;   14     ;     5
        24     ;        60     ;      f2   ;   Fault tag 2          ;   18     ;     5
        25     ;        62     ;      f3   ;   Fault tag 3          ;   19     ;     5
        26     ;        64     ;           ;   Unassigned           ;          ;
        27     ;        66     ;           ;   Unassigned           ;          ;
 
*/
struct faults
{
    int         fault_number;
    int         fault_address;
    const char *fault_mnemonic;
    const char *fault_name;
    int         fault_priority;
    int         fault_group;
    bool        fault_pending;        // when true fault is pending and waiting to be processed
} _faults[] = {
//  number  address  mnemonic   name                 Priority    Group
    {   12,     030,    "suf",  "Startup",                  1,	     1,     false },
    {   15,     036,    "exf",  "Execute",                  2,	     1,     false },
    {   11,     026,    "onc",  "Operation not complete", 	4,	     2,     false },
    {   7,      016,    "luf",  "Lockup",               	5,	     4,     false },
    {   14,     034,    "div",  "Divide check",         	6,	     3,     false },
    {   13,     032,    "ofl",  "Overflow",             	7,	     3,     false },
    {   9,      022,    "par",  "Parity",               	8,	     4,     false },
    {   5,      012,    "cmd",  "Command",              	9,	     4,     false },
    {   1,       2 ,    "str",  "Store",                	10,	     4,     false },
    {   2,       4 ,    "mme",  "Master mode entry 1",  	11,	     5,     false },
    {   21,     052,    "mme2", "Master mode entry 2",  	12,	     5,     false },
    {   22,     054,    "mme3", "Master mode entry 3",  	13,	     5,     false },
    {   23,     056,    "mme4", "Master mode entry 4",  	14,	     5,     false },
    {   6,      014,    "drl",  "Derail",               	15,	     5,     false },
    {   10,     024,    "ipr",  "Illegal procedure",    	16,	     5,     false },
    {   3,       06,    "f1",   "Fault tag 1",          	17,	     5,     false },
    {   24,     060,    "f2",   "Fault tag 2",          	18,	     5,     false },
    {   25,     062,    "f3",   "Fault tag 3",          	19,	     5,     false },
    {   16,     040,    "df0",  "Directed fault 0",     	20,	     6,     false },
    {   17,     042,    "df1",  "Directed fault 1",     	21,	     6,     false },
    {   18,     044,    "df2",  "Directed fault 2",     	22,	     6,     false },
    {   19,     046,    "df3",  "Directed fault 3",     	23,	     6,     false },
    {   20,     050,    "acv",  "Access violation",     	24,	     6,     false },
    {   8,      020,    "con",  "Connect",              	25,	     7,     false },
    {   4,      010,    "tro",  "Timer runout",         	26,	     7,     false },
    {   0,       0 ,    "sdf",  "Shutdown",             	27,	     7,     false },
    {   26,     064,    "???",  "Unassigned",               -1,     -1,     false },
    {   27,     066,    "???",  "Unassigned",               -1,     -1,     false },
    {   -1,     -1,     NULL,   NULL,                       -1,     -1,     false }
};
typedef struct faults faults;

bool pending_fault = false;     // true when a fault has been signalled, but not processed


bool port_interrupts[8] = {false, false, false, false, false, false, false, false };

/*
 * fault handler(s).
 */
DCDstruct *decodeInstruction(word36 inst, DCDstruct *dst);     // decode instruction into structure
t_stat executeInstruction(DCDstruct *ci);

t_stat doFaultInstructionPair(DCDstruct *i, word24 fltAddress)
{
    // XXX stolen from xed instruction
    
    DCDstruct _xip;   // our decoded instruction struct
    EISstruct _eis;

    word36 insPair[2];
    Read2(i, fltAddress, &insPair[0], &insPair[1], InstructionFetch, 0);
    
    //fetchInstruction(word18 addr, DCDstruct *i)  // fetch instrcution at address
    
    _xip.IWB = insPair[0];
    _xip.e = &_eis;
    
    DCDstruct *xec = decodeInstruction(insPair[0], &_xip);    // fetch instruction into current instruction
    
    t_stat ret = executeInstruction(xec);
    
    if (ret)
        return (ret);
    
    _xip.IWB = insPair[1];
    _xip.e = &_eis;
    
    xec = decodeInstruction(insPair[1], &_xip);               // fetch instruction into current instruction
    
    ret = executeInstruction(xec);
    
    if (ret)
        return (ret);
    
    return SCPE_OK;
}

void doFault(DCDstruct *i, int faultNumber, int faultGroup, char *faultMsg)
{
    printf("fault: %d %d '%s'\r\n", faultNumber, faultGroup, faultMsg ? faultMsg : "?");
    return;
    
    pending_fault = true;
    bool restart = false;
    
    faults *f = _faults;
    while (f->fault_mnemonic)
    {
        if (faultNumber == f->fault_number)
        {
            int fltAddress = rFAULTBASE & 07740; // (12-bits of which the top-most 7-bits are used)
            fltAddress += f->fault_address;
            
            f->fault_pending = true;        // this particular fault is pending, waiting for processing
            
            _processor_addressing_mode modeTemp = processorAddressingMode;
            
            processorAddressingMode = ABSOLUTE_MODE;
            word24 rIC_temp = rIC;
            
            doFaultInstructionPair(i, fltAddress);
            
            f->fault_pending = false;        // this particular fault is pending, waiting for processing
            pending_fault = false;
            
            processorAddressingMode = modeTemp;
            
            break;
        }
        f += 1;
    }
    
    if (!f->fault_mnemonic)
        printf("doFault(): unhandled fault# %d (%o)\n", faultNumber, faultNumber);

    // XXX we really only want to do this in extreme conditions since faults can be returned from *more-or-less*
    // XXX do it properly - later..
    if (restart)
         longjmp(jmpMain, faultNumber);
}


