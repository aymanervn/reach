#include "top_bar_tray.h"

#include "top_bar_common.h"

#include "reach/core/typography.h"
#include "reach/features/common/icon_feedback.h"

#include <math.h>
#include <new>

enum
{
    REACH_TOP_BAR_TRAY_ANIM_FEEDBACK = 0,
    REACH_TOP_BAR_TRAY_ANIM_COUNT
};

struct reach_top_bar_tray_popup
{
    reach_animation_manager animations;
    reach_animation_track tracks[REACH_TOP_BAR_TRAY_ANIM_COUNT];
    reach_pressable pressable;
    reach_rect_f32 item_slots[REACH_MAX_TRAY_ITEMS];
    size_t overflow_start;
    int32_t open;
    reach_rect_f32 pointer_bounds;
    int32_t pointer_bounds_valid;
    reach_popup_anchor anchor;
    reach_popup_placement placement;
};

typedef enum reach_top_bar_tray_hit_type
{
    REACH_TOP_BAR_TRAY_HIT_NONE = 0,
    REACH_TOP_BAR_TRAY_HIT_ITEM = 1,
    REACH_TOP_BAR_TRAY_HIT_POPUP = 2
} reach_top_bar_tray_hit_type;

typedef struct reach_top_bar_tray_hit
{
    reach_top_bar_tray_hit_type type;
    size_t index;
} reach_top_bar_tray_hit;

static reach_pressable_feedback_style reach_top_bar_tray_feedback(reach_top_bar *top_bar)
{
    reach_pressable_feedback_style feedback = {};
    feedback.animations = top_bar != nullptr ? &top_bar->tray_popup->animations : nullptr;
    feedback.track = REACH_TOP_BAR_TRAY_ANIM_FEEDBACK;
    feedback.pressed_value = 0.50f;
    feedback.press_seconds = 0.055;
    feedback.release_seconds = 0.055;
    feedback.press_easing = REACH_EASING_EASE_IN_OUT;
    feedback.release_easing = REACH_EASING_EASE_IN_OUT;
    return feedback;
}

static int32_t reach_top_bar_tray_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static size_t reach_top_bar_tray_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

static size_t reach_top_bar_find_provider_tray_item(const reach_top_bar *top_bar, uint32_t id)
{
    size_t count = reach_tray_service_item_count(top_bar != nullptr ? top_bar->tray : nullptr);
    for (size_t index = 0; index < count; ++index)
    {
        const reach_tray_item *item = reach_tray_service_item_at(top_bar->tray, index);
        if (item != nullptr && item->id == id)
        {
            return index;
        }
    }
    return REACH_MAX_TRAY_ITEMS;
}

static int32_t reach_top_bar_tray_order_contains(const uint32_t *order, size_t count, uint32_t id)
{
    for (size_t index = 0; index < count; ++index)
    {
        if (order[index] == id)
        {
            return 1;
        }
    }
    return 0;
}

void reach_top_bar_reset_tray_drag(reach_top_bar *top_bar)
{
    if (top_bar == nullptr)
    {
        return;
    }
    reach_draggable_init(&top_bar->tray_drag.gesture);
    top_bar->tray_drag.item_id = 0;
    reach_animation_manager_reset(&top_bar->manager, REACH_TOP_BAR_ANIM_TRAY_DRAG_SNAP);
}

void reach_top_bar_reconcile_tray_order(reach_top_bar *top_bar)
{
    if (top_bar == nullptr)
    {
        return;
    }

    uint32_t next[REACH_MAX_TRAY_ITEMS] = {};
    size_t next_count = 0;
    for (size_t index = 0; index < top_bar->tray_order_count; ++index)
    {
        uint32_t id = top_bar->tray_order[index];
        if (reach_top_bar_find_provider_tray_item(top_bar, id) < REACH_MAX_TRAY_ITEMS)
        {
            next[next_count++] = id;
        }
    }

    size_t provider_count = reach_tray_service_item_count(top_bar->tray);
    for (size_t index = 0; index < provider_count && next_count < REACH_MAX_TRAY_ITEMS; ++index)
    {
        const reach_tray_item *item = reach_tray_service_item_at(top_bar->tray, index);
        if (item != nullptr && !reach_top_bar_tray_order_contains(next, next_count, item->id))
        {
            next[next_count++] = item->id;
        }
    }

    for (size_t index = 0; index < next_count; ++index)
    {
        top_bar->tray_order[index] = next[index];
    }
    top_bar->tray_order_count = next_count;

    if (reach_draggable_tracking(&top_bar->tray_drag.gesture) &&
        !reach_top_bar_tray_order_contains(next, next_count, top_bar->tray_drag.item_id))
    {
        reach_top_bar_reset_tray_drag(top_bar);
    }
}

size_t reach_top_bar_ordered_tray_item_count(const reach_top_bar *top_bar)
{
    return top_bar != nullptr ? top_bar->tray_order_count : 0;
}

const reach_tray_item *reach_top_bar_ordered_tray_item_at(const reach_top_bar *top_bar,
                                                          size_t index)
{
    if (top_bar == nullptr || index >= top_bar->tray_order_count)
    {
        return nullptr;
    }
    size_t provider_index =
        reach_top_bar_find_provider_tray_item(top_bar, top_bar->tray_order[index]);
    return provider_index < REACH_MAX_TRAY_ITEMS
               ? reach_tray_service_item_at(top_bar->tray, provider_index)
               : nullptr;
}

void reach_top_bar_sync_tray_items(reach_top_bar *top_bar)
{
    if (top_bar == nullptr)
    {
        return;
    }
    reach_top_bar_state *state = &top_bar->state;
    reach_top_bar_reconcile_tray_order(top_bar);
    size_t count = reach_top_bar_ordered_tray_item_count(top_bar);
    state->tray_overflow = count >= REACH_TOP_BAR_TRAY_OVERFLOW_THRESHOLD;
    if (state->tray_overflow)
    {
        count = REACH_TOP_BAR_TRAY_INLINE_ICONS;
    }
    else if (count > REACH_TOP_BAR_MAX_TRAY_ICONS)
    {
        count = REACH_TOP_BAR_MAX_TRAY_ICONS;
    }
    state->tray_item_count = count;
    state->tray_popup_open = reach_top_bar_tray_popup_is_open(top_bar);
    for (size_t index = 0; index < count; ++index)
    {
        const reach_tray_item *item = reach_top_bar_ordered_tray_item_at(top_bar, index);
        state->tray_items[index] = {};
        if (item != nullptr)
        {
            state->tray_items[index].id = item->id;
            state->tray_items[index].icon_id = item->icon_id;
        }
    }
}

static const reach_tray_item *reach_top_bar_tray_item(const reach_top_bar *top_bar, size_t index)
{
    return reach_top_bar_ordered_tray_item_at(top_bar, index);
}

static reach_top_bar_tray_hit reach_top_bar_tray_hit_test(const reach_top_bar *top_bar, int32_t x,
                                                          int32_t y)
{
    reach_top_bar_tray_hit hit = {};
    hit.index = REACH_MAX_TRAY_ITEMS;
    if (top_bar == nullptr || top_bar->tray_popup == nullptr)
    {
        return hit;
    }

    const reach_top_bar_tray_popup *popup = top_bar->tray_popup;
    size_t count = reach_top_bar_ordered_tray_item_count(top_bar);
    for (size_t index = popup->overflow_start; index < count; ++index)
    {
        if (reach_top_bar_tray_contains(popup->item_slots[index], x, y))
        {
            hit.type = REACH_TOP_BAR_TRAY_HIT_ITEM;
            hit.index = index;
            return hit;
        }
    }
    if (reach_top_bar_tray_contains(popup->pointer_bounds, x, y))
    {
        hit.type = REACH_TOP_BAR_TRAY_HIT_POPUP;
    }
    return hit;
}

static uint64_t reach_top_bar_tray_target(const reach_top_bar *top_bar, reach_top_bar_tray_hit hit)
{
    const reach_tray_item *item = reach_top_bar_tray_item(top_bar, hit.index);
    return hit.type == REACH_TOP_BAR_TRAY_HIT_ITEM && item != nullptr ? (uint64_t)item->id
                                                                      : REACH_PRESSABLE_TARGET_NONE;
}

reach_result reach_top_bar_tray_popup_create(reach_top_bar_tray_popup **out_popup)
{
    if (out_popup == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_popup = new (std::nothrow) reach_top_bar_tray_popup();
    if (*out_popup == nullptr)
    {
        return REACH_ERROR;
    }
    reach_animation_manager_init(&(*out_popup)->animations, (*out_popup)->tracks,
                                 REACH_TOP_BAR_TRAY_ANIM_COUNT);
    reach_pressable_init(&(*out_popup)->pressable);
    return REACH_OK;
}

void reach_top_bar_tray_popup_destroy(reach_top_bar_tray_popup *popup)
{
    delete popup;
}

int32_t reach_top_bar_tray_popup_is_open(const reach_top_bar *top_bar)
{
    return top_bar != nullptr && top_bar->tray_popup != nullptr && top_bar->tray_popup->open;
}

int32_t reach_top_bar_set_tray_popup_open(reach_top_bar *top_bar, int32_t open)
{
    if (top_bar == nullptr || top_bar->tray_popup == nullptr)
    {
        return 0;
    }
    int32_t next = open ? 1 : 0;
    if (top_bar->tray_popup->open == next)
    {
        return 0;
    }
    top_bar->tray_popup->open = next;
    top_bar->state.tray_popup_open = next;
    if (!next)
    {
        reach_pressable_feedback_style feedback = reach_top_bar_tray_feedback(top_bar);
        reach_pressable_reset(&top_bar->tray_popup->pressable, &feedback);
    }
    else
    {
        (void)reach_top_bar_refresh_tray(top_bar);
    }
    return 1;
}

reach_result reach_top_bar_refresh_tray(reach_top_bar *top_bar)
{
    if (top_bar == nullptr)
    {
        return REACH_OK;
    }
    reach_result result = reach_tray_service_refresh(top_bar->tray);
    if (result == REACH_OK)
    {
        reach_top_bar_reconcile_tray_order(top_bar);
    }
    return result;
}

reach_result reach_top_bar_activate_tray_item(reach_top_bar *top_bar, uint32_t item_id,
                                              reach_tray_action action)
{
    return top_bar != nullptr ? reach_tray_service_activate(top_bar->tray, item_id, action)
                              : REACH_OK;
}

int32_t reach_top_bar_take_retired_tray_icon(reach_top_bar *top_bar, uint64_t *out_icon_id)
{
    return top_bar != nullptr ? reach_tray_service_take_retired_icon(top_bar->tray, out_icon_id)
                              : 0;
}

void reach_top_bar_release_retired_tray_icon(reach_top_bar *top_bar, uint64_t icon_id)
{
    if (top_bar != nullptr)
    {
        reach_tray_service_release_retired_icon(top_bar->tray, icon_id);
    }
}

size_t reach_top_bar_tray_item_count(const reach_top_bar *top_bar)
{
    return reach_top_bar_ordered_tray_item_count(top_bar);
}

size_t reach_top_bar_tray_overflow_start(const reach_top_bar *top_bar)
{
    return top_bar != nullptr && top_bar->state.tray_overflow ? top_bar->state.tray_item_count : 0;
}

uint64_t reach_top_bar_tray_item_icon_id(const reach_top_bar *top_bar, size_t index)
{
    const reach_tray_item *item = reach_top_bar_tray_item(top_bar, index);
    return item != nullptr ? item->icon_id : 0;
}

void reach_top_bar_layout_tray_popup(reach_top_bar *top_bar, const reach_theme *theme,
                                     const reach_popup_anchor *anchor, float dpi_scale,
                                     reach_rect_f32 *out_bounds)
{
    if (top_bar == nullptr || top_bar->tray_popup == nullptr || theme == nullptr ||
        anchor == nullptr)
    {
        return;
    }

    reach_top_bar_tray_popup *popup = top_bar->tray_popup;
    popup->anchor = *anchor;
    float slot_size = reach_theme_tray_slot_size(theme, anchor->bar_height);
    float gap = slot_size * 0.22f;
    float padding = slot_size * 0.3f;
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float border_thickness = reach_theme_border_thickness(theme, scale);
    float notch_height = reach_popup_notch_height_scaled(scale);
    size_t item_count = reach_top_bar_ordered_tray_item_count(top_bar);
    size_t overflow_count =
        item_count > popup->overflow_start ? item_count - popup->overflow_start : 0;
    size_t visual_count = overflow_count > 0 ? overflow_count : 1;
    size_t columns = reach_top_bar_tray_min_size(visual_count, 5);
    size_t rows = (visual_count + 4) / 5;
    float content_width = padding * 2.0f + (float)columns * slot_size + (float)(columns - 1) * gap;
    float content_height = padding * 2.0f + (float)rows * slot_size + (float)(rows - 1) * gap;
    popup->placement = reach_popup_place(
        anchor, ceilf(content_width + border_thickness * 2.0f),
        ceilf(content_height + notch_height + border_thickness * 2.0f), 8.0f * scale);

    reach_rect_f32 bounds = popup->placement.bounds;
    reach_rect_f32 surface_bounds = {0.0f, 0.0f, bounds.width, bounds.height};
    reach_rect_f32 content_bounds = reach_theme_border_content_rect(theme, scale, surface_bounds);
    float grid_height = (float)rows * slot_size + (float)(rows - 1) * gap;
    float content_top =
        content_bounds.y + (anchor->direction == REACH_POPUP_DROP_DOWN ? notch_height : 0.0f);
    float grid_y = content_top + (content_height - grid_height) * 0.5f;
    for (size_t index = 0; index < item_count; ++index)
    {
        if (index < popup->overflow_start)
        {
            popup->item_slots[index] = {};
            continue;
        }
        size_t shown = index - popup->overflow_start;
        size_t row = shown / 5;
        size_t column = shown % 5;
        size_t row_start = row * 5;
        size_t row_remaining = overflow_count - row_start;
        size_t row_columns = reach_top_bar_tray_min_size(row_remaining, 5);
        float row_width = (float)row_columns * slot_size + (float)(row_columns - 1) * gap;
        float row_x = content_bounds.x + (content_bounds.width - row_width) * 0.5f;
        popup->item_slots[index] = {row_x + (float)column * (slot_size + gap),
                                    grid_y + (float)row * (slot_size + gap), slot_size, slot_size};
    }
    popup->pointer_bounds = surface_bounds;
    popup->pointer_bounds_valid = 1;
    if (out_bounds != nullptr)
    {
        *out_bounds = bounds;
    }
}

static void reach_top_bar_tray_reset(void *capsule)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr || top_bar->tray_popup == nullptr)
    {
        return;
    }
    reach_pressable_feedback_style feedback = reach_top_bar_tray_feedback(top_bar);
    reach_pressable_reset(&top_bar->tray_popup->pressable, &feedback);
    top_bar->tray_popup->open = 0;
    top_bar->tray_popup->pointer_bounds_valid = 0;
    top_bar->state.tray_popup_open = 0;
}

static void reach_top_bar_tray_tick(void *capsule, double delta_seconds,
                                    reach_feature_tick_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr || top_bar->tray_popup == nullptr)
    {
        return;
    }
    reach_top_bar_tray_popup *popup = top_bar->tray_popup;
    int32_t was_active =
        reach_animation_manager_active(&popup->animations, REACH_TOP_BAR_TRAY_ANIM_FEEDBACK);
    reach_animation_manager_tick(&popup->animations, delta_seconds);
    int32_t active =
        reach_animation_manager_active(&popup->animations, REACH_TOP_BAR_TRAY_ANIM_FEEDBACK);
    if ((was_active || active) && out != nullptr)
    {
        out->redraw = 1;
    }
    reach_pressable_feedback_style feedback = reach_top_bar_tray_feedback(top_bar);
    reach_pressable_settle_feedback(&popup->pressable, &feedback);
}

static int32_t reach_top_bar_tray_is_open(const void *capsule)
{
    return reach_top_bar_tray_popup_is_open(static_cast<const reach_top_bar *>(capsule));
}

static int32_t reach_top_bar_tray_needs_frame(const void *capsule)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    return top_bar != nullptr && top_bar->tray_popup != nullptr &&
           reach_animation_manager_any_active(&top_bar->tray_popup->animations);
}

static int32_t reach_top_bar_tray_wants_pointer_move(const void *capsule)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    return top_bar != nullptr && top_bar->tray_popup != nullptr &&
           reach_pressable_tracking(&top_bar->tray_popup->pressable);
}

static void reach_top_bar_tray_surface_geometry(const void *capsule,
                                                reach_feature_surface_geometry *out)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    if (top_bar == nullptr || top_bar->tray_popup == nullptr || out == nullptr)
    {
        return;
    }
    out->visible_bounds = top_bar->tray_popup->placement.bounds;
    out->envelope_bounds = top_bar->tray_popup->placement.bounds;
    out->notch_anchor_x = top_bar->tray_popup->placement.notch_anchor_x;
    out->notch_side = top_bar->tray_popup->placement.notch_side;
}

static void reach_top_bar_tray_apply_pressable(const reach_pressable_result *result,
                                               reach_capsule_pointer_result *out)
{
    out->redraw = result->redraw;
    out->capture = result->capture;
    out->sync_pointer_subscriptions = result->sync_pointer_subscriptions;
}

static void reach_top_bar_tray_handle_pointer(void *capsule, const reach_pointer_event *event,
                                              reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr || top_bar->tray_popup == nullptr || event == nullptr ||
        out == nullptr || event->coordinate_space != REACH_POINTER_COORDINATE_SURFACE_LOCAL)
    {
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_DOWN && event->button == REACH_POINTER_BUTTON_PRIMARY &&
        event->owner_trigger)
    {
        out->handled = 1;
        out->continue_source_sequence = 1;
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_DOWN &&
        event->surface_relation == REACH_POINTER_SURFACE_OUTSIDE)
    {
        out->handled = 1;
        out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        out->cancel_source_sequence = event->button == REACH_POINTER_BUTTON_PRIMARY;
        out->continue_source_sequence = event->button != REACH_POINTER_BUTTON_PRIMARY;
        return;
    }
    if ((event->kind == REACH_POINTER_EVENT_DOWN || event->kind == REACH_POINTER_EVENT_UP) &&
        event->button == REACH_POINTER_BUTTON_MIDDLE)
    {
        return;
    }
    reach_top_bar_tray_popup *popup = top_bar->tray_popup;
    reach_top_bar_tray_hit hit = {};
    hit.index = REACH_MAX_TRAY_ITEMS;
    if (popup->open && popup->pointer_bounds_valid)
    {
        hit = reach_top_bar_tray_hit_test(top_bar, event->x, event->y);
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN)
    {
        if (hit.type == REACH_TOP_BAR_TRAY_HIT_ITEM)
        {
            reach_pressable_feedback_style feedback = reach_top_bar_tray_feedback(top_bar);
            reach_pressable_result result = {};
            reach_pressable_press(&popup->pressable, event->button,
                                  reach_top_bar_tray_target(top_bar, hit), hit.index, &feedback,
                                  &result);
            reach_top_bar_tray_apply_pressable(&result, out);
            out->handled = 1;
        }
        else if (hit.type == REACH_TOP_BAR_TRAY_HIT_POPUP)
        {
            out->handled = 1;
        }
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_UP)
    {
        reach_pressable_feedback_style feedback = reach_top_bar_tray_feedback(top_bar);
        reach_pressable_result result = {};
        int32_t was_tracking = reach_pressable_tracking(&popup->pressable);
        reach_pressable_release(&popup->pressable, event->button,
                                reach_top_bar_tray_target(top_bar, hit), &feedback, &result);
        reach_top_bar_tray_apply_pressable(&result, out);
        if (result.activated)
        {
            const reach_tray_item *item = reach_top_bar_tray_item(top_bar, hit.index);
            if (item != nullptr)
            {
                reach_tray_action action = event->button == REACH_POINTER_BUTTON_SECONDARY
                                               ? REACH_TRAY_ACTION_RIGHT_CLICK
                                               : REACH_TRAY_ACTION_LEFT_CLICK;
                (void)reach_top_bar_activate_tray_item(top_bar, item->id, action);
                out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            }
            out->handled = 1;
        }
        else if (hit.type == REACH_TOP_BAR_TRAY_HIT_POPUP || was_tracking)
        {
            out->handled = 1;
        }
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_MOVE || event->kind == REACH_POINTER_EVENT_LEAVE)
    {
        int32_t was_tracking = reach_pressable_tracking(&popup->pressable);
        reach_pressable_result result = {};
        uint64_t target = event->kind == REACH_POINTER_EVENT_MOVE
                              ? reach_top_bar_tray_target(top_bar, hit)
                              : REACH_PRESSABLE_TARGET_NONE;
        reach_pressable_update(&popup->pressable, target, &result);
        reach_top_bar_tray_apply_pressable(&result, out);
        out->handled = was_tracking;
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_CANCEL)
    {
        int32_t was_tracking = reach_pressable_tracking(&popup->pressable);
        reach_pressable_feedback_style feedback = reach_top_bar_tray_feedback(top_bar);
        reach_pressable_result result = {};
        reach_pressable_cancel(&popup->pressable, &feedback, &result);
        reach_top_bar_tray_apply_pressable(&result, out);
        out->handled = was_tracking;
    }
}

const reach_feature_capsule_ops *reach_top_bar_tray_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {reach_top_bar_tray_reset,
                                                  reach_top_bar_tray_tick,
                                                  reach_top_bar_tray_is_open,
                                                  nullptr,
                                                  reach_top_bar_tray_needs_frame,
                                                  reach_top_bar_tray_wants_pointer_move,
                                                  reach_top_bar_tray_handle_pointer,
                                                  reach_top_bar_tray_wants_pointer_move,
                                                  nullptr,
                                                  reach_top_bar_tray_surface_geometry};
    return &ops;
}

reach_animation_manager *reach_top_bar_tray_animation_manager(reach_top_bar *top_bar)
{
    return top_bar != nullptr && top_bar->tray_popup != nullptr ? &top_bar->tray_popup->animations
                                                                : nullptr;
}

reach_result reach_top_bar_append_tray_render_commands(reach_top_bar *top_bar,
                                                       const reach_top_bar_tray_render_context *ctx,
                                                       reach_render_command_buffer *out_commands)
{
    if (top_bar == nullptr || top_bar->tray_popup == nullptr || ctx == nullptr ||
        ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_top_bar_tray_popup *popup_state = top_bar->tray_popup;
    reach_render_command_buffer_clear(out_commands);
    reach_popup_background_input popup = {};
    popup.theme = ctx->theme;
    popup.bounds = ctx->bounds;
    popup.notch_center_x = popup_state->placement.notch_anchor_x - ctx->bounds.x;
    popup.notch_side = popup_state->placement.notch_side;
    popup.dpi_scale = ctx->dpi_scale;
    popup.background_color_override = ctx->theme->bar_tray_background;
    popup.has_background_color_override = 1;
    reach_result result = reach_popup_push_background(&popup, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }

    float icon_box_radius = reach_theme_icon_box_corner_radius(
        ctx->theme, reach_theme_tray_slot_size(ctx->theme, popup_state->anchor.bar_height));
    reach_pressable_feedback_style feedback = reach_top_bar_tray_feedback(top_bar);
    size_t feedback_index = reach_pressable_feedback_index(&popup_state->pressable);
    float feedback_opacity = reach_pressable_feedback_value(&popup_state->pressable, &feedback);
    size_t count = reach_top_bar_ordered_tray_item_count(top_bar);
    for (size_t index = popup_state->overflow_start; index < count; ++index)
    {
        const reach_tray_item *item = reach_top_bar_tray_item(top_bar, index);
        reach_rect_f32 slot = popup_state->item_slots[index];
        float icon_size = floorf(slot.height * 0.86f);
        float minimum = 16.0f * (ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f);
        if (icon_size < minimum && slot.height >= minimum)
        {
            icon_size = minimum;
        }
        reach_rect_f32 icon_rect = {slot.x + (slot.width - icon_size) * 0.5f,
                                    slot.y + (slot.height - icon_size) * 0.5f, icon_size,
                                    icon_size};
        reach_render_command command = {};
        if (item != nullptr && item->icon_id != 0)
        {
            command.type = REACH_RENDER_COMMAND_ICON;
            command.rect = icon_rect;
            command.icon_id = item->icon_id;
            command.color.a = 1.0f;
        }
        else
        {
            command.type = REACH_RENDER_COMMAND_TEXT;
            command.rect = slot;
            command.color = ctx->theme->fallback_icon_text;
            command.text_size = REACH_TEXT_SIZE_HEADING * ctx->dpi_scale;
            command.text_alignment = REACH_TEXT_ALIGNMENT_CENTER;
            command.text[0] = item != nullptr && item->title[0] != 0 ? item->title[0] : '?';
        }
        reach_render_command_buffer_push(out_commands, &command);

        if (feedback_index == index && feedback_opacity > 0.001f)
        {
            reach_push_icon_press_feedback(
                out_commands, item != nullptr && item->icon_id != 0 ? icon_rect : slot,
                icon_box_radius, item != nullptr ? item->icon_id : 0,
                ctx->theme->bar_tray_background, feedback_opacity, 0.001f);
        }
    }
    return REACH_OK;
}

void reach_top_bar_tray_set_overflow_start(reach_top_bar *top_bar, size_t overflow_start)
{
    if (top_bar != nullptr && top_bar->tray_popup != nullptr)
    {
        top_bar->tray_popup->overflow_start = overflow_start;
    }
}
