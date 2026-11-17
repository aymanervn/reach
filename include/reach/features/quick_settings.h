#ifndef REACH_FEATURES_QUICK_SETTINGS_H
#define REACH_FEATURES_QUICK_SETTINGS_H

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/wifi.h"
#include "reach/core/blink.h"
#include "reach/core/bluetooth.h"
#include "reach/features/common/keyed_rows.h"
#include "reach/features/common/scroll_view.h"
#include "reach/features/common/text_edit.h"
#include "reach/features/feature_capsule.h"
#include "reach/ports/audio_volume.h"
#include "reach/ports/power_session.h"
#include "reach/ports/system_controls.h"
#include "reach/ports/text_measure.h"
#include "reach/services/bluetooth.h"
#include "reach/services/profile.h"
#include "reach/services/system_status.h"
#include "reach/services/wifi.h"
#include "reach/support/util.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_quick_settings_page
    {
        REACH_QUICK_SETTINGS_PAGE_OVERVIEW = 0,
        REACH_QUICK_SETTINGS_PAGE_SOUND,
        REACH_QUICK_SETTINGS_PAGE_WIFI,
        REACH_QUICK_SETTINGS_PAGE_BLUETOOTH,
        REACH_QUICK_SETTINGS_PAGE_COUNT
    } reach_quick_settings_page;

    typedef enum reach_quick_settings_toggle
    {
        REACH_QUICK_SETTINGS_TOGGLE_WIFI = 0,
        REACH_QUICK_SETTINGS_TOGGLE_BLUETOOTH,
        REACH_QUICK_SETTINGS_TOGGLE_DARK_MODE,
        REACH_QUICK_SETTINGS_TOGGLE_PROJECT,
        REACH_QUICK_SETTINGS_TOGGLE_COUNT
    } reach_quick_settings_toggle;

#define REACH_QUICK_SETTINGS_WIFI_ROWS 32
#define REACH_QUICK_SETTINGS_BLUETOOTH_ROWS 32

    typedef struct reach_quick_settings_model
    {
        reach_quick_settings_page page;
        float main_volume_level;
        int32_t main_muted;
        reach_audio_volume_state input;
        int32_t input_available;
        reach_audio_volume_session_list sessions;
        reach_audio_output_device_list output_devices;
        reach_network_state network;
        reach_bluetooth_state bluetooth;
        int32_t bluetooth_pending;
        int32_t bluetooth_pending_enabled;
        reach_brightness_state brightness;
        reach_wifi_radio_state wifi_radio;
        reach_wifi_network wifi_rows[REACH_QUICK_SETTINGS_WIFI_ROWS];
        size_t wifi_row_count;
        size_t wifi_connected_count;
        int32_t wifi_status;
        uint16_t wifi_status_ssid[REACH_WIFI_SSID_CAPACITY];
        int32_t wifi_key_row;
        reach_text_edit wifi_key;
        reach_bluetooth_device bluetooth_rows[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS];
        size_t bluetooth_row_count;
        size_t bluetooth_paired_count;
        int32_t bluetooth_status;
        uint16_t bluetooth_status_device[REACH_BLUETOOTH_DEVICE_ID_CAPACITY];
        int32_t bluetooth_action_row;
        int32_t bluetooth_pin_active;
        uint16_t bluetooth_pin[REACH_BLUETOOTH_PIN_CAPACITY];
        int32_t bluetooth_scanning;
        int32_t light_theme;
        uint16_t display_name[REACH_USER_ACCOUNT_NAME_CAPACITY];
        uint64_t picture_image_id;
        int64_t uptime_minutes;
        reach_power_state power;
    } reach_quick_settings_model;

    typedef struct reach_quick_settings_level_pill_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 icon;
        reach_rect_f32 label;
        reach_rect_f32 value;
    } reach_quick_settings_level_pill_layout;

    typedef struct reach_quick_settings_output_device_row_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 icon;
        reach_rect_f32 title;
        reach_rect_f32 detail;
        reach_rect_f32 checkmark;
    } reach_quick_settings_output_device_row_layout;

    typedef struct reach_quick_settings_toggle_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 icon;
        reach_rect_f32 divider;
        reach_rect_f32 title;
        reach_rect_f32 status;
    } reach_quick_settings_toggle_layout;

    typedef struct reach_quick_settings_layout
    {
        reach_rect_f32 content_bounds;
        reach_rect_f32 interior;
        reach_rect_f32 page_bounds;
        float list_top[REACH_QUICK_SETTINGS_PAGE_COUNT];
        reach_rect_f32 list_viewport;

        reach_rect_f32 header;
        reach_rect_f32 settings_button;
        reach_rect_f32 avatar_ring;
        reach_rect_f32 avatar;
        reach_rect_f32 identity_name;
        reach_rect_f32 identity_detail;

        reach_rect_f32 rail;
        reach_rect_f32 rail_items[REACH_QUICK_SETTINGS_PAGE_COUNT];

        reach_quick_settings_level_pill_layout overview_volume;
        reach_rect_f32 sound_page_button;
        reach_quick_settings_level_pill_layout overview_brightness;
        reach_quick_settings_toggle_layout toggles[REACH_QUICK_SETTINGS_TOGGLE_COUNT];

        reach_quick_settings_level_pill_layout sound_volume;

        reach_rect_f32 output_label;
        reach_rect_f32 output_devices_panel;
        reach_rect_f32 output_devices_clip;
        reach_quick_settings_output_device_row_layout
            output_device_rows[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
        size_t output_device_row_count;

        reach_rect_f32 app_label;
        reach_rect_f32 app_volumes_panel;
        reach_rect_f32 app_volumes_clip;
        reach_quick_settings_level_pill_layout app_volume_rows[REACH_AUDIO_VOLUME_MAX_SESSIONS];
        size_t app_volume_row_count;

        reach_rect_f32 input_label;
        reach_quick_settings_level_pill_layout input_volume;

        reach_rect_f32 wifi_title;
        reach_rect_f32 wifi_switch;
        reach_rect_f32 wifi_message;
        reach_rect_f32 wifi_nearby_label;
        reach_quick_settings_output_device_row_layout wifi_rows[REACH_QUICK_SETTINGS_WIFI_ROWS];
        reach_rect_f32 wifi_forget_buttons[REACH_QUICK_SETTINGS_WIFI_ROWS];
        size_t wifi_row_count;
        reach_rect_f32 wifi_key_field;
        reach_rect_f32 wifi_key_connect;

        reach_rect_f32 bluetooth_title;
        reach_rect_f32 bluetooth_switch;
        reach_rect_f32 bluetooth_message;
        reach_rect_f32 bluetooth_mine_label;
        reach_rect_f32 bluetooth_nearby_label;
        reach_quick_settings_output_device_row_layout
            bluetooth_rows[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS];
        size_t bluetooth_row_count;
        reach_rect_f32 bluetooth_action_label;
        reach_rect_f32 bluetooth_action_buttons[2];
    } reach_quick_settings_layout;

    typedef struct reach_quick_settings_output_device_row
    {
        reach_audio_output_device device;
    } reach_quick_settings_output_device_row;

    typedef struct reach_quick_settings_app_volume_row
    {
        reach_audio_volume_session session;
    } reach_quick_settings_app_volume_row;

    typedef struct reach_quick_settings_render_input
    {
        reach_quick_settings_model model;
        reach_quick_settings_layout layout;
        reach_theme theme;
        float dpi_scale;
        reach_motion output_devices_span;
        reach_motion app_volumes_span;
        reach_keyed_row output_device_row_motions[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
        reach_quick_settings_output_device_row
            output_device_row_items[REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES];
        reach_keyed_row app_volume_row_motions[REACH_AUDIO_VOLUME_MAX_SESSIONS];
        reach_quick_settings_app_volume_row app_volume_row_items[REACH_AUDIO_VOLUME_MAX_SESSIONS];
        reach_keyed_row wifi_row_motions[REACH_QUICK_SETTINGS_WIFI_ROWS];
        reach_wifi_network wifi_row_items[REACH_QUICK_SETTINGS_WIFI_ROWS];
        reach_motion wifi_switch;
        reach_blink wifi_caret;
        reach_keyed_row bluetooth_row_motions[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS];
        reach_bluetooth_device bluetooth_row_items[REACH_QUICK_SETTINGS_BLUETOOTH_ROWS];
        reach_motion bluetooth_switch;
        reach_motion toggle_states[REACH_QUICK_SETTINGS_TOGGLE_COUNT];
        reach_motion page_opacity[REACH_QUICK_SETTINGS_PAGE_COUNT];
        reach_motion page_shift[REACH_QUICK_SETTINGS_PAGE_COUNT];
        reach_motion rail_marker;
        reach_rect_motion surface_motion;
        reach_scroll_view scroll;
        reach_rect_f32 render_bounds;
        reach_rect_f32 viewport;
        int32_t has_background;
        float notch_center_x;
        int32_t notch_side;
        reach_text_measure_port text_measure;
        uint64_t press_feedback_target;
        reach_motion press_feedback;
    } reach_quick_settings_render_input;

    void reach_quick_settings_model_init(reach_quick_settings_model *model);

    void reach_quick_settings_model_set_main_volume(reach_quick_settings_model *model,
                                                    float volume_level, int32_t muted);

    void reach_quick_settings_model_set_sessions(reach_quick_settings_model *model,
                                                 const reach_audio_volume_session_list *sessions);

    void
    reach_quick_settings_model_set_output_devices(reach_quick_settings_model *model,
                                                  const reach_audio_output_device_list *devices);

    void reach_quick_settings_model_set_system_states(reach_quick_settings_model *model,
                                                      const reach_network_state *network,
                                                      const reach_bluetooth_state *bluetooth,
                                                      const reach_brightness_state *brightness);

    uint32_t reach_quick_settings_volume_icon_id(float volume_level, int32_t muted);

    reach_quick_settings_layout
    reach_quick_settings_layout_for_content_bounds(reach_rect_f32 content_bounds,
                                                   const reach_theme *theme,
                                                   const reach_quick_settings_model *model);
    reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds_scaled(
        reach_rect_f32 content_bounds, const reach_theme *theme,
        const reach_quick_settings_model *model, float dpi_scale);

    float reach_quick_settings_content_height_for_model(const reach_quick_settings_model *model);
    float
    reach_quick_settings_content_height_for_model_scaled(const reach_quick_settings_model *model,
                                                         float dpi_scale);

    reach_result
    reach_quick_settings_build_render_commands(const reach_quick_settings_render_input *input,
                                               reach_render_command_buffer *commands);

    typedef struct reach_quick_settings reach_quick_settings;

    reach_result reach_quick_settings_create(reach_quick_settings **out_quick_settings);
    void reach_quick_settings_destroy(reach_quick_settings *quick_settings);
    typedef struct reach_quick_settings_drag_state
    {
        int32_t active;
        int32_t type;
        float last_level;
        int32_t level_valid;
        size_t session_index;
        uint16_t session_instance_id[REACH_AUDIO_VOLUME_SESSION_KEY_CAPACITY];
    } reach_quick_settings_drag_state;

    typedef struct reach_quick_settings_state
    {
        reach_quick_settings_model model;
        reach_quick_settings_layout layout;
        int32_t open;
        reach_rect_f32 bounds;
        reach_rect_f32 target_bounds;
        reach_rect_f32 content_bounds;
        reach_motion output_devices_span;
        reach_motion app_volumes_span;
        reach_rect_motion surface_motion;
        reach_scroll_view scroll;
        reach_rect_f32 render_bounds;
        float notch_anchor_x;
        int32_t drop_direction;
        reach_quick_settings_drag_state drag;
    } reach_quick_settings_state;

    const reach_quick_settings_state *
    reach_quick_settings_state_ptr(reach_quick_settings *quick_settings);

    const reach_feature_capsule_ops *reach_quick_settings_capsule_ops(void);

    int32_t reach_quick_settings_is_open(const reach_quick_settings *quick_settings);
    int32_t reach_quick_settings_set_open(reach_quick_settings *quick_settings, int32_t open);
    void reach_quick_settings_force_close(reach_quick_settings *quick_settings);

    void reach_quick_settings_apply_main_volume(reach_quick_settings *quick_settings, float level,
                                                int32_t muted);
    void reach_quick_settings_apply_input(reach_quick_settings *quick_settings,
                                          const reach_audio_volume_state *input);
    int32_t reach_quick_settings_apply_wifi_snapshot(reach_quick_settings *quick_settings,
                                                     const reach_wifi_snapshot *snapshot);
    int32_t reach_quick_settings_apply_bluetooth_snapshot(reach_quick_settings *quick_settings,
                                                          const reach_bluetooth_snapshot *snapshot);
    void reach_quick_settings_apply_sessions(reach_quick_settings *quick_settings,
                                             const reach_audio_volume_session_list *sessions);
    void reach_quick_settings_apply_output_devices(reach_quick_settings *quick_settings,
                                                   const reach_audio_output_device_list *devices);

    reach_result reach_quick_settings_append_render_commands(
        reach_quick_settings *quick_settings, const reach_theme *theme, float dpi_scale,
        const reach_text_measure_port *text_measure, reach_render_command_buffer *out_commands);

    typedef struct reach_quick_settings_layout_context
    {
        const reach_theme *theme;
        float dpi_scale;
        reach_rect_f32 anchor_button;
        reach_rect_f32 monitor;
        float bar_edge_y;
        int32_t drop_direction;
    } reach_quick_settings_layout_context;

    void reach_quick_settings_refresh_layout(reach_quick_settings *quick_settings,
                                             const reach_quick_settings_layout_context *ctx);

    void reach_quick_settings_relayout(reach_quick_settings *quick_settings,
                                       const reach_quick_settings_layout_context *ctx,
                                       int32_t animate_height);

    int32_t reach_quick_settings_bluetooth_available(reach_quick_settings *quick_settings);
    int32_t reach_quick_settings_bluetooth_enabled(reach_quick_settings *quick_settings);
    void reach_quick_settings_set_bluetooth_pending(reach_quick_settings *quick_settings,
                                                    int32_t pending, int32_t pending_enabled);

    const uint16_t *reach_quick_settings_set_session_level(reach_quick_settings *quick_settings,
                                                           size_t session_index, float level);
    const uint16_t *
    reach_quick_settings_output_device_id(const reach_quick_settings *quick_settings,
                                          size_t output_device_index);

    typedef struct reach_quick_settings_system_apply_result
    {
        int32_t relayout;
    } reach_quick_settings_system_apply_result;

    void reach_quick_settings_apply_system_states(reach_quick_settings *quick_settings,
                                                  const reach_network_state *network,
                                                  const reach_bluetooth_state *bluetooth,
                                                  const reach_brightness_state *brightness,
                                                  int32_t bluetooth_valid, uint32_t change_flags,
                                                  reach_quick_settings_system_apply_result *out);

    typedef struct reach_quick_settings_routes
    {
        void *user;
        void (*set_light_theme)(void *user, int32_t light_theme);
    } reach_quick_settings_routes;

    void reach_quick_settings_attach_services(reach_quick_settings *quick_settings,
                                              reach_system_status *status, reach_wifi_service *wifi,
                                              reach_bluetooth_service *bluetooth,
                                              reach_profile_service *profile,
                                              const reach_power_session_port *power_session);
    void reach_quick_settings_set_routes(reach_quick_settings *quick_settings,
                                         const reach_quick_settings_routes *routes);
    int32_t reach_quick_settings_apply_config(reach_quick_settings *quick_settings,
                                              int32_t light_theme);
    int32_t reach_quick_settings_select_page(reach_quick_settings *quick_settings,
                                             reach_quick_settings_page page);
    const reach_ui_event_type *reach_quick_settings_routed_events(size_t *out_count);

    void reach_quick_settings_refresh_audio(reach_quick_settings *quick_settings);
    void reach_quick_settings_refresh_system(reach_quick_settings *quick_settings,
                                             uint32_t change_flags);

    void reach_quick_settings_process_changes(reach_quick_settings *quick_settings,
                                              reach_feature_tick_result *out);
    size_t reach_quick_settings_take_retired_render_icons(reach_quick_settings *quick_settings,
                                                          uint64_t *out_ids, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
