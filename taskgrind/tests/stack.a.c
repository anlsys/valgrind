# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: 2 siblings tasks are data-dependent but no dependency expressed

int
main(void)
{
    int x[1];
    printf("x addr is %p\n", (void *) x);

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task shared(x)
                x[0] = 42;

            # pragma omp task shared(x)
                x[0] = 43;
        }
    }

    return 0;
}
