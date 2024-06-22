# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define SPMT_SPACE (1099511627776)
# include "spmt.h"

// Non empty intersection

int main(void)
{
    spmt_t A;
    SPMT_INITIALIZE(&A);
    SPMT_FILL(&A, 67293856, 67293860);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &A);

    spmt_t B;
    SPMT_INITIALIZE(&B);
    SPMT_FILL(&B, 67293860, 67293864);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &B);

    spmt_t C;
    SPMT_INITIALIZE(&C);

    SPMT_APPEND(&C, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &C);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &C);

    SPMT_APPEND(&C, &B);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &C);
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &C);

    SPMT_F_ASSERT(!SPMT_IS_EMPTY(&C));

    SPMT_RELEASE(&A);
    SPMT_RELEASE(&B);
    SPMT_RELEASE(&C);

    return 0;
}
