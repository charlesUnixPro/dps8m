// Purge the "Historical Background" from segments to make comparing easier

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// trailer length

#define LEN 2237
#define TEST "\r\n\r\n*/\r\n                                          -----------------------------------------------------------\r\n\r\n\r\nHistorical Background\r\n\r\n"

#define LEN2 2294
#define TEST2 "&\r\n&\r\n&                                          -----------------------------------------------------------\r\n&\r\n& \r\n& \r\n& Historical Background\r\n& \r\n"

#define LEN3 2227
#define TEST3 "\r\n\r\n                                          -----------------------------------------------------------\r\n\r\n\r\nHistorical Background\r\n\r\n"

#define LEN4 2283
#define TEST4 "\"\r\n\"\r\n\"                                          -----------------------------------------------------------\r\n\"\r\n\"\r\n\"\r\n\" Historical Background\r\n\"\r\n"

static off_t flen = 0;
static int fd;
static int test (size_t len, char * teststring)
  {
    // If file is too short, we're done
    if (flen < len)
     return 0;

    off_t os2 = lseek (fd, flen - len, SEEK_SET);
    if (os2 == -1)
      {
        perror ("lseek 2");
        exit (1);
      }

    uint8_t buffer [len];
    ssize_t sz = read (fd, buffer, len);
    if (sz != len)
      {
        printf ("%ld %ld %ld\n", flen, sz, len);
        perror ("read");
        exit (1);
      }

    if (memcmp (buffer, teststring, strlen (teststring)) == 0)
      {
        //printf ("match\n");
        int rc = ftruncate (fd, flen - len);
        if (rc < 0)
          {
            perror ("ftruncate");
            exit (1);
          }
      }
//else for (int i = 0; i < strlen (teststring); i ++) if (buffer [i] != teststring [i]) printf ("%d %o %o\n", i, buffer [i], teststring [i]);
    return 1;
  }

int main (int argc, char * argv [])
  {
    if (argc < 2)
      {
        printf ("purge filename\n");
        exit (1);
      }
    fd = open (argv [1], O_RDWR);
    if (fd < 0)
      {
        perror ("open");
        exit (1);
      }
    flen = lseek (fd, 0, SEEK_END);
    if (flen == -1)
      {
        perror ("lseek 1");
        exit (1);
      }

    if (test (LEN, TEST))
      {
        printf ("P1: %s\n", argv [1]);
        goto done;
      }
    if (test (LEN2, TEST2))
      {
        printf ("P2: %s\n", argv [1]);
        goto done;
      }
    if (test (LEN3, TEST3))
      {
        printf ("P3: %s\n", argv [1]);
        goto done;
      }
    if (test (LEN4, TEST4))
      {
        printf ("P4: %s\n", argv [1]);
        goto done;
      }

    printf ("XX: %s\n", argv [1]);
    
done:;
    close (fd);
  }
