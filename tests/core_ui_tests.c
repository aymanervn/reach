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
    failed += expect(reach_ui_state_should_show_pinned_apps(&state));

    event.type = REACH_UI_EVENT_TEXT;
    event.text[0] = 'a';
    event.text[1] = 0;
    failed += expect(reach_ui_handle_event(&state, &event, 0) == REACH_OK);
    failed += expect(reach_ui_state_should_show_search_placeholder(&state));

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
    failed += expect(layout.dock.tray_button.x + layout.dock.tray_button.width < layout.dock.system_separator.x);
    failed += expect(layout.dock.system_separator.x + layout.dock.system_separator.width < layout.dock.clock.x);
    failed += expect(layout.dock.clock.x + layout.dock.clock.width < layout.dock.power_button.x);

    reach_render_command_buffer buffer = {0};
    failed += expect(reach_ui_build_render_commands(&state, &layout, &buffer) == REACH_OK);
    failed += expect(buffer.count > 0);

    return failed == 0 ? 0 : 1;
}
