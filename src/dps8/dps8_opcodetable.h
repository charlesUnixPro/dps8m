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
#ifdef PANEL
extern word8 insGrp [01000], eisGrp [01000];
// CPT 3U 0-35, 3L 0-17
enum { GRP_UNKN =   0,
       GRP_FXDML =  1,  // Fixed-Point Data Movement Load
       GRP_FXDMS =  2,  // Fixed-Point Data Movement Store
       GRP_FXDMR =  3,  // Fixed-Point Data Movement Shift
       GRP_FXA =    4,  // Fixed-Point Addition
       GRP_FXS =    5,  // Fixed-Point Subtraction
       GRP_FXM =    6,  // Fixed-Point Multiplication
       GRP_FXD =    7,  // Fixed-Point Division
       GRP_FXN =    8,  // Fixed-Point Negate
       GRP_FXC =    9,  // Fixed-Point Comparision
       GRP_FXI =   10,  // Fixed-Point Miscellaneous
       GRP_BA =    11,  // Boolean And
       GRP_BO =    12,  // Boolean Or
       GRP_BE =    13,  // Boolean Exclusive Or
       GRP_BCA =   14,  // Boolean Comparative And
       GRP_BCN =   15,  // Boolean Comparative Not
       GRP_FLDML = 16,  // Floating-Point Data Movement Load
       GRP_FLDMS = 17,  // Floating-Point Data Movement Store
       GRP_FLA =   18,  // Floating-Point Addition
       GRP_FLS =   19,  // Floating-Point Subtraction
       GRP_FLM =   20,  // Floating-Point Multiplication
       GRP_FLD =   21,  // Floating-Point Division
       GRP_FLN =   22,  // Floating-Point Negate
       GRP_FLNOR = 23,  // Floating-Point Normalize
       GRP_FLR =   24,  // Floating-Point Round
       GRP_FLC =   25,  // Floating-Point Compare
       GRP_FLI =   26,  // Floating-Point Miscellaneous
       GRP_TRA =   27,  // Transfer
       GRP_PRDML = 28,  // Pointer Register Data Movement Load
       GRP_PRDMS = 29,  // Pointer Register Data Movement Store
       GRP_PRAA =  30,  // Pointer Register Address Arithmetic
       GRP_PRM =   31,  // Pointer Register Miscellaneous
       GRP_MISC =  32,  // Miscellaneous
       GRP_PRL =   33,  // Privileged - Register Load
       GRP_PRS =   34,  // Privileged - Register Store
       GRP_PCAM =  35,  // Privileged - Clear Associative Memory
       GRP_PCS =   36,  // Privileged - Configuration and Status
       GRP_PSC =   37,  // Privileged - System Control
       GRP_PM =    38,  // Privileged - Miscellaneous
       GRP_EARL =  39,  // EIS - Address Register Load
       GRP_EARS =  40,  // EIS - Address Register Store
       GRP_EARSA = 41,  // EIS - Address Register Special Arithmetic
       GRP_EANC =  42,  // EIS - Alphanumeric Compare
       GRP_EANM =  43,  // EIS - Alphanumeric Move
       GRP_ENC =   44,  // EIS - Numeric Compare
       GRP_ENM =   45,  // EIS - Numeric Move
       GRP_EBCN =  46,  // EIS - Bit String Combine
       GRP_EBCR =  47,  // EIS - Bit String Compare
       GRP_EBSI =  48,  // EIS - Bit String Set Indicators
       GRP_EDC =   49,  // EIS - Data Conversion
       GRP_EDA =   50,  // EIS - Decimal Addition
       GRP_EDS =   51,  // EIS - Decimal Subtration
       GRP_EDM =   52,  // EIS - Decimal Multiplication
       GRP_EDD =   53,  // EIS - Decimal Divison
};
#endif
