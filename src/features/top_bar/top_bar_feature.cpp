#include "reach/features/top_bar.h"

#include "top_bar_common.h"
#include "top_bar_metrics.h"

#include <new>
#include <stdio.h>
#include <time.h>

const reach_top_bar_state *reach_top_bar_state_ptr(const reach_top_bar *top_bar)
{
    return top_bar != nullptr ? &top_bar->state : nullptr;
}

reach_top_bar_state *reach_top_bar_state_mut(reach_top_bar *top_bar)
{
    return top_bar != nullptr ? &top_bar->state : nullptr;
}

reach_animation_manager *reach_top_bar_manager(reach_top_bar *top_bar)
{
    return top_bar != nullptr ? &top_bar->manager : nullptr;
}

reach_result reach_top_bar_create(reach_top_bar **out_top_bar)
{
    if (out_top_bar == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_top_bar *top_bar = new (std::nothrow) reach_top_bar();
    if (top_bar == nullptr)
    {
        return REACH_ERROR;
    }

    reach_animation_manager_init(&top_bar->manager, top_bar->tracks, REACH_TOP_BAR_ANIM_COUNT);
    top_bar->state.feedback_index = REACH_TOP_BAR_FEEDBACK_NONE;
    top_bar->state.pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
    *out_top_bar = top_bar;
    return REACH_OK;
}

void reach_top_bar_destroy(reach_top_bar *top_bar)
{
    delete top_bar;
}

float reach_top_bar_height(const reach_theme *theme, float dock_height)
{
    if (theme == nullptr || dock_height <= 0.0f)
    {
        return 0.0f;
    }
    return reach_theme_icon_box_size(theme, dock_height) *
           reach_top_bar_metrics_values.height_scale;
}

void reach_top_bar_build_layout(reach_top_bar *top_bar, const reach_top_bar_build_context *ctx)
{
    if (top_bar == nullptr || ctx == nullptr || ctx->theme == nullptr)
    {
        return;
    }

    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    const float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    const float height = reach_top_bar_height(ctx->theme, ctx->dock_height);
    reach_top_bar_layout *layout = &top_bar->state.layout;

    *layout = {};
    layout->bounds.x = ctx->monitor_bounds.x;
    layout->bounds.width = ctx->monitor_bounds.width;
    layout->bounds.y = ctx->monitor_bounds.y + metrics.screen_gap * scale;
    layout->bounds.height = height;

    if (height <= 0.0f || layout->bounds.width <= 0.0f)
    {
        return;
    }

    const float edge_inset = metrics.edge_inset * scale;
    const float pill_gap = metrics.pill_gap * scale;
    const float padding = metrics.pill_padding * scale;
    const float power_button_size = height * metrics.power_button_scale;
    const float clock_gap = metrics.clock_gap * scale;
    const float clock_width = metrics.clock_width * scale;

    float power_clock_width =
        padding * 2.0f + power_button_size + clock_gap + clock_width;
    float left = edge_inset;
    layout->pills[REACH_TOP_BAR_PILL_POWER_CLOCK] =
        reach_top_bar_rect(left, 0.0f, power_clock_width, height);
    layout->power_button =
        reach_top_bar_rect(left + padding, (height - power_button_size) * 0.5f, power_button_size,
                           power_button_size);
    layout->clock = reach_top_bar_rect(layout->power_button.x + power_button_size + clock_gap,
                                       0.0f, clock_width, height);

    left += power_clock_width + pill_gap;
    layout->pills[REACH_TOP_BAR_PILL_NOW_PLAYING] =
        reach_top_bar_rect(left, 0.0f, metrics.now_playing_collapsed_width * scale, height);

    float right = layout->bounds.width - edge_inset;
    float quick_settings_width = metrics.quick_settings_width * scale;
    layout->pills[REACH_TOP_BAR_PILL_QUICK_SETTINGS] =
        reach_top_bar_rect(right - quick_settings_width, 0.0f, quick_settings_width, height);
    right = layout->pills[REACH_TOP_BAR_PILL_QUICK_SETTINGS].x - pill_gap;
    float tray_width = metrics.tray_width * scale;
    layout->pills[REACH_TOP_BAR_PILL_TRAY] =
        reach_top_bar_rect(right - tray_width, 0.0f, tray_width, height);

    float current_app_width = metrics.current_app_width * scale;
    float current_app_max_width = layout->bounds.width * metrics.current_app_max_width_ratio;
    if (current_app_width > current_app_max_width)
    {
        current_app_width = current_app_max_width;
    }
    layout->pills[REACH_TOP_BAR_PILL_CURRENT_APP] = reach_top_bar_rect(
        (layout->bounds.width - current_app_width) * 0.5f, 0.0f, current_app_width, height);

    for (size_t index = 0; index < REACH_TOP_BAR_PILL_COUNT; ++index)
    {
        layout->pill_visible[index] = layout->pills[index].width > 0.0f ? 1 : 0;
    }
}

reach_point_i32 reach_top_bar_local_point(const reach_top_bar_layout *layout, int32_t x, int32_t y)
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

reach_rect_f32 reach_top_bar_rect_to_screen(const reach_top_bar_layout *layout, reach_rect_f32 rect)
{
    if (layout == nullptr)
    {
        return rect;
    }
    rect.x += layout->bounds.x;
    rect.y += layout->bounds.y;
    return rect;
}

size_t reach_top_bar_input_region_count(const reach_top_bar *top_bar)
{
    if (top_bar == nullptr)
    {
        return 0;
    }

    size_t count = 0;
    for (size_t index = 0; index < REACH_TOP_BAR_PILL_COUNT; ++index)
    {
        if (top_bar->state.layout.pill_visible[index])
        {
            ++count;
        }
    }
    return count;
}

reach_rect_f32 reach_top_bar_input_region_at(const reach_top_bar *top_bar, size_t index)
{
    if (top_bar == nullptr)
    {
        return reach_rect_f32{};
    }

    size_t visible = 0;
    for (size_t pill = 0; pill < REACH_TOP_BAR_PILL_COUNT; ++pill)
    {
        if (!top_bar->state.layout.pill_visible[pill])
        {
            continue;
        }
        if (visible == index)
        {
            return top_bar->state.layout.pills[pill];
        }
        ++visible;
    }
    return reach_rect_f32{};
}

reach_top_bar_pointer_region reach_top_bar_pointer_region_at(const reach_top_bar *top_bar,
                                                             int32_t local_x, int32_t local_y)
{
    if (top_bar == nullptr)
    {
        return REACH_TOP_BAR_POINTER_REGION_NONE;
    }
    return reach_top_bar_hit_test(&top_bar->state.layout, local_x, local_y);
}

int32_t reach_top_bar_pointer_sequence_active(const reach_top_bar *top_bar)
{
    return top_bar != nullptr && top_bar->state.pointer_sequence_active;
}

void reach_top_bar_suppress_power_release(reach_top_bar *top_bar)
{
    if (top_bar != nullptr)
    {
        top_bar->state.power_release_suppressed = 1;
    }
}

static int32_t reach_top_bar_take_power_release_suppressed(reach_top_bar *top_bar)
{
    if (top_bar == nullptr || !top_bar->state.power_release_suppressed)
    {
        return 0;
    }
    top_bar->state.power_release_suppressed = 0;
    return 1;
}

static void reach_top_bar_copy_ascii_to_utf16(uint16_t *dst, size_t dst_count, const char *src)
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

static int32_t reach_top_bar_utf16_equal(const uint16_t *a, const uint16_t *b)
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

int32_t reach_top_bar_update_clock(reach_top_bar *top_bar)
{
    if (top_bar == nullptr)
    {
        return 0;
    }

    reach_top_bar_state *state = &top_bar->state;

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
    reach_top_bar_copy_ascii_to_utf16(next_time, 32, time_text);
    reach_top_bar_copy_ascii_to_utf16(next_date, 64, date_text);
    int32_t redraw = 0;
    if (!state->clock_initialized ||
        !reach_top_bar_utf16_equal(state->clock_time_text, next_time) ||
        !reach_top_bar_utf16_equal(state->clock_date_text, next_date))
    {
        reach_copy_utf16(state->clock_time_text, 32, next_time);
        reach_copy_utf16(state->clock_date_text, 64, next_date);
        state->clock_initialized = 1;
        redraw = 1;
    }
    state->clock_last_minute = current_minute;
    return redraw;
}

void reach_top_bar_begin_reveal_session(reach_top_bar *top_bar)
{
    if (top_bar != nullptr)
    {
        reach_bar_begin_reveal_session(&top_bar->state.visibility);
    }
}

reach_bar_visibility_result
reach_top_bar_update_visibility(reach_top_bar *top_bar,
                                const reach_bar_visibility_request *request)
{
    if (top_bar == nullptr || request == nullptr)
    {
        return reach_bar_visibility_result{};
    }

    reach_bar_visibility_request bar_request = *request;
    bar_request.edge = REACH_BAR_EDGE_TOP;
    bar_request.pointer_sequence_active = top_bar->state.pointer_sequence_active;

    return reach_bar_update_visibility(&top_bar->state.visibility, &top_bar->manager,
                                       REACH_TOP_BAR_ANIM_Y, &bar_request);
}

static void reach_top_bar_capsule_reset(void *capsule)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr)
    {
        return;
    }
    reach_bar_visibility_reset(&top_bar->state.visibility);
    top_bar->state.pointer_sequence_active = 0;
    top_bar->state.pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
    top_bar->state.feedback_index = REACH_TOP_BAR_FEEDBACK_NONE;
    top_bar->state.feedback_pressed = 0;
    top_bar->state.power_hovered = 0;
    top_bar->state.power_release_suppressed = 0;
}

static void reach_top_bar_capsule_tick(void *capsule, double delta_seconds,
                                       reach_feature_tick_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr)
    {
        return;
    }

    reach_animation_manager *manager = &top_bar->manager;
    int32_t feedback_was_active =
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY);
    int32_t power_hover_was_active =
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_POWER_HOVER);

    reach_animation_manager_tick(manager, delta_seconds);

    int32_t redraw =
        feedback_was_active ||
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) ||
        power_hover_was_active ||
        reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_POWER_HOVER);

    if (feedback_was_active &&
        !reach_animation_manager_active(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) &&
        !top_bar->state.feedback_pressed &&
        reach_animation_manager_value(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY) <= 0.001f)
    {
        reach_animation_manager_set(manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY, 0.0f);
        top_bar->state.feedback_index = REACH_TOP_BAR_FEEDBACK_NONE;
    }

    if (redraw && out != nullptr)
    {
        out->redraw = 1;
    }
}

static int32_t reach_top_bar_capsule_is_open(const void *capsule)
{
    (void)capsule;
    return 1;
}

static int32_t reach_top_bar_capsule_needs_frame(const void *capsule)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    if (top_bar == nullptr)
    {
        return 0;
    }
    return reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_Y) ||
           reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_POWER_HOVER) ||
           reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY);
}

static int32_t reach_top_bar_capsule_wants_pointer_move(const void *capsule)
{
    (void)capsule;
    return 1;
}

static void reach_top_bar_capsule_handle_pointer(void *capsule, const reach_pointer_event *event,
                                                 reach_capsule_pointer_result *out)
{
    if (out != nullptr)
    {
        *out = {};
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    if (top_bar == nullptr || event == nullptr || out == nullptr)
    {
        return;
    }

    reach_top_bar_state *state = &top_bar->state;
    reach_point_i32 local = reach_top_bar_local_point(&state->layout, event->x, event->y);
    reach_top_bar_pointer_region hit =
        reach_top_bar_hit_test(&state->layout, local.x, local.y);

    if (event->kind == REACH_POINTER_EVENT_DOWN)
    {
        if (!state->pointer_sequence_active)
        {
            state->pointer_sequence_active = 1;
            out->sync_pointer_subscriptions = 1;
        }
        if (hit != REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON)
        {
            state->power_release_suppressed = 0;
        }
        state->pressed_control = hit;
        if (hit == REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON)
        {
            out->redraw = reach_top_bar_feedback_press(top_bar,
                                                       REACH_TOP_BAR_FEEDBACK_POWER_BUTTON);
            out->handled = 1;
            out->action.kind = REACH_TOP_BAR_POINTER_ACTION_PRESS_POWER;
        }
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_UP)
    {
        out->redraw = reach_top_bar_feedback_release(top_bar);
        reach_top_bar_pointer_region pressed =
            static_cast<reach_top_bar_pointer_region>(state->pressed_control);
        state->pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
        if (pressed == REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON && hit == pressed)
        {
            out->handled = 1;
            if (!reach_top_bar_take_power_release_suppressed(top_bar))
            {
                out->action.kind = REACH_TOP_BAR_POINTER_ACTION_TOGGLE_POWER;
            }
        }
        if (state->pointer_sequence_active)
        {
            state->pointer_sequence_active = 0;
            out->sync_pointer_subscriptions = 1;
        }
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_MOVE)
    {
        int32_t hovered = hit == REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON;
        if (hovered != state->power_hovered)
        {
            state->power_hovered = hovered;
            reach_animation_manager_animate_to(&top_bar->manager, REACH_TOP_BAR_ANIM_POWER_HOVER,
                                               hovered ? 1.0f : 0.0f, 0.18,
                                               REACH_EASING_EASE_IN_OUT);
            out->handled = 1;
            out->redraw = 1;
        }
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_CANCEL)
    {
        out->redraw = reach_top_bar_feedback_release(top_bar);
        state->pressed_control = REACH_TOP_BAR_POINTER_REGION_NONE;
        if (state->pointer_sequence_active)
        {
            state->pointer_sequence_active = 0;
            out->sync_pointer_subscriptions = 1;
        }
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_LEAVE)
    {
        if (state->power_hovered)
        {
            state->power_hovered = 0;
            reach_animation_manager_animate_to(&top_bar->manager, REACH_TOP_BAR_ANIM_POWER_HOVER,
                                               0.0f, 0.18, REACH_EASING_EASE_IN_OUT);
            out->redraw = 1;
        }
    }
}

const reach_feature_capsule_ops *reach_top_bar_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_top_bar_capsule_reset,
        reach_top_bar_capsule_tick,
        reach_top_bar_capsule_is_open,
        nullptr,
        nullptr,
        reach_top_bar_capsule_needs_frame,
        reach_top_bar_capsule_wants_pointer_move,
        reach_top_bar_capsule_handle_pointer,
    };
    return &ops;
}
