// run with OMP_NUM_THREADS=1
// should report determinacy race on x[0]

int
main(void)
{
    int x[1];

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task shared(x)
                x[0] = 42;

            # pragma omp task shared(x)
                x[0] = 43;
        }
    }

    return 0;
}
