//
//  fnp_2.h
//  fnp
//
//  Created by Harry Reed on 12/9/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//

#ifndef __fnp__fnp_2__
#define __fnp__fnp_2__

#include <stdio.h>
#include <stdbool.h>

#include "uthash.h"

#include <regex.h>

#include "fnp_defs.h"

enum muxtermstate
{
    eDisconnected = 0,  // disconnected
    eInput,             // waiting for user input
    ePassThrough,       // passthought mode
    eEndOfLine,         // EOL detected during user input (\r or \n)
};

typedef enum muxtermstate MUXTERMSTATE;

struct  deviceAttribute {
    char    *Attribute;
    char    *Value;
    UT_hash_handle hh;         /* makes this structure hashable */
};
typedef struct deviceAttribute ATTRIBUTE;

struct fauxMulticsTerminalInfo
{
    char *raw;
    struct
    {
        char    *name;     // Multics device name
        int     hsla_line_num; // Multiplexor slot number associated with name
        regex_t r;         // optional regex to match name
        char    *regex;    // text of optional regex
        
        ATTRIBUTE   *attrs;
    } multics;

    char    *uti;         // UNIX terminfo terminal type
    bool    inUse;        // True if device is being used
    
    struct fauxMulticsTerminalInfo  *next;
};
typedef struct fauxMulticsTerminalInfo FMTI;

struct muxtermio
{
    FMTI        *fmti;          // multics device attached to this line
    int32       mux_line;       // multiplexor line# used for this terminal (-1 == unused)
    TMLN        *tmln;          // terminal line
    char        buffer[1024];   // line buffer for initial device selection and line discipline
    int32       nPos;           // position where *next* user input is to be stored
    MUXTERMSTATE state;         // state of tty (eDisconnected, eInput ePassThrough)
    
    //t_MState    MState;         // RFU
};

typedef struct muxtermio MUXTERMIO;

MUXTERMSTATE processUserInput(TMXR *mp, TMLN *tmln, MUXTERMIO *tty, int32 line, int32 kar);

#define PROMPT  "HSLA Port (%s)? "

extern MUXTERMIO ttys[MAX_LINES];

extern FMTI *fmti;

void connectPrompt (TMLN *tmln);
void processInputCharacter(TMXR *mp, TMLN *tmln, MUXTERMIO *tty, int32 line, int32 kar);
void tmxr_linemsg_stall (TMLN *lp, char *msg);
void dumpFMTI(FMTI *);
FMTI *readDevInfo(FILE *);
void freeFMTI(FMTI *p, bool bRecurse);
char *strFMTI(FMTI *p, int line);
char * getDevList(void);
FMTI *searchForDevice(char *name);
#endif /* defined(__fnp__fnp_2__) */
