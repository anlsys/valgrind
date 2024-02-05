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

// TDG
static void
dump_access_tdg(VgFile * fp, task_t * pred)
{
    int i;
    for (i = 0 ; i < pred->access_successors.n ; ++i)
    {
        task_t * succ = pred->access_successors.tasks[i];
        VG_(fprintf)(fp, "    %lu -> %lu ;\n", pred->child_id, succ->child_id);
        dump_access_tdg(fp, succ);
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
    taskgrind_export_access_tdg(parent);

    int i;
    for (i = 0 ; i < parent->children.n ; ++i)
    {
        task_t * child = parent->children.tasks[i];
        if (child->client_id != -1)
            VG_(fprintf)(fp, "    %lu -> %lu ;\n", parent->client_id, child->client_id);
        dump_tcfg(fp, child);
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
