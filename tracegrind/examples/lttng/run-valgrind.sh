#!/usr/bin/env bash
#
# Run an lttng-instrumented program under tracegrind so that, before each of the
# program's own lttng events, an extra vgust:mem_accesses event is emitted
# carrying the compacted load/store intervals tracegrind recorded for the
# preceding window. The target program (./hello) is NOT recompiled for this: the
# whole mechanism lives in an LD_PRELOAD'd wrapper that Valgrind's redirection
# engine binds onto liblttng-ust's internal lttng_event_reserve().
#
# Assumes the lttng/thapi module is loaded (provides lttng, babeltrace2,
# pkg-config for lttng-ust).
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

# --- locate the tracegrind fork ----------------------------------------------
# This example lives at tracegrind/examples/lttng/, so the fork root is three
# levels up. It provides the `tracegrind` tool and the matching tracegrind.h
# client-request numbers. VALGRIND is its vg-in-place launcher.
VG_FORK="${VG_FORK:-$(cd "$HERE/../../.." && pwd)}"
VALGRIND="$VG_FORK/vg-in-place"
if [[ ! -x "$VALGRIND" || ! -f "$VG_FORK/tracegrind/tracegrind.h" ]]; then
	echo "error: tracegrind fork not found/built at $VG_FORK" >&2
	echo "       (expected vg-in-place and tracegrind/tracegrind.h)" >&2
	exit 1
fi
# Header search paths for building the wrapper against the fork.
VG_INC="-I$VG_FORK/include -I$VG_FORK/tracegrind"
echo ">> Using tracegrind fork: $VALGRIND"

# --- make sure the target program exists -------------------------------------
if [[ ! -x ./hello ]]; then
	echo ">> ./hello missing; building via run.sh build steps..."
	UST_CFLAGS="$(pkg-config --cflags lttng-ust)"
	UST_LIBS="$(pkg-config --libs lttng-ust)"
	UST_RPATH=""
	for d in $(pkg-config --libs-only-L lttng-ust); do UST_RPATH+=" -Wl,-rpath,${d#-L}"; done
	cc -c -I. $UST_CFLAGS hello-tp.c -o hello-tp.o
	cc -c -I. $UST_CFLAGS hello.c    -o hello.o
	cc hello.o hello-tp.o $UST_LIBS $UST_RPATH -o hello
fi

# --- build the wrapper shared object -----------------------------------------
# The wrapper now ALSO emits its own lttng event (vgust:mem_accesses), so it
# must compile the provider (vg-tp.c) and link against lttng-ust itself.
echo ">> Building vg_intercept.so..."
UST_CFLAGS="$(pkg-config --cflags lttng-ust)"
UST_LIBS="$(pkg-config --libs lttng-ust)"
UST_RPATH=""
for d in $(pkg-config --libs-only-L lttng-ust); do UST_RPATH+=" -Wl,-rpath,${d#-L}"; done
cc -fPIC -shared -I. $VG_INC $UST_CFLAGS \
	vg_intercept.c vg-tp.c \
	$UST_LIBS $UST_RPATH -o vg_intercept.so
echo ">> Built ./vg_intercept.so"

# --- set up an lttng session so the tracepoint is actually enabled -----------
SESSION="hello-vg-$$"
TRACE_DIR="$(mktemp -d)"
echo ">> Trace output dir: $TRACE_DIR"
lttng create "$SESSION" --output="$TRACE_DIR"
cleanup() { lttng destroy "$SESSION" >/dev/null 2>&1 || true; }
trap cleanup EXIT
lttng enable-event --userspace hello_world:my_first_tracepoint
lttng enable-event --userspace vgust:mem_accesses
lttng start

# --- run under tracegrind with the wrapper preloaded -------------------------
# --tool=tracegrind: records every load/store into per-thread interval queues
#   that our wrapper drains at each event.
# LD_PRELOAD: makes our _vgw... wrapper symbol visible so the core binds it.
echo
echo ">> Running ./hello under tracegrind with the reserve-wrapper..."
LD_PRELOAD="$HERE/vg_intercept.so" \
	"$VALGRIND" --tool=tracegrind --quiet ./hello

lttng stop

echo
echo ">> Recorded events (proof the events still flowed through):"
babeltrace2 "$TRACE_DIR"
