# include <assert.h>
# include <omp.h>
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

int
main(void)
{
    int * x = (int *) malloc(sizeof(int));
    int * y = (int *) malloc(sizeof(int));

    # pragma omp parallel shared(x, y)
    {
        # pragma omp single nowait
        {
            # pragma omp task shared(x, y) depend(in: x)
            {
                *x = V + 0;
                *y = V + 1;
            }

            # pragma omp task shared(x, y) depend(inoutset: x)
            {
                *x = V + 2;
                *y = *x;
            }

            # pragma omp task
                {}

            # pragma omp taskwait

            # pragma omp task
                {}
        }

        # pragma omp for schedule(static, 1)
        for (int i = 0 ; i < 4 ; ++i)
        {}

        # pragma omp for schedule(dynamic, 1)
        for (int i = 0 ; i < 4 ; ++i)
        {}

        # pragma omp single nowait
        {
            # pragma omp taskloop num_tasks(4)
            for (int i = 0 ; i < 4 ; ++i)
            {}
        }

        # pragma omp sections nowait
        {
            # pragma omp section
            {
            }

            # pragma omp section
            {
            }
        }

        // in single thread, LLVM does not seem to raise it in parallel region
        # pragma omp barrier
    }
    return 0;
}
