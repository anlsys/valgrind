# Tracegrind × LTTng-UST example

This example shows how to enrich the trace of an **unmodified** lttng-instrumented
program with Tracegrind memory-access data. For every event the program emits,
an extra `vgust:mem_accesses` event is injected **just before it**, carrying the
compacted load/store intervals `[a ; b)` that Tracegrind recorded since the
previous event — for the calling thread.

The target program is **not recompiled** to get this. Everything lives in an
`LD_PRELOAD`ed shared object; Valgrind's redirection engine binds our wrapper
onto liblttng-ust's internal `lttng_event_reserve()`.

## How it works

1. **`hello`** is an ordinary lttng-ust program with one tracepoint
   (`hello_world:my_first_tracepoint`). It knows nothing about Valgrind.

2. **`vg_intercept.so`** (the wrapper) is `LD_PRELOAD`ed and:
   - Wraps `lttng_event_reserve()` — the first liblttng-ust call on the per-event
     emit path. It is a *local* symbol with several identical copies, so it is
     unreachable by `LD_PRELOAD` alone; Valgrind reads the full symbol table and
     its `I_WRAP_SONAME_FNNAME_ZU` redirection binds to it. The Z-encoded soname
     is `liblttngZhustZdsoZd1` (`-`→`Zh`, `.`→`Zd`).
   - On each interception: pauses recording (`TRACEGRIND_DISABLE`), drains the
     per-thread LOAD and STORE interval queues (`TRACEGRIND_EMPTY_QUEUE`), emits
     `vgust:mem_accesses` with the intervals, clears + re-enables, then calls the
     real `lttng_event_reserve()` so the program's own event follows.
   - A thread-local re-entrancy guard stops the wrapper's own emit (which itself
     reserves an event) from recursing forever.

3. **`vgust:mem_accesses`** carries the intervals as CTF sequences. lttng-ust
   fields are scalar-only (no array-of-struct), so each interval is split across
   two parallel arrays: interval `i` is `[load_start[i] ; load_end[i])` and
   likewise for stores.

## Files

| File | Role |
|------|------|
| `hello.c`, `hello-tp.h`, `hello-tp.c` | The unmodified example app + its tracepoint provider |
| `vg-tp.h`, `vg-tp.c` | The injected `vgust:mem_accesses` provider |
| `vg_intercept.c` | The Valgrind wrapper that drains tracegrind and emits the event |
| `run.sh` | Build + trace `hello` on its own (no Valgrind), sanity check |
| `run-valgrind.sh` | Build the wrapper, run `hello` under tracegrind, dump the merged trace |

## Requirements

- This tracegrind fork, **built** (`vg-in-place` + the `tracegrind` tool):
  ```sh
  cd <fork> && ./autogen.sh && CC=gcc ./configure && make
  make -C tracegrind          # if the top-level build stops on an unrelated test
  ```
  Note: configure must select **gcc** — building VEX with icx produces an empty
  `libvex_guest_offsets.h`.
- `lttng-tools`, `liblttng-ust-dev`, `babeltrace2`, and `pkg-config` able to find
  `lttng-ust` (on the reference system: `module load thapi`).

## Running

```sh
./run-valgrind.sh
```

Expected: pairs of events, `vgust:mem_accesses(seq=N)` immediately preceding the
Nth user event, e.g.

```
vgust:mem_accesses:            { seq = 3, n_loads = 96, load_start = [ ... ], load_end = [ ... ],
                                 n_stores = 31, store_start = [ ... ], store_end = [ ... ] }
hello_world:my_first_tracepoint: { my_integer_field = 2 }
```

`seq = 1` is large — it captures process/loader start-up accesses before the
first event. Pass `--start-disabled=yes` to tracegrind, or clear the queues once
at first interception, to suppress that.

Override the fork location with `VG_FORK=/path/to/valgrind ./run-valgrind.sh`.
