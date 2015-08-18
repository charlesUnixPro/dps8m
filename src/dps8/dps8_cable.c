#include "dps8.h"
#include "dps8_simh.h"
#include "dps8_iom.h"
#include "dps8_mt.h"
#include "dps8_scu.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_console.h"
#include "dps8_disk.h"
#include "dps8_fnp.h"
#include "dps8_crdrdr.h"
#include "dps8_prt.h"
#include "dps8_cable.h"
#include "dps8_utils.h"
#ifdef M_SHARED
#include <unistd.h>
#include "shm.h"
#endif
struct cables_t * cables = NULL;

// cable_to_iom
//
// a peripheral is trying to attach a cable
//  to a IOM port [iomUnitNum, chanNum, dev_code]
//  from their simh dev [devType, devUnitNum]
//
// Verify that the port is unused; attach this end of the cable

static t_stat cable_to_iom (uint iomUnitNum, int chanNum, int dev_code, 
                            devType devType, chanType ctype, 
                            uint devUnitNum, DEVICE * devp, UNIT * unitp, 
                            iomCmd * iomCmd)
  {
    if (iomUnitNum >= iom_dev . numunits)
      {
        sim_printf ("cable_to_iom: iomUnitNum out of range <%u>\n", iomUnitNum);
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
      & cables -> cablesFromIomToDev [iomUnitNum] . devices [chanNum] [dev_code];

    if (d -> type != DEVT_NONE)
      {
        sim_printf ("cable_to_iom: socket in use\n");
        return SCPE_ARG;
      }
    d -> type = devType;
    d -> ctype = ctype;
    d -> devUnitNum = devUnitNum;
    d -> dev = devp;
    d -> board  = unitp;
    d -> iomCmd  = iomCmd;

    return SCPE_OK;
  }


// A scu is trying to attach a cable to a cpu.
//  cpu port cpu_unit_num, cpu_port_num
//  from  scu_unit_num, scu_port_num
//

static t_stat cable_to_cpu (int cpu_unit_num, int cpu_port_num, 
                            int scu_unit_num, UNUSED int scu_port_num)
  {
    if (cpu_unit_num < 0 || cpu_unit_num >= (int) cpu_dev . numunits)
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

    if (cables -> cablesFromScuToCpu [cpu_unit_num] . ports [cpu_port_num] . inuse)
      {
        //sim_debug (DBG_ERR, & sys_dev, "cable_to_cpu: socket in use\n");
        sim_printf ("cable_to_cpu: socket in use\n");
        return SCPE_ARG;
      }

    struct cpuPort * p = 
      & cables -> cablesFromScuToCpu [cpu_unit_num] . ports [cpu_port_num];

    p -> inuse = true;
    p -> scu_unit_num = scu_unit_num;
    p -> devp = & scu_dev;

    //sim_printf ("cablesFromScuToCpu [%d] [%d] . scu_unit_num = %d\n", cpu_unit_num, cpu_port_num, scu_unit_num);
    setup_scbank_map ();

    return SCPE_OK;
  }

static t_stat cable_crdrdr (int crdrdr_unit_num, int iomUnitNum, int chan_num, 
                            int dev_code)
  {
    if (crdrdr_unit_num < 0 || crdrdr_unit_num >= (int) crdrdr_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_crdrdr: crdrdr_unit_num out of range <%d>\n", crdrdr_unit_num);
        sim_printf ("cable_crdrdr: crdrdr_unit_num out of range <%d>\n", 
                    crdrdr_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . iomUnitNum != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_crdrdr: socket in use\n");
        sim_printf ("cable_crdrdr: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_CRDRDR, 
                              chanTypePSI, crdrdr_unit_num, & crdrdr_dev, 
                              & crdrdr_unit [crdrdr_unit_num], crdrdr_iom_cmd);
    if (rc)
      return rc;

    cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . iomUnitNum = iomUnitNum;
    cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToCrdRdr [crdrdr_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_prt (int prt_unit_num, int iomUnitNum, int chan_num, 
                            int dev_code)
  {
    if (prt_unit_num < 0 || prt_unit_num >= (int) prt_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_prt: prt_unit_num out of range <%d>\n", prt_unit_num);
        sim_printf ("cable_prt: prt_unit_num out of range <%d>\n", 
                    prt_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToPrt [prt_unit_num] . iomUnitNum != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_prt: socket in use\n");
        sim_printf ("cable_prt: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_PRT, 
                              chanTypePSI, prt_unit_num, & prt_dev, 
                              & prt_unit [prt_unit_num], prt_iom_cmd);
    if (rc)
      return rc;

    cables -> cablesFromIomToPrt [prt_unit_num] . iomUnitNum = iomUnitNum;
    cables -> cablesFromIomToPrt [prt_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToPrt [prt_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cableFNP (int fnpUnitNum, int iomUnitNum, int chan_num, 
                        int dev_code)
  {
    if (fnpUnitNum < 0 || fnpUnitNum >= (int) fnpDev . numunits)
      {
        sim_printf ("cableFNP: fnpUnitNum out of range <%d>\n", fnpUnitNum);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToFnp [fnpUnitNum] . iomUnitNum != -1)
      {
        sim_printf ("cableFNP: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_DN355, 
                              chanTypePSI, fnpUnitNum, & fnpDev, 
                              & fnp_unit [fnpUnitNum], fnpIOMCmd);
    if (rc)
      return rc;

    cables -> cablesFromIomToFnp [fnpUnitNum] . iomUnitNum = iomUnitNum;
    cables -> cablesFromIomToFnp [fnpUnitNum] . chan_num = chan_num;
    cables -> cablesFromIomToFnp [fnpUnitNum] . dev_code = dev_code;

    return SCPE_OK;
  }
 
static t_stat cable_disk (int disk_unit_num, int iomUnitNum, int chan_num, 
                          int dev_code)
  {
    if (disk_unit_num < 0 || disk_unit_num >= (int) disk_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_disk: disk_unit_num out of range <%d>\n", disk_unit_num);
        sim_printf ("cable_disk: disk_unit_num out of range <%d>\n", 
                    disk_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToDsk [disk_unit_num] . iomUnitNum != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_disk: socket in use\n");
        sim_printf ("cable_disk: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_DISK, 
                              chanTypePSI, disk_unit_num, & disk_dev, 
                              & disk_unit [disk_unit_num], disk_iom_cmd);
    if (rc)
      return rc;

    cables -> cablesFromIomToDsk [disk_unit_num] . iomUnitNum = iomUnitNum;
    cables -> cablesFromIomToDsk [disk_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToDsk [disk_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_opcon (int con_unit_num, int iomUnitNum, int chan_num, 
                           int dev_code)
  {
    if (con_unit_num < 0 || con_unit_num >= (int) opcon_dev . numunits)
      {
        sim_printf ("cable_opcon: opcon_unit_num out of range <%d>\n", 
                    con_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToCon [con_unit_num] . iomUnitNum != -1)
      {
        sim_printf ("cable_opcon: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_CON, 
                              chanTypeCPI, con_unit_num, & opcon_dev, 
                              & opcon_unit [con_unit_num], con_iom_cmd);
    if (rc)
      return rc;

    cables -> cablesFromIomToCon [con_unit_num] . iomUnitNum = iomUnitNum;
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
    if (scu_unit_num < 0 || scu_unit_num >= (int) scu_dev . numunits)
      {
        // sim_debug (DBG_ERR, & scu_dev, 
                      // "cable_scu: scu_unit_num out of range <%d>\n", 
                      // scu_unit_num);
        sim_printf ("cable_scu: scu_unit_num out of range <%d>\n", 
                    scu_unit_num);
        return SCPE_ARG;
      }

    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
      {
        // sim_debug (DBG_ERR, & scu_dev, 
                      // "cable_scu: scu_port_num out of range <%d>\n", 
                      // scu_unit_num);
        sim_printf ("cable_scu: scu_port_num out of range <%d>\n", 
                    scu_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFomCpu [scu_unit_num] [scu_port_num] . cpu_unit_num != -1)
      {
        // sim_debug (DBG_ERR, & scu_dev, "cable_scu: port in use\n");
        sim_printf ("cable_scu: port in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_cpu (cpu_unit_num, cpu_port_num, scu_unit_num, 
                              scu_port_num);
    if (rc)
      return rc;

    cables -> cablesFomCpu [scu_unit_num] [scu_port_num] . cpu_unit_num = 
      cpu_unit_num;
    cables -> cablesFomCpu [scu_unit_num] [scu_port_num] . cpu_port_num = 
      cpu_port_num;

    scu [scu_unit_num] . ports [scu_port_num] . type = ADEV_CPU;
    scu [scu_unit_num] . ports [scu_port_num] . idnum = cpu_unit_num;
    scu [scu_unit_num] . ports [scu_port_num] . dev_port = cpu_port_num;
    return SCPE_OK;
  }

//  An IOM is trying to attach a cable 
//  to SCU port scu_unit_num, scu_port_num
//  from it's port iomUnitNum, iom_port_num
//

static t_stat cable_to_scu (int scu_unit_num, int scu_port_num, int iomUnitNum, 
                     int iom_port_num)
  {
    sim_debug (DBG_DEBUG, & scu_dev, 
               "cable_to_scu: scu_unit_num: %d, scu_port_num: %d, "
               "iomUnitNum: %d, iom_port_num: %d\n", 
               scu_unit_num, scu_port_num, iomUnitNum, iom_port_num);

    if (scu_unit_num < 0 || scu_unit_num >= (int) scu_dev . numunits)
      {
        // sim_debug (DBG_ERR, & scu_dev, 
                      // "cable_to_scu: scu_unit_num out of range <%d>\n", 
                      // scu_unit_num);
        sim_printf ("cable_to_scu: scu_unit_num out of range <%d>\n", 
                    scu_unit_num);
        return SCPE_ARG;
      }

    if (scu_port_num < 0 || scu_port_num >= N_SCU_PORTS)
      {
        // sim_debug (DBG_ERR, & scu_dev, 
                      // "cable_to_scu: scu_port_num out of range <%d>\n", 
                      // scu_port_num);
        sim_printf ("cable_to_scu: scu_port_num out of range <%d>\n", 
                    scu_port_num);
        return SCPE_ARG;
      }

    if (scu [scu_unit_num] . ports [scu_port_num] . type != ADEV_NONE)
      {
        // sim_debug (DBG_ERR, & scu_dev, "cable_to_scu: socket in use\n");
        sim_printf ("cable_to_scu: socket in use\n");
        return SCPE_ARG;
      }

    scu [scu_unit_num] . ports [scu_port_num] . type = ADEV_IOM;
    scu [scu_unit_num] . ports [scu_port_num] . idnum = iomUnitNum;
    scu [scu_unit_num] . ports [scu_port_num] . dev_port = iom_port_num;

    return SCPE_OK;
  }

static t_stat cable_iom (uint iomUnitNum, int iomPortNum, int scuUnitNum, 
                         int scuPortNum)
  {
    if (iomUnitNum >= iom_dev . numunits)
      {
        sim_printf ("cable_iom: iomUnitNum out of range <%d>\n", iomUnitNum);
        return SCPE_ARG;
      }

    if (iomPortNum < 0 || iomPortNum >= N_IOM_PORTS)
      {
        sim_printf ("cable_iom: iomPortNum out of range <%d>\n", iomUnitNum);
        return SCPE_ARG;
      }

    if (cables -> cablesFromScus [iomUnitNum] [iomPortNum] . inuse)
      {
        sim_printf ("cable_iom: port in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_scu (scuUnitNum, scuPortNum, iomUnitNum, iomPortNum);
    if (rc)
      return rc;

    cables -> cablesFromScus [iomUnitNum] [iomPortNum] . inuse = true;
    cables -> cablesFromScus [iomUnitNum] [iomPortNum] . scuUnitNum = scuUnitNum;
    cables -> cablesFromScus [iomUnitNum] [iomPortNum] . scuPortNum = scuPortNum;

    return SCPE_OK;
  }

//
// String a cable from a tape drive to an IOM
//
// This end: mt_unit_num
// That end: iomUnitNum, chan_num, dev_code
// 

static t_stat cable_mt (int mt_unit_num, int iomUnitNum, int chan_num, 
                        int dev_code)
  {
    if (mt_unit_num < 0 || mt_unit_num >= (int) tape_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_mt: mt_unit_num out of range <%d>\n", mt_unit_num);
        sim_printf ("cable_mt: mt_unit_num out of range <%d>\n", mt_unit_num);
        return SCPE_ARG;
      }

    if (cables -> cablesFromIomToTap [mt_unit_num] . iomUnitNum != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_mt: socket in use\n");
        sim_printf ("cable_mt: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_TAPE, 
                              chanTypePSI, mt_unit_num, & tape_dev, 
                              & mt_unit [mt_unit_num], mt_iom_cmd);
    if (rc)
      return rc;

    cables -> cablesFromIomToTap [mt_unit_num] . iomUnitNum = iomUnitNum;
    cables -> cablesFromIomToTap [mt_unit_num] . chan_num = chan_num;
    cables -> cablesFromIomToTap [mt_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }
 

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
//   cable [TAPE | DISK],<dev_unit_num>,<iomUnitNum>,<chan_num>,<dev_code>
//
//   or iom to scu
//
//   cable IOM <iomUnitNum>,<iom_port_num>,<scu_unit_num>,<scu_port_num>
//
//   or scu to cpu
//
//   cable SCU <scu_unit_num>,<scu_port_num>,<cpu_unit_num>,<cpu_port_num>
//
//   or opcon to iom
//
//   cable OPCON <iomUnitNum>,<chan_num>,0,0
//


t_stat sys_cable (UNUSED int32 arg, char * buf)
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
        rc = cable_iom (n1, n2, n3, n4);
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
    else if (strcasecmp (name, "PRT") == 0)
      {
        rc = cable_prt (n1, n2, n3, n4);
      }
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

void sysCableInit (void)
  {
    if (! cables)
      {
#ifdef M_SHARED
        cables = (struct cables_t *) create_shm ("cables", getsid (0), sizeof (struct cables_t));
#else
        cables = (struct cables_t *) malloc (sizeof (struct cables_t));
#endif
        if (cables == NULL)
          {
            sim_printf ("create_shm cables failed\n");
            sim_err ("create_shm cables failed\n");
          }
      }

    memset (cables, 0, sizeof (struct cables_t));
    for (int i = 0; i < N_MT_UNITS_MAX; i ++)
      {
        cables -> cablesFromIomToTap [i] . iomUnitNum = -1;
      }
    for (int u = 0; u < N_SCU_UNITS_MAX; u ++)
      for (int p = 0; p < N_SCU_PORTS; p ++)
        cables -> cablesFomCpu [u] [p] . cpu_unit_num = -1; // not connected
    for (int i = 0; i < N_OPCON_UNITS_MAX; i ++)
      cables -> cablesFromIomToCon [i] . iomUnitNum = -1;
    for (int i = 0; i < N_DISK_UNITS_MAX; i ++)
      cables -> cablesFromIomToDsk [i] . iomUnitNum = -1;
    for (int i = 0; i < N_CRDRDR_UNITS_MAX; i ++)
      cables -> cablesFromIomToCrdRdr [i] . iomUnitNum = -1;
    for (int i = 0; i < N_PRT_UNITS_MAX; i ++)
      cables -> cablesFromIomToPrt [i] . iomUnitNum = -1;
    // sets cablesFromIomToDev [iomUnitNum] . devices [chanNum] [dev_code] . type to DEVT_NONE
  }
