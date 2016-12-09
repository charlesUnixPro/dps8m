/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

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
