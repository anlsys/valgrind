#ifndef __ERROR_H__
# define __ERROR_H__

# include "pub_tool_errormgr.h"

Bool taskgrind_eq_Error(VgRes res, const Error* e1, const Error* e2);
Bool taskgrind_eq_Error(VgRes res, const Error * e1, const Error * e2);
void taskgrind_before_pp_Error(const Error * err);
void taskgrind_pp_Error(const Error * err);
UInt taskgrind_update_extra(const Error * err);
Bool taskgrind_recognised_suppression(const HChar * name, Supp * su);
Bool taskgrind_read_extra_suppression_info(Int fd, HChar ** bufpp, SizeT * nBufp, Int * lineno, Supp * su);
Bool taskgrind_error_matches_suppression(const Error * err, const Supp * su);
const HChar * taskgrind_get_error_name(const Error * err);
SizeT taskgrind_get_extra_suppression_info(const Error * err, HChar * buf, Int nBuf);
SizeT taskgrind_print_extra_suppression_use(const Supp * su, HChar * buf, Int nBuf);
void taskgrind_update_extra_suppression_use(const Error * err, const Supp * su);

#endif /* __ERROR_H__ */
