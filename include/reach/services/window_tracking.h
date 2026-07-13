#ifndef REACH_SERVICES_WINDOW_TRACKING_H
#define REACH_SERVICES_WINDOW_TRACKING_H

#include <stdint.h>

#include "reach/core/pinned_app.h"
#include "reach/ports/window_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * Window-tracking service — the in-process window state shared by the
     * features (dock items, switcher ring, launcher focus policy, context-menu
     * capabilities).
     *
     * It OWNS the open-window snapshot cache (diffed against the
     * window-manager port on refresh) and the focus history. It is a pure
     * READ MODEL: every action (launch/activate/minimize/close) lives in the
     * app_control service.
     *
     * The service depends only on the window-manager PORT; the concrete
     * Windows adapter is injected by composition. It knows nothing about
     * features or render icons: a refresh returns a change report (what
     * changed + the pre-refresh identity snapshot) and composition translates
     * that into dock rebuilds and render-icon slot migration.
     */
    typedef struct reach_window_tracking reach_window_tracking;

    /*
     * Refresh change report. `changed` covers anything a surface shows
     * (identity, minimized/maximized/visible, title); `items_changed` is the
     * identity subset (the dock rebuilds its items); `icon_identity_changed`
     * additionally covers icon-ref changes (render-icon slots re-sync against
     * the old_* pre-refresh snapshot below).
     */
    typedef struct reach_window_tracking_refresh_report
    {
        int32_t changed;
        int32_t items_changed;
        int32_t icon_identity_changed;
        size_t old_count;
        uintptr_t old_windows[REACH_MAX_PINNED_APPS];
        uint16_t old_icon_refs[REACH_MAX_PINNED_APPS][260];
    } reach_window_tracking_refresh_report;

    /* The port is borrowed (composition owns and destroys it). */
    reach_result reach_window_tracking_create(reach_window_manager_port window_manager,
                                              reach_window_tracking **out_service);
    void reach_window_tracking_destroy(reach_window_tracking *service);

    /* Snapshot cache: re-read the window list from the port and diff. */
    reach_result reach_window_tracking_refresh(reach_window_tracking *service,
                                               reach_window_tracking_refresh_report *out_report);
    const reach_window_snapshot *reach_window_tracking_windows(const reach_window_tracking *service);
    size_t reach_window_tracking_window_count(const reach_window_tracking *service);
    int32_t reach_window_tracking_window_is_minimized(const reach_window_tracking *service,
                                                      uintptr_t window_id);
    const reach_window_snapshot *reach_window_tracking_window_by_id(
        const reach_window_tracking *service, uintptr_t window_id);

    /* Pinned-app <-> window identity rule (pure): AUMID match when both have
       one, else case-insensitive path match. Shared by the dock item builder
       and composition's launch-or-focus policy. */
    int32_t reach_window_tracking_window_matches_app(const reach_pinned_app_model *app,
                                                     const reach_window_snapshot *window);
    size_t reach_window_tracking_collect_unminimized(const reach_window_tracking *service,
                                                     uintptr_t *out_windows,
                                                     size_t out_window_count);

    /* Focus: note the foreground window (maintains the focus history). */
    void reach_window_tracking_note_foreground(reach_window_tracking *service,
                                               uintptr_t foreground_window);
    uintptr_t reach_window_tracking_foreground(const reach_window_tracking *service);
    const uintptr_t *reach_window_tracking_focus_history(const reach_window_tracking *service);
    size_t reach_window_tracking_focus_history_count(const reach_window_tracking *service);


#ifdef __cplusplus
}
#endif

#endif
