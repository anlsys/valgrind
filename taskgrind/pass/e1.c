# include "error.h"
# include "print.h"
# include "location.h"
# include "task.h"
# include "taskgrind.h"
# include "taskgrind_spmt.h"

# include "pub_tool_errormgr.h"

static int E1_MALLOC_ADDR_TO_REPORT = 5;
static int E1_MALLOC_N_IPS = 10;
static int E1_ERRORS = 0;

static int
report_err_alloc_addr(SPMT_PTR_T begin, SPMT_PTR_T end, void * opaque)
{
    int * reported = (int *) opaque;
    if (*reported >= E1_MALLOC_ADDR_TO_REPORT)
        return 1;

    taskgrind_alloc_record_t * record = taskgrind_alloc_record_get((void *) begin);
    if (record == NULL)
        return 0;

    tl_assert(record->ctx);

    ExeContext * ec = record->ctx;
    DiEpoch ep = VG_(get_ExeContext_epoch)(ec);
    Int n_ips = VG_(get_ExeContext_n_ips)(ec);
    Addr * ips = VG_(get_ExeContext_ips)(ec);

    HChar buffer[256];
    UInt show_dir = 1;

    TASKGRIND_ERR("    %lu bytes from %p allocated in block %p of size %lu", end - begin, (void *) begin, record->p, record->size);
    // location_get_from_ip(ep, ips[0], show_dir, buffer, sizeof(buffer));
    // TASKGRIND_ERR("         at %s", buffer);

    for (Int i = i ; i < n_ips && i < E1_MALLOC_N_IPS ; ++i)
    {
        location_get_from_ip(ep, ips[i], show_dir, buffer, sizeof(buffer));
        TASKGRIND_ERR("       from %s", buffer);
    }

    return ++(*reported) >= E1_MALLOC_ADDR_TO_REPORT;
}

static inline void
report_err(task_seg_t * seg_a, task_seg_t * seg_b, spmt_t * accesses)
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
    TASKGRIND_ERR("Segments %s and %s were declared independent while accessing the same memory address", loc_a, loc_b);

    int reported = 0;
    SPMT_FOREACH_FILLED(accesses, report_err_alloc_addr, &reported);

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
compare_segments_independent_intersect(SPMT_PTR_T begin, SPMT_PTR_T end, void * opaque)
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

// Confront memory accesses of both segments and report errors
static UInt
compare_segments_independent(task_seg_t * seg_a, task_seg_t * seg_b)
{
    // segments are declared independent,
    // check that
    //  - Wa n (Rb u Wb) == {}
    //  - Wb n (Ra u Wa) == {}
    // else, it means there exist memory addresses for which at least one
    // writes while the other accesses it

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
            SPMT_FOREACH_FILLED(&Wb_inter_RaWa, compare_segments_independent_intersect, &walk);

        if (!SPMT_IS_EMPTY(&Wa_inter_RbWb) && !walk.contains_heap_accesses && !walk.contains_parent_stack_accesses)
            SPMT_FOREACH_FILLED(&Wa_inter_RbWb, compare_segments_independent_intersect, &walk);

        // if both segments are accessing the same heap space
        if (walk.contains_heap_accesses)
        {
            err = 1;
        }
        // else, both segments are accessing the same stack space
        else
        {
            #if 0
            // TODO : check if one segment is accessing stack space outside its
            // allocated stack
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
        spmt_t accesses;
        SPMT_INITIALIZE(&accesses);
        SPMT_APPEND(&accesses, &Wa_inter_RbWb);
        SPMT_APPEND(&accesses, &Wb_inter_RaWa);
        report_err(seg_a, seg_b, &accesses);
        SPMT_RELEASE(&accesses);
    }

    SPMT_RELEASE(&RaWa);
    SPMT_RELEASE(&Wa_inter_RbWb);
    SPMT_RELEASE(&RbWb);
    SPMT_RELEASE(&Wb_inter_RaWa);

    return 0;
}

// segment reachability triangular matrix bits
static UChar * reachability = NULL;

static int
reachability_is_set(task_seg_t * s1, task_seg_t * s2)
{
    UInt i = s1->uid;
    UInt j = s2->uid;

    if (i == j)
        return 1;

    if (i > j)
    {
        UInt tmp = i;
        i = j;
        j = tmp;
    }

    tl_assert(i < j);

    UInt n = SEGS.n;
    UInt k = (n*(n-1)/2) - (n-i)*((n-i)-1)/2 + j - i - 1;
    tl_assert(k >= 0 && k < n*(n-1)/2);
    // return reachability[k];

    UInt byte = k / (8*sizeof(UChar));
    UInt bit = k % (8*sizeof(UChar));
    return reachability[byte] & (1 << bit);
}

static void
reachability_set(task_seg_t * s1, task_seg_t * s2)
{
    UInt i = s1->uid;
    UInt j = s2->uid;

    if (i == j)
        return ;

    if (i > j)
    {
        UInt tmp = i;
        i = j;
        j = tmp;
    }

    tl_assert(i < j);

    UInt n = SEGS.n;
    UInt k = (n*(n-1)/2) - (n-i)*((n-i)-1)/2 + j - i - 1;
    tl_assert(k >= 0 && k < n*(n-1)/2);
    // reachability[k] = 1;

    UInt byte = k / (8*sizeof(UChar));
    UInt bit = k % (8*sizeof(UChar));
    reachability[byte] |= (1 << bit);
}

static UInt
compute_reachability_walk(task_seg_t * s2, void * opaque)
{
    task_seg_t * s1 = (task_seg_t *) opaque;
    reachability_set(s1, s2);
    return 0;
}

static void
compute_reachability(void)
{
    ARRAY_FOREACH_BEGIN(&SEGS, task_seg_ref_t *, s1_ref)
    {
        task_seg_t * s1 = s1_ref->task->segs.segs + s1_ref->id;
        task_seg_dfs_from(compute_reachability_walk, (void *) s1, s1);
    }
    ARRAY_FOREACH_END(&SEGS, task_seg_ref_t *, s1_ref)
}

// a simple pass checking overly-dependent tasks
void
taskgrind_pass_e1(task_t * root)
{
    TASKGRIND_INFO("Running E1 pass");

    // compute the path matrix (triangular bit-flag matrix)
    reachability = (UChar *) VG_(malloc)("taskgrind_pass_e1", SEGS.n * (SEGS.n - 1) / 2 / 8 + 1);
    VG_(memset)(reachability, 0, SEGS.n * (SEGS.n - 1) / 2 / 8 + 1);
    compute_reachability();

    // check errors
    UInt n = SEGS.n;
    UInt processed = 0;
    UInt total = (n-1)*n/2;
    TASKGRIND_INFO("%u comparison to perform", total);
    ARRAY_FOREACH_BEGIN(&SEGS, task_seg_ref_t *, s1_ref)
    {
        task_seg_t * s1 = s1_ref->task->segs.segs + s1_ref->id;
        ARRAY_FOREACH_FROM_BEGIN(&SEGS, s1->uid + 1, task_seg_ref_t *, s2_ref)
        {
            task_seg_t * s2 = s2_ref->task->segs.segs + s2_ref->id;

            if (!reachability_is_set(s1, s2))
                compare_segments_independent(s1, s2);

            int percent = processed++ * 100 / total;
            if (processed % (total / 10 + 1) == 0)
                TASKGRIND_INFO("%d%% comparisons performed", percent);
        }
        ARRAY_FOREACH_FROM_END(&SEGS, s1->uid + 1, task_seg_ref_t *, s2_ref);
    }
    ARRAY_FOREACH_END(&SEGS, task_seg_ref_t *, s1_ref);

    // release reachability
    VG_(free)(reachability);

    // summary of errors
    if (E1_ERRORS)
        TASKGRIND_ERR("-> E1 reported %d possible determinacy races", E1_ERRORS);
    else
        TASKGRIND_INFO("-> E1 found no determinacy races :-)");
}
