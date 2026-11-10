#include "context_menu_common.h"
#include "reach/features/common/window_preview_layout.h"

#include <math.h>

static constexpr float reach_preview_height_dp = 140.0f;
static constexpr float reach_preview_padding_dp = 12.0f;
static constexpr float reach_preview_gap_dp = 12.0f;
static constexpr float reach_preview_header_dp = 24.0f;
static constexpr float reach_preview_header_gap_dp = 8.0f;
static constexpr float reach_preview_margin_dp = 16.0f;
static constexpr double reach_preview_width_seconds = 0.18;

void reach_context_menu_attach_services(reach_context_menu *menu, reach_window_tracking *windows,
                                        reach_icon_service *icons)
{
    if (menu != nullptr)
    {
        menu->windows = windows;
        menu->icons = icons;
    }
}

const uint16_t *reach_context_menu_preview_icon_path(const reach_context_menu *menu,
                                                     reach_window_id window_id)
{
    const reach_window_snapshot *window =
        reach_window_tracking_window_by_id(menu->windows, window_id);
    return window != nullptr
               ? (window->icon_ref[0] != 0
                      ? window->icon_ref
                      : reach_application_identity_primary_runtime_path(&window->identity))
               : menu->request.icon_ref;
}

void reach_context_menu_touch_icons(reach_context_menu *menu, int32_t icon_size_px)
{
    if (menu == nullptr || !menu->state.window_list_open)
    {
        return;
    }
    for (size_t index = 0; index < menu->previews.count; ++index)
    {
        const reach_context_menu_preview_card *card = &menu->previews.cards[index];
        if (!card->live)
        {
            reach_icon_service_touch(menu->icons,
                                     reach_context_menu_preview_icon_path(menu, card->window),
                                     icon_size_px);
        }
    }
}

static void reach_context_menu_load_windows(reach_context_menu *menu,
                                            const reach_menu_request *request)
{
    reach_window_id first = menu->state.visible_start < menu->state.item_count
                                ? menu->state.item_windows[menu->state.visible_start]
                                : 0;
    menu->request = *request;
    menu->state.item_count = 0;
    for (size_t index = 0; index < request->window_count && index < REACH_MENU_MAX_WINDOWS; ++index)
    {
        reach_window_id window = request->windows[index].window;
        if (window == 0 || (menu->windows != nullptr &&
                            reach_window_tracking_window_by_id(menu->windows, window) == nullptr))
        {
            continue;
        }
        size_t out = menu->state.item_count++;
        menu->state.item_windows[out] = window;
        reach_copy_utf16(menu->state.item_titles[out], REACH_MENU_TEXT_CAPACITY,
                         request->windows[index].title);
        if (window == first)
        {
            menu->state.visible_start = out;
        }
    }
    menu->state.hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    menu->state.close_hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
}

static int32_t reach_context_menu_arrange_previews_as(reach_context_menu *menu,
                                                      const reach_context_menu_open_context *ctx,
                                                      int32_t force_cut)
{
    if (ctx->theme == nullptr || ctx->monitor.width <= 0.0f || ctx->monitor.height <= 0.0f)
    {
        return 0;
    }
    reach_context_menu_state *state = &menu->state;
    if (!state->open && menu->layout_initialized &&
        reach_feature_transition_visible(&menu->popup_transition))
        return 0;
    reach_context_menu_open_context previous_context = menu->preview_context;
    int32_t monitor_changed = !reach_rect_equal(previous_context.monitor, ctx->monitor);
    menu->preview_context = *ctx;
    menu->preview_context.request = nullptr;
    menu->preview_context.item_commands = nullptr;
    float available_height = ctx->monitor.height;
    if (ctx->anchored)
    {
        available_height = ctx->drop_direction == REACH_POPUP_DROP_UP
                               ? ctx->bar_edge_y - ctx->monitor.y
                               : ctx->monitor.y + ctx->monitor.height - ctx->bar_edge_y;
        available_height = fmaxf(1.0f, fminf(available_height, ctx->monitor.height));
    }
    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    float base_border = reach_theme_border_thickness(ctx->theme, 1.0f);
    scale = fminf(scale, ctx->monitor.width / (2.0f * (reach_preview_margin_dp +
                                                       reach_preview_padding_dp + base_border) +
                                               1.0f));
    float chrome_units = 2.0f * (reach_preview_margin_dp + reach_preview_padding_dp + base_border) +
                         reach_popup_notch_height() + reach_preview_header_dp * 2.0f +
                         reach_preview_header_gap_dp * 2.0f;
    scale = fminf(scale, available_height / (chrome_units + 1.0f));
    float border = reach_theme_border_thickness(ctx->theme, scale);
    float padding = reach_preview_padding_dp * scale;
    float gap = reach_preview_gap_dp * scale;
    float header = reach_preview_header_dp * scale;
    float header_gap = reach_preview_header_gap_dp * scale;
    float margin = reach_preview_margin_dp * scale;
    float notch = reach_popup_notch_height_scaled(scale);
    float room = fmaxf(0.0f, ctx->monitor.width - 2.0f * (margin + border + padding));
    float chrome = 2.0f * (margin + border + padding) + notch + header + header_gap;
    float aspects[REACH_MENU_MAX_WINDOWS] = {};
    int32_t live[REACH_MENU_MAX_WINDOWS] = {};
    for (size_t index = 0; index < state->item_count; ++index)
    {
        reach_rect_f32 frame = {};
        int32_t valid = reach_window_tracking_window_frame_bounds(
                            menu->windows, state->item_windows[index], &frame) == REACH_OK &&
                        isfinite(frame.width) && isfinite(frame.height);
        aspects[index] = reach_window_preview_aspect(frame);
        const reach_window_snapshot *window =
            reach_window_tracking_window_by_id(menu->windows, state->item_windows[index]);
        live[index] = valid && window != nullptr && !window->minimized;
        if (window != nullptr)
        {
            if (window->title[0] != 0)
            {
                reach_copy_utf16(state->item_titles[index], REACH_MENU_TEXT_CAPACITY,
                                 window->title);
            }
            else
            {
                reach_window_tracking_app_display_name(window, state->item_titles[index],
                                                       REACH_MENU_TEXT_CAPACITY);
            }
        }
    }
    float height =
        reach_window_preview_height(aspects, state->item_count, reach_preview_height_dp * scale,
                                    available_height - chrome, room);
    float widths[REACH_MENU_MAX_WINDOWS] = {};
    for (size_t index = 0; index < state->item_count; ++index)
    {
        widths[index] = aspects[index] * height;
    }
    reach_window_preview_range full =
        reach_window_preview_pack(widths, state->item_count, 0, REACH_MENU_MAX_WINDOWS, room, gap);
    int32_t overflow = full.end < state->item_count;
    if (overflow)
    {
        height =
            reach_window_preview_height(aspects, state->item_count, reach_preview_height_dp * scale,
                                        available_height - chrome - header - header_gap, room);
        for (size_t index = 0; index < state->item_count; ++index)
        {
            widths[index] = aspects[index] * height;
        }
    }
    size_t start = state->item_count > 0 ? state->visible_start : 0;
    if (start >= state->item_count && state->item_count > 0)
    {
        start = state->item_count - 1;
    }
    if (!overflow)
    {
        start = 0;
    }
    reach_context_menu_preview_layout layout = {};
    for (size_t page_start = 0; page_start < state->item_count;)
    {
        size_t page = layout.page_count++;
        layout.page_starts[page] = page_start;
        if (page_start <= start)
        {
            layout.page_index = page;
        }
        reach_window_preview_range page_range = reach_window_preview_pack(
            widths, state->item_count, page_start, REACH_MENU_MAX_WINDOWS, room, gap);
        page_start = page_range.end;
    }
    start = layout.page_starts[layout.page_index];
    reach_window_preview_range range = reach_window_preview_pack(widths, state->item_count, start,
                                                                 REACH_MENU_MAX_WINDOWS, room, gap);
    float row_width = fminf(room, fmaxf(range.width, overflow ? header * 5 : header));
    float target_width = row_width + (padding + border) * 2;
    float popup_height = height + chrome - margin * 2 + (overflow ? header + header_gap : 0);
    double now = reach_motion_now_seconds();
    int32_t cut = force_cut || !menu->layout_initialized ||
                  !reach_feature_transition_visible(&menu->popup_transition) || monitor_changed ||
                  scale != state->dpi_scale || border != menu->previews.border ||
                  !reach_rect_equal(previous_context.anchor_button, ctx->anchor_button) ||
                  previous_context.drop_direction != ctx->drop_direction ||
                  previous_context.bar_edge_y != ctx->bar_edge_y;
    layout.count = range.end - range.start;
    layout.overflow = overflow;
    layout.border = border;
    layout.header_y = border + padding + (ctx->drop_direction == REACH_POPUP_DROP_DOWN ? notch : 0);
    int32_t sources_changed = layout.count != menu->previews.count;
    int32_t membership_changed = sources_changed;
    int32_t source_geometry_changed = 0;
    size_t survivors = 0;
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        size_t index = range.start + visible;
        auto &card = layout.cards[visible];
        card.window = state->item_windows[index];
        card.index = index;
        card.live = live[index];
        card.source_width = widths[index];
        card.source_height = height;
        card.header_height = header;
        source_geometry_changed |=
            card.source_width != menu->previews.cards[visible].source_width ||
            card.source_height != menu->previews.cards[visible].source_height;
        sources_changed |= card.window != menu->previews.cards[visible].window;
        membership_changed |= card.window != menu->previews.cards[visible].window ||
                              card.live != menu->previews.cards[visible].live;
        for (size_t old = 0; old < menu->previews.count; ++old)
            if (menu->previews.cards[old].window == card.window)
            {
                ++survivors;
                card.render_id = menu->previews.cards[old].render_id;
            }
        if (card.render_id == 0)
        {
            card.render_id = menu->next_card_id;
            menu->next_card_id += 16;
        }
    }
    int32_t restart =
        sources_changed || source_geometry_changed || fabsf(menu->width.to - target_width) >= .5f;
    if (cut)
    {
        reach_motion_set(&menu->reflow, 1);
        reach_motion_set(&menu->width, target_width);
        menu->layout_cleanup_pending = 0;
    }
    else if (state->open && restart)
    {
        float current = reach_motion_value_at(&menu->width, now);
        reach_motion_start_at(&menu->reflow, 0, 1, reach_preview_width_seconds,
                              REACH_EASING_EASE_IN_OUT, now);
        menu->width = reach_motion_affine(&menu->reflow, target_width - current, current);
        menu->layout_cleanup_pending = 1;
    }
    else if (state->open && !reach_motion_active_at(&menu->reflow, now) &&
             menu->width.to != target_width)
        reach_motion_set(&menu->width, target_width);
    reach_popup_anchor anchor = {};
    anchor.button = ctx->anchor_button;
    anchor.monitor = ctx->monitor;
    anchor.bar_edge_y = ctx->bar_edge_y;
    anchor.direction = ctx->drop_direction;
    if (reach_popup_width_motion(&anchor, &menu->width, popup_height, margin, scale, &layout.body,
                                 &layout.notch) != REACH_OK)
        return 0;
    float near_x = 0, far_x = 0, minimum = 0, maximum = 0;
    reach_motion far = {};
    if (reach_motion_range(&layout.body.x, &near_x, &maximum) != REACH_OK ||
        reach_motion_combine(&layout.body.x, 1, &layout.body.width, 1, 0, &far) != REACH_OK ||
        reach_motion_range(&far, &minimum, &far_x) != REACH_OK)
        return 0;
    float top = reach_motion_value_at(&layout.body.y, now);
    layout.envelope = {near_x, top, far_x - near_x, popup_height};
    if (!cut)
    {
        far_x = fmaxf(far_x, menu->previews.envelope.x + menu->previews.envelope.width);
        float bottom =
            fmaxf(top + popup_height, menu->previews.envelope.y + menu->previews.envelope.height);
        layout.envelope.x = fminf(near_x, menu->previews.envelope.x);
        layout.envelope.y = fminf(top, menu->previews.envelope.y);
        layout.envelope.width = far_x - layout.envelope.x;
        layout.envelope.height = bottom - layout.envelope.y;
    }
    layout.body.x = reach_motion_affine(&layout.body.x, 1, -layout.envelope.x);
    layout.body.y = reach_motion_affine(&layout.body.y, 1, -layout.envelope.y);
    reach_motion fit = {};
    auto available = reach_motion_affine(&menu->width, 1, -(padding + border) * 2);
    available = reach_motion_clamp(&available, 0, room);
    if (range.width > 0)
    {
        fit = reach_motion_affine(&available, 1 / range.width, 0);
        fit = reach_motion_clamp(&fit, 0, 1);
    }
    else
        reach_motion_set(&fit, 1);
    reach_motion_set(&layout.viewport.x, border + padding);
    reach_motion_set(&layout.viewport.y, layout.header_y);
    layout.viewport.width = available;
    reach_motion_set(&layout.viewport.height, header + header_gap + height);
    float prefix = 0;
    auto center = reach_motion_affine(&menu->width, .5f, 0);
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        auto &card = layout.cards[visible];
        if (reach_motion_combine(&center, 1, &fit, prefix - range.width * .5f, 0, &card.motion.x) !=
            REACH_OK)
            return 0;
        card.motion.y = reach_motion_affine(&fit, -height * .5f,
                                            layout.header_y + header + header_gap + height * .5f);
        card.motion.width = reach_motion_affine(&fit, card.source_width, 0);
        card.motion.height = reach_motion_affine(&fit, height, 0);
        prefix += card.source_width + gap;
    }
    float footer_y = layout.header_y + header + header_gap + height + header_gap;
    if (overflow)
    {
        reach_motion_set(&layout.footer.x, border + padding);
        reach_motion_set(&layout.footer.y, footer_y);
        layout.footer.width = available;
        reach_motion_set(&layout.footer.height, header);
        if (range.start > 0)
        {
            layout.previous = layout.footer;
            reach_motion_set(&layout.previous.width, header);
        }
        if (range.end < state->item_count)
        {
            layout.next = layout.footer;
            layout.next.x = reach_motion_affine(&menu->width, 1, -border - padding - header);
            reach_motion_set(&layout.next.width, header);
        }
    }
    state->dpi_scale = scale;
    if (reach_context_menu_reflow_previews(menu, &layout, now, restart,
                                           cut || (sources_changed && survivors == 0)) != REACH_OK)
        return 0;
    int32_t changed = !reach_rect_equal(layout.envelope, menu->previews.envelope) ||
                      !reach_motion_equal(&layout.body.width, &menu->previews.body.width) ||
                      !reach_motion_equal(&layout.body.x, &menu->previews.body.x) ||
                      !reach_motion_equal(&layout.body.y, &menu->previews.body.y) ||
                      !reach_motion_equal(&layout.body.height, &menu->previews.body.height) ||
                      !reach_motion_equal(&layout.notch, &menu->previews.notch) ||
                      layout.overflow != menu->previews.overflow;
    for (size_t index = 0; index < layout.count; ++index)
    {
        const auto &card = layout.cards[index];
        const auto &old = menu->previews.cards[index];
        changed |=
            card.index != old.index ||
            (card.source_width != old.source_width || card.source_height != old.source_height ||
             card.header_height != old.header_height) ||
            !reach_motion_equal(&card.motion.x, &old.motion.x) ||
            !reach_motion_equal(&card.motion.y, &old.motion.y) ||
            !reach_motion_equal(&card.motion.width, &old.motion.width) ||
            !reach_motion_equal(&card.motion.height, &old.motion.height);
    }
    if (membership_changed)
    {
        ++menu->preview_generation;
        changed = 1;
    }
    menu->previews = layout;
    menu->layout_initialized = 1;
    state->anchored = ctx->anchored;
    state->drop_direction = ctx->drop_direction;
    state->notch_anchor_x = ctx->anchor_button.x + ctx->anchor_button.width * .5f;
    state->visible_start = range.start;
    state->visible_count = layout.count;
    state->scroll_offset = state->scroll_max = 0;
    state->item_height = header + header_gap + height;
    (void)reach_context_menu_state_ptr(menu);
    menu->request.anchor_button = ctx->anchor_button;
    menu->request.bar_edge_y = ctx->bar_edge_y;
    menu->request.anchored = ctx->anchored;
    menu->request.drop_direction = ctx->drop_direction;
    menu->hover_margin = 12.0f * scale;
    reach_context_menu_anchor_presentation(menu);
    return changed;
}

int32_t reach_context_menu_arrange_previews(reach_context_menu *menu,
                                            const reach_context_menu_open_context *ctx)
{
    return reach_context_menu_arrange_previews_as(menu, ctx, 0);
}

int32_t reach_context_menu_settle_previews(reach_context_menu *menu)
{
    return reach_context_menu_arrange_previews_as(menu, &menu->preview_context, 1);
}

void reach_context_menu_open_window_list(reach_context_menu *menu, size_t target_index,
                                         const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr || ctx->request == nullptr ||
        ctx->request->window_count == 0)
    {
        return;
    }
    if (!menu->state.open)
        reach_context_menu_reset_preview_layout(menu);
    menu->state.item_count = 0;
    menu->state.visible_start = 0;
    reach_context_menu_load_windows(menu, ctx->request);
    if (menu->state.item_count == 0)
    {
        return;
    }
    menu->state.power_open = 0;
    menu->state.window_list_open = 1;
    menu->state.target_index = target_index;
    reach_context_menu_arrange_previews(menu, ctx);
    reach_feature_transition_configure(&menu->popup_transition, ctx->theme, menu->state.dpi_scale,
                                       ctx->drop_direction);
    reach_context_menu_anchor_presentation(menu);
    menu->state.open = 1;
    reach_feature_transition_set_open(&menu->popup_transition, 1);
    reach_motion_set(&menu->hover, 0.0f);
    reach_motion_set(&menu->close_hover, 0.0f);
}

void reach_context_menu_refresh_window_list(reach_context_menu *menu,
                                            const reach_menu_request *request)
{
    if (menu == nullptr || !menu->state.open || !menu->state.window_list_open || request == nullptr)
    {
        return;
    }
    reach_context_menu_load_windows(menu, request);
    menu->state.target_index = request->target_index;
    if (menu->state.item_count == 0)
    {
        reach_context_menu_set_open(menu, 0);
    }
    else
    {
        reach_context_menu_open_context ctx = menu->preview_context;
        ctx.anchor_button = request->anchor_button;
        ctx.bar_edge_y = request->bar_edge_y;
        ctx.drop_direction = request->drop_direction;
        ctx.anchored = request->anchored;
        reach_context_menu_arrange_previews(menu, &ctx);
    }
}

size_t reach_context_menu_window_list_remove(reach_context_menu *menu, uintptr_t window)
{
    if (menu == nullptr || !menu->state.window_list_open)
    {
        return menu != nullptr ? menu->state.item_count : 0;
    }
    for (size_t index = 0; index < menu->state.item_count; ++index)
    {
        if (menu->state.item_windows[index] != window)
        {
            continue;
        }
        for (size_t next = index + 1; next < menu->state.item_count; ++next)
        {
            menu->state.item_windows[next - 1] = menu->state.item_windows[next];
            reach_copy_utf16(menu->state.item_titles[next - 1], REACH_MENU_TEXT_CAPACITY,
                             menu->state.item_titles[next]);
        }
        --menu->state.item_count;
        menu->state.item_windows[menu->state.item_count] = 0;
        menu->state.item_titles[menu->state.item_count][0] = 0;
        if (index < menu->state.visible_start)
        {
            --menu->state.visible_start;
        }
        menu->state.hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
        menu->state.close_hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
        reach_context_menu_arrange_previews(menu, &menu->preview_context);
        break;
    }
    return menu->state.item_count;
}

void reach_context_menu_notify_windows_changed(reach_context_menu *menu,
                                               reach_feature_tick_result *out)
{
    if (menu == nullptr || out == nullptr || !menu->state.open || !menu->state.window_list_open)
    {
        return;
    }
    for (size_t index = menu->state.item_count; index > 0; --index)
    {
        if (reach_window_tracking_window_by_id(menu->windows,
                                               menu->state.item_windows[index - 1]) == nullptr)
        {
            reach_context_menu_window_list_remove(menu, menu->state.item_windows[index - 1]);
        }
    }
    if (menu->state.item_count == 0)
    {
        reach_context_menu_set_open(menu, 0);
    }
    else
    {
        reach_context_menu_arrange_previews(menu, &menu->preview_context);
    }
    out->redraw = 1;
    out->relayout = 1;
    out->request_update = 1;
}

int32_t reach_context_menu_window_list_page(reach_context_menu *menu, int32_t direction)
{
    if (menu == nullptr || !menu->state.open || !menu->state.window_list_open || direction == 0)
    {
        return 0;
    }
    size_t start = menu->state.visible_start;
    size_t page = menu->previews.page_index;
    if (direction > 0 && page + 1 < menu->previews.page_count)
    {
        menu->state.visible_start = menu->previews.page_starts[page + 1];
    }
    else if (direction < 0 && page > 0)
    {
        menu->state.visible_start = menu->previews.page_starts[page - 1];
    }
    if (menu->state.visible_start == start)
    {
        return 0;
    }
    menu->state.hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    menu->state.close_hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    reach_context_menu_arrange_previews(menu, &menu->preview_context);
    return 1;
}

size_t reach_context_menu_preview_generation(const reach_context_menu *menu)
{
    return menu != nullptr ? menu->preview_generation : 0;
}

size_t reach_context_menu_thumbnail_count(const reach_context_menu *menu)
{
    return menu != nullptr && menu->state.window_list_open ? menu->previews.count : 0;
}

reach_result reach_context_menu_thumbnail_motion_at(const reach_context_menu *menu, size_t index,
                                                    reach_window_id *out_window,
                                                    reach_rect_motion *out_destination)
{
    if (menu == nullptr || out_window == nullptr || out_destination == nullptr ||
        index >= reach_context_menu_thumbnail_count(menu))
        return REACH_INVALID_ARGUMENT;
    const reach_context_menu_preview_card *card = &menu->previews.cards[index];
    *out_window = card->live ? card->window : 0;
    *out_destination = card->motion;
    if (reach_motion_combine(&menu->previews.body.x, 1, &card->motion.x, 1,
                             menu->previews.envelope.x, &out_destination->x) != REACH_OK ||
        reach_motion_combine(&menu->previews.body.y, 1, &card->motion.y, 1,
                             menu->previews.envelope.y, &out_destination->y) != REACH_OK)
        return REACH_INVALID_ARGUMENT;
    return REACH_OK;
}

reach_result reach_context_menu_thumbnail_at(const reach_context_menu *menu, size_t index,
                                             reach_window_id *out_window,
                                             reach_rect_f32 *out_destination)
{
    if (out_destination == nullptr)
        return REACH_INVALID_ARGUMENT;
    reach_rect_motion motion = {};
    reach_result result = reach_context_menu_thumbnail_motion_at(menu, index, out_window, &motion);
    if (result == REACH_OK)
        *out_destination = reach_rect_motion_value_at(&motion, reach_motion_now_seconds());
    return result;
}
