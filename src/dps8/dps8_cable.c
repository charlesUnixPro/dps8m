#include "dps8.h"
#include "dps8_simh.h"
#include "dps8_iom.h"
#include "dps8_mt.h"
#include "dps8_scu.h"
#include "dps8_cpu.h"
#include "dps8_sys.h"
#include "dps8_console.h"
#include "dps8_disk.h"
#include "dps8_fnp.h"
#include "dps8_crdrdr.h"
#include "dps8_cable.h"
#include "dps8_utils.h"

struct cable_from_ioms_to_mt cables_from_ioms_to_mt [N_MT_UNITS_MAX];
struct cablesFromScu cablesFromScus [N_IOM_UNITS_MAX] [N_IOM_PORTS];
struct cable_from_cpu cables_from_cpus [N_SCU_UNITS_MAX] [N_SCU_PORTS];
struct cable_from_iom_to_con cables_from_ioms_to_con [N_OPCON_UNITS_MAX];
struct cable_from_iom_to_disk cables_from_ioms_to_disk [N_DISK_UNITS_MAX];
struct cable_from_iom_to_fnp  cables_from_ioms_to_fnp [N_FNP_UNITS_MAX];
struct cable_from_iom_to_crdrdr cables_from_ioms_to_crdrdr [N_CRDRDR_UNITS_MAX];
struct cpu_array cpu_array [N_CPU_UNITS_MAX];
struct iom iom [N_IOM_UNITS_MAX];

// cable_to_iom
//
// a peripheral is trying to attach a cable
//  to my port [iomUnitNum, chanNum, dev_code]
//  from their simh dev [dev_type, devUnitNum]
//
// Verify that the port is unused; attach this end of the cable

static t_stat cable_to_iom (uint iomUnitNum, int chanNum, int dev_code, 
                     enum dev_type dev_type, chan_type ctype, 
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

    if (iom [iomUnitNum] . devices [chanNum] [dev_code] . type != DEVT_NONE)
      {
        sim_printf ("cable_to_iom: socket in use\n");
        return SCPE_ARG;
      }
    iom [iomUnitNum] . devices [chanNum] [dev_code] . type = dev_type;
    iom [iomUnitNum] . devices [chanNum] [dev_code] . ctype = ctype;
    iom [iomUnitNum] . devices [chanNum] [dev_code] . devUnitNum = devUnitNum;

    iom [iomUnitNum] . devices [chanNum] [dev_code] . dev = devp;
    iom [iomUnitNum] . devices [chanNum] [dev_code] . board  = unitp;
    iom [iomUnitNum] . devices [chanNum] [dev_code] . iomCmd  = iomCmd;

    return SCPE_OK;
  }


// A scu is trying to attach a cable to us
//  to my port cpu_unit_num, cpu_port_num
//  from it's port scu_unit_num, scu_port_num
//

static t_stat cable_to_cpu (int cpu_unit_num, int cpu_port_num, int scu_unit_num, 
                     UNUSED int scu_port_num)
  {
    if (cpu_unit_num < 0 || cpu_unit_num >= (int) cpu_dev . numunits)
      {
        sim_printf ("cable_to_cpu: cpu_unit_num out of range <%d>\n", cpu_unit_num);
        return SCPE_ARG;
      }

    if (cpu_port_num < 0 || cpu_port_num >= N_CPU_PORTS)
      {
        sim_printf ("cable_to_cpu: cpu_port_num out of range <%d>\n", cpu_port_num);
        return SCPE_ARG;
      }

    if (cpu_array [cpu_unit_num] . ports [cpu_port_num] . inuse)
      {
        //sim_debug (DBG_ERR, & sys_dev, "cable_to_cpu: socket in use\n");
        sim_printf ("cable_to_cpu: socket in use\n");
        return SCPE_ARG;
      }

    DEVICE * devp = & scu_dev;
     
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . inuse = true;
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . scu_unit_num = scu_unit_num;
    cpu_array [cpu_unit_num] . ports [cpu_port_num] . devp = devp;

    //sim_printf ("cpu_array [%d] [%d] . scu_unit_num = %d\n", cpu_unit_num, cpu_port_num, scu_unit_num);
    setup_scbank_map ();

    return SCPE_OK;
  }

static t_stat cable_crdrdr (int crdrdr_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (crdrdr_unit_num < 0 || crdrdr_unit_num >= (int) crdrdr_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_crdrdr: crdrdr_unit_num out of range <%d>\n", crdrdr_unit_num);
        sim_printf ("cable_crdrdr: crdrdr_unit_num out of range <%d>\n", crdrdr_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_crdrdr [crdrdr_unit_num] . iom_unit_num != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_crdrdr: socket in use\n");
        sim_printf ("cable_crdrdr: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_CRDRDR, chan_type_PSI, crdrdr_unit_num, & crdrdr_dev, & crdrdr_unit [crdrdr_unit_num], crdrdr_iom_cmd);
    if (rc)
      return rc;

    cables_from_ioms_to_crdrdr [crdrdr_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_crdrdr [crdrdr_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_crdrdr [crdrdr_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cableFNP (int fnpUnitNum, int iomUnitNum, int chan_num, int dev_code)
  {
    if (fnpUnitNum < 0 || fnpUnitNum >= (int) fnpDev . numunits)
      {
        sim_printf ("cableFNP: fnpUnitNum out of range <%d>\n", fnpUnitNum);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum != -1)
      {
        sim_printf ("cableFNP: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iomUnitNum, chan_num, dev_code, DEVT_DN355, chan_type_PSI, fnpUnitNum, & fnpDev, & fnp_unit [fnpUnitNum], fnpIOMCmd);
    if (rc)
      return rc;

    cables_from_ioms_to_fnp [fnpUnitNum] . iomUnitNum = iomUnitNum;
    cables_from_ioms_to_fnp [fnpUnitNum] . chan_num = chan_num;
    cables_from_ioms_to_fnp [fnpUnitNum] . dev_code = dev_code;

    return SCPE_OK;
  }
 
static t_stat cable_disk (int disk_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (disk_unit_num < 0 || disk_unit_num >= (int) disk_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_disk: disk_unit_num out of range <%d>\n", disk_unit_num);
        sim_printf ("cable_disk: disk_unit_num out of range <%d>\n", disk_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_disk [disk_unit_num] . iom_unit_num != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_disk: socket in use\n");
        sim_printf ("cable_disk: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_DISK, chan_type_PSI, disk_unit_num, & disk_dev, & disk_unit [disk_unit_num], disk_iom_cmd);
    if (rc)
      return rc;

    cables_from_ioms_to_disk [disk_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_disk [disk_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_disk [disk_unit_num] . dev_code = dev_code;

    return SCPE_OK;
  }

static t_stat cable_opcon (int con_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (con_unit_num < 0 || con_unit_num >= (int) opcon_dev . numunits)
      {
        sim_printf ("cable_opcon: opcon_unit_num out of range <%d>\n", con_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_con [con_unit_num] . iom_unit_num != -1)
      {
        sim_printf ("cable_opcon: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_CON, chan_type_CPI, con_unit_num, & opcon_dev, & opcon_unit [con_unit_num], con_iom_cmd);
    if (rc)
      return rc;

    cables_from_ioms_to_con [con_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_con [con_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_con [con_unit_num] . dev_code = dev_code;

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

    if (cables_from_cpus [scu_unit_num] [scu_port_num] . cpu_unit_num != -1)
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

    cables_from_cpus [scu_unit_num] [scu_port_num] . cpu_unit_num = 
      cpu_unit_num;
    cables_from_cpus [scu_unit_num] [scu_port_num] . cpu_port_num = 
      cpu_port_num;

    scu [scu_unit_num] . ports [scu_port_num] . type = ADEV_CPU;
    scu [scu_unit_num] . ports [scu_port_num] . idnum = cpu_unit_num;
    scu [scu_unit_num] . ports [scu_port_num] . dev_port = cpu_port_num;
    return SCPE_OK;
  }

//  An IOM is trying to attach a cable 
//  to SCU port scu_unit_num, scu_port_num
//  from it's port iom_unit_num, iom_port_num
//

static t_stat cable_to_scu (int scu_unit_num, int scu_port_num, int iom_unit_num, 
                     int iom_port_num)
  {
    sim_debug (DBG_DEBUG, & scu_dev, 
               "cable_to_scu: scu_unit_num: %d, scu_port_num: %d, "
               "iom_unit_num: %d, iom_port_num: %d\n", 
               scu_unit_num, scu_port_num, iom_unit_num, iom_port_num);

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
    scu [scu_unit_num] . ports [scu_port_num] . idnum = iom_unit_num;
    scu [scu_unit_num] . ports [scu_port_num] . dev_port = iom_port_num;

    return SCPE_OK;
  }

static t_stat cable_iom (uint iomUnitNum, int iomPortNum, int scuUnitNum, int scuPortNum)
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

    if (cablesFromScus [iomUnitNum] [iomPortNum] . inuse)
      {
        sim_printf ("cable_iom: port in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_scu (scuUnitNum, scuPortNum, iomUnitNum, iomPortNum);
    if (rc)
      return rc;

    cablesFromScus [iomUnitNum] [iomPortNum] . inuse = true;
    cablesFromScus [iomUnitNum] [iomPortNum] . scuUnitNum = scuUnitNum;
    cablesFromScus [iomUnitNum] [iomPortNum] . scuPortNum = scuPortNum;

    return SCPE_OK;
  }

//
// String a cable from a tape drive to an IOM
//
// This end: mt_unit_num
// That end: iom_unit_num, chan_num, dev_code
// 

static t_stat cable_mt (int mt_unit_num, int iom_unit_num, int chan_num, int dev_code)
  {
    if (mt_unit_num < 0 || mt_unit_num >= (int) tape_dev . numunits)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_mt: mt_unit_num out of range <%d>\n", mt_unit_num);
        sim_printf ("cable_mt: mt_unit_num out of range <%d>\n", mt_unit_num);
        return SCPE_ARG;
      }

    if (cables_from_ioms_to_mt [mt_unit_num] . iom_unit_num != -1)
      {
        // sim_debug (DBG_ERR, & sys_dev, "cable_mt: socket in use\n");
        sim_printf ("cable_mt: socket in use\n");
        return SCPE_ARG;
      }

    // Plug the other end of the cable in
    t_stat rc = cable_to_iom (iom_unit_num, chan_num, dev_code, DEVT_TAPE, chan_type_PSI, mt_unit_num, & tape_dev, & mt_unit [mt_unit_num], mt_iom_cmd);
    if (rc)
      return rc;

    cables_from_ioms_to_mt [mt_unit_num] . iom_unit_num = iom_unit_num;
    cables_from_ioms_to_mt [mt_unit_num] . chan_num = chan_num;
    cables_from_ioms_to_mt [mt_unit_num] . dev_code = dev_code;

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
//   cable [TAPE | DISK],<dev_unit_num>,<iom_unit_num>,<chan_num>,<dev_code>
//
//   or iom to scu
//
//   cable IOM <iom_unit_num>,<iom_port_num>,<scu_unit_num>,<scu_port_num>
//
//   or scu to cpu
//
//   cable SCU <scu_unit_num>,<scu_port_num>,<cpu_unit_num>,<cpu_port_num>
//
//   or opcon to iom
//
//   cable OPCON <iom_unit_num>,<chan_num>,0,0
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
    memset (cablesFromScus, 0, sizeof (cablesFromScus));
    memset (cables_from_ioms_to_mt, 0, sizeof (cables_from_ioms_to_mt));
    for (int i = 0; i < N_MT_UNITS_MAX; i ++)
      {
        cables_from_ioms_to_mt [i] . iom_unit_num = -1;
      }
    for (int u = 0; u < N_SCU_UNITS_MAX; u ++)
      for (int p = 0; p < N_SCU_PORTS; p ++)
        cables_from_cpus [u] [p] . cpu_unit_num = -1; // not connected
    for (int i = 0; i < N_OPCON_UNITS_MAX; i ++)
      cables_from_ioms_to_con [i] . iom_unit_num = -1;
    for (int i = 0; i < N_DISK_UNITS_MAX; i ++)
      cables_from_ioms_to_disk [i] . iom_unit_num = -1;
    for (int i = 0; i < N_OPCON_UNITS_MAX; i ++)
      cables_from_ioms_to_con [i] . iom_unit_num = -1;
    for (int i = 0; i < N_CRDRDR_UNITS_MAX; i ++)
      cables_from_ioms_to_crdrdr [i] . iom_unit_num = -1;
    // sets iom [iomUnitNum] . devices [chanNum] [dev_code] . type to DEVT_NONE
    memset (& iom, 0, sizeof (iom));
  }
