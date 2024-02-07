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
    int y;
    x = (int *) malloc(2 * sizeof(int));
    printf("&x[0] == %p ; &x == %p ; &y == %p\n", x, &x, &y);

    # pragma omp parallel
    {
        assert(omp_get_num_threads() == 1);

        # pragma omp single nowait
        {
            # pragma omp task depend(out: x)
                x[0] = 42;

            # pragma omp task depend(in: x)
                x[1] = 43;
        }
    }

    printf("x[0]=%d, x[1]=%d\n", x[0], x[1]);

    return 0;
}
