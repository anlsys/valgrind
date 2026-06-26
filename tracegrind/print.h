#ifndef __PRINT_H__
# define __PRINT_H__

# include "pub_tool_libcprint.h"

# define TRACEGRIND_PRINT_INFO_ID     0
# define TRACEGRIND_PRINT_WARN_ID     1
# define TRACEGRIND_PRINT_ERROR_ID    2
# define TRACEGRIND_PRINT_DEBUG_ID    3

extern const char * TRACEGRIND_PRINT_COLORS[4];
extern const char * TRACEGRIND_PRINT_HEADERS[4];

# define TRACEGRIND_PRINT(LVL, ...)                                          \
    do {                                                                     \
        VG_(umsg)("[%s%s\033[0m] ",                                          \
                TRACEGRIND_PRINT_COLORS[LVL], TRACEGRIND_PRINT_HEADERS[LVL]);\
        VG_(umsg)(__VA_ARGS__);                                              \
        VG_(umsg)("\n");                                                     \
    } while (0)

# define TRACEGRIND_INFO(...)  TRACEGRIND_PRINT(TRACEGRIND_PRINT_INFO_ID,  __VA_ARGS__)
# define TRACEGRIND_WARN(...)  TRACEGRIND_PRINT(TRACEGRIND_PRINT_WARN_ID,  __VA_ARGS__)
# define TRACEGRIND_ERR(...)   TRACEGRIND_PRINT(TRACEGRIND_PRINT_ERROR_ID, __VA_ARGS__)
# define TRACEGRIND_DEBUG(...) TRACEGRIND_PRINT(TRACEGRIND_PRINT_DEBUG_ID, __VA_ARGS__)

#endif /* __PRINT_H__ */
