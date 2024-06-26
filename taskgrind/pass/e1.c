# include "error.h"
# include "pass.h"
# include "print.h"
# include "location.h"
# include "task.h"
# include "taskgrind.h"
# include "taskgrind_spmt.h"

# include "pub_tool_errormgr.h"

// total errors detected
static int ERRORS = 0;

// threhsold above which errors are no longer reported
static int MAX_ERRORS = 1000;

// output parameters
static int N_MALLOC_ADDR_TO_REPORT  = 5;
static int N_MALLOC_ADDR_IPS        = 10;

// Dump allocation context for the given interval
static void
report_err_alloc(interval_t * I)
{
    taskgrind_alloc_record_t * record = taskgrind_alloc_record_get((void *) I->a);
    if (record == NULL)
        TASKGRIND_ERR("    %lu bytes from %p (unknown allocation)", I->b - I->a, (void *) I->a);
    else
    {
        tl_assert(record->ctx);
        TASKGRIND_ERR("    %lu bytes from %p allocated in block %p of size %lu",
                I->b - I->a, (void *) I->a, record->p, record->size);

        ExeContext * ec = record->ctx;
        DiEpoch ep = VG_(get_ExeContext_epoch)(ec);
        Int n_ips = VG_(get_ExeContext_n_ips)(ec);
        Addr * ips = VG_(get_ExeContext_ips)(ec);
        HChar buffer[256];
        UInt show_dir = 1;

        // i = 1 to skip taskgrind allocator replacement
        for (Int i = 1 ; i < n_ips && i < N_MALLOC_ADDR_IPS ; ++i)
        {
            location_get_from_ip(ep, ips[i], show_dir, buffer, sizeof(buffer));
            TASKGRIND_ERR("       from %s", buffer);
        }
    }
}

// intervals buffer of size 'n' for intersecting segment accesses
static interval_t * intervals   = NULL;
static int n_intervals          = 0;

static inline void
report_err(task_seg_t * seg_a, task_seg_t * seg_b, int r)
{
    // bound error reporting
    if (++ERRORS >= MAX_ERRORS)
    {
        if (ERRORS == MAX_ERRORS)
            TASKGRIND_WARN("Too many errors, I stop reporting kek (errors will still be accounted)");
        return ;
    }

    // retrieve location
    HChar loc_a[256];
    task_seg_get_location(seg_a, 0, loc_a, sizeof(loc_a));

    HChar loc_b[256];
    task_seg_get_location(seg_b, 0, loc_b, sizeof(loc_b));

#if 1
    TASKGRIND_ERR("Segments %s and %s were declared independent while accessing the same memory address", loc_a, loc_b);
# else
    // TASKGRIND_WARN("  Segments %s (task=%p, sp=%lu, uid=%u) and %s (task=%p, sp=%lu, uid=%u) were declared independent while accessing the same memory address", loc_a, seg_a->task, seg_a->task->sp, seg_a->uid, loc_b, seg_b->task, seg_b->task->sp, seg_b->uid);
#endif

    // output error
    for (int i = 0 ; i < r && i < N_MALLOC_ADDR_TO_REPORT ; ++i)
        if (intervals[i].a) // if null, then it is a removed false positive
            report_err_alloc(intervals + i);
}

// TODO: analysis code bellow is experimental and temporary

// TODO: currently, this is used as a quick and dirty fix to ignore stack
// accesses, detecting them if they are 'close' to the current stack pointer
// (<1Go).  Otherwise, stack accesses causes every tasks to be inter-dependent,
// as they only execute on the same thread, on the same stack, one after
// another
static inline int
addr_is_stack(SPMT_PTR_T addr)
{
    static SPMT_PTR_T STACK_MAX_DISTANCE = (SPMT_PTR_T) 64*1000*1000*1000;
    return (TASKGRIND_BASE_STACK_PTR - STACK_MAX_DISTANCE <= addr) && (addr <= TASKGRIND_BASE_STACK_PTR);
}

// TODO: experimental code, with several assumptions
//  - architecture - VGA_amd64 - VGA_x86
//  - using a variant II
//
#if defined(VGA_amd64) || defined(VGA_x86)

typedef struct
{
    union {
        ULong gen;
        Addr addr;
    };
    Addr unused;
} dtv_t;

#endif

// [WIP] only partial support for Variant II of X86_64, see 'docs/tls.pdf'
// Return true if the address executed within the segment 'seg' is stored in
// the executing thread TLS
//
// TODO : analysis are run after the process terminated, so its probably a bad
// idea to dereference TCB/DTV structures here... even though it seems to work
// on minimal benchmarks
//  - move TLS detection at run-time
//  - find how to retrieve 'M' : the number of modules loaded <=> the dtv size
//
static inline int
addr_is_tls(task_seg_t * seg, SPMT_PTR_T addr)
{
//    TASKGRIND_DEBUG("testing %p", (void *) addr);

    return 0;

#if defined(VGA_amd64) || defined(VGA_x86)

    // FS register value, that is tp(t) starting of the TCB for the thread 't'
    Addr tp_t = seg->tls;

    // dtv(t) array location
    Addr ** dtv_loc = (Addr **) (tp_t + 0x8);
    dtv_t * dtv = (dtv_t *) dtv_loc[0];

    // assertion for Variant II
    tl_assert(              tp_t < (Addr) dtv);
    tl_assert(dtv[1].addr < tp_t             );

    // dtv[0] is gen(t)
    // dtv[1] is dtv(t,1)
    // dtv[2] is dtv(t,2)
    // [...]
    // dtv[n] is dtv(t, n)

    // TLS address must be before the TCB with Variant II
    if (addr > tp_t)
        return 0;

    // loop on each dtv(t, i) entry
    int m = 1;
    while (1)
    {
        Addr tlsoffset_t_i = dtv[m].addr;
        if (addr <= tlsoffset_t_i)
            return 1;
        // TODO : how to iterate on TCB blocks ? only read first one currently ...
        break ;
    }
    return 0;
#else
# pragma message("TLS not supported for this architecture")
    return 0;
#endif
}

// Mark the interval as false-positive
static inline void
mark_false_positive(interval_t * I, int * false_positive)
{
    I->a = 0;
    I->b = 0;
    ++(*false_positive);
}

static int
compare_segments_independent_accesses(
    task_seg_t * seg_a,
    task_seg_t * seg_b,
    spmt_t * A,
    spmt_t * B
) {
    // run intersections
    int r = SPMT_INTERSECT(intervals, &n_intervals, A, B);
    if (r == 0)
        return 0;

    // TODO : stack accesses logic may be broken

    // intersection is not empty
    int task_stack_pointer = (seg_a->task->sp < seg_b->task->sp) ? seg_a->task->sp : seg_b->task->sp;
    int false_positive = 0;

    for (int i = 0 ; i < r ; ++i)
    {
        interval_t * I = intervals + i;

        // accessing on the stack
        if (addr_is_stack(I->a))
        {
            // bellow the segment stack pointer
            if (I->b < task_stack_pointer)
                mark_false_positive(I, &false_positive);
        }
        // accessing a TLS
        else if (addr_is_tls(seg_a, I->a) || addr_is_tls(seg_b, I->a))
        {
            // on the same thread
            if (seg_a->tls == seg_b->tls)
                mark_false_positive(I, &false_positive);
        }
        // most likely accessing the heap
        else
        {
            // TODO : removing false positive coming from KMP memory allocator
            // Any accesses on memory allocated as part of a callstack
            // including any 'SUPPRESS_FN' is assumed race-free
            taskgrind_alloc_record_t * record = taskgrind_alloc_record_get((void *) I->a);
            static const HChar * SUPPRESS_FN[] = {
                "__kmp_task_alloc",
            };

            // TODO : if llvm is not compiled with debug symbols, cannot detect
            if (!record)
                continue ;

            ExeContext * ec = record->ctx;
            DiEpoch ep = VG_(get_ExeContext_epoch)(ec);
            Int n_ips = VG_(get_ExeContext_n_ips)(ec);
            Addr * ips = VG_(get_ExeContext_ips)(ec);

            for (int j = 0 ; j < n_ips ; ++j)
            {
                const char * fn = NULL;
                VG_(get_fnname)(ep, ips[j], &fn);
                if (fn)
                {
                    for (Int k = 0 ; k < sizeof(SUPPRESS_FN) / sizeof(const HChar *) ; ++k)
                    {
                        if (VG_(strstr)(fn, SUPPRESS_FN[k]))
                        {
                            mark_false_positive(I, &false_positive);
                            break ;
                        }
                    } /* each suppress fn */
                } /* if fnname */
            } /* for each frame */
        }
    }

    // only false positive
    if (false_positive == r)
        return 0;

    // some actual errors
    report_err(seg_a, seg_b, r);
    return r - false_positive;
}

// Confront memory accesses of both segments and report errors
static UInt
compare_segments_independent(task_seg_t * seg_a, task_seg_t * seg_b)
{
    // segments are declared independent, check that
    //  a.W n (b.R u b.W) == {}
    //  b.W n (a.R u a.W) == {}
    // else, it means there exist memory addresses for which at least one
    // writes while the other accesses it

    //  a.W n (b.R u b.W) == {}
    spmt_t B_RW;
    SPMT_INITIALIZE(&B_RW);
    SPMT_UNION(&B_RW, &seg_b->loads, &seg_b->stores);
    int r = compare_segments_independent_accesses(seg_a, seg_b, &seg_a->stores, &B_RW);
    SPMT_RELEASE(&B_RW);

    //  b.W n (a.R u a.W) == {}
    if (r == 0)
    {
        spmt_t A_RW;
        SPMT_INITIALIZE(&A_RW);
        SPMT_UNION(&A_RW, &seg_a->loads, &seg_a->stores);
        compare_segments_independent_accesses(seg_a, seg_b, &seg_b->stores, &A_RW);
        SPMT_RELEASE(&A_RW);
    }

    return 0;
}

// segment reachability triangular matrix bits
static UChar * reachability = NULL;

# define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
# define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

static int
reachability_is_set(task_seg_t * s1, task_seg_t * s2)
{
    UInt i = MIN(s1->uid, s2->uid);
    UInt j = MAX(s1->uid, s2->uid);

    if (i == j)
        return 1;

    tl_assert(i < j);

    UInt n = SEGS.n;
    UInt k = (n*(n-1)/2) - (n-i)*((n-i)-1)/2 + j - i - 1;
    tl_assert(k >= 0 && k < n*(n-1)/2);

    UInt byte = k / (8*sizeof(UChar));
    UInt bit  = k % (8*sizeof(UChar));
    return reachability[byte] & (1 << bit);
}

static void
reachability_set(task_seg_t * s1, task_seg_t * s2)
{
    UInt i = MIN(s1->uid, s2->uid);
    UInt j = MAX(s1->uid, s2->uid);

    if (i == j)
        return ;

    tl_assert(i < j);

    UInt n = SEGS.n;
    UInt k = (n*(n-1)/2) - (n-i)*((n-i)-1)/2 + j - i - 1;
    tl_assert(k >= 0 && k < n*(n-1)/2);

    UInt byte = k / (8*sizeof(UChar));
    UInt bit  = k % (8*sizeof(UChar));
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

    // allocate array to report intersections
    n_intervals = 64;
    intervals = (interval_t *) VG_(malloc)("taskgrind_pass_e1", n_intervals * sizeof(interval_t));

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
    if (ERRORS)
        TASKGRIND_ERR("-> E1 reported %d possible determinacy races", ERRORS);
    else
        TASKGRIND_INFO("-> E1 found no determinacy race :-)");

    VG_(free)(intervals);
}
