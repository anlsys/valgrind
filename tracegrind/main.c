/*--------------------------------------------------------------------*/
/*--- Tracegrind: trace memory accesses into per-thread interval    ---*/
/*--- queues that the client can drain on demand.          main.c  ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Tracegrind, a simplified spin-off of Taskgrind.

   Copyright (C) 2023-2024 Romain Pereira
      romain.pereira@outlook.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_options.h"
#include "pub_tool_libcassert.h"     /* tl_assert, VG_(tool_panic) */
#include "pub_tool_libcprint.h"      /* VG_(printf) */
#include "pub_tool_mallocfree.h"     /* VG_(calloc), VG_(realloc), VG_(free) */
#include "pub_tool_machine.h"        /* VG_(fnptr_to_fnentry) */
#include "pub_tool_threadstate.h"    /* VG_(get_running_tid), VG_N_THREADS */

#include "print.h"
#include "tracegrind.h"
#include "tracegrind_spmt.h"

///////////////////////////////////////////////////////////////////////////////
//  Command line options
///////////////////////////////////////////////////////////////////////////////

// When set, recording starts paused so that the (noisy) program start-up is
// not captured before the client explicitly enables it. Disabled by default
// (recording is on from the start).
static Bool clo_start_disabled = False;

static Bool
tg_process_cmd_line_option(const HChar * arg)
{
    if VG_BOOL_CLO(arg, "--start-disabled", clo_start_disabled) {}
    else
        return False;
    return True;
}

static void
tg_print_usage(void)
{
    VG_(printf)(
"    --start-disabled=no|yes   start with recording paused [no]\n"
    );
}

static void
tg_print_debug_usage(void)
{
    VG_(printf)(
"    (none)\n"
    );
}

///////////////////////////////////////////////////////////////////////////////
//  Per-thread state
///////////////////////////////////////////////////////////////////////////////

// Per guest-thread recording state. Valgrind serializes guest threads, so a
// plain array indexed by ThreadId behaves like thread-local storage.
typedef struct  tg_thread_s
{
    // queued load intervals
    spmt_t loads;

    // queued store intervals
    spmt_t stores;

    // when True, accesses performed by this thread are not recorded
    Bool   disabled;
}               tg_thread_t;

// Allocated in tg_post_clo_init once VG_N_THREADS is known. Zero-initialized:
// an all-zero spmt_t is a valid empty tree (root == SPMT_NULL == 0), and
// 'disabled == False' means recording is enabled by default.
static tg_thread_t * THREADS = NULL;

static inline tg_thread_t *
tg_thread(ThreadId tid)
{
    tl_assert(THREADS != NULL);
    tl_assert(tid < VG_N_THREADS);
    return THREADS + tid;
}

static inline spmt_t *
tg_queue(tg_thread_t * t, tracegrind_access_kind_t kind)
{
    return (kind == TRACEGRIND_STORES) ? &t->stores : &t->loads;
}

///////////////////////////////////////////////////////////////////////////////
//  Recording (called from instrumented code through dirty helpers)
///////////////////////////////////////////////////////////////////////////////

static VG_REGPARM(2) void
tg_trace_load(Addr addr, SizeT size)
{
    if (THREADS == NULL)
        return ;
    tg_thread_t * t = THREADS + VG_(get_running_tid)();
    if (!t->disabled)
        SPMT_FILL(&t->loads, (SPMT_PTR_T) addr, (SPMT_PTR_T) (addr + size));
}

static VG_REGPARM(2) void
tg_trace_store(Addr addr, SizeT size)
{
    if (THREADS == NULL)
        return ;
    tg_thread_t * t = THREADS + VG_(get_running_tid)();
    if (!t->disabled)
        SPMT_FILL(&t->stores, (SPMT_PTR_T) addr, (SPMT_PTR_T) (addr + size));
}

///////////////////////////////////////////////////////////////////////////////
//  Draining queues (called from client request handlers)
///////////////////////////////////////////////////////////////////////////////

// Accumulator used to snapshot all intervals of a queue.
typedef struct  tg_collect_s
{
    tracegrind_interval_t * arr;
    UWord n;
    UWord cap;
}               tg_collect_t;

static int
tg_collect_cb(spmt_t * spmt, spmt_node_t * node, void * obj)
{
    (void) spmt;
    tg_collect_t * c = (tg_collect_t *) obj;

    if (c->n == c->cap)
    {
        c->cap = c->cap ? c->cap * 2 : 64;
        c->arr = (tracegrind_interval_t *) VG_(realloc)(
                "tracegrind.collect", c->arr,
                c->cap * sizeof(tracegrind_interval_t));
    }

    c->arr[c->n].a = (unsigned long) node->I.a;
    c->arr[c->n].b = (unsigned long) node->I.b;
    ++c->n;

    return 0;
}

static int
tg_count_cb(spmt_t * spmt, spmt_node_t * node, void * obj)
{
    (void) spmt;
    (void) node;
    ++*(UWord *) obj;
    return 0;
}

static UWord
tg_queue_size(ThreadId tid, tracegrind_access_kind_t kind)
{
    UWord n = 0;
    SPMT_FOREACH(tg_queue(tg_thread(tid), kind), tg_count_cb, &n);
    return n;
}

// Drain up to 'cap' intervals of 'kind' for thread 'tid' into the client
// buffer 'buf'. Returns the number of intervals written. Drained intervals are
// removed; any that did not fit are kept queued.
static UWord
tg_empty_queue(ThreadId tid, tracegrind_access_kind_t kind,
               tracegrind_interval_t * buf, UWord cap)
{
    spmt_t * spmt = tg_queue(tg_thread(tid), kind);

    // snapshot every interval, then empty the tree
    tg_collect_t c = { NULL, 0, 0 };
    SPMT_FOREACH(spmt, tg_collect_cb, &c);
    SPMT_RELEASE(spmt);

    // copy what fits into the client buffer
    UWord ncopy = (c.n < cap) ? c.n : cap;
    tl_assert(buf != NULL || ncopy == 0);
    for (UWord i = 0 ; i < ncopy ; ++i)
        buf[i] = c.arr[i];

    // re-queue the overflow so nothing is lost
    for (UWord i = ncopy ; i < c.n ; ++i)
        SPMT_FILL(spmt, (SPMT_PTR_T) c.arr[i].a, (SPMT_PTR_T) c.arr[i].b);

    if (c.arr)
        VG_(free)(c.arr);

    return ncopy;
}

static void
tg_clear_queue(ThreadId tid, tracegrind_access_kind_t kind)
{
    spmt_t * spmt = tg_queue(tg_thread(tid), kind);
    SPMT_RELEASE(spmt);
}

///////////////////////////////////////////////////////////////////////////////
//  Client requests
///////////////////////////////////////////////////////////////////////////////

static Bool
tg_handle_client_request(ThreadId tid, UWord * arg, UWord * ret)
{
    if (!VG_IS_TOOL_USERREQ('T', 'R', arg[0]))
        return False;

    switch (arg[0])
    {
        case VG_USERREQ__TRACEGRIND_QUEUE_SIZE:
        {
            *ret = tg_queue_size(tid, (tracegrind_access_kind_t) arg[1]);
            return True;
        }

        case VG_USERREQ__TRACEGRIND_EMPTY_QUEUE:
        {
            *ret = tg_empty_queue(
                    tid,
                    (tracegrind_access_kind_t) arg[1],
                    (tracegrind_interval_t *) arg[2],
                    (UWord) arg[3]);
            return True;
        }

        case VG_USERREQ__TRACEGRIND_CLEAR_QUEUE:
        {
            tg_clear_queue(tid, (tracegrind_access_kind_t) arg[1]);
            *ret = 0;
            return True;
        }

        case VG_USERREQ__TRACEGRIND_ENABLE:
        {
            tg_thread(tid)->disabled = False;
            *ret = 0;
            return True;
        }

        case VG_USERREQ__TRACEGRIND_DISABLE:
        {
            tg_thread(tid)->disabled = True;
            *ret = 0;
            return True;
        }

        default:
        {
            TRACEGRIND_WARN("Unknown client request code %llx", (ULong) arg[0]);
            return False;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Instrumentation
///////////////////////////////////////////////////////////////////////////////

static void
tg_instrument_access(IRSB * sb, IRExpr * addr, Int size, Bool isStore, IRExpr * guard)
{
    void * fn          = isStore ? (void *) tg_trace_store : (void *) tg_trace_load;
    const HChar * name = isStore ? "tg_trace_store"        : "tg_trace_load";

    IRExpr ** argv = mkIRExprVec_2(addr, mkIRExpr_HWord(size));
    IRDirty * di   = unsafeIRDirty_0_N(2, name, VG_(fnptr_to_fnentry)(fn), argv);

    // only fire the helper when the (optional) guard holds
    if (guard)
        di->guard = guard;

    addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static IRSB *
tg_instrument(
    VgCallbackClosure * closure,
    IRSB * sb_in,
    const VexGuestLayout * layout,
    const VexGuestExtents * vge,
    const VexArchInfo * archinfo_host,
    IRType gWordTy,
    IRType hWordTy
) {
    Int i;
    IRSB * sb_out;
    IRTypeEnv * tyenv = sb_in->tyenv;

    if (gWordTy != hWordTy)
        VG_(tool_panic)("host/guest word size mismatch");

    // deep copy code until the first marker
    sb_out = deepCopyIRSBExceptStmts(sb_in);
    i = 0;
    while (i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark)
    {
        addStmtToIRSB(sb_out, sb_in->stmts[i]);
        ++i;
    }

    // instrument the remaining statements
    for ( ; i < sb_in->stmts_used ; ++i)
    {
        IRStmt * st = sb_in->stmts[i];

        switch (st->tag)
        {
            case Ist_WrTmp:
            {
                IRExpr * data = st->Ist.WrTmp.data;
                if (data->tag == Iex_Load)
                    tg_instrument_access(
                            sb_out,
                            data->Iex.Load.addr,
                            sizeofIRType(data->Iex.Load.ty),
                            False, NULL);
                break ;
            }

            case Ist_Store:
            {
                IRExpr * data = st->Ist.Store.data;
                tg_instrument_access(
                        sb_out,
                        st->Ist.Store.addr,
                        sizeofIRType(typeOfIRExpr(tyenv, data)),
                        True, NULL);
                break ;
            }

            case Ist_StoreG:
            {
                IRStoreG * sg = st->Ist.StoreG.details;
                tg_instrument_access(
                        sb_out,
                        sg->addr,
                        sizeofIRType(typeOfIRExpr(tyenv, sg->data)),
                        True, sg->guard);
                break ;
            }

            case Ist_LoadG:
            {
                IRLoadG * lg       = st->Ist.LoadG.details;
                IRType    type     = Ity_INVALID;   // loaded type
                IRType    typeWide = Ity_INVALID;   // after implicit widening
                typeOfIRLoadGOp(lg->cvt, &typeWide, &type);
                tg_instrument_access(
                        sb_out,
                        lg->addr,
                        sizeofIRType(type),
                        False, lg->guard);
                break ;
            }

            case Ist_Dirty:
            {
                IRDirty * d = st->Ist.Dirty.details;
                if (d->mFx != Ifx_None)
                {
                    tl_assert(d->mAddr != NULL);
                    tl_assert(d->mSize != 0);
                    if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                        tg_instrument_access(sb_out, d->mAddr, d->mSize, False, NULL);
                    if (d->mFx == Ifx_Write || d->mFx == Ifx_Modify)
                        tg_instrument_access(sb_out, d->mAddr, d->mSize, True, NULL);
                }
                else
                {
                    tl_assert(d->mAddr == NULL);
                    tl_assert(d->mSize == 0);
                }
                break ;
            }

            case Ist_CAS:
            {
                // treat a CAS as a read and a write of the location
                IRCAS * cas = st->Ist.CAS.details;
                Int     dataSize;

                tl_assert(cas->addr != NULL);
                tl_assert(cas->dataLo != NULL);

                dataSize = sizeofIRType(typeOfIRExpr(tyenv, cas->dataLo));
                if (cas->dataHi != NULL)
                    dataSize *= 2;  // doubleword-CAS

                tg_instrument_access(sb_out, cas->addr, dataSize, False, NULL);
                tg_instrument_access(sb_out, cas->addr, dataSize, True,  NULL);
                break ;
            }

            case Ist_LLSC:
            {
                if (st->Ist.LLSC.storedata == NULL)
                {
                    // Load-Linked
                    IRType dataTy = typeOfIRTemp(tyenv, st->Ist.LLSC.result);
                    tg_instrument_access(
                            sb_out, st->Ist.LLSC.addr,
                            sizeofIRType(dataTy), False, NULL);
                }
                else
                {
                    // Store-Conditional
                    IRType dataTy = typeOfIRExpr(tyenv, st->Ist.LLSC.storedata);
                    tg_instrument_access(
                            sb_out, st->Ist.LLSC.addr,
                            sizeofIRType(dataTy), True, NULL);
                }
                break ;
            }

            default:
            {
                break ;
            }
        }

        addStmtToIRSB(sb_out, st);
    }

    return sb_out;
}

///////////////////////////////////////////////////////////////////////////////
//  Setup / teardown
///////////////////////////////////////////////////////////////////////////////

static void
tg_post_clo_init(void)
{
    // VG_N_THREADS is only known after command line processing
    THREADS = (tg_thread_t *) VG_(calloc)(
            "tracegrind.threads", VG_N_THREADS, sizeof(tg_thread_t));
    tl_assert(THREADS);

    if (clo_start_disabled)
        for (UInt i = 0 ; i < VG_N_THREADS ; ++i)
            THREADS[i].disabled = True;

    TRACEGRIND_INFO("Recording memory accesses %s",
            clo_start_disabled ? "(initially paused)" : "(initially enabled)");
}

static void
tg_fini(Int exitcode)
{
    if (THREADS == NULL)
        return ;

    for (UInt i = 0 ; i < VG_N_THREADS ; ++i)
    {
        SPMT_RELEASE(&THREADS[i].loads);
        SPMT_RELEASE(&THREADS[i].stores);
    }

    VG_(free)(THREADS);
    THREADS = NULL;
}

static void
tg_pre_clo_init(void)
{
    VG_(details_name)            ("Tracegrind");
    VG_(details_version)         (NULL);
    VG_(details_description)     ("a memory access tracer with on-demand interval queues");
    VG_(details_copyright_author)(
            "Copyright (C) 2023-2026, and GNU GPL'd, by Romain Pereira et al.");
    VG_(details_bug_reports_to)  ("rpereira@anl.gov");

    VG_(details_avg_translation_sizeB) ( 275 );

    VG_(basic_tool_funcs)(tg_post_clo_init, tg_instrument, tg_fini);

    VG_(needs_command_line_options)(
            tg_process_cmd_line_option,
            tg_print_usage,
            tg_print_debug_usage);

    VG_(needs_client_requests)(tg_handle_client_request);

    VG_(needs_core_errors)(True);
}

VG_DETERMINE_INTERFACE_VERSION(tg_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
