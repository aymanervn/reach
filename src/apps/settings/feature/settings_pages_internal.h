#ifndef REACH_FEATURES_SETTINGS_PAGES_INTERNAL_H
#define REACH_FEATURES_SETTINGS_PAGES_INTERNAL_H

#include "reach/apps/settings/settings.h"

#include "reach/core/theme.h"
#include "reach/support/motion.h"

static inline void reach_settings_scroll_init(reach_scroll_view *view)
{
    reach_scroll_view_config config = {};
    config.unit = 1.0f;
    reach_scroll_view_init(view, &config);
}

static inline void reach_settings_scroll_to_top(reach_scroll_view *view)
{
    (void)reach_scroll_view_set_position(view, 0.0f);
    (void)reach_scroll_view_sync(view, nullptr, REACH_SCROLL_VIEW_SET, reach_motion_now_seconds());
}

static inline void reach_settings_layout_scroll(reach_scroll_view *view, reach_rect_f32 viewport,
                                                reach_rect_f32 track, float content_height,
                                                float minimum_thumb, double now_seconds,
                                                reach_rect_f32 *out_thumb)
{
    reach_scroll_view_config config = view->config;
    config.minimum_thumb = minimum_thumb;
    reach_scroll_view_configure(view, &config);
    reach_scroll_view_set_extents(view, content_height, viewport.height);
    reach_scroll_view_set_frame(view, viewport, track);
    (void)reach_scroll_view_sync(view, nullptr, REACH_SCROLL_VIEW_WHEEL, now_seconds);
    *out_thumb = reach_scroll_view_thumb_at(view, now_seconds);
}

static inline reach_motion_spec reach_settings_motion_preset(reach_motion_preset preset)
{
    return reach_theme_motion_preset(reach_theme_default(), preset);
}

static inline void reach_settings_animate(reach_motion *motion, float target,
                                          reach_motion_preset preset)
{
    reach_motion_animate_spec_to_at(motion, target, reach_settings_motion_preset(preset),
                                    reach_motion_now_seconds());
}

static inline int32_t reach_settings_advance_motions(reach_motion *motions, size_t count,
                                                     double now_seconds)
{
    int32_t pending = 0;
    for (size_t index = 0; index < count; ++index)
        pending |= reach_motion_advance_at(&motions[index], now_seconds);
    return pending;
}

static inline int32_t reach_settings_motions_pending(const reach_motion *motions, size_t count)
{
    for (size_t index = 0; index < count; ++index)
        if (reach_motion_pending(&motions[index]))
            return 1;
    return 0;
}

const uint16_t *reach_settings_wifi_page_title(void);
const uint16_t *reach_settings_bluetooth_page_title(void);
const uint16_t *reach_settings_account_page_title(void);
const uint16_t *reach_settings_startup_apps_page_title(void);
const uint16_t *reach_settings_power_sleep_page_title(void);
const uint16_t *reach_settings_display_page_title(void);
const uint16_t *reach_settings_update_page_title(void);

const uint16_t *reach_settings_wifi_page_placeholder(void);
const uint16_t *reach_settings_bluetooth_page_placeholder(void);
const uint16_t *reach_settings_account_page_placeholder(void);
const uint16_t *reach_settings_startup_apps_page_placeholder(void);
const uint16_t *reach_settings_installed_apps_page_title(void);
const uint16_t *reach_settings_installed_apps_page_placeholder(void);
const uint16_t *reach_settings_power_sleep_page_placeholder(void);
const uint16_t *reach_settings_display_page_placeholder(void);

typedef struct reach_settings_power_row_style
{
    uint32_t icon_id;
    const uint16_t *title;
    const uint16_t *subtitle;
    reach_theme_accent accent;
} reach_settings_power_row_style;

const reach_settings_power_row_style *reach_settings_power_row_styles(void);

int32_t reach_settings_wifi_row_needs_key_form(const reach_settings_model *model, size_t index);
int32_t reach_settings_bluetooth_row_is_pairing(const reach_settings_model *model, size_t index);
#endif
