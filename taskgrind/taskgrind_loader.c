// TODO: header

# include "taskgrind.h"

# include "pub_tool_libcassert.h"
# include "pub_tool_libcbase.h" /* memset */
# include "coregrind/pub_core_debuginfo.h" /* Avmas */

// Loading error or success
static const HChar * LOAD_ERROR_INCOMPLETE = "Some symbols are missing: please check versions compatibility.";

static void
taskgrind_loading_error(const HChar * err)
{
    TASKGRIND_INFO("%s\n", err);
    VG_(exit)(1);
}

static void
taskgrind_loading_found(taskgrind_env_t * env)
{
    TASKGRIND_INFO("Detected '%s'", env->name);
}

///////////////////////////////////////////////////////////////////////////////
//  Generic Tasking Runtime
///////////////////////////////////////////////////////////////////////////////
static const HChar * GENERIC_NAME = "Generic";
static const HChar * GENERIC_SYMBOLS_LIB = "*";
# define GENERIC_SYMBOLS_N 1
static const HChar * GENERIC_SYMBOLS_NAMES[GENERIC_SYMBOLS_N] = {
    "__taskgrind_get_current_task_id"
};
static SymAVMAs GENERIC_SYMBOLS_AVMAS[GENERIC_SYMBOLS_N];
static taskgrind_get_task_key_t generic_get_task_id;

static Bool
generic_get_current_task(taskgrind_task_key_t * key)
{
    *key = generic_get_task_id();
    return True;
}

static void
generic_runtime_found(taskgrind_env_t * env)
{
    generic_get_task_id = (taskgrind_get_task_key_t) GENERIC_SYMBOLS_AVMAS[0].main;
    env->get_current_task = generic_get_current_task;
}

///////////////////////////////////////////////////////////////////////////////
//  LLVM OpenMP
///////////////////////////////////////////////////////////////////////////////
static const HChar * OMP_LLVM_NAME = "LLVM OpenMP";
static const HChar * OMP_LLVM_SYMBOLS_LIB = "libomp.so";
# define OMP_LLVM_SYMBOLS_N 1
static const HChar * OMP_LLVM_SYMBOLS_NAMES[OMP_LLVM_SYMBOLS_N] = {
    "__kmpc_get_taskid"
};
static SymAVMAs OMP_LLVM_SYMBOLS_AVMAS[OMP_LLVM_SYMBOLS_N];

typedef unsigned long long kmp_uint64_t;
static taskgrind_get_task_key_t llvm_get_task_id;

static Bool
llvm_omp_get_current_task(taskgrind_task_key_t * key)
{
    *key = llvm_get_task_id();
    return True;
}

static void
llvm_omp_runtime_found(taskgrind_env_t * env)
{
    llvm_get_task_id = (taskgrind_get_task_key_t) OMP_LLVM_SYMBOLS_AVMAS[0].main;
    env->get_current_task = llvm_omp_get_current_task;
}

///////////////////////////////////////////////////////////////////////////////
//  GNU OpenMP
///////////////////////////////////////////////////////////////////////////////
static const HChar * OMP_GNU_NAME = "GNU OpenMP";
static const HChar * OMP_GNU_SYMBOLS_LIB = "libgomp.so.1";
# define OMP_GNU_SYMBOLS_N 1
static const HChar * OMP_GNU_SYMBOLS_NAMES[OMP_GNU_SYMBOLS_N] = {
    "GOMP_task",
};
static SymAVMAs OMP_GNU_SYMBOLS_AVMAS[OMP_GNU_SYMBOLS_N];

static Bool
gnu_omp_get_current_task(taskgrind_task_key_t * key)
{
    *key = 0;
    return True;
}

///////////////////////////////////////////////////////////////////////////////
//  Detect tasking environment
///////////////////////////////////////////////////////////////////////////////
void
taskgrind_load_environment(taskgrind_env_t * env)
{
    const DiEpoch ep = VG_(current_DiEpoch)();
    Int k = 0;

    VG_(memset)(env, 0, sizeof(taskgrind_env_t));

    // Search for LLVM OpenMP environment
    {
        k = VG_(lookup_symbols_SLOW)(
                    ep,
                    OMP_LLVM_SYMBOLS_LIB,
                    OMP_LLVM_SYMBOLS_NAMES,
                    OMP_LLVM_SYMBOLS_AVMAS,
                    OMP_LLVM_SYMBOLS_N
            );
        if (k)
        {
            env->name = OMP_LLVM_NAME;
            taskgrind_loading_found(env);
            if (k != OMP_LLVM_SYMBOLS_N)
                return taskgrind_loading_error(LOAD_ERROR_INCOMPLETE);
            else
                return llvm_omp_runtime_found(env);
        }
    }

    // Search for GNU OpenMP environment
    {
        k = VG_(lookup_symbols_SLOW)(
                    ep,
                    OMP_GNU_SYMBOLS_LIB,
                    OMP_GNU_SYMBOLS_NAMES,
                    OMP_GNU_SYMBOLS_AVMAS,
                    OMP_GNU_SYMBOLS_N
            );
        if (k)
        {
            env->name = OMP_GNU_NAME;
            taskgrind_loading_found(env);
            if (k != OMP_GNU_SYMBOLS_N)
                taskgrind_loading_error(LOAD_ERROR_INCOMPLETE);
            else
            {
                env->get_current_task = gnu_omp_get_current_task;
                return ;
            }
        }
    }

    // Search for generic environment
    {
        k = VG_(lookup_symbols_SLOW)(
                    ep,
                    GENERIC_SYMBOLS_LIB,
                    GENERIC_SYMBOLS_NAMES,
                    GENERIC_SYMBOLS_AVMAS,
                    GENERIC_SYMBOLS_N
            );
        if (k)
        {
            env->name = GENERIC_NAME;
            taskgrind_loading_found(env);
            if (k != OMP_LLVM_SYMBOLS_N)
                return taskgrind_loading_error(LOAD_ERROR_INCOMPLETE);
            else
                return generic_runtime_found(env);
        }
    }

    // No environment detected, try to find generic symbols
}
