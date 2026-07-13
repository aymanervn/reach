#ifndef REACH_CORE_UI_LAYOUT_H
#define REACH_CORE_UI_LAYOUT_H

#include "reach/core/ui_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_ui_layout_input
    {
        reach_rect_f32 monitor_bounds;
        reach_rect_f32 work_area;
        float dpi_scale;

        size_t pinned_app_count;
    } reach_ui_layout_input;

    typedef struct reach_dock_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 now_playing;
        reach_rect_f32 app_slots[REACH_MAX_PINNED_APPS];
        reach_rect_f32 tray_button;
        reach_rect_f32 quick_settings_button;
        reach_rect_f32 system_separator;
        reach_rect_f32 clock;
        reach_rect_f32 power_button;
        size_t app_slot_count;
    } reach_dock_layout;

    typedef struct reach_launcher_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 search_box;
        reach_rect_f32 search_text_input;
        reach_rect_f32 pinned_app_slots[REACH_MAX_PINNED_APPS];
        reach_rect_f32 search_results;
        reach_rect_f32 search_result_items;
        reach_rect_f32 search_result_scrollbar_track;
        reach_rect_f32 search_result_scrollbar_thumb;
        size_t pinned_app_slot_count;
    } reach_launcher_layout;

    typedef struct reach_ui_layout
    {
        reach_dock_layout dock;
        reach_launcher_layout launcher;
    } reach_ui_layout;

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
