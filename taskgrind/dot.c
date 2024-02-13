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

static VgFile *
create_file(const HChar * filename)
{
    TASKGRIND_INFO("Creating %s", filename);

    VgFile * fp = VG_(fopen)(filename, VKI_O_WRONLY|VKI_O_TRUNC, 0);
    if (fp == NULL) {
        fp = VG_(fopen)(filename, VKI_O_CREAT|VKI_O_WRONLY, VKI_S_IRUSR|VKI_S_IWUSR);
        if (fp == NULL)
            dot_file_err();
    }
    return fp;
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

static void
dump_task_part(VgFile * fp, task_part_t * part)
{
    dump_task(fp, part->task);
}

// TDG (task dependency graph)
void
taskgrind_export_tdgx(task_t * parent)
{
    if (parent->children.n == 0)
        return ;

    HChar * filename = (HChar *) VG_(malloc)("taskgrind_export_tdgx", sizeof(UChar) * 256);
    VG_(snprintf)(filename, 256, "tdgx-%p.dot", parent);

    VgFile * fp = create_file(filename);

    VG_(free)(filename);

    VG_(fprintf)(fp, "digraph G {\n");

    // dump tasks
    for (int i = 0 ; i < parent->children.n ; ++i)
        dump_task(fp, parent->children.tasks[i]);

    // dump edges
    for (int i = 0 ; i < parent->children.n ; ++i)
    {
        task_t * pred = parent->children.tasks[i];
        for (int j = 0 ; j < pred->successors.n ; ++j)
        {
            task_t * succ = pred->successors.tasks[j];
            VG_(fprintf)(fp, "    \"%p\" -> \"%p\" ;\n", pred, succ);
        }
    }

    VG_(fprintf)(fp, "}\n");
    VG_(fclose)(fp);
}

void
taskgrind_export_tdgx_recursive(task_t * task)
{
    taskgrind_export_tdgx(task);
    for (int i = 0 ; i < task->children.n ; ++i)
        taskgrind_export_tdgx_recursive(task->children.tasks[i]);
}

// TCFG (task control flow graph)
static void
dump_tcfg(VgFile * fp, task_t * parent)
{
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
    VgFile * fp = create_file("tcfg.dot");

    VG_(fprintf)(fp, "digraph G {\n");
    dump_tcfg(fp, task);
    VG_(fprintf)(fp, "}\n");

    VG_(fclose)(fp);
}

// TDXF (task dependnecy graph flatten)
static void
dump_tdgxf(VgFile * fp, task_part_t * pred)
{
    dump_task_part(fp, pred);

    int i;
    for (i = 0 ; i < pred->successors.n ; ++i)
    {
        task_part_t * succ = pred->successors.parts + i;
        dump_tdgxf(fp, succ);
        VG_(fprintf)(fp, "    \"%p\" -> \"%p\" ;\n", pred, succ);
    }
}

void
taskgrind_export_tdgxf(task_part_t * part)
{
    VgFile * fp = create_file("tdgxf.dot");

    VG_(fprintf)(fp, "digraph G {\n");
    dump_tdgxf(fp, part);
    VG_(fprintf)(fp, "}\n");

    VG_(fclose)(fp);

}
