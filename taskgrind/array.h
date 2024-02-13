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
        void ** objs;
        struct task_s ** tasks;
        struct task_part_s ** parts;
    };

    // capacity
    UInt capacity;

    // number of tasks set
    UInt n;

    // size of an object
    UInt objsize;
}               array_t;

# define TASK_ARRAY_INITIALIZE_STATIC {{NULL}, 0, 0}

typedef array_t task_array_t;
typedef array_t task_part_array_t;

void array_init(array_t * array, UInt objsize);
void array_push(task_array_t * array, void * obj);
void * array_last(array_t * array);
void array_clear(array_t * array);
void array_deinit(array_t * array);

struct task_s * task_array_last(task_array_t * array);
void task_array_init(task_array_t * array);

#endif /* __ARRAY_H__ */
