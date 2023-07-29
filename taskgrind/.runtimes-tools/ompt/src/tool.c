# include <assert.h>
# include <omp.h>
# include <ompt.h>
# include <stdio.h>
# include <string.h>

# define TOOL_NAME "Taskgrind_OMP"

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

void
on_ompt_callback_task_create(
        ompt_data_t * encountering_task_data,
        const ompt_frame_t * encountering_task_frame,
        ompt_data_t * new_task_data,
        int ﬂags,
        int has_dependences,
        const void *codeptr_ra
) {
    INFO("[CREATE] encountering_task_data, = %p, new_task_data = %p", encountering_task_data, new_task_data);
}

void
on_ompt_callback_task_schedule(
    ompt_data_t * prior_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t * next_task_data
) {
    INFO("[SCHEDULE] prior_task_data = %p, next_task_data = %p", prior_task_data, next_task_data);
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
    INFO("[IMPLICIT] task_data = %p", task_data);
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
    return 1;
}

ompt_start_tool_result_t *
ompt_start_tool(
    unsigned int omp_version,
    const char * runtime_version)
{
    static ompt_start_tool_result_t data = {&ompt_initialize, &ompt_finalize, (ompt_data_t) NULL};
    INFO("[OMPT] - %s - %d - %s", TOOL_NAME, omp_version, runtime_version);
    return &data;
}
