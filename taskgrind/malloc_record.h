#ifndef __MALLOC_RECORD_H__
# define __MALLOC_RECORD_H__

# include "pub_tool_basics.h"       /* ThreadId */
# include "pub_tool_libcassert.h"   /* tl_assert */
# include "pub_tool_execontext.h"   /* ExeContext */

typedef struct  taskgrind_alloc_record_t
{
    void * p;
    SizeT size;
    Bool slop;
    ExeContext * ctx;
    ThreadId tid;
}               taskgrind_alloc_record_t;

taskgrind_alloc_record_t * taskgrind_alloc_record_get(void * p);
void taskgrind_record_alloc(ThreadId tid, void * p, SizeT size, Bool slop);

#endif /* __MALLOC_RECORD_H__ */
