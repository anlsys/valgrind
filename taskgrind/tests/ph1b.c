# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: nothing

int
main(void)
{
    int * x = (int *) malloc(2 * sizeof(int));

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
                x[0] = 42;

            # pragma omp task
                x[1] = 43;
        }
    }

    return 0;
}
