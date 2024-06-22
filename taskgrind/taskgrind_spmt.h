#ifndef __TASKGRIND_SPMT_H__
# define __TASKGRIND_SPMT_H__

# include "pub_tool_mallocfree.h"   /* malloc, free */

# define SPMT_F_ALLOC(S)        VG_(malloc)("taskgrind.spmt", S)
# define SPMT_F_FREE(X)         VG_(free)(X)
# define SPMT_F_ALLOC_NODE(S)   SPMT_F_ALLOC(sizeof(spmt_node_t))
# define SPMT_F_FREE_NODE(X)    SPMT_F_FREE(X)
# define SPMT_F_MEMSET(A, V, S) VG_(memset)(A, V, S)

# include "pub_tool_libcassert.h"   /* tl_assert */
# define SPMT_F_ASSERT(X)       tl_assert(X)

# include "pub_tool_libcprint.h"
# define SPMT_F_PRINTF(...)     VG_(printf)(__VA_ARGS__)

# define SPMT_DISABLE_LIBSTDC

# define NDEBUG
# include "spmt/spmt.h"

#endif /* __TASKGRIND_SPMT_H__ */
