# include <assert.h>
# include <omp.h>
# include <stdio.h>
# include <stdlib.h>
# include <stdint.h>

# define V 42

///////////////////////////////////////////////////////////////////////////////
//  The application
///////////////////////////////////////////////////////////////////////////////

# define Nfor       4
# define Nz         8

# define DEVICE_ID  0

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


            # pragma omp task if(0)
                {}
        }

        # pragma omp for schedule(static, 1)
        for (int i = 0 ; i < Nfor ; ++i)
        {}

        # pragma omp for schedule(dynamic, 1)
        for (int i = 0 ; i < Nfor ; ++i)
        {}

        # pragma omp single nowait
        {
            # pragma omp taskloop num_tasks(Nfor)
            for (int i = 0 ; i < Nfor ; ++i)
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

        #if 0
        # pragma omp single nowait
        {
            int * z = (int *) malloc(sizeof(int) * Nz);
            # pragma omp target enter data map(alloc: z[0:Nz]) device(DEVICE_ID) depend(out: z)

            # pragma omp target teams distribute parallel for nowait device(DEVICE_ID) depend(in: z)
            for (int i = 0 ; i < Nz ; ++i)
                {}

            # pragma omp target exit data map(release: z[0:Nz]) device(DEVICE_ID) depend(out: z)

            # pragma omp taskwait
        }
        #endif


        // TODO: in single thread, LLVM does not seem to raise barrier OMPT callback in parallel region
        # pragma omp barrier
    }
    return 0;
}
