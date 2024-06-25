# include "pub_tool_errormgr.h"
# include "pub_tool_libcassert.h"

# include "error.h"

Bool
taskgrind_eq_Error(VgRes res, const Error * e1, const Error * e2)
{
    (void) res;
    (void) e1;
    (void) e2;
    return False;
}

void
taskgrind_before_pp_Error(const Error * err)
{
    (void) err;
}

void
taskgrind_pp_Error(const Error * err)
{
    (void) err;
}

UInt
taskgrind_update_extra(const Error * err)
{
    (void) err;
    return 0;
}

Bool
taskgrind_recognised_suppression(const HChar * name, Supp * su)
{
    return 0;
}

Bool taskgrind_read_extra_suppression_info(Int fd, HChar** bufpp, SizeT* nBufp,
                                        Int* lineno, Supp* su)
{
   return True;
}

Bool
taskgrind_error_matches_suppression(const Error * err, const Supp * su)
{
    return False;
}

const HChar *
taskgrind_get_error_name(const Error * err)
{
    return "";
}

SizeT
taskgrind_get_extra_suppression_info(const Error * err, HChar * buf, Int nBuf)
{
   tl_assert(nBuf >= 1);
   /* Do nothing */
   buf[0] = '\0';
   return 0;
}

SizeT
taskgrind_print_extra_suppression_use(const Supp * su, HChar * buf, Int nBuf)
{
   tl_assert(nBuf >= 1);
   /* Do nothing */
   buf[0] = '\0';
   return 0;
}

void
taskgrind_update_extra_suppression_use(const Error * err, const Supp * su)
{
}
