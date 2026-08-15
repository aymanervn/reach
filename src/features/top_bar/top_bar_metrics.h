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

    float clock_width;
    float now_playing_collapsed_width;
    float current_app_width;
    float current_app_max_width_ratio;
    float tray_width;
    float quick_settings_width;

    float pill_padding;
    float power_button_scale;
    float power_glyph_scale;
    float power_ring_stroke_width;
    float power_ring_inset;
    float power_ring_track_alpha;
    float power_percent_text_size;
    int32_t power_percent_text_weight;

    float clock_gap;
    float clock_time_height_ratio;
    float clock_time_text_size;
    int32_t clock_time_text_weight;
    float clock_date_text_size;
    int32_t clock_date_text_weight;

    float click_feedback_min_opacity;
};

static constexpr reach_top_bar_metrics reach_top_bar_make_metrics()
{
    reach_top_bar_metrics metrics = {};

    metrics.height_scale = 1.5f;
    metrics.screen_gap = 6.0f;
    metrics.edge_inset = 8.0f;
    metrics.pill_gap = 6.0f;
    metrics.cluster_gap = 12.0f;

    metrics.clock_width = 92.0f;
    metrics.now_playing_collapsed_width = 44.0f;
    metrics.current_app_width = 220.0f;
    metrics.current_app_max_width_ratio = 0.28f;
    metrics.tray_width = 96.0f;
    metrics.quick_settings_width = 84.0f;

    metrics.pill_padding = 6.0f;
    metrics.power_button_scale = 0.72f;
    metrics.power_glyph_scale = 0.50f;
    metrics.power_ring_stroke_width = 2.25f;
    metrics.power_ring_inset = 0.5f;
    metrics.power_ring_track_alpha = 0.16f;
    metrics.power_percent_text_size = 13.0f;
    metrics.power_percent_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;

    metrics.clock_gap = 8.0f;
    metrics.clock_time_height_ratio = 0.52f;
    metrics.clock_time_text_size = 15.0f;
    metrics.clock_time_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.clock_date_text_size = 11.0f;
    metrics.clock_date_text_weight = REACH_TEXT_WEIGHT_NORMAL;

    metrics.click_feedback_min_opacity = 0.001f;

    return metrics;
}

static constexpr reach_top_bar_metrics reach_top_bar_metrics_values = reach_top_bar_make_metrics();

#endif
