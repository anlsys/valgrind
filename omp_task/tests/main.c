int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            int x; (void) x;

            # pragma omp task shared(x)
                x = 42;

            # pragma omp task shared(x)
                x = 43;

            # pragma omp taskwait
        }
    }
    return 0;
}
