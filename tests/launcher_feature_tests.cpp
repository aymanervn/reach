#include "reach/features/launcher.h"

static int expect(int condition)
{
    return condition ? 0 : 1;
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
    return failed == 0 ? 0 : 1;
}
