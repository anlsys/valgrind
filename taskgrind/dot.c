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

// get the type string and label for a given task
static inline void
get_task_infos(task_t * task, const char ** type, const char ** shape)
{
    switch (task->type)
    {
        case (TASK_TYPE_IMPLICIT):
        {
            *type = "imp(?)";
            *shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_ROOT):
        {
            *type = "imp(root)";
            *shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_BARRIER):
        {
            *type = "imp(barrier)";
            *shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_OUTSET):
        {
            *type = "imp(outset)";
            *shape = "diamond";
            break ;
        }

        case (TASK_TYPE_IMPLICIT_UNKNOWN):
        {
            *type = "imp(unknown)";
            *shape = "diamond";
            break ;
        }

        case (TASK_TYPE_EXPLICIT):
        {
            *type = "explicit";
            *shape = "circle";
            break ;
        }

        case (TASK_TYPE_UNKNOWN):
        default:
        {
            *type = "unknown";
            *shape = "hexagon";
            TASKGRIND_ERR("Unknown task type %u", task->type);
            tl_assert(0);
            break ;
        }
    }
}

// dump a task
static inline void
dump_task(VgFile * fp, task_t * task)
{
    const char * type, * shape;
    get_task_infos(task, &type, &shape);

    VG_(fprintf)(fp, "    \"%p\" [label=\"type=%s\\nclient=%ld\\nchild=%ld\",shape=%s];\n",
        task, type, (Word)task->client_id, (Word)task->child_id, shape);
}

static void
dump_task_part_ref(VgFile * fp, task_part_ref_t * ref)
{
    const char * type, * shape;
    get_task_infos(ref->task, &type, &shape);

    task_t * task = ref->task;
    task_part_t * part = task->parts.parts + ref->id;
    VG_(fprintf)(fp, "    \"%p\" [label=\"type=%s\\nclient=%ld\\nchild=%ld\\npart=%u\",shape=%s];\n",
        part, type, (Word)task->client_id, (Word)task->child_id, ref->id, shape);
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
    ARRAY_FOREACH_BEGIN(&parent->children, task_t **, child)
        dump_task(fp, *child);
    ARRAY_FOREACH_END(&parent->children, task_t **, child)

    // dump edges
    ARRAY_FOREACH_BEGIN(&parent->children, task_t **, pred)
    {
        ARRAY_FOREACH_BEGIN(&(*pred)->successors, task_t **, succ)
        {
            VG_(fprintf)(fp, "    \"%p\" -> \"%p\" ;\n", pred, succ);
        }
        ARRAY_FOREACH_END(&(*pred)->successors, task_t **, succ)
    }
    ARRAY_FOREACH_END(&parent->children, task_t **, pred)

    VG_(fprintf)(fp, "}\n");
    VG_(fclose)(fp);
}

void
taskgrind_export_tdgx_recursive(task_t * task)
{
    taskgrind_export_tdgx(task);
    ARRAY_FOREACH_BEGIN(&task->children, task_t **, child)
    {
        taskgrind_export_tdgx_recursive(*child);
    }
    ARRAY_FOREACH_END(&parent->children, task_t **, child)
}

// TCFG (task control flow graph)
static void
dump_tcfg(VgFile * fp, task_t * parent)
{
    dump_task(fp, parent);

    ARRAY_FOREACH_BEGIN(&parent->children, task_t **, child)
    {
        dump_tcfg(fp, *child);
        VG_(fprintf)(fp, "    \"%p\" -> \"%p\" ;\n", parent, *child);
    }
    ARRAY_FOREACH_END(&parent->children, task_t **, child)
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

// logically parallel graph
static void
dump_lpg(VgFile * fp, task_part_ref_t * pred_ref)
{
    dump_task_part_ref(fp, pred_ref);

    task_part_t * pred = pred_ref->task->parts.parts + pred_ref->id;
    ARRAY_FOREACH_BEGIN(&pred->successors, task_part_ref_t *, succ_ref)
    {
        dump_lpg(fp, succ_ref);

        task_part_t * succ = succ_ref->task->parts.parts + succ_ref->id;
        VG_(fprintf)(fp, "    \"%p\" -> \"%p\" ;\n", pred, succ);
    }
    ARRAY_FOREACH_END(&pred->successors, task_part_ref_t *, succ_ref)
}

void
taskgrind_export_lpg(task_part_ref_t * root)
{
    VgFile * fp = create_file("lpg.dot");

    VG_(fprintf)(fp, "digraph G {\n");
    dump_lpg(fp, root);
    VG_(fprintf)(fp, "}\n");

    VG_(fclose)(fp);

}
