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

t_stat fnp_command(char *nodename, char *id, char *arg3);
void sendInputLine (int hsla_line_num, char * buffer, int nChars, bool isBreak);
t_stat dequeue_fnp_command (void);
void fnpQueueInit (void);
void tellCPU (UNUSED int cpuUnitNum, char * msg);



#endif /* defined(__fnp__fnp_cmds__) */
