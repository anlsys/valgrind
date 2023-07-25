int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            int x = 0;
            int y = 0;

            # pragma omp task shared(x, y)
            {
                x = 42;
                y = 43;
            }

            # pragma omp task shared(x, y)
            {
                x = 44;
                y = x;
            }

            # pragma omp taskwait

            (void) x;
            (void) y;
        }
    }
    return 0;
}
