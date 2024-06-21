# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "spmt-v2.h"

# define FILL(X)                    \
    do {                            \
        SPMT_FILL(&tree, X, X+1);   \
    } while (0)


int
main(void)
{
    srand(2024);

    spmt_t tree;
    SPMT_INITIALIZE(&tree);

    int N = 1000;
    for (int i = 0 ; i < N ; ++i)
    {
        int x = rand() / (double)RAND_MAX * N;
        FILL(x);
    }

    SPMT_DUMP((spmt_dump_t)printf, &tree);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &tree);
    SPMT_TO_PDF(&tree, "spmt");
    SPMT_RELEASE(&tree);
    return 0;
}
