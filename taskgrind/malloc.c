# include "pub_tool_basics.h"           /* ThreadId */
# include "pub_tool_replacemalloc.h"    /* cli_malloc */
# include "pub_tool_libcassert.h"       /* tl_assert */

# include "print.h"

static inline
void * alloc_and_record(ThreadId tid, SizeT size, SizeT align, Bool zero)
{
    void * p = VG_(cli_malloc)(align, size);
    if (p == NULL)
        return NULL;

    if (zero)
        VG_(memset)(p, 0, size);

    SizeT actual_size = VG_(cli_malloc_usable_size)(p);
    tl_assert(actual_size >= size);
    SizeT slop_size = actual_size - size;

    // TODO
    // record_block(tid, p, size, slop_szB, /*exclude_first_entry*/True, /*maybe_snapshot*/True);

    return p;
}

static inline
void * realloc_and_record(ThreadId tid, void * p, SizeT size)
{
    // TODO : instead of realloc, we should here 'malloc' a new pointer, and
    // copy data Currently, realloc may recycle memory that can lead to
    // false-positive
    void * pp = VG_(cli_realloc)(p, size);

    // TODO
    // record_block(tid, p, size, slop_szB, /*exclude_first_entry*/True, /*maybe_snapshot*/True);

    return pp;
}

void *
taskgrind_malloc(ThreadId tid, SizeT n)
{
//    TASKGRIND_DEBUG("malloc");
    return alloc_and_record(tid, n, VG_(clo_alignment), 0);
}

void *
taskgrind___builtin_new(ThreadId tid, SizeT n)
{
    TASKGRIND_DEBUG("new");
    return alloc_and_record(tid, n, VG_(clo_alignment), 0);
}

void *
taskgrind___builtin_new_aligned(ThreadId tid, SizeT n, SizeT align)
{
    TASKGRIND_DEBUG("new aligned");
    return alloc_and_record(tid, n, align, 0);
}

void *
taskgrind___builtin_vec_new(ThreadId tid, SizeT n)
{
    TASKGRIND_DEBUG("vec new");
    return alloc_and_record(tid, n, VG_(clo_alignment), 0);
}

void *
taskgrind___builtin_vec_new_aligned(ThreadId tid, SizeT n, SizeT align)
{
    TASKGRIND_DEBUG("vec new aligned");
    return alloc_and_record(tid, n, align, 0);
}

void *
taskgrind_memalign(ThreadId tid, SizeT align, SizeT n)
{
    TASKGRIND_DEBUG("mem aligned");
    return alloc_and_record(tid, n, align, 0);
}

void *
taskgrind_calloc(ThreadId tid, SizeT n, SizeT size)
{
    return alloc_and_record(tid, n * size, VG_(clo_alignment), 1);
}

void
taskgrind_free(ThreadId tid, void * p)
{
    // do not release memory, to avoid recycling that could lead to false-positive
    // VG_(cli_free)(p);
}

void
taskgrind___builtin_delete(ThreadId tid, void * p)
{
    TASKGRIND_DEBUG("delete");
}

void
taskgrind___builtin_delete_aligned(ThreadId tid, void * p, SizeT alignB)
{
    TASKGRIND_DEBUG("delete_aligned");
}

void
taskgrind___builtin_vec_delete(ThreadId tid, void * p)
{
    TASKGRIND_DEBUG("vec_delete");
}

void
taskgrind___builtin_vec_delete_aligned(ThreadId tid, void* p, SizeT alignB)
{
    TASKGRIND_DEBUG("vec_delete_aligned");
}

void *
taskgrind_realloc(ThreadId tid, void * p, SizeT size)
{
    return realloc_and_record(tid, p, size);
}

SizeT
taskgrind_malloc_usable_size(ThreadId tid, void * p)
{
    // TODO
    return 0;
}
