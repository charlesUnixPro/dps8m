
/**
 * \file dps8_sys.c
 * \project dps8
 * \date 9/17/12
 * \copyright Copyright (c) 2012 Harry Reed. All rights reserved.
*/

#include <stdio.h>

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

static char * lookupSystemBookAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset);


stats_t sys_stats;

static t_stat sys_cable (int32 arg, char * buf);
static t_stat dps_debug_start (int32 arg, char * buf);
static t_stat dps_debug_stop (int32 arg, char * buf);
static t_stat dps_debug_break (int32 arg, char * buf);
static t_stat loadSystemBook (int32 arg, char * buf);
static t_stat lookupSystemBook (int32 arg, char * buf);
static t_stat absAddr (int32 arg, char * buf);
static t_stat setSearchPath (int32 arg, char * buf);
static t_stat absAddrN (int segno, uint offset);
static t_stat test (int32 arg, char * buf);
static t_stat virtAddrN (uint address);
static t_stat virtAddr (int32 arg, char * buf);
static t_stat sbreak (int32 arg, char * buf);

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
    {"DISPLAYMATRIX", displayTheMatrix, 0, "displaymatrix Display instruction usage counts\n", NULL},
    {"LD_SYSTEM_BOOK", loadSystemBook, 0, "load_system_book: Load a Multics system book for symbolic debugging\n", NULL},
    {"LOOKUP_SYSTEM_BOOK", lookupSystemBook, 0, "lookup_system_book: lookup an address or symbol in the Multics system book\n", NULL},
    {"LSB", lookupSystemBook, 0, "lsb: lookup an address or symbol in the Multics system book\n", NULL},
    {"ABSOLUTE", absAddr, 0, "abs: Compute the absolute address of segno:offset\n", NULL},
    {"VIRTUAL", virtAddr, 0, "virtual: Compute the virtural address(es) of segno:offset\n", NULL},
    {"SPATH", setSearchPath, 0, "spath: Set source code search path\n", NULL},
    {"TEST", test, 0, "test: internal testing\n", NULL},
// copied from scp.c
#define SSH_ST          0                               /* set */
#define SSH_SH          1                               /* show */
#define SSH_CL          2                               /* clear */
    {"SBREAK", sbreak, SSH_ST, "sbreak: Set a breakpoint with segno:offset syntax\n", NULL},
    {"NOSBREAK", sbreak, SSH_CL, "nosbreak: Unset an SBREAK\n", NULL},
    {"FXE", fxe, 0, "fxe: enter the FXE environment\n", NULL},
    {"FXEDUMP", fxeDump, 0, "fxedump: dump the FXE environment\n", NULL},
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
    cpu_init ();
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

static t_stat sys_cable (int32 __attribute__((unused)) arg, char * buf)
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
        rc = cable_opcon (n1, n2);
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

static t_stat dps_debug_start (int32 __attribute__((unused)) arg, char * buf)
  {
    sim_deb_start = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to start at cycle: %lld\n", sim_deb_start);
    return SCPE_OK;
  }

static t_stat dps_debug_stop (int32 __attribute__((unused)) arg, char * buf)
  {
    sim_deb_stop = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to stop at cycle: %lld\n", sim_deb_stop);
    return SCPE_OK;
  }

static t_stat dps_debug_break (int32 __attribute__((unused)) arg, char * buf)
  {
    sim_deb_break = strtoull (buf, NULL, 0);
    sim_printf ("Debug set to break at cycle: %lld\n", sim_deb_break);
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
    if (compname)
      * compname = NULL;
    if (compoffset)
      * compoffset = 0;
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

static t_stat setSearchPath (int32 __attribute__((unused)) arg, char * buf)
  {
    if (sourceSearchPath)
      free (sourceSearchPath);
    sourceSearchPath = strdup (buf);
    return SCPE_OK;
  }

static t_stat test (int32 __attribute__((unused)) arg, char *  __attribute__((unused)) buf)
  {
    //listSource (buf, 0);
    return SCPE_OK;
  }

void listSource (char * compname, word18 offset)
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
            char line [133];
            if (feof (listing))
              goto fileDone;
            fgets (line, 132, listing);
            if (strncmp (line, "ASSEMBLY LISTING", 16) == 0)
              {
                // Search ALM listing file
                // sim_printf ("found <%s>\n", path);

                // ALM listing files look like:
                //     000226  4a  4 00010 7421 20  \tstx2]tbootload_0$entry_stack_ptr,id
                while (! feof (listing))
                  {
                    fgets (line, 132, listing);
                    if (strncmp (line, offset_str, offset_str_len) == 0)
                      {
                        sim_printf ("%s", line);
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
                    fgets (line, 132, listing);
                    if (strncmp (line, "   LINE    LOC", 14) != 0)
                      continue;
                    foundTable = true;
                    // Found the table
                    // Table lines look like
                    //     "     13 000705       275 000713  ...
                    int best = -1;
                    int bestLine = -1;
                    while (! feof (listing))
                      {
                        int lineno [7], loc [7];
                        fgets (line, 132, listing);
                        int cnt = sscanf (line,
                          " %d %o %d %o %d %o %d %o %d %o %d %o %d %o", 
                          & lineno [0], & loc [0], 
                          & lineno [1], & loc [1], 
                          & lineno [2], & loc [2], 
                          & lineno [3], & loc [3], 
                          & lineno [4], & loc [4], 
                          & lineno [5], & loc [5], 
                          & lineno [6], & loc [6]);
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
                                bestLine = lineno [n];
                              }
                          }
                        if (best == (int) offset)
                          break;
                      }
                    if (best == -1)
                      goto fileDone; // Not found in table

                    // Look for the line in the listing
                    rewind (listing);
                    while (! feof (listing))
                      {
                        fgets (line, 132, listing);
                        if (strncmp (line, "\f\tSOURCE", 8) == 0)
                          goto fileDone; // end of source code listing
                        char prefix [10];
                        strncpy (prefix, line, 9);
                        prefix [9] = '\0';
                        char * endptr;
                        long lno = strtol (prefix, & endptr, 10);
                        if (endptr != prefix + 9)
                          continue;
                        if (lno > bestLine)
                          break;
                        if (lno != bestLine)
                          continue;
                        // Got it
                        sim_printf ("%s", line);
                        break;
                      }
                    goto fileDone;
                  } // if table start
                if (! foundTable)
                  {
                    // Can't find the LINE/LOC table; look for listing
                    rewind (listing);
                    while (! feof (listing))
                      {
                        fgets (line, 132, listing);
                        if (strncmp (line, offset_str + 4, offset_str_len - 4) == 0)
                          {
                            sim_printf ("%s", line);
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

// ABS segno:offset

static t_stat absAddr (int32 __attribute__((unused)) arg, char * buf)
  {
    int segno;
    uint offset;
    if (sscanf (buf, "%i:%u", & segno, & offset) != 2)
      return SCPE_ARG;
    return absAddrN (segno, offset);
  }

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
        core_read (DSBR . ADDR + 2U * /*TPR . TSR*/ (uint) segno, & SDWe);
        core_read (DSBR . ADDR + 2U * /*TPR . TSR*/ (uint) segno  + 1, & SDWo);

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
        core_read ((DSBR . ADDR + x1) & PAMASK, & PTWx1);

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
        core_read2(((PTW1 . ADDR << 6) + y1) & PAMASK, & SDWeven, & SDWodd);

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
            core_read ((SDW0 . ADDR + x2) & PAMASK, & PTWx2);
    
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
            //     doFault(dir_flt0_fault + PTW2.FC, 0, "ABSA !PTW2.F");
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

static t_stat absAddrN (int segno, uint offset)
  {
    word24 res;

    t_stat rc = computeAbsAddrN (& res, segno, offset);

    if (rc)
      return rc;

    sim_printf ("Address is %08o\n", res);
    return SCPE_OK;
  }

// SBREAK segno:offset

static t_stat sbreak (int32 arg, char * buf)
  {
    //printf (">> <%s>\n", buf);
    int segno, offset;
    int where;
    int cnt = sscanf (buf, "%i:%i%n", & segno, & offset, & where);
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


// VIRTUAL address

static t_stat virtAddr (int32 __attribute__((unused)) arg, char * buf)
  {
    uint address;
    if (sscanf (buf, "%u", & address) != 1)
      return SCPE_ARG;
    return virtAddrN (address);
  }

static t_stat virtAddrN (uint address)
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
            core_read ((DSBR . ADDR + x1) & PAMASK, & PTWx1);

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
                core_read2(((PTW1 . ADDR << 6) + tspt * 2) & PAMASK, & SDWeven, & SDWodd);
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
                    for (word18 offset = 0; offset < 16 * (SDW0.BOUND + 1); offset += 1024)
                    {
                        word24 y2 = offset % 1024;
                        word24 x2 = (offset - y2) / 1024;

                        // 10. Fetch the target segment PTW(x2) from SDW(segno).ADDR + x2.

                        word36 PTWx2;
                        core_read ((SDW0 . ADDR + x2) & PAMASK, & PTWx2);

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
   
static t_stat lookupSystemBook (int32  __attribute__((unused)) arg, char * buf)
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
    segno = strtol (w1, & end1, 0);
    char * end2;
    offset = strtol (w2, & end2, 0);

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
            offset = strtol (w3, & end3, 0);
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
    if (sscanf (buf, "%i:%i", & segno, & offset) != 2)
      return SCPE_ARG;
    char * ans = lookupAddress (segno, offset);
    sim_printf ("%s\n", ans ? ans : "not found");
*/
    return SCPE_OK;
  }

static t_stat loadSystemBook (int32  __attribute__((unused)) arg, char * buf)
  {
  
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

static t_addr parse_addr(DEVICE * __attribute__((unused)) dptr, char *cptr, char **optr)
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

static void fprint_addr(FILE *stream, DEVICE * __attribute__((unused)) dptr, t_addr simh_addr)
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

t_stat fprint_sym (FILE *ofile, t_addr __attribute__((unused)) addr, t_value *val, UNIT *uptr, int32 sw)
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
t_stat parse_sym (char * __attribute__((unused)) cptr, t_addr __attribute__((unused))addr, UNIT * __attribute__((unused)) uptr, t_value * __attribute__((unused)) val, int32 __attribute__((unused)) sswitch)
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

static t_stat sys_show_config (FILE * __attribute__((unused)) st, UNIT * __attribute__((unused)) uptr, int __attribute__((unused)) val, void * __attribute__((unused)) desc)
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
    /*  0 */ { "connect_time", -1, 100000, cfg_timing_list }, // set sim_activate timing
    /*  1 */ { "activate_time", -1, 100000, cfg_timing_list }, // set sim_activate timing
    /*  2 */ { "mt_read_time", -1, 100000, cfg_timing_list }, // set sim_activate timing
    /*  3 */ { "mt_xfer_time", -1, 100000, cfg_timing_list }, // set sim_activate timing
    /*  4 */ { "iom_boot_time", -1, 100000, cfg_timing_list }, // set sim_activate timing
 };

static t_stat sys_set_config (UNIT * __attribute__((unused)) uptr, int32 __attribute__((unused)) value, char * cptr, void * __attribute__((unused)) desc)
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



static t_stat sys_reset (DEVICE __attribute__((unused)) * dptr)
  {
    return SCPE_OK;
  }

static DEVICE sys_dev = {
    (char *) "SYS",       /* name */
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
    & disk_dev,
    & scu_dev,
    & clk_dev,
    // & mpc_dev,
    & opcon_dev,
//    & disk_dev, // Not hooked up yet
    & sys_dev,
    & fxe_dev,
    NULL
};

// This is simh's sim_gtime, but returns total_cycles instead of sim_time.
t_uint64 sim_ctime (void)
  {
    return sys_stats . total_cycles;
  }

