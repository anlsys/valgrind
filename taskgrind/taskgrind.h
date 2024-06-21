// TODO: header

#ifndef TASKGRIND_API_H
# define TASKGRIND_API_H

# include "valgrind.h"

# define TASKGRIND_BASE_STACK_PTR       (0x1fffffffff)

// RESERVED ids that client cannot use for TASKGRIND tasks
# define TASKGRIND_CLIENT_ID_PRIVATE    ((UWord)-1)

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
    // a new task has been created
    VG_USERREQ__TASKGRIND_CREATE_EVENT      = VG_USERREQ_TOOL_BASE('T', 'G'),

    // a new task start executed on the current thread
    VG_USERREQ__TASKGRIND_SCHEDULE_EVENT,

    // depend(out: x) - send 'out' and 'address of x'
    VG_USERREQ__TASKGRIND_DEPEND_EVENT,

    // Executing thread waits for the completion of all children tasks of the current task
    VG_USERREQ__TASKGRIND_SYNC_EVENT,

}               taskgrind_client_request_t;

// Notify taskgring of a create event
//  - arg[1] is the task unique identifier (> 0) defined by the client
//  - arg[2] is the task type (taskgrind_task_type_t)
//  - arg[3] boolean whether the task is undeferred or not
#define TASKGRIND_CREATE_EVENT(_qzz_key, _qzz_type, _qzz_undeferred)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_CREATE_EVENT, (_qzz_key), (_qzz_type), (_qzz_undeferred), 0, 0)

// Notify taskgrind of a schedule event
//  - arg[1] is the task unique identifier (> 0)
#define TASKGRIND_SCHEDULE_EVENT(_qzz_key)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_SCHEDULE_EVENT, (_qzz_key), 0, 0, 0, 0)

// Notify taskgring of an access event (e.g. out: x)
//  - arg[1] is the task unique identifier (> 0)
//  - arg[2] is the access address (&x)
//  - arg[3] is the access type (TASKGRIND_OUT)
#define TASKGRIND_DEPEND_EVENT(_qzz_key, _qzz_addr, _qzz_access_type)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_DEPEND_EVENT, (_qzz_key), (_qzz_addr), (_qzz_access_type), 0, 0)

// Notify taskgring of a barrier requiring current task children completion
#define TASKGRIND_SYNC_EVENT()  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_SYNC_EVENT, 0, 0, 0, 0, 0)

#endif /* TASKGRIND_API_H */
