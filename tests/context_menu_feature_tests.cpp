#include "reach/features/context_menu.h"

#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static int text_equals_ascii(const uint16_t *text, const char *expected)
{
    size_t index = 0;
    while (expected[index] != 0)
    {
        if (text[index] != (uint16_t)(unsigned char)expected[index])
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0;
}

static void test_power_commands_and_text(void)
{
    uint32_t commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    uint32_t icons[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t count = 0;
    reach_context_menu_build_power_commands(commands, icons, &count);

    expect_true(count == 6, "power menu exposes six commands");
    expect_true(commands[0] == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK, "lock is first command");
    expect_true(commands[3] == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN,
                "shutdown command is fourth");
    expect_true(commands[5] == REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS,
                "settings command is last");
    expect_true(icons[0] == REACH_VECTOR_ICON_LOCK, "lock command has lock icon");
    expect_true(icons[5] == REACH_VECTOR_ICON_SETTINGS, "settings command has settings icon");
    expect_true(text_equals_ascii(reach_context_menu_command_text(commands[0]), "Lock"),
                "lock command text is stable");
    expect_true(text_equals_ascii(reach_context_menu_command_text(commands[5]), "Settings"),
                "settings command text is stable");
    expect_true(text_equals_ascii(reach_context_menu_command_text(0), ""),
                "unknown command text is empty");
}

static void test_window_list_remove(void)
{
    reach_context_menu *menu = nullptr;
    if (reach_context_menu_create(&menu) != REACH_OK || menu == nullptr)
    {
        ++failures;
        fprintf(stderr, "FAILED: context menu creation\n");
        return;
    }

    reach_context_menu_window_entry entries[3] = {};
    entries[0].window = 11;
    entries[0].title = (const uint16_t *)L"one";
    entries[1].window = 22;
    entries[1].title = (const uint16_t *)L"two";
    entries[2].window = 33;
    entries[2].title = (const uint16_t *)L"three";

    reach_context_menu_open_context ctx = {};
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.anchor_x = 400.0f;
    ctx.bar_edge_y = 1000.0f;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.window_entries = entries;
    ctx.window_entry_count = 3;
    reach_context_menu_open_window_list(menu, 0, &ctx);

    const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);
    expect_true(state->item_count == 3, "window list opens with every entry");

    size_t remaining = reach_context_menu_window_list_remove(menu, 22);
    expect_true(remaining == 2, "removing an entry drops the item count");
    expect_true(state->item_windows[0] == 11 && state->item_windows[1] == 33,
                "remaining entries keep their order");
    expect_true(text_equals_ascii(state->item_titles[1], "three"),
                "titles shift with their windows");
    expect_true(state->item_windows[2] == 0, "trailing entry is cleared");

    expect_true(reach_context_menu_window_list_remove(menu, 99) == 2,
                "removing an unknown window changes nothing");

    reach_context_menu_destroy(menu);
}

int main(void)
{
    test_power_commands_and_text();
    test_window_list_remove();
    return failures == 0 ? 0 : 1;
}
