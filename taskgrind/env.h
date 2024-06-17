// TODO: deadcode

#ifndef __ENV_H__
# define __ENV_H__

#include "pub_tool_libcprint.h"

typedef struct  taskgrind_env_s
{
    /* Which tasking environment are we instrumenting */
    const HChar * name;

}               taskgrind_env_t;

void taskgrind_env_init(taskgrind_env_t * env);
void taskgrind_env_detect(taskgrind_env_t * env);

#endif /* __ENV_H__ */
