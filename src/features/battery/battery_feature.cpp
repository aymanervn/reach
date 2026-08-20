#include "battery_common.h"

#include <new>

static const double REACH_BATTERY_SAVER_PENDING_TIMEOUT_SECONDS = 2.0;

const reach_battery_state *reach_battery_state_ptr(const reach_battery *battery)
{
    return battery != nullptr ? &battery->state : nullptr;
}

int32_t reach_battery_is_open(const reach_battery *battery)
{
    return battery != nullptr && battery->state.open;
}

void reach_battery_force_close(reach_battery *battery)
{
    if (battery != nullptr)
    {
        battery->state.open = 0;
    }
}

void reach_battery_reset(reach_battery *battery)
{
    if (battery == nullptr)
    {
        return;
    }
    battery->state = {};
    battery->saver_pending_seconds = 0.0;
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
    battery->state.model.saver_pending = pending ? 1 : 0;
    battery->state.model.saver_pending_enabled = pending_enabled ? 1 : 0;
    battery->saver_pending_seconds = 0.0;
}

int32_t reach_battery_saver_pending(const reach_battery *battery)
{
    return battery != nullptr && battery->state.model.saver_pending;
}

static void reach_battery_place(reach_battery_state *state,
                                const reach_battery_open_context *ctx)
{
    const reach_battery_metrics &metrics = reach_battery_metrics_values;
    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;

    state->drop_direction = ctx->drop_direction;

    float padding = metrics.padding * scale;
    float row_height = metrics.row_height * scale;
    float row_gap = metrics.row_gap * scale;
    float row_inset = metrics.row_inset * scale;
    float separator_height = metrics.separator_height * scale;
    float separator_inset = metrics.separator_inset * scale;
    float popup_width = metrics.popup_width * scale;
    float margin = metrics.screen_margin * scale;
    float notch_height = reach_popup_notch_height_scaled(scale);

    float body_height = padding * 2.0f + row_height * 2.0f + row_gap * 2.0f + separator_height;
    float popup_height = body_height + notch_height;

    reach_popup_anchor anchor = {};
    anchor.button = ctx->anchor_button;
    anchor.monitor = ctx->monitor;
    anchor.bar_edge_y = ctx->bar_edge_y;
    anchor.direction = ctx->drop_direction;

    reach_popup_placement placement =
        reach_popup_place(&anchor, popup_width, popup_height, margin);
    state->bounds = placement.bounds;
    state->notch_anchor_x = placement.notch_anchor_x;

    float content_y =
        padding + (ctx->drop_direction == REACH_POPUP_DROP_DOWN ? notch_height : 0.0f);
    float content_x = row_inset;
    float content_width = popup_width - row_inset * 2.0f;

    state->percent_label = {content_x, content_y, content_width, row_height};
    content_y += row_height + row_gap;

    state->separator = {separator_inset, content_y, popup_width - separator_inset * 2.0f,
                        separator_height};
    content_y += separator_height + row_gap;

    state->saver_row = {padding, content_y, popup_width - padding * 2.0f, row_height};
    state->saver_label = {content_x, content_y, content_width, row_height};

    float toggle_width = metrics.toggle_width * scale;
    float toggle_height = metrics.toggle_height * scale;
    state->saver_toggle = {content_x + content_width - toggle_width,
                           content_y + (row_height - toggle_height) * 0.5f, toggle_width,
                           toggle_height};
}

void reach_battery_open(reach_battery *battery, const reach_battery_open_context *ctx)
{
    if (battery == nullptr || ctx == nullptr)
    {
        return;
    }
    reach_battery_place(&battery->state, ctx);
    battery->state.open = 1;
}

void reach_battery_relayout(reach_battery *battery, const reach_battery_open_context *ctx)
{
    if (battery == nullptr || ctx == nullptr || !battery->state.open)
    {
        return;
    }
    reach_battery_place(&battery->state, ctx);
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

    if (!battery->state.model.saver_pending)
    {
        return;
    }

    if (delta_seconds < 0.0)
    {
        delta_seconds = 0.0;
    }
    battery->saver_pending_seconds += delta_seconds;

    if (battery->saver_pending_seconds >= REACH_BATTERY_SAVER_PENDING_TIMEOUT_SECONDS)
    {
        reach_battery_set_saver_pending(battery, 0, 0);
        out->redraw = 1;
        return;
    }

    out->request_update = 1;
}

static int32_t reach_battery_capsule_needs_frame(const void *capsule)
{
    const reach_battery *battery = static_cast<const reach_battery *>(capsule);
    return battery != nullptr && battery->state.open && battery->state.model.saver_pending;
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
    if (battery == nullptr || event == nullptr || !battery->state.open)
    {
        return;
    }
    if (event->kind != REACH_POINTER_EVENT_DOWN)
    {
        return;
    }

    reach_battery_pointer_action_kind action =
        reach_battery_hit_test(&battery->state, event->x, event->y);
    if (action == REACH_BATTERY_POINTER_ACTION_NONE)
    {
        return;
    }

    out->handled = 1;
    out->action.kind = (uint32_t)action;
    out->redraw = 1;
}

const reach_feature_capsule_ops *reach_battery_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_battery_capsule_reset,
        reach_battery_capsule_tick,
        reach_battery_capsule_is_open,
        nullptr,
        reach_battery_capsule_needs_frame,
        nullptr,
        reach_battery_capsule_handle_pointer,
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
    reach_battery_reset(battery);
    *out_battery = battery;
    return REACH_OK;
}

void reach_battery_destroy(reach_battery *battery)
{
    delete battery;
}
