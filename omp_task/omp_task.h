// TODO: header

#ifndef OMP_TASK_H
# define OMP_TASK_H

#include "pub_tool_libcprint.h"     /* snprintf */

#if 1
# define OMP_DEBUG(...) VG_(printf)(__VA_ARGS__)
#else
# define OMP_DEBUG(...)
#endif

void omp_task_load_symbols(void);

#endif /* OMP_TASK_H */
