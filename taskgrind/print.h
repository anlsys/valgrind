// TODO: header

#ifndef __PRINT_H__
# define __PRINT_H__

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

# define TASKGRIND_ERR(...)     do {                            \
                                    VG_(umsg)("[ERRR] ");       \
                                    VG_(umsg)(__VA_ARGS__);     \
                                    VG_(umsg)("\n");            \
                                } while (0)

#endif /* __PRINT_H__ */
