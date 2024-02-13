# include "array.h"
# include "task.h"

// GENERIC

void
array_init(array_t * array, UInt objsize)
{
    array->tasks    = NULL;
    array->capacity = 0;
    array->n        = 0;
    array->objsize  = objsize;
}

void
array_push(task_array_t * array, void * obj)
{
    if (array->n == array->capacity)
    {
        UInt capacity = (UInt)((array->n + 1) * 3 / 2);
        array->objs = VG_(realloc)("array_t", array->objs, array->objsize * capacity);
        array->capacity = capacity;
    }
    array->objs[array->n++] = obj;
}

void *
array_last(array_t * array)
{
    if (array->n == 0)
        return NULL;
    return array->tasks[array->n - 1];
}

void
array_clear(array_t * array)
{
    array->n = 0;
}

void
array_deinit(array_t * array)
{
    VG_(free)(array->tasks);
    array->n        = 0;
    array->capacity = 0;
}

// task_t
task_t *
task_array_last(task_array_t * array)
{
    return (task_t *) array_last(array);
}

void
task_array_init(task_array_t * array)
{
    array_init(array, sizeof(task_t));
}


