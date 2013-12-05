
/**
 * \file dps8_sys.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

#include "dps8.h"

// XXX Strictly speaking, memory belongs in the SCU
// We will treat memory as viewed from the CPU and elide the
// SCU configuration that maps memory across multiple SCUs.
// I would guess that multiple SCUs helped relieve memory
// contention across multiple CPUs, but that is a level of
// emulation that will be ignored.

word36 *M = NULL;                                          /*!< memory */


// These are part of the simh interface
char sim_name[] = "dps-8/m";
int32 sim_emax = 4; ///< some EIS can take up to 4-words
static void dps8_init(void);
void (*sim_vm_init) (void) = & dps8_init;    //CustomCmds;


stats_t sys_stats;

static t_stat sys_cable (int32 arg, char * buf);

CTAB dps8_cmds[] =
{
    {"DPSINIT", dpsCmd_Init,        0, "dpsinit dps8/m initialize stuff ...\n"},
    {"DPSDUMP", dpsCmd_Dump,        0, "dpsdump dps8/m dump stuff ...\n"},
    {"SEGMENT", dpsCmd_Segment,     0, "segment dps8/m segment stuff ...\n"},
    {"SEGMENTS", dpsCmd_Segments,   0, "segments dps8/m segments stuff ...\n"},
    { "CABLE", sys_cable, 0, "String a cable" },

    { NULL, NULL, 0, NULL}
};

/*!
 \brief special dps8 VM commands ....
 
 For greater flexibility, SCP provides some optional interfaces that can be used to extend its command input, command processing, and command post-processing capabilities. These interfaces are strictly optional
 and are off by default. Using them requires intimate knowledge of how SCP functions internally and is not recommended to the novice VM writer.
 
 Guess I shouldn't use these then :)
 */

static t_addr parse_addr(DEVICE *dptr, char *cptr, char **optr);
static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr addr);

static void dps8_init(void)    //CustomCmds(void)
{
    // special dps8 initialization stuff that cant be done in reset, etc .....

    // These are part of the simh interface
    sim_vm_parse_addr = parse_addr;
    sim_vm_fprint_addr = fprint_addr;

    sim_vm_cmd = dps8_cmds;

    init_opcodes();
    iom_init ();
    console_init ();
    disk_init ();
    mt_init ();
    //mpc_init ();
    scu_init ();
}

static int getval (char * * save, char * text)
  {
    char * value;
    char * endptr;
    value = strtok_r (NULL, ",", save);
    if (! value)
      {
        sim_debug (DBG_ERR, & iom_dev, "sys_cable: can't parse %s\n", text);
        sim_printf ("error: sys_cable: can't parse %s\n", text);
        return -1;
      }
    long l = strtol (value, & endptr, 0);
    if (* endptr || l < 0 || l > INT_MAX)
      {
        sim_debug (DBG_ERR, & iom_dev, "sys_cable: can't parse %s <%s>\n", text, value);
        sim_printf ("error: sys_cable: can't parse %s <%s>\n", text, value);
        return -1;
      }
    return (int) l;
  }

// Connect dev to iom
//
//   cable [TAPE | OPCON | DISK],<dev_unit_num>,<iom_unit_num>,<chan_num>,<dev_code>
//
//   or iom to scu
//
//   cable IOM <iom_unit_num>,<iom_port_num>,<scu_unit_num>,<scu_port_num>
//
//   or scu to cpu
//
//   cable SCU <scu_unit_num>,<scu_port_num>,<cpu_unit_num>,<cpu_port_num>
//
//

static t_stat sys_cable (int32 arg, char * buf)
  {
// XXX Minor bug; this code doesn't check for trailing garbage
// XXX iom_dev in the sim_debugs really should be sys_dev, which we haven't created yet

    char * copy = strdup (buf);
    t_stat rc = SCPE_ARG;

    // process statement

    // extract name
    char * name_save = NULL;
    char * name;
    name = strtok_r (copy, ",", & name_save);
    if (! name)
      {
        sim_debug (DBG_ERR, & iom_dev, "sys_cable: can't parse name\n");
        sim_printf ("error: sys_cable: can't parse name\n");
        goto exit;
      }


    int n1 = getval (& name_save, "parameter 1");
    if (n1 < 0)
      goto exit;
    int n2 = getval (& name_save, "parameter 2");
    if (n2 < 0)
      goto exit;
    int n3 = getval (& name_save, "parameter 3");
    if (n3 < 0)
      goto exit;
    int n4 = getval (& name_save, "parameter 4");
    if (n4 < 0)
      goto exit;


    if (strcasecmp (name, "TAPE") == 0)
      {
        rc = cable_mt (n1, n2, n3, n4);
      }
// XXX not yet
//    else if (strcasecmp (name, "DISK") == 0)
//      {
//        rc = cable_disk (n1, n2, n3, n4);
//      }
    else if (strcasecmp (name, "OPCON") == 0)
      {
        rc = cable_opcon (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "IOM") == 0)
      {
        rc = cable_iom (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "SCU") == 0)
      {
        rc = cable_scu (n1, n2, n3, n4);
      }
    else
      {
        sim_debug (DBG_ERR, & iom_dev, "sys_cable: Invalid switch name <%s>\n", name);
        sim_printf ("error: sys_cable: invalid switch name <%s>\n", name);
        goto exit;
      }

exit:
    free (copy);
    return rc;
  }

static struct PRtab {
    char *alias;    ///< pr alias
    int   n;        ///< number alias represents ....
} _prtab[] = {
    {"pr0", 0}, ///< pr0 - 7
    {"pr1", 1},
    {"pr2", 2},
    {"pr3", 3},
    {"pr4", 4},
    {"pr5", 5},
    {"pr6", 6},
    {"pr7", 7},

    {"pr[0]", 0}, ///< pr0 - 7
    {"pr[1]", 1},
    {"pr[2]", 2},
    {"pr[3]", 3},
    {"pr[4]", 4},
    {"pr[5]", 5},
    {"pr[6]", 6},
    {"pr[7]", 7},
    
    // from: ftp://ftp.stratus.com/vos/multics/pg/mvm.html
    {"ap",  0},
    {"ab",  1},
    {"bp",  2},
    {"bb",  3},
    {"lp",  4},
    {"lb",  5},
    {"sp",  6},
    {"sb",  7},
    
    {0,     0}
    
};

static t_addr parse_addr(DEVICE *dptr, char *cptr, char **optr)
{
    // a segment reference?
    if (strchr(cptr, '|'))
    {
        static char addspec[256];
        strcpy(addspec, cptr);
        
        *strchr(addspec, '|') = ' ';
        
        char seg[256], off[256];
        int params = sscanf(addspec, "%s %s", seg, off);
        if (params != 2)
        {
            sim_printf("parse_addr(): illegal number of parameters\n");
            *optr = cptr;   // signal error
            return 0;
        }
        
        // determine if segment is numeric or symbolic...
        char *endp;
        int PRoffset = 0;   // offset from PR[n] register (if any)
        int segno = (int)strtoll(seg, &endp, 8);
        if (endp == seg)
        {
            // not numeric...
            // 1st, see if it's a PR or alias thereof
            struct PRtab *prt = _prtab;
            while (prt->alias)
            {
                if (strcasecmp(seg, prt->alias) == 0)
                {
                    segno = PR[prt->n].SNR;
                    PRoffset = PR[prt->n].WORDNO;
                    
                    break;
                }
                
                prt += 1;
            }
            
            if (!prt->alias)    // not a PR or alias
            {
                segment *s = findSegmentNoCase(seg);
                if (s == NULL)
                {
                    sim_printf("parse_addr(): segment '%s' not found\n", seg);
                    *optr = cptr;   // signal error
                    
                    return 0;
                }
                segno = s->segno;
            }
        }
        
        // determine if offset is numeric or symbolic entry point/segdef...
        int offset = (int)strtoll(off, &endp, 8);
        if (endp == off)
        {
            // not numeric...
            segdef *s = findSegdefNoCase(seg, off);
            if (s == NULL)
            {
                sim_printf("parse_addr(): entrypoint '%s' not found in segment '%s'", off, seg);
                *optr = cptr;   // signal error

                return 0;
            }
            offset = s->value;
        }
        
        // if we get here then seg contains a segment# and offset.
        // So, fetch the actual address given the segment & offset ...
        // ... and return this absolute, 24-bit address
        
        t_addr absAddr = getAddress(segno, offset + PRoffset);
        
        // TODO: only luckily does this work FixMe
        *optr = endp;   //cptr + strlen(cptr);
        
        return absAddr;
    }
    else
    {
        // a PR or alias thereof
        int segno = 0;
        int offset = 0;
        struct PRtab *prt = _prtab;
        while (prt->alias)
        {
            if (strncasecmp(cptr, prt->alias, strlen(prt->alias)) == 0)
            {
                segno = PR[prt->n].SNR;
                offset = PR[prt->n].WORDNO;
                break;
            }
            
            prt += 1;
        }
        if (prt->alias)    // a PR or alias
        {
            t_addr absAddr = getAddress(segno, offset);
            *optr = cptr + strlen(prt->alias);
        
            return absAddr;
        }
    }
    
    // No, determine absolute address given by cptr
    return (t_addr)strtol(cptr, optr, 8);
}

static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr simh_addr)
{
    char temp[256];
    bool bFound = getSegmentAddressString(simh_addr, temp);
    if (bFound)
        fprintf(stream, "%s (%08o)", temp, simh_addr);
    else
        fprintf(stream, "%06o", simh_addr);
}

// This is part of the simh interface
/*! Based on the switch variable, symbolically output to stream ofile the data in array val at the specified addr
 in unit uptr.
 */
t_stat fprint_sym (FILE *ofile, t_addr addr, t_value *val, UNIT *uptr, int32 sswitch)
{
// XXX Bug: assumes single cpu
// XXX CAC: This seems rather bogus; deciding the output format based on the
// address of the UNIT? Would it be better to use sim_unit.u3 (or some such 
// as a word width?

    if (uptr == &cpu_unit[0])
    {
        fprintf(ofile, "%012llo", *val);
        return SCPE_OK;
    }
    return SCPE_ARG;
}

// This is part of the simh interface
/*!  – Based on the switch variable, parse character string cptr for a symbolic value val at the specified addr
 in unit uptr.
 */
t_stat parse_sym (char *cptr, t_addr addr, UNIT *uptr, t_value *val, int32 sswitch)
{
    return SCPE_ARG;
}

// from MM

sysinfo_t sys_opts =
  {
    0, /* clock speed */
    {
      0, /* iom_times.connect */
      0  /* iom_times.chan_activate */
    },
    {
// XXX This suddenly started working when I reworked the iom code for multiple units.
// XXX No idea why. However, setting it to zero queues the boot tape read instead of
// XXX performing it immediately. This makes the boot code fail because iom_boot 
// XXX returns before the read is dequeued, causing the CPU to start before the
// XXX tape is read into memory. 
// XXX Need to fix the cpu code to either do actual fault loop on unitialized memory, or force it into the wait for interrupt sate; and respond to the interrupt from the IOM's completion of the read.
//
      -1, /* mt_times.read */
      0  /* mt_times.xfer */
    },
    0, /* warn_uninit */
    0  /* startup_interrupt */
  };

// from MM


//-----------------------------------------------------------------------------
// *** Other devices


//=============================================================================



// This is part of the simh interface
DEVICE * sim_devices [] =
  {
    & cpu_dev,
    & iom_dev,
    & tape_dev,
    & scu_dev,
    & clk_dev,
    // & mpc_dev,
//    & opcon_dev, // Not hooked up yet
//    & disk_dev, // Not hooked up yet

    NULL
};

