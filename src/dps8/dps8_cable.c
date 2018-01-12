// XXX XXX XXX add code to new cable to check for end of line (strtok_r fail)
/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#include <ctype.h>

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
#ifdef NEW_CABLE
struct kables_s * kables = NULL;
#endif

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
            sim_printf ("%s: Card reader socket in use; unit number %d. (%o); "
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
 
static t_stat cable_disk (int uncable, int dsk_unit_num, int iomUnitIdx,
                          int chan_num, int dev_code)
  {
    cable_periph (uncable, dsk_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_disk", (int) dsk_dev.numunits, 
                  & cables->cablesFromIomToDsk[dsk_unit_num], DEVT_DISK,
                  chanTypePSI, & dsk_dev, & dsk_unit[dsk_unit_num],
                  dsk_iom_cmd);
    return SCPE_OK;
  }

static t_stat cable_opc (int uncable, int opc_unit_num, int iomUnitIdx,
                           int chan_num, int dev_code)
  {
    cable_periph (uncable, opc_unit_num, iomUnitIdx, chan_num, dev_code,
                  "cable_opc", (int) opc_dev.numunits, 
                  & cables->cablesFromIomToCon[opc_unit_num], DEVT_CON,
                  chanTypeCPI, & opc_dev, & opc_unit[opc_unit_num],
                  opc_iom_cmd);

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

static int parseval (char * value)
  {
    if (! value)
      return -1;
    if (strlen (value) == 1 && value[0] >= 'a' && value[0] <= 'z')
      return (int) (value[0] - 'a');
    if (strlen (value) == 1 && value[0] >= 'A' && value[0] <= 'Z')
      return (int) (value[0] - 'A');
    char * endptr;
    long l = strtol (value, & endptr, 0);
    if (* endptr || l < 0 || l > INT_MAX)
      {
        sim_printf ("error: CABLE: can't parse %s\n", value);
        return -1;
      }
    return (int) l;
  }

static int getval (char * * save, char * text)
  {
    char * value;
    value = strtok_r (NULL, ", ", save);
    if (! value)
      {
        sim_printf ("error: CABLE: can't parse %s\n", text);
        return -1;
      }
    return parseval (value);
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
//   or opc to iom
//
//   cable OPC <iomUnitIdx>,<chan_num>,0,0
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
    else if (strcasecmp (name, "OPC") == 0)
      {
        rc = cable_opc (arg, n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "IOM") == 0)
      {
        rc = cable_iom (arg, (uint) n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "SCU") == 0)
      {
        rc = cable_scu (arg, n1, n2, n3, n4);
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

#ifdef NEW_CABLE

// Match "FOO" with "FOOxxx" where xx is a decimal number or [A-Za-z]
//  "IOM" : IOM0 IOMB iom15 IOMc
// On match return value of number of mapped character (a = 0, b = 1, ...)
// On fail, return -1;

static bool name_match (const char * str, const char * pattern, uint * val)
  {
    // Does str match pattern?
    size_t pat_len = strlen (pattern);
    if (strncasecmp (pattern, str, pat_len))
      return false;

    // Isolate the string past the pattern
    size_t rest = strlen (str) - pat_len;
    const char * p = str + pat_len;

    // Can't be empty
    if (! rest)   
      return false; // no tag

    // [A-Za-z]? XXX Assume a-z contiguous; won't work in EBCDIC
    char * q;
    char * tags = "abcdefghijklmnopqrstuvwxyz";
    if (rest == 1 && (q = strchr (tags, tolower (*p))))
      {
        * val = (uint) (q - tags);
        return true;
      }

    // Starts with a digit?
    char * digits = "0123456789";
    q = strchr (digits, tolower (*p));
    if (! q)
      return false; // start not a digit

    long l = strtol (p, & q, 0);
    if (* q || l < 0 || l > INT_MAX)
      {
        sim_printf ("error: sys_cable: can't parse %s\n", str);
        return false;
      }
    * val =  (uint) l;
    return true;
  }

// back cable SCUx port# IOMx port#

static t_stat back_cable_iom_to_scu (int uncable, uint iom_unit_idx, uint iom_port_num, uint scu_unit_idx, uint scu_port_num)
  {
    struct iom_to_scu_s * p = & kables->iom_to_scu[iom_unit_idx][iom_port_num];
    if (uncable)
      {
        p->in_use = false;
      }
    else
      {
        if (p->in_use)
          {
            sim_printf ("cable SCU: IOM%u port %u in use.\n", iom_unit_idx, iom_port_num);
             return SCPE_ARG;
          }
        p->in_use = true;
        p->scu_unit_idx = scu_unit_idx;
        p->scu_port_num = scu_port_num;
      }
    return SCPE_OK;
  }

// cable SCUx IOMx

static t_stat cable_scu_to_iom (int uncable, uint scu_unit_idx, uint scu_port_num, uint iom_unit_idx, uint iom_port_num)
  {
    struct scu_to_iom_s * p = & kables->scu_to_iom[scu_unit_idx][scu_port_num];
    if (uncable)
      {
        if (! p->in_use)
          {
            sim_printf ("uncable SCU%u port %d: not cabled\n", scu_unit_idx, scu_port_num);
            return SCPE_ARG;
          }

        // Unplug the other end of the cable 
        t_stat rc = back_cable_iom_to_scu (uncable, iom_unit_idx, iom_port_num,
                                  scu_unit_idx, scu_port_num);
        if (rc)
          {
            return rc;
          }

        p->in_use = false;
        scu[scu_unit_idx].ports[scu_port_num].type = ADEV_NONE;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = 0;
// XXX is this wrong? is is_exp supposed to be an accumulation of bits?
        scu[scu_unit_idx].ports[scu_port_num].is_exp = false;
        //scu[scu_unit_idx].ports[scu_port_num].dev_port[scu_subport_num] = 0;
      }
    else
      {
        if (p->in_use)
          {
            sim_printf ("cable_scu: SCU %d port %d in use.\n", scu_unit_idx, scu_port_num);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = back_cable_iom_to_scu (uncable, iom_unit_idx, iom_port_num,
                                  scu_unit_idx, scu_port_num);
        if (rc)
          {
            return rc;
          }

        p->in_use = true;
        p->iom_unit_idx = iom_unit_idx;
        p->iom_port_num = (uint) iom_port_num;

        scu[scu_unit_idx].ports[scu_port_num].type = ADEV_IOM;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = (int) iom_unit_idx;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[0] = (int) iom_port_num;
// XXX is this wrong? is is_exp supposed to be an accumulation of bits?
        scu[scu_unit_idx].ports[scu_port_num].is_exp = 0;
        //scu[scu_unit_idx].ports[scu_port_num].dev_port[scu_subport_num] = 0;
      }
    return SCPE_OK;
  }

// back cable SCUx port# CPUx port#

static t_stat back_cable_cpu_to_scu (int uncable, uint cpu_unit_idx, uint cpu_port_num, uint scu_unit_idx, uint scu_port_num, uint scu_subport_num)
  {
    struct cpu_to_scu_s * p = & kables->cpu_to_scu[cpu_unit_idx][cpu_port_num];
    if (uncable)
      {
        p->in_use = false;
      }
    else
      {
        if (p->in_use)
          {
            sim_printf ("cable SCU: CPU%u port %u in use.\n", cpu_unit_idx, cpu_port_num);
             return SCPE_ARG;
          }
        p->in_use = true;
        p->scu_unit_idx = scu_unit_idx;
        p->scu_port_num = scu_port_num;
        p->scu_subport_num = scu_subport_num;
      }
    return SCPE_OK;
  }

// cable SCUx CPUx

static t_stat cable_scu_to_cpu (int uncable, uint scu_unit_idx, uint scu_port_num, uint scu_subport_num, uint cpu_unit_idx, uint cpu_port_num)
  {
    struct scu_to_cpu_s * p = & kables->scu_to_cpu[scu_unit_idx][scu_port_num][scu_subport_num];
    if (uncable)
      {
        if (! p->in_use)
          {
            sim_printf ("uncable SCU%u port %u subport %u: not cabled\n", scu_unit_idx, scu_port_num, scu_subport_num);
            return SCPE_ARG;
          }

        // Unplug the other end of the cable 
        t_stat rc = back_cable_cpu_to_scu (uncable, cpu_unit_idx, cpu_port_num,
                                  scu_unit_idx, scu_port_num, scu_subport_num);
        if (rc)
          {
            return rc;
          }

        p->in_use = false;
        scu[scu_unit_idx].ports[scu_port_num].type = ADEV_NONE;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = 0;
// XXX is this wrong? is is_exp supposed to be an accumulation of bits?
        scu[scu_unit_idx].ports[scu_port_num].is_exp = false;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[scu_subport_num] = 0;
      }
    else
      {
        if (p->in_use)
          {
            sim_printf ("cable_scu: SCU %u port %u subport %u in use.\n", scu_unit_idx, scu_port_num, scu_subport_num);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = back_cable_cpu_to_scu (uncable, cpu_unit_idx, cpu_port_num,
                                  scu_unit_idx, scu_port_num, scu_subport_num);
        if (rc)
          {
            return rc;
          }

        p->in_use = true;
        p->cpu_unit_idx = cpu_unit_idx;
        p->cpu_port_num = (uint) cpu_port_num;

        scu[scu_unit_idx].ports[scu_port_num].type = ADEV_CPU;
        scu[scu_unit_idx].ports[scu_port_num].devIdx = (int) cpu_unit_idx;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[0] = (int) cpu_port_num;
// XXX is this wrong? is is_exp supposed to be an accumulation of bits?
        scu[scu_unit_idx].ports[scu_port_num].is_exp = 0;
        scu[scu_unit_idx].ports[scu_port_num].dev_port[scu_subport_num] = 0;
      }
    // Taking this out breaks the unit test segment loader.
    setup_scbank_map ();
    return SCPE_OK;
  }

//    cable SCUx port# IOMx port#
//    cable SCUx port# CPUx port#

static t_stat kable_scu (int uncable, uint scu_unit_idx, char * name_save)
  {
    if (scu_unit_idx >= scu_dev.numunits)
      {
        sim_printf ("cable_scu: SCU unit number out of range <%d>\n", 
                    scu_unit_idx);
        return SCPE_ARG;
      }

    int scu_port_num = getval (& name_save, "SCU port number");

    // The scu port number may have subport data encoded; check range
    // after we have decided if the is a connection to an IOM or a CPU.

    //    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
    //      {
    //        sim_printf ("cable_scu: SCU port number out of range <%d>\n", 
    //                    scu_port_num);
    //        return SCPE_ARG;
    //      }

// XXX combine into parse_match ()
    // extract 'IOMx' or 'CPUx'
    char * param = strtok_r (NULL, ", ", & name_save);
    if (! param)
      {
        sim_printf ("cable_scu: can't parse IOM\n");
        return SCPE_ARG;
      }
    uint unit_idx;


// SCUx IOMx

    if (name_match (param, "IOM", & unit_idx))
      {
        if (unit_idx >= N_IOM_UNITS_MAX)
          {
            sim_printf ("cable SCU: IOM unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
          {
            sim_printf ("cable_scu: SCU port number out of range <%d>\n", 
                        scu_port_num);
            return SCPE_ARG;
          }

        // extract iom port number
        param = strtok_r (NULL, ", ", & name_save);
        if (! param)
          {
            sim_printf ("cable SCU: can't parse IOM port number\n");
            return SCPE_ARG;
          }
        int iom_port_num = parseval (param);

        if (iom_port_num < 0 || iom_port_num >= N_IOM_PORTS)
          {
            sim_printf ("cable SCU: IOM port number out of range <%d>\n", iom_port_num);
            return SCPE_ARG;
          }
        return cable_scu_to_iom (uncable, scu_unit_idx, (uint) scu_port_num, unit_idx, (uint) iom_port_num);
      }


// SCUx CPUx

    else if (name_match (param, "CPU", & unit_idx))
      {
        if (unit_idx >= N_CPU_UNITS_MAX)
          {
            sim_printf ("cable SCU: IOM unit number out of range <%d>\n", unit_idx);
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
                sim_printf ("cable SCU: subport number out of range <%d>\n", 
                            scu_subport_num);
                return SCPE_ARG;
              }
            scu_port_num /= 10;
            is_exp = true;
          }
        if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
          {
            sim_printf ("cable SCU: port number out of range <%d>\n", 
                        scu_port_num);
            return SCPE_ARG;
          }

        // extract cpu port number
        param = strtok_r (NULL, ", ", & name_save);
        if (! param)
          {
            sim_printf ("cable SCU: can't parse CPU port number\n");
            return SCPE_ARG;
          }
        int cpu_port_num = parseval (param);

        if (cpu_port_num < 0 || cpu_port_num >= N_CPU_PORTS)
          {
            sim_printf ("cable SCU: CPU port number out of range <%d>\n", cpu_port_num);
            return SCPE_ARG;
          }
        return cable_scu_to_cpu (uncable, scu_unit_idx, (uint) scu_port_num, (uint) scu_subport_num, unit_idx, (uint) cpu_port_num);
      }


    else
      {
        sim_printf ("cable SCU: can't parse IOM or CPU\n");
        return SCPE_ARG;
      }
  }

static t_stat cable_ctlr_to_iom (int uncable, struct ctlr_to_iom_s * there,
                                 uint iom_unit_idx, uint chan_num)
  {
    if (uncable)
      {
        if (! there->in_use)
          {
            sim_printf ("error: UNCABLE: controller not cabled\n");
            return SCPE_ARG;
          }
        if (there->iom_unit_idx != iom_unit_idx ||
            there->chan_num != chan_num)
          {
            sim_printf ("error: UNCABLE: wrong controller\n");
            return SCPE_ARG;
          }
        there->in_use = false;
      }
   else
      {
        if (there->in_use)
          {
            sim_printf ("error: CABLE: controller in use\n");
            return SCPE_ARG;
          }
        there->in_use = true;
        there->iom_unit_idx = iom_unit_idx;
        there->chan_num = chan_num;
      }
    return SCPE_OK;
  }

static t_stat kable_ctlr (int uncable,
                          uint iom_unit_idx, uint chan_num,
                          uint ctlr_unit_idx, uint port_num,
                          char * service,
                          DEVICE * devp,
                          struct ctlr_to_iom_s * there,
                          enum ctlr_type_e ctlr_type, enum chan_type_e chan_type,
                          UNIT * unitp, iomCmd * iomCmd)
  {
    if (ctlr_unit_idx >= devp->numunits)
      {
        sim_printf ("%s: unit index out of range <%d>\n", 
                    service, ctlr_unit_idx);
        return SCPE_ARG;
      }

    struct iom_to_ctlr_s * p = & kables->iom_to_ctlr[iom_unit_idx][chan_num];

    if (uncable)
      {
        if (! p->in_use)
          {
            sim_printf ("%s: not cabled\n", service);
            return SCPE_ARG;
          }

        if (p->ctlr_unit_idx != ctlr_unit_idx)
          {
            sim_printf ("%s: Wrong IOM expected %d, found %d\n",
                        service, ctlr_unit_idx, p->ctlr_unit_idx);
            return SCPE_ARG;
          }

        // Unplug the other end of the cable
        t_stat rc = cable_ctlr_to_iom (uncable, there,
                                       iom_unit_idx, chan_num);
        if (rc)
          {
            return rc;
          }
        p->in_use = false;
      }
    else
      {
        if (p->in_use)
          {
            sim_printf ("%s: socket in use; unit number %d. (%o); "
                        "not cabling.\n", service, iom_unit_idx, iom_unit_idx);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = cable_ctlr_to_iom (uncable, there,
                                       iom_unit_idx, chan_num);
        if (rc)
          {
            return rc;
          }
        p->in_use = true;
        p->ctlr_unit_idx = ctlr_unit_idx;
        p->port_num = port_num;
        p->ctlr_type = ctlr_type;
        p->chan_type = chan_type;
        p->dev = devp;
        p->board  = unitp;
        p->iom_cmd  = iomCmd;
      }

    return SCPE_OK;
  }

//    cable IOMx chan# MTP [port#]  // tape controller
//    cable IOMx chan# MSPx [port#] // disk controller
//    cable IOMx chah# IPCx [port#] // FIPS disk controller
//    cable IOMx chan# OPC        // Operator console
//    cable IOMx chan# FNPx       // FNP 
//    cable IOMx chan# ABSIx      // ABSI 

static t_stat kable_iom (int uncable, uint iom_unit_idx, char * name_save)
  {
    if (iom_unit_idx >= iom_dev.numunits)
      {
        sim_printf ("error: CABLE IOM: unit number out of range <%d>\n", 
                    iom_unit_idx);
        return SCPE_ARG;
      }

    int chan_num = getval (& name_save, "IOM channel number");

    if (chan_num < 0 || chan_num >= MAX_CHANNELS)
      {
        sim_printf ("error: CABLE IOM channel number out of range <%d>\n", 
                    chan_num);
        return SCPE_ARG;
      }

    // extract controller type
    char * param = strtok_r (NULL, ", ", & name_save);
    if (! param)
      {
        sim_printf ("error: CABLE IOM can't parse controller type\n");
        return SCPE_ARG;
      }
    uint unit_idx;


// IOMx IPCx
    if (name_match (param, "IPC", & unit_idx))
      {
        if (unit_idx >= N_IPC_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: IPC unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        // extract IPC port number
        int ipc_port_num = 0;
        param = strtok_r (NULL, ", ", & name_save);
        if (param)
          ipc_port_num = parseval (param);

        if (ipc_port_num < 0 || ipc_port_num >= MAX_CTLR_PORTS)
          {
            sim_printf ("error: CABLE IOM: IPC port number out of range <%d>\n", ipc_port_num);
            return SCPE_ARG;
          }
        return kable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, (uint) ipc_port_num,
                           "CABLE IOMx IPCx",
                           & ipc_dev,
                           & kables->ipc_to_iom[unit_idx][ipc_port_num],
                           DEV_T_IPC, chan_type_PSI,
                           & ipc_unit [unit_idx], dsk_iom_cmd); // XXX mtp_iom_cmd?
      }

// IOMx MTPx
    if (name_match (param, "MTP", & unit_idx))
      {
        if (unit_idx >= N_MTP_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: MTP unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        // extract MTP port number
        int mtp_port_num = 0;
        param = strtok_r (NULL, ", ", & name_save);
        if (param)
          mtp_port_num = parseval (param);

        if (mtp_port_num < 0 || mtp_port_num >= MAX_CTLR_PORTS)
          {
            sim_printf ("error: CABLE IOM: MTP port number out of range <%d>\n", mtp_port_num);
            return SCPE_ARG;
          }
        //return cable_iom_to_mtp (uncable, iom_unit_idx, (uint) chan_num, unit_idx, (uint) mtp_port_num);
        return kable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, (uint) mtp_port_num,
                           "CABLE IOMx MTPx",
                           & mtp_dev,
                           & kables->mtp_to_iom[unit_idx][mtp_port_num],
                           DEV_T_MTP, chan_type_PSI,
                           & mtp_unit [unit_idx], mt_iom_cmd); // XXX mtp_iom_cmd?
      }

// IOMx OPCx
    if (name_match (param, "OPC", & unit_idx))
      {
        if (unit_idx >= N_MTP_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: MTP unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        uint opc_port_num = 0;
        return kable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, opc_port_num,
                           "CABLE IOMx OPCx",
                           & opc_dev,
                           & kables->opc_to_iom[unit_idx][opc_port_num],
                           DEV_T_OPC, chan_type_CPI,
                           & opc_unit [unit_idx], opc_iom_cmd); // XXX mtp_iom_cmd?
      }

    else
      {
        sim_printf ("cable IOM: can't parse controller type\n");
        return SCPE_ARG;
      }
  }

static t_stat cable_periph_to_ctlr (int uncable,
                                    uint ctlr_unit_idx, uint dev_code,
                                    struct dev_to_ctlr_s * there,
                                    iomCmd * iom_cmd)
  {
    if (uncable)
      {
        if (! there->in_use)
          {
            sim_printf ("error: UNCABLE: device not cabled\n");
            return SCPE_ARG;
          }
        if (there->ctlr_unit_idx != ctlr_unit_idx ||
            there->dev_code != dev_code)
          {
            sim_printf ("error: UNCABLE: wrong controller\n");
            return SCPE_ARG;
          }
        there->in_use = false;
      }
   else
      {
        if (there->in_use)
          {
            sim_printf ("error: CABLE: device in use\n");
            return SCPE_ARG;
          }
        there->in_use = true;
        there->ctlr_unit_idx = ctlr_unit_idx;
        there->dev_code = dev_code;
      }
    return SCPE_OK;
  }

static t_stat kable_periph (int uncable,
                            uint ctlr_unit_idx,
                            uint dev_code,
                            struct ctlr_to_dev_s * here,
                            uint tape_unit_idx,
                            iomCmd * iom_cmd,
                            struct dev_to_ctlr_s * there,
                            char * service)
  {
    if (uncable)
      {
        if (! here->in_use)
          {
            sim_printf ("%s: socket not in use\n", service);
            return SCPE_ARG;
          }
        // Unplug the other end of the cable
        t_stat rc = cable_periph_to_ctlr (uncable,
                                          ctlr_unit_idx, dev_code,
                                          there,
                                          iom_cmd);
        if (rc)
          {
            return rc;
          }

        here->in_use = false;
      }
    else
      {
        if (here->in_use)
          {
            sim_printf ("%s: controller socket in use; unit number %u. dev_code %oo\n",
                        service, ctlr_unit_idx, dev_code);
            return SCPE_ARG;
          }

        // Plug the other end of the cable in
        t_stat rc = cable_periph_to_ctlr (uncable,
                                          ctlr_unit_idx, dev_code,
                                          there,
                                          iom_cmd);
        if (rc)
          {
            return rc;
          }

        here->in_use = true;
        here->iom_cmd = iom_cmd;
      }

    return SCPE_OK;
  }

//     cable MTPx dev_code TAPEx

static t_stat kable_mtp (int uncable, uint ctlr_unit_idx, char * name_save)
  {
    if (ctlr_unit_idx >= mtp_dev.numunits)
      {
        sim_printf ("error: CABLE MTP: controller unit number out of range <%d>\n", 
                    ctlr_unit_idx);
        return SCPE_ARG;
      }

    int dev_code = getval (& name_save, "MTP device code");

    if (dev_code < 0 || dev_code >= MAX_CHANNELS)
      {
        sim_printf ("error: CABLE MTP device code out of range <%d>\n", 
                    dev_code);
        return SCPE_ARG;
      }

    // extract tape index
    char * param = strtok_r (NULL, ", ", & name_save);
    if (! param)
      {
        sim_printf ("error: CABLE IOM can't parse device name\n");
        return SCPE_ARG;
      }
    uint mt_unit_idx;


// MPCx TAPEx
    if (name_match (param, "TAPE", & mt_unit_idx))
      {
        if (mt_unit_idx >= N_MT_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: TAPE unit number out of range <%d>\n", mt_unit_idx);
            return SCPE_ARG;
          }

        return kable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             & kables->mtp_to_tape[ctlr_unit_idx][dev_code],
                             mt_unit_idx,
                             mt_iom_cmd,
                             & kables->tape_to_mtp[mt_unit_idx],
                             "CABLE MTPx TAPEx");
      }


    else
      {
        sim_printf ("cable MTP: can't parse device name\n");
        return SCPE_ARG;
      }
  }

//     cable IPCx dev_code DISKx

static t_stat kable_ipc (int uncable, uint ctlr_unit_idx, char * name_save)
  {
    if (ctlr_unit_idx >= ipc_dev.numunits)
      {
        sim_printf ("error: CABLE IPC: controller unit number out of range <%d>\n", 
                    ctlr_unit_idx);
        return SCPE_ARG;
      }

    int dev_code = getval (& name_save, "IPC device code");

    if (dev_code < 0 || dev_code >= MAX_CHANNELS)
      {
        sim_printf ("error: CABLE IPC device code out of range <%d>\n", 
                    dev_code);
        return SCPE_ARG;
      }

    // extract tape index
    char * param = strtok_r (NULL, ", ", & name_save);
    if (! param)
      {
        sim_printf ("error: CABLE IOM can't parse device name\n");
        return SCPE_ARG;
      }
    uint dsk_unit_idx;


// MPCx DISKx
    if (name_match (param, "DISK", & dsk_unit_idx))
      {
        if (dsk_unit_idx >= N_DSK_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: DISK unit number out of range <%d>\n", dsk_unit_idx);
            return SCPE_ARG;
          }

        return kable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             & kables->ipc_to_dsk[ctlr_unit_idx][dev_code],
                             dsk_unit_idx,
                             dsk_iom_cmd, // XXX
                             & kables->dsk_to_ipc[dsk_unit_idx],
                             "CABLE IPCx DISKx");
      }


    else
      {
        sim_printf ("cable IPC: can't parse device name\n");
        return SCPE_ARG;
      }
  }

t_stat sys_kable (int32 arg, const char * buf)
  {
    char * copy = strdup (buf);
    t_stat rc = SCPE_ARG;

    // process statement

    // extract first word
    char * name_save = NULL;
    char * name;
    name = strtok_r (copy, ", ", & name_save);
    if (! name)
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: can't parse name\n");
        sim_printf ("error: sys_cable: can't parse name\n");
        goto exit;
      }

    uint unit_num;
    if (name_match (name, "SCU", & unit_num))
      rc = kable_scu (arg, unit_num, name_save);
    else if (name_match (name, "IOM", & unit_num))
      rc = kable_iom (arg, unit_num, name_save);
    else if (name_match (name, "MTP", & unit_num))
      rc = kable_mtp (arg, unit_num, name_save);
    else if (name_match (name, "IPC", & unit_num))
      rc = kable_ipc (arg, unit_num, name_save);
    else
      {
        sim_printf ("error: cable: invalid name <%s>\n", name);
        goto exit;
      }

exit:
    free (copy);
    return rc;
  }
#endif // NEW_CABLE

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
    for (int i = 0; i < N_OPC_UNITS_MAX; i ++)
      cables->cablesFromIomToCon[i].iomUnitIdx = -1;
    for (int i = 0; i < N_DSK_UNITS_MAX; i ++)
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
  }

#ifdef NEW_CABLE
static void kable_init (void)
  {
    // sets kablesFromIomToDev[iomUnitIdx].devices[chanNum][dev_code].type
    //  to DEVT_NONE and in_use to false

    memset (kables, 0, sizeof (struct kables_s));
  }
#endif

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
    for (int i = 0; i < N_DSK_UNITS_MAX; i ++)
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
#ifdef NEW_CABLE
    kable_init ();
#endif // NEW_CABLE
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

#ifdef NEW_CABLE
   if (! kables)
      {
#ifdef M_SHARED
        kables = (struct kables_s *) create_shm ("kables", getsid (0),
                                                 sizeof (struct kables_s));
#else
        kables = (struct kables_s *) malloc (sizeof (struct kables_s));
#endif
        if (kables == NULL)
          {
            sim_printf ("create_shm kables failed\n");
            sim_err ("create_shm kables failed\n");
          }
      }

    // Initialize data structures
    kable_init ();

#endif // NEW_CABLE
  }


