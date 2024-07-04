# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

int
main(void)
{
    int * x = (int *) malloc(1 * sizeof(int));
    printf("x addr is %llu\n", (long long unsigned int) x);

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            omp_event_handle_t hdl;
            # pragma omp task shared(x) depend(out: x) detach(hdl)
                x[0] = 42;


            # pragma omp task depend(in: x)
                {}

            omp_fulfill_event(hdl);
        }
    }

    return 0;
}
