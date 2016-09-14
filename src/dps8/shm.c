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

//#include "dps8.h"
//#include "dps8_utils.h"
#include "shm.h"

#define MAX_SEGS 256
static int nsegs = 0;
static char * seglist [MAX_SEGS];

static void cleanup (void)
  {
    //printf ("cleanup\n");
    for (int i = 0; i < nsegs; i ++)
      {
        //printf ("  unlink %s\n", seglist [i]);
        shm_unlink (seglist [i]);
      }
  }

static void addseg (char * name)
  {
    if (nsegs >= MAX_SEGS)
      return;
    seglist [nsegs ++] = strdup (name);
    if (nsegs == 1)
     atexit (cleanup);
  }

void * create_shm (char * key, pid_t system_pid, size_t size)
  {
    void * p;
    char buf [256];
    sprintf (buf, "/dps8m.%u.%s", system_pid, key);
    shm_unlink (buf);
    int fd = shm_open (buf, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1)
      {
#if 1
        //sim_printf ("create_shm shm_open fail %d\n", errno);
        return NULL;
#else
        // It already exists; assume that there was an emulator crash,
        // and just open it
//printf ("create_shm EXCL shm_open fail %d\n", errno);
        fd = shm_open (buf, O_RDWR, S_IRUSR | S_IWUSR);
        if (fd == -1)
          {
            close (fd);
            //printf ("open_shm shm_open fail %d\n", errno);
            return NULL;
          }
#endif
      }

    if (ftruncate (fd, (off_t) size) == -1)
      {
        //sim_printf ("create_shm  ftruncate  fail %d\n", errno);
        return NULL;
      }

    p = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (p == MAP_FAILED)
      {
        //sim_printf ("create_shm mmap  fail %d\n", errno);
        return NULL;
      }
    addseg (buf);

    return p;
  }

void * open_shm (char * key, pid_t system_pid, size_t size)
  {
    void * p;
    char buf [256];
    sprintf (buf, "/dps8m.%u.%s", system_pid, key);
    // Try to create it; it sucessful, then something is wrong, it should
    // already exist
    int fd = shm_open (buf, O_CREAT | O_RDWR | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd != -1)
      {
        close (fd);
        shm_unlink (buf);
        //printf ("open_shm shm_open fail %d\n", errno);
        return NULL;
      }

    fd = shm_open (buf, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1)
      {
        //printf ("open_shm shm_open fail %d\n", errno);
        return NULL;
      }

    if (ftruncate (fd, (off_t) size) == -1)
      {
        //printf ("open_shm ftruncate  fail %d\n", errno);
        close (fd);
        return NULL;
      }

    p = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED)
      {
        close (fd);
        //printf ("open_shm mmap  fail %d\n", errno);
        return NULL;
      }
    //addseg (buf);
    return p;
  }
