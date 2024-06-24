# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: 2 siblings tasks are data-dependent but no dependency expressed

_Thread_local int x[4096];
// extern _Thread_local int y[1024];

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
                x[0] = 0;

            # pragma omp task
                x[0] = 1;
        }

    }

    return 0;
}
