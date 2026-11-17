#include "reach/features/clipboard.h"
#include "reach/features/common/presentation.h"
#include "reach/support/motion.h"

#include "clipboard_common.h"
#include "clipboard_metrics.h"

#include <atomic>
#include <initializer_list>
#include <math.h>
#include <new>

static float reach_clipboard_max_float(float a, float b)
{
    return a > b ? a : b;
}

static float reach_clipboard_min_float(float a, float b)
{
    return a < b ? a : b;
}

static float reach_clipboard_clamp_float(float value, float minimum, float maximum)
{
    if (maximum < minimum)
    {
        return minimum;
    }
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static int32_t reach_clipboard_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x < rect.x + rect.width && (float)y >= rect.y && (float)y < rect.y + rect.height;
}

static reach_rect_f32 reach_clipboard_close_button_for(reach_rect_f32 item, float border_thickness,
                                                       const reach_clipboard_metrics &metrics)
{
    float item_border = reach_clipboard_min_float(
        border_thickness, reach_clipboard_min_float(item.width, item.height) * 0.5f);
    return {item.x + item.width - item_border - metrics.close_button_size -
                metrics.close_button_margin,
            item.y + item_border + metrics.close_button_margin, metrics.close_button_size,
            metrics.close_button_size};
}

reach_clipboard_layout reach_clipboard_compute_layout(reach_clipboard_model *model,
                                                      reach_rect_f32 monitor_bounds,
                                                      reach_rect_f32 launcher_bounds,
                                                      float dpi_scale, float border_thickness)
{
    reach_clipboard_layout layout = {};
    if (model == nullptr)
    {
        return layout;
    }

    (void)launcher_bounds;

    const reach_clipboard_metrics metrics = reach_clipboard_metrics_for_scale(dpi_scale);
    border_thickness = reach_clipboard_max_float(0.0f, border_thickness);
    const float screen_edge_margin = metrics.screen_edge_margin;
    const float available_width =
        reach_clipboard_max_float(0.0f, monitor_bounds.width - screen_edge_margin * 2.0f);
    const float available_height =
        reach_clipboard_max_float(0.0f, monitor_bounds.height - screen_edge_margin * 2.0f);

    const float width =
        reach_clipboard_min_float(metrics.panel_width + border_thickness * 2.0f, available_width);

    const float max_items_height = metrics.item_large_size * 4.0f + metrics.item_gap * 3.0f;
    const float chrome_height =
        border_thickness * 2.0f + 2.0f * metrics.padding + metrics.title_height + metrics.title_gap;
    const float default_height = chrome_height + metrics.item_default_size;
    const float max_height =
        reach_clipboard_min_float(chrome_height + max_items_height, available_height);

    const float available_for_items = reach_clipboard_max_float(0.0f, max_height - chrome_height);
    size_t max_visible_items = 0;
    if (available_for_items > 0.0f)
    {
        max_visible_items = (size_t)((available_for_items + metrics.item_gap) /
                                     (metrics.item_large_size + metrics.item_gap));
        if (max_visible_items > 4)
        {
            max_visible_items = 4;
        }
    }

    const size_t item_count = reach_clipboard_count_clamped(model);
    const size_t visible_count = item_count < max_visible_items ? item_count : max_visible_items;
    const float visible_items_height = visible_count > 0
                                           ? metrics.item_large_size * (float)visible_count +
                                                 metrics.item_gap * (float)(visible_count - 1)
                                           : 0.0f;

    float height = chrome_height + visible_items_height;
    if (height < default_height)
    {
        height = default_height;
    }
    height = reach_clipboard_min_float(height, available_height);

    const float monitor_left = monitor_bounds.x + screen_edge_margin;
    const float monitor_top = monitor_bounds.y + screen_edge_margin;
    const float monitor_right = monitor_bounds.x + monitor_bounds.width - screen_edge_margin;
    const float monitor_bottom = monitor_bounds.y + monitor_bounds.height - screen_edge_margin;

    layout.bounds.x =
        reach_clipboard_clamp_float(monitor_left, monitor_left, monitor_right - width);
    layout.bounds.y =
        reach_clipboard_clamp_float(monitor_bottom - height, monitor_top, monitor_bottom - height);
    layout.bounds.width = width;
    layout.bounds.height = height;
    layout.chrome_height = reach_clipboard_min_float(chrome_height, height);
    layout.max_height = reach_clipboard_max_float(height, max_height);

    const float horizontal_border =
        reach_clipboard_min_float(border_thickness, layout.bounds.width * 0.5f);
    const float vertical_border =
        reach_clipboard_min_float(border_thickness, layout.bounds.height * 0.5f);
    const float content_width =
        reach_clipboard_max_float(0.0f, width - horizontal_border * 2.0f - metrics.padding * 2.0f);
    const float clear_button_height =
        reach_clipboard_min_float(metrics.clear_button_height, metrics.title_height);
    layout.clear_button = {layout.bounds.x + layout.bounds.width - horizontal_border -
                               metrics.padding - metrics.clear_button_width,
                           layout.bounds.y + vertical_border + metrics.padding +
                               (metrics.title_height - clear_button_height) * 0.5f,
                           metrics.clear_button_width, clear_button_height};

    const float title_width = reach_clipboard_max_float(
        0.0f, content_width - metrics.clear_button_width - metrics.clear_button_gap);
    layout.title = {layout.bounds.x + horizontal_border + metrics.padding,
                    layout.bounds.y + vertical_border + metrics.padding, title_width,
                    metrics.title_height};

    layout.viewport = {
        layout.bounds.x + horizontal_border + metrics.padding,
        layout.title.y + metrics.title_height + metrics.title_gap, content_width,
        reach_clipboard_max_float(0.0f, height - vertical_border * 2.0f - metrics.padding * 2.0f -
                                            metrics.title_height - metrics.title_gap)};

    layout.item_large_size = metrics.item_large_size;
    layout.item_pitch = metrics.item_large_size + metrics.item_gap;
    layout.content_height = item_count > 0 ? metrics.item_large_size * (float)item_count +
                                                 metrics.item_gap * (float)(item_count - 1)
                                           : 0.0f;
    layout.item_text_width = reach_clipboard_max_float(
        0.0f, layout.viewport.width - metrics.scrollbar_gutter - metrics.item_scrollbar_gap);
    const int32_t needs_scrollbar =
        layout.content_height > layout.viewport.height && layout.viewport.height > 0.0f;
    layout.item_width = needs_scrollbar ? layout.item_text_width : layout.viewport.width;

    reach_scroll_view_config scroll = model->scroll.config;
    scroll.minimum_thumb = metrics.thumb_size;
    reach_scroll_view_configure(&model->scroll, &scroll);
    reach_scroll_view_set_extents(&model->scroll, layout.content_height, layout.viewport.height);
    (void)reach_scroll_view_sync(&model->scroll, nullptr, REACH_SCROLL_VIEW_WHEEL,
                                 reach_motion_now_seconds());
    const float scroll_offset = reach_scroll_view_position(&model->scroll);

    for (size_t index = 0; index < item_count; ++index)
    {
        layout.items[index] = {layout.viewport.x,
                               layout.viewport.y + layout.item_pitch * (float)index - scroll_offset,
                               layout.item_width, metrics.item_large_size};
        layout.close_buttons[index] =
            reach_clipboard_close_button_for(layout.items[index], border_thickness, metrics);
    }

    if (needs_scrollbar)
    {
        layout.scrollbar_track = {layout.viewport.x + layout.item_text_width +
                                      metrics.item_scrollbar_gap + metrics.track_x_offset,
                                  layout.viewport.y, metrics.track_width, layout.viewport.height};
    }
    reach_scroll_view_set_frame(&model->scroll, layout.viewport, layout.scrollbar_track);

    return layout;
}

reach_clipboard_hit_result reach_clipboard_hit_test(const reach_clipboard_model *model,
                                                    const reach_clipboard_layout *layout, int32_t x,
                                                    int32_t y)
{
    reach_clipboard_hit_result result = {};
    result.index = REACH_CLIPBOARD_MAX_ITEMS;
    if (model == nullptr || layout == nullptr || !model->open)
    {
        return result;
    }

    if (model->count > 0 && reach_clipboard_contains(layout->clear_button, x, y))
    {
        result.type = REACH_CLIPBOARD_HIT_CLEAR;
        return result;
    }

    if (reach_clipboard_contains(layout->scrollbar_track, x, y))
    {
        result.type = REACH_CLIPBOARD_HIT_SCROLLBAR;
        return result;
    }
    if (!reach_clipboard_contains(layout->viewport, x, y))
    {
        return result;
    }

    const size_t item_count = reach_clipboard_count_clamped(model);
    for (size_t index = 0; index < item_count; ++index)
    {
        if (reach_clipboard_contains(layout->close_buttons[index], x, y))
        {
            result.type = REACH_CLIPBOARD_HIT_ITEM_CLOSE;
            result.index = index;
            return result;
        }
        if (reach_clipboard_contains(layout->items[index], x, y))
        {
            result.type = REACH_CLIPBOARD_HIT_ITEM;
            result.index = index;
            return result;
        }
    }

    return result;
}

struct reach_clipboard_feature
{
    reach_motion height;
    reach_motion item_width;
    int32_t content_follows;
    reach_motion_spec smooth_spec;
    reach_motion_spec fade_in_spec;
    reach_motion_spec fade_out_spec;
    float layout_scale;
    float layout_border;
    reach_keyed_row rows[REACH_CLIPBOARD_ROW_CAPACITY];
    reach_clipboard_item row_items[REACH_CLIPBOARD_ROW_CAPACITY];
    reach_motion row_hover[REACH_CLIPBOARD_ROW_CAPACITY];
    uint64_t synced_ids[REACH_CLIPBOARD_MAX_ITEMS];
    size_t synced_count;
    size_t synced_first;
    size_t synced_window;
    int32_t rows_synced;
    double motion_deadline;
    int32_t pointer_inside;
    int32_t pointer_x;
    int32_t pointer_y;
    reach_clipboard_item held_removed[REACH_CLIPBOARD_MAX_ITEMS * 2];
    size_t held_removed_count;
    reach_feature_transition surface_transition;
    reach_clipboard_state state;
    std::atomic<int32_t> refresh_requested;
    reach_clipboard_port port;
    int32_t started;
    void (*request_update)(void *user);
    void *request_update_user;
    reach_clipboard_retired_resource retired[REACH_CLIPBOARD_MAX_ITEMS * 2];
    size_t retired_count;
};

static void reach_clipboard_feature_retire(reach_clipboard_feature *clipboard,
                                           const reach_clipboard_item *item)
{
    if (clipboard == nullptr || item == nullptr || item->id == 0 ||
        clipboard->retired_count >= REACH_CLIPBOARD_MAX_ITEMS * 2)
    {
        return;
    }
    clipboard->retired[clipboard->retired_count++] = {item->id, item->thumbnail_id};
}

static int32_t reach_clipboard_feature_row_holds(const reach_clipboard_feature *clipboard,
                                                 uint64_t id)
{
    for (size_t slot = 0; slot < REACH_CLIPBOARD_ROW_CAPACITY; ++slot)
    {
        if (clipboard->rows[slot].lifecycle != REACH_KEYED_ROW_EMPTY &&
            clipboard->row_items[slot].id == id)
            return 1;
    }
    return 0;
}

static void reach_clipboard_feature_drop(reach_clipboard_feature *clipboard,
                                         const reach_clipboard_item *item)
{
    if (clipboard == nullptr || item == nullptr || item->id == 0)
        return;
    if (reach_clipboard_feature_row_holds(clipboard, item->id) &&
        clipboard->held_removed_count < REACH_CLIPBOARD_MAX_ITEMS * 2)
    {
        clipboard->held_removed[clipboard->held_removed_count++] = *item;
        return;
    }
    reach_clipboard_feature_retire(clipboard, item);
}

static void reach_clipboard_feature_release_slot(reach_clipboard_feature *clipboard, size_t slot)
{
    uint64_t id = clipboard->row_items[slot].id;
    for (size_t index = 0; id != 0 && index < clipboard->held_removed_count; ++index)
    {
        if (clipboard->held_removed[index].id != id)
            continue;
        reach_clipboard_feature_retire(clipboard, &clipboard->held_removed[index]);
        clipboard->held_removed[index] = clipboard->held_removed[--clipboard->held_removed_count];
        break;
    }
    clipboard->row_items[slot] = {};
    reach_motion_set(&clipboard->row_hover[slot], 0.0f);
}

static void reach_clipboard_feature_apply_row_changes(reach_clipboard_feature *clipboard,
                                                      const reach_keyed_row_change *changes,
                                                      size_t count)
{
    for (size_t index = 0; index < count; ++index)
    {
        if (changes[index].kind == REACH_KEYED_ROW_RELEASED)
            reach_clipboard_feature_release_slot(clipboard, changes[index].slot);
        else if (changes[index].kind == REACH_KEYED_ROW_ALLOCATED)
            reach_motion_set(&clipboard->row_hover[changes[index].slot], 0.0f);
    }
}

static void reach_clipboard_feature_release_rows(reach_clipboard_feature *clipboard)
{
    for (size_t slot = 0; slot < REACH_CLIPBOARD_ROW_CAPACITY; ++slot)
    {
        if (clipboard->rows[slot].lifecycle != REACH_KEYED_ROW_EMPTY)
            reach_clipboard_feature_release_slot(clipboard, slot);
        clipboard->rows[slot] = {};
    }
    for (size_t index = 0; index < clipboard->held_removed_count; ++index)
        reach_clipboard_feature_retire(clipboard, &clipboard->held_removed[index]);
    clipboard->held_removed_count = 0;
    clipboard->rows_synced = 0;
}

static void reach_clipboard_feature_capture(reach_clipboard_feature *clipboard,
                                            reach_feature_tick_result *out)
{
    if (clipboard == nullptr || out == nullptr ||
        !reach_clipboard_feature_take_refresh(clipboard) ||
        clipboard->port.ops.capture_current == nullptr)
    {
        return;
    }
    reach_clipboard_item item = {};
    if (clipboard->port.ops.capture_current(clipboard->port.provider, &item) != REACH_OK ||
        item.id == 0)
    {
        return;
    }
    reach_clipboard_insert_outcome outcome = {};
    reach_clipboard_insert_captured(clipboard, item, &outcome);
    reach_clipboard_feature_drop(clipboard, &outcome.release_rejected);
    reach_clipboard_feature_drop(clipboard, &outcome.release_evicted);
    if (outcome.accepted)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
}

const reach_clipboard_state *reach_clipboard_feature_state_ptr(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? &clipboard->state : nullptr;
}

reach_clipboard_state *reach_clipboard_feature_state_mut(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? &clipboard->state : nullptr;
}

void reach_clipboard_feature_reset(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        reach_pressable_reset(&clipboard->state.pressable, nullptr);
        clipboard->state.press_identity = 0;
        reach_clipboard_feature_release_rows(clipboard);
        reach_clipboard_model_init(&clipboard->state.model);
        reach_feature_transition_reset(&clipboard->surface_transition);
        clipboard->height = {};
        clipboard->item_width = {};
        clipboard->content_follows = 0;
        clipboard->motion_deadline = 0;
        clipboard->pointer_inside = 0;
    }
}

static size_t reach_clipboard_feature_slot_for(const reach_clipboard_feature *clipboard,
                                               size_t item_index)
{
    for (size_t slot = 0; slot < REACH_CLIPBOARD_ROW_CAPACITY; ++slot)
    {
        const reach_keyed_row &row = clipboard->rows[slot];
        if (row.lifecycle == REACH_KEYED_ROW_LIVE &&
            clipboard->synced_first + row.index == item_index)
            return slot;
    }
    return REACH_CLIPBOARD_ROW_CAPACITY;
}

reach_clipboard_layout
reach_clipboard_feature_input_layout(const reach_clipboard_feature *clipboard, double now_seconds)
{
    reach_clipboard_layout result = {};
    if (clipboard == nullptr)
        return result;
    result = clipboard->state.layout;
    const reach_clipboard_model &model = clipboard->state.model;
    const reach_clipboard_metrics metrics =
        reach_clipboard_metrics_for_scale(clipboard->layout_scale);
    float scroll = reach_scroll_view_value_at(&model.scroll, now_seconds);
    float width = clipboard->item_width.generation != 0
                      ? reach_motion_value_at(&clipboard->item_width, now_seconds)
                      : result.item_width;
    float shift =
        clipboard->content_follows && clipboard->height.generation != 0
            ? result.bounds.height - reach_motion_value_at(&clipboard->height, now_seconds)
            : 0.0f;
    result.title.y += shift;
    result.clear_button.y += shift;
    result.viewport.y += shift;
    result.scrollbar_track.y += shift;
    for (size_t index = 0; index < REACH_CLIPBOARD_MAX_ITEMS; ++index)
    {
        result.items[index] = {};
        result.close_buttons[index] = {};
    }
    size_t count = reach_clipboard_count_clamped(&model);
    for (size_t slot = 0; slot < REACH_CLIPBOARD_ROW_CAPACITY; ++slot)
    {
        const reach_keyed_row &row = clipboard->rows[slot];
        size_t index = clipboard->synced_first + row.index;
        if (row.lifecycle != REACH_KEYED_ROW_LIVE || index >= count)
            continue;
        result.items[index] = {result.viewport.x,
                               result.viewport.y + reach_motion_value_at(&row.y, now_seconds) -
                                   scroll,
                               width, result.item_large_size};
        result.close_buttons[index] = reach_clipboard_close_button_for(
            result.items[index], clipboard->layout_border, metrics);
    }
    return result;
}

static void reach_clipboard_feature_load_motion_specs(reach_clipboard_feature *clipboard,
                                                      const reach_theme *theme)
{
    clipboard->smooth_spec = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_SMOOTH);
    clipboard->fade_in_spec = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_FADE_IN);
    clipboard->fade_out_spec = reach_theme_motion_preset(theme, REACH_MOTION_PRESET_FADE_OUT);
}

static void reach_clipboard_feature_refresh_deadline(reach_clipboard_feature *clipboard)
{
    double deadline = 0;
    if (reach_keyed_rows_active(clipboard->rows, REACH_CLIPBOARD_ROW_CAPACITY))
        deadline = reach_keyed_rows_deadline(clipboard->rows, REACH_CLIPBOARD_ROW_CAPACITY);
    if (reach_scroll_view_active(&clipboard->state.model.scroll))
        deadline = fmax(deadline, reach_scroll_view_deadline(&clipboard->state.model.scroll));
    for (const reach_motion *motion : {&clipboard->height, &clipboard->item_width})
    {
        if (motion->generation != 0 && reach_motion_active(motion))
            deadline = fmax(deadline, reach_motion_deadline(motion));
    }
    clipboard->motion_deadline = deadline;
}

void reach_clipboard_feature_collapse_all_hover(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
        return;
    double now = reach_motion_now_seconds();
    for (reach_motion &hover : clipboard->row_hover)
    {
        if (hover.generation != 0 && hover.to != 0.0f)
            reach_motion_animate_spec_to_at(&hover, 0, clipboard->fade_out_spec, now);
    }
}

void reach_clipboard_feature_move_hover(reach_clipboard_feature *clipboard, size_t previous,
                                        size_t next)
{
    if (clipboard == nullptr)
        return;
    double now = reach_motion_now_seconds();
    size_t from = reach_clipboard_feature_slot_for(clipboard, previous);
    size_t to = reach_clipboard_feature_slot_for(clipboard, next);
    if (from < REACH_CLIPBOARD_ROW_CAPACITY && from != to)
        reach_motion_animate_spec_to_at(&clipboard->row_hover[from], 0, clipboard->fade_out_spec,
                                        now);
    if (to < REACH_CLIPBOARD_ROW_CAPACITY)
        reach_motion_animate_spec_to_at(&clipboard->row_hover[to], 1, clipboard->fade_in_spec, now);
}

void reach_clipboard_feature_track_pointer(reach_clipboard_feature *clipboard, int32_t inside,
                                           int32_t x, int32_t y)
{
    if (clipboard == nullptr)
        return;
    clipboard->pointer_inside = inside;
    clipboard->pointer_x = x;
    clipboard->pointer_y = y;
}

static int32_t reach_clipboard_feature_refresh_hover(reach_clipboard_feature *clipboard)
{
    reach_clipboard_state *state = &clipboard->state;
    if (!state->model.open || !clipboard->pointer_inside ||
        reach_pressable_tracking(&state->pressable) ||
        reach_scroll_view_dragging(&state->model.scroll))
        return 0;
    reach_clipboard_layout layout =
        reach_clipboard_feature_input_layout(clipboard, reach_motion_now_seconds());
    reach_clipboard_hit_result hit = reach_clipboard_hit_test(
        &state->model, &layout, clipboard->pointer_x, clipboard->pointer_y);
    size_t next =
        (hit.type == REACH_CLIPBOARD_HIT_ITEM || hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE)
            ? hit.index
            : REACH_CLIPBOARD_MAX_ITEMS;
    if (next == state->model.hovered_index)
        return 0;
    size_t previous = state->model.hovered_index;
    state->model.hovered_index = next;
    reach_clipboard_feature_move_hover(clipboard, previous, next);
    return 1;
}

struct reach_clipboard_row_match
{
    const reach_clipboard_feature *clipboard;
    size_t first;
};

static int32_t reach_clipboard_row_same(const void *context, size_t slot, size_t item_index)
{
    const reach_clipboard_row_match *match =
        static_cast<const reach_clipboard_row_match *>(context);
    uint64_t held = match->clipboard->row_items[slot].id;
    return held != 0 && held == match->clipboard->state.model.items[match->first + item_index].id;
}

static int32_t reach_clipboard_feature_sync_rows(reach_clipboard_feature *clipboard,
                                                 const reach_theme *theme,
                                                 int32_t constraints_changed, double now)
{
    reach_clipboard_state *state = &clipboard->state;
    const reach_clipboard_model &model = state->model;
    const reach_clipboard_layout &layout = state->layout;
    size_t count = reach_clipboard_count_clamped(&model);
    int32_t items_changed = count != clipboard->synced_count;
    for (size_t index = 0; !items_changed && index < count; ++index)
        items_changed = clipboard->synced_ids[index] != model.items[index].id;
    reach_scroll_view_window window = {};
    if (layout.item_pitch > 0.0f && count > 0)
    {
        size_t visible = (size_t)ceilf(layout.viewport.height / layout.item_pitch) + 1;
        window = reach_scroll_view_item_window(&model.scroll, layout.item_pitch, count, visible, 1,
                                               REACH_CLIPBOARD_ROW_CAPACITY);
    }
    if (clipboard->rows_synced && !items_changed && !constraints_changed &&
        window.first == clipboard->synced_first && window.count == clipboard->synced_window)
    {
        reach_clipboard_feature_refresh_deadline(clipboard);
        return 0;
    }
    int32_t animate = clipboard->rows_synced && items_changed && !constraints_changed &&
                      model.open &&
                      reach_feature_transition_visible(&clipboard->surface_transition);
    if (items_changed)
    {
        reach_clipboard_feature_collapse_all_hover(clipboard);
        state->model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    }
    reach_keyed_row_change changes[REACH_KEYED_ROWS_MAX_CHANGES(REACH_CLIPBOARD_ROW_CAPACITY)];
    size_t changed = reach_keyed_rows_release_settled(clipboard->rows, REACH_CLIPBOARD_ROW_CAPACITY,
                                                      changes, REACH_CLIPBOARD_ROW_CAPACITY);
    reach_clipboard_feature_apply_row_changes(clipboard, changes, changed);
    float targets[REACH_CLIPBOARD_ROW_CAPACITY] = {};
    for (size_t index = 0; index < window.count; ++index)
        targets[index] = (float)(window.first + index) * layout.item_pitch;
    reach_clipboard_row_match match = {clipboard, window.first};
    reach_keyed_rows_sync_input input = {};
    input.count = window.count;
    input.targets = targets;
    input.same = reach_clipboard_row_same;
    input.context = &match;
    input.animate = animate;
    input.theme = theme;
    input.now_seconds = now;
    changed = reach_keyed_rows_sync(clipboard->rows, REACH_CLIPBOARD_ROW_CAPACITY, &input, changes,
                                    REACH_KEYED_ROWS_MAX_CHANGES(REACH_CLIPBOARD_ROW_CAPACITY));
    reach_clipboard_feature_apply_row_changes(clipboard, changes, changed);
    for (size_t slot = 0; slot < REACH_CLIPBOARD_ROW_CAPACITY; ++slot)
    {
        const reach_keyed_row &row = clipboard->rows[slot];
        if (row.lifecycle == REACH_KEYED_ROW_LIVE && window.first + row.index < count)
            clipboard->row_items[slot] = model.items[window.first + row.index];
    }
    for (size_t index = 0; index < REACH_CLIPBOARD_MAX_ITEMS; ++index)
        clipboard->synced_ids[index] = index < count ? model.items[index].id : 0;
    clipboard->synced_count = count;
    clipboard->synced_first = window.first;
    clipboard->synced_window = window.count;
    clipboard->rows_synced = 1;
    reach_clipboard_feature_refresh_deadline(clipboard);
    return 1;
}

static int32_t reach_clipboard_rect_equal(reach_rect_f32 a, reach_rect_f32 b)
{
    return fabsf(a.x - b.x) < 0.5f && fabsf(a.y - b.y) < 0.5f && fabsf(a.width - b.width) < 0.5f &&
           fabsf(a.height - b.height) < 0.5f;
}

int32_t reach_clipboard_feature_relayout(reach_clipboard_feature *clipboard,
                                         const reach_theme *theme, reach_rect_f32 monitor_bounds,
                                         reach_rect_f32 launcher_bounds, float dpi_scale)
{
    if (clipboard == nullptr)
    {
        return 0;
    }

    const reach_theme *resolved_theme = theme != nullptr ? theme : reach_theme_default();
    reach_feature_transition_configure(&clipboard->surface_transition, resolved_theme, dpi_scale,
                                       REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_clipboard_feature_load_motion_specs(clipboard, resolved_theme);
    float border_thickness = reach_theme_border_thickness(resolved_theme, dpi_scale);
    reach_clipboard_state *state = &clipboard->state;
    reach_clipboard_layout previous_layout = state->layout;
    state->layout = reach_clipboard_compute_layout(&state->model, monitor_bounds, launcher_bounds,
                                                   dpi_scale, border_thickness);
    int32_t constraints_changed =
        fabsf(previous_layout.bounds.width - state->layout.bounds.width) >= .5f ||
        clipboard->layout_scale != dpi_scale || clipboard->layout_border != border_thickness;
    clipboard->layout_scale = dpi_scale;
    clipboard->layout_border = border_thickness;
    int32_t visible = reach_feature_transition_visible(&clipboard->surface_transition);
    int32_t animate = state->model.open && !constraints_changed && visible;
    double now = reach_motion_now_seconds();
    float target_height = state->layout.bounds.height;
    if (!visible || constraints_changed)
    {
        reach_motion_set(&clipboard->height, target_height);
        clipboard->content_follows = 0;
    }
    else if (clipboard->height.generation == 0)
    {
        reach_motion_set(&clipboard->height,
                         fminf(fmaxf(1.0f, state->layout.chrome_height), target_height));
        reach_motion_animate_spec_to_at(&clipboard->height, target_height, clipboard->smooth_spec,
                                        now);
        clipboard->content_follows = 0;
    }
    else if (animate && fabsf(clipboard->height.to - target_height) >= 0.5f)
    {
        clipboard->content_follows =
            clipboard->content_follows || !reach_motion_active_at(&clipboard->height, now);
        reach_motion_animate_spec_to_at(&clipboard->height, target_height, clipboard->smooth_spec,
                                        now);
    }
    float target_width = state->layout.item_width;
    if (!animate || clipboard->item_width.generation == 0)
        reach_motion_set(&clipboard->item_width, target_width);
    else if (fabsf(clipboard->item_width.to - target_width) >= 0.5f)
        reach_motion_animate_spec_to_at(&clipboard->item_width, target_width,
                                        clipboard->smooth_spec, now);
    int32_t rows_changed = reach_clipboard_feature_sync_rows(
        clipboard, resolved_theme, constraints_changed, reach_motion_now_seconds());
    return rows_changed || constraints_changed ||
           !reach_clipboard_rect_equal(previous_layout.bounds, state->layout.bounds) ||
           !reach_clipboard_rect_equal(previous_layout.viewport, state->layout.viewport);
}

static void reach_clipboard_capsule_tick(void *capsule, double delta_seconds,
                                         reach_feature_tick_result *out)
{
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(capsule);
    if (out != nullptr)
    {
        *out = {};
    }
    int32_t transition_changed =
        reach_feature_transition_tick(&clipboard->surface_transition, delta_seconds);
    if (transition_changed && !reach_feature_transition_visible(&clipboard->surface_transition))
    {
        clipboard->height = {};
        clipboard->content_follows = 0;
        reach_scroll_view_settle(&clipboard->state.model.scroll);
        clipboard->rows_synced = 0;
        reach_clipboard_feature_refresh_deadline(clipboard);
        if (out != nullptr)
            out->relayout = 1;
    }
    if (clipboard->motion_deadline > 0 && reach_motion_wait_ms(clipboard->motion_deadline) == 0)
    {
        reach_scroll_view_settle(&clipboard->state.model.scroll);
        for (reach_motion *motion : {&clipboard->height, &clipboard->item_width})
        {
            if (motion->generation != 0)
                reach_motion_set(motion, motion->to);
        }
        reach_keyed_row_change released[REACH_CLIPBOARD_ROW_CAPACITY];
        size_t count = reach_keyed_rows_release_settled(
            clipboard->rows, REACH_CLIPBOARD_ROW_CAPACITY, released, REACH_CLIPBOARD_ROW_CAPACITY);
        reach_clipboard_feature_apply_row_changes(clipboard, released, count);
        reach_clipboard_feature_refresh_deadline(clipboard);
        (void)reach_clipboard_feature_refresh_hover(clipboard);
        if (out != nullptr)
        {
            out->redraw = 1;
            out->relayout = 1;
            out->request_update = 1;
        }
    }
    reach_clipboard_feature_capture(clipboard, out);
    if (out != nullptr && transition_changed)
    {
        out->redraw = 1;
        out->request_update = out->request_update ||
                              reach_feature_transition_needs_update(&clipboard->surface_transition);
    }
}

static void reach_clipboard_capsule_reset(void *capsule)
{
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(capsule);
    reach_clipboard_feature_reset(clipboard);
    reach_clipboard_feature_clear_refresh(clipboard);
}

static int32_t reach_clipboard_capsule_is_open(const void *capsule)
{
    reach_clipboard_feature *clipboard = const_cast<reach_clipboard_feature *>(
        static_cast<const reach_clipboard_feature *>(capsule));
    return reach_clipboard_is_open(clipboard);
}

static void reach_clipboard_capsule_on_game_mode(void *capsule, int32_t enabled)
{
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(capsule);
    if (enabled && clipboard != nullptr)
    {
        (void)reach_clipboard_set_open(clipboard, 0);
        reach_feature_transition_reset(&clipboard->surface_transition);
        clipboard->height = {};
        clipboard->content_follows = 0;
        reach_scroll_view_settle(&clipboard->state.model.scroll);
        clipboard->rows_synced = 0;
        reach_clipboard_feature_refresh_deadline(clipboard);
    }
}

static int32_t reach_clipboard_capsule_needs_frame(const void *)
{
    return 0;
}

static int32_t reach_clipboard_capsule_presentation_visible(const void *capsule)
{
    const reach_clipboard_feature *clipboard =
        static_cast<const reach_clipboard_feature *>(capsule);
    return clipboard != nullptr && reach_feature_transition_visible(&clipboard->surface_transition);
}

static int32_t reach_clipboard_capsule_wants_pointer_move(const void *capsule)
{
    reach_clipboard_feature *clipboard = const_cast<reach_clipboard_feature *>(
        static_cast<const reach_clipboard_feature *>(capsule));
    return reach_clipboard_is_open(clipboard);
}

static void
reach_clipboard_capsule_apply_event_result(const reach_clipboard_event_result *event_result,
                                           reach_clipboard_feature *clipboard,
                                           reach_capsule_pointer_result *out)
{
    if (event_result == nullptr || out == nullptr)
    {
        return;
    }
    out->handled = out->handled || event_result->handled;
    out->redraw = out->redraw || event_result->redraw;
    out->relayout = out->relayout || event_result->relayout;
    out->capture = event_result->capture_pointer;
    out->sync_pointer_subscriptions = event_result->sync_pointer_subscriptions;
    if (event_result->action == REACH_CLIPBOARD_ACTION_CLEAR_ALL)
    {
        size_t count = reach_clipboard_item_count(clipboard);
        for (size_t index = 0; index < count; ++index)
        {
            reach_clipboard_feature_drop(clipboard, reach_clipboard_item_at(clipboard, index));
        }
        reach_clipboard_clear_all(clipboard);
        out->redraw = 1;
        out->relayout = 1;
    }
    else if (event_result->action == REACH_CLIPBOARD_ACTION_REMOVE_ITEM)
    {
        const reach_clipboard_item *item =
            reach_clipboard_item_at(clipboard, event_result->item_index);
        if (item != nullptr && item->id == event_result->item_id)
        {
            reach_clipboard_item removed = *item;
            if (reach_clipboard_remove_item(clipboard, event_result->item_index,
                                            event_result->item_id))
            {
                reach_clipboard_feature_drop(clipboard, &removed);
                out->redraw = 1;
                out->relayout = 1;
            }
        }
    }
    else if (event_result->action == REACH_CLIPBOARD_ACTION_RESTORE_ITEM)
    {
        if (clipboard->port.ops.restore != nullptr &&
            clipboard->port.ops.restore(clipboard->port.provider, event_result->item_id) ==
                REACH_OK)
        {
            reach_clipboard_confirm_restore(clipboard, event_result->item_index);
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            out->redraw = 1;
            out->relayout = 1;
        }
    }
}

static void reach_clipboard_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                   reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(capsule);
    if (clipboard == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }

    reach_clipboard_event_result event_result = {};
    switch (event->kind)
    {
    case REACH_POINTER_EVENT_DOWN:
        if (event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        reach_clipboard_cancel_press(clipboard, &event_result);
        reach_clipboard_pointer_down(clipboard, event->x, event->y, &event_result);
        break;
    case REACH_POINTER_EVENT_UP:
        if (event->button != REACH_POINTER_BUTTON_PRIMARY)
        {
            return;
        }
        if (reach_scroll_view_dragging(&clipboard->state.model.scroll))
        {
            reach_clipboard_scrollbar_release(clipboard, &event_result);
        }
        else
        {
            reach_clipboard_pointer_up(clipboard, event->x, event->y, &event_result);
        }
        break;
    case REACH_POINTER_EVENT_MOVE:
        if (reach_scroll_view_dragging(&clipboard->state.model.scroll))
        {
            reach_clipboard_scrollbar_drag_move(clipboard, event->y, &event_result);
        }
        else
        {
            reach_clipboard_pointer_move(clipboard, event->x, event->y, &event_result);
        }
        break;
    case REACH_POINTER_EVENT_WHEEL:
        reach_clipboard_wheel(clipboard, event->x, event->y, event->wheel_delta, &event_result);
        break;
    case REACH_POINTER_EVENT_LEAVE:
        reach_clipboard_pointer_move(clipboard, event->x, event->y, &event_result);
        event_result.redraw |= reach_clipboard_pointer_leave(clipboard);
        break;
    case REACH_POINTER_EVENT_CANCEL:
        reach_clipboard_scrollbar_release(clipboard, &event_result);
        reach_clipboard_cancel_press(clipboard, &event_result);
        break;
    default:
        return;
    }
    reach_clipboard_capsule_apply_event_result(&event_result, clipboard, out);
}

static void reach_clipboard_capsule_surface_geometry(const void *capsule,
                                                     reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_clipboard_feature *clipboard =
        static_cast<const reach_clipboard_feature *>(capsule);
    if (clipboard == nullptr)
    {
        return;
    }
    reach_clipboard_frame frame = reach_clipboard_panel_frame(
        &clipboard->state.layout, &clipboard->height, clipboard->content_follows);
    out->visible_bounds = frame.origin;
    out->envelope_bounds = frame.origin;
    out->has_input_bounds = 1;
    out->input_bounds = frame.panel;
    reach_feature_transition_presentation(&clipboard->surface_transition, out);
    if (clipboard->motion_deadline > 0 &&
        (out->deadline_seconds <= 0 || clipboard->motion_deadline < out->deadline_seconds))
        out->deadline_seconds = clipboard->motion_deadline;
}

const reach_ui_event_type *reach_clipboard_activation_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {REACH_UI_EVENT_CLIPBOARD_TOGGLE};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}

const reach_feature_capsule_ops *reach_clipboard_feature_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_clipboard_capsule_reset,
        reach_clipboard_capsule_tick,
        reach_clipboard_capsule_is_open,
        reach_clipboard_capsule_on_game_mode,
        reach_clipboard_capsule_needs_frame,
        reach_clipboard_capsule_wants_pointer_move,
        reach_clipboard_capsule_handle_pointer,
        [](const void *capsule) -> int32_t
        {
            const reach_clipboard_feature *clipboard =
                static_cast<const reach_clipboard_feature *>(capsule);
            return clipboard != nullptr &&
                   (reach_scroll_view_dragging(&clipboard->state.model.scroll) ||
                    reach_pressable_tracking(&clipboard->state.pressable));
        },
        nullptr,
        reach_clipboard_capsule_surface_geometry,
        nullptr,
        nullptr,
        nullptr,
        reach_clipboard_capsule_presentation_visible,
    };
    return &ops;
}

void reach_clipboard_feature_request_refresh(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        clipboard->refresh_requested.store(1);
    }
}

void reach_clipboard_feature_clear_refresh(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        clipboard->refresh_requested.store(0);
    }
}

int32_t reach_clipboard_feature_take_refresh(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? clipboard->refresh_requested.exchange(0) : 0;
}

reach_result reach_clipboard_feature_create(reach_clipboard_feature **out_clipboard)
{
    if (out_clipboard == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_clipboard_feature *clipboard = new (std::nothrow) reach_clipboard_feature();
    if (clipboard == nullptr)
    {
        return REACH_ERROR;
    }
    reach_clipboard_feature_load_motion_specs(clipboard, reach_theme_default());
    reach_feature_transition_init(&clipboard->surface_transition,
                                  REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_pressable_init(&clipboard->state.pressable);
    clipboard->refresh_requested.store(0);
    clipboard->port = {};
    clipboard->retired_count = 0;
    reach_clipboard_model_init(&clipboard->state.model);
    for (reach_motion &hover : clipboard->row_hover)
        reach_motion_set(&hover, 0.0f);
    *out_clipboard = clipboard;
    return REACH_OK;
}

void reach_clipboard_feature_destroy(reach_clipboard_feature *clipboard)
{
    delete clipboard;
}

static void reach_clipboard_feature_on_changed(void *user)
{
    reach_clipboard_feature *clipboard = static_cast<reach_clipboard_feature *>(user);
    reach_clipboard_feature_request_refresh(clipboard);
    if (clipboard != nullptr && clipboard->request_update != nullptr)
    {
        clipboard->request_update(clipboard->request_update_user);
    }
}

void reach_clipboard_feature_attach_port(reach_clipboard_feature *clipboard,
                                         const reach_clipboard_port *port,
                                         void (*request_update)(void *user), void *user)
{
    if (clipboard != nullptr)
    {
        clipboard->port = port != nullptr ? *port : reach_clipboard_port{};
        clipboard->request_update = request_update;
        clipboard->request_update_user = user;
    }
}

reach_result reach_clipboard_feature_start(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (clipboard->started)
    {
        return REACH_OK;
    }
    reach_clipboard_feature_request_refresh(clipboard);
    reach_result result =
        clipboard->port.ops.start != nullptr
            ? clipboard->port.ops.start(clipboard->port.provider,
                                        reach_clipboard_feature_on_changed, clipboard)
            : REACH_OK;
    clipboard->started = result == REACH_OK;
    return result;
}

void reach_clipboard_feature_stop(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr && clipboard->started)
    {
        if (clipboard->port.ops.stop != nullptr)
        {
            (void)clipboard->port.ops.stop(clipboard->port.provider);
        }
        clipboard->started = 0;
    }
}

size_t reach_clipboard_feature_take_retired_resources(reach_clipboard_feature *clipboard,
                                                      reach_clipboard_retired_resource *out,
                                                      size_t cap)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return 0;
    }
    size_t count = clipboard->retired_count < cap ? clipboard->retired_count : cap;
    for (size_t index = 0; index < count; ++index)
    {
        out[index] = clipboard->retired[index];
    }
    for (size_t index = count; index < clipboard->retired_count; ++index)
    {
        clipboard->retired[index - count] = clipboard->retired[index];
    }
    clipboard->retired_count -= count;
    return count;
}

void reach_clipboard_feature_release_resource(reach_clipboard_feature *clipboard,
                                              const reach_clipboard_retired_resource *resource)
{
    if (clipboard != nullptr && resource != nullptr && resource->item_id != 0 &&
        clipboard->port.ops.release != nullptr)
    {
        clipboard->port.ops.release(clipboard->port.provider, resource->item_id);
    }
}

reach_result reach_clipboard_append_render_commands(reach_clipboard_feature *clipboard,
                                                    const reach_theme *theme, float dpi_scale,
                                                    reach_render_command_buffer *out_commands)
{
    if (clipboard == nullptr || theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_clipboard_render_input input = {};
    input.theme = theme;
    input.model = &clipboard->state.model;
    input.layout = &clipboard->state.layout;
    input.rows = clipboard->rows;
    input.row_items = clipboard->row_items;
    input.row_hover = clipboard->row_hover;
    input.height = clipboard->height;
    input.item_width = clipboard->item_width;
    input.content_follows = clipboard->content_follows;
    input.dpi_scale = dpi_scale;
    input.text_alignment_leading = REACH_TEXT_ALIGNMENT_LEADING;

    return reach_clipboard_build_render_commands(&input, out_commands);
}

int32_t reach_clipboard_is_open(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr && reach_clipboard_feature_state_mut(clipboard)->model.open;
}

size_t reach_clipboard_item_count(reach_clipboard_feature *clipboard)
{
    return clipboard != nullptr ? reach_clipboard_feature_state_mut(clipboard)->model.count : 0;
}

const reach_clipboard_item *reach_clipboard_item_at(reach_clipboard_feature *clipboard,
                                                    size_t index)
{
    if (clipboard == nullptr || index >= reach_clipboard_feature_state_mut(clipboard)->model.count)
    {
        return nullptr;
    }
    return &reach_clipboard_feature_state_mut(clipboard)->model.items[index];
}

void reach_clipboard_reset_items(reach_clipboard_feature *clipboard)
{
    if (clipboard != nullptr)
    {
        reach_clipboard_model_init(&reach_clipboard_feature_state_mut(clipboard)->model);
    }
}

void reach_clipboard_clear_all(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    reach_clipboard_model_clear_items(&state->model);
}

int32_t reach_clipboard_set_open(reach_clipboard_feature *clipboard, int32_t open)
{
    if (clipboard == nullptr)
    {
        return 0;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    int32_t next = open ? 1 : 0;
    if (state->model.open == next)
    {
        return 0;
    }
    state->model.open = next;
    state->model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_pressable_reset(&state->pressable, nullptr);
    state->press_identity = 0;
    (void)reach_scroll_view_pointer_up(&state->model.scroll);
    reach_clipboard_feature_collapse_all_hover(clipboard);
    int32_t was_visible = reach_feature_transition_visible(&clipboard->surface_transition);
    (void)reach_feature_transition_set_open(&clipboard->surface_transition, next);
    double now = reach_motion_now_seconds();
    const reach_clipboard_layout &layout = state->layout;
    if (!next)
    {
        if (clipboard->height.generation != 0)
            reach_motion_set(&clipboard->height, reach_motion_value_at(&clipboard->height, now));
    }
    else if (layout.bounds.height > 0.0f)
    {
        if (clipboard->height.generation == 0 || !was_visible)
        {
            reach_motion_set(&clipboard->height,
                             fminf(fmaxf(1.0f, layout.chrome_height), layout.bounds.height));
            clipboard->content_follows = 0;
        }
        reach_motion_animate_spec_to_at(&clipboard->height, layout.bounds.height,
                                        clipboard->smooth_spec, now);
    }
    reach_clipboard_feature_refresh_deadline(clipboard);
    return 1;
}

void reach_clipboard_insert_captured(reach_clipboard_feature *clipboard, reach_clipboard_item item,
                                     reach_clipboard_insert_outcome *out)
{
    if (clipboard == nullptr || out == nullptr)
    {
        return;
    }
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);

    reach_clipboard_item evicted = {};
    if (state->model.count == REACH_CLIPBOARD_MAX_ITEMS)
    {
        evicted = state->model.items[REACH_CLIPBOARD_MAX_ITEMS - 1];
    }
    reach_clipboard_insert_result insertion = reach_clipboard_model_insert(&state->model, item);
    if (insertion.rejected_id != 0)
    {
        out->release_rejected = item;
    }
    if (insertion.evicted_id != 0)
    {
        out->release_evicted = evicted;
    }
    if (insertion.inserted || insertion.promoted_existing)
    {
        (void)reach_scroll_view_set_position(&state->model.scroll, 0.0f);
        (void)reach_scroll_view_sync(&state->model.scroll, nullptr, REACH_SCROLL_VIEW_SET,
                                     reach_motion_now_seconds());
        out->accepted = 1;
    }
}

int32_t reach_clipboard_pointer_leave(reach_clipboard_feature *clipboard)
{
    if (clipboard == nullptr)
    {
        return 0;
    }
    reach_clipboard_feature_track_pointer(clipboard, 0, 0, 0);
    reach_clipboard_state *state = reach_clipboard_feature_state_mut(clipboard);
    if (state->model.hovered_index >= REACH_CLIPBOARD_MAX_ITEMS)
    {
        return 0;
    }
    size_t previous = state->model.hovered_index;
    state->model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_clipboard_feature_move_hover(clipboard, previous, REACH_CLIPBOARD_MAX_ITEMS);
    return 1;
}
