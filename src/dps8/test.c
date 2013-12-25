#include <stdio.h>

static void  t (unsigned int opcode, unsigned int rn)
  {
    unsigned int n = (opcode & 03) | ((opcode & 020) >> 2);
    if (n != rn)
      printf ("%o %d %d\n", opcode, rn, n);
  }

int main (int argc, char * argv [])
  {
    t (0350, 0);
    t (0351, 1);
    t (0352, 2);
    t (0353, 3);
    t (0370, 4);
    t (0371, 5);
    t (0372, 6);
    t (0373, 7);
  }
