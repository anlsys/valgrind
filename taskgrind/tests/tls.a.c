# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: 2 siblings tasks are data-dependent but no dependency expressed

__thread int myVar;

int
main(void)
{
    int * x = (int *) malloc(1 * sizeof(int));
    printf("x addr is %llu\n", (long long unsigned int) x);

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task shared(x)
                myVar = 0;

            # pragma omp task shared(x)
                myVar = 1;
        }

    }

    return 0;
}
