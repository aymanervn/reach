#include "settings_pages_internal.h"

const uint16_t *reach_settings_display_page_title(void)
{
    return (const uint16_t *)L"Display";
}

const uint16_t *reach_settings_display_page_placeholder(void)
{
    return (const uint16_t *)L"Display settings page";
}

void reach_settings_model_set_high_refresh_rate(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->display_high_refresh_rate = enabled ? 1 : 0;
    reach_animation_manager_set(&model->display_fps_animation, 0, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_high_refresh_rate(const reach_settings_model *model)
{
    return model != nullptr && model->display_high_refresh_rate;
}

int32_t reach_settings_model_toggle_high_refresh_rate(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->display_high_refresh_rate = model->display_high_refresh_rate ? 0 : 1;
    float current = reach_animation_manager_value(&model->display_fps_animation, 0);
    reach_animation_manager_start(&model->display_fps_animation, 0, current,
                                  model->display_high_refresh_rate ? 1.0f : 0.0f, 0.18,
                                  REACH_EASING_EASE_OUT);
    return 1;
}

void reach_settings_model_set_bundled_font(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->display_bundled_font = enabled ? 1 : 0;
    reach_animation_manager_set(&model->display_font_animation, 0, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_bundled_font(const reach_settings_model *model)
{
    return model != nullptr && model->display_bundled_font;
}

int32_t reach_settings_model_toggle_bundled_font(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->display_bundled_font = model->display_bundled_font ? 0 : 1;
    float current = reach_animation_manager_value(&model->display_font_animation, 0);
    reach_animation_manager_start(&model->display_font_animation, 0, current,
                                  model->display_bundled_font ? 1.0f : 0.0f, 0.18,
                                  REACH_EASING_EASE_OUT);
    return 1;
}

void reach_settings_model_set_light_theme(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->display_light_theme = enabled ? 1 : 0;
    reach_animation_manager_set(&model->display_theme_animation, 0, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_light_theme(const reach_settings_model *model)
{
    return model != nullptr && model->display_light_theme;
}

int32_t reach_settings_model_toggle_light_theme(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->display_light_theme = model->display_light_theme ? 0 : 1;
    float current = reach_animation_manager_value(&model->display_theme_animation, 0);
    reach_animation_manager_start(&model->display_theme_animation, 0, current,
                                  model->display_light_theme ? 1.0f : 0.0f, 0.18,
                                  REACH_EASING_EASE_OUT);
    return 1;
}

static int32_t reach_settings_theme_preference_valid(reach_config_theme_preference preference)
{
    return preference >= REACH_CONFIG_THEME_FOLLOW_REACH && preference <= REACH_CONFIG_THEME_DARK;
}

void reach_settings_model_set_windows_system_theme(reach_settings_model *model,
                                                   reach_config_theme_preference preference)
{
    if (model != nullptr && reach_settings_theme_preference_valid(preference))
    {
        model->display_windows_system_theme = preference;
    }
}

reach_config_theme_preference
reach_settings_model_windows_system_theme(const reach_settings_model *model)
{
    return model != nullptr ? model->display_windows_system_theme : REACH_CONFIG_THEME_FOLLOW_REACH;
}

int32_t reach_settings_model_select_windows_system_theme(reach_settings_model *model,
                                                         reach_config_theme_preference preference)
{
    if (model == nullptr || !reach_settings_theme_preference_valid(preference) ||
        model->display_windows_system_theme == preference)
    {
        return 0;
    }
    model->display_windows_system_theme = preference;
    return 1;
}

void reach_settings_model_set_windows_app_theme(reach_settings_model *model,
                                                reach_config_theme_preference preference)
{
    if (model != nullptr && reach_settings_theme_preference_valid(preference))
    {
        model->display_windows_app_theme = preference;
    }
}

reach_config_theme_preference
reach_settings_model_windows_app_theme(const reach_settings_model *model)
{
    return model != nullptr ? model->display_windows_app_theme : REACH_CONFIG_THEME_FOLLOW_REACH;
}

int32_t reach_settings_model_select_windows_app_theme(reach_settings_model *model,
                                                      reach_config_theme_preference preference)
{
    if (model == nullptr || !reach_settings_theme_preference_valid(preference) ||
        model->display_windows_app_theme == preference)
    {
        return 0;
    }
    model->display_windows_app_theme = preference;
    return 1;
}

int32_t reach_settings_model_tick_display_animations(reach_settings_model *model,
                                                     double delta_seconds)
{
    if (model == nullptr || !reach_settings_model_display_animations_active(model))
    {
        return 0;
    }
    reach_animation_manager_tick(&model->display_fps_animation, delta_seconds);
    reach_animation_manager_tick(&model->display_font_animation, delta_seconds);
    reach_animation_manager_tick(&model->display_theme_animation, delta_seconds);
    return 1;
}

int32_t reach_settings_model_display_animations_active(const reach_settings_model *model)
{
    return model != nullptr &&
           (reach_animation_manager_any_active(&model->display_fps_animation) ||
            reach_animation_manager_any_active(&model->display_font_animation) ||
            reach_animation_manager_any_active(&model->display_theme_animation));
}
