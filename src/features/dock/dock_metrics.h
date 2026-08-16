#ifndef REACH_FEATURES_DOCK_METRICS_H
#define REACH_FEATURES_DOCK_METRICS_H

#include "reach/core/render_commands.h"

struct reach_dock_metrics
{
    float fallback_icon_background_alpha;
    float running_indicator_size;
    float running_indicator_gap;
    float running_indicator_bottom_inset;
    float running_indicator_focused_alpha;
    float running_indicator_unfocused_alpha;

    float click_feedback_min_opacity;
    float reorder_neighbor_threshold_ratio;
};

static constexpr reach_dock_metrics reach_dock_make_metrics()
{
    reach_dock_metrics metrics = {};

    metrics.fallback_icon_background_alpha = 0.80f;
    metrics.running_indicator_size = 4.0f;
    metrics.running_indicator_gap = 4.0f;
    metrics.running_indicator_bottom_inset = 2.0f;
    metrics.running_indicator_focused_alpha = 1.0f;
    metrics.running_indicator_unfocused_alpha = 0.5f;

    metrics.click_feedback_min_opacity = 0.001f;
    metrics.reorder_neighbor_threshold_ratio = 0.25f;

    return metrics;
}

static constexpr reach_dock_metrics reach_dock_metrics_values = reach_dock_make_metrics();

#endif
