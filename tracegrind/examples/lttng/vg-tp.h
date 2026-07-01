#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER vgust

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./vg-tp.h"

#if !defined(_VG_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _VG_TP_H

#include <lttng/tracepoint.h>

/*
 * vgust:mem_accesses
 *
 * Emitted by the Valgrind reserve-wrapper just BEFORE each real user event.
 * Carries the compacted memory intervals [a ; b) that Tracegrind recorded for
 * the calling thread since the previous user event.
 *
 * The intervals are the raw 'tracegrind_interval_t' buffer flushed AS-IS: each
 * interval occupies two consecutive words, so the 'loads' and 'stores'
 * sequences are laid out as
 *
 *     [ a0, b0, a1, b1, ..., a(n-1), b(n-1) ]
 *
 * where interval i is [ a_i ; b_i ) (a = first byte, b = one-past-last byte).
 * 'n_loads' / 'n_stores' are the interval COUNTS, so each sequence carries
 * 2*count words. There are only two sequences (no separate start[]/end[]
 * arrays), which lets the wrapper hand Tracegrind's buffer to lttng directly.
 *
 * If a thread accumulated more than the wrapper's per-record cap, several
 * records are emitted with the same 'seq' and increasing 'chunk', so no
 * interval is ever dropped.
 */
TRACEPOINT_EVENT(
	vgust,
	mem_accesses,
	TP_ARGS(
		unsigned long,   seq_arg,
		int,             on_valgrind_arg,
		unsigned long,   ctx_arg,
		unsigned int,    chunk_arg,
		unsigned int,    n_loads_arg,
		unsigned long *, loads_arg,
		unsigned int,    n_stores_arg,
		unsigned long *, stores_arg
	),
	TP_FIELDS(
		/* monotonically increasing interception counter */
		ctf_integer(unsigned long, seq, seq_arg)
		/* RUNNING_ON_VALGRIND: 0 if native, else (version+1) */
		ctf_integer(int, on_valgrind, on_valgrind_arg)
		/* ring-buffer ctx address of the upcoming user event */
		ctf_integer_hex(unsigned long, user_ctx, ctx_arg)
		/* record index within this interception (0-based) */
		ctf_integer(unsigned int, chunk, chunk_arg)

		/* --- compacted LOAD intervals, flushed as-is (2 words each) --- */
		ctf_integer(unsigned int, n_loads, n_loads_arg)
		ctf_sequence_hex(unsigned long, loads, loads_arg,
				 unsigned int, n_loads_arg * 2)

		/* --- compacted STORE intervals, flushed as-is (2 words each) --- */
		ctf_integer(unsigned int, n_stores, n_stores_arg)
		ctf_sequence_hex(unsigned long, stores, stores_arg,
				 unsigned int, n_stores_arg * 2)
	)
)

#endif /* _VG_TP_H */

#include <lttng/tracepoint-event.h>
