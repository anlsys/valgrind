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
            # pragma omp task shared(x) detach(hdl) depend(out: x)
                x[0] = 42;

            # pragma omp task shared(hdl)
                omp_fulfill_event(hdl);

            # pragma omp task depend(in: x) if(0)
                {}

            x[0] = 43;
        }
    }

    return 0;
}
