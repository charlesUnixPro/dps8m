/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include "dps8.h"
#include "dps8_simh.h"
#include "dps8_iom.h"
#include "dps8_mt.h"
#include "dps8_scu.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_faults.h"
#include "dps8_console.h"
#include "dps8_disk.h"
#include "dps8_fnp2.h"
#include "dps8_fnp2_iomcmd.h"
#include "dps8_urp.h"
#include "dps8_crdrdr.h"
#include "dps8_crdpun.h"
#include "dps8_prt.h"
#ifndef __MINGW64__
#include "dps8_absi.h"
#endif
#include "dps8_cable.h"
#include "dps8_utils.h"
struct cables_t * cables = NULL;

//
// cable_to_iom and cable_to_scu are the "other end handlers"; they are
// only called indirectly to secure the other end of the cable.
//



// cable_to_iom
//
// a peripheral is trying to attach a cable
//  to a IOM port [iomUnitIdx, chanNum, dev_code]
//  from their simh dev [devType, devUnitIdx]
//
// Verify that the port is unused; attach this end of the cable

static t_stat cable_to_iom (uint iomUnitIdx, int chanNum, int dev_code, 
                            devType devType, chanType ctype, 
                            uint devUnitIdx, DEVICE * devp, UNIT * unitp, 
                            iomCmd * iomCmd)
  {
    //if (iomUnitIdx >= iom_dev . numunits)
    if (iomUnitIdx >= N_IOM_UNITS_MAX)
      {
        sim_printf ("cable_to_iom: iomUnitIdx out of range <%u>\n", iomUnitIdx);
        return SCPE_ARG;
      }

    if (chanNum < 0 || chanNum >= MAX_CHANNELS)
      {
        sim_printf ("cable_to_iom: chanNum out of range <%d>\n", chanNum);
        return SCPE_ARG;
      }

    if (dev_code < 0 || dev_code >= N_DEV_CODES)
      {
        sim_printf ("cable_to_iom: dev_code out of range <%d>\n", dev_code);
        return SCPE_ARG;
      }

    struct device * d = 
      & cables -> cablesFromIomToDev [iomUnitIdx] . devices [chanNum] [dev_code];

    if (d -> type != DEVT_NONE)
      {
        sim_printf ("cable_to_iom: IOM socket in use: IOM unit number %d, channel number %d. (%o), device code %d. (%o)\n", iomUnitIdx, chanNum, chanNum, dev_code, dev_code);
        return SCPE_ARG;
      }
    d -> type = devType;
    d -> ctype = ctype;
    d -> devUnitIdx = devUnitIdx;
    d -> dev = devp;
    d -> board  = unitp;
    d -> iomCmd  = iomCmd;

    return SCPE_OK;
  }


// A scu is trying to attach a cable to a cpu.
//  cpu port cpu_unit_num, cpu_port_num
//  from  scu_unit_num, scu_port_num, scu_subport_num
//

static t_stat cable_to_cpu (int cpu_unit_num, int cpu_port_num, 
                            int scu_unit_num, int scu_port_num,
                            int scu_subport_num)
  {
    //if (cpu_unit_num < 0 || cpu_unit_num >= (int) cpu_dev . numunits)
    if (cpu_unit_num < 0 || cpu_unit_num >= N_CPU_UNITS_MAX)
      {
        sim_printf ("cable_to_cpu: cpu_unit_num out of range <%d>\n", 
                    cpu_unit_num);
        return SCPE_ARG;
      }

    if (cpu_port_num < 0 || cpu_port_num >= N_CPU_PORTS)
      {
        sim_printf ("cable_to_cpu: cpu_port_num out of range <%d>\n", 
                    cpu_port_num);
        return SCPE_ARG;
      }

    if (scu_subport_num < 0 || scu_subport_num >= N_SCU_SUBPORTS)
      {
        sim_printf ("cable_to_cpu: scu_subport_num out of range <%d>\n", 
                    scu_subport_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromScuToCpu [cpu_unit_num] . ports [cpu_port_num] . inuse)
      {
        sim_printf ("cable_to_cpu: CPU socket in use; unit number %d, port number %d\n", cpu_unit_num, cpu_port_num);
        return SCPE_ARG;
      }

    struct cpuPort * p = 
      & cables -> cablesFromScuToCpu [cpu_unit_num] . ports [cpu_port_num];

    p -> inuse = true;
    p -> scu_unit_num = scu_unit_num;
    p -> scu_port_num = scu_port_num;
    p -> scu_subport_num = scu_subport_num;
    p -> devp = & scu_dev;

    // Taking this out breaks the unit test segment loader.
    setup_scbank_map ();

    return SCPE_OK;
  }






static t_stat cable_crdrdr (int crdrdr_unit_num, int iomUnitIdx, int chan_num, 
                            int dev_code)
  {
    //if (crdrdr_unit_num < 0 || crdrdr_unit_num >= (int) crdrdr_dev . numunits)
    if (crdrdr_unit_num < 0 || crdrdr_unit_num >= N_CRDRDR_UNITS_MAX)
      {
        sim_printf ("cable_crdrdr: crdrdr_unit_num out of range <%d>\n", 
                    crdrdr_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_crdrdr: Card reader socket in use; unit number %d. (%o); uncabling.\n", crdrdr_unit_num, crdrdr_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_CRDRDR, 
                              chanTypePSI, (uint) crdrdr_unit_num, & crdrdr_dev, 
                              & crdrdr_unit [crdrdr_unit_num], crdrdr_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_crdrdr: IOM socket error; uncabling Card reader unit number %d. (%o)\n", crdrdr_unit_num, crdrdr_unit_num);
        return rc;
      }

    cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_crdpun (int crdpun_unit_num, int iomUnitIdx, int chan_num, 
                            int dev_code)
  {
    //if (crdpun_unit_num < 0 || crdpun_unit_num >= (int) crdpun_dev . numunits)
    if (crdpun_unit_num < 0 || crdpun_unit_num >= N_CRDPUN_UNITS_MAX)
      {
        sim_printf ("cable_crdpun: crdpun_unit_num out of range <%d>\n", 
                    crdpun_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToCrdPun [crdpun_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_crdpun: Card punch socket in use; unit number %d. (%o); uncabling.\n", crdpun_unit_num, crdpun_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_CRDPUN, 
                              chanTypePSI, (uint) crdpun_unit_num, & crdpun_dev, 
                              & crdpun_unit [crdpun_unit_num], crdpun_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_crdpun: IOM socket error; uncabling Card punch unit number %d. (%o)\n", crdpun_unit_num, crdpun_unit_num);
        return rc;
      }

    cables -> cablesFromIomToCrdPun [crdpun_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToCrdPun [crdpun_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToCrdPun [crdpun_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_prt (int prt_unit_num, int iomUnitIdx, int chan_num, 
                            int dev_code)
  {
    //if (prt_unit_num < 0 || prt_unit_num >= (int) prt_dev . numunits)
    if (prt_unit_num < 0 || prt_unit_num >= N_PRT_UNITS_MAX)
      {
        sim_printf ("cable_prt: prt_unit_num out of range <%d>\n", 
                    prt_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToPrt [prt_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_prt: Printer socket in use; unit number %d. (%o); uncabling.\n", prt_unit_num, prt_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_PRT, 
                              chanTypePSI, (uint) prt_unit_num, & prt_dev, 
                              & prt_unit [prt_unit_num], prt_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_prt: IOM socket error; uncabling Printer unit number %d. (%o)\n", prt_unit_num, prt_unit_num);
        return rc;
      }

    cables -> cablesFromIomToPrt [prt_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToPrt [prt_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToPrt [prt_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_urp (int urp_unit_num, int iomUnitIdx, int chan_num, 
                            int dev_code)
  {
    //if (urp_unit_num < 0 || urp_unit_num >= (int) urp_dev . numunits)
    if (urp_unit_num < 0 || urp_unit_num >= N_URP_UNITS_MAX)
      {
        sim_printf ("cable_urp: urp_unit_num out of range <%d>\n", 
                    urp_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToUrp [urp_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_urp: Unit Record Processor socket in use; unit number %d. (%o); uncabling.\n", urp_unit_num, urp_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_URP, 
                              chanTypePSI, (uint) urp_unit_num, & urp_dev, 
                              & urp_unit [urp_unit_num], urp_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_urp: IOM socket error; uncabling Unit Record Processor unit number %d. (%o)\n", urp_unit_num, urp_unit_num);
        return rc;
      }

    cables -> cablesFromIomToUrp [urp_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToUrp [urp_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToUrp [urp_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cableFNP (int fnpUnitNum, int iomUnitIdx, int chan_num, 
                        int dev_code)
  {
    //if (fnpUnitNum < 0 || fnpUnitNum >= (int) fnpDev . numunits)
    if (fnpUnitNum < 0 || fnpUnitNum >= N_FNP_UNITS_MAX)
      {
        sim_printf ("cableFNP: fnpUnitNum out of range <%d>\n", fnpUnitNum);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToFnp [fnpUnitNum] . iomUnitIdx != -1)
      {
        sim_printf ("cableFNP: FNP socket in use; unit number %d. (%o); uncabling.\n", fnpUnitNum, fnpUnitNum);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_DN355, 
                              chanTypeDirect, (uint) fnpUnitNum, & fnpDev, 
                              & fnp_unit [fnpUnitNum], fnpIOMCmd);
    if (rc)
      {
        sim_printf ("cableFNP: IOM socket error; uncabling FNP unit number %d. (%o)\n", fnpUnitNum, fnpUnitNum);
        return rc;
      }

    cables -> cablesFromIomToFnp [fnpUnitNum] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToFnp [fnpUnitNum] . chan_num = chan_num;
    cables -> cablesFromIomToFnp [fnpUnitNum] . dev_code = dev_code;

    return SCPE_OK;
  }
 
static t_stat cable_disk (int disk_unit_num, int iomUnitIdx, int chan_num, 
                          int dev_code)
  {
    //if (disk_unit_num < 0 || disk_unit_num >= (int) disk_dev . numunits)
    if (disk_unit_num < 0 || disk_unit_num >= N_DISK_UNITS_MAX)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_disk: disk_unit_num out of range <%d>\n", disk_unit_num);
        sim_printf ("cable_disk: disk_unit_num out of range <%d>\n", 
                    disk_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToDsk [disk_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_disk: Disk in use; unit number %d. (%o); uncabling.\n", disk_unit_num, disk_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_DISK, 
                              chanTypePSI, (uint) disk_unit_num, & disk_dev, 
                              & disk_unit [disk_unit_num], disk_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_disk: IOM socket error; uncabling Disk number %d. (%o)\n", disk_unit_num, disk_unit_num);
        return rc;
      }

    cables -> cablesFromIomToDsk [disk_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToDsk [disk_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToDsk [disk_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_opcon (int con_unit_num, int iomUnitIdx, int chan_num, 
                           int dev_code)
  {
    //if (con_unit_num < 0 || con_unit_num >= (int) opcon_dev . numunits)
    if (con_unit_num < 0 || con_unit_num >= N_OPCON_UNITS_MAX)
      {
        sim_printf ("cable_opcon: opcon_unit_num out of range <%d>\n", 
                    con_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToCon [con_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_opcon: Console socket in use; unit number %d. (%o); uncabling.\n", con_unit_num, con_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_CON, 
                              chanTypeCPI, (uint) con_unit_num, & opcon_dev, 
                              & opcon_unit [con_unit_num], con_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_opcon: IOM socket error; uncabling Console unit number %d. (%o)\n", con_unit_num, con_unit_num);
        return rc;
      }

    cables -> cablesFromIomToCon [con_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToCon [con_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToCon [con_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_scu (int scu_unit_num, int scu_port_num, int cpu_unit_num, 
                         int cpu_port_num)
  {
    sim_debug (DBG_DEBUG, & scu_dev, 
               "cable_scu: scu_unit_num: %d, scu_port_num: %d, "
               "cpu_unit_num: %d, cpu_port_num: %d\n", 
               scu_unit_num, scu_port_num, cpu_unit_num, cpu_port_num);
    //if (scu_unit_num < 0 || scu_unit_num >= (int) scu_dev . numunits)
    if (scu_unit_num < 0 || scu_unit_num >= N_SCU_UNITS_MAX)
      {
        sim_printf ("cable_scu: scu_unit_num out of range <%d>\n", 
                    scu_unit_num);
        return SCPE_ARG;
      }

// Encoding expansion port info into port number:
//   [0-7]: port number
//   [1-7][0-3]:  port number, sub port number.
// This won't let me put an expansion port on port 0, but documents
// say to put the CPUs on the high ports and the IOMs on the low, so
// there is no reason to put an expander on port 0.
//

    int scu_subport_num = 0;
    bool is_exp = false;
    int exp_port = scu_port_num / 10;
    if (exp_port)
      {
        scu_subport_num = scu_port_num % 10;
        if (scu_subport_num < 0 || scu_subport_num >= N_SCU_SUBPORTS)
          {
            sim_printf ("cable_scu: scu_subport_num out of range <%d>\n", 
                        scu_subport_num);
            return SCPE_ARG;
          }
        scu_port_num /= 10;
        is_exp = true;
      }
    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
      {
        sim_printf ("cable_scu: scu_port_num out of range <%d>\n", 
                    scu_port_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromCpus [scu_unit_num] [scu_port_num] [scu_subport_num] . cpu_unit_num != -1)
      {
        sim_printf ("cable_scu: SCU socket in use; unit number %d. (%o); uncabling.\n", scu_unit_num, scu_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_cpu (cpu_unit_num, cpu_port_num, scu_unit_num, 
                              scu_port_num, scu_subport_num);
    if (rc)
      {
        sim_printf ("cable_scu: IOM socket error; uncabling SCU unit number %d. (%o)\n", scu_unit_num, scu_unit_num);
        return rc;
      }

    cables -> cablesFromCpus [scu_unit_num] [scu_port_num] [scu_subport_num] . cpu_unit_num = 
      cpu_unit_num;
    cables -> cablesFromCpus [scu_unit_num] [scu_port_num] [scu_subport_num] . cpu_port_num = 
      cpu_port_num;

    scu [scu_unit_num] . ports [scu_port_num] . type = ADEV_CPU;
    scu [scu_unit_num] . ports [scu_port_num] . idnum = cpu_unit_num;
    scu [scu_unit_num] . ports [scu_port_num] . is_exp |= is_exp;
    scu [scu_unit_num] . ports [scu_port_num] . dev_port [scu_subport_num] = cpu_port_num;
//if (scu [scu_unit_num] . ports [scu_port_num] . is_exp) sim_printf ("%o.%o is expanded\n", scu_unit_num, scu_port_num);
    return SCPE_OK;
  }

//  An IOM is trying to attach a cable 
//  to SCU port scu_unit_num, scu_port_num
//  from it's port iomUnitIdx, iom_port_num
//

static t_stat cable_to_scu (int scu_unit_num, int scu_port_num, int iomUnitIdx, 
                     int iom_port_num)
  {
    sim_debug (DBG_DEBUG, & scu_dev, 
               "cable_to_scu: scu_unit_num: %d, scu_port_num: %d, "
               "iomUnitIdx: %d, iom_port_num: %d\n", 
               scu_unit_num, scu_port_num, iomUnitIdx, iom_port_num);

    //if (scu_unit_num < 0 || scu_unit_num >= (int) scu_dev . numunits)
    if (scu_unit_num < 0 || scu_unit_num >= N_SCU_UNITS_MAX)
      {
        sim_printf ("cable_to_scu: scu_unit_num out of range <%d>\n", 
                    scu_unit_num);
        return SCPE_ARG;
      }

    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
      {
        sim_printf ("cable_to_scu: scu_port_num out of range <%d>\n", 
                    scu_port_num);
        return SCPE_ARG;
      }

    if (scu [scu_unit_num] . ports [scu_port_num] . type != ADEV_NONE)
      {
        sim_printf ("cable_to_scu: SCU socket in use; unit number %d, port number %d\n", scu_unit_num, scu_port_num);
        return SCPE_ARG;
      }

    scu [scu_unit_num] . ports [scu_port_num] . type = ADEV_IOM;
    scu [scu_unit_num] . ports [scu_port_num] . idnum = iomUnitIdx;
    scu [scu_unit_num] . ports [scu_port_num] . dev_port [0] = iom_port_num;
    scu [scu_unit_num] . ports [scu_port_num] . is_exp = false;

    return SCPE_OK;
  }

static t_stat cable_iom (uint iomUnitIdx, int iomPortNum, int scuUnitNum, 
                         int scuPortNum)
  {
    //if (iomUnitIdx >= iom_dev . numunits)
    if (iomUnitIdx >= N_IOM_UNITS_MAX)
      {
        sim_printf ("cable_iom: iomUnitIdx out of range <%d>\n", iomUnitIdx);
        return SCPE_ARG;
      }

    if (iomPortNum < 0 || iomPortNum >= N_IOM_PORTS)
      {
        sim_printf ("cable_iom: iomPortNum out of range <%d>\n", iomUnitIdx);
        return SCPE_ARG;
      }

    if (cables -> cablesFromScus [iomUnitIdx] [iomPortNum] . inuse)
      {
        sim_printf ("cable_iom: IOM socket in use; unit number %d. (%o); uncabling.\n", iomUnitIdx, iomUnitIdx);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_scu (scuUnitNum, scuPortNum, (int) iomUnitIdx, iomPortNum);
    if (rc)
      {
        sim_printf ("cable_iom: SCU socket error; uncabling IOM unit number %d. (%o), port number %d. (%o)\n", iomUnitIdx, iomUnitIdx, iomPortNum, iomPortNum);
        return rc;
      }

    cables -> cablesFromScus [iomUnitIdx] [iomPortNum] . inuse = true;
    cables -> cablesFromScus [iomUnitIdx] [iomPortNum] . scuUnitNum = scuUnitNum;
    cables -> cablesFromScus [iomUnitIdx] [iomPortNum] . scuPortNum = scuPortNum;

    return SCPE_OK;
  }

//
// String a cable from a tape drive to an IOM
//
// This end: mt_unit_num
// That end: iomUnitIdx, chan_num, dev_code
// 

static t_stat cable_mt (int mt_unit_num, int iomUnitIdx, int chan_num, 
                        int dev_code)
  {
    //if (mt_unit_num < 0 || mt_unit_num >= (int) tape_dev . numunits)
    if (mt_unit_num < 0 || mt_unit_num >= N_MT_UNITS_MAX)
      {
        sim_printf ("cable_mt: mt_unit_num out of range <%d>\n", mt_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToTap [mt_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_mt: Tape socket in use; unit number %d. (%o); uncabling.\n", mt_unit_num, mt_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_TAPE, 
                              chanTypePSI, (uint) mt_unit_num, & tape_dev, 
                              & mt_unit [mt_unit_num], mt_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_mt: IOM socket error; uncabling Tape unit number %d. (%o)\n", mt_unit_num, mt_unit_num);
        return rc;
      }

    cables -> cablesFromIomToTap [mt_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToTap [mt_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToTap [mt_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }
 
#ifndef __MINGW64__
//
// String a cable from a ABSI to an IOM
//
// This end: asbi_unit_num
// That end: iomUnitIdx, chan_num, dev_code
// 

static t_stat cable_absi (int absi_unit_num, int iomUnitIdx, int chan_num, 
                          int dev_code)
  {
    //if (absi_unit_num < 0 || absi_unit_num >= (int) absi_dev . numunits)
    if (absi_unit_num < 0 || absi_unit_num >= N_ABSI_UNITS_MAX)
      {
        sim_printf ("cable_absi: absi_unit_num out of range <%d>\n", absi_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToAbsi [absi_unit_num] . iomUnitIdx != -1)
      {
        sim_printf ("cable_absi: ABSI socket in use; unit number %d. (%o); uncabling.\n", absi_unit_num, absi_unit_num);
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom ((uint) iomUnitIdx, chan_num, dev_code, DEVT_TAPE, 
                              chanTypePSI, (uint) absi_unit_num, & tape_dev, 
                              & absi_unit [absi_unit_num], absi_iom_cmd);
    if (rc)
      {
        sim_printf ("cable_absi: IOM socket error; uncabling ABSI unit number %d. (%o)\n", absi_unit_num, absi_unit_num);
        return rc;
      }

    cables -> cablesFromIomToAbsi [absi_unit_num] . iomUnitIdx = iomUnitIdx;
    cables -> cablesFromIomToAbsi [absi_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToAbsi [absi_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }
#endif 

static int getval (char * * save, char * text)
  {
    char * value;
    char * endptr;
    value = strtok_r (NULL, ",", save);
    if (! value)
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: can't parse %s\n", text);
        sim_printf ("error: sys_cable: can't parse %s\n", text);
        return -1;
      }
    long l = strtol (value, & endptr, 0);
    if (* endptr || l < 0 || l > INT_MAX)
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: can't parse %s <%s>\n", text, value);
        sim_printf ("error: sys_cable: can't parse %s <%s>\n", text, value);
        return -1;
      }
    return (int) l;
  }

// Connect dev to iom
//
//   cable [TAPE | DISK],<dev_unit_num>,<iomUnitIdx>,<chan_num>,<dev_code>
//
//   or iom to scu
//
//   cable IOM <iomUnitIdx>,<iom_port_num>,<scu_unit_num>,<scu_port_num>
//
//   or scu to cpu
//
//   cable SCU <scu_unit_num>,<scu_port_num>,<cpu_unit_num>,<cpu_port_num>
//
//   or opcon to iom
//
//   cable OPCON <iomUnitIdx>,<chan_num>,0,0
//


t_stat sys_cable (UNUSED int32 arg, const char * buf)
  {
// XXX Minor bug; this code doesn't check for trailing garbage

    char * copy = strdup (buf);
    t_stat rc = SCPE_ARG;

    // process statement

    // extract name
    char * name_save = NULL;
    char * name;
    name = strtok_r (copy, ",", & name_save);
    if (! name)
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: can't parse name\n");
        sim_printf ("error: sys_cable: can't parse name\n");
        goto exit;
      }


    int n1 = getval (& name_save, "parameter 1");
    if (n1 < 0)
      goto exit;
    int n2 = getval (& name_save, "parameter 2");
    if (n2 < 0)
      goto exit;
    int n3 = getval (& name_save, "parameter 3");
    if (n3 < 0)
      goto exit;
    int n4 = getval (& name_save, "parameter 4");
    if (n4 < 0)
      goto exit;


    if (strcasecmp (name, "TAPE") == 0)
      {
        rc = cable_mt (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "DISK") == 0)
      {
        rc = cable_disk (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "OPCON") == 0)
      {
        rc = cable_opcon (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "IOM") == 0)
      {
        rc = cable_iom ((uint) n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "SCU") == 0)
      {
        rc = cable_scu (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "FNP") == 0)
      {
        rc = cableFNP (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "CRDRDR") == 0)
      {
        rc = cable_crdrdr (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "CRDPUN") == 0)
      {
        rc = cable_crdpun (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "PRT") == 0)
      {
        rc = cable_prt (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "URP") == 0)
      {
        rc = cable_urp (n1, n2, n3, n4);
      }
#ifndef __MINGW64__
    else if (strcasecmp (name, "ABSI") == 0)
      {
        rc = cable_absi (n1, n2, n3, n4);
      }
#endif
    else
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: Invalid switch name <%s>\n", name);
        sim_printf ("error: sys_cable: invalid switch name <%s>\n", name);
        goto exit;
      }

exit:
    free (copy);
    return rc;
  }

static void cable_init (void)
  {
    // sets cablesFromIomToDev [iomUnitIdx] . devices [chanNum] [dev_code] . type to DEVT_NONE
    memset (cables, 0, sizeof (struct cables_t));
    for (int i = 0; i < N_MT_UNITS_MAX; i ++)
      {
        cables -> cablesFromIomToTap [i] . iomUnitIdx = -1;
      }
    for (int u = 0; u < N_SCU_UNITS_MAX; u ++)
      for (int p = 0; p < N_SCU_PORTS; p ++)
        for (int s = 0; s < N_SCU_SUBPORTS; s ++)
          cables -> cablesFromCpus [u] [p] [s] . cpu_unit_num = -1; // not connected
    for (int i = 0; i < N_OPCON_UNITS_MAX; i ++)
      cables -> cablesFromIomToCon [i] . iomUnitIdx = -1;
    for (int i = 0; i < N_DISK_UNITS_MAX; i ++)
      cables -> cablesFromIomToDsk [i] . iomUnitIdx = -1;
    for (int i = 0; i < N_CRDRDR_UNITS_MAX; i ++)
      cables -> cablesFromIomToCrdRdr [i] . iomUnitIdx = -1;
    for (int i = 0; i < N_CRDPUN_UNITS_MAX; i ++)
      cables -> cablesFromIomToCrdPun [i] . iomUnitIdx = -1;
    for (int i = 0; i < N_PRT_UNITS_MAX; i ++)
      cables -> cablesFromIomToPrt [i] . iomUnitIdx = -1;
    for (int i = 0; i < N_URP_UNITS_MAX; i ++)
      cables -> cablesFromIomToUrp [i] . iomUnitIdx = -1;
    for (int i = 0; i < N_ABSI_UNITS_MAX; i ++)
      cables -> cablesFromIomToAbsi [i] . iomUnitIdx = -1;
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      cables -> cablesFromIomToFnp [i] . iomUnitIdx = -1;
  }

t_stat sys_cable_ripout (UNUSED int32 arg, UNUSED const char * buf)
  {
    cable_init ();
    scu_init ();
    return SCPE_OK;
  }

void sysCableInit (void)
  {
    if (! cables)
      {
        cables = (struct cables_t *) malloc (sizeof (struct cables_t));
        if (cables == NULL)
          {
            sim_printf ("create_shm cables failed\n");
            sim_err ("create_shm cables failed\n");
          }
      }

    // Initialize data structures
    cable_init ();
  }
