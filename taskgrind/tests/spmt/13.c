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
    SPMT_FILL(&A, 0, 1);
    SPMT_FILL(&A, 2, 3);
    SPMT_FILL(&A, 4, 5);
    SPMT_FILL(&A, 6, 7);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &A);

    spmt_t B;
    SPMT_INITIALIZE(&B);
    SPMT_FILL(&B, 1, 2);
    SPMT_FILL(&B, 3, 4);
    SPMT_FILL(&B, 5, 6);
    SPMT_FILL(&B, 7, 8);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &B);

    SPMT_UNION(&A, &A, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &A);

    SPMT_F_ASSERT(!SPMT_IS_EMPTY(&A));

    SPMT_RELEASE(&A);
    SPMT_RELEASE(&B);

    return 0;
}
