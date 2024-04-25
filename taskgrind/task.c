# include "dot.h"
# include "pass/pass.h"
# include "print.h"
# include "task.h"
# include "taskgrind.h"
# include "taskgrind_clo.h"

# include "pub_tool_libcassert.h"    /* tool_panic, lt_assert */
# include "pub_tool_threadstate.h"
# include "pub_tool_execontext.h"
# include "pub_tool_guest.h"

// The tasks hmap
task_t * TASKS;

// Implicit root task of the entire program
task_t ROOT_TASK;

// The current task
task_t * CURRENT_TASK = NULL;

// List of all segments
array_t SEGS;

// version for dfs
static UInt DFS_VERSION = 0;

static void
task_seg_dfs_go(UInt (*dfs)(task_seg_t *, void *), void * opaque, task_seg_t * seg, UInt * stop)
{
    if (seg->dfs_version == DFS_VERSION)
        return ;
    seg->dfs_version = DFS_VERSION;

    if (dfs(seg, opaque))
    {
        *stop = 1;
         return ;
    }

    ARRAY_FOREACH_BEGIN(&seg->successors, task_seg_ref_t *, succ_ref)
    {
        task_seg_t * succ_seg = succ_ref->task->segs.segs + succ_ref->id;
        task_seg_dfs_go(dfs, opaque, succ_seg, stop);
        if (*stop)
            return ;
    }
    ARRAY_FOREACH_END(&seg->successors, task_seg_ref_t *, succ_ref);
}

void
task_seg_dfs_from(UInt (*dfs)(task_seg_t *, void *), void * opaque, task_seg_t * seg)
{
    UInt stop = 0;
    ++DFS_VERSION;
    task_seg_dfs_go(dfs, opaque, seg, &stop);
}

void
task_seg_dfs(UInt (*dfs)(task_seg_t *, void *), void * opaque)
{
    task_seg_t * root_seg = (task_seg_t *) array_first(&ROOT_TASK.segs);
    tl_assert(root_seg);

    task_seg_dfs_from(dfs, opaque, root_seg);
}

static inline task_seg_t *
task_seg_new(task_t * task)
{
    task_seg_t * seg = array_push(&task->segs, NULL);
    seg->task = task;
    SPMT_INITIALIZE(&seg->loads);
    SPMT_INITIALIZE(&seg->stores);
    array_init(&seg->successors, 4, sizeof(task_seg_ref_t));
    seg->ctx = NULL;
    seg->uid = SEGS.n;
    seg->dfs_version = 0;

    ThreadId tid = VG_(get_running_tid)();
    if (tid != VG_INVALID_THREADID)
    {
        seg->ctx = NULL; // VG_(record_ExeContext)(tid, 0);
        seg->tid = tid;
    }

    task_seg_ref_t seg_ref;
    seg_ref.task = task;
    seg_ref.id = task->segs.n - 1;
    seg_ref.flag = 0;
    array_push(&SEGS, &seg_ref);

    return seg;
}

static inline void
__task_init(task_t * task, UWord id, task_type_t type, UWord undeferred)
{
    // set attributes
    task->type              = type;
    task->child_id          = CURRENT_TASK ? ++CURRENT_TASK->next_child_id : -1;
    task->next_child_id     = 0;
    task->id                = id;
    array_init(&task->successors, 4, sizeof(task_t *));
    task->depend            = NULL;
    task->parent            = CURRENT_TASK;
    array_init(&task->children, 0, sizeof(task_t *));
    task->last_sync         = NULL;
    array_init(&task->segs, 0, sizeof(task_seg_t));
    task->sp                = (Addr) TASKGRIND_BASE_STACK_PTR;
    task->flag              = 0;
    task->undeferred        = undeferred;

    // add an initial seg
    task_seg_new(task);

    // retrieve stack pointer
    if (VG_(get_running_tid)() != VG_INVALID_THREADID)
    {
        VexGuestArchState * state = VG_(get_CurrentThreadArchState)();
#if defined(VGA_x86)
        task->sp = state->guest_ESP;
#elif defined(VGA_amd64)
        task->sp = state->guest_RSP;
#else
        tl_assert("Arch not supported" && 0);
#endif
    }

    // tcfg parent reference
    if (CURRENT_TASK)
        array_push(&CURRENT_TASK->children, &task);
}

static inline task_t *
task_new(UWord id, task_type_t type, UWord undeferred)
{
    task_t * task;

    task = (task_t *) VG_(malloc)("task_new", sizeof(task_t));
    __task_init(task, id, type, undeferred);
    return task;
}

static inline task_t *
task_array_last(array_t * array)
{
    struct task_s ** tasks = array_last(array);
    return tasks ? tasks[0] : NULL;
}

// set the edge pred -> succ in the LPG
static inline void
task_seg_set_edge(task_seg_t * pred, task_t * succ, UInt seg_id)
{
    task_seg_ref_t seg_ref;
    seg_ref.task = succ;
    seg_ref.id = seg_id;
    seg_ref.flag = 0;
    array_push(&pred->successors, &seg_ref);
}

// set the edge pred -> succ in the TDG
static inline void
task_set_edge(task_t * pred, task_t * succ)
{
    // filter out multiple edges
    task_t ** tasks = array_last(&pred->successors);
    task_t * last = tasks ? tasks[0] : NULL;
    if (last == succ)
        return ;
    array_push(&pred->successors, &succ);
//    TASKGRIND_DEBUG("   Added edge %p -> %p", (void *)pred->id, (void *)succ->id);

    // LPG
    task_seg_t * pred_seg = (task_seg_t *) array_last(&pred->segs);
    task_seg_set_edge(pred_seg, succ, 0);
}

task_t *
task_create(UWord id, task_type_t type, UWord undeferred)
{
    //
    //  [...]                   // pred
    //
    //  # pragma omp task(wait) // task
    //   {}
    //  [...]                   // succ
    //

    // ensure this id has not already been used
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&id, sizeof(UWord), hashv);

    if (id != TASKGRIND_CLIENT_ID_PRIVATE)
    {
        HASH_FIND_BYHASHVALUE(hh, TASKS, &id, sizeof(UWord), hashv, task);
        tl_assert(task == NULL);
        if (task)
        {
            TASKGRIND_ERR("Client sent the same task id for two 'task_create'");
            VG_(exit)(1);
        }
    }

    // create the task
    task = task_new(id, type, undeferred);

    if (id != TASKGRIND_CLIENT_ID_PRIVATE)
    {
        HASH_ADD_KEYPTR_BYHASHVALUE(hh, TASKS, &(task->id), sizeof(UWord), hashv, task);
    }

//    TASKGRIND_DEBUG("Task create %p (parent %p)", (void *) id, (void *) (task->parent ? task->parent->id : TASKGRIND_CLIENT_ID_PRIVATE));

    tl_assert(task);
    tl_assert(CURRENT_TASK);

    /////////
    // TDG //
    /////////
    // add edges with respect to previous synchronizations
    if (CURRENT_TASK->last_sync)
        task_set_edge(CURRENT_TASK->last_sync, task);

    /////////
    // LPG //
    /////////

    // create a new successor seg for the current task
    task_t * succ = CURRENT_TASK;
    task_seg_t * succ_seg = task_seg_new(succ);
    int succ_seg_idx = succ->segs.n - 1;
    tl_assert(succ_seg);

    // retrieve the current seg
    task_t * pred = CURRENT_TASK;
    task_seg_t * pred_seg = (task_seg_t *) array_penultimate(&pred->segs);
       // int pred_seg_idx = pred->segs.n - 2;
    tl_assert(pred_seg);

    // retrieve the new task seg
    task_seg_t * task_seg = (task_seg_t *) array_first(&task->segs);
    int task_seg_idx = task->segs.n - 1;
    tl_assert(task_seg);

    // 'pred' -> 'task'
    task_seg_set_edge(pred_seg, task, task_seg_idx);

    // TODO : barrier and taskwait are 2 different things, fix me
    switch (type)
    {
        // if the task is a barrier
        case (TASK_TYPE_IMPLICIT_BARRIER):
        {
            // for each children task of the current task (excluding the new barrier)
            ARRAY_FOREACH_BEGIN(&CURRENT_TASK->children, task_t **, child)
            {
                if (*child == task)
                    continue ;

                // link leaves with the new task barrier
                if (array_is_empty(&(*child)->successors))
                    task_set_edge(*child, task);
            }
            ARRAY_FOREACH_END(&CURRENT_TASK->children, task_t **, child);

            // 'task' -> 'succ'
            task_seg_set_edge(task_seg, succ, succ_seg_idx);

            // in the future, link each next children with this barrier
            CURRENT_TASK->last_sync = task;

            // no need to set 'pred' -> 'succ' as we already have 'pred' -> 'task' -> 'succ'
            break ;
        }

        default:
        {
            // 'task' -> 'succ'
            if (undeferred)
                task_seg_set_edge(task_seg, succ, succ_seg_idx);
            // 'pred' -> 'succ'
            else
                task_seg_set_edge(pred_seg, succ, succ_seg_idx);

            break ;
        }
    }

    return task;
}

task_t *
task_get(UWord id)
{
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&id, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, TASKS, &id, sizeof(UWord), hashv, task);

    return task;
}

static inline task_seg_t *
task_seg_get_current(void)
{
    tl_assert(CURRENT_TASK);
    return array_last(&CURRENT_TASK->segs);
}

// schedule
void
task_schedule(UWord id)
{
    task_t * prev, * next;

    prev    = CURRENT_TASK;
    next    = task_get(id);
    tl_assert(prev);
    tl_assert(next);
    tl_assert(prev != next);

    CURRENT_TASK = next;
}

// depend
static inline task_depend_t *
task_depend_get(task_t * parent, UWord addr)
{
    task_depend_t * depend;
    unsigned hashv;

    HASH_VALUE(&addr, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, parent->depend, &addr, sizeof(UWord), hashv, depend);

    if (!depend)
    {
        depend = (task_depend_t *) VG_(malloc)("task_access", sizeof(task_depend_t));
        depend->addr          = addr;
        depend->out           = NULL;
        depend->last_out      = NULL;
        depend->last_in       = NULL;
        depend->last_outset   = NULL;
        array_init(&depend->ins, 0, sizeof(task_t *));
        array_init(&depend->outsets, 0, sizeof(task_t *));

        HASH_ADD_KEYPTR_BYHASHVALUE(hh, parent->depend, &(depend->addr), sizeof(UWord), hashv, depend);
    }
    tl_assert(depend);

    return depend;
}

// return true if the given task access is redundant for the given address
static inline Bool
task_access_is_redundant(
    task_t * task,
    task_depend_t * depend,
    UWord addr,
    UWord type)
{
    switch (type)
    {
        case (TASKGRIND_OUT):
        {
            if (depend->last_out == task)
                return True;
            depend->last_out = task;
            return False;
        }

        case (TASKGRIND_IN):
        {
            if (depend->last_out == task || depend->last_in == task)
                return True;
            depend->last_in = task;
            return False;
        }

        case (TASKGRIND_OUTSET):
        {
            if (depend->last_out == task || depend->last_outset == task)
                return True;
            depend->last_outset = task;
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
task_depend(UWord id, UWord addr, UWord type)
{
    tl_assert(type == TASKGRIND_IN || type == TASKGRIND_OUT || type == TASKGRIND_OUTSET);
//    TASKGRIND_DEBUG("Task %p depend %s at %p", (void *) id, type == TASKGRIND_IN ? "IN" : type == TASKGRIND_OUT ? "OUT" : type == TASKGRIND_OUTSET ? "OUTSET" : "(null)", (void *) addr);

    // retrieve current task and its parent depend
    task_t * task = task_get(id);
    tl_assert(task);
    tl_assert(task->parent);

    task_depend_t * depend = task_depend_get(task->parent, addr);
    tl_assert(depend);

    // filter out redundancies
    if (!task_access_is_redundant(task, depend, addr, type))
    {
        // infer edge between 'task' and its predecessor

        // case 1.1 - the generated task is dependant of previous 'in'
        if (!array_is_empty(&depend->ins) && (type == TASKGRIND_OUT || type == TASKGRIND_OUTSET))
        {
#if 0
            if (type == TASKGRIND_OUTSET)
            {
                /**
                 * in:      O O O   <- the predecessor
                 *           \|/
                 * out:       X     <- we insert this empty node
                 *           / \
                 * outset:  O   O   <- the task we are inserting
                 */
                depend->out = task_create(TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_OUTSET, 0);
                ARRAY_FOREACH_BEGIN(&depend->ins, task_t **, in)
                {
                    // prevent cyclic deps in case 'task' already had an 'in'
                    // dep type on the same addr previously
                    if (*in != task)
                        task_set_edge(*in, depend->out);
                }
                ARRAY_FOREACH_END(&depend->ins, task_t **, in)

                task_set_edge(depend->out, task);
                array_clear(&depend->ins);
            }
            else
#endif
            {
                ARRAY_FOREACH_BEGIN(&depend->ins, task_t **, in)
                    if (*in != task)
                        task_set_edge(*in, task);
                ARRAY_FOREACH_END(&depend->ins, task_t **, in)
            }
        } // 1.1

        // 1.2 - the generated task is dependent of previous 'outset'
        if (!array_is_empty(&depend->outsets) && (type == TASKGRIND_IN || type == TASKGRIND_OUT))
        {
# if 0
            if (type == TASKGRIND_IN)
            {
                /**
                 * outset:          O O O   <- the predecessor
                 *                   \|/
                 * out:               X     <- we insert this empty node
                 *                   / \
                 * in:              O   O   <- the task we are inserting
                 */
                depend->out = task_create(TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_OUTSET, 0);
                ARRAY_FOREACH_BEGIN(&depend->outsets, task_t **, outset)
                    task_set_edge(*outset, depend->out);
                ARRAY_FOREACH_END(&depend->outsets, task_t **, outset)

                task_set_edge(depend->out, task);
                array_clear(&depend->outsets);
            }
            else
#endif
            {
                // tl_assert(type == TASKGRIND_OUT);
                ARRAY_FOREACH_BEGIN(&depend->outsets, task_t **, outset)
                    task_set_edge(*outset, task);
                ARRAY_FOREACH_END(&depend->outsets, task_t **, outset)
            }
        } // 1.2

        // 1.3 - the generated task is dependent of previous 'out'
        if (depend->out && (type == TASKGRIND_OUT || type == TASKGRIND_IN || type == TASKGRIND_OUTSET))
        {
            if (type == TASKGRIND_OUT &&
                    (!array_is_empty(&depend->ins) || !array_is_empty(&depend->outsets)))
            {
                // nothing to do, the task already depend on a previous 'in'
                // or 'outset' that depend on the 'depend->out'
            }
            else
            {
                task_set_edge(depend->out, task);
            }
        }

        // save access for future task
        switch (type)
        {
            case (TASKGRIND_IN):
            {
                array_push(&depend->ins, &task);
                break ;
            }

            case (TASKGRIND_OUT):
            {
                array_clear(&depend->ins);
                array_clear(&depend->outsets);
                depend->out = task;
                break ;
            }

            case (TASKGRIND_OUTSET):
            {
                array_push(&depend->outsets, &task);
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
// adding an empty task node which depend on all previously created tasks with
// no successors (leaves)
void
task_sync(void)
{
    // create an empty task (the sync barrier)
    task_create(TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_BARRIER, 0);
}

// memory accesses
static inline void
task_seg_mem_access(task_seg_t * seg, Addr addr, SizeT size)
{
    if (seg->ctx == NULL)
    {
        ThreadId tid = VG_(get_running_tid)();
        if (tid != VG_INVALID_THREADID)
            seg->ctx = VG_(record_ExeContext)(tid, 0);
    }
}

void
task_mem_load(Addr addr, SizeT size)
{
    task_seg_t * seg = task_seg_get_current();

    #if 0
    if (addr == 68537960 && seg->uid == 768)
    {
        ThreadId tid = VG_(get_running_tid)();
        VG_(get_and_pp_StackTrace)(tid, 5);
        TASKGRIND_DEBUG("(task=%p, seg=%u) LOAD        0x%010lX %lu",
                (void *) CURRENT_TASK->id, seg->uid, addr, size);

    }
    #endif

    tl_assert(seg);
    SPMT_FILL(&seg->loads, addr, addr + size);

    task_seg_mem_access(seg, addr, size);
}

void
task_mem_store(Addr addr, SizeT size)
{
    task_seg_t * seg = task_seg_get_current();

    #if 0
    if (addr == 68537960 && seg->uid == 768)
    {
        ThreadId tid = VG_(get_running_tid)();
        VG_(get_and_pp_StackTrace)(tid, 5);
        TASKGRIND_DEBUG("(task=%p, seg=%u) STORE       0x%010lX %lu",
                (void *) CURRENT_TASK->id, seg->uid, addr, size);

    }
    #endif

    tl_assert(seg);
    SPMT_FILL(&seg->stores, addr, addr + size);

    task_seg_mem_access(seg, addr, size);
}

void
task_mem_load_atomic(Addr addr, SizeT size)
{
#if 0
    if (CURRENT_TASK->id == 3)
        TASKGRIND_DEBUG("(task=%p) LOAD ATOMIC  0x%010lX %lu",
                (void *) CURRENT_TASK->id, addr, size);
#endif
}

void
task_mem_store_atomic(Addr addr, SizeT size)
{
#if 0
    if (CURRENT_TASK->id == 3)
        TASKGRIND_DEBUG("(task=%p) STORE ATOMIC 0x%010lX %lu",
                (void *) CURRENT_TASK->id, addr, size);
#endif
}

// Initialize execution
void
task_init(void)
{
    array_init(&SEGS, 8192, sizeof(task_seg_ref_t));
    __task_init(&ROOT_TASK, TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_ROOT, 1);
    CURRENT_TASK = &ROOT_TASK;
}

// Execution terminated, perform analysis and report here
void
task_fini(void)
{
    if (CLOS.dump)
    {
        taskgrind_export_tcfg(&ROOT_TASK);
        taskgrind_export_tdgx_recursive(&ROOT_TASK);
        taskgrind_export_lpg((task_seg_t *)array_first(&ROOT_TASK.segs));
    }

    TASKGRIND_INFO("Starting analysis on a %u segments graph...", SEGS.n);
//    taskgrind_pass_w1(&ROOT_TASK);
    taskgrind_pass_e1(&ROOT_TASK);
    TASKGRIND_INFO("Analysis completed.");

    array_deinit(&SEGS);
}
