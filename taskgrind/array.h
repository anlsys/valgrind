#ifndef __ARRAY_H__
# define __ARRAY_H__

#include "pub_tool_basics.h"

struct task_s;
struct task_part_s;

// list of tasks
typedef struct  array_s
{
    // tasks
    union {
        UChar * objs;
        struct task_s ** tasks;
        struct task_part_s * parts;
    };

    // capacity
    UInt capacity;

    // number of tasks set
    UInt n;

    // size of an object
    UInt objsize;
}               array_t;

# define ARRAY_INITIALIZE_STATIC {{NULL}, 0, 0}

void array_init(array_t * array, UInt default_capacity, UInt objsize);
void * array_push(array_t * array, void * obj);
void * array_last(array_t * array);
void * array_penultimate(array_t * array);
void * array_first(array_t * array);
int array_is_empty(array_t * array);
void array_clear(array_t * array);
void array_deinit(array_t * array);

# define ARRAY_FOREACH_BEGIN(A, T, X)                   \
    do {                                                \
        for (int X##i = 0 ; X##i < (A)->n ; ++X##i) {   \
            T X = ((T) (A)->objs) + X##i;

# define ARRAY_FOREACH_END(A, T, X)             \
        }                                       \
    } while (0);

#if 0
typedef array_t task_array_t;
void task_array_init(task_array_t * array);
void task_array_push(task_array_t * array, struct task_s * task);
struct task_s * task_array_last(array_t * array);
void task_array_clear(task_array_t * array);
void task_array_deinit(task_array_t * array);

typedef array_t task_part_array_t;
void task_part_array_init(task_part_array_t * array);
struct task_part_s * task_part_array_push(task_part_array_t * array);
struct task_part_s * task_part_array_last(task_part_array_t * array);
void task_part_array_clear(task_part_array_t * array);
void task_part_array_deinit(task_part_array_t * array);
struct task_part_s * task_part_array_first(task_part_array_t * array);
#endif

#endif /* __ARRAY_H__ */
