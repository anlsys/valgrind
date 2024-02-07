# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define SPMT_SPACE (8)
# include "spmt.h"

int main(void)
{
    spmt_t A;
    SPMT_INITIALIZE(&A);
    SPMT_FILL(&A, 0, 8);
    puts("----------------------");
    SPMT_DUMP(printf, &A);

    spmt_t B;
    SPMT_INITIALIZE(&B);
    SPMT_FILL(&B, 0, 8);
    puts("----------------------");
    SPMT_DUMP(printf, &B);

    spmt_t DST;
    SPMT_INTERSECT(&DST, &A, &B);
    puts("----------------------");
    SPMT_DUMP(printf, &DST);

    return 0;
}
