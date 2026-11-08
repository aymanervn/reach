#ifndef REACH_PORTS_RENDER_DEVICE_H
#define REACH_PORTS_RENDER_DEVICE_H

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_render_device reach_render_device;

    typedef enum reach_render_device_state
    {
        REACH_RENDER_DEVICE_READY = 0,
        REACH_RENDER_DEVICE_LOST = 1
    } reach_render_device_state;

    typedef struct reach_render_device_status
    {
        reach_render_device_state state;
        uint32_t retry_wait_ms;
        uint64_t generation;
    } reach_render_device_status;

    typedef struct reach_render_device_diagnostics
    {
        uint32_t losses;
        uint32_t recoveries;
        int32_t warp_active;
        int32_t last_hr;
        int32_t last_reason;
        uint64_t generation;
    } reach_render_device_diagnostics;

    typedef struct reach_render_device_ops
    {
        reach_render_device_status (*status)(reach_render_device *device);
        reach_result (*recover)(reach_render_device *device);
        void *(*wait_handle)(reach_render_device *device);
        void (*diagnostics)(reach_render_device *device, reach_render_device_diagnostics *out);
    } reach_render_device_ops;

    typedef struct reach_render_device_port
    {
        reach_render_device *device;
        reach_render_device_ops ops;
    } reach_render_device_port;

#ifdef __cplusplus
}
#endif

#endif
