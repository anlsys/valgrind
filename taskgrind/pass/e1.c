# include "error.h"
# include "print.h"
# include "location.h"
# include "task.h"
# include "taskgrind.h"
# include "taskgrind_spmt.h"

# include "pub_tool_errormgr.h"

static int E1_ERRORS = 0;

static inline void
pass_e1_report_err(task_seg_t * seg_a, task_seg_t * seg_b)
{
    // retrieve location
    HChar loc_a[256];
    task_seg_get_location(seg_a, 0, loc_a, sizeof(loc_a));

    HChar loc_b[256];
    task_seg_get_location(seg_b, 0, loc_b, sizeof(loc_b));

    // output error

    // VALGRIND error system requires the program to be executing
    // Here, we are running after client program termination...
#if 0

    ThreadId tid = seg_a->tid;
    ErrorKind kind = TASKGRIND_E1;
    Addr a = 0;
    const HChar * s = "Hello";
    void * extra = NULL;
    VG_(maybe_record_error)(tid, kind, a, s, extra);
#else
    //TASKGRIND_WARN("  Segments %s (task=%p, sp=%lu, uid=%u) and %s (task=%p, sp=%lu, uid=%u) were declared independent while accessing the same memory address", loc_a, seg_a->task, seg_a->task->sp, seg_a->uid, loc_b, seg_b->task, seg_b->task->sp, seg_b->uid);
    TASKGRIND_WARN("  Segments %s and %s were declared independent while accessing the same memory address", loc_a, loc_b);
    ++E1_ERRORS;
#endif
}

// TODO: analysis code bellow is experimental and temporary

// TODO: currently, this is used as a quick and dirty fix to ignore stack
// accesses, detecting them if they are 'close' to the current stack pointer
// (<1Go).  Otherwise, stack accesses causes every tasks to be inter-dependent,
// as they only execute on the same thread, on the same stack, one after
// another
static inline int
pass_e1_addr_is_stack(SPMT_PTR_T addr)
{
    // distance bellow the access is assumed on the stack (64Go of stacks lol)
    static SPMT_PTR_T STACK_MAX_DISTANCE    = 0x0fffffffff;

    return (TASKGRIND_BASE_STACK_PTR - STACK_MAX_DISTANCE <= addr) && (addr <= TASKGRIND_BASE_STACK_PTR);
}

typedef struct  walk_e1_intersect_s
{
    int contains_heap_accesses;
    int contains_parent_stack_accesses;
    SPMT_PTR_T task_stack_pointer;

}               walk_e1_intersect_t;

static int
pass_e1_intersect_check(SPMT_PTR_T begin, SPMT_PTR_T end, void * opaque)
{
    #if 0
    TASKGRIND_INFO("Common access on [%lu, %lu[", begin, end);
    #endif

    walk_e1_intersect_t * walk = (walk_e1_intersect_t *) opaque;
    if (!pass_e1_addr_is_stack(begin) || !pass_e1_addr_is_stack(end))
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

typedef struct  walk_e1_precedes_s
{
    task_seg_t * seg_b;
    UInt precedes;
}               walk_e1_precedes_t;

static UInt
pass_e1_happens_before_walk(task_seg_t * curr_seg, void * opaque)
{
    walk_e1_precedes_t * walk = (walk_e1_precedes_t *) opaque;

    if (curr_seg == walk->seg_b)
    {
        walk->precedes = 1;
        return 1;
    }
    return 0;
}

// TODO : compute precedence relation once for all in an optimized way
// returns '1' 'seg_a' precedes 'seg'b' ; else 0
static UInt
pass_e1_seg_precedes(task_seg_t * seg_a, task_seg_t * seg_b)
{
    tl_assert(seg_a != seg_b);
    tl_assert(seg_a->task != seg_b->task);

    walk_e1_precedes_t walk = {
        .seg_b = seg_b,
        .precedes = 0,
    };
    task_seg_foreach_from(pass_e1_happens_before_walk, &walk, seg_a);
    return walk.precedes;
}

UInt
pass_e1_walk_compare(task_seg_t * seg_a, void * opaque)
{
    task_seg_t * seg_b = (task_seg_t *) opaque;

    // if comparing the same segment to itself, no determinacy race possible
    if (seg_a->task == seg_b->task)
        return 0;

    // avoid reporting twice the same error
    if (seg_a->uid > seg_b->uid)
        return 0;

    // if a segment precede another, no determinacy race possible
    if (pass_e1_seg_precedes(seg_a, seg_b) || pass_e1_seg_precedes(seg_a, seg_b))
        return 0;

    // segments are independent, check there memory accesses to ensure correctness
    // check that
    //  - Wa n (Rb u Wb) == {}
    //  - Wb n (Ra u Wa) == {}
    // else, it means there exist memory addresses for which at least one writes while the other accesses it

    // TODO
    spmt_t RaWa;
    SPMT_INITIALIZE(&RaWa);
    SPMT_APPEND(&RaWa, &seg_a->loads);
    SPMT_APPEND(&RaWa, &seg_a->stores);

    spmt_t Wb_inter_RaWa;
    SPMT_INITIALIZE(&Wb_inter_RaWa);
    SPMT_INTERSECT(&Wb_inter_RaWa, &seg_b->stores, &RaWa);

    spmt_t RbWb;
    SPMT_INITIALIZE(&RbWb);
    SPMT_APPEND(&RbWb, &seg_b->loads);
    SPMT_APPEND(&RbWb, &seg_b->stores);

    spmt_t Wa_inter_RbWb;
    SPMT_INITIALIZE(&Wa_inter_RbWb);
    SPMT_INTERSECT(&Wa_inter_RbWb, &seg_a->stores, &RbWb);

    UInt err;

    // intersect are empty, meaning both segments are accessing disjoint memory spaces
    if (SPMT_IS_EMPTY(&Wb_inter_RaWa) && SPMT_IS_EMPTY(&Wa_inter_RbWb))
    {
        err = 0;
    }
    // intersect is not empty
    else
    {
        //  Example with error
        //
        //  4       <- seg_a->task->sp
        //  3           <- seg_a and seg_b accesses
        //  2       <- seg_b->task->sp
        //  1           -> seg_b accesses
        //  0
        walk_e1_intersect_t walk = {
            .contains_heap_accesses = 0,
            .contains_parent_stack_accesses = 0,
            .task_stack_pointer = seg_a->task->sp < seg_b->task->sp ? seg_a->task->sp : seg_b->task->sp,
        };
        if (!SPMT_IS_EMPTY(&Wb_inter_RaWa))
            SPMT_FOREACH_FILLED(&Wb_inter_RaWa, pass_e1_intersect_check, &walk);

        if (!SPMT_IS_EMPTY(&Wa_inter_RbWb) && !walk.contains_heap_accesses && !walk.contains_parent_stack_accesses)
            SPMT_FOREACH_FILLED(&Wa_inter_RbWb, pass_e1_intersect_check, &walk);

        // if both segments are accessing the same heap space
        if (walk.contains_heap_accesses)
        {
            err = 1;
        }
        // else, both segments are accessing the same stack space
        else
        {
            #if 0
            // if one segment is accessing stack space outside its allocated stack
            if (walk.contains_parent_stack_accesses)
            {
                err = 1;
                // TODO : what is going on
            }
            else
            {
                err = 0;
            }
            #endif
            err = 0;
        }
    }

    if (err)
    {
        pass_e1_report_err(seg_a, seg_b);
    }

    SPMT_RELEASE(&RaWa);
    SPMT_RELEASE(&Wa_inter_RbWb);
    SPMT_RELEASE(&RbWb);
    SPMT_RELEASE(&Wb_inter_RaWa);

    return 0;
}

static UInt
pass_e1_walk(task_seg_t * curr_seg, void * opaque)
{
    task_seg_foreach(pass_e1_walk_compare, (void *) curr_seg);
    return 0;
}

// a simple pass checking overly-dependent tasks
void
taskgrind_pass_e1(task_t * root)
{
    TASKGRIND_INFO("Running E1 pass");
    task_seg_foreach(pass_e1_walk, NULL);

    if (E1_ERRORS)
        TASKGRIND_WARN("-> E1 reported %d possible determinacy races", E1_ERRORS);
    else
        TASKGRIND_INFO("-> E1 found no determinacy races :-)");
}
