# include <stdlib.h>

# include "body.h"

body_t bodies[N_BODIES];

# define SEED 0xB0D1

void
bodies_init(void)
{
    srand(SEED);
    for (int i = 0 ; i < N_BODIES ; ++i)
    {
        double rx = (double) rand();
        double ry = (double) rand();
        double rz = (double) rand();
        double r = sqrt(rx * rx + ry * ry + rz * rz);
        double d = MAX_DISTANCE - 2 * rand() / (double) RAND_MAX * MAX_DISTANCE;

        body_t * body = bodies + i;

        body->acceleration.x = 0.0;
        body->acceleration.y = 0.0;
        body->acceleration.z = 0.0;

        body->velocity.x = 0.0;
        body->velocity.y = 0.0;
        body->velocity.z = 0.0;

        body->position.x = d / r * rx;
        body->position.y = d / r * ry;
        body->position.z = d / r * rz;

        double m = rand() / (double) RAND_MAX;
        body->mass = MASS_MIN + m * (MASS_MAX - MASS_MIN);
    }
}

double
bodies_checksum(void)
{
    return bodies[0].position.x;
}
