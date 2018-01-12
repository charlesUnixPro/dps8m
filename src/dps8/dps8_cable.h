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
#ifdef THREADZ
extern char * devTypeStrs [/* devType */];
#endif

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

#if 0

// New cables

// Multics devices (from prph card, AM81-04, pp 7-21 on)
//
//  CCUn   Combination record units  CCU0401
//  DIAn   Direct Interface Adpator
//  DSKn   Disk MSU0400 MSU0402 MSU0451 MSU0500 MSU0501 MSU3380 MSU3381
//  FNPn   FNP DN6670
//  OPCn   Operator Console CSU6001 CSU6004 CSU6601
//  PRTn   Printer PRT401 PRT402 PRU1000 PRU1200 PRU1201 PRU1203 PRU1600
//  PUNn   Card Punch PCU0120 PCU0121 CPZ201 PCU0300 CPZ300 CPZ301
//  RDRn   Card Reader CRZ201 CRZ301 CRU0500 CRU0501 CRU1050
//  TAPn   Tape Drive MTU0500 MTU0500 MTU0600 MTU0610 MTUo630 MTU8200
//

// Controllers
//   mpc card, AM81-04, pp 7-15 on.
//
//   mpc mtpx model -- mtp is tape controller
//    MTC501   mtp 501.
//    MTC502   mtp 502.
//    MTC0602  mtp 602.
//    MTC0600  mtp 600.
//    MTP0610  mtp 610.
//    MTP0611  mtp 611.
//    MTP8021  mtp 611.
//    MTP8022  mtp 611.
//    MTP8023  mtp 611.
//
//   mpc mspx model -- mtp is disk controller
//    MSP0400  msp 400.
//    DSC0451  msp 451.
//    MSP0451  msp 451.
//    MSP0601  msp 601.
//    MSP0603  msp 603.
//    MSP0607  msp 607.
//    MSP0609  msp 609.
//    MSP0611  msp 611.
//    MSP0612  msp 612.
//    MSP8021  msp 800.
//    MSP8022  msp 800.
//    MSP8022  msp 800.
//
//   mpc urpx model -- urp is unit record controller
//    URC002   urp   2.
//    URP0600  urp 600.
//    URP0601  urp 601.
//    URP0602  urp 602.
//    URP0604  urp 604.
//

// Devices connected to an IOM (I/O multiplexer) (possibly indirectly)
// DEVT_NONE must be zero for memset to init it properly.
typedef enum devType
  {
     DEVT_NONE = 0,
     //, DEVT_TAPE, DEVT_CON, DEVT_DISK, 
     ,
     // DEVT_DN355, DEVT_CRDRDR, DEVT_CRDPUN, DEVT_PRT, DEVT_URP, DEVT_ABSI
     DEVT_MPC,
     DEVT_IPC,

  } devType;

// Connect iom to controller
//
//  cable MPC,iom#,chan#,ctlr#,port#
//  cable IPC,iom#,chan#,ctlr#,port#
//
//    (ctrl#, port#) = iom_to_ctlr (iom#, chan#)
//    (iom#, chan#) = ctlr_to_iom (ctlr#, port#)


struct iom_to_ctlr_s
  {
    uint dev_idx; // SIMH unit number ("ctrl#")
    uint port_num; // port#
    enum devType type; // TAPE, DISK, CON, ...
    enum chanType ctype; // CPI, PSI, Direct
    DEVICE * dev; // ctlr device
    UNIT * board; // points into iomUnit
    iomCmd iom_cmd;
  };

struct ctlr_to_iom_s
  {
    uint iom_idx;
    uint chan_num;
  };

// Connect msp/ipc ctlr to disk
//
//   cable DISK,ctlr#,dev_code,disk#,unused
//
//    disk# = ctlr_to_dev (ctlr#, dev_code)
//    (ctlr#, dev_code) = dev_to_ctlr (disk#)

struct ctlr_to_dev_s
  {
    iomCmd iom_cmd;
  };

struct dev_to_ctlr_s
  {
    uint ctlr_idx;
    uint dev_code;
  }
   
struct new_cables_s
  {
    // IOM->CTLR
    //   mtp
    struct iom_to_ctlr_s iom_to_mtp [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    struct ctlr_to_iom_s mtp_to_iom [N_MTP_UNITS_MAX] [MAX_PORTS];
    //   msp/ipc
    struct iom_to_ctlr_s iom_to_msp [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    struct ctlr_to_iom_s msp_to_iom [N_MSP_UNITS_MAX] [MAX_PORTS];
    //   urp
    struct iom_to_ctlr_s iom_to_urp [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    struct ctlr_to_iom_s urp_to_iom [N_URP_UNITS_MAX] [MAX_PORTS];
    //   dia
    struct iom_to_ctlr_s iom_to_dia [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    struct ctlr_to_iom_s dia_to_iom [N_DIA_UNITS_MAX] [MAX_PORTS];
    //   fnp
    struct iom_to_ctlr_s iom_to_fnp [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    struct ctlr_to_iom_s fnp_to_iom [N_FNP_UNITS_MAX] [MAX_PORTS];
    //   absi
    struct iom_to_ctlr_s iom_to_absi [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    struct ctlr_to_iom_s absi_to_iom [N_ABSI_UNITS_MAX] [MAX_PORTS];
    //   console
    struct iom_to_ctlr_s iom_to_opcon [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    struct ctlr_to_iom_s opcon_to_iom [N_UPCON_UNITS_MAX] [MAX_PORTS];

    // CTLR->DEV
    struct ctlr_to_dev_s msp_to_disk [N_CTLR_UNITS_MAX] [MAX_CHANNELS];
    struct dev_to_ctlr_s disk_to_msp [N_CTLR_UNITS_MAX] [MAX_CHANNELS];
  }

extern struct new_cables_t * new_cables;
#endif
