#include "motion_clock_test_support.h"
#include "reach/features/widgets.h"

#include <cstdio>
#include <memory>

static reach_result sample_clock(reach_clock_source *, reach_clock_sample *sample)
{
    *sample = {2026, 1, 1, 4, 12, 0, 0, 0};
    return REACH_OK;
}

int main()
{
    reach_clock_port clock_port = {};
    clock_port.ops.sample_local = sample_clock;
    reach_clock *clock = nullptr;
    if (reach_clock_create(clock_port, &clock) != REACH_OK)
        return 1;
    const std::unique_ptr<reach_clock, decltype(&reach_clock_destroy)> clock_owner(
        clock, reach_clock_destroy);
    reach_clock_tick(clock);

    reach_widgets *widgets = nullptr;
    if (reach_widgets_create(&widgets) != REACH_OK)
        return 1;
    const std::unique_ptr<reach_widgets, decltype(&reach_widgets_destroy)> widgets_owner(
        widgets, reach_widgets_destroy);
    reach_widgets_attach_services(widgets, clock, nullptr, nullptr);
    reach_widgets_set_enabled(widgets, REACH_DESKTOP_WIDGET_MASK);
    for (size_t index = 0; index < REACH_MAX_DESKTOP_WIDGETS; ++index)
    {
        const auto &limits = reach_widget_definition_at(index)->limits;
        reach_widgets_set_placement(widgets, index,
                                    {40 + static_cast<int32_t>(index % 3) * 480,
                                     60 + static_cast<int32_t>(index / 3) * 440, limits.max_width,
                                     limits.max_height});
    }
    reach_widgets_arrange_desktop(widgets, {100, 200, 1920, 1080}, {100, 248, 1920, 952}, 1);

    // Include the drop preview by placing the dragged clock over the system monitor.
    const auto *ops = reach_widgets_desktop_capsule_ops();
    reach_pointer_event pointer = {};
    pointer.kind = REACH_POINTER_EVENT_DOWN;
    pointer.button = REACH_POINTER_BUTTON_PRIMARY;
    pointer.x = 160;
    pointer.y = 280;
    reach_capsule_pointer_result pointer_result = {};
    ops->handle_pointer(widgets, &pointer, &pointer_result);
    pointer.kind = REACH_POINTER_EVENT_MOVE;
    pointer.x += 480;
    ops->handle_pointer(widgets, &pointer, &pointer_result);
    reach_test_motion_clock_advance(1);
    reach_feature_tick_result tick = {};
    ops->tick(widgets, 0, &tick);

    auto buffer = std::make_unique<reach_render_command_buffer>();
    auto render_fits = [&](size_t expected_groups)
    {
        reach_render_command_buffer_clear(buffer.get());
        if (reach_widgets_render_desktop(widgets, nullptr, 1, nullptr, buffer.get()) == REACH_OK &&
            buffer->count < REACH_MAX_RENDER_COMMANDS &&
            buffer->group_count < REACH_MAX_RENDER_GROUPS &&
            buffer->group_count == expected_groups &&
            reach_render_command_buffer_validate_groups(buffer.get()) == REACH_OK)
            return true;
        std::fprintf(stderr, "Invalid widget render buffer: %zu commands, %zu groups\n",
                     buffer->count, buffer->group_count);
        return false;
    };
    if (!render_fits(REACH_MAX_DESKTOP_WIDGETS + 1))
        return 1;

    pointer.kind = REACH_POINTER_EVENT_UP;
    ops->handle_pointer(widgets, &pointer, &pointer_result);
    return render_fits(REACH_MAX_DESKTOP_WIDGETS) &&
                   reach_render_command_buffer_motion_active(buffer.get())
               ? 0
               : 1;
}
