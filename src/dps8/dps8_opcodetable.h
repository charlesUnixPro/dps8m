
extern char GEBcdToASCII[64];   ///< GEBCD => ASCII map
#ifndef QUIET_UNUSED
extern char ASCIIToGEBcd[128];  ///< ASCII => GEBCD map
#endif

#ifndef QUIET_UNUSED
extern char *op0text[512];
extern char *op1text[512];
extern char *opcodes2text[1024];
#endif

struct adrMods {
    const char *mod;    ///< mnemonic
    int   Td;           ///< Td value
    int   Flags;
};
typedef struct adrMods adrMods;

extern struct adrMods extMods[0100]; ///< extended address modifiers
extern struct opCode NonEISopcodes[01000], EISopcodes[01000];
