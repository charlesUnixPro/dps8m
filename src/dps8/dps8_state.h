/*
 Copyright 2019 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

struct system_state_s
  {
    char commit_id [41];
    vol word36 M [MEMSIZE];
    cpu_state_t cpus [N_CPU_UNITS_MAX];
    struct cables_s cables;
  };

extern struct system_state_s * system_state;

