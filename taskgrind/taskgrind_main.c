/*--------------------------------------------------------------------*/
/*--- Taskgrind: a debugger for dependent tasks order of execution */
/*--------------------------------------------------------------------*/

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

#include "taskgrind.h"

#include "pub_tool_basics.h"
#include "pub_tool_guest.h"         /* thread state */
#include "pub_tool_tooliface.h"
#include "pub_tool_libcbase.h"      /* strstr */
#include "pub_tool_libcassert.h"    /* tool_panic, lt_assert */
#include "pub_tool_machine.h"       /* fnptr_to_fnentry */
#include "pub_tool_mallocfree.h"    /* malloc, free */

///////////////////////////////////////////////////////////////////////////////
//  Retrieve the task we are currently instrumenting
///////////////////////////////////////////////////////////////////////////////

# define uthash_malloc(size)        VG_(malloc)("taskgrind.uthash", size)
# define uthash_free(ptr, size)     VG_(free)(ptr)
# define uthash_exit(c)             VG_(exit)(c)
# define uthash_memcmp(s1, s2, n)   VG_(memcmp)(s1, s2, n)
# define uthash_memset(s, c, n)     VG_(memset)(s, c, n)

# include "uthash.h"

// The tasking environment being instrumented
static taskgrind_env_t ENV;

// hmap of task region
typedef struct  task_region_s
{
    // task region ID (= which task region)
    Int id;

    // task region (= which file, which task)
    HChar * name;

    // hmap handle
    UT_hash_handle hh;
}               task_region_t;

// list of tasks
typedef struct  task_list_elt_s
{
    // the task pointed
    struct task_s * task;

    // the next element
    struct task_list_elt_s * next;
}               task_list_elt_t;

// tasks
typedef struct  task_s
{
    // instance task ID (= which task construct)
    Int iid;

    // task region associated to the task
    task_region_t * region;

    // task successors infered from data dependencies
    struct task_list_elt_s * successors;

}               task_t;

// Task hash table
static task_region_t * TASK_REGIONS;

// Task global ID counter
static Int TASK_REGIONS_ID;

// Buffer to identify a task region
static HChar TASK_REGION_IDENTIFIER_BUFFER[1024];

// The current task
static task_t * TASK;

// The current task region
static task_region_t * TASK_REGION;

// Anonymous name
static const HChar * ANONYMOUS = "???";

// retrieve a task region from its file and line number
static inline task_region_t *
get_task_region(const HChar * dir, const HChar * file, UInt line, const HChar * fn)
{
    if (!VG_(strstr)(fn, "taskgrind_entry") && !VG_(strstr)(fn, "_omp_fn"))
        return NULL;

    tl_assert(sizeof(HChar) == 1);
    int len = VG_(snprintf)(
        TASK_REGION_IDENTIFIER_BUFFER, sizeof(TASK_REGION_IDENTIFIER_BUFFER),
        "%s/%s:%u %s", dir, file, line, fn
    );
    tl_assert(len < sizeof(TASK_REGION_IDENTIFIER_BUFFER));

    unsigned hashv;
    HASH_VALUE(&TASK_REGION_IDENTIFIER_BUFFER, len, hashv);

    task_region_t * region;
    HASH_FIND_BYHASHVALUE(hh, TASK_REGIONS, TASK_REGION_IDENTIFIER_BUFFER, len, hashv, region);

    if (region == NULL)
    {
        region          = (task_region_t *) VG_(malloc)("taskgrind.get_task", sizeof(task_region_t) + len + 1);
        region->id      = TASK_REGIONS_ID++;
        region->name    = (HChar *) (region + 1);
        VG_(strcpy)(region->name, TASK_REGION_IDENTIFIER_BUFFER);

        HASH_ADD_KEYPTR_BYHASHVALUE(hh, TASK_REGIONS, region->name, len, hashv, region);
    }

    tl_assert(region);
    return region;
}

# define KMP_TASKDATA_SIZE 320

typedef struct  kmp_taskdata_s
{
    UChar bytes[KMP_TASKDATA_SIZE];
}              kmp_taskdata_t; 

static void
taskgrind_update_current_task(
    VgCallbackClosure * closure,
    IRSB * irsb,
    Int i
) {
    IRStmt * st;
    Addr addr;
    DiEpoch ep;
    const HChar * file;
    const HChar * dir;
    const HChar * fn;
    UInt line;

    st = irsb->stmts[i];
    addr = st->Ist.IMark.addr + st->Ist.IMark.delta;
    ep = VG_(current_DiEpoch)();
    if (!VG_(get_filename_linenum)(ep, addr, &file, &dir, &line))
    {
        dir  = ANONYMOUS;
        file = ANONYMOUS;
        line = 0;
    }
    if (!VG_(get_fnname)(ep, addr, &fn))
        fn  = ANONYMOUS;

    // TASKGRIND_DEBUG("%s/%s %s\n", dir, file, fn);
    
    TASK_REGION = get_task_region(dir, file, line, fn);
    TASK        = NULL;

# if 0
    // Save the current task for TDG export
    if (TASK)
    {
        TASK->region    = TASK_REGION;
        TASKS[TASK_ID]  = TASK;
    }
# endif
}

///////////////////////////////////////////////////////////////////////////////
//  Coregrind callbacks
///////////////////////////////////////////////////////////////////////////////

static void
taskgrind_post_clo_init(void)
{
}

typedef enum    taskgrind_mem_access_type_e
{
    TASKGRIND_TASK_MEM_LOAD,
    TASKGRIND_TASK_MEM_STORE,
}               taskgrind_mem_access_type_t;

static void
taskgrind_instrument_mem_access_helper_load(
    Addr addr,
    SizeT size
) {
    //TASKGRIND_DEBUG("(task=%p) LOAD  0x%010lX %lu\n", TASK, addr, size);
}

static void
taskgrind_instrument_mem_access_helper_store(
    Addr addr,
    SizeT size
) {
    //TASKGRIND_DEBUG("(task=%p) STORE 0x%010lX %lu\n", TASK, addr, size);
}

static void
taskgrind_instrument_mem_access(
    IRSB * sb,
    IRExpr * addr,
    Int size,
    taskgrind_mem_access_type_t access_type
) {
    if (TASK_REGION)
    {
        IRExpr ** argv;
        IRDirty * di;
        void * fn;
        const char * fn_name;

        if (access_type == TASKGRIND_TASK_MEM_LOAD)
        {
            fn      = taskgrind_instrument_mem_access_helper_load;
            fn_name = "taskgrind_instrument_mem_access_helper_load";
        }
        else
        {
            fn = taskgrind_instrument_mem_access_helper_store;
            fn_name = "taskgrind_instrument_mem_access_helper_store";
        }

        argv =  mkIRExprVec_2(addr, mkIRExpr_HWord(size));
        di   =  unsafeIRDirty_0_N(2, fn_name, VG_(fnptr_to_fnentry)(fn), argv);

        addStmtToIRSB(sb, IRStmt_Dirty(di));
    }
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
    IRSB * sb_out;
    IRStmt * st;
    Int i;

    if (gWordTy != hWordTy)
        VG_(tool_panic)("host/guest word size mismatch");

    if (!ENV.name)
        taskgrind_load_environment(&ENV);

    sb_out = deepCopyIRSBExceptStmts(sb_in);
    for (i = 0 ; i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark ; ++i)
        addStmtToIRSB(sb_out, sb_in->stmts[i]);

    taskgrind_update_current_task(closure, sb_in, i);

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
                    TASKGRIND_TASK_MEM_STORE
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
                taskgrind_mem_access_type_t access_type;

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

static void
taskgrind_fini(Int exitcode)
{

}

static void
taskgrind_pre_clo_init(void)
{
   VG_(details_name)            ("Taskgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a debugger for dependent tasks order of execution");
   VG_(details_copyright_author)(
      "Copyright (C) 2002-2017, and GNU GPL'd, by Romain Pereira.");
   VG_(details_bug_reports_to)  ("romain.pereira@outlook.com");

   VG_(details_avg_translation_sizeB) ( 275 ); // TODO: adjust this

   VG_(basic_tool_funcs)        (taskgrind_post_clo_init,
                                 taskgrind_instrument,
                                 taskgrind_fini);
}

VG_DETERMINE_INTERFACE_VERSION(taskgrind_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
