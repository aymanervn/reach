#include "reach/features/context_menu.h"

void reach_context_menu_build_dock_item_commands(
    int32_t pinned,
    int32_t has_path,
    int32_t has_window,
    uint32_t *out_commands,
    size_t *out_count)
{
    if (out_commands == nullptr || out_count == nullptr) {
        return;
    }

    size_t count = 0;
    out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_OPEN_NEW;
    if (pinned) {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_UNPIN;
    } else if (has_path) {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_PIN;
    }
    if (has_window) {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_CLOSE;
    }
    *out_count = count;
}

const uint16_t *reach_context_menu_command_text(uint32_t command)
{
    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_NEW) {
        return (const uint16_t *)L"Open Another Instance";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_UNPIN) {
        return (const uint16_t *)L"Unpin app from dock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_PIN) {
        return (const uint16_t *)L"Pin app to dock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE) {
        return (const uint16_t *)L"Close app";
    }
    return (const uint16_t *)L"";
}
