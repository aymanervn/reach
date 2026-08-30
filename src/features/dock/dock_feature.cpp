#include "reach/features/dock.h"

#include "dock_common_state.h"
#include "dock_interaction.h"

#include <math.h>
#include <new>
#include <stdio.h>
#include <time.h>

static const reach_bar_edge REACH_DOCK_EDGE = REACH_BAR_EDGE_BOTTOM;

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
    uint32_t key;
};

static const double REACH_DOCK_SLOT_ANIMATION_SECONDS = 0.25;
static const float REACH_DOCK_SLOT_REVEAL_THRESHOLD = 0.7f;
static const float REACH_DOCK_OUTER_PADDING_SCALE = 1.15f;

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
    uint16_t slot_order[REACH_DOCK_SLOT_CAPACITY];
    size_t slot_order_count;
    int32_t slots_synced;

    reach_icon_service *icons;
    reach_window_tracking *windows;
    reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
    size_t pinned_app_count;
    reach_menu_request action_request;
    reach_menu_request hover_request;
    reach_dock_model metrics;
    uint32_t next_app_key;
    reach_dock_routes routes;
    const reach_theme *pointer_theme;
    reach_dock_layout pointer_layout;
    int32_t pointer_layout_valid;
    const reach_pinned_app_model *pointer_pinned_apps;
    size_t pointer_pinned_app_count;
    reach_rect_f32 coverage_shown_bounds;
    reach_rect_f32 coverage_monitor_bounds;
    float coverage_shadow_clearance;
    int32_t coverage_valid;
    int32_t coverage_trespassed;
};

void reach_dock_set_routes(reach_dock *dock, const reach_dock_routes *routes)
{
    if (dock != nullptr)
    {
        dock->routes = routes != nullptr ? *routes : reach_dock_routes{};
    }
}

void reach_dock_attach_services(reach_dock *dock, reach_icon_service *icons,
                                reach_window_tracking *windows)
{
    if (dock != nullptr)
    {
        dock->icons = icons;
        dock->windows = windows;
    }
}

reach_icon_service *reach_dock_icons(reach_dock *dock)
{
    return dock != nullptr ? dock->icons : nullptr;
}

reach_window_tracking *reach_dock_windows(reach_dock *dock)
{
    return dock != nullptr ? dock->windows : nullptr;
}

void reach_dock_touch_icons(reach_dock *dock, int32_t icon_size_px)
{
    if (dock == nullptr || dock->icons == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < dock->state.model.item_count; ++index)
    {
        const reach_dock_item_model *item = &dock->state.model.items[index];
        const uint16_t *icon_path = item->icon_ref;
        if (icon_path != nullptr && icon_path[0] != 0)
        {
            reach_icon_service_touch(dock->icons, icon_path, icon_size_px);
        }
    }
}

const reach_dock_state *reach_dock_state_ptr(reach_dock *animations)
{
    return animations != nullptr ? &animations->state : nullptr;
}

reach_dock_state *reach_dock_state_mut(reach_dock *animations)
{
    return animations != nullptr ? &animations->state : nullptr;
}

static reach_pressable_feedback_style reach_dock_pressable_feedback(reach_dock *dock)
{
    reach_pressable_feedback_style feedback = {};
    if (dock != nullptr)
    {
        feedback.animations = &dock->manager;
        feedback.track = REACH_DOCK_ANIM_FEEDBACK_OPACITY;
        feedback.pressed_value = 0.50f;
        feedback.press_seconds = 0.055;
        feedback.release_seconds = 0.055;
        feedback.press_easing = REACH_EASING_EASE_IN_OUT;
        feedback.release_easing = REACH_EASING_EASE_IN_OUT;
    }
    return feedback;
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
    reach_dock_model_defaults(&animations->metrics);
    reach_animation_manager_init(&animations->manager, animations->tracks, REACH_DOCK_ANIM_COUNT);
    reach_draggable_init(&animations->state.drag.gesture);
    animations->state.drag.target_index = REACH_MAX_DOCK_ITEMS;
    animations->state.hovered_item = REACH_MAX_DOCK_ITEMS;
    reach_pressable_init(&animations->state.pressable);
    *out_animations = animations;
    return REACH_OK;
}

void reach_dock_destroy(reach_dock *animations)
{
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

    int32_t feedback_was_active =
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY);
    int32_t drag_snap_was_active =
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP);
    int32_t item_was_active[REACH_MAX_DOCK_ITEMS] = {};
    for (size_t index = 0; index < state->model.item_count; ++index)
    {
        item_was_active[index] =
            reach_animation_manager_active(manager, reach_dock_item_animation_id(index));
    }

    int32_t slots_were_animating = reach_dock_slots_animating(animations);

    reach_animation_manager_tick(manager, delta_seconds);

    int32_t redraw = 0;
    if (feedback_was_active ||
        reach_animation_manager_active(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY))
    {
        redraw = 1;
    }

    reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(animations);
    reach_pressable_settle_feedback(&state->pressable, &feedback);

    for (size_t index = 0; index < state->model.item_count; ++index)
    {
        if (item_was_active[index] ||
            reach_animation_manager_active(manager, reach_dock_item_animation_id(index)))
        {
            redraw = 1;
        }
    }

    if (drag_snap_was_active || reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP))
    {
        state->drag.x = reach_animation_manager_value(manager, REACH_DOCK_ANIM_DRAG_SNAP);
        redraw = 1;
    }

    if (drag_snap_was_active && !reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP))
    {
        state->drag.target_index = REACH_MAX_DOCK_ITEMS;
        state->drag.key = {};
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
    case REACH_DOCK_HIT_TRIGGER:
        return REACH_DOCK_POINTER_REGION_TRIGGER;
    case REACH_DOCK_HIT_NONE:
    default:
        return REACH_DOCK_POINTER_REGION_NONE;
    }
}

static void reach_dock_bar_begin_session(void *capsule)
{
    reach_dock *dock = static_cast<reach_dock *>(capsule);
    if (dock != nullptr)
    {
        reach_bar_begin_reveal_session(&dock->state.visibility);
    }
}

static void reach_dock_reset_reveal_state(reach_dock *dock)
{
    if (dock == nullptr)
    {
        return;
    }
    reach_bar_visibility_reset(&dock->state.visibility);
    reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
    reach_pressable_reset(&dock->state.pressable, &feedback);
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

static void reach_dock_build_items(reach_dock *dock, const reach_pinned_app_model *pinned_apps,
                                   size_t pinned_app_count)
{
    if (dock == nullptr)
    {
        return;
    }
    reach_dock_feature_model_build_items(&dock->state.model, &dock->next_app_key, pinned_apps,
                                         pinned_app_count,
                                         reach_window_tracking_windows(dock->windows),
                                         reach_window_tracking_window_group_ids(dock->windows),
                                         reach_window_tracking_window_count(dock->windows),
                                         reach_dock_window_matches_app_thunk, nullptr);
}

size_t reach_dock_collect_item_windows(reach_dock *dock, size_t item_index,
                                       const reach_pinned_app_model *pinned_apps,
                                       size_t pinned_app_count, reach_dock_item_window *out,
                                       size_t cap)
{
    (void)pinned_apps;
    (void)pinned_app_count;
    if (dock == nullptr || out == nullptr || cap == 0 || item_index >= dock->state.model.item_count)
    {
        return 0;
    }

    const reach_dock_item_model *item = &dock->state.model.items[item_index];
    size_t count = 0;
    for (size_t index = 0; index < item->instance_count && count < cap; ++index)
    {
        const reach_window_snapshot *window =
            reach_window_tracking_window_by_id(dock->windows, item->instances[index]);
        out[count].window = item->instances[index];
        out[count].title = window != nullptr ? window->title : nullptr;
        ++count;
    }
    return count;
}

static void reach_dock_capsule_reset(void *capsule)
{
    reach_dock *dock = static_cast<reach_dock *>(capsule);
    reach_dock_reset_model(dock);
    if (dock != nullptr)
    {
        dock->pointer_layout_valid = 0;
        dock->state.hovered_item = REACH_MAX_DOCK_ITEMS;
        reach_draggable_init(&dock->state.drag.gesture);
        dock->state.drag.target_index = REACH_MAX_DOCK_ITEMS;
        dock->state.drag.key = {};
        reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
        reach_pressable_reset(&dock->state.pressable, &feedback);

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
        reach_dock_clear_item_x_animations(static_cast<reach_dock *>(capsule));
    }
}

static int32_t reach_dock_capsule_needs_frame(const void *capsule)
{
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    return dock != nullptr && (reach_animation_manager_any_active(&dock->manager) ||
                               reach_draggable_tracking(&dock->state.drag.gesture));
}

static int32_t reach_dock_capsule_pointer_sequence_active(const void *capsule)
{
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    return dock != nullptr && reach_pressable_tracking(&dock->state.pressable);
}

static int32_t reach_dock_capsule_wants_pointer_move(const void *capsule)
{
    return reach_dock_capsule_pointer_sequence_active(capsule);
}

static int32_t reach_dock_capsule_pointer_capture_active(const void *capsule)
{
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    return dock != nullptr && reach_pressable_tracking(&dock->state.pressable);
}

static void reach_dock_capsule_surface_geometry(const void *capsule,
                                                reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    if (dock != nullptr && dock->pointer_layout_valid)
    {
        out->visible_bounds = dock->pointer_layout.bounds;
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

/* Hovering an item offers the same complete request a right-click does; an item with no
   running windows offers nothing at all. */
static void reach_dock_notify_item_hovered(reach_dock *dock, size_t item_index)
{
    if (dock == nullptr || dock->routes.item_hovered == nullptr)
    {
        return;
    }
    if (!reach_dock_build_menu_request(dock, item_index, 0.0f, 0.0f, &dock->hover_request) ||
        dock->hover_request.window_count == 0)
    {
        dock->routes.item_hovered(dock->routes.user, nullptr);
        return;
    }
    dock->routes.item_hovered(dock->routes.user, &dock->hover_request);
}

static void
reach_dock_capsule_apply_interaction_result(reach_dock *dock,
                                            const reach_dock_interaction_result *interaction,
                                            reach_capsule_pointer_result *out)
{
    if (dock == nullptr || interaction == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw = out->redraw || interaction->redraw;
    if (interaction->rebuild_items)
    {
        reach_dock_mark_items_changed(dock);
        out->relayout = 1;
    }
    if (interaction->move_pin)
    {
        out->action.kind = REACH_FEATURE_ACTION_MOVE_PIN;
        out->action.id = interaction->move_pin_id;
        out->action.index = interaction->move_pin_target;
    }
}

static const uint64_t REACH_DOCK_PRESSABLE_TRIGGER = UINT64_MAX - 1;

static uint64_t reach_dock_pressable_target(const reach_dock *dock, reach_dock_hit_result hit,
                                            reach_pointer_button button)
{
    if (dock != nullptr && hit.type == REACH_DOCK_HIT_ITEM &&
        hit.index < dock->state.model.item_count)
    {
        return (uint64_t)reach_dock_item_key_at(&dock->state.model, hit.index);
    }
    if (button == REACH_POINTER_BUTTON_PRIMARY && hit.type == REACH_DOCK_HIT_TRIGGER)
    {
        return REACH_DOCK_PRESSABLE_TRIGGER;
    }
    return REACH_PRESSABLE_TARGET_NONE;
}

static void reach_dock_capsule_apply_pressable_result(const reach_pressable_result *pressable,
                                                      reach_capsule_pointer_result *out)
{
    if (pressable == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw = out->redraw || pressable->redraw;
    if (pressable->capture != 0)
    {
        out->capture = pressable->capture;
    }
    out->sync_pointer_subscriptions =
        out->sync_pointer_subscriptions || pressable->sync_pointer_subscriptions;
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
    reach_dock_state *state = &dock->state;
    reach_dock_interaction_context interaction_ctx = reach_dock_capsule_interaction_context(dock);
    reach_dock_hit_result hit = {};
    hit.type = REACH_DOCK_HIT_NONE;
    hit.index = REACH_MAX_DOCK_ITEMS;
    if (dock->pointer_layout_valid)
    {
        hit = reach_dock_hit_test(&dock->pointer_layout, event->x, event->y);
        reach_dock_gate_animating_hit(dock, &hit);
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN)
    {
        uint64_t target = reach_dock_pressable_target(dock, hit, event->button);
        if (target == REACH_PRESSABLE_TARGET_NONE)
        {
            return;
        }
        reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
        reach_pressable_result pressable = {};
        size_t feedback_index =
            hit.type == REACH_DOCK_HIT_TRIGGER ? REACH_DOCK_FEEDBACK_TRIGGER : hit.index;
        reach_pressable_press(&state->pressable, event->button, target, feedback_index, &feedback,
                              &pressable);
        reach_dock_capsule_apply_pressable_result(&pressable, out);
        if (hit.type == REACH_DOCK_HIT_TRIGGER)
        {
            out->handled = 1;
            out->control = {REACH_DOCK_CONTROL_TRIGGER, REACH_DOCK_TRIGGER_PRIMARY, 1};
            return;
        }
        if (hit.type == REACH_DOCK_HIT_ITEM)
        {
            out->handled = 1;
            out->control = {REACH_DOCK_CONTROL_ITEM, hit.index, 1,
                            event->button == REACH_POINTER_BUTTON_SECONDARY};
            if (event->button == REACH_POINTER_BUTTON_PRIMARY)
            {
                reach_dock_interaction_result interaction = {};
                reach_dock_item_press(dock, hit.index,
                                      reach_dock_capsule_screen_x(dock, event->x),
                                      reach_dock_capsule_screen_y(dock, event->y), &interaction_ctx,
                                      &interaction);
                reach_dock_capsule_apply_interaction_result(dock, &interaction, out);
            }
            return;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_UP)
    {
        int32_t moved = event->button == REACH_POINTER_BUTTON_PRIMARY &&
                        reach_draggable_tracking(&state->drag.gesture) &&
                        reach_draggable_moved(&state->drag.gesture);
        if (event->button == REACH_POINTER_BUTTON_PRIMARY &&
            reach_draggable_tracking(&state->drag.gesture))
        {
            reach_dock_interaction_result interaction = {};
            reach_dock_drag_end(dock, &interaction_ctx, &interaction);
            reach_dock_capsule_apply_interaction_result(dock, &interaction, out);
        }

        reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
        reach_pressable_result pressable = {};
        reach_pressable_release(&state->pressable, event->button,
                                reach_dock_pressable_target(dock, hit, event->button), &feedback,
                                &pressable);
        reach_dock_capsule_apply_pressable_result(&pressable, out);
        if (event->button == REACH_POINTER_BUTTON_SECONDARY)
        {
            if (pressable.activated && hit.type == REACH_DOCK_HIT_ITEM &&
                hit.index < state->model.item_count)
            {
                out->handled = 1;
                if (dock->routes.item_context_menu != nullptr)
                {
                    reach_menu_request request = {};
                    if (reach_dock_build_menu_request(dock, hit.index, (float)event->x,
                                                      (float)event->y, &request))
                    {
                        dock->routes.item_context_menu(dock->routes.user, &request);
                    }
                }
            }
            return;
        }
        if (moved)
        {
            out->handled = 1;
            return;
        }
        if (pressable.activated_target == REACH_DOCK_PRESSABLE_TRIGGER)
        {
            out->handled = 1;
            if (dock->routes.trigger_activated != nullptr)
            {
                dock->routes.trigger_activated(dock->routes.user, REACH_DOCK_TRIGGER_PRIMARY);
            }
        }
        else if (pressable.activated && hit.type == REACH_DOCK_HIT_ITEM &&
                 hit.index < state->model.item_count)
        {
            size_t item_index = hit.index;
            if (event->button == REACH_POINTER_BUTTON_MIDDLE)
            {
                out->handled = 1;
                if (reach_dock_build_menu_request(dock, item_index, 0.0f, 0.0f,
                                                  &dock->action_request) &&
                    dock->action_request.path[0] != 0)
                {
                    out->action.kind = REACH_FEATURE_ACTION_OPEN_TARGET;
                    out->action.flags |= REACH_FEATURE_ACTION_FLAG_NEW_INSTANCE;
                    out->action.target.kind = REACH_FEATURE_TARGET_APP;
                    out->action.target.path = dock->action_request.path;
                    out->action.target.arguments = dock->action_request.arguments[0] != 0
                                                       ? dock->action_request.arguments
                                                       : nullptr;
                    out->action.target.app_user_model_id =
                        dock->action_request.app_user_model_id[0] != 0
                            ? dock->action_request.app_user_model_id
                            : nullptr;
                }
                return;
            }
            reach_dock_item_action item_action =
                reach_dock_item_action_for_index(&state->model, item_index);
            out->handled = 1;
            if (item_action.type == REACH_DOCK_ITEM_ACTION_LAUNCH_PINNED)
            {
                out->action.kind = REACH_FEATURE_ACTION_OPEN_PINNED_APP;
                out->action.index = item_action.pinned_index;
                out->action.id = item_action.pin_id;
            }
            else if (item_action.type == REACH_DOCK_ITEM_ACTION_FOCUS_WINDOW)
            {
                out->action.kind = REACH_FEATURE_ACTION_TOGGLE_WINDOW_FOCUS;
                out->action.window = item_action.window;
            }
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_MOVE)
    {
        reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
        reach_pressable_result pressable = {};
        reach_pressable_update(
            &state->pressable,
            reach_dock_pressable_target(dock, hit, reach_pressable_button(&state->pressable)),
            &pressable);
        reach_dock_capsule_apply_pressable_result(&pressable, out);
        if (!reach_draggable_tracking(&state->drag.gesture))
        {
            size_t hovered_item =
                hit.type == REACH_DOCK_HIT_ITEM && hit.index < state->model.item_count
                    ? hit.index
                    : REACH_MAX_DOCK_ITEMS;
            if (hovered_item != state->hovered_item)
            {
                state->hovered_item = hovered_item;
                reach_dock_notify_item_hovered(dock, hovered_item);
            }
        }
        if (reach_draggable_tracking(&state->drag.gesture))
        {
            int32_t was_moved = reach_draggable_moved(&state->drag.gesture);
            reach_dock_interaction_result interaction = {};
            reach_dock_drag_update(dock, reach_dock_capsule_screen_x(dock, event->x),
                                   reach_dock_capsule_screen_y(dock, event->y), &interaction_ctx,
                                   &interaction);
            reach_dock_capsule_apply_interaction_result(dock, &interaction, out);
            if (!was_moved && reach_draggable_moved(&state->drag.gesture))
            {
                reach_pressable_disarm(&state->pressable, &feedback, &pressable);
                reach_dock_capsule_apply_pressable_result(&pressable, out);
            }
            out->handled = 1;
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_CANCEL)
    {
        if (reach_draggable_tracking(&state->drag.gesture))
        {
            reach_dock_interaction_result interaction = {};
            reach_dock_drag_end(dock, &interaction_ctx, &interaction);
            reach_dock_capsule_apply_interaction_result(dock, &interaction, out);
        }
        reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
        reach_pressable_result pressable = {};
        reach_pressable_cancel(&state->pressable, &feedback, &pressable);
        reach_dock_capsule_apply_pressable_result(&pressable, out);
        if (state->hovered_item != REACH_MAX_DOCK_ITEMS)
        {
            state->hovered_item = REACH_MAX_DOCK_ITEMS;
            reach_dock_notify_item_hovered(dock, REACH_MAX_DOCK_ITEMS);
        }
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_LEAVE)
    {
        reach_pressable_result pressable = {};
        reach_pressable_update(&state->pressable, REACH_PRESSABLE_TARGET_NONE, &pressable);
        reach_dock_capsule_apply_pressable_result(&pressable, out);
        if (state->hovered_item != REACH_MAX_DOCK_ITEMS)
        {
            state->hovered_item = REACH_MAX_DOCK_ITEMS;
            reach_dock_notify_item_hovered(dock, REACH_MAX_DOCK_ITEMS);
        }
    }
}

static int32_t reach_dock_capsule_control_at_point(const void *capsule, int32_t screen_x,
                                                   int32_t screen_y, reach_feature_control *out)
{
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    if (dock == nullptr || out == nullptr || !dock->pointer_layout_valid)
    {
        return 0;
    }

    reach_point_i32 local = reach_dock_local_point(&dock->pointer_layout, screen_x, screen_y);
    reach_dock_hit_result hit = reach_dock_hit_test(&dock->pointer_layout, local.x, local.y);
    *out = {};
    switch (hit.type)
    {
    case REACH_DOCK_HIT_ITEM:
        out->slot = REACH_DOCK_CONTROL_ITEM;
        out->index = hit.index;
        out->valid = 1;
        return 1;
    case REACH_DOCK_HIT_TRIGGER:
        out->slot = REACH_DOCK_CONTROL_TRIGGER;
        out->valid = 1;
        return 1;
    default:
        return 0;
    }
}

void reach_dock_apply_config(reach_dock *dock, float height)
{
    if (dock != nullptr && height > 0.0f)
    {
        dock->metrics.height = height;
    }
}

/* The dock resolves its own geometry: the metrics it owns, the pins it was handed and the
   windows it tracks, against the monitor composition gives every surface. */
const reach_dock_layout *reach_dock_arranged_layout(const reach_dock *dock)
{
    return dock != nullptr && dock->pointer_layout_valid ? &dock->pointer_layout : nullptr;
}

int32_t reach_dock_arrange(reach_dock *dock, const reach_dock_arrange_context *ctx)
{
    if (dock == nullptr || ctx == nullptr)
    {
        return 0;
    }

    const reach_theme *theme = ctx->theme != nullptr ? ctx->theme : reach_theme_default();
    dock->metrics.icon_size = reach_theme_icon_box_size(theme, dock->metrics.height);

    reach_ui_layout_input input = {};
    input.monitor_bounds = ctx->monitor_bounds;
    input.work_area = ctx->monitor_bounds;
    input.dpi_scale = ctx->dpi_scale;
    input.border_thickness = reach_theme_border_thickness(theme, ctx->dpi_scale);

    reach_dock_layout layout = {};
    if (reach_dock_layout_compute(&dock->metrics, &input, &layout) != REACH_OK)
    {
        return 0;
    }

    reach_dock_build_context build = {};
    build.theme = theme;
    build.dpi_scale = ctx->dpi_scale;
    build.icon_size = dock->metrics.icon_size;
    build.gap = dock->metrics.gap;
    build.pinned_apps = dock->pinned_apps;
    build.pinned_app_count = dock->pinned_app_count;

    reach_rect_f32 before = dock->pointer_layout_valid ? dock->pointer_layout.bounds
                                                       : reach_rect_f32{};
    if (reach_dock_take_items_changed(dock))
    {
        reach_dock_rebuild_items(dock, &build,
                                 dock->pointer_layout_valid ? &dock->pointer_layout : nullptr,
                                 &layout);
    }
    else
    {
        reach_dock_build_layout(dock, &build, &layout);
    }

    return !reach_rect_equal(before, dock->pointer_layout.bounds) ||
           reach_dock_slots_animating(dock);
}

const reach_feature_capsule_ops *reach_dock_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_dock_capsule_reset,
        reach_dock_capsule_tick,
        reach_dock_capsule_is_open,
        reach_dock_capsule_on_game_mode,
        reach_dock_capsule_needs_frame,
        reach_dock_capsule_wants_pointer_move,
        reach_dock_capsule_handle_pointer,
        reach_dock_capsule_pointer_sequence_active,
        nullptr,
        reach_dock_capsule_surface_geometry,
        reach_dock_capsule_pointer_capture_active,
        nullptr,
        reach_dock_capsule_control_at_point,
    };
    return &ops;
}

reach_animation_manager *reach_dock_manager(reach_dock *animations)
{
    return animations != nullptr ? &animations->manager : nullptr;
}

int32_t reach_dock_retain_context_feedback(reach_dock *dock)
{
    if (dock == nullptr)
    {
        return 0;
    }
    reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
    return reach_pressable_latch_feedback(&dock->state.pressable, &feedback);
}

int32_t reach_dock_clear_context_feedback(reach_dock *dock)
{
    if (dock == nullptr)
    {
        return 0;
    }
    reach_pressable_feedback_style feedback = reach_dock_pressable_feedback(dock);
    return reach_pressable_clear_latched_feedback(&dock->state.pressable, &feedback);
}

static reach_bar_visibility_result
reach_dock_bar_update_visibility(void *capsule, const reach_bar_visibility_request *request)
{
    reach_dock *animations = static_cast<reach_dock *>(capsule);
    if (animations == nullptr || request == nullptr)
    {
        return reach_bar_visibility_result{};
    }

    reach_bar_visibility_request bar_request = *request;
    bar_request.edge = REACH_DOCK_EDGE;
    bar_request.pointer_sequence_active = reach_pressable_tracking(&animations->state.pressable);
    int32_t bounds_changed =
        fabsf(animations->coverage_shown_bounds.x - request->shown_bounds.x) >= 0.5f ||
        fabsf(animations->coverage_shown_bounds.y - request->shown_bounds.y) >= 0.5f ||
        fabsf(animations->coverage_shown_bounds.width - request->shown_bounds.width) >= 0.5f ||
        fabsf(animations->coverage_shown_bounds.height - request->shown_bounds.height) >= 0.5f ||
        fabsf(animations->coverage_monitor_bounds.x - request->monitor_bounds.x) >= 0.5f ||
        fabsf(animations->coverage_monitor_bounds.y - request->monitor_bounds.y) >= 0.5f ||
        fabsf(animations->coverage_monitor_bounds.width - request->monitor_bounds.width) >= 0.5f ||
        fabsf(animations->coverage_monitor_bounds.height - request->monitor_bounds.height) >=
            0.5f ||
        fabsf(animations->coverage_shadow_clearance - request->shadow_clearance) >= 0.5f;
    if (!animations->coverage_valid || bounds_changed)
    {
        reach_rect_f32 protected_band =
            reach_bar_protected_band(REACH_DOCK_EDGE, request->shown_bounds,
                                     request->monitor_bounds, request->shadow_clearance);
        animations->coverage_trespassed = reach_window_tracking_any_trespassing(
            animations->windows, request->monitor_bounds, protected_band, request->excluded_window);
        animations->coverage_shown_bounds = request->shown_bounds;
        animations->coverage_monitor_bounds = request->monitor_bounds;
        animations->coverage_shadow_clearance = request->shadow_clearance;
        animations->coverage_valid = 1;
    }
    bar_request.can_hide = animations->coverage_trespassed;

    reach_bar_visibility_result result = reach_bar_update_visibility(
        &animations->state.visibility, &animations->manager, REACH_DOCK_ANIM_Y, &bar_request);
    if (!result.visible && reach_dock_clear_context_feedback(animations))
    {
        result.redraw = 1;
    }
    return result;
}

static reach_bar_reveal_animation reach_dock_bar_animation(const void *capsule)
{
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    reach_bar_reveal_animation animation = {};
    if (dock == nullptr)
    {
        return animation;
    }

    animation.position_animating =
        reach_animation_manager_active(&dock->manager, REACH_DOCK_ANIM_Y);
    animation.animated_y = reach_animation_manager_value(&dock->manager, REACH_DOCK_ANIM_Y);
    animation.content_animating =
        reach_dock_slots_animating(dock) || reach_draggable_tracking(&dock->state.drag.gesture) ||
        reach_animation_manager_active(&dock->manager, REACH_DOCK_ANIM_DRAG_SNAP) ||
        reach_animation_manager_active(&dock->manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY);
    return animation;
}

static void reach_dock_invalidate_bar_coverage(void *capsule)
{
    reach_dock *dock = static_cast<reach_dock *>(capsule);
    if (dock != nullptr)
    {
        dock->coverage_valid = 0;
    }
}

const reach_bar_reveal_ops *reach_dock_reveal_ops(void)
{
    static const reach_bar_reveal_ops ops = {
        reach_dock_bar_begin_session, reach_dock_bar_update_visibility, reach_dock_bar_animation,
        nullptr, reach_dock_invalidate_bar_coverage};
    return &ops;
}

size_t reach_dock_find_pinned_for_window(const reach_pinned_app_model *pinned_apps,
                                         size_t pinned_app_count,
                                         const reach_window_snapshot *window,
                                         reach_dock_window_matches_pinned_fn window_matches_pinned,
                                         void *match_user)
{
    if (pinned_apps == nullptr || window == nullptr || window_matches_pinned == nullptr)
    {
        return REACH_MAX_DOCK_ITEMS;
    }

    for (size_t index = 0; index < pinned_app_count; ++index)
    {
        if (window_matches_pinned(match_user, &pinned_apps[index], window))
        {
            return index;
        }
    }

    return REACH_MAX_DOCK_ITEMS;
}

static void reach_dock_item_set_identity(reach_dock_item_model *item, const uint16_t *path,
                                         const uint16_t *app_user_model_id,
                                         const uint16_t *icon_ref)
{
    reach_copy_utf16(item->path, REACH_DOCK_TEXT_CAPACITY, path);
    reach_copy_utf16(item->app_user_model_id, REACH_DOCK_TEXT_CAPACITY, app_user_model_id);
    reach_copy_utf16(item->icon_ref, REACH_DOCK_TEXT_CAPACITY,
                     icon_ref != nullptr && icon_ref[0] != 0 ? icon_ref : path);
}

static void reach_dock_item_add_instance(reach_dock_item_model *item, uintptr_t window)
{
    if (window == 0 || item->instance_count >= REACH_DOCK_MAX_INSTANCES)
    {
        return;
    }
    for (size_t index = 0; index < item->instance_count; ++index)
    {
        if (item->instances[index] == window)
        {
            return;
        }
    }
    item->instances[item->instance_count++] = window;
    if (item->window == 0)
    {
        item->window = window;
    }
}

void reach_dock_feature_model_build_candidates(
    reach_dock_item_model *items, size_t *item_count, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows,
    const uint32_t *window_group_ids, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (item_count != nullptr)
    {
        *item_count = 0;
    }
    if (items == nullptr || item_count == nullptr)
    {
        return;
    }

    uint32_t group_of[REACH_MAX_DOCK_ITEMS] = {};
    size_t count = 0;
    size_t running_count = 0;

    for (size_t index = 0;
         pinned_apps != nullptr && index < pinned_app_count && count < REACH_MAX_DOCK_ITEMS;
         ++index)
    {
        reach_dock_item_model *item = &items[count];
        *item = {};
        item->pinned = 1;
        item->pin_id = pinned_apps[index].id;
        reach_dock_item_set_identity(item, pinned_apps[index].path,
                                     pinned_apps[index].app_user_model_id,
                                     pinned_apps[index].icon_ref);
        ++count;
    }
    size_t pinned_count = count;

    for (size_t index = 0; open_windows != nullptr && index < open_window_count; ++index)
    {
        const reach_window_snapshot *window = &open_windows[index];
        size_t pinned_index = reach_dock_find_pinned_for_window(
            pinned_apps, pinned_app_count, window, window_matches_pinned, match_user);
        if (pinned_index != REACH_MAX_DOCK_ITEMS && pinned_index < pinned_count)
        {
            reach_dock_item_add_instance(&items[pinned_index], window->id);
            continue;
        }

        uint32_t group_id = window_group_ids != nullptr ? window_group_ids[index] : 0;
        size_t existing = REACH_MAX_DOCK_ITEMS;
        for (size_t at = pinned_count; group_id != 0 && at < count; ++at)
        {
            if (group_of[at] == group_id)
            {
                existing = at;
                break;
            }
        }
        if (existing != REACH_MAX_DOCK_ITEMS)
        {
            reach_dock_item_add_instance(&items[existing], window->id);
            continue;
        }
        if (running_count >= REACH_MAX_DOCK_RUNNING_APPS || count >= REACH_MAX_DOCK_ITEMS)
        {
            continue;
        }

        reach_dock_item_model *item = &items[count];
        *item = {};
        item->pinned = 0;
        reach_dock_item_set_identity(item, window->path, window->app_user_model_id,
                                     window->icon_ref);
        reach_dock_item_add_instance(item, window->id);
        group_of[count] = group_id;
        ++count;
        ++running_count;
    }

    *item_count = count;
}

/* A candidate inherits the key of the app it matches by identity, so an app keeps its dock slot
   through being pinned, unpinned, or handed a new pin id by the config store. */
static void reach_dock_assign_keys(reach_dock_feature_model *model, reach_dock_item_model *candidates,
                                   size_t candidate_count, uint32_t *next_key)
{
    for (size_t index = 0; index < candidate_count; ++index)
    {
        reach_dock_item_model *candidate = &candidates[index];
        candidate->key = 0;
        for (size_t at = 0; at < model->item_count; ++at)
        {
            if (reach_dock_item_identity_equal(&model->items[at], candidate->path,
                                               candidate->app_user_model_id))
            {
                candidate->key = model->items[at].key;
                break;
            }
        }
        if (candidate->key == 0)
        {
            candidate->key = (*next_key)++;
        }
    }
}

static void reach_dock_order_candidates(reach_dock_feature_model *model,
                                        const reach_dock_item_model *candidates,
                                        size_t candidate_count)
{
    reach_dock_item_model ordered[REACH_MAX_DOCK_ITEMS] = {};
    int32_t used[REACH_MAX_DOCK_ITEMS] = {};
    size_t ordered_count = 0;

    for (size_t order_index = 0;
         order_index < model->order_count && ordered_count < REACH_MAX_DOCK_ITEMS; ++order_index)
    {
        for (size_t index = 0; index < candidate_count; ++index)
        {
            if (!used[index] && candidates[index].key == model->order[order_index])
            {
                ordered[ordered_count++] = candidates[index];
                used[index] = 1;
                break;
            }
        }
    }

    for (size_t index = 0; index < candidate_count && ordered_count < REACH_MAX_DOCK_ITEMS; ++index)
    {
        if (!used[index])
        {
            ordered[ordered_count++] = candidates[index];
        }
    }

    model->item_count = ordered_count;
    model->order_count = ordered_count;
    for (size_t index = 0; index < ordered_count; ++index)
    {
        model->items[index] = ordered[index];
        model->order[index] = ordered[index].key;
    }
}

void reach_dock_feature_model_build_items(
    reach_dock_feature_model *model, uint32_t *next_key, const reach_pinned_app_model *pinned_apps,
    size_t pinned_app_count, const reach_window_snapshot *open_windows,
    const uint32_t *window_group_ids, size_t open_window_count,
    reach_dock_window_matches_pinned_fn window_matches_pinned, void *match_user)
{
    if (model == nullptr || next_key == nullptr)
    {
        return;
    }
    if (*next_key == 0)
    {
        *next_key = 1;
    }

    static reach_dock_item_model candidates[REACH_MAX_DOCK_ITEMS];
    size_t candidate_count = 0;
    reach_dock_feature_model_build_candidates(candidates, &candidate_count, pinned_apps,
                                              pinned_app_count, open_windows, window_group_ids,
                                              open_window_count, window_matches_pinned, match_user);
    reach_dock_assign_keys(model, candidates, candidate_count, next_key);
    reach_dock_order_candidates(model, candidates, candidate_count);
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
    if (dock == nullptr || out_commands == nullptr || item_index >= dock->state.model.item_count)
    {
        return 0;
    }
    const reach_dock_item_model *item = &dock->state.model.items[item_index];

    const int32_t has_path = item->path[0] != 0;
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
        reach_dock_item_window item_windows[2] = {};
        size_t item_window_count =
            reach_dock_collect_item_windows(dock, item_index, dock->pointer_pinned_apps,
                                            dock->pointer_pinned_app_count, item_windows, 2);
        out_commands[count++] = item_window_count > 1 ? REACH_CONTEXT_MENU_COMMAND_CLOSE_ALL
                                                      : REACH_CONTEXT_MENU_COMMAND_CLOSE;
    }
    return count;
}

/* Nothing to rebase. An entry is identified by its application, not by the pin id the config
   store happened to hand it, so pinning, unpinning and reissued ids only change properties on an
   entry that keeps its key and therefore its place. */
void reach_dock_apply_pinned_apps(reach_dock *dock, const reach_pinned_app_model *apps,
                                  size_t count)
{
    if (dock == nullptr)
    {
        return;
    }
    if (apps == nullptr)
    {
        count = 0;
    }
    if (count > REACH_MAX_PINNED_APPS)
    {
        count = REACH_MAX_PINNED_APPS;
    }

    dock->pinned_app_count = count;
    for (size_t index = 0; index < count; ++index)
    {
        dock->pinned_apps[index] = apps[index];
    }
    reach_dock_mark_items_changed(dock);
}

int32_t reach_dock_build_menu_request(reach_dock *dock, size_t item_index, float pointer_x,
                                      float pointer_y, reach_menu_request *out_request)
{
    if (dock == nullptr || out_request == nullptr || item_index >= reach_dock_item_count(dock))
    {
        return 0;
    }

    *out_request = {};
    out_request->target_index = item_index;
    out_request->pointer_x = pointer_x;
    out_request->pointer_y = pointer_y;
    out_request->drop_direction = REACH_POPUP_DROP_UP;

    const reach_dock_item_model *item = reach_dock_item_at(dock, item_index);
    out_request->window = item->window;
    out_request->pin_id = item->pin_id;
    reach_copy_utf16(out_request->path, REACH_MENU_TEXT_CAPACITY, item->path);
    reach_copy_utf16(out_request->app_user_model_id, REACH_MENU_TEXT_CAPACITY,
                     item->app_user_model_id);
    reach_copy_utf16(out_request->icon_ref, REACH_MENU_TEXT_CAPACITY, item->icon_ref);
    if (item->pinned)
    {
        for (size_t index = 0; index < dock->pinned_app_count; ++index)
        {
            if (dock->pinned_apps[index].id == item->pin_id)
            {
                reach_copy_utf16(out_request->arguments, REACH_MENU_TEXT_CAPACITY,
                                 dock->pinned_apps[index].arguments);
                break;
            }
        }
    }

    out_request->command_count = reach_dock_build_item_context_commands(
        dock, item_index, out_request->commands, REACH_CONTEXT_MENU_MAX_ITEMS);

    reach_dock_item_window item_windows[REACH_MENU_MAX_WINDOWS] = {};
    size_t window_count = reach_dock_collect_item_windows(
        dock, item_index, dock->pinned_apps, dock->pinned_app_count, item_windows,
        REACH_MENU_MAX_WINDOWS);
    out_request->window_count = window_count;
    for (size_t index = 0; index < window_count; ++index)
    {
        out_request->windows[index].window = item_windows[index].window;
        reach_copy_utf16(out_request->windows[index].title, REACH_MENU_TEXT_CAPACITY,
                         item_windows[index].title);
    }

    reach_rect_f32 anchor = {};
    float bar_edge_y = 0.0f;
    if (reach_dock_item_anchor(dock, item_index, &anchor, &bar_edge_y))
    {
        out_request->anchored = 1;
        out_request->anchor_button = anchor;
        out_request->bar_edge_y = bar_edge_y;
    }
    return 1;
}

size_t reach_dock_order_count(reach_dock *dock)
{
    return dock != nullptr ? reach_dock_state_mut(dock)->model.order_count : 0;
}

uint32_t reach_dock_order_key_at(reach_dock *dock, size_t index)
{
    if (dock == nullptr || index >= reach_dock_state_mut(dock)->model.order_count)
    {
        return 0;
    }
    return reach_dock_state_mut(dock)->model.order[index];
}

void reach_dock_restore_order(reach_dock *dock, const uint32_t *keys, size_t count)
{
    if (dock == nullptr || keys == nullptr || count > REACH_MAX_DOCK_ITEMS)
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

static float reach_dock_slot_reveal(const reach_dock *dock, size_t pool_index)
{
    float reveal = reach_animation_manager_value(&dock->manager, reach_dock_slot_track(pool_index));
    if (reveal < 0.0f)
    {
        return 0.0f;
    }
    return reveal > 1.0f ? 1.0f : reveal;
}

static void reach_dock_gate_animating_hit(reach_dock *dock, reach_dock_hit_result *hit)
{
    if (hit->type == REACH_DOCK_HIT_ITEM && reach_dock_slots_animating(dock))
    {
        hit->type = REACH_DOCK_HIT_NONE;
        hit->index = REACH_MAX_DOCK_ITEMS;
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
    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        if (dock->slots[pool].lifecycle == REACH_DOCK_SLOT_EMPTY)
        {
            return pool;
        }
    }
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        size_t pool = dock->slot_order[at];
        if (dock->slots[pool].lifecycle == REACH_DOCK_SLOT_DYING)
        {
            reach_dock_slot_free(dock, pool);
            return pool;
        }
    }
    REACH_ASSERT(0 && "dock slot pool exhausted");
    return REACH_DOCK_SLOT_CAPACITY;
}

static size_t reach_dock_slot_find(reach_dock *dock, uint32_t key)
{
    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        if (dock->slots[pool].lifecycle != REACH_DOCK_SLOT_EMPTY && key != 0 &&
            dock->slots[pool].key == key)
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
        else if (slot->lifecycle == REACH_DOCK_SLOT_DYING)
        {
            reach_dock_slot_free(dock, pool);
        }
    }
}

static void reach_dock_sync_slots(reach_dock *dock)
{
    reach_dock_state *state = &dock->state;
    size_t item_count = state->model.item_count;
    if (item_count > REACH_MAX_DOCK_ITEMS)
    {
        item_count = REACH_MAX_DOCK_ITEMS;
    }

    if (!dock->slots_synced)
    {
        for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
        {
            dock->slots[pool] = {};
            reach_animation_manager_reset(&dock->manager, reach_dock_slot_track(pool));
        }
        dock->slot_order_count = 0;
        for (size_t index = 0; index < item_count; ++index)
        {
            size_t pool = index;
            dock->slots[pool].lifecycle = REACH_DOCK_SLOT_STEADY;
            dock->slots[pool].key = reach_dock_item_key_at(&state->model, index);
            reach_animation_manager_set(&dock->manager, reach_dock_slot_track(pool), 1.0f);
            dock->slot_order[dock->slot_order_count++] = (uint16_t)pool;
        }
        dock->slots_synced = 1;
        return;
    }

    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
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
            if (slot->key != 0 && slot->key == reach_dock_item_key_at(&state->model, index))
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

    uint16_t dying_anchor[REACH_DOCK_SLOT_CAPACITY] = {};
    size_t last_live = 0;
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        size_t pool = dock->slot_order[at];
        if (dock->slots[pool].lifecycle == REACH_DOCK_SLOT_DYING)
        {
            dying_anchor[pool] = (uint16_t)last_live;
        }
        else if (dock->slots[pool].lifecycle != REACH_DOCK_SLOT_EMPTY)
        {
            last_live = pool;
        }
    }

    uint16_t new_order[REACH_DOCK_SLOT_CAPACITY] = {};
    size_t new_count = 0;
    for (size_t index = 0; index < item_count; ++index)
    {
        uint32_t item_key = reach_dock_item_key_at(&state->model, index);
        size_t pool = reach_dock_slot_find(dock, item_key);
        if (pool < REACH_DOCK_SLOT_CAPACITY)
        {
            reach_dock_slot *slot = &dock->slots[pool];
            slot->key = item_key;
            if (slot->lifecycle == REACH_DOCK_SLOT_DYING)
            {

                slot->lifecycle = REACH_DOCK_SLOT_APPEARING;
            }
            if (slot->lifecycle != REACH_DOCK_SLOT_STEADY)
            {
                if (reach_animation_manager_target(&dock->manager, reach_dock_slot_track(pool)) !=
                    1.0f)
                {
                    reach_animation_manager_animate_to(&dock->manager, reach_dock_slot_track(pool),
                                                       1.0f, REACH_DOCK_SLOT_ANIMATION_SECONDS,
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
            reach_animation_manager_start(&dock->manager, reach_dock_slot_track(pool), 0.0f, 1.0f,
                                          REACH_DOCK_SLOT_ANIMATION_SECONDS,
                                          REACH_EASING_EASE_IN_OUT);
        }
        new_order[new_count++] = (uint16_t)pool;
        for (size_t dying = 0; dying < REACH_DOCK_SLOT_CAPACITY; ++dying)
        {
            if (dock->slots[dying].lifecycle == REACH_DOCK_SLOT_DYING &&
                dying_anchor[dying] == pool && dying != pool)
            {
                new_order[new_count++] = (uint16_t)dying;
            }
        }
    }

    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
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
            new_order[new_count++] = (uint16_t)pool;
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
    for (size_t pool = 0; pool < REACH_DOCK_SLOT_CAPACITY; ++pool)
    {
        reach_dock_slot *slot = &dock->slots[pool];
        if (slot->lifecycle == REACH_DOCK_SLOT_EMPTY)
        {
            continue;
        }
        if (slot->lifecycle == REACH_DOCK_SLOT_DYING)
        {
            reach_dock_slot_free(dock, pool);
            continue;
        }
        slot->lifecycle = REACH_DOCK_SLOT_STEADY;
        reach_animation_manager_set(&dock->manager, reach_dock_slot_track(pool), 1.0f);
    }
}

float reach_dock_item_reveal(reach_dock *dock, size_t item_index)
{
    if (dock == nullptr || item_index >= dock->state.model.item_count)
    {
        return 0.0f;
    }
    uint32_t key = reach_dock_item_key_at(&dock->state.model, item_index);
    size_t pool = reach_dock_slot_find(dock, key);
    if (pool >= REACH_DOCK_SLOT_CAPACITY)
    {
        return 1.0f;
    }
    if (dock->slots[pool].lifecycle == REACH_DOCK_SLOT_STEADY)
    {
        return 1.0f;
    }
    const float progress = reach_dock_slot_reveal(dock, pool);
    if (progress <= REACH_DOCK_SLOT_REVEAL_THRESHOLD)
    {
        return 0.0f;
    }
    float reveal =
        (progress - REACH_DOCK_SLOT_REVEAL_THRESHOLD) / (1.0f - REACH_DOCK_SLOT_REVEAL_THRESHOLD);
    return reveal > 1.0f ? 1.0f : reveal;
}

reach_dock_fit_result reach_dock_fit_metrics(float native_height, float native_icon_size,
                                             float native_gap, float native_border_thickness,
                                             float available_width, float app_slot_units)
{
    reach_dock_fit_result result = {};
    if (native_height < 0.0f)
    {
        native_height = 0.0f;
    }
    if (native_icon_size < 0.0f)
    {
        native_icon_size = 0.0f;
    }
    if (native_gap < 0.0f)
    {
        native_gap = 0.0f;
    }
    if (native_border_thickness < 0.0f)
    {
        native_border_thickness = 0.0f;
    }
    if (native_border_thickness > native_height * 0.5f)
    {
        native_border_thickness = native_height * 0.5f;
    }
    float available_icon_height = native_height - native_border_thickness * 2.0f;
    if (native_icon_size > available_icon_height)
    {
        native_icon_size = available_icon_height;
    }
    if (app_slot_units < 0.0f)
    {
        app_slot_units = 0.0f;
    }

    const float native_outer_padding = native_gap * REACH_DOCK_OUTER_PADDING_SCALE;
    const float native_width = native_border_thickness * 2.0f + native_outer_padding * 2.0f +
                               native_icon_size + app_slot_units * (native_icon_size + native_gap);
    result.scale = 1.0f;
    if (available_width > 0.0f && native_width > available_width)
    {
        result.scale = available_width / native_width;
    }
    result.width = native_width * result.scale;
    result.height = native_height * result.scale;
    result.icon_size = native_icon_size * result.scale;
    result.gap = native_gap * result.scale;
    return result;
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
    reach_dock_sync_slots(dock);

    float app_slot_units = 0.0f;
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        float reveal = reach_dock_slot_reveal(dock, dock->slot_order[at]);
        app_slot_units += reveal;
    }

    const float dpi_scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    const float native_height =
        layout->native_height > 0.0f ? layout->native_height : layout->bounds.height;
    const float native_border_thickness = reach_theme_border_thickness(ctx->theme, dpi_scale);
    const reach_dock_fit_result fit =
        reach_dock_fit_metrics(native_height, ctx->icon_size * dpi_scale, ctx->gap * dpi_scale,
                               native_border_thickness, layout->available_width, app_slot_units);
    const float center_x = layout->bounds.x + layout->bounds.width * 0.5f;
    const float bottom = layout->bounds.y + layout->bounds.height;
    layout->bounds.x = center_x - fit.width * 0.5f;
    layout->bounds.y = bottom - fit.height;
    layout->bounds.width = fit.width;
    layout->bounds.height = fit.height;
    layout->native_height = native_height;
    layout->content_scale = fit.scale;

    const float icon_size = fit.icon_size;
    const float gap = fit.gap;
    const float border_thickness = native_border_thickness * fit.scale;
    const float app_slot_width = icon_size + gap;

    const float top = (layout->bounds.height - icon_size) * 0.5f;

    float x = border_thickness + gap * REACH_DOCK_OUTER_PADDING_SCALE;
    layout->trigger_button.width = icon_size;
    layout->trigger_button.height = icon_size;
    layout->trigger_button.x = x;
    layout->trigger_button.y = top;
    x += app_slot_width;

    size_t item_index = 0;
    for (size_t at = 0; at < dock->slot_order_count; ++at)
    {
        size_t pool = dock->slot_order[at];
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
        x += reach_dock_slot_reveal(dock, pool) * app_slot_width;
    }

    for (; item_index < layout->app_slot_count; ++item_index)
    {
        layout->app_slots[item_index].x = x;
        layout->app_slots[item_index].y = top;
        layout->app_slots[item_index].width = icon_size;
        layout->app_slots[item_index].height = icon_size;
        x += app_slot_width;
    }

    dock->pointer_layout = *layout;
    dock->pointer_layout_valid = 1;
}

reach_result reach_dock_append_surface_render_commands(reach_dock *dock,
                                                       const reach_dock_surface_render_context *ctx,
                                                       reach_render_command_buffer *out_commands)
{
    if (dock == nullptr || ctx == nullptr || out_commands == nullptr || !dock->pointer_layout_valid)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_dock_layout layout = dock->pointer_layout;
    layout.bounds = ctx->bounds;

    reach_dock_render_context render = {};
    render.theme = ctx->theme;
    render.layout = &layout;
    render.focused_window =
        dock->windows != nullptr ? reach_window_tracking_foreground(dock->windows) : 0;
    render.pinned_apps = dock->pointer_pinned_apps;
    render.pinned_app_count = dock->pointer_pinned_app_count;
    render.icon_size_px = ctx->icon_size_px;
    render.dpi_scale = ctx->dpi_scale;
    return reach_dock_append_render_commands(dock, &render, out_commands);
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
    for (size_t index = 0; index < layout.app_slot_count; ++index)
    {
        layout.app_slots[index] = reach_dock_rect_to_screen(&layout, layout.app_slots[index]);
    }
    return layout;
}

int32_t reach_dock_item_anchor(const reach_dock *dock, size_t index, reach_rect_f32 *out_button,
                               float *out_bar_edge_y)
{
    if (dock == nullptr || !dock->pointer_layout_valid || out_button == nullptr ||
        out_bar_edge_y == nullptr || index >= dock->pointer_layout.app_slot_count)
    {
        return 0;
    }
    *out_button =
        reach_dock_rect_to_screen(&dock->pointer_layout, dock->pointer_layout.app_slots[index]);
    *out_bar_edge_y = dock->pointer_layout.bounds.y;
    return 1;
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
    if (dock == nullptr || index >= REACH_MAX_DOCK_ITEMS)
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
    reach_animation_manager_start(&dock->manager, reach_dock_item_animation_id(index), offset, 0.0f,
                                  0.15, REACH_EASING_EASE_IN_OUT);
    dock->state.item_x_valid[index] = 1;
}

void reach_dock_clear_item_x_animations(reach_dock *dock)
{
    if (dock == nullptr)
    {
        return;
    }
    reach_dock_snap_slots(dock);
    for (size_t index = 0; index < REACH_MAX_DOCK_ITEMS; ++index)
    {
        reach_animation_manager_reset(&dock->manager, reach_dock_item_animation_id(index));
        dock->state.item_x_valid[index] = 0;
        dock->state.item_x_keys[index] = {};
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
    if (count > REACH_MAX_DOCK_ITEMS)
    {
        count = REACH_MAX_DOCK_ITEMS;
    }
    out_snapshot->count = count;
    for (size_t index = 0; index < count; ++index)
    {
        out_snapshot->keys[index] = reach_dock_item_key_at(&state->model, index);
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
        float target_x = reach_dock_slot_box_x(theme, layout, index);
        float from_x = target_x;
        uint32_t item_key = reach_dock_item_key_at(&state->model, index);
        for (size_t old_index = 0; old_index < snapshot->count; ++old_index)
        {
            if (item_key != 0 && snapshot->keys[old_index] == item_key)
            {
                from_x = snapshot->x[old_index];
                break;
            }
        }
        state->item_x_keys[index] = item_key;
        if (item_key != 0 && state->drag.key == item_key &&
            (reach_draggable_tracking(&state->drag.gesture) ||
             reach_animation_manager_active(&dock->manager, REACH_DOCK_ANIM_DRAG_SNAP)))
        {
            reach_dock_start_item_x_animation(dock, index, target_x, target_x);
        }
        else
        {
            reach_dock_start_item_x_animation(dock, index, from_x, target_x);
        }
    }
    for (size_t index = state->model.item_count; index < REACH_MAX_DOCK_ITEMS; ++index)
    {
        state->item_x_valid[index] = 0;
        reach_animation_manager_reset(&dock->manager, reach_dock_item_animation_id(index));
        state->item_x_keys[index] = {};
    }
}
