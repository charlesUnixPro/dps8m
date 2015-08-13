// Hit list:
//
//   Allocate segments by size.
//   Implement paging.
//   Add a stack dumper.
//   Understand control points.
//   Understand run units, run unit depth.
//   Implement signaling. (See AK92-2, page 184)
//   Move RNT into CLR?
//   fxeshell
//
// Hot items.
//   Implement sys_link_info_ptr area.
//   Implement combined_stat_ptr.
//   Implement clr_ptr.
//   Implement user_storage_ptr.
//   Implement rnt_ptr.
//   
// Medium items.
//    Research trans_op_tv_ptr, sct_ptrunwinder_ptr, ect_ptr, assign_linkage_ptr.
//
// system free area notes:
//
// MR12.3/documentation/info_segments/get_system_free_area_.info.ascii
// :Entry: get_system_free_area_: 02/02/83  get_system_free_area_
//
//
// Function: returns a pointer to the system free area for the ring in
// which it was called.  Allocations by system programs are performed in
// this area.
//
//
// Syntax:
// declare get_system_free_area_ entry returns (ptr);
// area_ptr = get_system_free_area_ ();
//
// Arguments:
// area_ptr
//    is a pointer to the system free area.  (Output)
//
// MR12.3/documentation/info_segments/allocation_storage.gi.info.ascii
//
// bound_file_system.s.archive
//   get_initial_linkage: This entry is called only by makestack when a new 
//                        ring is being created. The program makestack
//                        may have been called by link_man.
//
// dcl  1 ainfo aligned like area_info;
//
// /* set up linkage section area */
// 
//           if pds$clr_stack_size (aring) > 0 then do;        /* initial area is in stack */
//                ainfo.size = pds$clr_stack_size (aring);
//                ainfo.areap = ptr (sp, stack_end);
//                stack_end = stack_end + ainfo.size;          /* update length of stack */
//                stack_end = divide (stack_end + 15, 16, 17, 0) * 16;
//                                                             /* round up */
//                end;
//           else do;                                          /* clr is to go into separate seg */
//                ainfo.size = sys_info$max_seg_size;
//                ainfo.areap = null;
//                end;
//           ainfo.version = area_info_version_1;
//           string (ainfo.control) = "0"b;
//           ainfo.control.extend = "1"b;
//           ainfo.control.zero_on_free = "1"b;
//           ainfo.control.system = "1"b;
//           ainfo.owner = "linker";
//           call define_area_ (addr (ainfo), code);
//           if code ^= 0 then call terminate_proc (error_table_$termination_requested);
// 
//      sp -> stack_header.system_free_ptr, 
//      sp -> stack_header.user_free_ptr, 
//      sp -> stack_header.assign_linkage_ptr,
//      sp -> stack_header.clr_ptr, 
//      sp -> stack_header.combined_stat_ptr = ainfo.areap;
//


//  define_area_ is in bound_library_1.s.archive; it builds area_header from
//  area_info


// makestack is called by act_proc.
// create_proc is called (somehow) by cpg_ create process group
// cpg_ is called by dialup_, absentee_user_manager, and
//   daemon_user_manager_ to create user processes.


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_sys.h"
#include "dps8_cpu.h"
#include "dps8_append.h"
#include "dps8_utils.h"
#include "dps8_ins.h"
#include "dps8_fxe.h"

#include "object_map.incl.pl1.h"
#include "object_glop.incl.pl1.h"
#include "definition_dcls.incl.pl1.h"
#include "linkdcl.incl.pl1.h"
#include "symbol_block.incl.pl1.h"
#include "source_map.incl.pl1.h"
#include "pl1_symbol_block.incl.pl1.h"
#include "iocbx.incl.pl1.h"
#include "stack_header.incl.pl1.h"
#include "system_link_names.incl.pl1.h"
#include "area_info.incl.pl1.h"
#include "std_symbol_header.incl.pl1.h"
#include "bind_map.incl.pl1.h"
//#include "area_structures.incl.pl1.h"
#include "area_header_v2pl1.incl.pl1.h"
#include "aim_template.incl.pl1.h"
#include "status_structures.incl.pl1.h"
#include "acl_structures.incl.pl1.h"


//
// Configuration constants
//

// Made up numbers

#define RUN_UNIT_DEPTH 1

// keeping it simple: segments are loaded unpaged, and a full 128K space is
// allocated for them.

// SEGSIZE is 2^18
// MEMSIZE is the emulator's allocated memory size

// How many segments can we deal with?

// This is 64...
#define N_SEGS ((MEMSIZE + SEGSIZE - 1) / SEGSIZE)



//
// Static data
//

static bool FXEinitialized = false;

// Remember where important segments are

typedef int sgIdx; // -1 .. N_SEGS - 1
#define NOSGIDX -1

static sgIdx libIdx;
static sgIdx errIdx;
static sgIdx slIdx;
static sgIdx ssaIdx;
static sgIdx clrIdx;
static sgIdx dsegIdx;

//
// Forward declarations
//

static int loadSegmentFromFile (char * arg);
static void trapFXE_UnhandledSignal (void);
static int loadSegment (char * arg);

//
// Utility routines
//   printACC - print an ACC
//   sprintACC - convert an ACC to a C string
//   accCmp - strcmp a ACC to a C string
//   packedPtr - create a packed pointer
//   makeITS - create an ITS pointer
//   makeNullPtr - equivalent to Multics PL/I null()

#define SEGNAME_LEN 32
#define PATHNAME_LEN 168

typedef char segnameBuf [SEGNAME_LEN + 1];
typedef char pathnameBuf [PATHNAME_LEN + 1];

static void cpACC2buf (segnameBuf buf, word36 * p)
  {
    word9 cnt = (word9) (getbits36 (* p, 0, 9) & MASK9);
    if (cnt > SEGNAME_LEN)
      cnt = SEGNAME_LEN;
    for (uint i = 0; i < cnt; i ++)
      {
        uint woffset = (i + 1) / 4;
        uint coffset = (i + 1) % 4;
        word36 ch = getbits36 (* (p + woffset), coffset * 9, 9);
        buf [i] = (ch & 0177);
      }
    for (uint i = cnt; i < SEGNAME_LEN; i ++)
      buf [i] = ' ';
    buf [SEGNAME_LEN] = '\0';
  }

UNUSED static void printACC (word36 * p)
  {
    word36 cnt = getbits36 (* p, 0, 9);
    for (uint i = 0; i < cnt; i ++)
      {
        uint woffset = (i + 1) / 4;
        uint coffset = (i + 1) % 4;
        word36 ch = getbits36 (* (p + woffset), coffset * 9, 9);
        sim_printf ("%c", (char) (ch & 0177));
      }
  }

#if 0
static void printNChars (word36 * p, word24 cnt)
  {
    for (uint i = 0; i < cnt; i ++)
      {
        uint woffset = i / 4;
        uint coffset = i % 4;
        word36 ch = getbits36 (* (p + woffset), coffset * 9, 9);
        sim_printf ("%c", (char) (ch & 0177));
      }
  }
#endif

static char * sprintNChars (word36 * p, word24 cnt)
  {
    static char buf [257];
    char * bp = buf;
    for (uint i = 0; i < cnt; i ++)
      {
        uint woffset = i / 4;
        uint coffset = i % 4;
        word36 ch = getbits36 (* (p + woffset), coffset * 9, 9);
        * bp ++ = (char) (ch & 0177);
      }
    * bp ++ = '\0';
    return buf;
  }

static char * sprintACC (word36 * p)
  {
    static char buf [257];
    char * bp = buf;
    word36 cnt = getbits36 (* p, 0, 9);
    for (uint i = 0; i < cnt; i ++)
      {
        uint woffset = (i + 1) / 4;
        uint coffset = (i + 1) % 4;
        word36 ch = getbits36 (* (p + woffset), coffset * 9, 9);
        //sim_printf ("%c", (char) (ch & 0177));
        * bp ++ = (char) (ch & 0177);
      }
    * bp ++ = '\0';
    return buf;
  }

static int accCmp (word36 * acc, char * str)
  {
    //sim_printf ("accCmp <");
    //printACC (acc);
    //sim_printf ("> <%s>\n", str);
    word36 cnt = getbits36 (* acc, 0, 9);
    if (cnt != strlen (str))
      return 0;
    for (uint i = 0; i < cnt; i ++)
      {
        uint woffset = (i + 1) / 4;
        uint coffset = (i + 1) % 4;
        word36 ch = getbits36 (* (acc + woffset), coffset * 9, 9);
        if ((char) (ch & 0177) != str [i])
          return 0;
      }
    return 1;
  }
   
static void trimTrailingSpaces (char * str)
  {
    char * end = str + strlen(str) - 1;
    while (end > str && isspace (* end))
      end --;
    * (end + 1) = 0;
  }

static word36 packedPtr (word6 bitno, word12 shortSegNo, word18 wordno)
  {
    word36 p = 0;
    putbits36 (& p,  0,  6, bitno);
    putbits36 (& p,  6, 12, shortSegNo);
    putbits36 (& p, 18, 18, wordno);
    return p;
  }

static void makeITS (word36 * memory, word15 snr, word3 rnr,
                     word18 wordno, word6 bitno, word6 tag)
  {
      //sim_printf ("makeITS %08lo snr %05o wordno %06o\n", memory - M, snr, wordno);

      // even
      memory [0] = 0;
      putbits36 (memory + 0,  3, 15, snr);
      putbits36 (memory + 0, 18,  3, rnr);
      putbits36 (memory + 0, 30,  6, 043);

      // odd
      memory [1] = 1;
      putbits36 (memory + 1,  0, 18, wordno);
      putbits36 (memory + 1, 21,  6, bitno);
      putbits36 (memory + 1, 30,  6, tag);
  }

// AK92-2 pg 7-42.1 "A null pointer is represented by the virtual
// pointer 77777|1, by -1|1, or by -1.

static void makeNullPtr (word36 * memory)
  {
    //makeITS (memory, 077777, 0, 0777777, 0, 0);

    // The example null ptr I found was in http://www.multicians.org/pxss.html;
    // it has a tag of 0 instead of 43;
    //putbits36 (memory + 0, 30,  6, 0);

    makeITS (memory, 077777, 0, 1, 0, 0);
  }

static bool isNullPtr (word24 physaddr)
  {
    return getbits36 (M [physaddr], 3, 15) == 077777LLU;
  }

static void strcpyVarying (word24 base, word18 * next, char * str)
  {
    size_t len = strlen (str);
    word18 offset = * next;
    // store the lengtn in the first word
    M [base + offset] = len;
    //sim_printf ("len %3d %012llo\n", offset, M [base + offset]);
    offset ++;

    for (uint i = 0; i < len; i ++)
      {
        uint woff = i / 4;
        uint choff = i % 4;
        if (choff == 0)
          M [base + offset + woff] = 0;
        putbits36 (M + base + offset + woff, choff * 9, 9, (word36) str [i]);
        //sim_printf ("chn %3d %012llo\n", offset + woff, M [base + offset + woff]);
      }
    offset += (len + 3) / 4;
    * next = offset;
  }

static void strcpyNonVarying (word24 base, word18 * next, char * str)
  {
    size_t len = strlen (str);
    word18 offset;
    if (next)
      offset = * next;
    else
      offset = 0;
    for (uint i = 0; i < len; i ++)
      {
        uint woff = i / 4;
        uint choff = i % 4;
        if (choff == 0)
          M [base + offset + woff] = 0;
        putbits36 (M + base + offset + woff, choff * 9, 9, (word36) str [i]);
        //sim_printf ("chn %3d %012llo\n", offset + woff, M [base + offset + woff]);
      }
    offset += (len + 3) / 4;
    if (next)
      * next = offset;
  }

static void strcpyNonVaryingPad (word24 base, word18 * next, char * str, uint dstlen)
  {
    size_t len = strlen (str);
    word18 offset;
    if (next)
      offset = * next;
    else
      offset = 0;
    for (uint i = 0; i < dstlen; i ++)
      {
        uint woff = i / 4;
        uint choff = i % 4;
        if (choff == 0)
          M [base + offset + woff] = 0;
        if (i >= len)
          {
            putbits36 (M + base + offset + woff, choff * 9, 9, (word36) ' ');
          }
        else
          {
            putbits36 (M + base + offset + woff, choff * 9, 9, (word36) str [i]);
          }
        //sim_printf ("chn %3d %012llo\n", offset + woff, M [base + offset + woff]);
      }
    offset += (len + 3) / 4;
    if (next)
      * next = offset;
  }

// Copy multics string to C

static void strcpyC (word24 addr, word24 len, char * str)
  {
    for (uint i = 0; i < len; i ++)
      {
        uint woff = i / 4;
        uint choff = i % 4;
        word36 ch = getbits36 (M [addr + woff], choff * 9, 9);
        * (str ++) = (char) (ch & 0177);
        //sim_printf ("chn %3d %012llo\n", woff, M [addr + woff]);
      }
    * (str ++) = '\0';
  }

// Convert bit count to word count, rounding up

#define nbits2nwords(nbits) (((nbits) + 35u) / 36u)

//
// SLTE
//

//
// A running Multics system has a SLTE table which lists
// the segments loaded from the boot tape, and the segment numbers
// and other attributes assigned to them by the generate_multics
// process.
//
// The system book lists these values, but it finds them by parsing
// the boot tape. Rather than parsing the boot tape, extract the needed 
// values from the system book. (./buildSLTE.sh reads the system book
// and generates "slte.inc"
//

// The Multics SLTE table

typedef struct SLTEentry
  {
    char * segname;
    word15 segno; // 0 is invalid sentinal
    word1 R, E, W, P;
    word3 R1, R2, R3;
    char * path;
  } SLTEentry;

static SLTEentry SLTE [] =
  {
#include "slte.inc"
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, NULL }
  };

// getSLTEidx - lookup a segment name in the SLTE 

static sgIdx getSLTEidx (char * name)
  {
    //sim_printf ("getSLTEidx %s\n", name);
    for (sgIdx idx = 0; SLTE [idx] . segname; idx ++)
      {
        if (strcmp (SLTE [idx] . segname, name) == 0)
          return idx;
      }
    //sim_printf ("ERROR: %s not in SLTE\n", name);
    return -1;
  }

// getSegnoFromSLTE - get the segment number of a segment from the SLTE
// 0 is not found
static word15 getSegnoFromSLTE (char * name, sgIdx * slteIdx)
  {
    sgIdx idx = getSLTEidx (name);
    if (idx < 0)
      return 0;
    * slteIdx = idx;
    return SLTE  [idx] . segno;
  }

//
// KST: the list of known segments
//

// lookupSegAddrByIdx - return the physical memory address of a segment
// allocateSegment - find an unallocated entry in segtable, mark it allocated
//                   and allocate physical memory for it.
// allocateSegno - Allocate a segment number, starting at USER_SEGNO
//
// segno usage:
//          0  dseg
//      1-477  Multics
//  0500-0507  stacks
//       0510  iocbs
//       0511  combined linkage segment
//       0512  ring 5 system free ares
//       0513  system storage area
//       0577  fxe
//  
//  0600-0777  user 
//     077776  fxe trap       

#define N_SEGNOS 01000

#define DSEG_SEGNO 0
#define STACKS_SEGNO 0500
#define IOCB_SEGNO 0510
#define CLR_SEGNO 0511
#define SSA_SEGNO 0513
#define FXE_SEGNO 0557
#define USER_SEGNO 0600
#define TRAP_SEGNO 077776

#define MAX_SEGLEN 01000000

#define FXE_RING 5

#define IOCB_USER_INPUT 0
#define IOCB_USER_OUTPUT 1
#define IOCB_ERROR_OUTPUT 2
#define IOCB_USER_IO 3
#define N_IOCBS 4

//
// pathname mangling
//

// '>' (root) <-->  ./MR12.3/
// foo>       <-->  foo/

#define UNIX_PATHNAME_LEN (PATHNAME_LEN + 16) // space for the "./MR12_3
#define MROOT "./MR12.3/"

#define DEF_CWD ">udd>fxe"
static pathnameBuf cwd = DEF_CWD;



typedef struct KSTEntry
  {
    bool allocated;
    bool loaded;
    word15 segno;  // Multics segno
    word36 uid;

    bool wired;
    word3 R1, R2, R3;
    word1 R, E, W, P;

    segnameBuf segname;
    pathnameBuf pathname;
    // This the segment length in words; which means that it's maximum
    // value is 1<<18, which does not fit in a word18. See comments in
    // installSDW.
    word24 seglen;
    word24 allocatedLength;
    word24 physmem;
    word24 bit_count;

    bool explicit_deactivate_ok;

// Cache values discovered during parseSegment

    bool parsed; // parsing successfull

    word18 entry;
    segnameBuf entryName;
    bool gated;
    word18 entry_bound;
    word18 definition_offset;
    word18 linkage_offset;
    word18 linkage_length;
    word18 isot_offset;
    word18 symbol_offset;
    word36 * segnoPadAddr;

// RNT

    int RNTRefCount;
  } KSTEntry;



static KSTEntry KST [N_SEGS];

// Map segno to KSTIdx

static int segnoMap [N_SEGNOS];

//
// PDS: process data segment
//

static uint pdsValidationLevel = FXE_RING;

//
// CLR - Multics segment containing the LOT and ISOT
//

#define LOT_SIZE 0100000 // 2^12
#define LOT_OFFSET 0 // LOT starts at word 0
#define ISOT_OFFSET LOT_SIZE // ISOT follows LOT
//#define CLR_SIZE (LOT_SIZE * 2) // LOT + ISOT
#define CLR_FREE_START (LOT_SIZE * 2) // LOT + ISOT

// installLOT - install a segment LOT and ISOT in the SLT

static word18 clrFreePtr = CLR_FREE_START;

static void installLOT (int idx)
  {
    KSTEntry * segEntry = KST + idx;
    KSTEntry * clrEntry = KST + clrIdx;

    word36 * clrMemory = M + clrEntry -> physmem;
    word18 newLinkage = clrFreePtr;
    // Copy the linkage section to the CLR
    word36 * segMemory = M + segEntry -> physmem;
    word18 segLinkageOffset = segEntry -> linkage_offset;
    //sim_printf ("orig link @ %05o:%06o\n", segEntry -> segno, segEntry -> linkage_offset);
    word18 segLinkageLength = segEntry -> linkage_length;
    if (segLinkageLength & 1)
      segLinkageLength ++;
    word36 * from = segMemory + segLinkageOffset;
    word36 * to = clrMemory + clrFreePtr;
    for (uint i = 0; i < segLinkageLength; i ++)
      * (to ++) = * (from ++);
    clrFreePtr += segLinkageLength;

    link_header * lhp = (link_header *) (clrMemory + newLinkage);
    //sim_printf ("link header %08o\n", (word24) ((word36 *) lhp - M));
    makeITS (lhp -> original_linkage_ptr, segEntry -> segno, segEntry -> R1,
             segEntry -> linkage_offset, 0, 0);
    //clrMemory [LOT_OFFSET + KST [idx] . segno] = 
    //  packedPtr (0, KST [idx] . segno,
    //             KST [idx] . linkage_offset);
    //sim_printf ("newLinkage: %05o %06o-%06o\n", KST [idx] . segno, newLinkage, clrFreePtr - 1);
    clrMemory [LOT_OFFSET + KST [idx] . segno] = 
      packedPtr (0, KST [clrIdx] . segno,
                 newLinkage);

    clrMemory [ISOT_OFFSET + KST [idx] . segno] = 
      packedPtr (0, KST [idx] . segno,
                 KST [idx] . isot_offset);
  }

//
// KST routines
//

static sgIdx lookupSegname (char * name)
  {
    for (uint i = 0; i < N_SEGS; i ++)
      if (strcmp (name, KST [i] . segname) == 0)
        return (int) i;
    return -1;
  }

static word24 lookupSegAddrByIdx (sgIdx segIdx)
  {
    return KST [segIdx] . physmem;
  }

static void setSegno (sgIdx idx, word15 segno)
  {
    KST [idx] . segno = segno;
    segnoMap [segno] = idx;
  }

static word24 nextPhysmem = 0; 

#define RINGS_FFF FXE_RING, FXE_RING, FXE_RING
#define RINGS_ZFF 0, FXE_RING, FXE_RING
#define P_REW 1, 1, 1, 0
#define P_RW  1, 0, 1, 0

static word36 nextUID = 0;

// segno -- if 0, allocate segno

static sgIdx allocateSegment (uint seglen, char * segname, word15 segno,
                              word3 R1, word3 R2, word3 R3, 
                              word1 R, word1 E, word1 W, word1 P)
  {
#ifndef SPEED
    if_sim_debug (DBG_TRACE, & fxe_dev)
      sim_printf ("allocate segment len %u name %s\n", seglen, segname);
#endif
    // Round seglen up to next page boundary
    uint seglenpage = (seglen + 1023u) & ~1023u;
    for (int i = 0; i < (int) N_SEGS; i ++)
      {
        KSTEntry * e = KST + i;
        if (! e -> allocated)
          {
            e -> allocated = true;
            e -> physmem = nextPhysmem;
            nextPhysmem += seglenpage;
            e -> seglen = seglen;
            e -> allocatedLength = seglen;
            e -> bit_count = 36 * seglen;
            e -> R1 = R1;
            e -> R2 = R2;
            e -> R3 = R3;
            e -> R = R;
            e -> E = E;
            e -> W = W;
            e -> P = P;
            if (segno)
              setSegno (i, segno);
            strncpy (e -> segname, segname, SEGNAME_LEN + 1);
            e -> uid = nextUID ++;
            e -> parsed = false;
            return i;
          }
      }
    return -1;
  }

static word15 nextSegno = USER_SEGNO;

static word15 allocateSegno (void)
  {
    return nextSegno ++;
  }

static word24 ITSToPhysmem (word36 * its, word6 * bitno)
  {
    word36 even = * its;
    word36 odd = * (its + 1);

    word15 segno = getbits15 (even, 3);
    word18 wordno = (word18) getbits36 (odd, 0, 18);
    if (bitno)
      * bitno = getbits6 (odd, 57 - 36);

    word24 physmem = KST [segnoMap [segno]] . physmem  + wordno;
    return physmem;
  }


// RNT Process Reference Name Table

typedef struct RNTEntry
  {
    char * refName;
    sgIdx idx;
  } RNTEntry;
static RNTEntry RNT [N_SEGS]; // XXX Actually a segment can have many references; N_SEGS is not the right #
#define RNT_TABLE_SIZE (sizeof (RNT) / sizeof (RNTEntry))

static int searchRNT (char * name)
  {
    for (uint i = 0; i < RNT_TABLE_SIZE; i ++)
      if (RNT [i] . refName && strcmp (name, RNT [i] . refName) == 0)
        return (int) i;
    return -1;
  }

static void addRNTRef (sgIdx idx, char * name)
  {
    int i = searchRNT (name);
    if (i >= 0)
      {
        KST [RNT [i] . idx] . RNTRefCount ++;
        return;
      }
    for (uint i = 0; i < RNT_TABLE_SIZE; i ++)
      {
        if (! RNT [i] . refName)
          {
            RNT [i] . refName = strdup (name);
            RNT [i] . idx = idx;
            KST [idx] . RNTRefCount = 1;
            return;
          }
      }
    sim_printf ("ERROR: RNT full\n");
  }

static void delRNTRef (char * name)
  {
    sgIdx i = searchRNT (name);
    if (i >= 0)
      {
        if (KST [RNT [i] . idx] . RNTRefCount)
          KST [RNT [i] . idx] . RNTRefCount --;
        free (RNT [i] . refName);
        RNT [i] . refName = NULL;
        RNT [i] . idx = 0;
// XXX make segment unknown if RNTRefCount became 0;
        return;
      }
    //sim_printf ("ERROR: delRNTRef couldn't find %s\n", name);
  }



static void initializeDSEG (void)
  {

    // 0100 (64) - 0177 (127) Fault pairs
    // 0200 (128) - 0207 (135) SCU y8block
    // 0300 - 2377 descriptor segment: 1000 segments at 2 words per segment.
#define DESCSEG 0300
#define N_DESCS N_SEGNOS

#define DSEG_SIZE (DESCSEG + N_DESCS * 2)

    //    org   0100 " Fault pairs
    //    bss   64
    //    org   0200
    //    bss   8
    //    org   0300
    //    bss   0400*2


    dsegIdx = allocateSegment (DSEG_SIZE, "dseg", 0, 0, 0, 0, 0, 0, 0, 0);
    // Fill the fault pairs with fxeFaultHandler traps.

    // (12-bits of which the top-most 7-bits are used)
    int fltAddress = (switches . FLT_BASE << 5) & 07740;

    for (int i = 0; i < N_FAULTS; i ++)
      {
        M [fltAddress + i * 2 + 0] = 0000200657000;  // 'SCU 200' instruction.
        M [fltAddress + i * 2 + 1] = 0000000421400;  // FXE instruction.
      }

    // Fill the descriptor segment with SDWs
    int descAddress = DESCSEG;
    for (int i = 0; i < (int) N_DESCS; i ++)
      {
        //word24 segAddr = lookupSegAddrByIdx (i);
        word24 segAddr = 0;

        // even
        //   ADDR: memory address for segment
        //   R1, R2, R3: 0
        //   F: 0 - page is non-resident
        //   FC: 0 - fault code
        word36 even = ((word36) segAddr) << 12;
        M [descAddress + i * 2 + 0] = even;

        // odd
        //  BOUND: 0
        //  R,E,W,P: 0
        //  U: 1
        //  G,C: 0
        //  EB: 0
        M [descAddress + i * 2 + 1] = 0000000200000;
      }
    DSBR . ADDR = DESCSEG;
    DSBR . BND = N_DESCS / 8;
    DSBR . U = 1;
    // stack segno not yet assigned
    //DSBR . STACK = KST [STK_SEG + 5] . segno >> 3;
  }


#if 0
static void printLineNumbers (int segIdx)
  {
    sim_printf ("Line numbers:\n");
    KSTEntry * e = KST + segIdx;
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word36 * segp = M + segAddr;
    symbol_block * sbh = (symbol_block *) (segp + e -> symbol_offset);
//sim_printf ("e -> symbol_offset %06o\n", e -> symbol_offset);    
    //printNChars (sbh -> identifier, 8);
    while (sbh)
      {
#if 0
        //sim_printf ("source_map %06o\n", sbh -> source_map);    
        //source_map * sm = (source_map *) (segp + sbh -> source_map + e -> symbol_offset);
        source_map * sm = (source_map *) (((word36 *) sbh) + sbh -> source_map);
        //sim_printf ("sm version %llu number %llu\n", sm -> version, sm -> number);

        map * mp = & sm -> firstMap;
        for (uint i = 0; i < sm -> number; i ++)
          {
            //sim_printf ("os %06o sz %06o uid %012llo dtm %012llo %012llo\n",
              //mp [i] . pathname . offset, mp [i] . pathname . size,
              //mp [i] . uid, mp [i] . dtm [0], mp [i] . dtm [1]);
            word36 * pn = (((word36 *) sbh) + mp [i] . pathname . offset);
            printNChars (pn, mp [i] . pathname . size);
            sim_printf ("\n");
          }
#endif

        //sim_printf ("area %06o\n", sbh -> area_pointer);
        if (sbh -> area_pointer)
          {
            pl1_symbol_block * psb = 
             (pl1_symbol_block *) (((word36 *) sbh) + sbh -> area_pointer);
            if (psb -> identifier [0] == 0160154061151llu &&
                psb -> identifier [1] == 0156146157040llu) // "pl1info"
              {
                // printNChars (psb -> identifier, 8); sim_printf ("\n");
                if (psb -> flags . map)
                  sim_printf ("has map\n");
              }
          }
        //sim_printf ("next_block %06o\n", sbh -> next_block);
        if (sbh -> next_block)
          sbh = (symbol_block *) (segp + e -> symbol_offset + sbh -> next_block);
        else
          sbh = NULL;
     }
  }
#endif

static int lookupOffset (sgIdx segIdx, word18 offset, 
                         char * * compname, word18 * compoffset)
  {
    if (segIdx < 0)
      return 0;
    KSTEntry * e = KST + segIdx;
    if (! e -> parsed)
      return 0;
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word36 * segp = M + segAddr;
    def_header * oip_defp = (def_header *) (segp + e -> definition_offset);

    word36 * defBase = (word36 *) oip_defp;

    definition * p = (definition *) (defBase +
                                     oip_defp -> def_list_relp);
    // Search for the bindmap

    //definition * symDef = NULL;
    word18 value = 0;
    while (* (word36 *) p)
      {
        if (p -> ignore != 0)
          goto next;
        if (p -> class != 2)  // Not a symbol section reference
          goto next;
        if (accCmp (defBase + p -> symbol, "bind_map"))
          {
            //sim_printf ("hit\n");
            break;
          }
next:;
        p = (definition *) (defBase + p -> forward);
      }
    if (! (* (word36 *) p))
      {
        //sim_printf ("can't find bind_map\n");
        return 0;
      }

    //sim_printf ("bind map at %o\n", value);

    std_symbol_header * sshp = (std_symbol_header *) (segp + e -> symbol_offset + value);
    //sim_printf ("sshp identifier ");
    //printNChars (& sshp -> identifier [0], 8);
    //sim_printf ("\n");

    //sim_printf ("source_map offset %u\n", sshp -> source_map);
    //source_map * smp = (source_map *) (segp + e -> symbol_offset + sshp -> source_map);
    //sim_printf ("source map number %llu\n", smp -> number);

    //map * mp = & smp -> firstMap;
    //for (uint i = 0; i < smp -> number; i ++)
      //{
        //sim_printf ("%3d ", i);
        //printNChars (segp + e -> symbol_offset + mp [i] . offset, mp [i] . size);
        //sim_printf ("\n");
      //}

    //sim_printf ("bind_map offset %u\n", sshp -> area_pointer);

    bindmap * bmp = (bindmap *) (segp + e -> symbol_offset + sshp -> area_pointer);
    //sim_printf ("n_components %lld\n", bmp -> n_components);
    component * cp = & bmp -> firstComponent;
    for (uint i = 0; i < bmp -> n_components; i ++)
      {
        //sim_printf ("%3d ", i);
        //printNChars (segp + e -> symbol_offset + cp [i] . name_ptr, cp [i] . name_len);
        //sim_printf (" %6o %6o\n", cp [i] . text_start, cp [i] . text_lng);
        if (offset >= cp [i] . text_start && 
            offset < cp [i] . text_start + cp [i] . text_lng)
          {
            char * cname =
              sprintNChars (segp + e -> symbol_offset + cp [i] . name_ptr, cp [i] . name_len);
            if (compname)
              * compname = cname;
            if (compoffset)
              * compoffset = offset - cp [i] . text_start;
            return 1;
          }
      }
    return 0;
  }

static int lookupDef (sgIdx segIdx, char * segName, char * symbolName, word18 * value)
  {
    if (segIdx < 0)
      return 0;
    KSTEntry * e = KST + segIdx;
    if (! e -> parsed)
      {
         if (! symbolName && strcmp (segName, e -> segname) == 0)
           {
             * value = 0;
             return 1;
           }
         return 0;
      }
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word36 * segp = M + segAddr;
    def_header * oip_defp = (def_header *) (segp + e -> definition_offset);

    word36 * defBase = (word36 *) oip_defp;

    definition * p = (definition *) (defBase +
                                     oip_defp -> def_list_relp);
    // Search for the segment

    //definition * symDef = NULL;

    while (* (word36 *) p)
      {
        if (p -> ignore != 0)
          goto next;
        if (p -> class != 3)  // Not segment name?
          goto next;
        if (accCmp (defBase + p -> symbol, segName))
          {
            //sim_printf ("hit\n");
            break;
          }
next:
        p = (definition *) (defBase + p -> forward);
      }
    if (! (* (word36 *) p))
      {
        //sim_printf ("can't find segment name %s\n", segName);
        return 0;
      }

    // A null symbol name means we want the base of the segment
    if (! symbolName)
      {
        //symDef = p;
        //sim_printf ("hit2\n");
        * value =  0;
        return 1;
      }

    // Goto list of symbols defined in this segment
    p = (definition *) (defBase + p -> segname);

    while (* (word36 *) p)
      {
        if (p -> ignore != 0)
          goto next2;
        if (p -> class == 3)  // Segment name marks the end of the 
                              // list of symbols is the segment
          break;

        if (accCmp (defBase + p -> symbol, symbolName))
          {
            //symDef = p;
            //sim_printf ("hit2\n");
            * value =  p -> value;
            return 1;
          }
next2:
        p = (definition *) (defBase + p -> forward);
      }
    //sim_printf ("can't find symbol name %s\n", symbolName);
    return 0;
  }

static int lookupEntry (sgIdx segIdx, char * entryName, word18 * value)
  {
    KSTEntry * e = KST + segIdx;
    if (! e -> parsed)
      return 0;
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word36 * segp = M + segAddr;
    def_header * oip_defp = (def_header *) (segp + e -> definition_offset);

    word36 * defBase = (word36 *) oip_defp;

    definition * p = (definition *) (defBase +
                                     oip_defp -> def_list_relp);
    // Search for the segment

    //definition * symDef = NULL;

    while (* (word36 *) p)
      {
//sim_printf ("ignore %d class %d %s\n", p -> ignore, p -> class, sprintACC (defBase + p -> symbol));
        if (p -> ignore != 0)
          goto next;
        if (p -> class == 3)  // Segment name?
          {
            if (accCmp (defBase + p -> symbol, entryName))
              {
                //sim_printf ("hit segname\n");
                break;
              }
          }
        else if (p -> class == 0 && p -> entry) // Text section entry point?
          {
            //sim_printf ("text section %d <", p -> entry);
            //printACC (defBase + p -> symbol);
            //sim_printf (">\n");
            if (accCmp (defBase + p -> symbol, entryName))
              {
                //sim_printf ("hit entryname\n");
                * value =  p -> value;
                return 1;
              }
          }
next:
        p = (definition *) (defBase + p -> forward);
      }
    if (! (* (word36 *) p))
      {
        //sim_printf ("can't find segment name %s\n", entryName);
        return 0;
      }

    // Goto list of symbols defined in this segment
    p = (definition *) (defBase + p -> segname);

    while (* (word36 *) p)
      {
        if (p -> ignore != 0)
          goto next2;
        if (p -> class == 3)  // Segment name marks the end of the 
                              // list of symbols is the segment
          break;

        if (accCmp (defBase + p -> symbol, entryName))
          {
            //symDef = p;
            //sim_printf ("hit2\n");
            * value =  p -> value;
            return 1;
          }
next2:
        p = (definition *) (defBase + p -> forward);
      }
    //sim_printf ("can't find symbol name %s\n", entryName);
    return 0;
  }

static void installSDW (int segIdx)
  {
    KSTEntry * e = KST + segIdx;

    if (e -> seglen == 0)
      {
        sim_printf ("ERROR: Tried to install zero length segment (idx %d)\n",
                    segIdx);
        return;
     }
    word15 segno = e -> segno;
    // sim_printf ("install idx %d segno %o (%s) @ %08o len %d bound %o\n", segIdx, segno, e -> segname, e -> physmem, e -> seglen, (e -> seglen - 1) >> 4);
    word36 * even = M + DESCSEG + 2 * segno + 0;  
    word36 * odd  = M + DESCSEG + 2 * segno + 1;  

    putbits36 (even,  0, 24, e -> physmem);
    putbits36 (even, 24,  3, e -> R1);
    putbits36 (even, 27,  3, e -> R2);
    putbits36 (even, 30,  3, e -> R3);

    putbits36 (even, 33,  1, 1); // F: mark page as resident

    // seglen is of type word18 and is the length in words which has a max 
    // value of 1<<18, which will not fit in a word18. Word18 won't
    // hold that value, but since it is implemented as a 32 bit word,
    // there is no data loss. SDW.BOUND is interpreted (I think) with
    // a 0 value meaning that the first 16 words are accessable, and
    // a 777777 means the entire segment is.
    // This code does not cope with a seglen of 0.
    //   seglen    -1      >>4
    //        1       0      0
    //       17      16      0
    //       20      17      1
    //  1000000  777777  37777
    putbits36 (odd,   1, 14, (e -> seglen - 1) >> 4); // BOUND
    putbits36 (odd,  15,  1, e -> R);
    putbits36 (odd,  16,  1, e -> E);
    putbits36 (odd,  17,  1, e -> W);
    putbits36 (odd,  18,  1, e -> P);
    putbits36 (odd,  19,  1, 1); // unpaged
    putbits36 (odd,  20,  1, e -> gated ? 0U : 1U);
    //putbits36 (odd,  22, 14, e -> entry_bound >> 4);
    putbits36 (odd,  22, 14, e -> entry_bound);
//if (segno == 0161) sim_printf ("gated %d entry_bound %d\n", e -> gated, e -> entry_bound);
    do_camp (TPR . CA);
    do_cams (TPR . CA);
  }

typedef struct trapNameTableEntry
  {
    char * segName;
    char * symbolName;
    void (* trapFunc) (void);
  } trapNameTableEntry;

static void trapHCS_HistoryRegsSet (void);
static void trapHCS_HistoryRegsGet (void);
static void trapHCS_FSSearchGetWdir (void);
static void trapHCS_InitiateCount (void);
static void trapHCS_Initiate (void);
static void trapHCS_TerminateName (void);
static void trapHCS_MakePtr (void);
static void trapHCS_StatusMins (void);
static void trapHCS_MakeSeg (void);
static void trapHCS_SetBcSeg (void);
static void trapHCS_HighLowSegCount (void);
static void trapHCS_FSGetMode (void);
static void trapHCS_TerminateNoname (void);
static void trapPHCS_SetKSTAttributes (void);
static void trapPHCS_Deactivate (void);
static void trapGetLineLengthSwitch (void);
static void trapGetSegPtr (void);
//static void trapGetSegment (void);
static void trapGetType (void);
static void trapHomedir (void);
static void trapFXE_UnhandledSignal (void);
static void trapFXE_ReturnToFXE (void);
static void trapFXE_PutChars (void);
static void trapFXE_GetLine (void);
static void trapFXE_Control (void);
static void trapHCS_cpu_time_and_paging (void);
static void trapHCS_ProcInfo (void);
static void trapHCS_GetAuthorization (void);
static void trapHCS_FsGetPathName (void);
static void trapHCS_chnameSeg (void);
static void trapHCS_SetSafetySwSeg (void);
static void trapHCS_StatusLong (void);
static void trapHCS_LevelGet (void);
static void trapHCS_GetRingBrackets (void);
static void trapHCS_TruncateSeg (void);
static void trapHCS_GetMaxLengthSeg (void);
static void trapHCS_SetMaxLengthSeg (void);
static void trapHCS_AddAclEntries (void);
static void trapHCS_DeleteAclEntries (void);
static void trapHCS_ListAcl (void);
static void trapHCS_ReplaceAcl (void);
static void trapHCS_Status (void);

// debugging traps
#if 0
static void trapFXE_debug (void);
#endif

static trapNameTableEntry trapNameTable [] =
  {
    { "hcs_", "history_regs_set", trapHCS_HistoryRegsSet },
    { "hcs_", "history_regs_get", trapHCS_HistoryRegsGet },
    { "hcs_", "fs_search_get_wdir", trapHCS_FSSearchGetWdir },
    { "hcs_", "initiate_count", trapHCS_InitiateCount },
    { "hcs_", "initiate", trapHCS_Initiate },
    { "hcs_", "terminate_name", trapHCS_TerminateName },
    { "hcs_", "make_ptr", trapHCS_MakePtr },
    { "hcs_", "status_mins", trapHCS_StatusMins },
    { "hcs_", "make_seg", trapHCS_MakeSeg },
    { "hcs_", "set_bc_seg", trapHCS_SetBcSeg },
    { "hcs_", "high_low_seg_count", trapHCS_HighLowSegCount },
    { "hcs_", "fs_get_mode", trapHCS_FSGetMode },
    { "hcs_", "terminate_noname", trapHCS_TerminateNoname },
    { "hcs_", "proc_info", trapHCS_ProcInfo },
    { "hcs_", "get_authorization", trapHCS_GetAuthorization },
    { "hcs_", "fs_get_path_name", trapHCS_FsGetPathName },
    { "hcs_", "chname_seg", trapHCS_chnameSeg },
    { "hcs_", "set_safety_sw_seg", trapHCS_SetSafetySwSeg },
    { "hcs_", "status_long", trapHCS_StatusLong },
    { "hcs_", "level_get", trapHCS_LevelGet },
    { "hcs_", "get_ring_brackets", trapHCS_GetRingBrackets },
    { "hcs_", "truncate_seg", trapHCS_TruncateSeg },
    { "hcs_", "get_max_length_seg", trapHCS_GetMaxLengthSeg },
    { "hcs_", "set_max_length_seg", trapHCS_SetMaxLengthSeg },
    { "hcs_", "add_acl_entries", trapHCS_AddAclEntries },
    { "hcs_", "delete_acl_entries", trapHCS_DeleteAclEntries },
    { "hcs_", "list_acl", trapHCS_ListAcl },
    { "hcs_", "replace_acl", trapHCS_ReplaceAcl },
    { "hcs_", "status_", trapHCS_Status },

    { "cpu_time_and_paging_", "cpu_time_and_paging_", trapHCS_cpu_time_and_paging },

    { "phcs_", "set_kst_attributes", trapPHCS_SetKSTAttributes },
    { "phcs_", "deactivate", trapPHCS_Deactivate },

    { "get_line_length_", "switch", trapGetLineLengthSwitch },

    { "slt_manager", "get_seg_ptr", trapGetSegPtr },

    //{ "tssi_", "get_segment", trapGetSegment },

    { "fs_util_", "get_type", trapGetType },

    { "user_info_", "homedir", trapHomedir },

    { "fxe", "unhandled_signal", trapFXE_UnhandledSignal },
    { "fxe", "return_to_fxe", trapFXE_ReturnToFXE },
    { "fxe", "put_chars", trapFXE_PutChars },
    { "fxe", "get_line", trapFXE_GetLine },
    { "fxe", "control", trapFXE_Control },

    // { "com_err_", "com_err_", trapComErr },

// debug
    // { "expand_pathname_", "expand_pathname_", trapFXE_debug },
  };
#define N_TRAP_NAMES (sizeof (trapNameTable) / sizeof (trapNameTableEntry))

static int trapName (char * segName, char * symbolName)
  {
    for (int i = 0; i < (int) N_TRAP_NAMES; i ++)
      {
        if (strcmp (segName, trapNameTable [i] . segName) == 0 &&
            strcmp (symbolName, trapNameTable [i] . symbolName) == 0)
          {
            return i;
          }
      }
    return -1;
  }

static bool whiteList (char * segName)
  {
#if 0
    if (strcmp (segName, "pl1_error_messages_") == 0)
      return true;

    return false;
#endif
    if (strcmp (segName, "hcs_") == 0)
      return false;

    return true;
  }

static int resolveName (char * segName, char * symbolName, word15 * segno,
                        word18 * value, sgIdx * index)
  {
    //sim_printf ("resolveName %s:%s\n", segName, symbolName);
    int trapNo = trapName (segName, symbolName);
    if (trapNo >= 0)
      {
        * segno = TRAP_SEGNO;
        * value = (word18) trapNo;
        * index = -1;
        return 1;
      }
   
    sgIdx idx;
#if 1
    if ((idx = searchRNT (segName)) != NOSGIDX)
      {
        KSTEntry * e = KST + idx;
        if (e -> allocated && e -> loaded && e -> parsed && 
            e -> definition_offset &&
            lookupDef (idx, segName, symbolName, value))
          {
            * segno = e -> segno;
            * index = idx;
            //sim_printf ("resoveName %s:%s %05o:%06o\n", segName, symbolName, * segno, * value);
            return 1;
          }
      }
#endif
#if 1
    //sim_printf ("resolveName %s:%s\n", segName, symbolName);
    for (idx = 0; idx < (int) N_SEGS; idx ++)
      {
        KSTEntry * e = KST + idx;
        if (! e -> allocated)
          continue;
        if (! e -> loaded)
          continue;
        if (! e -> parsed)
          continue;
        if (e -> definition_offset == 0)
          continue;
        if (e -> RNTRefCount) // Searched this segment already above.
          continue;
        if (lookupDef (idx, segName, symbolName, value))
          {
            * segno = e -> segno;
            * index = idx;
            //sim_printf ("resoveName %s:%s %05o:%06o\n", segName, symbolName, * segno, * value);
            return 1;
          }
      }
#endif

    if (whiteList (segName))
      {
        //idx = loadSegmentFromFile (segName);
        idx = loadSegment (segName);
        if (idx >= 0)
          {
            //sim_printf ("got file\n");
            sgIdx slteIdx = -1;
            KST [idx] . segno = getSegnoFromSLTE (segName, & slteIdx);
            if (! KST [idx] . segno)
              {
                setSegno (idx, allocateSegno ());
                // sim_printf ("assigning %d to %s\n", KST [idx] . segno, segName);
              }
            if (slteIdx < 0) // segment not in slte
              {
                KST [idx] . R1 = FXE_RING;
                KST [idx] . R2 = FXE_RING;
                KST [idx] . R3 = FXE_RING;
                KST [idx] . R = 1;
                KST [idx] . E = 1;
                KST [idx] . W = 1;
                KST [idx] . P = 0;
              }
            else // segment in slte
              {
                KST [idx] . R1 = SLTE [slteIdx] . R1;
                KST [idx] . R2 = SLTE [slteIdx] . R2;
                KST [idx] . R3 = SLTE [slteIdx] . R3;
                KST [idx] . R = SLTE [slteIdx] . R;
                KST [idx] . E = SLTE [slteIdx] . E;
                KST [idx] . W = SLTE [slteIdx] . W;
                KST [idx] . P = SLTE [slteIdx] . P;
              }
            //sim_printf ("made %05o %s len %07o\n", KST [idx] . segno, 
                        //KST [idx] . segname, KST [idx] . seglen);
            installLOT (idx);
            installSDW (idx);
            if (lookupDef (idx, segName, symbolName, value))
              {
                * segno = KST [idx] . segno;
                * index = idx;
                // sim_printf ("resoveName %s:%s %05o:%06o\n", segName, symbolName, * segno, * value);
                return 1;
              }
            // sim_printf ("found segment but not symbol\n");
// XXX this segment should be terminated if the symbol was not found
            return 0; 
          } // idx >= 0
      } // whitelist

    // sim_printf ("resoveName fail\n");
    return 0;
  }

static void parseSegment (sgIdx segIdx)
  {
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    KSTEntry * e = KST + segIdx;
    e -> parsed = false;
    word24 seglen = e -> seglen;

    word36 * segp = M + segAddr;

    if (seglen == 0)
      {
        sim_printf ("ERROR: Can't parse empty segment\n");
        return;
      }
    word36 lastword = segp [seglen - 1];
    word24 i = GETHI (lastword);
    if (i >= seglen)
      {
        sim_printf ("ERROR: mapPtr too big %07u >= %07u\n", i, seglen);
        return;
      }
    if (seglen - i - 1 < 11)
      {
        sim_printf ("ERROR: mapPtr too small %07u\n", seglen - i - 1);
        return;
      }

    object_map * mapp = (object_map *) (segp + i);

    if (mapp -> identifier [0] == 0157142152137LLU && // "obj_"
        mapp -> identifier [1] == 0155141160040LLU) // "map "
      {

        if (mapp -> decl_vers != 2)
          {
            sim_printf ("ERROR: Can't hack object map version %llu\n", mapp -> decl_vers);
            return;
          }

//sim_printf ("text offset %o\n", mapp -> text_offset);
//sim_printf ("text length %o\n", mapp -> text_length);
//sim_printf ("definition offset %o\n", mapp -> definition_offset);
//sim_printf ("definition length %o\n", mapp -> definition_length);
        e -> definition_offset = mapp -> definition_offset;
//sim_printf ("align2 %012llo\n", mapp -> align2);
//sim_printf ("linkage offset %o\n", mapp -> linkage_offset);
        e -> linkage_offset = mapp -> linkage_offset;
//sim_printf ("linkage length %o\n", mapp -> linkage_length);
        e -> linkage_length = mapp -> linkage_length;
//sim_printf ("static offset %o\n", mapp -> static_offset);
        e -> isot_offset = mapp -> static_offset;
//sim_printf ("static length %o\n", mapp -> static_length);
//sim_printf ("symbol offset %o\n", mapp -> symbol_offset);
        e -> symbol_offset = mapp -> symbol_offset;

//sim_printf ("symbol length %o\n", mapp -> symbol_length);

//        word36 * oip_textp = segp + mapp -> text_offset;
        def_header * oip_defp = (def_header *) (segp + mapp -> definition_offset);
        link_header * oip_linkp = (link_header *) (segp + mapp -> linkage_offset);
//        word36 * oip_statp = segp + mapp -> static_offset;
//        word36 * oip_symbp = segp + mapp -> symbol_offset;
//        word36 * oip_bmapp = NULL;
//        if (mapp -> break_map_offset)
//          oip_bmapp = segp + mapp -> break_map_offset;
//        word18 oip_tlng = mapp -> text_length;
//        word18 oip_dlng = mapp -> definition_length;
//        word18 oip_llng = mapp -> linkage_length;
//        word18 oip_ilng = mapp -> static_length;
//        word18 oip_slng = mapp -> symbol_length;
//        word18 oip_blng = mapp -> break_map_length;
//        word1 oip_format_procedure = mapp -> format . procedure;
//        word1 oip_format_bound = mapp -> format . bound;
//        if (oip_format_bound)
//          sim_printf ("Segment is bound.\n");
//        else
//          sim_printf ("Segment is unbound.\n");
//        word1 oip_format_relocatable = mapp -> format . relocatable;
//        word1 oip_format_standard = mapp -> format . standard; /* could have standard obj. map but not std. seg. */
            bool oip_format_gate = mapp -> entry_bound != 0;
//        word1 oip_format_separate_static = mapp -> format . separate_static;
//        word1 oip_format_links_in_text = mapp -> format . links_in_text;
//        word1 oip_format_perprocess_static = mapp -> format . perprocess_static;
            word18 entry_bound = mapp -> entry_bound;
//        word18 textlinkp = mapp -> text_link_offset;

        if (oip_format_gate)
          {
            //sim_printf ("Segment is gated; entry bound %d.\n", entry_bound);
            e -> gated = true;
            e -> entry_bound = entry_bound;
          }
        else
          {
            //sim_printf ("Segment is ungated.\n");
            e -> gated = false;
            e -> entry_bound = 0;
          }

        bool entryFound = false;
        word18 entryValue = 0;
        segnameBuf entryName;

        // Walk the definiton
// oip_defp
        word36 * defBase = (word36 *) oip_defp;

//        sim_printf ("Definitions:\n");
//sim_printf ("def_list_relp %u\n", oip_defp -> def_list_relp);
//sim_printf ("hash_table_relp %u\n", oip_defp -> hash_table_relp);
        definition * p = (definition *) (defBase +
                                         oip_defp -> def_list_relp);
        while (* (word36 *) p)
          {
            if (p -> ignore == 0)
              {
#if 0
                if (p -> new == 0)
                  sim_printf ("warning: !new\n");
                sim_printf ("    %lu ", (word36 *) p - defBase);
                printACC (defBase + p -> symbol);
                sim_printf ("\n");
                if (p -> entry)
                  sim_printf ("        entry\n");
                if (p -> argcount)
                  sim_printf ("        argcount\n");
                if (p -> descriptors)
                  sim_printf ("        descriptors\n");
                switch (p -> class)
                  {
                    case 0:
                      sim_printf ("        text section\n");
                      break;
                    case 1:
                      sim_printf ("        linkage section\n");
                      break;
                    case 2:
                      sim_printf ("        symbol section\n");
                      break;
                    case 3:
                      sim_printf ("        segment name\n");
                      break;
                    case 4:
                      sim_printf ("        static section\n");
                      break;
                    default:
                      sim_printf ("        ???? section\n");
                      break;
                  }
                if (p -> class == 3)
                  {
                      sim_printf ("        segname_thread %u\n", p -> value);
                      sim_printf ("        first_relp *%u\n", p -> segname);
                  }
                else
                  {
                      sim_printf ("        value 0%o\n", p -> value);
                      sim_printf ("        segname *%u\n", p -> segname);
                  }
                sim_printf ("\n");
#endif
                if ((! entryFound) && p -> class == 0 && p -> entry)
                  {
                    entryFound = true;
                    entryValue = p -> value;
                    cpACC2buf (entryName, defBase + p -> symbol);
//sim_printf ("entry name %s\n", entryName);                
                  }
              }
            p = (definition *) (defBase + p -> forward);
          }

        if (! entryFound)
          {
            //sim_printf ("ERROR: entry point not found\n");
            e -> entry = 0;
          }
        else
          {
            e -> entry = entryValue;
            memcpy (e -> pathname, entryName, sizeof (pathnameBuf));
    
            //sim_printf ("entry %o\n", entryValue);
          }

        // Walk the linkage

        // word36 * linkBase = (word36 *) oip_linkp;

        //sim_printf ("defs_in_link %o\n", oip_linkp -> defs_in_link);
        //sim_printf ("def_offset %o\n", oip_linkp -> def_offset);
        //sim_printf ("first_ref_relp %o\n", oip_linkp -> first_ref_relp);
        //sim_printf ("link_begin %o\n", oip_linkp -> link_begin);
        //sim_printf ("linkage_section_lng %o\n", oip_linkp -> linkage_section_lng);
        //sim_printf ("segno_pad %o\n", oip_linkp -> segno_pad);
        //sim_printf ("static_length %o\n", oip_linkp -> static_length);
    
        e -> segnoPadAddr = & (oip_linkp -> align4);

#if 0
        link_ * l = (link_ *) (linkBase + oip_linkp -> link_begin);
        link_ * end = (link_ *) (linkBase + oip_linkp -> linkage_section_lng);

        do
          {
            if (l -> tag != 046)
              continue;
            //sim_printf ("  tag %02o\n", l -> tag);
            if (l -> header_relp != 
                ((word18) (- (((word36 *) l) - linkBase)) & 0777777))
              sim_printf ("WARNING:  header_relp wrong %06o (%06o)\n",
                          l -> header_relp,
                          (word18) (- (((word36 *) l) - linkBase)) & 0777777);
            if (l -> run_depth != 0)
              sim_printf ("WARNING:  run_depth wrong %06o\n", l -> run_depth);

            //sim_printf ("ringno %0o\n", l -> ringno);
            //sim_printf ("run_depth %02o\n", l -> run_depth);
            //sim_printf ("expression_relp %06o\n", l -> expression_relp);
       
            if (l -> expression_relp >= oip_dlng)
              sim_printf ("WARNING:  expression_relp too big %06o\n", l -> expression_relp);

            expression * expr = (expression *) (defBase + l -> expression_relp);

            //sim_printf ("  type_ptr %06o  exp %6o\n", 
                        //expr -> type_ptr, expr -> exp);

            if (expr -> type_ptr >= oip_dlng)
              sim_printf ("WARNING:  type_ptr too big %06o\n", expr -> type_ptr);
            type_pair * typePair = (type_pair *) (defBase + expr -> type_ptr);

            switch (typePair -> type)
              {
                case 1:
                  sim_printf ("    1: self-referencing link\n");
                  sim_printf ("WARNING: unhandled type %d\n", typePair -> type);
                  break;
                case 3:
                  sim_printf ("    3: referencing link\n");
                  sim_printf ("WARNING: unhandled type %d\n", typePair -> type);
                  break;
                case 4:
                  sim_printf ("    4: referencing link with offset\n");
                  sim_printf ("      seg %s\n", sprintACC (defBase + typePair -> seg_ptr));
                  sim_printf ("      ext %s\n", sprintACC (defBase + typePair -> ext_ptr));
                  //int dsegIdx = loadDeferred (sprintACC (defBase + typePair -> seg_ptr));
                  break;
                case 5:
                  sim_printf ("    5: self-referencing link with offset\n");
                  sim_printf ("WARNING: unhandled type %d\n", typePair -> type);
                  break;
                default:
                  sim_printf ("WARNING: unknown type %d\n", typePair -> type);
                  break;
              }


            //sim_printf ("    type %06o\n", typePair -> type);
            //sim_printf ("    trap_ptr %06o\n", typePair -> trap_ptr);
            //sim_printf ("    seg_ptr %06o\n", typePair -> seg_ptr);
            //sim_printf ("    ext_ptr %06o\n", typePair -> ext_ptr);

            if (typePair -> trap_ptr >= oip_dlng)
              sim_printf ("WARNING:  trap_ptr too big %06o\n", typePair -> trap_ptr);
            if (typePair -> ext_ptr >= oip_dlng)
              sim_printf ("WARNING:  ext_ptr too big %06o\n", typePair -> ext_ptr);

            if (typePair -> ext_ptr != 0)
              {
                 //sim_printf ("    ext %s\n", sprintACC (defBase + typePair -> ext_ptr));
              }
          }
        while (++ l < end);
    #endif
        e -> parsed = true;
        return;
      }

    word24 ilow = GETLO (lastword);
    if (ilow + 11u > seglen)
      {
        sim_printf ("ERROR: glopPtr too big %07u >= %07u\n", i, seglen);
        return;
      }
    object_glop * glopp = (object_glop *) (segp + ilow);

    if (glopp -> idwords [0] == 0525252525252llu &&
        glopp -> idwords [1] == 0525252525252llu &&
        glopp -> idwords [2] == 0525252525252llu &&
        glopp -> idwords [3] == 0525252525252llu)
      {
sim_printf ("textrel %012llo\n", glopp -> textrel);
sim_printf ("textbc %012llo %012llo\n", glopp -> textbc, glopp -> textbc / 36ull);
sim_printf ("linkrel %012llo\n", glopp -> linkrel);
sim_printf ("linkbc %012llo %012llo\n", glopp -> linkbc, glopp -> linkbc / 36ull);
sim_printf ("symbolrel %012llo\n", glopp -> symbolrel);
sim_printf ("symbolbc %012llo %012llo\n", glopp -> symbolbc, glopp -> symbolbc / 36ull);
sim_printf ("maprel %012llo\n", glopp -> maprel);
exit(3);
        //return;
      }

sim_printf ("i %o\n", i);
    sim_printf ("ERROR: mapID wrong %012llo %012llo\n", 
                mapp -> identifier [0], mapp -> identifier [1]);
    return;
  }

static void readSegment (int fd, sgIdx segIdx/* , off_t flen*/)
  {
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word24 maddr = segAddr;
    uint seglen = 0;

    // 72 bits at a time; 2 dps8m words == 9 bytes
    uint8 bytes [9];
    ssize_t n;
    while ((n = read (fd, bytes, 9)))
      {
        if (n != 5 && n != 9)
          {
            //sim_printf ("ERROR: readSegment: garbage at end of segment lost %ld\n", n);
          }
        if (seglen > MAX18)
          {
            sim_printf ("ERROR: File too long\n");
            return;
          }
        M [maddr ++] = extr36 (bytes, 0);
        M [maddr ++] = extr36 (bytes, 1);
        seglen += 2;
      }
    //sim_printf ("seglen %d flen %ld\n", seglen, flen * 8 / 36);
    //KST [segIdx] . seglen = seglen;
    //KST [segIdx] . seglen = flen * 8 / 36;
    //KST [segIdx] . bit_count = 36 * KST [segIdx] . seglen;
    KST [segIdx] . loaded = true;
    //sim_printf ("Loaded %u words in segment index %d @ %08o\n", 
                //seglen, segIdx, segAddr);
    parseSegment (segIdx);
  }

static int loadSegmentFromFile (char * arg)
  {

    char upathname [UNIX_PATHNAME_LEN + 1];
    strncpy (upathname, MROOT, UNIX_PATHNAME_LEN + 1);
    strncat (upathname, arg, UNIX_PATHNAME_LEN + 1);
    char * blow;
    while ((blow = strchr (upathname, '>')))
      * blow = '/';
    //sim_printf ("tada> %s\n", upathname);

    //int fd = open (arg, O_RDONLY);
    int fd = open (upathname, O_RDONLY);
    if (fd < 0)
      {
        //sim_printf ("ERROR: Unable to open '%s': %d\n", arg, errno);
        return -1;
      }

    off_t flen = lseek (fd, 0, SEEK_END);
    lseek (fd, 0, SEEK_SET);

    // mapID works better if the length is the last whole word loaded;
    // (ignore partial reads).
    //word24 bitcnt = flen * 8;
    //word18 wordcnt = nbits2nwords (bitcnt);
    word18 wordcnt = (word18) (((flen * 8) / 36) & MASK18); 
// XXX it would be better to be pasing around the segment name as well as the
// path instead of relying on basename here.
    sgIdx segIdx = allocateSegment (wordcnt, basename (upathname), 0, RINGS_ZFF, P_REW);
    if (segIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate segment for segment load\n");
        return -1;
      }

    readSegment (fd, segIdx /*, flen*/);

    return segIdx;
  }

static void buildPath (pathnameBuf pathname, char * path, char * component)
  {
    strncpy (pathname, path, PATHNAME_LEN + 1);
    strncat (pathname, ">", PATHNAME_LEN + 1);
    strncat (pathname, component, PATHNAME_LEN + 1);
    pathname [PATHNAME_LEN] = '\0';
  }

static int loadSegment (char * arg)
  {
// Search rules:
//
//  1. initiated segments.
//      reference name
//         a. use in a dynamically linked external program reference.
//         b. a call to hcs_$initiate, hcs_$initiate)count, or hcs_$make_seg.
//         c. a call to hcs_$make_ptr.
//
//  2. referencing directory
//       the referencing directory contains the procedure segment whose call
//       or reference initiated the search.
//
//  3. working directory
//
//  4. system libraries
//       >system_library_standard
//       >system_library_unbundled
//       >system_library_1
//       >system_library_tools
//       >system_library_auth_maint


// 1. initiated segments

    int idx = searchRNT (arg);

    if (idx >= 0)
      return idx;

    for (idx = 0; idx < (int) N_SEGS; idx ++)
      {
        KSTEntry * e = KST + idx;
        if (! e -> allocated)
          continue;
        if (e -> RNTRefCount) // Searched this segment already above.
          continue;
        if (strcmp (arg, e -> segname) == 0)
          return idx;
      }

// 2. referencing directory XXX don't know how to do this.

// 3. working directory

    pathnameBuf pathname = "";
    buildPath (& pathname [0], cwd, arg);

    idx = loadSegmentFromFile (pathname);
    if (idx >= 0)
      {
        memcpy (KST [idx] . pathname, cwd, sizeof (pathnameBuf));
        return idx;
      }

//  4. system libraries

    static pathnameBuf stds [5] =
      {
        ">system_library_standard",
        ">system_library_unbundled",
// XXX why is execution needed?
        ">library_dir_dir>system_library_1>execution",
        ">system_library_tools",
        ">system_library_auth_maint"
      };

    for (int i = 0; i < 5; i ++)
      {
        buildPath (& pathname [0], stds [i], arg);

        idx = loadSegmentFromFile (pathname);
        if (idx >= 0)
          {
            memcpy (KST [idx] . pathname, stds [i], sizeof (pathnameBuf));
            return idx;
          }
      }
    return -1;
  }

static void setupWiredSegments (void)
  {
     // allocate wired segments

     // 'dseg' contains the fault pairs and descriptors

     initializeDSEG ();
     KST [dsegIdx] . wired = true;
  }


static sgIdx stack0Idx;

// From multicians: The maximum size of user ring stacks is initially set to 48K

#define STK_SIZE (48 * 1024)

static void createStackSegments (void)
  {
    for (word3 i = 0; i < 8; i ++)
      {
        segnameBuf segname;
        sprintf (segname, "stack_%d", i);
        sgIdx ssIdx = allocateSegment (STK_SIZE, segname, STACKS_SEGNO + i,
                                       i, FXE_RING, FXE_RING, P_RW);
        if (i == 0)
          stack0Idx = ssIdx;
        KSTEntry * e = KST + ssIdx;
        e -> loaded = false;
        word24 segAddr = lookupSegAddrByIdx (ssIdx);
        memset (M + segAddr, 0, sizeof (stack_header));

        installSDW (ssIdx);
      }
  }

static void initStack (sgIdx ssIdx)
  {
// bound_file_system.s.archive has a 'MAKESTACK' function;
// borrowing from there.

#define HDR_OFFSET 0
#define HDR_SIZE 64
// we assume HDR_SIZE is a multiple of 8, since frames must be so aligned
#define STK_TOP HDR_SIZE

    word15 stkSegno = KST [ssIdx] . segno;
    word24 segAddr = lookupSegAddrByIdx (ssIdx);
    word24 hdrAddr = segAddr + HDR_OFFSET;

    word15 libSegno = KST [libIdx] . segno;
    word3 libRing = KST [libIdx] . R1;
    word24 libAddr = lookupSegAddrByIdx (libIdx);

    // To help with debugging, initialize the stack header with
    // null ptrs
    for (int i = 0; i < HDR_SIZE; i += 2)
      makeNullPtr (M + hdrAddr + i);

    // word  0,  1    reserved

    // word  2,  3    reserved

    // word  4,  5    odd_lot_ptr

    // word  6,  7    combined_stat_ptr
    makeITS (M + hdrAddr + 6, SSA_SEGNO, 0, 0, 0, 0); 

    // word  8,  9    clr_ptr
    makeITS (M + hdrAddr + 8, SSA_SEGNO, 0, 0, 0, 0); 

    // word 10        max_lot_size, run_unit_depth
    M [hdrAddr + 10] = ((word36) LOT_SIZE << 18) | RUN_UNIT_DEPTH;

    // word 11        cur_lot_size, pad2
    M [hdrAddr + 11] = ((word36) LOT_SIZE << 18) | 0;

    // word 12, 13    system_storage_ptr; aka system_free_ptr
    makeITS (M + hdrAddr + 12, SSA_SEGNO, 0, 0, 0, 0); 

    // word 14, 15    user_storage_ptr; aka user_free_ptr
    makeITS (M + hdrAddr + 14, SSA_SEGNO, 0, 0, 0, 0); 

    // word 16, 17    null_ptr
    makeNullPtr (M + hdrAddr + 16);

    // word 18, 19    stack_begin_ptr
    makeITS (M + hdrAddr + 18, stkSegno, (word3) (ssIdx - stack0Idx), 
             STK_TOP, 0, 0);

    // word 20, 21    stack_end_ptr
    makeITS (M + hdrAddr + 20, stkSegno, (word3) (ssIdx - stack0Idx), 
             STK_TOP, 0, 0);

    // word 22, 23    lot_ptr
    makeITS (M + hdrAddr + 22, CLR_SEGNO, (word3) (ssIdx - stack0Idx), 
             LOT_OFFSET, 0, 0); 

    // word 24, 25    signal_ptr
    int trapNo = trapName ("fxe", "unhandled_signal");
    if (trapNo < 0)
      makeNullPtr (M + hdrAddr + 24);
    else
      makeITS (M + hdrAddr + 24, TRAP_SEGNO, FXE_RING, (word18) trapNo, 0, 0);

    // word 26, 27    bar_mode_sp_ptr

    // word 28, 29    pl1_operators_table
    word18 operator_table = 0777777;
    if (! lookupDef (libIdx, "pl1_operators_", "operator_table",
                     & operator_table))
     {
       sim_printf ("ERROR: Can't find pl1_operators_$operator_table\n");
       makeNullPtr (M + hdrAddr + 28);
     }
    else
      {
        makeITS (M + hdrAddr + 28, libSegno, (word3) (ssIdx - stack0Idx), operator_table, 0, 0);
      }
// MR12.3_restoration/MR12.3/library_dir_dir/system_library_1/source/bound_file_system.s.archive.ascii, line 11779
// AK92, pg 59


    // word 30, 31    call_op_ptr
    // I am guessing that this is GETHI()
    //  stack_header.call_op_ptr =
    //    ptr (workptr, addrel (workptr, call_offset) -> 
    //      instruction.tra_offset);
    word18 callOffset = operator_table + call_offset;
    word24 callTraInstAddr = libAddr + callOffset;
    word36 callTraInst = M [callTraInstAddr];
//sim_printf ("callOffset %06o callTraInstAddr %08o calTraInst %012llo\n",
// callOffset, callTraInstAddr, callTraInst);
    makeITS (M + hdrAddr + 30, libSegno, libRing, GETHI (callTraInst), 0, 0);

    // word 32, 33    push_op_ptr
    word18 pushOffset = operator_table + push_offset;
    word24 pushTraInstAddr = libAddr + pushOffset;
    word36 pushTraInst = M [pushTraInstAddr];
    makeITS (M + hdrAddr + 32, libSegno, libRing, GETHI (pushTraInst), 0, 0);

    // word 34, 35    return_op_ptr
    word18 returnOffset = operator_table + return_offset;
    word24 returnTraInstAddr = libAddr + returnOffset;
    word36 returnTraInst = M [returnTraInstAddr];
    makeITS (M + hdrAddr + 34, libSegno, libRing, GETHI (returnTraInst), 0, 0);

    // word 36, 37    short_return_op_ptr
    word18 returnNoPopOffset = operator_table + return_no_pop_offset;
    word24 returnNoPopTraInstAddr = libAddr + returnNoPopOffset;
    word36 returnNoPopTraInst = M [returnNoPopTraInstAddr];
    makeITS (M + hdrAddr + 36, libSegno, libRing, GETHI (returnNoPopTraInst), 0, 0);

    // word 38, 39    entry_op_ptr
    word18 entryOffset = operator_table + entry_offset;
    word24 entryTraInstAddr = libAddr + entryOffset;
    word36 entryTraInst = M [entryTraInstAddr];
    makeITS (M + hdrAddr + 38, libSegno, libRing, GETHI (entryTraInst), 0, 0);

    // word 40, 41    trans_op_tv_ptr

    // word 42, 43    isot_ptr
    makeITS (M + hdrAddr + 42, CLR_SEGNO, 0, ISOT_OFFSET, 0, 0); 

    // word 44, 45    sct_ptr

    // word 46, 47    unwinder_ptr

    // word 48, 49    sys_link_info_ptr ptr to *system link name table

    // word 50, 51    rnt_ptr

    // word 52, 53    ect_ptr
    makeNullPtr (M + hdrAddr + 52);

    // word 54, 55    assign_linkage_ptr
    makeITS (M + hdrAddr + 54, SSA_SEGNO, 0, 0, 0, 0); 

    // word 56, 57    reserved

    // word 58, 59    reserved

    // word 60, 61    reserved

    // word 62, 63    reserved

  }

static void createFrame (sgIdx ssIdx, word15 prevSegno, word18 prevWordno, word3 prevRing)
  {
    word15 stkSegno = KST [ssIdx] . segno;
    word24 segAddr = lookupSegAddrByIdx (ssIdx);
    word24 frameAddr = segAddr + STK_TOP;
    
    //sim_printf ("stack %d frame @ %08o\n", ssIdx, frameAddr);

#define FRAME_SZ 40 // assuming no temporaries

    // word  0,  1    pr storage
    makeNullPtr (M + frameAddr +  0);

    // word  2,  3    pr storage
    makeNullPtr (M + frameAddr +  2);

    // word  4,  5    pr storage
    makeNullPtr (M + frameAddr +  4);

    // word  5,  7    pr storage
    makeNullPtr (M + frameAddr +  6);

    // word  8,  9    pr storage
    makeNullPtr (M + frameAddr +  8);

    // word 10, 11    pr storage
    makeNullPtr (M + frameAddr + 10);

    // word 12, 13    pr storage
    makeNullPtr (M + frameAddr + 12);

    // word 14, 15    pr storage
    makeNullPtr (M + frameAddr + 14);

    // word 16, 17    prev_stack_frame_ptr
    //makeNullPtr (M + frameAddr + 16);
    makeITS (M + frameAddr + 16, prevSegno, prevRing, prevWordno, 0, 0);
    //sim_printf ("stack %d prev %05o:%06o:%o @ %08o\n", ssIdx, prevSegno, prevWordno, prevRing, frameAddr + 16);
    // word 18, 19    next_stack_frame_ptr
    makeITS (M + frameAddr + 18, stkSegno, (word3) (ssIdx - stack0Idx), STK_TOP + FRAME_SZ, 0, 0);

    // word 20, 21    return_ptr
    //makeNullPtr (M + frameAddr + 20);
    int trapNo = trapName ("fxe", "return_to_fxe");
    if (trapNo < 0)
      makeNullPtr (M + frameAddr + 20);
    else
      makeITS (M + frameAddr + 20, TRAP_SEGNO, FXE_RING, (word18) trapNo, 0, 0);
    
    // word 22, 23    entry_ptr
    makeNullPtr (M + frameAddr + 22);

    // word 24, 25    operator_link_ptr
    //makeNullPtr (M + frameAddr + 24);
    word18 operator_table = 0777777;
    if (! lookupDef (libIdx, "pl1_operators_", "operator_table",
                     & operator_table))
      {
        sim_printf ("ERROR: Can't find pl1_operators_$operator_table\n");
        makeNullPtr (M + frameAddr + 24);
      }
    else
      {
        word15 libSegno = KST [libIdx] . segno;
        makeITS (M + frameAddr + 24, libSegno, FXE_RING, operator_table, 0, 0);
      }

    // word 25, 26    argument_ptr
    makeNullPtr (M + frameAddr + 26);

    // Update the header

    // word 18, 19    stack_begin_ptr
    word24 hdrAddr = segAddr + HDR_OFFSET;
    makeITS (M + hdrAddr + 18, stkSegno, (word3) (ssIdx - stack0Idx), STK_TOP, 0, 0);

    // word 20, 21    stack_end_ptr
    makeITS (M + hdrAddr + 20, stkSegno, (word3) (ssIdx - stack0Idx), STK_TOP + FRAME_SZ, 0, 0);

  }

static int installLibrary (char * name)
  {
    //int idx = loadSegmentFromFile (name);
    sgIdx idx = loadSegment (name);
    if (idx < 0)
      {
        sim_printf ("ERROR: installLibrary of %s couldn't\n", name);
        return -1;
      }
    sgIdx slteIdx = -1;
    word15 segno = getSegnoFromSLTE (name, & slteIdx);
    if (segno)
      {
        setSegno (idx, segno);
      }
    else
      {
        segno = allocateSegno ();
        setSegno (idx, segno);
        //sim_printf ("assigning %d to %s\n", segno, name);
      }
    //sim_printf ("lib %s segno %o\n", name, KST [idx] . segno);
    if (slteIdx < 0) // segment not in slte
      {
        KST [idx] . R1 = FXE_RING;
        KST [idx] . R2 = FXE_RING;
        KST [idx] . R3 = FXE_RING;
        KST [idx] . R = 1;
        KST [idx] . E = 1;
        KST [idx] . W = 0;
        KST [idx] . P = 0;
      }
    else // segment in slte
      {
        KST [idx] . R1 = SLTE [slteIdx] . R1;
        KST [idx] . R2 = SLTE [slteIdx] . R2;
        KST [idx] . R3 = SLTE [slteIdx] . R3;
        KST [idx] . R = SLTE [slteIdx] . R;
        KST [idx] . E = SLTE [slteIdx] . E;
        KST [idx] . W = SLTE [slteIdx] . W;
        KST [idx] . P = SLTE [slteIdx] . P;
      }

    installLOT (idx);
    installSDW (idx);
    return idx;
  }



static void initSysinfoStr (char * segName, char * compName, char * str,
                            bool vary)
  {
    word18 value;
    word15 segno;
    sgIdx defIdx;

    if (resolveName (segName, compName, & segno, & value, & defIdx))
      {
        if (defIdx < 0)
          sim_printf ("ERROR: dazed and confused; %s.%s has no idx\n",
                      segName, compName);
        else
          {
            //M [KST [defIdx] . physmem + value + offset] = word;
            if (vary)
              strcpyVarying (KST [defIdx] . physmem, & value, str);
            else
              strcpyNonVarying (KST [defIdx] . physmem, & value, str);
          }
      }
    else
      {
        sim_printf ("ERROR: can't find %s.%s\n", segName, compName);
      }
  }

static void initSysinfoWord36Offset (char * segName, char * compName, 
                                     uint offset, word36 word)
  {
    word18 value;
    word15 segno;
    sgIdx defIdx;

    if (resolveName (segName, compName, & segno, & value, & defIdx))
      {
        if (defIdx < 0)
          sim_printf ("ERROR: dazed and confused; %s.%s has no idx\n",
                      segName, compName);
        else
          M [KST [defIdx] . physmem + value + offset] = word;
      }
    else
      {
        sim_printf ("ERROR: can't find %s.%s\n", segName, compName);
      }
  }

static void initSysinfoWord36 (char * segName, char * compName, 
                                     word36 word)
  {
    initSysinfoWord36Offset (segName, compName, 0, word);
  }

static void initSysinfo (void)
  {
    // Set the flag that applications check to see if Multics is up
    initSysinfoWord36 ("sys_info", "service_system", (word36) (0400000000000llu));
    initSysinfoWord36 ("sys_info", "page_size", (word36) (1024));
    initSysinfoWord36 ("sys_info", "max_seg_size", (word36) (255 * 1024));
    initSysinfoWord36 ("sys_info", "default_max_length", (word36) (255 * 1024));
    initSysinfoWord36 ("sys_info", "seg_size_256K", (word36) (255 * 1024));
    initSysinfoWord36 ("sys_info", "default_256K_enable", (word36) 0);
    initSysinfoWord36 ("sys_info", "default_dir_max_length", (word36) (205 * 1024));
    initSysinfoWord36 ("sys_info", "default_stack_length", (word36) (64 * 1024));
    initSysinfoWord36 ("sys_info", "maxlinks", (word36) (10));
    initSysinfoWord36 ("sys_info", "data_management_ringno", (word36) (2));

//++ /* BEGIN INCLUDE FILE aim_template.incl.pl1 */
//++ 
//++ /* Created 740723 by PG */
//++ /* Modified 06/28/78 by C. D. Tavares to add rcp privilege */
//++ /* Modified 83-05-10 by E. N. Kitltitz to add communications privilege */
//++ 
//++ /* This structure defines the components of both an access
//++    class and an access authorization as interpreted by the
//++    Access Isolation Mechanism. */
//++ 
//++ 
//++ dcl  1 aim_template aligned based,                      /* authorization/access class template */
//++        2 categories bit (36),                           /* access categories */
//++        2 level fixed bin (17) unaligned,                /* sensitivity level */
//++        2 privileges unaligned,                  /* special access privileges (in authorization only) */
//++         (3 ipc,                                 /* interprocess communication privilege */
//++          3 dir,                                 /* directory privilege */
//++          3 seg,                                 /* segment privilege */
//++          3 soos,                                        /* security out-of-service privilege */
//++          3 ring1,                                       /* ring 1 access privilege */
//++          3 rcp,                                 /* RCP resource access privilege */
//++          3 comm) bit (1),                               /* communications cross-AIM privilege */
//++          3 pad bit (11);


/* END INCLUDE FILE aim_template.incl.pl1 */

    initSysinfoWord36Offset ("sys_info", "access_class_ceiling", 0, (word36) (0000001000001llu)); // access_class_ceiling.categories
    initSysinfoWord36Offset ("sys_info", "access_class_ceiling", 1, (word36) (0000007000000llu)); // access_class_ceiling.level

    initSysinfoWord36Offset ("sys_info", "successful_access_threshold", 0, (word36) (0000001000001llu)); // successful_access_threshold.categories
    initSysinfoWord36Offset ("sys_info", "successful_access_threshold", 1, (word36) (0000007000000llu)); // successful_access_threshold.level
    
    initSysinfoWord36Offset ("sys_info", "unsuccessful_access_threshold", 0, (word36) (0000001000001llu)); // unsuccessful_access_threshold.categories
    initSysinfoWord36Offset ("sys_info", "unsuccessful_access_threshold", 1, (word36) (0000007000000llu)); // unsuccessful_access_threshold.level
    
    initSysinfoWord36Offset ("sys_info", "covert_channel_threshold", 0, (word36) (0000001000001llu)); // covert_channel_threshold.categories
    initSysinfoWord36Offset ("sys_info", "covert_channel_threshold", 1, (word36) (0000007000000llu)); // covert_channel_threshold.level
    
    initSysinfoStr ("sys_info", "system_control_dir", ">system_control_dir",
                    true);
    initSysinfoWord36 ("sys_info", "initialization_state", (word36) (4));
    initSysinfoWord36 ("sys_info", "system_type", (word36) (1)); // L68_SYSTEM

// call set_time ("02/24/73 14:00 est Saturday", sys_info.first_reasonable_time);
//      /* The invention of NSS, roughly */
// call set_time ("12/31/99 23:59:59", sys_info.last_reasonable_time);

//  $ date -d "02/24/73 14:00 est Saturday" +%s
//  99428400   // UNIX epoch in seconds of first reasonable time
//  $ date -d ""12/31/99 23:59:59"" +%s
//  946713599   // UNIX epoch in seconds of first reasonable time
//  Sat Feb 24 11:00:00 PST 1973
//  $ date -d "0000 GMT, January 1, 1901" +%s
//  -2177452800 // UNIX epoch of Multics epoch
//  99428400 - (-2177452800)
//  2276881200 // Multics clock at first reasonable time (in seconds)
//  946713599 - (-2177452800)
//  3124166399 // Multics clock at last reasonable time (in seconds)

    word72 firstReasonable = 2276881200;
    firstReasonable *= 1000000; // convert to microseconds
    word72 lastReasonable = 3124166399;
    lastReasonable *= 1000000; // convert to microseconds

// call set_time ("02/24/73 14:00 est Saturday", sys_info.first_reasonable_time);
    initSysinfoWord36Offset ("sys_info", "first_reasonable_time", 0,
                             (word36) ((firstReasonable >> 36) & MASK36));
    initSysinfoWord36Offset ("sys_info", "first_reasonable_time", 1,
                             (word36) (firstReasonable & MASK36));
    initSysinfoWord36Offset ("sys_info", "last_reasonable_time", 0,
                             (word36) ((lastReasonable >> 36) & MASK36));
    initSysinfoWord36Offset ("sys_info", "last_reasonable_time", 1,
                             (word36) (lastReasonable & MASK36));

    initSysinfoWord36 ("sys_info", "hfp_exponent_available", (word36) (0));
    initSysinfoWord36 ("sys_info", "ips_mask_data", (word36) (11));

    initSysinfoStr ("sys_info", "quit_name", "quit", false);
    initSysinfoWord36 ("sys_info", "quit_mask", (word36) (1ull << 35));
    initSysinfoStr ("sys_info", "cput_name", "cput", false);
    initSysinfoWord36 ("sys_info", "cput_mask", (word36) (1ull << 34));
    initSysinfoStr ("sys_info", "alrm_name", "alrm", false);
    initSysinfoWord36 ("sys_info", "alrm_mask", (word36) (1ull << 33));
    initSysinfoStr ("sys_info", "neti_name", "neti", false);
    initSysinfoWord36 ("sys_info", "neti_mask", (word36) (1ull << 32));
    initSysinfoStr ("sys_info", "susp_name", "sus_", false);
    initSysinfoWord36 ("sys_info", "susp_mask", (word36) (1ull << 31));
    initSysinfoStr ("sys_info", "term_name", "trm_", false);
    initSysinfoWord36 ("sys_info", "term_mask", (word36) (1ull << 30));
    initSysinfoStr ("sys_info", "wkp_name", "wkp_", false);
    initSysinfoWord36 ("sys_info", "wkp_mask", (word36) (1ull << 29));
    initSysinfoStr ("sys_info", "pgt_name", "pgt_", false);
    initSysinfoWord36 ("sys_info", "pgt_mask", (word36) (1ull << 28));
    initSysinfoStr ("sys_info", "system_shutdown_scheduled_name", "system_shutdown_scheduled_", false);
    initSysinfoWord36 ("sys_info", "system_shutdown_scheduled_mask", (word36) (1ull << 27));
    initSysinfoStr ("sys_info", "dm_shutdown_scheduled_name", "dm_shutdown_scheduled_name_", false);
    initSysinfoWord36 ("sys_info", "dm_shutdown_scheduled_mask", (word36) (1ull << 26));

    initSysinfoStr ("sys_info", "system_message_name", "system_message_", false);
    initSysinfoWord36 ("sys_info", "system_message_mask", (word36) (1ull << 25));
    initSysinfoWord36 ("sys_info", "all_valid_ips_mask", (word36) (0777600000000llu));
    initSysinfoWord36 ("sys_info", "ipc_privilege", (word36) (1ull << 35));
    initSysinfoWord36 ("sys_info", "dir_privilege", (word36) (1ull << 34));
    initSysinfoWord36 ("sys_info", "seg_privilege", (word36) (1ull << 33));
    initSysinfoWord36 ("sys_info", "soos_privilege", (word36) (1ull << 32));
    initSysinfoWord36 ("sys_info", "ring1_privilege", (word36) (1ull << 31));
    initSysinfoWord36 ("sys_info", "rcp_privilege", (word36) (1ull << 30));
    initSysinfoWord36 ("sys_info", "comm_privilege", (word36) (1ull << 29));
  }

static void setupIOCB (KSTEntry * iocbEntry, char * name, uint i)
  {
    // Define iocb [i]

    iocb * iocbUser =
      (iocb *) (M + iocbEntry -> physmem + i * sizeof (iocb));

    // iocb [i] . version

    word36 * versionEntry = (word36 *) & iocbUser -> version;

    * versionEntry = 0111117130062LLU; // 'IOX2'

    // iocb [i] . put_chars

    word36 * putCharEntry = (word36 *) & iocbUser -> put_chars;

    int trapNo = trapName ("fxe", "put_chars");

    if (trapNo < 0)
      makeNullPtr (putCharEntry);
    else
      makeITS (putCharEntry, TRAP_SEGNO, FXE_RING, (word18) trapNo, 0, 0);
    makeNullPtr (putCharEntry + 2);

    // iocb [i] . get_line

    word36 * getLineEntry = (word36 *) & iocbUser -> get_line;

    trapNo = trapName ("fxe", "get_line");

    if (trapNo < 0)
      makeNullPtr (getLineEntry);
    else
      makeITS (getLineEntry, TRAP_SEGNO, FXE_RING, (word18) trapNo, 0, 0);
    makeNullPtr (getLineEntry + 2);

    // iocb [i] . control

    word36 * controlEntry = (word36 *) & iocbUser -> control;

    trapNo = trapName ("fxe", "control");
    if (trapNo < 0)
      makeNullPtr (controlEntry);
    else
      makeITS (controlEntry, TRAP_SEGNO, FXE_RING, (word18) trapNo, 0, 0);
    makeNullPtr (controlEntry + 2);

#if 0
    // iocb [i] . modes

    word36 * modesEntry = (word36 *) & iocbUser -> modes;

    makeITS (modesEntry, TRAP_SEGNO, FXE_RING, TRAP_MODES, 0, 0);
    makeNullPtr (modesEntry + 2);
#endif


    // Set iox_$<foo>

    word18 value;
    word15 segno;
    sgIdx defIdx;

    if (! resolveName ("iox_", name, & segno, & value, & defIdx))
      {
        sim_printf ("ERROR: can't find iox_:%s\n", name);
        return;
      }
    //sim_printf ("iox_:%s %05o:%06o\n", name, segno, value);
    makeITS (M + KST [defIdx] . physmem + value, IOCB_SEGNO, FXE_RING,
             i * sizeof (iocb), 0, 0);

  }

t_stat fxeDump (UNUSED int32 arg, UNUSED char * buf)
  {
    sim_printf ("\nSegment table\n------- -----\n\n");
    for (sgIdx idx = 0; idx < (int) N_SEGS; idx ++)
      {
        KSTEntry * e = KST + idx;
        if (! e -> allocated)
          continue;
        sim_printf ("%3d %c %06o %08o %07o %o%o%o %c%c%c%c %s\n", 
                    idx, e -> loaded ? '*' : ' ',
                    e -> segno, e -> physmem,
                    e -> seglen, 
                    e -> R1, e -> R2, e -> R3,
                    e -> R ? 'R' : '.',
                    e -> E ? 'E' : '.',
                    e -> W ? 'W' : '.',
                    e -> P ? 'P' : '.',
                    strlen (e -> segname) ? e -> segname : "anon");
      }
    sim_printf ("\n");
    return SCPE_OK;
  }

static word18 lookupErrorCode (char * name)
  {
    //word15 segno; 
    word18 value; 
    //int idx;

    //if (resolveName ("error_table_", name, & segno, & value, & idx))
    if (! lookupDef (errIdx, "error_table_", name, & value))
      {
        sim_printf ("ERROR: couldn't resolve error_table_$%s\n", name);
        return 1; // XXX what is the error code for unknown error code?
      }
    return value;
 }

//
// fxe - load a segment into memory and execute it
//

t_stat fxe (UNUSED int32 arg, char * buf)
  {
    t_stat run_boot_prep (void);
#ifndef SPEED
    if_sim_debug (DBG_TRACE, & fxe_dev)
      sim_printf ("FXE: initializing...\n");
#endif

    // Reset hardware
    run_boot_prep ();

    // Reset all state data
    memset (KST, 0, sizeof (KST));
    memset (segnoMap, -1, sizeof (segnoMap));
    memset (RNT, 0, sizeof (RNT));
    nextSegno = USER_SEGNO;
    nextPhysmem = 0;
    clrFreePtr = CLR_FREE_START;
    strcpy (cwd, DEF_CWD);

// From AK92-2, Process Initialization
//
// Initialize the Known Segment Table (KST)
// Initialize the Procsess Initialization Table (PIT)   ???
// Set working directory to home directory
// Call makestack
//   -- create stack_N
//   -- fill is null pointer, begin pointer, end pointer
//   -- call get_initial_linkage to get the initial linkage for the ring ???
//   -- call initialize_rnt 
//      -- call define_area to get an area for the Reference Name Table (RNT) 
//      (Not done; RNT is virtualized)
//      -- initialize the RNT
//      -- put pointer to RNT in stack header ???
//      -- initialize search rules to default
//   -- Add stack to KST.
//   -- Snap links to signal_, unwinder_, alm operators, pl1 operators
//       (Note: this picks up versions of these from the users home directory)
//   -- Set static condition handlers for no_write_permission, 
//   -- not_in_write_bracket, isot_fault and lot-fault, fills in the thread
//   -- pointers for the first stack frame.
// Find initial process.
// Call call_outer_ring to call out to the user's initial_ring.
// Start initial process.
//
// The default initial process is initialize_process, which:
//  -- Initiate the PIT.
//  -- Set the working directory.
//  -- Set static condition handlers for cput, alrm, trm_ wkp_ adn sus_.
//  -- Attach user_io, user_output, user_input and error_output switches.
//  -- Call the process overseer.

//#define INIT_PROC
#ifdef INIT_PROC
    initializeDSEG ();
    sgIdx segIdx = loadSegment ("bound_process_creation");
    if (segIdx < 0)
      {
        sim_printf ("ERROR: couldn't read bound_process_creation\n");
        return SCPE_OK;
      }
    setSegno (segIdx, allocateSegno ());
    KST [segIdx] . R1 = 0;
    KST [segIdx] . R2 = 0;
    KST [segIdx] . R3 = 0;
    KST [segIdx] . R = 1;
    KST [segIdx] . E = 1;
    KST [segIdx] . W = 0;
    KST [segIdx] . P = 1;

    installSDW (segIdx);
    installLOT (segIdx);

    // Default entry point is the first entry
    word18 entryOffset = KST [segIdx] . entry;

    set_addr_mode (APPEND_mode);
    PPR . IC = entryOffset;
    PPR . PRR = 0;
    PPR . PSR = KST [segIdx] . segno;
    PPR . P = 0;
    //DSBR . STACK = KST [stack0Idx + FXE_RING] . segno >> 3;

    run_cmd (RU_CONT, "");
#else
    setupWiredSegments ();

// Setup CLR

    clrIdx = allocateSegment (MAX_SEGLEN, "clr", CLR_SEGNO, RINGS_ZFF, P_RW);
    if (clrIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate clr segment\n");
        return SCPE_OK;
      }

    KSTEntry * clrEntry = KST + clrIdx;
    clrEntry -> loaded = false;
    installSDW (clrIdx);

    word36 * clrMemory = M + clrEntry -> physmem;

    for (uint i = 0; i < LOT_SIZE; i ++)
      clrMemory [i] = 0007777000000; // bitno 0, seg 7777, word 0

// Setup SSA

    //ssaIdx = allocateSegment (MAX_SEGLEN, "ssa", SSA_SEGNO, 0, 0, 0, P_RW);
    ssaIdx = allocateSegment (MAX_SEGLEN, "ssa", SSA_SEGNO, RINGS_ZFF, P_RW);
    if (ssaIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate clr segment\n");
        return SCPE_OK;
      }

    KSTEntry * ssaEntry = KST + ssaIdx;
    ssaEntry -> loaded = false;
    installSDW (ssaIdx);

    word36 * ssaMemory = M + ssaEntry -> physmem;


    // SSA segment layout

    //     0    +--------
    //          | area header
    //          |    areap -> (SSA, 1024)
    //  1024    +--------
    //          | area
    //          |  ...

    area_header * areah = (area_header *) ssaMemory;
    for (uint i = 0; i < MAX_SEGLEN; i ++)
      ssaMemory [i] = 0;

#if 0 // Version 2. Yay.
// From link_man.list, define_area.list

    areah -> allocation_method = STANDARD_ALLOCATION_METHOD;
    areah -> zero_on_free = 1;
    areah -> zero_on_alloc = 0;
    areah -> dont_free = 0;
    areah -> extend = 0;
    areah -> system = 1;
    areah -> defined_by_call = 0;
#else
    memset (areah -> area_header, 0, sizeof (areah -> area_header));
// the first two words are 0 so that the area can be identified as of the new style,
// the third word contains the size of the area in words,
    areah -> area_header [2] = MAX_SEGLEN - 1024;

// the fourth word is the high water mark,

//  From buddy_alloc_.pl1:
//      if area_ptr -> area_header (4) = 0 then do;
//         /* The following code will convert the area to a new style area 
//            and then allocate the block therein with the new area management 
//            code. */
//
//  The memset above set area_header (4) to 0.

// the fifth word is the first usable word in the area,
// the sixth word is the stratum word number corresponding to the largest 
// possible block in this area,
// words 7 through 23 are stratum words which point to blocks which are free
// and whose size is 2**2 through 2**18 */


#endif

    //for (uint i = 0; i < LOT_SIZE; i ++)
      //ssaMemory [i] = 0007777000000; // bitno 0, seg 7777, word 0


// Setup IOCB

    sgIdx iocbIdx = allocateSegment (MAX_SEGLEN, "iocb", IOCB_SEGNO, 
                                   RINGS_ZFF, P_RW);
    if (iocbIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate iocb segment\n");
        return SCPE_OK;
      }

    KSTEntry * iocbEntry = KST + iocbIdx;

    iocbEntry -> loaded = false;
    installSDW (iocbIdx);

    // sim_printf ("Loading library segment\n");

    libIdx = installLibrary ("bound_library_wired_");
    if (libIdx < 0)
      {
        sim_printf ("ERROR: unable to load bound_library_wired_\n");
        return SCPE_OK;
      }
    installLibrary ("bound_library_1_");
    installLibrary ("bound_library_2_");
    installLibrary ("bound_pl1_runtime_");
    installLibrary ("bound_process_env_");
    installLibrary ("bound_expand_path_");
    installLibrary ("bound_ti_term_");
    installLibrary ("bound_fscom1_");
    installLibrary ("bound_video_"); // for video_data_:terminal_iocb
    installLibrary ("bound_date_time_");
    installLibrary ("bound_command_env_");
    installLibrary ("bound_search_facility_");
    errIdx = installLibrary ("error_table_");
    installLibrary ("sys_info");
    initSysinfo ();
    //installLibrary ("bound_info_rtns_");
    slIdx = installLibrary ("search_list_defaults_");

    setupIOCB (iocbEntry, "user_input", IOCB_USER_INPUT);
    setupIOCB (iocbEntry, "user_output", IOCB_USER_OUTPUT);
    setupIOCB (iocbEntry, "error_output", IOCB_ERROR_OUTPUT);
    setupIOCB (iocbEntry, "user_io", IOCB_USER_IO);

    // Create the fxe segment

    sgIdx fxeIdx = allocateSegment (MAX_SEGLEN, "fxe", FXE_SEGNO, 
                                  RINGS_ZFF, P_REW);
    if (fxeIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate fxe segment\n");
        return SCPE_OK;
      }
    installLOT (fxeIdx);
    KSTEntry * fxeEntry = KST + fxeIdx;

    fxeEntry -> loaded = true;
    installSDW (fxeIdx);


    FXEinitialized = true;

#define maxargs 10
    char * args [maxargs];
    for (int i = 0; i < maxargs; i ++)
      args [i] = malloc (strlen (buf) + 1);
    char * segmentName = malloc (strlen (buf) + 1);
    uint nargs = (uint) sscanf (buf, "%s%s%s%s%s%s%s%s%s%s%s", 
                                segmentName,
                                args [0], args [1], args [2], args [3], 
                                args [4], args [5], args [6], args [7], 
                                args [8], args [9]);
    if (nargs >= 1)
      {
        nargs --; // don't count the segment name

        // segmentName may be "name" or "segment:entry"

        char * entryName = segmentName;
        char * colon = strchr (segmentName, ':');
        if (colon)
          {
            // replace the colon with a null, and point entryName to 
            // the trailing substr
            * colon = '\0';
            entryName = colon + 1;
          }

#ifndef SPEED
        if_sim_debug (DBG_TRACE, & fxe_dev)
          sim_printf ("Loading segment %s\n", segmentName);
#endif
        //int segIdx = loadSegmentFromFile (segmentName);
        int segIdx = loadSegment (segmentName);
        if (segIdx < 0)
          {
            sim_printf ("ERROR: couldn't read %s\n", segmentName);
            return SCPE_OK;
          }
        setSegno (segIdx, allocateSegno ());
        KST [segIdx] . R1 = FXE_RING;
        KST [segIdx] . R2 = FXE_RING;
        KST [segIdx] . R3 = FXE_RING;
        KST [segIdx] . R = 1;
        KST [segIdx] . E = 1;
        KST [segIdx] . W = 0;
        KST [segIdx] . P = 0;

        installSDW (segIdx);
        installLOT (segIdx);

        // Default entry point is the first entry
        word18 entryOffset = KST [segIdx] . entry;

        // If a component name was specified, use it.
        if (colon)
          {
             if (! lookupEntry (segIdx, entryName, & entryOffset))
               {
                 sim_printf ("ERROR: can't find entry point %s:%s\n",
                             segmentName, entryName);
                 return SCPE_OK;
               }
           }


#if 0
        printLineNumbers (segIdx);
#endif
        // sim_printf ("executed segment idx %d, segno %o, phyaddr %08o\n", 
        // segIdx, KST [segIdx] . segno, lookupSegAddrByIdx (segIdx));
        createStackSegments ();
        // sim_printf ("stack segment idx %d, segno %o, phyaddr %08o\n", 
        // stack0Idx + FXE_RING, KST [stack0Idx + FXE_RING] . segno, lookupSegAddrByIdx (stack0Idx + FXE_RING));
        // sim_printf ("lib segment idx %d, segno %o, phyaddr %08o\n", 
        // libIdx, KST [libIdx] . segno, lookupSegAddrByIdx (libIdx));

        initStack (stack0Idx + 0);
        createFrame (stack0Idx + 0, 077777, 1, 0);
        initStack (stack0Idx + FXE_RING);
        createFrame (stack0Idx + FXE_RING, KST [stack0Idx] . segno, STK_TOP, 0);



// AK92, pg 2-13: PR0 points to the argument list

        // Create the argument list.

        // Creating in memory
        //     arg1 len
        //     arg1
        //      ..
        //     argn len
        //     argn
        //     desc1
        //      ..
        //     descn
        //     arg list
        //        nargs, type
        //        desc_cnt
        //        arg1 ptr
        //      ..
        //        argn ptr
        //        desc1 ptr
        //      ..
        //        descn ptr

        word24 fxeMemPtr = fxeEntry -> physmem;
        word18 next = 0;

        //word36 argList = fxeMemPtr + next;
        word18 argAddrs [10];

        for (uint i = 0; i < nargs; i ++)
          {
            argAddrs [i] = next;
            strcpyNonVarying (fxeMemPtr, & next, args [i]);
          }

        word18 descAddrs [10];
        word36 descList = fxeMemPtr + next;
        for (uint i = 0; i < nargs; i ++)
          {
            descAddrs [i] = next + i;
            M [descList + i] = 0;
            putbits36 (M + descList + i, 0, 1, 1); // flag
            putbits36 (M + descList + i, 1, 6, 21); // type non-varying character string
            putbits36 (M + descList + i, 7, 1, 0); // unpacked
            putbits36 (M + descList + i, 8, 4, 0); // 0 dims
            putbits36 (M + descList + i, 12, 24, strlen (args [i])); // 0 dims
          }
        next += nargs;

        
        word36 argBlock = fxeMemPtr + next;

// Function: The cu_$arg_ptr entry point is used by a command or
// subroutine that can be called with a varying number of arguments,
// each of which is a variable-length unaligned character string
// (i.e., declared char(*)).  This entry point returns a pointer to
// the character-string argument specified by the argument number and
// also returns the length of the argument.

        // word 0  arg_count, call_type
        M [argBlock + 0] = 0;
        putbits36 (M + argBlock +  0,  0, 17, nargs);
        putbits36 (M + argBlock +  0, 18, 18, 4); // inter-segment call

        // word 1  desc_count, 0
        M [argBlock + 1] = 0;
        putbits36 (M + argBlock +  1,  0, 17, nargs);

        next += 2;

        // List of arg ptrs

        word36 argPtrList = fxeMemPtr + next;
        for (uint i = 0; i < nargs; i ++)
          {
            makeITS (M + argPtrList + 2 * i, FXE_SEGNO, FXE_RING,
                     argAddrs [i], 0, 0);
          }

        next += nargs * 2;

        // List of descriptors

        word36 descPtrList = fxeMemPtr + next;
        for (uint i = 0; i < nargs; i ++)
          {
            makeITS (M + descPtrList + 2 * i, FXE_SEGNO, FXE_RING,
                     descAddrs [i], 0, 0);
          }
        next += 2 * nargs;

#if 0
        for (uint i = 0; i < next; i ++)
          {
            sim_printf ("%3o %012llo\n", i, M [fxeMemPtr + i]);
          }
#endif

        PR [0] . SNR = FXE_SEGNO;
        PR [0] . RNR = FXE_RING;
        PR [0] . BITNO = 0;
        PR [0] . WORDNO = (word18) (argBlock - fxeMemPtr);

// AK92, pg 2-13: PR4 points to the linkage section for the executing procedure

        PR [4] . SNR = KST [segIdx] . segno;
        PR [4] . RNR = FXE_RING;
        PR [4] . BITNO = 0;
        PR [4] . WORDNO = KST [segIdx] . linkage_offset;

// AK92, pg 2-13: PR7 points to the stack frame

//        PR [6] . SNR = 077777;
//        PR [6] . RNR = FXE_RING;
//        PR [6] . BITNO = 0;
//        PR [6] . WORDNO = 0777777;
        PR [6] . SNR = STACKS_SEGNO + FXE_RING;
        PR [6] . RNR = FXE_RING;
        PR [6] . BITNO = 0;
        PR [6] . WORDNO = STK_TOP;

// AK92, pg 2-10: PR7 points to the base of the stack segement

        PR [7] . SNR = STACKS_SEGNO + FXE_RING;
        PR [7] . RNR = FXE_RING;
        PR [7] . BITNO = 0;
        PR [7] . WORDNO = 0;


        set_addr_mode (APPEND_mode);
        //PPR . IC = KST [segIdx] . entry;
        PPR . IC = entryOffset;
        PPR . PRR = FXE_RING;
        PPR . PSR = KST [segIdx] . segno;
        PPR . P = 0;
        DSBR . STACK = KST [stack0Idx + FXE_RING] . segno >> 3;
      }
    for (int i = 0; i < maxargs; i ++)
      free (args [i]);

#ifndef SPEED
    if_sim_debug (DBG_TRACE, & fxe_dev)
      sim_printf ("Starting execution...\n");
#endif
    run_cmd (RU_CONT, "");
#endif
    return SCPE_OK;
  }

//
// Fault handler
//

static bool c6tValid = false;
static word1 c6tPPR_P;
static word3 c6tPPR_PRR;
static word15 c6tPPR_PSR;
static word18 c6tPPR_IC;

void fxeSetCall6Trap (void)
  {
    c6tPPR_P = PPR . P;
    c6tPPR_PRR = PPR . PRR;
    c6tPPR_PSR = PPR . PSR;
    c6tPPR_IC = PPR . IC;
    c6tValid = true;
  }

void fxeCall6TrapRestore (void)
  {
    word18 value;
    word15 segno;
    sgIdx idx;
    if (! resolveName ("pl1_operators_", "alm_return_no_pop", 
                       & segno, & value, & idx))
      {
        sim_printf ("ERROR: can't find alm_return_no_pop\n");
        exit (1);
        //return;
      }
    PPR . PSR = segno;
    PPR . IC = value; // + 441; // return_main
  }

static void faultTag2Handler (void)
  {
    // The fault pair saved the SCU data @ 0200

    // Get the offending address from the SCU data (This is an address
    // in the CLR copy of the linkage section)

    word18 offset = GETHI (M [0200 + 5]);
    word15 segno = GETHI (M [0200 + 2]) & MASK15;
    //sim_printf ("f2 fault %05o:%06o\n", segno, offset);

    if (segno != CLR_SEGNO)
      {
        sim_printf ("ERROR: expected clr segno %05o, found %05o\n", 
                    CLR_SEGNO, segno);
        return;
      }
    // Get the physmem address of the segment
    word36 * even = M + DESCSEG + 2 * segno + 0;  
    //word36 * odd  = M + DESCSEG + 2 * segno + 1;  
    word24 segBaseAddr = getbits24 (* even, 0);
    word24 addr = (segBaseAddr + offset) & MASK24;
    //sim_printf ("addr %08o:%012llo\n", addr, M [addr]);

    link_ * l = (link_ *) (M + addr);

    // Make a copy of it so that when we overwrite it, we don't get confused
    link_ linkCopy = * l;

    if (linkCopy . tag != 046)
      {
        sim_printf ("ERROR: f2 handler dazed and confused. not f2? %012llo\n", M [addr]);
        return;
      }

    if (linkCopy . run_depth != 0)
      sim_printf ("WARNING:  run_depth wrong %06o\n", linkCopy . run_depth);

    // Find the copied linkage header
    // sim_printf ("header_relp %08o\n", linkCopy . header_relp);
    word24 cpLinkHeaderOffset = (offset + SIGNEXT18_24 (linkCopy . header_relp)) & MASK24;
    // sim_printf ("headerOffset %08o\n", linkHeaderOffset);
    link_header * cplh = (link_header *) (M + segBaseAddr + cpLinkHeaderOffset);

    // Find the original linkage header

    word15 linkHeaderSegno = GET_ITS_SEGNO (cplh -> original_linkage_ptr);
    word24 linkHeaderOffset = GET_ITS_WORDNO (cplh -> original_linkage_ptr);
    //sim_printf ("orig link @ %05o:%06o\n", linkHeaderSegno, linkHeaderOffset);
    word24 linkSegBaseAddr = KST [segnoMap [linkHeaderSegno]] . physmem;

    link_header * lh = (link_header *) (M + linkSegBaseAddr + linkHeaderOffset);
    // Find the definition header
    word36 * defBase = M + linkSegBaseAddr + lh -> def_offset;
    //sim_printf ("defs_in_link %o\n", lh -> defs_in_link);
    //sim_printf ("def_offset %o\n", lh -> def_offset);
    //sim_printf ("first_ref_relp %o\n", lh -> first_ref_relp);
    //sim_printf ("link_begin %o\n", lh -> link_begin);
    //sim_printf ("linkage_section_lng %o\n", lh -> linkage_section_lng);
    //sim_printf ("segno_pad %o\n", lh -> segno_pad);
    //sim_printf ("static_length %o\n", lh -> static_length);
    
    //sim_printf ("ringno %0o\n", linkCopy . ringno);
    //sim_printf ("run_depth %02o\n", linkCopy . run_depth);
    //sim_printf ("expression_relp %06o\n", linkCopy . expression_relp);
       
    expression * expr = (expression *) (defBase + linkCopy . expression_relp);

    //sim_printf ("  type_ptr %06o  exp %6o\n", 
                //expr -> type_ptr, expr -> exp);

    type_pair * typePair = (type_pair *) (defBase + expr -> type_ptr);

    switch (typePair -> type)
      {
        case 1:
          //sim_printf ("    1: self-referencing link\n");
          sim_printf ("ERROR: unhandled type %d\n", typePair -> type);
          return;

        case 3:
          {
            word15 refSegno;
            word18 refValue;
            sgIdx defIdx;

            //sim_printf ("    3: referencing link with offset\n");
            //sim_printf ("      seg %s\n", sprintACC (defBase + typePair -> seg_ptr));
            char * segStr = strdup (sprintACC (defBase + typePair -> seg_ptr));
            if (resolveName (segStr, NULL, & refSegno, & refValue, & defIdx))
              {
#ifndef SPEED
                if_sim_debug (DBG_TRACE, & fxe_dev)
                  sim_printf ("FXE: snap (3) %s %05o:%06o\n", segStr, refSegno, refValue);
#endif
                makeITS (M + addr, refSegno, linkCopy . ringno, refValue, 0, 
                         linkCopy . modifier);
                free (segStr);
                doRCU (false); // doesn't return
              }
            else
              {
                sim_printf ("ERROR: tag2 type 3 can't resolve %s\n", segStr);
              }

            free (segStr);
          }
          break;


        case 4:
          {
            word15 refSegno;
            word18 refValue;
            sgIdx defIdx;

            //sim_printf ("    4: referencing link with offset\n");
            //sim_printf ("      seg %s\n", sprintACC (defBase + typePair -> seg_ptr));
            //sim_printf ("      ext %s\n", sprintACC (defBase + typePair -> ext_ptr));
            char * segStr = strdup (sprintACC (defBase + typePair -> seg_ptr));
            char * extStr = strdup (sprintACC (defBase + typePair -> ext_ptr));
            if (resolveName (segStr, extStr, & refSegno, & refValue, & defIdx))
              {
#ifndef SPEED
                if_sim_debug (DBG_TRACE, & fxe_dev)
                  sim_printf ("FXE: snap (4) %s$%s %05o:%06o\n", segStr, extStr, refSegno, refValue);
#endif
                makeITS (M + addr, refSegno, linkCopy . ringno, refValue, 0, 
                         linkCopy . modifier);
                free (segStr);
                free (extStr);
                doRCU (false); // doesn't return
              }
            else
              {
                sim_printf ("ERROR: tag2 type 4 can't resolve %s:%s\n", segStr, extStr);
              }

            free (segStr);
            free (extStr);
            //sgIdx segIdx = loadSegmentFromFile (sprintACC (defBase + typePair -> seg_ptr));
          }
          break;

        case 5:
          {
            //sim_printf ("    5: self-referencing link with offset\n");
            //sim_printf ("      seg %d\n", typePair -> seg_ptr);
            //sim_printf ("      ext %s\n", sprintACC (defBase + typePair -> ext_ptr));

// XXX ext contains the name of the external variable; it is unclear
// how this name should be used.
            if (typePair -> seg_ptr != 5)
              {
                sim_printf ("ERROR: dont grok seg_ptr (%d) != 5\n",
                            typePair -> seg_ptr);
                break;
              }        

            initialization_info * iip = 
              (initialization_info *) (defBase + typePair -> trap_ptr);

            word36 n_words = iip -> n_words;
            //sim_printf ("n words %lld code %lld\n", n_words, iip -> code);

            word18 variableOffset = clrFreePtr;
            clrFreePtr += iip -> n_words;

            if (iip -> code == 0)
              {
                // no initialization
              }
            else if (iip -> code == 2)
              {
                // copy info array
                KSTEntry * clrEntry = KST + clrIdx;
                word36 * clrMemory = M + clrEntry -> physmem;
                word36 * to = clrMemory + variableOffset;

                for (uint i = 0; i < (uint) n_words; i ++)
                  to [i] = iip -> info [i]; 
              }
            else
              {
                sim_printf ("ERROR: dont grok code (%lld) != 0 or 2\n",
                            iip -> n_words);
                break;
              }
                //sim_printf ("FXE: snap (5) clr_$%s %05o:%06o\n", sprintACC (defBase + typePair -> ext_ptr), CLR_SEGNO, variableOffset);
            makeITS (M + addr, CLR_SEGNO, linkCopy . ringno, variableOffset, 0, 
                     linkCopy . modifier);
            doRCU (false); // doesn't return
          }

        default:
          sim_printf ("ERROR: unknown type %d\n", typePair -> type);
          return;
      }


    //sim_printf ("    type %06o\n", typePair -> type);
    //sim_printf ("    trap_ptr %06o\n", typePair -> trap_ptr);
    //sim_printf ("    seg_ptr %06o\n", typePair -> seg_ptr);
    //sim_printf ("    ext_ptr %06o\n", typePair -> ext_ptr);
  }

typedef struct argTableEntry
  {
    word6 dType;
    word24 argAddr;
    word24 descAddr;
    word24 dSize;
  } argTableEntry;

#define ARG1 0
#define ARG2 1
#define ARG3 2
#define ARG4 3
#define ARG5 4
#define ARG6 5
#define ARG7 6
#define DESC_PTR 13
#define DESC_ENTRY 16
#define DESC_BITS 19
#define DESC_FIXED 1
#define DESC_CHAR_SPLAT 21

static int processArgs (uint nargs, int ndescs, argTableEntry * t)
  {
    // Get the argument pointer
    word15 apSegno = PR [0] . SNR;
    word18 apWordno = PR [0] . WORDNO;
    //sim_printf ("ap: %05o:%06o\n", apSegno, apWordno);

    // Find the argument list in memory
    sgIdx alIdx = segnoMap [apSegno];
    word24 alPhysmem = KST [alIdx] . physmem + apWordno;

    // XXX 17s below are not typos.
    word18 arg_count  = (word18) (getbits36 (M [alPhysmem + 0],  0, 17));
    word18 call_type  = (word18) (getbits36 (M [alPhysmem + 0], 18, 18));
    word18 desc_count = (word18) (getbits36 (M [alPhysmem + 1],  0, 17));
    //sim_printf ("arg_count %u\n", arg_count);
    //sim_printf ("call_type %u\n", call_type);
    //sim_printf ("desc_count %u\n", desc_count);

    // Error checking
    if (call_type != 4)
      {
        sim_printf ("ERROR: call_type %d not handled\n", call_type);
        return 0;
      }

    if (desc_count && desc_count != arg_count)
      {
        sim_printf ("ERROR: arg_count %d != desc_count %d\n", 
                    arg_count, desc_count);
        return 0;
      }

    if (arg_count != nargs)
      {
        sim_printf ("ERROR: expected %d args, got %d\n",
                     nargs, (int) arg_count);
        return 0;
      }

    if ((int) desc_count != ndescs)
      {
        sim_printf ("ERROR: expected %d descs, got %d\n", 
                    ndescs, (int) desc_count);
        return 0;
      }

    uint alOffset = 2;
    uint dlOffset = alOffset + nargs * 2u;

    for (uint i = 0; i < nargs; i ++)
      {
        t [i] . argAddr = 
          ITSToPhysmem (M + alPhysmem + alOffset + i * 2, NULL);
        if (ndescs)
          {
            t [i] . descAddr = 
              ITSToPhysmem (M + alPhysmem + dlOffset + i * 2, NULL);
            word6 dt = (word6) (getbits36 (M [t [i] . descAddr], 1, 6));
            if (dt != t [i] . dType)
              {
                // Hack: Treat pointer and entry as synonyms
                if (! (dt == DESC_ENTRY && t [i] . dType == DESC_PTR))
                  {
                    sim_printf ("ERROR: expected d%dType %u, got %u\n", 
                                i + 1, t [i] . dType, dt);
                    return 0;
                  }
              }
            if (dt == DESC_CHAR_SPLAT)
              {
                t [i] . dSize = GET24 (M [t [i] . descAddr]);
              }
          }
        else
         {
            t [i] . descAddr = MASK24;
         }
      }
    return 1;
  }

static void trapFXE_PutChars (void)
  {
    // declare iox_$put_chars entry (ptr, ptr, fixed bin(21), fixed bin(35));

    argTableEntry t [4] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (4, 0, t))
      return;

    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;
    word24 ap3 = t [ARG3] . argAddr;

    word24 iocbPtr = ITSToPhysmem (M + ap1, NULL);
    word24 iocb0 = KST [segnoMap [IOCB_SEGNO]] . physmem;
    uint iocbOffset = (iocbPtr - iocb0) / sizeof (iocb);
    // sim_printf ("iocbOffset %u\n", iocbOffset);
    if (iocbOffset >= N_IOCBS)
      {
        //sim_printf ("ERROR: iocbOffset (%d) >= N_IOCBS (%d)\n", iocbOffset, N_IOCBS);
        M [t [ARG4] . argAddr] = lookupErrorCode ("not_a_valid_iocb");
        doRCU (true); // doesn't return
      }

    word24 bufPtr = ITSToPhysmem (M + ap2, NULL);
  
    word36 len = M [ap3];

    //sim_printf ("PUT_CHARS:");
    for (uint i = 0; i < len; i ++)
      {
        uint woff = i / 4;
        uint chno = i % 4;
        word36 ch = getbits36 (M [bufPtr + woff], chno * 9, 9);
        sim_printf ("%c", (char) (ch & 0177U));
      }

    M [t [ARG4] . argAddr] = 0;
    doRCU (true); // doesn't return
  }

static uint getline_ (char *buf, uint n)
  {
    uint whence = 0;
    buf [whence] = 0;
    while (1)
      {
        t_stat c;
        c = sim_poll_kbd ();
        if (c == SCPE_OK)
          {
            usleep (100000); // 1/10 of a second
            continue;
          }

        //sim_printf ("\n%o %o\n", c, c - SCPE_KFLAG);

        if (c == SCPE_STOP)
          {
            // XXX deliver break to multics?
            trapFXE_UnhandledSignal ();
            break;           
          }
        c -= SCPE_KFLAG;    // translate to ascii

        if (c == '\003') // control-c
          {
            // XXX deliver break to multics?
            trapFXE_UnhandledSignal ();
            break;           
          }
        if (c == '\004') // control-d
          {
            // XXX deliver break to multics?
            trapFXE_UnhandledSignal ();
            break;           
          }
        if (c == '\177' || c == '\010') // backspace, delete
          {
            if (whence > 0)
              whence --;
            sim_printf ("^H");
            continue;
          }
        if (c == '\012' || c == '\015') // cr, lf
          {
            if (whence >= n - 1)
              break;
            buf [whence ++] = (char) c;
            buf [whence] = 0;
            //sim_printf ("%c", (char) c);
            sim_printf ("\n");
            break;
          }
        if (whence >= n - 1)
          continue;
        buf [whence ++] = (char) c;
        buf [whence] = 0;
        sim_printf ("%c", (char) c);
      }
    return whence;
  }

static void trapFXE_Control (void)
  {

// :Entry:  control:  01/24/84 iox_$control
// 
// 
// Function: performs a specified control order on an I/O switch.  The
// allowed control orders depend on the attachment of the switch.  For
// details on control orders, see the description of the particular I/O
// module used in the attach operation.
// 
// 
// Syntax:
// declare iox_$control entry (ptr, char(*), ptr, fixed bin(35));
// call iox_$control (iocb_ptr, order, info_ptr, code);
// 
// Arguments:
// iocb_ptr
//    points to the switch's control block.  (Input)
// order
//    is the name of the control order.  (Input)
// info_ptr
//    is null or points to data whose form depends on the attachment.
//    (Input)
// 
// code
//    is an I/O system status code.  (Output)
//    error_table_$no_operation
//       is returned if the switch is open for a control order which is
//       not supported for a particular attachment, or is returned by I/O
//       modules that support orders with the switch closed.
//    error_table_$not_open
//       is returned if the switch is closed.


    argTableEntry t [4] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (4, 4, t))
      return;

#if 0
    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;
    word24 ap3 = t [ARG3] . argAddr;
    word24 ap4 = t [ARG4] . argAddr;

    word24 iocbPtr = ITSToPhysmem (M + ap1, NULL);
    word24 iocb0 = KST [segnoMap [IOCB_SEGNO]] . physmem;
    uint iocbOffset = (iocbPtr - iocb0) / sizeof (iocb);
    //sim_printf ("iocbOffset %u\n", iocbOffset);
    if (iocbOffset >= N_IOCBS)
      {
        M [t [ARG5] . argAddr] = lookupErrorCode ("not_a_valid_iocb");
        doRCU (true); // doesn't return
      }
#endif

    word24 code = 0;

    // arg 4: code
    word24 codePtr = t [ARG4] . argAddr;
    M [codePtr] = code;

    doRCU (true); // doesn't return
  }

static void trapFXE_GetLine (void)
  {
    //  entry (ptr, ptr, fixed (21), fixed (21), fixed bin (35)),
    // get_line(iocb,bufptr,buflen,actlen,code) */

    argTableEntry t [5] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (5, 0, t))
      return;
    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;
    word24 ap3 = t [ARG3] . argAddr;
    word24 ap4 = t [ARG4] . argAddr;
    //word24 ap5 = t [ARG5] . argAddr;

    word24 iocbPtr = ITSToPhysmem (M + ap1, NULL);
    word24 iocb0 = KST [segnoMap [IOCB_SEGNO]] . physmem;
    uint iocbOffset = (iocbPtr - iocb0) / sizeof (iocb);
    //sim_printf ("iocbOffset %u\n", iocbOffset);
    if (iocbOffset >= N_IOCBS)
      {
        M [t [ARG5] . argAddr] = lookupErrorCode ("not_a_valid_iocb");
        doRCU (true); // doesn't return
      }

    word24 bufPtr = ITSToPhysmem (M + ap2, NULL);
  
    word36 len = M [ap3];
    if (len < 1 || len > 2048)
      {
        sim_printf ("WARNING: patching len from %lld to 128\n", len);
        len = 128;
      }

//{
//stack_header * sh = (stack_header *) (KST[stack0Idx+5].physmem);
//sim_printf ("signal ptr %012llo %012llo\n", sh -> signal_ptr [0], sh -> signal_ptr [1]);
//}
    sim_printf ("GET_LINE: ");
    char buf [len + 1];
    uint actlen = getline_ (buf, (uint) len + 1);

    for (uint i = 0; i < actlen; i ++)
      {
        uint woff = i / 4;
        uint chno = i % 4;
        putbits36 (M + bufPtr + woff, chno * 9, 9, (word36) buf [i]);
      }

    M [ap4] = (word36) actlen;
    M [t [ARG5] . argAddr] = 0;

    doRCU (true); // doesn't return
  }

#if 0
static void trapModes (void)
  {
    // declare iox_$modes entry (ptr, char(*), char(*), fixed bin(35));

    // Get the argument pointer
    word15 apSegno = PR [0] . SNR;
    word15 apWordno = PR [0] . WORDNO;
    //sim_printf ("ap: %05o:%06o\n", apSegno, apWordno);

    // Find the argument list in memory
    sgIdx alIdx = segnoMap [apSegno];
    word24 alPhysmem = KST [alIdx] . physmem + apWordno;

    // XXX 17s below are not typos.
    word18 arg_count  = getbits36 (M [alPhysmem + 0],  0, 17);
    word18 call_type  = getbits36 (M [alPhysmem + 0], 18, 18);
    word18 desc_count = getbits36 (M [alPhysmem + 1],  0, 17);
    sim_printf ("arg_count %u\n", arg_count);
    sim_printf ("call_type %u\n", call_type);
    sim_printf ("desc_count %u\n", desc_count);

    // Error checking
    if (call_type != 4)
      {
        sim_printf ("ERROR: call_type %d not handled\n", call_type);
        return;
      }

    if (desc_count && desc_count != arg_count)
      {
        sim_printf ("ERROR: arg_count %d != desc_count %d\n", 
                    arg_count, desc_count);
        return;
      }

    if (arg_count != 4)
      {
        sim_printf ("ERROR: put_chars expected 4 args, got %d\n", arg_count);
        return;
      }

    if (desc_count != 4)
      {
        sim_printf ("ERROR: put_chars expected 4 descs, got %d\n", arg_count);
        return;
      }

    sim_printf ("ap1 %012llo %012llo\n", M [alPhysmem +  2],
                                         M [alPhysmem +  3]);
    sim_printf ("ap2 %012llo %012llo\n", M [alPhysmem +  4],
                                         M [alPhysmem +  5]);
    sim_printf ("ap3 %012llo %012llo\n", M [alPhysmem +  6],
                                         M [alPhysmem +  7]);
    sim_printf ("ap4 %012llo %012llo\n", M [alPhysmem +  8],
                                         M [alPhysmem +  9]);

    sim_printf ("dp1 %012llo %012llo\n", M [alPhysmem + 10],
                                         M [alPhysmem + 11]);
    sim_printf ("dp2 %012llo %012llo\n", M [alPhysmem + 12],
                                         M [alPhysmem + 13]);
    sim_printf ("dp3 %012llo %012llo\n", M [alPhysmem + 14],
                                         M [alPhysmem + 15]);
    sim_printf ("dp4 %012llo %012llo\n", M [alPhysmem + 16],
                                         M [alPhysmem + 17]);

    // Process the arguments

    word24 ap1 = ITSToPhysmem (M + alPhysmem +  2, NULL);
    word24 ap2 = ITSToPhysmem (M + alPhysmem +  4, NULL);
    word24 ap3 = ITSToPhysmem (M + alPhysmem +  6, NULL);
    word24 ap4 = ITSToPhysmem (M + alPhysmem +  8, NULL);

    word24 dp1 = ITSToPhysmem (M + alPhysmem + 10, NULL);
    word24 dp2 = ITSToPhysmem (M + alPhysmem + 12, NULL);
    word24 dp3 = ITSToPhysmem (M + alPhysmem + 14, NULL);
    word24 dp4 = ITSToPhysmem (M + alPhysmem + 16, NULL);

    sim_printf ("ap1 %08o\n", ap1);
    sim_printf ("ap2 %08o\n", ap2);
    sim_printf ("ap3 %08o\n", ap3);
    sim_printf ("ap4 %08o\n", ap4);

    sim_printf ("dp1 %08o\n", dp1);
    sim_printf ("dp2 %08o\n", dp2);
    sim_printf ("dp3 %08o\n", dp3);
    sim_printf ("dp4 %08o\n", dp4);

    sim_printf ("@ap1 %012llo\n", M [ap1]);
    sim_printf ("@dp1 %012llo\n", M [dp1]);
  
    word24 iocbPtr = ITSToPhysmem (M + ap1, NULL);
    sim_printf ("iocbPtr %08o\n", iocbPtr);

    word24 iocb0 = KST [segnoMap [IOCB_SEGNO]] . physmem;

    uint iocbOffset = (iocbPtr - iocb0) / sizeof (iocb);
    //sim_printf ("iocbOffset %u\n", iocbOffset);
    if (iocbOffset != 0)
      {
        sim_printf ("ERROR: iocbOffset (%d) != 0\n", iocbOffset);
        return;
      }

    sim_printf ("@ap2 %012llo\n", M [ap2]);
    sim_printf ("@dp2 %012llo\n", M [dp2]);
    sim_printf ("desc2 type %lld\n", getbits36 (M [dp2], 1, 6));
    sim_printf ("desc2 packed %lld\n", getbits36 (M [dp2], 7, 1));
    sim_printf ("desc2 size %lld\n", getbits36 (M [dp2], 12, 24));

    word6 newModesBitno;
    word24 newModesPtr = ITSToPhysmem (M + ap2, & newModesBitno);
    sim_printf ("newModesPtr %08o\n", newModesPtr);
    sim_printf ("@newModesPtr %012llo\n", M [newModesPtr]);
    sim_printf ("newModesBitno %u\n", newModesBitno);
    if (newModesBitno)
      {
        sim_printf ("ERROR: newModesBitno (%d) != 0\n", newModesBitno);
        return;
      }

    sim_printf ("@ap3 %012llo\n", M [ap3]);
    sim_printf ("@dp3 %012llo\n", M [dp3]);
    sim_printf ("desc3 type %lld\n", getbits36 (M [dp3], 1, 6));
    sim_printf ("desc3 packed %lld\n", getbits36 (M [dp3], 7, 1));
    sim_printf ("desc3 size %lld\n", getbits36 (M [dp3], 12, 24));
  
    word6 oldModesBitno;
    word24 oldModesPtr = ITSToPhysmem (M + ap2, & oldModesBitno);
    sim_printf ("oldModesPtr %08o\n", oldModesPtr);
    sim_printf ("@oldModesPtr %012llo\n", M [oldModesPtr]);
    sim_printf ("oldModesBitno %u\n", oldModesBitno);
    if (oldModesBitno)
      {
        sim_printf ("ERROR: oldModesBitno (%d) != 0\n", oldModesBitno);
        return;
      }

    //word36 status = M [ap4];

    //sim_printf ("status %012llo\n", status);

    doRCU (true); // doesn't return
  }
#endif

static void trapGetSegPtr (void)
  {
    // get_seg_ptr: entry (Name) returns (ptr);
    // dcl  Name char (32) aligned parameter;

    argTableEntry t [2] =
      {
        { DESC_FIXED, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 }
      };

    if (! processArgs (2, 0, t))
      return;

    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;
    char name [33];
    strcpyC (ap1, 32, name);
    //sim_printf ("name %s\n", name);
    trimTrailingSpaces (name);
    sgIdx idx = getSLTEidx (name);

    makeITS (M + ap2, SLTE  [idx] . segno, PPR . PRR, 0, 0, 0);
    doRCU (true); // doesn't return
  }

#if 0
static void trapGetSegment (void)
  {
///*  The get_segment entry returns a pointer to segment dirname>ename.  The
//     segment will have "rw" access to the current user.  If an old acl had to be
//     changed to do this, aclinfop is set pointing to information for reseting
//     the acl. */
// get_segment:
//        entry(dirname, ename, segp, aclinfop, code);
// dcl
//         dirname char(*),
//         ename char(*),
//         segp ptr,
//         aclinfop ptr,
//         code fixed bin(35),


    argTableEntry t [5] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (5, 5, t))
      return;

    word24 code = 0;

    // Argument 1: dirname (input)
    word24 ap1 = t [ARG1] . argAddr;
    word24 dp1 = t [ARG1] . descAddr;
    word24 d1size = getbits36 (M [dp1], 12, 24);

    char * dirname = malloc (d1size + 1);
    strcpyC (ap1, d1size, dirname);
    trimTrailingSpaces (dirname);

    // Argument 2: ename (input)
    word24 ap2 = t [ARG2] . argAddr;
    word24 dp2 = t [ARG2] . descAddr;
    word24 d2size = getbits36 (M [dp2], 12, 24);

    char * ename = malloc (d2size + 1);
    strcpyC (ap2, d2size, ename);
    trimTrailingSpaces (ename);

    //sim_printf ("%s>%s\n", dirname, ename);

//  call hcs_$make_seg(dir,enm,"",01100b,segp,code); /* try to make seg */

    //word6 mode = 014;
    word6 mode = 012;

    // Is the segment known?

    word36 ptr [2];
    sgIdx idx = lookupSegname (ename);
    if (idx >= 0)
      {
        KSTEntry * e = KST + idx;
        makeITS (ptr, e -> segno, e -> R1, 0, 0, 0);
        code = lookupErrorCode ("namedup");
        goto done;
      }

    // Does the segment exist?
    idx = loadSegment (ename); // XXX ignoring dir_name
    if (idx >= 0)
      {
        //sim_printf ("getSegment likes loadSegment\n");
        // XXX ignoring term_$nomakeunknown
        sim_printf ("WARNING: ignoring term_$nomakeunknown\n");
        // hcs_$truncate_seg (segp, 0, code)
        //sim_printf ("get_segment  zeroing segno %05o seglen %07o\n", KST [idx] . segno, KST [idx] . seglen);
        word24 physmem = KST [idx] . physmem;
        for (uint i = 0; i < KST [idx] . seglen; i ++)
          M [physmem + i] = 0;
      }
    else
      {
         //sim_printf ("getSegment likes allocateSegment\n");
         // No, create from whole cloth
         idx = allocateSegment (MAX_SEGLEN, ename, allocateSegno (), 
                                RINGS_ZFF, 
                                (mode & 010) ? 1 : 0,
                                (mode & 004) ? 1 : 0,
                                (mode & 002) ? 1 : 0,
                                0);
         if (idx < 0)
           {
             code = lookupErrorCode ("noalloc");
             goto done;
           }
      }
    
    KSTEntry * e = KST + idx;
    if (! e -> segno)
      e -> segno = allocateSegno();
    installSDW (idx);

    sim_printf ("getSegment made %05o %s\n", e -> segno, e -> segname);
    makeITS (ptr, e -> segno, FXE_RING, 0, 0, 0);

done:;

    // arg 3: segp
    word24 seg_ptrPtr = t [ARG3] . argAddr;
    M [seg_ptrPtr] = ptr [0];
    M [seg_ptrPtr + 1] = ptr [1];

    // arg 4: aclinfop
    word24 aclinfopPtr = t [ARG4] . argAddr;
    makeNullPtr (& M [aclinfopPtr]); // XXX punt

    // arg 5: code
    word24 codePtr = t [ARG5] . argAddr;
    M [codePtr] = code;

    doRCU (true); // doesn't return
  }
#endif

static void trapGetType (void)
  {


    argTableEntry t [5] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (4, 4, t))
      return;

    word24 code = 0;

    // Argument 1: dirname (input)
    word24 ap1 = t [ARG1] . argAddr;
    word24 dp1 = t [ARG1] . descAddr;
    word24 d1size = GET24 (M [dp1]);

    char * dirname = malloc (d1size + 1);
    strcpyC (ap1, d1size, dirname);
    trimTrailingSpaces (dirname);

    // Argument 2: entryname (input)
    word24 ap2 = t [ARG2] . argAddr;
    word24 dp2 = t [ARG2] . descAddr;
    word24 d2size = GET24 (M [dp2]);

    char * ename = malloc (d2size + 1);
    strcpyC (ap2, d2size, ename);
    trimTrailingSpaces (ename);

    //sim_printf ("get_type %s>%s\n", dirname, ename);

    // Argument 3: type (output)
    word24 ap3 = t [ARG3] . argAddr;
    //word24 dp3 = t [ARG3] . descAddr;
    //word24 d3size = getbits36 (M [dp3], 12, 24);
    strcpyNonVarying (ap3, NULL, "-segment");

    // arg 4: code
    word24 codePtr = t [ARG4] . argAddr;
    M [codePtr] = code;

    doRCU (true); // doesn't return
  }

static void trapHomedir (void)
  {
// declare user_info_$homedir entry (char(*));
// call user_info_$homedir (hdir);


    argTableEntry t [1] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 }
      };

    if (! processArgs (1, 1, t))
      return;

    // Argument 1: hdir (output)
    word24 ap1 = t [ARG1] . argAddr;
    word24 dp1 = t [ARG1] . descAddr;
    word24 d1size = GET24 (M [dp1]);
    strcpyNonVaryingPad (ap1, NULL, ">udd>fxe", d1size);

    doRCU (true); // doesn't return
  }

static void trapGetLineLengthSwitch (void)
  {
    // declare get_line_length_$switch entry (ptr, fixed bin(35)) returns
    //   (fixed bin);
    //     switch ptr, status code, line length

    argTableEntry t [3] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap3 = t [ARG3] . argAddr;

    uint iocbOffset;
    if (isNullPtr (ap1))
      iocbOffset = IOCB_USER_OUTPUT;
    else
      {
        word24 iocbPtr = ITSToPhysmem (M + ap1, NULL);
        //sim_printf ("iocbPtr %08o\n", iocbPtr);

        word24 iocb0 = KST [segnoMap [IOCB_SEGNO]] . physmem;

        iocbOffset = (iocbPtr - iocb0) / sizeof (iocb);
      }
    //sim_printf ("iocbOffset %u\n", iocbOffset);
    if (iocbOffset != IOCB_USER_OUTPUT)
      {
        //sim_printf ("ERROR: iocbOffset (%d) != IOCB_USER_OUTPUT (%d)\n", 
                    //iocbOffset, IOCB_USER_OUTPUT);
        M [t [ARG2] . argAddr] = lookupErrorCode ("not_a_valid_iocb");
        doRCU (true); // doesn't return
      }

    M [ap3] = 80;
    M [t [ARG2] . argAddr] = 0;

    doRCU (true); // doesn't return
  }

static void trapHCS_HistoryRegsSet (void)
  {
    //sim_printf ("trapHCS_HistoryRegsSet\n");

    doRCU (true); // doesn't return
  }

static void trapHCS_HistoryRegsGet (void)
  {
    //sim_printf ("trapHCS_HistoryRegsGet\n");

    doRCU (true); // doesn't return
  }

static void trapHCS_FSSearchGetWdir (void)
  {
    // dcl hcs_$fs_search_get_wdir ext entry(ptr,fixed bin(17));
    //   path (out), leng (out)

    argTableEntry t [2] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (2, 0, t))
      return;

    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;

    if (isNullPtr (ap1))
      {
        // Can't do anything
        M [ap2] = 0;
      }
    else
      {
        word24 resPtr = ITSToPhysmem (M + ap1, NULL);
        strcpyNonVarying (resPtr, NULL, cwd);
        M [ap2] = strlen (cwd);
      }

    doRCU (true); // doesn't return
  }

static int initiateSegment (char * dir, char * entry, 
                            word24 * bitcntp, word36 * segptrp, 
                            word15 * segnop)
  {
// XXX dir unused

    bool isTxt = strstr (entry, ".txt") != NULL;

    char upathname [UNIX_PATHNAME_LEN + 1];
    strncpy (upathname, MROOT, UNIX_PATHNAME_LEN + 1);
    strcat (upathname, ">");
    strncat (upathname, dir, UNIX_PATHNAME_LEN + 1);
    strcat (upathname, ">");
    strncat (upathname, entry, UNIX_PATHNAME_LEN + 1);
    char * blow;
    while ((blow = strchr (upathname, '>')))
      * blow = '/';
    //sim_printf ("tada> %s\n", upathname);

    int fd = open (upathname, O_RDONLY);
    if (fd < 0)
      {

// Fallback: try the UNIX cwd; maintains compatibility with earlier 
// version of FXE.
        fd = open (entry, O_RDONLY);
        if (fd < 0)
          {
            sim_printf ("ERROR: Unable to open '%s': %d\n", entry, errno);
            return -1;
          }
      }

    off_t flen = lseek (fd, 0, SEEK_END);
    lseek (fd, 0, SEEK_SET);
    
    // 8 bit ascii gets mapped to 9-bit ascii
    word24 bitcnt = (word24) ((flen * (isTxt ? 9 : 8)) & MASK24);

    word18 wordcnt = nbits2nwords (bitcnt);
    sgIdx segIdx = allocateSegment (wordcnt, entry, allocateSegno (),
                                  RINGS_ZFF, P_RW);
    if (segIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate segment for segment initiate\n");
        close (fd);
        return -1;
      }

    KSTEntry * e = KST + segIdx;

    memcpy (e -> pathname, dir, sizeof (pathnameBuf));
    installSDW (segIdx);

    makeITS (segptrp, e -> segno, FXE_RING, 0, 0, 0);
    * segnop = e -> segno;

    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word24 maddr = segAddr;
    uint seglen = 0;
    //word24 bitcntRead = 0;

    if (isTxt)
      {
        // 4 bytes at a time; mapped to 9 bit ASCII is one word.
        uint8 bytes [4];
        ssize_t n;
        while ((n = read (fd, bytes, 4)))
          {
            if (seglen > MAX18)
              {
                sim_printf ("ERROR: File too long\n");
                close (fd);
                return -1;
              }
            M [maddr] = 0;
            for (uint i = 0; i < n; i ++)
              {
                putbits36 (M + maddr, i * 9, 9, bytes [i]);
                //bitcntRead += 9;
              }
            maddr ++;
            seglen ++;
          }
      }
    else
      {
        // 9 bytes at a time; mapped to two 36 bit words
        uint8 bytes [9];
        ssize_t n;
        while (memset (bytes, 0, 9), (n = read (fd, bytes, 9)))
          {
            if (n != 5 && n != 9)
              {
                //sim_printf ("ERROR: initiateSegment: garbage at end of segment lost %ld\n", n);
              }
            if (seglen > MAX18)
              {
                sim_printf ("ERROR: File too long\n");
                close (fd);
                return -1;
              }
            M [maddr ++] = extr36 (bytes, 0);
            M [maddr ++] = extr36 (bytes, 1);
            seglen += 2;
          }
      }

    sim_debug (DBG_CAC, & cpu_dev,
      "initiate_segment <%s> flen %ld seglen %d bitcnt %d %d\n", 
      entry, flen, seglen, bitcnt, (bitcnt + 35) / 36);

    * bitcntp = bitcnt;
    KST [segIdx] . loaded = true;
#ifndef SPEED
    if_sim_debug (DBG_TRACE, & fxe_dev)
      sim_printf ("Loaded %u words in segment %05o %s index %d @ %08o\n", 
                  seglen, e -> segno, e -> segname, segIdx, segAddr);
#endif
    close (fd);
    return 0;
  }

static void trapHCS_InitiateCount (void)
  {
// :Entry: initiate_count: 03/08/82  hcs_$initiate_count
// 
// 
// Function: given a pathname and a reference name, causes the segment
// defined by the pathname to be made known and the given reference name
// initiated.  A segment number is assigned and returned as a pointer and
// the bit count of the segment is returned.
// 
// 
// Syntax:
// declare hcs_$initiate_count entry (char(*), char(*), char(*),
//      fixed bin(24), fixed bin(2), ptr, fixed bin(35));
// call hcs_$initiate_count (dir_name, entryname, ref_name, bit_count,
//      copy_ctl_sw, seg_ptr, code);


// Arguments:
// dir_name
//    is the pathname of the containing directory.  (Input)
// entryname
//    is the entryname of the segment.  (Input)
// ref_name
//    is the reference name.  (Input) If it is zero length, the segment is
//    initiated with a null reference name.
// bit_count
//    is the bit count of the segment.  (Output)
// copy_ctl_sw
//    is obsolete, and should be set to zero.  (Input)
// 
// seg_ptr
//    is a pointer to the segment.  (Output)
// code
//    is a storage system status code.  (Output)
// 
// 
// Notes:  The user must have nonnull access on the segment (the entryname
// argument) in order to make it known.
// 
// If entryname cannot be made known, a null pointer is returned for
// seg_ptr and the returned value of code indicates the reason for
// failure.        Thus, the usual way to test whether the call was successful
// is to check the pointer, not the code, since the code may be nonzero
// even if the segment was successfully initiated.  If entryname is
// already known to the user's process, code is returned as
// error_table_$segknown and the seg_ptr argument contains a nonnull
// pointer to entryname.  If entryname is not already known, and no
// problems are encountered, seg_ptr contains a valid pointer and code is
// 0.  If ref_name has already been initiated in the current ring, the
// code is returned as error_table_$namedup.  The seg_ptr argument
// contains a valid pointer to the segment being initiated.  If the
// seg_ptr argument contains a nonnull pointer, the bit_count argument is
// set to the bit count of the segment to which seg_ptr points.

    argTableEntry t [7] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (7, 7, t))
      return;

    // Process the arguments

    // Argument 1: dir_name (input)
    word24 ap1 = t [ARG1] . argAddr;
    word24 dp1 = t [ARG1] . descAddr;
    word24 d1size = GET24 (M [dp1]);

    char * arg1 = malloc (d1size + 1);
    strcpyC (ap1, d1size, arg1);

    // Argument 2: entry name
    word24 ap2 = t [ARG2] . argAddr;
    word24 dp2 = t [ARG2] . descAddr;

    word24 d2size = GET24 (M [dp2]);
    char * arg2 = malloc (d2size + 1);
    strcpyC (ap2, d2size, arg2);

    // Argument 3: reference name


// reference name
// Name supplied to hcs_$initiate when a segment is made known, and entered
// into the process's RNT. When a linkage fault occurs, the dynamic linker
// searches for a segment with that reference name, using the process's search
// rules. If the segment is not found by reference name, other search rules are
// used, and if a segment is found, it is initiated with the reference name
// used in the search. Thus one can issue the commands
//
//    initiate test_wankel wankel
//    mysubsustem
//
// to cause the invocation of mysubsystem to find test_wankel when it links to
// the function wankel.

    word24 ap3 = t [ARG3] . argAddr;
    word24 dp3 = t [ARG3] . descAddr;
    word24 d3size = GET24 (M [dp3]);

    char * arg3 = NULL;
    if (d3size)
      {
        arg3 = malloc (d3size + 1);
        strcpyC (ap3, d3size, arg3);
      }

    // Argument 4: bit count

    word24 ap4 = t [ARG4] . argAddr;
    word24 dp4 = t [ARG4] . descAddr;

    word24 d4size = GET24 (M [dp4]);
    if (d4size != 24)
      {
        sim_printf ("ERROR: initiate_count expected d4size 24, got %d\n", 
                    d4size);
        M [t [ARG7] . argAddr] = lookupErrorCode ("bad_arg");
        doRCU (true); // doesn't return
      }

    // Argument 6: seg ptr

    word24 ap6 = t [ARG6] . argAddr;
    word24 dp6 = t [ARG6] . descAddr;
    word24 d6size = GET24 (M [dp6]);

    if (d6size != 0)
      {
        sim_printf ("ERROR: initiate_count expected d6size 0, got %d\n", 
                    d6size);
        M [t [ARG7] . argAddr] = lookupErrorCode ("bad_arg");
        doRCU (true); // doesn't return
      }


    // Argument 7: code

    //word24 ap7 = t [ARG7] . argAddr;
    word24 dp7 = t [ARG7] . descAddr;
    word24 d7size = GET24 (M [dp7]);

    if (d7size != 35)
      {
        sim_printf ("ERROR: initiate_count expected d7size 35, got %d\n", d7size);
        M [t [ARG7] . argAddr] = lookupErrorCode ("bad_arg");
        doRCU (true); // doesn't return
      }


    word24 bitcnt;
    word36 ptr [2];

    trimTrailingSpaces (arg1);
    trimTrailingSpaces (arg2);

    // XXX Check if segment is known....

    // XXX Check if ref_name is known...

    word24 code = 0;
    word15 segno;
    int status = initiateSegment (arg1, arg2, & bitcnt, ptr, & segno);
    if (status)
      {
        sim_printf ("ERROR: initiateSegment fail\n");
        bitcnt = 0;
        makeNullPtr (ptr);
        code = lookupErrorCode ("bad_segment"); // XXX probably not the right code
      }       

    if (status == 0 && arg3)
      addRNTRef (segnoMap [segno], arg3);
    M [ap4] = bitcnt & MASK24;
    sim_debug (DBG_CAC, & cpu_dev,
               "initiateSegment setting bitcnt %d at addr %06o\n",
               bitcnt & MASK24, ap4);
    M [ap6] = ptr [0];
    M [ap6 + 1] = ptr [1];
    M [t [ARG7] . argAddr] = code;

    free (arg1);
    free (arg2);
    if (arg3)
      free (arg3);

    doRCU (true); // doesn't return
  }

static void trapHCS_Initiate (void)
  {
// 
// Function: given a pathname and a reference name, makes known the
// segment defined by the pathname, initiates the given reference name,
// and increments the count of initiated reference names for the segment.
// 
// 
// Syntax:
// declare hcs_$initiate entry (char(*), char(*), char(*), fixed bin(1),
//      fixed bin(2), ptr, fixed bin(35));
// call hcs_$initiate (dir_name, entryname, ref_name, seg_sw, copy_ctl_sw,
//      seg_ptr, code);
// 
//    
// Arguments:
// dir_name
//    is the        pathname of the containing directory.  (Input)
// entryname
//    is the entryname of the segment.  (Input)
// ref_name
//    is the reference name.  (Input) If it is zero length, the segment is
//    initiated with a null reference name.
// seg_sw
//    is the reserved segment switch.  (Input)
//    0   if no segment number has been reserved
//    1   if a segment number was reserved
// copy_ctl_sw
//    is obsolete, and should be set to zero.  (Input)
// 
// 
// seg_ptr
//    is a pointer to the segment.
//    1  if seg_sw is on (Input) 
//    0  if seg_sw is off (Output) 
// code
//    is a storage system status code.  (Output)
// 
// Notes: If a segment is concurrently initiated more than a
// system-defined number of times, the usage count of the segment is said
// to be in an overflowed condition, and further initiations do not
// affect the usage count.  This affects the use of the
// hcs_$terminate_noname and hcs_$terminate_name entry points.  If the
// reserved segment switch is on, then the segment pointer is input and
// the segment is made known with that segment number.  In this case, the
// user supplies the initial segment number.  If the reserved segment
// switch is off, a segment number is assigned and returned as a pointer.
// 
// 
// If entryname cannot be made known, a null pointer is returned for
// seg_ptr and the returned value of code indicates the reason for
// failure.        Thus, the usual way to test whether the call was successful
// is to check the pointer, not the code, since the code may be nonzero
// even if the segment was successfully initiated.  If entryname is
// already known to the user's process, code is returned as
// error_table_$segknown and the seg_ptr argument contains a nonnull
// pointer to entryname.  If ref_name has already been initiated in the
// current ring, the code is returned as error_table_$namedup.      The
// seg_ptr argument contains a valid pointer to the segment being
// initiated.  If entryname is not already known, and no problems are
// encountered, seg_ptr contains a valid pointer and code is 0.
// 


    argTableEntry t [7] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (7, 7, t))
      return;

    // Process the arguments

    // Argument 1: dir_name (input)
    word24 ap1 = t [ARG1] . argAddr;
    word24 dp1 = t [ARG1] . descAddr;
    word24 d1size = GET24 (M [dp1]);

    char * arg1 = malloc (d1size + 1);
    strcpyC (ap1, d1size, arg1);

    // Argument 2: entry name
    word24 ap2 = t [ARG2] . argAddr;
    word24 dp2 = t [ARG2] . descAddr;

    word24 d2size = GET24 (M [dp2]);
    char * arg2 = malloc (d2size + 1);
    strcpyC (ap2, d2size, arg2);

    // Argument 3: reference name


// reference name
// Name supplied to hcs_$initiate when a segment is made known, and entered
// into the process's RNT. When a linkage fault occurs, the dynamic linker
// searches for a segment with that reference name, using the process's search
// rules. If the segment is not found by reference name, other search rules are
// used, and if a segment is found, it is initiated with the reference name
// used in the search. Thus one can issue the commands
//
//    initiate test_wankel wankel
//    mysubsustem
//
// to cause the invocation of mysubsystem to find test_wankel when it links to
// the function wankel.

    word24 ap3 = t [ARG3] . argAddr;
    word24 dp3 = t [ARG3] . descAddr;
    word24 d3size = GET24 (M [dp3]);

    char * arg3 = NULL;
    if (d3size)
      {
        arg3 = malloc (d3size + 1);
        strcpyC (ap3, d3size, arg3);
      }

    // Argument 4: reserved segment switch

    word24 ap4 = t [ARG4] . argAddr;
    word24 dp4 = t [ARG4] . descAddr;

    word24 d4size = GET24 (M [dp4]);
    if (d4size != 17)
      {
        sim_printf ("ERROR: initiate_count expected d4size 17, got %d\n", 
                    d4size);
        M [t [ARG7] . argAddr] = lookupErrorCode ("bad_arg");
        doRCU (true); // doesn't return
      }

    word24 seg_sw = (word24) getbits36 (M [ap4], 36 - 17, 17);
    if (seg_sw)
      {
        sim_printf ("ERROR: can't grok seg_sw != 0\n");
        M [t [ARG7] . argAddr] = lookupErrorCode ("bad_arg");
        doRCU (true); // doesn't return
      }
    // Argument 6: seg ptr

    word24 ap6 = t [ARG6] . argAddr;
    word24 dp6 = t [ARG6] . descAddr;
    word24 d6size = GET24 (M [dp6]);

    if (d6size != 0)
      {
        sim_printf ("ERROR: initiate_count expected d6size 0, got %d\n", 
                    d6size);
        M [t [ARG7] . argAddr] = lookupErrorCode ("bad_arg");
        doRCU (true); // doesn't return
      }


    // Argument 7: code

    //word24 ap7 = t [ARG7] . argAddr;
    word24 dp7 = t [ARG7] . descAddr;
    word24 d7size = GET24 (M [dp7]);

    if (d7size != 35)
      {
        sim_printf ("ERROR: initiate_count expected d7size 35, got %d\n", d7size);
        M [t [ARG7] . argAddr] = lookupErrorCode ("bad_arg");
        doRCU (true); // doesn't return
      }


    word24 bitcnt;
    word36 ptr [2];

    trimTrailingSpaces (arg1);
    trimTrailingSpaces (arg2);

    // XXX Check if segment is known....

    // XXX Check if ref_name is known...

    word24 code = 0;
    word15 segno;
    int status = initiateSegment (arg1, arg2, & bitcnt, ptr, & segno);
    if (status)
      {
        sim_printf ("ERROR: initiateSegment fail\n");
        bitcnt = 0;
        makeNullPtr (ptr);
        code = lookupErrorCode ("bad_segment"); // XXX probably not the right code
      }       

    if (status == 0 && arg3)
      addRNTRef (segnoMap [segno], arg3);
    //M [ap4] = bitcnt & MASK24;
    M [ap6] = ptr [0];
    M [ap6 + 1] = ptr [1];
    M [t [ARG7] . argAddr] = code;

    free (arg1);
    free (arg2);
    if (arg3)
      free (arg3);

    doRCU (true); // doesn't return
  }

static void trapHCS_TerminateName (void)
  {
    // declare hcs_$terminate_name entry (char(*), fixed bin(35));
    // call hcs_$terminate_name (ref_name, code);

    argTableEntry t [2] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs ( 2, 2, t))
      return;

    // Process the arguments

    // Argument 1: ref name
    word24 ap1 = t [ARG1] . argAddr;
    word24 d1size = t [ARG1] . dSize;

    char * arg1 = malloc (d1size + 1);
    strcpyC (ap1, d1size, arg1);
    trimTrailingSpaces (arg1);
    //sim_printf ("arg1: '%s'\n", arg1);

    delRNTRef (arg1);


    doRCU (true); // doesn't return
  }

static void trapHCS_MakePtr (void)
  {

// :Entry: make_ptr: 03/08/82  hcs_$make_ptr
// 
// 
// Function: when given a reference name and an entry point name,
// returns a pointer to a specified entry point.  If the reference name
// has not yet been initiated, the search rules are used to find a
// segment with a name the same as the reference name.  The segment is
// made known and the reference name initiated.  Use hcs_$make_entry to
// have entry values returned.
// 
// 
// Syntax:
// declare hcs_$make_ptr entry (ptr, char(*), char(*), ptr, fixed
//      bin(35));
// call hcs_$make_ptr (ref_ptr, entryname, entry_point_name,
//      entry_point_ptr, code);
// 
// Arguments:
// ref_ptr
//   is a pointer to the segment that is considered the referencing
//    procedure.  (Input)  See "Notes" below.
// entryname
//    is the entryname of the segment.    (Input)
// entry_point_name
//    is the name of the entry point to be located.        (Input)
// entry_point_ptr
//    is the pointer to the segment entry point specified by entryname and
//    entry_point_name.  (Output)
// code
//    is a storage system status code.  (Output)
// 
// Notes:  The directory in which the segment pointed to by ref_ptr is
// located is used as the referencing directory for the standard search
// rules.  If ref_ptr is null, then the standard search rule specifying
// the referencing directory is skipped.  For a discussion of standard
// search rules, refer to "Search Rules," in the Reference Manual.
// Normally ref_ptr is null.
//
// The entryname and entry_point_name arguments are nonvarying character
// strings with a length of up to 32 characters.  They need not be aligned
// and can be blank padded.  If a null string is given for the
// entry_point_name argument, then a pointer to the base of the segment is
// returned.        In any case, the segment identified by entryname is made
// known to the process with the entryname argument initiated as a
// reference name.  If an error is encountered upon return, the
// entry_point_ptr argument is null and an error code is given.
 
    argTableEntry t [5] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (5, 5, t))
      return;


    // Process the arguments

    // Argument 1: ref name // XXX ignored
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = (word6) (t [ARG1] . dSize & MASK6);
    //sim_printf ("refer %012llo %012llo sz %o\n", M [ap1], M [ap1 + 1], d1size);
    if ((! isNullPtr (ap1)) && d1size)
      {
        sim_printf ("WARNING: make_ptr ref name ignored\n");
      }

    // Argument 2: entry name
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = (word6) (t [ARG2] . dSize & MASK6);

    char * arg2 = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        arg2 = malloc (d2size + 1);
        strcpyC (ap2, d2size, arg2);
        trimTrailingSpaces (arg2);
      }
    //sim_printf ("arg2: '%s'\n", arg2 ? arg2 : "NULL");


    // Argument 3: entry point name
    word24 ap3 = t [ARG3] . argAddr;
    word24 d3size = t [ARG3] . dSize;
    char * arg3 = NULL;
    if ((! isNullPtr (ap3)) && d3size)
      {
        arg3 = malloc (d3size + 1);
        strcpyC (ap3, d3size, arg3);
        trimTrailingSpaces (arg3);
      }

    // Argument 4: entry_point_ptr
    word24 ap4 = t [ARG4] . argAddr;

    // Argument 5: code
    word24 ap5 = t [ARG5] . argAddr;

    word15 segno;
    word18 value;
    word24 code = 0;
    int rc;
    sgIdx idx;
    word36 ptr [2];

    if (strcmp (arg2, "translator.search") == 0 && slIdx >= 0)
      {
         rc = lookupDef (slIdx, "search_list_defaults_", arg3, & value);
         //sim_printf ("lookupDef(search_list_defaults_, %s) returned %d\n", arg3, rc);
         segno = KST [slIdx] . segno;
      }
    else
      {
        rc = resolveName (arg2, arg3, & segno, & value, & idx);
      }
    if (! rc)
      {
        sim_printf ("WARNING: make_ptr resolve fail %s|%s\n", arg2, arg3);
        code = lookupErrorCode ("bad_entry_point_name");
        makeNullPtr (ptr);
      }
    else
      {
        makeITS (ptr, segno, FXE_RING, value, 0, 0);
      }
    M [ap4] = ptr [0];
    M [ap4 + 1] = ptr [1];
    M [ap5] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_StatusMins (void)
  {

// :Entry: status_mins: 03/08/82  hcs_$status_mins
// 
// 
// Function: returns the bit count and entry type given a pointer to the
// segment.  Status permission on the directory or nonnull access to the
// segment is required to use this entry point.
// Syntax:
// declare hcs_$status_mins entry (ptr, fixed bin(2), fixed bin(24),
//      fixed bin(35));
// call hcs_$status_mins (seg_ptr, type, bit_count, code);
// 
// 
// Arguments:
// seg_ptr
//    points to the segment about which information is desired.  (Input)
// type
//    specifies the type of entry.  (Output)  It can be:
//    0   link
//    1   segment
//    2   directory
// bit_count
//    is the bit count.  (Output)
// code
//    is a storage system status code.  (Output)

 
    argTableEntry t [4] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (4, 0, t))
      return;

    // Process the arguments

    // Argument 1: seg_ptr
    word24 ap1 = t [ARG1] . argAddr;
    word15 segno = GET_ITS_SEGNO (M + ap1);
    word24 code = 0;
    if (segno > N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // Argument 2: type
    word24 ap2 = t [ARG2] . argAddr;
    M [ap2] = 1; // XXX need real types

    // Argument 3: bit_count
    word24 ap3 = t [ARG3] . argAddr;
    M [ap3] = KST [idx] . seglen * 36u;
    //sim_debug (DBG_CAC, & cpu_dev, 
     //"status_mins segno %o <%s> seglen %u bitcount %u\n", 
     //segno, KST [idx] . segname, KST [idx] . seglen, KST [idx] . seglen * 36u);
done:;

    word24 ap4 = t [ARG4] . argAddr;
    M [ap4] = code;
    doRCU (true); // doesn't return
  }

static void trapHCS_MakeSeg (void)
  {

// :Entry: make_seg: 03/08/82  hcs_$make_seg
// 
// 
// Function: creates a segment with a specified entryname in a specified
// directory.  Once the segment is created, it is made known to the
// process and a pointer to the segment is returned to the caller.  If
// the segment already exists or is already known, a nonzero code is
// returned; however, a pointer to the segment is still returned.
// 
// 
// Syntax:
// declare hcs_$make_seg entry (char(*), char(*), char(*), fixed bin(5),
//      ptr, fixed bin(35));
// call hcs_$make_seg (dir_name, entryname, ref_name, mode, seg_ptr,
//      code);
// 
// 
// Arguments:
// dir_name
//    is the pathname of the containing directory.  (Input)
// entryname
//    is the entryname of the segment.  (Input)
// ref_name
//    is the desired reference name or a null character string ("").
//    (Input)
// mode
//    specifies the mode for this user.  (Input) See "Notes" in the
//    description of hcs_$append_branchx for more information on modes.
// seg_ptr
//    is a pointer to the created segment.  (Output)
// 
// 
// code
//    is a storage system status code.  (Output) It may be one of the
//    following:
//    error_table_$namedup
//         if the specified segment already exists or the specified
//         reference name has already been initiated
//    error_table_$segknown
//         if the specified segment is already known
// 
// 
// Notes: If dir_name is null, the process directory is used.  If the
// entryname is null, a unique name is generated.  The segment is made
// known and ref_name is initiated.  See also "Constructing and
// Interpreting Names" in the Reference Manual.
// 
// If the segment cannot be created or made known, a null pointer is
// returned for seg_ptr and the returned value of code indicates the
// reason for failure.      Thus, the usual way to test whether the call was
// successful is to check the pointer, not the code, since the code may be
// nonzero even if the segment was successfully initiated.
// 
 
    argTableEntry t [6] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (6, 6, t))
      return;

    word24 code = 0;

    // Process the arguments

    // Argument 1: dir_name // XXX ignored
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = (word6) (t [ARG1] . dSize & MASK6);

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = (word6) (t [ARG2] . dSize & MASK6);

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

    // Argument 3: ref_name
    word24 ap3 = t [ARG3] . argAddr;
    word6 d3size = (word6) (t [ARG3] . dSize & MASK6);

    char * ref_name = NULL;
    if ((! isNullPtr (ap3)) && d3size)
      {
        ref_name = malloc (d3size + 1);
        strcpyC (ap3, d3size, ref_name);
        trimTrailingSpaces (ref_name);
      }
    //sim_printf ("ref_name: '%s'\n", ref_name ? ref_name : "NULL");

    // Argument 4: mode
    word24 ap4 = t [ARG4] . argAddr;

    word6 mode = M [ap4] & MASK6;
    //sim_printf ("arg4: '%02o'\n", mode);

    word36 ptr [2];

    // Is the segment known?

    sgIdx idx = lookupSegname (entryname);
    if (idx >= 0)
      {
        KSTEntry * e = KST + idx;
        makeITS (ptr, e -> segno, e -> R1, 0, 0, 0);
        code = lookupErrorCode ("bad_entry_point_name");
        goto done;
      }

    // Does the segment exist?
    //idx = loadSegmentFromFile (entryname); // XXX ignoring dir_name
    idx = loadSegment (entryname); // XXX ignoring dir_name
    if (idx >= 0)
      {
        KSTEntry * e = KST + idx;
        makeITS (ptr, e -> segno, e -> R1, 0, 0, 0);
        code = lookupErrorCode ("bad_entry_point_name");
        goto done;
      }

    idx = allocateSegment (MAX_SEGLEN, entryname, allocateSegno (), 
                           RINGS_ZFF, 
                           (mode & 010) ? 1 : 0,
                           (mode & 004) ? 1 : 0,
                           (mode & 002) ? 1 : 0,
                           0);
    if (idx < 0)
      {
        code = lookupErrorCode ("noalloc");
        goto done;
      }

    KSTEntry * e = KST + idx;
    if (dir_name)
      memcpy (e -> pathname, dir_name, sizeof (pathnameBuf));
    else
      e -> pathname [0] = 0;

    installSDW (idx);

    //sim_printf ("made %05o %s\n", e -> segno, e -> segname);
    makeITS (ptr, e -> segno, FXE_RING, 0, 0, 0);

done:;
    word24 seg_ptrPtr = t [ARG5] . argAddr;
    M [seg_ptrPtr] = ptr [0];
    M [seg_ptrPtr + 1] = ptr [1];
    word24 codePtr = t [ARG6] . argAddr;
    M [codePtr] = code;
    doRCU (true); // doesn't return
  }

static void trapHCS_SetBcSeg (void)
  {

// :Entry: set_bc_seg: 03/08/82  hcs_$set_bc_seg
// 
// 
// Function: given a pointer to the segment, sets the bit count of a
// segment in the storage system.  It also sets that bit count author of
// the segment to be the user who called it.
// 
// 
// Syntax:
// declare hcs_$set_bc_seg entry (ptr, fixed bin(24), fixed bin(35));
// call hcs_$set_bc_seg (seg_ptr, bit_count, code);
// 
// 
// Arguments:
// seg_ptr
//    is a pointer to the segment whose bit count is to be changed.
//    (Input)
// bit_count
//    is the new bit count of the segment.  (Input)
// code
//    is a storage system status code.  (Output)
// 
// 
// Access required:
// The user must have write access on the segment, but does not
// need modify permission on the containing directory.
// 
// 
// Notes: The hcs_$set_bc entry point performs the same function, when
// provided with a pathname of a segment rather than a pointer.
// 
  
    argTableEntry t [3] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    word24 code = 0;

    // Process the arguments

    // Argument 1: seg_ptr

    word24 ap1 = t [ARG1] . argAddr;
    word15 segno = GET_ITS_SEGNO (M + ap1);
    if (segno > N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // Argument 2: bit_count

    word24 ap2 = t [ARG2] . argAddr;
    word24 bit_count = (word24) (M [ap2] & MASK24);

    KST [idx] . bit_count = bit_count;
    KST [idx] . seglen = (bit_count + 35) / 36;

    installSDW (idx);

    sim_debug (DBG_CAC, & cpu_dev, 
     "set_bc_seg segno %o seglen %u bitcount %u\n", 
     segno, KST [idx] . seglen, KST [idx] . bit_count);
    //sim_printf ("set_bc_seg segno %o seglen %u bitcount %u\n", 
     //segno, KST [idx] . seglen, KST [idx] . bit_count);

done:;
    word24 codePtr = t [ARG3] . argAddr;
    M [codePtr] = code;
    doRCU (true); // doesn't return
  }

static void trapHCS_HighLowSegCount (void)
  {

// dcl  hcs_$high_low_seg_count entry (fixed bin, fixed bin);
//
// 1) high_seg  the number to add to hcsc to get the highest segment number
// being used.
//
// 2) hcsc      is the lowest non-hardcore segment number.
//

// /* Obtain the range of valid non ring 0 segment numbers. */
//
//        call hcs_$high_low_seg_count (highest_segno, hc_seg_count);
//        highest_segno = highest_segno + hc_seg_count;
//

    argTableEntry t [2] =
      {
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (2, 0, t))
      return;
    M [t [ARG1] . argAddr] = nextSegno - 1ull - USER_SEGNO;
    M [t [ARG2] . argAddr] = USER_SEGNO;
    
    doRCU (true); // doesn't return
  }

static void trapHCS_TerminateNoname (void)
  {
// terminate_$noname removes a single null name from a segment given its
// segment pointer.
// USAGE: call terminate_$noname call hcs_$terminate_noname (segptr, code)

    argTableEntry t [2] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (2, 0, t))
      return;
// XXX implement

    doRCU (true); // doesn't return
  }

static void trapHCS_FSGetMode (void)
  {
// fs_get$mode returns the mode of the current user at the current
//   validation level for the segment specified by segptr.
// USAGE: call fs_get$mode (segptr, mode, code);
//   1) segptr ptr - - - pointer to segment
//   2) mode fixed bin(5) - - - mode of user (output)
//   3) code fixed bin - - - error code (output)
// Notes: The mode argument is a fixed binary number where the desired
// mode is encoded with one access mode specified by each bit.  For
// segments the modes are:
//    read         the 8-bit is 1 (i.e., 01000b)
//    execute      the 4-bit is 1 (i.e., 00100b)
//    write                the 2-bit is 1 (i.e., 00010b)
// For directories, the modes are:
//    status               the 8-bit is 1 (i.e., 01000b)
//    modify               the 2-bit is 1 (i.e., 00010b)
//    append               the 1-bit is 1 (i.e., 00001b)
    argTableEntry t [3] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    // Argument 1: seg_ptr
    word24 ap1 = t [ARG1] . argAddr;
    word15 segno = GET_ITS_SEGNO (M + ap1);
    word24 code = 0;
    if (segno >= N_SEGNOS)
      {
        code = lookupErrorCode ("invalidsegno");
      }
    else
      {
        sgIdx idx = segnoMap [segno];
        if (idx < 0)
          {
            code = lookupErrorCode ("invalidsegno");
          }
        else
          {
            word36 m = 0;
            KSTEntry * e = KST + idx;
            if (e -> R)
              m |= 010;
            if (e -> E)
              m |= 004;
            if (e -> W)
              m |= 002;
            M [t [ARG2] . argAddr] = m;
          }
      }
    M [t [ARG3] . argAddr] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_cpu_time_and_paging (void)
  {
// Return the virtual CPU time and page fault count.
// For FXE, return the cycle count for the time, and 0 for the count

    argTableEntry t [3] =
      {
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;
    word24 ap3 = t [ARG3] . argAddr;

    M [ap1] = sys_stats . total_cycles;
    M [ap2] = 0;
    M [ap3] = 0;
    doRCU (true); // doesn't return
  }


static void trapHCS_ProcInfo (void)
  {
// proc_info:    proc(process_id,process_group_id,process_dir_name,lock_id_);
//    declare process_id bit(36) aligned,
//            process_group_id char(32) aligned,
//            process_dir_name char(32) aligned,
//            lock_id_ bit(36) aligned,

    argTableEntry t [4] =
      {
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (4, 0, t))
      return;


    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;
    //word24 ap3 = t [ARG3] . argAddr;
    word24 ap4 = t [ARG4] . argAddr;

    M [ap1] = 1;
    M [ap2] = 2;
    //M [ap3] = 0;
    M [ap4] = 4;

    doRCU (true); // doesn't return
  }

static void trapHCS_GetAuthorization (void)
  {
// authorization: entry(auth, max_auth);
//    declare (auth, max_auth) bit(72) aligned,

    argTableEntry t [2] =
      {
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (2, 0, t))
      return;


    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;
    word24 ap2 = t [ARG2] . argAddr;

    M [ap1] = 0;
    M [ap1 + 1] = 0;
    M [ap2] = 0;
    M [ap2 + 1] = 0;

    doRCU (true); // doesn't return
  }

static void trapHCS_FsGetPathName (void)
  {
// -- ->  fs_get$path_name returns the pathname of the directory immediately 
// superior to, and the entry name of the segment specified by segptr.
//      
// USAGE: call fs_get$path_name (segptr, dirname, lnd, ename, code);
//      
// 1) segptr ptr - - - pointer to the segment
// 2) dirname char(168) - - - pathname of superior directory (output)
// 3) lnd fixed bin - - - number of significant chars in pathname (output)
// 4) ename char(32) - - - entry name of segment (output)
// 5) code fixed bin - - - error code (output)
//      
// path_name: entry (a_segptr, a_dirname, a_lnd, a_ename, a_code);
//   dcl  a_segptr                 ptr parameter;
//   dcl  a_dirname                char (*) parameter;
//   dcl  a_lnd                    fixed bin (17) parameter;
//   dcl  a_ename                  char (*) parameter;
//   dcl  a_code                   fixed bin (35) parameter;


    argTableEntry t [5] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (5, 5, t))
      return;

    word36 code = 0;

    // Process the arguments

    word24 ap1 = t [ARG1] . argAddr;

    //sim_printf ("%012llo %012llo\n", M [ap1], M [ap1 + 1]);
    word15 segno = getbits15 (M [ap1], 3);

    if (segno >= N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // a_dirname
    word24 ap2 = t [ARG2] . argAddr;
    strcpyNonVarying (ap2, NULL, KST [idx] . pathname);

    // a_lnd
    word24 ap3 = t [ARG3] . argAddr;
    M [ap3] = strlen (KST [idx] . pathname);

    // entry
    word24 ap4 = t [ARG4] . argAddr;
#if 0 // it appears that they mean the "file name", not the entry point name.
    // XXX buffer overrun?
    strcpyNonVarying (ap4, NULL, KST [idx] . entryName); // XXX if .parsed?
#endif
    strcpyNonVarying (ap4, NULL, KST [idx] . segname); 

//sim_printf ("get_pathname returning idx %d segno %05o entryName %s\n", idx, segno, KST [idx] . segname);
//sim_printf ("get_pathname dsize %d pathname %s\n", t [ARG2] . dSize, KST [idx] . pathname);

done:;
    // code
    word24 ap5 = t [ARG5] . argAddr;
    M [ap5] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_chnameSeg (void)
  {
// call chname$cseg(segment_pointer, old_name, new_name, error_code);
//   segment_pointer pointer         pointer to segment to be changed.
//   old_name char(*)                name to be deleted from name list of 
//                                     entry_name.
//   new_name char(*)                name to be added to name list of 
//                                     entry_name.
//   error_code fixed bin(35)        file system error code (Output).
//
// cseg: entry (a_sp, a_oldname, a_newname, a_code);
//   



    argTableEntry t [4] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (4, 4, t))
      return;

    word36 code = 0;

    // Process the arguments

    // segment ptr
    word24 ap1 = t [ARG1] . argAddr;

    //sim_printf ("%012llo %012llo\n", M [ap1], M [ap1 + 1]);
    word15 segno = getbits15 (M [ap1], 3);

    if (segno >= N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // oldname
    word24 ap2 = t [ARG2] . argAddr;
    word24 dp2 = t [ARG2] . descAddr;
    word24 d2size = GET24 (M [dp2]);

    char * oldname = malloc (d2size + 1);
    strcpyC (ap2, d2size, oldname);
    trimTrailingSpaces (oldname);

    // newname
    word24 ap3 = t [ARG3] . argAddr;
    word24 dp3 = t [ARG3] . descAddr;
    word24 d3size = GET24 (M [dp3]);

    char * newname = malloc (d3size + 1);
    strcpyC (ap3, d3size, newname);
    trimTrailingSpaces (newname);

    //sim_printf ("chname segno %05o %s %s->%s\n", segno, KST [idx] . segname, oldname, newname);

    if (strncmp (KST [idx] . segname, oldname, SEGNAME_LEN))
      sim_printf ("WARNING: chname_seg old (%s) != KST (%s)\n",
                  oldname, KST [idx] . segname);

    strncpy (KST [idx] . segname, oldname, SEGNAME_LEN);
    
done:;
    // code
    word24 ap4 = t [ARG4] . argAddr;
    M [ap4] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_SetSafetySwSeg (void)
  {
//  SET$SAFETY_SWITCH_PTR changes the safety switch in the directory entry 
//    corresponding to the pointer "segptr".
//  entry (a_segptr, a_safety_sw, a_code);

    argTableEntry t [3] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    word36 code = 0;
    word24 ap3 = t [ARG3] . argAddr;
    M [ap3] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_StatusLong (void)
  {
// long:
//      entry (a_dir_name, a_entryname, a_chase, a_return_struc_ptr, 
//             a_return_area_ptr, a_code);
//           dcl     a_dir_name               char (*) parameter;
//           dcl     a_entryname              char (*) parameter;
//           dcl     a_chase          fixed bin (1) parameter;
//           dcl     a_return_struc_ptr       ptr parameter;
//
//   a_dir_name is the pathname of the containing directory. (Input)
//
//   entryname is the entry name of the segment, dirctory or link. (Input)
//
//   chase_sw indicates whether the information returned is about a link
//            or about the entry to which the link points. (Input)
//     0 returns link information
//     1 returns information about the entry to which the link points
//
//   status_ptr is a pointer to the structure in which information is returned.
//              (Input)
//
//   status_area_ptr is a pointer to the area in which names are returned. 
//                   (Input)
//
//   code. (Output)
//


    argTableEntry t [6] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (6, 6, t))
      return;

    word36 code = 0;

    // Argument 1: dir_name 
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = (word6) (t [ARG1] . dSize & MASK6);

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = (word6) (t [ARG2] . dSize & MASK6);

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

// Is ths a known segment?

// XXX ignoring directory....

    sgIdx idx = lookupSegname (entryname);
    if (idx < 0)
      {
        sim_printf ("don't know about algebra (%s)\n", entryname);
        exit (1);
      }

    // Argument 4: status_ptr
    word24 ap4 = t [ARG4] . argAddr;
    status_branch * status_ptr = (status_branch *) (& M [ap4]);

    status_ptr -> short_ . type = 1; // segment

// The only things translator_info is interested in are dtcm and uid.

    status_ptr -> short_ . nnames = 0;
    status_ptr -> short_ . names_relp = 0;
    status_ptr -> short_ . dtcm = 0; // XXX translator_info looks here....
    status_ptr -> short_ . dtu = 0;
    status_ptr -> short_ . mode = 0;
    status_ptr -> short_ . raw_mode = 0;
    status_ptr -> short_ . records_used = 0;
    //status_ptr -> long_ . dtd = 0;
    //status_ptr -> long_ . dtem = 0;
    //status_ptr -> long_ . lvid = 0;
    //status_ptr -> long_ . current_length = KST [idx] . seglen / 1024u;
    //status_ptr -> long_ . bit_count = KST [idx] . bit_count;
    //status_ptr -> long_ . copy_switch = 0;
    //status_ptr -> long_ . tpd_switch = 0;
    //status_ptr -> long_ . mdir_switch = 0;
    //status_ptr -> long_ . damaged_switch = 0;
    //status_ptr -> long_ . synchronized_switch = 0;
    //status_ptr -> long_ . ring_brackets = 
    //  ((KST [idx] . R1 & 077) << 12) |
    //  ((KST [idx] . R2 & 077) <<  6) |
    //  ((KST [idx] . R3 & 077) <<  0);
    //status_ptr -> long_ . uid = KST [idx] . uid;

    word24 ap6 = t [ARG6] . argAddr;
    M [ap6] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_Status (void)
  {
// status:
//      entry (a_dir_name, a_entryname, a_chase, a_return_struc_ptr, 
//             a_return_area_ptr, a_code);
//           dcl     a_dir_name               char (*) parameter;
//           dcl     a_entryname              char (*) parameter;
//           dcl     a_chase          fixed bin (1) parameter;
//           dcl     a_return_struc_ptr       ptr parameter;
//
//   a_dir_name is the pathname of the containing directory. (Input)
//
//   entryname is the entry name of the segment, dirctory or link. (Input)
//
//   chase_sw indicates whether the information returned is about a link
//            or about the entry to which the link points. (Input)
//     0 returns link information
//     1 returns information about the entry to which the link points
//
//   status_ptr is a pointer to the structure in which information is returned.
//              (Input)
//
//   status_area_ptr is a pointer to the area in which names are returned. 
//                   (Input)
//
//   code. (Output)
//


    argTableEntry t [6] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (6, 6, t))
      return;

    word36 code = 0;

    // Argument 1: dir_name 
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = (word6) (t [ARG1] . dSize & MASK6);

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = (word6) (t [ARG2] . dSize & MASK6);

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

// Is ths a known segment?

// XXX ignoring directory....

    sgIdx idx = lookupSegname (entryname);
    if (idx < 0)
      {
        sim_printf ("don't know about algebra (%s) (%s)\n", dir_name, entryname);
        //exit (1);
        code = lookupErrorCode ("noentry");
        goto done;
      }

    // Argument 4: status_ptr
    word24 ap4 = t [ARG4] . argAddr;
    status_branch * status_ptr = (status_branch *) (& M [ap4]);

    status_ptr -> short_ . type = 1; // segment

// The only things translator_info are interested in are dtcm and uid.

    status_ptr -> short_ . nnames = 0;
    status_ptr -> short_ . names_relp = 0;
    status_ptr -> short_ . dtcm = 0; // XXX translator_info looks here....
    status_ptr -> short_ . dtu = 0;
    status_ptr -> short_ . mode = 0;
    status_ptr -> short_ . raw_mode = 0;
    status_ptr -> short_ . records_used = 0;
    status_ptr -> long_ . dtd = 0;
    status_ptr -> long_ . dtem = 0;
    status_ptr -> long_ . lvid = 0;
    status_ptr -> long_ . current_length = KST [idx] . seglen / 1024u;
    status_ptr -> long_ . bit_count = KST [idx] . bit_count;
    status_ptr -> long_ . copy_switch = 0;
    status_ptr -> long_ . tpd_switch = 0;
    status_ptr -> long_ . mdir_switch = 0;
    status_ptr -> long_ . damaged_switch = 0;
    status_ptr -> long_ . synchronized_switch = 0;
    status_ptr -> long_ . ring_brackets = 
      ((((uint) KST [idx] . R1) & 077) << 12) |
      ((((uint) KST [idx] . R2) & 077) <<  6) |
      ((((uint) KST [idx] . R3) & 077) <<  0);
    status_ptr -> long_ . uid = KST [idx] . uid;

done:;
    word24 ap6 = t [ARG6] . argAddr;
    M [ap6] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_LevelGet (void)
  {
    argTableEntry t [1] =
      {
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (1, 0, t))
      return;

    // Argument 1: lebel 
    word24 ap1 = t [ARG1] . argAddr;
    M [ap1] = pdsValidationLevel;

    doRCU (true); // doesn't return
  }

static void trapHCS_GetRingBrackets (void)
  {
// long:
//      entry (a_dir_name, a_entryname, rb, code)
//             a_return_area_ptr, a_code);
//           dcl     a_dir_name               char (*) parameter;
//           dcl     a_entryname              char (*) parameter;
//           dcl     rb          3 fixed bin (3) parameter;
//           dcl     code fixed bin (35)
//
//   a_dir_name is the pathname of the containing directory. (Input)
//
//   entryname is the entry name of the segment, dirctory or link. (Input)
//
//   rb ring brackets (Output)
//   code. (Output)
//


    argTableEntry t [4] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (4, 4, t))
      return;

    word36 code = 0;

    // Argument 1: dir_name 
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = (word6) (t [ARG1] . dSize & MASK6);

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = (word6) (t [ARG2] . dSize & MASK6);

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

// Is ths a known segment?

// XXX ignoring directory....

    sgIdx idx = lookupSegname (entryname);
    if (idx < 0)
      {
        sim_printf ("don't know about trig\n");
        exit (1);
      }

    // Argument 3: rb
    word24 ap3 = t [ARG3] . argAddr;

#if 0
    M [ap3] =
      ((KST [idx] . R1 & 077) << 12) |
      ((KST [idx] . R2 & 077) <<  6) |
      ((KST [idx] . R3 & 077) <<  0);
#else
    M [ap3] = KST [idx] . R1;
    M [ap3 + 1] = KST [idx] . R2;
    M [ap3 + 2] = KST [idx] . R3;
#endif
    word24 ap4 = t [ARG4] . argAddr;
    M [ap4] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_TruncateSeg (void)
  {
//      entry (ptr, fixed bin (19), fixed bin (235))
//           dcl     ptr
//           dcl     length fixed bin (19)
//           dcl     code fixed bin (35)
//      call hcs_$truncate_seg (seg_ptr, length, code);
//
//   length new segment length (Input)
//
//   code. (Output)
//
// "If the segment is already shorter then the specfied length, no truncation
// is done. The effect of truncating a segment is to store zeros in the words
// beyond the specified length"

    argTableEntry t [3] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    word36 code = 0;

    // Argument 1: seg ptr 
    word24 ap1 = t [ARG1] . argAddr;
    word15 segno = getbits15 (M [ap1], 3);

    if (segno > N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // Argument 2: length
    word24 ap2 = t [ARG2] . argAddr;
    word24 length = M [ap2] & MASK20;

//sim_printf ("truncate segno %05o seglen %07o new length %07o\n", segno, KST [idx] . seglen, length);
    if (length < KST [idx] . seglen)
      {
        word24 physmem = KST [idx] . physmem;
//sim_printf ("truncate zeroing segno %05o seglen %07o new length %07o\n", segno, KST [idx] . seglen, length);
        for (uint i = length; i < KST [idx] . seglen; i ++)
          M [physmem + i] = 0;
        KST [idx] . seglen = length;
      }
done:;
    word24 ap3 = t [ARG3] . argAddr;
    M [ap3] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_GetMaxLengthSeg (void)
  {
// hcs_$get_max_length_seg (seg_ptr, max_length, code)
//
//     seg_ptr  ptr (input)
//     max_length fixed bin (19) (output)
//     code fixed bin (35) (output)

    argTableEntry t [3] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    word36 code = 0;

    // Argument 1: seg ptr 
    word24 ap1 = t [ARG1] . argAddr;
    word15 segno = getbits15 (M [ap1], 3);

    if (segno > N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // Argument 2: length
    word24 ap2 = t [ARG2] . argAddr;
    M [ap2] = KST [idx] . allocatedLength;
    //sim_printf ("get_max_length_seg %05o length %07o\n", segno, KST [idx] . allocatedLength);

done:;
    word24 ap3 = t [ARG3] . argAddr;
    M [ap3] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_SetMaxLengthSeg (void)
  {
// hcs_$get_max_length_seg (seg_ptr, max_length, code)
//
//     seg_ptr  ptr (input)
//     max_length fixed bin (19) (output)
//     code fixed bin (35) (output)

    argTableEntry t [3] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (3, 0, t))
      return;

    word36 code = 0;

    // Argument 1: seg ptr 
    word24 ap1 = t [ARG1] . argAddr;
    word15 segno = getbits15 (M [ap1], 3);

    if (segno > N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // Argument 2: length
    word24 ap2 = t [ARG2] . argAddr;
//sim_printf ("set_max_length_seg %05o allocated %07o used %07o set %07llo\n", segno, KST [idx] . allocatedLength, KST [idx] . seglen, M [ap2] & 01777777);
    KST [idx] . seglen = M [ap2] & 01777777; // XXX limit checking?

done:;
    word24 ap3 = t [ARG3] . argAddr;
    M [ap3] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_AddAclEntries (void)
  {
// hcs_$add_acl_entries (char (*), char (*), ptr, fixed bin, fixed bin (35))
// call hcs_$add_acl_entries (dir_name, entry_name, acl_ptr, acl_count, code)
//

    argTableEntry t [5] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (5, 5, t))
      return;

    word36 code = 0;


    // Argument 1: dir_name 
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = (word6) (t [ARG1] . dSize & MASK6);

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = (word6) (t [ARG2] . dSize & MASK6);

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

// Is ths a known segment?

// XXX ignoring directory....

    sgIdx idx = lookupSegname (entryname);
    if (idx < 0)
      {
        sim_printf ("don't know about difeq\n");
        exit (1);
      }
//sim_printf ("add_acl_entry idx %s$%s %d\n", dir_name, entryname, idx);

    KSTEntry * e = KST + idx;

    // Argument 3: acl_ptr
    word24 ap3 = t [ARG3] . argAddr;
    word24 aclPtr = ITSToPhysmem (M + ap3, NULL);
    segment_acl_entry * acl_ptr = (segment_acl_entry *) & M [aclPtr];

    // Argument 4: acl_count
    word24 ap4 = t [ARG4] . argAddr;
    if (M [ap4] != 1)
      {
        sim_printf ("WARNING hcs_$add_acl_entry ignoring acl_count != 1 (%llu)\n", M [ap4]);
      }

    //sim_printf ("add_acl_entry mode %llo\n", acl_ptr -> mode);
    e -> R = ((acl_ptr -> mode) & 0400000000000llu) ? 1 : 0;
    e -> E = ((acl_ptr -> mode) & 0200000000000llu) ? 1 : 0;
    e -> W = ((acl_ptr -> mode) & 0100000000000llu) ? 1 : 0;
    installSDW (idx);

    word24 ap5 = t [ARG5] . argAddr;
    M [ap5] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_DeleteAclEntries (void)
  {
// hcs_$delete_acl_entries (char (*), char (*), ptr, fixed bin, fixed bin (35))
// call hcs_$add_acl_entries (dir_name, entry_name, acl_ptr, acl_count, code)
//

    argTableEntry t [5] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (5, 5, t))
      return;

    word36 code = 0;

#if 0
    // Argument 1: dir_name 
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = t [ARG1] . dSize;

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = t [ARG2] . dSize;

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

// Is ths a known segment?

// XXX ignoring directory....

    sgIdx idx = lookupSegname (entryname);
    if (idx < 0)
      {
        sim_printf ("don't know about difeq\n");
        exit (1);
      }
//sim_printf ("add_acl_entry idx %s$%s %d\n", dir_name, entryname, idx);

    KSTEntry * e = KST + idx;

    // Argument 3: acl_ptr
    word24 ap3 = t [ARG3] . argAddr;
    word24 aclPtr = ITSToPhysmem (M + ap3, NULL);
    segment_acl_entry * acl_ptr = (segment_acl_entry *) & M [aclPtr];

    // Argument 4: acl_count
    word24 ap4 = t [ARG4] . argAddr;
    if (M [ap4] != 1)
      {
        sim_printf ("WARNING hcs_$add_acl_entry ignoring acl_count != 1 (%llu)\n", M [ap4]);
      }
#endif

    word24 ap5 = t [ARG5] . argAddr;
    M [ap5] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_ListAcl (void)
  {
// hcs_$list_acl (char (*), char (*), ptr, ptr, ptr, fixed bin, fixed bin (35))
// call hcs_$list_acl (dir_name, entry_name, area_ptr, area_ret_ptr, acl_ptr, acl_count, code)
//

    argTableEntry t [7] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (7, 7, t))
      return;

    word36 code = 0;

#if 0
    // Argument 1: dir_name 
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = t [ARG1] . dSize;

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = t [ARG2] . dSize;

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

// Is ths a known segment?

// XXX ignoring directory....

    sgIdx idx = lookupSegname (entryname);
    if (idx < 0)
      {
        sim_printf ("don't know about difeq\n");
        exit (1);
      }
    //sim_printf ("acl_list idx %s$%s %d\n", dir_name, entryname, idx);

    KSTEntry * e = KST + idx;
#endif

    // Argument 3: area_ptr
    word24 ap3 = t [ARG3] . argAddr;
//sim_printf ("list_acl area_ptr %012llo %012llo\n", M [ap3], M [ap3 + 1]);
    if (isNullPtr (ap3))
      {
        sim_printf ("WARNING: list_acl ignoring null area_ptr case\n");
      }
    else
      {
        //word24 areaPtr = ITSToPhysmem (M + ap3, NULL);
        // Argument 6: acl_count
        word24 ap6 = t [ARG6] . argAddr;
        M [ap6] = 0;
      }

    // Argument 4: area_ret_ptr
    //word24 ap4 = t [ARG4] . argAddr;
    //word24 areaRetPtr = ITSToPhysmem (M + ap4, NULL);
    //sim_printf ("list_acl area_ret_ptr %012llo %012llo\n", M [ap4], M [ap4 + 1]);
    //makeNullPtr (M + areaRetPtr);

    word24 ap7 = t [ARG7] . argAddr;
    M [ap7] = code;

    doRCU (true); // doesn't return
  }

static void trapHCS_ReplaceAcl (void)
  {
// hcs_$replace_acl (char (*), char (*), ptr, fixed bin, bit (1), fixed bin (35))
// call hcs_$replace_acl (dir_name, entry_name, acl_ptr, acl_count, no_sysdaemon_sw, code)
//

    argTableEntry t [6] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 },
        { DESC_BITS, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };

    if (! processArgs (6, 6, t))
      return;

    word36 code = 0;

#if 0
    // Argument 1: dir_name 
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = t [ARG1] . dSize;

    char * dir_name = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        dir_name = malloc (d1size + 1);
        strcpyC (ap1, d1size, dir_name);
        trimTrailingSpaces (dir_name);
      }
    //sim_printf ("dir_name: '%s'\n", dir_name ? dir_name : "NULL");

    // Argument 2: entryname
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = t [ARG2] . dSize;

    char * entryname = NULL;
    if ((! isNullPtr (ap2)) && d2size)
      {
        entryname = malloc (d2size + 1);
        strcpyC (ap2, d2size, entryname);
        trimTrailingSpaces (entryname);
      }
    //sim_printf ("entryname: '%s'\n", entryname ? entryname : "NULL");

// Is ths a known segment?

// XXX ignoring directory....

    sgIdx idx = lookupSegname (entryname);
    if (idx < 0)
      {
        sim_printf ("don't know about difeq\n");
        exit (1);
      }
    //sim_printf ("acl_list idx %s$%s %d\n", dir_name, entryname, idx);

    KSTEntry * e = KST + idx;
#endif

#if 0
    // Argument 3: area_ptr
    word24 ap3 = t [ARG3] . argAddr;
//sim_printf ("list_acl area_ptr %012llo %012llo\n", M [ap3], M [ap3 + 1]);
    if (isNullPtr (ap3))
      {
        sim_printf ("WARNING: list_acl ignoring null area_ptr case\n");
      }
    else
      {
        word24 areaPtr = ITSToPhysmem (M + ap3, NULL);
        // Argument 6: acl_count
        word24 ap6 = t [ARG6] . argAddr;
        M [ap6] = 0;
      }
#endif

    word24 ap6 = t [ARG6] . argAddr;
    M [ap6] = code;

    doRCU (true); // doesn't return
  }

static void trapPHCS_SetKSTAttributes (void)
  {
// set_kst_attributes: proc (a_segno, a_kstap, a_code);
//
//   This procedure allows a sufficiently privileged user to change the segment
//   use attributes stored in his kst.
//
//   Privileged users may set: allow_write, explicit_deact_ok, tpd, and audit.
//   Highly privileged users may also set: tms, and tus.
//
// dcl  a_segno fixed bin (17),
//      a_kstap ptr,
//      a_code fixed bin (35);
    argTableEntry t [] =
      {
        { DESC_FIXED, 0, 0, 0 },
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };
    if (! processArgs (3, 0, t))
      return;

    word24 code = 0;

    // Argument 1: segno
    word24 ap1 = t [ARG1] . argAddr;
    word15 segno = (word15) (M [ap1] & MASK15);

    if (segno >= N_SEGNOS) // bigger segno then we deal with
      {
        sim_printf ("ERROR: too big\n");
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }
    sgIdx idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = lookupErrorCode ("invalidsegno");
        goto done;
      }

    // Argument 2: ksta
    word24 ap2 = t [ARG2] . argAddr;
    word24 kstaPtr = ITSToPhysmem (M + ap2, NULL);
    word36 kstaWhich = M [kstaPtr];
    word36 kstaValue = M [kstaPtr + 1];

    if (kstaWhich & (01llu << 35)) // allow_write
      sim_printf ("WARNING: allow_write not implemented\n");
    if (kstaWhich & (01llu << 34)) // tms
      sim_printf ("WARNING: tms not implemented\n");
    if (kstaWhich & (01llu << 33)) // tus
      sim_printf ("WARNING: tus not implemented\n");
    if (kstaWhich & (01llu << 32)) // tpd
      sim_printf ("WARNING: tpd not implemented\n");
    if (kstaWhich & (01llu << 31)) // audit
      sim_printf ("WARNING: audit not implemented\n");
    if (kstaWhich & (01llu << 30)) // explicit_deactivate_ok
      {
        KST [idx] . explicit_deactivate_ok = 
          (kstaValue & (01llu << 30)) ? true : false;
      }

done:;
    word24 codePtr = t [ARG3] . argAddr;
    M [codePtr] = code;
    doRCU (true); // doesn't return
  }

#if 0
static void trapComErr (void)
  {
// com_err_:
// procedure options (variable);
// 
// /* com_err_ formats error messages and signals the condition "command_error".
// *   Its calling sequence is of the form: call com_err_(code, callername, ioa_control, arg1, arg2,...);.
// *   If code > 0, the corresponding error_table_ message is included.  Callername is the name of the
// *   calling procedure and is inserted with a colon at the beginning of the error message.
// *   It may be either varying or fixed length; if it is null, the colon is omitted.
// *   The rest of the arguments are optional; however, if arg1, etc. are present, ioa_control
// *   must also be present.  ioa_control is a regular ioa_ control string and the argi are the
// *   format arguments to ioa_.  If print_sw = "1"b after signalling "command_error", the
// *   error message is printed.
// *   Several other entry points are included in this procedure.  The active_fnc_err_
// *   entry is similar to com_err_ except that the condition "active_function_error" is
// *   signalled.  The suppress_name entry is identical to com_err_ except that the
// *   callername is omitted from the error message.
// *   There is an entry point for convert_status_code_, which simply looks up the code and
// *   returns the error_table_ message.
 
    sim_printf ("com err\n");
    doRCU (true); // doesn't return
  }
#endif

static void trapPHCS_Deactivate (void)
  {

#if 0
// XXX This is called by eis_tester to check page fault handling in EIS.
//   1. We know EIS page fault handling is broken.
//   2. FXE is unpaged for now.
//   3. Therefore, this call is ignored.


//      phcs_$deactivate entry (ptr, fixed bin (35)),

//                        deactivate (astep, code)
//                        deactivate$for_delete (astep, code)
//
// FUNCTION -
// 
// The procedure "deactivate" deactivates the segment whose ASTE is pointed  to  by
// the  input argument "astep".  If the deactivation is successful, it returns with
// code=0; if the deactivation fails, it returns with code=0, or ehs=1.
// The procedure "deactivate" does not concern itself with the AST lock. It assumes
// there is no race condition.  It is the responsibility of the caller to make sure
// there is no race condition. The initializer  or  shutdown  of  course  may  call
// deactivate  without  locking  the AST. For normal processes, however, the caller
// must make sure the AST is locked before the call  is  issued,  and  it  will  be
// unlocked upon return as soon as it is safe to do so.
// 
// The  ASTE  is  left  in  the  circular list associated with the size of the page
// table, at the first position, so that it will be found right away should an ASTE
// of this size be needed.
// 
// The ASTE is removed from the uid hash table.
// 
// All items of the ASTE are zeroed except fp, bp, ptsi and marker. All  PTW's  are
// initialized with a page not in core flag and a coded null disk address.

// CAC: This makes no sense; eis_tester is passing in a pointer to a page
// it wants deactivated; the just is no way for an ASTE to be passed in;
// they don't exist in FXE.

    argTableEntry t [] =
      {
        { DESC_PTR, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };
    if (! processArgs (2, 0, t))
      return;

    word24 code = 0;

    // Argument 1: astep
    word24 ap1 = t [ARG1] . argAddr;
    word24 astepPtr = ITSToPhysmem (M + ap1, NULL);
    //for (int i = 0; i < /*12*/ 1; i ++)
      //sim_printf (">%08o [%012llo]\n", astepPtr + i, M [astepPtr + i]);
#endif
    doRCU (true); // doesn't return
  }

// Invoke signal handler...

// io_signal, signal_, signal: entry (a_name, a_mcptr, a_info_ptr);
//   a_name          char (*),      /* condition being signalled */
//   a_info_ptr      ptr,           /* information about software signal */
//   a_wcptr         ptr,           /* info about wall crossing from this ring before crawlout */
//   a_mcptr          ptr;           /* optional machine conditions ptr */
 

static void trapFXE_UnhandledSignal (void)
  {
    sim_printf ("ERROR: Unhandled signal\n");
    longjmp (jmpMain, JMP_STOP);
  }

static void trapFXE_ReturnToFXE (void)
  {
#ifndef SPEED
    if_sim_debug (DBG_TRACE, & fxe_dev)
      sim_printf ("Process exited\n");
#endif
    longjmp (jmpMain, JMP_STOP);
  }

#if 0
static void trapFXE_debug (void)
  {
    sim_printf ("fxe debug\n");

#if 1 // debug for expand_pathname_
    argTableEntry t [] =
      {
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_CHAR_SPLAT, 0, 0, 0 },
        { DESC_FIXED, 0, 0, 0 }
      };
    if (! processArgs (4, 4, t))
      return;

    // Argument 1: pathname
    word24 ap1 = t [ARG1] . argAddr;
    word6 d1size = t [ARG1] . dSize;

    char * pathname = NULL;
    if ((! isNullPtr (ap1)) && d1size)
      {
        pathname = malloc (d1size + 1);
        strcpyC (ap1, d1size, pathname);
        trimTrailingSpaces (pathname);
      }
    sim_printf ("pathname: '%s'\n", pathname ? pathname : "NULL");
#endif

  }
#endif

static void fxeTrap (void)
  {
    // Application has made an call or return into a routine that FXE wants to handle
    // on the host.

    // Get the offending address from the SCU data

    // Test FIF bit
    word18 offset;
    word1 FIF = (word1) getbits36 (M [0200 + 5], 29, 1);
    if (FIF != 0)
      offset = (word18) getbits36 (M [0200 + 4], 0, 18); // PPR . IC
    else
      offset = (word18) getbits36 (M [0200 + 5], 0, 18); // PPR . CA
#ifndef SPEED
    if_sim_debug (DBG_TRACE, & fxe_dev)
      sim_printf ("FXE: trap %s:%s\n", trapNameTable [offset] . segName, trapNameTable [offset] . symbolName);
#endif
    trapNameTable [offset] . trapFunc ();
  }

static void faultACVHandler (void)
  {
    // The fault pair saved the SCU data @ 0200

    // Get the offending address from the SCU data

    word18 offset = GETHI (M [0200 + 5]);
    word15 segno = GETHI (M [0200 + 2]) & MASK15;

    if (segno == TRAP_SEGNO)
      {
        fxeTrap ();
      }

    sim_printf ("ERROR: acv fault %05o:%06o\n", segno, offset);
#if 0
    // Get the physmem address of the segment
    word36 * even = M + DESCSEG + 2 * segno + 0;  
    //word36 * odd  = M + DESCSEG + 2 * segno + 1;  
    word24 segBaseAddr = getbits36 (* even, 0, 24);
    word24 addr = (segBaseAddr + offset) & MASK24;
    sim_printf ("addr %08o:%012llo\n", addr, M [addr]);

    link_ * l = (link_ *) (M + addr);

    // Make a copy of it so that when we overwrite it, we don't get confused
    link_ linkCopy = * l;

    if (linkCopy . tag != 046)
      {
        sim_printf ("f2 handler dazed and confused. not f2? %012llo\n", M [addr]);
        return;
      }

    if (linkCopy . run_depth != 0)
      sim_printf ("WARNING:  run_depth wrong %06o\n", linkCopy . run_depth);

    // Find the linkage header
    sim_printf ("header_relp %08o\n", linkCopy . header_relp);
    word24 linkHeaderOffset = (offset + SIGNEXT18 (linkCopy . header_relp)) & MASK24;
    sim_printf ("headerOffset %08o\n", linkHeaderOffset);
    link_header * lh = (link_header *) (M + segBaseAddr + linkHeaderOffset);

    // Find the definition header
    word36 * defBase = M + segBaseAddr + lh -> def_offset;
    //sim_printf ("defs_in_link %o\n", lh -> defs_in_link);
    //sim_printf ("def_offset %o\n", lh -> def_offset);
    //sim_printf ("first_ref_relp %o\n", lh -> first_ref_relp);
    //sim_printf ("link_begin %o\n", lh -> link_begin);
    //sim_printf ("linkage_section_lng %o\n", lh -> linkage_section_lng);
    //sim_printf ("segno_pad %o\n", lh -> segno_pad);
    //sim_printf ("static_length %o\n", lh -> static_length);
    
    //sim_printf ("ringno %0o\n", linkCopy . ringno);
    //sim_printf ("run_depth %02o\n", linkCopy . run_depth);
    //sim_printf ("expression_relp %06o\n", linkCopy . expression_relp);
       
    expression * expr = (expression *) (defBase + linkCopy . expression_relp);

    //sim_printf ("  type_ptr %06o  exp %6o\n", 
                //expr -> type_ptr, expr -> exp);

    type_pair * typePair = (type_pair *) (defBase + expr -> type_ptr);

    word15 refSegno;
    word18 refValue;
    sgIdx defIdx;

    switch (typePair -> type)
      {
        case 1:
          sim_printf ("    1: self-referencing link\n");
          sim_printf ("WARNING: unhandled type %d\n", typePair -> type);
          break;
        case 3:
          sim_printf ("    3: referencing link\n");
          sim_printf ("WARNING: unhandled type %d\n", typePair -> type);
          break;
        case 4:
          sim_printf ("    4: referencing link with offset\n");
          sim_printf ("      seg %s\n", sprintACC (defBase + typePair -> seg_ptr));
          sim_printf ("      ext %s\n", sprintACC (defBase + typePair -> ext_ptr));
          char * segStr = strdup (sprintACC (defBase + typePair -> seg_ptr));
          char * extStr = strdup (sprintACC (defBase + typePair -> ext_ptr));
          if (resolveName (segStr, extStr, & refSegno, & refValue, & defIdx))
            {
              makeITS (M + addr, refSegno, linkCopy . ringno, refValue, 0, 
                       linkCopy . modifier);
              free (segStr);
              free (extStr);
              doRCU (false);
            }
          else
            {
              sim_printf ("ERROR: can't resolve %s:%s\n", segStr, extStr);
            }

          free (segStr);
          free (extStr);
          //sgIdx segIdx = loadSegmentFromFile (sprintACC (defBase + typePair -> seg_ptr));
          break;
        case 5:
          sim_printf ("    5: self-referencing link with offset\n");
          sim_printf ("WARNING: unhandled type %d\n", typePair -> type);
          break;
        default:
          sim_printf ("WARNING: unknown type %d\n", typePair -> type);
          break;
      }


    //sim_printf ("    type %06o\n", typePair -> type);
    //sim_printf ("    trap_ptr %06o\n", typePair -> trap_ptr);
    //sim_printf ("    seg_ptr %06o\n", typePair -> seg_ptr);
    //sim_printf ("    ext_ptr %06o\n", typePair -> ext_ptr);
#endif
  }

void fxeFaultHandler (void)
  {
    switch (cpu . faultNumber)
      {
        case 20: // access violation
          faultACVHandler ();
          break;
        case 24: // fault tag 2
          faultTag2Handler ();
          break;
        default:
          sim_printf ("ERROR: fxeFaultHandler: fail: %d\n", cpu . faultNumber);
      }
  }

//
// support for instruction tracing
//

char * lookupFXESegmentAddress (word18 segno, word18 offset, 
                                char * * compname, word18 * compoffset)
  {
    if (! FXEinitialized)
      return NULL;

    static char buf [129];
    for (sgIdx i = 0; i < (int) N_SEGS; i ++)
      {
        if (KST [i] . allocated && KST [i] . segno == segno)
          {
            if (! lookupOffset (i, offset, compname, compoffset))
              {
                if (compname)
                    * compname = KST [i] . segname;
                if (compoffset)
                    * compoffset = 0;  
              }
            sprintf (buf, "%s:+0%0o", compname ? * compname : KST [i] . segname, compoffset ? * compoffset : offset);
            return buf;
          }
      }
    return NULL;
  }

// simh stuff

static DEBTAB fxe_dt [] = 
  {
    { "TRACE",      DBG_TRACE       },
  };

DEVICE fxe_dev =
  {
    (char *) "FXE",       /* name */
    NULL,        /* units */
    NULL,        /* registers */
    NULL,        /* modifiers */
    0,           /* #units */
    8,           /* address radix */
    PASIZE,      /* address width */
    1,           /* address increment */
    8,           /* data radix */
    36,          /* data width */
    NULL,        /* examine routine */
    NULL,        /* deposit routine */
    NULL,        /* reset routine */
    NULL,        /* boot routine */
    NULL,        /* attach routine */
    NULL,        /* detach routine */
    NULL,        /* context */
    DEV_DEBUG,   /* flags */
    0,           /* debug control flags */
    fxe_dt,      /* debug flag names */
    NULL,        /* memory size change */
    NULL,        /* logical name */
    NULL,        /* help */
    NULL,        /* attach_help */
    NULL,        /* help_ctx */
    NULL         /* description */
  };

