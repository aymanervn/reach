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

    typedef struct reach_context_menu_render_input
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        const reach_rect_f32 *item_slots;
        const uint32_t *item_commands;
        const uint32_t *item_icon_ids;
        size_t item_count;
        size_t hovered_index;
        size_t target_index;
        const reach_dock_layout *dock_layout;
        int32_t has_layout;
        int32_t use_anchor_x;
        float anchor_x;
        float dpi_scale;
        int32_t text_alignment_leading;
    } reach_context_menu_render_input;

    typedef enum reach_context_menu_pointer_action_kind
    {
        REACH_CONTEXT_MENU_POINTER_ACTION_NONE = 0,
        REACH_CONTEXT_MENU_POINTER_ACTION_DISMISS = 1,
        REACH_CONTEXT_MENU_POINTER_ACTION_EXECUTE = 2
    } reach_context_menu_pointer_action_kind;

    void reach_context_menu_build_power_commands(uint32_t *out_commands, uint32_t *out_icon_ids,
                                                 size_t *out_count);
    const uint16_t *reach_context_menu_command_text(uint32_t command);
    reach_result
    reach_context_menu_build_render_commands(const reach_context_menu_render_input *input,
                                             reach_render_command_buffer *out_commands);

    typedef struct reach_context_menu_state
    {
        int32_t open;
        int32_t power_open;
        size_t target_index;
        reach_rect_f32 bounds;
        reach_rect_f32 item_slots[REACH_CONTEXT_MENU_MAX_ITEMS];
        uint32_t item_commands[REACH_CONTEXT_MENU_MAX_ITEMS];
        uint32_t item_icon_ids[REACH_CONTEXT_MENU_MAX_ITEMS];
        size_t item_count;
        size_t hovered_index;

        int32_t anchored;
        float anchor_popup_width;
        float anchor_ratio;
    } reach_context_menu_state;

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
        float anchor_x;
        float dock_top_y;
        int32_t anchored;
        float pointer_x;
        float pointer_y;
        const uint32_t *item_commands;
        size_t item_count;
    } reach_context_menu_open_context;

    void reach_context_menu_open_power(reach_context_menu *menu,
                                       const reach_context_menu_open_context *ctx);

    void reach_context_menu_reanchor(reach_context_menu *menu,
                                     const reach_context_menu_open_context *ctx);
    void reach_context_menu_open_for_item(reach_context_menu *menu, size_t target_index,
                                          const reach_context_menu_open_context *ctx);

    const reach_context_menu_state *reach_context_menu_state_ptr(reach_context_menu *menu);

    typedef struct reach_context_menu_render_context
    {
        const reach_theme *theme;
        const reach_dock_layout *dock_layout;
        int32_t has_layout;
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
