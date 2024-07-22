#ifndef BODIES_H
# define BODIES_H

/************************/
/* BODIES DEFINITION    */
/************************/
# include <omp.h>
# include <math.h>
# include <stdio.h>

# include "parameters.h"

typedef struct
{
    double x, y, z;
} vec3_t;

typedef struct
{
    double mass;
    vec3_t forces;
    vec3_t acceleration;
    vec3_t velocity;
    vec3_t position;
} body_t;

extern body_t bodies[N_BODIES];

void bodies_init(void);
double bodies_checksum(void);

#endif /* BODIES_H */
