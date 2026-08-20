#include "reach/features/top_bar.h"

#include "top_bar_common.h"
#include "top_bar_now_playing.h"

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

static int32_t reach_top_bar_feedback_start(reach_top_bar *top_bar, size_t slot,
                                            float target_opacity)
{
    if (top_bar == nullptr || slot >= REACH_TOP_BAR_FEEDBACK_NONE)
    {
        return 0;
    }

    reach_top_bar_state_mut(top_bar)->feedback_index = slot;
    reach_animation_manager_animate_to(reach_top_bar_manager(top_bar),
                                       REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY, target_opacity, 0.055,
                                       REACH_EASING_EASE_IN_OUT);
    return 1;
}

int32_t reach_top_bar_feedback_press(reach_top_bar *top_bar, size_t slot)
{
    if (top_bar == nullptr)
    {
        return 0;
    }

    reach_top_bar_state_mut(top_bar)->feedback_pressed = 1;
    return reach_top_bar_feedback_start(top_bar, slot, 0.50f);
}

int32_t reach_top_bar_feedback_release(reach_top_bar *top_bar)
{
    if (top_bar == nullptr ||
        (!reach_top_bar_state_mut(top_bar)->feedback_pressed &&
         reach_top_bar_state_mut(top_bar)->feedback_index == REACH_TOP_BAR_FEEDBACK_NONE))
    {
        return 0;
    }

    reach_top_bar_state_mut(top_bar)->feedback_pressed = 0;
    if (reach_top_bar_state_mut(top_bar)->feedback_index != REACH_TOP_BAR_FEEDBACK_NONE)
    {
        return reach_top_bar_feedback_start(
            top_bar, reach_top_bar_state_mut(top_bar)->feedback_index, 0.0f);
    }
    return 0;
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

static uint32_t reach_top_bar_media_action(reach_now_playing_action action)
{
    switch (action)
    {
    case REACH_NOW_PLAYING_ACTION_PREVIOUS:
        return REACH_TOP_BAR_POINTER_ACTION_MEDIA_PREVIOUS;
    case REACH_NOW_PLAYING_ACTION_PLAY_PAUSE:
        return REACH_TOP_BAR_POINTER_ACTION_MEDIA_PLAY_PAUSE;
    case REACH_NOW_PLAYING_ACTION_NEXT:
        return REACH_TOP_BAR_POINTER_ACTION_MEDIA_NEXT;
    default:
        return REACH_TOP_BAR_POINTER_ACTION_NONE;
    }
}

static void reach_top_bar_begin_pointer_sequence(reach_top_bar_state *state,
                                                 reach_top_bar_event_result *out)
{
    if (!state->pointer_sequence_active)
    {
        state->pointer_sequence_active = 1;
        out->sync_pointer_subscriptions = 1;
    }
}

static void reach_top_bar_end_pointer_sequence(reach_top_bar_state *state,
                                               reach_top_bar_event_result *out)
{
    if (state->pointer_sequence_active)
    {
        state->pointer_sequence_active = 0;
        out->sync_pointer_subscriptions = 1;
    }
}

void reach_top_bar_pointer_down(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    reach_top_bar_pointer_region hit = reach_top_bar_hit_test(&state->layout, local_x, local_y);
    reach_top_bar_begin_pointer_sequence(state, out);
    if (hit != REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON)
    {
        state->power_release_suppressed = 0;
    }

    if (reach_top_bar_now_playing_pointer_down(reach_top_bar_now_playing_subfeature(top_bar),
                                               local_x, local_y))
    {
        state->pressed_control = REACH_TOP_BAR_POINTER_REGION_NOW_PLAYING;
        out->handled = 1;
        out->redraw = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_NOW_PLAYING;
        return;
    }

    state->pressed_control = hit;
    switch (hit)
    {
    case REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON:
        out->redraw = reach_top_bar_feedback_press(top_bar, REACH_TOP_BAR_FEEDBACK_POWER_BUTTON);
        out->handled = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_POWER;
        return;
    case REACH_TOP_BAR_POINTER_REGION_TRAY_ICON:
        state->pressed_tray_index = reach_top_bar_tray_icon_at(&state->layout, local_x, local_y);
        out->redraw = reach_top_bar_feedback_press(
            top_bar, REACH_TOP_BAR_FEEDBACK_TRAY_BASE + state->pressed_tray_index);
        out->handled = 1;
        return;
    case REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW:
        out->redraw = reach_top_bar_feedback_press(top_bar, REACH_TOP_BAR_FEEDBACK_TRAY_OVERFLOW);
        out->handled = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_TRAY_OVERFLOW;
        return;
    case REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON:
        out->redraw =
            reach_top_bar_feedback_press(top_bar, REACH_TOP_BAR_FEEDBACK_QUICK_SETTINGS_BUTTON);
        out->handled = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_QUICK_SETTINGS;
        return;
    case REACH_TOP_BAR_POINTER_REGION_SETTINGS_BUTTON:
        out->redraw = reach_top_bar_feedback_press(top_bar, REACH_TOP_BAR_FEEDBACK_SETTINGS_BUTTON);
        out->handled = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_SETTINGS;
        return;
    case REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON:
        out->redraw = reach_top_bar_feedback_press(top_bar, REACH_TOP_BAR_FEEDBACK_LANGUAGE_BUTTON);
        out->handled = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_LANGUAGE;
        return;
    case REACH_TOP_BAR_POINTER_REGION_BATTERY_BUTTON:
        out->redraw = reach_top_bar_feedback_press(top_bar, REACH_TOP_BAR_FEEDBACK_BATTERY_BUTTON);
        out->handled = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_BATTERY;
        return;
    default:
        return;
    }
}

void reach_top_bar_pointer_up(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                              reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    out->redraw = reach_top_bar_feedback_release(top_bar);

    reach_now_playing_action media = REACH_NOW_PLAYING_ACTION_NONE;
    if (reach_top_bar_now_playing_pointer_up(reach_top_bar_now_playing_subfeature(top_bar), local_x,
                                             local_y, &media))
    {
        state->pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
        out->handled = 1;
        out->redraw = 1;
        out->action_kind = reach_top_bar_media_action(media);
        reach_top_bar_end_pointer_sequence(state, out);
        return;
    }

    reach_top_bar_pointer_region hit = reach_top_bar_hit_test(&state->layout, local_x, local_y);
    reach_top_bar_pointer_region pressed =
        static_cast<reach_top_bar_pointer_region>(state->pressed_control);
    state->pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;

    if (hit == pressed)
    {
        switch (pressed)
        {
        case REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON:
            out->handled = 1;
            if (!reach_top_bar_take_power_release_suppressed(top_bar))
            {
                out->action_kind = REACH_TOP_BAR_POINTER_ACTION_TOGGLE_POWER;
            }
            break;
        case REACH_TOP_BAR_POINTER_REGION_TRAY_ICON:
            if (reach_top_bar_tray_icon_at(&state->layout, local_x, local_y) ==
                    state->pressed_tray_index &&
                state->pressed_tray_index < state->tray_item_count)
            {
                out->handled = 1;
                out->action_kind = REACH_TOP_BAR_POINTER_ACTION_ACTIVATE_TRAY_LEFT;
                out->action_id = state->tray_items[state->pressed_tray_index].id;
            }
            break;
        case REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW:
            out->handled = 1;
            out->action_kind = REACH_TOP_BAR_POINTER_ACTION_TOGGLE_TRAY_OVERFLOW;
            break;
        case REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON:
            out->handled = 1;
            out->action_kind = REACH_TOP_BAR_POINTER_ACTION_TOGGLE_QUICK_SETTINGS;
            break;
        case REACH_TOP_BAR_POINTER_REGION_SETTINGS_BUTTON:
            out->handled = 1;
            out->action_kind = REACH_TOP_BAR_POINTER_ACTION_OPEN_SETTINGS;
            break;
        case REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON:
            out->handled = 1;
            out->action_kind = REACH_TOP_BAR_POINTER_ACTION_CYCLE_LANGUAGE;
            break;
        case REACH_TOP_BAR_POINTER_REGION_BATTERY_BUTTON:
            out->handled = 1;
            out->action_kind = REACH_TOP_BAR_POINTER_ACTION_TOGGLE_BATTERY;
            break;
        default:
            break;
        }
    }

    reach_top_bar_end_pointer_sequence(state, out);
}

void reach_top_bar_pointer_move(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
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

void reach_top_bar_pointer_context(reach_top_bar *top_bar, int32_t local_x, int32_t local_y,
                                   reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    if (reach_top_bar_hit_test(&state->layout, local_x, local_y) !=
        REACH_TOP_BAR_POINTER_REGION_TRAY_ICON)
    {
        return;
    }

    size_t index = reach_top_bar_tray_icon_at(&state->layout, local_x, local_y);
    if (index < state->tray_item_count)
    {
        out->handled = 1;
        out->action_kind = REACH_TOP_BAR_POINTER_ACTION_ACTIVATE_TRAY_RIGHT;
        out->action_id = state->tray_items[index].id;
    }
}

void reach_top_bar_pointer_cancel(reach_top_bar *top_bar, reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    out->redraw =
        reach_top_bar_now_playing_pointer_cancel(reach_top_bar_now_playing_subfeature(top_bar));
    out->redraw = reach_top_bar_feedback_release(top_bar) || out->redraw;
    state->pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
    reach_top_bar_end_pointer_sequence(state, out);
}

void reach_top_bar_pointer_leave(reach_top_bar *top_bar, reach_top_bar_event_result *out)
{
    reach_top_bar_state *state = reach_top_bar_state_mut(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return;
    }

    out->redraw =
        reach_top_bar_now_playing_pointer_cancel(reach_top_bar_now_playing_subfeature(top_bar));
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
