#include "reach/features/launcher.h"

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main()
{
    int failed = 0;

    reach_ui_state state = {};
    reach_ui_state_init(&state);
    (void)reach_ui_state_open_launcher(&state);
    const uint16_t query[] = { 'b','r','a','v','e',0 };
    failed += expect(reach_ui_state_set_query(&state, query) == REACH_OK);
    failed += expect(reach_ui_state_move_launcher_caret_left(&state, 0) == REACH_OK);

    reach_launcher_layout layout = {};
    layout.bounds = { 0.0f, 0.0f, 640.0f, 52.0f };
    layout.search_box = layout.bounds;

    reach_render_command_buffer buffer = {};
    reach_launcher_render_input input = {};
    input.theme = reach_theme_default();
    input.state = &state;
    input.layout = &layout;
    input.text_alignment_leading = 0;

    failed += expect(reach_launcher_build_render_commands(&input, &buffer) == REACH_OK);

    int found_caret = 0;
    int found_border = 0;
    for (size_t index = 0; index < buffer.count; ++index) {
        if (buffer.commands[index].type == REACH_RENDER_COMMAND_TEXT_CARET) {
            found_caret = 1;
            failed += expect(buffer.commands[index].text[0] == 'b');
            failed += expect(buffer.commands[index].text[1] == 'r');
            failed += expect(buffer.commands[index].text[2] == 'a');
            failed += expect(buffer.commands[index].text[3] == 'v');
            failed += expect(buffer.commands[index].text[4] == 0);
        }
        if (buffer.commands[index].type == REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE) {
            found_border = 1;
        }
    }
    failed += expect(found_caret);
    failed += expect(found_border);

    return failed == 0 ? 0 : 1;
}
