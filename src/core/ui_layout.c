#include "reach/core/ui_layout.h"

static float reach_scale(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

static size_t reach_visible_launcher_result_count(const reach_launcher_model *launcher)
{
    if (launcher == 0)
    {
        return 0;
    }
    return launcher->result_count < REACH_SEARCH_VISIBLE_RESULTS ? launcher->result_count
                                                                 : REACH_SEARCH_VISIBLE_RESULTS;
}

static int32_t reach_launcher_error_row_visible(const reach_launcher_model *launcher)
{
    return launcher != 0 && launcher->search_error && launcher->result_count == 0 &&
           launcher->query_length > 0;
}

reach_result reach_dock_layout_compute(const reach_dock_model *dock,
                                       const reach_ui_layout_input *input,
                                       reach_dock_layout *out_layout)
{
    if (dock == 0 || input == 0 || out_layout == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    float scale = input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    float dock_height = reach_scale(dock->height, scale);
    float dock_width = reach_scale(dock->width, scale);
    float dock_x = input->work_area.x + (input->work_area.width - dock_width) * 0.5f;
    float dock_y =
        input->work_area.y + input->work_area.height - dock_height - reach_scale(18.0f, scale);

    out_layout->bounds.x = dock_x;
    out_layout->bounds.y = dock_y;
    out_layout->bounds.width = dock_width;
    out_layout->bounds.height = dock_height;
    out_layout->app_slot_count = input->pinned_app_count;
    out_layout->now_playing.x = 0.0f;
    out_layout->now_playing.y = 0.0f;
    out_layout->now_playing.width = 0.0f;
    out_layout->now_playing.height = 0.0f;

    float icon_size = reach_scale(dock->icon_size, scale);
    float gap = reach_scale(dock->gap, scale);
    float clock_width = reach_scale(92.0f, scale);
    float separator_width = reach_scale(1.0f, scale);
    float separator_height = dock_height * 0.56f;
    float top = (dock_height - icon_size) * 0.5f;
    float left = gap;

    for (size_t index = 0; index < input->pinned_app_count; ++index)
    {
        out_layout->app_slots[index].x = left + (icon_size + gap) * (float)index;
        out_layout->app_slots[index].y = top;
        out_layout->app_slots[index].width = icon_size;
        out_layout->app_slots[index].height = icon_size;
    }

    out_layout->tray_button.width = icon_size;
    out_layout->tray_button.height = icon_size;

    out_layout->quick_settings_button.width = icon_size;
    out_layout->quick_settings_button.height = icon_size;

    out_layout->power_button.width = icon_size;
    out_layout->power_button.height = icon_size;
    out_layout->power_button.x = dock_width - icon_size - gap;
    out_layout->power_button.y = top;

    out_layout->clock.width = clock_width;
    out_layout->clock.height = icon_size;
    out_layout->clock.x = out_layout->power_button.x - gap - clock_width;
    out_layout->clock.y = top;

    out_layout->system_separator.width = separator_width;
    out_layout->system_separator.height = separator_height;
    out_layout->system_separator.x = out_layout->clock.x - gap - separator_width;
    out_layout->system_separator.y = (dock_height - separator_height) * 0.5f;

    out_layout->quick_settings_button.x = out_layout->system_separator.x - gap - icon_size;
    out_layout->quick_settings_button.y = top;

    out_layout->tray_button.x = out_layout->quick_settings_button.x - icon_size;
    out_layout->tray_button.y = top;

    return REACH_OK;
}

reach_result reach_launcher_layout_compute(const reach_launcher_model *launcher,
                                           const reach_ui_layout_input *input,
                                           reach_launcher_layout *out_layout)
{
    if (launcher == 0 || input == 0 || out_layout == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    float scale = input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;

    out_layout->search_box.width = reach_scale(640.0f, scale);
    out_layout->search_box.height = reach_scale(52.0f, scale);
    out_layout->search_box.x =
        input->monitor_bounds.x +
        (input->monitor_bounds.width - out_layout->search_box.width) * 0.5f;
    out_layout->search_box.y =
        input->monitor_bounds.y +
        (input->monitor_bounds.height - out_layout->search_box.height) * 0.5f;

    float search_text_padding_x = reach_scale(12.0f, scale);
    float search_text_padding_y = reach_scale(8.0f, scale);
    float search_icon_size = reach_scale(20.0f, scale);
    out_layout->search_icon.width = search_icon_size;
    out_layout->search_icon.height = search_icon_size;
    out_layout->search_icon.x = out_layout->search_box.x + out_layout->search_box.width -
                                search_text_padding_x - search_icon_size;
    out_layout->search_icon.y = out_layout->search_box.y +
                                (out_layout->search_box.height - search_icon_size) * 0.5f +
                                reach_scale(1.0f, scale);
    out_layout->search_text_input.x = out_layout->search_box.x + search_text_padding_x;
    out_layout->search_text_input.y = out_layout->search_box.y + search_text_padding_y;
    out_layout->search_text_input.width = out_layout->search_icon.x - search_text_padding_x -
                                          out_layout->search_text_input.x;
    out_layout->search_text_input.height =
        out_layout->search_box.height - search_text_padding_y * 2.0f;

    out_layout->pinned_app_slot_count = 0;

    float launcher_icon = reach_scale(56.0f, scale);
    float launcher_gap = reach_scale(16.0f, scale);
    float total_width =
        (launcher_icon * (float)input->pinned_app_count) +
        (launcher_gap * (float)(input->pinned_app_count > 0 ? input->pinned_app_count - 1 : 0));
    float apps_x = input->monitor_bounds.x + (input->monitor_bounds.width - total_width) * 0.5f;
    float apps_y =
        out_layout->search_box.y + out_layout->search_box.height + reach_scale(24.0f, scale);

    for (size_t index = 0; index < input->pinned_app_count; ++index)
    {
        out_layout->pinned_app_slots[index].x =
            apps_x + (launcher_icon + launcher_gap) * (float)index;
        out_layout->pinned_app_slots[index].y = apps_y;
        out_layout->pinned_app_slots[index].width = launcher_icon;
        out_layout->pinned_app_slots[index].height = launcher_icon;
    }

    float search_results_top_padding = reach_scale(8.0f, scale);
    out_layout->search_results.x = out_layout->search_box.x;
    out_layout->search_results.y = out_layout->search_box.y + out_layout->search_box.height;
    out_layout->search_results.width = out_layout->search_box.width;
    size_t visible_result_count = reach_visible_launcher_result_count(launcher);
    size_t visible_row_count =
        visible_result_count > 0 ? visible_result_count
                                 : (reach_launcher_error_row_visible(launcher) ? 1 : 0);
    out_layout->search_results.height =
        visible_row_count > 0
            ? search_results_top_padding + reach_scale(56.0f * (float)visible_row_count, scale)
            : 0.0f;
    out_layout->search_result_items = out_layout->search_results;
    out_layout->search_result_items.y += search_results_top_padding;
    out_layout->search_result_items.height =
        out_layout->search_results.height > search_results_top_padding
            ? out_layout->search_results.height - search_results_top_padding
            : 0.0f;
    out_layout->search_result_scrollbar_track = (reach_rect_f32){0};
    out_layout->search_result_scrollbar_thumb = (reach_rect_f32){0};

    if (launcher->result_count > REACH_SEARCH_VISIBLE_RESULTS && visible_result_count > 0)
    {
        float gutter_width = reach_scale(18.0f, scale);
        float track_width = reach_scale(4.0f, scale);
        float track_padding_y = reach_scale(8.0f, scale);
        out_layout->search_result_items.width = out_layout->search_results.width - gutter_width;
        out_layout->search_result_scrollbar_track.x =
            out_layout->search_results.x + out_layout->search_results.width - gutter_width * 0.5f -
            track_width * 0.5f;
        out_layout->search_result_scrollbar_track.y =
            out_layout->search_result_items.y + track_padding_y;
        out_layout->search_result_scrollbar_track.width = track_width;
        out_layout->search_result_scrollbar_track.height =
            out_layout->search_result_items.height - track_padding_y * 2.0f;

        float min_thumb_height = reach_scale(22.0f, scale);
        reach_scrollbar_layout scrollbar = reach_scrollbar_compute_layout(
            &launcher->result_scrollbar, out_layout->search_result_scrollbar_track,
            (float)visible_result_count, (float)launcher->result_count, min_thumb_height);
        out_layout->search_result_scrollbar_track = scrollbar.track;
        out_layout->search_result_scrollbar_thumb = scrollbar.thumb;
    }
    out_layout->bounds = out_layout->search_box;

    if (launcher->result_count > 0 || reach_launcher_error_row_visible(launcher))
    {
        out_layout->bounds.x = out_layout->search_box.x;
        out_layout->bounds.width = out_layout->search_box.width;
        out_layout->bounds.y = out_layout->search_box.y;
        out_layout->bounds.height = out_layout->search_results.y +
                                    out_layout->search_results.height - out_layout->bounds.y;
    }
    else if (out_layout->pinned_app_slot_count > 0)
    {
        float launcher_left =
            apps_x < out_layout->search_box.x ? apps_x : out_layout->search_box.x;
        float launcher_right = apps_x + total_width;
        float search_right = out_layout->search_box.x + out_layout->search_box.width;
        if (search_right > launcher_right)
        {
            launcher_right = search_right;
        }
        float launcher_bottom = apps_y + launcher_icon;
        out_layout->bounds.x = launcher_left;
        out_layout->bounds.width = launcher_right - launcher_left;
        out_layout->bounds.y = out_layout->search_box.y;
        out_layout->bounds.height = launcher_bottom - out_layout->bounds.y;
    }

    return REACH_OK;
}
