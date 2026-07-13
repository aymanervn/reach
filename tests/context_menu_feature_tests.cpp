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

int main(void)
{
    test_power_commands_and_text();
    return failures == 0 ? 0 : 1;
}
