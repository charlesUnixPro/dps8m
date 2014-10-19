//++ 
//++ /* BEGIN INCLUDE SEGMENT ... symbol_block.incl.pl1
//++ coded February 8, 1972 by Michael J. Spier	*/
//++ 
//++ /* last modified may 3, 1972  by M. Weaver */
//++ 
//++ declare	1 sb aligned based(sblkp),		/* structure describing a standard symbol block */
//++ 	2 decl_vers fixed bin,		/* version number of current structure format */
//++ 	2 identifier char(8) aligned,		/* symbolic code to define purpose of this symb block */
//++ 	2 gen_version_number fixed bin,	/* positive integer designating version of object generator */
//++ 	2 gen_creation_time fixed bin(71),	/* clock reading of date/time generator was created */
//++ 	2 obj_creation_time fixed bin(71),	/* clock reading of date/time object was generated */
//++ 	2 generator char(8) aligned,		/* name of processor which generated segment */
//++ 	2 gen_name_offset bit(18) unaligned,	/* offset of generator name in words rel to base of symbol block */
//++ 	2 gen_name_length bit(18) unaligned,	/* length of printable generator version name in characters */
//++ 	2 uid_offset bit(18) unaligned,	/* offset of creator id in words rel to base of symbol block */
//++ 	2 uid_length bit(18) unaligned,	/* length of standard Multics id of object creator in characters */
//++ 	2 comment_offset bit(18) unaligned,	/* offset of comment in words relative to base of symbol block */
//++ 	2 comment_length bit(18) unaligned,	/* length of printable generator comment in characters */
//++ 	2 tbound bit(18) unaligned,		/* specifies mod of text section base boundary */
//++ 	2 stat_bound bit(18) unaligned,	/* specifies mod of internal static base boundary */
//++ 	2 source_map bit(18) unaligned,	/* offset relative to base of symbol block of source map structure */
//++ 	2 area_ptr bit(18) unaligned,		/* offset of block info in words relative to base of symbol block */
//++ 	2 symb_base bit(18) unaligned,	/* back pointer (rel to base of symb block) to base of symb section */
//++ 	2 block_size bit(18) unaligned,	/* size in words of entire symbol block */
//++ 	2 next_block bit(18) unaligned,	/* if ^= "0"b, is thread (rel to base of symb section) to next symb block */
//++ 	2 rel_text bit(18) unaligned,		/* offset rel to base of symbol block of text sect relocation info */
//++ 	2 rel_def bit(18) unaligned,		/* offset rel to base of symb block of def section relocation info */
//++ 	2 rel_link bit(18) unaligned,		/* offset rel to base of symb block of link sect relocation info */
//++ 	2 rel_symb bit(18) unaligned,		/* offset rel to base of symb block of symb sect relocation info */
//++ 	2 default_truncate bit(18) unaligned,	/* offset RTBOSB for binder to automatically trunc. symb sect. */
//++ 	2 optional_truncate bit(18) unaligned;	/* offset RTBOSB for binder to optionally truncate symb section */
//++ 
//++ /* END INCLUDE SEGMENT ... symbol_block.incl.pl1 */

typedef struct __attribute__ ((__packed__)) symbol_block
  {
    word36 dcl_version;
    word36 identifier [2];
    word36 gen_number;
    word72 gen_created;
    word72 object_created;
    word36 generator [2];
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint size : 18;
            uint offset : 18;
          } gen_version;
        word36 align1;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint size : 18;
            uint offset : 18;
          } userid;
        word36 align2;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint size : 18;
            uint offset : 18;
          } comment;
        word36 align3;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint stat_boundary : 18;
            uint text_boundary : 18;
          };
        word36 align4;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint area_pointer : 18;
            uint source_map : 18;
          };
        word36 align5;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint block_size : 18;
            uint backpointer : 18;
          };
        word36 align6;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint rel_text : 18;
            uint next_block : 18;
          };
        word36 align7;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint rel_link : 18;
            uint rel_def : 18;
          };
        word36 align8;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint maxi_truncate : 18;
            uint rel_symbol : 18;
          };
        word36 align9;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint unused : 18;
            uint mini_truncate : 18;
          };
        word36 align10;
      };
  } symbol_block;
