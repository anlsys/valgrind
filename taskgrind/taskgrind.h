// TODO: header

#ifndef TASKGRIND_API_H
# define TASKGRIND_API_H

# include "valgrind.h"

typedef enum    taskgrind_client_request_t
{
    VG_USERREQ__TASKGRIND_CREATE_EVENT      = VG_USERREQ_TOOL_BASE('T', 'G'),
    VG_USERREQ__TASKGRIND_SCHEDULE_EVENT,
    VG_USERREQ__TASKGRIND_ACCESS_EVENT,
    // VG_USERREQ__TASKGRIND_TASK_DEPS_EVENT,

}               taskgrind_client_request_t;

// Notify taskgring of a create event
//  - arg[1] is the task unique identifier, defined by the client
#define TASKGRIND_CREATE_EVENT(_qzz_key)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_CREATE_EVENT, (_qzz_key), 0, 0, 0, 0)

// Notify taskgrind of a schedule event
//  - arg[1] is the task unique identifier
#define TASKGRIND_SCHEDULE_EVENT(_qzz_key)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_SCHEDULE_EVENT, (_qzz_key), 0, 0, 0, 0)

// Notify taskgring of an access event (e.g. out: x)
//  - arg[1] is the task unique identifier
//  - arg[2] is the access address (&x)
//  - arg[3] is the access type (TASKGRIND_OUT)
#define TASKGRIND_ACCESS_EVENT(_qzz_key, _qzz_addr, _qzz_access_type)  \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__TASKGRIND_ACCESS_EVENT, (_qzz_key), (_qzz_addr), (_qzz_access_type), 0, 0)

// Taskgrind accesses, similar to OmpSs / OpenMP
typedef enum    taskgrind_access_t
{
    TASKGRIND_IN,
    TASKGRIND_OUT,
    TASKGRIND_OUTSET,
}               taskgrind_access_t;

#endif /* TASKGRIND_API_H */
