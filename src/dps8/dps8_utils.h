// Interface for cfgparse

typedef struct config_value_list
  {
    const char * value_name;
    int64_t value;
  } config_value_list_t;

typedef struct config_list
  {
    const char * name; // opt name
    int64_t min, max; // value limits
    config_value_list_t * value_list;
  } config_list_t;

typedef struct config_state
  {
    char * copy;
    char * statement_save;
  } config_state_t;

int cfgparse (const char * tag, char * cptr, config_list_t * clist, config_state_t * state, int64_t * result);
void cfgparse_done (config_state_t * state);

struct opCode *getIWBInfo(DCDstruct *i);
char * dumpFlags(word18 flags);
char *disAssemble(word36 instruction);
char *getModString(int32 tag);
word72 convertToWord72(word36 even, word36 odd);
void convertToWord36(word72 src, word36 *even, word36 *odd);

word36 compl36(word36 op1, word18 *flags, bool * ovf);
word18 compl18(word18 op1, word18 *flags, bool * ovf);

void copyBytes(int posn, word36 src, word36 *dst);
void copyChars(int posn, word36 src, word36 *dst);

#ifndef QUIET_UNUSED
word9 getByte(int posn, word36 src);
#endif
void putByte(word36 *dst, word9 data, int posn);
void putChar(word36 *dst, word6 data, int posn);

void cmp36(word36 op1, word36 op2, word18 *flags);
void cmp36wl(word36 A, word36 Y, word36 Q, word18 *flags);
void cmp18(word18 op1, word18 op2, word18 *flags);
void cmp72(word72 op1, word72 op2, word18 *flags);

char *strlower(char *q);
int strmask(char *str, char *mask);
char *Strtok(char *, char *);
char *stripquotes(char *s);
char *trim(char *s);
char *ltrim(char *s);
char *rtrim(char *s);

//word36 bitfieldInsert36(word36 a, word36 b, int c, int d);
//word72 bitfieldInsert72(word72 a, word72 b, int c, int d);
//word36 bitfieldExtract36(word36 a, int b, int c);
//word72 bitfieldExtract72(word72 a, int b, int c);

//int bitfieldInsert(int a, int b, int c, int d);
//int bitfieldExtract(int a, int b, int c);
char *bin2text(uint64 word, int n);
void sim_printf( const char * format, ... )    // not really simh, by my impl
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;


//  getbits36 (data, starting bit, number of bits)

static inline word36 getbits36(word36 x, uint i, uint n) {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)n+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36: bad args (%012llo,i=%d,n=%d)\n", x, i, n);
        return 0;
    } else
        return (x >> (unsigned) shift) & ~ (~0U << n);
}

static inline word1 getbits36_1 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_1: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 01;
  }

static inline word2 getbits36_2 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)2+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_2: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 03;
  }

static inline word3 getbits36_3 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)3+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_3: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 07;
  }

static inline word4 getbits36_4 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)4+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_4: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 017;
  }

static inline word5 getbits36_5 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)5+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_5: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 037;
  }

static inline word6 getbits36_6 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)6+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_6: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 077;
  }

static inline word7 getbits36_7 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)7+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_7: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 0177;
  }

static inline word8 getbits36_8 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)8+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_8: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 0377;
  }

static inline word9 getbits36_9 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)9+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_9: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 0777;
  }

static inline word10 getbits36_10 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)10+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_10: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 01777;
  }

static inline word12 getbits36_12 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)12+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_12: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 07777;
  }

static inline word14 getbits36_14 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)14+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_14: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 037777;
  }

static inline word15 getbits36_15 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)15+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_15: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 077777;
  }

static inline word16 getbits36_16 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)16+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_16: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 0177777;
  }

static inline word18 getbits36_18 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)18+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_18: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 0777777;
  }

static inline word24 getbits36_24 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)24+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_24: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & MASK24;
  }

static inline word28 getbits36_28 (word36 x, uint i)
  {
    // bit 35 is right end, bit zero is 36th from the right
    int shift = 35-(int)i-(int)28+1;
    if (shift < 0 || shift > 35) {
        sim_printf ("getbits36_28: bad args (%012llo,i=%d)\n", x, i);
        return 0;
    } else
        return (x >> (unsigned) shift) & 01777777777;
  }

static inline word36 setbits36(word36 x, uint p, uint n, word36 val)
{
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35) {
        sim_printf ("setbits36: bad args (%012llo,pos=%d,n=%d)\n", x, p, n);
        return 0;
    }
    word36 mask = ~ (~0U<<n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    word36 result = (x & ~ mask) | ((val&MASKBITS(n)) << (36 - p - n));
    return result;
}

static inline void putbits36 (word36 * x, uint p, uint n, word36 val)
  {
    int shift = 36 - (int) p - (int) n;
    if (shift < 0 || shift > 35)
      {
        sim_printf ("putbits36: bad args (%012llo,pos=%d,n=%d)\n", * x, p, n);
        return;
      }
    word36 mask = ~ (~0U << n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    * x = (* x & ~mask) | ((val & MASKBITS (n)) << (36 - p - n));
    return;
  }

static inline void putbits72 (word72 * x, uint p, uint n, word72 val)
  {
    int shift = 72 - (int) p - (int) n;
    if (shift < 0 || shift > 71)
      {
        sim_printf ("putbits72: bad args (pos=%d,n=%d)\n", p, n);
        return;
      }
    word72 mask = ~ ((~(word72)0) << n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    * x = (* x & ~mask) | ((val & MASKBITS72 (n)) << (72 - p - n));
    return;
  }


//  getbits18 (data, starting bit, number of bits)

static inline word36 getbits18 (word18 x, uint i, uint n)
  {
    // bit 17 is right end, bit zero is 18th from the right
    int shift = 17 - (int) i - (int) n + 1;
    if (shift < 0 || shift > 17)
      {
        sim_printf ("getbits18: bad args (%06o,i=%d,n=%d)\n", x, i, n);
        return 0;
      }
    else
      return (x >> (unsigned) shift) & ~ (~0U << n);
  }

static inline void putbits18 (word18 * x, uint p, uint n, word18 val)
  {
    int shift = 18 - (int) p - (int) n;
    if (shift < 0 || shift > 17)
      {
        sim_printf ("putbits18: bad args (%012o,pos=%d,n=%d)\n", * x, p, n);
        return;
      }
    word18 mask = ~ (~0U << n);  // n low bits on
    mask <<= (unsigned) shift;  // shift 1s to proper position; result 0*1{n}0*
    // caller may provide val that is too big, e.g., a word with all bits
    // set to one, so we mask val
    * x = (* x & ~mask) | ((val & MASKBITS18 (n)) << (18 - p - n));
    return;
  }

char * strdupesc (const char * str);


word36 extr36 (uint8 * bits, uint woffset);
#ifndef QUIET_UNUSED
word9 extr9 (uint8 * bits, uint coffset);
word18 extr18 (uint8 * bits, uint coffset);
uint64 extr (void * bits, int offset, int nbits);
#endif
uint8 getbit (void * bits, int offset);
void put36 (word36 val, uint8 * bits, uint woffset);
int extractASCII36FromBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 *wordp);
int extractWord36FromBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, uint64 *wordp);
int insertASCII36toBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 word);
int insertWord36toBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 word);
void print_int128 (__int128_t n, char * p);
uint64 sim_timell (void);
void sim_puts (char * str);
#if 0
void sim_err (const char * format, ...) NO_RETURN
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;
#endif
void sim_printl (const char * format, ...)
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;
#if 0
void sim_warn (const char * format, ...)
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;
#endif
void sim_printl (const char * format, ...)
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;

word36 Add36b (word36 op1, word36 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf);
word36 Sub36b (word36 op1, word36 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf);
word36 Add18b (word18 op1, word18 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf);
word18 Sub18b (word18 op1, word18 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf);
word72 Add72b (word72 op1, word72 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf);
word72 Sub72b (word72 op1, word72 op2, word1 carryin, word18 flagsToSet, word18 * flags, bool * ovf);

#ifdef EISTESTJIG
t_stat eisTest (UNUSED int32 arg, char *buf);
#endif
