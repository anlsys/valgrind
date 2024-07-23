// racy: no

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                int x[1];
                x[0] = 42;
            }

            # pragma omp task
            {
                int x[1];
                x[0] = 42;
            }
        }
    }

    return 0;
}
