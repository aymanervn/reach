#ifndef REACH_FEATURES_CONTEXT_MENU_H
#define REACH_FEATURES_CONTEXT_MENU_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/menu_commands.h"
#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/features/feature_capsule.h"
#include "reach/features/popup.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_context_menu_pointer_action_kind
    {
        REACH_CONTEXT_MENU_POINTER_ACTION_NONE = 0,
        REACH_CONTEXT_MENU_POINTER_ACTION_DISMISS = 1,
        REACH_CONTEXT_MENU_POINTER_ACTION_EXECUTE = 2,
        REACH_CONTEXT_MENU_POINTER_ACTION_FOCUS_WINDOW = 3,
        REACH_CONTEXT_MENU_POINTER_ACTION_CLOSE_WINDOW = 4
    } reach_context_menu_pointer_action_kind;

    void reach_context_menu_build_power_commands(uint32_t *out_commands, uint32_t *out_icon_ids,
                                                 size_t *out_count);
    const uint16_t *reach_context_menu_command_text(uint32_t command);

    typedef struct reach_context_menu_state
    {
        int32_t open;
        int32_t power_open;
        int32_t window_list_open;
        size_t target_index;
        reach_rect_f32 bounds;
        reach_rect_f32 item_slots[REACH_CONTEXT_MENU_MAX_ITEMS];
        uint32_t item_commands[REACH_CONTEXT_MENU_MAX_ITEMS];
        uint32_t item_icon_ids[REACH_CONTEXT_MENU_MAX_ITEMS];
        uintptr_t item_windows[REACH_CONTEXT_MENU_MAX_ITEMS];
        uint16_t item_titles[REACH_CONTEXT_MENU_MAX_ITEMS][260];
        size_t item_count;
        size_t hovered_index;
        size_t close_hovered_index;
        float dpi_scale;

        int32_t anchored;
        int32_t drop_direction;
        float anchor_popup_width;
        float anchor_ratio;
        float notch_anchor_x;
    } reach_context_menu_state;

    typedef struct reach_context_menu_window_entry
    {
        uintptr_t window;
        const uint16_t *title;
    } reach_context_menu_window_entry;

    typedef struct reach_context_menu reach_context_menu;

    reach_result reach_context_menu_create(reach_context_menu **out_menu);
    void reach_context_menu_destroy(reach_context_menu *menu);

    const reach_feature_capsule_ops *reach_context_menu_capsule_ops(void);

    int32_t reach_context_menu_is_open(const reach_context_menu *menu);
    void reach_context_menu_force_close(reach_context_menu *menu);
    void reach_context_menu_reset(reach_context_menu *menu);

    typedef struct reach_context_menu_open_context
    {
        reach_rect_f32 monitor;
        float dpi_scale;
        reach_rect_f32 anchor_button;
        float bar_edge_y;
        int32_t drop_direction;
        int32_t anchored;
        float pointer_x;
        float pointer_y;
        const uint32_t *item_commands;
        size_t item_count;
        const reach_context_menu_window_entry *window_entries;
        size_t window_entry_count;
    } reach_context_menu_open_context;

    void reach_context_menu_open_power(reach_context_menu *menu,
                                       const reach_context_menu_open_context *ctx);

    void reach_context_menu_reanchor(reach_context_menu *menu,
                                     const reach_context_menu_open_context *ctx);
    void reach_context_menu_open_for_item(reach_context_menu *menu, size_t target_index,
                                          const reach_context_menu_open_context *ctx);
    void reach_context_menu_open_window_list(reach_context_menu *menu, size_t target_index,
                                             const reach_context_menu_open_context *ctx);

    int32_t reach_context_menu_window_list_is_open(const reach_context_menu *menu);
    float reach_context_menu_hover_opacity(const reach_context_menu *menu);
    float reach_context_menu_close_hover(const reach_context_menu *menu);

    size_t reach_context_menu_window_list_remove(reach_context_menu *menu, uintptr_t window);

    int32_t reach_context_menu_hover_region_contains(reach_rect_f32 popup_bounds,
                                                     reach_rect_f32 anchor_slot, float bar_edge_y,
                                                     int32_t drop_direction, float margin, float x,
                                                     float y);

    const reach_context_menu_state *reach_context_menu_state_ptr(reach_context_menu *menu);

    typedef struct reach_context_menu_render_context
    {
        const reach_theme *theme;
        float dpi_scale;
    } reach_context_menu_render_context;

    reach_result
    reach_context_menu_append_render_commands(reach_context_menu *menu,
                                              const reach_context_menu_render_context *ctx,
                                              reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
