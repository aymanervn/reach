#include "settings_pages_internal.h"

void reach_settings_model_scroll_theme(reach_settings_model *model, float delta)
{
    if (model != nullptr)
    {
        (void)reach_scroll_view_scroll_by(&model->theme_scroll, delta, reach_motion_now_seconds());
    }
}

const uint16_t *reach_settings_display_page_title(void)
{
    return (const uint16_t *)L"Display";
}

const uint16_t *reach_settings_theme_page_title(void)
{
    return (const uint16_t *)L"Theme";
}

void reach_settings_model_set_high_refresh_rate(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->theme_high_refresh_rate = enabled ? 1 : 0;
    reach_motion_set(&model->theme_fps_toggle, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_high_refresh_rate(const reach_settings_model *model)
{
    return model != nullptr && model->theme_high_refresh_rate;
}

int32_t reach_settings_model_toggle_high_refresh_rate(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->theme_high_refresh_rate = model->theme_high_refresh_rate ? 0 : 1;
    reach_settings_animate(&model->theme_fps_toggle, model->theme_high_refresh_rate ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
    return 1;
}

void reach_settings_model_set_bundled_font(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->theme_bundled_font = enabled ? 1 : 0;
    reach_motion_set(&model->theme_font_toggle, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_bundled_font(const reach_settings_model *model)
{
    return model != nullptr && model->theme_bundled_font;
}

int32_t reach_settings_model_toggle_bundled_font(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->theme_bundled_font = model->theme_bundled_font ? 0 : 1;
    reach_settings_animate(&model->theme_font_toggle, model->theme_bundled_font ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
    return 1;
}

void reach_settings_model_set_glass(reach_settings_model *model,
                                    const reach_config_glass_settings *glass)
{
    if (model == nullptr || glass == nullptr)
        return;
    model->glass = *glass;
    reach_motion_set(&model->glass_toggle, glass->enabled ? 1.0f : 0.0f);
}

void reach_settings_model_set_glass_enabled(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr || model->glass.enabled == (enabled != 0))
        return;
    model->glass.enabled = enabled != 0;
    reach_settings_animate(&model->glass_toggle, model->glass.enabled ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
}

int32_t reach_settings_model_set_glass_intensity(reach_settings_model *model, int32_t intensity)
{
    if (model == nullptr || intensity < 0 || intensity > REACH_CONFIG_GLASS_INTENSITY_MAX ||
        model->glass.intensity == intensity)
        return 0;
    model->glass.intensity = intensity;
    return 1;
}

void reach_settings_model_set_light_theme(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->theme_light = enabled ? 1 : 0;
    reach_motion_set(&model->theme_reach_selection, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_light_theme(const reach_settings_model *model)
{
    return model != nullptr && model->theme_light;
}

int32_t reach_settings_model_select_light_theme(reach_settings_model *model, int32_t enabled)
{
    int32_t light_theme = enabled ? 1 : 0;
    if (model == nullptr || model->theme_light == light_theme)
    {
        return 0;
    }
    model->theme_light = light_theme;
    reach_settings_animate(&model->theme_reach_selection, light_theme ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_FADE_IN);
    return 1;
}

void reach_settings_model_set_accent_preset(reach_settings_model *model, int32_t preset)
{
    if (model == nullptr)
    {
        return;
    }
    model->theme_accent = reach_accent_preset_clamp(preset);
    reach_motion_set(&model->theme_accent_selection, (float)model->theme_accent);
}

int32_t reach_settings_model_accent_preset(const reach_settings_model *model)
{
    return model != nullptr ? model->theme_accent : REACH_ACCENT_PRESET_BLUE;
}

int32_t reach_settings_model_select_accent_preset(reach_settings_model *model, int32_t preset)
{
    if (model == nullptr || preset < 0 || preset >= REACH_ACCENT_PRESET_COUNT ||
        model->theme_accent == preset)
    {
        return 0;
    }
    model->theme_accent = preset;
    reach_settings_animate(&model->theme_accent_selection, (float)preset,
                           REACH_MOTION_PRESET_SNAPPY);
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
        model->theme_windows_system_theme = preference;
    }
}

reach_config_theme_preference
reach_settings_model_windows_system_theme(const reach_settings_model *model)
{
    return model != nullptr ? model->theme_windows_system_theme : REACH_CONFIG_THEME_FOLLOW_REACH;
}

int32_t reach_settings_model_select_windows_system_theme(reach_settings_model *model,
                                                         reach_config_theme_preference preference)
{
    if (model == nullptr || !reach_settings_theme_preference_valid(preference) ||
        model->theme_windows_system_theme == preference)
    {
        return 0;
    }
    model->theme_windows_system_theme = preference;
    return 1;
}

void reach_settings_model_set_windows_app_theme(reach_settings_model *model,
                                                reach_config_theme_preference preference)
{
    if (model != nullptr && reach_settings_theme_preference_valid(preference))
    {
        model->theme_windows_app_theme = preference;
    }
}

reach_config_theme_preference
reach_settings_model_windows_app_theme(const reach_settings_model *model)
{
    return model != nullptr ? model->theme_windows_app_theme : REACH_CONFIG_THEME_FOLLOW_REACH;
}

int32_t reach_settings_model_select_windows_app_theme(reach_settings_model *model,
                                                      reach_config_theme_preference preference)
{
    if (model == nullptr || !reach_settings_theme_preference_valid(preference) ||
        model->theme_windows_app_theme == preference)
    {
        return 0;
    }
    model->theme_windows_app_theme = preference;
    return 1;
}
