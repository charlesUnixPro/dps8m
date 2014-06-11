#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "dps8.h"
#include "dps8_cpu.h"
#include "dps8_sys.h"
#include "dps8_utils.h"

// keeping it simple: segments are loaded unpaged, and a full 128K space is
// allocated for them.

// SEGSIZE is 2^18
// MEMSIZE is the emulator's allocated memory size

// How many segments can we deal with?

// This is 64...
#define N_SEGS ((MEMSIZE + SEGSIZE - 1) / SEGSIZE)
#define STK_SEG 8

typedef struct
  {
    bool allocated;
    word15 segno;  // Multics segno
    bool wired;
    word3 R1, R2, R3;
    word1 R, E, W, P;
    bool gated;
    word18 entry_bound;
    char * segname;
    word18 seglen;
  } segTableEntry;

static segTableEntry segTable [N_SEGS];

static word24 lookupSegAddrByIdx (int segIdx)
  {
    return segIdx * SEGSIZE;
  }

static int allocateSegment (void)
  {
    for (int i = 0; i < (int) N_SEGS; i ++)
      {
        if (! segTable [i] . allocated)
          {
            segTable [i] . allocated = true;
            return i;
          }
      }
    return -1;
  }

//static loadSegment (char * segname, word15 segno);
static void initializeDSEG (void)
  {

    // 0100 (64) - 0177 (127) Fault pairs
    // 0200 (128) - 0207 (135) SCU yblock
    // 0300 - 0477 descriptor segment: 64 segments at 2 words per segment.
#define DESCSEG 0300

    //    org   0100 " Fault pairs
    //    bss   64
    //    org   0200
    //    bss   8
    //    org   0300
    //    bss   64*2


    // Fill the fault pairs with fxeFaultHandler traps.

    // (12-bits of which the top-most 7-bits are used)
    int fltAddress = (switches . FLT_BASE << 5) & 07740;

    for (int i = 0; i < N_FAULTS; i ++)
      {
        M [fltAddress + i * 2 + 0] = 0000200657000;  // 'SCU 200' instruction.
        M [fltAddress + i * 2 + 1] = 0000000425400;  // FXE instruction.
      }

    // Fill the descriptor segment with SDWs
    int descAddress = DESCSEG;
    for (int i = 0; i < (int) N_SEGS; i ++)
      {
        word24 segAddr = lookupSegAddrByIdx (i);

        // even
        //   ADDR: memory address for segment
        //   R1, R2, R3: 0
        //   F: 0 - page is non-resident
        //   FC: 0 - fault code
        word36 even = ((word36) segAddr) << 8;
        M [descAddress + i * 2 + 0] = even;

        // odd
        //  BOUND: 0
        //  R,E,W,P: 0
        //  U: 1
        //  G,C: 0
        //  EB: 0
        M [descAddress + i * 2 + 0] = 0000000400000;
      }
    DSBR . ADDR = DESCSEG;
    DSBR . BND = N_SEGS / 8;
    DSBR . U = 1;
    DSBR . STACK = STK_SEG >> 3;
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
struct object_map
  {
//++    2 decl_vers fixed bin,  /* Version number of current structure format */
    word36 decl_vers; /* Version number of current structure format */

//++    2 identifier char (8) aligned, /* Must be the constant "obj_map" */
    word36 identifier [2]; /* Must be the constant "obj_map" */

//++    2 text_offset bit (18) unaligned,     /* Offset relative to base of object segment of base of text section */
//++    2 text_length bit (18) unaligned,     /* Length in words of text section */
    union
      {
        struct
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
        struct
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
        struct
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
        struct
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
        struct
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
        struct
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
        struct
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
        struct
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
  };

//++ 
//++ declare   map_ptr bit(18) aligned based;          /* Last word of the segment. It points to the base of the object map. */
//++ 
//++ declare   object_map_version_2 fixed bin static init(2);

#define object_map_version_2 2

//++ 
//++ /* END INCLUDE FILE ... object_map.incl.pl1 */

static void parseSegment (int segIdx)
  {
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    segTableEntry * e = segTable + segIdx;
    word24 seglen = e -> seglen;

    word36 * segp = M + segAddr;

    if (seglen == 0)
      {
        sim_printf ("Can't parse empty segment\n");
        return;
      }
    word36 lastword = segp [seglen - 1];
    word24 i = GETHI (lastword);
    if (i >= seglen)
      {
        sim_printf ("mapPtr too big %06u >= %06u\n", i, seglen);
        return;
      }
    if (seglen - i - 1 < 11)
      {
        sim_printf ("mapPtr too small %06u\n", seglen - i - 1);
        return;
      }

    struct object_map * mapp = (struct object_map *) (segp + i);

    if (mapp -> identifier [0] != 0157142152137LLU || // "obj_"
        mapp -> identifier [1] != 0155141160040LLU) // "map "
      {
        sim_printf ("mapID wrong %012llo %012llo\n", 
                    mapp -> identifier [0], mapp -> identifier [1]);
        return;
      }

    if (mapp -> decl_vers != 2)
      {
        sim_printf ("Can't hack object map version %llu\n", mapp -> decl_vers);
        return;
      }

//    word36 * oip_textp = segp + mapp -> text_offset;
//    word36 * oip_defp = segp + mapp -> definition_offset;
//    word36 * oip_linkp = segp + mapp -> linkage_offset;
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
    word1 oip_format_bound = mapp -> format . bound;
    if (oip_format_bound)
      sim_printf ("Segment is bound.\n");
    else
      sim_printf ("Segment is unbound.\n");
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
        sim_printf ("Segment is gated; entry bound %d.\n", entry_bound);
        e -> gated = true;
        e -> entry_bound = entry_bound;
      }
    else
      {
        sim_printf ("Segment is ungated.\n");
        e -> gated = false;
        e -> entry_bound = 0;
      }
  }

static void readSegment (int fd, int segIdx)
  {
    word24 segAddr = lookupSegAddrByIdx (segIdx);
    word24 maddr = segAddr;
    uint seglen = 0;

    // 72 bits at a time; 2 dps8m words == 9 bytes
    uint8 bytes [9];
    while (read (fd, bytes, 9))
      {
        if (seglen > MAX18)
          {
            sim_printf ("File too long\n");
            return;
          }
        M [maddr ++] = extr36 (bytes, 0);
        M [maddr ++] = extr36 (bytes, 1);
        seglen += 2;
      }
    segTable [segIdx] . seglen = seglen;
    sim_printf ("Loaded %u words in segment index %d @ %08o\n", 
                seglen, segIdx, segAddr);
    parseSegment (segIdx);
  }

static int loadSegmentFromFile (char * arg)
  {
    int fd = open (arg, O_RDONLY);
    if (fd < 0)
      {
        sim_printf ("Unable to open '%s': %d\n", arg, errno);
        return -1;
      }
    int segIdx = allocateSegment ();
    if (segIdx < 0)
      {
        sim_printf ("Unable to open '%s': %d\n", arg, errno);
        return -1;
      }

    segTableEntry * e = segTable + segIdx;

    // e -> segno = ???? XXX
    e -> R1 = 5;
    e -> R2 = 5;
    e -> R3 = 5;
    e -> R = 1;
    e -> E = 1;
    e -> W = 1;
    e -> P = 0;

    readSegment (fd, segIdx);

    return segIdx;
  }

static void setupWiredSegments (void)
  {
     // allocate wired segments

     // 'dseg' contains the fault traps

     segTable [0] . allocated = true;
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
typedef struct
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
        struct
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
        struct
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


static int createStackSegment (void)
  {
    int segIdx = STK_SEG + 5; // ring 5
    segTableEntry * e = segTable + segIdx;
    e -> segname = strdup ("stack_5");

    // e -> segno = ???? XXX
    e -> R1 = 5;
    e -> R2 = 5;
    e -> R3 = 5;
    e -> R = 1;
    e -> E = 0;
    e -> W = 1;
    e -> P = 0;

    word24 segAddr = lookupSegAddrByIdx (segIdx);
    memset (M + segAddr, 0, sizeof (stack_header));
    return segIdx;
  }

static void installSDW (int segIdx)
  {
     segTableEntry * e = segTable + segIdx;

     word36 * even = M + DESCSEG + 2 * segIdx + 0;  
     word36 * odd  = M + DESCSEG + 2 * segIdx + 1;  

     putbits36 (even, 24,  3, e -> R1);
     putbits36 (even, 27,  3, e -> R2);
     putbits36 (even, 30,  3, e -> R3);

     putbits36 (even, 33,  1, 1); // F: mark page as resident

     putbits36 (odd,   1, 14, e -> seglen >> 4); // BOUND
     putbits36 (odd,  15,  1, e -> R);
     putbits36 (odd,  16,  1, e -> E);
     putbits36 (odd,  17,  1, e -> W);
     putbits36 (odd,  18,  1, e -> P);
     putbits36 (odd,  20,  1, e -> gated ? 1U : 0U);
     putbits36 (odd,  22, 14, e -> entry_bound >> 4);
  }

t_stat fxe (int32 __attribute__((unused)) arg, char * buf)
  {
     sim_printf ("FXE initializing...\n");
     sim_printf ("(%d segments)\n", N_SEGS);

     memset (segTable, 0, sizeof (segTable));

     // The stack segments must be allocated as an aligned set of 8.
     for (int i = 0; i < 8; i ++)
       segTable [STK_SEG + i] . allocated = true;

     setupWiredSegments ();

     char * fname = malloc (strlen (buf) + 1);
     int n = sscanf (buf, "%s", fname);
     if (n == 1)
       {
         sim_printf ("Loading segment %s\n", fname);
         int segIdx = loadSegmentFromFile (fname);
         installSDW (segIdx);
         int ssIdx = createStackSegment ();
         installSDW (ssIdx);



         set_addr_mode (APPEND_mode);
         PPR . IC = 0;
         PPR . PRR = 5;
         PPR . PSR = segIdx;
         PPR . P = 0;
       }
     free (fname);

     return SCPE_OK;
  }

void fxeFaultHandler (void)
  {
    sim_printf ("fxeFaultHandler\n");
  }

