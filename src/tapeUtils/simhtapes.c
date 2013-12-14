#include <unistd.h>
#include <stdint.h>

#include "simhtapes.h"

// ret: >0 sizeread
//      0 tapemark 
//      -1 eof
//      -2 file format error
//      -3 buffer overrun

int read_simh_blk (int fd, void * buf, ssize_t buflen)
  {
    ssize_t sz;
    uint32_t blksiz, blksiz2;

    sz = read (fd, & blksiz, sizeof (blksiz));

    if (sz == 0)
      return -1;

    if (sz != sizeof (blksiz))
      {
        //printf ("can't read blksiz\n");
        return -2;
      }

    //printf ("blksiz %d\n", blksiz);

    if (! blksiz)
      {
        //printf ("tapemark\n");
        return 0;
      }

    if (blksiz <= buflen)
      {
        sz = read (fd, buf, blksiz);
        if (sz != blksiz)
          {
            //printf ("can't read blk\n");
            return -2;
          }
      }
    else
      lseek (fd, blksiz, SEEK_CUR);


    sz = read (fd, & blksiz2, sizeof (blksiz2));

    if (sz != sizeof (blksiz2))
      {
        //printf ("can't read blksiz2\n");
        return -2;
      }

    if (blksiz != blksiz2)
      {
        //printf ("can't sync\n");
        return -2;
      }
    if (blksiz > buflen)
      return -3; // buffer overrun

    return blksiz;
  }
