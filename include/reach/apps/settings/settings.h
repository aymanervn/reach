#ifndef REACH_FEATURES_SETTINGS_H
#define REACH_FEATURES_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/app_update.h"
#include "reach/core/bluetooth.h"
#include "reach/core/config.h"
#include "reach/core/loader.h"
#include "reach/core/render_commands.h"
#include "reach/core/scrollbar.h"
#include "reach/features/common/pressable.h"
#include "reach/ports/system_controls.h"
#include "reach/core/startup_apps.h"
#include "reach/core/theme.h"
#include "reach/core/wifi.h"
#include "reach/core/windows_update.h"
#include "reach/features/common/text_edit.h"
#include "reach/support/animation.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SETTINGS_NAV_ITEM_COUNT 7
#define REACH_SETTINGS_POWER_TIMER_COUNT 5
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
#define REACH_SETTINGS_THEME_OPTION_COUNT 3
#define REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT 3
#define REACH_SETTINGS_WIFI_FIELD_NONE (-1)
#define REACH_SETTINGS_WIFI_FIELD_KEY 0
#define REACH_SETTINGS_WIFI_FIELD_ADD_NAME 1
#define REACH_SETTINGS_WIFI_FIELD_ADD_KEY 2
#define REACH_SETTINGS_WIFI_ROW_NONE (-1)
#define REACH_SETTINGS_WIFI_ROW_ADD (-2)
#define REACH_SETTINGS_BLUETOOTH_ROW_NONE (-1)

    typedef enum reach_settings_page
    {
        REACH_SETTINGS_PAGE_WIFI = 0,
        REACH_SETTINGS_PAGE_BLUETOOTH = 1,
        REACH_SETTINGS_PAGE_ACCOUNT = 2,
        REACH_SETTINGS_PAGE_STARTUP_APPS = 3,
        REACH_SETTINGS_PAGE_POWER_SLEEP = 4,
        REACH_SETTINGS_PAGE_DISPLAY = 5,
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
        REACH_SETTINGS_HIT_REACH_UPDATE,
        REACH_SETTINGS_HIT_UPDATE_CHECKBOX,
        REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_TRACK,
        REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB,
        REACH_SETTINGS_HIT_POWER_OPTION,
        REACH_SETTINGS_HIT_POWER_APPLY,
        REACH_SETTINGS_HIT_POWER_WAIT_TOGGLE,
        REACH_SETTINGS_HIT_DISPLAY_FPS_TOGGLE,
        REACH_SETTINGS_HIT_DISPLAY_FONT_TOGGLE,
        REACH_SETTINGS_HIT_DISPLAY_THEME_TOGGLE,
        REACH_SETTINGS_HIT_DISPLAY_WINDOWS_SYSTEM_THEME,
        REACH_SETTINGS_HIT_DISPLAY_WINDOWS_APP_THEME,
        REACH_SETTINGS_HIT_ACCOUNT_PASSWORD,
        REACH_SETTINGS_HIT_ACCOUNT_PASSWORD_FIELD,
        REACH_SETTINGS_HIT_STARTUP_TOGGLE,
        REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_TRACK,
        REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_THUMB,
        REACH_SETTINGS_HIT_WIFI_RADIO_TOGGLE,
        REACH_SETTINGS_HIT_WIFI_SCAN,
        REACH_SETTINGS_HIT_WIFI_ADD,
        REACH_SETTINGS_HIT_WIFI_KNOWN,
        REACH_SETTINGS_HIT_WIFI_BACK,
        REACH_SETTINGS_HIT_WIFI_ROW,
        REACH_SETTINGS_HIT_WIFI_KEY_FIELD,
        REACH_SETTINGS_HIT_WIFI_SHOW_KEY,
        REACH_SETTINGS_HIT_WIFI_AUTO_TOGGLE,
        REACH_SETTINGS_HIT_WIFI_CONNECT,
        REACH_SETTINGS_HIT_WIFI_DISCONNECT,
        REACH_SETTINGS_HIT_WIFI_FORGET,
        REACH_SETTINGS_HIT_WIFI_ADD_NAME_FIELD,
        REACH_SETTINGS_HIT_WIFI_ADD_KEY_FIELD,
        REACH_SETTINGS_HIT_WIFI_ADD_SHOW_KEY,
        REACH_SETTINGS_HIT_WIFI_ADD_SECURITY,
        REACH_SETTINGS_HIT_WIFI_ADD_AUTO_TOGGLE,
        REACH_SETTINGS_HIT_WIFI_ADD_SUBMIT,
        REACH_SETTINGS_HIT_WIFI_SCROLLBAR_TRACK,
        REACH_SETTINGS_HIT_WIFI_SCROLLBAR_THUMB,
        REACH_SETTINGS_HIT_BLUETOOTH_RADIO_TOGGLE,
        REACH_SETTINGS_HIT_BLUETOOTH_SCAN,
        REACH_SETTINGS_HIT_BLUETOOTH_ROW,
        REACH_SETTINGS_HIT_BLUETOOTH_ACTION,
        REACH_SETTINGS_HIT_BLUETOOTH_PIN_ACCEPT,
        REACH_SETTINGS_HIT_BLUETOOTH_PIN_REJECT,
        REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_TRACK,
        REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_THUMB
    } reach_settings_hit_type;

    typedef enum reach_settings_startup_status
    {
        REACH_SETTINGS_STARTUP_STATUS_NONE = 0,
        REACH_SETTINGS_STARTUP_STATUS_LOADING,
        REACH_SETTINGS_STARTUP_STATUS_FAILED
    } reach_settings_startup_status;

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
        REACH_SETTINGS_POWER_TIMER_SCREEN_OFF = 0,
        REACH_SETTINGS_POWER_TIMER_SLEEP = 1,
        REACH_SETTINGS_POWER_TIMER_LOCK = 2,
        REACH_SETTINGS_POWER_TIMER_SHUTDOWN = 3,
        REACH_SETTINGS_POWER_TIMER_RESTART = 4
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

    typedef enum reach_settings_reach_update_state
    {
        REACH_SETTINGS_REACH_UPDATE_IDLE = 0,
        REACH_SETTINGS_REACH_UPDATE_CHECKING,
        REACH_SETTINGS_REACH_UPDATE_UP_TO_DATE,
        REACH_SETTINGS_REACH_UPDATE_AVAILABLE,
        REACH_SETTINGS_REACH_UPDATE_DOWNLOADING,
        REACH_SETTINGS_REACH_UPDATE_ERROR
    } reach_settings_reach_update_state;

    typedef enum reach_settings_wifi_view
    {
        REACH_SETTINGS_WIFI_VIEW_AVAILABLE = 0,
        REACH_SETTINGS_WIFI_VIEW_KNOWN
    } reach_settings_wifi_view;

    typedef enum reach_settings_wifi_status
    {
        REACH_SETTINGS_WIFI_STATUS_IDLE = 0,
        REACH_SETTINGS_WIFI_STATUS_SCANNING,
        REACH_SETTINGS_WIFI_STATUS_CONNECTING,
        REACH_SETTINGS_WIFI_STATUS_CONNECTED,
        REACH_SETTINGS_WIFI_STATUS_INVALID_KEY,
        REACH_SETTINGS_WIFI_STATUS_NOT_FOUND,
        REACH_SETTINGS_WIFI_STATUS_FAILED,
        REACH_SETTINGS_WIFI_STATUS_SCAN_FAILED,
        REACH_SETTINGS_WIFI_STATUS_FORGET_FAILED
    } reach_settings_wifi_status;

    typedef enum reach_settings_bluetooth_status
    {
        REACH_SETTINGS_BLUETOOTH_STATUS_IDLE = 0,
        REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING,
        REACH_SETTINGS_BLUETOOTH_STATUS_PAIRING,
        REACH_SETTINGS_BLUETOOTH_STATUS_CONFIRM_PIN,
        REACH_SETTINGS_BLUETOOTH_STATUS_PAIRED,
        REACH_SETTINGS_BLUETOOTH_STATUS_REJECTED,
        REACH_SETTINGS_BLUETOOTH_STATUS_FAILED
    } reach_settings_bluetooth_status;

    typedef struct reach_settings_model
    {
        reach_settings_page selected_page;
        reach_animation_track nav_selection_track;
        reach_animation_manager nav_selection_animation;
        reach_settings_update_page_state update_page_state;
        int32_t update_scan_completed;
        reach_windows_update_list update_list;
        reach_scrollbar_model update_scrollbar;
        reach_loader_model update_loader;
        reach_settings_reach_update_state reach_update_state;
        reach_app_update_info reach_update_info;
        uint16_t reach_current_version[REACH_APP_UPDATE_VERSION_CAPACITY];
        uint64_t reach_download_received;
        uint64_t reach_download_total;
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
        reach_startup_app_list startup_apps;
        uint64_t startup_icons[REACH_STARTUP_APP_MAX_ENTRIES];
        reach_scrollbar_model startup_scrollbar;
        reach_animation_track startup_tracks[REACH_STARTUP_APP_MAX_ENTRIES];
        reach_animation_manager startup_animations;
        int32_t startup_loaded;
        int32_t startup_busy;
        int32_t startup_status;
        int32_t display_high_refresh_rate;
        reach_animation_track display_fps_track;
        reach_animation_manager display_fps_animation;
        int32_t display_bundled_font;
        reach_animation_track display_font_track;
        reach_animation_manager display_font_animation;
        int32_t display_light_theme;
        reach_animation_track display_theme_track;
        reach_animation_manager display_theme_animation;
        reach_config_theme_preference display_windows_system_theme;
        reach_config_theme_preference display_windows_app_theme;
        int32_t hovered_button;
        reach_animation_track button_press_track;
        reach_animation_manager button_press_animation;
        reach_pressable button_pressable;
        reach_wifi_radio_state wifi_radio;
        reach_wifi_network_list wifi_networks;
        reach_settings_wifi_view wifi_view;
        int32_t wifi_status;
        uint16_t wifi_status_ssid[REACH_WIFI_SSID_CAPACITY];
        int32_t wifi_expanded_row;
        int32_t wifi_focused_field;
        int32_t wifi_show_key;
        int32_t wifi_connect_automatically;
        reach_wifi_security wifi_add_security;
        reach_text_edit wifi_key_edit;
        reach_text_edit wifi_add_name_edit;
        reach_text_edit wifi_add_key_edit;
        int32_t wifi_caret_visible;
        double wifi_caret_phase;
        int32_t wifi_loaded;
        reach_scrollbar_model wifi_scrollbar;
        reach_loader_model wifi_loader;
        reach_animation_track wifi_row_tracks[REACH_WIFI_MAX_NETWORKS + 1];
        reach_animation_manager wifi_row_animations;
        reach_animation_track wifi_radio_track;
        reach_animation_manager wifi_radio_animation;
        reach_bluetooth_device_list bluetooth_devices;
        uint64_t bluetooth_icons[REACH_BLUETOOTH_MAX_DEVICES];
        reach_bluetooth_pairing_request bluetooth_pairing;
        reach_bluetooth_state bluetooth_radio;
        int32_t bluetooth_status;
        uint16_t bluetooth_status_device[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
        int32_t bluetooth_expanded_row;
        int32_t bluetooth_scanning;
        int32_t bluetooth_loaded;
        reach_scrollbar_model bluetooth_scrollbar;
        reach_loader_model bluetooth_loader;
        reach_animation_track bluetooth_row_tracks[REACH_BLUETOOTH_MAX_DEVICES];
        reach_animation_manager bluetooth_row_animations;
        reach_animation_track bluetooth_radio_track;
        reach_animation_manager bluetooth_radio_animation;
    } reach_settings_model;

    typedef struct reach_settings_nav_item
    {
        reach_settings_page page;
        uint32_t icon_id;
        const uint16_t *label;
        reach_theme_accent accent;
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
        reach_rect_f32 reach_section_title;
        reach_rect_f32 reach_update_row;
        reach_rect_f32 reach_update_button;
        reach_rect_f32 windows_section_title;
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
        reach_rect_f32 power_options[REACH_SETTINGS_POWER_TIMER_COUNT]
                                    [REACH_SETTINGS_POWER_OPTION_COUNT];
        reach_rect_f32 power_custom_fields[REACH_SETTINGS_POWER_TIMER_COUNT]
                                          [REACH_SETTINGS_POWER_FIELD_COUNT];
        reach_rect_f32 power_wait_toggles[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32 power_wait_labels[REACH_SETTINGS_POWER_TIMER_COUNT];
        reach_rect_f32 power_apply_button;
        reach_rect_f32 startup_summary;
        reach_rect_f32 startup_viewport;
        reach_rect_f32 startup_scrollbar_track;
        reach_rect_f32 startup_scrollbar_thumb;
        reach_rect_f32 startup_rows[REACH_STARTUP_APP_MAX_ENTRIES];
        reach_rect_f32 startup_toggles[REACH_STARTUP_APP_MAX_ENTRIES];
        size_t startup_row_count;
        float startup_content_height;
        reach_rect_f32 display_fps_card;
        reach_rect_f32 display_fps_icon;
        reach_rect_f32 display_fps_title;
        reach_rect_f32 display_fps_subtitle;
        reach_rect_f32 display_fps_toggle;
        reach_rect_f32 display_font_card;
        reach_rect_f32 display_font_icon;
        reach_rect_f32 display_font_title;
        reach_rect_f32 display_font_subtitle;
        reach_rect_f32 display_font_toggle;
        reach_rect_f32 display_theme_card;
        reach_rect_f32 display_theme_icon;
        reach_rect_f32 display_theme_title;
        reach_rect_f32 display_theme_subtitle;
        reach_rect_f32 display_theme_toggle;
        reach_rect_f32 display_windows_section_title;
        reach_rect_f32 display_windows_system_card;
        reach_rect_f32 display_windows_system_title;
        reach_rect_f32 display_windows_system_subtitle;
        reach_rect_f32 display_windows_system_options[REACH_SETTINGS_THEME_OPTION_COUNT];
        reach_rect_f32 display_windows_app_card;
        reach_rect_f32 display_windows_app_title;
        reach_rect_f32 display_windows_app_subtitle;
        reach_rect_f32 display_windows_app_options[REACH_SETTINGS_THEME_OPTION_COUNT];
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
        reach_rect_f32 wifi_radio_card;
        reach_rect_f32 wifi_radio_icon;
        reach_rect_f32 wifi_radio_title;
        reach_rect_f32 wifi_radio_subtitle;
        reach_rect_f32 wifi_radio_toggle;
        reach_rect_f32 wifi_scan_button;
        reach_rect_f32 wifi_add_button;
        reach_rect_f32 wifi_known_button;
        reach_rect_f32 wifi_back_button;
        reach_rect_f32 wifi_loader_bar;
        reach_rect_f32 wifi_viewport;
        reach_rect_f32 wifi_scrollbar_track;
        reach_rect_f32 wifi_scrollbar_thumb;
        reach_rect_f32 wifi_rows[REACH_WIFI_MAX_NETWORKS];
        size_t wifi_row_indices[REACH_WIFI_MAX_NETWORKS];
        size_t wifi_row_count;
        float wifi_content_height;
        reach_rect_f32 wifi_add_row;
        reach_rect_f32 wifi_add_clip;
        reach_rect_f32 wifi_add_name_field;
        reach_rect_f32 wifi_add_security_options[REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT];
        reach_rect_f32 wifi_add_key_field;
        reach_rect_f32 wifi_add_show_button;
        reach_rect_f32 wifi_add_auto_toggle;
        reach_rect_f32 wifi_add_submit_button;
        reach_rect_f32 wifi_key_field;
        reach_rect_f32 wifi_show_button;
        reach_rect_f32 wifi_auto_toggle;
        reach_rect_f32 wifi_connect_button;
        reach_rect_f32 wifi_disconnect_button;
        reach_rect_f32 wifi_forget_button;
        reach_rect_f32 bluetooth_radio_card;
        reach_rect_f32 bluetooth_radio_icon;
        reach_rect_f32 bluetooth_radio_title;
        reach_rect_f32 bluetooth_radio_subtitle;
        reach_rect_f32 bluetooth_radio_toggle;
        reach_rect_f32 bluetooth_scan_button;
        reach_rect_f32 bluetooth_loader_bar;
        reach_rect_f32 bluetooth_viewport;
        reach_rect_f32 bluetooth_scrollbar_track;
        reach_rect_f32 bluetooth_scrollbar_thumb;
        reach_rect_f32 bluetooth_rows[REACH_BLUETOOTH_MAX_DEVICES];
        size_t bluetooth_row_indices[REACH_BLUETOOTH_MAX_DEVICES];
        size_t bluetooth_row_count;
        float bluetooth_content_height;
        reach_rect_f32 bluetooth_section_titles[2];
        size_t bluetooth_section_ids[2];
        size_t bluetooth_section_count;
        reach_rect_f32 bluetooth_action_button;
        reach_rect_f32 bluetooth_pin_accept_button;
        reach_rect_f32 bluetooth_pin_reject_button;
        reach_settings_nav_item_layout nav_items[REACH_SETTINGS_NAV_ITEM_COUNT];
        size_t nav_item_count;
        reach_rect_f32 nav_footer;
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
        size_t startup_index;
        reach_config_theme_preference display_theme_preference;
        size_t wifi_index;
        size_t wifi_security_option;
        size_t bluetooth_index;
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
    int32_t reach_settings_model_tick_nav_selection(reach_settings_model *model,
                                                    double delta_seconds);
    int32_t reach_settings_model_nav_selection_active(const reach_settings_model *model);
    void reach_settings_model_begin_update_scan(reach_settings_model *model);
    void reach_settings_model_apply_update_scan(reach_settings_model *model,
                                                const reach_windows_update_list *updates,
                                                int32_t hresult);
    void reach_settings_model_begin_update_install(reach_settings_model *model);
    void reach_settings_model_begin_update_resume(reach_settings_model *model,
                                                  const reach_windows_update_journal *journal);
    void reach_settings_model_apply_update_operation(
        reach_settings_model *model, const reach_windows_update_operation_result *result);
    void reach_settings_model_toggle_update(reach_settings_model *model, size_t index);
    size_t reach_settings_model_selected_update_count(const reach_settings_model *model);
    size_t reach_settings_model_restart_required_count(const reach_settings_model *model);
    int32_t reach_settings_model_update_busy(const reach_settings_model *model);
    void reach_settings_model_scroll_updates(reach_settings_model *model, float delta);
    int32_t reach_settings_model_update_scroll(reach_settings_model *model, double delta_seconds);
    int32_t reach_settings_model_update_loader(reach_settings_model *model, double delta_seconds);

    void reach_settings_model_set_current_version(reach_settings_model *model,
                                                  const uint16_t *version);
    void reach_settings_model_begin_reach_check(reach_settings_model *model);
    void reach_settings_model_apply_reach_check(reach_settings_model *model,
                                                const reach_app_update_info *info, int32_t result);
    void reach_settings_model_begin_reach_download(reach_settings_model *model);
    void reach_settings_model_apply_reach_download(reach_settings_model *model, int32_t success);
    int32_t reach_settings_model_reach_update_action_enabled(const reach_settings_model *model);
    const uint16_t *reach_settings_model_reach_update_status(const reach_settings_model *model);
    const uint16_t *
    reach_settings_model_reach_update_button_label(const reach_settings_model *model);

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
    void reach_settings_model_apply_startup_apps(reach_settings_model *model,
                                                 const reach_startup_app_list *list);
    void reach_settings_model_set_startup_icon(reach_settings_model *model, size_t index,
                                               uint64_t icon_id);
    void reach_settings_model_set_startup_enabled(reach_settings_model *model, size_t index,
                                                  int32_t enabled);
    int32_t reach_settings_model_startup_enabled(const reach_settings_model *model, size_t index);
    void reach_settings_model_set_startup_busy(reach_settings_model *model, int32_t busy);
    void reach_settings_model_set_startup_status(reach_settings_model *model, int32_t status);
    const uint16_t *reach_settings_startup_status_message(int32_t status);
    void reach_settings_model_scroll_startup(reach_settings_model *model, float delta);
    int32_t reach_settings_model_startup_scroll(reach_settings_model *model, double delta_seconds);
    int32_t reach_settings_model_tick_startup_animations(reach_settings_model *model,
                                                         double delta_seconds);
    int32_t reach_settings_model_startup_animations_active(const reach_settings_model *model);

    void reach_settings_model_set_high_refresh_rate(reach_settings_model *model, int32_t enabled);
    int32_t reach_settings_model_high_refresh_rate(const reach_settings_model *model);
    int32_t reach_settings_model_toggle_high_refresh_rate(reach_settings_model *model);
    void reach_settings_model_set_bundled_font(reach_settings_model *model, int32_t enabled);
    int32_t reach_settings_model_bundled_font(const reach_settings_model *model);
    int32_t reach_settings_model_toggle_bundled_font(reach_settings_model *model);
    void reach_settings_model_set_light_theme(reach_settings_model *model, int32_t enabled);
    int32_t reach_settings_model_light_theme(const reach_settings_model *model);
    int32_t reach_settings_model_toggle_light_theme(reach_settings_model *model);
    void reach_settings_model_set_windows_system_theme(reach_settings_model *model,
                                                       reach_config_theme_preference preference);
    reach_config_theme_preference
    reach_settings_model_windows_system_theme(const reach_settings_model *model);
    int32_t
    reach_settings_model_select_windows_system_theme(reach_settings_model *model,
                                                     reach_config_theme_preference preference);
    void reach_settings_model_set_windows_app_theme(reach_settings_model *model,
                                                    reach_config_theme_preference preference);
    reach_config_theme_preference
    reach_settings_model_windows_app_theme(const reach_settings_model *model);
    int32_t reach_settings_model_select_windows_app_theme(reach_settings_model *model,
                                                          reach_config_theme_preference preference);
    int32_t reach_settings_model_tick_display_animations(reach_settings_model *model,
                                                         double delta_seconds);
    int32_t reach_settings_model_display_animations_active(const reach_settings_model *model);
    int32_t reach_settings_model_power_dirty(const reach_settings_model *model);
    void reach_settings_model_power_mark_applied(reach_settings_model *model);
    int32_t reach_settings_model_tick_power_caret(reach_settings_model *model,
                                                  double delta_seconds);

    void reach_settings_model_set_account(reach_settings_model *model, const uint16_t *display_name,
                                          const uint16_t *user_name, int32_t is_admin,
                                          uint64_t picture);
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

    float reach_settings_model_button_press_value(const reach_settings_model *model,
                                                  int32_t hit_type);
    int32_t reach_settings_model_tick_button_press(reach_settings_model *model,
                                                   double delta_seconds);
    int32_t reach_settings_model_button_press_active(const reach_settings_model *model);
    int32_t reach_settings_model_set_hovered_button(reach_settings_model *model, int32_t hit_type);

    int32_t
    reach_windows_update_matches_security_maintenance(const reach_windows_update_item *update);
    void reach_windows_update_apply_default_selection(reach_windows_update_list *updates);
    const uint16_t *reach_windows_update_state_label(reach_windows_update_state state);

    void reach_settings_model_apply_wifi(reach_settings_model *model, reach_wifi_radio_state radio,
                                         const reach_wifi_network_list *networks);
    void reach_settings_model_set_wifi_radio(reach_settings_model *model,
                                             reach_wifi_radio_state radio);
    int32_t reach_settings_model_toggle_wifi_radio(reach_settings_model *model);
    void reach_settings_model_set_wifi_view(reach_settings_model *model,
                                            reach_settings_wifi_view view);
    void reach_settings_model_set_wifi_status(reach_settings_model *model, int32_t status,
                                              const uint16_t *ssid);
    const uint16_t *reach_settings_wifi_status_message(int32_t status);
    int32_t reach_settings_model_wifi_busy(const reach_settings_model *model);
    int32_t reach_settings_model_wifi_row_visible(const reach_settings_model *model, size_t index);
    void reach_settings_model_wifi_expand_row(reach_settings_model *model, int32_t row);
    void reach_settings_model_wifi_focus_field(reach_settings_model *model, int32_t field);
    void reach_settings_model_wifi_blur(reach_settings_model *model);
    int32_t reach_settings_model_wifi_toggle_show_key(reach_settings_model *model);
    int32_t reach_settings_model_wifi_toggle_auto(reach_settings_model *model);
    void reach_settings_model_wifi_select_security(reach_settings_model *model, size_t option);
    reach_wifi_security reach_settings_wifi_security_option(size_t option);
    const uint16_t *reach_settings_wifi_security_option_label(size_t option);
    int32_t reach_settings_model_wifi_insert_char(reach_settings_model *model, uint16_t ch);
    int32_t reach_settings_model_wifi_handle_edit_key(reach_settings_model *model,
                                                      reach_text_edit_key key,
                                                      reach_text_edit_modifiers modifiers);
    int32_t reach_settings_model_wifi_connect_ready(const reach_settings_model *model);
    int32_t reach_settings_model_wifi_add_ready(const reach_settings_model *model);
    int32_t reach_settings_model_wifi_build_connect(const reach_settings_model *model, size_t index,
                                                    reach_wifi_connect_request *out_request);
    int32_t reach_settings_model_wifi_build_add(const reach_settings_model *model,
                                                reach_wifi_connect_request *out_request);
    void reach_settings_model_wifi_clear_secrets(reach_settings_model *model);
    void reach_settings_model_scroll_wifi(reach_settings_model *model, float delta);
    int32_t reach_settings_model_wifi_scroll(reach_settings_model *model, double delta_seconds);
    int32_t reach_settings_model_wifi_loader(reach_settings_model *model, double delta_seconds);
    int32_t reach_settings_model_tick_wifi_animations(reach_settings_model *model,
                                                      double delta_seconds);
    int32_t reach_settings_model_wifi_animations_active(const reach_settings_model *model);
    int32_t reach_settings_model_tick_wifi_caret(reach_settings_model *model, double delta_seconds);

    void reach_settings_model_apply_bluetooth(reach_settings_model *model,
                                              const reach_bluetooth_device_list *devices,
                                              const reach_bluetooth_pairing_request *pairing,
                                              int32_t scanning);
    void reach_settings_model_set_bluetooth_radio(reach_settings_model *model,
                                                  reach_bluetooth_state state);
    int32_t reach_settings_model_toggle_bluetooth_radio(reach_settings_model *model);
    void reach_settings_model_set_bluetooth_status(reach_settings_model *model, int32_t status,
                                                   const uint16_t *device_id);
    const uint16_t *reach_settings_bluetooth_status_message(int32_t status);
    void reach_settings_model_set_bluetooth_icon(reach_settings_model *model, size_t index,
                                                 uint64_t icon_id);
    void reach_settings_model_bluetooth_expand_row(reach_settings_model *model, int32_t row);
    void reach_settings_model_scroll_bluetooth(reach_settings_model *model, float delta);
    int32_t reach_settings_model_bluetooth_scroll(reach_settings_model *model,
                                                  double delta_seconds);
    int32_t reach_settings_model_bluetooth_loader(reach_settings_model *model,
                                                  double delta_seconds);
    int32_t reach_settings_model_tick_bluetooth_animations(reach_settings_model *model,
                                                           double delta_seconds);
    int32_t reach_settings_model_bluetooth_animations_active(const reach_settings_model *model);

    void reach_settings_layout_wifi(reach_settings_layout *layout, reach_settings_model *model,
                                    float scale);
    void reach_settings_layout_bluetooth(reach_settings_layout *layout, reach_settings_model *model,
                                         float scale);

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
