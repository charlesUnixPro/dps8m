/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2017 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

extern UNIT dn6600_unit [N_FNP_UNITS_MAX];
extern DEVICE dn6600_dev;

// Indexed by sim unit number
struct dn6600_unit_data
  {
    uint mailboxAddress;
    int link;
  };

typedef struct s_dn6600_data
  {
    struct dn6600_unit_data dn6600_unit_data [N_FNP_UNITS_MAX];
  } t_dn6600_data;

extern t_dn6600_data dn6600_data;

#if 0
// dn355_mailbox.incl.pl1 
//   input_sub_mbx
//       pad1:8, line_number:10, n_free_buffers:18
//       n_chars:18, op_code:9, io_cmd:9
//       n_buffers 
//       { abs_addr:24, tally:12 } [24]
//       command_data

struct input_sub_mbx
  {
    word36 word1; // dn355_no; is_hsla; la_no; slot_no    // 0      word0
    word36 word2; // cmd_data_len; op_code; io_cmd        // 1      word1
    word36 n_buffers;
    word36 dcws [24];
    word36 command_data;
  };
#endif

void dn6600_init(void);
int dn6600_iom_cmd (uint iomUnitIdx, uint chan);
