/*
 Copyright 2014-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

void * create_shm (char * key, pid_t system_pid, size_t size);
void * open_shm (char * key, pid_t system_pid, size_t size);

