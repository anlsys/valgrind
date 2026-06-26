#ifndef __TRACEGRIND_SPMT_H__
# define __TRACEGRIND_SPMT_H__

# include "pub_tool_mallocfree.h"   /* malloc, free */

# define SPMT_F_ALLOC(S)        VG_(malloc)("tracegrind.spmt", S)
# define SPMT_F_REALLOC(P, S)   VG_(realloc)("tracegrind.spmt", P, S)
# define SPMT_F_FREE(X)         VG_(free)(X)
# define SPMT_F_ALLOC_NODE(S)   SPMT_F_ALLOC(sizeof(spmt_node_t))
# define SPMT_F_FREE_NODE(X)    SPMT_F_FREE(X)
# define SPMT_F_MEMSET(A, V, S) VG_(memset)(A, V, S)

# include "pub_tool_libcassert.h"   /* tl_assert */
# define SPMT_F_ASSERT(X)       tl_assert(X)

# include "pub_tool_libcprint.h"
# define SPMT_F_PRINTF(...)     VG_(printf)(__VA_ARGS__)

# define SPMT_DISABLE_LIBSTDC

/* Disable the SPMT's internal O(n^2) coherency self-checks (guarded by
 * '#ifndef NDEBUG' inside spmt.h). They would otherwise run on every single
 * traced access. This does NOT affect Valgrind's own 'tl_assert', which is
 * always active regardless of NDEBUG. */
# ifndef NDEBUG
#  define NDEBUG
# endif

# include "spmt/spmt.h"

#endif /* __TRACEGRIND_SPMT_H__ */
