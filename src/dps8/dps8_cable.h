/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
// DEVT_NONE must be zero for memset to init it properly.
typedef enum devType
  {
     DEVT_NONE = 0, DEVT_TAPE, DEVT_CON, DEVT_DISK, 
     DEVT_MPC, DEVT_DN355, DEVT_CRDRDR, DEVT_CRDPUN, DEVT_PRT, DEVT_URP, DEVT_ABSI
  } devType;
extern char * devTypeStrs [];

typedef enum chanType { chanTypeCPI, chanTypePSI, chanTypeDirect } chanType;

struct cableFromIomToDev
  {
    struct device
      {
        enum devType type;
        enum chanType ctype;
        DEVICE * dev; // attached device; points into sim_devices[]
        uint devUnitIdx; // simh unit of the attached device
        UNIT * board;  // points into iomUnit
        iomCmd * iomCmd;
      } devices [MAX_CHANNELS] [N_DEV_CODES];
  };

struct cableFromScuToCpu
  {
    struct cpuPort
      {
        bool inuse;
        int scu_unit_idx; 
        int scu_port_num; 
        int scu_subport_num; 
      
        DEVICE * devp;
      } ports [N_CPU_PORTS];

  };

struct cableFromIom
  {
    int iomUnitIdx;
    int chan_num;
    int dev_code;
  };

struct cablesFromScu
  {
    bool inuse;
    int scuUnitIdx;
    int scuPortNum;
  };

struct cableFromCpu
  {
    int cpuUnitIdx;
    int cpu_port_num;
  };

struct cables_t
  {
    // CPU -> SCU
    struct cableFromCpu cablesFromCpus [N_SCU_UNITS_MAX] [N_SCU_PORTS] [N_SCU_SUBPORTS];

    // IOM -> SCU
    struct cablesFromScu cablesFromScus [N_IOM_UNITS_MAX] [N_IOM_PORTS];

    // IOM -> device
    struct cableFromIom cablesFromIomToUrp [N_URP_UNITS_MAX];
    struct cableFromIom cablesFromIomToCrdRdr [N_CRDRDR_UNITS_MAX];
    struct cableFromIom cablesFromIomToCrdPun [N_CRDPUN_UNITS_MAX];
    struct cableFromIom cablesFromIomToPrt [N_PRT_UNITS_MAX];
    struct cableFromIom cablesFromIomToFnp [N_FNP_UNITS_MAX];
    struct cableFromIom cablesFromIomToDsk [N_DISK_UNITS_MAX];
    struct cableFromIom cablesFromIomToCon [N_OPCON_UNITS_MAX];
    struct cableFromIom cablesFromIomToTap [N_MT_UNITS_MAX];
    struct cableFromIom cablesFromIomToAbsi [N_ABSI_UNITS_MAX];

    // SCU -> CPU
    struct cableFromScuToCpu cablesFromScuToCpu [N_CPU_UNITS_MAX];

    // SCU -> IOM
    struct cableFromIomToDev cablesFromIomToDev [N_IOM_UNITS_MAX];
  };

extern struct cables_t * cables;

t_stat sys_cable (UNUSED int32 arg, const char * buf);
t_stat sys_cable_ripout (UNUSED int32 arg, UNUSED const char * buf);
t_stat sys_cable_show (UNUSED int32 arg, UNUSED const char * buf);
void sysCableInit (void);
