# include "array.h"
# include "task.h"

// GENERIC
// TODO: optimize this

void
array_init(array_t * array, UInt default_capacity, UInt objsize)
{
    array->capacity = default_capacity;
    array->n        = 0;
    array->objsize  = objsize;
    array->objs     = default_capacity ? VG_(malloc)("array_init", objsize * default_capacity) : NULL;
    tl_assert(!default_capacity || array->objs);
}

void
array_deinit(array_t * array)
{
    VG_(free)(array->tasks);
    array->n        = 0;
    array->capacity = 0;
}

void *
array_push(array_t * array, void * src)
{
    if (array->n == array->capacity)
    {
        UInt capacity = (UInt)((array->n + 1) * 3 / 2);
        array->objs = VG_(realloc)("array_t", array->objs, array->objsize * capacity);
        array->capacity = capacity;
    }

    void * dst = array->objs + array->n * array->objsize;
    if (src)
        VG_(memcpy)(dst, src, array->objsize);
    ++array->n;
    return dst;
}

void *
array_last(array_t * array)
{
    if (array->n == 0)
        return NULL;
    return (void *) (array->tasks + (array->n - 1) * array->objsize);
}

void
array_clear(array_t * array)
{
    array->n = 0;
}

// struct task_s *
void
task_array_init(task_array_t * array)
{
    array_init(array, 0, sizeof(struct task_s *));
}

void
task_array_push(task_array_t * array, struct task_s * task)
{
    array_push(array, &task);
}

struct task_s *
task_array_last(task_array_t * array)
{
    struct task_s ** tasks = array_last(array);
    return tasks ? tasks[0] : NULL;
}

void
task_array_clear(task_array_t * array)
{
    array_clear(array);
}

// struct task_part_s
void
task_part_array_init(task_part_array_t * array)
{
    array_init(array, 0, sizeof(struct task_part_s));
}

struct task_part_s *
task_part_array_push(task_part_array_t * array)
{
    return array_push(array, NULL);
}

struct task_part_s *
task_part_array_last(task_part_array_t * array)
{
    return array_last(array);
}

struct task_part_s *
task_part_array_first(task_part_array_t * array)
{
    return (struct task_part_s *) (array->n == 0 ? NULL : array->parts);
}
