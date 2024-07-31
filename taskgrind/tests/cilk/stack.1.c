// racy: yes - both accessing parent stack
#include<cilk/cilk.h>


int
main(void)
{
    int x[1];

    cilk_spawn x[0] = 42;
    x[0] = 43;

    cilk_sync; 

    return 0;
}
