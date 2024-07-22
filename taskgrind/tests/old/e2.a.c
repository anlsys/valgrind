# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: 2 non-siblings tasks are data-dependent but no dependency expressed

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
            }

            # pragma omp task
            {
                # pragma omp task
                    x[0] = 43;
            }
        }
    }

    return 0;
}
