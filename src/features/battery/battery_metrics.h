#ifndef REACH_FEATURES_BATTERY_METRICS_H
#define REACH_FEATURES_BATTERY_METRICS_H

#include "reach/core/typography.h"

struct reach_battery_metrics
{
    float popup_width;
    float padding;
    float screen_margin;

    float row_height;
    float row_gap;
    float row_inset;

    float separator_height;
    float separator_inset;

    float label_text_size;
    float value_text_size;

    float toggle_width;
    float toggle_height;
};

static constexpr reach_battery_metrics reach_battery_make_metrics()
{
    reach_battery_metrics metrics = {};

    metrics.popup_width = 224.0f;
    metrics.padding = 8.0f;
    metrics.screen_margin = 8.0f;

    metrics.row_height = 34.0f;
    metrics.row_gap = 2.0f;
    metrics.row_inset = 12.0f;

    metrics.separator_height = 1.0f;
    metrics.separator_inset = 12.0f;

    metrics.label_text_size = REACH_TEXT_SIZE_MEDIUM;
    metrics.value_text_size = REACH_TEXT_SIZE_MEDIUM;

    metrics.toggle_width = 36.0f;
    metrics.toggle_height = 20.0f;

    return metrics;
}

static constexpr reach_battery_metrics reach_battery_metrics_values = reach_battery_make_metrics();

#endif
