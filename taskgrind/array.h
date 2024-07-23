#ifndef __ARRAY_H__
# define __ARRAY_H__

#include "pub_tool_basics.h"

struct task_s;
struct task_seg_s;

// list of tasks
typedef struct  array_s
{
    // tasks
    union {
        UChar * objs;
        struct task_s ** tasks;
        struct task_seg_s * segs;
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
void * array_get(array_t * array, int n);
int array_is_empty(array_t * array);
void array_clear(array_t * array);
void array_deinit(array_t * array);

# define ARRAY_FOREACH_FROM_BEGIN(A, I, T, X)           \
    do {                                                \
        for (int X##i = I ; X##i < (A)->n ; ++X##i) {   \
            T X = ((T) (A)->objs) + X##i;

# define ARRAY_FOREACH_FROM_END(A, I, T, X)     \
        }                                       \
    } while (0);

# define ARRAY_FOREACH_BEGIN(A, T, X) ARRAY_FOREACH_FROM_BEGIN(A, 0, T, X)
# define ARRAY_FOREACH_END(A, T, X)   ARRAY_FOREACH_FROM_END(A, 0, T, X)

#endif /* __ARRAY_H__ */
