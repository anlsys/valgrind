# include <stdlib.h>
# include <stdio.h>

static int X = 0;

int main(void)
{
    # pragma omp parallel
    {
        # pragma omp single
        {
            // MT-A
            # pragma omp task depend(out: X)
            {
                printf("starting MT-A\n");
                for (int i = 0 ; i < 4 ; ++i)
                {
                    # pragma omp task firstprivate(i) depend(out: x[i])
                    {
                        printf("i am MT-A - T-%d\n", i);
                    }
                }
                # pragma omp taskwait
                printf("finishing MT-A\n");
            }

            // MT-B
            # pragma omp task depend(in: X)
            {
                printf("starting MT-B\n");
                for (int i = 0 ; i < 4 ; ++i)
                {
                    # pragma omp task firstprivate(i) depend(out: x[i])
                    {
                        printf("i am MT-B - T-%d\n", i);
                    }
                }
                printf("finishing MT-B\n");
            }
        }
    }
    return 0;
}
