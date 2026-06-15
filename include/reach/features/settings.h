#ifndef REACH_FEATURES_SETTINGS_H
#define REACH_FEATURES_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SETTINGS_NAV_ITEM_COUNT 6

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
        REACH_SETTINGS_HIT_MINIMIZE
    } reach_settings_hit_type;

    typedef struct reach_settings_model
    {
        reach_settings_page selected_page;
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
        reach_settings_nav_item_layout nav_items[REACH_SETTINGS_NAV_ITEM_COUNT];
        size_t nav_item_count;
    } reach_settings_layout;

    typedef struct reach_settings_hit_result
    {
        reach_settings_hit_type type;
        reach_settings_page page;
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
