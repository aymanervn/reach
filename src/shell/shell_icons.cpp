#include "shell_internal.h"

static void reach_shell_release_render_icon_from_surface(
    reach_surface_runtime *surface,
    uint64_t icon_id)
{
    if (surface == nullptr ||
        icon_id == 0 ||
        surface->renderer.ops.release_icon == nullptr) {
        return;
    }

    surface->renderer.ops.release_icon(
        surface->renderer.backend,
        icon_id);
}

void reach_shell_release_render_icon(reach_shell *shell, uint64_t icon_id)
{
    if (shell == nullptr || icon_id == 0) {
        return;
    }

    /*
        Conservative by design: a render icon id can be cached by whichever
        surface rendered it. This makes eviction a shell policy instead of
        spreading it across dock, launcher, tray, and lifecycle code.
    */
    reach_shell_release_render_icon_from_surface(&shell->launcher, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->dock, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->tray, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->switcher, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->context_menu, icon_id);
    reach_shell_release_render_icon_from_surface(&shell->quick_settings, icon_id);
}

void reach_shell_release_icon_handle(reach_shell *shell, reach_icon_handle *icon)
{
    if (icon == nullptr) {
        return;
    }

    if (shell == nullptr || icon->id == 0) {
        *icon = {};
        return;
    }

    reach_shell_release_render_icon(shell, icon->id);

    if (shell->icon_provider.ops.release != nullptr) {
        (void)shell->icon_provider.ops.release(
            shell->icon_provider.provider,
            *icon);
    }

    *icon = {};
}

reach_result reach_shell_load_icon_handle(
    reach_shell *shell,
    const uint16_t *path,
    int32_t size_px,
    reach_icon_handle *out_icon)
{
    if (out_icon != nullptr) {
        *out_icon = {};
    }

    if (shell == nullptr ||
        path == nullptr ||
        path[0] == 0 ||
        out_icon == nullptr ||
        shell->icon_provider.ops.load == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    reach_icon_request request = {};
    request.size_px = size_px;
    reach_copy_utf16(request.path, 260, path);

    return shell->icon_provider.ops.load(
        shell->icon_provider.provider,
        &request,
        out_icon);
}

void reach_shell_release_dock_icons(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    size_t pinned_count = shell->dock_icons.pinned_icon_count;
    if (pinned_count > REACH_MAX_PINNED_APPS) {
        pinned_count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < pinned_count; ++index) {
        reach_shell_release_icon_handle(
            shell,
            &shell->dock_icons.pinned_icons[index]);
        shell->dock_icons.pinned_icon_initials[index] = 0;
    }

    shell->dock_icons.pinned_icon_count = 0;

    size_t open_count = shell->open_window_count;
    if (open_count > REACH_MAX_PINNED_APPS) {
        open_count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < open_count; ++index) {
        reach_shell_release_icon_handle(
            shell,
            &shell->dock_icons.open_window_icons[index]);
        shell->dock_icons.open_window_initials[index] = 0;
    }
    reach_dock_clear_all_icons(&shell->dock_icons, REACH_MAX_PINNED_APPS);
}

void reach_shell_release_open_window_icons(reach_shell *shell, size_t old_count)
{
    if (shell == nullptr) {
        return;
    }

    size_t count = old_count;
    if (count > REACH_MAX_PINNED_APPS) {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index) {
        reach_shell_release_icon_handle(
            shell,
            &shell->dock_icons.open_window_icons[index]);
        shell->dock_icons.open_window_initials[index] = 0;
    }
}

void reach_shell_load_open_window_icons(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    size_t count = shell->open_window_count;
    if (count > REACH_MAX_PINNED_APPS) {
        count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < count; ++index) {
        shell->dock_icons.open_window_initials[index] =
            shell->open_windows[index].title[0] != 0
                ? shell->open_windows[index].title[0]
                : '?';

        (void)reach_shell_load_icon_handle(
            shell,
            shell->open_windows[index].path,
            reach_shell_dock_icon_size_px(shell),
            &shell->dock_icons.open_window_icons[index]);
    }
}

void reach_shell_release_tray_render_icons(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    size_t count = shell->tray_state.model.item_count;
    if (count > REACH_MAX_TRAY_ITEMS) {
        count = REACH_MAX_TRAY_ITEMS;
    }

    for (size_t index = 0; index < count; ++index) {
        uint64_t icon_id = shell->tray_state.model.items[index].icon_id;
        if (icon_id != 0) {
            reach_shell_release_render_icon(shell, icon_id);
        }
    }
}

void reach_shell_release_quick_settings_audio_render_icons(reach_shell *shell)
{
    if (shell == nullptr) {
        return;
    }

    size_t session_count = shell->quick_settings_audio_sessions.count;
    if (session_count > REACH_AUDIO_VOLUME_MAX_SESSIONS) {
        session_count = REACH_AUDIO_VOLUME_MAX_SESSIONS;
    }

    for (size_t index = 0; index < session_count; ++index) {
        uint64_t icon_id = shell->quick_settings_audio_sessions.sessions[index].icon_id;
        if (icon_id != 0) {
            reach_shell_release_render_icon(shell, icon_id);
        }
    }

    size_t device_count = shell->quick_settings_output_devices.count;
    if (device_count > REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES) {
        device_count = REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
    }

    for (size_t index = 0; index < device_count; ++index) {
        uint64_t icon_id = shell->quick_settings_output_devices.devices[index].icon_id;
        if (icon_id != 0) {
            reach_shell_release_render_icon(shell, icon_id);
        }
    }
}
