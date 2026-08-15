#include "reach/features/top_bar.h"

#include "top_bar_common.h"

static void reach_top_bar_push_pill(const reach_theme *theme,
                                    reach_render_command_buffer *commands, reach_rect_f32 pill)
{
    float radius = pill.height * 0.5f;

    reach_render_command background = {};
    background.type = REACH_RENDER_COMMAND_RECT;
    background.rect = pill;
    background.color = theme->dock_background;
    background.radius = radius;
    reach_render_command_buffer_push(commands, &background);

    if (theme->border_thickness <= 0.0f || theme->dock_border.a <= 0.0f)
    {
        return;
    }

    reach_render_command border = {};
    border.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    border.rect = reach_top_bar_rect(pill.x + theme->border_thickness * 0.5f,
                                     pill.y + theme->border_thickness * 0.5f,
                                     pill.width - theme->border_thickness,
                                     pill.height - theme->border_thickness);
    border.color = theme->dock_border;
    border.radius = radius;
    border.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(commands, &border);
}

reach_result reach_top_bar_append_render_commands(reach_top_bar *top_bar,
                                                  const reach_top_bar_render_context *ctx,
                                                  reach_render_command_buffer *out_commands)
{
    if (top_bar == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);

    const reach_top_bar_layout *layout = &top_bar->state.layout;
    for (size_t index = 0; index < REACH_TOP_BAR_PILL_COUNT; ++index)
    {
        if (!layout->pill_visible[index])
        {
            continue;
        }
        reach_top_bar_push_pill(ctx->theme, out_commands, layout->pills[index]);
    }

    return REACH_OK;
}
