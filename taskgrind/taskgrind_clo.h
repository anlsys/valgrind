#ifndef __TASKGRIND_CLO_H__
# define __TASKGRIND_CLO_H__

typedef struct  taskgrind_clo_s
{
    // dump internal data structure to dot files at the end of execution
    int dump;
}               taskgrind_clo_t;

extern taskgrind_clo_t CLOS;

#endif /* __TASKGRIND_CLO_H__ */
