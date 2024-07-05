#ifndef SIMULATE_H
# define SIMULATE_H

void update_forces(void);
void update_acceleration(void);
void update_velocity(double dt);
void update_position(double dt);

void init(void);
void deinit(void);

void simulate(void);

#endif /* SIMULATE_H */
