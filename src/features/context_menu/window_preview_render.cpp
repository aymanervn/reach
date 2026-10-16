#include "context_menu_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/ui_controls.h"

#include <math.h>
#include <stdio.h>

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
    reach_popup_background_input popup = {};
    popup.theme = theme;
    popup.bounds = {0, 0,
                    menu->layout_cleanup_pending ? fmaxf(maximum_width, layout.envelope.width)
                                                 : maximum_width,
                    reach_motion_value(&layout.body.height)};
    popup.notch_side = reach_popup_notch_side(state.drop_direction);
    popup.dpi_scale = scale;
    if (result == REACH_OK)
        result = reach_popup_push_width_background(&popup, &layout.body.width, &layout.notch,
                                                   body.id, 8, out);
    if (result != REACH_OK)
        return result;
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        const auto &card = layout.cards[visible];
        float header = card.header_height, border = layout.border;
        reach_render_group parent = {};
        parent.id = card.render_id;
        parent.parent_id = body.id;
        parent.bounds = {0, 0, card.source_width, header + 8 * scale + card.source_height};
        parent.x = card.motion.x;
        reach_motion_set(&parent.y, layout.header_y);
        result = reach_render_command_buffer_begin_group(out, &parent);
        reach_render_command_buffer_end_group(out);
        if (result != REACH_OK)
            return result;
        reach_motion hover = {};
        reach_motion_set(&hover, 0);
        if (state.hovered_index == card.index)
            hover = menu->hover;
        reach_rect_motion rect = {};
        reach_motion_set(&rect.x, 0);
        reach_motion_set(&rect.y, 0);
        rect.width = card.motion.width;
        reach_motion_set(&rect.height, header);
        result = reach_preview_fill(
            out, card.render_id + 1, parent.id, rect, card.source_width, header, header * .5f,
            reach_theme_color_alpha(theme->context_menu_hover_background, 1), &hover);
        if (result != REACH_OK)
            return result;
        rect.y = reach_motion_affine(&card.motion.y, 1, -layout.header_y);
        rect.height = card.motion.height;
        auto outer = rect;
        outer.x = reach_motion_affine(&outer.x, 1, -border);
        outer.y = reach_motion_affine(&outer.y, 1, -border);
        outer.width = reach_motion_affine(&outer.width, 1, border * 2);
        outer.height = reach_motion_affine(&outer.height, 1, border * 2);
        reach_preview_border_geometry(out, parent.id, border, &outer, &rect);
        reach_color base = theme->popup_background, outline = theme->context_menu_hover_background;
        base.a = outline.a = 1;
        result = reach_preview_fill(
            out, card.render_id + 3, parent.id, outer, card.source_width + border * 2 + 1,
            card.source_height + border * 2 + 1, 0, {1, 1, 1, 1}, nullptr, &hover, base, outline);
        if (result == REACH_OK)
            result = reach_preview_fill(out, card.render_id + 5, parent.id, rect,
                                        card.source_width + 1, card.source_height + 1, 0, base);
        if (result != REACH_OK)
            return result;
        auto close_width = reach_motion_clamp(&card.motion.width, 0, header);
        reach_motion title_width = {};
        if (reach_motion_combine(&card.motion.width, 1, &close_width, -1, -12 * scale,
                                 &title_width) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        float title_maximum =
            fmaxf(0, card.source_width - fminf(header, card.source_width) - 12 * scale);
        title_width = reach_motion_clamp(&title_width, 0, title_maximum);
        const auto *label = state.item_titles[card.index][0] ? state.item_titles[card.index]
                                                             : (const uint16_t *)L"App";
        result = reach_preview_text(out, card.render_id + 7, parent.id,
                                    {8 * scale, 0, title_maximum, header}, title_width, label,
                                    REACH_TEXT_SIZE_MEDIUM * scale, REACH_TEXT_ALIGNMENT_LEADING,
                                    theme->context_menu_text);
        if (result != REACH_OK)
            return result;
        reach_motion close_hover = {};
        reach_motion_set(&close_hover, 0);
        if (state.close_hovered_index == card.index)
            close_hover = menu->close_hover;
        auto opacity = reach_motion_affine(&close_hover, .74f, .14f);
        reach_rect_motion close = {};
        if (reach_motion_combine(&card.motion.width, 1, &close_width, -.9f, 0, &close.x) !=
            REACH_OK)
            return REACH_INVALID_ARGUMENT;
        reach_motion_set(&close.y, header * .1f);
        close.width = reach_motion_affine(&close_width, .8f, 0);
        reach_motion_set(&close.height, header * .8f);
        result = reach_preview_fill(
            out, card.render_id + 8, parent.id, close, fminf(header, card.source_width) * .8f,
            header * .8f, fminf(theme->radius_small * scale, header * .4f),
            reach_theme_color_alpha(theme->context_menu_close_background, 1), &opacity);
        if (result != REACH_OK)
            return result;
        auto glyph_size = reach_motion_affine(&close_width, .32f, 0);
        reach_motion glyph_x = {}, glyph_y = reach_motion_affine(&close_width, -.16f, header * .5f);
        if (reach_motion_combine(&card.motion.width, 1, &close_width, -.66f, 0, &glyph_x) !=
            REACH_OK)
            return REACH_INVALID_ARGUMENT;
        result = reach_preview_icon(out, card.render_id + 10, parent.id, glyph_x, glyph_y,
                                    glyph_size, fminf(header, card.source_width) * .32f,
                                    REACH_RENDER_COMMAND_VECTOR_ICON, REACH_VECTOR_ICON_CLOSE,
                                    theme->context_menu_close_glyph);
        if (result != REACH_OK)
            return result;
        if (!card.live)
        {
            const auto *path = reach_context_menu_preview_icon_path(menu, card.window);
            uint64_t icon =
                path && path[0] ? reach_icon_service_get(menu->icons, path, ctx->icon_size_px) : 0;
            float maximum = fminf(64 * scale, fminf(card.source_width, card.source_height));
            auto size = reach_motion_clamp(&card.motion.width, 0, maximum);
            reach_motion x = {}, y = {};
            if (reach_motion_min(&size, &card.motion.height, &size) != REACH_OK ||
                reach_motion_combine(&card.motion.width, .5f, &size, -.5f, 0, &x) != REACH_OK ||
                reach_motion_combine(&card.motion.y, 1, &card.motion.height, .5f, -layout.header_y,
                                     &y) != REACH_OK ||
                reach_motion_combine(&y, 1, &size, -.5f, 0, &y) != REACH_OK)
                return REACH_INVALID_ARGUMENT;
            result = reach_preview_icon(out, card.render_id + 11, parent.id, x, y, size, maximum,
                                        REACH_RENDER_COMMAND_ICON, icon, {1, 1, 1, 1});
            if (result != REACH_OK)
                return result;
        }
    }
    if (layout.overflow)
    {
        uint16_t label[64] = {};
        swprintf_s((wchar_t *)label, 64, L"%zu\u2013%zu of %zu", state.visible_start + 1,
                   state.visible_start + state.visible_count, state.item_count);
        float header = reach_motion_value(&layout.footer.height);
        float width = 0;
        if (reach_motion_range(&layout.footer.width, &minimum, &width) != REACH_OK)
            return REACH_INVALID_ARGUMENT;
        auto available = reach_motion_affine(&layout.footer.width, 1, -header * 2);
        available = reach_motion_clamp(&available, 0, fmaxf(0, width - header * 2));
        result = reach_preview_text(out, 1024, body.id,
                                    {reach_motion_value(&layout.footer.x) + header,
                                     reach_motion_value(&layout.footer.y),
                                     fmaxf(0, width - header * 2), header},
                                    available, label, REACH_TEXT_SIZE_SMALL * scale,
                                    REACH_TEXT_ALIGNMENT_CENTER, theme->context_menu_text);
        if (result != REACH_OK)
            return result;
        const reach_rect_motion *buttons[] = {&layout.previous, &layout.next};
        const uint16_t *arrows[] = {(const uint16_t *)L"\u2039", (const uint16_t *)L"\u203a"};
        for (size_t index = 0; index < 2; ++index)
        {
            if (reach_motion_value(&buttons[index]->width) <= 0)
                continue;
            reach_render_group arrow = {};
            arrow.id = 1025 + index;
            arrow.parent_id = body.id;
            arrow.bounds = {0, 0, header, header};
            arrow.x = buttons[index]->x;
            arrow.y = buttons[index]->y;
            result = reach_render_command_buffer_begin_group(out, &arrow);
            reach_render_command command = {};
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect = arrow.bounds;
            command.color = theme->context_menu_text;
            command.text_size = REACH_TEXT_SIZE_HEADING * scale;
            command.text_weight = REACH_TEXT_WEIGHT_NORMAL;
            command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
            reach_copy_utf16(command.text, REACH_MENU_TEXT_CAPACITY, arrows[index]);
            if (result == REACH_OK)
                result = reach_render_command_buffer_push(out, &command);
            reach_render_command_buffer_end_group(out);
            if (result != REACH_OK)
                return result;
        }
    }
    return REACH_OK;
}
