/**
 * \file dps8_iefp.c
 * \project dps8
 * \date 01/11/14
 * \copyright Copyright (c) 2014 Harry Reed. All rights reserved.
 */

#include "dps8.h"
#include "dps8_bar.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_iefp.h"
#include "dps8_utils.h"
#include "hdbg.h"

// new Read/Write stuff ...

t_stat Read(word18 address, word36 *result, _processor_cycle_type cyctyp, bool b29)
{
    //word24 finalAddress;
    cpu . iefpFinalAddress = address;

    //bool isBAR = TST_I_NBAR ? false : true;
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
                cpu . iefpFinalAddress = getBARaddress(address);
        
                core_read(cpu . iefpFinalAddress, result, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Read (Actual) Read:       bar address=%08o  readData=%012llo\n", address, *result);
#ifdef HDBG
                hdbgMRead (cpu . iefpFinalAddress, * result);
#endif
                return SCPE_OK;
            } else {
                setAPUStatus (apuStatus_FABS);
                core_read(address, result, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Read (Actual) Read:       abs address=%08o  readData=%012llo\n", address, *result);
#ifdef HDBG
                hdbgMRead (address, * result);
#endif
                return SCPE_OK;
            }
        }

        case APPEND_mode:
        {
B29:;
            if (isBAR)
            {
                word18 barAddress = getBARaddress (address);
                cpu . iefpFinalAddress = doAppendCycle(barAddress, cyctyp);
                core_read(cpu . iefpFinalAddress, result, __func__);
                sim_debug (DBG_APPENDING | DBG_FINAL, &cpu_dev, "Read (Actual) Read:  bar iefpFinalAddress=%08o  readData=%012llo\n", cpu . iefpFinalAddress, *result);
#ifdef HDBG
                hdbgMRead (cpu . iefpFinalAddress, * result);
#endif

                return SCPE_OK;
            } else {
                //    <generate address from procedure base registers>
                //iefpFinalAddress = doAppendRead(i, accessType, address);
                cpu . iefpFinalAddress = doAppendCycle(address, cyctyp);
                core_read(cpu . iefpFinalAddress, result, __func__);
                // XXX Don't trace Multics idle loop
                if (cpu . PPR.PSR != 061 && cpu . PPR.IC != 0307)
                  {
                    sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Read (Actual) Read:  iefpFinalAddress=%08o  readData=%012llo\n", cpu . iefpFinalAddress, *result);
#ifdef HDBG
                    hdbgMRead (cpu . iefpFinalAddress, * result);
#endif
                  }
            }
            return SCPE_OK;
        }
    }
    return SCPE_UNK;
}

t_stat Read8 (word18 address, word36 * result, _processor_cycle_type cyctyp, bool b29)
  {
    cpu.iefpFinalAddress = address;

    bool isBAR = get_bar_mode ();

    if (b29 || get_went_appending ())
      {
        goto B29;
      }

    switch (get_addr_mode ())
      {
        case ABSOLUTE_mode:
          {
            if (isBAR)
              {
                setAPUStatus (apuStatus_FABS); // XXX maybe...
                cpu.iefpFinalAddress = getBARaddress (address);
        
                core_readN (cpu.iefpFinalAddress, result, 8, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < 8; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "Read (Actual) Read:       bar address=%08o  readData=%012llo\n", address + i, result [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < 8; i ++)
                  hdbgMRead (cpu.iefpFinalAddress + i, result [i]);
#endif
                return SCPE_OK;
              }
            else
              {
                setAPUStatus (apuStatus_FABS);
                core_readN (address, result, 8, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (int i = 0; i < 8; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "Read (Actual) Read:       abs address=%08o  readData=%012llo\n", address, *result);
                  }
#ifdef HDBG
                for (uint i = 0; i < 8; i ++)
                  hdbgMRead (address + i, result [i]);
#endif
                return SCPE_OK;
              }
          }

        case APPEND_mode:
          {
B29:;
            if (isBAR)
              {
                word18 barAddress = getBARaddress (address);
                cpu.iefpFinalAddress = doAppendCycle (barAddress, cyctyp);
                core_readN (cpu.iefpFinalAddress, result, 8, __func__);
                if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < 8; i ++)
                     sim_debug (DBG_APPENDING | DBG_FINAL, &cpu_dev, "Read (Actual) Read:  bar iefpFinalAddress=%08o  readData=%012llo\n", cpu.iefpFinalAddress + i, result [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < 8; i ++)
                  hdbgMRead (cpu.iefpFinalAddress + i, result [i]);
#endif

                return SCPE_OK;
              }
            else
              {
                cpu.iefpFinalAddress = doAppendCycle (address, cyctyp);
                core_readN (cpu.iefpFinalAddress, result, 8, __func__);
                // XXX Don't trace Multics idle loop
                if (cpu.PPR.PSR != 061 && cpu.PPR.IC != 0307)
                  {
                    if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                      {
                        for (uint i = 0; i < 8; i ++)
                          sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev, "Read (Actual) Read:  iefpFinalAddress=%08o  readData=%012llo\n", cpu.iefpFinalAddress + i, result [i]);
                      }
#ifdef HDBG
                    for (uint i = 0; i < 8; i ++)
                      hdbgMRead (cpu.iefpFinalAddress + i, result [i]);
#endif
                  }
              }
            return SCPE_OK;
          }
      }
    return SCPE_UNK;
  }

t_stat ReadPage (word18 address, word36 * result, _processor_cycle_type cyctyp, bool b29)
  {
    if ((address & PGMK) != 0)
      {
        sim_err ("ReadPage not on boundary %06o\n", address);
      }
    address &= ~PGMK; // Round to page boundary
    cpu.iefpFinalAddress = address;

    // We don't need get_bar_mode here as this code won't be
    // used when reading fault pairs
    bool isBAR = TST_I_NBAR ? false : true;

    if (b29 || get_went_appending ())
      {
        goto B29;
      }

    switch (get_addr_mode ())
      {
        case ABSOLUTE_mode:
          {
            if (isBAR)
              {
                setAPUStatus (apuStatus_FABS); // XXX maybe...
                cpu.iefpFinalAddress = getBARaddress (address);
        
                core_readN (cpu.iefpFinalAddress, result, PGSZ, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < PGSZ; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "ReadPage (Actual) Read:       bar address=%08o  readData=%012llo\n", address + i, result [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < PGSZ; i ++)
                  hdbgMRead (cpu.iefpFinalAddress + i, result [i]);
#endif
                return SCPE_OK;
              }
            else
              {
                setAPUStatus (apuStatus_FABS);
                core_readN (address, result, PGSZ, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (int i = 0; i < PGSZ; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "ReadPage (Actual) Read:       abs address=%08o  readData=%012llo\n", address, *result);
                  }
#ifdef HDBG
                for (uint i = 0; i < PGSZ; i ++)
                  hdbgMRead (address + i, result [i]);
#endif
                return SCPE_OK;
              }
          }

        case APPEND_mode:
          {
B29:;
            if (isBAR)
              {
                word18 barAddress = getBARaddress (address);
                cpu.iefpFinalAddress = doAppendCycle (barAddress, cyctyp);
                core_readN (cpu.iefpFinalAddress, result, PGSZ, __func__);
                if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < PGSZ; i ++)
                     sim_debug (DBG_APPENDING | DBG_FINAL, &cpu_dev, "ReadPage (Actual) Read:  bar iefpFinalAddress=%08o  readData=%012llo\n", cpu.iefpFinalAddress + i, result [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < PGSZ; i ++)
                  hdbgMRead (cpu.iefpFinalAddress + i, result [i]);
#endif

                return SCPE_OK;
              }
            else
              {
                cpu.iefpFinalAddress = doAppendCycle (address, cyctyp);
                core_readN (cpu.iefpFinalAddress, result, PGSZ, __func__);
                // XXX Don't trace Multics idle loop
                if (cpu.PPR.PSR != 061 && cpu.PPR.IC != 0307)
                  {
                    if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                      {
                        for (uint i = 0; i < PGSZ; i ++)
                          sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev, "ReadPage (Actual) Read:  iefpFinalAddress=%08o  readData=%012llo\n", cpu.iefpFinalAddress + i, result [i]);
                      }
#ifdef HDBG
                    for (uint i = 0; i < PGSZ; i ++)
                      hdbgMRead (cpu.iefpFinalAddress + i, result [i]);
#endif
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
    cpu . iefpFinalAddress = address;

    // We don't need get_bar_mode here as this code won't be
    // used when reading fault pairs
    bool isBAR = TST_I_NBAR ? false : true;

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
                cpu . iefpFinalAddress = getBARaddress(address);
                setAPUStatus (apuStatus_FABS); // XXX maybe...
                core_write(cpu . iefpFinalAddress, data, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Write(Actual) Write:      bar address=%08o writeData=%012llo\n", address, data);
#ifdef HDBG
                hdbgMWrite (cpu . iefpFinalAddress, data);
#endif
                return SCPE_OK;
            } else {
                setAPUStatus (apuStatus_FABS);
                core_write(address, data, __func__);
                sim_debug(DBG_FINAL, &cpu_dev, "Write(Actual) Write:      abs address=%08o writeData=%012llo\n", address, data);
#ifdef HDBG
                hdbgMWrite (address, data);
#endif
                return SCPE_OK;
            }
        }

        case APPEND_mode:
        {
B29:
            if (isBAR)
            {
                word18 barAddress = getBARaddress (address);
                cpu . iefpFinalAddress = doAppendCycle(barAddress, cyctyp);
                core_write(cpu . iefpFinalAddress, data, __func__);
        
                sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Write(Actual) Write: bar iefpFinalAddress=%08o writeData=%012llo\n", cpu . iefpFinalAddress, data);
#ifdef HDBG
                hdbgMWrite (cpu . iefpFinalAddress, data);
#endif
        
                return SCPE_OK;
            } else {
                //    <generate address from procedure base registers>
                //cpu . iefpFinalAddress = doAppendDataWrite(i, address);
                cpu . iefpFinalAddress = doAppendCycle(address, cyctyp);
                core_write(cpu . iefpFinalAddress, data, __func__);
        
                sim_debug(DBG_APPENDING | DBG_FINAL, &cpu_dev, "Write(Actual) Write: iefpFinalAddress=%08o writeData=%012llo\n", cpu . iefpFinalAddress, data);
#ifdef HDBG
                hdbgMWrite (cpu . iefpFinalAddress, data);
#endif
        
                return SCPE_OK;
            }
        }
    }
    
    return SCPE_UNK;
}


t_stat Write8 (word18 address, word36 * data, _processor_cycle_type cyctyp, bool b29)
  {
    address &= paragraphMask; // Round to 8 word boundarryt
    cpu.iefpFinalAddress = address;

    // We don't need get_bar_mode here as this code won't be
    // used when reading fault pairs
    bool isBAR = TST_I_NBAR ? false : true;

    if (b29 || get_went_appending ())
      goto B29;
    
    
    switch (get_addr_mode ())
     {
        case ABSOLUTE_mode:
          {
            if (isBAR)
             {
                cpu.iefpFinalAddress = getBARaddress (address);
                setAPUStatus (apuStatus_FABS); // XXX maybe...
                core_writeN (cpu.iefpFinalAddress, data, 8, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < 8; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "Write8(Actual) Write:      bar address=%08o writeData=%012llo\n", address + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < 8; i ++)
                  hdbgMWrite (cpu.iefpFinalAddress + i, data [i]);
#endif
                return SCPE_OK;
              }
            else
              {
                setAPUStatus (apuStatus_FABS);
                core_writeN (address, data, 8, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < 8; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "Write(Actual) Write:      abs address=%08o writeData=%012llo\n", address + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < 8; i ++)
                  hdbgMWrite (address + i, data [i]);
#endif
                return SCPE_OK;
              }
          }

        case APPEND_mode:
          {
B29:
            if (isBAR)
              {
                word18 barAddress = getBARaddress (address);
                cpu.iefpFinalAddress = doAppendCycle (barAddress, cyctyp);
                core_writeN (cpu.iefpFinalAddress, data, 8, __func__);
        
                if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < 8; i ++)
                      sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev, "Write8(Actual) Write: bar iefpFinalAddress=%08o writeData=%012llo\n", cpu.iefpFinalAddress + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < 8; i ++)
                  hdbgMWrite (cpu.iefpFinalAddress + i, data [i]);
#endif
        
                return SCPE_OK;
              }
            else
              {
                cpu.iefpFinalAddress = doAppendCycle (address, cyctyp);
                core_writeN (cpu.iefpFinalAddress, data, 8, __func__);
        
                if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < 8; i ++)
                      sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev, "Write(Actual) Write: iefpFinalAddress=%08o writeData=%012llo\n", cpu.iefpFinalAddress + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < 8; i ++)
                  hdbgMWrite (cpu.iefpFinalAddress + i, data [i]);
#endif
        
                return SCPE_OK;
              }
          }
      }
    return SCPE_UNK;
  }

t_stat WritePage (word18 address, word36 * data, _processor_cycle_type cyctyp, bool b29)
  {
    if ((address & PGMK) != 0)
      {
        sim_err ("WritePage not on boundary %06o\n", address);
      }
    address &= ~PGMK; // Round to page boundary
    cpu.iefpFinalAddress = address;

    // We don't need get_bar_mode here as this code won't be
    // used when reading fault pairs
    bool isBAR = TST_I_NBAR ? false : true;

    if (b29 || get_went_appending ())
      goto B29;
    
    
    switch (get_addr_mode ())
     {
        case ABSOLUTE_mode:
          {
            if (isBAR)
             {
                cpu.iefpFinalAddress = getBARaddress (address);
                setAPUStatus (apuStatus_FABS); // XXX maybe...
                core_writeN (cpu.iefpFinalAddress, data, PGSZ, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < PGSZ; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "WritePage(Actual) Write:      bar address=%08o writeData=%012llo\n", address + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < PGSZ; i ++)
                  hdbgMWrite (cpu.iefpFinalAddress + i, data [i]);
#endif
                return SCPE_OK;
              }
            else
              {
                setAPUStatus (apuStatus_FABS);
                core_writeN (address, data, PGSZ, __func__);
                if_sim_debug (DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < PGSZ; i ++)
                      sim_debug (DBG_FINAL, & cpu_dev, "WritePage(Actual) Write:      abs address=%08o writeData=%012llo\n", address + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < PGSZ; i ++)
                  hdbgMWrite (address + i, data [i]);
#endif
                return SCPE_OK;
              }
          }

        case APPEND_mode:
          {
B29:
            if (isBAR)
              {
                word18 barAddress = getBARaddress (address);
                cpu.iefpFinalAddress = doAppendCycle (barAddress, cyctyp);
                core_writeN (cpu.iefpFinalAddress, data, PGSZ, __func__);
        
                if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < PGSZ; i ++)
                      sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev, "WritePage(Actual) Write: bar iefpFinalAddress=%08o writeData=%012llo\n", cpu.iefpFinalAddress + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < PGSZ; i ++)
                  hdbgMWrite (cpu.iefpFinalAddress + i, data [i]);
#endif
        
                return SCPE_OK;
              }
            else
              {
                cpu.iefpFinalAddress = doAppendCycle (address, cyctyp);
                core_writeN (cpu.iefpFinalAddress, data, PGSZ, __func__);
        
                if_sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev)
                  {
                    for (uint i = 0; i < PGSZ; i ++)
                      sim_debug (DBG_APPENDING | DBG_FINAL, & cpu_dev, "WritePage(Actual) Write: iefpFinalAddress=%08o writeData=%012llo\n", cpu.iefpFinalAddress + i, data [i]);
                  }
#ifdef HDBG
                for (uint i = 0; i < PGSZ; i ++)
                  hdbgMWrite (cpu.iefpFinalAddress + i, data [i]);
#endif
        
                return SCPE_OK;
              }
          }
      }
    return SCPE_UNK;
  }


