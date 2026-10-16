#include "reach/features/switcher.h"

#include "switcher_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/ui_controls.h"

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

static reach_result reach_switcher_fill(reach_render_command_buffer *commands, uint64_t id,
                                        const reach_rect_motion &bounds, float width, float height,
                                        float radius, reach_color color)
{
    reach_render_group source = {};
    source.id = id;
    source.parent_id = 1;
    source.bounds = {0, 0, fmaxf(1, ceilf(width)), fmaxf(1, ceilf(height))};
    return reach_render_push_rect_motion(commands, &source, &bounds, radius, color);
}

reach_result reach_switcher_append_render_commands(reach_switcher *switcher,
                                                   const reach_switcher_render_context *ctx,
                                                   reach_render_command_buffer *out_commands)
{
    if (switcher == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_render_command_buffer_clear(out_commands);
    const auto &layout = *reach_switcher_card_layout(switcher);
    const auto *theme = ctx->theme;
    float scale = layout.scale > 0 ? layout.scale : 1;
    float border = reach_theme_border_thickness(theme, scale);
    reach_render_group body = {};
    body.id = 1;
    body.bounds = {0, 0, fmaxf(1, ceilf(layout.envelope.width)),
                   fmaxf(1, ceilf(layout.envelope.height))};
    body.x = layout.body.x;
    body.y = layout.body.y;
    reach_result result = reach_render_command_buffer_begin_group(out_commands, &body);
    reach_render_command_buffer_end_group(out_commands);
    reach_rect_motion chrome = layout.body;
    reach_motion_set(&chrome.x, 0);
    reach_motion_set(&chrome.y, 0);
    if (result == REACH_OK)
        result = reach_ui_motion_background_render(
            out_commands, &chrome, theme->radius_large * scale, border, theme->switcher_background,
            theme->bar_border, &theme->popup_shadow, scale, 8, body.id);
    if (result != REACH_OK)
        return result;
    float selected_inset = REACH_SWITCHER_SELECTION_INSET * scale;
    for (size_t index = 0; index < layout.count; ++index)
    {
        const auto &card = layout.cards[index];
        const auto *window =
            reach_window_tracking_window_by_id(reach_switcher_windows(switcher), card.window);
        if (card.selected)
        {
            reach_rect_motion selected = card.preview_motion;
            selected.x = reach_motion_affine(&selected.x, 1, -selected_inset);
            selected.y = reach_motion_affine(&selected.y, 1, -selected_inset);
            selected.width = reach_motion_affine(&selected.width, 1, selected_inset * 2);
            selected.height = reach_motion_affine(&selected.height, 1, selected_inset * 2);
            result = reach_switcher_fill(
                out_commands, card.render_id, selected, card.preview.width + selected_inset * 2,
                card.preview.height + selected_inset * 2, theme->radius_small * scale,
                theme->switcher_selection_background);
            if (result != REACH_OK)
                return result;
        }
        result = reach_switcher_fill(out_commands, card.render_id + 2, card.preview_motion,
                                     card.preview.width, card.preview.height, 0,
                                     theme->switcher_background);
        if (result != REACH_OK)
            return result;
        if (!card.preview_visible && window != nullptr)
        {
            const auto *runtime_path =
                reach_application_identity_primary_runtime_path(&window->identity);
            const auto *path = window->icon_ref[0] != 0 ? window->icon_ref : runtime_path;
            uint64_t icon = path != nullptr && path[0] != 0
                                ? reach_icon_service_get(reach_switcher_icons(switcher), path,
                                                         ctx->icon_size_px)
                                : 0;
            if (icon != 0)
            {
                float size = fminf(REACH_SWITCHER_FALLBACK_ICON_SIZE * scale,
                                   fminf(card.preview.width, card.preview.height));
                if (size > 0)
                {
                    reach_motion visible_size =
                        reach_motion_clamp(&card.preview_motion.width, 0, size);
                    if (reach_motion_min(&visible_size, &card.preview_motion.height,
                                         &visible_size) != REACH_OK)
                        return REACH_INVALID_ARGUMENT;
                    reach_render_group source = {};
                    source.id = card.render_id + 4;
                    source.parent_id = body.id;
                    source.bounds = {0, 0, size, size};
                    source.scale = reach_motion_affine(&visible_size, 1 / size, 0);
                    if (reach_motion_combine(&card.preview_motion.x, 1, &card.preview_motion.width,
                                             .5f, 0, &source.x) != REACH_OK ||
                        reach_motion_combine(&source.x, 1, &visible_size, -.5f, 0, &source.x) !=
                            REACH_OK ||
                        reach_motion_combine(&card.preview_motion.y, 1, &card.preview_motion.height,
                                             .5f, 0, &source.y) != REACH_OK ||
                        reach_motion_combine(&source.y, 1, &visible_size, -.5f, 0, &source.y) !=
                            REACH_OK)
                        return REACH_INVALID_ARGUMENT;
                    result = reach_render_command_buffer_begin_group(out_commands, &source);
                    reach_render_command command = {};
                    command.type = REACH_RENDER_COMMAND_ICON;
                    command.rect = source.bounds;
                    command.icon_id = icon;
                    command.color.a = 1;
                    if (result == REACH_OK)
                        result = reach_render_command_buffer_push(out_commands, &command);
                    reach_render_command_buffer_end_group(out_commands);
                    if (result != REACH_OK)
                        return result;
                }
            }
        }
        if (card.selected)
        {
            reach_render_group position = {};
            position.id = card.render_id + 5;
            position.parent_id = body.id;
            position.bounds = {0, 0, fmaxf(1, ceilf(card.preview.width)),
                               REACH_SWITCHER_LABEL_HEIGHT * scale};
            position.x = card.preview_motion.x;
            if (reach_motion_combine(&card.preview_motion.y, 1, &card.preview_motion.height, 1,
                                     REACH_SWITCHER_LABEL_GAP * scale, &position.y) != REACH_OK)
                return REACH_INVALID_ARGUMENT;
            result = reach_render_command_buffer_begin_group(out_commands, &position);
            reach_render_command_buffer_end_group(out_commands);
            if (result != REACH_OK)
                return result;
            reach_render_group viewport = {};
            viewport.id = card.render_id + 6;
            viewport.parent_id = position.id;
            viewport.bounds = viewport.clip = position.bounds;
            viewport.has_clip = 1;
            viewport.clip_insets[REACH_RENDER_CLIP_RIGHT] =
                reach_motion_affine(&card.preview_motion.width, -1, viewport.bounds.width);
            reach_render_command command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect = viewport.bounds;
            command.color = theme->switcher_label_text;
            command.text_weight = REACH_TEXT_WEIGHT_DEMIBOLD;
            command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
            command.text_ellipsis = 1;
            command.text_size = REACH_TEXT_SIZE_MEDIUM * scale;
            reach_switcher_label_for_window(command.text, 260, window);
            result = reach_render_push_width_text(out_commands, &command, &viewport);
            if (result != REACH_OK)
                return result;
        }
    }
    return REACH_OK;
}
