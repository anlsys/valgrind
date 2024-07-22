// TODO: header

#ifndef TASKGRIND_API_H
# define TASKGRIND_API_H

# include "valgrind.h"

# define TASKGRIND_BASE_STACK_PTR       (0x1fffffffff)

// RESERVED ids that client cannot use for TASKGRIND tasks
# define TASKGRIND_CLIENT_ID_PRIVATE    ((UWord)-1)

// event type
typedef enum    taskgrind_event_t
{
    TASKGRIND_EVENT_BEGIN,
    TASKGRIND_EVENT_END,
}               taskgrind_event_t;

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

// Taskgrind sync type
typedef enum    taskgrind_sync_t
{
    // wait for all descendent tasks
    TASKGRIND_SYNC_BARRIER,

    // wait for children tasks
    TASKGRIND_SYNC_TASKWAIT,

    // wait for the group task
    TASKGRIND_SYNC_TASKGROUP,

}               taskgrind_sync_t;

// Taskgrind fulfill mode
typedef enum    taskgrind_fulfill_mode_t
{
    // the task executed before the allow completion event fulfilled
    TASKGRIND_FULFILL_LATE,

    // the opposite
    TASKGRIND_FULFILL_EARLY,
}               taskgrind_fulfill_mode_t;

// valgrind client requests
typedef enum    taskgrind_client_request_t
{
    // a parallel fork point
    VG_USERREQ__TASKGRIND_FORK_POINT_EVENT = VG_USERREQ_TOOL_BASE('T', 'G'),
    VG_USERREQ__TASKGRIND_JOIN_POINT_EVENT,

    // a thread begin / end
    VG_USERREQ__TASKGRIND_THREAD_BEGIN_EVENT,
    VG_USERREQ__TASKGRIND_THREAD_END_EVENT,

    // a new task has been created
    VG_USERREQ__TASKGRIND_CREATE_EVENT,

    // a new task start executed on the current thread
    VG_USERREQ__TASKGRIND_SCHEDULE_EVENT,

    // depend(out: x) - send 'out' and 'address of x'
    VG_USERREQ__TASKGRIND_DEPEND_EVENT,

    // Executing thread waits for the completion of the current tasks
    //  - 0 - children
    //  - 1 - descendent
    VG_USERREQ__TASKGRIND_SYNC_EVENT,

    // The current task allow the completion of the passed parameter task
    VG_USERREQ__TASKGRIND_DETACH_FULFILL_EVENT,

}               taskgrind_client_request_t;

// Notify taskgrind of a parallel region begin (called on the parent thread)
// so it save current segment as the root of future thread_begin
#define TASKGRIND_FORK_POINT_EVENT()    \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_FORK_POINT_EVENT, 0, 0, 0, 0, 0)

// Notify taskgrind of a parallel region end (called on the parent thread)
// so any forked threads join here
#define TASKGRIND_JOIN_POINT_EVENT()    \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_JOIN_POINT_EVENT, 0, 0, 0, 0, 0)

// Notify taskgrind that a thread begin (called on the thread)
// so it forks an empty segment for that thread
#define TASKGRIND_THREAD_BEGIN_EVENT()    \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_THREAD_BEGIN_EVENT, 0, 0, 0, 0, 0)

// Notify taskgrind that a thread ended (called on the thread)
// so it join its last segment to the parent join request
#define TASKGRIND_THREAD_END_EVENT()    \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_THREAD_END_EVENT, 0, 0, 0, 0, 0)



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
//  - arg[1] is a 'taskgrind_sync_t'
#define TASKGRIND_SYNC_EVENT(_qzz_type)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_SYNC_EVENT, (_qzz_type), 0, 0, 0, 0)

// Notify taskgrind of that the current task allows the completion of the given task
//  - arg[1] - the task unique identifier (> 0)
//  - arg[2] - the fulfill mode (early or late) - see taskgrind_fulfill_mode_t
# define TASKGRIND_DETACH_FULFILL_EVENT(_qzz_key, _qzz_type) \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_DETACH_FULFILL_EVENT, (_qzz_key), (_qzz_type), 0, 0, 0)
#endif /* TASKGRIND_API_H */
