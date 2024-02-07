#include "print.h"
#include "task.h"
#include "taskgrind.h"

#include "pub_tool_libcassert.h"    /* tool_panic, lt_assert */

// The tasks hmap
task_t * TASKS;

// Implicit root task of the entire program
static task_t ROOT_TASK = TASK_INITIALIZE_STATIC(TASK_TYPE_IMPLICIT_ROOT);

// The current task
task_t * CURRENT_TASK = &ROOT_TASK;

void
task_array_init(task_array_t * array)
{
    array->tasks    = NULL;
    array->capacity = 0;
    array->n        = 0;
}

void
task_array_push(task_array_t * array, struct task_s * task)
{
    if (array->n == array->capacity)
    {
        UInt capacity = (UInt)((array->n + 1) * 3 / 2);
        array->tasks = VG_(realloc)("task_array_t", array->tasks, sizeof(struct task_s *) * capacity);
        array->capacity = capacity;
    }
    array->tasks[array->n++] = task;
}

struct task_s *
task_array_last(task_array_t * array)
{
    if (array->n == 0)
        return NULL;
    return array->tasks[array->n - 1];
}

void
task_array_clear(task_array_t * array)
{
    array->n = 0;
}

void
task_array_deinit(task_array_t * array)
{
    VG_(free)(array->tasks);
    array->n        = 0;
    array->capacity = 0;
}

static inline task_t *
task_alloc(void)
{
    task_t * task;

    task = (task_t *) VG_(malloc)("task_alloc", sizeof(task_t));
    task->child_id          = ++CURRENT_TASK->next_child_id;
    task->next_child_id     = 0;
    task->client_id         = TASKGRIND_CLIENT_ID_PRIVATE;
    task->type              = TASK_TYPE_UNKNOWN;
    task->accesses          = NULL;
    task->parent            = CURRENT_TASK;
    task_array_init(&task->access_successors);
    task_array_init(&task->raw_successors);
    task_array_init(&task->children);

    task_array_push(&CURRENT_TASK->children, task);

    return task;
}

task_t *
task_create(UWord client_id, task_type_t type)
{
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&client_id, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, TASKS, &client_id, sizeof(UWord), hashv, task);

    tl_assert(task == NULL);

    if (task == NULL)
    {
        task = task_alloc();
        task->client_id = client_id;
        task->type = type;
        HASH_ADD_KEYPTR_BYHASHVALUE(hh, TASKS, &(task->client_id), sizeof(UWord), hashv, task);
        TASKGRIND_DEBUG("Task create %p (parent %p)", (void *) client_id, (void *) (task->parent ? task->parent->client_id : TASKGRIND_CLIENT_ID_PRIVATE));
        SPMT_INITIALIZE(&task->loads);
        SPMT_INITIALIZE(&task->stores);
    }

    tl_assert(task);

    return task;
}

task_t *
task_get(UWord client_id)
{
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&client_id, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, TASKS, &client_id, sizeof(UWord), hashv, task);

    return task;
}

// schedule
void
task_schedule(UWord client_id)
{
    // DEBUG REMOVE ME
    UWord old = CURRENT_TASK ? CURRENT_TASK->client_id : TASKGRIND_CLIENT_ID_PRIVATE;

    CURRENT_TASK = task_get(client_id);
    tl_assert(CURRENT_TASK);

    if (old != CURRENT_TASK->client_id)
    {
        TASKGRIND_DEBUG("Task switch %p -> %p", (void *)old, (void *)CURRENT_TASK->client_id);
    }
    // DEBUG REMOVE ME
}

// accesses
static inline task_accesses_t *
task_accesses_get(task_t * parent, UWord addr)
{
    task_accesses_t * accesses;
    unsigned hashv;

    HASH_VALUE(&addr, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, parent->accesses, &addr, sizeof(UWord), hashv, accesses);

    if (!accesses)
    {
        accesses = (task_accesses_t *) VG_(malloc)("task_access", sizeof(task_accesses_t));
        accesses->addr          = addr;
        accesses->out           = NULL;
        accesses->last_out      = NULL;
        accesses->last_in       = NULL;
        accesses->last_outset   = NULL;
        task_array_init(&accesses->ins);
        task_array_init(&accesses->outsets);

        HASH_ADD_KEYPTR_BYHASHVALUE(hh, parent->accesses, &(accesses->addr), sizeof(UWord), hashv, accesses);
    }
    tl_assert(accesses);

    return accesses;
}

// return true if the given task access is redundant for the given address
static inline Bool
task_access_is_redundant(
    task_t * task,
    task_accesses_t * accesses,
    UWord addr,
    UWord type)
{
    switch (type)
    {
        case (TASKGRIND_OUT):
        {
            if (accesses->last_out == task)
                return True;
            accesses->last_out = task;
            return False;
        }

        case (TASKGRIND_IN):
        {
            if (accesses->last_out == task || accesses->last_in == task)
                return True;
            accesses->last_in = task;
            return False;
        }

        case (TASKGRIND_OUTSET):
        {
            if (accesses->last_out == task || accesses->last_outset == task)
                return True;
            accesses->last_outset = task;
            return False;
        }

        default:
        {
            tl_assert(0);
            return False;
        }
    }
}

// set the edge pred -> succ
static inline void
task_link_access(task_t * pred, task_t * succ)
{
    // filter out multiple edges
    if (task_array_last(&pred->access_successors) == succ)
        return ;
    task_array_push(&pred->access_successors, succ);
    TASKGRIND_DEBUG("   Added edge %p -> %p", (void *)pred->client_id, (void *)succ->client_id);
}

// add a dependency to the task following RaW constraints
void
task_access(UWord client_id, UWord addr, UWord type)
{
    tl_assert(type == TASKGRIND_IN || type == TASKGRIND_OUT || type == TASKGRIND_OUTSET);
    TASKGRIND_DEBUG("Task %p accesses %s at %p", (void *) client_id, type == TASKGRIND_IN ? "IN" : type == TASKGRIND_OUT ? "OUT" : type == TASKGRIND_OUTSET ? "OUTSET" : "(null)", (void *) addr);

    // retrieve current task and its parent accesses
    task_t * task, * in, * outset;
    task_accesses_t * accesses;
    int i;

    task = task_get(client_id);
    tl_assert(task);
    tl_assert(task->parent);

    accesses = task_accesses_get(task->parent, addr);
    tl_assert(accesses);

    // filter out redundancies
    if (!task_access_is_redundant(task, accesses, addr, type))
    {
        // infer edge between 'task' and its predecessor

        // case 1.1 - the generated task is dependant of previous 'in'
        if (accesses->ins.n && (type == TASKGRIND_OUT || type == TASKGRIND_OUTSET))
        {
            if (type == TASKGRIND_OUTSET)
            {
                /**
                 * in:      O O O   <- the predecessor
                 *           \|/
                 * out:       X     <- we insert this empty node
                 *           / \
                 * outset:  O   O   <- the task we are inserting
                 */
                accesses->out = task_alloc();
                accesses->out->client_id = TASKGRIND_CLIENT_ID_PRIVATE;
                accesses->out->type      = TASK_TYPE_IMPLICIT_OUTSET;
                for (i = 0 ; i < accesses->ins.n ; ++i)
                {
                    // prevent cyclic deps in case 'task' already had an 'in'
                    // dep type on the same addr previously
                    in = accesses->ins.tasks[i];
                    if (in != task)
                        task_link_access(in, accesses->out);
                }
                task_link_access(accesses->out, task);
                task_array_clear(&accesses->ins);
            }
            else
            {
                for (i = 0 ; i < accesses->ins.n ; ++i)
                {
                    in = accesses->ins.tasks[i];
                    task_link_access(in, task);
                }
            }
        } // 1.1

        // 1.2 - the generated task is dependent of previous 'outset'
        if (accesses->outsets.n && (type == TASKGRIND_IN || type == TASKGRIND_OUT))
        {
            if (type == TASKGRIND_IN)
            {
                /**
                 * outset:          O O O   <- the predecessor
                 *                   \|/
                 * out:               X     <- we insert this empty node
                 *                   / \
                 * in:              O   O   <- the task we are inserting
                 */
                accesses->out = task_alloc();
                accesses->out->client_id = 0xFA3E;
                for (i = 0 ; i < accesses->outsets.n ; ++i)
                {
                    outset = accesses->outsets.tasks[i];
                    task_link_access(outset, accesses->out);
                }
                task_link_access(accesses->out, task);
                task_array_clear(&accesses->outsets);
            }
            else
            {
                for (i = 0 ; i < accesses->outsets.n ; ++i)
                {
                    outset = accesses->outsets.tasks[i];
                    task_link_access(outset, accesses->out);
                }
            }
        } // 1.2

        // 1.3 - the generated task is dependent of previous 'out'
        if (accesses->out && (type == TASKGRIND_OUT || type == TASKGRIND_IN || type == TASKGRIND_OUTSET))
        {
            if (type == TASKGRIND_OUT && (accesses->ins.n || accesses->outsets.n))
            {
                // nothing to do, the task already depends on a previous 'in'
                // or 'outset' that depend on the 'accesses->out'
            }
            else
            {
                task_link_access(accesses->out, task);
            }
        }

        // save access for future task
        switch (type)
        {
            case (TASKGRIND_IN):
            {
                task_array_push(&accesses->ins, task);
                break ;
            }

            case (TASKGRIND_OUT):
            {
                task_array_clear(&accesses->ins);
                task_array_clear(&accesses->outsets);
                accesses->out = task;
                break ;
            }

            case (TASKGRIND_OUTSET):
            {
                task_array_push(&accesses->outsets, task);
                break ;
            }

            default:
            {
                tl_assert(0);
                break ;
            }
        }

    } // redundant check
}

// a task barrier: wait for the completion of children tasks, represented by
// adding an empty task node which depends on all previously created tasks with
// no successors (leaves)
void
task_sync(void)
{
    //// create an empty task (the barrier)
    //task_t * barrier = task_alloc();
    //barrier->client_id = TASKGRIND_CLIENT_ID_PRIVATE;
    //barrier->type      = TASKGRIND_TYPE_IMPLICIT_BARRIER;

    //// for each children task of the current task
    //int i;
    //for (i = 0 ; i < CURRENT_TASK->children.n ; ++i)
    //{
    //    // link them with the new barrier
    //    task_t * task = CURRENT_TASK->children.tasks[i];
    //    if (task->access_successors.n == 0)
    //        task_link_access(task, barrier);
    //}

    // TODO: link each future children with this barrier
}

// memory accesses
void
task_mem_load(Addr addr, SizeT size)
{
#if 0
    TASKGRIND_DEBUG("(task=%p) LOAD  0x%010lX %lu", CURRENT_TASK, addr, size);
#endif
    SPMT_FILL(&CURRENT_TASK->loads, addr, addr + size);
}

void
task_mem_store(Addr addr, SizeT size)
{
#if 0
    TASKGRIND_DEBUG("(task=%p) STORE 0x%010lX %lu", CURRENT_TASK, addr, size);
#endif
    SPMT_FILL(&CURRENT_TASK->loads, addr, addr + size);
}




