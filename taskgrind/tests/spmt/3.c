# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define SPMT_SPACE (8)
# include "spmt.h"

int main(void)
{
    spmt_t tree;
    SPMT_INITIALIZE(&tree);
    SPMT_FILL(&tree, 0, 3);
    SPMT_FILL(&tree, 6, 7);
    SPMT_DUMP((spmt_dump_t)printf, &tree);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &tree);
    SPMT_RELEASE(&tree);
    return 0;
}
