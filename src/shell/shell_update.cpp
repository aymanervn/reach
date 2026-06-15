#include "shell_internal.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

static int32_t reach_shell_float_animation_active(const reach_float_animation *animation)
{
    return animation != nullptr && animation->elapsed_seconds < animation->duration_seconds;
}

size_t reach_shell_find_dock_order_key(const reach_shell *shell, int32_t pinned, uint32_t pin_id,
                                       uintptr_t window)
{
    if (shell == nullptr)
    {
        return REACH_MAX_PINNED_APPS;
    }
    reach_dock_order_key key = {pinned, pin_id, window};
    return reach_dock_feature_model_find_order_key(&shell->dock_model, key);
}

void reach_shell_move_dock_order(reach_shell *shell, size_t source, size_t target)
{
    if (shell != nullptr)
    {
        reach_dock_feature_model_move_order(&shell->dock_model, source, target);
    }
}

uint32_t reach_shell_dock_item_pin_id(const reach_shell *shell, size_t index)
{
    return shell != nullptr ? reach_dock_feature_model_item_pin_id(&shell->dock_model, index) : 0;
}

int32_t reach_shell_dock_item_matches_key(const reach_shell *shell, size_t index, int32_t pinned,
                                          uint32_t pin_id, uintptr_t window)
{
    if (shell == nullptr)
    {
        return 0;
    }
    reach_dock_order_key key = {pinned, pin_id, window};
    return reach_dock_feature_model_item_matches_key(&shell->dock_model, index, key);
}

int32_t reach_shell_dock_icon_size_px(const reach_shell *shell)
{
    const reach_theme *theme =
        shell != nullptr && shell->theme != nullptr ? shell->theme : reach_theme_default();

    float dock_height = shell != nullptr ? shell->ui.dock.height : 48.0f;
    float dpi_scale = reach_shell_layout_dpi_scale(shell);
    float icon_box_size = reach_theme_icon_box_size(theme, dock_height) * dpi_scale;

    int32_t requested = (int32_t)ceilf(icon_box_size * 4.0f);
    if (requested < 128)
    {
        requested = 128;
    }
    if (requested > 256)
    {
        requested = 256;
    }

    return requested;
}

static float reach_shell_monitor_dpi_scale(const reach_monitor_info *monitor)
{
    if (monitor == nullptr)
    {
        return 1.0f;
    }

    int32_t dpi = monitor->dpi_y > 0 ? monitor->dpi_y : monitor->dpi_x;
    return dpi > 0 ? (float)dpi / 96.0f : 1.0f;
}

static int32_t reach_shell_switcher_width_animation_active(const reach_shell *shell)
{
    return shell != nullptr && shell->switcher_state.width_animating &&
           shell->switcher_state.width_animation.elapsed_seconds <
               shell->switcher_state.width_animation.duration_seconds;
}

static reach_rect_f32 reach_shell_apply_switcher_bounds_animation(reach_shell *shell,
                                                                  reach_rect_f32 target,
                                                                  double delta_seconds)
{
    if (shell == nullptr || !shell->switcher_state.open)
    {
        return target;
    }

    reach_float_animation *animation = &shell->switcher_state.width_animation;
    if (!shell->switcher.bounds_valid || animation->to <= 0.0f)
    {
        *animation = {};
        animation->from = target.width;
        animation->to = target.width;
        animation->value = target.width;
        shell->switcher_state.width_animating = 0;
    }
    else if (fabsf(animation->to - target.width) >= 0.5f)
    {
        float from = shell->switcher_state.width_animating ? animation->value
                                                           : shell->switcher.last_bounds.width;
        reach_float_animation_start(animation, from, target.width, 0.18);
        shell->switcher_state.width_animating = 1;
    }

    float width = target.width;
    if (shell->switcher_state.width_animating)
    {
        reach_float_animation_update(animation, delta_seconds);
        width = animation->value;
        shell->switcher_state.width_animating =
            reach_shell_switcher_width_animation_active(shell);
        shell->switcher.dirty_flags = 1;
    }
    else if (animation->to > 0.0f)
    {
        width = animation->to;
    }

    if (fabsf(width - target.width) < 0.5f)
    {
        width = target.width;
    }

    reach_rect_f32 animated = target;
    float center = target.x + target.width * 0.5f;
    animated.width = width;
    animated.x = center - width * 0.5f;
    return animated;
}

void reach_shell_refresh_pinned_icon_slots(reach_shell *shell)
{
    REACH_ASSERT(shell != nullptr);
    if (shell == nullptr)
    {
        return;
    }

    uint32_t old_pin_ids[REACH_MAX_PINNED_APPS] = {};
    reach_icon_handle old_icons[REACH_MAX_PINNED_APPS] = {};
    uint16_t old_initials[REACH_MAX_PINNED_APPS] = {};
    int32_t old_used[REACH_MAX_PINNED_APPS] = {};

    size_t old_count = shell->dock_icons.pinned_icon_count;
    if (old_count > REACH_MAX_PINNED_APPS)
    {
        old_count = REACH_MAX_PINNED_APPS;
    }

    for (size_t index = 0; index < old_count; ++index)
    {
        old_pin_ids[index] = shell->dock_icons.pinned_icon_pin_ids[index];
        old_icons[index] = shell->dock_icons.pinned_icons[index];
        old_initials[index] = shell->dock_icons.pinned_icon_initials[index];
        shell->dock_icons.pinned_icons[index] = {};
        shell->dock_icons.pinned_icon_pin_ids[index] = 0;
        shell->dock_icons.pinned_icon_initials[index] = 0;
    }

    shell->dock_icons.pinned_icon_count = shell->ui.pinned_app_count > REACH_MAX_PINNED_APPS
                                              ? REACH_MAX_PINNED_APPS
                                              : shell->ui.pinned_app_count;

    for (size_t index = 0; index < shell->dock_icons.pinned_icon_count; ++index)
    {
        const reach_pinned_app_model *app = &shell->ui.pinned_apps[index];

        shell->dock_icons.pinned_icon_pin_ids[index] = app->id;
        shell->dock_icons.pinned_icon_initials[index] = app->title[0] != 0 ? app->title[0] : '?';

        for (size_t old_index = 0; old_index < old_count; ++old_index)
        {
            if (old_used[old_index] || old_icons[old_index].id == 0 ||
                old_pin_ids[old_index] != app->id)
            {
                continue;
            }

            shell->dock_icons.pinned_icons[index] = old_icons[old_index];
            shell->dock_icons.pinned_icon_initials[index] = old_initials[old_index];
            old_icons[old_index] = {};
            old_used[old_index] = 1;
            break;
        }
    }

    for (size_t index = 0; index < old_count; ++index)
    {
        reach_shell_release_icon_handle(shell, &old_icons[index]);
    }

    reach_shell_schedule_pinned_icon_loads(shell);
}

static void reach_shell_copy_ascii_to_utf16(uint16_t *dst, size_t dst_count, const char *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    size_t index = 0;
    if (src != nullptr)
    {
        while (index + 1 < dst_count && src[index] != 0)
        {
            dst[index] = (uint16_t)(unsigned char)src[index];
            ++index;
        }
    }
    dst[index] = 0;
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

static const double REACH_MUSIC_WIDGET_HIDE_GRACE_SECONDS = 4.0;
static const double REACH_MUSIC_WIDGET_PENDING_COVER_SECONDS = 1.0;

static int32_t reach_shell_color_equal(reach_color a, reach_color b)
{
    return fabsf(a.r - b.r) < 0.001f && fabsf(a.g - b.g) < 0.001f &&
           fabsf(a.b - b.b) < 0.001f && fabsf(a.a - b.a) < 0.001f;
}

static void reach_shell_clear_music_widget_pending_cover(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    if (shell->music_widget_pending_cover_icon_id != 0 &&
        shell->music_widget_pending_cover_icon_id != shell->music_widget_model.cover_icon_id)
    {
        reach_shell_release_render_icon(shell, shell->music_widget_pending_cover_icon_id);
    }
    shell->music_widget_pending_cover_seconds = 0.0;
    shell->music_widget_pending_cover_icon_id = 0;
    shell->music_widget_pending_cover_accent = {};
    shell->music_widget_pending_cover_title[0] = 0;
}

static void reach_shell_apply_music_widget_cover(reach_shell *shell, uint64_t cover_icon_id,
                                                 reach_color cover_accent)
{
    if (shell == nullptr)
    {
        return;
    }
    if (shell->music_widget_model.cover_icon_id != cover_icon_id)
    {
        if (shell->music_widget_model.cover_icon_id != 0)
        {
            reach_shell_release_render_icon(shell, shell->music_widget_model.cover_icon_id);
        }
        shell->music_widget_model.cover_icon_id = cover_icon_id;
    }
    shell->music_widget_model.cover_accent = cover_accent;
}

static void reach_shell_music_widget_effective_cover(reach_shell *shell,
                                                     const reach_media_controls_state *state,
                                                     int32_t title_changed,
                                                     uint64_t *out_cover_icon_id,
                                                     reach_color *out_cover_accent)
{
    if (out_cover_icon_id != nullptr)
    {
        *out_cover_icon_id = 0;
    }
    if (out_cover_accent != nullptr)
    {
        *out_cover_accent = {};
    }
    if (shell == nullptr || state == nullptr || !state->has_media)
    {
        return;
    }

    if (title_changed)
    {
        reach_shell_clear_music_widget_pending_cover(shell);
        if (state->cover_icon_id != 0 || shell->music_widget_model.cover_icon_id != 0)
        {
            shell->music_widget_pending_cover_seconds =
                REACH_MUSIC_WIDGET_PENDING_COVER_SECONDS;
            shell->music_widget_pending_cover_icon_id = state->cover_icon_id;
            shell->music_widget_pending_cover_accent = state->cover_accent;
            reach_copy_utf16(shell->music_widget_pending_cover_title, 260, state->title);
        }
        if (out_cover_icon_id != nullptr)
        {
            *out_cover_icon_id = shell->music_widget_model.cover_icon_id;
        }
        if (out_cover_accent != nullptr)
        {
            *out_cover_accent = shell->music_widget_model.cover_accent;
        }
        return;
    }

    if (shell->music_widget_pending_cover_seconds > 0.0 &&
        reach_shell_utf16_equal(shell->music_widget_pending_cover_title, state->title))
    {
        if (state->cover_icon_id != 0 &&
            state->cover_icon_id != shell->music_widget_pending_cover_icon_id)
        {
            reach_shell_clear_music_widget_pending_cover(shell);
            if (out_cover_icon_id != nullptr)
            {
                *out_cover_icon_id = state->cover_icon_id;
            }
            if (out_cover_accent != nullptr)
            {
                *out_cover_accent = state->cover_accent;
            }
            return;
        }
        if (out_cover_icon_id != nullptr)
        {
            *out_cover_icon_id = shell->music_widget_model.cover_icon_id;
        }
        if (out_cover_accent != nullptr)
        {
            *out_cover_accent = shell->music_widget_model.cover_accent;
        }
        return;
    }

    if (out_cover_icon_id != nullptr)
    {
        *out_cover_icon_id = state->cover_icon_id;
    }
    if (out_cover_accent != nullptr)
    {
        *out_cover_accent = state->cover_accent;
    }
}

void reach_shell_refresh_music_widget(reach_shell *shell)
{
    if (shell == nullptr || shell->media_controls.get_state == nullptr)
    {
        return;
    }

    reach_media_controls_state state = {};
    if (shell->media_controls.get_state(shell->media_controls.userdata, &state) != REACH_OK)
    {
        return;
    }

    if (!state.has_media && shell->music_widget_model.visible &&
        shell->music_widget_hide_grace_seconds > 0.0)
    {
        return;
    }

    int32_t title_changed = !reach_shell_utf16_equal(shell->music_widget_model.title, state.title);
    uint64_t next_cover_icon_id = 0;
    reach_color next_cover_accent = {};
    reach_shell_music_widget_effective_cover(shell, &state, title_changed, &next_cover_icon_id,
                                             &next_cover_accent);

    int32_t visibility_changed = shell->music_widget_model.visible != state.has_media;
    int32_t changed = visibility_changed ||
                      title_changed ||
                      shell->music_widget_model.cover_icon_id != next_cover_icon_id ||
                      !reach_shell_color_equal(shell->music_widget_model.cover_accent,
                                               next_cover_accent) ||
                      shell->music_widget_model.playback != state.playback;
    if (!changed)
    {
        return;
    }

    if (state.has_media)
    {
        shell->music_widget_model.visible = 1;
        reach_copy_utf16(shell->music_widget_model.title, 260, state.title);
        reach_shell_apply_music_widget_cover(shell, next_cover_icon_id, next_cover_accent);
        shell->music_widget_model.playback = state.playback;
    }
    else
    {
        reach_shell_clear_music_widget_pending_cover(shell);
        reach_music_widget_model_init(&shell->music_widget_model);
    }
    if (visibility_changed)
    {
        reach_shell_clear_dock_item_x_animations(shell);
        shell->dock_items_changed = 1;
    }
    shell->dirty.layout = 1;
    shell->dock.dirty_flags = 1;
}

static void reach_shell_process_music_widget_refresh(reach_shell *shell)
{
    if (shell == nullptr || !shell->music_widget_refresh_requested.exchange(0))
    {
        return;
    }

    reach_shell_refresh_music_widget(shell);
}

void reach_shell_start_music_widget_hide_grace(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    shell->music_widget_hide_grace_seconds = REACH_MUSIC_WIDGET_HIDE_GRACE_SECONDS;
    shell->music_widget_refresh_requested = 1;
    reach_shell_request_update(shell);
}

static void reach_shell_update_music_widget_hide_grace(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr || shell->music_widget_hide_grace_seconds <= 0.0)
    {
        return;
    }

    shell->music_widget_hide_grace_seconds -= delta_seconds;
    if (shell->music_widget_hide_grace_seconds <= 0.0)
    {
        shell->music_widget_hide_grace_seconds = 0.0;
        shell->music_widget_refresh_requested = 1;
    }
    reach_shell_request_update(shell);
}

static void reach_shell_update_music_widget_pending_cover(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr || shell->music_widget_pending_cover_seconds <= 0.0)
    {
        return;
    }

    shell->music_widget_pending_cover_seconds -= delta_seconds;
    if (shell->music_widget_pending_cover_seconds <= 0.0)
    {
        shell->music_widget_pending_cover_seconds = 0.0;
        reach_shell_apply_music_widget_cover(shell, shell->music_widget_pending_cover_icon_id,
                                             shell->music_widget_pending_cover_accent);
        shell->music_widget_pending_cover_icon_id = 0;
        shell->music_widget_pending_cover_accent = {};
        shell->music_widget_pending_cover_title[0] = 0;
        shell->dock.dirty_flags = 1;
    }
    reach_shell_request_update(shell);
}

void reach_shell_request_update(reach_shell *shell)
{
    if (shell != nullptr)
    {
        shell->dirty.update_requested = 1;
    }
}

static void reach_shell_update_clock_text(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    time_t now = time(nullptr);
    int64_t current_minute = (int64_t)(now / 60);
    if (shell->dock_clock_initialized && shell->dock_clock_last_minute == current_minute)
    {
        return;
    }

    struct tm local = {};
    if (now == (time_t)-1 || localtime_s(&local, &now) != 0)
    {
        return;
    }

    static const char *months[] = {"January",   "February", "March",    "April",
                                   "May",       "June",     "July",     "August",
                                   "September", "October",  "November", "December"};
    static const char *days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                                 "Thursday", "Friday", "Saturday"};

    int hour = local.tm_hour % 12;
    if (hour == 0)
    {
        hour = 12;
    }
    const char *suffix = local.tm_hour >= 12 ? "PM" : "AM";

    char time_text[32] = {};
    char date_text[64] = {};
    snprintf(time_text, sizeof(time_text), "%d:%02d %s", hour, local.tm_min, suffix);
    if (local.tm_mon < 0 || local.tm_mon > 11 || local.tm_wday < 0 || local.tm_wday > 6)
    {
        return;
    }
    snprintf(date_text, sizeof(date_text), "%.3s %d, %.3s", months[local.tm_mon], local.tm_mday,
             days[local.tm_wday]);

    uint16_t next_time[32] = {};
    uint16_t next_date[64] = {};
    reach_shell_copy_ascii_to_utf16(next_time, 32, time_text);
    reach_shell_copy_ascii_to_utf16(next_date, 64, date_text);
    if (!shell->dock_clock_initialized ||
        !reach_shell_utf16_equal(shell->dock_time_text, next_time) ||
        !reach_shell_utf16_equal(shell->dock_date_text, next_date))
    {
        reach_copy_utf16(shell->dock_time_text, 32, next_time);
        reach_copy_utf16(shell->dock_date_text, 64, next_date);
        shell->dock_clock_initialized = 1;
        shell->dock.dirty_flags = 1;
    }
    shell->dock_clock_last_minute = current_minute;
}

reach_result reach_shell_update(reach_shell *shell, double delta_seconds)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (shell->dirty.events_dispatched_this_cycle)
    {
        shell->dirty.events_dispatched_this_cycle = 0;
    }
    else
    {
        (void)reach_shell_dispatch_events(shell);
        shell->dirty.events_dispatched_this_cycle = 0;
    }

    reach_shell_apply_window_control_result(shell);

    if (reach_shell_can_move_dock_without_redraw(shell))
    {
        return reach_shell_move_dock_animation_frame(shell, delta_seconds);
    }

    reach_shell_process_quick_settings_system_changes(shell, delta_seconds);
    reach_shell_apply_quick_settings_system_refresh_result(shell);
    reach_shell_apply_quick_settings_audio_refresh_result(shell);
    reach_shell_apply_open_window_icon_results(shell);
    reach_shell_apply_launcher_result_icon_results(shell);
    reach_shell_update_music_widget_hide_grace(shell, delta_seconds);
    reach_shell_update_music_widget_pending_cover(shell, delta_seconds);
    reach_shell_process_music_widget_refresh(shell);

    reach_shell_update_clock_text(shell);
    if (shell->feedback.dock_animating)
    {
        reach_float_animation_update(&shell->feedback.dock_opacity, delta_seconds);
        shell->feedback.dock_animating =
            reach_shell_float_animation_active(&shell->feedback.dock_opacity);

        if (!shell->feedback.dock_animating && !shell->feedback.dock_pressed &&
            !shell->feedback.dock_sticky && shell->feedback.dock_opacity.value <= 0.001f)
        {
            shell->feedback.dock_opacity.value = 0.0f;
            shell->feedback.dock_index = REACH_SHELL_DOCK_FEEDBACK_NONE;
        }

        shell->dock.dirty_flags = 1;
    }

    if (shell->feedback.tray_animating)
    {
        reach_float_animation_update(&shell->feedback.tray_opacity, delta_seconds);
        shell->feedback.tray_animating =
            reach_shell_float_animation_active(&shell->feedback.tray_opacity);

        if (!shell->feedback.tray_animating && !shell->feedback.tray_pressed &&
            shell->feedback.tray_opacity.value <= 0.001f)
        {
            shell->feedback.tray_opacity.value = 0.0f;
            shell->feedback.tray_index = REACH_MAX_TRAY_ITEMS;
        }
        shell->tray.dirty_flags = 1;
    }

    for (size_t index = 0; index < shell->dock_model.item_count; ++index)
    {
        if (shell->dock_item_x_animating[index])
        {
            reach_float_animation_update(&shell->dock_item_x_animations[index], delta_seconds);
            shell->dock_item_x_animating[index] =
                reach_shell_float_animation_active(&shell->dock_item_x_animations[index]);
            shell->dock.dirty_flags = 1;
        }
    }
    if (shell->dock_drag.snapping)
    {
        reach_float_animation_update(&shell->dock_drag.snap_animation, delta_seconds);
        shell->dock_drag.x = shell->dock_drag.snap_animation.value;
        shell->dock_drag.snapping =
            reach_shell_float_animation_active(&shell->dock_drag.snap_animation);
        shell->dock.dirty_flags = 1;
        if (!shell->dock_drag.snapping)
        {
            shell->dock_drag.source_index = REACH_MAX_PINNED_APPS;
            shell->dock_drag.target_index = REACH_MAX_PINNED_APPS;
            shell->dock_drag.pinned = 0;
            shell->dock_drag.pin_id = 0;
            shell->dock_drag.window = 0;
        }
    }

    int32_t window_manager_dirty =
        shell->window_manager.ops.needs_refresh != nullptr &&
        shell->window_manager.ops.needs_refresh(shell->window_manager.manager);
    if (window_manager_dirty && shell->window_manager.ops.refresh != nullptr)
    {
        (void)shell->window_manager.ops.refresh(shell->window_manager.manager);

        int32_t open_windows_changed = 0;
        (void)reach_shell_refresh_open_windows(shell, &open_windows_changed);

        uintptr_t foreground_window =
            shell->window_manager.ops.foreground != nullptr
                ? shell->window_manager.ops.foreground(shell->window_manager.manager)
                : 0;
        int32_t foreground_changed = shell->foreground_window != foreground_window;
        reach_shell_note_foreground_window(shell, foreground_window);

        if (open_windows_changed || foreground_changed)
        {
            reach_shell_refresh_switcher_windows(shell);
            shell->dock.dirty_flags = 1;
            shell->switcher.dirty_flags = 1;
        }
        if (foreground_changed && foreground_window != 0 && shell->ui.launcher.open)
        {
            reach_shell_clear_launcher_restore_window(shell);
            reach_shell_close_launcher_without_focus_restore(shell);
        }
    }
    (void)reach_shell_update_game_mode(shell);
    int32_t game_mode = reach_shell_game_mode_enabled(shell);

    if (!game_mode && shell->tray_state.popup_open &&
        shell->tray_provider.ops.needs_refresh != nullptr &&
        shell->tray_provider.ops.needs_refresh(shell->tray_provider.provider))
    {
        (void)reach_shell_refresh_tray_items(shell);
        shell->tray.dirty_flags = 1;
    }

    reach_result monitor_result = reach_shell_refresh_monitor_layout(shell);
    if (monitor_result != REACH_OK)
    {
        return monitor_result;
    }

    if (shell->launcher.window.ops.set_bounds != nullptr && shell->monitors.list != nullptr &&
        shell->monitors.ops.count != nullptr && shell->monitors.ops.primary != nullptr &&
        shell->monitors.ops.count(shell->monitors.list) > 0)
    {
        const reach_monitor_info *monitor = shell->monitors.ops.primary(shell->monitors.list);
        REACH_ASSERT(monitor != nullptr);
        REACH_ASSERT(monitor->primary || shell->monitors.ops.count(shell->monitors.list) == 1);
        if (monitor == nullptr)
        {
            return REACH_ERROR;
        }

        reach_rect_f32 bounds = {};
        bounds.x = (float)monitor->bounds.left;
        bounds.y = (float)monitor->bounds.top;
        bounds.width = (float)(monitor->bounds.right - monitor->bounds.left);
        bounds.height = (float)(monitor->bounds.bottom - monitor->bounds.top);

        reach_result result = REACH_OK;
        int32_t launcher_window_changed = 0;

        if (shell->launcher.renderer.ops.begin_frame != nullptr)
        {
            reach_ui_layout_input input = {};
            input.monitor_bounds = bounds;
            input.work_area = bounds;
            input.dpi_scale = reach_shell_monitor_dpi_scale(monitor);
            shell->layout_dpi_scale = input.dpi_scale;

            reach_ui_layout layout = {};
            if (reach_ui_layout_compute(&shell->ui, &input, &layout) == REACH_OK)
            {
                if (shell->dock_items_changed)
                {
                    reach_shell_rebuild_dock_items_with_animations(shell, &layout.dock);
                    shell->dock_items_changed = 0;
                }
                else
                {
                    reach_shell_build_dock_items(shell, &layout.dock);
                }
                if (!game_mode)
                {
                    reach_shell_apply_dock_width_animation(shell, &layout.dock, delta_seconds);
                }
                reach_rect_f32 shown_dock_bounds = layout.dock.bounds;
                reach_rect_f32 animated_dock_bounds =
                    game_mode ? shown_dock_bounds
                              : reach_shell_apply_dock_animation(shell, shown_dock_bounds, bounds,
                                                                 delta_seconds);
                if (!game_mode)
                {
                    reach_shell_sync_dock_reveal_edge(shell, shown_dock_bounds, bounds);
                    if (shell->dock_reveal.check_dirty)
                    {
                        shell->dock_reveal.check_dirty = 0;
                    }
                }
                else
                {
                    shell->dock_reveal.requested = 0;
                    shell->dock_reveal.check_dirty = 0;
                    if (shell->dock_reveal_edge.ops.hide != nullptr)
                    {
                        (void)shell->dock_reveal_edge.ops.hide(shell->dock_reveal_edge.edge);
                    }
                    shell->dock_reveal.edge_visible = 0;
                }

                layout.dock.bounds = animated_dock_bounds;

                float dock_left_offset = bounds.x - layout.dock.bounds.x;
                float dock_right_offset =
                    bounds.x + bounds.width - (layout.dock.bounds.x + layout.dock.bounds.width);
                float dock_x_offset = 0.0f;
                if (dock_left_offset > 0.0f)
                {
                    dock_x_offset = dock_left_offset;
                }
                else if (dock_right_offset < 0.0f)
                {
                    dock_x_offset = dock_right_offset;
                }

                if (dock_x_offset != 0.0f)
                {
                    layout.dock.bounds.x += dock_x_offset;
                }

                shell->music_widget_layout = reach_music_widget_compute_layout(
                    &shell->music_widget_model, shell->theme, layout.dock.music_widget,
                    reach_shell_layout_dpi_scale(shell));

                int32_t dock_layout_changed =
                    !shell->has_layout ||
                    !reach_shell_rect_equal(shell->layout.dock.bounds, layout.dock.bounds);
                int32_t launcher_layout_changed =
                    !shell->has_layout ||
                    !reach_shell_rect_equal(shell->layout.launcher.bounds, layout.launcher.bounds);

                shell->layout = layout;
                shell->has_layout = 1;

                result = reach_shell_apply_window_state(
                    &shell->launcher.window, layout.launcher.bounds,
                    (!game_mode && shell->ui.launcher.open) ? 1.0f : 0.0f,
                    &shell->launcher.last_bounds, &shell->launcher.last_opacity,
                    &shell->launcher.bounds_valid, &shell->launcher.opacity_valid,
                    &launcher_window_changed);
                if (result != REACH_OK)
                {
                    return result;
                }

                if (!game_mode && shell->ui.launcher.open &&
                    (shell->dirty.render || shell->launcher.dirty_flags ||
                     launcher_window_changed || launcher_layout_changed))
                {
                    (void)reach_shell_render_launcher_surface(shell, &layout.launcher);
                }
                if (game_mode && shell->launcher.window.ops.hide != nullptr)
                {
                    (void)shell->launcher.window.ops.hide(shell->launcher.window.window);
                }
                if (game_mode)
                {
                    if (shell->dock.window.ops.hide != nullptr)
                    {
                        (void)shell->dock.window.ops.hide(shell->dock.window.window);
                    }
                }
                else if (shell->dock.window.ops.set_bounds != nullptr)
                {
                    if (shell->dock.window.ops.show != nullptr)
                    {
                        (void)shell->dock.window.ops.show(shell->dock.window.window);
                    }
                    int32_t dock_window_changed = 0;
                    float dock_radius =
                        reach_theme_dock_corner_radius(shell->theme, layout.dock.bounds.height);
                    result = reach_shell_apply_window_state(
                        &shell->dock.window, layout.dock.bounds, 1.0f, &shell->dock.last_bounds,
                        &shell->dock.last_opacity,
                        &shell->dock.bounds_valid, &shell->dock.opacity_valid,
                        &dock_window_changed);
                    if (result != REACH_OK)
                    {
                        return result;
                    }

                    if (dock_window_changed &&
                        shell->dock.window.ops.apply_rounded_corners != nullptr)
                    {
                        (void)shell->dock.window.ops.apply_rounded_corners(
                            shell->dock.window.window, dock_radius);
                    }

                    int32_t dock_reveal_position_only =
                        shell->dock_animation.animating && !shell->dirty.render &&
                        !shell->dock.dirty_flags && !shell->dock_width.animating &&
                        !shell->dock_drag.active && !shell->dock_drag.snapping &&
                        !shell->feedback.dock_animating;

                    if (shell->dirty.render || shell->dock.dirty_flags ||
                        (!dock_reveal_position_only && (dock_window_changed || dock_layout_changed)))
                    {
                        (void)reach_shell_render_dock_surface(shell, &layout.dock);
                    }
                }

                if (shell->tray.window.ops.set_bounds != nullptr)
                {
                    reach_rect_f32 tray_bounds = {};
                    reach_dock_layout screen_dock = reach_shell_dock_layout_to_screen(layout.dock);
                    reach_shell_compute_tray_popup_layout(shell, &screen_dock, &tray_bounds,
                                                          shell->tray_state.model.item_slots);
                    int32_t tray_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->tray.window, tray_bounds,
                        (!game_mode && shell->tray_state.popup_open) ? 1.0f : 0.0f,
                        &shell->tray.last_bounds, &shell->tray.last_opacity,
                        &shell->tray.bounds_valid, &shell->tray.opacity_valid,
                        &tray_window_changed);
                    if (result != REACH_OK)
                    {
                        return result;
                    }

                    if (tray_window_changed &&
                        shell->tray.window.ops.apply_rounded_corners != nullptr)
                    {
                        (void)shell->tray.window.ops.apply_rounded_corners(
                            shell->tray.window.window,
                            reach_popup_radius_scaled(reach_shell_layout_dpi_scale(shell)));
                    }
                    if (shell->tray_state.popup_open)
                    {
                        if (shell->tray.window.ops.show != nullptr)
                        {
                            (void)shell->tray.window.ops.show(shell->tray.window.window);
                        }
                        if (shell->dirty.render || shell->tray.dirty_flags || tray_window_changed)
                        {
                            (void)reach_shell_render_tray_surface(shell, tray_bounds);
                        }
                    }
                    else
                    {
                        if (shell->tray_state.popup_open)
                        {
                            reach_shell_set_tray_popup_open(shell, 0);
                        }
                        else if (shell->tray.window.ops.hide != nullptr)
                        {
                            (void)shell->tray.window.ops.hide(shell->tray.window.window);
                        }
                    }
                }

                if (shell->quick_settings.window.ops.set_bounds != nullptr)
                {
                    int32_t quick_window_changed = 0;

                    if (!game_mode)
                    {
                        reach_shell_refresh_quick_settings_layout(shell);
                        reach_shell_update_quick_settings_animation(shell, delta_seconds);
                    }

                    result = reach_shell_apply_window_state(
                        &shell->quick_settings.window, shell->quick_settings_bounds,
                        (!game_mode && shell->quick_settings_open) ? 1.0f : 0.0f,
                        &shell->quick_settings.last_bounds, &shell->quick_settings.last_opacity,
                        &shell->quick_settings.bounds_valid, &shell->quick_settings.opacity_valid,
                        &quick_window_changed);
                    if (result != REACH_OK)
                    {
                        return result;
                    }

                    if (quick_window_changed &&
                        shell->quick_settings.window.ops.apply_rounded_corners != nullptr)
                    {
                        (void)shell->quick_settings.window.ops.apply_rounded_corners(
                            shell->quick_settings.window.window,
                            reach_popup_radius_scaled(reach_shell_layout_dpi_scale(shell)));
                    }

                    if (!game_mode && shell->quick_settings_open)
                    {
                        if (shell->quick_settings.window.ops.show != nullptr)
                        {
                            (void)shell->quick_settings.window.ops.show(
                                shell->quick_settings.window.window);
                        }
                        if (shell->dirty.render || shell->quick_settings.dirty_flags ||
                            quick_window_changed)
                        {
                            (void)reach_shell_render_quick_settings_surface(shell);
                        }
                    }
                    else if (shell->quick_settings.window.ops.hide != nullptr)
                    {
                        (void)shell->quick_settings.window.ops.hide(
                            shell->quick_settings.window.window);
                    }
                }

                if (shell->switcher.window.ops.set_bounds != nullptr)
                {
                    reach_rect_f32 target_switcher_bounds =
                        reach_switcher_bounds_for_count_scaled(
                            bounds, reach_shell_switcher_visible_count(shell),
                            reach_shell_layout_dpi_scale(shell));
                    reach_rect_f32 switcher_bounds = reach_shell_apply_switcher_bounds_animation(
                        shell, target_switcher_bounds, delta_seconds);
                    int32_t switcher_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->switcher.window, switcher_bounds,
                        (!game_mode && shell->switcher_state.open) ? 1.0f : 0.0f,
                        &shell->switcher.last_bounds, &shell->switcher.last_opacity,
                        &shell->switcher.bounds_valid, &shell->switcher.opacity_valid,
                        &switcher_window_changed);
                    if (result != REACH_OK)
                    {
                        return result;
                    }

                    if (switcher_window_changed &&
                        shell->switcher.window.ops.apply_rounded_corners != nullptr)
                    {
                        (void)shell->switcher.window.ops.apply_rounded_corners(
                            shell->switcher.window.window,
                            16.0f * reach_shell_layout_dpi_scale(shell));
                    }
                    if (!game_mode && shell->switcher_state.open)
                    {
                        if (shell->switcher.window.ops.show != nullptr)
                        {
                            (void)shell->switcher.window.ops.show(shell->switcher.window.window);
                        }
                        if (shell->dirty.render || shell->switcher.dirty_flags ||
                            switcher_window_changed)
                        {
                            (void)reach_shell_render_switcher_surface(shell, switcher_bounds);
                        }
                    }
                    else if (shell->switcher.window.ops.hide != nullptr)
                    {
                        (void)shell->switcher.window.ops.hide(shell->switcher.window.window);
                    }
                }

                if (shell->context_menu.window.ops.set_bounds != nullptr)
                {
                    int32_t context_window_changed = 0;
                    result = reach_shell_apply_window_state(
                        &shell->context_menu.window, shell->context_menu_state.bounds,
                        (!game_mode && shell->context_menu_state.open) ? 1.0f : 0.0f,
                        &shell->context_menu.last_bounds, &shell->context_menu.last_opacity,
                        &shell->context_menu.bounds_valid, &shell->context_menu.opacity_valid,
                        &context_window_changed);
                    if (result != REACH_OK)
                    {
                        return result;
                    }

                    if (context_window_changed &&
                        shell->context_menu.window.ops.apply_rounded_corners != nullptr)
                    {
                        (void)shell->context_menu.window.ops.apply_rounded_corners(
                            shell->context_menu.window.window,
                            reach_popup_radius_scaled(reach_shell_layout_dpi_scale(shell)));
                    }
                    if (!game_mode && shell->context_menu_state.open)
                    {
                        if (shell->context_menu.window.ops.show != nullptr)
                        {
                            (void)shell->context_menu.window.ops.show(
                                shell->context_menu.window.window);
                        }
                        if (shell->dirty.render || shell->context_menu.dirty_flags ||
                            context_window_changed)
                        {
                            (void)reach_shell_render_context_menu_surface(shell);
                        }
                    }
                    else if (shell->context_menu.window.ops.hide != nullptr)
                    {
                        (void)shell->context_menu.window.ops.hide(
                            shell->context_menu.window.window);
                    }
                }

                if (!game_mode && shell->ui.launcher.open)
                {
                    reach_shell_raise_launcher(shell);
                }
            }
        }
    }
    shell->dirty.layout = 0;
    shell->dirty.render = 0;
    shell->dirty.update_requested = 0;
    shell->dock.dirty_flags = 0;
    shell->launcher.dirty_flags = 0;
    shell->tray.dirty_flags = 0;
    shell->switcher.dirty_flags = 0;
    shell->context_menu.dirty_flags = 0;
    shell->quick_settings.dirty_flags = 0;

    return REACH_OK;
}

int32_t reach_shell_needs_frame(const reach_shell *shell)
{
    int32_t dock_item_animating = 0;
    int32_t window_manager_needs_refresh = 0;

    if (shell != nullptr)
    {
        for (size_t index = 0; index < shell->dock_model.item_count; ++index)
        {
            if (shell->dock_item_x_animating[index])
            {
                dock_item_animating = 1;
                break;
            }
        }

        if (shell->window_manager.manager != nullptr &&
            shell->window_manager.ops.needs_refresh != nullptr)
        {
            window_manager_needs_refresh =
                shell->window_manager.ops.needs_refresh(shell->window_manager.manager);
        }
    }

    return shell != nullptr &&
           (shell->dirty.update_requested || window_manager_needs_refresh || shell->dirty.render ||
            shell->dock.dirty_flags || shell->launcher.dirty_flags || shell->tray.dirty_flags ||
            shell->switcher.dirty_flags || shell->context_menu.dirty_flags ||
            shell->quick_settings.dirty_flags || shell->dock_reveal.check_dirty ||
            reach_shell_open_window_icon_work_pending(shell) ||
            reach_shell_launcher_result_icon_work_pending(shell) ||
            reach_shell_config_reload_work_pending(shell) ||
            reach_shell_quick_settings_audio_refresh_work_pending(shell) ||
            reach_shell_quick_settings_system_refresh_work_pending(shell) ||
            shell->quick_settings_bluetooth_pending.active ||
            shell->music_widget_refresh_requested.load() ||
            shell->music_widget_hide_grace_seconds > 0.0 ||
            shell->music_widget_pending_cover_seconds > 0.0 ||
            reach_shell_popup_bounds_animation_active(&shell->quick_settings_bounds_animation) ||
            shell->dock_animation.animating || shell->dock_width.animating ||
            reach_shell_switcher_width_animation_active(shell) || shell->dock_drag.active ||
            shell->dock_drag.snapping || dock_item_animating || shell->feedback.dock_animating ||
            shell->feedback.tray_animating);
}
