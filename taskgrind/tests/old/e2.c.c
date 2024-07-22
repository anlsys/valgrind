# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// All good, no problem

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
                # pragma omp task
                    x[0] = 42;

                # pragma omp taskwait
            }

            # pragma omp taskwait

            # pragma omp task
            {
                # pragma omp task
                    x[0] = 43;
            }
        }
    }

    return 0;
}
