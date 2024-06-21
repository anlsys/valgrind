# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define SPMT_SPACE (8)
# include "spmt.h"

// Empty intersection

static int
f(SPMT_PTR_T begin, SPMT_PTR_T end, void * opaque)
{
    SPMT_F_ASSERT(opaque == NULL);
    SPMT_F_ASSERT(begin % 2     == 0);
    SPMT_F_ASSERT(end - begin   == 1);
    printf("[%lu, %lu[\n", begin, end);
    return 0;
}

int main(void)
{
    spmt_t A;
    SPMT_INITIALIZE(&A);
    SPMT_FILL(&A, 0, 1);
    SPMT_FILL(&A, 2, 3);
    SPMT_FILL(&A, 4, 5);
    SPMT_FILL(&A, 6, 7);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &A);

    SPMT_FOREACH_FILLED(&A, f, NULL);

    SPMT_RELEASE(&A);

    return 0;
}
