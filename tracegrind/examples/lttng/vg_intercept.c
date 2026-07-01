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
 *
 * ZERO-COPY DRAIN: Tracegrind's queue is drained straight into a per-thread
 * 'tracegrind_interval_t' buffer that is then handed to lttng AS-IS. Each
 * interval is two consecutive words (a, b), so a single sequence per kind
 * carries [a0, b0, a1, b1, ...] — no deinterleaving into start[]/end[].
 *
 * NO LOSS: a single record carries at most MAX_INTERVALS intervals per kind; if
 * a thread accumulated more since the previous user event, several records are
 * emitted back-to-back (same seq, increasing chunk) so nothing is dropped.
 */
#include "valgrind.h"
#include "tracegrind.h"
#include <stdlib.h>		/* malloc */

/* Our own provider. TRACEPOINT_DEFINE/CREATE_PROBES live in vg-tp.c. */
#include "vg-tp.h"

/* Per-thread guard: true while we are emitting our own event, so the nested
 * lttng_event_reserve it triggers is passed straight through. */
static __thread int in_wrapper;

/*
 * Maximum number of compacted intervals carried by a SINGLE vgust:mem_accesses
 * record, per kind. Thread-private (so it can be tuned per thread); the default
 * 16384 intervals means a 256 KiB drain buffer per kind
 * (sizeof(tracegrind_interval_t) == 2 words == 16 bytes, x 16384).
 */
static __thread unsigned long MAX_INTERVALS = 16384;

/*
 * Per-thread drain buffers, lazily allocated to hold MAX_INTERVALS intervals.
 * Tracegrind drains straight into these and they are forwarded to lttng as-is,
 * so there is a single copy out of the queue (no intermediate start[]/end[]).
 * They live for the thread's lifetime (intentionally not freed).
 */
static __thread tracegrind_interval_t *load_buf;
static __thread tracegrind_interval_t *store_buf;

int I_WRAP_SONAME_FNNAME_ZU(liblttngZhustZdsoZd1, lttng_event_reserve)(void *ctx);

int I_WRAP_SONAME_FNNAME_ZU(liblttngZhustZdsoZd1, lttng_event_reserve)(void *ctx)
{
	static __thread unsigned long seq;	/* per-thread interception counter */
	OrigFn fn;
	int result;

	VALGRIND_GET_ORIG_FN(fn);

	/* If this reservation is the one our own event is making, don't recurse
	 * — just call the real function and return. */
	if (in_wrapper) {
		CALL_FN_W_W(result, fn, ctx);
		return result;
	}

	in_wrapper = 1;

	/* Pause recording so our own bookkeeping (drain, emit, reserve) does not
	 * pollute the NEXT window. */
	TRACEGRIND_DISABLE();

	/* Lazily size the per-thread drain buffers to MAX_INTERVALS. */
	if (load_buf == NULL)
		load_buf = malloc(MAX_INTERVALS * sizeof(*load_buf));
	if (store_buf == NULL)
		store_buf = malloc(MAX_INTERVALS * sizeof(*store_buf));

	if (load_buf != NULL && store_buf != NULL) {
		unsigned long this_seq = ++seq;
		unsigned int  chunk = 0;
		unsigned long nl, ns;

		/* Drain everything, emitting as many records as needed. Each drain
		 * removes at most MAX_INTERVALS intervals of a kind; when a kind
		 * fills the buffer, more may remain, so we loop -> no loss. The
		 * first iteration always emits (even when empty) so that every user
		 * event still gets exactly one leading 'seq'. */
		do {
			nl = TRACEGRIND_EMPTY_QUEUE(TRACEGRIND_LOADS,  load_buf,  MAX_INTERVALS);
			ns = TRACEGRIND_EMPTY_QUEUE(TRACEGRIND_STORES, store_buf, MAX_INTERVALS);

			/* Both queues already fully flushed by a previous chunk: stop
			 * without emitting a trailing empty record. (chunk 0 always
			 * emits, so every user event still gets one leading seq.) */
			if (chunk > 0 && nl == 0 && ns == 0)
				break;

			/* Flush the tracegrind_interval_t buffers as-is: interval i is
			 * [ loads[2i] ; loads[2i+1] ), likewise for stores. */
			tracepoint(vgust, mem_accesses,
				   this_seq,
				   RUNNING_ON_VALGRIND,
				   (unsigned long)ctx,
				   chunk,
				   (unsigned int)nl, (unsigned long *)load_buf,
				   (unsigned int)ns, (unsigned long *)store_buf);
			chunk++;
		} while (nl == MAX_INTERVALS || ns == MAX_INTERVALS);
	}

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
