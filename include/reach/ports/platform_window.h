#ifndef REACH_PORTS_PLATFORM_WINDOW_H
#define REACH_PORTS_PLATFORM_WINDOW_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/animated_rect.h"
#include "reach/core/ui_events.h"
#include "reach/core/window_id.h"
#include "reach/support/util.h"
#include "reach/support/motion.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_platform_window_flag
    {
        REACH_WINDOW_ACTIVATE = 1u << 0,
        REACH_WINDOW_COMPOSITION_BACKED = 1u << 1,
        REACH_WINDOW_APPLICATION = 1u << 2,
        REACH_WINDOW_SESSION_EVENTS = 1u << 3,
        REACH_WINDOW_DESKTOP_MINIMIZE_SUPPORT = 1u << 4,
        REACH_WINDOW_CLOSE_REQUEST_EVENT = 1u << 5,
        REACH_WINDOW_BOUNDS_EVENTS = 1u << 6,
        REACH_WINDOW_FOCUS_LOST_EVENT = 1u << 7,
        REACH_WINDOW_CROSSHAIR_CURSOR = 1u << 8,
        REACH_WINDOW_TEXT_INPUT = 1u << 9,
        REACH_WINDOW_STACK_ABOVE_DESKTOP = 1u << 10
    } reach_platform_window_flag;

#define REACH_PLATFORM_WINDOW_MAX_CAPTION_EXCLUSIONS 4
#define REACH_PLATFORM_WINDOW_MAX_INPUT_REGIONS 16

    typedef struct reach_platform_window reach_platform_window;
    typedef void (*reach_platform_window_event_callback)(void *user, const reach_ui_event *event);

    typedef struct reach_platform_window_caption
    {
        reach_rect_f32 bounds;
        reach_rect_f32 exclusions[REACH_PLATFORM_WINDOW_MAX_CAPTION_EXCLUSIONS];
        size_t exclusion_count;
    } reach_platform_window_caption;

    typedef struct reach_platform_window_input_presentation
    {
        reach_rect_f32 bounds;
        reach_motion x;
        reach_motion y;
        reach_motion scale;
        reach_motion scale_y;
        reach_motion center_x;
        reach_motion center_y;
        int32_t has_motion_bounds;
        reach_rect_motion motion_bounds;
        size_t (*input_regions_at)(const void *user, double now_seconds,
                                   reach_rect_f32 *out_regions, size_t max_regions);
        const void *input_regions_user;
        reach_transform_f32 input_regions_transform;
    } reach_platform_window_input_presentation;

    typedef struct reach_platform_window_ops
    {
        reach_result (*show)(reach_platform_window *window);
        reach_result (*hide)(reach_platform_window *window);
        reach_result (*set_bounds)(reach_platform_window *window, reach_rect_f32 bounds);
        reach_result (*get_bounds)(const reach_platform_window *window, reach_rect_f32 *out_bounds);
        float (*dpi_scale)(const reach_platform_window *window);
        reach_result (*set_blur_enabled)(reach_platform_window *window, int32_t enabled);
        reach_result (*set_caption)(reach_platform_window *window,
                                    const reach_platform_window_caption *caption);
        reach_result (*set_event_callback)(reach_platform_window *window,
                                           reach_platform_window_event_callback callback,
                                           void *user);
        int32_t (*has_pending_events)(const reach_platform_window *window);
        reach_result (*dispatch_events)(reach_platform_window *window);
        reach_result (*set_pointer_move_enabled)(reach_platform_window *window, int32_t enabled);
        reach_result (*set_pointer_capture)(reach_platform_window *window, int32_t enabled);
        reach_result (*set_topmost)(reach_platform_window *window, int32_t enabled);
        reach_result (*raise)(reach_platform_window *window);
        reach_result (*minimize)(reach_platform_window *window);
        int32_t (*is_minimized)(const reach_platform_window *window);
        int32_t (*is_active)(const reach_platform_window *window);
        reach_window_id (*native_id)(const reach_platform_window *window);
        reach_result (*place_behind)(reach_platform_window *window, reach_window_id target);
        reach_result (*post_event)(reach_platform_window *window, reach_ui_event_type type);
        reach_result (*set_input_passthrough)(reach_platform_window *window, int32_t enabled);
        reach_result (*set_input_regions)(reach_platform_window *window,
                                          const reach_rect_f32 *regions, size_t region_count);
        void (*destroy)(reach_platform_window *window);
        reach_result (*set_input_presentation)(
            reach_platform_window *window,
            const reach_platform_window_input_presentation *presentation);
        reach_result (*take_keyboard_focus)(reach_platform_window *window);
        /* Reapply the surface's platform-managed stacking while it is presented. */
        reach_result (*restack)(reach_platform_window *window);
        /* A window-local rect (the space of set_input_regions) that shows the vertical resize
           cursor; nullptr clears it. */
        reach_result (*set_cursor_region)(reach_platform_window *window,
                                          const reach_rect_f32 *region);
    } reach_platform_window_ops;

    typedef struct reach_platform_window_port
    {
        reach_platform_window *window;
        reach_platform_window_ops ops;
    } reach_platform_window_port;

#ifdef __cplusplus
}
#endif

#endif
