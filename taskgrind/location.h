#ifndef __LOCATION_H__
# define __LOCATION_H__

# include "malloc_record.h"
# include "task.h"

extern const HChar * UNKNOWN;

HChar * task_seg_get_location(task_seg_t * seg, UInt use_dir, HChar * buffer, UInt len);
HChar * taskgrind_alloc_record_get_location(taskgrind_alloc_record_t * record, UInt use_dir, HChar * buffer, UInt len);
HChar * location_get_from_ip(DiEpoch ep, Addr ip, UInt use_dir, HChar * buffer, UInt len);

#endif /* __LOCATION_H__ */
