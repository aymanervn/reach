#ifndef REACH_SERVICES_WINDOW_TRACKING_H
#define REACH_SERVICES_WINDOW_TRACKING_H

#include <stdint.h>

#include "reach/core/pinned_app.h"
#include "reach/ports/window_manager.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_window_tracking reach_window_tracking;

    typedef struct reach_window_tracking_refresh_report
    {
        int32_t changed;
        int32_t items_changed;
        int32_t icon_identity_changed;
        size_t old_count;
        uintptr_t old_windows[REACH_MAX_PINNED_APPS];
        uint16_t old_icon_refs[REACH_MAX_PINNED_APPS][260];
    } reach_window_tracking_refresh_report;

    reach_result reach_window_tracking_create(reach_window_manager_port window_manager,
                                              reach_window_tracking **out_service);
    void reach_window_tracking_destroy(reach_window_tracking *service);

    reach_result reach_window_tracking_refresh(reach_window_tracking *service,
                                               reach_window_tracking_refresh_report *out_report);
    const reach_window_snapshot *reach_window_tracking_windows(const reach_window_tracking *service);
    size_t reach_window_tracking_window_count(const reach_window_tracking *service);
    int32_t reach_window_tracking_window_is_minimized(const reach_window_tracking *service,
                                                      uintptr_t window_id);
    const reach_window_snapshot *reach_window_tracking_window_by_id(
        const reach_window_tracking *service, uintptr_t window_id);

    int32_t reach_window_tracking_window_matches_app(const reach_pinned_app_model *app,
                                                     const reach_window_snapshot *window);
    size_t reach_window_tracking_collect_unminimized(const reach_window_tracking *service,
                                                     uintptr_t *out_windows,
                                                     size_t out_window_count);

    void reach_window_tracking_note_foreground(reach_window_tracking *service,
                                               uintptr_t foreground_window);
    uintptr_t reach_window_tracking_foreground(const reach_window_tracking *service);
    const uintptr_t *reach_window_tracking_focus_history(const reach_window_tracking *service);
    size_t reach_window_tracking_focus_history_count(const reach_window_tracking *service);

#ifdef __cplusplus
}
#endif

#endif
