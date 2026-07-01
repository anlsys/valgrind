/*
 * This file instantiates the tracepoint provider code. It must be a
 * dedicated source file (no other includes) that defines
 * TRACEPOINT_CREATE_PROBES before including the provider header.
 */
#define TRACEPOINT_CREATE_PROBES
#define TRACEPOINT_DEFINE
#include "hello-tp.h"
