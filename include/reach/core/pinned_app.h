#ifndef REACH_CORE_PINNED_APP_H
#define REACH_CORE_PINNED_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_MAX_PINNED_APPS 12

    typedef struct reach_pinned_app_model
    {
        uint32_t id;
        uint16_t title[128];
        uint16_t path[260];
        uint16_t arguments[260];
        uint16_t icon_ref[260];
        uint16_t app_user_model_id[260];
    } reach_pinned_app_model;

#ifdef __cplusplus
}
#endif

#endif
