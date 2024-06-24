# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: 2 siblings tasks are data-dependent but no dependency expressed

_Thread_local int x[1];

int
main(void)
{
    int * y = (int *) malloc(sizeof(int));
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task shared(y)
            {
                x[0] = 0;
                y[0] = 0;
            }

            # pragma omp task shared(y)
            {
                x[0] = 1;
                y[0] = 1;
            }
        }

    }

    return 0;
}
