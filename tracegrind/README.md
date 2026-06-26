# Tracegrind

Tracegrind is a simplified spin-off of Taskgrind. It instruments every
load/store performed by the program and enqueues them, **per guest thread**,
into a *Sparse Memory Tree* (SPMT) — a self-balancing interval tree that
automatically compacts dense/contiguous accesses (e.g. `x[0], x[1], ... x[n]`
collapses into a single interval `[&x[0] ; &x[n+1])`).

Unlike Taskgrind, Tracegrind performs no task/dependency analysis. Instead, the
instrumented program drains the queues on demand through client requests and
receives the compacted intervals back into its own buffers.

## Building

Adding a new tool changes the autotools inputs, so the build files must be
regenerated once:

```
cd valgrind
./autogen.sh
./configure                 # add your usual flags
make
make -C tracegrind/tests
```

## Running

```
./vg-in-place --tool=tracegrind tracegrind/tests/basic.exe
```

Options:

- `--start-disabled=no|yes` — start with recording paused (default `no`). Useful
  to skip the noisy program start-up / dynamic loader accesses.

## Client API (`tracegrind.h`)

Include `<valgrind/tracegrind.h>` (installed) or `tracegrind.h` (in-tree).

| Macro | Meaning |
|-------|---------|
| `TRACEGRIND_QUEUE_SIZE(kind)` | number of compacted intervals queued for the calling thread |
| `TRACEGRIND_EMPTY_QUEUE(kind, buf, cap)` | drain up to `cap` intervals into `buf`; returns the count written and removes them |
| `TRACEGRIND_CLEAR_QUEUE(kind)` | discard all queued intervals without reading them |
| `TRACEGRIND_DISABLE()` / `TRACEGRIND_ENABLE()` | pause / resume recording for the calling thread |

where `kind` is `TRACEGRIND_LOADS` or `TRACEGRIND_STORES`, and `buf` is an array
of `tracegrind_interval_t { unsigned long a, b; }` describing `[a ; b)`.

### Recommended pattern

Recording is always on, so the bookkeeping code around a drain would otherwise
pollute the queue. Pause recording while draining:

```c
TRACEGRIND_DISABLE();
unsigned long n = TRACEGRIND_QUEUE_SIZE(TRACEGRIND_STORES);
tracegrind_interval_t *buf = malloc(n * sizeof *buf);
unsigned long got = TRACEGRIND_EMPTY_QUEUE(TRACEGRIND_STORES, buf, n);
/* ... process buf[0..got) ... */
free(buf);
TRACEGRIND_ENABLE();
```

If the provided buffer is too small, `TRACEGRIND_EMPTY_QUEUE` writes what fits,
keeps the rest queued, and returns the number written — so you may also loop
until it returns 0.

## Notes

- Recording is per *guest thread* (Valgrind serializes thread execution); each
  thread has independent load and store queues.
- Drained intervals are disjoint and non-adjacent but are **not** returned in
  address order (the queue is a tree, drained in tree order).
- Atomic accesses (CAS, LL/SC) are recorded as ordinary loads/stores. A CAS is
  recorded as both a load and a store of the location.
- Accesses generated *between* client requests (e.g. the `malloc` for the drain
  buffer) are recorded too — hence `TRACEGRIND_DISABLE/ENABLE` around drains and
  `TRACEGRIND_CLEAR_QUEUE` at the start of a region of interest.
