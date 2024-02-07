# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

static int * x;

void toto(int * x, int i, int v)
{
    x[i] = v;
}

int
main(void)
{
    x = (int *) malloc(2 * sizeof(int));
    printf("&x[0] == %p\n", x);

    # pragma omp parallel
    {
        assert(omp_get_num_threads() == 1);

        # pragma omp single nowait
        {
            # pragma omp task depend(out: x)
                toto(x, 0, 42);

            # pragma omp task depend(in: x)
                toto(x, 1, 43);
        }
    }

    printf("x[0]=%d, x[1]=%d\n", x[0], x[1]);

    return 0;
}
