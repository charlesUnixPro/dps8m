
/**
 * \file dps8_sys.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>
#include <wordexp.h>
#include <signal.h>

#include "dps8.h"
#include "dps8_console.h"
#include "dps8_clk.h"
#include "dps8_cpu.h"
#include "dps8_ins.h"
#include "dps8_iom.h"
#include "dps8_loader.h"
#include "dps8_math.h"
#include "dps8_scu.h"
#include "dps8_sys.h"
#include "dps8_mt.h"
#include "dps8_disk.h"
#include "dps8_utils.h"
#include "dps8_fxe.h"
#include "dps8_append.h"
#include "dps8_faults.h"
#include "dps8_fnp.h"
#include "dps8_crdrdr.h"
#include "utlist.h"

#ifdef MULTIPASS
#include "dps8_mp.h"
#include "shm.h"
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#endif

#include "fnp_ipc.h"        /*  for fnp IPC stuff */

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


// These are part of the shm interface

static pid_t dps8m_sid; // Session id

static char * lookupSystemBookAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset);


stats_t sys_stats;

static t_stat sys_cable (int32 arg, char * buf);
static t_stat dps_debug_start (int32 arg, char * buf);
static t_stat dps_debug_stop (int32 arg, char * buf);
static t_stat dps_debug_break (int32 arg, char * buf);
static t_stat dps_debug_segno (int32 arg, char * buf);
static t_stat dps_debug_ringno (int32 arg, char * buf);
static t_stat loadSystemBook (int32 arg, char * buf);
static t_stat addSystemBookEntry (int32 arg, char * buf);
static t_stat lookupSystemBook (int32 arg, char * buf);
static t_stat absAddr (int32 arg, char * buf);
static t_stat setSearchPath (int32 arg, char * buf);
static t_stat absAddrN (int segno, uint offset);
//static t_stat virtAddrN (uint address);
static t_stat virtAddr (int32 arg, char * buf);
static t_stat sbreak (int32 arg, char * buf);
static t_stat stackTrace (int32 arg, char * buf);
static t_stat listSourceAt (int32 arg, char * buf);
static t_stat doEXF (UNUSED int32 arg,  UNUSED char * buf);
static t_stat launch (int32 arg, char * buf);
#ifdef MULTIPASS
static void multipassInit (pid_t sid);
#endif
#ifdef DVFDBG
static t_stat dfx1entry (int32 arg, char * buf);
static t_stat dfx1exit (int32 arg, char * buf);
static t_stat dv2scale (int32 arg, char * buf);
static t_stat dfx2entry (int32 arg, char * buf);
static t_stat mdfx3entry (int32 arg, char * buf);
static t_stat smfx1entry (int32 arg, char * buf);
#endif
static t_stat searchMemory (UNUSED int32 arg, char * buf);

static CTAB dps8_cmds[] =
{
    {"DPSINIT",  dpsCmd_Init,     0, "dpsinit dps8/m initialize stuff ...\n", NULL},
    {"DPSDUMP",  dpsCmd_Dump,     0, "dpsdump dps8/m dump stuff ...\n", NULL},
    {"SEGMENT",  dpsCmd_Segment,  0, "segment dps8/m segment stuff ...\n", NULL},
    {"SEGMENTS", dpsCmd_Segments, 0, "segments dps8/m segments stuff ...\n", NULL},
    {"CABLE",    sys_cable,       0, "cable String a cable\n" , NULL},
    {"DBGSTART", dps_debug_start, 0, "dbgstart Limit debugging to N > Cycle count\n", NULL},
    {"DBGSTOP", dps_debug_stop, 0, "dbgstop Limit debugging to N < Cycle count\n", NULL},
    {"DBGBREAK", dps_debug_break, 0, "dbgstop Break when N >= Cycle count\n", NULL},
    {"DBGSEGNO", dps_debug_segno, 0, "dbgsegno Limit debugging to PSR == segno\n", NULL},
    {"DBGRINGNO", dps_debug_ringno, 0, "dbgsegno Limit debugging to PRR == ringno\n", NULL},
    {"DISPLAYMATRIX", displayTheMatrix, 0, "displaymatrix Display instruction usage counts\n", NULL},
    {"LD_SYSTEM_BOOK", loadSystemBook, 0, "load_system_book: Load a Multics system book for symbolic debugging\n", NULL},
    {"ASBE", addSystemBookEntry, 0, "asbe: Add an entry to the system book\n", NULL},
    {"LOOKUP_SYSTEM_BOOK", lookupSystemBook, 0, "lookup_system_book: lookup an address or symbol in the Multics system book\n", NULL},
    {"LSB", lookupSystemBook, 0, "lsb: lookup an address or symbol in the Multics system book\n", NULL},
    {"ABSOLUTE", absAddr, 0, "abs: Compute the absolute address of segno:offset\n", NULL},
    {"VIRTUAL", virtAddr, 0, "virtual: Compute the virtural address(es) of segno:offset\n", NULL},
    {"SPATH", setSearchPath, 0, "spath: Set source code search path\n", NULL},
    {"TEST", brkbrk, 0, "test: internal testing\n", NULL},
// copied from scp.c
#define SSH_ST          0                               /* set */
#define SSH_SH          1                               /* show */
#define SSH_CL          2                               /* clear */
    {"SBREAK", sbreak, SSH_ST, "sbreak: Set a breakpoint with segno:offset syntax\n", NULL},
    {"NOSBREAK", sbreak, SSH_CL, "nosbreak: Unset an SBREAK\n", NULL},
    {"FXE", fxe, 0, "fxe: enter the FXE environment\n", NULL},
    {"FXEDUMP", fxeDump, 0, "fxedump: dump the FXE environment\n", NULL},
    {"STK", stackTrace, 0, "stk: print a stack trace\n", NULL},
    {"LIST", listSourceAt, 0, "list segno:offet: list source for an address\n", NULL},
    {"XF", doEXF, 0, "Execute fault: Press the execute fault button\n", NULL},
#ifdef DVFDBG
    // dvf debugging
    {"DFX1ENTRY", dfx1entry, 0, "", NULL},
    {"DFX2ENTRY", dfx2entry, 0, "", NULL},
    {"DFX1EXIT", dfx1exit, 0, "", NULL},
    {"DV2SCALE", dv2scale, 0, "", NULL},
    {"MDFX3ENTRY", mdfx3entry, 0, "", NULL},
    {"SMFX1ENTRY", smfx1entry, 0, "", NULL},
#endif
    {"DUMPKST", dumpKST, 0, "dumpkst: dump the Known Segment Table\n", NULL},
    {"WATCH", memWatch, 1, "watch: watch memory location\n", NULL},
    {"NOWATCH", memWatch, 0, "watch: watch memory location\n", NULL},
    {"AUTOINPUT", opconAutoinput, 0, "set console auto-input\n", NULL},
    {"CLRAUTOINPUT", opconAutoinput, 1, "clear console auto-input\n", NULL},
    {"LAUNCH", launch, 0, "start subprocess\n", NULL},
    
#ifdef VM_DPS8
    {"SHOUT",  ipc_shout,       0, "Shout (broadcast) message to all connected peers\n", NULL},
    {"WHISPER",ipc_whisper,     0, "Whisper (per-to-peer) message to specified peer\n", NULL},
#endif
    
    {"SEARCHMEMORY", searchMemory, 0, "searchMemory: search memory for value\n", NULL},

    { NULL, NULL, 0, NULL, NULL}
};

/*!
 \brief special dps8 VM commands ....
 
 For greater flexibility, SCP provides some optional interfaces that can be used to extend its command input, command processing, and command post-processing capabilities. These interfaces are strictly optional
 and are off by default. Using them requires intimate knowledge of how SCP functions internally and is not recommended to the novice VM writer.
 
 Guess I shouldn't use these then :)
 */

static t_addr parse_addr(DEVICE *dptr, char *cptr, char **optr);
static void fprint_addr(FILE *stream, DEVICE *dptr, t_addr addr);

// Once-only initialization

static void dps8_init(void)
{
    // special dps8 initialization stuff that cant be done in reset, etc .....

    // These are part of the simh interface
    sim_vm_parse_addr = parse_addr;
    sim_vm_fprint_addr = fprint_addr;

    sim_vm_cmd = dps8_cmds;

    // Create a session for this dps8m system instance.
    dps8m_sid = setsid ();
    if (dps8m_sid == (pid_t) -1)
      dps8m_sid = getsid (0);
    sim_printf ("DPS8M system session id is %d\n", dps8m_sid);

    init_opcodes();
    iom_init ();
    console_init ();
    disk_init ();
    mt_init ();
    fnpInit ();
    //mpc_init ();
    scu_init ();
    cpu_init ();
    crdrdr_init ();
#ifdef MULTIPASS
    multipassInit (dps8m_sid);
#endif
}

static int getval (char * * save, char * text)
  {
    char * value;
    char * endptr;
    value = strtok_r (NULL, ",", save);
    if (! value)
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: can't parse %s\n", text);
        sim_printf ("error: sys_cable: can't parse %s\n", text);
        return -1;
      }
    long l = strtol (value, & endptr, 0);
    if (* endptr || l < 0 || l > INT_MAX)
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: can't parse %s <%s>\n", text, value);
        sim_printf ("error: sys_cable: can't parse %s <%s>\n", text, value);
        return -1;
      }
    return (int) l;
  }

// Connect dev to iom
//
//   cable [TAPE | DISK],<dev_unit_num>,<iom_unit_num>,<chan_num>,<dev_code>
//
//   or iom to scu
//
//   cable IOM <iom_unit_num>,<iom_port_num>,<scu_unit_num>,<scu_port_num>
//
//   or scu to cpu
//
//   cable SCU <scu_unit_num>,<scu_port_num>,<cpu_unit_num>,<cpu_port_num>
//
//   or opcon to iom
//
//   cable OPCON <iom_unit_num>,<chan_num>,0,0
//

static t_stat sys_cable (UNUSED int32 arg, char * buf)
  {
// XXX Minor bug; this code doesn't check for trailing garbage

    char * copy = strdup (buf);
    t_stat rc = SCPE_ARG;

    // process statement

    // extract name
    char * name_save = NULL;
    char * name;
    name = strtok_r (copy, ",", & name_save);
    if (! name)
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: can't parse name\n");
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
    else if (strcasecmp (name, "DISK") == 0)
      {
        rc = cable_disk (n1, n2, n3, n4);
      }
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
    else if (strcasecmp (name, "FNP") == 0)
      {
        rc = cableFNP (n1, n2, n3, n4);
      }
    else if (strcasecmp (name, "CRDRDR") == 0)
      {
        rc = cable_crdrdr (n1, n2, n3, n4);
      }
    else
      {
        //sim_debug (DBG_ERR, & sys_dev, "sys_cable: Invalid switch name <%s>\n", name);
        sim_printf ("error: sys_cable: invalid switch name <%s>\n", name);
        goto exit;
      }

exit:
    free (copy);
    return rc;
  }

uint64 sim_deb_start = 0;
uint64 sim_deb_stop = 0;
uint64 sim_deb_break = 0;
uint64 sim_deb_segno = NO_SUCH_SEGNO;
uint64 sim_deb_ringno = NO_SUCH_RINGNO;

static t_stat dps_debug_start (UNUSED int32 arg, char * buf)
  {
    sim_deb_start = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to start at cycle: %lld\n", sim_deb_start);
    return SCPE_OK;
  }

static t_stat dps_debug_stop (UNUSED int32 arg, char * buf)
  {
    sim_deb_stop = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to stop at cycle: %lld\n", sim_deb_stop);
    return SCPE_OK;
  }

static t_stat dps_debug_break (UNUSED int32 arg, char * buf)
  {
    sim_deb_break = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to break at cycle: %lld\n", sim_deb_break);
    return SCPE_OK;
  }

static t_stat dps_debug_segno (UNUSED int32 arg, char * buf)
  {
    sim_deb_segno = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to segno %llo\n", sim_deb_segno);
    return SCPE_OK;
  }

static t_stat dps_debug_ringno (UNUSED int32 arg, char * buf)
  {
    sim_deb_ringno = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to ringno %llo\n", sim_deb_ringno);
    return SCPE_OK;
  }

// LOAD_SYSTEM_BOOK <filename>
#define bookSegmentsMax 1024
#define bookComponentsMax 4096
#define bookSegmentNameLen 33
struct bookSegment
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
    if (ret)
      return ret;
    ret = lookupFXESegmentAddress (segno, offset, compname, compoffset);
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
int lookupSystemBookName (char * segname, char * compname, long * segno, long * offset)
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
            * offset = bookComponents [j] . txt_start;
            return 0;
          }
      }

   return -1;
 }

static char * sourceSearchPath = NULL;

// search path is path:path:path....

static t_stat setSearchPath (UNUSED int32 arg, char * buf)
  {
    if (sourceSearchPath)
      free (sourceSearchPath);
    sourceSearchPath = strdup (buf);
    return SCPE_OK;
  }

t_stat brkbrk (UNUSED int32 arg, UNUSED char *  buf)
  {
    //listSource (buf, 0);
    return SCPE_OK;
  }

static t_stat listSourceAt (UNUSED int32 arg, UNUSED char *  buf)
  {
    // list seg:offset
    int segno;
    uint offset;
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    char * compname;
    word18 compoffset;
    char * where = lookupAddress (segno, offset,
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
                    if (strncmp (line, offset_str, offset_str_len) == 0)
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

static t_stat searchMemory (UNUSED int32 arg, char * buf)
  {
    word36 value;
    if (sscanf (buf, "%llo", & value) != 1)
      return SCPE_ARG;
    
    uint i;
    for (i = 0; i < MEMSIZE; i ++)
      if ((M [i] & DMASK) == value)
        sim_printf ("%08o\n", i);
    return SCPE_OK;
  }


// ABS segno:offset

static t_stat absAddr (UNUSED int32 arg, char * buf)
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

    if (DSBR.U == 1) // Unpaged
      {
        if (2 * (uint) /*TPR . TSR*/ segno >= 16 * ((uint) DSBR . BND + 1))
          {
            sim_printf ("DSBR boundary violation.\n");
            return SCPE_ARG;
          }

        // 2. Fetch the target segment SDW from DSBR.ADDR + 2 * segno.

        word36 SDWe, SDWo;
        core_read ((DSBR . ADDR + 2U * /*TPR . TSR*/ (uint) segno) & PAMASK, & SDWe, __func__);
        core_read ((DSBR . ADDR + 2U * /*TPR . TSR*/ (uint) segno  + 1) & PAMASK, & SDWo, __func__);

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

        // 1. If 2 * segno >= 16 * (DSBR.BND + 1), then generate an access 
        // violation, out of segment bounds, fault.

        if (2 * (uint) segno >= 16 * ((uint) DSBR . BND + 1))
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
        core_read ((DSBR . ADDR + x1) & PAMASK, & PTWx1, __func__);

        struct _ptw0 PTW1;
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

        struct _sdw0 SDW0; 
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
    
            struct _ptw0 PTW2;
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
    if (dbgLookupAddress (segno, offset, & res, NULL))
      return SCPE_ARG;

    sim_printf ("Address is %08o\n", res);
    return SCPE_OK;
  }

// EXF

static t_stat doEXF (UNUSED int32 arg,  UNUSED char * buf)
  {
    setG7fault (FAULT_EXF, 0);
    return SCPE_OK;
  }

// STK 

t_stat dbgStackTrace (void)
  {
    return stackTrace (0, "");
  }

static t_stat stackTrace (UNUSED int32 arg,  UNUSED char * buf)
  {
    char * msg;

    word15 icSegno = PPR . PSR;
    word18 icOffset = PPR . IC;
    
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

    word15 fpSegno = PR [6] . SNR;
    word15 fpOffset = PR [6] . WORDNO;

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
                where = lookupAddress (icSegno, rX [7] - 1,
                                       & compname, & compoffset);
                if (where)
                  {
                    sim_printf ("%05o:%06o %s\n", icSegno, rX [7] - 1, where);
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
                sim_printf ("arg%d value   %05o:%06o [%08o] %012llo (%llu)\n", 
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

static t_stat sbreak (int32 arg, char * buf)
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

static t_stat virtAddr (UNUSED int32 arg, char * buf)
  {
    uint address;
    if (sscanf (buf, "%o", & address) != 1)
      return SCPE_ARG;
    return virtAddrN (address);
  }

t_stat virtAddrN (uint address)
  {
    if (DSBR.U) {
        for(word15 segno = 0; 2 * segno < 16 * (DSBR.BND + 1); segno += 1)
        {
            _sdw0 *s = fetchSDW(segno);
            if (address >= s -> ADDR && address < s -> ADDR + s -> BOUND * 16)
              sim_printf ("  %06o:%06o\n", segno, address - s -> ADDR);
        }
    } else {
        for(word15 segno = 0; 2 * segno < 16 * (DSBR.BND + 1); segno += 512)
        {
            word24 y1 = (2 * segno) % 1024;
            word24 x1 = (2 * segno - y1) / 1024;
            word36 PTWx1;
            core_read ((DSBR . ADDR + x1) & PAMASK, & PTWx1, __func__);

            struct _ptw0 PTW1;
            PTW1.ADDR = GETHI(PTWx1);
            PTW1.U = TSTBIT(PTWx1, 9);
            PTW1.M = TSTBIT(PTWx1, 6);
            PTW1.F = TSTBIT(PTWx1, 2);
            PTW1.FC = PTWx1 & 3;
           
            if (PTW1.F == 0)
                continue;
            //sim_printf ("%06o  Addr %06o U %o M %o F %o FC %o\n", 
            //            segno, PTW1.ADDR, PTW1.U, PTW1.M, PTW1.F, PTW1.FC);
            //sim_printf ("    Target segment page table\n");
            for (word15 tspt = 0; tspt < 512; tspt ++)
            {
                word36 SDWeven, SDWodd;
                core_read2(((PTW1 . ADDR << 6) + tspt * 2) & PAMASK, & SDWeven, & SDWodd, __func__);
                struct _sdw0 SDW0;
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

                if (SDW0.F == 0)
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

                        struct _ptw0 PTW2;
                        PTW2.ADDR = GETHI(PTWx2);
                        PTW2.U = TSTBIT(PTWx2, 9);
                        PTW2.M = TSTBIT(PTWx2, 6);
                        PTW2.F = TSTBIT(PTWx2, 2);
                        PTW2.FC = PTWx2 & 3;

                        //sim_printf ("        %06o  Addr %06o U %o M %o F %o FC %o\n", 
                        //             offset, PTW2.ADDR, PTW2.U, PTW2.M, PTW2.F, PTW2.FC);
                        if (address >= PTW2.ADDR + offset && address < PTW2.ADDR + offset + 1024)
                          sim_printf ("  %06o:%06o\n", tspt, (address - offset) - PTW2.ADDR);

                      }
                  }
                else
                  {
                    if (address >= SDW0.ADDR && address < SDW0.ADDR + SDW0.BOUND * 16)
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
   
static t_stat lookupSystemBook (UNUSED int32  arg, char * buf)
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
        char * ans = lookupAddress (segno, offset, NULL, NULL);
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
        absAddrN  (segno, comp_offset + offset);
      }
/*
    if (sscanf (buf, "%o:%o", & segno, & offset) != 2)
      return SCPE_ARG;
    char * ans = lookupAddress (segno, offset);
    sim_printf ("%s\n", ans ? ans : "not found");
*/
    return SCPE_OK;
  }

static t_stat addSystemBookEntry (UNUSED int32 arg, char * buf)
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

    int idx = addBookSegment (segname, segno);
    if (idx < 0)
      return SCPE_ARG;

    if (addBookComponent (idx, compname, txt_start, txt_len, intstat_start, intstat_length, symbol_start, symbol_length) < 0)
      return SCPE_ARG;

    return SCPE_OK;
  }

static t_stat loadSystemBook (UNUSED int32 arg, char * buf)
  {
  
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
                int rc = addBookSegment (name, c3 ++);
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

static t_addr parse_addr (UNUSED DEVICE * dptr, char *cptr, char **optr)
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
                segno = PR[prt->n].SNR;
                offset = PR[prt->n].WORDNO;
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
    return (t_addr)strtol(cptr, optr, 8);
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
            
            for(int n = 0 ; n < p->info->ndes; n += 1)
                fprintf(ofile, " %012llo", val[n + 1]);
          
            return -p->info->ndes;
        }
        
        return SCPE_OK;

        //fprintf(ofile, "%012llo", *val);
        //return SCPE_OK;
    }
    return SCPE_ARG;
}

// This is part of the simh interface
/*!  – Based on the switch variable, parse character string cptr for a symbolic value val at the specified addr
 in unit uptr.
 */
t_stat parse_sym (UNUSED char * cptr, UNUSED t_addr addr, UNUSED UNIT * uptr, 
                  UNUSED t_value * val, UNUSED int32 sswitch)
{
    return SCPE_ARG;
}

// from MM

sysinfo_t sys_opts =
  {
    0, /* clock speed */
    {
// I get too much jitter in cpuCycles when debugging 20184; try turning queing
// off here (changing 0 to -1)
// still get a little jitter, and once a hang in DIS. very strange
      -1, /* iom_times.connect */
       0,  /* iom_times.chan_activate */
      10, /* boot_time */
      10000, /* terminate_time */
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
                               UNUSED int  val, UNUSED void * desc)
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
                              char * cptr, UNUSED void * desc)
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
static t_stat dfx1entry (UNUSED int32 arg, UNUSED char * buf)
  {
// divide_fx1, divide_fx3
    sim_printf ("dfx1entry\n");
    sim_printf ("rA %012llo (%llu)\n", rA, rA);
    sim_printf ("rQ %012llo (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012llo (%llu)\n", M [pa], M [pa]);
      }
sim_printf ("%05o:%06o\n", PR [2] . SNR, rX [0]);
//dbgStackTrace ();
    if (dbgLookupAddress (PR [2] . SNR, rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale %012llo (%llu)\n", M [pa], M [pa]);
      }
    if (dbgLookupAddress (PR [2] . SNR, PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012llo (%llu)\n", M [pa], M [pa]);
      }
    return SCPE_OK;
  }

static t_stat dfx1exit (UNUSED int32 arg, UNUSED char * buf)
  {
    sim_printf ("dfx1exit\n");
    sim_printf ("rA %012llo (%llu)\n", rA, rA);
    sim_printf ("rQ %012llo (%llu)\n", rQ, rQ);
    return SCPE_OK;
  }

static t_stat dv2scale (UNUSED int32 arg, UNUSED char * buf)
  {
    sim_printf ("dv2scale\n");
    sim_printf ("rQ %012llo (%llu)\n", rQ, rQ);
    return SCPE_OK;
  }

static t_stat dfx2entry (UNUSED int32 arg, UNUSED char * buf)
  {
// divide_fx2
    sim_printf ("dfx2entry\n");
    sim_printf ("rA %012llo (%llu)\n", rA, rA);
    sim_printf ("rQ %012llo (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012llo (%llu)\n", M [pa], M [pa]);
      }
#if 0
sim_printf ("%05o:%06o\n", PR [2] . SNR, rX [0]);
//dbgStackTrace ();
    if (dbgLookupAddress (PR [2] . SNR, rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale ptr %012llo (%llu)\n", M [pa], M [pa]);
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
                sim_printf ("scale %012llo (%llu)\n", M [ipa], M [ipa]);
              }
          }
      }
#endif
    if (dbgLookupAddress (PR [2] . SNR, PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012llo (%llu)\n", M [pa], M [pa]);
        sim_printf ("divisor %012llo (%llu)\n", M [pa + 1], M [pa + 1]);
      }
    return SCPE_OK;
  }

static t_stat mdfx3entry (UNUSED int32 arg, UNUSED char * buf)
  {
// operator to form mod(fx2,fx1)
// entered with first arg in q, bp pointing at second

// divide_fx1, divide_fx2
    sim_printf ("mdfx3entry\n");
    //sim_printf ("rA %012llo (%llu)\n", rA, rA);
    sim_printf ("rQ %012llo (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012llo (%llu)\n", M [pa], M [pa]);
      }
//sim_printf ("%05o:%06o\n", PR [2] . SNR, rX [0]);
//dbgStackTrace ();
#if 0
    if (dbgLookupAddress (PR [2] . SNR, rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale %012llo (%llu)\n", M [pa], M [pa]);
      }
#endif
    if (dbgLookupAddress (PR [2] . SNR, PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012llo (%llu)\n", M [pa], M [pa]);
      }
    return SCPE_OK;
  }

static t_stat smfx1entry (UNUSED int32 arg, UNUSED char * buf)
  {
// operator to form mod(fx2,fx1)
// entered with first arg in q, bp pointing at second

// divide_fx1, divide_fx2
    sim_printf ("smfx1entry\n");
    //sim_printf ("rA %012llo (%llu)\n", rA, rA);
    sim_printf ("rQ %012llo (%llu)\n", rQ, rQ);
    // Figure out the caller's text segment, according to pli_operators.
    // sp:tbp -> PR[6].SNR:046
    word24 pa;
    char * msg;
    if (dbgLookupAddress (PR [6] . SNR, 046, & pa, & msg))
      {
        sim_printf ("text segment number lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("text segno %012llo (%llu)\n", M [pa], M [pa]);
      }
sim_printf ("%05o:%06o\n", PR [2] . SNR, rX [0]);
//dbgStackTrace ();
    if (dbgLookupAddress (PR [2] . SNR, rX [0], & pa, & msg))
      {
        sim_printf ("return address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("scale %012llo (%llu)\n", M [pa], M [pa]);
      }
    if (dbgLookupAddress (PR [2] . SNR, PR [2] . WORDNO, & pa, & msg))
      {
        sim_printf ("divisor address lookup failed because %s\n", msg);
      }
    else
      {
        sim_printf ("divisor %012llo (%llu)\n", M [pa], M [pa]);
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
    NULL         /* description */
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
    & clk_dev,
    // & mpc_dev,
    & opcon_dev,
    & sys_dev,
    & fxe_dev,
    & ipc_dev,  // for fnp IPC
    & crdrdr_dev,
    NULL
  };

#ifdef MULTIPASS

multipassStats * multipassStatsPtr;

// Once only initialization
static void multipassInit (pid_t sid)
  {
#if 0
    //sim_printf ("Session %d\n", getsid (0));
    pid_t pid = getpid ();
    char buf [256];
    sprintf (buf, "/dps8m.%u.multipass", pid);
    int fd = shm_open (buf, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
      {
        sim_printf ("multipass shm_open fail %d\n", errno);
        return;
      }

    if (ftruncate (fd, sizeof (multipassStats)) == -1)
      {
        sim_printf ("multipass ftruncate  fail %d\n", errno);
        return;
      }

    multipassStatsPtr = (multipassStats *) mmap (NULL, sizeof (multipassStats),
                                                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (multipassStatsPtr == MAP_FAILED)
      {
        sim_printf ("multipass mmap  fail %d\n", errno);
        return;
      }
#if 0
    multipassStatsPtr = NULL;
    mpStatsSegID = shmget (0x6180 + switches . cpu_num, sizeof (multipassStats),
                         IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (mpStatsSegID == -1)
      {
        perror ("multipassInit shmget");
        return;
      }
    multipassStatsPtr = (multipassStats *) shmat (mpStatsSegID, 0, 0);
    if (multipassStatsPtr == (void *) -1)
      {
        perror ("multipassInit shmat");
        return;
      }
    shmctl (mpStatsSegID, IPC_RMID, 0);
#endif
#endif
    multipassStatsPtr = (multipassStats *) create_shm ("multipass", sid,
      sizeof (multipassStats));
    if (! multipassStatsPtr)
      {
        sim_printf ("create_shm multipass failed\n");
        sim_err ("create_shm multipass failed\n");
      }
  }
#endif

#define MAX_CHILDREN 256
static int nChildren = 0;
static pid_t childrenList [MAX_CHILDREN];

static void cleanupChildren (void)
  {
    printf ("cleanupChildren\n");
    for (int i = 0; i < nChildren; i ++)
      {
        printf ("  kill %d\n", childrenList [i]);
        kill (childrenList [i], SIGHUP);
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

static t_stat launch (int32 UNUSED arg, char * buf)
  {
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

    return SCPE_OK;
  }

// SCP message queue; when IPC messages come in, they are append to this
// queue. The sim_instr loop will poll the queue for messages for delivery 
// to the simh code.

pthread_mutex_t scpMQlock;
typedef struct scpQueueElement scpQueueElement;
struct scpQueueElement
  {
    char * msg;
    scpQueueElement * prev, * next;
  };

scpQueueElement * scpQueue = NULL;

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

        //size_t msg_len = strlen (msg);
        //char keyword [msg_len];
        //sscanf (msg, "%s", keyword);

        //if (strcmp(keyword, "attach") == 0)

//drop:
        free (msg);
      }
  }

t_stat scpCommand (UNUSED char *nodename, UNUSED char *id, char *arg3)
  {
    // ASSUME0 XXX parse nodename to get unit #
    scpQueueMsg (arg3);
    return SCPE_OK;
  }

  
