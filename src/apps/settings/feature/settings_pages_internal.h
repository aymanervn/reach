#ifndef REACH_FEATURES_SETTINGS_PAGES_INTERNAL_H
#define REACH_FEATURES_SETTINGS_PAGES_INTERNAL_H

#include "reach/apps/settings/settings.h"

#include "reach/core/theme.h"
#include "reach/support/animation.h"
#include "reach/support/motion.h"

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

static inline void reach_settings_track_to(reach_animation_manager *manager, size_t track,
                                           float target, reach_motion_preset preset)
{
    reach_motion_spec spec = reach_settings_motion_preset(preset);
    double now = reach_motion_now_seconds();
    if (spec.easing == REACH_EASING_SPRING)
        reach_animation_manager_spring_to_at(manager, track, target, spec.response_seconds,
                                             spec.damping, now);
    else
        reach_animation_manager_animate_to_at(manager, track, target, spec.duration_seconds,
                                              spec.easing, now);
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
