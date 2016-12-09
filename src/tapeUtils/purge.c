/*
 Copyright 2013-2016 by Charles Anthony

 All rights reserved.

 This software is made available under the terms of the
 ICU License -- ICU 1.8.1 and later.
 See the LICENSE file at the top-level directory of this distribution and
 at https://sourceforge.net/p/dps8m/code/ci/master/tree/LICENSE
 */

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

#define LEN1 2238
#define TEST1 "\r\r\n\r\n*/\r\n                                          -----------------------------------------------------------\r\n\r\n\r\nHistorical Background\r\n\r\n"

#define LEN2 2294
#define TEST2 "&\r\n&\r\n&                                          -----------------------------------------------------------\r\n&\r\n& \r\n& \r\n& Historical Background\r\n& \r\n"

#define LEN3 2227
#define TEST3 "\r\n\r\n                                          -----------------------------------------------------------\r\n\r\n\r\nHistorical Background\r\n\r\n"

#define LEN4 2283
#define TEST4 "\"\r\n\"\r\n\"                                          -----------------------------------------------------------\r\n\"\r\n\"\r\n\"\r\n\" Historical Background\r\n\"\r\n"

#define LEN5 2322
//#define TEST5 "\r\f\r\n\r\n\r\n\r\n\t\t    bull_copyright_notice.txt       08/30/05  1008.4r   08/30/05  1007.3    00020025\r\n\r\n                                          -----------------------------------------------------------"
#define TEST5 "\n\r\f\r\n\r\n\r\n\r\n\t\t    bull_copyright_notice.txt"

// bcpl
#define LEN6 2312
#define TEST6 "//\r\n//\r\n//                                          -----------------------------------------------------------\r\n//\r\n//\r\n// Historical Background\r\n"

// lisp
#define LEN7 2314
#define TEST7 ";;\r\n;;\r\n;;\r\n;;                                          -----------------------------------------------------------\r\n;;\r\n;;\r\n;; Historical Background\r\n"

// gcos job deck
#define LEN8 2613
#define TEST8 "$\tcomment\t\t\r\n$\tcomment\t\t\r\n$\tcomment\t\t                                          -----------------------------------------------------------\r\n$\tcomment\t\t\r\n$\tcomment\t\t\r\n$\tcomment\t\t\r\n$\tcomment\t\tHistorical Background\r\n" 

// bos .ascii
#define LEN9 2278
#define TEST9 "*\r\n*\r\n*                                         -----------------------------------------------------------\r\n*\r\n*\r\n* Historical Background\r\n"

// fortran
#define LEN10 2295
#define TEST10 "c\r\nc\r\nc                                          -----------------------------------------------------------\r\nc\r\nc \r\nc \r\nc Historical Background\r\n"

// some .archive files
#define LEN11 2320
#define TEST11 "\f\r\n\r\n\r\n\r\n\t\t    bull_copyright_notice.txt       08/30/05  1008.4r   08/30/05  1007.3    00020025\r\n\r\n                                          -----------------------------------------------------------\r\n\r\n\r\nHistorical Background\r\n" 

// 355 .asm
#define LEN12 2280
#define TEST12 "$\r\n$\r\n$                                          -----------------------------------------------------------\r\n$\r\n$\r\n$ Historical Background\r\n"

// must come after 1; is subset
#define LEN13 2237
#define TEST13 "\r\n\r\n*/\r\n                                          -----------------------------------------------------------\r\n\r\n\r\nHistorical Background\r\n\r\n"

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
        return 1;
      }
//else for (int i = 0; i < strlen (teststring); i ++) if (buffer [i] != teststring [i]) printf ("%d %o %o\n", i, buffer [i], teststring [i]);
    return 0;
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

    if (test (LEN1, TEST1))
      {
        //printf ("P1: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN2, TEST2))
      {
        //printf ("P2: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN3, TEST3))
      {
        //printf ("P3: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN4, TEST4))
      {
        //printf ("P4: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN5, TEST5))
      {
        //printf ("P5: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN6, TEST6))
      {
        //printf ("P6: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN7, TEST7))
      {
        //printf ("P7: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN8, TEST8))
      {
        //printf ("P8: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN9, TEST9))
      {
        //printf ("P9: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN10, TEST10))
      {
        //printf ("P10: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN11, TEST11))
      {
        //printf ("P11: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN12, TEST12))
      {
        //printf ("P12: %s\n", argv [1]);
        goto done;
      }

    if (test (LEN13, TEST13))
      {
        //printf ("P13: %s\n", argv [1]);
        goto done;
      }



    lseek (fd, 0, SEEK_SET);

    while (1)
      {
        uint8_t ch;
        ssize_t sz = read (fd, & ch, 1);
        if (sz == 0)
          {
            break;
          }
        if (sz != 1)
          {
            perror ("read search");
            exit (1);
          }
        if (ch == '\r')
         {
           printf ("XX: %s\n", argv [1]);
           break;
         }
     }
    
done:;
    close (fd);
  }
