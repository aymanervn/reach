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

static float reach_context_menu_preview_width(reach_context_menu *menu, float target,
                                              int32_t constraints_changed)
{
    float previous_target =
        reach_animation_manager_target(&menu->animations, REACH_CONTEXT_MENU_ANIM_WIDTH);
    if (constraints_changed || previous_target <= 0.0f ||
        !reach_feature_transition_visible(&menu->popup_transition))
    {
        reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_WIDTH, target);
    }
    else if (fabsf(previous_target - target) >= 0.5f)
    {
        reach_animation_manager_animate_to(&menu->animations, REACH_CONTEXT_MENU_ANIM_WIDTH, target,
                                           reach_preview_width_seconds, REACH_EASING_EASE_IN_OUT);
    }
    else if (previous_target != target &&
             !reach_animation_manager_active(&menu->animations, REACH_CONTEXT_MENU_ANIM_WIDTH))
    {
        reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_WIDTH, target);
    }
    return reach_animation_manager_value(&menu->animations, REACH_CONTEXT_MENU_ANIM_WIDTH);
}

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

int32_t reach_context_menu_arrange_previews(reach_context_menu *menu,
                                            const reach_context_menu_open_context *ctx)
{
    if (ctx->theme == nullptr || ctx->monitor.width <= 0.0f || ctx->monitor.height <= 0.0f)
    {
        return 0;
    }
    reach_context_menu_state *state = &menu->state;
    int32_t monitor_changed = !reach_rect_equal(menu->preview_context.monitor, ctx->monitor);
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
    float row_width = fmaxf(range.width, overflow ? header * 5.0f : header);
    row_width = fminf(room, row_width);
    float popup_width = row_width + (padding + border) * 2.0f;
    popup_width = reach_context_menu_preview_width(menu, popup_width,
                                                   monitor_changed || scale != state->dpi_scale);
    row_width = fmaxf(0.0f, popup_width - (padding + border) * 2.0f);
    float fit = reach_window_preview_row_scale(range.width, row_width);
    float popup_height = height + chrome - margin * 2.0f + (overflow ? header + header_gap : 0.0f);
    reach_popup_anchor anchor = {};
    anchor.button = ctx->anchor_button;
    anchor.monitor = ctx->monitor;
    anchor.bar_edge_y = ctx->bar_edge_y;
    anchor.direction = ctx->drop_direction;
    reach_popup_placement placement = reach_popup_place(&anchor, popup_width, popup_height, margin);
    reach_rect_f32 before = state->bounds;
    float previous_scale = state->dpi_scale;
    state->bounds = placement.bounds;
    state->notch_anchor_x = placement.notch_anchor_x;
    state->anchored = ctx->anchored;
    state->drop_direction = ctx->drop_direction;
    state->dpi_scale = scale;
    state->visible_start = range.start;
    state->visible_count = range.end - range.start;
    state->scroll_offset = 0.0f;
    state->scroll_max = 0.0f;
    state->item_height = header + header_gap + height;
    float y = border + padding + (ctx->drop_direction == REACH_POPUP_DROP_DOWN ? notch : 0.0f);
    float x = border + padding + (row_width - range.width * fit) * 0.5f;
    state->item_viewport = {border + padding, y, row_width, state->item_height};
    layout.count = state->visible_count;
    layout.overflow = overflow;
    for (size_t index = 0; index < REACH_MENU_MAX_WINDOWS; ++index)
    {
        state->item_slots[index] = {};
    }
    int32_t membership_changed = layout.count != menu->previews.count;
    int32_t changed = !reach_rect_equal(before, state->bounds) || scale != previous_scale ||
                      layout.overflow != menu->previews.overflow;
    for (size_t visible = 0; visible < layout.count; ++visible)
    {
        size_t index = range.start + visible;
        reach_context_menu_preview_card *card = &layout.cards[visible];
        card->window = state->item_windows[index];
        card->index = index;
        card->live = live[index];
        float width = widths[index] * fit;
        card->header = {x, y, width, header};
        float close_size = fminf(header, width);
        card->close = {x + width - close_size, y, close_size, header};
        card->preview = {x, y + header + header_gap + height * (1.0f - fit) * 0.5f, width,
                         height * fit};
        state->item_slots[index] = {x, y, width, state->item_height};
        const reach_context_menu_preview_card *previous = &menu->previews.cards[visible];
        membership_changed |= card->window != previous->window || card->live != previous->live;
        changed |= card->index != previous->index;
        x += width + gap * fit;
    }
    if (overflow)
    {
        float footer_y = y + state->item_height + header_gap;
        layout.footer = {border + padding, footer_y, row_width, header};
        if (range.start > 0)
        {
            layout.previous = {border + padding, footer_y, header, header};
        }
        if (range.end < state->item_count)
        {
            layout.next = {border + padding + row_width - header, footer_y, header, header};
        }
    }
    reach_context_menu_reflow_previews(menu, &layout, before,
                                       monitor_changed || scale != previous_scale);
    for (size_t index = 0; index < layout.count; ++index)
    {
        changed |=
            !reach_rect_equal(layout.cards[index].preview, menu->previews.cards[index].preview);
    }
    changed |= !reach_rect_equal(layout.previous, menu->previews.previous) ||
               !reach_rect_equal(layout.next, menu->previews.next);
    if (membership_changed)
    {
        ++menu->preview_generation;
        changed = 1;
    }
    menu->previews = layout;
    menu->request.anchor_button = ctx->anchor_button;
    menu->request.bar_edge_y = ctx->bar_edge_y;
    menu->request.anchored = ctx->anchored;
    menu->request.drop_direction = ctx->drop_direction;
    menu->hover_margin = 12.0f * scale;
    return changed;
}

void reach_context_menu_open_window_list(reach_context_menu *menu, size_t target_index,
                                         const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr || ctx->request == nullptr ||
        ctx->request->window_count == 0)
    {
        return;
    }
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
    menu->state.open = 1;
    reach_feature_transition_set_open(&menu->popup_transition, 1);
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER, 0.0f);
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

reach_result reach_context_menu_thumbnail_at(const reach_context_menu *menu, size_t index,
                                             reach_window_id *out_window,
                                             reach_rect_f32 *out_destination)
{
    if (menu == nullptr || out_window == nullptr || out_destination == nullptr ||
        index >= reach_context_menu_thumbnail_count(menu))
    {
        return REACH_INVALID_ARGUMENT;
    }
    const reach_context_menu_preview_card *card = &menu->previews.cards[index];
    *out_window = card->live ? card->window : 0;
    *out_destination = card->preview;
    out_destination->x += menu->state.bounds.x;
    out_destination->y += menu->state.bounds.y;
    return REACH_OK;
}
