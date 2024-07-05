int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            int x; (void) x;

            # pragma omp task depend(out: x)
            {}

            # pragma omp task depend(in: x)
            {}
        }
    }
    return 0;
}
