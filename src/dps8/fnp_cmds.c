//
//  fnp_cmds.c
//  fnp
//
//  Created by Harry Reed on 5/17/15.
//  Copyright (c) 2015 Harry Reed. All rights reserved.
//

#include <string.h>
#include "dps8.h"
#include "dps8_utils.h"
#include "dps8_fnp.h"
//#include "fnp_defs.h"
#include "fnp_cmds.h"
#include "fnp_2.h"
#include "fnp_mux.h"
#include "utlist.h"

void tellCPU (int fnpUnitNum, char * msg)
  {
//sim_printf ("tellCPU %d %s\n", fnpUnitNum, msg);
    fnpQueueMsg (fnpUnitNum, msg);
  }

t_MState MState;

static char * unpack (char * buffer, int which, size_t * retSize)
  {
    char * p = buffer;
    for (int i = 0; i < which; i ++)
      {
        //sim_printf ("unpack i %d p %s strstr %s\n", i, p, strstr (p, "data:"));
        p = strstr (p, "data:");
        if (! p)
          return NULL;
        p += 5; // strlen ("data:");
      }
    char * q;
    int nBytes = (int)strtol (p, & q, 10);
    if (p == q)
      return NULL;
    if (* q != ':')
      return NULL;
    if (nBytes < 0)
      {
        sim_printf ("unpack nBytes (%d) < 0; skipping\n", nBytes);
        return NULL;
      }
    q ++;
    char * out = malloc (nBytes);
    if (! out)
      goto fail;
    char * o = out;
    int remaining = nBytes; 
    while (remaining --)
      {
        int val;

        char chh = * q ++;
        if (chh >= '0' && chh <= '9')
          val = (chh - '0') << 4;
        else if (chh >= 'a' && chh<= 'f')
          val = (chh - 'a' + 10) << 4;
        else
          goto fail;

        char chl = * q ++;
        if (chl >= '0' && chl <= '9')
          val |= (chl - '0');
        else if (chl >= 'a' && chl<= 'f')
          val |= (chl - 'a' + 10);
        else
          goto fail;

        //sim_printf ("%c%c %02x\n", chh, chl, val);
        * o ++ = val;
      }
    if (retSize)
      * retSize = nBytes;
    return out;
fail:
    free (out);
    return NULL;
  }

// FNP message queue; when IPC messages come in, they are append to this
// queue. The sim_instr loop will poll the queue for messages for delivery 
// to the fnp code.

static pthread_mutex_t fnpMQlock;
typedef struct fnpQueueElement fnpQueueElement;
struct fnpQueueElement
  {
    int fnpUnitNum;
    char * arg3;
    fnpQueueElement * prev, * next;
  };

static fnpQueueElement * fnpQueue = NULL;

void fnpQueueInit (void)
  {
    if (pthread_mutex_init (& fnpMQlock, NULL) != 0)
      {
        sim_printf ("n mutex init failed\n");
      }
  }

t_stat fnp_command (int fnpUnitNum, char *arg3)
  {
    pthread_mutex_lock (& fnpMQlock);
    fnpQueueElement * element = malloc (sizeof (fnpQueueElement));
    if (! element)
      {
         sim_printf ("couldn't malloc fnpQueueElement\n");
      }
    else
      {
        element -> fnpUnitNum = fnpUnitNum;
        element -> arg3 = strdup (arg3);
        DL_APPEND (fnpQueue, element);
      }
    pthread_mutex_unlock (& fnpMQlock);
      return SCPE_OK;
  }

t_stat dequeue_fnp_command (void)
{
    char * arg3 = NULL;
while (fnpQueue) {
    pthread_mutex_lock (& fnpMQlock);
    fnpQueueElement * rv = fnpQueue;
    DL_DELETE (fnpQueue, rv);
    pthread_mutex_unlock (& fnpMQlock);
    int fnpUnitNum = rv -> fnpUnitNum;
    char * arg3 = rv -> arg3;
    free (rv);

    //sim_printf("fnp_command(\"%s\", \"%s\", \"%s\")\n", nodename, id, arg3);
    
    size_t arg3_len = strlen (arg3);
    char keyword [arg3_len];
    sscanf (arg3, "%s", keyword);

    if (strcmp(keyword, "bootload") == 0)
    {
        sim_printf("Received BOOTLOAD command...\n");
        MState . accept_calls = false;
        for (int p1 = 0; p1 < MAX_LINES; p1 ++)
          {
            MState . line [p1] . listen = false;
            int muxLineNum = MState.line[p1].muxLineNum;
            if (muxLineNum != -1)
            {
                tmxr_linemsg_stall(ttys[muxLineNum].tmln, "The FNP has been restarted\r\n");
            }
        }




    } else if (strcmp(keyword, "accept_calls") == 0)
    {
        sim_printf("Received ACCEPT_CALLS command...\n");
        MState . accept_calls = true;



    } else if (strcmp(keyword, "dont_accept_calls") == 0)
    {
        sim_printf("Received DONT_ACCEPT_CALLS command...\n");
        MState . accept_calls = false;



    } else if (strcmp(keyword, "listen") == 0)
    {
        int p1, p2, p3;
        int n = sscanf(arg3, "%*s %d %d %d", &p1, &p2, &p3);
        if (n != 3)
            goto scpe_arg; // listen is supposed to have 3 args
        //sim_printf("received LISTEN %d %d %d ...\n", p1, p2, p3);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: LISTEN p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: LISTEN p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . listen = p2 != 0;
        MState . line [p1] . inputBufferSize = p3;
        if (p2)
        {
            int muxLineNum = MState.line[p1].muxLineNum;
            if (muxLineNum != -1 && ttys[muxLineNum].state == ePassThrough)
            {
                tmxr_linemsg_stall(ttys[muxLineNum].tmln, "Multics is now listening to this line\r\n");
                char buf [256];
                sprintf (buf, "accept_new_terminal %d 1 0", p1);
                tellCPU (fnpUnitNum, buf);
            }
        }

    } else if (strcmp(keyword, "change_control_string") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg; // change_control_string is supposed to have 2 args
        //sim_printf("received CHANGE_CONTROL_STRING %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: CHANGE_CONTROL_STRING p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        MState . line [p1] . ctrlStrIdx = p2;



    } else if (strcmp(keyword, "dump_input") == 0)
    {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received dump_input %d ...\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: DUMP_INPUT p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        // dump the input
       int muxLineNo = MState . line [p1] . muxLineNum;
       //sim_printf ("dumping mux line %d\n");
       ttys [muxLineNo] . nPos = 0; 


    } else if (strcmp(keyword, "wru") == 0)
    {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received WRU %d ...\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: WRU p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        // XXX send wru
        // XXX for now, just reply that there was a timeout
        char msg [256];
        sprintf (msg, "wru_timeout %d", p1);
        tellCPU (fnpUnitNum, msg);



    } else if (strcmp(keyword, "terminal_accepted") == 0)
    {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received terminal_accepted %d ...\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: terminal_accepted p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        // XXX terminal accepted ... init stuff?
        char msg [256];
        sprintf (msg, "send_output %d", p1);
        tellCPU (fnpUnitNum, msg);



    } else if (strcmp(keyword, "break_all") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received break_all %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: break_all p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: break_all p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        // set break all on terminal
        MState . line [p1] . breakAll = !! p2;



    } else if (strcmp(keyword, "handle_quit") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received handle_quit %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: handle_quit p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: handle_quit p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . handleQuit = !! p2;



    } else if (strcmp(keyword, "full_duplex") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received full_duplex %d ...\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: full_duplex p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: full_duplex p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . fullDuplex = !! p2;



    } else if (strcmp(keyword, "echoplex") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received echoplex %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: echoplex p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: echoplex p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . echoPlex = !! p2;



    } else if (strcmp(keyword, "crecho") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received crecho %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: crecho p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: crecho p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . crecho = !! p2;



    } else if (strcmp(keyword, "lfecho") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received lfecho %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: lfecho p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: lfecho p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . lfecho = !! p2;



    } else if (strcmp(keyword, "tabecho") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received tabecho %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: tabecho p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: tabecho p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . tabecho = !! p2;



    } else if (strcmp(keyword, "replay") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received replay %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: replay p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: replay p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . replay = !! p2;



    } else if (strcmp(keyword, "polite") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received polite %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: polite p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: polite p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . polite = !! p2;



    } else if (strcmp(keyword, "prefixnl") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received prefixnl %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: prefixnl p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: prefixnl p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . prefixnl = !! p2;



    } else if (strcmp(keyword, "eight_bit_out") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received eight_bit_out %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: eight_bit_out p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: eight_bit_out p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . eight_bit_out = !! p2;



    } else if (strcmp(keyword, "eight_bit_in") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received eight_bit_in %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: eight_bit_in p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: eight_bit_in p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . eight_bit_in = !! p2;



    } else if (strcmp(keyword, "odd_parity") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received odd_parity %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: odd_parity p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: odd_parity p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . odd_parity = !! p2;



    } else if (strcmp(keyword, "output_flow_control") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received output_flow_control %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: output_flow_control p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: output_flow_control p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . output_flow_control = !! p2;



    } else if (strcmp(keyword, "input_flow_control") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received input_flow_control %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: input_flow_control p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        if (p2 != 0 && p2 != 1)
        {
            sim_printf("err: input_flow_control p2 (%d) != [0..1]\n", p2);
            goto scpe_arg; 
        }
        MState . line [p1] . input_flow_control = !! p2;



    } else if (strcmp(keyword, "block_xfer") == 0)
    {
        int p1, p2, p3;
        int n = sscanf(arg3, "%*s %d %d %d", &p1, &p2, &p3);
        if (n != 3)
            goto scpe_arg;
        //sim_printf("received block_xfer %d %d %d ...\n", p1, p2, p3);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: block_xfer p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        MState . line [p1] . block_xfer_in_frame = p2;
        MState . line [p1] . block_xfer_out_of_frame = p3;



    } else if (strcmp(keyword, "set_delay_table") == 0)
    {
        int p1, d1, d2, d3, d4, d5, d6;
        int n = sscanf(arg3, "%*s %d %d %d %d %d %d %d", &p1, &d1, &d2, &d3, &d4, &d5, &d6);
        if (n != 7)
            goto scpe_arg;
        //sim_printf("received set_delay_table %d %d %d %d %d %d %d...\n", p1, d1, d2, d3, d4, d5, d6);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: set_delay_table p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        MState . line [p1] . delay_table [0] = d1;
        MState . line [p1] . delay_table [1] = d2;
        MState . line [p1] . delay_table [2] = d3;
        MState . line [p1] . delay_table [3] = d4;
        MState . line [p1] . delay_table [4] = d5;
        MState . line [p1] . delay_table [5] = d6;






    } else if (strcmp(keyword, "line_control") == 0)
    {
        int p1;
        word36 d1, d2;

        int n = sscanf(arg3, "%*s %d %llo %llo", &p1, &d1, &d2);
        if (n != 3)
            goto scpe_arg;
        sim_printf ("received line_control %d %012llo %012llo\n", p1, d1, d2);
        sim_printf ("  dce_or_dte  %llo\n", getbits36 (d1, 0, 1));
        sim_printf ("  lap_or_lapb %llo\n", getbits36 (d1, 1, 1));
        sim_printf ("  disc_first  %llo\n", getbits36 (d1, 2, 1));
        sim_printf ("  trace_off   %llo\n", getbits36 (d1, 3, 1));
        sim_printf ("  activation_order %09llo\n", getbits36 (d1, 9, 9));
        sim_printf ("  frame_size %llo %lld.\n", getbits36 (d1, 18, 18), getbits36 (d1, 18, 18));
        sim_printf ("  K  %llo %lld.\n", getbits36 (d2,  0, 9), getbits36 (d2,  0, 9));
        sim_printf ("  N2 %llo %lld.\n", getbits36 (d2,  9, 9), getbits36 (d2,  9, 9));
        sim_printf ("  T1 %llo %lld.\n", getbits36 (d2, 18, 9), getbits36 (d2, 18, 9));
        sim_printf ("  T3 %llo %lld.\n", getbits36 (d2, 27, 9), getbits36 (d2, 27, 9));












    } else if (strcmp(keyword, "output") == 0)
    {
        int p1, p2;
        int n = sscanf(arg3, "%*s %d %d", &p1, &p2);
        if (n != 2)
            goto scpe_arg;
        //sim_printf("received output %d %d ...\n", p1, p2);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: output p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        char * data = unpack (arg3, 1, NULL);
//sim_printf ("msg:<%s>\n", data);
        if (! data)
        {
            sim_printf ("illformatted output message data; dropping\n");
            goto scpe_arg;
        }
        // delete NULs
        char * clean = malloc (p2 + 1);
        char * p = data;
        char * q = clean;
        for (int i = 0; i < p2; i ++)
          {
            char c = * p ++;
            if (c)
              * q ++ = c;
          }
        * q ++ = 0;

//sim_printf ("clean:<%s>\r\n", clean);
        int muxLineNum = MState . line [p1] . muxLineNum;
        tmxr_linemsg_stall (& mux_ldsc [muxLineNum], clean);
        free (data);
        free (clean);
        char msg [256];
        sprintf (msg, "send_output %d", p1);
//sim_printf ("tell CPU to send_output\n");
        tellCPU (fnpUnitNum, msg);




    } else if (strcmp(keyword, "disconnect_line") == 0)
    {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received disconnect_line %d\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: disconnect_line p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        int muxLineNum = MState.line[p1].muxLineNum;
        if (muxLineNum >= 0)
          tmxr_linemsg_stall(ttys[muxLineNum].tmln, "Multics has disconnected you\r\n");

        char msg [256];
        sprintf (msg, "line_disconnected %d", p1);
        tellCPU (fnpUnitNum, msg);
        

    } else if (strcmp(keyword, "output_fc_chars") == 0)
    {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received output_fc_chars %d\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: output_fc_chars p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        size_t retSize;

        char * data = unpack (arg3, 1, & retSize);
        if (retSize > FC_STR_SZ)
          {
            sim_printf ("data sz (%lu) truncated\n", retSize);
            retSize = FC_STR_SZ;
          }
        memcpy (MState . line [p1] . outputSuspendStr, data, retSize);
        MState . line [p1] . outputSuspendLen = retSize;

        data = unpack (arg3, 2, & retSize);
        if (retSize > FC_STR_SZ)
          {
            sim_printf ("data sz (%lu) truncated\n", retSize);
            retSize = FC_STR_SZ;
          }
        memcpy (MState . line [p1] . outputResumeStr, data, retSize);
        MState . line [p1] . outputResumeLen = retSize;




    } else if (strcmp(keyword, "set_framing_chars") == 0)
    {
        int p1, d1, d2;
        int n = sscanf(arg3, "%*s %d %d %d", &p1, &d1, &d2);
        if (n != 3)
            goto scpe_arg;
        //sim_printf("received set_framing_chars %d %d %d %d %d %d %d...\n", p1, d1, d2, d3, d4, d5, d6);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: set_framing_chars p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        MState . line [p1] . frame_begin = d1;
        MState . line [p1] . frame_end = d2;
        // XXX ignored



    } else if (strcmp(keyword, "dumpoutput") == 0)
    {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received dumpoutput %d...\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
            sim_printf("err: dumpoutput p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
        }
        // XXX ignored
        char msg [256];
        sprintf (msg, "send_output %d", p1);
//ipc_printf ("tell CPU to send_output\n");
        tellCPU (fnpUnitNum, msg);




    } else if (strcmp(keyword, "set_echnego_break_table") == 0)
    {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received dumpoutput %d...\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
        {
          sim_printf("err: set_echnego_break_table p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
          goto scpe_arg;
        }
      uint table [16];
      n = sscanf (arg3, "%*s %*d %o %o %o %o %o %o %o %o %o %o %o %o %o %o %o %o", 
                  table +  0, table +  1, table +  2, table +  3, 
                  table +  4, table +  5, table +  6, table +  7, 
                  table +  8, table +  9, table + 10, table + 11, 
                  table + 12, table + 13, table + 14, table + 15);
      if (n != 16)
        {
          sim_printf("err: set_echnego_break_table expected 16, got %d\n", n);
          goto scpe_arg;
        }
      for (int i = 0; i < 256; i ++)
        {
          int wordno = i / 16;
          int bitno = i % 16;
          int bitoffset = 15 - bitno;
          MState . line [p1] . echnego [i] = !!(table [wordno] & (1 << bitoffset));
        }
      //sim_printf ("set:");
      //for (int i = 0; i < 256; i ++)
        //if (MState . line [p1] . echnego [i])
          //sim_printf (" %d", i);
      //sim_printf ("\n");
//sim_printf ("recvd %s\n", keyword);


    } else if (strcmp(keyword, "start_negotiated_echo") == 0) {

    } else if (strcmp(keyword, "stop_echo_negotiation") == 0) {


    } else if (strcmp(keyword, "init_echo_negotiation") == 0) {
        int p1;
        int n = sscanf(arg3, "%*s %d", &p1);
        if (n != 1)
            goto scpe_arg;
        //sim_printf("received init_echo_negotiation %d...\n", p1);
        if (p1 < 0 && p1 >= MAX_LINES)
          {
            sim_printf("err: init_echo_negotiation p1 (%d) != [0..%d]\n", p1, MAX_LINES - 1);
            goto scpe_arg;
          }
//sim_printf ("recvd %s\n", keyword);

        char msg [256];
        sprintf (msg, "ack_echnego_init %d", p1);
//ipc_printf ("tell CPU to send_output\n");
        tellCPU (fnpUnitNum, msg);


    } else {
       sim_printf ("dropping <%s>\n", arg3);
       goto scpe_arg;
    }
    
    free (arg3);
} // while fnpQueue
    return SCPE_OK;

scpe_arg:
    if (arg3)
      free (arg3);
    return SCPE_ARG;
}


void sendInputLine (int fnpUnitNum, int hsla_line_num, char * buffer, int nChars, bool isBreak)
  {
//sim_printf ("sendInputLine fnpUnitNum %d hsla_line_num %d nChars %d\n", fnpUnitNum, hsla_line_num, nChars);
    int inputBufferSize =  MState . line [hsla_line_num].inputBufferSize;
    int remaining = nChars;
    int offset = 0;
    while (remaining > 0)
      {
        // Our encoding scheme is 2 hex digits/char

        // temporary until the logic is in place XXX
        int outputChainPresent = 0;

        int breakPresent;

        int thisSize;
        if (remaining > inputBufferSize)
          {
            thisSize = inputBufferSize;
            breakPresent = false;
          }
        else
          {
            thisSize = remaining;
            breakPresent = isBreak ? 1 : 0;
          }
        // Our encoding scheme is 2 hex digits/char
        char cmd [256 + 2 * thisSize];
        sprintf (cmd, "input %d %d %d %d data:%d:", hsla_line_num, thisSize, outputChainPresent, breakPresent, thisSize);
        char * tail = cmd;
        while (* tail)
          tail ++;
        for (int i = 0; i < thisSize; i ++)
           {
             * tail ++ = "0123456789abcdef" [(buffer [i + offset] >> 4) % 16];
             * tail ++ = "0123456789abcdef" [(buffer [i + offset]     ) % 16];
           }
        * tail = 0;
        tellCPU (fnpUnitNum, cmd);
        offset += thisSize;
        remaining -= thisSize;


#if 0
        char msg [256];
        sprintf (msg, "send_output %d", hsla_line_num);
//ipc_printf ("tell CPU to send_output\n");
        tellCPU (fnpUnitNum, msg);
#endif
      }
  }
