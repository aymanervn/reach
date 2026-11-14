#include "context_menu_common.h"

#include "reach/features/common/ui_controls.h"

static float reach_context_menu_scale(const reach_context_menu_render_input *input, float value)
{
    float scale = input != nullptr && input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f;
    return value * scale;
}

reach_result reach_context_menu_build_render_commands(const reach_context_menu_render_input *input,
                                                      reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->item_slots == nullptr ||
        input->item_commands == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_context_menu_metrics *metrics = &reach_context_menu_popup_metrics;

    reach_render_command_buffer_clear(out_commands);
    reach_render_command command = {};

    reach_popup_background_input popup = {};
    popup.theme = input->theme;
    popup.bounds = input->bounds;
    popup.notch_center_x = input->notch_center_x;
    popup.notch_side = input->notch_side;
    popup.dpi_scale = input->dpi_scale;
    if (input->top_bar_dropdown)
    {
        popup.background_color_override = input->theme->top_bar_dropdown_background;
        popup.has_background_color_override = 1;
        popup.hide_notch = 1;
    }
    reach_result popup_result = reach_popup_push_background(&popup, out_commands);
    if (popup_result != REACH_OK)
    {
        return popup_result;
    }

    reach_render_command_buffer_set_scissor(out_commands, input->item_viewport);

    for (size_t index = 0; index < input->item_count; ++index)
    {
        reach_rect_f32 item = input->item_slots[index];
        if (reach_context_menu_separator_before(input->item_commands, index))
        {
            float gap = reach_context_menu_scale(input, reach_context_menu_separator_gap);
            reach_rect_f32 line =
                reach_ui_horizontal_separator_bounds(item.x, item.y, item.width, input->dpi_scale);
            line.y = item.y - (gap + line.height) * 0.5f;
            (void)reach_ui_horizontal_separator_render(out_commands, line,
                                                       input->theme->popup_separator);
        }
        reach_color foreground = input->theme->context_menu_text;

        if (input->hovered_index == index)
        {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect = item;
            command.color = input->theme->context_menu_hover_background;
            command.radius = reach_context_menu_scale(input, input->theme->radius_small);
            reach_render_command_buffer_push(out_commands, &command);
        }

        int32_t has_icon = input->item_icon_ids != nullptr && input->item_icon_ids[index] != 0;
        if (has_icon)
        {
            float icon_size = reach_context_menu_scale(input, metrics->icon_size);
            reach_render_command icon_command = {};
            icon_command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
            icon_command.rect.x = item.x + reach_context_menu_scale(input, metrics->icon_inset);
            icon_command.rect.y = item.y + (item.height - icon_size) * 0.5f;
            icon_command.rect.width = icon_size;
            icon_command.rect.height = icon_size;
            icon_command.color = foreground;
            icon_command.icon_id = input->item_icon_ids[index];
            reach_render_command_buffer_push(out_commands, &icon_command);
        }

        float text_left = reach_context_menu_scale(input, has_icon ? metrics->icon_text_inset
                                                                   : metrics->text_leading_inset);
        float text_right = reach_context_menu_scale(input, metrics->text_trailing_inset);

        command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect.x = item.x + text_left;
        command.rect.y = item.y;
        command.rect.width = item.width - text_left - text_right;
        command.rect.height = item.height;
        command.color = foreground;
        command.text_size = reach_context_menu_scale(input, metrics->text_size);
        command.text_alignment = input->text_alignment_leading;
        command.text_ellipsis = 1;
        reach_copy_utf16(command.text, 260,
                         reach_context_menu_command_text(input->item_commands[index]));
        reach_render_command_buffer_push(out_commands, &command);
    }

    reach_render_command_buffer_clear_scissor(out_commands);

    return REACH_OK;
}

reach_result reach_context_menu_append_render_commands(reach_context_menu *menu,
                                                       const reach_context_menu_render_context *ctx,
                                                       reach_render_command_buffer *out_commands)
{
    if (menu == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (menu->state.window_list_open)
    {
        return reach_context_menu_render_previews(menu, ctx, out_commands);
    }
    const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);

    reach_context_menu_render_input input = {};
    input.theme = ctx->theme;
    input.bounds = state->bounds;
    input.item_viewport = state->item_viewport;
    input.item_slots = state->item_slots;
    input.item_commands = state->item_commands;
    input.item_icon_ids = state->item_icon_ids;
    input.item_count = state->item_count;
    input.hovered_index = state->hovered_index;
    input.notch_center_x = state->notch_anchor_x - state->bounds.x;
    input.notch_side = reach_popup_notch_side(state->drop_direction);
    input.top_bar_dropdown = state->power_open;
    input.dpi_scale = ctx->dpi_scale;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    return reach_context_menu_build_render_commands(&input, out_commands);
}
