int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            int x;
            (void) x;

            # pragma omp task depend(out: x)
            {}

            # pragma omp task depend(inoutset: x)
            {}

            # pragma omp taskwait
        }
    }
    return 0;
}
