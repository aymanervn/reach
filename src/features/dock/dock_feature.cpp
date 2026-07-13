#include "reach/features/dock.h"

#include "dock_common_state.h"
#include "dock_interaction.h"
#include "dock_now_playing.h"

#include <math.h>
#include <new>
#include <stdio.h>
#include <time.h>

enum reach_dock_slot_lifecycle
{
    REACH_DOCK_SLOT_EMPTY = 0,
    REACH_DOCK_SLOT_APPEARING = 1,
    REACH_DOCK_SLOT_STEADY = 2,
    REACH_DOCK_SLOT_DYING = 3
};

struct reach_dock_slot
{
    int32_t lifecycle;
    reach_dock_order_key key;
    float target_width;
};

static const double REACH_DOCK_SLOT_ANIMATION_SECONDS = 0.25;
static const float REACH_DOCK_SLOT_REVEAL_THRESHOLD = 0.7f;

struct reach_dock;
static void reach_dock_settle_slots(reach_dock *dock);
static void reach_dock_snap_slots(reach_dock *dock);
static void reach_dock_gate_animating_hit(reach_dock *dock, reach_dock_hit_result *hit);

struct reach_dock
{
    reach_animation_manager manager;
    reach_animation_track tracks[REACH_DOCK_ANIM_COUNT];
    reach_dock_state state;

    reach_dock_slot slots[REACH_DOCK_SLOT_CAPACITY];
    uint8_t slot_order[REACH_DOCK_SLOT_CAPACITY];
    size_t slot_order_count;
    int32_t slots_synced;

    int32_t np_content_armed;
    double np_content_delay;
    reach_icon_service *icons;
    reach_window_tracking *windows;
    reach_now_playing_service *now_playing;
    reach_dock_now_playing *now_playing_subfeature;
    const reach_theme *pointer_theme;
    reach_dock_layout pointer_layout;
    int32_t pointer_layout_valid;
    const reach_pinned_app_model *pointer_pinned_apps;
    size_t pointer_pinned_app_count;
};

void reach_dock_attach_services(reach_dock *dock, reach_icon_service *icons,
                                reach_window_tracking *windows,
                                reach_now_playing_service *now_playing)
{
    if (dock != nullptr)
    {
        dock->icons = icons;
        dock->windows = windows;
        dock->now_playing = now_playing;
    }
}

reach_dock_now_playing *reach_dock_now_playing_subfeature(reach_dock *dock)
{
    return dock != nullptr ? dock->now_playing_subfeature : nullptr;
}

reach_icon_service *reach_dock_icons(reach_dock *dock)
{
    return dock != nullptr ? dock->icons : nullptr;
}

reach_window_tracking *reach_dock_windows(reach_dock *dock)
{
    return dock != nullptr ? dock->windows : nullptr;
}

const reach_dock_state *reach_dock_state_ptr(reach_dock *animations)
{
    return animations != nullptr ? &animations->state : nullptr;
}

reach_dock_state *reach_dock_state_mut(reach_dock *animations)
{
    return animations != nullptr ? &animations->state : nullptr;
}

reach_result reach_dock_create(reach_dock **out_animations)
{
    if (out_animations == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_dock *animations = new (std::nothrow) reach_dock();
    if (animations == nullptr)
    {
        return REACH_ERROR;
    }
    reach_animation_manager_init(&animations->manager, animations->tracks, REACH_DOCK_ANIM_COUNT);
    if (reach_dock_now_playing_create(&animations->now_playing_subfeature) != REACH_OK)
    {
        delete animations;
        return REACH_ERROR;
    }
    animations->state.drag.source_index = REACH_MAX_PINNED_APPS;
    animations->state.drag.target_index = REACH_MAX_PINNED_APPS;
    animations->state.pressed_index = REACH_MAX_PINNED_APPS;
    animations->state.pressed_control = REACH_DOCK_HIT_NONE;
    animations->state.feedback_index = REACH_DOCK_FEEDBACK_NONE;
    *out_animations = animations;
    return REACH_OK;
}

void reach_dock_destroy(reach_dock *animations)
{
    if (animations != nullptr)
    {
        reach_dock_now_playing_destroy(animations->now_playing_subfeature);
    }
    delete animations;
}

static void reach_dock_tick(reach_dock *animations, double delta_seconds,
                            reach_feature_tick_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    if (animations == nullptr)
    {
        return;
    }
    reach_dock_state *state = &animations->state;
    reach_animation_manager *manager = &animations->manager;

    reach_dock_now_playing_update_result now_playing = {};
    reach_dock_now_playing_sync(animations->now_playing_subfeature, animations->now_playing,
                                &now_playing);
    if (now_playing.changed && out != nullptr)
    {
        out->redraw = 1;
    }
    if (now_playing.visibility_changed)
    {
        state->items_changed = 1;
        if (out != nullptr)
        {
            out->relayout = 1;
        }
    }

    int32_t feedback_was_active =
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY);
    int32_t drag_snap_was_active =
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP);
    int32_t item_was_active[REACH_MAX_PINNED_APPS] = {};
    for (size_t index = 0; index < state->model.item_count; ++index)
    {
        item_was_active[index] =
            reach_animation_manager_active(manager, reach_dock_item_animation_id(index));
    }

    int32_t slots_were_animating = reach_dock_slots_animating(animations);
    int32_t content_was_active =
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT);

    reach_animation_manager_tick(manager, delta_seconds);

    int32_t redraw = 0;
    if (feedback_was_active ||
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY))
    {
        redraw = 1;
    }

    if (feedback_was_active &&
        !reach_animation_manager_active(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY) &&
        !state->feedback_pressed && !state->feedback_sticky &&
        reach_animation_manager_value(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY) <= 0.001f)
    {
        reach_animation_manager_set(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY, 0.0f);
        state->feedback_index = REACH_DOCK_FEEDBACK_NONE;
    }

    for (size_t index = 0; index < state->model.item_count; ++index)
    {
        if (item_was_active[index] ||
            reach_animation_manager_active(manager, reach_dock_item_animation_id(index)))
        {
            redraw = 1;
        }
    }

    if (drag_snap_was_active ||
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP))
    {
        state->drag.x = reach_animation_manager_value(manager, REACH_DOCK_ANIM_DRAG_SNAP);
        redraw = 1;
    }

    if (drag_snap_was_active &&
        !reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP))
    {
        state->drag.source_index = REACH_MAX_PINNED_APPS;
        state->drag.target_index = REACH_MAX_PINNED_APPS;
        state->drag.pinned = 0;
        state->drag.pin_id = 0;
        state->drag.window = 0;
    }

    reach_dock_settle_slots(animations);
    if (slots_were_animating || reach_dock_slots_animating(animations))
    {
        redraw = 1;
        if (out != nullptr)
        {
            out->relayout = 1;
        }
    }

    {
        const reach_animation_track *np_track = &animations->tracks[REACH_DOCK_ANIM_SLOT_BASE];
        const float np_target = animations->slots[0].target_width;
        const int32_t content_active =
            reach_animation_manager_active(manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT);
        const float content_value =
            reach_animation_manager_value(manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT);
        if (np_target > 0.0f && !content_active && content_value < 1.0f)
        {
            if (np_track->active)
            {
                if (!animations->np_content_armed)
                {
                    animations->np_content_delay = reach_animation_track_time_to_value(
                        np_track, np_target * REACH_DOCK_SLOT_REVEAL_THRESHOLD);
                    animations->np_content_armed = 1;
                }
                else
                {
                    animations->np_content_delay -= delta_seconds;
                }
                if (animations->np_content_delay <= 0.0)
                {
                    double land_seconds =
                        reach_animation_track_time_to_value(np_track, np_target);
                    if (land_seconds < 0.05)
                    {
                        land_seconds = 0.05;
                    }
                    reach_animation_manager_start(manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT,
                                                  0.0f, 1.0f, land_seconds,
                                                  REACH_EASING_EASE_OUT);
                    animations->np_content_armed = 0;
                }
            }
            else
            {

                animations->np_content_armed = 0;
                reach_animation_manager_start(manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT, 0.0f,
                                              1.0f, 0.1, REACH_EASING_EASE_OUT);
            }
        }
        if (content_was_active ||
            reach_animation_manager_active(manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT))
        {
            redraw = 1;
        }
    }

    if (redraw && out != nullptr)
    {
        out->redraw = 1;
    }
}

void reach_dock_mark_items_changed(reach_dock *dock)
{
    if (dock != nullptr)
    {
        dock->state.items_changed = 1;
    }
}

int32_t reach_dock_take_items_changed(reach_dock *dock)
{
    if (dock == nullptr || !dock->state.items_changed)
    {
        return 0;
    }
    dock->state.items_changed = 0;
    return 1;
}

int32_t reach_dock_pointer_sequence_active(const reach_dock *dock)
{
    return dock != nullptr && dock->state.pointer_sequence_active;
}

reach_dock_pointer_region reach_dock_pointer_region_at(const reach_dock *dock, int32_t local_x,
                                                       int32_t local_y)
{
    if (dock == nullptr || !dock->pointer_layout_valid)
    {
        return REACH_DOCK_POINTER_REGION_NONE;
    }
    reach_dock_hit_result hit = reach_dock_hit_test(&dock->pointer_layout, local_x, local_y);
    reach_dock_gate_animating_hit(const_cast<reach_dock *>(dock), &hit);
    switch (hit.type)
    {
    case REACH_DOCK_HIT_ITEM:
        return REACH_DOCK_POINTER_REGION_ITEM;
    case REACH_DOCK_HIT_TRAY_BUTTON:
        return REACH_DOCK_POINTER_REGION_TRAY_BUTTON;
    case REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON:
        return REACH_DOCK_POINTER_REGION_QUICK_SETTINGS_BUTTON;
    case REACH_DOCK_HIT_POWER_BUTTON:
        return REACH_DOCK_POINTER_REGION_POWER_BUTTON;
    case REACH_DOCK_HIT_NONE:
    default:
        return REACH_DOCK_POINTER_REGION_NONE;
    }
}

static int32_t reach_dock_begin_pointer_sequence(reach_dock *dock)
{
    if (dock == nullptr || dock->state.pointer_sequence_active)
    {
        return 0;
    }
    dock->state.pointer_sequence_active = 1;
    return 1;
}

static int32_t reach_dock_end_pointer_sequence(reach_dock *dock)
{
    if (dock == nullptr || !dock->state.pointer_sequence_active)
    {
        return 0;
    }
    dock->state.pointer_sequence_active = 0;
    return 1;
}

void reach_dock_suppress_power_release(reach_dock *dock)
{
    if (dock != nullptr)
    {
        dock->state.power_release_suppressed = 1;
    }
}

int32_t reach_dock_take_power_release_suppressed(reach_dock *dock)
{
    if (dock == nullptr || !dock->state.power_release_suppressed)
    {
        return 0;
    }
    dock->state.power_release_suppressed = 0;
    return 1;
}

void reach_dock_clear_power_release_suppressed(reach_dock *dock)
{
    if (dock != nullptr)
    {
        dock->state.power_release_suppressed = 0;
    }
}

void reach_dock_begin_reveal_session(reach_dock *dock)
{
    if (dock != nullptr)
    {
        dock->state.reveal_session_active = 1;
    }
}

static void reach_dock_reset_reveal_state(reach_dock *dock)
{
    if (dock == nullptr)
    {
        return;
    }
    dock->state.dock_animation_initialized = 0;
    dock->state.reveal_session_active = 0;
    dock->state.pointer_sequence_active = 0;
}

static void reach_dock_reset_model(reach_dock *dock)
{
    if (dock != nullptr)
    {
        reach_dock_feature_model_init(&dock->state.model);
    }
}

static int32_t reach_dock_window_matches_app_thunk(void *user,
                                                   const reach_pinned_app_model *pinned_app,
                                                   const reach_window_snapshot *window)
{
    (void)user;
    return reach_window_tracking_window_matches_app(pinned_app, window);
}

static void reach_dock_build_items(reach_dock *dock,
                                   const reach_pinned_app_model *pinned_apps,
                                   size_t pinned_app_count)
{
    if (dock == nullptr)
    {
        return;
    }
    reach_dock_feature_model_build_items(
        &dock->state.model, pinned_apps, pinned_app_count,
        reach_window_tracking_windows(dock->windows),
        reach_window_tracking_window_count(dock->windows),
        reach_dock_window_matches_app_thunk, nullptr);
}

static void reach_dock_capsule_reset(void *capsule)
{
    reach_dock *dock = static_cast<reach_dock *>(capsule);
    reach_dock_reset_model(dock);
    if (dock != nullptr)
    {
        reach_dock_now_playing_reset(dock->now_playing_subfeature);
        dock->pointer_layout_valid = 0;
        dock->state.pressed_control = REACH_DOCK_HIT_NONE;

        dock->slots_synced = 0;
    }
}

static void reach_dock_capsule_tick(void *capsule, double delta_seconds,
                                    reach_feature_tick_result *out)
{
    reach_dock_tick(static_cast<reach_dock *>(capsule), delta_seconds, out);
}

static int32_t reach_dock_capsule_is_open(const void *capsule)
{
    (void)capsule;
    return 1;
}

static void reach_dock_capsule_on_game_mode(void *capsule, int32_t enabled)
{
    if (enabled)
    {
        reach_dock_reset_reveal_state(static_cast<reach_dock *>(capsule));
    }
}

static int32_t reach_dock_capsule_needs_frame(const void *capsule)
{
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    return dock != nullptr &&
           (reach_animation_manager_any_active(&dock->manager) || dock->state.drag.active);
}

static int32_t reach_dock_capsule_wants_pointer_move(const void *capsule)
{
    return reach_dock_pointer_sequence_active(static_cast<const reach_dock *>(capsule));
}

static void reach_dock_capsule_begin_pointer_sequence(reach_dock *dock,
                                                      reach_capsule_pointer_result *out)
{
    if (reach_dock_begin_pointer_sequence(dock))
    {
        out->capture = 1;
        out->sync_pointer_subscriptions = 1;
    }
}

static void reach_dock_capsule_end_pointer_sequence(reach_dock *dock,
                                                    reach_capsule_pointer_result *out)
{
    if (reach_dock_end_pointer_sequence(dock))
    {
        out->capture = -1;
        out->sync_pointer_subscriptions = 1;
    }
}

static reach_dock_interaction_context reach_dock_capsule_interaction_context(reach_dock *dock)
{
    reach_dock_interaction_context ctx = {};
    if (dock != nullptr)
    {
        ctx.theme = dock->pointer_theme;
        ctx.layout = dock->pointer_layout_valid ? &dock->pointer_layout : nullptr;
        ctx.pinned_apps = dock->pointer_pinned_apps;
        ctx.pinned_app_count = dock->pointer_pinned_app_count;
    }
    return ctx;
}

static int32_t reach_dock_capsule_screen_x(const reach_dock *dock, int32_t local_x)
{
    return dock != nullptr && dock->pointer_layout_valid
               ? static_cast<int32_t>((float)local_x + dock->pointer_layout.bounds.x)
               : local_x;
}

static int32_t reach_dock_capsule_screen_y(const reach_dock *dock, int32_t local_y)
{
    return dock != nullptr && dock->pointer_layout_valid
               ? static_cast<int32_t>((float)local_y + dock->pointer_layout.bounds.y)
               : local_y;
}

static void reach_dock_capsule_apply_interaction_result(
    const reach_dock_interaction_result *interaction, reach_capsule_pointer_result *out)
{
    if (interaction == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw = out->redraw || interaction->redraw;
    if (interaction->rebuild_items)
    {
        out->action.kind = REACH_DOCK_POINTER_ACTION_REBUILD_ITEMS;
    }
    if (interaction->move_pin)
    {
        out->action.kind = REACH_DOCK_POINTER_ACTION_MOVE_PIN;
        out->action.id = interaction->move_pin_id;
        out->action.index = interaction->move_pin_target;
    }
}

static uint32_t reach_dock_capsule_media_action(reach_now_playing_action action)
{
    switch (action)
    {
    case REACH_NOW_PLAYING_ACTION_PREVIOUS:
        return REACH_DOCK_POINTER_ACTION_MEDIA_PREVIOUS;
    case REACH_NOW_PLAYING_ACTION_PLAY_PAUSE:
        return REACH_DOCK_POINTER_ACTION_MEDIA_PLAY_PAUSE;
    case REACH_NOW_PLAYING_ACTION_NEXT:
        return REACH_DOCK_POINTER_ACTION_MEDIA_NEXT;
    default:
        return REACH_DOCK_POINTER_ACTION_NONE;
    }
}

static void reach_dock_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                              reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_dock *dock = static_cast<reach_dock *>(capsule);
    if (dock == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }

    reach_pointer_event local_event = *event;
    if (dock->pointer_layout_valid)
    {
        reach_point_i32 local = reach_dock_local_point(&dock->pointer_layout, event->x, event->y);
        local_event.x = local.x;
        local_event.y = local.y;
    }
    event = &local_event;
    reach_dock_now_playing_update_result update = {};
    reach_dock_now_playing_sync(dock->now_playing_subfeature, dock->now_playing, &update);
    if (update.changed)
    {
        out->redraw = 1;
    }
    if (update.visibility_changed)
    {
        dock->state.items_changed = 1;
        out->relayout = 1;
    }

    reach_dock_state *state = &dock->state;
    reach_dock_interaction_context interaction_ctx =
        reach_dock_capsule_interaction_context(dock);
    reach_dock_hit_result hit = {};
    hit.type = REACH_DOCK_HIT_NONE;
    hit.index = REACH_MAX_PINNED_APPS;
    if (dock->pointer_layout_valid)
    {
        hit = reach_dock_hit_test(&dock->pointer_layout, event->x, event->y);
        reach_dock_gate_animating_hit(dock, &hit);
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN)
    {
        reach_dock_capsule_begin_pointer_sequence(dock, out);
        if (!reach_dock_slots_animating(dock) &&
            reach_dock_now_playing_pointer_down(dock->now_playing_subfeature, event->x,
                                                event->y))
        {
            out->handled = 1;
            out->redraw = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_PRESS_NOW_PLAYING;
            return;
        }

        if (hit.type != REACH_DOCK_HIT_POWER_BUTTON)
        {
            reach_dock_clear_power_release_suppressed(dock);
        }
        state->pressed_control = hit.type;
        if (hit.type == REACH_DOCK_HIT_TRAY_BUTTON)
        {
            out->redraw = out->redraw || reach_dock_feedback_press(
                                              dock, REACH_DOCK_FEEDBACK_TRAY_BUTTON);
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_PRESS_TRAY;
            return;
        }
        if (hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON)
        {
            out->redraw = out->redraw || reach_dock_feedback_press(
                                              dock, REACH_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON);
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_PRESS_QUICK_SETTINGS;
            return;
        }
        if (hit.type == REACH_DOCK_HIT_POWER_BUTTON)
        {
            out->redraw = out->redraw || reach_dock_feedback_press(
                                              dock, REACH_DOCK_FEEDBACK_POWER_BUTTON);
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_PRESS_POWER;
            return;
        }
        if (hit.type == REACH_DOCK_HIT_ITEM)
        {
            reach_dock_interaction_result interaction = {};
            reach_dock_item_press(
                dock, hit.index, reach_dock_capsule_screen_x(dock, event->x),
                reach_dock_capsule_screen_y(dock, event->y), &interaction_ctx, &interaction);
            reach_dock_capsule_apply_interaction_result(&interaction, out);
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_PRESS_ITEM;
            out->action.index = hit.index;
            return;
        }

        state->pressed_control = REACH_DOCK_HIT_NONE;
        reach_dock_clear_pressed(dock);
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_UP)
    {
        if (state->drag.active)
        {
            int32_t moved = state->drag.moved;
            reach_dock_interaction_result interaction = {};
            reach_dock_drag_end(dock, &interaction_ctx, &interaction);
            reach_dock_capsule_apply_interaction_result(&interaction, out);
            if (moved)
            {
                state->pressed_control = REACH_DOCK_HIT_NONE;
                out->handled = 1;
                reach_dock_capsule_end_pointer_sequence(dock, out);
                return;
            }
        }
        else
        {
            out->redraw = out->redraw || reach_dock_feedback_release(dock);
        }

        reach_now_playing_action action = REACH_NOW_PLAYING_ACTION_NONE;
        if (reach_dock_now_playing_pointer_up(dock->now_playing_subfeature, event->x,
                                              event->y, &action))
        {
            out->handled = 1;
            out->redraw = 1;
            out->action.kind = reach_dock_capsule_media_action(action);
            state->pressed_control = REACH_DOCK_HIT_NONE;
            reach_dock_capsule_end_pointer_sequence(dock, out);
            return;
        }

        reach_dock_hit_type pressed = static_cast<reach_dock_hit_type>(state->pressed_control);
        state->pressed_control = REACH_DOCK_HIT_NONE;
        if (pressed == REACH_DOCK_HIT_TRAY_BUTTON && hit.type == pressed)
        {
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_TOGGLE_TRAY;
        }
        else if (pressed == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON && hit.type == pressed)
        {
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_TOGGLE_QUICK_SETTINGS;
        }
        else if (pressed == REACH_DOCK_HIT_POWER_BUTTON && hit.type == pressed)
        {
            out->handled = 1;
            if (!reach_dock_take_power_release_suppressed(dock))
            {
                out->action.kind = REACH_DOCK_POINTER_ACTION_TOGGLE_POWER;
            }
        }
        else if (pressed == REACH_DOCK_HIT_ITEM && hit.type == pressed)
        {
            reach_dock_item_action item_action = {};
            reach_dock_interaction_result interaction = {};
            if (reach_dock_item_release(dock, hit.index, &item_action, &interaction))
            {
                reach_dock_capsule_apply_interaction_result(&interaction, out);
                out->handled = 1;
                if (item_action.type == REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED)
                {
                    out->action.kind = REACH_DOCK_POINTER_ACTION_LAUNCH_PINNED;
                    out->action.index = item_action.pinned_index;
                    out->action.id = item_action.pin_id;
                }
                else if (item_action.type == REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW)
                {
                    out->action.kind = REACH_DOCK_POINTER_ACTION_FOCUS_WINDOW;
                    out->action.window = item_action.window;
                }
            }
        }
        else
        {
            reach_dock_clear_pressed(dock);
        }
        reach_dock_capsule_end_pointer_sequence(dock, out);
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_MOVE)
    {
        if (state->drag.active)
        {
            reach_dock_interaction_result interaction = {};
            reach_dock_drag_update(
                dock, reach_dock_capsule_screen_x(dock, event->x),
                reach_dock_capsule_screen_y(dock, event->y), &interaction_ctx, &interaction);
            reach_dock_capsule_apply_interaction_result(&interaction, out);
            out->handled = 1;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_MIDDLE)
    {
        out->redraw = out->redraw || reach_dock_feedback_release(dock);
        state->pressed_control = REACH_DOCK_HIT_NONE;
        reach_dock_clear_pressed(dock);
        if (hit.type == REACH_DOCK_HIT_ITEM)
        {
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_LAUNCH_NEW_INSTANCE;
            out->action.index = hit.index;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_CONTEXT)
    {
        out->redraw = out->redraw || reach_dock_feedback_clear_sticky(dock);
        if (hit.type == REACH_DOCK_HIT_ITEM)
        {
            out->redraw = out->redraw ||
                          reach_dock_feedback_press_immediate(dock, hit.index, 0.50f);
            out->handled = 1;
            out->action.kind = REACH_DOCK_POINTER_ACTION_SHOW_ITEM_CONTEXT;
            out->action.index = hit.index;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_CANCEL)
    {
        out->redraw = out->redraw ||
                      reach_dock_now_playing_pointer_cancel(dock->now_playing_subfeature);
        if (state->drag.active)
        {
            reach_dock_interaction_result interaction = {};
            reach_dock_drag_end(dock, &interaction_ctx, &interaction);
            reach_dock_capsule_apply_interaction_result(&interaction, out);
        }
        out->redraw = out->redraw || reach_dock_feedback_release(dock);
        state->pressed_control = REACH_DOCK_HIT_NONE;
        reach_dock_clear_pressed(dock);
        reach_dock_capsule_end_pointer_sequence(dock, out);
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_LEAVE)
    {
        out->redraw = out->redraw ||
                      reach_dock_now_playing_pointer_cancel(dock->now_playing_subfeature);
    }
}

const reach_feature_capsule_ops *reach_dock_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_dock_capsule_reset,   reach_dock_capsule_tick,
        reach_dock_capsule_is_open, nullptr  ,
        reach_dock_capsule_on_game_mode, reach_dock_capsule_needs_frame,
        reach_dock_capsule_wants_pointer_move, reach_dock_capsule_handle_pointer,
    };
    return &ops;
}

reach_animation_manager *reach_dock_manager(reach_dock *animations)
{
    return animations != nullptr ? &animations->manager : nullptr;
}

reach_rect_f32 reach_dock_reveal_edge_bounds(int32_t mode, reach_rect_f32 shown_dock_bounds,
                                             reach_rect_f32 monitor_bounds)
{
    float monitor_bottom = monitor_bounds.y + monitor_bounds.height;
    reach_rect_f32 bounds = {};
    bounds.x = shown_dock_bounds.x;
    bounds.width = shown_dock_bounds.width;
    if (mode == REACH_DOCK_REVEAL_EDGE_BRIDGE)
    {
        bounds.y = shown_dock_bounds.y;
        bounds.height = monitor_bottom - shown_dock_bounds.y;
    }
    else
    {
        bounds.y = monitor_bottom - 2.0f;
        bounds.height = 3.0f;
    }
    return bounds;
}

static int32_t reach_dock_point_in_rect(reach_point_i32 point, reach_rect_f32 rect)
{
    return (float)point.x >= rect.x && (float)point.x < rect.x + rect.width &&
           (float)point.y >= rect.y && (float)point.y < rect.y + rect.height;
}

reach_dock_visibility_result reach_dock_update_visibility(reach_dock *animations,
                                                         const reach_dock_visibility_request *request)
{
    reach_dock_visibility_result result = {};
    if (animations == nullptr || request == nullptr)
    {
        return result;
    }

    reach_dock_state *state = &animations->state;
    reach_animation_manager *manager = &animations->manager;

    float hidden_y = request->monitor_bounds.y + request->monitor_bounds.height + 4.0f;
    reach_rect_f32 current_dock_bounds = request->shown_bounds;
    if (state->dock_animation_initialized)
    {
        current_dock_bounds.y = reach_animation_manager_value(manager, REACH_DOCK_ANIM_Y);
    }
    reach_rect_f32 bridge_bounds = reach_dock_reveal_edge_bounds(
        REACH_DOCK_REVEAL_EDGE_BRIDGE, request->shown_bounds, request->monitor_bounds);
    int32_t pointer_over_dock =
        request->pointer_valid && reach_dock_point_in_rect(request->pointer, current_dock_bounds);
    int32_t pointer_in_bridge =
        request->pointer_valid && reach_dock_point_in_rect(request->pointer, bridge_bounds);

    int32_t target_hidden = 0;
    int32_t edge_mode = REACH_DOCK_REVEAL_EDGE_DISABLED;

    if (request->game_mode)
    {
        state->reveal_session_active = 0;
        target_hidden = 1;
        edge_mode = REACH_DOCK_REVEAL_EDGE_DISABLED;
    }
    else if (!request->can_hide)
    {
        state->reveal_session_active = 0;
        target_hidden = 0;
        edge_mode = REACH_DOCK_REVEAL_EDGE_BRIDGE;
    }
    else if (state->pointer_sequence_active || request->transient_open)
    {
        target_hidden = 0;
        edge_mode = REACH_DOCK_REVEAL_EDGE_BRIDGE;
    }
    else if (state->reveal_session_active)
    {
        if (pointer_in_bridge || pointer_over_dock)
        {
            edge_mode = REACH_DOCK_REVEAL_EDGE_BRIDGE;
        }
        else
        {
            state->reveal_session_active = 0;
            target_hidden = 1;
            edge_mode = REACH_DOCK_REVEAL_EDGE_THIN;
        }
    }
    else if (pointer_over_dock)
    {
        target_hidden = 0;
    }
    else
    {
        target_hidden = 1;
        edge_mode = REACH_DOCK_REVEAL_EDGE_THIN;
    }

    float target_y = target_hidden ? hidden_y : request->shown_bounds.y;
    if (target_hidden && request->dock_sticky_feedback)
    {
        result.clear_sticky_feedback = 1;
    }

    if (!state->dock_animation_initialized)
    {
        state->dock_animation_initialized = 1;
        state->target_hidden = target_hidden;
        reach_animation_manager_set(manager, REACH_DOCK_ANIM_Y, target_y);
    }

    if (state->target_hidden != target_hidden)
    {
        state->target_hidden = target_hidden;
        reach_animation_manager_animate_to(manager, REACH_DOCK_ANIM_Y, target_y, 0.25,
                                           REACH_EASING_EASE_IN_OUT);
    }

    reach_rect_f32 animated = request->shown_bounds;
    animated.y = reach_animation_manager_value(manager, REACH_DOCK_ANIM_Y);
    result.animated_bounds = animated;
    result.edge_mode = edge_mode;
    result.visible = target_hidden ? 0 : 1;
    return result;
}

static void reach_dock_copy_ascii_to_utf16(uint16_t *dst, size_t dst_count, const char *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    size_t index = 0;
    if (src != nullptr)
    {
        while (index + 1 < dst_count && src[index] != 0)
        {
            dst[index] = (uint16_t)(unsigned char)src[index];
            ++index;
        }
    }
    dst[index] = 0;
}

static int32_t reach_dock_utf16_equal(const uint16_t *a, const uint16_t *b)
{
    size_t index = 0;
    if (a == nullptr || b == nullptr)
    {
        return a == b;
    }
    while (a[index] != 0 || b[index] != 0)
    {
        if (a[index] != b[index])
        {
            return 0;
        }
        ++index;
    }
    return 1;
}

int32_t reach_dock_update_clock(reach_dock *dock)
{
    if (dock == nullptr)
    {
        return 0;
    }

    reach_dock_state *state = &dock->state;

    time_t now = time(nullptr);
    int64_t current_minute = (int64_t)(now / 60);
    if (state->clock_initialized && state->clock_last_minute == current_minute)
    {
        return 0;
    }

    struct tm local = {};
    if (now == (time_t)-1 || localtime_s(&local, &now) != 0)
    {
        return 0;
    }

    static const char *months[] = {"January",   "February", "March",    "April",
                                   "May",       "June",     "July",     "August",
                                   "September", "October",  "November", "December"};
    static const char *days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};

    int hour = local.tm_hour % 12;
    if (hour == 0)
    {
        hour = 12;
    }
    const char *suffix = local.tm_hour >= 12 ? "PM" : "AM";

    char time_text[32] = {};
    char date_text[64] = {};
    snprintf(time_text, sizeof(time_text), "%d:%02d %s", hour, local.tm_min, suffix);
    if (local.tm_mon < 0 || local.tm_mon > 11 || local.tm_wday < 0 || local.tm_wday > 6)
    {
        return 0;
    }
    snprintf(date_text, sizeof(date_text), "%.3s %d, %.3s", months[local.tm_mon], local.tm_mday,
             days[local.tm_wday]);

    uint16_t next_time[32] = {};
    uint16_t next_date[64] = {};
    reach_dock_copy_ascii_to_utf16(next_time, 32, time_text);
    reach_dock_copy_ascii_to_utf16(next_date, 64, date_text);
    int32_t redraw = 0;
    if (!state->clock_initialized || !reach_dock_utf16_equal(state->clock_time_text, next_time) ||
        !reach_dock_utf16_equal(state->clock_date_text, next_date))
    {
        reach_copy_utf16(state->clock_time_text, 32, next_time);
        reach_copy_utf16(state->clock_date_text, 64, next_date);
        state->clock_initialized = 1;
        redraw = 1;
    }
    state->clock_last_minute = current_minute;
    return redraw;
}

static reach_dock_order_key reach_dock_order_key_for_item(const reach_dock_item_model *item)
{
    reach_dock_order_key key = {};
    if (item == nullptr)
    {
        return key;
    }

    key.pinned = item->pinned;
    key.pin_id = item->pin_id;
    key.window = item->window;
    return key;
}

static void reach_dock_set_order_key(reach_dock_feature_model *model, size_t index,
                                     const reach_dock_item_model *item)
{
    if (model == nullptr || item == nullptr || index >= REACH_MAX_PINNED_APPS)
    {
        return;
    }
    model->order[index] = reach_dock_order_key_for_item(item);
}

size_t reach_dock_find_pinned_for_window(const reach_pinned_app_model *pinned_apps,
                                         size_t pinned_app_count,
                                         const reach_window_snapshot *window,
                                         reach_dock_window_matches_pinned_fn window_matches_pinned,
                                         void *match_user)
{
    if (pinned_apps == nullptr || window == nullptr || window_matches_pinned == nullptr)
    {
        return REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < pinned_app_count; ++index)
    {
        if (window_matches_pinned(match_user, &pinned_apps[index], window))
        {
            return index;
        }
    }

    return REACH_MAX_PINNED_APPS;
}

static int32_t reach_dock_pinned_running(const reach_pinned_app_model *pinned_apps,
                                         size_t pinned_app_count,
                                         const reach_window_snapshot *open_windows,
                                         size_t open_window_count, size_t pinned_index,
                                         reach_dock_window_matches_pinned_fn window_matches_pinned,
                                         void *match_user, uintptr_t *out_window)
{
    if (out_window != nullptr)
    {
        *out_window = 0;
    }
    if (pinned_apps == nullptr || open_windows == nullptr || pinned_index >= pinned_app_count)
    {
        return 0;
    }

    for (size_t index = 0; index < open_window_count; ++index)
    {
        if (reach_dock_find_pinned_for_window(pinned_apps, pinned_app_count, &open_windows[index],
                                              window_matches_pinned, match_user) == pinned_index)
        {
            if (out_window != nullptr)
            {
                *out_window = open_windows[index].id;
            }
            return 1;
        }
    }
    return 0;
}

static int32_t reach_dock_append_item(reach_dock_item_model *items, size_t *item_count,
                                      const reach_dock_item_model *item)
{
    if (items == nullptr || item_count == nullptr || item == nullptr ||
        *item_count >= REACH_MAX_PINNED_APPS)
    {
        return 0;
    }

    items[*item_count] = *item;
    ++(*item_count);
    return 1;
}

static void reach_dock_append_pinned_items(
    reach_dock_item_model *items, size_t *item_count, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (items == nullptr || item_count == nullptr || pinned_apps == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < pinned_app_count && *item_count < REACH_MAX_PINNED_APPS; ++index)
    {
        uintptr_t window_id = 0;
        reach_dock_item_model item = {};
        item.pinned = 1;
        item.pin_id = pinned_apps[index].id;
        item.window = reach_dock_pinned_running(pinned_apps, pinned_app_count, open_windows,
                                                open_window_count, index, window_matches_pinned,
                                                match_user, &window_id)
                          ? window_id
                          : 0;
        item.pinned_index = index;
        item.open_index = REACH_MAX_PINNED_APPS;

        reach_dock_append_item(items, item_count, &item);
    }
}

static void reach_dock_append_unpinned_open_windows(
    reach_dock_item_model *items, size_t *item_count, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (items == nullptr || item_count == nullptr || open_windows == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < open_window_count && *item_count < REACH_MAX_PINNED_APPS;
         ++index)
    {
        if (reach_dock_find_pinned_for_window(pinned_apps, pinned_app_count, &open_windows[index],
                                              window_matches_pinned,
                                              match_user) != REACH_MAX_PINNED_APPS)
        {
            continue;
        }

        reach_dock_item_model item = {};
        item.pinned = 0;
        item.pin_id = 0;
        item.window = open_windows[index].id;
        item.pinned_index = REACH_MAX_PINNED_APPS;
        item.open_index = index;

        reach_dock_append_item(items, item_count, &item);
    }
}

static void reach_dock_build_candidate_items(
    reach_dock_item_model *items, size_t *item_count, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (item_count != nullptr)
    {
        *item_count = 0;
    }

    reach_dock_append_pinned_items(items, item_count, pinned_apps, pinned_app_count, open_windows,
                                   open_window_count, window_matches_pinned, match_user);

    reach_dock_append_unpinned_open_windows(items, item_count, pinned_apps, pinned_app_count,
                                            open_windows, open_window_count, window_matches_pinned,
                                            match_user);
}

static void reach_dock_apply_existing_order(reach_dock_feature_model *model,
                                            const reach_dock_item_model *candidates,
                                            size_t candidate_count, int32_t *used)
{
    if (model == nullptr || candidates == nullptr || used == nullptr)
    {
        return;
    }

    model->item_count = 0;

    for (size_t order_index = 0;
         order_index < model->order_count && model->item_count < REACH_MAX_PINNED_APPS;
         ++order_index)
    {
        for (size_t candidate_index = 0; candidate_index < candidate_count; ++candidate_index)
        {
            reach_dock_order_key candidate_key =
                reach_dock_order_key_for_item(&candidates[candidate_index]);
            if (!used[candidate_index] &&
                reach_dock_key_equal(&model->order[order_index], &candidate_key))
            {
                model->items[model->item_count++] = candidates[candidate_index];
                used[candidate_index] = 1;
                break;
            }
        }
    }
}

static void reach_dock_append_new_items(reach_dock_feature_model *model,
                                        const reach_dock_item_model *candidates,
                                        size_t candidate_count, const int32_t *used)
{
    if (model == nullptr || candidates == nullptr || used == nullptr)
    {
        return;
    }

    for (size_t candidate_index = 0;
         candidate_index < candidate_count && model->item_count < REACH_MAX_PINNED_APPS;
         ++candidate_index)
    {
        if (!used[candidate_index])
        {
            model->items[model->item_count++] = candidates[candidate_index];
        }
    }
}

static void reach_dock_store_current_order(reach_dock_feature_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    model->order_count = model->item_count;
    for (size_t index = 0; index < model->item_count; ++index)
    {
        reach_dock_set_order_key(model, index, &model->items[index]);
    }
}

void reach_dock_feature_model_build_items(
    reach_dock_feature_model *model, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (model == nullptr)
    {
        return;
    }

    reach_dock_item_model candidates[REACH_MAX_PINNED_APPS] = {};
    int32_t used[REACH_MAX_PINNED_APPS] = {};
    size_t candidate_count = 0;

    reach_dock_build_candidate_items(candidates, &candidate_count, pinned_apps, pinned_app_count,
                                     open_windows, open_window_count, window_matches_pinned,
                                     match_user);

    reach_dock_apply_existing_order(model, candidates, candidate_count, used);
    reach_dock_append_new_items(model, candidates, candidate_count, used);
    reach_dock_store_current_order(model);
}

size_t reach_dock_item_count(reach_dock *dock)
{
    return dock != nullptr ? reach_dock_state_mut(dock)->model.item_count : 0;
}

const reach_dock_item_model *reach_dock_item_at(reach_dock *dock, size_t index)
{
    if (dock == nullptr || index >= reach_dock_state_mut(dock)->model.item_count)
    {
        return nullptr;
    }
    return &reach_dock_state_mut(dock)->model.items[index];
}

size_t reach_dock_build_item_context_commands(reach_dock *dock, size_t item_index,
                                              uint32_t *out_commands, size_t cap)
{
    if (dock == nullptr || out_commands == nullptr ||
        item_index >= dock->state.model.item_count)
    {
        return 0;
    }
    const reach_dock_item_model *item = &dock->state.model.items[item_index];

    const uint16_t *path = nullptr;
    if (item->pinned)
    {
        path = item->pinned_index < dock->pointer_pinned_app_count
                   ? dock->pointer_pinned_apps[item->pinned_index].path
                   : nullptr;
    }
    else if (dock->windows != nullptr)
    {
        const reach_window_snapshot *windows = reach_window_tracking_windows(dock->windows);
        size_t window_count = reach_window_tracking_window_count(dock->windows);
        path = windows != nullptr && item->open_index < window_count
                   ? windows[item->open_index].path
                   : nullptr;
    }
    const int32_t has_path = path != nullptr;
    const int32_t has_window = item->window != 0;

    size_t count = 0;
    if (has_path && count < cap)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_OPEN_NEW;
    }
    if (has_path && count < cap)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN;
    }
    if (item->pinned && count < cap)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_UNPIN;
    }
    if (!item->pinned && has_path && count < cap)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_PIN;
    }
    if (has_window && count < cap)
    {
        out_commands[count++] = REACH_CONTEXT_MENU_COMMAND_CLOSE;
    }
    return count;
}

size_t reach_dock_order_count(reach_dock *dock)
{
    return dock != nullptr ? reach_dock_state_mut(dock)->model.order_count : 0;
}

reach_dock_order_key reach_dock_order_key_at(reach_dock *dock, size_t index)
{
    reach_dock_order_key key = {};
    if (dock == nullptr || index >= reach_dock_state_mut(dock)->model.order_count)
    {
        return key;
    }
    return reach_dock_state_mut(dock)->model.order[index];
}

void reach_dock_restore_order(reach_dock *dock, const reach_dock_order_key *keys, size_t count)
{
    if (dock == nullptr || keys == nullptr || count > REACH_MAX_PINNED_APPS)
    {
        return;
    }
    reach_dock_state *state = reach_dock_state_mut(dock);
    state->model.order_count = count;
    for (size_t index = 0; index < count; ++index)
    {
        state->model.order[index] = keys[index];
    }
}

static size_t reach_dock_slot_track(size_t pool_index)
{
    return REACH_DOCK_ANIM_SLOT_BASE + pool_index;
}

static float reach_dock_slot_width(const reach_dock *dock, size_t pool_index)
{
    return reach_animation_manager_value(&dock->manager, reach_dock_slot_track(pool_index));
}

static void reach_dock_gate_animating_hit(reach_dock *dock, reach_dock_hit_result *hit)
{
    if (hit->type == REACH_DOCK_HIT_ITEM && reach_dock_slots_animating(dock))
    {
        hit->type = REACH_DOCK_HIT_NONE;
        hit->index = REACH_MAX_PINNED_APPS;
    }
}

int32_t reach_dock_slots_animating(const reach_dock *dock)
{
    if (dock == nullptr)
    {
        return 0;
    }
    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        if (dock->slots[pool].lifecycle != REACH_DOCK_SLOT_EMPTY &&
            reach_animation_manager_active(&dock->manager, reach_dock_slot_track(pool)))
        {
            return 1;
        }
    }
    return 0;
}

static void reach_dock_slot_order_remove(reach_dock *dock, size_t pool_index)
{
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        if (dock->slot_order[at] == pool_index)
        {
            for (size_t rest = at + 1; rest < dock->slot_order_count; ++rest)
            {
                dock->slot_order[rest - 1] = dock->slot_order[rest];
            }
            --dock->slot_order_count;
            return;
        }
    }
}

static void reach_dock_slot_free(reach_dock *dock, size_t pool_index)
{
    dock->slots[pool_index] = {};
    reach_animation_manager_reset(&dock->manager, reach_dock_slot_track(pool_index));
    reach_dock_slot_order_remove(dock, pool_index);
}

static size_t reach_dock_slot_alloc(reach_dock *dock)
{
    for (size_t pool = 1; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        if (dock->slots[pool].lifecycle == REACH_DOCK_SLOT_EMPTY)
        {
            return pool;
        }
    }
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        size_t pool = dock->slot_order[at];
        if (pool != 0 && dock->slots[pool].lifecycle == REACH_DOCK_SLOT_DYING)
        {
            reach_dock_slot_free(dock, pool);
            return pool;
        }
    }
    REACH_ASSERT(0 && "dock slot pool exhausted");
    return REACH_DOCK_SLOT_CAPACITY;
}

static size_t reach_dock_slot_find(reach_dock *dock, reach_dock_order_key key)
{
    for (size_t pool = 1; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        if (dock->slots[pool].lifecycle != REACH_DOCK_SLOT_EMPTY &&
            reach_dock_key_equal(&dock->slots[pool].key, &key))
        {
            return pool;
        }
    }
    return REACH_DOCK_SLOT_CAPACITY;
}

static void reach_dock_settle_slots(reach_dock *dock)
{
    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        reach_dock_slot *slot = &dock->slots[pool];
        if (slot->lifecycle == REACH_DOCK_SLOT_EMPTY ||
            reach_animation_manager_active(&dock->manager, reach_dock_slot_track(pool)))
        {
            continue;
        }
        if (slot->lifecycle == REACH_DOCK_SLOT_APPEARING)
        {
            slot->lifecycle = REACH_DOCK_SLOT_STEADY;
        }
        else if (slot->lifecycle == REACH_DOCK_SLOT_DYING && pool != 0)
        {
            reach_dock_slot_free(dock, pool);
        }
    }
}

static void reach_dock_sync_slots(reach_dock *dock, float app_slot_width, float np_slot_width)
{
    reach_dock_state *state = &dock->state;
    size_t item_count = state->model.item_count;
    if (item_count > REACH_MAX_PINNED_APPS)
    {
        item_count = REACH_MAX_PINNED_APPS;
    }

    if (!dock->slots_synced)
    {
        for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
        {
            dock->slots[pool] = {};
            reach_animation_manager_reset(&dock->manager, reach_dock_slot_track(pool));
        }
        dock->slot_order_count = 0;
        dock->slots[0].lifecycle = REACH_DOCK_SLOT_STEADY;
        dock->slots[0].target_width = np_slot_width;
        reach_animation_manager_set(&dock->manager, reach_dock_slot_track(0), np_slot_width);
        reach_animation_manager_set(&dock->manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT,
                                    np_slot_width > 0.0f ? 1.0f : 0.0f);
        dock->np_content_armed = 0;
        dock->slot_order[dock->slot_order_count++] = 0;
        for (size_t index = 0; index < item_count; ++index)
        {
            size_t pool = index + 1;
            dock->slots[pool].lifecycle = REACH_DOCK_SLOT_STEADY;
            dock->slots[pool].key = {state->model.items[index].pinned,
                                     reach_dock_feature_model_item_pin_id(&state->model, index),
                                     state->model.items[index].window};
            dock->slots[pool].target_width = app_slot_width;
            reach_animation_manager_set(&dock->manager, reach_dock_slot_track(pool),
                                        app_slot_width);
            dock->slot_order[dock->slot_order_count++] = (uint8_t)pool;
        }
        dock->slots_synced = 1;
        return;
    }

    if (dock->slots[0].target_width != np_slot_width)
    {
        dock->slots[0].target_width = np_slot_width;
        reach_animation_manager_animate_to(&dock->manager, reach_dock_slot_track(0),
                                           np_slot_width, REACH_DOCK_SLOT_ANIMATION_SECONDS,
                                           REACH_EASING_EASE_IN_OUT);
        if (np_slot_width <= 0.0f)
        {
            reach_animation_manager_set(&dock->manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT,
                                        0.0f);
        }
        dock->np_content_armed = 0;
    }

    for (size_t pool = 1; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        reach_dock_slot *slot = &dock->slots[pool];
        if (slot->lifecycle != REACH_DOCK_SLOT_APPEARING &&
            slot->lifecycle != REACH_DOCK_SLOT_STEADY)
        {
            continue;
        }
        int32_t found = 0;
        for (size_t index = 0; index < item_count; ++index)
        {
            reach_dock_order_key item_key = {
                state->model.items[index].pinned,
                reach_dock_feature_model_item_pin_id(&state->model, index),
                state->model.items[index].window};
            if (reach_dock_key_equal(&slot->key, &item_key))
            {
                found = 1;
                break;
            }
        }
        if (!found)
        {
            slot->lifecycle = REACH_DOCK_SLOT_DYING;
            reach_animation_manager_animate_to(&dock->manager, reach_dock_slot_track(pool), 0.0f,
                                               REACH_DOCK_SLOT_ANIMATION_SECONDS,
                                               REACH_EASING_EASE_IN_OUT);
        }
    }

    uint8_t dying_anchor[REACH_DOCK_SLOT_CAPACITY] = {};
    size_t last_live = 0;
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        size_t pool = dock->slot_order[at];
        if (dock->slots[pool].lifecycle == REACH_DOCK_SLOT_DYING && pool != 0)
        {
            dying_anchor[pool] = (uint8_t)last_live;
        }
        else if (dock->slots[pool].lifecycle != REACH_DOCK_SLOT_EMPTY)
        {
            last_live = pool;
        }
    }

    uint8_t new_order[REACH_DOCK_SLOT_CAPACITY] = {};
    size_t new_count = 0;
    new_order[new_count++] = 0;
    for (size_t pool = 1; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        if (dock->slots[pool].lifecycle == REACH_DOCK_SLOT_DYING && dying_anchor[pool] == 0)
        {
            new_order[new_count++] = (uint8_t)pool;
        }
    }
    for (size_t index = 0; index < item_count; ++index)
    {
        reach_dock_order_key item_key = {state->model.items[index].pinned,
                                         reach_dock_feature_model_item_pin_id(&state->model, index),
                                         state->model.items[index].window};
        size_t pool = reach_dock_slot_find(dock, item_key);
        if (pool < REACH_DOCK_SLOT_CAPACITY)
        {
            reach_dock_slot *slot = &dock->slots[pool];
            slot->key = item_key;
            if (slot->lifecycle == REACH_DOCK_SLOT_DYING)
            {

                slot->lifecycle = REACH_DOCK_SLOT_APPEARING;
            }
            if (slot->target_width != app_slot_width || slot->lifecycle != REACH_DOCK_SLOT_STEADY)
            {
                slot->target_width = app_slot_width;
                if (slot->lifecycle == REACH_DOCK_SLOT_STEADY)
                {

                    reach_animation_manager_set(&dock->manager, reach_dock_slot_track(pool),
                                                app_slot_width);
                }
                else if (reach_animation_manager_target(&dock->manager,
                                                        reach_dock_slot_track(pool)) !=
                         app_slot_width)
                {
                    reach_animation_manager_animate_to(&dock->manager,
                                                       reach_dock_slot_track(pool),
                                                       app_slot_width,
                                                       REACH_DOCK_SLOT_ANIMATION_SECONDS,
                                                       REACH_EASING_EASE_IN_OUT);
                }
            }
        }
        else
        {
            pool = reach_dock_slot_alloc(dock);
            if (pool >= REACH_DOCK_SLOT_CAPACITY)
            {
                continue;
            }
            dock->slots[pool].lifecycle = REACH_DOCK_SLOT_APPEARING;
            dock->slots[pool].key = item_key;
            dock->slots[pool].target_width = app_slot_width;
            reach_animation_manager_start(&dock->manager, reach_dock_slot_track(pool), 0.0f,
                                          app_slot_width, REACH_DOCK_SLOT_ANIMATION_SECONDS,
                                          REACH_EASING_EASE_IN_OUT);
        }
        new_order[new_count++] = (uint8_t)pool;
        for (size_t dying = 1; dying < REACH_DOCK_SLOT_CAPACITY; ++dying)
        {
            if (dock->slots[dying].lifecycle == REACH_DOCK_SLOT_DYING &&
                dying_anchor[dying] == pool && dying != pool)
            {
                new_order[new_count++] = (uint8_t)dying;
            }
        }
    }

    for (size_t pool = 1; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        if (dock->slots[pool].lifecycle != REACH_DOCK_SLOT_DYING)
        {
            continue;
        }
        int32_t present = 0;
        for (size_t at = 0; at < new_count; ++at)
        {
            if (new_order[at] == pool)
            {
                present = 1;
                break;
            }
        }
        if (!present)
        {
            new_order[new_count++] = (uint8_t)pool;
        }
    }
    for (size_t at = 0; at < new_count; ++at)
    {
        dock->slot_order[at] = new_order[at];
    }
    dock->slot_order_count = new_count;
}

static void reach_dock_snap_slots(reach_dock *dock)
{
    reach_animation_manager_set(&dock->manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT,
                                dock->slots[0].target_width > 0.0f ? 1.0f : 0.0f);
    dock->np_content_armed = 0;
    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        reach_dock_slot *slot = &dock->slots[pool];
        if (slot->lifecycle == REACH_DOCK_SLOT_EMPTY)
        {
            continue;
        }
        if (slot->lifecycle == REACH_DOCK_SLOT_DYING && pool != 0)
        {
            reach_dock_slot_free(dock, pool);
            continue;
        }
        slot->lifecycle = REACH_DOCK_SLOT_STEADY;
        reach_animation_manager_set(&dock->manager, reach_dock_slot_track(pool),
                                    slot->target_width);
    }
}

float reach_dock_item_reveal(reach_dock *dock, size_t item_index)
{
    if (dock == nullptr || item_index >= dock->state.model.item_count)
    {
        return 0.0f;
    }
    reach_dock_order_key key = {
        dock->state.model.items[item_index].pinned,
        reach_dock_feature_model_item_pin_id(&dock->state.model, item_index),
        dock->state.model.items[item_index].window};
    size_t pool = reach_dock_slot_find(dock, key);
    if (pool >= REACH_DOCK_SLOT_CAPACITY)
    {
        return 1.0f;
    }
    const reach_dock_slot *slot = &dock->slots[pool];
    if (slot->lifecycle == REACH_DOCK_SLOT_STEADY)
    {
        return 1.0f;
    }
    if (slot->target_width <= 0.0f)
    {
        return 0.0f;
    }
    const float progress = reach_dock_slot_width(dock, pool) / slot->target_width;
    if (progress <= REACH_DOCK_SLOT_REVEAL_THRESHOLD)
    {
        return 0.0f;
    }
    float reveal = (progress - REACH_DOCK_SLOT_REVEAL_THRESHOLD) /
                   (1.0f - REACH_DOCK_SLOT_REVEAL_THRESHOLD);
    return reveal > 1.0f ? 1.0f : reveal;
}

float reach_dock_now_playing_reveal_width(reach_dock *dock, float scaled_gap)
{
    if (dock == nullptr)
    {
        return 0.0f;
    }
    const float width = reach_dock_slot_width(dock, 0) - scaled_gap;
    return width > 0.0f ? width : 0.0f;
}

void reach_dock_build_layout(reach_dock *dock, const reach_dock_build_context *ctx,
                             reach_dock_layout *layout)
{
    if (dock == nullptr || ctx == nullptr || layout == nullptr || ctx->theme == nullptr)
    {
        return;
    }

    dock->pointer_theme = ctx->theme;
    dock->pointer_pinned_apps = ctx->pinned_apps;
    dock->pointer_pinned_app_count = ctx->pinned_app_count;

    reach_dock_build_items(dock, ctx->pinned_apps, ctx->pinned_app_count);

    layout->app_slot_count = dock->state.model.item_count;
    const float scale = ctx->dpi_scale;
    const float icon_size = ctx->icon_size * scale;
    const float gap = ctx->gap * scale;
    const size_t count = dock->state.model.item_count;
    const reach_theme *theme = ctx->theme;
    const float clock_width = theme->dock_clock_width * scale;
    const float separator_width = theme->dock_system_separator_width * scale;
    const float separator_height =
        layout->bounds.height * theme->dock_system_separator_height_ratio;
    const float now_playing_height =
        reach_theme_now_playing_height(theme, layout->bounds.height);
    const float now_playing_render_width = reach_dock_now_playing_desired_width(
        dock->now_playing_subfeature, theme, scale);
    const float now_playing_reserved_width =
        reach_dock_now_playing_visible(dock->now_playing_subfeature) ? now_playing_render_width
                                                                    : 0.0f;

    const float app_slot_width = icon_size + gap;
    const float np_slot_width =
        now_playing_reserved_width > 0.0f ? ceilf(now_playing_reserved_width) + gap : 0.0f;
    reach_dock_sync_slots(dock, app_slot_width, np_slot_width);

    const float now_playing_left = theme->now_playing_left_margin * scale;
    const float top = (layout->bounds.height - icon_size) * 0.5f;

    float x = gap;
    layout->now_playing = {};
    const float np_width_now = reach_dock_slot_width(dock, 0);
    const float np_content =
        reach_animation_manager_value(&dock->manager, REACH_DOCK_ANIM_NOW_PLAYING_CONTENT);
    if (now_playing_render_width > 0.0f && np_width_now > 0.0f && np_content > 0.0f)
    {
        layout->now_playing.x = now_playing_left;
        layout->now_playing.y = (layout->bounds.height - now_playing_height) * 0.5f;
        layout->now_playing.width = now_playing_render_width;
        layout->now_playing.height = now_playing_height;
        if (np_content < 1.0f)
        {

            const float inset_x = layout->now_playing.width * (1.0f - np_content) * 0.5f;
            const float inset_y = layout->now_playing.height * (1.0f - np_content) * 0.5f;
            layout->now_playing.x += inset_x;
            layout->now_playing.y += inset_y;
            layout->now_playing.width -= inset_x * 2.0f;
            layout->now_playing.height -= inset_y * 2.0f;
        }
    }
    x += np_width_now;

    size_t item_index = 0;
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        size_t pool = dock->slot_order[at];
        if (pool == 0)
        {
            continue;
        }
        const reach_dock_slot *slot = &dock->slots[pool];
        if (slot->lifecycle == REACH_DOCK_SLOT_APPEARING ||
            slot->lifecycle == REACH_DOCK_SLOT_STEADY)
        {
            if (item_index < layout->app_slot_count)
            {
                layout->app_slots[item_index].x = x;
                layout->app_slots[item_index].y = top;
                layout->app_slots[item_index].width = icon_size;
                layout->app_slots[item_index].height = icon_size;
                ++item_index;
            }
        }
        x += reach_dock_slot_width(dock, pool);
    }

    for (; item_index < layout->app_slot_count; ++item_index)
    {
        layout->app_slots[item_index].x = x;
        layout->app_slots[item_index].y = top;
        layout->app_slots[item_index].width = icon_size;
        layout->app_slots[item_index].height = icon_size;
        x += app_slot_width;
    }

    layout->tray_button.width = icon_size;
    layout->tray_button.height = icon_size;
    layout->tray_button.x = x;
    layout->tray_button.y = top;

    layout->quick_settings_button.width = icon_size;
    layout->quick_settings_button.height = icon_size;
    layout->quick_settings_button.x = layout->tray_button.x + icon_size;
    layout->quick_settings_button.y = top;

    layout->system_separator.width = separator_width;
    layout->system_separator.height = separator_height;
    layout->system_separator.x = layout->quick_settings_button.x + icon_size + gap;
    layout->system_separator.y = (layout->bounds.height - separator_height) * 0.5f;

    layout->clock.width = clock_width;
    layout->clock.height = icon_size;
    layout->clock.x = layout->system_separator.x + separator_width + gap;
    layout->clock.y = top;

    layout->power_button.width = icon_size;
    layout->power_button.height = icon_size;
    layout->power_button.x = layout->clock.x + clock_width + gap;
    layout->power_button.y = top;

    const float dock_width = ceilf(layout->power_button.x + icon_size + gap);
    const float old_width = layout->bounds.width;
    if (dock_width != old_width)
    {
        layout->bounds.x += (old_width - dock_width) * 0.5f;
        layout->bounds.width = dock_width;
    }

    reach_dock_now_playing_relayout(dock->now_playing_subfeature, theme, layout->now_playing,
                                    scale);
    dock->pointer_layout = *layout;
    dock->pointer_layout_valid = 1;
}

reach_point_i32 reach_dock_local_point(const reach_dock_layout *layout, int32_t x, int32_t y)
{
    reach_point_i32 point = {};
    if (layout == nullptr)
    {
        point.x = x;
        point.y = y;
        return point;
    }
    point.x = static_cast<int32_t>((float)x - layout->bounds.x);
    point.y = static_cast<int32_t>((float)y - layout->bounds.y);
    return point;
}

reach_rect_f32 reach_dock_rect_to_screen(const reach_dock_layout *layout, reach_rect_f32 rect)
{
    if (layout == nullptr)
    {
        return rect;
    }
    rect.x += layout->bounds.x;
    rect.y += layout->bounds.y;
    return rect;
}

reach_dock_layout reach_dock_layout_to_screen(reach_dock_layout layout)
{
    layout.now_playing = reach_dock_rect_to_screen(&layout, layout.now_playing);
    for (size_t index = 0; index < layout.app_slot_count; ++index)
    {
        layout.app_slots[index] = reach_dock_rect_to_screen(&layout, layout.app_slots[index]);
    }
    layout.tray_button = reach_dock_rect_to_screen(&layout, layout.tray_button);
    layout.quick_settings_button = reach_dock_rect_to_screen(&layout, layout.quick_settings_button);
    layout.system_separator = reach_dock_rect_to_screen(&layout, layout.system_separator);
    layout.clock = reach_dock_rect_to_screen(&layout, layout.clock);
    layout.power_button = reach_dock_rect_to_screen(&layout, layout.power_button);
    return layout;
}

void reach_dock_rebuild_items(reach_dock *dock, const reach_dock_build_context *ctx,
                              const reach_dock_layout *old_layout, reach_dock_layout *out_layout)
{
    if (dock == nullptr || ctx == nullptr || out_layout == nullptr)
    {
        return;
    }
    reach_dock_item_x_snapshot snapshot = {};
    reach_dock_item_x_snapshot_take(dock, ctx->theme,
                                    old_layout != nullptr ? old_layout : out_layout, &snapshot);
    reach_dock_build_layout(dock, ctx, out_layout);
    reach_dock_item_x_rebind(dock, ctx->theme, out_layout, &snapshot);
}

static void reach_dock_start_item_x_animation(reach_dock *dock, size_t index, float from, float to)
{
    if (dock == nullptr || index >= REACH_MAX_PINNED_APPS)
    {
        return;
    }
    const float offset = from - to;
    if (fabsf(offset) < 0.5f)
    {
        reach_animation_manager_set(&dock->manager, reach_dock_item_animation_id(index), 0.0f);
        dock->state.item_x_valid[index] = 1;
        return;
    }
    reach_animation_manager_start(&dock->manager, reach_dock_item_animation_id(index), offset,
                                  0.0f, 0.15, REACH_EASING_EASE_IN_OUT);
    dock->state.item_x_valid[index] = 1;
}

void reach_dock_clear_item_x_animations(reach_dock *dock)
{
    if (dock == nullptr)
    {
        return;
    }
    reach_dock_snap_slots(dock);
    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index)
    {
        reach_animation_manager_reset(&dock->manager, reach_dock_item_animation_id(index));
        dock->state.item_x_valid[index] = 0;
        dock->state.item_x_pinned[index] = 0;
        dock->state.item_x_pin_ids[index] = 0;
        dock->state.item_x_windows[index] = 0;
    }
}

void reach_dock_item_x_snapshot_take(reach_dock *dock, const reach_theme *theme,
                                     const reach_dock_layout *old_layout,
                                     reach_dock_item_x_snapshot *out_snapshot)
{
    if (out_snapshot != nullptr)
    {
        *out_snapshot = {};
    }
    if (dock == nullptr || out_snapshot == nullptr)
    {
        return;
    }
    reach_dock_state *state = &dock->state;
    size_t count = state->model.item_count;
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }
    out_snapshot->count = count;
    for (size_t index = 0; index < count; ++index)
    {
        out_snapshot->pinned[index] = state->model.items[index].pinned;
        out_snapshot->pin_ids[index] =
            reach_dock_feature_model_item_pin_id(&state->model, index);
        out_snapshot->windows[index] = state->model.items[index].window;
        out_snapshot->x[index] = reach_dock_item_current_x(dock, theme, old_layout, index);
    }
}

void reach_dock_item_x_rebind(reach_dock *dock, const reach_theme *theme,
                              const reach_dock_layout *layout,
                              const reach_dock_item_x_snapshot *snapshot)
{
    if (dock == nullptr || layout == nullptr || snapshot == nullptr)
    {
        return;
    }
    reach_dock_state *state = &dock->state;

    for (size_t index = 0; index < state->model.item_count; ++index)
    {
        uint32_t pin_id = reach_dock_feature_model_item_pin_id(&state->model, index);
        float target_x = reach_dock_slot_box_x(theme, layout, index);
        float from_x = target_x;
        reach_dock_order_key item_key = {state->model.items[index].pinned, pin_id,
                                         state->model.items[index].window};
        for (size_t old_index = 0; old_index < snapshot->count; ++old_index)
        {
            reach_dock_order_key old_key = {snapshot->pinned[old_index],
                                            snapshot->pin_ids[old_index],
                                            snapshot->windows[old_index]};
            if (reach_dock_key_equal(&old_key, &item_key))
            {
                from_x = snapshot->x[old_index];
                break;
            }
        }
        state->item_x_pinned[index] = state->model.items[index].pinned;
        state->item_x_pin_ids[index] = pin_id;
        state->item_x_windows[index] = state->model.items[index].window;
        reach_dock_order_key drag_key = {state->drag.pinned, state->drag.pin_id,
                                         state->drag.window};
        if (reach_dock_key_equal(&drag_key, &item_key) &&
            (state->drag.active ||
             reach_animation_manager_active(&dock->manager, REACH_DOCK_ANIM_DRAG_SNAP)))
        {
            reach_dock_start_item_x_animation(dock, index, target_x, target_x);
        }
        else
        {
            reach_dock_start_item_x_animation(dock, index, from_x, target_x);
        }
    }
    for (size_t index = state->model.item_count; index < REACH_MAX_PINNED_APPS; ++index)
    {
        state->item_x_valid[index] = 0;
        reach_animation_manager_reset(&dock->manager, reach_dock_item_animation_id(index));
        state->item_x_pinned[index] = 0;
        state->item_x_pin_ids[index] = 0;
        state->item_x_windows[index] = 0;
    }
}
