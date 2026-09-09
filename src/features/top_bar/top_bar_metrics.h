#ifndef REACH_FEATURES_TOP_BAR_METRICS_H
#define REACH_FEATURES_TOP_BAR_METRICS_H

#include "reach/core/config.h"
#include "reach/core/render_commands.h"
#include "reach/core/typography.h"

typedef enum reach_top_bar_background_style
{
    REACH_TOP_BAR_BACKGROUND_SPLIT = 0,
    REACH_TOP_BAR_BACKGROUND_UNIFIED = 1,
    REACH_TOP_BAR_BACKGROUND_SIMPLE = 2
} reach_top_bar_background_style;

struct reach_top_bar_style_profile
{
    float height;
    float screen_gap;
    float app_clearance;
    float edge_inset;
    reach_top_bar_background_style background;
    int32_t border;
};

static constexpr reach_top_bar_style_profile
reach_top_bar_style_profile_for(reach_config_top_bar_style style)
{
    switch (style)
    {
    case REACH_CONFIG_TOP_BAR_STYLE_UNIFIED:
        return {34.5f, 6.0f, 6.0f, 8.0f, REACH_TOP_BAR_BACKGROUND_UNIFIED, 1};
    case REACH_CONFIG_TOP_BAR_STYLE_SIMPLE:
        return {34.5f, 0.0f, 0.0f, 0.0f, REACH_TOP_BAR_BACKGROUND_SIMPLE, 0};
    case REACH_CONFIG_TOP_BAR_STYLE_SPLIT:
    default:
        return {34.5f, 6.0f, 6.0f, 8.0f, REACH_TOP_BAR_BACKGROUND_SPLIT, 1};
    }
}

struct reach_top_bar_metrics
{
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
    float language_padding;
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

    metrics.pill_gap = 6.0f;
    metrics.pill_padding = 10.0f;

    metrics.glyph_advance_ratio = 0.62f;
    metrics.width_animation_seconds = 0.22;

    metrics.clock_gap = 9.0f;
    metrics.clock_time_text_size = REACH_TEXT_SIZE_MEDIUM;
    metrics.clock_time_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.clock_date_text_size = REACH_TEXT_SIZE_SMALL;
    metrics.clock_date_text_weight = REACH_TEXT_WEIGHT_NORMAL;

    metrics.now_playing_collapsed_width = 44.0f;
    metrics.now_playing_collapsed_glyph_scale = 0.46f;

    metrics.current_app_icon_scale = 0.55f;
    metrics.current_app_gap = 8.0f;
    metrics.current_app_name_text_size = REACH_TEXT_SIZE_MEDIUM;
    metrics.current_app_name_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
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
    metrics.network_name_text_size = REACH_TEXT_SIZE_MEDIUM;
    metrics.network_name_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.network_name_max_width = 96.0f;
    metrics.volume_text_size = REACH_TEXT_SIZE_SMALL;
    metrics.volume_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.language_padding = 6.0f;
    metrics.language_text_size = REACH_TEXT_SIZE_SMALL;
    metrics.language_text_weight = REACH_TEXT_WEIGHT_SEMIBOLD;

    metrics.stats_gap = 8.0f;
    metrics.stats_group_gap = 12.0f;
    metrics.stats_text_size = REACH_TEXT_SIZE_SMALL;
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
