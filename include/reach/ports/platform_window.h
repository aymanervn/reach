#ifndef REACH_PORTS_PLATFORM_WINDOW_H
#define REACH_PORTS_PLATFORM_WINDOW_H

#include <stdint.h>

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum reach_surface_role {
    REACH_SURFACE_DOCK = 0,
    REACH_SURFACE_LAUNCHER = 1,
    REACH_SURFACE_TRAY_MENU = 2
} reach_surface_role;

typedef struct reach_platform_window reach_platform_window;

typedef struct reach_platform_window_ops {
    reach_result (*show)(reach_platform_window *window);
    reach_result (*hide)(reach_platform_window *window);
    reach_result (*set_bounds)(reach_platform_window *window, reach_rect_f32 bounds);
    reach_result (*set_opacity)(reach_platform_window *window, float opacity);
    void *(*native_handle)(reach_platform_window *window);
    void (*destroy)(reach_platform_window *window);
} reach_platform_window_ops;

typedef struct reach_platform_window_port {
    reach_platform_window *window;
    reach_platform_window_ops ops;
    reach_surface_role role;
} reach_platform_window_port;

#ifdef __cplusplus
}
#endif

#endif
