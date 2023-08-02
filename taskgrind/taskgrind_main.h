// TODO: header

#ifndef TASKGRIND_MAIN_H
# define TASKGRIND_MAIN_H

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

# define TASKGRIND_WARN(...)    do {                            \
                                    VG_(umsg)("[WARN] ");       \
                                    VG_(umsg)(__VA_ARGS__);     \
                                    VG_(umsg)("\n");            \
                                } while (0)

typedef struct  taskgrind_env_s
{
    /* Which tasking environment are we instrumenting */
    const HChar * name;

}               taskgrind_env_t;

void taskgrind_env_init(taskgrind_env_t * env);
void taskgrind_env_detect(taskgrind_env_t * env);

#endif /* TASKGRIND_MAIN_H */
