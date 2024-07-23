// racy: yes

int
main(void)
{
    int x[1];

    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
                x[0] = 42;

            # pragma omp task
                x[0] = 43;
        }
    }

    return 0;
}
