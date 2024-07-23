// racy: yes

int
main(void)
{
    # pragma omp parallel
    {
        # pragma omp single nowait
        {
            # pragma omp task
            {
                int x = 0;

                # pragma omp task shared(x)
                    x = 1;

                x = 2;

                # pragma omp taskwait
            }
        }
    }

    return 0;
}
