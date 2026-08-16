#include "context_menu_common.h"

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

static reach_context_menu_item_style reach_context_menu_style_for_command(const reach_theme *theme,
                                                                          uint32_t command)
{
    reach_context_menu_item_style style = {};
    style.foreground = theme->context_menu_text;
    style.hover_background = theme->context_menu_hover_background;
    style.hover_foreground = style.foreground;
    style.use_hover_foreground = 0;

    reach_color accent = {};
    int32_t power_color = 1;
    switch (command)
    {
    case REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN:
        accent = theme->menu_accent_shutdown;
        break;
    case REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP:
        accent = theme->menu_accent_sleep;
        break;
    case REACH_CONTEXT_MENU_COMMAND_POWER_RESTART:
        accent = theme->menu_accent_restart;
        break;
    case REACH_CONTEXT_MENU_COMMAND_POWER_LOCK:
    case REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT:
        accent = theme->menu_accent_lock;
        break;
    case REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS:
        accent = theme->menu_accent_settings;
        break;
    default:
        power_color = 0;
        break;
    }

    if (power_color)
    {
        style.hover_background = reach_theme_color_alpha(accent, 0.12f);
        style.hover_foreground = accent;
        style.use_hover_foreground = 1;
    }

    return style;
}

static void reach_context_menu_push_close_button(const reach_context_menu_render_input *input,
                                                 const reach_context_menu_metrics *metrics,
                                                 reach_rect_f32 item, size_t index, float alpha,
                                                 reach_render_command_buffer *out_commands)
{
    reach_rect_f32 button = reach_context_menu_close_button_rect(metrics, item, input->dpi_scale);
    float hover = input->close_hovered_index == index ? input->close_hover : 0.0f;

    reach_render_command backing = {};
    backing.type = REACH_RENDER_COMMAND_RECT;
    backing.rect = button;
    backing.radius = button.height * 0.5f;
    backing.color = reach_theme_color_alpha(input->theme->context_menu_close_background,
                                            (0.14f + 0.74f * hover) * alpha);
    reach_render_command_buffer_push(out_commands, &backing);

    float inset = button.width * metrics->close_glyph_inset_ratio;
    reach_render_command glyph = {};
    glyph.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    glyph.icon_id = REACH_VECTOR_ICON_CLOSE;
    glyph.rect.x = button.x + inset;
    glyph.rect.y = button.y + inset;
    glyph.rect.width = button.width - inset * 2.0f;
    glyph.rect.height = button.height - inset * 2.0f;
    glyph.color = reach_theme_color_alpha(input->theme->context_menu_close_glyph,
                                          (0.72f + 0.28f * hover) * alpha);
    reach_render_command_buffer_push(out_commands, &glyph);
}

reach_result reach_context_menu_build_render_commands(const reach_context_menu_render_input *input,
                                                      reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->item_slots == nullptr ||
        input->item_commands == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_context_menu_metrics *metrics = reach_context_menu_metrics_for(input->window_list);

    reach_render_command_buffer_clear(out_commands);
    reach_render_command command = {};
    float width = input->bounds.width;

    reach_popup_background_input popup = {};
    popup.theme = input->theme;
    popup.bounds = input->bounds;
    popup.notch_center_x = input->notch_center_x;
    popup.notch_side = input->notch_side;
    popup.dpi_scale = input->dpi_scale;
    reach_result popup_result = reach_popup_push_background(&popup, out_commands);
    if (popup_result != REACH_OK)
    {
        return popup_result;
    }

    float hover_opacity = input->window_list ? input->hover_opacity : 1.0f;
    if (hover_opacity < 0.0f)
    {
        hover_opacity = 0.0f;
    }
    if (hover_opacity > 1.0f)
    {
        hover_opacity = 1.0f;
    }

    for (size_t index = 0; index < input->item_count; ++index)
    {
        reach_rect_f32 item = input->item_slots[index];
        item.x -= input->bounds.x;
        item.y -= input->bounds.y;
        reach_context_menu_item_style style =
            reach_context_menu_style_for_command(input->theme, input->item_commands[index]);
        reach_color foreground = style.foreground;

        if (!input->window_list && input->hovered_index == index && hover_opacity > 0.01f)
        {
            command = {};
            command.type = REACH_RENDER_COMMAND_RECT;
            command.rect = item;
            command.color = style.hover_background;
            command.color.a *= hover_opacity;
            command.radius = reach_context_menu_scale(input, input->theme->radius_small);
            reach_render_command_buffer_push(out_commands, &command);
            if (style.use_hover_foreground)
            {
                foreground = style.hover_foreground;
            }
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

        float text_left = reach_context_menu_scale(
            input, has_icon ? metrics->icon_text_inset : metrics->text_leading_inset);
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
                         input->window_list && input->item_titles != nullptr
                             ? input->item_titles[index]
                             : reach_context_menu_command_text(input->item_commands[index]));
        reach_render_command_buffer_push(out_commands, &command);

        if (input->window_list && input->hovered_index == index && hover_opacity > 0.01f)
        {
            reach_context_menu_push_close_button(input, metrics, item, index, hover_opacity,
                                                 out_commands);
        }
    }

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

    const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);

    reach_context_menu_render_input input = {};
    input.theme = ctx->theme;
    input.bounds = state->bounds;
    input.item_slots = state->item_slots;
    input.item_commands = state->item_commands;
    input.item_icon_ids = state->item_icon_ids;
    input.item_titles = state->item_titles;
    input.window_list = state->window_list_open;
    input.hover_opacity = reach_context_menu_hover_opacity(menu);
    input.close_hovered_index = state->close_hovered_index;
    input.close_hover = reach_context_menu_close_hover(menu);
    input.item_count = state->item_count;
    input.hovered_index = state->hovered_index;
    input.notch_center_x = state->notch_anchor_x - state->bounds.x;
    input.notch_side = reach_popup_notch_side(state->drop_direction);
    input.dpi_scale = ctx->dpi_scale;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    return reach_context_menu_build_render_commands(&input, out_commands);
}
