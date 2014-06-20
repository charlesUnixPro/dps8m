//++ 
//++ dcl	1 pl1_symbol_block	aligned based,
//++ 	2 version		fixed bin,
//++ 	2 identifier	char(8),		/* must be "pl1info" */
//++ 	2 flags,
//++ 	  3 profile	bit(1) unal,
//++ 	  3 table		bit(1) unal,
//++ 	  3 map		bit(1) unal,
//++ 	  3 flow		bit(1) unal,
//++ 	  3 io		bit(1) unal,
//++ 	  3 table_removed	bit(1) unal,
//++ 	  3 long_profile	bit(1) unal,
//++ 	  3 pad		bit(29) unal,
//++ 	2 greatest_severity	fixed bin,
//++ 	2 root		unal bit(18),
//++ 	2 profile		unal bit(18),
//++ 	2 map		unal,
//++ 	  3 first		bit(18),
//++ 	  3 last		bit(18),
//++ 	2 segname		unaligned,
//++ 	  3 offset	bit(18),
//++ 	  3 size		bit(18);


typedef struct __attribute__ ((__packed__)) pl1_symbol_block
  {
    word36 version;
    word36 identifier [2];
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint pad : 1;
            uint long_profile : 1;
            uint table_removed : 1;
            uint io : 1;
            uint flow : 1;
            uint map : 1;
            uint table : 1;
            uint profile : 1;
          } flags;
        word36 align1;
      };
    word36 greatest_severity [2];
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint profile : 18;
            uint root : 18;
          };
        word36 align2;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint last : 18;
            uint first : 18;
          } map;
        word36 align3;
      };
    union
      {
        struct __attribute__ ((__packed__))
          {
            uint length : 18;
            uint offset : 18;
          } segname;
        word36 align4;
      };

  } pl1_symbol_block;
