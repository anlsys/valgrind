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
    puts("----------------------------------------------------");
    SPMT_DUMP((spmt_dump_t)printf, &A);
    puts("----------------------------------------------------");
    SPMT_DUMP_FILLED((spmt_dump_t)printf, &A);

    spmt_t B;
    SPMT_INITIALIZE(&B);
    SPMT_FILL(&B, 137422172216, 137422172224);
//    SPMT_FILL(&B, 137422172228, 137422172232);
//    SPMT_FILL(&B, 137422172464, 137422172472);
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
