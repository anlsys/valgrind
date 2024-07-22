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
                int x = 0;

                # pragma omp task shared(x)
                    x = 1;

                # pragma omp taskwait

                x = 2;
            }
        }
    }

    return 0;
}
