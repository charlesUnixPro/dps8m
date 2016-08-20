//
//  fnp_2.c
//  fnp
//
//  Created by Harry Reed on 12/9/14.
//  Copyright (c) 2014 Harry Reed. All rights reserved.
//
#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_utils.h"
#include "fnp_defs.h"
#include "fnp_2.h"
#include "fnp_utils.h"
#include "fnp_cmds.h"

FMTI *fmti = NULL;


/*
 * some regular expression stuff ...
 * stolen from http://www.lemoda.net/c/unix-regex/
 */

// The following is the size of a buffer to contain any error messages encountered when the regular expression is compiled.
#define MAX_ERROR_MSG 0x1000

static int compile_regex (regex_t *r, const char *regex_text)
{
    int status = regcomp (r, regex_text, REG_EXTENDED|REG_NEWLINE);
    if (status != 0)
    {
        char error_message[MAX_ERROR_MSG];
        regerror (status, r, error_message, MAX_ERROR_MSG);
        sim_printf ("Regex error compiling '%s': %s\n", regex_text, error_message);
        return 1;
    }
    return 0;
}

/*
 Match the string in "to_match" against the compiled regular
 expression in "r".
 */

static bool match_regex (regex_t * r, const char * to_match)
{
//    /* "P" is a pointer into the string which points to the end of the
//     previous match. */
//    const char * p = to_match;
//    /* "N_matches" is the maximum number of matches allowed. */
//    const int n_matches = 10;
//    /* "M" contains the matches found. */
//    regmatch_t m[n_matches];
//    
//    while (1) {
//        int i = 0;
//        int nomatch = regexec (r, p, n_matches, m, 0);
//        if (nomatch)
//        {
//            sim_printf ("No more matches.\n");
//            return nomatch;
//        }
//        for (i = 0; i < n_matches; i++) {
//            regoff_t start;
//            regoff_t finish;
//            if (m[i].rm_so == -1) {
//                break;
//            }
//            start = m[i].rm_so + (p - to_match);
//            finish = m[i].rm_eo + (p - to_match);
//            if (i == 0) {
//                sim_printf ("$& is ");
//            }
//            else {
//                sim_printf ("$%d is ", i);
//            }
//            sim_printf ("'%.*s' (bytes %d:%d)\n", (finish - start), to_match + start, start, finish);
//        }
//        p += m[0].rm_eo;
//    }
//    return 0;
    /* Execute regular expression */
    int reti = regexec(r, to_match, 0, NULL, 0);
    if (!reti)
        return  true;
    else if (reti == REG_NOMATCH)
        return false;
    else
    {
        char msgbuf[MAX_ERROR_MSG];
        regerror(reti, r, msgbuf, sizeof(msgbuf));
        sim_printf("match_regex(): match failed: %s\n", msgbuf);
        return false;
    }

}

void dumpFMTI(FMTI *p)
{
    if (!p)
        return;
    
    sim_printf("name:            %s\n", p->multics.name);

    ATTRIBUTE *current, *tmp;
    
    HASH_ITER(hh, p->multics.attrs, current, tmp)
    {
        char temp[128];
        snprintf(temp, sizeof(temp), "%s:", current->Attribute);
        sim_printf("%-17s%s\n", trim(temp), current->Value);
    }
    
    sim_printf("\n");
}

char *strFMTI(FMTI *p, int line)
{
    if (!p)
        return "";
    static char str[1024];
    const int len = sizeof(str);
    
    snprintf(str, len, "\r\nLine %d connected as\r\n"
             "name:            %s\r\n",
             line,      p->multics.name
    );
    
    ATTRIBUTE *current, *tmp;
    HASH_ITER(hh, p->multics.attrs, current, tmp)
    {
        char temp[128], temp2[128];
        snprintf(temp,  sizeof(temp),  "%s:", current->Attribute);
        snprintf(temp2, sizeof(temp2), "%-17s%s\n", trim(temp), current->Value);
        
        strcat(str, temp2);
    }
    
    // XXX be careful of buffer overruns!
    strcat(str, "\n");

    
    return str;
}

#define FREE(t)     \
    if (t) { free(t); t = 0; }

void freeFMTI(FMTI *p, bool bRecurse)
{
    if (!p)
        return;
    
    if (bRecurse)
    {
        while (p)
        {
            FREE(p->multics.name);
        
            ATTRIBUTE *current, *tmp;
            HASH_ITER(hh, p->multics.attrs, current, tmp)
            {
                HASH_DEL(p->multics.attrs, current);  /* delete; users advances to next */
                FREE(current->Attribute);
                FREE(current->Value);
                FREE(current);            /* optional- if you want to free  */
            }
            
            FREE(p->uti);
            
            if (p->multics.regex)
            {
                FREE(p->multics.regex);
                regfree(&p->multics.r);
            }
            
            FMTI *nxt = p->next;
            
            FREE(p);
            
            p = nxt;
        }

    } else {
    
        FREE(p->multics.name);
       
        ATTRIBUTE *current, *tmp;
        HASH_ITER(hh, p->multics.attrs, current, tmp)
        {
            HASH_DEL(p->multics.attrs, current);  /* delete; users advances to next */
            FREE(current->Attribute);
            FREE(current->Value);
            FREE(current);            /* optional- if you want to free  */
        }

        FREE(p->uti);
        
        if (p->multics.regex)
        {
            FREE(p->multics.regex);
            regfree(&p->multics.r);
        }
        
        FREE(p);
    }
    
}

static FMTI *newFMTI()
{
    return calloc(1, sizeof(FMTI));
}

char *
getDevList()
{
    static char buf[2048];
    strcpy(buf, "");
    
    FMTI *t = fmti;
    while (t)
    {
        if (t->inUse == false &&
            t->multics.hsla_line_num != -1 &&
            t->multics.service == service_login)
        {
            if (strlen(buf) > 0)
                strcat(buf, ",");
            strcat(buf, t->multics.name);
        }
        t = t->next;
    }
    return buf;
}


FMTI *searchForDevice(char *name)
{
    //sim_printf("looking for <%s>\n", dev);
    FMTI *t = fmti;
        
    while (t)
    {
        if (t->inUse == false && t->multics.service == service_login)
        {
            if (strlen (name) == 0)
              return t;

            if (t->multics.regex)    // a regex is to be used to match the device names
            {
                bool iMatch = match_regex(& t->multics.r, name);
                if (iMatch)
                    return t;
            }
            else
                if (strcmp(name, t->multics.name) == 0)
                    return t;
        }
        t = t->next;
    }

    return NULL;
}

static ATTRIBUTE *searchForAttribute(char *attrib, ATTRIBUTE *a)
{
    ATTRIBUTE *s;
    
    HASH_FIND_STR(a, attrib, s);
    return s;
}

MUXTERMIO ttys[MAX_LINES];
extern TMLN mux_ldsc[MAX_LINES];

void connectPrompt (TMLN *tmln)
{
    tmxr_linemsgf (tmln, PROMPT, getDevList());
}

MUXTERMSTATE processUserInput(UNUSED TMXR *mp, TMLN *tmln, MUXTERMIO *tty, int32 line, int32 kar)
{
    if (kar == 0x1b || kar == 0x03)             // ESCape ('\e') | ^C
    {
//        char n[132];
//        snprintf(n, sizeof(n), "%d", line);
//        tmxr_dscln(&mux_unit, !0, n, mp);       // disconnect line
        
        tmxr_reset_ln( &mux_ldsc[line] ) ;
        
        tty->mux_line = -1;
        tty->state = eDisconnected;
        tty->tmln = NULL;
        if (tty->fmti)
            tty->fmti->inUse = false;
        tty->fmti = NULL;
        
        return eDisconnected;                   // line disconnect
    }
    
    // buffer too full for anything more?
    if ((size_t) tty->nPos >= sizeof(tty->buffer))
    {
        // yes. Only allow \n, \r, ^H, ^R
        switch (kar)
        {
            case '\b':  // backspace
            case 127:   // delete
                tmxr_linemsg_stall(tmln, "\b \b");    // remove char from line
                tty->buffer[tty->nPos] = 0;     // remove char from buffer
                tty->nPos -= 1;                 // back up buffer pointer
                break;
                
            case '\n':
            case '\r':
                tty->buffer[tty->nPos] = 0;
                return eEndOfLine;              // EOL found

            case 0x12:  // ^R
                tmxr_linemsg_stall  (tmln, "^R\r\n");       // echo ^R
                tmxr_linemsgf (tmln, PROMPT, getDevList());
                tmxr_linemsg_stall  (tmln, tty->buffer);
                break;
                
            default:
                break;
        }
        return eInput;  // stay in input mode
    }

    if (isprint(kar))   // printable?
    {
        MuxWrite(line, kar);
        tty->buffer[tty->nPos++] = (char) kar;
    } else {
        switch (kar)
        {
            case '\b':  // backspace
            case 127:   // delete
                if (tty->nPos > 0)
                {
                    tmxr_linemsg_stall(tmln, "\b \b");    // remove char from line
                    tty->buffer[tty->nPos] = 0;     // remove char from buffer
                    tty->nPos -= 1;                 // back up buffer pointer
                } else
                    tmxr_linemsg_stall(tmln, "\a");
                
                break;
                
            case '\n':
            case '\r':
                tty->buffer[tty->nPos] = 0;
                return eEndOfLine;
                
            case 0x12:  // ^R
                tmxr_linemsg_stall  (tmln, "^R\r\n");       // echo ^R
                tmxr_linemsgf (tmln, PROMPT, getDevList());
                tty->buffer[tty->nPos] = 0;
                tmxr_linemsg_stall  (tmln, tty->buffer);
                break;

                
            default:
                break;
        }

    }
    
    return eInput;  // stay in input mode
}

FMTI * readDevInfo(FILE *src)
{
    char buff[1024];

    FMTI *head = NULL;      // head of linked list
    FMTI *current = NULL;   // currrent entry (tail)

    int nLines = 0;
    
    while (fgets(buff, sizeof(buff), src))
    {
        nLines += 1;
        
        char *p = trim(buff);   // trim leading and trailing whitespace
        
        if (p[0] == '#')        // a '#' as first non-white charater is a comment line
            continue;
        
        if (p[0] == 0)          // blank line
            continue;;
        
        if (strncmp(p, "name:", 5) == 0)
        {
            if (!head)
            {
                head = newFMTI();
                current = head;
            } else {
                current->next = newFMTI();
                current = current->next;
            }
        }
    
        char *first  = trim(Strtok(p, ":"));       // stuff to the left of ':'
        char *second = trim(Strtok(NULL, ":;"));    // stuff to the right of ':'
        
        //sim_printf("%s %s\n", first, second);
        
        if (strcmp(first, "name") == 0)
        {
            current->multics.name = strdup(trim(second));
            // line # is encoded in the name. "d.h005" is line 5
            // assuming last 3 chars are a number....
            size_t len = strlen (current->multics.name);
            current->multics.hsla_line_num = -1;
            if (len >= 3)
            {
                int lineno = atoi (current->multics.name + len - 3);
                if (lineno >= 0 && lineno < MAX_LINES)  
                    current->multics.hsla_line_num = lineno;
            }
            // fnp unit number is encoded in the first char
            int fnpUnitNum = current->multics.name [0] - 'a';
            if (fnpUnitNum < 0 || fnpUnitNum >= N_FNP_UNITS_MAX)
                sim_err ("bad unit name in Devices.txt: %s", current->multics.name);
            current->multics.fnpUnitNum = fnpUnitNum;
            current->multics.service = service_login; // default service type
        //sim_printf ("%s %d\n", current->multics.name, current->multics.hsla_line_num);
        }
        else if (current && second && strcmp(first, "service") == 0)
        {
            trim (second);
            if (strcmp (second, "login") == 0)
                current->multics.service = service_login;
            else if (strcmp (second, "autocall") == 0)
                current->multics.service = service_autocall;
            else if (strcmp (second, "slave") == 0)
                current->multics.service = service_slave;
            else
               sim_printf ("service type '%s' not regcognized; ignored\n", second);
//sim_printf ("%s service %d\n", current->multics.name, current->multics.service);
        }
// This is not part of the CMF language, but I need away to communicate which MUX line has
// been reserved for an autocall
        else if (current && second && strcmp(first, "mux_line") == 0)
        {
            trim (second);
            char * end;
            long line = strtol (second, & end, 0);
            if (* end || line < 0 || line >= MAX_LINES)
              {
                sim_printf ("can't parse mux line '%s'; ignored\n", second);
              }
            else
              {
                current->multics.mux_line = (int) line;
              }
//sim_printf ("%s mux_line %d\n", current->multics.name, current->multics.mux_line);
        }
// This is not part of the CMF language, but I need away to configure the 
// from port on an autocall line
        else if (current && second && strcmp(first, "fromport") == 0)
        {
            trim (second);
            char * end;
            long port = strtol (second, & end, 0);
            if (* end || port < 0 || port >= 65535)
              {
                sim_printf ("can't parse fromport '%s'; ignored\n", second);
              }
            else
              {
                current->multics.fromport = (int) port;
              }
//sim_printf ("%s fromport %d\n", current->multics.name, current->multics.fromport);
        }
        else if (current && second && strcmp(first, "regex") == 0)
        {
            char *regx = stripquotes(second);
            int res = compile_regex(&current->multics.r, regx);
            if (!res)
                current->multics.regex = strdup(regx);
        }
        else if (current && second)
        {
            if (searchForAttribute(first, current->multics.attrs))
                sim_printf("Warning: Ignoring duplicate attribute <%s> at line %d\n", first, nLines);
            else
            {
                ATTRIBUTE *a = (ATTRIBUTE*)calloc(1, sizeof(ATTRIBUTE));
                a->Attribute = strdup(first);
                a->Value = strdup(second);
                HASH_ADD_KEYPTR(hh, current->multics.attrs, a->Attribute, strlen(a->Attribute), a);
            }
        }
    }
    
    return head;
}

#if 0
FMTI *readAndPrint(char *file)
{
    FILE *in = fopen(file, "r");
    FMTI *p = readDevInfo(in);
    fclose(in);
    
    while (p)
    {
        dumpFMTI(p);
        p = p->next;
    }
    
    return p;
}
#endif 

//void processInputCharacter (int line, int kar)
void processInputCharacter(UNUSED TMXR *mp, TMLN *tmln, MUXTERMIO *tty, int32 line, int32 kar)
{
    int hsla_line_num = tty->fmti->multics.hsla_line_num;
    int fnpUnitNum = tty->fmti->multics.fnpUnitNum;

    if (MState[fnpUnitNum].line [hsla_line_num] .echoPlex)
    {
        // echo \r, \n & \t
        if (MState[fnpUnitNum].line [hsla_line_num] .crecho && kar == '\n')   // echo a CR when a LF is typed
        {
            MuxWrite(line, '\r');
            MuxWrite(line, '\n');
        }
        else if (MState[fnpUnitNum].line [hsla_line_num] .lfecho && kar == '\r')  // echoes and inserts a LF in the users input stream when a CR is typed
        {
            MuxWrite(line, '\r');
            MuxWrite(line, '\n');
            //kar = '\n';
        }
        else if (MState[fnpUnitNum].line [hsla_line_num] .tabecho && kar == '\t') // echos the appropriate number of spaces when a TAB is typed
        {
            int nCol = tty->nPos;        // since nPos starts at 0 this'll work well with % operator
            // for now we use tabstops of 1,11,21,31,41,51, etc...
            nCol += 10;                  // 10 spaces/tab
            int nSpaces = 10 - (nCol % 10);
            for(int i = 0 ; i < nSpaces ; i += 1)
                MuxWrite(line, ' ');
        }
    
        // XXX slightly bogus logic here..
        // ^R ^U ^H DEL LF CR FF ETX 
        else if (kar == '\022'  || kar == '\025' || kar == '\b' ||
                 kar == 127     || kar == '\n'   || kar == '\r' || 
                 kar == '\f'    || kar == '\003')
        {
          // handled below
        }

        // echo character
        else
        {
            MuxWrite(line, kar);
        }
    } // if echoPlex

    // send of each and every character
    if (MState[fnpUnitNum].line [hsla_line_num] .breakAll)
    {
        ttys [line] . buffer [ttys [line] . nPos ++] = (char) kar;
        ttys [line] . buffer [ttys [line] . nPos] = 0;
        int hsla_line_num_2 = ttys [line] . fmti -> multics . hsla_line_num;
        sendInputLine (fnpUnitNum, hsla_line_num_2, ttys [line] . buffer, ttys [line] . nPos, true);
        ttys [line] . nPos = 0;
        
        return;
    }
    
    // Multics seems to want CR changed to LF
    if (kar == '\r')
      kar = '\n';

    // nothing after here tested (yet)
    
    // buffer too full for anything more or we reach our buffer threshold?
    //int inputBufferSize =  MState[fnpUnitNum].line [hsla_line_num].inputBufferSize;
    
    //if (tty->nPos >= sizeof(tty->buffer) || tty->nPos >= inputBufferSize)
    // 2 --> the current char plus a '\0'
    if ((size_t) tty->nPos >= sizeof(tty->buffer) - 2)
    {
        sendInputLine (fnpUnitNum, hsla_line_num, ttys [line] . buffer, ttys [line] . nPos, false);
        tty->nPos = 0;
        ttys [line] . buffer [ttys [line] . nPos ++] = (char) kar;
        tty->buffer[tty->nPos] = 0;
        return;
    }
    
    if ((MState[fnpUnitNum].line [hsla_line_num] . frame_begin != 0 &&
         MState[fnpUnitNum].line [hsla_line_num] . frame_begin == kar) ||
        (MState[fnpUnitNum].line [hsla_line_num] . frame_end != 0 &&
         MState[fnpUnitNum].line [hsla_line_num] . frame_end == kar))
      {
        if (tty -> nPos != 0)
          {
            sendInputLine (fnpUnitNum, hsla_line_num, ttys [line] . buffer, ttys [line] . nPos, true);
            tty->nPos = 0;
            tty->buffer[tty->nPos] = 0;
          }
        return;
      }

// Dosen't help. Also, breaks ^C
#if 0
    // Partial support of framing to get kermit working
    if (MState.line[hsla_line_num].block_xfer_out_of_frame)
      {
        if (kar == '\r')
          kar = '\n';     // translate to NL
        tty->buffer[tty->nPos++] = kar;
        tty->buffer[tty->nPos] = 0;
        if (kar == '\n' || tty->nPos >= MState.line[hsla_line_num].block_xfer_out_of_frame)
          {
sim_printf ("sending out of frame\n");
            sendInputLine (hsla_line_num, ttys [line] . buffer, ttys [line] . nPos, true);
            tty->nPos = 0;
            tty->buffer[tty->nPos] = 0;
          }
        return;
      }
#endif 
    switch (kar)
    {
//            case '\b':  // backspace
//            case 127:   // delete
//                tmxr_linemsg_stall(tmln, "\b \b");    // remove char from line
//                tty->buffer[tty->nPos] = 0;     // remove char from buffer
//                tty->nPos -= 1;                 // back up buffer pointer
//                break;
//                
        case '\n':          // NL
        case '\r':          // CR
        case '\f':          // FF
            kar = '\n';     // translate to NL
            tty->buffer[tty->nPos++] = (char) kar;
            tty->buffer[tty->nPos] = 0;
            sendInputLine (fnpUnitNum, hsla_line_num, ttys [line] . buffer, ttys [line] . nPos, true);
            tty->nPos = 0;
            tty->buffer[tty->nPos] = 0;
            return;
            
        case 0x03:          // ETX (^C) // line break
            {
                char buf [256];
                sprintf (buf, "line_break %d 1 0", hsla_line_num);
                tellCPU (fnpUnitNum, buf);
            }
            return;

        case '\b':  // backspace
        case 127:   // delete
            //if (MState[fnpUnitNum].line [hsla_line_num].erkl)
            {
                if (tty->nPos > 0)
                {
                    tmxr_linemsg_stall(tmln, "\b \b");    // remove char from line
                    tty->nPos -= 1;                 // back up buffer pointer
                    tty->buffer[tty->nPos] = 0;     // remove char from buffer
                } else
                    tmxr_linemsg_stall(tmln, "\a");
            }
            return;
            
        case 21:    // ^U kill
            //if (MState[fnpUnitNum].line [hsla_line_num].erkl)
            {
                tty->nPos = 0;
                tty->buffer[tty->nPos] = 0;
                tmxr_linemsg_stall(tmln, "^U\r\n");
            }
            return;
     
        case 0x12:  // ^R
            tmxr_linemsg_stall  (tmln, "^R\r\n");       // echo ^R
            //tmxr_linemsgf (tmln, PROMPT, getDevList());
            tmxr_linemsg_stall  (tmln, tty->buffer);
            return;
                
        default:
            break;
    }
    
    //// if half duplex, echo back to MUX line
    //if (!(MState[fnpUnitNum].line [hsla_line_num] .fullDuplex))
        //MuxWrite(line, kar);
    
    tty->buffer[tty->nPos++] = (char) kar;
    tty->buffer[tty->nPos] = 0;
        
    return ;  // stay in input mode

  }


// XXX Bad code -- blocks the thread

void tmxr_linemsg_stall (TMLN *lp, char *msg)
  {
    int32 len;

    for (len = (int32)strlen (msg); len > 0; --len)
      {
        while (SCPE_STALL == tmxr_putc_ln (lp, *msg))
          {
            if (lp->txbsz == tmxr_send_buffered_data (lp))
              usleep (100); // 10 ms
          }
        msg ++;
      }
    return;
  }
