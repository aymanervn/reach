#include "reach/features/context_menu.h"

static reach_color reach_context_menu_rgb(uint8_t r, uint8_t g, uint8_t b, float a)
{
    reach_color color = {};
    color.r = (float)r / 255.0f;
    color.g = (float)g / 255.0f;
    color.b = (float)b / 255.0f;
    color.a = a;
    return color;
}

static float reach_context_menu_scale(const reach_context_menu_render_input *input, float value)
{
    float scale = input != nullptr && input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    return value * scale;
}

typedef struct reach_context_menu_item_style
{
    reach_color foreground;
    reach_color hover_background;
    reach_color hover_foreground;
    int32_t use_hover_foreground;
} reach_context_menu_item_style;

static reach_context_menu_item_style reach_context_menu_style_for_command(uint32_t command)
{
    reach_context_menu_item_style style = {};
    style.foreground = reach_context_menu_rgb(232, 229, 224, 0.96f);
    style.hover_background = reach_context_menu_rgb(255, 255, 255, 0.12f);
    style.hover_foreground = style.foreground;
    style.use_hover_foreground = 0;

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    int32_t power_color = 1;
    switch (command)
    {
    case REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN:
        r = 236;
        g = 92;
        b = 92;
        break;
    case REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP:
        r = 176;
        g = 132;
        b = 232;
        break;
    case REACH_CONTEXT_MENU_COMMAND_POWER_RESTART:
        r = 98;
        g = 210;
        b = 132;
        break;
    case REACH_CONTEXT_MENU_COMMAND_POWER_LOCK:
    case REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT:
        r = 236;
        g = 202;
        b = 92;
        break;
    default:
        power_color = 0;
        break;
    }

    if (power_color)
    {
        style.hover_background = reach_context_menu_rgb(r, g, b, 0.12f);
        style.hover_foreground = reach_context_menu_rgb(r, g, b, 1.0f);
        style.use_hover_foreground = 1;
    }

    return style;
}

reach_result reach_context_menu_build_render_commands(const reach_context_menu_render_input *input,
                                                      reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->item_slots == nullptr ||
        input->item_commands == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);
    reach_render_command command = {};
    float width = input->bounds.width;
    float notch_center = width * 0.30f;
    if (input->use_anchor_x)
    {
        notch_center = input->anchor_x - input->bounds.x;
    }
    else if (input->has_layout && input->dock_layout != nullptr &&
             input->target_index < input->dock_layout->app_slot_count)
    {
        reach_rect_f32 slot = input->dock_layout->app_slots[input->target_index];
        notch_center = slot.x + slot.width * 0.5f - input->bounds.x;
    }

    reach_popup_background_input popup = {};
    popup.theme = input->theme;
    popup.bounds = input->bounds;
    popup.notch_center_x = notch_center;
    popup.dpi_scale = input->dpi_scale;
    reach_result popup_result = reach_popup_push_background(&popup, out_commands);
    if (popup_result != REACH_OK)
    {
        return popup_result;
    }

    for (size_t index = 0; index < input->item_count; ++index)
    {
        reach_rect_f32 item = input->item_slots[index];
        item.x -= input->bounds.x;
        item.y -= input->bounds.y;
        reach_context_menu_item_style style =
            reach_context_menu_style_for_command(input->item_commands[index]);
        reach_color foreground = style.foreground;

        if (input->hovered_index == index)
        {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect = item;
            command.color = style.hover_background;
            command.radius = reach_context_menu_scale(input, 8.0f);
            reach_render_command_buffer_push(out_commands, &command);
            if (style.use_hover_foreground)
            {
                foreground = style.hover_foreground;
            }
        }

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        float text_left =
            input->item_icon_ids != nullptr && input->item_icon_ids[index] != 0
                ? reach_context_menu_scale(input, 40.0f)
                : reach_context_menu_scale(input, 14.0f);
        if (input->item_icon_ids != nullptr && input->item_icon_ids[index] != 0)
        {
            float icon_size = reach_context_menu_scale(input, 16.0f);
            reach_render_command icon_command = {};
            icon_command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
            icon_command.rect.x = item.x + reach_context_menu_scale(input, 13.0f);
            icon_command.rect.y = item.y + (item.height - icon_size) * 0.5f;
            icon_command.rect.width = icon_size;
            icon_command.rect.height = icon_size;
            icon_command.color = foreground;
            icon_command.icon_id = input->item_icon_ids[index];
            reach_render_command_buffer_push(out_commands, &icon_command);
        }
        command.rect.x = item.x + text_left;
        command.rect.y = item.y;
        command.rect.width = item.width - text_left - reach_context_menu_scale(input, 14.0f);
        command.rect.height = item.height;
        command.color = foreground;
        command.text_size = reach_context_menu_scale(input, 14.0f);
        command.text_alignment = input->text_alignment_leading;
        command.text_ellipsis = 1;
        reach_copy_utf16(command.text, 260,
                         reach_context_menu_command_text(input->item_commands[index]));
        reach_render_command_buffer_push(out_commands, &command);
    }

    return REACH_OK;
}
