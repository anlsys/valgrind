# include <assert.h>
# include <stdio.h>
# include <sys/time.h>

# include "body.h"
# include "parameters.h"
# include "simulate.h"

# include <omp.h>

int TASKS_PER_LOOP = 1;
int BODIES_PER_TASK = N_BODIES;

int
main(int argc, char ** argv)
{
    if (argc > 2)
    {
        fprintf(stderr, "usage: %s [TASKS_PER_LOOP]\n", argv[0]);
        return 1;
    }

    if (argc == 1)
    {
        printf("usage: %s [TASKS_PER_LOOP]\n", argv[0]);
    }

    /* initialize bodies */
    bodies_init();

    /* initialize openmp runtime */
    int nthreads;
    # pragma omp parallel
    {
        # pragma omp single
        {
            nthreads = omp_get_num_threads();
        }
    }
    TASKS_PER_LOOP = (argc == 2) ? atoi(argv[1]) : nthreads;
    BODIES_PER_TASK = N_BODIES / TASKS_PER_LOOP;

    printf("OpenMP threads: %d\n", nthreads);
    printf("TASKS_PER_LOOP: %d%s\n", TASKS_PER_LOOP, TASKS_PER_LOOP == nthreads ? " (default)" : "");
    printf("Inital position: %lf\n", bodies_checksum());

    /* simulation */
    init();
    double t0 = omp_get_wtime();
    simulate();
    double tf = omp_get_wtime();
    deinit();
    printf("Simulation took %lf s.\n", tf - t0);
    printf("Final position: %lf\n", bodies_checksum());
    printf("Final position diff: %lf\n", fabs(bodies_checksum() - EXPECTED_CHECKSUM));

    /* check result */
    if (fabs(bodies_checksum() - EXPECTED_CHECKSUM) >= EPSILON)
    {
        fprintf(stderr, "========================\n");
        fprintf(stderr, "= Error in calculation =\n");
        fprintf(stderr, "========================\n");
        return 1;
    }

    return 0;
}
