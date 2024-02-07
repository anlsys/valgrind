# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

void toto(int * x, int i)
{
    x[i] = 1;
}

int
main(void)
{
    int * x = (int *) malloc(2 * sizeof(int));
    printf("&x[0] == %p\n", x);

    # pragma omp parallel shared(x)
    {
        assert(omp_get_num_threads() == 1);

        # pragma omp single nowait
        {
            # pragma omp task shared(x) depend(in: x)
                toto(x, 0);

            # pragma omp task shared(x) depend(out: x)
                toto(x, 1);
        }
    }
    return 0;
}
