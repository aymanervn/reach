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

static reach_result reach_preview_text(reach_render_command_buffer *out, uint64_t id,
                                       uint64_t parent, reach_rect_f32 source,
                                       const reach_motion &width, const uint16_t *text, float size,
                                       int32_t alignment, reach_color color)
{
    if (source.width <= 0 || source.height <= 0)
        return REACH_OK;
    reach_render_group viewport = {};
    viewport.id = id;
    viewport.parent_id = parent;
    viewport.bounds = viewport.clip = source;
    viewport.has_clip = 1;
    viewport.clip_insets[REACH_RENDER_CLIP_RIGHT] = reach_motion_affine(&width, -1, source.width);
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = source;
    command.color = color;
    command.text_size = size;
    command.text_weight = REACH_TEXT_WEIGHT_NORMAL;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    reach_copy_utf16(command.text, REACH_MENU_TEXT_CAPACITY, text);
    return reach_render_push_width_text(out, &command, &viewport);
}

static void reach_preview_border_geometry(const reach_render_command_buffer *commands,
                                          uint64_t parent, float border, reach_rect_motion *outer,
                                          reach_rect_motion *inner)
{
    auto fixed = [](const reach_motion &motion)
    {
        float minimum = 0, maximum = 0;
        return reach_motion_range(&motion, &minimum, &maximum) == REACH_OK && minimum == maximum;
    };
    if (!fixed(outer->x) || !fixed(outer->y) || !fixed(outer->width) || !fixed(outer->height))
        return;
    const auto *group = reach_render_command_buffer_group(commands, parent);
    while (group != nullptr)
    {
        if (!fixed(group->x) || !fixed(group->y))
            return;
        group = reach_render_command_buffer_group(commands, group->parent_id);
    }
    auto transform = reach_render_command_buffer_group_transform_at(commands, parent, 0);
    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = reach_rect_motion_value_at(outer, 0);
    shape.rect.x += transform.offset_x;
    shape.rect.y += transform.offset_y;
    reach_render_command shapes[2] = {};
    size_t count =
        reach_render_bordered_background_shapes(&shape, {1, 1, 1, 1}, {1, 1, 1, 1}, border, shapes);
    auto assign = [&](reach_rect_motion *motion, reach_rect_f32 rect)
    {
        reach_motion_set(&motion->x, rect.x - transform.offset_x);
        reach_motion_set(&motion->y, rect.y - transform.offset_y);
        reach_motion_set(&motion->width, rect.width);
        reach_motion_set(&motion->height, rect.height);
    };
    if (count > 0)
        assign(outer, shapes[0].rect);
    if (count > 1)
        assign(inner, shapes[1].rect);
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
    if (result == REACH_OK)
        result = reach_popup_push_width_background(&popup, &layout.body.width, &layout.body.height,
                                                   &layout.notch, body.id, 8, out);
    if (result != REACH_OK)
        return result;
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        const auto &card = layout.cards[visible];
        float header = card.header_height, border = layout.border;
        float thumbnail_y = header + 8 * card_scale;
        float close_width = fminf(header, card.source_width);
        float title_maximum = fmaxf(0, card.source_width - close_width - 12 * card_scale);
        float close_radius = fminf(theme->radius_small * card_scale, header * .4f);
        float glyph = close_width * .32f;
        int32_t interactive =
            state.hovered_index == card.index || state.close_hovered_index == card.index;
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
        reach_rect_f32 icon_rect = {(card.source_width - icon_size) * .5f,
                                    thumbnail_y + (card.source_height - icon_size) * .5f, icon_size,
                                    icon_size};
        reach_rect_f32 close_rect = {card.source_width - close_width * .9f, header * .1f,
                                     close_width * .8f, header * .8f};
        reach_rect_f32 glyph_rect = {card.source_width - close_width * .66f,
                                     header * .5f - close_width * .16f, glyph, glyph};
        reach_color base = theme->popup_background, outline = theme->context_menu_hover_background;
        base.a = outline.a = 1;

        reach_render_group parent = {};
        parent.id = card.render_id;
        parent.parent_id = body.id;
        float pad = ceilf(border) + 1;
        parent.bounds = {-pad, 0, ceilf(card.source_width + pad * 2),
                         ceilf(thumbnail_y + card.source_height + pad)};
        parent.x = card.motion.x;
        parent.y = card.header_top;
        parent.scale = card.size;
        result = reach_render_command_buffer_begin_group(out, &parent);
        if (result != REACH_OK)
            return result;
        reach_rect_motion rect = {};
        reach_motion_set(&rect.x, 0);
        reach_motion_set(&rect.y, thumbnail_y);
        reach_motion_set(&rect.width, card.source_width);
        reach_motion_set(&rect.height, card.source_height);
        auto outer = rect;
        outer.x = reach_motion_affine(&outer.x, 1, -border);
        outer.y = reach_motion_affine(&outer.y, 1, -border);
        outer.width = reach_motion_affine(&outer.width, 1, border * 2);
        outer.height = reach_motion_affine(&outer.height, 1, border * 2);
        reach_preview_border_geometry(out, parent.id, border, &outer, &rect);
        if (!interactive)
        {
            auto push = [&](reach_render_command command)
            {
                if (result == REACH_OK)
                    result = reach_render_command_buffer_push(out, &command);
            };
            reach_render_command shape = {};
            shape.type = REACH_RENDER_COMMAND_RECT;
            shape.color = base;
            shape.rect = reach_rect_motion_value_at(&outer, 0);
            push(shape);
            shape.rect = reach_rect_motion_value_at(&rect, 0);
            push(shape);
            if (icon != 0)
            {
                reach_render_command image = {};
                image.type = REACH_RENDER_COMMAND_ICON;
                image.rect = icon_rect;
                image.icon_id = icon;
                image.color = {1, 1, 1, 1};
                push(image);
            }
            if (title_maximum > 0)
            {
                reach_render_command text = {};
                text.type = REACH_RENDER_COMMAND_TEXT;
                text.rect = {8 * card_scale, 0, title_maximum, header};
                text.color = theme->context_menu_text;
                text.text_size = REACH_TEXT_SIZE_MEDIUM * card_scale;
                text.text_weight = REACH_TEXT_WEIGHT_NORMAL;
                text.text_alignment = REACH_TEXT_ALIGNMENT_LEADING;
                text.text_ellipsis = 1;
                reach_copy_utf16(text.text, REACH_MENU_TEXT_CAPACITY, label);
                push(text);
            }
            shape.rect = close_rect;
            shape.radius = close_radius;
            shape.color = reach_theme_color_alpha(theme->context_menu_close_background, .14f);
            push(shape);
            reach_render_command cross = {};
            cross.type = REACH_RENDER_COMMAND_VECTOR_ICON;
            cross.rect = glyph_rect;
            cross.icon_id = REACH_VECTOR_ICON_CLOSE;
            cross.color = theme->context_menu_close_glyph;
            push(cross);
        }
        reach_render_command_buffer_end_group(out);
        if (result != REACH_OK)
            return result;
        if (!interactive)
            continue;

        reach_motion hover = {};
        reach_motion_set(&hover, 0);
        if (state.hovered_index == card.index)
            hover = menu->hover;
        reach_rect_motion strip = {};
        reach_motion_set(&strip.x, 0);
        reach_motion_set(&strip.y, 0);
        reach_motion_set(&strip.width, card.source_width);
        reach_motion_set(&strip.height, header);
        result = reach_preview_fill(
            out, card.render_id + 1, parent.id, strip, card.source_width, header, header * .5f,
            reach_theme_color_alpha(theme->context_menu_hover_background, 1), &hover);
        if (result == REACH_OK)
            result = reach_preview_fill(out, card.render_id + 3, parent.id, outer,
                                        card.source_width + border * 2 + 1,
                                        card.source_height + border * 2 + 1, 0, {1, 1, 1, 1},
                                        nullptr, &hover, base, outline);
        if (result == REACH_OK)
            result = reach_preview_fill(out, card.render_id + 5, parent.id, rect,
                                        card.source_width + 1, card.source_height + 1, 0, base);
        if (result == REACH_OK && icon != 0)
        {
            reach_motion x = {}, y = {}, size = {};
            reach_motion_set(&x, icon_rect.x);
            reach_motion_set(&y, icon_rect.y);
            reach_motion_set(&size, icon_size);
            result = reach_preview_icon(out, card.render_id + 11, parent.id, x, y, size, icon_size,
                                        REACH_RENDER_COMMAND_ICON, icon, {1, 1, 1, 1});
        }
        reach_motion title_width = {};
        reach_motion_set(&title_width, title_maximum);
        if (result == REACH_OK)
            result = reach_preview_text(out, card.render_id + 7, parent.id,
                                        {8 * card_scale, 0, title_maximum, header}, title_width,
                                        label, REACH_TEXT_SIZE_MEDIUM * card_scale,
                                        REACH_TEXT_ALIGNMENT_LEADING, theme->context_menu_text);
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
        if (result == REACH_OK)
            result = reach_preview_fill(
                out, card.render_id + 8, parent.id, close, close_rect.width, close_rect.height,
                close_radius, reach_theme_color_alpha(theme->context_menu_close_background, 1),
                &opacity);
        reach_motion glyph_x = {}, glyph_y = {}, glyph_size = {};
        reach_motion_set(&glyph_x, glyph_rect.x);
        reach_motion_set(&glyph_y, glyph_rect.y);
        reach_motion_set(&glyph_size, glyph);
        if (result == REACH_OK)
            result = reach_preview_icon(out, card.render_id + 10, parent.id, glyph_x, glyph_y,
                                        glyph_size, glyph, REACH_RENDER_COMMAND_VECTOR_ICON,
                                        REACH_VECTOR_ICON_CLOSE, theme->context_menu_close_glyph);
        if (result != REACH_OK)
            return result;
    }
    return REACH_OK;
}
