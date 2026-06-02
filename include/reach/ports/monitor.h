#ifndef REACH_PORTS_MONITOR_H
#define REACH_PORTS_MONITOR_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_monitor_info
    {
        uint32_t id;
        reach_rect_i32 bounds;
        reach_rect_i32 work_area;
        int32_t dpi_x;
        int32_t dpi_y;
        int32_t primary;
    } reach_monitor_info;

    typedef struct reach_monitor_list reach_monitor_list;

    typedef struct reach_monitor_ops
    {
        void (*destroy)(reach_monitor_list *list);
        reach_result (*refresh)(reach_monitor_list *list);
        size_t (*count)(const reach_monitor_list *list);
        const reach_monitor_info *(*get)(const reach_monitor_list *list, size_t index);
        const reach_monitor_info *(*primary)(const reach_monitor_list *list);
    } reach_monitor_ops;

    typedef struct reach_monitor_port
    {
        reach_monitor_list *list;
        reach_monitor_ops ops;
    } reach_monitor_port;

#ifdef __cplusplus
}
#endif

#endif
