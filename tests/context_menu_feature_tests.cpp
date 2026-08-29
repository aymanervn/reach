#include "reach/features/context_menu.h"
#include "test_utf16.h"

#include <math.h>
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

static void expect_near(float actual, float expected, float tolerance, const char *message)
{
    if (fabsf(actual - expected) > tolerance)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s (expected %.3f, got %.3f)\n", message, expected, actual);
    }
}

struct test_text_measure
{
    float unit_width;
};

static reach_result measure_text(void *context, const uint16_t *text, float text_size,
                                 int32_t text_weight, float *out_width)
{
    (void)text_size;
    (void)text_weight;
    if (context == nullptr || text == nullptr || out_width == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    size_t length = 0;
    while (text[length] != 0)
    {
        ++length;
    }
    *out_width = (float)length * static_cast<test_text_measure *>(context)->unit_width;
    return REACH_OK;
}

static void test_power_commands_and_text(void)
{
    uint32_t commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    uint32_t icons[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t count = 0;
    reach_context_menu_build_power_commands(commands, icons, &count);

    expect_true(count == 5, "power menu exposes five commands");
    expect_true(commands[0] == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK, "lock is first command");
    expect_true(commands[3] == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN,
                "shutdown command is fourth");
    expect_true(commands[4] == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT,
                "sign out is the last command");
    expect_true(icons[0] == REACH_VECTOR_ICON_LOCK, "lock command has lock icon");
    expect_true(icons[4] == REACH_VECTOR_ICON_SIGN_OUT, "sign out command has sign out icon");
    expect_true(reach_test_utf16_equals_ascii(reach_context_menu_command_text(commands[0]), "Lock"),
                "lock command text is stable");
    expect_true(
        reach_test_utf16_equals_ascii(reach_context_menu_command_text(commands[4]), "Sign out"),
        "sign out command text is stable");
    expect_true(reach_test_utf16_equals_ascii(reach_context_menu_command_text(0), ""),
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
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.anchor_button = {380.0f, 960.0f, 40.0f, 40.0f};
    ctx.bar_edge_y = 1000.0f;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.window_entries = entries;
    ctx.window_entry_count = 3;
    reach_context_menu_open_window_list(menu, 0, &ctx);

    const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);
    expect_true(state->item_count == 3, "window list opens with every entry");
    expect_true(state->item_slots[0].x >= 0.0f && state->item_slots[0].y >= 0.0f &&
                    state->item_slots[0].x + state->item_slots[0].width <= state->bounds.width &&
                    state->item_slots[0].y + state->item_slots[0].height <= state->bounds.height,
                "window-list items use surface-local coordinates");

    size_t remaining = reach_context_menu_window_list_remove(menu, 22);
    expect_true(remaining == 2, "removing an entry drops the item count");
    expect_true(state->item_windows[0] == 11 && state->item_windows[1] == 33,
                "remaining entries keep their order");
    expect_true(reach_test_utf16_equals_ascii(state->item_titles[1], "three"),
                "titles shift with their windows");
    expect_true(state->item_windows[2] == 0, "trailing entry is cleared");

    expect_true(reach_context_menu_window_list_remove(menu, 99) == 2,
                "removing an unknown window changes nothing");

    reach_context_menu_destroy(menu);
}

static void test_window_list_width_fits_and_clamps_measured_titles(void)
{
    reach_context_menu *menu = nullptr;
    if (reach_context_menu_create(&menu) != REACH_OK || menu == nullptr)
    {
        ++failures;
        fprintf(stderr, "FAILED: context menu creation for sizing\n");
        return;
    }

    test_text_measure measure = {10.0f};
    reach_context_menu_window_entry entry = {11, (const uint16_t *)L"Reach"};
    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.anchor_button = {380.0f, 960.0f, 40.0f, 40.0f};
    ctx.bar_edge_y = 1000.0f;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.window_entries = &entry;
    ctx.window_entry_count = 1;
    ctx.text_measure.context = &measure;
    ctx.text_measure.measure = measure_text;

    reach_context_menu_open_window_list(menu, 0, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, 90.0f, 0.0001f,
                "window list fits measured title and close-button chrome");

    entry.title =
        (const uint16_t *)L"A title long enough to exceed the configured preview maximum width";
    reach_context_menu_open_window_list(menu, 0, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, 322.0f, 0.0001f,
                "window list clamps long titles to its maximum width");

    ctx.monitor.width = 180.0f;
    reach_context_menu_reanchor(menu, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, 164.0f, 0.0001f,
                "window list maximum respects monitor margins");

    entry.title = (const uint16_t *)L"A";
    ctx.monitor.width = 1920.0f;
    reach_context_menu_open_window_list(menu, 0, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, 50.0f, 0.0001f,
                "window list minimum fits one letter and close-button chrome");

    reach_theme wide_border_theme = *reach_theme_default();
    wide_border_theme.border_thickness = 3.0f;
    ctx.theme = &wide_border_theme;
    reach_context_menu_open_window_list(menu, 0, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, 54.0f, 0.0001f,
                "window list outer width tracks arbitrary border thickness");

    reach_context_menu_destroy(menu);
}

static void test_capsule_owns_popup_pointer_policy(void)
{
    reach_context_menu *menu = nullptr;
    expect_true(reach_context_menu_create(&menu) == REACH_OK,
                "context menu is created for popup policy");
    if (menu == nullptr)
    {
        return;
    }

    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.anchor_button = {1200.0f, 8.0f, 32.0f, 24.0f};
    ctx.bar_edge_y = 40.0f;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    reach_context_menu_open_power(menu, &ctx);

    reach_pointer_event pointer = {};
    pointer.kind = REACH_POINTER_EVENT_DOWN;
    pointer.coordinate_space = REACH_POINTER_COORDINATE_SURFACE_LOCAL;
    pointer.surface_relation = REACH_POINTER_SURFACE_OUTSIDE;
    pointer.button = REACH_POINTER_BUTTON_PRIMARY;
    pointer.owner_trigger = 1;
    reach_capsule_pointer_result result = {};
    reach_context_menu_capsule_ops()->handle_pointer(menu, &pointer, &result);
    expect_true(result.handled && result.cancel_source_sequence &&
                    !result.continue_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_CLOSE_SELF,
                "the context menu closes and cancels its primary owner sequence");

    pointer.button = REACH_POINTER_BUTTON_SECONDARY;
    reach_context_menu_capsule_ops()->handle_pointer(menu, &pointer, &result);
    expect_true(result.handled && result.continue_source_sequence &&
                    !result.cancel_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_CLOSE_SELF,
                "the context menu closes before continuing an outside secondary press");

    reach_context_menu_destroy(menu);
}

int main(void)
{
    test_power_commands_and_text();
    test_window_list_remove();
    test_window_list_width_fits_and_clamps_measured_titles();
    test_capsule_owns_popup_pointer_policy();
    return failures == 0 ? 0 : 1;
}
