#ifndef REACH_FEATURES_WIDGETS_H
#define REACH_FEATURES_WIDGETS_H

#include "reach/core/render_commands.h"
#include "reach/core/config.h"
#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/clock.h"
#include "reach/services/now_playing.h"
#include "reach/services/system_stats.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_widgets reach_widgets;

    typedef struct reach_widgets_routes
    {
        void *user;
        void (*placement_changed)(void *user, size_t index,
                                  reach_config_widget_placement placement);
    } reach_widgets_routes;

    reach_result reach_widgets_create(reach_widgets **out_widgets);
    void reach_widgets_destroy(reach_widgets *widgets);
    void reach_widgets_attach_services(reach_widgets *widgets, reach_clock *clock,
                                       reach_system_stats *stats,
                                       reach_now_playing_service *now_playing);
    void reach_widgets_set_routes(reach_widgets *widgets, const reach_widgets_routes *routes);
    int32_t reach_widgets_set_placement(reach_widgets *widgets, size_t index,
                                        reach_config_widget_placement placement);
    int32_t reach_widgets_set_enabled(reach_widgets *widgets, uint32_t mask);

    const reach_feature_capsule_ops *reach_widgets_desktop_capsule_ops(void);

    int32_t reach_widgets_arrange_desktop(reach_widgets *widgets, reach_rect_f32 monitor,
                                          float dpi_scale);
    reach_result reach_widgets_render_desktop(const reach_widgets *widgets,
                                              const reach_theme *theme, float dpi_scale,
                                              reach_render_command_buffer *commands);

#ifdef __cplusplus
}
#endif

#endif
