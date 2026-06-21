#include "shell_internal.h"

#include <math.h>

static int32_t reach_utf16_starts_with_ascii_case_insensitive(const uint16_t *text,
                                                              const char *prefix)
{
    if (text == nullptr || prefix == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (prefix[index] != 0)
    {
        uint16_t current = text[index];
        char expected = prefix[index];

        if (current >= 'A' && current <= 'Z')
        {
            current = (uint16_t)(current - 'A' + 'a');
        }

        if (expected >= 'A' && expected <= 'Z')
        {
            expected = (char)(expected - 'A' + 'a');
        }

        if (current != (uint16_t)expected)
        {
            return 0;
        }

        ++index;
    }

    return 1;
}

static reach_color reach_shell_opaque_color(reach_color color)
{
    color.a = 1.0f;
    return color;
}

static int32_t reach_shell_textbox_color_equal(reach_color a, reach_color b)
{
    return fabsf(a.r - b.r) < 0.001f && fabsf(a.g - b.g) < 0.001f && fabsf(a.b - b.b) < 0.001f &&
           fabsf(a.a - b.a) < 0.001f;
}

static int32_t reach_shell_utf16_equal(const uint16_t *a, const uint16_t *b)
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

static int32_t reach_shell_textbox_style_equal(const reach_textbox_style *a,
                                               const reach_textbox_style *b)
{
    return a != nullptr && b != nullptr && fabsf(a->font_size - b->font_size) < 0.001f &&
           a->font_weight == b->font_weight &&
           reach_shell_textbox_color_equal(a->text_color, b->text_color) &&
           reach_shell_textbox_color_equal(a->background_color, b->background_color) &&
           a->max_length == b->max_length &&
           reach_shell_utf16_equal(a->placeholder, b->placeholder);
}

static reach_textbox_style reach_shell_launcher_textbox_style(const reach_shell *shell)
{
    const reach_theme *theme =
        shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();
    float scale = reach_shell_layout_dpi_scale(shell);

    reach_textbox_style style = {};
    style.font_size = 18.0f * scale;
    style.font_weight = REACH_TEXT_WEIGHT_NORMAL;
    style.text_color = theme->launcher_search_text;
    style.background_color = reach_shell_opaque_color(theme->launcher_search_background);
    style.max_length = REACH_MAX_SEARCH_CHARS;
    (void)reach_copy_utf16(style.placeholder, REACH_TEXTBOX_PLACEHOLDER_CAPACITY,
                           (const uint16_t *)L"Search for anything");
    return style;
}

reach_result reach_shell_configure_launcher_textbox(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher_textbox.ops.set_style == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_textbox_style style = reach_shell_launcher_textbox_style(shell);
    if (shell->launcher_textbox_style_valid &&
        reach_shell_textbox_style_equal(&shell->launcher_textbox_style, &style))
    {
        return REACH_OK;
    }

    reach_result result =
        shell->launcher_textbox.ops.set_style(shell->launcher_textbox.textbox, &style);
    if (result == REACH_OK)
    {
        shell->launcher_textbox_style = style;
        shell->launcher_textbox_style_valid = 1;
    }
    return result;
}

void reach_shell_show_launcher_textbox(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher_textbox_active)
    {
        return;
    }
    if (shell->launcher_textbox.ops.show != nullptr)
    {
        (void)shell->launcher_textbox.ops.show(shell->launcher_textbox.textbox);
    }
    if (shell->launcher_textbox.ops.set_focused != nullptr)
    {
        (void)shell->launcher_textbox.ops.set_focused(shell->launcher_textbox.textbox, 1);
    }
    shell->launcher_textbox_active = 1;
}

void reach_shell_hide_launcher_textbox(reach_shell *shell)
{
    if (shell == nullptr || !shell->launcher_textbox_active)
    {
        return;
    }
    if (shell->launcher_textbox.ops.set_focused != nullptr)
    {
        (void)shell->launcher_textbox.ops.set_focused(shell->launcher_textbox.textbox, 0);
    }
    if (shell->launcher_textbox.ops.hide != nullptr)
    {
        (void)shell->launcher_textbox.ops.hide(shell->launcher_textbox.textbox);
    }
    shell->launcher_textbox_active = 0;
}

void reach_shell_reset_launcher_textbox(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    reach_shell_hide_launcher_textbox(shell);
    if (shell->launcher_textbox.ops.set_text != nullptr)
    {
        const uint16_t empty[] = {0};
        (void)shell->launcher_textbox.ops.set_text(shell->launcher_textbox.textbox, empty);
    }
}

void reach_shell_cleanup_closed_launcher(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    reach_shell_cancel_launcher_search(shell);
    reach_shell_release_launcher_result_icons(shell);
    shell->ui.launcher.query[0] = 0;
    shell->ui.launcher.query_length = 0;
    (void)reach_ui_state_clear_launcher_results(&shell->ui);
    shell->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
    shell->pressed_launcher_index = REACH_MAX_PINNED_APPS;
    reach_shell_reset_launcher_textbox(shell);
}

void reach_shell_notify_launcher_search_ready(reach_shell *shell)
{
    if (shell == nullptr || shell->launcher.window.ops.post_event == nullptr)
    {
        return;
    }

    (void)shell->launcher.window.ops.post_event(shell->launcher.window.window,
                                                REACH_UI_EVENT_LAUNCHER_SEARCH_READY);
}

void reach_shell_on_launcher_textbox_event(void *user, const reach_textbox_event *event)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell == nullptr || event == nullptr || !shell->ui.launcher.open)
    {
        return;
    }

    if (event->type == REACH_TEXTBOX_EVENT_TEXT_CHANGED)
    {
        (void)reach_ui_state_set_query(&shell->ui, event->text);
        (void)reach_shell_schedule_launcher_search(shell);
        shell->dirty.layout = 1;
        shell->launcher.dirty_flags = 1;
        reach_shell_request_update(shell);
    }
    else if (event->type == REACH_TEXTBOX_EVENT_SUBMIT)
    {
        reach_ui_event ui_event = {};
        ui_event.type = REACH_UI_EVENT_ENTER;
        (void)reach_shell_handle_event(shell, &ui_event);
    }
    else if (event->type == REACH_TEXTBOX_EVENT_CANCEL)
    {
        reach_ui_event ui_event = {};
        ui_event.type = REACH_UI_EVENT_ESCAPE;
        (void)reach_shell_handle_event(shell, &ui_event);
    }
    else if (event->type == REACH_TEXTBOX_EVENT_NAVIGATE_UP)
    {
        reach_ui_event ui_event = {};
        ui_event.type = REACH_UI_EVENT_ARROW_UP;
        (void)reach_shell_handle_event(shell, &ui_event);
    }
    else if (event->type == REACH_TEXTBOX_EVENT_NAVIGATE_DOWN)
    {
        reach_ui_event ui_event = {};
        ui_event.type = REACH_UI_EVENT_ARROW_DOWN;
        (void)reach_shell_handle_event(shell, &ui_event);
    }
}

void reach_shell_remember_launcher_restore_window(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->launcher_restore_window = 0;
    shell->launcher_restore_window_valid = 0;
    if (shell->window_manager.ops.foreground == nullptr)
    {
        return;
    }

    uintptr_t foreground = shell->window_manager.ops.foreground(shell->window_manager.manager);
    if (foreground != 0)
    {
        shell->launcher_restore_window = foreground;
        shell->launcher_restore_window_valid = 1;
    }
}

void reach_shell_clear_launcher_restore_window(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->launcher_restore_window = 0;
    shell->launcher_restore_window_valid = 0;
}

static int32_t reach_shell_launcher_can_restore_focus_to(reach_shell *shell, uintptr_t window)
{
    if (shell == nullptr || window == 0 || shell->window_manager.ops.activate == nullptr)
    {
        return 0;
    }

    return !reach_shell_window_is_minimized(shell, window);
}

void reach_shell_restore_launcher_focus(reach_shell *shell)
{
    if (shell == nullptr || !shell->launcher_restore_window_valid)
    {
        return;
    }

    uintptr_t window = shell->launcher_restore_window;
    reach_shell_clear_launcher_restore_window(shell);
    if (reach_shell_launcher_can_restore_focus_to(shell, window))
    {
        (void)shell->window_manager.ops.activate(shell->window_manager.manager, window);
    }
}

static void reach_shell_close_launcher_impl(reach_shell *shell, int32_t restore_focus)
{
    if (shell == nullptr || !shell->ui.launcher.open)
    {
        return;
    }

    (void)reach_ui_state_close_launcher(&shell->ui);
    shell->dirty.layout = 1;
    shell->launcher.dirty_flags = 1;
    reach_shell_surface_transition_set(shell, &shell->launcher_transition, 0);
    reach_shell_sync_popup_mouse_hook(shell);
    if (restore_focus)
    {
        reach_shell_restore_launcher_focus(shell);
    }
    else
    {
        reach_shell_clear_launcher_restore_window(shell);
    }
}

void reach_shell_close_launcher(reach_shell *shell)
{
    reach_shell_close_launcher_impl(shell, 1);
}

void reach_shell_close_launcher_without_focus_restore(reach_shell *shell)
{
    reach_shell_close_launcher_impl(shell, 0);
}

reach_result reach_shell_open_launcher_result(reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr || shell->explorer_service.service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (shell->ui.launcher.result_count > 0 &&
        shell->ui.launcher.selected_result_index < shell->ui.launcher.result_count)
    {
        const reach_search_candidate *result =
            &shell->ui.launcher.results[shell->ui.launcher.selected_result_index];
        if (result->path[0] == 0)
        {
            return REACH_OK;
        }
        if (result->kind == REACH_SEARCH_RESULT_APP)
        {
            return reach_shell_open_app(shell, result->path, nullptr, nullptr, 0,
                                        shell->ui.launcher.open);
        }
        if (shell->explorer_service.ops.open_path != nullptr)
        {
            return shell->explorer_service.ops.open_path(shell->explorer_service.service,
                                                         result->path);
        }
        return REACH_OK;
    }

    const uint16_t *query = shell->ui.launcher.query;
    if (query[0] == 0)
    {
        if (shell->explorer_service.ops.open_default != nullptr)
        {
            return shell->explorer_service.ops.open_default(shell->explorer_service.service);
        }
        return REACH_OK;
    }

    if (reach_utf16_starts_with_ascii_case_insensitive(query, "shell:") &&
        shell->explorer_service.ops.open_shell_location != nullptr)
    {
        return shell->explorer_service.ops.open_shell_location(shell->explorer_service.service,
                                                               query);
    }

    if (shell->explorer_service.ops.path_exists != nullptr &&
        shell->explorer_service.ops.path_exists(shell->explorer_service.service, query) &&
        shell->explorer_service.ops.open_path != nullptr)
    {
        return shell->explorer_service.ops.open_path(shell->explorer_service.service, query);
    }

    if (shell->explorer_service.ops.open_default != nullptr)
    {
        return shell->explorer_service.ops.open_default(shell->explorer_service.service);
    }
    return REACH_OK;
}

reach_result reach_shell_reveal_launcher_result(reach_shell *shell, size_t result_index)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr || shell->explorer_service.service == nullptr ||
        shell->explorer_service.ops.reveal_path == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if (result_index >= shell->ui.launcher.result_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_search_candidate *result = &shell->ui.launcher.results[result_index];
    if (result->kind != REACH_SEARCH_RESULT_APP || result->path[0] == 0)
    {
        return REACH_OK;
    }

    return shell->explorer_service.ops.reveal_path(shell->explorer_service.service, result->path);
}
