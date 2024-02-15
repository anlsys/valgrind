# include "print.h"
# include "task.h"
# include "taskgrind.h"
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
    // distance bellow the access is assumed on the stack (64Go of stacks lol)
    static SPMT_PTR_T STACK_MAX_DISTANCE    = 0x0fffffffff;

    return (TASKGRIND_BASE_STACK_PTR - STACK_MAX_DISTANCE <= addr) && (addr <= TASKGRIND_BASE_STACK_PTR);
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

static inline
void task_store_union(task_t * task, spmt_t * accesses)
{
    SPMT_INITIALIZE(accesses);
    SPMT_DUMP(VG_(umsg), accesses);

    task_seg_t * task_seg = (task_seg_t *) array_first(&task->segs);

    //SPMT_UNION(accesses, accesses, &task_seg->stores);

   // ARRAY_FOREACH_BEGIN(&task->segs, task_seg_t *, task_seg)
   // {
   //     SPMT_UNION(accesses, accesses, &task_seg->stores);
   // }
   // ARRAY_FOREACH_END(&task->segs, task_seg_t *, task_seg);
}

static inline void
pass_e5_check_deps(task_t * pred, task_t * succ)
{
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("CHECKING");
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p (rsp=%lu)", (void *) pred->client_id, pred->sp);
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p (rsp=%lu)", (void *) succ->client_id, succ->sp);
    TASKGRIND_INFO("--------------------");

    spmt_t pred_stores;
    task_store_union(pred, &pred_stores);
    SPMT_DUMP_FILLED(VG_(umsg), &pred_stores);


    #if 0
    //SPMT_DUMP_FILLED(VG_(umsg), &pred->stores);
    //SPMT_DUMP_FILLED(VG_(umsg), &succ->stores);

    spmt_t inter;
    SPMT_INTERSECT(&inter, &pred->stores, &succ->stores);

    //TASKGRIND_INFO("--------------------------------");
    //TASKGRIND_INFO("Intersect tasks %p n %p", (void*)pred->task->client_id, (void*)succ->task->client_id);
    //TASKGRIND_INFO("------------------------------");
    //SPMT_DUMP_FILLED(VG_(umsg), &inter);

    int err;
    int empty_intersect = SPMT_IS_EMPTY(&inter);

    if (empty_intersect)
    {
        err = 1;
    }
    else
    {
        walk_e5_t walk = {
            .contains_only_stack_accesses = 1,
            .contains_parent_stack_accesses = 0,
            .task_stack_pointer = 0,    // TODO : get stack pointer at the start of this task_seg
        };
        SPMT_FOREACH_FILLED(&inter, pass_e5_inter_check, &walk);

        if (!walk.contains_only_stack_accesses)
            err = 1;
        else
            err = walk.contains_parent_stack_accesses;
    }

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
    #endif
}

static void
pass_e5(task_t * pred)
{
    tl_assert(pred);
    tl_assert(pred->type == TASK_TYPE_EXPLICIT);

    ARRAY_FOREACH_BEGIN(&pred->successors, task_t **, succ_ptr)
    {
        task_t * succ = *succ_ptr;
        tl_assert(succ);
        tl_assert(pred != succ);

        switch (succ->type)
        {
            case (TASK_TYPE_EXPLICIT):
            {
                pass_e5_check_deps(pred, succ);
                pass_e5(succ);
                break;
            }

            case (TASK_TYPE_IMPLICIT_OUTSET):
            {
                // succ is 'outset' empty node, we check its successors
                ARRAY_FOREACH_BEGIN(&succ->successors, task_t **, actual_succ_ptr)
                {
                    task_t * actual_succ = *actual_succ_ptr;
                    tl_assert(actual_succ);
                    tl_assert(pred != actual_succ);

                    pass_e5_check_deps(pred, actual_succ);
                    pass_e5(succ);
                }
                ARRAY_FOREACH_END(&succ->successors, task_t **, actual_succ_ptr);
                break ;
            }

            default:
            {
                break ;
            }
        }
    }
    ARRAY_FOREACH_END(&pred->successors, task_t **, succ_ptr);
}

// a simple pass checking overly-dependent tasks
void
taskgrind_pass_e5(task_t * root)
{
    if (root->parent == NULL)
        TASKGRIND_INFO("Running E5 pass");

    ARRAY_FOREACH_BEGIN(&root->children, task_t **, child_ptr)
    {
        task_t * child = *child_ptr;
        tl_assert(child);

        if (child->type == TASK_TYPE_EXPLICIT)
            pass_e5(child);

        taskgrind_pass_e5(child);
    }
    ARRAY_FOREACH_END(&root->children, task_t **, child_ptr);
}
