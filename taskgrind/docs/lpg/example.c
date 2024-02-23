int
main(void)
{
    int x;

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task shared(x)
                x = 42;

            # pragma omp task shared(x)
                x = 43;
        }
    }

    return 0;
}
