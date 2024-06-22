# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "spmt-v2.h"

# define FILL(T, X)             \
    do {                        \
        SPMT_FILL(&T, X, X+1);  \
    } while (0)


int
main(void)
{
    srand(2024);

    spmt_t A, B, C;
    SPMT_INITIALIZE(&A);
    SPMT_INITIALIZE(&B);
    SPMT_INITIALIZE(&C);

    int x;
    int N = 100;
    for (int i = 0 ; i < N ; ++i)
    {
        x = rand() / (double)RAND_MAX * N;
        FILL(A, x);

        x = rand() / (double)RAND_MAX * N;
        FILL(B, x);
    }

    SPMT_APPEND(&C, &A);
    SPMT_APPEND(&C, &B);

    // SPMT_DUMP((spmt_dump_t)printf, &tree);
    puts("----------------------------------------------------");
    // SPMT_DUMP_FILLED((spmt_dump_t)printf, &tree);
    SPMT_TO_PDF(&A, "spmt-a");
    SPMT_TO_PDF(&B, "spmt-b");
    SPMT_TO_PDF(&C, "spmt-aub");

    SPMT_RELEASE(&A);
    SPMT_RELEASE(&B);
    SPMT_RELEASE(&C);

    return 0;
}
