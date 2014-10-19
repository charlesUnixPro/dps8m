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

typedef struct __attribute__ ((__packed__)) initialization_info
  {
    word36 n_words;
    word36 code;
    word36 info [];
  } initialization_info;
//++ 
//++ declare   map_ptr bit(18) aligned based;          /* Last word of the segment. It points to the base of the object map. */
//++ 
//++ declare   object_map_version_2 fixed bin static init(2);

#define object_map_version_2 2

//++ 
//++ /* END INCLUDE FILE ... object_map.incl.pl1 */
