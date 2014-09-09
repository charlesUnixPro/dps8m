// Taken from bound_ti_term_.s.archive/make_object_map_.pl1

// dcl     1 object_glop based(p) aligned,
//          2 idwords(0:3) bit(36) aligned,
//          2 textrel fixed bin(35),
//          2 textbc fixed bin(35),
//          2 linkrel fixed bin(35),
//          2 linkbc fixed bin(35),
//          2 symbolrel fixed bin(35),
//          2 symbolbc fixed bin(35),
//          2 maprel fixed bin(35);
typedef struct __attribute__ ((__packed__)) object_glop
  {
    word36  idwords [4];
    word36  textrel;
    word36  textbc;
    word36  linkrel;
    word36  linkbc;
    word36  symbolrel;
    word36  symbolbc;
    word36  maprel;
  } object_glop;

