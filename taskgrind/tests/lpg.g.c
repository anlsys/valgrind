# define N 2

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            int x;
            (void) x;

            for (int i = 0 ; i < N ; ++i)
            {
                # pragma omp task depend(inoutset: x)
                {}
            }

            for (int i = 0 ; i < N ; ++i)
            {
                # pragma omp task depend(out: x)
                {}
            }

            # pragma omp taskwait
        }
    }
    return 0;
}
