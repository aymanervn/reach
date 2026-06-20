#include "shell_internal.h"

static void reach_shell_mark_dirty_for_event(reach_shell *shell, const reach_ui_event *event)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(event != nullptr);
    if (shell == nullptr || event == nullptr)
    {
        return;
    }

    switch (event->type)
    {
    case REACH_UI_EVENT_WINDOWS_KEY:
    case REACH_UI_EVENT_ESCAPE:
    case REACH_UI_EVENT_ENTER:
    case REACH_UI_EVENT_ARROW_UP:
    case REACH_UI_EVENT_ARROW_DOWN:
        shell->dirty.layout = 1;
        shell->launcher.dirty_flags = 1;
        break;

    case REACH_UI_EVENT_DOCK_APP_CLICK:
    case REACH_UI_EVENT_TRAY_BUTTON_CLICK:
    case REACH_UI_EVENT_POINTER_UP:
    case REACH_UI_EVENT_POINTER_MOVE:
    case REACH_UI_EVENT_POINTER_LEAVE:
    case REACH_UI_EVENT_POINTER_MIDDLE:
    case REACH_UI_EVENT_POINTER_DOWN:
    case REACH_UI_EVENT_POINTER_CANCEL:
    case REACH_UI_EVENT_POINTER_WHEEL:
        break;

    case REACH_UI_EVENT_NONE:
    default:
        break;
    }
}

static int32_t reach_shell_game_mode_allows_event(reach_ui_event_type type)
{
    return type == REACH_UI_EVENT_CONFIG_CHANGED || type == REACH_UI_EVENT_DISPLAY_CHANGED ||
           type == REACH_UI_EVENT_WINDOW_STATE_CHANGED ||
           type == REACH_UI_EVENT_WALLPAPER_CHANGED || type == REACH_UI_EVENT_POINTER_CANCEL ||
           type == REACH_UI_EVENT_MEDIA_PREVIOUS || type == REACH_UI_EVENT_MEDIA_PLAY_PAUSE ||
           type == REACH_UI_EVENT_MEDIA_NEXT || type == REACH_UI_EVENT_VOLUME_UP ||
           type == REACH_UI_EVENT_VOLUME_DOWN || type == REACH_UI_EVENT_VOLUME_MUTE ||
           type == REACH_UI_EVENT_BRIGHTNESS_UP || type == REACH_UI_EVENT_BRIGHTNESS_DOWN;
}

static int32_t reach_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return (float)x >= rect.x && (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

static reach_point_i32 reach_shell_event_dock_point(const reach_shell *shell,
                                                    const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr)
    {
        return {};
    }
    return reach_shell_dock_local_point(&shell->layout.dock, event->x, event->y);
}

static reach_dock_hit_result reach_shell_dock_hit_test_event(const reach_shell *shell,
                                                             const reach_ui_event *event)
{
    reach_dock_hit_result result = {};
    result.type = REACH_DOCK_HIT_NONE;
    result.index = REACH_MAX_PINNED_APPS;
    if (shell == nullptr || event == nullptr)
    {
        return result;
    }
    reach_point_i32 point = reach_shell_event_dock_point(shell, event);
    return reach_dock_hit_test(&shell->layout.dock, point.x, point.y);
}

static reach_music_widget_action_type
reach_shell_music_widget_hit_test_event(const reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr)
    {
        return REACH_MUSIC_WIDGET_ACTION_NONE;
    }
    reach_point_i32 point = reach_shell_event_dock_point(shell, event);
    return reach_music_widget_hit_test(&shell->music_widget_model, &shell->music_widget_layout,
                                       point.x, point.y);
}

static void reach_shell_note_launcher_viewport_changed(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    reach_shell_schedule_launcher_result_icons(shell);
    shell->dirty.layout = 1;
    shell->launcher.dirty_flags = 1;
    reach_shell_request_update(shell);
}

static reach_result reach_shell_scroll_launcher_results(reach_shell *shell, int32_t delta)
{
    if (shell == nullptr || delta == 0)
    {
        return REACH_OK;
    }

    size_t old_offset = reach_ui_state_launcher_result_scroll_offset(&shell->ui);
    reach_result result = reach_ui_state_scroll_launcher_results(&shell->ui, delta);
    if (result != REACH_OK)
    {
        return result;
    }
    if (old_offset != reach_ui_state_launcher_result_scroll_offset(&shell->ui))
    {
        reach_shell_note_launcher_viewport_changed(shell);
    }
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_wheel(reach_shell *shell,
                                                     const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr)
    {
        return REACH_OK;
    }
    if (!shell->has_layout)
    {
        return REACH_OK;
    }

    if (shell->clipboard_model.open && event->wheel_delta != 0 &&
        reach_rect_contains(shell->clipboard_layout.viewport, event->x, event->y))
    {
        reach_scrollbar_scroll(&shell->clipboard_model.scrollbar,
                               event->wheel_delta > 0 ? -72.0f : 72.0f);
        shell->clipboard_model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
        for (size_t index = 0; index < REACH_CLIPBOARD_MAX_ITEMS; ++index)
        {
            reach_animation_manager_animate_to(&shell->animations,
                                               reach_shell_clipboard_hover_animation_id(index),
                                               0.0f, 0.10, REACH_EASING_EASE_OUT);
        }
        shell->clipboard_surface.dirty_flags = 1;
        shell->dirty.layout = 1;
        reach_shell_request_update(shell);
        return REACH_OK;
    }
    if (!shell->ui.launcher.open)
    {
        return REACH_OK;
    }

    if (event->wheel_delta == 0 ||
        !reach_rect_contains(shell->layout.launcher.search_results, event->x, event->y))
    {
        return REACH_OK;
    }

    int32_t direction = event->wheel_delta > 0 ? -1 : 1;
    return reach_shell_scroll_launcher_results(shell, direction * REACH_SEARCH_SCROLL_STEP);
}

static void reach_shell_end_launcher_scrollbar_drag(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    reach_scrollbar_end_drag(&shell->launcher_scrollbar_drag);
    reach_shell_sync_pointer_move_subscriptions(shell);
    if (shell->launcher.window.ops.set_pointer_capture != nullptr)
    {
        (void)shell->launcher.window.ops.set_pointer_capture(shell->launcher.window.window, 0);
    }
}

static reach_result reach_shell_begin_launcher_scrollbar_drag(reach_shell *shell,
                                                              const reach_ui_event *event,
                                                              reach_launcher_hit_result hit)
{
    if (shell == nullptr || event == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    size_t old_offset = reach_ui_state_launcher_result_scroll_offset(&shell->ui);
    reach_scrollbar_layout layout = {shell->layout.launcher.search_result_scrollbar_track,
                                     shell->layout.launcher.search_result_scrollbar_thumb};
    reach_scrollbar_begin_drag(&shell->ui.launcher.result_scrollbar,
                               &shell->launcher_scrollbar_drag, &layout, (float)event->y,
                               hit.type == REACH_LAUNCHER_HIT_SCROLLBAR_THUMB);
    if (old_offset != reach_ui_state_launcher_result_scroll_offset(&shell->ui))
    {
        reach_shell_note_launcher_viewport_changed(shell);
    }

    reach_shell_sync_pointer_move_subscriptions(shell);
    if (shell->launcher.window.ops.set_pointer_capture != nullptr)
    {
        (void)shell->launcher.window.ops.set_pointer_capture(shell->launcher.window.window, 1);
    }
    return REACH_OK;
}

static reach_result reach_shell_update_launcher_scrollbar_drag(reach_shell *shell,
                                                               const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->launcher_scrollbar_drag.active)
    {
        return REACH_OK;
    }

    size_t old_offset = reach_ui_state_launcher_result_scroll_offset(&shell->ui);
    reach_scrollbar_layout layout = {shell->layout.launcher.search_result_scrollbar_track,
                                     shell->layout.launcher.search_result_scrollbar_thumb};
    reach_scrollbar_update_drag(&shell->ui.launcher.result_scrollbar,
                                &shell->launcher_scrollbar_drag, &layout, (float)event->y);
    if (old_offset != reach_ui_state_launcher_result_scroll_offset(&shell->ui))
    {
        reach_shell_note_launcher_viewport_changed(shell);
    }
    return REACH_OK;
}

static float reach_shell_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

typedef enum reach_shell_media_action
{
    REACH_SHELL_MEDIA_ACTION_PREVIOUS = 1,
    REACH_SHELL_MEDIA_ACTION_PLAY_PAUSE = 2,
    REACH_SHELL_MEDIA_ACTION_NEXT = 3
} reach_shell_media_action;

static reach_result reach_shell_execute_media_action(reach_shell *shell,
                                                     reach_shell_media_action action)
{
    if (shell == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    if ((action == REACH_SHELL_MEDIA_ACTION_PREVIOUS || action == REACH_SHELL_MEDIA_ACTION_NEXT) &&
        shell->music_widget_model.visible)
    {
        reach_shell_start_music_widget_hide_grace(shell);
    }

    reach_result result = REACH_OK;
    switch (action)
    {
    case REACH_SHELL_MEDIA_ACTION_PREVIOUS:
        result = shell->media_controls.previous_track != nullptr
                     ? shell->media_controls.previous_track(shell->media_controls.userdata)
                     : REACH_OK;
        break;
    case REACH_SHELL_MEDIA_ACTION_PLAY_PAUSE:
        result = shell->media_controls.play_pause != nullptr
                     ? shell->media_controls.play_pause(shell->media_controls.userdata)
                     : REACH_OK;
        break;
    case REACH_SHELL_MEDIA_ACTION_NEXT:
        result = shell->media_controls.next_track != nullptr
                     ? shell->media_controls.next_track(shell->media_controls.userdata)
                     : REACH_OK;
        break;
    default:
        result = REACH_OK;
        break;
    }

    return result;
}

static reach_result reach_shell_step_main_volume(reach_shell *shell, float delta)
{
    if (shell == nullptr || shell->audio_volume.get_state == nullptr ||
        shell->audio_volume.set_level == nullptr)
    {
        return REACH_OK;
    }

    reach_audio_volume_state state = {};
    if (shell->audio_volume.get_state(shell->audio_volume.userdata, &state) != REACH_OK)
    {
        return REACH_ERROR;
    }

    float level = reach_shell_clamp01(state.level + delta);
    return shell->audio_volume.set_level(shell->audio_volume.userdata, level);
}

static reach_result reach_shell_toggle_main_volume_mute(reach_shell *shell)
{
    if (shell == nullptr || shell->audio_volume.get_state == nullptr ||
        shell->audio_volume.set_muted == nullptr)
    {
        return REACH_OK;
    }

    reach_audio_volume_state state = {};
    if (shell->audio_volume.get_state(shell->audio_volume.userdata, &state) != REACH_OK)
    {
        return REACH_ERROR;
    }

    return shell->audio_volume.set_muted(shell->audio_volume.userdata, state.muted ? 0 : 1);
}

static reach_result reach_shell_step_brightness(reach_shell *shell, float delta)
{
    if (shell == nullptr || shell->system_controls.get_brightness_state == nullptr ||
        shell->system_controls.set_brightness_level == nullptr)
    {
        return REACH_OK;
    }

    reach_brightness_state state = {};
    if (shell->system_controls.get_brightness_state(shell->system_controls.userdata, &state) !=
        REACH_OK)
    {
        return REACH_ERROR;
    }
    if (!state.available)
    {
        return REACH_OK;
    }

    float level = reach_shell_clamp01(state.level + delta);
    reach_result result =
        shell->system_controls.set_brightness_level(shell->system_controls.userdata, level);
    if (result != REACH_OK)
    {
        return result;
    }

    if (shell->quick_settings_open)
    {
        reach_shell_start_quick_settings_system_refresh(shell,
                                                        REACH_SYSTEM_CONTROLS_CHANGE_BRIGHTNESS);
        shell->quick_settings.dirty_flags = 1;
        shell->dirty.render = 1;
        reach_shell_request_update(shell);
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_up(reach_shell *shell, const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    if (shell->context_menu_state.open)
    {
        reach_context_menu_hit_result context_hit = reach_context_menu_hit_test_items(
            shell->context_menu_state.item_slots, shell->context_menu_state.item_count, event->x,
            event->y);

        reach_context_menu_action context_action =
            reach_context_menu_action_for_hit(shell->context_menu_state.item_commands,
                                              shell->context_menu_state.item_count, context_hit);

        if (context_action.command != 0)
        {
            return reach_shell_execute_context_command(shell, context_action.command);
        }

        reach_shell_close_context_menu(shell);
        return REACH_OK;
    }

    if (shell->dock_drag.active)
    {
        reach_result drag_result = reach_shell_end_dock_drag(shell);
        if (drag_result != REACH_OK)
        {
            return drag_result;
        }

        if (shell->pressed_dock_index == REACH_MAX_PINNED_APPS)
        {
            return REACH_OK;
        }
    }
    else
    {
        reach_shell_release_dock_item(shell);
    }

    if (shell->quick_settings_drag.active)
    {
        reach_shell_end_quick_settings_drag(shell);
        return REACH_OK;
    }

    if (shell->launcher_scrollbar_drag.active)
    {
        reach_shell_end_launcher_scrollbar_drag(shell);
        return REACH_OK;
    }

    if (shell->clipboard_scrollbar_drag.active)
    {
        reach_scrollbar_end_drag(&shell->clipboard_scrollbar_drag);
        if (shell->clipboard_surface.window.ops.set_pointer_capture != nullptr)
        {
            (void)shell->clipboard_surface.window.ops.set_pointer_capture(
                shell->clipboard_surface.window.window, 0);
        }
        return REACH_OK;
    }

    if (shell->clipboard_model.open)
    {
        reach_clipboard_hit_result clipboard_hit = reach_clipboard_hit_test(
            &shell->clipboard_model, &shell->clipboard_layout, event->x, event->y);
        size_t pressed = shell->clipboard_model.pressed_index;
        reach_clipboard_hit_type pressed_hit_type =
            (reach_clipboard_hit_type)shell->clipboard_model.pressed_hit_type;
        uint64_t pressed_item_id = shell->clipboard_model.pressed_item_id;
        reach_clipboard_model_clear_press(&shell->clipboard_model);
        if (clipboard_hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE &&
            pressed_hit_type == REACH_CLIPBOARD_HIT_ITEM_CLOSE && clipboard_hit.index == pressed &&
            clipboard_hit.index < shell->clipboard_model.count &&
            shell->clipboard_model.items[clipboard_hit.index].id == pressed_item_id)
        {
            reach_clipboard_item removed_item = shell->clipboard_model.items[clipboard_hit.index];
            if (shell->clipboard.ops.release != nullptr)
            {
                shell->clipboard.ops.release(shell->clipboard.provider, removed_item.id);
            }
            reach_clipboard_model_remove(&shell->clipboard_model, clipboard_hit.index);
            shell->clipboard_surface.dirty_flags = 1;
            shell->dirty.layout = 1;
            reach_shell_request_update(shell);
            return REACH_OK;
        }

        if (clipboard_hit.type == REACH_CLIPBOARD_HIT_ITEM &&
            pressed_hit_type == REACH_CLIPBOARD_HIT_ITEM && clipboard_hit.index == pressed &&
            clipboard_hit.index < shell->clipboard_model.count &&
            shell->clipboard_model.items[clipboard_hit.index].id == pressed_item_id &&
            shell->clipboard.ops.restore != nullptr)
        {
            uint64_t item_id = pressed_item_id;
            if (shell->clipboard.ops.restore(shell->clipboard.provider, item_id) == REACH_OK)
            {
                (void)reach_clipboard_model_promote(&shell->clipboard_model, clipboard_hit.index);
                shell->clipboard_model.scrollbar.offset = 0.0f;
                shell->clipboard_model.scrollbar.target = 0.0f;
                reach_shell_set_clipboard_open(shell, 0);
                return REACH_OK;
            }
        }
    }

    if (shell->ui.launcher.open)
    {
        reach_launcher_hit_result launcher_hit =
            reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);

        reach_launcher_action launcher_action =
            reach_launcher_action_for_hit(&shell->ui, launcher_hit);

        int32_t launcher_pressed_match = shell->pressed_launcher_hit_type == launcher_hit.type &&
                                         shell->pressed_launcher_index == launcher_hit.index;

        shell->pressed_launcher_hit_type = REACH_LAUNCHER_HIT_NONE;
        shell->pressed_launcher_index = REACH_MAX_PINNED_APPS;

        if (launcher_action.type == REACH_LAUNCHER_ACTION_LAUNCH_PINNED && launcher_pressed_match)
        {
            reach_ui_event routed = {};
            routed.type = REACH_UI_EVENT_DOCK_APP_CLICK;
            routed.id = launcher_action.pin_id;
            return reach_shell_handle_event(shell, &routed);
        }

        if (launcher_action.type == REACH_LAUNCHER_ACTION_OPEN_RESULT && launcher_pressed_match)
        {
            return reach_shell_open_launcher_result(shell);
        }
    }

    if (shell->tray_state.popup_open && shell->tray_provider.ops.activate != nullptr)
    {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(
            &shell->tray_state.model, shell->tray.last_bounds, event->x, event->y);

        reach_tray_feature_action tray_action = reach_tray_action_for_hit(
            &shell->tray_state.model, tray_hit, REACH_TRAY_ACTION_LEFT_CLICK);

        if (tray_action.type != REACH_TRAY_FEATURE_ACTION_NONE)
        {
            return reach_shell_execute_tray_action(shell, tray_action);
        }
    }

    reach_dock_hit_result dock_hit = reach_shell_dock_hit_test_event(shell, event);

    reach_music_widget_action_type music_action =
        reach_shell_music_widget_hit_test_event(shell, event);
    if (music_action != REACH_MUSIC_WIDGET_ACTION_NONE)
    {
        reach_music_widget_action_type pressed_action = shell->pressed_music_widget_action;
        shell->pressed_music_widget_action = REACH_MUSIC_WIDGET_ACTION_NONE;
        if (pressed_action == music_action)
        {
            if (music_action == REACH_MUSIC_WIDGET_ACTION_PREVIOUS)
            {
                return reach_shell_execute_media_action(shell, REACH_SHELL_MEDIA_ACTION_PREVIOUS);
            }
            if (music_action == REACH_MUSIC_WIDGET_ACTION_PLAY_PAUSE)
            {
                return reach_shell_execute_media_action(shell, REACH_SHELL_MEDIA_ACTION_PLAY_PAUSE);
            }
            if (music_action == REACH_MUSIC_WIDGET_ACTION_NEXT)
            {
                return reach_shell_execute_media_action(shell, REACH_SHELL_MEDIA_ACTION_NEXT);
            }
        }
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON)
    {
        reach_shell_toggle_tray_popup(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON)
    {
        reach_shell_toggle_quick_settings(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON)
    {
        if (shell->suppress_power_button_release)
        {
            shell->suppress_power_button_release = 0;
            return REACH_OK;
        }

        if (shell->context_menu_state.open && shell->context_menu_state.power_open)
        {
            reach_shell_close_context_menu(shell);
            return REACH_OK;
        }

        return reach_shell_show_power_context_menu(shell);
    }

    if (shell->tray_state.popup_open &&
        !reach_rect_contains(shell->tray.last_bounds, event->x, event->y))
    {
        reach_shell_set_tray_popup_open(shell, 0);
    }

    if (dock_hit.type == REACH_DOCK_HIT_ITEM && shell->pressed_dock_index == dock_hit.index)
    {
        shell->pressed_dock_index = REACH_MAX_PINNED_APPS;

        reach_dock_item_action action =
            reach_dock_item_action_for_index(&shell->dock_model, dock_hit.index);

        reach_shell_release_dock_item(shell);
        shell->dock.dirty_flags = 1;
        reach_shell_request_dock_visibility_update(shell);
        reach_shell_request_update(shell);

        return reach_shell_execute_dock_item_action(shell, action);
    }

    shell->pressed_dock_index = REACH_MAX_PINNED_APPS;
    shell->pressed_music_widget_action = REACH_MUSIC_WIDGET_ACTION_NONE;
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_down(reach_shell *shell, const reach_ui_event *event,
                                                    reach_surface_role source)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    reach_clipboard_model_clear_press(&shell->clipboard_model);

    reach_shell_clear_sticky_dock_feedback(shell);

    if (source == REACH_SURFACE_CLIPBOARD && shell->clipboard_model.open)
    {
        reach_clipboard_hit_result clipboard_hit = reach_clipboard_hit_test(
            &shell->clipboard_model, &shell->clipboard_layout, event->x, event->y);
        if (clipboard_hit.type == REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB ||
            clipboard_hit.type == REACH_CLIPBOARD_HIT_SCROLLBAR_TRACK)
        {
            reach_scrollbar_begin_drag(&shell->clipboard_model.scrollbar,
                                       &shell->clipboard_scrollbar_drag,
                                       &shell->clipboard_layout.scrollbar, (float)event->y,
                                       clipboard_hit.type == REACH_CLIPBOARD_HIT_SCROLLBAR_THUMB);
            if (shell->clipboard_surface.window.ops.set_pointer_capture != nullptr)
            {
                (void)shell->clipboard_surface.window.ops.set_pointer_capture(
                    shell->clipboard_surface.window.window, 1);
            }
            shell->clipboard_surface.dirty_flags = 1;
            shell->dirty.layout = 1;
            return REACH_OK;
        }
        if (clipboard_hit.type == REACH_CLIPBOARD_HIT_ITEM ||
            clipboard_hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE)
        {
            shell->clipboard_model.pressed_index = clipboard_hit.index;
            shell->clipboard_model.pressed_hit_type = clipboard_hit.type;
            shell->clipboard_model.pressed_item_id =
                clipboard_hit.index < shell->clipboard_model.count
                    ? shell->clipboard_model.items[clipboard_hit.index].id
                    : 0;
            return REACH_OK;
        }
    }

    if (shell->quick_settings_open)
    {
        reach_dock_hit_result dock_hit = reach_shell_dock_hit_test_event(shell, event);

        if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON)
        {
            reach_shell_press_quick_settings_button(shell);
            return REACH_OK;
        }

        if (reach_rect_contains(shell->quick_settings_bounds, event->x, event->y))
        {
            return reach_shell_begin_quick_settings_drag_if_hit(shell, event);
        }

        reach_shell_set_quick_settings_open(shell, 0);
        return REACH_OK;
    }

    if (shell->context_menu_state.open)
    {
        reach_dock_hit_result dock_hit = reach_shell_dock_hit_test_event(shell, event);

        if (shell->context_menu_state.power_open && dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON)
        {
            reach_shell_close_context_menu(shell);
            reach_shell_clear_sticky_dock_feedback(shell);
            shell->suppress_power_button_release = 1;
            return REACH_OK;
        }

        if (!reach_rect_contains(shell->context_menu_state.bounds, event->x, event->y))
        {
            reach_shell_close_context_menu(shell);
        }
        return REACH_OK;
    }

    reach_dock_hit_result dock_hit = reach_shell_dock_hit_test_event(shell, event);

    reach_music_widget_action_type music_action =
        reach_shell_music_widget_hit_test_event(shell, event);
    if (music_action != REACH_MUSIC_WIDGET_ACTION_NONE)
    {
        shell->pressed_music_widget_action = music_action;
        return REACH_OK;
    }

    if (shell->ui.launcher.open)
    {
        reach_launcher_hit_result launcher_hit =
            reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);

        if (launcher_hit.type == REACH_LAUNCHER_HIT_NONE &&
            !reach_rect_contains(shell->layout.launcher.bounds, event->x, event->y))
        {
            if (dock_hit.type != REACH_DOCK_HIT_NONE)
            {
                reach_shell_close_launcher_without_focus_restore(shell);
            }
            else
            {
                reach_shell_close_launcher(shell);
                return REACH_OK;
            }
        }
        else
        {
            if (launcher_hit.type == REACH_LAUNCHER_HIT_SCROLLBAR_THUMB ||
                launcher_hit.type == REACH_LAUNCHER_HIT_SCROLLBAR_TRACK)
            {
                return reach_shell_begin_launcher_scrollbar_drag(shell, event, launcher_hit);
            }

            if (launcher_hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
                launcher_hit.index < shell->ui.launcher.result_count)
            {
                (void)reach_ui_state_set_launcher_selected_result(&shell->ui, launcher_hit.index);
                shell->launcher.dirty_flags = 1;
            }

            shell->pressed_launcher_hit_type = launcher_hit.type;
            shell->pressed_launcher_index = launcher_hit.index;
            return REACH_OK;
        }
    }

    if (shell->suppress_power_button_release && dock_hit.type != REACH_DOCK_HIT_POWER_BUTTON)
    {
        shell->suppress_power_button_release = 0;
    }

    if (dock_hit.type == REACH_DOCK_HIT_TRAY_BUTTON)
    {
        reach_shell_press_tray_button(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_QUICK_SETTINGS_BUTTON)
    {
        reach_shell_press_quick_settings_button(shell);
        return REACH_OK;
    }

    if (dock_hit.type == REACH_DOCK_HIT_POWER_BUTTON)
    {
        reach_shell_press_power_button(shell);
        return REACH_OK;
    }

    if (shell->tray_state.popup_open)
    {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(
            &shell->tray_state.model, shell->tray.last_bounds, event->x, event->y);

        if (tray_hit.type == REACH_TRAY_HIT_ITEM)
        {
            reach_shell_press_tray_item(shell, tray_hit.index);
            return REACH_OK;
        }

        if (tray_hit.type == REACH_TRAY_HIT_NONE)
        {
            return REACH_OK;
        }
    }

    if (dock_hit.type == REACH_DOCK_HIT_ITEM)
    {
        size_t index = dock_hit.index;
        shell->pressed_dock_index = index;

        reach_shell_press_dock_item(shell, index);
        reach_shell_begin_dock_drag(shell, index, event);

        return REACH_OK;
    }

    reach_shell_release_tray_item(shell);
    shell->pressed_dock_index = REACH_MAX_PINNED_APPS;
    shell->pressed_music_widget_action = REACH_MUSIC_WIDGET_ACTION_NONE;
    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_move(reach_shell *shell, const reach_ui_event *event,
                                                    reach_surface_role source)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    if (source == REACH_SURFACE_CONTEXT_MENU && shell->context_menu_state.open)
    {
        reach_context_menu_hit_result context_hit = reach_context_menu_hit_test_items(
            shell->context_menu_state.item_slots, shell->context_menu_state.item_count, event->x,
            event->y);

        size_t hovered_context = context_hit.hit ? context_hit.index : REACH_MAX_PINNED_APPS;

        if (shell->context_menu_state.hovered_index != hovered_context)
        {
            shell->context_menu_state.hovered_index = hovered_context;
            shell->context_menu.dirty_flags = 1;
        }

        return REACH_OK;
    }

    if (shell->quick_settings_drag.active)
    {
        return reach_shell_update_quick_settings_drag(shell, event);
    }

    if (shell->launcher_scrollbar_drag.active)
    {
        return reach_shell_update_launcher_scrollbar_drag(shell, event);
    }

    if (shell->clipboard_scrollbar_drag.active)
    {
        reach_scrollbar_update_drag(&shell->clipboard_model.scrollbar,
                                    &shell->clipboard_scrollbar_drag,
                                    &shell->clipboard_layout.scrollbar, (float)event->y);
        shell->clipboard_model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
        shell->clipboard_surface.dirty_flags = 1;
        shell->dirty.layout = 1;
        return REACH_OK;
    }

    if (source == REACH_SURFACE_CLIPBOARD && shell->clipboard_model.open)
    {
        reach_clipboard_hit_result hit = reach_clipboard_hit_test(
            &shell->clipboard_model, &shell->clipboard_layout, event->x, event->y);
        size_t next =
            (hit.type == REACH_CLIPBOARD_HIT_ITEM || hit.type == REACH_CLIPBOARD_HIT_ITEM_CLOSE)
                ? hit.index
                : REACH_CLIPBOARD_MAX_ITEMS;
        if (next != shell->clipboard_model.hovered_index)
        {
            size_t previous = shell->clipboard_model.hovered_index;
            shell->clipboard_model.hovered_index = next;
            if (previous < REACH_CLIPBOARD_MAX_ITEMS)
            {
                reach_animation_manager_animate_to(
                    &shell->animations, reach_shell_clipboard_hover_animation_id(previous), 0.0f,
                    0.12, REACH_EASING_EASE_OUT);
            }
            if (next < REACH_CLIPBOARD_MAX_ITEMS)
            {
                reach_animation_manager_animate_to(&shell->animations,
                                                   reach_shell_clipboard_hover_animation_id(next),
                                                   1.0f, 0.14, REACH_EASING_EASE_OUT);
            }
            shell->clipboard_surface.dirty_flags = 1;
            reach_shell_request_update(shell);
        }
        return REACH_OK;
    }

    if (shell->dock_drag.active)
    {
        return reach_shell_update_dock_drag(shell, event);
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_middle(reach_shell *shell,
                                                      const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    reach_shell_release_dock_item(shell);
    reach_shell_release_tray_item(shell);

    reach_dock_hit_result dock_hit = reach_shell_dock_hit_test_event(shell, event);

    if (dock_hit.type == REACH_DOCK_HIT_ITEM)
    {
        return reach_shell_launch_dock_item(shell, dock_hit.index, 1);
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_leave(reach_shell *shell, reach_surface_role source)
{
    if (shell == nullptr)
    {
        return REACH_OK;
    }

    if (source == REACH_SURFACE_DOCK)
    {
        reach_shell_request_dock_visibility_update(shell);
    }
    else if (source == REACH_SURFACE_CONTEXT_MENU &&
             shell->context_menu_state.hovered_index != REACH_MAX_PINNED_APPS)
    {
        shell->context_menu_state.hovered_index = REACH_MAX_PINNED_APPS;
        shell->context_menu.dirty_flags = 1;
    }
    else if (source == REACH_SURFACE_CLIPBOARD &&
             shell->clipboard_model.hovered_index < REACH_CLIPBOARD_MAX_ITEMS)
    {
        size_t previous = shell->clipboard_model.hovered_index;
        shell->clipboard_model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
        reach_animation_manager_animate_to(&shell->animations,
                                           reach_shell_clipboard_hover_animation_id(previous), 0.0f,
                                           0.12, REACH_EASING_EASE_OUT);
        shell->clipboard_surface.dirty_flags = 1;
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_cancel(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return REACH_OK;
    }

    if (shell->quick_settings_drag.active)
    {
        reach_shell_end_quick_settings_drag(shell);
    }

    if (shell->launcher_scrollbar_drag.active)
    {
        reach_shell_end_launcher_scrollbar_drag(shell);
    }
    if (shell->clipboard_scrollbar_drag.active)
    {
        reach_scrollbar_end_drag(&shell->clipboard_scrollbar_drag);
        if (shell->clipboard_surface.window.ops.set_pointer_capture != nullptr)
        {
            (void)shell->clipboard_surface.window.ops.set_pointer_capture(
                shell->clipboard_surface.window.window, 0);
        }
    }

    if (shell->dock_drag.active)
    {
        return reach_shell_end_dock_drag(shell);
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_pointer_context(reach_shell *shell,
                                                       const reach_ui_event *event)
{
    if (shell == nullptr || event == nullptr || !shell->has_layout)
    {
        return REACH_OK;
    }

    reach_shell_clear_sticky_dock_feedback(shell);

    if (shell->context_menu_state.open)
    {
        reach_shell_close_context_menu(shell);
    }

    if (shell->quick_settings_open)
    {
        reach_shell_set_quick_settings_open(shell, 0);
    }

    if (shell->ui.launcher.open)
    {
        reach_launcher_hit_result launcher_hit =
            reach_launcher_hit_test(&shell->ui, &shell->layout.launcher, event->x, event->y);

        if (launcher_hit.type == REACH_LAUNCHER_HIT_SEARCH_RESULT &&
            launcher_hit.index < shell->ui.launcher.result_count)
        {
            const reach_search_candidate *result = &shell->ui.launcher.results[launcher_hit.index];
            if (result->kind == REACH_SEARCH_RESULT_APP)
            {
                reach_result open_result =
                    reach_shell_reveal_launcher_result(shell, launcher_hit.index);
                if (open_result == REACH_OK)
                {
                    reach_shell_close_launcher(shell);
                }
                return open_result;
            }

            return REACH_OK;
        }

        if (reach_rect_contains(shell->layout.launcher.bounds, event->x, event->y))
        {
            return REACH_OK;
        }
    }

    if (shell->tray_state.popup_open && shell->tray_provider.ops.activate != nullptr)
    {
        reach_tray_hit_result tray_hit = reach_tray_hit_test_popup(
            &shell->tray_state.model, shell->tray.last_bounds, event->x, event->y);

        reach_tray_feature_action tray_action = reach_tray_action_for_hit(
            &shell->tray_state.model, tray_hit, REACH_TRAY_ACTION_RIGHT_CLICK);

        if (tray_action.type != REACH_TRAY_FEATURE_ACTION_NONE)
        {
            reach_shell_press_tray_item(shell, tray_action.item_index);
            return reach_shell_execute_tray_action(shell, tray_action);
        }
    }

    reach_dock_hit_result dock_hit = reach_shell_dock_hit_test_event(shell, event);

    if (dock_hit.type == REACH_DOCK_HIT_ITEM)
    {
        size_t index = dock_hit.index;

        shell->feedback.dock_pressed = 1;
        shell->feedback.dock_sticky = 0;

        reach_shell_set_dock_click_feedback_immediate(shell, index, 0.50f);
        (void)reach_shell_render_dock_surface(shell, &shell->layout.dock);

        reach_result result =
            reach_shell_show_dock_app_context_menu(shell, index, event->x, event->y);

        reach_shell_stick_dock_item(shell);
        (void)reach_shell_render_dock_surface(shell, &shell->layout.dock);

        return result;
    }

    return REACH_OK;
}

static reach_result reach_shell_handle_surface_event(reach_shell *shell,
                                                     const reach_ui_event *event,
                                                     reach_surface_role source);

static void reach_shell_on_surface_event(void *user, const reach_ui_event *event,
                                         reach_surface_role source)
{
    reach_shell *shell = static_cast<reach_shell *>(user);
    if (shell != nullptr && event != nullptr)
    {
        (void)reach_shell_handle_surface_event(shell, event, source);
    }
}

static void reach_shell_begin_dock_pointer_sequence(reach_shell *shell)
{
    if (shell == nullptr || shell->dock_reveal.pointer_sequence_active)
    {
        return;
    }
    shell->dock_reveal.pointer_sequence_active = 1;
    reach_shell_sync_pointer_move_subscriptions(shell);
    if (shell->dock.window.ops.set_pointer_capture != nullptr)
    {
        (void)shell->dock.window.ops.set_pointer_capture(shell->dock.window.window, 1);
    }
    reach_shell_request_dock_visibility_update(shell);
}

static void reach_shell_end_dock_pointer_sequence(reach_shell *shell)
{
    if (shell == nullptr || !shell->dock_reveal.pointer_sequence_active)
    {
        return;
    }
    if (shell->dock.window.ops.set_pointer_capture != nullptr)
    {
        (void)shell->dock.window.ops.set_pointer_capture(shell->dock.window.window, 0);
    }
    shell->dock_reveal.pointer_sequence_active = 0;
    reach_shell_sync_pointer_move_subscriptions(shell);
    reach_shell_request_dock_visibility_update(shell);
}

void reach_shell_on_launcher_window_event(void *user, const reach_ui_event *event)
{
    reach_shell_on_surface_event(user, event, REACH_SURFACE_LAUNCHER);
}

void reach_shell_on_dock_window_event(void *user, const reach_ui_event *event)
{
    reach_shell_on_surface_event(user, event, REACH_SURFACE_DOCK);
}

void reach_shell_on_tray_window_event(void *user, const reach_ui_event *event)
{
    reach_shell_on_surface_event(user, event, REACH_SURFACE_TRAY_MENU);
}

void reach_shell_on_switcher_window_event(void *user, const reach_ui_event *event)
{
    reach_shell_on_surface_event(user, event, REACH_SURFACE_SWITCHER);
}

void reach_shell_on_context_menu_window_event(void *user, const reach_ui_event *event)
{
    reach_shell_on_surface_event(user, event, REACH_SURFACE_CONTEXT_MENU);
}

void reach_shell_on_quick_settings_window_event(void *user, const reach_ui_event *event)
{
    reach_shell_on_surface_event(user, event, REACH_SURFACE_QUICK_SETTINGS);
}

void reach_shell_on_clipboard_window_event(void *user, const reach_ui_event *event)
{
    reach_shell_on_surface_event(user, event, REACH_SURFACE_CLIPBOARD);
}

static reach_result reach_shell_handle_surface_event(reach_shell *shell,
                                                     const reach_ui_event *event,
                                                     reach_surface_role source)
{
    REACH_ASSERT(shell != nullptr);
    REACH_ASSERT(event != nullptr);
    if (shell == nullptr || event == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_ui_intent intent = {};

    if (event->type == REACH_UI_EVENT_CLIPBOARD_CHANGED)
    {
        shell->clipboard_refresh_requested = 1;
        reach_shell_request_update(shell);
        return REACH_OK;
    }

    if (reach_shell_game_mode_enabled(shell) && !reach_shell_game_mode_allows_event(event->type))
    {
        return REACH_OK;
    }

    int32_t launcher_was_open = shell->ui.launcher.open;

    if (source == REACH_SURFACE_DOCK && event->type == REACH_UI_EVENT_POINTER_DOWN)
    {
        reach_shell_begin_dock_pointer_sequence(shell);
    }

    if (event->type == REACH_UI_EVENT_POINTER_UP)
    {
        reach_result result = reach_shell_handle_pointer_up(shell, event);
        reach_shell_end_dock_pointer_sequence(shell);
        return result;
    }

    if (event->type == REACH_UI_EVENT_POINTER_DOWN)
    {
        return reach_shell_handle_pointer_down(shell, event, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_MOVE)
    {
        return reach_shell_handle_pointer_move(shell, event, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_WHEEL)
    {
        return reach_shell_handle_pointer_wheel(shell, event);
    }

    if (event->type == REACH_UI_EVENT_POINTER_MIDDLE)
    {
        return reach_shell_handle_pointer_middle(shell, event);
    }

    if (event->type == REACH_UI_EVENT_POINTER_LEAVE)
    {
        return reach_shell_handle_pointer_leave(shell, source);
    }

    if (event->type == REACH_UI_EVENT_POINTER_CANCEL)
    {
        reach_result result = reach_shell_handle_pointer_cancel(shell);
        reach_shell_end_dock_pointer_sequence(shell);
        return result;
    }

    if (event->type == REACH_UI_EVENT_POINTER_CONTEXT)
    {
        return reach_shell_handle_pointer_context(shell, event);
    }

    if (event->type == REACH_UI_EVENT_WALLPAPER_CHANGED)
    {
        reach_shell_reload_wallpaper(shell, 1);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_CONFIG_CHANGED)
    {
        if (reach_shell_apply_config_reload_result(shell))
        {
            return REACH_OK;
        }
        return reach_shell_schedule_config_reload(shell);
    }

    if (event->type == REACH_UI_EVENT_LAUNCHER_SEARCH_READY)
    {
        reach_shell_apply_launcher_search_results(shell);
        reach_shell_apply_launcher_result_icon_results(shell);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_DISPLAY_CHANGED)
    {
        shell->dirty.monitors = 1;
        shell->dirty.layout = 1;
        shell->dock.dirty_flags = 1;
        shell->launcher.dirty_flags = 1;
        shell->tray.dirty_flags = 1;
        shell->switcher.dirty_flags = 1;
        shell->context_menu.dirty_flags = 1;
        shell->quick_settings.dirty_flags = 1;
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOW_STATE_CHANGED)
    {
        if (shell->window_manager.ops.refresh != nullptr)
        {
            (void)shell->window_manager.ops.refresh(shell->window_manager.manager);
        }

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
        (void)reach_shell_update_game_mode(shell);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_WINDOWS_D_MINIMIZE_ALL)
    {
        (void)reach_shell_schedule_minimize_open_windows(shell);
        return REACH_OK;
    }

    if (event->type == REACH_UI_EVENT_MEDIA_PREVIOUS)
    {
        return reach_shell_execute_media_action(shell, REACH_SHELL_MEDIA_ACTION_PREVIOUS);
    }

    if (event->type == REACH_UI_EVENT_MEDIA_PLAY_PAUSE)
    {
        return reach_shell_execute_media_action(shell, REACH_SHELL_MEDIA_ACTION_PLAY_PAUSE);
    }

    if (event->type == REACH_UI_EVENT_MEDIA_NEXT)
    {
        return reach_shell_execute_media_action(shell, REACH_SHELL_MEDIA_ACTION_NEXT);
    }

    if (event->type == REACH_UI_EVENT_VOLUME_UP)
    {
        return reach_shell_step_main_volume(shell, 0.02f);
    }

    if (event->type == REACH_UI_EVENT_VOLUME_DOWN)
    {
        return reach_shell_step_main_volume(shell, -0.02f);
    }

    if (event->type == REACH_UI_EVENT_VOLUME_MUTE)
    {
        return reach_shell_toggle_main_volume_mute(shell);
    }

    if (event->type == REACH_UI_EVENT_BRIGHTNESS_UP)
    {
        return reach_shell_step_brightness(shell, 0.02f);
    }

    if (event->type == REACH_UI_EVENT_BRIGHTNESS_DOWN)
    {
        return reach_shell_step_brightness(shell, -0.02f);
    }

    if (event->type == REACH_UI_EVENT_ALT_TAB_BEGIN || event->type == REACH_UI_EVENT_ALT_TAB_NEXT ||
        event->type == REACH_UI_EVENT_ALT_TAB_PREVIOUS ||
        event->type == REACH_UI_EVENT_ALT_TAB_COMMIT ||
        event->type == REACH_UI_EVENT_ALT_TAB_CANCEL)
    {
        return reach_shell_handle_switcher_event(shell, event);
    }

    if (event->type == REACH_UI_EVENT_WINDOWS_KEY && !shell->ui.launcher.open)
    {
        reach_shell_remember_launcher_restore_window(shell);
    }
    if (event->type == REACH_UI_EVENT_WINDOWS_KEY)
    {
        reach_shell_toggle_clipboard(shell);
    }
    else if (event->type == REACH_UI_EVENT_ESCAPE)
    {
        reach_shell_set_clipboard_open(shell, 0);
    }

    reach_result result = reach_ui_handle_event(&shell->ui, event, &intent);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_shell_mark_dirty_for_event(shell, event);

    if (launcher_was_open != shell->ui.launcher.open)
    {
        reach_shell_surface_transition_set(shell, &shell->launcher_transition,
                                           shell->ui.launcher.open);
        if (!shell->ui.launcher.open)
        {
            reach_shell_cleanup_closed_launcher(shell);
        }

        reach_shell_sync_popup_mouse_hook(shell);
    }

    else if (intent.type == REACH_UI_INTENT_LAUNCH_APP)
    {
        for (size_t index = 0; index < shell->ui.pinned_app_count; ++index)
        {
            if (shell->ui.pinned_apps[index].id == intent.id &&
                shell->app_launcher.ops.launch != nullptr)
            {
                reach_app_launch_request request = {};

                reach_copy_utf16(request.path, 260, shell->ui.pinned_apps[index].path);

                reach_copy_utf16(request.arguments, 260, shell->ui.pinned_apps[index].arguments);

                if (shell->ui.launcher.open)
                {
                    return reach_shell_defer_app_launch_until_launcher_closed(shell, &request);
                }
                return reach_shell_schedule_app_launch(shell, &request);
            }
        }
    }
    else if (intent.type == REACH_UI_INTENT_OPEN_LAUNCHER_RESULT)
    {
        return reach_shell_open_launcher_result(shell);
    }

    return REACH_OK;
}

reach_result reach_shell_handle_event(reach_shell *shell, const reach_ui_event *event)
{
    return reach_shell_handle_surface_event(shell, event, REACH_SURFACE_SETTINGS);
}
