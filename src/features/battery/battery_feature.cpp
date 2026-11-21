#include "battery_common.h"

#include "reach/features/common/ui_controls.h"

#include <new>

static const double REACH_BATTERY_SAVER_PENDING_TIMEOUT_SECONDS = 2.0;
static const uint64_t REACH_BATTERY_PRESSABLE_TARGET_SAVER = 1;

static reach_pressable_feedback_style reach_battery_pressable_feedback(reach_battery *battery)
{
    reach_pressable_feedback_style feedback = {};
    feedback.motion = battery != nullptr ? &battery->press_feedback : nullptr;
    feedback.pressed_value = 1.0f;
    return feedback;
}

static void reach_battery_reset_pressable(reach_battery *battery)
{
    if (battery == nullptr)
    {
        return;
    }
    reach_pressable_feedback_style feedback = reach_battery_pressable_feedback(battery);
    reach_pressable_reset(&battery->pressable, &feedback);
}

const reach_battery_state *reach_battery_state_ptr(const reach_battery *battery)
{
    return battery != nullptr ? &battery->state : nullptr;
}

int32_t reach_battery_is_open(const reach_battery *battery)
{
    return battery != nullptr && battery->state.open;
}

static void reach_battery_settle_time_reveal(reach_battery *battery)
{
    reach_motion_set(&battery->time_reveal_opacity, 1.0f);
    reach_motion_set(&battery->time_reveal_y, 0.0f);
}

static void reach_battery_reveal_time(reach_battery *battery)
{
    if (!battery->state.open)
    {
        reach_battery_settle_time_reveal(battery);
        return;
    }
    double now = reach_motion_now_seconds();
    reach_motion_start_spec_at(&battery->time_reveal_opacity, 0.0f, 1.0f, 0.0f,
                               battery->fade_in_spec, now);
    reach_motion_start_spec_at(&battery->time_reveal_y, battery->time_reveal_offset, 0.0f, 0.0f,
                               battery->smooth_spec, now);
}

int32_t reach_battery_set_open(reach_battery *battery, int32_t open)
{
    if (battery == nullptr)
    {
        return 0;
    }
    int32_t next = open ? 1 : 0;
    if (battery->state.open == next)
    {
        return 0;
    }
    if (next)
    {
        reach_system_status_refresh_system(battery->status, REACH_SYSTEM_CONTROLS_CHANGE_POWER);
        (void)reach_battery_refresh_power(battery);
        reach_battery_settle_time_reveal(battery);
    }
    else
    {
        reach_battery_reset_pressable(battery);
    }
    battery->state.open = next;
    (void)reach_feature_transition_set_open(&battery->popup_transition, next);
    return 1;
}

void reach_battery_force_close(reach_battery *battery)
{
    if (battery != nullptr)
    {
        reach_battery_reset_pressable(battery);
        battery->state.open = 0;
        reach_feature_transition_reset(&battery->popup_transition);
    }
}

void reach_battery_reset(reach_battery *battery)
{
    if (battery == nullptr)
    {
        return;
    }
    reach_battery_reset_pressable(battery);
    battery->state = {};
    battery->state.model.seconds_remaining = -1;
    battery->saver_pending_deadline = 0.0;
    reach_battery_settle_time_reveal(battery);
    reach_feature_transition_reset(&battery->popup_transition);
}

int32_t reach_battery_model_saver_effective(const reach_battery_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    return model->saver_pending ? (model->saver_pending_enabled ? 1 : 0)
                                : (model->saver_on ? 1 : 0);
}

void reach_battery_format_percent(uint16_t *dst, size_t dst_count, int32_t percent)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }

    uint16_t digits[3] = {};
    size_t digit_count = 0;
    if (percent == 0)
    {
        digits[digit_count++] = '0';
    }
    while (percent > 0 && digit_count < 3)
    {
        digits[digit_count++] = (uint16_t)('0' + percent % 10);
        percent /= 10;
    }

    size_t length = 0;
    while (digit_count > 0 && length + 2 < dst_count)
    {
        dst[length++] = digits[--digit_count];
    }
    if (length + 1 < dst_count)
    {
        dst[length++] = '%';
    }
    dst[length] = 0;
}

static size_t reach_battery_append_number(uint16_t *dst, size_t dst_count, size_t length,
                                          int32_t value, uint16_t unit)
{
    uint16_t digits[10] = {};
    size_t digit_count = 0;
    do
    {
        digits[digit_count++] = (uint16_t)('0' + value % 10);
        value /= 10;
    } while (value > 0 && digit_count < 10);
    while (digit_count > 0 && length + 2 < dst_count)
    {
        dst[length++] = digits[--digit_count];
    }
    if (length + 1 < dst_count)
    {
        dst[length++] = unit;
    }
    dst[length] = 0;
    return length;
}

void reach_battery_format_time_remaining(uint16_t *dst, size_t dst_count,
                                         const reach_battery_model *model)
{
    static const uint16_t fully_charged[] = {'F', 'u', 'l', 'l', 'y', ' ', 'c',
                                             'h', 'a', 'r', 'g', 'e', 'd', 0};
    static const uint16_t charging[] = {'C', 'h', 'a', 'r', 'g', 'i', 'n', 'g', 0};
    static const uint16_t estimating[] = {'E', 's', 't', 'i', 'm',    'a',
                                          't', 'i', 'n', 'g', 0x2026, 0};
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    dst[0] = 0;
    if (model == nullptr)
    {
        return;
    }
    if (model->plugged_in)
    {
        reach_copy_utf16(dst, dst_count, model->percent >= 100 ? fully_charged : charging);
        return;
    }
    if (model->seconds_remaining < 0)
    {
        reach_copy_utf16(dst, dst_count, estimating);
        return;
    }
    int32_t minutes = (model->seconds_remaining + 30) / 60;
    if (minutes < 1)
    {
        minutes = 1;
    }
    size_t length = 0;
    if (minutes >= 60)
    {
        length = reach_battery_append_number(dst, dst_count, length, minutes / 60, 'h');
        if (minutes % 60 != 0 && length + 1 < dst_count)
        {
            dst[length++] = ' ';
            dst[length] = 0;
        }
    }
    if (minutes < 60 || minutes % 60 != 0)
    {
        (void)reach_battery_append_number(dst, dst_count, length, minutes % 60, 'm');
    }
}

void reach_battery_attach_services(reach_battery *battery, reach_system_status *status)
{
    if (battery != nullptr)
    {
        battery->status = status;
    }
}

void reach_battery_set_routes(reach_battery *battery, const reach_battery_routes *routes)
{
    if (battery != nullptr)
    {
        battery->routes = routes != nullptr ? *routes : reach_battery_routes{};
    }
}

int32_t reach_battery_refresh_power(reach_battery *battery)
{
    if (battery == nullptr)
    {
        return 0;
    }

    reach_system_status_system_snapshot snapshot = {};
    reach_system_status_read_system(battery->status, &snapshot);
    if (!snapshot.power_valid)
    {
        return 0;
    }

    int32_t has_reading =
        snapshot.power_valid && snapshot.power.has_battery && snapshot.power.battery_percent >= 0;
    int32_t power_changed =
        reach_battery_set_power(battery, has_reading ? snapshot.power.battery_percent : 0,
                                snapshot.power_valid && snapshot.power.battery_saver_on ? 1 : 0);
    int32_t time_changed = reach_battery_set_time_remaining(
        battery, snapshot.power.battery_charging, snapshot.power.battery_seconds_remaining);
    return power_changed || time_changed;
}

int32_t reach_battery_set_time_remaining(reach_battery *battery, int32_t plugged_in,
                                         int32_t seconds_remaining)
{
    if (battery == nullptr)
    {
        return 0;
    }
    reach_battery_model *model = &battery->state.model;
    int32_t next_plugged = plugged_in ? 1 : 0;
    int32_t next_seconds = !next_plugged && seconds_remaining >= 0 ? seconds_remaining : -1;
    if (model->plugged_in == next_plugged && model->seconds_remaining == next_seconds)
    {
        return 0;
    }

    uint16_t before[REACH_BATTERY_TIME_TEXT_CAPACITY] = {};
    reach_battery_format_time_remaining(before, REACH_BATTERY_TIME_TEXT_CAPACITY, model);
    model->plugged_in = next_plugged;
    model->seconds_remaining = next_seconds;
    uint16_t after[REACH_BATTERY_TIME_TEXT_CAPACITY] = {};
    reach_battery_format_time_remaining(after, REACH_BATTERY_TIME_TEXT_CAPACITY, model);
    if (!reach_utf16_equal(before, after))
    {
        reach_battery_reveal_time(battery);
    }
    return 1;
}

int32_t reach_battery_set_power(reach_battery *battery, int32_t percent, int32_t saver_on)
{
    if (battery == nullptr)
    {
        return 0;
    }

    reach_battery_model *model = &battery->state.model;
    int32_t next_saver = saver_on ? 1 : 0;
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }

    int32_t changed = model->percent != percent || model->saver_on != next_saver;

    model->percent = percent;
    model->saver_on = next_saver;

    if (model->saver_pending && model->saver_on == model->saver_pending_enabled)
    {
        reach_battery_set_saver_pending(battery, 0, 0);
        changed = 1;
    }

    return changed;
}

void reach_battery_set_saver_pending(reach_battery *battery, int32_t pending,
                                     int32_t pending_enabled)
{
    if (battery == nullptr)
    {
        return;
    }
    int32_t next_pending = pending ? 1 : 0;
    int32_t next_enabled = pending_enabled ? 1 : 0;
    int32_t changed = battery->state.model.saver_pending != next_pending ||
                      battery->state.model.saver_pending_enabled != next_enabled;
    battery->state.model.saver_pending = next_pending;
    battery->state.model.saver_pending_enabled = next_enabled;
    battery->saver_pending_deadline =
        next_pending ? reach_motion_now_seconds() + REACH_BATTERY_SAVER_PENDING_TIMEOUT_SECONDS : 0;
    if (changed && battery->routes.saver_pending_changed != nullptr)
    {
        battery->routes.saver_pending_changed(battery->routes.user, next_pending, next_enabled);
    }
}

int32_t reach_battery_saver_pending(const reach_battery *battery)
{
    return battery != nullptr && battery->state.model.saver_pending;
}

static void reach_battery_place(reach_battery_state *state, const reach_battery_open_context *ctx)
{
    const reach_battery_metrics &metrics = reach_battery_metrics_values;
    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    float border_thickness = reach_theme_border_thickness(ctx->theme, scale);

    state->drop_direction = ctx->drop_direction;

    float padding = metrics.padding * scale;
    float row_height = metrics.row_height * scale;
    float row_gap = metrics.row_gap * scale;
    float row_inset = metrics.row_inset * scale;
    float separator_height = reach_ui_horizontal_separator_bounds(0, 0, 0, scale).height;
    float separator_inset = metrics.separator_inset * scale;
    float body_width = metrics.popup_width * scale;
    float popup_width = body_width + border_thickness * 2.0f;
    float margin = metrics.screen_margin * scale;
    float notch_height = reach_popup_notch_height_scaled(scale);

    float body_height = padding * 2.0f + row_height * 3.0f + row_gap * 3.0f + separator_height;
    float popup_height = body_height + notch_height + border_thickness * 2.0f;

    reach_popup_anchor anchor = {};
    anchor.button = ctx->anchor_button;
    anchor.monitor = ctx->monitor;
    anchor.bar_edge_y = ctx->bar_edge_y;
    anchor.direction = ctx->drop_direction;

    reach_popup_placement placement = reach_popup_place(&anchor, popup_width, popup_height, margin);
    state->bounds = placement.bounds;
    state->notch_anchor_x = placement.notch_anchor_x;
    body_width = placement.bounds.width - border_thickness * 2.0f;
    if (body_width < 0.0f)
    {
        body_width = 0.0f;
    }

    float content_y = border_thickness + padding +
                      (ctx->drop_direction == REACH_POPUP_DROP_DOWN ? notch_height : 0.0f);
    float content_x = border_thickness + row_inset;
    float content_width = body_width - row_inset * 2.0f;

    state->percent_label = {content_x, content_y, content_width, row_height};
    content_y += row_height + row_gap;

    state->time_label = {content_x, content_y, content_width, row_height};
    content_y += row_height + row_gap;

    state->separator = reach_ui_horizontal_separator_bounds(
        border_thickness + separator_inset, content_y, body_width - separator_inset * 2.0f, scale);
    content_y += separator_height + row_gap;

    state->saver_row = {border_thickness + padding, content_y, body_width - padding * 2.0f,
                        row_height};
    state->saver_label = {content_x, content_y, content_width, row_height};

    float toggle_width = metrics.toggle_width * scale;
    float toggle_height = metrics.toggle_height * scale;
    state->saver_toggle = {content_x + content_width - toggle_width,
                           content_y + (row_height - toggle_height) * 0.5f, toggle_width,
                           toggle_height};
}

static void reach_battery_configure_motion(reach_battery *battery,
                                           const reach_battery_open_context *ctx)
{
    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    battery->fade_in_spec = reach_theme_motion_preset(ctx->theme, REACH_MOTION_PRESET_FADE_IN);
    battery->smooth_spec = reach_theme_motion_preset(ctx->theme, REACH_MOTION_PRESET_SMOOTH);
    battery->time_reveal_offset = reach_battery_metrics_values.time_reveal_offset * scale;
}

void reach_battery_open(reach_battery *battery, const reach_battery_open_context *ctx)
{
    if (battery == nullptr || ctx == nullptr)
    {
        return;
    }
    reach_battery_reset_pressable(battery);
    reach_battery_place(&battery->state, ctx);
    reach_battery_configure_motion(battery, ctx);
    reach_feature_transition_configure(&battery->popup_transition, ctx->theme, ctx->dpi_scale,
                                       ctx->drop_direction);
    reach_feature_transition_set_trigger(&battery->popup_transition, battery->state.bounds,
                                         battery->state.notch_anchor_x);
    battery->state.open = 1;
    (void)reach_feature_transition_set_open(&battery->popup_transition, 1);
}

void reach_battery_relayout(reach_battery *battery, const reach_battery_open_context *ctx)
{
    if (battery == nullptr || ctx == nullptr || !battery->state.open)
    {
        return;
    }
    reach_battery_place(&battery->state, ctx);
    reach_battery_configure_motion(battery, ctx);
    reach_feature_transition_configure(&battery->popup_transition, ctx->theme, ctx->dpi_scale,
                                       ctx->drop_direction);
    reach_feature_transition_set_trigger(&battery->popup_transition, battery->state.bounds,
                                         battery->state.notch_anchor_x);
}

static void reach_battery_capsule_reset(void *capsule)
{
    reach_battery_reset(static_cast<reach_battery *>(capsule));
}

static int32_t reach_battery_capsule_is_open(const void *capsule)
{
    return reach_battery_is_open(static_cast<const reach_battery *>(capsule));
}

static void reach_battery_capsule_tick(void *capsule, double delta_seconds,
                                       reach_feature_tick_result *out)
{
    reach_battery *battery = static_cast<reach_battery *>(capsule);
    if (battery == nullptr || out == nullptr)
    {
        return;
    }

    if (delta_seconds < 0.0)
    {
        delta_seconds = 0.0;
    }

    if (reach_battery_refresh_power(battery))
    {
        out->redraw = 1;
    }

    int32_t popup_changed =
        reach_feature_transition_tick(&battery->popup_transition, delta_seconds);
    reach_pressable_feedback_style feedback = reach_battery_pressable_feedback(battery);
    reach_pressable_settle_feedback(&battery->pressable, &feedback);
    if (popup_changed)
    {
        out->redraw = 1;
    }
    if (reach_feature_transition_needs_update(&battery->popup_transition))
    {
        out->request_update = 1;
    }

    if (battery->state.model.saver_pending && battery->saver_pending_deadline > 0 &&
        reach_motion_now_seconds() >= battery->saver_pending_deadline)
    {
        reach_battery_set_saver_pending(battery, 0, 0);
        out->redraw = 1;
    }
}

static int32_t reach_battery_capsule_needs_frame(const void *capsule)
{
    const reach_battery *battery = static_cast<const reach_battery *>(capsule);
    return battery != nullptr && reach_feature_transition_needs_update(&battery->popup_transition);
}

static int32_t reach_battery_capsule_presentation_visible(const void *capsule)
{
    const reach_battery *battery = static_cast<const reach_battery *>(capsule);
    return battery != nullptr && reach_feature_transition_visible(&battery->popup_transition);
}

static int32_t reach_battery_capsule_wants_pointer_move(const void *capsule)
{
    const reach_battery *battery = static_cast<const reach_battery *>(capsule);
    return battery != nullptr && reach_pressable_tracking(&battery->pressable);
}

static int32_t reach_battery_capsule_pointer_sequence_active(const void *capsule)
{
    return reach_battery_capsule_wants_pointer_move(capsule);
}

static void reach_battery_capsule_apply_pressable_result(const reach_pressable_result *pressable,
                                                         reach_capsule_pointer_result *out)
{
    if (pressable == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw |= pressable->redraw;
    if (pressable->capture != 0)
    {
        out->capture = pressable->capture;
    }
    out->sync_pointer_subscriptions |= pressable->sync_pointer_subscriptions;
}

static uint64_t reach_battery_pressable_target(reach_battery_pointer_action_kind action)
{
    return action == REACH_BATTERY_POINTER_ACTION_TOGGLE_SAVER
               ? REACH_BATTERY_PRESSABLE_TARGET_SAVER
               : REACH_PRESSABLE_TARGET_NONE;
}

static void reach_battery_toggle_saver(reach_battery *battery)
{
    int32_t target_enabled = reach_battery_model_saver_effective(&battery->state.model) ? 0 : 1;
    const reach_result result =
        reach_system_status_set_battery_saver_enabled(battery->status, target_enabled);
    reach_battery_set_saver_pending(battery, result == REACH_OK, target_enabled);
    (void)reach_battery_refresh_power(battery);
}

static void reach_battery_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                 reach_capsule_pointer_result *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};

    reach_battery *battery = static_cast<reach_battery *>(capsule);
    if (battery == nullptr || event == nullptr || !battery->state.open ||
        event->coordinate_space != REACH_POINTER_COORDINATE_SURFACE_LOCAL)
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
    reach_battery_pointer_action_kind action =
        reach_battery_hit_test(&battery->state, event->x, event->y);
    reach_pressable_feedback_style feedback = reach_battery_pressable_feedback(battery);

    if (event->kind == REACH_POINTER_EVENT_DOWN && event->button == REACH_POINTER_BUTTON_PRIMARY)
    {
        if (action == REACH_BATTERY_POINTER_ACTION_DISMISS)
        {
            out->handled = 1;
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            out->redraw = 1;
            return;
        }
        reach_pressable_result pressable = {};
        uint64_t target = reach_battery_pressable_target(action);
        reach_pressable_press(&battery->pressable, event->button, target,
                              target == REACH_PRESSABLE_TARGET_NONE ? REACH_PRESSABLE_FEEDBACK_NONE
                                                                    : 0,
                              &feedback, &pressable);
        reach_battery_capsule_apply_pressable_result(&pressable, out);
        out->handled = reach_pressable_tracking(&battery->pressable);
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_UP && reach_pressable_tracking(&battery->pressable))
    {
        reach_pressable_result pressable = {};
        reach_pressable_release(&battery->pressable, event->button,
                                reach_battery_pressable_target(action), &feedback, &pressable);
        reach_battery_capsule_apply_pressable_result(&pressable, out);
        out->handled = 1;
        if (pressable.activated)
        {
            reach_battery_toggle_saver(battery);
            out->redraw = 1;
        }
        return;
    }
    if ((event->kind == REACH_POINTER_EVENT_MOVE || event->kind == REACH_POINTER_EVENT_LEAVE) &&
        reach_pressable_tracking(&battery->pressable))
    {
        reach_pressable_result pressable = {};
        reach_pressable_update(&battery->pressable,
                               event->kind == REACH_POINTER_EVENT_MOVE
                                   ? reach_battery_pressable_target(action)
                                   : REACH_PRESSABLE_TARGET_NONE,
                               &pressable);
        reach_battery_capsule_apply_pressable_result(&pressable, out);
        out->handled = 1;
        return;
    }
    if (event->kind == REACH_POINTER_EVENT_CANCEL && reach_pressable_tracking(&battery->pressable))
    {
        reach_pressable_result pressable = {};
        reach_pressable_cancel(&battery->pressable, &feedback, &pressable);
        reach_battery_capsule_apply_pressable_result(&pressable, out);
        out->handled = 1;
    }
}

static void reach_battery_capsule_surface_geometry(const void *capsule,
                                                   reach_feature_surface_geometry *out)
{
    if (capsule == nullptr || out == nullptr)
    {
        return;
    }
    const reach_battery *battery = static_cast<const reach_battery *>(capsule);
    *out = {};
    out->visible_bounds = battery->state.bounds;
    out->envelope_bounds = battery->state.bounds;
    out->notch_anchor_x = battery->state.notch_anchor_x;
    out->notch_side = reach_popup_notch_side(battery->state.drop_direction);
    reach_feature_transition_presentation(&battery->popup_transition, out);
    if (battery->saver_pending_deadline > 0 &&
        (out->deadline_seconds <= 0 || battery->saver_pending_deadline < out->deadline_seconds))
        out->deadline_seconds = battery->saver_pending_deadline;
}

const reach_feature_capsule_ops *reach_battery_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_battery_capsule_reset,
        reach_battery_capsule_tick,
        reach_battery_capsule_is_open,
        nullptr,
        reach_battery_capsule_needs_frame,
        reach_battery_capsule_wants_pointer_move,
        reach_battery_capsule_handle_pointer,
        reach_battery_capsule_pointer_sequence_active,
        nullptr,
        reach_battery_capsule_surface_geometry,
        nullptr,
        nullptr,
        nullptr,
        reach_battery_capsule_presentation_visible,
    };
    return &ops;
}

reach_result reach_battery_create(reach_battery **out_battery)
{
    if (out_battery == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_battery *battery = new (std::nothrow) reach_battery();
    if (battery == nullptr)
    {
        return REACH_ERROR;
    }
    reach_feature_transition_init(&battery->popup_transition, REACH_FEATURE_TRANSITION_FROM_ABOVE);
    reach_pressable_init(&battery->pressable);
    reach_battery_reset(battery);
    *out_battery = battery;
    return REACH_OK;
}

void reach_battery_destroy(reach_battery *battery)
{
    delete battery;
}
