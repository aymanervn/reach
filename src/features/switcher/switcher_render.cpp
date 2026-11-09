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

static const uint64_t REACH_SWITCHER_SELECTION_ANCHOR_ID = 2;

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
    float card_scale = layout.card_scale > 0 ? layout.card_scale : scale;
    float inset = REACH_SWITCHER_SELECTION_INSET * card_scale;
    float body_minimum = 0, body_maximum = 0;
    if (reach_motion_range(&layout.body.width, &body_minimum, &body_maximum) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    float content_inset = border + REACH_SWITCHER_PADDING * scale;
    if (layout.has_selection)
    {
        float minimum_width = 0, minimum_height = 0, width = 0, height = 0;
        if (reach_motion_range(&layout.selection.width, &minimum_width, &width) != REACH_OK ||
            reach_motion_range(&layout.selection.height, &minimum_height, &height) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        float pill_radius = fminf(theme->radius_small * card_scale,
                                  fmaxf(0, fminf(minimum_width, minimum_height) * .5f));
        reach_render_group anchor = {};
        anchor.id = REACH_SWITCHER_SELECTION_ANCHOR_ID;
        anchor.parent_id = body.id;
        anchor.bounds = body.bounds;
        anchor.x = reach_motion_affine(&layout.body.width, .5f, -layout.envelope.width * .5f);
        anchor.has_clip = 1;
        anchor.clip = anchor.bounds;
        anchor.clip_radius = fmaxf(0, theme->radius_large * scale - border);
        reach_motion_set(&anchor.clip_insets[REACH_RENDER_CLIP_LEFT], border);
        anchor.clip_insets[REACH_RENDER_CLIP_RIGHT] =
            reach_motion_affine(&layout.body.width, -1, anchor.bounds.width + border);
        reach_motion_set(&anchor.clip_insets[REACH_RENDER_CLIP_TOP], border);
        anchor.clip_insets[REACH_RENDER_CLIP_BOTTOM] =
            reach_motion_affine(&layout.body.height, -1, anchor.bounds.height + border);
        result = reach_render_command_buffer_begin_group(out_commands, &anchor);
        reach_render_command_buffer_end_group(out_commands);
        reach_render_group pill = {};
        pill.id = anchor.id + 1;
        pill.parent_id = anchor.id;
        pill.bounds = {0, 0, fmaxf(1, ceilf(width)), fmaxf(1, ceilf(height))};
        if (result == REACH_OK)
            result =
                reach_render_push_rect_motion(out_commands, &pill, &layout.selection, pill_radius,
                                              theme->switcher_selection_background);
        if (result != REACH_OK)
            return result;
    }
    for (size_t index = 0; index < layout.count; ++index)
    {
        const auto &card = layout.cards[index];
        const auto *window =
            reach_window_tracking_window_by_id(reach_switcher_windows(switcher), card.window);
        reach_render_group group = {};
        group.id = card.render_id;
        group.parent_id = body.id;
        group.bounds = {0, 0, ceilf(card.preview.width + inset * 2),
                        ceilf(card.preview.height + inset * 2)};
        group.scale = layout.fit;
        if (reach_motion_combine(&card.preview_motion.x, 1, &layout.fit, -inset, 0, &group.x) !=
                REACH_OK ||
            reach_motion_combine(&card.preview_motion.y, 1, &layout.fit, -inset, 0, &group.y) !=
                REACH_OK)
            return REACH_INVALID_ARGUMENT;
        result = reach_render_command_buffer_begin_group(out_commands, &group);
        auto push = [&](const reach_render_command &command)
        {
            if (result == REACH_OK)
                result = reach_render_command_buffer_push(out_commands, &command);
        };
        if (!card.preview_visible && window != nullptr)
        {
            const auto *runtime_path =
                reach_application_identity_primary_runtime_path(&window->identity);
            const auto *path = window->icon_ref[0] != 0 ? window->icon_ref : runtime_path;
            uint64_t icon = path != nullptr && path[0] != 0
                                ? reach_icon_service_get(reach_switcher_icons(switcher), path,
                                                         ctx->icon_size_px)
                                : 0;
            float size = fminf(REACH_SWITCHER_FALLBACK_ICON_SIZE * card_scale,
                               fminf(card.preview.width, card.preview.height));
            if (icon != 0 && size > 0)
            {
                reach_render_command image = {};
                image.type = REACH_RENDER_COMMAND_ICON;
                image.rect = {inset + (card.preview.width - size) * .5f,
                              inset + (card.preview.height - size) * .5f, size, size};
                image.icon_id = icon;
                image.color.a = 1;
                push(image);
            }
        }
        reach_render_command_buffer_end_group(out_commands);
        if (result != REACH_OK)
            return result;
        if (!card.selected)
            continue;
        uint16_t label_text[260] = {};
        reach_switcher_label_for_window(label_text, 260, window);
        float label_size = REACH_TEXT_SIZE_MEDIUM * scale;
        float text_width = reach_text_width_or_estimate(&ctx->text_measure, label_text, label_size,
                                                        REACH_TEXT_WEIGHT_DEMIBOLD,
                                                        REACH_SWITCHER_LABEL_GLYPH_ADVANCE);
        float label_width =
            fmaxf(1, fminf(ceilf(text_width) + 2, body_maximum - content_inset * 2));
        reach_render_group position = {};
        position.id = card.render_id + 8;
        position.parent_id = body.id;
        position.bounds = {0, 0, ceilf(label_width), REACH_SWITCHER_LABEL_HEIGHT * scale};
        reach_motion right =
            reach_motion_affine(&layout.body.width, 1, -content_inset - label_width);
        reach_motion left = {};
        reach_motion_set(&left, content_inset);
        if (reach_motion_combine(&card.preview_motion.x, 1, &card.preview_motion.width, .5f,
                                 -label_width * .5f, &position.x) != REACH_OK ||
            reach_motion_min(&position.x, &right, &position.x) != REACH_OK ||
            reach_motion_max(&position.x, &left, &position.x) != REACH_OK ||
            reach_motion_combine(&card.preview_motion.y, 1, &card.preview_motion.height, 1,
                                 REACH_SWITCHER_LABEL_GAP * scale, &position.y) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        result = reach_render_command_buffer_begin_group(out_commands, &position);
        reach_render_command_buffer_end_group(out_commands);
        if (result != REACH_OK)
            return result;
        reach_render_group viewport = {};
        viewport.id = card.render_id + 9;
        viewport.parent_id = position.id;
        viewport.bounds = viewport.clip = position.bounds;
        viewport.has_clip = 1;
        reach_motion_set(&viewport.clip_insets[REACH_RENDER_CLIP_RIGHT], 0);
        reach_render_command text = {};
        text.type = REACH_RENDER_COMMAND_TEXT;
        text.rect = viewport.bounds;
        text.color = theme->switcher_label_text;
        text.text_weight = REACH_TEXT_WEIGHT_DEMIBOLD;
        text.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
        text.text_ellipsis = 1;
        text.text_size = label_size;
        reach_copy_utf16(text.text, 260, label_text);
        result = reach_render_push_width_text(out_commands, &text, &viewport);
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}
