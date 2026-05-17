#ifndef REACH_DOCK_H
#define REACH_DOCK_H

#include <stddef.h>
#include <stdint.h>

#include "reach/applist.h"
#include "reach/config.h"
#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_dock reach_dock;

typedef struct reach_dock_desc {
    const reach_dock_config *config;
    const reach_applist *apps;
} reach_dock_desc;

reach_result reach_dock_create(const reach_dock_desc *desc, reach_dock **out_dock);
void reach_dock_destroy(reach_dock *dock);
reach_result reach_dock_show(reach_dock *dock);
reach_result reach_dock_hide(reach_dock *dock);
reach_result reach_dock_update(reach_dock *dock, double delta_seconds);
reach_result reach_dock_set_auto_hidden(reach_dock *dock, int32_t hidden);
reach_result reach_dock_show_tray_menu(reach_dock *dock, int32_t x, int32_t y);

#ifdef __cplusplus
}
#endif

#endif
