/*
 Copyright 2014-2016 by Harry Reed
 Copyright 2014-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//
//  fnp_cmds.h
//  fnp
//
//  Created by Harry Reed on 5/17/15.
//  Copyright (c) 2015 Harry Reed. All rights reserved.
//

#ifndef __fnp__fnp_cmds__
#define __fnp__fnp_cmds__

#include <stdio.h>

#include "fnp_defs.h"

t_stat fnp_command(int fnpUnitNum, char *arg3);
void sendInputLine (int fnpUnitNum, int hsla_line_num, char * buffer, int nChars, bool isBreak);
t_stat dequeue_fnp_command (void);
void cpuToFnpQueueInit (void);
void tellCPU (int fnpUnitNum, char * msg);



#endif /* defined(__fnp__fnp_cmds__) */
