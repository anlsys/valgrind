# include "pub_tool_debuginfo.h"
# include "pub_tool_libcfile.h"
# include "pub_tool_libcassert.h"
# include "pub_tool_execontext.h"

# include "location.h"
# include "task.h"

// retrieve a single LoC pointing pointing to the given task seg
const HChar * UNKNOWN = "(unknown)";

static HChar *
__unknown_location(HChar * buffer, UInt len)
{
    if (buffer == NULL)
        return (HChar *) VG_(strdup)("__unknown_location", UNKNOWN);
    else
    {
        VG_(strncpy)(buffer, UNKNOWN, len);
        return buffer;
    }
}

static HChar *
task_seg_get_location_from_ip(DiEpoch ep, Addr ip, UInt use_dir, HChar * buffer, UInt len)
{
    const HChar * dir;
    const HChar * file;
    UInt line;
    if (!VG_(get_filename_linenum)(ep, ip, &file, &dir, &line))
        return NULL;

    if (buffer == NULL)
    {
        if (use_dir)
        {
            // format is: dir/file:line
            //      dir : is known
            //      /   : + 1
            //      file : is known
            //      :   : +1
            //      line : len(str(1 << 32)) == 10
            //      \0  : +1
            len = VG_(strlen)(dir) + 1 + VG_(strlen)(file) + 1 + 10 + 1;
        }
        else
        {
            // format is: file:line
            //      file : is known
            //      :   : +1
            //      line : len(str(1 << 32)) == 10
            //      \0  : +1
            len = VG_(strlen)(file) + 1 + 10 + 1;
        }
        buffer = (HChar *) VG_(malloc)("task_seg_get_location_from_ip", len);
    }


    if (use_dir)
        VG_(snprintf)(buffer, len, "%s%c%s%c%u", dir, '/', file, ':', line);
    else
        VG_(snprintf)(buffer, len, "%s%c%u", file, ':', line);

    return buffer;
}

//  seg - the task segments
//  use_dir - append file directory to path
//  buffer - buffer to copy (allocated on the heap if NULL)
//  len - buffer len (ignored if buffer is NULL)
HChar *
task_seg_get_location(task_seg_t * seg, UInt use_dir, HChar * buffer, UInt len)
{
    ExeContext * ec = seg->ctx;

    if (ec == NULL)
        return __unknown_location(buffer, len);

    DiEpoch ep = VG_(get_ExeContext_epoch)(ec);
    Int n_ips = VG_(get_ExeContext_n_ips)(ec);
    Addr * ips = VG_(get_ExeContext_ips)(ec);

    Int i;
    for (i = 0 ; i < n_ips ; ++i)
    {
        Addr ip = ips[i];

        const HChar * name;
        if (VG_(get_fnname)(ep, ip, &name))
        {
            // detect LLVM outlined sections
            if (VG_(strstr)(name, "omp_task_entry"))
            {
                HChar * r = task_seg_get_location_from_ip(ep, ips[i], use_dir, buffer, len);
                if (r)
                    return r;
            }
        }
    }

    HChar * r = task_seg_get_location_from_ip(ep, ips[0], use_dir, buffer, len);
    return r ? r : __unknown_location(buffer, len);
}

HChar *
taskgrind_alloc_record_get_location(
    taskgrind_alloc_record_t * record,
    HChar * buffer,
    UInt len
) {
    if (record == NULL)
        return __unknown_location(buffer, len);
    // TODO
    return NULL;
}
