#ifndef REACH_FEATURES_SCREENSHOT_H
#define REACH_FEATURES_SCREENSHOT_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"
#include "reach/features/feature_capsule.h"
#include "reach/ports/clipboard.h"
#include "reach/ports/screen_capture.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_screenshot reach_screenshot;

    typedef struct reach_screenshot_routes
    {
        void *user;
        void (*copied)(void *user);
    } reach_screenshot_routes;

    reach_result reach_screenshot_create(reach_screenshot **out_screenshot);
    void reach_screenshot_destroy(reach_screenshot *screenshot);

    const reach_feature_capsule_ops *reach_screenshot_capsule_ops(void);

    void reach_screenshot_attach_ports(reach_screenshot *screenshot,
                                       const reach_screen_capture_port *capture,
                                       const reach_clipboard_port *clipboard);
    void reach_screenshot_set_routes(reach_screenshot *screenshot,
                                     const reach_screenshot_routes *routes);
    void reach_screenshot_set_display(reach_screenshot *screenshot,
                                      const reach_display_environment *display);

    int32_t reach_screenshot_is_open(const reach_screenshot *screenshot);
    int32_t reach_screenshot_set_open(reach_screenshot *screenshot, int32_t open);

    int32_t reach_screenshot_arrange(reach_screenshot *screenshot, reach_rect_f32 monitor_bounds);
    reach_result reach_screenshot_append_render_commands(const reach_screenshot *screenshot,
                                                         const reach_theme *theme, float dpi_scale,
                                                         reach_render_command_buffer *out_commands);

    const reach_ui_event_type *reach_screenshot_activation_events(size_t *out_count);
    const reach_ui_event_type *reach_screenshot_routed_events(size_t *out_count);

    size_t reach_screenshot_take_retired_images(reach_screenshot *screenshot, uint64_t *out_ids,
                                                size_t capacity);
    size_t reach_screenshot_image_count(const reach_screenshot *screenshot);
    uint64_t reach_screenshot_image_at(const reach_screenshot *screenshot, size_t index);
    void reach_screenshot_release_image(reach_screenshot *screenshot, uint64_t image_id);
    void reach_screenshot_forget_images(reach_screenshot *screenshot);

#ifdef __cplusplus
}
#endif

#endif
