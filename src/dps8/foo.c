#define GETBITS(from,mask,where) \
 (((from) >> (35 - (where))) & (unsigned long long) (mask))
 

#include <stdio.h>

int main (int argc, char * argv [])
  {
    unsigned long long int pattern = 0765432107654LL;
    printf ("7 = %llo\n", GETBITS (pattern, 7, 2));
    printf ("1 = %llo\n", GETBITS (pattern, 1, 0));
    printf ("7654 = %llo\n", GETBITS (pattern, 07777, 11));
    printf ("7654 = %llo\n", GETBITS (pattern, 07777, 35));
    return 0;
  }
