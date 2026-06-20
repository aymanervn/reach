#ifndef REACH_FEATURES_CLIPBOARD_METRICS_H
#define REACH_FEATURES_CLIPBOARD_METRICS_H

#include <stdint.h>

#include "reach/core/render_commands.h"

struct reach_clipboard_metrics
{
    float panel_width;
    float padding;
    float screen_edge_margin;
    float launcher_gap;

    float title_height;
    float title_gap;
    float title_font_size;
    int32_t title_font_weight;
    float items_font_size;
    float clear_button_width;
    float clear_button_height;
    float clear_button_gap;
    float clear_button_radius;
    float clear_button_background_alpha;

    float scrollbar_gutter;
    float item_scrollbar_gap;

    float item_default_size;
    float item_large_size;
    float item_gap;

    float close_button_size;
    float close_button_margin;
    float close_button_hover_alpha;
    float close_button_inset_ratio;

    float track_x_offset;
    float track_width;
    float thumb_size;

    float item_radius;
    float thumbnail_padding;
    float thumbnail_height;
    float thumbnail_text_gap;
    float thumbnail_max_width_ratio;
};

static constexpr reach_clipboard_metrics reach_clipboard_make_metrics()
{
    reach_clipboard_metrics metrics = {};

    metrics.panel_width = 390.0f;
    metrics.screen_edge_margin = 12.0f;
    metrics.launcher_gap = 12.0f;

    metrics.title_height = 32.0f;
    metrics.title_gap = 8.0f;
    metrics.title_font_size = 15.0f;
    metrics.title_font_weight = REACH_TEXT_WEIGHT_SEMIBOLD;
    metrics.items_font_size = 12.0f;
    metrics.clear_button_width = 54.0f;
    metrics.clear_button_height = 24.0f;
    metrics.clear_button_gap = 8.0f;
    metrics.clear_button_radius = 5.0f;
    metrics.clear_button_background_alpha = 0.10f;

    metrics.padding = 8.0f;
    metrics.scrollbar_gutter = 15.0f;
    metrics.item_scrollbar_gap = 4.0f;

    metrics.item_default_size = 44.0f;
    metrics.item_large_size = 88.0f;
    metrics.item_gap = 8.0f;

    metrics.close_button_size = 20.0f;
    metrics.close_button_margin = 6.0f;
    metrics.close_button_hover_alpha = 0.12f;
    metrics.close_button_inset_ratio = 0.18f;

    metrics.track_x_offset = 7.0f;
    metrics.track_width = 4.0f;
    metrics.thumb_size = 24.0f;

    metrics.item_radius = 6.0f;
    metrics.thumbnail_padding = 12.0f;
    metrics.thumbnail_height = 72.0f;
    metrics.thumbnail_text_gap = 12.0f;
    metrics.thumbnail_max_width_ratio = 0.50f;
    return metrics;
}

static constexpr reach_clipboard_metrics reach_clipboard_metrics_values =
    reach_clipboard_make_metrics();

static inline float reach_clipboard_effective_dpi_scale(float dpi_scale)
{
    return dpi_scale > 0.0f ? dpi_scale : 1.0f;
}

static inline float reach_clipboard_scale_value(float value, float dpi_scale)
{
    return value * reach_clipboard_effective_dpi_scale(dpi_scale);
}

static inline reach_clipboard_metrics reach_clipboard_metrics_for_scale(float dpi_scale)
{
    reach_clipboard_metrics metrics = reach_clipboard_metrics_values;

    metrics.panel_width = reach_clipboard_scale_value(metrics.panel_width, dpi_scale);
    metrics.screen_edge_margin = reach_clipboard_scale_value(metrics.screen_edge_margin, dpi_scale);
    metrics.launcher_gap = reach_clipboard_scale_value(metrics.launcher_gap, dpi_scale);

    metrics.title_height = reach_clipboard_scale_value(metrics.title_height, dpi_scale);
    metrics.title_gap = reach_clipboard_scale_value(metrics.title_gap, dpi_scale);
    metrics.title_font_size = reach_clipboard_scale_value(metrics.title_font_size, dpi_scale);
    metrics.items_font_size = reach_clipboard_scale_value(metrics.items_font_size, dpi_scale);
    metrics.clear_button_width = reach_clipboard_scale_value(metrics.clear_button_width, dpi_scale);
    metrics.clear_button_height =
        reach_clipboard_scale_value(metrics.clear_button_height, dpi_scale);
    metrics.clear_button_gap = reach_clipboard_scale_value(metrics.clear_button_gap, dpi_scale);
    metrics.clear_button_radius =
        reach_clipboard_scale_value(metrics.clear_button_radius, dpi_scale);

    metrics.padding = reach_clipboard_scale_value(metrics.padding, dpi_scale);
    metrics.scrollbar_gutter = reach_clipboard_scale_value(metrics.scrollbar_gutter, dpi_scale);
    metrics.item_scrollbar_gap = reach_clipboard_scale_value(metrics.item_scrollbar_gap, dpi_scale);

    metrics.item_default_size = reach_clipboard_scale_value(metrics.item_default_size, dpi_scale);
    metrics.item_large_size = reach_clipboard_scale_value(metrics.item_large_size, dpi_scale);
    metrics.item_gap = reach_clipboard_scale_value(metrics.item_gap, dpi_scale);

    metrics.close_button_size = reach_clipboard_scale_value(metrics.close_button_size, dpi_scale);
    metrics.close_button_margin =
        reach_clipboard_scale_value(metrics.close_button_margin, dpi_scale);

    metrics.track_x_offset = reach_clipboard_scale_value(metrics.track_x_offset, dpi_scale);
    metrics.track_width = reach_clipboard_scale_value(metrics.track_width, dpi_scale);
    metrics.thumb_size = reach_clipboard_scale_value(metrics.thumb_size, dpi_scale);

    metrics.item_radius = reach_clipboard_scale_value(metrics.item_radius, dpi_scale);
    metrics.thumbnail_padding = reach_clipboard_scale_value(metrics.thumbnail_padding, dpi_scale);
    metrics.thumbnail_height = reach_clipboard_scale_value(metrics.thumbnail_height, dpi_scale);
    metrics.thumbnail_text_gap = reach_clipboard_scale_value(metrics.thumbnail_text_gap, dpi_scale);
    return metrics;
}

#endif
