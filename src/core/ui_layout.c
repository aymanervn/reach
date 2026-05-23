#include "reach/core/ui_layout.h"

static float reach_scale(float value, float dpi_scale)
{
    return value * (dpi_scale > 0.0f ? dpi_scale : 1.0f);
}

reach_result reach_ui_layout_compute(const reach_ui_state *state, const reach_ui_layout_input *input, reach_ui_layout *out_layout)
{
    if (state == 0 || input == 0 || out_layout == 0) {
        return REACH_INVALID_ARGUMENT;
    }

    float scale = input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    float dock_height = reach_scale(state->dock.height, scale);
    float dock_width = reach_scale(state->dock.width, scale);
    float dock_x = input->work_area.x + (input->work_area.width - dock_width) * 0.5f;
    float dock_y = input->work_area.y + input->work_area.height - dock_height - reach_scale(18.0f, scale);
    out_layout->dock.bounds.x = dock_x;
    out_layout->dock.bounds.y = dock_y;
    out_layout->dock.bounds.width = dock_width;
    out_layout->dock.bounds.height = dock_height;
    out_layout->dock.app_slot_count = state->pinned_app_count;

    float icon_size = reach_scale(state->dock.icon_size, scale);
    float gap = reach_scale(state->dock.gap, scale);
    float clock_width = reach_scale(92.0f, scale);
    float separator_width = reach_scale(1.0f, scale);
    float separator_height = dock_height * 0.56f;
    float left = dock_x + gap;
    float top = dock_y + (dock_height - icon_size) * 0.5f;
    for (size_t index = 0; index < state->pinned_app_count; ++index) {
        out_layout->dock.app_slots[index].x = left + (icon_size + gap) * (float)index;
        out_layout->dock.app_slots[index].y = top;
        out_layout->dock.app_slots[index].width = icon_size;
        out_layout->dock.app_slots[index].height = icon_size;
    }

    out_layout->dock.tray_button.width = icon_size;
    out_layout->dock.tray_button.height = icon_size;
    out_layout->dock.power_button.width = icon_size;
    out_layout->dock.power_button.height = icon_size;
    out_layout->dock.power_button.x = dock_x + dock_width - icon_size - gap;
    out_layout->dock.power_button.y = top;
    out_layout->dock.clock.width = clock_width;
    out_layout->dock.clock.height = icon_size;
    out_layout->dock.clock.x = out_layout->dock.power_button.x - gap - clock_width;
    out_layout->dock.clock.y = top;
    out_layout->dock.system_separator.width = separator_width;
    out_layout->dock.system_separator.height = separator_height;
    out_layout->dock.system_separator.x = out_layout->dock.clock.x - gap - separator_width;
    out_layout->dock.system_separator.y = dock_y + (dock_height - separator_height) * 0.5f;
    out_layout->dock.tray_button.x = out_layout->dock.system_separator.x - gap - icon_size;
    out_layout->dock.tray_button.y = top;

    out_layout->launcher.search_box.width = reach_scale(640.0f, scale);
    out_layout->launcher.search_box.height = reach_scale(52.0f, scale);
    out_layout->launcher.search_box.x = input->monitor_bounds.x + (input->monitor_bounds.width - out_layout->launcher.search_box.width) * 0.5f;
    out_layout->launcher.search_box.y = input->monitor_bounds.y + (input->monitor_bounds.height - out_layout->launcher.search_box.height) * 0.5f;
    out_layout->launcher.pinned_app_slot_count =
        state->launcher.open && state->launcher.query_length == 0
            ? state->pinned_app_count
            : 0;

    float launcher_icon = reach_scale(56.0f, scale);
    float launcher_gap = reach_scale(16.0f, scale);
    float total_width = (launcher_icon * (float)state->pinned_app_count) + (launcher_gap * (float)(state->pinned_app_count > 0 ? state->pinned_app_count - 1 : 0));
    float apps_x = input->monitor_bounds.x + (input->monitor_bounds.width - total_width) * 0.5f;
    float apps_y = out_layout->launcher.search_box.y + out_layout->launcher.search_box.height + reach_scale(24.0f, scale);
    for (size_t index = 0; index < state->pinned_app_count; ++index) {
        out_layout->launcher.pinned_app_slots[index].x = apps_x + (launcher_icon + launcher_gap) * (float)index;
        out_layout->launcher.pinned_app_slots[index].y = apps_y;
        out_layout->launcher.pinned_app_slots[index].width = launcher_icon;
        out_layout->launcher.pinned_app_slots[index].height = launcher_icon;
    }

    out_layout->launcher.search_results.x = out_layout->launcher.search_box.x;
    out_layout->launcher.search_results.y = out_layout->launcher.search_box.y + out_layout->launcher.search_box.height + reach_scale(8.0f, scale);
    out_layout->launcher.search_results.width = out_layout->launcher.search_box.width;
    out_layout->launcher.search_results.height = reach_scale(120.0f, scale);
    out_layout->launcher.bounds = out_layout->launcher.search_box;
    if (out_layout->launcher.pinned_app_slot_count > 0) {
        float launcher_left = apps_x < out_layout->launcher.search_box.x ? apps_x : out_layout->launcher.search_box.x;
        float launcher_right = apps_x + total_width;
        float search_right = out_layout->launcher.search_box.x + out_layout->launcher.search_box.width;
        if (search_right > launcher_right) {
            launcher_right = search_right;
        }
        float launcher_bottom = apps_y + launcher_icon;
        out_layout->launcher.bounds.x = launcher_left;
        out_layout->launcher.bounds.width = launcher_right - launcher_left;
        out_layout->launcher.bounds.y = out_layout->launcher.search_box.y;
        out_layout->launcher.bounds.height = launcher_bottom - out_layout->launcher.bounds.y;
    }
    return REACH_OK;
}
