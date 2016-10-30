/*
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

#if 0
// relocation codes
enum relocationCodes
{
    relAbsolute     = 0,    // Absolute - does not relocate (a)
    relText         = 020,  // Text - uses text section relocation counter (0)
    relTextN        = 021,  // Similar to text (cf. AK92-2 pg. 1-24) (1)
    relLink18       = 022,  // (2)
    relLink18N      = 023,  // (3)
    relLink15       = 024,  // (4)
    relDefinition   = 025,  // (5)
    relSymbol       = 026,  // (6)
    relSymbolN      = 027,  // (7)
    relInt18        = 030,  // (8)
    relInt15        = 031,  // (9)
    relSelf         = 032,  // L
    relExpAbs       = 036,
    relEscape       = 037   // (*)
};
#endif

#define LOT "lot_"

struct segdef          // definitions for externally available symbols
{
    char    *symbol;    ///< name of externallay available symbol
    int     value;      ///< address of value in segment
    int     relType;    ///< relocation type (RFU)
    
    int     segno;      ///< when filled-in is the segment # where the segdef is found (default=-1)
    
    struct segdef  *next;
    struct segdef  *prev;
};
typedef struct segdef segdef;

struct segref      // references to external symbols in this segment
{
    char    *segname;   ///< name of segment external symbol resides
    char    *symbol;    ///< name of extern symbol
    int     value;      ///< address of ITS pair in segment
    int     offset;     ///< if ext reference is an offset from segname/symbol (display purposes only for now)
    int     relType;    ///< relocation type (RFU)
    
    int     segno;      ///< when filled-in is the segment # where the segref is to be found (default=-1)
    
    bool    snapped;    ///< true when link has been filled in with a correct ITS pointer
    
    struct segref  *next;
    struct segref  *prev;
};
typedef struct segref segref;

struct segment
{
    char    *name;  ///< name of this segment
    word36  *M;     ///< contents of this segment
    int     size;   ///< size of this segment in 36-bit words
    
    segdef *defs;   ///< symbols available to other segments
    segref *refs;   ///< external symbols needed by this segment
    
    bool    deferred; ///< if true segment is deferred, not loaded into memory
    
    int     segno;  ///< segment# segment is assigned
    int     ldaddr; ///< address where to load segment
    
    int     linkOffset; ///< link offset in segment
    int     linkSize;   ///< size of segments linkage section

    // For symbolic debugging support
    char    *filename;
    
    struct segment *next;
    struct segment *prev;
};
typedef struct segment segment;

int removeSegment(char *seg);
int removeSegdef(char *seg, char *sym);
int removeSegref(char *seg, char *sym);
int resolveLinks(bool);
int loadDeferredSegments(bool);
int getAddress(int segno, int offset);  // return the 24-bit absolute address of segment + offset
bool getSegmentAddressString(int addr, char *msg);
t_stat createLOT(bool);     // create link offset table segment
t_stat snapLOT(bool);       // fill in link offset table segment
t_stat createStack(int, bool);    // create ring n stack
char * lookupSegmentAddress (word18 segno, word18 offset, char * * compname, word18 * compoffset);

segdef *findSegdef(char *seg, char *sgdef);
segdef *findSegdefNoCase(char *seg, char *sgdef);
segment *findSegment(char *segname);
segment *findSegmentNoCase(char *segname);  // same as above, but case insensitive

