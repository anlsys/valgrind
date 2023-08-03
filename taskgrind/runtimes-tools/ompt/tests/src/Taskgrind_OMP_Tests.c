# include <stdio.h>
# include <omp.h>

# define N 16384

int main(void)
{
    # pragma omp parallel for
    for (int i = 0 ; i < N ; ++i)
    {
        for (int j = 0 ; j < i ; ++j)
        {
        }
    }

    # pragma omp parallel for
    for (int i = 0 ; i < N ; ++i)
    {
        for (int j = i ; j < N ; ++j)
        {
        }
    }

    return 0;
}

