#ifndef __TASK_H__
# define __TASK_H__

#include "pub_tool_basics.h"
#include "pub_tool_libcbase.h"      /* strstr */

# include "taskgrind_uthash.h"
# include "taskgrind_spmt.h"

// task types
typedef enum    task_type_e
{
    TASK_TYPE_UNKNOWN,
    TASK_TYPE_EXPLICIT,
    TASK_TYPE_IMPLICIT_ROOT,
    TASK_TYPE_IMPLICIT_OUTSET,
    TASK_TYPE_IMPLICIT_BARRIER,
    TASK_TYPE_IMPLICIT_UNKNOWN,
}               task_type_t;

// list of tasks
typedef struct  task_array_s
{
    // tasks
    struct task_s ** tasks;

    // capacity
    UInt capacity;

    // number of tasks set
    UInt n;
}               task_array_t;

# define TASK_ARRAY_INITIALIZE_STATIC {NULL, 0, 0}

// task accesses hmap for child dependences
typedef struct  task_accesses_t
{
    // dependency address
    Addr addr;

    // last task that had an 'out' for this address
    struct task_s * out;

    // last task that had an 'in' for this address
    task_array_t ins;

    // last task that had an 'outset' for this address
    task_array_t outsets;

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
    task_array_t access_successors;

    // real successors using RaW on load/stores
    task_array_t raw_successors;

    // task hmap for child dependencies
    task_accesses_t * accesses;

    // parent
    struct task_s * parent;

    // children
    task_array_t children;

    // memory loads
    spmt_t loads;

    // memory stores
    spmt_t stores;

    // hmap handle
    UT_hash_handle hh;
}               task_t;

# define TASK_INITIALIZE_STATIC(T)                          \
    {                                                       \
        .type               = T,                            \
        .child_id           = 0,                            \
        .next_child_id      = 0,                            \
        .client_id          = TASKGRIND_CLIENT_ID_PRIVATE,  \
        .access_successors  = TASK_ARRAY_INITIALIZE_STATIC, \
        .raw_successors     = TASK_ARRAY_INITIALIZE_STATIC, \
        .accesses           = NULL,                         \
        .parent             = NULL,                         \
        .children           = TASK_ARRAY_INITIALIZE_STATIC, \
        .loads              = SPMT_INITIALIZE_STATIC,       \
        .stores             = SPMT_INITIALIZE_STATIC,       \
        .hh                 = {0},                          \
    }

// GLOBAL VARIABLE MAPPING EXECUTION AS TASKS

// The tasks hmap
extern task_t * TASKS;

// The current task
extern task_t * CURRENT_TASK;

// FUNCTIONS TO BUILD THE MAPPING
task_t * task_create(UWord client_id, task_type_t type);
void task_schedule(UWord client_id);
void task_access(UWord client_id, UWord addr, UWord type);
void task_sync(void);

// FUNCTIONS FOR MEMORY ACCESSES DETECTED
void task_mem_load(Addr addr, SizeT size);
void task_mem_store(Addr addr, SizeT size);

// HELPER FUNCTIONS
task_t * task_get(UWord client_id);
void task_array_init(task_array_t * array);
void task_array_push(task_array_t * array, struct task_s * task);
struct task_s * task_array_last(task_array_t * array);
void task_array_clear(task_array_t * array);
void task_array_deinit(task_array_t * array);

#endif /* __TASK_H__ */
