#ifndef REACH_PORTS_POWER_SESSION_H
#define REACH_PORTS_POWER_SESSION_H

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_power_session reach_power_session;

typedef struct reach_power_session_ops {
    reach_result (*lock)(reach_power_session *session);
    reach_result (*sleep)(reach_power_session *session);
    reach_result (*restart)(reach_power_session *session);
    reach_result (*shutdown)(reach_power_session *session);
    reach_result (*sign_out)(reach_power_session *session);
    void (*destroy)(reach_power_session *session);
} reach_power_session_ops;

typedef struct reach_power_session_port {
    reach_power_session *session;
    reach_power_session_ops ops;
} reach_power_session_port;

#ifdef __cplusplus
}
#endif

#endif
