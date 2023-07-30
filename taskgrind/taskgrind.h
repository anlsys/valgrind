// TODO: header

#ifndef TASKGRIND_TASK_H
# define TASKGRIND_TASK_H

#include "pub_tool_libcprint.h"     /* snumsg */

#if 1
# define TASKGRIND_DEBUG(...)   do {                            \
                                    VG_(umsg)("[DEBUG] ");      \
                                    VG_(umsg)(__VA_ARGS__);     \
                                    VG_(umsg)("\n");            \
                                } while (0)
#else
# define TASKGRIND_DEBUG(...)
#endif

# define TASKGRIND_INFO(...)    do {                            \
                                    VG_(umsg)("[INFO] ");       \
                                    VG_(umsg)(__VA_ARGS__);     \
                                    VG_(umsg)("\n");            \
                                } while (0)

typedef unsigned long long taskgrind_task_key_t;
typedef taskgrind_task_key_t (*taskgrind_get_task_key_t)(void);

typedef struct  taskgrind_env_s
{
    /* Which tasking environment are we instrumenting */
    const HChar * name;

    /* Retrieve current task unique identifier */
    Bool (*get_current_task)(taskgrind_task_key_t *);

}               taskgrind_env_t;

void taskgrind_load_environment(taskgrind_env_t * env);

#endif /* TASKGRIND_TASK_H */
