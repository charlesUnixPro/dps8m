typedef uint16_t word9;
typedef uint64_t word36;
typedef unsigned int uint;

word36 extr36 (uint8_t * bits, uint woffset);
word9 extr9 (uint8_t * bits, uint coffset);
uint8_t getbit (void * bits, int offset);
uint64_t extr (void * bits, int offset, int nbits);
