int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {}

            # pragma omp task
            {}
        }
    }
    return 0;
}
