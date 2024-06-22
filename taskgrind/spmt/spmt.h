//  A self-balanced red-black interval tree where each node is a disjoint interval
//  Ideas for improving performances
//  
//     '__spmt_includes_fixup'
//     This routine is executed after inserting a node N.
//     It merges successive intervals from N to the root, deleting one of the two node on a merge

#ifndef __SPMT_H__
# define __SPMT_H__

# include <stdint.h>

/* SPMT pointer type */
# ifndef SPMT_PTR_T
#  define SPMT_PTR_T        long unsigned int
#  define SPMT_PTR_T_ID     "%lu"
# endif

# define SPMT_NULL ((void *) 0)

/* Interfaces */
# ifndef SPMT_F_MEMSET
#  define SPMT_F_MEMSET(A, V, S) memset(A, V, S)
# endif

# ifndef SPMT_F_PRINTF
#  define SPMT_F_PRINTF(...) printf(__VA_ARGS__)
# endif

# ifndef SPMT_F_ALLOC
#  include <stdlib.h>
#  define SPMT_F_ALLOC(S)       malloc(S)
#  define SPMT_F_FREE(X)        free(S)
#  define SPMT_F_ALLOC_NODE()   malloc(sizeof(spmt_node_t))
#  define SPMT_F_FREE_NODE(X)   free(X)
# endif

# ifndef SPMT_F_ASSERT
#  include <assert.h>
#  define SPMT_F_ASSERT(X) assert(X)
# endif

# define SPMT_MIN(X, Y) ((X) < (Y) ?  (X) : (Y))
# define SPMT_MAX(X, Y) ((X) < (Y) ?  (Y) : (X))
# define SPMT_ABS(X)    ((X) < 0)  ? -(X) : (X)

typedef enum
{
    SPMT_LEFT       = 0,
    SPMT_RIGHT      = 1,
    SPMT_N_CHILDREN = 2,
} spmt_direction_t;

typedef enum
{
    SPMT_BLACK = 0,
    SPMT_RED   = 1,
} spmt_color_t;

typedef struct
{
    SPMT_PTR_T a, b;
} interval_t;

/* Represent the memory interval [a ; b[ */
typedef struct  spmt_node_s
{
    struct spmt_node_s * parent;
    union {
        struct spmt_node_s * child[SPMT_N_CHILDREN];
        struct {
            struct spmt_node_s * left;
            struct spmt_node_s * right;
        };
    };
    interval_t I;
    struct {
        interval_t I;
    } includes;

    spmt_color_t color;
}               spmt_node_t;

/* sparse memory tree */
typedef struct  spmt_t
{
    spmt_node_t * root;
}               spmt_t;

/* an array with the result of two spmt intersection */
typedef struct  spmt_inter_s
{
    /* U(i=0:n) [ I[2*i]..I[2*i+1] [ */
    SPMT_PTR_T * intervals;
    SPMT_PTR_T n;
}               spmt_inter_t;


typedef unsigned int (*spmt_dump_t)(const char *, ...);

# define SPMT_FOREACH_CHILD_BEGIN(N, C, D)                  \
    do {                                                    \
        for (int D = SPMT_LEFT ; D < SPMT_N_CHILDREN ; ++D) \
        {                                                   \
            spmt_node_t * C = N->child[D];                  \
            if (C)                                          \
            {
# define SPMT_FOREACH_CHILD_END(N, C, D)                    \
            }                                               \
        }                                                   \
    } while (0)

# ifndef SPMT_DISABLE_LIBSTDC
static inline void
__spmt_to_dot_node(spmt_node_t * node, FILE * f)
{
    const char * color = (node->color == SPMT_BLACK) ? "#000000" : "#FF0000";
    fprintf(f, "    N%p[fontcolor=\"#ffffff\", label=\"[" SPMT_PTR_T_ID ".." SPMT_PTR_T_ID "[\\n[" SPMT_PTR_T_ID ".." SPMT_PTR_T_ID "[\", style=filled, fillcolor=\"%s\"] ;\n", node, node->I.a, node->I.b, node->includes.I.a, node->includes.I.b, color);
    SPMT_FOREACH_CHILD_BEGIN(node, child, dir)
    {
        __spmt_to_dot_node(child, f);
        fprintf(f, "    N%p->N%p ; \n", node, child);
    }
    SPMT_FOREACH_CHILD_END(node, child, dir);
}

static inline void
__spmt_to_dot(spmt_t * spmt, const char * fpath)
{
    FILE * f = fopen(fpath, "w");
    fprintf(f, "digraph g {\n");
    if (spmt->root != SPMT_NULL)
        __spmt_to_dot_node(spmt->root, f);
    fprintf(f, "}\n");
    fclose(f);
}

/* Write the spmt to the given dot file */
# define SPMT_TO_DOT(T, F)          \
    do {                            \
        __spmt_to_dot(T, F);        \
    } while (0)

static inline void
__spmt_to_pdf(spmt_t * spmt, const char * fpath)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s.dot", fpath);
    SPMT_TO_DOT(spmt, buf);

    snprintf(buf, sizeof(buf), "dot -Tpdf %s.dot > %s.pdf", fpath, fpath);
    if (system(buf) == 1)
        fprintf(stderr, "Could not export '%s' to pdf\n", fpath);
}

# define SPMT_TO_PDF(T, F)      \
    do {                        \
        __spmt_to_pdf(T, F);    \
    } while (0)

# endif /* SPMT_DISABLE_LIBSTDC */

/* Return true if the spmt is empty */
# define SPMT_IS_EMPTY(T) __spmt_is_empty(T)
static inline int
__spmt_is_empty(spmt_t * spmt)
{
    return spmt->root == SPMT_NULL;
}

static inline void
__spmt_foreach_node(
    spmt_t * spmt,
    spmt_node_t * node,
    int (*f)(spmt_t *, spmt_node_t *, void *),
    void * obj,
    int * stop
) {
    if (node == SPMT_NULL || *stop)
        return ;
    f(spmt, node, obj);
    __spmt_foreach_node(spmt, node->left,  f, obj, stop);
    __spmt_foreach_node(spmt, node->right, f, obj, stop);
}

static inline void
__spmt_foreach(
    spmt_t * spmt,
    int (*f)(spmt_t *, spmt_node_t *, void *),
    void * obj
) {
    int stop = 0;
    __spmt_foreach_node(spmt, spmt->root, f, obj, &stop);
}

# define SPMT_FOREACH(T, F, O)      \
    do {                            \
        __spmt_foreach(T, F, O);    \
    } while(0)


/* Initialize a new spmt */
# define SPMT_INITIALIZE(T)     \
    do {                        \
        (T)->root = SPMT_NULL;  \
    } while (0)
# define SPMT_INITIALIZE_STATIC { SPMT_NULL }

/* Free the SPMT */
# define SPMT_RELEASE(DST)              \
    do {                                \
        __spmt_release((DST)->root);    \
        (DST)->root = SPMT_NULL;        \
    } while (0)

static inline void
__spmt_release(spmt_node_t * node)
{
    if (node == SPMT_NULL)
        return ;
    __spmt_release(node->left);
    __spmt_release(node->right);
    SPMT_F_FREE_NODE(node);
}

/* Insert node 'N' in the spmt 'T' and set 'M' to '1' if the node was merged */
# define SPMT_FILL(T, B, E)         \
    do {                            \
        __spmt_fill((T), (B), (E)); \
    } while(0)

static inline int
__spmt_log2(int n)
{
    return 31 - __builtin_clz(n);
}

static inline int
__spmt_twopow(int n)
{
    return (1 << n);
}

static inline int
__spmt_height(spmt_node_t * node)
{
    if (node == SPMT_NULL)
        return 0;
    int l = __spmt_height(node->left);
    int r = __spmt_height(node->right);
    return 1 + SPMT_MAX(l, r);
}

static inline int
__spmt_size(spmt_node_t * node)
{
    if (node == SPMT_NULL)
        return 0;
    int l = __spmt_size(node->left);
    int r = __spmt_size(node->right);
    return 1 + l + r;
}

static inline int
__spmt_intervals_intersect_or_are_succesive(interval_t * I, interval_t * J)
{
    return (I->a <= J->b && I->b >= J->a);
}

static inline int
__spmt_nodes_intersect_or_are_succesive(spmt_node_t * x, spmt_node_t * y)
{
    return __spmt_intervals_intersect_or_are_succesive(&(x->I), &(y->I));
}

static inline int
__spmt_intervals_intersect(interval_t * I, interval_t * J)
{
    return (I->a < J->b && I->b > J->a);
}

static inline int
__spmt_nodes_intersect(spmt_node_t * x, spmt_node_t * y)
{
    return __spmt_intervals_intersect(&(x->I), &(y->I));
}

# ifndef NDEBUG

static inline void
__spmt_coherency_includes(spmt_t * spmt, spmt_node_t * node)
{
    if (node == SPMT_NULL)
        return ;
    __spmt_coherency_includes(spmt, node->left);
    __spmt_coherency_includes(spmt, node->right);
    SPMT_PTR_T a = node->left  ? node->left->includes.I.a  : node->I.a;
    SPMT_PTR_T b = node->right ? node->right->includes.I.b : node->I.b;
    SPMT_F_ASSERT(a == node->includes.I.a && b == node->includes.I.b);
}

static inline void
__spmt_coherency_are_succesive(spmt_t * spmt)
{
    int height    = __spmt_height(spmt->root);
    int nelements = __spmt_size(spmt->root);
    int ideal_height = __spmt_log2(nelements + 1);
    SPMT_F_ASSERT(height <= 2 * ideal_height);
}

static inline void
__spmt_coherency_balance(spmt_t * spmt)
{
    int height    = __spmt_height(spmt->root);
    int nelements = __spmt_size(spmt->root);
    int ideal_height = __spmt_log2(nelements + 1);
    SPMT_F_ASSERT(height <= 2 * ideal_height);
}

static inline void
__spmt_coherency_color(spmt_node_t * node)
{
    if (!node)
        return ;

    SPMT_F_ASSERT(node->color == SPMT_BLACK || node->color == SPMT_RED);
    if (node->color == SPMT_RED)
    {
        SPMT_F_ASSERT(!node->left  || node->left->color  == SPMT_BLACK);
        SPMT_F_ASSERT(!node->right || node->right->color == SPMT_BLACK);
    }
    __spmt_coherency_color(node->left);
    __spmt_coherency_color(node->right);
}

static inline int
__spmt_coherency_black_height(spmt_node_t * node)
{
    if (node == SPMT_NULL)
        return 1;

    int left_child_height  = __spmt_coherency_black_height(node->left);
    int right_child_height = __spmt_coherency_black_height(node->right);
    SPMT_F_ASSERT(left_child_height == right_child_height);

    spmt_color_t color = (node->color == SPMT_BLACK) ? 1 : 0;
    return color + left_child_height;
}

static inline int
__spmt_coherency_interval_disjoint_nodes(spmt_t * spmt, spmt_node_t * x, void * obj)
{
    (void) spmt;
    spmt_node_t * y = (spmt_node_t *) obj;
    SPMT_F_ASSERT(x == y || !__spmt_nodes_intersect(x, y));
    // SPMT_F_ASSERT(x == y || !__spmt_nodes_intersect_or_are_succesive(x, y));
    return 0;
}

static inline int
__spmt_coherency_interval_disjoint_foreach(spmt_t * spmt, spmt_node_t * node, void * obj)
{
    (void) obj;
    __spmt_foreach(spmt, __spmt_coherency_interval_disjoint_nodes, node);
    return 0;
}

static inline void
__spmt_coherency_interval_disjoint(spmt_t * spmt)
{
    __spmt_foreach(spmt, __spmt_coherency_interval_disjoint_foreach, SPMT_NULL);
}

static inline void
__spmt_coherency(spmt_t * spmt)
{
    if (spmt->root)
    {
        SPMT_F_ASSERT(spmt->root->color == SPMT_BLACK);
        __spmt_coherency_color(spmt->root);
        __spmt_coherency_includes(spmt, spmt->root);
        __spmt_coherency_black_height(spmt->root);
        __spmt_coherency_interval_disjoint(spmt);
    }
    __spmt_coherency_balance(spmt);
}

# endif /* NDEBUG */

static inline spmt_node_t *
__spmt_node_new(spmt_color_t color, SPMT_PTR_T a, SPMT_PTR_T b)
{
    spmt_node_t * node = SPMT_F_ALLOC_NODE();
    node->I.a          = a;
    node->I.b          = b;
    node->includes.I.a = a;
    node->includes.I.b = b;
    node->left         = SPMT_NULL;
    node->right        = SPMT_NULL;
    node->parent       = SPMT_NULL;
    node->color        = color;
    return node;
}

///////////////
// ROTATIONS //
///////////////

static inline int
__spmt_child_direction(spmt_node_t * node)
{
    return (node == node->parent->left) ? SPMT_LEFT : SPMT_RIGHT;
}

static inline spmt_node_t *
__spmt_get_sibling(spmt_node_t * node)
{
    SPMT_F_ASSERT(node->parent);
    const spmt_direction_t dir = __spmt_child_direction(node);
    return node->parent->child[1-dir];
}

static inline int
__spmt_node_is_black(spmt_node_t * node)
{
    return node == SPMT_NULL || node->color == SPMT_BLACK;
}

static inline int
__spmt_values_are_succesive(SPMT_PTR_T a, SPMT_PTR_T b, SPMT_PTR_T aa, SPMT_PTR_T bb)
{
    return a == bb || b == aa;
}

static inline int
__spmt_intervals_are_succesive(interval_t * I, interval_t * J)
{
    return __spmt_values_are_succesive(I->a, I->b, J->a, J->b);
}

static inline int
__spmt_nodes_are_succesive(spmt_node_t * x, spmt_node_t * y)
{
    return __spmt_intervals_are_succesive(&(x->I), &(y->I));
}

static inline void
__spmt_includes_fixup_interval(spmt_node_t * node)
{
    node->includes.I.a = node->left  ? node->left->includes.I.a  : node->I.a;
    node->includes.I.b = node->right ? node->right->includes.I.b : node->I.b;
}

static inline void
__spmt_includes_fixup_node(spmt_node_t * node)
{
    __spmt_includes_fixup_interval(node);
}

/**
 *      C              A
 *     / \            / \
 *    A   E    <-    B   C
 *   / \                / \
 *  B   D              D   E
 */
static inline void
__spmt_rotate_left(spmt_t * spmt, spmt_node_t * A)
{
//  spmt_node_t * B = A->left;
    spmt_node_t * C = A->right;
    spmt_node_t * D = C->left;
//  spmt_node_t * E = C->right;

    C->left  = A;
 // C->right = E;
 // A->left  = B;
    A->right = D;

    C->parent = A->parent;
    if (A->parent == SPMT_NULL)
        spmt->root = C;
    else if (A->parent->left == A)
        A->parent->left = C;
    else
        A->parent->right = C;

 // B->parent = A;
    A->parent = C;
    if (D)
        D->parent = A;
 // E->parent = C;

    __spmt_includes_fixup_node(A);
 // __spmt_includes_fixup_node(B);
    __spmt_includes_fixup_node(C);
 // __spmt_includes_fixup_node(D);
 // __spmt_includes_fixup_node(E);
}

/**
 *      A              B
 *     / \            / \
 *    B   C    ->    D   A
 *   / \                / \
 *  D   E              E   C
 */
static inline void
__spmt_rotate_right(spmt_t * spmt, spmt_node_t * A)
{
    spmt_node_t * B = A->left;
 // spmt_node_t * C = A->right;
 // spmt_node_t * D = B->left;
    spmt_node_t * E = B->right;

 // B->left  = D;
    B->right = A;
    A->left  = E;
 // A->right = C;

    B->parent = A->parent;
    if (A->parent == SPMT_NULL)
        spmt->root = B;
    else if (A->parent->left == A)
        A->parent->left = B;
    else
        A->parent->right = B;

    if (E)
        E->parent = A;
 // C->parent = A;
    A->parent = B;
 // D->parent = B;

    __spmt_includes_fixup_node(A);
    __spmt_includes_fixup_node(B);
 // __spmt_includes_fixup_node(C);
 // __spmt_includes_fixup_node(D);
 // __spmt_includes_fixup_node(E);
}

static inline void
__spmt_rotate_dir(spmt_t * T, spmt_node_t * P, spmt_direction_t dir)
{
    if (dir == SPMT_LEFT)
        __spmt_rotate_left(T, P);
    else
        __spmt_rotate_right(T, P);
}

// fixup after insertion
static inline void
__spmt_balance_fixup(spmt_t * spmt, spmt_node_t * node)
{
    spmt_node_t * z = node;
    while (z->parent && z->parent->color == SPMT_RED)
    {
        if (z->parent == z->parent->parent->left)
        {
            spmt_node_t * y = z->parent->parent->right;
            if (y && y->color == SPMT_RED)
            {
                z->parent->color = SPMT_BLACK;
                y->color = SPMT_BLACK;
                z->parent->parent->color = SPMT_RED;
                z = z->parent->parent;
            }
            else
            {
                if (z == z->parent->right)
                {
                    z = z->parent;
                    __spmt_rotate_left(spmt, z);
                }
                z->parent->color = SPMT_BLACK;
                z->parent->parent->color = SPMT_RED;
                __spmt_rotate_right(spmt, z->parent->parent);
            }
        }
        else
        {
            spmt_node_t * y = z->parent->parent->left;

            if (y && y->color == SPMT_RED)
            {
                z->parent->color = SPMT_BLACK;
                y->color = SPMT_BLACK;
                z->parent->parent->color = SPMT_RED;
                z = z->parent->parent;
            }
            else
            {
                if (z == z->parent->left)
                {
                    z = z->parent;
                    __spmt_rotate_right(spmt, z);
                }
                z->parent->color = SPMT_BLACK;
                z->parent->parent->color = SPMT_RED;
                __spmt_rotate_left(spmt, z->parent->parent);
            }
        }
    }
    spmt->root->color = SPMT_BLACK;
}

static inline void
__spmt_delete_node_fixup(spmt_t * spmt, spmt_node_t * node)
{
    if (node == spmt->root)
    {
        node->color = SPMT_BLACK;
        return ;
    }
    SPMT_F_ASSERT(node->parent);

    spmt_node_t * sibling = __spmt_get_sibling(node);

    if (sibling->color == SPMT_RED)
    {
        sibling->color = SPMT_BLACK;
        sibling->parent->color = SPMT_RED;
        __spmt_rotate_dir(spmt, node->parent, __spmt_child_direction(node));

        sibling = __spmt_get_sibling(node);
    }

    if (__spmt_node_is_black(sibling->left) && __spmt_node_is_black(sibling->right))
    {
        sibling->color = SPMT_RED;

        if (node->parent->color == SPMT_RED)
            node->parent->color = SPMT_BLACK;
        else
            __spmt_delete_node_fixup(spmt, node->parent);
    }
    else
    {
          spmt_direction_t dir = __spmt_child_direction(node);

          if (dir == SPMT_LEFT && __spmt_node_is_black(sibling->right))
          {
              sibling->left->color = SPMT_BLACK;
              sibling->color = SPMT_RED;
              __spmt_rotate_right(spmt, sibling);
              sibling = node->parent->right;
          }
          else if (dir == SPMT_RIGHT && __spmt_node_is_black(sibling->left))
          {
              sibling->right->color = SPMT_BLACK;
              sibling->color = SPMT_RED;
              __spmt_rotate_left(spmt, sibling);
              sibling = node->parent->left;
          }

          sibling->color = node->parent->color;
          node->parent->color = SPMT_BLACK;
          if (dir == SPMT_LEFT)
          {
              sibling->right->color = SPMT_BLACK;
                __spmt_rotate_left(spmt, node->parent);
          }
          else
          {
              sibling->left->color = SPMT_BLACK;
              __spmt_rotate_right(spmt, node->parent);
          }
    }

    SPMT_F_ASSERT(node->parent);
}

static inline void
__spmt_transplant(spmt_t  * spmt, spmt_node_t * u, spmt_node_t * v)
{
    if (u->parent == SPMT_NULL)
        spmt->root = v;
    else
    {
        if (u == u->parent->left)
            u->parent->left = v;
        else
            u->parent->right = v;
    }

    if (v)
        v->parent = u->parent;

    // TODO : this can probably be avoided most of the time
    while ((u = u->parent) != SPMT_NULL)
        __spmt_includes_fixup_node(u);
}

static inline spmt_node_t *
__spmt_inorder_predecessor(spmt_node_t * z)
{
    spmt_node_t * node = z->left;
    while (node->right)
        node = node->right;
    return node;
}

static inline spmt_node_t *
__spmt_inorder_successor(spmt_node_t * z)
{
    spmt_node_t * node = z->right;
    while (node->left)
        node = node->left;
    return node;
}

static inline void
__spmt_delete_node(spmt_t * spmt, spmt_node_t * z)
{
    // When the deleted node has 2 child (non-NIL), then we can swap its
    // value with its in-order successor (the leftmost child of the right
    // subtree), and then delete the successor instead.
    if (z->left != SPMT_NULL && z->right != SPMT_NULL)
    {
        spmt_node_t * node = __spmt_inorder_successor(z);
        z->I.a = node->I.a;
        z->I.b = node->I.b;
        return __spmt_delete_node(spmt, node);
    }
    // When the deleted node has only 1 child (non-SPMT_NULL). In this case, just
    // replace the node with its child, and color it black.  The single child
    // (non-SPMT_NULL) must be red
    else if (z->left != SPMT_NULL && z->right == SPMT_NULL)
    {
        SPMT_F_ASSERT(z->color == SPMT_BLACK);
        SPMT_F_ASSERT(z->left->color == SPMT_RED);
        z->left->color = SPMT_BLACK;
        __spmt_transplant(spmt, z, z->left);
    }
    else if (z->left == SPMT_NULL && z->right != SPMT_NULL)
    {
        SPMT_F_ASSERT(z->right->color == SPMT_RED);
        z->right->color = SPMT_BLACK;
        __spmt_transplant(spmt, z, z->right);
    }
    // When the deleted node has no child (both SPMT_NULL)
    else
    {
        // and is the root, replace it with SPMT_NULL. The tree is empty.
        if (z == spmt->root)
        {
            spmt->root = SPMT_NULL;
        }
        // and is red, simply remove the leaf node.
        else if (z->color == SPMT_RED)
        {
            __spmt_transplant(spmt, z, SPMT_NULL);
        }
        // and is black, deleting it will create an imbalance
        else
        {
            SPMT_F_ASSERT(z->parent != SPMT_NULL);
            SPMT_F_ASSERT(z->color == SPMT_BLACK);
            SPMT_F_ASSERT(z->left == SPMT_NULL && z->right == SPMT_NULL);

            __spmt_delete_node_fixup(spmt, z);
            __spmt_transplant(spmt, z, SPMT_NULL);
        }
    }

    SPMT_F_FREE_NODE(z);
}

static inline void
__spmt_merge_nodes_value(spmt_node_t * parent, SPMT_PTR_T a, SPMT_PTR_T b)
{
    parent->I.a = SPMT_MIN(parent->I.a, a);
    parent->I.b = SPMT_MAX(parent->I.b, b);
}

static inline void
__spmt_merge_nodes(spmt_node_t * parent, spmt_node_t * node)
{
    return __spmt_merge_nodes_value(parent, node->I.a, node->I.b);
}

static inline void
__spmt_includes_fixup(spmt_t * spmt, spmt_node_t * node)
{
    __spmt_includes_fixup_node(node);

    spmt_node_t * parent = node->parent;
    while (parent)
    {
        if (__spmt_nodes_are_succesive(parent, node))
        {
            __spmt_merge_nodes(parent, node);
            __spmt_delete_node(spmt, node);

            spmt_node_t * restart = parent;
#if 0
            while (1)
            {
                if (__spmt_nodes_are_succesive(parent, restart))
                    break ;

                if (restart->left && __spmt_intervals_intersect_or_are_succesive(&(parent->I), &(restart->left->includes.I)))
                {
                    restart = restart->left;
                    continue ;
                }

                if (restart->right && __spmt_intervals_intersect_or_are_succesive(&(parent->I), &(restart->right->includes.I)))
                {
                    restart = restart->right;
                    continue ;
                }

                break ;
            }
# endif
            node = restart;
        }
        __spmt_includes_fixup_node(parent);
        parent = parent->parent;
    }
}

static inline void
__spmt_fill_fixup(spmt_t * spmt, spmt_node_t * parent, spmt_direction_t dir, SPMT_PTR_T a, SPMT_PTR_T b)
{
    SPMT_F_ASSERT(parent->child[dir] == SPMT_NULL);

    // quick way out inserting a successive interval
    if (__spmt_values_are_succesive(parent->I.a, parent->I.b, a, b))
    {
        __spmt_merge_nodes_value(parent, a, b);
        return __spmt_includes_fixup(spmt, parent);
    }

    spmt_node_t * node = __spmt_node_new(SPMT_RED, a, b);
    parent->child[dir] = node;
    node->parent = parent;

    __spmt_balance_fixup(spmt, node);
    __spmt_includes_fixup(spmt, node);
}

static inline void
__spmt_fill_from(spmt_t * spmt, spmt_node_t * parent, SPMT_PTR_T a, SPMT_PTR_T b)
{
    if (a >= b)
        return ;

    while (1)
    {
        // TODO if the following condition is met, we can fastly merge all the subtree
        //  if (a <= parent->includes.I.a && parent->includes.I.b <= b)
        //      then merge the subtree from parent
        //
        // It would require rebalancing and recoloring the tree to ensure properties
        // However, i do not think there is practical use-case for Taskgrind purposes

        SPMT_PTR_T aa = parent->I.a;
        SPMT_PTR_T bb = parent->I.b;

        // case (1)    J << I
        if (b <= aa)
        {
            if (parent->left == SPMT_NULL)
            {
                __spmt_fill_fixup(spmt, parent, SPMT_LEFT, a, b);
                break ;
            }
            else
                parent = parent->left;
        }

        // case (2)     J >> I
        else if (a >= bb)
        {
            if (parent->right == SPMT_NULL)
            {
                __spmt_fill_fixup(spmt, parent, SPMT_RIGHT, a, b);
                break ;
            }
            else
                parent = parent->right;
        }

        // case (3)     J c I
        else if (aa <= a && b <= bb)
        {
            // nothing to do
            break ;
        }

        // case (5)     I c J
        else if (a <= aa && bb <= b)
        {
            __spmt_fill_from(spmt, parent,      a, aa);
            __spmt_fill_from(spmt, spmt->root, bb,  b); // 'parent' may have been deleted
            break ;
        }

        // case (6)     J < I
        else if (a <= parent->I.a && b <= parent->I.b)
        {
            __spmt_fill_from(spmt, parent,      a, aa);
            __spmt_fill_from(spmt, spmt->root, aa,  b); // 'parent' may have been deleted
            break ;
        }

        // case (7)     J > I
        else if (parent->I.a <= a && a <= parent->I.b)
        {
            __spmt_fill_from(spmt, parent,     bb, b);
            __spmt_fill_from(spmt, spmt->root,  a, bb); // 'parent' may have been deleted
            break ;
        }
    }
}

static inline void
__spmt_fill(spmt_t * spmt, SPMT_PTR_T a, SPMT_PTR_T b)
{
    if (a >= b)
        return ;

    if (spmt->root == SPMT_NULL)
        spmt->root = __spmt_node_new(SPMT_BLACK, a, b);
    else
        __spmt_fill_from(spmt, spmt->root, a, b);

# ifndef NDEBUG
#  pragma message("Coherency tests are set. Use -DNDEBUG if you need performance")
    __spmt_coherency(spmt);
# endif /* NDEBUG */
}

/* Dump the spmt */
# define SPMT_DUMP(F, T)                \
    do {                                \
        __spmt_dump(F, (T)->root, 0);   \
    } while (0)

# define SPMT_DUMP_FILLED(F, T) SPMT_DUMP(F, T)

static inline void
__spmt_dump(spmt_dump_t print, spmt_node_t * parent, int depth)
{
    if (!parent)
        return ;

    print("%*c(%llu, %llu)\n", 2*depth + 1, ' ', parent->I.a, parent->I.b);
    SPMT_FOREACH_CHILD_BEGIN(parent, child, dir)
    {
        __spmt_dump(print, child, depth+1);
    }
    SPMT_FOREACH_CHILD_END(parent, child, dir);
}

/*
 *  Compute the intersection A n B.
 *
 *  Store at most 'n' intersections to the 'intervals' array.
 *  Return the number of intervals stored
 *
 * in: A
 * in: B
 * in: n
 * out: intervals
 * out: i
 */
static inline int
__spmt_intersect_from(interval_t * intervals, int i, int n, spmt_node_t * A, spmt_node_t * B)
{
    if (A == SPMT_NULL || B == SPMT_NULL || i >= n ||
            !__spmt_intervals_intersect(&(A->includes.I), &(B->includes.I)))
        return i;

    if (__spmt_nodes_intersect(A, B))
    {
        intervals[i].a = SPMT_MAX(A->I.a, B->I.a);
        intervals[i].b = SPMT_MIN(A->I.b, B->I.b);
        ++i;
    }

    i = __spmt_intersect_from(intervals, i, n, A->left , B->left );
    i = __spmt_intersect_from(intervals, i, n, A->left , B->right );
    i = __spmt_intersect_from(intervals, i, n, A->right, B->left );
    i = __spmt_intersect_from(intervals, i, n, A->right, B->right );

    return i;
}

static inline int
__spmt_intersect(interval_t * intervals, int n, spmt_t * A, spmt_t * B)
{
    return __spmt_intersect_from(intervals, 0, n, A->root, B->root);
}

# define SPMT_INTERSECT(I, N, A, B) __spmt_intersect(I, N, A, B)

static inline int
__spmt_append_add(spmt_t * SRC, spmt_node_t * node, void * obj)
{
    (void) SRC;
    spmt_t * DST = (spmt_t *) obj;
    SPMT_FILL(DST, node->I.a, node->I.b);
    return 0;
}

static inline void
__spmt_append(spmt_t * DST, spmt_t * SRC)
{
    __spmt_foreach(SRC, __spmt_append_add, DST);
}

# define SPMT_APPEND(DST, SRC)      \
    do {                            \
        __spmt_append(DST, SRC);    \
    } while (0)

static inline void
__spmt_union(spmt_t * DST, spmt_t * A, spmt_t * B)
{
    // TODO can probably implement that better, still O(n.log n) though, good
    // enough for now
    __spmt_append(DST, A);
    __spmt_append(DST, B);
}

# define SPMT_UNION(DST, A, B)              \
    do {                                    \
        if (DST == A)                       \
            __spmt_append(DST, B);          \
        else if (DST == B)                  \
            __spmt_append(DST, A);          \
        else                                \
            __spmt_union(DST, A, B);        \
    } while (0)

#endif /* __SPMT_H__ */
