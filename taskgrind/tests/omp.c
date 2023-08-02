# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

# define V 42

///////////////////////////////////////////////////////////////////////////////
//  Symbols for taskgrind, in case environment functions are explicitely
//  declared by the programmer
///////////////////////////////////////////////////////////////////////////////

// uint64_t
// __taskgrind_get_current_task_id(void)
// {
//     return (i++ / 10000);   // just a simple trick to test taskgrind
// }

///////////////////////////////////////////////////////////////////////////////
//  The application
///////////////////////////////////////////////////////////////////////////////

void
toto(int * x, int * y)
{
    printf("&toto=%p\n", toto);
    printf("x=%p, y=%p, &x=%p, &y=%p\n", x, y, &x, &y);
}

int
main(void)
{
    int * x = (int *) malloc(sizeof(int));
    int * y = (int *) malloc(sizeof(int));
    toto(x, y);
    # pragma omp parallel shared(x, y)
    {
        # pragma omp single
        {
            # pragma omp task shared(x, y)
            {
                *x = V + 0;
                *y = V + 1;
            }

            # pragma omp task shared(x, y) depend(in: x)
            {
                *x = V + 2;
                *y = *x;
            }

            # pragma omp taskwait

            printf("x=%d, y=%d\n", *x, *y);
        }
    }
    return 0;
}
