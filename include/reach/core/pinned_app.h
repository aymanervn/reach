#ifndef REACH_CORE_PINNED_APP_H
#define REACH_CORE_PINNED_APP_H

#include <stdint.h>

#include "reach/core/application.h"
#include "reach/core/limits.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_pinned_app_model
    {
        uint32_t id;
        reach_application application;
    } reach_pinned_app_model;

#ifdef __cplusplus
}
#endif

#endif
