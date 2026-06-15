#include "reach/features/context_menu.h"

void reach_context_menu_build_dock_item_commands(int32_t pinned, int32_t has_path,
                                                 int32_t has_window, uint32_t *out_commands,
                                                 size_t *out_count)
{
    if (out_commands == nullptr || out_count == nullptr)
    {
        return;
    }

    size_t count = 0;
    out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_OPEN_NEW;
    if (has_path)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN;
    }
    if (pinned)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_UNPIN;
    }
    else if (has_path)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_PIN;
    }
    if (has_window)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_CLOSE;
    }
    *out_count = count;
}

void reach_context_menu_build_power_commands(uint32_t *out_commands, uint32_t *out_icon_ids,
                                             size_t *out_count)
{
    if (out_commands == nullptr || out_count == nullptr)
    {
        return;
    }

    out_commands[0] = REACH_CONTEXT_MENU_COMMAND_POWER_LOCK;
    out_commands[1] = REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP;
    out_commands[2] = REACH_CONTEXT_MENU_COMMAND_POWER_RESTART;
    out_commands[3] = REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN;
    out_commands[4] = REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT;
    out_commands[5] = REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS;
    if (out_icon_ids != nullptr)
    {
        out_icon_ids[0] = REACH_VECTOR_ICON_LOCK;
        out_icon_ids[1] = REACH_VECTOR_ICON_SLEEP;
        out_icon_ids[2] = REACH_VECTOR_ICON_RESTART;
        out_icon_ids[3] = REACH_VECTOR_ICON_SHUTDOWN;
        out_icon_ids[4] = REACH_VECTOR_ICON_SIGN_OUT;
        out_icon_ids[5] = REACH_VECTOR_ICON_SETTINGS;
    }
    *out_count = 6;
}

const uint16_t *reach_context_menu_command_text(uint32_t command)
{
    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_NEW)
    {
        return (const uint16_t *)L"Open Another Instance";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN)
    {
        return (const uint16_t *)L"Open as admin";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_UNPIN)
    {
        return (const uint16_t *)L"Unpin app from dock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_PIN)
    {
        return (const uint16_t *)L"Pin app to dock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE)
    {
        return (const uint16_t *)L"Close app";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK)
    {
        return (const uint16_t *)L"Lock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP)
    {
        return (const uint16_t *)L"Sleep";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_RESTART)
    {
        return (const uint16_t *)L"Restart";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN)
    {
        return (const uint16_t *)L"Shutdown";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT)
    {
        return (const uint16_t *)L"Sign out";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS)
    {
        return (const uint16_t *)L"Settings";
    }
    return (const uint16_t *)L"";
}
