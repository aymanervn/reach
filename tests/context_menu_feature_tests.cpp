#include "reach/features/context_menu.h"
#include "reach/features/common/window_preview_layout.h"
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

    reach_menu_request request = {};
    request.window_count = 3;
    request.windows[0].window = 11;
    request.windows[1].window = 22;
    request.windows[2].window = 33;
    reach_copy_utf16(request.windows[0].title, 260, (const uint16_t *)L"one");
    reach_copy_utf16(request.windows[1].title, 260, (const uint16_t *)L"two");
    reach_copy_utf16(request.windows[2].title, 260, (const uint16_t *)L"three");

    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.anchor_button = {380.0f, 960.0f, 40.0f, 40.0f};
    ctx.bar_edge_y = 1000.0f;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.request = &request;
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
    expect_true(result.handled && result.continue_source_sequence &&
                    !result.cancel_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_NONE,
                "the power menu preserves its primary owner sequence until toggle release");

    pointer.owner_trigger = 0;
    reach_context_menu_capsule_ops()->handle_pointer(menu, &pointer, &result);
    expect_true(result.handled && result.cancel_source_sequence &&
                    !result.continue_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_CLOSE_SELF,
                "the context menu closes and cancels an arbitrary outside primary press");

    pointer.button = REACH_POINTER_BUTTON_SECONDARY;
    reach_context_menu_capsule_ops()->handle_pointer(menu, &pointer, &result);
    expect_true(result.handled && result.continue_source_sequence &&
                    !result.cancel_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_CLOSE_SELF,
                "the context menu closes before continuing an outside secondary press");

    reach_menu_request request = {};
    request.window_count = 1;
    request.windows[0].window = 11;
    reach_copy_utf16(request.windows[0].title, 260, (const uint16_t *)L"one");
    ctx.anchor_button = {700.0f, 1000.0f, 40.0f, 40.0f};
    ctx.bar_edge_y = 1000.0f;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.request = &request;
    reach_context_menu_open_window_list(menu, 3, &ctx);
    pointer.button = REACH_POINTER_BUTTON_PRIMARY;
    pointer.owner_trigger = 1;
    reach_context_menu_capsule_ops()->handle_pointer(menu, &pointer, &result);
    expect_true(result.handled && result.continue_source_sequence &&
                    !result.cancel_source_sequence &&
                    result.action.kind == REACH_FEATURE_ACTION_CLOSE_SELF,
                "the passive window list closes without cancelling its Dock owner sequence");

    reach_context_menu_destroy(menu);
}

static reach_menu_request make_request(void)
{
    reach_menu_request request = {};
    request.pin_id = 0;
    request.window = 4242;
    request.commands[0] = REACH_CONTEXT_MENU_COMMAND_OPEN_NEW;
    request.commands[1] = REACH_CONTEXT_MENU_COMMAND_CLOSE_ALL;
    request.command_count = 2;
    request.path[0] = 'a';
    request.path[1] = 0;
    request.window_count = 2;
    request.windows[0].window = 11;
    request.windows[1].window = 22;
    return request;
}

static void test_menu_commands_execute_only_from_the_published_snapshot(void)
{
    reach_menu_request request = make_request();
    uintptr_t windows[REACH_MENU_MAX_WINDOWS] = {11, 22};

    reach_capsule_action allowed = {};
    reach_context_menu_command_action(&request, windows, REACH_CONTEXT_MENU_COMMAND_OPEN_NEW,
                                      &allowed);
    expect_true(allowed.kind == REACH_FEATURE_ACTION_OPEN_TARGET,
                "a published command resolves to a complete target");
    expect_true(allowed.target.path == request.path &&
                    (allowed.flags & REACH_FEATURE_ACTION_FLAG_NEW_INSTANCE) != 0,
                "open-new carries the request path and asks for a new instance");
    expect_true((allowed.flags & REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST) != 0,
                "acting on a menu command dismisses the menu first");

    reach_capsule_action withheld = {};
    reach_context_menu_command_action(&request, windows, REACH_CONTEXT_MENU_COMMAND_UNPIN,
                                      &withheld);
    expect_true(withheld.kind == REACH_FEATURE_ACTION_NONE,
                "a command the owner never published is refused");

    reach_capsule_action close_all = {};
    reach_context_menu_command_action(&request, windows, REACH_CONTEXT_MENU_COMMAND_CLOSE_ALL,
                                      &close_all);
    expect_true(close_all.kind == REACH_FEATURE_ACTION_CLOSE_WINDOWS &&
                    close_all.window_count == 2 && close_all.windows == windows,
                "close-all carries the window set captured when the menu opened");

    request.window_count = 0;
    reach_capsule_action close_one = {};
    reach_context_menu_command_action(&request, windows, REACH_CONTEXT_MENU_COMMAND_CLOSE_ALL,
                                      &close_one);
    expect_true(close_one.kind == REACH_FEATURE_ACTION_CLOSE_WINDOW && close_one.window == 4242,
                "close-all falls back to the item's own window when the set is empty");

    reach_capsule_action power = {};
    reach_context_menu_command_action(&request, windows, REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP,
                                      &power);
    expect_true(power.kind == REACH_FEATURE_ACTION_EXECUTE_MENU_COMMAND &&
                    power.id == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP,
                "power commands stay system actions and skip the item snapshot");
}

static void test_window_list_holds_the_pointer_across_the_gap(void)
{
    reach_context_menu *menu = nullptr;
    if (reach_context_menu_create(&menu) != REACH_OK || menu == nullptr)
    {
        ++failures;
        fprintf(stderr, "FAILED: context menu creation\n");
        return;
    }

    expect_true(reach_context_menu_window_list_target(menu) == REACH_CONTEXT_MENU_NO_TARGET,
                "a closed menu reports no window-list target");
    expect_true(!reach_context_menu_window_list_holds_pointer(menu, 400.0f, 900.0f),
                "a closed menu never holds the pointer");

    reach_menu_request request = {};
    request.target_index = 3;
    request.anchored = 1;
    request.anchor_button = {380.0f, 960.0f, 40.0f, 40.0f};
    request.bar_edge_y = 1000.0f;
    request.drop_direction = REACH_POPUP_DROP_UP;

    request.window_count = 2;
    request.windows[0].window = 11;
    request.windows[1].window = 22;
    reach_copy_utf16(request.windows[0].title, 260, (const uint16_t *)L"one");
    reach_copy_utf16(request.windows[1].title, 260, (const uint16_t *)L"two");

    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.anchored = request.anchored;
    ctx.anchor_button = request.anchor_button;
    ctx.bar_edge_y = request.bar_edge_y;
    ctx.drop_direction = request.drop_direction;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.request = &request;
    reach_context_menu_open_window_list(menu, request.target_index, &ctx);

    const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);
    expect_true(reach_context_menu_window_list_target(menu) == 3,
                "an open window list reports the item it belongs to");

    float inside_x = state->bounds.x + state->bounds.width * 0.5f;
    float inside_y = state->bounds.y + state->bounds.height * 0.5f;
    expect_true(reach_context_menu_window_list_holds_pointer(menu, inside_x, inside_y),
                "the pointer inside the list holds it open");

    float gap_y = state->bounds.y + state->bounds.height + 4.0f;
    expect_true(reach_context_menu_window_list_holds_pointer(menu, 400.0f, gap_y),
                "the pointer in the gap between the item and the list holds it open");

    expect_true(!reach_context_menu_window_list_holds_pointer(menu, 40.0f, 200.0f),
                "the pointer well away from both dismisses the list");

    /* Reach is told about the pointer leaving this band and nothing else, so anything the hold
       test still accepts has to be inside it. A hole here is a window list that can never be
       dismissed once the pointer is over another application. */
    reach_rect_f32 watch = {};
    expect_true(reach_context_menu_window_list_hover_bounds(menu, &watch),
                "an open window list publishes a band for the platform to watch");
    int32_t escaped = 0;
    for (float sample_x = 200.0f; sample_x <= 700.0f; sample_x += 5.0f)
    {
        for (float sample_y = 700.0f; sample_y <= 1060.0f; sample_y += 5.0f)
        {
            if (!reach_context_menu_window_list_holds_pointer(menu, sample_x, sample_y))
            {
                continue;
            }
            if (sample_x < watch.x || sample_x > watch.x + watch.width || sample_y < watch.y ||
                sample_y > watch.y + watch.height)
            {
                escaped = 1;
            }
        }
    }
    expect_true(!escaped, "every point that holds the list open lies inside the watched band");

    reach_context_menu_destroy(menu);
}

static reach_window_snapshot preview_windows[REACH_MENU_MAX_WINDOWS];
static reach_rect_f32 preview_frames[REACH_MENU_MAX_WINDOWS];
static size_t preview_window_count;

static void reset_preview_windows(size_t count)
{
    preview_window_count = count;
    for (size_t index = 0; index < count; ++index)
    {
        preview_windows[index] = {};
        preview_windows[index].id = index + 1;
        preview_windows[index].title[0] = 'A';
        preview_frames[index] = {0, 0, 1600, 900};
    }
}

static size_t preview_count(const reach_window_manager *)
{
    return preview_window_count;
}

static reach_result preview_at(const reach_window_manager *, size_t index,
                               reach_window_snapshot *out)
{
    if (index >= preview_window_count)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out = preview_windows[index];
    return REACH_OK;
}

static reach_result preview_frame(const reach_window_manager *, reach_window_id window,
                                  reach_rect_f32 *out)
{
    if (window == 0 || window > REACH_MENU_MAX_WINDOWS)
    {
        return REACH_ERROR;
    }
    *out = preview_frames[window - 1];
    return REACH_OK;
}

static void advance_preview_layout(reach_context_menu *menu,
                                   const reach_context_menu_open_context *ctx, double seconds)
{
    reach_feature_tick_result tick = {};
    reach_context_menu_capsule_ops()->tick(menu, seconds, &tick);
    reach_context_menu_reanchor(menu, ctx);
}

static void test_preview_geometry_and_paging_across_dpi(void)
{
    expect_near(reach_window_preview_aspect({0, 0, 600, 1200}), 0.5f, 0.001f,
                "portrait geometry keeps its aspect");
    expect_near(reach_window_preview_aspect({0, 0, NAN, 1200}), 1.0f, 0.001f,
                "invalid geometry uses a finite fallback");
    reach_menu_request request = {};
    request.window_count = REACH_MENU_MAX_WINDOWS;
    reset_preview_windows(request.window_count);
    for (size_t index = 0; index < request.window_count; ++index)
    {
        request.windows[index].window = index + 1;
    }
    preview_frames[1] = {0, 0, 600, 1200};
    preview_frames[2] = {0, 0, 2400, 900};
    reach_window_manager_port port = {};
    port.ops.window_count = preview_count;
    port.ops.window_at = preview_at;
    port.ops.frame_bounds = preview_frame;
    reach_window_tracking *windows = nullptr;
    expect_true(reach_window_tracking_create(port, &windows) == REACH_OK, "preview cache exists");
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu *menu = nullptr;
    reach_context_menu_create(&menu);
    reach_context_menu_attach_services(menu, windows, nullptr);
    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.anchored = 1;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.request = &request;
    const float scales[] = {1.0f, 1.25f, 1.5f, 2.0f};
    const reach_rect_f32 monitors[] = {{-100, 50, 1920, 1080},
                                       {100, 50, 2560, 1440},
                                       {100, -200, 3840, 2160},
                                       {10, 20, 640, 360},
                                       {10, 20, 320, 180}};
    for (reach_rect_f32 monitor : monitors)
    {
        for (float scale : scales)
        {
            ctx.monitor = monitor;
            ctx.dpi_scale = scale;
            ctx.anchor_button = {monitor.x + monitor.width * 0.5f, monitor.y + monitor.height - 40,
                                 40, 40};
            ctx.bar_edge_y = ctx.anchor_button.y;
            reach_context_menu_open_window_list(menu, 0, &ctx);
            advance_preview_layout(menu, &ctx, 0.2);
            expect_true(reach_context_menu_state_ptr(menu)->item_count == REACH_MENU_MAX_WINDOWS,
                        "preview capacity is independent of command capacity");
            bool seen[REACH_MENU_MAX_WINDOWS] = {};
            float common_height = 0.0f;
            size_t pages = 0;
            do
            {
                advance_preview_layout(menu, &ctx, 0.2);
                const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);
                expect_true(state->bounds.x >= monitor.x && state->bounds.y >= monitor.y &&
                                state->bounds.x + state->bounds.width <=
                                    monitor.x + monitor.width + 0.01f &&
                                state->bounds.y + state->bounds.height <=
                                    monitor.y + monitor.height + 0.01f,
                            "preview popup fits its monitor");
                for (size_t index = 0; index < reach_context_menu_thumbnail_count(menu); ++index)
                {
                    reach_window_id window = 0;
                    reach_rect_f32 rect = {};
                    reach_context_menu_thumbnail_at(menu, index, &window, &rect);
                    expect_true(window >= 1 && window <= REACH_MENU_MAX_WINDOWS,
                                "visible preview has its source identity");
                    if (window == 0 || window > REACH_MENU_MAX_WINDOWS)
                    {
                        continue;
                    }
                    seen[window - 1] = true;
                    if (common_height == 0.0f)
                    {
                        common_height = rect.height;
                    }
                    expect_near(rect.height, common_height, 0.01f,
                                "every page retains a common preview height");
                    expect_near(rect.width / rect.height,
                                reach_window_preview_aspect(preview_frames[window - 1]), 0.001f,
                                "each preview retains its source ratio");
                    expect_true(
                        rect.x >= state->bounds.x && rect.y >= state->bounds.y &&
                            rect.x + rect.width <= state->bounds.x + state->bounds.width + 0.01f &&
                            rect.y + rect.height <= state->bounds.y + state->bounds.height + 0.01f,
                        "complete previews fit inside the popup");
                }
                ++pages;
            } while (pages < REACH_MENU_MAX_WINDOWS &&
                     reach_context_menu_window_list_page(menu, 1));
            for (bool visited : seen)
            {
                expect_true(visited, "paging reaches every supplied window");
            }
            if (monitor.height >= 1080.0f)
            {
                expect_near(common_height, 140.0f * scale, 0.01f,
                            "unconstrained preview height is DPI-scaled DP");
            }
            for (size_t page = 1; page < pages; ++page)
            {
                expect_true(reach_context_menu_window_list_page(menu, -1),
                            "previous page returns through the same page sequence");
            }
            expect_true(reach_context_menu_state_ptr(menu)->visible_start == 0,
                        "previous navigation reaches the first page");
        }
    }
    reach_context_menu_destroy(menu);
    reach_window_tracking_destroy(windows);
}

static void test_preview_membership_refresh_and_reanchor(void)
{
    reset_preview_windows(3);
    reach_window_manager_port port = {};
    port.ops.window_count = preview_count;
    port.ops.window_at = preview_at;
    port.ops.frame_bounds = preview_frame;
    reach_window_tracking *windows = nullptr;
    reach_window_tracking_create(port, &windows);
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu *menu = nullptr;
    reach_context_menu_create(&menu);
    reach_context_menu_attach_services(menu, windows, nullptr);
    reach_menu_request request = {};
    request.window_count = 3;
    request.anchored = 1;
    request.drop_direction = REACH_POPUP_DROP_UP;
    request.anchor_button = {400, 1000, 40, 40};
    request.bar_edge_y = 1000;
    for (size_t index = 0; index < request.window_count; ++index)
    {
        request.windows[index].window = index + 1;
    }
    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.monitor = {0, 0, 1920, 1080};
    ctx.dpi_scale = 1.0f;
    ctx.anchored = request.anchored;
    ctx.drop_direction = request.drop_direction;
    ctx.anchor_button = request.anchor_button;
    ctx.bar_edge_y = request.bar_edge_y;
    ctx.request = &request;
    reach_context_menu_open_window_list(menu, 0, &ctx);
    size_t generation = reach_context_menu_preview_generation(menu);
    ctx.dpi_scale = 1.5f;
    ctx.anchor_button = {1500, 900, 40, 40};
    ctx.bar_edge_y = 900;
    reach_context_menu_reanchor(menu, &ctx);
    expect_true(reach_context_menu_preview_generation(menu) == generation,
                "DPI and anchor changes reuse source membership");
    expect_true(reach_context_menu_window_list_holds_pointer(menu, 1510, 920),
                "hold region follows the current anchor");
    expect_true(!reach_context_menu_window_list_holds_pointer(menu, 410, 1020),
                "hold region retires the old anchor");
    reach_feature_tick_result tick = {};
    preview_windows[1].minimized = 1;
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu_notify_windows_changed(menu, &tick);
    reach_window_id window = 0;
    reach_rect_f32 rect = {};
    reach_context_menu_thumbnail_at(menu, 1, &window, &rect);
    expect_true(window == 0 && reach_context_menu_preview_generation(menu) != generation,
                "minimization retires the native source");
    preview_windows[1].minimized = 0;
    preview_frames[1] = {0, 0, 1200, 600};
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu_notify_windows_changed(menu, &tick);
    reach_context_menu_thumbnail_at(menu, 1, &window, &rect);
    expect_true(window == 2, "restoration republishes the live source");
    expect_near(rect.width / rect.height, 2.0f, 0.001f, "resize uses refreshed source geometry");
    preview_window_count = 2;
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu_notify_windows_changed(menu, &tick);
    expect_true(reach_context_menu_state_ptr(menu)->item_count == 2,
                "departed windows disappear without a new hover");
    preview_window_count = 3;
    reach_window_tracking_refresh(windows, nullptr);
    request.target_index = 4;
    request.anchor_button = ctx.anchor_button;
    request.bar_edge_y = ctx.bar_edge_y;
    reach_context_menu_refresh_window_list(menu, &request);
    expect_true(reach_context_menu_is_open(menu) &&
                    reach_context_menu_window_list_target(menu) == 4 &&
                    reach_context_menu_state_ptr(menu)->item_count == 3,
                "same-owner refresh updates its index and added sources without closing");
    ctx.monitor = {0, 0, 640, 360};
    ctx.anchor_button = {300, 320, 40, 40};
    ctx.bar_edge_y = 320;
    reach_context_menu_reanchor(menu, &ctx);
    while (reach_context_menu_window_list_page(menu, 1))
    {
    }
    expect_true(reach_context_menu_state_ptr(menu)->visible_start > 0,
                "constrained sources have later pages");
    preview_window_count = 2;
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu_notify_windows_changed(menu, &tick);
    expect_true(reach_context_menu_state_ptr(menu)->visible_start < 2 &&
                    reach_context_menu_thumbnail_count(menu) > 0,
                "departure from the last page clamps to surviving sources");
    preview_window_count = 0;
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu_notify_windows_changed(menu, &tick);
    expect_true(!reach_context_menu_is_open(menu), "the last departure closes the popup");
    reach_context_menu_destroy(menu);
    reach_window_tracking_destroy(windows);
}

static void expect_preview_animation_geometry(const reach_context_menu *menu)
{
    const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);
    float height = 0.0f;
    for (size_t index = 0; index < reach_context_menu_thumbnail_count(menu); ++index)
    {
        reach_window_id window = 0;
        reach_rect_f32 rect = {};
        reach_context_menu_thumbnail_at(menu, index, &window, &rect);
        if (index == 0)
        {
            height = rect.height;
        }
        expect_near(rect.height, height, 0.01f, "animated previews have a common height");
        expect_true(rect.width > 0.0f && rect.height > 0.0f && rect.x >= state->bounds.x &&
                        rect.y >= state->bounds.y &&
                        rect.x + rect.width <= state->bounds.x + state->bounds.width + 0.01f &&
                        rect.y + rect.height <= state->bounds.y + state->bounds.height + 0.01f,
                    "animated previews fit completely inside current popup bounds");
        if (window > 0 && window <= REACH_MENU_MAX_WINDOWS && rect.height > 0.0f)
        {
            expect_near(rect.width / rect.height,
                        reach_window_preview_aspect(preview_frames[window - 1]), 0.001f,
                        "animated previews preserve source proportions");
        }
    }
}

static void test_preview_width_animation_retarget_and_reset(void)
{
    reset_preview_windows(3);
    preview_frames[0] = {0, 0, 600, 1200};
    preview_frames[2] = {0, 0, 2400, 900};
    reach_window_manager_port port = {};
    port.ops.window_count = preview_count;
    port.ops.window_at = preview_at;
    port.ops.frame_bounds = preview_frame;
    reach_window_tracking *windows = nullptr;
    reach_window_tracking_create(port, &windows);
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu *menu = nullptr;
    reach_context_menu_create(&menu);
    reach_context_menu_attach_services(menu, windows, nullptr);
    reach_menu_request request = {};
    for (size_t index = 0; index < 3; ++index)
    {
        request.windows[index].window = index + 1;
    }
    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.monitor = {-100, 50, 1920, 1080};
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.anchor_button = {-80, 1050, 40, 40};
    ctx.bar_edge_y = 1050;
    ctx.request = &request;
    request.window_count = 1;
    reach_context_menu_open_window_list(menu, 0, &ctx);
    float narrow = reach_context_menu_state_ptr(menu)->bounds.width;
    request.window_count = 3;
    reach_context_menu_open_window_list(menu, 1, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, narrow, 0.01f,
                "owner replacement starts from the displayed width");
    size_t generation = reach_context_menu_preview_generation(menu);
    expect_preview_animation_geometry(menu);
    advance_preview_layout(menu, &ctx, 0.06);
    float expanding = reach_context_menu_state_ptr(menu)->bounds.width;
    expect_true(expanding > narrow, "manager ticks advance popup expansion");
    expect_true(reach_context_menu_preview_generation(menu) == generation,
                "width frames preserve visible native membership");
    expect_preview_animation_geometry(menu);
    request.window_count = 1;
    reach_context_menu_open_window_list(menu, 2, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, expanding, 0.01f,
                "rapid retargeting preserves the current width");
    advance_preview_layout(menu, &ctx, 0.03);
    float shrinking = reach_context_menu_state_ptr(menu)->bounds.width;
    expect_true(shrinking > narrow && shrinking < expanding,
                "interrupted expansion smoothly reverses toward the narrow target");
    expect_preview_animation_geometry(menu);
    request.window_count = 3;
    reach_context_menu_open_window_list(menu, 3, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, shrinking, 0.01f,
                "a second interruption preserves continuity");
    advance_preview_layout(menu, &ctx, 0.2);
    float wide = reach_context_menu_state_ptr(menu)->bounds.width;
    expect_true(wide > expanding, "animation reaches the full row width");
    expect_preview_animation_geometry(menu);
    reach_window_id window = 0;
    reach_rect_f32 rect = {};
    reach_context_menu_thumbnail_at(menu, 0, &window, &rect);
    expect_near(rect.height, 140.0f, 0.01f, "settled previews return to nominal height");
    request.window_count = 1;
    reach_context_menu_open_window_list(menu, 4, &ctx);
    advance_preview_layout(menu, &ctx, 0.03);
    float closing = reach_context_menu_state_ptr(menu)->bounds.width;
    reach_context_menu_set_open(menu, 0);
    advance_preview_layout(menu, &ctx, 0.2);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, closing, 0.01f,
                "close presentation freezes the current width");
    reach_context_menu_force_close(menu);
    reach_context_menu_open_window_list(menu, 0, &ctx);
    expect_near(reach_context_menu_state_ptr(menu)->bounds.width, narrow, 0.01f,
                "a fresh presentation does not inherit an old width track");
    request.window_count = 3;
    reach_context_menu_open_window_list(menu, 1, &ctx);
    advance_preview_layout(menu, &ctx, 0.03);
    ctx.dpi_scale = 1.5f;
    ctx.monitor = {100, -100, 640, 360};
    ctx.anchor_button = {680, 220, 40, 40};
    ctx.bar_edge_y = 220;
    reach_context_menu_reanchor(menu, &ctx);
    const reach_context_menu_state *state = reach_context_menu_state_ptr(menu);
    expect_true(state->bounds.x >= ctx.monitor.x && state->bounds.y >= ctx.monitor.y &&
                    state->bounds.x + state->bounds.width <=
                        ctx.monitor.x + ctx.monitor.width + 0.01f,
                "display and DPI changes cut to a monitor-safe layout");
    expect_preview_animation_geometry(menu);
    reach_context_menu_destroy(menu);
    reach_window_tracking_destroy(windows);
}

static float preview_screen_x(const reach_context_menu *menu, reach_window_id source)
{
    for (size_t index = 0; index < reach_context_menu_thumbnail_count(menu); ++index)
    {
        reach_window_id window = 0;
        reach_rect_f32 rect = {};
        reach_context_menu_thumbnail_at(menu, index, &window, &rect);
        if (window == source)
        {
            return rect.x;
        }
    }
    expect_true(0, "the requested preview source is published");
    return 0.0f;
}

static void test_preview_reflow_preserves_surviving_sources(void)
{
    reset_preview_windows(3);
    preview_frames[0] = {0, 0, 600, 1200};
    preview_frames[2] = {0, 0, 2400, 900};
    reach_window_manager_port port = {};
    port.ops.window_count = preview_count;
    port.ops.window_at = preview_at;
    port.ops.frame_bounds = preview_frame;
    reach_window_tracking *windows = nullptr;
    reach_window_tracking_create(port, &windows);
    reach_window_tracking_refresh(windows, nullptr);
    reach_context_menu *menu = nullptr;
    reach_context_menu_create(&menu);
    reach_context_menu_attach_services(menu, windows, nullptr);
    reach_menu_request request = {};
    request.window_count = 3;
    for (size_t index = 0; index < request.window_count; ++index)
    {
        request.windows[index].window = index + 1;
    }
    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.monitor = {0, 0, 1920, 1080};
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.anchor_button = {940, 1000, 40, 40};
    ctx.bar_edge_y = 1000;
    ctx.request = &request;
    reach_context_menu_open_window_list(menu, 0, &ctx);
    advance_preview_layout(menu, &ctx, 0.2);
    float initial = preview_screen_x(menu, 3);
    reach_context_menu_window_list_remove(menu, 2);
    expect_near(preview_screen_x(menu, 3), initial, 0.01f,
                "removing a middle source preserves the survivor's displayed position");
    size_t generation = reach_context_menu_preview_generation(menu);
    advance_preview_layout(menu, &ctx, 0.06);
    float moving = preview_screen_x(menu, 3);
    expect_true(moving < initial, "a survivor moves into the removed source's space over time");
    expect_true(reach_context_menu_preview_generation(menu) == generation,
                "reflow frames preserve native source membership");
    expect_preview_animation_geometry(menu);
    request.anchored = ctx.anchored;
    request.anchor_button = ctx.anchor_button;
    request.bar_edge_y = ctx.bar_edge_y;
    request.drop_direction = ctx.drop_direction;
    reach_context_menu_refresh_window_list(menu, &request);
    expect_near(preview_screen_x(menu, 3), moving, 0.01f,
                "an addition during reflow retargets from the survivor's current position");
    reach_context_menu_reanchor(menu, &ctx);
    expect_near(preview_screen_x(menu, 3), moving, 0.01f,
                "arranging twice without ticking does not advance or restart reflow");
    for (size_t frame = 0; frame < 12; ++frame)
    {
        advance_preview_layout(menu, &ctx, 0.016);
        expect_preview_animation_geometry(menu);
    }
    expect_near(preview_screen_x(menu, 3), initial, 0.01f,
                "interrupted reflow settles back to the original packed row");
    reach_context_menu_window_list_remove(menu, 2);
    advance_preview_layout(menu, &ctx, 0.04);
    float closing = preview_screen_x(menu, 3);
    reach_context_menu_set_open(menu, 0);
    advance_preview_layout(menu, &ctx, 0.2);
    expect_near(preview_screen_x(menu, 3), closing, 0.01f,
                "closing freezes the currently arranged preview position");
    reach_context_menu_force_close(menu);
    reach_context_menu_open_window_list(menu, 0, &ctx);
    expect_near(preview_screen_x(menu, 3), initial, 0.01f,
                "fresh presentation retires previous reflow origins");
    reach_context_menu_window_list_remove(menu, 2);
    advance_preview_layout(menu, &ctx, 0.04);
    ctx.dpi_scale = 1.5f;
    reach_context_menu_reanchor(menu, &ctx);
    float constrained = preview_screen_x(menu, 3);
    advance_preview_layout(menu, &ctx, 0.2);
    expect_near(preview_screen_x(menu, 3), constrained, 0.01f,
                "a DPI cut clears stale reflow origins and width motion");
    expect_preview_animation_geometry(menu);
    reach_context_menu_destroy(menu);
    reach_window_tracking_destroy(windows);
}

int main(void)
{
    test_window_list_holds_the_pointer_across_the_gap();
    test_menu_commands_execute_only_from_the_published_snapshot();
    test_power_commands_and_text();
    test_window_list_remove();
    test_preview_geometry_and_paging_across_dpi();
    test_preview_membership_refresh_and_reanchor();
    test_preview_width_animation_retarget_and_reset();
    test_preview_reflow_preserves_surviving_sources();
    test_capsule_owns_popup_pointer_policy();
    return failures == 0 ? 0 : 1;
}
