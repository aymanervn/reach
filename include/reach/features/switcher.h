#ifndef REACH_FEATURES_SWITCHER_H
#define REACH_FEATURES_SWITCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
#include "reach/services/window_tracking.h"
#include "reach/core/theme.h"
#include "reach/core/ui_events.h"
#include "reach/ports/text_measure.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SWITCHER_VISIBLE_MAX 12

    typedef struct reach_switcher_state
    {
        int32_t open;
        reach_rect_f32 bounds;
        size_t selected_index;
        size_t visible_start;
        uintptr_t windows[REACH_MAX_OPEN_WINDOWS];
        size_t window_count;
        size_t visible_count;
    } reach_switcher_state;

    typedef struct reach_switcher reach_switcher;

    typedef struct reach_switcher_thumbnail_placement
    {
        reach_window_id window;
        reach_rect_f32 destination;
        int32_t visible;
    } reach_switcher_thumbnail_placement;

    size_t reach_switcher_preview_generation(const reach_switcher *switcher);
    size_t reach_switcher_thumbnail_count(const reach_switcher *switcher);
    reach_result reach_switcher_thumbnail_at(const reach_switcher *switcher, size_t index,
                                             reach_switcher_thumbnail_placement *out);

    typedef struct reach_switcher_arrange_context
    {
        const reach_theme *theme;
        reach_rect_f32 monitor_bounds;
        reach_rect_f32 last_bounds;
        float dpi_scale;
        int32_t transition_visible;
        int32_t bounds_valid;
    } reach_switcher_arrange_context;

    reach_result reach_switcher_create(reach_switcher **out_switcher);

    void reach_switcher_attach_services(reach_switcher *switcher, reach_icon_service *icons,
                                        reach_window_tracking *windows);

    const reach_switcher_state *reach_switcher_state_ptr(const reach_switcher *switcher);

    int32_t reach_switcher_is_open(const reach_switcher *switcher);

    const reach_feature_capsule_ops *reach_switcher_capsule_ops(void);

    const reach_ui_event_type *reach_switcher_routed_events(size_t *out_count);
    void reach_switcher_destroy(reach_switcher *switcher);

    int32_t reach_switcher_arrange(reach_switcher *switcher,
                                   const reach_switcher_arrange_context *ctx);

    typedef struct reach_switcher_render_context
    {
        const reach_theme *theme;
        int32_t icon_size_px;
        reach_text_measure_port text_measure;
    } reach_switcher_render_context;

    void reach_switcher_force_close(reach_switcher *switcher);

    int32_t reach_switcher_set_open(reach_switcher *switcher, int32_t open);
    void reach_switcher_notify_windows_changed(reach_switcher *switcher,
                                               reach_feature_tick_result *out);

    reach_result reach_switcher_append_render_commands(reach_switcher *switcher,
                                                       const reach_switcher_render_context *ctx,
                                                       reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
