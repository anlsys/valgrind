# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// all good, no problem

int
main(void)
{
    int * x = (int *) malloc(1 * sizeof(int));

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                int y = 42;
                # pragma omp task
                    x[0] = y;

                # pragma omp taskwait
            }
        }
    }

    return 0;
}
