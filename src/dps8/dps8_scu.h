/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// Devices connected to a SCU
enum active_dev { ADEV_NONE, ADEV_CPU, ADEV_IOM };

typedef struct
  {
    vol uint port_enable [N_SCU_PORTS];  // enable/disable

    // Mask registers A and B, each with 32 interrupt bits.
    vol word32 exec_intr_mask [N_ASSIGNMENTS];

    // Mask assignment.
    // 2 mask registers, A and B, each 32 bits wide.
    // A CPU will be attached to port N. 
    // Mask assignment assigns a mask register (A or B) to a CPU
    // on port N.
    // That is, when interrupt I is set:
    //   For reg = A, B
    //     
    // Mask A, B is set to Off or 0-7.
    // mask_enable [A|B] says that mask A or B is not off
    // if (mask_enable) then mask_assignment is a port number
    vol uint mask_enable [N_ASSIGNMENTS]; // enable/disable
    vol uint mask_assignment [N_ASSIGNMENTS]; // assigned port number

    vol uint cells [N_CELL_INTERRUPTS];

    uint lower_store_size; // In K words, power of 2; 32 - 4096
    uint cyclic; // 7 bits
    uint nea; // 8 bits
    uint onl; // 4 bits
    uint interlace; // 1 bit
    uint lwr; // 1 bit

    // Note that SCUs had no switches to designate SCU 'A' or 'B', etc.
    // Instead, SCU "A" is the one with base address switches set for 01400,
    // SCU "B" is the SCU with base address switches set to 02000, etc.
    // uint mem_base; // zero on boot scu
    // mode reg: mostly not stored here; returned by scu_get_mode_register()
    // int mode; // program/manual; if 1, sscr instruction can set some fields

    // CPU/IOM connectivity; designated 0..7
    // [CAC] really CPU/SCU and SCU/IOM connectivity
    struct ports
      {
        //bool is_enabled;
        enum active_dev type; // type of connected device
        int dev_idx; // index of connected dev (cpu_unit_udx, iom_unit_idx
        bool is_exp;
        // which port on the connected device?
        // if is_exp is false, then only [0] is used.
        // if true, one connection for each sub-port; -1 if not connected
        vol int dev_port [N_SCU_SUBPORTS];
        vol bool subport_enables [N_SCU_SUBPORTS]; 
        vol bool xipmask [N_SCU_SUBPORTS]; 
        vol int xipmaskval;
      } ports [N_SCU_PORTS];

    // system controller mode regsister    
    word4 id;
    word18 mode_reg;

    uint elapsed_days;
    uint steady_clock;    // If non-zero the clock is tied to the cycle counter
    uint bullet_time;
    uint y2k;
    int64 user_correction;
    uint64 last_time;
#ifdef SCUMEM
enum { SCU_MEM_SIZE = 020000000 };
    vol word36 M [SCU_MEM_SIZE];
#endif
} scu_t;

extern scu_t scu [N_SCU_UNITS_MAX];

extern DEVICE scu_dev;


int scu_set_interrupt(uint scu_unit_idx, uint inum);
void scu_init (void);
t_stat scu_sscr (uint scu_unit_idx, UNUSED uint cpu_unit_idx, uint cpu_port_num, word18 addr, 
                 word36 rega, word36 regq);
t_stat scu_smic (uint scu_unit_idx, uint UNUSED cpu_unit_idx, uint cpu_port_num, word36 rega);
t_stat scu_rscr (uint scu_unit_idx, uint cpu_unit_idx, word18 addr, word36 * rega, word36 * regq);
int scu_cioc (uint cpu_unit_idx, uint scu_unit_idx, uint scu_port_num, uint expander_command, uint sub_mask);
t_stat scu_rmcm (uint scu_unit_idx, uint cpu_unit_idx, word36 * rega, word36 * regq);
t_stat scu_smcm (uint scu_unit_idx, uint cpu_unit_idx, word36 rega, word36 regq);
void scu_clear_interrupt (uint scu_unit_idx, uint inum);
uint scu_get_highest_intr (uint scu_unit_idx);
t_stat scu_reset (DEVICE *dptr);
t_stat scu_reset_unit (UNIT * uptr, int32 value, const char * cptr, 
                       void * desc);
void scu_unit_reset (int scu_unit_idx);

