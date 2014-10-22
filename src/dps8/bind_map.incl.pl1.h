//++    /* BEGIN INCLUDE SEGMENT ... bind_map.incl.pl1 Michael J. Spier, 4/29/71  */
//++    /* last modified  April, 1972 by M. Weaver */
//++ 
//++ declare    1 bindmap aligned based(bmp),
//++ 
//++    2 dcl_version fixed bin,                /* version number of this structure = 1 */
//++    2 n_components fixed bin,               /* number of entries in this array */
//++    2 component(0 refer(bindmap.n_components)) aligned,
//++      3 name,                               /* objectname of component object */
//++        4 name_ptr bit(18) unaligned,       /* pointer to name string */
//++        4 name_lng bit(18) unaligned,       /* length of name string */
//++      3 comp_name char(8) aligned,  /* name of component's compiler */
//++      3 text_start bit(18) unaligned,       /* text section relocation counter */
//++      3 text_lng bit(18) unaligned, /* length of text section */
//++      3 stat_start bit(18) unaligned,       /* internal static relocation counter */
//++      3 stat_lng bit(18) unaligned, /* length of internal static */
//++      3 symb_start bit(18) unaligned,       /* symbol section relocation counter */
//++      3 symb_lng bit(18) unaligned, /* length of symbol section */
//++      3 defblock_ptr bit(18) unaligned,     /* pointer to components definition block */
//++      3 n_blocks bit(18) unaligned, /* number of symbol blocks the component has */
//++    2 bf_name aligned,                      /* name of bindfile */
//++      3 bf_name_ptr bit(18) unaligned,      /* pointer to name string */
//++      3 bf_name_lng bit(18) unaligned,      /* length of name string */
//++    2 bf_date_up char(24),          /* date updated in archive */
//++    2 bf_date_mod char(24);         /* date last modified */
//++ 
//++ /* END INCLUDE SEGMENT ... bind_map.incl.pl1 */

typedef struct __attribute__ ((__packed__)) component
  {
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint name_len : 18;
            uint name_ptr : 18;
          };
        word36 align1;
      };  
    word36 comp_name [2];
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint text_lng : 18;
            uint text_start : 18;
          };
        word36 align2;
      };  
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint stat_lng : 18;
            uint stat_start : 18;
          };
        word36 align3;
      };  
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint symb_lng : 18;
            uint symb_start : 18;
          };
        word36 align4;
      };  
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint n_blocks : 18;
            uint defblock_ptr : 18;
          };
        word36 align5;
      };  
  } component;

typedef struct __attribute__ ((__packed__)) bindmap
  {
    word36 dcl_version;
    word36 n_components;
    component firstComponent;
  } bindmap;


