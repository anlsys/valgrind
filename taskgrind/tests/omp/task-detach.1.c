// racy: no

# include <omp.h>

int
main(void)
{
    int x[1];

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            omp_event_handle_t hdl;
            # pragma omp task depend(out: x) detach(hdl)
            {
                x[0] = 42;
            }

            omp_fulfill_event(hdl);

            # pragma omp taskwait depend(in: x)

            x[0] = 42;

        }
    }

    return 0;
}
