# include "print.h"
# include "task.h"
# include "taskgrind_spmt.h"

// TODO: analysis code bellow is experimental and temporary

// TODO: currently, this is used as a quick and dirty fix to ignore stack
// accesses, detecting them if they are 'close' to the current stack pointer
// (<1Go).  Otherwise, stack accesses causes every tasks to be inter-dependent,
// as they only execute on the same thread, on the same stack, one after
// another
static inline int
pass_e5_addr_is_stack(SPMT_PTR_T addr)
{
    // adress of the top of the stack
    static SPMT_PTR_T STACK_BEGIN           = 0x1fffffffff;

    // distance bellow the access is assumed on the stack (64Go of stacks lol)
    static SPMT_PTR_T STACK_MAX_DISTANCE    = 0x0fffffffff;

    return (STACK_BEGIN - STACK_MAX_DISTANCE <= addr) && (addr <= STACK_BEGIN);
}

typedef struct  walk_e5_s
{
    int contains_only_stack_accesses;
    int contains_parent_stack_accesses;
    SPMT_PTR_T task_stack_pointer;

}               walk_e5_t;

static int
pass_e5_inter_check(SPMT_PTR_T begin, SPMT_PTR_T end, void * opaque)
{
    #if 0
    TASKGRIND_INFO("Common access on [%lu, %lu[", begin, end);
    #endif

    walk_e5_t * walk = (walk_e5_t *) opaque;
    if (!pass_e5_addr_is_stack(begin) || !pass_e5_addr_is_stack(end))
    {
        walk->contains_only_stack_accesses = 0;
        return 1;
    }
    else
    {
        if (begin < walk->task_stack_pointer)
        {
            walk->contains_parent_stack_accesses = 1;
            return 1;
        }
    }
    return 0;
}

static inline void
pass_e5_check_deps(task_part_t * pred, task_part_t * succ)
{
    #if 0
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p", (void *) pred->task->client_id);
    TASKGRIND_INFO("--------------------");
    SPMT_DUMP_FILLED(VG_(umsg), &pred->stores);
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p", (void *) succ->task->client_id);
    TASKGRIND_INFO("--------------------");
    SPMT_DUMP_FILLED(VG_(umsg), &succ->stores);
    #endif

    spmt_t inter;
    SPMT_INTERSECT(&inter, &pred->stores, &succ->stores);

    #if 0
    TASKGRIND_INFO("--------------------------------");
    TASKGRIND_INFO("Intersect tasks %p n %p", (void*)pred->task->client_id, (void*)succ->task->client_id);
    TASKGRIND_INFO("------------------------------");
    SPMT_DUMP_FILLED(VG_(umsg), &inter);
    #endif

    int err;

    int empty_intersect = SPMT_IS_EMPTY(&inter);

    if (!empty_intersect)
    {
        walk_e5_t walk = {
            .contains_only_stack_accesses = 1,
            .contains_parent_stack_accesses = 0,
            .task_stack_pointer = 0,    // TODO : get stack pointer at the start of this task_part

        };
        SPMT_FOREACH_FILLED(&inter, pass_e5_inter_check, &walk);

        if (!walk.contains_only_stack_accesses)
            err = 1;
        else
            err = walk.contains_parent_stack_accesses;
    }
    else
        err = 0;

    if (err)
    {
        TASKGRIND_WARN("  %p and %p were declared dependent having no data dependencies",
                (void *)pred->task->client_id,
                (void *)succ->task->client_id);
    }
    else
    {
        TASKGRIND_INFO("  No error detected, all good :-)");
    }


    SPMT_RELEASE(&inter);
}

static void
pass_e5(task_part_t * pred)
{
    tl_assert(pred->task);
    tl_assert(pred->task->parts.n > 0);

    ARRAY_FOREACH_BEGIN(&pred->successors, task_part_ref_t *, succ_ref)
    {
        task_part_t * succ = succ_ref->task->parts.parts + succ_ref->id;
        tl_assert(succ->task);
        if (succ->task != pred->task
                && succ->task->type == TASK_TYPE_EXPLICIT
                && pred->task->type == TASK_TYPE_EXPLICIT
                && succ->task->parts.n == 1
                && pred->task->parts.n == 1)
        {
            pass_e5_check_deps(pred, succ);
        }
        pass_e5(succ);
    }
    ARRAY_FOREACH_END(&pred->successors, task_part_t *, succ);
}

// a simple pass checking overly-dependent tasks
void
taskgrind_pass_e5(task_t * root)
{
    TASKGRIND_INFO("Running E5 pass");
    task_part_t * root_part = (task_part_t *) array_first(&root->parts);
    pass_e5(root_part);
}
