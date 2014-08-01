#include <stdio.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <stdint.h>

typedef uint64_t t_uint64;
typedef uint64_t word36;

#include "dps8_mp.h"

multipassStats * multipassStatsPtr;

int main (int argc, char * argv [])
  {
    int mpStatsSegID = shmget (0x6180 /* + switches . cpu_num */, 
                               sizeof (multipassStats),
                               S_IRUSR | S_IWUSR);
    if (mpStatsSegID == -1)
      {
        perror ("multipassInit shmget");
        return 1;
      }
    multipassStatsPtr = (multipassStats *) shmat (mpStatsSegID, 0, 0);
    if (multipassStatsPtr == (void *) -1)
      {
        perror ("multipassInit shmat");
        return 1;
      }
  }

