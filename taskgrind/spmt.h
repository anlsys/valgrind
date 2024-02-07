#ifndef __SPMT_H__
# define __SPMT_H__

# include <stdint.h>

/**
 * A sparse memory tree structure must be declared with
 *
 *  struct {
 *      [...]
 *      spmt_node_t spmt_node;
 *      [...]
 *  };
 */

/* SPMT pointer type */
# define SPMT_PTR_T uintptr_t

/* Default value is 1TB */
# ifndef SPMT_SPACE
#  define SPMT_SPACE ((SPMT_PTR_T)1024*1024*1024*1024)
# endif /* SPMT_SPACE */

/* Interfaces */
# ifndef SPMT_F_MEMSET
#  define SPMT_F_MEMSET(A, V, S) memset(A, V, S)
# endif

# ifndef SPMT_F_ALLOC_NODE
#  include <stdlib.h>
#  define SPMT_F_ALLOC_NODE() malloc(sizeof(spmt_node_t))
#  define SPMT_F_FREE_NODE(X) free(X)
# else /* SPMT_F_ALLOC_NODE */
#  ifndef SPMT_F_FREE_NODE
#   error "You must define 'SPMT_F_FREE_NODE'"
#  endif /* SPMT_F_FREE_NODE */
# endif

# ifndef SPMT_F_ASSERT
#  include <assert.h>
#  define SPMT_F_ASSERT(X) assert(X)
# endif

// TODO: implement generic interface for 'N' number of children and compare performances
# define SPMT_LEFT          0
# define SPMT_RIGHT         1
# define SPMT_N_CHILDREN    2

/* Represent the memory interval [begin ; end[ */
typedef struct  spmt_node_s
{
    SPMT_PTR_T begin;
    SPMT_PTR_T end;
    char filled;
    struct spmt_node_s * children[SPMT_N_CHILDREN];
}               spmt_node_t;

typedef spmt_node_t spmt_t;

/* Initialize a new spmt */
# define SPMT_INITIALIZE(T) do {                                            \
                                SPMT_F_MEMSET((T), 0, sizeof(spmt_node_t)); \
                                (T)->end = SPMT_SPACE;                      \
                                (T)->filled = 0;                            \
                            } while (0);

# define SPMT_INITIALIZE_STATIC {0, SPMT_SPACE, 0, {(void*) 0, (void*) 0}}

static inline void
__spmt_release_children(spmt_t * parent)
{
    int i;
    for (i = 0 ; i < SPMT_N_CHILDREN ; ++i)
    {
        if (parent->children[i])
        {
            __spmt_release_children(parent->children[i]);
            SPMT_F_FREE_NODE(parent->children[i]);
            parent->children[i] = (spmt_node_t *) 0;
        }
    }
}

// insert an aligned child
static inline void
__spmt_alloc_child(spmt_t * parent, int child_id, SPMT_PTR_T begin, SPMT_PTR_T end)
{
    spmt_node_t * child = SPMT_F_ALLOC_NODE();
    child->begin = begin;
    child->end = end;
    child->filled = 0;
    SPMT_F_MEMSET(child->children, 0, sizeof(child->children));
    parent->children[child_id] = child;
}

static inline void
__spmt_fill(spmt_t * parent, SPMT_PTR_T begin, SPMT_PTR_T end)
{
    SPMT_F_ASSERT(parent);
    SPMT_F_ASSERT(parent->begin <= begin);
    SPMT_F_ASSERT(end <= parent->end);
    SPMT_F_ASSERT(begin < end);

    if (begin == end)
        return ;

    // fast way out
    if (parent->begin == begin && parent->end == end)
    {
        parent->filled = 1;
        return __spmt_release_children(parent);
    }

    // insert new nodes
    SPMT_PTR_T half = parent->begin + (parent->end - parent->begin) / 2;
    if (begin < half)
    {
        // GOING LEFT
        if (!parent->children[SPMT_LEFT])
            __spmt_alloc_child(parent, SPMT_LEFT, parent->begin, half);

        if (end <= half)
            __spmt_fill(parent->children[SPMT_LEFT], begin, end);
        else
        {
            __spmt_fill(parent->children[SPMT_LEFT], begin, half);
            if (!parent->children[SPMT_RIGHT])
            {
                __spmt_alloc_child(parent, SPMT_RIGHT, half, parent->end);
                __spmt_fill(parent->children[SPMT_RIGHT], half,  end);
            }
        }
    }
    else
    {
        // GOING RIGHT
        if (!parent->children[SPMT_RIGHT])
            __spmt_alloc_child(parent, SPMT_RIGHT, half, parent->end);
        __spmt_fill(parent->children[SPMT_RIGHT], begin, end);
    }

    // check if the parent is now filled
    int filled = 1;
    for (int i = 0 ; i < SPMT_N_CHILDREN ; ++i)
    {
        if (!parent->children[i] || !parent->children[i]->filled)
        {
            filled = 0;
            break ;
        }
    }

    if (filled)
    {
        __spmt_release_children(parent);
        parent->filled = 1;
    }
}

/* Insert node 'N' in the tree 'T' and set 'M' to '1' if the node was merged */
# define SPMT_FILL(T, B, E)   do {                                \
                                    __spmt_fill((T), (B), (E));   \
                                } while(0);

static inline void
__spmt_dump(int (*print)(const char *, ...), spmt_node_t * parent, int depth)
{
    print("%*c(%llu, %llu, %d)\n", 2*depth + 1, ' ', parent->begin, parent->end, parent->filled);
    int i;
    for (i = 0 ; i < SPMT_N_CHILDREN ; ++i)
    {
        if (parent->children[i])
            __spmt_dump(print, parent->children[i], depth+1);
    }
}

# define SPMT_DUMP(F, T)    do {                        \
                                __spmt_dump(F, T, 0);   \
                            } while (0);

#endif /* __SPMT_H__ */
