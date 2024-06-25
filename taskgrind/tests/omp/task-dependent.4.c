# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

int
main(void)
{
    int * x = (int *) malloc(4 * sizeof(int));
    printf("x addr is %llu\n", (long long unsigned int) x);

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            for (int i = 0 ; i < 4 ; ++i)
                # pragma omp task shared(x) depend(inoutset: x)
                    x[i] = 42;

            # pragma omp task shared(x) depend(in: x)
                x[0] = 43;
        }
    }

    return 0;
}
