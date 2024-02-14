#ifndef __TASK_H__
# define __TASK_H__

#include "pub_tool_basics.h"
#include "pub_tool_libcbase.h"      /* strstr */

# include "taskgrind_uthash.h"
# include "taskgrind_spmt.h"

# include "array.h"

// task types
typedef enum    task_type_e
{
    TASK_TYPE_UNKNOWN,
    TASK_TYPE_EXPLICIT,
    TASK_TYPE_IMPLICIT,
    TASK_TYPE_IMPLICIT_ROOT,
    TASK_TYPE_IMPLICIT_OUTSET,
    TASK_TYPE_IMPLICIT_BARRIER,
    TASK_TYPE_IMPLICIT_UNKNOWN,
}               task_type_t;

// task accesses hmap for child dependences
typedef struct  task_accesses_t
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
}               task_accesses_t;

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
    UWord client_id;

    // task successors infered from dependences provided by user program
    array_t successors;

    // task hmap for child dependencies
    task_accesses_t * accesses;

    // parent
    struct task_s * parent;

    // children
    array_t children;

    // last synchronization node
    struct task_s * last_sync;

    // parts
    array_t parts;

    // hmap handle
    UT_hash_handle hh;
}               task_t;

// a logical task part
typedef struct  task_part_s
{
    // the client task
    task_t * task;

    // memory loads
    spmt_t loads;

    // memory stores
    spmt_t stores;

    // successors expressed by the client
    array_t successors;
}               task_part_t;

typedef struct  task_part_ref_s
{
    // the task
    task_t * task;

    // part id
    UInt id;
}               task_part_ref_t;

typedef enum    task_mem_access_type_e
{
    TASKGRIND_TASK_MEM_LOAD,
    TASKGRIND_TASK_MEM_STORE,
    TASKGRIND_TASK_MEM_LOAD_ATOMIC,
    TASKGRIND_TASK_MEM_STORE_ATOMIC,
}               task_mem_access_type_t;

// GLOBAL VARIABLE MAPPING EXECUTION AS TASKS

// The tasks hmap
extern task_t * TASKS;

// The current task
extern task_t * CURRENT_TASK;

// The root task
extern task_t ROOT_TASK;

// FUNCTIONS TO BUILD THE MAPPING
task_t * task_create(UWord client_id, task_type_t type);
void task_schedule(UWord client_id);
void task_access(UWord client_id, UWord addr, UWord type);
void task_sync(void);

// FUNCTIONS FOR MEMORY ACCESSES DETECTED
void task_mem_load(Addr addr, SizeT size);
void task_mem_store(Addr addr, SizeT size);
void task_mem_load_atomic(Addr addr, SizeT size);
void task_mem_store_atomic(Addr addr, SizeT size);

// HELPER FUNCTIONS
task_t * task_get(UWord client_id);

// INIT -> setup root task
void task_init(void);

// FINILIZE -> generate analysis and report
void task_fini(void);

#endif /* __TASK_H__ */
