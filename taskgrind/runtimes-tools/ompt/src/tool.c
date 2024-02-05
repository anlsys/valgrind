# include <assert.h>
# include <bfd.h>
# include <omp.h>
# include <ompt.h>
# include <stdio.h>
# include <string.h>

# include <valgrind/taskgrind.h>
# define TOOL_NAME "Taskgrind"

# include "uthash.h"

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

// a map from OMP runtime 'task_data' to taskgrind 'client id'
typedef struct  client_id_s
{
    void * task_data;
    uint64_t value;
    UT_hash_handle hh;
}               client_id_t;

// next client id
static uint64_t NEXT_CLIENT_ID;

// the map
static client_id_t * CLIENT_IDS;

static inline client_id_t *
client_id_get(void * task_data)
{
    client_id_t * client_id;
    unsigned hashv;

    HASH_VALUE(&task_data, sizeof(void *), hashv);
    HASH_FIND_BYHASHVALUE(hh, CLIENT_IDS, &task_data, sizeof(void *), hashv, client_id);

    return client_id;
}

static inline client_id_t *
client_id_insert(void * task_data)
{
    client_id_t * client_id;
    unsigned hashv;

    HASH_VALUE(&task_data, sizeof(void *), hashv);
    HASH_FIND_BYHASHVALUE(hh, CLIENT_IDS, &task_data, sizeof(void *), hashv, client_id);

    client_id = client_id_get(task_data);
    if (client_id == NULL)
    {
        client_id = (client_id_t *) malloc(sizeof(client_id_t));
        assert(client_id);
        client_id->task_data = task_data;
        HASH_ADD_KEYPTR_BYHASHVALUE(hh, CLIENT_IDS, &(client_id->task_data), sizeof(void *), hashv, client_id);
    }
    return client_id;
}

void
on_ompt_callback_task_create(
        ompt_data_t * encountering_task_data,
        const ompt_frame_t * encountering_task_frame,
        ompt_data_t * new_task_data,
        int ﬂags,
        int has_dependences,
        const void *codeptr_ra
) {
    // INFO("[CREATE] encountering_task_data, = %p, new_task_data = %p, codeptr_ra = %p", encountering_task_data, new_task_data, codeptr_ra);

    client_id_t * client_id;

    client_id = client_id_insert(new_task_data);
    client_id->value = ++NEXT_CLIENT_ID;
    TASKGRIND_CREATE_EVENT(client_id->value, TASKGRIND_TASK_TYPE_EXPLICIT);
}

void
on_ompt_callback_task_schedule(
    ompt_data_t * prior_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t * next_task_data
) {
    // INFO("[SCHEDULE] prior_task_data = %p, next_task_data = %p", prior_task_data, next_task_data);

    client_id_t * client_id;

    client_id = client_id_get(next_task_data);
    TASKGRIND_SCHEDULE_EVENT(client_id->value);
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
    // INFO("[IMPLICIT] task_data = %p", task_data);

    client_id_t * client_id;

    if (endpoint == ompt_scope_begin)
    {
        client_id = client_id_insert(task_data);
        client_id->value = ++NEXT_CLIENT_ID;
        TASKGRIND_CREATE_EVENT(client_id->value, TASKGRIND_TASK_TYPE_IMPLICIT);
        TASKGRIND_SCHEDULE_EVENT(client_id->value);
    }
}

void
on_ompt_callback_dependences(
    ompt_data_t * task_data,
    const ompt_dependence_t * deps,
    int ndeps
) {
    // INFO("[IMPLICIT] task_data = %p", task_data);

    client_id_t * client_id;
    int i;

    client_id = client_id_get(task_data);

    // convert to taskgrind dependency format
    for (i = 0 ; i < ndeps ; ++i)
    {
        const ompt_dependence_t * dep = deps + i;
        switch (dep->dependence_type)
        {
            case ompt_dependence_type_in:
            {
                TASKGRIND_ACCESS_EVENT(client_id->value, dep->variable.ptr, TASKGRIND_IN);
                break ;
            }

            case ompt_dependence_type_out:
            case ompt_dependence_type_inout:
            // mutexinoutset is implement as 'out' in practice (2023)
            case ompt_dependence_type_mutexinoutset:
            {
                TASKGRIND_ACCESS_EVENT(client_id->value, dep->variable.ptr, TASKGRIND_OUT);
                break ;
            }

            case ompt_dependence_type_inoutset:
            {
                TASKGRIND_ACCESS_EVENT(client_id->value, dep->variable.ptr, TASKGRIND_OUTSET);
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
            DEBUG("sync region kind=%d", kind);
            TASKGRIND_SYNC_EVENT();
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
    register_callback(ompt_callback_task_create);
    register_callback(ompt_callback_implicit_task);
    register_callback(ompt_callback_task_schedule);
    register_callback(ompt_callback_dependences);
    register_callback(ompt_callback_sync_region);
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
