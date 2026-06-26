/*
   ----------------------------------------------------------------

   Notice that the following BSD-style license applies to this one
   file (tracegrind.h) only.  The rest of Valgrind is licensed under the
   terms of the GNU General Public License, version 2, unless
   otherwise indicated.  See the COPYING file in the source
   distribution for details.

   ----------------------------------------------------------------

   This file is part of Tracegrind, a Valgrind tool for tracing the
   memory accesses performed by a program.

   Copyright (C) 2023-2024 Romain Pereira.

   ----------------------------------------------------------------
*/

#ifndef TRACEGRIND_API_H
# define TRACEGRIND_API_H

# include "valgrind.h"

/* Which per-thread access queue a client request operates on. */
typedef enum    tracegrind_access_kind_e
{
    TRACEGRIND_LOADS  = 0,
    TRACEGRIND_STORES = 1,
}               tracegrind_access_kind_t;

/* A traced memory interval [a ; b[ (b is exclusive). The tool fills an array of
 * these (provided by the client) when draining a queue. */
typedef struct  tracegrind_interval_s
{
    unsigned long a;    /* first address (inclusive)     */
    unsigned long b;    /* one-past-last address (excl.) */
}               tracegrind_interval_t;

/* Valgrind client requests handled by Tracegrind. */
typedef enum    tracegrind_client_request_e
{
    /* Return the number of compacted intervals currently queued for the
     * calling thread (arg[1] is a 'tracegrind_access_kind_t'). */
    VG_USERREQ__TRACEGRIND_QUEUE_SIZE = VG_USERREQ_TOOL_BASE('T', 'R'),

    /* Drain queued intervals for the calling thread into a client buffer.
     *  - arg[1] is a 'tracegrind_access_kind_t'
     *  - arg[2] is a 'tracegrind_interval_t *' buffer
     *  - arg[3] is the buffer capacity (number of intervals)
     * Returns the number of intervals written; drained intervals are removed
     * from the queue. */
    VG_USERREQ__TRACEGRIND_EMPTY_QUEUE,

    /* Discard every queued interval for the calling thread without reading
     * them (arg[1] is a 'tracegrind_access_kind_t'). */
    VG_USERREQ__TRACEGRIND_CLEAR_QUEUE,

    /* Resume / pause recording memory accesses for the calling thread. */
    VG_USERREQ__TRACEGRIND_ENABLE,
    VG_USERREQ__TRACEGRIND_DISABLE,
}               tracegrind_client_request_t;

/* Number of compacted intervals currently queued for '_qzz_kind' on the
 * calling thread. */
#define TRACEGRIND_QUEUE_SIZE(_qzz_kind)                                       \
    (unsigned long)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                          \
        VG_USERREQ__TRACEGRIND_QUEUE_SIZE, (_qzz_kind), 0, 0, 0, 0)

/* Drain up to '_qzz_cap' intervals of kind '_qzz_kind' from the calling
 * thread's queue into '_qzz_buf' (a 'tracegrind_interval_t' array). Returns
 * the number of intervals written; drained intervals are removed. */
#define TRACEGRIND_EMPTY_QUEUE(_qzz_kind, _qzz_buf, _qzz_cap)                  \
    (unsigned long)VALGRIND_DO_CLIENT_REQUEST_EXPR(0,                          \
        VG_USERREQ__TRACEGRIND_EMPTY_QUEUE, (_qzz_kind), (_qzz_buf),           \
        (_qzz_cap), 0, 0)

/* Discard all queued intervals of kind '_qzz_kind' for the calling thread. */
#define TRACEGRIND_CLEAR_QUEUE(_qzz_kind)                                      \
    VALGRIND_DO_CLIENT_REQUEST_STMT(                                           \
        VG_USERREQ__TRACEGRIND_CLEAR_QUEUE, (_qzz_kind), 0, 0, 0, 0)

/* Resume recording memory accesses for the calling thread. */
#define TRACEGRIND_ENABLE()                                                   \
    VALGRIND_DO_CLIENT_REQUEST_STMT(                                           \
        VG_USERREQ__TRACEGRIND_ENABLE, 0, 0, 0, 0, 0)

/* Pause recording memory accesses for the calling thread. */
#define TRACEGRIND_DISABLE()                                                  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(                                           \
        VG_USERREQ__TRACEGRIND_DISABLE, 0, 0, 0, 0, 0)

#endif /* TRACEGRIND_API_H */
