//++ 
//++ /* BEGIN INCLUDE FILE iocbx.incl.pl1 */
//++ /* written 27 Dec 1973, M. G. Smith */
//++ /* returns attributes removed, hashing support BIM Spring 1981 */
//++ /* version made character string June 1981 BIM */
//++ /* Modified 11/29/82 by S. Krupp to add new entries and to change
//++       version number to IOX2. */
//++ /* format: style2 */
//++ 
//++      dcl      1 iocb                     aligned based,    /* I/O control block. */
//++         2 version        character (4) aligned,    /* IOX2 */
//++         2 name           char (32),                /* I/O name of this block. */
//++         2 actual_iocb_ptr    ptr,          /* IOCB ultimately SYNed to. */
//++         2 attach_descrip_ptr ptr,          /* Ptr to printable attach description. */
//++         2 attach_data_ptr    ptr,          /* Ptr to attach data structure. */
//++         2 open_descrip_ptr   ptr,          /* Ptr to printable open description. */
//++         2 open_data_ptr      ptr,          /* Ptr to open data structure (old SDB). */
//++         2 event_channel      bit (72),             /* Event channel for asynchronous I/O. */
//++         2 detach_iocb            entry (ptr, fixed bin (35)),
//++                                            /* detach_iocb(p) */
//++         2 open           entry (ptr, fixed, bit (1) aligned, fixed bin (35)),
//++                                            /* open(p,mode,not_used) */
//++         2 close          entry (ptr, fixed bin (35)),
//++                                            /* close(p) */
//++         2 get_line       entry (ptr, ptr, fixed (21), fixed (21), fixed bin (35)),
//++                                            /* get_line(p,bufptr,buflen,actlen) */
//++         2 get_chars              entry (ptr, ptr, fixed (21), fixed (21), fixed bin (35)),
//++                                            /* get_chars(p,bufptr,buflen,actlen) */
//++         2 put_chars              entry (ptr, ptr, fixed (21), fixed bin (35)),
//++                                            /* put_chars(p,bufptr,buflen) */
//++         2 modes          entry (ptr, char (*), char (*), fixed bin (35)),
//++                                            /* modes(p,newmode,oldmode) */
//++         2 position       entry (ptr, fixed, fixed (21), fixed bin (35)),
//++                                            /* position(p,u1,u2) */
//++         2 control        entry (ptr, char (*), ptr, fixed bin (35)),
//++                                            /* control(p,order,infptr) */
//++         2 read_record            entry (ptr, ptr, fixed (21), fixed (21), fixed bin (35)),
//++                                            /* read_record(p,bufptr,buflen,actlen) */
//++         2 write_record           entry (ptr, ptr, fixed (21), fixed bin (35)),
//++                                            /* write_record(p,bufptr,buflen) */
//++         2 rewrite_record     entry (ptr, ptr, fixed (21), fixed bin (35)),
//++                                            /* rewrite_record(p,bufptr,buflen) */
//++         2 delete_record      entry (ptr, fixed bin (35)),
//++                                            /* delete_record(p) */
//++         2 seek_key       entry (ptr, char (256) varying, fixed (21), fixed bin (35)),
//++                                            /* seek_key(p,key,len) */
//++         2 read_key       entry (ptr, char (256) varying, fixed (21), fixed bin (35)),
//++                                            /* read_key(p,key,len) */
//++         2 read_length            entry (ptr, fixed (21), fixed bin (35)),
//++                                            /* read_length(p,len) */
//++         2 open_file              entry (ptr, fixed bin, char (*), bit (1) aligned, fixed bin (35)),
//++                                            /* open_file(p,mode,desc,not_used,s) */
//++         2 close_file             entry (ptr, char (*), fixed bin (35)),
//++                                            /* close_file(p,desc,s) */
//++         2 detach         entry (ptr, char (*), fixed bin (35)),
//++                                            /* detach(p,desc,s) */
//++                                            /* Hidden information, to support SYN attachments. */
//++         2 ios_compatibility  ptr,          /* Ptr to old DIM's IOS transfer vector. */
//++         2 syn_inhibits           bit (36),         /* Operations inhibited by SYN. */
//++         2 syn_father             ptr,              /* IOCB immediately SYNed to. */
//++         2 syn_brother            ptr,              /* Next IOCB SYNed as this one is. */
//++         2 syn_son        ptr,              /* First IOCB SYNed to this one. */
//++         2 hash_chain_ptr     ptr;          /* Next IOCB in hash bucket */
//++ 
//++      declare iox_$iocb_version_sentinel
//++                          character (4) aligned external static;
//++ 
//++ /* END INCLUDE FILE iocbx.incl.pl1 */


typedef struct  __attribute__ ((__packed__)) iocb
  {
    word36 version; // 0
    word36 name [8]; // 1
    word36 make_even1; // 9
    word72 actual_iocb_ptr; // 10
    word72 attach_descrip_ptr; // 12
    word72 attach_data_ptr;  // 14
    word72 open_descrip_ptr; // 16
    word72 open_data_ptr; // 18
    word72 event_channel; // 20
    word72 detach_iocb [2]; // 22
    word72 open [2]; // 26
    word72 close [2]; // 30
    word72 get_line [2]; // 34
    word72 get_chars [2]; // 38
    word72 put_chars [2]; // 42
    word72 modes [2]; // 46
    word72 position [2]; // 50
    word72 control [2]; // 54
    word72 read_record [2]; // 58
    word72 write_record [2]; // 62
    word72 rewrite_record [2]; // 66
    word72 delete_record [2]; // 70
    word72 seek_key [2]; // 74
    word72 read_key [2]; // 78
    word72 read_length [2]; // 82
    word72 open_file [2]; // 86
    word72 close_file [2]; // 90
    word72 detach [2]; // 94
    word72 ios_compatibility; // 98
    word36 syn_inhibits; // 100
    word36 make_even2; // 101
    word72 syn_father; // 102
    word72 syn_brother; // 104
    word72 syn_son; // 106
    word72 hash_chain_ptr; // 108
  } iocb;

