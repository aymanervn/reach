#include "host_internal.h"

#include "reach/features/battery.h"
#include "reach/features/clipboard.h"
#include "reach/features/context_menu.h"
#include "reach/features/dock.h"
#include "reach/features/launcher.h"
#include "reach/features/quick_settings.h"
#include "reach/features/screenshot.h"
#include "reach/features/stage.h"
#include "reach/features/switcher.h"
#include "reach/features/system_hud.h"
#include "reach/features/top_bar.h"

static reach_result reach_feature_attach_launcher(void *capsule,
                                                  const reach_feature_dependencies *dependencies)
{
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    reach_launcher_attach_search(launcher,
                                 dependencies != nullptr ? dependencies->search : nullptr);
    reach_launcher_attach_icons(launcher, dependencies != nullptr ? dependencies->icons : nullptr);
    reach_launcher_set_terminal_icon_ref(
        launcher, dependencies != nullptr ? dependencies->terminal_icon_ref : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_dock(void *capsule,
                                              const reach_feature_dependencies *dependencies)
{
    reach_dock_attach_services(static_cast<reach_dock *>(capsule),
                               dependencies != nullptr ? dependencies->icons : nullptr,
                               dependencies != nullptr ? dependencies->windows : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_top_bar(void *capsule,
                                                 const reach_feature_dependencies *dependencies)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    reach_top_bar_attach_services(top_bar,
                                  dependencies != nullptr ? dependencies->now_playing : nullptr,
                                  dependencies != nullptr ? dependencies->icons : nullptr,
                                  dependencies != nullptr ? dependencies->windows : nullptr,
                                  dependencies != nullptr ? dependencies->clock : nullptr,
                                  dependencies != nullptr ? dependencies->input_language : nullptr,
                                  dependencies != nullptr ? dependencies->tray : nullptr);
    reach_top_bar_attach_app_control(top_bar,
                                     dependencies != nullptr ? dependencies->app_control : nullptr);
    reach_top_bar_attach_status(top_bar,
                                dependencies != nullptr ? dependencies->system_status : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_stage(void *capsule,
                                               const reach_feature_dependencies *dependencies)
{
    reach_stage_attach_services(static_cast<reach_stage *>(capsule),
                                dependencies != nullptr ? dependencies->windows : nullptr,
                                dependencies != nullptr ? dependencies->icons : nullptr,
                                dependencies != nullptr ? dependencies->app_control : nullptr);
    return REACH_OK;
}

static reach_result
reach_feature_attach_context_menu(void *capsule, const reach_feature_dependencies *dependencies)
{
    reach_context_menu_attach_services(static_cast<reach_context_menu *>(capsule),
                                       dependencies != nullptr ? dependencies->windows : nullptr,
                                       dependencies != nullptr ? dependencies->icons : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_switcher(void *capsule,
                                                  const reach_feature_dependencies *dependencies)
{
    reach_switcher_attach_services(static_cast<reach_switcher *>(capsule),
                                   dependencies != nullptr ? dependencies->icons : nullptr,
                                   dependencies != nullptr ? dependencies->windows : nullptr);
    return REACH_OK;
}

static reach_result
reach_feature_attach_quick_settings(void *capsule, const reach_feature_dependencies *dependencies)
{
    reach_quick_settings_attach_services(
        static_cast<reach_quick_settings *>(capsule),
        dependencies != nullptr ? dependencies->system_status : nullptr,
        dependencies != nullptr ? dependencies->wifi : nullptr,
        dependencies != nullptr ? dependencies->bluetooth : nullptr,
        dependencies != nullptr ? dependencies->profile : nullptr,
        dependencies != nullptr ? dependencies->power_session : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_battery(void *capsule,
                                                 const reach_feature_dependencies *dependencies)
{
    reach_battery_attach_services(static_cast<reach_battery *>(capsule),
                                  dependencies != nullptr ? dependencies->system_status : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_system_hud(void *capsule,
                                                    const reach_feature_dependencies *dependencies)
{
    reach_system_hud_attach_now_playing(static_cast<reach_system_hud *>(capsule),
                                        dependencies != nullptr ? dependencies->now_playing
                                                                : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_clipboard(void *capsule,
                                                   const reach_feature_dependencies *dependencies)
{
    if (dependencies != nullptr &&
        (dependencies->clipboard == nullptr || dependencies->clipboard->provider == nullptr))
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_clipboard_feature_attach_port(
        static_cast<reach_clipboard_feature *>(capsule),
        dependencies != nullptr ? dependencies->clipboard : nullptr,
        dependencies != nullptr ? dependencies->request_update : nullptr,
        dependencies != nullptr ? dependencies->request_update_user : nullptr);
    return REACH_OK;
}

static reach_result reach_feature_attach_screenshot(void *capsule,
                                                    const reach_feature_dependencies *dependencies)
{
    reach_screenshot_attach_ports(static_cast<reach_screenshot *>(capsule),
                                  dependencies != nullptr ? dependencies->screen_capture : nullptr,
                                  dependencies != nullptr ? dependencies->clipboard : nullptr);
    return REACH_OK;
}

template <reach_result (*Attach)(void *, const reach_feature_dependencies *)>
static void reach_feature_detach(void *capsule)
{
    (void)Attach(capsule, nullptr);
}

static reach_result reach_feature_start_clipboard(void *capsule)
{
    return reach_clipboard_feature_start(static_cast<reach_clipboard_feature *>(capsule));
}

static void reach_feature_stop_clipboard(void *capsule)
{
    reach_clipboard_feature_stop(static_cast<reach_clipboard_feature *>(capsule));
}

static reach_result reach_feature_start_context_menu(void *capsule)
{
    reach_context_menu_force_close(static_cast<reach_context_menu *>(capsule));
    return REACH_OK;
}

static reach_result reach_feature_start_quick_settings(void *capsule)
{
    reach_quick_settings_force_close(static_cast<reach_quick_settings *>(capsule));
    return REACH_OK;
}

static void reach_feature_stop_launcher(void *capsule)
{
    reach_launcher_cancel_search(static_cast<reach_launcher *>(capsule));
}

static void reach_feature_stop_tray(void *capsule)
{
    reach_top_bar_set_tray_popup_open(static_cast<reach_top_bar *>(capsule), 0);
}

static void reach_feature_stop_switcher(void *capsule)
{
    reach_switcher_force_close(static_cast<reach_switcher *>(capsule));
}

static void reach_feature_stop_stage(void *capsule)
{
    reach_stage_force_close(static_cast<reach_stage *>(capsule));
}

static void reach_feature_stop_context_menu(void *capsule)
{
    reach_context_menu_force_close(static_cast<reach_context_menu *>(capsule));
}

static void reach_feature_stop_quick_settings(void *capsule)
{
    reach_quick_settings_force_close(static_cast<reach_quick_settings *>(capsule));
}

static void reach_feature_stop_battery(void *capsule)
{
    reach_battery_force_close(static_cast<reach_battery *>(capsule));
}

static void reach_feature_stop_system_hud(void *capsule)
{
    reach_system_hud_force_close(static_cast<reach_system_hud *>(capsule));
}

static int32_t reach_feature_control_stage_open(void *capsule, int32_t open,
                                                reach_feature_tick_result *out)
{
    int32_t changed = reach_stage_set_open(static_cast<reach_stage *>(capsule), open);
    if (out != nullptr)
    {
        out->redraw = 1;
        out->request_update = 1;
    }
    return changed;
}

static void reach_feature_notify_stage(void *capsule,
                                       const reach_feature_notification *notification,
                                       reach_feature_tick_result *out)
{
    reach_stage *stage = static_cast<reach_stage *>(capsule);
    if (stage == nullptr || notification == nullptr || out == nullptr)
    {
        return;
    }
    if (notification->kind == REACH_FEATURE_NOTIFICATION_DISPLAY_CHANGED)
    {
        reach_stage_set_display(stage, &notification->display);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_CONFIG_CHANGED &&
             notification->config != nullptr && notification->config->stage_animation_ms > 0)
    {
        reach_stage_set_animation_seconds(stage, (float)notification->config->stage_animation_ms /
                                                     1000.0f);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED &&
             reach_stage_sync_windows(stage))
    {
        out->redraw = 1;
        out->request_update = 1;
    }
}

static int32_t reach_feature_control_context_menu_open(void *capsule, int32_t open,
                                                       reach_feature_tick_result *out)
{
    int32_t changed = reach_context_menu_set_open(static_cast<reach_context_menu *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    return changed;
}

static void reach_feature_notify_dock(void *capsule, const reach_feature_notification *notification,
                                      reach_feature_tick_result *out)
{
    reach_dock *dock = static_cast<reach_dock *>(capsule);
    if (dock == nullptr || notification == nullptr || out == nullptr)
    {
        return;
    }
    if (notification->kind == REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED)
    {
        if (notification->windows.items_changed)
        {
            reach_dock_mark_items_changed(dock);
        }
        if (notification->windows.icon_identity_changed)
        {
            out->redraw = 1;
        }
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_MEDIA_ACTION)
    {
        out->redraw = 1;
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_PINNED_APPS_CHANGED)
    {
        reach_dock_apply_pinned_apps(dock, notification->pinned_apps,
                                     notification->pinned_app_count);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_ICONS_RETAIN)
    {
        reach_dock_touch_icons(dock, notification->icon_size_px);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_CONFIG_CHANGED &&
             notification->config != nullptr)
    {
        reach_dock_apply_config(dock, notification->config->dock_height,
                                notification->config->dock_size);
        out->relayout = 1;
        out->redraw = 1;
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_POPUPS_CLOSED &&
             reach_dock_clear_context_feedback(dock))
    {
        out->redraw = 1;
    }
}

static void reach_feature_notify_top_bar(void *capsule,
                                         const reach_feature_notification *notification,
                                         reach_feature_tick_result *out)
{
    if (capsule == nullptr || notification == nullptr || out == nullptr)
    {
        return;
    }

    if (notification->kind == REACH_FEATURE_NOTIFICATION_SYSTEM_STATS_CHANGED)
    {
        int32_t layout_changed = 0;
        if (reach_top_bar_apply_stats(static_cast<reach_top_bar *>(capsule),
                                      &notification->system_stats, &notification->text_measure,
                                      notification->dpi_scale, &layout_changed))
        {
            out->redraw = 1;
            out->relayout = layout_changed;
        }
        return;
    }

    if (notification->kind != REACH_FEATURE_NOTIFICATION_CONFIG_CHANGED ||
        notification->config == nullptr)
    {
        return;
    }

    if (reach_top_bar_apply_config(static_cast<reach_top_bar *>(capsule),
                                   notification->config->top_bar_style,
                                   notification->config->top_bar_mode))
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    if (reach_top_bar_apply_icon_only(static_cast<reach_top_bar *>(capsule),
                                      notification->config->top_bar_icon_only))
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
}

static int32_t reach_feature_control_launcher_open(void *capsule, int32_t open,
                                                   reach_feature_tick_result *out)
{
    int32_t changed = reach_launcher_set_open(static_cast<reach_launcher *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    return changed;
}

static void reach_feature_control_launcher_hidden(void *capsule, reach_feature_tick_result *out)
{
    reach_launcher_surface_hidden(static_cast<reach_launcher *>(capsule));
    if (out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
    }
}

static int32_t reach_feature_control_switcher_open(void *capsule, int32_t open,
                                                   reach_feature_tick_result *out)
{
    int32_t changed = reach_switcher_set_open(static_cast<reach_switcher *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->request_update = 1;
    }
    return changed;
}

static void reach_feature_notify_context_menu(void *capsule,
                                              const reach_feature_notification *notification,
                                              reach_feature_tick_result *out)
{
    if (notification != nullptr &&
        notification->kind == REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED &&
        notification->windows.changed)
    {
        reach_context_menu_notify_windows_changed(static_cast<reach_context_menu *>(capsule), out);
    }
    else if (notification != nullptr &&
             notification->kind == REACH_FEATURE_NOTIFICATION_ICONS_RETAIN)
    {
        reach_context_menu_touch_icons(static_cast<reach_context_menu *>(capsule),
                                       notification->icon_size_px);
    }
}

static void reach_feature_notify_switcher(void *capsule,
                                          const reach_feature_notification *notification,
                                          reach_feature_tick_result *out)
{
    if (notification == nullptr || notification->kind != REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED)
    {
        return;
    }
    if (notification->windows.changed)
    {
        reach_switcher_notify_windows_changed(static_cast<reach_switcher *>(capsule), out);
    }
    if (notification->windows.icon_identity_changed && out != nullptr)
    {
        out->redraw = 1;
    }
}

static int32_t reach_feature_control_clipboard_open(void *capsule, int32_t open,
                                                    reach_feature_tick_result *out)
{
    int32_t changed =
        reach_clipboard_set_open(static_cast<reach_clipboard_feature *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    return changed;
}

static int32_t reach_feature_control_screenshot_open(void *capsule, int32_t open,
                                                     reach_feature_tick_result *out)
{
    int32_t changed = reach_screenshot_set_open(static_cast<reach_screenshot *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    return changed;
}

static void reach_feature_notify_screenshot(void *capsule,
                                            const reach_feature_notification *notification,
                                            reach_feature_tick_result *out)
{
    (void)out;
    if (notification != nullptr && notification->kind == REACH_FEATURE_NOTIFICATION_DISPLAY_CHANGED)
    {
        reach_screenshot_set_display(static_cast<reach_screenshot *>(capsule),
                                     &notification->display);
    }
}

static int32_t reach_feature_control_tray_open(void *capsule, int32_t open,
                                               reach_feature_tick_result *out)
{
    int32_t changed =
        reach_top_bar_set_tray_popup_open(static_cast<reach_top_bar *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    return changed;
}

static int32_t reach_feature_control_quick_settings_open(void *capsule, int32_t open,
                                                         reach_feature_tick_result *out)
{
    int32_t changed =
        reach_quick_settings_set_open(static_cast<reach_quick_settings *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    return changed;
}

static int32_t reach_feature_control_battery_open(void *capsule, int32_t open,
                                                  reach_feature_tick_result *out)
{
    int32_t changed = reach_battery_set_open(static_cast<reach_battery *>(capsule), open);
    if (changed && out != nullptr)
    {
        out->redraw = 1;
        out->relayout = 1;
        out->request_update = 1;
    }
    return changed;
}

static void reach_feature_notify_quick_settings(void *capsule,
                                                const reach_feature_notification *notification,
                                                reach_feature_tick_result *out)
{
    reach_quick_settings *quick_settings = static_cast<reach_quick_settings *>(capsule);
    if (quick_settings == nullptr || notification == nullptr || out == nullptr)
    {
        return;
    }
    if (notification->kind == REACH_FEATURE_NOTIFICATION_CONFIG_CHANGED &&
        notification->config != nullptr)
    {
        if (reach_quick_settings_apply_config(quick_settings, notification->config->light_theme))
        {
            out->redraw = 1;
        }
        return;
    }
    if (!reach_quick_settings_is_open(quick_settings))
    {
        return;
    }
    if (notification->kind == REACH_FEATURE_NOTIFICATION_MAIN_VOLUME)
    {
        reach_quick_settings_apply_main_volume(quick_settings, notification->volume.level,
                                               notification->volume.muted);
        out->redraw = 1;
        out->request_update = 1;
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_BRIGHTNESS)
    {
        const reach_quick_settings_model *model =
            &reach_quick_settings_state_ptr(quick_settings)->model;
        reach_quick_settings_system_apply_result applied = {};
        reach_quick_settings_apply_system_states(quick_settings, &model->network, &model->bluetooth,
                                                 &notification->brightness, 0, 0, &applied);
        out->redraw = 1;
        out->relayout = applied.relayout;
        out->request_update = 1;
    }
}

static void reach_feature_notify_system_hud(void *capsule,
                                            const reach_feature_notification *notification,
                                            reach_feature_tick_result *out)
{
    reach_system_hud *hud = static_cast<reach_system_hud *>(capsule);
    if (hud == nullptr || notification == nullptr || out == nullptr)
    {
        return;
    }
    if (notification->kind == REACH_FEATURE_NOTIFICATION_MEDIA_ACTION)
    {
        reach_system_hud_show_media(hud, notification->media_action);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_MAIN_VOLUME)
    {
        reach_system_hud_show_volume(hud, &notification->volume);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_BRIGHTNESS)
    {
        reach_system_hud_show_brightness(hud, &notification->brightness);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_NOW_PLAYING_CHANGED)
    {
        reach_system_hud_refresh_media(hud);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_APP_LAUNCH_FAILED &&
             notification->app_launch_failure == REACH_APP_LAUNCH_FAILURE_NOT_FOUND)
    {
        reach_system_hud_show_app_removed(hud, notification->app_name);
    }
    else
    {
        return;
    }
    out->redraw = 1;
    out->relayout = 1;
    out->request_update = 1;
}

static size_t reach_feature_quick_settings_take_retired(void *capsule,
                                                        reach_feature_render_resource *out,
                                                        size_t cap)
{
    if (out == nullptr)
    {
        return 0;
    }
    uint64_t ids[REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES] = {};
    size_t count = reach_quick_settings_take_retired_render_icons(
        static_cast<reach_quick_settings *>(capsule), ids,
        cap < REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES
            ? cap
            : REACH_AUDIO_VOLUME_MAX_SESSIONS + REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES);
    for (size_t index = 0; index < count; ++index)
    {
        out[index].render_icon_id = ids[index];
    }
    return count;
}

static size_t reach_feature_quick_settings_active_count(const void *capsule)
{
    const reach_quick_settings_state *state = reach_quick_settings_state_ptr(
        const_cast<reach_quick_settings *>(static_cast<const reach_quick_settings *>(capsule)));
    return state != nullptr ? state->model.sessions.count + state->model.output_devices.count : 0;
}

static int32_t reach_feature_quick_settings_active_at(const void *capsule, size_t index,
                                                      reach_feature_render_resource *out)
{
    const reach_quick_settings_state *state = reach_quick_settings_state_ptr(
        const_cast<reach_quick_settings *>(static_cast<const reach_quick_settings *>(capsule)));
    if (state == nullptr || out == nullptr)
    {
        return 0;
    }
    if (index < state->model.sessions.count)
    {
        out->render_icon_id = state->model.sessions.sessions[index].icon_id;
        return 1;
    }
    index -= state->model.sessions.count;
    if (index < state->model.output_devices.count)
    {
        out->render_icon_id = state->model.output_devices.devices[index].icon_id;
        return 1;
    }
    return 0;
}

static size_t reach_feature_tray_take_retired(void *capsule, reach_feature_render_resource *out,
                                              size_t cap)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    size_t count = 0;
    while (count < cap && reach_top_bar_take_retired_tray_icon(top_bar, &out[count].render_icon_id))
    {
        out[count].source_id = out[count].render_icon_id;
        out[count].release_source = 1;
        ++count;
    }
    return count;
}

static size_t reach_feature_tray_active_count(const void *capsule)
{
    return reach_top_bar_tray_item_count(static_cast<const reach_top_bar *>(capsule));
}

static int32_t reach_feature_tray_active_at(const void *capsule, size_t index,
                                            reach_feature_render_resource *out)
{
    if (out == nullptr ||
        index >= reach_top_bar_tray_item_count(static_cast<const reach_top_bar *>(capsule)))
    {
        return 0;
    }
    out->render_icon_id =
        reach_top_bar_tray_item_icon_id(static_cast<const reach_top_bar *>(capsule), index);
    return 1;
}

static void reach_feature_tray_release_source(void *capsule,
                                              const reach_feature_render_resource *resource)
{
    if (resource != nullptr && resource->release_source)
    {
        reach_top_bar_release_retired_tray_icon(static_cast<reach_top_bar *>(capsule),
                                                resource->source_id);
    }
}

static size_t reach_feature_clipboard_take_retired(void *capsule,
                                                   reach_feature_render_resource *out, size_t cap)
{
    if (out == nullptr)
    {
        return 0;
    }
    reach_clipboard_retired_resource resources[REACH_CLIPBOARD_MAX_ITEMS * 2] = {};
    size_t count = reach_clipboard_feature_take_retired_resources(
        static_cast<reach_clipboard_feature *>(capsule), resources,
        cap < REACH_CLIPBOARD_MAX_ITEMS * 2 ? cap : REACH_CLIPBOARD_MAX_ITEMS * 2);
    for (size_t index = 0; index < count; ++index)
    {
        out[index] = {resources[index].thumbnail_id, resources[index].item_id, 1};
    }
    return count;
}

static size_t reach_feature_clipboard_active_count(const void *capsule)
{
    return reach_clipboard_item_count(const_cast<reach_clipboard_feature *>(
        static_cast<const reach_clipboard_feature *>(capsule)));
}

static int32_t reach_feature_clipboard_active_at(const void *capsule, size_t index,
                                                 reach_feature_render_resource *out)
{
    const reach_clipboard_item *item =
        reach_clipboard_item_at(const_cast<reach_clipboard_feature *>(
                                    static_cast<const reach_clipboard_feature *>(capsule)),
                                index);
    if (item == nullptr || out == nullptr)
    {
        return 0;
    }
    *out = {item->thumbnail_id, item->id, 1};
    return 1;
}

static void reach_feature_clipboard_release_source(void *capsule,
                                                   const reach_feature_render_resource *resource)
{
    if (resource != nullptr && resource->release_source)
    {
        reach_clipboard_retired_resource retired = {resource->source_id, resource->render_icon_id};
        reach_clipboard_feature_release_resource(static_cast<reach_clipboard_feature *>(capsule),
                                                 &retired);
    }
}

static void reach_feature_clipboard_clear_active(void *capsule)
{
    reach_clipboard_reset_items(static_cast<reach_clipboard_feature *>(capsule));
}

static size_t reach_feature_screenshot_take_retired(void *capsule,
                                                    reach_feature_render_resource *out, size_t cap)
{
    if (out == nullptr)
    {
        return 0;
    }
    uint64_t ids[REACH_DISPLAY_MAX_MONITORS * 2] = {};
    size_t count = reach_screenshot_take_retired_images(
        static_cast<reach_screenshot *>(capsule), ids,
        cap < REACH_DISPLAY_MAX_MONITORS * 2 ? cap : REACH_DISPLAY_MAX_MONITORS * 2);
    for (size_t index = 0; index < count; ++index)
    {
        out[index] = {ids[index], ids[index], 1};
    }
    return count;
}

static size_t reach_feature_screenshot_active_count(const void *capsule)
{
    return reach_screenshot_image_count(static_cast<const reach_screenshot *>(capsule));
}

static int32_t reach_feature_screenshot_active_at(const void *capsule, size_t index,
                                                  reach_feature_render_resource *out)
{
    uint64_t image_id =
        reach_screenshot_image_at(static_cast<const reach_screenshot *>(capsule), index);
    if (out == nullptr || image_id == 0)
    {
        return 0;
    }
    *out = {image_id, image_id, 1};
    return 1;
}

static void reach_feature_screenshot_release_source(void *capsule,
                                                    const reach_feature_render_resource *resource)
{
    if (resource != nullptr && resource->release_source)
    {
        reach_screenshot_release_image(static_cast<reach_screenshot *>(capsule),
                                       resource->source_id);
    }
}

static void reach_feature_screenshot_clear_active(void *capsule)
{
    reach_screenshot_forget_images(static_cast<reach_screenshot *>(capsule));
}

static const reach_feature_render_resource_ops reach_screenshot_resource_ops = {
    reach_feature_screenshot_take_retired, reach_feature_screenshot_active_count,
    reach_feature_screenshot_active_at, reach_feature_screenshot_release_source,
    reach_feature_screenshot_clear_active};

static const reach_feature_render_resource_ops reach_quick_settings_resource_ops = {
    reach_feature_quick_settings_take_retired, reach_feature_quick_settings_active_count,
    reach_feature_quick_settings_active_at, nullptr, nullptr};

static const reach_feature_render_resource_ops reach_tray_resource_ops = {
    reach_feature_tray_take_retired, reach_feature_tray_active_count, reach_feature_tray_active_at,
    reach_feature_tray_release_source, nullptr};

static const reach_feature_render_resource_ops reach_clipboard_resource_ops = {
    reach_feature_clipboard_take_retired, reach_feature_clipboard_active_count,
    reach_feature_clipboard_active_at, reach_feature_clipboard_release_source,
    reach_feature_clipboard_clear_active};

static const reach_feature_control_ops reach_stage_control_ops = {
    reach_feature_control_stage_open, nullptr, reach_feature_notify_stage, nullptr, nullptr};
static const reach_feature_control_ops reach_context_menu_control_ops = {
    reach_feature_control_context_menu_open, nullptr, reach_feature_notify_context_menu, nullptr,
    nullptr};
static const reach_feature_control_ops reach_dock_control_ops = {
    nullptr, nullptr, reach_feature_notify_dock, nullptr, nullptr};
static const reach_feature_control_ops reach_top_bar_control_ops = {
    nullptr, nullptr, reach_feature_notify_top_bar, nullptr, nullptr};
static const reach_feature_control_ops reach_launcher_control_ops = {
    reach_feature_control_launcher_open, reach_feature_control_launcher_hidden, nullptr, nullptr,
    nullptr};
static const reach_feature_control_ops reach_switcher_control_ops = {
    reach_feature_control_switcher_open, nullptr, reach_feature_notify_switcher, nullptr, nullptr};
static const reach_feature_control_ops reach_clipboard_control_ops = {
    reach_feature_control_clipboard_open, nullptr, nullptr, nullptr, &reach_clipboard_resource_ops};
static const reach_feature_control_ops reach_tray_control_ops = {
    reach_feature_control_tray_open, nullptr, nullptr, nullptr, &reach_tray_resource_ops};
static const reach_feature_control_ops reach_quick_settings_control_ops = {
    reach_feature_control_quick_settings_open, nullptr, reach_feature_notify_quick_settings,
    nullptr, &reach_quick_settings_resource_ops};
static const reach_feature_control_ops reach_battery_control_ops = {
    reach_feature_control_battery_open, nullptr, nullptr, nullptr, nullptr};
static const reach_feature_control_ops reach_screenshot_control_ops = {
    reach_feature_control_screenshot_open, nullptr, reach_feature_notify_screenshot, nullptr,
    &reach_screenshot_resource_ops};
static const reach_feature_control_ops reach_system_hud_control_ops = {
    nullptr, nullptr, reach_feature_notify_system_hud, nullptr, nullptr};

#define REACH_HOST_LAYER_DOCK_EDGE_REVEAL 120
#define REACH_HOST_LAYER_BAR_ACTIVE 130
#define REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL 140
#define REACH_HOST_LAYER_STAGE_EDGE_REVEAL 150

static int32_t reach_prearranged_surface_arrange(void *capsule,
                                                 const reach_feature_surface_context *ctx)
{
    (void)capsule;
    (void)ctx;
    return 0;
}

static reach_result reach_dock_surface_render(void *capsule,
                                              const reach_feature_surface_context *ctx,
                                              reach_render_command_buffer *out_commands)
{
    reach_dock_surface_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->visible_bounds;
    render.icon_size_px = ctx->icon_size_px;
    render.dpi_scale = ctx->dpi_scale;
    return reach_dock_append_surface_render_commands(static_cast<reach_dock *>(capsule), &render,
                                                     out_commands);
}

static int32_t reach_dock_surface_arrange(void *capsule, const reach_feature_surface_context *ctx)
{
    reach_dock_arrange_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor_bounds = ctx->monitor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    return reach_dock_arrange(static_cast<reach_dock *>(capsule), &arrange);
}

static const reach_feature_surface_ops reach_dock_surface_ops = {
    reach_dock_surface_arrange,
    reach_dock_surface_render,
};

static int32_t reach_dock_resolve_anchor(const void *capsule, uint32_t slot, size_t index,
                                         uint64_t identity, reach_feature_anchor *out)
{
    if (slot != REACH_DOCK_CONTROL_ITEM || out == nullptr)
    {
        return 0;
    }
    const reach_dock *dock = static_cast<const reach_dock *>(capsule);
    if (identity != 0)
    {
        const reach_dock_state *state = reach_dock_state_ptr(const_cast<reach_dock *>(dock));
        if (state == nullptr || identity > UINT32_MAX)
            return 0;
        index = reach_dock_feature_model_find_item_key(&state->model, (uint32_t)identity);
    }
    reach_rect_f32 button = {};
    float bar_edge_y = 0.0f;
    if (!reach_dock_item_anchor(dock, index, &button, &bar_edge_y))
    {
        return 0;
    }
    out->button = button;
    out->bar_edge_y = bar_edge_y;
    out->direction = REACH_POPUP_DROP_UP;
    return 1;
}

static int32_t reach_top_bar_surface_arrange(void *capsule,
                                             const reach_feature_surface_context *ctx)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    reach_rect_f32 before = reach_top_bar_state_ptr(top_bar)->layout.bounds;
    reach_top_bar_build_context build = {};
    build.theme = ctx->theme;
    build.monitor_bounds = ctx->monitor_bounds;
    build.dpi_scale = ctx->dpi_scale;
    build.text_measure = ctx->text_measure;
    reach_top_bar_build_layout(top_bar, &build);
    return !reach_rect_equal(before, reach_top_bar_state_ptr(top_bar)->layout.bounds);
}

static reach_result reach_top_bar_surface_render(void *capsule,
                                                 const reach_feature_surface_context *ctx,
                                                 reach_render_command_buffer *out_commands)
{
    reach_top_bar_render_context render = {};
    render.retained_supported = ctx->retained_supported;
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    render.icon_size_px = ctx->icon_size_px;
    return reach_top_bar_append_render_commands(static_cast<reach_top_bar *>(capsule), &render,
                                                out_commands);
}

static const reach_feature_surface_ops reach_top_bar_surface_ops = {
    reach_top_bar_surface_arrange,
    reach_top_bar_surface_render,
};

static int32_t reach_launcher_surface_arrange(void *capsule,
                                              const reach_feature_surface_context *ctx)
{
    reach_launcher_arrange_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor_bounds = ctx->monitor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    return reach_launcher_arrange(static_cast<reach_launcher *>(capsule), &arrange);
}

static reach_result reach_launcher_surface_render(void *capsule,
                                                  const reach_feature_surface_context *ctx,
                                                  reach_render_command_buffer *out_commands)
{
    return reach_launcher_append_surface_render_commands(static_cast<reach_launcher *>(capsule),
                                                         ctx->theme, ctx->dpi_scale,
                                                         ctx->retained_supported, out_commands);
}

static const reach_feature_surface_ops reach_launcher_surface_ops = {
    reach_launcher_surface_arrange,
    reach_launcher_surface_render,
    nullptr,
};

static int32_t reach_context_menu_surface_layout_anchor(const void *capsule,
                                                        reach_feature_layout_anchor *out)
{
    const reach_context_menu_state *state =
        reach_context_menu_state_ptr(static_cast<const reach_context_menu *>(capsule));
    if (state == nullptr || out == nullptr || !state->open || !state->anchored)
    {
        return 0;
    }
    out->surface = state->power_open ? REACH_SURFACE_ID_TOP_BAR : REACH_SURFACE_ID_DOCK;
    out->slot = state->power_open ? REACH_TOP_BAR_CONTROL_POWER : REACH_DOCK_CONTROL_ITEM;
    out->index = state->power_open ? 0 : state->target_index;
    out->identity =
        state->power_open
            ? 0
            : reach_context_menu_owner_identity(static_cast<const reach_context_menu *>(capsule));
    return 1;
}

static int32_t reach_context_menu_surface_arrange(void *capsule,
                                                  const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_context_menu *menu = static_cast<reach_context_menu *>(capsule);
    reach_context_menu_open_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor = ctx->monitor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    arrange.anchor_button = ctx->anchor_button;
    arrange.bar_edge_y = ctx->anchor_bar_edge_y;
    arrange.drop_direction = ctx->anchor_direction;
    arrange.anchored = 1;
    arrange.text_measure = ctx->text_measure;
    return reach_context_menu_reanchor(menu, &arrange);
}

static reach_result reach_context_menu_surface_render(void *capsule,
                                                      const reach_feature_surface_context *ctx,
                                                      reach_render_command_buffer *out_commands)
{
    reach_context_menu_render_context render = {};
    render.theme = ctx->theme;
    render.icon_size_px = ctx->icon_size_px;
    render.dpi_scale = ctx->dpi_scale;
    return reach_context_menu_append_render_commands(static_cast<reach_context_menu *>(capsule),
                                                     &render, out_commands);
}

static size_t reach_context_menu_native_overlay_generation(const void *capsule)
{
    return reach_context_menu_preview_generation(static_cast<const reach_context_menu *>(capsule));
}

static size_t reach_context_menu_native_overlay_count(const void *capsule)
{
    return reach_context_menu_thumbnail_count(static_cast<const reach_context_menu *>(capsule));
}

static reach_result reach_context_menu_native_overlay_item(const void *capsule, size_t index,
                                                           const reach_theme *theme,
                                                           reach_feature_native_overlay_item *out)
{
    (void)theme;
    if (out == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out = {};
    out->plane = REACH_WINDOW_THUMBNAIL_PLANE_TARGET;
    out->placement.opacity = 1.0f;
    reach_result result =
        reach_context_menu_thumbnail_motion_at(static_cast<const reach_context_menu *>(capsule),
                                               index, &out->source, &out->destination_motion);
    out->has_destination_motion = 1;
    out->placement.visible = out->source != 0;
    return result;
}

static int32_t reach_context_menu_pointer_observation(const void *capsule, reach_rect_union *out)
{
    return reach_context_menu_window_list_hold_shape(
        static_cast<const reach_context_menu *>(capsule), out);
}

static const reach_feature_native_overlay_ops reach_context_menu_native_overlay_ops = {
    reach_context_menu_native_overlay_generation,
    reach_context_menu_native_overlay_count,
    reach_context_menu_native_overlay_item,
};

static const reach_feature_surface_ops reach_context_menu_surface_ops = {
    reach_context_menu_surface_arrange,       reach_context_menu_surface_render,
    reach_context_menu_surface_layout_anchor, &reach_context_menu_native_overlay_ops,
    reach_context_menu_pointer_observation,
};

static reach_result reach_stage_surface_render(void *capsule,
                                               const reach_feature_surface_context *ctx,
                                               reach_render_command_buffer *out_commands)
{
    reach_stage_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->visible_bounds;
    render.dpi_scale = ctx->dpi_scale;
    render.preferred_front_window = ctx->preferred_front_source;
    return reach_stage_append_render_commands(static_cast<reach_stage *>(capsule), &render,
                                              out_commands);
}

static size_t reach_stage_native_overlay_generation(const void *capsule)
{
    return reach_stage_tile_generation(static_cast<const reach_stage *>(capsule));
}

static size_t reach_stage_native_overlay_count(const void *capsule)
{
    return reach_stage_thumbnail_count(static_cast<const reach_stage *>(capsule));
}

static reach_result reach_stage_native_overlay_item(const void *capsule, size_t index,
                                                    const reach_theme *theme,
                                                    reach_feature_native_overlay_item *out)
{
    if (theme == nullptr || out == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    const reach_stage *stage = static_cast<const reach_stage *>(capsule);
    reach_stage_thumbnail_placement placement = {};
    reach_result result = reach_stage_thumbnail_at(stage, index, &placement);
    if (result != REACH_OK)
    {
        return result;
    }
    out->source = placement.window;
    out->plane = placement.behind_surface ? REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET
                                          : REACH_WINDOW_THUMBNAIL_PLANE_TARGET;
    out->placement.destination = placement.destination;
    out->placement.source_screen = placement.source_screen;
    out->placement.opacity = placement.opacity;
    out->placement.visible = placement.visible;
    out->placement.source_screen_valid = placement.source_screen_valid;
    out->placement.background = theme->stage_backdrop;
    out->placement.background.a = placement.backdrop_opacity;
    out->placement.background_visible = placement.desktop && reach_stage_is_open(stage);
    return REACH_OK;
}

static int32_t reach_stage_surface_arrange(void *capsule, const reach_feature_surface_context *ctx)
{
    return reach_stage_set_desktop_bounds(static_cast<reach_stage *>(capsule),
                                          ctx->available_bounds);
}

static const reach_feature_native_overlay_ops reach_stage_native_overlay_ops = {
    reach_stage_native_overlay_generation,
    reach_stage_native_overlay_count,
    reach_stage_native_overlay_item,
};

static const reach_feature_surface_ops reach_stage_surface_ops = {
    reach_stage_surface_arrange,
    reach_stage_surface_render,
    nullptr,
    &reach_stage_native_overlay_ops,
};

static int32_t reach_system_hud_surface_arrange(void *capsule,
                                                const reach_feature_surface_context *ctx)
{
    reach_system_hud_arrange_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor_bounds = ctx->monitor_bounds;
    arrange.dock_shown_bounds = ctx->anchor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    return reach_system_hud_arrange(static_cast<reach_system_hud *>(capsule), &arrange);
}

static reach_result reach_system_hud_surface_render(void *capsule,
                                                    const reach_feature_surface_context *ctx,
                                                    reach_render_command_buffer *out_commands)
{
    reach_system_hud_render_context render = {};
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    return reach_system_hud_append_render_commands(static_cast<const reach_system_hud *>(capsule),
                                                   &render, out_commands);
}

static const reach_feature_surface_ops reach_system_hud_surface_ops = {
    reach_system_hud_surface_arrange,
    reach_system_hud_surface_render,
};

static int32_t reach_switcher_surface_arrange(void *capsule,
                                              const reach_feature_surface_context *ctx)
{
    reach_switcher_arrange_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor_bounds = ctx->monitor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    arrange.transition_visible = ctx->transition_visible;
    arrange.bounds_valid = ctx->bounds_valid;
    return reach_switcher_arrange(static_cast<reach_switcher *>(capsule), &arrange);
}

static reach_result reach_switcher_surface_render(void *capsule,
                                                  const reach_feature_surface_context *ctx,
                                                  reach_render_command_buffer *out_commands)
{
    reach_switcher_render_context render = {};
    render.theme = ctx->theme;
    render.icon_size_px = ctx->icon_size_px;
    render.text_measure = ctx->text_measure;
    return reach_switcher_append_render_commands(static_cast<reach_switcher *>(capsule), &render,
                                                 out_commands);
}

static size_t reach_switcher_native_overlay_generation(const void *capsule)
{
    return reach_switcher_preview_generation(static_cast<const reach_switcher *>(capsule));
}

static size_t reach_switcher_native_overlay_count(const void *capsule)
{
    return reach_switcher_thumbnail_count(static_cast<const reach_switcher *>(capsule));
}

static reach_result reach_switcher_native_overlay_item(const void *capsule, size_t index,
                                                       const reach_theme *theme,
                                                       reach_feature_native_overlay_item *out)
{
    (void)theme;
    if (out == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out = {};
    out->plane = REACH_WINDOW_THUMBNAIL_PLANE_TARGET;
    out->placement.opacity = 1;
    reach_result result = reach_switcher_thumbnail_motion_at(
        static_cast<const reach_switcher *>(capsule), index, &out->source, &out->destination_motion,
        &out->placement.visible);
    out->has_destination_motion = 1;
    if (result != REACH_OK)
        return result;
    return REACH_OK;
}

static const reach_feature_native_overlay_ops reach_switcher_native_overlay_ops = {
    reach_switcher_native_overlay_generation,
    reach_switcher_native_overlay_count,
    reach_switcher_native_overlay_item,
};

static const reach_feature_surface_ops reach_switcher_surface_ops = {
    reach_switcher_surface_arrange,
    reach_switcher_surface_render,
    nullptr,
    &reach_switcher_native_overlay_ops,
};

static int32_t reach_clipboard_surface_arrange(void *capsule,
                                               const reach_feature_surface_context *ctx)
{
    return reach_clipboard_feature_relayout(static_cast<reach_clipboard_feature *>(capsule),
                                            ctx->theme, ctx->monitor_bounds, ctx->anchor_bounds,
                                            ctx->dpi_scale);
}

static reach_result reach_clipboard_surface_render(void *capsule,
                                                   const reach_feature_surface_context *ctx,
                                                   reach_render_command_buffer *out_commands)
{
    return reach_clipboard_append_render_commands(static_cast<reach_clipboard_feature *>(capsule),
                                                  ctx->theme, ctx->dpi_scale, out_commands);
}

static int32_t reach_screenshot_surface_arrange(void *capsule,
                                                const reach_feature_surface_context *ctx)
{
    return reach_screenshot_arrange(static_cast<reach_screenshot *>(capsule), ctx->monitor_bounds);
}

static reach_result reach_screenshot_surface_render(void *capsule,
                                                    const reach_feature_surface_context *ctx,
                                                    reach_render_command_buffer *out_commands)
{
    return reach_screenshot_append_render_commands(static_cast<const reach_screenshot *>(capsule),
                                                   ctx->theme, ctx->dpi_scale, out_commands);
}

static const reach_feature_surface_ops reach_screenshot_surface_ops = {
    reach_screenshot_surface_arrange,
    reach_screenshot_surface_render,
};

static const reach_feature_surface_ops reach_clipboard_surface_ops = {
    reach_clipboard_surface_arrange,
    reach_clipboard_surface_render,
};

static int32_t reach_top_bar_resolve_anchor(const void *capsule, uint32_t slot, size_t index,
                                            uint64_t identity, reach_feature_anchor *out)
{
    (void)index;
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    const reach_top_bar_state *state = reach_top_bar_state_ptr(top_bar);
    if (state == nullptr || out == nullptr || identity != 0)
    {
        return 0;
    }

    reach_rect_f32 button = {};
    if (slot == REACH_TOP_BAR_CONTROL_TRAY)
    {
        button = state->layout.tray_overflow_button;
    }
    else if (slot == REACH_TOP_BAR_CONTROL_QUICK_SETTINGS)
    {
        button = state->layout.quick_settings_button;
    }
    else if (slot == REACH_TOP_BAR_CONTROL_BATTERY)
    {
        button = state->layout.battery_button;
    }
    else if (slot == REACH_TOP_BAR_CONTROL_POWER)
    {
        button = state->layout.power_button;
    }
    else
    {
        return 0;
    }

    out->button = reach_top_bar_rect_to_screen(&state->layout, button);
    out->bar_edge_y = state->layout.bounds.y + state->layout.bounds.height;
    out->bar_height = state->layout.bounds.height;
    out->direction = REACH_POPUP_DROP_DOWN;
    return 1;
}

static int32_t reach_quick_settings_surface_arrange(void *capsule,
                                                    const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_quick_settings *quick_settings = static_cast<reach_quick_settings *>(capsule);
    reach_rect_f32 before = reach_quick_settings_state_ptr(quick_settings)->bounds;
    reach_quick_settings_layout_context layout = {};
    layout.theme = ctx->theme;
    layout.dpi_scale = ctx->dpi_scale;
    layout.anchor_button = ctx->anchor_button;
    layout.monitor = ctx->monitor_bounds;
    layout.bar_edge_y = ctx->anchor_bar_edge_y;
    layout.drop_direction = ctx->anchor_direction;
    reach_quick_settings_refresh_layout(quick_settings, &layout);
    return !reach_rect_equal(before, reach_quick_settings_state_ptr(quick_settings)->bounds);
}

static reach_result reach_quick_settings_surface_render(void *capsule,
                                                        const reach_feature_surface_context *ctx,
                                                        reach_render_command_buffer *out_commands)
{
    return reach_quick_settings_append_render_commands(static_cast<reach_quick_settings *>(capsule),
                                                       ctx->theme, ctx->dpi_scale,
                                                       &ctx->text_measure, out_commands);
}

static const reach_feature_surface_ops reach_quick_settings_surface_ops = {
    reach_quick_settings_surface_arrange,
    reach_quick_settings_surface_render,
};

static int32_t reach_battery_surface_arrange(void *capsule,
                                             const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_battery *battery = static_cast<reach_battery *>(capsule);
    reach_rect_f32 before = reach_battery_state_ptr(battery)->bounds;
    reach_battery_open_context layout = {};
    layout.theme = ctx->theme;
    layout.monitor = ctx->monitor_bounds;
    layout.anchor_button = ctx->anchor_button;
    layout.bar_edge_y = ctx->anchor_bar_edge_y;
    layout.dpi_scale = ctx->dpi_scale;
    layout.drop_direction = ctx->anchor_direction;
    reach_battery_relayout(battery, &layout);
    return !reach_rect_equal(before, reach_battery_state_ptr(battery)->bounds);
}

static reach_result reach_battery_surface_render(void *capsule,
                                                 const reach_feature_surface_context *ctx,
                                                 reach_render_command_buffer *out_commands)
{
    reach_battery_render_context render = {};
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    return reach_battery_append_render_commands(static_cast<const reach_battery *>(capsule),
                                                &render, out_commands);
}

static const reach_feature_surface_ops reach_battery_surface_ops = {
    reach_battery_surface_arrange,
    reach_battery_surface_render,
};

static int32_t reach_tray_surface_arrange(void *capsule, const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    reach_feature_surface_geometry before = {};
    reach_top_bar_tray_capsule_ops()->surface_geometry(top_bar, &before);
    reach_popup_anchor anchor = {};
    anchor.button = ctx->anchor_button;
    anchor.monitor = ctx->monitor_bounds;
    anchor.bar_edge_y = ctx->anchor_bar_edge_y;
    anchor.bar_height = ctx->anchor_bar_height;
    anchor.direction = ctx->anchor_direction;
    reach_rect_f32 bounds = {};
    reach_top_bar_layout_tray_popup(top_bar, ctx->theme, &anchor, ctx->dpi_scale, &bounds);
    return !reach_rect_equal(before.visible_bounds, bounds);
}

static reach_result reach_tray_surface_render(void *capsule,
                                              const reach_feature_surface_context *ctx,
                                              reach_render_command_buffer *out_commands)
{
    reach_top_bar_tray_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->visible_bounds;
    render.dpi_scale = ctx->dpi_scale;
    return reach_top_bar_append_tray_render_commands(static_cast<reach_top_bar *>(capsule), &render,
                                                     out_commands);
}

static const reach_feature_surface_ops reach_tray_surface_ops = {
    reach_tray_surface_arrange,
    reach_tray_surface_render,
};

template <typename Feature, reach_result (*Create)(Feature **)>
static reach_result reach_feature_create(void **out_capsule)
{
    if (out_capsule == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    Feature *capsule = nullptr;
    reach_result result = Create(&capsule);
    *out_capsule = capsule;
    return result;
}

template <typename Feature, void (*Destroy)(Feature *)>
static void reach_feature_destroy(void *capsule)
{
    Destroy(static_cast<Feature *>(capsule));
}

static reach_surface_id reach_find_registered_surface(const reach_feature_definition *definitions,
                                                      size_t count, const reach_surface_key *key)
{
    if (key != nullptr)
    {
        for (size_t index = 0; index < count; ++index)
        {
            if (definitions[index].key == key)
            {
                return static_cast<reach_surface_id>(index);
            }
        }
    }
    return REACH_SURFACE_ID_INVALID;
}

reach_surface_id reach_host_find_surface(const reach_host *host, const reach_surface_key &key)
{
    return host == nullptr ? REACH_SURFACE_ID_INVALID
                           : reach_find_registered_surface(host->feature_definitions,
                                                           host->surface_count, &key);
}

static bool reach_compile_surface_order(const reach_feature_runtime *runtimes, size_t count,
                                        bool layout, reach_surface_id *order)
{
    bool placed[REACH_HOST_SURFACE_CAPACITY] = {};
    for (size_t position = 0; position < count; ++position)
    {
        reach_surface_id next = REACH_SURFACE_ID_INVALID;
        for (reach_surface_id id = 0; id < count; ++id)
        {
            const auto &runtime = runtimes[id];
            const reach_surface_id dependency =
                layout ? runtime.links->layout_anchor : runtime.links->capsule_owner;
            if (placed[id] || (dependency != REACH_SURFACE_ID_INVALID && !placed[dependency]))
            {
                continue;
            }
            if (next == REACH_SURFACE_ID_INVALID ||
                (layout &&
                 runtime.definition->layout.priority < runtimes[next].definition->layout.priority))
            {
                next = id;
            }
        }
        if (next == REACH_SURFACE_ID_INVALID)
        {
            return false;
        }
        order[position] = next;
        placed[next] = true;
    }
    return true;
}

reach_result reach_host_set_feature_registry(reach_host *host,
                                             const reach_feature_definition *definitions,
                                             size_t count)
{
    if (host == nullptr || definitions == nullptr || count > REACH_HOST_SURFACE_CAPACITY)
    {
        return REACH_INVALID_ARGUMENT;
    }
    host->surface_count = 0;

    // Reserve legacy IDs before assigning slots to entries without an explicit numeric ID.
    const reach_feature_definition *slots[REACH_HOST_SURFACE_CAPACITY] = {};
    for (size_t index = 0; index < count; ++index)
    {
        const auto &entry = definitions[index];
        if (entry.id != REACH_SURFACE_ID_INVALID)
        {
            if (entry.id >= count || slots[entry.id] != nullptr)
            {
                return REACH_INVALID_ARGUMENT;
            }
            slots[entry.id] = &entry;
        }
    }
    size_t available = 0;
    for (size_t index = 0; index < count; ++index)
    {
        const auto &entry = definitions[index];
        if (entry.id == REACH_SURFACE_ID_INVALID)
        {
            while (slots[available] != nullptr)
            {
                ++available;
            }
            slots[available++] = &entry;
        }
    }
    for (reach_surface_id id = 0; id < count; ++id)
    {
        const auto &entry = *slots[id];
        const bool owns_capsule = entry.capsule_owner == nullptr;
        const auto &lifecycle = entry.lifecycle;
        if (reach_find_registered_surface(host->feature_definitions, id, entry.key) !=
                REACH_SURFACE_ID_INVALID ||
            entry.capsule_ops == nullptr || entry.capsule_ops->surface_geometry == nullptr ||
            entry.surface_ops == nullptr || entry.surface_ops->arrange == nullptr ||
            entry.surface_ops->append_render_commands == nullptr ||
            (owns_capsule &&
             (entry.factory.create == nullptr || entry.factory.destroy == nullptr)) ||
            (!owns_capsule &&
             (entry.factory.create != nullptr || entry.factory.destroy != nullptr)) ||
            ((lifecycle.attach == nullptr) != (lifecycle.detach == nullptr)) ||
            (lifecycle.start != nullptr && lifecycle.stop == nullptr) ||
            (entry.toggle_event_count != 0 && entry.toggle_events == nullptr) ||
            (entry.routed_event_count != 0 && entry.routed_events == nullptr) ||
            (entry.window_event_count != 0 && entry.window_events == nullptr))
        {
            return REACH_INVALID_ARGUMENT;
        }
        host->feature_definitions[id] = entry;
        host->feature_definitions[id].id = id;
    }
    for (reach_surface_id id = 0; id < count; ++id)
    {
        const auto &entry = host->feature_definitions[id];
        auto &links = host->feature_links[id];
        auto &runtime = host->feature_runtimes[id];
        runtime = {};
        runtime.surface = &host->surfaces[id];
        reach_surface_runtime_init(runtime.surface);
        runtime.definition = &entry;
        runtime.links = &links;
        const auto resolve = [&](const reach_surface_key *key, reach_surface_id *target)
        {
            *target = reach_find_registered_surface(host->feature_definitions, count, key);
            return key == nullptr || (*target < count && *target != id);
        };
        if (!resolve(entry.capsule_owner, &links.capsule_owner) ||
            !resolve(entry.layout.anchor, &links.layout_anchor) ||
            !resolve(entry.surface.dismiss_guard_surface, &links.dismiss_guard_surface) ||
            !resolve(entry.surface.opening_origin, &links.opening_origin) ||
            (links.capsule_owner < count &&
             host->feature_definitions[links.capsule_owner].capsule_owner != nullptr))
        {
            return REACH_INVALID_ARGUMENT;
        }
    }
    if (!reach_compile_surface_order(host->feature_runtimes, count, false,
                                     host->feature_lifecycle_order) ||
        !reach_compile_surface_order(host->feature_runtimes, count, true,
                                     host->feature_layout_order))
    {
        return REACH_INVALID_ARGUMENT;
    }
    host->surface_count = count;
    return REACH_OK;
}

reach_result reach_host_init_feature_registry(reach_host *host)
{
    static reach_surface_key dock_key;
    static const auto &top_bar_key = reach_surface_key_for<reach_top_bar>();
    static reach_surface_key launcher_key;
    static constexpr reach_ui_event_type escape_events[] = {REACH_UI_EVENT_ESCAPE};
    static const reach_feature_definition definitions[] = {
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_DOCK;
            definition.key = &dock_key;
            definition.factory = {reach_feature_create<reach_dock, reach_dock_create>,
                                  reach_feature_destroy<reach_dock, reach_dock_destroy>};
            definition.lifecycle.attach = reach_feature_attach_dock;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_dock>;
            definition.capsule_ops = reach_dock_capsule_ops();
            definition.surface_ops = &reach_dock_surface_ops;
            definition.control_ops = &reach_dock_control_ops;
            definition.resolve_anchor = reach_dock_resolve_anchor;
            definition.surface.shadow = REACH_SURFACE_SHADOW_BAR;
            definition.surface.layer = 110;
            definition.surface.edge_reveal = {1, REACH_HOST_LAYER_DOCK_EDGE_REVEAL, {}, 0};
            definition.surface.bar_reveal = {reach_dock_reveal_ops(), 0, 0.0f};
            definition.surface.pointer_priority = 90;
            definition.surface.pointer_flags =
                REACH_SURFACE_POINTER_SOURCE_GATED | REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE;
            definition.surface.window_flags = REACH_WINDOW_COMPOSITION_BACKED |
                                              REACH_WINDOW_SESSION_EVENTS |
                                              REACH_WINDOW_DESKTOP_MINIMIZE_SUPPORT;
            definition.surface.redraw_on = REACH_SURFACE_REDRAW_OPEN_WINDOWS |
                                           REACH_SURFACE_REDRAW_FOREGROUND |
                                           REACH_SURFACE_REDRAW_ACTIVATION_REQUESTED |
                                           REACH_SURFACE_REDRAW_ACTIVATION_COMPLETED;
            definition.layout.priority = 30;
            definition.layout.reservation_edge = REACH_LAYOUT_RESERVATION_BOTTOM;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_TOP_BAR;
            definition.key = &top_bar_key;
            definition.factory = {reach_feature_create<reach_top_bar, reach_top_bar_create>,
                                  reach_feature_destroy<reach_top_bar, reach_top_bar_destroy>};
            definition.lifecycle.attach = reach_feature_attach_top_bar;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_top_bar>;
            definition.capsule_ops = reach_top_bar_capsule_ops();
            definition.surface_ops = &reach_top_bar_surface_ops;
            definition.control_ops = &reach_top_bar_control_ops;
            definition.resolve_anchor = reach_top_bar_resolve_anchor;
            definition.surface.shadow = REACH_SURFACE_SHADOW_BAR;
            definition.surface.edge_reveal = {1, REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL, {}, 0};
            definition.surface.bar_reveal = {reach_top_bar_reveal_ops(),
                                             REACH_HOST_LAYER_BAR_ACTIVE, 4.0f, 18.0f};
            definition.surface.pointer_priority = 80;
            definition.surface.pointer_flags = REACH_SURFACE_POINTER_SOURCE_GATED;
            definition.surface.redraw_on =
                REACH_SURFACE_REDRAW_OPEN_WINDOWS | REACH_SURFACE_REDRAW_FOREGROUND |
                REACH_SURFACE_REDRAW_CLOCK | REACH_SURFACE_REDRAW_INPUT_LANGUAGE;
            definition.layout.priority = 35;
            definition.layout.reservation_edge = REACH_LAYOUT_RESERVATION_TOP;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_LAUNCHER;
            definition.key = &launcher_key;
            definition.factory = {reach_feature_create<reach_launcher, reach_launcher_create>,
                                  reach_feature_destroy<reach_launcher, reach_launcher_destroy>};
            definition.lifecycle.attach = reach_feature_attach_launcher;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_launcher>;
            definition.lifecycle.stop = reach_feature_stop_launcher;
            definition.capsule_ops = reach_launcher_capsule_ops();
            definition.surface_ops = &reach_launcher_surface_ops;
            definition.control_ops = &reach_launcher_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 100;
            definition.surface.pointer_priority = 30;
            definition.surface.dismiss_guard_surface = &dock_key;
            definition.surface.dismiss_guard_any_control = 1;
            definition.surface.restores_focus_on_close = 1;
            definition.surface.close_on_persistent_press = 1;
            definition.surface.behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
            definition.surface.cls = REACH_SURFACE_CLASS_TRANSIENT;
            definition.surface.pointer_flags = REACH_SURFACE_POINTER_RELAYOUT_REDRAWS |
                                               REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED;
            definition.surface.window_flags = REACH_WINDOW_COMPOSITION_BACKED |
                                              REACH_WINDOW_ACTIVATE | REACH_WINDOW_TEXT_INPUT |
                                              REACH_WINDOW_FOCUS_LOST_EVENT;
            definition.layout.priority = 10;
            definition.toggle_events =
                reach_launcher_activation_events(&definition.toggle_event_count);
            definition.routed_events = escape_events;
            definition.routed_event_count = std::size(escape_events);
            definition.window_events = reach_launcher_window_events(&definition.window_event_count);
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_CLIPBOARD;
            definition.factory = {
                reach_feature_create<reach_clipboard_feature, reach_clipboard_feature_create>,
                reach_feature_destroy<reach_clipboard_feature, reach_clipboard_feature_destroy>};
            definition.lifecycle.attach = reach_feature_attach_clipboard;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_clipboard>;
            definition.lifecycle.start = reach_feature_start_clipboard;
            definition.lifecycle.stop = reach_feature_stop_clipboard;
            definition.capsule_ops = reach_clipboard_feature_capsule_ops();
            definition.surface_ops = &reach_clipboard_surface_ops;
            definition.control_ops = &reach_clipboard_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 190;
            definition.surface.pointer_priority = 20;
            definition.surface.cls = REACH_SURFACE_CLASS_TRANSIENT;
            definition.surface.pointer_flags =
                REACH_SURFACE_POINTER_SOURCE_GATED | REACH_SURFACE_POINTER_CANCEL_BEFORE_DOWN;
            definition.surface.behavior_flags = REACH_SURFACE_BEHAVIOR_DISMISS_ON_ESCAPE;
            definition.layout.priority = 20;
            definition.layout.anchor = &launcher_key;
            definition.toggle_events =
                reach_clipboard_activation_events(&definition.toggle_event_count);
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_TRAY;
            definition.capsule_owner = &top_bar_key;
            definition.lifecycle.stop = reach_feature_stop_tray;
            definition.capsule_ops = reach_top_bar_tray_capsule_ops();
            definition.surface_ops = &reach_tray_surface_ops;
            definition.control_ops = &reach_tray_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 180;
            definition.surface.pointer_priority = 40;
            definition.surface.opening_origin = &top_bar_key;
            definition.surface.cls = REACH_SURFACE_CLASS_POPUP;
            definition.surface.pointer_flags = REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED;
            definition.layout.anchor = &top_bar_key;
            definition.layout.anchor_slot = REACH_TOP_BAR_CONTROL_TRAY;
            definition.layout.priority = 40;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_QUICK_SETTINGS;
            definition.factory = {
                reach_feature_create<reach_quick_settings, reach_quick_settings_create>,
                reach_feature_destroy<reach_quick_settings, reach_quick_settings_destroy>};
            definition.lifecycle.attach = reach_feature_attach_quick_settings;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_quick_settings>;
            definition.lifecycle.start = reach_feature_start_quick_settings;
            definition.lifecycle.stop = reach_feature_stop_quick_settings;
            definition.capsule_ops = reach_quick_settings_capsule_ops();
            definition.surface_ops = &reach_quick_settings_surface_ops;
            definition.control_ops = &reach_quick_settings_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 170;
            definition.surface.pointer_priority = 50;
            definition.surface.opening_origin = &top_bar_key;
            definition.surface.cls = REACH_SURFACE_CLASS_POPUP;
            definition.surface.pointer_flags = REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE |
                                               REACH_SURFACE_POINTER_CAPTURE_OWNS_MOVE;
            definition.surface.window_flags =
                REACH_WINDOW_COMPOSITION_BACKED | REACH_WINDOW_TEXT_INPUT;
            definition.layout.anchor = &top_bar_key;
            definition.layout.anchor_slot = REACH_TOP_BAR_CONTROL_QUICK_SETTINGS;
            definition.layout.priority = 50;
            definition.routed_events = escape_events;
            definition.routed_event_count = std::size(escape_events);
            definition.window_events =
                reach_quick_settings_window_events(&definition.window_event_count);
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_CONTEXT_MENU;
            definition.factory = {
                reach_feature_create<reach_context_menu, reach_context_menu_create>,
                reach_feature_destroy<reach_context_menu, reach_context_menu_destroy>};
            definition.lifecycle.attach = reach_feature_attach_context_menu;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_context_menu>;
            definition.lifecycle.start = reach_feature_start_context_menu;
            definition.lifecycle.stop = reach_feature_stop_context_menu;
            definition.capsule_ops = reach_context_menu_capsule_ops();
            definition.surface_ops = &reach_context_menu_surface_ops;
            definition.control_ops = &reach_context_menu_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 160;
            definition.surface.pointer_priority = 10;
            definition.surface.cls = REACH_SURFACE_CLASS_POPUP;
            definition.surface.pointer_flags = REACH_SURFACE_POINTER_EXCLUSIVE_WHILE_OPEN;
            definition.layout.priority = 70;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_SWITCHER;
            definition.factory = {reach_feature_create<reach_switcher, reach_switcher_create>,
                                  reach_feature_destroy<reach_switcher, reach_switcher_destroy>};
            definition.lifecycle.attach = reach_feature_attach_switcher;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_switcher>;
            definition.lifecycle.stop = reach_feature_stop_switcher;
            definition.capsule_ops = reach_switcher_capsule_ops();
            definition.surface_ops = &reach_switcher_surface_ops;
            definition.control_ops = &reach_switcher_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 200;
            definition.surface.pointer_priority = 100;
            definition.surface.behavior_flags =
                REACH_SURFACE_BEHAVIOR_EXCLUSIVE | REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE;
            definition.surface.cls = REACH_SURFACE_CLASS_OVERLAY;
            definition.surface.redraw_on =
                REACH_SURFACE_REDRAW_FOREGROUND | REACH_SURFACE_REDRAW_ACTIVATION_REQUESTED;
            definition.layout.priority = 60;
            definition.routed_events = reach_switcher_routed_events(&definition.routed_event_count);
            definition.refresh_world_before_event = REACH_UI_EVENT_APP_SWITCH_BEGIN;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_STAGE;
            definition.factory = {reach_feature_create<reach_stage, reach_stage_create>,
                                  reach_feature_destroy<reach_stage, reach_stage_destroy>};
            definition.lifecycle.attach = reach_feature_attach_stage;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_stage>;
            definition.lifecycle.stop = reach_feature_stop_stage;
            definition.capsule_ops = reach_stage_capsule_ops();
            definition.surface_ops = &reach_stage_surface_ops;
            definition.control_ops = &reach_stage_control_ops;
            definition.surface.refresh_world_on_open = 1;
            definition.surface.dismiss_guard_surface = &dock_key;
            definition.surface.dismiss_guard_slot = REACH_DOCK_CONTROL_TRIGGER;
            definition.surface.layer = 50;
            definition.surface.pointer_priority = 60;
            definition.surface.bar_shown_while_open = 1;
            definition.surface.behavior_flags =
                REACH_SURFACE_BEHAVIOR_EXCLUSIVE | REACH_SURFACE_BEHAVIOR_DISMISS_ON_ESCAPE;
            definition.surface.edge_reveal = {1,
                                              REACH_HOST_LAYER_STAGE_EDGE_REVEAL,
                                              {REACH_EDGE_REVEAL_ANCHOR_TOP_LEFT, 4.0f, 4.0f, 1},
                                              1};
            definition.surface.cls = REACH_SURFACE_CLASS_TRANSIENT;
            definition.layout.priority = 65;
            definition.layout.uses_reserved_bounds = 1;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_BATTERY;
            definition.factory = {reach_feature_create<reach_battery, reach_battery_create>,
                                  reach_feature_destroy<reach_battery, reach_battery_destroy>};
            definition.lifecycle.attach = reach_feature_attach_battery;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_battery>;
            definition.lifecycle.stop = reach_feature_stop_battery;
            definition.capsule_ops = reach_battery_capsule_ops();
            definition.surface_ops = &reach_battery_surface_ops;
            definition.control_ops = &reach_battery_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 175;
            definition.surface.pointer_priority = 55;
            definition.surface.opening_origin = &top_bar_key;
            definition.surface.popup_chrome = 1;
            definition.surface.cls = REACH_SURFACE_CLASS_POPUP;
            definition.layout.anchor = &top_bar_key;
            definition.layout.anchor_slot = REACH_TOP_BAR_CONTROL_BATTERY;
            definition.layout.priority = 55;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_SYSTEM_HUD;
            definition.factory = {
                reach_feature_create<reach_system_hud, reach_system_hud_create>,
                reach_feature_destroy<reach_system_hud, reach_system_hud_destroy>};
            definition.lifecycle.attach = reach_feature_attach_system_hud;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_system_hud>;
            definition.lifecycle.stop = reach_feature_stop_system_hud;
            definition.capsule_ops = reach_system_hud_capsule_ops();
            definition.surface_ops = &reach_system_hud_surface_ops;
            definition.control_ops = &reach_system_hud_control_ops;
            definition.surface.shadow = REACH_SURFACE_SHADOW_POPUP;
            definition.surface.layer = 220;
            definition.surface.behavior_flags =
                REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE | REACH_SURFACE_BEHAVIOR_INPUT_PASSTHROUGH;
            definition.surface.pointer_flags = REACH_SURFACE_POINTER_SOURCE_GATED;
            definition.layout.priority = 80;
            definition.layout.anchor = &dock_key;
            return definition;
        }(),
        []
        {
            reach_feature_definition definition = {};
            definition.id = REACH_SURFACE_ID_SCREENSHOT;
            definition.factory = {
                reach_feature_create<reach_screenshot, reach_screenshot_create>,
                reach_feature_destroy<reach_screenshot, reach_screenshot_destroy>};
            definition.lifecycle.attach = reach_feature_attach_screenshot;
            definition.lifecycle.detach = reach_feature_detach<reach_feature_attach_screenshot>;
            definition.capsule_ops = reach_screenshot_capsule_ops();
            definition.surface_ops = &reach_screenshot_surface_ops;
            definition.control_ops = &reach_screenshot_control_ops;
            definition.surface.layer = 210;
            definition.surface.pointer_priority = 5;
            definition.surface.refresh_world_on_open = 1;
            definition.surface.restores_focus_on_close = 1;
            definition.surface.behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
            definition.surface.cls = REACH_SURFACE_CLASS_OVERLAY;
            definition.surface.pointer_flags = REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE;
            definition.surface.window_flags = REACH_WINDOW_COMPOSITION_BACKED |
                                              REACH_WINDOW_ACTIVATE | REACH_WINDOW_CROSSHAIR_CURSOR;
            definition.layout.priority = 85;
            definition.toggle_events =
                reach_screenshot_activation_events(&definition.toggle_event_count);
            definition.routed_events =
                reach_screenshot_routed_events(&definition.routed_event_count);
            return definition;
        }(),
    };
    static_assert(std::size(definitions) <= REACH_HOST_SURFACE_CAPACITY);
    return reach_host_set_feature_registry(host, definitions, std::size(definitions));
}
