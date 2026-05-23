#include "reach/features/dock.h"
#include "reach/features/context_menu.h"

static int expect(int condition)
{
    return condition ? 0 : 1;
}

int main(void)
{
    int failed = 0;

    reach_dock_feature_model model = {};
    model.item_count = 3;

    reach_dock_layout layout = {};
    layout.app_slot_count = 3;
    layout.app_slots[0] = { 10.0f, 0.0f, 40.0f, 40.0f };
    layout.app_slots[1] = { 62.0f, 0.0f, 40.0f, 40.0f };
    layout.app_slots[2] = { 114.0f, 0.0f, 40.0f, 40.0f };

    failed += expect(reach_dock_reorder_target(&model, &layout, 0, 51.5f) == 0);
    failed += expect(reach_dock_reorder_target(&model, &layout, 0, 52.0f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 103.5f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 104.0f) == 2);

    failed += expect(reach_dock_reorder_target(&model, &layout, 2, 72.5f) == 2);
    failed += expect(reach_dock_reorder_target(&model, &layout, 2, 72.0f) == 1);
    failed += expect(reach_dock_reorder_target(&model, &layout, 1, 20.0f) == 0);

    failed += expect(reach_dock_reorder_target(&model, &layout, 3, 10.0f) == REACH_MAX_PINNED_APPS);

    layout.tray_button = { 166.0f, 0.0f, 40.0f, 40.0f };
    layout.quick_settings_button = { 218.0f, 0.0f, 40.0f, 40.0f };
    layout.system_separator = { 270.0f, 8.0f, 1.0f, 24.0f };
    layout.clock = { 283.0f, 0.0f, 92.0f, 40.0f };
    layout.power_button = { 387.0f, 0.0f, 40.0f, 40.0f };
    failed += expect(layout.tray_button.x + layout.tray_button.width < layout.quick_settings_button.x);
    failed += expect(layout.quick_settings_button.x + layout.quick_settings_button.width < layout.system_separator.x);
    failed += expect(layout.system_separator.x + layout.system_separator.width < layout.clock.x);
    failed += expect(layout.clock.x + layout.clock.width < layout.power_button.x);
    failed += expect(reach_dock_hit_test(&layout, 180, 20).type == REACH_DOCK_HIT_TRAY_BUTTON);
    failed += expect(reach_dock_hit_test(&layout, 230, 20).type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON);
    failed += expect(reach_dock_hit_test(&layout, 400, 20).type == REACH_DOCK_HIT_POWER_BUTTON);
    failed += expect(reach_dock_hit_test(&layout, 30, 20).type == REACH_DOCK_HIT_ITEM);

    uint32_t commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    uint32_t icons[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t count = 0;
    reach_context_menu_build_power_commands(commands, icons, &count);
    failed += expect(count == 5);
    failed += expect(commands[0] == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK);
    failed += expect(commands[1] == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP);
    failed += expect(commands[2] == REACH_CONTEXT_MENU_COMMAND_POWER_RESTART);
    failed += expect(commands[3] == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN);
    failed += expect(commands[4] == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT);
    failed += expect(icons[0] == REACH_VECTOR_ICON_LOCK);
    failed += expect(icons[4] == REACH_VECTOR_ICON_SIGN_OUT);

    reach_rect_f32 slots[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    for (size_t index = 0; index < count; ++index) {
        slots[index] = { 10.0f, 10.0f + 34.0f * (float)index, 120.0f, 34.0f };
    }
    reach_context_menu_hit_result hit = reach_context_menu_hit_test_items(slots, count, 20, 10 + 34 * 3 + 10);
    reach_context_menu_action action = reach_context_menu_action_for_hit(commands, count, hit);
    failed += expect(action.command == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN);

    return failed == 0 ? 0 : 1;
}
