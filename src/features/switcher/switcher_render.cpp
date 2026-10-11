#include "reach/features/switcher.h"

#include "switcher_common.h"

#include "reach/core/typography.h"

#include <math.h>

static void reach_switcher_label_for_window(uint16_t *out_label, size_t out_count,
                                            const reach_window_snapshot *window)
{
    if (window != nullptr && window->title[0] != 0)
    {
        (void)reach_copy_utf16(out_label, out_count, window->title);
    }
    else
    {
        reach_window_tracking_app_display_name(window, out_label, out_count);
    }
    if (out_label[0] == 0)
    {
        const uint16_t fallback[] = {'A', 'p', 'p', 0};
        (void)reach_copy_utf16(out_label, out_count, fallback);
    }
}

reach_result reach_switcher_append_render_commands(reach_switcher *switcher,
                                                   const reach_switcher_render_context *ctx,
                                                   reach_render_command_buffer *out_commands)
{
    if (switcher == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_render_command_buffer_clear(out_commands);
    const reach_switcher_layout *layout = reach_switcher_card_layout(switcher);
    const reach_switcher_state *state = reach_switcher_state_ptr(switcher);
    const reach_theme *theme = ctx->theme;
    float scale = layout->scale > 0.0f ? layout->scale : 1.0f;
    float border = reach_theme_border_thickness(theme, scale);
    float selected_inset = REACH_SWITCHER_SELECTION_INSET * scale;
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = {0.0f, 0.0f, state->bounds.width, state->bounds.height};
    shape.radius = theme->radius_large * scale;
    reach_result result = reach_render_push_bordered_background(
        out_commands, &shape, theme->switcher_background, theme->bar_border, border,
        &theme->popup_shadow, scale);
    if (result != REACH_OK)
    {
        return result;
    }

    for (size_t index = 0; index < layout->count; ++index)
    {
        const reach_switcher_card *card = &layout->cards[index];
        const reach_window_snapshot *window =
            reach_window_tracking_window_by_id(reach_switcher_windows(switcher), card->window);
        reach_render_command command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = card->preview;
        command.color = theme->switcher_selection_background;
        if (card->selected)
        {
            command.rect.x -= selected_inset;
            command.rect.y -= selected_inset;
            command.rect.width += selected_inset * 2.0f;
            command.rect.height += selected_inset * 2.0f;
            command.radius = theme->radius_small * scale;
            result = reach_render_command_buffer_push(out_commands, &command);
            if (result != REACH_OK)
            {
                return result;
            }
        }

        command = {};
        command.type = REACH_RENDER_COMMAND_RECT;
        command.rect = card->preview;
        command.color = theme->switcher_background;
        result = reach_render_command_buffer_push(out_commands, &command);
        if (result != REACH_OK)
        {
            return result;
        }

        if (!card->preview_visible && window != nullptr)
        {
            const uint16_t *runtime_path =
                reach_application_identity_primary_runtime_path(&window->identity);
            const uint16_t *icon_path = window->icon_ref[0] != 0 ? window->icon_ref : runtime_path;
            uint64_t icon_id = icon_path != nullptr && icon_path[0] != 0
                                   ? reach_icon_service_get(reach_switcher_icons(switcher),
                                                            icon_path, ctx->icon_size_px)
                                   : 0;
            if (icon_id != 0)
            {
                float size = fminf(REACH_SWITCHER_FALLBACK_ICON_SIZE * scale,
                                   fminf(card->preview.width, card->preview.height));
                command = {};
                command.type = REACH_RENDER_COMMAND_ICON;
                command.rect = {card->preview.x + (card->preview.width - size) * 0.5f,
                                card->preview.y + (card->preview.height - size) * 0.5f, size, size};
                command.icon_id = icon_id;
                command.color.a = 1.0f;
                result = reach_render_command_buffer_push(out_commands, &command);
                if (result != REACH_OK)
                {
                    return result;
                }
            }
        }

        if (card->selected)
        {
            command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect = {card->preview.x,
                            card->preview.y + card->preview.height +
                                REACH_SWITCHER_LABEL_GAP * scale,
                            card->preview.width, REACH_SWITCHER_LABEL_HEIGHT * scale};
            command.color = theme->switcher_label_text;
            command.text_weight = REACH_TEXT_WEIGHT_DEMIBOLD;
            command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
            command.text_size = REACH_TEXT_SIZE_MEDIUM * scale;
            reach_switcher_label_for_window(command.text, 260, window);
            command.text_ellipsis =
                reach_text_width_or_estimate(&ctx->text_measure, command.text, command.text_size,
                                             command.text_weight, 0.62f) > command.rect.width;
            result = reach_render_command_buffer_push(out_commands, &command);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }
    return REACH_OK;
}
