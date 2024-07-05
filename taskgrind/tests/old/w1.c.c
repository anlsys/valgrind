# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// no error, all good

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            int x;

            # pragma omp task depend(out: x) shared(x)
                x = 42;

            # pragma omp task depend(in: x) shared(x)
                x = 43;

            # pragma omp taskwait
        }
    }

    return 0;
}
