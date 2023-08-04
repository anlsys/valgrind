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
#include "taskgrind_main.h"

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

// The tasking environment being instrumented
static taskgrind_env_t ENV;

# define uthash_malloc(size)        VG_(malloc)("taskgrind.uthash", size)
# define uthash_free(ptr, size)     VG_(free)(ptr)
# define uthash_exit(c)             VG_(exit)(c)
# define uthash_memcmp(s1, s2, n)   VG_(memcmp)(s1, s2, n)
# define uthash_memset(s, c, n)     VG_(memset)(s, c, n)

# include "uthash.h"

// list of tasks
typedef struct  task_array_s
{
    // tasks
    struct task_s ** tasks;

    // capacity
    UInt capacity;

    // number of tasks set
    UInt n;

}               task_array_t;

static void
task_array_init(task_array_t * array)
{
    array->tasks    = NULL;
    array->capacity = 0;
    array->n        = 0;
}

static void
task_array_push(task_array_t * array, struct task_s * task)
{
    if (array->n == array->capacity)
    {
        UInt capacity = (UInt)((array->n + 1) * 3 / 2);
        array->tasks = VG_(realloc)("task_array_t", array->tasks, sizeof(struct task_s *) * capacity);
        array->capacity = capacity;
    }
    array->tasks[array->n++] = task;
}

static struct task_s *
task_array_last(task_array_t * array)
{
    if (array->n == 0)
        return NULL;
    return array->tasks[array->n - 1];
}

static inline void
task_array_clear(task_array_t * array)
{
    array->n = 0;
}

static void
task_array_deinit(task_array_t * array)
{
    VG_(free)(array->tasks);
    array->n        = 0;
    array->capacity = 0;
}

// task accesses hmap for child dependences
typedef struct  task_accesses_t
{
    // dependency address
    Addr addr;

    // last task that had an 'out' for this address
    struct task_s * out;

    // last task that had an 'in' for this address
    task_array_t ins;

    // last task that had an 'outset' for this address
    task_array_t outsets;

    // the last successor task that matched the 'out' dependency (for redundancy filtering)
    struct task_s * last_out;

    // the last successor task that matched the 'in' dependency (for redundancy filtering)
    struct task_s * last_in;

    // the last successor task that matched the 'outset' dependency (for redundancy filtering)
    struct task_s * last_outset;

    // hmap handle
    UT_hash_handle hh;
}               task_accesses_t;

// tasks
typedef struct  task_s
{
    // unique identifier relative to its parent (independant from schedule)
    UWord child_id;

    // next unique identifier for a child
    UWord next_child_id;

    // id for the tool client (depend on schedule)
    UWord client_id;

    // task successors infered from dependences provided by user program
    task_array_t access_successors;

    // real successors using RaW on load/stores
    task_array_t actual_successors;

    // task hmap for child dependencies
    task_accesses_t * accesses;

    // parent
    struct task_s * parent;

    // hmap handle
    UT_hash_handle hh;
}               task_t;

// The tasks hmap
static task_t * TASKS;

// The current task
static task_t * TASK;

static inline task_t *
task_alloc(void)
{
    task_t * task;

    task = (task_t *) VG_(malloc)("task_alloc", sizeof(task_t));
    task->child_id          = TASK ? ++TASK->next_child_id : 0;
    task->next_child_id     = 0;
    task->client_id         = 0;
    task->accesses          = NULL;
    task->parent            = TASK;
    task_array_init(&task->access_successors);
    task_array_init(&task->actual_successors);

    return task;
}

static inline task_t *
task_create(UWord client_id)
{
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&client_id, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, TASKS, &client_id, sizeof(UWord), hashv, task);

    if (task == NULL)
    {
        task = task_alloc();
        task->client_id = client_id;
        HASH_ADD_KEYPTR_BYHASHVALUE(hh, TASKS, &(task->client_id), sizeof(UWord), hashv, task);
        TASKGRIND_DEBUG("Task create %p (parent %p)", (void *) client_id, (void *) (task->parent ? task->parent->client_id : 0));
    }

    tl_assert(task);

    return task;
}

static inline task_t *
task_get(UWord client_id)
{
    task_t * task;
    unsigned hashv;

    HASH_VALUE(&client_id, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, TASKS, &client_id, sizeof(UWord), hashv, task);

    return task;
}

static inline void
task_schedule(UWord client_id)
{
    UWord old = TASK ? TASK->client_id : -1;   // DEBUG REMOVE ME

    TASK = task_get(client_id);
    tl_assert(TASK);

    if (old == -1 || old != TASK->client_id)                                      // DEBUG REMOVE ME
    {
        TASKGRIND_DEBUG("Task switch %p -> %p", (void *)old, (void *)TASK->client_id);  // DEBUG REMOVE ME
    }
}

static inline task_accesses_t *
task_accesses_get(task_t * parent, UWord addr)
{
    task_accesses_t * accesses;
    unsigned hashv;

    HASH_VALUE(&addr, sizeof(UWord), hashv);
    HASH_FIND_BYHASHVALUE(hh, parent->accesses, &addr, sizeof(UWord), hashv, accesses);

    if (!accesses)
    {
        accesses = (task_accesses_t *) VG_(malloc)("task_access", sizeof(task_accesses_t));
        accesses->addr          = addr;
        accesses->out           = NULL;
        accesses->last_out      = NULL;
        accesses->last_in       = NULL;
        accesses->last_outset   = NULL;
        task_array_init(&accesses->ins);
        task_array_init(&accesses->outsets);

        HASH_ADD_KEYPTR_BYHASHVALUE(hh, parent->accesses, &(accesses->addr), sizeof(UWord), hashv, accesses);
    }
    tl_assert(accesses);

    return accesses;
}

// return true if the given task access is redundant for the given address
static inline Bool
task_access_is_redundant(
    task_t * task,
    task_accesses_t * accesses,
    UWord addr,
    UWord type)
{
    switch (type)
    {
        case (TASKGRIND_OUT):
        {
            if (accesses->last_out == task)
                return True;
            accesses->last_out = task;
            return False;
        }

        case (TASKGRIND_IN):
        {
            if (accesses->last_out == task || accesses->last_in == task)
                return True;
            accesses->last_in = task;
            return False;
        }

        case (TASKGRIND_OUTSET):
        {
            if (accesses->last_out == task || accesses->last_outset == task)
                return True;
            accesses->last_outset = task;
            return False;
        }

        default:
        {
            tl_assert(0);
            return False;
        }
    }
}

// set the edge pred -> succ
static inline void
task_link_access(task_t * pred, task_t * succ)
{
    // filter out multiple edges
    if (task_array_last(&pred->access_successors) == succ)
        return ;
    task_array_push(&pred->access_successors, succ);
    TASKGRIND_DEBUG("set %p as a successor to %p", (void *)pred->client_id, (void *)succ->client_id);
}

// add a dependency to the task following RaW constraints
static inline void
task_access(UWord client_id, UWord addr, UWord type)
{
    tl_assert(type == TASKGRIND_IN || type == TASKGRIND_OUT || type == TASKGRIND_OUTSET);
    TASKGRIND_DEBUG("Task %p accesses %s at %p", (void *) client_id, type == TASKGRIND_IN ? "IN" : type == TASKGRIND_OUT ? "OUT" : type == TASKGRIND_OUTSET ? "OUTSET" : "(null)", (void *) addr);

    // retrieve current task and its parent accesses
    task_t * task, * in;
    task_accesses_t * accesses;
    int i;

    task = task_get(client_id);
    tl_assert(task);
    tl_assert(task->parent);

    accesses = task_accesses_get(task->parent, addr);
    tl_assert(accesses);

    // filter out redundancies
    if (!task_access_is_redundant(task, accesses, addr, type))
    {
        // infer edge between 'task' and its predecessor

        // case 1.1 - the generated task is dependant of previous 'in'
        if (accesses->ins.n && (type == TASKGRIND_OUT || type == TASKGRIND_OUTSET))
        {
            if (type == TASKGRIND_OUTSET)
            {
                /**
                 * in:      O O O
                 *           \|/
                 * out:       X     <- we insert this empty node
                 *           / \
                 * outset:  O   O
                 */
                accesses->out = task_alloc();
                for (i = 0 ; i < accesses->ins.n ; ++i)
                {
                    // prevent cyclic deps in case 'task' already had an 'in'
                    // dep type on the same addr previously
                    in = accesses->ins.tasks[i];
                    if (in != task)
                    {
                        TASKGRIND_DEBUG("linking");
                        task_link_access(in, accesses->out);
                    }
                    else
                        TASKGRIND_DEBUG("not linking as %p == %p", (void *)in->client_id, (void *)task->client_id);
                }
            }
            else
            {
                for (i = 0 ; i < accesses->ins.n ; ++i)
                {
                    in = accesses->ins.tasks[i];
                    task_link_access(in, task);
                }
            }
        } // 1.1

        // save access for future task
        switch (type)
        {
            case (TASKGRIND_IN):
            {
                task_array_push(&accesses->ins, task);
                break ;
            }

            case (TASKGRIND_OUT):
            {
                task_array_clear(&accesses->ins);
                task_array_clear(&accesses->outsets);
                accesses->out = task;
                break ;
            }

            case (TASKGRIND_OUTSET):
            {
                task_array_push(&accesses->outsets, task);
                break ;
            }

            default:
            {
                tl_assert(0);
                break ;
            }
        }

    } // redundant check
}

static Bool
taskgrind_handle_client_request(ThreadId tid, UWord * arg, UWord * ret)
{
    switch (arg[0])
    {
        case VG_USERREQ__TASKGRIND_CREATE_EVENT:
        {
            task_create(arg[1]);
            return True;
        }

        case VG_USERREQ__TASKGRIND_SCHEDULE_EVENT:
        {
            task_schedule(arg[1]);
            return True;
        }

        case VG_USERREQ__TASKGRIND_ACCESS_EVENT:
        {
            task_access(arg[1], arg[2], arg[3]);
            return True;
        }

        default:
        {
            TASKGRIND_WARN("Unknown client request code %llx", (ULong)arg[0]);
            return False;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Instrumentation
///////////////////////////////////////////////////////////////////////////////

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
    if (TASK)
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

    // nothing to do until we detected the tasking environment
    if (!ENV.name)
        taskgrind_env_detect(&ENV);
    if (!ENV.name || !TASK)
        return sb_in;

    // instrument code
    sb_out = deepCopyIRSBExceptStmts(sb_in);
    for (i = 0 ; i < sb_in->stmts_used && sb_in->stmts[i]->tag != Ist_IMark ; ++i)
        addStmtToIRSB(sb_out, sb_in->stmts[i]);

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

///////////////////////////////////////////////////////////////////////////////
//  Command line argument, init and finialize
///////////////////////////////////////////////////////////////////////////////

static const char * clo_record      = NULL;

static const char * clo_compare     = NULL;
static const char * clo_compare_a   = NULL;
static const char * clo_compare_b   = NULL;

static void
taskgrind_fini(Int exitcode)
{
    if (clo_compare_a && clo_compare_b)
    {
        TASKGRIND_DEBUG("Comparing task graph '%s' with '%s'", clo_compare_a, clo_compare_b);
    }
}

static void
taskgrind_print_usage(void)
{
   VG_(printf)(
"    --record=<name>            Execute and record the task graph into <name> directory\n"
"    --compare=<name1>,<name2>  Compare the two task graph previously recorded\n"
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
taskgrind_clo_error(const HChar * err)
{
    VG_(printf)("Error usage: %s\n", err);
    taskgrind_print_usage();
    VG_(exit)(1);
    return False;
}

static Bool
taskgrind_process_cmd_line_option(const HChar * arg)
{
    if (VG_(strcmp)(arg, "--record") == 0)
    {
        if (!VG_STR_CLO(arg, "--record", clo_record))
        {
            HChar * record;
            struct vki_timeval tv;
            struct vki_timezone tz;

            record  = (HChar *) VG_(malloc)("clo_record", sizeof(UChar) * 1024);
            VG_(gettimeofday)(&tv, &tz);
            VG_(snprintf)(record, 1024, "taskgrind-%ld", 1000000 * tv.tv_sec + tv.tv_usec);
            clo_record = (const HChar *) record;
        }
    }
    else if (VG_STR_CLO(arg, "--compare", clo_compare))
    {
        char * comma = VG_(strchr)(clo_compare, ',');
        if (!comma)
            return taskgrind_clo_error("invalid task graph record names");

        comma[0] = 0;
        clo_compare_a = clo_compare;
        clo_compare_b = comma + 1;

        if (!*clo_compare_a || !*clo_compare_b)
            return taskgrind_clo_error("invalid task graph record names");

    }
    else
    {
        return False;
    }

    return True;
}

static void
taskgrind_post_clo_init(void)
{
    if (clo_record)
        TASKGRIND_INFO("Recording task graph to '%s'", clo_record);

    if (!clo_record && !clo_compare)
       taskgrind_clo_error("at least one command line option must be passed");
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
   VG_(details_bug_reports_to)  ("romain.pereira@outlook.com");

   VG_(details_avg_translation_sizeB) ( 500 ); // TODO: adjust this

   VG_(needs_command_line_options)(taskgrind_process_cmd_line_option,
                                   taskgrind_print_usage,
                                   taskgrind_print_debug_usage);
   VG_(needs_client_requests)(taskgrind_handle_client_request);
   VG_(basic_tool_funcs)(taskgrind_post_clo_init, taskgrind_instrument, taskgrind_fini);
   taskgrind_env_init(&ENV);
}

VG_DETERMINE_INTERFACE_VERSION(taskgrind_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
