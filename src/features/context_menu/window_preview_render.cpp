#include "context_menu_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/ui_controls.h"

#include <math.h>

static reach_result reach_preview_fill(reach_render_command_buffer *out, uint64_t id,
                                       uint64_t parent, const reach_rect_motion &rect, float width,
                                       float height, float radius, reach_color color,
                                       const reach_motion *opacity = nullptr,
                                       const reach_motion *tint = nullptr, reach_color from = {},
                                       reach_color to = {})
{
    reach_render_group source = {};
    source.id = id;
    source.parent_id = parent;
    source.bounds = {0, 0, fmaxf(1, ceilf(width)), fmaxf(1, ceilf(height))};
    if (opacity)
        source.opacity = *opacity;
    if (tint)
        reach_ui_color_feedback_apply(&source, from, to, tint);
    return reach_render_push_rect_motion(out, &source, &rect, radius, color);
}

static reach_result reach_preview_icon(reach_render_command_buffer *out, uint64_t id,
                                       uint64_t parent, const reach_motion &x,
                                       const reach_motion &y, const reach_motion &size,
                                       float maximum, reach_render_command_type type, uint64_t icon,
                                       reach_color color)
{
    if (maximum <= 0 || icon == 0)
        return REACH_OK;
    reach_render_group source = {};
    source.id = id;
    source.parent_id = parent;
    source.bounds = {0, 0, maximum, maximum};
    source.x = x;
    source.y = y;
    source.scale = reach_motion_affine(&size, 1 / maximum, 0);
    float minimum = 0, maximum_size = 0;
    if (reach_motion_range(&size, &minimum, &maximum_size) == REACH_OK && minimum == maximum &&
        maximum_size == maximum)
        reach_motion_set(&source.scale, 1);
    reach_result result = reach_render_command_buffer_begin_group(out, &source);
    reach_render_command command = {};
    command.type = type;
    command.rect = source.bounds;
    command.icon_id = icon;
    command.color = color;
    if (result == REACH_OK)
        result = reach_render_command_buffer_push(out, &command);
    reach_render_command_buffer_end_group(out);
    return result;
}

reach_result reach_context_menu_render_previews(reach_context_menu *menu,
                                                const reach_context_menu_render_context *ctx,
                                                reach_render_command_buffer *out)
{
    const auto *theme = ctx->theme;
    const auto &state = menu->state;
    const auto &layout = menu->previews;
    float scale = state.dpi_scale;
    float card_scale = layout.card_scale;
    reach_render_command_buffer_clear(out);
    reach_render_group body = {};
    body.id = 1;
    body.bounds = {0, 0, fmaxf(1, ceilf(layout.envelope.width)),
                   fmaxf(1, ceilf(layout.envelope.height))};
    body.x = layout.body.x;
    body.y = layout.body.y;
    reach_result result = reach_render_command_buffer_begin_group(out, &body);
    reach_render_command_buffer_end_group(out);
    float minimum = 0, maximum_width = 0;
    if (reach_motion_range(&layout.body.width, &minimum, &maximum_width) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    float minimum_height = 0, maximum_height = 0;
    if (reach_motion_range(&layout.body.height, &minimum_height, &maximum_height) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    reach_popup_background_input popup = {};
    popup.theme = theme;
    popup.bounds = {0, 0,
                    menu->layout_cleanup_pending ? fmaxf(maximum_width, layout.envelope.width)
                                                 : maximum_width,
                    maximum_height};
    popup.notch_side = reach_popup_notch_side(state.drop_direction);
    popup.dpi_scale = scale;
    popup.hide_notch = 1;
    popup.background_color_override = theme->surface.base;
    popup.has_background_color_override = 1;
    popup.glass = theme->glass;
    if (result == REACH_OK)
        result = reach_popup_push_width_background(&popup, &layout.body.width, &layout.body.height,
                                                   &layout.notch, body.id, 8, out);
    if (result != REACH_OK)
        return result;
    if (layout.label_mode && menu->request.label[0] != 0)
    {
        reach_render_group label = {};
        label.id = 4;
        label.parent_id = body.id;
        label.bounds = {0, 0, ceilf(layout.label_width) + 1, ceilf(layout.label_height)};
        label.x = reach_motion_affine(&menu->width, .5f, -layout.label_width * .5f);
        reach_motion_set(&label.y, layout.header_y);
        label.opacity = menu->reflow;
        result = reach_render_command_buffer_begin_group(out, &label);
        reach_render_command text = {};
        text.type = REACH_RENDER_COMMAND_TEXT;
        text.rect = {0, 0, ceilf(layout.label_width) + 1, layout.label_height};
        text.color = theme->text.primary;
        text.text_size = REACH_TEXT_SIZE_XSMALL * scale;
        text.text_weight = REACH_TEXT_WEIGHT_NORMAL;
        text.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
        text.text_ellipsis = 1;
        reach_copy_utf16(text.text, REACH_MENU_TEXT_CAPACITY, menu->request.label);
        if (result == REACH_OK)
            result = reach_render_command_buffer_push(out, &text);
        reach_render_command_buffer_end_group(out);
        if (result != REACH_OK)
            return result;
    }
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        const auto &card = layout.cards[visible];
        float header = card.header_height, border = layout.border;
        float thumbnail_y = header + card.header_gap;
        float close_width = fminf(header, card.source_width);
        float close_radius = fminf(theme->radius_small * card_scale, header * .4f);
        float glyph = close_width * .32f;
        int32_t interactive =
            state.hovered_index == card.index || state.close_hovered_index == card.index;
        float title_maximum = fmaxf(
            0, card.source_width - (interactive ? close_width + 12 * card_scale : 16 * card_scale));
        const auto *label = state.item_titles[card.index][0] ? state.item_titles[card.index]
                                                             : (const uint16_t *)L"App";
        uint64_t icon = 0;
        float icon_size = fminf(64 * card_scale, fminf(card.source_width, card.source_height));
        if (!card.live)
        {
            const auto *path = reach_context_menu_preview_icon_path(menu, card.window);
            icon =
                path && path[0] ? reach_icon_service_get(menu->icons, path, ctx->icon_size_px) : 0;
        }
        reach_rect_f32 close_rect = {card.source_width - close_width * .9f, header * .1f,
                                     close_width * .8f, header * .8f};
        reach_rect_f32 glyph_rect = {card.source_width - close_width * .66f,
                                     header * .5f - close_width * .16f, glyph, glyph};

        reach_render_group frame = {};
        frame.parent_id = body.id;
        float pad = ceilf(border) + 1;
        frame.bounds = {-pad, 0, ceilf(card.source_width + pad * 2),
                        ceilf(thumbnail_y + card.source_height + pad)};
        frame.x = card.motion.x;
        frame.y = card.header_top;
        frame.scale = card.size;

        reach_motion hover = {};
        reach_motion_set(&hover, 0);
        if (state.hovered_index == card.index)
            hover = menu->hover;
        frame.id = card.render_id;
        result = reach_render_command_buffer_begin_group(out, &frame);
        if (result != REACH_OK)
            return result;
        auto push = [&](reach_render_command command)
        {
            if (result == REACH_OK)
                result = reach_render_command_buffer_push(out, &command);
        };
        if (icon != 0)
        {
            reach_render_command image = {};
            image.type = REACH_RENDER_COMMAND_ICON;
            image.rect = {(card.source_width - icon_size) * .5f,
                          thumbnail_y + (card.source_height - icon_size) * .5f, icon_size,
                          icon_size};
            image.icon_id = icon;
            image.color = {1, 1, 1, 1};
            push(image);
        }
        if (title_maximum > 0)
        {
            reach_render_command text = {};
            text.type = REACH_RENDER_COMMAND_TEXT;
            text.rect = {8 * card_scale, 0, title_maximum, header};
            text.color = theme->text.primary;
            text.text_size = REACH_TEXT_SIZE_XSMALL * card_scale;
            text.text_weight = REACH_TEXT_WEIGHT_NORMAL;
            text.text_alignment = REACH_TEXT_ALIGNMENT_LEADING;
            text.text_ellipsis = 1;
            reach_copy_utf16(text.text, REACH_MENU_TEXT_CAPACITY, label);
            push(text);
        }
        reach_render_command_buffer_end_group(out);
        if (result != REACH_OK)
            return result;
        if (!interactive)
            continue;

        reach_render_group reveal = {};
        reveal.id = card.render_id + 12;
        reveal.parent_id = frame.id;
        reveal.bounds = {0, 0, ceilf(card.source_width), ceilf(header)};
        reveal.opacity = hover;
        result = reach_render_command_buffer_begin_group(out, &reveal);
        reach_render_command_buffer_end_group(out);
        if (result != REACH_OK)
            return result;
        reach_motion close_hover = {};
        reach_motion_set(&close_hover, 0);
        if (state.close_hovered_index == card.index)
            close_hover = menu->close_hover;
        auto opacity = reach_motion_affine(&close_hover, .74f, .14f);
        reach_rect_motion close = {};
        reach_motion_set(&close.x, close_rect.x);
        reach_motion_set(&close.y, close_rect.y);
        reach_motion_set(&close.width, close_rect.width);
        reach_motion_set(&close.height, close_rect.height);
        result = reach_preview_fill(out, card.render_id + 8, reveal.id, close, close_rect.width,
                                    close_rect.height, close_radius,
                                    reach_theme_color_alpha(theme->danger, 1), &opacity);
        reach_motion glyph_x = {}, glyph_y = {}, glyph_size = {};
        reach_motion_set(&glyph_x, glyph_rect.x);
        reach_motion_set(&glyph_y, glyph_rect.y);
        reach_motion_set(&glyph_size, glyph);
        if (result == REACH_OK)
            result = reach_preview_icon(out, card.render_id + 10, reveal.id, glyph_x, glyph_y,
                                        glyph_size, glyph, REACH_RENDER_COMMAND_VECTOR_ICON,
                                        REACH_VECTOR_ICON_CLOSE, theme->on_color);
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}
