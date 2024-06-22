# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define SPMT_SPACE (8)
# include "spmt.h"

// Empty intersection

int main(void)
{
    spmt_t A;
    SPMT_INITIALIZE(&A);
    SPMT_FILL(&A, 0, 4);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &A);

    spmt_t B;
    SPMT_INITIALIZE(&B);
    SPMT_FILL(&B, 4, 8);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &B);

    spmt_t DST;
    SPMT_INITIALIZE(&DST);
    SPMT_INTERSECT(&DST, &A, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &DST);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &DST);

    SPMT_F_ASSERT(SPMT_IS_EMPTY(&DST));

    SPMT_RELEASE(&A);
    SPMT_RELEASE(&B);
    SPMT_RELEASE(&DST);

    return 0;
}
