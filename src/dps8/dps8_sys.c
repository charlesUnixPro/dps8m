/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony
 Copyright 2016 by Michal Tomek

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

/**
 * \file dps8_sys.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#ifndef __MINGW64__
#include <wordexp.h>
#include <signal.h>
#endif
#include <unistd.h>

#ifdef __APPLE__
#include <pthread.h>
#endif

#include "dps8.h"
#include "dps8_console.h"
#ifndef QUIET_UNUSED
#include "dps8_clk.h"
#endif
#include "dps8_sys.h"
#include "dps8_faults.h"
#include "dps8_cpu.h"
#include "dps8_ins.h"
#include "dps8_iom.h"
#include "dps8_loader.h"
#include "dps8_math.h"
#include "dps8_scu.h"
#include "dps8_mt.h"
#include "dps8_disk.h"
#include "dps8_utils.h"
#include "dps8_append.h"
#ifdef FNP2
#include "dps8_fnp2.h"
#else
#include "dps8_fnp.h"
#include "fnp.h"
#endif
#include "dps8_crdrdr.h"
#include "dps8_crdpun.h"
#include "dps8_prt.h"
#include "dps8_urp.h"
#include "dps8_cable.h"
#include "dps8_absi.h"
#include "utlist.h"
#include "hdbg.h"

// XXX Strictly speaking, memory belongs in the SCU
// We will treat memory as viewed from the CPU and elide the
// SCU configuration that maps memory across multiple SCUs.
// I would guess that multiple SCUs helped relieve memory
// contention across multiple CPUs, but that is a level of
// emulation that will be ignored.

word36 *M = NULL;                                          /*!< memory */


// These are part of the simh interface
#ifdef DPS8M
char sim_name[] = "DPS8M";
#endif
#ifdef L68
char sim_name[] = "L68";
#endif
int32 sim_emax = 4; ///< some EIS can take up to 4-words
static void dps8_init(void);
void (*sim_vm_init) (void) = & dps8_init;    //CustomCmds;

#ifndef __MINGW64__
static pid_t dps8m_sid; // Session id
#endif

static char * lookupSystemBookAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset);


stats_t sys_stats;

static t_stat dps_debug_mme_cntdwn (UNUSED int32 arg, const char * buf);
static t_stat dps_debug_skip (int32 arg, const char * buf);
static t_stat dps_debug_start (int32 arg, const char * buf);
static t_stat dps_debug_stop (int32 arg, const char * buf);
static t_stat dps_debug_break (int32 arg, const char * buf);
static t_stat dps_debug_segno (int32 arg, const char * buf);
static t_stat dps_debug_ringno (int32 arg, const char * buf);
static t_stat dps_debug_bar (int32 arg, UNUSED const char * buf);
static t_stat loadSystemBook (int32 arg, const char * buf);
static t_stat addSystemBookEntry (int32 arg, const char * buf);
static t_stat lookupSystemBook (int32 arg, const char * buf);
static t_stat absAddr (int32 arg, const char * buf);
static t_stat setSearchPath (int32 arg, const char * buf);
static t_stat absAddrN (int segno, uint offset);
//static t_stat virtAddrN (uint address);

static t_stat virtAddr (int32 arg, const char * buf);
static t_stat sbreak (int32 arg, const char * buf);
static t_stat stackTrace (int32 arg, const char * buf);
static t_stat listSourceAt (int32 arg, const char * buf);
static t_stat doEXF (UNUSED int32 arg,  UNUSED const char * buf);
#define LAUNCH
#ifdef LAUNCH
static t_stat launch (int32 arg, const char * buf);
#endif
static t_stat defaultBaseSystem (int32 arg, const char * buf);
#ifdef DVFDBG
static t_stat dfx1entry (int32 arg, const char * buf);
static t_stat dfx1exit (int32 arg, const char * buf);
static t_stat dv2scale (int32 arg, const char * buf);
static t_stat dfx2entry (int32 arg, const char * buf);
static t_stat mdfx3entry (int32 arg, const char * buf);
static t_stat smfx1entry (int32 arg, const char * buf);
#endif
static t_stat searchMemory (UNUSED int32 arg, const char * buf);
static t_stat bootSkip (int32 UNUSED arg, const char * UNUSED buf);

static CTAB dps8_cmds[] =
{
    {"DPSINIT",  dpsCmd_Init,     0, "dpsinit dps8/m initialize stuff ...\n", NULL, NULL},
    {"DPSDUMP",  dpsCmd_Dump,     0, "dpsdump dps8/m dump stuff ...\n", NULL, NULL},
    {"SEGMENT",  dpsCmd_Segment,  0, "segment dps8/m segment stuff ...\n", NULL, NULL},
    {"SEGMENTS", dpsCmd_Segments, 0, "segments dps8/m segments stuff ...\n", NULL, NULL},
    {"CABLE",    sys_cable,       0, "cable String a cable\n" , NULL, NULL},
    {"CABLE_RIPOUT",    sys_cable_ripout,       0, "cable Unstring all cables\n" , NULL, NULL},
    {"DBGMMECNTDWN", dps_debug_mme_cntdwn, 0, "dbgmmecntdwn Enable debug after n MMEs\n", NULL, NULL},
    {"DBGSKIP", dps_debug_skip, 0, "dbgskip Skip first n TRACE debugs\n", NULL, NULL},
    {"DBGSTART", dps_debug_start, 0, "dbgstart Limit debugging to N > Cycle count\n", NULL, NULL},
    {"DBGSTOP", dps_debug_stop, 0, "dbgstop Limit debugging to N < Cycle count\n", NULL, NULL},
    {"DBGBREAK", dps_debug_break, 0, "dbgstop Break when N >= Cycle count\n", NULL, NULL},
    {"DBGSEGNO", dps_debug_segno, 0, "dbgsegno Limit debugging to PSR == segno\n", NULL, NULL},
    {"DBGRINGNO", dps_debug_ringno, 0, "dbgsegno Limit debugging to PRR == ringno\n", NULL, NULL},
    {"DBGBAR", dps_debug_bar, 1, "dbgbar Limit debugging to BAR mode\n", NULL, NULL},
    {"NODBGBAR", dps_debug_bar, 0, "dbgbar Limit debugging to BAR mode\n", NULL, NULL},
    {"HDBG", hdbg_size, 0, "set hdbg size\n", NULL, NULL},
    {"DISPLAYMATRIX", displayTheMatrix, 0, "displaymatrix Display instruction usage counts\n", NULL, NULL},
    {"LD_SYSTEM_BOOK", loadSystemBook, 0, "load_system_book: Load a Multics system book for symbolic debugging\n", NULL, NULL},
    {"ASBE", addSystemBookEntry, 0, "asbe: Add an entry to the system book\n", NULL, NULL},
    {"LOOKUP_SYSTEM_BOOK", lookupSystemBook, 0, "lookup_system_book: lookup an address or symbol in the Multics system book\n", NULL, NULL},
    {"LSB", lookupSystemBook, 0, "lsb: lookup an address or symbol in the Multics system book\n", NULL, NULL},
    {"ABSOLUTE", absAddr, 0, "abs: Compute the absolute address of segno:offset\n", NULL, NULL},
    {"VIRTUAL", virtAddr, 0, "virtual: Compute the virtural address(es) of segno:offset\n", NULL, NULL},
    {"SPATH", setSearchPath, 0, "spath: Set source code search path\n", NULL, NULL},
    {"TEST", brkbrk, 0, "test: internal testing\n", NULL, NULL},
// copied from scp.c
#define SSH_ST          0                               /* set */
#define SSH_SH          1                               /* show */
#define SSH_CL          2                               /* clear */
    {"SBREAK", sbreak, SSH_ST, "sbreak: Set a breakpoint with segno:offset syntax\n", NULL, NULL},
    {"NOSBREAK", sbreak, SSH_CL, "nosbreak: Unset an SBREAK\n", NULL, NULL},
    {"STK", stackTrace, 0, "stk: print a stack trace\n", NULL, NULL},
    {"LIST", listSourceAt, 0, "list segno:offet: list source for an address\n", NULL, NULL},
    {"XF", doEXF, 0, "Execute fault: Press the execute fault button\n", NULL, NULL},
#ifdef DVFDBG
    // dvf debugging
    {"DFX1ENTRY", dfx1entry, 0, "", NULL, NULL},
    {"DFX2ENTRY", dfx2entry, 0, "", NULL, NULL},
    {"DFX1EXIT", dfx1exit, 0, "", NULL, NULL},
    {"DV2SCALE", dv2scale, 0, "", NULL, NULL},
    {"MDFX3ENTRY", mdfx3entry, 0, "", NULL, NULL},
    {"SMFX1ENTRY", smfx1entry, 0, "", NULL, NULL},
#endif
    // doesn't work
    //{"DUMPKST", dumpKST, 0, "dumpkst: dump the Known Segment Table\n", NULL},
    {"WATCH", memWatch, 1, "watch: watch memory location\n", NULL, NULL},
    {"NOWATCH", memWatch, 0, "watch: watch memory location\n", NULL, NULL},
    {"AUTOINPUT", opconAutoinput, 0, "set console auto-input\n", NULL, NULL},
    {"CLRAUTOINPUT", opconAutoinput, 1, "clear console auto-input\n", NULL, NULL},
#ifdef LAUNCH
    {"LAUNCH", launch, 0, "start subprocess\n", NULL, NULL},
#endif
    
    {"SEARCHMEMORY", searchMemory, 0, "searchMemory: search memory for value\n", NULL, NULL},

    {"FNPLOAD", fnpLoad, 0, "fnpload: load Devices.txt into FNP", NULL, NULL},
#ifdef FNP2
    {"FNPSERVERPORT", fnpServerPort, 0, "fnpServerPort: set the FNP dialin telnter port number", NULL, NULL},
#endif
#ifdef EISTESTJIG
    // invoke EIS test jig.......∫
    {"ET", eisTest, 0, "invoke EIS test jig\n", NULL, NULL}, 
#endif
    {"SKIPBOOT", bootSkip, 0, "skip forward on boot tape", NULL, NULL},
    {"DEFAULT_BASE_SYSTEM", defaultBaseSystem, 0, "Set configuration to defaults", NULL, NULL},
    {"FNPSTART", fnpStart, 0, "Force early FNP initialization", NULL, NULL},
    { NULL, NULL, 0, NULL, NULL, NULL}
};

/*!
 \brief special dps8 VM commands ....
 
 For greater flexibility, SCP provides some optional interfaces that can be used to extend its command input, command processing, and command post-processing capabilities. These interfaces are strictly optional
 and are off by default. Using them requires intimate knowledge of how SCP functions internally and is not recommended to the novice VM writer.
 
 Guess I shouldn't use these then :)
 */

static t_addr parse_addr(DEVICE *dptr, const char *cptr, const char **optr);
static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr addr);

#ifndef __MINGW64__
static void usr1SignalHandler (UNUSED int sig)
  {
    sim_printf ("USR1 signal caught; pressing the EXF button\n");
    // Assume the bootload CPU
    setG7fault (0, FAULT_EXF, (_fault_subtype) {.bits=0});
    return;
  }
#endif

// Once-only initialization

static void dps8_init(void)
{
#include "dps8.sha1.txt"
#ifdef DPS8M
    sim_printf ("DPS8/M emulator (git %8.8s)\n", COMMIT_ID);
#endif
#ifdef L68
    sim_printf ("L68 emulator (git %8.8s)\n", COMMIT_ID);
#endif
#ifdef TESTING
    sim_printf ("#### TESTING BUILD ####\n");
#else
    sim_printf ("Production build\n");
#endif
#ifdef ISOLTS
    sim_printf ("#### ISOLTS BUILD ####\n");
#endif

    // special dps8 initialization stuff that cant be done in reset, etc .....

    // These are part of the simh interface
    sim_vm_parse_addr = parse_addr;
    sim_vm_fprint_addr = fprint_addr;

    sim_vm_cmd = dps8_cmds;

#ifndef __MINGW64__
    // Create a session for this dps8m system instance.
    dps8m_sid = setsid ();
    if (dps8m_sid == (pid_t) -1)
      dps8m_sid = getsid (0);
#ifdef DPS8M
    sim_printf ("DPS8M system session id is %d\n", dps8m_sid);
#endif
#ifdef L68
    sim_printf ("L68 system session id is %d\n", dps8m_sid);
#endif
#endif

#ifndef __MINGW64__
    // Wire the XF button to signal USR1
    signal (SIGUSR1, usr1SignalHandler);
#endif

    init_opcodes();
    sysCableInit ();
    iom_init ();
    console_init ();
    disk_init ();
    mt_init ();
    fnpInit ();
    //mpc_init ();
    scu_init ();
    cpu_init ();
    crdrdr_init ();
    crdpun_init ();
    prt_init ();
    urp_init ();
#ifndef __MINGW64__
    absi_init ();
#endif
    defaultBaseSystem (0, NULL);
}

uint64 sim_deb_start = 0;
uint64 sim_deb_stop = 0;
uint64 sim_deb_break = 0;
uint64 sim_deb_segno = NO_SUCH_SEGNO;
uint64 sim_deb_ringno = NO_SUCH_RINGNO;
uint64 sim_deb_skip_limit = 0;
uint64 sim_deb_skip_cnt = 0;
uint64 sim_deb_mme_cntdwn = 0;

bool sim_deb_bar = false;

static t_stat dps_debug_mme_cntdwn (UNUSED int32 arg, const char * buf)
  {
    sim_deb_mme_cntdwn = strtoull (buf, NULL, 0);
    sim_printf ("Debug MME countdown set to %"PRId64"\n", sim_deb_mme_cntdwn);
    return SCPE_OK;
  }

static t_stat dps_debug_skip (UNUSED int32 arg, const char * buf)
  {
    sim_deb_skip_cnt = 0;
    sim_deb_skip_limit = strtoull (buf, NULL, 0);
    sim_printf ("Debug skip set to %"PRId64"\n", sim_deb_skip_limit);
    return SCPE_OK;
  }

static t_stat dps_debug_start (UNUSED int32 arg, const char * buf)
  {
    sim_deb_start = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to start at cycle: %"PRId64"\n", sim_deb_start);
    return SCPE_OK;
  }

static t_stat dps_debug_stop (UNUSED int32 arg, const char * buf)
  {
    sim_deb_stop = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to stop at cycle: %"PRId64"\n", sim_deb_stop);
    return SCPE_OK;
  }

static t_stat dps_debug_break (UNUSED int32 arg, const char * buf)
  {
    sim_deb_break = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to break at cycle: %"PRId64"\n", sim_deb_break);
    return SCPE_OK;
  }

static t_stat dps_debug_segno (UNUSED int32 arg, const char * buf)
  {
    sim_deb_segno = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to segno %"PRIo64"\n", sim_deb_segno);
    return SCPE_OK;
  }

static t_stat dps_debug_ringno (UNUSED int32 arg, const char * buf)
  {
    sim_deb_ringno = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to ringno %"PRIo64"\n", sim_deb_ringno);
    return SCPE_OK;
  }

static t_stat dps_debug_bar (int32 arg, UNUSED const char * buf)
  {
    sim_deb_bar = arg;
    if (arg)
      sim_printf ("Debug set BAR %"PRIo64"\n", sim_deb_ringno);
    else
      sim_printf ("Debug unset BAR %"PRIo64"\n", sim_deb_ringno);
    return SCPE_OK;
  }

// LOAD_SYSTEM_BOOK <filename>
#define bookSegmentsMax 1024
#define bookComponentsMax 4096
#define bookSegmentNameLen 33
static struct bookSegment
  {
    char * segname;
    int segno;
  } bookSegments [bookSegmentsMax];

static int nBookSegments = 0;

static struct bookComponent
  {
    char * compname;
    int bookSegmentNum;
    uint txt_start, txt_length;
    int intstat_start, intstat_length, symbol_start, symbol_length;
  } bookComponents [bookComponentsMax];

static int nBookComponents = 0;

static int lookupBookSegment (char * name)
  {
    for (int i = 0; i < nBookSegments; i ++)
      if (strcmp (name, bookSegments [i] . segname) == 0)
        return i;
    return -1;
  }

static int addBookSegment (char * name, int segno)
  {
    int n = lookupBookSegment (name);
    if (n >= 0)
      return n;
    if (nBookSegments >= bookSegmentsMax)
      return -1;
    bookSegments [nBookSegments] . segname = strdup (name);
    bookSegments [nBookSegments] . segno = segno;
    n = nBookSegments;
    nBookSegments ++;
    return n;
  }
 
static int addBookComponent (int segnum, char * name, uint txt_start, uint txt_length, int intstat_start, int intstat_length, int symbol_start, int symbol_length)
  {
    if (nBookComponents >= bookComponentsMax)
      return -1;
    bookComponents [nBookComponents] . compname = strdup (name);
    bookComponents [nBookComponents] . bookSegmentNum = segnum;
    bookComponents [nBookComponents] . txt_start = txt_start;
    bookComponents [nBookComponents] . txt_length = txt_length;
    bookComponents [nBookComponents] . intstat_start = intstat_start;
    bookComponents [nBookComponents] . intstat_length = intstat_length;
    bookComponents [nBookComponents] . symbol_start = symbol_start;
    bookComponents [nBookComponents] . symbol_length = symbol_length;
    int n = nBookComponents;
    nBookComponents ++;
    return n;
  }
 

char * lookupAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset)
  {
    if (compname)
      * compname = NULL;
    if (compoffset)
      * compoffset = 0;

    // Magic numbers!
    // Multics seems to have a copy of hpchs_ (segno 0162) in segment 0322;
    // This little tweak allows source code level tracing for segment 0322,
    // and has no operational significance to the emulator
    // Hmmm. What is happening is that these segments are being loaded into
    // ring 4, and assigned segment #'s; the assigned number will vary 
    // depending on the exact sequence of events.
    if (segno == 0322)
      segno = 0162;
    if (segno == 0310)
      segno = 041;
    if (segno == 0314)
      segno = 041;
    if (segno == 0313)
      segno = 040;
    if (segno == 0317)
      segno = 0161;

#if 0
    // Hack to support formline debugging
#define IOPOS 02006 // interpret_op_ptr_ offset
    if (segno == 0371)
      {
        if (offset < IOPOS)
          {
            if (compname)
              * compname = "find_condition_info_";
            if (compoffset)
              * compoffset = offset;
            static char buf [129];
            sprintf (buf, "bound_debug_util_:find_condition_info_+0%0o", 
                  offset - 0);
            return buf;
          }
        else
          {
            if (compname)
              * compname = "interpret_op_ptr_";
            if (compoffset)
              * compoffset = offset - IOPOS;
            static char buf [129];
            sprintf (buf, "bound_debug_util_:interpret_op_ptr_+0%0o", 
                  offset - IOPOS);
            return buf;
          }

      }
#endif

    char * ret = lookupSystemBookAddress (segno, offset, compname, compoffset);
    if (ret)
      return ret;
    ret = lookupSegmentAddress (segno, offset, compname, compoffset);
    return ret;
  }

// Warning: returns ptr to static buffer
static char * lookupSystemBookAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset)
  {
    static char buf [129];
    int i;
    for (i = 0; i < nBookSegments; i ++)
      if (bookSegments [i] . segno == (int) segno)
        break;
    if (i >= nBookSegments)
      return NULL;

    int best = -1;
    uint bestoffset = 0;

    for (int j = 0; j < nBookComponents; j ++)
      {
        if (bookComponents [j] . bookSegmentNum != i)
          continue;
        if (bookComponents [j] . txt_start <= offset &&
            bookComponents [j] . txt_start + bookComponents [j] . txt_length > offset)
          {
            sprintf (buf, "%s:%s+0%0o", bookSegments [i] . segname,
              bookComponents [j].compname,
              offset - bookComponents [j] . txt_start);
            if (compname)
              * compname = bookComponents [j].compname;
            if (compoffset)
              * compoffset = offset - bookComponents [j] . txt_start;
            return buf;
          }
        if (bookComponents [j] . txt_start <= offset &&
            bookComponents [j] . txt_start > bestoffset)
          {
            best = j;
            bestoffset = bookComponents [j] . txt_start;
          }
      }

    if (best != -1)
      {
        // Didn't find a component track bracketed the offset; return the
        // component that was before the offset
        if (compname)
          * compname = bookComponents [best].compname;
        if (compoffset)
          * compoffset = offset - bookComponents [best] . txt_start;
        sprintf (buf, "%s:%s+0%0o", bookSegments [i] . segname,
          bookComponents [best].compname,
          offset - bookComponents [best] . txt_start);
        return buf;
      }

    // Found a segment, but it had no components. Return the segment name
    // as the component name

    if (compname)
      * compname = bookSegments [i] . segname;
    if (compoffset)
      * compoffset = offset;
    sprintf (buf, "%s:+0%0o", bookSegments [i] . segname,
             offset);
    return buf;
 }

// Warning: returns ptr to static buffer
static int lookupSystemBookName (char * segname, char * compname, long * segno, long * offset)
  {
    int i;
    for (i = 0; i < nBookSegments; i ++)
      if (strcmp (bookSegments [i] . segname, segname) == 0)
        break;
    if (i >= nBookSegments)
      return -1;

    for (int j = 0; j < nBookComponents; j ++)
      {
        if (bookComponents [j] . bookSegmentNum != i)
          continue;
        if (strcmp (bookComponents [j] . compname, compname) == 0)
          {
            * segno = bookSegments [i] . segno;
            * offset = (long) bookComponents[j].txt_start;
            return 0;
          }
      }

   return -1;
 }

static char * sourceSearchPath = NULL;

// search path is path:path:path....

static t_stat setSearchPath (UNUSED int32 arg, UNUSED const char * buf)
  {
// Quietly ignore if debugging not enabled
#ifndef SPEED
    if (sourceSearchPath)
      free (sourceSearchPath);
    sourceSearchPath = strdup (buf);
#endif
    return SCPE_OK;
  }

t_stat brkbrk (UNUSED int32 arg, UNUSED const char *  buf)
  {
    //listSource (buf, 0);
    return SCPE_OK;
  }

static t_stat listSourceAt (UNUSED int32 arg, UNUSED const char *  buf)
  {
    // list seg:offset
    int segno;
    uint offset;
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    char * compname;
    word18 compoffset;
    char * where = lookupAddress ((word18) segno, offset,
                                  & compname, & compoffset);
    if (where)
      {
        sim_printf ("%05o:%06o %s\n", segno, offset, where);
        listSource (compname, compoffset, 0);
      }
    return SCPE_OK;
  }

void listSource (char * compname, word18 offset, uint dflag)
  {
    const int offset_str_len = 10;
    //char offset_str [offset_str_len + 1];
    char offset_str [17];
    sprintf (offset_str, "    %06o", offset);

    char path [(sourceSearchPath ? strlen (sourceSearchPath) : 1) + 
               1 + // "/"
               (compname ? strlen (compname) : 1) +
                1 + strlen (".list") + 1];
    char * searchp = sourceSearchPath ? sourceSearchPath : ".";
    // find <search path>/<compname>.list
    while (* searchp)
      {
        size_t pathlen = strcspn (searchp, ":");
        strncpy (path, searchp, pathlen);
        path [pathlen] = '\0';
        if (searchp [pathlen] == ':')
          searchp += pathlen + 1;
        else
          searchp += pathlen;

        if (compname)
          {
            strcat (path, "/");
            strcat (path, compname);
          }
        strcat (path, ".list");
        //sim_printf ("<%s>\n", path);
        FILE * listing = fopen (path, "r");
        if (listing)
          {
            char line [1025];
            if (feof (listing))
              goto fileDone;
            fgets (line, 1024, listing);
            if (strncmp (line, "ASSEMBLY LISTING", 16) == 0)
              {
                // Search ALM listing file
                // sim_printf ("found <%s>\n", path);

                // ALM listing files look like:
                //     000226  4a  4 00010 7421 20  \tstx2]tbootload_0$entry_stack_ptr,id
                while (! feof (listing))
                  {
                    fgets (line, 1024, listing);
                    if (strncmp (line, offset_str, (size_t) offset_str_len) == 0)
                      {
                        if (! dflag)
                          sim_printf ("%s", line);
                        else
                          sim_debug (dflag, & cpu_dev, "%s", line);
                        //break;
                      }
                    if (strcmp (line, "\fLITERALS\n") == 0)
                      break;
                  }
              } // if assembly listing
            else if (strncmp (line, "\tCOMPILATION LISTING", 20) == 0)
              {
                // Search PL/I listing file

                // PL/I files have a line location table
                //     "   LINE    LOC      LINE    LOC ...."

                bool foundTable = false;
                while (! feof (listing))
                  {
                    fgets (line, 1024, listing);
                    if (strncmp (line, "   LINE    LOC", 14) != 0)
                      continue;
                    foundTable = true;
                    // Found the table
                    // Table lines look like
                    //     "     13 000705       275 000713  ...
                    // But some times
                    //     "     10 000156   21   84 000164
                    //     "      8 000214        65 000222    4   84 000225    
                    //
                    //     "    349 001442       351 001445       353 001454    1    9 001456    1   11 001461    1   12 001463    1   13 001470
                    //     " 1   18 001477       357 001522       361 001525       363 001544       364 001546       365 001547       366 001553

                    //  I think the numbers refer to include files...
                    //   But of course the format is slightly off...
                    //    table    ".1...18
                    //    listing  ".1....18
                    int best = -1;
                    char bestLines [8] = {0, 0, 0, 0, 0, 0, 0};
                    while (! feof (listing))
                      {
                        int loc [7];
                        char linenos [7] [8];
                        memset (linenos, 0, sizeof (linenos));
                        fgets (line, 1024, listing);
                        // sometimes the leading columns are blank...
                        while (strncmp (line, "                 ", 8 + 6 + 3) == 0)
                          memmove (line, line + 8 + 6 + 3, strlen (line + 8 + 6 + 3));
                        // deal with the extra numbers...

                        int cnt = sscanf (line,
                          // " %d %o %d %o %d %o %d %o %d %o %d %o %d %o", 
                          "%8c%o%*3c%8c%o%*3c%8c%o%*3c%8c%o%*3c%8c%o%*3c%8c%o%*3c%8c%o", 
                          (char *) & linenos [0], & loc [0], 
                          (char *) & linenos [1], & loc [1], 
                          (char *) & linenos [2], & loc [2], 
                          (char *) & linenos [3], & loc [3], 
                          (char *) & linenos [4], & loc [4], 
                          (char *) & linenos [5], & loc [5], 
                          (char *) & linenos [6], & loc [6]);
                        if (! (cnt == 2 || cnt == 4 || cnt == 6 ||
                               cnt == 8 || cnt == 10 || cnt == 12 ||
                               cnt == 14))
                          break; // end of table
                        int n;
                        for (n = 0; n < cnt / 2; n ++)
                          {
                            if (loc [n] > best && loc [n] <= (int) offset)
                              {
                                best = loc [n];
                                memcpy (bestLines, linenos [n], sizeof (bestLines));
                              }
                          }
                        if (best == (int) offset)
                          break;
                      }
                    if (best == -1)
                      goto fileDone; // Not found in table

                    //   But of course the format is slightly off...
                    //    table    ".1...18
                    //    listing  ".1....18
                    // bestLines "21   84 "
                    // listing   " 21    84 "
                    char searchPrefix [10];
                    searchPrefix [ 0] = ' ';
                    searchPrefix [ 1] = bestLines [ 0];
                    searchPrefix [ 2] = bestLines [ 1];
                    searchPrefix [ 3] = ' ';
                    searchPrefix [ 4] = bestLines [ 2];
                    searchPrefix [ 5] = bestLines [ 3];
                    searchPrefix [ 6] = bestLines [ 4];
                    searchPrefix [ 7] = bestLines [ 5];
                    searchPrefix [ 8] = bestLines [ 6];
                    // ignore trailing space; some times its a tab
                    // searchPrefix [ 9] = bestLines [ 7];
                    searchPrefix [9] = '\0';

                    // Look for the line in the listing
                    rewind (listing);
                    while (! feof (listing))
                      {
                        fgets (line, 1024, listing);
                        if (strncmp (line, "\f\tSOURCE", 8) == 0)
                          goto fileDone; // end of source code listing
                        char prefix [10];
                        strncpy (prefix, line, 9);
                        prefix [9] = '\0';
                        if (strcmp (prefix, searchPrefix) != 0)
                          continue;
                        // Got it
                        if (!dflag)
                          sim_printf ("%s", line);
                        else
                          sim_debug (dflag, & cpu_dev, "%s", line);
                        //break;
                      }
                    goto fileDone;
                  } // if table start
                if (! foundTable)
                  {
                    // Can't find the LINE/LOC table; look for listing
                    rewind (listing);
                    while (! feof (listing))
                      {
                        fgets (line, 1024, listing);
                        if (strncmp (line, offset_str + 4, offset_str_len - 4) == 0)
                          {
                            if (! dflag)
                              sim_printf ("%s", line);
                            else
                              sim_debug (dflag, & cpu_dev, "%s", line);
                            //break;
                          }
                        //if (strcmp (line, "\fLITERALS\n") == 0)
                          //break;
                      }
                  } // if ! tableFound
              } // if PL/I listing
                        
fileDone:
            fclose (listing);
          } // if (listing)
      }
  }

// SEARCHMEMORY valye

static t_stat searchMemory (UNUSED int32 arg, const char * buf)
  {
    word36 value;
    if (sscanf (buf, "%"PRIo64"", & value) != 1)
      return SCPE_ARG;
    
    uint i;
    for (i = 0; i < MEMSIZE; i ++)
      if ((M [i] & DMASK) == value)
        sim_printf ("%08o\n", i);
    return SCPE_OK;
  }


// ABS segno:offset

static t_stat absAddr (UNUSED int32 arg, const char * buf)
  {
    int segno;
    uint offset;
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    return absAddrN (segno, offset);
  }

#if 0
t_stat computeAbsAddrN (word24 * absAddr, int segno, uint offset)
  {
    word24 res;

    if (get_addr_mode () != APPEND_mode)
      {
        sim_printf ("CPU not in append mode\n");
        return SCPE_ARG;
      }

    if (cpu . DSBR.U == 1) // Unpaged
      {
        if (2 * (uint) /*TPR . TSR*/ segno >= 16 * ((uint) cpu . DSBR . BND + 1))
          {
            sim_printf ("DSBR boundary violation.\n");
            return SCPE_ARG;
          }

        // 2. Fetch the target segment SDW from cpu . DSBR.ADDR + 2 * segno.

        word36 SDWe, SDWo;
        core_read ((cpu . DSBR . ADDR + 2U * /*TPR . TSR*/ (uint) segno) & PAMASK, & SDWe, __func__);
        core_read ((cpu . DSBR . ADDR + 2U * /*TPR . TSR*/ (uint) segno  + 1) & PAMASK, & SDWo, __func__);

        // 3. If SDW.F = 0, then generate directed fault n where n is given in
        // SDW.FC. The value of n used here is the value assigned to define a
        // missing segment fault or, simply, a segment fault.

        // absAddr doesn't care if the page isn't resident


        // 4. If offset >= 16 * (SDW.BOUND + 1), then generate an access violation, out of segment bounds, fault.

        word14 BOUND = (SDWo >> (35 - 14)) & 037777;
        if (/*TPR . CA*/ offset >= 16 * (BOUND + 1))
          {
            sim_printf ("SDW boundary violation.\n");
            return SCPE_ARG;
          }

        // 5. If the access bits (SDW.R, SDW.E, etc.) of the segment are incompatible with the reference, generate the appropriate access violation fault.

        // absAddr doesn't care

        // 6. Generate 24-bit absolute main memory address SDW.ADDR + offset.

        word24 ADDR = (SDWe >> 12) & 077777760;
        res = (word24) ADDR + (word24) /*TPR.CA*/ offset;
        res &= PAMASK; //24 bit math
        //res <<= 12; // 24:12 format

      }
    else
      {
        //word15 segno = TPR . TSR;
        //word18 offset = TPR . CA;

        // 1. If 2 * segno >= 16 * (cpu . DSBR.BND + 1), then generate an access 
        // violation, out of segment bounds, fault.

        if (2 * (uint) segno >= 16 * ((uint) cpu . DSBR . BND + 1))
          {
            sim_printf ("DSBR boundary violation.\n");
            return SCPE_ARG;
          }

        // 2. Form the quantities:
        //       y1 = (2 * segno) modulo 1024
        //       x1 = (2 * segno ­ y1) / 1024

        word24 y1 = (2 * (uint) segno) % 1024;
        word24 x1 = (2 * (uint) segno - y1) / 1024;

        // 3. Fetch the descriptor segment PTW(x1) from DSBR.ADR + x1.

        word36 PTWx1;
        core_read ((cpu . DSBR . ADDR + x1) & PAMASK, & PTWx1, __func__);

        _ptw0 PTW1;
        PTW1.ADDR = GETHI(PTWx1);
        PTW1.U = TSTBIT(PTWx1, 9);
        PTW1.M = TSTBIT(PTWx1, 6);
        PTW1.F = TSTBIT(PTWx1, 2);
        PTW1.FC = PTWx1 & 3;

        // 4. If PTW(x1).F = 0, then generate directed fault n where n is 
        // given in PTW(x1).FC. The value of n used here is the value 
        // assigned to define a missing page fault or, simply, a
        // page fault.

        if (!PTW1.F)
          {
            sim_printf ("!PTW1.F\n");
            return SCPE_ARG;
          }

        // 5. Fetch the target segment SDW, SDW(segno), from the 
        // descriptor segment page at PTW(x1).ADDR + y1.

        word36 SDWeven, SDWodd;
        core_read2(((PTW1 . ADDR << 6) + y1) & PAMASK, & SDWeven, & SDWodd, __func__);

        _sdw0 SDW0; 
        // even word
        SDW0.ADDR = (SDWeven >> 12) & PAMASK;
        SDW0.R1 = (SDWeven >> 9) & 7;
        SDW0.R2 = (SDWeven >> 6) & 7;
        SDW0.R3 = (SDWeven >> 3) & 7;
        SDW0.F = TSTBIT(SDWeven, 2);
        SDW0.FC = SDWeven & 3;

        // odd word
        SDW0.BOUND = (SDWodd >> 21) & 037777;
        SDW0.R = TSTBIT(SDWodd, 20);
        SDW0.E = TSTBIT(SDWodd, 19);
        SDW0.W = TSTBIT(SDWodd, 18);
        SDW0.P = TSTBIT(SDWodd, 17);
        SDW0.U = TSTBIT(SDWodd, 16);
        SDW0.G = TSTBIT(SDWodd, 15);
        SDW0.C = TSTBIT(SDWodd, 14);
        SDW0.EB = SDWodd & 037777;

        // 6. If SDW(segno).F = 0, then generate directed fault n where 
        // n is given in SDW(segno).FC.
        // This is a segment fault as discussed earlier in this section.

        if (!SDW0.F)
          {
            sim_printf ("!SDW0.F\n");
            return SCPE_ARG;
          }

        // 7. If offset >= 16 * (SDW(segno).BOUND + 1), then generate an 
        // access violation, out of segment bounds, fault.

        if (((offset >> 4) & 037777) > SDW0 . BOUND)
          {
            sim_printf ("SDW boundary violation\n");
            return SCPE_ARG;
          }

        // 8. If the access bits (SDW(segno).R, SDW(segno).E, etc.) of the 
        // segment are incompatible with the reference, generate the 
        // appropriate access violation fault.

        // Only the address is wanted, so no check

        if (SDW0.U == 0)
          {
            // Segment is paged
            // 9. Form the quantities:
            //    y2 = offset modulo 1024
            //    x2 = (offset - y2) / 1024

            word24 y2 = offset % 1024;
            word24 x2 = (offset - y2) / 1024;
    
            // 10. Fetch the target segment PTW(x2) from SDW(segno).ADDR + x2.

            word36 PTWx2;
            core_read ((SDW0 . ADDR + x2) & PAMASK, & PTWx2, __func__);
    
            _ptw0 PTW2;
            PTW2.ADDR = GETHI(PTWx2);
            PTW2.U = TSTBIT(PTWx2, 9);
            PTW2.M = TSTBIT(PTWx2, 6);
            PTW2.F = TSTBIT(PTWx2, 2);
            PTW2.FC = PTWx2 & 3;

            // 11.If PTW(x2).F = 0, then generate directed fault n where n is 
            // given in PTW(x2).FC. This is a page fault as in Step 4 above.

            // absAddr only wants the address; it doesn't care if the page is
            // resident

            // if (!PTW2.F)
            //   {
            //     sim_debug (DBG_APPENDING, & cpu_dev, "absa fault !PTW2.F\n");
            //     // initiate a directed fault
            //     doFault(FAULT_DF0 + PTW2.FC, 0, "ABSA !PTW2.F");
            //   }

            // 12. Generate the 24-bit absolute main memory address 
            // PTW(x2).ADDR + y2.

            res = (((word24) PTW2 . ADDR) << 6)  + (word24) y2;
            res &= PAMASK; //24 bit math
            //res <<= 12; // 24:12 format
          }
        else
          {
            // Segment is unpaged
            // SDW0.ADDR is the base address of the segment
            res = (word24) SDW0 . ADDR + offset;
            res &= PAMASK; //24 bit math
            res <<= 12; // 24:12 format
          }
      }

    * absAddr = res;
    return SCPE_OK;
  }
#endif

static t_stat absAddrN (int segno, uint offset)
  {
    word24 res;

    //t_stat rc = computeAbsAddrN (& res, segno, offset);
    if (dbgLookupAddress ((word18) segno, offset, & res, NULL))
      return SCPE_ARG;

    sim_printf ("Address is %08o\n", res);
    return SCPE_OK;
  }

// EXF

static t_stat doEXF (UNUSED int32 arg,  UNUSED const char * buf)
  {
    // Assume bootload CPU
    setG7fault (0, FAULT_EXF, (_fault_subtype) {.bits=0});
    return SCPE_OK;
  }

// STK 

#if 0
t_stat dbgStackTrace (void)
  {
    return stackTrace (0, "");
  }
#endif 

static t_stat stackTrace (UNUSED int32 arg,  UNUSED const char * buf)
  {
    char * msg;

    word15 icSegno = cpu . PPR . PSR;
    word18 icOffset = cpu . PPR . IC;
    
    sim_printf ("Entry ptr   %05o:%06o\n", icSegno, icOffset);
    
    char * compname;
    word18 compoffset;
    char * where = lookupAddress (icSegno, icOffset,
                                  & compname, & compoffset);
    if (where)
      {
        sim_printf ("%05o:%06o %s\n", icSegno, icOffset, where);
        listSource (compname, compoffset, 0);
      }
    sim_printf ("\n");

    // According to AK92
    //
    //  pr0/ap operator segment pointer
    //  pr6/sp stack frame pointer
    //  pr4/lp linkage section for the executing procedure
    //  pr7/sb stack base

    word15 fpSegno = cpu . PR [6] . SNR;
    word18 fpOffset = cpu . PR [6] . WORDNO;

    for (uint frameNo = 1; ; frameNo ++)
      {
        sim_printf ("Frame %d %05o:%06o\n", 
                    frameNo, fpSegno, fpOffset);

        word24 fp;
        if (dbgLookupAddress (fpSegno, fpOffset, & fp, & msg))
          {
            sim_printf ("can't lookup fp (%05o:%06o) because %s\n",
                    fpSegno, fpOffset, msg);
            break;
          }
    
        word15 prevfpSegno = (word15) ((M [fp + 16] >> 18) & MASK15);
        word18 prevfpOffset = (word18) ((M [fp + 17] >> 18) & MASK18);
    
        sim_printf ("Previous FP %05o:%06o\n", prevfpSegno, prevfpOffset);
    
        word15 returnSegno = (word15) ((M [fp + 20] >> 18) & MASK15);
        word18 returnOffset = (word18) ((M [fp + 21] >> 18) & MASK18);
    
        sim_printf ("Return ptr  %05o:%06o\n", returnSegno, returnOffset);
    
        if (returnOffset == 0)
          {
            if (frameNo == 1)
              {
                // try rX[7] as the return address
                sim_printf ("guessing X7 has a return address....\n");
                where = lookupAddress (icSegno, cpu . rX [7] - 1,
                                       & compname, & compoffset);
                if (where)
                  {
                    sim_printf ("%05o:%06o %s\n", icSegno, cpu . rX [7] - 1, where);
                    listSource (compname, compoffset, 0);
                  }
              }
          }
        else
          {
            where = lookupAddress (returnSegno, returnOffset - 1,
                                   & compname, & compoffset);
            if (where)
              {
                sim_printf ("%05o:%06o %s\n", returnSegno, returnOffset - 1, where);
                listSource (compname, compoffset, 0);
              }
          }

        word15 entrySegno = (word15) ((M [fp + 22] >> 18) & MASK15);
        word18 entryOffset = (word18) ((M [fp + 23] >> 18) & MASK18);
    
        sim_printf ("Entry ptr   %05o:%06o\n", entrySegno, entryOffset);
    
        where = lookupAddress (entrySegno, entryOffset,
                               & compname, & compoffset);
        if (where)
          {
            sim_printf ("%05o:%06o %s\n", entrySegno, entryOffset, where);
            listSource (compname, compoffset, 0);
          }
    
        word15 argSegno = (word15) ((M [fp + 26] >> 18) & MASK15);
        word18 argOffset = (word18) ((M [fp + 27] >> 18) & MASK18);
        sim_printf ("Arg ptr     %05o:%06o\n", argSegno, argOffset);
    
        word24 ap;
        if (dbgLookupAddress (argSegno, argOffset, & ap, & msg))
          {
            sim_printf ("can't lookup arg ptr (%05o:%06o) because %s\n",
                    argSegno, argOffset, msg);
            goto skipArgs;
          }
    
        word16 argCount = (word16) ((M [ap + 0] >> 19) & MASK17);
        word18 callType = (word18) (M [ap + 0] & MASK18);
        word16 descCount = (word16) ((M [ap + 1] >> 19) & MASK17);
        sim_printf ("arg_count   %d\n", argCount);
        switch (callType)
          {
            case 0u:
              sim_printf ("call_type Quick internal call\n");
              break;
            case 4u:
              sim_printf ("call_type Inter-segment\n");
              break;
            case 8u:
              sim_printf ("call_type Enviroment pointer\n");
              break;
            default:
              sim_printf ("call_type Unknown (%o)\n", callType);
              goto skipArgs;
              }
        sim_printf ("desc_count  %d\n", descCount);
    
#if 0
        if (descCount)
          {
            // XXX walk descriptor and arg list together
          }
        else
#endif
          {
            for (uint argno = 0; argno < argCount; argno ++)
              {
                uint argnoos = ap + 2 + argno * 2;
                word15 argnoSegno = (word15) ((M [argnoos] >> 18) & MASK15);
                word18 argnoOffset = (word18) ((M [argnoos + 1] >> 18) & MASK18);
                word24 argnop;
                if (dbgLookupAddress (argnoSegno, argnoOffset, & argnop, & msg))
                  {
                    sim_printf ("can't lookup arg%d ptr (%05o:%06o) because %s\n",
                                argno, argSegno, argOffset, msg);
                    continue;
                  }
                word36 argv = M [argnop];
                sim_printf ("arg%d value   %05o:%06o [%08o] %012"PRIo64" (%"PRIu64")\n", 
                            argno, argSegno, argOffset, argnop, argv, argv);
                sim_printf ("\n");
             }
         }
skipArgs:;

        sim_printf ("End of frame %d\n\n", frameNo);

        if (prevfpSegno == 077777 && prevfpOffset == 1)
          break;
        fpSegno = prevfpSegno;
        fpOffset = prevfpOffset;
      }
    return SCPE_OK;
  }

// SBREAK segno:offset

static t_stat sbreak (int32 arg, const char * buf)
  {
    //printf (">> <%s>\n", buf);
    int segno, offset;
    int where;
    int cnt = sscanf (buf, "%o:%o%n", & segno, & offset, & where);
    if (cnt != 2)
      {
        return SCPE_ARG;
      }
    char reformatted [strlen (buf) + 20];
    sprintf (reformatted, "0%04o%06o%s", segno, offset, buf + where);
    //printf (">> <%s>\n", reformatted);
    t_stat rc = brk_cmd (arg, reformatted);
    return rc;
  }

t_stat virtAddrN (uint address);

// VIRTUAL address

static t_stat virtAddr (UNUSED int32 arg, const char * buf)
  {
    uint address;
    if (sscanf (buf, "%o", & address) != 1)
      return SCPE_ARG;
    return virtAddrN (address);
  }

t_stat virtAddrN (uint address)
  {
    if (cpu . DSBR.U) {
        for(word15 segno = 0; 2u * segno < 16u * (cpu . DSBR.BND + 1u); segno += 1)
        {
            _sdw0 *s = fetchSDW(segno);
            if (address >= s -> ADDR && address < s -> ADDR + s -> BOUND * 16u)
              sim_printf ("  %06o:%06o\n", segno, address - s -> ADDR);
        }
    } else {
        for(word15 segno = 0; 2u * segno < 16u * (cpu . DSBR.BND + 1u); segno += 512u)
        {
            word24 y1 = (2u * segno) % 1024u;
            word24 x1 = (2u * segno - y1) / 1024u;
            word36 PTWx1;
            core_read ((cpu . DSBR . ADDR + x1) & PAMASK, & PTWx1, __func__);

            _ptw0 PTW1;
            PTW1.ADDR = GETHI(PTWx1);
            PTW1.U = TSTBIT(PTWx1, 9);
            PTW1.M = TSTBIT(PTWx1, 6);
            PTW1.DF = TSTBIT(PTWx1, 2);
            PTW1.FC = PTWx1 & 3;
           
            if (PTW1.DF == 0)
                continue;
            //sim_printf ("%06o  Addr %06o U %o M %o F %o FC %o\n", 
            //            segno, PTW1.ADDR, PTW1.U, PTW1.M, PTW1.F, PTW1.FC);
            //sim_printf ("    Target segment page table\n");
            for (word15 tspt = 0; tspt < 512u; tspt ++)
            {
                word36 SDWeven, SDWodd;
                core_read2(((PTW1 . ADDR << 6) + tspt * 2u) & PAMASK, & SDWeven, & SDWodd, __func__);
                _sdw0 SDW0;
                // even word
                SDW0.ADDR = (SDWeven >> 12) & PAMASK;
                SDW0.R1 = (SDWeven >> 9) & 7u;
                SDW0.R2 = (SDWeven >> 6) & 7u;
                SDW0.R3 = (SDWeven >> 3) & 7u;
                SDW0.DF = TSTBIT(SDWeven, 2);
                SDW0.FC = SDWeven & 3u;

                // odd word
                SDW0.BOUND = (SDWodd >> 21) & 037777;
                SDW0.R = TSTBIT(SDWodd, 20);
                SDW0.E = TSTBIT(SDWodd, 19);
                SDW0.W = TSTBIT(SDWodd, 18);
                SDW0.P = TSTBIT(SDWodd, 17);
                SDW0.U = TSTBIT(SDWodd, 16);
                SDW0.G = TSTBIT(SDWodd, 15);
                SDW0.C = TSTBIT(SDWodd, 14);
                SDW0.EB = SDWodd & 037777;

                if (SDW0.DF == 0)
                    continue;
                //sim_printf ("    %06o Addr %06o %o,%o,%o F%o BOUND %06o %c%c%c%c%c\n",
                //          tspt, SDW0.ADDR, SDW0.R1, SDW0.R2, SDW0.R3, SDW0.F, SDW0.BOUND, SDW0.R ? 'R' : '.', SDW0.E ? 'E' : '.', SDW0.W ? 'W' : '.', SDW0.P ? 'P' : '.', SDW0.U ? 'U' : '.');
                if (SDW0.U == 0)
                {
                    for (word18 offset = 0; offset < 16u * (SDW0.BOUND + 1u); offset += 1024)
                    {
                        word24 y2 = offset % 1024;
                        word24 x2 = (offset - y2) / 1024;

                        // 10. Fetch the target segment PTW(x2) from SDW(segno).ADDR + x2.

                        word36 PTWx2;
                        core_read ((SDW0 . ADDR + x2) & PAMASK, & PTWx2, __func__);

                        _ptw0 PTW2;
                        PTW2.ADDR = GETHI(PTWx2);
                        PTW2.U = TSTBIT(PTWx2, 9);
                        PTW2.M = TSTBIT(PTWx2, 6);
                        PTW2.DF = TSTBIT(PTWx2, 2);
                        PTW2.FC = PTWx2 & 3;

                        //sim_printf ("        %06o  Addr %06o U %o M %o F %o FC %o\n", 
                        //             offset, PTW2.ADDR, PTW2.U, PTW2.M, PTW2.F, PTW2.FC);
                        if (address >= PTW2.ADDR + offset && address < PTW2.ADDR + offset + 1024)
                          sim_printf ("  %06o:%06o\n", tspt, (address - offset) - PTW2.ADDR);

                      }
                  }
                else
                  {
                    if (address >= SDW0.ADDR && address < SDW0.ADDR + SDW0.BOUND * 16u)
                      sim_printf ("  %06o:%06o\n", tspt, address - SDW0.ADDR);
                  }
            }
        }
    }

    return SCPE_OK;

  }

// LSB n:n   given a segment number and offset, return a segment name,
//           component and offset in that component
//     sname:cname+offset
//           given a segment name, component name and offset, return
//           the segment number and offset
   
static t_stat lookupSystemBook (UNUSED int32  arg, const char * buf)
  {
    char w1 [strlen (buf)];
    char w2 [strlen (buf)];
    char w3 [strlen (buf)];
    long segno, offset;

    size_t colon = strcspn (buf, ":");
    if (buf [colon] != ':')
      return SCPE_ARG;

    strncpy (w1, buf, colon);
    w1 [colon] = '\0';
    //sim_printf ("w1 <%s>\n", w1);

    size_t plus = strcspn (buf + colon + 1, "+");
    if (buf [colon + 1 + plus] == '+')
      {
        strncpy (w2, buf + colon + 1, plus);
        w2 [plus] = '\0';
        strcpy (w3, buf + colon + 1 + plus + 1);
      }
    else
      {
        strcpy (w2, buf + colon + 1);
        strcpy (w3, "");
      }
    //sim_printf ("w1 <%s>\n", w1);
    //sim_printf ("w2 <%s>\n", w2);
    //sim_printf ("w3 <%s>\n", w3);

    char * end1;
    segno = strtol (w1, & end1, 8);
    char * end2;
    offset = strtol (w2, & end2, 8);

    if (* end1 == '\0' && * end2 == '\0' && * w3 == '\0')
      { 
        // n:n
        char * ans = lookupAddress ((word18) segno, (word18) offset, NULL, NULL);
        sim_printf ("%s\n", ans ? ans : "not found");
      }
    else
      {
        if (* w3)
          {
            char * end3;
            offset = strtol (w3, & end3, 8);
            if (* end3 != '\0')
              return SCPE_ARG;
          }
        else
          offset = 0;
        long comp_offset;
        int rc = lookupSystemBookName (w1, w2, & segno, & comp_offset);
        if (rc)
          {
            sim_printf ("not found\n");
            return SCPE_OK;
          }
        sim_printf ("0%o:0%o\n", (uint) segno, (uint) (comp_offset + offset));
        absAddrN  ((int) segno, (uint) (comp_offset + offset));
      }
/*
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    char * ans = lookupAddress (segno, offset);
    sim_printf ("%s\n", ans ? ans : "not found");
*/
    return SCPE_OK;
  }

static t_stat addSystemBookEntry (UNUSED int32 arg, const char * buf)
  {
    // asbe segname compname seg txt_start txt_len intstat_start intstat_length symbol_start symbol_length
    char segname [bookSegmentNameLen];
    char compname [bookSegmentNameLen];
    uint segno;
    uint txt_start, txt_len;
    uint  intstat_start, intstat_length;
    uint  symbol_start, symbol_length;

    // 32 is bookSegmentNameLen - 1
    if (sscanf (buf, "%32s %32s %o %o %o %o %o %o %o", 
                segname, compname, & segno, 
                & txt_start, & txt_len, & intstat_start, & intstat_length, 
                & symbol_start, & symbol_length) != 9)
      return SCPE_ARG;

    int idx = addBookSegment (segname, (int) segno);
    if (idx < 0)
      return SCPE_ARG;

    if (addBookComponent (idx, compname, txt_start, txt_len, (int) intstat_start, (int) intstat_length, (int) symbol_start, (int) symbol_length) < 0)
      return SCPE_ARG;

    return SCPE_OK;
  }

static t_stat loadSystemBook (UNUSED int32 arg, UNUSED const char * buf)
  {
// Quietly ignore if not debug enabled
#ifndef SPEED
    // Multics 12.5 assigns segment number to collection 3 starting at 0244.
    uint c3 = 0244;

#define bufSz 257
    char filebuf [bufSz];
    int current = -1;

    FILE * fp = fopen (buf, "r");
    if (! fp)
      {
        sim_printf ("error opening file %s\n", buf);
        return SCPE_ARG;
      }
    for (;;)
      {
        char * bufp = fgets (filebuf, bufSz, fp);
        if (! bufp)
          break;
        //sim_printf ("<%s\n>", filebuf);
        char name [bookSegmentNameLen];
        int segno, p0, p1, p2;

        // 32 is bookSegmentNameLen - 1
        int cnt = sscanf (filebuf, "%32s %o  (%o, %o, %o)", name, & segno, 
          & p0, & p1, & p2);
        if (filebuf [0] != '\t' && cnt == 5)
          {
            //sim_printf ("A: %s %d\n", name, segno);
            int rc = addBookSegment (name, segno);
            if (rc < 0)
              {
                sim_printf ("error adding segment name\n");
                fclose (fp);
                return SCPE_ARG;
              }
            continue;
          }
        else
          {
            // Check for collection 3 segment
            // 32 is bookSegmentNameLen - 1
            cnt = sscanf (filebuf, "%32s  (%o, %o, %o)", name, 
              & p0, & p1, & p2);
            if (filebuf [0] != '\t' && cnt == 4)
              {
                if (strstr (name, "fw.") || strstr (name, ".ec"))
                  continue;
                //sim_printf ("A: %s %d\n", name, segno);
                int rc = addBookSegment (name, (int) (c3 ++));
                if (rc < 0)
                  {
                    sim_printf ("error adding segment name\n");
                    fclose (fp);
                    return SCPE_ARG;
                  }
                continue;
              }
          }
        cnt = sscanf (filebuf, "Bindmap for >ldd>h>e>%32s", name);
        if (cnt == 1)
          {
            //sim_printf ("B: %s\n", name);
            //int rc = addBookSegment (name);
            int rc = lookupBookSegment (name);
            if (rc < 0)
              {
                // The collection 3.0 segments do not have segment numbers,
                // and the 1st digit of the 3-tuple is 1, not 0. Ignore
                // them for now.
                current = -1;
                continue;
                //sim_printf ("error adding segment name\n");
                //return SCPE_ARG;
              }
            current = rc;
            continue;
          }

        uint txt_start, txt_length;
        int intstat_start, intstat_length, symbol_start, symbol_length;
        cnt = sscanf (filebuf, "%32s %o %o %o %o %o %o", name, & txt_start, & txt_length, & intstat_start, & intstat_length, & symbol_start, & symbol_length);

        if (cnt == 7)
          {
            //sim_printf ("C: %s\n", name);
            if (current >= 0)
              {
                addBookComponent (current, name, txt_start, txt_length, intstat_start, intstat_length, symbol_start, symbol_length);
              }
            continue;
          }

        cnt = sscanf (filebuf, "%32s %o  (%o, %o, %o)", name, & segno, 
          & p0, & p1, & p2);
        if (filebuf [0] == '\t' && cnt == 5)
          {
            //sim_printf ("D: %s %d\n", name, segno);
            int rc = addBookSegment (name, segno);
            if (rc < 0)
              {
                sim_printf ("error adding segment name\n");
                fclose (fp);
                return SCPE_ARG;
              }
            continue;
          }

      }
    fclose (fp);
#if 0
    for (int i = 0; i < nBookSegments; i ++)
      { 
        sim_printf ("  %-32s %6o\n", bookSegments [i] . segname, bookSegments [i] . segno);
        for (int j = 0; j < nBookComponents; j ++)
          {
            if (bookComponents [j] . bookSegmentNum == i)
              {
                printf ("    %-32s %6o %6o %6o %6o %6o %6o\n",
                  bookComponents [j] . compname, 
                  bookComponents [j] . txt_start, 
                  bookComponents [j] . txt_length, 
                  bookComponents [j] . intstat_start, 
                  bookComponents [j] . intstat_length, 
                  bookComponents [j] . symbol_start, 
                  bookComponents [j] . symbol_length);
              }
          }
      }
#endif
#endif
    return SCPE_OK;
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

static t_addr parse_addr (UNUSED DEVICE * dptr, const char *cptr, const char **optr)
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
        word18 PRoffset = 0;   // offset from PR[n] register (if any)
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
                    segno = cpu . PR[prt->n].SNR;
                    PRoffset = cpu . PR[prt->n].WORDNO;
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
        uint offset = (uint)strtoll(off, &endp, 8);
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
            offset = (uint) s->value;
        }
        
        // if we get here then seg contains a segment# and offset.
        // So, fetch the actual address given the segment & offset ...
        // ... and return this absolute, 24-bit address
        
        word24 absAddr = (word24) getAddress(segno, (int) (offset + PRoffset));
        
        // TODO: only luckily does this work FixMe
        *optr = endp;   //cptr + strlen(cptr);
        
        return absAddr;
    }
    else
    {
        // a PR or alias thereof
        int segno = 0;
        word24 offset = 0;
        struct PRtab *prt = _prtab;
        while (prt->alias)
        {
            if (strncasecmp(cptr, prt->alias, strlen(prt->alias)) == 0)
            {
                segno = cpu . PR[prt->n].SNR;
                offset = cpu . PR[prt->n].WORDNO;
                break;
            }
            
            prt += 1;
        }
        if (prt->alias)    // a PR or alias
        {
            word24 absAddr = (word24) getAddress(segno, (int) offset);
            *optr = cptr + strlen(prt->alias);
        
            return absAddr;
        }
    }
    
    // No, determine absolute address given by cptr
    return (t_addr)strtol(cptr, (char **) optr, 8);
}

static void fprint_addr (FILE * stream, UNUSED DEVICE *  dptr, t_addr simh_addr)
{
    char temp[256];
    bool bFound = getSegmentAddressString((int)simh_addr, temp);
    if (bFound)
        fprintf(stream, "%s (%08o)", temp, simh_addr);
    else
        fprintf(stream, "%06o", simh_addr);
}

// This is part of the simh interface
/*! Based on the switch variable, symbolically output to stream ofile the data in array val at the specified addr
 in unit uptr.

 * simh "fprint_sym" – Based on the switch variable, symbolically output to stream ofile the data in array val at the specified addr in unit uptr.
 */

t_stat fprint_sym (FILE * ofile, UNUSED t_addr  addr, t_value *val, 
                   UNIT *uptr, int32 sw)
{
// XXX Bug: assumes single cpu
// XXX CAC: This seems rather bogus; deciding the output format based on the
// address of the UNIT? Would it be better to use sim_unit.u3 (or some such 
// as a word width?

    if (!((uint) sw & SWMASK ('M')))
        return SCPE_ARG;
    
    if (uptr == &cpu_dev . units [0])
    {
        word36 word1 = *val;
        
        // get base syntax
        char *d = disAssemble(word1);
        
        fprintf(ofile, "%s", d);
        
        // decode instruction
        DCDstruct ci;
        DCDstruct * p = & ci;
        decodeInstruction (word1, p);
        
        // MW EIS?
        if (p->info->ndes > 1)
        {
            // Yup, just output word values (for now)
            
            // XXX Need to complete MW EIS support in disAssemble()
            
            for(uint n = 0 ; n < p->info->ndes; n += 1)
                fprintf(ofile, " %012"PRIo64"", val[n + 1]);
          
            return (t_stat) -p->info->ndes;
        }
        
        return SCPE_OK;

        //fprintf(ofile, "%012"PRIo64"", *val);
        //return SCPE_OK;
    }
    return SCPE_ARG;
}

// This is part of the simh interface
/*!  – Based on the switch variable, parse character string cptr for a symbolic value val at the specified addr
 in unit uptr.
 */
t_stat parse_sym (UNUSED const char * cptr, UNUSED t_addr addr, UNUSED UNIT * uptr, 
                  UNUSED t_value * val, UNUSED int32 sswitch)
{
    return SCPE_ARG;
}

// from MM

sysinfo_t sys_opts =
  {
    0, /* clock speed */
    {
#ifdef FNPDBG
      4000, /* iom_times.connect */
#else
      -1, /* iom_times.connect */
#endif
       0,  /* iom_times.chan_activate */
      10, /* boot_time */
      10000, /* terminate_time */
    },
    {
      -1, /* mt_times.read */
      -1  /* mt_times.xfer */
    },
    0 /* warn_uninit */
  };

static char * encode_timing (int timing)
  {
    static char buf [64];
    if (timing < 0)
      return (char *) "Off";
    sprintf (buf, "%d", timing);
    return buf;
  }

static t_stat sys_show_config (UNUSED FILE * st, UNUSED UNIT * uptr, 
                               UNUSED int  val, UNUSED const void * desc)
  {
    sim_printf ("IOM connect time:         %s\n",
                encode_timing (sys_opts . iom_times . connect));
    sim_printf ("IOM activate time:        %s\n",
                encode_timing (sys_opts . iom_times . chan_activate));
    sim_printf ("IOM boot time:            %s\n",
                encode_timing (sys_opts . iom_times . boot_time));
    sim_printf ("MT Read time:             %s\n",
                encode_timing (sys_opts . mt_times . read));
    sim_printf ("MT Xfer time:             %s\n",
                encode_timing (sys_opts . mt_times . xfer));

    return SCPE_OK;
}

static config_value_list_t cfg_timing_list [] =
  {
    { "disable", -1 },
    { NULL, 0 }
  };

static config_list_t sys_config_list [] =
  {
    /*  0 */ { "connect_time", -1, 100000, cfg_timing_list },
    /*  1 */ { "activate_time", -1, 100000, cfg_timing_list },
    /*  2 */ { "mt_read_time", -1, 100000, cfg_timing_list },
    /*  3 */ { "mt_xfer_time", -1, 100000, cfg_timing_list },
    /*  4 */ { "iom_boot_time", -1, 100000, cfg_timing_list },
    /*  5 */ { "terminate_time", -1, 100000, cfg_timing_list },
    { NULL, 0, 0, NULL }
 };

static t_stat sys_set_config (UNUSED UNIT *  uptr, UNUSED int32 value, 
                              const char * cptr, UNUSED void * desc)
  {
    config_state_t cfg_state = { NULL, NULL };

    for (;;)
      {
        int64_t v;
        int rc = cfgparse ("sys_set_config", cptr, sys_config_list, & cfg_state, & v);
        switch (rc)
          {
            case -2: // error
              cfgparse_done (& cfg_state);
              return SCPE_ARG;

            case -1: // done
              break;

            case  0: // CONNECT_TIME
              sys_opts . iom_times . connect = (int) v;
              break;

            case  1: // ACTIVATE_TIME
              sys_opts . iom_times . chan_activate = (int) v;
              break;

            case  2: // MT_READ_TIME
              sys_opts . mt_times . read = (int) v;
              break;

            case  3: // MT_XFER_TIME
              sys_opts . mt_times . xfer = (int) v;
              break;

            case  4: // IOM_BOOT_TIME
              sys_opts . iom_times . boot_time = (int) v;
              break;

            case  5: // TERMINATE_TIME
              sys_opts . iom_times . terminate_time = (int) v;
              break;

            default:
              sim_debug (DBG_ERR, & iom_dev, "sys_set_config: Invalid cfgparse rc <%d>\n", rc);
              sim_printf ("error: iom_set_config: invalid cfgparse rc <%d>\n", rc);
              cfgparse_done (& cfg_state);
              return SCPE_ARG;
          } // switch
        if (rc < 0)
          break;
      } // process statements
    cfgparse_done (& cfg_state);
    return SCPE_OK;
  }


#ifdef DVFDBG
static t_stat dfx1entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// divide_fx1, divide_fx3
    sim_printf ("dfx1entry\n");
    sim_printf ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_printf ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu . PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
sim_printf ("%05o:%06o\n", cpu . PR [2] . SNR, cpu . rX [0]);
//dbgStackTrace ();
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
    return SCPE_OK;
  }

static t_stat dfx1exit (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_printf ("dfx1exit\n");
    sim_printf ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_printf ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    return SCPE_OK;
  }

static t_stat dv2scale (UNUSED int32 arg, UNUSED const char * buf)
  {
    sim_printf ("dv2scale\n");
    sim_printf ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    return SCPE_OK;
  }

static t_stat dfx2entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// divide_fx2
    sim_printf ("dfx2entry\n");
    sim_printf ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_printf ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu . PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
#if 0
sim_printf ("%05o:%06o\n", cpu . PR [2] . SNR, cpu . rX [0]);
//dbgStackTrace ();
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale ptr %012"PRIo64" (%llu)\n", M [pa], M [pa]);
        if ((M [pa] & 077) == 043)
          {
            word15 segno = (M [pa] >> 18u) & MASK15;
            word18 offset = (M [pa + 1] >> 18u) & MASK18;
            word24 ipa;
            if (dbgLookupAddress (segno, offset, & ipa, & msg))
              {
                sim_printf ("divisor address lookup failed because %s\n", msg);
              }
            else
              {
                sim_printf ("scale %012"PRIo64" (%llu)\n", M [ipa], M [ipa]);
              }
          }
      }
#endif
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012"PRIo64" (%llu)\n", M [pa], M [pa]);
        sim_printf ("divisor %012"PRIo64" (%llu)\n", M [pa + 1], M [pa + 1]);
      }
    return SCPE_OK;
  }

static t_stat mdfx3entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// operator to form mod(fx2,fx1)
// entered with first arg in q, bp pointing at second

// divide_fx1, divide_fx2
    sim_printf ("mdfx3entry\n");
    //sim_printf ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_printf ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu . PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
//sim_printf ("%05o:%06o\n", cpu . PR [2] . SNR, cpu . rX [0]);
//dbgStackTrace ();
#if 0
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
#endif
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
    return SCPE_OK;
  }

static t_stat smfx1entry (UNUSED int32 arg, UNUSED const char * buf)
  {
// operator to form mod(fx2,fx1)
// entered with first arg in q, bp pointing at second

// divide_fx1, divide_fx2
    sim_printf ("smfx1entry\n");
    //sim_printf ("rA %012"PRIo64" (%llu)\n", rA, rA);
    sim_printf ("rQ %012"PRIo64" (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (cpu . PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
sim_printf ("%05o:%06o\n", cpu . PR [2] . SNR, cpu . rX [0]);
//dbgStackTrace ();
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
    if (dbgLookupAddress (cpu . PR [2] . SNR, cpu . PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012"PRIo64" (%llu)\n", M [pa], M [pa]);
      }
    return SCPE_OK;
  }
#endif

static MTAB sys_mod [] =
  {
    {
      MTAB_XTD | MTAB_VDV | MTAB_NMO /* | MTAB_VALR */, /* mask */
      0,            /* match */
      (char *) "CONFIG",     /* print string */
      (char *) "CONFIG",         /* match string */
      sys_set_config,         /* validation routine */
      sys_show_config, /* display routine */
      NULL,          /* value descriptor */
      NULL,            /* help */
    },
    { 0, 0, NULL, NULL, NULL, NULL, NULL, NULL }
  };



static t_stat sys_reset (UNUSED DEVICE  * dptr)
  {
    return SCPE_OK;
  }

static DEVICE sys_dev = {
    "SYS",       /* name */
    NULL,        /* units */
    NULL,        /* registers */
    sys_mod,     /* modifiers */
    0,           /* #units */
    8,           /* address radix */
    PASIZE,      /* address width */
    1,           /* address increment */
    8,           /* data radix */
    36,          /* data width */
    NULL,        /* examine routine */
    NULL,        /* deposit routine */
    & sys_reset, /* reset routine */
    NULL,        /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    0,           /* flags */
    0,           /* debug control flags */
    0,           /* debug flag names */
    NULL,        /* memory size change */
    NULL,        /* logical name */
    NULL,        /* help */
    NULL,        /* attach_help */
    NULL,        /* help_ctx */
    NULL,        /* description */
    NULL
};


// This is part of the simh interface
DEVICE * sim_devices [] =
  {
    & cpu_dev, // dev[0] is special to simh; it is the 'default device'
    & iom_dev,
    & tape_dev,
    & fnpDev,
    & disk_dev,
    & scu_dev,
#ifndef QUIET_UNUSED
    & clk_dev,
#endif
    // & mpc_dev,
    & opcon_dev,
    & sys_dev,
    // & ipc_dev,  // for fnp IPC
#ifndef FNP2
    & mux_dev,
#endif
    & urp_dev,
    & crdrdr_dev,
    & crdpun_dev,
    & prt_dev,
#ifndef __MINGW64__
    & absi_dev,
#endif
    NULL
  };

//#ifdef LAUNCH
#define MAX_CHILDREN 256
static int nChildren = 0;
static pid_t childrenList [MAX_CHILDREN];

static void cleanupChildren (void)
  {
    printf ("cleanupChildren\n");
    for (int i = 0; i < nChildren; i ++)
      {
#ifndef __MINGW64__
        printf ("  kill %d\n", childrenList [i]);
        kill (childrenList [i], SIGHUP);
#else
        TerminateProcess((HANDLE)childrenList [i], 1);
        CloseHandle((HANDLE)childrenList [i]);
#endif
      }
  }

static void addChild (pid_t pid)
  {
    if (nChildren >= MAX_CHILDREN)
      return;
    childrenList [nChildren ++] = pid;
    if (nChildren == 1)
     atexit (cleanupChildren);
  }

#ifdef LAUNCH
static t_stat launch (int32 UNUSED arg, const char * buf)
  {
#ifndef __MINGW64__
    wordexp_t p;
    int rc = wordexp (buf, & p, WRDE_SHOWERR | WRDE_UNDEF);
    if (rc)
      {
        sim_printf ("wordexp failed %d\n", rc);
        return SCPE_ARG;
      }
    //for (uint i = 0; i < p . we_wordc; i ++)
      //sim_printf ("    %s\n", p . we_wordv [i]);
    pid_t pid = fork ();
    if (pid == -1) // parent, fork failed
      {
        sim_printf ("fork failed\n");
        return SCPE_ARG;
      }
    if (pid == 0)  // child
      {
        execv (p . we_wordv [0], & p . we_wordv [1]);
        sim_printf ("exec failed\n");
        exit (1);
      }
    addChild (pid);
    wordfree (& p);
#else
     STARTUPINFO si;
     PROCESS_INFORMATION pi;
 
     memset( &si, 0, sizeof(si) );
     si.cb = sizeof(si);
     memset( &pi, 0, sizeof(pi) );
 
     if( !CreateProcess( NULL, (LPSTR)buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ) ) 
     {
         sim_printf ("fork failed\n");
         return SCPE_ARG;
     }
     addChild ((pid_t)pi.hProcess);
#endif
    return SCPE_OK;
  }
#endif

static void doIniLine (char * text)
  {
    //sim_printf ("<%s?\n", text);
    char gbuf [257];
    const char * cptr = get_glyph (text, gbuf, 0); /* get command glyph */
    CTAB *cmdp;
    if ((cmdp = find_cmd (gbuf)))            /* lookup command */
      {
        t_stat stat = cmdp->action (cmdp->arg, cptr); /* if found, exec */
        if (stat != SCPE_OK)
          sim_printf ("%s: %s\n", sim_error_text (SCPE_UNK), text);
      }
    else
      sim_printf ("%s: %s\n", sim_error_text (SCPE_UNK), text);
  }

static t_stat defaultBaseSystem (UNUSED int32 arg, UNUSED const char * buf)
  {

    // ;
    // ; Configure test system
    // ;
    // ; CPU, IOM * 2, MPC, TAPE * 16, DISK * 16, SCU * 4, OPCON, FNP, URP * 3,
    // ; PRT, RDR, PUN
    // ;
    // ;
    // ; From AN70-1 System Initialization PLM May 84, pg 8-4:
    // ;
    // ; All CPUs and IOMs must share the same layout of port assignments to
    // ; SCUs. Thus, if memory port B of CPU C goes to SCU D, the memory port
    // ; B of all other CPUs and IOMs must go to SCU D. All CPUs and IOMs must
    // ; describe this SCU the same; all must agree in memory sizes. Also, all
    // ; SCUs must agree on port assignments of CPUs and IOMs. This, if port 3 
    // ; of SCU C goes to CPU A, the port 3 of all other SCUs must also go to
    // ; CPU A.
    // ;
    // ; Pg. 8-6:
    // ;
    // ; The actual memory size of the memory attached to the SCU attached to
    // ; the processor port in questions is 32K * 2 ** (encoded memory size).
    // ; The port assignment couples with the memory size to determine the base 
    // ; address of the SCU connected to the specified CPU port (absoulte
    // ; address of the first location in the memory attached to that SCU). The 
    // ; base address of the SCU is the (actual memory size) * (port assignment).
    // ;
    // ; Pg. 8-6
    // ;
    // ; [bits 09-11 lower store size]
    // ;
    // ; A DPS-8 SCU may have up to four store units attached to it. If this is
    // ; the case, two stores units form a pair of units. The size of a pair of
    // ; units (or a single unit) is 32K * 2 ** (lower store size) above.
    // ;
    // ;
    // ;
    // ; Looking at bootload_io, it would appear that Multics is happier with
    // ; IOM0 being the bootload IOM, despite suggestions elsewhere that was
    // ; not a requirement.

    // ; Disconnect everything...
    doIniLine ("cable_ripout");

    doIniLine ("set cpu nunits=8");
    doIniLine ("set iom nunits=2");
    // ; 16 drives plus the controller
    doIniLine ("set tape nunits=17");
    // ; 16 drives; no controller
    doIniLine ("set disk nunits=16");
    doIniLine ("set scu nunits=4");
    doIniLine ("set opcon nunits=1");
    doIniLine ("set fnp nunits=8");
    doIniLine ("set urp nunits=3");
    doIniLine ("set crdrdr nunits=1");
    doIniLine ("set crdpun nunits=1");
    doIniLine ("set prt nunits=17");
#ifndef __MINGW64__
    doIniLine ("set absi nunits=1");

    // ;Create card reader queue directory
    doIniLine ("! if [ ! -e /tmp/rdra ]; then mkdir /tmp/rdra; fi");
#else
    doIniLine ("! mkdir %TEMP%\\rdra");
#endif

    doIniLine ("set cpu config=faultbase=Multics");

    doIniLine ("set cpu config=num=0");
    // ; As per GB61-01 Operators Guide, App. A
    // ; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    doIniLine ("set cpu config=data=024000717200");

    // ; enable ports 0 and 1 (scu connections)
    // ; portconfig: ABCD
    // ;   each is 3 bits addr assignment
    // ;           1 bit enabled 
    // ;           1 bit sysinit enabled
    // ;           1 bit interlace enabled (interlace?)
    // ;           3 bit memory size
    // ;              0 - 32K
    // ;              1 - 64K
    // ;              2 - 128K
    // ;              3 - 256K
    // ;              4 - 512K
    // ;              5 - 1M
    // ;              6 - 2M
    // ;              7 - 4M  

    doIniLine ("set cpu config=port=A");
    doIniLine ("set cpu   config=assignment=0");
    doIniLine ("set cpu   config=interlace=0");
    doIniLine ("set cpu   config=enable=1");
    doIniLine ("set cpu   config=init_enable=1");
    doIniLine ("set cpu   config=store_size=4M");
 
    doIniLine ("set cpu config=port=B");
    doIniLine ("set cpu   config=assignment=1");
    doIniLine ("set cpu   config=interlace=0");
    doIniLine ("set cpu   config=enable=1");
    doIniLine ("set cpu   config=init_enable=1");
    doIniLine ("set cpu   config=store_size=4M");

    doIniLine ("set cpu config=port=C");
    doIniLine ("set cpu   config=assignment=2");
    doIniLine ("set cpu   config=interlace=0");
    doIniLine ("set cpu   config=enable=1");
    doIniLine ("set cpu   config=init_enable=1");
    doIniLine ("set cpu   config=store_size=4M");

    doIniLine ("set cpu config=port=D");
    doIniLine ("set cpu   config=assignment=3");
    doIniLine ("set cpu   config=interlace=0");
    doIniLine ("set cpu   config=enable=1");
    doIniLine ("set cpu   config=init_enable=1");
    doIniLine ("set cpu   config=store_size=4M");

    // ; 0 = GCOS 1 = VMS
    doIniLine ("set cpu config=mode=Multics");
    // ; 0 = 8/70
    doIniLine ("set cpu config=speed=0");

    // ;echo
    // ;show cpu config
    // ;echo

    // ;;;---set cpu0 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu0 config=num=0
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu0 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu0 config=port=A
    // ;;;---set cpu0   config=assignment=0
    // ;;;---set cpu0   config=interlace=0
    // ;;;---set cpu0   config=enable=1
    // ;;;---set cpu0   config=init_enable=1
    // ;;;---set cpu0   config=store_size=4M
    // ;;;---
    // ;;;---set cpu0 config=port=B
    // ;;;---set cpu0   config=assignment=1
    // ;;;---set cpu0   config=interlace=0
    // ;;;---set cpu0   config=enable=1
    // ;;;---set cpu0   config=init_enable=1
    // ;;;---set cpu0   config=store_size=4M
    // ;;;---
    // ;;;---set cpu0 config=port=C
    // ;;;---set cpu0   config=assignment=2
    // ;;;---set cpu0   config=interlace=0
    // ;;;---set cpu0   config=enable=1
    // ;;;---set cpu0   config=init_enable=1
    // ;;;---set cpu0   config=store_size=4M
    // ;;;---
    // ;;;---set cpu0 config=port=D
    // ;;;---set cpu0   config=assignment=3
    // ;;;---set cpu0   config=interlace=0
    // ;;;---set cpu0   config=enable=1
    // ;;;---set cpu0   config=init_enable=1
    // ;;;---set cpu0   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu0 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu0 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu0 config
    // ;;;---;echo
    // ;;;---
    // ;;;---
    // ;;;---set cpu1 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu1 config=num=1
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu1 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu1 config=port=A
    // ;;;---set cpu1   config=assignment=0
    // ;;;---set cpu1   config=interlace=0
    // ;;;---set cpu1   config=enable=1
    // ;;;---set cpu1   config=init_enable=1
    // ;;;---set cpu1   config=store_size=4M
    // ;;;---
    // ;;;---set cpu1 config=port=B
    // ;;;---set cpu1   config=assignment=1
    // ;;;---set cpu1   config=interlace=0
    // ;;;---set cpu1   config=enable=1
    // ;;;---set cpu1   config=init_enable=1
    // ;;;---set cpu1   config=store_size=4M
    // ;;;---
    // ;;;---set cpu1 config=port=C
    // ;;;---set cpu1   config=assignment=2
    // ;;;---set cpu1   config=interlace=0
    // ;;;---set cpu1   config=enable=1
    // ;;;---set cpu1   config=init_enable=1
    // ;;;---set cpu1   config=store_size=4M
    // ;;;---
    // ;;;---set cpu1 config=port=D
    // ;;;---set cpu1   config=assignment=3
    // ;;;---set cpu1   config=interlace=0
    // ;;;---set cpu1   config=enable=1
    // ;;;---set cpu1   config=init_enable=1
    // ;;;---set cpu1   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu1 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu1 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu1 config
    // ;;;---;echo
    // ;;;---
    // ;;;---set cpu2 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu2 config=num=2
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu2 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu2 config=port=A
    // ;;;---set cpu2   config=assignment=0
    // ;;;---set cpu2   config=interlace=0
    // ;;;---set cpu2   config=enable=1
    // ;;;---set cpu2   config=init_enable=1
    // ;;;---set cpu2   config=store_size=4M
    // ;;;---
    // ;;;---set cpu2 config=port=B
    // ;;;---set cpu2   config=assignment=1
    // ;;;---set cpu2   config=interlace=0
    // ;;;---set cpu2   config=enable=1
    // ;;;---set cpu2   config=init_enable=1
    // ;;;---set cpu2   config=store_size=4M
    // ;;;---
    // ;;;---set cpu2 config=port=C
    // ;;;---set cpu2   config=assignment=2
    // ;;;---set cpu2   config=interlace=0
    // ;;;---set cpu2   config=enable=1
    // ;;;---set cpu2   config=init_enable=1
    // ;;;---set cpu2   config=store_size=4M
    // ;;;---
    // ;;;---set cpu2 config=port=D
    // ;;;---set cpu2   config=assignment=3
    // ;;;---set cpu2   config=interlace=0
    // ;;;---set cpu2   config=enable=1
    // ;;;---set cpu2   config=init_enable=1
    // ;;;---set cpu2   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu2 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu2 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu2 config
    // ;;;---;echo
    // ;;;---
    // ;;;---set cpu3 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu3 config=num=3
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu3 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu3 config=port=A
    // ;;;---set cpu3   config=assignment=0
    // ;;;---set cpu3   config=interlace=0
    // ;;;---set cpu3   config=enable=1
    // ;;;---set cpu3   config=init_enable=1
    // ;;;---set cpu3   config=store_size=4M
    // ;;;---
    // ;;;---set cpu3 config=port=B
    // ;;;---set cpu3   config=assignment=1
    // ;;;---set cpu3   config=interlace=0
    // ;;;---set cpu3   config=enable=1
    // ;;;---set cpu3   config=init_enable=1
    // ;;;---set cpu3   config=store_size=4M
    // ;;;---
    // ;;;---set cpu3 config=port=C
    // ;;;---set cpu3   config=assignment=2
    // ;;;---set cpu3   config=interlace=0
    // ;;;---set cpu3   config=enable=1
    // ;;;---set cpu3   config=init_enable=1
    // ;;;---set cpu3   config=store_size=4M
    // ;;;---
    // ;;;---set cpu3 config=port=D
    // ;;;---set cpu3   config=assignment=3
    // ;;;---set cpu3   config=interlace=0
    // ;;;---set cpu3   config=enable=1
    // ;;;---set cpu3   config=init_enable=1
    // ;;;---set cpu3   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu3 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu3 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu3 config
    // ;;;---;echo
    // ;;;---
    // ;;;---set cpu4 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu4 config=num=4
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu4 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu4 config=port=A
    // ;;;---set cpu4   config=assignment=0
    // ;;;---set cpu4   config=interlace=0
    // ;;;---set cpu4   config=enable=1
    // ;;;---set cpu4   config=init_enable=1
    // ;;;---set cpu4   config=store_size=4M
    // ;;;---
    // ;;;---set cpu4 config=port=B
    // ;;;---set cpu4   config=assignment=1
    // ;;;---set cpu4   config=interlace=0
    // ;;;---set cpu4   config=enable=1
    // ;;;---set cpu4   config=init_enable=1
    // ;;;---set cpu4   config=store_size=4M
    // ;;;---
    // ;;;---set cpu4 config=port=C
    // ;;;---set cpu4   config=assignment=2
    // ;;;---set cpu4   config=interlace=0
    // ;;;---set cpu4   config=enable=1
    // ;;;---set cpu4   config=init_enable=1
    // ;;;---set cpu4   config=store_size=4M
    // ;;;---
    // ;;;---set cpu4 config=port=D
    // ;;;---set cpu4   config=assignment=3
    // ;;;---set cpu4   config=interlace=0
    // ;;;---set cpu4   config=enable=1
    // ;;;---set cpu4   config=init_enable=1
    // ;;;---set cpu4   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu4 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu4 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu4 config
    // ;;;---;echo
    // ;;;---
    // ;;;---set cpu5 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu5 config=num=5
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu5 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu5 config=port=A
    // ;;;---set cpu5   config=assignment=0
    // ;;;---set cpu5   config=interlace=0
    // ;;;---set cpu5   config=enable=1
    // ;;;---set cpu5   config=init_enable=1
    // ;;;---set cpu5   config=store_size=4M
    // ;;;---
    // ;;;---set cpu5 config=port=B
    // ;;;---set cpu5   config=assignment=1
    // ;;;---set cpu5   config=interlace=0
    // ;;;---set cpu5   config=enable=1
    // ;;;---set cpu5   config=init_enable=1
    // ;;;---set cpu5   config=store_size=4M
    // ;;;---
    // ;;;---set cpu5 config=port=C
    // ;;;---set cpu5   config=assignment=2
    // ;;;---set cpu5   config=interlace=0
    // ;;;---set cpu5   config=enable=1
    // ;;;---set cpu5   config=init_enable=1
    // ;;;---set cpu5   config=store_size=4M
    // ;;;---
    // ;;;---set cpu5 config=port=D
    // ;;;---set cpu5   config=assignment=3
    // ;;;---set cpu5   config=interlace=0
    // ;;;---set cpu5   config=enable=1
    // ;;;---set cpu5   config=init_enable=1
    // ;;;---set cpu5   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu5 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu5 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu5 config
    // ;;;---;echo
    // ;;;---
    // ;;;---set cpu6 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu6 config=num=6
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu6 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu6 config=port=A
    // ;;;---set cpu6   config=assignment=0
    // ;;;---set cpu6   config=interlace=0
    // ;;;---set cpu6   config=enable=1
    // ;;;---set cpu6   config=init_enable=1
    // ;;;---set cpu6   config=store_size=4M
    // ;;;---
    // ;;;---set cpu6 config=port=B
    // ;;;---set cpu6   config=assignment=1
    // ;;;---set cpu6   config=interlace=0
    // ;;;---set cpu6   config=enable=1
    // ;;;---set cpu6   config=init_enable=1
    // ;;;---set cpu6   config=store_size=4M
    // ;;;---
    // ;;;---set cpu6 config=port=C
    // ;;;---set cpu6   config=assignment=2
    // ;;;---set cpu6   config=interlace=0
    // ;;;---set cpu6   config=enable=1
    // ;;;---set cpu6   config=init_enable=1
    // ;;;---set cpu6   config=store_size=4M
    // ;;;---
    // ;;;---set cpu6 config=port=D
    // ;;;---set cpu6   config=assignment=3
    // ;;;---set cpu6   config=interlace=0
    // ;;;---set cpu6   config=enable=1
    // ;;;---set cpu6   config=init_enable=1
    // ;;;---set cpu6   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu6 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu6 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu6 config
    // ;;;---;echo
    // ;;;---
    // ;;;---set cpu7 config=faultbase=Multics
    // ;;;---
    // ;;;---set cpu7 config=num=4M
    // ;;;---; As per GB61-01 Operators Guide, App. A
    // ;;;---; switches: 4, 6, 18, 19, 20, 23, 24, 25, 26, 28
    // ;;;---set cpu7 config=data=024000717200
    // ;;;---
    // ;;;---; enable ports 0 and 1 (scu connections)
    // ;;;---; portconfig: ABCD
    // ;;;---;   each is 3 bits addr assignment
    // ;;;---;           1 bit enabled 
    // ;;;---;           1 bit sysinit enabled
    // ;;;---;           1 bit interlace enabled (interlace?)
    // ;;;---;           3 bit memory size
    // ;;;---;              0 - 32K
    // ;;;---;              1 - 64K
    // ;;;---;              2 - 128K
    // ;;;---;              3 - 256K
    // ;;;---;              4 - 512K
    // ;;;---;              5 - 1M
    // ;;;---;              6 - 2M
    // ;;;---;              7 - 4M  
    // ;;;---
    // ;;;---set cpu7 config=port=A
    // ;;;---set cpu7   config=assignment=0
    // ;;;---set cpu7   config=interlace=0
    // ;;;---set cpu7   config=enable=1
    // ;;;---set cpu7   config=init_enable=1
    // ;;;---set cpu7   config=store_size=4M
    // ;;;---
    // ;;;---set cpu7 config=port=B
    // ;;;---set cpu7   config=assignment=1
    // ;;;---set cpu7   config=interlace=0
    // ;;;---set cpu7   config=enable=1
    // ;;;---set cpu7   config=init_enable=1
    // ;;;---set cpu7   config=store_size=4M
    // ;;;---
    // ;;;---set cpu7 config=port=C
    // ;;;---set cpu7   config=assignment=2
    // ;;;---set cpu7   config=interlace=0
    // ;;;---set cpu7   config=enable=1
    // ;;;---set cpu7   config=init_enable=1
    // ;;;---set cpu7   config=store_size=4M
    // ;;;---
    // ;;;---set cpu7 config=port=D
    // ;;;---set cpu7   config=assignment=3
    // ;;;---set cpu7   config=interlace=0
    // ;;;---set cpu7   config=enable=1
    // ;;;---set cpu7   config=init_enable=1
    // ;;;---set cpu7   config=store_size=4M
    // ;;;---
    // ;;;---; 0 = GCOS 1 = VMS
    // ;;;---set cpu7 config=mode=Multics
    // ;;;---; 0 = 8/70
    // ;;;---set cpu7 config=speed=0
    // ;;;---
    // ;;;---;echo
    // ;;;---;show cpu7 config
    // ;;;---;echo


    doIniLine ("set iom0 config=iom_base=Multics");
    doIniLine ("set iom0 config=multiplex_base=0120");
    doIniLine ("set iom0 config=os=Multics");
    doIniLine ("set iom0 config=boot=tape");
    doIniLine ("set iom0 config=tapechan=012");
    doIniLine ("set iom0 config=cardchan=011");
    doIniLine ("set iom0 config=scuport=0");

    doIniLine ("set iom0 config=port=0");
    doIniLine ("set iom0   config=addr=0");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=1");
    doIniLine ("set iom0   config=addr=1");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=2");
    doIniLine ("set iom0   config=addr=2");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=3");
    doIniLine ("set iom0   config=addr=3");
    doIniLine ("set iom0   config=interlace=0");
    doIniLine ("set iom0   config=enable=1");
    doIniLine ("set iom0   config=initenable=0");
    doIniLine ("set iom0   config=halfsize=0");
    doIniLine ("set iom0   config=store_size=4M");

    doIniLine ("set iom0 config=port=4");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom0 config=port=5");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom0 config=port=6");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom0 config=port=7");
    doIniLine ("set iom0   config=enable=0");

    doIniLine ("set iom1 config=iom_base=Multics2");
    doIniLine ("set iom1 config=multiplex_base=0121");
    doIniLine ("set iom1 config=os=Multics");
    doIniLine ("set iom1 config=boot=tape");
    doIniLine ("set iom1 config=tapechan=012");
    doIniLine ("set iom1 config=cardchan=011");
    doIniLine ("set iom1 config=scuport=0");

    doIniLine ("set iom1 config=port=0");
    doIniLine ("set iom1   config=addr=0");
    doIniLine ("set iom1   config=interlace=0");
    doIniLine ("set iom1   config=enable=1");
    doIniLine ("set iom1   config=initenable=0");
    doIniLine ("set iom1   config=halfsize=0;");

    doIniLine ("set iom1 config=port=1");
    doIniLine ("set iom1   config=addr=1");
    doIniLine ("set iom1   config=interlace=0");
    doIniLine ("set iom1   config=enable=1");
    doIniLine ("set iom1   config=initenable=0");
    doIniLine ("set iom1   config=halfsize=0;");

    doIniLine ("set iom1 config=port=2");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=3");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=4");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=5");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=6");
    doIniLine ("set iom1   config=enable=0");
    doIniLine ("set iom1 config=port=7");
    doIniLine ("set iom1   config=enable=0");

    // ;echo
    // ;show iom0 config
    // ;echo
    // ;show iom1 config
    // ;echo

    doIniLine ("set scu0 config=mode=program");
    doIniLine ("set scu0 config=port0=enable");
    doIniLine ("set scu0 config=port1=enable");
    doIniLine ("set scu0 config=port2=enable");
    doIniLine ("set scu0 config=port3=enable");
    doIniLine ("set scu0 config=port4=enable");
    doIniLine ("set scu0 config=port5=enable");
    doIniLine ("set scu0 config=port6=enable");
    doIniLine ("set scu0 config=port7=enable");
    doIniLine ("set scu0 config=maska=7");
    doIniLine ("set scu0 config=maskb=off");
    doIniLine ("set scu0 config=lwrstoresize=7");
    doIniLine ("set scu0 config=cyclic=0040");
    doIniLine ("set scu0 config=nea=0200");
    doIniLine ("set scu0 config=onl=014");
    doIniLine ("set scu0 config=int=0");
    doIniLine ("set scu0 config=lwr=0");

    doIniLine ("set scu1 config=mode=program");
    doIniLine ("set scu1 config=port0=enable");
    doIniLine ("set scu1 config=port1=enable");
    doIniLine ("set scu1 config=port2=enable");
    doIniLine ("set scu1 config=port3=enable");
    doIniLine ("set scu1 config=port4=enable");
    doIniLine ("set scu1 config=port5=enable");
    doIniLine ("set scu1 config=port6=enable");
    doIniLine ("set scu1 config=port7=enable");
    doIniLine ("set scu1 config=maska=off");
    doIniLine ("set scu1 config=maskb=off");
    doIniLine ("set scu1 config=lwrstoresize=7");
    doIniLine ("set scu1 config=cyclic=0040");
    doIniLine ("set scu1 config=nea=0200");
    doIniLine ("set scu1 config=onl=014");
    doIniLine ("set scu1 config=int=0");
    doIniLine ("set scu1 config=lwr=0");

    doIniLine ("set scu2 config=mode=program");
    doIniLine ("set scu2 config=port0=enable");
    doIniLine ("set scu2 config=port1=enable");
    doIniLine ("set scu2 config=port2=enable");
    doIniLine ("set scu2 config=port3=enable");
    doIniLine ("set scu2 config=port4=enable");
    doIniLine ("set scu2 config=port5=enable");
    doIniLine ("set scu2 config=port6=enable");
    doIniLine ("set scu2 config=port7=enable");
    doIniLine ("set scu2 config=maska=off");
    doIniLine ("set scu2 config=maskb=off");
    doIniLine ("set scu2 config=lwrstoresize=7");
    doIniLine ("set scu2 config=cyclic=0040");
    doIniLine ("set scu2 config=nea=0200");
    doIniLine ("set scu2 config=onl=014");
    doIniLine ("set scu2 config=int=0");
    doIniLine ("set scu2 config=lwr=0");

    doIniLine ("set scu3 config=mode=program");
    doIniLine ("set scu3 config=port0=enable");
    doIniLine ("set scu3 config=port1=enable");
    doIniLine ("set scu3 config=port2=enable");
    doIniLine ("set scu3 config=port3=enable");
    doIniLine ("set scu3 config=port4=enable");
    doIniLine ("set scu3 config=port5=enable");
    doIniLine ("set scu3 config=port6=enable");
    doIniLine ("set scu3 config=port7=enable");
    doIniLine ("set scu3 config=maska=off");
    doIniLine ("set scu3 config=maskb=off");
    doIniLine ("set scu3 config=lwrstoresize=7");
    doIniLine ("set scu3 config=cyclic=0040");
    doIniLine ("set scu3 config=nea=0200");
    doIniLine ("set scu3 config=onl=014");
    doIniLine ("set scu3 config=int=0");
    doIniLine ("set scu3 config=lwr=0");

    // ; There are bugs in the FNP code that require sim unit number
    // ; to be the same as the Multics unit number; ie fnp0 == fnpa, etc.
    // ;
    // ; fnp a 3400
    // ; fnp b 3700
    // ; fnp c 4200
    // ; fnp d 4500
    // ; fnp e 5000
    // ; fnp f 5300
    // ; fnp g 5600
    // ; fnp h 6100

    doIniLine ("set fnp0 config=mailbox=03400");
    doIniLine ("set fnp0 ipc_name=fnp-a");
    doIniLine ("set fnp1 config=mailbox=03700");
    doIniLine ("set fnp1 ipc_name=fnp-b");
    doIniLine ("set fnp2 config=mailbox=04200");
    doIniLine ("set fnp2 ipc_name=fnp-c");
    doIniLine ("set fnp3 config=mailbox=04500");
    doIniLine ("set fnp3 ipc_name=fnp-d");
    doIniLine ("set fnp4 config=mailbox=05000");
    doIniLine ("set fnp4 ipc_name=fnp-e");
    doIniLine ("set fnp5 config=mailbox=05300");
    doIniLine ("set fnp5 ipc_name=fnp-f");
    doIniLine ("set fnp6 config=mailbox=05600");
    doIniLine ("set fnp6 ipc_name=fnp-g");
    doIniLine ("set fnp7 config=mailbox=06100");
    doIniLine ("set fnp7 ipc_name=fnp-h");


    // ;echo
    // ;show scu0 config
    // ;echo
    // ;show scu1 config
    // ;echo
    // ;echo
    // ;show fnp0 config

    doIniLine ("set tape0 boot_drive");

    // ;cable ripout

    // ; Attach tape MPC to IOM 0, chan 012, dev_code 0
    doIniLine ("cable tape,0,0,012,0");
    doIniLine ("set tape0 device_name=mpca");
    // ; Attach TAPE unit 0 to IOM 0, chan 012, dev_code 1
    doIniLine ("cable tape,1,0,012,1");
    doIniLine ("set tape1 device_name=tapa_01");
    doIniLine ("cable tape,2,0,012,2");
    doIniLine ("set tape2 device_name=tapa_02");
    doIniLine ("cable tape,3,0,012,3");
    doIniLine ("set tape3 device_name=tapa_03");
    doIniLine ("cable tape,4,0,012,4");
    doIniLine ("set tape4 device_name=tapa_04");
    doIniLine ("cable tape,5,0,012,5");
    doIniLine ("set tape5 device_name=tapa_05");
    doIniLine ("cable tape,6,0,012,6");
    doIniLine ("set tape6 device_name=tapa_06");
    doIniLine ("cable tape,7,0,012,7");
    doIniLine ("set tape7 device_name=tapa_07");
    doIniLine ("cable tape,8,0,012,8");
    doIniLine ("set tape8 device_name=tapa_08");
    doIniLine ("cable tape,9,0,012,9");
    doIniLine ("set tape9 device_name=tapa_09");
    doIniLine ("cable tape,10,0,012,10");
    doIniLine ("set tape10 device_name=tapa_10");
    doIniLine ("cable tape,11,0,012,11");
    doIniLine ("set tape11 device_name=tapa_11");
    doIniLine ("cable tape,12,0,012,12");
    doIniLine ("set tape12 device_name=tapa_12");
    doIniLine ("cable tape,13,0,012,13");
    doIniLine ("set tape13 device_name=tapa_13");
    doIniLine ("cable tape,14,0,012,14");
    doIniLine ("set tape14 device_name=tapa_14");
    doIniLine ("cable tape,15,0,012,15");
    doIniLine ("set tape15 device_name=tapa_15");
    doIniLine ("cable tape,16,0,012,16");
    doIniLine ("set tape16 device_name=tapa_16");

    // ; Attach DISK unit 0 to IOM 0, chan 013, dev_code 0");
    doIniLine ("cable disk,0,0,013,0");
    // ; Attach DISK unit 1 to IOM 0, chan 013, dev_code 1");
    doIniLine ("cable disk,1,0,013,1");
    // ; Attach DISK unit 2 to IOM 0, chan 013, dev_code 2");
    doIniLine ("cable disk,2,0,013,2");
    // ; Attach DISK unit 3 to IOM 0, chan 013, dev_code 3");
    doIniLine ("cable disk,3,0,013,3");
    // ; Attach DISK unit 4 to IOM 0, chan 013, dev_code 4");
    doIniLine ("cable disk,4,0,013,4");
    // ; Attach DISK unit 5 to IOM 0, chan 013, dev_code 5");
    doIniLine ("cable disk,5,0,013,5");
    // ; Attach DISK unit 6 to IOM 0, chan 013, dev_code 6");
    doIniLine ("cable disk,6,0,013,6");
    // ; Attach DISK unit 7 to IOM 0, chan 013, dev_code 7");
    doIniLine ("cable disk,7,0,013,7");
    // ; Attach DISK unit 8 to IOM 0, chan 013, dev_code 8");
    doIniLine ("cable disk,8,0,013,8");
    // ; Attach DISK unit 9 to IOM 0, chan 013, dev_code 9");
    doIniLine ("cable disk,9,0,013,9");
    // ; Attach DISK unit 10 to IOM 0, chan 013, dev_code 10");
    doIniLine ("cable disk,10,0,013,10");
    // ; Attach DISK unit 11 to IOM 0, chan 013, dev_code 11");
    doIniLine ("cable disk,11,0,013,11");
    // ; Attach DISK unit 12 to IOM 0, chan 013, dev_code 12");
    doIniLine ("cable disk,12,0,013,12");
    // ; Attach DISK unit 13 to IOM 0, chan 013, dev_code 13");
    doIniLine ("cable disk,13,0,013,13");
    // ; Attach DISK unit 14 to IOM 0, chan 013, dev_code 14");
    doIniLine ("cable disk,14,0,013,14");
    // ; Attach DISK unit 15 to IOM 0, chan 013, dev_code 15");
    doIniLine ("cable disk,15,0,013,15");

    // ; Attach OPCON unit 0 to IOM A, chan 036, dev_code 0
    doIniLine ("cable opcon,0,0,036,0");

    // ;;;
    // ;;; FNP
    // ;;;

    // ; Attach FNP unit 3 (d) to IOM A, chan 020, dev_code 0
    doIniLine ("cable fnp,3,0,020,0");

    // ; Attach FNP unit 0 (a) to IOM A, chan 021, dev_code 0
    doIniLine ("cable fnp,0,0,021,0");

    // ; Attach FNP unit 1 (b) to IOM A, chan 022, dev_code 0
    doIniLine ("cable fnp,1,0,022,0");

    // ; Attach FNP unit 2 (c) to IOM A, chan 023, dev_code 0
    doIniLine ("cable fnp,2,0,023,0");

    // ; Attach FNP unit 4 (e) to IOM A, chan 024, dev_code 0
    doIniLine ("cable fnp,4,0,024,0");

    // ; Attach FNP unit 5 (f) to IOM A, chan 025, dev_code 0
    doIniLine ("cable fnp,5,0,025,0");

    // ; Attach FNP unit 6 (g) to IOM A, chan 026, dev_code 0
    doIniLine ("cable fnp,6,0,026,0");

    // ; Attach FNP unit 7 (h) to IOM A, chan 027, dev_code 0
    doIniLine ("cable fnp,7,0,027,0");

    // ;;;
    // ;;; MPC
    // ;;;

    // ; Attach MPC unit 0 to IOM 0, char 015, dev_code 0
    doIniLine ("cable urp,0,0,015, 0");
    doIniLine ("set urp0 device_name=urpa");

    // ; Attach CRDRDR unit 0 to IOM 0, chan 015, dev_code 1
    doIniLine ("cable crdrdr,0,0,015,1");
    doIniLine ("set crdrdr0 device_name=rdra");

    // ; Attach MPC unit 1 to IOM 0, char 016, dev_code 0
    doIniLine ("cable urp,1,0,016, 0");
    doIniLine ("set urp1 device_name=urpb");

    // ; Attach CRDPUN unit 0 to IOM 0, chan 016, dev_code 1
    doIniLine ("cable crdpun,0,0,016,1");
    doIniLine ("set crdpun0 device_name=puna");

    // ; Attach MPC unit 2 to IOM 0, char 017, dev_code 0
    doIniLine ("cable urp,2,0,017,0");
    doIniLine ("set urp2 device_name=urpc");

    // ; Attach PRT unit 0 to IOM 0, chan 017, dev_code 1
    doIniLine ("cable prt,0,0,017,1");
    doIniLine ("set prt0 device_name=prta");

    // ; Attach PRT unit 1 to IOM 0, chan 017, dev_code 2
    doIniLine ("cable prt,1,0,017,2");
    doIniLine ("set prt1 device_name=prtb");

    // ; Attach PRT unit 2 to IOM 0, chan 017, dev_code 3
    doIniLine ("cable prt,2,0,017,3");
    doIniLine ("set prt2 device_name=prtc");

    // ; Attach PRT unit 3 to IOM 0, chan 017, dev_code 4
    doIniLine ("cable prt,3,0,017,4");
    doIniLine ("set prt3 device_name=prtd");

    // ; Attach PRT unit 4 to IOM 0, chan 017, dev_code 5
    doIniLine ("cable prt,4,0,017,5");
    doIniLine ("set prt4 device_name=prte");

    // ; Attach PRT unit 5 to IOM 0, chan 017, dev_code 6
    doIniLine ("cable prt,5,0,017,6");
    doIniLine ("set prt5 device_name=prtf");

    // ; Attach PRT unit 6 to IOM 0, chan 017, dev_code 7
    doIniLine ("cable prt,6,0,017,7");
    doIniLine ("set prt6 device_name=prtg");

    // ; Attach PRT unit 7 to IOM 0, chan 017, dev_code 8
    doIniLine ("cable prt,7,0,017,8");
    doIniLine ("set prt7 device_name=prth");

    // ; Attach PRT unit 8 to IOM 0, chan 017, dev_code 9
    doIniLine ("cable prt,8,0,017,9");
    doIniLine ("set prt8 device_name=prti");

    // ; Attach PRT unit 9 to IOM 0, chan 017, dev_code 10
    doIniLine ("cable prt,9,0,017,10");
    doIniLine ("set prt9 device_name=prtj");

    // ; Attach PRT unit 10 to IOM 0, chan 017, dev_code 11
    doIniLine ("cable prt,10,0,017,11");
    doIniLine ("set prt10 device_name=prtk");

    // ; Attach PRT unit 11 to IOM 0, chan 017, dev_code 12
    doIniLine ("cable prt,11,0,017,12");
    doIniLine ("set prt11 device_name=prtl");

    // ; Attach PRT unit 12 to IOM 0, chan 017, dev_code 13
    doIniLine ("cable prt,12,0,017,13");
    doIniLine ("set prt12 device_name=prtm");

    // ; Attach PRT unit 13 to IOM 0, chan 017, dev_code 14
    doIniLine ("cable prt,13,0,017,14");
    doIniLine ("set prt13 device_name=prtn");

    // ; Attach PRT unit 14 to IOM 0, chan 017, dev_code 15
    doIniLine ("cable prt,14,0,017,15");
    doIniLine ("set prt14 device_name=prto");

    // ; Attach PRT unit 15 to IOM 0, chan 017, dev_code 16
    doIniLine ("cable prt,15,0,017,16");
    doIniLine ("set prt15 device_name=prtp");

    // ; Attach PRT unit 16 to IOM 0, chan 017, dev_code 17
    doIniLine ("cable prt,16,0,017,17");
    doIniLine ("set prt16 device_name=prtq");


    // ; Attach ABSI unit 0 to IOM 0, chan 032, dev_code 0
    doIniLine ("cable absi,0,0,032,0");


    // ; Attach IOM unit 0 port A (0) to SCU unit 0, port 0
    doIniLine ("cable iom,0,0,0,0");

    // ; Attach IOM unit 0 port B (1) to SCU unit 1, port 0
    doIniLine ("cable iom,0,1,1,0");

    // ; Attach IOM unit 0 port C (2) to SCU unit 2, port 0
    doIniLine ("cable iom,0,2,2,0");

    // ; Attach IOM unit 0 port D (3) to SCU unit 3, port 0
    doIniLine ("cable iom,0,3,3,0");

    // ; Attach IOM unit 1 port A (0) to SCU unit 0, port 1
    doIniLine ("cable iom,1,0,0,1");

    // ; Attach IOM unit 1 port B (1) to SCU unit 1, port 1
    doIniLine ("cable iom,1,1,1,1");

    // ; Attach IOM unit 1 port C (2) to SCU unit 2, port 1
    doIniLine ("cable iom,1,2,2,1");

    // ; Attach IOM unit 1 port D (3) to SCU unit 3, port 1
    doIniLine ("cable iom,1,3,3,1");


    // ;;;
    // ;;; SCU 0 --> CPUs
    // ;;;

    // ; Attach SCU unit 0 port 7 to CPU unit A (0), port 0
    doIniLine ("cable scu,0,7,0,0");

    // ; Attach SCU unit 0 port 6 to CPU unit B (1), port 0
    doIniLine ("cable scu,0,6,1,0");

    // ; Attach SCU unit 0 port 5 to CPU unit C (2), port 0
    doIniLine ("cable scu,0,5,2,0");

    // ; Attach SCU unit 0 port 4 to CPU unit D (3), port 0
    doIniLine ("cable scu,0,4,3,0");

    // ;;;
    // ;;; SCU 1 --> CPUs
    // ;;;

    // ; Attach SCU unit 1 port 7 to CPU unit A (0), port 1
    doIniLine ("cable scu,1,7,0,1");

    // ; Attach SCU unit 1 port 6 to CPU unit B (1), port 1
    doIniLine ("cable scu,1,6,1,1");

    // ; Attach SCU unit 1 port 5 to CPU unit C (2), port 1
    doIniLine ("cable scu,1,5,2,1");

    // ; Attach SCU unit 1 port 4 to CPU unit D (3), port 1
    doIniLine ("cable scu,1,4,3,1");


    // ;;;
    // ;;; SCU 2 --> CPUs
    // ;;;

    // ; Attach SCU unit 2 port 7 to CPU unit A (0), port 2
    doIniLine ("cable scu,2,7,0,2");

    // ; Attach SCU unit 2 port 6 to CPU unit B (1), port 2
    doIniLine ("cable scu,2,6,1,2");

    // ; Attach SCU unit 2 port 5 to CPU unit C (2), port 2
    doIniLine ("cable scu,2,5,2,2");

    // ; Attach SCU unit 2 port 4 to CPU unit D (3), port 2
    doIniLine ("cable scu,2,4,3,2");

    // ;;;
    // ;;; SCU 3 --> CPUs
    // ;;;

    // ; Attach SCU unit 3 port 7 to CPU unit A (0), port 3
    doIniLine ("cable scu,3,7,0,3");

    // ; Attach SCU unit 3 port 6 to CPU unit B (1), port 3
    doIniLine ("cable scu,3,6,1,3");

    // ; Attach SCU unit 3 port 5 to CPU unit C (2), port 3
    doIniLine ("cable scu,3,5,2,3");

    // ; Attach SCU unit 3 port 4 to CPU unit D (3), port 3
    doIniLine ("cable scu,3,4,3,3");

    // ;cable show
    // ;cable verify

    doIniLine ("set cpu config=b29test=enable");
    doIniLine ("set cpu config=dis_enable=enable");
    doIniLine ("set cpu config=lprp_highonly=enable");
    doIniLine ("set cpu config=steady_clock=disable");
    doIniLine ("set cpu config=append_after=enable");
    //doIniLine ("set cpu config=super_user=disable");
    doIniLine ("set cpu config=epp_hack=enable");
    doIniLine ("set cpu config=halt_on_unimplemented=disable");
    doIniLine ("set cpu config=disable_wam=enable");
    doIniLine ("set cpu config=tro_enable=enable");
    doIniLine ("set cpu config=bullet_time=disable");
    doIniLine ("set cpu config=y2k=disable");
    // ; 6 MIP Processor
    //doIniLine ("set cpu config=trlsb=12");

#ifndef THREADZ
    doIniLine ("set sys config=activate_time=8");
    doIniLine ("set sys config=terminate_time=8");
#endif

    doIniLine ("fnpload Devices.txt");
    doIniLine ("fnpserverport 6180");

    return SCPE_OK;
  }

// SCP message queue; when IPC messages come in, they are append to this
// queue. The sim_instr loop will poll the queue for messages for delivery 
// to the simh code.

static pthread_mutex_t scpMQlock;
typedef struct scpQueueElement scpQueueElement;
struct scpQueueElement
  {
    char * msg;
    scpQueueElement * prev, * next;
  };

static scpQueueElement * scpQueue = NULL;

static void scpQueueMsg (char * msg)
  {
    pthread_mutex_lock (& scpMQlock);
    scpQueueElement * element = malloc (sizeof (scpQueueElement));
    if (! element)
      {
         sim_debug (DBG_ERR, & sys_dev, "couldn't malloc scpQueueElement\n");
      }
    else
      {
        element -> msg = strdup (msg);
        DL_APPEND (scpQueue, element);
      }
    pthread_mutex_unlock (& scpMQlock);
  }

static bool scpPollQueue (void)
  {
    return !! scpQueue;
  }


static char * scpDequeueMsg (void)
  {
    if (! scpQueue)
      return NULL;
    pthread_mutex_lock (& scpMQlock);
    scpQueueElement * rv = scpQueue;
    DL_DELETE (scpQueue, rv);
    pthread_mutex_unlock (& scpMQlock);
    char * msg = rv -> msg;
    free (rv);
    return msg;
  }

//
//   "attach <device> <filename>"
//   "attachr <device> <filename>"
//   "detach <device>


void scpProcessEvent (void)
  {
    // Queue empty?
    if (! scpPollQueue ())
      return;
    char * msg = scpDequeueMsg ();
    if (msg)
      {
        sim_printf ("dia dequeued %s\n", msg);

        size_t msg_len = strlen (msg);
        char keyword [msg_len];
        sscanf (msg, "%s", keyword);


        // "crdrdy %d"  -- Input hopper is ready on card reader sim unit n
        if (strcmp(keyword, "crdrdy") == 0)
          {
            int unitNum;
            int n = sscanf(msg, "%*s %d", & unitNum);
            if (n != 1)
              {
                sim_printf ("illformatted crdrdy message; dropping\n");  
                goto done;
              }
sim_printf ("dia thinks crdrdy %d\n", unitNum);
            crdrdrCardReady (unitNum);
            goto done;
          }
          
        sim_printf ("dia dequeued and ignored%s\n", msg);

done:
        free (msg);
      }
  }

t_stat scpCommand (UNUSED char *nodename, UNUSED char *id, char *arg3)
  {
    // ASSUME0 XXX parse nodename to get unit #
    scpQueueMsg (arg3);
    return SCPE_OK;
  }

static t_stat bootSkip (int32 UNUSED arg, const char * UNUSED buf)
  {
    uint32 skipped;
    return sim_tape_sprecsf (& mt_unit [0], 1, & skipped);
  }
  
