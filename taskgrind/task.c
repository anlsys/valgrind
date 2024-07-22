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

// Threads
static thread_t THREADS[TASKGRIND_MAX_THREADS];

static inline thread_t *
thread_get(void)
{
    ThreadId tid = VG_(get_running_tid)();
    tl_assert(tid >= 0);
    tl_assert(tid < TASKGRIND_MAX_THREADS);
    return THREADS + tid;
}

static inline task_t *
task_get_root(void)
{
    return &THREADS[0].root_task;
}

static inline task_t *
task_get_current(void)
{
    thread_t * thread = thread_get();
    tl_assert(thread);
    return thread->current_task;
}

static inline task_seg_t *
task_seg_get_current(void)
{
    task_t * task = task_get_current();
    tl_assert(task);
    return array_last(&task->segs);
}

// The tasks hmap
static task_t * TASKS;

task_t *
task_get(UWord id)
{
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&id, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, TASKS, &id, sizeof(UWord), hashv, task);

    return task;
}

// List of all segments
array_t SEGS;

// segments of the last fork/join point
static task_seg_t * FORK_SEG;
static task_seg_t * JOIN_SEG;

// DFS version
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
    task_t * root = task_get_root();
    tl_assert(root);

    task_seg_t * root_seg = (task_seg_t *) array_first(&root->segs);
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
    seg->tls.tp = 0;
    array_init(&seg->tls.dtv, 4, sizeof(Addr) * 2);
    seg->dfs_version = 0;

    ThreadId tid = VG_(get_running_tid)();
    if (tid != VG_INVALID_THREADID)
        seg->tid = tid;

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
    task_t * current_task = task_get_current();

    // set attributes
    task->type              = type;
    task->child_id          = current_task ? ++current_task->next_child_id : -1;
    task->next_child_id     = 0;
    task->id                = id;
    array_init(&task->successors, 4, sizeof(task_t *));
    task->depend            = NULL;
    task->parent            = current_task;
    array_init(&task->children, 0, sizeof(task_t *));
    task->last_taskwait     = NULL;
    task->last_barrier      = NULL;
    array_init(&task->segs, 1, sizeof(task_seg_t));
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
#elif defined(VGA_arm64)
        task->sp = state->guest_XSP;
#else
# error "Arch not supported"
#endif
    }

    // tcfg parent reference
    if (current_task)
        array_push(&current_task->children, &task);
}

static inline task_t *
task_new(UWord id, task_type_t type, UWord undeferred)
{
    task_t * task = (task_t *) VG_(malloc)("task_new", sizeof(task_t));
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

// TODO: code with several assumptions
//  - architecture - VGA_amd64 - VGA_x86
//  - using a variant II
//  - DTV location located at TCB+0x8
#if defined(VGA_amd64) || defined(VGA_x86)

typedef struct
{
    union {
        ULong gen;
        Addr addr;
        Addr counter;
    };
    Addr unused;
} dtv_t;

#endif

// the given segment terminated
static inline void
task_seg_fini(task_t * task, task_seg_t * seg)
{
    // Save the current TLS information
    // [WIP] only partial support for Variant II of X86_64, see 'docs/tls.pdf'
    // TODO : is it correct dereferencing TCB/DTV structures like I do here ?
    // TODO : find how to retrieve 'N' = 'N1' + 'N2'
    // TODO : find how to retrieve 'N1' : the number of static TLS blocks
    // TODO : find how to retrieve 'N2' : the number of modules loaded for dynamic TLS
    // -> the impl. currently assumes 'N=N1' and 'N2=0'

    VexGuestArchState * state = VG_(get_CurrentThreadArchState)();
# if defined(VGA_amd64) || defined(VGA_x86)

#  if defined(VGA_amd64)
    seg->tls.tp = (Addr) state->guest_FS_CONST;
#  else /* defined(VGA_x86) */
    seg->tls.tp = (Addr) state->guest_FS;
# endif

    Addr ** dtv_loc = (Addr **) (seg->tls.tp + 0x8);
    dtv_t * dtv = (dtv_t *) dtv_loc[0];

    // assertion for Variant II
    // tl_assert(              seg->tls.tp < (Addr) dtv);
    tl_assert(dtv[1].addr < seg->tls.tp             );

    // dtv[0]     is gen(t)
    // dtv[1]     is dtv(t,1)   - static
    // dtv[2]     is dtv(t,2)   - static
    // [...]
    // dtv[N1]    is dtv(t, N1) - static
    // dtv[N1+1]  is dtv(t, N1) - dynamic
    // [...]
    // dtv[N1+N2] is dtv(t, n)  - dynamic

    // loop on each dtv(t, i) entry

    // https://sourceware.org/git/?p=glibc.git;a=blob;f=elf/dl-tls.c;h=670dbc42fc2e3334739e115dd12390a8abefbd49;hb=HEAD#l484
    unsigned N = dtv[-1].counter;
    array_clear(&(seg->tls.dtv));

    // TODO : this code is wrong if there is dynamic TLS block in the dtv
    for (int m = 1 ; m <= N && dtv[m].addr ; ++m)
    {
        Addr block[2] = {
                                     dtv[m  ].addr,
            (m == 1) ? seg->tls.tp : dtv[m-1].addr
        };
        // TASKGRIND_DEBUG("tls [%p, %p] block n°%d", block[0], block[1], m);
        array_push(&seg->tls.dtv, &block);
    }

#else
# pragma message("TLS support not implemented for this architecture")
# endif
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

    task_t * current_task = task_get_current();
    tl_assert(current_task);

    /////////
    // TDG //
    /////////
    // add edges with respect to previous taskwait and barrier
    if (current_task->last_taskwait)
        task_set_edge(current_task->last_taskwait, task);

    /////////
    // LPG //
    /////////

    // create a new successor seg for the current task
    task_t * succ = current_task;
    task_seg_t * succ_seg = task_seg_new(succ);
    int succ_seg_idx = succ->segs.n - 1;
    tl_assert(succ_seg);

    // retrieve the current seg
    task_t * pred = current_task;
    task_seg_t * pred_seg = (task_seg_t *) array_penultimate(&pred->segs);
 // int pred_seg_idx = pred->segs.n - 2;
    tl_assert(pred_seg);

    // callback : the previous segment terminated
    task_seg_fini(pred, pred_seg);

    // retrieve the new task seg
    task_seg_t * task_seg = (task_seg_t *) array_first(&task->segs);
    int task_seg_idx = task->segs.n - 1;
    tl_assert(task_seg);

    // 'pred' -> 'task'
    task_seg_set_edge(pred_seg, task, task_seg_idx);

    // TODO : barrier and taskwait are 2 different things, fix me
    switch (type)
    {
        // wait for all children tasks of the current task
        case (TASK_TYPE_IMPLICIT_TASKWAIT):
        {
            // for each children task of the current task (excluding the new barrier)
            ARRAY_FOREACH_BEGIN(&current_task->children, task_t **, child)
            {
                if (*child == task)
                    continue ;

                // link leaves with the new task barrier
                if (array_is_empty(&(*child)->successors))
                    task_set_edge(*child, task);
            }
            ARRAY_FOREACH_END(&current_task->children, task_t **, child);

            // 'task' -> 'succ'
            task_seg_set_edge(task_seg, succ, succ_seg_idx);

            // in the future, link each next children with this barrier
            current_task->last_taskwait = task;

            // A barrier has no instructions, retrieve context here
            task_seg->ctx = VG_(record_ExeContext)(VG_(get_running_tid)(), 0);

            // no need to set 'pred' -> 'succ' as we already have 'pred' -> 'task' -> 'succ'
            break ;
        }

        // wait for all tasks and their descendent
        case (TASK_TYPE_IMPLICIT_BARRIER):
        {
            // for each children task of the current task (excluding the new barrier)
            ARRAY_FOREACH_BEGIN(&current_task->children, task_t **, child)
            {
                if (*child == task)
                    continue ;

                // link leaves with the new task barrier
                if (array_is_empty(&(*child)->successors))
                    task_set_edge(*child, task);
            }
            ARRAY_FOREACH_END(&current_task->children, task_t **, child);

            // 'task' -> 'succ'
            task_seg_set_edge(task_seg, succ, succ_seg_idx);

            // in the future, link each next children with this barrier
            current_task->last_barrier = task;

            // A barrier has no instructions, retrieve context here
            task_seg->ctx = VG_(record_ExeContext)(VG_(get_running_tid)(), 0);

            break ;
        }

        case (TASK_TYPE_IMPLICIT_TASKGROUP):
        {
            tl_assert(0 && "Not implemented");
            break ;
        }

        case (TASK_TYPE_UNKNOWN):
        case (TASK_TYPE_EXPLICIT):
        case (TASK_TYPE_IMPLICIT):
        case (TASK_TYPE_IMPLICIT_ROOT):
        case (TASK_TYPE_IMPLICIT_OUTSET):
        case (TASK_TYPE_IMPLICIT_UNKNOWN):
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

// schedule
void
task_schedule(UWord id)
{
    TASKGRIND_DEBUG("schedule");
    task_t * prev = task_get_current();
    task_seg_t * prev_seg = task_seg_get_current();
    task_seg_fini(prev, prev_seg);

    task_t * next = task_get(id);
    tl_assert(prev);
    tl_assert(next);
    tl_assert(prev != next);

    thread_t * thread = thread_get();
    thread->current_task = next;
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
task_sync(taskgrind_sync_t mode)
{
    // create an empty task (the sync barrier)
    task_create(
            TASKGRIND_CLIENT_ID_PRIVATE,
            mode == TASKGRIND_SYNC_BARRIER ? TASK_TYPE_IMPLICIT_BARRIER : TASK_TYPE_IMPLICIT_TASKWAIT,
            0
    );
}

// the current task allows the completion of the passed task
void
task_detach_fulfill(UWord id, taskgrind_fulfill_mode_t mode)
{
    task_t * task = task_get(id);
    tl_assert(task);
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

    # if 0
    if (addr == 0x1ffefff218)
    {
        TASKGRIND_DEBUG("Checking address %p on segment %u", (void *) addr, seg->uid);
        ThreadId tid = VG_(get_running_tid)();
        ExeContext * ec = VG_(record_ExeContext)(tid, 0);
        DiEpoch ep = VG_(get_ExeContext_epoch)(ec);
        Int n_ips = VG_(get_ExeContext_n_ips)(ec);
        Addr * ips = VG_(get_ExeContext_ips)(ec);
        VG_(pp_StackTrace)(ep, ips, 10);
    }
    # endif
}

void
task_mem_load(Addr addr, SizeT size)
{
    // if task is null, the thread has not been initialized (thread_begin) - so ignore this access
    task_t * task = task_get_current();
    if (task == NULL)
        return ;

    task_seg_t * seg = task_seg_get_current();
    tl_assert(seg);

    SPMT_FILL(&seg->loads, addr, addr + size);
    task_seg_mem_access(seg, addr, size);

    #if 0
    if (addr == 68537960 && seg->uid == 768)
    {
        ThreadId tid = VG_(get_running_tid)();
        VG_(get_and_pp_StackTrace)(tid, 5);
        TASKGRIND_DEBUG("(task=%p, seg=%u) LOAD        0x%010lX %lu",
                (void *) CURRENT_TASK->id, seg->uid, addr, size);

    }
    #endif
}

void
task_mem_store(Addr addr, SizeT size)
{
    // if task is null, the thread has not been initialized (thread_begin) - so ignore this access
    task_t * task = task_get_current();
    if (task == NULL)
        return ;

    task_seg_t * seg = task_seg_get_current();
    tl_assert(seg);

    SPMT_FILL(&seg->stores, addr, addr + size);
    task_seg_mem_access(seg, addr, size);


    #if 0
    if (addr == 68537960 && seg->uid == 768)
    {
        ThreadId tid = VG_(get_running_tid)();
        VG_(get_and_pp_StackTrace)(tid, 5);
        TASKGRIND_DEBUG("(task=%p, seg=%u) STORE       0x%010lX %lu",
                (void *) CURRENT_TASK->id, seg->uid, addr, size);

    }
    #endif

    // TASKGRIND_DEBUG("store %u %lu %lu", seg->uid, addr, addr+size);
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


// register the current segment as a fork point for future threads (begin) or remove it (end)
void
task_fork(void)
{
    FORK_SEG = task_seg_get_current();

    ThreadId tid = VG_(get_running_tid)();
    TASKGRIND_DEBUG("set FORK_SEG to %p with %u successors (tid=%d)", FORK_SEG, FORK_SEG->successors.n, tid);
}

void
task_join(void)
{
    JOIN_SEG = task_seg_get_current();
}

void
task_thread_begin(void)
{
    ThreadId tid = VG_(get_running_tid)();
    TASKGRIND_DEBUG("thread begin %d", tid);

    thread_t * thread = thread_get();
    __task_init(&thread->root_task, TASKGRIND_CLIENT_ID_PRIVATE, TASK_TYPE_IMPLICIT_ROOT, 1);
    thread->current_task = &thread->root_task;

    if (FORK_SEG)
    {
        TASKGRIND_DEBUG("seg %p has %u successors", FORK_SEG, FORK_SEG->successors.n);
        task_seg_set_edge(FORK_SEG, thread->current_task, 0);
        TASKGRIND_DEBUG("seg %p has %u successors", FORK_SEG, FORK_SEG->successors.n);
    }
}

void
task_thread_end(void)
{
    // TODO : queue thread and link them on join call
}

// Initialize execution
void
task_init(void)
{
    array_init(&SEGS, 8192, sizeof(task_seg_ref_t));
    task_thread_begin();
    task_fork();
}

// Execution terminated, perform analysis and report here
void
task_fini(void)
{
    task_thread_end();
    task_join();

    task_t * root = task_get_root();

    if (CLOS.dump)
    {
        taskgrind_export_tcfg(root);
        taskgrind_export_tdgx_recursive(root);
        taskgrind_export_lpg(root);
    }

    TASKGRIND_INFO("Starting analysis on a %u segments graph...", SEGS.n);
    // taskgrind_pass_w1(root);
    taskgrind_pass_e1(root);
    TASKGRIND_INFO("Analysis completed.");

    array_deinit(&SEGS);
}
