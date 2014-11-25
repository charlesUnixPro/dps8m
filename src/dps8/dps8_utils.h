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
#ifndef QUIET_UNUSED
word36 AddSub36 (char op, bool isSigned, word36 op1, word36 op2, word18 flagsToSet, word18 *flags);
#endif
word36 AddSub36b(char op, bool isSigned, word36 op1, word36 op2, word18 flagsToSet, word18 *flags);
word18 AddSub18b(char op, bool isSigned, word18 op1, word18 op2, word18 flagsToSet, word18 *flags);
word72 AddSub72b(char op, bool isSigned, word72 op1, word72 op2, word18 flagsToSet, word18 *flags);
word72 convertToWord72(word36 even, word36 odd);
void convertToWord36(word72 src, word36 *even, word36 *odd);

word36 compl36(word36 op1, word18 *flags);
word18 compl18(word18 op1, word18 *flags);

void copyBytes(int posn, word36 src, word36 *dst);
void copyChars(int posn, word36 src, word36 *dst);

word9 getByte(int posn, word36 src);
void putByte(word36 *dst, word9 data, int posn);
void putChar(word36 *dst, word6 data, int posn);

void cmp36(word36 op1, word36 op2, word18 *flags);
void cmp36wl(word36 A, word36 Y, word36 Q, word18 *flags);
void cmp18(word18 op1, word18 op2, word18 *flags);
void cmp72(word72 op1, word72 op2, word18 *flags);

char *strlower(char *q);
int strmask(char *str, char *mask);

word36 bitfieldInsert36(word36 a, word36 b, int c, int d);
word72 bitfieldInsert72(word72 a, word72 b, int c, int d);
word36 bitfieldExtract36(word36 a, int b, int c);
word72 bitfieldExtract72(word72 a, int b, int c);

int bitfieldInsert(int a, int b, int c, int d);
int bitfieldExtract(int a, int b, int c);
char *bin2text(uint64 word, int n);
void sim_printf( const char * format, ... )    // not really simh, by my impl
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;

#if 0
word36 getbits36 (word36 x, uint i, uint n);
word36 setbits36 (word36 x, uint p, uint n, word36 val);
void putbits36 (word36 * x, uint p, uint n, word36 val);
#else

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
#endif
#define getbits6(x,i) ((word6) (getbits36 ((x), (i), 6) & MASK6))
#define getbits15(x,i) ((word15) (getbits36 ((x), (i), 15) & MASK15))
#define getbits24(x,i) ((word24) (getbits36 ((x), (i), 24) & MASK24))

char * strdupesc (const char * str);


word36 extr36 (uint8 * bits, uint woffset);
word9 extr9 (uint8 * bits, uint coffset);
word18 extr18 (uint8 * bits, uint coffset);
uint8 getbit (void * bits, int offset);
uint64 extr (void * bits, int offset, int nbits);
void put36 (word36 val, uint8 * bits, uint woffset);
int extractWord36FromBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, uint64 *wordp);
int insertWord36toBuffer (uint8 * bufp, t_mtrlnt tbc, uint * words_processed, word36 word);
void print_int128 (__int128_t n, char * p);
uint64 sim_timell (void);
void sim_puts (char * str);
void sim_err (const char * format, ...) NO_RETURN
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;
void sim_printl (const char * format, ...)
#ifdef __GNUC__
  __attribute__ ((format (printf, 1, 2)))
#endif
;
