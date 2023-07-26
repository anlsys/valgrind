/*--------------------------------------------------------------------*/
/*--- OpenMP Tasks: a debugger for dependent tasks order of execution */
/*--------------------------------------------------------------------*/

/*
   This file is part of Nulgrind, the minimal Valgrind tool,
   which does no instrumentation or analysis.

   Copyright (C) 2002-2017 Nicholas Nethercote
      njn@valgrind.org

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
#include "pub_tool_libcbase.h"      /* strstr */
#include "pub_tool_libcprint.h"     /* snprintf */
#include "pub_tool_libcassert.h"    /* tool_panic, lt_assert */
#include "pub_tool_machine.h"       /* fnptr_to_fnentry */
#include "pub_tool_mallocfree.h"    /* malloc, free */

#if 1
# define OMP_DEBUG(...) VG_(printf)(__VA_ARGS__)
#else
# define OMP_DEBUG(...)
#endif

///////////////////////////////////////////////////////////////////////////////
//  Retrieve the task we are currently instrumenting
///////////////////////////////////////////////////////////////////////////////

# define uthash_malloc(size)        VG_(malloc)("omp_task.uthash", size)
# define uthash_free(ptr, size)     VG_(free)(ptr)
# define uthash_exit(c)             VG_(exit)(c)
# define uthash_memcmp(s1, s2, n)   VG_(memcmp)(s1, s2, n)
# define uthash_memset(s, c, n)     VG_(memset)(s, c, n)

# include "uthash.h"

// hmap of tasks
typedef struct  task_s
{
    // global task ID (= which task region)
    Int gid;

    // instance task ID (= which task construct)
    Int iid;

    // task region (= which file, which task)
    HChar * region;

    // hmap handle
    UT_hash_handle hh;

}               task_t;

// Task hash table
static task_t * TASKS;

// Task global ID counter
static Int TASKS_GID;

// Buffer to identify tasks to their global ID
static HChar TASK_IDENTIFIER_BUFFER[1024];

// The current task
static task_t *  CURRENT_TASK;

// retrieve a task from its file and line number
static inline task_t *
get_task(const HChar * dir, const HChar * file, UInt line, const HChar * fn)
{
    if (!VG_(strstr)(fn, "omp_task_entry") && !VG_(strstr)(fn, "_omp_fn"))
        return NULL;

    tl_assert(sizeof(HChar) == 1);
    int len = VG_(snprintf)(
        TASK_IDENTIFIER_BUFFER, sizeof(TASK_IDENTIFIER_BUFFER),
        "%s/%s:%u %s", dir, file, line, fn
    );
    tl_assert(len < sizeof(TASK_IDENTIFIER_BUFFER));

    unsigned hashv;
    HASH_VALUE(&TASK_IDENTIFIER_BUFFER, len, hashv);

    task_t * task;
    HASH_FIND_BYHASHVALUE(hh, TASKS, TASK_IDENTIFIER_BUFFER, len, hashv, task);

    if (task == NULL)
    {
        task            = (task_t *) VG_(malloc)("omp_task.get_task", sizeof(task_t) + len + 1);
        task->region    = (HChar *) (task + 1);
        task->gid       = TASKS_GID++;

        // TODO: task the OpenMP runtime which task instance is currently running
        task->iid       = -1;

        VG_(strcpy)(task->region, TASK_IDENTIFIER_BUFFER);

        HASH_ADD_KEYPTR_BYHASHVALUE(hh, TASKS, task->region, len, hashv, task);
    }

    tl_assert(task);
    return task;
}

static void
omp_task_update_current_task(IRSB * irsb, IRStmt * st)
{
    static const HChar * anonymous = "???";

    Addr addr;
    DiEpoch ep;
    const HChar * file;
    const HChar * dir;
    const HChar * fn;
    UInt line;

    addr = st->Ist.IMark.addr + st->Ist.IMark.delta;
    ep = VG_(current_DiEpoch)();
    if (!VG_(get_filename_linenum)(ep, addr, &file, &dir, &line))
    {
        dir  = anonymous;
        file = anonymous;
        line = 0;
    }
    if (!VG_(get_fnname)(ep, addr, &fn))
        CURRENT_TASK = NULL;
    else
        CURRENT_TASK = get_task(dir, file, line, fn);
}

///////////////////////////////////////////////////////////////////////////////
//  Coregrind callbacks
///////////////////////////////////////////////////////////////////////////////

static void
omp_task_post_clo_init(void)
{
}

typedef enum    omp_task_mem_access_type_e
{
    OMP_TASK_MEM_LOAD,
    OMP_TASK_MEM_STORE,
}               omp_task_mem_access_type_t;

static void
omp_task_instrument_mem_access_helper_load(
    Addr addr,
    SizeT size
) {
    OMP_DEBUG("(task=%p) LOAD  0x%010lX %lu\n", CURRENT_TASK, addr, size);
}

static void
omp_task_instrument_mem_access_helper_store(
    Addr addr,
    SizeT size
) {
    OMP_DEBUG("(task=%p) STORE 0x%010lX %lu\n", CURRENT_TASK, addr, size);
}

static void
omp_task_instrument_mem_access(
    IRSB * sb,
    IRExpr * addr,
    Int size,
    omp_task_mem_access_type_t access_type
) {
    if (CURRENT_TASK)
    {
        IRExpr ** argv;
        IRDirty * di;
        void * fn;
        const char * fn_name;

        if (access_type == OMP_TASK_MEM_LOAD)
        {
            fn      = omp_task_instrument_mem_access_helper_load;
            fn_name = "omp_task_instrument_mem_access_helper_load";
        }
        else
        {
            fn = omp_task_instrument_mem_access_helper_store;
            fn_name = "omp_task_instrument_mem_access_helper_store";
        }

        argv =  mkIRExprVec_2(addr, mkIRExpr_HWord(size));
        di   =  unsafeIRDirty_0_N(2, fn_name, VG_(fnptr_to_fnentry)(fn), argv);

        addStmtToIRSB(sb, IRStmt_Dirty(di));
    }
}

static IRSB *
omp_task_instrument(
    VgCallbackClosure * closure,
    IRSB * sb_in,
    const VexGuestLayout * layout,
    const VexGuestExtents * vge,
    const VexArchInfo * archinfo_host,
    IRType gWordTy,
    IRType hWordTy
) {
    IRSB * sb_out;
    IRStmt * st;
    Int i;

    if (gWordTy != hWordTy)
        VG_(tool_panic)("host/guest word size mismatch");

    sb_out = deepCopyIRSBExceptStmts(sb_in);
    for (i = 0 ; i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark ; ++i)
        addStmtToIRSB(sb_out, sb_in->stmts[i]);

    omp_task_update_current_task(sb_in, sb_in->stmts[i]);

    for ( ; i < sb_in->stmts_used; ++i)
    {
        st = sb_in->stmts[i];

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
                    omp_task_instrument_mem_access(
                        sb_out,
                        data->Iex.Load.addr,
                        sizeofIRType(data->Iex.Load.ty),
                        OMP_TASK_MEM_LOAD
                    );
                }
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_Store:
            {
                omp_task_instrument_mem_access(
                    sb_out,
                    st->Ist.Store.addr,
                    sizeofIRType(typeOfIRExpr(sb_out->tyenv, st->Ist.Store.data)),
                    OMP_TASK_MEM_STORE
                );
                addStmtToIRSB(sb_out, st);
                break ;
            }

            case Ist_LoadG:
            {
                IRType type, wtype;

                typeOfIRLoadGOp(st->Ist.LoadG.details->cvt, &wtype, &type);

                omp_task_instrument_mem_access(
                    sb_out,
                    st->Ist.LoadG.details->addr,
                    sizeofIRType(type),
                    OMP_TASK_MEM_LOAD
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

                omp_task_instrument_mem_access(
                    sb_out,
                    cas->addr,
                    isDCAS ? 2 : 1,
                    OMP_TASK_MEM_STORE
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
                omp_task_mem_access_type_t access_type;

                d = st->Ist.Dirty.details;
                if (d->mFx != Ifx_None)
                {
                    tl_assert(d->mAddr != NULL);
                    tl_assert(d->mSize != 0);

                    data_size = d->mSize;
                    if (d->mFx == Ifx_Read || d->mFx == Ifx_Modify)
                        access_type = OMP_TASK_MEM_LOAD;
                    else if (d->mFx == Ifx_Write)
                        access_type = OMP_TASK_MEM_STORE;
                    else
                        tl_assert(d->mFx == Ifx_None);

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
                OMP_DEBUG("ERROR unknown statement: ");
                ppIRStmt(st);
                OMP_DEBUG("\n");
                break ;
            }
        }
    } /* for each sb_in statements */

    return sb_out;
}

static void
omp_task_fini(Int exitcode)
{
#if 0
    const char * tags_str[] = {
        "Iex_Binder",
        "Iex_Get",
        "Iex_GetI",
        "Iex_RdTmp",
        "Iex_Qop",
        "Iex_Triop",
        "Iex_Binop",
        "Iex_Unop",
        "Iex_Load",
        "Iex_Const",
        "Iex_ITE",
        "Iex_CCall",
        "Iex_VECRET",
        "Iex_GSPTR"
    };

    IRExprTag tags[] = {
        Iex_Binder,
        Iex_Get,
        Iex_GetI,
        Iex_RdTmp,
        Iex_Qop,
        Iex_Triop,
        Iex_Binop,
        Iex_Unop,
        Iex_Load,
        Iex_Const,
        Iex_ITE,
        Iex_CCall,
        Iex_VECRET,
        Iex_GSPTR
    };
    OMP_DEBUG("-------------------------------\n");
    for (int i = 0 ; i < 14 ; ++i)
        OMP_DEBUG("%s = %u\n", tags_str[i], tags[i]);
#endif
}

static void
omp_task_pre_clo_init(void)
{
   VG_(details_name)            ("OpenMP Task");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a debugger for dependent tasks order of execution");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2017, and GNU GPL'd, by Romain Pereira.");
   VG_(details_bug_reports_to)  ("romain.pereira@outlook.com");

   VG_(details_avg_translation_sizeB) ( 275 ); // TODO: adjust this

   VG_(basic_tool_funcs)        (omp_task_post_clo_init,
                                 omp_task_instrument,
                                 omp_task_fini);
}

VG_DETERMINE_INTERFACE_VERSION(omp_task_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
