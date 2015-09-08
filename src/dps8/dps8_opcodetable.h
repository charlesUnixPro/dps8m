
extern const char GEBcdToASCII[64];   ///< GEBCD => ASCII map
#ifndef QUIET_UNUSED
extern const char ASCIIToGEBcd[128];  ///< ASCII => GEBCD map
#endif

#ifndef QUIET_UNUSED
extern const char *op0text[512];
extern const char *op1text[512];
extern const char *opcodes2text[1024];
#endif

struct adrMods {
    const char *mod;    ///< mnemonic
    int   Td;           ///< Td value
    int   Flags;
};
typedef struct adrMods adrMods;

extern const struct adrMods extMods[0100]; ///< extended address modifiers
extern const struct opCode NonEISopcodes[01000], EISopcodes[01000];
