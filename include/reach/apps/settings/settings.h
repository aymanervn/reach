#ifndef REACH_FEATURES_SETTINGS_H
#define REACH_FEATURES_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/scrollbar.h"
#include "reach/core/theme.h"
#include "reach/core/windows_update.h"
#include "reach/features/common/text_edit.h"
#include "reach/support/animation.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SETTINGS_NAV_ITEM_COUNT 7
#define REACH_SETTINGS_POWER_TIMER_COUNT 4
#define REACH_SETTINGS_POWER_OPTION_COUNT 6
#define REACH_SETTINGS_POWER_PRESET_COUNT (REACH_SETTINGS_POWER_OPTION_COUNT - 1)
#define REACH_SETTINGS_POWER_CUSTOM_OPTION REACH_SETTINGS_POWER_PRESET_COUNT
#define REACH_SETTINGS_POWER_CUSTOM_DIGITS 2
#define REACH_SETTINGS_POWER_FIELD_HOURS 0
#define REACH_SETTINGS_POWER_FIELD_MINUTES 1
#define REACH_SETTINGS_POWER_FIELD_COUNT 2
#define REACH_SETTINGS_ACCOUNT_NAME_CAPACITY 128
#define REACH_SETTINGS_ACCOUNT_PASSWORD_CAPACITY 64
#define REACH_SETTINGS_ACCOUNT_FIELD_CURRENT 0
#define REACH_SETTINGS_ACCOUNT_FIELD_NEW 1
#define REACH_SETTINGS_ACCOUNT_FIELD_CONFIRM 2
#define REACH_SETTINGS_ACCOUNT_FIELD_COUNT 3

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
        REACH_SETTINGS_HIT_UPDATE_REFRESH,
        REACH_SETTINGS_HIT_UPDATE_INSTALL,
        REACH_SETTINGS_HIT_UPDATE_RESTART,
        REACH_SETTINGS_HIT_UPDATE_CHECKBOX,
        REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_TRACK,
        REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB,
        REACH_SETTINGS_HIT_POWER_OPTION,
        REACH_SETTINGS_HIT_POWER_APPLY,
        REACH_SETTINGS_HIT_POWER_WAIT_TOGGLE,
        REACH_SETTINGS_HIT_ACCOUNT_PASSWORD,
        REACH_SETTINGS_HIT_ACCOUNT_PASSWORD_FIELD
    } reach_settings_hit_type;

    typedef enum reach_settings_account_status
    {
        REACH_SETTINGS_ACCOUNT_STATUS_NONE = 0,
        REACH_SETTINGS_ACCOUNT_STATUS_EMPTY,
        REACH_SETTINGS_ACCOUNT_STATUS_MISMATCH,
        REACH_SETTINGS_ACCOUNT_STATUS_WRONG_CURRENT,
        REACH_SETTINGS_ACCOUNT_STATUS_POLICY,
        REACH_SETTINGS_ACCOUNT_STATUS_ERROR,
        REACH_SETTINGS_ACCOUNT_STATUS_SUCCESS
    } reach_settings_account_status;

    typedef enum reach_settings_power_timer
    {
        REACH_SETTINGS_POWER_TIMER_SLEEP = 0,
        REACH_SETTINGS_POWER_TIMER_LOCK = 1,
        REACH_SETTINGS_POWER_TIMER_SHUTDOWN = 2,
        REACH_SETTINGS_POWER_TIMER_RESTART = 3
    } reach_settings_power_timer;

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
        int32_t update_scan_completed;
        reach_windows_update_list update_list;
        reach_scrollbar_model update_scrollbar;
        int32_t power_minutes[REACH_SETTINGS_POWER_TIMER_COUNT];
        int32_t power_applied_minutes[REACH_SETTINGS_POWER_TIMER_COUNT];
        int32_t power_wait_apps[REACH_SETTINGS_POWER_TIMER_COUNT];
        int32_t power_applied_wait_apps[REACH_SETTINGS_POWER_TIMER_COUNT];
        size_t power_selected[REACH_SETTINGS_POWER_TIMER_COUNT];
        size_t power_previous[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_text_edit power_custom_edits[REACH_SETTINGS_POWER_TIMER_COUNT]
                                          [REACH_SETTINGS_POWER_FIELD_COUNT];
        int32_t power_focused_timer;
        int32_t power_focused_field;
        int32_t power_caret_visible;
        double power_caret_phase;
        reach_animation_track power_tracks[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_animation_manager power_animations;
        reach_animation_track power_wait_tracks[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_animation_manager power_wait_animations;
        uint16_t account_display_name[REACH_SETTINGS_ACCOUNT_NAME_CAPACITY];
        uint16_t account_user_name[REACH_SETTINGS_ACCOUNT_NAME_CAPACITY];
        int32_t account_is_admin;
        uint64_t account_picture;
        reach_text_edit account_password_edits[REACH_SETTINGS_ACCOUNT_FIELD_COUNT];
        int32_t account_focused_field;
        int32_t account_caret_visible;
        double account_caret_phase;
        int32_t account_status;
        int32_t pressed_button;
        reach_animation_track button_press_track;
        reach_animation_manager button_press_animation;
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
        reach_rect_f32 update_refresh_button;
        reach_rect_f32 update_install_button;
        reach_rect_f32 update_restart_button;
        reach_rect_f32 update_viewport;
        reach_rect_f32 update_scrollbar_track;
        reach_rect_f32 update_scrollbar_thumb;
        reach_rect_f32 update_section_titles[3];
        size_t update_section_ids[3];
        size_t update_section_count;
        reach_rect_f32 update_rows[REACH_WINDOWS_UPDATE_MAX_UPDATES];
        reach_rect_f32 update_checkboxes[REACH_WINDOWS_UPDATE_MAX_UPDATES];
        size_t update_indices[REACH_WINDOWS_UPDATE_MAX_UPDATES];
        size_t update_row_count;
        float update_content_height;
        reach_rect_f32 power_cards[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32 power_icon_boxes[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32 power_titles[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32 power_subtitles[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32
            power_options[REACH_SETTINGS_POWER_TIMER_COUNT][REACH_SETTINGS_POWER_OPTION_COUNT];
        reach_rect_f32 power_custom_fields[REACH_SETTINGS_POWER_TIMER_COUNT]
                                          [REACH_SETTINGS_POWER_FIELD_COUNT];
        reach_rect_f32 power_wait_toggles[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32 power_wait_labels[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32 power_apply_button;
        reach_rect_f32 account_card;
        reach_rect_f32 account_avatar;
        reach_rect_f32 account_name;
        reach_rect_f32 account_user;
        reach_rect_f32 account_type_badge;
        reach_rect_f32 account_password_card;
        reach_rect_f32 account_password_icon;
        reach_rect_f32 account_password_title;
        reach_rect_f32 account_password_subtitle;
        reach_rect_f32 account_password_status;
        reach_rect_f32 account_password_fields[REACH_SETTINGS_ACCOUNT_FIELD_COUNT];
        reach_rect_f32 account_password_button;
        reach_settings_nav_item_layout nav_items[REACH_SETTINGS_NAV_ITEM_COUNT];
        size_t nav_item_count;
    } reach_settings_layout;

    typedef struct reach_settings_hit_result
    {
        reach_settings_hit_type type;
        reach_settings_page page;
        size_t update_index;
        size_t power_timer;
        size_t power_option;
        size_t power_custom_field;
        size_t account_field;
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
    size_t reach_settings_model_restart_required_count(const reach_settings_model *model);
    int32_t reach_settings_model_update_busy(const reach_settings_model *model);
    void reach_settings_model_scroll_updates(reach_settings_model *model, float delta);
    int32_t reach_settings_model_update_scroll(reach_settings_model *model, double delta_seconds);

    int32_t reach_settings_power_option_minutes(size_t timer, size_t option);
    const uint16_t *reach_settings_power_option_label(size_t timer, size_t option);
    void reach_settings_model_set_power_minutes(reach_settings_model *model, size_t timer,
                                                int32_t minutes);
    int32_t reach_settings_model_power_minutes(const reach_settings_model *model, size_t timer);
    void reach_settings_model_select_power_option(reach_settings_model *model, size_t timer,
                                                  size_t option);
    int32_t reach_settings_model_tick_power_animations(reach_settings_model *model,
                                                       double delta_seconds);
    int32_t reach_settings_model_power_animations_active(const reach_settings_model *model);
    void reach_settings_model_power_focus_custom(reach_settings_model *model, size_t timer,
                                                 size_t field);
    void reach_settings_model_power_blur(reach_settings_model *model);
    int32_t reach_settings_model_power_insert_char(reach_settings_model *model, uint16_t ch);
    int32_t reach_settings_model_power_handle_edit_key(reach_settings_model *model,
                                                       reach_text_edit_key key,
                                                       reach_text_edit_modifiers modifiers);
    int32_t reach_settings_power_timer_supports_wait(size_t timer);
    void reach_settings_model_set_power_wait_apps(reach_settings_model *model, size_t timer,
                                                  int32_t enabled);
    int32_t reach_settings_model_power_wait_apps(const reach_settings_model *model, size_t timer);
    int32_t reach_settings_model_toggle_power_wait_apps(reach_settings_model *model, size_t timer);
    int32_t reach_settings_model_power_dirty(const reach_settings_model *model);
    void reach_settings_model_power_mark_applied(reach_settings_model *model);
    int32_t reach_settings_model_tick_power_caret(reach_settings_model *model,
                                                  double delta_seconds);

    void reach_settings_model_set_account(reach_settings_model *model,
                                          const uint16_t *display_name, const uint16_t *user_name,
                                          int32_t is_admin, uint64_t picture);
    const uint16_t *reach_settings_account_type_label(int32_t is_admin);
    uint16_t reach_settings_account_initial(const reach_settings_model *model);
    void reach_settings_model_account_focus_password(reach_settings_model *model, size_t field);
    void reach_settings_model_account_blur(reach_settings_model *model);
    int32_t reach_settings_model_account_insert_char(reach_settings_model *model, uint16_t ch);
    int32_t reach_settings_model_account_handle_edit_key(reach_settings_model *model,
                                                         reach_text_edit_key key,
                                                         reach_text_edit_modifiers modifiers);
    int32_t reach_settings_model_account_submit_ready(reach_settings_model *model);
    void reach_settings_model_account_apply_status(reach_settings_model *model, int32_t status);
    int32_t reach_settings_model_tick_account_caret(reach_settings_model *model,
                                                    double delta_seconds);
    const uint16_t *reach_settings_account_status_message(int32_t status);

    void reach_settings_model_press_button(reach_settings_model *model, int32_t hit_type);
    void reach_settings_model_release_button(reach_settings_model *model);
    float reach_settings_model_button_press_value(const reach_settings_model *model,
                                                  int32_t hit_type);
    int32_t reach_settings_model_tick_button_press(reach_settings_model *model,
                                                   double delta_seconds);
    int32_t reach_settings_model_button_press_active(const reach_settings_model *model);

    int32_t
    reach_windows_update_matches_security_maintenance(const reach_windows_update_item *update);
    void reach_windows_update_apply_default_selection(reach_windows_update_list *updates);
    const uint16_t *reach_windows_update_state_label(reach_windows_update_state state);

    const reach_settings_nav_item *reach_settings_nav_items(size_t *out_count);
    const uint16_t *reach_settings_page_title(reach_settings_page page);
    const uint16_t *reach_settings_page_placeholder(reach_settings_page page);
    reach_settings_layout reach_settings_layout_for_bounds(reach_rect_f32 bounds,
                                                           const reach_theme *theme,
                                                           float dpi_scale,
                                                           reach_settings_model *model);
    reach_settings_hit_result reach_settings_hit_test(const reach_settings_layout *layout, float x,
                                                      float y);
    reach_result reach_settings_build_render_commands(const reach_settings_render_input *input,
                                                      reach_render_command_buffer *commands);

#ifdef __cplusplus
}
#endif
#endif
