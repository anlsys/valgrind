# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>
# include <unistd.h>

// should report: 2 siblings tasks are data-dependent but no dependency expressed
_Thread_local int x[2];

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                x[0] = 0;
                printf("x addr is %p\n", x);
            }

            # pragma omp task
            {
                x[0] = 1;
                printf("x addr is %p\n", x);
            }
        }
    }

    return 0;
}
