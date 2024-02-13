# include <stdatomic.h>
# include <stdio.h>
# include <stdlib.h>

int
main(void)
{
    atomic_int * x = (atomic_int *) malloc(sizeof(atomic_int));
    x[0] = 0;

    int * y = (int *) malloc(sizeof(int));
    y[0] = 0;

    printf("x == %p && y == %p\n", x, y);

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                ++x[0];
                ++y[0];
            }

            # pragma omp task
            {
                ++x[0];
                ++y[0];
            }
        }
    }

    return 0;
}
