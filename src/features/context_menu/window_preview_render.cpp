#include "context_menu_common.h"

#include "reach/core/typography.h"

#include <math.h>
#include <stdio.h>

static constexpr float reach_preview_title_padding_dp = 8.0f;
static constexpr float reach_preview_title_close_gap_dp = 4.0f;

static reach_result reach_preview_push_text(reach_render_command_buffer *out, reach_rect_f32 rect,
                                            const uint16_t *text, reach_color color, float size,
                                            int32_t alignment)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.color = color;
    command.text_size = size;
    command.text_weight = REACH_TEXT_WEIGHT_NORMAL;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, REACH_MENU_TEXT_CAPACITY, text);
    return reach_render_command_buffer_push(out, &command);
}

reach_result reach_context_menu_render_previews(reach_context_menu *menu,
                                                const reach_context_menu_render_context *ctx,
                                                reach_render_command_buffer *out_commands)
{
    const reach_theme *theme = ctx->theme;
    const reach_context_menu_state *state = &menu->state;
    float scale = state->dpi_scale;
    reach_render_command_buffer_clear(out_commands);
    reach_popup_background_input popup = {};
    popup.theme = theme;
    popup.bounds = state->bounds;
    popup.notch_center_x = state->notch_anchor_x - state->bounds.x;
    popup.notch_side = reach_popup_notch_side(state->drop_direction);
    popup.dpi_scale = scale;
    reach_result result = reach_popup_push_background(&popup, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }
    for (size_t visible = 0; visible < menu->previews.count; ++visible)
    {
        const reach_context_menu_preview_card *card = &menu->previews.cards[visible];
        reach_color highlight = reach_theme_color_alpha(
            theme->context_menu_hover_background,
            state->hovered_index == card->index ? reach_context_menu_hover_opacity(menu) : 0.0f);
        reach_render_command command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = card->header;
        command.radius = card->header.height * 0.5f;
        command.color = highlight;
        result = reach_render_command_buffer_push(out_commands, &command);
        if (result != REACH_OK)
        {
            return result;
        }
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        float border = reach_theme_border_thickness(theme, scale);
        command.rect = {card->preview.x - border, card->preview.y - border,
                        card->preview.width + border * 2.0f, card->preview.height + border * 2.0f};
        reach_color outline =
            reach_theme_color_mix(theme->popup_background, highlight, highlight.a);
        result = reach_render_push_bordered_background(
            out_commands, &command, theme->popup_background, outline, border, nullptr, scale);
        if (result != REACH_OK)
        {
            return result;
        }
        reach_rect_f32 title = card->header;
        float title_padding = reach_preview_title_padding_dp * scale;
        title.x += title_padding;
        title.width = fmaxf(0.0f, title.width - card->close.width - title_padding -
                                      reach_preview_title_close_gap_dp * scale);
        const uint16_t *label = state->item_titles[card->index];
        if (label[0] == 0)
        {
            label = (const uint16_t *)L"App";
        }
        result =
            reach_preview_push_text(out_commands, title, label, theme->context_menu_text,
                                    REACH_TEXT_SIZE_MEDIUM * scale, REACH_TEXT_ALIGNMENT_LEADING);
        if (result != REACH_OK)
        {
            return result;
        }
        float hover =
            state->close_hovered_index == card->index ? reach_context_menu_close_hover(menu) : 0.0f;
        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = {card->close.x + card->close.width * 0.1f,
                        card->close.y + card->close.height * 0.1f, card->close.width * 0.8f,
                        card->close.height * 0.8f};
        command.radius = fminf(theme->radius_small * scale, command.rect.height * 0.5f);
        command.color =
            reach_theme_color_alpha(theme->context_menu_close_background, 0.14f + 0.74f * hover);
        result = reach_render_command_buffer_push(out_commands, &command);
        if (result != REACH_OK)
        {
            return result;
        }
        command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        command.icon_id = REACH_VECTOR_ICON_CLOSE;
        float inset = fminf(command.rect.width, command.rect.height) * 0.3f;
        command.rect = {command.rect.x + inset, command.rect.y + inset,
                        command.rect.width - inset * 2.0f, command.rect.height - inset * 2.0f};
        command.color = theme->context_menu_close_glyph;
        result = reach_render_command_buffer_push(out_commands, &command);
        if (result != REACH_OK)
        {
            return result;
        }
        if (!card->live)
        {
            const uint16_t *path = reach_context_menu_preview_icon_path(menu, card->window);
            if (path != nullptr && path[0] != 0)
            {
                command = {};
                command.type = REACH_RENDER_COMMAND_ICON;
                command.icon_id = reach_icon_service_get(menu->icons, path, ctx->icon_size_px);
                float size = fminf(64.0f * scale, fminf(card->preview.width, card->preview.height));
                command.rect = {card->preview.x + (card->preview.width - size) * 0.5f,
                                card->preview.y + (card->preview.height - size) * 0.5f, size, size};
                command.color.a = 1.0f;
                result = reach_render_command_buffer_push(out_commands, &command);
                if (result != REACH_OK)
                {
                    return result;
                }
            }
        }
    }
    if (menu->previews.overflow)
    {
        uint16_t label[64] = {};
        swprintf_s((wchar_t *)label, 64, L"%zu\u2013%zu of %zu", state->visible_start + 1,
                   state->visible_start + state->visible_count, state->item_count);
        reach_rect_f32 footer = menu->previews.footer;
        float inset = footer.height;
        footer.x += inset;
        footer.width = fmaxf(0.0f, footer.width - inset * 2.0f);
        result =
            reach_preview_push_text(out_commands, footer, label, theme->context_menu_text,
                                    REACH_TEXT_SIZE_SMALL * scale, REACH_TEXT_ALIGNMENT_CENTER);
        if (result != REACH_OK)
        {
            return result;
        }
        const reach_rect_f32 buttons[] = {menu->previews.previous, menu->previews.next};
        const uint16_t *arrows[] = {(const uint16_t *)L"\u2039", (const uint16_t *)L"\u203a"};
        for (size_t index = 0; index < 2; ++index)
        {
            if (buttons[index].width <= 0.0f)
            {
                continue;
            }
            result = reach_preview_push_text(
                out_commands, buttons[index], arrows[index], theme->context_menu_text,
                REACH_TEXT_SIZE_HEADING * scale, REACH_TEXT_ALIGNMENT_CENTER);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }
    return REACH_OK;
}
