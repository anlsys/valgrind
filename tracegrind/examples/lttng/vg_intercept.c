/*
 * Valgrind function-wrapper for the lowest-level LTTng-UST emit call, which
 * itself emits a SECOND lttng event (vgust:mem_accesses) carrying the compacted
 * load/store intervals Tracegrind recorded, just BEFORE every real user event.
 *
 * The instrumented application is NOT modified or recompiled: it only has its
 * own lttng tracepoints. We run it under `--tool=tracegrind` with this wrapper
 * LD_PRELOAD'd; the wrapper owns the vgust provider and injects the extra event.
 *
 * Target: lttng_event_reserve(struct lttng_ust_ring_buffer_ctx *ctx) in
 * liblttng-ust.so.1 — the per-event ring-buffer reservation, the first lib call
 * on the emit path. It is a local symbol with 9 identical copies, unreachable
 * by LD_PRELOAD alone; Valgrind's redirection engine (which reads the full
 * symbol table) binds to it across all matching objects.
 *
 * Z-encoding (valgrind/pub_tool_redir.h):
 *   soname "liblttng-ust.so.1" -> liblttngZhustZdsoZd1   ('-'=Zh, '.'=Zd)
 *
 * RE-ENTRANCY: emitting our event itself calls lttng_event_reserve, which
 * Valgrind redirects back into this wrapper. A thread-local guard breaks that.
 */
#include "valgrind.h"
#include "tracegrind.h"
#include <stdio.h>

/* Our own provider. TRACEPOINT_DEFINE/CREATE_PROBES live in vg-tp.c. */
#include "vg-tp.h"

/* Per-thread guard: true while we are emitting our own event, so the nested
 * lttng_event_reserve it triggers is passed straight through. */
static __thread int in_wrapper;

/* Max compacted intervals we forward per kind per event. Program start-up
 * touches ~10k load intervals; size generously so we don't truncate. */
#define MAX_INTERVALS 65536

/* Drain one kind fully into parallel start[]/end[] arrays. Returns how many
 * intervals were written (capped at MAX_INTERVALS; any beyond that are drained
 * and dropped so the next window still starts clean). */
static unsigned int
drain_kind(tracegrind_access_kind_t kind,
	   unsigned long *start, unsigned long *end)
{
	static __thread tracegrind_interval_t buf[4096];
	unsigned int total = 0;
	unsigned long got;

	do {
		got = TRACEGRIND_EMPTY_QUEUE(kind, buf, 4096);
		for (unsigned long i = 0; i < got; i++) {
			if (total < MAX_INTERVALS) {
				start[total] = buf[i].a;
				end[total]   = buf[i].b;
				total++;
			}
		}
	} while (got == 4096);   /* buffer was full -> more may remain */

	return total;
}

int I_WRAP_SONAME_FNNAME_ZU(liblttngZhustZdsoZd1, lttng_event_reserve)(void *ctx);

int I_WRAP_SONAME_FNNAME_ZU(liblttngZhustZdsoZd1, lttng_event_reserve)(void *ctx)
{
	/* Big per-thread scratch arrays; static so they don't blow the stack. */
	static __thread unsigned long load_start[MAX_INTERVALS];
	static __thread unsigned long load_end[MAX_INTERVALS];
	static __thread unsigned long store_start[MAX_INTERVALS];
	static __thread unsigned long store_end[MAX_INTERVALS];
	static unsigned long seq;
	OrigFn fn;
	int result;
	unsigned int n_loads = 0, n_stores = 0;

	VALGRIND_GET_ORIG_FN(fn);

	/* If this reservation is the one our own event is making, don't recurse
	 * — just call the real function and return. */
	if (in_wrapper) {
		CALL_FN_W_W(result, fn, ctx);
		return result;
	}

	in_wrapper = 1;

	/* Pause recording so our own bookkeeping (drain, emit, reserve) does not
	 * pollute the NEXT window, then read out everything accessed since the
	 * previous user event. */
	TRACEGRIND_DISABLE();
	n_loads  = drain_kind(TRACEGRIND_LOADS,  load_start,  load_end);
	n_stores = drain_kind(TRACEGRIND_STORES, store_start, store_end);

	/* Emit the compacted intervals as arrays, just before the user event. */
	tracepoint(vgust, mem_accesses,
		   ++seq,
		   RUNNING_ON_VALGRIND,
		   (unsigned long)ctx,
		   n_loads,  load_start,  load_end,
		   n_stores, store_start, store_end);

	/* Drop anything our emit touched, then resume so the user event and the
	 * following user code are recorded into a fresh window. */
	TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_LOADS);
	TRACEGRIND_CLEAR_QUEUE(TRACEGRIND_STORES);
	TRACEGRIND_ENABLE();

	in_wrapper = 0;

	/* Now let the real reservation proceed -> the user event follows. */
	CALL_FN_W_W(result, fn, ctx);
	return result;
}
