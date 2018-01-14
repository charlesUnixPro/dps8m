/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern char * ctlr_type_strs [/* *enum ctlr_type_e */];
extern char * chan_type_strs [/* *enum chan_type_e */];

typedef enum chanType { chanTypeCPI, chanTypePSI, chanTypeDirect } chanType;


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
     CTLR_T_NONE = 0,
     CTLR_T_MTP,
     CTLR_T_MSP,
     CTLR_T_IPC,
     CTLR_T_OPC,
     CTLR_T_URP,
     CTLR_T_FNP,
     CTLR_T_ABSI
     // DEVT_DN355

  };

// Connect SCU to IOM/CPU
//
//    (iom#, port#) = scu_to_iom (scu#, port#, subport#)
//    (scu#, port#, subport#) = iom_to_scu (iom#, port#)
//
//    (cpu#, port#) = scu_to_cpu (scu#, port#, subport#)
//    (scu#, port#, subport#) = cpu_to_scu (cpu#, port#)
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
//    cable IOMx chan# URPx       // Unit record processor
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
//     cable URPx dev_code RDRx
//     cable URPx dev_code PUNx
//     cable URPx dev_code PRTx


struct ctlr_to_dev_s
  {
    bool in_use;
    uint unit_idx;
    iomCmd * iom_cmd;
  };

struct dev_to_ctlr_s
  {
    bool in_use;
    uint ctlr_unit_idx;
    uint dev_code;
    enum ctlr_type_e ctlr_type; // Used by disks to determine if the controler
                                // is MSP or IPC
  };
   
struct cables_s
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
    //   ipc->disk
    struct ctlr_to_dev_s ipc_to_dsk [N_IPC_UNITS_MAX] [N_DEV_CODES];
    struct dev_to_ctlr_s dsk_to_ipc [N_DSK_UNITS_MAX];
    //   msp->disk
    struct ctlr_to_dev_s msp_to_dsk [N_MSP_UNITS_MAX] [N_DEV_CODES];
    struct dev_to_ctlr_s dsk_to_msp [N_DSK_UNITS_MAX];
    //   urp->rdr/pun/prt
    struct ctlr_to_dev_s urp_to_urd [N_URP_UNITS_MAX] [N_DEV_CODES];
    struct dev_to_ctlr_s rdr_to_urp [N_RDR_UNITS_MAX];
    struct dev_to_ctlr_s pun_to_urp [N_PUN_UNITS_MAX];
    struct dev_to_ctlr_s prt_to_urp [N_PRT_UNITS_MAX];
  };

extern struct cables_s * cables;

t_stat sys_cable (UNUSED int32 arg, const char * buf);


// Accessors

// Get controller index from (IOM index, channel)

#define get_ctlr_idx(iom_unit_idx, chan) \
   (cables->iom_to_ctlr[iom_unit_idx][chan].ctlr_unit_idx)

// Get controller in_use from (IOM index, channel)

#define get_ctlr_in_use(iom_unit_idx, chan) \
   (cables->iom_to_ctlr[iom_unit_idx][chan].in_use)

// Get SCU index from (CPU index, port)

#define get_scu_idx(cpu_unit_idx, cpu_port_num) \
   (cables->cpu_to_scu[cpu_unit_idx][cpu_port_num].scu_unit_idx)


// Get SCU in_use from (CPU index, port)

#define get_scu_in_use(cpu_unit_idx, cpu_port_num) \
   (cables->cpu_to_scu[cpu_unit_idx][cpu_port_num].in_use)


t_stat sys_cable (UNUSED int32 arg, const char * buf);
t_stat sys_cable_ripout (UNUSED int32 arg, UNUSED const char * buf);
t_stat sys_cable_show (UNUSED int32 arg, UNUSED const char * buf);
void sysCableInit (void);
