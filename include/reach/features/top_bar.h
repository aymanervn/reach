#ifndef REACH_FEATURES_TOP_BAR_H
#define REACH_FEATURES_TOP_BAR_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/features/common/bar_visibility.h"
#include "reach/features/common/draggable.h"
#include "reach/features/common/pressable.h"
#include "reach/features/feature_capsule.h"
#include "reach/features/popup.h"
#include "reach/ports/text_measure.h"
#include "reach/services/app_control.h"
#include "reach/services/icon_service.h"
#include "reach/services/now_playing.h"
#include "reach/services/clock.h"
#include "reach/services/input_language.h"
#include "reach/services/system_stats.h"
#include "reach/services/system_status.h"
#include "reach/services/tray.h"
#include "reach/services/window_tracking.h"
#include "reach/support/animation.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_top_bar_pill
    {
        REACH_TOP_BAR_PILL_POWER_CLOCK = 0,
        REACH_TOP_BAR_PILL_CURRENT_APP = 1,
        REACH_TOP_BAR_PILL_TRAY = 2,
        REACH_TOP_BAR_PILL_QUICK_SETTINGS = 3,
        REACH_TOP_BAR_PILL_COUNT = 4
    } reach_top_bar_pill;

// Every icon stays inline until the tray reaches the overflow threshold; past that the bar keeps
// REACH_TOP_BAR_TRAY_INLINE_ICONS inline and moves the rest into the dropdown. The dropdown button
// takes the cell the hidden icons vacate, so the pill never spans more than MAX_TRAY_ICONS cells.
#define REACH_TOP_BAR_MAX_TRAY_ICONS 5
#define REACH_TOP_BAR_TRAY_OVERFLOW_THRESHOLD 6
#define REACH_TOP_BAR_TRAY_INLINE_ICONS 4

    typedef struct reach_top_bar_tray_item
    {
        uint32_t id;
        uint64_t icon_id;
    } reach_top_bar_tray_item;

    typedef struct reach_top_bar_layout
    {
        reach_rect_f32 bounds;
        reach_rect_f32 pills[REACH_TOP_BAR_PILL_COUNT];
        int32_t pill_visible[REACH_TOP_BAR_PILL_COUNT];
        reach_rect_f32 power_button;
        reach_rect_f32 clock_time;
        reach_rect_f32 clock_date;
        reach_rect_f32 now_playing_separator;
        reach_rect_f32 now_playing;
        reach_rect_f32 current_app_icon;
        reach_rect_f32 current_app_text;
        reach_rect_f32 tray_background;
        reach_rect_f32 tray_icons[REACH_TOP_BAR_MAX_TRAY_ICONS];
        size_t tray_icon_count;
        reach_rect_f32 tray_overflow_button;
        reach_rect_f32 tray_separator;
        reach_rect_f32 quick_settings_button;
        reach_rect_f32 network_icon;
        reach_rect_f32 network_label;
        reach_rect_f32 bluetooth_icon;
        reach_rect_f32 volume_label;
        reach_rect_f32 settings_button;
        reach_rect_f32 language_button;
        reach_rect_f32 battery_button;
        reach_rect_f32 battery_shell;
        reach_rect_f32 stats_cpu;
        reach_rect_f32 stats_memory;
        reach_rect_f32 stats_download;
        reach_rect_f32 stats_upload;
    } reach_top_bar_layout;

    enum reach_top_bar_animation_id
    {
        REACH_TOP_BAR_ANIM_Y = 0,
        REACH_TOP_BAR_ANIM_POWER_HOVER,
        REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY,
        REACH_TOP_BAR_ANIM_TRAY_DRAG_SNAP,
        REACH_TOP_BAR_ANIM_NOW_PLAYING_WIDTH,
        REACH_TOP_BAR_ANIM_CURRENT_APP_WIDTH,
        REACH_TOP_BAR_ANIM_TRAY_WIDTH,
        REACH_TOP_BAR_ANIM_QUICK_SETTINGS_WIDTH,
        REACH_TOP_BAR_ANIM_TRAY_ITEM_X_BASE,
        REACH_TOP_BAR_ANIM_COUNT =
            REACH_TOP_BAR_ANIM_TRAY_ITEM_X_BASE + REACH_TOP_BAR_MAX_TRAY_ICONS
    };

    static inline size_t reach_top_bar_tray_item_x_animation_id(size_t index)
    {
        return REACH_TOP_BAR_ANIM_TRAY_ITEM_X_BASE + index;
    }

    enum reach_top_bar_feedback_slot
    {
        REACH_TOP_BAR_FEEDBACK_POWER_BUTTON = 0,
        REACH_TOP_BAR_FEEDBACK_TRAY_BASE,
        REACH_TOP_BAR_FEEDBACK_TRAY_OVERFLOW =
            REACH_TOP_BAR_FEEDBACK_TRAY_BASE + REACH_TOP_BAR_MAX_TRAY_ICONS,
        REACH_TOP_BAR_FEEDBACK_QUICK_SETTINGS_BUTTON,
        REACH_TOP_BAR_FEEDBACK_SETTINGS_BUTTON,
        REACH_TOP_BAR_FEEDBACK_LANGUAGE_BUTTON,
        REACH_TOP_BAR_FEEDBACK_BATTERY_BUTTON
    };

    typedef enum reach_top_bar_pointer_region
    {
        REACH_TOP_BAR_POINTER_REGION_NONE = 0,
        REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON = 1,
        REACH_TOP_BAR_POINTER_REGION_NOW_PLAYING = 2,
        REACH_TOP_BAR_POINTER_REGION_TRAY_ICON = 3,
        REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW = 4,
        REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON = 5,
        REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON = 6,
        REACH_TOP_BAR_POINTER_REGION_SETTINGS_BUTTON = 7,
        REACH_TOP_BAR_POINTER_REGION_BATTERY_BUTTON = 8
    } reach_top_bar_pointer_region;

    typedef enum reach_top_bar_control_slot
    {
        REACH_TOP_BAR_CONTROL_NONE = 0,
        REACH_TOP_BAR_CONTROL_TRAY = 1,
        REACH_TOP_BAR_CONTROL_QUICK_SETTINGS = 2,
        REACH_TOP_BAR_CONTROL_BATTERY = 3,
        REACH_TOP_BAR_CONTROL_POWER = 4
    } reach_top_bar_control_slot;

    typedef struct reach_top_bar_state
    {
        reach_top_bar_layout layout;
        reach_bar_visibility_state visibility;
        reach_pressable pressable;
        int32_t power_hovered;

        uint16_t clock_time_text[32];
        uint16_t clock_date_text[64];
        int32_t clock_initialized;

        uint16_t current_app_name[260];
        uint16_t current_app_icon_ref[260];

        uint16_t language_code[8];

        uint32_t network_icon_id;
        uint32_t bluetooth_icon_id;
        int32_t network_connected;
        int32_t bluetooth_enabled;
        double bluetooth_absent_seconds;
        int32_t volume_valid;
        int32_t volume_muted;
        uint16_t volume_text[8];
        uint16_t network_name[REACH_SYSTEM_NETWORK_LABEL_CAPACITY];

        uint16_t stats_cpu_text[16];
        uint16_t stats_memory_text[16];
        uint16_t stats_download_text[16];
        uint16_t stats_upload_text[16];
        int32_t stats_valid;
        int32_t battery_valid;
        int32_t battery_percent;
        int32_t battery_saver_on;
        int32_t battery_charging;
        int32_t battery_saver_pending;
        int32_t battery_saver_pending_enabled;

        reach_top_bar_tray_item tray_items[REACH_TOP_BAR_MAX_TRAY_ICONS];
        size_t tray_item_count;
        int32_t tray_overflow;
        int32_t tray_popup_open;
    } reach_top_bar_state;

    typedef struct reach_top_bar reach_top_bar;

    reach_result reach_top_bar_create(reach_top_bar **out_top_bar);
    void reach_top_bar_destroy(reach_top_bar *top_bar);

    void reach_top_bar_attach_services(reach_top_bar *top_bar,
                                       reach_now_playing_service *now_playing,
                                       reach_icon_service *icons, reach_window_tracking *windows,
                                       reach_system_stats *stats, reach_clock *clock,
                                       reach_input_language_service *input_language,
                                       reach_tray_service *tray);

    void reach_top_bar_attach_app_control(reach_top_bar *top_bar, reach_app_control *apps);

    void reach_top_bar_invalidate_occlusion(reach_top_bar *top_bar);

    void reach_top_bar_attach_status(reach_top_bar *top_bar, reach_system_status *status);

    typedef struct reach_top_bar_routes
    {
        void *user;

        void (*power_activated)(void *user);
        void (*quick_settings_activated)(void *user);
        void (*battery_activated)(void *user);
        void (*tray_overflow_activated)(void *user);
    } reach_top_bar_routes;

    void reach_top_bar_set_routes(reach_top_bar *top_bar, const reach_top_bar_routes *routes);

    const reach_feature_capsule_ops *reach_top_bar_capsule_ops(void);
    const reach_top_bar_state *reach_top_bar_state_ptr(const reach_top_bar *top_bar);
    reach_animation_manager *reach_top_bar_manager(reach_top_bar *top_bar);

    typedef struct reach_top_bar_build_context
    {
        const reach_theme *theme;
        reach_rect_f32 monitor_bounds;
        float dpi_scale;
        reach_text_measure_port text_measure;
    } reach_top_bar_build_context;

    void reach_top_bar_build_layout(reach_top_bar *top_bar, const reach_top_bar_build_context *ctx);

    reach_point_i32 reach_top_bar_local_point(const reach_top_bar_layout *layout, int32_t x,
                                              int32_t y);
    reach_rect_f32 reach_top_bar_rect_to_screen(const reach_top_bar_layout *layout,
                                                reach_rect_f32 rect);

    reach_top_bar_pointer_region reach_top_bar_pointer_region_at(const reach_top_bar *top_bar,
                                                                 int32_t local_x, int32_t local_y);
    void reach_top_bar_set_battery_saver_pending(reach_top_bar *top_bar, int32_t pending,
                                                 int32_t pending_enabled);

    size_t reach_top_bar_tray_overflow_start(const reach_top_bar *top_bar);

    const reach_feature_capsule_ops *reach_top_bar_tray_capsule_ops(void);
    int32_t reach_top_bar_tray_popup_is_open(const reach_top_bar *top_bar);
    int32_t reach_top_bar_set_tray_popup_open(reach_top_bar *top_bar, int32_t open);
    reach_result reach_top_bar_refresh_tray(reach_top_bar *top_bar);
    void reach_top_bar_layout_tray_popup(reach_top_bar *top_bar, const reach_theme *theme,
                                         const reach_popup_anchor *anchor, float dpi_scale,
                                         reach_rect_f32 *out_bounds);
    reach_animation_manager *reach_top_bar_tray_animation_manager(reach_top_bar *top_bar);
    size_t reach_top_bar_tray_item_count(const reach_top_bar *top_bar);
    uint64_t reach_top_bar_tray_item_icon_id(const reach_top_bar *top_bar, size_t index);

    typedef struct reach_top_bar_tray_render_context
    {
        const reach_theme *theme;
        reach_rect_f32 bounds;
        float dpi_scale;
    } reach_top_bar_tray_render_context;

    reach_result
    reach_top_bar_append_tray_render_commands(reach_top_bar *top_bar,
                                              const reach_top_bar_tray_render_context *ctx,
                                              reach_render_command_buffer *out_commands);

    const reach_bar_reveal_ops *reach_top_bar_reveal_ops(void);

    typedef struct reach_top_bar_render_context
    {
        const reach_theme *theme;
        float dpi_scale;
        int32_t icon_size_px;
    } reach_top_bar_render_context;

    reach_result reach_top_bar_append_render_commands(reach_top_bar *top_bar,
                                                      const reach_top_bar_render_context *ctx,
                                                      reach_render_command_buffer *out_commands);

#ifdef __cplusplus
}
#endif

#endif
