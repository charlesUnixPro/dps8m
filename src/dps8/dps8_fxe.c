// From multicians: The maximum size of user ring stacks is initially set to 48K.SEGSIZE;
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "dps8.h"
#include "dps8_append.h"
#include "dps8_cpu.h"
#include "dps8_sys.h"
#include "dps8_utils.h"
#include "dps8_ins.h"

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

static void makeNullPtr (word36 * memory)
  {
    makeITS (memory, 077777, 0, 0777777, 0, 0);

    // The example null ptr I found was in http://www.multicians.org/pxss.html;
    // it has a tag of 0 instead of 43;
    putbits36 (memory + 0, 30,  6, 0);
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
// the boot tape. Rather than parsing the boot tape or system boook,
// just copy the needed values from the system book.
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
    sim_printf ("ERROR: %s not in SLTE\n", name);
    return -1;
  }

// getSegnoFromSLTE - get the segment number of a segment from the SLTE

static int getSegnoFromSLTE (char * name)
  {
    int idx = getSLTEidx (name);
    if (idx < 0)
      return -1;
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

#define DSEG_SEGNO 0
#define IOCB_SEGNO 0265
#define CLT_SEGNO 0266
#define FXE_SEGNO 0267
#define STACKS_SEGNO 0270
#define USER_SEGNO 0600
#define TRAP_SEGNO 077776

// Traps

#define TRAP_PUT_CHARS 1


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
    word18 isot_offset;
    word36 * segnoPadAddr;

  } segTableEntry;

static segTableEntry segTable [N_SEGS];


//
// CLT - Multics segment containing the LOT and ISOT
//

// Remember which segment has the CLT

static int cltIdx;

#define LOT_SIZE 0100000 // 2^12
#define LOT_OFFSET 0 // LOT starts at word 0
#define ISOT_OFFSET LOT_SIZE // ISOT follows LOT

#define CLT_SIZE (LOT_SIZE * 2) // LOT + ISOT

// installLOT - install a segment LOT and ISOT in the SLT

static void installLOT (int idx)
  {
    word36 * cltMemory = M + segTable [cltIdx] . physmem;
    cltMemory [LOT_OFFSET + segTable [idx] . segno] = 
      packedPtr (0, segTable [idx] . segno,
                 segTable [idx] . linkage_offset);
    cltMemory [ISOT_OFFSET + segTable [idx] . segno] = 
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


static int allocateSegment (void)
  {
    static word24 nextPhysmem = 1; // First block is used by dseg;
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

static int allocateSegno (void)
  {
    static int nextSegno = USER_SEGNO;
    return nextSegno ++;
  }

//static loadSegment (char * segname, word15 segno);
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

    word36 filled_in_later [4];

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

static int resolveName (char * segName, char * symbolName, word15 * segno,
                        word18 * value, int * index)
  {
    //sim_printf ("resolveName %s:%s\n", segName, symbolName);
    int idx;
    for (idx = 0; idx < (int) N_SEGS; idx ++)
      {
        segTableEntry * e = segTable + idx;
        if (! e -> allocated)
          continue;
        if (! e -> loaded)
          continue;
        if (e -> definition_offset == 0)
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
        segTable [idx] . segno = getSegnoFromSLTE (segName);
        if (! segTable [idx] . segno)
          {
            segTable [idx] . segno = allocateSegno ();
            sim_printf ("assigning %d to %s\n", segTable [idx] . segno, segName);
          }
        segTable [idx] . segname = strdup (segName);
        segTable [idx] . R1 = 0;
        segTable [idx] . R2 = 5;
        segTable [idx] . R3 = 5;
        segTable [idx] . R = 1;
        segTable [idx] . E = 1;
        segTable [idx] . W = 0;
        segTable [idx] . P = 0;

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
        sim_printf ("ERROR: entry point not found\n");
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
          sim_printf ("Warning:  header_relp wrong %06o (%06o)\n",
                      l -> header_relp,
                      (word18) (- (((word36 *) l) - linkBase)) & 0777777);
        if (l -> run_depth != 0)
          sim_printf ("Warning:  run_depth wrong %06o\n", l -> run_depth);

        //sim_printf ("ringno %0o\n", l -> ringno);
        //sim_printf ("run_depth %02o\n", l -> run_depth);
        //sim_printf ("expression_relp %06o\n", l -> expression_relp);
       
        if (l -> expression_relp >= oip_dlng)
          sim_printf ("Warning:  expression_relp too big %06o\n", l -> expression_relp);

        expression * expr = (expression *) (defBase + l -> expression_relp);

        //sim_printf ("  type_ptr %06o  exp %6o\n", 
                    //expr -> type_ptr, expr -> exp);

        if (expr -> type_ptr >= oip_dlng)
          sim_printf ("Warning:  type_ptr too big %06o\n", expr -> type_ptr);
        type_pair * typePair = (type_pair *) (defBase + expr -> type_ptr);

        switch (typePair -> type)
          {
            case 1:
              sim_printf ("    1: self-referencing link\n");
              sim_printf ("Warning: unhandled type %d\n", typePair -> type);
              break;
            case 3:
              sim_printf ("    3: referencing link\n");
              sim_printf ("Warning: unhandled type %d\n", typePair -> type);
              break;
            case 4:
              sim_printf ("    4: referencing link with offset\n");
              sim_printf ("      seg %s\n", sprintACC (defBase + typePair -> seg_ptr));
              sim_printf ("      ext %s\n", sprintACC (defBase + typePair -> ext_ptr));
              //int dsegIdx = loadDeferred (sprintACC (defBase + typePair -> seg_ptr));
              break;
            case 5:
              sim_printf ("    5: self-referencing link with offset\n");
              sim_printf ("Warning: unhandled type %d\n", typePair -> type);
              break;
            default:
              sim_printf ("Warning: unknown type %d\n", typePair -> type);
              break;
          }


        //sim_printf ("    type %06o\n", typePair -> type);
        //sim_printf ("    trap_ptr %06o\n", typePair -> trap_ptr);
        //sim_printf ("    seg_ptr %06o\n", typePair -> seg_ptr);
        //sim_printf ("    ext_ptr %06o\n", typePair -> ext_ptr);

        if (typePair -> trap_ptr >= oip_dlng)
          sim_printf ("Warning:  trap_ptr too big %06o\n", typePair -> trap_ptr);
        if (typePair -> ext_ptr >= oip_dlng)
          sim_printf ("Warning:  ext_ptr too big %06o\n", typePair -> ext_ptr);

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
    e -> R1 = 5;
    e -> R2 = 5;
    e -> R3 = 5;
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

    //e -> segno = allocateSegno ();
    e -> R1 = 5;
    e -> R2 = 5;
    e -> R3 = 5;
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
     segTable [0] . segno = 0;
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
        e -> segno = STACKS_SEGNO + i;
        e -> R1 = 5;
        e -> R2 = 5;
        e -> R3 = 5;
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
    makeITS (M + hdrAddr + 18, stkSegno, 5, STK_TOP, 0, 0);

    // word 20, 21    stack_end_ptr
    makeITS (M + hdrAddr + 20, stkSegno, 5, STK_TOP, 0, 0);

    // word 22, 23    lot_ptr
    makeITS (M + hdrAddr + 22, CLT_SEGNO, 5, LOT_OFFSET, 0, 0); 

    // word 24, 25    signal_ptr

    // word 26, 27    bar_mode_sp_ptr

    // word 28, 29    pl1_operators_table
    word18 operator_table = 0777777;
    if (! lookupDef (libIdx, "pl1_operators_", "operator_table",
                     & operator_table))
     {
       sim_printf ("ERROR: Can't find pl1_operators_$operator_table\n");
     }
    makeITS (M + hdrAddr + 28, libSegno, 5, operator_table, 0, 0);

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
    makeITS (M + hdrAddr + 30, libSegno, 5, GETHI (callTraInst), 0, 0);

    // word 32, 33    push_op_ptr
    word18 pushOffset = operator_table + push_offset;
    word24 pushTraInstAddr = libAddr + pushOffset;
    word36 pushTraInst = M [pushTraInstAddr];
    makeITS (M + hdrAddr + 32, libSegno, 5, GETHI (pushTraInst), 0, 0);

    // word 34, 35    return_op_ptr
    word18 returnOffset = operator_table + return_offset;
    word24 returnTraInstAddr = libAddr + returnOffset;
    word36 returnTraInst = M [returnTraInstAddr];
    makeITS (M + hdrAddr + 34, libSegno, 5, GETHI (returnTraInst), 0, 0);

    // word 36, 37    short_return_op_ptr
    word18 returnNoPopOffset = operator_table + return_no_pop_offset;
    word24 returnNoPopTraInstAddr = libAddr + returnNoPopOffset;
    word36 returnNoPopTraInst = M [returnNoPopTraInstAddr];
    makeITS (M + hdrAddr + 36, libSegno, 5, GETHI (returnNoPopTraInst), 0, 0);

    // word 38, 39    entry_op_ptr
    word18 entryOffset = operator_table + entry_offset;
    word24 entryTraInstAddr = libAddr + entryOffset;
    word36 entryTraInst = M [entryTraInstAddr];
    makeITS (M + hdrAddr + 38, libSegno, 5, GETHI (entryTraInst), 0, 0);

    // word 40, 41    trans_op_tv_ptr

    // word 42, 43    isot_ptr
    makeITS (M + hdrAddr + 42, CLT_SEGNO, 5, ISOT_OFFSET, 0, 0); 

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

static void createFrame (int ssIdx)
  {
    word15 stkSegno = segTable [ssIdx] . segno;
    word24 segAddr = lookupSegAddrByIdx (ssIdx);
    word24 frameAddr = segAddr + STK_TOP;
    
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
    makeNullPtr (M + frameAddr + 16);

    // word 18, 19    next_stack_frame_ptr
    makeITS (M + frameAddr + 18, stkSegno, 5, STK_TOP + FRAME_SZ, 0, 0);

    // word 20, 21    return_ptr
    makeNullPtr (M + frameAddr + 20);
    
    // word 22, 23    entry_ptr
    makeNullPtr (M + frameAddr + 22);

    // word 24, 25    operator_link_ptr
    makeNullPtr (M + frameAddr + 23);

    // word 25, 26    operator_link_ptr
    makeNullPtr (M + frameAddr + 23);

    // Update the header

    // word 18, 19    stack_begin_ptr
    word24 hdrAddr = segAddr + HDR_OFFSET;
    makeITS (M + hdrAddr + 18, stkSegno, 5, STK_TOP, 0, 0);

    // word 20, 21    stack_end_ptr
    makeITS (M + hdrAddr + 20, stkSegno, 5, STK_TOP + FRAME_SZ, 0, 0);

  }

static int installLibrary (char * name)
  {
    int idx = loadSegmentFromFile (name);
    segTable [idx] . segno = getSegnoFromSLTE (name);
    if (! segTable [idx] . segno)
      {
        segTable [idx] . segno = allocateSegno ();
        sim_printf ("assigning %d to %s\n", segTable [idx] . segno, name);
      }
    //sim_printf ("lib %s segno %o\n", name, segTable [idx] . segno);
    segTable [idx] . segname = strdup (name);
    segTable [idx] . R1 = 5;
    segTable [idx] . R2 = 5;
    segTable [idx] . R3 = 5;
    segTable [idx] . R = 1;
    segTable [idx] . E = 1;
    segTable [idx] . W = 0;
    segTable [idx] . P = 0;

    installLOT (idx);
    installSDW (idx);
    return idx;
  }


//
// fxe - load a segment into memory and execute it
//

t_stat fxe (int32 __attribute__((unused)) arg, char * buf)
  {
    sim_printf ("FXE initializing...\n");
    memset (segTable, 0, sizeof (segTable));

    setupWiredSegments ();

// Setup CLT

    cltIdx = allocateSegment ();
    if (cltIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate clt segment\n");
        return -1;
      }

    segTableEntry * cltEntry = segTable + cltIdx;

    cltEntry -> R1 = 5;
    cltEntry -> R2 = 5;
    cltEntry -> R3 = 5;
    cltEntry -> R = 1;
    cltEntry -> E = 0;
    cltEntry -> W = 1;
    cltEntry -> P = 0;
    cltEntry -> segname = strdup ("clt");
    cltEntry -> segno = CLT_SEGNO;
    cltEntry -> seglen = 2 * LOT_SIZE;
    cltEntry -> loaded = true;
    installSDW (cltIdx);

    word36 * cltMemory = M + cltEntry -> physmem;

    for (uint i = 0; i < LOT_SIZE; i ++)
      cltMemory [i] = 0007777000000; // bitno 0, seg 7777, word 0


// Setup IOCB

    int iocbIdx = allocateSegment ();
    if (iocbIdx < 0)
      {
        sim_printf ("ERROR: Unable to allocate iocb segment\n");
        return -1;
      }

    segTableEntry * iocbEntry = segTable + iocbIdx;

    iocbEntry -> R1 = 5;
    iocbEntry -> R2 = 5;
    iocbEntry -> R3 = 5;
    iocbEntry -> R = 1;
    iocbEntry -> E = 0;
    iocbEntry -> W = 1;
    iocbEntry -> P = 0;
    iocbEntry -> segname = strdup ("iocb");
    iocbEntry -> segno = IOCB_SEGNO;
    iocbEntry -> seglen = 0777777;
    iocbEntry -> loaded = true;
    installSDW (iocbIdx);

#define IOCB_USER_OUTPUT 0

    // Define iocb [IOCB_USER_OUTPUT] . put_chars

    iocb * iocbUser = (iocb *) (M + iocbEntry -> physmem + IOCB_USER_OUTPUT * sizeof (iocb));

    word36 * putCharEntry = (word36 *) & iocbUser -> put_chars;

    makeITS (putCharEntry, TRAP_SEGNO, 5, TRAP_PUT_CHARS, 0, 0);
    makeNullPtr (putCharEntry + 2);

    sim_printf ("Loading library segment\n");

    libIdx = installLibrary ("bound_library_wired_");

    installLibrary ("bound_library_1_");
    installLibrary ("bound_process_env_");
    //int lib2Idx = installLibrary ("bound_bce_wired");
    int infoIdx = installLibrary ("sys_info");

    // Set the flag that applications check to see if Multics is up
    word18 value;
    word15 segno;
    int defIdx;

    if (resolveName ("sys_info", "service_system", & segno, & value, & defIdx))
      {
        M [segTable [defIdx] . physmem + value] = 1;
      }
    else
      {
        sim_printf ("ERROR: can't find sys_info:service_system\n");
      }

    // Set the iox_$user_output

    if (resolveName ("iox_", "user_output", & segno, & value, & defIdx))
      {
        //M [segTable [infoIdx] . physmem + value] = 1;
      }
    else
      {
        sim_printf ("ERROR: can't find iox_:user_output\n");
      }
sim_printf ("iox_:user_output %05o:%06o\n", segno, value);
    makeITS (M + segTable [defIdx] . physmem + value, IOCB_SEGNO, 5,
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
    fxeEntry -> R1 = 5;
    fxeEntry -> R2 = 5;
    fxeEntry -> R3 = 5;
    fxeEntry -> R = 1;
    fxeEntry -> E = 1;
    fxeEntry -> W = 1;
    fxeEntry -> P = 0;
    fxeEntry -> segname = strdup ("fxe");
    fxeEntry -> segno = FXE_SEGNO;
    fxeEntry -> loaded = true;
    installSDW (fxeIdx);


    FXEinitialized = true;

#define maxargs 10
    char * args [maxargs];
    for (int i = 0; i < maxargs; i ++)
      args [i] = malloc (strlen (buf) + 1);
    
    int n = sscanf (buf, "%s%s%s%s%s%s%s%s%s%s", 
                    args [0], args [1], args [2], args [3], args [4],
                    args [5], args [6], args [7], args [8], args [9]);
    if (n >= 1)
      {
        sim_printf ("Loading segment %s\n", args [0]);
        int segIdx = loadSegmentFromFile (args [0]);
        segTable [segIdx] . segname = strdup (args [0]);
        segTable [segIdx] . segno = allocateSegno ();
        segTable [segIdx] . R1 = 0;
        segTable [segIdx] . R2 = 5;
        segTable [segIdx] . R3 = 5;
        segTable [segIdx] . R = 1;
        segTable [segIdx] . E = 1;
        segTable [segIdx] . W = 0;
        segTable [segIdx] . P = 0;
        segTable [segIdx] . segname = strdup (args [0]);

        installSDW (segIdx);
        installLOT (segIdx);
        // sim_printf ("executed segment idx %d, segno %o, phyaddr %08o\n", 
        // segIdx, segTable [segIdx] . segno, lookupSegAddrByIdx (segIdx));
        createStackSegments ();
        // sim_printf ("stack segment idx %d, segno %o, phyaddr %08o\n", 
        // stack0Idx + 5, segTable [stack0Idx + 5] . segno, lookupSegAddrByIdx (stack0Idx + 5));
        // sim_printf ("lib segment idx %d, segno %o, phyaddr %08o\n", 
        // libIdx, segTable [libIdx] . segno, lookupSegAddrByIdx (libIdx));

        initStack (stack0Idx + 5);
        createFrame (stack0Idx + 5);


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

// AK92, pg 2-13: PR0 points to the argument list

        // Create an empty argument list for now.

        word36 fxeMemPtr = fxeEntry -> physmem;

        word36 argList = fxeMemPtr - fxeEntry -> physmem; // wordno

        // word 0  arg_count, call_type
        M [fxeMemPtr ++] = 0000000000004; // 0 args, inter-segment call

        // word 1  desc_count, 0
        M [fxeMemPtr ++] = 0000000000000; // 0 descs

 
        PR [0] . SNR = FXE_SEGNO;
        PR [0] . RNR = 5;
        PR [0] . BITNO = 0;
        PR [0] . WORDNO = argList;

// AK92, pg 2-13: PR4 points to the linkage section for the executing procedure

        PR [4] . SNR = segTable [segIdx] . segno;
        PR [4] . RNR = 5;
        PR [4] . BITNO = 0;
        PR [4] . WORDNO = segTable [segIdx] . linkage_offset;

// AK92, pg 2-13: PR7 points to the stack frame

        PR [6] . SNR = 077777;
        PR [6] . RNR = 5;
        PR [6] . BITNO = 0;
        PR [6] . WORDNO = 0777777;

// AK92, pg 2-10: PR7 points to the base of the stack segement

        PR [7] . SNR = STACKS_SEGNO + 5;
        PR [7] . RNR = 5;
        PR [7] . BITNO = 0;
        PR [7] . WORDNO = 0;


        set_addr_mode (APPEND_mode);
        PPR . IC = segTable [segIdx] . entry;
        PPR . PRR = 5;
        PPR . PSR = segTable [segIdx] . segno;
        PPR . P = 0;
        DSBR . STACK = segTable [stack0Idx + 5] . segno >> 3;
      }
    for (int i = 0; i < maxargs; i ++)
      free (args [i]);

    return SCPE_OK;
  }

//
// Fault handler
//

static void faultTag2Handler (void)
  {
    // The fault pair saved the SCU data @ 0200

    // Get the offending address from the SCU data

    word18 offset = GETHI (M [0200 + 5]);
    word15 segno = GETHI (M [0200 + 2]) & MASK15;
    sim_printf ("f2 fault %05o:%06o\n", segno, offset);

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
      sim_printf ("Warning:  run_depth wrong %06o\n", linkCopy . run_depth);

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
          sim_printf ("Warning: unhandled type %d\n", typePair -> type);
          break;
        case 3:
          sim_printf ("    3: referencing link\n");
          sim_printf ("Warning: unhandled type %d\n", typePair -> type);
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
              doRCU ();
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
          sim_printf ("Warning: unhandled type %d\n", typePair -> type);
          break;
        default:
          sim_printf ("Warning: unknown type %d\n", typePair -> type);
          break;
      }


    //sim_printf ("    type %06o\n", typePair -> type);
    //sim_printf ("    trap_ptr %06o\n", typePair -> trap_ptr);
    //sim_printf ("    seg_ptr %06o\n", typePair -> seg_ptr);
    //sim_printf ("    ext_ptr %06o\n", typePair -> ext_ptr);
  }

void fxeFaultHandler (void)
  {
    switch (cpu . faultNumber)
      {
        case 24: // fault tag 2
          faultTag2Handler ();
          break;
        default:
          sim_printf ("fxeFaultHandler: fail: %d\n", cpu . faultNumber);
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

