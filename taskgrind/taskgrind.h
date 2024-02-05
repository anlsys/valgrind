// TODO: header

#ifndef TASKGRIND_API_H
# define TASKGRIND_API_H

# include "valgrind.h"

// RESERVED ids that client cannot use for TASKGRIND tasks
# define TASKGRIND_CLIENT_ID_NULL       ((UWord)-1)
# define TASKGRIND_CLIENT_ID_PRIVATE    ((UWord)-2)

// task types
typedef enum    taskgrind_task_type_e
{
    TASKGRIND_TASK_TYPE_EXPLICIT,
    TASKGRIND_TASK_TYPE_IMPLICIT,
}               taskgrind_task_type_t;

// Taskgrind accesses, similar to StarSs / StarPU / OpenMP
typedef enum    taskgrind_access_t
{
    TASKGRIND_IN,
    TASKGRIND_OUT,
    TASKGRIND_OUTSET,
}               taskgrind_access_t;

// valgrind client requests
typedef enum    taskgrind_client_request_t
{
    VG_USERREQ__TASKGRIND_CREATE_EVENT      = VG_USERREQ_TOOL_BASE('T', 'G'),
    VG_USERREQ__TASKGRIND_SCHEDULE_EVENT,
    VG_USERREQ__TASKGRIND_ACCESS_EVENT,
    VG_USERREQ__TASKGRIND_SYNC_EVENT,

}               taskgrind_client_request_t;

// Notify taskgring of a create event
//  - arg[1] is the task unique identifier (> 0) defined by the client
//  - arg[2] is the task type (taskgrind_task_type_t)
#define TASKGRIND_CREATE_EVENT(_qzz_key, _qzz_type)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_CREATE_EVENT, (_qzz_key), (_qzz_type), 0, 0, 0)

// Notify taskgrind of a schedule event
//  - arg[1] is the task unique identifier (> 0)
#define TASKGRIND_SCHEDULE_EVENT(_qzz_key)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_SCHEDULE_EVENT, (_qzz_key), 0, 0, 0, 0)

// Notify taskgring of an access event (e.g. out: x)
//  - arg[1] is the task unique identifier (> 0)
//  - arg[2] is the access address (&x)
//  - arg[3] is the access type (TASKGRIND_OUT)
#define TASKGRIND_ACCESS_EVENT(_qzz_key, _qzz_addr, _qzz_access_type)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_ACCESS_EVENT, (_qzz_key), (_qzz_addr), (_qzz_access_type), 0, 0)

// Notify taskgring of a barrier requiring current task children completion
#define TASKGRIND_SYNC_EVENT()  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_SYNC_EVENT, 0, 0, 0, 0, 0)

#endif /* TASKGRIND_API_H */
