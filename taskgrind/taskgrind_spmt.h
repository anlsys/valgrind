#ifndef __TASKGRIND_SPMT_H__
# define __TASKGRIND_SPMT_H__

# include "pub_tool_mallocfree.h"   /* malloc, free */

# define SPMT_F_ALLOC_NODE()    VG_(malloc)("taskgrind.spmt", sizeof(spmt_node_t))
# define SPMT_F_FREE_NODE(X)    VG_(free)(X)
# define SPMT_F_MEMSET(A, V, S) VG_(memset)(A, V, S)

# include "pub_tool_libcassert.h"   /* tl_assert */
# define SPMT_F_ASSERT(X)       tl_assert(X)

# include "pub_tool_libcprint.h"
# define SPMT_F_PRINTF(...)     VG_(printf)(__VA_ARGS__)

# include "spmt.h"

#endif /* __TASKGRIND_SPMT_H__ */
