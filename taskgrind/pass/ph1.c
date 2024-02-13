# include "print.h"
# include "task.h"
# include "taskgrind_spmt.h"

// TODO: analysis code bellow is experimental and temporary

static inline void
__analyze_useless_dependencies_between(task_t * pred, task_t * succ)
{
    #if 0
    # if 0
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p", (void *) pred->client_id);
    TASKGRIND_INFO("--------------------");
    SPMT_DUMP_FILLED(VG_(umsg), &pred->stores);
    TASKGRIND_INFO("--------------------");
    TASKGRIND_INFO("Task %p", (void *) succ->client_id);
    TASKGRIND_INFO("--------------------");
    SPMT_DUMP_FILLED(VG_(umsg), &succ->stores);
    #endif

    spmt_t inter;
    SPMT_INTERSECT(&inter, &pred->stores, &succ->stores);

    if (SPMT_IS_EMPTY(&inter))
    {
        TASKGRIND_WARN("  %p and %p were declared dependent having no data dependencies",
                (void *)pred->client_id,
                (void *)succ->client_id);
    }
    else
    {
        TASKGRIND_WARN("  %p and %p were declared dependent having data dependencies",
                (void *)pred->client_id,
                (void *)succ->client_id);
    }

    # if 0
    TASKGRIND_INFO("--------------------------------");
    TASKGRIND_INFO("Intersect tasks %p n %p", (void*)pred->client_id, (void*)succ->client_id);
    TASKGRIND_INFO("------------------------------");
    SPMT_DUMP_FILLED(VG_(umsg), &inter);
    #endif

    SPMT_RELEASE(&inter);
    #endif
}

void
taskgrind_pass_ph1(task_t * parent)
{
    #if 0
    if (parent->children.n == 0)
        return ;

    for (int i = 0 ; i < parent->children.n ; ++i)
    {
        task_t * pred = parent->children.tasks[i];
        if (pred->type >= TASK_TYPE_IMPLICIT)
            continue ;

        for (int j = 0 ; j < pred->successors.n ; ++j)
        {
            task_t * succ = pred->successors.tasks[j];

            // outset tasks are 'empty' and ensure control-flow dependency, not data dependency
            // data dependency are between their predecessors and successors
            if (succ->type == TASK_TYPE_IMPLICIT_OUTSET)
                for (int k = 0 ; k < succ->successors.n ; ++k)
                    __analyze_useless_dependencies_between(pred, succ->successors.tasks[k]);
            else
                __analyze_useless_dependencies_between(pred, succ);
        }
    }

    for (int i = 0 ; i < parent->children.n ; ++i)
        taskgrind_pass_ph1(parent->children.tasks[i]);

    #endif
}
