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
    spmt_t tree;
    SPMT_INITIALIZE(&tree);

    for (int i = 0 ; i < 100 ; i += 2)
        FILL(i);

    for (int i = 1 ; i < 100 ; i += 2)
        FILL(i);

    SPMT_DUMP((spmt_dump_t)printf, &tree);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &tree);
    SPMT_TO_PDF(&tree, "spmt");
    SPMT_RELEASE(&tree);
    return 0;
}
