#ifndef REACH_FEATURES_CLIPBOARD_METRICS_H
#define REACH_FEATURES_CLIPBOARD_METRICS_H

#include "reach/core/render_commands.h"

struct reach_clipboard_metrics
{
    // Panel
    float panel_width;
    float panel_height;
    float margin;

    // Padding / Gutters
    float padding;
    float scrollbar_gutter;

    // Title
    float title_height;

    // Rows
    float row_height;
    float row_gap;

    // Close button
    float close_button_size;
    float close_button_margin;
    float close_button_hover_alpha;
    float close_button_inset_ratio;

    // Scrollbar
    float track_x_offset;
    float track_width;
    float thumb_size;

    // Render
    float item_radius;
    float thumbnail_padding;
};

static constexpr reach_clipboard_metrics reach_clipboard_make_metrics()
{
    reach_clipboard_metrics metrics = {};

    // Panel
    metrics.panel_width = 390.0f;
    metrics.panel_height = 500.0f;
    metrics.margin = 12.0f;

    // Padding / Gutters
    metrics.padding = 16.0f;
    metrics.scrollbar_gutter = 15.0f;

    // Title
    metrics.title_height = 34.0f;

    // Rows
    metrics.row_height = 88.0f;
    metrics.row_gap = 8.0f;

    // Close button
    metrics.close_button_size = 20.0f;
    metrics.close_button_margin = 6.0f;
    metrics.close_button_hover_alpha = 0.12f;
    metrics.close_button_inset_ratio = 0.18f;

    // Scrollbar
    metrics.track_x_offset = 7.0f;
    metrics.track_width = 4.0f;
    metrics.thumb_size = 24.0f;

    // Render
    metrics.item_radius = 6.0f;
    metrics.thumbnail_padding = 12.0f;

    return metrics;
}

static constexpr reach_clipboard_metrics reach_clipboard_metrics_values =
    reach_clipboard_make_metrics();

static inline float reach_clipboard_scale_value(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

static inline reach_clipboard_metrics reach_clipboard_metrics_for_scale(float dpi_scale)
{
    reach_clipboard_metrics metrics = reach_clipboard_metrics_values;

    // Panel
    metrics.panel_width = reach_clipboard_scale_value(metrics.panel_width, dpi_scale);
    metrics.panel_height = reach_clipboard_scale_value(metrics.panel_height, dpi_scale);
    metrics.margin = reach_clipboard_scale_value(metrics.margin, dpi_scale);

    // Padding / Gutters
    metrics.padding = reach_clipboard_scale_value(metrics.padding, dpi_scale);
    metrics.scrollbar_gutter = reach_clipboard_scale_value(metrics.scrollbar_gutter, dpi_scale);

    // Title
    metrics.title_height = reach_clipboard_scale_value(metrics.title_height, dpi_scale);

    // Rows
    metrics.row_height = reach_clipboard_scale_value(metrics.row_height, dpi_scale);
    metrics.row_gap = reach_clipboard_scale_value(metrics.row_gap, dpi_scale);

    // Close button
    metrics.close_button_size = reach_clipboard_scale_value(metrics.close_button_size, dpi_scale);
    metrics.close_button_margin =
        reach_clipboard_scale_value(metrics.close_button_margin, dpi_scale);

    // Scrollbar
    metrics.track_x_offset = reach_clipboard_scale_value(metrics.track_x_offset, dpi_scale);
    metrics.track_width = reach_clipboard_scale_value(metrics.track_width, dpi_scale);
    metrics.thumb_size = reach_clipboard_scale_value(metrics.thumb_size, dpi_scale);

    // Render
    metrics.item_radius = reach_clipboard_scale_value(metrics.item_radius, dpi_scale);
    metrics.thumbnail_padding =
        reach_clipboard_scale_value(metrics.thumbnail_padding, dpi_scale);

    return metrics;
}

#endif
