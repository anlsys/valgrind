#ifndef __TASK_H__
# define __TASK_H__

# include "pub_tool_basics.h"
# include "pub_tool_libcbase.h"      /* strstr */
# include "pub_tool_execontext.h"

# include "taskgrind_uthash.h"
# include "taskgrind_spmt.h"
# include "taskgrind.h"

# include "array.h"

// task types
typedef enum    task_type_e
{
    TASK_TYPE_UNKNOWN,
    TASK_TYPE_EXPLICIT,
    TASK_TYPE_IMPLICIT,
    TASK_TYPE_IMPLICIT_ROOT,
    TASK_TYPE_IMPLICIT_OUTSET,
    TASK_TYPE_IMPLICIT_TASKWAIT,
    TASK_TYPE_IMPLICIT_BARRIER,
    TASK_TYPE_IMPLICIT_TASKGROUP,
    TASK_TYPE_IMPLICIT_UNKNOWN,
}               task_type_t;

// task depends hmap for child dependences
typedef struct  task_depend_s
{
    // dependency address
    Addr addr;

    // last task that had an 'out' for this address
    struct task_s * out;

    // last task that had an 'in' for this address
    array_t ins;

    // last task that had an 'outset' for this address
    array_t outsets;

    // the last successor task that matched the 'out' dependency (for redundancy filtering)
    struct task_s * last_out;

    // the last successor task that matched the 'in' dependency (for redundancy filtering)
    struct task_s * last_in;

    // the last successor task that matched the 'outset' dependency (for redundancy filtering)
    struct task_s * last_outset;

    // hmap handle
    UT_hash_handle hh;
}               task_depend_t;

// tasks
typedef struct  task_s
{
    // the task type
    task_type_t type;

    // unique identifier relative to its parent (independant from schedule)
    UWord child_id;

    // next unique identifier for a child
    UWord next_child_id;

    // id for the tool client (depend on schedule)
    UWord id;

    // task successors infered from dependences provided by user program
    array_t successors;

    // task hmap for child dependencies
    task_depend_t * depend;

    // parent
    struct task_s * parent;

    // children
    array_t children;

    // last taskwait node
    struct task_s * last_taskwait;

    // last barrier node
    struct task_s * last_barrier;

    // segs
    array_t segs;

    // hmap handle
    UT_hash_handle hh;

    // stack pointer when starting the task
    Addr sp;

    // if undeferred
    UWord undeferred;

    // !! BELLOW ARE ATTRIBUTES USED BY PASSES !!
    char flag;
}               task_t;

// a logical task seg
typedef struct  task_seg_s
{
    // the client task
    task_t * task;

    // memory loads
    spmt_t loads;

    // memory stores
    spmt_t stores;

    // successors expressed by the client
    array_t successors;

    // thread that executed this segment
    ThreadId tid;

    // execution context on first memory access
    ExeContext * ctx;

    // TLS information when this segment terminated
    struct {

        // Address of the TCB (frontier between TCB and static TLS)
        Addr tp;

        // Array of couple (a, b) representing a TLS block [a; b[
        array_t dtv;

    } tls;

    // a unique identifier for this segment in [0, N_SEGMENTS[
    UInt uid;

    // a version counter for dfs, to detect
    // whether the segment had already been visited
    UInt dfs_version;
}               task_seg_t;

// a reference to a task segment Thats because 'task->segs' array may be
// realloc-ed, therefore, using 'task_seg_t *' reference directly may be
// inconsistent
typedef struct  task_seg_ref_s
{
    // the task
    task_t * task;

    // seg id
    UInt id;

    // !! BELLOW ARE ATTRIBUTES USED BY PASSES !!

    // a flag for searching
    char flag;

}               task_seg_ref_t;

typedef enum    task_mem_access_type_e
{
    TASKGRIND_TASK_MEM_LOAD,
    TASKGRIND_TASK_MEM_STORE,
    TASKGRIND_TASK_MEM_LOAD_ATOMIC,
    TASKGRIND_TASK_MEM_STORE_ATOMIC,
}               task_mem_access_type_t;

typedef struct  thread_t
{
    // the implicit task
    task_t root_task;

    // the current task
    task_t * current_task;

}               thread_t;

# define TASKGRIND_MAX_THREADS 256

// GLOBAL VARIABLE MAPPING EXECUTION AS TASKS

// Number of segments
extern array_t SEGS;

// FUNCTIONS TO BUILD THE MAPPING
void task_fork(void);
void task_join(void);
void task_thread_begin(void);
void task_thread_end(void);
task_t * task_create(UWord id, task_type_t type, UWord undeferred);
void task_schedule(UWord id);
void task_depend(UWord id, UWord addr, UWord type);
void task_sync(taskgrind_sync_t mode);
void task_detach_fulfill(UWord id, taskgrind_fulfill_mode_t mode);

// FUNCTIONS FOR MEMORY ACCESSES DETECTED
void task_mem_load(Addr addr, SizeT size);
void task_mem_store(Addr addr, SizeT size);
void task_mem_load_atomic(Addr addr, SizeT size);
void task_mem_store_atomic(Addr addr, SizeT size);

// HELPER FUNCTIONS
task_t * task_get(UWord id);
void task_seg_dfs(UInt (*walk)(task_seg_t *, void *), void * opaque);
void task_seg_dfs_from(UInt (*walk)(task_seg_t *, void *), void * opaque, task_seg_t * seg);

// INIT -> setup root task
void task_init(void);

// FINILIZE -> generate analysis and report
void task_fini(void);

#endif /* __TASK_H__ */
