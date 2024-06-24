#ifndef __DOT_H__
# define __DOT_H__

# include "task.h"

void taskgrind_export_tcfg(task_t * task);

void taskgrind_export_tdgx(task_t * task);
void taskgrind_export_tdgx_recursive(task_t * task);

void taskgrind_export_lpg(task_t * seg);

#endif /* __DOT_H__ */
