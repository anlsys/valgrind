# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define OFFSET     (64)
# define STEP       (8)
# define N          (128)
# define N_BLOCK    (3)
# define SPMT_SPACE (OFFSET * 2 + STEP * N * N_BLOCK)

# include "spmt.h"

int main(void)
{
    spmt_t tree;
    SPMT_INITIALIZE(&tree);
    for (int i = 0 ; i < N ; ++i)
    {
        for (int j = 0 ; j < N_BLOCK ; ++j)
        {
            int a = OFFSET + j * N * STEP + i * STEP;
            int b = a + STEP;
            SPMT_FILL(&tree, a, b);
        }
    }

    SPMT_DUMP((spmt_dump_t)printf, &tree);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &tree);
    SPMT_RELEASE(&tree);
    return 0;
}
