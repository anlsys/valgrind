# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# define SPMT_ADDR_T long long unsigned int
# include "spmt.h"

# define FILL(T, X, Y)      \
    do {                    \
        SPMT_FILL(T, X, Y); \
    } while (0)


int
main(void)
{
    spmt_t A, B;
    SPMT_INITIALIZE(&A);
    SPMT_INITIALIZE(&B);

    FILL(&A, 0xC5FFB40,     0xC5FFB48);
    FILL(&A, 0xC3D773C,     0xC3D7748);
    FILL(&A, 0x1FFEFFEF7C,  0x1FFEFFEFA8);
    FILL(&A, 0x1FFEFFEF70,  0x1FFEFFEF78);
    FILL(&A, 0x1FFEFFEFAC,  0x1FFEFFEFC0);

    FILL(&B, 0x1FFEFFEF7C, 0x1FFEFFEFA8);
    FILL(&B, 0x1FFEFFEF70, 0x1FFEFFEF78);
    FILL(&B, 0x1FFEFFEFAC, 0x1FFEFFEFB8);
    FILL(&B, 0xC3D773C,    0xC3D7740);

    SPMT_DUMP((spmt_dump_t)printf, &A);
    SPMT_DUMP((spmt_dump_t)printf, &B);

    int n = 5;
    interval_t intervals[5];
    int r = 0;
//    r += SPMT_INTERSECT(intervals, n, &A, &B);
    r += SPMT_INTERSECT(intervals, n, &B, &A);
    for (int i = 0 ; i < n ; ++i)
        printf("intervals[%d] = [%p, %p]\n", i, (void *) intervals[i].a, (void *) intervals[i].b);
    printf("r=%d\n", r);

    SPMT_TO_PDF(&A, "spmt-A");
    SPMT_TO_PDF(&B, "spmt-B");

    SPMT_RELEASE(&A);
    SPMT_RELEASE(&B);

    return 0;
}
