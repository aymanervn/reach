#include "reach/features/common/section_reveal.h"

void reach_section_reveal_apply(reach_render_command_buffer *buffer, size_t first_command,
                                float progress, float maximum_y_offset)
{
    if (buffer == nullptr)
    {
        return;
    }
    if (progress < 0.0f)
    {
        progress = 0.0f;
    }
    if (progress > 1.0f)
    {
        progress = 1.0f;
    }

    float opacity = (progress - 0.10f) / 0.65f;
    if (opacity < 0.0f)
    {
        opacity = 0.0f;
    }
    if (opacity > 1.0f)
    {
        opacity = 1.0f;
    }

    float y_offset = (1.0f - opacity) * maximum_y_offset;
    for (size_t index = first_command; index < buffer->count; ++index)
    {
        reach_render_command *command = &buffer->commands[index];
        command->rect.y += y_offset;
        command->color.a *= opacity;
        command->text_color.a *= opacity;
        command->placeholder_color.a *= opacity;
        command->selection_color.a *= opacity;
    }
}
