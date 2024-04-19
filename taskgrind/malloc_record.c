# include "pub_tool_basics.h"       /* ThreadId */
# include "pub_tool_libcassert.h"   /* tl_assert */
# include "pub_tool_threadstate.h"  /* VG_INVALID_THREADID */

# include "malloc_record.h"
# include "print.h"

taskgrind_alloc_record_t *
taskgrind_alloc_record_get(void * p)
{
    // TODO
    return NULL;
}

void
taskgrind_record_alloc(ThreadId tid, void * p, SizeT size, Bool slop)
{
    tl_assert(tid != VG_INVALID_THREADID);

//    TASKGRIND_INFO("Recording malloc for %p of size %d", p, size);

    taskgrind_alloc_record_t record;
    record.p = p;
    record.size = size;
    record.slop = slop;
    record.ctx = VG_(record_ExeContext)(tid, 0);
    record.tid = tid;

    // TODO : register
}
