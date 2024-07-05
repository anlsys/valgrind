# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: no errors

int
main(void)
{
    volatile int done = 0;

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            while (done < 2 && omp_get_thread_num() != 0);

            # pragma omp task
            {
                int * x = (int *) malloc(1 * sizeof(int));
                printf("x==%p\n",x);
                x[0] = 42;
                free(x);
                ++done;
            }

            # pragma omp task
            {
                int * x = (int *) malloc(1 * sizeof(int));
                printf("x==%p\n",x);
                x[0] = 42;
                free(x);
                ++done;
            }
        }
    }

    return 0;
}
