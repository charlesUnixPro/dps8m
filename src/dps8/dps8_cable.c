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
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_console.h"
#include "dps8_disk.h"
#include "dps8_fnp2.h"
#include "dps8_dn6600.h"
#include "dps8_urp.h"
#include "dps8_crdrdr.h"
#include "dps8_crdpun.h"
#include "dps8_prt.h"
#ifndef __MINGW64__
#include "dps8_absi.h"
#endif
#include "dps8_cable.h"
#include "dps8_utils.h"
#ifdef M_SHARED
#include <unistd.h>
#include "shm.h"
#endif

#define DBG_CTR 1

struct cables_t * cables = NULL;

#ifdef THREADZ
char * devTypeStrs [/* devType */] =
  {
    "none", "tape", "console", "disk", "mpc", 
    "dn355", "card reader","card punch", "printer", "urp"
  };
#endif

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

static t_stat cable_to_iom (int uncable, uint iomUnitIdx, int chanNum,
                            int dev_code, devType devType, chanType ctype, 
                            uint devUnitIdx, DEVICE * devp, UNIT * unitp, 
                            iomCmd * iomCmd)
  {
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

    if (uncable)
      {
        struct device * d = 
          & cables->cablesFromIomToDev[iomUnitIdx].devices[chanNum][dev_code];

        if (d->type != devType)
          {
            sim_printf ("cable_to_iom: IOM socket type wrong; not uncabling\n");
            return SCPE_ARG;
          }
        d->type = DEVT_NONE;
        d->ctype = 0;
        d->devUnitIdx = 0;
        d->dev = 0;
        d->board  = 0;
        d->iomCmd  = NULL;
      }
    else
      {
        struct device * d = 
          & cables->cablesFromIomToDev[iomUnitIdx].devices[chanNum][dev_code];

        if (d->type != DEVT_NONE)
          {
            sim_printf ("cable_to_iom: IOM socket in use: IOM unit number %d, "
                        "channel number %d. (%o), device code %d. (%o)\n",
                        iomUnitIdx, chanNum, chanNum, dev_code, dev_code);
            return SCPE_ARG;
          }
        d->type = devType;
        d->ctype = ctype;
        d->devUnitIdx = devUnitIdx;
        d->dev = devp;
        d->board  = unitp;
        d->iomCmd  = iomCmd;
      }
    return SCPE_OK;
  }


// A scu is trying to attach a cable to a cpu.
//  cpu port cpuUnitIdx, cpu_port_num
//  from  scu_unit_idx, scu_port_num, scu_subport_num
//

static t_stat cable_to_cpu (int uncable, int cpuUnitIdx, int cpu_port_num, 
                            int scu_unit_idx, int scu_port_num,
                            int scu_subport_num)
  {
    if (cpuUnitIdx < 0 || cpuUnitIdx >= (int) cpu_dev.numunits)
      {
        sim_printf ("cable_to_cpu: cpuUnitIdx out of range <%d>\n", 
                    cpuUnitIdx);
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

    if (uncable)
      {
        if (! cables->cablesFromScuToCpu[cpuUnitIdx].ports[cpu_port_num].inuse)
          {
            sim_printf ("cable_to_cpu: CPU socket not in use\n");
            return SCPE_ARG;
          }

        struct cpuPort * p = 
          & cables->cablesFromScuToCpu[cpuUnitIdx].ports[cpu_port_num];

        p->inuse = false;
        p->scu_unit_idx = 0;
        p->scu_port_num = 0;
            p->scu_subport_num = 0;
        p->devp = NULL;

      }
    else
      {
        if (cables->cablesFromScuToCpu[cpuUnitIdx].ports[cpu_port_num].inuse)
          {
            sim_printf ("cable_to_cpu: CPU socket in use; unit number %d, "
                        "port number %d\n", cpuUnitIdx, cpu_port_num);
            return SCPE_ARG;
          }

        struct cpuPort * p = 
          & cables->cablesFromScuToCpu[cpuUnitIdx].ports[cpu_port_num];

        p->inuse = true;
        p->scu_unit_idx = scu_unit_idx;
        p->scu_port_num = scu_port_num;
            p->scu_subport_num = scu_subport_num;
        p->devp = & scu_dev;

      }

    // Taking this out breaks the unit test segment loader.
    setup_scbank_map ();

    return SCPE_OK;
  }




static t_stat cable_periph (int uncable, int unit_num, int iomUnitIdx,
                            int chan_num, int dev_code, char * service,
                            int numunits, struct cableFromIom * from,
                            devType dev_type, chanType chan_type,
                            DEVICE * devp, UNIT * unitp, iomCmd * iomCmd)
  {
    if (unit_num < 0 || unit_num >= numunits)
      {
        sim_printf ("%s: unit_num out of range <%d>\n", 
                    service, unit_num);
        return SCPE_ARG;
      }

    if (uncable)
      {
        if (from->iomUnitIdx != iomUnitIdx)
          {
            sim_printf ("%s: Wrong IOM expected %d, found %d\n",
                        service, iomUnitIdx, from->iomUnitIdx);
            return SCPE_ARG;
          }

        // Unplug the other end of the cable
        t_stat rc = cable_to_iom (uncable, (uint) iomUnitIdx, chan_num,
                                  dev_code, dev_type, 
                                  chan_type, (uint) unit_num, devp, unitp,
                                  iomCmd);
        if (rc)
          {
            sim_printf ("%s: IOM socket error; not uncabling unit number %d. "
                        "(%o)\n", service, unit_num, unit_num);
            return rc;
          }

        from->iomUnitIdx = -1;
        from->chan_num = 0;
        from->dev_code = 0;
      }
    else
      {
        if (from->iomUnitIdx != -1)
          {
            sim_printf ("%s: socket in use; unit number %d. (%o); "
                        "not cabling.\n", service, unit_num, unit_num);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = cable_to_iom (uncable, (uint) iomUnitIdx, chan_num,
                                  dev_code, dev_type, chan_type,
                                  (uint) unit_num, devp, unitp, iomCmd);
        if (rc)
          {
            sim_printf ("%s: IOM socket error; not cabling Card reader unit "
                        "number %d. (%o)\n", service, unit_num, unit_num);
            return rc;
          }

        from->iomUnitIdx = iomUnitIdx;
        from->chan_num = chan_num;
        from->dev_code = dev_code;
      }

    return SCPE_OK;
  }



static t_stat cable_crdrdr (int uncable, int crdrdr_unit_num, int iomUnitIdx,
                            int chan_num, int dev_code)
  {
    cable_periph (uncable, crdrdr_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_crdrdr", (int) crdrdr_dev.numunits, 
                  & cables->cablesFromIomToCrdRdr[crdrdr_unit_num],
                  DEVT_CRDRDR, chanTypePSI, & crdrdr_dev,
                  & crdrdr_unit[crdrdr_unit_num], crdrdr_iom_cmd);

    return SCPE_OK;
  }

static t_stat cable_crdpun (int uncable, int crdpun_unit_num, int iomUnitIdx,
                            int chan_num, int dev_code)
  {
    cable_periph (uncable, crdpun_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_crdpun", (int) crdpun_dev.numunits, 
                  & cables->cablesFromIomToCrdPun[crdpun_unit_num],
                  DEVT_CRDPUN, chanTypePSI, & crdrdr_dev,
                  & crdpun_unit[crdpun_unit_num], crdpun_iom_cmd);

    return SCPE_OK;
  }

static t_stat cable_prt (int uncable, int prt_unit_num, int iomUnitIdx,
                         int chan_num, int dev_code)
  {
    cable_periph (uncable, prt_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_prt", (int) prt_dev.numunits, 
                  & cables->cablesFromIomToPrt[prt_unit_num], DEVT_PRT,
                  chanTypePSI, & prt_dev, & prt_unit[prt_unit_num],
                  prt_iom_cmd);
    return SCPE_OK;
  }

static t_stat cable_urp (int uncable, int urp_unit_num, int iomUnitIdx,
                         int chan_num, int dev_code)
  {
    cable_periph (uncable, urp_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_urp", (int) urp_dev.numunits, 
                  & cables->cablesFromIomToUrp[urp_unit_num], DEVT_URP,
                 chanTypePSI, & urp_dev, & urp_unit[urp_unit_num], urp_iom_cmd);
    return SCPE_OK;
  }

static t_stat cableFNP (int uncable, int fnpUnitNum, int iomUnitIdx,
                        int chan_num, int dev_code)
  {
    cable_periph (uncable, fnpUnitNum, iomUnitIdx, chan_num, dev_code,
                  "cableFNP", (int) fnpDev.numunits, 
                  & cables->cablesFromIomToFnp[fnpUnitNum], DEVT_DN355,
                  chanTypeDirect, & fnpDev, & fnp_unit[fnpUnitNum], fnpIOMCmd);
    return SCPE_OK;
  }
 
static t_stat cable_dn6600 (int uncable, int dn6600_unit_num, int iom_unit_idx,
                            int chan_num, int dev_code)
  {
sim_printf ("cable_dn6600 %o %o\r\n", chan_num, dev_code);
    cable_periph (uncable, dn6600_unit_num, iom_unit_idx, chan_num, dev_code,
                  "cable_dn6600", (int) dn6600_dev.numunits, 
                  & cables->cables_from_iom_to_dn6600[dn6600_unit_num], DEVT_DN6600,
                  chanTypeDirect, & dn6600_dev, & dn6600_unit[dn6600_unit_num], dn6600_iom_cmd);
    return SCPE_OK;
  }
 
static t_stat cable_disk (int uncable, int disk_unit_num, int iomUnitIdx,
                          int chan_num, int dev_code)
  {
    cable_periph (uncable, disk_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_disk", (int) disk_dev.numunits, 
                  & cables->cablesFromIomToDsk[disk_unit_num], DEVT_DISK,
                  chanTypePSI, & disk_dev, & disk_unit[disk_unit_num],
                  disk_iom_cmd);
    return SCPE_OK;
  }

static t_stat cable_opcon (int uncable, int con_unit_num, int iomUnitIdx,
                           int chan_num, int dev_code)
  {
    cable_periph (uncable, con_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_opcon", (int) opcon_dev.numunits, 
                  & cables->cablesFromIomToCon[con_unit_num], DEVT_CON,
                  chanTypeCPI, & opcon_dev, & opcon_unit[con_unit_num],
                  con_iom_cmd);

    return SCPE_OK;
  }

static t_stat cable_scu (int uncable, int scu_unit_idx, int scu_port_num,
                         int cpuUnitIdx, int cpu_port_num)
  {
    sim_debug (DBG_DEBUG, & scu_dev, 
               "cable_scu: scu_unit_idx: %d, scu_port_num: %d, "
               "cpuUnitIdx: %d, cpu_port_num: %d\n", 
               scu_unit_idx, scu_port_num, cpuUnitIdx, cpu_port_num);
    if (scu_unit_idx < 0 || scu_unit_idx >= (int) scu_dev.numunits)
      {
        sim_printf ("cable_scu: scu_unit_idx out of range <%d>\n", 
                    scu_unit_idx);
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

    if (uncable)
      {
        if (cables->cablesFromCpus[scu_unit_idx][scu_port_num]
            [scu_subport_num].cpuUnitIdx != cpuUnitIdx)
          {
            sim_printf ("cable_scu: wrong CPU; unit number %d. (%o); not "
                        "uncabling.\n", scu_unit_idx, scu_unit_idx);
            return SCPE_ARG;
          }

        // Unplug the other end of the cable 
        t_stat rc = cable_to_cpu (uncable, cpuUnitIdx, cpu_port_num,
                                  scu_unit_idx, scu_port_num, scu_subport_num);
        if (rc)
          {
            sim_printf ("cable_scu: IOM socket error; not uncabling SCU unit "
                        "number %d. (%o)\n", scu_unit_idx, scu_unit_idx);
            return rc;
          }

        cables->cablesFromCpus[scu_unit_idx][scu_port_num]
          [scu_subport_num].cpuUnitIdx = -1;
        cables->cablesFromCpus[scu_unit_idx][scu_port_num]
          [scu_subport_num].cpu_port_num = -1;

        scu[scu_unit_idx].ports[scu_port_num].type = 0;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = 0;
// XXX is this wrong? is is_exp supposed to be an accumulation of bits?
        scu[scu_unit_idx].ports[scu_port_num].is_exp = 0;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[scu_subport_num] = 0;
      }
    else
      {
        if (cables->cablesFromCpus[scu_unit_idx][scu_port_num]
              [scu_subport_num].cpuUnitIdx != -1)
          {
            sim_printf ("cable_scu: SCU socket in use; unit number %d. "
                        "(%o); uncabling.\n", scu_unit_idx, scu_unit_idx);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = cable_to_cpu (uncable, cpuUnitIdx, cpu_port_num,
                                  scu_unit_idx, scu_port_num, scu_subport_num);
        if (rc)
          {
            sim_printf ("cable_scu: IOM socket error; uncabling SCU unit "
                        "number %d. (%o)\n", scu_unit_idx, scu_unit_idx);
            return rc;
          }

        cables->cablesFromCpus[scu_unit_idx][scu_port_num]
          [scu_subport_num].cpuUnitIdx = cpuUnitIdx;
        cables->cablesFromCpus[scu_unit_idx][scu_port_num]
          [scu_subport_num].cpu_port_num = cpu_port_num;

        scu[scu_unit_idx].ports[scu_port_num].type = ADEV_CPU;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = cpuUnitIdx;
// XXX is this right? is is_exp supposed to be an accumulation of bits? If so,
// change to a ref. count so uncable will work.
        scu[scu_unit_idx].ports[scu_port_num].is_exp |= is_exp;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[scu_subport_num] =
          cpu_port_num;
      }
    return SCPE_OK;
  }

//  An IOM is trying to attach a cable 
//  to SCU port scu_unit_idx, scu_port_num
//  from it's port iomUnitIdx, iom_port_num
//

static t_stat cable_to_scu (int uncable, int scu_unit_idx, int scu_port_num,
                            int iomUnitIdx, int iom_port_num)
  {
    sim_debug (DBG_DEBUG, & scu_dev, 
               "cable_to_scu: scu_unit_idx: %d, scu_port_num: %d, "
               "iomUnitIdx: %d, iom_port_num: %d\n", 
               scu_unit_idx, scu_port_num, iomUnitIdx, iom_port_num);

    if (scu_unit_idx < 0 || scu_unit_idx >= (int) scu_dev.numunits)
      {
        sim_printf ("cable_to_scu: scu_unit_idx out of range <%d>\n", 
                    scu_unit_idx);
        return SCPE_ARG;
      }

    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
      {
        sim_printf ("cable_to_scu: scu_port_num out of range <%d>\n", 
                    scu_port_num);
        return SCPE_ARG;
      }

    if (uncable)
      {
        if (scu[scu_unit_idx].ports[scu_port_num].type != ADEV_IOM)
          {
            sim_printf ("cable_to_scu: wrong SCU socket; unit number %d, "
                        "port number %d\n", scu_unit_idx, scu_port_num);
            return SCPE_ARG;
          }

        scu[scu_unit_idx].ports[scu_port_num].type = ADEV_NONE;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = 0;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[0] = 0;
        scu[scu_unit_idx].ports[scu_port_num].is_exp = false;
      }
    else
      {
        if (scu[scu_unit_idx].ports[scu_port_num].type != ADEV_NONE)
          {
            sim_printf ("cable_to_scu: SCU socket in use; unit number %d, "
                        "port number %d\n", scu_unit_idx, scu_port_num);
            return SCPE_ARG;
          }

        scu[scu_unit_idx].ports[scu_port_num].type = ADEV_IOM;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = iomUnitIdx;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[0] = iom_port_num;
        scu[scu_unit_idx].ports[scu_port_num].is_exp = false;
      }

    return SCPE_OK;
  }

static t_stat cable_iom (int uncable, uint iomUnitIdx, int iomPortNum,
                         int scuUnitIdx, int scuPortNum)
  {
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

    if (uncable)
      {
        if (! cables->cablesFromScus[iomUnitIdx][iomPortNum].inuse)
          {
            sim_printf ("cable_iom: wrong IOM socket; unit number %d. "
                        "(%o); not uncabling.\n", iomUnitIdx, iomUnitIdx);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = cable_to_scu (uncable, scuUnitIdx, scuPortNum,
                                  (int) iomUnitIdx, iomPortNum);
        if (rc)
          {
            sim_printf ("cable_iom: SCU socket error; not uncabling IOM unit "
                        "number %d. (%o), port number %d. (%o)\n",
                        iomUnitIdx, iomUnitIdx, iomPortNum, iomPortNum);
            return rc;
          }

        cables->cablesFromScus[iomUnitIdx][iomPortNum].inuse = false;
        cables->cablesFromScus[iomUnitIdx][iomPortNum].scuUnitIdx = 0;
        cables->cablesFromScus[iomUnitIdx][iomPortNum].scuPortNum = 0;
      }
    else
      {
        if (cables->cablesFromScus[iomUnitIdx][iomPortNum].inuse)
          {
            sim_printf ("cable_iom: IOM socket in use; unit number %d. "
                        "(%o); uncabling.\n", iomUnitIdx, iomUnitIdx);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = cable_to_scu (uncable, scuUnitIdx, scuPortNum,
                                  (int) iomUnitIdx, iomPortNum);
        if (rc)
          {
            sim_printf ("cable_iom: SCU socket error; uncabling IOM unit "
                        "number %d. (%o), port number %d. (%o)\n",
                        iomUnitIdx, iomUnitIdx, iomPortNum, iomPortNum);
            return rc;
          }

        cables->cablesFromScus[iomUnitIdx][iomPortNum].inuse = true;
        cables->cablesFromScus[iomUnitIdx][iomPortNum].scuUnitIdx = scuUnitIdx;
        cables->cablesFromScus[iomUnitIdx][iomPortNum].scuPortNum = scuPortNum;
      }

    return SCPE_OK;
  }

//
// String a cable from a tape drive to an IOM
//
// This end: mt_unit_num
// That end: iomUnitIdx, chan_num, dev_code
// 

static t_stat cable_mt (int uncable, int mt_unit_num, int iomUnitIdx,
                        int chan_num, int dev_code)
  {
    cable_periph (uncable, mt_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_mt", (int) tape_dev.numunits, 
                  & cables->cablesFromIomToTap[mt_unit_num], DEVT_TAPE,
                  chanTypePSI, & tape_dev, & mt_unit[mt_unit_num], mt_iom_cmd);

    return SCPE_OK;
  }
 
#ifndef __MINGW64__
//
// String a cable from a ABSI to an IOM
//
// This end: asbi_unit_num
// That end: iomUnitIdx, chan_num, dev_code
// 

static t_stat cable_absi (int uncable, int absi_unit_num, int iomUnitIdx,
                          int chan_num, int dev_code)
  {
    cable_periph (uncable, absi_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_absi", (int) absi_dev.numunits, 
                  & cables->cablesFromIomToAbsi[absi_unit_num], DEVT_ABSI,
                  chanTypePSI, & absi_dev, & absi_unit[absi_unit_num],
                  absi_iom_cmd);
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
        sim_printf ("error: sys_cable: can't parse %s\n", text);
        return -1;
      }
    if (strlen (value) == 1 && value[0] >= 'a' && value[0] <= 'z')
      return (int) (value[0] - 'a');
    if (strlen (value) == 1 && value[0] >= 'A' && value[0] <= 'Z')
      return (int) (value[0] - 'A');
    long l = strtol (value, & endptr, 0);
    if (* endptr || l < 0 || l > INT_MAX)
      {
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
//   cable IOM <iomUnitIdx>,<iom_port_num>,<scu_unit_idx>,<scu_port_num>
//
//   or scu to cpu
//
//   cable SCU <scu_unit_idx>,<scu_port_num>,<cpuUnitIdx>,<cpu_port_num>
//
//   or opcon to iom
//
//   cable OPCON <iomUnitIdx>,<chan_num>,0,0
//
//
// arg 0: cable
// arg 1: uncable


t_stat sys_cable (int32 arg, const char * buf)
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
        rc = cable_mt (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "DISK") == 0)
      {
        rc = cable_disk (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "OPCON") == 0)
      {
        rc = cable_opcon (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "IOM") == 0)
      {
        rc = cable_iom (arg, (uint) n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "SCU") == 0)
      {
        rc = cable_scu (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "DN") == 0)
      {
        rc = cable_dn6600 (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "FNP") == 0)
      {
        rc = cableFNP (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "CRDRDR") == 0)
      {
        rc = cable_crdrdr (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "CRDPUN") == 0)
      {
        rc = cable_crdpun (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "PRT") == 0)
      {
        rc = cable_prt (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "URP") == 0)
      {
        rc = cable_urp (arg, n1, n2, n3, n4);
      }
#ifndef __MINGW64__
    else if (strcasecmp (name, "ABSI") == 0)
      {
        rc = cable_absi (arg, n1, n2, n3, n4);
      }
#endif
    else
      {
        sim_printf ("error: sys_cable: invalid switch name <%s>\n", name);
        goto exit;
      }

exit:
    free (copy);
    return rc;
  }

static void cable_init (void)
  {
    // sets cablesFromIomToDev[iomUnitIdx].devices[chanNum][dev_code].type
    //  to DEVT_NONE
    memset (cables, 0, sizeof (struct cables_t));
    for (int i = 0; i < N_MT_UNITS_MAX; i ++)
      {
        cables->cablesFromIomToTap[i].iomUnitIdx = -1;
      }
    for (int u = 0; u < N_SCU_UNITS_MAX; u ++)
      for (int p = 0; p < N_SCU_PORTS; p ++)
        for (int s = 0; s < N_SCU_SUBPORTS; s ++)
          cables->cablesFromCpus[u][p][s].cpuUnitIdx = -1; // not connected
    for (int i = 0; i < N_OPCON_UNITS_MAX; i ++)
      cables->cablesFromIomToCon[i].iomUnitIdx = -1;
    for (int i = 0; i < N_DISK_UNITS_MAX; i ++)
      cables->cablesFromIomToDsk[i].iomUnitIdx = -1;
    for (int i = 0; i < N_CRDRDR_UNITS_MAX; i ++)
      cables->cablesFromIomToCrdRdr[i].iomUnitIdx = -1;
    for (int i = 0; i < N_CRDPUN_UNITS_MAX; i ++)
      cables->cablesFromIomToCrdPun[i].iomUnitIdx = -1;
    for (int i = 0; i < N_PRT_UNITS_MAX; i ++)
      cables->cablesFromIomToPrt[i].iomUnitIdx = -1;
    for (int i = 0; i < N_URP_UNITS_MAX; i ++)
      cables->cablesFromIomToUrp[i].iomUnitIdx = -1;
    for (int i = 0; i < N_ABSI_UNITS_MAX; i ++)
      cables->cablesFromIomToAbsi[i].iomUnitIdx = -1;
    for (int i = 0; i < N_FNP_UNITS_MAX; i ++)
      cables->cablesFromIomToFnp[i].iomUnitIdx = -1;
    for (int i = 0; i < N_DN6600_UNITS_MAX; i ++)
      cables->cables_from_iom_to_dn6600[i].iomUnitIdx = -1;
  }

t_stat sys_cable_show (UNUSED int32 arg, UNUSED const char * buf)
  {
    for (int i = 0; i < N_CPU_UNITS_MAX; i ++)
      for (int j = 0; j < N_CPU_PORTS; j ++)
        sim_printf ("cpu %3o port %3o: inuse %d scu %d port %d subport %d\n",
                    i, j, 
                    cables->cablesFromScuToCpu[i].ports[j].inuse,
                    cables->cablesFromScuToCpu[i].ports[j].scu_unit_idx,
                    cables->cablesFromScuToCpu[i].ports[j].scu_port_num,
                    cables->cablesFromScuToCpu[i].ports[j].scu_subport_num);
    for (int i = 0; i < N_IOM_UNITS_MAX; i ++)
      for (int j = 0; j < N_IOM_PORTS; j ++)
        sim_printf ("cpu %3o port %3o: inuse %d scu %d port %d\n", i, j, 
                    cables->cablesFromScus[i][j].inuse,
                    cables->cablesFromScus[i][j].scuUnitIdx,
                    cables->cablesFromScus[i][j].scuPortNum);
    for (int i = 0; i < N_IOM_UNITS_MAX; i ++)
      for (int c = 0; c < MAX_CHANNELS; c ++)
        for (int d = 0; d < N_DEV_CODES; d ++)
          if (cables->cablesFromIomToDev[i].devices[c][d].type)
            {
               char * dt [] =
                 {
                   "DEVT_NONE", "DEVT_TAPE", "DEVT_CON", "DEVT_DISK", 
                   "DEVT_MPC", "DEVT_DN355", "DEVT_CRDRDR", "DEVT_CRDPUN",
                   "DEVT_PRT", "DEVT_URP", "DEVT_ABSI"};
               sim_printf ("iom %3o chan %3o dev %3o: %s unit %d devp %p "
                           "unitp %p cmdp %p\n", i, c, d,
                           dt[cables->cablesFromIomToDev[i].devices[c][d].type],
                           cables->cablesFromIomToDev[i].devices[c][d].
                             devUnitIdx,
                           cables->cablesFromIomToDev[i].devices[c][d].dev,
                           cables->cablesFromIomToDev[i].devices[c][d].board,
                           cables->cablesFromIomToDev[i].devices[c][d].iomCmd);
            }
    for (int i = 0; i < N_MT_UNITS_MAX; i ++)
      {
        if (cables->cablesFromIomToTap[i].iomUnitIdx != -1)
          {
            sim_printf ("tape %3o: iom %3o chan %3o dev %3o\n",
                        i,
                        cables->cablesFromIomToTap[i].iomUnitIdx,
                        cables->cablesFromIomToTap[i].chan_num,
                        cables->cablesFromIomToTap[i].dev_code);
          }
      }
    for (int i = 0; i < N_DISK_UNITS_MAX; i ++)
      {
        if (cables->cablesFromIomToDsk[i].iomUnitIdx != -1)
          {
            sim_printf ("disk %3o iom %3o chan %3o dev %3o\n",
                        i,
                        cables->cablesFromIomToDsk[i].iomUnitIdx,
                        cables->cablesFromIomToDsk[i].chan_num,
                        cables->cablesFromIomToDsk[i].dev_code);
          }
      }
    return SCPE_OK;
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
#ifdef M_SHARED
        cables = (struct cables_t *) create_shm ("cables", getsid (0),
                                                 sizeof (struct cables_t));
#else
        cables = (struct cables_t *) malloc (sizeof (struct cables_t));
#endif
        if (cables == NULL)
          {
            sim_printf ("create_shm cables failed\n");
            sim_err ("create_shm cables failed\n");
          }
      }

    // Initialize data structures
    cable_init ();
  }


