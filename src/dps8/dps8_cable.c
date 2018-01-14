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

struct cables_s * cables = NULL;

char * ctlr_type_strs [/* enum ctlr_type_e */] =
  {
    "none", "MTP", "MSP", "IPC", "OPC", 
    "URP", "FNP", "ABSI"
  };

char * chan_type_strs [/* enum ctlr_type_e */] =
  {
    "CPI", "PSI", "Direct"
  };


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
    struct iom_to_scu_s * p = & cables->iom_to_scu[iom_unit_idx][iom_port_num];
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
    struct scu_to_iom_s * p = & cables->scu_to_iom[scu_unit_idx][scu_port_num];
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
    struct cpu_to_scu_s * p = & cables->cpu_to_scu[cpu_unit_idx][cpu_port_num];
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
    struct scu_to_cpu_s * p = & cables->scu_to_cpu[scu_unit_idx][scu_port_num][scu_subport_num];
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

static t_stat cable_scu (int uncable, uint scu_unit_idx, char * name_save)
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

static t_stat cable_ctlr (int uncable,
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

    struct iom_to_ctlr_s * p = & cables->iom_to_ctlr[iom_unit_idx][chan_num];

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

static t_stat cable_iom (int uncable, uint iom_unit_idx, char * name_save)
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
        return cable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, (uint) ipc_port_num,
                           "CABLE IOMx IPCx",
                           & ipc_dev,
                           & cables->ipc_to_iom[unit_idx][ipc_port_num],
                           CTLR_T_IPC, chan_type_PSI,
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
        return cable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, (uint) mtp_port_num,
                           "CABLE IOMx MTPx",
                           & mtp_dev,
                           & cables->mtp_to_iom[unit_idx][mtp_port_num],
                           CTLR_T_MTP, chan_type_PSI,
                           & mtp_unit [unit_idx], mt_iom_cmd); // XXX mtp_iom_cmd?
      }

// IOMx URPx
    if (name_match (param, "URP", & unit_idx))
      {
        if (unit_idx >= N_URP_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: URP unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        // extract URP port number
        int urp_port_num = 0;
        param = strtok_r (NULL, ", ", & name_save);
        if (param)
          urp_port_num = parseval (param);

        if (urp_port_num < 0 || urp_port_num >= MAX_CTLR_PORTS)
          {
            sim_printf ("error: CABLE IOM: URP port number out of range <%d>\n", urp_port_num);
            return SCPE_ARG;
          }

        return cable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, (uint) urp_port_num,
                           "CABLE IOMx URPx",
                           & urp_dev,
                           & cables->urp_to_iom[unit_idx][urp_port_num],
                           CTLR_T_URP, chan_type_PSI,
                           & urp_unit [unit_idx], urp_iom_cmd);
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
        return cable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, opc_port_num,
                           "CABLE IOMx OPCx",
                           & opc_dev,
                           & cables->opc_to_iom[unit_idx][opc_port_num],
                           CTLR_T_OPC, chan_type_CPI,
                           & opc_unit [unit_idx], opc_iom_cmd);
      }

// IOMx FNPx
    if (name_match (param, "FNP", & unit_idx))
      {
        if (unit_idx >= N_FNP_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: FNP unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        uint fnp_port_num = 0;
        return cable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, fnp_port_num,
                           "CABLE IOMx FNPx",
                           & fnp_dev,
                           & cables->fnp_to_iom[unit_idx][fnp_port_num],
                           CTLR_T_FNP, chan_type_direct,
                           & fnp_unit [unit_idx], fnp_iom_cmd);
      }

// IOMx ABSIx
    if (name_match (param, "ABSI", & unit_idx))
      {
        if (unit_idx >= N_ABSI_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: ABSI unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        uint absi_port_num = 0;
        return cable_ctlr (uncable,
                           iom_unit_idx, (uint) chan_num,
                           unit_idx, absi_port_num,
                           "CABLE IOMx ABSIx",
                           & absi_dev,
                           & cables->absi_to_iom[unit_idx][absi_port_num],
                           CTLR_T_ABSI, chan_type_direct,
                           & absi_unit [unit_idx], absi_iom_cmd);
      }

    else
      {
        sim_printf ("cable IOM: can't parse controller type\n");
        return SCPE_ARG;
      }
  }

static t_stat cable_periph_to_ctlr (int uncable,
                                    uint ctlr_unit_idx, uint dev_code,
                                    enum ctlr_type_e ctlr_type,
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
        there->ctlr_type = ctlr_type;
      }
    return SCPE_OK;
  }

static t_stat cable_periph (int uncable,
                            uint ctlr_unit_idx,
                            uint dev_code,
                            enum ctlr_type_e ctlr_type,
                            struct ctlr_to_dev_s * here,
                            uint unit_idx,
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
                                          ctlr_unit_idx, dev_code, ctlr_type,
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
                                          ctlr_unit_idx, dev_code, ctlr_type,
                                          there,
                                          iom_cmd);
        if (rc)
          {
            return rc;
          }

        here->in_use = true;
        here->unit_idx = unit_idx;
        here->iom_cmd = iom_cmd;
      }

    return SCPE_OK;
  }

//     cable MTPx dev_code TAPEx

static t_stat cable_mtp (int uncable, uint ctlr_unit_idx, char * name_save)
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

        return cable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             CTLR_T_MTP,
                             & cables->mtp_to_tape[ctlr_unit_idx][dev_code],
                             mt_unit_idx,
                             mt_iom_cmd,
                             & cables->tape_to_mtp[mt_unit_idx],
                             "CABLE MTPx TAPEx");
      }


    else
      {
        sim_printf ("cable MTP: can't parse device name\n");
        return SCPE_ARG;
      }
  }

//     cable IPCx dev_code DISKx

static t_stat cable_ipc (int uncable, uint ctlr_unit_idx, char * name_save)
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

        return cable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             CTLR_T_IPC,
                             & cables->ipc_to_dsk[ctlr_unit_idx][dev_code],
                             dsk_unit_idx,
                             dsk_iom_cmd, // XXX
                             & cables->dsk_to_ipc[dsk_unit_idx],
                             "CABLE IPCx DISKx");
      }


    else
      {
        sim_printf ("cable IPC: can't parse device name\n");
        return SCPE_ARG;
      }
  }

//     cable MSPx dev_code DISKx

static t_stat cable_msp (int uncable, uint ctlr_unit_idx, char * name_save)
  {
    if (ctlr_unit_idx >= msp_dev.numunits)
      {
        sim_printf ("error: CABLE MSP: controller unit number out of range <%d>\n", 
                    ctlr_unit_idx);
        return SCPE_ARG;
      }

    int dev_code = getval (& name_save, "MSP device code");

    if (dev_code < 0 || dev_code >= MAX_CHANNELS)
      {
        sim_printf ("error: CABLE MSP device code out of range <%d>\n", 
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

        return cable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             CTLR_T_MSP,
                             & cables->msp_to_dsk[ctlr_unit_idx][dev_code],
                             dsk_unit_idx,
                             dsk_iom_cmd, // XXX
                             & cables->dsk_to_msp[dsk_unit_idx],
                             "CABLE MSPx DISKx");
      }


    else
      {
        sim_printf ("cable MSP: can't parse device name\n");
        return SCPE_ARG;
      }
  }

//     cable URPx dev_code [RDRx PUNx PRTx]

static t_stat cable_urp (int uncable, uint ctlr_unit_idx, char * name_save)
  {
    if (ctlr_unit_idx >= urp_dev.numunits)
      {
        sim_printf ("error: CABLE URP: controller unit number out of range <%d>\n", 
                    ctlr_unit_idx);
        return SCPE_ARG;
      }

    int dev_code = getval (& name_save, "URP device code");

    if (dev_code < 0 || dev_code >= MAX_CHANNELS)
      {
        sim_printf ("error: CABLE URP device code out of range <%d>\n", 
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
    uint unit_idx;


// URPx RDRx
    if (name_match (param, "RDR", & unit_idx))
      {
        if (unit_idx >= N_RDR_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: DISK unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        return cable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             CTLR_T_URP,
                             & cables->urp_to_urd[ctlr_unit_idx][dev_code],
                             unit_idx,
                             rdr_iom_cmd, // XXX
                             & cables->rdr_to_urp[unit_idx],
                             "CABLE URPx RDRx");
      }

// URPx PUNx
    if (name_match (param, "PUN", & unit_idx))
      {
        if (unit_idx >= N_PUN_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: DISK unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        return cable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             CTLR_T_URP,
                             & cables->urp_to_urd[ctlr_unit_idx][dev_code],
                             unit_idx,
                             pun_iom_cmd, // XXX
                             & cables->pun_to_urp[unit_idx],
                             "CABLE URPx PUNx");
      }

// URPx PRTx
    if (name_match (param, "PRT", & unit_idx))
      {
        if (unit_idx >= N_PRT_UNITS_MAX)
          {
            sim_printf ("error: CABLE IOM: DISK unit number out of range <%d>\n", unit_idx);
            return SCPE_ARG;
          }

        return cable_periph (uncable,
                             ctlr_unit_idx,
                             (uint) dev_code,
                             CTLR_T_URP,
                             & cables->urp_to_urd[ctlr_unit_idx][dev_code],
                             unit_idx,
                             prt_iom_cmd, // XXX
                             & cables->prt_to_urp[unit_idx],
                             "CABLE URPx PRTx");
      }


    else
      {
        sim_printf ("cable URP: can't parse device name\n");
        return SCPE_ARG;
      }
  }

t_stat sys_cable (int32 arg, const char * buf)
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
      rc = cable_scu (arg, unit_num, name_save);
    else if (name_match (name, "IOM", & unit_num))
      rc = cable_iom (arg, unit_num, name_save);
    else if (name_match (name, "MTP", & unit_num))
      rc = cable_mtp (arg, unit_num, name_save);
    else if (name_match (name, "IPC", & unit_num))
      rc = cable_ipc (arg, unit_num, name_save);
    else if (name_match (name, "MSP", & unit_num))
      rc = cable_msp (arg, unit_num, name_save);
    else if (name_match (name, "URP", & unit_num))
      rc = cable_urp (arg, unit_num, name_save);
    else
      {
        sim_printf ("error: cable: invalid name <%s>\n", name);
        goto exit;
      }

exit:
    free (copy);
    return rc;
  }

static void cable_init (void)
  {
    // sets cablesFromIomToDev[iomUnitIdx].devices[chanNum][dev_code].type
    //  to DEVT_NONE and in_use to false

    memset (cables, 0, sizeof (struct cables_s));
  }

t_stat sys_cable_show (UNUSED int32 arg, UNUSED const char * buf)
  {
#define all(i,n) \
  for (uint i = 0; i < n; i ++)

    sim_printf ("SCU <--> IOM\n");
    sim_printf ("   SCU port --> IOM port\n");
    all (u, N_SCU_UNITS_MAX)
      all (prt, N_SCU_PORTS)
        {
          struct scu_to_iom_s * p = & cables->scu_to_iom[u][prt];
          if (p->in_use)
            sim_printf (" %4u %4u    %4u %4u\n", u, prt, p->iom_unit_idx, p->iom_port_num);
        }
 
    sim_printf ("   IOM port --> SCU port\n");
    all (u, N_IOM_UNITS_MAX)
      all (prt, N_IOM_PORTS)
        {
          struct iom_to_scu_s * p = & cables->iom_to_scu[u][prt];
          if (p->in_use)
            sim_printf (" %4u %4u    %4u %4u\n", u, prt, p->scu_unit_idx, p->scu_port_num);
        }
    sim_printf ("\n");


    sim_printf ("SCU <--> CPU\n");
    sim_printf ("   SCU port --> CPU port\n");
    all (u, N_SCU_UNITS_MAX)
      all (prt, N_SCU_PORTS)
        all (sp, N_SCU_SUBPORTS)
          {
            struct scu_to_cpu_s * p = & cables->scu_to_cpu[u][prt][sp];
            if (p->in_use)
              sim_printf (" %4u %4u    %4u %4u\n", u, prt, p->cpu_unit_idx, p->cpu_port_num);
          }
 
    sim_printf ("   CPU port --> SCU port subport\n");
    all (u, N_CPU_UNITS_MAX)
      all (prt, N_CPU_PORTS)
        {
          struct cpu_to_scu_s * p = & cables->cpu_to_scu[u][prt];
          if (p->in_use)
            sim_printf (" %4u %4u    %4u %4u  %4u\n", u, prt, p->scu_unit_idx, p->scu_port_num, p->scu_subport_num);
        }
    sim_printf ("\n");

    sim_printf ("IOM <--> controller\n");
    sim_printf ("                 ctlr       ctlr  chan\n");
    sim_printf ("   IOM chan -->  idx  port  type  type      device      board    command\n");
    all (u, N_IOM_UNITS_MAX)
      all (c, MAX_CHANNELS)
        {
          struct iom_to_ctlr_s * p = & cables->iom_to_ctlr[u][c];
          if (p->in_use)
            sim_printf (" %4u %4u     %4u  %4u %-6s  %-6s %10p %10p %10p\n", u, c, p->ctlr_unit_idx, p->port_num, ctlr_type_strs[p->ctlr_type], chan_type_strs[p->chan_type], p->dev, p->board, p->iom_cmd);
        }

#define CTLR_IOM(big,small) \
    sim_printf ("  %-4s port --> IOM channel\n", #big); \
    all (u, N_ ## big ## _UNITS_MAX) \
      all (prt, MAX_CTLR_PORTS) \
        { \
          struct ctlr_to_iom_s * p = & cables->small ## _to_iom[u][prt]; \
          if (p->in_use) \
            sim_printf (" %4u %4u    %4u %4u\n", u, prt, p->iom_unit_idx, p->chan_num); \
        } 
    CTLR_IOM (MTP, mtp)
    CTLR_IOM (MSP, msp)
    CTLR_IOM (IPC, ipc)
    CTLR_IOM (URP, urp)
    CTLR_IOM (DIA, dia)
    CTLR_IOM (ABSI, absi)
    CTLR_IOM (OPC, opc)
    sim_printf ("\n");

    sim_printf ("controller <--> device\n");

#define CTLR_DEV(from_big,from_small, to_label, to_big, to_small) \
    sim_printf ("  %-4s dev_code --> %-4s   command\n", #from_big, #to_label); \
    all (u, N_ ## from_big ## _UNITS_MAX) \
      all (prt, N_DEV_CODES) \
        { \
          struct ctlr_to_dev_s * p = & cables->from_small ## _to_ ## to_small[u][prt]; \
          if (p->in_use) \
            sim_printf (" %4u  %4u        %4u %10p\n", u, prt, p->unit_idx, p->iom_cmd); \
        } 
#define DEV_CTLR(from_big,from_small, to_label, to_big, to_small) \
    sim_printf ("  %-4s --> %-4s dev_code type\n", #to_label, #from_big); \
    all (u, N_ ## to_big ## _UNITS_MAX) \
      { \
        struct dev_to_ctlr_s * p = & cables->to_small ## _to_ ## from_small[u]; \
        if (p->in_use) \
          sim_printf (" %4u    %4u   %4u    %5s\n", u, p->ctlr_unit_idx, p->dev_code, ctlr_type_strs[p->ctlr_type]); \
      } 
    CTLR_DEV (MTP, mtp, TAPE, MT, tape);
    DEV_CTLR (MTP, mtp, TAPE, MT, tape);
    CTLR_DEV (IPC, ipc, DISK, DSK, dsk);
    DEV_CTLR (IPC, ipc, DISK, DSK, dsk);
    CTLR_DEV (MSP, msp, DISK, DSK, dsk);
    DEV_CTLR (MSP, msp, DISK, DSK, dsk);
    CTLR_DEV (URP, urp, URP, URP, urd);
    DEV_CTLR (URP, urp, RDR, RDR, rdr);
    DEV_CTLR (URP, urp, RDR, RDR, rdr);
    DEV_CTLR (URP, urp, PUN, PUN, pun);
    DEV_CTLR (URP, urp, PRT, PRT, prt);

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
        cables = (struct cables_s *) create_shm ("cables", getsid (0),
                                                 sizeof (struct cables_s));
#else
        cables = (struct cables_s *) malloc (sizeof (struct cables_s));
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


