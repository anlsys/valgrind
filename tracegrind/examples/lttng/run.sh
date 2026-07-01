#!/usr/bin/env bash
#
# Build, trace, and view an LTTng-UST "hello world".
#
# Requirements (Debian/Ubuntu names):
#   sudo apt install lttng-tools liblttng-ust-dev babeltrace2
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

SESSION="hello-lttng-$$"

# 1. Resolve lttng-ust build/link flags via pkg-config. The thapi/lttng
#    module already puts lttng-ust.pc on PKG_CONFIG_PATH.
if ! pkg-config --exists lttng-ust; then
	echo "error: pkg-config can't find lttng-ust (is the lttng/thapi module loaded?)" >&2
	exit 1
fi
UST_CFLAGS="$(pkg-config --cflags lttng-ust)"
UST_LIBS="$(pkg-config --libs lttng-ust)"
# Add an rpath for each -L so the binary finds liblttng-ust at runtime.
UST_RPATH=""
for d in $(pkg-config --libs-only-L lttng-ust); do
	UST_RPATH+=" -Wl,-rpath,${d#-L}"
done

# 2. Build. We compile the tracepoint provider and the app, linking against
#    lttng-ust. -I. lets the generated provider find ./hello-tp.h.
echo ">> Building..."
CC="${CC:-cc}"
$CC -c -I. $UST_CFLAGS hello-tp.c -o hello-tp.o
$CC -c -I. $UST_CFLAGS hello.c -o hello.o
$CC hello.o hello-tp.o -o hello $UST_LIBS $UST_RPATH
echo ">> Built ./hello"

# 2. Create a tracing session that stores traces in a temp dir.
TRACE_DIR="$(mktemp -d)"
echo ">> Trace output dir: $TRACE_DIR"

lttng create "$SESSION" --output="$TRACE_DIR"

# Make sure we always tear down the session, even on error.
cleanup() {
	lttng destroy "$SESSION" >/dev/null 2>&1 || true
}
trap cleanup EXIT

# 3. Enable our one userspace tracepoint and start tracing.
lttng enable-event --userspace hello_world:my_first_tracepoint
lttng start

# 4. Run the instrumented program.
echo ">> Running ./hello..."
./hello

# 5. Stop tracing and print the recorded events.
lttng stop

echo
echo ">> Recorded events:"
babeltrace2 "$TRACE_DIR"
