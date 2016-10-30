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
//  fnp_mux.h
//  fnp
//
//  Created by Harry Reed on 11/26/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//

#ifndef __fnp__fnp_mux__
#define __fnp__fnp_mux__

#include <stdio.h>

#include "fnp_defs.h"

t_stat mux_setnl( UNIT * uptr, int32 val, char * cptr, void * desc );
//int32 muxWhatUnitAttached();

extern int32   mux_max;
extern TMLN    mux_ldsc[ MAX_LINES ];
extern TMXR    mux_desc;

extern char fnpName[32];        // IPC node name
extern char fnpGroup[32];       // IPC group name

t_stat do_mux_attach (char * attstr);
#endif /* defined(__fnp__fnp_mux__) */
