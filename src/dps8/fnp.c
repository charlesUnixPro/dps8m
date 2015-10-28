//
//  fnp.c
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
      ttys [i] . mux_line = -1;
      MState . line [i] . muxLineNum = -1;
    }
    fnpQueueInit ();
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
 
    if (sim_switches & SWMASK ('V'))  /* verbose? */
    {
        FMTI *q = p;
        sim_printf("Faux Multics devices loaded ...\n");
        while (q)
        {
            dumpFMTI(q);
            q = q->next;
        }
    }
    
    if (sim_switches & SWMASK ('A'))  /* append? */
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

