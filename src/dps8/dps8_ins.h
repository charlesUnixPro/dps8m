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

void tidy_cu (void);
void cu_safe_store(void);
#ifdef MATRIX
void initializeTheMatrix (void);
void addToTheMatrix (uint32 opcode, bool opcodeX, bool a, word6 tag);
#endif
t_stat displayTheMatrix (int32 arg, const char * buf);
t_stat prepareComputedAddress (void);   // new
void cu_safe_restore(void);
void fetchInstruction(word18 addr);
t_stat executeInstruction (void);
void doRCU (void) NO_RETURN;
void traceInstruction (uint flag);
bool tstOVFfault (void);
bool chkOVF (void);
