#ifndef REACH_FEATURES_SETTINGS_H
#define REACH_FEATURES_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/windows_update.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SETTINGS_NAV_ITEM_COUNT 7

    typedef enum reach_settings_page
    {
        REACH_SETTINGS_PAGE_WIFI = 0,
        REACH_SETTINGS_PAGE_BLUETOOTH = 1,
        REACH_SETTINGS_PAGE_ACCOUNT = 2,
        REACH_SETTINGS_PAGE_STARTUP_APPS = 3,
        REACH_SETTINGS_PAGE_POWER_SLEEP = 4,
        REACH_SETTINGS_PAGE_MONITORS_SCALING = 5,
        REACH_SETTINGS_PAGE_UPDATE = 6
    } reach_settings_page;

    typedef enum reach_settings_hit_type
    {
        REACH_SETTINGS_HIT_NONE = 0,
        REACH_SETTINGS_HIT_NAV_ITEM,
        REACH_SETTINGS_HIT_CLOSE,
        REACH_SETTINGS_HIT_MINIMIZE,
        REACH_SETTINGS_HIT_UPDATE_SEARCH,
        REACH_SETTINGS_HIT_UPDATE_INSTALL,
        REACH_SETTINGS_HIT_UPDATE_CHECKBOX
    } reach_settings_hit_type;

    typedef enum reach_settings_update_page_state
    {
        REACH_SETTINGS_UPDATE_NOT_SCANNED = 0,
        REACH_SETTINGS_UPDATE_SCANNING,
        REACH_SETTINGS_UPDATE_AVAILABLE,
        REACH_SETTINGS_UPDATE_PREPARING,
        REACH_SETTINGS_UPDATE_DOWNLOADING,
        REACH_SETTINGS_UPDATE_INSTALLING,
        REACH_SETTINGS_UPDATE_VERIFYING,
        REACH_SETTINGS_UPDATE_COMPLETE,
        REACH_SETTINGS_UPDATE_ERROR
    } reach_settings_update_page_state;

    typedef struct reach_settings_model
    {
        reach_settings_page selected_page;
        reach_settings_update_page_state update_page_state;
        reach_windows_update_list update_list;
        uint16_t update_status[REACH_WINDOWS_UPDATE_TEXT_CAPACITY];
        int32_t update_operation_hresult;
        size_t update_scroll_offset;
    } reach_settings_model;

    typedef struct reach_settings_nav_item
    {
        reach_settings_page page;
        uint32_t icon_id;
        const uint16_t *label;
        reach_color accent;
        reach_color accent_background;
    } reach_settings_nav_item;

    typedef struct reach_settings_nav_item_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 icon_background;
        reach_rect_f32 icon;
        reach_rect_f32 label;
    } reach_settings_nav_item_layout;

    typedef struct reach_settings_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 close_button;
        reach_rect_f32 minimize_button;
        reach_rect_f32 nav;
        reach_rect_f32 content;
        reach_rect_f32 content_title;
        reach_rect_f32 content_placeholder;
        reach_rect_f32 update_search_button;
        reach_rect_f32 update_install_button;
        reach_rect_f32 update_status;
        reach_rect_f32 update_rows[REACH_WINDOWS_UPDATE_MAX_UPDATES];
        reach_rect_f32 update_checkboxes[REACH_WINDOWS_UPDATE_MAX_UPDATES];
        size_t update_row_count;
        reach_settings_nav_item_layout nav_items[REACH_SETTINGS_NAV_ITEM_COUNT];
        size_t nav_item_count;
    } reach_settings_layout;

    typedef struct reach_settings_hit_result
    {
        reach_settings_hit_type type;
        reach_settings_page page;
        size_t update_index;
    } reach_settings_hit_result;

    typedef struct reach_settings_render_input
    {
        const reach_theme *theme;
        const reach_settings_model *model;
        const reach_settings_layout *layout;
        float dpi_scale;
        int32_t text_alignment_leading;
    } reach_settings_render_input;

    void reach_settings_model_init(reach_settings_model *model);
    void reach_settings_model_select_page(reach_settings_model *model, reach_settings_page page);
    void reach_settings_model_begin_update_scan(reach_settings_model *model);
    void reach_settings_model_apply_update_scan(reach_settings_model *model,
                                                const reach_windows_update_list *updates,
                                                int32_t hresult);
    void reach_settings_model_begin_update_install(reach_settings_model *model);
    void reach_settings_model_apply_update_operation(
        reach_settings_model *model, const reach_windows_update_operation_result *result);
    void reach_settings_model_toggle_update(reach_settings_model *model, size_t index);
    size_t reach_settings_model_selected_update_count(const reach_settings_model *model);
    int32_t reach_settings_model_update_busy(const reach_settings_model *model);
    void reach_settings_model_scroll_updates(reach_settings_model *model, int32_t delta,
                                             size_t visible_count);

    const reach_settings_nav_item *reach_settings_nav_items(size_t *out_count);
    const uint16_t *reach_settings_page_title(reach_settings_page page);
    const uint16_t *reach_settings_page_placeholder(reach_settings_page page);
    reach_settings_layout reach_settings_layout_for_bounds(reach_rect_f32 bounds,
                                                           const reach_theme *theme,
                                                           float dpi_scale);
    reach_settings_hit_result reach_settings_hit_test(const reach_settings_layout *layout, float x,
                                                      float y);
    reach_result reach_settings_build_render_commands(const reach_settings_render_input *input,
                                                      reach_render_command_buffer *commands);

#ifdef __cplusplus
}
#endif
#endif
