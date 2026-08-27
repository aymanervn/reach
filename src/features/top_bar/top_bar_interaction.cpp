#include "reach/features/top_bar.h"

#include "top_bar_common.h"
#include "top_bar_now_playing.h"

static_assert(REACH_TOP_BAR_POINTER_ACTION_PRESS_POWER >= REACH_FEATURE_ACTION_PRIVATE_BASE &&
                  REACH_TOP_BAR_POINTER_ACTION_PRESS_QUICK_SETTINGS >=
                      REACH_FEATURE_ACTION_PRIVATE_BASE,
              "top bar pointer policy kinds must not collide with the shared action vocabulary");

static int32_t reach_top_bar_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_top_bar_pointer_region reach_top_bar_hit_test(const reach_top_bar_layout *layout,
                                                    int32_t local_x, int32_t local_y)
{
    if (layout == nullptr)
    {
        return REACH_TOP_BAR_POINTER_REGION_NONE;
    }
    if (reach_top_bar_rect_contains(layout->power_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON;
    }
    if (reach_top_bar_tray_icon_at(layout, local_x, local_y) < layout->tray_icon_count)
    {
        return REACH_TOP_BAR_POINTER_REGION_TRAY_ICON;
    }
    if (reach_top_bar_rect_contains(layout->tray_overflow_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW;
    }
    if (reach_top_bar_rect_contains(layout->quick_settings_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON;
    }
    if (reach_top_bar_rect_contains(layout->settings_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_SETTINGS_BUTTON;
    }
    if (reach_top_bar_rect_contains(layout->language_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON;
    }
    if (reach_top_bar_rect_contains(layout->battery_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_BATTERY_BUTTON;
    }
    return REACH_TOP_BAR_POINTER_REGION_NONE;
}

size_t reach_top_bar_tray_icon_at(const reach_top_bar_layout *layout, int32_t local_x,
                                  int32_t local_y)
{
    if (layout == nullptr)
    {
        return REACH_TOP_BAR_MAX_TRAY_ICONS;
    }
    for (size_t index = 0; index < layout->tray_icon_count; ++index)
    {
        if (reach_top_bar_rect_contains(layout->tray_icons[index], local_x, local_y))
        {
            return index;
        }
    }
    return REACH_TOP_BAR_MAX_TRAY_ICONS;
}

reach_pressable_feedback_style reach_top_bar_pressable_feedback(reach_top_bar *top_bar)
{
    reach_pressable_feedback_style feedback = {};
    if (top_bar != nullptr)
    {
        feedback.animations = reach_top_bar_manager(top_bar);
        feedback.track = REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY;
        feedback.pressed_value = 0.50f;
        feedback.press_seconds = 0.055;
        feedback.release_seconds = 0.055;
        feedback.press_easing = REACH_EASING_EASE_IN_OUT;
        feedback.release_easing = REACH_EASING_EASE_IN_OUT;
    }
    return feedback;
}

static uint64_t reach_top_bar_pressable_target(reach_top_bar_pointer_region region, uint32_t detail)
{
    return region == REACH_TOP_BAR_POINTER_REGION_NONE
               ? REACH_PRESSABLE_TARGET_NONE
               : (static_cast<uint64_t>(region) << 32) | detail;
}

static reach_top_bar_pointer_region reach_top_bar_pressable_region(uint64_t target)
{
    return target == REACH_PRESSABLE_TARGET_NONE
               ? REACH_TOP_BAR_POINTER_REGION_NONE
               : static_cast<reach_top_bar_pointer_region>(target >> 32);
}

static uint32_t reach_top_bar_pressable_detail(uint64_t target)
{
    return static_cast<uint32_t>(target);
}

static size_t reach_top_bar_inline_tray_index(const reach_top_bar *top_bar, uint32_t id)
{
    if (top_bar == nullptr)
    {
        return REACH_TOP_BAR_MAX_TRAY_ICONS;
    }
    for (size_t index = 0; index < top_bar->state.tray_item_count; ++index)
    {
        if (top_bar->state.tray_items[index].id == id)
        {
            return index;
        }
    }
    return REACH_TOP_BAR_MAX_TRAY_ICONS;
}

static size_t reach_top_bar_tray_order_index(const reach_top_bar *top_bar, uint32_t id)
{
    if (top_bar == nullptr)
    {
        return REACH_MAX_TRAY_ITEMS;
    }
    for (size_t index = 0; index < top_bar->tray_order_count; ++index)
    {
        if (top_bar->tray_order[index] == id)
        {
            return index;
        }
    }
    return REACH_MAX_TRAY_ITEMS;
}

float reach_top_bar_tray_item_current_x(const reach_top_bar *top_bar, size_t index)
{
    if (top_bar == nullptr || index >= top_bar->state.layout.tray_icon_count)
    {
        return 0.0f;
    }
    size_t track = reach_top_bar_tray_item_x_animation_id(index);
    return reach_animation_manager_active(&top_bar->manager, track)
               ? reach_animation_manager_value(&top_bar->manager, track)
               : top_bar->state.layout.tray_icons[index].x;
}

size_t reach_top_bar_tray_dragged_index(const reach_top_bar *top_bar)
{
    return top_bar != nullptr && (reach_draggable_tracking(&top_bar->tray_drag.gesture) ||
                                  reach_animation_manager_active(&top_bar->manager,
                                                                 REACH_TOP_BAR_ANIM_TRAY_DRAG_SNAP))
               ? reach_top_bar_inline_tray_index(top_bar, top_bar->tray_drag.item_id)
               : REACH_TOP_BAR_MAX_TRAY_ICONS;
}

typedef struct reach_top_bar_tray_x_snapshot
{
    uint32_t ids[REACH_TOP_BAR_MAX_TRAY_ICONS];
    float x[REACH_TOP_BAR_MAX_TRAY_ICONS];
    size_t count;
} reach_top_bar_tray_x_snapshot;

static reach_top_bar_tray_x_snapshot reach_top_bar_tray_x_snapshot_take(reach_top_bar *top_bar)
{
    reach_top_bar_tray_x_snapshot snapshot = {};
    if (top_bar == nullptr)
    {
        return snapshot;
    }
    snapshot.count = top_bar->state.tray_item_count;
    for (size_t index = 0; index < snapshot.count; ++index)
    {
        snapshot.ids[index] = top_bar->state.tray_items[index].id;
        snapshot.x[index] = reach_top_bar_tray_item_current_x(top_bar, index);
    }
    return snapshot;
}

static float reach_top_bar_tray_x_snapshot_find(const reach_top_bar_tray_x_snapshot *snapshot,
                                                uint32_t id, float fallback)
{
    if (snapshot == nullptr)
    {
        return fallback;
    }
    for (size_t index = 0; index < snapshot->count; ++index)
    {
        if (snapshot->ids[index] == id)
        {
            return snapshot->x[index];
        }
    }
    return fallback;
}

static void reach_top_bar_move_tray_item(reach_top_bar *top_bar, size_t source, size_t target)
{
    if (top_bar == nullptr || source >= top_bar->state.tray_item_count ||
        target >= top_bar->state.tray_item_count || source == target)
    {
        return;
    }

    reach_top_bar_tray_x_snapshot snapshot = reach_top_bar_tray_x_snapshot_take(top_bar);
    reach_top_bar_tray_item moved = top_bar->state.tray_items[source];
    if (source < target)
    {
        for (size_t index = source; index < target; ++index)
        {
            top_bar->state.tray_items[index] = top_bar->state.tray_items[index + 1];
        }
    }
    else
    {
        for (size_t index = source; index > target; --index)
        {
            top_bar->state.tray_items[index] = top_bar->state.tray_items[index - 1];
        }
    }
    top_bar->state.tray_items[target] = moved;

    size_t order_source = reach_top_bar_tray_order_index(top_bar, moved.id);
    if (order_source < top_bar->tray_order_count)
    {
        uint32_t ordered_id = top_bar->tray_order[order_source];
        if (order_source < target)
        {
            for (size_t index = order_source; index < target; ++index)
            {
                top_bar->tray_order[index] = top_bar->tray_order[index + 1];
            }
        }
        else
        {
            for (size_t index = order_source; index > target; --index)
            {
                top_bar->tray_order[index] = top_bar->tray_order[index - 1];
            }
        }
        top_bar->tray_order[target] = ordered_id;
    }

    for (size_t index = 0; index < top_bar->state.tray_item_count; ++index)
    {
        size_t track = reach_top_bar_tray_item_x_animation_id(index);
        float target_x = top_bar->state.layout.tray_icons[index].x;
        if (top_bar->state.tray_items[index].id == top_bar->tray_drag.item_id)
        {
            reach_animation_manager_reset(&top_bar->manager, track);
            continue;
        }
        float start_x = reach_top_bar_tray_x_snapshot_find(
            &snapshot, top_bar->state.tray_items[index].id, target_x);
        reach_animation_manager_start(&top_bar->manager, track, start_x, target_x, 0.12,
                                      REACH_EASING_EASE_IN_OUT);
    }
}

static void reach_top_bar_tray_drag_begin(reach_top_bar *top_bar, size_t index, int32_t x,
                                          int32_t y, uint64_t target)
{
    if (top_bar == nullptr || index >= top_bar->state.tray_item_count ||
        reach_draggable_tracking(&top_bar->tray_drag.gesture))
    {
        return;
    }
    reach_draggable_begin(&top_bar->tray_drag.gesture, target, x, y);
    top_bar->tray_drag.item_id = top_bar->state.tray_items[index].id;
    top_bar->tray_drag.grab_offset_x = (float)x - top_bar->state.layout.tray_icons[index].x;
    top_bar->tray_drag.x = top_bar->state.layout.tray_icons[index].x;
    reach_animation_manager_reset(&top_bar->manager, REACH_TOP_BAR_ANIM_TRAY_DRAG_SNAP);
}

static int32_t reach_top_bar_tray_drag_end(reach_top_bar *top_bar)
{
    if (top_bar == nullptr || !reach_draggable_tracking(&top_bar->tray_drag.gesture))
    {
        return 0;
    }
    int32_t moved = reach_draggable_moved(&top_bar->tray_drag.gesture);
    reach_draggable_end(&top_bar->tray_drag.gesture, nullptr);
    size_t index = reach_top_bar_inline_tray_index(top_bar, top_bar->tray_drag.item_id);
    if (moved && index < top_bar->state.layout.tray_icon_count)
    {
        reach_animation_manager_start(
            &top_bar->manager, REACH_TOP_BAR_ANIM_TRAY_DRAG_SNAP, top_bar->tray_drag.x,
            top_bar->state.layout.tray_icons[index].x, 0.12, REACH_EASING_EASE_IN_OUT);
    }
    else
    {
        top_bar->tray_drag.item_id = 0;
    }
    return moved;
}

static uint64_t reach_top_bar_pressable_target_at(reach_top_bar *top_bar, int32_t local_x,
                                                  int32_t local_y, reach_pointer_button button)
{
    if (button == REACH_POINTER_BUTTON_PRIMARY)
    {
        reach_now_playing_action media = reach_top_bar_now_playing_action_at(
            reach_top_bar_now_playing_subfeature(top_bar), local_x, local_y);
        if (media != REACH_NOW_PLAYING_ACTION_NONE)
        {
            return reach_top_bar_pressable_target(REACH_TOP_BAR_POINTER_REGION_NOW_PLAYING,
                                                  static_cast<uint32_t>(media));
        }
    }
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    reach_top_bar_pointer_region region =
        reach_top_bar_hit_test(state != nullptr ? &state->layout : nullptr, local_x, local_y);
    if (button == REACH_POINTER_BUTTON_SECONDARY &&
        region != REACH_TOP_BAR_POINTER_REGION_TRAY_ICON)
    {
        return REACH_PRESSABLE_TARGET_NONE;
    }
    uint32_t detail = 0;
    if (region == REACH_TOP_BAR_POINTER_REGION_TRAY_ICON && state != nullptr)
    {
        size_t index = reach_top_bar_tray_icon_at(&state->layout, local_x, local_y);
        if (index >= state->tray_item_count)
        {
            return REACH_PRESSABLE_TARGET_NONE;
        }
        detail = state->tray_items[index].id;
    }
    return reach_top_bar_pressable_target(region, detail);
}

static size_t reach_top_bar_pressable_feedback_index(reach_top_bar *top_bar, uint64_t target)
{
    switch (reach_top_bar_pressable_region(target))
    {
    case REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON:
        return REACH_TOP_BAR_FEEDBACK_POWER_BUTTON;
    case REACH_TOP_BAR_POINTER_REGION_TRAY_ICON:
    {
        reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
        if (state == nullptr)
        {
            return REACH_PRESSABLE_FEEDBACK_NONE;
        }
        uint32_t item_id = reach_top_bar_pressable_detail(target);
        for (size_t index = 0; index < state->tray_item_count; ++index)
        {
            if (state->tray_items[index].id == item_id)
            {
                return REACH_TOP_BAR_FEEDBACK_TRAY_BASE + index;
            }
        }
        return REACH_PRESSABLE_FEEDBACK_NONE;
    }
    case REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW:
        return REACH_TOP_BAR_FEEDBACK_TRAY_OVERFLOW;
    case REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON:
        return REACH_TOP_BAR_FEEDBACK_QUICK_SETTINGS_BUTTON;
    case REACH_TOP_BAR_POINTER_REGION_SETTINGS_BUTTON:
        return REACH_TOP_BAR_FEEDBACK_SETTINGS_BUTTON;
    case REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON:
        return REACH_TOP_BAR_FEEDBACK_LANGUAGE_BUTTON;
    case REACH_TOP_BAR_POINTER_REGION_BATTERY_BUTTON:
        return REACH_TOP_BAR_FEEDBACK_BATTERY_BUTTON;
    case REACH_TOP_BAR_POINTER_REGION_NOW_PLAYING:
    case REACH_TOP_BAR_POINTER_REGION_NONE:
    default:
        return REACH_PRESSABLE_FEEDBACK_NONE;
    }
}

static void reach_top_bar_apply_pressable_result(const reach_pressable_result *pressable,
                                                 reach_top_bar_event_result *out)
{
    if (pressable == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw = out->redraw || pressable->redraw;
    out->capture = pressable->capture;
    out->sync_pointer_subscriptions =
        out->sync_pointer_subscriptions || pressable->sync_pointer_subscriptions;
}

static int32_t reach_top_bar_take_power_release_suppressed(reach_top_bar *top_bar)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || !state->power_release_suppressed)
    {
        return 0;
    }
    state->power_release_suppressed = 0;
    return 1;
}


void reach_top_bar_pointer_down(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                reach_pointer_button button, reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    uint64_t target = reach_top_bar_pressable_target_at(top_bar, local_x, local_y, button);
    reach_top_bar_pointer_region region = reach_top_bar_pressable_region(target);
    if (target == REACH_PRESSABLE_TARGET_NONE)
    {
        return;
    }
    if (button == REACH_POINTER_BUTTON_PRIMARY &&
        region != REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON)
    {
        state->power_release_suppressed = 0;
    }
    reach_pressable_feedback_style feedback = reach_top_bar_pressable_feedback(top_bar);
    reach_pressable_result pressable = {};
    reach_pressable_press(&state->pressable, button, target,
                          reach_top_bar_pressable_feedback_index(top_bar, target), &feedback,
                          &pressable);
    reach_top_bar_apply_pressable_result(&pressable, out);
    out->handled = 1;
    if (button == REACH_POINTER_BUTTON_PRIMARY && region == REACH_TOP_BAR_POINTER_REGION_TRAY_ICON)
    {
        size_t index =
            reach_top_bar_inline_tray_index(top_bar, reach_top_bar_pressable_detail(target));
        reach_top_bar_tray_drag_begin(top_bar, index, local_x, local_y, target);
    }
    if (button == REACH_POINTER_BUTTON_SECONDARY)
    {
        return;
    }
    switch (region)
    {
    case REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON:
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_POWER;
        return;
    case REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON:
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_QUICK_SETTINGS;
        return;
    case REACH_TOP_BAR_POINTER_REGION_NOW_PLAYING:
    case REACH_TOP_BAR_POINTER_REGION_TRAY_ICON:
    case REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW:
    case REACH_TOP_BAR_POINTER_REGION_SETTINGS_BUTTON:
    case REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON:
    case REACH_TOP_BAR_POINTER_REGION_BATTERY_BUTTON:
    default:
        return;
    }
}

void reach_top_bar_pointer_up(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                              reach_pointer_button button, reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    int32_t dragged =
        button == REACH_POINTER_BUTTON_PRIMARY ? reach_top_bar_tray_drag_end(top_bar) : 0;
    reach_pressable_feedback_style feedback = reach_top_bar_pressable_feedback(top_bar);
    reach_pressable_result pressable = {};
    reach_pressable_release(&state->pressable, button,
                            reach_top_bar_pressable_target_at(top_bar, local_x, local_y, button),
                            &feedback, &pressable);
    reach_top_bar_apply_pressable_result(&pressable, out);
    if (dragged)
    {
        out->handled = 1;
        out->redraw = 1;
        return;
    }
    if (!pressable.activated)
    {
        if (button == REACH_POINTER_BUTTON_PRIMARY)
        {
            state->power_release_suppressed = 0;
        }
        return;
    }
    reach_top_bar_pointer_region region =
        reach_top_bar_pressable_region(pressable.activated_target);
    uint32_t detail = reach_top_bar_pressable_detail(pressable.activated_target);
    out->handled = 1;
    if (button == REACH_POINTER_BUTTON_SECONDARY)
    {
        out->action_kind = REACH_FEATURE_ACTION_ACTIVATE_TRAY_ITEM;
        out->action_id = detail;
        out->action_index = REACH_TRAY_ACTION_RIGHT_CLICK;
        return;
    }
    switch (region)
    {
    case REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON:
        if (!reach_top_bar_take_power_release_suppressed(top_bar) &&
            top_bar->routes.power_activated != nullptr)
        {
            top_bar->routes.power_activated(top_bar->routes.user);
        }
        return;
    case REACH_TOP_BAR_POINTER_REGION_NOW_PLAYING:
        out->action_kind = REACH_FEATURE_ACTION_MEDIA_CONTROL;
        out->action_id = detail;
        return;
    case REACH_TOP_BAR_POINTER_REGION_TRAY_ICON:
        out->action_kind = REACH_FEATURE_ACTION_ACTIVATE_TRAY_ITEM;
        out->action_id = detail;
        out->action_index = REACH_TRAY_ACTION_LEFT_CLICK;
        return;
    case REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW:
        if (top_bar->routes.tray_overflow_activated != nullptr)
        {
            top_bar->routes.tray_overflow_activated(top_bar->routes.user);
        }
        return;
    case REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON:
        if (top_bar->routes.quick_settings_activated != nullptr)
        {
            top_bar->routes.quick_settings_activated(top_bar->routes.user);
        }
        return;
    case REACH_TOP_BAR_POINTER_REGION_SETTINGS_BUTTON:
        out->action_kind = REACH_FEATURE_ACTION_OPEN_SETTINGS_APP;
        return;
    case REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON:
        out->action_kind = REACH_FEATURE_ACTION_CYCLE_INPUT_LANGUAGE;
        return;
    case REACH_TOP_BAR_POINTER_REGION_BATTERY_BUTTON:
        if (top_bar->routes.battery_activated != nullptr)
        {
            top_bar->routes.battery_activated(top_bar->routes.user);
        }
        return;
    case REACH_TOP_BAR_POINTER_REGION_NONE:
    default:
        return;
    }
}

void reach_top_bar_pointer_move(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    reach_pressable_result pressable = {};
    reach_pressable_update(
        &state->pressable,
        reach_top_bar_pressable_target_at(top_bar, local_x, local_y,
                                          reach_pressable_button(&state->pressable)),
        &pressable);
    reach_top_bar_apply_pressable_result(&pressable, out);

    if (reach_draggable_tracking(&top_bar->tray_drag.gesture))
    {
        int32_t was_moved = reach_draggable_moved(&top_bar->tray_drag.gesture);
        reach_draggable_result gesture = {};
        reach_draggable_update(&top_bar->tray_drag.gesture, local_x, local_y, 36, &gesture);
        if (!was_moved && gesture.started)
        {
            reach_pressable_feedback_style feedback = reach_top_bar_pressable_feedback(top_bar);
            reach_pressable_disarm(&state->pressable, &feedback, &pressable);
            reach_top_bar_apply_pressable_result(&pressable, out);
        }
        if (gesture.moved && state->layout.tray_icon_count > 0)
        {
            float wanted_x = (float)local_x - top_bar->tray_drag.grab_offset_x;
            float min_x = state->layout.tray_icons[0].x;
            float max_x = state->layout.tray_icons[state->layout.tray_icon_count - 1].x;
            top_bar->tray_drag.x = wanted_x < min_x ? min_x : wanted_x > max_x ? max_x : wanted_x;
            size_t current = reach_top_bar_inline_tray_index(top_bar, top_bar->tray_drag.item_id);
            size_t target = reach_horizontal_reorder_target(state->layout.tray_icons,
                                                            state->layout.tray_icon_count, current,
                                                            top_bar->tray_drag.x, 0.25f);
            if (target < state->layout.tray_icon_count && target != current)
            {
                reach_top_bar_move_tray_item(top_bar, current, target);
            }
            out->redraw = 1;
        }
        out->handled = 1;
    }

    int32_t hovered = reach_top_bar_hit_test(&state->layout, local_x, local_y) ==
                      REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON;
    if (hovered == state->power_hovered)
    {
        return;
    }

    state->power_hovered = hovered;
    reach_animation_manager_animate_to(reach_top_bar_manager(top_bar),
                                       REACH_TOP_BAR_ANIM_POWER_HOVER, hovered ? 1.0f : 0.0f, 0.18,
                                       REACH_EASING_EASE_IN_OUT);
    out->handled = 1;
    out->redraw = 1;
}

void reach_top_bar_pointer_cancel(reach_top_bar *top_bar, reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    reach_pressable_feedback_style feedback = reach_top_bar_pressable_feedback(top_bar);
    reach_pressable_result pressable = {};
    reach_pressable_cancel(&state->pressable, &feedback, &pressable);
    reach_top_bar_apply_pressable_result(&pressable, out);
    (void)reach_top_bar_tray_drag_end(top_bar);
    state->power_release_suppressed = 0;
}

void reach_top_bar_pointer_leave(reach_top_bar *top_bar, reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    reach_pressable_result pressable = {};
    reach_pressable_update(&state->pressable, REACH_PRESSABLE_TARGET_NONE, &pressable);
    reach_top_bar_apply_pressable_result(&pressable, out);
    if (!state->power_hovered)
    {
        return;
    }

    state->power_hovered = 0;
    reach_animation_manager_animate_to(reach_top_bar_manager(top_bar),
                                       REACH_TOP_BAR_ANIM_POWER_HOVER, 0.0f, 0.18,
                                       REACH_EASING_EASE_IN_OUT);
    out->redraw = 1;
}
