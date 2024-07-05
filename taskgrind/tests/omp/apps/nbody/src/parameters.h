#ifndef PARAMETERS_H
# define PARAMETERS_H

/***********************/
/* YOU CAN MODIFY HERE */
/***********************/

/* simulation times */
# define N_ITERS        10

/* number of bodies in total */
# define N_BODIES       16384 // 8192

/* number of tasks per loop */
extern int TASKS_PER_LOOP;
extern int BODIES_PER_TASK;

/* body mass interval */
# define MASS_MIN       5.972 * 10e24           /* kg.  */
# define MASS_MAX       (10.0 * MASS_MIN)       /* kg.  */

/* body max distance from origin */
# define MAX_DISTANCE   4e12     /* m. */

/************************/
/* DO NOT MODIFY BELLOW */
/************************/
# define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
# define G  (6.67408 * 10e-11)  /* N.m^2.kg^(-2)    */

# define T0 0.0             /* s. */
# define TF 24 * 60 * 60    /* s. */
# define DT ((TF - T0) / N_ITERS)

/* position check */
# define EXPECTED_CHECKSUM      3364147359171.906738
# define EPSILON                1e-0

#endif /* PARAMETERS_H */
