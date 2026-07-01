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
 * Carries the compacted load/store intervals [a ; b) that Tracegrind recorded
 * for the calling thread since the previous user event, as CTF sequences.
 *
 * Each interval is split across two parallel arrays (start[i], end[i]) because
 * CTF sequences are arrays of a scalar type. Loads and stores are independent.
 */
TRACEPOINT_EVENT(
	vgust,
	mem_accesses,
	TP_ARGS(
		unsigned long, seq_arg,
		int, on_valgrind_arg,
		unsigned long, ctx_arg,
		unsigned int,  n_loads_arg,
		unsigned long *, load_start_arg,
		unsigned long *, load_end_arg,
		unsigned int,  n_stores_arg,
		unsigned long *, store_start_arg,
		unsigned long *, store_end_arg
	),
	TP_FIELDS(
		/* monotonically increasing interception counter */
		ctf_integer(unsigned long, seq, seq_arg)
		/* RUNNING_ON_VALGRIND: 0 if native, else (version+1) */
		ctf_integer(int, on_valgrind, on_valgrind_arg)
		/* ring-buffer ctx address of the upcoming user event */
		ctf_integer_hex(unsigned long, user_ctx, ctx_arg)

		/* --- compacted LOAD intervals [load_start[i] ; load_end[i]) --- */
		ctf_integer(unsigned int, n_loads, n_loads_arg)
		ctf_sequence_hex(unsigned long, load_start, load_start_arg,
				 unsigned int, n_loads_arg)
		ctf_sequence_hex(unsigned long, load_end, load_end_arg,
				 unsigned int, n_loads_arg)

		/* --- compacted STORE intervals [store_start[i] ; store_end[i]) --- */
		ctf_integer(unsigned int, n_stores, n_stores_arg)
		ctf_sequence_hex(unsigned long, store_start, store_start_arg,
				 unsigned int, n_stores_arg)
		ctf_sequence_hex(unsigned long, store_end, store_end_arg,
				 unsigned int, n_stores_arg)
	)
)

#endif /* _VG_TP_H */

#include <lttng/tracepoint-event.h>
