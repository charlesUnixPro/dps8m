/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2018 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#ifndef DPS8_EM_CONSTS_H
#define DPS8_EM_CONSTS_H

////////////////
// 
// System components: SCU, IOM, CPU
//
////////////////

// SCU
enum { N_SCU_UNITS_MAX = 8 };

// IOM
enum { N_IOM_UNITS_MAX = 4 };

// CPU
enum { N_CPU_UNITS_MAX = 8 };


////////////////
//
// Controllers
//
////////////////

// Unit record processor
enum { N_URP_UNITS_MAX = 16 };

// ABSI
enum { N_ABSI_UNITS_MAX = 1 };

// FNP
enum { N_FNP_UNITS_MAX = 16 };

// Operator console
enum { N_OPC_UNITS_MAX = 8 };

// MTP
enum { N_MTP_UNITS_MAX = 16 };

// MSP
enum { N_MSP_UNITS_MAX = 16 };

// IPC
enum { N_IPC_UNITS_MAX = 16 };

// DIA
enum { N_DIA_UNITS_MAX = 16 };

////////////////
//
// Peripherals
//
////////////////

// Tape drive
enum { N_MT_UNITS_MAX = 34 };

// Printer
enum { N_PRT_UNITS_MAX = 34 };

// Socket controller
enum { N_SKC_UNITS_MAX = 1 };

// Sockets
enum { N_SK_UNITS_MAX = 64 };

// Card reader
enum { N_RDR_UNITS_MAX = 16 };

// Card punch
enum { N_PUN_UNITS_MAX = 16 };

// Disk
enum { N_DSK_UNITS_MAX = 64 };

//
// Memory
//

enum { MEMSIZE = MEM_SIZE_MAX };

//
// Controller ports
//

enum { MAX_CTLR_PORTS = 8 };

//
// CPU ports
//

#ifdef DPS8M
enum { N_CPU_PORTS = 4 };
#endif
#ifdef L68
enum { N_CPU_PORTS = 8 };
#endif

#endif // DPS8_EM_CONSTS_H

