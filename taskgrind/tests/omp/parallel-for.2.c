# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

int
main(void)
{
    int * x = (int *) malloc(1 * sizeof(int));
    printf("x addr is %llu\n", (long long unsigned int) x);

    # pragma omp parallel
    {
        # pragma omp for
        for (int i = 0 ; i < 1 ; ++i)
            x[0] = 42;
    }

    return 0;
}
