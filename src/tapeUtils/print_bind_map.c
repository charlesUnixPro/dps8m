#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "bit36.h"

//static int fd;
typedef uint16_t word9;
typedef uint64_t word36;
typedef unsigned int uint;

//static uint8_t * blk;
//static word9 * blk9;
//static word36 * blk36;

#define form_link_info_args_version_1 1

struct form_link_info_args
  {
    int version;
    struct flags
      {
        bool hd_sw;
        bool ln_sw;
        bool et_sw;
        bool lk_sw;
        bool lg_sw;
        bool cn_sw;
        bool bc_sw;
      } flags;
     word36 * list_ptr;
     word36 * obj_ptr;
     char * componentname;
     long bit_count;
     long list_bc; // list segment bit count
  };
static struct form_link_info_args form_link_info_args;


static int form_msf_link_info (void);
static int form_link_info_ (struct form_link_info_args * P_arg_ptr);

static void die (char * msg)
  {
    fprintf (stderr, "%s\n", msg);
    exit (1);
  }

static int initiate_file_component (char * filename, word36 * * obj_ptr, long * bitcount)
  {
    int fd = open (filename, O_RDONLY);
    if (fd < 0)
      return fd;

    off_t length = lseek (fd, 0, SEEK_END);

    * bitcount = length * 8;

    lseek (fd, 0, SEEK_SET);
    // round up to 72 bit boundary; 72 bits in 9 bytes
    // printf ("%ld\n", length);
    length = ((length + 8) / 9) * 9;
    // printf ("%ld\n", length);
    void * blk = malloc (length);
    // length is in bytes; each word occupies 4.5 bytes
    unsigned long nwords = length * 2 / 9;
    void * blk36 = malloc (nwords * sizeof (word36));
//    blk9 = malloc (nwords * 4 * sizeof (word9));
//    if (! blk || ! blk9 || ! blk36)
    if (! blk || ! blk36)
      die ("print_bind_map: component malloc\n");

    int rc = read (fd, blk, length);
    if (rc != length)
      die ("read segment");

    word36 * p36 = blk36;
    for (uint i = 0; i < nwords; i ++)
      {
        * p36 ++ = extr36 (blk, i);
      }
//    word9 * p9 = blk9;
//    for (uint i = 0; i < nwords * 4; i ++)
//      {
//        word9 w9 = extr9 (blk, i);
//        * p9 ++ = w9;
//      }
    * obj_ptr = blk36;
    free (blk);
    close (fd);
    return 0;
  }
#if 0

component_info_.pl1 

/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   *********************************************************** */
%;
/* ******************************************************
   *                                                    *
   *                                                    *
   * Copyright (c) 1972 by Massachusetts Institute of   *
   * Technology and Honeywell Information Systems, Inc. *
   *                                                    *
   *                                                    *
   ****************************************************** */

/* The procedure component_info_ finds and returns object information about a component of a bound segment. */
/* coded  4/26/72  by M. Weaver */

name:        proc(segptr, compname, argptr, code);
/* this entry locates the component by its name */


declare        (segptr, argptr, bmp, sblkp, p, objp) pointer;

declare        (j, k, offset) fixed bin(18);
declare        code fixed bin(35);
declare        (type, i, namsw) fixed bin;
declare        error_table_$name_not_found ext fixed bin(35);
declare        bitcount fixed bin(24);

declare        (addr, addrel, bin, bit,  divide, fixed, ptr, rel, substr) builtin;

declare        var_string char(j) based(p);
declare        string char(100000) based(sblkp);
declare        compname char(32) aligned;

declare        hcs_$status_mins ext entry(ptr, fixed bin, fixed bin(24), fixed bin(35));
declare        get_bound_seg_info_ ext entry( ptr, fixed bin(24), ptr, ptr, ptr, fixed bin(35));

%include bind_map;

declare        1 oi aligned like object_info;

%include object_info;

%include symbol_block;


declare        1 osb aligned based(sblkp),
        2 gen_name_boff fixed bin,
        2 dum1 bit(18) unaligned,
        2 gen_name_blength bit(18) unaligned,
        2 gen_vers_boff fixed bin,
        2 dum2 bit(18) unaligned,
        2 gen_vers_blength bit(18) unaligned,
        2 gen_creation_time fixed bin(71),
        2 obj_creation_time fixed bin(71),
        2 symb_tree bit(18) unaligned,
        2 extension bit(18) unaligned,
        2 hmap bit(18) unaligned,
        2 n bit(18) unaligned,
        2 next_header bit(18) unaligned,
        2 bind_flag bit(18) unaligned,
        2 text_lng bit(18) unaligned,
        2 link_lng bit(18) unaligned,
        2 obj_name_boff fixed bin,
        2 dum3 bit(18) unaligned,
        2 obj_name_blength bit(18) unaligned;


/* the following structure is copied from component_info.incl.pl1 */

declare        1 ci aligned based(argptr),                /* structure to be filled in */
        2 dcl_version fixed bin,                /* version number of this structure */
        2 name char(32) aligned,                /* objectname of component segment */
        2 text_start pointer,                /* ptr to component's section of text */
        2 stat_start pointer,                /* pointer to component's section of internal static */
        2 symb_start pointer,                /* pointer to component's first symbol block */
        2 defblock_ptr pointer,                /* ptr to component's definition block */
        2 text_lng fixed bin,                /* length of text section */
        2 stat_lng fixed bin,                /* length of internal static */
        2 symb_lng fixed bin,                /* length of symbol section */
        2 n_blocks fixed bin,                /* number of symbol blocks in component's symbol section */
        2 standard bit(1) aligned,                /* indicates whether component is in standard (new) format */
        2 compiler char(8) aligned,                /* name of component's compiler */
        2 compile_time fixed bin(71),                /* time component was compiled */
        2 userid char(32) aligned,                /* id of creator of component */
        2 cvers aligned,                        /* version of component's compiler */
          3 offset bit(18) unaligned,                /* offset in words relative to symb_start */
          3 length bit(18) unaligned,                /* length of version name in characters */
        2 comment aligned,                        /* component's comment */
          3 offset bit(18) unaligned,                /* offset in words relative to symb_start */
          3 length bit(18) unaligned,                /* length of comment in characters */
        2 source_map fixed bin;                /* offset, rel to beg of symbol block, of component's source map */


        namsw = 1;                                /* indicate that name was given */
        go to start;

offset:        entry(segptr, offset, argptr, code);

        namsw = 0;                                /* indicate that name was not given */

start:        objp = ptr(segptr, 0);                        /* be sure we have ptr to beg of bound seg */
        call hcs_$status_mins(objp, type, bitcount, code);   /* get bit count */
        if code ^= 0 then return;                        /* too bad */

        oi.version_number = object_info_version_2;
        call get_bound_seg_info_(objp, bitcount, addr(oi), bmp, sblkp, code);
                                                /* get ptrs to bindmap indo */
        if code ^= 0 then return;                        /* evidently not a bound segment */

        go to looplab(namsw);                        /* bindmap is searched differently for offset or name */

looplab(0):
find_offset:
        do i = 1 to bmp -> bindmap.n_components;        /* look at each component */
             j = fixed(component(i).text_start, 18);
             k = fixed(component(i).text_lng, 18);
             if offset >= j then if offset < j+k        /* offset is within this component's text section */
                then go to fill_in;
             j = fixed(component(i).stat_start, 18) + fixed(rel(oi.statp), 18);
                                                /* get offset of this conponent's internal static */
             k = fixed(component(i).stat_lng, 18);
             if offset >= j then if offset < j+k        /* offset is within component's internal static */
                then go to fill_in;
             j = fixed(component(i).symb_start, 18) + fixed(rel(oi.symbp), 18);
                                                /* get offset of this component's symbol section */
             k = fixed(component(i).symb_lng, 18);
             if offset >= j then if offset < j+k        /* offset is within component's symbol section */
                then go to fill_in;
        end;

        code = error_table_$name_not_found;                /* offset can't be associated with a component */
        return;

looplab(1):
find_name:
        do i = 1 to n_components;                        /* look at each component */
             p = addrel(sblkp, component(i).name_ptr);        /* get ptr to component's name in bindmap symb block */
             j = fixed(component(i).name_lng, 18);
             if compname = var_string then go to fill_in;   /* name matches component name */
        end;

        code = error_table_$name_not_found;                /* component name not in bound segment */
        return;

fill_in:                                                /* fill in argument structure */
        ci.dcl_version = 1;
        if namsw > 0 then ci.name = compname;
        else do;
             p = addrel(sblkp, component(i).name_ptr);        /* get addr of component name in bindmap */
             j = fixed(component(i).name_lng, 18);
             ci.name = var_string;
        end;

        ci.compiler = component(i).comp_name;


        ci.text_start = addrel(oi.textp, component(i).text_start);
        ci.stat_start = addrel(oi.statp, component(i).stat_start);
        ci.symb_start = addrel(oi.symbp, component(i).symb_start);
        ci.defblock_ptr = addrel(oi.defp, component(i).defblock_ptr);


        ci.text_lng = fixed(component(i).text_lng,17);
        ci.stat_lng = fixed(component(i).stat_lng,17);
        ci.symb_lng = fixed(component(i).symb_lng,17);

        j = fixed(component(i).n_blocks,17);
        if j = 0 then ci.n_blocks = 1;                /* assume 1 if not filled in */
        else ci.n_blocks = j;

         /* we have finished copying items found in the bind map;
           now we must go to the component's symbol section for the rest */

        sblkp = ci.symb_start;                        /* get ptr to component's symbol section */
        if substr(osb.dum1, 1, 9) = "010100000"b then go to old_format;        /* look for old block flag */

new_format:
        ci.standard = "1"b;                /* get_bound_seg_info_ would have returned bad code if new and non-standard */
        ci.compile_time = sb.obj_creation_time;

        j = fixed(sb.uid_offset, 18) * 4 + 1;
        k = fixed(sb.uid_length, 18);
        ci.userid = substr(string, j, k);                /* fill in userid string */

        ci.cvers.offset = sb.gen_name_offset;
        ci.cvers.length = sb.gen_name_length;

        ci.comment.offset = sb.comment_offset;
        ci.comment.length = sb.comment_length;

        ci.source_map = fixed(sb.source_map, 18);

        return;

old_format:
        ci.standard = "0"b;

        ci.compile_time = osb.obj_creation_time;
        ci.userid = " ";

        ci.cvers.offset = bit(bin(divide(osb.gen_vers_boff, 36, 18, 0), 18), 18);
        ci.cvers.length = bit(bin(divide(fixed(osb.gen_vers_blength, 18), 9, 18, 0), 18), 18);

        ci.comment.offset, ci.comment.length = "0"b;
        ci.source_map = 0;

return;
        end;



                    date_compiled.pl1               07/17/90  1535.9rew 07/17/90  1534.5      170892



/****^  ***********************************************************
        *                                                         *
        * Copyright, (C) BULL HN Information Systems Inc., 1990   *
        *                                                         *
        * Copyright, (C) Honeywell Information Systems Inc., 1982 *
        *                                                         *
        * Copyright (c) 1972 by Massachusetts Institute of        *
        * Technology and Honeywell Information Systems, Inc.      *
        *                                                         *
        *********************************************************** */


date_compiled: dtc: proc;

/* DATE_COMPILED - useful program tells when object was compiled. */
/* Modified 08/04/83 by Jim Lippard to print error message when specified
   component of a bound object is not found, to accept archive components,
   and to upgrade code */
/* Modified 06/02/84 by Jim Lippard to not print octal garbage when bad
   pathnames are given and to reject starnames */


/****^  HISTORY COMMENTS:
  1) change(85-09-05,Spitzer), approve(85-09-05,MCR7267),
     audit(85-12-10,Lippard), install(85-12-16,MR12.0-1001):
     1) Bugfix to AF use
     so it won't display an error when it finds a match.  2) Use all segnames
     when given a bound component, rather than only the primary segname.
  2) change(85-12-19,Spitzer), approve(85-12-19,MCR7267),
     audit(85-12-19,Martinson), install(85-12-19,MR12.0-1003):
     Revoke changes made for 1).
  3) change(86-02-19,Spitzer), approve(86-02-19,PBF7267),
     audit(86-03-12,Lippard), install(86-03-17,MR12.0-1031):
     PBF to 1025.
  4) change(86-08-04,Lippard), approve(86-08-04,PBF7267),
     audit(86-08-06,Hartogs), install(86-08-07,MR12.0-1121):
     Print error message correctly for case when entry point is not found
     in bound segment or archive.
  5) change(86-08-06,Elhard), approve(86-08-06,MCR7457),
     audit(86-08-21,DGHowe), install(86-11-20,MR12.0-1222):
     Modified to extract dates from all components of an object MSF.
  6) change(90-04-20,Vu), approve(90-04-20,MCR8172), audit(90-06-04,Blackmore),
     install(90-07-17,MR12.4-1021):
     date_compiled prints more than what is asked for.
                                                   END HISTORY COMMENTS */


        dcl     ME                 char (13) internal static options (constant) init ("date_compiled");
        dcl     dname                 char (168),
                ename                 char (32),
                cname                 char (32),
                (idx, jdx)                 fixed bin,
                bitc                 fixed bin (24),
                (long, brief)         bit (1),
                firstarg                 bit (1),
                archive                 bit (1),
                msf                 bit (1),
                nsr                 fixed bin,
                nargs                 fixed bin,
                an                 fixed bin,
                ap                 ptr,
                al                 fixed bin,
                arg                 char (al) unaligned based (ap),
                fb71u                 fixed bin (71) unal based,
                code                 fixed bin (35),
                rvp                 ptr,
                rvl                 fixed bin,
                rv                 char (rvl) var based (rvp),
                afsw                 bit (1),
                seg_ptr                 ptr,
                comp_ptr                 ptr,
                comp_bc                 fixed bin (24),
                comp_name                 char (32),
                eof                 bit (1) aligned;

        dcl     1 search                 (25) aligned,
                2 name                 char (32),
                2 found                 bit (1);

        dcl     (addr, null, substr, fixed, hbound, index, addrel, reverse, rtrim) builtin;

        dcl     complain                 entry variable options (variable);
        dcl     check_star_name_$path         entry (char(*), fixed bin(35)),
                cu_$arg_ptr                 entry (fixed bin, ptr, fixed bin, fixed bin (35)),
                cu_$arg_count         entry (fixed bin, fixed bin (35)),
                cu_$af_return_arg         entry (fixed bin, ptr, fixed bin, fixed bin (35)),
                decode_definition_         entry (ptr, ptr) returns(bit(1) aligned),
                  decode_definition_$full entry (ptr, ptr, ptr) returns (bit (1) aligned),
                expand_pathname_$component entry (char (*), char (*), char (*), char (*), fixed bin (35)),
                ioa_                 entry options (variable),
                date_time_                 entry (fixed bin (71), char (*)),
                active_fnc_err_         entry options (variable),
                com_err_                 entry options (variable);

        dcl     archive_$next_component entry (ptr, fixed bin (24), ptr, fixed bin (24), char (*), fixed bin (35));

        dcl     pathname_$component         entry (char (*), char (*), char (*)) returns (char (194));
        dcl     pathname_                 entry (char(*), char(*)) returns(char(168));
        dcl     error_table_$badopt         fixed bin (35) ext;
        dcl     error_table_$noarg         fixed bin (35) ext;
        dcl     error_table_$nostars         fixed bin (35) ext;
        dcl     error_table_$not_act_fnc fixed bin (35) ext;
        dcl     error_table_$too_many_args fixed bin (35) ext;

        dcl     get_system_free_area_         entry() returns(ptr);
        dcl     hcs_$get_bc_author         entry (char (*), char (*), char (*), fixed bin (35));

        dcl     object_lib_$initiate         entry (char(*), char(*), char(*), bit(1), ptr, fixed bin(24), bit(1), fixed bin(35));
        dcl     object_lib_$get_component_info
                                 entry (ptr, ptr, char(8), char(*), ptr, fixed bin(35));
        dcl     terminate_file_         entry (ptr, fixed bin (24), bit (*), fixed bin (35));

        dcl     object_info_$display         entry (ptr, fixed bin (24), ptr, fixed bin (35)),
                get_bound_seg_info_         entry (ptr, fixed bin (24), ptr, ptr, ptr, fixed bin (35));

        dcl     TRUE                 bit (1) internal static options (constant) init ("1"b);
        dcl     FALSE                 bit (1) internal static options (constant) init ("0"b);

/* ======================================================= */

        long, brief, archive = FALSE;
        firstarg = TRUE;
        nsr = 0;

        call cu_$af_return_arg (nargs, rvp, rvl, code);
        if code = error_table_$not_act_fnc then afsw = FALSE; else afsw = TRUE;
        if afsw then complain = active_fnc_err_;
        else do;
                complain = com_err_;
                call cu_$arg_count (nargs, code);
             end;
        if nargs = 0 then do;
                call complain (error_table_$noarg, ME, "path");
                return;
             end;
        do an = 1 to nargs;
             call cu_$arg_ptr (an, ap, al, code);
             if index (arg, "-") = 1 then do;
                     if arg = "-long" | arg = "-lg" then long = TRUE;
                     else if arg = "-brief" | arg = "-bf" then brief = TRUE;
                     else do;
                             call complain (error_table_$badopt, ME, arg);
                             return;
                        end;
                end;
             else if firstarg then do;
                     firstarg = FALSE;

                     call expand_pathname_$component (arg, dname, ename, cname, code);
                     if code ^= 0 then do;
                             call complain (code, ME, "^a", arg);
                             return;
                        end;

                     call check_star_name_$path (ename, code);

                     if code ^= 0 then do;
have_star:                     if (code = 1) | (code = 2) then
                                call complain (error_table_$nostars, ME, "^a", arg);
                             else call complain (code, ME, "^a", arg);
                             return;
                        end;

                     if cname ^= "" then do;
                             call check_star_name_$path (cname, code);
                             if code ^= 0 then go to have_star;

                             nsr = 1;
                             search.name (nsr) = cname;
                        end;
                end;
             else do;                                /* search arg */
                     nsr = nsr + 1;
                     if nsr > hbound (search, 1) then do;
                             call complain (error_table_$too_many_args, ME);
                             return;
                        end;

                     call check_star_name_$path (arg, code);

                     if code ^= 0 then do;
                             call complain (error_table_$nostars, ME, "^a", arg);
                             return;
                        end;

                     search.name (nsr) = arg;
                     search.found (nsr) = FALSE;
                end;

        end;

        call object_lib_$initiate (dname, ename, "", ""b, seg_ptr, bitc, msf, code);
        if seg_ptr = null then do;
                call complain (code, ME, "^a", pathname_$component (dname, ename, cname));
                return;
             end;

        if msf then do;
                call date_msf_compiled_ (seg_ptr, ename, code);
                if code ^= 0 then call complain (code, ME, "^a", pathname_ (dname, ename));
             end;
        
        else if index (reverse (rtrim (ename)), "evihcra.") = 1 then do;
                archive = TRUE;
                comp_ptr = null ();
                call archive_$next_component (seg_ptr, bitc, comp_ptr, comp_bc, comp_name, code);
                if code ^= 0 then do;
                        call complain (code, ME, "^a", pathname_$component (dname, ename, cname));
                        return;
                     end;
                do while (comp_ptr ^= null ());
                     if index (reverse (rtrim (comp_name)), "dnib.") ^= 1 then do;
                             if nsr = 0 then call date_compiled_ (comp_ptr, comp_bc, comp_name, code);
                             else do idx = 1 to nsr;
                                     if rtrim (comp_name) = search.name (idx) then do;
                                             call date_compiled_ (comp_ptr, comp_bc, comp_name, code);
                                             search.found (idx) = TRUE;
                                        end;
                                end;
                             if code ^= 0 then call complain (code, ME, "^a", pathname_$component (dname, ename, cname));
                        end;
                     call archive_$next_component (seg_ptr, bitc, comp_ptr, comp_bc, comp_name, code);
                     if code ^= 0 then do;
                             call complain (code, ME, "^a", pathname_$component (dname, ename, cname));
                             return;
                        end;
                end;
             end;

        else do;
                call date_compiled_ (seg_ptr, bitc, ename, code);
                if code ^= 0 then call complain (code, ME, "^a", pathname_$component (dname, ename, cname));
             end;

        call terminate_file_ (seg_ptr, (0), TERM_FILE_TERM, code);
nlx:
        if afsw
        then do;
             firstarg = (nsr = 0);
             do jdx = 1 to nsr while (^firstarg);
                firstarg = search.found (jdx);
                end;
             if ^firstarg then call complain (0, ME, "No matching components found in ^[archive^s^;bound ^[MSF^;segment^]^].", archive, msf);
             end;
        else do jdx = 1 to nsr;
             if ^search.found (jdx) then call complain (0, ME, "Component not found in ^[archive^s^;bound ^[MSF^;segment^]^]. ^a", archive, msf, search.name (jdx));
        end;

        return;

date_compiled_: proc (seg_ptr, bitc, ename, code);

        dcl     seg_ptr                 ptr,                /* ptr to segment */
                bitc                 fixed bin (24),        /* lth of segment */
                ename                 char (32),        /* name of segment */
                code                 fixed bin (35);        /* errcode */

        dcl     (bmp, sblkp, mapp, comp_name_ptr) ptr;
        dcl     answer                 char (256) var init ("");
        dcl     author                 char (32);
        dcl     atp                 ptr, atl fixed bin, based_author char (atl) based (atp);
        dcl     (idx, jdx, kdx)         fixed bin;
        dcl     comp_name_len         fixed bin;
        dcl     datstr                 char (16);
        dcl     user                 char (32);
        dcl     compiler                 char (64);
        dcl     name                 char (comp_name_len) based (comp_name_ptr);
        dcl     cname                 char (kk) based (qq);
        dcl     kk                 fixed bin, qq ptr;
        dcl     1 dd                 like decode_definition_full aligned;
        dcl     1 oi                 like object_info aligned;

/* include files at end */

        code = 0;
        oi.version_number = object_info_version_2;
        call object_info_$display (seg_ptr, bitc, addr (oi), code);
        if code ^= 0 then return;

        call date_time_ (oi.compile_time, datstr);
        compiler = oi.compiler;
        user = oi.userid;
        if substr (user, 1, 1) <= " " then call hcs_$get_bc_author (dname, ename, user, code);
        if long then do;
                if oi.symbp ^= null & oi.cvers.offset ^= FALSE then do;
                        qq = addrel (oi.symbp, oi.cvers.offset);
                        kk = fixed (oi.cvers.length, 18);
                        if kk > 0 & substr (cname, 1, 1) > " "
                        then compiler = cname;
                     end;
             end;

        if oi.format.bound then do;
                call get_bound_seg_info_ (seg_ptr, bitc, addr (oi), bmp, sblkp, code);
                if code ^= 0 then do;
                        call complain (code, ME, "^a", pathname_$component (dname, ename, cname));
                        return;
                     end;
                if (nsr = 0) | archive then do;        /* always title if bound seg is in archive */
                        if brief then answer = datstr;
                        else answer = datstr || " " || rtrim (ename) || " " || rtrim (user) || " " || rtrim (compiler);
                        if afsw then go to afret;
                        call ioa_ ("Bound ^a", answer);
                     end;
                do idx = 1 to n_components;
                     comp_name_ptr = addrel (sblkp, component (idx).name_ptr);
                     comp_name_len = fixed (component (idx).name_lng, 18);
                     jdx = fixed (component (idx).symb_start, 18);
                     if nsr > 0 then do;
                                  do kdx = 1 to nsr;
                                     if name = search.name (kdx) then goto doit1;
                                             end;

                             eof = decode_definition_$full (addrel (oi.defp, bindmap.component(idx).defblock_ptr), addr (dd), addr (oi));
                             do while (^eof & (dd.section = "segn") & ^dd.flags.ignore);
                                do kdx = 1 to nsr;
                                     if dd.symbol = search.name (kdx) then goto doit1;
                                        end;                /* do kdx */
                                eof = decode_definition_$full (dd.next_def, addr (dd), addr (oi));
                                end;                /* do while */
                             go to skp1;
                        end;
doit1:                     mapp = addrel (oi.symbp, jdx);
                     call date_time_ ((addr (mapp -> sb.obj_creation_time) -> fb71u), datstr);
                     if nsr > 0 then search.found (kdx) = TRUE;
                     compiler = component (idx).comp_name;
                     if mapp -> sb.uid_offset > component (idx).symb_lng then author = "?.?.?";
                     else do;
                             atp = addrel (mapp, mapp -> sb.uid_offset);
                             atl = fixed (mapp -> sb.uid_length, 18);
                             author = based_author;
                             if substr (author, 1, 1) < " " then author = "?.?.?";
                        end;
                     if long then do;
                             if sb.decl_vers = 1 then do;
                                     qq = addrel (sblkp, component (idx).symb_start);
                                     kk = fixed (qq -> sb.gen_name_length, 18);
                                     qq = addrel (qq, qq -> sb.gen_name_offset);
                                     if kk > 0
                                     then if substr (cname, 1, 1) > " "
                                        then compiler = cname;
                                end;
                        end;
                     if brief then answer = datstr || " " || name;
                     else answer = datstr || " " || rtrim (name) || " " || rtrim (author) || " " || rtrim (compiler);
                     if afsw then go to afret;
                     call ioa_ ("  ^a", answer);
skp1:                end;
                return;
             end;

        if brief then answer = datstr;
        else answer = datstr || " " || rtrim (ename) || " " || rtrim (user) || " " || rtrim (compiler);
        if afsw then do;
afret:                rv = answer;
                go to nlx;
             end;
        call ioa_ ("^a", answer);
%page;
%include symbol_block;
%include bind_map;
%include object_info;
     end date_compiled_;

date_msf_compiled_: proc (seg_ptr, ename, code);

        dcl     seg_ptr                ptr,                /* ptr to component 0 */
                ename                char (32),        /* name of MSF */
                code                fixed bin(35);        /* errcode */

        dcl     sys_areap                ptr;
        dcl     based_authorp        ptr;
        dcl     based_authorl        fixed bin (18) unsigned;
        dcl     based_author        char (based_authorl) based (based_authorp);
        dcl     cnamep                ptr;
        dcl     cnamel                fixed bin (18) unsigned;
        dcl     cname                char (cnamel) based (cnamep);
        dcl     mapp                ptr;
        dcl     sys_area                area based (sys_areap);
        dcl     oip                ptr;
        dcl     compiler                char (64);
        dcl     user                char (32);
        dcl     bmp                ptr;
        dcl     sblkp                ptr;
        dcl     c                        fixed bin;
        dcl     idx                fixed bin;
        dcl     sr_idx                fixed bin;
        dcl     date_str                char (16);
        dcl     answer                char (256) varying;
        dcl     author                char (32);
        dcl     found                bit (1);
        dcl     map_relp                fixed bin (18) unsigned;
        dcl     comp_namep                ptr;
        dcl     comp_namel                fixed bin (18) unsigned;
        dcl     comp_name                char (comp_namel) based (comp_namep);
        dcl     01 oi                aligned like object_info based (oip);
        dcl     01 dd                aligned like decode_definition_str;
        dcl     cleanup                condition;
        
        code = 0;
        answer = "";
        sys_areap = get_system_free_area_ ();
        comp_infop = null;
        
        on cleanup begin;
          if comp_infop ^= null
            then do;
              do c = 1 to component_info.max;
                call terminate_file_ (component_info.comp (c).segp, component_info.comp (c).bc, TERM_FILE_TERM, 0);
              end;
              free component_info in (sys_area);
            end;
        end;
        
        call object_lib_$get_component_info (seg_ptr, sys_areap, component_info_version_1, "display", comp_infop, code);
        if code ^= 0 then return;
        
        oip = addr (component_info.comp (0).info);
        
        call date_time_ (oi.compile_time, date_str);
        compiler = oi.compiler;
        user = oi.userid;
        
        if substr (user, 1, 1) = " " then call hcs_$get_bc_author (pathname_ (dname, ename), "0", user, code);
        if long then do;
                if oi.symbp ^= null & oi.cvers.offset ^= ""b then do;
                        cnamep = addrel (oi.symbp, oi.cvers.offset);
                        cnamel = fixed (oi.cvers.length, 18);
                        if cnamel > 0 & substr (cname, 1, 1) > " "
                        then compiler = cname;
                     end;
             end;
        
        if nsr = 0 then do;
                if brief then answer = date_str;
                else answer = date_str || " " || rtrim (ename) || " " || rtrim (user) || " " || rtrim (compiler);
                if afsw then go to afret;
                call ioa_ ("Bound ^a", answer);
             end;
        
        do c = 1 to component_info.max;
             oip = addr (component_info.comp (c).info);
             call get_bound_seg_info_ (component_info.comp (c).segp, component_info.comp (c).bc, oip, bmp, sblkp, code);
             if code ^= 0 then do;
                     call complain (code, ME, "^a", pathname_ (dname, ename));
                     go to return;
                end;
             
             do idx = 1 to bindmap.n_components;
                     comp_namep = addrel (sblkp, component (idx).name_ptr);
                     comp_namel = fixed (component (idx).name_lng, 18);
                     map_relp = fixed (component (idx).symb_start, 18);
                     if nsr > 0 then do;
                             found = FALSE;
                             do sr_idx = 1 to nsr;
                                if comp_name = search.name (sr_idx) then search.found (sr_idx), found = TRUE;
                             end;
                             eof = decode_definition_ (seg_ptr, addr (dd));
                             eof = decode_definition_ (addrel (oi.defp, bindmap.component(idx).defblock_ptr), addr (dd));
                             do while (^eof & (dd.section = "segn") & ^found);
                                do sr_idx = 1 to nsr;
                                     if dd.symbol = search.name (sr_idx) then search.found (sr_idx), found = TRUE;
                                     end;
                                eof = decode_definition_ (dd.next_def, addr (dd));
                                end;                /* do while */
                        end;
                     else found = TRUE;

                if found then do;
                        mapp = addrel (oi.symbp, map_relp);
                        call date_time_ ((addr (mapp -> sb.obj_creation_time) -> fb71u), date_str);
                        compiler = bindmap.component (idx).comp_name;
                        if mapp -> sb.uid_offset > bindmap.component (idx).symb_lng then author = "?.?.?";
                        else do;
                                based_authorp = addrel (mapp, mapp -> sb.uid_offset);
                                based_authorl = fixed (mapp -> sb.uid_length, 18);
                                author = based_author;
                                if substr (author, 1, 1) < " " then author = "?.?.?";
                             end;
                          if long then do;
                                if mapp -> sb.decl_vers = 1 then do;
                                        cnamep = addrel (sblkp, bindmap.component (idx).symb_start);
                                        cnamel = fixed (cnamep -> sb.gen_name_length, 18);
                                        cnamep = addrel (cnamep, cnamep -> sb.gen_name_offset);
                                        if cnamel > 0
                                        then if substr (cname, 1, 1) > " "
                                             then compiler = cname;
                                     end;
                             end;
                        
                        if brief then answer = date_str || " " || comp_name;
                        else answer = date_str || " " || rtrim (comp_name) || " " || rtrim (author) || " " || rtrim (compiler);
                        if afsw then go to afret;
                        call ioa_ ("  ^a", answer);
                     end;
             end;
        end;
        
return:        do c = 1 to component_info.max;
             call terminate_file_ (component_info.comp (c).segp, component_info.comp (c).bc, TERM_FILE_TERM, 0);
        end;
        free component_info in (sys_area);
        return;
                
afret:        rv = answer;
        do c = 1 to component_info.max;
             call terminate_file_ (component_info.comp (c).segp, component_info.comp (c).bc, TERM_FILE_TERM, 0);
        end;
        free component_info in (sys_area);
        goto nlx;
        
%include object_info;
%include object_lib_defs;
%include bind_map;
%include symbol_block;

     end date_msf_compiled_;
        
%include decode_definition_str;
%include terminate_file;
%include access_mode_values;
     end date_compiled;




                    decode_definition_.pl1          11/04/82  1947.9rew 11/04/82  1624.8      129123



/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   * Copyright (c) 1972 by Massachusetts Institute of        *
   * Technology and Honeywell Information Systems, Inc.      *
   *                                                         *
   *********************************************************** */


/* Decode Definition - internal subroutine to return the information contained
   in a given object segment definition, in an explicit and directly-accessible format.

   Designed and initially coded by Michael J. Spier, March 29, 1971         */
/* Modified 1972 by R. Barnes to add $full entrypoint */
/* Modified May 1981 by M. Weaver to make $full not mess up static variables */


decode_definition_:        procedure(definition_ptr, structure_ptr, code);

declare        (definition_ptr, structure_ptr) pointer, code bit(1) aligned;

        /* DECLARATION OF EXTERNAL SYMBOLS */

declare        object_info_$brief external entry(pointer,fixed bin(24),pointer,fixed bin(35));
declare        hcs_$status_mins entry(ptr, fixed bin, fixed bin(24), fixed bin(35));

        /* DECLARATION OF INTERNAL STATIC VARIABLES */

declare        section_table(0:4) char(4) internal static initial("text","link","symb","segn", "stat");
declare        (segbase,static_defbase,static_linkbase) pointer internal static initial(null);
declare        (initialize, bitcount) fixed bin(24) internal static initial(0);

        /* DECLARATION OF AUTOMATIC STORAGE VARIABLES */

declare        (i, lng) fixed bin;
declare        err_code fixed bin(35);
declare        ret_acc bit(1) aligned init("0"b);
declare        fullsw bit(1) aligned;
declare        (argp, dp, acc_ptr, linkp, oip, oi_ptr) pointer;
declare        (defbase, linkbase) pointer;
declare        based_bit bit(36) based;
declare        oi_area(50) fixed bin(35);                /* for automatic version of oi structure */


        /* DECLARATION OF BUILTIN FUNCTIONS */

declare        (addr, addrel, baseno, bin, fixed, null, rel, substr) builtin;

        /* DECLARATION OF BASED STRUCTURES */

declare        1 short_dd aligned based(argp),
        2 snext_def pointer,                        /* pointer to next definition in list */
        2 slast_def pointer,                        /* pointer to previous definition in list */
        2 sblock_ptr pointer,                /* pointer to either defblock or segname block */
        2 ssection char(4) aligned,                /* "text", "link", "symb" or "segn"   */
        2 soffset fixed bin,                        /* offset within class (if ^= "segn")   */
        2 sentrypoint fixed bin,                /* value of entrypoint in text if ^= 0   */
        2 ssymbol char(32) aligned;                /* the symbolic name of the definition  */

declare        1 dd aligned based(argp),                /* structure filled in by full entry */
        2 next_def ptr,                        /* ptr to next definition in list */
        2 last_def ptr,                        /* ptr to previous definition in list */
        2 block_ptr ptr,                        /* ptr to either defblock or segname block */
        2 section char(4) aligned,                /* "text", "link", "symb" or "segn" */
        2 offset fixed bin,                        /* offset within class (if ^= "segn") */
        2 entrypoint fixed bin,                /* value of entrypoint in text if ^= 0 */
        2 symbol char(256) aligned,                /* the symbolic name of the definition */
        2 symbol_lng fixed bin,                /* the actual length of symbol */
        2 flags,                                /* same flags as in std def */
          3 a_new_format bit(1) unaligned,        /* def is in new format */
          3 a_ignore bit(1) unaligned,        /* linker should ignore this def */
          3 a_entrypoint bit(1) unaligned,        /* def is for entrypoint */
          3 a_retain bit(1) unaligned,
          3 a_arg_count bit(1) unaligned,        /* there is an arg count for entry */
          3 a_descr_sw bit(1) unaligned,        /* there are valid descriptors for entry */
          3 unused bit(12) unaligned,
        2 n_args fixed bin,                        /* # of args entry expects */
        2 descr_ptr ptr;                        /* ptr to array of rel ptrs to descriptors for entry */

declare 1 acc_structure aligned based(argp),
        2 next_defx ptr,                        /* pointer to next definition in list */
        2 last_defx ptr,                        /* pointer to previous definition in list */
        2 block_ptrx ptr,                /* pointer to either defblock or segname block */
        2 sectionx char(4) aligned,                /* "text", "link", "symb" or "segn"   */
        2 offsetx fixed bin,                        /* offset within class (if ^= "segn")   */
        2 entrypointx fixed bin,                /* value of entrypoint in text if ^= 0   */
        2 ret_acc_ptr ptr;                        /* Pointer to ACC representation of string */

declare        1 definition based(dp) aligned,
        2 forward bit(18) unaligned,
        2 backward bit(18) unaligned,
        2 value bit(18) unaligned,
        2 flags unaligned,
          3 new_def bit(1) unaligned,
          3 ignore bit(1) unaligned,
          3 entrypoint bit(1) unaligned,
          3 retain bit(1) unaligned,
          3 arg_count bit(1) unaligned,
          3 descr_sw bit(1) unaligned,
          3 dum bit(9) unaligned,
        2 class bit(3) unaligned,
        2 string_ptr bit(18) unaligned,
        2 segname_ptr bit(18) unaligned,
        2 nargs bit(18) unaligned,
        2 descriptor(bin(nargs,18)) bit(18) unaligned;

declare        1 eax(3) aligned based(linkp),
        2 location bit(18) unaligned,
        2 op_code bit(6) unaligned;

declare        1 linkheader aligned based(linkbase),
        2 defseg fixed bin,
        2 defoffset bit(18) unaligned,
        2 dum1 bit(18) unaligned,
        2 block_thread pointer,
        2 dum2 pointer,
        2 link_begin bit(18) unaligned,
        2 sect_lng bit(18) unaligned;

declare        1 class_3_def based(dp) aligned,
        2 dum1 bit(36) aligned,
        2 next_class3 bit(18) unaligned,
        2 dum2 bit(36) unaligned,
        2 defblock_ptr bit(18) unaligned;

declare        1 acc aligned based(acc_ptr),
        2 lng bit(9) unaligned,
        2 dum1 bit(27) unaligned;

declare        acc_string char(33) aligned based(acc_ptr);

declare        delimitor fixed bin based(dp);



declare        1 oi aligned based(oip) like object_info;

%include object_info;


declare        1 old_oi aligned based(oip),                /* structure containing object info, returned by object_info_ */
        2 version_number fixed bin,                /* version number of current structure format */
        2 textp pointer,                        /* pointer to beginning of text section */
        2 defp pointer,                        /* pointer to beginning of definition section */
        2 linkp pointer,                        /* pointer to beginning of linkage section */
        2 symbp pointer,                        /* pointer to beginning of symbol section */
        2 bmapp pointer,                        /* pointer to beginning of break map (may be null) */
        2 tlng fixed bin,                        /* length in words of text section */
        2 dlng fixed bin,                        /* length in words of definition section */
        2 llng fixed bin,                        /* length in words of linkage section */
        2 slng fixed bin,                        /* length in words of symbol section */
        2 blng fixed bin,                        /* length in words of break map */
        2 format,                                /* word containing bit flags about object type */
          3 old_format bit(1) unaligned,        /* on if segment isn't in new format, i.e. has old style object map */
          3 bound bit(1) unaligned,                /* on if segment is bound */
          3 relocatable bit(1) unaligned,        /* on if seg has relocation info in its first symbol block */
          3 procedure bit(1) unaligned,        /* on if segment is an executable object program */
          3 standard bit(1) unaligned,        /* on if seg is in standard format (more than just standard map) */
          3 gate bit(1) unaligned,                /* on if segment is a gate */
          3 no_old_alm bit(1) unaligned,        /* if bound, means there are no old format alm components (temp.) */
        2 call_delimiter fixed bin,                /* call delimiter if segment is a gate */

        /* LIMIT OF BRIEF STRUCTURE */

        2 compiler char(8) aligned,                /* name of processor which generated segment */
        2 compile_time fixed bin(71),                /* clock reading of date/time object was generated */
        2 userid char(32) aligned,                /* standard Multics id of creator of object segment */
        2 cvers aligned,                        /* generator version name in printable char string form */
          3 offset bit(18) unaligned,                /* offset of name in words relative to base of symbol section */
          3 length bit(18) unaligned,                /* length of name in characters */
        2 comment aligned,                        /* printable comment concerning generator or generation of segment */
          3 offset bit(18) unaligned,                /* offset of comment in words relative to base of symbol section */
          3 length bit(18) unaligned,                /* length of comment in characters */
        2 source_map fixed bin,                /* offset, relative to base of symbol section, of source map structure */

        /* LIMIT OF DISPLAY STRUCTURE */

        2 rel_text pointer,                        /* offset rel to base of symbol section of text sect. relocation info */
        2 rel_def pointer,                         /* offset relative to symbp of definition section relocation info */
        2 rel_link pointer,                        /* offset relative to symbp of linkage section relocation info */
        2 rel_symbol pointer,                /* offset relative to symbp of symbol section relocation info */
        2 text_boundary fixed bin,                /* specifies mod  of text section base boundary */
        2 static_boundary fixed bin,                /* specifies mod of internal static base boundary */
        2 default_truncate fixed bin,                /* offset rel to symbp for binder to automatically trunc. symb sect. */
        2 optional_truncate fixed bin;        /* offset rel to symbp for binder to optionally trunc. symb sect. */




        fullsw = "0"b;
        oip = addr(oi_area);                /* initialize for original entries */
        argp = structure_ptr;                /* copy arguments into stack */
        dp = definition_ptr;                /* ... */
        if initialize = 1 then
        do;
             if baseno(dp) = baseno(segbase) then goto get_object_info;
             initialize,
             bitcount = 0;
             segbase = null;
        end;
        if rel(dp) = "0"b then                /* initialization, locate first def */
        do;
             segbase = dp;
             call hcs_$status_mins(segbase,i,bitcount,err_code);
             if err_code ^= 0 then goto no_definition;

get_object_info:
             oi.version_number = object_info_version_2;
             call object_info_$brief(segbase,bitcount,oip,err_code);
             if err_code ^= 0 then goto no_definition;
             static_linkbase = oi.linkp;
             dp,
             static_defbase = oi.defp;
             if oi.format.standard = "1"b then dp = addrel(static_defbase,forward);
                                        /* skip definition section header */
             else if ^oi.format.old_format then go to no_definition;
                                                /* don't attempt to process non-standard new format segs */
             initialize = 0;
        end;

nonreentrant_join:
        defbase = static_defbase;                        /* copy so $full doesn't use static variables */
        linkbase = static_linkbase;

loop:
        code = "0"b;                                /* preset error code */

        if dp = null then goto no_definition;
        if delimitor = 0 then                /* end of definition thread */
        do;
no_definition:
             code = "1"b;                        /* EOF */
             return;
        end;

        next_def = addrel(defbase, forward);        /* pointer to next def on list */
        last_def,
        block_ptr = null;                        /* temporarily preset to null */
        dd.offset,
        dd.entrypoint = 0;                        /* temporarily preset to zero */
        i = fixed(class, 3);                /* get class code, convert to fixed bin */
        section = section_table(i);                /* get symbolic section code */

        if section ^= "segn" then                /* its a regular definition */
             dd.offset = fixed(value, 18);        /* compute value of definition */

        if section = "link" then if dd.offset >= fixed(link_begin, 18) then
        do;                                /* this seems to be an entrypoint */
             linkp = addrel(linkbase, dd.offset);        /* pointer to entry sequence */
             if eax(1).op_code = "110010"b then dd.entrypoint = fixed(eax(1).location, 18);
             else if eax(3).op_code = "110010"b then dd.entrypoint = fixed(eax(3).location, 18);
        end;

        if ignore = "1"b then if ^fullsw then
        do;                                /* ignore this definition */
             dp = next_def;                        /* follow thread */
             goto loop;
        end;

        if definition.flags.new_def then if section = "text"
        then if definition.flags.entrypoint = "1"b
        then dd.entrypoint = dd.offset;                /* have standard entrypoint in text */

        if definition.flags.new_def then acc_ptr = addrel(defbase, string_ptr);        /* new definition format */
        else acc_ptr = addrel(dp, 2);                /* pointer to symbol */

        if ret_acc then ret_acc_ptr = acc_ptr;
        else do;
             lng = fixed(acc.lng, 9);                /* length of ACC string */
             if fullsw then symbol = substr(acc_string, 2, lng);
             else ssymbol = substr(acc_string, 2, lng);        /* get string, convert to fixed format */
        end;

        if fullsw then do;
             symbol_lng = lng;                /* fill in actual symbol length */
             n_args = 0;                        /* initialize */
             descr_ptr = null;
             if definition.flags.new_def then do;                /* there is more info */
                addr(dd.flags)->based_bit = substr(addr(definition.flags)->based_bit,1,15);
                if definition.flags.arg_count then do;
                     n_args = fixed(nargs,18);
                     descr_ptr = addr(definition.descriptor(1));
                end;
             end;
             else addr(dd.flags)->based_bit = "0"b;
        end;

        if definition.flags.new_def = "0"b then return;        /* that's all for old def format */

        last_def = addrel(defbase, backward);        /* pointer to previous def on list */
        block_ptr = addrel(defbase, defblock_ptr);        /* pointer to related block */


        return;

init:        entry(segment_pointer, segment_bitcount);

declare        segment_pointer pointer, segment_bitcount fixed bin(24);

        segbase = segment_pointer;
        bitcount = segment_bitcount;
        initialize = 1;

        return;


decode_cref: entry(definition_ptr, structure_ptr, code, link_ptr);

declare        link_ptr pointer;

        ret_acc = "1"b;                        /* Set flag saying return pointer to ACC string */
        fullsw = "0"b;                        /* want smaller amount of information */
        argp = structure_ptr;                /* Copy args */
        dp = definition_ptr;
        linkp = link_ptr;
        if linkp ^= null
        then do;
             static_defbase = dp;
             static_linkbase = linkp;
        end;

        go to nonreentrant_join;


full:        entry(definition_ptr, structure_ptr, oi_ptr, code);

        fullsw = "1"b;

        /* initialize oip each time; object info is passed each time to avoid
           using internal static, thus enabling this procedure to be called
           concurrently for 2 different sets of definitions */

        oip = oi_ptr;
        argp = structure_ptr;
        dp = definition_ptr;                        /* must initialize at this entry */
        if oi.version_number = 2 then do;        /* use new structure */
             linkbase = oi.linkp;
             defbase = oi.defp;
             if oi.format.standard then do;
                if dp = defbase                /* first def is dummy */
                     then dp = addrel(defbase,forward);  /* skip over it */
             end;
             else if ^oi.format.old_format then go to no_definition;        /* unrecognizable format */
        end;
        else do;                                /* use old structure */
             linkbase = old_oi.linkp;
             defbase = old_oi.defp;
             if old_oi.format.standard then do;
                if dp = defbase                /* first def is dummy */
                     then dp = addrel(defbase,forward);  /* skip over it */
             end;
             else if ^old_oi.format.old_format then go to no_definition;        /* unrecognizable format */
        end;

        go to loop;                        /* have done all necessary initialization */

end        decode_definition_;




                    display_component_name.pl1      11/04/82  1947.9rew 11/04/82  1625.4       62316



/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   * Copyright (c) 1972 by Massachusetts Institute of        *
   * Technology and Honeywell Information Systems, Inc.      *
   *                                                         *
   *********************************************************** */


/* Modified 5/28/75 by M. Weaver to convert to v2pl1 and remove search of non-text sections */
/* Modified 761227 by PG to remove ibm entry & call expand_pathname_ */
/* Modified October 1979 by Larry Johnson to take octal segment numbers */

display_component_name: dcn:
     procedure options (variable);

/* entries */

declare  com_err_ external entry options (variable);
declare  cu_$arg_count external entry () returns (fixed bin);
declare  cu_$arg_ptr external entry (fixed bin, pointer, fixed bin, fixed bin (35));
declare  get_bound_seg_info_ ext entry (pointer, fixed bin (24), pointer, pointer, pointer, fixed bin (35));
declare  expand_pathname_ entry (char (*), char (*), char (*), fixed bin (35));
declare  cv_oct_check_ external entry (char (*), fixed bin (35)) returns (fixed bin);
declare  hcs_$initiate_count entry (char (*), char (*), char (*), fixed bin (24),
         fixed bin, pointer, fixed bin (35));
declare  hcs_$terminate_noname ext entry (pointer, fixed bin (35));
declare  ioa_ external entry options (variable);
declare  hcs_$status_mins entry (ptr, fixed bin (2), fixed bin (24), fixed bin (35));
dcl  hcs_$fs_get_path_name entry (ptr, char (*), fixed bin, char (*), fixed bin (35));

/* external static */

dcl  error_table_$dirseg ext fixed bin (35);
dcl  error_table_$bad_segment ext fixed bin (35);
dcl  error_table_$not_bound ext fixed bin (35);

/* automatic */

declare (i, j, nargs, nopts, arg_lng) fixed bin;
declare (k, l, value, lng) fixed bin (18);
dcl  wordcount fixed bin (18);
declare  bitcount fixed bin (24);
declare  code fixed bin (35);
declare (p, argp, objp, bmp, sblkp) pointer;
declare  dirname char (168);
declare  objname char (32);
dcl  segno_given bit (1);
dcl  segno fixed bin;
dcl  seg_type fixed bin (2);
dcl  argno fixed bin;
dcl  msg char (16) var;

declare 1 oi aligned like object_info;

declare 1 comp (16) aligned,
        2 c_offset fixed bin,
        2 found fixed bin;

/* builtins */

declare (addr, addrel, binary, divide, null) builtin;

/* internal static */

declare  errname char (22) internal static options (constant) initial ("display_component_name");

/* based */

declare  var_string char (lng) based (p);
declare  argname char (arg_lng) unaligned based (argp);

/* include files */

%include bind_map;
%include object_info;

/* program */

        nopts = 0;
        nargs = cu_$arg_count ();
        segno_given = "0"b;
        argno = 1;

        call cu_$arg_ptr (argno, argp, arg_lng, code);
        if code ^= 0 then do;
usage:             call com_err_ (code, errname, "Usage: ^a path offset1 ..... offset16", errname);
             return;
        end;
        segno = cv_oct_check_ (argname, code);                /* check for octal segment number */
        if code = 0 then segno_given = "1"b;
        else if argname = "-name" | argname = "-nm" then do;
             argno = argno + 1;
             call cu_$arg_ptr (argno, argp, arg_lng, code);
             if code ^= 0 then go to usage;
             else go to path;
        end;
        else do;
path:             call expand_pathname_ (argname, dirname, objname, code);
             if code ^= 0 then do;
                call com_err_ (code, errname, "^a", argname);
                return;
             end;
        end;

        argno = argno + 1;
        if argno > nargs then go to usage;

        do while (argno <= nargs);
             call cu_$arg_ptr (argno, argp, arg_lng, code);
             nopts = nopts + 1;
             if nopts > hbound (comp, 1) then do;
                call com_err_ (0, errname, "Too many offsets specified.");
                return;
             end;
             comp (nopts).c_offset = cv_oct_check_ (argname, code);
             if code ^= 0 then do;
                call com_err_ (0, errname, "Invalid octal number: ^a", argname);
                return;
             end;
             comp (nopts).found = 0;
             argno = argno + 1;
        end;

        if segno_given then do;
             objp = baseptr (segno);
             call hcs_$fs_get_path_name (objp, dirname, (0), objname, code);
             if code ^= 0 then do;
                call com_err_ (code, errname, "Unable to get pathname for segment ^o.", segno);
                return;
             end;
             call hcs_$status_mins (objp, seg_type, bitcount, code);
             if code ^= 0 then do;
segerr:                call com_err_ (code, errname, "^a^[>^]^a", dirname, (dirname ^= ">"), objname);
                return;
             end;
             if seg_type = 2 then do;
                code = error_table_$dirseg;
                go to segerr;
             end;
        end;
        else do;
             call hcs_$initiate_count (dirname, objname, "", bitcount, 0, objp, code);
             if objp = null () then do;
                call com_err_ (code, errname, "^a^[>^]^a", dirname, (dirname ^= ">"), objname);
                return;
             end;
        end;

        wordcount = divide (bitcount + 35, 36, 18, 0);
        oi.version_number = object_info_version_2;
        call get_bound_seg_info_ (objp, bitcount, addr (oi), bmp, sblkp, code);
                                                /* get ptrs to bindmap and symbol section */
        if code ^= 0 then do;                        /* can't find bindmap */
             if code = error_table_$bad_segment then do;
                msg = "non object";
                go to special_scan;
             end;
             if code = error_table_$not_bound then do;
                msg = "not bound";
                go to special_scan;
             end;
             call com_err_ (code, errname, "^a>^a", dirname, objname);
             goto return_noline;
        end;

        do i = 1 to nopts;
             value = comp (i).c_offset;                /* get desired offset */
             do j = 1 to n_components;
                k = binary (component (j).text_start, 18);
                l = binary (component (j).text_lng, 18);
                if value >= k & value < k+l then do;
                     p = addrel (sblkp, component (j).name_ptr);
                     lng = binary (component (j).name_lng, 18);
                     call ioa_ ("^6o^-^a|^o", value, var_string, value-k);
                     comp (i).found = 1;
                     goto next;
                end;
             end;
             if value < wordcount then do;
                call ioa_ ("^6o^-^a|^o (not in text)", value, objname, value);
                comp (i).found = 1;
             end;
next:
        end;

check_errors:
        do i = 1 to nopts;                                /* rescan option table */
             if comp (i).found = 0 then do;
                if comp (i).c_offset <= wordcount
                then call com_err_ (0, errname, "Offset ^o not in text section^[ of ^a^[>^]^a^;^3s^]", comp (i).c_offset,
                     segno_given, dirname, (dirname ^= ">"), objname);
                else call com_err_ (0, errname, "Offset ^o out of bounds^[ in ^a^[>^]^a^;^3s^]", comp (i).c_offset,
                     segno_given, dirname, (dirname ^= ">"), objname);
             end;
        end;
return_noline:
        if objp ^= null then if ^segno_given then call hcs_$terminate_noname (objp, code);
        return;

special_scan:
        do i = 1 to nopts;
             if comp (i).c_offset < wordcount then do;
                call ioa_ ("^6o^-^a|^o (^a)", comp (i).c_offset, objname, comp (i).c_offset, msg);
                comp (i).found = 1;
             end;
        end;
        go to check_errors;

     end display_component_name;




                    form_bind_map_.pl1              11/04/82  1947.9rew 11/04/82  1608.6      128079



/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   * Copyright (c) 1972 by Massachusetts Institute of        *
   * Technology and Honeywell Information Systems, Inc.      *
   *                                                         *
   *********************************************************** */


/* Form Bind Map - subroutine to produce a printable bind_map segment from the
   information stored in a bound object's symbol table.
Designed and Initially coded by Michael J. Spier, May 3, 1971        */
/* modified 5/26/75 by M. Weaver  to use version 2 object_info_ structure */
/* modified 1/22/81 by E. N. Kittlitz for page_offset option */

form_bind_map_:        procedure(arg_pointer, code);

declare        arg_pointer pointer, code fixed bin(35);


        /* DECLARATION OF EXTERNAL ENTRIES */

declare        date_time_ external entry(fixed bin(71), char(*) aligned);
declare        get_bound_seg_info_ entry(ptr, fixed bin(24), ptr, ptr, ptr, fixed bin(35));
declare        (error_table_$oldobj, error_table_$bad_segment) external fixed bin(35);
declare        hcs_$fs_get_path_name external entry(ptr, char(*) aligned, fixed bin,
        char(*) aligned, fixed bin(35));
declare        hcs_$status_mins entry (ptr, fixed bin(2), fixed bin(24), fixed bin(35));
declare        ioa_$rs external entry options(variable);

        /* DECLARATION OF AUTOMATIC STORAGE VARIABLES */

declare        (j,k,lng,count,bfx,n_extra_bfs) fixed bin;
declare   i fixed bin (21);
declare   (tx_lng,tx_pg1,tx_pg2,tx_start) fixed bin (18);
declare        extra_bfs (20) fixed bin;
declare        trel fixed bin(18);
declare        bitcount fixed bin(24);
declare        (bm_lng,l1) fixed bin;
declare        (p,bmp,sblkp,mapp,listp,argp) pointer;
declare        wst char(256) aligned;
declare        (objname char(32), dirname char(168)) aligned;
declare        address char(12) aligned;
declare        bf_name char(32) aligned;
declare        short_date char(15) aligned;
declare        date char(24) aligned;
declare        maker char(40) aligned;
declare        (nl char(1), bnl char(2)) aligned;
declare        print bit(1) aligned;

declare        1 dd aligned,
        2 next_def pointer,
        2 last_def pointer,
        2 block_ptr pointer,
        2 section char(4) aligned,
        2 offset fixed bin,
        2 entrypoint fixed bin,
        2 defname char(32) aligned;

        /* DECLARATION OF BUILTIN FUNCTIONS */

declare        (addr,addrel,divide,fixed,index,rel,substr,unspec) builtin;

        /* DECLARATION OF BASED STRUCTURES */

declare        1 x aligned based(argp),
        2 objp pointer,                        /* pointer to base of object segment */
        2 list_ptr pointer,                        /* pointer to base of listing segment */
        2 list_bc fixed bin(21),                /* list segment bitcount */
        2 flags aligned,                        /* option indicators */
          3 pad bit(33) unaligned,                /* this field used to be fixed bin */
          3 page_offset bit (1) unaligned,        /* "1"b->show text page number */
          3 no_header bit(1) unaligned,        /* "1"b->do not print header */
          3 long bit(1) unaligned,                /* 1 -> long option; 0 -> brief option */
        2 nopts fixed bin,                        /* size of following option array */
        2 op(1000) aligned,
          3 opt_name char(32) aligned,        /* name of option component */
          3 opt_offset fixed bin,                /* desired option offset */
          3 name_sw bit(1) unaligned,                /* 1 -> option name; 0 -> offset */
          3 found bit(1) unaligned;                /* 1 -> option found; 0 -> not found */

declare        1 link_header aligned based(linkp),
        2 dum(3) pointer,
        2 link_begin bit(18) unaligned;

declare        1 map aligned based(mapp),
        2 decl_vers fixed bin,
        2 size fixed bin,
        2 entry(1000) aligned,
          3 name_offset bit(18) unaligned,
          3 name_length bit(18) unaligned,
          3 uid fixed bin,
          3 dtm fixed bin(71);

declare        1 omap(1000) aligned based(mapp),
        2 inp_ptr bit(18) unaligned,
        2 inp_lng bit(18) unaligned;

declare        string char(1000) aligned based(oi.symbp);
declare        bind_map char(10000) based(listp);


declare        1 oi aligned like object_info;

%include object_info;

% include symbol_block;

        /* Declaration of obsolete symbol block temporarily
           generated by the binder                        */

declare        1 symblk_head aligned based(sblkp),

        2 block_id char(8) aligned,                /* identifier of this symbol block */
        2 dcl_version fixed bin,                /* version number of this structure = 1 */
        2 comp_version_number fixed bin,        /* generator's version number */
        2 comp_creation_time fixed bin(71),        /* compiler creation time (compatible with old symb table) */
        2 object_creation_time fixed bin(71),        /* clock reading (compatible with old symb table) */
        2 comp_id char(8) aligned,                /* identifier of object generator */
        2 comp_version_name,
          3 name_ptr bit(18) unaligned,        /* pointer to string */
          3 name_lng bit(18) unaligned,        /* length of string */
        2 userid,
          3 id_ptr bit(18) unaligned,                /* pointer to userid string */
          3 id_lng bit(18) unaligned,                /* lng of userid string */
        2 comment,
          3 com_ptr bit(18) unaligned,        /* pointer to comment string */
          3 com_lng bit(18) unaligned,        /* length of comment string */
        2 source_map_ptr bit(18) unaligned,        /* pointer to array of sourcefile specifiers */
        2 source_map_size bit(18) unaligned,        /* dimension of source map */
        2 block_ptr bit(18) unaligned,        /* pointer to beginning of symbol block */
        2 section_base bit(18) unaligned,        /* backpointer to symbol section base */
        2 block_size bit(18) unaligned,        /* size of symbol block (incl header) */
        2 next_block bit(18) unaligned,        /* thread to next symbol block */
        2 rel_text bit(18) unaligned,                /* pointer to text relocation bits */
        2 rel_link bit(18) unaligned,                /* pointer to linkage relocation bits */
        2 rel_symbol bit(18) unaligned,        /* pointer to symbol relocation bits */
        2 mini_truncate bit(18) unaligned,        /* minimum truncate value */
        2 maxi_truncate bit(18) unaligned;        /* maximum truncate value */


% include bind_map;


        argp = arg_pointer;                        /* copy for efficiency */
        listp = list_ptr;                        /* ... */
        bm_lng = divide(list_bc,9,21,0);        /* convert bitcount into character count */
        code = 0;                                /* preset error code */
        count = 0;                        /* preset count to see how many opts were processed */
        bfx, n_extra_bfs = 0;        /* preset counts for bindfile names */
        unspec(nl) = "000001010"b;
        unspec(bnl) = "000001111000001010"b;

        call hcs_$fs_get_path_name(objp,dirname,lng,objname,code);
        if code ^= 0 then goto return_noline;
        call hcs_$status_mins(objp,(0),bitcount,code);
        if code ^= 0 then goto return_noline;

        oi.version_number = object_info_version_2;
        call get_bound_seg_info_(objp, bitcount, addr(oi), bmp, sblkp, code);
        if code ^= 0 then goto return_noline;

        if bindmap.dcl_version = 1 then bf_name = " ";        /* no bindfile info */
        else do;                /* extract name for comparisons */
             p = addrel(sblkp, bindmap.bf_name_ptr);
             lng =fixed(bindmap.bf_name_lng, 18);
             bf_name = substr(p -> string, 1, lng);
        end;
print_bind_map:
        if no_header then go to print_components;
        if nopts > 0 then goto print_header;
        maker = " ";
        if oi.userid ^= " " then
        do;
             substr(maker,1,5) = ", by ";
             substr(maker,6,32) = oi.userid;
        end;
        call date_time_(oi.compile_time, date);
        call ioa_$rs("^2/Bindmap for ^a>^a^/Created on ^a^a",wst,l1,dirname,objname,date,maker);
        substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
        bm_lng = bm_lng + l1;                /* update length */
        i = fixed(oi.cvers.offset, 18)*4 +1;
        lng = fixed(oi.cvers.length, 18);
        if ^oi.format.separate_static
        then call ioa_$rs("using ^a",wst,l1,substr(string,i,lng));
        else call ioa_$rs("using ^a^/with separate static",wst,l1,substr(string,i,lng));
        substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
        bm_lng = bm_lng + l1;                /* update length */
        if oi.source_map ^= 0 then
        do;                        /* there is a source map */
             mapp = addrel(sblkp, oi.source_map);
             if oi.format.old_format = "0"b
             then j = map.size;
             else j = fixed(source_map_size, 18);
             do i = 1 to j;                /* print out source map */
                if oi.format.old_format = "0"b then
                do;
                     p = addrel(sblkp,map.entry(i).name_offset);
                     lng = fixed(map.entry(i).name_length, 18);
                end;
                else do;
                     p = addrel(sblkp, omap(i).inp_ptr);
                     lng = fixed(omap(i).inp_lng, 18);
                end;
                call ioa_$rs("^a",wst,l1,substr(p->string,1,lng));
                substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
                bm_lng = bm_lng + l1;                /* update length */
             end;
        end;


        call ioa_$rs("^/^8xObject    Text    Defs    Link    Symb  Static",wst,l1);
        substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
        bm_lng = bm_lng + l1;                /* update length */
        trel = fixed(rel(oi.textp), 18);                 /* Find offset of text (eg, for component of archive) */
        call ioa_$rs("Start   ^6o  ^6o  ^6o  ^6o  ^6o  ^6o",wst,l1,
                     fixed(rel(oi.textp), 18) - trel, fixed(rel(oi.textp), 18) - trel, fixed(rel(oi.defp), 18) - trel,
                     fixed(rel(oi.linkp), 18) - trel, fixed(rel(oi.symbp), 18) - trel, fixed(rel(oi.statp), 18) - trel);

        substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
        bm_lng = bm_lng + l1;                /* update length */
        call ioa_$rs("Length  ^6o  ^6o  ^6o  ^6o  ^6o  ^6o",wst,l1,divide(bitcount,36,18,0),oi.tlng,
                     oi.dlng, oi.llng, oi.slng, oi.ilng);
        substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
        bm_lng = bm_lng + l1;                /* update length */

print_header:
        call ioa_$rs("^2/Component^28x^[^3xText^2x^;Text^]^8xInt-Stat^7xSymbol^[^9xDate^6xSource^]",wst,l1,page_offset,long);
        substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
        bm_lng = bm_lng + l1;                /* update length */
        call ioa_$rs("^33xStart Length^[ Page^]  Start Length  Start Length^[^3xCompiled^4xLanguage^]^/",wst,l1,page_offset,long);
        substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
        bm_lng = bm_lng + l1;                /* update length */
print_components:
        do i = 1 to n_components;
             print = "1"b;                        /* assume line will be printed */
             p = addrel(sblkp, component(i).name_ptr);
             lng = fixed(component(i).name_lng, 18);
             tx_start = fixed(component(i).text_start, 18);
             tx_lng = fixed(component(i).text_lng, 18);
             tx_pg1 = divide (tx_start, 1024, 18, 0);
             tx_pg2 = divide (tx_start + tx_lng - 1, 1024, 18, 0);
             if nopts > 0 then                /* there are options */
             do;
                print = "0"b;                /* assume line won't be printed */
                do j = 1 to nopts;                /* lookup option table */
                     if op(j).name_sw = "1"b then
                     do;
                        if op(j).opt_name = substr(p->string,1,lng) then
                        do;
display:
                             op(j).found = "1"b;
                             k = k + 1;
                             print = "1"b;
                             count = count + 1;
                        end;
                     end;
                     else do;
                        if op(j).opt_offset >= tx_start then
                        if op(j).opt_offset < tx_start + tx_lng then goto display;
                     end;
                end;
             end;

             if print = "0"b then goto skip;
             j = fixed(component(i).symb_start,18);        /* get symbol table offset */
             mapp = addrel(oi.symbp, j);                /* pointer to component's symbol table */
             call date_time_(mapp->sb.obj_creation_time, date);
             substr(short_date,1,8) = substr(date,1,8);
             substr(short_date,9,7) = substr(date,10,7);
             call ioa_$rs("^32a^6o ^6o^[^x^3o^[+^; ^]^;^2s^] ^6o ^6o ^6o ^6o^[ ^15a ^a^;^2s^]",wst, l1, 
                     substr(p->string,1,lng),
                     tx_start,tx_lng,
                     page_offset, tx_pg1, (tx_pg2 ^= tx_pg1),
                     fixed(component(i).stat_start,18),fixed(component(i).stat_lng,18),
                     j,fixed(component(i).symb_lng,18),
                     long,short_date,component(i).comp_name);
             substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
             bm_lng = bm_lng + l1;                /* update length */
skip:
        end;

return:
        if nopts > 0 & count < nopts then do;        /* had errors */
             if count = 0 then do;                /* had only errors */
                listp = list_ptr;                /* re-initialize; don't print any header */
                bm_lng = divide(list_bc, 9, 21, 0);
             end;
             else do;                        /* separate map fram errors */
                substr(bind_map, bm_lng+1, 1) = nl;
                bm_lng = bm_lng + 1;
             end;
             do i = 1 to nopts;                /* rescan option table */
                if op(i).found = "0"b then
                do;
                if op(i).name_sw then do;
                     if index(op(i).opt_name, ".bind") > 0 then do;        /* special case bindfiles */
                        if op(i).opt_name = bf_name then bfx = i;        /* real name was requested */
                        else do;                        /* just guessing; print error */
                             n_extra_bfs = n_extra_bfs + 1;
                             extra_bfs(n_extra_bfs) = i;
                        end;
                        l1 = 0;                /* nothing to add to output yet */
                     end;
                     else call ioa_$rs("Component ^a not found",wst,l1,op(i).opt_name);
                end;
                     else call ioa_$rs("Offset ^o out of bounds",wst,l1,op(i).opt_offset);
                     substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);        /* copy into bind_map area */
                     bm_lng = bm_lng + l1;                /* update length */
                end;
             end;
        end;

        if bf_name = " " then do;                /* no bindfile info exists */
             if n_extra_bfs = 0 then go to return_noline;        /* no bf info requested */
             else do;
                call ioa_$rs("^/There is no bindfile information", wst, l1);
                substr(bind_map,bm_lng+1,l1) = substr(wst,1,l1);
                bm_lng = bm_lng + l1;
             end;
        end;

        else do;                        /* there is bindfile info */
                     if (nopts > 0) & (bfx = 0) & (n_extra_bfs = 0) then go to return_noline;
                                        /* but none was requested */
             if (nopts = 0) | (bfx > 0) then do;        /* print bindfile info */
                if ^no_header then do;
                     call ioa_$rs("^2/Bindfile^[^28xDate Updated^8xDate Modified^]",wst,l1,long);
                     substr(bind_map, bm_lng+1, l1) = substr(wst,1,l1);
                     bm_lng = bm_lng + l1;
                end;
                call ioa_$rs("^/^34a^[^16a^4x^16a^;^2s^]",wst,l1,
                bf_name, long, bindmap.bf_date_up, bindmap.bf_date_mod);
                substr(bind_map, bm_lng+1, l1) = substr(wst, 1, l1);
                bm_lng = bm_lng + l1;
             end;
             if n_extra_bfs > 0 then do;
                substr(bind_map, bm_lng+1,1) = nl;        /* separate errors */
                bm_lng = bm_lng + 1;
                do i = 1 to n_extra_bfs;
                     call ioa_$rs("^a not found",wst,l1,op(extra_bfs(i)).opt_name);
                     substr(bind_map, bm_lng+1, l1) = substr(wst, 1, l1);
                     bm_lng = bm_lng + l1;
                end;
             end;
        end;

return_noline:
        if (nopts > 0) & (count = 0) & (bfx = 0) then no_header = "1"b;
                                        /* had only errors; don't want title */
        list_bc = bm_lng * 9;                /* return new listing bitcount */

end        form_bind_map_;



                    form_link_info_.pl1             11/20/86  1409.7r w 11/20/86  1145.0      172872



/****^  ***********************************************************
        *                                                         *
        * Copyright, (C) Honeywell Information Systems Inc., 1982 *
        *                                                         *
        * Copyright (c) 1972 by Massachusetts Institute of        *
        * Technology and Honeywell Information Systems, Inc.      *
        *                                                         *
        *********************************************************** */



/****^  HISTORY COMMENTS:
  1) change(86-09-05,JMAthane), approve(86-09-05,MCR7525),
     audit(86-09-11,Martinson), install(86-11-12,MR12.0-1212):
     Fixed to stop link analysis before reference trap structure.  Now displays
     first reference trap info in a convenient format.  Now uses
     object_link_dcls.incl.pl1.
                                                   END HISTORY COMMENTS */

/* Form Link Info - subroutine to format object segment linkage information into printable file.

   Designed and Initially coded by Michael J. Spier, March 17, 1971   */
/* modified 6/75 by M. Weaver to use version 2 object_info_ structure */
/* modified 1/77 by S. Webber to sort entry names, use get_temp_segments_, and handle non-standard definitions */
/* modified 8/77 by M. Weaver to check perprocess static attribute */
/* modified 3/82 by M. Weaver to handle archive components */
/* Modified: 28 July 1982 by G. Palter to fix bug which caused a null component name to appear when called with the old
   format structure (eg: by create_data_segment_) */
/* Changed to use interpret_link_info.incl.pl1 05/16/83 S. Herbst */
/* modified 1/84 by M. Weaver to detect loop in definitions */
/* modified 4/84 by M. Weaver to copy dates so they are even-word aligned */
/* Modified JMAthane 11/24/84 to stop link analysis before first reference trap structure if any,
display first reference trap info if any, and use object_link_dcls.incl.pl1   */

/* format: style3,^indnoniterdo */

#endif

static int form_link_info_ (struct form_link_info_args * P_arg_ptr)
  {
#if 0
form_link_info_:
     procedure (P_arg_ptr, code);

dcl        P_arg_ptr                ptr,
        code                fixed bin (35);


/* DECLARATION OF EXTERNAL ENTRIES */

dcl        date_time_        entry (fixed bin (71), char (*) aligned);
dcl        (get_temp_segment_, release_temp_segment_)
                        entry (char (*), ptr, fixed bin (35));
dcl        decode_definition_        entry (ptr, ptr) returns (bit (1) aligned);
dcl        decode_definition_$init
                        entry (ptr, fixed bin (24));
dcl        hcs_$fs_get_path_name
                        entry (ptr, char (*), fixed bin, char (*), fixed bin (35));
dcl        hcs_$status_mins        entry (ptr, fixed bin, fixed bin (24), fixed bin (35));
dcl        interpret_link_$tptr
                        entry (ptr, ptr, ptr, fixed bin (35));
dcl        (
        ioa_$rs,
        ioa_$rsnnl
        )                entry options (variable);
dcl        object_info_$long        entry (ptr, fixed bin (24), ptr, fixed bin (35));
dcl        pathname_$component entry (char (*), char (*), char (*)) returns (char (194));

dcl        error_table_$bad_segment
                        external fixed bin (35);

/* DECLARATION OF AUTOMATIC STORAGE VARIABLES */

dcl        (junk, i, j, lng, lslng, l1, link_offset)
                        fixed bin;
dcl        orel                fixed bin (18);
dcl        offset21                fixed bin (21);
dcl        bitcount                fixed bin (24);
dcl        aligned_bin_date        fixed bin (71);
dcl        (counter, previous) fixed bin;
dcl        total_def_counter        fixed bin;
dcl        (p, argp, objp, dp, lp, listp, first_dp, arg_ptr)
                        ptr;
dcl        (sblkp, smp)        ptr;
dcl        attributes        char (128) var;
dcl        n                fixed bin;
dcl        severity                fixed bin;
dcl        alm_or_bcpl        fixed bin;
dcl        first_link        bit (1) aligned;
dcl        get_out                bit (1) aligned;
dcl        new_defblock        fixed bin (1);
dcl        sortp                ptr;
dcl        objname                char (32);
dcl        dirname                char (168);
dcl        wst                char (256) aligned;
dcl        date                char (24) aligned;
dcl        user                char (40) aligned;
dcl        address                char (12) aligned;
dcl        entry                char (20) aligned;
dcl        link_end                fixed bin (18);
dcl        fr_trap_ptr        ptr;

dcl        1 dd                aligned,
          2 next_def        ptr,
          2 last_def        ptr,
          2 block_ptr        ptr,
          2 section        char (4) aligned,
          2 offset        fixed bin,
          2 entrypoint        fixed bin,
          2 defname        char (32) aligned;
#endif

//--- dcl        1 auto_form_link_info_args
//---                         aligned like form_link_info_args;

#if 0

/* DECLARATION OF BUILTIN FUNCTIONS */

dcl        (addr, addrel, bin, char, divide, index, rel, substr, length, null, rtrim, unspec)
                        builtin;

/* DECLARATIONS OF STATIC STORAGE */

dcl        FT2                bit (6) aligned static options (constant) init ("100110"b);
// XXX CAC replaced explicit nl
dcl        newline                char (1) static init ("\n ");

/* DECLARATION OF BASED STRUCTURES */

dcl        1 sort                (0:1) aligned based (sortp),
          2 thread        fixed bin,
          2 def_info        char (32),
          2 section        char (4),
          2 offset        fixed bin,
          2 entrypoint        fixed bin;

dcl        1 lsort                (0:1) aligned based (sortp),
          2 thread        fixed bin,
          2 link_address        char (12),
          2 link_info        aligned like interpret_link_info;

dcl        1 args                aligned based (argp),
          2 obj_ptr        ptr,
          2 list_ptr        ptr,
          2 list_bc        fixed bin,
          2 hd_sw                bit (1) unaligned,
          2 ln_sw                bit (1) unaligned,
          2 et_sw                bit (1) unaligned,
          2 lk_sw                bit (1) unaligned,
          2 lg_sw                bit (1) unaligned;

dcl        listing                char (131072) based (listp);
dcl        string                char (131071) based (p);
dcl        based_fixed        fixed bin based;

dcl        cleanup                condition;

declare        1 oi                aligned like object_info;

/* \014 */
        arg_ptr, argp = P_arg_ptr;
        if form_link_info_args.version ^= form_link_info_args_version_1
        then do;                                        /* old input structure; copy first */
             unspec (auto_form_link_info_args) = "0"b;
             arg_ptr = addr (auto_form_link_info_args);
             form_link_info_args.version = form_link_info_args_version_1;
             form_link_info_args.list_bc = args.list_bc;
             form_link_info_args.list_ptr = args.list_ptr;
             form_link_info_args.obj_ptr = args.obj_ptr;
             form_link_info_args.flags.hd_sw = args.hd_sw;
             form_link_info_args.flags.ln_sw = args.ln_sw;
             form_link_info_args.flags.et_sw = args.et_sw;
             form_link_info_args.flags.lk_sw = args.lk_sw;
             form_link_info_args.flags.lg_sw = args.lg_sw;
             form_link_info_args.component_name = "";
        end;

        objp = form_link_info_args.obj_ptr;
        listp = form_link_info_args.list_ptr;
        lslng = divide (form_link_info_args.list_bc, 9, 17, 0);
                                                /* get size of list segment in characters */
        code = 0;                                        /* preset error code */
        sortp = null;                                /* don't  call release_temp_segments_ unnecessarily */

        call hcs_$fs_get_path_name (objp, dirname, lng, objname, code);
        if code ^= 0
        then return;
        if form_link_info_args.flags.bc_sw
        then bitcount = form_link_info_args.bit_count;
        else do;
             call hcs_$status_mins (objp, i, bitcount, code);
             if code ^= 0
             then return;
        end;

        oi.version_number = object_info_version_2;
        call object_info_$long (objp, bitcount, addr (oi), code);
        if code ^= 0
        then return;

        if ^oi.format.standard
        then if ^oi.format.old_format
             then do;

                code = error_table_$bad_segment;
                return;

             end;

        link_offset = bin (oi.linkp -> linkage_header.begin_links, 18);

        if form_link_info_args.flags.lk_sw | form_link_info_args.flags.et_sw
        then do;
             sortp = null;
             on cleanup call release_temp_segment_ ("form_link_info_", sortp, code);
             call get_temp_segment_ ("form_link_info_", sortp, code);
        end;

        if form_link_info_args.flags.hd_sw
        then do;                                        /* print header information */
             if oi.format.old_format & ((oi.compiler = "alm     ") | (oi.compiler = "bcpl    "))
             then alm_or_bcpl = 1;
             else alm_or_bcpl = 0;
             call date_time_ (oi.compile_time, date);        /* convert object creation time */
             user = " ";
             if oi.format.standard
             then do;                                /* standard, we can get user name */
                substr (user, 1, 2) = "by";
                substr (user, 4, 32) = oi.userid;
                i = index (substr (user, 4, 34), " ") + 3;
                substr (user, i, 1) = newline;
             end;
             offset21 = bin (oi.cvers.offset, 18) * 4 + 1 + alm_or_bcpl;
                                                /* offset is from symb|0 */
             lng = bin (oi.cvers.length, 18);
             call ioa_$rs ("^2/Object Segment ^a^/Created on ^a^/^ausing ^a^[^/with separate static^;^]", wst, l1,
                pathname_$component (dirname, objname, (form_link_info_args.component_name)), date, user,
                substr (oi.symbp -> string, offset21, lng), oi.format.separate_static);
             call put_list;

/* Do the following for long-format output */

             if form_link_info_args.flags.lg_sw
             then do;
                severity = 0;
                if ^oi.format.old_format
                then do;
                     sblkp = addrel (oi.symbp, oi.symbp -> sb.area_ptr);
                     if rel (sblkp)
                     then if sblkp -> pl1_symbol_block.identifier = "pl1info"
                        then severity = sblkp -> pl1_symbol_block.greatest_severity;
                end;

                call ioa_$rs ("^/Translator:^-^a", wst, l1, oi.compiler);
                call put_list;

                offset21 = bin (oi.comment.offset, 18) * 4 + 1 + alm_or_bcpl;
                lng = bin (oi.comment.length, 18);
                call ioa_$rs ("Comment:^2-^a", wst, l1, substr (oi.symbp -> string, offset21, lng));
                call put_list;

                if severity > 0
                then do;
                     call ioa_$rs ("Severity:^2-^d", wst, l1, severity);
                     call put_list;
                end;

                if oi.source_map ^= 0
                then do;
                     call ioa_$rs ("Source:", wst, l1);
                     call put_list;
                     smp = addrel (oi.symbp, oi.source_map);
                     do n = 1 to smp -> source_map.number;
                        offset21 = bin (smp -> source_map.map (n).pathname.offset, 18) * 4 + 1 + alm_or_bcpl;
                        lng = bin (smp -> source_map.map (n).pathname.size, 18);
                        unspec (aligned_bin_date) = unspec (smp -> source_map.map (n).dtm);
                                                /* copy date to align properly */
                        call date_time_ (aligned_bin_date, date);
                        call ioa_$rs ("   ^26a  ^a", wst, l1, date, substr (oi.symbp -> string, offset21, lng));
                        call put_list;
                     end;
                end;

                if oi.entry_bound ^= 0
                then do;
                     call ioa_$rs ("Entry Bound^-^d", wst, l1, oi.entry_bound);
                     call put_list;
                end;

                if oi.text_boundary ^= 2
                then do;
                     call ioa_$rs ("Text Boundary^-^d", wst, l1, oi.text_boundary);
                     call put_list;
                end;

                if oi.static_boundary ^= 2
                then do;
                     call ioa_$rs ("Static Boundary^-^d", wst, l1, oi.static_boundary);
                     call put_list;
                end;

                attributes = "";
                if oi.format.old_format
                then attributes = attributes || "old_format, ";
                if oi.format.bound
                then attributes = attributes || "bound, ";
                if oi.format.relocatable
                then attributes = attributes || "relocatable, ";
                if oi.format.procedure
                then attributes = attributes || "procedure, ";
                if oi.format.standard
                then attributes = attributes || "standard, ";
                if oi.format.gate
                then attributes = attributes || "gate, ";
                if oi.format.separate_static
                then attributes = attributes || "separate_static, ";
                if oi.format.perprocess_static
                then attributes = attributes || "perprocess_static, ";
                if oi.format.links_in_text
                then attributes = attributes || "links_in_text, ";
                lng = length (attributes) - 2;
                if lng > 1
                then attributes = substr (attributes, 1, lng);
                call ioa_$rs ("Attributes:^-^a", wst, l1, attributes);
                call put_list;

             end;
        end;
        if form_link_info_args.flags.ln_sw
        then do;                                        /* print length information */
             orel = bin (rel (objp), 18);                /* Find offset of object (eg, for component of archive) */
             call ioa_$rs ("^/^8xObject    Text    Defs    Link    Symb  Static^/Start^9o^8o^8o^8o^8o^8o", wst, l1,
                orel, bin (rel (oi.textp), 18) - orel, bin (rel (oi.defp), 18) - orel,
                bin (rel (oi.linkp), 18) - orel, bin (rel (oi.symbp), 18) - orel, bin (rel (oi.statp), 18) - orel);
             call put_list;
             call ioa_$rs ("Length  ^6o  ^6o  ^6o  ^6o  ^6o  ^6o", wst, l1, divide (bitcount, 36, 17, 0), oi.tlng,
                oi.dlng, oi.llng, oi.slng, oi.ilng);
             call put_list;
        end;

        if form_link_info_args.flags.et_sw
        then do;                                        /* we want defs info */
             first_dp = oi.defp;
             if oi.format.standard
             then first_dp = addrel (oi.defp, first_dp -> definition.forward);
                                                /* skip def section header */
             total_def_counter = 0;                        /* set loop detection counter */
             counter = 0;                                /* reset counter */
             do dp = first_dp repeat addrel (oi.defp, dp -> definition.forward) while (dp -> based_fixed ^= 0);
                if ^dp -> definition.ignore
                then counter = counter + 1;                /* count defs that aren't ignored */
                total_def_counter = total_def_counter + 1;
                if total_def_counter > 8000
                then do;
                     call put_nl;
                     wst = "Cannot display definitions because of a loop.";
                     l1 = length (rtrim (wst));
                     call put_list;
                     call put_nl;
                     call put_nl;
                     go to print_links;                /* continue processing */
                end;
             end;

             call ioa_$rs ("^2/^d Definitions:^/", wst, l1, counter);
             call put_list;

             call decode_definition_$init (objp, bitcount);
             dd.next_def = objp;                        /* initialize definition search */
             new_defblock = 1b;
             get_out = "0"b;

             if decode_definition_ (dd.next_def, addr (dd))
             then goto print_links;
class_3:
             if dd.section = "segn"
             then do while ("1"b);

                     call put_nl;
                     if dd.next_def ^= dd.block_ptr
                     then address = "segnames:";
                     else address = "segname:";

                     wst = char (address, 12) || dd.defname;
                     l1 = length (rtrim (wst));
                     call put_list;
                     call put_nl;

                     if decode_definition_ (dd.next_def, addr (dd))
                     then goto print_links;
                     do while (dd.section = "segn");
                        wst = (12)" " || dd.defname;
                        l1 = length (rtrim (wst));
                        call put_list;
                        call put_nl;
                        if decode_definition_ (dd.next_def, addr (dd))
                        then goto print_links;
                     end;
                     call put_nl;

/* Now we've processed all segnames, do entroes... */

                     sort (0).thread = 0;
                     sort (0).def_info = "";

/* now search and sort within this class 3 list */

                     do j = 1 by 1;
                        sort (j).def_info = dd.defname;
                        sort (j).section = dd.section;
                        sort (j).offset = dd.offset;
                        sort (j).entrypoint = dd.entrypoint;

/* now sort */

                        previous, i = 0;
merge_defs:
                        if sort (j).def_info <= sort (i).def_info
                        then do;
                             sort (previous).thread = j;
                             sort (j).thread = i;
                        end;
                        else if sort (i).thread = 0
                        then do;
                             sort (j).thread = 0;
                             sort (i).thread = j;
                        end;
                        else do;
                             previous = i;
                             i = sort (i).thread;
                             goto merge_defs;
                        end;
                        if decode_definition_ (dd.next_def, addr (dd))
                        then do;
                             get_out = "1"b;
                             goto print_defs;
                        end;
                        if dd.section = "segn"
                        then goto print_defs;
                     end;

/* Now output the defs for this class 3 loop */

print_defs:
                     do i = sort (0).thread repeat sort (i).thread while (i ^= 0);
                        call ioa_$rsnnl ("^a|^o", address, junk, sort (i).section, sort (i).offset);
                        if sort (i).entrypoint ^= 0
                        then entry = "Entrypoint";
                        else entry = "";
                        wst = char (address, 16) || char (sort (i).def_info, 32) || entry;
                        l1 = length (rtrim (wst));
                        call put_list;
                        call put_nl;
                     end;
                     if get_out
                     then goto print_links;

                end;

             else do while ("1"b);                        /* no segname defs; just loop and print */
                     if dd.entrypoint ^= 0
                     then entry = "Entrypoint";
                     else entry = "";
                     call ioa_$rsnnl ("^a|^o", address, junk, dd.section, dd.offset);
                     wst = char (address, 16) || char (dd.defname, 32) || entry;
                     l1 = length (rtrim (wst));
                     call put_list;
                     call put_nl;
                     if decode_definition_ (dd.next_def, addr (dd))
                     then goto print_links;
                end;

        end;


print_links:
        if form_link_info_args.flags.lk_sw
        then do;                                        /* link info wanted */


/* Now print out linkage information */

             if oi.linkp -> first_ref_relp = 0
             then link_end = oi.llng;
             else link_end = bin (oi.linkp -> first_ref_relp, 18);
             counter = 0;                                /* reset link counter */
             do j = link_offset to link_end - 1 by 2;        /* first count number of links */
                p = addrel (oi.linkp, j);                /* ptr to tentative link */
                if p -> object_link.tag = FT2 & p -> object_link.mbz2 = "0"b
                then counter = counter + 1;
             end;
             j = 0;                                /* reset j */
             if counter > 0
             then do;

                lsort (0).thread = 0;                /* initialize sort table */
                lsort (0).segment_name = " ";                /* ... */

                do link_offset = link_offset to link_end - 1 by 2;
                                                /* don't bother if no links */
                     p = addrel (oi.linkp, link_offset);
                     if (p -> object_link.tag = FT2) & (p -> object_link.mbz2 = "0"b)
                     then do;
                        j = j + 1;                /* count the links */
                        if addrel (p, p -> object_link.header_relp) ^= oi.linkp
                        then lsort (j).segment_name = "***invalid link";
                                                /* link does not point to its header */
                        else do;
                             lsort (j).link_info.version = INTERPRET_LINK_INFO_VERSION_1;
                             call interpret_link_$tptr (addr (lsort (j).link_info), p, objp, code);
                             if code ^= 0
                             then do;
                                lsort (j).segment_name = "***invalid link";
                                code = 0;
                             end;
                             else if lsort (j).segment_name = substr (lsort (j).entry_point_name, 2, 32)
                             then if lsort (j).expression = " "
                                then if lsort (j).modifier = " "
                                     then lsort (j).entry_point_name = " ";
                        end;
                        call ioa_$rsnnl ("link|^o", lsort (j).link_address, i, link_offset);
                        previous, i = 0;
merge_in:
                        if lsort (j).segment_name <= lsort (i).segment_name
                        then do;
                             lsort (previous).thread = j;
                             lsort (j).thread = i;
                        end;
                        else if lsort (i).thread = 0
                        then do;
                             lsort (j).thread = 0;
                             lsort (i).thread = j;
                        end;
                        else do;
                             previous = i;
                             i = lsort (i).thread;
                             goto merge_in;
                        end;
                     end;
                end;
                do i = lsort (0).thread repeat lsort (i).thread while (i ^= 0);
                     if first_link ^= "1"b
                     then do;                        /* no header for links */
                        call ioa_$rs ("^2/^d Links:^/", wst, l1, counter);
                        call put_list;
                        first_link = "1"b;
                     end;
                     lp = addr (lsort (i).link_info);        /* ptr to link to be printed */
                     call ioa_$rs ("^12a^a^a^a^a^a", wst, l1, lsort (i).link_address, lsort (i).segment_name,
                        lsort (i).entry_point_name, lsort (i).expression, lsort (i).modifier, lsort (i).trap);
                     call put_list;
                end;
                if oi.linkp -> first_ref_relp ^= 0
                then do;
                     call ioa_$rs ("^2/First reference trap:^/", wst, l1);
                     call put_list;
                     fr_trap_ptr = addrel (oi.linkp, oi.linkp -> first_ref_relp);
                     do j = 1 to fr_trap_ptr -> n_traps;
                        call ioa_$rs ("call ^o, ^[no arg^;arg ^o^]", wst, l1, fr_trap_ptr -> call_relp (j),
                             fr_trap_ptr -> info_relp (j) = 0, fr_trap_ptr -> info_relp (j));
                        call put_list;
                     end;
                end;
             end;
             else do;
                call ioa_$rs ("^2/No Links.", wst, l1);
                call put_list;
             end;

        end;
        call ioa_$rs ("^2/", wst, l1);
        call put_list;

        if P_arg_ptr -> form_link_info_args.version = form_link_info_args_version_1
        then form_link_info_args.list_bc = lslng * 9;
        else args.list_bc = lslng * 9;
        if sortp ^= null
        then call release_temp_segment_ ("form_link_info_", sortp, code);

        return;

put_list:
     proc;                                        /* to place formatted output in caller's listing */

        substr (listing, lslng + 1, l1) = substr (wst, 1, l1);
                                                /* copy string into listing */
        lslng = lslng + l1;

        return;

     end put_list;

put_nl:
     proc;

        substr (listing, lslng + 1, 1) = newline;
        lslng = lslng + 1;
        return;

     end put_nl;                                        /* \014 */

%include form_link_info_args;

%include interpret_link_info;

%include object_link_dcls;

%include definition;

%include object_info;
%include source_map;
%include symbol_block;
%include pl1_symbol_block;

     end form_link_info_;
#endif
    return 0;
  }
#if 0

                    get_bound_seg_info_.pl1         11/04/82  1947.9rew 11/04/82  1624.8       37908



/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   * Copyright (c) 1972 by Massachusetts Institute of        *
   * Technology and Honeywell Information Systems, Inc.      *
   *                                                         *
   *********************************************************** */


get_bound_seg_info_: proc (objp, bitcount, oip, bmp, sblkp, code);

/*
   This procedure looks through the defs of an object segment to find the bindmap.

   Modified: 17 July 1981 by Jeff Ives to use decode_definition_$full which has no memory between calls
*/

        declare (objp, oip, bmp, sblkp) pointer;
        declare bitcount                 fixed bin (24);
        declare code                 fixed bin (35);

        declare (error_table_$not_bound, error_table_$bad_segment, error_table_$oldobj, error_table_$unimplemented_version) ext fixed bin (35);
        declare (addr, addrel, null, substr, unspec) builtin;
        declare eof                 bit aligned;
        declare decode_definition_$full entry (pointer, pointer, pointer, bit aligned);
        declare object_info_$display         external entry (pointer, fixed bin (24), pointer, fixed bin (35));


        declare 1 dd                 aligned,                /* structure filled in by full entry */
                2 next_def         ptr,                /* ptr to next definition in list */
                2 last_def         ptr,                /* ptr to previous definition in list */
                2 block_ptr         ptr,                /* ptr to either defblock or segname block */
                2 section                 char (4) aligned,        /* "text", "link", "symb" or "segn" */
                2 offset                 fixed bin,        /* offset within class (if ^= "segn") */
                2 entrypoint         fixed bin,        /* value of entrypoint in text if ^= 0 */
                2 symbol                 char (256) aligned,/* the symbolic name of the definition */
                2 symbol_lng         fixed bin,        /* the actual length of symbol */
                2 flags,                                /* same flags as in std def */
                  3 a_new_format         bit (1) unaligned, /* def is in new format */
                  3 a_ignore         bit (1) unaligned, /* linker should ignore this def */
                  3 a_entrypoint         bit (1) unaligned, /* def is for entrypoint */
                  3 a_retain         bit (1) unaligned,
                  3 a_arg_count         bit (1) unaligned, /* there is an arg count for entry */
                  3 a_descr_sw         bit (1) unaligned, /* there are valid descriptors for entry */
                  3 unused         bit (12) unaligned,
                2 n_args                 fixed bin,        /* # of args entry expects */
                2 descr_ptr         ptr;                /* ptr to array of rel ptrs to descriptors for entry */

        declare 1 oi                 aligned based (oip) like object_info;

%include object_info;


%include symbol_block;


%include symbol_block_header;

        bmp, sblkp = null ();                        /* initialize */

/* ASSUME THE USER HAS ALREADY SET THE VERSION NUMBER */
        call object_info_$display (objp, bitcount, oip, code); /* get basic object info (for caller too) */
        if code ^= 0 then return;                        /* couldn't find bind map if there was one */

        if oi.version_number = object_info_version_2 then do; /* have new structure */
                if ^oi.format.standard then if ^oi.format.old_format then do;
                             code = error_table_$bad_segment; /* have non-standard new format seg; don't process */
                             return;
                        end;

                unspec (dd) = ""b;                        /* safety first! */
                dd.next_def = oi.defp;                /* initialize definition lookup */

lookup_defs:
                call decode_definition_$full ((dd.next_def), addr (dd), addr (oi), eof);

                if eof then do;                        /* have looked at all the definitions */
                        if oi.compiler = "binder  " then code = error_table_$oldobj; /* bound but has no bindmap */
                        else code = error_table_$not_bound; /* assume anything not generated by binder is not bound */
                        return;
                     end;

                if dd.flags.a_ignore
                then go to lookup_defs;

                if substr (dd.symbol, 1, dd.symbol_lng) = "bind_map" then if dd.section = "symb" then do; /* should have real bindmap */
                             sblkp = addrel (oi.symbp, dd.offset); /* get ptr to binder's symbol block */
                             if oi.format.standard then bmp = addrel (sblkp, sb.area_ptr);
                             else bmp = addrel (sblkp, symblk_head.block_ptr);
                             return;
                        end;

                goto lookup_defs;                        /* get next definition */
             end;
        else code = error_table_$unimplemented_version;

     end get_bound_seg_info_;


                    get_definition_.pl1             11/04/82  1947.9rew 11/04/82  1624.8       16767



/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   *********************************************************** */
/* GET_DEFINITION_ - Get pointer to definition for external symbol.
        coded 9/27/76 by Noel I. Morris                */


/* ******************************************************
   *                                                    *
   *                                                    *
   * Copyright (c) 1972 by Massachusetts Institute of   *
   * Technology and Honeywell Information Systems, Inc. *
   *                                                    *
   *                                                    *
   ****************************************************** */


get_definition_: proc (defsp, seg, sym, defp, code);

dcl  defsp ptr,                                        /* pointer to definitions section */
     seg char (*),                                        /* segment name */
     sym char (*),                                        /* external symbol name */
     defp ptr,                                        /* returned pointer to definition */
     code fixed bin (35);                                /* error code */

dcl  l fixed bin;

dcl 1 segacc aligned,
   (2 l fixed bin (8),
    2 c char (31)) unal;

dcl 1 symacc aligned,
   (2 l fixed bin (8),
    2 c char (31)) unal;

dcl  get_defptr_ entry (ptr, ptr, ptr, ptr, fixed bin (35));

dcl (addr, length, reverse, substr, unspec, verify) builtin;


        l = length (seg) - verify (reverse (seg), " ") + 1;
        substr (unspec (segacc), 1, 36) = "0"b;
        segacc.l = l;
        substr (segacc.c, 1, l) = seg;

        l = length (sym) - verify (reverse (sym), " ") + 1;
        substr (unspec (symacc), 1, 36) = "0"b;
        symacc.l = l;
        substr (symacc.c, 1, l) = sym;

        call get_defptr_ (defsp, addr (segacc), addr (symacc), defp, code);

        return;


     end;


                    interpret_link_.pl1             11/20/86  1409.7rew 11/20/86  1148.9      104103



/****^  ***********************************************************
        *                                                         *
        * Copyright, (C) Honeywell Information Systems Inc., 1982 *
        *                                                         *
        * Copyright (c) 1972 by Massachusetts Institute of        *
        * Technology and Honeywell Information Systems, Inc.      *
        *                                                         *
        *********************************************************** */




/****^  HISTORY COMMENTS:
  1) change(86-06-24,DGHowe), approve(86-06-24,MCR7396), audit(86-11-12,Zwick),
     install(86-11-20,MR12.0-1222):
     add an understanding of heap links
                                                   END HISTORY COMMENTS */


/* Procedure to decode a given link and return to the caller a symbolic and
   directly printable interpretation of it.

   Initially designed and coded by Michael J. Spier, March 2, 1971
   Modified by Richard A. Barnes, October 14, 1974 for interpret_link_$tptr        */
/* Modified 5/26/75 by M. Weaver  to add *static segref code */
/* modified 9/19/75 by M. Weaver to use standard include files */
/* modified 1/30/76 by S. Webber to handle *system links */
/* modified 12/76 by M. Weaver to  be compatible with new stack_header include file */
/* modified 10/77 by R. Barnes to print out more link info */
/* modified 8/82 BIM for better include files, entrypoint for checker */
/* Changed to use interpret_link_info.incl.pl1 05/12/83 S. Herbst */

interpret_link_: procedure (A_structure_ptr, A_link_ptr, A_code);

/* PARAMETERS */

declare (A_structure_ptr, A_link_ptr, text_ptr) pointer, A_code fixed bin (35);


/* DECLARATION OF EXTERNAL SYMBOLS */

declare  error_table_$no_defs fixed bin (35) external;
declare  error_table_$bad_link_type fixed bin (35) external;
declare  error_table_$no_linkage fixed bin (35) external;

/* ENTRIES */

declare  ioa_$rsnnl external entry options (variable);

/* DECLARATION OF INTERNAL STATIC VARIABLES */


declare  symbolic_modifier (0:63) char (4) aligned internal static
         initial ("    ", ",au ", ",qu ", ",du ", ",ic ", ",al ", ",ql ", ",dl ",
         ",x0 ", ",x1 ", ",x2 ", ",x3 ", ",x4 ", ",x5 ", ",x6 ", ",x7 ",
         ",*  ", ",au*", ",qu*", ",du*", ",ic*", ",al*", ",ql*", ",dl*",
         ",x0*", ",x1*", ",x2*", ",x3*", ",x4*", ",x5*", ",x6*", ",x7*",
         ",f1 ", ",itp", ",???", ",its", ",sd ", ",scr", ",f2 ", ",f3 ",
         ",ci ", ",*  ", ",sc ", ",ad ", ",di ", ",dic", ",id ", ",idc",
         ",*  ", ",*au", ",*qu", ",*du", ",*ic", ",*al", ",*ql", ",*dl",
         ",*x0", ",*x1", ",*x2", ",*x3", ",*x4", ",*x5", ",*x6", ",*x7");

/* DECLARATION OF AUTOMATIC STORAGE VARIABLES */

declare 1 auto_interpret_link_info aligned like interpret_link_info;

declare 1 old_interpret_link_info aligned based,
        2 segment_name char (32) aligned,
        2 entry_point_name char (33) aligned,
        2 expression char (8) aligned,
        2 modifier char (4) aligned,
        2 trap char (32) aligned;

declare (defbase, ili_ptr, lhp, linkp, original_lp, p, tpp, xp) pointer;
declare (have_textp, old_version_sw) bit (1) aligned;
declare (i, j, lng, type, offset, class) fixed bin;
declare  link_segment_no bit (18) aligned;
declare  modx fixed bin;
declare  segno fixed bin (18);
declare  searching bit (1) aligned;
declare  reloffset fixed bin (18);
declare  sign char (1) aligned;

/* DECLARATION OF BUILTIN FUNCTIONS */

declare (addr, addrel, baseno, bin, ptr, rel, substr) builtin;

/* DECLARATION OF BASED STRUCTURES */

declare 1 b1 aligned based (p),
        2 lhe18 fixed bin (18) unsigned unaligned,
        2 rhe18 fixed bin (18) unsigned unaligned;

declare 1 b7 aligned based (p),
        2 nwords fixed bin,
        2 dA_code fixed bin;

declare  based_ptr pointer based (lhp);


        have_textp = "0"b;
        go to join;

interpret_link_$tptr: entry (A_structure_ptr, A_link_ptr, text_ptr, A_code);

        have_textp = "1"b;

join:
        A_code = 0;
        if A_structure_ptr -> interpret_link_info.version = INTERPRET_LINK_INFO_VERSION_1 then do;
             old_version_sw = "0"b;
             ili_ptr = A_structure_ptr;                /* can put it right into caller's structure */
        end;
        else do;
             old_version_sw = "1"b;
             ili_ptr = addr (auto_interpret_link_info);
        end;

        ili_ptr -> interpret_link_info.segment_name,
             ili_ptr -> interpret_link_info.entry_point_name,
             ili_ptr -> interpret_link_info.expression,
             ili_ptr -> interpret_link_info.modifier,
             ili_ptr -> interpret_link_info.trap = "";

        linkp = A_link_ptr;                                /* copy argument for efficiency */

        if linkp -> object_link.tag = ITS_MODIFIER then do;
                                                /* we have a snapped link, we must find unsnapped version */
             sb = ptr (addr (p), 0);                        /* get ptr to stack header */
             lotp = sb -> stack_header.lot_ptr;                /* get ptr to lot from stack header */
             offset = bin (rel (linkp), 18);                /* get offset of snapped link in linkage segment */
             link_segment_no = baseno (linkp);                /* get segment number of linkage segment */
             searching = "1"b;
             do segno = 80 to sb -> stack_header.cur_lot_size while (searching); /* scan the lot */
                lhp = lot (segno).lp;
                if baseno (lhp) = link_segment_no then do;
                     reloffset = offset - bin (rel (lhp), 18);
                                                /* get offset into linkage section */
                     if reloffset >= lhp -> linkage_header.begin_links then
                                                /* might be in this section */
                        if reloffset < lhp -> linkage_header.block_length then do;
                                                /* we found the right linkage section */
                             if lhp -> linkage_header.unused then original_lp = addr (lhp -> linkage_header.unused) -> based_ptr;
                             else original_lp = lhp -> linkage_header.original_linkage_ptr;
                                                /* get pointer to virgin section */
                             linkp = addrel (original_lp, reloffset); /* get pointer to unsnapped link */
                             if linkp -> object_link.tag = FAULT_TAG_2 then
                                goto start;
                             searching = "0"b;        /* stop the search */
                        end;
                end;
             end;

             A_code = error_table_$no_linkage;
             return;
        end;

        else if linkp -> object_link.tag = FAULT_TAG_2 then
                                                /* its unsnapped, no problem */
             lhp = addrel (linkp, linkp -> object_link.header_relp);
                                                /* pointer to linkage block header */

        else do;
             A_code = error_table_$bad_link_type;
             return;
        end;

start:

        if lhp -> its.its_mod = ITS_MODIFIER                /* linkage header begins with pointer */
        then defbase = lhp -> linkage_header.def_ptr;        /* we have pointer to definition section */
        else do;                                        /* virgin linkage section, locate defs */
             if lhp -> virgin_linkage_header.defs_in_link = "0"b
                                                /* defs in text */
             then if have_textp
                then defbase = addrel (text_ptr, lhp -> virgin_linkage_header.def_offset);
                else defbase = ptr (lhp, lhp -> virgin_linkage_header.def_offset);
             else if lhp -> virgin_linkage_header.defs_in_link = "010000"b
                                                /* defs in linkage */
             then defbase = addrel (lhp, lhp -> virgin_linkage_header.def_offset);
             else do;
                A_code = error_table_$no_defs;        /* cannot locate defs */
                return;
             end;
        end;

        xp = addrel (defbase, linkp -> object_link.expression_relp);        /* pointer to link's expression word */

        modx = bin (linkp -> object_link.modifier, 6);        /* get the modifier */
        go to EXPRESSION_JOIN;

given_exp: entry (A_structure_ptr, Exp_relp, Modifier, Def_section_ptr, A_code);

declare Exp_relp fixed bin (18);
declare Def_section_ptr pointer;
declare Modifier bit (6) unaligned;

        A_code = 0;
        if A_structure_ptr -> interpret_link_info.version = INTERPRET_LINK_INFO_VERSION_1 then do;
             old_version_sw = "0"b;
             ili_ptr = A_structure_ptr;                /* can put it directly into caller's structure */
        end;
        else do;
             old_version_sw = "1"b;
             ili_ptr = addr (auto_interpret_link_info);
        end;

        xp = addrel (Def_section_ptr, Exp_relp);
        modx = bin (Modifier, 6);
        defbase = Def_section_ptr;
        ili_ptr -> interpret_link_info.segment_name,
             ili_ptr -> interpret_link_info.entry_point_name,
             ili_ptr -> interpret_link_info.expression,
             ili_ptr -> interpret_link_info.modifier,
             ili_ptr -> interpret_link_info.trap = "";

EXPRESSION_JOIN:

        if xp -> exp_word.expression ^= 0 then do;        /* there is an xp -> exp_word.expression */
             sign = "+";                                /* assume its positive */
             i = xp -> exp_word.expression;                /* convert value to fixed bin */
             if i < 0 then do;                        /* display negative value as minus sign        &         */
                  i = -i;                                /*   positive octal value.                        */
                sign = "-";
             end;
             call ioa_$rsnnl ("^a^o", ili_ptr -> interpret_link_info.expression, lng, sign, i);
        end;
        tpp = addrel (defbase, xp -> exp_word.type_relp);        /* pointer to type-pair  */

        type = tpp -> type_pair.type;                        /* get type of link */
        if (type = LINK_SELF_BASE) |
           (type = LINK_SELF_OFFSETNAME) then do;        /* self relative links */
             class = tpp -> type_pair.segname_relp;        /* get segref A_code */
             if (class >= CLASS_TEXT) &
                (class <= CLASS_HEAP) then
                ili_ptr -> interpret_link_info.segment_name = 
                         SYMBOLIC_SECTION_NAMES (class);
             else do;
                A_code = error_table_$bad_link_type;
                return;
             end;
        end;
        else do;                                        /* links with symbolic segname */
             p = addrel (defbase, tpp -> type_pair.segname_relp);
                                                /* pointer to segname ACC string */
             ili_ptr -> interpret_link_info.segment_name =
                  p -> acc_string.string;
        end;

        if (type = LINK_SELF_BASE) |
           (type = LINK_REFNAME_BASE) then
             substr (ili_ptr -> interpret_link_info.entry_point_name, 1, 2) = "|0";
        else do;
             p = addrel (defbase, tpp -> type_pair.offsetname_relp);
                                                /* pointer to entryname ACC string */
             substr (ili_ptr -> interpret_link_info.entry_point_name, 1, 1) = "$";
             substr (ili_ptr -> interpret_link_info.entry_point_name, 2) = 
                p -> acc_string.string;
        end;

        ili_ptr -> interpret_link_info.modifier = symbolic_modifier (modx);
                                                /* and convert to symbolic */

        if tpp -> type_pair.trap_relp ^= 0 then                /* there is a trap before link */
             do;
             p = addrel (defbase, tpp -> type_pair.trap_relp);
                                                /* pointer to trap-pair */
             if (type = LINK_CREATE_IF_NOT_FOUND) | 
                (type = LINK_SELF_OFFSETNAME & class = CLASS_SYSTEM) |
                (type = LINK_SELF_OFFSETNAME & class = CLASS_HEAP) then do; /* create if not found, or *system or *heap */
                     call ioa_$rsnnl ("        Init -> def|^o, length = ^d", ili_ptr -> interpret_link_info.trap,
                     lng, tpp -> type_pair.trap_relp, p -> nwords);
             end;
             else do;                                /* have real trap before link */
                i = p -> lhe18;                        /* offset of call link */
                j = p -> rhe18;                        /* offset of arg link */
                call ioa_$rsnnl ("        Trap: call ^o, arg ^o", ili_ptr -> interpret_link_info.trap, lng, i, j);
             end;
        end;

        if old_version_sw then do;
             A_structure_ptr -> old_interpret_link_info.segment_name = ili_ptr -> interpret_link_info.segment_name;
             A_structure_ptr -> old_interpret_link_info.entry_point_name =
                substr(ili_ptr -> interpret_link_info.entry_point_name,1,33);
             A_structure_ptr -> old_interpret_link_info.expression = ili_ptr -> interpret_link_info.expression;
             A_structure_ptr -> old_interpret_link_info.modifier = ili_ptr -> interpret_link_info.modifier;
             A_structure_ptr -> old_interpret_link_info.trap = 
                substr(ili_ptr -> interpret_link_info.trap,1,32);
        end;

        return;

%page;
/* Include Files */
%include interpret_link_info;
%page;
%include lot;
%page;
%include definition_dcls;
%page;
%include object_link_dcls;
%page;
%include its;
%page;
%include stack_header;
     end interpret_link_;


                    print_bind_map.pl1              11/04/82  1947.9rew 11/04/82  1608.7       67455



/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   * Copyright (c) 1972 by Massachusetts Institute of        *
   * Technology and Honeywell Information Systems, Inc.      *
   *                                                         *
   *********************************************************** */


/* Print Object Map - Command to display the bindmap of a segment bound by the
   new Binder.
   Designed and Initially coded by Michael J. Spier, May 3, 1971
   Modified 01/22/82 by E. N. Kittlitz. added -page_offset */

/* format: style4 */
print_bind_map:
pbm:
     procedure;


/* DECLARATION OF EXTERNAL ENTRIES */

declare  com_err_ external entry options (variable);
declare  cv_oct_check_ external entry (char (*), fixed bin (35)) returns (fixed bin);
declare  cv_dec_check_ external entry (char (*), fixed bin (35)) returns (fixed bin);
declare  cu_$arg_count external entry (fixed bin, fixed bin (35));
declare  cu_$arg_ptr external entry (fixed bin, pointer, fixed bin, fixed bin (35));
declare  date_time_ external entry (fixed bin (71), char (*));
declare  expand_pathname_ entry (char (*), char (*), char (*), fixed bin (35));
declare  form_bind_map_ external entry (pointer, fixed bin (35));
declare  get_temp_segment_ entry (char (*), ptr, fixed bin (35));
declare  hcs_$initiate external entry (char (*), char (*), char (*), fixed bin, fixed bin, ptr, fixed bin (35));
declare  ioa_ external entry options (variable);
declare  iox_$put_chars entry (ptr, ptr, fixed bin (21), fixed bin (35));
declare  iox_$user_output ext ptr;
declare  hcs_$terminate_noname external entry (ptr, fixed bin (35));
declare  pathname_ entry (char(*), char(*)) returns(char(168));
declare  release_temp_segment_ entry (char (*), ptr, fixed bin (35));

/* DECLARATIONS OF EXTERNAL STATIC VARIABLES */

declare  (
         error_table_$noarg,
         error_table_$too_many_args
         ) external fixed bin (35);

/* DECLARATIONS OF BUILTIN FUNCTIONS */

declare  (addr, clock, divide, null) builtin;

/* DECLARATIONS OF CONDITIONS */

declare  cleanup condition;

/* DECLARATION OF AUTOMATIC STORAGE VARIABLES */

declare  (i, nargs, curarg, arg_lng, value) fixed bin;
declare  code fixed bin (35);
declare  (p, argp, valptr) pointer;
declare  (
         objname char (32),
         dirname char (168),
         pathname char (168)
         );
declare  errname char (16);
declare  date char (24);
declare  (have_pathname, name_string, option) bit (1) aligned;

declare  1 x aligned,
         2 objp pointer,                                /* pointer to object segment */
         2 listp pointer,                                /* pointer to list segment */
         2 list_bc fixed bin (24),                        /* list segment bitcount */
         2 flags aligned,                                /* option indicators */
           3 pad bit (33) unaligned,                        /* this field used to be fixed bin */
           3 page_offset bit (1) unaligned,                /* 1 -> show page number; */
           3 no_header bit (1) unaligned,                /* 1 -> do not print header */
           3 long bit (1) unaligned,                        /* 1 -> long option; 0 -> short option */
         2 nopts fixed bin,                                /* number of options */
         2 op (16) aligned,
           3 opt_name char (32) aligned,                /* name of option component */
           3 opt_offset fixed bin,                        /* offset option */
           3 name_sw bit (1) unaligned,                /* 0 -> offset; 1 -> segname */
           3 found bit (1) unaligned;                        /* preset to 0  */


/* DECLARATION OF BASED STRUCTURES */

declare  argname char (arg_lng) unaligned based (argp);
declare  1 b0 aligned based (valptr),
         2 dum char (3) unaligned,
         2 valchar char (1) unaligned;




        errname = "print_bind_map";

start:
        valptr = addr (value);
        objp, listp = null;

        on cleanup call clean_up;

        list_bc, value, nopts = 0;

        page_offset, long, no_header, name_string, have_pathname, option = "0"b;
        pathname = "";

        call cu_$arg_count (nargs, code);
        if code ^= 0 then do;
             call com_err_ (code, errname);
             return;
        end;
        if nargs = 0 then do;                        /* must have at least bound_name */
give_usage:
             call com_err_ (0, errname, "Usage: print_bind_map pathname {-control_args}");
             return;
        end;

        do curarg = 1 to nargs;
             call cu_$arg_ptr (curarg, argp, arg_lng, code);
             if code ^= 0 then do;
                call com_err_ (code, errname);
                return;
             end;
             if name_string then
                go to name_arg;                        /* if this follows -name */
             else if char (argname, 1) ^= "-" then do;
                if ^have_pathname then do;                /* must be the bound object name */
                     call expand_pathname_ (argname, dirname, objname, code);
                     if code ^= 0 then do;
                        call com_err_ (code, errname, argname);
                        goto return;
                     end;
                     pathname = pathname_ (dirname, objname);
                     have_pathname = "1"b;
                end;
                else do;                                /* must be a component name/offset */
name_arg:
                     option = "1"b;
                     nopts = nopts + 1;
                     if nopts > hbound (op, 1) then do;
                        call com_err_ (error_table_$too_many_args, errname,
                             "At most ^d components may be specified.", hbound (op, 1));
                        return;
                     end;
                     op (nopts).found = "0"b;
                     if name_string = "1"b then do;        /* this arg is a name */
                        name_string = "0"b;                /* don't inflict this on rest of args */
                        go to char_string;
                     end;
                     op (nopts).opt_name = " ";
                     op (nopts).opt_offset = cv_oct_check_ (argname, code);
                     if code ^= 0 then do;
                        i = cv_dec_check_ (argname, code);
                                                /* see if it's decimal */
                        if code ^= 0 then
                             goto char_string;
                        call com_err_ (0, errname, "^a is not an octal number", argname);
                        nopts = nopts - 1;                /* reset index */
                        goto get_next;
                     end;
                     op (nopts).name_sw = "0"b;
                     goto get_next;
char_string:
                     op (nopts).opt_name = argname;
                     op (nopts).opt_offset = 0;
                     op (nopts).name_sw = "1"b;

                end;
             end;
             else if argname = "-nm" | argname = "-name" then
                name_string = "1"b;                        /* remember next string is a name */
             else if argname = "-lg" | argname = "-long" then
                long = "1"b;
             else if argname = "-nhe" | argname = "-no_header" then
                no_header = "1"b;
             else if argname = "-page_offset" | argname = "-pgofs" then
                page_offset = "1"b;
get_next:
        end;

        if ^have_pathname then
             go to give_usage;

        if name_string then do;
             call com_err_ (error_table_$noarg, errname, "-name must be followed by an argument.");
             return;
        end;

        if option = "1"b then
             if nopts = 0 then
                return;

        call hcs_$initiate (dirname, objname, "", 0, 0, objp, code);
        if objp = null then do;
             call com_err_ (code, errname, pathname);
             goto return;
        end;

        call get_temp_segment_ (errname, listp, code);
        if code ^= 0 then do;
             call com_err_ (code, errname, "cannot get temporary segment.");
             goto return;
        end;

        call form_bind_map_ (addr (x), code);                /* go format bindmap into list segment */
        if code ^= 0 then do;                        /* file system error */
             call com_err_ (code, errname, pathname);
             goto return;
        end;
        if ^no_header then do;                        /* print title only with header */
             call date_time_ ((clock ()), date);
             call ioa_ ("^/^-^-^a^4x^a", objname, date);
        end;
        call iox_$put_chars (iox_$user_output, listp, divide (list_bc, 9, 21, 0), code);


return:        call clean_up;


clean_up: procedure;
        if objp ^= null then
             call hcs_$terminate_noname (objp, code);
        if listp ^= null then
             call release_temp_segment_ (errname, listp, code);
     end;


     end print_bind_map;


                    print_link_info.pl1             10/06/88  1044.8rew 10/06/88  1044.0       95157



/****^  ***********************************************************
        *                                                         *
        * Copyright, (C) Honeywell Bull Inc., 1988                *
        *                                                         *
        * Copyright, (C) Honeywell Information Systems Inc., 1982 *
        *                                                         *
        * Copyright (c) 1972 by Massachusetts Institute of        *
        * Technology and Honeywell Information Systems, Inc.      *
        *                                                         *
        *********************************************************** */




/****^  HISTORY COMMENTS:
  1) change(86-07-31,Elhard), approve(86-07-31,MCR7457),
     audit(86-11-05,DGHowe), install(86-11-20,MR12.0-1222):
     Modified to iterate across components 1 to N of an object MSF.
  2) change(88-09-21,TLNguyen), approve(88-09-21,MCR7997),
     audit(88-10-03,Parisek), install(88-10-06,MR12.2-1135):
     Change the print_link_info command to return selected items of information
     for the specified object segment.
                                                   END HISTORY COMMENTS */


/* Print Link Info - command to display the contents of a standard Multics object segment.

   Designed and Initially coded by Michael J. Spier, March 17, 1971
   modified 75/06/05 by M. Weaver to remove call to old_print_link_info
   modified 76/07/28 by S. Webber to use get_temp_segments_ and to add -header
   modified 77/03/03 by M. Weaver to handle case of -he only
   modified 78/10/12 by J. Spencer Love to add a cleanup handler.
   also changed to use clock builtin and singular version of get_temp_segment_
   modified 82/03/08 by M. Weaver to handle archive components
*/

/* format: style3,^indnoniterdo */
#endif

static void print_link_info (int argc, char * argv [])
  {
#if 0

/* Obligatory overhead */

dcl        (addr, clock, divide, min, null, ptr, substr, unspec)
                        builtin;
dcl        cleanup                condition;

/* AUTOMATIC STORAGE VARIABLES */
#endif

//--- dcl        (arg_count, arg_len)
//---                         fixed bin;

    int last_path, arg_idx;

//--- dcl        status                fixed bin (35);
//--- dcl        bitcount                fixed bin (24);
    int status;
    long bitcount;
#if 0
dcl        (my_arg_ptr, arg_ptr, p)
                        ptr;
#endif
    bool header_sw;
    bool msf_sw;
//--- dcl        componentname        char (32);
    char * componentname;
#if 0
dcl        objname                char (32);
dcl        dirname                char (168);
dcl        date                char (24) aligned;

dcl        1 auto_form_link_info_args
                        aligned like form_link_info_args;

/* STATIC VARIABLES AND CONSTANTS */

dcl        who_am_i                char (16) static init ("print_link_info") options (constant);

/* BASED STRUCTURES */

dcl        arg                char (arg_len) unaligned based (my_arg_ptr);

%include form_link_info_args;
%include object_info;

/* EXTERNALS */

dcl        active_fnc_err_        entry () options (variable);
dcl        check_star_name_$entry
                        entry (char (*), fixed bin (35));
dcl        com_err_                entry () options (variable);
dcl        com_err_$suppress_name
                        entry () options (variable);
dcl        cu_$af_arg_count        entry (fixed bin, fixed bin (35));
dcl        cu_$arg_ptr        entry (fixed bin, ptr, fixed bin, fixed bin (35));
dcl        date_time_        entry (fixed bin (71), char (*) aligned);
dcl        error_table_$badopt external fixed bin (35);
dcl        error_table_$dirseg external fixed bin (35);
dcl        error_table_$nostars
                        ext fixed bin (35);
dcl        error_table_$not_act_fnc
                        fixed bin (35) external;
dcl        expand_pathname_$component
                        entry (char (*), char (*), char (*), char (*), fixed bin (35));
dcl        form_link_info_        entry (ptr, fixed bin (35));
dcl        get_system_free_area_
                        entry() returns(ptr);
dcl        get_temp_segment_        entry (char (*), ptr, fixed bin (35));
dcl        hcs_$terminate_noname
                        entry (ptr, fixed bin (35));
dcl        initiate_file_$component
                        entry (char (*), char (*), char (*), bit (*), ptr, fixed bin (24), fixed bin (35));
dcl        ioa_                entry () options (variable);
dcl        iox_$put_chars        entry (ptr, ptr, fixed bin (22), fixed bin (35));
dcl        iox_$user_output        ext ptr;
dcl        object_lib_$get_component_info
                        entry (ptr, ptr, char(8), char(*), ptr,
                          fixed bin(35));
dcl        object_lib_$initiate
                        entry (char (*), char (*), char (*), bit (1), ptr, fixed bin (24), bit (1), fixed bin (35));
dcl        release_temp_segment_
                        entry (char (*), ptr, fixed bin (35));
dcl        pathname_$component entry (char (*), char (*), char (*)) returns (char (194));

#endif

//---        call cu_$af_arg_count (arg_count, status);
//---        if status = 0
//---        then do;
//---             call active_fnc_err_ (0, who_am_i, "This command may not be invoked as an active function.");
//---             return;
//---        end;
//---        else if status ^= error_table_$not_act_fnc
//---        then do;
//---             call com_err_ (status, who_am_i);
//---             return;
//---        end;
//---

    msf_sw = false; /* must be initialized */
    last_path = 0;
    header_sw = true;

//--- unspec (auto_form_link_info_args) = "0"b;
    // I think....
    memset (& form_link_info_args, 0, sizeof (form_link_info_args));

#if 0
        arg_ptr = addr (auto_form_link_info_args);
#endif
    form_link_info_args . version = form_link_info_args_version_1;

    for (arg_idx = 1; arg_idx < argc; arg_idx ++)
      {
        if (argv [arg_idx] [0] != '-')
          last_path = arg_idx;
        else if (strcmp (argv [arg_idx], "-no_header") == 0 ||
                 strcmp (argv [arg_idx], "-nhe") == 0)
          form_link_info_args . flags . hd_sw = true;
        else if (strcmp (argv [arg_idx], "-length") == 0 ||
                 strcmp (argv [arg_idx], "-ln") == 0)
          form_link_info_args . flags . ln_sw = true;
        else if (strcmp (argv [arg_idx], "-entry") == 0 ||
                 strcmp (argv [arg_idx], "-et") == 0)
          form_link_info_args . flags . et_sw = true;
        else if (strcmp (argv [arg_idx], "-link") == 0 ||
                 strcmp (argv [arg_idx], "-lk") == 0)
          form_link_info_args . flags . lk_sw = true;
        else if (strcmp (argv [arg_idx], "-long") == 0 ||
                 strcmp (argv [arg_idx], "-lg") == 0)
          form_link_info_args . flags . lg_sw = true;
        else
          {
            die ("print_bind_map: bad option\n");
            exit (1);
          }
      }

    if (last_path == 0)
      die ("Usage print_bind_map paths {-control args}\n");

    if ((! form_link_info_args . flags . hd_sw) &&
        (! form_link_info_args . flags . ln_sw) &&
        (! form_link_info_args . flags . et_sw) &&
        (! form_link_info_args . flags . lk_sw))
      {
        form_link_info_args . flags . hd_sw = header_sw;
        form_link_info_args . flags . ln_sw = true;
        form_link_info_args . flags . et_sw = true;
        form_link_info_args . flags . lk_sw = true;
      }
    else
      header_sw = form_link_info_args . flags. hd_sw; /* -header overrides -no_header */

    if (form_link_info_args . flags . lg_sw)
      header_sw = form_link_info_args . flags . hd_sw = true; /* -long forces -header */

/* Now proceed to print object segment information */

    form_link_info_args . list_ptr = NULL;
    form_link_info_args . obj_ptr = NULL;

#if 0
        on cleanup
             begin;
                if form_link_info_args.list_ptr ^= null ()
                then call release_temp_segment_ (who_am_i, form_link_info_args.list_ptr, status);
                if form_link_info_args.obj_ptr ^= null ()
                then call hcs_$terminate_noname (ptr (form_link_info_args.obj_ptr, 0), status);
             end;
#endif

//---   call get_temp_segment_ (who_am_i, form_link_info_args.list_ptr, status);
//---   if status ^= 0
//---   then do;
//---        call com_err_ (status, who_am_i, "Getting temp segment.");
//---        return;
//---   end;

    form_link_info_args . list_ptr = malloc (256 * 1024 * sizeof (word36));
    if (! form_link_info_args . list_ptr)
      die ("print_bind_map: Getting temp segment\n");

    for (arg_idx = 1; arg_idx < argc; arg_idx ++)
      {
        if (argv [arg_idx] [0] != '-')
          {

//---           call expand_pathname_$component (arg, dirname, objname, componentname, status);
//---           if status ^= 0
//---           then do;
//---                call com_err_ (status, who_am_i, "^a", arg);
//---                goto finish_up;
//---           end;
//---           call check_star_name_$entry (objname, status);
//---           if status ^= 0
//---           then do;
//---                if status = 1 | status = 2
//---                then status = error_table_$nostars;
//---                call com_err_ (status, who_am_i, "^a", objname);
//---                go to finish_up;
//---           end;
            componentname = argv [arg_idx];

//---           form_link_info_args.flags.cn_sw = (componentname ^= "");
//---           form_link_info_args.component_name = componentname;
            form_link_info_args. flags . cn_sw = strlen (componentname) != 0;
            form_link_info_args . componentname = componentname;

//---           call initiate_file_$component (dirname, objname, componentname, "100"b, form_link_info_args.obj_ptr,
//---                bitcount, status);
            status = initiate_file_component (componentname, & form_link_info_args . obj_ptr, & bitcount);

//---           if status = error_table_$dirseg & componentname = ""
//---           then call object_lib_$initiate (dirname, objname, "", "1"b, form_link_info_args.obj_ptr, bitcount,
//---                   msf_sw, status);
//---
//---           if form_link_info_args.obj_ptr = null ()
//---           then do;
//--- error_return:
//---                call com_err_ (status, who_am_i, "^a", pathname_$component (dirname, objname, componentname));
//---                goto get_next;
//---           end;
            if (status < 0)
              {
                fprintf (stderr, "initiate_file_component returned %d; skipping", status);
                continue;
              }

            form_link_info_args . flags . bc_sw = true;
            form_link_info_args . bit_count = bitcount;

            form_link_info_args . list_bc = 0;

/* if we have an msf use special handling */

            if (msf_sw)
              status = form_msf_link_info ();
            else
              status = form_link_info_ (& form_link_info_args); /* format information into listing segment */
#if 0
                if status ^= 0
                then go to error_return;

                if header_sw
                then do;
                     call date_time_ (clock (), date);
                     call ioa_ ("^/^2-^a^4x^a", arg, date);
                end;
                call iox_$put_chars (iox_$user_output, form_link_info_args.list_ptr,
                     divide (form_link_info_args.list_bc, 9, 21, 0), status);

get_next:
                if form_link_info_args.obj_ptr ^= null ()
                then do;
                     p = ptr (form_link_info_args.obj_ptr, 0);
                     form_link_info_args.obj_ptr = null ();
                                                /* prevent race with cleanup handler */
                     call hcs_$terminate_noname (p, status);
                end;
             end;
        end;
#endif
          }
      }
#if 0
finish_up:
        if form_link_info_args.list_ptr ^= null ()
        then call release_temp_segment_ (who_am_i, form_link_info_args.list_ptr, status);
        return;
#endif

  } // print_link_info

static int form_msf_link_info (void)
  {
#if 0
form_msf_link_info:
dcl        status                fixed bin (35);

dcl        sys_areap                ptr;
dcl        sys_area                area based (sys_areap);
dcl        c                fixed bin;

%include object_lib_defs;

        sys_areap = get_system_free_area_ ();

        comp_infop = null;

        on cleanup
             begin;
                if comp_infop ^= null
                then do;
                     do c = 1 to component_info.max;
                        call hcs_$terminate_noname (component_info.comp (c).segp, 0);
                     end;
                     free component_info in (sys_area);
                end;
             end;

        call object_lib_$get_component_info (form_link_info_args.obj_ptr, sys_areap, component_info_version_1, "none",
             comp_infop, status);
        if status ^= 0
        then return;

        do c = 1 to component_info.max while (status = 0);
             form_link_info_args.obj_ptr = component_info.comp (c).segp;
             form_link_info_args.bit_count = component_info.comp (c).bc;

             call form_link_info_ (arg_ptr, status);
        end;
        
        do c = 1 to component_info.max;
             call hcs_$terminate_noname (component_info.comp (c).segp, 0);
        end;
        free component_info in (sys_area);

     end form_msf_link_info;
#endif

    return 0;
  }

#if 0

     end;                                                /* END OF EXTERNAL PROCEDURE print_link_info */




                    print_linkage_usage.pl1         11/05/86  1303.1r w 11/04/86  1033.7       55458



/* ***********************************************************
   *                                                         *
   * Copyright, (C) Honeywell Information Systems Inc., 1982 *
   *                                                         *
   * Copyright (c) 1972 by Massachusetts Institute of        *
   * Technology and Honeywell Information Systems, Inc.      *
   *                                                         *
   *********************************************************** */


print_linkage_usage: plu: proc;

/* procedure to print current combined linkage segment
   usage using LOT and ISOT as a guide */

/* originally coded 10/69 by J W Gintell
   modified several times in 1969 and 1970 by JWG
   modified to print out static stuff and convert to v2pl1 SHW 1/31/74
   re-written to handle separate static (via the ISOT) 9/8/75 by M. Weaver
   modified 06/82 by F. W. Martinson to check arguments
*/

dcl (i, j, n, k, owner_segno) fixed bin;
dcl (l, l1, l2, l3, total_static, next_ls, static_lng) fixed bin (18);
dcl (hc_seg_count, highest_segno) fixed bin;
dcl  type fixed bin (2);
dcl  mode fixed bin (5);
dcl  bitcnt fixed bin (24);
dcl  code fixed bin (35);

dcl (p, statp) ptr;

dcl  pathname char (168) aligned;
dcl  entry char (32) aligned;

dcl 1 t (1024) aligned,                                /* keeps merged lot and isot */
    2 sect_ptr ptr unaligned,                                /* ptr to beginning of section */
    2 segno fixed bin,                                /* segment number of section's owner */
    2 sep_stat bit (1) aligned,                        /* "1"b->separate static */
    2 sect_type fixed bin;                                /* 1->linkage section, 2->static section */

dcl  ti (2000) fixed bin;                                /* keeps sorted indices into t */

dcl (baseno, baseptr, bin, bit, convert, divide, fixed, rel, ptr, substr, unspec) builtin;

dcl  com_err_ entry () options (variable);
dcl  cu_$arg_count entry (fixed bin);
dcl  cu_$stack_frame_ptr entry returns (ptr);
dcl  hcs_$high_low_seg_count entry (fixed bin, fixed bin);
dcl  hcs_$status_mins ext entry (ptr, fixed bin (2), fixed bin (24), fixed bin (35));
dcl  hcs_$fs_get_mode entry (ptr, fixed bin (5), fixed bin (35));
dcl  hcs_$fs_get_path_name entry (ptr, char (*) aligned, fixed bin, char (*) aligned, fixed bin (35));
dcl  ioa_ entry options (variable);

%include stack_header;


%include lot;
%include linkdcl;

        call cu_$arg_count (n);                        /* Make sure there are no arguments */
        if n ^= 0 then do;
             call com_err_ (0, "print_linkage_usage", " This command takes no arguments.");
             return;
        end;
                                                /* Get a pointer to the current stack frame.  From that construct a pointer to the
                                                   base of the stack.  Then pick up pointers to the LOT and the ISOT from the base of the stack. */

        sb = cu_$stack_frame_ptr ();
        sb = ptr (sb, 0);
        lotp = sb -> stack_header.lot_ptr;
        isotp = sb -> stack_header.isot_ptr;

/* Obtain the range of valid non ring 0 segment numbers. */

        call hcs_$high_low_seg_count (highest_segno, hc_seg_count);
        highest_segno = highest_segno + hc_seg_count;

/* Must end up with a list ordered according to location in the cls.
   First merge LOT and ISOT and initialize the index array used to keep
   track of the sorting.  ASSUME that an isot entry is not filled in unless
   the corresponding lot entry is. */

        total_static,
             n = 0;

        do i = hc_seg_count+1 to highest_segno;
             if unspec (lot.lp (i))                        /* nonzero lot entry */
             then if substr (unspec (lot.lp (i)), 1, 2) ^= "11"b then do; /* no packed ptr fault */
                     ti (n+1), n = n+1;
                     t (n).sect_ptr = lot.lp (i);
                     t (n).segno = i;
                     t (n).sect_type = 1;
                     t (n).sep_stat = "0"b;
                     if unspec (isot.isp (i))
                     then if isot.isp (i) ^= lot.lp (i)
                        then if substr (unspec (isot.isp (i)), 1, 2) ^= "11"b then do;
                                                /* not a packed ptr fault */
                                t (n).sep_stat = "1"b;
                                ti (n+1), n = n+1;
                                t (n).sect_ptr = isot.isp (i);
                                t (n).segno = i;
                                t (n).sect_type = 2;
                             end;
                end;
        end;

        call ioa_ ("^/^/^-Current linkage and static section usage"); /* want linkage fault now */

/* sort merged list by sorting ti */

        do i = 1 to n-1;
             do j = i to 1 by -1 while (unspec (t (ti (j)).sect_ptr) > unspec (t (ti (j+1)).sect_ptr));
                k = ti (j);
                ti (j) = ti (j+1);
                ti (j+1) = k;
             end;
        end;

/* now sorted */

        call ioa_ ("^/^5xsegment^29xlinkage^4xsize^7xstatic^5xsize^/");

        do i = 1 to n;                                /* print each entry, sorted */

             owner_segno = t (ti (i)).segno;                /* copy segment number of section's owner */
             static_lng = bin (lot.lp (owner_segno) -> header.stats.static_length, 18);

             if t (ti (i)).sect_type = 2 then go to nope; /* don't print  separately  */

             call hcs_$fs_get_path_name (baseptr (owner_segno), pathname, j, entry, code);
             if code ^= 0 then entry = "NOT A LINKAGE SECTION";

/* scan for end of section */
/* p is ptr to base of section
   l is offset of last location in linkage section
   l1 = 0mod(2) value of l
   l2 = offset of beginning of linkage section
   l3 = true length of linkage section
*/

             p = t (ti (i)).sect_ptr;
             total_static = total_static + static_lng;

             l2 = fixed (rel (p), 18);
             l3 = fixed (p -> header.stats.block_length, 18);
             l = l2 + l3;
             l1 = divide (l+1, 2, 18, 0)*2;

             if t (ti (i)).sep_stat then do;                /* separate static */
                statp = t (ti (i)+1).sect_ptr;
                call ioa_ ("^32a^4o^6o^7o^7o^8o^7o^7o", entry, owner_segno,
                     bin (baseno (p), 18), l2, l3, bin (baseno (statp), 18), bin (rel (statp), 18),
                     static_lng);
             end;

             else call ioa_ ("^32a^4o^6o^7o^7o^15x^7o",
                entry, owner_segno, bin (baseno (p), 18), l2, l3, static_lng);

/*
   !* if not at end of sorted list and next entry has unused segno
   then check for unused space *!

   check_for_hole:
   if i = n then go to nope;
   if baseno (t (ti (i)).sect_ptr) ^= baseno (t (ti (i+1)).sect_ptr) then go to nope;
   next_ls = fixed (rel (t (ti (i+1)).sect_ptr), 18);
   if l1 < next_ls then do;
   call ioa_ ("NOT A LINKAGE SECTION               ^6o^7o^7o",
   fixed (baseno (t (ti (i)).sect_ptr), 18), l, next_ls - l);
   end;
*/

nope:        end;

        call ioa_ ("^/Total static = ^o^/", total_static);

     end;



                    bull_copyright_notice.txt       08/30/05  1008.4r   08/30/05  1007.3    00020025

                                          -----------------------------------------------------------


Historical Background

This edition of the Multics software materials and documentation is provided and donated
to Massachusetts Institute of Technology by Group Bull including Bull HN Information Systems Inc. 
as a contribution to computer science knowledge.  
This donation is made also to give evidence of the common contributions of Massachusetts Institute of Technology,
Bell Laboratories, General Electric, Honeywell Information Systems Inc., Honeywell Bull Inc., Groupe Bull
and Bull HN Information Systems Inc. to the development of this operating system. 
Multics development was initiated by Massachusetts Institute of Technology Project MAC (1963-1970),
renamed the MIT Laboratory for Computer Science and Artificial Intelligence in the mid 1970s, under the leadership
of Professor Fernando Jose Corbato.Users consider that Multics provided the best software architecture for 
managing computer hardware properly and for executing programs. Many subsequent operating systems
incorporated Multics principles.
Multics was distributed in 1975 to 2000 by Group Bull in Europe , and in the U.S. by Bull HN Information Systems Inc., 
as successor in interest by change in name only to Honeywell Bull Inc. and Honeywell Information Systems Inc. .

                                          -----------------------------------------------------------

Permission to use, copy, modify, and distribute these programs and their documentation for any purpose and without
fee is hereby granted,provided that the below copyright notice and historical background appear in all copies
and that both the copyright notice and historical background and this permission notice appear in supporting
documentation, and that the names of MIT, HIS, Bull or Bull HN not be used in advertising or publicity pertaining
to distribution of the programs without specific prior written permission.
    Copyright 1972 by Massachusetts Institute of Technology and Honeywell Information Systems Inc.
    Copyright 2006 by Bull HN Information Systems Inc.
    Copyright 2006 by Bull SAS
    All Rights Reserved
#endif


// usage print_bind_map <segment file>

int main (int argc, char * argv [])
  {
    print_link_info (argc, argv);
  }

