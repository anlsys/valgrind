# include "print.h"
# include "task.h"

# include "pub_tool_libcfile.h"
# include "pub_tool_libcassert.h"

///////////////////////////////////////////////////////////////////////////////
//  Export graph to dot file
///////////////////////////////////////////////////////////////////////////////

static
void dot_file_err(void)
{
    TASKGRIND_ERR("Cannot output to file");
    VG_(exit)(1);
}

// dump a task
static inline void
dump_task(VgFile * fp, task_t * task)
{
    const char * type, * shape;

    switch (task->type)
    {
        case (TASK_TYPE_IMPLICIT):
        {
            type = "imp(?)";
            shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_ROOT):
        {
            type = "imp(root)";
            shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_BARRIER):
        {
            type = "imp(barrier)";
            shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_OUTSET):
        {
            type = "imp(outset)";
            shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_UNKNOWN):
        {
            type = "imp(unknown)";
            shape = "diamond";
            break ;
        }


        case (TASK_TYPE_EXPLICIT):
        {
            type = "explicit";
            shape = "circle";
            break ;
        }

        case (TASK_TYPE_UNKNOWN):
        default:
        {
            type = "unknown";
            shape = "hexagon";
            TASKGRIND_ERR("Unknown task type %u", task->type);
            tl_assert(0);
            break ;
        }
    }
    VG_(fprintf)(fp, "    \"%p\" [label=\"type=%s\\nclient=%ld\nchild=%ld\",shape=%s];\n",
        task, type, (Word)task->client_id, (Word)task->child_id, shape);

}

// TDG
static void
dump_access_tdg(VgFile * fp, task_t * pred)
{
    dump_task(fp, pred);

    int i;
    for (i = 0 ; i < pred->access_successors.n ; ++i)
    {
        task_t * succ = pred->access_successors.tasks[i];
        dump_access_tdg(fp, succ);
        VG_(fprintf)(fp, "    \"%p\" -> \"%p\" ;\n", pred, succ);
    }
}

void
taskgrind_export_access_tdg(task_t * task)
{
    HChar * filename = (HChar *) VG_(malloc)("taskgrind_export_access_tdg", sizeof(UChar) * 256);
    VG_(snprintf)(filename, 256, "tdg-%p.dot", task);

    TASKGRIND_INFO("Exporting %s", filename);

    VgFile * fp = VG_(fopen)(filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
    if (fp == NULL) {
        fp = VG_(fopen)(filename, VKI_O_CREAT|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
        if (fp == NULL)
            dot_file_err();
    }
    VG_(free)(filename);

    VG_(fprintf)(fp, "digraph G {\n");
    dump_access_tdg(fp, task);
    VG_(fprintf)(fp, "}\n");

    VG_(fclose)(fp);

}

// TCFG
static void
dump_tcfg(VgFile * fp, task_t * parent)
{
    // TODO: debug remove me
    if (parent->access_successors.n)
        taskgrind_export_access_tdg(parent);

    dump_task(fp, parent);

    int i;
    for (i = 0 ; i < parent->children.n ; ++i)
    {
        task_t * child = parent->children.tasks[i];
        dump_tcfg(fp, child);
        VG_(fprintf)(fp, "    \"%p\" -> \"%p\" ;\n", parent, child);
    }
}

void
taskgrind_export_tcfg(task_t * task)
{
    const HChar * filename = "tcfg.dot";

    TASKGRIND_INFO("Exporting %s", filename);

    VgFile * fp = VG_(fopen)(filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
    if (fp == NULL) {
        fp = VG_(fopen)(filename, VKI_O_CREAT|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
        if (fp == NULL)
            dot_file_err();
    }

    VG_(fprintf)(fp, "digraph G {\n");
    dump_tcfg(fp, task);
    VG_(fprintf)(fp, "}\n");

    VG_(fclose)(fp);
}
