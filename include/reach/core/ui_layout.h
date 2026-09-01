#ifndef REACH_CORE_UI_LAYOUT_H
#define REACH_CORE_UI_LAYOUT_H

#include "reach/core/ui_state.h"

#define REACH_DOCK_BOTTOM_MARGIN_DP 12.0f
#define REACH_DOCK_SIDE_MARGIN_DP 32.0f

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_ui_layout_input
    {
        reach_rect_f32 monitor_bounds;
        reach_rect_f32 work_area;
        float dpi_scale;
        float border_thickness;
    } reach_ui_layout_input;

    typedef struct reach_dock_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 trigger_button;
        reach_rect_f32 app_slots[REACH_MAX_DOCK_ITEMS];
        size_t app_slot_count;
        float available_width;
        float native_height;
        float content_scale;
    } reach_dock_layout;

    typedef struct reach_launcher_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 envelope_bounds;
        reach_rect_f32 search_box;
        reach_rect_f32 search_text_input;
        reach_rect_f32 search_icon;
        reach_rect_f32 search_results;
        reach_rect_f32 search_result_items;
        reach_rect_f32 search_result_scrollbar_track;
        reach_rect_f32 search_result_scrollbar_thumb;
    } reach_launcher_layout;

    typedef struct reach_ui_layout
    {
        reach_dock_layout dock;
    } reach_ui_layout;

#define REACH_DISPLAY_MAX_MONITORS 8

    /* Monitors resolved by composition, so a feature that has to reason about where a window
       lives never reaches for the platform itself. */
    typedef struct reach_display_environment
    {
        reach_rect_f32 primary_bounds;
        reach_rect_f32 monitors[REACH_DISPLAY_MAX_MONITORS];
        size_t monitor_count;
        uintptr_t desktop_window;
        int32_t icon_size_px;
        float dpi_scale;
    } reach_display_environment;

    reach_result reach_dock_layout_compute(const reach_dock_model *dock,
                                           const reach_ui_layout_input *input,
                                           reach_dock_layout *out_layout);
    reach_result reach_launcher_layout_compute(const reach_launcher_model *launcher,
                                               const reach_ui_layout_input *input,
                                               reach_launcher_layout *out_layout);

#ifdef __cplusplus
}
#endif

#endif
