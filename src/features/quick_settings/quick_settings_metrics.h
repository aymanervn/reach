#ifndef REACH_FEATURES_QUICK_SETTINGS_METRICS_H
#define REACH_FEATURES_QUICK_SETTINGS_METRICS_H

#include "reach/core/render_commands.h"

struct reach_quick_settings_metrics
{
    float content_padding;
    float text_padding;

    float system_grid_tile_height;
    float system_grid_gap;
    float system_grid_bottom_gap;
    float system_tile_icon_size;
    float system_tile_icon_gap;

    float pill_header_icon_size;
    float pill_header_label_gap;
    float pill_header_height;
    float pill_header_gap;
    float pill_height;
    float section_header_gap;

    float output_button_gap;
    float output_button_height;
    float output_title_height;
    float output_panel_gap;
    float output_device_row_height;
    float output_icon_size;
    float output_check_size;
    float output_row_horizontal_padding;
    float output_row_label_gap;

    float app_title_height;
    float app_title_gap;
    float app_panel_gap;
    float app_volume_row_height;
    float app_icon_size;
    float app_row_horizontal_padding;
    float app_row_label_gap;
    float app_row_slider_width;
    float app_row_slider_gap;
    float app_row_slider_line_height;
    float app_row_thumb_size;
    float app_row_percent_width;
    float app_row_percent_gap;

    float separator_inset;
    float separator_thickness;
    float expand_button_gap;
    float expand_button_height;
    float chevron_icon_size;

    float system_tile_text_size;
    float default_pill_label_text_size;
    float master_pill_label_text_size;
    int pill_label_text_weight;

    float output_row_primary_top;
    float output_row_primary_height;
    float output_row_primary_text_size;
    float output_row_secondary_top;
    float output_row_secondary_height;
    float output_row_secondary_text_size;

    float output_button_title_top;
    float output_button_title_height;
    float output_button_title_text_size;
    float output_button_device_top;
    float output_button_device_height;
    float output_button_device_text_size;

    float section_title_text_size;
    float app_row_text_size;
    float app_row_percent_text_size;
    float expand_button_text_size;
};

static constexpr reach_quick_settings_metrics reach_quick_settings_make_metrics()
{
    reach_quick_settings_metrics metrics = {};

    metrics.content_padding = 8.0f;
    metrics.text_padding = 12.0f;

    metrics.system_grid_tile_height = 46.0f;
    metrics.system_grid_gap = 8.0f;
    metrics.system_grid_bottom_gap = 10.0f;
    metrics.system_tile_icon_size = 20.0f;
    metrics.system_tile_icon_gap = 8.0f;

    metrics.pill_header_icon_size = 22.0f;
    metrics.pill_header_label_gap = 6.0f;
    metrics.pill_header_height = 16.0f;
    metrics.pill_header_gap = 12.0f;
    metrics.pill_height = 24.0f;
    metrics.section_header_gap = 26.0f;

    metrics.output_button_gap = 10.0f;
    metrics.output_button_height = 46.0f;
    metrics.output_title_height = 18.0f;
    metrics.output_panel_gap = 8.0f;
    metrics.output_device_row_height = 44.0f;
    metrics.output_icon_size = 16.0f;
    metrics.output_check_size = 14.0f;
    metrics.output_row_horizontal_padding = 12.0f;
    metrics.output_row_label_gap = 8.0f;

    metrics.app_title_height = 18.0f;
    metrics.app_title_gap = 12.0f;
    metrics.app_panel_gap = 8.0f;
    metrics.app_volume_row_height = 40.0f;
    metrics.app_icon_size = 18.0f;
    metrics.app_row_horizontal_padding = 12.0f;
    metrics.app_row_label_gap = 8.0f;
    metrics.app_row_slider_width = 78.0f;
    metrics.app_row_slider_gap = 10.0f;
    metrics.app_row_slider_line_height = 2.0f;
    metrics.app_row_thumb_size = 8.0f;
    metrics.app_row_percent_width = 38.0f;
    metrics.app_row_percent_gap = 10.0f;

    metrics.separator_inset = 12.0f;
    metrics.separator_thickness = 1.0f;
    metrics.expand_button_gap = 10.0f;
    metrics.expand_button_height = 34.0f;
    metrics.chevron_icon_size = 14.0f;

    metrics.system_tile_text_size = 12.0f;
    metrics.default_pill_label_text_size = 13.0f;
    metrics.master_pill_label_text_size = 14.0f;
    metrics.pill_label_text_weight = REACH_TEXT_WEIGHT_DEMIBOLD;

    metrics.output_row_primary_top = 5.0f;
    metrics.output_row_primary_height = 18.0f;
    metrics.output_row_primary_text_size = 12.0f;
    metrics.output_row_secondary_top = 22.0f;
    metrics.output_row_secondary_height = 15.0f;
    metrics.output_row_secondary_text_size = 10.0f;

    metrics.output_button_title_top = 6.0f;
    metrics.output_button_title_height = 16.0f;
    metrics.output_button_title_text_size = 11.0f;
    metrics.output_button_device_top = 23.0f;
    metrics.output_button_device_height = 18.0f;
    metrics.output_button_device_text_size = 13.0f;

    metrics.section_title_text_size = 12.0f;
    metrics.app_row_text_size = 12.0f;
    metrics.app_row_percent_text_size = 12.0f;
    metrics.expand_button_text_size = 13.0f;

    return metrics;
}

static constexpr reach_quick_settings_metrics reach_quick_settings_metrics_values =
    reach_quick_settings_make_metrics();

static inline float reach_quick_settings_metric_scale_value(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

static inline reach_quick_settings_metrics
reach_quick_settings_metrics_for_scale(float dpi_scale)
{
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_values;

    metrics.content_padding = reach_quick_settings_metric_scale_value(metrics.content_padding,
                                                                      dpi_scale);
    metrics.text_padding = reach_quick_settings_metric_scale_value(metrics.text_padding,
                                                                   dpi_scale);
    metrics.system_grid_tile_height =
        reach_quick_settings_metric_scale_value(metrics.system_grid_tile_height, dpi_scale);
    metrics.system_grid_gap =
        reach_quick_settings_metric_scale_value(metrics.system_grid_gap, dpi_scale);
    metrics.system_grid_bottom_gap =
        reach_quick_settings_metric_scale_value(metrics.system_grid_bottom_gap, dpi_scale);
    metrics.system_tile_icon_size =
        reach_quick_settings_metric_scale_value(metrics.system_tile_icon_size, dpi_scale);
    metrics.system_tile_icon_gap =
        reach_quick_settings_metric_scale_value(metrics.system_tile_icon_gap, dpi_scale);
    metrics.pill_header_icon_size =
        reach_quick_settings_metric_scale_value(metrics.pill_header_icon_size, dpi_scale);
    metrics.pill_header_label_gap =
        reach_quick_settings_metric_scale_value(metrics.pill_header_label_gap, dpi_scale);
    metrics.pill_header_height =
        reach_quick_settings_metric_scale_value(metrics.pill_header_height, dpi_scale);
    metrics.pill_header_gap =
        reach_quick_settings_metric_scale_value(metrics.pill_header_gap, dpi_scale);
    metrics.pill_height = reach_quick_settings_metric_scale_value(metrics.pill_height, dpi_scale);
    metrics.section_header_gap =
        reach_quick_settings_metric_scale_value(metrics.section_header_gap, dpi_scale);
    metrics.output_button_gap =
        reach_quick_settings_metric_scale_value(metrics.output_button_gap, dpi_scale);
    metrics.output_button_height =
        reach_quick_settings_metric_scale_value(metrics.output_button_height, dpi_scale);
    metrics.output_title_height =
        reach_quick_settings_metric_scale_value(metrics.output_title_height, dpi_scale);
    metrics.output_panel_gap =
        reach_quick_settings_metric_scale_value(metrics.output_panel_gap, dpi_scale);
    metrics.output_device_row_height =
        reach_quick_settings_metric_scale_value(metrics.output_device_row_height, dpi_scale);
    metrics.output_icon_size =
        reach_quick_settings_metric_scale_value(metrics.output_icon_size, dpi_scale);
    metrics.output_check_size =
        reach_quick_settings_metric_scale_value(metrics.output_check_size, dpi_scale);
    metrics.output_row_horizontal_padding =
        reach_quick_settings_metric_scale_value(metrics.output_row_horizontal_padding, dpi_scale);
    metrics.output_row_label_gap =
        reach_quick_settings_metric_scale_value(metrics.output_row_label_gap, dpi_scale);
    metrics.app_title_height =
        reach_quick_settings_metric_scale_value(metrics.app_title_height, dpi_scale);
    metrics.app_title_gap =
        reach_quick_settings_metric_scale_value(metrics.app_title_gap, dpi_scale);
    metrics.app_panel_gap =
        reach_quick_settings_metric_scale_value(metrics.app_panel_gap, dpi_scale);
    metrics.app_volume_row_height =
        reach_quick_settings_metric_scale_value(metrics.app_volume_row_height, dpi_scale);
    metrics.app_icon_size =
        reach_quick_settings_metric_scale_value(metrics.app_icon_size, dpi_scale);
    metrics.app_row_horizontal_padding =
        reach_quick_settings_metric_scale_value(metrics.app_row_horizontal_padding, dpi_scale);
    metrics.app_row_label_gap =
        reach_quick_settings_metric_scale_value(metrics.app_row_label_gap, dpi_scale);
    metrics.app_row_slider_width =
        reach_quick_settings_metric_scale_value(metrics.app_row_slider_width, dpi_scale);
    metrics.app_row_slider_gap =
        reach_quick_settings_metric_scale_value(metrics.app_row_slider_gap, dpi_scale);
    metrics.app_row_slider_line_height =
        reach_quick_settings_metric_scale_value(metrics.app_row_slider_line_height, dpi_scale);
    metrics.app_row_thumb_size =
        reach_quick_settings_metric_scale_value(metrics.app_row_thumb_size, dpi_scale);
    metrics.app_row_percent_width =
        reach_quick_settings_metric_scale_value(metrics.app_row_percent_width, dpi_scale);
    metrics.app_row_percent_gap =
        reach_quick_settings_metric_scale_value(metrics.app_row_percent_gap, dpi_scale);
    metrics.separator_inset =
        reach_quick_settings_metric_scale_value(metrics.separator_inset, dpi_scale);
    metrics.separator_thickness =
        reach_quick_settings_metric_scale_value(metrics.separator_thickness, dpi_scale);
    metrics.expand_button_gap =
        reach_quick_settings_metric_scale_value(metrics.expand_button_gap, dpi_scale);
    metrics.expand_button_height =
        reach_quick_settings_metric_scale_value(metrics.expand_button_height, dpi_scale);
    metrics.chevron_icon_size =
        reach_quick_settings_metric_scale_value(metrics.chevron_icon_size, dpi_scale);
    metrics.system_tile_text_size =
        reach_quick_settings_metric_scale_value(metrics.system_tile_text_size, dpi_scale);
    metrics.default_pill_label_text_size =
        reach_quick_settings_metric_scale_value(metrics.default_pill_label_text_size, dpi_scale);
    metrics.master_pill_label_text_size =
        reach_quick_settings_metric_scale_value(metrics.master_pill_label_text_size, dpi_scale);
    metrics.output_row_primary_top =
        reach_quick_settings_metric_scale_value(metrics.output_row_primary_top, dpi_scale);
    metrics.output_row_primary_height =
        reach_quick_settings_metric_scale_value(metrics.output_row_primary_height, dpi_scale);
    metrics.output_row_primary_text_size =
        reach_quick_settings_metric_scale_value(metrics.output_row_primary_text_size, dpi_scale);
    metrics.output_row_secondary_top =
        reach_quick_settings_metric_scale_value(metrics.output_row_secondary_top, dpi_scale);
    metrics.output_row_secondary_height =
        reach_quick_settings_metric_scale_value(metrics.output_row_secondary_height, dpi_scale);
    metrics.output_row_secondary_text_size =
        reach_quick_settings_metric_scale_value(metrics.output_row_secondary_text_size, dpi_scale);
    metrics.output_button_title_top =
        reach_quick_settings_metric_scale_value(metrics.output_button_title_top, dpi_scale);
    metrics.output_button_title_height =
        reach_quick_settings_metric_scale_value(metrics.output_button_title_height, dpi_scale);
    metrics.output_button_title_text_size =
        reach_quick_settings_metric_scale_value(metrics.output_button_title_text_size, dpi_scale);
    metrics.output_button_device_top =
        reach_quick_settings_metric_scale_value(metrics.output_button_device_top, dpi_scale);
    metrics.output_button_device_height =
        reach_quick_settings_metric_scale_value(metrics.output_button_device_height, dpi_scale);
    metrics.output_button_device_text_size =
        reach_quick_settings_metric_scale_value(metrics.output_button_device_text_size, dpi_scale);
    metrics.section_title_text_size =
        reach_quick_settings_metric_scale_value(metrics.section_title_text_size, dpi_scale);
    metrics.app_row_text_size =
        reach_quick_settings_metric_scale_value(metrics.app_row_text_size, dpi_scale);
    metrics.app_row_percent_text_size =
        reach_quick_settings_metric_scale_value(metrics.app_row_percent_text_size, dpi_scale);
    metrics.expand_button_text_size =
        reach_quick_settings_metric_scale_value(metrics.expand_button_text_size, dpi_scale);

    return metrics;
}

#endif
