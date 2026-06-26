/* Self-checking Tracegrind test.
 *
 * Build:  make -C tracegrind/tests
 * Run:    ./vg-in-place --tool=tracegrind tracegrind/tests/basic.exe
 *
 * Strategy: drive recording with DISABLE / CLEAR / ENABLE so that only a
 * tightly controlled region is captured, then assert on the *global* objects
 * being accessed. Stack noise (loop counters, client-request arg buffers, ...)
 * lives at unrelated addresses, so it never overlaps the global ranges we
 * check.
 */

#undef NDEBUG               /* make sure assert() is always active */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "tracegrind.h"

#define N 1024

static int           g_array[N];
static volatile long g_scalar;

/* Index of an interval in buf[0..n) that fully covers [lo;hi), else -1. */
static long
find_covering(const tracegrind_interval_t * buf, unsigned long n,
              unsigned long lo, unsigned long hi)
{
    for (unsigned long i = 0; i < n; ++i)
        if (buf[i].a <= lo && hi <= buf[i].b)
            return (long) i;
    return -1;
}

/* Number of intervals in buf[0..n) that overlap [lo;hi). */
static unsigned long
count_overlapping(const tracegrind_interval_t * buf, unsigned long n,
                  unsigned long lo, unsigned long hi)
{
    unsigned long c = 0;
    for (unsigned long i = 0; i < n; ++i)
        if (buf[i].a < hi && lo < buf[i].b)
            ++c;
    return c;
}

/* Fully drain a queue. MUST be called while recording is disabled so that the
 * bookkeeping below does not perturb the result. Sets *out to a malloc'd
 * buffer (caller frees) and returns the number of intervals. */
static unsigned long
drain(tracegrind_access_kind_t kind, tracegrind_interval_t ** out)
{
    unsigned long n   = TRACEGRIND_QUEUE_SIZE(kind);
    tracegrind_interval_t * buf = malloc((n ? n : 1) * sizeof(*buf));
    unsigned long got = TRACEGRIND_EMPTY_QUEUE(kind, buf, n);

    assert(got == n);                          /* sized exactly    */
    assert(TRACEGRIND_QUEUE_SIZE(kind) == 0);  /* queue now empty  */

    *out = buf;
    return got;
}

/* Returns 1 iff Tracegrind is the active tool. Probes by recording a single
 * store and checking it shows up; under any other tool (or natively) the client
 * requests are no-ops that return 0, so the probe store is never seen. */
static int
tracegrind_is_active(void)
{
    static volatile int probe;
    unsigned long n;

    TRACEGRIND_DISABLE();
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_STORES);
    TRACEGRIND_ENABLE();
    probe = 1;
    TRACEGRIND_DISABLE();
    n = TRACEGRIND_QUEUE_SIZE(TRACEGRIND_STORES);
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_STORES);

    return n > 0;
}

int
main(void)
{
    const unsigned long abase = (unsigned long) &g_array[0];
    const unsigned long aend  = abase + sizeof(g_array);
    tracegrind_interval_t * buf;
    unsigned long n;
    long idx;
    long sum = 0;

    if (!RUNNING_ON_VALGRIND)
    {
        printf("SKIP: run with `valgrind --tool=tracegrind`\n");
        return 0;
    }

    if (!tracegrind_is_active())
    {
        printf("SKIP: this test requires --tool=tracegrind\n");
        return 0;
    }

    /* ----------------------------------------------------------------
     * TEST 1 - dense stores are compacted into ONE exact interval.
     * ---------------------------------------------------------------- */
    TRACEGRIND_DISABLE();
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_LOADS);
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_STORES);
    assert(TRACEGRIND_QUEUE_SIZE(TRACEGRIND_STORES) == 0);
    assert(TRACEGRIND_QUEUE_SIZE(TRACEGRIND_LOADS)  == 0);
    TRACEGRIND_ENABLE();

    for (int i = 0; i < N; ++i)
        g_array[i] = i;

    TRACEGRIND_DISABLE();

    n   = drain(TRACEGRIND_STORES, &buf);
    idx = find_covering(buf, n, abase, aend);
    assert(idx >= 0);                                    /* array was recorded  */
    assert(buf[idx].a == abase && buf[idx].b == aend);   /* exact bounds        */
    assert(count_overlapping(buf, n, abase, aend) == 1); /* compaction: 1 node  */
    free(buf);
    printf("TEST 1 ok: %d contiguous stores -> 1 interval [%#lx;%#lx)\n",
           N, abase, aend);

    /* ----------------------------------------------------------------
     * TEST 2 - dense loads are compacted into ONE exact interval.
     * ---------------------------------------------------------------- */
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_LOADS);
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_STORES);
    TRACEGRIND_ENABLE();

    for (int i = 0; i < N; ++i)
        sum += g_array[i];

    TRACEGRIND_DISABLE();

    assert(sum == (long) N * (N - 1) / 2);               /* loads really ran    */

    n   = drain(TRACEGRIND_LOADS, &buf);
    idx = find_covering(buf, n, abase, aend);
    assert(idx >= 0);
    assert(buf[idx].a == abase && buf[idx].b == aend);
    assert(count_overlapping(buf, n, abase, aend) == 1);
    free(buf);
    printf("TEST 2 ok: %d contiguous loads -> 1 interval [%#lx;%#lx)\n",
           N, abase, aend);

    /* ----------------------------------------------------------------
     * TEST 3 - a single scalar store has the exact reported byte range.
     * ---------------------------------------------------------------- */
    {
        const unsigned long sbase = (unsigned long) &g_scalar;
        const unsigned long send  = sbase + sizeof(g_scalar);

        TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_STORES);
        TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_LOADS);
        TRACEGRIND_ENABLE();

        g_scalar = 0x1234;

        TRACEGRIND_DISABLE();

        n   = drain(TRACEGRIND_STORES, &buf);
        idx = find_covering(buf, n, sbase, send);
        assert(idx >= 0);
        assert(buf[idx].a == sbase && buf[idx].b == send);
        assert(count_overlapping(buf, n, sbase, send) == 1);
        free(buf);
        printf("TEST 3 ok: scalar store -> interval [%#lx;%#lx) (%lu bytes)\n",
               sbase, send, send - sbase);
    }

    /* ----------------------------------------------------------------
     * TEST 4 - accesses performed while disabled are NOT recorded.
     * ---------------------------------------------------------------- */
    TRACEGRIND_DISABLE();
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_STORES);
    TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_LOADS);

    for (int i = 0; i < N; ++i)
        g_array[i] = -i;                 /* must not be recorded */
    assert(g_array[N - 1] == -(N - 1));  /* defeat dead-store elimination */

    assert(TRACEGRIND_QUEUE_SIZE(TRACEGRIND_STORES) == 0);
    assert(TRACEGRIND_QUEUE_SIZE(TRACEGRIND_LOADS)  == 0);
    printf("TEST 4 ok: accesses while disabled are not recorded\n");

    TRACEGRIND_ENABLE();

    printf("All Tracegrind assertions passed.\n");
    return 0;
}
