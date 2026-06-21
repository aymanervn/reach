#include "reach/core/render_commands.h"
#include "reach/core/ui_events.h"
#include "reach/core/ui_layout.h"

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main(void)
{
    int failed = 0;

    reach_ui_state state;
    reach_ui_state_init(&state);

    reach_pinned_app_model apps[2] = {0};
    apps[0].id = 1;
    apps[1].id = 2;
    failed += expect(reach_ui_state_set_pinned_apps(&state, apps, 2) == REACH_OK);
    failed += expect(state.pinned_app_count == 2);

    reach_ui_event event = {0};
    event.type = REACH_UI_EVENT_WINDOWS_KEY;
    failed += expect(reach_ui_handle_event(&state, &event, 0) == REACH_OK);
    failed += expect(state.launcher.open == 1);
    failed += expect(!reach_ui_state_should_show_pinned_apps(&state));

    uint16_t query[] = {'a', 0};
    failed += expect(reach_ui_state_set_query(&state, query) == REACH_OK);

    reach_ui_layout_input input = {0};
    input.monitor_bounds.x = 0.0f;
    input.monitor_bounds.y = 0.0f;
    input.monitor_bounds.width = 1920.0f;
    input.monitor_bounds.height = 1080.0f;
    input.work_area = input.monitor_bounds;
    input.dpi_scale = 1.0f;

    reach_ui_layout layout = {0};
    failed += expect(reach_ui_layout_compute(&state, &input, &layout) == REACH_OK);
    failed += expect(layout.dock.bounds.y > 900.0f);
    failed += expect(layout.launcher.search_box.y == 514.0f);
    failed += expect(layout.launcher.bounds.width == layout.launcher.search_box.width);
    failed += expect(layout.launcher.bounds.height == layout.launcher.search_box.height);
    failed += expect(layout.launcher.pinned_app_slot_count == 0);

    reach_search_candidate results[2] = {0};
    results[0].name[0] = 'a';
    results[0].name[1] = '.';
    results[0].name[2] = 'e';
    results[0].name[3] = 'x';
    results[0].name[4] = 'e';
    results[0].kind = REACH_SEARCH_RESULT_APP;
    results[1].name[0] = 'b';
    results[1].kind = REACH_SEARCH_RESULT_FILE;
    failed += expect(reach_ui_state_set_launcher_results(&state, results, 2) == REACH_OK);
    failed += expect(state.launcher.result_count == 2);
    failed += expect(state.launcher.selected_result_index == 0);
    failed += expect(reach_ui_layout_compute(&state, &input, &layout) == REACH_OK);
    failed += expect(layout.launcher.search_results.y ==
                     layout.launcher.search_box.y + layout.launcher.search_box.height);
    failed += expect(layout.launcher.search_results.height == 120.0f);
    failed +=
        expect(layout.launcher.search_result_items.y == layout.launcher.search_results.y + 8.0f);
    failed += expect(layout.launcher.search_result_items.height == 112.0f);
    failed += expect(layout.launcher.bounds.height == 172.0f);
    event.type = REACH_UI_EVENT_ARROW_DOWN;
    event.modifiers = 0;
    failed += expect(reach_ui_handle_event(&state, &event, 0) == REACH_OK);
    failed += expect(state.launcher.selected_result_index == 1);
    event.type = REACH_UI_EVENT_ARROW_DOWN;
    failed += expect(reach_ui_handle_event(&state, &event, 0) == REACH_OK);
    failed += expect(state.launcher.selected_result_index == 1);
    event.type = REACH_UI_EVENT_ARROW_UP;
    failed += expect(reach_ui_handle_event(&state, &event, 0) == REACH_OK);
    failed += expect(state.launcher.selected_result_index == 0);
    failed += expect(reach_ui_state_clear_launcher_results(&state) == REACH_OK);
    failed += expect(state.launcher.result_count == 0);
    failed += expect(layout.dock.tray_button.x + layout.dock.tray_button.width <
                     layout.dock.system_separator.x);
    failed += expect(layout.dock.system_separator.x + layout.dock.system_separator.width <
                     layout.dock.clock.x);
    failed += expect(layout.dock.clock.x + layout.dock.clock.width < layout.dock.power_button.x);

    reach_render_command_buffer buffer = {0};
    failed += expect(reach_ui_build_render_commands(&state, &layout, &buffer) == REACH_OK);
    failed += expect(buffer.count > 0);

    return failed == 0 ? 0 : 1;
}
