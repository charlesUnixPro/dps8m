/*
 Copyright 2014-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

//#define USE_SID
// shared memory library

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "shm.h"

//#include "dps8.h"
//#include "dps8_utils.h"
#include "shm.h"

void * create_shm (char * key, size_t size)
  {
    void * p;
    char buf [256];
    sprintf (buf, "dps8m.%s", key);
    int fd = open (buf, O_RDWR | O_CREAT, 0600);
    if (fd == -1)
      {
        printf ("create_shm open fail %d\r\n", errno);
        return NULL;
      }

    if (ftruncate (fd, (off_t) size) == -1)
      {
        printf ("create_shm  ftruncate  fail %d\r\n", errno);
        return NULL;
      }

    p = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (p == MAP_FAILED)
      {
        printf ("create_shm mmap  fail %d\r\n", errno);
        return NULL;
      }
    return p;
  }

void * open_shm (char * key, size_t size)
  {
    void * p;
    char buf [256];
    sprintf (buf, "dps8m.%s", key);
    int fd = open (buf, O_RDWR, 0600);
    if (fd == -1)
      {
        printf ("open_shm open fail %d\r\n", errno);
        return NULL;
      }

    p = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED)
      {
        close (fd);
        printf ("open_shm mmap  fail %d\r\n", errno);
        return NULL;
      }
    return p;
  }
