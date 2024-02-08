# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: task may access its parent stack after its completion

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
            }
        }
    }

    return 0;
}
