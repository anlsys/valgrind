# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: no error

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                int * x = (int *) malloc(1 * sizeof(int));
                x[0] = 42;
            }

            # pragma omp task
            {
                int * x = (int *) malloc(1 * sizeof(int));
                x[0] = 42;
            }
        }
    }

    return 0;
}
