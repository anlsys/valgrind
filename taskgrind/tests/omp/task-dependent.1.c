# include <stdlib.h>

int
main(void)
{
    int * x = (int *) malloc(1 * sizeof(int));

    # pragma omp parallel
    {
        # pragma omp single
        {
            # pragma omp task
                x[0] = 42;

            # pragma omp task
                x[0] = 43;
        }
    }

    return 0;
}
