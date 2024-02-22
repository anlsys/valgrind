#ifndef __LOCATION_H__
# define __LOCATION_H__

# include "task.h"

extern const HChar * UNKNOWN;

HChar * task_seg_get_location(task_seg_t * seg, UInt use_dir, HChar * buffer, UInt len);

#endif /* __LOCATION_H__ */
