#ifndef REACH_FEATURES_SETTINGS_PAGES_INTERNAL_H
#define REACH_FEATURES_SETTINGS_PAGES_INTERNAL_H

#include "reach/apps/settings/settings.h"

const uint16_t *reach_settings_wifi_page_title(void);
const uint16_t *reach_settings_bluetooth_page_title(void);
const uint16_t *reach_settings_account_page_title(void);
const uint16_t *reach_settings_startup_apps_page_title(void);
const uint16_t *reach_settings_power_sleep_page_title(void);
const uint16_t *reach_settings_monitors_scaling_page_title(void);
const uint16_t *reach_settings_update_page_title(void);

const uint16_t *reach_settings_wifi_page_placeholder(void);
const uint16_t *reach_settings_bluetooth_page_placeholder(void);
const uint16_t *reach_settings_account_page_placeholder(void);
const uint16_t *reach_settings_startup_apps_page_placeholder(void);
const uint16_t *reach_settings_power_sleep_page_placeholder(void);
const uint16_t *reach_settings_monitors_scaling_page_placeholder(void);

typedef struct reach_settings_power_row_style
{
    uint32_t icon_id;
    const uint16_t *title;
    const uint16_t *subtitle;
    reach_color accent;
} reach_settings_power_row_style;

const reach_settings_power_row_style *reach_settings_power_row_styles(void);
#endif
