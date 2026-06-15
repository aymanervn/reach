#include "reach/features/dock.h"
#include "reach/features/context_menu.h"

#include <math.h>
#include <stdio.h>

static int expect(int condition)
{
    return condition ? 0 : 1;
}

static int expect_near_at(float actual, float expected, float epsilon, int line)
{
    if (fabsf(actual - expected) > epsilon)
    {
        printf("FAIL: dock_feature_tests.cpp:%d expected %.3f got %.3f\n", line, expected, actual);
        return 1;
    }
    return 0;
}

#define expect_near(actual, expected, epsilon)                                                     \
    expect_near_at((actual), (expected), (epsilon), __LINE__)

int main(void)
{
    int failed = 0;

    reach_dock_feature_model model = {};
    model.item_count = 3;

    reach_dock_layout layout = {};
    layout.app_slot_count = 3;
    layout.app_slots[0] = {10.0f, 0.0f, 40.0f, 40.0f};
    layout.app_slots[1] = {62.0f, 0.0f, 40.0f, 40.0f};
    layout.app_slots[2] = {114.0f, 0.0f, 40.0f, 40.0f};

    failed += expect(reach_dock_reorder_target(&model, &layout, 0, 51.5f) == 0);
    failed += expect(reach_dock_reorder_target(&model, &layout, 0, 52.0f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 103.5f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 104.0f) == 2);

    failed += expect(reach_dock_reorder_target(&model, &layout, 2, 72.5f) == 2);
    failed += expect(reach_dock_reorder_target(&model, &layout, 2, 72.0f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 20.0f) == 0);

    failed += expect(reach_dock_reorder_target(&model, &layout, 3, 10.0f) == REACH_MAX_PINNED_APPS);

    layout.tray_button = {166.0f, 0.0f, 40.0f, 40.0f};
    layout.quick_settings_button = {218.0f, 0.0f, 40.0f, 40.0f};
    layout.system_separator = {270.0f, 8.0f, 1.0f, 24.0f};
    layout.clock = {283.0f, 0.0f, 92.0f, 40.0f};
    layout.power_button = {387.0f, 0.0f, 40.0f, 40.0f};
    failed +=
        expect(layout.tray_button.x + layout.tray_button.width < layout.quick_settings_button.x);
    failed += expect(layout.quick_settings_button.x + layout.quick_settings_button.width <
                     layout.system_separator.x);
    failed += expect(layout.system_separator.x + layout.system_separator.width < layout.clock.x);
    failed += expect(layout.clock.x + layout.clock.width < layout.power_button.x);
    failed += expect(reach_dock_hit_test(&layout, 180, 20).type == REACH_DOCK_HIT_TRAY_BUTTON);
    failed +=
        expect(reach_dock_hit_test(&layout, 230, 20).type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON);
    failed += expect(reach_dock_hit_test(&layout, 400, 20).type == REACH_DOCK_HIT_POWER_BUTTON);
    failed += expect(reach_dock_hit_test(&layout, 30, 20).type == REACH_DOCK_HIT_ITEM);

    layout.bounds = {0.0f, 0.0f, 440.0f, 56.0f};
    reach_dock_icon_cache icon_cache = {};
    reach_dock_render_input dock_render = {};
    dock_render.theme = reach_theme_default();
    dock_render.layout = &layout;
    dock_render.model = &model;
    dock_render.icons = &icon_cache;
    dock_render.dragged_render_index = REACH_MAX_PINNED_APPS;
    dock_render.tray_feedback_index = REACH_MAX_PINNED_APPS;
    dock_render.quick_settings_feedback_index = REACH_MAX_PINNED_APPS + 1;
    dock_render.power_feedback_index = REACH_MAX_PINNED_APPS + 2;

    reach_render_command_buffer render_commands = {};
    failed += expect(reach_dock_build_render_commands(&dock_render, &render_commands) == REACH_OK);
    const reach_render_command *tray_arrow_icon = nullptr;
    const reach_render_command *settings_icon = nullptr;
    for (size_t index = 0; index < render_commands.count; ++index)
    {
        if (render_commands.commands[index].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            render_commands.commands[index].icon_id == REACH_VECTOR_ICON_ARROW_UP)
        {
            tray_arrow_icon = &render_commands.commands[index];
        }
        if (render_commands.commands[index].type == REACH_RENDER_COMMAND_VECTOR_ICON &&
            render_commands.commands[index].icon_id == REACH_VECTOR_ICON_SETTINGS)
        {
            settings_icon = &render_commands.commands[index];
        }
    }
    failed += expect(tray_arrow_icon != nullptr);
    failed += expect(settings_icon != nullptr);
    if (tray_arrow_icon != nullptr && settings_icon != nullptr)
    {
        failed += expect(tray_arrow_icon->rect.width > 0.0f);
        failed += expect(settings_icon->rect.width > 0.0f);
        failed += expect_near(tray_arrow_icon->rect.height, tray_arrow_icon->rect.width, 0.001f);
        failed += expect_near(settings_icon->rect.height, settings_icon->rect.width, 0.001f);
        failed += expect_near(settings_icon->color.r,
                              dock_render.theme->icon_backplate_background.r, 0.001f);
        failed += expect_near(settings_icon->color.g,
                              dock_render.theme->icon_backplate_background.g, 0.001f);
        failed += expect_near(settings_icon->color.b,
                              dock_render.theme->icon_backplate_background.b, 0.001f);
        failed += expect_near(settings_icon->color.a,
                              dock_render.theme->icon_backplate_background.a, 0.001f);
        failed += expect_near(tray_arrow_icon->color.r,
                              dock_render.theme->icon_backplate_background.r, 0.001f);
        failed += expect_near(tray_arrow_icon->color.g,
                              dock_render.theme->icon_backplate_background.g, 0.001f);
        failed += expect_near(tray_arrow_icon->color.b,
                              dock_render.theme->icon_backplate_background.b, 0.001f);
        failed += expect_near(tray_arrow_icon->color.a,
                              dock_render.theme->icon_backplate_background.a, 0.001f);
    }

    uint32_t commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    uint32_t icons[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t count = 0;
    reach_context_menu_build_power_commands(commands, icons, &count);
    failed += expect(count == 6);
    failed += expect(commands[0] == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK);
    failed += expect(commands[1] == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP);
    failed += expect(commands[2] == REACH_CONTEXT_MENU_COMMAND_POWER_RESTART);
    failed += expect(commands[3] == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN);
    failed += expect(commands[4] == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT);
    failed += expect(commands[5] == REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS);
    failed += expect(icons[0] == REACH_VECTOR_ICON_LOCK);
    failed += expect(icons[4] == REACH_VECTOR_ICON_SIGN_OUT);
    failed += expect(icons[5] == REACH_VECTOR_ICON_SETTINGS);

    reach_rect_f32 slots[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    for (size_t index = 0; index < count; ++index)
    {
        slots[index] = {10.0f, 10.0f + 34.0f * (float)index, 120.0f, 34.0f};
    }
    reach_context_menu_hit_result hit =
        reach_context_menu_hit_test_items(slots, count, 20, 10 + 34 * 3 + 10);
    reach_context_menu_action action = reach_context_menu_action_for_hit(commands, count, hit);
    failed += expect(action.command == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN);

    return failed == 0 ? 0 : 1;
}
