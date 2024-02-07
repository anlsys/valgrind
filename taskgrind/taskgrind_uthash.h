#ifndef __TASKGRIND_UTHASH_H__

# define __TASKGRIND_UTHASH_H__

# define uthash_malloc(size)        VG_(malloc)("taskgrind.uthash", size)
# define uthash_free(ptr, size)     VG_(free)(ptr)
# define uthash_exit(c)             VG_(exit)(c)
# define uthash_memcmp(s1, s2, n)   VG_(memcmp)(s1, s2, n)
# define uthash_memset(s, c, n)     VG_(memset)(s, c, n)

# include "uthash.h"

#endif /* __TASKGRIND_UTHASH_H__ */
