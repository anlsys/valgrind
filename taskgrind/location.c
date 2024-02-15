# include "pub_tool_debuginfo.h"
# include "pub_tool_libcfile.h"
# include "pub_tool_libcassert.h"
# include "pub_tool_execontext.h"

# include "location.h"
# include "task.h"

// retrieve a single LoC pointing pointing to the given task seg
const HChar * UNKNOWN = "(unknown)";

static HChar *
task_seg_get_location_from_ip(DiEpoch ep, Addr ip)
{
    const HChar * dir;
    const HChar * file;
    UInt line;
    if (!VG_(get_filename_linenum)(ep, ip, &file, &dir, &line))
        return (HChar *) UNKNOWN;

    dir = NULL; // don't show directory

    HChar * s;
    if (dir)
    {
        // format is: dir/file:line
        //      dir : is known
        //      /   : + 1
        //      file : is known
        //      :   : +1
        //      line : len(str(1 << 32)) == 10
        //      \0  : +1
        UInt len = VG_(strlen)(dir) + 1 + VG_(strlen)(file) + 1 + 10 + 1;
        s = (HChar *) VG_(malloc)("task_seg_get_location_from_ip", len);
        VG_(snprintf)(s, len, "%s%c%s%c%u", dir, '/', file, ':', line);
    }
    else
    {
        // format is: file:line
        //      file : is known
        //      :   : +1
        //      line : len(str(1 << 32)) == 10
        //      \0  : +1
        UInt len = VG_(strlen)(file) + 1 + 10 + 1;
        s = (HChar *) VG_(malloc)("task_seg_get_location_from_ip", len);
        VG_(snprintf)(s, len, "%s%c%u", file, ':', line);
    }

    return s;
}

HChar *
task_seg_get_location(task_seg_t * seg)
{
    ExeContext * ec = seg->ctx;

    if (ec == NULL)
        return (HChar *) UNKNOWN;

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
            if (VG_(strstr)(name, "outline"))
            {
                return task_seg_get_location_from_ip(ep, ips[i]);
            }
        }
    }

    return task_seg_get_location_from_ip(ep, ips[0]);
}
