#include "reach/features/launcher.h"
#include "reach/core/ui_layout.h"
#include "test_utf16.h"

#include <math.h>
#include <stdio.h>

static int expect_at(int condition, int line)
{
    if (!condition)
    {
        printf("launcher feature expectation failed at line %d\n", line);
        return 1;
    }
    return 0;
}

#define expect(condition) expect_at(condition, __LINE__)

static int expect_close_at(float actual, float expected, int line)
{
    if (fabsf(actual - expected) >= 0.01f)
    {
        printf("launcher feature expectation failed at line %d: %.3f != %.3f\n", line, actual,
               expected);
        return 1;
    }
    return 0;
}

#define expect_close(actual, expected) expect_close_at(actual, expected, __LINE__)

static void type_ascii(reach_launcher *launcher, const char *text)
{
    for (size_t index = 0; text[index] != 0; ++index)
    {
        reach_ui_event event = {};
        event.type = REACH_UI_EVENT_TEXT_CHAR;
        event.id = (uint32_t)(unsigned char)text[index];
        reach_launcher_text_event_result result = {};
        reach_launcher_handle_text_event(launcher, &event, &result);
    }
}

static int test_terminal_command_mode(void)
{
    int failed = 0;
    reach_launcher *launcher = nullptr;
    failed += expect(reach_launcher_create(&launcher) == REACH_OK);
    failed += expect(reach_launcher_toggle(launcher) == REACH_OK);

    const uint16_t terminal_icon_ref[] = {'w', 't', '.', 'e', 'x', 'e', 0};
    reach_launcher_set_terminal_icon_ref(launcher, terminal_icon_ref);

    type_ascii(launcher, "!");
    failed += expect(reach_launcher_result_count(launcher) == 1);
    failed += expect(reach_launcher_selected_result_index(launcher) == 0);
    const reach_launcher_result *result = reach_launcher_result_at(launcher, 0);
    failed += expect(result != nullptr);
    failed +=
        expect(result != nullptr && result->action == REACH_LAUNCHER_RESULT_RUN_TERMINAL_COMMAND);
    failed += expect(result != nullptr &&
                     reach_test_utf16_equals_ascii(result->title, "Run in Windows Terminal"));
    failed +=
        expect(result != nullptr && reach_test_utf16_equals_ascii(result->icon_path, "wt.exe"));
    failed += expect(result != nullptr && result->payload.terminal_command[0] == 0);

    const char *command = "ls | Where-Object { $_.Length -gt 0 }; Write-Output \"done\"";
    type_ascii(launcher, command);
    result = reach_launcher_result_at(launcher, 0);
    failed += expect(reach_launcher_result_count(launcher) == 1);
    failed += expect(reach_launcher_selected_result_index(launcher) == 0);
    failed += expect(result != nullptr && reach_test_utf16_equals_ascii(result->subtitle, command));
    failed += expect(result != nullptr &&
                     reach_test_utf16_equals_ascii(result->payload.terminal_command, command));

    reach_ui_event event = {};
    reach_ui_intent intent = {};
    event.type = REACH_UI_EVENT_ENTER;
    failed += expect(reach_launcher_handle_event(launcher, &event, &intent) == REACH_OK);
    failed += expect(intent.type == REACH_UI_INTENT_OPEN_LAUNCHER_RESULT);

    event = {};
    event.type = REACH_UI_EVENT_TEXT_EDIT;
    event.id = REACH_UI_EDIT_KEY_SELECT_ALL;
    reach_launcher_text_event_result text_result = {};
    reach_launcher_handle_text_event(launcher, &event, &text_result);
    type_ascii(launcher, "b");
    failed += expect(reach_launcher_result_count(launcher) == 0);

    reach_launcher_destroy(launcher);
    return failed;
}

int main()
{
    int failed = 0;

    reach_launcher *capsule = nullptr;
    failed += expect(reach_launcher_create(&capsule) == REACH_OK);
    const reach_launcher_state *state = reach_launcher_state_ptr(capsule);
    failed += expect(state != nullptr);

    reach_ui_event event = {};
    size_t activation_count = 0;
    const reach_ui_event_type *activation = reach_launcher_activation_events(&activation_count);
    failed += expect(activation_count == 1 && activation[0] == REACH_UI_EVENT_WINDOWS_KEY);
    failed += expect(reach_launcher_toggle(capsule) == REACH_OK);
    failed += expect(state->model.open == 1);

    const uint16_t query[] = {'b', 'r', 'a', 'v', 'e', 0};
    failed += expect(reach_launcher_set_query(capsule, query) == REACH_OK);
    failed += expect(state->model.query_length == 5);

    reach_search_candidate results[2] = {};
    results[0].name[0] = 'a';
    results[0].kind = REACH_SEARCH_RESULT_APP;
    results[1].name[0] = 'b';
    results[1].kind = REACH_SEARCH_RESULT_FILE;
    failed += expect(reach_launcher_set_results(capsule, results, 2) == REACH_OK);
    failed += expect(state->model.result_count == 2);
    failed += expect(state->model.selected_result_index == 0);

    reach_launcher_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {0.0f, 0.0f, 1480.0f, 900.0f};
    arrange.dpi_scale = 1.0f;
    failed += expect(reach_launcher_arrange(capsule, &arrange) == 1);

    reach_ui_layout_input layout_input = {};
    layout_input.monitor_bounds = arrange.monitor_bounds;
    layout_input.work_area = arrange.monitor_bounds;
    layout_input.dpi_scale = arrange.dpi_scale;
    layout_input.border_thickness =
        reach_theme_border_thickness(arrange.theme, arrange.dpi_scale);
    reach_launcher_layout expansion_layout = {};
    failed +=
        expect(reach_launcher_layout_compute(&state->model, &layout_input, &expansion_layout) ==
               REACH_OK);

    const reach_feature_capsule_ops *capsule_ops = reach_launcher_capsule_ops();
    reach_feature_surface_geometry geometry = {};
    capsule_ops->surface_geometry(capsule, &geometry);
    failed += expect_close(geometry.visible_bounds.height, expansion_layout.search_box.height);
    failed += expect_close(geometry.envelope_bounds.x, expansion_layout.bounds.x);
    failed += expect_close(geometry.envelope_bounds.y, expansion_layout.bounds.y);
    failed +=
        expect_close(geometry.envelope_bounds.height, expansion_layout.envelope_bounds.height);

    reach_feature_tick_result tick = {};
    capsule_ops->tick(capsule, 0.08, &tick);
    capsule_ops->surface_geometry(capsule, &geometry);
    failed += expect(geometry.visible_bounds.height > expansion_layout.search_box.height &&
                     geometry.visible_bounds.height < expansion_layout.bounds.height);
    failed += expect(tick.redraw == 1);
    tick = {};
    capsule_ops->tick(capsule, 0.16, &tick);
    capsule_ops->surface_geometry(capsule, &geometry);
    failed += expect_close(geometry.visible_bounds.height, expansion_layout.bounds.height);

    float result_row_height =
        expansion_layout.search_result_items.height / (float)state->model.result_count;
    reach_pointer_event pointer = {};
    pointer.kind = REACH_POINTER_EVENT_DOWN;
    pointer.button = REACH_POINTER_BUTTON_SECONDARY;
    pointer.x = (int32_t)(expansion_layout.search_result_items.x +
                          expansion_layout.search_result_items.width * 0.5f);
    pointer.y = (int32_t)(expansion_layout.search_result_items.y + result_row_height * 1.5f);
    reach_capsule_pointer_result pointer_result = {};
    capsule_ops->handle_pointer(capsule, &pointer, &pointer_result);
    failed += expect(pointer_result.handled == 1);
    pointer.kind = REACH_POINTER_EVENT_UP;
    capsule_ops->handle_pointer(capsule, &pointer, &pointer_result);
    failed += expect(pointer_result.action.kind == REACH_FEATURE_ACTION_REVEAL_SEARCH_RESULT);
    failed += expect(pointer_result.action.index == 1);

    event.type = REACH_UI_EVENT_ARROW_DOWN;
    failed += expect(reach_launcher_handle_event(capsule, &event, 0) == REACH_OK);
    failed += expect(state->model.selected_result_index == 1);
    event.type = REACH_UI_EVENT_ARROW_DOWN;
    failed += expect(reach_launcher_handle_event(capsule, &event, 0) == REACH_OK);
    failed += expect(state->model.selected_result_index == 1);
    event.type = REACH_UI_EVENT_ARROW_UP;
    failed += expect(reach_launcher_handle_event(capsule, &event, 0) == REACH_OK);
    failed += expect(state->model.selected_result_index == 0);

    reach_ui_intent intent = {};
    event.type = REACH_UI_EVENT_ENTER;
    failed += expect(reach_launcher_handle_event(capsule, &event, &intent) == REACH_OK);
    failed += expect(intent.type == REACH_UI_INTENT_OPEN_LAUNCHER_RESULT);

    event.type = REACH_UI_EVENT_ESCAPE;
    failed += expect(reach_launcher_handle_event(capsule, &event, 0) == REACH_OK);
    failed += expect(state->model.open == 0);
    failed += expect(reach_launcher_clear_results(capsule) == REACH_OK);
    failed += expect(state->model.result_count == 0);

    reach_launcher_destroy(capsule);
    failed += test_terminal_command_mode();
    return failed == 0 ? 0 : 1;
}
