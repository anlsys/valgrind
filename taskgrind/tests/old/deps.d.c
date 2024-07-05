# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

// should report: no errors

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                int * x = (int *) malloc(1 * sizeof(int));
//                printf("x==%p\n",x);
                x[0] = 42;
                free(x);
            }

            # pragma omp task
            {
                int * x = (int *) malloc(1 * sizeof(int));
//                printf("x==%p\n",x);
                x[0] = 42;
                free(x);
            }
        }
    }

    return 0;
}
