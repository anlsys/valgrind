// TODO: header

#ifndef __PRINT_H__
# define __PRINT_H__

# include "pub_tool_libcprint.h"     /* snumsg */

# define TASKGRIND_PRINT_INFO_ID     0
# define TASKGRIND_PRINT_WARN_ID     1
# define TASKGRIND_PRINT_ERROR_ID    2
# define TASKGRIND_PRINT_DEBUG_ID    3

extern char * TASKGRIND_PRINT_COLORS[4];
extern char * TASKGRIND_PRINT_HEADERS[4];

# define TASKGRIND_PRINT(LVL, ...)                                          \
    do {                                                                    \
        VG_(umsg)("[%s%s\033[0m] ",                                         \
                TASKGRIND_PRINT_COLORS[LVL], TASKGRIND_PRINT_HEADERS[LVL]); \
        VG_(umsg)(__VA_ARGS__);                                             \
        VG_(umsg)("\n");                                                    \
    } while (0)

# define TASKGRIND_INFO(...)  TASKGRIND_PRINT(TASKGRIND_PRINT_INFO_ID,  __VA_ARGS__)
# define TASKGRIND_WARN(...)  TASKGRIND_PRINT(TASKGRIND_PRINT_WARN_ID,  __VA_ARGS__)
# define TASKGRIND_ERR(...)   TASKGRIND_PRINT(TASKGRIND_PRINT_ERROR_ID, __VA_ARGS__)
# define TASKGRIND_DEBUG(...) TASKGRIND_PRINT(TASKGRIND_PRINT_DEBUG_ID, __VA_ARGS__)

#endif /* __PRINT_H__ */
