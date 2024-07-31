// racy: yes - both accessing parent stack
#include<cilk/cilk.h>


int x[2]; 

int
main(void)
{

    cilk_spawn { x[0] = 42; }
    x[1] = 43;

    cilk_sync; 

    return x[0]; 
}
