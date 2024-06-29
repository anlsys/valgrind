# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                int x[1];
                printf("x addr is %p\n", (void *) x);
                x[0] = 42;
            }

            # pragma omp task
            {
                int x[1];
                printf("x addr is %p\n", (void *) x);
                x[0] = 42;
            }
        }
    }

    return 0;
}
