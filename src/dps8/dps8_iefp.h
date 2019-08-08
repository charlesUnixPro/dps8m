/*
 Copyright (c) 2007-2013 Michael Mondy
 Copyright 2012-2016 by Harry Reed
 Copyright 2013-2016 by Charles Anthony
 Copyright 2015 by Eric Swenson

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

void Read (word18 addr, word36 *dat, processor_cycle_type cyctyp);
void Read2 (word18 addr, word36 *dat, processor_cycle_type cyctyp);
void Write (word18 addr, word36 dat, processor_cycle_type cyctyp);
void Write2 (word18 address, word36 * data, processor_cycle_type cyctyp);
#ifdef CWO
void Write1 (word18 address, word36 data, bool isAR);
#endif
void Write8 (word18 address, word36 * data, bool isAR);
void Write16 (word18 address, word36 * data);
void Write32 (word18 address, word36 * data);
void Read8 (word18 address, word36 * result, bool isAR);
void Read16 (word18 address, word36 * result);
void WritePage (word18 address, word36 * data, bool isAR);
void ReadPage (word18 address, word36 * result, bool isAR);
void ReadIndirect (void);
