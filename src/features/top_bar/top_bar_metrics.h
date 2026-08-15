#ifndef REACH_FEATURES_TOP_BAR_METRICS_H
#define REACH_FEATURES_TOP_BAR_METRICS_H

#include "reach/core/render_commands.h"

struct reach_top_bar_metrics
{
    float height_scale;
    float screen_gap;
    float edge_inset;
    float pill_gap;
    float cluster_gap;

    float power_clock_width;
    float now_playing_collapsed_width;
    float current_app_width;
    float current_app_max_width_ratio;
    float tray_width;
    float quick_settings_width;
};

static constexpr reach_top_bar_metrics reach_top_bar_make_metrics()
{
    reach_top_bar_metrics metrics = {};

    metrics.height_scale = 1.5f;
    metrics.screen_gap = 6.0f;
    metrics.edge_inset = 8.0f;
    metrics.pill_gap = 6.0f;
    metrics.cluster_gap = 12.0f;

    metrics.power_clock_width = 132.0f;
    metrics.now_playing_collapsed_width = 44.0f;
    metrics.current_app_width = 220.0f;
    metrics.current_app_max_width_ratio = 0.28f;
    metrics.tray_width = 96.0f;
    metrics.quick_settings_width = 84.0f;

    return metrics;
}

static constexpr reach_top_bar_metrics reach_top_bar_metrics_values = reach_top_bar_make_metrics();

#endif
