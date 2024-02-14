#include "dot.h"
#include "pass/pass.h"
#include "print.h"
#include "task.h"
#include "taskgrind.h"

#include "pub_tool_libcassert.h"    /* tool_panic, lt_assert */

// The tasks hmap
task_t * TASKS;

// Implicit root task of the entire program
task_t ROOT_TASK;

// The current task
task_t * CURRENT_TASK = NULL;

static inline task_part_t *
task_part_new(task_t * task)
{
    task_part_t * part = array_push(&task->parts, NULL);
    part->task = task;
    SPMT_INITIALIZE(&part->loads);
    SPMT_INITIALIZE(&part->stores);
    array_init(&part->successors, 0, sizeof(task_part_ref_t));
    return part;
}

static inline void
task_new_init(task_t * task, UWord client_id, task_type_t type)
{
    // set attributes
    task->type              = type;
    task->child_id          = CURRENT_TASK ? ++CURRENT_TASK->next_child_id : -1;
    task->next_child_id     = 0;
    task->client_id         = client_id;
    array_init(&task->successors, 4, sizeof(task_t *));
    task->accesses          = NULL;
    task->parent            = CURRENT_TASK;
    array_init(&task->children, 0, sizeof(task_t *));
    task->last_sync         = NULL;
    array_init(&task->parts, 0, sizeof(task_part_t));

    // add an initial part
    task_part_new(task);

    // tcfg parent reference
    if (CURRENT_TASK)
        array_push(&CURRENT_TASK->children, &task);
}

static inline task_t *
task_new(UWord client_id, task_type_t type)
{
    task_t * task;

    task = (task_t *) VG_(malloc)("task_new", sizeof(task_t));
    task_new_init(task, client_id, type);
    return task;
}

static inline task_t *
task_array_last(array_t * array)
{
    struct task_s ** tasks = array_last(array);
    return tasks ? tasks[0] : NULL;
}

// set the edge pred -> succ
static inline void
task_link_access(task_t * pred, task_t * succ)
{
    // filter out multiple edges
    task_t ** tasks = array_last(&pred->successors);
    task_t * last = tasks ? tasks[0] : NULL;
    if (last == succ)
        return ;
    array_push(&pred->successors, &succ);
    TASKGRIND_DEBUG("   Added edge %p -> %p", (void *)pred->client_id, (void *)succ->client_id);
}

task_t *
task_create(UWord client_id, task_type_t type)
{
    // ensure this client_id has not already been used
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&client_id, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, TASKS, &client_id, sizeof(UWord), hashv, task);

    tl_assert(task == NULL);

    // create the task
    if (task == NULL)
    {
        task = task_new(client_id, type);
        HASH_ADD_KEYPTR_BYHASHVALUE(hh, TASKS, &(task->client_id), sizeof(UWord), hashv, task);
        TASKGRIND_DEBUG("Task create %p (parent %p)", (void *) client_id, (void *) (task->parent ? task->parent->client_id : TASKGRIND_CLIENT_ID_PRIVATE));
    }

    tl_assert(task);

    // add edges with respect to previous synchronizations
    tl_assert(CURRENT_TASK);
    if (CURRENT_TASK->last_sync)
        task_link_access(CURRENT_TASK->last_sync, task);

    // insert a new task part for the parent
    task_part_t * succ = task_part_new(CURRENT_TASK);
    tl_assert(succ);

    // retrieve penultimate to insert dependency edge
    task_part_t * pred = (task_part_t *) array_penultimate(&CURRENT_TASK->parts);
    tl_assert(pred);

    task_part_ref_t part_ref;
    part_ref.task = CURRENT_TASK;
    part_ref.id = CURRENT_TASK->parts.n - 1;
    TASKGRIND_DEBUG("adding logical edge %p -> %p", pred, succ);
    array_push(&pred->successors, &part_ref);
    TASKGRIND_DEBUG("added logical edge %p -> %p", pred, succ);

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

static inline task_part_t *
task_part_get_current(void)
{
    tl_assert(CURRENT_TASK);
    return array_last(&CURRENT_TASK->parts);
}

// schedule
void
task_schedule(UWord client_id)
{
    task_t * prev, * next;

    prev    = CURRENT_TASK;
    next    = task_get(client_id);
    tl_assert(prev);
    tl_assert(next);
    tl_assert(prev != next);

    CURRENT_TASK = next;
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
        array_init(&accesses->ins, 0, sizeof(task_t *));
        array_init(&accesses->outsets, 0, sizeof(task_t *));

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

// add a dependency to the task following RaW constraints
void
task_access(UWord client_id, UWord addr, UWord type)
{
    tl_assert(type == TASKGRIND_IN || type == TASKGRIND_OUT || type == TASKGRIND_OUTSET);
    TASKGRIND_DEBUG("Task %p accesses %s at %p", (void *) client_id, type == TASKGRIND_IN ? "IN" : type == TASKGRIND_OUT ? "OUT" : type == TASKGRIND_OUTSET ? "OUTSET" : "(null)", (void *) addr);

    // retrieve current task and its parent accesses
    task_t * task = task_get(client_id);
    tl_assert(task);
    tl_assert(task->parent);

    task_accesses_t * accesses = task_accesses_get(task->parent, addr);
    tl_assert(accesses);

    // filter out redundancies
    if (!task_access_is_redundant(task, accesses, addr, type))
    {
        // infer edge between 'task' and its predecessor

        // case 1.1 - the generated task is dependant of previous 'in'
        if (!array_is_empty(&accesses->ins) && (type == TASKGRIND_OUT || type == TASKGRIND_OUTSET))
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
                accesses->out = task_new(TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_OUTSET);
                ARRAY_FOREACH_BEGIN(&accesses->ins, task_t **, in)
                {
                    // prevent cyclic deps in case 'task' already had an 'in'
                    // dep type on the same addr previously
                    if (*in != task)
                        task_link_access(*in, accesses->out);
                }
                ARRAY_FOREACH_END(&accesses->ins, task_t **, in)

                task_link_access(accesses->out, task);
                array_clear(&accesses->ins);
            }
            else
            {
                ARRAY_FOREACH_BEGIN(&accesses->ins, task_t **, in)
                    task_link_access(*in, task);
                ARRAY_FOREACH_END(&accesses->ins, task_t **, in)
            }
        } // 1.1

        // 1.2 - the generated task is dependent of previous 'outset'
        if (!array_is_empty(&accesses->outsets) && (type == TASKGRIND_IN || type == TASKGRIND_OUT))
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
                accesses->out = task_new(TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_OUTSET);
                ARRAY_FOREACH_BEGIN(&accesses->outsets, task_t **, outset)
                    task_link_access(*outset, accesses->out);
                ARRAY_FOREACH_END(&accesses->outsets, task_t *, outset)

                task_link_access(accesses->out, task);
                array_clear(&accesses->outsets);
            }
            else
            {
                ARRAY_FOREACH_BEGIN(&accesses->outsets, task_t **, outset)
                    task_link_access(*outset, accesses->out);
                ARRAY_FOREACH_END(&accesses->outsets, task_t **, outset)
            }
        } // 1.2

        // 1.3 - the generated task is dependent of previous 'out'
        if (accesses->out && (type == TASKGRIND_OUT || type == TASKGRIND_IN || type == TASKGRIND_OUTSET))
        {
            if (type == TASKGRIND_OUT &&
                    (!array_is_empty(&accesses->ins) || !array_is_empty(&accesses->outsets)))
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
                array_push(&accesses->ins, &task);
                break ;
            }

            case (TASKGRIND_OUT):
            {
                array_clear(&accesses->ins);
                array_clear(&accesses->outsets);
                accesses->out = task;
                break ;
            }

            case (TASKGRIND_OUTSET):
            {
                array_push(&accesses->outsets, &task);
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

// a task sync: wait for the completion of sibling tasks, represented by
// adding an empty task node which depends on all previously created tasks with
// no successors (leaves)
void
task_sync(void)
{
    // create an empty task (the sync barrier)
    task_t * sync = task_new(TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_BARRIER);

    // for each children task of the current task
    ARRAY_FOREACH_BEGIN(&CURRENT_TASK->children, task_t **, child)
    {
        // link them with the new sync barrier

        // skip the newly inserted sync barrier
        if (*child == sync)
            continue ;

        if (array_is_empty(&(*child)->successors))
            task_link_access(*child, sync);
    }
    ARRAY_FOREACH_END(&CURRENT_TASK->children, task_t **, child)

    // in the future, link each next children with this barrier
    CURRENT_TASK->last_sync = sync;
}

// memory accesses
void
task_mem_load(Addr addr, SizeT size)
{
    task_part_t * part = task_part_get_current();

    #if 0
    TASKGRIND_DEBUG("(task=%p, part=%p) LOAD        0x%010lX %lu",
            (void *) CURRENT_TASK->client_id, part, addr, size);
    #endif

    tl_assert(part);
    SPMT_FILL(&part->loads, addr, addr + size);
}

void
task_mem_store(Addr addr, SizeT size)
{
    task_part_t * part = task_part_get_current();

    #if 0
    TASKGRIND_DEBUG("(task=%p, part=%p) STORE       0x%010lX %lu",
            (void *) CURRENT_TASK->client_id, part, addr, size);
    #endif

    tl_assert(part);
    SPMT_FILL(&part->stores, addr, addr + size);
}

void
task_mem_load_atomic(Addr addr, SizeT size)
{
#if 0
    if (CURRENT_TASK->client_id == 3)
        TASKGRIND_DEBUG("(task=%p) LOAD ATOMIC  0x%010lX %lu",
                (void *) CURRENT_TASK->client_id, addr, size);
#endif
}

void
task_mem_store_atomic(Addr addr, SizeT size)
{
#if 0
    if (CURRENT_TASK->client_id == 3)
        TASKGRIND_DEBUG("(task=%p) STORE ATOMIC 0x%010lX %lu",
                (void *) CURRENT_TASK->client_id, addr, size);
#endif
}

// Initialize execution
void
task_init(void)
{
    task_new_init(&ROOT_TASK, TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_ROOT);
    CURRENT_TASK = &ROOT_TASK;
}

// Execution terminated, perform analysis and report here
void
task_fini(void)
{
    TASKGRIND_INFO("Starting analysis...");
    // __analyze_useless_dependencies(CURRENT_TASK);
    taskgrind_export_tcfg(&ROOT_TASK);
    taskgrind_export_tdgx_recursive(&ROOT_TASK);
    taskgrind_export_lpg((task_part_t *)array_first(&ROOT_TASK.parts));
    taskgrind_pass_ph1(&ROOT_TASK);
    TASKGRIND_INFO("Analysis completed.");

    // taskgrind_export_tcfg(CURRENT_TASK);
}
