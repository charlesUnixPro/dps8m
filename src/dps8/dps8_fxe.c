// From multicians: The maximum size of user ring stacks is initially set to 48K.SEGSIZE;
// XXX set the ring brackets from SLTE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "dps8.h"
#include "dps8_append.h"
#include "dps8_cpu.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_ins.h"

typedef struct __attribute__ ((__packed__)) def_header
  {
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint unused1 : 18;
            uint def_list_relp : 18;
          };
        word36 align1;
      };

    union
      {
        struct __attribute__ ((__packed__))
          {
            uint unused2 : 16;
            uint ignore : 1;
            uint new_format : 1;
            uint hash_table_relp : 18;
          };
        word36 align2;
      };
  } def_header;

typedef struct __attribute__ ((__packed__)) link_header
  {
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint defs_in_link : 6;
            uint pad : 30;
          };
        word36 align1;
      };

    union
      {
        struct __attribute__ ((__packed__))
          {
            uint first_ref_relp : 18;
            uint def_offset : 18;
          };
        word36 align2;
      };

    //word36 filled_in_later [4];
    word36 symbol_ptr [2];
    word36 original_linkage_ptr [2];

    union
      {
        struct __attribute__ ((__packed__))
          {
            uint linkage_section_lng : 18;
            uint link_begin : 18;
          };
        word36 align3;
      };

    union
      {
        struct __attribute__ ((__packed__))
          {
            uint static_length : 18;
            uint segno_pad : 18;
          };
        word36 align4;
      };


  } link_header;

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

// Remember which segment we loaded bound_library_wired_ into

static int libIdx;

//
// Forward declarations
//

static int loadSegmentFromFile (char * arg);

//
// Utility routines
//   printACC - print an ACC
//   sprintACC - convert an ACC to a C string
//   accCmp - strcmp a ACC to a C string
//   packedPtr - create a packed pointer
//   makeITS - create an ITS pointer
//   makeNullPtr - equivalent to Multics PL/I null()

#if 0 // unused
static void printACC (word36 * p)
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
#endif

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
        int woff = i / 4;
        int choff = i % 4;
        if (choff == 0)
          M [base + offset + woff] = 0;
        putbits36 (M + base + offset + woff, choff * 9, 9, str [i]);
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
#if 0
    // store the lengtn in the first word
    M [base + offset] = len;
    //sim_printf ("len %3d %012llo\n", offset, M [base + offset]);
    offset ++;
#endif
    for (uint i = 0; i < len; i ++)
      {
        int woff = i / 4;
        int choff = i % 4;
        if (choff == 0)
          M [base + offset + woff] = 0;
        putbits36 (M + base + offset + woff, choff * 9, 9, str [i]);
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
        int woff = i / 4;
        int choff = i % 4;
        word36 ch = getbits36 (M [addr + woff], choff * 9, 9);
        * (str ++) = (char) (ch & 0177);
        //sim_printf ("chn %3d %012llo\n", woff, M [addr + woff]);
      }
    * (str ++) = '\0';
  }

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
    int segno, R, E, W, P, R1, R2, R3;
    char * path;
  } SLTEentry;

static SLTEentry SLTE [] =
  {
#include "slte.inc"
    { NULL, 0, 0, 0, 0, 0, 0, 0, 0, NULL }
  };

// getSLTEidx - lookup a segment name in the SLTE 

static int getSLTEidx (char * name)
  {
    //sim_printf ("getSLTEidx %s\n", name);
    for (int idx = 0; SLTE [idx] . segname; idx ++)
      {
        if (strcmp (SLTE [idx] . segname, name) == 0)
          return idx;
      }
    //sim_printf ("ERROR: %s not in SLTE\n", name);
    return -1;
  }

// getSegnoFromSLTE - get the segment number of a segment from the SLTE

static int getSegnoFromSLTE (char * name, int * slteIdx)
  {
    int idx = getSLTEidx (name);
    if (idx < 0)
      return 0;
    * slteIdx = idx;
    return SLTE  [idx] . segno;
  }

//
// segTable: the list of known segments
//

// lookupSegAddrByIdx - return the physical memory address of a segment
// allocateSegment - find an unallocated entry in segtable, mark it allocated
//                   and allocate physical memory for it.
// allocateSegno - Allocate a segment number, starting at USER_SEGNO
//
// segno usage:
//          0  dseg
//       0265  iocbs
//       0266  combined linkage segment
//       0267  fxe
/// 0270-0277  stacks
//  0600-0777  user 
//     077776  fxe trap       

#define N_SEGNOS 01000
#define DSEG_SEGNO 0
#define IOCB_SEGNO 0265
#define CLR_SEGNO 0266
#define FXE_SEGNO 0267
#define STACKS_SEGNO 0270
#define USER_SEGNO 0600
#define TRAP_SEGNO 077776

#define FXE_RING 5
// Traps

enum
  {
    TRAP_UNUSED, // Don't use 0 so as to help distingish uninitialized fields
    // iox_
    TRAP_PUT_CHARS, 
    // TRAP_MODES, // deprecated

    // bound_io_commands
    TRAP_GET_LINE_LENGTH_SWITCH,
    
    // hcs_
    TRAP_HISTORY_REGS_SET,
    TRAP_HISTORY_REGS_GET,
    TRAP_FS_SEARCH_GET_WDIR,
    TRAP_INITIATE_COUNT,
    TRAP_TERMINATE_NAME,
    TRAP_MAKE_PTR,
    TRAP_STATUS_MINS,

    // FXE internal
    TRAP_RETURN_TO_FXE
  };

typedef struct segTableEntry
  {
    bool allocated;
    bool loaded;
    word15 segno;  // Multics segno
    bool wired;
    word3 R1, R2, R3;
    word1 R, E, W, P;
    char * segname;
    word18 seglen;
    word24 physmem;

// Cache values discovered during parseSegment

    word18 entry;
    bool gated;
    word18 entry_bound;
    word18 definition_offset;
    word18 linkage_offset;
    word18 linkage_length;
    word18 isot_offset;
    word36 * segnoPadAddr;

// RNT

    int RNTRefCount;
  } segTableEntry;



static segTableEntry segTable [N_SEGS];

// Map segno to segTableIdx

static int segnoMap [N_SEGNOS];

//
// CLR - Multics segment containing the LOT and ISOT
//

// Remember which segment has the CLR

static int clrIdx;

#define LOT_SIZE 0100000 // 2^12
#define LOT_OFFSET 0 // LOT starts at word 0
#define ISOT_OFFSET LOT_SIZE // ISOT follows LOT
//#define CLR_SIZE (LOT_SIZE * 2) // LOT + ISOT
#define CLR_FREE_START (LOT_SIZE * 2) // LOT + ISOT

// installLOT - install a segment LOT and ISOT in the SLT

static word18 clrFreePtr = CLR_FREE_START;

static void installLOT (int idx)
  {
    segTableEntry * segEntry = segTable + idx;
    segTableEntry * clrEntry = segTable + clrIdx;

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
    //clrMemory [LOT_OFFSET + segTable [idx] . segno] = 
    //  packedPtr (0, segTable [idx] . segno,
    //             segTable [idx] . linkage_offset);
    //sim_printf ("newLinkage: %05o %06o-%06o\n", segTable [idx] . segno, newLinkage, clrFreePtr - 1);
    clrMemory [LOT_OFFSET + segTable [idx] . segno] = 
      packedPtr (0, segTable [clrIdx] . segno,
                 newLinkage);

    clrMemory [ISOT_OFFSET + segTable [idx] . segno] = 
      packedPtr (0, segTable [idx] . segno,
                 segTable [idx] . isot_offset);
  }

//
// segTable routines
//

static word24 lookupSegAddrByIdx (int segIdx)
  {
    return segTable [segIdx] . physmem;
  }


static word24 nextPhysmem = 1; // First block is used by dseg;
static int allocateSegment (void)
  {
    for (int i = 0; i < (int) N_SEGS; i ++)
      {
        if (! segTable [i] . allocated)
          {
            segTable [i] . allocated = true;
            segTable [i] . physmem = (nextPhysmem ++) * SEGSIZE;
            return i;
          }
      }
    return -1;
  }

static int nextSegno = USER_SEGNO;

static int allocateSegno (void)
  {
    return nextSegno ++;
  }

static void setSegno (int idx, word15 segno)
  {
    segTable [idx] . segno = segno;
    segnoMap [segno] = idx;
  }

static word24 ITSToPhysmem (word36 * its, word6 * bitno)
  {
    word36 even = * its;
    word36 odd = * (its + 1);

    word15 segno = getbits36 (even, 3, 15);
    word18 wordno = getbits36 (odd, 0, 18);
    if (bitno)
      * bitno = getbits36 (odd, 57 - 36, 6);

    word24 physmem = segTable [segnoMap [segno]] . physmem  + wordno;
    return physmem;
  }


// RNT Process Reference Name Table

typedef struct RNTEntry
  {
    char * refName;
    int idx;
  } RNTEntry;
static RNTEntry RNT [N_SEGS]; 
#define RNT_TABLE_SIZE (sizeof (RNT) / sizeof (RNTEntry))

// XXX Actually a segment can have many references; N_SEGS is not the right #

static int searchRNT (char * name)
  {
    for (uint i = 0; i < RNT_TABLE_SIZE; i ++)
      if (RNT [i] . refName && strcmp (name, RNT [i] . refName) == 0)
        return i;
    return -1;
  }

static void addRNTRef (int idx, char * name)
  {
    int i = searchRNT (name);
    if (i >= 0)
      {
        segTable [RNT [i] . idx] . RNTRefCount ++;
        return;
      }
    for (uint i = 0; i < RNT_TABLE_SIZE; i ++)
      {
        if (! RNT [i] . refName)
          {
            RNT [i] . refName = strdup (name);
            RNT [i] . idx = idx;
            segTable [idx] . RNTRefCount = 1;
            return;
          }
      }
    sim_printf ("ERROR: RNT full\n");
  }

static void delRNTRef (char * name)
  {
    int i = searchRNT (name);
    if (i >= 0)
      {
        segTable [RNT [i] . idx] . RNTRefCount --;
        free (RNT [i] . refName);
        RNT [i] . refName = NULL;
        RNT [i] . idx = 0;
// XXX make unknown
        return;
      }
    //sim_printf ("ERROR: delRNTRef couldn't find %s\n", name);
  }



static void initializeDSEG (void)
  {

    // 0100 (64) - 0177 (127) Fault pairs
    // 0200 (128) - 0207 (135) SCU yblock
    // 0300 - 2377 descriptor segment: 1000 segments at 2 words per segment.
#define DESCSEG 0300
#define N_DESCS 01000

    //    org   0100 " Fault pairs
    //    bss   64
    //    org   0200
    //    bss   8
    //    org   0300
    //    bss   0400*2


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
    //DSBR . STACK = segTable [STK_SEG + 5] . segno >> 3;
  }


/* BEGIN INCLUDE FILE ... object_map.incl.pl1 */
/* coded February 8, 1972 by Michael J. Spier */
/* Last modified on 05/20/72 at 13:29:38 by R F Mabee. */
/* Made to agree with Spier's document on 20 May 1972 by R F Mabee. */
/* modified on 6 May 1972 by R F Mabee to add map_ptr at end of object map. */
/* modified May, 1972 by M. Weaver */
/* modified 5/75 by E. Wiatrowski and 6/75 by M. Weaver */
/* modified 5/77 by M. Weaver to add perprocess_static bit */

//++  /* Structure describing standard object map */
//++  declare  1 object_map aligned based,
typedef struct  __attribute__ ((__packed__)) object_map
  {
//++    2 decl_vers fixed bin,  /* Version number of current structure format */
    word36 decl_vers; /* Version number of current structure format */

//++    2 identifier char (8) aligned, /* Must be the constant "obj_map" */
    word36 identifier [2]; /* Must be the constant "obj_map" */

//++    2 text_offset bit (18) unaligned,     /* Offset relative to base of object segment of base of text section */
//++    2 text_length bit (18) unaligned,     /* Length in words of text section */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint text_length : 18; 
            uint text_offset : 18; 
          };
        word36 align1;
      };

//++    2 definition_offset bit (18) unaligned, /* Offset relative to base of object seg of base of definition section */
//++    2 definition_length bit (18) unaligned, /* Length in words of definition section */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint definition_length : 18; 
            uint definition_offset : 18; 
          };
        word36 align2;
      };

//++    2 linkage_offset bit (18) unaligned,  /* Offset relative to base of object seg of base of linkage section */
//++    2 linkage_length bit (18) unaligned,  /* Length in words of linkage section */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint linkage_length : 18; 
            uint linkage_offset : 18; 
          };
        word36 align3;
      };

//++    2 static_offset bit (18) unaligned,   /* Offset relative to base of obj seg of static section */
//++    2 static_length bit (18) unaligned,   /* Length in words of static section */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint static_length : 18; 
            uint static_offset : 18; 
          };
        word36 align4;
      };

//++    2 symbol_offset bit (18) unaligned,   /* Offset relative to base of object seg of base of symbol section */
//++    2 symbol_length bit (18) unaligned,   /* Length in words of symbol section */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint symbol_length : 18; 
            uint symbol_offset : 18; 
          };
        word36 align5;
      };

//++    2 break_map_offset bit (18) unaligned, /* Offset relative to base of object seg of base of break map */
//++    2 break_map_length bit (18) unaligned, /* Length in words of break map */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint break_map_length : 18; 
            uint break_map_offset : 18; 
          };
        word36 align6;
      };

//++    2 entry_bound bit (18) unaligned,     /* Offset in text of last gate entry */
//++    2 text_link_offset bit (18) unaligned, /* Offset of first text-embedded link */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint text_link_offset : 18; 
            uint entry_bound : 18; 
          };
        word36 align7;
      };

//++    2 format aligned,                     /* Word containing bit flags about object type */
//++    3 bound bit (1) unaligned,          /* On if segment is bound */
//++    3 relocatable bit (1) unaligned,    /* On if segment has relocation info in its first symbol block */
//++    3 procedure bit (1) unaligned,      /* On if segment is an executable object program */
//++    3 standard bit (1) unaligned,       /* On if segment is in standard format (more than just standard map) */
//++    3 separate_static bit(1) unaligned, /* On if static is a separate section from linkage */
//++    3 links_in_text bit (1) unaligned,  /* On if there are text-embedded links */
//++    3 perprocess_static bit (1) unaligned, /* On if static is not to be per run unit */
//++    3 unused bit (29) unaligned;        /* Reserved */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint unused : 29; 
            uint perprocess_static : 1; 
            uint links_in_text : 1; 
            uint separate_static : 1; 
            uint standard : 1; 
            uint procedure : 1; 
            uint relocatable : 1; 
            uint bound : 1; 
          } format;
        word36 align8;
      };
  } object_map;

typedef struct __attribute__ ((__packed__)) definition
  {
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint backward : 18;
            uint forward : 18;
          };
        word36 align1;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint class : 3;
            uint unused : 9;
            uint descriptors : 1;
            uint argcount : 1;
            uint retain : 1;
            uint entry : 1;
            uint ignore : 1;
            uint new : 1;
            uint value : 18;
          };
        word36 align2;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint segname : 18;
            uint symbol : 18;
          };
        word36 align3;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint descriptor_relp : 18;
            uint nargs : 18;
          };
        word36 align4;
      };
  } definition;

typedef struct __attribute__ ((__packed__)) link_
  {
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint tag: 6;
            uint run_depth: 6;
            uint mbz: 3;
            uint ringno: 3;
            uint header_relp: 18;
          };
        word36 align1;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint modifier: 6;
            uint mbz2 : 12;
            uint expression_relp: 18;
          };
        word36 align2;
      };
  } link_;

typedef struct __attribute__ ((__packed__)) expression
  {
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint exp: 18;
            uint type_ptr: 18;
          };
        word36 align1;
      };
  } expression;

typedef struct __attribute__ ((__packed__)) type_pair
  {
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint trap_ptr: 18;
            uint type: 18;
          };
        word36 align1;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint ext_ptr: 18;
            uint seg_ptr: 18;
          };
        word36 align2;
      };
  } type_pair;

//++ 
//++ declare   map_ptr bit(18) aligned based;          /* Last word of the segment. It points to the base of the object map. */
//++ 
//++ declare   object_map_version_2 fixed bin static init(2);

#define object_map_version_2 2

//++ 
//++ /* END INCLUDE FILE ... object_map.incl.pl1 */

static int lookupDef (int segIdx, char * segName, char * symbolName, word18 * value)
  {
    segTableEntry * e = segTable + segIdx;
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word36 * segp = M + segAddr;
    def_header * oip_defp = (def_header *) (segp + e -> definition_offset);

    word36 * defBase = (word36 *) oip_defp;

    definition * p = (definition *) (defBase +
                                     oip_defp -> def_list_relp);
    // Search for the segment

    definition * symDef = NULL;

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
        symDef = p;
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
            symDef = p;
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

static void installSDW (int segIdx)
  {
    segTableEntry * e = segTable + segIdx;
    word15 segno = e -> segno;
    // sim_printf ("install idx %d segno %o (%s) @ %08o len %d\n", segIdx, segno, e -> segname, e -> physmem, e -> seglen);
    word36 * even = M + DESCSEG + 2 * segno + 0;  
    word36 * odd  = M + DESCSEG + 2 * segno + 1;  

    putbits36 (even,  0, 24, e -> physmem);
    putbits36 (even, 24,  3, e -> R1);
    putbits36 (even, 27,  3, e -> R2);
    putbits36 (even, 30,  3, e -> R3);

    putbits36 (even, 33,  1, 1); // F: mark page as resident

    putbits36 (odd,   1, 14, e -> seglen >> 4); // BOUND
    putbits36 (odd,  15,  1, e -> R);
    putbits36 (odd,  16,  1, e -> E);
    putbits36 (odd,  17,  1, e -> W);
    putbits36 (odd,  18,  1, e -> P);
    putbits36 (odd,  19,  1, 1); // unpaged
    putbits36 (odd,  20,  1, e -> gated ? 1U : 0U);
    putbits36 (odd,  22, 14, e -> entry_bound >> 4);

    do_camp (TPR . CA);
    do_cams (TPR . CA);
  }

typedef struct trapNameTableEntry
  {
    char * segName;
    char * symbolName;
    int trapNo;
  } trapNameTableEntry;

static trapNameTableEntry trapNameTable [] =
  {
    { "hcs_", "history_regs_set", TRAP_HISTORY_REGS_SET },
    { "hcs_", "history_regs_get", TRAP_HISTORY_REGS_GET },
    { "hcs_", "fs_search_get_wdir", TRAP_FS_SEARCH_GET_WDIR },
    { "hcs_", "initiate_count", TRAP_INITIATE_COUNT },
    { "hcs_", "terminate_name", TRAP_TERMINATE_NAME },
    { "hcs_", "make_ptr", TRAP_MAKE_PTR },
    { "hcs_", "status_mins", TRAP_STATUS_MINS },
    { "get_line_length_", "switch", TRAP_GET_LINE_LENGTH_SWITCH }
  };
#define N_TRAP_NAMES (sizeof (trapNameTable) / sizeof (trapNameTableEntry))

static int trapName (char * segName, char * symbolName)
  {
    for (uint i = 0; i < N_TRAP_NAMES; i ++)
      {
        if (strcmp (segName, trapNameTable [i] . segName) == 0 &&
            strcmp (symbolName, trapNameTable [i] . symbolName) == 0)
          {
            return trapNameTable [i] . trapNo;
          }
      }
    return -1;
  }

static int resolveName (char * segName, char * symbolName, word15 * segno,
                        word18 * value, int * index)
  {
    int trapNo = trapName (segName, symbolName);
    if (trapNo >= 0)
      {
        * segno = TRAP_SEGNO;
        * value = trapNo;
        * index = -1;
        return 1;
      }
   
    int idx;
    if ((idx = searchRNT (segName)))
      {
        segTableEntry * e = segTable + idx;
        if (e -> allocated && e -> loaded && e -> definition_offset &&
            lookupDef (idx, segName, symbolName, value))
          {
            * segno = e -> segno;
            * index = idx;
            //sim_printf ("resoveName %s:%s %05o:%06o\n", segName, symbolName, * segno, * value);
            return 1;
          }
      }
    //sim_printf ("resolveName %s:%s\n", segName, symbolName);
    for (idx = 0; idx < (int) N_SEGS; idx ++)
      {
        segTableEntry * e = segTable + idx;
        if (! e -> allocated)
          continue;
        if (! e -> loaded)
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
    idx = loadSegmentFromFile (segName);
    if (idx >= 0)
      {
        //sim_printf ("got file\n");
        int slteIdx = -1;
        segTable [idx] . segno = getSegnoFromSLTE (segName, & slteIdx);
        if (! segTable [idx] . segno)
          {
            setSegno (idx, allocateSegno ());
            // sim_printf ("assigning %d to %s\n", segTable [idx] . segno, segName);
          }
        segTable [idx] . segname = strdup (segName);
        if (slteIdx < 0) // segment not in slte
          {
            segTable [idx] . R1 = FXE_RING;
            segTable [idx] . R2 = FXE_RING;
            segTable [idx] . R3 = FXE_RING;
            segTable [idx] . R = 1;
            segTable [idx] . E = 1;
            segTable [idx] . W = 0;
            segTable [idx] . P = 0;
          }
        else // segment in slte
          {
            segTable [idx] . R1 = SLTE [slteIdx] . R1;
            segTable [idx] . R2 = SLTE [slteIdx] . R2;
            segTable [idx] . R3 = SLTE [slteIdx] . R2;
            segTable [idx] . R = SLTE [slteIdx] . R;
            segTable [idx] . E = SLTE [slteIdx] . E;
            segTable [idx] . W = SLTE [slteIdx] . W;
            segTable [idx] . P = SLTE [slteIdx] . P;
          }
        installLOT (idx);
        installSDW (idx);
        if (lookupDef (idx, segName, symbolName, value))
          {
            * segno = segTable [idx] . segno;
            * index = idx;
            // sim_printf ("resoveName %s:%s %05o:%06o\n", segName, symbolName, * segno, * value);
            return 1;
          }
        // sim_printf ("found segment but not symbol\n");
        return 0; 
      }
    // sim_printf ("resoveName fail\n");
    return 0;
  }

static void parseSegment (int segIdx)
  {
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    segTableEntry * e = segTable + segIdx;
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
        sim_printf ("ERROR: mapPtr too big %06u >= %06u\n", i, seglen);
        return;
      }
    if (seglen - i - 1 < 11)
      {
        sim_printf ("ERROR: mapPtr too small %06u\n", seglen - i - 1);
        return;
      }

    object_map * mapp = (object_map *) (segp + i);

    if (mapp -> identifier [0] != 0157142152137LLU || // "obj_"
        mapp -> identifier [1] != 0155141160040LLU) // "map "
      {
        sim_printf ("ERROR: mapID wrong %012llo %012llo\n", 
                    mapp -> identifier [0], mapp -> identifier [1]);
        return;
      }

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
//sim_printf ("symbol length %o\n", mapp -> symbol_length);

//    word36 * oip_textp = segp + mapp -> text_offset;
    def_header * oip_defp = (def_header *) (segp + mapp -> definition_offset);
    link_header * oip_linkp = (link_header *) (segp + mapp -> linkage_offset);
//    word36 * oip_statp = segp + mapp -> static_offset;
//    word36 * oip_symbp = segp + mapp -> symbol_offset;
//    word36 * oip_bmapp = NULL;
//    if (mapp -> break_map_offset)
//      oip_bmapp = segp + mapp -> break_map_offset;
//    word18 oip_tlng = mapp -> text_length;
//    word18 oip_dlng = mapp -> definition_length;
//    word18 oip_llng = mapp -> linkage_length;
//    word18 oip_ilng = mapp -> static_length;
//    word18 oip_slng = mapp -> symbol_length;
//    word18 oip_blng = mapp -> break_map_length;
//    word1 oip_format_procedure = mapp -> format . procedure;
//    word1 oip_format_bound = mapp -> format . bound;
//    if (oip_format_bound)
//      sim_printf ("Segment is bound.\n");
//    else
//      sim_printf ("Segment is unbound.\n");
//    word1 oip_format_relocatable = mapp -> format . relocatable;
//    word1 oip_format_standard = mapp -> format . standard; /* could have standard obj. map but not std. seg. */
    bool oip_format_gate = mapp -> entry_bound != 0;
//    word1 oip_format_separate_static = mapp -> format . separate_static;
//    word1 oip_format_links_in_text = mapp -> format . links_in_text;
//    word1 oip_format_perprocess_static = mapp -> format . perprocess_static;
    word18 entry_bound = mapp -> entry_bound;
//    word18 textlinkp = mapp -> text_link_offset;

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

    // Walk the definiton
// oip_defp
    word36 * defBase = (word36 *) oip_defp;

//    sim_printf ("Definitions:\n");
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
  }

static void readSegment (int fd, int segIdx, off_t flen)
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
            sim_printf ("ERROR: garbage at end of segment lost\n");
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
    //segTable [segIdx] . seglen = seglen;
    segTable [segIdx] . seglen = flen * 8 / 36;
    segTable [segIdx] . loaded = true;
    //sim_printf ("Loaded %u words in segment index %d @ %08o\n", 
                //seglen, segIdx, segAddr);
    parseSegment (segIdx);
  }

#if 0
static int loadDeferred (char * arg)
  {
    int idx;
    for (idx = 0; idx < (int) N_SEGS; idx ++)
      if (segTable [idx] . allocated && segTable [idx] . segname &&
          strcmp (arg, segTable [idx] . segname) == 0)
        return idx;

    idx = allocateSegment ();
    if (idx < 0)
      {
        sim_printf ("Unable to allocate segment for segment %s\n", arg);
        return -1;
      }

    segTableEntry * e = segTable + idx;

    e -> segno = allocateSegno ();
    e -> R1 = FXE_RING;
    e -> R2 = FXE_RING;
    e -> R3 = FXE_RING;
    e -> R = 1;
    e -> E = 1;
    e -> W = 1;
    e -> P = 0;
    e -> segname = strdup (arg);
    e -> loaded = false;

    sim_printf ("Deferred %s in %d\n", arg, idx);
    return idx;
  }
#endif

static int loadSegmentFromFile (char * arg)
  {
    int fd = open (arg, O_RDONLY);
    if (fd < 0)
      {
        sim_printf ("ERROR: Unable to open '%s': %d\n", arg, errno);
        return -1;
      }

    off_t flen = lseek (fd, 0, SEEK_END);
    lseek (fd, 0, SEEK_SET);
    
    int segIdx = allocateSegment ();
    if (segIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate segment for segment load\n");
        return -1;
      }

    segTableEntry * e = segTable + segIdx;

    e -> R1 = FXE_RING;
    e -> R2 = FXE_RING;
    e -> R3 = FXE_RING;
    e -> R = 1;
    e -> E = 1;
    e -> W = 1;
    e -> P = 0;

    readSegment (fd, segIdx, flen);

    return segIdx;
  }

static void setupWiredSegments (void)
  {
     // allocate wired segments

     // 'dseg' contains the fault traps

     segTable [0] . allocated = true;
     segTable [0] . loaded = true;
     setSegno (0, 0);
     segTable [0] . wired = true;
     segTable [0] . R1 = 0;
     segTable [0] . R2 = 0;
     segTable [0] . R3 = 0;
     segTable [0] . R = 1;
     segTable [0] . E = 0;
     segTable [0] . W = 0;
     segTable [0] . P = 0;
     segTable [0] . segname = strdup ("dseg");
          
     initializeDSEG ();
  }


//++ /* BEGIN INCLUDE FILE iocbx.incl.pl1 */
//++ /* written 27 Dec 1973, M. G. Smith */
//++ /* returns attributes removed, hashing support BIM Spring 1981 */
//++ /* version made character string June 1981 BIM */
//++ /* Modified 11/29/82 by S. Krupp to add new entries and to change
//++       version number to IOX2. */
//++ /* format: style2 */
//++ 
//++      dcl	   1 iocb		      aligned based,	/* I/O control block. */
typedef struct  __attribute__ ((__packed__)) iocb
  {
//++ 	     2 version	      character (4) aligned,	/* IOX2 */
    word36 version; // 0
//++ 	     2 name	      char (32),		/* I/O name of this block. */
    word36 name [8]; // 1
//++ 	     2 actual_iocb_ptr    ptr,		/* IOCB ultimately SYNed to. */
    word36 make_even1; // 9
    word72 actual_iocb_ptr; // 10
//++ 	     2 attach_descrip_ptr ptr,		/* Ptr to printable attach description. */
    word72 attach_descrip_ptr; // 12
//++ 	     2 attach_data_ptr    ptr,		/* Ptr to attach data structure. */
    word72 attach_data_ptr;  // 14
//++ 	     2 open_descrip_ptr   ptr,		/* Ptr to printable open description. */
    word72 open_descrip_ptr; // 16
//++ 	     2 open_data_ptr      ptr,		/* Ptr to open data structure (old SDB). */
    word72 open_data_ptr; // 18
//++ 	     2 event_channel      bit (72),		/* Event channel for asynchronous I/O. */
    word72 event_channel; // 20
//++ 	     2 detach_iocb	      entry (ptr, fixed bin (35)),
//++ 						/* detach_iocb(p) */
    word72 detach_iocb [2]; // 22
//++ 	     2 open	      entry (ptr, fixed, bit (1) aligned, fixed bin (35)),
//++ 						/* open(p,mode,not_used) */
    word72 open [2]; // 26
//++ 	     2 close	      entry (ptr, fixed bin (35)),
//++ 						/* close(p) */
    word72 close [2]; // 30
//++ 	     2 get_line	      entry (ptr, ptr, fixed (21), fixed (21), fixed bin (35)),
//++ 						/* get_line(p,bufptr,buflen,actlen) */
    word72 get_line [2]; // 34
//++ 	     2 get_chars	      entry (ptr, ptr, fixed (21), fixed (21), fixed bin (35)),
    word72 get_chars [2]; // 38
//++ 						/* get_chars(p,bufptr,buflen,actlen) */
//++ 	     2 put_chars	      entry (ptr, ptr, fixed (21), fixed bin (35)),
//++ 						/* put_chars(p,bufptr,buflen) */
    word72 put_chars [2]; // 42
//++ 	     2 modes	      entry (ptr, char (*), char (*), fixed bin (35)),
//++ 						/* modes(p,newmode,oldmode) */
    word72 modes [2]; // 46
//++ 	     2 position	      entry (ptr, fixed, fixed (21), fixed bin (35)),
//++ 						/* position(p,u1,u2) */
    word72 position [2]; // 50
//++ 	     2 control	      entry (ptr, char (*), ptr, fixed bin (35)),
//++ 						/* control(p,order,infptr) */
    word72 control [2]; // 54
//++ 	     2 read_record	      entry (ptr, ptr, fixed (21), fixed (21), fixed bin (35)),
//++ 						/* read_record(p,bufptr,buflen,actlen) */
    word72 read_record [2]; // 58
//++ 	     2 write_record	      entry (ptr, ptr, fixed (21), fixed bin (35)),
//++ 						/* write_record(p,bufptr,buflen) */
    word72 write_record [2]; // 62
//++ 	     2 rewrite_record     entry (ptr, ptr, fixed (21), fixed bin (35)),
//++ 						/* rewrite_record(p,bufptr,buflen) */
    word72 rewrite_record [2]; // 66
//++ 	     2 delete_record      entry (ptr, fixed bin (35)),
//++ 						/* delete_record(p) */
    word72 delete_record [2]; // 70
//++ 	     2 seek_key	      entry (ptr, char (256) varying, fixed (21), fixed bin (35)),
//++ 						/* seek_key(p,key,len) */
    word72 seek_key [2]; // 74
//++ 	     2 read_key	      entry (ptr, char (256) varying, fixed (21), fixed bin (35)),
//++ 						/* read_key(p,key,len) */
    word72 read_key [2]; // 78
//++ 	     2 read_length	      entry (ptr, fixed (21), fixed bin (35)),
//++ 						/* read_length(p,len) */
    word72 read_length [2]; // 82
//++ 	     2 open_file	      entry (ptr, fixed bin, char (*), bit (1) aligned, fixed bin (35)),
//++ 						/* open_file(p,mode,desc,not_used,s) */
    word72 open_file [2]; // 86
//++ 	     2 close_file	      entry (ptr, char (*), fixed bin (35)),
//++ 						/* close_file(p,desc,s) */
    word72 close_file [2]; // 90
//++ 	     2 detach	      entry (ptr, char (*), fixed bin (35)),
//++ 						/* detach(p,desc,s) */
//++ 						/* Hidden information, to support SYN attachments. */
    word72 detach [2]; // 94
//++ 	     2 ios_compatibility  ptr,		/* Ptr to old DIM's IOS transfer vector. */
    word72 ios_compatibility; // 98
//++ 	     2 syn_inhibits	      bit (36),		/* Operations inhibited by SYN. */
    word36 syn_inhibits; // 100
//++ 	     2 syn_father	      ptr,		/* IOCB immediately SYNed to. */
    word36 make_even2; // 101
    word72 syn_father; // 102
//++ 	     2 syn_brother	      ptr,		/* Next IOCB SYNed as this one is. */
    word72 syn_brother; // 104
//++ 	     2 syn_son	      ptr,		/* First IOCB SYNed to this one. */
    word72 syn_son; // 106
//++ 	     2 hash_chain_ptr     ptr;		/* Next IOCB in hash bucket */
    word72 hash_chain_ptr; // 108
//++ 
//++      declare iox_$iocb_version_sentinel
//++ 			      character (4) aligned external static;
//++ 
  } iocb;

//++ /* END INCLUDE FILE iocbx.incl.pl1 */

//++ /*  BEGIN INCLUDE FILE ... stack_header.incl.pl1 .. 3/72 Bill Silver  */
//++ /* modified 7/76 by M. Weaver for *system links and more system use of areas */
//++ /* modified 3/77 by M. Weaver to add rnt_ptr */
//++ /* Modified April 1983 by C. Hornig for tasking */
//++ 
//++ /****^  HISTORY COMMENTS:
//++   1) change(86-06-24,DGHowe), approve(86-06-24,MCR7396),
//++      audit(86-08-05,Schroth), install(86-11-03,MR12.0-1206):
//++      added the heap_header_ptr definition.
//++   2) change(86-08-12,Kissel), approve(86-08-12,MCR7473),
//++      audit(86-10-10,Fawcett), install(86-11-03,MR12.0-1206):
//++      Modified to support control point management.  These changes were actually
//++      made in February 1985 by G. Palter.
//++   3) change(86-10-22,Fawcett), approve(86-10-22,MCR7473),
//++      audit(86-10-22,Farley), install(86-11-03,MR12.0-1206):
//++      Remove the old_lot pointer and replace it with cpm_data_ptr. Use the 18
//++      bit pad after cur_lot_size for the cpm_enabled. This was done to save some
//++      space int the stack header and change the cpd_ptr unal to cpm_data_ptr
//++      (ITS pair).
//++                                                    END HISTORY COMMENTS */
//++ 
//++ /* format: style2 */
//++ 
//++      dcl    sb        ptr;  /* the  main pointer to the stack header */
//++ 
//++      dcl    1 stack_header       based (sb) aligned,
typedef struct __attribute__ ((__packed__)) stack_header
  {

//++       2 pad1       (4) fixed bin, /*  (0) also used as arg list by outward_call_handler  */
    word36 pad1 [4];

//++       2 cpm_data_ptr       ptr,  /*  (4)  pointer to control point which owns this stack */
    word72 cpm_data_ptr;

//++       2 combined_stat_ptr  ptr,  /*  (6)  pointer to area containing separate static */
    word72 combined_stat_ptr;

//++       2 clr_ptr       ptr,  /*  (8)  pointer to area containing linkage sections */
    word72 clr_ptr;

//++       2 max_lot_size       fixed bin (17) unal, /*  (10) DU  number of words allowed in lot */
//++       2 main_proc_invoked  fixed bin (11) unal, /*  (10) DL  nonzero if main procedure invoked in run unit */
//++       2 have_static_vlas   bit (1) unal,  /*  (10) DL  "1"b if (very) large arrays are being used in static */
//++       2 pad4       bit (2) unal,
//++       2 run_unit_depth     fixed bin (2) unal, /*  (10) DL  number of active run units stacked */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint run_unit_depth : 3;
            uint pad4 : 2;
            uint have_static_vlas : 1;
            uint main_proc_invoked : 12;
            uint max_lot_size :18;
          };
        word36 align1;
      };

//++       2 cur_lot_size       fixed bin (17) unal, /*  (11) DU  number of words (entries) in lot */
//++       2 cpm_enabled       bit (18) unal, /*  (11) DL  non-zero if control point management is enabled */
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint cpm_enabled : 18;
            uint cur_lot_size : 18;
          };
        word36 align2;
      };

//++       2 system_free_ptr    ptr,  /*  (12)  pointer to system storage area */
    word72 system_free_ptr;

//++       2 user_free_ptr      ptr,  /*  (14)  pointer to user storage area */
    word72 user_free_ptr;

//++       2 null_ptr       ptr,  /*  (16)  */
    word72 null_ptr;

//++       2 stack_begin_ptr    ptr,  /*  (18)  pointer to first stack frame on the stack */
    word72 stack_begin_ptr;

//++       2 stack_end_ptr      ptr,  /*  (20)  pointer to next useable stack frame */
    word72 stack_end_ptr;

//++       2 lot_ptr       ptr,  /*  (22)  pointer to the lot for the current ring */
    word72 lot_ptr;

//++       2 signal_ptr       ptr,  /*  (24)  pointer to signal procedure for current ring */
    word72 signal_ptr;

//++       2 bar_mode_sp       ptr,  /*  (26)  value of sp before entering bar mode */
    word72 bar_mode_sp;

//++       2 pl1_operators_ptr  ptr,  /*  (28)  pointer to pl1_operators_$operator_table */
    word72 pl1_operators_ptr;

//++       2 call_op_ptr       ptr,  /*  (30)  pointer to standard call operator */
    word72 call_op_ptr;

//++       2 push_op_ptr       ptr,  /*  (32)  pointer to standard push operator */
    word72 push_op_ptr;

//++       2 return_op_ptr      ptr,  /*  (34)  pointer to standard return operator */
    word72 return_op_ptr;

//++       2 return_no_pop_op_ptr
//++          ptr,  /*  (36)  pointer to standard return / no pop operator */
    word72 return_no_pop_op_ptr;

//++       2 entry_op_ptr       ptr,  /*  (38)  pointer to standard entry operator */
    word72 entry_op_ptr;

//++       2 trans_op_tv_ptr    ptr,  /*  (40)  pointer to translator operator ptrs */
    word72 trans_op_tv_ptr;

//++       2 isot_ptr       ptr,  /*  (42)  pointer to ISOT */
    word72 isot_ptr;

//++       2 sct_ptr       ptr,  /*  (44)  pointer to System Condition Table */
    word72 sct_ptr;

//++       2 unwinder_ptr       ptr,  /*  (46)  pointer to unwinder for current ring */
    word72 unwinder_ptr;

//++       2 sys_link_info_ptr  ptr,  /*  (48)  pointer to *system link name table */
    word72 sys_link_info_ptr;

//++       2 rnt_ptr       ptr,  /*  (50)  pointer to Reference Name Table */
    word72 rnt_ptr;

//++       2 ect_ptr       ptr,  /*  (52)  pointer to event channel table */
    word72 ect_ptr;

//++       2 assign_linkage_ptr ptr,  /*  (54)  pointer to storage for (obsolete) hcs_$assign_linkage */
    word72 assign_linkage_ptr;

//++       2 heap_header_ptr     ptr,  /*  (56)  pointer to the heap header for this ring */
    word72 heap_header_ptr;

//++       2 trace,
//++         3 frames,
//++           4 count       fixed bin,  /*  (58)  number of trace frames */
    word36 trace_frames_count;

//++           4 top_ptr       ptr unal,  /*  (59)  pointer to last trace frame */
    word72 trace_frames_top_ptr;

//++         3 in_trace       bit (36) aligned, /*  (60)  trace antirecursion flag */
    word36 in_trace;

//++       2 pad2       bit (36),  /*  (61) */
    word36 pad2;

//++                2 pad5       pointer;  /*  (62)  pointer to future stuff */
    word72 pad5;
  } stack_header;

//++ 
//++ /* The following offset refers to a table within the  pl1  operator table.  */
//++ 
//++      dcl    tv_offset       fixed bin init (361) internal static;
//++       /* (551) octal */
//++ 
//++ 
//++ /* The following constants are offsets within this transfer vector table.  */
//++ 
//++      dcl    (
//++     call_offset       fixed bin init (271),
//++     push_offset       fixed bin init (272),
//++     return_offset       fixed bin init (273),
//++     return_no_pop_offset   fixed bin init (274),
//++     entry_offset       fixed bin init (275)
//++     )        internal static;
//++ 
//++ 
//++ /* The following declaration  is an overlay of the whole stack header.   Procedures which
//++  move the whole stack header should use this overlay.
//++ */
//++ 
//++      dcl    stack_header_overlay   (size (stack_header)) fixed bin based (sb);
//++ 
//++ 
//++ /*  END INCLUDE FILE ... stack_header.incl.pl1 */

static int stack0Idx;

static void createStackSegments (void)
  {
    for (int i = 0; i < 8; i ++)
      {
        int ssIdx = allocateSegment ();
        if (i == 0)
          stack0Idx = ssIdx;
        segTableEntry * e = segTable + ssIdx;
        e -> segname = strdup ("stack_0");
        e -> segname [6] += i; // do not try this at home
        setSegno (ssIdx, STACKS_SEGNO + i);
        e -> R1 = i;
        e -> R2 = FXE_RING;
        e -> R3 = FXE_RING;
        e -> R = 1;
        e -> E = 0;
        e -> W = 1;
        e -> P = 0;
        e -> seglen = 0777777;
        e -> loaded = true;
        word24 segAddr = lookupSegAddrByIdx (ssIdx);
        memset (M + segAddr, 0, sizeof (stack_header));

        installSDW (ssIdx);
      }
  }

//++ "	BEGIN INCLUDE FILE ... stack_header.incl.alm  3/72  Bill Silver
//++ "
//++ "	modified 7/76 by M. Weaver for *system links and more system use of areas
//++ "	modified 3/77 by M. Weaver  to add rnt_ptr
//++ "	modified 7/77 by S. Webber to add run_unit_depth and assign_linkage_ptr
//++ "	modified 6/83 by J. Ives to add trace_frames and in_trace.
//++ 
//++ " HISTORY COMMENTS:
//++ "  1) change(86-06-24,DGHowe), approve(86-06-24,MCR7396),
//++ "     audit(86-08-05,Schroth), install(86-11-03,MR12.0-1206):
//++ "     added the heap_header_ptr definition
//++ "  2) change(86-08-12,Kissel), approve(86-08-12,MCR7473),
//++ "     audit(86-10-10,Fawcett), install(86-11-03,MR12.0-1206):
//++ "     Modified to support control point management.  These changes were
//++ "     actually made in February 1985 by G. Palter.
//++ "  3) change(86-10-22,Fawcett), approve(86-10-22,MCR7473),
//++ "     audit(86-10-22,Farley), install(86-11-03,MR12.0-1206):
//++ "     Remove the old_lot pointer and replace it with cpm_data_ptr. Use the 18
//++ "     bit pad after cur_lot_size for the cpm_enabled. This was done to save
//++ "     some space int the stack header and change the cpd_ptr unal to
//++ "     cpm_data_ptr (ITS pair).
//++ "                                                      END HISTORY COMMENTS
//++ 
//++ 	equ	stack_header.cpm_data_ptr,4		ptr to control point for this stack
//++ 	equ	stack_header.combined_stat_ptr,6	ptr to separate static area
//++ 
//++ 	equ	stack_header.clr_ptr,8		ptr to area containing linkage sections
//++ 	equ	stack_header.max_lot_size,10		number of words allowed in lot (DU)
//++ 	equ	stack_header.main_proc_invoked,10	nonzero if main proc was invoked in run unit (DL)
//++ 	equ	stack_header.run_unit_depth,10	number of active run units stacked (DL)
//++ 	equ	stack_header.cur_lot_size,11		DU number of words (entries) in lot
//++           equ	stack_header.cpm_enabled,11		DL  non-zero if control point management is enabled
//++ 	equ	stack_header.system_free_ptr,12	ptr to system storage area
//++ 	equ	stack_header.user_free_ptr,14		ptr to user storage area
//++ 
//++ 	equ	stack_header.parent_ptr,16		ptr to parent stack or null
//++ 	equ	stack_header.stack_begin_ptr,18	ptr to first stack frame
//++ 	equ	stack_header.stack_end_ptr,20		ptr to next useable stack frame
//++ 	equ	stack_header.lot_ptr,22		ptr to the lot for the current ring
//++ 
//++ 	equ	stack_header.signal_ptr,24		ptr to signal proc for current ring
//++ 	equ	stack_header.bar_mode_sp,26		value of sp before entering bar mode
//++ 	equ	stack_header.pl1_operators_ptr,28	ptr: pl1_operators_$operator_table
//++ 	equ	stack_header.call_op_ptr,30		ptr to standard call operator
//++ 
//++ 	equ	stack_header.push_op_ptr,32		ptr to standard push operator
//++ 	equ	stack_header.return_op_ptr,34		ptr to standard return operator
//++ 	equ	stack_header.ret_no_pop_op_ptr,36	ptr: stand. return/ no pop operator
//++ 	equ	stack_header.entry_op_ptr,38		ptr to standard entry operator
//++ 
//++ 	equ	stack_header.trans_op_tv_ptr,40	ptr to table of translator operator ptrs
//++ 	equ	stack_header.isot_ptr,42		pointer to ISOT
//++ 	equ	stack_header.sct_ptr,44		pointer to System Condition Table
//++ 	equ	stack_header.unwinder_ptr,46		pointer to unwinder for current ring
//++ 
//++ 	equ	stack_header.sys_link_info_ptr,48	ptr to *system link name table
//++ 	equ	stack_header.rnt_ptr,50		ptr to reference name table
//++ 	equ	stack_header.ect_ptr,52		ptr to event channel table
//++ 	equ	stack_header.assign_linkage_ptr,54	ptr to area for hcs_$assign_linkage calls
//++ 	equ	stack_header.heap_header_ptr,56	ptr to heap header.
//++ 	equ	stack_header.trace_frames,58		stack of trace_catch_ frames
//++ 	equ	stach_header.trace_top_ptr,59		trace pointer
//++ 	equ	stack_header.in_trace,60		trace antirecurse bit
//++ 	equ	stack_header_end,64			length of stack header
//++ 
//++ 
//++ 
//++ 
//++ 	equ	trace_frames.count,0		number of trace frames on stack
//++ 	equ	trace_frames.top_ptr,1		packed pointer to top one
//++ 
//++ "	The  following constant is an offset within the  pl1  operators table.
//++ "	It  references a  transfer vector table.
//++ 
//++ 	bool	tv_offset,551
#define tv_offset 0551
//++ 
//++ 
//++ "	The  following constants are offsets within this transfer vector table.
//++ 
//++ 	equ	call_offset,tv_offset+271
#define call_offset (tv_offset+271)
//++ 	equ	push_offset,tv_offset+272
#define push_offset (tv_offset+272)
//++ 	equ	return_offset,tv_offset+273
#define return_offset (tv_offset+273)
//++ 	equ	return_no_pop_offset,tv_offset+274
#define return_no_pop_offset (tv_offset+274)
//++ 	equ	entry_offset,tv_offset+275
#define entry_offset (tv_offset+275)
//++ 
//++ 
//++ " 	END INCLUDE FILE stack_header.incl.alm

static void initStack (int ssIdx)
  {
// bound_file_system.s.archive has a 'MAKESTACK' function;
// borrowing from there.

#define HDR_OFFSET 0
#define HDR_SIZE 64
// we assume HDR_SIZE is a multiple of 8, since frames must be so aligned
#define STK_TOP HDR_SIZE

    word15 stkSegno = segTable [ssIdx] . segno;
    word24 segAddr = lookupSegAddrByIdx (ssIdx);
    word24 hdrAddr = segAddr + HDR_OFFSET;

    word15 libSegno = segTable [libIdx] . segno;
    word3 libRing = segTable [libIdx] . R1;
    word24 libAddr = lookupSegAddrByIdx (libIdx);

    // To help with debugging, initialize the stack header with
    // null ptrs
    for (int i = 0; i < HDR_SIZE; i += 2)
      makeNullPtr (M + hdrAddr + i);

    // word  0,  1    reserved

    // word  2,  3    reserved

    // word  4,  5    odd_lot_ptr

    // word  6,  7    combined_stat_ptr

    // word  8,  9    clr_ptr

    // word 10        max_lot_size, run_unit_depth
    M [hdrAddr + 10] = ((word36) LOT_SIZE << 18) | RUN_UNIT_DEPTH;

    // word 11        cur_lot_size, pad2
    M [hdrAddr + 11] = ((word36) LOT_SIZE << 18) | 0;

    // word 12, 13    system_storage_ptr

    // word 14, 15    user_storage_ptr

    // word 16, 17    null_ptr
    makeNullPtr (M + hdrAddr + 16);

    // word 18, 19    stack_begin_ptr
    makeITS (M + hdrAddr + 18, stkSegno, ssIdx - stack0Idx, STK_TOP, 0, 0);

    // word 20, 21    stack_end_ptr
    makeITS (M + hdrAddr + 20, stkSegno, ssIdx - stack0Idx, STK_TOP, 0, 0);

    // word 22, 23    lot_ptr
    makeITS (M + hdrAddr + 22, CLR_SEGNO, ssIdx - stack0Idx, LOT_OFFSET, 0, 0); 

    // word 24, 25    signal_ptr

    // word 26, 27    bar_mode_sp_ptr

    // word 28, 29    pl1_operators_table
    word18 operator_table = 0777777;
    if (! lookupDef (libIdx, "pl1_operators_", "operator_table",
                     & operator_table))
     {
       sim_printf ("ERROR: Can't find pl1_operators_$operator_table\n");
     }
    makeITS (M + hdrAddr + 28, libSegno, ssIdx - stack0Idx, operator_table, 0, 0);

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

    // word 48, 49    sys_link_info_ptr

    // word 50, 51    rnt_ptr

    // word 52, 53    ect_ptr
    makeNullPtr (M + hdrAddr + 52);

    // word 54, 55    assign_linkage_ptr

    // word 56, 57    reserved

    // word 58, 59    reserved

    // word 60, 61    reserved

    // word 62, 63    reserved

  }

static void createFrame (int ssIdx, word15 prevSegno, word18 prevWordno, word3 prevRing)
  {
    word15 stkSegno = segTable [ssIdx] . segno;
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
    makeITS (M + frameAddr + 18, stkSegno, ssIdx - stack0Idx, STK_TOP + FRAME_SZ, 0, 0);

    // word 20, 21    return_ptr
    //makeNullPtr (M + frameAddr + 20);
    makeITS (M + frameAddr + 20, TRAP_SEGNO, FXE_RING, TRAP_RETURN_TO_FXE, 0, 0);
    
    // word 22, 23    entry_ptr
    makeNullPtr (M + frameAddr + 22);

    // word 24, 25    operator_link_ptr
    //makeNullPtr (M + frameAddr + 24);
    word18 operator_table = 0777777;
    if (! lookupDef (libIdx, "pl1_operators_", "operator_table",
                     & operator_table))
     {
       sim_printf ("ERROR: Can't find pl1_operators_$operator_table\n");
     }
    word15 libSegno = segTable [libIdx] . segno;
    makeITS (M + frameAddr + 24, libSegno, FXE_RING, operator_table, 0, 0);


    // word 25, 26    argument_ptr
    makeNullPtr (M + frameAddr + 26);

    // Update the header

    // word 18, 19    stack_begin_ptr
    word24 hdrAddr = segAddr + HDR_OFFSET;
    makeITS (M + hdrAddr + 18, stkSegno, ssIdx - stack0Idx, STK_TOP, 0, 0);

    // word 20, 21    stack_end_ptr
    makeITS (M + hdrAddr + 20, stkSegno, ssIdx - stack0Idx, STK_TOP + FRAME_SZ, 0, 0);

  }

static int installLibrary (char * name)
  {
    int idx = loadSegmentFromFile (name);
    int slteIdx = -1;
    int segno = getSegnoFromSLTE (name, & slteIdx);
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
    //sim_printf ("lib %s segno %o\n", name, segTable [idx] . segno);
    segTable [idx] . segname = strdup (name);
    if (slteIdx < 0) // segment not in slte
      {
        segTable [idx] . R1 = FXE_RING;
        segTable [idx] . R2 = FXE_RING;
        segTable [idx] . R3 = FXE_RING;
        segTable [idx] . R = 1;
        segTable [idx] . E = 1;
        segTable [idx] . W = 0;
        segTable [idx] . P = 0;
      }
    else // segment in slte
      {
        segTable [idx] . R1 = SLTE [slteIdx] . R1;
        segTable [idx] . R2 = SLTE [slteIdx] . R2;
        segTable [idx] . R3 = SLTE [slteIdx] . R2;
        segTable [idx] . R = SLTE [slteIdx] . R;
        segTable [idx] . E = SLTE [slteIdx] . E;
        segTable [idx] . W = SLTE [slteIdx] . W;
        segTable [idx] . P = SLTE [slteIdx] . P;
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
    int defIdx;

    if (resolveName (segName, compName, & segno, & value, & defIdx))
      {
        if (defIdx < 0)
          sim_printf ("ERROR: dazed and confused; %s.%s has no idx\n",
                      segName, compName);
        else
          {
            //M [segTable [defIdx] . physmem + value + offset] = word;
            if (vary)
              strcpyVarying (segTable [defIdx] . physmem, & value, str);
            else
              strcpyNonVarying (segTable [defIdx] . physmem, & value, str);
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
    int defIdx;

    if (resolveName (segName, compName, & segno, & value, & defIdx))
      {
        if (defIdx < 0)
          sim_printf ("ERROR: dazed and confused; %s.%s has no idx\n",
                      segName, compName);
        else
          M [segTable [defIdx] . physmem + value + offset] = word;
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
    initSysinfoWord36 ("sys_info", "service_system", (word36) (1));
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

// XXX
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

//
// fxe - load a segment into memory and execute it
//

t_stat fxe (int32 __attribute__((unused)) arg, char * buf)
  {
    sim_printf ("FXE: initializing...\n");

    // Reset all state data
    memset (segTable, 0, sizeof (segTable));
    memset (segnoMap, -1, sizeof (segnoMap));
    memset (RNT, 0, sizeof (RNT));
    nextSegno = USER_SEGNO;
    nextPhysmem = 1;
    clrFreePtr = CLR_FREE_START;
    
    setupWiredSegments ();

// Setup CLR

    clrIdx = allocateSegment ();
    if (clrIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate clr segment\n");
        return -1;
      }

    segTableEntry * clrEntry = segTable + clrIdx;

    clrEntry -> R1 = 0;
    clrEntry -> R2 = FXE_RING;
    clrEntry -> R3 = FXE_RING;
    clrEntry -> R = 1;
    clrEntry -> E = 0;
    clrEntry -> W = 1;
    clrEntry -> P = 0;
    clrEntry -> segname = strdup ("clr");
    setSegno (clrIdx, CLR_SEGNO);
    //clrEntry -> seglen = 2 * LOT_SIZE;
    clrEntry -> seglen = 0777777;
    clrEntry -> loaded = true;
    installSDW (clrIdx);

    word36 * clrMemory = M + clrEntry -> physmem;

    for (uint i = 0; i < LOT_SIZE; i ++)
      clrMemory [i] = 0007777000000; // bitno 0, seg 7777, word 0


// Setup IOCB

    int iocbIdx = allocateSegment ();
    if (iocbIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate iocb segment\n");
        return -1;
      }

    segTableEntry * iocbEntry = segTable + iocbIdx;

    iocbEntry -> R1 = 0;
    iocbEntry -> R2 = FXE_RING;
    iocbEntry -> R3 = FXE_RING;
    iocbEntry -> R = 1;
    iocbEntry -> E = 0;
    iocbEntry -> W = 1;
    iocbEntry -> P = 0;
    iocbEntry -> segname = strdup ("iocb");
    setSegno (iocbIdx, IOCB_SEGNO);
    iocbEntry -> seglen = 0777777;
    iocbEntry -> loaded = true;
    installSDW (iocbIdx);

#define IOCB_USER_OUTPUT 0

    // Define iocb [IOCB_USER_OUTPUT]

    iocb * iocbUser =
      (iocb *) (M + iocbEntry -> physmem + IOCB_USER_OUTPUT * sizeof (iocb));

    // iocb [IOCB_USER_OUTPUT] . version

    word36 * versionEntry = (word36 *) & iocbUser -> version;

    * versionEntry = 0111117130062LLU; // 'IOX2'

    // iocb [IOCB_USER_OUTPUT] . put_chars

    word36 * putCharEntry = (word36 *) & iocbUser -> put_chars;

    makeITS (putCharEntry, TRAP_SEGNO, FXE_RING, TRAP_PUT_CHARS, 0, 0);
    makeNullPtr (putCharEntry + 2);

#if 0
    // iocb [IOCB_USER_OUTPUT] . modes

    word36 * modesEntry = (word36 *) & iocbUser -> modes;

    makeITS (modesEntry, TRAP_SEGNO, FXE_RING, TRAP_MODES, 0, 0);
    makeNullPtr (modesEntry + 2);
#endif

    // sim_printf ("Loading library segment\n");

    libIdx = installLibrary ("bound_library_wired_");

    installLibrary ("bound_library_1_");
    installLibrary ("bound_library_2_");
    installLibrary ("bound_process_env_");
    installLibrary ("bound_expand_path_");
    //installLibrary ("bound_io_commands_");
    installLibrary ("error_table_");
    //int lib2Idx = installLibrary ("bound_bce_wired");
    installLibrary ("sys_info");
    initSysinfo ();

    word18 value;
    word15 segno;
    int defIdx;

    // Set iox_$user_output

    if (resolveName ("iox_", "user_output", & segno, & value, & defIdx))
      {
        //if (defIdx < 0)
          //sim_printf ("ERROR: dazed and confused; iox_ has no idx\n");
        //else
          //M [segTable [infoIdx] . physmem + value] = 1;
      }
    else
      {
        sim_printf ("ERROR: can't find iox_:user_output\n");
      }
    //sim_printf ("iox_:user_output %05o:%06o\n", segno, value);
    makeITS (M + segTable [defIdx] . physmem + value, IOCB_SEGNO, FXE_RING,
             IOCB_USER_OUTPUT * sizeof (iocb), 0, 0);

    // Create the fxe segment

    int fxeIdx = allocateSegment ();
    if (fxeIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate fxe segment\n");
        return -1;
      }
    installLOT (fxeIdx);
    segTableEntry * fxeEntry = segTable + fxeIdx;

    fxeEntry -> seglen = 0777777;
    fxeEntry -> R1 = FXE_RING;
    fxeEntry -> R2 = FXE_RING;
    fxeEntry -> R3 = FXE_RING;
    fxeEntry -> R = 1;
    fxeEntry -> E = 1;
    fxeEntry -> W = 1;
    fxeEntry -> P = 0;
    fxeEntry -> segname = strdup ("fxe");
    setSegno (fxeIdx, FXE_SEGNO);
    fxeEntry -> loaded = true;
    installSDW (fxeIdx);


    FXEinitialized = true;

#define maxargs 10
    char * args [maxargs];
    for (int i = 0; i < maxargs; i ++)
      args [i] = malloc (strlen (buf) + 1);
    char * segmentName = malloc (strlen (buf) + 1);
    int nargs = sscanf (buf, "%s%s%s%s%s%s%s%s%s%s%s", 
                    segmentName,
                    args [0], args [1], args [2], args [3], args [4],
                    args [5], args [6], args [7], args [8], args [9]);
    if (nargs >= 1)
      {
        nargs --; // don't count the segment name
        sim_printf ("Loading segment %s\n", segmentName);
        int segIdx = loadSegmentFromFile (segmentName);
        segTable [segIdx] . segname = strdup (segmentName);
        setSegno (segIdx, allocateSegno ());
        segTable [segIdx] . R1 = FXE_RING;
        segTable [segIdx] . R2 = FXE_RING;
        segTable [segIdx] . R3 = FXE_RING;
        segTable [segIdx] . R = 1;
        segTable [segIdx] . E = 1;
        segTable [segIdx] . W = 0;
        segTable [segIdx] . P = 0;
        segTable [segIdx] . segname = strdup (segmentName);

        installSDW (segIdx);
        installLOT (segIdx);
        // sim_printf ("executed segment idx %d, segno %o, phyaddr %08o\n", 
        // segIdx, segTable [segIdx] . segno, lookupSegAddrByIdx (segIdx));
        createStackSegments ();
        // sim_printf ("stack segment idx %d, segno %o, phyaddr %08o\n", 
        // stack0Idx + FXE_RING, segTable [stack0Idx + FXE_RING] . segno, lookupSegAddrByIdx (stack0Idx + FXE_RING));
        // sim_printf ("lib segment idx %d, segno %o, phyaddr %08o\n", 
        // libIdx, segTable [libIdx] . segno, lookupSegAddrByIdx (libIdx));

        initStack (stack0Idx + 0);
        createFrame (stack0Idx + 0, 077777, 1, 0);
        initStack (stack0Idx + FXE_RING);
        createFrame (stack0Idx + FXE_RING, segTable [stack0Idx] . segno, STK_TOP, 0);


#if 0
        sim_printf ("\nSegment table\n------- -----\n\n");
        for (int idx = 0; idx < (int) N_SEGS; idx ++)
          {
            segTableEntry * e = segTable + idx;
            if (! e -> allocated)
              continue;
            sim_printf ("%3d %c %06o %08o %06o %s\n", 
                        idx, e -> loaded ? ' ' : '*',
                        e -> segno, e -> physmem,
                        e -> seglen, 
                        e -> segname ? e -> segname : "anon");
          }
        sim_printf ("\n");
#endif

// AK92, pg 2-13: PR0 points to the argument list

        // Create the argument list.

        // Creating in memory
        //     arg1 len
        //     arg1
        //     arg2 len
        //     arg2
        //     arg3 len
        //     arg3
        //     arg4 len
        //     arg4
        //     desc1
        //     desc2
        //     desc3
        //     desc4
        //     arg list
        //        nargs, type
        //        desc_cnt
        //        arg1 ptr
        //        arg2 ptr
        //        arg3 ptr
        //        arg4 ptr
        //        desc1 ptr
        //        desc2 ptr
        //        desc3 ptr
        //        desc4 ptr

        word24 fxeMemPtr = fxeEntry -> physmem;
        word18 next = 0;

        //word36 argList = fxeMemPtr + next;
        word18 argAddrs [10];

        for (int i = 0; i < nargs; i ++)
          {
            argAddrs [i] = next;
            strcpyNonVarying (fxeMemPtr, & next, args [i]);
          }

        word18 descAddrs [10];
        word36 descList = fxeMemPtr + next;
        for (int i = 0; i < nargs; i ++)
          {
            descAddrs [i] = next;
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
        for (int i = 0; i < nargs; i ++)
          {
            makeITS (M + argPtrList + 2 * i, FXE_SEGNO, FXE_RING,
                     argAddrs [i], 0, 0);
          }

        next += nargs * 2;

        // List of descriptors

        word36 descPtrList = fxeMemPtr + next;
        for (int i = 0; i < nargs; i ++)
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
        PR [0] . WORDNO = argBlock;

// AK92, pg 2-13: PR4 points to the linkage section for the executing procedure

        PR [4] . SNR = segTable [segIdx] . segno;
        PR [4] . RNR = FXE_RING;
        PR [4] . BITNO = 0;
        PR [4] . WORDNO = segTable [segIdx] . linkage_offset;

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
        PPR . IC = segTable [segIdx] . entry;
        PPR . PRR = FXE_RING;
        PPR . PSR = segTable [segIdx] . segno;
        PPR . P = 0;
        DSBR . STACK = segTable [stack0Idx + FXE_RING] . segno >> 3;
      }
    for (int i = 0; i < maxargs; i ++)
      free (args [i]);

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
#if 0
    if (! c6tValid)
      {
        sim_printf ("ERROR: !c6tValid\n");
        return;
      }

    PPR . P = c6tPPR_P;
    PPR . PRR = c6tPPR_PRR;
    PPR . PSR = c6tPPR_PSR;
    PPR . IC = c6tPPR_IC;
    c6tValid = false;
#endif

#if 0
    // emulate "rtcd    sp|stack_frame.return_ptr"
    word15 spSegno = PR [6] . SNR;
    word18 spWordno = PR [6] . WORDNO;
    int spIdx = segnoMap [spSegno];
    word24 spPhysmem = segTable [spIdx] . physmem + spWordno;
    word24 rpAddr = spPhysmem + 24;
    word36 even = M [rpAddr];
    word36 odd = M [rpAddr + 1];
sim_printf ("rp %08o %012llo %012llo\n", rpAddr, M [rpAddr], M [rpAddr + 1]);  

    // C(Y-pair)3,17 -> C(PPR.PSR)
    PPR . PSR = getbits36 (even, 3, 15);
    word3 targetRing = segTable [segnoMap [PPR . PSR]] . R1;
    word3 targetP = segTable [segnoMap [PPR . PSR]] . P;
    
    // Maximum of C(Y-pair)18,20; C(TPR.TRR); C(SDW.R1) -> C(PPR.PRR)
    PPR . PRR = max3(getbits36 (even, 18, 3), TPR . TRR, targetRing);

    // C(Y-pair)36,53 -> C(PPR.IC)
    PPR . IC = getbits36 (odd, 0, 18);

    // If C(PPR.PRR) = 0 then C(SDW.P) -> C(PPR.P);
    // otherwise 0 -> C(PPR.P)
    if (PPR . PRR == 0)
      PPR . P = targetP;
    else
      PPR . P = 0;

    // C(PPR.PRR) -> C(PRn.RNR) for n = (0, 1, ..., 7)

    PR [0] . RNR =
    PR [1] . RNR =
    PR [2] . RNR =
    PR [3] . RNR =
    PR [4] . RNR =
    PR [5] . RNR =
    PR [6] . RNR =
    PR [7] . RNR = PPR . PRR;
#endif

#if 0
    // Transfer to the return_ptr
    word15 spSegno = PR [6] . SNR;
    word18 spWordno = PR [6] . WORDNO;
    int spIdx = segnoMap [spSegno];
    word24 spPhysmem = segTable [spIdx] . physmem + spWordno;
    word24 rpAddr = spPhysmem + 20;
    word36 even = M [rpAddr];
    word36 odd = M [rpAddr + 1];
    sim_printf ("rp %08o %012llo %012llo\n", rpAddr, M [rpAddr], M [rpAddr + 1]);  
    if (getbits36 (odd, 30, 6) != 020)
      {
        sim_printf ("ERROR: Expected tag 020 (%02llo)\n", 
                    getbits36 (odd, 30, 6));
        return;
      }
    word15 indSegno = getbits36 (even, 3, 15);
    word15 indRing = getbits36 (even, 18, 3);
    word18 indWordno = getbits36 (odd, 0, 18);
    //word24 indPhysmem = segTable [segnoMap [indSegno]] . physmem + indWordno;

    //sim_printf ("ind %08o %012llo %012llo\n", indPhysmem, M [indPhysmem], M [indPhysmem + 1]);  

    PPR . PSR = indSegno;
    PPR . PRR = indRing;
    PPR . IC = indWordno;
    // XXX PPR.P...
#endif

#if 1
    word18 value;
    word15 segno;
    int idx;
    if (! resolveName ("pl1_operators_", "alm_return_no_pop", 
                       & segno, & value, & idx))
      {
        sim_printf ("ERROR: can't find alm_return_no_pop\n");
        exit (1);
        //return;
      }
    PPR . PSR = segno;
    PPR . IC = value; // + 441; // return_main
#endif

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
    word24 segBaseAddr = getbits36 (* even, 0, 24);
    word24 addr = (segBaseAddr + offset) & MASK24;
    //sim_printf ("addr %08o:%012llo\n", addr, M [addr]);

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

    // Find the copied linkage header
    // sim_printf ("header_relp %08o\n", linkCopy . header_relp);
    word24 cpLinkHeaderOffset = (offset + SIGNEXT18 (linkCopy . header_relp)) & MASK24;
    // sim_printf ("headerOffset %08o\n", linkHeaderOffset);
    link_header * cplh = (link_header *) (M + segBaseAddr + cpLinkHeaderOffset);

    // Find the original linkage header

    word15 linkHeaderSegno = GET_ITS_SEGNO (cplh -> original_linkage_ptr);
    word24 linkHeaderOffset = GET_ITS_WORDNO (cplh -> original_linkage_ptr);
    //sim_printf ("orig link @ %05o:%06o\n", linkHeaderSegno, linkHeaderOffset);
    word24 linkSegBaseAddr = segTable [segnoMap [linkHeaderSegno]] . physmem;

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

    word15 refSegno;
    word18 refValue;
    int defIdx;

    switch (typePair -> type)
      {
        case 1:
          //sim_printf ("    1: self-referencing link\n");
          sim_printf ("ERROR: unhandled type %d\n", typePair -> type);
          return;
        case 3:
          //sim_printf ("    3: referencing link\n");
          sim_printf ("ERROT: unhandled type %d\n", typePair -> type);
          return;
        case 4:
          //sim_printf ("    4: referencing link with offset\n");
          //sim_printf ("      seg %s\n", sprintACC (defBase + typePair -> seg_ptr));
          //sim_printf ("      ext %s\n", sprintACC (defBase + typePair -> ext_ptr));
          ;
          char * segStr = strdup (sprintACC (defBase + typePair -> seg_ptr));
          char * extStr = strdup (sprintACC (defBase + typePair -> ext_ptr));
          if (resolveName (segStr, extStr, & refSegno, & refValue, & defIdx))
            {
              sim_printf ("FXE: snap %s:%s\n", segStr, extStr);
              makeITS (M + addr, refSegno, linkCopy . ringno, refValue, 0, 
                       linkCopy . modifier);
              free (segStr);
              free (extStr);
              doRCU (false); // doesn't return
            }
          else
            {
              sim_printf ("ERROR: can't resolve %s:%s\n", segStr, extStr);
            }

          free (segStr);
          free (extStr);
          //int segIdx = loadSegmentFromFile (sprintACC (defBase + typePair -> seg_ptr));
          break;
        case 5:
          //sim_printf ("    5: self-referencing link with offset\n");
          sim_printf ("ERROR: unhandled type %d\n", typePair -> type);
          return;
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
#define DESC_FIXED 1
#define DESC_CHAR_SPLAT 21

static int processArgs (int nargs, int ndescs, argTableEntry * t)
  {
    // Get the argument pointer
    word15 apSegno = PR [0] . SNR;
    word15 apWordno = PR [0] . WORDNO;
    //sim_printf ("ap: %05o:%06o\n", apSegno, apWordno);

    // Find the argument list in memory
    int alIdx = segnoMap [apSegno];
    word24 alPhysmem = segTable [alIdx] . physmem + apWordno;

    // XXX 17s below are not typos.
    word18 arg_count  = getbits36 (M [alPhysmem + 0],  0, 17);
    word18 call_type  = getbits36 (M [alPhysmem + 0], 18, 18);
    word18 desc_count = getbits36 (M [alPhysmem + 1],  0, 17);
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

    if ((int) arg_count != nargs)
      {
        sim_printf ("ERROR: expected %d args, got %d\n",
                     nargs, (int) arg_count);
        return 0;
      }

    if ((int) desc_count != ndescs)
      {
        sim_printf ("ERROR: expected %d2 descs, got %d\n", 
                    ndescs, (int) desc_count);
        return 0;
      }

    uint alOffset = 2;
    uint dlOffset = alOffset + nargs * 2;

    for (int i = 0; i < nargs; i ++)
      {
        t [i] . argAddr = 
          ITSToPhysmem (M + alPhysmem + alOffset + i * 2, NULL);
        if (ndescs)
          {
            t [i] . descAddr = 
              ITSToPhysmem (M + alPhysmem + dlOffset + i * 2, NULL);
            word6 dt = getbits36 (M [t [i] . descAddr], 1, 6);
            if (dt != t [i] . dType)
              {
                sim_printf ("ERROR: expected d%dType %u, got %u\n", 
                            i + 1, t [i] . dType, dt);
                return 0;
              }
            if (dt == DESC_CHAR_SPLAT)
              {
                t [i] . dSize = getbits36 (M [t [i] . descAddr], 12, 24);
              }
          }
        else
         {
            t [i] . descAddr = MASK24;
         }
      }
    return 1;
  }

static void trapPutChars (void)
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
    word24 iocb0 = segTable [segnoMap [IOCB_SEGNO]] . physmem;
    uint iocbIdx = (iocbPtr - iocb0) / sizeof (iocb);
    //sim_printf ("iocbIdx %u\n", iocbIdx);
    if (iocbIdx != 0)
      {
        sim_printf ("ERROR: iocbIdx (%d) != 0\n", iocbIdx);
        return;
      }

    word24 bufPtr = ITSToPhysmem (M + ap2, NULL);
  
    word36 len = M [ap3];

    //sim_printf ("PUT_CHARS:");
    for (int i = 0; i < (int) len; i ++)
      {
        int woff = i / 4;
        int chno = i % 4;
        word36 ch = getbits36 (M [bufPtr + woff], chno * 9, 9);
        sim_printf ("%c", (char) (ch & 0177U));
      }

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
    int alIdx = segnoMap [apSegno];
    word24 alPhysmem = segTable [alIdx] . physmem + apWordno;

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

    word24 iocb0 = segTable [segnoMap [IOCB_SEGNO]] . physmem;

    uint iocbIdx = (iocbPtr - iocb0) / sizeof (iocb);
    sim_printf ("iocbIdx %u\n", iocbIdx);
    if (iocbIdx != 0)
      {
        sim_printf ("ERROR: iocbIdx (%d) != 0\n", iocbIdx);
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

    uint iocbIdx;
    if (isNullPtr (ap1))
      iocbIdx = IOCB_USER_OUTPUT;
    else
      {
        word24 iocbPtr = ITSToPhysmem (M + ap1, NULL);
        //sim_printf ("iocbPtr %08o\n", iocbPtr);

        word24 iocb0 = segTable [segnoMap [IOCB_SEGNO]] . physmem;

        iocbIdx = (iocbPtr - iocb0) / sizeof (iocb);
      }
    //sim_printf ("iocbIdx %u\n", iocbIdx);
    if (iocbIdx != IOCB_USER_OUTPUT)
      {
        sim_printf ("ERROR: iocbIdx (%d) != IOCB_USER_OUTPUT (%d)\n", 
                    iocbIdx, IOCB_USER_OUTPUT);
        return;
      }

    M [ap3] = 80;

    doRCU (true); // doesn't return
  }

static void trapHistoryRegsSet (void)
  {
    //sim_printf ("trapHistoryRegsSet\n");

    doRCU (true); // doesn't return
  }

static void trapHistoryRegsGet (void)
  {
    //sim_printf ("trapHistoryRegsGet\n");

    doRCU (true); // doesn't return
  }

static void trapFSSearchGetWdir (void)
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
      }
    else
      {
        char * cwd = ">udd>fxe>fxe";
        word24 resPtr = ITSToPhysmem (M + ap1, NULL);
        strcpyNonVarying (resPtr, NULL, cwd);
        M [ap2] = strlen (cwd);
      }

    doRCU (true); // doesn't return
  }

static void trimTrailingSpaces (char * str)
  {
    char * end = str + strlen(str) - 1;
    while (end > str && isspace (* end))
      end --;
    * (end + 1) = 0;
  }

static int initiateSegment (char * __attribute__((unused)) dir, char * entry, 
                            word24 * bitcntp, word36 * segptrp, 
                            word15 * segnop)
  {
// XXX dir unused

    int fd = open (entry, O_RDONLY);
    if (fd < 0)
      {
        sim_printf ("ERROR: Unable to open '%s': %d\n", entry, errno);
        return -1;
      }

    /* off_t flen = */ lseek (fd, 0, SEEK_END);
    lseek (fd, 0, SEEK_SET);
    
    int segIdx = allocateSegment ();
    if (segIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate segment for segment initiate\n");
        close (fd);
        return -1;
      }

    segTableEntry * e = segTable + segIdx;

    e -> R1 = FXE_RING;
    e -> R2 = FXE_RING;
    e -> R3 = FXE_RING;
    e -> R = 1;
    e -> E = 0;
    e -> W = 1;
    e -> P = 0;

    makeITS (segptrp, e -> segno, FXE_RING, 0, 0, 0);
    * segnop = e -> segno;

    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word24 maddr = segAddr;
    uint seglen = 0;
    word24 bitcnt = 0;

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
            bitcnt += 9;
          }
        maddr ++;
        seglen ++;
      }
    //sim_printf ("flen %ld seglen %d bitcnt %d %d\n", flen, seglen, bitcnt,
                //(bitcnt + 35) / 36);
    * bitcntp = bitcnt;
    segTable [segIdx] . seglen = seglen;
    segTable [segIdx] . loaded = true;
    sim_printf ("Loaded %u words in segment index %d @ %08o\n", 
                seglen, segIdx, segAddr);

    close (fd);
    return 0;
  }

static void trapInitiateCount (void)
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
    word24 d1size = getbits36 (M [dp1], 12, 24);

    char * arg1 = malloc (d1size + 1);
    strcpyC (ap1, d1size, arg1);

    // Argument 2: entry name
    word24 ap2 = t [ARG2] . argAddr;
    word24 dp2 = t [ARG2] . descAddr;

    word24 d2size = getbits36 (M [dp2], 12, 24);
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
    word24 d3size = getbits36 (M [dp3], 12, 24);

    char * arg3 = NULL;
    if (d3size)
      {
        arg3 = malloc (d3size + 1);
        strcpyC (ap3, d3size, arg3);
      }

    // Argument 4: bit count

    word24 ap4 = t [ARG4] . argAddr;
    word24 dp4 = t [ARG4] . descAddr;

    word24 d4size = getbits36 (M [dp4], 12, 24);
    if (d4size != 24)
      {
        sim_printf ("ERROR: initiate_count expected d4size 24, got %d\n", 
                    d4size);
        return;
      }

    // Argument 6: seg ptr

    word24 ap6 = t [ARG6] . argAddr;
    word24 dp6 = t [ARG6] . descAddr;
    word24 d6size = getbits36 (M [dp6], 12, 24);

    if (d6size != 0)
      {
        sim_printf ("ERROR: initiate_count expected d6size 0, got %d\n", 
                    d6size);
        return;
      }


    // Argument 7: code

    //word24 ap7 = t [ARG7] . argAddr;
    word24 dp7 = t [ARG7] . descAddr;
    word24 d7size = getbits36 (M [dp7], 12, 24);

    if (d7size != 35)
      {
        sim_printf ("ERROR: initiate_count expected d7size 35, got %d\n", d7size);
        return;
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
        code = 1; // XXX need real codes
      }       

    if (status == 0 && arg3)
      addRNTRef (segnoMap [segno], arg3);
    M [ap4] = bitcnt & MASK24;
    M [ap6] = ptr [0];
    M [ap6 + 1] = ptr [1];

    free (arg1);
    free (arg2);
    if (arg3)
      free (arg3);

    doRCU (true); // doesn't return
  }

static void trapTerminateName (void)
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

static void trapMakePtr (void)
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
    word6 d1size = t [ARG1] . dSize;
    if ((! isNullPtr (ap1)) && d1size)
      {
        sim_printf ("WARNING: make_ptr ref name ignored\n");
      }

    // Argument 2: entry name
    word24 ap2 = t [ARG2] . argAddr;
    word6 d2size = t [ARG2] . dSize;

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

    // Argument 4: entry_point_ptr
    word24 ap5 = t [ARG5] . argAddr;

    word15 segno;
    word18 value;
    word24 code = 0;
    int idx;
    word36 ptr [2];

    int rc = resolveName (arg2, arg3, & segno, & value, & idx);
    if (! rc)
      {
        sim_printf ("WARNING: make_ptr resolve fail %s|%s\n", arg2, arg3);
        code = 1; // XXX need real code
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

static void trapStatusMins (void)
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
        code = 1; // Need a real code XXX
        goto done;
      }
    int idx = segnoMap [segno];
    if (idx < 0) // unassigned segno
      {
        sim_printf ("ERROR: unassigned %05o\n", segno);
        code = 1; // Need a real code XXX
        goto done;
      }

    // Argument 2: type
    word24 ap2 = t [ARG2] . argAddr;
    M [ap2] = 1; // XXX need real types

    // Argument 3: bit_count
    word24 ap3 = t [ARG3] . argAddr;
    M [ap3] = segTable [idx] . seglen * 36u;
done:;

    word24 ap4 = t [ARG4] . argAddr;
    M [ap4] = code;
    doRCU (true); // doesn't return
  }

static void trapReturnToFXE (void)
  {
    longjmp (jmpMain, JMP_STOP);
  }

static void fxeTrap (void)
  {
    // Application has made an call or return into a routine that FXE wants to handle
    // on the host.

    // Get the offending address from the SCU data

    // Test FIF bit
    word18 offset;
    word1 FIF = getbits36 (M [0200 + 5], 29, 1);
    if (FIF != 0)
      offset = getbits36 (M [0200 + 4], 0, 18); // PPR . IC
    else
      offset = getbits36 (M [0200 + 5], 0, 18); // PPR . CA

    // XXX make this into trapNamesTable callbacks
    switch (offset)
      {
        case TRAP_PUT_CHARS:
          trapPutChars ();
          break;
#if 0
        case TRAP_MODES:
          trapModes ();
          break;
#endif
        case TRAP_GET_LINE_LENGTH_SWITCH:
          trapGetLineLengthSwitch ();
          break;
        case TRAP_HISTORY_REGS_SET:
          trapHistoryRegsSet ();
          break;
        case TRAP_HISTORY_REGS_GET:
          trapHistoryRegsGet ();
          break;
        case TRAP_FS_SEARCH_GET_WDIR:
          trapFSSearchGetWdir ();
          break;
        case TRAP_INITIATE_COUNT:
          trapInitiateCount ();
          break;
        case TRAP_TERMINATE_NAME:
          trapTerminateName ();
          break;
        case TRAP_MAKE_PTR:
          trapMakePtr ();
          break;
        case TRAP_STATUS_MINS:
          trapStatusMins ();
          break;
        case TRAP_RETURN_TO_FXE:
          trapReturnToFXE ();
          break;
        default:
          sim_printf ("ERROR: unknown trap code %d\n", offset);
          break;
      }
  }

static void faultACVHandler (void)
  {
    // The fault pair saved the SCU data @ 0200

    // Get the offending address from the SCU data

    //word18 offset = GETHI (M [0200 + 5]);
    word15 segno = GETHI (M [0200 + 2]) & MASK15;
    //sim_printf ("acv fault %05o:%06o\n", segno, offset);

    if (segno == TRAP_SEGNO)
      {
        fxeTrap ();
      }

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
    int defIdx;

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
          //int segIdx = loadSegmentFromFile (sprintACC (defBase + typePair -> seg_ptr));
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
    for (uint i = 0; i < N_SEGS; i ++)
      {
        if (segTable [i] . allocated && segTable [i] . segno == segno)
          {
            if (compname)
                * compname = segTable [i] . segname;
            if (compoffset)
                * compoffset = 0;  
            sprintf (buf, "%s:+0%0o", segTable [i] . segname, offset);
            return buf;
          }
      }
    return NULL;
  }

