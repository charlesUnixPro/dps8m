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
    struct cableFromIom cablesFromIomToDsk [N_DSK_UNITS_MAX];
    struct cableFromIom cablesFromIomToCon [N_OPC_UNITS_MAX];
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

#ifdef NEW_CABLE

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
//   mpc mspx model -- msp is disk controller
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

enum chan_type_e { chan_type_CPI, chan_type_PSI, chan_type_direct };
// DEVT_NONE must be zero for memset to init it properly.
enum ctlr_type_e
  {
     DEV_T_NONE = 0,
     //, DEVT_TAPE, DEVT_CON, DEVT_DISK, 
     // DEVT_DN355, DEVT_CRDRDR, DEVT_CRDPUN, DEVT_PRT, DEVT_URP, DEVT_ABSI
     DEV_T_MTP,
     DEV_T_MSP,
     DEV_T_IPC,
     DEV_T_OPC,

  };

// Connect SCU to IOM/CPU
//
//    (iom#, port#) = scu_to_iom (scu#, port#, subport#)
//    (scu#, port#, subport#) = iom_to_scu (iom#, port#)
//
//    cable SCUx port# IOMx port#
//    cable SCUx port# CPUx port#
//

struct scu_to_iom_s
  {
    bool in_use;
    uint iom_unit_idx;
    uint iom_port_num;
  };

struct iom_to_scu_s
  {
    bool in_use;
    uint scu_unit_idx;
    uint scu_port_num;
    uint scu_subport_num;
  };

struct scu_to_cpu_s
  {
    bool in_use;
    uint cpu_unit_idx;
    uint cpu_port_num;
  };

struct cpu_to_scu_s
  {
    bool in_use;
    uint scu_unit_idx;
    uint scu_port_num;
    uint scu_subport_num;
  };

//
// Connect iom to controller
//
//    (ctrl#, port#) = iom_to_ctlr (iom#, chan#)
//    (iom#, chan#) = ctlr_to_iom (ctlr#, port#)
//
//    cable IOMx chan# MTPx [port#]  // tape controller
//    cable IOMx chan# MSPx [port#] // disk controller
//    cable IOMx chah# IPCx [port#] // FIPS disk controller
//    cable IOMx chan# OPCx       // Operator console
//    cable IOMx chan# FNPx       // FNP 
//    cable IOMx chan# ABSIx      // ABSI 
//

struct iom_to_ctlr_s
  {
    bool in_use;
    uint ctlr_unit_idx; // SIMH unit number ("ctrl#")
    uint port_num; // port#
    enum ctlr_type_e ctlr_type; // TAPE, DISK, CON, ...
    enum chan_type_e chan_type; // CPI, PSI, Direct
    DEVICE * dev; // ctlr device
    UNIT * board; // points into iomUnit
    iomCmd * iom_cmd;
  };

struct ctlr_to_iom_s
  {
    bool in_use;
    uint iom_unit_idx;
    uint chan_num;
  };

// Connect controller to device
//
//    device# = ctlr_to_dev (ctlr#, dev_code)
//    (ctlr#, dev_code) = dev_to_ctlr (disk#)
//
//   msp ctlr to disk
//
//     cable MSPx dev_code DISKx 
//
//   ipc ctlr to disk
//
//     cable FIPSx dev_code DISKx
//
//   fnp doesn't have a device
//
//   absi doesn't have a device
//
//   opc doesn't have a device
//
//   mpt to tape
//
//     cable MTPx dev_code TAPEx
//
//   urp to  device
//
//     cable URPx dev_code CRDRDRx
//     cable URPx dev_code CRDPUNx
//     cable URPx dev_code PRTx


struct ctlr_to_dev_s
  {
    bool in_use;
    iomCmd * iom_cmd;
  };

struct dev_to_ctlr_s
  {
    bool in_use;
    uint ctlr_unit_idx;
    uint dev_code;
  };
   
struct kables_s
  {
    // SCU->unit
    //  IOM
    struct scu_to_iom_s scu_to_iom [N_SCU_UNITS_MAX] [N_SCU_PORTS];
    struct iom_to_scu_s iom_to_scu [N_IOM_UNITS_MAX] [N_IOM_PORTS];
    //  CPU
    struct scu_to_cpu_s scu_to_cpu [N_SCU_UNITS_MAX] [N_SCU_PORTS] [N_SCU_SUBPORTS];
    struct cpu_to_scu_s cpu_to_scu [N_CPU_UNITS_MAX] [N_CPU_PORTS];

    // IOM->CTLR
    struct iom_to_ctlr_s iom_to_ctlr [N_IOM_UNITS_MAX] [MAX_CHANNELS];
    //   mtp
    struct ctlr_to_iom_s mtp_to_iom [N_MTP_UNITS_MAX] [MAX_CTLR_PORTS];
    //   msp
    struct ctlr_to_iom_s msp_to_iom [N_MSP_UNITS_MAX] [MAX_CTLR_PORTS];
    //   ipc
    struct ctlr_to_iom_s ipc_to_iom [N_IPC_UNITS_MAX] [MAX_CTLR_PORTS];
    //   urp
    struct ctlr_to_iom_s urp_to_iom [N_URP_UNITS_MAX] [MAX_CTLR_PORTS];
    //   dia
    struct ctlr_to_iom_s dia_to_iom [N_DIA_UNITS_MAX] [MAX_CTLR_PORTS];
    //   fnp
    struct ctlr_to_iom_s fnp_to_iom [N_FNP_UNITS_MAX] [MAX_CTLR_PORTS];
    //   absi
    struct ctlr_to_iom_s absi_to_iom [N_ABSI_UNITS_MAX] [MAX_CTLR_PORTS];
    //   console
    struct ctlr_to_iom_s opc_to_iom [N_OPC_UNITS_MAX] [MAX_CTLR_PORTS];

    // CTLR->DEV
    //   mtp->tape
    struct ctlr_to_dev_s mtp_to_tape [N_MTP_UNITS_MAX] [N_DEV_CODES];
    struct dev_to_ctlr_s tape_to_mtp [N_MT_UNITS_MAX];

    // CTLR->DEV
    //   ipc->disk
    struct ctlr_to_dev_s ipc_to_dsk [N_IPC_UNITS_MAX] [N_DEV_CODES];
    struct dev_to_ctlr_s dsk_to_ipc [N_DSK_UNITS_MAX];
    //   msp->disk
    struct ctlr_to_dev_s msp_to_dsk [N_MSP_UNITS_MAX] [N_DEV_CODES];
    struct dev_to_ctlr_s dsk_to_msp [N_DSK_UNITS_MAX];
  };

extern struct kables_s * kables;

t_stat sys_kable (UNUSED int32 arg, const char * buf);
#endif
