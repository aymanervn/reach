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
    REACH_SURFACE_TRAY_MENU = 2,
    REACH_SURFACE_SWITCHER = 3,
    REACH_SURFACE_CONTEXT_MENU = 4,
    REACH_SURFACE_QUICK_SETTINGS = 5
} reach_surface_role;

typedef struct reach_platform_window reach_platform_window;
typedef void (*reach_platform_window_event_callback)(void *user, const reach_ui_event *event);

typedef struct reach_platform_window_ops {
    reach_result (*show)(reach_platform_window *window);
    reach_result (*hide)(reach_platform_window *window);
    reach_result (*set_bounds)(reach_platform_window *window, reach_rect_f32 bounds);
    reach_result (*set_opacity)(reach_platform_window *window, float opacity);
    reach_result (*set_blur_enabled)(reach_platform_window *window, int32_t enabled);
    reach_result (*apply_rounded_corners)(reach_platform_window *window, float radius);
    reach_result (*set_event_callback)(reach_platform_window *window, reach_platform_window_event_callback callback, void *user);
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
