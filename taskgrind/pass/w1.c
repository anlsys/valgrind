# include "print.h"
# include "location.h"
# include "task.h"
# include "taskgrind.h"
# include "taskgrind_spmt.h"

static int W1_COUNT = 0;

// TODO: analysis code bellow is experimental and temporary

// TODO: currently, this is used as a quick and dirty fix to ignore stack
// accesses, detecting them if they are 'close' to the current stack pointer
// (<1Go).  Otherwise, stack accesses causes every tasks to be inter-dependent,
// as they only execute on the same thread, on the same stack, one after
// another
static inline int
pass_w1_addr_is_stack(SPMT_PTR_T addr)
{
    // distance bellow the access is assumed on the stack (64Go of stacks lol)
    static SPMT_PTR_T STACK_MAX_DISTANCE    = 0x0fffffffff;

    return (TASKGRIND_BASE_STACK_PTR - STACK_MAX_DISTANCE <= addr) && (addr <= TASKGRIND_BASE_STACK_PTR);
}

typedef struct  walk_w1_s
{
    int contains_heap_accesses;
    int contains_parent_stack_accesses;
    SPMT_PTR_T task_stack_pointer;

}               walk_w1_t;

static int
pass_w1_intersect_check(SPMT_PTR_T begin, SPMT_PTR_T end, void * opaque)
{
    #if 0
    TASKGRIND_INFO("Common access on [%lu, %lu[", begin, end);
    #endif

    walk_w1_t * walk = (walk_w1_t *) opaque;
    if (!pass_w1_addr_is_stack(begin) || !pass_w1_addr_is_stack(end))
    {
        walk->contains_heap_accesses = 1;
        return 1;
    }
    else
    {
        if (end > walk->task_stack_pointer)
        {
            walk->contains_parent_stack_accesses = 1;
            return 1;
        }
    }
    return 0;
}

static inline
void task_accesses_union(task_t * task, spmt_t * accesses, int load, int store)
{
    SPMT_INITIALIZE(accesses);
    ARRAY_FOREACH_BEGIN(&task->segs, task_seg_t *, task_seg)
    {
        if (load)
            SPMT_APPEND(accesses, &task_seg->loads);
        if (store)
            SPMT_APPEND(accesses, &task_seg->stores);
    }
    ARRAY_FOREACH_END(&task->segs, task_seg_t *, task_seg);
}

static inline void
pass_w1_check_deps(task_t * pred, task_t * succ)
{

#if 0
    task_seg_t * pred_seg = (task_seg_t *) array_first(&pred->segs);
    tl_assert(pred_seg);

    task_seg_t * succ_seg = (task_seg_t *) array_first(&succ->segs);
    tl_assert(succ_seg);

    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("CHECKING");
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p (rsp=%lu) at %s", (void *) pred->id, pred->sp, task_seg_get_location(pred_seg));
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p (rsp=%lu) at %s", (void *) succ->id, succ->sp, task_seg_get_location(succ_seg));
    TASKGRIND_INFO("--------------------");

#endif

    spmt_t pred_l;
    task_accesses_union(pred, &pred_l, 1, 0);

    spmt_t pred_s;
    task_accesses_union(pred, &pred_s, 0, 1);

    spmt_t succ_ls;
    task_accesses_union(succ, &succ_ls, 1, 1);

    spmt_t succ_s;
    task_accesses_union(succ, &succ_s, 0, 1);

    spmt_t inter_s_ls;
    SPMT_INITIALIZE(&inter_s_ls);
    SPMT_INTERSECT(&inter_s_ls, &pred_s, &succ_ls);

    spmt_t inter_l_s;
    SPMT_INITIALIZE(&inter_l_s);
    SPMT_INTERSECT(&inter_l_s, &pred_l, &succ_s);

    int warn;
    if (SPMT_IS_EMPTY(&inter_s_ls) && SPMT_IS_EMPTY(&inter_l_s))
    {
        // the two tasks does not access shared memory
        warn = 1;
    }
    else
    {
        walk_w1_t walk = {
            .contains_heap_accesses = 0,
            .contains_parent_stack_accesses = 0,
            .task_stack_pointer = pred->sp < succ->sp ? pred->sp : succ->sp,
        };
        SPMT_FOREACH_FILLED(&inter_s_ls, pass_w1_intersect_check, &walk);
        SPMT_FOREACH_FILLED(&inter_l_s, pass_w1_intersect_check, &walk);
        if (walk.contains_heap_accesses)
        {
            // the two tasks access same address in the heap memory
            warn = 0;
        }
        else
        {
            // the two tasks access same address in the stack memory
            if (walk.contains_parent_stack_accesses)
            {
                // the two tasks access same address in the stack memory
                // referenced by their parent
                warn = 0;
            }
            else
            {
                // the two tasks access same address in the stack memory
                // being scheduled one after the other on the same thread
                warn = 1;
            }
        }
    }

    SPMT_RELEASE(&pred_s);
    SPMT_RELEASE(&succ_ls);
    SPMT_RELEASE(&inter_s_ls);

    if (warn)
    {
        task_seg_t * pred_seg = (task_seg_t *) array_first(&pred->segs);
        tl_assert(pred_seg);

        task_seg_t * succ_seg = (task_seg_t *) array_first(&succ->segs);
        tl_assert(succ_seg);

        HChar pred_loc[256];
        task_seg_get_location(pred_seg, 0, pred_loc, 256);

        HChar succ_loc[256];
        task_seg_get_location(succ_seg, 0, succ_loc, 256);

        TASKGRIND_WARN("  %p (%s) and %p (%s) were declared dependent having no data dependencies",
                (void *)pred->id,
                pred_loc,
                (void *)succ->id,
                succ_loc
         );
        ++W1_COUNT;
    }
    else
    {
        //TASKGRIND_INFO("  No needless dependencies detected, all good :-)");
    }
}

static void
pass_w1(task_t * pred)
{
    if (pred->flag)
        return ;
    pred->flag = 1;

    tl_assert(pred);
    tl_assert(pred->type == TASK_TYPE_EXPLICIT);

    ARRAY_FOREACH_BEGIN(&pred->successors, task_t **, succ_ptr)
    {
        task_t * succ = *succ_ptr;
        tl_assert(succ);
        if (pred == succ)
            continue ;
        tl_assert(pred != succ);

        switch (succ->type)
        {
            case (TASK_TYPE_EXPLICIT):
            {
                pass_w1_check_deps(pred, succ);
                pass_w1(succ);
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

                    pass_w1_check_deps(pred, actual_succ);
                    pass_w1(actual_succ);
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
taskgrind_pass_w1(task_t * root)
{
    if (root->parent == NULL)
        TASKGRIND_INFO("Running W1 pass");

    ARRAY_FOREACH_BEGIN(&root->children, task_t **, child_ptr)
    {
        task_t * child = *child_ptr;
        tl_assert(child);

        if (child->type == TASK_TYPE_EXPLICIT)
            pass_w1(child);

        taskgrind_pass_w1(child);
    }
    ARRAY_FOREACH_END(&root->children, task_t **, child_ptr);

    if (root->parent == NULL)
    {
        if (W1_COUNT)
            TASKGRIND_WARN("-> W1 reported %d issue", W1_COUNT);
        else
            TASKGRIND_INFO("-> W1 found no issues :-)");
    }
}
