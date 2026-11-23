#include "screenshot_common.h"

#include "reach/core/typography.h"

#include <math.h>
#include <stdio.h>

static reach_result reach_screenshot_push_rect(reach_render_command_buffer *commands,
                                               reach_rect_f32 rect, reach_color color, float radius)
{
    if (rect.width <= 0.0f || rect.height <= 0.0f)
    {
        return REACH_OK;
    }
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    return reach_render_command_buffer_push(commands, &command);
}

static reach_result reach_screenshot_push_scrim(reach_render_command_buffer *commands,
                                                reach_rect_f32 surface, reach_rect_f32 hole,
                                                reach_color color)
{
    if (hole.width <= 0.0f || hole.height <= 0.0f)
    {
        return reach_screenshot_push_rect(commands, surface, color, 0.0f);
    }
    float hole_right = hole.x + hole.width;
    float hole_bottom = hole.y + hole.height;
    reach_rect_f32 bands[4] = {
        {0.0f, 0.0f, surface.width, hole.y},
        {0.0f, hole_bottom, surface.width, surface.height - hole_bottom},
        {0.0f, hole.y, hole.x, hole.height},
        {hole_right, hole.y, surface.width - hole_right, hole.height},
    };
    for (const reach_rect_f32 &band : bands)
    {
        reach_result result = reach_screenshot_push_rect(commands, band, color, 0.0f);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
}

static reach_result reach_screenshot_push_outline(reach_render_command_buffer *commands,
                                                  reach_rect_f32 rect, float thickness,
                                                  reach_color color)
{
    reach_rect_f32 edges[4] = {
        {rect.x - thickness, rect.y - thickness, rect.width + thickness * 2.0f, thickness},
        {rect.x - thickness, rect.y + rect.height, rect.width + thickness * 2.0f, thickness},
        {rect.x - thickness, rect.y, thickness, rect.height},
        {rect.x + rect.width, rect.y, thickness, rect.height},
    };
    for (const reach_rect_f32 &edge : edges)
    {
        reach_result result = reach_screenshot_push_rect(commands, edge, color, 0.0f);
        if (result != REACH_OK)
        {
            return result;
        }
    }
    return REACH_OK;
}

static reach_result reach_screenshot_push_size_label(reach_render_command_buffer *commands,
                                                     reach_rect_f32 surface,
                                                     reach_rect_f32 selection,
                                                     const reach_theme *theme, float scale)
{
    char ascii[32] = {};
    int length =
        snprintf(ascii, sizeof(ascii), "%d x %d", (int)selection.width, (int)selection.height);
    if (length <= 0)
    {
        return REACH_OK;
    }
    reach_render_command text = {};
    for (int index = 0; index < length && index < 31; ++index)
    {
        text.text[index] = ascii[index] == 'x' ? (uint16_t)0x00D7 : (uint16_t)ascii[index];
    }

    float height = 22.0f * scale;
    float width = (8.0f + 7.0f * (float)length) * scale;
    float gap = 8.0f * scale;
    float x = selection.x;
    float y = selection.y + selection.height + gap;
    if (y + height > surface.height)
    {
        y = selection.y - gap - height;
    }
    if (y < 0.0f)
    {
        y = selection.y + gap;
        x = selection.x + gap;
    }
    if (x + width > surface.width)
    {
        x = surface.width - width;
    }
    if (x < 0.0f)
    {
        x = 0.0f;
    }
    reach_rect_f32 pill = {roundf(x), roundf(y), width, height};
    reach_result result = reach_screenshot_push_rect(
        commands, pill, theme->screenshot_label_background, height * 0.5f);
    if (result != REACH_OK)
    {
        return result;
    }
    text.type = REACH_RENDER_COMMAND_TEXT;
    text.rect = pill;
    text.text_size = REACH_TEXT_SIZE_SMALL * scale;
    text.text_weight = REACH_TEXT_WEIGHT_NORMAL;
    text.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
    text.color = theme->text.primary;
    return reach_render_command_buffer_push(commands, &text);
}

reach_result reach_screenshot_append_render_commands(const reach_screenshot *screenshot,
                                                     const reach_theme *theme, float dpi_scale,
                                                     reach_render_command_buffer *out_commands)
{
    if (screenshot == nullptr || theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    const reach_screen_image *active = reach_screenshot_active(screenshot);
    if (!screenshot->open || active == nullptr)
    {
        return REACH_OK;
    }
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    reach_rect_f32 surface = {0.0f, 0.0f, screenshot->bounds.width, screenshot->bounds.height};

    reach_render_command frame = {};
    frame.type = REACH_RENDER_COMMAND_ICON;
    frame.rect = surface;
    frame.icon_id = active->image_id;
    frame.color = {1.0f, 1.0f, 1.0f, 1.0f};
    reach_result result = reach_render_command_buffer_push(out_commands, &frame);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_rect_f32 selection = {};
    int32_t has_selection = reach_screenshot_selection_rect(screenshot, &selection);
    if (has_selection)
    {
        selection.x -= screenshot->bounds.x;
        selection.y -= screenshot->bounds.y;
    }
    result = reach_screenshot_push_scrim(out_commands, surface,
                                         has_selection ? selection : reach_rect_f32{},
                                         theme->screenshot_scrim);
    if (result != REACH_OK || !has_selection)
    {
        return result;
    }
    result = reach_screenshot_push_outline(out_commands, selection, fmaxf(1.0f, roundf(scale)),
                                           theme->on_color);
    if (result != REACH_OK)
    {
        return result;
    }
    return reach_screenshot_push_size_label(out_commands, surface, selection, theme, scale);
}
