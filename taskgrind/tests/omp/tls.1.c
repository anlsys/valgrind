// racy: no

_Thread_local int x[2];

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
                x[0] = 0;

            # pragma omp task
                x[0] = 1;
        }
    }
    return 0;
}
