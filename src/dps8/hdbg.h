/*
 Copyright 2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */
#ifndef HDBG_H
#define HDBG_H

void hdbg_mark (void);
t_stat hdbg_size (int32 arg, UNUSED const char * buf);
t_stat hdbg_print (int32 arg, UNUSED const char * buf);
#ifdef HDBG
void hdbgTrace (void);
void hdbgPrint (void);
void hdbgMRead (word24 addr, word36 data);
void hdbgMWrite (word24 addr, word36 data);
void hdbgFault (_fault faultNumber, _fault_subtype subFault,
                const char * faultMsg);
void hdbgIntrSet (uint inum, uint cpuUnitIdx, uint scuUnitIdx);
void hdbgIntr (uint intr_pair_addr);
enum hregs_t
  {
    hreg_A,
    hreg_Q,
    hreg_X0, hreg_X1, hreg_X2, hreg_X3, hreg_X4, hreg_X5, hreg_X6, hreg_X7,
    hreg_AR0, hreg_AR1, hreg_AR2, hreg_AR3, hreg_AR4, hreg_AR5, hreg_AR6, hreg_AR7,
    hreg_PR0, hreg_PR1, hreg_PR2, hreg_PR3, hreg_PR4, hreg_PR5, hreg_PR6, hreg_PR7
  };
void hdbgReg (enum hregs_t type, word36 data);
struct _par;
void hdbgPAReg (enum hregs_t type, struct _par * data);
#endif

#ifdef HDBG
#define HDBGMRead(a, d) hdbgMRead (a, d)
#define HDBGMWrite(a, d) hdbgMWrite (a, d)
#define HDBGRegA() hdbgReg (hreg_A, cpu.rA)
#define HDBGRegQ() hdbgReg (hreg_Q, cpu.rQ)
#define HDBGRegX(i) hdbgReg (hreg_X0+(i), (word36) cpu.rX[i])
#define HDBGRegPR(i) hdbgPAReg (hreg_PR0+(i), & cpu.PAR[i]);
#define HDBGRegAR(i) hdbgPAReg (hreg_AR0+(i), & cpu.PAR[i]);
#else
#define HDBGMRead(a, d) 
#define HDBGMWrite(a, d)
#define HDBGRegA()
#define HDBGRegQ()
#define HDBGRegX(i)
#define HDBGRegPR(i)
#define HDBGRegAR(i)
#endif
#endif
