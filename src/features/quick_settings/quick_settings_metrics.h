#ifndef REACH_FEATURES_QUICK_SETTINGS_METRICS_H
#define REACH_FEATURES_QUICK_SETTINGS_METRICS_H

#include "reach/core/render_commands.h"
#include "reach/core/typography.h"

struct reach_quick_settings_metrics
{
    float surface_padding;
    float content_padding;
    float panel_width;
    float banner_height;
    float avatar_size;
    float avatar_ring;
    float avatar_overlap;
    float avatar_inset;
    float identity_height;
    float identity_gap;
    float identity_name_height;
    float identity_detail_height;
    float rail_width;
    float rail_item_size;
    float rail_item_gap;
    float page_shift;

    float overview_pill_height;
    float overview_row_gap;
    float overview_pill_icon_size;
    float overview_pill_icon_inset;
    float overview_pill_value_inset;
    float overview_pill_value_width;
    float pill_label_gap;

    float toggle_grid_gap;
    float toggle_height;
    float toggle_gap;
    float toggle_icon_size;
    float toggle_leading_padding;
    float toggle_trailing_padding;
    float toggle_divider_leading_gap;
    float toggle_divider_trailing_gap;
    float toggle_divider_height;
    float toggle_title_height;
    float toggle_status_height;

    float section_label_height;
    float output_device_row_height;
    float output_row_horizontal_padding;
    float output_icon_size;
    float output_check_size;
    float output_row_label_gap;
    float output_row_title_height;
    float output_row_detail_height;
    float app_volume_row_height;
    float wifi_header_height;
    float wifi_switch_width;
    float wifi_switch_height;
    float wifi_key_area_height;
    float wifi_key_gap;
    float wifi_connect_width;
    float wifi_forget_inset;
    float wifi_forget_width;

    float toggle_title_text_size;
    float toggle_status_text_size;
    float identity_name_text_size;
    float identity_detail_text_size;
    float overview_pill_value_text_size;
    float pill_label_text_size;
    float section_label_text_size;
    float output_row_title_text_size;
    float output_row_detail_text_size;
    float wifi_title_text_size;
};

static constexpr reach_quick_settings_metrics reach_quick_settings_make_metrics()
{
    reach_quick_settings_metrics metrics = {};

    metrics.surface_padding = 4.0f;
    metrics.content_padding = 8.0f;
    metrics.panel_width = 340.0f;
    metrics.banner_height = 84.0f;
    metrics.avatar_size = 40.0f;
    metrics.avatar_ring = 2.0f;
    metrics.avatar_overlap = 16.0f;
    metrics.avatar_inset = 10.0f;
    metrics.identity_height = 34.0f;
    metrics.identity_gap = 8.0f;
    metrics.identity_name_height = 17.0f;
    metrics.identity_detail_height = 14.0f;
    metrics.rail_width = 38.0f;
    metrics.rail_item_size = 28.0f;
    metrics.rail_item_gap = 6.0f;
    metrics.page_shift = 12.0f;

    metrics.overview_pill_height = 32.0f;
    metrics.overview_row_gap = 8.0f;
    metrics.overview_pill_icon_size = 16.0f;
    metrics.overview_pill_icon_inset = 10.0f;
    metrics.overview_pill_value_inset = 12.0f;
    metrics.overview_pill_value_width = 44.0f;
    metrics.pill_label_gap = 8.0f;

    metrics.toggle_grid_gap = 10.0f;
    metrics.toggle_height = 40.0f;
    metrics.toggle_gap = 6.0f;
    metrics.toggle_icon_size = 16.0f;
    metrics.toggle_leading_padding = 13.0f;
    metrics.toggle_trailing_padding = 10.0f;
    metrics.toggle_divider_leading_gap = 9.0f;
    metrics.toggle_divider_trailing_gap = 6.0f;
    metrics.toggle_divider_height = 20.0f;
    metrics.toggle_title_height = 15.0f;
    metrics.toggle_status_height = 13.0f;

    metrics.section_label_height = 22.0f;
    metrics.output_device_row_height = 38.0f;
    metrics.output_row_horizontal_padding = 12.0f;
    metrics.output_icon_size = 16.0f;
    metrics.output_check_size = 14.0f;
    metrics.output_row_label_gap = 9.0f;
    metrics.output_row_title_height = 15.0f;
    metrics.output_row_detail_height = 14.0f;
    metrics.app_volume_row_height = 38.0f;
    metrics.wifi_header_height = 32.0f;
    metrics.wifi_switch_width = 36.0f;
    metrics.wifi_switch_height = 20.0f;
    metrics.wifi_key_area_height = 42.0f;
    metrics.wifi_key_gap = 4.0f;
    metrics.wifi_connect_width = 80.0f;
    metrics.wifi_forget_inset = 4.0f;
    metrics.wifi_forget_width = 64.0f;

    metrics.toggle_title_text_size = REACH_TEXT_SIZE_SMALL;
    metrics.toggle_status_text_size = REACH_TEXT_SIZE_XSMALL;
    metrics.identity_name_text_size = REACH_TEXT_SIZE_MEDIUM;
    metrics.identity_detail_text_size = REACH_TEXT_SIZE_XSMALL;
    metrics.overview_pill_value_text_size = REACH_TEXT_SIZE_XSMALL;
    metrics.pill_label_text_size = REACH_TEXT_SIZE_SMALL;
    metrics.section_label_text_size = REACH_TEXT_SIZE_XSMALL;
    metrics.output_row_title_text_size = REACH_TEXT_SIZE_SMALL;
    metrics.output_row_detail_text_size = REACH_TEXT_SIZE_XSMALL;
    metrics.wifi_title_text_size = REACH_TEXT_SIZE_MEDIUM;

    return metrics;
}

static constexpr reach_quick_settings_metrics reach_quick_settings_metrics_values =
    reach_quick_settings_make_metrics();

static inline float reach_quick_settings_metric_scale_value(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

static inline reach_quick_settings_metrics reach_quick_settings_metrics_for_scale(float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_values;
    metrics.surface_padding =
        reach_quick_settings_metric_scale_value(metrics.surface_padding, dpi_scale);
    metrics.content_padding =
        reach_quick_settings_metric_scale_value(metrics.content_padding, dpi_scale);
    metrics.panel_width = reach_quick_settings_metric_scale_value(metrics.panel_width, dpi_scale);
    metrics.banner_height =
        reach_quick_settings_metric_scale_value(metrics.banner_height, dpi_scale);
    metrics.avatar_size = reach_quick_settings_metric_scale_value(metrics.avatar_size, dpi_scale);
    metrics.avatar_ring = reach_quick_settings_metric_scale_value(metrics.avatar_ring, dpi_scale);
    metrics.avatar_overlap =
        reach_quick_settings_metric_scale_value(metrics.avatar_overlap, dpi_scale);
    metrics.avatar_inset = reach_quick_settings_metric_scale_value(metrics.avatar_inset, dpi_scale);
    metrics.identity_height =
        reach_quick_settings_metric_scale_value(metrics.identity_height, dpi_scale);
    metrics.identity_gap = reach_quick_settings_metric_scale_value(metrics.identity_gap, dpi_scale);
    metrics.identity_name_height =
        reach_quick_settings_metric_scale_value(metrics.identity_name_height, dpi_scale);
    metrics.identity_detail_height =
        reach_quick_settings_metric_scale_value(metrics.identity_detail_height, dpi_scale);
    metrics.rail_width = reach_quick_settings_metric_scale_value(metrics.rail_width, dpi_scale);
    metrics.rail_item_size =
        reach_quick_settings_metric_scale_value(metrics.rail_item_size, dpi_scale);
    metrics.rail_item_gap =
        reach_quick_settings_metric_scale_value(metrics.rail_item_gap, dpi_scale);
    metrics.page_shift = reach_quick_settings_metric_scale_value(metrics.page_shift, dpi_scale);
    metrics.overview_pill_height =
        reach_quick_settings_metric_scale_value(metrics.overview_pill_height, dpi_scale);
    metrics.overview_row_gap =
        reach_quick_settings_metric_scale_value(metrics.overview_row_gap, dpi_scale);
    metrics.overview_pill_icon_size =
        reach_quick_settings_metric_scale_value(metrics.overview_pill_icon_size, dpi_scale);
    metrics.overview_pill_icon_inset =
        reach_quick_settings_metric_scale_value(metrics.overview_pill_icon_inset, dpi_scale);
    metrics.overview_pill_value_inset =
        reach_quick_settings_metric_scale_value(metrics.overview_pill_value_inset, dpi_scale);
    metrics.overview_pill_value_width =
        reach_quick_settings_metric_scale_value(metrics.overview_pill_value_width, dpi_scale);
    metrics.pill_label_gap =
        reach_quick_settings_metric_scale_value(metrics.pill_label_gap, dpi_scale);
    metrics.toggle_grid_gap =
        reach_quick_settings_metric_scale_value(metrics.toggle_grid_gap, dpi_scale);
    metrics.toggle_height =
        reach_quick_settings_metric_scale_value(metrics.toggle_height, dpi_scale);
    metrics.toggle_gap = reach_quick_settings_metric_scale_value(metrics.toggle_gap, dpi_scale);
    metrics.toggle_icon_size =
        reach_quick_settings_metric_scale_value(metrics.toggle_icon_size, dpi_scale);
    metrics.toggle_leading_padding =
        reach_quick_settings_metric_scale_value(metrics.toggle_leading_padding, dpi_scale);
    metrics.toggle_trailing_padding =
        reach_quick_settings_metric_scale_value(metrics.toggle_trailing_padding, dpi_scale);
    metrics.toggle_divider_leading_gap =
        reach_quick_settings_metric_scale_value(metrics.toggle_divider_leading_gap, dpi_scale);
    metrics.toggle_divider_trailing_gap =
        reach_quick_settings_metric_scale_value(metrics.toggle_divider_trailing_gap, dpi_scale);
    metrics.toggle_divider_height =
        reach_quick_settings_metric_scale_value(metrics.toggle_divider_height, dpi_scale);
    metrics.toggle_title_height =
        reach_quick_settings_metric_scale_value(metrics.toggle_title_height, dpi_scale);
    metrics.toggle_status_height =
        reach_quick_settings_metric_scale_value(metrics.toggle_status_height, dpi_scale);
    metrics.section_label_height =
        reach_quick_settings_metric_scale_value(metrics.section_label_height, dpi_scale);
    metrics.output_device_row_height =
        reach_quick_settings_metric_scale_value(metrics.output_device_row_height, dpi_scale);
    metrics.output_row_horizontal_padding =
        reach_quick_settings_metric_scale_value(metrics.output_row_horizontal_padding, dpi_scale);
    metrics.output_icon_size =
        reach_quick_settings_metric_scale_value(metrics.output_icon_size, dpi_scale);
    metrics.output_check_size =
        reach_quick_settings_metric_scale_value(metrics.output_check_size, dpi_scale);
    metrics.output_row_label_gap =
        reach_quick_settings_metric_scale_value(metrics.output_row_label_gap, dpi_scale);
    metrics.output_row_title_height =
        reach_quick_settings_metric_scale_value(metrics.output_row_title_height, dpi_scale);
    metrics.output_row_detail_height =
        reach_quick_settings_metric_scale_value(metrics.output_row_detail_height, dpi_scale);
    metrics.app_volume_row_height =
        reach_quick_settings_metric_scale_value(metrics.app_volume_row_height, dpi_scale);
    metrics.wifi_header_height =
        reach_quick_settings_metric_scale_value(metrics.wifi_header_height, dpi_scale);
    metrics.wifi_switch_width =
        reach_quick_settings_metric_scale_value(metrics.wifi_switch_width, dpi_scale);
    metrics.wifi_switch_height =
        reach_quick_settings_metric_scale_value(metrics.wifi_switch_height, dpi_scale);
    metrics.wifi_key_area_height =
        reach_quick_settings_metric_scale_value(metrics.wifi_key_area_height, dpi_scale);
    metrics.wifi_key_gap = reach_quick_settings_metric_scale_value(metrics.wifi_key_gap, dpi_scale);
    metrics.wifi_connect_width =
        reach_quick_settings_metric_scale_value(metrics.wifi_connect_width, dpi_scale);
    metrics.wifi_forget_inset =
        reach_quick_settings_metric_scale_value(metrics.wifi_forget_inset, dpi_scale);
    metrics.wifi_forget_width =
        reach_quick_settings_metric_scale_value(metrics.wifi_forget_width, dpi_scale);
    metrics.toggle_title_text_size =
        reach_quick_settings_metric_scale_value(metrics.toggle_title_text_size, dpi_scale);
    metrics.toggle_status_text_size =
        reach_quick_settings_metric_scale_value(metrics.toggle_status_text_size, dpi_scale);
    metrics.identity_name_text_size =
        reach_quick_settings_metric_scale_value(metrics.identity_name_text_size, dpi_scale);
    metrics.identity_detail_text_size =
        reach_quick_settings_metric_scale_value(metrics.identity_detail_text_size, dpi_scale);
    metrics.overview_pill_value_text_size =
        reach_quick_settings_metric_scale_value(metrics.overview_pill_value_text_size, dpi_scale);
    metrics.pill_label_text_size =
        reach_quick_settings_metric_scale_value(metrics.pill_label_text_size, dpi_scale);
    metrics.section_label_text_size =
        reach_quick_settings_metric_scale_value(metrics.section_label_text_size, dpi_scale);
    metrics.output_row_title_text_size =
        reach_quick_settings_metric_scale_value(metrics.output_row_title_text_size, dpi_scale);
    metrics.output_row_detail_text_size =
        reach_quick_settings_metric_scale_value(metrics.output_row_detail_text_size, dpi_scale);
    metrics.wifi_title_text_size =
        reach_quick_settings_metric_scale_value(metrics.wifi_title_text_size, dpi_scale);
    return metrics;
}

#endif
