# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// error : 'B' depends on 'A' while they are data-parallel

int
main(void)
{
    int * x = (int *) malloc(2 * sizeof(int));

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task depend(out: x)
                x[0] = 42;

            # pragma omp task depend(in: x)
                x[1] = 43;
        }
    }

    return 0;
}
