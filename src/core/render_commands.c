#include "reach/core/render_commands.h"

static void reach_copy_text(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    size_t index = 0;
    if (dst == 0 || dst_count == 0)
    {
        return;
    }
    if (src == 0)
    {
        dst[0] = 0;
        return;
    }
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = src[index];
        ++index;
    }
    dst[index] = 0;
}

static reach_rect_f32 reach_dock_child_to_screen(const reach_ui_layout *layout, reach_rect_f32 rect)
{
    rect.x += layout->dock.bounds.x;
    rect.y += layout->dock.bounds.y;
    return rect;
}

void reach_render_command_buffer_clear(reach_render_command_buffer *buffer)
{
    if (buffer != 0)
    {
        buffer->count = 0;
    }
}

reach_result reach_render_command_buffer_push(reach_render_command_buffer *buffer,
                                              const reach_render_command *command)
{
    if (buffer == 0 || command == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (buffer->count >= REACH_MAX_RENDER_COMMANDS)
    {
        return REACH_ERROR;
    }

    buffer->commands[buffer->count] = *command;
    buffer->count += 1;
    return REACH_OK;
}

reach_result reach_ui_build_render_commands(const reach_ui_state *state,
                                            const reach_ui_layout *layout,
                                            reach_render_command_buffer *buffer)
{
    if (state == 0 || layout == 0 || buffer == 0)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(buffer);

    reach_render_command command = {0};
    if (state->launcher.open)
    {
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = layout->launcher.search_box;
        command.color.r = 0.08f;
        command.color.g = 0.08f;
        command.color.b = 0.09f;
        command.color.a = 0.95f;
        command.radius = 10.0f;
        reach_render_command_buffer_push(buffer, &command);

        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect = layout->launcher.search_box;
        command.color.r = 1.0f;
        command.color.g = 1.0f;
        command.color.b = 1.0f;
        command.color.a = state->launcher.query_length > 0 ? 0.95f : 0.60f;
        reach_copy_text(command.text, 260,
                        state->launcher.query_length > 0 ? state->launcher.query
                                                         : (const uint16_t *)L"Search");
        reach_render_command_buffer_push(buffer, &command);
    }

    if (state->dock.visible)
    {
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = layout->dock.bounds;
        command.color.r = 0.08f;
        command.color.g = 0.08f;
        command.color.b = 0.09f;
        command.color.a = 0.92f;
        command.radius = 14.0f;
        reach_render_command_buffer_push(buffer, &command);

        for (size_t index = 0; index < layout->dock.app_slot_count; ++index)
        {
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect = reach_dock_child_to_screen(layout, layout->dock.app_slots[index]);
            reach_copy_text(command.text, 260, state->pinned_apps[index].icon_ref);
            reach_render_command_buffer_push(buffer, &command);
        }

        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = reach_dock_child_to_screen(layout, layout->dock.tray_button);
        command.color.r = 0.18f;
        command.color.g = 0.18f;
        command.color.b = 0.20f;
        command.color.a = 1.0f;
        command.radius = 8.0f;
        reach_render_command_buffer_push(buffer, &command);
    }

    return REACH_OK;
}
