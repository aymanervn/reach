#ifndef REACH_MONITOR_H
#define REACH_MONITOR_H

#include <stddef.h>
#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_monitor_info {
    uint32_t id;
    reach_rect_i32 bounds;
    reach_rect_i32 work_area;
    int32_t dpi_x;
    int32_t dpi_y;
    int32_t primary;
} reach_monitor_info;

typedef struct reach_monitor_list reach_monitor_list;

reach_result reach_monitor_list_create(reach_monitor_list **out_list);
void reach_monitor_list_destroy(reach_monitor_list *list);
reach_result reach_monitor_refresh(reach_monitor_list *list);
size_t reach_monitor_count(const reach_monitor_list *list);
const reach_monitor_info *reach_monitor_get(const reach_monitor_list *list, size_t index);
const reach_monitor_info *reach_monitor_primary(const reach_monitor_list *list);

#ifdef __cplusplus
}
#endif

#endif
