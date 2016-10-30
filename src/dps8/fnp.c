/*
 Copyright 2014-2016 by Harry Reed
 Copyright 2014-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

// //  fnp.c
//  fnp
//
//  Created by Harry Reed on 11/26/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//

#include "dps8.h"
#include "dps8_simh.h"
#include "dps8_utils.h"
#include "fnp_mux.h"

#include "fnp_defs.h"
#include "fnp_2.h"
#include "fnp.h"
#include "fnp_cmds.h"

#include "fnp_udplib.h"


FMTI *readAndParse(char *file);
FMTI *readDevInfo(FILE *);

void dumpFMTI(FMTI *);

void freeFMTI(FMTI *p, bool bRecurse);

extern FMTI *fmti;

// Once-only initialization

void fnp_init(void)
{
    for (int i = 0; i < MAX_LINES; i ++)
      {
        ttys[i].mux_line = -1;
      }
    for (int f = 0; f < MAX_FNPS; f ++)
      for (int i = 0; i < MAX_LINES; i ++)
        {
          MState[f].line[i].muxLineNum = -1;
          MState[f].line[i].muxLineNum = FNP_NOLINK;
        }
    cpuToFnpQueueInit ();
}

t_stat fnpLoad (UNUSED int32 arg, char * buf)
//static t_stat sim_load0 (FILE *fileref, int flag)
{
    FILE * fileref = fopen (buf, "r");
    if (! fileref)
    {
        sim_printf("Couldn't open %s\n", buf);
        return SCPE_ARG;
    }

    FMTI *p = readDevInfo(fileref);
 
    if ((unsigned int) sim_switches & SWMASK ('V'))  /* verbose? */
    {
        FMTI *q = p;
        sim_printf("Faux Multics devices loaded ...\n");
        while (q)
        {
            dumpFMTI(q);
            q = q->next;
        }
    }
    
    if ((unsigned int) sim_switches & SWMASK ('A'))  /* append? */
    {
        if (fmti)
        {
            FMTI *q = fmti;
            while (q->next)
                q = q->next;
            q->next = p;
        } else
            fmti = p;
    } else {
        if (fmti)
            freeFMTI(fmti, true);
        fmti = p;
    }
    fclose (fileref);    
    return SCPE_OK;
}

