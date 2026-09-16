#include "top_bar_now_playing.h"

#include <cmath>
#include <cstdio>

static int failures = 0;

static void expect_true(bool condition, const char *message)
{
    if (!condition)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void expect_near(double actual, double expected, double tolerance, const char *message)
{
    expect_true(std::fabs(actual - expected) <= tolerance, message);
}

static const reach_render_command *find_command(const reach_render_command_buffer &commands,
                                                reach_render_command_type type)
{
    for (size_t index = 0; index < commands.count; ++index)
    {
        if (commands.commands[index].type == type)
        {
            return &commands.commands[index];
        }
    }
    return nullptr;
}

struct now_playing_fixture
{
    reach_theme theme = {};
    reach_top_bar_now_playing_model model = {};
    reach_top_bar_now_playing_layout layout = {};

    now_playing_fixture()
    {
        model.visible = 1;
        model.line[0] = 'T';
        layout.bounds = {0.0f, 0.0f, 320.0f, 48.0f};
        layout.cover = {0.0f, 0.0f, 48.0f, 48.0f};
        layout.text = {50.0f, 0.0f, 100.0f, 48.0f};
        layout.text_advance = 152.0f;
        layout.previous_button = {160.0f, 0.0f, 40.0f, 48.0f};
        layout.play_pause_button = {200.0f, 0.0f, 40.0f, 48.0f};
        layout.next_button = {240.0f, 0.0f, 40.0f, 48.0f};
    }

    reach_top_bar_now_playing_render_input input(int32_t animate_text) const
    {
        reach_top_bar_now_playing_render_input value = {};
        value.theme = &theme;
        value.model = &model;
        value.layout = &layout;
        value.dpi_scale = 1.0f;
        value.animate_text = animate_text;
        return value;
    }
};

static void test_overflow_uses_one_gpu_animation()
{
    now_playing_fixture fixture;
    reach_top_bar_now_playing_render_input input = fixture.input(1);
    reach_render_command_buffer commands = {};

    expect_true(reach_top_bar_now_playing_build_render_commands(&input, &commands) == REACH_OK,
                "GPU render commands build");
    const reach_render_command *animated =
        find_command(commands, REACH_RENDER_COMMAND_ANIMATED_TEXT);
    expect_true(animated != nullptr, "overflow uses one animated text layer");
    if (animated != nullptr)
    {
        expect_near(animated->animation_offset_x, -52.0, 0.0001,
                    "animation travels the exact overflow");
        expect_near(animated->animation_hold_seconds, 1.6, 0.0001,
                    "animation retains its endpoint hold");
        expect_near(animated->animation_travel_seconds, 2.0, 0.0001,
                    "animation retains its movement speed");
    }
    expect_true(find_command(commands, REACH_RENDER_COMMAND_TEXT) == nullptr,
                "GPU path does not draw duplicate text");
}

static void test_unavailable_gpu_animation_stays_static()
{
    now_playing_fixture fixture;
    reach_top_bar_now_playing_render_input input = fixture.input(0);
    reach_render_command_buffer commands = {};

    expect_true(reach_top_bar_now_playing_build_render_commands(&input, &commands) == REACH_OK,
                "static render commands build");
    const reach_render_command *text = find_command(commands, REACH_RENDER_COMMAND_TEXT);
    expect_true(text != nullptr, "unavailable GPU animation leaves clipped static text");
    if (text != nullptr)
    {
        expect_near(text->rect.x, 50.0, 0.0001, "static text never receives a CPU offset");
    }
    expect_true(find_command(commands, REACH_RENDER_COMMAND_ANIMATED_TEXT) == nullptr,
                "static path emits no animation command");
}

int main()
{
    test_overflow_uses_one_gpu_animation();
    test_unavailable_gpu_animation_stays_static();
    if (failures != 0)
    {
        std::fprintf(stderr, "%d Now Playing animation test(s) failed\n", failures);
        return 1;
    }
    std::printf("Now Playing animation tests passed\n");
    return 0;
}
