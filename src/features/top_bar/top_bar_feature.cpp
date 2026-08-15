#include "reach/features/top_bar.h"

#include "top_bar_common.h"
#include "top_bar_metrics.h"

#include <new>

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

    float left = edge_inset;
    layout->pills[REACH_TOP_BAR_PILL_POWER_CLOCK] =
        reach_top_bar_rect(left, 0.0f, metrics.power_clock_width * scale, height);
    left += layout->pills[REACH_TOP_BAR_PILL_POWER_CLOCK].width + pill_gap;
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
    reach_animation_manager_tick(&top_bar->manager, delta_seconds);
}

static int32_t reach_top_bar_capsule_is_open(const void *capsule)
{
    (void)capsule;
    return 1;
}

static int32_t reach_top_bar_capsule_needs_frame(const void *capsule)
{
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    return top_bar != nullptr &&
           reach_animation_manager_active(&top_bar->manager, REACH_TOP_BAR_ANIM_Y);
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
    (void)capsule;
    (void)event;
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
