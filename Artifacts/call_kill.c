//#include<sys/types.h>
//#include<signal.h>
#include <stdio.h>

int call_kill(short pid, short sig) {
  return fprintf(stderr, "%d, %u\n", pid, sig);
}
