# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define SPMT_SPACE (8)
# include "spmt.h"

// Non empty intersection

int main(void)
{
    spmt_t A;
    SPMT_INITIALIZE(&A);
    SPMT_FILL(&A, 0, 7);
    puts("----------------------------------------------------");
    SPMT_DUMP(printf, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED(printf, &A);

    spmt_t B;
    SPMT_INITIALIZE(&B);
    SPMT_FILL(&B, 1, 8);
    puts("----------------------------------------------------");
    SPMT_DUMP(printf, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED(printf, &B);

    spmt_t DST;
    SPMT_INTERSECT(&DST, &A, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP(printf, &DST);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED(printf, &DST);

    SPMT_F_ASSERT(!SPMT_IS_EMPTY(&DST));

    SPMT_RELEASE(&A);
    SPMT_RELEASE(&B);
    SPMT_RELEASE(&DST);

    return 0;
}
