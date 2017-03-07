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

t_stat Read (word18 addr, word36 *dat, _processor_cycle_type cyctyp);
t_stat Read2 (word18 addr, word36 *dat, _processor_cycle_type cyctyp);
t_stat Write (word18 addr, word36 dat, _processor_cycle_type cyctyp);
t_stat Write2 (word18 address, word36 * data, _processor_cycle_type cyctyp);
t_stat Write8 (word18 address, word36 * data);
t_stat Write16 (word18 address, word36 * data);
t_stat Write32 (word18 address, word36 * data);
t_stat Read8 (word18 address, word36 * result);
t_stat Read16 (word18 address, word36 * result);
t_stat WritePage (word18 address, word36 * data);
t_stat ReadPage (word18 address, word36 * result);
t_stat ReadIndirect (void);
