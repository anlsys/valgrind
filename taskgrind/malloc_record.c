# include "pub_tool_basics.h"       /* ThreadId */
# include "pub_tool_libcassert.h"   /* tl_assert */
# include "pub_tool_threadstate.h"  /* VG_INVALID_THREADID */
# include "pub_tool_mallocfree.h"  /* VG_(realloc) */

# include "malloc_record.h"
# include "print.h"

taskgrind_alloc_record_t * alloc_records = NULL;
unsigned int n_alloc_records = 0;
unsigned int capacity_alloc_records = 0;

taskgrind_alloc_record_t *
taskgrind_alloc_record_get(void * p)
{
    unsigned int i;
    for (i = 0 ; i < n_alloc_records ; ++i)
    {
        taskgrind_alloc_record_t * record = alloc_records + i;
        if (record->p <= p && p < record->p + record->size)
            return record;
    }
    return NULL;
}

void
taskgrind_record_alloc(ThreadId tid, void * p, SizeT size, Bool slop)
{
    tl_assert(tid != VG_INVALID_THREADID);

    if (n_alloc_records == capacity_alloc_records)
    {
        capacity_alloc_records = 2 * capacity_alloc_records + 1;
        alloc_records = VG_(realloc)("taskgrind_record_alloc", alloc_records, capacity_alloc_records * sizeof(taskgrind_alloc_record_t));
        tl_assert(alloc_records);
    }

    taskgrind_alloc_record_t * record = alloc_records + n_alloc_records;
    record->p = p;
    record->size = size;
    record->slop = slop;
    record->ctx = VG_(record_ExeContext)(tid, 0);
    record->tid = tid;

    ++n_alloc_records;
}
