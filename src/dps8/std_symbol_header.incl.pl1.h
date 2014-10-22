//++  dcl       1 std_symbol_header     based aligned,
//++    2 dcl_version   fixed bin,
//++    2 identifier    char(8),
//++    2 gen_number    fixed bin,
//++    2 gen_created   fixed bin(71),
//++    2 object_created        fixed bin(71),
//++    2 generator     char(8),
//++    2 gen_version   unaligned,
//++      3 offset      bit(18),
//++      3 size                bit(18),
//++    2 userid                unaligned,
//++      3 offset      bit(18),
//++      3 size                bit(18),
//++    2 comment               unaligned,
//++      3 offset      bit(18),
//++      3 size                bit(18),
//++    2 text_boundary bit(18) unaligned,
//++    2 stat_boundary bit(18) unaligned,
//++    2 source_map    bit(18) unaligned,
//++    2 area_pointer  bit(18) unaligned,
//++    2 backpointer   bit(18) unaligned,
//++    2 block_size    bit(18) unaligned,
//++    2 next_block    bit(18) unaligned,
//++    2 rel_text      bit(18) unaligned,
//++    2 rel_def               bit(18) unaligned,
//++    2 rel_link      bit(18) unaligned,
//++    2 rel_symbol    bit(18) unaligned,
//++    2 mini_truncate bit(18) unaligned,
//++    2 maxi_truncate bit(18) unaligned;

typedef struct __attribute__ ((__packed__)) std_symbol_header
  {
    word36 dcl_version;
    word36 identifier [2];
    word36 gen_number;
    word36 gen_created [2];
    word36 object_created [2];
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
            uint rel_symbol : 18;
            uint rel_def : 18;
          };
        word36 align8;
      };

    union
      {
        struct __attribute__ ((__packed__))
          {
            uint maxi_truncate : 18;
            uint mini_truncate : 18;
          };
        word36 align9;
      };
  } std_symbol_header;


