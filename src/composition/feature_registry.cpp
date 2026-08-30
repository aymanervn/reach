#include "host_internal.h"

#include "reach/features/battery.h"
#include "reach/features/clipboard.h"
#include "reach/features/context_menu.h"
#include "reach/features/dock.h"
#include "reach/features/launcher.h"
#include "reach/features/quick_settings.h"
#include "reach/features/stage.h"
#include "reach/features/switcher.h"
#include "reach/features/system_hud.h"
#include "reach/features/top_bar.h"

static void reach_feature_attach_launcher(void *capsule,
                                          const reach_feature_dependencies *dependencies)
{
    reach_launcher *launcher = static_cast<reach_launcher *>(capsule);
    reach_launcher_attach_search(launcher,
                                 dependencies != nullptr ? dependencies->search : nullptr);
    reach_launcher_attach_icons(launcher, dependencies != nullptr ? dependencies->icons : nullptr);
    reach_launcher_set_terminal_icon_ref(
        launcher, dependencies != nullptr ? dependencies->terminal_icon_ref : nullptr);
}

static void reach_feature_attach_dock(void *capsule, const reach_feature_dependencies *dependencies)
{
    reach_dock_attach_services(static_cast<reach_dock *>(capsule),
                               dependencies != nullptr ? dependencies->icons : nullptr,
                               dependencies != nullptr ? dependencies->windows : nullptr);
}

static void reach_feature_attach_top_bar(void *capsule,
                                         const reach_feature_dependencies *dependencies)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    reach_top_bar_attach_services(top_bar,
                                  dependencies != nullptr ? dependencies->now_playing : nullptr,
                                  dependencies != nullptr ? dependencies->icons : nullptr,
                                  dependencies != nullptr ? dependencies->windows : nullptr,
                                  dependencies != nullptr ? dependencies->system_stats : nullptr,
                                  dependencies != nullptr ? dependencies->clock : nullptr,
                                  dependencies != nullptr ? dependencies->input_language : nullptr,
                                  dependencies != nullptr ? dependencies->tray : nullptr);
    reach_top_bar_attach_app_control(top_bar,
                                     dependencies != nullptr ? dependencies->app_control : nullptr);
    reach_top_bar_attach_status(top_bar,
                                dependencies != nullptr ? dependencies->system_status : nullptr);
}

static void reach_feature_attach_stage(void *capsule,
                                       const reach_feature_dependencies *dependencies)
{
    reach_stage_attach_services(static_cast<reach_stage *>(capsule),
                                dependencies != nullptr ? dependencies->windows : nullptr,
                                dependencies != nullptr ? dependencies->icons : nullptr,
                                dependencies != nullptr ? dependencies->app_control : nullptr);
}

static void reach_feature_attach_switcher(void *capsule,
                                          const reach_feature_dependencies *dependencies)
{
    reach_switcher_attach_services(static_cast<reach_switcher *>(capsule),
                                   dependencies != nullptr ? dependencies->icons : nullptr,
                                   dependencies != nullptr ? dependencies->windows : nullptr);
}

static void reach_feature_attach_quick_settings(void *capsule,
                                                const reach_feature_dependencies *dependencies)
{
    reach_quick_settings_attach_status(static_cast<reach_quick_settings *>(capsule),
                                       dependencies != nullptr ? dependencies->system_status
                                                               : nullptr);
}

static void reach_feature_attach_battery(void *capsule,
                                         const reach_feature_dependencies *dependencies)
{
    reach_battery_attach_services(static_cast<reach_battery *>(capsule),
                                  dependencies != nullptr ? dependencies->system_stats : nullptr,
                                  dependencies != nullptr ? dependencies->system_status : nullptr);
}

static void reach_feature_attach_system_hud(void *capsule,
                                            const reach_feature_dependencies *dependencies)
{
    reach_system_hud_attach_now_playing(static_cast<reach_system_hud *>(capsule),
                                        dependencies != nullptr ? dependencies->now_playing
                                                                : nullptr);
}

static void reach_feature_attach_clipboard(void *capsule,
                                           const reach_feature_dependencies *dependencies)
{
    reach_clipboard_feature_attach_port(
        static_cast<reach_clipboard_feature *>(capsule),
        dependencies != nullptr ? dependencies->clipboard : nullptr,
        dependencies != nullptr ? dependencies->request_update : nullptr,
        dependencies != nullptr ? dependencies->request_update_user : nullptr);
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

static void reach_host_surface_clipboard_close(reach_host *host)
{
    reach_host_set_registered_surface_open(host, REACH_SURFACE_ID_CLIPBOARD, 0);
}

static void reach_host_surface_tray_close(reach_host *host)
{
    reach_host_set_registered_surface_open(host, REACH_SURFACE_ID_TRAY, 0);
}

static void reach_host_surface_quick_settings_close(reach_host *host)
{
    reach_host_set_registered_surface_open(host, REACH_SURFACE_ID_QUICK_SETTINGS, 0);
}

static void reach_host_surface_battery_close(reach_host *host)
{
    reach_host_set_registered_surface_open(host, REACH_SURFACE_ID_BATTERY, 0);
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
        reach_stage_set_animation_seconds(stage,
                                          (float)notification->config->stage_animation_ms / 1000.0f);
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

static void reach_feature_notify_dock(void *capsule,
                                     const reach_feature_notification *notification,
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
        reach_dock_apply_config(dock, notification->config->dock_height);
        out->relayout = 1;
        out->redraw = 1;
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_POPUPS_CLOSED &&
             reach_dock_clear_context_feedback(dock))
    {
        out->redraw = 1;
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

static void reach_feature_notify_switcher(void *capsule,
                                          const reach_feature_notification *notification,
                                          reach_feature_tick_result *out)
{
    if (notification == nullptr ||
        notification->kind != REACH_FEATURE_NOTIFICATION_WINDOWS_CHANGED)
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
    if (quick_settings == nullptr || notification == nullptr || out == nullptr ||
        !reach_quick_settings_is_open(quick_settings))
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

static void reach_feature_notify_battery(void *capsule,
                                         const reach_feature_notification *notification,
                                         reach_feature_tick_result *out)
{
    if (notification != nullptr && out != nullptr &&
        notification->kind == REACH_FEATURE_NOTIFICATION_SYSTEM_STATS_CHANGED &&
        reach_battery_refresh_power(static_cast<reach_battery *>(capsule)))
    {
        out->redraw = 1;
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
    if (notification->kind == REACH_FEATURE_NOTIFICATION_MEDIA_ACTION && notification->present)
    {
        reach_system_hud_show_media(hud, notification->media_action);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_MAIN_VOLUME && notification->present)
    {
        reach_system_hud_show_volume(hud, &notification->volume);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_BRIGHTNESS && notification->present)
    {
        reach_system_hud_show_brightness(hud, &notification->brightness);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_TOP_BAR_VISIBLE &&
             notification->present)
    {
        reach_system_hud_hide(hud);
    }
    else if (notification->kind == REACH_FEATURE_NOTIFICATION_NOW_PLAYING_CHANGED)
    {
        reach_system_hud_refresh_media(hud);
    }
    else
    {
        return;
    }
    out->redraw = 1;
    out->request_update = 1;
}

static int32_t reach_feature_quick_settings_blocks_position_frame(const void *capsule)
{
    return reach_quick_settings_height_animation_active(
        static_cast<const reach_quick_settings *>(capsule));
}

static int32_t reach_feature_tray_blocks_position_frame(const void *capsule)
{
    reach_animation_manager *animations = reach_top_bar_tray_animation_manager(
        const_cast<reach_top_bar *>(static_cast<const reach_top_bar *>(capsule)));
    return animations != nullptr && reach_animation_manager_any_active(animations);
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
    reach_feature_control_context_menu_open, nullptr, nullptr, nullptr, nullptr};
static const reach_feature_control_ops reach_dock_control_ops = {
    nullptr, nullptr, reach_feature_notify_dock, nullptr, nullptr};
static const reach_feature_control_ops reach_launcher_control_ops = {
    reach_feature_control_launcher_open, reach_feature_control_launcher_hidden, nullptr, nullptr,
    nullptr};
static const reach_feature_control_ops reach_switcher_control_ops = {
    reach_feature_control_switcher_open, nullptr, reach_feature_notify_switcher, nullptr, nullptr};
static const reach_feature_control_ops reach_clipboard_control_ops = {
    reach_feature_control_clipboard_open, nullptr, nullptr, nullptr,
    &reach_clipboard_resource_ops};
static const reach_feature_control_ops reach_tray_control_ops = {
    reach_feature_control_tray_open, nullptr, nullptr, reach_feature_tray_blocks_position_frame,
    &reach_tray_resource_ops};
static const reach_feature_control_ops reach_quick_settings_control_ops = {
    reach_feature_control_quick_settings_open, nullptr, reach_feature_notify_quick_settings,
    reach_feature_quick_settings_blocks_position_frame, &reach_quick_settings_resource_ops};
static const reach_feature_control_ops reach_battery_control_ops = {
    reach_feature_control_battery_open, nullptr, reach_feature_notify_battery, nullptr, nullptr};
static const reach_feature_control_ops reach_system_hud_control_ops = {
    nullptr, nullptr, reach_feature_notify_system_hud, nullptr, nullptr};

#define REACH_HOST_LAYER_DOCK_EDGE_REVEAL 120
#define REACH_HOST_LAYER_BAR_ACTIVE 130
#define REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL 140
#define REACH_HOST_LAYER_STAGE_EDGE_REVEAL 150

static void reach_host_define_feature(reach_host *host, reach_surface_id id,
                                      reach_surface_class cls, int32_t has_transition,
                                      void (*force_close)(reach_host *),
                                      const reach_feature_capsule_ops *capsule_ops,
                                      uint32_t pointer_flags)
{
    reach_surface_runtime *surface = &host->surfaces[id];
    reach_host_surface_transition *transition = has_transition ? &host->transitions[id] : nullptr;
    reach_feature_definition *definition = &host->feature_definitions[id];
    *definition = {};
    definition->id = id;
    definition->capsule_ops = capsule_ops;
    definition->surface.cls = cls;
    definition->surface.pointer_flags = pointer_flags;
    definition->surface.has_transition = has_transition != 0;
    definition->surface.opening_origin = REACH_HOST_SURFACE_COUNT;
    definition->layout.anchor = REACH_HOST_SURFACE_COUNT;
    definition->surface.dismiss_guard_surface = REACH_HOST_SURFACE_COUNT;
    definition->force_close = force_close;

    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    *runtime = {};
    runtime->surface = surface;
    runtime->transition = transition;
    runtime->definition = definition;
}

static void reach_host_init_feature_definitions(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_define_feature(host, REACH_SURFACE_ID_DOCK, REACH_SURFACE_CLASS_PERSISTENT,
                              0, nullptr, reach_dock_capsule_ops(),
                              REACH_SURFACE_POINTER_SOURCE_GATED |
                                  REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE);
    reach_host_define_feature(host, REACH_SURFACE_ID_TOP_BAR, REACH_SURFACE_CLASS_PERSISTENT,
                              0, nullptr, reach_top_bar_capsule_ops(),
                              REACH_SURFACE_POINTER_SOURCE_GATED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_LAUNCHER, REACH_SURFACE_CLASS_TRANSIENT, 1, nullptr, reach_launcher_capsule_ops(),
        REACH_SURFACE_POINTER_RELAYOUT_REDRAWS | REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_CLIPBOARD, REACH_SURFACE_CLASS_TRANSIENT, 1, reach_host_surface_clipboard_close,
        reach_clipboard_feature_capsule_ops(), REACH_SURFACE_POINTER_SOURCE_GATED);
    reach_host_define_feature(host, REACH_SURFACE_ID_TRAY, REACH_SURFACE_CLASS_POPUP, 1, reach_host_surface_tray_close,
                              reach_top_bar_tray_capsule_ops(),
                              REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_QUICK_SETTINGS, REACH_SURFACE_CLASS_POPUP, 1, reach_host_surface_quick_settings_close,
        reach_quick_settings_capsule_ops(),
        REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE | REACH_SURFACE_POINTER_CAPTURE_OWNS_MOVE);
    reach_host_define_feature(host, REACH_SURFACE_ID_BATTERY, REACH_SURFACE_CLASS_POPUP,
                              1,
                              reach_host_surface_battery_close, reach_battery_capsule_ops(),
                              REACH_SURFACE_POINTER_NONE);
    reach_host_define_feature(host, REACH_SURFACE_ID_SYSTEM_HUD, REACH_SURFACE_CLASS_PERSISTENT,
                              0, nullptr, reach_system_hud_capsule_ops(),
                              REACH_SURFACE_POINTER_SOURCE_GATED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_CONTEXT_MENU, REACH_SURFACE_CLASS_POPUP, 1, nullptr, reach_context_menu_capsule_ops(),
        REACH_SURFACE_POINTER_EXCLUSIVE_WHILE_OPEN);
    reach_host_define_feature(host, REACH_SURFACE_ID_SWITCHER, REACH_SURFACE_CLASS_OVERLAY,
                              1, nullptr,
                              reach_switcher_capsule_ops(), REACH_SURFACE_POINTER_NONE);
    reach_host_define_feature(host, REACH_SURFACE_ID_STAGE, REACH_SURFACE_CLASS_TRANSIENT,
                              1, nullptr,
                              reach_stage_capsule_ops(), REACH_SURFACE_POINTER_NONE);

    reach_feature_definition *definitions = host->feature_definitions;

    definitions[REACH_SURFACE_ID_DOCK].lifecycle.attach = reach_feature_attach_dock;
    definitions[REACH_SURFACE_ID_TOP_BAR].lifecycle.attach = reach_feature_attach_top_bar;
    definitions[REACH_SURFACE_ID_LAUNCHER].lifecycle.attach = reach_feature_attach_launcher;
    definitions[REACH_SURFACE_ID_LAUNCHER].lifecycle.stop = reach_feature_stop_launcher;
    definitions[REACH_SURFACE_ID_CLIPBOARD].lifecycle.attach = reach_feature_attach_clipboard;
    definitions[REACH_SURFACE_ID_CLIPBOARD].lifecycle.start = reach_feature_start_clipboard;
    definitions[REACH_SURFACE_ID_CLIPBOARD].lifecycle.stop = reach_feature_stop_clipboard;
    definitions[REACH_SURFACE_ID_TRAY].lifecycle.stop = reach_feature_stop_tray;
    definitions[REACH_SURFACE_ID_TRAY].lifecycle.close_transition_on_stop = 1;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].lifecycle.attach =
        reach_feature_attach_quick_settings;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].lifecycle.start =
        reach_feature_start_quick_settings;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].lifecycle.stop = reach_feature_stop_quick_settings;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].lifecycle.close_transition_on_stop = 1;
    definitions[REACH_SURFACE_ID_BATTERY].lifecycle.attach = reach_feature_attach_battery;
    definitions[REACH_SURFACE_ID_BATTERY].lifecycle.stop = reach_feature_stop_battery;
    definitions[REACH_SURFACE_ID_BATTERY].lifecycle.close_transition_on_stop = 1;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].lifecycle.attach = reach_feature_attach_system_hud;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].lifecycle.stop = reach_feature_stop_system_hud;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].lifecycle.start = reach_feature_start_context_menu;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].lifecycle.stop = reach_feature_stop_context_menu;
    definitions[REACH_SURFACE_ID_STAGE].lifecycle.attach = reach_feature_attach_stage;
    definitions[REACH_SURFACE_ID_SWITCHER].lifecycle.attach = reach_feature_attach_switcher;
    definitions[REACH_SURFACE_ID_SWITCHER].lifecycle.stop = reach_feature_stop_switcher;
    definitions[REACH_SURFACE_ID_STAGE].lifecycle.stop = reach_feature_stop_stage;

    definitions[REACH_SURFACE_ID_STAGE].control_ops = &reach_stage_control_ops;
    definitions[REACH_SURFACE_ID_STAGE].surface.refresh_world_on_open = 1;
    definitions[REACH_SURFACE_ID_STAGE].surface.dismiss_guard_surface = REACH_SURFACE_ID_DOCK;
    definitions[REACH_SURFACE_ID_STAGE].surface.dismiss_guard_slot = REACH_DOCK_CONTROL_TRIGGER;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].control_ops = &reach_context_menu_control_ops;
    definitions[REACH_SURFACE_ID_DOCK].control_ops = &reach_dock_control_ops;
    definitions[REACH_SURFACE_ID_LAUNCHER].control_ops = &reach_launcher_control_ops;
    definitions[REACH_SURFACE_ID_SWITCHER].control_ops = &reach_switcher_control_ops;
    definitions[REACH_SURFACE_ID_CLIPBOARD].control_ops = &reach_clipboard_control_ops;
    definitions[REACH_SURFACE_ID_TRAY].control_ops = &reach_tray_control_ops;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].control_ops = &reach_quick_settings_control_ops;
    definitions[REACH_SURFACE_ID_BATTERY].control_ops = &reach_battery_control_ops;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].control_ops = &reach_system_hud_control_ops;

    definitions[REACH_SURFACE_ID_DOCK].surface.shadow = REACH_SURFACE_SHADOW_BAR;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.shadow = REACH_SURFACE_SHADOW_BAR;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_TRAY].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_BATTERY].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.shadow = REACH_SURFACE_SHADOW_POPUP;

    definitions[REACH_SURFACE_ID_STAGE].surface.layer = 50;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.layer = 100;
    definitions[REACH_SURFACE_ID_DOCK].surface.layer = 110;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.layer = 0;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.layer = 160;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.layer = 170;
    definitions[REACH_SURFACE_ID_BATTERY].surface.layer = 175;
    definitions[REACH_SURFACE_ID_TRAY].surface.layer = 180;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.layer = 190;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.layer = 200;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.layer = 220;

    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.role = REACH_SURFACE_CONTEXT_MENU;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.pointer_priority = 10;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.role = REACH_SURFACE_CLIPBOARD;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.pointer_priority = 20;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.role = REACH_SURFACE_LAUNCHER;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.pointer_priority = 30;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.dismiss_guard_surface = REACH_SURFACE_ID_DOCK;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.dismiss_guard_any_control = 1;
    
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.restores_focus_on_close = 1;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.close_on_persistent_press = 1;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.behavior_flags =
        REACH_SURFACE_BEHAVIOR_ACTIVATES | REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.scale_in_envelope = 1;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.start_scale = REACH_HOST_TRANSITION_SCALE_IN;
    definitions[REACH_SURFACE_ID_TRAY].surface.settle_from_above = 1;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.settle_from_above = 1;
    definitions[REACH_SURFACE_ID_BATTERY].surface.settle_from_above = 1;
    definitions[REACH_SURFACE_ID_TRAY].surface.role = REACH_SURFACE_TRAY_MENU;
    definitions[REACH_SURFACE_ID_TRAY].surface.pointer_priority = 40;
    definitions[REACH_SURFACE_ID_BATTERY].surface.role = REACH_SURFACE_BATTERY;
    definitions[REACH_SURFACE_ID_BATTERY].surface.pointer_priority = 55;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.role = REACH_SURFACE_QUICK_SETTINGS;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.pointer_priority = 50;
    definitions[REACH_SURFACE_ID_DOCK].surface.edge_reveal = {
        1, REACH_HOST_LAYER_DOCK_EDGE_REVEAL, {}, nullptr};
    definitions[REACH_SURFACE_ID_DOCK].surface.bar_reveal = {reach_dock_reveal_ops(), 0, 0.0f};
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.edge_reveal = {
        1, REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL, {}, nullptr};
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.bar_reveal = {reach_top_bar_reveal_ops(),
                                                                REACH_HOST_LAYER_BAR_ACTIVE, 4.0f};
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.role = REACH_SURFACE_TOP_BAR;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.pointer_priority = 80;
    definitions[REACH_SURFACE_ID_DOCK].surface.role = REACH_SURFACE_DOCK;
    definitions[REACH_SURFACE_ID_DOCK].surface.pointer_priority = 90;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.role = REACH_SURFACE_SWITCHER;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.pointer_priority = 100;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.behavior_flags =
        REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    definitions[REACH_SURFACE_ID_STAGE].surface.role = REACH_SURFACE_STAGE;
    definitions[REACH_SURFACE_ID_STAGE].surface.pointer_priority = 60;
    definitions[REACH_SURFACE_ID_STAGE].surface.bar_shown_while_open = 1;
    definitions[REACH_SURFACE_ID_STAGE].surface.behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.role = REACH_SURFACE_SYSTEM_HUD;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.pointer_priority = 0;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.behavior_flags =
        REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].layout.anchor = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.opening_origin = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].layout.anchor_slot =
        REACH_TOP_BAR_CONTROL_QUICK_SETTINGS;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.popup_chrome = 1;
    definitions[REACH_SURFACE_ID_BATTERY].layout.anchor = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_BATTERY].surface.opening_origin = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_BATTERY].layout.anchor_slot = REACH_TOP_BAR_CONTROL_BATTERY;
    definitions[REACH_SURFACE_ID_BATTERY].surface.popup_chrome = 1;
    definitions[REACH_SURFACE_ID_TRAY].layout.anchor = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_TRAY].surface.opening_origin = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_TRAY].layout.anchor_slot = REACH_TOP_BAR_CONTROL_TRAY;
    definitions[REACH_SURFACE_ID_STAGE].surface.edge_reveal = {
        1,
        REACH_HOST_LAYER_STAGE_EDGE_REVEAL,
        {REACH_EDGE_REVEAL_ANCHOR_TOP_LEFT, 4.0f, 4.0f, 1},
        reach_host_on_surface_edge_reveal};

    definitions[REACH_SURFACE_ID_LAUNCHER].layout.priority = 10;
    definitions[REACH_SURFACE_ID_CLIPBOARD].layout.priority = 20;
    definitions[REACH_SURFACE_ID_DOCK].layout.priority = 30;
    definitions[REACH_SURFACE_ID_TOP_BAR].layout.priority = 35;
    definitions[REACH_SURFACE_ID_TRAY].layout.priority = 40;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].layout.priority = 50;
    definitions[REACH_SURFACE_ID_BATTERY].layout.priority = 55;
    definitions[REACH_SURFACE_ID_SWITCHER].layout.priority = 60;
    definitions[REACH_SURFACE_ID_STAGE].layout.priority = 65;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].layout.priority = 70;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].layout.priority = 80;

    definitions[REACH_SURFACE_ID_LAUNCHER].toggle_events = reach_launcher_activation_events(
        &definitions[REACH_SURFACE_ID_LAUNCHER].toggle_event_count);
    definitions[REACH_SURFACE_ID_LAUNCHER].routed_events = reach_launcher_routed_events(
        &definitions[REACH_SURFACE_ID_LAUNCHER].routed_event_count);
    definitions[REACH_SURFACE_ID_CLIPBOARD].toggle_events = reach_clipboard_activation_events(
        &definitions[REACH_SURFACE_ID_CLIPBOARD].toggle_event_count);
    definitions[REACH_SURFACE_ID_SWITCHER].routed_events =
        reach_switcher_routed_events(&definitions[REACH_SURFACE_ID_SWITCHER].routed_event_count);
}

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
    render.bounds = ctx->render_bounds;
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
                                         reach_feature_anchor *out)
{
    if (slot != REACH_DOCK_CONTROL_ITEM || out == nullptr)
    {
        return 0;
    }
    reach_rect_f32 button = {};
    float bar_edge_y = 0.0f;
    if (!reach_dock_item_anchor(static_cast<const reach_dock *>(capsule), index, &button,
                                &bar_edge_y))
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
                                                         ctx->theme, ctx->dpi_scale, out_commands);
}

static void reach_launcher_surface_set_pointer_transform(void *capsule,
                                                         reach_transform_f32 transform)
{
    reach_launcher_set_pointer_transform(static_cast<reach_launcher *>(capsule), transform);
}

static const reach_feature_surface_ops reach_launcher_surface_ops = {
    reach_launcher_surface_arrange,
    reach_launcher_surface_render,
    nullptr,
    reach_launcher_surface_set_pointer_transform,
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
    reach_rect_f32 before = reach_context_menu_state_ptr(menu)->bounds;
    reach_context_menu_open_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor = ctx->monitor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    arrange.anchor_button = ctx->anchor_button;
    arrange.bar_edge_y = ctx->anchor_bar_edge_y;
    arrange.drop_direction = ctx->anchor_direction;
    arrange.anchored = 1;
    arrange.text_measure = ctx->text_measure;
    reach_context_menu_reanchor(menu, &arrange);
    return !reach_rect_equal(before, reach_context_menu_state_ptr(menu)->bounds);
}

static reach_result reach_context_menu_surface_render(void *capsule,
                                                      const reach_feature_surface_context *ctx,
                                                      reach_render_command_buffer *out_commands)
{
    reach_context_menu_render_context render = {};
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    return reach_context_menu_append_render_commands(static_cast<reach_context_menu *>(capsule),
                                                     &render, out_commands);
}

static const reach_feature_surface_ops reach_context_menu_surface_ops = {
    reach_context_menu_surface_arrange,
    reach_context_menu_surface_render,
    reach_context_menu_surface_layout_anchor,
};

static reach_result reach_stage_surface_render(void *capsule,
                                               const reach_feature_surface_context *ctx,
                                               reach_render_command_buffer *out_commands)
{
    reach_stage_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->visible_bounds;
    render.dpi_scale = ctx->dpi_scale;
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
                                                    reach_feature_native_overlay_item *out)
{
    if (out == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_stage_thumbnail_placement placement = {};
    reach_result result =
        reach_stage_thumbnail_at(static_cast<const reach_stage *>(capsule), index, &placement);
    if (result != REACH_OK)
    {
        return result;
    }
    out->source = placement.window;
    out->placement.destination = placement.destination;
    out->placement.source_screen = placement.source_screen;
    out->placement.opacity = placement.opacity;
    out->placement.visible = placement.visible;
    out->placement.source_screen_valid = placement.source_screen_valid;
    return REACH_OK;
}

static const reach_feature_native_overlay_ops reach_stage_native_overlay_ops = {
    reach_stage_native_overlay_generation,
    reach_stage_native_overlay_count,
    reach_stage_native_overlay_item,
};

static const reach_feature_surface_ops reach_stage_surface_ops = {
    reach_prearranged_surface_arrange, reach_stage_surface_render, nullptr, nullptr,
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
    arrange.last_bounds = ctx->last_bounds;
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
    render.bounds = ctx->render_bounds;
    render.dpi_scale = ctx->dpi_scale;
    render.icon_size_px = ctx->icon_size_px;
    return reach_switcher_append_render_commands(static_cast<reach_switcher *>(capsule), &render,
                                                 out_commands);
}

static const reach_feature_surface_ops reach_switcher_surface_ops = {
    reach_switcher_surface_arrange,
    reach_switcher_surface_render,
};

static int32_t reach_clipboard_surface_arrange(void *capsule,
                                               const reach_feature_surface_context *ctx)
{
    float border = reach_theme_border_thickness(ctx->theme, ctx->dpi_scale);
    return reach_clipboard_feature_relayout(static_cast<reach_clipboard_feature *>(capsule),
                                            ctx->monitor_bounds, ctx->anchor_bounds, ctx->dpi_scale,
                                            border, nullptr);
}

static reach_result reach_clipboard_surface_render(void *capsule,
                                                   const reach_feature_surface_context *ctx,
                                                   reach_render_command_buffer *out_commands)
{
    return reach_clipboard_append_render_commands(static_cast<reach_clipboard_feature *>(capsule),
                                                  ctx->theme, ctx->dpi_scale, out_commands);
}

static const reach_feature_surface_ops reach_clipboard_surface_ops = {
    reach_clipboard_surface_arrange,
    reach_clipboard_surface_render,
};

static int32_t reach_top_bar_resolve_anchor(const void *capsule, uint32_t slot, size_t index,
                                            reach_feature_anchor *out)
{
    (void)index;
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    const reach_top_bar_state *state = reach_top_bar_state_ptr(top_bar);
    if (state == nullptr || out == nullptr)
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
    int32_t animation_changed = reach_quick_settings_update_open_animation(quick_settings, &layout);
    return animation_changed ||
           !reach_rect_equal(before, reach_quick_settings_state_ptr(quick_settings)->bounds);
}

static reach_result reach_quick_settings_surface_render(void *capsule,
                                                        const reach_feature_surface_context *ctx,
                                                        reach_render_command_buffer *out_commands)
{
    return reach_quick_settings_append_render_commands(static_cast<reach_quick_settings *>(capsule),
                                                       ctx->theme, ctx->dpi_scale, out_commands);
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

static void reach_host_bind_shared_feature_capsules(reach_host *host)
{
    reach_feature_runtime *runtimes = host->feature_runtimes;
    runtimes[REACH_SURFACE_ID_TRAY].capsule = runtimes[REACH_SURFACE_ID_TOP_BAR].capsule;
}

void reach_host_init_feature_registry(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_init_feature_definitions(host);
    reach_feature_definition *definitions = host->feature_definitions;
    definitions[REACH_SURFACE_ID_DOCK].factory = {
        reach_feature_create<reach_dock, reach_dock_create>,
        reach_feature_destroy<reach_dock, reach_dock_destroy>};
    definitions[REACH_SURFACE_ID_TOP_BAR].factory = {
        reach_feature_create<reach_top_bar, reach_top_bar_create>,
        reach_feature_destroy<reach_top_bar, reach_top_bar_destroy>};
    definitions[REACH_SURFACE_ID_LAUNCHER].factory = {
        reach_feature_create<reach_launcher, reach_launcher_create>,
        reach_feature_destroy<reach_launcher, reach_launcher_destroy>};
    definitions[REACH_SURFACE_ID_CLIPBOARD].factory = {
        reach_feature_create<reach_clipboard_feature, reach_clipboard_feature_create>,
        reach_feature_destroy<reach_clipboard_feature, reach_clipboard_feature_destroy>};
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].factory = {
        reach_feature_create<reach_quick_settings, reach_quick_settings_create>,
        reach_feature_destroy<reach_quick_settings, reach_quick_settings_destroy>};
    definitions[REACH_SURFACE_ID_BATTERY].factory = {
        reach_feature_create<reach_battery, reach_battery_create>,
        reach_feature_destroy<reach_battery, reach_battery_destroy>};
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].factory = {
        reach_feature_create<reach_system_hud, reach_system_hud_create>,
        reach_feature_destroy<reach_system_hud, reach_system_hud_destroy>};
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].factory = {
        reach_feature_create<reach_context_menu, reach_context_menu_create>,
        reach_feature_destroy<reach_context_menu, reach_context_menu_destroy>};
    definitions[REACH_SURFACE_ID_SWITCHER].factory = {
        reach_feature_create<reach_switcher, reach_switcher_create>,
        reach_feature_destroy<reach_switcher, reach_switcher_destroy>};
    definitions[REACH_SURFACE_ID_STAGE].factory = {
        reach_feature_create<reach_stage, reach_stage_create>,
        reach_feature_destroy<reach_stage, reach_stage_destroy>};
    definitions[REACH_SURFACE_ID_DOCK].surface_ops = &reach_dock_surface_ops;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface_ops = &reach_top_bar_surface_ops;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface_ops = &reach_launcher_surface_ops;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface_ops = &reach_context_menu_surface_ops;
    definitions[REACH_SURFACE_ID_STAGE].surface_ops = &reach_stage_surface_ops;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface_ops = &reach_system_hud_surface_ops;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].layout.anchor = REACH_SURFACE_ID_DOCK;
    definitions[REACH_SURFACE_ID_SWITCHER].surface_ops = &reach_switcher_surface_ops;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface_ops = &reach_clipboard_surface_ops;
    definitions[REACH_SURFACE_ID_CLIPBOARD].layout.anchor = REACH_SURFACE_ID_LAUNCHER;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface_ops = &reach_quick_settings_surface_ops;
    definitions[REACH_SURFACE_ID_BATTERY].surface_ops = &reach_battery_surface_ops;
    definitions[REACH_SURFACE_ID_TRAY].surface_ops = &reach_tray_surface_ops;
    definitions[REACH_SURFACE_ID_DOCK].resolve_anchor = reach_dock_resolve_anchor;
    definitions[REACH_SURFACE_ID_TOP_BAR].resolve_anchor = reach_top_bar_resolve_anchor;
}

void reach_host_bind_registered_surface_ports(reach_host *host,
                                              const reach_host_dependencies *dependencies)
{
    if (host == nullptr || dependencies == nullptr)
    {
        return;
    }

    host->surfaces[REACH_SURFACE_ID_LAUNCHER].window = dependencies->launcher_window;
    host->surfaces[REACH_SURFACE_ID_LAUNCHER].renderer = dependencies->launcher_renderer;
    host->surfaces[REACH_SURFACE_ID_DOCK].window = dependencies->dock_window;
    host->surfaces[REACH_SURFACE_ID_DOCK].renderer = dependencies->dock_renderer;
    host->surfaces[REACH_SURFACE_ID_TOP_BAR].window = dependencies->top_bar_window;
    host->surfaces[REACH_SURFACE_ID_TOP_BAR].renderer = dependencies->top_bar_renderer;
    host->surfaces[REACH_SURFACE_ID_TRAY].window = dependencies->tray_window;
    host->surfaces[REACH_SURFACE_ID_TRAY].renderer = dependencies->tray_renderer;
    host->surfaces[REACH_SURFACE_ID_SWITCHER].window = dependencies->switcher_window;
    host->surfaces[REACH_SURFACE_ID_SWITCHER].renderer = dependencies->switcher_renderer;
    host->surfaces[REACH_SURFACE_ID_STAGE].window = dependencies->stage_window;
    host->surfaces[REACH_SURFACE_ID_STAGE].renderer = dependencies->stage_renderer;
    host->surfaces[REACH_SURFACE_ID_CONTEXT_MENU].window = dependencies->context_menu_window;
    host->surfaces[REACH_SURFACE_ID_CONTEXT_MENU].renderer = dependencies->context_menu_renderer;
    host->surfaces[REACH_SURFACE_ID_QUICK_SETTINGS].window = dependencies->quick_settings_window;
    host->surfaces[REACH_SURFACE_ID_QUICK_SETTINGS].renderer = dependencies->quick_settings_renderer;
    host->surfaces[REACH_SURFACE_ID_BATTERY].window = dependencies->battery_window;
    host->surfaces[REACH_SURFACE_ID_BATTERY].renderer = dependencies->battery_renderer;
    host->surfaces[REACH_SURFACE_ID_SYSTEM_HUD].window = dependencies->system_hud_window;
    host->surfaces[REACH_SURFACE_ID_SYSTEM_HUD].renderer = dependencies->system_hud_renderer;
    host->surfaces[REACH_SURFACE_ID_CLIPBOARD].window = dependencies->clipboard_window;
    host->surfaces[REACH_SURFACE_ID_CLIPBOARD].renderer = dependencies->clipboard_renderer;
}

void reach_host_init_registered_surfaces(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_runtime_init(host->feature_runtimes[index].surface);
    }
}

void reach_host_destroy_registered_surface_ports(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = REACH_HOST_SURFACE_COUNT; index > 0; --index)
    {
        reach_surface_runtime *surface = host->feature_runtimes[index - 1].surface;
        if (surface == nullptr)
        {
            continue;
        }
        if (surface->window.ops.destroy != nullptr)
        {
            surface->window.ops.destroy(surface->window.window);
        }
        if (surface->renderer.ops.destroy != nullptr)
        {
            surface->renderer.ops.destroy(surface->renderer.backend);
        }
        reach_surface_runtime_init(surface);
    }
}

void reach_host_attach_registered_features(reach_host *host,
                                           const reach_feature_dependencies *dependencies)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (runtime->capsule != nullptr && runtime->definition->lifecycle.attach != nullptr)
        {
            runtime->definition->lifecycle.attach(runtime->capsule, dependencies);
        }
    }
}

reach_result reach_host_start_registered_features(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index];
        if (runtime->capsule != nullptr && runtime->definition->lifecycle.start != nullptr)
        {
            reach_result result = runtime->definition->lifecycle.start(runtime->capsule);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }
    return REACH_OK;
}

void reach_host_stop_registered_features(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = REACH_HOST_SURFACE_COUNT; index > 0; --index)
    {
        reach_feature_runtime *runtime = &host->feature_runtimes[index - 1];
        const reach_feature_lifecycle_ops *lifecycle = &runtime->definition->lifecycle;
        if (runtime->capsule == nullptr || lifecycle->stop == nullptr)
        {
            continue;
        }
        lifecycle->stop(runtime->capsule);
        if (lifecycle->close_transition_on_stop && runtime->transition != nullptr)
        {
            reach_host_surface_transition_set(host, runtime->transition, 0);
        }
        if (runtime->surface != nullptr)
        {
            runtime->surface->dirty_flags = 1;
        }
    }
}

reach_result reach_host_create_registered_features(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_OK;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        const reach_feature_definition *definition = desc->definition;
        if (definition == nullptr || definition->factory.create == nullptr)
        {
            continue;
        }
        reach_result create_result = definition->factory.create(&desc->capsule);
        if (create_result != REACH_OK)
        {
            result = create_result;
        }
    }
    reach_host_bind_shared_feature_capsules(host);
    return result;
}

void reach_host_destroy_registered_features(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = REACH_HOST_SURFACE_COUNT; index > 0; --index)
    {
        reach_feature_runtime *desc = &host->feature_runtimes[index - 1];
        const reach_feature_definition *definition = desc->definition;
        if (definition != nullptr && definition->factory.destroy != nullptr &&
            desc->capsule != nullptr)
        {
            definition->factory.destroy(desc->capsule);
        }
        desc->capsule = nullptr;
    }
    reach_host_bind_shared_feature_capsules(host);
}
