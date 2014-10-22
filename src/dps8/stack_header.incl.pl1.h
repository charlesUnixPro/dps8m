//++ 
//++ /*         BEGIN INCLUDE FILE ... stack_header.incl.pl1 .. 3/72 Bill Silver  */
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
//++      dcl      sb                 ptr;              /* the  main pointer to the stack header */
//++ 
//++      dcl      1 stack_header             based (sb) aligned,
//++         2 pad1           (4) fixed bin,    /*  (0) also used as arg list by outward_call_handler  */
//++         2 cpm_data_ptr           ptr,              /*  (4)  pointer to control point which owns this stack */
//++         2 combined_stat_ptr  ptr,          /*  (6)  pointer to area containing separate static */
//++         2 clr_ptr        ptr,              /*  (8)  pointer to area containing linkage sections */
//++         2 max_lot_size           fixed bin (17) unal,      /*  (10) DU  number of words allowed in lot */
//++         2 main_proc_invoked  fixed bin (11) unal,  /*  (10) DL  nonzero if main procedure invoked in run unit */
//++         2 have_static_vlas   bit (1) unal,         /*  (10) DL  "1"b if (very) large arrays are being used in static */
//++         2 pad4           bit (2) unal,
//++         2 run_unit_depth     fixed bin (2) unal,   /*  (10) DL  number of active run units stacked */
//++         2 cur_lot_size           fixed bin (17) unal,      /*  (11) DU  number of words (entries) in lot */
//++         2 cpm_enabled            bit (18) unal,    /*  (11) DL  non-zero if control point management is enabled */
//++         2 system_free_ptr    ptr,          /*  (12)  pointer to system storage area */
//++         2 user_free_ptr      ptr,          /*  (14)  pointer to user storage area */
//++         2 null_ptr       ptr,              /*  (16)  */
//++         2 stack_begin_ptr    ptr,          /*  (18)  pointer to first stack frame on the stack */
//++         2 stack_end_ptr      ptr,          /*  (20)  pointer to next useable stack frame */
//++         2 lot_ptr        ptr,              /*  (22)  pointer to the lot for the current ring */
//++         2 signal_ptr             ptr,              /*  (24)  pointer to signal procedure for current ring */
//++         2 bar_mode_sp            ptr,              /*  (26)  value of sp before entering bar mode */
//++         2 pl1_operators_ptr  ptr,          /*  (28)  pointer to pl1_operators_$operator_table */
//++         2 call_op_ptr            ptr,              /*  (30)  pointer to standard call operator */
//++         2 push_op_ptr            ptr,              /*  (32)  pointer to standard push operator */
//++         2 return_op_ptr      ptr,          /*  (34)  pointer to standard return operator */
//++         2 return_no_pop_op_ptr
//++                          ptr,              /*  (36)  pointer to standard return / no pop operator */
//++         2 entry_op_ptr           ptr,              /*  (38)  pointer to standard entry operator */
//++         2 trans_op_tv_ptr    ptr,          /*  (40)  pointer to translator operator ptrs */
//++         2 isot_ptr       ptr,              /*  (42)  pointer to ISOT */
//++         2 sct_ptr        ptr,              /*  (44)  pointer to System Condition Table */
//++         2 unwinder_ptr           ptr,              /*  (46)  pointer to unwinder for current ring */
//++         2 sys_link_info_ptr  ptr,          /*  (48)  pointer to *system link name table */
//++         2 rnt_ptr        ptr,              /*  (50)  pointer to Reference Name Table */
//++         2 ect_ptr        ptr,              /*  (52)  pointer to event channel table */
//++         2 assign_linkage_ptr ptr,          /*  (54)  pointer to storage for (obsolete) hcs_$assign_linkage */
//++         2 heap_header_ptr     ptr,         /*  (56)  pointer to the heap header for this ring */
//++         2 trace,
//++           3 frames,
//++             4 count              fixed bin,                /*  (58)  number of trace frames */
//++             4 top_ptr            ptr unal,         /*  (59)  pointer to last trace frame */
//++           3 in_trace             bit (36) aligned, /*  (60)  trace antirecursion flag */
//++         2 pad2           bit (36),         /*  (61) */
//++                2 pad5            pointer;          /*  (62)  pointer to future stuff */
//++ 
//++ /* The following offset refers to a table within the  pl1  operator table.  */
//++ 
//++      dcl      tv_offset          fixed bin init (361) internal static;
//++                                            /* (551) octal */
//++ 
//++ 
//++ /* The following constants are offsets within this transfer vector table.  */
//++ 
//++      dcl      (
//++       call_offset        fixed bin init (271),
//++       push_offset        fixed bin init (272),
//++       return_offset              fixed bin init (273),
//++       return_no_pop_offset   fixed bin init (274),
//++       entry_offset       fixed bin init (275)
//++       )                  internal static;
//++ 
//++ 
//++ 
//++ 
//++ 
//++ /* The following declaration  is an overlay of the whole stack header.   Procedures which
//++    move the whole stack header should use this overlay.
//++ */
//++ 
//++      dcl      stack_header_overlay   (size (stack_header)) fixed bin based (sb);
//++ 
//++ 
//++ 
//++ /*         END INCLUDE FILE ... stack_header.incl.pl1 */


typedef struct __attribute__ ((__packed__)) stack_header
  {

    word36 pad1 [4];

    word72 cpm_data_ptr;

    word72 combined_stat_ptr;

    word72 clr_ptr;

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

    union
      {
        struct __attribute__ ((__packed__))
          {
            uint cpm_enabled : 18;
            uint cur_lot_size : 18;
          };
        word36 align2;
      };

    word72 system_free_ptr;

    word72 user_free_ptr;

    word72 null_ptr;

    word72 stack_begin_ptr;

    word72 stack_end_ptr;

    word72 lot_ptr;

    word36 signal_ptr [2];

    word72 bar_mode_sp;

    word72 pl1_operators_ptr;

    word72 call_op_ptr;

    word72 push_op_ptr;

    word72 return_op_ptr;

    word72 return_no_pop_op_ptr;

    word72 entry_op_ptr;

    word72 trans_op_tv_ptr;

    word72 isot_ptr;

    word72 sct_ptr;

    word72 unwinder_ptr;

    word72 sys_link_info_ptr;

    word72 rnt_ptr;

    word72 ect_ptr;

    word72 assign_linkage_ptr;

    word72 heap_header_ptr;

    word36 trace_frames_count;

    word72 trace_frames_top_ptr;

    word36 in_trace;

    word36 pad2;

    word72 pad5;
  } stack_header;



//++ "  BEGIN INCLUDE FILE ... stack_header.incl.alm  3/72  Bill Silver
//++ "
//++ "  modified 7/76 by M. Weaver for *system links and more system use of areas
//++ "  modified 3/77 by M. Weaver  to add rnt_ptr
//++ "  modified 7/77 by S. Webber to add run_unit_depth and assign_linkage_ptr
//++ "  modified 6/83 by J. Ives to add trace_frames and in_trace.
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
//++    equ     stack_header.cpm_data_ptr,4             ptr to control point for this stack
//++    equ     stack_header.combined_stat_ptr,6        ptr to separate static area
//++ 
//++    equ     stack_header.clr_ptr,8          ptr to area containing linkage sections
//++    equ     stack_header.max_lot_size,10            number of words allowed in lot (DU)
//++    equ     stack_header.main_proc_invoked,10       nonzero if main proc was invoked in run unit (DL)
//++    equ     stack_header.run_unit_depth,10  number of active run units stacked (DL)
//++    equ     stack_header.cur_lot_size,11            DU number of words (entries) in lot
//++           equ      stack_header.cpm_enabled,11             DL  non-zero if control point management is enabled
//++    equ     stack_header.system_free_ptr,12 ptr to system storage area
//++    equ     stack_header.user_free_ptr,14           ptr to user storage area
//++ 
//++    equ     stack_header.parent_ptr,16              ptr to parent stack or null
//++    equ     stack_header.stack_begin_ptr,18 ptr to first stack frame
//++    equ     stack_header.stack_end_ptr,20           ptr to next useable stack frame
//++    equ     stack_header.lot_ptr,22         ptr to the lot for the current ring
//++ 
//++    equ     stack_header.signal_ptr,24              ptr to signal proc for current ring
//++    equ     stack_header.bar_mode_sp,26             value of sp before entering bar mode
//++    equ     stack_header.pl1_operators_ptr,28       ptr: pl1_operators_$operator_table
//++    equ     stack_header.call_op_ptr,30             ptr to standard call operator
//++ 
//++    equ     stack_header.push_op_ptr,32             ptr to standard push operator
//++    equ     stack_header.return_op_ptr,34           ptr to standard return operator
//++    equ     stack_header.ret_no_pop_op_ptr,36       ptr: stand. return/ no pop operator
//++    equ     stack_header.entry_op_ptr,38            ptr to standard entry operator
//++ 
//++    equ     stack_header.trans_op_tv_ptr,40 ptr to table of translator operator ptrs
//++    equ     stack_header.isot_ptr,42                pointer to ISOT
//++    equ     stack_header.sct_ptr,44         pointer to System Condition Table
//++    equ     stack_header.unwinder_ptr,46            pointer to unwinder for current ring
//++ 
//++    equ     stack_header.sys_link_info_ptr,48       ptr to *system link name table
//++    equ     stack_header.rnt_ptr,50         ptr to reference name table
//++    equ     stack_header.ect_ptr,52         ptr to event channel table
//++    equ     stack_header.assign_linkage_ptr,54      ptr to area for hcs_$assign_linkage calls
//++    equ     stack_header.heap_header_ptr,56 ptr to heap header.
//++    equ     stack_header.trace_frames,58            stack of trace_catch_ frames
//++    equ     stach_header.trace_top_ptr,59           trace pointer
//++    equ     stack_header.in_trace,60                trace antirecurse bit
//++    equ     stack_header_end,64                     length of stack header
//++ 
//++ 
//++ 
//++ 
//++    equ     trace_frames.count,0            number of trace frames on stack
//++    equ     trace_frames.top_ptr,1          packed pointer to top one
//++ 
//++ "  The  following constant is an offset within the  pl1  operators table.
//++ "  It  references a  transfer vector table.
//++ 
//++    bool    tv_offset,551
//++ 
//++ 
//++ "  The  following constants are offsets within this transfer vector table.
//++ 
//++    equ     call_offset,tv_offset+271
//++    equ     push_offset,tv_offset+272
//++    equ     return_offset,tv_offset+273
//++    equ     return_no_pop_offset,tv_offset+274
//++    equ     entry_offset,tv_offset+275
//++ 
//++ 
//++ "  END INCLUDE FILE stack_header.incl.alm

#define tv_offset 0551
#define call_offset (tv_offset+271)
#define push_offset (tv_offset+272)
#define return_offset (tv_offset+273)
#define return_no_pop_offset (tv_offset+274)
#define entry_offset (tv_offset+275)

