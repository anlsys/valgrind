/*-----------------------------------------------*/
/*--- Taskgrind: a debugger for dependent tasks  */
/*-----------------------------------------------*/

/*
   This file is part of Taskgrind

   Copyright (C) 2023 Romain PEREIRA
      romain.pereira@outlook.com

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PAENVICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.

   The GNU General Public License is contained in the file COPYING.
*/

#include "dot.h"
#include "env.h"
#include "error.h"
#include "malloc.h"
#include "print.h"
#include "task.h"
#include "taskgrind.h"
#include "taskgrind_clo.h"

#include "pub_tool_basics.h"
#include "pub_tool_guest.h"         /* thread state */
#include "pub_tool_tooliface.h"
#include "pub_tool_options.h"
#include "pub_tool_libcbase.h"      /* strstr */
#include "pub_tool_libcassert.h"    /* tool_panic, lt_assert */
#include "pub_tool_machine.h"       /* fnptr_to_fnentry */
#include "pub_tool_mallocfree.h"    /* malloc, free */
#include "pub_tool_libcproc.h"      /* gettimeofday */


///////////////////////////////////////////////////////////////////////////////
//  Track tasks and their dependencies
///////////////////////////////////////////////////////////////////////////////

static Bool
taskgrind_handle_client_request(ThreadId tid, UWord * arg, UWord * ret)
{
    switch (arg[0])
    {
        case VG_USERREQ__TASKGRIND_FORK_POINT_EVENT:
        {
            UWord fork_id = (UWord) arg[1];
            task_fork(fork_id);
            break ;
        }

        case VG_USERREQ__TASKGRIND_JOIN_POINT_EVENT:
        {
            UWord fork_id = (UWord) arg[1];
            task_join(fork_id);
            break ;
        }

        case VG_USERREQ__TASKGRIND_IMPLICIT_TASK_BEGIN_EVENT:
        {
            UWord fork_id = (UWord) arg[1];
            UWord task_id = (UWord) arg[2];
            task_implicit_begin(fork_id, task_id);
            break ;
        }

        case VG_USERREQ__TASKGRIND_IMPLICIT_TASK_END_EVENT:
        {
            UWord task_id = (UWord) arg[1];
            task_implicit_end(task_id);
            break ;
        }

        case VG_USERREQ__TASKGRIND_CREATE_EVENT:
        {
            task_type_t type;
            taskgrind_task_type_t ttype = (taskgrind_task_type_t) arg[2];
            UWord undeferred = arg[3];

            switch (ttype)
            {
                case (TASKGRIND_TASK_TYPE_EXPLICIT):
                {
                    type = TASK_TYPE_EXPLICIT;
                    break ;
                }

                case (TASKGRIND_TASK_TYPE_IMPLICIT):
                {
                    type = TASK_TYPE_IMPLICIT_UNKNOWN;
                    break ;
                }

                default:
                {
                    type = TASK_TYPE_UNKNOWN;
                    break ;
                }
            }

            task_create(arg[1], type, undeferred);
            return True;
        }

        case VG_USERREQ__TASKGRIND_SCHEDULE_EVENT:
        {
            task_schedule(arg[1]);
            return True;
        }

        case VG_USERREQ__TASKGRIND_DEPEND_EVENT:
        {
            task_depend(arg[1], arg[2], arg[3]);
            return True;
        }

        case VG_USERREQ__TASKGRIND_SYNC_EVENT:
        {
            task_sync(arg[1]);
            return True;
        }

        case VG_USERREQ__TASKGRIND_DETACH_FULFILL_EVENT:
        {
            task_detach_fulfill(arg[1], arg[2]);
            return True;
        }

        default:
        {
            // TODO : what is the client request '0x4d430101' that appeared
            // between June 2023 and June 2024 ?

            if (arg[0] == 0x4d430101)
            {
                static int done = 0;
                if (!done)
                    done = 1;
                else
                    return False;
            }

            TASKGRIND_WARN("Unknown client request code %llx", (ULong)arg[0]);

            return False;
        }
    }
    return False;
}

///////////////////////////////////////////////////////////////////////////////
//  Instrumentation
///////////////////////////////////////////////////////////////////////////////

// Only accesses in this instrumentlist are instrumented if passing the '--instrumentlist' CLA
static const HChar * WHITELIST_FN[] = {
    "omp_task_entry",
};

// Accesses in these functions are ignored
static const HChar * BLACKLIST_FN[] = {
    "on_ompt",          // ignore ompt plugin code
    "__kmp",            // ignore llvm runtime code
    "outlined_debug",   // ignore llvm debug micro tasks
};

static void
taskgrind_instrument_mem_access(
    IRSB * sb,
    IRExpr * addr,
    Int size,
    task_mem_access_type_t access_type
) {
    IRExpr ** argv;
    IRDirty * di;
    void * fn;
    const char * fn_name;

    switch (access_type)
    {
        case (TASKGRIND_TASK_MEM_LOAD):
        {
            fn      = task_mem_load;
            fn_name = "task_mem_load";
            break ;
        }

        case (TASKGRIND_TASK_MEM_STORE):
        {
            fn      = task_mem_store;
            fn_name = "task_mem_store";
            break ;
        }

        case (TASKGRIND_TASK_MEM_LOAD_ATOMIC):
        {
            fn      = task_mem_load_atomic;
            fn_name = "task_mem_load_atomic";
            break ;
        }

        case (TASKGRIND_TASK_MEM_STORE_ATOMIC):
        {
            fn      = task_mem_store_atomic;
            fn_name = "task_mem_store_atomic";
            break ;
        }

        default:
        {
            tl_assert("Unknown memory access type" && 0);
            break ;
        }
    }

    argv =  mkIRExprVec_2(addr, mkIRExpr_HWord(size));
    di   =  unsafeIRDirty_0_N(2, fn_name, VG_(fnptr_to_fnentry)(fn), argv);

    addStmtToIRSB(sb, IRStmt_Dirty(di));
}

static IRSB *
taskgrind_instrument(
    VgCallbackClosure * closure,
    IRSB * sb_in,
    const VexGuestLayout * layout,
    const VexGuestExtents * vge,
    const VexArchInfo * archinfo_host,
    IRType gWordTy,
    IRType hWordTy
) {
    // ignore initial accesses from the root segment
    if (SEGS.n <= 1)
        return sb_in;

    if (gWordTy != hWordTy)
        VG_(tool_panic)("host/guest word size mismatch");

    IRStmt * st = sb_in->stmts[0];
    Addr addr = st->Ist.IMark.addr + st->Ist.IMark.delta;
    DiEpoch ep = VG_(current_DiEpoch)();
    const HChar * fn;
    VG_(get_fnname)(ep, addr, &fn);

    if (fn)
    {
        if (CLOS.instrumentlist)
        {
            int instrument = 0;
            for (int i = 0 ; i < sizeof(WHITELIST_FN) / sizeof(const HChar *) ; ++i)
            {
                if (VG_(strstr)(fn, WHITELIST_FN[i]))
                {
                    instrument = 1;
                    break ;
                }
            }
            if (!instrument)
                return sb_in;
        }

        if (CLOS.ignorelist)
        {
            for (int i = 0 ; i < sizeof(BLACKLIST_FN) / sizeof(const HChar *) ; ++i)
                if (VG_(strstr)(fn, BLACKLIST_FN[i]))
                    return sb_in;
        }
    }

    // deep copy code until marker
    IRSB * sb_out = deepCopyIRSBExceptStmts(sb_in);
    Int i;
    for (i = 0 ; i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark ; ++i)
        addStmtToIRSB(sb_out, sb_in->stmts[i]);

    // instrument code
    for ( ; i < sb_in->stmts_used; ++i)
    {
        st = sb_in->stmts[i];

#if 0
        addr = st->Ist.IMark.addr + st->Ist.IMark.delta;
        ep = VG_(current_DiEpoch)();
        if (VG_(get_fnname)(ep, addr, &fn))
            if (CURRENT_TASK->client_id == 3)
                TASKGRIND_INFO("  Instrumenting %s", fn);
#endif

        switch (st->tag)
        {
            case Ist_NoOp:
            {
                // do nothing
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_IMark:
            {
                // do nothing
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_AbiHint:
            {
                // do nothing
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_Put:
            {
                // do nothing
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_PutI:
            {
                // do nothing
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_WrTmp:
            {
                // TODO: is it a load ?
                IRExpr * data = st->Ist.WrTmp.data;
                if (data->tag == Iex_Load)
                {
                    taskgrind_instrument_mem_access(
                        sb_out,
                        data->Iex.Load.addr,
                        sizeofIRType(data->Iex.Load.ty),
                        TASKGRIND_TASK_MEM_LOAD
                    );
                }
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_Store:
            {
                taskgrind_instrument_mem_access(
                    sb_out,
                    st->Ist.Store.addr,
                    sizeofIRType(typeOfIRExpr(sb_out->tyenv, st->Ist.Store.data)),
                    TASKGRIND_TASK_MEM_STORE
                );

                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_LoadG:
            {
                IRType type, wtype;

                typeOfIRLoadGOp(st->Ist.LoadG.details->cvt, &wtype, &type);

                taskgrind_instrument_mem_access(
                    sb_out,
                    st->Ist.LoadG.details->addr,
                    sizeofIRType(type),
                    TASKGRIND_TASK_MEM_LOAD
                );

                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_StoreG:
            {
                tl_assert(0);
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_CAS:
            {
                IRCAS * cas;
                Bool isDCAS;

                cas = st->Ist.CAS.details;

                isDCAS = cas->oldHi != IRTemp_INVALID;
                tl_assert((isDCAS && cas->expdHi) || (!isDCAS && !cas->expdHi));
                tl_assert((isDCAS && cas->dataHi) || (!isDCAS && !cas->dataHi));

                taskgrind_instrument_mem_access(
                    sb_out,
                    cas->addr,
                    isDCAS ? 2 : 1,
                    TASKGRIND_TASK_MEM_STORE_ATOMIC
                );

                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_LLSC:
            {
                tl_assert(0);
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_Dirty:
            {
                IRDirty * d;
                Int data_size;
                task_mem_access_type_t access_type;

                d = st->Ist.Dirty.details;
                if (d->mFx != Ifx_None)
                {
                    tl_assert(d->mAddr != NULL);
                    tl_assert(d->mSize != 0);

                    data_size = d->mSize;
                    if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                        access_type = TASKGRIND_TASK_MEM_LOAD;
                    else if (d->mFx == Ifx_Write)
                        access_type = TASKGRIND_TASK_MEM_STORE;
                    else
                        tl_assert(d->mFx == Ifx_None);

                    taskgrind_instrument_mem_access(
                        sb_out,
                        d->mAddr,
                        data_size,
                        access_type
                    );

                }
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_MBE:
            {
                // do nothing
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_Exit:
            {
                // do nothing
                addStmtToIRSB(sb_out, st);
                break ;
            }

            default:
            {
                TASKGRIND_DEBUG("ERROR unknown statement: ");
                ppIRStmt(st);
                TASKGRIND_DEBUG("\n");
                break ;
            }
        }
    } /* for each sb_in statements */

    return sb_out;
}

///////////////////////////////////////////////////////////////////////////////
//  Command line argument, init and finialize
///////////////////////////////////////////////////////////////////////////////

# if 0
static const char * clo_record      = NULL;
#endif

// global command line options
taskgrind_clo_t CLOS = {
    .dump=0,
    .instrumentlist=1,
    .ignorelist=0,
    .noanalysis=0,
};

static void
taskgrind_print_usage(void)
{
    VG_(printf)(
"    --dump               Dump internal data structures to dot files\n"
"    --no-instrument-list Instrument all memory accesses --- by default, only instrument the one in the instrument list\n"
"    --ignore-list        Do not instrument accesses in functions of the ignore list\n"
"    --no-analysis        Only record the execution but perform no analysis\n"
   );
}

static void
taskgrind_print_debug_usage(void)
{
   VG_(printf)(
"    (none)\n"
   );
}

static Bool
taskgrind_process_cmd_line_option(const HChar * arg)
{
    if (VG_(strcmp)(arg, "--dump") == 0)
    {
        CLOS.dump = 1;
        return True;
    }

    if (VG_(strcmp)(arg, "--no-instrument-list") == 0)
    {
        CLOS.instrumentlist = 0;
        return True;
    }

    if (VG_(strcmp)(arg, "--ignore-list") == 0)
    {
        CLOS.ignorelist = 1;
        return True;
    }

    if (VG_(strcmp)(arg, "--no-analysis") == 0)
    {
        CLOS.noanalysis = 1;
        return True;
    }

    return False;
}

static void
taskgrind_post_clo_init(void)
{
    if (CLOS.dump)
        TASKGRIND_INFO("Export to dot files enabled");
    else
        TASKGRIND_INFO("Export to dot files disabled");

    if (CLOS.instrumentlist || CLOS.ignorelist)
        TASKGRIND_INFO("You can modify the lists of instrumented symbol in the file `%s`", __FILE__);

    if (CLOS.instrumentlist)
    {
        TASKGRIND_INFO("Only instrumenting accesses in functions matching the instrumentlist:");
        for (int i = 0 ; i < sizeof(WHITELIST_FN) / sizeof(HChar *) ; ++i)
            TASKGRIND_INFO("   `%s`", WHITELIST_FN[i]);
    }

    if (CLOS.ignorelist)
    {
        TASKGRIND_INFO("Not instrumenting accesses in functions matching the ignorelist:");
        for (int i = 0 ; i < sizeof(BLACKLIST_FN) / sizeof(HChar *) ; ++i)
            TASKGRIND_INFO("   `%s`", BLACKLIST_FN[i]);
    }

    task_init();
}

static void
taskgrind_fini(Int exitcode)
{
    task_fini();
}

///////////////////////////////////////////////////////////////////////////////
//  Tool entry point
///////////////////////////////////////////////////////////////////////////////
static void
taskgrind_pre_clo_init(void)
{
    VG_(details_name)            ("Taskgrind");
    VG_(details_version)         (NULL);
    VG_(details_description)     ("a debugger for dependent tasks order of execution");
    VG_(details_copyright_author)(
            "Copyright (C) 2023, and GNU GPL'd, by Romain Pereira et al.");
    VG_(details_bug_reports_to)  ("romain.pereira@inria.fr");

    VG_(details_avg_translation_sizeB) ( 500 ); // TODO: adjust this

    VG_(needs_command_line_options)(taskgrind_process_cmd_line_option,
            taskgrind_print_usage,
            taskgrind_print_debug_usage);

    VG_(needs_client_requests)(taskgrind_handle_client_request);
    VG_(basic_tool_funcs)(taskgrind_post_clo_init, taskgrind_instrument, taskgrind_fini);

    TASKGRIND_INFO("Replacing malloc");
    VG_(needs_libc_freeres)();
    VG_(needs_cxx_freeres)();
    VG_(needs_malloc_replacement)(
        taskgrind_malloc,
        taskgrind___builtin_new,
        taskgrind___builtin_new_aligned,
        taskgrind___builtin_vec_new,
        taskgrind___builtin_vec_new_aligned,
        taskgrind_memalign,
        taskgrind_calloc,
        taskgrind_free,
        taskgrind___builtin_delete,
        taskgrind___builtin_delete_aligned,
        taskgrind___builtin_vec_delete,
        taskgrind___builtin_vec_delete_aligned,
        taskgrind_realloc,
        taskgrind_malloc_usable_size,
        16
    );

   #if 0
   VG_(needs_tool_errors)(
       taskgrind_eq_Error,
       taskgrind_before_pp_Error,
       taskgrind_pp_Error,
       False,
       taskgrind_update_extra,
       taskgrind_recognised_suppression,
       taskgrind_read_extra_suppression_info,
       taskgrind_error_matches_suppression,
       taskgrind_get_error_name,
       taskgrind_get_extra_suppression_info,
       taskgrind_print_extra_suppression_use,
       taskgrind_update_extra_suppression_use
   );
   #endif
}

///////////////////////////////////////////////////////////////////////////////
//   Callback to prepare environment before loading the client program
///////////////////////////////////////////////////////////////////////////////
static void
taskgrind_prepare_env(HChar *** envp)
{
    // TODO: make it portable between developer and installed setup

    // const HChar * relative_so = "/../taskgrind/runtimes-tools/ompt/build/libtaskgrind_omp.so";
    // HChar * absolute_so = VG_(malloc)("prepare_env", sizeof(HChar) * VG_(strlen)(VG_(libdir)) + VG_(strlen)(relative_so) + 1);
    // VG_(strcpy)(absolute_so, VG_(libdir));
    // VG_(strcat)(absolute_so, relative_so);

    HChar * absolute_so = VG_(getenv)("TASKGRIND_OMPT");
    if (absolute_so == NULL)
        TASKGRIND_WARN("OMPT tool not found - please build 'libtaskgrind_omp.so' and set 'TASKGRIND_OMPT'");
    else
    {
        TASKGRIND_INFO("Loading OMPT Plugin from %s", absolute_so);
        VG_(env_setenv)(envp, "OMP_TOOL_LIBRARIES", absolute_so);
        VG_(env_setenv)(envp, "LIBOMP_USE_HIDDEN_HELPER_TASK", "0");
        VG_(env_setenv)(envp, "KMP_ENABLE_TASK_THROTTLING", "0");
    }
}

VG_DETERMINE_INTERFACE_VERSION_WITH_ENV(taskgrind_pre_clo_init, taskgrind_prepare_env)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
