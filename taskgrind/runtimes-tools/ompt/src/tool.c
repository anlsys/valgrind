# include <assert.h>
# include <omp.h>
# include <ompt.h>
# include <stdio.h>
# include <string.h>
# include <stdatomic.h>

# include <valgrind/taskgrind.h>
# define TOOL_NAME "Taskgrind"

///////////////////////////////////////////////////////////////////////////////
// OMPT EVENT CALLBACKS
///////////////////////////////////////////////////////////////////////////////

# define register_callback_t(name, type)                                            \
    do {                                                                            \
        type f_##name = &on_##name;                                                 \
        if (ompt_set_callback(name, (ompt_callback_t)f_##name) == ompt_set_never)   \
            printf("0: Could not register callback '" #name "'\n");                 \
    } while(0)
# define register_callback(name) register_callback_t(name, name##_t)

#if 1
# define DEBUG(...)                         \
    do {                                    \
        fprintf(stderr, "[OMPT] [DEBUG] "); \
        fprintf(stderr, __VA_ARGS__);       \
        fprintf(stderr, "\n");              \
    } while (0)
#else
# define DEBUG(...)
#endif

# define INFO(...)                          \
    do {                                    \
        fprintf(stdout, "[OMPT] [INFO] ");  \
        fprintf(stdout, __VA_ARGS__);       \
        fprintf(stdout, "\n");              \
    } while (0)

// TODO : use 'get_unique_id' or generate one 'per thread' to avoid memory
// contention on these global atomic when supporting actual multithreading in valgrind
static atomic_int NEXT_TASK_ID = 0;
static atomic_int NEXT_FORK_ID = 0;

// number of running openmp threads
static int NTHREADS = 1;

// number of procs
static int NPROCS = 1;

void
on_ompt_callback_task_create(
        ompt_data_t * encountering_task_data,
        const ompt_frame_t * encountering_task_frame,
        ompt_data_t * new_task_data,
        int flags,
        int has_dependences,
        const void *codeptr_ra
) {
    // INFO("[CREATE] encountering_task_data, = %p, new_task_data = %p, codeptr_ra = %p", encountering_task_data, new_task_data, codeptr_ra);

    uint64_t task_id = ++NEXT_TASK_ID;
    new_task_data->value = task_id;

    taskgrind_task_type_t type = (flags & ompt_task_explicit) ? TASKGRIND_TASK_TYPE_EXPLICIT : TASKGRIND_TASK_TYPE_IMPLICIT;
    unsigned int undeferred = (flags & ompt_task_undeferred) ? 1 : 0;
    // TODO : with OMP_NUM_THREADS=1, LLVM sets every tasks as 'undeferred', so we
    // cannot really track whether the task is undeferred because of user code
    // or runtime implementation
    // For now, assume all tasks are deferable, else we may loose expressed parallelism
    if (NTHREADS == 1 && undeferred)
    {
        static int reported = 0;
        if (!reported)
        {
            reported = 1;
            fprintf(stderr, "Undefered task with NTHREADS==1 - cannot tell if undefered by "
                    "the runtime or by the user code... There may be "
                    "false-negative\n");
        }
        undeferred = 0;
    }
    TASKGRIND_CREATE_EVENT(task_id, TASKGRIND_TASK_TYPE_EXPLICIT, undeferred);
}

void
on_ompt_callback_task_schedule(
    ompt_data_t * prior_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t * next_task_data
) {
//    INFO("[SCHEDULE] prior_task_data = %p, next_task_data = %p", prior_task_data, next_task_data);
    if (next_task_data)
        TASKGRIND_SCHEDULE_EVENT(next_task_data->value);
    else
        TASKGRIND_DETACH_FULFILL_EVENT(prior_task_data->value, prior_task_status == ompt_task_early_fulfill ? TASKGRIND_FULFILL_EARLY : TASKGRIND_FULFILL_LATE);
}

void
on_ompt_callback_implicit_task(
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    unsigned int actual_parallelism,
    unsigned int index,
    int ﬂags
) {
//    INFO("[IMPLICIT] task_data = %p ; actual_parallelism=%u ", task_data, actual_parallelism);

    uint64_t fork_id = parallel_data ? parallel_data->value : 0;

    if (endpoint == ompt_scope_begin)
    {
        uint64_t task_id = ++NEXT_TASK_ID;
        task_data->value = task_id;
        TASKGRIND_IMPLICIT_TASK_BEGIN_EVENT(fork_id, task_id);
    }

    if (endpoint == ompt_scope_end)
    {
        uint64_t task_id = task_data->value;
        TASKGRIND_IMPLICIT_TASK_END_EVENT(task_id);
    }
}

void
on_ompt_callback_dependences(
    ompt_data_t * task_data,
    const ompt_dependence_t * deps,
    int ndeps
) {
    // INFO("[IMPLICIT] task_data = %p", task_data);

    uint64_t task_id = task_data->value;
    int i;

    // convert to taskgrind dependency format
    for (i = 0 ; i < ndeps ; ++i)
    {
        const ompt_dependence_t * dep = deps + i;
        switch (dep->dependence_type)
        {
            case ompt_dependence_type_in:
            {
                TASKGRIND_DEPEND_EVENT(task_id, dep->variable.ptr, TASKGRIND_IN);
                break ;
            }

            case ompt_dependence_type_out:
            case ompt_dependence_type_inout:
            // mutexinoutset is implement as 'out' in practice (2023)
            case ompt_dependence_type_mutexinoutset:
            {
                TASKGRIND_DEPEND_EVENT(task_id, dep->variable.ptr, TASKGRIND_OUT);
                break ;
            }

            case ompt_dependence_type_inoutset:
            {
                TASKGRIND_DEPEND_EVENT(task_id, dep->variable.ptr, TASKGRIND_OUTSET);
                break ;
            }

            case ompt_dependence_type_source:
            case ompt_dependence_type_sink:
            case ompt_dependence_type_out_all_memory:
            case ompt_dependence_type_inout_all_memory:
            default:
            {
                INFO("Dependence type not supported %d\n", dep->dependence_type);
                assert(0);
                return ;
            }
        }
    }
}

// The ompt_callback_dispatch_t type is used for callbacks that are dispatched
// when a thread begins to execute a section or loop iteration.
void
on_ompt_callback_dispatch(
    ompt_data_t * parallel_data,
    ompt_data_t * task_data,
    ompt_dispatch_t kind,
    ompt_data_t instance
) {
    switch (kind)
    {
        case (ompt_dispatch_iteration):
        {
            assert("Not implemented" && 0);
            break ;
        }

        case (ompt_dispatch_section):
        {
            // assert("Not implemented" && 0);
            break ;
        }

        case (ompt_dispatch_ws_loop_chunk):
        {
            #if 0
            ompt_dispatch_chunk_t * chunk = (ompt_dispatch_chunk_t *) instance.ptr;
            printf("%lu %lu\n", chunk->start, chunk->iterations);
            assert("Not implemented" && 0);
            #endif
#if 0
            // OpenMP semantics to taskgrind:
            // 1) Create an implicit node
            // 2) Schedule it (instead of the current implicit task)
            uint64_t task_id = ++NEXT_TASK_ID;
            TASKGRIND_CREATE_EVENT(task_id, TASKGRIND_TASK_TYPE_EXPLICIT, 0);

            // TODO: schedule should be in 'dispatch' instead probably
            TASKGRIND_SCHEDULE_EVENT(task_id);
    #endif
            break ;
        }

        case (ompt_dispatch_taskloop_chunk):
        {
            //assert("Not implemented" && 0);
            break ;
        }

        case (ompt_dispatch_distribute_chunk):
        {
            assert("Not implemented" && 0);
            break ;
        }
    }
}

void
on_ompt_callback_sync_region(
    ompt_sync_region_t kind,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t * parallel_data,
    ompt_data_t * task_data,
    const void * codeptr_ra
) {
    switch (endpoint)
    {
        case (ompt_scope_begin):
        {
            switch (kind)
            {
                case (ompt_sync_region_barrier):
                case (ompt_sync_region_barrier_implicit):
                case (ompt_sync_region_barrier_explicit):
                case (ompt_sync_region_barrier_implementation):
                {
                    TASKGRIND_SYNC_EVENT(TASKGRIND_SYNC_BARRIER);
                    break ;
                }

                case (ompt_sync_region_taskwait):
                {
                    TASKGRIND_SYNC_EVENT(TASKGRIND_SYNC_TASKWAIT);
                    break ;
                }

                case (ompt_sync_region_taskgroup):
                {
                    TASKGRIND_SYNC_EVENT(TASKGRIND_SYNC_TASKGROUP);
                    break ;
                }

                case (ompt_sync_region_reduction):
                default:
                {
                    fprintf(stderr, "ompt_sync_region_reduction not implemented\n");
//                    assert(0 && "Not implemented");
                    break ;
                }
            }
            break ;
        }

        case (ompt_scope_end):
        case (ompt_scope_beginend):
        {
            // do nothing
            break ;
        }
    }
}

// "The ompt_callback_work_t type is used for callbacks that are dispatched
// when worksharing regions and taskloop regions begin and end. "
void
on_ompt_callback_work(
    ompt_work_t work_type,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t * parallel_data,
    ompt_data_t * task_data,
    uint64_t count,
    const void * codeptr_ra
) {
    switch (work_type)
    {
        // "Each thread executes its assigned chunks in the context of its
        // implicit task."
        //
        // ompt_work_loop unknown at runtime
        // ompt_work_loop_static static
        // ompt_work_loop_dynamic dynamic
        // ompt_work_loop_guided guided
        // ompt_work_loop_other implementation specific
        case (ompt_work_loop):
        {
            switch (endpoint)
            {
                case (ompt_scope_beginend):
                {
                    assert("Not implemented" && 0);
                    break ;
                }

                case (ompt_scope_begin):
                {
                    // assuming taskgrind run with 1 thread, virtually create
                    // 'NPROC' tasks for that parallel loop
                    for (int i = 0 ; i < NPROCS ; ++i)
                    {
                        uint64_t task_id = ++NEXT_TASK_ID;
                        TASKGRIND_CREATE_EVENT(task_id, TASKGRIND_TASK_TYPE_IMPLICIT, 0);
                    }
                    break ;
                }

                case (ompt_scope_end):
                {
                    // Reschedule implicit task
                    // uint64_t task_id = task_data->value;
                    // TASKGRIND_SCHEDULE_EVENT(task_id);
                    break ;
                }
            }
            break ;
        }

        case (ompt_work_loop_static):
        {
            assert("Not implemented" && 0);
            break ;
        }

        case (ompt_work_loop_dynamic):
        {
            assert("Not implemented" && 0);
            break ;
        }

        case (ompt_work_loop_guided):
        {
            assert("Not implemented" && 0);
            break ;
        }

        case (ompt_work_loop_other):
        {
            assert("Not implemented" && 0);
            break ;
        }

        case (ompt_work_sections):
        {
            // Nothing to do
            break ;
        }

        // "The single construct specifies that the associated structured block
        // is executed [...] in the context of its [thread] implicit task."
        //
        // -> so, there is nothing to do for taskgrind (?)
        case (ompt_work_single_executor):
        {
            switch (endpoint)
            {
                case (ompt_scope_begin):
                case (ompt_scope_end):
                case (ompt_scope_beginend):
                default:
                {
                    break ;
                }
            }
            break ;
        }

        case (ompt_work_single_other):
        {
//            assert("There should not be any other thread, we run in single-thread only" && 0);
            break ;
        }

        case (ompt_work_workshare):
        {
            assert("Not implemented" && 0);
            break ;
        }

        case (ompt_work_distribute):
        {
            assert("Not implemented" && 0);
            break ;
        }

        case (ompt_work_taskloop):
        {
            // Nothing to do
            // Maybe add an empty implicit taskgrind task for dependency with previous barriers
            break ;
        }

        case (ompt_work_scope):
        {
            assert("Not implemented" && 0);
            break ;
        }

        default:
        {
            assert("Unknown event" && 0);
            break ;
        }
    }
}

void
on_ompt_callback_parallel_begin(
    ompt_data_t * encountering_task_data,
    const ompt_frame_t * encountering_task_frame,
    ompt_data_t * parallel_data,
    unsigned int requested_parallelism,
    int flags,
    const void * codeptr_ra
) {
    NTHREADS = requested_parallelism;
    uint64_t fork_id = ++NEXT_FORK_ID;
    parallel_data->value = fork_id;
    TASKGRIND_FORK_POINT_EVENT(fork_id);
}

void
on_ompt_callback_parallel_end(
    ompt_data_t * parallel_data,
    ompt_data_t * encountering_task_data,
    int flags,
    const void * codeptr_ra
) {
    uint64_t fork_id = parallel_data->value;
    TASKGRIND_JOIN_POINT_EVENT(fork_id);
    NTHREADS = 1;
}

///////////////////////////////////////////////////////////////////////////////
// OMPT INIT / DEINIT CALLBACKS
///////////////////////////////////////////////////////////////////////////////
void
ompt_finalize(ompt_data_t * tool_data)
{
    (void)tool_data;
}

int ompt_initialize(
    ompt_function_lookup_t lookup,
    int initial_device_num,
    ompt_data_t * tool_data)
{
    (void) initial_device_num;
    (void) tool_data;
    ompt_set_callback_t ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
    register_callback(ompt_callback_parallel_begin);
    register_callback(ompt_callback_parallel_end);
    register_callback(ompt_callback_task_create);
    register_callback(ompt_callback_implicit_task);
    register_callback(ompt_callback_task_schedule);
    register_callback(ompt_callback_dependences);
    register_callback(ompt_callback_sync_region);
    register_callback(ompt_callback_work);
    register_callback(ompt_callback_dispatch);

    ompt_get_num_procs_t ompt_get_num_procs = (ompt_get_num_procs_t) lookup("ompt_get_num_procs");
    NPROCS = ompt_get_num_procs ? ompt_get_num_procs() : 1;

    return 1;
}

ompt_start_tool_result_t *
ompt_start_tool(
    unsigned int omp_version,
    const char * runtime_version)
{
    static ompt_start_tool_result_t data = {&ompt_initialize, &ompt_finalize, (ompt_data_t) NULL};
    INFO("%s: OpenMP %d and runtime %s", TOOL_NAME, omp_version, runtime_version);
    return &data;
}
