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
    VG_(free)(array->objs);
    array->n        = 0;
    array->capacity = 0;
}

void *
array_push(array_t * array, void * src)
{
    if (array->n == array->capacity)
    {
        UInt capacity = (UInt)((array->capacity < 4) ? 4 : (array->capacity * 2));
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
array_get(array_t * array, int n)
{
    if (n < 0 || n >= array->n)
        return NULL;
    return (void *) (array->objs + n * array->objsize);
}

void *
array_last(array_t * array)
{
    return array_get(array, array->n - 1);
}

void *
array_penultimate(array_t * array)
{
    return array_get(array, array->n - 2);
}

void *
array_first(array_t * array)
{
    return array_get(array, 0);
}

int
array_is_empty(array_t * array)
{
    return array->n == 0;
}

void
array_clear(array_t * array)
{
    array->n = 0;
}
