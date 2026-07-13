#include "host_internal.h"

static void reach_host_handle_global_mouse_down(reach_host *host, reach_point_i32 point)
{
    if (host == nullptr)
    {
        return;
    }

    int32_t on_tray = reach_tray_state_ptr(host->tray_capsule)->popup_open && (float)point.x >= host->tray.last_bounds.x &&
                      (float)point.x <= host->tray.last_bounds.x + host->tray.last_bounds.width &&
                      (float)point.y >= host->tray.last_bounds.y &&
                      (float)point.y <= host->tray.last_bounds.y + host->tray.last_bounds.height;
    reach_rect_f32 context_bounds = host->context_menu.bounds_valid
                                        ? host->context_menu.last_bounds
                                        : reach_context_menu_state_ptr(host->context_menu_capsule)->bounds;
    int32_t on_context = reach_context_menu_state_ptr(host->context_menu_capsule)->open && (float)point.x >= context_bounds.x &&
                         (float)point.x <= context_bounds.x + context_bounds.width &&
                         (float)point.y >= context_bounds.y &&
                         (float)point.y <= context_bounds.y + context_bounds.height;
    reach_rect_f32 quick_settings_bounds = host->quick_settings.bounds_valid
                                               ? host->quick_settings.last_bounds
                                               : reach_quick_settings_state_ptr(host->quick_settings_capsule)->bounds;
    int32_t on_quick_settings =
        reach_quick_settings_state_ptr(host->quick_settings_capsule)->open && (float)point.x >= quick_settings_bounds.x &&
        (float)point.x <= quick_settings_bounds.x + quick_settings_bounds.width &&
        (float)point.y >= quick_settings_bounds.y &&
        (float)point.y <= quick_settings_bounds.y + quick_settings_bounds.height;
    reach_rect_f32 launcher_bounds =
        host->launcher.bounds_valid ? host->launcher.last_bounds : host->layout.launcher.bounds;
    int32_t on_launcher = reach_launcher_is_open(host->launcher_capsule) && (float)point.x >= launcher_bounds.x &&
                          (float)point.x <= launcher_bounds.x + launcher_bounds.width &&
                          (float)point.y >= launcher_bounds.y &&
                          (float)point.y <= launcher_bounds.y + launcher_bounds.height;
    reach_rect_f32 clipboard_bounds = host->clipboard_surface.bounds_valid
                                          ? host->clipboard_surface.last_bounds
                                          : reach_clipboard_feature_state_ptr(host->clipboard_capsule)->layout.bounds;
    int32_t on_clipboard = reach_clipboard_is_open(host->clipboard_capsule) && (float)point.x >= clipboard_bounds.x &&
                           (float)point.x <= clipboard_bounds.x + clipboard_bounds.width &&
                           (float)point.y >= clipboard_bounds.y &&
                           (float)point.y <= clipboard_bounds.y + clipboard_bounds.height;
    reach_point_i32 dock_point =
        host->has_layout ? reach_dock_local_point(&host->layout.dock, point.x, point.y)
                          : reach_point_i32{};
    reach_dock_pointer_region dock_region =
        host->has_layout ? reach_dock_pointer_region_at(host->dock_capsule, dock_point.x,
                                                        dock_point.y)
                         : REACH_DOCK_POINTER_REGION_NONE;
    int32_t on_tray_button =
        reach_tray_state_ptr(host->tray_capsule)->popup_open &&
        dock_region == REACH_DOCK_POINTER_REGION_TRAY_BUTTON;
    int32_t on_quick_settings_button =
        reach_quick_settings_state_ptr(host->quick_settings_capsule)->open &&
        dock_region == REACH_DOCK_POINTER_REGION_QUICK_SETTINGS_BUTTON;
    int32_t on_power_button = reach_context_menu_state_ptr(host->context_menu_capsule)->open &&
                              reach_context_menu_state_ptr(host->context_menu_capsule)->power_open &&
                              dock_region == REACH_DOCK_POINTER_REGION_POWER_BUTTON;

    auto opening = [host](const reach_host_surface_transition *transition)
    {
        return transition->target_open &&
               reach_host_surface_transition_active(host, transition);
    };

    if (reach_tray_state_ptr(host->tray_capsule)->popup_open && !on_tray && !on_tray_button &&
        !opening(&host->tray_transition))
    {
        reach_host_set_tray_popup_open(host, 0);
    }
    if (reach_context_menu_state_ptr(host->context_menu_capsule)->open && !on_context && !on_power_button &&
        !opening(&host->context_menu_transition))
    {
        reach_host_close_context_menu(host);
        reach_host_clear_sticky_dock_feedback(host);
    }
    if (reach_quick_settings_state_ptr(host->quick_settings_capsule)->open && !on_quick_settings && !on_quick_settings_button &&
        !opening(&host->quick_settings_transition))
    {
        reach_host_set_quick_settings_open(host, 0);
        reach_host_clear_sticky_dock_feedback(host);
    }
    if (reach_launcher_is_open(host->launcher_capsule) && !on_launcher &&
        dock_region == REACH_DOCK_POINTER_REGION_NONE && !opening(&host->launcher_transition))
    {
        reach_host_close_launcher_without_focus_restore(host);
    }
    if (reach_clipboard_is_open(host->clipboard_capsule) && !on_clipboard &&
        !opening(&host->clipboard_transition))
    {
        reach_host_set_clipboard_open(host, 0);
    }
}

static void reach_host_on_popup_mouse_down(void *user, int32_t x, int32_t y)
{
    reach_host *host = static_cast<reach_host *>(user);
    if (host == nullptr)
    {
        return;
    }
    reach_point_i32 point = {x, y};
    reach_host_handle_global_mouse_down(host, point);
}

void reach_host_sync_popup_mouse_hook(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    if (host->popup_capture.sync_mouse_hook != nullptr)
    {

        int32_t should_hook = reach_host_any_surface_open(
            host, reach_surface_class_bit(REACH_SURFACE_CLASS_TRANSIENT) |
                      reach_surface_class_bit(REACH_SURFACE_CLASS_POPUP));
        (void)host->popup_capture.sync_mouse_hook(host->popup_capture.userdata, should_hook,
                                                   reach_host_on_popup_mouse_down, host);
    }
}

void reach_host_close_transient_surfaces(reach_host *host, int32_t restore_launcher_focus)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->cls != REACH_SURFACE_CLASS_TRANSIENT && desc->cls != REACH_SURFACE_CLASS_POPUP)
        {
            continue;
        }
        if (desc->id == REACH_SURFACE_ID_LAUNCHER)
        {
            if (restore_launcher_focus)
            {
                reach_host_close_launcher(host);
            }
            else
            {
                reach_host_close_launcher_without_focus_restore(host);
            }
            continue;
        }
        if (desc->force_close != nullptr)
        {
            desc->force_close(host);
        }
    }
    reach_host_clear_sticky_dock_feedback(host);
}
