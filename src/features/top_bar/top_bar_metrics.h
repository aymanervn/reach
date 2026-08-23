#ifndef REACH_FEATURES_TOP_BAR_METRICS_H
#define REACH_FEATURES_TOP_BAR_METRICS_H

#include "reach/core/render_commands.h"

struct reach_top_bar_metrics
{
    float height;
    float screen_gap;
    float edge_inset;
    float pill_gap;
    float pill_padding;

    float glyph_advance_ratio;
    double width_animation_seconds;

    float clock_gap;
    float clock_time_text_size;
    int32_t clock_time_text_weight;
    float clock_date_text_size;
    int32_t clock_date_text_weight;

    float now_playing_collapsed_width;
    float now_playing_collapsed_glyph_scale;

    float current_app_icon_scale;
    float current_app_gap;
    float current_app_name_text_size;
    int32_t current_app_name_text_weight;
    float current_app_min_text_width;
    float current_app_max_width_ratio;

    float tray_icon_scale;
    float tray_icon_gap;
    float tray_overflow_glyph_scale;
    float tray_background_padding;
    float tray_background_scale;

    float bar_button_scale;
    float bar_button_glyph_scale;
    float quick_settings_padding;
    float quick_settings_content_gap;
    float network_name_text_size;
    int32_t network_name_text_weight;
    float network_name_max_width;
    float volume_text_size;
    int32_t volume_text_weight;
    float language_width;
    float language_text_size;
    int32_t language_text_weight;

    float stats_gap;
    float stats_group_gap;
    float stats_text_size;
    int32_t stats_text_weight;

    float power_glyph_scale;
    double bluetooth_absence_grace_seconds;

    float battery_width;
    float battery_low_percent;

    float click_feedback_min_opacity;
};

static constexpr reach_top_bar_metrics reach_top_bar_make_metrics()
{
    reach_top_bar_metrics metrics = {};

    metrics.height = 34.5f;
    metrics.screen_gap = 6.0f;
    metrics.edge_inset = 8.0f;
    metrics.pill_gap = 6.0f;
    metrics.pill_padding = 10.0f;

    metrics.glyph_advance_ratio = 0.62f;
    metrics.width_animation_seconds = 0.22;

    metrics.clock_gap = 9.0f;
    metrics.clock_time_text_size = 13.0f;
    metrics.clock_time_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.clock_date_text_size = 11.5f;
    metrics.clock_date_text_weight = REACH_TEXT_WEIGHT_NORMAL;

    metrics.now_playing_collapsed_width = 44.0f;
    metrics.now_playing_collapsed_glyph_scale = 0.46f;

    metrics.current_app_icon_scale = 0.55f;
    metrics.current_app_gap = 8.0f;
    metrics.current_app_name_text_size = 12.5f;
    metrics.current_app_name_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.current_app_min_text_width = 34.0f;
    metrics.current_app_max_width_ratio = 0.28f;

    metrics.tray_icon_scale = 0.52f;
    metrics.tray_icon_gap = 7.0f;
    metrics.tray_overflow_glyph_scale = 0.46f;
    metrics.tray_background_padding = 7.0f;
    metrics.tray_background_scale = 0.72f;

    metrics.bar_button_scale = 0.62f;
    metrics.bar_button_glyph_scale = 0.52f;
    metrics.quick_settings_padding = 8.0f;
    metrics.quick_settings_content_gap = 6.0f;
    metrics.network_name_text_size = 11.0f;
    metrics.network_name_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.network_name_max_width = 96.0f;
    metrics.volume_text_size = 11.0f;
    metrics.volume_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.language_width = 32.0f;
    metrics.language_text_size = 11.0f;
    metrics.language_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;

    metrics.stats_gap = 8.0f;
    metrics.stats_group_gap = 12.0f;
    metrics.stats_text_size = 11.0f;
    metrics.stats_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;

    metrics.power_glyph_scale = 0.50f;
    metrics.bluetooth_absence_grace_seconds = 2.0;

    metrics.battery_width = 46.0f;
    metrics.battery_low_percent = 15.0f;

    metrics.click_feedback_min_opacity = 0.001f;

    return metrics;
}

static constexpr reach_top_bar_metrics reach_top_bar_metrics_values = reach_top_bar_make_metrics();

#endif
